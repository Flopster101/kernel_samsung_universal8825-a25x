// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */


#include <linux/types.h>
#include <linux/slab.h>
#include <linux/iommu.h>
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
#include <linux/ion_exynos.h>
#include <linux/exynos_iovmm.h>
#include <asm/cacheflush.h>
#else
#include <linux/dma-buf.h>
#include <linux/ion.h>
#endif
#include <linux/of_reserved_mem.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-memory.h"

#if !defined(CONFIG_EXYNOS_IOVMM) || !defined(CONFIG_ION_EXYNOS)
static bool __dsp_hw_common_memory_ion_cached_dmabuf(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer;

	dsp_check();
	buffer = dmabuf->priv;
	return !!(buffer->flags & ION_FLAG_CACHED);
}
#endif

int dsp_hw_common_memory_map_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	int ret;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t iova;

	dsp_enter();
	if (buf->fd <= 0) {
		ret = -EINVAL;
		dsp_err("fd(%d) is invalid\n", buf->fd);
		goto p_err;
	}

	dbuf = dma_buf_get(buf->fd);
	if (IS_ERR(dbuf)) {
		ret = PTR_ERR(dbuf);
		dsp_err("dma_buf is invalid(%d/%d)\n", buf->fd, ret);
		goto p_err;
	}
	buf->dbuf = dbuf;

	if (buf->size + buf->offset > dbuf->size) {
		ret = -EINVAL;
		dsp_err("size is invalid(%zu/%u/%zu)\n",
				buf->size, buf->offset, dbuf->size);
		goto p_err_size;
	}
	buf->dbuf_size = dbuf->size;

	attach = dma_buf_attach(dbuf, mem->dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dsp_err("Failed to attach dma_buf(%d)\n", ret);
		goto p_err_attach;
	}
	buf->attach = attach;

	sgt = dma_buf_map_attachment(attach, buf->dir);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dsp_err("Failed to map attachment(%d)\n", ret);
		goto p_err_map_attach;
	}
	buf->sgt = sgt;

#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	iova = ion_iovmm_map(attach, 0, buf->dbuf_size, buf->dir, IOMMU_CACHE);
#else
	iova = sg_dma_address(buf->sgt->sgl);
#endif
	if (IS_ERR_VALUE(iova)) {
		ret = (int)iova;
		dsp_err("Failed to map iova(%d)\n", ret);
		goto p_err_map_dva;
	}
	buf->iova = iova;

	/* dma-coherent is supported. cached values used only for debugging */
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	buf->cached = ion_cached_dmabuf(dbuf);
#else
	buf->cached = __dsp_hw_common_memory_ion_cached_dmabuf(dbuf);
#endif

	dsp_dbg("buffer (%d/%zu/%#x/%d)\n",
			buf->fd, buf->dbuf_size, (int)iova, buf->cached);

	dsp_leave();
	return 0;
p_err_map_dva:
	dma_buf_unmap_attachment(attach, sgt, buf->dir);
p_err_map_attach:
	dma_buf_detach(dbuf, attach);
p_err_attach:
p_err_size:
	dma_buf_put(dbuf);
p_err:
	return ret;
}

int dsp_hw_common_memory_unmap_buffer(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	ion_iovmm_unmap(buf->attach, buf->iova);
#endif
	dma_buf_unmap_attachment(buf->attach, buf->sgt, buf->dir);
	dma_buf_detach(buf->dbuf, buf->attach);
	dma_buf_put(buf->dbuf);
	dsp_leave();
	return 0;
}

int dsp_hw_common_memory_sync_for_device(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	if (!buf->cached)
		return 0;

	dsp_leave();
	return 0;
}

int dsp_hw_common_memory_sync_for_cpu(struct dsp_memory *mem,
		struct dsp_buffer *buf)
{
	dsp_enter();
	if (!buf->cached)
		return 0;

	dsp_leave();
	return 0;
}

static int __dsp_hw_common_memory_iovmm_map_sg(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	size_t size;

	dsp_enter();
	size = iommu_map_sg(mem->domain, pmem->iova, pmem->sgt->sgl,
			pmem->sgt->orig_nents, 0);
	if (!size) {
		dsp_err("Failed to map sg[%s]\n", pmem->name);
		return -ENOMEM;
	}

	if (size != pmem->size) {
		dsp_warn("pmem size(%zd) is different from mapped size(%zd)\n",
				pmem->size, size);
		pmem->size = size;
	}

	dsp_leave();
	return 0;
}

