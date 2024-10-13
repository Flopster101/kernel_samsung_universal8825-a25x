/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GD1_TELE_H
#define IS_CIS_GD1_TELE_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GD1_TELE_MAX_WIDTH		(6528)
#define SENSOR_GD1_TELE_MAX_HEIGHT		(4896)

/* TODO: Check below values are valid */
#define SENSOR_GD1_TELE_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_GD1_TELE_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_GD1_TELE_COARSE_INTEGRATION_TIME_MIN              0x6 /* TODO */
#define SENSOR_GD1_TELE_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x6 /* TODO */
#define SENSOR_GD1_TELE_W_DIR_PAGE		(0x6028)
#define SENSOR_GD1_TELE_ABS_GAIN_PAGE	(0x4000)
#define SENSOR_GD1_TELE_ABS_GAIN_OFFSET	(0x0D82)

#define SENSOR_GD1_TELE_MIN_ANALOG_GAIN		(0x0020)
#define SENSOR_GD1_TELE_MAX_ANALOG_GAIN		(0x0200)
#define SENSOR_GD1_TELE_MIN_DIGITAL_GAIN	(0x0100)
#define SENSOR_GD1_TELE_MAX_DIGITAL_GAIN	(0x8000)

#define USE_GROUP_PARAM_HOLD	(0)

static const struct sensor_crop_xy_info sensor_gd1_tele_crop_xy_A[] = {
	{0, 0},
	{0, 0},
	{0, 0},
};

#endif

