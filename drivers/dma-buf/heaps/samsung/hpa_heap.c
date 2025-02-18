// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF HPA heap exporter for Samsung
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/anon_inodes.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direct.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/genalloc.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/kmemleak.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <trace/hooks/mm.h>

#include "secure_buffer.h"

struct hpa_dma_heap {
	struct dma_heap *dma_heap;
	const char *name;
	unsigned int protection_id;
	atomic_long_t total_bytes;
};

struct hpa_dma_buffer {
	struct hpa_dma_heap *heap;
	struct sg_table sg_table;
	void *priv;
};

static struct hpa_dma_heap hpa_heaps[] = {
	{
		.name = "vframe-secure",
		.protection_id = 5,
	},
	{
		.name = "system-secure-vframe-secure",
		.protection_id = 5,
	},
	{
		.name = "vscaler-secure",
		.protection_id = 6,
	},
	{
		.name = "system-secure-gpu_buffer-secure",
		.protection_id = 9,
	},
};

#define HPA_EVENT_LOG	256
#define HPA_EVENT_CLAMP_ID(id) ((id) & (HPA_EVENT_LOG - 1))

static atomic_t hpa_eventid;

enum hpa_event_type {
	HPA_EVENT_ALLOC = 0,
	HPA_EVENT_FREE,
	HPA_EVENT_DMA_MAP,
	HPA_EVENT_DMA_UNMAP,
};

static char * const hpa_event_name[] = {
	"alloc",
	"free",
	"map_dma_buf",
	"unmap_dma_buf",
};

static struct hpa_event {
	ktime_t begin;
	ktime_t done;
	unsigned char heapname[16];
	unsigned long ino;
	size_t size;
	enum hpa_event_type type;
} hpa_events[HPA_EVENT_LOG];

#define hpa_event_begin() ktime_t begin  = ktime_get()

void hpa_event_record(enum hpa_event_type type, struct dma_buf *dmabuf, ktime_t begin)
{
	int idx = HPA_EVENT_CLAMP_ID(atomic_inc_return(&hpa_eventid));
	struct hpa_event *event = &hpa_events[idx];

	event->begin = begin;
	event->done = ktime_get();
	strlcpy(event->heapname, dmabuf->exp_name, sizeof(event->heapname));
	event->ino = file_inode(dmabuf->file)->i_ino;
	event->size = dmabuf->size;
	event->type = type;
}

