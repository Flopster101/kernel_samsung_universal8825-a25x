/****************************************************************************
*
* Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
*
****************************************************************************/

/* Implements */

#include "pcie_mif.h"

/* Uses */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/moduleparam.h>
#include <asm/barrier.h>
#include <scsc/scsc_logring.h>
#include "pcie_mif_module.h"
#include "pcie_proc.h"
#include "pcie_mbox.h"
#include "functor.h"
#ifdef CONFIG_SCSC_PCIE_SNOW
#include "pmu_patch_pringle.h"
#include "mif_reg_snow_pcie.h"
#endif
#include "mifproc.h"
#include <linux/delay.h>


#define PCIE_MIF_RESET_REQUEST_SOURCE 31

struct pcie_mif *g_pcie;
struct scsc_mif_abs *g_if;
bool global_enable;

static int first_time;

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

static void *pcie_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
static int pcie_mif_reset(struct scsc_mif_abs *interface, bool reset);

/* Module parameters */

static void pcie_mif_unmap(struct scsc_mif_abs *interface, void *mem);

static bool enable_pcie_mif_arm_reset = true;
module_param(enable_pcie_mif_arm_reset, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_pcie_mif_arm_reset, "Enables ARM cores reset");

/* Types */

struct pcie_mif {
	struct scsc_mif_abs interface;
	struct pci_dev        *pdev;
	int dma_using_dac;            /* =1 if 64-bit DMA is used, =0 otherwise. */
	__iomem void          *registers;
	__iomem void          *registers_bar1;

	struct device         *dev;

	bool in_reset;

	void *mem;             /* DMA memory mapped to PCIe space for MX-AP comms */
	struct pcie_mbox mbox;                  /* mailbox emulation */
	size_t mem_allocated;
	dma_addr_t dma_addr;

	/* Callback function and dev pointer mif_intr manager handler */
	void (*wlan_handler)(int irq, void *data);
	void                  *irq_dev;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	/* Reset Request handler and context */
	void (*reset_request_handler)(int irq_num_ignored, void *data);
	void *reset_request_handler_data;

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/**
	 * Functors to trigger, or simulate, MIF WLBT Mailbox interrupts.
	 *
	 * These functors isolates the Interrupt Generator logic
	 * from differences in physical interrupt generation.
	 */
	struct functor trigger_ap_interrupt;
	struct functor trigger_r4_interrupt;
	struct functor trigger_m4_interrupt;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct functor trigger_m4_1_interrupt;
#endif
#endif
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
	void          (*pmu_handler)(int irq, void *data);
	void          *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;
};

/* Private Macros */

/** Upcast from interface member to pcie_mif */
#define pcie_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct pcie_mif, interface)

/** Upcast from trigger_ap_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_ap_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_ap_interrupt)

/** Upcast from trigger_r4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_r4_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_r4_interrupt)

/** Upcast from trigger_m4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_m4_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_m4_interrupt)

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
/** Upcast from trigger_m4_interrupt member to pcie_mif */
#define pcie_mif_from_trigger_m4_1_interrupt(trigger) container_of(trigger, struct pcie_mif, trigger_m4_1_interrupt)
#endif

/* Private Functions */
static int pcie_enable_control_set_param_cb(const char *buffer,
					      const struct kernel_param *kp)
{
	int ret;
	u32 value, val;
	struct pcie_mif *pcie;

	if (!g_if) {
		SCSC_TAG_ERR(PCIE_MIF, "Interface not initialized\n");
		return 0;
	}

	pcie = pcie_mif_from_mif_abs(g_if);

	ret = kstrtou32(buffer, 0, &value);
	if (!ret) {
		if (value == 1) {
			SCSC_TAG_ERR(PCIE_MIF, "Forcing enable\n");
			pcie_mif_reset(g_if, false);
			global_enable = true;
		} else if (0 == value) {
			SCSC_TAG_ERR(PCIE_MIF, "Forcing disable\n");
			pcie_mif_reset(g_if, true);
			global_enable = false;
		} else if (3 == value) {
			SCSC_TAG_ERR(PCIE_MIF, "Enabling TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE\n");
			writel(0xffffffff, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE);
			val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_DISABLE);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_DISABLE %x\n", val);
			val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE %x\n", val);
			val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW %x\n", val);
			val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR %x\n", val);
			if (val & AP2GFG) {
				/* BOOT_CFG_ACK */
				writel(0x1, pcie->registers_bar1 + BOOT_CFG_ACK);
				wmb();
				/* CFG_IRQ Triggered */
				SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "CFG_IRQ triggered. Clearing\n", val);
				writel(AP2GFG, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
				val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
				SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR_PENDING %x\n", val);
				val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW);
				SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW %x\n", val);
			}

			if (val & AP2WB_IRQ) {
				SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "AP2WL_IRQ triggered. Clearing\n", val);
				writel(AP2WB_IRQ, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
				val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW);
				SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW %x\n", val);
			}
		} else if (4 == value) {
			/* Enable WLAN */
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Boot WLAN\n");
			writel(PMU_AP_MB_MSG_START_WLAN, pcie->registers_bar1 + AP2WB_MAILBOX);
			wmb();
			val = readl(pcie->registers_bar1 + AP2WB_MAILBOX);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read AP2WB_MAILBOX %x\n", val);
		}
	}

	return ret;
}

static int pcie_enable_control_get_param_cb(char *buffer,
					      const struct kernel_param *kp)
{
	return sprintf(buffer, "%u\n", global_enable);
}

static struct kernel_param_ops pcie_enable_cb = {
	.set = pcie_enable_control_set_param_cb,
	.get = pcie_enable_control_get_param_cb,
};

module_param_cb(enable_pcie, &pcie_enable_cb, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable,
		 "Enables/disable MX over PCIE");


static inline void pcie_mif_reg_write(struct pcie_mif *pcie, u32 offset, u32 value)
{
	writel(value, pcie->registers_bar1 + offset);
}

static inline u32 pcie_mif_reg_read(struct pcie_mif *pcie, u32 offset)
{
	return readl(pcie->registers_bar1 + offset);
}

