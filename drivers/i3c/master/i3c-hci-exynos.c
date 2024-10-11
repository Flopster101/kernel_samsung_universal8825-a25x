// SPDX-License-Identifier: GPL-2.0-only
/*
 * i3c-hci-exynos.c - Samsung Exynos I3C HCI Controller Driver
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; You can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/i3c/master.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include "../../pinctrl/core.h"
#include <linux/pm_runtime.h>
#include "i3c-hci-exynos.h"

#ifdef CONFIG_CPU_IDLE
#include <soc/samsung/exynos-cpupm.h>
#endif

/* Register Map */
#define I3C_HCI_VERSION				0x00
#define I3C_HC_CONTROL				0x04
#define I3C_MASTER_DEVICE_ADDR			0x08
#define I3C_HC_CAPABILITIES			0x0C
#define I3C_RESET_CTRL				0x10
#define I3C_PRESENT_STATE			0x14
#define I3C_DAT_SECTION_OFFSET			0x30
#define I3C_DCT_SECTION_OFFSET			0x34
#define I3C_RH_SECTION_OFFSET			0x38	/* not support */
#define I3C_PIO_SECTION_OFFSET			0x3C
#define I3C_EXT_CAPS_SECTION_OFFSET		0x40
#define I3C_IBI_NOTIFY_CTRL			0x58
#define I3C_DEV_CTX_BASE_LO			0x60
#define I3C_DEV_CTX_BASE_HI			0x64
#define I3C_DEV_ADDR_TABLE(x)			((x) * 4 + 0x100)
#define I3C_DEV_CHAR_TABLE_0(x)			((x) * 16 + 0x200)
#define I3C_DEV_CHAR_TABLE_1(x)			((x) * 16 + 0x204)
#define I3C_DEV_CHAR_TABLE_2(x)			((x) * 16 + 0x208)
#define I3C_DEV_CHAR_TABLE_3(x)			((x) * 16 + 0x20C)
#define I3C_CMD_QUEUE				0x300
#define I3C_RESP_QUEUE				0x304
#define I3C_XFER_DATA_BUF			0x308
#define I3C_IBI_PORT				0x30C
#define I3C_QUEUE_THLD_CTRL			0x310
#define I3C_DATA_BUFFER_THLD_CTRL		0x314
#define I3C_QUEUE_SIZE				0x318
#define I3C_PIO_INTR_STATUS			0x320
#define I3C_PIO_INTR_EN				0x324
#define I3C_PIO_INTR_SIGNAL_EN			0x328
#define I3C_PIO_INTR_FORCE			0x32C

/* EXT Register Map */
#define I3C_EXTCAP_HDR1				0x2000
#define I3C_COMP_MANUF				0x2004
#define I3C_COMP_VERSION			0x2008
#define I3C_COMP_TYPE				0x200C
#define I3C_EXTCAP_HDR2				0x2010
#define I3C_MASTER_CONFIG			0x2014
#define I3C_EXTCAP_HDR3				0x2018
#define I3C_BUS_INSTANCE_COUNT			0x201C
#define I3C_BUS_INSTANCE_OFFSET			0x2020
#define I3C_EXTCAP_HDR4				0x2024
#define I3C_QUEUE_STATUS_LEVEL			0x2028
#define I3C_DATA_BUFFER_THLD_STATUS_LEVEL	0x202C
#define I3C_PRESENT_STATE_DEBUG			0x2030
#define I3C_MX_ERROR_COUNTERS			0x2034
#define	I3C_EXTCAP_HDR5				0x20FC
#define	I3C_EXT_DEV_ADDR_TABLE(x)		((x) * 4 + 0x2100)
#define	I3C_EXTCAP_HDR6				0x2180
#define	I3C_EXT_PIO_INTR_STATUS			0x2184
#define	I3C_EXT_PIO_INTR_EN			0x2188
#define	I3C_EXT_PIO_INTR_SIGNAL_EN		0x218C
#define	I3C_EXT_PIO_INTR_FORCE			0x2190
#define	I3C_EXTCAP_HDR_7			0x219C
#define I3C_EXT_DEVICE_CTRL			0x2198
#define	I3C_EXTCAP_HDR_8			0x219C
#define	I3C_EXT_RESET_CTRL			0x21A0
#define	I3C_EXTCAP_HDR_9			0x21A4
#define I3C_TRAILING_CYCLE			0x21A8
#define I3C_SCL_QUARTER_PERIOD			0x21AC
#define I3C_SCL_CTRL				0x21B0
#define I3C_TSU_STA				0x21B4
#define I3C_TCBP				0x21B8
#define I3C_TLOW				0x21BC
#define I3C_THD_STA				0x21C0
#define I3C_TBUF				0x21C4
#define I3C_BUS_CONDITION_CYCLE			0x21C8
#define I3C_SAMPLE_CYCLE			0x21CC
#define	I3C_EXTCAP_HDR_10			0x21D0
#define I3C_SCL_LOW_FORCE_STEP			0x21D4
#define I3C_EXTCAP_HDR_11			0x21D8
#define I3C_FILTER_CTRL				0x21DC
#define	I3C_EXTCAP_HDR_12			0x21E0
#define	I3C_VGPIO_TX_CONFIG			0x21E4
#define	I3C_VGPIO_TX_RESP			0x21E8
#define	I3C_EXTCAP_HDR_13			0x21EC
#define	I3C_VGPIO_RX_MASK(x)			((x) * 4 + 0x21F0)
#define I3C_EXTCAP_HDR_14			0x2210
#define I3C_HDR_DDR_RD_ABORT			0x2214
#define	I3C_EXTCAP_HDR_15			0x2218
#define	I3C_EVENT_COMMAND			0x221C
#define	I3C_ENTER_ACTIVITY_STATE		0x2220
#define	I3C_MAX_WRITE_LENGTH			0x2224
#define I3C_MAX_READ_LENGTH			0x2228
#define	I3C_DEVICE_COUNT			0x222C
#define	I3C_EXT_CURRENT_MASTER			0x2230
#define	I3C_TEST_MODE_BYTE			0x2234
#define	I3C_HDR_MODE				0x2238
#define I3C_CHAR_REG0				0x223C
#define I3C_CHAR_REG1				0x2240
#define	I3C_MAX_SPEED				0x2244
#define	I3C_MAX_RD_TURN				0x2248
#define	I3C_HDR_CAP				0x224C
#define	I3C_DATA_LEN_SLAVE_HDR			0x2250
#define	I3C_DATA_SLAVE_HDR_TS_TX_PERIOD		0x2254
#define	I3C_EXTCAP_HDR_16			0x22FC
#define I3C_IBI_TX_DATA_PORT			0x2300
#define I3C_IBI_TX_BUF_LEVEL			0x2304
#define	I3C_EXTCAP_HDR_17			0x230C
#define I3C_DEBUG_CMD_QUEUE_0(x)		((x) * 8 + 0x2310)
#define I3C_DEBUG_CMD_QUEUE_1(x)		((x) * 8 + 0x2314)
#define I3C_DEBUG_RESP_STAT_QUEUE(x)		((x) * 4 + 0x2330)
#define I3C_DEBUG_TX_DATA_BUF(x)		((x) * 4 + 0x2340)
#define I3C_DEBUG_RX_DATA_BUF(x)		((x) * 4 + 0x2380)
#define I3C_DEBUG_IBI_QUEUE(x)			((x) * 4 + 0x23C0)
#define I3C_DEBUG_IBI_TX_BUF(x)			((x)  + 0x2400)
#define	I3C_VGPIO_TX_MONITOR			0x2410
#define	I3C_VGPIO_RX_PAYLOAD			0x2414
#define	I3C_MASTER_STAT_0			0x2418
#define	I3C_MASTER_STAT_1			0x241C
#define	I3C_SLAVE_STAT				0x2420
#define I3C_EXTCAP_HDR18			0x2424
#define	I3C_TS_OE_DISABLE			0x2428
#define	I3C_CHICKEN				0x242C

/* I3C_HC_CONTROL(0x4) Register bits */

#define I3C_ENABLE				BIT(31)
#define I3C_ABORT				BIT(29)
#define I3C_HOT_JOIN_CTRL			BIT(8)
#define I3C_I2C_SLV_PRESENT			BIT(7)
#define I3C_IBA_INCLUDE				BIT(0)

/* I3C_EXT_DEV_CTRL(0x2198) */

#define I3C_HWACG_DISABLE_S			BIT(31)
#define I3C_VGPIO_TX_REQ_FORCE			BIT(22)
#define I3C_NOTIFY_VGPIO_RX			BIT(21)
#define I3C_VGPIO_ENABLE			BIT(20)
#define I3C_IBI_REQ				BIT(15)
#define I3C_HJ_REQ				BIT(14)
#define I3C_MASTERSHIP_REQ			BIT(13)
#define I3C_SLAVE_MODE				BIT(12)
#define I3C_DATA_SWAP				BIT(10)
#define I3C_GETMXDS_5BYTE			BIT(9)
#define I3C_TX_DMA_FREE_RUN			BIT(8)
#define I3C_RX_STALL_EN				BIT(7)
#define I3C_TX_STALL_EN				BIT(6)
#define I3C_RX_DMA_EN				BIT(5)
#define I3C_TX_DMA_EN				BIT(4)
#define I3C_ADDR_OPT_EN				BIT(2)
#define I3C_SLOW_BUS				BIT(0)

/* I3C_MASTER_DEVICE_ADDR(0x8) */

#define I3C_DYNAMIC_ADDR_VALID			BIT(31)
#define I3C_DYNAMIC_ADDR(x)			(((x) << 16) & GENMASK(22, 16))

/* I3C_HC_CAPABILITIES(0xC) */

#define I3C_HDR_TS_EN				BIT(7)
#define I3C_HDR_DDR_EN				BIT(6)
#define I3C_NON_CURRENT_MASTER_CAP		BIT(5)
#define I3C_AUTO_CMD				BIT(3)
#define I3C_COMB_CMD				BIT(2)

/* I3C_RESET_CTRL(0x10) */

#define I3C_IBI_QUEUE_RST			BIT(5)
#define I3C_RX_FIFO_RST				BIT(4)
#define I3C_TX_FIFO_RST				BIT(3)
#define I3C_RESP_QUEUE_RST			BIT(2)
#define I3C_CMD_QUEUE_RST			BIT(1)
#define I3C_SOFT_RST				BIT(0)

/* I3C_EXT_RESET_CTRL(0x21A0) */

#define	I3C_SCL_LOW_FORCE			BTT(8)
#define I3C_IBI_TX_FIFO_RST			BIT(5)

/* I3C_PRESENT_STATE(0x14) */

#define I3C_CURRENT_MASTER			BIT(2)

/* I3C_PRESENT_STATE_DEBUG(0x2030) */

#define I3C_CMD_TID_STATUS			GENMASK(27, 24)
#define I3C_CM_TFR_ST_STATUS			GENMASK(21, 16)
#define I3C_CM_TFR_STATUS			GENMASK(13, 8)
#define I3C_SCL_LINE_SIGNAL_LV			BIT(0)
#define I3C_SDA_LINE_SIGNAL_LV			BIT(1)


/* I3C_PIO_INTR_STATUS(0x320) */

#define I3C_TRANSFER_ERR_STAT			BIT(9)
#define I3C_TRANSFER_ABORT_STAT			BIT(5)
#define I3C_RESP_READY_STAT			BIT(4)
#define I3C_CMD_QUEUE_READY_STAT		BIT(3)
#define I3C_IBI_THLD_STAT			BIT(2)
#define I3C_RX_THLD_STAT			BIT(1)
#define I3C_TX_THLD_STAT			BIT(0)

/* I3C_EXT_PIO_INTR_STATUS(0x2184) */
#define I3C_SCL_LOW_FORCE_DONE_STAT		BIT(31)
#define I3C_VGPIO_TX_PARITY_ERR_STAT		BIT(22)
#define I3C_VGPIO_RX_STAT			BIT(21)
#define I3C_VGPIO_TX_STAT			BIT(20)
#define I3C_SLAVE_INT_ACKED			BIT(18)
#define I3C_GETACCMST_STAT			BIT(17)
#define I3C_BUS_AVAIL_STAT			BIT(16)
#define I3C_BUS_IDLE_STAT			BIT(15)
#define I3C_TRAILING_STAT			BIT(12)
#define I3C_BUS_AVAIL_VIOLATION_STAT		BIT(11)
#define I3C_BUS_IDLE_VIOLATION_STAT		BIT(10)

#define I3C_INTR_ALL				(I3C_RESP_READY_STAT | \
						I3C_TRANSFER_ABORT_STAT | \
						I3C_TRANSFER_ERR_STAT)