#ifdef CONFIG_DEBUG_FS
static int hpa_event_show(struct seq_file *s, void *unused)
{
	int index = HPA_EVENT_CLAMP_ID(atomic_read(&hpa_eventid) + 1);
	int i = index;

	seq_printf(s, "%14s %18s %16s %16s %10s %10s\n",
		   "timestamp", "event", "name", "size(kb)", "ino",  "elapsed(us)");

	do {
		struct hpa_event *event = &hpa_events[HPA_EVENT_CLAMP_ID(i)];
		long elapsed = ktime_us_delta(event->done, event->begin);
		struct timespec64 ts = ktime_to_timespec64(event->begin);

		if (elapsed == 0)
			continue;

		seq_printf(s, "[%06ld.%06ld]", ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
		seq_printf(s, "%18s %16s %16zd %10lu %10ld", hpa_event_name[event->type],
			   event->heapname, event->size >> 10, event->ino, elapsed);

		if (elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		seq_puts(s, "\n");
	} while (HPA_EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int hpa_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, hpa_event_show, inode->i_private);
}

static const struct file_operations hpa_event_fops = {
	.open = hpa_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void hpa_debug_init(void)
{
	struct dentry *root = debugfs_create_dir("hpa", NULL);

	if (IS_ERR(root)) {
		pr_err("Failed to create debug directory for dma_heap");
		return;
	}

	if (IS_ERR(debugfs_create_file("event", 0444, root, NULL, &hpa_event_fops)))
		pr_err("Failed to create event file for dma_heap\n");
}
#else
void hpa_debug_init(void)
{
}
#endif
#define MAX_EXCEPTION_AREAS 4
static phys_addr_t hpa_exception_areas[MAX_EXCEPTION_AREAS][2];
static int nr_hpa_exception;

#define HPA_SECURE_DMA_BASE	0x80000000
#define HPA_SECURE_DMA_SIZE	0x80000000

#define HPA_DEFAULT_ORDER 4
#define HPA_CHUNK_SIZE  (PAGE_SIZE << HPA_DEFAULT_ORDER)
#define HPA_PAGE_COUNT(len) (ALIGN(len, HPA_CHUNK_SIZE) / HPA_CHUNK_SIZE)
#define HPA_MAX_CHUNK_COUNT ((PAGE_SIZE * 2) / sizeof(struct page *))

/*
 * Alignment to a secure address larger than 16MiB is not beneficial because
 * the protection alignment just needs 64KiB by the buffer protection H/W and
 * the largest granule of H/W security firewall (the secure context of SysMMU)
 * is 16MiB.
 */
#define MAX_SECURE_VA_ALIGN	(SZ_16M / PAGE_SIZE)

static struct gen_pool *hpa_iova_pool;

static int hpa_secure_iova_pool_init(void)
{
	struct device_node *np;
	phys_addr_t base = HPA_SECURE_DMA_BASE, size = HPA_SECURE_DMA_SIZE;
	int ret;

	hpa_iova_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!hpa_iova_pool) {
		pr_err("failed to create HPA IOVA pool\n");
		return -ENOMEM;
	}

	for_each_node_by_name(np, "secure-iova-domain") {
		phys_addr_t domain_end;
		int naddr = of_n_addr_cells(np);
		int nsize = of_n_size_cells(np);
		const __be32 *prop;
		int len;

		prop = of_get_property(np, "#address-cells", NULL);
		if (prop)
			naddr = be32_to_cpup(prop);

		prop = of_get_property(np, "#size-cells", NULL);
		if (prop)
			nsize = be32_to_cpup(prop);

		prop = of_get_property(np, "domain-ranges", &len);
		if (prop && len > 0) {
			domain_end = (phys_addr_t)of_read_number(prop, naddr);
			prop += naddr;
			domain_end += (phys_addr_t)of_read_number(prop, nsize);
		} else {
			pr_err("%s: failed to get domain-ranges attributes\n", __func__);
			break;
		}

		prop = of_get_property(np, "hpa,reserved", &len);
		if (prop && len > 0) {
			size = (phys_addr_t)of_read_number(prop, nsize);
			base = domain_end - size;
		} else {
			pr_err("%s: failed to get hpa,reserved attributes\n", __func__);
			break;
		}
	}

	ret = gen_pool_add(hpa_iova_pool, base, size, -1);
	if (ret) {
		pr_err("failed to set address range of HPA IOVA pool\n");
		gen_pool_destroy(hpa_iova_pool);
		return ret;
	}
	pr_info("Add hpa iova ranges %#lx-%#lx\n", base, size);

	return 0;
}

unsigned long hpa_iova_alloc(unsigned long size, unsigned int align)
{
	unsigned long out_addr;
	struct genpool_data_align alignment = {
		.align = max_t(int, PFN_DOWN(align), MAX_SECURE_VA_ALIGN),
	};

	if (WARN_ON_ONCE(!hpa_iova_pool))
		return 0;

	out_addr = gen_pool_alloc_algo(hpa_iova_pool, size, gen_pool_first_fit_align, &alignment);
	if (out_addr == 0)
		pr_err("failed alloc secure iova. %zu/%zu bytes used\n",
		       gen_pool_avail(hpa_iova_pool),
		       gen_pool_size(hpa_iova_pool));

	return out_addr;
}

static void hpa_iova_free(unsigned long addr, unsigned long size)
{
	gen_pool_free(hpa_iova_pool, addr, size);
}

static int hpa_secure_protect(struct buffer_prot_info *protdesc, struct device *dev)
{
	unsigned long size = protdesc->chunk_count * protdesc->chunk_size;
	unsigned long ret = 0, dma_addr = 0;

	dma_addr = hpa_iova_alloc(size, max_t(u32, HPA_CHUNK_SIZE, PAGE_SIZE));
	if (!dma_addr)
		return -ENOMEM;

	protdesc->dma_addr = (unsigned int)dma_addr;

	dma_map_single(dev, protdesc, sizeof(*protdesc), DMA_TO_DEVICE);

	ret = ppmp_smc(SMC_DRM_PPMP_PROT, virt_to_phys(protdesc), 0, 0);
	if (ret) {
		dma_unmap_single(dev, phys_to_dma(dev, virt_to_phys(protdesc)),
				 sizeof(*protdesc), DMA_TO_DEVICE);
		hpa_iova_free(dma_addr, size);
		pr_err("CMD %#x (err=%#lx,dva=%#x,size=%#lx,cnt=%u,flg=%u,phy=%#lx)\n",
		       SMC_DRM_PPMP_UNPROT, ret, protdesc->dma_addr,
		       protdesc->chunk_size, protdesc->chunk_count,
		       protdesc->flags, protdesc->bus_address);
		return -EACCES;
	}

	return 0;
}

static int hpa_secure_unprotect(struct buffer_prot_info *protdesc, struct device *dev)
{
	unsigned long size = protdesc->chunk_count * protdesc->chunk_size;
	unsigned long ret;

	dma_unmap_single(dev, phys_to_dma(dev, virt_to_phys(protdesc)),
			 sizeof(*protdesc), DMA_TO_DEVICE);

	ret = ppmp_smc(SMC_DRM_PPMP_UNPROT, virt_to_phys(protdesc), 0, 0);
	if (ret) {
		pr_err("CMD %#x (err=%#lx,dva=%#x,size=%#lx,cnt=%u,flg=%u,phy=%#lx)\n",
		       SMC_DRM_PPMP_UNPROT, ret, protdesc->dma_addr,
		       protdesc->chunk_size, protdesc->chunk_count,
		       protdesc->flags, protdesc->bus_address);
		return -EACCES;
	}
	/*
	 * retain the secure device address if unprotection to its area fails.
	 * It might be unusable forever since we do not know the state of the
	 * secure world before returning error from ppmp_smc() above.
	 */
	hpa_iova_free(protdesc->dma_addr, size);

	return 0;
}

static void *hpa_heap_protect(struct hpa_dma_heap *hpa_heap, struct page **pages,
			      unsigned int nr_pages)
{
	struct buffer_prot_info *protdesc;
	struct device *dev = dma_heap_get_dev(hpa_heap->dma_heap);
	unsigned long *paddr_array;
	unsigned int array_size = 0;
	int i, ret;

	protdesc = kmalloc(sizeof(*protdesc), GFP_KERNEL);
	if (!protdesc)
		return ERR_PTR(-ENOMEM);

	if (nr_pages == 1) {
		protdesc->bus_address = page_to_phys(pages[0]);
	} else {
		paddr_array = kmalloc_array(nr_pages, sizeof(*paddr_array), GFP_KERNEL);
		if (!paddr_array) {
			kfree(protdesc);
			return ERR_PTR(-ENOMEM);
		}

		for (i = 0; i < nr_pages; i++)
			paddr_array[i] = page_to_phys(pages[i]);

		protdesc->bus_address = virt_to_phys(paddr_array);

		kmemleak_ignore(paddr_array);
		array_size = nr_pages * sizeof(*paddr_array);
		dma_map_single(dev, paddr_array, array_size, DMA_TO_DEVICE);
	}

	protdesc->chunk_count = nr_pages,
	protdesc->flags = hpa_heap->protection_id;
	protdesc->chunk_size = HPA_CHUNK_SIZE;

	ret = hpa_secure_protect(protdesc, dev);
	if (ret) {
		if (protdesc->chunk_count > 1) {
			dma_unmap_single(dev, phys_to_dma(dev, protdesc->bus_address),
					 array_size, DMA_TO_DEVICE);
			kfree(paddr_array);
		}
		kfree(protdesc);
		return ERR_PTR(ret);
	}

	return protdesc;
}

static int hpa_heap_unprotect(void *priv, struct device *dev)
{
	struct buffer_prot_info *protdesc = priv;
	int ret = 0;

	if (!priv)
		return 0;

	ret = hpa_secure_unprotect(protdesc, dev);

	if (protdesc->chunk_count > 1) {
		dma_unmap_single(dev, phys_to_dma(dev, protdesc->bus_address),
				 sizeof(unsigned long) * protdesc->chunk_count, DMA_TO_DEVICE);
		kfree(phys_to_virt(protdesc->bus_address));
	}
	kfree(protdesc);

	return ret;
}

static int hpa_compare_pages(const void *p1, const void *p2)
{
	if (*((unsigned long *)p1) > (*((unsigned long *)p2)))
		return 1;
	else if (*((unsigned long *)p1) < (*((unsigned long *)p2)))
		return -1;
	return 0;
}

static struct hpa_dma_buffer* hpa_dma_buffer_alloc(struct hpa_dma_heap *hpa_heap,
						   size_t size, unsigned long nents)
{
	struct hpa_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	if (sg_alloc_table(&buffer->sg_table, nents, GFP_KERNEL)) {
		pr_err("failed to allocate sgtable with %u entry\n", nents);
		kfree(buffer);
		return ERR_PTR(-ENOMEM);
	}

	buffer->heap = hpa_heap;

	return buffer;
}

void hpa_dma_buffer_free(struct hpa_dma_buffer *buffer)
{
	sg_free_table(&buffer->sg_table);
	kfree(buffer);
}

static struct sg_table *hpa_dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static DEFINE_MUTEX(hpa_trace_lock);

struct hpa_trace_task {
	struct list_head node;
	struct task_struct *task;
	struct file *file;
};
static struct list_head hpa_task_list = LIST_HEAD_INIT(hpa_task_list);

#define MAX_HPA_MAP_DEVICE 64
static struct hpa_map_device {
	struct device *dev;
	size_t size;
	unsigned long mapcnt;
} hpa_map_lists[MAX_HPA_MAP_DEVICE];
static unsigned int hpa_map_devices;

static struct hpa_map_device *hpa_trace_get_device(struct device *dev)
{
	unsigned int i;

	for (i = 0; i < hpa_map_devices; i++) {
		if (hpa_map_lists[i].dev == dev)
			return &hpa_map_lists[i];
	}

	BUG_ON(hpa_map_devices == MAX_HPA_MAP_DEVICE);
	hpa_map_lists[hpa_map_devices].dev = dev;

	return &hpa_map_lists[hpa_map_devices++];
}

void hpa_trace_map(struct dma_buf *dmabuf, struct device *dev)
{
	struct hpa_map_device *trace_device;

	mutex_lock(&hpa_trace_lock);
	trace_device = hpa_trace_get_device(dev);

	trace_device->size += dmabuf->size;
	trace_device->mapcnt++;

	mutex_unlock(&hpa_trace_lock);
}

void hpa_trace_unmap(struct dma_buf *dmabuf, struct device *dev)
{
	struct hpa_map_device *trace_device;

	mutex_lock(&hpa_trace_lock);
	trace_device = hpa_trace_get_device(dev);

	trace_device->size -= dmabuf->size;
	trace_device->mapcnt--;

	mutex_unlock(&hpa_trace_lock);
}

static int hpa_trace_task_release(struct inode *inode, struct file *file)
{
	struct hpa_trace_task *task = file->private_data;

	if (!(task->task->flags & PF_EXITING)) {
		pr_err("%s: Invalid to close '%d' on process '%s'(%x, %lx)\n",
		       __func__, task->task->pid, task->task->comm,
		       task->task->flags, task->task->state);
		dump_stack();
	}
	put_task_struct(task->task);

	mutex_lock(&hpa_trace_lock);
	list_del(&task->node);
	mutex_unlock(&hpa_trace_lock);

	kfree(task);

	return 0;
}

static const struct file_operations hpa_trace_task_fops = {
	.release = hpa_trace_task_release,
};

void hpa_trace_add_task(void)
{
	struct hpa_trace_task *task;
	unsigned char name[10];
	int fd;

	if (!current->mm && (current->flags & PF_KTHREAD))
		return;

	if (current->group_leader->pid == 1)
		return;

	mutex_lock(&hpa_trace_lock);

	list_for_each_entry(task, &hpa_task_list, node) {
		if (task->task == current->group_leader) {
			mutex_unlock(&hpa_trace_lock);
			return;
		}
	}

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		goto err;

	get_task_struct(current->group_leader);
	task->task = current->group_leader;

	fd = get_unused_fd_flags(O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		goto err_fd;

	scnprintf(name, 10, "%d", current->group_leader->pid);
	task->file = anon_inode_getfile(name, &hpa_trace_task_fops, task, O_RDWR);
	if (IS_ERR(task->file))
		goto err_inode;

	fd_install(fd, task->file);
	list_add_tail(&task->node, &hpa_task_list);

	mutex_unlock(&hpa_trace_lock);

	return;

err_inode:
	put_unused_fd(fd);
err_fd:
	put_task_struct(current->group_leader);
	kfree(task);
err:
	mutex_unlock(&hpa_trace_lock);
}

static struct sg_table *hpa_heap_map_dma_buf(struct dma_buf_attachment *a,
					     enum dma_data_direction direction)
{
	struct hpa_dma_buffer *buffer = a->dmabuf->priv;
	struct sg_table *table;
	int ret = 0;

	hpa_event_begin();

	table = hpa_dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table))
		return table;

	if (dev_iommu_fwspec_get(a->dev)) {
		struct buffer_prot_info *info = buffer->priv;

		sg_dma_address(table->sgl) = info->dma_addr;
		sg_dma_len(table->sgl) = info->chunk_count * info->chunk_size;
		table->nents = 1;
	} else {
		ret = dma_map_sgtable(a->dev, table, direction, a->dma_map_attrs);
		if (ret) {
			sg_free_table(table);
			kfree(table);

			return ERR_PTR(ret);
		}
	}

	hpa_trace_map(a->dmabuf, a->dev);

	hpa_event_record(HPA_EVENT_DMA_MAP, a->dmabuf, begin);

	return table;
}

static void hpa_heap_unmap_dma_buf(struct dma_buf_attachment *a,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	hpa_event_begin();

	hpa_trace_unmap(a->dmabuf, a->dev);

	if (!dev_iommu_fwspec_get(a->dev))
		dma_unmap_sgtable(a->dev, table, direction, a->dma_map_attrs);

	sg_free_table(table);
	kfree(table);

	hpa_event_record(HPA_EVENT_DMA_UNMAP, a->dmabuf, begin);
}

static int hpa_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	pr_err("mmap() to protected buffer is not allowed\n");
	return -EACCES;
}