#ifndef CONFIG_DMABUF_HEAPS
#if !defined(CONFIG_EXYNOS_IOVMM) || !defined(CONFIG_ION_EXYNOS)
static unsigned int __dsp_hw_common_get_heap_mask(const char *heap_name)
{
	unsigned int heap_mask = 0;
	int idx, cnt;
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];

	dsp_enter();
	cnt = ion_query_heaps_kernel(NULL, 0);
	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);

	for (idx = 0; idx < cnt; ++idx) {
		if (!strncmp(data[idx].name, heap_name, MAX_HEAP_NAME)) {
			heap_mask = 1 << data[idx].heap_id;
			goto p_found;
		}
	}

p_found:
	dsp_leave();
	return heap_mask;
}
#endif
#endif

#ifdef CONFIG_DMABUF_HEAPS
static struct dma_buf *__dsp_hw_common_memory_ion_alloc(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	dsp_check();
	if (pmem->flags)
		return dma_heap_buffer_alloc(mem->system_heap, pmem->size,
				0, 0);
	else
		return dma_heap_buffer_alloc(mem->system_uncached_heap,
				pmem->size, 0, 0);
}
#else
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
static struct dma_buf *__dsp_hw_common_memory_ion_alloc(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	dsp_check();
	return ion_alloc_dmabuf("ion_system_heap", pmem->size, pmem->flags);
}
#else
static struct dma_buf *__dsp_hw_common_memory_ion_alloc(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	unsigned int heap_mask;

	dsp_enter();
	heap_mask = __dsp_hw_common_get_heap_mask("vendor_system_heap");
	if (!heap_mask) {
		heap_mask = __dsp_hw_common_get_heap_mask("ion_system_heap");
		if (!heap_mask) {
			dsp_err("Failed to get system system mask\n");
			return ERR_PTR(-EINVAL);
		}
	}

	dsp_leave();
	return ion_alloc(pmem->size, heap_mask, pmem->flags);
}
#endif // defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
#endif // CONFIG_DMABUF_HEAPS