#define I3C_EXT_INTR_ALL			(I3C_BUS_IDLE_VIOLATION_STAT | \
						I3C_BUS_AVAIL_VIOLATION_STAT | \
						I3C_TRAILING_STAT | \
						I3C_BUS_IDLE_VIOLATION_STAT | \
						I3C_BUS_AVAIL_VIOLATION_STAT | \
						I3C_GETACCMST_STAT | \
						I3C_SLAVE_INT_ACKED | \
						I3C_VGPIO_TX_STAT | \
						I3C_VGPIO_RX_STAT | \
						I3C_VGPIO_TX_PARITY_ERR_STAT | \
						I3C_SCL_LOW_FORCE_DONE_STAT)

/* I3C_PIO_INTR_EN(0x324) */

#define I3C_TRANSFER_ERR_STAT_EN		BIT(9)
#define I3C_TRANSFER_ABORT_STAT_EN		BIT(5)
#define I3C_RESP_READY_STAT_EN			BIT(4)
#define I3C_CMD_QUEUE_READY_STAT_EN		BIT(3)
#define I3C_IBI_THLD_STAT_EN			BIT(2)
#define I3C_RX_THLD_STAT_EN			BIT(1)
#define I3C_TX_THLD_STAT_EN			BIT(0)

/* I3C_EXT_PIO_INTR_EN(0x2188) */

#define I3C_SCL_LOW_FORCE_DONE_STAT_EN		BIT(31)
#define I3C_VGPIO_TX_PARITY_STAT_EN		BIT(22)
#define I3C_VGPIO_RX_STAT_EN			BIT(21)
#define I3C_VGPIO_TX_STAT_EN			BIT(20)
#define I3C_SLAVE_INT_ACKED_EN			BIT(18)
#define I3C_GETACCMST_STAT_EN			BIT(17)
#define I3C_BUS_AVAIL_STAT_EN			BIT(16)
#define I3C_BUS_IDLE_STAT_EN			BIT(15)
#define I3C_TRAILING_STAT_EN			BIT(12)
#define I3C_BUS_AVAIL_VIOLATION_STAT_EN		BIT(11)
#define I3C_BUS_IDLE_VIOLATION_STAT_EN		BIT(10)

/* I3C_PIO_INTR_SIGNAL_EN(0x328) */

#define I3C_TRANSFER_ERR_SIG_EN			BIT(9)
#define I3C_TRANSFER_ABORT_SIG_EN		BIT(5)
#define I3C_RESP_READY_SIG_EN			BIT(4)
#define I3C_CMD_QUEUE_READY_SIG_EN		BIT(3)
#define I3C_IBI_THLD_SIG_EN			BIT(2)
#define I3C_RX_THLD_SIG_EN			BIT(1)
#define I3C_TX_THLD_SIG_EN			BIT(0)


/* I3C_EXT_PIO_INTR_SIGNAL_EN(0x218C) */

#define I3C_SCL_LOW_FORCE_DONE_SIG_EN		BIT(31)
#define I3C_VGPIO_TX_PARITY_SIG_EN		BIT(22)
#define I3C_VGPIO_RX_SIG_EN			BIT(21)
#define I3C_VGPIO_TX_SIG_EN			BIT(20)
#define I3C_SLAVE_INT_ACKED_SIG_EN		BIT(18)
#define I3C_GETACCMST_SIG_EN			BIT(17)
#define I3C_BUS_AVAIL_SIG_EN			BIT(16)
#define I3C_BUS_IDLE_SIG_EN			BIT(15)
#define I3C_TRAILING_SIG_EN			BIT(12)
#define I3C_BUS_AVAIL_VIOLATION_SIG_EN		BIT(11)
#define I3C_BUS_IDLE_VIOLATION_SIG_EN		BIT(10)


/* I3C_PIO_INTR_FORCE(0x32C) */

#define I3C_TRANSFER_ERR_FORCE			BIT(9)
#define I3C_TRANSFER_ABORT_FORCE		BIT(5)
#define I3C_RESP_READY_FORCE			BIT(4)
#define I3C_CMD_QUEU_READY_FORCE		BIT(3)
#define I3C_IBI_THLD_FORCE			BIT(2)
#define I3C_RX_THLD_FORCE			BIT(1)
#define I3C_TX_THLD_FORCE			BIT(0)

/* I3C_EXT_PIO_INTR_FORCE(0x2190) */

#define I3C_SCL_LOW_FORCE_DONE_FORCE		BIT(31)
#define I3C_VGPIO_TX_PARITY_FORCE		BIT(22)
#define I3C_VGPIO_RX_FORCE			BIT(21)
#define I3C_VGPIO_TX_FORCE			BIT(20)
#define I3C_SLAVE_INT_ACKED_FORCE		BIT(18)
#define I3C_GETACCMST_FORCE			BIT(17)
#define I3C_BUS_AVAIL_FORCE			BIT(16)
#define I3C_BUS_IDLE_FORCE			BIT(15)
#define I3C_TRAILING_FORCE			BIT(12)
#define I3C_BUS_AVAIL_VIOLATION_FORCE		BIT(11)
#define I3C_BUS_IDLE_VIOLATION_FORCE		BIT(10)

/* I3C_QUEUE_THLD_CTRL(0x310) */

#define I3C_IBI_STAT_QUEUE_THLD(x)		(((x) << 24) & GENMASK(31, 24))
#define	IBI_DATA_THLD(x)			(((x) << 16) & GENMASK(23, 16))
#define RESP_BUF_THLD(x)			(((x) << 8) & GENMASK(15, 8))
#define RESP_BUF_THLD_MASK			GENMASK(15, 8)
#define I3C_CMD_EMPTY_BUF_THLD(x)		(((x) << 0) & GENMASK(7, 0))

/* I3C_DATA_BUFFER_THLD_CTRL(0x314) */

#define I3C_RX_START_THLD(x)			(((x) << 24) & GENMASK(26, 24))
#define I3C_TX_START_THLD(x)			(((x) << 16) & GENMASK(18, 16))
#define I3C_RX_BUF_THLD(x)			(((x) << 8) & GENMASK(10, 8))
#define I3C_TX_BUF_THLD(x)			(((x) << 0) & GENMASK(2, 0))

/* I3C_QUEUE_SIZE(0x318) */

#define I3C_TX_DATA_BUFF_SIZE			GENMASK(31, 24)
#define I3C_RX_DATA_BUFF_SIZE			GENMASK(23, 16)
#define I3C_IBI_STATUS_SIZE			GENMASK(15, 8)

/* I3C_QUEUE_STATUS_LEVEL(0x2028) */

#define I3C_IBI_STAT_CNT_LEVEL			GENMASK(28, 24)
#define I3C_IBI_BUF_LEVEL			GENMASK(23, 16)
#define RESP_BUFFER_LEVEL			GENMASK(15, 8)
#define I3C_CMD_QUEUE_FREE_LEVEL		GENMASK(7, 0)

/* I3C_DATA_BUF_LEVEL(0x2304) */

#define I3C_TX_BUF_LEVEL			GENMASK(20, 16)

/* I3C_DATA_BUF_STATUS_LEVEL(0x202C) */

#define I3C_RX_BUF_LVL				GENMASK(15, 8)
#define I3C_TX_BUF_FREE_LVL			GENMASK(7, 0)

/* I3C_IBI_NOTIFY_CTRL(0x58) */

#define I3C_NOTIFY_SIR_REJECTED			BIT(3)
#define I3C_NOTIFY_MR_REJECTED			BIT(1)
#define I3C_NOTIFY_HJ_REJECTED			BIT(0)

/* I3C_SCL_QUARTER_PERIOD(0x21AC) */

#define I3C_FM					GENMASK(31, 24)
#define I3C_FM_PLUS				GENMASK(23, 16)
#define I3C_OPEN_DRAIN				GENMASK(15, 8)
#define I3C_PUSH_PULL				GENMASK(7, 0)

/* I3C_SCL_CTRL(0x21B0) */

#define I3C_PP_HCNT				GENMASK(7, 0)

/* I3C_TSU_STA(0x21B4) */

#define I3C_FM_tSU_STA				GENMASK(23, 16)
#define I3C_FM_PLUS_tSU_STA			GENMASK(15, 8)
#define I3C_tCBSr				GENMASK(7, 0)

/* I3C_TCBP(0x21B0) */

#define I3C_TCBP_MIXED				GENMASK(23, 16)
#define I3C_TCBP_PURE				GENMASK(7, 0)

/* I3C_TLOW(0x21BC) */

#define I3C_FM_tLOW				GENMASK(24, 16)
#define I3C_FM_PLUS_tLOW			GENMASK(15, 8)
#define I3C_tLOW_OD				GENMASK(7, 0)

/* I3C_THD_STA(0x21C0) */

#define I3C_FM_tHD_STA				GENMASK(23, 16)
#define I3C_FM_PLUS_tHD_STA			GENMASK(15, 8)
#define I3C_tCAS				GENMASK(7, 0)

/* I3C_TBUF(0x21C4) */

#define I3C_TBUF_MIXED				GENMASK(24, 16)
#define I3C_TBUF_PURE				GENMASK(8, 0)

/* I3C_BUS_CONDITION_CYCLE(0x21C8) */

#define I3C_BUS_AVAIL_CYCLE			GENMASK(31, 24)
#define I3C_BUS_IDLE_CYCLE			GENMASK(23, 0)

/* I3C_SAMPLE_CYCLE(0x21CC) */

#define I3C_HDR_TS_SDA_SAMPLE_CYCLE_FOR_OE	GENMASK(13, 12)
#define I3C_HDR_TS_SCL_SAMPLE_CYCLE_FOR_OE	GENMASK(9, 8)
#define I3C_HDR_TS_SAMPLE_CYCLE			GENMASK(6, 4)
#define I3C_SDA_SAMPLE_CYCLE			GENMASK(1, 0)

/* I3C_FILTER_CTRL(0x218D) */

#define I3C_FILTER_CYCLE_SDA			GENMASK(23, 20)
#define I3C_FILTER_EN_SDA			BIT(16)
#define I3C_FILTER_CYCLE_SCL			GENMASK(7, 4)
#define I3C_FILTER_EN_SCL			BIT(0)

/* I3C_VGPIO_TX_CONFIG(0x21E4) */

#define I3C_M_VGPIO_TX_DATA_LEN			GENMASK(24, 23)
#define I3C_S_VGPIO_TX_DATA_LEN			GENMASK(22, 21)
#define I3C_VGPIO_TX_DELAY_STEP			GENMASK(20, 16)
#define I3C_VGPIO_TX_PARITY_ERR_IDX		GENMASK(12, 8)
#define I3C_VGPIO_TX_CCC			GENMASK(7, 0)

/* I3C_DEV_ADDR_TABLE(0x100 ~  )*/

#define I3C_DEVICE_TYPE				BIT(31)
#define I3C_NACK_RETRY_CNT(x)			(((x) << 29) & GENMASK(30, 29))
#define I3C_NACK_RETRY_CNT_MASK			GENMASK(30, 29)
#define I3C_DYNAMIC_ADDRESS(x)			(((x) << 16) & GENMASK(26, 16))
#define I3C_DYNAMIC_ADDRESS_MASK		GENMASK(23, 16)
#define I3C_GET_DYNAMIC_ADDRESS(x)		((x) >> 16 & GENMASK(7, 0))
#define I3C_MR_REJECT				BIT(14)
#define I3C_SIR_REJECT				BIT(13)
#define I3C_IBI_PAYLOAD				BIT(12)
#define I3C_STATIC_ADDRESS_MASK			GENMASK(6, 0)
#define I3C_STATIC_ADDRESS(x)			(((x) << 0) & GENMASK(6, 0))

/* I3C_EXT_DEV_ADDR_TABLE(0x2100 ~ )*/
#define I3C_IBI_PAYLOAD_SIZE(x)			(((x) << 16) & GENMASK(23, 16))
#define I3C_IS_VGPIO_ALIVE			BIT(12)
#define I3C_DYNAMIC_ADDR_ASSIGNED		BIT(7)

/* I3C_DEV_CHAR_TABLE_1(0x204 ~ ) */

#define I3C_PID_1				GENMASK(31, 0)

/* I3C_DEV_CHAR_TABLE_1(0x204 ~ ) */

#define I3C_PID_0				GENMASK(15, 0)

/* I3C_DEV_CHAR_TABLE_2(0x208 ~ */

#define I3C_DCR					GENMASK(15, 8)
#define I3C_BCR					GENMASK(7, 0)

/* I3C_DEV_CHAR_TABLE_3(0x20C ~ */

#define I3C_CHAR_DYNAMIC_ADDR			GENMASK(7, 0)

/* I3C_CMD_QUEUE(0x300) */

