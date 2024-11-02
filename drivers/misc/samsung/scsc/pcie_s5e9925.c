/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements */
#include "pcie_s5e9925.h"
/* Uses */
#include "pcie_mif.h"
#include <linux/pfn.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <scsc/scsc_logring.h>
#define DRV_NAME "scscPCIe"

#include "pmu_paean.h"
#include "pmu_patch_paean.h"
#include "mif_reg_paean_pcie.h"

#define PCIE_MIF_PREALLOC_MEM (16 * 1024 * 1024)

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

static bool enable_pcie_mif_arm_reset = true;
module_param(enable_pcie_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_mif_arm_reset, "Enables ARM cores reset");

#define NUM_TO_HOST_IRQ_PMU 1
#define NUM_TO_HOST_IRQ_WLAN 16
#define NUM_TO_HOST_IRQ_WPAN 10
#define NUM_TO_HOST_IRQ_PMU_START 0
#define NUM_TO_HOST_IRQ_PMU_END ((NUM_TO_HOST_IRQ_PMU_START + NUM_TO_HOST_IRQ_PMU) - 1)
#define NUM_TO_HOST_IRQ_WLAN_START (NUM_TO_HOST_IRQ_PMU_END + 1)
#define NUM_TO_HOST_IRQ_WLAN_END ((NUM_TO_HOST_IRQ_WLAN_START + NUM_TO_HOST_IRQ_WLAN) - 1)
#define NUM_TO_HOST_IRQ_WPAN_START (NUM_TO_HOST_IRQ_WLAN_END + 1)
#define NUM_TO_HOST_IRQ_WPAN_END ((NUM_TO_HOST_IRQ_WPAN_START + NUM_TO_HOST_IRQ_WPAN) - 1)
#define NUM_TO_HOST_IRQ_TOTAL (NUM_TO_HOST_IRQ_PMU + NUM_TO_HOST_IRQ_WLAN + NUM_TO_HOST_IRQ_WPAN)

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
	bool initialized;

	void *mem; /* DMA memory mapped to PCIe space for MX-AP comms */
	size_t mem_allocated;
	dma_addr_t dma_addr;
	unsigned long mem_start;
	size_t mem_sz;

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
	void (*wlan_handler)(int irq, void *data);
	void (*wpan_handler)(int irq, void *data);
	void *irq_dev_wlan;
	void *irq_dev_wpan;
	void *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

	struct msi_emul pmu_msi[NUM_TO_HOST_IRQ_PMU];
	struct msi_emul wlan_msi[NUM_TO_HOST_IRQ_WLAN];
	struct msi_emul wpan_msi[NUM_TO_HOST_IRQ_WPAN];
	u32 rcv_irq_wlan;
	u32 rcv_irq_wpan;
};

static struct pcie_mif single_pci;
struct pcie_mif *pcie = &single_pci;

static int pcie_s5e9925_filter_set_param_cb(const char *buffer, const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(buffer, 0, &value);
	writel(value, pcie->registers_ramrp);
	value = readl(pcie->registers_ramrp);

	writel(value + 1, pcie->registers_ramrp + 4);
	value = readl(pcie->registers_ramrp + 4);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "CAFE?????? %x\n", value);
	return 0;
}

static struct kernel_param_ops pcie_s5e9925_filter_ops = {
	.set = pcie_s5e9925_filter_set_param_cb,
	.get = NULL,
};

module_param_cb(pcie_write_ramp, &pcie_s5e9925_filter_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pcie_write_ramp, "Forces a crash of the Bluetooth driver");

static void pcie_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;
}