static void pcie_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;
}

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
static void pcie_mif_emulate_reset_request_interrupt(struct pcie_mif *pcie)
{
	/* The RESET_REQUEST interrupt is emulated over PCIe using a spare MIF interrupt source */
	if (pcie_mbox_is_ap_interrupt_source_pending(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE)) {
		/* Invoke handler if registered */
		if (pcie->reset_request_handler)
			pcie->reset_request_handler(0, pcie->reset_request_handler_data);
		/* Clear the source to emulate hardware interrupt behaviour */
		pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
	}
}
#endif

#ifdef CONFIG_SCSC_QOS
static int pcie_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	return 0;
}

static int pcie_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	return 0;
}

static int pcie_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	return 0;
}
#endif

static void pcie_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PCIE_MIF, NULL, "INT reset_request handler not registered\n");
}


#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
irqreturn_t pcie_mif_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;

	SCSC_TAG_DEBUG(PCIE_MIF, "MIF Interrupt Received. (Pending 0x%08x, Mask 0x%08x)\n",
		pcie_mbox_get_ap_interrupt_pending_bitmask(&pcie->mbox),
		pcie_mbox_get_ap_interrupt_masked_bitmask(&pcie->mbox)
	);

	/*
	 * Intercept mailbox interrupt sources (numbers > 15) used to emulate other
	 * signalling paths missing from emulator/PCIe hardware.
	 */
	pcie_mif_emulate_reset_request_interrupt(pcie);

	/* Invoke the normal MIF interrupt handler */
	if (pcie->wlan_handler != pcie_mif_irq_default_handler)
		pcie->wlan_handler(irq, pcie->irq_dev);
	else
		SCSC_TAG_INFO(PCIE_MIF, "Any handler registered\n");
	return IRQ_HANDLED;
}

#else

