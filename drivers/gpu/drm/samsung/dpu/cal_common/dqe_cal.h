/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DQE CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DQE_CAL_H__
#define __SAMSUNG_DQE_CAL_H__

#include <linux/types.h>
#include <regs-dqe.h>

struct dqe_regs {
	void __iomem *dqe_base_regs;
	void __iomem *edma_base_regs;
};

enum dqe_reg_type {
	DQE_REG_GAMMA_MATRIX,
	DQE_REG_DISP_DITHER,
	DQE_REG_CGC_DITHER,
	DQE_REG_CGC_CON,
	DQE_REG_CGC_DMA,
	DQE_REG_CGC,
	DQE_REG_DEGAMMA,
	DQE_REG_REGAMMA,
	DQE_REG_HSC,
	DQE_REG_ATC,
	DQE_REG_SCL,
	DQE_REG_DE,
	DQE_REG_LPD,
	DQE_REG_MAX,
};

enum dqe_reg_hsc_opt {
	DQE_REG_HSC_CON,
	DQE_REG_HSC_POLY,
	DQE_REG_HSC_GAIN,
	DQE_REG_HSC_MAX,
};

enum dqe_reg_cgc_opt {
	DQE_REG_CGC_R,
	DQE_REG_CGC_G,
	DQE_REG_CGC_B,
	DQE_REG_CGC_MAX,
};

void edma_reg_set_irq_mask_all(u32 id, u32 en);
void edma_reg_set_irq_enable(u32 id, u32 en);
void edma_reg_clear_irq_all(u32 id);
void edma_reg_set_sw_reset(u32 id);
void edma_reg_set_start(u32 id, u32 en);
void edma_reg_set_base_addr(u32 id, dma_addr_t addr);
u32 edma_reg_get_interrupt_and_clear(u32 id);
void __dqe_dump(u32 id, struct dqe_regs *regs, bool en_lut);
u32 dqe_reg_get_version(u32 id);
u32 dqe_reg_get_lut(u32 id, enum dqe_reg_type type, u32 index, u32 opt);
void dqe_reg_set_lut(u32 id, enum dqe_reg_type type, u32 index, u32 value, u32 opt);
void dqe_reg_set_lut_on(u32 id, enum dqe_reg_type type, u32 on);
int dqe_reg_wait_cgc_dmareq(u32 id);
void dqe_reg_set_atc_partial_ibsi(u32 id, u32 width, u32 height);
void dqe_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		enum dqe_regs_type type);
void dqe_reg_init(u32 id, u32 width, u32 height);
void dqe_reg_set_disp_dither(u32 id, u32 val);
#endif /* __SAMSUNG_DQE_CAL_H__ */
