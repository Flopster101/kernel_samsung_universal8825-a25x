/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <soc/samsung/exynos-smc.h>
#else
#include <linux/smc.h>
#endif

#include "npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"

struct reg_cmd_list;

char *npu_cmd_type[] = {
	"read",
	"write",
	"smc_read",
	"smc_write"
};

struct npu_reg_format {
	unsigned int	base;
	unsigned int	offset;
	unsigned int	flag;
	unsigned int	count;
	unsigned int	interval;
	unsigned int	size;
	const char		*name;
	unsigned int	*sfr;
};

#ifdef CONFIG_NPU_USE_HW_DEVICE
static struct npu_reg_format npu_dsp_sfr_reg[] = {
	{ 0x20000, 0, 0, 0, 0, 0x12a0, "DNC_CTRL_NS" },
	{ 0xA0000, 0, 0, 0, 0, 0x60, "SDMA_SHADOW_NS"},
	{ 0xD1000, 0, 0, 0, 0, 0x2004, "GIC" },
//	{ 0x100000, 0, 0, 0, 0, 0x4000, "SDMA_COMMON_SFR" },
	{ 0x110000, 0, 0, 0, 0, 0x8000, "SDMA_ND_MEM" },
	{ 0x120000, 0, 0, 0, 0, 0x100, "SDMA_SH_MEM" },
	{ 0x130000, 0, 0, 0, 0, 0x100, "BAAW0" },
	{ 0x140000, 0, 0, 0, 0, 0x100, "BAAW1" },
	{ 0x150000, 0, 0, 0, 0, 0x100, "BAAW2" },
	{ 0x160000, 0, 0, 0, 0, 0x100, "BAAW3" },
	{ 0x190000, 0, 0, 0, 0, 0x648, "DSP0_CMDQ" },
	{ 0x1A0000, 0, 0, 0, 0, 0x14c4, "DSP0_RQ" },
	{ 0x210000, 0, 0, 0, 0, 0x648, "NPUCORE0_CMDQ" },
	{ 0x220000, 0, 0, 0, 0, 0x14c4, "NPUCORE0_RQ" },
	{ 0x2E0000, 0, 0, 0, 0, 0x14, "NPUCORE0_EXCEPTION" },
	{ 0x2F0000, 0, 0, 0, 0, 0x658, "NPUCORE0_SYSREG" },
	{ 0x310000, 0, 0, 0, 0, 0x648, "NPUCORE1_CMDQ" },
	{ 0x320000, 0, 0, 0, 0, 0x14c4, "NPUCORE1_RQ" },
	{ 0x3E0000, 0, 0, 0, 0, 0x14, "NPUCORE1_EXCEPTION" },
	{ 0x3F0000, 0, 0, 0, 0, 0x658, "NPUCORE1_SYSREG" },
};

