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
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "../others/shub_motor_callback.h"
#include "accelerometer.h"

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

get_init_chipset_funcs_ptr get_acc_funcs_ary[] = {
	get_accelometer_icm42605m_function_pointer,
	get_accelometer_lsm6dsl_function_pointer,
	get_accelometer_lis2dlc12_function_pointer,
	get_accelometer_lsm6dsotr_function_pointer,
	get_accelometer_lsm6dsvtr_function_pointer,
	get_accelometer_icm42632m_function_pointer,
};

static get_init_chipset_funcs_ptr *get_accel_init_chipset_funcs(int *len)
{
	*len = ARRAY_SIZE(get_acc_funcs_ary);
	return get_acc_funcs_ary;
}

static int init_accelerometer_variable(void)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	if (is_support_system_feature(SF_ACCEL_16G))
		data->range = 16;
	else
		data->range = 8;

	return 0;
}

void parse_dt_accelerometer(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int accel_motor_coef = 0;

	if (!of_property_read_u32(np, "acc-motor-coef", &accel_motor_coef))
		set_motor_coef(accel_motor_coef);
	shub_infof("acc-motor-coef[%d]", accel_motor_coef);
}

int set_accel_position(int position)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	data->position = position;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, SENSOR_AXIS, (char *)&(data->position),
				sizeof(data->position));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("A : %u", data->position);

	return ret;
}

int get_accel_position(void)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	return data->position;
}

static int open_accel_calibration_file(void)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	ret = shub_file_read(ACCEL_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data))
		ret = -EIO;

	shub_infof("open accel calibration %d, %d, %d\n", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	if ((data->cal_data.x == 0) && (data->cal_data.y == 0) && (data->cal_data.z == 0))
		return -EINVAL;

	return ret;
}

int set_accel_cal(struct accelerometer_data *data)
{
	int ret = 0;
	s16 accel_cal[3] = {0, };

	if (!get_sensor_probe_state(SENSOR_TYPE_ACCELEROMETER)) {
		shub_infof("[SHUB] Skip this function!!!, accel sensor is not connected\n");
		return ret;
	}

	accel_cal[0] = data->cal_data.x;
	accel_cal[1] = data->cal_data.y;
	accel_cal[2] = data->cal_data.z;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, CAL_DATA,
				(char *)accel_cal, sizeof(accel_cal));

	if (ret < 0) {
		shub_errf("CMD Fail %d", ret);
		return ret;
	}
	shub_info("[SHUB] Set accel cal data %d, %d, %d\n", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	return ret;
}

int sync_accelerometer_status(void)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	shub_infof();
	ret = set_accel_position(data->position);
	if (ret < 0) {
		shub_errf("set position failed");
		return ret;
	}

	ret = set_accel_cal(data);
	if (ret < 0) {
		shub_errf("set_mag_cal failed");
		return ret;
	}

	return ret;
}

void print_accelerometer_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct accel_event *sensor_value = (struct accel_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_ACCELEROMETER, sensor_value->x,
		  sensor_value->y, sensor_value->z, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

static struct sensor_funcs accelerometer_sensor_func = {
	.sync_status = sync_accelerometer_status,
	.print_debug = print_accelerometer_debug,
	.set_position = set_accel_position,
	.get_position = get_accel_position,
	.open_calibration_file = open_accel_calibration_file,
	.parse_dt = parse_dt_accelerometer,
	.init_variable = init_accelerometer_variable,
	.get_init_chipset_funcs = get_accel_init_chipset_funcs,
};

static struct accelerometer_data accelerometer_data;

int init_accelerometer(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "accelerometer_sensor", 6, 6, sizeof(struct accel_event));

		sensor->report_mode_continuous = true;
		sensor->data = (void *)&accelerometer_data;
		sensor->funcs = &accelerometer_sensor_func;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}

static void print_accelerometer_uncal_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED);
	struct sensor_event *event = &(sensor->last_event_buffer);
	struct uncal_accel_event *sensor_value = (struct uncal_accel_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name,
		  SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED, sensor_value->uncal_x, sensor_value->uncal_y,
		  sensor_value->uncal_z, sensor_value->offset_x, sensor_value->offset_y, sensor_value->offset_z,
		  event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

static struct sensor_funcs accelerometer_uncal_sensor_func = {
	.print_debug = print_accelerometer_uncal_debug,
};

int init_accelerometer_uncal(bool en)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED);

	if (!sensor)
		return 0;

	if (en) {
		ret = init_default_func(sensor, "uncal_accel_sensor", 12, 12, sizeof(struct uncal_accel_event));

		sensor->report_mode_continuous = true;
		sensor->funcs = &accelerometer_uncal_sensor_func;
	} else {
		destroy_default_func(sensor);
	}

	return ret;
}
