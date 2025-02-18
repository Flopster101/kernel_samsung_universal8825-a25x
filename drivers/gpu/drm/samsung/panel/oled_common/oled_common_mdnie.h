/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_MDNIE_H__
#define __OLED_COMMON_MDNIE_H__

#include "../maptbl.h"

int oled_maptbl_getidx_mdnie_scenario_mode(struct maptbl *tbl);
#ifdef CONFIG_USDM_PANEL_HMD
int oled_maptbl_getidx_mdnie_hmd(struct maptbl *tbl);
#endif
int oled_maptbl_getidx_mdnie_hdr(struct maptbl *tbl);
int oled_maptbl_getidx_mdnie_trans_mode(struct maptbl *tbl);
int oled_maptbl_getidx_mdnie_night_mode(struct maptbl *tbl);
int oled_maptbl_getidx_color_lens(struct maptbl *tbl);
void oled_maptbl_copy_scr_white(struct maptbl *tbl, u8 *dst);
void oled_maptbl_copy_scr_cr(struct maptbl *tbl, u8 *dst);
void oled_maptbl_copy_scr_white_anti_glare(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_USDM_MDNIE_AFC
void oled_maptbl_copy_afc(struct maptbl *tbl, u8 *dst);
#endif
#endif /* __OLED_COMMON_MDNIE_H__ */
