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

#ifndef __SHUB_ACCELEROMETER_H_
#define __SHUB_ACCELEROMETER_H_

#include <linux/types.h>
#include <linux/device.h>

#ifdef CONFIG_SHUB_TEST_FOR_ONLY_UML
#define ACCEL_CALIBRATION_FILE_PATH "accelerometer_calibration.txt"
#else
#define ACCEL_CALIBRATION_FILE_PATH "/efs/FactoryApp/calibration_data"
#endif
struct accel_event {
	s16 x;
	s16 y;
	s16 z;
} __attribute__((__packed__));

struct uncal_accel_event {
	s16 uncal_x;
	s16 uncal_y;
	s16 uncal_z;
	s16 offset_x;
	s16 offset_y;
	s16 offset_z;
} __attribute__((__packed__));

struct calibration_data {
	s16 x;
	s16 y;
	s16 z;
};

struct accelerometer_data {
	int position;
	struct calibration_data cal_data;
	bool is_accel_alert;
	int range;
};

struct sensor_chipset_init_funcs *get_accelometer_lsm6dsl_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_accelometer_icm42605m_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_accelometer_lis2dlc12_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_accelometer_lsm6dsotr_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_accelometer_icm42632m_function_pointer(char *name);
struct sensor_chipset_init_funcs *get_accelometer_lsm6dsvtr_function_pointer(char *name);

int set_accel_cal(struct accelerometer_data *data);
#endif
