/****************************************************************************
*
* Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

/* Implements */

#include "pcie_mif.h"

/* Uses */
#include <linux/pfn.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/moduleparam.h>
#include <asm/barrier.h>
#include <scsc/scsc_logring.h>
#include "pcie_mif_module.h"
#include "pcie_proc.h"
#include "pcie_mbox.h"
#include "pmu_paean.h"
#include "pmu_patch_paean.h"
#include "mif_reg_paean_pcie.h"
#include "mifproc.h"
#include <linux/delay.h>

#define PCIE_MIF_RESET_REQUEST_SOURCE 31

struct pcie_mif *g_pcie;
struct scsc_mif_abs *g_if;
bool global_enable;

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

static void *pcie_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
static int pcie_mif_reset(struct scsc_mif_abs *interface, bool reset);
static int pcie_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val);

/* Module parameters */

static void pcie_mif_unmap(struct scsc_mif_abs *interface, void *mem);

static bool enable_pcie_mif_arm_reset = true;
module_param(enable_pcie_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_mif_arm_reset, "Enables ARM cores reset");

#define NUM_TO_HOST_IRQ_PMU		1
#define NUM_TO_HOST_IRQ_WLAN		16
#define NUM_TO_HOST_IRQ_WPAN		10
#define NUM_TO_HOST_IRQ_PMU_START	0
#define NUM_TO_HOST_IRQ_PMU_END		((NUM_TO_HOST_IRQ_PMU_START + NUM_TO_HOST_IRQ_PMU) - 1)
#define NUM_TO_HOST_IRQ_WLAN_START	(NUM_TO_HOST_IRQ_PMU_END + 1)
#define NUM_TO_HOST_IRQ_WLAN_END	((NUM_TO_HOST_IRQ_WLAN_START + NUM_TO_HOST_IRQ_WLAN) - 1)
#define NUM_TO_HOST_IRQ_WPAN_START	(NUM_TO_HOST_IRQ_WLAN_END + 1)
#define NUM_TO_HOST_IRQ_WPAN_END	((NUM_TO_HOST_IRQ_WPAN_START + NUM_TO_HOST_IRQ_WPAN) - 1)
#define NUM_TO_HOST_IRQ_TOTAL		(NUM_TO_HOST_IRQ_PMU + NUM_TO_HOST_IRQ_WLAN + NUM_TO_HOST_IRQ_WPAN)

/* Types */
struct msi_emul {
	u8 msi_bit;
	u32 msi_vec;
	bool is_masked;
};

struct pcie_mif {
	struct scsc_mif_abs interface;
	struct pci_dev *pdev;
	int dma_using_dac; /* =1 if 64-bit DMA is used, =0 otherwise. */
	__iomem void *registers_pert;
	__iomem void *registers_ramrp;
	__iomem void *registers_msix;

	struct device *dev;

	bool in_reset;

	void *mem; /* DMA memory mapped to PCIe space for MX-AP comms */
	struct pcie_mbox mbox; /* mailbox emulation */
	size_t mem_allocated;
	dma_addr_t dma_addr;

	/* Callback function and dev pointer mif_intr manager handler */
	void (*wlan_handler)(int irq, void *data);
	void (*wpan_handler)(int irq, void *data);
	void *irq_dev;
	/* spinlock to serialize driver access */
	spinlock_t mif_spinlock;
	spinlock_t mask_spinlock;
	/* Reset Request handler and context */
	void (*reset_request_handler)(int irq_num_ignored, void *data);
	void *reset_request_handler_data;

	enum wlbt_boot_state {
		WLBT_BOOT_IN_RESET = 0,
		WLBT_BOOT_WAIT_CFG_REQ,
		WLBT_BOOT_ACK_CFG_REQ,
		WLBT_BOOT_CFG_DONE,
		WLBT_BOOT_CFG_ERROR
	} boot_state;

	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void (*pmu_handler)(int irq, void *data);
	void *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

