/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DSIMFC CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DSIMFC_CAL_H__
#define __SAMSUNG_DSIMFC_CAL_H__

#if IS_ENABLED(CONFIG_EXYNOS_DMA_DSIMFC)

/* DSIMFC CAL APIs exposed to DSIMFC driver */
void dsimfc_regs_desc_init(void __iomem *regs, const char *name,
		unsigned int id);
void dsimfc_reg_init(u32 id);
void dsimfc_reg_set_config(u32 id);
void dsimfc_reg_irq_enable(u32 id);
int dsimfc_reg_deinit(u32 id, bool reset);
void dsimfc_reg_set_start(u32 id);

/* DMA_DSIMFC DEBUG */
void __dsimfc_dump(u32 id, void __iomem *regs);

/* DMA_DSIMFC interrupt handler */
u32 dsimfc_reg_get_irq_and_clear(u32 id);

#endif

#endif /* __SAMSUNG_DSIMFC_CAL_H__ */
