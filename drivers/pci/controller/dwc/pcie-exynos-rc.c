/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 * Supporting PCIe Gen4
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyounghye Yun <k-hye.yun@samsung.com>
 *         Seungo Jang <seungo.jang@samsung.com>
 *         Kwangho Kim <kwangho2.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/pm_qos.h>
#include <dt-bindings/pci/pci.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_fdt.h>
#include <linux/dma-map-ops.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/random.h>
#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
#include <soc/samsung/exynos-itmon.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
//#if IS_ENABLED(CONFIG_CPU_IDLE)
//#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos_pm_qos.h>
#endif
#include "pcie-designware.h"
#include "pcie-exynos-common.h"
#include "pcie-exynos-rc.h"
#include "pcie-exynos-dbg.h"
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
#include <soc/samsung/shm_ipc.h>
//#define MODIFY_MSI_ADDR
#endif	/* CONFIG_LINK_DEVICE_PCIE */
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>

static spinlock_t s2mpu_lock;
#endif
#include <soc/samsung/exynos-pcie-iommu-exp.h>

static struct exynos_pcie g_pcie_rc[MAX_RC_NUM];

static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp);
int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size);
void exynos_pcie_rc_register_dump(int ch_num);
static int exynos_pcie_rc_link_up(struct dw_pcie *pci);
static int exynos_pcie_set_l1ss_wifi(int enable, struct pcie_port *pp, int id);
void exynos_pcie_config_l1ss(struct pcie_port *pp);
static int exynos_pcie_rc_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val);
static int exynos_pcie_rc_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val);
static int exynos_pcie_rc_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val);
static int exynos_pcie_rc_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val);
static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				const struct cpumask *mask, bool force);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
static struct exynos_pm_qos_request exynos_pcie_int_qos[MAX_RC_NUM];
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
#define to_pci_dev_from_dev(dev) container_of((dev), struct pci_dev, dev)
extern inline void set_dma_ops(struct device *dev, const struct dma_map_ops *dma_ops);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
dma_addr_t dma_pool_start;
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
static char refer_table[1048580] = {0};//0~524299, 524300~8388700

uint64_t exynos_pcie_set_dev_stage2_ap(const char *s2mpu_name, uint64_t base,
		uint64_t size, enum stage2_ap ap)
{
	uint64_t start_idx, num, i;
	uint64_t ret = 0;
	unsigned long flags;

	/* Make sure start address align least 32KB */
	if ((base & (SZ_32K - 1)) != 0) {
		size += base & (SZ_32K - 1);
		base &= ~(SZ_32K - 1);
	}

	/* Make sure size align least 32KB */
	size = (size + SZ_32K - 1) & ~(SZ_32K - 1);

	if (base < 0x100000000 && base >= 0x80000000) {
		start_idx = (base - 0x80000000) / SZ_32K;
		/* maybe, need to add condition (base + size >= 0x100000000) */
	} else if (base < 0x1000000000 && base >= 0x880000000) {
		start_idx = ((base - 0x880000000) / SZ_32K) + 65540;
	} else {
		pr_err("NO DRAM Zone!(WiFi code)\n");
		return 1;
	}

	/* num  */
	num = size / SZ_32K;//num = size / SZ_32K;

	spin_lock_irqsave(&s2mpu_lock, flags);

	/* table set */
	for (i = 0; i < num; i++) {
		(refer_table[start_idx + i])++;

		/*printk("[harx] pcie_set, refer_table[%ld] = %d, dev_addr = 0x%lx\n",
		  start_idx + i, refer_table[start_idx + i], base + (i * 0x1000));*/

		if (refer_table[start_idx + i] == 1) {
			ret = exynos_set_dev_stage2_ap(s2mpu_name,
					base + (i * SZ_32K), SZ_32K,
					ATTR_RW);
			if (ret != 0) {
				pr_err("[pcie_set] set_ap error return: %d\n", ret);
				goto map_finish;
			}
			/* Need to fix "goto map_finish" like "goto retry" for the other loop*/
		}
	}

map_finish:
	spin_unlock_irqrestore(&s2mpu_lock, flags);
	return ret;
}

uint64_t exynos_pcie_unset_dev_stage2_ap(const char *s2mpu_name, uint64_t base,
		uint64_t size, enum stage2_ap ap)
{
	uint64_t start_idx, num, i;
	uint64_t ret = 0;
	unsigned long flags;

	/* Make sure start address align least 32KB */
	if ((base & (SZ_32K - 1)) != 0) {
		size += base & (SZ_32K - 1);
		base &= ~(SZ_32K - 1);
	}

	/* Make sure size align least 32KB */
	size = (size + SZ_32K - 1) & ~(SZ_32K - 1);

	if (base < 0x100000000 && base >= 0x80000000) {
		start_idx = (base - 0x80000000) / SZ_32K;
		/* maybe, need to add condition (base + size >= 0x100000000) */
	} else if (base < 0x1000000000 && base >= 0x880000000) {
		start_idx = ((base - 0x880000000) / SZ_32K) + 65540;
	} else {
		pr_err("NO DRAM Zone!(WiFi code)\n");
		return 1;
	}

	num = size / SZ_32K;//num = size / SZ_32K;

	spin_lock_irqsave(&s2mpu_lock, flags);

	/* reference counter--  */
	for (i = 0; i < num; i++) {
		(refer_table[start_idx + i])--;

		if (refer_table[start_idx + i] < 1) {
			refer_table[start_idx + i] = 0;

			/*printk("[harx] pcie_unset, refer_table[%ld] = %d, dev_addr = 0x%lx\n",
			  start_idx + i, refer_table[start_idx + i], base + (i * 0x1000));*/

			ret = exynos_set_dev_stage2_ap(s2mpu_name,
					base + (i * SZ_32K), SZ_32K,
					ATTR_NO_ACCESS);
			if (ret != 0) {
				pr_err("[pcie_unset] set_ap error return: %d\n", ret);
				goto unmap_finish;
			}
			/* Need to fix "goto map_finish" like "goto retry" for the other loop*/
		}
	}

unmap_finish:
	spin_unlock_irqrestore(&s2mpu_lock, flags);
	return ret;
}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */

static void *exynos_pcie_dma_alloc_attrs(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flags,
		unsigned long attrs)
{
	void *coherent_ptr;
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
	int ch_num = 0;
	int ret = 0;
	struct exynos_pcie *exynos_pcie;
	static int flag_once;

	//dev_info(dev, "[%s] size: 0x%x\n", __func__, (unsigned int)size);
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return NULL;
	} else {
		ch_num = pci_domain_nr(epdev->bus);
	}
	exynos_pcie = &g_pcie_rc[ch_num];
	if (flag_once == 0) {
		memcpy(&exynos_pcie->dup_ep_dev, &exynos_pcie->ep_pci_dev->dev,
				sizeof(exynos_pcie->dup_ep_dev));
		exynos_pcie->dup_ep_dev.dma_ops = NULL;
		flag_once = 1;
	}

	coherent_ptr = dma_alloc_attrs(&exynos_pcie->dup_ep_dev, size, dma_handle, flags, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	if (coherent_ptr != NULL) {
		ret = pcie_iommu_map(*dma_handle, *dma_handle, size,
				DMA_BIDIRECTIONAL, ch_num);
		if (ret != 0) {
			pr_err("DMA alloc - Can't map PCIe SysMMU table!!!\n");
			dma_free_attrs(&exynos_pcie->dup_ep_dev, size, coherent_ptr, *dma_handle, attrs);
			return NULL;
		}
	}
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	if (coherent_ptr != NULL) {
		ret = exynos_pcie_set_dev_stage2_ap("PCIE_GEN2", *dma_handle,
				size, ATTR_RW);
		if (ret) {
			pr_err("[harx] ATTR_RW setting fail\n");
			dma_free_attrs(exynos_pcie->dup_ep_dev, size, coherent_ptr, *dma_handle, attrs);
			return NULL;
		}
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return coherent_ptr;

}

static void exynos_pcie_dma_free_attrs(struct device *dev, size_t size,
		void *vaddr, dma_addr_t dma_handle,
		unsigned long attrs)
{
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	int ch_num = 0;
	struct exynos_pcie *exynos_pcie;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	int ret = 0;
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	exynos_pcie = &g_pcie_rc[ch_num];

	dma_free_attrs(&exynos_pcie->dup_ep_dev, size, vaddr, dma_handle, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	pcie_iommu_unmap(dma_handle, size, ch_num);
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_unset_dev_stage2_ap("PCIE_GEN2", dma_handle,
			size, ATTR_NO_ACCESS);
	if (ret) {
		pr_err("[harx] ATTR_NO_ACCESS setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
}

static dma_addr_t exynos_pcie_dma_map_page_attrs(struct device *dev,
		struct page *page, unsigned long offset,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	dma_addr_t dev_addr;
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	int ch_num = 0;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_IOMMU */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	int ret = 0;
	struct exynos_pcie *exynos_pcie;

	//dev_info(dev, "[%s] size: 0x%x\n", __func__,(unsigned int)size);
	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return -EINVAL;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	exynos_pcie = &g_pcie_rc[ch_num];
	dev_addr = dma_map_page_attrs(&exynos_pcie->dup_ep_dev,
					page, offset, size, dir, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	ret = pcie_iommu_map(dev_addr, dev_addr, size, dir, ch_num);
	if (ret != 0) {
		pr_err("DMA map - Can't map PCIe SysMMU table!!!\n");

		return 0;
	}
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_set_dev_stage2_ap("PCIE_GEN2", dev_addr,
			size, ATTR_RW);
	if (ret) {
		pr_err("[harx] ATTR_RW setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return dev_addr;
}

static void exynos_pcie_dma_unmap_page_attrs(struct device *dev,
		dma_addr_t dev_addr,
		size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC) || IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	int ch_num = 0;
#endif /* CONFIG_PCI_DOMAINS_GENERIC || CONFIG_EXYNOS_PCIE_IOMMU */
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
	struct pci_dev *epdev = to_pci_dev_from_dev(dev);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	int ret = 0;
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	struct exynos_pcie *exynos_pcie;

	if (dev == NULL) {
		pr_err("EP device is NULL!!!\n");
		return;
	} else {
#if IS_ENABLED(CONFIG_PCI_DOMAINS_GENERIC)
		ch_num = pci_domain_nr(epdev->bus);
#endif /* CONFIG_PCI_DOMAINS_GENERIC */
	}

	exynos_pcie = &g_pcie_rc[ch_num];
	dma_unmap_page_attrs(&exynos_pcie->dup_ep_dev, dev_addr, size, dir, attrs);

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
	pcie_iommu_unmap(dev_addr, size, ch_num);
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	ret = exynos_pcie_unset_dev_stage2_ap("PCIE_GEN2", dev_addr,
			size, ATTR_NO_ACCESS);
	if (ret) {
		pr_err("[harx] ATTR_NO_ACCESS setting fail\n");
	}
#endif /* CONFIG_EXYNOS_PCIE_GEN2_S2MPU */
	return;
}

#if 0
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
void get_atomic_pool_info(dma_addr_t *paddr, size_t *size)
{
	*paddr = dma_pool_start;
	*size = gen_pool_size(atomic_pool);
}
EXPORT_SYMBOL(get_atomic_pool_info);
#endif /* CONFIG_EXYNOS_PCIE_IOMMU */
#endif

struct dma_map_ops exynos_pcie_gen2_dma_ops = {
	.alloc = exynos_pcie_dma_alloc_attrs,
	.free = exynos_pcie_dma_free_attrs,
	//.mmap = dma_mmap_attrs,
	//.get_sgtable = dma_get_sgtable_attrs,
	.map_page = exynos_pcie_dma_map_page_attrs,
	.unmap_page = exynos_pcie_dma_unmap_page_attrs,
	//.map_sg = dma_map_sg_attrs,
	//.unmap_sg = dma_unmap_sg_attrs,
	//.map_resource = dma_map_resource,
	//.sync_single_for_cpu = dma_sync_single_for_cpu,
	//.sync_single_for_device = dma_sync_single_for_device,
	//.sync_sg_for_cpu = dma_sync_sg_for_cpu,
	//.sync_sg_for_device = dma_sync_sg_for_device,
	//.get_required_mask = dma_get_required_mask,

};
EXPORT_SYMBOL(exynos_pcie_gen2_dma_ops);
#endif /* CONFIG_EXYNOS_PCIE_IOMMU || CONFIG_EXYNOS_PCIE_GEN2_S2MPU */

/*
 * Reserved memory
 */
#define MAX_PCIE_RMEM	1

struct pcie_reserved_mem {
	char *name;
	unsigned long base;
	u32 size;
};

static struct pcie_reserved_mem _pcie_rmem[MAX_PCIE_RMEM];

static int __init pcie_rmem_setup(struct reserved_mem *rmem)
{
	pr_info("%s: +++\n", __func__);

	_pcie_rmem[0].name = (char *)rmem->name;
	_pcie_rmem[0].base = rmem->base;
	_pcie_rmem[0].size = rmem->size;

	pr_info("%s: [rmem] base = 0x%x, size = 0x%x\n", __func__,
			rmem->base, rmem->size);
	pr_info("%s: [_pcie_rmem] name = %s, base = 0x%llx, size = 0x%x\n",
			__func__, _pcie_rmem[0].name,
			_pcie_rmem[0].base, _pcie_rmem[0].size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifi_msi_rmem, "exynos,pcie_rmem", pcie_rmem_setup);

static int save_pcie_rmem(struct device *dev, int ch_num)
{
	int ret = 0;
	struct device_node *np;
	struct reserved_mem *rmem;
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	np = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!np) {
		dev_err(dev, "%s: there is no memory-region node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		dev_err(dev, "%s: no such rmem node\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	exynos_pcie->rmem_msi_name = (char *)rmem->name;
	exynos_pcie->rmem_msi_base = rmem->base;
	exynos_pcie->rmem_msi_size = rmem->size;

	pr_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);
	dev_info(dev, "%s: success init rmem\n", __func__);
out:

	return 0;
}

static unsigned long get_msi_base(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pr_info("%s: [rmem_msi] name=%s, base=0x%llx, size=0x%x\n", __func__,
			exynos_pcie->rmem_msi_name, exynos_pcie->rmem_msi_base,
			exynos_pcie->rmem_msi_size);

	return exynos_pcie->rmem_msi_base;
}

void exynos_pcie_set_perst(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pr_info("%s: force settig for abnormal state\n", __func__);
	if (on) {
		gpio_set_value(exynos_pcie->perst_gpio, 1);
		pr_info("%s: Set PERST to HIGH, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	} else {
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		pr_info("%s: Set PERST to LOW, gpio val = %d\n",
				__func__, gpio_get_value(exynos_pcie->perst_gpio));
	}
}
EXPORT_SYMBOL(exynos_pcie_set_perst);

void exynos_pcie_set_perst_gpio(int ch_num, bool on)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		pr_info("%s: force settig for abnormal state\n", __func__);
		if (on) {
			gpio_set_value(exynos_pcie->perst_gpio, 1);
			pr_info("%s: Set PERST to HIGH, gpio val = %d\n",
					__func__, gpio_get_value(exynos_pcie->perst_gpio));
		} else {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			pr_info("%s: Set PERST to LOW, gpio val = %d\n",
					__func__, gpio_get_value(exynos_pcie->perst_gpio));
		}
	}
}
EXPORT_SYMBOL(exynos_pcie_set_perst_gpio);

void exynos_pcie_set_ready_cto_recovery(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	pr_info("[%s] +++\n", __func__);

	disable_irq(pp->irq);

	exynos_pcie_set_perst_gpio(ch_num, 0);

	/* LTSSM disable */
	pr_info("[%s] LTSSM disable\n", __func__);
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
			PCIE_APP_LTSSM_ENABLE);
}
EXPORT_SYMBOL(exynos_pcie_set_ready_cto_recovery);

int exynos_pcie_get_irq_num(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	dev_info(pci->dev, "%s: pp->irq = %d\n", __func__, pp->irq);
	return pp->irq;
}
EXPORT_SYMBOL(exynos_pcie_get_irq_num);

static ssize_t exynos_pcie_rc_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	ret += snprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe Test <<<<\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "0 : PCIe Unit Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "1 : Link Test\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "2 : DisLink Test\n");

	return ret;
}
static ssize_t exynos_pcie_rc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int ret = 0;
	u32 val;

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		dev_info(dev, "## PCIe UNIT test START ##\n");
		ret = exynos_pcie_dbg_unit_test(dev, exynos_pcie);
		if (ret) {
			dev_err(dev, "PCIe UNIT test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "## PCIe UNIT test SUCCESS!!##\n");
		break;
	case 1:
		dev_info(dev, "## PCIe establish link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);
		if (ret) {
			dev_err(dev, "PCIe establish link test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "PCIe establish link test success\n");
		break;
	case 2:
		dev_info(dev, "## PCIe dis-link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 0);
		if (ret) {
			dev_err(dev, "PCIe dis-link test failed (%d)\n", ret);
			break;
		}
		dev_err(dev, "PCIe dis-link test success\n");
		break;
	case 3:
		dev_info(dev, "## LTSSM ##\n");
		ret = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0xff;
		dev_info(dev, "PCIE_ELBI_RDLH_LINKUP :0x%x \n", ret);
		break;
	case 5:
		dev_info(dev, " PCIE supports to enter ASPM L1 state test\n");

		if (exynos_pcie->ch_num == 0) { //PCIE GEN2 channel
			exynos_pcie_rc_rd_own_conf(pp, 0x0158, 4, &val);
			val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
			val = val & 0xffffff00;
			exynos_pcie_rc_wr_own_conf(pp, 0x0158, 4, val);
			exynos_pcie_rc_rd_own_conf(pp, 0x0158, 4, &val);
			dev_info(dev, "dbi + 0x158 :0x%08x\n", val);
		}
		else if (exynos_pcie->ch_num == 1) { //PCIE GEN3
			exynos_pcie_rc_rd_own_conf(pp, 0x0168, 4, &val);
			val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
			val = val & 0xffffff00;
			exynos_pcie_rc_wr_own_conf(pp, 0x0168, 4, val);
			exynos_pcie_rc_rd_own_conf(pp, 0x0168, 4, &val);
			dev_info(dev, "dbi + 0x168 :0x%08x\n", val);
		}

		mdelay(50);

		dev_info(dev, "LTSSM: 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
		dev_info(dev, "phy_pcs(+0x188): 0x%08x (L0(0x0), L1.0(0x2), L1.2(0x6), L2(0x3))\n",
				exynos_phy_pcs_read(exynos_pcie, 0x188));
		break;
	case 10:
		dev_info(dev, "L1.2 Disable....\n");
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case 11:
		dev_info(dev, "L1.2 Enable....\n");
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case 12:
		dev_info(dev, "l1ss_ctrl_id_state = 0x%08x\n",
				exynos_pcie->l1ss_ctrl_id_state);
		dev_info(dev, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
				exynos_phy_pcs_read(exynos_pcie, 0x188));
		break;

	case 13:
		dev_info(dev, "%s: force perst setting \n", __func__);
		exynos_pcie_set_perst_gpio(1, 0);
		break;

	case 14:
		/* set input clk path change to disable */
		pr_info("%s: force set input clk path to disable", __func__);
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}
		break;

	case 15:
		pr_info("%s: force all pwndn", __func__);
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
		break;

	case 16:
		exynos_pcie_rc_set_outbound_atu(1, 0x47200000, 0x0, SZ_1M);
		break;

	case 17:
		exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		break;
	}

	return count;
}

static DEVICE_ATTR(pcie_rc_test, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_rc_show, exynos_pcie_rc_store);

static ssize_t exynos_pcie_eom1_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 0, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!! \n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
	}

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static ssize_t exynos_pcie_eom1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		if (exynos_pcie->phy_ops.phy_eom != NULL)
			exynos_pcie->phy_ops.phy_eom(dev,
					exynos_pcie->phy_base);
		break;
	}

	return count;
}

static ssize_t exynos_pcie_eom2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* prevent to print kerenl warning message
	   eom1_store function do all operation to get eom data */

	return count;
}

static ssize_t exynos_pcie_eom2_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 1, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!! \n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
		len += snprintf(buf + len, PAGE_SIZE,
				"can't get num of lanes !! \n");
		goto exit;
	}

	if (lane_width == 1) {
		len += snprintf(buf + len, PAGE_SIZE,
				"EOM2NULL\n");
		goto exit;
	}

	while (current_cnt != EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX) {
		len += snprintf(buf + len, PAGE_SIZE,
				"%u %u %lu\n",
				eom_result[i][current_cnt].phase,
				eom_result[i][current_cnt].vref,
				eom_result[i][current_cnt].err_cnt);
		current_cnt++;
		test_cnt++;
		if (test_cnt == 100)
			break;
	}

	if (current_cnt == EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX)
		current_cnt = 0;

exit:
	return len;
}

static DEVICE_ATTR(eom1, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom1_show, exynos_pcie_eom1_store);

static DEVICE_ATTR(eom2, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom2_show, exynos_pcie_eom2_store);

static inline int create_pcie_sys_file(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	int num_lane;

	ret = device_create_file(dev, &dev_attr_pcie_rc_test);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for test(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = of_property_read_u32(np, "num-lanes", &num_lane);
	if (ret) {
		num_lane = 0;
	}

	ret = device_create_file(dev, &dev_attr_eom1);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
				__func__, ret);
		return ret;
	}

	if (num_lane > 0) {
		ret = device_create_file(dev, &dev_attr_eom2);
		if (ret) {
			dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
					__func__, ret);
			return ret;
		}

	}

	return 0;
}