	struct msi_emul	pmu_msi[NUM_TO_HOST_IRQ_PMU];
	struct msi_emul	wlan_msi[NUM_TO_HOST_IRQ_WLAN];
	struct msi_emul	wpan_msi[NUM_TO_HOST_IRQ_WPAN];
	u32 rcv_irq_wlan;
	u32 rcv_irq_wpan;
};

/* Private Macros */

/** Upcast from interface member to pcie_mif */
#define pcie_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct pcie_mif, interface)

static inline void pcie_mif_reg_write(struct pcie_mif *pcie, u32 offset, u32 value)
{
	writel(value, pcie->registers_ramrp + offset);
}

static inline u32 pcie_mif_reg_read(struct pcie_mif *pcie, u32 offset)
{
	return readl(pcie->registers_ramrp + offset);
}

static void pcie_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;
}

static void pcie_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PCIE_MIF, NULL, "INT reset_request handler not registered\n");
}

irqreturn_t pcie_mif_pmu_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "INT received, boot_state = %u\n", pcie->boot_state);

	/* pcie->boot_state = WLBT_BOOT_CFG_DONE; */
	if (pcie->pmu_handler != pcie_mif_irq_default_handler)
		pcie->pmu_handler(irq, pcie->irq_dev_pmu);
	else
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "MIF PMU Interrupt Handler not registered\n");

	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_isr_wlan(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	u8 i;
	unsigned long flags;

	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	/* Set the rcv_irq before calling the handler! */
	pcie->rcv_irq_wlan = 0;
	/* get MSI bit correspin to line */
	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++) {
		if (pcie->wlan_msi[i].msi_vec == irq)
			pcie->rcv_irq_wlan = pcie->wlan_msi[i].msi_bit;
	}

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IN WLAN ISR!!!!!!!!!!!!!!!!! line %d msi_bit %u cpu %d", irq, pcie->rcv_irq_wlan, smp_processor_id());
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wlan_handler != pcie_mif_irq_default_handler) {
		pcie->wlan_handler(irq, pcie->irq_dev);
	}
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");

	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	return IRQ_HANDLED;
}

irqreturn_t pcie_mif_isr_wpan(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	u8 i;
	unsigned long flags;

	spin_lock_irqsave(&pcie->mif_spinlock, flags);

	/* Set the rcv_irq before calling the handler! */
	pcie->rcv_irq_wlan = 0;
	/* get MSI bit correspin to line */
	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		if (pcie->wpan_msi[i].msi_vec == irq)
			pcie->rcv_irq_wpan = pcie->wpan_msi[i].msi_bit;
	}

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IN WPAN ISR!!!!!!!!!!!!!!!!! line %d msi_bit %u", irq, pcie->rcv_irq_wpan);
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wpan_handler != pcie_mif_irq_default_handler)
		pcie->wpan_handler(irq, pcie->irq_dev);
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");

	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	return IRQ_HANDLED;
}

static void pcie_mif_destroy(struct scsc_mif_abs *interface)
{
}

static char *pcie_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	/* TODO */
	/* return "0" for the time being */
	return "0";
}

