// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3HAB Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>

#include "panel-samsung-drv.h"

static unsigned char PPS_TABLE[][88] = {
	{
		/* PPS MODE0 : 1440x3200, Slice Info : 720x40 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
		0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
		0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
		0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
	{
		/* PPS MODE1 : 1080x2400, Slice Info : 540x40 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
		0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
		0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
		0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
	{
		/* PPS MODE2 : 720x1600, Slice Info : 360x80 */
		0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x06, 0x40,
		0x02, 0xD0, 0x00, 0x50, 0x01, 0x68, 0x01, 0x68,
		0x02, 0x00, 0x01, 0xB4, 0x00, 0x20, 0x06, 0x2F,
		0x00, 0x05, 0x00, 0x0C, 0x01, 0x38, 0x01, 0xE9,
		0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
		0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
		0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
		0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
		0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
		0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4
	},
};

static unsigned char SCALER_TABLE[][2] = {
	/* scaler off, 1440x3200 */
	{0xBA, 0x01},
	/* 1.78x scaler on, 1080x2400 */
	{0xBA, 0x02},
	/* 4x scaler on, 720x1600 */
	{0xBA, 0x00},
};

static unsigned char CASET_TABLE[][5] = {
	/* scaler off, 1440x3200 */
	{0x2A, 0x00, 0x00, 0x05, 0x9F},
	/* 1.78x scaler on, 1080x2400 */
	{0x2A, 0x00, 0x00, 0x04, 0x37},
	/* 4x scaler on, 720x1600 */
	{0x2A, 0x00, 0x00, 0x02, 0xCF},
};

static unsigned char PASET_TABLE[][5] = {
	/* scaler off, 1440x3200 */
	{0x2B, 0x00, 0x00, 0x0C, 0x7F},
	/* 1.78x scaler on, 1080x2400 */
	{0x2B, 0x00, 0x00, 0x09, 0x5F},
	/* 4x scaler on, 720x1600 */
	{0x2B, 0x00, 0x00, 0x06, 0x3F},
};

static u32 s6e3hab_find_table_index(const struct exynos_panel *ctx, u32 yres)
{
	u32 i, val;
	u32 mres_count = ARRAY_SIZE(PPS_TABLE);

	for (i = 0; i < mres_count ; i++) {
		val = (PPS_TABLE[i][6] << 8) | (PPS_TABLE[i][7] << 0);
		if (yres == val)
			return i;
	}

	panel_err(ctx, "no match for yres(%d) -> forcing FHD+ mode\n", yres);
	/* FHD+ mode */
	return 1;
}

static int s6e3hab_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);
	ctx->enabled = false;
	panel_info(ctx, "+\n");
	return 0;
}

static int s6e3hab_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_info(ctx, "+\n");
	exynos_panel_set_power(ctx, false);
	panel_info(ctx, "-\n");
	return 0;
}

static int s6e3hab_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;

	ctx = container_of(panel, struct exynos_panel, panel);

	panel_info(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	panel_info(ctx, "-\n");

	return 0;
}

static int s6e3hab_set_brightness(struct exynos_panel *exynos_panel, u16 br)
{
	u16 brightness;

	brightness = (br & 0xff) << 8 | br >> 8;
	return exynos_dcs_set_brightness(exynos_panel, brightness);
}

static void s6e3hab_set_resolution(struct exynos_panel *ctx,
				const struct exynos_panel_mode *pmode)
{
	const struct exynos_display_mode *exynos_mode = &pmode->exynos_mode;
	u32 tab_idx = s6e3hab_find_table_index(ctx, pmode->mode.vdisplay);

	panel_info(ctx, "+\n");

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0xA5, 0xA5);
	/* DSC related configuration */
	if (exynos_mode->dsc.enabled) {
		exynos_dcs_compression_mode(ctx, true);
		EXYNOS_PPS_LONG_WRITE_TABLE(ctx, PPS_TABLE[tab_idx]);
	} else {
		exynos_dcs_compression_mode(ctx, false);
	}
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0x5A, 0x5A);

	/* partial update configuration */
	EXYNOS_DCS_WRITE_TABLE(ctx, CASET_TABLE[tab_idx]);
	EXYNOS_DCS_WRITE_TABLE(ctx, PASET_TABLE[tab_idx]);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF0, 0x5A, 0x5A);
	/* DDI scaling configuration */
	EXYNOS_DCS_WRITE_TABLE(ctx, SCALER_TABLE[tab_idx]);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF0, 0xA5, 0xA5);

	panel_info(ctx, "-\n");
}

static void s6e3hab_set_vrefresh(struct exynos_panel *ctx,
				const struct drm_display_mode *pmode)
{
	int vrefresh = drm_mode_vrefresh(pmode);

	panel_info(ctx, "+\n");

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xF0, 0x5A, 0x5A);

#if defined(CONFIG_EXYNOS_CMD_SVSYNC)
	if (vrefresh == 120) {
		/* TE start timing is advanced by about 3msec due to SV-Sync
		 *	default value : 3199(active line) + 15(vbp+1) - 2 = 0xC8C
		 *	modified value : default value - 1200(modifying line) = 0x7DC
		 */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x01, 0x70, 0xDC, 0x09);
	} else {
		/* TE start timing is advanced by about 3msec due to SV-Sync
		 *	default value : 3199(active line) + 15(vbp+1) - 2 = 0xC8C
		 *	modified value : default value - 600(modifying line) = 0xA34
		 */
		EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x01, 0xA0, 0x34, 0x09);
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

