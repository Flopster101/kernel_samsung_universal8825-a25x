// SPDX-License-Identifier: GPL-2.0-only
/*
 * dpu30/cal_9925/dsimfc_regs.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register access functions for Samsung EXYNOS Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/iopoll.h>

#include <exynos_drm_dsimfc.h>
#include <cal_config.h>
#include <dsimfc_cal.h>
#include <regs-dsimfc.h>

static struct cal_regs_desc regs_dsimfc[REGS_DSIMFC_ID_MAX];

#define dsimfc_regs_desc(id)				\
	(&regs_dsimfc[id])
#define dsimfc_read(id, offset)				\
	cal_read(dsimfc_regs_desc(id), offset)
#define dsimfc_write(id, offset, val)			\
	cal_write(dsimfc_regs_desc(id), offset, val)
#define dsimfc_read_mask(id, offset, mask)		\
	cal_read_mask(dsimfc_regs_desc(id), offset, mask)
#define dsimfc_write_mask(id, offset, val, mask)		\
	cal_write_mask(dsimfc_regs_desc(id), offset, val, mask)

void dsimfc_regs_desc_init(void __iomem *regs, const char *name,
		unsigned int id)
{
	regs_dsimfc[id].regs = regs;
	regs_dsimfc[id].name = name;
}


/****************** DMA DSIMFC CAL functions ******************/
static void dsimfc_reg_set_sw_reset(u32 id)
{
	dsimfc_write_mask(id, DMA_DSIMFC_ENABLE, ~0, DSIMFC_SRESET);
}

static int dsimfc_reg_wait_sw_reset_status(u32 id)
{
	struct dsimfc_device *dsimfc = get_dsimfc_drvdata(id);
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsimfc->res.regs + DMA_DSIMFC_ENABLE,
			val, !(val & DSIMFC_SRESET), 10, 2000); /* timeout 2ms */
	if (ret) {
		cal_log_err(id, "[dsimfc%d] timeout sw-reset\n", id);
		return ret;
	}

	return 0;
}

void dsimfc_reg_set_start(u32 id)
{
	dsimfc_write_mask(id, DMA_DSIMFC_ENABLE, 1, DSIMFC_START);
}

u32 dsimfc_reg_get_op_status(u32 id)
{
	u32 val;

	val = dsimfc_read(id, DMA_DSIMFC_ENABLE);
	return DSIMFC_OP_STATUS_GET(val);
}

static void dsimfc_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsimfc_write_mask(id, DMA_DSIMFC_IRQ, val, DSIMFC_ALL_IRQ_MASK);
}

static void dsimfc_reg_set_irq_enable(u32 id)
{
	dsimfc_write_mask(id, DMA_DSIMFC_IRQ, ~0, DSIMFC_IRQ_ENABLE);
}

static void dsimfc_reg_set_irq_disable(u32 id)
{
	dsimfc_write_mask(id, DMA_DSIMFC_IRQ, 0, DSIMFC_IRQ_ENABLE);
}

static void dsimfc_reg_clear_irq(u32 id, u32 irq)
{
	dsimfc_write_mask(id, DMA_DSIMFC_IRQ, ~0, irq);
}

static void dsimfc_reg_set_ic_max(u32 id, u32 ic_max)
{
	dsimfc_write_mask(id, DMA_DSIMFC_IN_CTRL, DSIMFC_IC_MAX(ic_max),
			DSIMFC_IC_MAX_MASK);
}

static void dsimfc_reg_set_pkt_di(u32 id, u32 di)
{
	dsimfc_write_mask(id, DMA_DSIMFC_PKT_HDR,
			DSIMFC_PKT_DI(di), DSIMFC_PKT_DI_MASK);
}

static void dsimfc_reg_set_pkt_size(u32 id, u32 size)
{
	dsimfc_write_mask(id, DMA_DSIMFC_PKT_SIZE,
			DSIMFC_PKT_SIZE(size), DSIMFC_PKT_SIZE_MASK);
}

static void dsimfc_reg_set_pkt_unit(u32 id, u32 unit)
{
	dsimfc_write_mask(id, DMA_DSIMFC_PKT_CTRL,
			DSIMFC_PKT_UNIT(unit), DSIMFC_PKT_UNIT_MASK);
}

