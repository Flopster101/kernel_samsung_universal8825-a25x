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

#include <linux/slab.h>

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

struct step_counter_event {
	u32 step_diff;
	u64 step_total;
} __attribute__((__packed__));

static void report_event_step_counter(void)
{
	struct step_counter_event *sensor_value =
	    (struct step_counter_event *)(get_sensor_event(SENSOR_TYPE_STEP_COUNTER)->value);

	sensor_value->step_total += sensor_value->step_diff;
}

void print_step_counter_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_COUNTER);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct step_counter_event *sensor_value = (struct step_counter_event *)(event->value);

	shub_info("%s(%u) : %lld %u(%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_STEP_COUNTER,
		  sensor_value->step_total, sensor_value->step_diff, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}


static struct sensor_funcs step_cnt_sensor_funcs = {
	.report_event = report_event_step_counter,
	.print_debug = print_step_counter_debug,
};

int init_step_counter(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_COUNTER);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "step_cnt_sensor", 4, 12, sizeof(struct step_counter_event));
		sensor->funcs = &step_cnt_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_step_detector(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_DETECTOR);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "step_det_sensor", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_sequential_step(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SEQUENTIAL_STEP);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "sequential_step", 4, 4, 4);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}