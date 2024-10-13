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
#include "../debug/shub_debug.h"
#include "../sensor/pressure.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"

#include <linux/of_gpio.h>
#include <linux/slab.h>

#define CALIBRATION_FILE_PATH "/efs/FactoryApp/baro_delta"
#define SW_OFFSET_FILE_PATH "/efs/FactoryApp/baro_sw_offset"

get_init_chipset_funcs_ptr get_pressure_funcs_ary[] = {
	get_pressure_bmp580_function_pointer,
	get_pressure_lps22hh_function_pointer,
	get_pressure_lps22df_function_pointer,
};

static get_init_chipset_funcs_ptr *get_pressure_init_chipset_funcs(int *len)
{
	*len = ARRAY_SIZE(get_pressure_funcs_ary);
	return get_pressure_funcs_ary;
}

static unsigned int parse_int(const char *s, unsigned int base, int *p)
{
	int result = 0;
	int i = 0, size = 0, val = 0;
	int negative_value = 1;

	for (i = 0; i < 10; i++) {
		if ('-' == s[i])
			negative_value = -1;
		else if ('0' <= s[i] && s[i] <= '9')
			val = s[i] - '0';
		else
			break;

		result = result * base + val;
		size++;
	}
	*p = (result * negative_value);

	return size;
}

static int open_pressure_calibration_file(void)
{
	char chBuf[10] = {0, };
	int ret = 0;
	struct pressure_event *sensor_value = (struct pressure_event *)(get_sensor_event(SENSOR_TYPE_PRESSURE)->value);

	ret = shub_file_read(CALIBRATION_FILE_PATH, chBuf, sizeof(chBuf), 0);
	if (ret < 0) {
		shub_errf("Can't read the cal data from file (%d)\n", ret);
		goto exit;
	}

	ret = parse_int(chBuf, 10, &sensor_value->pressure_cal);
	if (ret < 0) {
		shub_errf("kstrtoint failed. %d", ret);
		goto exit;
	}

	shub_infof("open pressure calibration %d", sensor_value->pressure_cal);

exit:
	set_open_cal_result(SENSOR_TYPE_PRESSURE, ret);
	return ret;
}

int open_pressure_sw_offset_file(void)
{
	int ret = 0;
	struct pressure_data *data = get_sensor(SENSOR_TYPE_PRESSURE)->data;
	int sw_offset = 0;

	shub_infof("");

	ret = shub_file_read(SW_OFFSET_FILE_PATH, (char *)&sw_offset, sizeof(sw_offset), 0);
	if (ret != sizeof(sw_offset)) {
		ret = -EIO;
	} else {
		data->sw_offset = sw_offset;
		shub_infof("sw offset file %d", sw_offset);
	}

	return ret;
}

int save_pressure_sw_offset_file(int offset)
{
	int ret = 0;

	shub_infof("");
	ret = shub_file_write_no_wait(SW_OFFSET_FILE_PATH, (char *)&offset, sizeof(offset), 0);
	if (ret != sizeof(offset)) {
		shub_errf("Can't write sw offset to file");
		ret = -EIO;
	}

	return ret;
}

static void parse_dt_pressure(struct device *dev)
{
	struct pressure_data *data = get_sensor(SENSOR_TYPE_PRESSURE)->data;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "pressure-sw-offset", &data->sw_offset)) {
		shub_infof("no sw-offset");
		data->sw_offset_default = 0;
		data->sw_offset = 0;
	} else {
		data->sw_offset_default = data->sw_offset;
	}
}

int sync_pressure_status(void)
{
	shub_infof();
	return 0;
}


static void report_pressure_event(void)
{
#ifndef CONFIG_SEC_FACTORY
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);
	struct sensor_event *event = &(sensor->event_buffer);
	struct pressure_event *sensor_value = (struct pressure_event *)(event->value);
	struct pressure_data *data = sensor->data;

	sensor_value->pressure -= data->sw_offset * data->convert_coef / 100;
#endif
}

void print_pressure_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct pressure_event *sensor_value = (struct pressure_event *)(event->value);
	struct pressure_data *data = sensor->data;

	shub_info("%s(%u) : %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PRESSURE,
		  sensor_value->pressure, sensor_value->temperature, sensor_value->pressure_cal, data->sw_offset,
		  data->convert_coef, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

static int open_pressure_files(void)
{
	shub_infof("");
	open_pressure_calibration_file();

	return 0;
}

static int enable_pressure_sensor(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);
	struct pressure_data *data = sensor->data;

	shub_infof("so %d", data->sw_offset);

	return 0;
}

static int disable_pressure_sensor(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);
	struct pressure_data *data = sensor->data;

	shub_infof("so %d", data->sw_offset);

	return 0;
}

static struct sensor_funcs pressure_sensor_funcs = {
	.enable = enable_pressure_sensor,
	.disable = disable_pressure_sensor,
	.print_debug = print_pressure_debug,
	.open_calibration_file = open_pressure_files,
	.report_event = report_pressure_event,
	.parse_dt = parse_dt_pressure,
	.get_init_chipset_funcs = get_pressure_init_chipset_funcs,
};

static struct pressure_data pressure_data;

int init_pressure(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "pressure_sensor", 6, 14, sizeof(struct pressure_event));
		sensor->report_mode_continuous = true;
		sensor->data = (void *)&pressure_data;
		sensor->funcs = &pressure_sensor_funcs;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