static void *hpa_heap_vmap(struct dma_buf *dmabuf)
{
	pr_err("vmap() to protected buffer is not allowed\n");
	return ERR_PTR(-EACCES);
}

static void hpa_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct hpa_dma_buffer *buffer = dmabuf->priv;
	struct device *dev = dma_heap_get_dev(buffer->heap->dma_heap);
	struct sg_table *sgt = &buffer->sg_table;
	struct scatterlist *sg;
	int i;
	int unprot_err;

	hpa_event_begin();

	unprot_err = hpa_heap_unprotect(buffer->priv, dev);

	if (!unprot_err) {
		for_each_sgtable_sg(sgt, sg, i)
			__free_pages(sg_page(sg), HPA_DEFAULT_ORDER);

		atomic_long_sub(dmabuf->size, &buffer->heap->total_bytes);
	}

	hpa_dma_buffer_free(buffer);

	hpa_event_record(HPA_EVENT_FREE, dmabuf, begin);
}

#define HPA_HEAP_FLAG_PROTECTED BIT(1)

static int hpa_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags)
{
	*flags = HPA_HEAP_FLAG_PROTECTED;

	return 0;
}

const struct dma_buf_ops hpa_dma_buf_ops = {
	.map_dma_buf = hpa_heap_map_dma_buf,
	.unmap_dma_buf = hpa_heap_unmap_dma_buf,
	.mmap = hpa_heap_mmap,
	.vmap = hpa_heap_vmap,
	.release = hpa_heap_dma_buf_release,
	.get_flags = hpa_heap_dma_buf_get_flags,
};

