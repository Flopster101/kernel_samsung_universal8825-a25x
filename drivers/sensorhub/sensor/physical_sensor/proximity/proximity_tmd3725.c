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

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define TMD3725_NAME "TMD3725"

int init_proximity_tmd3725(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	data->cal_data_len = sizeof(int) * 2;

	if (data->threshold_data == NULL) {
		data->threshold_data = kzalloc(sizeof(struct proximity_tmd3725_data), GFP_KERNEL);
		if (!data->threshold_data)
			return -ENOMEM;
	}

	return 0;
}

void parse_dt_proximity_tmd3725(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;
	u16 thd[4];

	if (of_property_read_u16_array(np, "prox-tmd3725-thresh", thd, sizeof(thd)))
		shub_err("no prox-tmd3725-thresh, set as 0");

	memcpy(data->prox_threshold, &thd[0], sizeof(data->prox_threshold));
	memcpy(thd_data->prox_thresh_detect, &thd[2], sizeof(thd_data->prox_thresh_detect));
	shub_info("thresh %u, %u, %u, %u",
		  data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW],
		  thd_data->prox_thresh_detect[PROX_THRESH_HIGH], thd_data->prox_thresh_detect[PROX_THRESH_LOW]);
}

void set_proximity_threshold_tmd3725(void)
{
	int ret;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;
	u16 prox_th[4] = {0, };

	memcpy(&prox_th[0], data->prox_threshold, sizeof(data->prox_threshold));
	memcpy(&prox_th[2], thd_data->prox_thresh_detect, sizeof(thd_data->prox_thresh_detect));
	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_THRESHOLD, (char *)prox_th,
				sizeof(prox_th));
	if (ret < 0) {
		shub_errf("shub_send_command failed");
		return;
	}
}

struct proximity_chipset_funcs prox_tmd3725_funcs = {
	.set_proximity_threshold = set_proximity_threshold_tmd3725,
};

void *get_proximity_tmd3725_chipset_funcs(void)
{
	return &prox_tmd3725_funcs;
}

struct sensor_chipset_init_funcs prox_tmd3725_ops = {
	.init = init_proximity_tmd3725,
	.parse_dt = parse_dt_proximity_tmd3725,
	.get_chipset_funcs = get_proximity_tmd3725_chipset_funcs,
};

struct sensor_chipset_init_funcs *get_proximity_tmd3725_function_pointer(char *name)
{
	if (strcmp(name, TMD3725_NAME) != 0)
		return NULL;

	return &prox_tmd3725_ops;
}
