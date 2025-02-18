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

#ifndef IS_CIS_GW1_V2_H
#define IS_CIS_GW1_V2_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GW1_MAX_WIDTH		(9216)
#define SENSOR_GW1_MAX_HEIGHT		(6912)

/* TODO: Check below values are valid */
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MIN              0x6 /* TODO */
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x6 /* TODO */
#define SENSOR_GW1_W_DIR_PAGE		(0x6028)
#define SENSOR_GW1_ABS_GAIN_PAGE	(0x4000)
#define SENSOR_GW1_ABS_GAIN_OFFSET	(0x0D82)

#define SENSOR_GW1_V2_MIN_ANALOG_GAIN	(0x0020)
#define SENSOR_GW1_V2_MAX_ANALOG_GAIN	(0x0400)
#define SENSOR_GW1_V2_MIN_DIGITAL_GAIN	(0x0100)
#define SENSOR_GW1_V2_MAX_DIGITAL_GAIN	(0x8000)

#define USE_GROUP_PARAM_HOLD	(0)

static const u32 sensor_gw1_cis_dual_master_settings[] = {
	0x6028, 0x4000, 2,
	0x0A70, 0x0001, 2, // sync enable
	0x0A72, 0x0000, 2,
	0x0A72, 0x0100, 2, // master mode
};

static const u32 sensor_gw1_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_master_settings);

static const u32 sensor_gw1_cis_dual_slave_settings[] = {
	0x6028, 0x4000, 2,
	0x0A70, 0x0001, 2, // sync enable
	0x0A72, 0x0000, 2,
	0x0A72, 0x0001, 2, // slave mode
	0x0A76, 0x0001, 2, // resync
};

static const u32 sensor_gw1_cis_dual_slave_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_slave_settings);

static const u32 sensor_gw1_cis_dual_standalone_settings[] = {
	0x6028, 0x4000, 2,
	0x0A70, 0x0001, 2, // sync enable
	0x0A72, 0x0000, 2, // standalone mode
};

static const u32 sensor_gw1_cis_dual_standalone_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_standalone_settings);

#endif
