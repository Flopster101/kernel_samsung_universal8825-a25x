/* SPDX-License-Identifier: GPL-2.0-only
 *
 * cal_8825/regs-decon.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register definition file for Samsung DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REGS_DECON_H
#define _REGS_DECON_H

#define MAX_WIN_PER_DECON	8
#define MAX_DECON_CNT		REGS_DECON_ID_MAX
enum decon_regs_id {
	REGS_DECON0_ID = 0,
	REGS_DECON1_ID,
	REGS_DECON_ID_MAX
};
enum decon_regs_type {
	REGS_DECON = 0,
	REGS_DECON_SYS,
	REGS_DECON_WIN,
	REGS_DECON_SUB,
	REGS_DECON_WINCON,
	REGS_DECON_SRAMC,
	REGS_DECON_SRAMC1, /* only valid at EVT0 */
	REGS_DECON_TYPE_MAX
};

/*
 * [ DPU BASE ADDRESS ]
 * - CMU_DPU		     0x1480_0000
 * - D_TZPC_DPU              0x1481_0000
 * - SYSREG_DPU              0x1482_0000 : DPU_MIPI_PHY_CON (0x1008)
 * - GPC_DISP                0x????
 * - MIPI_DSIM0              0x148C_0000
 * - MIPI_DCPHY              0x148E_0000
 * - DECON_MAIN              0x1494_0000
 * - DECON_WIN               0x1495_0000
 * - DECON_SUB               0x1496_0000
 * - DECON0_WINCON           0x1497_0000
 * - DECON1_WINCON           0x1498_0000
 * - DQE0                    0x149B_0000
 *
 * [ DPU BASE ADDRESS ]
 *
 * - CMU_DPUF0               0x???
 * - D_TZPC_DPU0             0x????
 * - SYSREG_DPU0             0x???? : DRCG (0x0104)
 * - SYSMMU_D0_S1_NS_DPU     0x1490_0000
 * - SYSMMU_D1_S1_NS_DPU     0x149D_0000
 * - SYSMMU_D0_S1_S_DPU      0x1491_0000
 * - SYSMMU_D1_S1_S_DPU      0x149E_0000
 * - SYSMMU_D0_S2_DPU_S      0x1492_0000
 * - SYSMMU_D1_S2_DPU_N      0x149F_0000
 * - PPMU_D0_DPU             0x1493_0000
 * - PPMU_D1_DPU             0x149C_0000
 * - DPU_DMA                 0x1489_0000
 * - DPU_VGEN                0x148A_0000
 * - DPU_C2SERV              0x1488_0000
 * - DPU_DPP                 0x1484_0000
 * - DPU_DPP_COMMON          0x????
 * - DPU_HDR_LSI             0x1486_0000
 *
 */

//-------------------------------------------------------------------
// DECON_MAIN
//-------------------------------------------------------------------
#define DECON_OFFSET(_id)			(0x0000 + 0x1000 * (_id))
#define DECON0_OFFSET				(0x0000)
#define DECON1_OFFSET				(0x1000)
//#define DECON2_OFFSET				(0x2000)

#define SHADOW_OFFSET				(0x0800)


#define DECON_VERSION				(0x0000)
#define DECON_VERSION_GET(_v)			(((_v) >> 0) & 0xffffffff)

#define FRAME_COUNT				(0x0004)
#define FRAME_COUNT_GET(_v)			(((_v) >> 0) & 0xffffffff)

#define GLOBAL_CON				(0x0020)
#define GLOBAL_CON_SRESET			(1 << 28)
#define GLOBAL_CON_TEN_BPC_MODE_F		(1 << 20)
#define GLOBAL_CON_TEN_BPC_MODE_MASK		(1 << 20)
/* Not available in Papaya, Check please
#define GLOBAL_CON_MASTER_MODE_F(_v)		((_v) << 12)
#define GLOBAL_CON_MASTER_MODE_MASK		(0xF << 12)
*/
#define GLOBAL_CON_OPERATION_MODE_F		(1 << 8)
#define GLOBAL_CON_OPERATION_MODE_VIDEO_F	(0 << 8)
#define GLOBAL_CON_OPERATION_MODE_CMD_F		(1 << 8)
#define GLOBAL_CON_IDLE_STATUS			(1 << 5)
#define GLOBAL_CON_RUN_STATUS			(1 << 4)
#define GLOBAL_CON_DECON_EN			(1 << 1)
#define GLOBAL_CON_DECON_EN_F			(1 << 0)

#define TRIG_CON				(0x0030)
#define HW_SW_TRIG_HS_STATUS			(1 << 28)
#define HW_TRIG_SEL(_v)				((_v) << 24)
#define HW_TRIG_SEL_MASK			(0x3 << 24)
#define HW_TRIG_SEL_FROM_NONE			(3 << 24)
#define HW_TRIG_SEL_FROM_DDI2			(2 << 24)
#define HW_TRIG_SEL_FROM_DDI1			(1 << 24)
#define HW_TRIG_SEL_FROM_DDI0			(0 << 24)
#define HW_TRIG_SKIP(_v)			((_v) << 16)
#define HW_TRIG_SKIP_MASK			(0xff << 16)
#define HW_TRIG_ACTIVE_VALUE			(1 << 13)
#define HW_TRIG_EDGE_POLARITY			(1 << 12) // Must be stable before HW_TIG_EN:1
#define SW_TRIG_EN				(1 << 8)
/* Not available in Papaya
#define HW_TRIG_MASK_SLAVE1			(1 << 6)
#define HW_TRIG_MASK_SLAVE0			(1 << 5)
*/
#define HW_TRIG_MASK_DECON			(1 << 4)
#define HW_SW_TRIG_TIMER_CLEAR			(1 << 3)
#define HW_SW_TRIG_TIMER_EN			(1 << 2)
#define SW_TRIG_DET_EN				(1 << 1) //new, along with SW_TRIG_EN
#define HW_TRIG_EN				(1 << 0)

#define TRIG_TIMER				(0x0034)
#define HW_TE_CNT				(0x0038)
#define HW_TRIG_CNT_B_GET(_v)			(((_v) >> 16) & 0xffff)
#define HW_TRIG_CNT_A_GET(_v)			(((_v) >> 0) & 0xffff)