#define I3C_TOC					BIT(31)
#define I3C_ROC					BIT(30)
#define I3C_RNW					BIT(29)
#define I3C_CP					BIT(15)

#define I3C_DEV_COUNT(x)			(((x) << 26) & GENMASK(29, 26))
#define I3C_MODE(x)				(((x) << 26) & GENMASK(28, 26))
#define I3C_BYTE_CNT(x)				(((x) << 23) & GENMASK(25, 23))
#define I3C_DEVICE_INDEX(x)			(((x) << 16) & GENMASK(20, 16))
#define I3C_CMD_CODE(x)				(((x) << 7) & GENMASK(14, 7))
#define I3C_CMD_TID(x)				(((x) << 3) & GENMASK(6, 3))
#define I3C_CMD_ATTR(x)				((x) & GENMASK(2, 0))
#define I3C_CMD_ATTR_R				0
#define I3C_CMD_ATTR_IM				1
#define I3C_CMD_ATTR_A				2
#define I3C_CMD_ATTR_C				3
#define I3C_CMD_ATTR_IC				7

//#define I3C_HDR_DDR			BIT(1)
//#define I3C_HDR_TS			BIT(2)
#define I3C_CMD_MAX_LEN				(127)


/* I3C_HDR_DDR_RD_ABORT(0x2214) */
#define HDR_EXIT				BIT(31)
#define HDR_DDR_RD_ABORT			BIT(0)

/* I3C_RESP_QUEUE(0x304) */
#define RESP_ERR_STAT(x)			(((x) & GENMASK(31, 28)) >> 28)
#define RESP_CMDID(x)				(((x) & GENMASK(27, 24)) >> 24)
#define RESP_DATA_LENGTH(x)			((x) & GENMASK(15, 0))

/* I3C_IBI_QUEUE */

#define I3C_IBI_STAT				BIT(0)
#define I3C_DATA_LENGTH(x)			((x)  & 0xff)
#define I3C_IBI_ID(x)				(((x) >> 8) & 0xff)
#define I3C_IBI_ADDR(x)				(((x) >> 9) & 0x7f)
#define I3C_IBI_RNW(x)				(((x) >> 8) & 0x1)

#define I3C_RX_CRC_ERROR			1
#define I3C_RX_PARITY_ERROR			2
#define I3C_RX_FRAME_ERROR			3
#define I3C_ADDR_HEADER_NACK_M2			4
#define I3C_NACK				5
#define I3C_OVERFLOW_OR_UNDERFLOW		6
#define I3C_ABORTED_BY_USING_ABORT		8


#define I3C_PURE_BUS 0
#define I3C_MIXED_FAST_BUS 2
#define I3C_MIXED_SLOW_BUS 3

#define CMD_QUEUE_DEPTH				4
#define RESP_STAT_QUEUE_DEPTH			4
#define RX_FIFO_DEPTH				16
#define TX_FIFO_DEPTH				16
#define IBI_STAT_QUEUE_DEPTH			16
#define IBI_DATA_BUFFER_DEPTH			16

#define I3C_CCC	0
#define I3C_READ	1
#define I3C_WRITE	2

#define EXYNOS_I3C_TIMEOUT (msecs_to_jiffies(1000))
#define EXYNOS_FIFO_SIZE		32
#define DEFAULT_CLK 200000000

#define RX_FRAME_ERROR_HDR_DDR 0
#define RX_CRC_ERROR_HDR_DDR 1
#define RX_PARITY_ERROR_HDR 2
#define TX_DATA_MISMATCH_M1 3
#define ADDR_HEADER_NACK_M2 4
#define ADDR_NACK 5
#define SYMBOL_2_ERROR_HDR_TS 6
#define ABORT 7
#define RX_OVERFLOW_TX_UNDERFLOW 8

#define I3C_OPEN_DRAIN_CLOCK 1000000

static void exynos_i3c_hci_master_enable(struct exynos_i3c_hci_master *master);
static int exynos_i3c_hci_master_disable(struct exynos_i3c_hci_master *master);

static inline void dump_i3c_dat(struct exynos_i3c_hci_master *i3c)
{
	dev_info(i3c->dev, "register DAT\n"
		"DAT0						0x%08x   "
		"DAT1						0x%08x   "
		"DAT2						0x%08x   "
		"DAT3						0x%08x\n   "
		"DAT4						0x%08x   "
		"DAT5						0x%08x   "
		"DAT6						0x%08x   "
		"DAT7						0x%08x\n   "
		"EXTDAT0					0x%08x   "
		"EXTDAT1					0x%08x   "
		"EXTDAT2					0x%08x   "
		"EXTDAT3					0x%08x\n   "
		"EXTDAT4					0x%08x   "
		"EXTDAT5					0x%08x   "
		"EXTDAT6					0x%08x   "
		"EXTDAT7					0x%08x\n   "
		"free_rr_slots		0x%08x	"
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(0))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(1))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(2))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(3))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(4))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(5))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(6))
		, readl(i3c->regs + I3C_DEV_ADDR_TABLE(7))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(0))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(1))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(2))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(3))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(4))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(5))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(6))
		, readl(i3c->regs + I3C_EXT_DEV_ADDR_TABLE(7))
		, i3c->free_rr_slots
	);
}

static inline void dump_i3c_register(struct exynos_i3c_hci_master *i3c)
{
	dev_info(i3c->dev, "register dump(suspended : %d)\n"
		"HC_CONTROL						0x%08x   "
		"MASTER_DEVICE_ADDR					0x%08x   "
		"RESET_CTRL							0x%08x   "
		"PRESENT_STATE					0x%08x   \n"
		"INT_STATUS							0x%08x   "
		"INT_EN									0x%08x   "
		"INT_SIGNAL_EN					0x%08x   "
		"QUEUE_THLD_CTRL				0x%08x   \n"
		"DATA_BUFFER_THLD_CTRL	0x%08x   "
		"QUEUE_STATUS_LV				0x%08x   "
		"DATA_BUF_LEVEL					0x%08x   "
		"IBI_QUEUE_CTRL					0x%08x   \n"
		"TRAILING_CYCLE					0x%08x   "
		"SCL_QUARTER_PERIOD			0x%08x   "
		"SCL_CTRL								0x%08x   "
		"TSU_STA								0x%08x   \n"
		"TCBP										0x%08x   "
		"TLOW										0x%08x   "
		"THD_STA								0x%08x   "
		"TBUF										0x%08x   \n"
		"BUS_CONDITION_CYCLE		0x%08x   "
		"SAMPLE_CYCLE						0x%08x   "
		"FILTER_CTRL						0x%08x   "
		"PRESENT_STATE_DEBUG					0x%08x   \n"
		, i3c->suspended
		, readl(i3c->regs + I3C_HC_CONTROL)
		, readl(i3c->regs + I3C_MASTER_DEVICE_ADDR)
		, readl(i3c->regs + I3C_RESET_CTRL)
		, readl(i3c->regs + I3C_PRESENT_STATE)
		, readl(i3c->regs + I3C_PIO_INTR_STATUS)
		, readl(i3c->regs + I3C_PIO_INTR_EN)
		, readl(i3c->regs + I3C_PIO_INTR_SIGNAL_EN)
		, readl(i3c->regs + I3C_QUEUE_THLD_CTRL)
		, readl(i3c->regs + I3C_DATA_BUFFER_THLD_CTRL)
		, readl(i3c->regs + I3C_QUEUE_STATUS_LEVEL)
		, readl(i3c->regs + I3C_DATA_BUFFER_THLD_STATUS_LEVEL)
		, readl(i3c->regs + I3C_IBI_NOTIFY_CTRL)
		, readl(i3c->regs + I3C_TRAILING_CYCLE)
		, readl(i3c->regs + I3C_SCL_QUARTER_PERIOD)
		, readl(i3c->regs + I3C_SCL_CTRL)
		, readl(i3c->regs + I3C_TSU_STA)
		, readl(i3c->regs + I3C_TCBP)
		, readl(i3c->regs + I3C_TLOW)
		, readl(i3c->regs + I3C_THD_STA)
		, readl(i3c->regs + I3C_TBUF)
		, readl(i3c->regs + I3C_BUS_CONDITION_CYCLE)
		, readl(i3c->regs + I3C_SAMPLE_CYCLE)
		, readl(i3c->regs + I3C_FILTER_CTRL)
		, readl(i3c->regs + I3C_PRESENT_STATE_DEBUG)
	);
	dump_i3c_dat(i3c);
}

static void exynos_i3c_hci_master_wr_to_tx_fifo(struct exynos_i3c_hci_master *master,
		const u8 *tx_buf, int nbytes)
{
	writesl(master->regs + I3C_XFER_DATA_BUF, tx_buf, nbytes / 4);
	if (nbytes & 3) {
		u32 tmp = 0;

		memcpy(&tmp, tx_buf + (nbytes & ~3), nbytes & 3);
		writesl(master->regs + I3C_XFER_DATA_BUF, &tmp, 1);
	}
}

static void exynos_i3c_hci_master_rd_from_rx_fifo(struct exynos_i3c_hci_master *master,
					    u8 *rx_buf, int nbytes)
{
	if (nbytes == 0)
		return;

	readsl(master->regs + I3C_XFER_DATA_BUF, rx_buf, nbytes / 4);
	if (nbytes & 3) {
		u32 tmp;

		readsl(master->regs + I3C_XFER_DATA_BUF, &tmp, 1);
		memcpy(rx_buf + (nbytes & ~3), &tmp, nbytes & 3);
	}
}

static void exynos_i3c_hci_master_start_xfer_locked(struct exynos_i3c_hci_master *master)
{
	struct exynos_i3c_hci_xfer *xfer = master->xfer_queue.xfer;
	unsigned int i;
	unsigned int queue_thld;

	dev_dbg(master->dev, "Start %s\n", __func__);

	if (!xfer)
		return;

	for (i = 0; i < xfer->ncmds; i++) {
		struct exynos_i3c_hci_cmd *cmd = &xfer->cmds[i];

		exynos_i3c_hci_master_wr_to_tx_fifo(master, cmd->tx_buf,
				cmd->tx_len);
	}

	writel(I3C_RESP_READY_STAT_EN | I3C_TRANSFER_ERR_STAT_EN | I3C_TRANSFER_ABORT_STAT_EN | I3C_IBI_THLD_STAT_EN, master->regs + I3C_PIO_INTR_EN);
	writel(I3C_RESP_READY_STAT_EN | I3C_TRANSFER_ERR_STAT_EN | I3C_TRANSFER_ABORT_STAT_EN | I3C_IBI_THLD_STAT_EN, master->regs + I3C_PIO_INTR_SIGNAL_EN);

	queue_thld = readl(master->regs + I3C_QUEUE_THLD_CTRL);
	queue_thld &= ~RESP_BUF_THLD_MASK;
	queue_thld |= RESP_BUF_THLD(xfer->ncmds);
	writel(queue_thld, master->regs + I3C_QUEUE_THLD_CTRL);

	for (i = 0; i < xfer->ncmds; i++) {
		struct exynos_i3c_hci_cmd *cmd = &xfer->cmds[i];

		writel(cmd->cmd | I3C_CMD_TID(i),
		       master->regs + I3C_CMD_QUEUE);
		/* For Address Assign command, do not send data length */
		if(I3C_CMD_ATTR(cmd->cmd) == I3C_CMD_ATTR_A)
			continue;
		if(cmd->cmd & I3C_RNW)  {
			writel(cmd->rx_len << 16,
			       master->regs + I3C_CMD_QUEUE);
		}
		else{
			writel(cmd->tx_len << 16,
			       master->regs + I3C_CMD_QUEUE);
		}
	}

	dev_dbg(master->dev, "End %s\n", __func__);
}

static void exynos_i3c_hci_master_unqueue_xfer_locked(struct exynos_i3c_hci_master *master,
					 struct exynos_i3c_hci_xfer *xfer)
{
	if (master->xfer_queue.xfer == xfer) {
		exynos_i3c_hci_master_disable(master);
		master->xfer_queue.xfer = NULL;
		writel(I3C_CMD_QUEUE_RST | I3C_RESP_QUEUE_RST | I3C_TX_FIFO_RST |
		       I3C_RX_FIFO_RST,
		       master->regs + I3C_RESET_CTRL);
		writel(0, master->regs + I3C_PIO_INTR_EN);
		exynos_i3c_hci_master_enable(master);
	} else {
		list_del_init(&xfer->node);
	}

}

static void exynos_i3c_hci_master_unqueue_xfer(struct exynos_i3c_hci_master *master,
					 struct exynos_i3c_hci_xfer *xfer)
{
	unsigned long flags;

	spin_lock_irqsave(&master->xfer_queue.lock, flags);
	exynos_i3c_hci_master_unqueue_xfer_locked(master, xfer);
	spin_unlock_irqrestore(&master->xfer_queue.lock, flags);
}

