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

#include "../sensor/proximity.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"

#define PROX_AVG_READ_NUM       80

unsigned int praw_num;
unsigned int praw_sum;
unsigned int praw_min;
unsigned int praw_max;
unsigned int praw_avg;

void report_event_proximity_raw(void)
{
	struct prox_raw_event *sensor_value =
	    (struct prox_raw_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_RAW)->value);
	struct proximity_raw_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_RAW)->data;

	if (praw_num++ >= PROX_AVG_READ_NUM) {
		data->avg_data[PROX_RAW_AVG] = praw_sum / PROX_AVG_READ_NUM;
		data->avg_data[PROX_RAW_MIN] = praw_min;
		data->avg_data[PROX_RAW_MAX] = praw_max;

		praw_num = 0;
		praw_min = 0;
		praw_max = 0;
		praw_sum = 0;
	} else {
		praw_sum += sensor_value->prox_raw;

		if (praw_num == 1)
			praw_min = sensor_value->prox_raw;
		else if (sensor_value->prox_raw < praw_min)
			praw_min = sensor_value->prox_raw;

		if (sensor_value->prox_raw > praw_max)
			praw_max = sensor_value->prox_raw;
	}
}

static struct proximity_raw_data proximity_raw_data;
static struct sensor_funcs proximity_raw_sensor_funcs = {
	.report_event = report_event_proximity_raw,
};

int init_proximity_raw(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY_RAW);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "proximity_raw", 2, 0, sizeof(struct prox_raw_event));
		sensor->hal_sensor = false;
		sensor->data = (void *)&proximity_raw_data;
		sensor->funcs = &proximity_raw_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