static inline void remove_pcie_sys_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pcie_rc_test);
}

static int exynos_pcie_rc_clock_enable(struct pcie_port *pp, int enable)
{
#if 0
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;
	int ret = 0;

	if (enable) {
		for (i = 0; i < exynos_pcie->pcie_clk_num; i++)
			ret = clk_prepare_enable(clks->pcie_clks[i]);
	} else {
		for (i = 0; i < exynos_pcie->pcie_clk_num; i++)
			clk_disable_unprepare(clks->pcie_clks[i]);
	}

	return ret;
#endif
	return 0;
}

static int exynos_pcie_rc_phy_clock_enable(struct pcie_port *pp, int enable)
{
#if 0
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;
	int ret = 0;

	if (enable) {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			ret = clk_prepare_enable(clks->phy_clks[i]);
	} else {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			clk_disable_unprepare(clks->phy_clks[i]);
	}

	return ret;
#endif
	return 0;
}

void exynos_pcie_rc_print_msi_register(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, &val);
	dev_info(pci->dev, "PCIE_MSI_INTR0_MASK(0x%x):0x%x \n", PCIE_MSI_INTR0_MASK, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	dev_info(pci->dev, "PCIE_MSI_INTR0_ENABLE(0x%x):0x%x \n", PCIE_MSI_INTR0_ENABLE, val);
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	dev_info(pci->dev, "PCIE_IRQ2_EN: 0x%x \n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_ADDR_LO, 4, &val);
	dev_info(pci->dev, "PCIE_MSI_ADDR_LO: 0x%x \n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_ADDR_HI, 4, &val);
	dev_info(pci->dev, "PCIE_MSI_ADDR_HI: 0x%x \n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS, 4, &val);
	dev_info(pci->dev, "PCIE_MSI_INTR0_STATUS: 0x%x \n", val);
}
EXPORT_SYMBOL(exynos_pcie_rc_print_msi_register);

void exynos_pcie_rc_print_link_history(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 history_buffer[32];
	int i;

	for (i = 31; i >= 0; i--)
		history_buffer[i] = exynos_elbi_read(exynos_pcie,
				PCIE_HISTORY_REG(i));
	for (i = 31; i >= 0; i--)
		dev_info(dev, "LTSSM: 0x%02x, L1sub: 0x%x, D state: 0x%x\n",
				LTSSM_STATE(history_buffer[i]),
				L1SUB_STATE(history_buffer[i]),
				PM_DSTATE(history_buffer[i]));
}
EXPORT_SYMBOL(exynos_pcie_rc_print_link_history);

static int exynos_pcie_rc_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);
	}

	ret = dw_pcie_read(exynos_pcie->rc_dbi_base + (where), size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);

		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_pcie_rc_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;
	int ret = 0;
	u32 __maybe_unused reg_val;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);
		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);
	}

	ret = dw_pcie_write(exynos_pcie->rc_dbi_base + (where), size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static void exynos_pcie_rc_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	/* Program viewport 0 : OUTBOUND : CFG0 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND0, 4, pp->cfg0_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND0, 4,
					(pp->cfg0_base >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND0, 4,
					pp->cfg0_base + pp->cfg0_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND0, 4, busdev);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND0, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, EXYNOS_PCIE_ATU_TYPE_CFG0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, EXYNOS_PCIE_ATU_ENABLE);
	exynos_pcie->atu_ok = 1;
}

#if 0
static void exynos_pcie_rc_prog_viewport_cfg1(struct pcie_port *pp, u32 busdev)
{
	/* Program viewport 1 : OUTBOUND : CFG1 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND0, 4, EXYNOS_PCIE_ATU_TYPE_CFG1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND0, 4, pp->cfg1_base);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND0, 4,
					(pp->cfg1_base >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND0, 4,
					pp->cfg1_base + pp->cfg1_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND0, 4, busdev);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND0, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND0, 4, EXYNOS_PCIE_ATU_ENABLE);
}
#endif

static void exynos_pcie_rc_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	struct resource_entry *tmp = NULL, *entry = NULL;

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;
	if (entry == NULL) {
		pr_err("entry is NULL\n");
		return;
	}

	/* Program viewport 0 : OUTBOUND : MEM */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND1, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND1, 4, entry->res->start);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND1, 4,
					(entry->res->start >> 32));
	/* remove orgin code for btl */
	/* exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
				entry->res->start + pp->mem_size - 1); */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND1, 4,
			entry->res->start + resource_size(entry->res) - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND1, 4, entry->res->start - entry->offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND1, 4,
					upper_32_bits(entry->res->start - entry->offset));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND1, 4, EXYNOS_PCIE_ATU_ENABLE);
}

int exynos_pcie_rc_set_bar(int ch_num, u32 bar_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_dev *ep_pci_dev;
	u32 val;

	pr_info("%s: +++\n", __func__);

	if (exynos_pcie->state == STATE_LINK_UP) {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	} else {
		pr_info("%s: PCIe link is not up\n", __func__);
		return -EPIPE;
	}

	/* EP BAR setup */
	ep_pci_dev->resource[bar_num].start = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset;
	ep_pci_dev->resource[bar_num].end = exynos_pcie->btl_target_addr + \
		exynos_pcie->btl_offset + exynos_pcie->btl_size;
	ep_pci_dev->resource[bar_num].flags = 0x82000000;
	pci_assign_resource(ep_pci_dev, bar_num);

	pci_read_config_dword(ep_pci_dev, PCI_BASE_ADDRESS_0 + (bar_num * 0x4), &val);
	pr_info("%s: Check EP BAR[%d] = 0x%x\n", __func__, bar_num, val);

	pr_info("%s: ---\n", __func__);
	return 0;
}

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 val;
	int ret;
	struct resource_entry *tmp = NULL, *entry = NULL;

	pr_info("%s: +++\n", __func__);

	exynos_pcie->btl_target_addr = target_addr;
	exynos_pcie->btl_offset = offset;
	exynos_pcie->btl_size = size;

	pr_info("%s: target_addr = 0x%x, offset = 0x%x, size = 0x%x\n", __func__,
			exynos_pcie->btl_target_addr,
			exynos_pcie->btl_offset,
			exynos_pcie->btl_size);

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;
	if (entry == NULL) {
		pr_err("entry is NULL\n");
		return -EPIPE;
	}

	/* Only for BTL */
	/* 0x1420_0000 ~ (size -1) */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, EXYNOS_PCIE_ATU_TYPE_MEM);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4,
			entry->res->start + SZ_2M);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4,
			((entry->res->start + SZ_2M) >> 32));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4,
			entry->res->start + SZ_2M + exynos_pcie->btl_size - 1);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4,
			exynos_pcie->btl_target_addr + exynos_pcie->btl_offset);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, 0);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, EXYNOS_PCIE_ATU_ENABLE);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_CR1_OUTBOUND2(0x400) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LOWER_BASE_OUTBOUND2(0x408) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_UPPER_BASE_OUTBOUND2(0x40C) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LIMIT_OUTBOUND2(0x410) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_LOWER_TARGET_OUTBOUND2(0x414) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_UPPER_TARGET_OUTBOUND2(0x418) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, &val);
	pr_info("%s:  PCIE_ATU_CR2_OUTBOUND2(0x404) = 0x%x\n", __func__, val);

	ret = exynos_pcie_rc_set_bar(ch_num, 2);

	pr_info("%s: ---\n", __func__);

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_outbound_atu);

static int exynos_pcie_rc_rd_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 *val)
{
	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));
	//if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
	//} else {
	//	type = EXYNOS_PCIE_ATU_TYPE_CFG1;
	//	cpu_addr = pp->cfg1_base;
	//	cfg_size = pp->cfg1_size;
	//	va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
	//	exynos_pcie_rc_prog_viewport_cfg1(pp, busdev);
	//}
	ret = dw_pcie_read(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	return ret;
}

static int exynos_pcie_rc_wr_other_conf(struct pcie_port *pp,
		struct pci_bus *bus, u32 devfn, int where, int size, u32 val)
{

	int ret, type;
	u32 busdev, cfg_size;
	u64 cpu_addr;
	void __iomem *va_cfg_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int is_linked = 0;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}
	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	//if (bus->parent->number == pp->root_bus_nr) {
		type = EXYNOS_PCIE_ATU_TYPE_CFG0;
		cpu_addr = pp->cfg0_base;
		cfg_size = pp->cfg0_size;
		va_cfg_base = pp->va_cfg0_base;
		/* setup ATU for cfg/mem outbound */
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
	//} else {
	//	type = EXYNOS_PCIE_ATU_TYPE_CFG1;
	//	cpu_addr = pp->cfg1_base;
	//	cfg_size = pp->cfg1_size;
	//	va_cfg_base = pp->va_cfg1_base;
		/* setup ATU for cfg/mem outbound */
	//	exynos_pcie_rc_prog_viewport_cfg1(pp, busdev);
	//}

	ret = dw_pcie_write(va_cfg_base + where, size, val);
	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	return ret;
}

static void __iomem *exynos_pcie_rc_own_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	return dw_pcie_own_conf_map_bus(bus, devfn, where);
}

static int exynos_rc_generic_own_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret, is_linked = 0;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
					exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
					exynos_pcie->ch_num);
	}

	ret = pci_generic_config_read(bus, devfn, where, size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
					exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
					exynos_pcie->ch_num);

		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static int exynos_rc_generic_own_config_write(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret, is_linked = 0;
	unsigned long flags;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&exynos_pcie->reg_lock, flags);

	if (exynos_pcie->state == STATE_LINK_UP ||
			exynos_pcie->state == STATE_LINK_UP_TRY)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
					exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
					exynos_pcie->ch_num);
	}

	ret = pci_generic_config_write(bus, devfn, where, size, val);

	if (is_linked == 0) {
		if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
			exynos_pcie->phy_ops.phy_check_rx_elecidle(
					exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
					exynos_pcie->ch_num);

		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	spin_unlock_irqrestore(&exynos_pcie->reg_lock, flags);

	return ret;
}

static void __iomem *exynos_pcie_rc_other_conf_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	u32 busdev;

	if (!exynos_pcie_rc_link_up(pci))
		return NULL;

	busdev = EXYNOS_PCIE_ATU_BUS(bus->number) | EXYNOS_PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 EXYNOS_PCIE_ATU_FUNC(PCI_FUNC(devfn));

	/* setup ATU for cfg/mem outbound */
	exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);

	return pp->va_cfg0_base + where;
}

static int exynos_rc_generic_other_config_read(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret, is_linked = 0;

	if (pci->dev->power.runtime_status != RPM_ACTIVE) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}

	ret = pci_generic_config_read(bus, devfn, where, size, val);

	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	return ret;
}

static int exynos_rc_generic_other_config_write(struct pci_bus *bus, unsigned int devfn,
		int where, int size, u32 val)
{
	struct pcie_port *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret, is_linked = 0;

	if (pci->dev->power.runtime_status != RPM_ACTIVE)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (exynos_pcie->state == STATE_LINK_UP)
		is_linked = 1;

	if (is_linked == 0) {
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}
	}

	ret = pci_generic_config_write(bus, devfn, where, size, val);

	if (is_linked == 0) {
		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}

		/* TCXO_FAR off */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);
	}

	return ret;
}

static struct pci_ops exynos_rc_own_pcie_ops = {
	.map_bus = exynos_pcie_rc_own_conf_map_bus,
	.read = exynos_rc_generic_own_config_read,
	.write = exynos_rc_generic_own_config_write,
};

static struct pci_ops exynos_rc_child_pcie_ops = {
	.map_bus = exynos_pcie_rc_other_conf_map_bus,
	.read = exynos_rc_generic_other_config_read,
	.write = exynos_rc_generic_other_config_write,
};

u32 exynos_pcie_rc_read_dbi(struct dw_pcie *pci, void __iomem *base,
				u32 reg, size_t size)
{
	struct pcie_port *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, reg, size, &val);

	return val;
}

void exynos_pcie_rc_write_dbi(struct dw_pcie *pci, void __iomem *base,
				  u32 reg, size_t size, u32 val)
{
	struct pcie_port *pp = &pci->pp;

	exynos_pcie_rc_wr_own_conf(pp, reg, size, val);
}

static int exynos_pcie_rc_link_up(struct dw_pcie *pci)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15)
		return 1;

	return 0;
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.read_dbi = exynos_pcie_rc_read_dbi,
	.write_dbi = exynos_pcie_rc_write_dbi,
	.link_up = exynos_pcie_rc_link_up,
};

#if IS_ENABLED(CONFIG_PCI_MSI)
static void exynos_msi_setup_irq(msi_alloc_info_t *arg, int retval)
{
	struct device *dev = arg->desc->dev;
	struct irq_domain *irq_domain = dev->msi_domain->parent;
	struct irq_domain *msi_domain = dev->msi_domain;
	struct pcie_port *pp = irq_domain->host_data;
	struct msi_domain_info *msi_info = msi_domain->host_data;
	u32 val, mask_val;
	u32 *vec_msi_use;
	int  num_ctrls, ctrl;

	if (pp == NULL)
		return;

	vec_msi_use = (u32 *)pp->msi_irq_in_use;
	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
		val = vec_msi_use[ctrl];
		pr_info("msi_irq_in_use : Vector[%d] : 0x%x\n", ctrl, val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, val);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &mask_val);
		mask_val &= ~(val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, mask_val);
		pr_err("%s: MSI Enable 0x%x, MSI Mask 0x%x \n", __func__,
				val, mask_val);
	}

	msi_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;
}

static void exynos_msi_enable(struct pcie_port *pp)
{
	u32 val, mask_val;
	u32 *vec_msi_use;
	int  num_ctrls, ctrl;

	if (pp == NULL)
		return;

	vec_msi_use = (u32 *)pp->msi_irq_in_use;
	num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;

	for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
		val = vec_msi_use[ctrl];
		pr_info("msi_irq_in_use : Vector[%d] : 0x%x\n", ctrl, val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, val);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, &mask_val);
		mask_val &= ~(val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
				(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, mask_val);
		pr_err("%s: MSI Enable 0x%x, MSI Mask 0x%x \n", __func__,
				val, mask_val);
	}
}
#endif

