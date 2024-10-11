// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3FAB Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>

#include "panel-samsung-drv.h"

static unsigned char SEQ_PPS_SLICE[] = {
	/* 1080x2400, Slice Info : 540x120 */
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00, 0x78, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x0B, 0xAF,
	0x00, 0x07, 0x00, 0x0C, 0x00, 0xCF, 0x00, 0xD9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
};

static int s6e3fab_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);
	ctx->enabled = false;
	panel_info(ctx, "+\n");
	return 0;
}

static int s6e3fab_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_info(ctx, "+\n");
	exynos_panel_set_power(ctx, false);
	panel_info(ctx, "-\n");
	return 0;
}

static int s6e3fab_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_info(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	panel_info(ctx, "-\n");

	return 0;
}

static int s6e3fab_set_brightness(struct exynos_panel *exynos_panel, u16 br)
{
	u16 brightness;

	brightness = (br & 0xff) << 8 | br >> 8;
	return exynos_dcs_set_brightness(exynos_panel, brightness);
}

static void s6e3fab_set_vrefresh(struct exynos_panel *ctx,
				const struct drm_display_mode *pmode)
{
	int vrefresh = drm_mode_vrefresh(pmode);

	panel_info(ctx, "+\n");

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF0, 0x5A, 0x5A);

#if defined(CONFIG_EXYNOS_CMD_SVSYNC)
	if (vrefresh == 120) {
		/* TE start timing is advanced by about 3msec due to SV-Sync
		 *	default value : 2399(active line) + 15(vbp+1) - 2 = 0x96C
		 *	modified value : default value - 900(modifying line) = 0x5E8
		 */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x01, 0x50, 0xE8, 0x09);
	} else {
		/* TE start timing is advanced by about 3msec due to SV-Sync
		 *	default value : 2399(active line) + 15(vbp+1) - 2 = 0x96C
		 *	modified value : default value - 450(modifying line) = 0x7AA
		 */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x01, 0x70, 0xAA, 0x09);
	}
#endif

	if (vrefresh == 60) {
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0xA5, 0xA5);
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x60, 0x00);
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0x5A, 0x5A);
	} else if (vrefresh == 120) {
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0xA5, 0xA5);
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x60, 0x20);
		EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0x5A, 0x5A);
	} else if (vrefresh == 30) {
		// lp mode
	} else {
		panel_err(ctx, "not supported fps(%d)\n", vrefresh);
		goto end;
	}

	/* Panelupdate : gamma set, ltps set, transition control update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF7, 0x0F);

end:
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF0, 0xA5, 0xA5);

	panel_info(ctx, "-\n");
}

static void s6e3fab_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	panel_info(ctx, "+\n");

	if (!ctx->enabled)
		return;

	if (SEAMLESS_MODESET_VREF & flags)
		s6e3fab_set_vrefresh(ctx, &pmode->mode);

	panel_info(ctx, "-\n");
}

static void s6e3fab_lp_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}

static int s6e3fab_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;

	panel_info(ctx, "+\n");
	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		return -EINVAL;
	}

	exynos_panel_reset(ctx);

	/* DSC related configuration */
	exynos_dcs_compression_mode(ctx, true);
	EXYNOS_PPS_LONG_WRITE_TABLE(ctx, SEQ_PPS_SLICE);

	/* sleep out: 200ms delay */
	EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, 10, 0x11);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0x5a, 0x5a);

	/* CASET */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x2a, 0x00, 0x00, 0x04, 0x37);
	/* PASET */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x2b, 0x00, 0x00, 0x09, 0x5f);

	/* ESD flag: [2]=VLIN3, [6]=VLIN1 error check*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xED, 0x04, 0x40, 0x20);

	/* FAB_FFC_1362 */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xC5, 0x09, 0x10, 0x68, 0x22, 0x02, 0x22, 0x02);

	/* enable brightness control */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x53, 0x28);

	/* ACL on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x55, 0x01);

	/* Default brightness aor40% */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x51, 0x02, 0xED);

	/* Panelupdate : gamma set, ltps set, transition control update */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF7, 0x0F);

	/* TE on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x35);

	/* TSP HSYNC */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
			0x00, 0x11, 0x03);

	/* vrefresh rate configuration */
	s6e3fab_set_vrefresh(ctx, &pmode->mode);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0xa5, 0xa5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0xa5, 0xa5);

	/* display on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0xA5, 0xA5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x29);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0x5A, 0x5A);
	ctx->enabled = true;

	if (pmode->exynos_mode.is_lp_mode)
		s6e3fab_lp_mode_set(ctx, pmode);

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
static const struct exynos_panel_mode s6e3fab_mode[] = {
	{
		.mode = { DRM_MODE("1080x2400@60", DRM_MODE_TYPE_DRIVER, 157057,
				1080, 1081, 1082, 1083, 0, 2400, 2415, 2416,
				2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 120)},
	},
	{
		.mode = { DRM_MODE("1080x2400@120", DRM_MODE_TYPE_DRIVER, 314113,
				1080, 1081, 1082, 1083, 0, 2400, 2415, 2416,
				2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 120)},
	}
};

static const struct exynos_panel_mode s6e3fab_lp_mode[] = {
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78528,
				1080, 1081, 1082, 1083, 0, 2400, 2415, 2416,
				2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 2, 2, 120)},
	},
};

static const struct drm_panel_funcs s6e3fab_drm_funcs = {
	.disable = s6e3fab_disable,
	.unprepare = s6e3fab_unprepare,
	.prepare = s6e3fab_prepare,
	.enable = s6e3fab_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs s6e3fab_exynos_funcs = {
	.set_brightness = s6e3fab_set_brightness,
	.set_lp_mode = s6e3fab_lp_mode_set,
	.mode_set = s6e3fab_mode_set,
};

const struct exynos_panel_desc samsung_s6e3fab = {
	.data_lane_cnt = 4,
	.max_brightness = 1023, /* TODO check */
	.dft_brightness = 511, /* TODO check */
	.modes = s6e3fab_mode,
	.num_modes = ARRAY_SIZE(s6e3fab_mode),
	.lp_modes = s6e3fab_lp_mode,
	.num_lp_modes = ARRAY_SIZE(s6e3fab_lp_mode),
	.panel_func = &s6e3fab_drm_funcs,
	.exynos_panel_func = &s6e3fab_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,s6e3fab", .data = &samsung_s6e3fab },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e3fab",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Wonyoeng choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung s6e3fab panel driver");
MODULE_LICENSE("GPL");
