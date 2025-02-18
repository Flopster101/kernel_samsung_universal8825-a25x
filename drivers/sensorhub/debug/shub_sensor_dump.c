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

#include "shub_sensor_dump.h"
#include "shub_debug.h"
#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

static char *sensor_dump[SENSOR_TYPE_LEGACY_MAX];

static bool is_support_registerdump(int sensor_type)
{
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	int list_len = (int)(ARRAY_SIZE(types));
	int i, ret = false;

	for (i = 0; i < list_len ; i++) {
		if (types[i] == sensor_type) {
			ret = true;
			break;
		}
	}

	return ret;
}

static int store_sensor_dump(int sensor_type, u16 length, u8 *buf)
{
	char *contents;
	int ret = 0;
	int i = 0;
	int dump_len = sensor_dump_length(length);
	char tmp_ch;

	shub_infof("type %d, length %d", sensor_type, length);

	/*make file contents*/
	contents = kzalloc(dump_len, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(contents)) {
		kfree(contents);
		return -ENOMEM;
	}

	for (i = 0; i < length; i++) {
		tmp_ch = ((i % NUM_LINE_ITEM == NUM_LINE_ITEM - 1) || (i == length - 1)) ? '\n' : ' ';
		snprintf(contents + i * LENGTH_1BYTE_HEXA_WITH_BLANK, dump_len - i * LENGTH_1BYTE_HEXA_WITH_BLANK,
			 "%02x%c", buf[i], tmp_ch);
	}

	if (sensor_dump[sensor_type] != NULL) {
		kfree(sensor_dump[sensor_type]);
		sensor_dump[sensor_type] = NULL;
	}

	sensor_dump[sensor_type] =  contents;

	return ret;
}

void clear_sensor_dump(void)
{
	memset(sensor_dump, 0, sizeof(sensor_dump));
}

int send_sensor_dump_command(u8 sensor_type)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;

	if (sensor_type >= SENSOR_TYPE_LEGACY_MAX) {
		shub_errf("invalid sensor type %d\n", sensor_type);
		return -EINVAL;
	} else if (!get_sensor_probe_state(sensor_type)) {
		shub_errf("%u is not connected", sensor_type);
		return ret;
	}

	if (!is_support_registerdump(sensor_type)) {
		shub_errf("unsupported sensor type %u\n", sensor_type);
		return -EINVAL;
	}

	ret = shub_send_command_wait(CMD_GETVALUE, sensor_type, SENSOR_REGISTER_DUMP,
				     1000, NULL, 0, &buffer, &buffer_length, false);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	shub_infof("(%u)\n", sensor_type);

	ret = store_sensor_dump(sensor_type, buffer_length, buffer);

	kfree(buffer);

	return ret;
}

void send_hub_dump_command(void)
{
	shub_infof("");
	init_log_dump();
	shub_send_command(CMD_SETVALUE, TYPE_HUB, SENSOR_REGISTER_DUMP, NULL, 0);
}

int send_all_sensor_dump_command(void)
{
	int types[] = SENSOR_DUMP_SENSOR_LIST;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(types); i++) {
		int temp = send_sensor_dump_command(types[i]);

		if (temp != 0)
			ret = temp;
	}
	send_hub_dump_command();
	return ret;
}

char **get_sensor_dump_data(void)
{
	return sensor_dump;
}
