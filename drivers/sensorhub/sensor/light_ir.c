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

#include <linux/of_gpio.h>
#include <linux/slab.h>

#include "light_ir.h"
#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../comm/shub_iio.h"
#include "../utility/shub_utility.h"

static int light_ir_log_cnt;

static int enable_light_ir(void)
{
	light_ir_log_cnt = 0;
	return 0;
}

static void report_event_light_ir(void)
{
	struct light_ir_event *sensor_value =
	    (struct light_ir_event *)(get_sensor_event(SENSOR_TYPE_LIGHT_CCT)->value);

	if (light_ir_log_cnt < 3) {
		shub_info("Light ir Sensor : ir=%u white=%u atime=%d again=%d",
					sensor_value->irData, sensor_value->lightWhite,
					sensor_value->irAtime, sensor_value->irAgain);
		light_ir_log_cnt++;
	}
}

void print_light_ir_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_IR);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct light_ir_event *sensor_value = (struct light_ir_event *)(event->value);

	shub_info("%s(%u) : %u, %u (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_LIGHT_IR, sensor_value->irData,
		  sensor_value->lightWhite, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

static struct sensor_funcs light_ir_sensor_funcs = {
	.enable = enable_light_ir,
	.report_event = report_event_light_ir,
	.print_debug = print_light_ir_debug,
};

int init_light_ir(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_IR);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "light_ir_sensor", 24, 24, sizeof(struct light_ir_event));
		sensor->funcs = &light_ir_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
