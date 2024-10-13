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

#ifndef IS_CIS_3M5FOLD_H
#define IS_CIS_3M5FOLD_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_3M5FOLD_MAX_WIDTH		(4160)
#define SENSOR_3M5FOLD_MAX_HEIGHT		(3120)

/* TODO: Check below values are valid */
#define SENSOR_3M5FOLD_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_3M5FOLD_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_3M5FOLD_COARSE_INTEGRATION_TIME_MIN              0x4 /* TODO */
#define SENSOR_3M5FOLD_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x4 /* TODO */

#define SENSOR_3M5_FOLD_MIN_ANALOG_GAIN		(0x0020)
#define SENSOR_3M5_FOLD_MAX_ANALOG_GAIN		(0x0200)
#define SENSOR_3M5_FOLD_MIN_DIGITAL_GAIN	(0x0100)
#define SENSOR_3M5_FOLD_MAX_DIGITAL_GAIN	(0x8000)

#define USE_GROUP_PARAM_HOLD	(0)

static const u32 sensor_3m5fold_cis_dual_master_settings[] = {
};

static const u32 sensor_3m5fold_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_3m5fold_cis_dual_master_settings);

static const u32 sensor_3m5fold_cis_dual_slave_settings[] = {
	0x6028, 0x4000, 2,
	0x0B30, 0x0001, 2,
	0x0B36, 0x0101, 2,
	0x6028, 0x2000, 2,
	0x2BC0, 0x0000, 2,
};

static const u32 sensor_3m5fold_cis_dual_slave_settings_size =
	ARRAY_SIZE(sensor_3m5fold_cis_dual_slave_settings);

static const u32 sensor_3m5fold_cis_dual_standalone_settings[] = {
	0x6028, 0x4000, 2,
	0x0B30, 0x0000, 2,
};

static const u32 sensor_3m5fold_cis_dual_standalone_settings_size =
	ARRAY_SIZE(sensor_3m5fold_cis_dual_standalone_settings);
#endif