static void exynos_i3c_hci_master_end_xfer_locked(struct exynos_i3c_hci_master *master,
					    u32 isr)
{
	struct exynos_i3c_hci_xfer *xfer = master->xfer_queue.xfer;
	int i, ret = 0;
	struct exynos_i3c_hci_cmd *cmd;
	unsigned int queue_status;
	unsigned int rx_len;
	unsigned int resp;

	dev_dbg(master->dev, "Start %s\n", __func__);

	if (!xfer)
		return;

	/* TODO : Dequeueing RESP until RESP is EMPTY because there are multiple cmd in one Xfer */
	for (queue_status = readl(master->regs + I3C_QUEUE_STATUS_LEVEL);
			(queue_status & RESP_BUFFER_LEVEL);
			queue_status = readl(master->regs + I3C_QUEUE_STATUS_LEVEL)) {
		resp = readl(master->regs + I3C_RESP_QUEUE);

		cmd = &xfer->cmds[RESP_CMDID(resp)];
		dev_dbg(master->dev, "RESP 0x%08x\n", resp);

		rx_len = min_t(unsigned int, RESP_DATA_LENGTH(resp), cmd->rx_len);

		cmd->error = RESP_ERR_STAT(resp);
		if (cmd->rx_len && !cmd->error)
			exynos_i3c_hci_master_rd_from_rx_fifo(master, cmd->rx_buf, rx_len);
	}

	for (i = 0; i < xfer->ncmds; i++) {
		unsigned int err = xfer->cmds[i].error;
		if (!err)
			continue;

		dev_err(master->dev, "CMD[%d] Error detected: 0x%08x\n", i, err);
		switch(err){
			case I3C_NACK:
				dev_err(master->dev, "Error: NOACK\n");
				ret = -EIO;
				break;
			case I3C_RX_FRAME_ERROR:
				dev_err(master->dev, "Error: I3C_HDR_DDR_RX_FRAME_ERROR\n");
				ret = -EIO;
				break;
			case I3C_RX_CRC_ERROR:
				dev_err(master->dev, "Error: I3C_HDR_DDR_RX_CRC_ERROR\n");
				ret = -EIO;
				break;
			case I3C_RX_PARITY_ERROR:
				dev_err(master->dev, "Error: I3C_HDR_RX_PARITY_ERROR\n");
				ret = -EIO;
				break;
			case I3C_ADDR_HEADER_NACK_M2:
				dev_err(master->dev, "Error: I3C_ADDR_HEADER_NOACK_M2_ERROR\n");
				ret = -EINVAL;
				break;
			case I3C_ABORTED_BY_USING_ABORT:
				dev_err(master->dev, "Error: I3C_ABORTED_BY_USING_ABORT\n");
				ret = -EIO;
				break;
			case I3C_OVERFLOW_OR_UNDERFLOW:
				dev_err(master->dev, "Error: I3C_OVERFLOW_OR_UNDERFLOW\n");
				ret = -ENOSPC;
				break;
			default:
				dev_err(master->dev, "Error: Reserved or Not defined\n");
				ret = -EINVAL;
				break;

		}
	}

	xfer->ret = ret;
	complete(&xfer->comp);

	if (ret < 0)
		exynos_i3c_hci_master_unqueue_xfer_locked(master, xfer);

	xfer = list_first_entry_or_null(&master->xfer_queue.list,
					struct exynos_i3c_hci_xfer, node);
	if (xfer)
		list_del_init(&xfer->node);

	master->xfer_queue.xfer = xfer;
	exynos_i3c_hci_master_start_xfer_locked(master);
	dev_dbg(master->dev, "End %s\n", __func__);
}

static void exynos_i3c_hci_master_queue_xfer(struct exynos_i3c_hci_master *master,
				       struct exynos_i3c_hci_xfer *xfer)
{
	unsigned long flags;

	dev_dbg(master->dev, "Start %s\n", __func__);

	init_completion(&xfer->comp);
	spin_lock_irqsave(&master->xfer_queue.lock, flags);
	if (master->xfer_queue.xfer) {
		list_add_tail(&xfer->node, &master->xfer_queue.list);
	} else {
		master->xfer_queue.xfer = xfer;
		exynos_i3c_hci_master_start_xfer_locked(master);
	}
	spin_unlock_irqrestore(&master->xfer_queue.lock, flags);

	dev_dbg(master->dev, "End %s\n", __func__);
}

static inline struct exynos_i3c_hci_master *
to_exynos_i3c_hci_master(struct i3c_master_controller *master)
{
	return container_of(master, struct exynos_i3c_hci_master, base);
}

static void exynos_i3c_hci_master_enable(struct exynos_i3c_hci_master *master)
{
	writel(readl(master->regs + I3C_HC_CONTROL) | I3C_ENABLE, master->regs + I3C_HC_CONTROL);
}

static int exynos_i3c_hci_master_disable(struct exynos_i3c_hci_master *master)
{
	u32 status;

	writel(readl(master->regs + I3C_HC_CONTROL) & ~I3C_ENABLE, master->regs + I3C_HC_CONTROL);

	return readl_poll_timeout_atomic(master->regs + I3C_PRESENT_STATE_DEBUG, status,
				  (status & I3C_CM_TFR_ST_STATUS) == 0, 10, 1000000);
}

static enum i3c_error_code exynos_i3c_hci_cmd_get_err(struct exynos_i3c_hci_cmd *cmd)
{
	switch (cmd->error) {
	case TX_DATA_MISMATCH_M1:
		return I3C_ERROR_M1;

	case ADDR_HEADER_NACK_M2:
		return I3C_ERROR_M2;

	default:
		break;
	}

	return I3C_ERROR_UNKNOWN;
}

static struct exynos_i3c_hci_xfer* exynos_i3c_hci_master_alloc_xfer(struct exynos_i3c_hci_master *master, int nxfers)
{
	struct exynos_i3c_hci_xfer *xfer;

	xfer = kzalloc(struct_size(xfer, cmds, nxfers), GFP_KERNEL);

	if (!xfer)
		return NULL;

	INIT_LIST_HEAD(&xfer->node);
	xfer->ncmds = nxfers;
	xfer->ret = -ETIMEDOUT;

	return xfer;
}

static void exynos_i3c_hci_master_free_xfer(struct exynos_i3c_hci_xfer *xfer)
{
	kfree(xfer);
}

static int exynos_i3c_hci_master_get_free_slot(struct exynos_i3c_hci_master *master)
{
	if (!(master->free_rr_slots & GENMASK(master->maxdevs - 1, 0)))
		return -ENOSPC;

	return ffs(master->free_rr_slots) - 1;
}

static int exynos_i3c_hci_master_get_addr_slot(struct exynos_i3c_hci_master *master, u8 dyn_addr)
{
	u32 rr0, rr1;
	int i;

	for (i = 0; i < master->maxdevs; i++) {
		rr0 = readl(master->regs + I3C_DEV_ADDR_TABLE(i));
		rr1 = readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(i));
		if (!(rr1 & I3C_DYNAMIC_ADDR_ASSIGNED))
			continue;

		if ((rr0 & I3C_DEVICE_TYPE) ||
		    I3C_GET_DYNAMIC_ADDRESS(rr0) != dyn_addr)
			continue;

		return i;
	}

	return -EINVAL;
}

static void exynos_i3c_hci_master_upd_i3c_scl_lim(struct exynos_i3c_hci_master *master)
{
	struct i3c_master_controller *m = &master->base;
//	unsigned long i3c_lim_period, pres_step, ncycles, ipclk_rate;
	unsigned long new_i3c_scl_lim = 0;
	struct i3c_dev_desc *dev;
//	u32 prescl1, ctrl;

	/* TODO : i3c_master_get_bus로 전환 */
	i3c_bus_for_each_i3cdev(&m->bus, dev) {
		unsigned long max_fscl;

		max_fscl = max(I3C_CCC_MAX_SDR_FSCL(dev->info.max_read_ds),
			       I3C_CCC_MAX_SDR_FSCL(dev->info.max_write_ds));
		switch (max_fscl) {
		case I3C_SDR1_FSCL_8MHZ:
			max_fscl = 8000000;
			break;
		case I3C_SDR2_FSCL_6MHZ:
			max_fscl = 6000000;
			break;
		case I3C_SDR3_FSCL_4MHZ:
			max_fscl = 4000000;
			break;
		case I3C_SDR4_FSCL_2MHZ:
			max_fscl = 2000000;
			break;
		case I3C_SDR0_FSCL_MAX:
		default:
			max_fscl = 0;
			break;
		}

		if (max_fscl &&
		    (new_i3c_scl_lim > max_fscl || !new_i3c_scl_lim))
			new_i3c_scl_lim = max_fscl;
	}

	/* Only update PRESCL_CTRL1 if the I3C SCL limitation has changed. */
	if (new_i3c_scl_lim == master->i3c_scl_lim)
		return;
	master->i3c_scl_lim = new_i3c_scl_lim;
	if (!new_i3c_scl_lim)
		return;

	/* Todo : set_timing
	 *	We only use 200MHz ipclk and sfr reset value for test
	 * */
//	ipclk_rate = clk_get_rate(master->ipclk);

	/*
	pres_step = ipclk_rate / (master->base.bus->scl_rate.i3c * 4);

	prescl1 = readl(master->regs + PRESCL_CTRL1) &
		  ~PRESCL_CTRL1_PP_LOW_MASK;
	ctrl = readl(master->regs + CTRL);

	i3c_lim_period = DIV_ROUND_UP(1000000000, master->i3c_scl_lim);
	ncycles = DIV_ROUND_UP(i3c_lim_period, pres_step);
	if (ncycles < 4)
		ncycles = 0;
	else
		ncycles -= 4;

	prescl1 |= PRESCL_CTRL1_PP_LOW(ncycles);

	if (ctrl & CTRL_DEV_EN)
		exynos_i3c_hci_master_disable(master);

	writel(prescl1, master->regs + PRESCL_CTRL1);

	if (ctrl & CTRL_DEV_EN)
		exynos_i3c_hci_master_enable(master);
		*/
}

static void exynos_i3c_hci_master_upd_i3c_addr(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	u32 addr;
	u32 dat;

	dev_dbg(master->dev, "Start %s\n", __func__);
	addr = dev->info.dyn_addr ? dev->info.dyn_addr : dev->info.static_addr;
	dat = readl(master->regs + I3C_DEV_ADDR_TABLE(data->id));
	if (dev->info.dyn_addr) {
		dat &= ~(I3C_DYNAMIC_ADDRESS_MASK | I3C_DEVICE_TYPE);
		dat |= I3C_DYNAMIC_ADDRESS(addr);
		writel(dat, master->regs + I3C_DEV_ADDR_TABLE(data->id));
	}	else {
		dat &= ~(I3C_STATIC_ADDRESS_MASK | I3C_DEVICE_TYPE);
		dat |= I3C_STATIC_ADDRESS(addr);
		writel(dat, master->regs + I3C_DEV_ADDR_TABLE(data->id));
	}
	master->addrs[data->id] = addr;
	dev_dbg(master->dev, "End %s\n", __func__);
}

static int exynos_i3c_hci_master_attach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data;
	int slot;

	dev_dbg(master->dev, "Start %s dyn_addr : 0x%02X\n", __func__, dev->info.dyn_addr);

	slot = exynos_i3c_hci_master_get_free_slot(master);
	if (slot < 0) {
		dev_err(master->dev, "%s: failed to get slot\n", __func__);
		return slot;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(master->dev, "%s: kzalloc fail\n", __func__);
		return -ENOMEM;
	}

	data->ibi = -1;
	data->id = slot;
	master->addrs[slot] = dev->info.dyn_addr ? : dev->info.static_addr;
	i3c_dev_set_master_data(dev, data);
	master->free_rr_slots &= ~BIT(slot);

	if (!dev->info.dyn_addr) {
		exynos_i3c_hci_master_upd_i3c_addr(dev);
		writel(readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(data->id))
				| I3C_DYNAMIC_ADDR_ASSIGNED,
		       master->regs + I3C_EXT_DEV_ADDR_TABLE(data->id));
	}

	dev_dbg(master->dev, "End %s\n", __func__);

	return 0;
}

