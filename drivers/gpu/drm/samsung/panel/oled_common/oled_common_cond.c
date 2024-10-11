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
#include "../panel_bl.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../panel_vrr.h"

bool oled_cond_is_panel_display_mode_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_idx !=
			panel->panel_data.props.vrr_origin_idx);
}

bool oled_cond_is_panel_refresh_rate_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_fps !=
			panel->panel_data.props.vrr_origin_fps);
}

bool oled_cond_is_panel_refresh_mode_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_mode !=
			panel->panel_data.props.vrr_origin_mode);
}

bool oled_cond_is_panel_refresh_mode_ns(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (get_panel_refresh_mode(panel) == VRR_NORMAL_MODE);
}

bool oled_cond_is_panel_refresh_mode_hs(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (get_panel_refresh_mode(panel) == VRR_HS_MODE);
}

bool oled_cond_is_panel_state_lpm(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel_get_cur_state(panel) == PANEL_STATE_ALPM);
}

bool oled_cond_is_panel_state_not_lpm(struct panel_device *panel)
{
	if (!panel)
		return false;

	return !oled_cond_is_panel_state_lpm(panel);
}

bool oled_cond_is_first_set_bl(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel_bl_get_brightness_set_count(&panel->panel_bl) == 0);
}

bool oled_cond_is_panel_mres_updated(struct panel_device *panel)
{
	if (!panel)
		return false;

	return panel->panel_data.props.mres_updated;
}

bool oled_cond_is_panel_mres_updated_bigger(struct panel_device *panel)
{
	struct panel_mres *mres;
	struct panel_properties *props;

	if (!panel)
		return false;

	mres = &panel->panel_data.mres;
	props = &panel->panel_data.props;

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		panel_warn("not support multi-resolution\n");
		return false;
	}

	if (props->old_mres_mode >= mres->nr_resol) {
		panel_warn("old_mres_mode(%d) is out of number of resolutions(%d)\n",
				props->old_mres_mode, mres->nr_resol);
		return false;
	}

	if (props->mres_mode >= mres->nr_resol) {
		panel_warn("mres_mode(%d) is out of number of resolutions(%d)\n",
				props->mres_mode, mres->nr_resol);
		return false;
	}

	return oled_cond_is_panel_mres_updated(panel) &&
			((mres->resol[props->old_mres_mode].w *
			  mres->resol[props->old_mres_mode].h) <
			 (mres->resol[props->mres_mode].w *
			  mres->resol[props->mres_mode].h));
}

bool oled_cond_is_acl_pwrsave_on(struct panel_device *panel)
{
	return (panel_bl_get_acl_pwrsave(&panel->panel_bl) == ACL_PWRSAVE_ON);
}

MODULE_DESCRIPTION("oled_common driver");
MODULE_LICENSE("GPL");