void pcie_set_wlbt_regs(struct pcie_mif *pcie)
{
	unsigned int ka_addr = PMU_BOOT_RAM_START_ADDR;
	uint32_t val, low_clock = 0;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;

	int i = 0;
	if (pcie->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch present in FW image\n");
		ka_patch_addr = pcie->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (pcie->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = pcie->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch not present in FW image. ARRAY_SIZE %d Use default\n", ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

	low_clock = readl(pcie->registers + TB_ICONNECT_32K_CLOCK_SCALE_COUNTER);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read TB_ICONNECT_32K_CLOCK_SCALE_COUNTER %d\n", low_clock);

	// ********** MIF BAAW regions **********
	//
	// Region #0 covers DRAM shared with AP
	// Region #1 covers DRAM shared with AP (optional)
	val = readl(pcie->registers_bar1 + MIF_START_0);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read MIF_START_0 before write 0x%x\n", val);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write MIF BAAW code\n");
	/* BAAW 0*/
	writel(0x00090000, pcie->registers_bar1 + MIF_START_0);
	val = readl(pcie->registers_bar1 + MIF_START_0);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read Test val should be 0x90000 0x%x\n", val);
	writel(0x00091000, pcie->registers_bar1 + MIF_END_0);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Apply holly magic to get PCIe addresses\n");
	/* Add an 0x900000000 offset so the CCN will redicet the transactions to PCIe */
	writel((pcie->dma_addr) >> 12, pcie->registers_bar1 + MIF_REMAP_0);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read pcie->dma_addr 0x%x\n", (uintptr_t)pcie->dma_addr);

	writel(0x80000003, pcie->registers_bar1 + MIF_DONE_0);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write R7 & M7 boot offset\n");
	writel((0x90000000 + pcie->remap_addr_wlan) >> 12, pcie->registers_bar1 + PROC_RMP_BOOT_ADDR);
	val = readl(pcie->registers_bar1 + PROC_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read PROC_RMP_BOOT_ADDR 0x%x\n", val);
	writel((0x90000000 + pcie->remap_addr_wpan) >> 12, pcie->registers_bar1 + WPAN_RMP_BOOT_ADDR);
	val = readl(pcie->registers_bar1 + WPAN_RMP_BOOT_ADDR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read WPAN_RMP_BOOT_ADDR 0x%x\n", val);
   // ********** APM BAAW regions **********

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write MIF APM\n");
   /* Region #0 covers GNSS and APM mailboxes
   { "APM_START_0"                         ,  APM_BAAW_START_ADDR + 0x00, 0x00058000 },
   { "APM_END_0"                           ,  APM_BAAW_START_ADDR + 0x04, 0x00058020 },
   { "APM_REMAP_0"                         ,  APM_BAAW_START_ADDR + 0x08, 0x00014C10 },
   { "APM_DONE_0"                          ,  APM_BAAW_START_ADDR + 0x0C, 0x80000003 }, */
	/* APM 0*/
	writel(0x00058000, pcie->registers_bar1 + APM_START_0);
	writel(0x00058020, pcie->registers_bar1 + APM_END_0);
	writel(0x00014C10, pcie->registers_bar1 + APM_REMAP_0);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_0);
   /* Region #1 covers A-BOX and CHUB mailboxes
   { "APM_START_1"                         ,  APM_BAAW_START_ADDR + 0x10, 0x00058020 },
   { "APM_END_1"                           ,  APM_BAAW_START_ADDR + 0x14, 0x00058030 },
   { "APM_REMAP_1"                         ,  APM_BAAW_START_ADDR + 0x18, 0x00014CB0 },
   { "APM_DONE_1"                          ,  APM_BAAW_START_ADDR + 0x1C, 0x80000003 }, */
	/* APM 1*/
	writel(0x00058020, pcie->registers_bar1 + APM_START_1);
	writel(0x00058030, pcie->registers_bar1 + APM_END_1);
	writel(0x00014CB0, pcie->registers_bar1 + APM_REMAP_1);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_1);
	/*
   { "APM_START_2"                         ,  APM_BAAW_START_ADDR + 0x20, 0x00058030 },
   { "APM_END_2"                           ,  APM_BAAW_START_ADDR + 0x24, 0x00058060 },
   { "APM_REMAP_2"                         ,  APM_BAAW_START_ADDR + 0x28, 0x00014CD0 },
   { "APM_DONE_2"                          ,  APM_BAAW_START_ADDR + 0x2C, 0x80000003 }, */
	/* APM 2*/
	writel(0x00058030, pcie->registers_bar1 + APM_START_2);
	writel(0x00058060, pcie->registers_bar1 + APM_END_2);
	writel(0x00014CD0, pcie->registers_bar1 + APM_REMAP_2);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_2);
	/*
   { "APM_START_3"                         ,  APM_BAAW_START_ADDR + 0x30, 0x00058060 },
   { "APM_END_3"                           ,  APM_BAAW_START_ADDR + 0x34, 0x00058070 },
   { "APM_REMAP_3"                         ,  APM_BAAW_START_ADDR + 0x38, 0x00015970 },
   { "APM_DONE_3"                          ,  APM_BAAW_START_ADDR + 0x3C, 0x80000003 }, */
	/* APM 3*/
	writel(0x00058060, pcie->registers_bar1 + APM_START_3);
	writel(0x00058070, pcie->registers_bar1 + APM_END_3);
	writel(0x00015970, pcie->registers_bar1 + APM_REMAP_3);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_3);
	/*
   { "APM_START_4"                         ,  APM_BAAW_START_ADDR + 0x40, 0x00058070 },
   { "APM_END_4"                           ,  APM_BAAW_START_ADDR + 0x44, 0x00058080 },
   { "APM_REMAP_4"                         ,  APM_BAAW_START_ADDR + 0x48, 0x000159E0 },
   { "APM_DONE_4"                          ,  APM_BAAW_START_ADDR + 0x4C, 0x80000003 }, */
	/* APM 4*/
	writel(0x00058070, pcie->registers_bar1 + APM_START_4);
	writel(0x00058080, pcie->registers_bar1 + APM_END_4);
	writel(0x000159E0, pcie->registers_bar1 + APM_REMAP_4);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_4);
	/*
   { "APM_START_5"                         ,  APM_BAAW_START_ADDR + 0x50, 0x00058080 },
   { "APM_END_5"                           ,  APM_BAAW_START_ADDR + 0x54, 0x00058090 },
   { "APM_REMAP_5"                         ,  APM_BAAW_START_ADDR + 0x58, 0x00014E30 },
   { "APM_DONE_5"                          ,  APM_BAAW_START_ADDR + 0x5C, 0x80000003 }, */
	/* APM 5*/
	writel(0x00058080, pcie->registers_bar1 + APM_START_5);
	writel(0x00058090, pcie->registers_bar1 + APM_END_5);
	writel(0x00014E30, pcie->registers_bar1 + APM_REMAP_5);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_5);
	/*
   { "APM_START_6"                         ,  APM_BAAW_START_ADDR + 0x60, 0x00058090 },
   { "APM_END_6"                           ,  APM_BAAW_START_ADDR + 0x64, 0x000580A0 },
   { "APM_REMAP_6"                         ,  APM_BAAW_START_ADDR + 0x68, 0x00014EB0 },
   { "APM_DONE_6"                          ,  APM_BAAW_START_ADDR + 0x6C, 0x80000003 }, */
	/* APM 6*/
	writel(0x00058090, pcie->registers_bar1 + APM_START_6);
	writel(0x000580A0, pcie->registers_bar1 + APM_END_6);
	writel(0x00014EB0, pcie->registers_bar1 + APM_REMAP_6);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_6);
	/*
   { "APM_START_7"                         ,  APM_BAAW_START_ADDR + 0x70, 0x000580A0 },
   { "APM_END_7"                           ,  APM_BAAW_START_ADDR + 0x74, 0x00058120 },
   { "APM_REMAP_7"                         ,  APM_BAAW_START_ADDR + 0x78, 0x00014F00 },
   { "APM_DONE_7"                          ,  APM_BAAW_START_ADDR + 0x7C, 0x80000003 }, */
	/* APM 7*/
	writel(0x000580A0, pcie->registers_bar1 + APM_START_7);
	writel(0x00058120, pcie->registers_bar1 + APM_END_7);
	writel(0x00014F00, pcie->registers_bar1 + APM_REMAP_7);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_7);
	/*
   { "APM_START_8"                         ,  APM_BAAW_START_ADDR + 0x80, 0x00058120 },
   { "APM_END_8"                           ,  APM_BAAW_START_ADDR + 0x84, 0x00058160 },
   { "APM_REMAP_8"                         ,  APM_BAAW_START_ADDR + 0x88, 0x00014940 },
   { "APM_DONE_8"                          ,  APM_BAAW_START_ADDR + 0x8C, 0x80000003 }, */
	/* APM 8*/
	writel(0x00058120, pcie->registers_bar1 + APM_START_8);
	writel(0x00058160, pcie->registers_bar1 + APM_END_8);
	writel(0x00014940, pcie->registers_bar1 + APM_REMAP_8);
	writel(0x80000003, pcie->registers_bar1 + APM_DONE_8);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TrustZone Config\n");
	writel(0x1, pcie->registers_bar1 + TZPC_PROT0SET);
	writel(0x3, pcie->registers_bar1 + TZPC_PROT0SET);
	writel(0x2, pcie->registers_bar1 + TZPC_PROT0CLR);
	writel(0x4, pcie->registers_bar1 + TZPC_PROT0SET);
	writel(0xC, pcie->registers_bar1 + TZPC_PROT0SET);
	writel(0x8, pcie->registers_bar1 + TZPC_PROT0CLR);
	writel(0xFFF, pcie->registers_bar1 + TZPC_PROT0SET);
	writel(0xFFF, pcie->registers_bar1 + TZPC_PROT0CLR);
	writel(0xEC, pcie->registers_bar1 + TZPC_PROT0SET);

	//SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Set TB_ICONNECT_32K_CLOCK_SCALE_COUNTER to %d\n", low_clock);
	//writel(low_clock, pcie->registers + TB_ICONNECT_32K_CLOCK_SCALE_COUNTER);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Set AXI to nonsecure\n");
	writel(0x1, pcie->registers + TB_ICONNECT_WLBT_AXI_NONSECURE);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Start BOOT_SOURCE\n");
	writel(0x1, pcie->registers_bar1 + BOOT_SOURCE);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write PMU code\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch start\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_addr : 0x%x\n", ka_patch_addr);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_addr_end: 0x%x\n", ka_patch_addr_end);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	while (ka_patch_addr < ka_patch_addr_end) {
		writel(*ka_patch_addr, pcie->registers_bar1 + ka_addr);
		wmb();
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
		i++;
	}
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "KA patch done\n");
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "End BOOT_SOURCE\n");
	writel(0x00, pcie->registers_bar1 + BOOT_SOURCE);
	wmb();

	pcie->boot_state = WLBT_BOOT_ACK_CFG_REQ;
	/* BOOT_CFG_ACK */
	writel(0x1, pcie->registers_bar1 + BOOT_CFG_ACK);
	wmb();
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "End GFG\n");

	pcie->boot_state = WLBT_BOOT_CFG_DONE;
}