static void hpa_cache_flush(struct device *dev, struct hpa_dma_buffer *buffer)
{
	dma_map_sgtable(dev, &buffer->sg_table, DMA_TO_DEVICE, 0);
	dma_unmap_sgtable(dev, &buffer->sg_table, DMA_FROM_DEVICE, 0);
}

static void hpa_pages_clean(struct sg_table *sgt)
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

static struct dma_buf *hpa_heap_allocate(struct dma_heap *heap, unsigned long len,
					 unsigned long fd_flags, unsigned long heap_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct hpa_dma_heap *hpa_heap = dma_heap_get_drvdata(heap);
	struct device *dev = dma_heap_get_dev(heap);
	struct hpa_dma_buffer *buffer;
	struct scatterlist *sg;
	struct dma_buf *dmabuf;
	struct page **pages;
	unsigned long size, nr_pages;
	int ret, protret = 0;
	pgoff_t pg;

	hpa_event_begin();

	size = ALIGN(len, HPA_CHUNK_SIZE);
	nr_pages = size / HPA_CHUNK_SIZE;

	if (nr_pages > HPA_MAX_CHUNK_COUNT) {
		pr_err("Too big size %lu (HPA limited size %lu)\n", len,
		       HPA_MAX_CHUNK_COUNT * HPA_CHUNK_SIZE);
		return ERR_PTR(-EINVAL);
	}

	pages = kmalloc_array(nr_pages, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return ERR_PTR(-ENOMEM);

	ret = alloc_pages_highorder_except(HPA_DEFAULT_ORDER, pages, nr_pages,
					   hpa_exception_areas, nr_hpa_exception);
	if (ret)
		goto err_alloc;

	sort(pages, nr_pages, sizeof(*pages), hpa_compare_pages, NULL);

	buffer = hpa_dma_buffer_alloc(hpa_heap, size, nr_pages);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);
		goto err_buffer;
	}

	sg = buffer->sg_table.sgl;
	for (pg = 0; pg < nr_pages; pg++) {
		sg_set_page(sg, pages[pg], HPA_CHUNK_SIZE, 0);
		sg = sg_next(sg);
	}

	hpa_pages_clean(&buffer->sg_table);
	hpa_cache_flush(dev, buffer);

	buffer->priv = hpa_heap_protect(hpa_heap, pages, nr_pages);
	if (IS_ERR(buffer->priv)) {
		ret = PTR_ERR(buffer->priv);
		goto err_prot;
	}

	exp_info.ops = &hpa_dma_buf_ops;
	exp_info.exp_name = hpa_heap->name;
	exp_info.size = size;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto err_export;
	}

	atomic_long_add(dmabuf->size, &hpa_heap->total_bytes);

	kvfree(pages);

	hpa_trace_add_task();

	hpa_event_record(HPA_EVENT_ALLOC, dmabuf, begin);

	return dmabuf;

