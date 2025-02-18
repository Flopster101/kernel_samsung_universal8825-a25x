// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Rbin heap exporter for Samsung
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memblock.h>

#include "rbinregion.h"
#include "heap_private.h"
#include "../deferred-free-helper.h"

static const unsigned int orders[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
#define NUM_ORDERS ARRAY_SIZE(orders)

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

struct rbin_heap {
	struct task_struct *task;
	struct task_struct *task_shrink;
	bool task_run;
	bool shrink_run;
	wait_queue_head_t waitqueue;
	unsigned long count;
	struct dmabuf_page_pool *pools[NUM_ORDERS];
};

static void rbin_page_pool_add(struct dmabuf_page_pool *pool, struct page *page)
{
	int index;

	if (PageHighMem(page))
		index = POOL_HIGHPAGE;
	else
		index = POOL_LOWPAGE;

	mutex_lock(&pool->mutex);
	list_add_tail(&page->lru, &pool->items[index]);
	pool->count[index]++;
	mutex_unlock(&pool->mutex);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
}

static struct page *rbin_page_pool_remove(struct dmabuf_page_pool *pool, int index)
{
	struct page *page;

	mutex_lock(&pool->mutex);
	page = list_first_entry_or_null(&pool->items[index], struct page, lru);
	if (page) {
		pool->count[index]--;
		list_del(&page->lru);
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
				    -(1 << pool->order));
	}
	mutex_unlock(&pool->mutex);

	return page;
}

static struct page *rbin_page_pool_fetch(struct dmabuf_page_pool *pool)
{
	struct page *page = NULL;

	page = rbin_page_pool_remove(pool, POOL_HIGHPAGE);
	if (!page)
		page = rbin_page_pool_remove(pool, POOL_LOWPAGE);

	return page;
}

static struct dmabuf_page_pool *rbin_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct dmabuf_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	int i;

	if (!pool)
		return NULL;

	for (i = 0; i < POOL_TYPE_SIZE; i++) {
		pool->count[i] = 0;
		INIT_LIST_HEAD(&pool->items[i]);
	}
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);

	return pool;
}

static void rbin_page_pool_free(struct dmabuf_page_pool *pool, struct page *page)
{
	rbin_page_pool_add(pool, page);
}

static struct page *alloc_rbin_page(unsigned long size, unsigned long last_size)
{
	struct page *page = ERR_PTR(-ENOMEM);
	phys_addr_t paddr = -ENOMEM;
	void *addr;
	int order;

	order = min(get_order(last_size), get_order(size));
	for (; order >= 0; order--) {
		size = min_t(unsigned long, size, PAGE_SIZE << order);
		paddr = dmabuf_rbin_allocate(size);
		if (paddr == -ENOMEM)
			continue;
		if (paddr == -EBUSY)
			page = ERR_PTR(-EBUSY);
		break;
	}

	if (!IS_ERR_VALUE(paddr)) {
		page = phys_to_page(paddr);
		INIT_LIST_HEAD(&page->lru);
		addr = page_address(page);
		memset(addr, 0, size);
		set_page_private(page, size);
	}
	return page;
}

static inline void do_expand(struct rbin_heap *rbin_heap,
			     struct page *page, unsigned int nr_pages)
{
	unsigned int rem_nr_pages;
	unsigned int order;
	unsigned int total_nr_pages;
	unsigned int free_nr_page;
	struct page *free_page;
	struct dmabuf_page_pool *pool;

	total_nr_pages = page_private(page) >> PAGE_SHIFT;
	rem_nr_pages = total_nr_pages - nr_pages;
	free_page = page + total_nr_pages;

	while (rem_nr_pages) {
		order = ilog2(rem_nr_pages);
		free_nr_page = 1 << order;
		free_page -= free_nr_page;
		set_page_private(free_page, free_nr_page << PAGE_SHIFT);
		pool = rbin_heap->pools[order_to_index(order)];
		rbin_page_pool_free(pool, free_page);
		rem_nr_pages -= free_nr_page;
	}
	set_page_private(page, nr_pages << PAGE_SHIFT);
}

static struct page *alloc_rbin_page_from_pool(struct rbin_heap *rbin_heap,
					      unsigned long size)
{
	struct page *page = NULL;
	unsigned int size_order = get_order(size);
	unsigned int nr_pages = size >> PAGE_SHIFT;
	int i;

	/* try the same or higher order */
	for (i = NUM_ORDERS - 1; i >= 0; i--) {
		if (orders[i] < size_order)
			continue;
		page = rbin_page_pool_fetch(rbin_heap->pools[i]);
		if (!page)
			continue;
		if (nr_pages < (1 << orders[i]))
			do_expand(rbin_heap, page, nr_pages);
		goto done;
	}

	/* try lower order */
	for (i = 0; i < NUM_ORDERS; i++) {
		if (orders[i] >= size_order)
			continue;
		page = rbin_page_pool_fetch(rbin_heap->pools[i]);
		if (!page)
			continue;
		goto done;
	}
done:
	if (page)
		atomic_sub(page_private(page) >> PAGE_SHIFT, &rbin_pool_pages);
	return page;
}