#define TRIG_CON_SECURE				(0x003C)
/* Not available in Papaya`
#define HW_TRIG_MASK_SECURE_SLAVE1		(1 << 6)
#define HW_TRIG_MASK_SECURE_SLAVE0		(1 << 5)
*/
#define HW_TRIG_MASK_SECURE			(1 << 4)

#define OPMODE_OPTION				(0x0040)

#define CMD_FRAME_CON_F(_v)			((_v) << 4)
#define CMD_FRAME_CON_F_MASK			(0x7 << 4)
#define CMD_FC_LEGACY				(0)
#define CMD_FC_SVSYNC				(1)
#define CMD_FC_PIXELSYNC			(2)
#define VDO_FRAME_CON_F(_v)			((_v) << 0)
#define VDO_FRAME_CON_F_MASK			(0xf << 0)
#define VDO_FC_LEGACY				(0)
#define VDO_FC_LEGACY_C2V			(1)
#define VDO_FC_ADAPTIVE				(2)
#define VDO_FC_ADAPTIVE_C2V			(3)
#define VDO_FC_ONESHOT				(4)
#define VDO_FC_ONESHOT_C2V			(5)
#define VDO_FC_ONESHOT_ALIGN			(6)
#define VDO_FC_ONESHOT_ALIGN_C2V		(7)
#define VDO_FC_TE_DRIVEN			(8)

/**
 * TH cycle based on OSCCLK
 * effective when SV-sync command mode or DP path video mode
 * using VIDEO_FRAME_CON_F = 2
 * DECON postpone frame start triggering until VFP counter reach TH value
 */
#define VFP_THRESHOLD_DP			(0x0048)
#define VFP_TH(_v)				((_v) << 0)
#define VFP_TH_MASK				(0xFFFFFFFF << 0)

#define SHD_REG_UP_REQ				(0x0050)
#define SHD_REG_UP_REQ_GLOBAL			(1 << 31)
#define SHD_REG_UP_REQ_DQE_ROI			(1 << 29) // decon0,1 only
#define SHD_REG_UP_REQ_DQE			(1 << 28) // decon0,1 only
#define SHD_REG_UP_REQ_CMP			(1 << 20)
#define SHD_REG_UP_REQ_WIN(_win)		(1 << (_win))
#define SHD_REG_UP_REQ_FOR_DECON		(0xFFFF)

#define DECON_INT_EN				(0x0060)
#define INT_EN_DQE_DIMMING_END			(1 << 21)
#define INT_EN_DQE_DIMMING_START		(1 << 20)
#define INT_EN_FRAME_DONE			(1 << 13)
#define INT_EN_FRAME_START			(1 << 12)
#define INT_EN_EXTRA				(1 << 4)
#define INT_EN					(1 << 0)
#define INT_EN_MASK				(0x303011)

#define DECON_INT_EN_EXTRA			(0x0064)
#define INT_EN_UNMASK_START			(1 << 24)
#define INT_EN_TE_FALL				(1 << 20)
#define INT_EN_TE_RISE				(1 << 16)
#define INT_EN_DSIM_LATE_START_ALARM		(1 << 12)
// Not available in Papaya
//#define INT_EN_CWB_INST_OFF			(1 << 8)
#define INT_EN_RESOURCE_CONFLICT		(1 << 4)
#define INT_EN_TIME_OUT				(1 << 0)

#define INT_TIMEOUT_VAL				(0x0068)

#define DECON_INT_PEND				(0x0070)
#define INT_PEND_DQE_DIMMING_END		(1 << 21)
#define INT_PEND_DQE_DIMMING_START		(1 << 20)
#define INT_PEND_FRAME_DONE			(1 << 13)
#define INT_PEND_FRAME_START			(1 << 12)
#define INT_PEND_EXTRA				(1 << 4)

/* defined for common part of driver only */
#define DPU_FRAME_DONE_INT_PEND			INT_PEND_FRAME_DONE
#define DPU_FRAME_START_INT_PEND		INT_PEND_FRAME_START

#define DPU_DQE_DIMMING_END_INT_PEND		INT_PEND_DQE_DIMMING_END
#define DPU_DQE_DIMMING_START_INT_PEND		INT_PEND_DQE_DIMMING_START

#define DECON_INT_PEND_EXTRA			(0x0074)
#define INT_PEND_UNMASK_START			(1 << 24)
#define INT_PEND_TE_FALL			(1 << 20)
#define INT_PEND_TE_RISE			(1 << 16)
#define INT_PEND_DSIM_LATE_START_ALARM		(1 << 12)
/* Not Available in Papaya
#define INT_PEND_CWB_INST_OFF			(1 << 8)
*/
#define INT_PEND_RESOURCE_CONFLICT		(1 << 4)
#define INT_PEND_TIME_OUT			(1 << 0)

/* defined for common part of driver only */
#define DPU_RESOURCE_CONFLICT_INT_PEND		INT_PEND_RESOURCE_CONFLICT
#define DPU_TIME_OUT_INT_PEND			INT_PEND_TIME_OUT


#define RSC_STATUS_0				(0x0100)
#define SEL_CH7					(7 << 28)
#define SEL_CH6					(7 << 24)
#define SEL_CH5					(7 << 20)
#define SEL_CH4					(7 << 16)
#define SEL_CH3					(7 << 12)
#define SEL_CH2					(7 << 8)
#define SEL_CH1					(7 << 4)
#define SEL_CH0					(7 << 0)

#define RSC_STATUS_2				(0x0108)
#define SEL_WIN7				(7 << 28)
#define SEL_WIN6				(7 << 24)
#define SEL_WIN5				(7 << 20)
#define SEL_WIN4				(7 << 16)
#define SEL_WIN3				(7 << 12)
#define SEL_WIN2				(7 << 8)
#define SEL_WIN1				(7 << 4)
#define SEL_WIN0				(7 << 0)

/*
 * _n: [0, 1] // Only 0 is valid in Papaya
 * _id: [0, 7]
 *
 */
// REVISIT for Papaya, as of now not being used
#define RSC_STATUS_N(_n)			(0x011C + ((_n) * 0x4))
#define SEL_SRAM_ID(_id)			(0x7 << (_id * 4))
// Pamir-9925 it is 14, but need to check value for Papaya
#define MAX_SRAM_CNT				14

