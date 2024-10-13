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

#ifndef __SHUB_MAGNETOMETER_FACTORY_H_
#define __SHUB_MAGNETOMETER_FACTORY_H_

#include "../../sensor/magnetometer.h"

#include <linux/device.h>
#include <linux/types.h>

struct device_attribute **get_magnetometer_ak09918c_dev_attrs(char *name);
int check_ak09918c_adc_data_spec(s32 sensor_value[3]);

struct device_attribute **get_magnetometer_mmc5633_dev_attrs(char *name);
int check_mmc5633_adc_data_spec(s32 sensor_value[3]);

struct device_attribute **get_magnetometer_yas539_dev_attrs(char *name);
int check_yas539_adc_data_spec(s32 sensor_value[3]);

struct device_attribute **get_magnetometer_mxg4300s_dev_attrs(char *name);
int check_mxg4300s_adc_data_spec(s32 sensor_value[3]);

void get_magnetometer_sensor_value_s32(struct mag_power_event *event, s32 *sensor_value);
#endif