static void exynos_i3c_hci_master_detach_i3c_dev(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	writel(0, master->regs + I3C_DEV_CHAR_TABLE_0(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_1(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_2(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_3(data->id));

	i3c_dev_set_master_data(dev, NULL);
	master->addrs[data->id] = 0;
	master->free_rr_slots |= BIT(data->id);
	kfree(data);
}

static int exynos_i3c_hci_master_attach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data;
	int slot;
	unsigned dat;

	dev_dbg(master->dev, "Start %s i2c addr: 0x%02X\n", __func__, dev->boardinfo->base.addr);

	slot = exynos_i3c_hci_master_get_free_slot(master);
	if (slot < 0)
		return slot;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->id = slot;
	master->addrs[slot] = dev->boardinfo->base.addr;
	master->free_rr_slots &= ~BIT(slot);
	i2c_dev_set_master_data(dev, data);

	/* exynos cannot support 10bit address of i2c normal i2c device	 */

	/* Write DAT */
	dat = I3C_DEVICE_TYPE |
		I3C_NACK_RETRY_CNT(master->nack_retry_cnt) |
		I3C_STATIC_ADDRESS(dev->boardinfo->base.addr);
	dev_dbg(master->dev, "DAT(%d) : %lx\n", slot, dat);
	writel(dat, master->regs + I3C_DEV_ADDR_TABLE(data->id));

	master->i2c_slv_present = true;

	dev_dbg(master->dev, "End %s\n", __func__);

	return 0;
}

static void exynos_i3c_hci_master_detach_i2c_dev(struct i2c_dev_desc *dev)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i2c_dev_get_master_data(dev);

	writel(0, master->regs + I3C_DEV_ADDR_TABLE(data->id));
	writel(0, master->regs + I3C_EXT_DEV_ADDR_TABLE(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_0(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_1(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_2(data->id));
	writel(0, master->regs + I3C_DEV_CHAR_TABLE_3(data->id));

	i2c_dev_set_master_data(dev, NULL);
	master->addrs[data->id] = 0;
	master->free_rr_slots |= BIT(data->id);
	kfree(data);
}

static int exynos_i3c_hci_master_do_daa(struct i3c_master_controller *m)
{
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	u32 olddevs = 0, newdevs = 0;
	int ret, slot;
	u8 last_addr = 0;
	u32 dat, exdat;

	dev_dbg(master->dev, "Start %s\n", __func__);

	for (slot = 0; slot < master->maxdevs; slot++) {
		dat = readl(master->regs + I3C_DEV_ADDR_TABLE(slot));
		exdat = readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(slot));
		/* skip i2c device */
		if (dat & I3C_DEVICE_TYPE || exdat & I3C_DYNAMIC_ADDR_ASSIGNED)
			olddevs |= BIT(slot);
	}

	master->dat_idx = master->maxdevs;

	/* Prepare RR slots before launching DAA. */
	for (slot = 0; slot < master->maxdevs; slot++) {
		if (olddevs & BIT(slot))
			continue;

		ret = i3c_master_get_free_addr(m, last_addr + 1);
		if (ret < 0) {
			dev_err(master->dev, "%s: Failed to get free\n", __func__);
			return -ENOSPC;
		}

		last_addr = ret;
		master->addrs[slot] = last_addr;
		if(master->dat_idx > slot)
			master->dat_idx = slot;
		writel(I3C_DYNAMIC_ADDRESS(last_addr) | I3C_NACK_RETRY_CNT(master->nack_retry_cnt),
		       master->regs + I3C_DEV_ADDR_TABLE(slot));
		writel(0, master->regs + I3C_DEV_CHAR_TABLE_0(slot));
		writel(0, master->regs + I3C_DEV_CHAR_TABLE_1(slot));
		writel(0, master->regs + I3C_DEV_CHAR_TABLE_2(slot));
		writel(0, master->regs + I3C_DEV_CHAR_TABLE_3(slot));
	}

	writel((master->dat_idx << 19), master->regs + I3C_DCT_SECTION_OFFSET);

	ret = i3c_master_entdaa_locked(&master->base);
	if (ret && ret != I3C_ERROR_M2) {
		dev_err(master->dev, "%s: Failed to ENTDAA CCC\n", __func__);
		return ret;
	}

	for (slot = 0; slot < master->maxdevs; slot++) {
		if (readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(slot)) & I3C_DYNAMIC_ADDR_ASSIGNED)
			newdevs |= BIT(slot);
	}
	newdevs &= ~olddevs;

	/*
	 * Clear all retaining registers filled during DAA. We already
	 * have the addressed assigned to them in the addrs array.
	 */
	for (slot = 0; slot < master->maxdevs; slot++) {
		if (newdevs & BIT(slot))
			i3c_master_add_i3c_dev_locked(m, master->addrs[slot]);
	}

	/*
	 * Clear slots that ended up not being used. Can be caused by I3C
	 * device creation failure or when the I3C device was already known
	 * by the system but with a different address (in this case the device
	 * already has a slot and does not need a new one).
	 */
	for (slot = 0; slot < master->maxdevs; slot++) {
		dat = readl(master->regs + I3C_DEV_ADDR_TABLE(slot));
		exdat = readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(slot));
		if (dat & I3C_DEVICE_TYPE)
			continue;
		if (master->free_rr_slots & BIT(slot) &&
				(exdat & I3C_DYNAMIC_ADDR_ASSIGNED)) {
			writel(0, master->regs + I3C_DEV_ADDR_TABLE(slot));
			writel(0, master->regs + I3C_EXT_DEV_ADDR_TABLE(slot));
		}
	}

	i3c_master_defslvs_locked(&master->base);

	exynos_i3c_hci_master_upd_i3c_scl_lim(master);

	/* Mask Hot-Join and Mastership request interrupts. */
	i3c_master_enec_locked(m, I3C_BROADCAST_ADDR,
			       I3C_CCC_EVENT_HJ | I3C_CCC_EVENT_MR);

	dev_dbg(master->dev, "End %s\n", __func__);

	return 0;
}

#ifdef CONFIG_OF
static int exynos_i3c_hci_parse_dt(struct exynos_i3c_hci_master *i3c, struct device *dev)
{
	u32 temp;

	/* Mode of operation High/Fast/Fast+ Speed mode */
	if (of_property_read_u32(dev->of_node, "samsung,fs-clock", &i3c->fs_clock))
		i3c->fs_clock = I3C_BUS_I2C_FM_SCL_RATE;
	if (of_property_read_u32(dev->of_node, "samsung,fs-plus-clock", &i3c->fs_plus_clock))
		i3c->fs_plus_clock = I3C_BUS_I2C_FM_PLUS_SCL_RATE;
	if (of_property_read_u32(dev->of_node, "samsung,open-drain-clock", &i3c->open_drain_clock))
		i3c->open_drain_clock = I3C_OPEN_DRAIN_CLOCK;
	if (of_property_read_u32(dev->of_node, "samsung,push-pull-clock", &i3c->push_pull_clock))
		i3c->push_pull_clock = I3C_BUS_TYP_I3C_SCL_RATE;

	if (of_get_property(dev->of_node, "samsung,disable_hold_initiating_command", NULL))
		i3c->hold_initiating_command = 0;
	else
		i3c->hold_initiating_command = 1;

	if (of_property_read_u32(dev->of_node, "samsung,nack_retry_cnt", &temp))
		i3c->nack_retry_cnt = 2;
	else
		i3c->nack_retry_cnt = temp;

	if (of_get_property(dev->of_node, "dma-mode", NULL))
		i3c->dma_mode = 1;
	else
		i3c->dma_mode = 0;

	if (of_get_property(dev->of_node, "samsung,hot_join_nack", NULL))
		i3c->hot_join_nack = 1;
	else
		i3c->hot_join_nack = 0;

	if (of_get_property(dev->of_node, "samsung,mixed-fast", NULL))
		i3c->i3c_bus_mode = I3C_MIXED_FAST_BUS;
	else if (of_get_property(dev->of_node, "samsung,mixed-slow", NULL))
		i3c->i3c_bus_mode = I3C_MIXED_SLOW_BUS;
	else
		i3c->i3c_bus_mode = I3C_PURE_BUS;

	if (of_get_property(dev->of_node, "samsung,notify_sir_rejected", NULL))
		i3c->notify_sir_rejected = 1;
	else
		i3c->notify_sir_rejected = 0;

	if (of_get_property(dev->of_node, "samsung,notify_mr_rejected", NULL))
		i3c->notify_mr_rejected = 1;
	else
		i3c->notify_mr_rejected = 0;

	if (of_get_property(dev->of_node, "samsung,notify_hj_rejected", NULL))
		i3c->notify_hj_rejected = 1;
	else
		i3c->notify_hj_rejected = 0;

	if (of_get_property(dev->of_node, "samsung,mr_req_nack", NULL))
		i3c->mr_req_nack = 1;
	else
		i3c->mr_req_nack = 0;

	if (of_get_property(dev->of_node, "samsung,sir_req_nack", NULL))
		i3c->sir_req_nack = 1;
	else
		i3c->sir_req_nack = 0;

	if (of_get_property(dev->of_node, "samsung,filter_disable_sda", NULL))
		i3c->filter_en_sda = 0;
	else
		i3c->filter_en_sda = 1;

	if (of_get_property(dev->of_node, "samsung,filter_disable_scl", NULL))
		i3c->filter_en_scl = 0;
	else
		i3c->filter_en_scl = 1;

	if (of_get_property(dev->of_node, "samsung, iba_exclude", NULL))
		i3c->iba_include = 0;
	else
		i3c->iba_include = 1;

	/* EXT_DEVICE_CTRL */
	if (of_get_property(dev->of_node, "samsung,hwacg_enable_s", NULL))
		i3c->hwacg_enable_s = 1;
	else
		i3c->hwacg_enable_s = 0;

	if (of_get_property(dev->of_node, "samsung,slave_mode", NULL))
		i3c->slave_mode = 1;
	else
		i3c->slave_mode = 0;

	if (of_get_property(dev->of_node, "samsung,data_swap", NULL))
		i3c->data_swap = 1;
	else
		i3c->data_swap = 0;

	if (of_get_property(dev->of_node, "samsung,tx_dma_free_run", NULL))
		i3c->tx_dma_free_run = 1;
	else
		i3c->tx_dma_free_run = 0;

	if (of_get_property(dev->of_node, "samsung,rx_stall_dis", NULL))
		i3c->rx_stall_en = 0;
	else
		i3c->rx_stall_en = 1;

	if (of_get_property(dev->of_node, "samsung,tx_stall_dis", NULL))
		i3c->tx_stall_en = 0;
	else
		i3c->tx_stall_en = 1;

	if (of_get_property(dev->of_node, "samsung,addr_opt_en", NULL))
		i3c->addr_opt_en = 1;
	else
		i3c->addr_opt_en = 0;

	return 0;
}
#endif

static bool exynos_i3c_hci_broadcast_ccc(const struct i3c_ccc_cmd *cmd)
{
	switch (cmd->id) {
	/* Broadcast CCC w/ extra data */
	case I3C_CCC_ENEC(true):
		fallthrough;
	case I3C_CCC_DISEC(true):
		fallthrough;
	case I3C_CCC_SETMWL(true):
		fallthrough;
	case I3C_CCC_SETMRL(true):
		fallthrough;
	case I3C_CCC_DEFSLVS:
		fallthrough;
	case I3C_CCC_ENTHDR(0):
		return true;
	default:
		break;
	}
	return false;
}

static int exynos_i3c_hci_find_first_i3c_dev(struct exynos_i3c_hci_master *master,
						int slot)
{
	int i;
	u32 reg;

	for (i = slot; i < master->maxdevs; i++) {
		reg = readl(master->regs + I3C_DEV_ADDR_TABLE(i));
		if ((reg & I3C_DEVICE_TYPE) == 0)
			return i;
	}
	return 0;
}

static int exynos_i3c_hci_send_ccc_cmd(struct i3c_master_controller *m,
					struct i3c_ccc_cmd *cmd)
{
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_xfer *xfer;
	struct exynos_i3c_hci_cmd *ccmd;
	int ret;
	int slot;

	dev_dbg(master->dev, "Start %s\n", __func__);
	dev_info(master->dev, "addr: 0x%02X CCC: 0x%02X\n", cmd->dests[0].addr, cmd->id);

	xfer = exynos_i3c_hci_master_alloc_xfer(master, 1);
	if (!xfer) {
		dev_err(master->dev, "%s: failed to alloc xfer\n", __func__);
		return -ENOMEM;
	}

	ccmd = xfer->cmds;
	ccmd->cmd = I3C_CMD_CODE(cmd->id) |
		I3C_CP |
		I3C_ROC |
		I3C_TOC;

	if (cmd->id == I3C_CCC_ENTDAA || cmd->id == I3C_CCC_SETDASA){
		if (master->dat_idx < 0 || master->dat_idx >= master->maxdevs){
			dev_err(master->dev, "Invalid DAT Index\n", master->dat_idx);
			return -EINVAL;
		}
		ccmd->cmd |= I3C_DEVICE_INDEX(master->dat_idx);
		ccmd->cmd |= I3C_DEV_COUNT(master->maxdevs - master->dat_idx);
		ccmd->cmd |= I3C_CMD_ATTR(I3C_CMD_ATTR_A);
	}

	if (cmd->id & I3C_CCC_DIRECT) {
		slot = exynos_i3c_hci_master_get_addr_slot(master, cmd->dests[0].addr);
		if (slot < 0) {
			dev_err(master->dev, "send CCC to invalid addr. ret:%d\n", slot);
			return slot;
		}
		ccmd->cmd |= I3C_DEVICE_INDEX(slot);
	}

	if (exynos_i3c_hci_broadcast_ccc(cmd)) {
		/*
		 * For a case that I2C and I3C slaves in same bus.
		 * To send Broadcast CCC w/ extra data, DEV_INDEX should be one of I3C devices.
		 * If DEV_INDEX is I2C device, master makes NOACK response.
		 */
		slot = exynos_i3c_hci_find_first_i3c_dev(master, 0);
		ccmd->cmd |= I3C_DEVICE_INDEX(slot);
	}

	if (cmd->rnw) {
		ccmd->cmd |= I3C_RNW;
		ccmd->rx_buf = cmd->dests[0].payload.data;
		ccmd->rx_len = cmd->dests[0].payload.len;
	} else {
		ccmd->tx_buf = cmd->dests[0].payload.data;
		ccmd->tx_len = cmd->dests[0].payload.len;
	}

	exynos_i3c_hci_master_queue_xfer(master, xfer);
	if (!wait_for_completion_timeout(&xfer->comp, msecs_to_jiffies(1000))) {
		dev_err(master->dev, "I3C CCC Transfer timeout\n");
		dump_i3c_register(master);
		exynos_i3c_hci_master_unqueue_xfer(master, xfer);
	}

	ret = xfer->ret;
	cmd->err = exynos_i3c_hci_cmd_get_err(&xfer->cmds[0]);
	exynos_i3c_hci_master_free_xfer(xfer);

	dev_dbg(master->dev, "End %s\n", __func__);

	return ret;
}

static bool exynos_i3c_hci_supports_ccc_cmd(struct i3c_master_controller *m,
		const struct i3c_ccc_cmd *cmd)
{

	if (cmd->ndests > 1)
		return false;

	switch (cmd->id) {
	case I3C_CCC_ENEC(true):
	case I3C_CCC_ENEC(false):
	case I3C_CCC_DISEC(true):
	case I3C_CCC_DISEC(false):
	case I3C_CCC_ENTAS(0, true):
	case I3C_CCC_ENTAS(0, false):
	case I3C_CCC_RSTDAA(true):
	case I3C_CCC_RSTDAA(false):
	case I3C_CCC_ENTDAA:
	case I3C_CCC_SETMWL(true):
	case I3C_CCC_SETMWL(false):
	case I3C_CCC_SETMRL(true):
	case I3C_CCC_SETMRL(false):
	case I3C_CCC_DEFSLVS:
	case I3C_CCC_ENTHDR(0):
	case I3C_CCC_SETDASA:
	case I3C_CCC_SETNEWDA:
	case I3C_CCC_GETMWL:
	case I3C_CCC_GETMRL:
	case I3C_CCC_GETPID:
	case I3C_CCC_GETBCR:
	case I3C_CCC_GETDCR:
	case I3C_CCC_GETSTATUS:
	case I3C_CCC_GETACCMST:
	case I3C_CCC_GETMXDS:
	case I3C_CCC_GETHDRCAP:
		return true;
	default:
		break;
	}

	return false;
}

static int exynos_i3c_hci_xfer_priv(struct i3c_dev_desc *dev,
		struct i3c_priv_xfer *xfers,
		int nxfers)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	int i = 0;
	int ret = 0;
	int txslots = 0, rxslots = 0;
	struct exynos_i3c_hci_xfer *xfer;
	int slot;

	dev_dbg(master->dev, "Start %s\n", __func__);
	dev_dbg(master->dev, "addr : 0x%02X\n", dev->info.dyn_addr);

	if (!nxfers)
		return 0;

	if (nxfers > master->caps.cmdfifodepth)
		return -ENOTSUPP;

	/* max lenth config */
	for (i = 0; i < nxfers; i++) {
		if (xfers[i].len > I3C_CMD_MAX_LEN) {
			dev_err(master->dev, "Xfer size %u is more than MAX_LEN %u\n",
					xfers[i].len, I3C_CMD_MAX_LEN);
			return -ENOTSUPP;
		}
	}

	for (i = 0; i < nxfers; i++) {
		if (xfers[i].rnw)
			rxslots += DIV_ROUND_UP(xfers[i].len, 4);
		else
			txslots += DIV_ROUND_UP(xfers[i].len, 4);
	}
	if (rxslots > master->caps.rxfifodepth ||
	    txslots > master->caps.txfifodepth)
		return -ENOTSUPP;

	xfer = exynos_i3c_hci_master_alloc_xfer(master, nxfers);
	if (!xfer) {
		dev_err(master->dev, "%s: failed to alloc xfer\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < nxfers; i++) {
		struct exynos_i3c_hci_cmd  *ccmd = &xfer->cmds[i];
		u32 pl_len = xfers[i].len;

		slot = exynos_i3c_hci_master_get_addr_slot(master, dev->info.dyn_addr);
		ccmd->cmd |= I3C_DEVICE_INDEX(slot);
		ccmd->cmd |= I3C_BYTE_CNT(pl_len);

		if (xfers[i].rnw) {
			ccmd->cmd |= I3C_RNW;
			ccmd->rx_buf = xfers[i].data.in;
			ccmd->rx_len = xfers[i].len;
		} else {
			ccmd->tx_buf = xfers[i].data.out;
			ccmd->tx_len = xfers[i].len;
		}

		ccmd->cmd |= I3C_ROC | I3C_CMD_TID(i);
		if (i == (nxfers - 1))
			ccmd->cmd |= I3C_TOC;
	}

	exynos_i3c_hci_master_queue_xfer(master, xfer);
	if (!wait_for_completion_timeout(&xfer->comp, msecs_to_jiffies(1000))) {
		dev_err(master->dev, "I3C priv_xfer Transfer timeout\n");
		dump_i3c_register(master);
		exynos_i3c_hci_master_unqueue_xfer(master, xfer);
	}

	ret = xfer->ret;

	for (i = 0; i < nxfers; i++)
		xfers[i].err = exynos_i3c_hci_cmd_get_err(&xfer->cmds[i]);
	exynos_i3c_hci_master_free_xfer(xfer);

	dev_dbg(master->dev, "End %s\n", __func__);

	return ret;
}

static int exynos_i3c_hci_master_i2c_xfers(struct i2c_dev_desc *dev,
				     const struct i2c_msg *xfers, int nxfers)
{
	struct i3c_master_controller *m = i2c_dev_get_master(dev);
	struct exynos_i3c_hci_i2c_dev_data *data = i2c_dev_get_master_data(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	unsigned int nrxwords = 0, ntxwords = 0;
	struct exynos_i3c_hci_xfer *xfer;
	int i, ret = 0;
	int slot;

	dev_dbg(master->dev, "Start %s\n", __func__);
	dev_dbg(master->dev, "addr : 0x%02X\n", xfers[0].addr);

	if (!nxfers)
		return 0;

	if (nxfers > master->caps.cmdfifodepth)
		return -ENOTSUPP;

	/* max length config */
	for (i = 0; i < nxfers; i++) {
		if (xfers[i].len > I3C_CMD_MAX_LEN)
			return -ENOTSUPP;
	}

	for (i = 0; i < nxfers; i++) {
		if (xfers[i].flags & I2C_M_RD)
			nrxwords += DIV_ROUND_UP(xfers[i].len, 4);
		else
			ntxwords += DIV_ROUND_UP(xfers[i].len, 4);
	}

	if (ntxwords > master->caps.txfifodepth ||
	    nrxwords > master->caps.rxfifodepth)
		return -ENOTSUPP;

	xfer = exynos_i3c_hci_master_alloc_xfer(master, nxfers);
	if (!xfer)
		return -ENOMEM;

	for (i = 0; i < nxfers; i++) {
		struct exynos_i3c_hci_cmd *ccmd = &xfer->cmds[i];

		slot = data->id;
		ccmd->cmd |= I3C_DEVICE_INDEX(slot);

		if (xfers[i].flags & I2C_M_RD) {
			ccmd->cmd |= I3C_RNW;
			ccmd->rx_buf = xfers[i].buf;
			ccmd->rx_len = xfers[i].len;
		} else {
			ccmd->tx_buf = xfers[i].buf;
			ccmd->tx_len = xfers[i].len;
		}
		ccmd->cmd |= I3C_CMD_TID(i) | I3C_ROC;
		if (i == (nxfers - 1))
			ccmd->cmd |= I3C_TOC;

		dev_dbg(master->dev, "CMD %d 0x%x\n", i, ccmd->cmd);
	}

	/* Exclude 0x7e for i2c private transfers */
	writel(readl(master->regs + I3C_HC_CONTROL) & ~I3C_IBA_INCLUDE, master->regs+I3C_HC_CONTROL);
	exynos_i3c_hci_master_queue_xfer(master, xfer);
	if (!wait_for_completion_timeout(&xfer->comp, msecs_to_jiffies(1000))) {
		dev_err(master->dev, "I3C i2c xfer Transfer timeout\n");
		dump_i3c_register(master);
		exynos_i3c_hci_master_unqueue_xfer(master, xfer);
	}

	ret = xfer->ret;
	exynos_i3c_hci_master_free_xfer(xfer);
	writel(readl(master->regs + I3C_HC_CONTROL) | I3C_IBA_INCLUDE, master->regs+I3C_HC_CONTROL);

	dev_dbg(master->dev, "End %s\n", __func__);

	return ret;
}

static int exynos_i3c_hci_master_reattach_i3c_dev(struct i3c_dev_desc *dev,
					    u8 old_dyn_addr)
{
	exynos_i3c_hci_master_upd_i3c_addr(dev);
	return 0;
}

static int exynos_i3c_hci_master_clk_cfg(struct exynos_i3c_hci_master *master)
{
	unsigned long ipclk_rate;
	//u32 tbuf;
	int ret;
	__u32 fs_clock_div_quat;
	__u32 fs_plus_clock_div_quat;
	__u32 open_drain_clock_div_quat;
	__u32 push_pull_clock_div_quat;

	ipclk_rate = clk_get_rate(master->ipclk);
	if (!ipclk_rate) {
		dev_err(master->dev, "Failed to get clock %d\n");
		return -EINVAL;
	}

	if (ipclk_rate != DEFAULT_CLK) {
		ret = clk_set_rate(master->ipclk, DEFAULT_CLK);
		if (ret < 0) {
			dev_err(master->dev, "Failed to set clock %d\n", DEFAULT_CLK);
			return -EINVAL;
		}
		ipclk_rate = clk_get_rate(master->ipclk);
	}

	dev_info(master->dev, "ipclk: %luHz SCL_QUAR_PER 0x%x\n",
			ipclk_rate, readl(master->regs + I3C_SCL_QUARTER_PERIOD));

	/* Tbuf reset value 0x09 is too short to make STOP completely */
	//tbuf = readl(master->regs + I3C_TBUF);
	//writel((tbuf & ~0xff) | 0x30, master->regs + I3C_TBUF);

	/* Todo : setting timing parameters */
	/* But We have no calculating equiation
	 * We only use default reset value for 200MHz ipclk and 400KHz FM, 1MHz FM+ mode */
		fs_clock_div_quat = ipclk_rate / master->fs_clock / 4;
		fs_plus_clock_div_quat = ipclk_rate / master->fs_plus_clock / 4;
		open_drain_clock_div_quat = ipclk_rate / master->open_drain_clock / 4;
		push_pull_clock_div_quat = ipclk_rate / master->push_pull_clock / 4;

		writel((fs_clock_div_quat & 0xff) << 24 | (fs_plus_clock_div_quat & 0xff) << 16 |
				(open_drain_clock_div_quat & 0xff) << 8 | (push_pull_clock_div_quat & 0xff) << 0,
				master->regs + I3C_SCL_QUARTER_PERIOD);

		/* Todo : setting PP Low, TSU_STA, TCBP, TLOW, THD_STA, TBUF, BUS_AVAIL
		 * I3C_PP_HCNT = 2 * PP_CLK - 1
		 * writel((push_pull_clock_div_quat * 2 - 1) & 0xff, i3c->regs + I3C_SCL_CTRL);
		 * TSU_STA = 600ns in 400KHz, 260ns in 1MHz, tCBSr = 1/4T?
		 * TCBP_MIXED = tSU_STA in FM, TCBP_PURE = 1/2*tCBSr
		 * TLOW = 1300ns in 400KHz, 500ns in 1MHz, 200ns in OD
		 * THD = 600ns in 400KHz, 260ns in 1MHz, 38.4n in OD
		 * TBUF = 1300ns in 400Khz, 45ns in OD
		 * tAVAL = 1000ns in OD, tIDLE = 1ms in OD
		 *
		 */

	return 0;
}

static void exynos_i3c_hci_master_config(struct exynos_i3c_hci_master *m)
{
	u32 ctrl = readl(m->regs + I3C_HC_CONTROL);
	/* clear optional bits */
	ctrl &= (I3C_ENABLE | I3C_ABORT | I3C_HOT_JOIN_CTRL);

	if (m->i2c_slv_present)
		ctrl |= I3C_I2C_SLV_PRESENT;
	if (m->iba_include)
		ctrl |= I3C_IBA_INCLUDE;

	writel(ctrl, m->regs + I3C_HC_CONTROL);
}

static void exynos_i3c_hci_master_extconfig(struct exynos_i3c_hci_master *m)
{
	u32 ctrl = readl(m->regs + I3C_EXT_DEVICE_CTRL);
	/* clear optional bits */
	ctrl &= (I3C_VGPIO_TX_REQ_FORCE | I3C_IBI_REQ | I3C_HJ_REQ | I3C_MASTERSHIP_REQ);

	if (!m->hwacg_enable_s)
		ctrl |= I3C_HWACG_DISABLE_S;
	if (m->notify_vgpio_rx)
		ctrl |= I3C_NOTIFY_VGPIO_RX;
	if (m->vgpio_en)
		ctrl |= I3C_VGPIO_ENABLE;
	if (m->slave_mode)
		ctrl |= I3C_SLAVE_MODE;
	if (m->data_swap)
		ctrl |= I3C_DATA_SWAP;
	if (m->getmxds_5byte)
		ctrl |= I3C_GETMXDS_5BYTE;
	if (m->tx_dma_free_run)
		ctrl |= I3C_TX_DMA_FREE_RUN;
	if (m->rx_stall_en)
		ctrl |= I3C_RX_STALL_EN;
	if (m->tx_stall_en)
		ctrl |= I3C_TX_STALL_EN;
	if (m->addr_opt_en)
		ctrl |= I3C_ADDR_OPT_EN;
	if (m->i3c_bus_mode == I3C_MIXED_SLOW_BUS)
		ctrl |= I3C_SLOW_BUS;

	writel(ctrl, m->regs + I3C_EXT_DEVICE_CTRL);

}

static int exynos_i3c_hci_master_bus_init(struct i3c_master_controller *m)
{
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct i3c_device_info info = { };
	u32 ctrl;
	int ret;
	unsigned int char_reg0, char_reg1;
//	unsigned int queue_thld, buffer_thld;

	dev_dbg(master->dev, "Start %s\n", __func__);

	/* Setting I3C_DEVICE_CTRL
	 * For now, we set sfr reset value */

	switch (m->bus.mode) {
	case I3C_BUS_MODE_PURE:
		ctrl = I3C_PURE_BUS;
		break;

	case I3C_BUS_MODE_MIXED_FAST:
		ctrl = I3C_MIXED_FAST_BUS;
		break;

	case I3C_BUS_MODE_MIXED_SLOW:
		ctrl = I3C_MIXED_SLOW_BUS;
		break;

	default:
		dev_err(master->dev, "%s: Not configured bus mode\n", __func__);
		return -EINVAL;
	}

	exynos_i3c_hci_master_extconfig(master);

	/* Set Timing */
	exynos_i3c_hci_master_clk_cfg(master);

	writel(I3C_EXT_INTR_ALL, master->regs + I3C_EXT_PIO_INTR_STATUS);
	writel(I3C_INTR_ALL, master->regs + I3C_PIO_INTR_STATUS);
	writel(0, master->regs + I3C_PIO_INTR_EN);
	writel(0, master->regs + I3C_EXT_PIO_INTR_EN);
	writel(0, master->regs + I3C_PIO_INTR_SIGNAL_EN);
	writel(0, master->regs + I3C_EXT_PIO_INTR_SIGNAL_EN);

	/* Setting I3C_DEVICE_ADDR
	 * Get an address for the master. */
	ret = i3c_master_get_free_addr(m, 0);
	if (ret < 0) {
		dev_err(master->dev, "%s: Failed to get free addr for MST\n", __func__);
		return ret;
	}

	writel(I3C_DYNAMIC_ADDR_VALID | I3C_DYNAMIC_ADDR(ret),
	       master->regs + I3C_MASTER_DEVICE_ADDR);

	info.dyn_addr = ret;
	char_reg0 = readl(master->regs + I3C_CHAR_REG0);
	char_reg1 = readl(master->regs + I3C_CHAR_REG1);
	info.dcr = char_reg0 & 0xff;
	info.bcr = (char_reg0 >> 8) & 0xff;
	info.pid = (char_reg0 >> 16) & 0xffff;
	info.pid |= (u64)char_reg0 << 16;


	if (info.bcr & I3C_BCR_HDR_CAP)
		info.hdr_cap = I3C_CCC_HDR_MODE(I3C_HDR_DDR);

	ret = i3c_master_set_info(&master->base, &info);
	if (ret)
		return ret;

	/* S/W Reset cannot be called to prevent clearing DAT */
	writel(0x3e, master->regs + I3C_RESET_CTRL);
	writel(0x120, master->regs + I3C_EXT_RESET_CTRL);

	/*
	 * Enable Hot-Join, and, when a Hot-Join request happens, disable all
	 * events coming from this device.
	 *
	 * We will issue ENTDAA afterwards from the threaded IRQ handler.
	 */

	exynos_i3c_hci_master_config(master);

	exynos_i3c_hci_master_enable(master);

	dev_dbg(master->dev, "End %s\n", __func__);

	return 0;
}

static void exynos_i3c_hci_store_dat(struct exynos_i3c_hci_master *master)
{
	int i;
	for(i = 0; i < MAX_DEVS ; i++){
		master->DAT[i] = readl(master->regs + I3C_DEV_ADDR_TABLE(i));
		master->EXT_DAT[i] = readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(i));
		master->DCT[i][0] = readl(master->regs + I3C_DEV_CHAR_TABLE_0(i));
		master->DCT[i][1] = readl(master->regs + I3C_DEV_CHAR_TABLE_1(i));
		master->DCT[i][2] = readl(master->regs + I3C_DEV_CHAR_TABLE_2(i));
		master->DCT[i][3] = readl(master->regs + I3C_DEV_CHAR_TABLE_3(i));
	}
}

static void exynos_i3c_hci_load_dat(struct exynos_i3c_hci_master *master)
{
	int i;
	for(i = 0; i < MAX_DEVS ; i++){
		writel(master->DAT[i], master->regs + I3C_DEV_ADDR_TABLE(i));
		writel(master->EXT_DAT[i], master->regs + I3C_EXT_DEV_ADDR_TABLE(i));
		writel(master->DCT[i][0], master->regs + I3C_DEV_CHAR_TABLE_0(i));
		writel(master->DCT[i][1], master->regs + I3C_DEV_CHAR_TABLE_1(i));
		writel(master->DCT[i][2], master->regs + I3C_DEV_CHAR_TABLE_2(i));
		writel(master->DCT[i][3],master->regs + I3C_DEV_CHAR_TABLE_3(i));
	}
}

static int exynos_i3c_hci_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "runtime suspending\n", __func__);
	/*
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_i3c_hci_master *master = platform_get_drvdata(pdev);

	clk_disable(master->ipclk);
	clk_disable(master->pclk);
	master->runtime_resumed = 0;
	*/

	return 0;
}

