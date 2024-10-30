// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E8DC1 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>

#include "panel-samsung-drv.h"

static const unsigned char SEQ_ERR_FG[] = {
	0xED, 0x00, 0x01, 0x00, /* ERR_FG VLIN Monitor ON */
	0x40, 0x04, 0x08, 0xA8, /* Line Copy On */
	0x84, 0x4A, 0x73, 0x02, 0x0A, 0x00
};

static int s6e8fc1_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	if (!ctx->enabled)
		return 0;

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x28, 0x00, 0x00);
	mdelay(20);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x10, 0x00, 0x00);

	ctx->enabled = false;
	panel_debug(ctx, "+\n");
	return 0;
}

static int s6e8fc1_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, false);
	panel_debug(ctx, "-\n");
	return 0;
}

static int s6e8fc1_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	usleep_range(10000, 11000);
	exynos_panel_reset(ctx);
	panel_debug(ctx, "-\n");

	return 0;
}

static int s6e8fc1_set_brightness(struct exynos_panel *exynos_panel, u16 br)
{
	u16 brightness;

	brightness = (br & 0xff) << 8 | br >> 8;
	return exynos_dcs_set_brightness(exynos_panel, brightness);
}

static void s6e8fc1_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	panel_debug(ctx, "+\n");

	if (!ctx->enabled)
		return;

	panel_debug(ctx, "-\n");
}

static void s6e8fc1_lp_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_debug(ctxv, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}

static int s6e8fc1_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;
	const struct drm_display_mode *mode;

	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		return -EINVAL;
	}
	mode = &pmode->mode;

	panel_info(ctx, "+\n");

	EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, 20, 0x11);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_TABLE(ctx, SEQ_ERR_FG); // fail safe1 disable
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf1, 0xa5, 0xa5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x53, 0x20);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB3, 0x11, 0x50, 0xDD, 0x0D, 0xC0, 0x0A);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x51, 0x03, 0xFF);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf9, 0x20);	// fail safe2 disable
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0xa5, 0xa5);

	mdelay(100);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x29); /* display on */

	ctx->enabled = true;

	if (pmode->exynos_mode.is_lp_mode)
		s6e8fc1_lp_mode_set(ctx, pmode);

	panel_info(ctx, "-\n");

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
static const struct exynos_panel_mode s6e8fc1_mode[] = {
	{
		.mode = { DRM_MODE("720x1600@60", DRM_MODE_TYPE_DRIVER, 77375,
				   720, 746, 750, 798, 0, 1600, 1605, 1607,
				   1616, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_MODE_VIDEO, 8,
					     false, false, 0, 0, 0) },
	},
	{
		.mode = { DRM_MODE("720x1600@40", DRM_MODE_TYPE_DRIVER, 77375,
				   720, 746, 750, 798, 0, 1600, 2410, 2412,
				   2421, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_MODE_VIDEO, 8,
					     false, false, 0, 0, 0) },
	},
};

static const struct exynos_panel_mode s6e8fc1_lp_mode[] = {
	{
		.mode = { DRM_MODE("720x1600@60", DRM_MODE_TYPE_DRIVER, 38688,
				   720, 746, 750, 798, 0, 1600, 1605, 1607,
				   1616, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_MODE_VIDEO, 8, true,
					     false, 0, 0, 0) },
	},
	{
		.mode = { DRM_MODE("720x1600@40", DRM_MODE_TYPE_DRIVER, 77375,
				   720, 746, 750, 798, 0, 1600, 2410, 2412,
				   2421, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_MODE_VIDEO, 8,
					     false, false, 0, 0, 0) },
	},
};

static const struct drm_panel_funcs s6e8fc1_drm_funcs = {
	.disable = s6e8fc1_disable,
	.unprepare = s6e8fc1_unprepare,
	.prepare = s6e8fc1_prepare,
	.enable = s6e8fc1_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs s6e8fc1_exynos_funcs = {
	.set_brightness = s6e8fc1_set_brightness,
	.set_lp_mode = s6e8fc1_lp_mode_set,
	.mode_set = s6e8fc1_mode_set,
};

const struct exynos_panel_desc samsung_s6e8fc1 = {
	.data_lane_cnt = 4,
	.max_brightness = 1023,
	.dft_brightness = 511,
	.modes = s6e8fc1_mode,
	.num_modes = ARRAY_SIZE(s6e8fc1_mode),
	.lp_modes = s6e8fc1_lp_mode,
	.num_lp_modes = ARRAY_SIZE(s6e8fc1_lp_mode),
	.panel_func = &s6e8fc1_drm_funcs,
	.exynos_panel_func = &s6e8fc1_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,s6e8fc1", .data = &samsung_s6e8fc1 },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e8fc1",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Wonyoeng choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung s6e8fc1 panel driver");
MODULE_LICENSE("GPL");
