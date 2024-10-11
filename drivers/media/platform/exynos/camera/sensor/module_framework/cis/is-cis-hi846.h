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

#ifndef IS_CIS_HI846_H
#define IS_CIS_HI846_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_HI846_MAX_WIDTH		(6528)
#define SENSOR_HI846_MAX_HEIGHT		(4896)

/* TODO: Check below values are valid */
#define SENSOR_HI846_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_HI846_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_HI846_COARSE_INTEGRATION_TIME_MIN              0x6 /* TODO */
#define SENSOR_HI846_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x6 /* TODO */

#define USE_GROUP_PARAM_HOLD	(0)

static const u32 sensor_hi846_cis_dual_master_settings[] = {
};

static const u32 sensor_hi846_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_hi846_cis_dual_master_settings);

static const u32 sensor_hi846_cis_dual_slave_settings[] = {
       0x0040, 0x0201, 2,
       0x0042, 0x0301, 2,
       0x0D04, 0x0000, 2,
};

static const u32 sensor_hi846_cis_dual_slave_settings_size =
	ARRAY_SIZE(sensor_hi846_cis_dual_slave_settings);

static const u32 sensor_hi846_cis_dual_standalone_settings[] = {
	0x0040, 0x0200, 2,
};

static const u32 sensor_hi846_cis_dual_standalone_settings_size =
	ARRAY_SIZE(sensor_hi846_cis_dual_standalone_settings);

#endif