static void dsimfc_reg_set_base_addr(u32 id, u32 addr)
{
	dsimfc_write_mask(id, DMA_DSIMFC_BASE_ADDR,
			DSIMFC_BASE_ADDR(addr), DSIMFC_BASE_ADDR_MASK);
}

static void dsimfc_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	dsimfc_write_mask(id, DMA_DSIMFC_DEADLOCK_CTRL, val, DSIMFC_DEADLOCK_NUM_EN);
	dsimfc_write_mask(id, DMA_DSIMFC_DEADLOCK_CTRL, DSIMFC_DEADLOCK_NUM(dl_num),
				DSIMFC_DEADLOCK_NUM_MASK);
}

static void dsimfc_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = DMA_DSIMFC_QOS_LUT_LOW;
	else
		reg_id = DMA_DSIMFC_QOS_LUT_HIGH;
	dsimfc_write(id, reg_id, qos_t);
}

static void dsimfc_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsimfc_write_mask(id, DMA_DSIMFC_DYNAMIC_GATING, val, DSIMFC_SRAM_CG_EN);
}

static void dsimfc_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsimfc_write_mask(id, DMA_DSIMFC_DYNAMIC_GATING, val, DSIMFC_DG_EN_ALL);
}

u32 dsimfc_reg_get_pkt_transfer(u32 id)
{
	u32 val;

	val = dsimfc_read(id, DMA_DSIMFC_PKT_TRANSFER);
	return DSIMFC_PKT_TR_SIZE_GET(val);
}

void dsimfc_reg_init(u32 id)
{
	dsimfc_reg_set_irq_mask_all(id, 1);
	dsimfc_reg_set_irq_disable(id);
	dsimfc_reg_set_in_qos_lut(id, 0, 0x44444444);
	dsimfc_reg_set_in_qos_lut(id, 1, 0x44444444);
	dsimfc_reg_set_sram_clk_gate_en(id, 0);
	dsimfc_reg_set_dynamic_gating_en_all(id, 0);
	dsimfc_reg_set_ic_max(id, 0x40);
	dsimfc_reg_set_deadlock(id, 1, 0x7fffffff);
}

void dsimfc_reg_set_config(u32 id)
{
	struct dsimfc_device *dsimfc = get_dsimfc_drvdata(id);
	struct dsimfc_config *config = dsimfc->config;

	dsimfc_reg_set_pkt_di(id, config->di);
	dsimfc_reg_set_pkt_size(id, config->size);
	dsimfc_reg_set_pkt_unit(id, config->unit);
	dsimfc_reg_set_base_addr(id, config->buf);
}

void dsimfc_reg_irq_enable(u32 id)
{
	dsimfc_reg_set_irq_mask_all(id, 0);
	dsimfc_reg_set_irq_enable(id);
}

int dsimfc_reg_deinit(u32 id, bool reset)
{
	dsimfc_reg_clear_irq(id, DSIMFC_ALL_IRQ_CLEAR);
	dsimfc_reg_set_irq_mask_all(id, 1);

	if (reset) {
		dsimfc_reg_set_sw_reset(id);
		if (dsimfc_reg_wait_sw_reset_status(id))
			return -1;
	}

	return 0;
}

u32 dsimfc_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dsimfc_read(id, DMA_DSIMFC_IRQ);
	if (val & DSIMFC_CONFIG_ERR_IRQ) {
		cfg_err = dsimfc_read(id, DMA_DSIMFC_S_CONFIG_ERR_STATUS);
		cal_log_err(id, "dsimfc%d config error occur(0x%x)\n", id, cfg_err);
	}
	dsimfc_reg_clear_irq(id, val);

	return val;
}

void __dsimfc_dump(u32 id, void __iomem *regs)
{
	cal_log_info(id, "\n=== DMA_DSIMFC%d SFR DUMP ===\n", id);
	dpu_print_hex_dump(regs, regs + 0x0000, 0x310);
	dpu_print_hex_dump(regs, regs + 0x0420, 0x4);
	dpu_print_hex_dump(regs, regs + 0x0740, 0x4);
}