static int exynos_i3c_hci_runtime_resume(struct device *dev)
{
	/*
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_i3c_hci_master *master = platform_get_drvdata(pdev);
	int ret = 0;

	clk_enable(master->ipclk);
	if (ret) {
		return ret;
	}
	clk_enable(master->pclk);
	if (ret) {
		return ret;
	}

	master->runtime_resumed = 1;
*/
	dev_dbg(dev, "runtime resumed\n", __func__);
	return 0;
}

static int exynos_i3c_hci_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_i3c_hci_master *master = platform_get_drvdata(pdev);

	dev_dbg(master->dev, "suspend_noirq\n", __func__);

	master->current_master = readl(master->regs + I3C_MASTER_DEVICE_ADDR);
	exynos_i3c_hci_store_dat(master);
	/* S/W reset while entering suspend */
	writel(I3C_SOFT_RST, master->regs + I3C_RESET_CTRL);
	exynos_i3c_hci_master_disable(master);

	clk_disable(master->ipclk);
	clk_disable(master->gate_clk);
	master->suspended = 1;

	return 0;
}

static int exynos_i3c_hci_resume_noirq(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_i3c_hci_master *master = platform_get_drvdata(pdev);

	dev_dbg(master->dev, "resume_noirq\n", __func__);

	ret = clk_enable(master->ipclk);
	if (ret) {
		dev_err(master->dev, "ipclk enable fail\n");
		return ret;
	}
	ret = clk_enable(master->gate_clk);
	if (ret) {
		dev_err(master->dev, "gate_clk enable fail\n");
		clk_disable(master->ipclk);
		return ret;
	}
	writel(master->current_master, master->regs + I3C_MASTER_DEVICE_ADDR);
	exynos_i3c_hci_load_dat(master);

	exynos_i3c_hci_master_extconfig(master);

	/* Set Timing */
	exynos_i3c_hci_master_clk_cfg(master);

	exynos_i3c_hci_master_config(master);

	exynos_i3c_hci_master_enable(master);
	master->suspended = 0;

	return 0;
}