void pcie_mif_handle_pmu_irq(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	uint32_t val= 0;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "INT received, boot_state = %u\n", pcie->boot_state);

	if (pcie->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
		/* it is executed on interrupt context. */
		pcie_set_wlbt_regs(pcie);
	} else {
		/* pcie->boot_state = WLBT_BOOT_CFG_DONE; */
		if (pcie->pmu_handler != pcie_mif_irq_default_handler)
			pcie->pmu_handler(irq, pcie->irq_dev_pmu);
		else
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "MIF PMU Interrupt Handler not registered\n");
		/* BOOT_CFG_ACK */
		writel(0x1, pcie->registers_bar1 + BOOT_CFG_ACK);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Updated BOOT_CFG_ACK\n");
	}

	/* Clear the IRQ */
	/* CFG_IRQ Triggered */
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "CFG_IRQ triggered. Clearing\n", val);
	writel(AP2GFG, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
	val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR_PENDING %x\n", val);
	val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW %x\n", val);
}

void pcie_mif_handle_wlan_irq(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	uint32_t val= 0;


	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "INT %pS\n", pcie->wlan_handler);
	if (pcie->wlan_handler != pcie_mif_irq_default_handler) {
		pcie->wlan_handler(irq, pcie->irq_dev);
	} else {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "MIF Interrupt Handler not registered.\n");
		pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTMR0), (0xffff << 16));
		pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTCR0), (0xffff << 16));
	}

	/* Clear the IRQ */
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "AP2WB_IRQ triggered. Clearing\n", val);
	writel(AP2WB_IRQ, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
	val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR_PENDING %x\n", val);
	val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_RAW %x\n", val);
}

irqreturn_t pcie_mif_isr(int irq, void *data)
{
	struct pcie_mif *pcie = (struct pcie_mif *)data;
	uint32_t val= 0;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "IN ISR!!!!!!!!!!!!!!!!!");

	val = readl(pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_SINGLE_IRQ_TO_HOST_CLEAR %x\n", val);

	if (val & AP2GFG) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "IRQ from PMU\n", val);
		pcie_mif_handle_pmu_irq(irq, data);
	}

	if (val & AP2WB_IRQ) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "IRQ from WLAN MBOX\n", val);
		pcie_mif_handle_wlan_irq(irq, data);
	}

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
/**
 * Trigger, or simulate, inbound (to AP) PCIe interrupt.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_ap_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_ap_interrupt(trigger);

	/*
	 * Invoke the normal isr handler synchronously.
	 *
	 * If synchronous handling proves problematic then launch
	 * an async task or trigger GIC interrupt manually (if supported).
	 */
	(void)pcie_mif_isr(0, (void *)pcie);
};

/**
 * Trigger PCIe interrupt to R4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_r4_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_r4_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering R4 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	mmiowb();
#else
	smp_mb();
#endif
};

/**
 * Trigger PCIe interrupt to M4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_m4_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_m4_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering M4 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG2);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
        mmiowb();
#else
	smp_mb();
#endif
};

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
/**
 * Trigger PCIe interrupt to M4.
 *
 * Called back via functor.
 */
static void pcie_mif_trigger_m4_1_interrupt(struct functor *trigger)
{
	struct pcie_mif *pcie = pcie_mif_from_trigger_m4_1_interrupt(trigger);

	SCSC_TAG_DEBUG(PCIE_MIF, "Triggering M4 1 Mailbox interrupt.\n");

	iowrite32(0x00000001, pcie->registers + SCSC_PCIE_NEWMSG3);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
        mmiowb();
#else
	smp_mb();
#endif
};
#endif
#endif

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


static int pcie_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	int ret, reta, retb, retc;
#ifndef CONFIG_SCSC_PCIE_MBOX_EMULATION
	int loop;
#endif
	if (enable_pcie_mif_arm_reset || !reset) {
		/* Sanity check */
#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
		writel(0xdeadbeef, pcie->registers_bar1 + SCSC_PCIE_SIGNATURE);
		wmb();
		ret = readl(pcie->registers_bar1 + SCSC_PCIE_SIGNATURE);
		if (ret != 0xdeadbeef) {
			SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Can't acces BAR0 magic number. Readed: 0x%x Expected: 0x%x\n",
					 ret, 0xdeadbeef);
			return -ENODEV;
		}
		iowrite32(reset ? 1 : 0,
			  pcie->registers + SCSC_PCIE_GRST_OFFSET);
		mmiowb();