static void pcie_set_atu(struct pcie_mif *pcie)
{
	pcie_atu_config_t atu;

	/* Send ATU mappings */
	atu.start_addr_0 = pcie->dma_addr;
	atu.end_addr_0 = pcie->dma_addr + PCIE_MIF_PREALLOC_MEM;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Copy ATU offset 0x%x start 0x%lx end 0x%lx\n", RAMRP_HOSTIF_PMU_BUF_PTR,
			  atu.start_addr_0, atu.end_addr_0);
	memcpy_toio(pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR, &atu, sizeof(pcie_atu_config_t));
	msleep(1);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write PMU_HOSTIF_MBOX_REQ_PCIE_ATU_CONFIG at offset %x\n",
			  RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	writel(PMU_HOSTIF_MBOX_REQ_PCIE_ATU_CONFIG, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	msleep(1);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x2: PMU_INTERRUPT_FROM_HOST_INTGR\n");
	writel(0x2, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	msleep(1);
}

static void pcie_set_remappers(struct pcie_mif *pcie)
{
	u32 cmd_dw;

	/* Set remmapers */
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU0_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU0_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_UCPU0_RMP_BOOT_ADDR 0x%x\n", (pcie->remap_addr_wlan >> 12));
	writel((pcie->remap_addr_wlan >> 12), pcie->registers_pert + PMU_WLAN_UCPU0_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU0_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU0_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU1_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU1_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_UCPU1_RMP_BOOT_ADDR 0x%x\n", (pcie->remap_addr_wlan >> 12));
	writel((pcie->remap_addr_wlan >> 12), pcie->registers_pert + PMU_WLAN_UCPU1_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU1_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU1_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

	cmd_dw = readl(pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", (pcie->remap_addr_wpan >> 12));
	writel((pcie->remap_addr_wpan >> 12), pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WPAN_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WPAN_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

}

static void pcie_load_pmu(struct pcie_mif *pcie)
{
	uint32_t *ka_patch_addr;
	size_t ka_patch_len;
	u32 cmd_dw;

	if (pcie->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch present in FW image\n");
		ka_patch_addr = pcie->ka_patch_fw;
		ka_patch_len = pcie->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n",
				  ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_len = ARRAY_SIZE(ka_patch) * sizeof(uint32_t);
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch download start\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	memcpy_toio(pcie->registers_ramrp + PMU_BOOT_RAM_START_ADDR,
		    ka_patch_addr, ka_patch_len);

	msleep(100);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch done\n");

	wmb();

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "End GFG\n");
	writel(SECOND_STAGE_BOOT_ADDR, pcie->registers_pert + PMU_SECOND_STAGE_BOOT_ADDR);
	msleep(100);
	cmd_dw = readl(pcie->registers_pert + PMU_SECOND_STAGE_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PMU_SECOND_STAGE_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x1: PMU_INTERRUPT_FROM_HOST_INTGR\n");
	writel(0x1, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	msleep(100);

	pcie_set_atu(pcie);
	pcie_set_remappers(pcie);

	pcie->boot_state = WLBT_BOOT_CFG_DONE;
}

static bool already_done;

static int pcie_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	if (enable_pcie_mif_arm_reset || !reset) {
		/* Sanity check */
		if (!reset && pcie->in_reset == true) {
			if (already_done == false) {
				pcie_load_pmu(pcie);
				already_done = true;
			} else {
				pcie_set_atu(pcie);
				pcie_set_remappers(pcie);
			}
			pcie->in_reset = false;
		} else if (pcie->in_reset == false) {
			pcie->in_reset = true;
		}
	} else
		SCSC_TAG_INFO(PCIE_MIF, "Not resetting ARM Cores enable_pcie_mif_arm_reset: %d\n",
			      enable_pcie_mif_arm_reset);
	return 0;
}

static void *pcie_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	size_t map_len = PCIE_MIF_ALLOC_MEM;

	if (pcie->mem_allocated) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Do not allocate again\n");
		*allocated = map_len;
		return pcie->mem;
	}

	if (allocated)
		*allocated = 0;

	pcie->mem_allocated = 0;
	/* should return PAGE_ALIGN Memory */
	SCSC_TAG_ERR(PCIE_MIF, "Allocate %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
	pcie->mem = dma_alloc_coherent(pcie->dev, PCIE_MIF_PREALLOC_MEM, &pcie->dma_addr, GFP_KERNEL);
	if (pcie->mem == NULL) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
		return 0;
	}
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Allocated dma coherent vmem: %x phy addr %x\n", pcie->mem,
			  (void *)pcie->dma_addr);

	/* Return the max allocatable memory on this abs. implementation */
	if (allocated)
		*allocated = map_len;

	pcie->mem_allocated = map_len;
	return pcie->mem;
}

/* HERE: Not sure why mem is passed in - its stored in pcie - as it should be */
static void pcie_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Do not unmap.. but memset to 0\n");
	memset(pcie->mem, 0, PCIE_MIF_PREALLOC_MEM);
	return;

	/* Avoid unused parameter error */
	(void)mem;

	if (pcie->mem_allocated) {
		pcie->mem_allocated = 0;
		dma_free_coherent(pcie->dev, PCIE_MIF_PREALLOC_MEM, pcie->mem, pcie->dma_addr);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Freed dma coherent mem: %x addr %x\n", pcie->mem,
				  (void *)pcie->dma_addr);
	}
}

static u32 pcie_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	u32 val = 0;

	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Mask status get not implemented\n");

	return val;
}

