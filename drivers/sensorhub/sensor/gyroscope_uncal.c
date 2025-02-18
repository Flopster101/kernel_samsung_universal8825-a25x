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
#include "../sensor/gyroscope.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

void print_gyroscope_uncal_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE_UNCALIBRATED);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct uncal_gyro_event *sensor_value = (struct uncal_gyro_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name,
		  SENSOR_TYPE_GYROSCOPE_UNCALIBRATED, sensor_value->uncal_x, sensor_value->uncal_y, sensor_value->uncal_z,
		  sensor_value->offset_x, sensor_value->offset_y, sensor_value->offset_z, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

static struct sensor_funcs gyroscope_uncal_sensor_funcs = {
	.print_debug = print_gyroscope_uncal_debug,
};
		
int init_gyroscope_uncal(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE_UNCALIBRATED);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "uncal_gyro_sensor", 12, 12, sizeof(struct uncal_gyro_event));

		sensor->report_mode_continuous = true;
		sensor->funcs = &gyroscope_uncal_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