#else
	if (!reset && pcie->in_reset == true) {
		first_time = 0;
		ret = readl(pcie->registers + 0x4);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read ENABLE WLBT RB. End %x\n", ret);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Enable WLBT RB\n");
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_WLBT_AXI_NONSECURE\n");
		msleep(100);
		writel(0x0, pcie->registers + TB_ICONNECT_WLBT_AXI_NONSECURE);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_WLBT_RESET\n");
		/* We're now ready for the IRQ */
		pcie->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
		msleep(100);
		writel(0x0, pcie->registers + TB_ICONNECT_WLBT_RESET);
		ret = readl(pcie->registers + 0x4);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ENABLE WLBT RB. End %x\n", ret);
		pcie->in_reset = false;
	} else if (pcie->in_reset == false){
		loop = 3;
		writel(0x1, pcie->registers + TB_ICONNECT_SWEEPER_REQ);
		while(loop-- != 0) {
			ret = readl(pcie->registers + TB_ICONNECT_SWEEPER_ACK);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "ACK = 1? %x\n", ret);
			if (ret)
				loop = 0;
			else
				msleep(1000);
		}

		loop = 3;
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_APM_PCH_LONGHOP_DOWN_REQ to 1\n");
		writel(0x1, pcie->registers + TB_ICONNECT_APM_PCH_LONGHOP_DOWN_REQ);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_REQ to 1\n");
		writel(0x1, pcie->registers + TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_REQ);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_REQ to 1\n");
		writel(0x1, pcie->registers + TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_REQ);
		msleep(100);

		while(loop-- != 0) {
			reta = readl(pcie->registers + TB_ICONNECT_APM_PCH_LONGHOP_DOWN_ACK);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_APM_PCH_LONGHOP_DOWN_ACK = 1? %x\n", reta);
			retb = readl(pcie->registers + TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_ACK);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_ACK = 1? %x\n", retb);
			retc = readl(pcie->registers + TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_ACK);
			SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_ACK = 1? %x\n", retc);
			if (reta == 1 && retb == 1 && retc == 1)
				loop = 0;
			else
				msleep(1000);
		}

		writel(0x1, pcie->registers + TB_ICONNECT_WLBT_RESET);
		ret = readl(pcie->registers + TB_ICONNECT_WLBT_RESET);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Disable WLBT RB. End %x\n", ret);

		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Disable Sweeper\n");
		writel(0x0, pcie->registers + TB_ICONNECT_SWEEPER_REQ);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_APM_PCH_LONGHOP_DOWN_REQ to 0\n");
		writel(0x0, pcie->registers + TB_ICONNECT_APM_PCH_LONGHOP_DOWN_REQ);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_REQ to 0\n");
		writel(0x0, pcie->registers + TB_ICONNECT_MIF_PCH_LONGHOP_DOWN_REQ);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_REQ to 0\n");
		writel(0x0, pcie->registers + TB_ICONNECT_CFG_PCH_LONGHOP_DOWN_REQ);

		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "***** Resetting WLBT *****\n");
		pcie->in_reset = true;
}
#endif
	} else
		SCSC_TAG_INFO(PCIE_MIF, "Not resetting ARM Cores enable_pcie_mif_arm_reset: %d\n",
			      enable_pcie_mif_arm_reset);
	return 0;
}

static void *pcie_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	int ret;
#endif
	size_t map_len = PCIE_MIF_ALLOC_MEM;

	if (allocated)
		*allocated = 0;

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	if (map_len > (PCIE_MIF_PREALLOC_MEM - 1)) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating DMA memory, requested %zu, maximum %d, consider different size\n", map_len, PCIE_MIF_PREALLOC_MEM);
		return NULL;
	}

	/* should return PAGE_ALIGN Memory */
	pcie->mem = dma_alloc_coherent(pcie->dev,
				       PCIE_MIF_PREALLOC_MEM, &pcie->dma_addr, GFP_KERNEL);
	if (pcie->mem == NULL) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Allocated dma coherent mem: %p addr %p\n", pcie->mem, (void *)pcie->dma_addr);

	iowrite32((unsigned int)pcie->dma_addr,
		  pcie->registers + SCSC_PCIE_OFFSET);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	mmiowb();
#else
	smp_mb();
#endif
	ret = ioread32(pcie->registers + SCSC_PCIE_OFFSET);
	SCSC_TAG_INFO(PCIE_MIF, "Read SHARED_BA 0x%0x\n", ret);
	if (ret != (unsigned int)pcie->dma_addr) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Can't acces BAR0 Shared BA. Readed: 0x%x Expected: 0x%x\n", ret, (unsigned int)pcie->dma_addr);
		return NULL;
	}

	/* Initialised the interrupt trigger functors required by mbox emulation */
	functor_init(&pcie->trigger_ap_interrupt, pcie_mif_trigger_ap_interrupt);
	functor_init(&pcie->trigger_r4_interrupt, pcie_mif_trigger_r4_interrupt);
	functor_init(&pcie->trigger_m4_interrupt, pcie_mif_trigger_m4_interrupt);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	functor_init(&pcie->trigger_m4_1_interrupt, pcie_mif_trigger_m4_1_interrupt);
#endif

	/* Initialise mailbox emulation to use shared memory at the end of PCIE_MIF_PREALLOC_MEM */
	pcie_mbox_init(
		&pcie->mbox,
		pcie->mem + PCIE_MIF_PREALLOC_MEM - PCIE_MIF_MBOX_RESERVED_LEN,
		pcie->registers,
		&pcie->trigger_ap_interrupt,
		&pcie->trigger_r4_interrupt,
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
		&pcie->trigger_m4_interrupt,
		&pcie->trigger_m4_1_interrupt
#else
		&pcie->trigger_m4_interrupt
#endif
		);
#else
	pcie->mem_allocated = 0;
	/* should return PAGE_ALIGN Memory */
	SCSC_TAG_ERR(PCIE_MIF, "Allocate %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
	pcie->mem = dma_alloc_coherent(pcie->dev,
					PCIE_MIF_PREALLOC_MEM, &pcie->dma_addr, GFP_KERNEL);
	if (pcie->mem == NULL) {
		SCSC_TAG_ERR(PCIE_MIF, "Error allocating %d DMA memory\n", PCIE_MIF_PREALLOC_MEM);
		return 0;
	}
#endif
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

	/* Avoid unused parameter error */
	(void)mem;

	if (pcie->mem_allocated) {
		pcie->mem_allocated = 0;
		dma_free_coherent(pcie->dev, PCIE_MIF_PREALLOC_MEM, pcie->mem, pcie->dma_addr);
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Freed dma coherent mem: %x addr %x\n", pcie->mem, (void *)pcie->dma_addr);
	}
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static u32 pcie_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_masked_bitmask(&pcie->mbox);
#else
	u32 val;

	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Getting INT-INTMSR0: 0x%x\n", val);
	return val;
#endif
}
#else
static u32 pcie_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_masked_bitmask(&pcie->mbox);
#else
	u32 val;

	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Getting INT-INTMSR0: 0x%x\n", val);
	return val;
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static u32 pcie_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_pending_bitmask(&pcie->mbox);
#else
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMSR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Getting INT-INTMSR0: 0x%x\n", val);

	return val;
