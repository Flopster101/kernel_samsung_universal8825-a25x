/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102E_GTA4XLS_00_RESOL_H__
#define __HX83102E_GTA4XLS_00_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "tft_common.h"

enum {
	HX83102E_GTA4XLS_00_DISPLAY_MODE_1200x2000_60HS,
	MAX_HX83102E_GTA4XLS_00_DISPLAY_MODE,
};

enum {
	HX83102E_GTA4XLS_00_RESOL_1200x2000,
	MAX_HX83102E_GTA4XLS_00_RESOL,
};

enum {
	HX83102E_GTA4XLS_00_VRR_60HS,
	MAX_HX83102E_GTA4XLS_00_VRR,
};

struct panel_vrr hx83102e_gta4xls_00_default_panel_vrr[] = {
	[HX83102E_GTA4XLS_00_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *hx83102e_gta4xls_00_default_vrrtbl[] = {
	&hx83102e_gta4xls_00_default_panel_vrr[HX83102E_GTA4XLS_00_VRR_60HS],
};

static struct panel_resol hx83102e_gta4xls_00_default_resol[] = {
	[HX83102E_GTA4XLS_00_RESOL_1200x2000] = {
		.w = 1200,
		.h = 2000,
		.comp_type = PN_COMP_TYPE_NONE,
		.available_vrr = hx83102e_gta4xls_00_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(hx83102e_gta4xls_00_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode hx83102e_gta4xls_00_display_mode[] = {
	[HX83102E_GTA4XLS_00_DISPLAY_MODE_1200x2000_60HS] = {
		.name = PANEL_DISPLAY_MODE_1200x2000_60HS,
		.resol = &hx83102e_gta4xls_00_default_resol[HX83102E_GTA4XLS_00_RESOL_1200x2000],
		.vrr = &hx83102e_gta4xls_00_default_panel_vrr[HX83102E_GTA4XLS_00_VRR_60HS],
	},
};

static struct common_panel_display_mode *hx83102e_gta4xls_00_display_mode_array[] = {
	&hx83102e_gta4xls_00_display_mode[HX83102E_GTA4XLS_00_DISPLAY_MODE_1200x2000_60HS],
};

static struct common_panel_display_modes hx83102e_gta4xls_00_display_modes = {
	.num_modes = ARRAY_SIZE(hx83102e_gta4xls_00_display_mode_array),
	.modes = (struct common_panel_display_mode **)&hx83102e_gta4xls_00_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __HX83102E_GTA4XLS_00_RESOL_H__ */