#define RSC_STATUS_7				(0x011C)
/* Only SRAM0 & SRAM1 are applicable for Papaya */
#define SEL_SRAM1				(7 << 4)
#define SEL_SRAM0				(7 << 0)

#define DATA_PATH_CON				(0x0200)
#define ENHANCE_PATH_F(_v)			((_v) << 12)
#define ENHANCE_DITHER_ON			ENHANCE_PATH_F(1)
#define ENHANCE_DQE_ON				ENHANCE_PATH_F(2)
#define ENHANCE_DQE_ON_ENH_8BPC			ENHANCE_PATH_F(3)
// 7 is also valid, since last bit is dont care
#define ENHANCE_DQE_ON_RCD_ON			ENHANCE_PATH_F(6)
#define ENHANCE_PATH_MASK			(0x7 << 12)
#define ENHANCE_PATH_GET(_v)			(((_v) >> 12) & 0x7)
// COMP0[4], WBIF[2], OF0-DSIMIF[0]
#define COMP_OUTIF_PATH_F(_v)			((_v) << 0)
#define COMP_OUTIF_PATH_MASK			(0xff << 0)
#define COMP_OUTIF_PATH_GET(_v)			(((_v) >> 0) & 0xff)
#define CWB_PATH_EN				(1 << 2) // WBIF[2]

/* primary display path */
#define SRAM_EN_OF_PRI_0			(0x0210)
#define SRAM1_EN_F				(1 << 4)
#define SRAM0_EN_F				(1 << 0)

/* CWB : same with primary */
#define BLD_BG_IMG_SIZE_PRI			(0x0220)
#define BLENDER_BG_HEIGHT_F(_v)			((_v) << 16)
#define BLENDER_BG_HEIGHT_MASK			(0x3fff << 16)
#define BLENDER_BG_HEIGHT_GET(_v)		(((_v) >> 16) & 0x3fff)
#define BLENDER_BG_WIDTH_F(_v)			((_v) << 0)
#define BLENDER_BG_WIDTH_MASK			(0x3fff << 0)
#define BLENDER_BG_WIDTH_GET(_v)		(((_v) >> 0) & 0x3fff)

#define BLD_BG_IMG_COLOR_0_PRI			(0x0224)
#define BLENDER_BG_A_F(_v)			((_v) << 16)
#define BLENDER_BG_A_MASK			(0xff << 16)
#define BLENDER_BG_A_GET(_v)			(((_v) >> 16) & 0xff)
#define BLENDER_BG_R_F(_v)			((_v) << 0)
#define BLENDER_BG_R_MASK			(0x3ff << 0)
#define BLENDER_BG_R_GET(_v)			(((_v) >> 0) & 0x3ff)

#define BLD_BG_IMG_COLOR_1_PRI			(0x0228)
#define BLENDER_BG_G_F(_v)			((_v) << 16)
#define BLENDER_BG_G_MASK			(0x3ff << 16)
#define BLENDER_BG_G_GET(_v)			(((_v) >> 16) & 0x3ff)
#define BLENDER_BG_B_F(_v)			((_v) << 0)
#define BLENDER_BG_B_MASK			(0x3ff << 0)
#define BLENDER_BG_B_GET(_v)			(((_v) >> 0) & 0x3ff)

#define BLD_LRM					(0x0240)
#define LRM01_MODE_F(_v)			((_v) << 0)
#define LRM01_MODE_MASK				(0x7 << 0)

#define OF_SIZE_0				(0x0290)
#define OUTFIFO_HEIGHT_F(_v)			((_v) << 16)
#define OUTFIFO_HEIGHT_MASK			(0x3fff << 16)
#define OUTFIFO_HEIGHT_GET(_v)			(((_v) >> 16) & 0x3fff)
#define OUTFIFO_WIDTH_F(_v)			((_v) << 0)
#define OUTFIFO_WIDTH_MASK			(0x3fff << 0)
#define OUTFIFO_WIDTH_GET(_v)			(((_v) >> 0) & 0x3fff)

#define OF_SIZE_2				(0x0298)  // decon0 only
#define OUTFIFO_COMPRESSED_SLICE_HEIGHT_F(_v)	((_v) << 16)
#define OUTFIFO_COMPRESSED_SLICE_HEIGHT_MASK	(0x3fff << 16)
#define OUTFIFO_COMPRESSED_SLICE_HEIGHT_GET(_v)	(((_v) >> 16) & 0x3fff)
#define OUTFIFO_COMPRESSED_SLICE_WIDTH_F(_v)	((_v) << 0)
#define OUTFIFO_COMPRESSED_SLICE_WIDTH_MASK	(0x3fff << 0)
#define OUTFIFO_COMPRESSED_SLICE_WIDTH_GET(_v)	(((_v) >> 0) & 0x3fff)

#define OF_TH_TYPE				(0x029C)
#define OUTFIFO_TH_1H_F				(0x5 << 0)
#define OUTFIFO_TH_2H_F				(0x6 << 0)
#define OUTFIFO_TH_F(_v)			((_v) << 0)
#define OUTFIFO_TH_MASK				(0x7 << 0)
#define OUTFIFO_TH_GET(_v)			((_v) >> 0 & 0x7)

#define OF_PIXEL_ORDER				(0x02A0)
#define OUTFIFO_PIXEL_ORDER_SWAP_F(_v)		((_v) << 4)
#define OUTFIFO_PIXEL_ORDER_SWAP_MASK		(0x7 << 4)
#define OUTFIFO_PIXEL_ORDER_SWAP_GET(_v)	(((_v) >> 4) & 0x7)

#define OF_LEVEL				(0x02B0)
#define OUTFIFO_FIFO_LEVEL(_v)			((_v) << 0)
#define OUTFIFO_FIFO_LEVEL_GET(_v)		(((_v) >> 0) & 0xffff)

#define OF_LAT_MON				(0x02B8)
#define LATENCY_COUNTER_CLEAR			(1 << 29)
#define LATENCY_COUNTER_ENABLE			(1 << 28)
#define LATENCY_COUNTER_VALUE_GET(_v)		(((_v) >> 0) & 0xffffff)

#define OF_URGENT_EN				(0x02C0)
#define WRITE_URGENT_GENERATION_EN_F		(0x1 << 1)
#define READ_URGENT_GENERATION_EN_F		(0x1 << 0)