#endif
}
#else
static u32 pcie_mif_irq_get(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	return pcie_mbox_get_ap_interrupt_pending_bitmask(&pcie->mbox);
#else
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not masked */
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMSR0)) >> 16;
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Getting INT-INTMSR0: 0x%x\n", val);

	return val;
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTCR0), ((1 << bit_num) << 16));
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INTCR0: bit %d\n", bit_num);
#endif
}
#else
static void pcie_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTCR0), ((1 << bit_num) << 16));
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INTCR0: bit %d\n", bit_num);
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox interrupt emulation component */
	pcie_mbox_mask_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0));
	/* WRITE : 1 = Mask Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTMR0), val | ((1 << bit_num) << 16));
	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INTMR0: 0x%x bit %d\n", val | (1 << bit_num), bit_num);
#endif

}
#else
static void pcie_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox interrupt emulation component */
	pcie_mbox_mask_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0));
	/* WRITE : 1 = Mask Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTMR0), val | ((1 << bit_num) << 16));
	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INTMR0: 0x%x bit %d\n", val | (1 << bit_num), bit_num);
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	pcie_mbox_unmask_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0));
	/* WRITE : 0 = Unmask Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTMR0), val & ~((1 << bit_num) << 16));
	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "UNMASK Setting INTMR0: 0x%x bit %d\n", val & ~((1 << bit_num) << 16), bit_num);
#endif
}
#else
static void pcie_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation component */
	pcie_mbox_unmask_ap_interrupt_source(&pcie->mbox, bit_num);
#else
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	val = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(INTMR0));
	/* WRITE : 0 = Unmask Interrupt */
	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTMR0), val & ~((1 << bit_num) << 16));
	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "UNMASK Setting INTMR0: 0x%x bit %d\n", val & ~((1 << bit_num) << 16), bit_num);
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		pcie->remap_addr_wlan = remap_addr;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		pcie->remap_addr_wpan = remap_addr;
	else
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect target %d\n", target);
}
#endif

static void pcie_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct pcie_mif                 *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation sub-module */
	pcie_mbox_set_outgoing_interrupt_source(&pcie->mbox, target, bit_num);
#else
	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(INTGR1), (1 << bit_num));
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Setting INTGR1: bit %d on target %d\n", bit_num, target);
#endif
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

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wlan_handler = handler;
	pcie->irq_dev = dev;
}

static void pcie_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->wlan_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;
}
#endif
static void pcie_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie->reset_request_handler = handler;
	pcie->reset_request_handler_data = dev;

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	pcie_mbox_clear_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
	pcie_mbox_unmask_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
#endif
}

static void pcie_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	pcie_mbox_mask_ap_interrupt_source(&pcie->mbox, PCIE_MIF_RESET_REQUEST_SOURCE);
#else
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "UnRegistering mif reset_request int handler %pS\n", interface);
	pcie->reset_request_handler = pcie_mif_irq_reset_request_default_handler;
#endif
	pcie->reset_request_handler = NULL;
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void pcie_mif_mailbox_set(struct scsc_mif_abs *interface, u32 mbox_index, u32 value, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	pcie_mif_reg_write(pcie, MAILBOX_WLBT_WL_REG(ISSR(mbox_index)), value);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "write 0x%x at mbox 0x%x\n", value, mbox_index);
}

static u32 pcie_mif_mailbox_get(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	u32 ret;
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	ret = pcie_mif_reg_read(pcie, MAILBOX_WLBT_WL_REG(ISSR(mbox_index)));
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "read 0x%x at mbox 0x%x\n", ret, mbox_index);
	return ret;
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static u32 *pcie_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation sub-module */
	return pcie_mbox_get_mailbox_ptr(&pcie->mbox, mbox_index);
#else
	u32                 *addr;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "mbox_index 0x%x\n", mbox_index);
	addr = pcie->registers_bar1 + MAILBOX_WLBT_WL_REG(ISSR(mbox_index));

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "bar1 pcie->registers_bar1 %x offset %x addr %x\n", pcie->registers_bar1, MAILBOX_WLBT_WL_REG(ISSR(mbox_index)), addr);
	return addr;
#endif
}
#else
static u32 *pcie_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
	/* delegate to mbox emulation sub-module */
	return pcie_mbox_get_mailbox_ptr(&pcie->mbox, mbox_index);
#else
	u32                 *addr;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "mbox_index 0x%x\n", mbox_index);
	addr = pcie->registers_bar1 + MAILBOX_WLBT_WL_REG(ISSR(mbox_index));

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "bar1 pcie->registers_bar1 %x offset %x addr %x\n", pcie->registers_bar1, MAILBOX_WLBT_WL_REG(ISSR(mbox_index)), addr);
	return addr;
#endif
}
#endif

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static int pcie_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	u32 val;

	//regmap_read(pcie->boot_cfg, WB2AP_MAILBOX, &val);
	val = readl(pcie->registers_bar1 + WB2AP_MAILBOX);
	SCSC_TAG_DEBUG_DEV(PCIE_MIF, pcie->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

static int pcie_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Write AP2WB_MAILBOX: %u\n", val);
	writel(val, pcie->registers_bar1 + AP2WB_MAILBOX);
	val = readl(pcie->registers_bar1 + AP2WB_MAILBOX);
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Read AP2WB_MAILBOX %x\n", val);
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

static void pcie_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, pcie, interface);
	spin_lock_irqsave(&pcie->mif_spinlock, flags);
	pcie->pmu_handler = handler;
	pcie->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&pcie->mif_spinlock, flags);
}
#endif

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

	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "\n");

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

	SCSC_TAG_ERR_DEV(PCIE_MIF, &pdev->dev,"BAR%u vaddr=0x%p busaddr=%pad len=%u\n",
			index, vaddr, &busaddr, (int)len);

	return vaddr;
}


