// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3FC3 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>

#include "panel-samsung-drv.h"

static unsigned char PPS_TABLE[] = {
	//1080x2400 Slice Info : 540x40
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60, 0x04, 0x38, 0x00, 0x28,
	0x02, 0x1C, 0x02, 0x1C, 0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
	0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B, 0x18, 0x00, 0x10, 0xF0,
	0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
	0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B, 0x74,
	0x3B, 0x74, 0x6B, 0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char GLOBAL_PARAM[] = {
	0xF2,
	0x00, 0x05, 0x0E, 0x58, 0x54, 0x01, 0x0C, 0x00,
	0x04, 0x27, 0x24, 0x2F, 0xB0, 0x0C, 0x09, 0x74,
	0x26, 0xE4, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};

static u32 s6e3fc3_find_table_index(const struct exynos_panel *ctx, u32 yres)
{
	/* FHD+ mode */
	return 0;
}

static int s6e3fc3_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);
	ctx->enabled = false;
	panel_debug(ctx, "+\n");
	return 0;
}

static int s6e3fc3_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, false);
	panel_debug(ctx, "-\n");
	return 0;
}

static int s6e3fc3_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	panel_debug(ctx, "-\n");

	return 0;
}

static int s6e3fc3_set_brightness(struct exynos_panel *exynos_panel, u16 br)
{
	u16 brightness;

	brightness = (br & 0xff) << 8 | br >> 8;
	return exynos_dcs_set_brightness(exynos_panel, brightness);
}

static void s6e3fc3_set_resolution(struct exynos_panel *ctx,
				const struct exynos_panel_mode *pmode)
{
	panel_debug(ctx, "+\n");

	panel_debug(ctx, "-\n");
}

static void s6e3fc3_set_vrefresh(struct exynos_panel *ctx,
				const struct drm_display_mode *pmode)
{
	panel_debug(ctx, "+\n");

	// if needs for lp mode, should put code in this.

	panel_debug(ctx, "-\n");
}

static void s6e3fc3_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	panel_debug(ctx, "+\n");

	if (!ctx->enabled)
		return;

	if (SEAMLESS_MODESET_MRES & flags)
		s6e3fc3_set_resolution(ctx, pmode);
	if (SEAMLESS_MODESET_VREF & flags)
		s6e3fc3_set_vrefresh(ctx, &pmode->mode);

	panel_debug(ctx, "-\n");
}

static void s6e3fc3_lp_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}

