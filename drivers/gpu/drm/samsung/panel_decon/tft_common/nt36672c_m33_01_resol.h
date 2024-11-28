/*
 * linux/drivers/video/fbdev/exynos/panel/s6e8fc3/nt36672c_m33x_01_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36672C_M33X_01_RESOL_H__
#define __NT36672C_M33X_01_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"

enum {
	NT36672C_M33X_01_DISPLAY_MODE_1080x2408_120HS,
	NT36672C_M33X_01_DISPLAY_MODE_1080x2408_60HS,
	MAX_NT36672C_M33X_01_DISPLAY_MODE,
};

enum {
	NT36672C_M33X_01_RESOL_1080x2408,
	MAX_NT36672C_M33X_01_RESOL,
};

enum {
	NT36672C_M33X_01_VRR_120HS,
	NT36672C_M33X_01_VRR_60HS,
	MAX_NT36672C_M33X_01_VRR,
};

struct panel_vrr nt36672c_m33x_01_default_panel_vrr[] = {
	[NT36672C_M33X_01_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[NT36672C_M33X_01_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *nt36672c_m33x_01_default_vrrtbl[] = {
	&nt36672c_m33x_01_default_panel_vrr[NT36672C_M33X_01_VRR_120HS],
	&nt36672c_m33x_01_default_panel_vrr[NT36672C_M33X_01_VRR_60HS],
};

static struct panel_resol nt36672c_m33x_01_default_resol[] = {
	[NT36672C_M33X_01_RESOL_1080x2408] = {
		.w = 1080,
		.h = 2408,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 8,
			},
		},
		.available_vrr = nt36672c_m33x_01_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(nt36672c_m33x_01_default_vrrtbl),
	}
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode nt36672c_m33x_01_display_mode[] = {
	/* FHD */
	[NT36672C_M33X_01_DISPLAY_MODE_1080x2408_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_120HS,
		.resol = &nt36672c_m33x_01_default_resol[NT36672C_M33X_01_RESOL_1080x2408],
		.vrr = &nt36672c_m33x_01_default_panel_vrr[NT36672C_M33X_01_VRR_120HS],
	},
	[NT36672C_M33X_01_DISPLAY_MODE_1080x2408_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_60HS,
		.resol = &nt36672c_m33x_01_default_resol[NT36672C_M33X_01_RESOL_1080x2408],
		.vrr = &nt36672c_m33x_01_default_panel_vrr[NT36672C_M33X_01_VRR_60HS],
	},
};

static struct common_panel_display_mode *nt36672c_m33x_01_display_mode_array[] = {
	&nt36672c_m33x_01_display_mode[NT36672C_M33X_01_DISPLAY_MODE_1080x2408_120HS],
	&nt36672c_m33x_01_display_mode[NT36672C_M33X_01_DISPLAY_MODE_1080x2408_60HS],
};

static struct common_panel_display_modes nt36672c_m33x_01_display_modes = {
	.num_modes = ARRAY_SIZE(nt36672c_m33x_01_display_mode_array),
	.modes = (struct common_panel_display_mode **)&nt36672c_m33x_01_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __NT36672C_M33X_01_RESOL_H__ */