int dsp_hw_common_memory_ion_alloc(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	int ret;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t iova;
	void *kvaddr;

	dsp_enter();
	if (!pmem->size)
		return 0;

	dbuf = __dsp_hw_common_memory_ion_alloc(mem, pmem);
	if (IS_ERR(dbuf)) {
		ret = PTR_ERR(dbuf);
		dsp_err("Failed to allocate dma_buf(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_alloc;
	}
	pmem->dbuf = dbuf;
	pmem->dbuf_size = dbuf->size;

	attach = dma_buf_attach(dbuf, mem->dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dsp_err("Failed to attach dma_buf(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_attach;
	}
	pmem->attach = attach;

	sgt = dma_buf_map_attachment(attach, pmem->dir);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dsp_err("Failed to map attachment(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_map_attach;
	}
	pmem->sgt = sgt;

	if (pmem->kmap) {
		kvaddr = dma_buf_vmap(dbuf);
		if (!kvaddr) {
			ret = -EFAULT;
			dsp_err("Failed to map kvaddr(%d)[%s]\n",
					ret, pmem->name);
			goto p_err_kmap;
		}
		pmem->kvaddr = kvaddr;

		if (pmem->backup) {
			kvaddr = vzalloc(pmem->size);
			if (!kvaddr) {
				ret = -ENOMEM;
				dsp_err("Failed to alloc backup[%s]\n",
						pmem->name);
				goto p_err_backup;
			}
			pmem->bac_kvaddr = kvaddr;
		} else {
			pmem->bac_kvaddr = NULL;
		}
	} else {
		pmem->kvaddr = NULL;
	}

	if (pmem->fixed_iova) {
		ret = __dsp_hw_common_memory_iovmm_map_sg(mem, pmem);
		if (ret)
			goto p_err_map_dva;
	} else {
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
		iova = ion_iovmm_map(attach, 0, pmem->size, pmem->dir,
				IOMMU_CACHE);
#else
		iova = sg_dma_address(pmem->sgt->sgl);
#endif
		if (IS_ERR_VALUE(iova)) {
			ret = (int)iova;
			dsp_err("Failed to map iova(%d)[%s]\n",
					ret, pmem->name);
			goto p_err_map_dva;
		}
		pmem->iova = iova;
	}

	dsp_info("[%16s] %#x/%zuKB(%u)\n",
			pmem->name, (int)pmem->iova, pmem->size / SZ_1K,
			pmem->kmap);

	dsp_leave();
	return 0;
p_err_map_dva:
	if (pmem->kmap && pmem->backup)
		vfree(pmem->bac_kvaddr);
p_err_backup:
	if (pmem->kmap)
		dma_buf_vunmap(dbuf, pmem->kvaddr);
p_err_kmap:
	dma_buf_unmap_attachment(attach, sgt, pmem->dir);
p_err_map_attach:
	dma_buf_detach(dbuf, attach);
p_err_attach:
	dma_buf_put(dbuf);
p_err_alloc:
	return ret;
}

#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
extern void exynos_sysmmu_tlb_invalidate(struct iommu_domain *iommu_domain,
		dma_addr_t d_start, size_t size);
#endif

static int __dsp_hw_common_memory_iovmm_unmap(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	size_t size;

	dsp_enter();
	size = iommu_unmap(mem->domain, pmem->iova, pmem->size);
	if (!size) {
		dsp_err("Failed to unmap sg[%s]\n", pmem->name);
		return -EINVAL;
	}
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	exynos_sysmmu_tlb_invalidate(mem->domain, pmem->iova, pmem->size);
#endif

	dsp_leave();
	return 0;
}

void dsp_hw_common_memory_ion_free(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	if (!pmem->size)
		return;

	if (pmem->fixed_iova)
		__dsp_hw_common_memory_iovmm_unmap(mem, pmem);
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	else
		ion_iovmm_unmap(pmem->attach, pmem->iova);
#endif

	if (pmem->kmap && pmem->backup)
		vfree(pmem->bac_kvaddr);

	if (pmem->kmap)
		dma_buf_vunmap(pmem->dbuf, pmem->kvaddr);

	dma_buf_unmap_attachment(pmem->attach, pmem->sgt, pmem->dir);
	dma_buf_detach(pmem->dbuf, pmem->attach);
	dma_buf_put(pmem->dbuf);
	dsp_leave();
}

#ifdef CONFIG_DMABUF_HEAPS
static struct dma_buf *__dsp_hw_common_memory_ion_alloc_secure(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	dsp_check();
	if (mem->secure_heap) {
		return dma_heap_buffer_alloc(mem->secure_heap, pmem->size,
				0, 0);
	} else {
		dsp_err("There is no secure dsp heap\n");
		return ERR_PTR(-EINVAL);
	}
}
#else
#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
static struct dma_buf *__dsp_hw_common_memory_ion_alloc_secure(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	dsp_check();
	return ion_alloc_dmabuf("secure_dsp_heap", pmem->size, pmem->flags);
}
#else
static struct dma_buf *__dsp_hw_common_memory_ion_alloc_secure(
		struct dsp_memory *mem, struct dsp_priv_mem *pmem)
{
	unsigned int heap_mask;

	dsp_enter();
	heap_mask = __dsp_hw_common_get_heap_mask("secure_dsp_heap");
	if (!heap_mask) {
		dsp_err("Failed to get secure heap mask\n");
		return ERR_PTR(-EINVAL);
	}

	dsp_leave();
	return ion_alloc(pmem->size, heap_mask, pmem->flags);
}
#endif // defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
#endif // CONFIG_DMABUF_HEAPS

int dsp_hw_common_memory_ion_alloc_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	int ret;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	dsp_enter();
	if (!pmem->size)
		return 0;

	dbuf = __dsp_hw_common_memory_ion_alloc_secure(mem, pmem);
	if (IS_ERR(dbuf)) {
		ret = PTR_ERR(dbuf);
		dsp_err("Failed to allocate dma_buf(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_alloc;
	}
	pmem->dbuf = dbuf;
	pmem->dbuf_size = dbuf->size;

	attach = dma_buf_attach(dbuf, mem->dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dsp_err("Failed to attach dma_buf(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_attach;
	}
	pmem->attach = attach;

	sgt = dma_buf_map_attachment(attach, pmem->dir);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dsp_err("Failed to map attachment(%d)[%s]\n",
				ret, pmem->name);
		goto p_err_map_attach;
	}
	pmem->sgt = sgt;
	pmem->iova = sg_phys(sgt->sgl);

	dsp_info("[%16s] %zuKB\n", pmem->name, pmem->size / SZ_1K);
	dsp_leave();
	return 0;
p_err_map_attach:
	dma_buf_detach(dbuf, attach);
p_err_attach:
	dma_buf_put(dbuf);
p_err_alloc:
	return ret;
}

void dsp_hw_common_memory_ion_free_secure(struct dsp_memory *mem,
		struct dsp_priv_mem *pmem)
{
	dsp_enter();
	if (!pmem->size)
		return;

	dma_buf_unmap_attachment(pmem->attach, pmem->sgt, pmem->dir);
	dma_buf_detach(pmem->dbuf, pmem->attach);
	dma_buf_put(pmem->dbuf);
	dsp_leave();
}

static int __dsp_hw_common_memory_iommu_map_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	int ret;

	dsp_enter();
	ret = iommu_map(mem->domain, rmem->iova, rmem->paddr, rmem->size,
			rmem->flags);
	if (ret) {
		dsp_err("Failed to map rmem[%s](%d)\n", rmem->name, ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_memory_iommu_unmap_reserved_mem(
		struct dsp_memory *mem, struct dsp_reserved_mem *rmem)
{
	size_t size;

	dsp_enter();
	size = iommu_unmap(mem->domain, rmem->iova, rmem->size);
	if (!size) {
		dsp_err("Failed to unmap rmem[%s]\n", rmem->name);
		return -EINVAL;
	}

	dsp_leave();
	return 0;
}

static int __dsp_hw_common_memory_kmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	int ret;
	unsigned int num_pages;
	struct page **pages, **page;
	phys_addr_t phys;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);

	dsp_enter();
	if (!rmem->kmap)
		return 0;

	num_pages = (unsigned int)DIV_ROUND_UP(rmem->size, PAGE_SIZE);
	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc pages for reserved_mem vmap[%s]\n",
				rmem->name);
		goto p_err;
	}

	phys = rmem->paddr;
	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	rmem->kvaddr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);
	if (!rmem->kvaddr) {
		ret = -ENOMEM;
		dsp_err("Failed to vmap %u pages[%s]\n", num_pages, rmem->name);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_common_memory_kunmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	dsp_enter();
	if (!rmem->kmap)
		return 0;

	vunmap(rmem->kvaddr);
	dsp_leave();
	return 0;
}

