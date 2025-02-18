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
#include "../comm/shub_comm.h"

#include <linux/slab.h>

#define ORIENTATION_SUBCMD_MODE	128

struct orientation_data {
	u8 mode;
};

int set_device_orientation_mode(struct orientation_data *data)
{
	int ret = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_DEVICE_ORIENTATION)) {
		shub_infof("sensor is not connected");
		return ret;
	}

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_DEVICE_ORIENTATION, ORIENTATION_SUBCMD_MODE,
				&data->mode, sizeof(data->mode));
	if (ret < 0) {
		shub_errf("CMD fail %d", ret);
		return ret;
	}

	shub_infof("%d", data->mode);

	return ret;
}

static int sync_device_orientation_status(void)
{
	int ret = 0;
	struct orientation_data *data = get_sensor(SENSOR_TYPE_DEVICE_ORIENTATION)->data;

	set_device_orientation_mode(data);

	return ret;
}


int inject_device_orientation_additional_data(char *buf, int count)
{
	struct orientation_data *data = get_sensor(SENSOR_TYPE_DEVICE_ORIENTATION)->data;

	if (count < 1) {
		shub_errf("length error %d", count);
		return -EINVAL;
	}

	data->mode = buf[0];

	return set_device_orientation_mode(data);
}

static struct sensor_funcs device_orientation_sensor_func = {
	.sync_status = sync_device_orientation_status,
	.inject_additional_data = inject_device_orientation_additional_data,
};

static struct orientation_data orientation_data;

int init_device_orientation(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_DEVICE_ORIENTATION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "device_orientation", 1, 1, 1);

		sensor->data = (void *)&orientation_data;
		sensor->funcs = &device_orientation_sensor_func;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

int init_device_orientation_wu(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_DEVICE_ORIENTATION_WU);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "device_orientation_wu", 1, 1, 1);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