static void pcie_set_atu(struct pcie_mif *pcie)
{
	pcie_atu_config_t atu;

	/* Send ATU mappings */
	atu.start_addr_0 = (uint64_t)pcie->mem_start;
	atu.end_addr_0 = (uint64_t)(pcie->mem_start + pcie->mem_sz);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Copy ATU offset 0x%x start 0x%lx end 0x%lx\n", RAMRP_HOSTIF_PMU_BUF_PTR,
			  atu.start_addr_0, atu.end_addr_0);

	writel(atu.start_addr_0 & 0xffffffff, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR);
	writel(atu.start_addr_0 >> 32, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR + 0x4);
	writel(atu.end_addr_0 & 0xffffffff, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR + 0x8);
	writel(atu.end_addr_0 >> 32, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR + 0xc);

	//memcpy_toio(pcie->registers_ramrp + RAMRP_HOSTIF_PMU_BUF_PTR, &atu, sizeof(pcie_atu_config_t));
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
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_UCPU0_RMP_BOOT_ADDR 0x%x\n",
			  (pcie->remap_addr_wlan >> 12));
	writel((pcie->remap_addr_wlan >> 12), pcie->registers_pert + PMU_WLAN_UCPU0_RMP_BOOT_ADDR);
	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU0_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU0_RMP_BOOT_ADDR 0x%x\n", cmd_dw);

	cmd_dw = readl(pcie->registers_pert + PMU_WLAN_UCPU1_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read PMU_WLAN_UCPU1_RMP_BOOT_ADDR 0x%x\n", cmd_dw);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU_WLAN_UCPU1_RMP_BOOT_ADDR 0x%x\n",
			  (pcie->remap_addr_wlan >> 12));
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
	unsigned int ka_addr = 0x0;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;
	u32 cmd_dw;

	if (pcie->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch present in FW image\n");
		ka_patch_addr = pcie->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (pcie->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = pcie->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n",
				  ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch download start\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	while (ka_patch_addr < ka_patch_addr_end) {
		writel(*ka_patch_addr, pcie->registers_ramrp + ka_addr);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
	}

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

void pcie_remap_set(struct pcie_mif *pcie, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Setting remapper address %u target %s\n", remap_addr,
			  (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		pcie->remap_addr_wlan = remap_addr + DRAM_BASE_ADDR;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		pcie->remap_addr_wpan = remap_addr + DRAM_BASE_ADDR;
	else
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect target %d\n", target);
}

void pcie_irq_bit_set(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
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

void pcie_irq_bit_clear(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
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

void pcie_irq_bit_mask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	unsigned int logic_bit_num;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = true;
		disable_irq_nosync(pcie->wlan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already masked ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = true;
		disable_irq_nosync(pcie->wpan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Mask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wpan_msi[logic_bit_num].msi_vec);
	}
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);
	return;
}

void pcie_irq_bit_unmask(struct pcie_mif *pcie, int bit_num, enum scsc_mif_abs_target target)
{
	unsigned long flags;
	unsigned int logic_bit_num;

	/* bit_num is MSI, so we need to modify to logical */
	spin_lock_irqsave(&pcie->mask_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		logic_bit_num = bit_num - pcie->wlan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WLAN)
			goto end;
		if (!pcie->wlan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already enabled ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wlan_msi[logic_bit_num].is_masked = false;
		enable_irq(pcie->wlan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wlan_msi[logic_bit_num].msi_vec);
	} else {
		logic_bit_num = bit_num - pcie->wpan_msi[0].msi_bit;
		if (logic_bit_num >= NUM_TO_HOST_IRQ_WPAN)
			goto end;
		if (!pcie->wpan_msi[logic_bit_num].is_masked) {
			SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev,
					   "MSI Bit Mask %u subsystem %d line %d. Already enabled ignore request\n",
					   bit_num, target, pcie->wlan_msi[logic_bit_num].msi_vec);
			goto end;
		}
		pcie->wpan_msi[logic_bit_num].is_masked = false;
		enable_irq(pcie->wpan_msi[logic_bit_num].msi_vec);
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MSI Bit Unmask %u subsystem %d line %d\n", bit_num, target,
				   pcie->wpan_msi[logic_bit_num].msi_vec);
	}
end:
	spin_unlock_irqrestore(&pcie->mask_spinlock, flags);

	return;
}

int pcie_get_mbox_pmu(struct pcie_mif *pcie)
{
	u32 val = 0;

	val = readl(pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_TO_HOST_PTR);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

int pcie_set_mbox_pmu(struct pcie_mif *pcie, u32 val)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write CMD %d at offset 0x%x\n", val,
			  RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	writel(val, pcie->registers_ramrp + RAMRP_HOSTIF_PMU_MBOX_FROM_HOST_PTR);
	msleep(1);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TRIGGER IRQ 0x2: PMU_INTERRUPT_FROM_HOST_INTGR\n");
	writel(0x2, pcie->registers_pert + PMU_INTERRUPT_FROM_HOST_INTGR);
	msleep(1);
	return 0;
}

int pcie_load_pmu_fw(struct pcie_mif *pcie, u32 *ka_patch, size_t ka_patch_len)
{
	if (pcie->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "ka_patch already allocated\n");
		devm_kfree(pcie->dev, pcie->ka_patch_fw);
	}

	pcie->ka_patch_fw = kzalloc(ka_patch_len, GFP_KERNEL);

	if (!pcie->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PCIE_MIF, pcie->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(pcie->ka_patch_fw, ka_patch, ka_patch_len);
	pcie->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}

int pci_get_mifram_ref(struct pcie_mif *pcie, void *ptr, scsc_mifram_ref *ref)
{
	if (ptr > (pcie->mem + pcie->mem_sz)) {
		SCSC_TAG_ERR(PCIE_MIF, "ooops limits reached\n");
		return -ENOMEM;
	}

	/* Ref is byte offset wrt start of shared memory */
	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)pcie->mem);

	return 0;
}

