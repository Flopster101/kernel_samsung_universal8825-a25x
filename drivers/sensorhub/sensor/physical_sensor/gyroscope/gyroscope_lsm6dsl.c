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

#include "../../gyroscope.h"
#include "../../accelerometer.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../utility/shub_utility.h"

#include <linux/device.h>

#define LSM6DSL_NAME		"LSM6DSL"
static void parse_dt_gyroscope_lsm6dsl(struct device *dev)
{
	struct accelerometer_data *acc_data =  get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	data->position = acc_data->position;
	shub_infof("position[%d]", data->position);
}

struct sensor_chipset_init_funcs gyro_lsm6dsl_ops = {
	.parse_dt = parse_dt_gyroscope_lsm6dsl,
};

struct sensor_chipset_init_funcs *get_gyroscope_lsm6dsl_function_pointer(char *name)
{
	if (strcmp(name, LSM6DSL_NAME) != 0)
		return NULL;

	return &gyro_lsm6dsl_ops;
}
