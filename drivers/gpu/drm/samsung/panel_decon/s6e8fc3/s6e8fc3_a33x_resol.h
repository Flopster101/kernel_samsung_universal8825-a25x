/*
 * linux/drivers/video/fbdev/exynos/panel/s6e8fc3/s6e8fc3_a33x_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E8FC3_A33X_RESOL_H__
#define __S6E8FC3_A33X_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e8fc3.h"
#include "s6e8fc3_dimming.h"

struct panel_vrr s6e8fc3_a33x_default_panel_vrr[] = {
	[S6E8FC3_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E8FC3_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e8fc3_a33x_default_vrrtbl[] = {
	&s6e8fc3_a33x_default_panel_vrr[S6E8FC3_VRR_90HS],
	&s6e8fc3_a33x_default_panel_vrr[S6E8FC3_VRR_60HS],
};

static struct panel_resol s6e8fc3_a33x_default_resol[] = {
	[S6E8FC3_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = s6e8fc3_a33x_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e8fc3_a33x_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e8fc3_a33x_display_mode[] = {
	/* FHD */
	[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_90HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_90HS,
		.resol = &s6e8fc3_a33x_default_resol[S6E8FC3_RESOL_1080x2400],
		.vrr = &s6e8fc3_a33x_default_panel_vrr[S6E8FC3_VRR_90HS],
	},
	[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &s6e8fc3_a33x_default_resol[S6E8FC3_RESOL_1080x2400],
		.vrr = &s6e8fc3_a33x_default_panel_vrr[S6E8FC3_VRR_60HS],
	},
};

static struct common_panel_display_mode *s6e8fc3_a33x_display_mode_array[] = {
	[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_90HS] = &s6e8fc3_a33x_display_mode[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_90HS],
	[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_60HS] = &s6e8fc3_a33x_display_mode[S6E8FC3_A33X_DISPLAY_MODE_1080x2400_60HS],
};

static struct common_panel_display_modes s6e8fc3_a33x_display_modes = {
	.num_modes = ARRAY_SIZE(s6e8fc3_a33x_display_mode),
	.modes = (struct common_panel_display_mode **)&s6e8fc3_a33x_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E8FC3_A33X_RESOL_H__ */