err_export:
	protret = hpa_heap_unprotect(buffer->priv, dev);
err_prot:
	hpa_dma_buffer_free(buffer);
err_buffer:
	for (pg = 0; !protret && pg < nr_pages; pg++)
		__free_pages(pages[pg], HPA_DEFAULT_ORDER);
err_alloc:
	kvfree(pages);

	pr_err("failed to alloc (len %zu, %#lx %#lx) from %s heap",
	       len, fd_flags, heap_flags, hpa_heap->name);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops hpa_heap_ops = {
	.allocate = hpa_heap_allocate,
};

static void hpa_add_exception_area(void)
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

		prop = of_get_property(np, "ppmpu-limit", &len);
		if (prop && len > 0) {
			base = (phys_addr_t)of_read_number(prop, naddr);

			hpa_exception_areas[0][0] = base;
			hpa_exception_areas[0][1] = -1;

			nr_hpa_exception++;
		}

		prop = of_get_property(np, "exception-range", &len);
		if (prop && len > 0) {
			int n_area = len / (sizeof(*prop) * (nsize + naddr));

			n_area += nr_hpa_exception;
			n_area = min_t(int, n_area, MAX_EXCEPTION_AREAS);

			for (i = nr_hpa_exception; i < n_area ; i++) {
				base = (phys_addr_t)of_read_number(prop, naddr);
				prop += naddr;
				size = (phys_addr_t)of_read_number(prop, nsize);
				prop += nsize;

				hpa_exception_areas[i][0] = base;
				hpa_exception_areas[i][1] = base + size - 1;
			}

			nr_hpa_exception = n_area;
		}
	}
}