static struct dma_buf *rbin_heap_allocate(struct dma_heap *heap, unsigned long len,
					  unsigned long fd_flags, unsigned long heap_flags)
{
	struct samsung_dma_heap *samsung_dma_heap = dma_heap_get_drvdata(heap);
	struct samsung_dma_buffer *buffer;
	struct rbin_heap *rbin_heap = samsung_dma_heap->priv;
	struct scatterlist *sg;
	struct dma_buf *dmabuf;
	struct list_head pages;
	struct page *page, *tmp_page;
	unsigned long size_remain;
	unsigned long last_size;
	unsigned long nr_free;
	int i = 0;
	int ret = -ENOMEM;

	if (dma_heap_flags_video_aligned(samsung_dma_heap->flags))
		len = dma_heap_add_video_padding(len);
	size_remain = last_size = PAGE_ALIGN(len);

	nr_free = rbin_heap->count - atomic_read(&rbin_allocated_pages);
	if (size_remain > nr_free << PAGE_SHIFT)
		return ERR_PTR(ret);

	INIT_LIST_HEAD(&pages);
	while (size_remain > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current)) {
			perrfn("Fatal signal pending pid #%d", current->pid);
			ret = -EINTR;
			goto free_buffer;
		}

		if (atomic_read(&rbin_pool_pages)) {
			page = alloc_rbin_page_from_pool(rbin_heap, size_remain);
			if (page)
				goto got_pg;
		}

		page = alloc_rbin_page(size_remain, last_size);
		if (IS_ERR(page))
			goto free_buffer;
		else
			last_size = page_private(page);
got_pg:
		list_add_tail(&page->lru, &pages);
		size_remain -= page_private(page);
		i++;
	}

	buffer = samsung_dma_buffer_alloc(samsung_dma_heap, len, i);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);
		goto free_buffer;
	}

	sg = buffer->sg_table.sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_private(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	heap_cache_flush(buffer);

	dmabuf = samsung_export_dmabuf(buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_export;
	}
	atomic_add(len >> PAGE_SHIFT, &rbin_allocated_pages);
	return dmabuf;

free_export:
	for_each_sgtable_sg(&buffer->sg_table, sg, i) {
		struct page *p = sg_page(sg);

		dmabuf_rbin_free(page_to_phys(p), page_private(p));
	}
	samsung_dma_buffer_free(buffer);
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		dmabuf_rbin_free(page_to_phys(page), page_private(page));

	return ERR_PTR(ret);
}

static long rbin_heap_get_pool_size(struct dma_heap *heap)
{
	const char *name = dma_heap_get_name(heap);

	if (strcmp(name, "camera"))
		return 0;

	return atomic_read(&rbin_pool_pages) << PAGE_SHIFT;
}

static const struct dma_heap_ops rbin_heap_ops = {
	.allocate = rbin_heap_allocate,
	.get_pool_size = rbin_heap_get_pool_size,
};

static void rbin_heap_release(struct samsung_dma_buffer *buffer)
{
	struct sg_table *table;
	struct scatterlist *sg;
	struct page *page;
	int i;

	table = &buffer->sg_table;
	for_each_sg(table->sgl, sg, table->nents, i) {
		page = sg_page(sg);
		dmabuf_rbin_free(page_to_phys(page), page_private(page));
	}
	atomic_sub(buffer->len >> PAGE_SHIFT, &rbin_allocated_pages);
	samsung_dma_buffer_free(buffer);
}

static struct rbin_heap *g_rbin_heap;

void wake_dmabuf_rbin_heap_prereclaim(void)
{
	if (g_rbin_heap) {
		g_rbin_heap->task_run = 1;
		wake_up(&g_rbin_heap->waitqueue);
	}
}

void wake_dmabuf_rbin_heap_shrink(void)
{
	if (g_rbin_heap) {
		g_rbin_heap->shrink_run = 1;
		wake_up(&g_rbin_heap->waitqueue);
	}
}

static void dmabuf_rbin_heap_destroy_pools(struct dmabuf_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		kfree(pools[i]);
}

static int dmabuf_rbin_heap_create_pools(struct dmabuf_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i] = rbin_page_pool_create(GFP_KERNEL, orders[i]);
		if (!pools[i])
			goto err_create_pool;
	}
	return 0;

err_create_pool:
	dmabuf_rbin_heap_destroy_pools(pools);
	return -ENOMEM;
}

