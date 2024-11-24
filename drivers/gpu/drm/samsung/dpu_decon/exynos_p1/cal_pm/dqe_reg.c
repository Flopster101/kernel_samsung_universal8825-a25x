// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/iopoll.h>
#include <cal_config.h>
#include <regs-dqe.h>
#include <regs-dpp.h>
#include <regs-decon.h>
#include <dqe_cal.h>
#include <decon_cal.h>
#include <linux/dma-buf.h>

static struct cal_regs_desc regs_dqe[REGS_DQE_TYPE_MAX][REGS_DQE_ID_MAX];

#define dqe_regs_desc(id)			(&regs_dqe[REGS_DQE][id])
#define dqe_read(id, offset)	cal_read(dqe_regs_desc(id), offset)
#define dqe_write(id, offset, val)	cal_write(dqe_regs_desc(id), offset, val)
#define dqe_read_mask(id, offset, mask)		\
		cal_read_mask(dqe_regs_desc(id), offset, mask)
#define dqe_write_mask(id, offset, val, mask)	\
		cal_write_mask(dqe_regs_desc(id), offset, val, mask)
#define dqe_write_relaxed(id, offset, val)		\
		cal_write_relaxed(dqe_regs_desc(id), offset, val)

#define dma_regs_desc(id)			(&regs_dqe[REGS_EDMA][id])
#define dma_read(id, offset)	cal_read(dma_regs_desc(id), offset)
#define dma_write(id, offset, val)	cal_write(dma_regs_desc(id), offset, val)
#define dma_read_mask(id, offset, mask)		\
		cal_read_mask(dma_regs_desc(id), offset, mask)
#define dma_write_mask(id, offset, val, mask)	\
		cal_write_mask(dma_regs_desc(id), offset, val, mask)
#define dma_write_relaxed(id, offset, val)		\
		cal_write_relaxed(dma_regs_desc(id), offset, val)

/****************** EDMA CAL functions ******************/
void edma_reg_set_irq_mask_all(u32 id, u32 en)
{
#if defined(EDMA_CGC_IRQ)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_IRQ, val, CGC_ALL_IRQ_MASK);
#endif
}

void edma_reg_set_irq_enable(u32 id, u32 en)
{
#if defined(EDMA_CGC_IRQ)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_IRQ, val, CGC_IRQ_ENABLE);
#endif
}

void edma_reg_clear_irq_all(u32 id)
{
#if defined(EDMA_CGC_IRQ)
	dma_write_mask(id, EDMA_CGC_IRQ, ~0, CGC_ALL_IRQ_CLEAR);
#endif
}

u32 edma_reg_get_interrupt_and_clear(u32 id)
{
	u32 val = 0;

#if defined(EDMA_CGC_IRQ)
	val = dqe_read(id, EDMA_CGC_IRQ);
	edma_reg_clear_irq_all(id);

	if (val & CGC_CONFIG_ERR_IRQ)
		cal_log_err(id, "CGC_CONFIG_ERR_IRQ\n");
	if (val & CGC_READ_SLAVE_ERR_IRQ)
		cal_log_err(id, "CGC_READ_SLAVE_ERR_IRQ\n");
	if (val & CGC_DEADLOCK_IRQ)
		cal_log_err(id, "CGC_DEADLOCK_IRQ\n");
	if (val & CGC_FRAME_DONE_IRQ)
		cal_log_info(id, "CGC_FRAME_DONE_IRQ\n");
#endif
	return val;
}

void edma_reg_set_sw_reset(u32 id)
{
#if defined(EDMA_CGC_ENABLE)
	dma_write_mask(id, EDMA_CGC_ENABLE, ~0, CGC_SRESET);
#endif
}

void edma_reg_set_start(u32 id, u32 en)
{
#if defined(EDMA_CGC_ENABLE)
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, EDMA_CGC_ENABLE, val, CGC_START_SET0);
#endif
}

void edma_reg_set_base_addr(u32 id, dma_addr_t addr)
{
#if defined(EDMA_CGC_BASE_ADDR_SET0)
	dma_write(id, EDMA_CGC_BASE_ADDR_SET0, addr);
#endif
}