#define OF_RD_URGENT_0				(0x02C4)
#define READ_URGENT_HIGH_THRESHOLD_F(_v)	((_v) << 16)
#define READ_URGENT_HIGH_THRESHOLD_MASK		(0xffff << 16)
#define READ_URGENT_HIGH_THRESHOLD_GET(_v)	(((_v) >> 16) & 0xffff)
#define READ_URGENT_LOW_THRESHOLD_F(_v)		((_v) << 0)
#define READ_URGENT_LOW_THRESHOLD_MASK		(0xffff << 0)
#define READ_URGENT_LOW_THRESHOLD_GET(_v)	(((_v) >> 0) & 0xffff)

#define OF_RD_URGENT_1				(0x02C8)
#define READ_URGENT_WAIT_CYCLE_F(_v)		((_v) << 0)
#define READ_URGENT_WAIT_CYCLE_GET(_v)		((_v) >> 0)

#define OF_WR_URGENT_0				(0x02CC)
#define WRITE_URGENT_HIGH_THRESHOLD_F(_v)	((_v) << 16)
#define WRITE_URGENT_HIGH_THRESHOLD_MASK	(0xffff << 16)
#define WRITE_URGENT_HIGH_THRESHOLD_GET(_v)	(((_v) >> 16) & 0xffff)
#define WRITE_URGENT_LOW_THRESHOLD_F(_v)	((_v) << 0)
#define WRITE_URGENT_LOW_THRESHOLD_MASK		(0xffff << 0)
#define WRITE_URGENT_LOW_THRESHOLD_GET(_v)	(((_v) >> 0) & 0xffff)

#define OF_DTA_CONTROL				(0x02D0)
#define DTA_EN_F				(1 << 0)

#define OF_DTA_THRESHOLD			(0x02D4)
#define DTA_HIGH_TH_F(_v)			((_v) << 16)
#define DTA_HIGH_TH_MASK			(0xffff << 16)
#define DTA_HIGH_TH_GET(_v)			(((_v) >> 16) & 0xffff)
#define DTA_LOW_TH_F(_v)			((_v) << 0)
#define DTA_LOW_TH_MASK				(0xffff << 0)
#define DTA_LOW_TH_GET(_v)			(((_v) >> 0) & 0xffff)

#define OF_WM_CON				(0x02E0) // only in decon0
#define WM_PULSE_CNT_F(_v)			((_v) << 4)
#define WM_PULSE_CNT_MASK			(0xf << 4)
#define WM_EN_F					(1 << 0)

#define OF_WM_TH_VAL				(0x02E4) // Only in decon0
#define WM_HIGH_THRESHOLD_F(_v)			((_v) << 16)
#define WM_HIGH_THRESHOLD_MASK			(0xffff << 16)
#define WM_HIGH_THRESHOLD_GET(_v)		(((_v) >> 16) & 0xffff)
#define WM_LOW_THRESHOLD_F(_v)			((_v) << 0)
#define WM_LOW_THRESHOLD_MASK			(0xffff << 0)
#define WM_LOW_THRESHOLD_GET(_v)		(((_v) >> 0) & 0xffff)

#define EWR_CON					(0x0300)
#define EWR_EN_F				(1 << 0)

#define EWR_TIMER				(0x0304)
#define TIMER_VALUE(_v)				((_v) << 0)
#define TIMER_VALUE_GET(_v)			(((_v) >> 0) & 0xffffffff)

#define PLL_SLEEP_CON				(0x0310)
#define SLEEP_CTRL_MODE_F			(1 << 8)
#define PLL_SLEEP_MASK_OUTIF1			(1 << 5)
#define PLL_SLEEP_MASK_OUTIF0			(1 << 4)
#define PLL_SLEEP_EN_OUTIF1_F			(1 << 1)
#define PLL_SLEEP_EN_OUTIF0_F			(1 << 0)

#define WAIT_CYCLE_AFTER_SFR_UPDATE		(0x0340)
#define WAIT_CYCLE_AFTER_SFR_UPDATE_V(_v)	((_v) << 0)
#define WAIT_CYCLE_AFTER_SFR_UPDATE_MASK	(0x3f << 0)

/* for VOTF control */
#define UPDATE_CTRL_EXTRA			(0x0700)
#define UPDATE_REJECT_EN			(1 << 0)
#define UPDATE_LEGACY				(0)
#define UPDATE_IGNORE_VOTF			(1)

/* for cursor control */
#define DBG_INFO_OF_CNT0(_id)			(0x0560 + (_id * 0x8))
#define DBG_INFO_OF_CNT1(_id)			(0x0564 + (_id * 0x8))
#define DBG_INFO_OF_XCNT(_v)			(((_v) >> 16) & 0x3fff)
#define DBG_INFO_OF_YCNT(_v)			(((_v) >> 0) & 0x3fff)

#define CLOCK_CON				(0x5000)
/*
 * [28] QACTIVE_PLL_VALUE = 0
 * [24] QACTIVE_VALUE = 0
 *   0: QACTIVE is dynamically changed by DECON h/w,
 *   1: QACTIVE is stuck to 1'b1
 * [20] AUTO_CG_EN_SCL (1=en / 0=dis)
 * [16] AUTO_CG_EN_CTRL (1=en / 0=dis)
 * [12] AUTO_CG_EN_OUTIF
 * [ 8] AUTO_CG_EN_COMP
 * [ 4] AUTO_CG_EN_ENH
 * [ 0] AUTO_CG_EN_BLD
 */
/* clock gating is disabled during initial bringup */
#define CLOCK_CON_QACTIVE_PLL_MASK             (0x1 << 28)
#define CLOCK_CON_QACTIVE_MASK                 (0x1 << 24)
#define CLOCK_CON_AUTO_CG_MASK                 (0x111111 << 0)

#define SECURE_CON				(0x5010)
#define PSLVERR_EN				(1 << 0)

//-------------------------------------------------------------------
// DECON_WIN : 0~7
//-------------------------------------------------------------------
/* WIN index offset increment : 0x1000 */
#define WIN_OFFSET(_id)				(0x0000 + 0x1000 * (_id))

#define WIN0_FUNC_CON_0				(0x0004)
#define WIN0_FUNC_CON_1				(0x0008)
#define WIN0_START_POSITION			(0x000C)
#define WIN0_END_POSITION			(0x0010)
#define WIN0_COLORMAP_0				(0x0014)
#define WIN0_COLORMAP_1				(0x0018)
#define WIN0_START_TIME_CON			(0x001C)