static u32 pcie_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	u32 val = 0;
	u32 bit;

	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		bit = pcie->rcv_irq_wlan;
	} else {
		bit = pcie->rcv_irq_wpan;
	}

	val = 1 << bit;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Bit %u get val 0x%x\n", bit, val);
	return val;
}

static void pcie_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Bit Clear %u subsystem %d\n", bit_num, target);
#if 0 /* not yet */
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		if (pcie->wlan_msi[bit_num].is_masked == false) {
			enable_irq(pcie->wlan_msi[bit_num].msi_vec);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Enable IRQ %d\n", pcie->wlan_msi[bit_num].msi_vec);
		}
	} else if (pcie->wpan_msi[bit_num].is_masked == false) {
			enable_irq(pcie->wpan_msi[bit_num].msi_vec);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Enable IRQ %d\n", pcie->wpan_msi[bit_num].msi_vec);
	}
#endif
	return;
}

static void pcie_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	unsigned long flags;
	unsigned int logic_bit_num;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num  >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = true;
		disable_irq_nosync(pcie->wlan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num  >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = true;
		disable_irq_nosync(pcie->wpan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target, pcie->wpan_msi[logic_bit_num].msi_vec);
	}
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
	return;
}

static void pcie_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	unsigned long flags;
	unsigned int logic_bit_num;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num  >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (!pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d. Already enabled ignore request\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = false;
		enable_irq(pcie->wlan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num  >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (!pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d. Already enabled ignore request\n", bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = false;
		enable_irq(pcie->wpan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target, pcie->wpan_msi[logic_bit_num].msi_vec);
	}
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);

	return;
}

static void pcie_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		pcie->remap_addr_wlan = remap_addr + DRAM_BASE_ADDR;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		pcie->remap_addr_wpan = remap_addr + DRAM_BASE_ADDR;
	else
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect target %d\n", target);
}

static void pcie_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	u32 target_off = WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INT from host: bit %d on target %d\n", bit_num, target);

	if (bit_num >= NUM_TO_HOST_IRQ_TOTAL) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		target_off = WLAN_INTERRUPT_FROM_HOST_PENDING_BIT_ARRAY_SET;
	else
		target_off = WPAN_INTERRUPT_FROM_HOST_INTGR;

	writel(1 << bit_num, pcie->registers_pert + target_off);
}

static void pcie_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wlan_handler = handler;
	pcie->irq_dev = dev;
}

static void pcie_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wlan_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;
}

static void pcie_mif_get_msi_range(struct scsc_mif_abs *interface, u8 *start, u8 *end, enum scsc_mif_abs_target target)
{
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		*start = NUM_TO_HOST_IRQ_WLAN_START;
		*end = NUM_TO_HOST_IRQ_WLAN_END;
	} else {
		*start = NUM_TO_HOST_IRQ_WPAN_START;
		*end = NUM_TO_HOST_IRQ_WPAN_END;
	}
}

static void pcie_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					  void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wpan_handler = handler;
	pcie->irq_dev = dev;
}

static void pcie_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wpan_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;
}

static void pcie_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
						   void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->reset_request_handler = handler;
	pcie->reset_request_handler_data = dev;

}

static void pcie_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	pcie->reset_request_handler = pcie_mif_irq_reset_request_default_handler;
	pcie->reset_request_handler = NULL;
}