static int s6e3fc3_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx =
		container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;
	const struct drm_display_mode *mode;
	u32 tab_idx;

	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		return -EINVAL;
	}
	mode = &pmode->mode;

	panel_debug(ctx, "+\n");

	exynos_panel_reset(ctx);
	msleep(5);

	/* sleep out: 50ms delay */
	EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, 50, 0x11);

	/* Command Unlock */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9f, 0xa5, 0xa5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0x5a, 0x5a);

	/* Global Params */
	EXYNOS_DCS_WRITE_TABLE(ctx, GLOBAL_PARAM);

	/* Panel Update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf7, 0x0f);

	/* DSC related configuration */
	exynos_dcs_compression_mode(ctx, true);
	tab_idx = s6e3fc3_find_table_index(ctx, pmode->mode.vdisplay);
	EXYNOS_PPS_LONG_WRITE_TABLE(ctx, PPS_TABLE);

	/* TE on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x35);

	/* Address Set */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);

	/* Set Min. ROI Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xC2, 0x1B, 0x41, 0xB0, 0x0E, 0x00, 0x3C,
			     0x5A, 0x00, 0x00);

	/* FFC1 Default Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x2A, 0xC5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xC5,	0x0D, 0x10, 0x80, 0x45);

	/* FFC2 Default Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0,	0x00, 0x2E, 0xC5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xC5,	0x53, 0xA0);

	/* ERR FG ON */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xE5, 0x15);

	/* ERR FG Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xED,	0x04, 0x4C, 0x20);

	/* Vsync Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0,	0x00, 0x04, 0xF2);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0x54);

	/* PCD Det Set */
	/* 1st 0x5C: default high, 2nd 0x51 : Enable SW RESET */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xCC,	0x5C, 0x51);

	/* Freq. Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0,	0x00, 0x27, 0xF2);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0x00);

	/* Panel Update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf7, 0x0f);

	/* Porch Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x2E, 0xF2);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF2, 0x55);

	/* Panel Update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf7, 0x0f);

	/* ACL Off */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x55, 0x00);

	/* 60Hz Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x60, 0x00, 0x00);

	/* Dimming Setting 1 */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB0, 0x00, 0x91, 0x63);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x63, 0x60);

	/* Dimming Setting 2 */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x53, 0x28);

	/* Brightness Setting */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x51, 0x01, 0xc5);

	/* Panel Update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf7, 0x0f);

	/* TSET */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xb5, 0x19);

	/* Command Lock */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9f, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0xa5, 0xa5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0xa5, 0xa5);

	msleep(100);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9f, 0xa5, 0xa5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x29); /* display on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9f, 0x5a, 0x5a);

	ctx->enabled = true;

	if (pmode->exynos_mode.is_lp_mode)
		s6e3fc3_lp_mode_set(ctx, pmode);

	panel_debug(ctx, "-\n");

	return 0;
}

/* drm_display_mode
 *
 * @clock: vtotal * htotal * fps / 1000
 * @hdisplay = horizon resolution
 * @hsync_start = hdisplay + hfp
 * @hsync_end = hdisplay + hfp + hsa
 * @htotal = hdisplay + hfp + hsa + hbp
 * @vdisplay = vertical resolution,
 * @vsync_start = vdisplay + vfp
 * @vsync_end = vdisplay + vfp + vsa
 * @vtotal = vdisplay + vfp + vsa + + vbp
 */
static const struct exynos_panel_mode s6e3fc3_mode[] = {
	{
		.mode = {
			.name = "1080x2400@60",
			.clock = 157948,
			.hdisplay = 1080,
			.hsync_start = 1080 + 2,	// add hfp
			.hsync_end = 1080 + 2 + 2,	// add hsa
			.htotal = 1080 + 2 + 2 + 2,	// add hbp
			.vdisplay = 2400,
			.vsync_start = 2400 + 8,	// add vfp
			.vsync_end = 2400 + 8 + 1,	// add vsa
			.vtotal = 2400 + 8 + 1 + 15,	// add vbp
			.flags = 0,
			.width_mm = 69,
			.height_mm = 154,
		},
		.exynos_mode = {
			.mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS,
			.bpc = 8,
			.dsc = {
				.enabled = true,
				.dsc_count = 1,
				.slice_count = 2,
				.slice_height = 40,
			},
		},
	},
};

static const struct exynos_panel_mode s6e3fc3_lp_mode[] = {
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78974,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0)},
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 1, 2, 40)},
	},
};

static const struct drm_panel_funcs s6e3fc3_drm_funcs = {
	.disable = s6e3fc3_disable,
	.unprepare = s6e3fc3_unprepare,
	.prepare = s6e3fc3_prepare,
	.enable = s6e3fc3_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs s6e3fc3_exynos_funcs = {
	.set_brightness = s6e3fc3_set_brightness,
	.set_lp_mode = s6e3fc3_lp_mode_set,
	.mode_set = s6e3fc3_mode_set,
};

const struct exynos_panel_desc samsung_s6e3fc3 = {
	.data_lane_cnt = 4,
	.max_brightness = 1023, /* TODO check */
	.dft_brightness = 511, /* TODO check */
	.modes = s6e3fc3_mode,
	.num_modes = ARRAY_SIZE(s6e3fc3_mode),
	.lp_modes = s6e3fc3_lp_mode,
	.num_lp_modes = ARRAY_SIZE(s6e3fc3_lp_mode),
	.panel_func = &s6e3fc3_drm_funcs,
	.exynos_panel_func = &s6e3fc3_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,s6e3fc3", .data = &samsung_s6e3fc3 },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e3fc3",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Wonyoeng choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung s6e3fc3 panel driver");
MODULE_LICENSE("GPL");