void *pcie_get_mifram_ptr(struct pcie_mif *pcie, scsc_mifram_ref ref)
{
	/* Check limits */
	if (ref >= 0 && ref < pcie->mem_sz)
		return (void *)((uintptr_t)pcie->mem + (uintptr_t)ref);
	else
		return NULL;
}

static bool already_done;

int pcie_reset(struct pcie_mif *pcie, bool reset)
{
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

#define SCSC_STATIC_MIFRAM_PAGE_TABLE
#define MAX_RSV_MEM 16 * 1024 * 1204

static void __iomem *pcie_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t i;
	struct page **pages;
	void *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PCIE_MIF, "wlbt: kmalloc of %zd byte pages table failed\n",
			     (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[(MAX_RSV_MEM >> PAGE_SHIFT) * sizeof(*pages)];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));

		if (size > MAX_RSV_MEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PCIE_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
				     size, MAX_RSV_MEM);
			return NULL;
		}

		SCSC_TAG_INFO(PCIE_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PCIE_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

void pcie_get_msi_range(struct pcie_mif *pcie, u8 *start, u8 *end, enum scsc_mif_abs_target target)
{
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		*start = NUM_TO_HOST_IRQ_WLAN_START;
		*end = NUM_TO_HOST_IRQ_WLAN_END;
	} else {
		*start = NUM_TO_HOST_IRQ_WPAN_START;
		*end = NUM_TO_HOST_IRQ_WPAN_END;
	}
}

void *pcie_map(struct pcie_mif *pcie, size_t *allocated)
{
	if (allocated)
		*allocated = 0;

	pcie->mem = pcie_mif_map_region(pcie->mem_start, pcie->mem_sz);

	if (!pcie->mem) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Map: virt %p phys %lx\n", pcie->mem, (uintptr_t)pcie->mem_start);

	if (allocated)
		*allocated = pcie->mem_sz;
	return pcie->mem;
}

/* Functions for proc entry */
void *pcie_mif_get_mem(struct pcie_mif *pcie)
{
	//SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "READ %x\n", pcie->mem);
	return pcie->mem;
}

irqreturn_t pcie_mif_pmu_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "INT received, boot_state = %u\n", pcie->boot_state);

	/* pcie->boot_state = WLBT_BOOT_CFG_DONE; */
	if (pcie->pmu_handler != pcie_mif_irq_default_handler)
		pcie->pmu_handler(irq, pcie->irq_dev_pmu);
	else
		SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "MIF PMU Interrupt Handler not registered\n");

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

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IRQ received line %d msi_bit %u cpu %d", irq,
			   pcie->rcv_irq_wlan, smp_processor_id());
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wlan_handler != pcie_mif_irq_default_handler) {
		pcie->wlan_handler(irq, pcie->irq_dev_wlan);
	} else
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
	pcie->rcv_irq_wpan = 0;
	/* get MSI bit correspin to line */
	for (i = 0; i < NUM_TO_HOST_IRQ_WPAN; i++) {
		if (pcie->wpan_msi[i].msi_vec == irq)
			pcie->rcv_irq_wpan = pcie->wpan_msi[i].msi_bit;
	}

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "IRQ received line %d msi_bit %u cpu %d", irq,
			   pcie->rcv_irq_wlan, smp_processor_id());
#if 0 /* not yet */
	/* We need to disable this IRQ line based on MBOX emulation */
	disable_irq_nosync(irq);
#endif

	if (pcie->wpan_handler != pcie_mif_irq_default_handler)
		pcie->wpan_handler(irq, pcie->irq_dev_wpan);
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");

	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	return IRQ_HANDLED;
}

u32 pcie_irq_get(struct pcie_mif *pcie, enum scsc_mif_abs_target target)
{
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

static int pcie_mif_enable_msi(struct pcie_mif *pcie)
{
	struct msi_desc *msi_desc;
	int num_vectors;
	int ret;
	u32 acc = 0, i;

	num_vectors = pci_alloc_irq_vectors(pcie->pdev, NUM_TO_HOST_IRQ_TOTAL, NUM_TO_HOST_IRQ_TOTAL, PCI_IRQ_MSI);
	if (num_vectors != NUM_TO_HOST_IRQ_TOTAL) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "failed to get %d MSI vectors, only %d available\n",
				 NUM_TO_HOST_IRQ_TOTAL, num_vectors);

		if (num_vectors >= 0)
			return -EINVAL;
		else
			return num_vectors;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "vectors %d\n", num_vectors);

	for (i = 0; i < NUM_TO_HOST_IRQ_PMU; i++, acc++) {
		pcie->pmu_msi[i].msi_vec = pci_irq_vector(pcie->pdev, 0);
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for PMU\n", acc,
				 pcie->pmu_msi[i]);
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
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WLAN\n", acc,
				 pcie->wlan_msi[i].msi_vec);
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
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting MSI %d interrupt vector line %d for WPAN\n", acc,
				 pcie->wpan_msi[i].msi_vec);
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