static u32 *pcie_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	u32 *addr;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "mbox_index 0x%x\n", mbox_index);
	addr = pcie->registers_ramrp + MAILBOX_WLBT_WL_REG(ISSR(mbox_index));

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "bar1 pcie->registers_ramrp %x offset %x addr %x\n",
			  pcie->registers_ramrp, MAILBOX_WLBT_WL_REG(ISSR(mbox_index)), addr);
	return addr;
}

static int pcie_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	u32 val = 0;

	val = readl(pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

static int pcie_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write CMD %d at offset 0x%x\n",
			  val, RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	writel(val, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	msleep(1);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x2: PMU_INTERRUPT_FROM_HOST_INTGR\n");
	writel(0x2, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	msleep(1);
	return 0;
}

static int pcie_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	if (pcie->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "ka_patch already allocated\n");
		devm_kfree(pcie->dev, pcie->ka_patch_fw);
	}

	pcie->ka_patch_fw = devm_kzalloc(pcie->dev, ka_patch_len, GFP_KERNEL);

	if (!pcie->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PCIE_MIF, pcie->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(pcie->ka_patch_fw, ka_patch, ka_patch_len);
	pcie->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}

static void pcie_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data),
					 void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, pcie,
			  interface);
	pcie->pmu_handler = handler;
	pcie->irq_dev_pmu = dev;
}

static int pcie_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	if (ptr > (pcie->mem + PCIE_MIF_PREALLOC_MEM)) {
		SCSC_TAG_ERR(PCIE_MIF, "ooops limits reached\n");
		return -ENOMEM;
	}

	/* Ref is byte offset wrt start of shared memory */
	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)pcie->mem);

	return 0;
}

static void *pcie_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "\n");

	/* Check limits */
	if (ref >= 0 && ref < PCIE_MIF_ALLOC_MEM)
		return (void *)((uintptr_t)pcie->mem + (uintptr_t)ref);
	else
		return NULL;
}

static uintptr_t pcie_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	return virt_to_phys(pcie->mem) >> PAGE_SHIFT;
}

static struct device *pcie_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	return pcie->dev;
}

static void pcie_mif_irq_clear(void)
{
}

static void pcie_mif_dump_register(struct scsc_mif_abs *interface)
{
}

static void __iomem *pcie_mif_map_bar(struct pci_dev *pdev, u8 index)
{
	void __iomem *vaddr;
	dma_addr_t busaddr;
	size_t len;
	int ret;

	ret = pcim_iomap_regions(pdev, 1 << index, DRV_NAME);
	if (ret)
		return IOMEM_ERR_PTR(ret);

	busaddr = pci_resource_start(pdev, index);
	len = pci_resource_len(pdev, index);
	vaddr = pcim_iomap_table(pdev)[index];
	if (!vaddr)
		return IOMEM_ERR_PTR(-ENOMEM);

	SCSC_TAG_ERR_DEV(PCIE_MIF, &pdev->dev, "BAR%u vaddr=0x%p busaddr=%pad len=%u\n", index, vaddr, &busaddr,
			 (int)len);

	return vaddr;
}

