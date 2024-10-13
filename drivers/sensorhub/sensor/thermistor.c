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

struct thermistor_event {
	u8 type;
	s16 raw;
} __attribute__((__packed__));

static void print_thermistor_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_THERMISTOR);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct thermistor_event *sensor_value = (struct thermistor_event *)(event->value);

	shub_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_THERMISTOR, sensor_value->type,
		  sensor_value->raw, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}


static struct sensor_funcs thermistor_sensor_funcs = {
	.print_debug = print_thermistor_debug,
};

int init_thermistor(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_THERMISTOR);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "thermistor_sensor", 3, 3, sizeof(struct thermistor_event));
		sensor->funcs = &thermistor_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