int dsp_hw_common_memory_map_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_memory_iommu_map_reserved_mem(mem, rmem);
	if (ret)
		goto p_err;

	if (rmem->kmap) {
		ret = __dsp_hw_common_memory_kmap_reserved_mem(mem, rmem);
		if (ret)
			goto p_err_kmap;
	}

	dsp_info("[%16s] %#x/%zuKB(%u)\n",
			rmem->name, (int)rmem->iova, rmem->size / SZ_1K,
			rmem->kmap);

	dsp_leave();
	return 0;
p_err_kmap:
	__dsp_hw_common_memory_iommu_unmap_reserved_mem(mem, rmem);
p_err:
	return ret;
}

void dsp_hw_common_memory_unmap_reserved_mem(struct dsp_memory *mem,
		struct dsp_reserved_mem *rmem)
{
	dsp_enter();
	if (rmem->kmap)
		__dsp_hw_common_memory_kunmap_reserved_mem(mem, rmem);

	__dsp_hw_common_memory_iommu_unmap_reserved_mem(mem, rmem);
	dsp_leave();
}

static int __dsp_hw_common_memory_priv_alloc(struct dsp_memory *mem)
{
	int ret;
	struct dsp_priv_mem *pmem;
	int idx;

	dsp_enter();
	pmem = mem->priv_mem;

	for (idx = 0; idx < mem->priv_mem_count; ++idx) {
		ret = mem->ops->ion_alloc(mem, &pmem[idx]);
		if (ret)
			goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	for (idx -= 1; idx >= 0; --idx)
		mem->ops->ion_free(mem, &pmem[idx]);

	return ret;
}

int dsp_hw_common_memory_open(struct dsp_memory *mem)
{
	int ret;

	dsp_enter();
	ret = __dsp_hw_common_memory_priv_alloc(mem);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_common_memory_priv_free(struct dsp_memory *mem)
{
	struct dsp_priv_mem *pmem;
	int idx;

	dsp_enter();
	pmem = mem->priv_mem;

	for (idx = 0; idx < mem->priv_mem_count; ++idx)
		mem->ops->ion_free(mem, &pmem[idx]);

	dsp_leave();
}

int dsp_hw_common_memory_close(struct dsp_memory *mem)
{
	dsp_enter();
	__dsp_hw_common_memory_priv_free(mem);
	dsp_leave();
	return 0;
}

static int __dsp_hw_common_memory_parse_dt(struct dsp_memory *mem)
{
#ifdef CONFIG_EXYNOS_DSP_HW_N3
	dsp_check();
	return 0;
#else
	int ret, idx;
	struct device_node *np;
	struct reserved_mem *rmem;
	struct dsp_reserved_mem *d_rmem;

	dsp_enter();
	for (idx = 0; idx < mem->reserved_mem_count; ++idx) {
		np = of_parse_phandle(mem->dev->of_node, "memory-region", idx);
		if (!np) {
			ret = -EFAULT;
			dsp_err("Failed to get memory-region%u from DT\n", idx);
			goto p_err;
		}

		rmem = of_reserved_mem_lookup(np);
		if (!rmem) {
			ret = -EFAULT;
			dsp_err("Failed to get reserved_mem%u\n", idx);
			goto p_err;
		}
		dsp_info("rmem%u %pa/%pa\n", idx, &rmem->base, &rmem->size);

		d_rmem = &mem->reserved_mem[idx];
		d_rmem->paddr = rmem->base;
		d_rmem->size = rmem->size;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
#endif
}

int dsp_hw_common_memory_probe(struct dsp_memory *mem, void *sys)
{
	int ret;
	struct dsp_priv_mem *pmem;
	struct dsp_reserved_mem *rmem;

	dsp_enter();
	mem->sys = sys;
	mem->dev = mem->sys->dev;

#if defined(CONFIG_EXYNOS_IOVMM) && defined(CONFIG_ION_EXYNOS)
	dma_set_mask(mem->dev, DMA_BIT_MASK(36));
	mem->domain = get_domain_from_dev(mem->dev);
#else
	dma_set_mask(mem->dev, DMA_BIT_MASK(32));
	mem->domain = iommu_get_domain_for_dev(mem->dev);
#endif

#ifdef CONFIG_DMABUF_HEAPS
	mem->system_heap = dma_heap_find("system");
	if (mem->system_heap) {
		dma_heap_put(mem->system_heap);
	} else {
		ret = -EINVAL;
		dsp_err("Failed to get system dma heap\n");
		goto p_err;
	}

	mem->system_uncached_heap = dma_heap_find("system-uncached");
	if (mem->system_uncached_heap) {
		dma_heap_put(mem->system_uncached_heap);
	} else {
		ret = -EINVAL;
		dsp_err("Failed to get system-uncached dma heap\n");
		goto p_err;
	}

	mem->secure_heap = dma_heap_find("secure_dsp_heap");
	if (mem->secure_heap)
		dma_heap_put(mem->secure_heap);
	else
		dsp_warn("Failed to get secure dsp heap\n");
#endif

	if (mem->priv_mem_count) {
		pmem = kcalloc(mem->priv_mem_count, sizeof(*pmem), GFP_KERNEL);
		if (!pmem) {
			ret = -ENOMEM;
			dsp_err("Failed to alloc priv_mem\n");
			goto p_err;
		}
		mem->priv_mem = pmem;
	}

	if (mem->reserved_mem_count) {
		rmem = kcalloc(mem->reserved_mem_count, sizeof(*rmem),
				GFP_KERNEL);
		if (!rmem) {
			ret = -ENOMEM;
			dsp_err("Failed to alloc rmem\n");
			goto p_err_rmem;
		}
		mem->reserved_mem = rmem;

		ret = __dsp_hw_common_memory_parse_dt(mem);
		if (ret)
			goto p_err_parse;
	}

	dsp_leave();
	return 0;
p_err_parse:
	kfree(mem->reserved_mem);
p_err_rmem:
	kfree(mem->priv_mem);
p_err:
	return ret;
}

void dsp_hw_common_memory_remove(struct dsp_memory *mem)
{
	dsp_enter();
	if (mem->reserved_mem_count)
		kfree(mem->reserved_mem);

	if (mem->priv_mem_count)
		kfree(mem->priv_mem);

	dsp_leave();
}

int dsp_hw_common_memory_set_ops(struct dsp_memory *mem, const void *ops)
{
	dsp_enter();
	mem->ops = ops;
	dsp_leave();
	return 0;
}
