/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CPIF IOMMU header
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#ifndef _EXYNOS_CPIF_IOMMU_H
#define _EXYNOS_CPIF_IOMMU_H

#if IS_ENABLED(CONFIG_EXYNOS_CPIF_IOMMU)
void cpif_sysmmu_set_use_iocc(void);
void cpif_sysmmu_enable(void);
void cpif_sysmmu_disable(void);
void cpif_sysmmu_all_buff_free(void);
void print_cpif_sysmmu_tlb(void);
int cpif_iommu_map(unsigned long iova, phys_addr_t paddr, size_t size, int prot);
size_t cpif_iommu_unmap(unsigned long iova, size_t size);
#else
static inline void cpif_sysmmu_set_use_iocc(void) { return; }
static inline void cpif_sysmmu_enable(void) { return; }
static inline void cpif_sysmmu_disable(void) { return; }
static inline void cpif_sysmmu_all_buff_free(void) { return; }
static inline void print_cpif_sysmmu_tlb(void) { return; }
static inline int cpif_iommu_map(unsigned long iova, phys_addr_t paddr,
				size_t size, int prot) { return 0; }
static inline size_t cpif_iommu_unmap(unsigned long iova, size_t size) { return 0; }
#endif

#endif /* _EXYNOS_CPIF_IOMMU_H */
