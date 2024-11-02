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

#include "../utility/shub_utility.h"
#include "../sensor/magnetometer.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"

#define UNCAL_MAG_RECEIVE_EVENT_SIZE(x) ((x) * 6)

void print_magnetometer_uncal_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct uncal_mag_event *sensor_value = (struct uncal_mag_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)",
		  sensor->name, SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
		  sensor_value->uncal_x, sensor_value->uncal_y, sensor_value->uncal_z,
		  sensor_value->offset_x, sensor_value->offset_y, sensor_value->offset_z, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

int get_magnetometer_uncal_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);
	struct uncal_mag_event *sensor_value = (struct uncal_mag_event *)event->value;

	if (sensor->receive_event_size == sizeof(struct uncal_mag_event)) {
		memcpy(sensor_value, dataframe + *index, sizeof(struct uncal_mag_event));
	} else {
		s16 temp_mag_value[6];

		memcpy(&temp_mag_value, dataframe + *index, sizeof(temp_mag_value));
		sensor_value->uncal_x = (s32) temp_mag_value[0];
		sensor_value->uncal_y = (s32) temp_mag_value[1];
		sensor_value->uncal_z = (s32) temp_mag_value[2];
		sensor_value->offset_x = (s32) temp_mag_value[3];
		sensor_value->offset_y = (s32) temp_mag_value[4];
		sensor_value->offset_z = (s32) temp_mag_value[5];
	}

	*index += sensor->receive_event_size;

	return 0;
}

static struct sensor_funcs magnetometer_uncal_sensor_funcs = {
	.print_debug = print_magnetometer_uncal_debug,
	.get_sensor_value = get_magnetometer_uncal_sensor_value,
};

int init_magnetometer_uncal(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);

	if (!sensor)
		return 0;

	if (en) {
		int receive_size = sensor->spec.version >= MAG_EVENT_SIZE_4BYTE_VERSION ?
								UNCAL_MAG_RECEIVE_EVENT_SIZE(sizeof(s32)) : UNCAL_MAG_RECEIVE_EVENT_SIZE(sizeof(s16));

		shub_infof("receive_event_size : %d", sensor->receive_event_size);

		ret = init_default_func(sensor, "uncal_geomagnetic_sensor", receive_size, sizeof(struct uncal_mag_event), sizeof(struct uncal_mag_event));
		sensor->report_mode_continuous = true;
		sensor->funcs = &magnetometer_uncal_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
