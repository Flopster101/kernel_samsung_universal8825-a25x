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

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "hub_debugger.h"

#include <linux/slab.h>

u8 buf_fifo_data[512];

u8 *get_hub_debugger_fifo_data(void)
{
	return buf_fifo_data;
}

int get_hub_debugger_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	u16 length = 0;
	u8 *buf;
	u8 cmd = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_HUB_DEBUGGER);

	memcpy(&length, dataframe + *index, 2);
	if (*index + length > frame_len)
		return -EINVAL;

	*index += 2;

	buf = kzalloc(length, GFP_KERNEL);
	memcpy(buf, dataframe + *index, length);

	if (sensor->spec.version == 0) {
		shub_info("[M] %s", buf);
	} else if (sensor->spec.version == 1) {
		cmd = buf[0];

		if (cmd == HUB_DUBBGER_SUBCMD_LOG) {
			shub_info("[M] %s", &buf[1]);
		} else if (cmd == HUB_DUBBGER_SUBCMD_FIFO_DATA) {
			memset(buf_fifo_data, 0, sizeof(buf_fifo_data));
			memcpy(buf_fifo_data, &buf[1], length-1);
		} else {
			shub_info("hub_debugger cmd is wrong : %d", cmd);
		}
	}

	*index += length;

	kfree(buf);

	return 0;
}

static struct sensor_funcs hub_debugger_sensor_funcs = {
	.get_sensor_value = get_hub_debugger_value,
};

int init_hub_debugger(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_HUB_DEBUGGER);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "hub_debugger", 0, 0, 0);
		sensor->hal_sensor = false;
		sensor->funcs = &hub_debugger_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