/* this can be handled by Secure OS */
#define WIN_SECURE_CON(_id)			(WIN_OFFSET(_id) + 0x0)
#define TZPC_FLAG_WIN_F				(1 << 0)
#define TZPC_WIN_SECURE				(0 << 0)
#define TZPC_WIN_NONSECURE			(1 << 0)

#define WIN_FUNC_CON_0(_id)			(WIN_OFFSET(_id) + 0x4)
#define WIN_ALPHA1_F(_v)			(((_v) & 0xFF) << 24)
#define WIN_ALPHA1_MASK				(0xFF << 24)
#define WIN_ALPHA0_F(_v)			(((_v) & 0xFF) << 16)
#define WIN_ALPHA0_MASK				(0xFF << 16)
#define WIN_ALPHA_GET(_v, _n)			(((_v) >>	\
						(16 + 8 * (_n))) & 0xFF)
#define WIN_FUNC_F(_v)				(((_v) & 0xF) << 8)
#define WIN_FUNC_MASK				(0xF << 8)
#define WIN_FUNC_GET(_v)			(((_v) >> 8) & 0xf)
#define WIN_SRESET				(1 << 4)
#define WIN_ALPHA_MULT_SRC_SEL_F(_v)		(((_v) & 0x3) << 0)
#define WIN_ALPHA_MULT_SRC_SEL_MASK		(0x3 << 0)

#define WIN_FUNC_CON_1(_id)			(WIN_OFFSET(_id) + 0x8)
#define WIN_FG_ALPHA_D_SEL_F(_v)		(((_v) & 0xF) << 24)
#define WIN_FG_ALPHA_D_SEL_MASK			(0xF << 24)
#define WIN_BG_ALPHA_D_SEL_F(_v)		(((_v) & 0xF) << 16)
#define WIN_BG_ALPHA_D_SEL_MASK			(0xF << 16)
#define WIN_FG_ALPHA_A_SEL_F(_v)		(((_v) & 0xF) << 8)
#define WIN_FG_ALPHA_A_SEL_MASK			(0xF << 8)
#define WIN_BG_ALPHA_A_SEL_F(_v)		(((_v) & 0xF) << 0)
#define WIN_BG_ALPHA_A_SEL_MASK			(0xF << 0)

#define WIN_START_POSITION(_id)			(WIN_OFFSET(_id) + 0xC)
#define WIN_STRPTR_Y_F(_v)			(((_v) & 0x3FFF) << 16)
#define WIN_STRPTR_X_F(_v)			(((_v) & 0x3FFF) << 0)

#define WIN_END_POSITION(_id)			(WIN_OFFSET(_id) + 0x10)
#define WIN_ENDPTR_Y_F(_v)			(((_v) & 0x3FFF) << 16)
#define WIN_ENDPTR_X_F(_v)			(((_v) & 0x3FFF) << 0)

#define WIN_COLORMAP_0(_id)			(WIN_OFFSET(_id) + 0x14)
#define WIN_MAPCOLOR_A_F(_v)			((_v) << 16)
#define WIN_MAPCOLOR_A_MASK			(0xff << 16)
#define WIN_MAPCOLOR_R_F(_v)			((_v) << 0)
#define WIN_MAPCOLOR_R_MASK			(0x3ff << 0)

#define WIN_COLORMAP_1(_id)			(WIN_OFFSET(_id) + 0x18)
#define WIN_MAPCOLOR_G_F(_v)			((_v) << 16)
#define WIN_MAPCOLOR_G_MASK			(0x3ff << 16)
#define WIN_MAPCOLOR_B_F(_v)			((_v) << 0)
#define WIN_MAPCOLOR_B_MASK			(0x3ff << 0)

#define WIN_START_TIME_CON(_id)			(WIN_OFFSET(_id) + 0x1C)
#define WIN_START_TIME_CONTROL_F(_v)		((_v) << 0)
#define WIN_START_TIME_CONTROL_MASK		(0x3fff << 0)

//-------------------------------------------------------------------
// DECON_WINCON : 0~7
//-------------------------------------------------------------------
/* WIN index offset increment : 0x1000 */
#define DECON_CON_WIN(_id)			(WIN_OFFSET(_id) + 0x0)
/* Win 0~3, channel map 0~3
 * Win 4~7, channel map 0~15
 */
#define WIN_CHMAP_F(_ch)			(((_ch) & 0xF) << 4)
#define WIN_CHMAP_MASK				(0xF << 4)
#define WIN_MAPCOLOR_EN_F			(1 << 1)
#define _WIN_EN_F				(1 << 0)
#define WIN_EN_F(_win)				_WIN_EN_F

//-------------------------------------------------------------------
// DECON_SUB : COMP0  +  DSIMIF0
//-------------------------------------------------------------------
#define COMP_OFFSET(_i)				(0x0000 + 0x1000 * (_i))
#define DSIMIF_OFFSET(_i)			(0x8000 + 0x1000 * (_i))

#define DSC_CONTROL0(_id)			(COMP_OFFSET(_id) + 0x0000)
#define DSC_DCG_EN_REF(_v)			((_v) << 3)
#define DSC_DCG_EN_SSM(_v)			((_v) << 2)
#define DSC_DCG_EN_ICH(_v)			((_v) << 1)
#define DSC_DCG_EN_ALL_OFF			(0x0 << 1)
#define DSC_DCG_EN_ALL_MASK			(0x7 << 1)
#define DSC_DCG_EN(_v)				((_v) << 0)
#define DSC_DCG_EN_MASK				(0x1 << 0)

#define DSC_CONTROL1(_id)			(COMP_OFFSET(_id) + 0x0004)
#define DSC_SW_RESET				(0x1 << 28)
#define DSC_COMP_SEL_F(_v)			((_v) << 24)
#define DSC_COMP_SEL_MASK			(0x1 << 24)
#define DSC_BIT_SWAP(_v)			((_v) << 10)
#define DSC_BYTE_SWAP(_v)			((_v) << 9)
#define DSC_WORD_SWAP(_v)			((_v) << 8)
#define DSC_SWAP(_b, _c, _w)			((_b << 10) |\
							(_c << 9) | (_w << 8))