static void exynos_i3c_hci_master_bus_cleanup(struct i3c_master_controller *m)
{
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);

	exynos_i3c_hci_master_disable(master);
}

static void exynos_i3c_hci_master_handle_ibi(struct exynos_i3c_hci_master *master,
				       unsigned int ibiq)
{
	struct exynos_i3c_hci_i2c_dev_data *data;
	bool data_consumed = false;
	struct i3c_ibi_slot *slot;
	u32 addr = I3C_IBI_ADDR(ibiq);
	struct i3c_dev_desc *dev;
	size_t nbytes;
	u8 *buf;
	unsigned int i;

	/*
	 * FIXME: maybe we should report the FIFO OVF errors to the upper
	 * layer.
	 * Todo: dynamic addr to ibi slot number
	 */
	for (i = 0; i < master->ibi.num_slots; i++) {
		dev = master->ibi.slots[i];
		if (dev && dev->info.dyn_addr == addr)
			break;
	}
	if (i == master->ibi.num_slots) {
		dev_err(master->dev, "Invalid ibi slot\n");
		return;
	}

	if (!dev) {
		dev_err(master->dev, "ibi slot(%x) not found\n", addr);
		return;
	}

	spin_lock(&master->ibi.lock);

	data = i3c_dev_get_master_data(dev);
	slot = i3c_generic_ibi_get_free_slot(data->ibi_pool);
	if (!slot)
		goto out_unlock;

	buf = slot->data;

	nbytes = I3C_DATA_LENGTH(ibiq);
	for (i = 0;  i < nbytes; i++) {
		buf[i] = readl(master->regs + I3C_IBI_PORT);
	}

	slot->len = min_t(unsigned int, I3C_DATA_LENGTH(ibiq),
			  dev->ibi->max_payload_len);
	i3c_master_queue_ibi(dev, slot);
	data_consumed = true;

out_unlock:
	spin_unlock(&master->ibi.lock);

	/* Consume data from the FIFO if it's not been done already. */
	if (!data_consumed) {
		for (i = 0; i < I3C_DATA_LENGTH(ibiq); i++)
			readl(master->regs + I3C_IBI_PORT);
	}
}

static void exynos_i3c_hci_master_demux_ibis(struct exynos_i3c_hci_master *master)
{
	unsigned int queue_status;
	unsigned int ibiq;

	for (queue_status = readl(master->regs + I3C_QUEUE_STATUS_LEVEL);
			(queue_status & I3C_IBI_STAT_CNT_LEVEL);
			queue_status = readl(master->regs + I3C_QUEUE_STATUS_LEVEL)) {
		ibiq = readl(master->regs + I3C_IBI_PORT);
		switch(I3C_IBI_ID(ibiq)) {
			case 0x2:
				/* HJ Case */
				queue_work(master->base.wq, &master->hj_work);
				break;

			default:
				exynos_i3c_hci_master_handle_ibi(master, ibiq);
				break;
		}
	}
}

