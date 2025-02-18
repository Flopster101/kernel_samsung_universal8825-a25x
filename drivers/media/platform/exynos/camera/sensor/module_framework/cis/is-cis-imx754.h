/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#ifndef IS_CIS_IMX754_H
#define IS_CIS_IMX754_H

#include "is-cis.h"

#define EXT_CLK_Khz (19200)

/* FIXME */
#define SENSOR_IMX754_MAX_WIDTH		(4000 + 0)
#define SENSOR_IMX754_MAX_HEIGHT		(3000 + 0)

#define SENSOR_IMX754_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_IMX754_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_IMX754_COARSE_INTEGRATION_TIME_MIN              0x1
#define SENSOR_IMX754_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x10

#define USE_GROUP_PARAM_HOLD	(0)

#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0
#define CROP_SHIFT_DELTA_ALIGN_X	2
#define CROP_SHIFT_DELTA_ALIGN_Y	4

enum sensor_imx754_mode_enum {
	SENSOR_IMX754_3648X2736_60FPS = 0,
	SENSOR_IMX754_3840X2160_60FPS = 1,
	SENSOR_IMX754_3648X2736_30FPS = 2,
	SENSOR_IMX754_3840X2160_30FPS = 3,
	SENSOR_IMX754_1920X1080_120FPS = 4,
	SENSOR_IMX754_1920X1080_240FPS = 5,
	SENSOR_IMX754_912X684_120FPS = 6,
	SENSOR_IMX754_3648X2052_60FPS = 7,
	SENSOR_IMX754_MODE_MAX
};

struct is_start_pos_info {
	u16 x_start;
	u16 y_start;
} start_pos_infos[SENSOR_IMX754_MODE_MAX];

static const u32 sensor_imx754_cis_dual_master_settings[] = {
	0x3050,	0x01,	0x01,
	0x3030,	0x01,	0x01,
	0x3053,	0x00,	0x01,
	0x3051,	0x01,	0x01,
	0x3052,	0x07,	0x01,
};

static const u32 sensor_imx754_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_imx754_cis_dual_master_settings);

#endif