static int pcie_mif_enable_msi(struct pcie_mif *pcie)
{
	struct msi_desc *msi_desc;
	int num_vectors;
	int ret;
	u32 acc = 0, i;

	num_vectors = pci_alloc_irq_vectors(pcie->pdev, 1, NUM_TO_HOST_IRQ_TOTAL, PCI_IRQ_MSI);
	if (num_vectors != NUM_TO_HOST_IRQ_TOTAL) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to get %d MSI vectors, only %d available\n", NUM_TO_HOST_IRQ_TOTAL,
				 num_vectors);

		if (num_vectors >= 0)
			return -EINVAL;
		else
			return num_vectors;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "vectors %d\n", num_vectors);

	for (i = 0; i < NUM_TO_HOST_IRQ_PMU; i++, acc++) {
		pcie->pmu_msi[i].msi_vec = pci_irq_vector(pcie->pdev, 0);
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for PMU\n", acc, pcie->pmu_msi[i].msi_vec);
		ret = request_irq(pcie->pmu_msi[i].msi_vec, pcie_mif_pmu_isr, 0, DRV_NAME, pcie);
		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_TO_HOST_IRQ_WLAN; i++, acc++) {
		pcie->wlan_msi[i].msi_vec = pci_irq_vector(pcie->pdev, acc);
		pcie->wlan_msi[i].msi_bit = acc;
		pcie->wlan_msi[i].is_masked = false;
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WLAN\n", acc, pcie->wlan_msi[i].msi_vec);
		ret = request_irq(pcie->wlan_msi[i].msi_vec, pcie_mif_isr_wlan, 0, DRV_NAME, pcie);
		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++, acc++) {
		pcie->wpan_msi[i].msi_vec = pci_irq_vector(pcie->pdev, acc);
		pcie->wpan_msi[i].msi_bit = acc;
		pcie->wpan_msi[i].is_masked = false;
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WPAN\n", acc, pcie->wpan_msi[i].msi_vec);
		ret = request_irq(pcie->wpan_msi[i].msi_vec, pcie_mif_isr_wpan, 0, DRV_NAME, pcie);
		if (ret) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to register MSI handler. Aborting.\n");
			return -EINVAL;
		}
	}

	msi_desc = irq_get_msi_desc(pcie->pdev->irq);
	if (!msi_desc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "msi_desc is NULL\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "msi base data is %d\n", msi_desc->msg.data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(pcie->pdev);

	return ret;
}

