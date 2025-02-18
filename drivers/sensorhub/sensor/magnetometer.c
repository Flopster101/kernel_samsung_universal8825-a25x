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
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_wakelock.h"
#include "../utility/shub_file_manager.h"
#include "flip_cover_detector.h"
#include "magnetometer.h"

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define MAG_CALIBRATION_FILE_PATH "/efs/FactoryApp/mag_cal_data"

#define MAG_RECEIVE_EVENT_SIZE(x) (((x) * 3) + 1)

get_init_chipset_funcs_ptr get_mag_funcs_ary[] = {
	get_magnetic_ak09918c_function_pointer,
	get_magnetic_yas539_function_pointer,
	get_magnetic_mmc5633_function_pointer,
	get_magnetic_mxg4300s_function_pointer,
};

static get_init_chipset_funcs_ptr *get_magnetometer_init_chipset_funcs(int *len)
{
	*len = ARRAY_SIZE(get_mag_funcs_ary);
	return get_mag_funcs_ary;
}

static int init_magnetometer_variable(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	if (data->cal_data_len) {
		data->cal_data = kzalloc(data->cal_data_len, GFP_KERNEL);
		if (!data->cal_data)
			return -ENOMEM;
	}

	if (data->mag_matrix_len) {
		data->mag_matrix = kzalloc(data->mag_matrix_len, GFP_KERNEL);
		if (!data->mag_matrix)
			return -ENOMEM;

		if (get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR) && check_flip_cover_detector_supported()) {
			data->cover_matrix = kzalloc(data->mag_matrix_len, GFP_KERNEL);
			if (!data->cover_matrix)
				return -ENOMEM;
		}
	}

	return 0;
}

static int set_mag_position(int position)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	data->position = position;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_AXIS, (char *)&(data->position),
				sizeof(data->position));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("%u", data->position);

	return ret;
}

static int get_mag_position(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	return data->position;
}

int set_mag_matrix(struct magnetometer_data *data)
{
	int ret = 0;

	shub_infof();

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, MAGNETIC_STATIC_MATRIX,
				(char *)data->mag_matrix, data->mag_matrix_len);
	shub_infof("%u", data->position);

	if (ret < 0) {
		shub_errf("failed %d", ret);
		return ret;
	}

	return 0;
}

int set_mag_cover_matrix(struct magnetometer_data *data)
{
	int ret = 0;

	if (!data->cover_matrix)
		return 0;

	shub_infof();

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, MAGNETIC_COVER_MATRIX,
				(char *)data->cover_matrix, data->mag_matrix_len);
	shub_infof("%u", data->position);

	if (ret < 0) {
		shub_errf("failed %d", ret);
		return ret;
	}

	return 0;
}

int get_mag_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct mag_event *sensor_value = (struct mag_event *)event->value;

	if (sensor->receive_event_size == sizeof(struct mag_event)) {
		memcpy(sensor_value, dataframe + *index, sizeof(struct mag_event));
		*index += sensor->receive_event_size;
	} else {
		s16 temp_mag_value[3];

		memcpy(&temp_mag_value, dataframe + *index, sizeof(temp_mag_value));
		*index += sizeof(temp_mag_value);
		sensor_value->x = (s32) temp_mag_value[0];
		sensor_value->y = (s32) temp_mag_value[1];
		sensor_value->z = (s32) temp_mag_value[2];
		memcpy(&sensor_value->accuracy, dataframe + *index, sizeof(sensor_value->accuracy));
		*index += sizeof(sensor_value->accuracy);
	}
	return 0;
}

static int open_mag_calibration_file(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	ret = shub_file_read(MAG_CALIBRATION_FILE_PATH, data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		ret = -EIO;
		shub_errf("Can't read calibration file %d", ret);
		memset(data->cal_data, 0, data->cal_data_len);
	}

	set_open_cal_result(SENSOR_TYPE_GEOMAGNETIC_FIELD, ret);
	return ret;
}

static int save_mag_calibration_file(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	ret = shub_file_write_no_wait(MAG_CALIBRATION_FILE_PATH, data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("Can't write mag cal to file");
		ret = -EIO;
	}

	return ret;
}

static int parsing_mag_calibration(char *dataframe, int *index, int frame_len)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	if (*index + data->cal_data_len > frame_len) {
		shub_errf("parssing error");
		return -EINVAL;
	}

	shub_infof("Mag caldata received from MCU(%d)", data->cal_data_len);
	memcpy(data->cal_data, dataframe + (*index), data->cal_data_len);
	shub_wake_lock();
	save_mag_calibration_file();
	shub_wake_unlock();
	(*index) += data->cal_data_len;

	return 0;
}

static int set_mag_cal(struct magnetometer_data *data)
{
	int ret = 0;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, CAL_DATA,
				(char *)data->cal_data, data->cal_data_len);
	if (ret < 0)
		shub_errf("shub_send_command fail %d", ret);
	return ret;
}

static int sync_magnetometer_status(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	shub_infof();
	ret = set_mag_position(data->position);
	if (ret < 0) {
		shub_errf("set_position failed");
		return ret;
	}

	ret = set_mag_matrix(data);
	if (ret < 0) {
		shub_errf("initialize magnetic sensor failed");
		return ret;
	}

	ret = set_mag_cal(data);
	if (ret < 0)
		shub_errf("set_mag_cal failed");

	set_mag_cover_matrix(data);

	return ret;
}

static void print_magnetometer_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct mag_event *sensor_value = (struct mag_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_GEOMAGNETIC_FIELD,
		  sensor_value->x, sensor_value->y, sensor_value->z, sensor_value->accuracy, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

static struct magnetometer_data magnetometer_data;
static struct sensor_funcs magnetometer_sensor_funcs = {
	.sync_status = sync_magnetometer_status,
	.set_position = set_mag_position,
	.get_position = get_mag_position,
	.print_debug = print_magnetometer_debug,
	.parsing_data = parsing_mag_calibration,
	.open_calibration_file = open_mag_calibration_file,
	.get_sensor_value = get_mag_sensor_value,
	.init_variable = init_magnetometer_variable,
	.get_init_chipset_funcs = get_magnetometer_init_chipset_funcs,
};

int init_magnetometer(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);

	if (!sensor)
		return 0;

	if (en) {
		int receive_size = sensor->spec.version >= MAG_EVENT_SIZE_4BYTE_VERSION ?
								MAG_RECEIVE_EVENT_SIZE(sizeof(s32)) : MAG_RECEIVE_EVENT_SIZE(sizeof(s16));

		shub_infof("receive_event_size : %d", sensor->receive_event_size);

		ret = init_default_func(sensor, "geomagnetic_sensor", receive_size, sizeof(struct mag_event), sizeof(struct mag_event));
		sensor->report_mode_continuous = true;
		sensor->data = (void *)&magnetometer_data;
		sensor->funcs = &magnetometer_sensor_funcs;
	} else {
		kfree_and_clear(magnetometer_data.cal_data);
		kfree_and_clear(magnetometer_data.mag_matrix);
		kfree_and_clear(magnetometer_data.cover_matrix);
		destroy_default_func(sensor);
	}

	return ret;
}

int init_magnetometer_power(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_POWER);

	if (!sensor)
		return 0;

	if (en) {
		int size = sizeof(struct mag_power_event);
		ret = init_default_func(sensor, "geomagnetic_power", size, size, size);
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