static void exynos_pcie_rc_set_iocc(struct pcie_port *pp, int enable)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int val;

	/* only for 9830 HSI2 block */
	if (exynos_pcie->ip_ver == 0x983000) {
		if (enable) {
			dev_info(pci->dev, "enable cache coherency.\n");

			/* set PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);

			/* set PCIe Shareability */
			val = exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
			val &= ~(PCIE_SYSREG_HSI2_SHARABLE_MASK);
			val |= PCIE_SYSREG_SHARABLE_ENABLE;
			exynos_sysreg_write(exynos_pcie, val, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
		} else {
			dev_info(pci->dev, "disable cache coherency.\n");

			/* clear PCIe Axcache[1] = 1 */
			exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

			/* clear PCIe Shareability */
			val = exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
			val &= ~(PCIE_SYSREG_HSI2_SHARABLE_MASK);
			exynos_sysreg_write(exynos_pcie, val, PCIE_SYSREG_HSI2_SHARABILITY_CTRL);
		}

		exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
		dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);

		dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__,
				exynos_sysreg_read(exynos_pcie, PCIE_SYSREG_HSI2_SHARABILITY_CTRL));

	}
	else if (exynos_pcie->ip_ver == 0x992500) {
		if (exynos_pcie->ch_num == 0) {//PCIe GEN2 channel
			if (enable) {
				dev_dbg(pci->dev, "enable cache coherency.\n");

				/* set PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);
				exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);

				/* set PCIe Shareability */
				val = exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
				val &= ~(PCIE_SYSREG_HSI1_SHARABLE_MASK << 14);
				val |= (PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 14);
				exynos_sysreg_iocc_write(exynos_pcie, val, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
			} else {
				dev_dbg(pci->dev, "disable cache coherency.\n");
				/* clear PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

				/* clear PCIe Shareability */
				val = exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
				val &= ~(PCIE_SYSREG_HSI1_SHARABLE_MASK << 14);
				exynos_sysreg_iocc_write(exynos_pcie, val, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
			}

			exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
			dev_dbg(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
			dev_dbg(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__,
					exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL));
		}
		if (exynos_pcie->ch_num == 1) {//PCIe GEN4 channel
			if (enable) {
				dev_info(pci->dev, "enable cache coherency.\n");

				/* set PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x10101010);
				exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);

				/* set PCIe Shareability */
				val = exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
				val &= ~(PCIE_SYSREG_HSI1_SHARABLE_MASK << 18);
				val |= (PCIE_SYSREG_HSI1_SHARABLE_ENABLE << 18);
				exynos_sysreg_iocc_write(exynos_pcie, val, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
			} else {
				dev_info(pci->dev, "disable cache coherency.\n");
				/* clear PCIe Axcache[1] = 1 */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, 0x0);

				/* clear PCIe Shareability */
				val = exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
				val &= ~(PCIE_SYSREG_HSI1_SHARABLE_MASK << 18);
				exynos_sysreg_iocc_write(exynos_pcie, val, PCIE_SYSREG_HSI1_SHARABILITY_CTRL);
			}

			exynos_pcie_rc_rd_own_conf(pp, PCIE_COHERENCY_CONTROL_3_OFF, 4, &val);
			dev_info(pci->dev, "%s: PCIe Axcache[1] = 0x%x\n", __func__, val);
			dev_info(pci->dev, "%s: PCIe Shareability = 0x%x.\n", __func__,
					exynos_sysreg_iocc_read(exynos_pcie, PCIE_SYSREG_HSI1_SHARABILITY_CTRL));
		}
	} else {
		dev_dbg(pci->dev, "%s: not supported!!!\n", __func__);
	}
}

static int exynos_pcie_rc_parse_dt(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	struct device_node *np = dev->of_node;
	const char *use_cache_coherency;
	const char *use_msi;
	const char *use_sicd;
	const char *use_sysmmu;
	const char *use_ia;
	const char *use_nclkoff_en;
	const char *use_pcieon_sleep;

	if (of_property_read_u32(np, "ip-ver",
					&exynos_pcie->ip_ver)) {
		dev_err(dev, "Failed to parse the number of ip-ver\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "ep-ver",
					&exynos_pcie->ep_ver)) {
		dev_err(dev, "Failed to parse the number of ep-ver\n");
		//return -EINVAL;
	}

#if 0
	if (of_property_read_u32(np, "pcie-clk-num",
					&exynos_pcie->pcie_clk_num)) {
		dev_err(dev, "Failed to parse the number of pcie clock\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "phy-clk-num",
					&exynos_pcie->phy_clk_num)) {
		dev_err(dev, "Failed to parse the number of phy clock\n");
		return -EINVAL;
	}
#endif

	if (of_property_read_u32(np, "pmu-offset",
				&exynos_pcie->pmu_offset)) {
		dev_err(dev, "Failed to parse the number of pmu-offset\n");
	}

	if (of_property_read_u32(np, "pmu-offset1",
				&exynos_pcie->pmu_offset1)) {
		dev_err(dev, "Failed to parse the number of pmu-offset1\n");
	}

	if (of_property_read_u32(np, "pmu-offset2",
				&exynos_pcie->pmu_offset2)) {
		dev_err(dev, "Failed to parse the number of pmu-offset2\n");
	}

	if (of_property_read_u32(np, "ep-device-type",
				&exynos_pcie->ep_device_type)) {
		dev_err(dev, "EP device type is NOT defined, device type is 'EP_NO_DEVICE(0)'\n");
		exynos_pcie->ep_device_type = EP_NO_DEVICE;
	}

	if (of_property_read_u32(np, "max-link-speed",
				&exynos_pcie->max_link_speed)) {
		dev_err(dev, "MAX Link Speed is NOT defined...(GEN1)\n");
		/* Default Link Speet is GEN1 */
		exynos_pcie->max_link_speed = LINK_SPEED_GEN1;
	}

	if (of_property_read_u32(np, "chip-ver",
					&exynos_pcie->chip_ver)) {
		dev_err(dev, "Failed to parse the number of chip-ver, default '0'\n");
		exynos_pcie->chip_ver = 0;
	}

	if (!of_property_read_string(np, "use-cache-coherency",
						&use_cache_coherency)) {
		if (!strcmp(use_cache_coherency, "true")) {
			dev_info(dev, "Cache Coherency unit is ENABLED.\n");
			exynos_pcie->use_cache_coherency = true;
		} else if (!strcmp(use_cache_coherency, "false")) {
			exynos_pcie->use_cache_coherency = false;
		} else {
			dev_err(dev, "Invalid use-cache-coherency value"
					"(Set to default -> false)\n");
			exynos_pcie->use_cache_coherency = false;
		}
	} else {
		exynos_pcie->use_cache_coherency = false;
	}

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
	if (!of_property_read_string(np, "ep-device-name", (const char**)&exynos_pcie->ep_device_name)) {
		dev_info(dev, "EP device name is %s\n", exynos_pcie->ep_device_name);
	} else {
		dev_err(dev, "EP device name is NOT defined, device node name is %s\n", np->name);
		exynos_pcie->ep_device_name = np->name;
	}
#endif

	if (!of_property_read_string(np, "use-msi", &use_msi)) {
		if (!strcmp(use_msi, "true")) {
			exynos_pcie->use_msi = true;
			dev_info(dev, "MSI is ENABLED.\n");
		} else if (!strcmp(use_msi, "false")) {
			dev_info(dev, "## PCIe don't use MSI\n");
			exynos_pcie->use_msi = false;
		} else {
			dev_err(dev, "Invalid use-msi value"
					"(Set to default -> true)\n");
			exynos_pcie->use_msi = true;
		}
	} else {
		exynos_pcie->use_msi = false;
	}

	if (!of_property_read_string(np, "use-sicd", &use_sicd)) {
		if (!strcmp(use_sicd, "true")) {
			dev_info(dev, "## PCIe use SICD\n");
			exynos_pcie->use_sicd = true;
		} else if (!strcmp(use_sicd, "false")) {
			dev_info(dev, "## PCIe don't use SICD\n");
			exynos_pcie->use_sicd = false;
		} else {
			dev_err(dev, "Invalid use-sicd value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sicd = false;
		}
	} else {
		exynos_pcie->use_sicd = false;
	}

	if (!of_property_read_string(np, "use-pcieon-sleep", &use_pcieon_sleep)) {
		if (!strcmp(use_pcieon_sleep, "true")) {
			dev_info(dev, "## PCIe use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = true;
		} else if (!strcmp(use_pcieon_sleep, "false")) {
			dev_info(dev, "## PCIe don't use PCIE ON Sleep\n");
			exynos_pcie->use_pcieon_sleep = false;
		} else {
			dev_err(dev, "Invalid use-pcieon-sleep value"
				       "(set to default -> false)\n");
			exynos_pcie->use_pcieon_sleep = false;
		}
	} else {
		exynos_pcie->use_pcieon_sleep = false;
	}

	if (!of_property_read_string(np, "use-sysmmu", &use_sysmmu)) {
		if (!strcmp(use_sysmmu, "true")) {
			dev_info(dev, "PCIe SysMMU is ENABLED.\n");
			exynos_pcie->use_sysmmu = true;
		} else if (!strcmp(use_sysmmu, "false")) {
			dev_info(dev, "PCIe SysMMU is DISABLED.\n");
			exynos_pcie->use_sysmmu = false;
		} else {
			dev_err(dev, "Invalid use-sysmmu value"
				       "(set to default -> false)\n");
			exynos_pcie->use_sysmmu = false;
		}
	} else {
		exynos_pcie->use_sysmmu = false;
	}

	if (!of_property_read_string(np, "use-ia", &use_ia)) {
		if (!strcmp(use_ia, "true")) {
			dev_info(dev, "PCIe I/A is ENABLED.\n");
			exynos_pcie->use_ia = true;
		} else if (!strcmp(use_ia, "false")) {
			dev_info(dev, "PCIe I/A is DISABLED.\n");
			exynos_pcie->use_ia = false;
		} else {
			dev_err(dev, "Invalid use-ia value"
				       "(set to default -> false)\n");
			exynos_pcie->use_ia = false;
		}
	} else {
		exynos_pcie->use_ia = false;
	}

	if (!of_property_read_string(np, "use-nclkoff-en", &use_nclkoff_en)) {
		if (!strcmp(use_nclkoff_en, "true")) {
			dev_info(dev, "PCIe NCLKOFF is ENABLED.\n");
			exynos_pcie->use_nclkoff_en = true;
		} else if (!strcmp(use_nclkoff_en, "false")) {
			dev_info(dev, "PCIe NCLKOFF is DISABLED.\n");
			exynos_pcie->use_nclkoff_en = false;
		} else {
			dev_err(dev, "Invalid use-nclkoff_en value"
				       "(set to default -> false)\n");
			exynos_pcie->use_nclkoff_en = false;
		}
	} else {
		exynos_pcie->use_nclkoff_en = false;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	if (of_property_read_u32(np, "pcie-pm-qos-int",
					&exynos_pcie->int_min_lock))
		exynos_pcie->int_min_lock = 0;
	if (exynos_pcie->int_min_lock)
		exynos_pm_qos_add_request(&exynos_pcie_int_qos[exynos_pcie->ch_num],
				PM_QOS_DEVICE_THROUGHPUT, 0);
	dev_info(dev, "%s: pcie int_min_lock = %d\n",
			__func__, exynos_pcie->int_min_lock);
#endif

	exynos_pcie->pmureg = syscon_regmap_lookup_by_phandle(np,
					"samsung,syscon-phandle");
	if (IS_ERR(exynos_pcie->pmureg)) {
		dev_err(dev, "syscon regmap lookup failed.\n");
		return PTR_ERR(exynos_pcie->pmureg);
	}

	exynos_pcie->sysreg = syscon_regmap_lookup_by_phandle(np,
			"samsung,sysreg-phandle");
	/* Check definitions to access SYSREG in DT*/
	if (IS_ERR(exynos_pcie->sysreg) && IS_ERR(exynos_pcie->sysreg_base)) {
		dev_err(dev, "SYSREG is not defined.\n");
		return PTR_ERR(exynos_pcie->sysreg);
	}

	/* SSD & WIFI power control */
	exynos_pcie->wlan_gpio = of_get_named_gpio(np, "pcie,wlan-gpio", 0);
	if (exynos_pcie->wlan_gpio < 0) {
		dev_err(dev, "wlan gpio is not defined -> don't use wifi through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->wlan_gpio, 0);
	}

	exynos_pcie->ssd_gpio = of_get_named_gpio(np, "pcie,ssd-gpio", 0);
	if (exynos_pcie->ssd_gpio < 0) {
		dev_err(dev, "ssd gpio is not defined -> don't use ssd through pcie#%d\n",
				exynos_pcie->ch_num);
	} else {
		gpio_direction_output(exynos_pcie->ssd_gpio, 0);
	}

	return 0;
}

static int exynos_pcie_rc_get_pin_state(struct platform_device *pdev,
				struct exynos_pcie *exynos_pcie)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	exynos_pcie->perst_gpio = of_get_gpio(np, 0);
	if (exynos_pcie->perst_gpio < 0) {
		dev_err(&pdev->dev, "cannot get perst_gpio\n");
	} else {
		ret = devm_gpio_request_one(dev, exynos_pcie->perst_gpio,
					    GPIOF_OUT_INIT_LOW, dev_name(dev));
		if (ret)
			return -EINVAL;
	}
	/* Get pin state */
	exynos_pcie->pcie_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(exynos_pcie->pcie_pinctrl)) {
		dev_err(&pdev->dev, "Can't get pcie pinctrl!!!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_ACTIVE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "active");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_ACTIVE])) {
		dev_err(&pdev->dev, "Can't set pcie clkerq to output high!\n");
		return -EINVAL;
	}
	exynos_pcie->pin_state[PCIE_PIN_IDLE] =
		pinctrl_lookup_state(exynos_pcie->pcie_pinctrl, "idle");
	if (IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
		dev_err(&pdev->dev, "No idle pin state(but it's OK)!!\n");
	else
		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_IDLE]);

	return 0;
}

static int exynos_pcie_rc_clock_get(struct pcie_port *pp)
{
#if 0
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i, total_clk_num, phy_count;

	/*
	 * CAUTION - PCIe and phy clock have to define in order.
	 * You must define related PCIe clock first in DT.
	 */
	total_clk_num = exynos_pcie->pcie_clk_num + exynos_pcie->phy_clk_num;

	for (i = 0; i < total_clk_num; i++) {
		if (i < exynos_pcie->pcie_clk_num) {
			clks->pcie_clks[i] = of_clk_get(dev->of_node, i);
			if (IS_ERR(clks->pcie_clks[i])) {
				dev_err(dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		} else {
			phy_count = i - exynos_pcie->pcie_clk_num;
			clks->phy_clks[phy_count] =
				of_clk_get(dev->of_node, i);
			if (IS_ERR(clks->phy_clks[i])) {
				dev_err(dev, "Failed to get pcie clock\n");
				return -ENODEV;
			}
		}
	}
#endif
	return 0;
}

static int exynos_pcie_rc_nclkoff_ctrl(struct platform_device *pdev,
			struct exynos_pcie *exynos_pcie)
{
	struct device *dev = &pdev->dev;
#if 0
	u32 val;
#endif
	dev_dbg(dev, "control NCLK OFF to prevent DBI asseccing when PCIE off \n");
#if 0
	/* TBD: need to check base address & offset of each channel's sysreg */
	val = readl(exynos_pcie->sysreg_base + 0x4);
	dev_info(dev, "orig HSI1_PCIE_GEN4_0_BUS_CTRL: 0x%x\n", val);
	val &= ~PCIE_SUB_CTRL_SLV_EN;
	val &= ~PCIE_SLV_BUS_NCLK_OFF;
	val &= ~PCIE_DBI_BUS_NCLK_OFF;
	writel(val, exynos_pcie->sysreg_base);
	dev_info(dev, "aft HSI1_PCIE_GEN4_0_BUS_CTRL: 0x%x\n", val);
#endif
	return 0;
}

static int exynos_pcie_rc_get_resource(struct platform_device *pdev,
			struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct resource *temp_rsc;
	int ret;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "elbi");
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "soc");
	exynos_pcie->soc_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->soc_base)) {
		ret = PTR_ERR(exynos_pcie->soc_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phyudbg");
	exynos_pcie->phyudbg_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phyudbg_base)) {
		ret = PTR_ERR(exynos_pcie->phyudbg_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysreg_iocc");
	exynos_pcie->sysreg_iocc_base = devm_ioremap_resource(&pdev->dev, temp_rsc);

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysreg");
	exynos_pcie->sysreg_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->sysreg_base)) {
		ret = PTR_ERR(exynos_pcie->sysreg_base);
		return ret;
	}

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->rc_dbi_base)) {
		ret = PTR_ERR(exynos_pcie->rc_dbi_base);
		return ret;
	}

	pci->dbi_base = exynos_pcie->rc_dbi_base;

	temp_rsc = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
	exynos_pcie->phy_pcs_base = devm_ioremap_resource(&pdev->dev, temp_rsc);
	if (IS_ERR(exynos_pcie->phy_pcs_base)) {
		ret = PTR_ERR(exynos_pcie->phy_pcs_base);
		return ret;
	}

	if (exynos_pcie->use_ia) {
		temp_rsc = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "ia");
		exynos_pcie->ia_base =
				devm_ioremap_resource(&pdev->dev, temp_rsc);
		if (IS_ERR(exynos_pcie->ia_base)) {
			ret = PTR_ERR(exynos_pcie->ia_base);
			return ret;
		}
	}

	return 0;
}

static void exynos_pcie_rc_enable_interrupts(struct pcie_port *pp, int enable)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	dev_info(pci->dev, "## %s PCIe INTERRUPT ##\n", enable ? "ENABLE" : "DISABLE");

	if (enable) {
		/* enable INTX interrupt */
		val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
			IRQ_INTC_ASSERT | IRQ_INTD_ASSERT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0_EN);

		/* enable IRQ1 interrupt - only LINKDOWN */
		exynos_elbi_write(exynos_pcie, 0x400, PCIE_IRQ1_EN);
		//exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_IRQ1_EN);

		/* enable IRQ2 interrupt */
		val = IRQ_RADM_CPL_TIMEOUT;
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);
	} else {
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ0_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ1_EN);
		exynos_elbi_write(exynos_pcie, 0, PCIE_IRQ2_EN);
	}

}

static void __maybe_unused exynos_pcie_notify_callback(struct pcie_port *pp,
		int event)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 id;

	if (event == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (event == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		id = 1;
	} else {
		pr_err("PCIe: unknown event!!!\n");
		goto exit;
	}

	pr_err("[%s] event = 0x%x, id = %d\n", __func__, event, id);

	if (exynos_pcie->rc_event_reg[id] && exynos_pcie->rc_event_reg[id]->callback &&
			(exynos_pcie->rc_event_reg[id]->events & event)) {
		struct exynos_pcie_notify *notify =
			&exynos_pcie->rc_event_reg[id]->notify;
		notify->event = event;
		notify->user = exynos_pcie->rc_event_reg[id]->user;
		dev_info(pci->dev, "Callback for the event : %d\n", event);
		exynos_pcie->rc_event_reg[id]->callback(notify);
		return;
	} else {
		dev_info(pci->dev, "Client driver does not have registration "
				"of the event : %d\n", event);
		goto exit;
	}

exit:
	dev_info(pci->dev, "Force PCIe poweroff --> poweron\n");
	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
	exynos_pcie_rc_poweron(exynos_pcie->ch_num);
}

static void __maybe_unused exynos_pcie_notify_callback_wifi(struct pcie_port *pp,
		int event)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);

	if (exynos_pcie->event_reg && exynos_pcie->event_reg->callback &&
			(exynos_pcie->event_reg->events & event)) {
		struct exynos_pcie_notify *notify = &exynos_pcie->event_reg->notify;

		notify->event = event;
		notify->user = exynos_pcie->event_reg->user;

		dev_info(pci->dev, "Callback for the event : %d\n", event);
		exynos_pcie->event_reg->callback(notify);
	} else {
		dev_info(pci->dev, "Client driver does not have registration "
				"of the event : %d\n", event);

		dev_info(pci->dev, "Force PCIe poweroff --> poweron\n");
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
		exynos_pcie_rc_poweron(exynos_pcie->ch_num);
	}
}