static void s6e3hab_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	panel_info(ctx, "+\n");

	if (!ctx->enabled)
		return;

	if (SEAMLESS_MODESET_MRES & flags)
		s6e3hab_set_resolution(ctx, pmode);
	if (SEAMLESS_MODESET_VREF & flags)
		s6e3hab_set_vrefresh(ctx, &pmode->mode);

	panel_info(ctx, "-\n");
}

static void s6e3hab_lp_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}

static int s6e3hab_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;
	const struct drm_display_mode *mode;
	u32 tab_idx;

	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		return -EINVAL;
	}
	mode = &pmode->mode;

	panel_info(ctx, "+\n");

	exynos_panel_reset(ctx);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0xf0, 0x5a, 0x5a);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xfc, 0x5a, 0x5a);

	/* DSC related configuration */
	exynos_dcs_compression_mode(ctx, true);
	EXYNOS_PPS_LONG_WRITE_TABLE(ctx, PPS_TABLE[0]);

	/* sleep out: 200ms delay */
	EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, 200, 0x11);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x00, 0xC0, 0x8C, 0x09, 0x00, 0x00,
			0x00, 0x11, 0x03);

	/* enable brightness control */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x53, 0x20);

	/* WRDISBV(51h) = 1st[7:0], 2nd[15:8] */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x51, 0xff, 0x7f);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x35); /* TE on */

	/* ESD flag: [2]=VLIN3, [6]=VLIN1 error check*/
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xED, 0x04, 0x44);

	/* Typical high duration: 123.57 (122~125us) */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0xB9, 0x00, 0xC0, 0x8C, 0x09);

	/* vrefresh rate configuration */
	s6e3hab_set_vrefresh(ctx, mode);

	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0xA5, 0xA5);
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x29); /* display on */
	EXYNOS_DCS_WRITE_SEQ(ctx, 0x9F, 0x5A, 0x5A);

	tab_idx = s6e3hab_find_table_index(ctx, mode->vdisplay);
	if (tab_idx)
		s6e3hab_set_resolution(ctx, pmode);

	ctx->enabled = true;

	if (pmode->exynos_mode.is_lp_mode)
		s6e3hab_lp_mode_set(ctx, pmode);

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
#if IS_ENABLED(CONFIG_SOC_S5E8825)
static const struct exynos_panel_mode s6e3hab_mode[] = {
	{
		.mode = { DRM_MODE("1080x2400@60", DRM_MODE_TYPE_DRIVER, 157948,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 1, 2, 40)},
	},
	{
		.mode = { DRM_MODE("1080x2400@120", DRM_MODE_TYPE_DRIVER, 315896,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 1, 2, 40)},
	},
};

static const struct exynos_panel_mode s6e3hab_lp_mode[] = {
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78974,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 1, 2, 40)},
	},
};
#else
static const struct exynos_panel_mode s6e3hab_mode[] = {
	{
		.mode = { DRM_MODE("1440x3200@60", DRM_MODE_TYPE_DRIVER, 279714,
				1440, 1442, 1444, 1446, 0, 3200, 3208, 3209,
				3224, 0, 0)},
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 40)},
	},
	{
		.mode = { DRM_MODE("1080x2400@60", DRM_MODE_TYPE_DRIVER, 157948,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 40)},
	},
	{
		.mode = { DRM_MODE("1080x2400@120", DRM_MODE_TYPE_DRIVER, 315896,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 40)},
	},
	{
		.mode = { DRM_MODE("720x1600@60", DRM_MODE_TYPE_DRIVER, 70741,
				720, 722, 724, 726, 0, 1600, 1608, 1609,
				1624, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 80)},
	},
	{
		.mode = { DRM_MODE("720x1600@120", DRM_MODE_TYPE_DRIVER, 141483,
				720, 722, 724, 726, 0, 1600, 1608, 1609,
				1624, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, false, true, 2, 2, 80)},
	},
};

static const struct exynos_panel_mode s6e3hab_lp_mode[] = {
	{
		.mode = { DRM_MODE("1440x3200@30", DRM_MODE_TYPE_DRIVER, 139857,
				1440, 1442, 1444, 1446, 0, 3200, 3208, 3209,
				3224, 0, 0)},
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 2, 2, 40)},
	},
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78974,
				1080, 1082, 1084, 1086, 0, 2400, 2408, 2409,
				2424, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 2, 2, 40)},
	},
	{
		.mode = { DRM_MODE("720x1600@30", DRM_MODE_TYPE_DRIVER, 35371,
				720, 722, 724, 726, 0, 1600, 1608, 1609,
				1624, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 2, 2, 80)},
	},
};
#endif

static const struct drm_panel_funcs s6e3hab_drm_funcs = {
	.disable = s6e3hab_disable,
	.unprepare = s6e3hab_unprepare,
	.prepare = s6e3hab_prepare,
	.enable = s6e3hab_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs s6e3hab_exynos_funcs = {
	.set_brightness = s6e3hab_set_brightness,
	.set_lp_mode = s6e3hab_lp_mode_set,
	.mode_set = s6e3hab_mode_set,
};

const struct exynos_panel_desc samsung_s6e3hab = {
	.data_lane_cnt = 4,
	.max_brightness = 1023, /* TODO check */
	.dft_brightness = 511, /* TODO check */
	.modes = s6e3hab_mode,
	.num_modes = ARRAY_SIZE(s6e3hab_mode),
	.lp_modes = s6e3hab_lp_mode,
	.num_lp_modes = ARRAY_SIZE(s6e3hab_lp_mode),
	.panel_func = &s6e3hab_drm_funcs,
	.exynos_panel_func = &s6e3hab_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,s6e3hab", .data = &samsung_s6e3hab },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.driver = {
		.name = "panel-samsung-s6e3hab",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Wonyoeng choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung s6e3hab panel driver");
MODULE_LICENSE("GPL");