#define DSC_SWAP_MASK				((1 << 10) |\
							(1 << 9) | (1 << 8))
#define DSC_FLATNESS_DET_TH_MASK		(0xf << 4)
#define DSC_FLATNESS_DET_TH_F(_v)		((_v) << 4)
#define DSC_SLICE_MODE_CH_MASK			(0x1 << 1)
#define DSC_SLICE_MODE_CH_F(_v)			((_v) << 1)
#define DSC_DUAL_SLICE_EN_MASK			(0x1 << 0)
#define DSC_DUAL_SLICE_EN_F(_v)			((_v) << 0)

#define DSC_TZ_CONTROL(_id)			(COMP_OFFSET(_id) + 0x0008)
#define DSC_TZ_FLAG				(1 << 0)
#define DSC_TZ_SECURE				(0 << 0)
#define DSC_TZ_NONSECURE			(1 << 0)

#define DSC_CONTROL3(_id)			(COMP_OFFSET(_id) + 0x000C)
#define DSC_REMAINDER_F(_v)			((_v) << 12)
#define DSC_REMAINDER_MASK			(0x3 << 12)
#define DSC_REMAINDER_GET(_v)			(((_v) >> 12) & 0x3)
#define DSC_GRPCNTLINE_F(_v)			((_v) << 0)
#define DSC_GRPCNTLINE_MASK			(0x7ff << 0)
#define DSC_GRPCNTLINE_GET(_v)			(((_v) >> 0) & 0x7ff)

#define DSC_CRC_0(_id)				(COMP_OFFSET(_id) + 0x0010)
#define DSC_CRC_EN_MASK				(0x1 << 16)
#define DSC_CRC_EN_F(_v)			((_v) << 16)
#define DSC_CRC_CODE_MASK			(0xffff << 0)
#define DSC_CRC_CODE_F(_v)			((_v) << 0)

#define DSC_CRC_1(_id)				(COMP_OFFSET(_id) + 0x0014)
#define DSC_CRC_Y_S0_MASK			(0xffff << 16)
#define DSC_CRC_Y_S0_F(_v)			((_v) << 16)
#define DSC_CRC_CO_S0_MASK			(0xffff << 0)
#define DSC_CRC_CO_S0_F(_v)			((_v) << 0)

#define DSC_CRC_2(_id)				(COMP_OFFSET(_id) + 0x0018)
#define DSC_CRC_CG_S0_MASK			(0xffff << 16)
#define DSC_CRC_CG_S0_F(_v)			((_v) << 16)
#define DSC_CRC_Y_S1_MASK			(0xffff << 0)
#define DSC_CRC_Y_S1_F(_v)			((_v) << 0)

#define DSC_CRC_3(_id)				(COMP_OFFSET(_id) + 0x001C)
#define DSC_CRC_CO_S1_MASK			(0xffff << 16)
#define DSC_CRC_CO_S1_F(_v)			((_v) << 16)
#define DSC_CRC_CG_S1_MASK			(0xffff << 0)
#define DSC_CRC_CG_S1_F(_v)			((_v) << 0)

#define DSC_PPS00_03(_id)			(COMP_OFFSET(_id) + 0x0020)
#define PPS00_VER(_v)				((_v) << 24)
#define PPS00_VER_MASK				(0xff << 24)
#define PPS01_ID(_v)				(_v << 16)
#define PPS01_ID_MASK				(0xff << 16)
// PPS2_F [15:8] is seems to be reserved
#define PPS03_BPC_LBD_MASK			(0xffff << 0)
#define PPS03_BPC_LBD(_v)			(_v << 0)

#define DSC_PPS04_07(_id)			(COMP_OFFSET(_id) + 0x0024)
// Each bit has its own meaning in PPS04, bits [7:6] reserved, so mask is 0x3f
#define PPS04_COMP_CFG(_v)			((_v) << 24)
#define PPS04_COMP_CFG_MASK			(0x3f << 24)
#define PPS05_BPP(_v)				(_v << 16)
#define PPS05_BPP_MASK				(0xff << 16)
#define PPS06_07_PIC_HEIGHT_MASK		(0xffff << 0)
#define PPS06_07_PIC_HEIGHT(_v)			(_v << 0)

#define DSC_PPS08_11(_id)			(COMP_OFFSET(_id) + 0x0028)
#define PPS08_09_PIC_WIDHT_MASK			(0xffff << 16)
#define PPS08_09_PIC_WIDHT(_v)			((_v) << 16)
#define PPS10_11_SLICE_HEIGHT_MASK		(0xffff << 0)
#define PPS10_11_SLICE_HEIGHT(_v)		(_v << 0)

#define DSC_PPS12_15(_id)			(COMP_OFFSET(_id) + 0x002C)
#define PPS12_13_SLICE_WIDTH_MASK		(0xffff << 16)
#define PPS12_13_SLICE_WIDTH(_v)		((_v) << 16)
#define PPS14_15_CHUNK_SIZE_MASK		(0xffff << 0)
#define PPS14_15_CHUNK_SIZE(_v)			(_v << 0)

#define DSC_PPS16_19(_id)			(COMP_OFFSET(_id) + 0x0030)
// Considering [31:26] ie. [7:2] of MSB as reserved bits, mask will be 0x3ff
#define PPS16_17_INIT_XMIT_DELAY_MASK		(0x3ff << 16)
#define PPS16_17_INIT_XMIT_DELAY(_v)		((_v) << 16)
#define PPS18_19_INIT_DEC_DELAY_MASK		(0xffff << 0)
#define PPS18_19_INIT_DEC_DELAY(_v)		((_v) << 0)

#define DSC_PPS20_23(_id)			(COMP_OFFSET(_id) + 0x0034)
// Considering [31:22] ie. [7:0],[7:6] of MSB as reserved bits, mask will be 0x3f
#define PPS21_INIT_SCALE_VALUE_MASK		(0x3f << 16)
#define PPS21_INIT_SCALE_VALUE(_v)		((_v) << 16)
#define PPS22_23_SCALE_INC_INTERVAL_MASK	(0xffff << 0)
#define PPS22_23_SCALE_INC_INTERVAL(_v)		(_v << 0)

