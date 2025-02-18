/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GH1_SET_A_H
#define IS_CIS_GH1_SET_A_H
#include "is-cis.h"
#include "is-cis-gh1.h"

const u32 sensor_gh1_setfile_A_Global_Secure[] ={
};

const u32 sensor_gh1_setfile_A_Global_Secure_Pd_On[] ={
};

const u32 sensor_gh1_setfile_A_Global[] = {
};

const u32 sensor_gh1_setfile_A_7296x5472_15fps[] = {
};

const u32 sensor_gh1_setfile_A_3648x2736_30fps[] = {
};

const u32 sensor_gh1_setfile_A_3968x2232_30fps[] = {
};

const u32 sensor_gh1_setfile_A_3968x2232_60fps[] = {
};

const u32 sensor_gh1_setfile_A_1984x1116_240fps[] = {
};

const u32 sensor_gh1_setfile_A_1824x1368_30fps[] = {
};

const u32 sensor_gh1_setfile_A_1984x1116_30fps[] = {
};

const u32 sensor_gh1_setfile_A_2944x2208_30fps[] = {
};

const u32 sensor_gh1_setfile_A_3216x1808_30fps[] = {
};

const u32 sensor_gh1_setfile_A_912x684_120fps[] = {
};

const u32 sensor_gh1_setfile_A_3968x2232_120fps[] = {
};

const u32 sensor_gh1_setfile_A_3216x2208_30fps[] = {
};

const u32 sensor_gh1_setfile_A_3648x2736_30fps_short[] = {
};

const u32 sensor_gh1_setfile_A_3968x2232_30fps_short[] = {
};

const u32 sensor_gh1_setfile_A_3648x2736_30fps_secure[] = {
};

const u32 sensor_gh1_setfile_A_3648x2736_60fps[] = { 
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_7296x5472_15fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3648x2736_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3968x2232_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3968x2232_60fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_1984x1116_240fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_1824x1368_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_1984x1116_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_2944x2208_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3216x1808_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_912x684_120fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3968x2232_120fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3216x2208_30fps = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3648x2736_30fps_secure = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3648x2736_30fps_short = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3968x2232_30fps_short = {
};

const struct sensor_pll_info_compact sensor_gh1_pllinfo_A_3648x2736_60fps = {
};

static const u32 *sensor_gh1_setfiles_A[] = {
};

static const u32 sensor_gh1_setfile_A_sizes[] = {
};

static const struct sensor_pll_info_compact *sensor_gh1_pllinfos_A[] = {
};


static const u32 sensor_gh1_setfile_A_mipi_mode135678_1299p2_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode135678_1331p2_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode135678_1395p2_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode135678_1452p8_mhz[] = {
};

static const struct cam_mipi_setting sensor_gh1_setfile_A_mipi_setting_mode135678[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode2_1395p2_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode2_1459p2_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode2_1465p6_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode2_1452p8_mhz[] = {
};

static const struct cam_mipi_setting sensor_gh1_setfile_A_mipi_setting_mode2[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode4_1516p8_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode4_1510p4_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode4_1504_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode4_1497p6_mhz[] = {
};

static const struct cam_mipi_setting sensor_gh1_setfile_A_mipi_setting_mode4[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode9_2124p8_mhz[] = {
};

static const u32 sensor_gh1_setfile_A_mipi_mode9_2195p2_mhz[] = {
};

static const struct cam_mipi_setting sensor_gh1_setfile_A_mipi_setting_mode9[] = {
};

/* must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel */
static const struct cam_mipi_channel sensor_gh1_setfile_A_mipi_channel_mode135678[] = {
};

/* must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel */
static const struct cam_mipi_channel sensor_gh1_setfile_A_mipi_channel_mode2[] = {
};

/* must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel */
static const struct cam_mipi_channel sensor_gh1_setfile_A_mipi_channel_mode4[] = {
};

/* must be sorted. if not, trigger panic in fimc_is_vendor_verify_mipi_channel */
static const struct cam_mipi_channel sensor_gh1_setfile_A_mipi_channel_mode9[] = {
};

static const struct cam_mipi_sensor_mode sensor_gh1_setfile_A_mipi_sensor_mode[] = {
};

/* structure for only verifying channel list. to prevent redundant checking */
const int sensor_gh1_setfile_A_verify_sensor_mode[] = {
};

#endif