static void show_hpa_heap_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	struct hpa_dma_heap *heap = data;
	u64 total_size_kb = div_u64(atomic_long_read(&heap->total_bytes), 1024);

	if (total_size_kb)
		pr_info("%s: %lukb ", heap->name, total_size_kb);
}

struct hpa_fd_iterdata {
	int fd_ref_size;
	int fd_ref_cnt;
};

int hpa_traverse_filetable(const void *t, struct file *file, unsigned fd)
{
	struct hpa_fd_iterdata *iterdata = (struct hpa_fd_iterdata*)t;
	struct dma_buf *dmabuf;

	if (!is_dma_buf_file(file))
		return 0;

	dmabuf = file->private_data;

	iterdata->fd_ref_cnt++;
	iterdata->fd_ref_size += dmabuf->size >> 10;

	return 0;
}

/*
 * The HPA buffers are referenced by attached device, and file descriptor of process.
 * These buffers don't need to consider mmap count because secure buffer can not be mmaped.
 * The file descriptor might be tracked on dma-buf-trace because that interates dma-buf
 * file descriptor when show_mem() calls, so we only show devices to attach the hpa buffers.
 */
static void show_hpa_trace_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	static DEFINE_RATELIMIT_STATE(hpa_map_ratelimit, HZ * 10, 1);
	struct hpa_trace_task *task;
	int i;
	bool show_header = false;

	if (!__ratelimit(&hpa_map_ratelimit))
		return;

	if (in_interrupt()) {
		pr_info("log is skipped in interrupt context\n");
		return;
	}

	if (!mutex_trylock(&hpa_trace_lock))
		return;

	if (!list_empty(&hpa_task_list))
		pr_info("%20s %5s %10s %10s (HPA allocation tasks)\n",
			"comm", "pid", "fdrefcnt", "fdsize(kb)");

	list_for_each_entry(task, &hpa_task_list, node) {
		struct hpa_fd_iterdata iterdata;
		bool locked;

		iterdata.fd_ref_cnt = iterdata.fd_ref_size = 0;

		locked = spin_trylock(&task->task->alloc_lock);
		if (locked) {
			iterate_fd(task->task->files, 0, hpa_traverse_filetable, &iterdata);
			spin_unlock(&task->task->alloc_lock);
		}

		pr_info("%20s %5d %10d %10zu %s\n", task->task->comm, task->task->pid,
			iterdata.fd_ref_cnt, iterdata.fd_ref_size,
			locked ? "" : "-> skip by lock contention");
	}

	for (i = 0; i < hpa_map_devices; i++) {
		if (!hpa_map_lists[i].size && !hpa_map_lists[i].mapcnt)
			continue;

		if (!show_header) {
			pr_info("HPA attached device list:\n");
			pr_info("%20s %20s %10s\n", "attached device", "size(kb)", "mapcount");

			show_header = true;
		}

		pr_info("%20s %20zu %10lu\n", dev_name(hpa_map_lists[i].dev),
			hpa_map_lists[i].size, hpa_map_lists[i].mapcnt);
	}
	mutex_unlock(&hpa_trace_lock);
}

static int __init hpa_dma_heap_init(void)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *dma_heap;
	int ret, i = 0;

	ret = hpa_secure_iova_pool_init();
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(hpa_heaps); i++) {
		exp_info.name = hpa_heaps[i].name;
		exp_info.priv = &hpa_heaps[i];
		exp_info.ops = &hpa_heap_ops;

		dma_heap = dma_heap_add(&exp_info);
		if (IS_ERR(dma_heap))
			return PTR_ERR(dma_heap);

		hpa_heaps[i].dma_heap = dma_heap;
		dma_coerce_mask_and_coherent(dma_heap_get_dev(dma_heap), DMA_BIT_MASK(36));
		register_trace_android_vh_show_mem(show_hpa_heap_handler, &hpa_heaps[i]);

		pr_info("Registered %s dma-heap successfully\n", hpa_heaps[i].name);
	}
	register_trace_android_vh_show_mem(show_hpa_trace_handler, NULL);

	hpa_add_exception_area();
	hpa_debug_init();

	return 0;
}
module_init(hpa_dma_heap_init);
MODULE_LICENSE("GPL v2");