void exynos_pcie_rc_register_dump(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	u32 i, val_0, val_4, val_8, val_c;

	pr_err("%s: +++\n", __func__);
	/* ---------------------- */
	/* Link Reg : 0x0 ~ 0x47C */
	/* ---------------------- */
	pr_err("[Print SUB_CTRL region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x1000; i += 0x10) {
		pr_err("ELBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_elbi_read(exynos_pcie, i + 0x0),
				exynos_elbi_read(exynos_pcie, i + 0x4),
				exynos_elbi_read(exynos_pcie, i + 0x8),
				exynos_elbi_read(exynos_pcie, i + 0xC));
	}
	pr_err("\n");

	/* ---------------------- */
	/* PHY Reg : 0x0 ~ 0x19C */
	/* ---------------------- */
	pr_err("[Print PHY region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x2000; i += 0x10) {
		pr_err("PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}

	/* ---------------------- */
	/* PHY PCS : 0x0 ~ 0x19C */
	/* ---------------------- */
	pr_err("[Print PHY_PCS region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x200; i += 0x10) {
		pr_err("PCS 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_pcs_read(exynos_pcie, i + 0x0),
				exynos_phy_pcs_read(exynos_pcie, i + 0x4),
				exynos_phy_pcs_read(exynos_pcie, i + 0x8),
				exynos_phy_pcs_read(exynos_pcie, i + 0xC));
	}
	pr_err("\n");
	pr_err("PCS(0x15C) 0x%08x\n", exynos_phy_pcs_read(exynos_pcie, 0x15C));
	pr_err("PCS(0x17C) 0x%08x\n", exynos_phy_pcs_read(exynos_pcie, 0x17C));

	pr_err("[Print PHY_UDBG region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x1000; i += 0x10) {
		pr_err("PHYUDBG(+0xC000) 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x0),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x4),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0x8),
				exynos_phyudbg_read(exynos_pcie, i + 0xC000 + 0xC));
	}
	pr_err("\n");
	pr_err("PCS(0x17C) 0x%08x\n", exynos_phy_pcs_read(exynos_pcie, 0x17C));

	/* ---------------------- */
	/* DBI : 0x0 ~ 0x8FC */
	/* ---------------------- */
	pr_err("[Print DBI region]\n");
	pr_err("offset:             0x0               0x4               0x8               0xC\n");
	for (i = 0; i < 0x900; i += 0x10) {
		exynos_pcie_rc_rd_own_conf(pp, i + 0x0, 4, &val_0);
		exynos_pcie_rc_rd_own_conf(pp, i + 0x4, 4, &val_4);
		exynos_pcie_rc_rd_own_conf(pp, i + 0x8, 4, &val_8);
		exynos_pcie_rc_rd_own_conf(pp, i + 0xC, 4, &val_c);
		pr_err("DBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i, val_0, val_4, val_8, val_c);
	}
	pr_err("\n");
	pr_err("%s: ---\n", __func__);
}
EXPORT_SYMBOL(exynos_pcie_rc_register_dump);

void exynos_pcie_rc_dump_link_down_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(pci->dev, "LTSSM: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
	dev_info(pci->dev, "LTSSM_H: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_CXPL_DEBUG_INFO_H));
	dev_info(pci->dev, "DMA_MONITOR1: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR1));
	dev_info(pci->dev, "DMA_MONITOR2: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR2));
	dev_info(pci->dev, "DMA_MONITOR3: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_DMA_MONITOR3));
	dev_info(pci->dev, "PCIE link state is %d\n",
			exynos_pcie->state);
}

void exynos_pcie_rc_dislink_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, dislink_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	exynos_pcie_rc_print_link_history(pp);
	exynos_pcie_rc_dump_link_down_status(exynos_pcie->ch_num);
	exynos_pcie_rc_register_dump(exynos_pcie->ch_num);

#if 0
	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(dev, "%s, pcie_is_linkup 0\n", __func__);
		exynos_pcie->pcie_is_linkup = 0;
	}
#endif

	if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		exynos_pcie->linkdown_cnt++;
		dev_info(dev, "link down and recovery cnt: %d\n",
				exynos_pcie->linkdown_cnt);

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		panic("[PCIe Case#4] PCIe Link down occurred! %s\n", exynos_pcie->ep_device_name);
#endif

		exynos_pcie_notify_callback_wifi(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
	}
	else if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;
	}
	else if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		exynos_pcie->linkdown_cnt++;
		dev_info(dev, "link down and recovery cnt: %d\n",
				exynos_pcie->linkdown_cnt);
		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
	} else if (exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		exynos_pcie->linkdown_cnt++;
		dev_info(dev, "link down and recovery cnt: %d\n",
				exynos_pcie->linkdown_cnt);

		exynos_pcie_notify_callback_wifi(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
	}

}

void exynos_pcie_rc_cpl_timeout_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, cpl_timeout_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;

	dev_err(dev, "[%s] +++ \n", __func__);

	dev_info(dev, "[%s] call PCIE_CPL_TIMEOUT callback func.\n", __func__);
	exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_CPL_TIMEOUT);
}

void exynos_pcie_work_l1ss(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
		container_of(work, struct exynos_pcie, l1ss_boot_delay_work.work);
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;

	if (exynos_pcie->work_l1ss_cnt == 0) {
		dev_info(dev, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		exynos_pcie_set_l1ss_wifi(1, pp, PCIE_L1SS_CTRL_BOOT);
		exynos_pcie->work_l1ss_cnt++;
	} else {
		dev_info(dev, "[%s]boot_cnt: %d, work_l1ss_cnt: %d\n",
				__func__, exynos_pcie->boot_cnt,
				exynos_pcie->work_l1ss_cnt);
		return;
	}
}

static void exynos_pcie_rc_use_ia(struct exynos_pcie *exynos_pcie)
{
	if (exynos_pcie->use_ia) {
		if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
			pr_info("[%s] Set I/A for refclk common voltage off ver.9\n", __func__);

			//Release NOTE: Pamir_PCIE_GEN2_ERIO_control_IA_code_ver. 9
			//(ver.6) Release NOTE: Use PMA register PMA, offset:0x 1814, bit[2] ?˜ln0_mon_tx_drv_cm_keeper_en?? which is kept high only at L1.1 & L1.2
			//(ver.6) Release NOTE: Minimize the number of polling keeper_en at L1 exit by using l1sub_exit interrupt
			//(ver.7) Move code which releases PMA ERIO overriding control from ?˜after l1 exit?? to ?˜before l1 exit??
			//======================================================================
			// Please insert below PCIE_IA code before LTSSM_EN is set.
			//======================================================================

			/* PCIE_IA_IRQ Selection */
			// IRQ_EN for PCIE_IA
			exynos_elbi_write(exynos_pcie, 0x800, 0x388); //bit[11]: irq_TOE_en_pm_linkst_in_l1sub_1 (L1.2 enter interrupt)

			//BASE_ADDR setting
			exynos_ia_write(exynos_pcie, 0x113B0000, 0x30); //BASE_ADDR_0: SUB_CTRL_GEN2 (0x113B0000)
			exynos_ia_write(exynos_pcie, 0x113E0000, 0x34); // BASE_ADDR_1: PMA_GEN2 (0x113E0000)
			exynos_ia_write(exynos_pcie, 0x113D0000, 0x38); // BASE_ADDR_2: PCS_GEN2 (0x113D0000)
			exynos_ia_write(exynos_pcie, 0x11390000, 0x3C); // BASE_ADDR_3: PCIE_IA (0x11390000)

			/* IA_CNT_TRG repeat: max. 0xffff) */
			exynos_ia_write(exynos_pcie, 0xffff, 0x10); //target IA cnt: set this larger than the repetition number

			/* LOOP setting*/
			//This LOOP counter and LOOP time are used at BRT & BRF command
			exynos_ia_write(exynos_pcie, 0x00C8012C, 0x40); //loop_cnt:200(0xC8), loop_interval_time: 300(0x12C)*3.3ns (1us), Total Max. Time: 200us

			/* PCIE PCS setting for this W/A */
			exynos_ia_write(exynos_pcie, 0x640A5, 0x158); // pll_en enable @P1.1
			exynos_ia_write(exynos_pcie, 0x640A5, 0x15C); // pll_en enable @P1.2
			exynos_ia_write(exynos_pcie, 0x700D5, 0x154); //pll_en enable @P1.CPM

			//======================================================================

			/* (TRIGGERED by the EVENT: L1sub entry interrupt (pm_linkst_in_l1_1) happens */

			//Sequencer START!

			//======================================================================
			/* Sequence code for L1 substate ENTRY */

			/* SQ0) DATA MASK: Mask ln0_mon_tx_drv_cm_keeper_en bit*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x100);
			exynos_ia_write(exynos_pcie, 0x4, 0x104); //check bit[2]

			/* SQ1) LOOP: Check ln0_mon_tx_drv_cm_keeper_en for (max.) 200us*/
			exynos_ia_write(exynos_pcie, 0x10011418, 0x108); //If ln0_mon_tx_drv_cm_keeper_en (BASE:BA_1(PMA), 0x1418)=1 is TRUE, go to next. Else, stay at current SQ
			exynos_ia_write(exynos_pcie, 0x4, 0x10C); //check value is 0x4

			/* SQ2) WRITE: PMA register*/
			exynos_ia_write(exynos_pcie, 0x40010580, 0x110); //Write to BA_1(PMA),  0x580
			exynos_ia_write(exynos_pcie, 0xA, 0x114); //Write  0xA, ana_aux_ec_en_aux override 0x0

			/* SQ3) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x40020158, 0x118); //Write to BA_2(PCS),  0x158
			exynos_ia_write(exynos_pcie, 0x640AA, 0x11C); //Write  0x640AA, pll, bias off

			/* SQ4) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x120); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640AA, 0x124); //Write  0x640AA, pll, bias off

			/* SQ5) WRITE: PMA register to control ERIO by PCS early.*/
			exynos_ia_write(exynos_pcie, 0x40010580, 0x128); //Write to BA_1(PMA),  0x580
			exynos_ia_write(exynos_pcie, 0x2, 0x12C); //Write  0x2, ana_aux_ec_en_aux release

			/* Sequence code for L1 substate EXIT */
			/* SQ6) DATA MASK: Mask pm_state bit[2]*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x130);
			exynos_ia_write(exynos_pcie, 0x4, 0x134); //check bit[2]

			/* SQ7) BRT: Check pm_state[2] == 1(L1.N) */
			exynos_ia_write(exynos_pcie, 0x20620188, 0x138); // If false (BA_2(PCS), 0x188), go to previous(SQ6), Else, go to next
			exynos_ia_write(exynos_pcie, 0x4, 0x13C); //check (ln0_mon_tx_drv_cm_keeper_en == 0).

			/* SQ8) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x40020158, 0x140); //Write to BA_2(PCS),  0x158
			exynos_ia_write(exynos_pcie, 0x640A5, 0x144); //Write 0x640A5, pll, bias on@P1.1

			/* SQ9) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x148); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x14C); //Write  0x640A5, pll, bias on@P1.2

			//*W/A setting#2 to turn on PHY PLL at L1.1 & L1.2: END!

			/* SQ10) Clear L1 entry &  L1 exit pending register */
			exynos_ia_write(exynos_pcie, 0x40000008, 0x150); //Write to BA_0, offset: 0x8
			exynos_ia_write(exynos_pcie, 0x800, 0x154); //Write ??1?? to clear bit[11](L1 entry)

			/* SQ11) EOP: Return to idle */
			exynos_ia_write(exynos_pcie, 0x80000000, 0x158);
			exynos_ia_write(exynos_pcie, 0x0, 0x15C);

			//Sequencer END!

			/* PCIE_IA Enable */
			exynos_ia_write(exynos_pcie, 0x1, 0x0);
		} else if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
			pr_info("[%s] Set I/A for refclk common voltage off ver.13\n", __func__);

			//Release NOTE: Pamir_PCIE_GEN2_ERIO_control_IA_code_ver. 9
			//(ver.6) Use PMA register PMA, offset:0x 1814, bit[2] ¿ln0_mon_tx_drv_cm_keeper_en¿ which is kept high only at L1.1 & L1.2
			//(ver.6) Minimize the number of polling keeper_en at L1 exit by using l1sub_exit interrupt
			//(ver.7) Move code which releases PMA ERIO overriding control from ¿after l1 exit¿ to ¿before l1 exit¿
			//(ver.8) Change condition from ¿(cm_keeper_en==0)¿ to ¿i_mon_pll_lock¿ at L1substate exit.
			//(ver.9) Change condition from ¿(cm_keeper_en==0)¿ to ¿pcs_pm_state[2]==1(which means L1.N) ¿ at L1substate exit. Change scheme to start polling exit condition (IRQ trigger -> Keep polling)
			//(ver.10) This version is dropped.
			//(ver.11) Insert deskew fifo reset when pll_lock is detected in L1ss exit scenario.
			//(ver.12) Edit deskew fifo reset de-assertion timing.


			//======================================================================
			// Please insert below PCIE_IA code before LTSSM_EN is set.
			//======================================================================

			/* PCIE_IA_IRQ Selection */
			// IRQ_EN for PCIE_IA
			exynos_elbi_write(exynos_pcie, 0x800, 0x388); //bit[11]: irq_TOE_en_pm_linkst_in_l1sub_1 (L1.2 enter interrupt)

			//BASE_ADDR setting
			exynos_ia_write(exynos_pcie, 0x113B0000, 0x30); //BASE_ADDR_0: SUB_CTRL_GEN2 (0x113B0000)
			exynos_ia_write(exynos_pcie, 0x113E0000, 0x34); // BASE_ADDR_1: PMA_GEN2 (0x113E0000)
			exynos_ia_write(exynos_pcie, 0x113D0000, 0x38); // BASE_ADDR_2: PCS_GEN2 (0x113D0000)
			exynos_ia_write(exynos_pcie, 0x11390000, 0x3C); // BASE_ADDR_3: PCIE_IA (0x11390000)

			/* IA_CNT_TRG repeat: max. 0xffff) */
			exynos_ia_write(exynos_pcie, 0xffff, 0x10); //target IA cnt: set this larger than the repetition number

			/* LOOP setting*/
			//This LOOP counter and LOOP time are used at BRT & BRF command
			exynos_ia_write(exynos_pcie, 0x00C8012C, 0x40); //loop_cnt:200(0xC8), loop_interval_time: 300(0x12C)*3.3ns (1us), Total Max. Time: 200us

			/* PCIE PCS setting for this W/A */
			exynos_ia_write(exynos_pcie, 0x640A5, 0x158); // pll_en enable @P1.1
			exynos_ia_write(exynos_pcie, 0x640A5, 0x15C); // pll_en enable @P1.2
			exynos_ia_write(exynos_pcie, 0x700D5, 0x154); //pll_en enable @P1.CPM

			//======================================================================
			/* (TRIGGERED by the EVENT: L1sub entry interrupt (pm_linkst_in_l1_1) happens */
			//Sequencer START!
			//======================================================================

			/* Sequence code for L1 substate ENTRY */

			/* SQ0) DATA MASK: Mask pm_state[2:0]*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x100);
			exynos_ia_write(exynos_pcie, 0x7, 0x104); //check bit[2:0]

			/* SQ1) BRF: Check pm_state[2:0] == 0x4(L1.CPM) */
			exynos_ia_write(exynos_pcie, 0x30020188, 0x108);// If false (BA_2(PCS), 0x188), go to previous(SQ0), Else, go to next
			exynos_ia_write(exynos_pcie, 0x4, 0x10C); //check (pm_state[2:0] == 0x4)., pm_state L1.CPM

			/* SQ2) DATA MASK: Mask pm_state[2:0]*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x110);
			exynos_ia_write(exynos_pcie, 0x7, 0x114); //check bit[2:0]

			/* SQ3) BRT: Check pm_state[2:0] == 2(L1.0) */
			exynos_ia_write(exynos_pcie, 0x20C20188, 0x118); // If true (BA_2(PCS), 0x188), go to SQ12, Else, go to next
			exynos_ia_write(exynos_pcie, 0x2, 0x11C); //check (pm_state[2:0] == 0x2)., pm_state L1.0

			/* SQ4)DATA MASK: Mask ln0_mon_tx_drv_cm_keeper_en bit*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x120);
			exynos_ia_write(exynos_pcie, 0x4, 0x124); //check bit[2]

			/* SQ5) BRF: Check ln0_mon_tx_drv_cm_keeper_en */
			exynos_ia_write(exynos_pcie, 0x30211418, 0x128); // If false (BA_1(PMA), 0x1418), go to SQ2, Else, go to next
			exynos_ia_write(exynos_pcie, 0x4, 0x12C); //check (ln0_mon_tx_drv_cm_keeper_en).

			/* SQ6) WRITE: PMA register*/
			exynos_ia_write(exynos_pcie, 0x40010580, 0x130); //Write to BA_1(PMA),  0x580
			exynos_ia_write(exynos_pcie, 0xA, 0x134); //Write  0xA, ana_aux_ec_en_aux override 0x0

			/* SQ7) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x40020158, 0x138); //Write to BA_2(PCS),  0x158
			exynos_ia_write(exynos_pcie, 0x640AA, 0x13C); //Write  0x640AA, pll, bias off

			/* SQ8) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x140); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640AA, 0x144); //Write  0x640AA, pll, bias off

			/* SQ9) WRITE: PMA register to control ERIO by PCS early.*/
			exynos_ia_write(exynos_pcie, 0x40010580, 0x148); //Write to BA_1(PMA),  0x580
			exynos_ia_write(exynos_pcie, 0x2, 0x14C); //Write  0x2, ana_aux_ec_en_aux release

			//======================================================================

			/* Sequence code for L1 substate EXIT */

			/* SQ10) DATA MASK: Mask pm_state bit[2]*/
			exynos_ia_write(exynos_pcie, 0x50000004, 0x150);
			exynos_ia_write(exynos_pcie, 0x4, 0x154); //check bit[2]

			/* SQ11) BRT: Check pm_state[2] == 1(L1.N) */
			exynos_ia_write(exynos_pcie, 0x20A20188, 0x158); // If true (BA_2(PCS), 0x188), go to previous(SQ10), Else, go to next
			exynos_ia_write(exynos_pcie, 0x4, 0x15C); //check (pm_state[2] == 1)., pm_state L1.1=0x5, L1.2=0x6

			/* SQ12) WRITE: PMA register*/
			exynos_ia_write(exynos_pcie, 0x40011350, 0x160); //Write to BA_1(PMA),  0x1350
			exynos_ia_write(exynos_pcie, 0x8, 0x164); //Write 0x8, bit[3] o_ln0_sfr_ov_s_txd_deskew_rstn ==1(select overriding path), bit[2] ln0_ov_i_txd_deskew_rstn==0(select overriding value)

			/* SQ13) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x40020158, 0x168); //Write to BA_2(PCS),  0x158
			exynos_ia_write(exynos_pcie, 0x640A5, 0x16C); //Write 0x640A5, pll, bias on@P1.1

			/* SQ14) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x170); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x174); //Write  0x640A5, pll, bias on@P1.2
			//*W/A setting#2 to turn on PHY PLL at L1.1 & L1.2: END!

			//Dummy write
			/* SQ15) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x178); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x17C); //Write  0x640A5, pll, bias on@P1.2

			//Dummy write
			/* SQ16) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x180); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x184); //Write  0x640A5, pll, bias on@P1.2

			//Dummy write
			/* SQ17) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x188); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x18C); //Write  0x640A5, pll, bias on@P1.2

			//Dummy write
			/* SQ18) WRITE: PCS register*/
			exynos_ia_write(exynos_pcie, 0x4002015C, 0x190); //Write to BA_2(PCS),  0x15C
			exynos_ia_write(exynos_pcie, 0x640A5, 0x194); //Write  0x640A5, pll, bias on@P1.2

			/* SQ19) WRITE: PMA register*/
			exynos_ia_write(exynos_pcie, 0x40011350, 0x198); //Write to BA_1(PMA),  0x1350
			exynos_ia_write(exynos_pcie, 0x0, 0x19C); //Write 0x0, bit[3] o_ln0_sfr_ov_s_txd_deskew_rstn ==0 (select function Path)

			/* SQ20) Clear L1 entry pending register */
			exynos_ia_write(exynos_pcie, 0x40000008, 0x1A0); //Write to BA_0, offset: 0x8
			exynos_ia_write(exynos_pcie, 0x800, 0x1A4); //Write ¿1¿ to clear bit[11](L1 entry)

			/* SQ21) EOP: Return to idle */
			exynos_ia_write(exynos_pcie, 0x80000000, 0x1A8);
			exynos_ia_write(exynos_pcie, 0x0, 0x1AC);
			//======================================================================


			//Sequencer END!
			//======================================================================

			/* PCIE_IA Enable */
			exynos_ia_write(exynos_pcie, 0x1, 0x0);
		}
	} else {
		dev_info(exynos_pcie->pci->dev , "[%s] Not support I/A!!!\n", __func__);
	}
}

static void exynos_pcie_rc_assert_phy_reset(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	int ret;

	ret = exynos_pcie_rc_phy_clock_enable(pp, PCIE_ENABLE_CLOCK);
	dev_dbg(dev, "phy clk enable, ret value = %d\n", ret);

	if (exynos_pcie->phy_ops.phy_config != NULL)
		exynos_pcie->phy_ops.phy_config(exynos_pcie, exynos_pcie->ch_num);

	/* Added for CDR Lock */
	exynos_pcie_rc_use_ia(exynos_pcie);
}

static void exynos_pcie_rc_resumed_phydown(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	int ret;

	/* phy all power down during suspend/resume */
	ret = exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
	dev_err(dev, "pcie clk enable, ret value = %d\n", ret);

	if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
			(exynos_pcie->ep_device_type == EP_QC_WIFI)) {
		exynos_pcie_rc_enable_interrupts(pp, 1);
	} else if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM ||
			(exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI)) {
		exynos_pcie_rc_enable_interrupts(pp, 0);
	}

	dev_info(dev, "[%s] ## PCIe PMU regmap update 1 : BYPASS ##\n", __func__);
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);

	exynos_pcie_rc_assert_phy_reset(pp);

	/* phy all power down */
	if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
		exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
	}
	exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
	exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);
}

static void exynos_pcie_setup_rc(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 pcie_cap_off = PCIE_CAP_OFFSET;
	u32 pm_cap_off = PM_CAP_OFFSET;
	u32 val;

	/* enable writing to DBI read-only registers */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MISC_CONTROL, 4, DBI_RO_WR_EN);

	if (exynos_pcie->max_link_speed >= 3) {
		/* Disable BAR and Exapansion ROM BAR */
		exynos_pcie_rc_wr_own_conf(pp, 0x100010, 4, 0);
		exynos_pcie_rc_wr_own_conf(pp, 0x100030, 4, 0);
	}

	/* change vendor ID and device ID for PCIe */
	exynos_pcie_rc_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	exynos_pcie_rc_wr_own_conf(pp, PCI_DEVICE_ID, 2,
			    PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set L1 exit latency & show max speed, max link width reset value */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	val &= ~(PCI_EXP_LNKCAP_L1EL);
	val |= PCI_EXP_LNKCAP_L1EL_64USEC;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCAP, 4, &val);
	dev_dbg(pci->dev, "PCIE_CAP_LINK_CAPABILITIES_REG(0x7c: 0x%x)\n", val);

	/* set auxiliary clock frequency: 38MHz */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4,
			0x4c);

	/* set duration of L1.2 & L1.2.Entry */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);

	/* clear power management control and status register */
	exynos_pcie_rc_wr_own_conf(pp, pm_cap_off + PCI_PM_CTRL, 4, 0x0);

	/* set target speed from DT */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= exynos_pcie->max_link_speed;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL2, 4, val);