struct scsc_mif_abs *pcie_mif_create(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	struct scsc_mif_abs *pcie_if;
	struct pcie_mif *pcie = (struct pcie_mif *)devm_kzalloc(&pdev->dev, sizeof(struct pcie_mif), GFP_KERNEL);
	u16 cmd;

	/* Avoid unused parameter error */
	(void)id;

	SCSC_TAG_ERR(PCIE_MIF, "Paean. Hello World !!!\n");

	if (!pcie) {
		SCSC_TAG_ERR(PCIE_MIF, "PCIE is null!\n");
		return NULL;
	}

	pcie_if = &pcie->interface;

	g_if = pcie_if;
	/* initialise interface structure */
	pcie_if->destroy = pcie_mif_destroy;
	pcie_if->get_uid = pcie_mif_get_uid;
	pcie_if->reset = pcie_mif_reset;
	pcie_if->map = pcie_mif_map;
	pcie_if->unmap = pcie_mif_unmap;
	pcie_if->irq_bit_set = pcie_mif_irq_bit_set;
	pcie_if->irq_get = pcie_mif_irq_get;
	pcie_if->irq_bit_mask_status_get = pcie_mif_irq_bit_mask_status_get;
	pcie_if->irq_bit_clear = pcie_mif_irq_bit_clear;
	pcie_if->irq_bit_mask = pcie_mif_irq_bit_mask;
	pcie_if->irq_bit_unmask = pcie_mif_irq_bit_unmask;
	pcie_if->irq_reg_handler = pcie_mif_irq_reg_handler;
	pcie_if->irq_unreg_handler = pcie_mif_irq_unreg_handler;
	pcie_if->get_msi_range = pcie_mif_get_msi_range;
	pcie_if->remap_set = pcie_mif_remap_set;
	pcie_if->irq_reg_handler_wpan = pcie_mif_irq_reg_handler_wpan;
	pcie_if->irq_unreg_handler_wpan = pcie_mif_irq_unreg_handler_wpan;
	pcie_if->irq_reg_reset_request_handler = pcie_mif_irq_reg_reset_request_handler;
	pcie_if->irq_unreg_reset_request_handler = pcie_mif_irq_unreg_reset_request_handler;
	pcie_if->get_mbox_ptr = pcie_mif_get_mbox_ptr;
	pcie_if->get_mifram_ptr = pcie_mif_get_mifram_ptr;
	pcie_if->get_mifram_ref = pcie_mif_get_mifram_ref;
	pcie_if->get_mifram_pfn = pcie_mif_get_mif_pfn;
	pcie_if->get_mif_device = pcie_mif_get_mif_device;
	pcie_if->irq_clear = pcie_mif_irq_clear;
	pcie_if->mif_dump_registers = pcie_mif_dump_register;
	pcie_if->mif_read_register = NULL;
	/* Suspend/resume not supported in PCIe MIF */
	pcie_if->suspend_reg_handler = NULL;
	pcie_if->suspend_unreg_handler = NULL;

	pcie_if->get_mbox_pmu = pcie_mif_get_mbox_pmu;
	pcie_if->set_mbox_pmu = pcie_mif_set_mbox_pmu;
	pcie_if->load_pmu_fw = pcie_load_pmu_fw;
	pcie_if->irq_reg_pmu_handler = pcie_mif_irq_reg_pmu_handler;
	pcie->pmu_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev_pmu = NULL;

	/* Update state */
	pcie->pdev = pdev;

	pcie->dev = &pdev->dev;

	pcie->wlan_handler = pcie_mif_irq_default_handler;
	pcie->wpan_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;

	/* Just do whats is necessary to meet the pci probe
	 * -BAR0 stuff
	 * -Interrupt (will be able to handle interrupts?)
	 */

	/* My stuff */
	pci_set_drvdata(pdev, pcie);

	rc = pcim_enable_device(pdev);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Error enabling device.\n");
		return NULL;
	}

	/* This function returns the flags associated with this resource.*/
	/* esource flags are used to define some features of the individual resource.
	 *      For PCI resources associated with PCI I/O regions, the information is extracted from the base address registers */
	/* IORESOURCE_MEM = If the associated I/O region exists, one and only one of these flags is set */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		SCSC_TAG_ERR(PCIE_MIF, "Incorrect BAR configuration\n");
		return NULL;
	}

	pci_set_master(pdev);

	/* Access iomap allocation table */
	/* return __iomem * const * */
	pcie->registers_pert = pcie_mif_map_bar(pdev, 0);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR0/1 registers %x\n", pcie->registers_pert);
	pcie->registers_ramrp = pcie_mif_map_bar(pdev, 2);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR2/3 registers %x\n", pcie->registers_ramrp);
	pcie->registers_msix = pcie_mif_map_bar(pdev, 4);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR4/5 registers %x\n", pcie->registers_msix);

	pcie_mif_enable_msi(pcie);

	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting DMA mask to 32\n");
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Error DMA mask 32bits.\n");
		pcie->dma_using_dac = 0;
	} else {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to set DMA mask. Aborting.\n");
		return NULL;
	}

	pcie->in_reset = true;
	/* Initialize spinlock */
	spin_lock_init(&pcie->mif_spinlock);
	spin_lock_init(&pcie->mask_spinlock);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCI read configuration\n");

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_COMMAND 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_VENDOR_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_VENDOR_ID 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_DEVICE_ID 0x%x\n", cmd);
	pcie->mem_allocated = 0;

	pcie_create_proc_dir(pcie);

	return pcie_if;
}

void pcie_mif_destroy_pcie(struct pci_dev *pdev, struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_ERR(PCIE_MIF, "Paean. Bye World !!!\n");

	free_irq(pci_irq_vector(pdev, 0), pcie);
	free_irq(pci_irq_vector(pdev, 1), pcie);

	pci_free_irq_vectors(pdev);

	pcie_remove_proc_dir();

	/* Unmap/free allocated memory */
	pcie_mif_unmap(interface, NULL);

	/* Create debug proc entry */
	pcie_remove_proc_dir();

	pcie_mif_reset(interface, true);

	pci_disable_device(pdev);
}

struct pci_dev *pcie_mif_get_pci_dev(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	BUG_ON(!interface || !pcie);

	return pcie->pdev;
}

struct device *pcie_mif_get_dev(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	BUG_ON(!interface || !pcie);

	return pcie->dev;
}

/* Functions for proc entry */
void *pcie_mif_get_mem(struct pcie_mif *pcie)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "READ %x\n", pcie->mem);
	return pcie->mem;
}
