// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Allocator - Common implementation
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <trace/hooks/mm.h>

#include "heap_private.h"

#define MAX_EVENT_LOG	2048
#define EVENT_CLAMP_ID(id) ((id) & (MAX_EVENT_LOG - 1))

static atomic_t dma_heap_eventid;

static char * const dma_heap_event_name[] = {
	"alloc",
	"free",
	"mmap",
	"vmap",
	"vunmap",
	"map_dma_buf",
	"unmap_dma_buf",
	"begin_cpu_access",
	"end_cpu_access",
	"begin_cpu_partial",
	"end_cpu_partial",
};

static struct dma_heap_event {
	ktime_t begin;
	ktime_t done;
	unsigned char heapname[16];
	unsigned long ino;
	size_t size;
	enum dma_heap_event_type type;
} dma_heap_events[MAX_EVENT_LOG];

void dma_heap_event_record(enum dma_heap_event_type type, struct dma_buf *dmabuf, ktime_t begin)
{
	int idx = EVENT_CLAMP_ID(atomic_inc_return(&dma_heap_eventid));
	struct dma_heap_event *event = &dma_heap_events[idx];

	event->begin = begin;
	event->done = ktime_get();
	strlcpy(event->heapname, dmabuf->exp_name, sizeof(event->heapname));
	event->ino = file_inode(dmabuf->file)->i_ino;
	event->size = dmabuf->size;
	event->type = type;
}