#if 0
	/* TBD : */
	/* initiate link retraining */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val |= PCI_EXP_LNKCTL_RL;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_LNKCTL, 4, val);
#endif

	/* set completion timeout */
	/*
	Range/ Encoding/ Spec Minimum/ Spec Maximum/ PCIe controller Minimum/ PCIe controller Maximum
		Default 0000b 50 us 50 ms 28 ms 44 ms (M-PCIe: 45)
		A 0001b 50 us 100 us 65 us 99 us (M-PCIe: 100)
		A 0010b 1 ms 10 ms 4.1 ms 6.2 ms (M-PCIe: 6.4)
		B 0101b 16 ms 55 ms 28 ms 44 ms (M-PCIe: 45)
		B 0110b 65 ms 210 ms 86 ms 131 ms (M-PCIe: 133)
		C 1001b 260 ms 900 ms 260 ms 390 ms (M-PCIe: 398)
		C 1010b 1s 3.5 s 1.8 s 2.8 s (M-PCIe: 2.8)
		D 1101b 4s 13 s 5.4 s 8.2 s (M-PCIe: 8.4)
		D 1110b 17s 64 s 38 s 58 s (M-PCIe: 59) */
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	dev_dbg(pci->dev, "%s: before device_ctrl_status(0x98) = 0x%x\n", __func__, val);
	val &= ~(PCIE_CAP_CPL_TIMEOUT_VAL_MASK);
	val |= PCIE_CAP_CPL_TIMEOUT_VAL_6_2MS;
	exynos_pcie_rc_wr_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, pcie_cap_off + PCI_EXP_DEVCTL2, 4, &val);
	dev_dbg(pci->dev, "%s: after device_ctrl_status(0x98) = 0x%x\n", __func__, val);

	if (exynos_pcie->max_link_speed >= 3) {
		/* EQ Off */
		exynos_pcie_rc_wr_own_conf(pp, 0x890, 4, 0x12000);
	}
}

static int exynos_pcie_rc_init(struct pcie_port *pp)
{

	/* Set default bus ops */
	pp->bridge->ops = &exynos_rc_own_pcie_ops;
	pp->bridge->child_ops = &exynos_rc_child_pcie_ops;

	/* Setup RC to avoid initialization faile in PCIe stack */
	dw_pcie_setup_rc(pp);
	return 0;
}

static void exynos_pcie_set_num_vectors(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct device *dev = pci->dev;

	dev_dbg(dev, "Set MSI vector number to : 256\n");
	pp->num_vectors = MAX_MSI_IRQS;
}

static struct dw_pcie_host_ops exynos_pcie_rc_ops = {
	.host_init = exynos_pcie_rc_init,
	.set_num_vectors = exynos_pcie_set_num_vectors,
};

static irqreturn_t exynos_pcie_rc_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;
	u32 val_irq0, val_irq1, val_irq2;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	int ctrl, num_ctrls;

	/* handle IRQ0 interrupt */
	val_irq0 = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
	exynos_elbi_write(exynos_pcie, val_irq0, PCIE_IRQ0);

	/* handle IRQ1 interrupt */
	val_irq1 = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
	exynos_elbi_write(exynos_pcie, val_irq1, PCIE_IRQ1);

	/* handle IRQ2 interrupt */
	val_irq2 = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
	exynos_elbi_write(exynos_pcie, val_irq2, PCIE_IRQ2);

	/* only support after EXYNOS9820 EVT 1.1 */
	if (val_irq1 & IRQ_LINK_DOWN) {
		dev_info(dev, "!!! PCIE LINK DOWN !!!\n");
		dev_info(dev, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

		if (exynos_pcie->cpl_timeout_recovery) {
			dev_info(dev, "!!!now is already cto recovering..\n");
		} else {
			exynos_pcie->sudden_linkdown = 1;
			exynos_pcie->state = STATE_LINK_DOWN_TRY;

			if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
				exynos_pcie->linkdown_cnt++;
				dev_info(pci->dev, "link down and recovery cnt: %d\n",
						exynos_pcie->linkdown_cnt);

				exynos_pcie_notify_callback_wifi(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
			}

			queue_work(exynos_pcie->pcie_wq,
					&exynos_pcie->dislink_work.work);
		}
	}

	if (val_irq2 & IRQ_RADM_CPL_TIMEOUT) {
		dev_info(dev, "!!!PCIE_CPL_TIMEOUT (PCIE_IRQ2: 0x%x)!!!\n", val_irq2);
		dev_info(dev, "!!!irq0 = 0x%x, irq1 = 0x%x, irq2 = 0x%x!!!\n",
				val_irq0, val_irq1, val_irq2);

		if (exynos_pcie->sudden_linkdown) {
			dev_info(dev, "!!!now is already link down recovering..\n");
		} else {
			if (exynos_pcie->cpl_timeout_recovery == 0) {
				exynos_pcie->state = STATE_LINK_DOWN;
				exynos_pcie->cpl_timeout_recovery = 1;
				dev_info(dev, "!!!call cpl_timeout work\n");
				queue_work(exynos_pcie->pcie_wq,
						&exynos_pcie->cpl_timeout_work.work);
			} else {
				dev_info(dev, "!!!now is already cto recovering..\n");
			}
		}
	}

#if IS_ENABLED(CONFIG_PCI_MSI)
	if (val_irq2 & IRQ_MSI_RISING_ASSERT && exynos_pcie->use_msi) {
		dw_handle_msi_irq(pp);

		/* Mask & Clear MSI to pend MSI interrupt.
		 * After clearing IRQ_PULSE, MSI interrupt can be ignored if
		 * lower MSI status bit is set while processing upper bit.
		 * Through the Mask/Unmask, ignored interrupts will be pended.
		 */
		if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
			exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, &val_irq2);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, 0xffffffff);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, val_irq2);
		} else {
			num_ctrls = pp->num_vectors / MAX_MSI_IRQS_PER_CTRL;
			for (ctrl = 0; ctrl < num_ctrls; ctrl++) {
				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
						(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, 0xffffffff);
				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_MASK +
						(ctrl * MSI_REG_CTRL_BLOCK_SIZE), 4, 0x0);
			}
		}
	}
#endif
	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_PCI_MSI)
static void exynos_pcie_rc_msi_init_wifi(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	unsigned long msi_addr_from_dt = 0;

	if (!exynos_pcie->use_msi)
		return;

	if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		msi_addr_from_dt = get_msi_base(exynos_pcie->ch_num);

		pp->msi_data = msi_addr_from_dt;

		dev_dbg(pci->dev, "%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n",
				__func__, msi_addr_from_dt, pp->msi_data);

		/* program the msi_data */
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
				lower_32_bits(pp->msi_data));
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
				upper_32_bits(pp->msi_data));
	} else {
		/*
		 * dw_pcie_msi_init() function allocates msi_data.
		 * The following code is added to avoid duplicated allocation.
		 */

		dw_pcie_msi_init(pp);
	}

	/* enable MSI interrupt */
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	exynos_msi_enable(pp);
}

static int exynos_pcie_rc_msi_init(struct pcie_port *pp)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	unsigned long msi_addr_from_dt;
#endif
	/*
	 * The following code is added to avoid duplicated allocation.
	 */
	if (!exynos_pcie->probe_ok) {
		dev_info(dev, "%s: allocate MSI data \n", __func__);

		if (exynos_pcie->ep_pci_bus == NULL)
			exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

		exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, MSI_CONTROL,
				4, &val);
		dev_info(dev, "%s: EP support %d-bit MSI address (0x%x)\n", __func__,
				(val & MSI_64CAP_MASK) ? 64 : 32, val);

		if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
			/* get the MSI target address from DT */
			msi_addr_from_dt = shm_get_msi_base();

			if (msi_addr_from_dt) {
				dev_info(dev, "%s: MSI target addr. from DT: 0x%lx\n",
						__func__, msi_addr_from_dt);
				pp->msi_data = msi_addr_from_dt;
				goto program_msi_data;
			} else {
				dev_err(dev, "%s: msi_addr_from_dt is null \n", __func__);
				return -EINVAL;
			}
#else
			dev_info(dev, "EP device is Modem but, ModemIF isn't enabled\n");
#endif
		} else {
			dw_pcie_msi_init(pp);

			if ((pp->msi_data >> 32) != 0)
				dev_info(dev, "MSI memory is allocated over 32bit boundary\n");
			dev_info(dev, "%s: msi_data : 0x%llx\n", __func__, pp->msi_data);
		}
	}

#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
program_msi_data:
#endif
	dev_info(dev, "%s: Program the MSI data: %lx (probe ok:%d)\n", __func__,
				(unsigned long int)pp->msi_data, exynos_pcie->probe_ok);
	/* Program the msi_data */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			lower_32_bits(pp->msi_data));
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
			upper_32_bits(pp->msi_data));

	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	val |= IRQ_MSI_CTRL_EN_RISING_EDG;
	val |= IRQ_RADM_CPL_TIMEOUT;
	exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2_EN);

	/* Enable MSI interrupt after PCIe reset */
	val = (u32)(*pp->msi_irq_in_use);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
	dev_info(dev, "MSI INIT check INTR0 ENABLE, 0x%x: 0x%x \n", PCIE_MSI_INTR0_ENABLE, val);
	if (val != 0xf1) {
		exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, 0xf1);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	}
#endif
	dev_info(dev, "%s: MSI INIT END, 0x%x: 0x%x \n", __func__, PCIE_MSI_INTR0_ENABLE, val);

	return 0;
}
#endif

static void exynos_pcie_rc_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct device *dev = pci->dev;
	struct pcie_port *pp = &pci->pp;
	int count = 0;
	int retry_cnt = 0;
	u32 val;

	/* L1.2 enable check */
	dev_dbg(dev, "Current PM state(PCS + 0x188) : 0x%x \n",
			readl(exynos_pcie->phy_pcs_base + 0x188));

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
	dev_dbg(dev, "DBI Link Control Register: 0x%x \n", val);

	val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	dev_dbg(dev, "%s: link state:%x\n", __func__, val);
	if (!(val >= 0x0d && val <= 0x14)) {
		dev_info(dev, "%s, pcie link is not up\n", __func__);
		return;
	}

	exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val &= ~APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

retry_pme_turnoff:
	if (retry_cnt > 0) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		dev_err(dev, "Current LTSSM State is 0x%x with retry_cnt =%d.\n",
				val, retry_cnt);
	}

	exynos_elbi_write(exynos_pcie, 0x1, XMIT_PME_TURNOFF);

	while (count < MAX_L2_TIMEOUT) {
		if ((exynos_elbi_read(exynos_pcie, PCIE_IRQ0)
						& IRQ_RADM_PM_TO_ACK)) {
			dev_dbg(dev, "ack message is ok\n");
			udelay(10);
			break;
		}

		udelay(10);
		count++;
	}
	if (count >= MAX_L2_TIMEOUT)
		dev_err(dev, "cannot receive ack message from EP\n");

	exynos_elbi_write(exynos_pcie, 0x0, XMIT_PME_TURNOFF);

	count = 0;
	do {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			dev_dbg(dev, "received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_L2_TIMEOUT);

	if (count >= MAX_L2_TIMEOUT) {
		if (retry_cnt < 5) {
			retry_cnt++;

			goto retry_pme_turnoff;
		}
		dev_err(dev, "cannot receive L23_READY DLLP packet(0x%x)\n", val);
#ifdef CONFIG_SEC_PANIC_PCIE_ERR
		if (exynos_pcie->ep_device_type != EP_QC_WIFI) {
			exynos_pcie_rc_register_dump(0);			
			panic("[PCIe Case#5] L2/3 READY fail! %s\n", exynos_pcie->ep_device_name);
		}
#endif		
	}
}

static int exynos_pcie_rc_establish_link(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	u32 val, busdev;
	int count = 0, try_cnt = 0;
	u32 speed_recovery_cnt = 0;

retry:
	/* avoid checking rx elecidle when access DBI */
	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, IGNORE_ELECIDLE,
				exynos_pcie->ch_num);

	exynos_pcie_rc_assert_phy_reset(pp);

	/* Q-ch disable */
	exynos_elbi_write(exynos_pcie, 0x0, 0x3A8);
	if (exynos_pcie->ep_device_type != EP_SAMSUNG_MODEM) {
		exynos_elbi_write(exynos_pcie, 0x2, 0xD5C);
	}
	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		exynos_elbi_write(exynos_pcie, 0x0, 0x20);//#12
	}
	//EVT1 only
	if (exynos_pcie->chip_ver == 1) {
		//PMU setting for PHY power gating enable @L1.2
		//"1" = Enable PHY power(CDR) gating
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_offset1,
				PCIE_PHY_CONTROL_MASK1, (0x0 << 5));
		regmap_update_bits(exynos_pcie->pmureg,
				exynos_pcie->pmu_offset1,
				PCIE_PHY_CONTROL_MASK1, (0x1 << 5));
	}

	if (exynos_pcie->ep_device_type == EP_QC_WIFI)
		usleep_range(10000, 12000);

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);

	dev_info(dev, "%s: Set PERST to HIGH, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

	if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
		if (exynos_pcie->ep_ver == 0x04389) {
			dev_dbg(dev, "ep_ver: 0x%x (usleep 20~22ms)\n", exynos_pcie->ep_ver);
			usleep_range(20000, 22000);
		}
		else {
			dev_dbg(dev, "ep_ver: 0x%x (usleep 18~20ms)\n", exynos_pcie->ep_ver);
			usleep_range(18000, 20000);
		}
	}
	else {
		dev_dbg(dev, "default (usleep 18~20ms)\n");
		usleep_range(18000, 20000);
	}

	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val |= APP_REQ_EXIT_L1_MODE;
	val |= L1_REQ_NAK_CONTROL_MASTER;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	exynos_elbi_write(exynos_pcie, PCIE_LINKDOWN_RST_MANUAL,
			  PCIE_LINKDOWN_RST_CTRL_SEL);

	/* NAK enable when AXI pending */
	exynos_elbi_write(exynos_pcie, NACK_ENABLE, PCIE_MSTR_PEND_SEL_NAK);
	dev_dbg(dev, "%s: NACK option enable: 0x%x\n", __func__,
			exynos_elbi_read(exynos_pcie, PCIE_MSTR_PEND_SEL_NAK));

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	exynos_pcie_setup_rc(pp);

	if (exynos_pcie->use_cache_coherency)
		exynos_pcie_rc_set_iocc(pp, 1);

	if (exynos_pcie->phy_ops.phy_check_rx_elecidle != NULL)
		exynos_pcie->phy_ops.phy_check_rx_elecidle(
				exynos_pcie->phy_pcs_base, ENABLE_ELECIDLE,
				exynos_pcie->ch_num);

	dev_info(dev, "D state: %x, %x\n",
			exynos_elbi_read(exynos_pcie, PCIE_PM_DSTATE) & 0x7,
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));

	exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP);
	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
		//usleep_range(18000, 20000);
		usleep_range(48000, 50000);
	}

	/* assert LTSSM enable */
	exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_ENABLE,
				PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		if (val == 0x11)
			break;

		count++;

		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		try_cnt++;

		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		dev_err(dev, "%s: Link is not up, try count: %d, linksts: %s(0x%x)\n",
			__func__, try_cnt, LINK_STATE_DISP(val), val);

		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
				__func__,
				gpio_get_value(exynos_pcie->perst_gpio));

			/* LTSSM disable */
			exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
					  PCIE_APP_LTSSM_ENABLE);

			exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);

			goto retry;
		} else {
			exynos_pcie_rc_print_link_history(pp);
			exynos_pcie_rc_register_dump(0);

#ifdef CONFIG_SEC_PANIC_PCIE_ERR
			panic("[PCIe Case#1] PCIe Link up fail! %s\n", exynos_pcie->ep_device_name);
#endif			
			return -EPIPE;
		}
	} else {
		val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		dev_dbg(dev, "%s: %s(0x%x)\n", __func__,
				LINK_STATE_DISP(val), val);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		dev_dbg(dev, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		speed_recovery_cnt = 0;
		while (speed_recovery_cnt < MAX_TIMEOUT_GEN1_GEN2) {
			exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
			val = (val >> 16) & 0xf;
			if (val == exynos_pcie->max_link_speed) {
				dev_dbg(dev, "%s: [cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						__func__,
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
				break;
			}

			speed_recovery_cnt++;

			udelay(10);
		}

		/* check link training result(speed) */
		if (exynos_pcie->ip_ver >= 0x982000 && val < exynos_pcie->max_link_speed) {
			try_cnt++;

			dev_err(dev, "%s: [cnt:%d] Link is up. But not GEN%d speed, try count: %d\n",
					__func__, speed_recovery_cnt, exynos_pcie->max_link_speed, try_cnt);

			if (try_cnt < 10) {
				gpio_set_value(exynos_pcie->perst_gpio, 0);
				dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
						__func__,
						gpio_get_value(exynos_pcie->perst_gpio));

				/* LTSSM disable */
				exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
						PCIE_APP_LTSSM_ENABLE);

				exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);

				goto retry;
			} else {
				dev_info(dev, "[cnt:%d] Current Link Speed is GEN%d (MAX GEN%d)\n",
						speed_recovery_cnt, val, exynos_pcie->max_link_speed);
			}
		}

		/* one more check L0 state for Gen3 recovery */
		count = 0;
		dev_dbg(dev, "%s: check L0 state for Gen3 recovery\n", __func__);
		while (count < MAX_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &val);
		val = (val >> 16) & 0xf;
		exynos_pcie->current_speed = val;
		dev_info(dev, "Current Link Speed is GEN%d (MAX GEN%d)\n",
				val, exynos_pcie->max_link_speed);

		if (exynos_pcie->ep_device_type != EP_SAMSUNG_MODEM) {
		/* Q-ch enable */
			exynos_elbi_write(exynos_pcie, 0x577, 0x3A8);
			exynos_elbi_write(exynos_pcie, 0x0, 0xD5C);
		}

		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ0);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ0);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1);
		val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2);
		exynos_elbi_write(exynos_pcie, val, PCIE_IRQ2);

		/* enable IRQ */
		//if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
			exynos_pcie_rc_enable_interrupts(pp, 1);
		//}

		/* setup ATU for cfg/mem outbound */
		busdev = EXYNOS_PCIE_ATU_BUS(1) | EXYNOS_PCIE_ATU_DEV(0) | EXYNOS_PCIE_ATU_FUNC(0);
		exynos_pcie_rc_prog_viewport_cfg0(pp, busdev);
		exynos_pcie_rc_prog_viewport_mem_outbound(pp);

	}
	return 0;
}

int exynos_pcie_rc_speedchange(int ch_num, int spd)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int i;
	u32 val, val1, current_spd;

	if (exynos_pcie->state != STATE_LINK_UP) {
		dev_err(pci->dev, "Link is not up\n");
		return 1;
	}

	dev_err(pci->dev, "%s: force l1ss disable\n", __func__);
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, ch_num);

	if (spd > 3 || spd < 1) {
		dev_err(pci->dev, "Unable to change to GEN%d\n", spd);
		return 1;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
	current_spd = current_spd >> 16;
	current_spd &= PCIE_CAP_LINK_SPEED;
	dev_info(pci->dev, "Current link speed(0x80) : from GEN%d\n", current_spd);

	if (current_spd == spd) {
		dev_err(pci->dev, "Already changed to GEN%d\n", spd);
		return 1;
	}

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

	exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | 0x3;
	exynos_pcie_rc_wr_other_conf(pp, exynos_pcie->ep_pci_bus, 0, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	dev_info(pci->dev, "Check EP Current Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, &val);
	val = val & PCIE_CAP_TARGET_LINK_SPEED_MASK;
	val = val | spd;
	exynos_pcie_rc_wr_own_conf(pp, LINK_CONTROL2_LINK_STATUS2_REG, 4, val);
	dev_info(pci->dev, "Set RC Target Speed Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val & DIRECT_SPEED_CHANGE_MASK;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	dev_info(pci->dev, "Clear Direct Speed Change Val = 0x%x\n", val);

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val = val | DIRECT_SPEED_CHANGE_ENABLE;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);
	dev_info(pci->dev, "Set Direct Speed Change Val = 0x%x\n", val);

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &current_spd);
		current_spd = current_spd >> 16;
		current_spd &= PCIE_CAP_LINK_SPEED;

		if (current_spd == spd)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_SPEEDCHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (current_spd != spd) {
		dev_err(pci->dev, "Fail: Unable to change to GEN%d\n", spd);
		return 1;
	}

	dev_info(pci->dev, "Changed link speed(0x80) : to GEN%d\n", current_spd);

	dev_err(pci->dev, "%s: force l1ss enable\n", __func__);
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, ch_num);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_speedchange);

