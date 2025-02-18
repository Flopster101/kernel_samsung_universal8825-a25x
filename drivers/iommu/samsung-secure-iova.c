// SPDX-License-Identifier: GPL-2.0
/*
 * Secure IOVA Management
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/dma-heap.h>
#include <linux/genalloc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#define SECURE_DMA_BASE	0x40000000
/*
 * MFC have H/W restriction that could only access 0xC000_0000
 * offset from base, so secure device virtual address manager
 * uses the size as 0xC000_0000.
 */
#define SECURE_DMA_SIZE 0x40000000

static struct gen_pool *secure_iova_pool;

/*
 * Alignment to a secure address larger than 16MiB is not beneficial because
 * the protection alignment just needs 64KiB by the buffer protection H/W and
 * the largest granule of H/W security firewall (the secure context of SysMMU)
 * is 16MiB.
 */
#define MAX_SECURE_VA_ALIGN	(SZ_16M / PAGE_SIZE)

unsigned long secure_iova_alloc(unsigned long size, unsigned int align)
{
	unsigned long out_addr;
	struct genpool_data_align alignment = {
		.align = max_t(int, PFN_DOWN(align), MAX_SECURE_VA_ALIGN),
	};

	if (WARN_ON_ONCE(!secure_iova_pool))
		return 0;

	out_addr = gen_pool_alloc_algo(secure_iova_pool, size,
				       gen_pool_first_fit_align, &alignment);

	if (out_addr == 0)
		pr_err("failed alloc secure iova. %zu/%zu bytes used",
		       gen_pool_avail(secure_iova_pool),
		       gen_pool_size(secure_iova_pool));

	return out_addr;
}
EXPORT_SYMBOL_GPL(secure_iova_alloc);

void secure_iova_free(unsigned long addr, unsigned long size)
{
	gen_pool_free(secure_iova_pool, addr, size);
}
EXPORT_SYMBOL_GPL(secure_iova_free);

static int __init samsung_secure_iova_init(void)
{
	struct device_node *np;
	phys_addr_t base = SECURE_DMA_BASE, size = SECURE_DMA_SIZE;
	int ret;

	secure_iova_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!secure_iova_pool) {
		pr_err("failed to create Secure IOVA pool\n");
		return -ENOMEM;
	}

	for_each_node_by_name(np, "secure-iova-domain") {
		struct dma_heap *hpa_heap;
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
			base = (phys_addr_t)of_read_number(prop, naddr);
			prop += naddr;
			size = (phys_addr_t)of_read_number(prop, nsize);
		} else {
			pr_err("%s: failed to get domain-ranges attributes\n", __func__);
			break;
		}

		hpa_heap = dma_heap_find("system-secure-gpu_buffer-secure");
		if (hpa_heap) {
			dma_heap_put(hpa_heap);

			prop = of_get_property(np, "hpa,reserved", &len);
			if (prop && len > 0) {
				size -= (phys_addr_t)of_read_number(prop, nsize);
			} else {
				base = SECURE_DMA_BASE;
				size = SECURE_DMA_SIZE;
				pr_err("%s: failed to get hpa,reserved attributes\n", __func__);
				break;
			}
		}
	}

	ret = gen_pool_add(secure_iova_pool, base, size, -1);
	if (ret) {
		pr_err("failed to set address range of Secure IOVA pool\n");
		gen_pool_destroy(secure_iova_pool);
		return ret;
	}
	pr_info("Add secure iova ranges %#lx-%#lx\n", base, size);

	return 0;
}

static void __exit samsung_secure_iova_exit(void)
{
	gen_pool_destroy(secure_iova_pool);
}

module_init(samsung_secure_iova_init);
module_exit(samsung_secure_iova_exit);
MODULE_DESCRIPTION("Samsung Secure IOVA Manager");
MODULE_LICENSE("GPL v2");