static irqreturn_t exynos_i3c_hci_master_interrupt(int irq, void *data)
{
	struct exynos_i3c_hci_master *master = data;
	u32 status;

	dev_dbg(master->dev, "Start %s\n", __func__);

	status = readl(master->regs + I3C_PIO_INTR_STATUS);

	if (!(status & readl(master->regs + I3C_PIO_INTR_EN))) {
		writel(status, master->regs + I3C_PIO_INTR_STATUS);
		return IRQ_NONE;
	}

	spin_lock(&master->xfer_queue.lock);
	dev_dbg(master->dev, "INT STATUS 0x%08x\n", status);
	if (status & I3C_TRANSFER_ERR_STAT)
		dev_err(master->dev, "INT STATUS 0x%08x Transfer ERR Interrupt detected\n", status);
	if (status & I3C_TRANSFER_ABORT_STAT)
		dev_err(master->dev, "INT STATUS 0x%08x Transfer ABORT Interrupt detected\n", status);
	if (status & I3C_RESP_READY_STAT)
		exynos_i3c_hci_master_end_xfer_locked(master, status);
	else if (!(status & I3C_IBI_THLD_STAT))
		dev_err(master->dev, "INT STATUS 0x%08x Wrong with transaction\n", status);

	/* Pending clear */
	writel(status & ~I3C_IBI_THLD_STAT, master->regs + I3C_PIO_INTR_STATUS);

	spin_unlock(&master->xfer_queue.lock);

	if (status & I3C_IBI_THLD_STAT) {
		exynos_i3c_hci_master_demux_ibis(master);
		writel(I3C_IBI_THLD_STAT, master->regs + I3C_PIO_INTR_STATUS);
	}

	dev_dbg(master->dev, "End %s\n", __func__);

	return IRQ_HANDLED;
}

static int exynos_i3c_hci_master_disable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	int ret;

	ret = i3c_master_disec_locked(m, dev->info.dyn_addr,
				      I3C_CCC_EVENT_SIR);
	if (ret)
		dev_err(master->dev, "Fail to DISEC CCC while disable_ibi, ret = %d\n", ret);

	return ret;
}

static int exynos_i3c_hci_master_enable_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;
//	u32 sircfg, sirmap;
	unsigned int dat, exdat;
	int ret;

	spin_lock_irqsave(&master->ibi.lock, flags);
	/* CDNS SIR_MAP contains DEV_ROLE(slv or mst), DEV_DA, DEV_PL, ACK and DEV_SLOW.
	 * Exynos uses DAT as IBI MAP register. DAT includes DYNAMIC_ADDRESS, IBI_PAYLOAD_SIZE, SIR_REG_NACK bits.
	 */
	dat = readl(master->regs + I3C_DEV_ADDR_TABLE(data->id));
	exdat = readl(master->regs + I3C_EXT_DEV_ADDR_TABLE(data->id));
	if (!(exdat & I3C_DYNAMIC_ADDR_ASSIGNED)) {
		dev_err(master->dev, "Enable_ibi fails: DYNAMIC ADDR wasn't assigned\n");
		return -ENODEV;
	} else if (dat & I3C_DEVICE_TYPE) {
		dev_err(master->dev, "Enable_ibi fails: dev is I2C slave\n");
		return -ENODEV;
	};

	exdat |= I3C_IBI_PAYLOAD_SIZE(dev->info.max_ibi_len);
	dat |= I3C_IBI_PAYLOAD;
	dat &= ~I3C_SIR_REJECT;
	writel(dat, master->regs + I3C_DEV_ADDR_TABLE(data->id));
	writel(exdat, master->regs + I3C_EXT_DEV_ADDR_TABLE(data->id));

	spin_unlock_irqrestore(&master->ibi.lock, flags);

	ret = i3c_master_enec_locked(m, dev->info.dyn_addr,
				     I3C_CCC_EVENT_SIR);
	if (ret) {
		dev_err(master->dev, "Fail to ENEC CCC while enable_ibi, ret = %d\n", ret);
	}

	return ret;
}

static int exynos_i3c_hci_master_request_ibi(struct i3c_dev_desc *dev,
				       const struct i3c_ibi_setup *req)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;
	unsigned int i;

	data->ibi_pool = i3c_generic_ibi_alloc_pool(dev, req);
	if (IS_ERR(data->ibi_pool))
		return PTR_ERR(data->ibi_pool);

	spin_lock_irqsave(&master->ibi.lock, flags);
	for (i = 0; i < master->ibi.num_slots; i++) {
		if (!master->ibi.slots[i]) {
			data->ibi = i;
			master->ibi.slots[i] = dev;
			break;
		}
	}
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	if (i < master->ibi.num_slots)
		return 0;

	i3c_generic_ibi_free_pool(data->ibi_pool);
	data->ibi_pool = NULL;

	return -ENOSPC;
}

static void exynos_i3c_hci_master_free_ibi(struct i3c_dev_desc *dev)
{
	struct i3c_master_controller *m = i3c_dev_get_master(dev);
	struct exynos_i3c_hci_master *master = to_exynos_i3c_hci_master(m);
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);
	unsigned long flags;

	spin_lock_irqsave(&master->ibi.lock, flags);
	master->ibi.slots[data->ibi] = NULL;
	data->ibi = -1;
	spin_unlock_irqrestore(&master->ibi.lock, flags);

	i3c_generic_ibi_free_pool(data->ibi_pool);
}

static void exynos_i3c_hci_master_recycle_ibi_slot(struct i3c_dev_desc *dev,
					     struct i3c_ibi_slot *slot)
{
	struct exynos_i3c_hci_i2c_dev_data *data = i3c_dev_get_master_data(dev);

	i3c_generic_ibi_recycle_slot(data->ibi_pool, slot);
}


static const struct i3c_master_controller_ops exynos_i3c_hci_master_ops = {
	.bus_init = exynos_i3c_hci_master_bus_init,
	.bus_cleanup = exynos_i3c_hci_master_bus_cleanup,
	.priv_xfers			= exynos_i3c_hci_xfer_priv,
	.i2c_xfers = exynos_i3c_hci_master_i2c_xfers,
	.attach_i3c_dev = exynos_i3c_hci_master_attach_i3c_dev,
	.reattach_i3c_dev = exynos_i3c_hci_master_reattach_i3c_dev,
	.detach_i3c_dev = exynos_i3c_hci_master_detach_i3c_dev,
	.attach_i2c_dev = exynos_i3c_hci_master_attach_i2c_dev,
	.detach_i2c_dev = exynos_i3c_hci_master_detach_i2c_dev,
	.do_daa = exynos_i3c_hci_master_do_daa,
	.send_ccc_cmd		= exynos_i3c_hci_send_ccc_cmd,
	.supports_ccc_cmd		= exynos_i3c_hci_supports_ccc_cmd,
	.enable_ibi = exynos_i3c_hci_master_enable_ibi,
	.disable_ibi = exynos_i3c_hci_master_disable_ibi,
	.request_ibi = exynos_i3c_hci_master_request_ibi,
	.free_ibi = exynos_i3c_hci_master_free_ibi,
	.recycle_ibi_slot = exynos_i3c_hci_master_recycle_ibi_slot,
};

static void exynos_i3c_hci_master_hj(struct work_struct *work)
{
	struct exynos_i3c_hci_master *master = container_of(work,
						      struct exynos_i3c_hci_master,
						      hj_work);

	i3c_master_do_daa(&master->base);
}

static int exynos_i3c_hci_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct exynos_i3c_hci_master *master;
	struct resource *mem;
	int ret, irq;

	dev_info(&pdev->dev, "Entering I3C probe\n");

	if (!np) {
		dev_err(&pdev->dev, "No device node\n");
		return -ENOENT;
	}

	master = devm_kzalloc(&pdev->dev, sizeof(struct exynos_i3c_hci_master), GFP_KERNEL);
	if (!master) {
		dev_err(&pdev->dev, "No memory for I3C device\n");
		return -ENOMEM;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	master->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(master->regs)) {
		dev_err(&pdev->dev, "faile to ioremap\n");
		return PTR_ERR(master->regs);
	}

	master->ipclk = devm_clk_get(&pdev->dev, "ipclk");
	if (IS_ERR(master->ipclk)) {
		dev_err(&pdev->dev, "cannot get ipclk(sysclk)\n");
		return PTR_ERR(master->ipclk);
	}

	master->gate_clk = devm_clk_get(&pdev->dev, "gate_clk");
	if (IS_ERR(master->gate_clk)) {
		dev_err(&pdev->dev, "cannot get gateclk(pclk)\n");
		return PTR_ERR(master->gate_clk);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "cannot get I3C IRQ\n");
		return irq;
	}

	ret = clk_prepare_enable(master->gate_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to gateclk enable\n");
		return ret;
	}

	ret = clk_prepare_enable(master->ipclk);
	if (ret) {
		dev_err(&pdev->dev, "failed to ipclk enable\n");
		goto err_enable_ipclk;
	}

	if(pdev->dev.of_node)
		exynos_i3c_hci_parse_dt(master, &pdev->dev);

	spin_lock_init(&master->xfer_queue.lock);
	INIT_LIST_HEAD(&master->xfer_queue.list);

	INIT_WORK(&master->hj_work, exynos_i3c_hci_master_hj);

	ret = devm_request_irq(&pdev->dev, irq, exynos_i3c_hci_master_interrupt, 0,
			 dev_name(&pdev->dev), master);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto err_disable_pclk;
	}

	/* TOTO : load from DAT entries */
	master->maxdevs = 8;
	master->free_rr_slots = GENMASK(master->maxdevs, 0);

	platform_set_drvdata(pdev, master);
	master->dev = &pdev->dev;

	/* TODO : IBI work */
	spin_lock_init(&master->ibi.lock);
	master->ibi.num_slots = IBI_STAT_QUEUE_DEPTH;
	master->ibi.slots = devm_kcalloc(&pdev->dev, master->ibi.num_slots,
					 sizeof(*master->ibi.slots),
					 GFP_KERNEL);
	if (!master->ibi.slots)
		goto err_disable_pclk;

	master->caps.cmdfifodepth = CMD_QUEUE_DEPTH;
	master->caps.respfifodepth = RESP_STAT_QUEUE_DEPTH;
	master->caps.rxfifodepth = RX_FIFO_DEPTH;
	master->caps.txfifodepth = TX_FIFO_DEPTH;
	master->caps.ibiqfifodepth = IBI_STAT_QUEUE_DEPTH;
	master->caps.ibidfifodepth = IBI_DATA_BUFFER_DEPTH;

//	writel(IBIR_THR(1), master->regs + CMD_IBI_THR_CTRL);
	writel(I3C_IBI_THLD_STAT_EN, master->regs + I3C_PIO_INTR_EN);
	writel(I3C_IBI_THLD_SIG_EN, master->regs + I3C_PIO_INTR_SIGNAL_EN);

	ret = i3c_master_register(&master->base, &pdev->dev,
					&exynos_i3c_hci_master_ops, false);
	if (ret) {
		dev_err(&pdev->dev, "failed to i3c master register ret(%d)\n", ret);
		goto err_disable_pclk;
	}

	dev_info(&pdev->dev, "I3C probe success\n");

	return 0;

err_disable_pclk:
	clk_disable_unprepare(master->ipclk);

err_enable_ipclk:
	clk_disable_unprepare(master->gate_clk);

	return ret;
}

static int exynos_i3c_hci_remove(struct platform_device *pdev)
{
	struct exynos_i3c_hci_master *master = platform_get_drvdata(pdev);
	int ret;

	ret = i3c_master_unregister(&master->base);
	if (ret)
		return ret;

	clk_disable_unprepare(master->ipclk);
	clk_disable_unprepare(master->gate_clk);

	return 0;
}


static const struct dev_pm_ops exynos_i3c_hci_pm = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(exynos_i3c_hci_suspend_noirq,
				      exynos_i3c_hci_resume_noirq)
	SET_RUNTIME_PM_OPS(exynos_i3c_hci_runtime_suspend,
			   exynos_i3c_hci_runtime_resume, NULL)
};

static const struct of_device_id exynos_i3c_hci_match[] = {
	{ .compatible = "samsung,exynos-i3c-hci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_i3c_hci_match);

static struct platform_driver exynos_i3c_hci_driver = {
	.probe		= exynos_i3c_hci_probe,
	.remove		= exynos_i3c_hci_remove,
	.driver		= {
		.name	= "exynos-i3c-hci",
		.pm	= &exynos_i3c_hci_pm,
		.of_match_table = of_match_ptr(exynos_i3c_hci_match),
	},
};
module_platform_driver(exynos_i3c_hci_driver);

MODULE_AUTHOR("Kang Kyungwoo <kwoo.kang@samsung.com> \
		Cha Myungsu <myung-su.cha@samsung.com>");
MODULE_DESCRIPTION("Exynos I3C HCI master driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-i3c-hci-master");
