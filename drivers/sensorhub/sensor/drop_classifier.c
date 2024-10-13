/*
 *  Copyright (C) 2023, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../utility/shub_utility.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "drop_classifier.h"


void print_drop_classifier_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_DROP_CLASSIFIER);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct drop_classifier_event *sensor_value = (struct drop_classifier_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d, %d / (%lld) (%ums, %dms)", sensor->name,
		  SENSOR_TYPE_DROP_CLASSIFIER, sensor_value->drop_result, sensor_value->height, (int)sensor_value->acc_x, (int)sensor_value->acc_y,
		  (int)sensor_value->acc_z, (int)sensor_value->sns_ts_diff_ms, (int)sensor_value->rt_ts_diff_ms, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

static struct sensor_funcs drop_classifier_sensor_func = {
	.print_debug = print_drop_classifier_debug,
};

int init_drop_classifier(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_DROP_CLASSIFIER);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "drop_classifier", 33, 25, sizeof(struct drop_classifier_event));
		sensor->funcs = &drop_classifier_sensor_func;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
