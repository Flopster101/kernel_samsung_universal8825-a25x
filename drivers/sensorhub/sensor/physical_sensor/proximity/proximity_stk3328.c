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

#include "../../../comm/shub_comm.h"
#include "../../../sensor/proximity.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define STK3328_NAME   "STK3328"

void parse_dt_proximity_stk3328(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_stk3328_data *thd_data = data->threshold_data;


	if (of_property_read_u16_array(np, "prox-stk3328-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-stk3328-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);

	if (of_property_read_u16(np, "prox-stk3328-cal-add-value", &thd_data->prox_cal_add_value))
		shub_err("no prox-stk3328-cal-add-value, set as 0");

	shub_info("cal add value - %u", thd_data->prox_cal_add_value);

	if (of_property_read_u16_array(np, "prox-stk3328-cal-thresh", thd_data->prox_cal_thresh, 2))
		shub_err("no prox-stk3328-cal-thresh, set as 0");

	shub_info("cal thresh - %u, %u", thd_data->prox_cal_thresh[0], thd_data->prox_cal_thresh[1]);

	thd_data->prox_thresh_default[0] = data->prox_threshold[0];
	thd_data->prox_thresh_default[1] = data->prox_threshold[1];
}

int proximity_open_calibration_stk3328(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	ret = shub_file_read(PROX_CALIBRATION_FILE_PATH,
			     (char *)&data->prox_threshold, sizeof(data->prox_threshold), 0);
	if (ret != sizeof(data->prox_threshold))
		ret = -EIO;

	shub_infof("thresh %d, %d", data->prox_threshold[0], data->prox_threshold[1]);

	return ret;
}

int init_proximity_stk3328(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->threshold_data == NULL) {
		data->threshold_data = kzalloc(sizeof(struct proximity_stk3328_data), GFP_KERNEL);
		if (!data->threshold_data)
			return -ENOMEM;
	}
	return 0;
}

struct proximity_chipset_funcs prox_stk3328_funcs = {
	.open_calibration_file = proximity_open_calibration_stk3328,
};

void *get_proximity_stk3328_chipset_funcs(void)
{
	return &prox_stk3328_funcs;
}

struct sensor_chipset_init_funcs prox_stk3328_ops = {
	.init = init_proximity_stk3328,
	.parse_dt = parse_dt_proximity_stk3328,
	.get_chipset_funcs = get_proximity_stk3328_chipset_funcs,
};

struct sensor_chipset_init_funcs *get_proximity_stk3328_function_pointer(char *name)
{
	if (strcmp(name, STK3328_NAME) != 0)
		return NULL;

	return &prox_stk3328_ops;
}
