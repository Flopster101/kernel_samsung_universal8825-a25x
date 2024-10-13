// SPDX-License-Identifier: GPL-2.0-only
/*
 * MIPI-DSI based emulator panel driver.
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>
#include "panel-samsung-drv.h"

static int emul_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);
	ctx->enabled = false;
	panel_info(ctx, "+\n");
	return 0;
}

static int emul_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, false);
	panel_debug(ctx, "-\n");
	return 0;
}

static int emul_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	panel_debug(ctx, "-\n");

	return 0;
}

static int emul_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_debug(ctx, "+\n");

	/* sleep out: 120ms delay */
	EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, 120, 0x11);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x29); /* display on */

	ctx->enabled = true;

	panel_debug(ctx, "-\n");

	return 0;
}

static const struct exynos_panel_mode emul_modes[] = {
	{
		/* 640x480 @ 60 */
		.mode = {
			.name = "640x480@60",
			/* @clock: vtotal * htotal * fps / 1000 */
			.clock = 22680,
			.hdisplay = 640,
			.hsync_start = 640 + 20,
			.hsync_end = 640 + 20 + 20,
			.htotal = 640 + 20 + 20 + 20,
			.vdisplay = 480,
			.vsync_start = 480 + 20,
			.vsync_end = 480 + 20 + 20,
			.vtotal = 480 + 20 + 20 + 20,
			.flags = 0,
			.width_mm = 80,
			.height_mm = 120,
		},
		.exynos_mode = {
			.mode_flags = MIPI_DSI_MODE_VIDEO,
			.bpc = 8,
			.dsc = {
				.enabled = false,
				.dsc_count = 2,
				.slice_count = 2,
				.slice_height = 65,
			},
		},
	},
	{
		/* 1440x3120 @ 60 */
		.mode = {
			.name = "1440x3200@60",
			.clock = 283680,
			.hdisplay = 1440,
			.hsync_start = 1440 + 32,
			.hsync_end = 1440 + 32 + 12,
			.htotal = 1440 + 32 + 12 + 16,
			.vdisplay = 3120,
			.vsync_start = 3120 + 12,
			.vsync_end = 3120 + 12 + 4,
			.vtotal = 3120 + 12 + 4 + 16,
			.flags = 0,
			.width_mm = 80,
			.height_mm = 120,
		},
		.exynos_mode = {
			.mode_flags = MIPI_DSI_MODE_VIDEO,
			.bpc = 8,
			.dsc = {
				.enabled = true,
				.dsc_count = 2,
				.slice_count = 2,
				.slice_height = 65,
			},
		},
	},
	{
		/* 1440x3120 @ 120 */
		.mode = {
			.name = "1440x3200@120",
			.clock = 567360,
			.hdisplay = 1440,
			.hsync_start = 1440 + 32,
			.hsync_end = 1440 + 32 + 12,
			.htotal = 1440 + 32 + 12 + 16,
			.vdisplay = 3120,
			.vsync_start = 3120 + 12,
			.vsync_end = 3120 + 12 + 4,
			.vtotal = 3120 + 12 + 4 + 16,
			.flags = 0,
			.width_mm = 80,
			.height_mm = 120,
		},
		.exynos_mode = {
			.mode_flags = MIPI_DSI_MODE_VIDEO,
			.bpc = 8,
			.dsc = {
				.enabled = true,
				.dsc_count = 2,
				.slice_count = 2,
				.slice_height = 65,
			},
		},
	},
};

static const struct drm_panel_funcs emul_drm_funcs = {
	.disable = emul_disable,
	.unprepare = emul_unprepare,
	.prepare = emul_prepare,
	.enable = emul_enable,
	.get_modes = exynos_panel_get_modes,
};

const struct exynos_panel_desc samsung_emul = {
	.data_lane_cnt = 4,
	.modes = emul_modes,
	.num_modes = ARRAY_SIZE(emul_modes),
	.panel_func = &emul_drm_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,emul", .data = &samsung_emul },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-emul",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Jiun Yu <jiun.yu@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung emul panel driver");
MODULE_LICENSE("GPL");