struct scsc_mif_abs *pcie_mif_create(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	struct scsc_mif_abs *pcie_if;
	struct pcie_mif     *pcie = (struct pcie_mif *)devm_kzalloc(&pdev->dev, sizeof(struct pcie_mif), GFP_KERNEL);
	int nvec;

	/* Avoid unused parameter error */
	(void)id;

	if (!pcie)
		return NULL;

	pcie_if = &pcie->interface;

	g_if = pcie_if;
	/* initialise interface structure */
	pcie_if->destroy = pcie_mif_destroy;
	pcie_if->get_uid = pcie_mif_get_uid;
	pcie_if->reset = pcie_mif_reset;
	pcie_if->map = pcie_mif_map;
	pcie_if->unmap = pcie_mif_unmap;
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	pcie_if->mailbox_set = pcie_mif_mailbox_set;
	pcie_if->mailbox_get = pcie_mif_mailbox_get;
#endif
	pcie_if->irq_bit_set = pcie_mif_irq_bit_set;
	pcie_if->irq_get = pcie_mif_irq_get;
	pcie_if->irq_bit_mask_status_get = pcie_mif_irq_bit_mask_status_get;
	pcie_if->irq_bit_clear = pcie_mif_irq_bit_clear;
	pcie_if->irq_bit_mask = pcie_mif_irq_bit_mask;
	pcie_if->irq_bit_unmask = pcie_mif_irq_bit_unmask;
	pcie_if->irq_reg_handler = pcie_mif_irq_reg_handler;
	pcie_if->irq_unreg_handler = pcie_mif_irq_unreg_handler;
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	pcie_if->remap_set = pcie_mif_remap_set;
	pcie_if->irq_reg_handler_wpan = pcie_mif_irq_reg_handler_wpan;
	pcie_if->irq_unreg_handler_wpan = pcie_mif_irq_unreg_handler_wpan;
#endif
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
#ifdef CONFIG_SCSC_QOS
	pcie_if->mif_pm_qos_add_request = pcie_mif_pm_qos_add_request;
	pcie_if->mif_pm_qos_update_request = pcie_mif_pm_qos_update_request;
	pcie_if->mif_pm_qos_remove_request = pcie_mif_pm_qos_remove_request;
#endif
	/* Suspend/resume not supported in PCIe MIF */
	pcie_if->suspend_reg_handler = NULL;
	pcie_if->suspend_unreg_handler = NULL;

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	pcie_if->get_mbox_pmu = pcie_mif_get_mbox_pmu;
	pcie_if->set_mbox_pmu = pcie_mif_set_mbox_pmu;
	pcie_if->load_pmu_fw = pcie_load_pmu_fw;
	pcie_if->irq_reg_pmu_handler = pcie_mif_irq_reg_pmu_handler;
#endif
	pcie->pmu_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev_pmu = NULL;

	/* Update state */
	pcie->pdev = pdev;

	pcie->dev = &pdev->dev;

	pcie->wlan_handler = pcie_mif_irq_default_handler;
	pcie->irq_dev = NULL;

	/* Just do whats is necessary to meet the pci probe
	 * -BAR0 stuff
	 * -Interrupt (will be able to handle interrupts?)
	 */

	/* My stuff */
	pci_set_drvdata(pdev, pcie);

	rc = pcim_enable_device(pdev);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "Error enabling device.\n");
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

	/* old --- rc = pci_request_regions(pdev, "foo"); */
	/* Request and iomap regions specified by @mask (0x01 ---> BAR0)*/

	pci_set_master(pdev);

	/* Access iomap allocation table */
	/* return __iomem * const * */
	pcie->registers = pcie_mif_map_bar(pdev, 0);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "registers %x\n", pcie->registers);
	pcie->registers_bar1 = pcie_mif_map_bar(pdev, 1);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "registers bar 1%x\n", pcie->registers_bar1);

	writel(0x1, pcie->registers + TB_ICONNECT_CMGP);
	nvec = pci_alloc_irq_vectors(pdev, 1, 1,
				PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "nvec %d IRQs\n", nvec);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting Single IRQ Mode\n");
	writel(0x1, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_MODE);
	rc = devm_request_irq(&pdev->dev, pci_irq_vector(pdev, 0), pcie_mif_isr, 0,
				DRV_NAME, pcie);
	if (rc) {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev,
				 "Failed to register MSI handler. Aborting.\n");
		return NULL;
	}

	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting DMA mask to 32\n");
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "DMA mask 32bits.\n");
		pcie->dma_using_dac = 0;
	} else {
		SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Failed to set DMA mask. Aborting.\n");
		return NULL;
	}

	pcie->in_reset = true;
	/* Initialize spinlock */
	spin_lock_init(&pcie->mif_spinlock);

	//SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "PCI read configuration\n");
	//pci_read_config_word(pdev, PCI_COMMAND, &cmd);

	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "PCIE config finished\n");

	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting CMGP for accessing MBOXES\n");
	writel(0x1, pcie->registers + TB_ICONNECT_CMGP);
	SCSC_TAG_ERR_DEV(PCIE_MIF, pcie->dev, "Setting CMGP for accessing MBOXES read 0x%x\n", readl(pcie->registers + TB_ICONNECT_CMGP));

	SCSC_TAG_ERR(PCIE_MIF, "Enabling TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE\n");
	writel(0xffffffff, pcie->registers + TB_ICONNECT_SINGLE_IRQ_TO_HOST_ENABLE);

	pcie_create_proc_dir(pcie);

	return pcie_if;
}

