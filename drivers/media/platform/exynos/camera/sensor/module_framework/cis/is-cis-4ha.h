/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_4HA_H
#define IS_CIS_4HA_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_4HA_MAX_WIDTH		(3264 + 0)
#define SENSOR_4HA_MAX_HEIGHT		(2448 + 0)

/* TODO: Check below values are valid */
#define SENSOR_4HA_FINE_INTEGRATION_TIME_MIN                0x1C5
#define SENSOR_4HA_FINE_INTEGRATION_TIME_MAX                0x1C5
#define SENSOR_4HA_COARSE_INTEGRATION_TIME_MIN              0x4
#define SENSOR_4HA_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x6

#define SENSOR_4HA_EXPOSURE_TIME_MAX						250000 /* 250ms */

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_4ha_mode_enum {
	SENSOR_4HA_3264X2448_30FPS = 0,
	SENSOR_4HA_3264X1836_30FPS,
	SENSOR_4HA_2880X1980_30FPS,
	SENSOR_4HA_2640X1980_30FPS,
	SENSOR_4HA_2608X1956_30FPS,
	SENSOR_4HA_1632X1224_30FPS,
	SENSOR_4HA_1632X916_30FPS,
	SENSOR_4HA_800X600_120FPS,
};
#endif

