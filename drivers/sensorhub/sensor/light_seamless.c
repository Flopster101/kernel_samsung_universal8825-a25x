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

#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "light_seamless.h"

#include <linux/slab.h>

#define INPUT_MAX 256

static int inject_light_seamless_additional_data(char *buf, int count)
{
	int ret = 0;
	int index = 0;
	int thd[2] = {200, 1000000};
	char input[INPUT_MAX] = {0,};
	char *input_tmp = NULL, *token = NULL;

	if (count > INPUT_MAX) {
		shub_errf("bufsize too long(%d)", count);
		return -EINVAL;
	}

	memcpy(input, buf, count);
	input_tmp = input;

	while ((token = strsep(&input_tmp, ",")) != NULL && index < 2) {
		ret = kstrtoint(token, 10, &thd[index++]);
		if (ret < 0) {
			shub_errf("kstrtoint failed.(%d)", ret);
			return ret;
		}
	}

	shub_infof("thd[0] = %d thd[1] = %d", thd[0], thd[1]);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT_SEAMLESS, LIGHT_SUBCMD_THRESHOLD, (char *)&thd, sizeof(thd));

	return ret;
}

static struct sensor_funcs light_seamless_sensor_funcs = {
	.inject_additional_data = inject_light_seamless_additional_data,
};

int init_light_seamless(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_SEAMLESS);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "light_seamless_sensor", 4, 4, sizeof(struct light_seamless_event));

		sensor->funcs = &light_seamless_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
