/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_soc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_DEP_SOC_V2_H
#define __SND_SOC_VTS_DEP_SOC_V2_H

/* SYSREG_VTS */
#define VTS_DEBUG			(0x0404)
#define VTS_DMIC_CLK_CTRL		(0x0408)
#define VTS_HWACG_CM4_CLKREQ	(0x0428)
#define VTS_DMIC_CLK_CON		(0x0434)
#define VTS_SYSPOWER_CTRL		(0x0500)
#define VTS_SYSPOWER_STATUS		(0x0504)
#define VTS_MIF_REQ_OUT			(0x0200)
#define VTS_MIF_REQ_ACK_IN		(0x0204)
#define VTS_SYSREG_YAMIN_DUMP		(0x0948)

/* VTS_DEBUG */
#define VTS_NMI_EN_BY_WDT_OFFSET	(0)
#define VTS_NMI_EN_BY_WDT_SIZE		(1)
/* VTS_DMIC_CLK_CTRL */
#define VTS_CG_STATUS_OFFSET            (5)
#define VTS_CG_STATUS_SIZE		(1)
#define VTS_CLK_ENABLE_OFFSET		(4)
#define VTS_CLK_ENABLE_SIZE		(1)
#define VTS_CLK_SEL_OFFSET		(0)
#define VTS_CLK_SEL_SIZE		(1)
/* VTS_HWACG_CM4_CLKREQ */
#define VTS_MASK_OFFSET			(0)
#define VTS_MASK_SIZE			(32)
/* VTS_DMIC_CLK_CON */
#define VTS_ENABLE_CLK_GEN_OFFSET       (0)
#define VTS_ENABLE_CLK_GEN_SIZE         (1)
#define VTS_SEL_EXT_DMIC_CLK_OFFSET     (1)
#define VTS_SEL_EXT_DMIC_CLK_SIZE       (1)
#define VTS_ENABLE_CLK_CLK_GEN_OFFSET   (14)
#define VTS_ENABLE_CLK_CLK_GEN_SIZE     (1)

/* VTS_SYSPOWER_CTRL */
#define VTS_SYSPOWER_CTRL_OFFSET	(0)
#define VTS_SYSPOWER_CTRL_SIZE		(1)
/* VTS_SYSPOWER_STATUS */
#define VTS_SYSPOWER_STATUS_OFFSET	(0)
#define VTS_SYSPOWER_STATUS_SIZE	(1)

#define VTS_DMIC_ENABLE_DMIC_IF		(0x0000)
#define VTS_DMIC_CONTROL_DMIC_IF	(0x0004)
/* VTS_DMIC_ENABLE_DMIC_IF */
#define VTS_DMIC_ENABLE_DMIC_IF_OFFSET	(31)
#define VTS_DMIC_ENABLE_DMIC_IF_SIZE	(1)
#define VTS_DMIC_PERIOD_DATA2REQ_OFFSET	(16)
#define VTS_DMIC_PERIOD_DATA2REQ_SIZE	(2)
/* VTS_DMIC_CONTROL_DMIC_IF */
#define VTS_DMIC_HPF_EN_OFFSET		(31)
#define VTS_DMIC_HPF_EN_SIZE		(1)
#define VTS_DMIC_HPF_SEL_OFFSET		(28)
#define VTS_DMIC_HPF_SEL_SIZE		(1)
#define VTS_DMIC_CPS_SEL_OFFSET		(27)
#define VTS_DMIC_CPS_SEL_SIZE		(1)
#define VTS_DMIC_GAIN_OFFSET		(24)
#define VTS_DMIC_1DB_GAIN_OFFSET	(21)
#define VTS_DMIC_GAIN_SIZE		(3)
#define VTS_DMIC_DMIC_SEL_OFFSET	(18)
#define VTS_DMIC_DMIC_SEL_SIZE		(1)
#define VTS_DMIC_RCH_EN_OFFSET		(17)
#define VTS_DMIC_RCH_EN_SIZE		(1)
#define VTS_DMIC_LCH_EN_OFFSET		(16)
#define VTS_DMIC_LCH_EN_SIZE		(1)
#define VTS_DMIC_SYS_SEL_OFFSET		(12)
#define VTS_DMIC_SYS_SEL_SIZE		(2)
#define VTS_DMIC_POLARITY_CLK_OFFSET	(10)
#define VTS_DMIC_POLARITY_CLK_SIZE	(1)
#define VTS_DMIC_POLARITY_OUTPUT_OFFSET	(9)
#define VTS_DMIC_POLARITY_OUTPUT_SIZE	(1)
#define VTS_DMIC_POLARITY_INPUT_OFFSET	(8)
#define VTS_DMIC_POLARITY_INPUT_SIZE	(1)
#define VTS_DMIC_OVFW_CTRL_OFFSET	(4)
#define VTS_DMIC_OVFW_CTRL_SIZE		(1)
#define VTS_DMIC_CIC_SEL_OFFSET		(0)
#define VTS_DMIC_CIC_SEL_SIZE		(1)

/* CM4 */
#define VTS_CM4_R(x)			(0x0010 + (x * 0x4))
#define VTS_CM4_PC			(0x0004)

