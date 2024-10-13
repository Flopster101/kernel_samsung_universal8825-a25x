/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

int init_pocket_pos_mode(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POCKET_POS_MODE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "pocket_pos_mode", 15, 15, 15);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_pocket_mode_lite(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POCKET_MODE_LITE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "pocket_mode_lite", 5, 5, 5);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_pocket_mode(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POCKET_MODE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "pocket_mode", 58, 58, 58);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
