/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __PCIE_S5E9925_H
#define __PCIE_S5E9925_H
#include "scsc_mif_abs.h"
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/resource.h>

struct pcie_mif;

/* PCIE interface */
struct pcie_mif *pcie_get_instance(void);
void pcie_register_driver(void);
int pcie_reset(struct pcie_mif *pcie, bool reset);
void pcie_set_mem_range(struct pcie_mif *pcie, unsigned long mem_start, size_t mem_sz);
void pcie_irq_reg_pmu_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_wlan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_irq_reg_wpan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev);
void pcie_remap_set(struct pcie_mif *pcie, uintptr_t remap_addr, enum scsc_mif_abs_target target);
int pcie_get_mbox_pmu(struct pcie_mif *pcie);
int pcie_set_mbox_pmu(struct pcie_mif *pcie, u32 val);
int pcie_load_pmu_fw(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len);
void *pcie_map(struct pcie_mif *pcie, size_t *allocated);
int pci_get_mifram_ref(struct pcie_mif *pcie, void *ptr, scsc_mifram_ref *ref);
void *pcie_get_mifram_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref);
bool pcie_is_initialized(void);
void pcie_irq_bit_set(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
u32 pcie_irq_get(struct pcie_mif *pcie, enum scsc_mif_abs_target target);
void pcie_irq_bit_clear(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_irq_bit_mask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_irq_bit_unmask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target);
void pcie_get_msi_range(struct pcie_mif *pcie, u8 *start, u8 *end, enum scsc_mif_abs_target target);

#endif
