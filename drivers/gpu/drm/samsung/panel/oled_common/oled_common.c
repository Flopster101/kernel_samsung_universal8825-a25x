/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_kunit.h"
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../panel_bl.h"
#include "../maptbl.h"
#include "../util.h"
#include "oled_function.h"

int oled_maptbl_init_default(struct maptbl *tbl)
{
	if (!tbl) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (!tbl->pdata) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	return 0;
}

EXPORT_SYMBOL(oled_maptbl_init_default);

int oled_maptbl_getidx_default(struct maptbl *m)
{
	return maptbl_getidx_from_props(m);
}

EXPORT_SYMBOL(oled_maptbl_getidx_default);

int oled_maptbl_getidx_gm2_brt(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	int row;

	if (!tbl)
		return -EINVAL;

	if (maptbl_get_countof_dimen(tbl) != 2) {
		panel_err("expected 2D-array, but %dD\n",
				maptbl_get_countof_dimen(tbl));
		return -EINVAL;
	}

	panel = tbl->pdata;
	if (!panel) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	row = get_brightness_pac_step_by_subdev_id(panel_bl,
			PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

void oled_maptbl_copy_dummy(struct maptbl *tbl, u8 *dst)
{
}

void oled_maptbl_copy_default(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl) {
		panel_err("tbl is null\n");
		return;
	}

	if (!dst) {
		panel_err("dst is null\n");
		return;
	}

	if (!tbl->arr) {
		panel_err("maptbl(%s) source buffer is null\n",
				maptbl_get_name(tbl));
		return;
	}

	if (maptbl_get_sizeof_copy(tbl) == 0) {
		panel_err("maptbl(%s) sizeof_copy is zero\n",
				maptbl_get_name(tbl));
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("maptbl(%s) failed to get index(%d)\n",
				maptbl_get_name(tbl), idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx],
			sizeof(u8) * maptbl_get_sizeof_copy(tbl));
	panel_dbg("maptbl(%s) copy to dst(index:%d size:%d)\n",
			maptbl_get_name(tbl), idx, maptbl_get_sizeof_copy(tbl));
	usdm_dbg_bytes(dst, maptbl_get_sizeof_copy(tbl));
}
EXPORT_SYMBOL(oled_maptbl_copy_default);

void oled_maptbl_copy_tset(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl) {
		panel_err("tbl is null\n");
		return;
	}

	if (!dst) {
		panel_err("dst is null\n");
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	*dst = (panel_data->props.temperature < 0) ?
		BIT(7) | abs(panel_data->props.temperature) :
		panel_data->props.temperature;
}

#ifdef CONFIG_USDM_PANEL_COPR
void oled_maptbl_copy_copr(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct copr_info *copr;

	if (!tbl || !dst)
		return;

	panel = tbl->pdata;
	if (unlikely(!panel))
		return;

	copr = &panel->copr;
	copr_reg_to_byte_array(&copr->props.reg,
			copr->props.version, dst);
}
#endif

MODULE_DESCRIPTION("oled_common driver");
MODULE_LICENSE("GPL");
