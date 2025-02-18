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
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#include "../../../sensor/proximity.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../comm/shub_comm.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"
#include "../../../others/shub_panel.h"

#define STK33910_NAME "STK33910"
#define STK33915_NAME "STK33915"
#define STK_VENDOR "Sitronix"

int init_proximity_stk3391x(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	data->cal_data_len = sizeof(u16);
	data->setting_mode = 1;

	return 0;
}

void parse_dt_proximity_stk3391x(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (of_property_read_u16_array(np, "prox-stk3391x-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-stk3391x-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);
}

int proximity_open_calibration_stk3391x(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	open_default_proximity_setting_mode();
	open_default_proximity_calibration();
	shub_infof(" mode:%d base:%d", data->setting_mode, *((u16 *)data->cal_data));

	return 0;
}

void set_proximity_state_stk3391x(struct proximity_data *data)
{
	set_proximity_setting_mode();
	set_proximity_calibration();
}

struct proximity_chipset_funcs prox_stk3391x_funcs = {
	.open_calibration_file = proximity_open_calibration_stk3391x,
	.sync_proximity_state = set_proximity_state_stk3391x,
};

void *get_proximity_stk3391x_chipset_funcs(void)
{
	return &prox_stk3391x_funcs;
}

struct sensor_chipset_init_funcs prox_stk3391x_ops = {
	.init = init_proximity_stk3391x,
	.parse_dt = parse_dt_proximity_stk3391x,
	.get_chipset_funcs = get_proximity_stk3391x_chipset_funcs,
};

struct sensor_chipset_init_funcs *get_proximity_stk3391x_function_pointer(char *name)
{
	if (strcmp(name, STK33910_NAME) != 0 && strcmp(name, STK33915_NAME) != 0)
		return NULL;

	return &prox_stk3391x_ops;
}