void __iomem *pcie_mif_map_bar(struct pci_dev *pdev, u8 index)
{
	void __iomem *vaddr;
#if 0
	struct resource *temp;
	temp = &pdev->resource[index];

	vaddr = devm_ioremap_resource(&pdev->dev, temp);
#else
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
#endif
	return vaddr;
}

static int pcie_mif_module_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	u16 cmd;
#if 0
	u16 val16;
	u32 val;
#endif
	/* Update state */
	pcie->pdev = pdev;
	pcie->dev = &pdev->dev;

	rc = pcim_enable_device(pdev);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Error enabling device.\n");
		return rc;
	}
	/* This function returns the flags associated with this resource.*/
	/* esource flags are used to define some features of the individual resource.
	 *      For PCI resources associated with PCI I/O regions, the information is extracted from the base address registers */
	/* IORESOURCE_MEM = If the associated I/O region exists, one and only one of these flags is set */
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		SCSC_TAG_ERR(PCIE_MIF, "Incorrect BAR configuration\n");
		return -EIO;
	}
	pci_set_master(pdev);
	pci_set_drvdata(pdev, pcie);
	pcie_mif_enable_msi(pcie);

	/* Access iomap allocation table */
	/* return __iomem * const * */
	pcie->registers_pert = pcie_mif_map_bar(pdev, 0);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR0/1 registers %x\n", pcie->registers_pert);
	pcie->registers_ramrp = pcie_mif_map_bar(pdev, 2);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR2/3 registers %x\n", pcie->registers_ramrp);
	pcie->registers_msix = pcie_mif_map_bar(pdev, 4);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "BAR4/5 registers %x\n", pcie->registers_msix);

	pcie->in_reset = true;
	/* Initialize spinlock */
	spin_lock_init(&pcie->mif_spinlock);
	/* spin_lock_init(&pcie->mask_spinlock); */
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCI read configuration\n");

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_COMMAND 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_VENDOR_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_VENDOR_ID 0x%x\n", cmd);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &cmd);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "PCIE PCI_DEVICE_ID 0x%x\n", cmd);
	pcie->mem_allocated = 0;

	pcie->initialized = true;
	return 0;
}

static void pcie_mif_module_remove(struct pci_dev *pcie_dev)
{
}

static const struct pci_device_id pcie_mif_module_tbl[] = { {
								    PCI_VENDOR_ID_SAMSUNG,
								    PCI_ANY_ID,
								    PCI_ANY_ID,
								    PCI_ANY_ID,
							    },
							    { /*End: all zeroes */ } };

MODULE_DEVICE_TABLE(pci, pcie_mif_module_tbl);

static struct pci_driver scsc_pcie = {
	.name = "scsc_pcie",
	.id_table = pcie_mif_module_tbl,
	.probe = pcie_mif_module_probe,
	.remove = pcie_mif_module_remove,
};

void pcie_irq_reg_pmu_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu int handler %pS\n", handler);
	pcie->pmu_handler = handler;
	pcie->irq_dev_pmu = dev;
}

void pcie_irq_reg_wlan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif wlan int handler %pS\n", handler);
	pcie->wlan_handler = handler;
	pcie->irq_dev_wlan = dev;
}

void pcie_irq_reg_wpan_handler(struct pcie_mif *pcie, void (*handler)(int irq, void *data), void *dev)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif wpan int handler %pS\n", handler);
	pcie->wpan_handler = handler;
	pcie->irq_dev_wpan = dev;
}

bool pcie_is_initialized(void)
{
	return pcie->initialized;
}

void pcie_set_mem_range(struct pcie_mif *pcie, unsigned long mem_start, size_t mem_sz)
{
	pcie->mem_start = mem_start;
	pcie->mem_sz = mem_sz;
}

void pcie_register_driver(void)
{
	pci_register_driver(&scsc_pcie);
}

struct pcie_mif *pcie_get_instance(void)
{
	pcie->initialized = false;

	/* return singleton refererence */
	return pcie;
}
