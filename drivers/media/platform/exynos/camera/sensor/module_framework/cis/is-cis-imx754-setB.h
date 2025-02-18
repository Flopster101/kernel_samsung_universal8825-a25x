/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_IMX754_SET_B_H
#define IS_CIS_IMX754_SET_B_H

#include "is-cis.h"
#include "is-cis-imx754.h"

/* IMX754 sensor setting version - Temp */
/* IMX754 MIPI Table version - Temp */
/* Global setting */
/* sensor_imx754_setfile_B_Global */

const u32 sensor_imx754_setfile_B_Global[] = {
};

const u32 sensor_imx754_setfile_B_4000x3000_60fps[] = {
};

const u32 sensor_imx754_setfile_B_3648x2736_60fps[] = {
};

const u32 sensor_imx754_setfile_B_3840x2160_60fps[] = {
};

const u32 sensor_imx754_setfile_B_4000x3000_30fps[] = {
};

const u32 sensor_imx754_setfile_B_3648x2736_30fps[] = {
};

const u32 sensor_imx754_setfile_B_3840x2160_30fps[] = {
};

const u32 sensor_imx754_setfile_B_1920x1080_120fps[] = {
};

const u32 sensor_imx754_setfile_B_1920x1080_240fps[] = {
};

/* dual sync slave */
const u32 sensor_imx754_dual_slave_settings[] = {
	// Not Yet
};

/* dual sync single */
const u32 sensor_imx754_dual_single_settings[] = {
	// Not Yet
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_4000x3000_60fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_3648x2736_60fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_3840x2160_60fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_4000x3000_30fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_3648x2736_30fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_3840x2160_30fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_1920x1080_120fps = {
};

const struct sensor_pll_info_compact sensor_imx754_pllinfo_B_1920x1080_240fps = {
};

static const u32 *sensor_imx754_setfiles_B[] = {
};

static const u32 sensor_imx754_setfile_B_sizes[] = {
};

static const struct sensor_pll_info_compact *sensor_imx754_pllinfos_B[] = {
};

/* merge into sensor driver */

static const u32 sensor_imx754_setfile_B_mipi_full_1660p8_mhz[] = {
};

static const u32 sensor_imx754_setfile_B_mipi_full_1670p4_mhz[] = {
};

static const u32 sensor_imx754_setfile_B_mipi_full_1708p8_mhz[] = {
};

static const struct cam_mipi_setting sensor_imx754_setfile_B_mipi_setting_full[] = {
};

static const u32 sensor_imx754_setfile_B_mipi_2bin240_1900p8_mhz[] = {
};

static const u32 sensor_imx54_setfile_B_mipi_2bin240_1910p4_mhz[] = {
};

static const u32 sensor_imx754_setfile_B_mipi_2bin240_1968_mhz[] = {
};

static const struct cam_mipi_setting sensor_imx754_setfile_B_mipi_setting_2bin240[] = {
};

// must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel
static const struct cam_mipi_channel sensor_imx754_setfile_B_mipi_channel_full[] = {
};

static const struct cam_mipi_channel sensor_imx754_setfile_B_mipi_channel_BINNING[] = {
	// Not Yet
};

static const struct cam_mipi_channel sensor_imx754_setfile_B_mipi_channel_VISION[] = {
	// Not Yet
};

static const struct cam_mipi_channel sensor_imx754_setfile_B_mipi_channel_2bin240[] = {
};

static const struct cam_mipi_sensor_mode sensor_imx754_setfile_B_mipi_sensor_mode[] = {
};

// structure for only verifying channel list. to prevent redundant checking
const int sensor_imx754_setfile_B_verify_sensor_mode[] = {
};
#endif
