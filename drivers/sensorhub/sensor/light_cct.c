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
#include "light.h"

#include <linux/slab.h>

static int light_cct_log_cnt;

static int enable_light_cct(void)
{
	light_cct_log_cnt = 0;

	return 0;
}

static void report_event_light_cct(void)
{
	struct light_cct_event *sensor_value =
	    (struct light_cct_event *)(get_sensor_event(SENSOR_TYPE_LIGHT_CCT)->value);

	if (light_cct_log_cnt < 3) {
		shub_info("Light cct Sensor : lux=%u r=%d g=%d b=%d c=%d atime=%d again=%d", sensor_value->lux,
			  sensor_value->r, sensor_value->g, sensor_value->b, sensor_value->w, sensor_value->a_time,
			  sensor_value->a_gain);

		light_cct_log_cnt++;
	}
}

int get_light_cct_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;
	struct light_cct_event *sensor_value = (struct light_cct_event *)event->value;

	memcpy(&sensor_value->lux, dataframe + *index, sizeof(sensor_value->lux));
	*index += sizeof(sensor_value->lux);
	memcpy(&sensor_value->cct, dataframe + *index, sizeof(sensor_value->cct));
	*index += sizeof(sensor_value->cct);
	memcpy(&sensor_value->raw_lux, dataframe + *index, sizeof(sensor_value->raw_lux));
	*index += sizeof(sensor_value->raw_lux);

	if (data->ddi_support) {
		memcpy(&sensor_value->roi, dataframe + *index, sizeof(sensor_value->roi));
		*index += sizeof(sensor_value->roi);
	}

	memcpy(&sensor_value->r, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->g, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->b, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->w, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;

	memcpy(&sensor_value->a_time, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->a_gain, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;

	return 0;
}

void print_light_cct_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_CCT);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct light_cct_event *sensor_value = (struct light_cct_event *)(event->value);

	shub_info("%s(%u) : %u, %u, %u (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_LIGHT_CCT, sensor_value->lux,
		  sensor_value->cct, sensor_value->raw_lux, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

static struct sensor_funcs light_cct_sensor_funcs = {
	.enable = enable_light_cct,
	.report_event = report_event_light_cct,
	.print_debug = print_light_cct_debug,
	.get_sensor_value = get_light_cct_sensor_value,
};
		
int init_light_cct(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_CCT);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "light_cct_sensor", 
					sensor->spec.version >= LIGHT_DEBIG_EVENT_SIZE_4BYTE_VERSION ? 36 : 24, 14, sizeof(struct light_cct_event));

		sensor->funcs = &light_cct_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
