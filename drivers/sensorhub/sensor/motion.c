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

int init_significant_motion(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SIGNIFICANT_MOTION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "sig_motion_sensor", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_tilt_detector(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_TILT_DETECTOR);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "tilt_detector", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_pick_up_gesture(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PICK_UP_GESTURE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "pickup_gesture", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_call_gesture(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_CALL_GESTURE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "call_gesture", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_wake_up_motion(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_WAKE_UP_MOTION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "wake_up_motion", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_protos_motion(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROTOS_MOTION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "protos_motion", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