void pcie_mif_destroy_pcie(struct pci_dev *pdev, struct scsc_mif_abs *interface)
{
	struct pcie_mif *pcie = pcie_mif_from_mif_abs(interface);

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
void * pcie_mif_get_mem(struct pcie_mif *pcie)
{
	SCSC_TAG_INFO_DEV(PCIE_MIF, pcie->dev, "READ %x\n", pcie->mem);
	return pcie->mem;

}

#ifdef CONFIG_SCSC_PCIE_MBOX_EMULATION
/* Functions for proc entry */
int pcie_mif_set_bar0_register(struct pcie_mif *pcie, unsigned int value, unsigned int offset)
{
	iowrite32(value, pcie->registers + offset);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	mmiowb();
#else
	smp_mb();
#endif

	return 0;
}

void pcie_mif_get_bar0(struct pcie_mif *pcie, struct scsc_bar0_reg *bar0)
{
	bar0->NEWMSG            = ioread32(pcie->registers + SCSC_PCIE_NEWMSG);
	bar0->SIGNATURE         = ioread32(pcie->registers + SCSC_PCIE_SIGNATURE);
	bar0->OFFSET            = ioread32(pcie->registers + SCSC_PCIE_OFFSET);
	bar0->RUNEN             = ioread32(pcie->registers + SCSC_PCIE_RUNEN);
	bar0->DEBUG             = ioread32(pcie->registers + SCSC_PCIE_DEBUG);
	bar0->AXIWCNT           = ioread32(pcie->registers + SCSC_PCIE_AXIWCNT);
	bar0->AXIRCNT           = ioread32(pcie->registers + SCSC_PCIE_AXIRCNT);
	bar0->AXIWADDR          = ioread32(pcie->registers + SCSC_PCIE_AXIWADDR);
	bar0->AXIRADDR          = ioread32(pcie->registers + SCSC_PCIE_AXIRADDR);
	bar0->TBD               = ioread32(pcie->registers + SCSC_PCIE_TBD);
	bar0->AXICTRL           = ioread32(pcie->registers + SCSC_PCIE_AXICTRL);
	bar0->AXIDATA           = ioread32(pcie->registers + SCSC_PCIE_AXIDATA);
	bar0->AXIRDBP           = ioread32(pcie->registers + SCSC_PCIE_AXIRDBP);
	bar0->IFAXIWCNT         = ioread32(pcie->registers + SCSC_PCIE_IFAXIWCNT);
	bar0->IFAXIRCNT         = ioread32(pcie->registers + SCSC_PCIE_IFAXIRCNT);
	bar0->IFAXIWADDR        = ioread32(pcie->registers + SCSC_PCIE_IFAXIWADDR);
	bar0->IFAXIRADDR        = ioread32(pcie->registers + SCSC_PCIE_IFAXIRADDR);
	bar0->IFAXICTRL         = ioread32(pcie->registers + SCSC_PCIE_IFAXICTRL);
	bar0->GRST              = ioread32(pcie->registers + SCSC_PCIE_GRST);
	bar0->AMBA2TRANSAXIWCNT = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIWCNT);
	bar0->AMBA2TRANSAXIRCNT = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIRCNT);
	bar0->AMBA2TRANSAXIWADDR        = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIWADDR);
	bar0->AMBA2TRANSAXIRADDR        = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXIRADDR);
	bar0->AMBA2TRANSAXICTR  = ioread32(pcie->registers + SCSC_PCIE_AMBA2TRANSAXICTR);
	bar0->TRANS2PCIEREADALIGNAXIWCNT        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIWCNT);
	bar0->TRANS2PCIEREADALIGNAXIRCNT        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIRCNT);
	bar0->TRANS2PCIEREADALIGNAXIWADDR       = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIWADDR);
	bar0->TRANS2PCIEREADALIGNAXIRADDR       = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXIRADDR);
	bar0->TRANS2PCIEREADALIGNAXICTRL        = ioread32(pcie->registers + SCSC_PCIE_TRANS2PCIEREADALIGNAXICTRL);
	bar0->READROUNDTRIPMIN  = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPMIN);
	bar0->READROUNDTRIPMAX  = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPMAX);
	bar0->READROUNDTRIPLAST = ioread32(pcie->registers + SCSC_PCIE_READROUNDTRIPLAST);
	bar0->CPTAW0            = ioread32(pcie->registers + SCSC_PCIE_CPTAW0);
	bar0->CPTAW1            = ioread32(pcie->registers + SCSC_PCIE_CPTAW1);
	bar0->CPTAR0            = ioread32(pcie->registers + SCSC_PCIE_CPTAR0);
	bar0->CPTAR1            = ioread32(pcie->registers + SCSC_PCIE_CPTAR1);
	bar0->CPTB0             = ioread32(pcie->registers + SCSC_PCIE_CPTB0);
	bar0->CPTW0             = ioread32(pcie->registers + SCSC_PCIE_CPTW0);
	bar0->CPTW1             = ioread32(pcie->registers + SCSC_PCIE_CPTW1);
	bar0->CPTW2             = ioread32(pcie->registers + SCSC_PCIE_CPTW2);
	bar0->CPTR0             = ioread32(pcie->registers + SCSC_PCIE_CPTR0);
	bar0->CPTR1             = ioread32(pcie->registers + SCSC_PCIE_CPTR1);
	bar0->CPTR2             = ioread32(pcie->registers + SCSC_PCIE_CPTR2);
	bar0->CPTRES            = ioread32(pcie->registers + SCSC_PCIE_CPTRES);
	bar0->CPTAWDELAY        = ioread32(pcie->registers + SCSC_PCIE_CPTAWDELAY);
	bar0->CPTARDELAY        = ioread32(pcie->registers + SCSC_PCIE_CPTARDELAY);
	bar0->CPTSRTADDR        = ioread32(pcie->registers + SCSC_PCIE_CPTSRTADDR);
	bar0->CPTENDADDR        = ioread32(pcie->registers + SCSC_PCIE_CPTENDADDR);
	bar0->CPTSZLTHID        = ioread32(pcie->registers + SCSC_PCIE_CPTSZLTHID);
	bar0->CPTPHSEL          = ioread32(pcie->registers + SCSC_PCIE_CPTPHSEL);
	bar0->CPTRUN            = ioread32(pcie->registers + SCSC_PCIE_CPTRUN);
	bar0->FPGAVER           = ioread32(pcie->registers + SCSC_PCIE_FPGAVER);
}
#endif