#define DSC_PPS24_27(_id)			(COMP_OFFSET(_id) + 0x0038)
// Considering [31:28] ie. [7:4] of MSB as reserved bits, mask will be 0xfff
#define PPS24_25_SCALE_DEC_INTERVAL_MASK	(0xfff << 16)
#define PPS24_25_SCALE_DEC_INTERVAL(_v)		((_v) << 16)
/* FL : First Line */
// Considering [15:5] ie. [7:0],[7,5] of MSB as reserved bits, mask will be 0x1f
#define PPS27_FL_BPG_OFFSET_MASK		(0x1f << 0)
#define PPS27_FL_BPG_OFFSET(_v)			(_v << 0)

#define DSC_PPS28_31(_id)			(COMP_OFFSET(_id) + 0x003C)
/* NFL : Not First Line */
#define PPS28_29_NFL_BPG_OFFSET_MASK		(0xffff << 16)
#define PPS28_29_NFL_BPG_OFFSET(_v)		((_v) << 16)
#define PPS30_31_SLICE_BPG_OFFSET_MASK		(0xffff << 0)
#define PPS30_31_SLICE_BPG_OFFSET(_v)		(_v << 0)

#define DSC_PPS32_35(_id)			(COMP_OFFSET(_id) + 0x0040)
#define PPS32_33_INIT_OFFSET_MASK		(0xffff << 16)
#define PPS32_33_INIT_OFFSET(_v)		((_v) << 16)
#define PPS34_35_FINAL_OFFSET_MASK		(0xffff << 0)
#define PPS34_35_FINAL_OFFSET(_v)		(_v << 0)

#define DSC_PPS36_39(_id)			(COMP_OFFSET(_id) + 0x0044)
// Considering [31:28] ie. [7:4] of MSB as reserved bits, mask is not 0xf??
#define PPS36_FLATNESS_MIN_QP_MASK		(0xff << 24)
#define PPS36_FLATNESS_MIN_QP(_v)		((_v) << 24)
// Considering [23:20] ie. [7:4] of MSB as reserved bits, mask is not 0xf??
#define PPS37_FLATNESS_MAX_QP_MASK		(0xff << 16)
#define PPS37_FLATNESS_MAX_QP(_v)		((_v) << 16)
#define PPS38_39_RC_MODEL_SIZE_MASK		(0xffff << 0)
#define PPS38_39_RC_MODEL_SIZE(_v)		(_v << 0)

#define DSC_PPS40_43(_id)			(COMP_OFFSET(_id) + 0x0048)
// Considering [31:28] ie. [7:4] of MSB as reserved bits, not considered for mask, TBD all mask
#define PPS40_RC_EDGE_FACTOR_MASK		(0xff << 24)
#define PPS40_RC_EDGE_FACTOR(_v)		((_v) << 24)
#define PPS41_RC_QUANT_INCR_LIMIT0_MASK		(0xff << 16)
#define PPS41_RC_QUANT_INCR_LIMIT0(_v)		((_v) << 16)
#define PPS42_RC_QUANT_INCR_LIMIT1_MASK		(0xff << 8)
#define PPS42_RC_QUANT_INCR_LIMIT1(_v)		((_v) << 8)
#define PPS44_RC_TGT_OFFSET_HI_MASK		(0xf << 4)
#define PPS44_RC_TGT_OFFSET_HI(_v)		((_v) << 4)
#define PPS44_RC_TGT_OFFSET_LO_MASK		(0xf << 0)
#define PPS44_RC_TGT_OFFSET_LO(_v)		((_v) << 0)

#define DSC_PPS44_47(_id)			(COMP_OFFSET(_id) + 0x004C)
#define PPS44_RC_BUF_THRESH_0_MASK		(0xff << 24)
#define PPS44_RC_BUF_THRESH_0(_v)		((_v) << 24)
#define PPS45_RC_BUF_THRESH_1_MASK		(0xff << 16)
#define PPS45_RC_BUF_THRESH_1(_v)		((_v) << 16)
#define PPS46_RC_BUF_THRESH_2_MASK		(0xff << 8)
#define PPS46_RC_BUF_THRESH_3(_v)		((_v) << 8)
#define PPS47_RC_BUF_THRESH_3_MASK		(0xff << 0)
#define PPS47_RC_BUF_THRESH_3(_v)		((_v) << 0)

#define DSC_PPS48_51(_id)			(COMP_OFFSET(_id) + 0x0050)
#define PPS48_RC_BUF_THRESH_4_MASK		(0xff << 24)
#define PPS48_RC_BUF_THRESH_4(_v)		((_v) << 24)
#define PPS49_RC_BUF_THRESH_5_MASK		(0xff << 16)
#define PPS49_RC_BUF_THRESH_5(_v)		((_v) << 16)
#define PPS50_RC_BUF_THRESH_6_MASK		(0xff << 8)
#define PPS50_RC_BUF_THRESH_6(_v)		((_v) << 8)
#define PPS51_RC_BUF_THRESH_7_MASK		(0xff << 0)
#define PPS51_RC_BUF_THRESH_7(_v)		((_v) << 0)

#define DSC_PPS52_55(_id)			(COMP_OFFSET(_id) + 0x0054)
#define PPS52_RC_BUF_THRESH_8_MASK		(0xff << 24)
#define PPS52_RC_BUF_THRESH_8(_v)		((_v) << 24)
#define PPS53_RC_BUF_THRESH_9_MASK		(0xff << 16)
#define PPS53_RC_BUF_THRESH_9(_v)		((_v) << 16)
#define PPS54_RC_BUF_THRESH_A_MASK		(0xff << 8)
#define PPS54_RC_BUF_THRESH_A(_v)		((_v) << 8)
#define PPS55_RC_BUF_THRESH_B_MASK		(0xff << 0)
#define PPS55_RC_BUF_THRESH_B(_v)		((_v) << 0)

#define DSC_PPS56_59(_id)			(COMP_OFFSET(_id) + 0x0058)
#define PPS56_RC_BUF_THRESH_C_MASK		(0xff << 24)
#define PPS56_RC_BUF_THRESH_C(_v)		((_v) << 24)
#define PPS57_RC_BUF_THRESH_D_MASK		(0xff << 16)
#define PPS57_RC_BUF_THRESH_D(_v)		((_v) << 16)
#define PPS58_RC_RANGE_PARAM_MASK		(0xff << 8)
#define PPS58_RC_RANGE_PARAM(_v)		((_v) << 8)
#define PPS59_RC_RANGE_PARAM_MASK		(0xff << 0)
#define PPS59_RC_RANGE_PARAM(_v)		((_v) << 0)
#define PPS58_59_RC_RANGE_PARAM_MASK		(0xFFFF << 0)
#define PPS58_59_RC_RANGE_PARAM(_v)		((_v) << 0)