#define VTS_IRQ_VTS_ERROR               (0)
#define VTS_IRQ_VTS_BOOT_COMPLETED      (1)
#define VTS_IRQ_VTS_IPC_RECEIVED        (2)
#define VTS_IRQ_VTS_VOICE_TRIGGERED     (3)
#define VTS_IRQ_VTS_PERIOD_ELAPSED      (4)
#define VTS_IRQ_VTS_REC_PERIOD_ELAPSED  (5)
#define VTS_IRQ_VTS_DBGLOG_BUFZERO      (6)
#define VTS_IRQ_VTS_DBGLOG_BUFONE       (7)
#define VTS_IRQ_VTS_AUDIO_DUMP          (8)
#define VTS_IRQ_VTS_LOG_DUMP            (9)
#define VTS_IRQ_COUNT                   (10)
#define VTS_IRQ_VTS_SLIF_DUMP           (11)
#define VTS_IRQ_VTS_CP_WAKEUP           (15)
#define VTS_IRQ_AP_IPC_RECEIVED         (16)
#define VTS_IRQ_AP_SET_DRAM_BUFFER      (17)
#define VTS_IRQ_AP_START_RECOGNITION    (18)
#define VTS_IRQ_AP_STOP_RECOGNITION     (19)
#define VTS_IRQ_AP_START_COPY           (20)
#define VTS_IRQ_AP_STOP_COPY            (21)
#define VTS_IRQ_AP_SET_MODE             (22)
#define VTS_IRQ_AP_POWER_DOWN           (23)
#define VTS_IRQ_AP_TARGET_SIZE          (24)
#define VTS_IRQ_AP_SET_REC_BUFFER       (25)
#define VTS_IRQ_AP_START_REC            (26)
#define VTS_IRQ_AP_STOP_REC             (27)
#define VTS_IRQ_AP_GET_VERSION		(28)
#define VTS_IRQ_AP_RESTART_RECOGNITION  (29)
#define VTS_IRQ_AP_COMMAND		(31)

#define VTS_DMIC_IF_ENABLE_DMIC_IF		(0x1000)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD0		(0x6)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD1		(0x7)
#define VTS_DMIC_IF_ENABLE_DMIC_AUD2		(0x8)

#define VTS_IRQ_LIMIT			(32)

#define VTS_BAAW_BASE			(0x60000000)
#define VTS_BAAW_SRC_START_ADDRESS	(0x10000)
#define VTS_BAAW_SRC_END_ADDRESS	(0x10004)
#define VTS_BAAW_REMAPPED_ADDRESS	(0x10008)
#define VTS_BAAW_INIT_DONE		(0x1000C)

#define SICD_SOC_DOWN_OFFSET	(0x18C)
#define SICD_MIF_DOWN_OFFSET	(0x19C)

#define BUFFER_BYTES_MAX (0xa0000)
#define PERIOD_BYTES_MIN (SZ_4)
#define PERIOD_BYTES_MAX (BUFFER_BYTES_MAX / 2)

#define SOUND_MODEL_SIZE_MAX (SZ_32K)
#define SOUND_MODEL_COUNT (3)

/* DRAM for copying VTS firmware logs */
#define LOG_BUFFER_BYTES_MAX	   (0x2000)
#define VTS_SRAMLOG_MSGS_OFFSET	   (0x00190000)
#define VTS_SRAMLOG_SIZE_MAX	   (SZ_2K)  /* SZ_2K : 0x800 */
#define VTS_SRAM_TIMELOG_SIZE_MAX  (SZ_1K)
#define VTS_SRAM_EVENTLOG_SIZE_MAX (VTS_SRAMLOG_SIZE_MAX - VTS_SRAM_TIMELOG_SIZE_MAX)  /* SZ_2K : 0x800 */

/* VTS firmware version information offset */
#define VTSFW_VERSION_OFFSET	(0xac)
#define DETLIB_VERSION_OFFSET	(0xa8)

#define PAD_RETENTION_VTS_OPTION  (0x30A0) // VTS_OUT : PAD__RTO
#define VTS_CPU_CONFIGURATION   (0x3640)
#define VTS_CPU_LOCAL_PWR_CFG			(0x00000001)
/* REMOVED */
#define VTS_CPU_IN      (0x3664)
/* REMOVED */
#define VTS_CPU_IN_SLEEPING_MASK		(0x00000002)

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))
#define DMIC_CLK_RATE (768000)
#define VTS_TRIGGERED_TIMEOUT_MS (5000)

#define VTS_DUMP_MAGIC "VTS-LOG0" // 0x30474F4C2D535456ull /* VTS-LOG0 */
#define VTS_SYS_CLOCK_MAX	(393216000)
#define VTS_SYS_CLOCK		(196608000)	/* 200M */
#define DEF_VTS_PCM_DUMP

#define VTS_DMIC_CLK_MUX	(49152000)
#define SLIF_DMIC_CLK_MUX	(73728000)
#define DUAL_DMIC_CLK_MUX	(49152000)

#define VTS_ERR_HARD_FAULT	(0x1)
#define VTS_ERR_BUS_FAULT	(0x3)

#define VTS_ITMON_NAME "CHUBVTS(VTS:YAMIN)" /* refer to bootloader/platform/s5e9925/debug/itmon.c */

#endif /* __SND_SOC_VTS_DEP_SOC_V2_H */