static struct npu_reg_format dsp0_ctrl_sfr_reg[] = {
	/* REG_DSP0_CTRL */
	{ 0x180000, 0x0000, 0, 0, 0, 1, "DSP0_CTRL_BUSACTREQ" },
	{ 0x180000, 0x0004, 0, 0, 0, 1, "DSP0_CTRL_SWRESET" },
	{ 0x180000, 0x0008, 0, 0, 0, 1, "DSP0_CTRL_CORE_ID" },
	{ 0x180000, 0x0010, 0, 0, 0, 1, "DSP0_CTRL_PERF_MON_ENABLE" },
	{ 0x180000, 0x0014, 0, 0, 0, 1, "DSP0_CTRL_PERF_MON_CLEAR" },
	{ 0x180000, 0x0018, 0, 0, 0, 1, "DSP0_CTRL_DBG_MON_ENABLE" },
	{ 0x180000, 0x0020, 0, 0, 0, 1, "DSP0_CTRL_DBG_INTR_STATUS" },
	{ 0x180000, 0x0024, 0, 0, 0, 1, "DSP0_CTRL_DBG_INTR_ENABLE" },
	{ 0x180000, 0x0028, 0, 0, 0, 1, "DSP0_CTRL_DBG_INTR_CLEAR" },
	{ 0x180000, 0x002C, 0, 0, 0, 1, "DSP0_CTRL_DBG_INTR_MSTATUS" },
	{ 0x180000, 0x0030, 0, 0, 0, 1, "DSP0_CTRL_IVP_SFR2AXI_SGMO" },
	{ 0x180000, 0x0038, 0, 0, 0, 1, "DSP0_CTRL_SRESET_DONE_STATUS" },
	{ 0x180000, 0x0040, 0, 0, 8, 1, "DSP0_CTRL_VM_STATCK_START$" },
	{ 0x180000, 0x0044, 0, 0, 8, 1, "DSP0_CTRL_VM_STATCK_END$" },
	{ 0x180000, 0x0060, 0, 0, 0, 1, "DSP0_CTRL_VM_MODE" },
	{ 0x180000, 0x0080, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH0_PC" },
	{ 0x180000, 0x0084, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH0_VALID_CNTL" },
	{ 0x180000, 0x0088, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH0_VALID_CNTH" },
	{ 0x180000, 0x008C, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH0_STALL_CNT" },
	{ 0x180000, 0x0090, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH1_PC" },
	{ 0x180000, 0x0094, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH1_VALID_CNTL" },
	{ 0x180000, 0x0098, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH1_VALID_CNTH" },
	{ 0x180000, 0x009C, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_TH1_STALL_CNT" },
	{ 0x180000, 0x00A0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_IC_REQL" },
	{ 0x180000, 0x00A4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_IC_REQH" },
	{ 0x180000, 0x00A8, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_IC_MISS" },
	{ 0x180000, 0x00AC, 0, 0, 8, 1, "DSP0_CTRL_PERF_IVP0_INST_CNTL$" },
	{ 0x180000, 0x00B0, 0, 0, 8, 1, "DSP0_CTRL_PERF_IVP0_INST_CNTH$" },
	{ 0x180000, 0x00CC, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH0_PC" },
	{ 0x180000, 0x00D0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH0_VALID_CNTL" },
	{ 0x180000, 0x00D4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH0_VALID_CNTH" },
	{ 0x180000, 0x00D8, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH0_STALL_CNT" },
	{ 0x180000, 0x00DC, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH1_PC" },
	{ 0x180000, 0x00E0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH1_VALID_CNTL" },
	{ 0x180000, 0x00E4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH1_VALID_CNTH" },
	{ 0x180000, 0x00E8, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_TH1_STALL_CNT" },
	{ 0x180000, 0x00EC, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_IC_REQL" },
	{ 0x180000, 0x00F0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_IC_REQH" },
	{ 0x180000, 0x00F4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_IC_MISS" },
	{ 0x180000, 0x00F8, 0, 0, 8, 1, "DSP0_CTRL_PERF_IVP1_INST_CNTL$" },
	{ 0x180000, 0x00FC, 0, 0, 8, 1, "DSP0_CTRL_PERF_IVP1_INST_CNTH$" },
	{ 0x180000, 0x0128, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP0_ADDR_PM" },
	{ 0x180000, 0x012c, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP0_ADDR_DM" },
	{ 0x180000, 0x0130, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP0_ERROR_INFO" },
	{ 0x180000, 0x0154, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP1_ADDR_PM" },
	{ 0x180000, 0x0158, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP1_ADDR_DM" },
	{ 0x180000, 0x015c, 0, 0, 0, 1, "DSP0_CTRL_DBG_IVP1_ERROR_INFO" },
	{ 0x180000, 0x0180, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_STALL_CNTL" },
	{ 0x180000, 0x0184, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_STALL_CNTH" },
	{ 0x180000, 0x0188, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_SFR_STALL_CNTL" },
	{ 0x180000, 0x018c, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_SFR_STALL_CNTH" },
	{ 0x180000, 0x0190, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_VM0_STALL_CNTL" },
	{ 0x180000, 0x0194, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_VM0_STALL_CNTH" },
	{ 0x180000, 0x0198, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_VM1_STALL_CNTL" },
	{ 0x180000, 0x019c, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP0_VM1_STALL_CNTH" },
	{ 0x180000, 0x01a0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_STALL_CNTL" },
	{ 0x180000, 0x01a4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_STALL_CNTH" },
	{ 0x180000, 0x01a8, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_SFR_STALL_CNTL" },
	{ 0x180000, 0x01ac, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_SFR_STALL_CNTH" },
	{ 0x180000, 0x01b0, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_VM0_STALL_CNTL" },
	{ 0x180000, 0x01b4, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_VM0_STALL_CNTH" },
	{ 0x180000, 0x01b8, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_VM1_STALL_CNTL" },
	{ 0x180000, 0x01bc, 0, 0, 0, 1, "DSP0_CTRL_PERF_IVP1_VM1_STALL_CNTH" },
	{ 0x180000, 0x01c8, 0, 0, 4, 1, "DSP0_CTRL_SM_ID$" },
	{ 0x180000, 0x01d8, 0, 0, 4, 1, "DSP0_CTRL_SM_ADDR$" },
	{ 0x180000, 0x01e8, 0, 0, 0, 1, "DSP0_CTRL_IVP0_STM_FUNC_STATUS" },
	{ 0x180000, 0x01ec, 0, 0, 0, 1, "DSP0_CTRL_IVP1_STM_FUNC_STATUS" },
	{ 0x180000, 0x0208, 0, 0, 0, 1, "DSP0_CTRL_AXI_ERROR_RD" },
	{ 0x180000, 0x020c, 0, 0, 0, 1, "DSP0_CTRL_AXI_ERROR_WR" },
	{ 0x180000, 0x0210, 0, 0, 4, 1, "DSP0_CTRL_RD_MOCNT$" },
	{ 0x180000, 0x0280, 0, 0, 4, 1, "DSP0_CTRL_WR_MOCNT$" },
	{ 0x180000, 0x0400, 0, 0, 0, 1, "DSP0_CTRL_IVP0_WAKEUP" },
	{ 0x180000, 0x0404, 0, 0, 4, 1, "DSP0_CTRL_IVP0_INTR_STATUS_TH$" },
	{ 0x180000, 0x040c, 0, 0, 4, 1, "DSP0_CTRL_IVP0_INTR_ENABLE_TH$" },
	{ 0x180000, 0x0414, 0, 0, 4, 1, "DSP0_CTRL_IVP0_SWI_SET_TH$" },
	{ 0x180000, 0x041c, 0, 0, 4, 1, "DSP0_CTRL_IVP0_SWI_CLEAR_TH$" },
	{ 0x180000, 0x0424, 0, 0, 4, 1, "DSP0_CTRL_IVP0_MASKED_STATUS_TH$" },
	{ 0x180000, 0x042c, 0, 0, 0, 1, "DSP0_CTRL_IVP0_IC_BASE_ADDR" },
	{ 0x180000, 0x0430, 0, 0, 0, 1, "DSP0_CTRL_IVP0_IC_CODE_SIZE" },
	{ 0x180000, 0x0434, 0, 0, 0, 1, "DSP0_CTRL_IVP0_IC_INVALID_REQ" },
	{ 0x180000, 0x0438, 0, 0, 0, 1, "DSP0_CTRL_IVP0_IC_INVALID_STATUS" },
	{ 0x180000, 0x043c, 0, 0, 8, 1, "DSP0_CTRL_IVP0_DM_STACK_START$" },
	{ 0x180000, 0x0440, 0, 0, 8, 1, "DSP0_CTRL_IVP0_DM_STACK_END$" },
	{ 0x180000, 0x044c, 0, 0, 0, 1, "DSP0_CTRL_IVP0_STATUS" },
	{ 0x180000, 0x0480, 0, 0, 0, 1, "DSP0_CTRL_IVP1_WAKEUP" },
	{ 0x180000, 0x0484, 0, 0, 4, 1, "DSP0_CTRL_IVP1_INTR_STATUS_TH$" },
	{ 0x180000, 0x048c, 0, 0, 4, 1, "DSP0_CTRL_IVP1_INTR_ENABLE_TH$" },
	{ 0x180000, 0x0494, 0, 0, 4, 1, "DSP0_CTRL_IVP1_SWI_SET_TH$" },
	{ 0x180000, 0x049c, 0, 0, 4, 1, "DSP0_CTRL_IVP1_SWI_CLEAR_TH$" },
	{ 0x180000, 0x04a4, 0, 0, 4, 1, "DSP0_CTRL_IVP1_MASKED_STATUS_TH$" },
	{ 0x180000, 0x04ac, 0, 0, 0, 1, "DSP0_CTRL_IVP1_IC_BASE_ADDR" },
	{ 0x180000, 0x04b0, 0, 0, 0, 1, "DSP0_CTRL_IVP1_IC_CODE_SIZE" },
	{ 0x180000, 0x04b4, 0, 0, 0, 1, "DSP0_CTRL_IVP1_IC_INVALID_REQ" },
	{ 0x180000, 0x04b8, 0, 0, 0, 1, "DSP0_CTRL_IVP1_IC_INVALID_STATUS" },
	{ 0x180000, 0x04bc, 0, 0, 8, 1, "DSP0_CTRL_IVP1_DM_STACK_START$" },
	{ 0x180000, 0x04c0, 0, 0, 8, 1, "DSP0_CTRL_IVP1_DM_STACK_END$" },
	{ 0x180000, 0x04cc, 0, 0, 0, 1, "DSP0_CTRL_IVP1_STATUS" },
	{ 0x180000, 0x0800, 0, 0, 0, 1, "DSP0_CTRL_IVP0_MAILBOX_INTR_TH0" },
	{ 0x180000, 0x0804, 0, 0, 4, 1, "DSP0_CTRL_IVP0_MAILBOX_TH0_$" },
	{ 0x180000, 0x0a00, 0, 0, 0, 1, "DSP0_CTRL_IVP0_MAILBOX_INTR_TH1" },
	{ 0x180000, 0x0a04, 0, 0, 4, 1, "DSP0_CTRL_IVP0_MAILBOX_TH1_$" },
	{ 0x180000, 0x0c00, 0, 0, 0, 1, "DSP0_CTRL_IVP1_MAILBOX_INTR_TH0" },
	{ 0x180000, 0x0c04, 0, 0, 4, 1, "DSP0_CTRL_IVP1_MAILBOX_TH0_$" },
	{ 0x180000, 0x0e00, 0, 0, 0, 1, "DSP0_CTRL_IVP1_MAILBOX_INTR_TH1" },
	{ 0x180000, 0x0e04, 0, 0, 4, 1, "DSP0_CTRL_IVP1_MAILBOX_TH1_$" },
	{ 0x180000, 0x1000, 0, 0, 4, 1, "DSP0_CTRL_IVP0_MSG_TH0_$" },
	{ 0x180000, 0x1400, 0, 0, 4, 1, "DSP0_CTRL_IVP0_MSG_TH1_$" },
	{ 0x180000, 0x1800, 0, 0, 4, 1, "DSP0_CTRL_IVP1_MSG_TH0_$" },
	{ 0x180000, 0x1c00, 0, 0, 4, 1, "DSP0_CTRL_IVP1_MSG_TH1_$" },
};

static struct npu_reg_format cpu_sfr_reg[] = {
	/* REG_CPU_SS */
	{ 0xE0000, 0x0000, 0, 0, 0, 1, "DNC_CPU_REMAPS0_NS" },
	{ 0xE0000, 0x0004, 1, 0, 0, 1, "DNC_CPU_REMAPS1" },
	{ 0xE0000, 0x0008, 0, 0, 0, 1, "DNC_CPU_REMAPD0_NS" },
	{ 0xE0000, 0x000c, 1, 0, 0, 1, "DNC_CPU_REMAPD1" },
	{ 0xE0000, 0x0010, 0, 0, 0, 1, "DNC_CPU_RUN" },
	{ 0xE0000, 0x0024, 0, 0, 0, 1, "DNC_CPU_EVENT" },
	{ 0xE0000, 0x0040, 1, 0, 0, 1, "DNC_CPU_RELEASE" },
	{ 0xE0000, 0x0044, 0, 0, 0, 1, "DNC_CPU_RELEASE_NS" },
	{ 0xE0000, 0x0048, 0, 0, 0, 1, "DNC_CPU_CFGEND" },
	{ 0xE0000, 0x004C, 0, 0, 0, 1, "DNC_CPU_CFGTE" },
	{ 0xE0000, 0x0050, 0, 0, 0, 1, "DNC_CPU_VINITHI" },
	{ 0xE0000, 0x00C0, 0, 0, 0, 1, "DNC_CPU_EVENT_STATUS" },
	{ 0xE0000, 0x00C4, 0, 0, 0, 1, "DNC_CPU_WFI_STATUS" },
	{ 0xE0000, 0x00C8, 0, 0, 0, 1, "DNC_CPU_WFE_STATUS" },
	{ 0xE0000, 0x00CC, 0, 0, 0, 1, "DNC_CPU_PC_L" },
	{ 0xE0000, 0x00D0, 0, 0, 0, 1, "DNC_CPU_PC_H" },
	{ 0xE0000, 0x00D4, 0, 0, 0, 1, "DNC_CPU_ACTIVE_CNT_L" },
	{ 0xE0000, 0x00D8, 0, 0, 0, 1, "DNC_CPU_ACTIVE_CNT_H" },
	{ 0xE0000, 0x00DC, 0, 0, 0, 1, "DNC_CPU_STALL_CNT" },
	{ 0xE0000, 0x011C, 0, 0, 0, 1, "DNC_CPU_ALIVE_CTRL" },
};
#endif

u32 npu_read_hw_reg(const struct npu_iomem_area *base, u32 offset, u32 mask, int silent)
{
	volatile u32 v;
	void __iomem *reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->vaddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->vaddr + offset;
	v = readl(reg_addr);

	if (!silent)
		npu_dbg("setting register pa(0x%08x) va(%p) cur(0x%08x(raw 0x%08x)) mask(0x%08x)\n",
			base->paddr + offset, reg_addr, (v & mask), v, mask);

	return (v & mask);
}

void npu_write_hw_reg(const struct npu_iomem_area *base, u32 offset, u32 val, u32 mask, int silent)
{
	volatile u32 v;
	void __iomem *reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->vaddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->vaddr + offset;
	v = readl(reg_addr);
	if (!silent)
		npu_dbg("setting register pa(0x%08x) va(%p) cur(0x%08x) val(0x%08x) mask(0x%08x)\n",
			base->paddr + offset, reg_addr,	v, val, mask);

	v = (v & (~mask)) | (val & mask);
	writel(v, (void *)(reg_addr));
	npu_dbg("written (0x%08x) at (%p)\n", v, reg_addr);
}

u32 npu_smc_read_hw_reg(const struct npu_iomem_area *base,
						u32 offset, u32 mask, int silent)
{
	unsigned long v;
	u32 reg_addr;

	if (!base || !base->paddr) {
		npu_err("base or base->paddr is NULL\n");
		return 0;
	}

	if (offset > base->size) {
		npu_err("offset(%u) > size(%llu), starting at (%u)\n",
					offset, base->size, base->paddr);
		return 0;
	}
	reg_addr = base->paddr + offset;
	if (!silent)
		npu_dbg("calling smc pa(0x%08x) va(%u) mask(0x%08x)\n",
			base->paddr + offset, reg_addr,	mask);

	if (!exynos_smc_readsfr((unsigned long)reg_addr, &v))
		return (v & mask);

	return 0;
}

void npu_smc_write_hw_reg(const struct npu_iomem_area *base,
						u32 offset, u32 val, u32 mask, int silent)
{
	volatile u32 v;
	u32 reg_addr;

	BUG_ON(!base);
	BUG_ON(!base->paddr);

	if (offset > base->size) {
		npu_err("offset(%u) exceeds iomem region size(%llu), starting at (%u)\n",
				offset, base->size, base->paddr);
		BUG_ON(1);
	}
	reg_addr = base->paddr + offset;

	v = val & mask;

	if (!silent)
		npu_dbg("calling smc pa(0x%08x) va(%u) val(0x%08x) mask(0x%08x) write(0x%08x)\n",
			base->paddr + offset, reg_addr, val, mask, v);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)reg_addr), v, 0x0);

	npu_dbg("smc (0x%08x) at (%u)\n", v, reg_addr);
}

/*
 * Set the set of register value specified in set_map,
 * with the base specified at base.
 * if regset_mdelay != 0, it will be the delay between register set.
 */
int npu_set_hw_reg(const struct npu_iomem_area *base, const struct reg_set_map *set_map,
		      size_t map_len, int regset_mdelay)
{
	size_t i;

	BUG_ON(!base);
	BUG_ON(!set_map);
	BUG_ON(!base->vaddr);

	for (i = 0; i < map_len; ++i) {
		npu_write_hw_reg(base, set_map[i].offset, set_map[i].val, set_map[i].mask, 0);
		/* Insert delay between register setting */
		if (regset_mdelay > 0)
			mdelay(regset_mdelay);
	}
	return 0;
}

int npu_set_hw_reg_2(const struct reg_set_map_2 *set_map, size_t map_len, int regset_mdelay)
{
	size_t i;

	BUG_ON(!set_map);

	for (i = 0; i < map_len; ++i) {
		npu_write_hw_reg(set_map[i].iomem_area, set_map[i].offset, set_map[i].val, set_map[i].mask, 0);

		/* Insert delay between register setting */
		if (regset_mdelay > 0)
			mdelay(regset_mdelay);
	}
	return 0;
}

/* dt_name has prefix "samsung,npucmd-" */
#define NPU_REG_CMD_PREFIX	"samsung,npucmd-"
int npu_get_reg_cmd_map(struct npu_system *system, struct reg_cmd_list *cmd_list)
{
	int i, ret = 0;
	u32 *cmd_data = NULL;
	char sfr_name[32], data_name[32];
	char **cmd_sfr;
	struct device *dev;

	sfr_name[0] = '\0';
	data_name[0] = '\0';
	strcpy(sfr_name, NPU_REG_CMD_PREFIX);
	strcpy(data_name, NPU_REG_CMD_PREFIX);
	strcat(strcat(sfr_name, cmd_list->name), "-sfr");
	strcat(strcat(data_name, cmd_list->name), "-data");

	dev = &(system->pdev->dev);
	cmd_list->count = of_property_count_strings(
			dev->of_node, sfr_name);
	if (cmd_list->count <= 0) {
		probe_err("invalid reg_cmd list by %s\n", sfr_name);
		cmd_list->list = NULL;
		cmd_list->count = 0;
		ret = -ENODEV;
		goto err_exit;
	}
	probe_info("%s register %d commands\n", sfr_name, cmd_list->count);

	cmd_list->list = (struct reg_cmd_map *)devm_kmalloc(dev,
				(cmd_list->count + 1) * sizeof(struct reg_cmd_map),
				GFP_KERNEL);
	if (!cmd_list->list) {
		probe_err("failed to alloc for cmd map\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	(cmd_list->list)[cmd_list->count].name = NULL;

	cmd_sfr = (char **)devm_kmalloc(dev,
			cmd_list->count * sizeof(char *), GFP_KERNEL);
	if (!cmd_sfr) {
		probe_err("failed to alloc for reg_cmd sfr for %s\n", sfr_name);
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = of_property_read_string_array(dev->of_node, sfr_name, (const char **)cmd_sfr, cmd_list->count);
	if (ret < 0) {
		probe_err("failed to get reg_cmd for %s (%d)\n", sfr_name, ret);
		ret = -EINVAL;
		goto err_sfr;
	}

	cmd_data = (u32 *)devm_kmalloc(dev,
			cmd_list->count * NPU_REG_CMD_LEN * sizeof(u32), GFP_KERNEL);
	if (!cmd_data) {
		probe_err("failed to alloc for reg_cmd data for %s\n", data_name);
		ret = -ENOMEM;
		goto err_sfr;
	}
	ret = of_property_read_u32_array(dev->of_node, data_name, (u32 *)cmd_data,
			cmd_list->count * NPU_REG_CMD_LEN);
	if (ret) {
		probe_err("failed to get reg_cmd for %s (%d)\n", data_name, ret);
		ret = -EINVAL;
		goto err_data;
	}

	for (i = 0; i < cmd_list->count; i++) {
		(*(cmd_list->list + i)).name = *(cmd_sfr + i);
		memcpy((void *)(&(*(cmd_list->list + i)).cmd),
				(void *)(cmd_data + (i * NPU_REG_CMD_LEN)),
				sizeof(u32) * NPU_REG_CMD_LEN);
		probe_info("copy %s cmd (%lu)\n", *(cmd_sfr + i), sizeof(u32) * NPU_REG_CMD_LEN);
	}
err_data:
	devm_kfree(dev, cmd_data);
err_sfr:
	devm_kfree(dev, cmd_sfr);
err_exit:
	return ret;
}

int npu_cmd_sfr(struct npu_system *system, const struct reg_cmd_list *cmd_list, int silent)
{
	int ret = 0;
	size_t i;
	struct reg_cmd_map *t;
	struct npu_iomem_area *sfrmem;

	BUG_ON(!system);

	if (!cmd_list) {
		npu_dbg("No cmd for sfr\n");
		return 0;
	}

	for (i = 0; i < cmd_list->count; ++i) {
		t = (cmd_list->list + i);
		sfrmem = npu_get_io_area(system, t->name);
		if (t->name) {
			switch (t->cmd) {
			case NPU_CMD_SFR_READ:
				ret = (int)npu_read_hw_reg(sfrmem,
							t->offset, t->mask, silent);
				if (!silent)
					npu_info("%s+0x%08x : 0x%08x\n",
							t->name, t->offset, ret);
				break;
			case NPU_CMD_SFR_WRITE:
				npu_write_hw_reg(sfrmem, t->offset, t->val, t->mask, silent);
				break;
			case NPU_CMD_SFR_SMC_READ:
				ret = (int)npu_smc_read_hw_reg(sfrmem,
						t->offset, t->mask, silent);
				if (!silent)
					npu_info("%s+0x%08x : 0x%08x\n",
							t->name, t->offset, ret);
				break;
			case NPU_CMD_SFR_SMC_WRITE:
				npu_smc_write_hw_reg(sfrmem,
						t->offset, t->val, t->mask, silent);
				break;
			default:
				break;
			}
		} else
				break;
		/* Insert delay between register setting */
		if (t->mdelay) {
			npu_dbg("%s %d mdelay\n", t->name, t->mdelay);
			mdelay(t->mdelay);
		}
	}
	return ret;
}

int npu_cmd_map(struct npu_system *system, const char *cmd_name)
{
	BUG_ON(!system);
	BUG_ON(!cmd_name);

	return npu_cmd_sfr(system, (const struct reg_cmd_list *)get_npu_cmd_map(system, cmd_name), 0);
}

int npu_set_sfr(const u32 sfr_addr, const u32 value, const u32 mask)
{
	int			ret;
	void __iomem		*iomem = NULL;
	struct npu_iomem_area	area_info;	/* Save iomem result */

	iomem = ioremap(sfr_addr, sizeof(u32));
	if (IS_ERR_OR_NULL(iomem)) {
		probe_err("fail(%p) in ioremap(0x%08x)\n",
			  iomem, sfr_addr);
		ret = -EFAULT;
		goto err_exit;
	}
	area_info.vaddr = iomem;
	area_info.paddr = sfr_addr;
	area_info.size = sizeof(u32);

	npu_write_hw_reg(&area_info, 0, value, mask, 0);

	ret = 0;
err_exit:
	if (iomem)
		iounmap(iomem);

	return ret;
}

int npu_get_sfr(const u32 sfr_addr)
{
	int			ret;
	void __iomem		*iomem = NULL;
	struct npu_iomem_area	area_info;	/* Save iomem result */
	volatile u32 v;
	void __iomem *reg_addr;

	iomem = ioremap(sfr_addr, sizeof(u32));
	if (IS_ERR_OR_NULL(iomem)) {
		probe_err("fail(%p) in ioremap(0x%08x)\n",
			  iomem, sfr_addr);
		ret = -EFAULT;
		goto err_exit;
	}
	area_info.vaddr = iomem;
	area_info.paddr = sfr_addr;
	area_info.size = sizeof(u32);

	reg_addr = area_info.vaddr;

	v = readl(reg_addr);
	npu_trace("get_sfr, vaddr(0x%p), paddr(0x%08x), val(0x%x)\n",
		area_info.vaddr, area_info.paddr, v);

	ret = 0;
err_exit:
	if (iomem)
		iounmap(iomem);

	return ret;
}

#ifdef CONFIG_NPU_USE_HW_DEVICE
void npu_reg_dump(struct npu_device *device)
{
	int i, j;
	u32 val = 0;
	struct npu_system *system;

	system = &device->system;
	if (!system)
		return;

	for (i = 0; i < ((sizeof(cpu_sfr_reg)) / (sizeof(struct npu_reg_format))); i++) {
		npu_info("sfr dump: %s\n", cpu_sfr_reg[i].name);

		if (cpu_sfr_reg[i].flag)
			val = npu_smc_read_hw_reg(npu_get_io_area(system, "sfrdnc"),
					cpu_sfr_reg[i].base + cpu_sfr_reg[i].offset, 0xFFFFFFFF, 1);
		else
			val = npu_read_hw_reg(npu_get_io_area(system, "sfrdnc"),
					cpu_sfr_reg[i].base + cpu_sfr_reg[i].offset, 0xFFFFFFFF, 1);

		cpu_sfr_reg[i].sfr = vmalloc(cpu_sfr_reg[i].size);
		if (likely(cpu_sfr_reg[i].sfr)) {
			*cpu_sfr_reg[i].sfr = val;
		} else {
			npu_err("fail in memory allocation\n");
		}
	}

	for (i = 0; i < ((sizeof(dsp0_ctrl_sfr_reg)) / (sizeof(struct npu_reg_format))); i++) {
		npu_info("sfr dump: %s\n", dsp0_ctrl_sfr_reg[i].name);

		val = npu_read_hw_reg(npu_get_io_area(system, "sfrdnc"),
				dsp0_ctrl_sfr_reg[i].base + dsp0_ctrl_sfr_reg[i].offset, 0xFFFFFFFF, 1);

		dsp0_ctrl_sfr_reg[i].sfr = vmalloc(dsp0_ctrl_sfr_reg[i].size);
		if (likely(dsp0_ctrl_sfr_reg[i].sfr)) {
			*dsp0_ctrl_sfr_reg[i].sfr = val;
		} else {
			npu_err("fail in memory allocation\n");
		}
	}

	for (i = 0; i < ((sizeof(npu_dsp_sfr_reg)) / (sizeof(struct npu_reg_format))); i++) {
		npu_dsp_sfr_reg[i].sfr = vmalloc(npu_dsp_sfr_reg[i].size);
		npu_info("sfr dump: %s\n", npu_dsp_sfr_reg[i].name);

		for (j = 0; j < (npu_dsp_sfr_reg[i].size / 0x4); j++) {
			val = npu_read_hw_reg(npu_get_io_area(system, "sfrdnc"),
				npu_dsp_sfr_reg[i].base + (0x4 * j), 0xFFFFFFFF, 1);

			if (likely(npu_dsp_sfr_reg[i].sfr)) {
				*npu_dsp_sfr_reg[i].sfr = val;
				npu_dsp_sfr_reg[i].sfr++;
			} else {
				npu_err("fail in memory allocation\n");
			}
		}
	}
}
#endif
