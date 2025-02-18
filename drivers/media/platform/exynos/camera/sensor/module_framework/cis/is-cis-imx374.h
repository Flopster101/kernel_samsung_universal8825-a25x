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

#ifndef IS_CIS_IMX374_H
#define IS_CIS_IMX374_H

#include "is-cis.h"

/* FIXME */
#define SENSOR_IMX374_MAX_WIDTH		(3976 + 0)
#define SENSOR_IMX374_MAX_HEIGHT		(2736 + 0)

#define SENSOR_IMX374_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_IMX374_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_IMX374_COARSE_INTEGRATION_TIME_MIN              0x4
#define SENSOR_IMX374_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x10

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_imx374_mode_enum {
	SENSOR_IMX374_3648x2736_30FPS = 0,
	SENSOR_IMX374_3968x2232_30FPS = 1,
	SENSOR_IMX374_3968x2232_60FPS = 2,
	SENSOR_IMX374_3216x2208_30FPS = 3,
	SENSOR_IMX374_2944x2208_30FPS = 4,
	SENSOR_IMX374_3216X1808_30FPS = 5,
	SENSOR_IMX374_1988x1120_30FPS = 6,
	SENSOR_IMX374_1988x1120_120FPS = 7,
	SENSOR_IMX374_1824x1368_30FPS = 8,
	SENSOR_IMX374_1472x1104_30FPS = 9,
	SENSOR_IMX374_912x684_120FPS = 10,
	SENSOR_IMX374_MODE_MAX
};
#endif