int exynos_pcie_rc_lanechange(int ch_num, int lane)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	int i;
	u32 val, val1, lane_num;

	if (exynos_pcie->ip_ver == 0x992500) {
		dev_err(pci->dev, "9925_EVT0 don't support lane change\n");
		return 1;
	}

	if (exynos_pcie->state != STATE_LINK_UP) {
		dev_err(pci->dev, "Link is not up\n");
		return 1;
	}

	dev_err(pci->dev, "%s: force l1ss disable\n", __func__);
	exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, ch_num);

	if (lane > 2 || lane < 1) {
		dev_err(pci->dev, "Unable to change to %d lane\n", lane);
		return 1;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
	lane_num = lane_num >> 20;
	lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;
	dev_info(pci->dev, "Current lane_num(0x80) : from %d lane\n", lane_num);

	if (lane_num == lane) {
		dev_err(pci->dev, "Already changed to %d lane\n", lane);
		return 1;
	}

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);

	exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val & TARGET_LINK_WIDTH_MASK;
	val = val | lane;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	exynos_pcie_rc_rd_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, &val);
	val = val | DIRECT_LINK_WIDTH_CHANGE_SET;
	exynos_pcie_rc_wr_own_conf(pp, MULTI_LANE_CONTROL_OFF, 4, val);

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_CTRL_STAT, 4, &lane_num);
		lane_num = lane_num >> 20;
		lane_num &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;

		if (lane_num == lane)
			break;
		udelay(10);
	}

	for (i = 0; i < MAX_TIMEOUT_LANECHANGE; i++) {
		val = exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		val1 = exynos_phy_pcs_read(exynos_pcie, 0x188);

		if ((val == 0x11) || ((val == 0x14) && (val1 == 0x6)))
			break;
		udelay(10);
	}

	if (lane_num != lane) {
		dev_err(pci->dev, "Unable to change to %d lane\n", lane);
		return 1;
	}

	dev_info(pci->dev, "Changed lane_num(0x80) : to %d lane\n", lane_num);

	dev_err(pci->dev, "%s: force l1ss enable\n", __func__);
	exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, ch_num);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_lanechange);

int exynos_pcie_rc_poweron(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device *dev;
	u32 val;
	int ret;
	struct irq_desc *exynos_pcie_desc;
	//unsigned long flags;

	if (exynos_pcie->probe_done != 1) {
		pr_err("%s: RC%d has not been successfully probed yet\n", __func__, ch_num);
		return -EPROBE_DEFER;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;
	dev = pci->dev;
	exynos_pcie_desc = irq_to_desc(pp->irq);

	dev_info(dev, "%s, start of poweron, pcie state: %d\n", __func__,
							 exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		pm_runtime_get_sync(dev);
		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		/* set input clk path change to enable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 1);
		}

#if 0
		if (exynos_pcie->use_pcieon_sleep) {
			dev_info(dev, "%s, pcie_is_linkup 1\n", __func__);
			exynos_pcie->pcie_is_linkup = 1;
		}
#endif

		ret = exynos_pcie_rc_clock_enable(pp, PCIE_ENABLE_CLOCK);
		dev_dbg(dev, "pcie clk enable, ret value = %d\n", ret);

#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd) {
			dev_dbg(dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_ACTIVE, exynos_pcie->idle_ip_index);
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_ACTIVE);
		}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num],
					exynos_pcie->int_min_lock);
			dev_dbg(dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif

		/* Enable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_enable(ch_num);

		pinctrl_select_state(exynos_pcie->pcie_pinctrl,
				exynos_pcie->pin_state[PCIE_PIN_ACTIVE]);

		dev_dbg(dev, "[%s] ## PCIe PMU regmap update 1 : BYPASS ##\n", __func__);
		regmap_update_bits(exynos_pcie->pmureg,
				   exynos_pcie->pmu_offset,
				   PCIE_PHY_CONTROL_MASK, 1);

		/* phy all power down clear */
		if (exynos_pcie->phy_ops.phy_all_pwrdn_clear != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn_clear(exynos_pcie,
								exynos_pcie->ch_num);
		}

		exynos_pcie->state = STATE_LINK_UP_TRY;//!!!~~~

		if ((exynos_pcie_desc) && (exynos_pcie_desc->depth > 0)) {
			enable_irq(pp->irq);
		} else {
			dev_dbg(pci->dev, "%s, already enable_irq, so skip\n", __func__);
		}

		if (exynos_pcie_rc_establish_link(pp)) {
			dev_err(dev, "pcie link up fail\n");
			goto poweron_fail;
		}

		/* Enable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val &= ~(HISTORY_BUFFER_CONDITION_SEL);
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_S);

		exynos_elbi_write(exynos_pcie, 0xffffffff, PCIE_STATE_POWER_M);

		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val |= HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		exynos_pcie->state = STATE_LINK_UP;

		exynos_pcie->sudden_linkdown = 0;

		dev_dbg(dev, "[%s] exynos_pcie->probe_ok : %d\n", __func__, exynos_pcie->probe_ok);
		if (!exynos_pcie->probe_ok) {
			pci_rescan_bus(exynos_pcie->pci_dev->bus);

			exynos_pcie->ep_pci_dev = exynos_pcie_get_pci_dev(&pci->pp);
			memcpy(&exynos_pcie->dup_ep_dev,
					&exynos_pcie->ep_pci_dev->dev,
					sizeof(exynos_pcie->dup_ep_dev));
			exynos_pcie->dup_ep_dev.dma_ops = NULL;

			exynos_pcie->ep_pci_dev->dma_mask = DMA_BIT_MASK(36);
			exynos_pcie->ep_pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
			dma_set_seg_boundary(&(exynos_pcie->ep_pci_dev->dev), DMA_BIT_MASK(36));

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
			if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
					(exynos_pcie->ep_device_type == EP_QC_WIFI) ||
					(exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI)) {

				/* This function is only for BCM WIFI devices */
				set_dma_ops(&exynos_pcie->ep_pci_dev->dev, &exynos_pcie_gen2_dma_ops);
				dev_dbg(pci->dev, "DMA operations are changed to support SysMMU or S2MPU\n");
			}
#endif

			if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
					(exynos_pcie->ep_device_type == EP_QC_WIFI)) {
				exynos_pcie_config_l1ss(&pci->pp);
			}

#if IS_ENABLED(CONFIG_PCI_MSI)
			if (exynos_pcie->use_msi) {
				if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
						(exynos_pcie->ep_device_type == EP_QC_WIFI) ||
						(exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI)) {
					exynos_pcie_rc_msi_init_wifi(pp);
				}
				else if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
					ret = exynos_pcie_rc_msi_init(pp);
					if (ret) {
						dev_err(dev, "%s: failed to MSI initialization(%d)\n",
								__func__, ret);
						pm_runtime_put_sync(dev);
						return ret;
					}
				}
			}
#endif

			if (pci_save_state(exynos_pcie->pci_dev)) {
				dev_err(dev, "Failed to save pcie state\n");
				goto poweron_fail;
			}
			exynos_pcie->pci_saved_configs =
				pci_store_saved_state(exynos_pcie->pci_dev);

			exynos_pcie->probe_ok = 1;
		} else if (exynos_pcie->probe_ok) {
			if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
					(exynos_pcie->ep_device_type == EP_QC_WIFI)) {
				if (exynos_pcie->boot_cnt == 0) {
					schedule_delayed_work(
							&exynos_pcie->l1ss_boot_delay_work,
							msecs_to_jiffies(40000));
					exynos_pcie->boot_cnt++;
					exynos_pcie->l1ss_ctrl_id_state |=
						PCIE_L1SS_CTRL_BOOT;
				}
			}

#if IS_ENABLED(CONFIG_PCI_MSI)
			if (exynos_pcie->use_msi) {
				if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
						(exynos_pcie->ep_device_type == EP_QC_WIFI) ||
						(exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI)) {
					exynos_pcie_rc_msi_init_wifi(pp);
				}
				else if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM) {
					ret = exynos_pcie_rc_msi_init(pp);
					if (ret) {
						dev_err(dev, "%s: failed to MSI initialization(%d)\n",
								__func__, ret);
						return ret;
					}
				}
			}
#endif

			if (pci_load_saved_state(exynos_pcie->pci_dev,
					     exynos_pcie->pci_saved_configs)) {
				dev_err(dev, "Failed to load pcie state\n");
				goto poweron_fail;
			}
			pci_restore_state(exynos_pcie->pci_dev);

			if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
					(exynos_pcie->ep_device_type == EP_QC_WIFI)) {
				exynos_pcie_config_l1ss(&pci->pp);
			}
		}
		exynos_pcie->cpl_timeout_recovery = 0;
	}

	dev_info(dev, "%s, end of poweron, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	return 0;

poweron_fail:
	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie->cpl_timeout_recovery = 0;
	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweron);

void exynos_pcie_rc_poweroff(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	struct device *dev = pci->dev;
	unsigned long flags;
	u32 val;

	dev_info(dev, "%s, start of poweroff, pcie state: %d\n", __func__,
		 exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_UP ||
	    exynos_pcie->state == STATE_LINK_DOWN_TRY) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;

		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		disable_irq(pp->irq);

		if (exynos_pcie->ip_ver == 0x982000) {
			/* only support EVT1.1 */
				val = exynos_elbi_read(exynos_pcie, PCIE_IRQ1_EN);
				val &= ~IRQ_LINKDOWN_ENABLE_EVT1_1;
				exynos_elbi_write(exynos_pcie, val, PCIE_IRQ1_EN);
		}

		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		exynos_pcie_rc_send_pme_turn_off(exynos_pcie);

		if (exynos_pcie->ep_device_type != EP_SAMSUNG_MODEM) {
			writel(0xA, exynos_pcie->phy_base + 0x580);
			writel(0x300DE, exynos_pcie->phy_pcs_base + 0x150);
		}

		exynos_pcie->state = STATE_LINK_DOWN;

		if (exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
			dev_dbg(dev, "%s, ep samsung wifi needs delay before setting perst to low\n", __func__);
			mdelay(10); // It needs delay before setting PERST to LOW
		}

		/* Disable SysMMU */
		if (exynos_pcie->use_sysmmu)
			pcie_sysmmu_disable(ch_num);

		/* Disable history buffer */
		val = exynos_elbi_read(exynos_pcie, PCIE_STATE_HISTORY_CHECK);
		val &= ~HISTORY_BUFFER_ENABLE;
		exynos_elbi_write(exynos_pcie, val, PCIE_STATE_HISTORY_CHECK);

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		dev_info(dev, "%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));

		if (exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
			dev_dbg(dev, "%s, ep samsung wifi needs delay after setting perst to low\n", __func__);
			mdelay(20); // It needs delay before setting PERST to LOW
		}

		/* LTSSM disable */
		exynos_elbi_write(exynos_pcie, PCIE_ELBI_LTSSM_DISABLE,
				PCIE_APP_LTSSM_ENABLE);

		spin_lock(&exynos_pcie->reg_lock);
		/* force SOFT_PWR_RESET */
		dev_dbg(dev, "%s: Set PERST to LOW, gpio val = %d\n",
			__func__, gpio_get_value(exynos_pcie->perst_gpio));
		val = exynos_elbi_read(exynos_pcie, PCIE_SOFT_RESET);
		val &= ~SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
		udelay(20);
		val |= SOFT_PWR_RESET;
		exynos_elbi_write(exynos_pcie, val, PCIE_SOFT_RESET);
		spin_unlock(&exynos_pcie->reg_lock);

		/* phy all power down */
		if (exynos_pcie->phy_ops.phy_all_pwrdn != NULL) {
			exynos_pcie->phy_ops.phy_all_pwrdn(exynos_pcie,
					exynos_pcie->ch_num);
		}

		/* set input clk path change to disable */
		if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
			exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
		}
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

		exynos_pcie_rc_phy_clock_enable(pp, PCIE_DISABLE_CLOCK);
		exynos_pcie_rc_clock_enable(pp, PCIE_DISABLE_CLOCK);

		exynos_pcie->atu_ok = 0;

		if (!IS_ERR(exynos_pcie->pin_state[PCIE_PIN_IDLE]))
			pinctrl_select_state(exynos_pcie->pcie_pinctrl,
					exynos_pcie->pin_state[PCIE_PIN_IDLE]);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		if (exynos_pcie->int_min_lock) {
			exynos_pm_qos_update_request(&exynos_pcie_int_qos[ch_num], 0);
			dev_dbg(dev, "%s: pcie int_min_lock = %d\n",
					__func__, exynos_pcie->int_min_lock);
		}
#endif

#if IS_ENABLED(CONFIG_CPU_IDLE)
		if (exynos_pcie->use_sicd) {
			dev_dbg(dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);
			exynos_update_ip_idle_status(
					exynos_pcie->idle_ip_index,
					PCIE_IS_IDLE);
		}
#endif
		pm_runtime_put_sync(dev);
	}
#if 0
	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(dev, "%s, pcie_is_linkup 0\n", __func__);
		exynos_pcie->pcie_is_linkup = 0;
	}
#endif
	/* TCXO_FAR off */
	//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);

	dev_info(dev, "%s, end of poweroff, pcie state: %d\n",  __func__,
			exynos_pcie->state);

	return;
}
EXPORT_SYMBOL(exynos_pcie_rc_poweroff);

/* Get EP pci_dev structure of BUS 1 */
static struct pci_dev *exynos_pcie_get_pci_dev(struct pcie_port *pp)
{
	int domain_num;
	static struct pci_dev *ep_pci_dev;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (ep_pci_dev != NULL)
		return ep_pci_dev;

	/* Get EP vendor/device ID to get pci_dev structure */
	domain_num = exynos_pcie->pci_dev->bus->domain_nr;

	if (exynos_pcie->ep_pci_bus == NULL)
		exynos_pcie->ep_pci_bus = pci_find_bus(domain_num, 1);

	exynos_pcie_rc_rd_other_conf(pp, exynos_pcie->ep_pci_bus, 0, PCI_VENDOR_ID, 4, &val);
	/* DBG: dev_info(pci->dev, "(%s): ep_pci_device: vendor/device id = 0x%x\n",
	 *          *                      __func__, val);
	 *                   */

	ep_pci_dev = pci_get_device(val & ID_MASK, (val >> 16) & ID_MASK, NULL);
	if (ep_pci_dev == NULL)
		return NULL;

	exynos_pcie->ep_pcie_cap_off = ep_pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	exynos_pcie->ep_l1ss_cap_off = pci_find_ext_capability(ep_pci_dev, PCI_EXT_CAP_ID_L1SS);
	if (!exynos_pcie->ep_l1ss_cap_off)
		pr_info("%s: cannot find l1ss capability offset\n", __func__);

	return ep_pci_dev;
}

void exynos_pcie_config_l1ss_bcm_wifi(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;
	u32 val;

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev, "Failed to set L1SS Enable (pci_dev == NULL)!\n");
		return ;
	}

	pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL2,
			PORT_LINK_TPOWERON_130US);
	pci_write_config_dword(ep_pci_dev, 0x1B4, 0x10031003);
	pci_read_config_dword(ep_pci_dev, 0xD4, &val);
	pci_write_config_dword(ep_pci_dev, 0xD4, val | (1 << 10));

	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
	val |= PORT_LINK_TCOMMON_70US | PORT_LINK_L1SS_ENABLE |
		PORT_LINK_L12_LTR_THRESHOLD;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2_GEN2, 4,
			PORT_LINK_TPOWERON_130US);
	/*
	   val = PORT_LINK_L1SS_T_PCLKACK | PORT_LINK_L1SS_T_L1_2 |
	   PORT_LINK_L1SS_T_POWER_OFF;
	 */
	exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	/* Enable L1SS on Root Complex */
	exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val &= ~PCI_EXP_LNKCTL_ASPMC;
	val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
	exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);

	/* Enable supporting ASPM/PCIPM */
	pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
	pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL,
			val | WIFI_COMMON_RESTORE_TIME | WIFI_ALL_PM_ENABEL |
			PORT_LINK_L12_LTR_THRESHOLD);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, val);

		dev_dbg(pci->dev, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_CLK_REQ_EN | WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL, val);

		exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, val);
		dev_dbg(pci->dev, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
}

void exynos_pcie_config_l1ss_common(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;
	u32 val;

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
		return ;
	}
	pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl2_off,
			PORT_LINK_TPOWERON_130US);

	/* Get COMMON_MODE Value */
	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CAP_GEN2, 4, &val);
	val &= ~(PORT_LINK_TCOMMON_MASK);
	val |= WIFI_COMMON_RESTORE_TIME_70US;
	exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CAP, 4, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CAP, 4, &val);
	dev_dbg(pci->dev, "%s:RC l1ss cap: 0x%x\n", __func__, val);


	exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, PCIE_L1_SUB_VAL);
	exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
			PCI_EXP_DEVCTL2_LTR_EN);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* RC L1ss Enable */
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
		val &= ~(PORT_LINK_TCOMMON_MASK);
		val |= WIFI_COMMON_RESTORE_TIME_70US | PORT_LINK_L1SS_ENABLE |
			PORT_LINK_L12_LTR_THRESHOLD;
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2_GEN2, 4,
				PORT_LINK_TPOWERON_130US);

		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
		dev_dbg(pci->dev, "%s: RC l1ss control: 0x%x\n", __func__, val);

		/* EP L1ss Enable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off,
				val | WIFI_ALL_PM_ENABEL | PORT_LINK_L12_LTR_THRESHOLD);
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		dev_dbg(pci->dev, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC ASPM Enable */
		exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
		exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
		dev_dbg(pci->dev, "%s: RC link control: 0x%x\n", __func__, val);

		/* EP ASPM Enable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK |
			WIFI_ASPM_L1_ENTRY_EN;
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);
		dev_dbg(pci->dev, "%s: EP link control: 0x%x\n", __func__, val);

		dev_dbg(pci->dev, "l1ss enabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	} else {
		/* EP ASPM Dsiable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
		val &= ~(WIFI_ASPM_CONTROL_MASK);
		val |= WIFI_USE_SAME_REF_CLK;
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
		dev_dbg(pci->dev, "%s: EP link control: 0x%x\n", __func__, val);

		/* RC ASPM Disable */
		exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, &val);
		val &= ~PCI_EXP_LNKCTL_ASPMC;
		val |= PCI_EXP_LNKCTL_CCC;
		exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL,
				4, val);
		dev_dbg(pci->dev, "%s: RC link control: 0x%x\n", __func__, val);

		/* EP L1ss Disable */
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		val |= PORT_LINK_L12_LTR_THRESHOLD;
		val &= ~(WIFI_ALL_PM_ENABEL);
		pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
		pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
		dev_dbg(pci->dev, "%s: EP l1ss control: 0x%x\n", __func__, val);

		/* RC L1ss Disable */
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
		val &= ~(PORT_LINK_TCOMMON_MASK);
		val |= WIFI_COMMON_RESTORE_TIME_70US | PORT_LINK_L12_LTR_THRESHOLD;
		val &= ~(PORT_LINK_L1SS_ENABLE);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_130US);
		exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		dev_dbg(pci->dev, "%s: RC l1ss control: 0x%x\n", __func__, val);

		dev_dbg(pci->dev, "l1ss disabled(0x%x)\n",
				exynos_pcie->l1ss_ctrl_id_state);
	}
}

