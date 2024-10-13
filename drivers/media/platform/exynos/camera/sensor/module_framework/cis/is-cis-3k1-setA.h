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

#ifndef IS_CIS_3K1_SET_A_H
#define IS_CIS_3K1_SET_A_H

#include "is-cis.h"
#include "is-cis-3k1.h"

/* 3K1 sensor setting version - Temp */
/* 3K1 MIPI Table version - Temp */

const u32 sensor_3k1_setfile_A_Global[] = {
};

// A1_3648x2736 2303Msps 60fps
const u32 sensor_3k1_setfile_A_3648x2736_60fps[] = {
};

// A2_3648x2052 2303Msps 60fps
const u32 sensor_3k1_setfile_A_3648x2052_60fps[] = {
};

// A3_3328x1872 2303Msps 60fps
const u32 sensor_3k1_setfile_A_3328x1872_60fps[] = {
};

// A4_2800x2100 2303Msps 31fps
const u32 sensor_3k1_setfile_A_2800x2100_30fps[] = {
};

// A5_1824x1368 2303Msps 120fps
const u32 sensor_3k1_setfile_A_1824x1368_120fps[] = {
};

// A6_1824x1368 2303Msps 30fps
const u32 sensor_3k1_setfile_A_1824x1368_30fps[] = {
};

// A7_3840x2880 2303Msps 60fps
const u32 sensor_3k1_setfile_A_3840x2880_60fps[] = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_3648x2736_60fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_3648x2052_60fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_3328x1872_60fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_2800x2100_30fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_1824x1368_120fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_1824x1368_30fps = {
};

const struct sensor_pll_info_compact sensor_3k1_pllinfo_A_3840x2880_60fps = {
};

static const u32 *sensor_3k1_setfiles_A[] = {
	sensor_3k1_setfile_A_3648x2736_60fps,
	sensor_3k1_setfile_A_3648x2052_60fps,
	sensor_3k1_setfile_A_3328x1872_60fps,
	sensor_3k1_setfile_A_2800x2100_30fps,
	sensor_3k1_setfile_A_1824x1368_120fps,
	sensor_3k1_setfile_A_1824x1368_30fps,
	sensor_3k1_setfile_A_3840x2880_60fps,
};

static const u32 sensor_3k1_setfile_A_sizes[] = {
	ARRAY_SIZE(sensor_3k1_setfile_A_3648x2736_60fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_3648x2052_60fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_3328x1872_60fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_2800x2100_30fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_1824x1368_120fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_1824x1368_30fps),
	ARRAY_SIZE(sensor_3k1_setfile_A_3840x2880_60fps),
};

static const struct sensor_pll_info_compact *sensor_3k1_pllinfos_A[] = {
	&sensor_3k1_pllinfo_A_3648x2736_60fps,
	&sensor_3k1_pllinfo_A_3648x2052_60fps,
	&sensor_3k1_pllinfo_A_3328x1872_60fps,
	&sensor_3k1_pllinfo_A_2800x2100_30fps,
	&sensor_3k1_pllinfo_A_1824x1368_120fps,
	&sensor_3k1_pllinfo_A_1824x1368_30fps,
	&sensor_3k1_pllinfo_A_3840x2880_60fps,
};

/* merge into sensor driver */

static const u32 sensor_3k1_setfile_A_mipi_all_1152_mhz[] = {
};

static const u32 sensor_3k1_setfile_A_mipi_all_1196p8_mhz[] = {
};

static const u32 sensor_3k1_setfile_A_mipi_all_1139p2_mhz[] = {
};

static const struct cam_mipi_setting sensor_3k1_setfile_A_mipi_setting_all[] = {
};

// must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel
static const struct cam_mipi_channel sensor_3k1_setfile_A_mipi_channel_all[] = {
};

static const struct cam_mipi_sensor_mode sensor_3k1_setfile_A_mipi_sensor_mode[] = {
};

/* structure for only verifying channel list. to prevent redundant checking */
const int sensor_3k1_setfile_A_verify_sensor_mode[] = {
};
#endif