static int dmabuf_rbin_heap_prereclaim(void *data)
{
	struct rbin_heap *rbin_heap = data;
	unsigned int order;
	unsigned long total_size;
	unsigned long size = PAGE_SIZE << orders[0];
	unsigned long last_size;
	struct dmabuf_page_pool *pool;
	struct page *page;
	unsigned long jiffies_bstop;

	while (true) {
		wait_event_freezable(rbin_heap->waitqueue, rbin_heap->task_run);
		jiffies_bstop = jiffies + (HZ / 10);
		trace_printk("%s\n", "start");
		total_size = 0;
		last_size = size;
		while (true) {
			page = alloc_rbin_page(size, last_size);
			if (PTR_ERR(page) == -ENOMEM)
				break;
			if (PTR_ERR(page) == -EBUSY) {
				if (time_is_after_jiffies(jiffies_bstop))
					continue;
				else
					break;
			}
			last_size = page_private(page);
			order = get_order(page_private(page));
			pool = rbin_heap->pools[order_to_index(order)];
			rbin_page_pool_free(pool, page);
			total_size += page_private(page);
			atomic_add(1 << order, &rbin_pool_pages);
		}
		trace_printk("end %lu\n", total_size);
		rbin_heap->task_run = 0;
	}
	return 0;
}

static int dmabuf_rbin_heap_shrink(void *data)
{
	struct rbin_heap *rbin_heap = data;
	unsigned long total_size;
	unsigned long size = PAGE_SIZE << orders[0];
	struct page *page;

	while (true) {
		wait_event_freezable(rbin_heap->waitqueue, rbin_heap->shrink_run);
		trace_printk("%s", "start\n");
		total_size = 0;
		while (true) {
			page = alloc_rbin_page_from_pool(rbin_heap, size);
			if (!page)
				break;
			dmabuf_rbin_free(page_to_phys(page), page_private(page));
			total_size += page_private(page);
		}
		trace_printk("%lu\n", total_size);
		rbin_heap->shrink_run = 0;
	}
	return 0;
}

struct kobject *rbin_kobject;

static bool under_8GB_device(void)
{
	return memblock_end_of_DRAM() <= 0xa00000000 ? true : false;
}

static int rbin_heap_probe(struct platform_device *pdev)
{
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	struct rbin_heap *rbin_heap;

	if (!under_8GB_device()) {
		pr_info("%s for >8GB devices, create carveout heap instead.\n", __func__);
		return carveout_heap_probe(pdev);
	}

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}
	if (!rmem->size) {
		perrdev(&pdev->dev, "memory-region has no size");
		return -EINVAL;
	}
	if (is_dma_heap_exception_page(phys_to_page(rmem->base)) ||
	    is_dma_heap_exception_page(phys_to_page(rmem->base + rmem->size - PAGE_SIZE))) {
		perrdev(&pdev->dev, "memory-region has invalid base address");
		return -EINVAL;
	}

	rbin_heap = kzalloc(sizeof(struct rbin_heap), GFP_KERNEL);
	if (!rbin_heap)
		return -ENOMEM;
	rbin_heap->count = rmem->size >> PAGE_SHIFT;

	rbin_kobject = kobject_create_and_add("rbin", kernel_kobj);
	if (!rbin_kobject) {
		perrdev(&pdev->dev, "failed to create rbin_kobject");
		goto out;
	}
	if (dmabuf_rbin_heap_create_pools(rbin_heap->pools)) {
		perrdev(&pdev->dev, "failed to create dma-buf page pool");
		goto out;
	}
	if (init_rbinregion(rmem->base, rmem->size)) {
		perrdev(&pdev->dev, "failed to init rbinregion");
		dmabuf_rbin_heap_destroy_pools(rbin_heap->pools);
		goto out;
	}
	init_waitqueue_head(&rbin_heap->waitqueue);
	rbin_heap->task = kthread_run(dmabuf_rbin_heap_prereclaim, rbin_heap, "rbin");
	rbin_heap->task_shrink = kthread_run(dmabuf_rbin_heap_shrink, rbin_heap, "rbin_shrink");
	g_rbin_heap = rbin_heap;
	pr_info("%s created %s\n", __func__, rmem->name);

	return samsung_heap_add(&pdev->dev, rbin_heap, rbin_heap_release, &rbin_heap_ops);
out:
	if (rbin_kobject)
		kobject_put(rbin_kobject);
	kfree(rbin_heap);

	return -ENOMEM;
}

static const struct of_device_id rbin_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-rbin", },
	{ },
};
MODULE_DEVICE_TABLE(of, rbin_heap_of_match);

static struct platform_driver rbin_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-rbin",
		.of_match_table = rbin_heap_of_match,
	},
	.probe		= rbin_heap_probe,
};

int __init rbin_dma_heap_init(void)
{
	return platform_driver_register(&rbin_heap_driver);
}

void rbin_dma_heap_exit(void)
{
	platform_driver_unregister(&rbin_heap_driver);
}