void exynos_pcie_config_l1ss(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	unsigned long flags;
	struct pci_dev *ep_pci_dev;

	/* This function is only for BCM WIFI devices */
	if ((exynos_pcie->ep_device_type != EP_BCM_WIFI) && (exynos_pcie->ep_device_type != EP_QC_WIFI)) {
		dev_err(pci->dev, "Can't set L1SS(It's not WIFI device)!!!\n");
		return ;
	}

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		dev_err(pci->dev,
				"Failed to set L1SS Enable (pci_dev == NULL)!!!\n");
#ifdef CONFIG_SEC_PANIC_PCIE_ERR			
		panic("[PCIe Case#3] No ep_pci_dev found! %s\n", exynos_pcie->ep_device_name);
#endif				
		return ;
	}

	exynos_pcie->ep_link_ctrl_off = ep_pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	exynos_pcie->ep_l1ss_ctrl1_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL1;
	exynos_pcie->ep_l1ss_ctrl2_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL2;
	dev_dbg(pci->dev, "%s: (offset)link_ctrl: 0x%x, l1ss_ctrl1: 0x%x, l1ss_ctrl2: 0x%x\n",
			__func__,
			exynos_pcie->ep_link_ctrl_off,
			exynos_pcie->ep_l1ss_ctrl1_off,
			exynos_pcie->ep_l1ss_ctrl2_off);

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
		exynos_pcie_config_l1ss_bcm_wifi(pp);
	} else if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		exynos_pcie_config_l1ss_common(pp);
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
}

static int exynos_pcie_set_l1ss_samsung(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct device *dev = pci->dev;
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;
	u32 l1sub_ctrl1_reg, l1sub_ctrl2_reg;

	/* This function is only working with the devices which support L1SS */
	if (exynos_pcie->ep_device_type != EP_SAMSUNG_MODEM &&
			exynos_pcie->ep_device_type != EP_SAMSUNG_WIFI) {
		dev_err(dev, "Can't set L1SS!!! (L1SS not supported)\n");

		return -EINVAL;
	}

	dev_info(dev, "%s:L1SS_START(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	if (exynos_pcie->state != STATE_LINK_UP || exynos_pcie->atu_ok == 0) {
		spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_info(dev, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);

		return -1;
	} else {
		ep_pci_dev = exynos_pcie_get_pci_dev(pp);
		if (ep_pci_dev == NULL) {
			dev_err(dev, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
					enable ? "ENABLE" : "FALSE");
			return -EINVAL;
		}
		exynos_pcie->ep_link_ctrl_off = ep_pci_dev->pcie_cap + PCI_EXP_LNKCTL;
		exynos_pcie->ep_l1ss_ctrl1_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL1;
		exynos_pcie->ep_l1ss_ctrl2_off = exynos_pcie->ep_l1ss_cap_off + PCI_L1SS_CTL2;

		if (exynos_pcie->ch_num == 0) { /* For GEN2 Channel */
			l1sub_ctrl1_reg = PCIE_LINK_L1SS_CONTROL_GEN2;
			l1sub_ctrl2_reg = PCIE_LINK_L1SS_CONTROL2_GEN2;
		} else if (exynos_pcie->ch_num == 1) { /* For GEN3 Channel */
			l1sub_ctrl1_reg = PCIE_LINK_L1SS_CONTROL;
			l1sub_ctrl2_reg = PCIE_LINK_L1SS_CONTROL2;
		} else {
			dev_err(dev, "%s: unexpected channel number!!!\n");
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (enable) {	/* enable == 1 */
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);

		if (exynos_pcie->l1ss_ctrl_id_state == 0) {

			/* RC & EP L1SS & ASPM setting */

			/* 1-1 RC, set L1SS */
			exynos_pcie_rc_rd_own_conf(pp, l1sub_ctrl1_reg, 4, &val);
			/* Actual TCOMMON value is 42usec (val = 0x2a << 8) */
			val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
			exynos_pcie_rc_wr_own_conf(pp, l1sub_ctrl1_reg, 4, val);
			dev_info(dev, "RC L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* 1-2 RC: set TPOWERON */
			/* Set TPOWERON value for RC: 90->170 usec */
			exynos_pcie_rc_wr_own_conf(pp, l1sub_ctrl2_reg, 4,
					PORT_LINK_TPOWERON_170US);

			/* exynos_pcie_rc_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4,
			 * PCIE_L1_SUB_VAL);
			 */

			/* 1-3 RC: set LTR_EN */
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
					PCI_EXP_DEVCTL2_LTR_EN);

			/* 2-1 EP: set LTR_EN (reg_addr = 0x98) */
			pci_read_config_dword(ep_pci_dev, exp_cap_off + PCI_EXP_DEVCTL2, &val);
			val |= PCI_EXP_DEVCTL2_LTR_EN;
			pci_write_config_dword(ep_pci_dev, exp_cap_off + PCI_EXP_DEVCTL2, val);
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4,
					PCI_EXP_DEVCTL2_LTR_EN);

			/* 2-2 EP: set TPOWERON */
			/* Set TPOWERON value for EP: 90->170 usec */
			pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl2_off,
					PORT_LINK_TPOWERON_170US);

			/* 2-3 EP: set Entrance latency */
			/* Set L1.2 Enterance Latency for EP: 64 usec */
			pci_read_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, &val);
			val &= ~PCIE_L1_ENTERANCE_LATENCY;
			val |= PCIE_L1_ENTERANCE_LATENCY_64us;
			pci_write_config_dword(ep_pci_dev, PCIE_ACK_F_ASPM_CONTROL, val);

			/* 2-4 EP: set L1SS */
			pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
			val |= PORT_LINK_L1SS_ENABLE;
			pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
			dev_info(dev, "Enable EP L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* 3. RC ASPM Enable*/
			exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
			dev_info(dev, "Enable RC ASPM(0x80) = 0x%x\n", val);

			/* 4. EP ASPM Enable */
			pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_CLKREQ_EN |
				PCI_EXP_LNKCTL_ASPM_L1;
			pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);
			dev_info(dev, "Enable EP ASPM(0x80) = 0x%x\n", val);

			/* DBG:
			 * dev_info(dev, "(%s): l1ss_enabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *			__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */
		}
	} else {	/* enable == 0 */
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			/* 1. EP ASPM Disable */
			pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, &val);
			val &= ~(PCI_EXP_LNKCTL_ASPMC);
			pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off, val);
			dev_info(dev, "Disable EP ASPM(0x80) = 0x%x\n", val);

			/* 2. RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_rc_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
			dev_info(dev, "Disable RC ASPM(0x80) = 0x%x\n", val);

			/* EP: clear L1SS */
			pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
			dev_info(dev, "Disable L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* RC: clear L1SS */
			exynos_pcie_rc_rd_own_conf(pp, l1sub_ctrl1_reg, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_rc_wr_own_conf(pp, l1sub_ctrl1_reg, 4, val);
			dev_info(dev, "Disable RC L1SS_CONTROL(0x19C) = 0x%x\n", val);

			/* DBG:
			 * dev_info(dev, "(%s): l1ss_disabled(l1ss_ctrl_id_state = 0x%x)\n",
			 *		__func__, exynos_pcie->l1ss_ctrl_id_state);
			 */
		}
	}
	dev_info(dev, "LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
			exynos_phy_pcs_read(exynos_pcie, 0x188));

	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	dev_info(dev, "%s:L1SS_END(l1ss_ctrl_id_state=0x%x, id=0x%x, enable=%d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

static int exynos_pcie_set_l1ss_wifi(int enable, struct pcie_port *pp, int id)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;
	unsigned long flags;
	struct pci_dev *ep_pci_dev;
	u32 exp_cap_off = PCIE_CAP_OFFSET;

	/* This function is only for BCM WIFI devices */
	if ((exynos_pcie->ep_device_type != EP_BCM_WIFI) && (exynos_pcie->ep_device_type != EP_QC_WIFI)) {
		dev_err(pci->dev, "Can't set L1SS(It's not WIFI device)!!!\n");
		return -EINVAL;
	}

	dev_info(pci->dev, "%s: START (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	if (exynos_pcie->state != STATE_LINK_UP || exynos_pcie->atu_ok == 0) {
		if (enable)
			exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		else
			exynos_pcie->l1ss_ctrl_id_state |= id;
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_info(pci->dev, "%s: It's not needed. This will be set later."
				"(state = 0x%x, id = 0x%x)\n",
				__func__, exynos_pcie->l1ss_ctrl_id_state, id);
		return -1;
	}

	ep_pci_dev = exynos_pcie_get_pci_dev(pp);
	if (ep_pci_dev == NULL) {
		spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
		dev_err(pci->dev, "Failed to set L1SS %s (pci_dev == NULL)!!!\n",
				enable ? "ENABLE" : "FALSE");
		return -EINVAL;
	}

	if (enable) {
		exynos_pcie->l1ss_ctrl_id_state &= ~(id);
		if (exynos_pcie->l1ss_ctrl_id_state == 0) {
			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* RC L1ss Enable */
				exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
				val |= PORT_LINK_L1SS_ENABLE;
				exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
				dev_dbg(pci->dev, "Enable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);

				/* EP L1ss Enable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
				val |= WIFI_ALL_PM_ENABEL;
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, val);
				dev_dbg(pci->dev, "Enable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			} else {
				/* RC L1ss Enable */
				exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
				val |= PORT_LINK_L1SS_ENABLE;
				exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
				dev_dbg(pci->dev, "Enable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);

				/* EP L1ss Enable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
				val |= WIFI_ALL_PM_ENABEL;
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
				dev_dbg(pci->dev, "Enable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			}

			/* RC ASPM Enable */
			exynos_pcie_rc_rd_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
			exynos_pcie_rc_wr_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, val);

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP ASPM Enable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL,
						&val);
				val |= WIFI_ASPM_L1_ENTRY_EN;
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_LINKCTRL,
						val);
			} else {
				/* EP ASPM Enable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off,
						&val);
				val |= WIFI_ASPM_L1_ENTRY_EN;
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_link_ctrl_off,
						val);
			}
		}
	} else {
		if (exynos_pcie->l1ss_ctrl_id_state) {
			exynos_pcie->l1ss_ctrl_id_state |= id;
		} else {
			exynos_pcie->l1ss_ctrl_id_state |= id;

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP ASPM Disable */
				pci_read_config_dword(ep_pci_dev,
						WIFI_L1SS_LINKCTRL, &val);
				val &= ~(WIFI_ASPM_CONTROL_MASK);
				pci_write_config_dword(ep_pci_dev,
						WIFI_L1SS_LINKCTRL, val);
			} else {
				/* EP ASPM Disable */
				pci_read_config_dword(ep_pci_dev,
						exynos_pcie->ep_link_ctrl_off, &val);
				val &= ~(WIFI_ASPM_CONTROL_MASK);
				pci_write_config_dword(ep_pci_dev,
						exynos_pcie->ep_link_ctrl_off, val);
			}

			/* RC ASPM Disable */
			exynos_pcie_rc_rd_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
			val &= ~PCI_EXP_LNKCTL_ASPMC;
			exynos_pcie_rc_wr_own_conf(pp,
					exp_cap_off + PCI_EXP_LNKCTL, 4, val);

			if (exynos_pcie->ep_device_type == EP_BCM_WIFI) {
				/* EP L1ss Disable */
				pci_read_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, &val);
				val &= ~(WIFI_ALL_PM_ENABEL);
				pci_write_config_dword(ep_pci_dev, WIFI_L1SS_CONTROL, val);
				dev_dbg(pci->dev, "Disable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			} else {
				/* EP L1ss Disable */
				pci_read_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, &val);
				val &= ~(WIFI_ALL_PM_ENABEL);
				pci_write_config_dword(ep_pci_dev, exynos_pcie->ep_l1ss_ctrl1_off, val);
				dev_dbg(pci->dev, "Disable L1ss: EP L1SS_CONTROL = 0x%x\n", val);
			}

			/* RC L1ss Disable */
			exynos_pcie_rc_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, &val);
			val &= ~(PORT_LINK_L1SS_ENABLE);
			exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL_GEN2, 4, val);
			dev_dbg(pci->dev, "Disable L1ss: L1SS_CONTROL (RC_read)=0x%x\n", val);
		}
	}
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);
	dev_info(pci->dev, "%s: END (state = 0x%x, id = 0x%x, enable = %d)\n",
			__func__, exynos_pcie->l1ss_ctrl_id_state, id, enable);

	return 0;
}

int exynos_pcie_l1ss_ctrl(int enable, int id, int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	if (exynos_pcie->ep_device_type == EP_BCM_WIFI || exynos_pcie->ep_device_type == EP_QC_WIFI) {
		return	exynos_pcie_set_l1ss_wifi(enable, pp, id);
	}
	else if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM ||
			exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
		return	exynos_pcie_set_l1ss_samsung(enable, pp, id);
	}
	else
		return -EINVAL;
}
EXPORT_SYMBOL(exynos_pcie_l1ss_ctrl);

int exynos_pcie_l1_exit(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	u32 count = 0, ret = 0, val = 0;
	unsigned long flags;

	spin_lock_irqsave(&exynos_pcie->pcie_l1_exit_lock, flags);

	if (exynos_pcie->l1ss_ctrl_id_state == 0) {
		/* Set s/w L1 exit mode */
		exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val &= ~APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);

		/* Max timeout = 3ms (300 * 10us) */
		while (count < MAX_L1_EXIT_TIMEOUT) {
			val = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x1f;
			if (val == 0x11)
				break;

			count++;

			udelay(10);
		}

		if (count >= MAX_L1_EXIT_TIMEOUT) {
			pr_err("%s: cannot change to L0(LTSSM = 0x%x, cnt = %d)\n",
					__func__, val, count);
			ret = -EPIPE;
		} else {
			/* remove too much print */
			/* pr_err("%s: L0 state(LTSSM = 0x%x, cnt = %d)\n",
			   __func__, val, count); */
		}

		/* Set h/w L1 exit mode */
		val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
		val |= APP_REQ_EXIT_L1_MODE;
		exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
		exynos_elbi_write(exynos_pcie, 0x0, PCIE_APP_REQ_EXIT_L1);
	} else {
		/* remove too much print */
		/* pr_err("%s: skip!!! l1.2 is already diabled(id = 0x%x)\n",
		   __func__, exynos_pcie->l1ss_ctrl_id_state); */
	}

	spin_unlock_irqrestore(&exynos_pcie->pcie_l1_exit_lock, flags);

	return ret;

}
EXPORT_SYMBOL(exynos_pcie_l1_exit);

int exynos_pcie_host_v1_poweron(int ch_num)
{
	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweron);

void exynos_pcie_host_v1_poweroff(int ch_num)
{
	return exynos_pcie_rc_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_host_v1_poweroff);

int exynos_pcie_pm_resume(int ch_num)
{
	return exynos_pcie_rc_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_resume);

void exynos_pcie_pm_suspend(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci = exynos_pcie->pci;
	unsigned long flags;

	dev_info(pci->dev, "## exynos_pcie_pm_suspend(state: %d)\n", exynos_pcie->state);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(pci->dev, "RC%d already off\n", exynos_pcie->ch_num);
		return;
	}

	spin_lock_irqsave(&exynos_pcie->conf_lock, flags);
	exynos_pcie->state = STATE_LINK_DOWN_TRY;
	spin_unlock_irqrestore(&exynos_pcie->conf_lock, flags);

	exynos_pcie_rc_poweroff(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_suspend);

bool exynos_pcie_rc_get_cpl_timeout_state(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	return exynos_pcie->cpl_timeout_recovery;
}
EXPORT_SYMBOL(exynos_pcie_rc_get_cpl_timeout_state);

void exynos_pcie_rc_set_cpl_timeout_state(int ch_num, bool recovery)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];

	pr_err("[%s] set cpl_timeout_state to recovery_on\n", __func__);
	exynos_pcie->cpl_timeout_recovery = recovery;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_cpl_timeout_state);

/* PCIe link status checking function from S359 ref. code */
int exynos_pcie_rc_chk_link_status(int ch_num)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct device *dev;

	u32 val;
	int link_status;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}
	pci = exynos_pcie->pci;
	dev = pci->dev;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return 0;

	if (exynos_pcie->ep_device_type == EP_SAMSUNG_MODEM
			|| exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if (val >= 0x0d && val <= 0x14) {
			link_status = 1;
		} else {
			dev_err(dev, "Check unexpected state - H/W:0x%x, S/W:%d\n",
					val, exynos_pcie->state);
			/* exynos_pcie->state = STATE_LINK_DOWN; */
			link_status = 1;
		}

		return link_status;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_chk_link_status);