#define DSC_PPS60_63(_id)			(COMP_OFFSET(_id) + 0x005C)
#define DSC_PPS64_67(_id)			(COMP_OFFSET(_id) + 0x0060)
#define DSC_PPS68_71(_id)			(COMP_OFFSET(_id) + 0x0064)
#define DSC_PPS72_75(_id)			(COMP_OFFSET(_id) + 0x0068)
#define DSC_PPS76_79(_id)			(COMP_OFFSET(_id) + 0x006C)
#define DSC_PPS80_83(_id)			(COMP_OFFSET(_id) + 0x0070)
#define DSC_PPS84_87(_id)			(COMP_OFFSET(_id) + 0x0074)


#define DSC_DEBUG_EN(_id)			(DSC_OFFSET(_id) + 0x0078)
#define DSC_DBG_EN_MASK                        (1 << 31)
#define DSC_DBG_EN(_v)                         ((_v) << 31)
#define DSC_DBG_SEL_MASK                       (0xffff << 0)
#define DSC_DBG_SEL(_v)                        ((_v) << 0)

#define DSC_DEBUG_DATA(_id)                    (COMP_OFFSET(_id) + 0x007C)

// DSIMIF (_id: 0)
#define DSIMIF_SEL(_id)				(DSIMIF_OFFSET(_id) + 0x0000)
#define SEL_DSIM(_v)				((_v) << 0)
#define SEL_DSIM_MASK				(0x7 << 0)
#define SEL_DSIM_GET(_v)			(((_v) >> 0) & 0x7)

#define DSIMIF_TE_TIMEOUT_CON(_id)		(DSIMIF_OFFSET(_id) + 0x0004)
#define TE_TIMEOUT_CNT(_v)			((_v) << 0)
#define TE_TIMEOUT_CNT_MASK			(0xffff << 0)
#define TE_TIMEOUT_CNT_GET(_v)			(((_v) >> 0) & 0xffff)

#define DSIMIF_START_TIME_CON(_id)		(DSIMIF_OFFSET(_id) + 0x0008)
#define START_TIME_CONTROL(_v)			((_v) << 0)

#define DSIMIF_LATE_START_ALARM_TH(_id)		(DSIMIF_OFFSET(_id) + 0x000C)
#define LATE_START_ALARM_TH(_v)			((_v) << 0)
#define LATE_START_ALARM_TH_GET(_v)		(((_v) >> 0) & 0xffffffff)

#define DSIMIF_VT0(_id)				(DSIMIF_OFFSET(_id) + 0x0020)
#define VT0_VACTIVE(_v)				((_v) << 16)
#define VT0_VACTIVE_MASK			(0xffff << 16)
#define VT0_VSA(_v)				((_v) << 0)
#define VT0_VSA_MASK				(0xffff << 0)

#define DSIMIF_VT1(_id)				(DSIMIF_OFFSET(_id) + 0x0024)
#define VT1_VBP(_v)				((_v) << 16)
#define VT1_VBP_MASK				(0xffff << 16)
#define VT1_VFP(_v)				((_v) << 0)
#define VT1_VFP_MASK				(0xffff << 0)

#define DSIMIF_VT2(_id)				(DSIMIF_OFFSET(_id) + 0x0028)
#define VT2_HACTIVE(_v)				((_v) << 16)
#define VT2_HACTIVE_MASK			(0xffff << 16)
#define VT2_HSA(_v)				((_v) << 0)
#define VT2_HSA_MASK				(0xffff << 0)

#define DSIMIF_VT3(_id)				(DSIMIF_OFFSET(_id) + 0x002C)
#define VT3_HBP(_v)				((_v) << 16)
#define VT3_HBP_MASK				(0xffff << 16)
#define VT3_HFP(_v)				((_v) << 0)
#define VT3_HFP_MASK				(0xffff << 0)

#define DSIMIF_VRR_CON0(_id)			(DSIMIF_OFFSET(_id) + 0x0030)
#define VRR_RELEASE_CNT(_v)			((_v) << 8)
#define VRR_RELEASE_CNT_MASK			(0xf << 8)
#define VRR_ALIGN_UNIT(_v)			((_v) << 0)
#define VRR_ALIGN_UNIT_MASK			(0x3f << 0)

#define DSIMIF_VRR_CON1(_id)			(DSIMIF_OFFSET(_id) + 0x0034)
#define VRR_EXPIRE_CNT(_v)			((_v) << 0)
#define VRR_EXPIRE_CNT_MASK			(0xffffffff << 0)

#define DSIMIF_VRR_CON2(_id)			(DSIMIF_OFFSET(_id) + 0x0038)
#define VRR_INITIAL_CNT(_v)			((_v) << 0)
#define VRR_INITIAL_CNT_MASK			(0xffffffff << 0)

#define DSIMIF_CRC_CON(_id)			(DSIMIF_OFFSET(_id) + 0x0080)
#define DSIMIF_CRC_DATA_R(_id)			(DSIMIF_OFFSET(_id) + 0x0084)
#define DSIMIF_CRC_DATA_G(_id)			(DSIMIF_OFFSET(_id) + 0x0088)
#define DSIMIF_CRC_DATA_B(_id)			(DSIMIF_OFFSET(_id) + 0x008C)


/* used commonly for DSIMIF & DPIF */
#define CRC_START				(1 << 0)
#define CRC_DATA_R_GET(_v)			(((_v) >> 0) & 0xffffffff)
#define CRC_DATA_G_GET(_v)			(((_v) >> 0) & 0xffffffff)
#define CRC_DATA_B_GET(_v)			(((_v) >> 0) & 0xffffffff)

//-------------------------------------------------------------------
// DECON_DQE : dqe_reg.c & regs-dqe.h
//-------------------------------------------------------------------
#define DQE0_DQE_TOP				(0x0000)
#define DQE0_DQE_GAMMA_MATRIX			(0x1400)

#define DQE0_SHADOW_OFFSET			(0x0200)

#endif /* _REGS_DECON_H */

