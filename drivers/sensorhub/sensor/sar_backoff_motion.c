/*
 *  Copyright (C) 2022, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include "../comm/shub_comm.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

#define SBM_SUBCMD_RESET	128

static int set_sar_backoff_reset_value(int32_t value)
{
	int ret = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_SAR_BACKOFF_MOTION)) {
		shub_infof("sensor is not connected");
		return ret;
	}

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_SAR_BACKOFF_MOTION, SBM_SUBCMD_RESET,
				(char *)&value, sizeof(value));
	if (ret < 0) {
		shub_errf("CMD fail %d", ret);
		return ret;
	}

	shub_infof();

	return ret;
}

static int inject_sar_backoff_motion_additional_data(char *buf, int count)
{
	int32_t value;

	if (count < 4) {
		shub_errf("length error %d", count);
		return -EINVAL;
	}

	value = *((int32_t *) (buf));

	return set_sar_backoff_reset_value(value);
}


static struct sensor_funcs sar_backoff_motion_sensor_funcs = {
	.inject_additional_data = inject_sar_backoff_motion_additional_data,
};

int init_sar_backoff_motion(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SAR_BACKOFF_MOTION);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "sar_backoff_motion", 1, 1, 1);
		sensor->funcs = &sar_backoff_motion_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