#ifdef CONFIG_DEBUG_FS
static int dma_heap_event_show(struct seq_file *s, void *unused)
{
	int index = EVENT_CLAMP_ID(atomic_read(&dma_heap_eventid) + 1);
	int i = index;

	seq_printf(s, "%14s %18s %16s %16s %10s %10s\n",
		   "timestamp", "event", "name", "size(kb)", "ino",  "elapsed(us)");

	do {
		struct dma_heap_event *event = &dma_heap_events[EVENT_CLAMP_ID(i)];
		long elapsed = ktime_us_delta(event->done, event->begin);
		struct timespec64 ts = ktime_to_timespec64(event->begin);

		if (elapsed == 0)
			continue;

		seq_printf(s, "[%06ld.%06ld]", ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
		seq_printf(s, "%18s %16s %16zd %10lu %10ld", dma_heap_event_name[event->type],
			   event->heapname, event->size >> 10, event->ino, elapsed);

		if (elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		seq_puts(s, "\n");
	} while (EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int dma_heap_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_heap_event_show, inode->i_private);
}

static const struct file_operations dma_heap_event_fops = {
	.open = dma_heap_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void dma_heap_debug_init(void)
{
	struct dentry *root = debugfs_create_dir("dma_heap", NULL);

	if (IS_ERR(root)) {
		pr_err("Failed to create debug directory for dma_heap");
		return;
	}

	if (IS_ERR(debugfs_create_file("event", 0444, root, NULL, &dma_heap_event_fops)))
		pr_err("Failed to create event file for dma_heap\n");
}
#else
void dma_heap_debug_init(void)
{
}
#endif

#define MAX_EXCEPTION_AREAS 4
static phys_addr_t dma_heap_exception_areas[MAX_EXCEPTION_AREAS][2];
static int nr_dma_heap_exception;

bool is_dma_heap_exception_page(struct page *page)
{
	phys_addr_t phys = page_to_phys(page);
	int i;

	for (i = 0; i < nr_dma_heap_exception; i++)
		if ((dma_heap_exception_areas[i][0] <= phys) &&
		    (phys <= dma_heap_exception_areas[i][1]))
			return true;

	return false;
}

void heap_cache_flush(struct samsung_dma_buffer *buffer)
{
	struct device *dev = dma_heap_get_dev(buffer->heap->dma_heap);

	if (!dma_heap_skip_cache_ops(buffer->flags))
		return;
	/*
	 * Flushing caches on buffer allocation is intended for preventing
	 * corruption from writing back to DRAM from the dirty cache lines
	 * while updating the buffer from DMA. However, cache flush should be
	 * performed on the entire allocated area if the buffer is to be
	 * protected from non-secure access to prevent the dirty write-back
	 * to the protected area.
	 */
	dma_map_sgtable(dev, &buffer->sg_table, DMA_TO_DEVICE, 0);
	dma_unmap_sgtable(dev, &buffer->sg_table, DMA_FROM_DEVICE, 0);
}

void heap_sgtable_pages_clean(struct sg_table *sgt)
{
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
}

/*
 * It should be called by physically contiguous buffer.
 */
void heap_page_clean(struct page *pages, unsigned long size)
{
	unsigned long nr_pages, i;

	if (!PageHighMem(pages)) {
		memset(page_address(pages), 0, size);
		return;
	}

	nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	for (i = 0; i < nr_pages; i++) {
		void *vaddr = kmap_atomic(&pages[i]);

		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
}

struct samsung_dma_buffer *samsung_dma_buffer_alloc(struct samsung_dma_heap *samsung_dma_heap,
						    unsigned long size, unsigned int nents)
{
	struct samsung_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	if (sg_alloc_table(&buffer->sg_table, nents, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable with %u entry", nents);
		kfree(buffer);
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = samsung_dma_heap;
	buffer->len = size;
	buffer->flags = samsung_dma_heap->flags;

	return buffer;
}
EXPORT_SYMBOL_GPL(samsung_dma_buffer_alloc);

void samsung_dma_buffer_free(struct samsung_dma_buffer *buffer)
{
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}
EXPORT_SYMBOL_GPL(samsung_dma_buffer_free);

static const char *samsung_add_heap_name(unsigned long flags)
{
	if (dma_heap_flags_uncached(flags))
		return "-uncached";

	if (dma_heap_flags_protected(flags))
		return "-secure";

	return "";
}

static void show_dmaheap_total_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	struct samsung_dma_heap *heap = data;
	u64 total_size_kb = samsung_heap_total_kbsize(heap);

	/* skip if the size is zero in order not to show meaningless log. */
	if (total_size_kb)
		pr_info("%s: %lukb ", heap->name, total_size_kb);
}

static void show_dmaheap_meminfo(void *data, struct seq_file *m)
{
	struct samsung_dma_heap *heap = data;
	u64 total_size_kb = samsung_heap_total_kbsize(heap);

	if (total_size_kb == 0)
		return;

	show_val_meminfo(m, heap->name, total_size_kb);
}

static struct samsung_dma_heap *__samsung_heap_add(struct device *dev, void *priv,
						   void (*release)(struct samsung_dma_buffer *),
						   const struct dma_heap_ops *ops,
						   unsigned int flags)
{
	struct samsung_dma_heap *heap;
	unsigned int alignment = PAGE_SIZE, order, protid = 0;
	struct dma_heap_export_info exp_info;
	const char *name;
	char *heap_name;
	struct dma_heap *dma_heap;

	if (dma_heap_flags_protected(flags)) {
		of_property_read_u32(dev->of_node, "dma-heap,protection_id", &protid);
		if (!protid) {
			perrfn("Secure heap should be set with protection id");
			return ERR_PTR(-EINVAL);
		}
	}

	if (of_property_read_bool(dev->of_node, "dma-heap,video_aligned"))
		flags |= DMA_HEAP_FLAG_VIDEO_ALIGNED;

	if (of_property_read_bool(dev->of_node, "dma-heap,untouchable"))
		flags |= DMA_HEAP_FLAG_UNTOUCHABLE | DMA_HEAP_FLAG_PROTECTED;

	if (of_property_read_string(dev->of_node, "dma-heap,name", &name)) {
		perrfn("The heap should define name on device node");
		return ERR_PTR(-EINVAL);
	}

	of_property_read_u32(dev->of_node, "dma-heap,alignment", &alignment);
	order = min_t(unsigned int, get_order(alignment), MAX_ORDER);

	heap = devm_kzalloc(dev, sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->flags = flags;

	heap_name = devm_kasprintf(dev, GFP_KERNEL, "%s%s", name, samsung_add_heap_name(flags));
	if (!heap_name)
		return ERR_PTR(-ENOMEM);

	dma_heap = dma_heap_find(heap_name);
	if (dma_heap) {
		pr_err("Already registered heap named %s\n", heap_name);
		dma_heap_put(dma_heap);
		return ERR_PTR(-ENODEV);
	}

	heap->protection_id = protid;
	heap->alignment = 1 << (order + PAGE_SHIFT);
	heap->release = release;
	heap->priv = priv;
	heap->name = heap_name;

	exp_info.name = heap_name;
	exp_info.ops = ops;
	exp_info.priv = heap;

	heap->dma_heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap->dma_heap))
		return heap;

	register_trace_android_vh_show_mem(show_dmaheap_total_handler, heap);
	if (!strncmp(heap_name, "system", strlen("system")))
		register_trace_android_vh_meminfo_proc_show(show_dmaheap_meminfo, heap);
	dma_coerce_mask_and_coherent(dma_heap_get_dev(heap->dma_heap), DMA_BIT_MASK(36));

	pr_info("Registered %s dma-heap successfully\n", heap_name);

	return heap;
}

static const unsigned int nonsecure_heap_type[] = {
	0,
	DMA_HEAP_FLAG_UNCACHED,
};

#define num_nonsecure_heaps ARRAY_SIZE(nonsecure_heap_type)

static const unsigned int secure_heap_type[] = {
	DMA_HEAP_FLAG_PROTECTED,
};

#define num_secure_heaps ARRAY_SIZE(secure_heap_type)

/*
 * Maximum heap types is defined by cachable heap types
 * because prot heap type is always only 1.
 */
#define num_max_heaps (num_nonsecure_heaps)

/*
 * NOTE: samsung_heap_create returns error when heap creation fails.
 * In case of -ENODEV, it means that the secure heap doesn't need to be added
 * if system doesn't support content protection. Or, the same name heap is
 * already added.
 */
int samsung_heap_add(struct device *dev, void *priv,
		     void (*release)(struct samsung_dma_buffer *buffer),
		     const struct dma_heap_ops *ops)
{
	struct samsung_dma_heap *heap[num_max_heaps];
	const unsigned int *types;
	int i, ret, count;

	/*
	 * Secure heap should allocate only secure buffer.
	 * Normal cachable heap and uncachable heaps are not registered.
	 */
	if (of_property_read_bool(dev->of_node, "dma-heap,secure")) {
		count = num_secure_heaps;
		types = secure_heap_type;
	} else {
		count = num_nonsecure_heaps;
		types = nonsecure_heap_type;
	}

	for (i = 0; i < count; i++) {
		heap[i] = __samsung_heap_add(dev, priv, release, ops, types[i]);
		if (IS_ERR(heap[i])) {
			ret = PTR_ERR(heap[i]);
			goto heap_put;
		}
	}

	return 0;
heap_put:
	while (i-- > 0)
		dma_heap_put(heap[i]->dma_heap);

	return ret;
}

struct dma_buf *samsung_export_dmabuf(struct samsung_dma_buffer *buffer, unsigned long fd_flags)
{
	struct dma_buf *dmabuf;

	DEFINE_SAMSUNG_DMA_BUF_EXPORT_INFO(exp_info, buffer->heap->name);

	exp_info.ops = &samsung_dma_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		perrfn("failed to export buffer (ret: %d)", PTR_ERR(dmabuf));
		return dmabuf;
	}
	atomic_long_add(dmabuf->size, &buffer->heap->total_bytes);
	dmabuf_trace_alloc(dmabuf);

	return dmabuf;
}
EXPORT_SYMBOL_GPL(samsung_export_dmabuf);

static void dma_heap_add_exception_area(void)
{
	struct device_node *np;

	for_each_node_by_name(np, "dma_heap_exception_area") {
		int naddr = of_n_addr_cells(np);
		int nsize = of_n_size_cells(np);
		phys_addr_t base, size;
		const __be32 *prop;
		int len;
		int i;

		prop = of_get_property(np, "#address-cells", NULL);
		if (prop)
			naddr = be32_to_cpup(prop);

		prop = of_get_property(np, "#size-cells", NULL);
		if (prop)
			nsize = be32_to_cpup(prop);

		prop = of_get_property(np, "exception-range", &len);
		if (prop && len > 0) {
			int n_area = len / (sizeof(*prop) * (nsize + naddr));

			n_area = min_t(int, n_area, MAX_EXCEPTION_AREAS);

			for (i = 0; i < n_area ; i++) {
				base = (phys_addr_t)of_read_number(prop, naddr);
				prop += naddr;
				size = (phys_addr_t)of_read_number(prop, nsize);
				prop += nsize;

				dma_heap_exception_areas[i][0] = base;
				dma_heap_exception_areas[i][1] = base + size - 1;
			}

			nr_dma_heap_exception = n_area;
		}
	}
}

static int __init samsung_dma_heap_init(void)
{
	int ret;

	ret = chunk_dma_heap_init();
	if (ret)
		return ret;

	ret = cma_dma_heap_init();
	if (ret)
		goto err_cma;

	ret = carveout_dma_heap_init();
	if (ret)
		goto err_carveout;

	ret = system_dma_heap_init();
	if (ret)
		goto err_system;

	ret = dmabuf_trace_create();
	if (ret)
		goto err_trace;

	ret = rbin_dma_heap_init();
	if (ret)
		goto err_rbin;

	dma_heap_add_exception_area();
	dma_heap_debug_init();

	return 0;
err_rbin:
	dmabuf_trace_remove();
err_trace:
	system_dma_heap_exit();
err_system:
	carveout_dma_heap_exit();
err_carveout:
	cma_dma_heap_exit();
err_cma:
	chunk_dma_heap_exit();

	return ret;
}

static void __exit samsung_dma_heap_exit(void)
{
	rbin_dma_heap_exit();
	dmabuf_trace_remove();
	system_dma_heap_exit();
	carveout_dma_heap_exit();
	cma_dma_heap_exit();
	chunk_dma_heap_exit();
}

module_init(samsung_dma_heap_init);
module_exit(samsung_dma_heap_exit);
MODULE_DESCRIPTION("DMA-BUF Samsung Heap");
MODULE_LICENSE("GPL v2");
