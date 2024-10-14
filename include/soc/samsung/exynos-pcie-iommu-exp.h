/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * PCIe Exynos IOMMU driver header file
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 */

#ifndef _EXYNOS_PCIE_IOMMU_EXP_H_
#define _EXYNOS_PCIE_IOMMU_EXP_H_

#include <linux/types.h>

#if IS_ENABLED(CONFIG_EXYNOS_PCIE_IOMMU)
int pcie_iommu_map(unsigned long iova, phys_addr_t paddr, size_t size,
		   int prot, int ch_num);
size_t pcie_iommu_unmap(unsigned long iova, size_t size, int ch_num);
void pcie_sysmmu_enable(int ch_num);
void pcie_sysmmu_disable(int ch_num);
void pcie_sysmmu_set_use_iocc(int ch_num);
void pcie_sysmmu_all_buff_free(int ch_num);
#else
static void __maybe_unused pcie_sysmmu_enable(int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
static void __maybe_unused pcie_sysmmu_disable(int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
static size_t __maybe_unused pcie_iommu_map(unsigned long iova, phys_addr_t paddr,
		size_t size, int prot, int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");

	return -ENODEV;
}
static size_t __maybe_unused pcie_iommu_unmap(unsigned long iova, size_t size,
		int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");

	return -ENODEV;
}
static void __maybe_unused pcie_sysmmu_set_use_iocc(int ch_num)
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
static void __maybe_unused pcie_sysmmu_all_buff_free(int ch_num);
{
	pr_err("PCIe SysMMU is NOT Enabled!!!\n");
}
#endif
#endif /* _EXYNOS_PCIE_IOMMU_EXP_H_ */