int exynos_pcie_register_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (pp) {
		exynos_pcie->event_reg = reg;
		dev_info(pci->dev,
				"Event 0x%x is registered for RC %d\n",
				reg->events, exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_register_event);

int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);
	if (pp) {
		exynos_pcie->event_reg = NULL;
		dev_info(pci->dev, "Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_deregister_event);

int exynos_pcie_host_v1_register_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	u32 id;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (reg->events == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (reg->events == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		id = 1;
	} else {
		pr_err("PCIe: unknown event!!!\n");
		return -EINVAL;
	}
	pr_err("[%s] event = 0x%x, id = %d\n", __func__, reg->events, id);

	if (pp) {
		exynos_pcie->rc_event_reg[id] = reg;
		dev_info(pci->dev,
				"Event 0x%x is registered for RC %d\n",
				reg->events, exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_host_v1_register_event);

int exynos_pcie_host_v1_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	u32 id;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	pci = to_dw_pcie_from_pp(pp);
	exynos_pcie = to_exynos_pcie(pci);

	if (reg->events == EXYNOS_PCIE_EVENT_LINKDOWN) {
		id = 0;
	} else if (reg->events == EXYNOS_PCIE_EVENT_CPL_TIMEOUT) {
		id = 1;
	} else {
		pr_err("PCIe: unknown event!!!\n");
		return -EINVAL;
	}

	if (pp) {
		exynos_pcie->rc_event_reg[id] = NULL;
		dev_info(pci->dev, "Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_host_v1_deregister_event);

int exynos_pcie_rc_set_affinity(int ch_num, int affinity)
{
	struct exynos_pcie *exynos_pcie = &g_pcie_rc[ch_num];
	struct dw_pcie *pci;
	struct pcie_port *pp;

	if (!exynos_pcie) {
		pr_err("%s: ch#%d PCIe device is not loaded\n", __func__, ch_num);
		return -ENODEV;
	}

	pci = exynos_pcie->pci;
	pp = &pci->pp;

	irq_set_affinity_hint(pp->irq, cpumask_of(affinity));

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_set_affinity);

#if IS_ENABLED(CONFIG_CPU_IDLE)
static void __maybe_unused exynos_pcie_rc_set_tpoweron(struct pcie_port *pp,
							int max)
{
	void __iomem *ep_dbi_base = pp->va_cfg0_base;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return;

	/* Disable ASPM */
	val = readl(ep_dbi_base + WIFI_L1SS_LINKCTRL);
	val &= ~(WIFI_ASPM_CONTROL_MASK);
	writel(val, ep_dbi_base + WIFI_L1SS_LINKCTRL);

	val = readl(ep_dbi_base + WIFI_L1SS_CONTROL);
	writel(val & ~(WIFI_ALL_PM_ENABEL),
			ep_dbi_base + WIFI_L1SS_CONTROL);

	if (max) {
		writel(PORT_LINK_TPOWERON_3100US,
				ep_dbi_base + WIFI_L1SS_CONTROL2);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_3100US);
	} else {
		writel(PORT_LINK_TPOWERON_130US,
				ep_dbi_base + WIFI_L1SS_CONTROL2);
		exynos_pcie_rc_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4,
				PORT_LINK_TPOWERON_130US);
	}

	/* Enable L1ss */
	val = readl(ep_dbi_base + WIFI_L1SS_LINKCTRL);
	val |= WIFI_ASPM_L1_ENTRY_EN;
	writel(val, ep_dbi_base + WIFI_L1SS_LINKCTRL);

	val = readl(ep_dbi_base + WIFI_L1SS_CONTROL);
	val |= WIFI_ALL_PM_ENABEL;
	writel(val, ep_dbi_base + WIFI_L1SS_CONTROL);
}

/* Temporary remove: Need to enable to use sicd powermode */
#ifdef PCIE_SICD
static int exynos_pcie_rc_power_mode_event(struct notifier_block *nb,
					unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct exynos_pcie *exynos_pcie = container_of(nb,
				struct exynos_pcie, power_mode_nb);
	u32 val;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	dev_info(pci->dev, "[%s] event: %lx\n", __func__, event);
	switch (event) {
	case SICD_ENTER:
		if (exynos_pcie->use_sicd) {
			if (exynos_pcie->ip_ver >= 0x889500) {
				if (exynos_pcie->state != STATE_LINK_DOWN) {
					val = readl(exynos_pcie->elbi_base +
						PCIE_ELBI_RDLH_LINKUP) & 0x1f;
					if (val == 0x14 || val == 0x15) {
						ret = NOTIFY_DONE;
						/* Change tpower on time to
						 * value
						 */
						exynos_pcie_rc_set_tpoweron(pp, 1);
					} else {
						ret = NOTIFY_BAD;
					}
				}
			}
		}
		break;
	case SICD_EXIT:
		if (exynos_pcie->use_sicd) {
			if (exynos_pcie->ip_ver >= 0x889500) {
				if (exynos_pcie->state != STATE_LINK_DOWN) {
					/* Change tpower on time to NORMAL
					 * value */
					exynos_pcie_rc_set_tpoweron(pp, 0);
				}
			}
		}
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return notifier_from_errno(ret);
}
#endif
#endif
static int exynos_pcie_msi_set_affinity(struct irq_data *irq_data,
				   const struct cpumask *mask, bool force)
{
	struct pcie_port *pp = irq_data->parent_data->domain->host_data;

	irq_set_affinity_hint(pp->irq, mask);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
int exynos_pcie_rc_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct exynos_pcie *exynos_pcie = container_of(nb, struct exynos_pcie, itmon_nb);
	struct device *dev = exynos_pcie->pci->dev;
	struct itmon_notifier *itmon_info = nb_data;
	unsigned int val;

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/* only for 9830 HSI2 block */
	if (exynos_pcie->ip_ver == 0x983000) {
		if ((itmon_info->port && !strcmp(itmon_info->port, "HSI2")) ||
				(itmon_info->dest && !strcmp(itmon_info->dest, "HSI2"))) {
			regmap_read(exynos_pcie->pmureg, exynos_pcie->pmu_offset, &val);
			dev_info(dev, "### PMU PHY Isolation : 0x%x\n", val);

			exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		}
	} else {
		dev_dbg(dev, "skip register dump(ip_ver = 0x%x)\n", exynos_pcie->ip_ver);
	}

	return NOTIFY_DONE;
}
#endif

static int exynos_pcie_rc_add_port(struct platform_device *pdev,
					struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct irq_domain *msi_domain;
	struct msi_domain_info *msi_domain_info;
	int ret;
	u32 val, vendor_id, device_id;
#if IS_ENABLED(CONFIG_PCI_MSI)
	unsigned long msi_addr_from_dt = 0;
#endif

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}
	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_rc_irq_handler,
				IRQF_SHARED | IRQF_TRIGGER_HIGH, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	//pp->root_bus_nr = 0;
	pp->ops = &exynos_pcie_rc_ops;

	exynos_pcie_setup_rc(pp);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to dw pcie host init\n");
		return ret;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
	vendor_id = val & ID_MASK;
	device_id = (val >> 16) & ID_MASK;
	exynos_pcie->pci_dev = pci_get_device(vendor_id,
			device_id, NULL);
	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	}
	dev_info(pci->dev, "(%s): ep_pci_device: vendor/device id = 0x%x\n", __func__, val);

	exynos_pcie->pci_dev->dma_mask = DMA_BIT_MASK(36);
	exynos_pcie->pci_dev->dev.coherent_dma_mask = DMA_BIT_MASK(36);
	dma_set_seg_boundary(&(exynos_pcie->pci_dev->dev), DMA_BIT_MASK(36));

#if IS_ENABLED(CONFIG_PCI_MSI)
	if (exynos_pcie->use_msi) {
		if (exynos_pcie->ep_device_type == EP_QC_WIFI ||
				exynos_pcie->ep_device_type == EP_BCM_WIFI ||
				exynos_pcie->ep_device_type == EP_SAMSUNG_WIFI) {
			dw_pcie_msi_init(pp);

			if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
				msi_addr_from_dt = get_msi_base(exynos_pcie->ch_num);

				pp->msi_data = msi_addr_from_dt;

				pr_info("%s: msi_addr_from_dt: 0x%llx, pp->msi_data: 0x%llx\n", __func__,
						msi_addr_from_dt, pp->msi_data);

				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
						lower_32_bits(pp->msi_data));
				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
						upper_32_bits(pp->msi_data));
			} else {
#ifdef MODIFY_MSI_ADDR
				/* Change MSI address to fit within a 32bit address boundary */
				free_page(pp->msi_data);

				pp->msi_data = (u64)kmalloc(4, GFP_DMA);
				msi_target = virt_to_phys((void *)pp->msi_data);

				if ((msi_target >> 32) != 0)
					dev_info(&pdev->dev,
							"MSI memory is allocated over 32bit boundary\n");

				/* program the msi_data */
				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
						(u32)(msi_target & 0xffffffff));
				exynos_pcie_rc_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4,
						(u32)(msi_target >> 32 & 0xffffffff));
#endif
			}
		}
	}
#endif

	if (pp->msi_domain) {
		msi_domain = pp->msi_domain;
		msi_domain_info = (struct msi_domain_info *)msi_domain->host_data;
		msi_domain_info->chip->irq_set_affinity = exynos_pcie_msi_set_affinity;

#if IS_ENABLED(CONFIG_PCI_MSI)
		msi_domain_info->ops->msi_finish = exynos_msi_setup_irq;
#endif
	}

	return 0;
}

static void exynos_pcie_rc_pcie_ops_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pci);
	struct exynos_pcie_ops *pcie_ops = &exynos_pcie->exynos_pcie_ops;
	struct device *dev = pci->dev;

	dev_info(dev, "Initialize PCIe function.\n");

	pcie_ops->poweron = exynos_pcie_rc_poweron;
	pcie_ops->poweroff = exynos_pcie_rc_poweroff;
	pcie_ops->rd_own_conf = exynos_pcie_rc_rd_own_conf;
	pcie_ops->wr_own_conf = exynos_pcie_rc_wr_own_conf;
	pcie_ops->rd_other_conf = exynos_pcie_rc_rd_other_conf;
	pcie_ops->wr_other_conf = exynos_pcie_rc_wr_other_conf;
}

static int exynos_pcie_rc_make_reg_tb(struct device *dev, struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct pcie_port *pp = &pci->pp;
	unsigned int pos, val, id;
	int i;

	/* initialize the reg table */
	for (i = 0; i < 48; i++) {
		exynos_pcie->pci_cap[i] = 0;
		exynos_pcie->pci_ext_cap[i] = 0;
	}

	exynos_pcie_rc_rd_own_conf(pp, PCI_CAPABILITY_LIST, 4, &val);
	pos = 0xFF & val;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		id = val & CAP_ID_MASK;
		exynos_pcie->pci_cap[id] = pos;
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		pos = (val & CAP_NEXT_OFFSET_MASK) >> 8;
		dev_dbg(dev, "Next Cap pointer : 0x%x\n", pos);
	}

	pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		exynos_pcie_rc_rd_own_conf(pp, pos, 4, &val);
		if (val == 0) {
			dev_info(dev, "we have no ext capabilities!\n");
			break;
		}
		id = PCI_EXT_CAP_ID(val);
		exynos_pcie->pci_ext_cap[id] = pos;
		pos = PCI_EXT_CAP_NEXT(val);
		dev_dbg(dev, "Next ext Cap pointer : 0x%x\n", pos);
	}

	for (i = 0; i < 48; i++) {
		if (exynos_pcie->pci_cap[i])
			dev_info(dev, "PCIe cap [0x%x][%s]: 0x%x\n", i, CAP_ID_NAME(i), exynos_pcie->pci_cap[i]);
	}
	for (i = 0; i < 48; i++) {
		if (exynos_pcie->pci_ext_cap[i])
			dev_info(dev, "PCIe ext cap [0x%x][%s]: 0x%x\n", i, EXT_CAP_ID_NAME(i), exynos_pcie->pci_ext_cap[i]);
	}
	return 0;
}
#if 0
u32 pcie_linkup_stat(void)
{
	struct exynos_pcie *exynos_pcie;

	for (i = 0; i < MAX_RC_NUM; i++) {
		exynos_pcie = &g_pcie[i];

		if (exynos_pcie->use_pcieon_sleep)
			if (exynos_pcie->pcie_is_linkup) {
				dev_info("[%s] pcie_is_linkup : %d\n",
						__func__, exynos_pcie->pcie_is_linkup);

				return exynos_pcie->pcie_is_linkup;
			}
	}
	pr_info("[%s] All PCIe channel  pcie_is_linkup: false\n",__func__);
	return exynos_pcie->pcie_is_linkup;
}
EXPORT_SYMBOL_GPL(pcie_linkup_stat);
#endif
static int exynos_pcie_rc_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct dw_pcie *pci;
	struct pcie_port *pp;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int ch_num;

	dev_info(&pdev->dev, "## PCIe RC PROBE start \n");

	if (create_pcie_sys_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie sys file\n");

	if (of_property_read_u32(np, "ch-num", &ch_num)) {
		dev_err(&pdev->dev, "Failed to parse the channel number\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "## PCIe ch %d ##\n", ch_num);

	pci = devm_kzalloc(&pdev->dev, sizeof(*pci), GFP_KERNEL);
	if (!pci) {
		dev_err(&pdev->dev, "dw_pcie allocation is failed\n");
		return -ENOMEM;
	}

	exynos_pcie = &g_pcie_rc[ch_num];
	exynos_pcie->pci = pci;

	pci->dev = &pdev->dev;
	pci->ops = &dw_pcie_ops;

	pp = &pci->pp;

	exynos_pcie->ep_pci_bus = NULL;
	exynos_pcie->ch_num = ch_num;
	exynos_pcie->l1ss_enable = 1;
	exynos_pcie->state = STATE_LINK_DOWN;

	exynos_pcie->linkdown_cnt = 0;
	exynos_pcie->boot_cnt = 0;
	exynos_pcie->work_l1ss_cnt = 0;
	exynos_pcie->l1ss_ctrl_id_state = 0;
	exynos_pcie->atu_ok = 0;
	exynos_pcie->cpl_timeout_recovery = 0;
	exynos_pcie->sudden_linkdown = 0;
	exynos_pcie->probe_done = 0;

	exynos_pcie->app_req_exit_l1 = PCIE_APP_REQ_EXIT_L1;
	exynos_pcie->app_req_exit_l1_mode = PCIE_APP_REQ_EXIT_L1_MODE;
	exynos_pcie->linkup_offset = PCIE_ELBI_RDLH_LINKUP;

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	platform_set_drvdata(pdev, exynos_pcie);

	/* parsing pcie dts data for exynos */
	ret = exynos_pcie_rc_parse_dt(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_pin_state(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_clock_get(pp);
	if (ret)
		goto probe_fail;

	ret = exynos_pcie_rc_get_resource(pdev, exynos_pcie);
	if (ret)
		goto probe_fail;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	spin_lock_init(&exynos_pcie->reg_lock);
	spin_lock_init(&exynos_pcie->conf_lock);
	spin_lock_init(&exynos_pcie->pcie_l1_exit_lock);
#if IS_ENABLED(CONFIG_EXYNOS_PCIE_GEN2_S2MPU)
	spin_lock_init(&s2mpu_lock);
#endif

	/* TCXO_FAR on */
	//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

	/* Mapping PHY functions */
	exynos_pcie_rc_phy_init(pp);

	exynos_pcie_rc_pcie_ops_init(pp);

	exynos_pcie_rc_resumed_phydown(pp);

	if (exynos_pcie->use_nclkoff_en)
		exynos_pcie_rc_nclkoff_ctrl(pdev, exynos_pcie);

	/* if it needed for msi init, property should be added on dt */
	//set_dma_ops(&pdev->dev, &exynos_pcie_dma_ops); //why need?
	//dev_info(&pdev->dev, "DMA opertaions are changed\n");

	ret = exynos_pcie_rc_make_reg_tb(&pdev->dev, exynos_pcie);
	if (ret)
		goto probe_fail;

	if (exynos_pcie->ep_device_type == EP_QC_WIFI) {
		save_pcie_rmem(&pdev->dev, exynos_pcie->ch_num);
	}
	ret = exynos_pcie_rc_add_port(pdev, pp);
	if (ret)
		goto probe_fail;

	if (exynos_pcie->use_cache_coherency)
		exynos_pcie_rc_set_iocc(pp, 1);

	disable_irq(pp->irq);

#if IS_ENABLED(CONFIG_CPU_IDLE)
	exynos_pcie->idle_ip_index =
			exynos_get_idle_ip_index(dev_name(&pdev->dev), 0);
	if (exynos_pcie->idle_ip_index < 0) {
		dev_err(&pdev->dev, "Cant get idle_ip_dex!!!\n");
		ret = -EINVAL;
		goto probe_fail;
	}

	exynos_update_ip_idle_status(exynos_pcie->idle_ip_index, PCIE_IS_IDLE);
	dev_info(&pdev->dev, "%s, ip idle status : %d, idle_ip_index: %d \n",
					__func__, PCIE_IS_IDLE,	exynos_pcie->idle_ip_index);

#ifdef PCIE_SICD /* Temporary remove: Need to enable to use sicd powermode */
	exynos_pcie->power_mode_nb.notifier_call = exynos_pcie_rc_power_mode_event;
	exynos_pcie->power_mode_nb.next = NULL;
	exynos_pcie->power_mode_nb.priority = 0;

	ret = exynos_cpupm_notifier_register(&exynos_pcie->power_mode_nb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register lpa notifier\n");
		goto probe_fail;
	}
#endif
#endif

	exynos_pcie->pcie_wq = create_freezable_workqueue("pcie_wq");
	if (IS_ERR(exynos_pcie->pcie_wq)) {
		dev_err(&pdev->dev, "couldn't create workqueue\n");
		ret = EBUSY;
		goto probe_fail;
	}

	INIT_DELAYED_WORK(&exynos_pcie->dislink_work,
			exynos_pcie_rc_dislink_work);

	INIT_DELAYED_WORK(&exynos_pcie->cpl_timeout_work,
			exynos_pcie_rc_cpl_timeout_work);

	if ((exynos_pcie->ep_device_type == EP_BCM_WIFI) ||
			(exynos_pcie->ep_device_type == EP_QC_WIFI)) {
		INIT_DELAYED_WORK(&exynos_pcie->l1ss_boot_delay_work,
				exynos_pcie_work_l1ss);
	}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	exynos_pcie->itmon_nb.notifier_call = exynos_pcie_rc_itmon_notifier;
	itmon_notifier_chain_register(&exynos_pcie->itmon_nb);
#endif

#if 0
	if (exynos_pcie->use_pcieon_sleep) {
		dev_info(&pdev->dev, "## register pcie connection function\n");
		register_pcie_is_connect(pcie_linkup_stat);//tmp
	}
#endif

	/* TCXO_FAR on */
	//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

	/* force set input clk path change to disable */
	if (exynos_pcie->phy_ops.phy_input_clk_change != NULL) {
		exynos_pcie->phy_ops.phy_input_clk_change(exynos_pcie, 0);
	}

	/* TCXO_FAR off */
	//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 0);

	platform_set_drvdata(pdev, exynos_pcie);

probe_fail:
	pm_runtime_put_sync(&pdev->dev);


	if (ret)
		dev_err(&pdev->dev, "## %s: PCIe probe failed\n", __func__);
	else {
		exynos_pcie->probe_done = 1;
		dev_info(&pdev->dev, "## %s: PCIe probe success\n", __func__);
	}
	return ret;
}

static int __exit exynos_pcie_rc_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int exynos_pcie_rc_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(dev, "## SUSPEND[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "%s: RC%d already off\n", __func__, exynos_pcie->ch_num);
		pm_runtime_put_sync(pci->dev);
		return 0;
	}

	if (exynos_pcie->ch_num == 0) { //PCIE GEN2 channel
		pr_err("WARNNING - Unexpected PCIe state(try to power OFF)\n");
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
	}

	return 0;
}

static int exynos_pcie_rc_resume_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(dev, "## RESUME[%s]: %s(pcie_is_linkup: %d)\n", __func__,
			EXUNOS_PCIE_STATE_NAME(exynos_pcie->state),
			exynos_pcie->pcie_is_linkup);

	if (!exynos_pcie->pci_dev) {
		dev_err(pci->dev, "Failed to get pci device\n");
	} else {
		exynos_pcie->pci_dev->state_saved = false;
		dev_dbg(&(exynos_pcie->pci_dev->dev), "state_saved flag = false");
	}

	pm_runtime_get_sync(pci->dev);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "%s: RC%d Link down state-> phypwr off\n", __func__,
							exynos_pcie->ch_num);

		/* TCXO_FAR on */
		//hsi_tcxo_far_control(exynos_pcie->ch_num + 1, 1);

		exynos_pcie_rc_resumed_phydown(&pci->pp);
	}

	return 0;
}

static int exynos_pcie_suspend_prepare(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	dev_info(dev, "%s - PCIe(HSI) block enable to prepare suspend.\n", __func__);
	pm_runtime_get_sync(pci->dev);

	return 0;
}

static void exynos_pcie_resume_complete(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct dw_pcie *pci = exynos_pcie->pci;

	pm_runtime_put_sync(pci->dev);
}

static int exynos_pcie_runtime_suspend(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	/* DO PHY Isolation */
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 0);

	return 0;
}

static int exynos_pcie_runtime_resume(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	/* DO PHY ByPass */
	regmap_update_bits(exynos_pcie->pmureg,
			   exynos_pcie->pmu_offset,
			   PCIE_PHY_CONTROL_MASK, 1);

	return 0;
}
#endif

static const struct dev_pm_ops exynos_pcie_rc_pm_ops = {
	.suspend_noirq	= exynos_pcie_rc_suspend_noirq,
	.resume_noirq	= exynos_pcie_rc_resume_noirq,
	.prepare	= exynos_pcie_suspend_prepare,
	.complete	= exynos_pcie_resume_complete,
	SET_RUNTIME_PM_OPS(exynos_pcie_runtime_suspend,
			   exynos_pcie_runtime_resume, NULL)
};

static const struct of_device_id exynos_pcie_rc_of_match[] = {
	{ .compatible = "samsung,exynos-pcie-rc", },
	{},
};

static struct platform_driver exynos_pcie_rc_driver = {
	.probe		= exynos_pcie_rc_probe,
	.remove		= exynos_pcie_rc_remove,
	.driver = {
		.name		= "exynos-pcie-rc",
		.owner		= THIS_MODULE,
		.of_match_table = exynos_pcie_rc_of_match,
		.pm		= &exynos_pcie_rc_pm_ops,
	},
};
module_platform_driver(exynos_pcie_rc_driver);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_AUTHOR("Seungo Jang <seungo.jang@samsung.com>");
MODULE_AUTHOR("Kwangho Kim <kwangho2.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe RootComplex controller driver");
MODULE_LICENSE("GPL v2");
