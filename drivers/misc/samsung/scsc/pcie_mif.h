/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

#ifndef __PCIE_MIF_H
#define __PCIE_MIF_H
#include <linux/pci.h>
#include "scsc_mif_abs.h"

#define BAR1_MAPPING_OFFSET     (0x10000000)

#define MAILBOX_WLBT_BASE_WL    ((0x159E0000 -  BAR1_MAPPING_OFFSET))
#define MAILBOX_WLBT_BASE_WP    ((0x14CB0000 -  BAR1_MAPPING_OFFSET))
#define MAILBOX_WLBT_WL_REG(r)  (MAILBOX_WLBT_BASE_WL + (r))
#define MCUCTRL                 0x000 /* MCU Controller Register */
#define IS_VERSION              0x004 /* Version Information Register */
/* R0 [31:16]  - Int FROM R4/M4 */
#define INTGR0                  0x008 /* Interrupt Generation Register 0 (r/w) */
#define INTCR0                  0x00C /* Interrupt Clear Register 0 (w) */
#define INTMR0                  0x010 /* Interrupt Mask Register 0 (r/w) */
#define INTSR0                  0x014 /* Interrupt Status Register 0 (r) */
#define INTMSR0                 0x018 /* Interrupt Mask Status Register 0 (r) */
/* R1 [15:0]  - Int TO R4 */
#define INTGR1                  0x01c /* Interrupt Generation Register 1 */
#define INTCR1                  0x020 /* Interrupt Clear Register 1 */
#define INTMR1                  0x024 /* Interrupt Mask Register 1 */
#define INTSR1                  0x028 /* Interrupt Status Register 1 */
#define INTMSR1                 0x02c /* Interrupt Mask Status Register 1 */
/* R2 [15:0]  - Int TO M4 */
#define INTGR2                  0x030 /* Interrupt Generation Register 2 */
#define INTCR2                  0x034 /* Interrupt Clear Register 2 */
#define INTMR2                  0x038 /* Interrupt Mask Register 2 */
#define INTSR2                  0x03c /* Interrupt Status Register 2 */
#define INTMSR2                 0x040 /* Interrupt Mask Status Register 2 */
#define MIF_INIT                0x04c /* MIF_init */
#define ISSR_BASE               0x100 /* IS_Shared_Register Base address */
#define ISSR(r)                 (ISSR_BASE + (4 * (r)))
#define SEMAPHORE_BASE          0x180 /* IS_Shared_Register Base address */
#define SEMAPHORE(r)            (SEMAPHORE_BASE + (4 * (r)))
#define SEMA0CON                0x1c0
#define SEMA0STATE              0x1c8

struct scsc_mif_abs *pcie_mif_create(struct pci_dev *pdev, const struct pci_device_id *id);
void pcie_mif_destroy_pcie(struct pci_dev *pdev, struct scsc_mif_abs *interface);
struct pci_dev      *pcie_mif_get_pci_dev(struct scsc_mif_abs *interface);
struct device       *pcie_mif_get_dev(struct scsc_mif_abs *interface);

struct pcie_mif;

void * pcie_mif_get_mem(struct pcie_mif *pcie);

typedef struct _oATU {
        u64 start_phy;
	u64 end_phy;
} ATU_info;

#endif
