/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_H__
#define __OLED_COMMON_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include "../maptbl.h"
#include "../panel_function.h"

int oled_maptbl_init_default(struct maptbl *tbl);
int oled_maptbl_getidx_default(struct maptbl *tbl);
int oled_maptbl_getidx_gm2_brt(struct maptbl *tbl);
void oled_maptbl_copy_dummy(struct maptbl *tbl, u8 *dst);
void oled_maptbl_copy_default(struct maptbl *tbl, u8 *dst);
void oled_maptbl_copy_tset(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_USDM_PANEL_COPR
void oled_maptbl_copy_copr(struct maptbl *tbl, u8 *dst);
#endif

#endif /* __OLED_COMMON_H__ */
