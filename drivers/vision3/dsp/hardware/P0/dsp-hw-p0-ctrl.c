// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/delay.h>
#if defined(CONFIG_EXYNOS_DSP_HW_P0)
#include <soc/samsung/exynos-smc.h>
#endif
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-dump.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-reg.h"
#include "dsp-hw-p0-dump.h"
#include "dsp-hw-p0-memory.h"
#include "dsp-hw-common-ctrl.h"
#include "dsp-hw-p0-ctrl.h"

#if defined(CONFIG_EXYNOS_DSP_HW_P0) && !defined(ENABLE_DSP_VELOCE)
#define ENABLE_SECURE_READ
#define ENABLE_SECURE_WRITE
#endif

static const struct dsp_reg_format p0_sfr_reg[] = {
	/* REG_DNC_CTRL_NS */
	{ 0x20000, 0x0000, 0, 0, 0, "DNCC_NS_VERSION" },
	{ 0x20000, 0x0008, 0, 0, 0, "DNCC_NS_CLOCK_CONFIG" },
	{ 0x20000, 0x000C, 0, 0, 0, "DNCC_NS_CPU_AxUSER_AxCACHE" },
	{ 0x20000, 0x0010, 0, 0, 0, "DNCC_NS_HOST_BUS_RESP_ENABLE" },
	{ 0x20000, 0x0014, 0, 0, 0, "DNCC_NS_WDT_CNT_EN" },
	{ 0x20000, 0x0018, 0, 0, 0, "DNCC_NS_STM_CTRL" },
	{ 0x20000, 0x001C, 0, 0, 4, "DNCC_NS_STM_FUNC$" },
	{ 0x20000, 0x0074, 0, 0, 0, "DNCC_NS_OTF_BASE_ADDR" },
	{ 0x20000, 0x0078, 0, 0, 0, "DNCC_NS_OTF_STRIDE" },
	{ 0x20000, 0x007C, 0, 0, 0, "DNCC_NS_OTF_FORMAT" },
	{ 0x20000, 0x0080, 0, 0, 0, "DNCC_NS_OTF_STATUS" },
	{ 0x20000, 0x0200, 0, 0, 0, "DNCC_NS_DBG_ENABLE" },
	{ 0x20000, 0x0204, 0, 0, 0, "DNCC_NS_DBG_REG0" },
	{ 0x20000, 0x0208, 0, 0, 0, "DNCC_NS_DBG_REG1" },
	{ 0x20000, 0x020C, 0, 0, 0, "DNCC_NS_DBG_REG2" },
	{ 0x20000, 0x0210, 0, 0, 0, "DNCC_NS_DBG_REG3" },
	{ 0x20000, 0x0400, 0, 0, 0, "DNCC_NS_STATUS_INT_BUS_SECURE_ERROR_RD" },
	{ 0x20000, 0x0404, 0, 0, 0, "DNCC_NS_STATUS_INT_BUS_SECURE_ERROR_WR" },
	{ 0x20000, 0x0408, 0, 0, 0, "DNCC_NS_STATUS_INT_BUS_ERROR_RD" },
	{ 0x20000, 0x040C, 0, 0, 0, "DNCC_NS_STATUS_INT_BUS_ERROR_WR" },
	{ 0x20000, 0x0410, 0, 0, 4, "DNCC_NS_STATUS_MOCK$" },
	{ 0x20000, 0x0800, 0, 0, 0, "DNCC_NS_IRQ_DBG_STATUS_FR_PBOX" },
	{ 0x20000, 0x0804, 0, 0, 0, "DNCC_NS_IRQ_DBG_MSTATUS_FR_PBOX" },
	{ 0x20000, 0x0808, 0, 0, 0, "DNCC_NS_IRQ_DBG_ENABLE_FR_PBOX" },
	{ 0x20000, 0x080C, 0, 0, 0, "DNCC_NS_IRQ_DBG_CLR_FR_PBOX" },
	{ 0x20000, 0x0810, 0, 0, 0, "DNCC_NS_IRQ_STATUS_TO_CC" },
	{ 0x20000, 0x0814, 0, 0, 0, "DNCC_NS_IRQ_MSTATUS_TO_CC" },
	{ 0x20000, 0x0818, 0, 0, 0, "DNCC_NS_IRQ_ENABLE_TO_CC" },
	{ 0x20000, 0x081C, 0, 0, 0, "DNCC_NS_IRQ_SET_TO_CC" },
	{ 0x20000, 0x0820, 0, 0, 0, "DNCC_NS_IRQ_CLR_TO_CC" },
	{ 0x20000, 0x0824, 0, 0, 0, "DNCC_NS_IRQ_STATUS_TO_HOST" },
	{ 0x20000, 0x0828, 0, 0, 0, "DNCC_NS_IRQ_MSTATUS_TO_HOST" },
	{ 0x20000, 0x082C, 0, 0, 0, "DNCC_NS_IRQ_ENABLE_TO_HOST" },
	{ 0x20000, 0x0830, 0, 0, 0, "DNCC_NS_IRQ_SET_TO_HOST" },
	{ 0x20000, 0x0834, 0, 0, 0, "DNCC_NS_IRQ_CLR_TO_HOST" },
	{ 0x20000, 0x0838, 0, 0, 0, "DNCC_NS_IRQ_STATUS_FR_CC_TO_GNPU0" },
	{ 0x20000, 0x083C, 0, 0, 0, "DNCC_NS_IRQ_MSTATUS_FR_CC_TO_GNPU0" },
	{ 0x20000, 0x0840, 0, 0, 0, "DNCC_NS_IRQ_ENABLE_FR_CC_TO_GNPU0" },
	{ 0x20000, 0x0844, 0, 0, 0, "DNCC_NS_IRQ_SET_FR_CC_TO_GNPU0" },
	{ 0x20000, 0x0848, 0, 0, 0, "DNCC_NS_IRQ_CLR_FR_CC_TO_GNPU0" },
	{ 0x20000, 0x084C, 0, 0, 0, "DNCC_NS_IRQ_STATUS_FR_CC_TO_GNPU1" },
	{ 0x20000, 0x0850, 0, 0, 0, "DNCC_NS_IRQ_MSTATUS_FR_CC_TO_GNPU1" },
	{ 0x20000, 0x0854, 0, 0, 0, "DNCC_NS_IRQ_ENABLE_FR_CC_TO_GNPU1" },
	{ 0x20000, 0x0858, 0, 0, 0, "DNCC_NS_IRQ_SET_FR_CC_TO_GNPU1" },
	{ 0x20000, 0x085C, 0, 0, 0, "DNCC_NS_IRQ_CLR_FR_CC_TO_GNPU1" },
	{ 0x20000, 0x0860, 0, 0, 0, "DNCC_NS_IRQ_STATUS_FR_GNPU_TO_CC" },
	{ 0x20000, 0x0864, 0, 0, 0, "DNCC_NS_IRQ_MSTATUS_FR_GNPU_TO_CC" },
	{ 0x20000, 0x0868, 0, 0, 0, "DNCC_NS_IRQ_ENABLE_FR_GNPU_TO_CC" },
	{ 0x20000, 0x086C, 0, 0, 0, "DNCC_NS_IRQ_SET_FR_GNPU_TO_CC" },
	{ 0x20000, 0x0870, 0, 0, 0, "DNCC_NS_IRQ_CLR_FR_GNPU_TO_CC" },
	{ 0x20000, 0x0874, 0, 0, 0, "DNCC_NS_IRQ_DBG_STATUS_TO_SRESETN" },
	{ 0x20000, 0x0878, 0, 0, 0, "DNCC_NS_IRQ_DBG_MSTATUS_TO_SRESETN" },
	{ 0x20000, 0x087C, 0, 0, 0, "DNCC_NS_IRQ_DBG_ENABLE_TO_SRESETN" },
	{ 0x20000, 0x0880, 0, 0, 0, "DNCC_NS_IRQ_DBG_CLR_TO_SRESETN" },
	{ 0x20000, 0x1000, 0, 0, 0, "DNCC_NS_IRQ_MBOX_ENABLE_FR_CC" },
	{ 0x20000, 0x1004, 0, 0, 0, "DNCC_NS_IRQ_MBOX_ENABLE_TO_CC" },
	{ 0x20000, 0x1008, 0, 0, 0, "DNCC_NS_IRQ_MBOX_STATUS_FR_CC" },
	{ 0x20000, 0x100C, 0, 0, 0, "DNCC_NS_IRQ_MBOX_STATUS_TO_CC" },
	{ 0x20000, 0x1010, 0, 0, 0, "DNCC_NS_IRQ_MBOX_MSTATUS_FR_CC" },
	{ 0x20000, 0x1014, 0, 0, 0, "DNCC_NS_IRQ_MBOX_MSTATUS_TO_CC" },
	{ 0x20000, 0x1018, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_HOST_INTR" },
	{ 0x20000, 0x101C, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_HOST$" },
	{ 0x20000, 0x103C, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_ABOX_INTR" },
	{ 0x20000, 0x1040, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_ABOX$" },
	{ 0x20000, 0x1060, 0, 0, 0, "DNCC_NS_MBOX_FR_HOST_TO_CC_INTR" },
	{ 0x20000, 0x1064, 0, 0, 4, "DNCC_NS_MBOX_FR_HOST_TO_CC$" },
	{ 0x20000, 0x10A4, 0, 0, 0, "DNCC_NS_MBOX_FR_ABOX_TO_CC_INTR" },
	{ 0x20000, 0x10A8, 0, 0, 4, "DNCC_NS_MBOX_FR_ABOX_TO_CC$" },
	{ 0x20000, 0x10C8, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP0_TH0_TO_CC_INTR" },
	{ 0x20000, 0x10CC, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP0_TH0_TO_CC$" },
	{ 0x20000, 0x10EC, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP0_TH1_TO_CC_INTR" },
	{ 0x20000, 0x10F0, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP0_TH1_TO_CC$" },
	{ 0x20000, 0x1110, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP1_TH0_TO_CC_INTR" },
	{ 0x20000, 0x1114, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP1_TH0_TO_CC$" },
	{ 0x20000, 0x1134, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP1_TH1_TO_CC_INTR" },
	{ 0x20000, 0x1138, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP1_TH1_TO_CC$" },
	{ 0x20000, 0x1158, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP2_TH0_TO_CC_INTR" },
	{ 0x20000, 0x115C, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP2_TH0_TO_CC$" },
	{ 0x20000, 0x117C, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP2_TH1_TO_CC_INTR" },
	{ 0x20000, 0x1180, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP2_TH1_TO_CC$" },
	{ 0x20000, 0x11A0, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP3_TH0_TO_CC_INTR" },
	{ 0x20000, 0x11A4, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP3_TH0_TO_CC$" },
	{ 0x20000, 0x11C4, 0, 0, 0, "DNCC_NS_MBOX_FR_IVP3_TH1_TO_CC_INTR" },
	{ 0x20000, 0x11C8, 0, 0, 4, "DNCC_NS_MBOX_FR_IVP3_TH1_TO_CC$" },
	{ 0x20000, 0x11E8, 0, 0, 0, "DNCC_NS_MBOX_FR_GNPU0_TO_CC_INTR" },
	{ 0x20000, 0x11EC, 0, 0, 4, "DNCC_NS_MBOX_FR_GNPU0_TO_CC$" },
	{ 0x20000, 0x120C, 0, 0, 0, "DNCC_NS_MBOX_FR_GNPU1_TO_CC_INTR" },
	{ 0x20000, 0x1210, 0, 0, 4, "DNCC_NS_MBOX_FR_GNPU1_TO_CC$" },
	{ 0x20000, 0x1230, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_0_INTR" },
	{ 0x20000, 0x1234, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_0" },
	{ 0x20000, 0x1254, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_1_INTR" },
	{ 0x20000, 0x1258, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_1" },
	{ 0x20000, 0x1278, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_2_INTR" },
	{ 0x20000, 0x127C, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_2" },
	{ 0x20000, 0x129C, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_3_INTR" },
	{ 0x20000, 0x12A0, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU0_3" },
	{ 0x20000, 0x12C0, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_0_INTR" },
	{ 0x20000, 0x12C4, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_0" },
	{ 0x20000, 0x12E4, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_1_INTR" },
	{ 0x20000, 0x12E8, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_1" },
	{ 0x20000, 0x1308, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_2_INTR" },
	{ 0x20000, 0x130C, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_2" },
	{ 0x20000, 0x132C, 0, 0, 0, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_3_INTR" },
	{ 0x20000, 0x1330, 0, 0, 4, "DNCC_NS_MBOX_FR_CC_TO_GNPU1_3" },
	/* REG_CPU_SS */
	{ 0xE0000, 0x0000, 0, 0, 0, "DNC_CPU_REMAPS0_NS" },
	{ 0xE0000, 0x0004, REG_SEC_RW, 0, 0, "DNC_CPU_REMAPS1" },
	{ 0xE0000, 0x0008, 0, 0, 0, "DNC_CPU_REMAPD0_NS" },
	{ 0xE0000, 0x000c, REG_SEC_RW, 0, 0, "DNC_CPU_REMAPD1" },
	{ 0xE0000, 0x0010, 0, 0, 0, "DNC_CPU_RUN" },
	{ 0xE0000, 0x0024, 0, 0, 0, "DNC_CPU_EVENT" },
	{ 0xE0000, 0x0040, REG_SEC_RW, 0, 0, "DNC_CPU_RELEASE" },
	{ 0xE0000, 0x0044, 0, 0, 0, "DNC_CPU_RELEASE_NS" },
	{ 0xE0000, 0x0048, 0, 0, 0, "DNC_CPU_CFGEND" },
	{ 0xE0000, 0x004C, 0, 0, 0, "DNC_CPU_CFGTE" },
	{ 0xE0000, 0x0050, 0, 0, 0, "DNC_CPU_VINITHI" },
	{ 0xE0000, 0x00C0, 0, 0, 0, "DNC_CPU_EVENT_STATUS" },
	{ 0xE0000, 0x00C4, 0, 0, 0, "DNC_CPU_WFI_STATUS" },
	{ 0xE0000, 0x00C8, 0, 0, 0, "DNC_CPU_WFE_STATUS" },
	{ 0xE0000, 0x00CC, 0, 0, 0, "DNC_CPU_PC_L" },
	{ 0xE0000, 0x00D0, 0, 0, 0, "DNC_CPU_PC_H" },
	{ 0xE0000, 0x00D4, 0, 0, 0, "DNC_CPU_ACTIVE_CNT_L" },
	{ 0xE0000, 0x00D8, 0, 0, 0, "DNC_CPU_ACTIVE_CNT_H" },
	{ 0xE0000, 0x00DC, 0, 0, 0, "DNC_CPU_STALL_CNT" },
	{ 0xE0000, 0x011C, 0, 0, 0, "DNC_CPU_ALIVE_CTRL" },
	/* REG_DSP0_CTRL */
	{ 0x180000, 0x0000, 0, 0, 0, "DSP0_CTRL_BUSACTREQ" },
	{ 0x180000, 0x0004, 0, 0, 0, "DSP0_CTRL_SWRESET" },
	{ 0x180000, 0x0008, 0, 0, 0, "DSP0_CTRL_CORE_ID" },
	{ 0x180000, 0x0010, 0, 0, 0, "DSP0_CTRL_PERF_MON_ENABLE" },
	{ 0x180000, 0x0014, 0, 0, 0, "DSP0_CTRL_PERF_MON_CLEAR" },
	{ 0x180000, 0x0018, 0, 0, 0, "DSP0_CTRL_DBG_MON_ENABLE" },
	{ 0x180000, 0x0020, 0, 0, 0, "DSP0_CTRL_DBG_INTR_STATUS" },
	{ 0x180000, 0x0024, 0, 0, 0, "DSP0_CTRL_DBG_INTR_ENABLE" },
	{ 0x180000, 0x0028, 0, 0, 0, "DSP0_CTRL_DBG_INTR_CLEAR" },
	{ 0x180000, 0x002C, 0, 0, 0, "DSP0_CTRL_DBG_INTR_MSTATUS" },
	{ 0x180000, 0x0030, 0, 0, 0, "DSP0_CTRL_IVP_SFR2AXI_SGMO" },
	{ 0x180000, 0x0038, 0, 0, 0, "DSP0_CTRL_SRESET_DONE_STATUS" },
	{ 0x180000, 0x0040, 0, 0, 8, "DSP0_CTRL_VM_STATCK_START$" },
	{ 0x180000, 0x0044, 0, 0, 8, "DSP0_CTRL_VM_STATCK_END$" },
	{ 0x180000, 0x0060, 0, 0, 0, "DSP0_CTRL_VM_MODE" },
	{ 0x180000, 0x0080, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH0_PC" },
	{ 0x180000, 0x0084, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH0_VALID_CNTL" },
	{ 0x180000, 0x0088, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH0_VALID_CNTH" },
	{ 0x180000, 0x008C, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH0_STALL_CNT" },
	{ 0x180000, 0x0090, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH1_PC" },
	{ 0x180000, 0x0094, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH1_VALID_CNTL" },
	{ 0x180000, 0x0098, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH1_VALID_CNTH" },
	{ 0x180000, 0x009C, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_TH1_STALL_CNT" },
	{ 0x180000, 0x00A0, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_IC_REQL" },
	{ 0x180000, 0x00A4, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_IC_REQH" },
	{ 0x180000, 0x00A8, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_IC_MISS" },
	{ 0x180000, 0x00AC, 0, 0, 8, "DSP0_CTRL_PERF_IVP0_INST_CNTL$" },
	{ 0x180000, 0x00B0, 0, 0, 8, "DSP0_CTRL_PERF_IVP0_INST_CNTH$" },
	{ 0x180000, 0x00CC, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH0_PC" },
	{ 0x180000, 0x00D0, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH0_VALID_CNTL" },
	{ 0x180000, 0x00D4, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH0_VALID_CNTH" },
	{ 0x180000, 0x00D8, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH0_STALL_CNT" },
	{ 0x180000, 0x00DC, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH1_PC" },
	{ 0x180000, 0x00E0, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH1_VALID_CNTL" },
	{ 0x180000, 0x00E4, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH1_VALID_CNTH" },
	{ 0x180000, 0x00E8, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_TH1_STALL_CNT" },
	{ 0x180000, 0x00EC, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_IC_REQL" },
	{ 0x180000, 0x00F0, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_IC_REQH" },
	{ 0x180000, 0x00F4, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_IC_MISS" },
	{ 0x180000, 0x00F8, 0, 0, 8, "DSP0_CTRL_PERF_IVP1_INST_CNTL$" },
	{ 0x180000, 0x00FC, 0, 0, 8, "DSP0_CTRL_PERF_IVP1_INST_CNTH$" },
	{ 0x180000, 0x0128, 0, 0, 0, "DSP0_CTRL_DBG_IVP0_ADDR_PM" },
	{ 0x180000, 0x012c, 0, 0, 0, "DSP0_CTRL_DBG_IVP0_ADDR_DM" },
	{ 0x180000, 0x0130, 0, 0, 0, "DSP0_CTRL_DBG_IVP0_ERROR_INFO" },
	{ 0x180000, 0x0154, 0, 0, 0, "DSP0_CTRL_DBG_IVP1_ADDR_PM" },
	{ 0x180000, 0x0158, 0, 0, 0, "DSP0_CTRL_DBG_IVP1_ADDR_DM" },
	{ 0x180000, 0x015c, 0, 0, 0, "DSP0_CTRL_DBG_IVP1_ERROR_INFO" },
	{ 0x180000, 0x0180, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_STALL_CNTL" },
	{ 0x180000, 0x0184, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_STALL_CNTH" },
	{ 0x180000, 0x0188, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_SFR_STALL_CNTL" },
	{ 0x180000, 0x018c, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_SFR_STALL_CNTH" },
	{ 0x180000, 0x0190, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_VM0_STALL_CNTL" },
	{ 0x180000, 0x0194, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_VM0_STALL_CNTH" },
	{ 0x180000, 0x0198, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_VM1_STALL_CNTL" },
	{ 0x180000, 0x019c, 0, 0, 0, "DSP0_CTRL_PERF_IVP0_VM1_STALL_CNTH" },
	{ 0x180000, 0x01a0, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_STALL_CNTL" },
	{ 0x180000, 0x01a4, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_STALL_CNTH" },
	{ 0x180000, 0x01a8, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_SFR_STALL_CNTL" },
	{ 0x180000, 0x01ac, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_SFR_STALL_CNTH" },
	{ 0x180000, 0x01b0, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_VM0_STALL_CNTL" },
	{ 0x180000, 0x01b4, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_VM0_STALL_CNTH" },
	{ 0x180000, 0x01b8, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_VM1_STALL_CNTL" },
	{ 0x180000, 0x01bc, 0, 0, 0, "DSP0_CTRL_PERF_IVP1_VM1_STALL_CNTH" },
	{ 0x180000, 0x01c8, 0, 0, 4, "DSP0_CTRL_SM_ID$" },
	{ 0x180000, 0x01d8, 0, 0, 4, "DSP0_CTRL_SM_ADDR$" },
	{ 0x180000, 0x01e8, 0, 0, 0, "DSP0_CTRL_IVP0_STM_FUNC_STATUS" },
	{ 0x180000, 0x01ec, 0, 0, 0, "DSP0_CTRL_IVP1_STM_FUNC_STATUS" },
	{ 0x180000, 0x0208, 0, 0, 0, "DSP0_CTRL_AXI_ERROR_RD" },
	{ 0x180000, 0x020c, 0, 0, 0, "DSP0_CTRL_AXI_ERROR_WR" },
	{ 0x180000, 0x0210, 0, 0, 4, "DSP0_CTRL_RD_MOCNT$" },
	{ 0x180000, 0x0280, 0, 0, 4, "DSP0_CTRL_WR_MOCNT$" },
	{ 0x180000, 0x0400, 0, 0, 0, "DSP0_CTRL_IVP0_WAKEUP" },
	{ 0x180000, 0x0404, 0, 0, 4, "DSP0_CTRL_IVP0_INTR_STATUS_TH$" },
	{ 0x180000, 0x040c, 0, 0, 4, "DSP0_CTRL_IVP0_INTR_ENABLE_TH$" },
	{ 0x180000, 0x0414, 0, 0, 4, "DSP0_CTRL_IVP0_SWI_SET_TH$" },
	{ 0x180000, 0x041c, 0, 0, 4, "DSP0_CTRL_IVP0_SWI_CLEAR_TH$" },
	{ 0x180000, 0x0424, 0, 0, 4, "DSP0_CTRL_IVP0_MASKED_STATUS_TH$" },
	{ 0x180000, 0x042c, 0, 0, 0, "DSP0_CTRL_IVP0_IC_BASE_ADDR" },
	{ 0x180000, 0x0430, 0, 0, 0, "DSP0_CTRL_IVP0_IC_CODE_SIZE" },
	{ 0x180000, 0x0434, 0, 0, 0, "DSP0_CTRL_IVP0_IC_INVALID_REQ" },
	{ 0x180000, 0x0438, 0, 0, 0, "DSP0_CTRL_IVP0_IC_INVALID_STATUS" },
	{ 0x180000, 0x043c, 0, 0, 8, "DSP0_CTRL_IVP0_DM_STACK_START$" },
	{ 0x180000, 0x0440, 0, 0, 8, "DSP0_CTRL_IVP0_DM_STACK_END$" },
	{ 0x180000, 0x044c, 0, 0, 0, "DSP0_CTRL_IVP0_STATUS" },
	{ 0x180000, 0x0480, 0, 0, 0, "DSP0_CTRL_IVP1_WAKEUP" },
	{ 0x180000, 0x0484, 0, 0, 4, "DSP0_CTRL_IVP1_INTR_STATUS_TH$" },
	{ 0x180000, 0x048c, 0, 0, 4, "DSP0_CTRL_IVP1_INTR_ENABLE_TH$" },
	{ 0x180000, 0x0494, 0, 0, 4, "DSP0_CTRL_IVP1_SWI_SET_TH$" },
	{ 0x180000, 0x049c, 0, 0, 4, "DSP0_CTRL_IVP1_SWI_CLEAR_TH$" },
	{ 0x180000, 0x04a4, 0, 0, 4, "DSP0_CTRL_IVP1_MASKED_STATUS_TH$" },
	{ 0x180000, 0x04ac, 0, 0, 0, "DSP0_CTRL_IVP1_IC_BASE_ADDR" },
	{ 0x180000, 0x04b0, 0, 0, 0, "DSP0_CTRL_IVP1_IC_CODE_SIZE" },
	{ 0x180000, 0x04b4, 0, 0, 0, "DSP0_CTRL_IVP1_IC_INVALID_REQ" },
	{ 0x180000, 0x04b8, 0, 0, 0, "DSP0_CTRL_IVP1_IC_INVALID_STATUS" },
	{ 0x180000, 0x04bc, 0, 0, 8, "DSP0_CTRL_IVP1_DM_STACK_START$" },
	{ 0x180000, 0x04c0, 0, 0, 8, "DSP0_CTRL_IVP1_DM_STACK_END$" },
	{ 0x180000, 0x04cc, 0, 0, 0, "DSP0_CTRL_IVP1_STATUS" },
	{ 0x180000, 0x0800, 0, 0, 0, "DSP0_CTRL_IVP0_MAILBOX_INTR_TH0" },
	{ 0x180000, 0x0804, 0, 0, 4, "DSP0_CTRL_IVP0_MAILBOX_TH0_$" },
	{ 0x180000, 0x0a00, 0, 0, 0, "DSP0_CTRL_IVP0_MAILBOX_INTR_TH1" },
	{ 0x180000, 0x0a04, 0, 0, 4, "DSP0_CTRL_IVP0_MAILBOX_TH1_$" },
	{ 0x180000, 0x0c00, 0, 0, 0, "DSP0_CTRL_IVP1_MAILBOX_INTR_TH0" },
	{ 0x180000, 0x0c04, 0, 0, 4, "DSP0_CTRL_IVP1_MAILBOX_TH0_$" },
	{ 0x180000, 0x0e00, 0, 0, 0, "DSP0_CTRL_IVP1_MAILBOX_INTR_TH1" },
	{ 0x180000, 0x0e04, 0, 0, 4, "DSP0_CTRL_IVP1_MAILBOX_TH1_$" },
	{ 0x180000, 0x1000, 0, 0, 4, "DSP0_CTRL_IVP0_MSG_TH0_$" },
	{ 0x180000, 0x1400, 0, 0, 4, "DSP0_CTRL_IVP0_MSG_TH1_$" },
	{ 0x180000, 0x1800, 0, 0, 4, "DSP0_CTRL_IVP1_MSG_TH0_$" },
	{ 0x180000, 0x1c00, 0, 0, 4, "DSP0_CTRL_IVP1_MSG_TH1_$" },
	/* REG_DSP1_CTRL */
	{ 0x190000, 0x0000, 0, 0, 0, "DSP1_CTRL_BUSACTREQ" },
	{ 0x190000, 0x0004, 0, 0, 0, "DSP1_CTRL_SWRESET" },
	{ 0x190000, 0x0008, 0, 0, 0, "DSP1_CTRL_CORE_ID" },
	{ 0x190000, 0x0010, 0, 0, 0, "DSP1_CTRL_PERF_MON_ENABLE" },
	{ 0x190000, 0x0014, 0, 0, 0, "DSP1_CTRL_PERF_MON_CLEAR" },
	{ 0x190000, 0x0018, 0, 0, 0, "DSP1_CTRL_DBG_MON_ENABLE" },
	{ 0x190000, 0x0020, 0, 0, 0, "DSP1_CTRL_DBG_INTR_STATUS" },
	{ 0x190000, 0x0024, 0, 0, 0, "DSP1_CTRL_DBG_INTR_ENABLE" },
	{ 0x190000, 0x0028, 0, 0, 0, "DSP1_CTRL_DBG_INTR_CLEAR" },
	{ 0x190000, 0x002C, 0, 0, 0, "DSP1_CTRL_DBG_INTR_MSTATUS" },
	{ 0x190000, 0x0030, 0, 0, 0, "DSP1_CTRL_IVP_SFR2AXI_SGMO" },
	{ 0x190000, 0x0038, 0, 0, 0, "DSP1_CTRL_SRESET_DONE_STATUS" },
	{ 0x190000, 0x0040, 0, 0, 8, "DSP1_CTRL_VM_STATCK_START$" },
	{ 0x190000, 0x0044, 0, 0, 8, "DSP1_CTRL_VM_STATCK_END$" },
	{ 0x190000, 0x0060, 0, 0, 0, "DSP1_CTRL_VM_MODE" },
	{ 0x190000, 0x0080, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH0_PC" },
	{ 0x190000, 0x0084, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH0_VALID_CNTL" },
	{ 0x190000, 0x0088, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH0_VALID_CNTH" },
	{ 0x190000, 0x008C, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH0_STALL_CNT" },
	{ 0x190000, 0x0090, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH1_PC" },
	{ 0x190000, 0x0094, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH1_VALID_CNTL" },
	{ 0x190000, 0x0098, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH1_VALID_CNTH" },
	{ 0x190000, 0x009C, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_TH1_STALL_CNT" },
	{ 0x190000, 0x00A0, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_IC_REQL" },
	{ 0x190000, 0x00A4, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_IC_REQH" },
	{ 0x190000, 0x00A8, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_IC_MISS" },
	{ 0x190000, 0x00AC, 0, 0, 8, "DSP1_CTRL_PERF_IVP0_INST_CNTL$" },
	{ 0x190000, 0x00B0, 0, 0, 8, "DSP1_CTRL_PERF_IVP0_INST_CNTH$" },
	{ 0x190000, 0x00CC, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH0_PC" },
	{ 0x190000, 0x00D0, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH0_VALID_CNTL" },
	{ 0x190000, 0x00D4, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH0_VALID_CNTH" },
	{ 0x190000, 0x00D8, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH0_STALL_CNT" },
	{ 0x190000, 0x00DC, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH1_PC" },
	{ 0x190000, 0x00E0, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH1_VALID_CNTL" },
	{ 0x190000, 0x00E4, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH1_VALID_CNTH" },
	{ 0x190000, 0x00E8, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_TH1_STALL_CNT" },
	{ 0x190000, 0x00EC, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_IC_REQL" },
	{ 0x190000, 0x00F0, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_IC_REQH" },
	{ 0x190000, 0x00F4, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_IC_MISS" },
	{ 0x190000, 0x00F8, 0, 0, 8, "DSP1_CTRL_PERF_IVP1_INST_CNTL$" },
	{ 0x190000, 0x00FC, 0, 0, 8, "DSP1_CTRL_PERF_IVP1_INST_CNTH$" },
	{ 0x190000, 0x0128, 0, 0, 0, "DSP1_CTRL_DBG_IVP0_ADDR_PM" },
	{ 0x190000, 0x012c, 0, 0, 0, "DSP1_CTRL_DBG_IVP0_ADDR_DM" },
	{ 0x190000, 0x0130, 0, 0, 0, "DSP1_CTRL_DBG_IVP0_ERROR_INFO" },
	{ 0x190000, 0x0154, 0, 0, 0, "DSP1_CTRL_DBG_IVP1_ADDR_PM" },
	{ 0x190000, 0x0158, 0, 0, 0, "DSP1_CTRL_DBG_IVP1_ADDR_DM" },
	{ 0x190000, 0x015c, 0, 0, 0, "DSP1_CTRL_DBG_IVP1_ERROR_INFO" },
	{ 0x190000, 0x0180, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_STALL_CNTL" },
	{ 0x190000, 0x0184, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_STALL_CNTH" },
	{ 0x190000, 0x0188, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_SFR_STALL_CNTL" },
	{ 0x190000, 0x018c, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_SFR_STALL_CNTH" },
	{ 0x190000, 0x0190, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_VM0_STALL_CNTL" },
	{ 0x190000, 0x0194, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_VM0_STALL_CNTH" },
	{ 0x190000, 0x0198, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_VM1_STALL_CNTL" },
	{ 0x190000, 0x019c, 0, 0, 0, "DSP1_CTRL_PERF_IVP0_VM1_STALL_CNTH" },
	{ 0x190000, 0x01a0, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_STALL_CNTL" },
	{ 0x190000, 0x01a4, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_STALL_CNTH" },
	{ 0x190000, 0x01a8, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_SFR_STALL_CNTL" },
	{ 0x190000, 0x01ac, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_SFR_STALL_CNTH" },
	{ 0x190000, 0x01b0, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_VM0_STALL_CNTL" },
	{ 0x190000, 0x01b4, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_VM0_STALL_CNTH" },
	{ 0x190000, 0x01b8, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_VM1_STALL_CNTL" },
	{ 0x190000, 0x01bc, 0, 0, 0, "DSP1_CTRL_PERF_IVP1_VM1_STALL_CNTH" },
	{ 0x190000, 0x01c8, 0, 0, 4, "DSP1_CTRL_SM_ID$" },
	{ 0x190000, 0x01d8, 0, 0, 4, "DSP1_CTRL_SM_ADDR$" },
	{ 0x190000, 0x01e8, 0, 0, 0, "DSP1_CTRL_IVP0_STM_FUNC_STATUS" },
	{ 0x190000, 0x01ec, 0, 0, 0, "DSP1_CTRL_IVP1_STM_FUNC_STATUS" },
	{ 0x190000, 0x0208, 0, 0, 0, "DSP1_CTRL_AXI_ERROR_RD" },
	{ 0x190000, 0x020c, 0, 0, 0, "DSP1_CTRL_AXI_ERROR_WR" },
	{ 0x190000, 0x0210, 0, 0, 4, "DSP1_CTRL_RD_MOCNT$" },
	{ 0x190000, 0x0280, 0, 0, 4, "DSP1_CTRL_WR_MOCNT$" },
	{ 0x190000, 0x0400, 0, 0, 0, "DSP1_CTRL_IVP0_WAKEUP" },
	{ 0x190000, 0x0404, 0, 0, 4, "DSP1_CTRL_IVP0_INTR_STATUS_TH$" },
	{ 0x190000, 0x040c, 0, 0, 4, "DSP1_CTRL_IVP0_INTR_ENABLE_TH$" },
	{ 0x190000, 0x0414, 0, 0, 4, "DSP1_CTRL_IVP0_SWI_SET_TH$" },
	{ 0x190000, 0x041c, 0, 0, 4, "DSP1_CTRL_IVP0_SWI_CLEAR_TH$" },
	{ 0x190000, 0x0424, 0, 0, 4, "DSP1_CTRL_IVP0_MASKED_STATUS_TH$" },
	{ 0x190000, 0x042c, 0, 0, 0, "DSP1_CTRL_IVP0_IC_BASE_ADDR" },
	{ 0x190000, 0x0430, 0, 0, 0, "DSP1_CTRL_IVP0_IC_CODE_SIZE" },
	{ 0x190000, 0x0434, 0, 0, 0, "DSP1_CTRL_IVP0_IC_INVALID_REQ" },
	{ 0x190000, 0x0438, 0, 0, 0, "DSP1_CTRL_IVP0_IC_INVALID_STATUS" },
	{ 0x190000, 0x043c, 0, 0, 8, "DSP1_CTRL_IVP0_DM_STACK_START$" },
	{ 0x190000, 0x0440, 0, 0, 8, "DSP1_CTRL_IVP0_DM_STACK_END$" },
	{ 0x190000, 0x044c, 0, 0, 0, "DSP1_CTRL_IVP0_STATUS" },
	{ 0x190000, 0x0480, 0, 0, 0, "DSP1_CTRL_IVP1_WAKEUP" },
	{ 0x190000, 0x0484, 0, 0, 4, "DSP1_CTRL_IVP1_INTR_STATUS_TH$" },
	{ 0x190000, 0x048c, 0, 0, 4, "DSP1_CTRL_IVP1_INTR_ENABLE_TH$" },
	{ 0x190000, 0x0494, 0, 0, 4, "DSP1_CTRL_IVP1_SWI_SET_TH$" },
	{ 0x190000, 0x049c, 0, 0, 4, "DSP1_CTRL_IVP1_SWI_CLEAR_TH$" },
	{ 0x190000, 0x04a4, 0, 0, 4, "DSP1_CTRL_IVP1_MASKED_STATUS_TH$" },
	{ 0x190000, 0x04ac, 0, 0, 0, "DSP1_CTRL_IVP1_IC_BASE_ADDR" },
	{ 0x190000, 0x04b0, 0, 0, 0, "DSP1_CTRL_IVP1_IC_CODE_SIZE" },
	{ 0x190000, 0x04b4, 0, 0, 0, "DSP1_CTRL_IVP1_IC_INVALID_REQ" },
	{ 0x190000, 0x04b8, 0, 0, 0, "DSP1_CTRL_IVP1_IC_INVALID_STATUS" },
	{ 0x190000, 0x04bc, 0, 0, 8, "DSP1_CTRL_IVP1_DM_STACK_START$" },
	{ 0x190000, 0x04c0, 0, 0, 8, "DSP1_CTRL_IVP1_DM_STACK_END$" },
	{ 0x190000, 0x04cc, 0, 0, 0, "DSP1_CTRL_IVP1_STATUS" },
	{ 0x190000, 0x0800, 0, 0, 0, "DSP1_CTRL_IVP0_MAILBOX_INTR_TH0" },
	{ 0x190000, 0x0804, 0, 0, 4, "DSP1_CTRL_IVP0_MAILBOX_TH0_$" },
	{ 0x190000, 0x0a00, 0, 0, 0, "DSP1_CTRL_IVP0_MAILBOX_INTR_TH1" },
	{ 0x190000, 0x0a04, 0, 0, 4, "DSP1_CTRL_IVP0_MAILBOX_TH1_$" },
	{ 0x190000, 0x0c00, 0, 0, 0, "DSP1_CTRL_IVP1_MAILBOX_INTR_TH0" },
	{ 0x190000, 0x0c04, 0, 0, 4, "DSP1_CTRL_IVP1_MAILBOX_TH0_$" },
	{ 0x190000, 0x0e00, 0, 0, 0, "DSP1_CTRL_IVP1_MAILBOX_INTR_TH1" },
	{ 0x190000, 0x0e04, 0, 0, 4, "DSP1_CTRL_IVP1_MAILBOX_TH1_$" },
	{ 0x190000, 0x1000, 0, 0, 4, "DSP1_CTRL_IVP0_MSG_TH0_$" },
	{ 0x190000, 0x1400, 0, 0, 4, "DSP1_CTRL_IVP0_MSG_TH1_$" },
	{ 0x190000, 0x1800, 0, 0, 4, "DSP1_CTRL_IVP1_MSG_TH0_$" },
	{ 0x190000, 0x1c00, 0, 0, 4, "DSP1_CTRL_IVP1_MSG_TH1_$" },
};

static unsigned int dsp_hw_p0_ctrl_dhcp_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	int ret;
	struct dsp_priv_mem *pmem;
	unsigned int *sm;

	dsp_enter();
	pmem = &ctrl->sys->memory.priv_mem[DSP_P0_PRIV_MEM_DHCP];

	if (!pmem->kvaddr) {
		ret = 0xdead0010;
		dsp_warn("DHCP mem is not allocated yet\n");
		goto p_err;
	}

	if (addr > pmem->size) {
		ret = 0xdead0011;
		dsp_err("address is outside DHCP mem range(%#x/%#zx)\n",
				addr, pmem->size);
		goto p_err;
	}

	sm = pmem->kvaddr + addr;
	dsp_leave();
	return *sm;
p_err:
	return ret;
}

static int dsp_hw_p0_ctrl_dhcp_writel(struct dsp_ctrl *ctrl,
		unsigned int addr, int val)
{
	int ret;
	struct dsp_priv_mem *pmem;
	unsigned int *sm;

	dsp_enter();
	pmem = &ctrl->sys->memory.priv_mem[DSP_P0_PRIV_MEM_DHCP];

	if (!pmem->kvaddr) {
		ret = 0xdead0015;
		dsp_warn("DHCP mem is not allocated yet\n");
		goto p_err;
	}

	if (addr > pmem->size) {
		ret = 0xdead0016;
		dsp_err("address is outside DHCP mem range(%#x/%#zx)\n",
				addr, pmem->size);
		goto p_err;
	}

	sm = pmem->kvaddr + addr;
	*sm = val;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static unsigned int dsp_hw_p0_ctrl_secure_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	int ret;
	unsigned long val;

	dsp_enter();
#ifdef ENABLE_SECURE_READ
	ret = exynos_smc_readsfr(ctrl->sfr_pa + addr, &val);
	if (ret) {
		dsp_err("Failed to read secure sfr by smc(%#x)(%d)\n",
				addr, ret);
		return 0xdead0020;
	} else {
		dsp_dbg("read secure sfr by smc(%#x)(%lx)\n", addr, val);
	}
#else
	ret = 0;
	val = readl(ctrl->sfr + addr);
#endif
	dsp_leave();
	return val;
}

static int dsp_hw_p0_ctrl_secure_writel(struct dsp_ctrl *ctrl,
		unsigned int addr, int val)
{
	int ret;

	dsp_enter();
#ifdef ENABLE_SECURE_WRITE
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(ctrl->sfr_pa + addr),
			val, 0x0);
	if (ret) {
		dsp_err("Failed to write secure sfr by smc(%#x)(%d)\n",
				addr, ret);
		return ret;
	} else {
		dsp_dbg("write secure sfr by smc(%#x)\n", addr);
	}
#else
	ret = 0;
	writel(val, ctrl->sfr + addr);
#endif
	dsp_leave();
	return 0;
}

static int dsp_hw_p0_ctrl_common_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_ALIVE_CTRL, 0, 0);
	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_REMAPS0_NS, 0, 1);
	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_REMAPD0_NS, 0,
			0x30000000);

	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_REMAPS1, 0, 1);
	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_REMAPD1, 0,
			0x30000000);

	dsp_leave();
	return 0;
}

static int dsp_hw_p0_ctrl_extra_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_p0_ctrl_start(struct dsp_ctrl *ctrl)
{
	dsp_enter();
#ifdef ENABLE_DSP_VELOCE
	/* disable S2MPU */
	ctrl->ops->remap_writel(ctrl, 0x16060000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x16090000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x160C0000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x160F0000, 0x0);
	ctrl->ops->remap_writel(ctrl, 0x16120000, 0x0);

	/* set TZPC */
	ctrl->ops->remap_writel(ctrl, 0x16010204, 0x3);

	/* set CMU */
	/* Because of clk_gating issue in veloce, this setting is
	 * required.
	 * This clk_gating issue is related with setting clk mux.
	 * If clk mux is not selected, clk gating could be operated.
	 * For preventing unintended clk_gating, this setting is
	 * required.
	 */
	ctrl->ops->remap_writel(ctrl, 0x1600180c, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x16001800, 0x00000008);
	ctrl->ops->remap_writel(ctrl, 0x16000620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x16801804, 0x00000001);
	ctrl->ops->remap_writel(ctrl, 0x16800600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x16901804, 0x00000001);
	ctrl->ops->remap_writel(ctrl, 0x16900600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19801804, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x19800620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19800600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19800610, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19901804, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x19900620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19900600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19900610, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19a01804, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x19a00620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19a00600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19a00610, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19b01804, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x19b00620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19b00600, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x19b00610, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x16b01804, 0x00000003);
	ctrl->ops->remap_writel(ctrl, 0x16b00620, 0x00000010);
	ctrl->ops->remap_writel(ctrl, 0x16000800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x16800800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x16900800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x19800800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x19900800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x19a00800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x19b00800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x16b00800, 0x3133d06c);
	ctrl->ops->remap_writel(ctrl, 0x16000838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x16000830, 0x00000011);
	ctrl->ops->remap_writel(ctrl, 0x16800838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x16800830, 0x00000011);
	ctrl->ops->remap_writel(ctrl, 0x16900838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x16900830, 0x00000011);
	ctrl->ops->remap_writel(ctrl, 0x19800838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x19800830, 0x00000011);
	ctrl->ops->remap_writel(ctrl, 0x19900838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x19900830, 0x00000011);
	ctrl->ops->remap_writel(ctrl, 0x16b00838, 0x00f041c3);
	ctrl->ops->remap_writel(ctrl, 0x16b00830, 0x00000011);

	ctrl->ops->offset_writel(ctrl, DSP_P0_DNCC_NS_IRQ_MBOX_ENABLE_FR_CC, 0,
			0x0);
#endif
	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_RELEASE, 0, 0x3);
	dsp_leave();
	return 0;
}

static int dsp_hw_p0_ctrl_reset(struct dsp_ctrl *ctrl)
{
	int ret;
	unsigned int wfi, wfe;
	unsigned int repeat = 1000;

	dsp_enter();
	while (1) {
		wfi = ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_WFI_STATUS, 0);
		wfe = ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_WFE_STATUS, 0);
		if (wfi & 0x1 || wfe & 0x1 || !repeat)
			break;
		repeat--;
		udelay(100);
	};

	if (!(wfi & 0x1 || wfe & 0x1)) {
		dsp_err("status of device is not idle(%#x/%#x)\n", wfi, wfe);
		ret = -ETIMEDOUT;
		dsp_dump_ctrl();
		goto p_err;
	}

	ctrl->ops->offset_writel(ctrl, DSP_P0_DNC_CPU_RELEASE, 0, 0x0);
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_p0_ctrl_pc_dump(struct dsp_ctrl *ctrl, int idx)
{
	dsp_notice("[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DNC_CPU_PC_L].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_PC_L, 0),
			ctrl->reg[DSP_P0_DNC_CPU_PC_H].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_PC_H, 0));
	dsp_notice("[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP0_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC, 0));
	dsp_notice("[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH1_PC, 0));
	dsp_notice("[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP0_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC, 0));
	dsp_notice("[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH1_PC, 0));
}

static void dsp_hw_p0_ctrl_pc_dump(struct dsp_ctrl *ctrl)
{
	int idx, repeat = 8;

	dsp_enter();
	dsp_notice("pc register dump (repeat:%d)\n", repeat);
	for (idx = 0; idx < repeat; ++idx)
		__dsp_hw_p0_ctrl_pc_dump(ctrl, idx);

	dsp_leave();
}

static void dsp_hw_p0_ctrl_dhcp_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("dhcp mem dump (count:%d)\n", DSP_P0_DHCP_USED_COUNT);
	for (idx = 0; idx < DSP_P0_DHCP_USED_COUNT; idx += 4) {
		addr = DSP_P0_DHCP_IDX(idx);
		val0 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx));
		val1 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 1));
		val2 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 2));
		val3 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 3));

		dsp_notice("[DHCP_INFO(%#06x-%#06x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_p0_ctrl_userdefined_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("userdefined dump (count:%d)\n",
			DSP_P0_SM_USERDEFINED_COUNT);
	for (idx = 0; idx < DSP_P0_SM_USERDEFINED_COUNT; idx += 4) {
		addr = DSP_P0_SM_USERDEFINED(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_USERDEFINED(idx));
		val1 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 3));

		dsp_notice("[USERDEFINED(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_p0_ctrl_fw_info_dump(struct dsp_ctrl *ctrl)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	dsp_notice("fw_info dump (count:%d)\n", DSP_P0_SM_FW_INFO_COUNT);
	for (idx = 0; idx < DSP_P0_SM_FW_INFO_COUNT; idx += 4) {
		addr = DSP_P0_SM_FW_INFO(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx));
		val1 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 3));

		dsp_notice("[FW_INFO(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void __dsp_hw_p0_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file, int idx)
{
	seq_printf(file, "[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DNC_CPU_PC_L].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_PC_L, 0),
			ctrl->reg[DSP_P0_DNC_CPU_PC_H].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DNC_CPU_PC_H, 0));
	seq_printf(file, "[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP0_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC, 0));
	seq_printf(file, "[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP0_CTRL_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP0_CTRL_PERF_IVP1_TH1_PC, 0));
	seq_printf(file, "[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP0_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP0_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP0_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC, 0));
	seq_printf(file, "[%2d][%26s] %8x [%26s] %8x\n",
			idx, ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH0_PC, 0),
			ctrl->reg[DSP_P0_DSP1_CTRL_PERF_IVP1_TH1_PC].name,
			ctrl->ops->offset_readl(ctrl,
				DSP_P0_DSP1_CTRL_PERF_IVP1_TH1_PC, 0));
}

static void dsp_hw_p0_ctrl_user_pc_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx, repeat = 8;

	dsp_enter();
	seq_printf(file, "pc register dump (repeat:%d)\n", repeat);
	for (idx = 0; idx < repeat; ++idx)
		__dsp_hw_p0_ctrl_user_pc_dump(ctrl, file, idx);

	dsp_leave();
}

static void dsp_hw_p0_ctrl_user_dhcp_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "dhcp mem dump (count:%d)\n", DSP_P0_DHCP_USED_COUNT);
	for (idx = 0; idx < DSP_P0_DHCP_USED_COUNT; idx += 4) {
		addr = DSP_P0_DHCP_IDX(idx);
		val0 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx));
		val1 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 1));
		val2 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 2));
		val3 = ctrl->ops->dhcp_readl(ctrl, DSP_P0_DHCP_IDX(idx + 3));

		seq_printf(file, "[DHCP_INFO(%#06x-%#06x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_p0_ctrl_user_userdefined_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "userdefined dump (count:%d)\n",
			DSP_P0_SM_USERDEFINED_COUNT);
	for (idx = 0; idx < DSP_P0_SM_USERDEFINED_COUNT; idx += 4) {
		addr = DSP_P0_SM_USERDEFINED(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_USERDEFINED(idx));
		val1 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl,
				DSP_P0_SM_USERDEFINED(idx + 3));

		seq_printf(file, "[USERDEFINED(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static void dsp_hw_p0_ctrl_user_fw_info_dump(struct dsp_ctrl *ctrl,
		struct seq_file *file)
{
	int idx;
	unsigned int addr;
	unsigned int val0, val1, val2, val3;

	dsp_enter();
	seq_printf(file, "fw_info dump (count:%d)\n", DSP_P0_SM_FW_INFO_COUNT);
	for (idx = 0; idx < DSP_P0_SM_FW_INFO_COUNT; idx += 4) {
		addr = DSP_P0_SM_FW_INFO(idx);
		val0 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx));
		val1 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 1));
		val2 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 2));
		val3 = ctrl->ops->sm_readl(ctrl, DSP_P0_SM_FW_INFO(idx + 3));

		seq_printf(file, "[FW_INFO(%#7x-%#7x)] %8x %8x %8x %8x\n",
				addr, addr + 0xf, val0, val1, val2, val3);
	}
	dsp_leave();
}

static int dsp_hw_p0_ctrl_probe(struct dsp_ctrl *ctrl, void *sys)
{
	int ret;

	dsp_enter();
	ctrl->reg = p0_sfr_reg;
	ctrl->reg_count = DSP_P0_REG_END;

	ret = dsp_hw_common_ctrl_probe(ctrl, sys);
	if (ret) {
		dsp_err("Failed to probe common ctrl(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_ctrl_ops p0_ctrl_ops = {
	.remap_readl		= dsp_hw_common_ctrl_remap_readl,
	.remap_writel		= dsp_hw_common_ctrl_remap_writel,
	.sm_readl		= dsp_hw_common_ctrl_sm_readl,
	.sm_writel		= dsp_hw_common_ctrl_sm_writel,
	.dhcp_readl		= dsp_hw_p0_ctrl_dhcp_readl,
	.dhcp_writel		= dsp_hw_p0_ctrl_dhcp_writel,
	.secure_readl		= dsp_hw_p0_ctrl_secure_readl,
	.secure_writel		= dsp_hw_p0_ctrl_secure_writel,
	.offset_readl		= dsp_hw_common_ctrl_offset_readl,
	.offset_writel		= dsp_hw_common_ctrl_offset_writel,

	.reg_print		= dsp_hw_common_ctrl_reg_print,
	.dump			= dsp_hw_common_ctrl_dump,
	.pc_dump		= dsp_hw_p0_ctrl_pc_dump,
	.dhcp_dump		= dsp_hw_p0_ctrl_dhcp_dump,
	.userdefined_dump	= dsp_hw_p0_ctrl_userdefined_dump,
	.fw_info_dump		= dsp_hw_p0_ctrl_fw_info_dump,

	.user_reg_print		= dsp_hw_common_ctrl_user_reg_print,
	.user_dump		= dsp_hw_common_ctrl_user_dump,
	.user_pc_dump		= dsp_hw_p0_ctrl_user_pc_dump,
	.user_dhcp_dump		= dsp_hw_p0_ctrl_user_dhcp_dump,
	.user_userdefined_dump	= dsp_hw_p0_ctrl_user_userdefined_dump,
	.user_fw_info_dump	= dsp_hw_p0_ctrl_user_fw_info_dump,

	.common_init		= dsp_hw_p0_ctrl_common_init,
	.extra_init		= dsp_hw_p0_ctrl_extra_init,
	.all_init		= dsp_hw_common_ctrl_all_init,
	.start			= dsp_hw_p0_ctrl_start,
	.reset			= dsp_hw_p0_ctrl_reset,
	.force_reset		= dsp_hw_common_ctrl_force_reset,

	.open			= dsp_hw_common_ctrl_open,
	.close			= dsp_hw_common_ctrl_close,
	.probe			= dsp_hw_p0_ctrl_probe,
	.remove			= dsp_hw_common_ctrl_remove,
};

int dsp_hw_p0_ctrl_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_CTRL,
			&p0_ctrl_ops, sizeof(p0_ctrl_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