void __dqe_dump(u32 id, struct dqe_regs *regs, bool en_lut)
{
	void __iomem *dqe_base_regs = regs->dqe_base_regs;
	void __iomem *edma_base_regs = regs->edma_base_regs;

	cal_log_info(id, "=== DQE_TOP SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x0000, 0x100);

	cal_log_info(id, "=== DQE_ATC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x400, 0x40);
	cal_log_info(id, "=== DQE_ATC SHADOW SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x400 + SHADOW_DQE_OFFSET, 0x40);

	cal_log_info(id, "=== DQE_HSC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x800, 0x760);

	cal_log_info(id, "=== DQE_GAMMA_MATRIX SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x1400, 0x20);

	cal_log_info(id, "=== DQE_DEGAMMA SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x1800, 0x100);

	cal_log_info(id, "=== DQE_CGC SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2000, 0x20);

	cal_log_info(id, "=== DQE_REGAMMA SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2400, 0x200);

	cal_log_info(id, "=== DQE_CGC_DITHER SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2800, 0x20);

	cal_log_info(id, "=== DQE_DISP_DITHER SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x2C00, 0x20);

	cal_log_info(id, "=== DQE_RCD SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3000, 0x20);

	cal_log_info(id, "=== DQE_DE SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3200, 0x20);

	cal_log_info(id, "=== DQE_SCL SFR DUMP ===\n");
	dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x3400, 0x200);

	if (en_lut) {
		cal_log_info(id, "=== DQE_CGC_LUT SFR DUMP ===\n");
		cal_log_info(id, "== [ RED CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x4000, 0x2664);
		cal_log_info(id, "== [ GREEN CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0x8000, 0x2664);
		cal_log_info(id, "== [ BLUE CGC LUT ] ==\n");
		dpu_print_hex_dump(dqe_base_regs, dqe_base_regs + 0xC000, 0x2664);
	}

	if (edma_base_regs) {
		cal_log_info(id, "=== EDMA CGC SFR DUMP ===\n");
		dpu_print_hex_dump(edma_base_regs, edma_base_regs + 0x0000, 0x48);
	}
}

u32 dqe_reg_get_version(u32 id)
{
	return dqe_read(id, DQE_TOP_VER);
}

static inline u32 dqe_reg_get_lut_addr(u32 id, enum dqe_reg_type type,
					u32 index, u32 opt)
{
	u32 addr = 0;

	switch (type) {
	case DQE_REG_GAMMA_MATRIX:
		if (index >= DQE_GAMMA_MATRIX_REG_MAX)
			return 0;
		addr = DQE_GAMMA_MATRIX_LUT(index);
		break;
	case DQE_REG_DISP_DITHER:
		addr = DQE_DISP_DITHER;
		break;
	case DQE_REG_CGC_DITHER:
		addr = DQE_CGC_DITHER;
		break;
	case DQE_REG_CGC_CON:
		if (index >= DQE_CGC_CON_REG_MAX)
			return 0;
		addr = DQE_CGC_MC_R(index);
		break;
	case DQE_REG_CGC:
		if (index >= DQE_CGC_REG_MAX)
			return 0;
		switch (opt) {
		case DQE_REG_CGC_R:
			addr = DQE_CGC_LUT_R(index);
			break;
		case DQE_REG_CGC_G:
			addr = DQE_CGC_LUT_G(index);
			break;
		case DQE_REG_CGC_B:
			addr = DQE_CGC_LUT_B(index);
			break;
		}
		break;
	case DQE_REG_DEGAMMA:
		if (index >= DQE_DEGAMMA_REG_MAX)
			return 0;
		addr = DQE_DEGAMMA_LUT(index);
		break;
	case DQE_REG_REGAMMA:
		if (index >= DQE_REGAMMA_REG_MAX)
			return 0;
		addr = DQE_REGAMMA_LUT(index);
		break;
	case DQE_REG_HSC:
		if (index >= DQE_HSC_REG_MAX)
			return 0;
		switch (opt) {
		case DQE_REG_HSC_CON:
			if (index >= DQE_HSC_REG_CTRL_MAX)
				return 0;
			addr = DQE_HSC_CONTROL_LUT(index);
			break;
		case DQE_REG_HSC_POLY:
			if (index >= DQE_HSC_REG_POLY_MAX)
				return 0;
			addr = DQE_HSC_POLY_LUT(index);
			break;
		case DQE_REG_HSC_GAIN:
			if (index >= DQE_HSC_REG_GAIN_MAX)
				return 0;
			addr = DQE_HSC_LSC_GAIN_LUT(index);
			break;
		}
		break;
	case DQE_REG_ATC:
		if (index >= DQE_ATC_REG_MAX)
			return 0;
		addr = DQE_ATC_CONTROL_LUT(index);
		break;
	case DQE_REG_SCL:
		if (index >= DQE_SCL_REG_MAX)
			return 0;
#if !IS_ENABLED(CONFIG_SOC_S5E9925_EVT0) // EVT0 support DECON SCL but not implemented
		addr = DQE_SCL_Y_VCOEF(index);
#endif
		break;
	case DQE_REG_DE:
		if (index >= DQE_DE_REG_MAX)
			return 0;
		addr = DQE_DE_LUT(index);
		break;
	case DQE_REG_LPD:
		if (index >= DQE_LPD_REG_MAX)
			return 0;
		addr = DQE_TOP_LPD(index);
		break;
	default:
		cal_log_err(id, "invalid reg type %d\n", type);
		return 0;
	}

	return addr;
}

u32 dqe_reg_get_lut(u32 id, enum dqe_reg_type type, u32 index, u32 opt)
{
	u32 addr = dqe_reg_get_lut_addr(id, type, index, opt);

	if (addr == 0)
		return 0;
	return dqe_read(id, addr);
}

void dqe_reg_set_lut(u32 id, enum dqe_reg_type type, u32 index, u32 value, u32 opt)
{
	u32 addr = dqe_reg_get_lut_addr(id, type, index, opt);

	if (addr == 0)
		return;
	dqe_write_relaxed(id, addr, value);
}

void dqe_reg_set_lut_on(u32 id, enum dqe_reg_type type, u32 on)
{
	u32 addr;
	u32 value;
	u32 mask;

	switch (type) {
	case DQE_REG_GAMMA_MATRIX:
		addr = DQE_GAMMA_MATRIX_CON;
		value = GAMMA_MATRIX_EN(on);
		mask = GAMMA_MATRIX_EN_MASK;
		break;
	case DQE_REG_DISP_DITHER:
		addr = DQE_DISP_DITHER;
		value = DISP_DITHER_EN(on);
		mask = DISP_DITHER_EN_MASK;
		break;
	case DQE_REG_CGC_DITHER:
		addr = DQE_CGC_DITHER;
		value = CGC_DITHER_EN(on);
		mask = CGC_DITHER_EN_MASK;
		break;
	case DQE_REG_CGC_CON:
		addr = DQE_CGC_CON;
		value = CGC_EN(on);
		mask = CGC_EN_MASK;
		break;
	case DQE_REG_CGC_DMA:
		addr = DQE_CGC_CON;
		value = CGC_COEF_DMA_REQ(on);
		mask = CGC_COEF_DMA_REQ_MASK;
		break;
	case DQE_REG_DEGAMMA:
		addr = DQE_DEGAMMA_CON;
		value = DEGAMMA_EN(on);
		mask = DEGAMMA_EN_MASK;
		break;
	case DQE_REG_REGAMMA:
		addr = DQE_REGAMMA_CON;
		value = REGAMMA_EN(on);
		mask = REGAMMA_EN_MASK;
		break;
	case DQE_REG_HSC:
		addr = DQE_HSC_CONTROL;
		value = HSC_EN(on);
		mask = HSC_EN_MASK;
		break;
	case DQE_REG_ATC:
		addr = DQE_ATC_CONTROL;
		value = DQE_ATC_EN(on);
		mask = DQE_ATC_EN_MASK;
		break;
	case DQE_REG_DE:
		addr = DQE_DE_CONTROL;
		value = DE_EN(on);
		mask = DE_EN_MASK;
		break;
	case DQE_REG_LPD:
		addr = DQE_TOP_LPD_MODE_CONTROL;
		value = DQE_LPD_MODE_EXIT(on);
		mask = DQE_LPD_MODE_EXIT_MASK;
		break;
	default:
		cal_log_err(id, "invalid reg type %d\n", type);
		return;
	}

	dqe_write_mask(id, addr, value, mask);
}

int dqe_reg_wait_cgc_dmareq(u32 id)
{
#if defined(CGC_COEF_DMA_REQ_MASK)
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dqe_regs_desc(id)->regs + DQE_CGC_CON,
			val, !(val & CGC_COEF_DMA_REQ_MASK), 10, 10000); /* timeout 10ms */
	if (ret) {
		cal_log_err(id, "[edma] timeout dma reg\n");
		return ret;
	}
#endif
	return 0;
}

void dqe_reg_set_atc_partial_ibsi(u32 id, u32 width, u32 height)
{
	u32 val, val1, val2;
	u32 small_n = width / 8;
	u32 large_N = small_n + 1;
	u32 grid = height / 16;

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 4)) : ((1 << 16) / (large_N * 4));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 4)) : ((1 << 16) / (grid * 4) + 4);
	val = ATC_IBSI_X_P1(val1) | ATC_IBSI_Y_P1(val2);
	dqe_write_relaxed(id, DQE_ATC_PARTIAL_IBSI_P1, val);

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 2)) : ((1 << 16) / (large_N * 2));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 2)) : ((1 << 16) / (grid * 2) + 2);
	val = ATC_IBSI_X_P2(val1) | ATC_IBSI_Y_P2(val2);
	dqe_write_relaxed(id, DQE_ATC_PARTIAL_IBSI_P2, val);
}

void dqe_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		enum dqe_regs_type type)
{
	regs_dqe[type][id].regs = regs;
	regs_dqe[type][id].name = name;
}

static void dqe_reg_set_img_size(u32 id, u32 width, u32 height)
{
	u32 val;

	val = DQE_FULL_IMG_VSIZE(height) | DQE_FULL_IMG_HSIZE(width);
	dqe_write_relaxed(id, DQE_TOP_FRM_SIZE, val);

	val = DQE_FULL_PXL_NUM(width * height);
	dqe_write_relaxed(id, DQE_TOP_FRM_PXL_NUM, val);

	val = DQE_PARTIAL_START_Y(0) | DQE_PARTIAL_START_X(0);
	dqe_write_relaxed(id, DQE_TOP_PARTIAL_START, val);

	val = DQE_IMG_VSIZE(height) | DQE_IMG_HSIZE(width);
	dqe_write_relaxed(id, DQE_TOP_IMG_SIZE, val);

	val = DQE_PARTIAL_SAME(0) | DQE_PARTIAL_UPDATE_EN(0);
	dqe_write_relaxed(id, DQE_TOP_PARTIAL_CON, val);
}

static void dqe_reg_set_scaled_img_size(u32 id, u32 xres, u32 yres)
{
#if !IS_ENABLED(CONFIG_SOC_S5E9925_EVT0) // EVT0 support DECON SCL but not implemented
	u32 addr;
	int h_ratio = 1 << 20; /* DECON post scaler is deprecated */
	int v_ratio = 1 << 20;

	addr = DQE_SCL_SCALED_IMG_SIZE;
	dqe_write_relaxed(id, addr, SCALED_IMG_HEIGHT(yres) | SCALED_IMG_WIDTH(xres));

	addr = DQE_SCL_MAIN_H_RATIO;
	dqe_write_mask(id, addr, H_RATIO(h_ratio), H_RATIO_MASK);

	addr = DQE_SCL_MAIN_V_RATIO;
	dqe_write_mask(id, addr, V_RATIO(v_ratio), V_RATIO_MASK);
#endif
}

/* exposed to driver layer for DQE CAL APIs */
void dqe_reg_init(u32 id, u32 width, u32 height)
{
	cal_log_debug(id, "%s +\n", __func__);
	dqe_reg_set_img_size(id, width, height);
	dqe_reg_set_scaled_img_size(id, width, height);
	decon_reg_set_dqe_enable(id, true);
	cal_log_debug(id, "%s -\n", __func__);
}
