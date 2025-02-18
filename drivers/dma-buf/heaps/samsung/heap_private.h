// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Allocator - Internal header
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#ifndef _HEAP_PRIVATE_H
#define _HEAP_PRIVATE_H

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "secure_buffer.h"
#include "../deferred-free-helper.h"

struct dma_iovm_map {
	struct list_head list;
	struct device *dev;
	struct sg_table table;
	unsigned long attrs;
	unsigned int mapcnt;
};

struct dmabuf_trace_buffer;

struct samsung_dma_buffer {
	struct samsung_dma_heap *heap;
	struct dmabuf_trace_buffer *trace_buffer;
	struct list_head attachments;
	/* Manage buffer resource of attachments and vaddr, vmap_cnt */
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	void *vaddr;
	void *priv;
	unsigned long flags;
	int vmap_cnt;
	struct deferred_freelist_item deferred_free;
};

struct samsung_dma_heap {
	struct dma_heap *dma_heap;
	void (*release)(struct samsung_dma_buffer *buffer);
	void *priv;
	const char *name;
	unsigned long flags;
	unsigned int alignment;
	unsigned int protection_id;
	atomic_long_t total_bytes;
};

extern const struct dma_buf_ops samsung_dma_buf_ops;

#define DEFINE_SAMSUNG_DMA_BUF_EXPORT_INFO(name, heap_name)	\
	struct dma_buf_export_info name = { .exp_name = heap_name, \
					 .owner = THIS_MODULE }

enum dma_heap_event_type {
	DMA_HEAP_EVENT_ALLOC = 0,
	DMA_HEAP_EVENT_FREE,
	DMA_HEAP_EVENT_MMAP,
	DMA_HEAP_EVENT_VMAP,
	DMA_HEAP_EVENT_VUNMAP,
	DMA_HEAP_EVENT_DMA_MAP,
	DMA_HEAP_EVENT_DMA_UNMAP,
	DMA_HEAP_EVENT_CPU_BEGIN,
	DMA_HEAP_EVENT_CPU_END,
	DMA_HEAP_EVENT_CPU_BEGIN_PARTIAL,
	DMA_HEAP_EVENT_CPU_END_PARTIAL,
};

#define dma_heap_event_begin() ktime_t begin  = ktime_get()
void dma_heap_event_record(enum dma_heap_event_type type, struct dma_buf *dmabuf, ktime_t begin);

bool is_dma_heap_exception_page(struct page *page);
void heap_sgtable_pages_clean(struct sg_table *sgt);
void heap_cache_flush(struct samsung_dma_buffer *buffer);
void heap_page_clean(struct page *pages, unsigned long size);
struct samsung_dma_buffer *samsung_dma_buffer_alloc(struct samsung_dma_heap *samsung_dma_heap,
						    unsigned long size, unsigned int nents);
void samsung_dma_buffer_free(struct samsung_dma_buffer *buffer);
int samsung_heap_add(struct device *dev, void *priv,
		     void (*release)(struct samsung_dma_buffer *buffer),
		     const struct dma_heap_ops *ops);
struct dma_buf *samsung_export_dmabuf(struct samsung_dma_buffer *buffer, unsigned long fd_flags);
void show_dmabuf_trace_info(void);
void show_dmabuf_dva(struct device *dev);

#define DMA_HEAP_VIDEO_PADDING (512)
#define dma_heap_add_video_padding(len) (PAGE_ALIGN((len) + DMA_HEAP_VIDEO_PADDING))

#define DMA_HEAP_FLAG_UNCACHED  BIT(0)
#define DMA_HEAP_FLAG_PROTECTED BIT(1)
#define DMA_HEAP_FLAG_VIDEO_ALIGNED BIT(2)
#define DMA_HEAP_FLAG_UNTOUCHABLE BIT(3)

static inline bool dma_heap_flags_uncached(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_UNCACHED);
}

static inline bool dma_heap_flags_protected(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_PROTECTED);
}

static inline bool dma_heap_flags_untouchable(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_UNTOUCHABLE);
}

static inline bool dma_heap_skip_cache_ops(unsigned long flags)
{
	return dma_heap_flags_protected(flags) || dma_heap_flags_uncached(flags);
}

static inline bool dma_heap_flags_video_aligned(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_VIDEO_ALIGNED);
}

/*
 * Use pre-mapped protected device virtual address instead of dma-mapping.
 */
static inline bool dma_heap_tzmp_buffer(struct device *dev, unsigned long flags)
{
	return dma_heap_flags_protected(flags) && !!dev_iommu_fwspec_get(dev);
}

void *samsung_dma_buffer_protect(struct samsung_dma_heap *heap, unsigned int chunk_size,
				 unsigned int nr_pages, unsigned long paddr);
int samsung_dma_buffer_unprotect(void *priv, struct device *dev);

int __init dmabuf_trace_create(void);
void dmabuf_trace_remove(void);

int dmabuf_trace_alloc(struct dma_buf *dmabuf);
void dmabuf_trace_free(struct dma_buf *dmabuf);
int dmabuf_trace_track_buffer(struct dma_buf *dmabuf);
int dmabuf_trace_untrack_buffer(struct dma_buf *dmabuf);
void dmabuf_trace_map(struct dma_buf_attachment *a);
void dmabuf_trace_unmap(struct dma_buf_attachment *a);

static inline u64 samsung_heap_total_kbsize(struct samsung_dma_heap *heap)
{
	return div_u64(atomic_long_read(&heap->total_bytes), 1024);
}

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_SYSTEM)
int __init system_dma_heap_init(void);
void system_dma_heap_exit(void);
#else
static inline int __init system_dma_heap_init(void)
{
	return 0;
}

#define system_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_CMA)
int __init cma_dma_heap_init(void);
void cma_dma_heap_exit(void);
#else
static inline int __init cma_dma_heap_init(void)
{
	return 0;
}

#define cma_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_CARVEOUT)
int __init carveout_dma_heap_init(void);
void carveout_dma_heap_exit(void);
#else
static inline int __init carveout_dma_heap_init(void)
{
	return 0;
}

#define carveout_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_RBIN)
int carveout_heap_probe(struct platform_device *pdev);
int __init rbin_dma_heap_init(void);
void rbin_dma_heap_exit(void);
#else
static inline int __init rbin_dma_heap_init(void)
{
	return 0;
}

#define rbin_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_CHUNK)
int __init chunk_dma_heap_init(void);
void chunk_dma_heap_exit(void);
#else
static inline int __init chunk_dma_heap_init(void)
{
	return 0;
}

#define chunk_dma_heap_exit() do { } while (0)
#endif

#define DMAHEAP_PREFIX "[Exynos][DMA-HEAP] "
#define perr(format, arg...) \
	pr_err(DMAHEAP_PREFIX format "\n", ##arg)

#define perrfn(format, arg...) \
	pr_err(DMAHEAP_PREFIX "%s: " format "\n", __func__, ##arg)

#define perrdev(dev, format, arg...) \
	dev_err(dev, DMAHEAP_PREFIX format "\n", ##arg)

#define perrfndev(dev, format, arg...) \
	dev_err(dev, DMAHEAP_PREFIX "%s: " format "\n", __func__, ##arg)

static inline void samsung_allocate_error_report(struct samsung_dma_heap *heap, unsigned long len,
						 unsigned long fd_flags, unsigned long heap_flags)
{
	perrfn("failed to alloc (len %zu, %#lx %#lx) from %s heap (total allocated %lu kb)",
	       len, fd_flags, heap_flags, heap->name, samsung_heap_total_kbsize(heap));
}

#endif /* _HEAP_PRIVATE_H */
