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

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../../sensor/proximity.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../comm/shub_comm.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"

#define GP2AP110S_NAME    "GP2AP110S"
#define GP2AP110S_VENDOR  "SHARP"

int init_proximity_gp2ap110s(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->threshold_data == NULL) {
		data->threshold_data = kzalloc(sizeof(struct proximity_gp2ap110s_data), GFP_KERNEL);
		if (!data->threshold_data)
			return -ENOMEM;
	}

	data->setting_mode = 1;
	return 0;
}

void parse_dt_proximity_gp2ap110s(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	if (of_property_read_u16_array(np, "prox-gp2ap110s-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-gp2ap110s-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);

	if (of_property_read_u16_array(np, "prox-gp2ap110s-setting-thresh", thd_data->prox_setting_thresh, 2))
		shub_err("no prox-gp2ap110s-setting-thresh, set as 0");

	shub_info("prox-gp2ap110s-setting-thresh - %u, %u", thd_data->prox_setting_thresh[0],
		  thd_data->prox_setting_thresh[1]);

	if (of_property_read_u16_array(np, "prox-gp2ap110s-mode-thresh", thd_data->prox_mode_thresh, PROX_THRESH_SIZE))
		shub_err("no prox-gp2ap110s-mode-thresh, set as 0");

	shub_info("prox-gp2ap110s-mode-thresh - %u, %u", thd_data->prox_mode_thresh[PROX_THRESH_HIGH],
		  thd_data->prox_mode_thresh[PROX_THRESH_LOW]);
}

int open_proximity_setting_mode(void)
{
	int ret = -1;
	char buf[3] = "";
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	ret = shub_file_read(PROX_SETTING_MODE_FILE_PATH, buf, sizeof(buf), 0);
	if (ret <= 0) {
		shub_errf("Can't read the prox settings data from file, bytes=%d", ret);
		ret = -EIO;
	} else {
		if (buf[0] == 1 || buf[0] == 2)
			data->setting_mode = buf[0];
		else
			sscanf(buf, "%hhu", &data->setting_mode);

		shub_infof("prox_settings %d", data->setting_mode);
		if (data->setting_mode != 1 && data->setting_mode != 2) {
			data->setting_mode = 1;
			shub_errf("leg_reg_val is wrong. set defulat setting");
		}
	}

	if (data->setting_mode != 1)
		memcpy(data->prox_threshold, thd_data->prox_mode_thresh, sizeof(data->prox_threshold));

	return ret;
}

void set_proximity_gp2ap110s_state(struct proximity_data *data) // sync
{
	set_proximity_setting_mode();
}

void pre_enable_proximity_gp2ap110s(struct proximity_data *data)
{
	set_proximity_setting_mode();
}

struct proximity_chipset_funcs prox_gp2ap110s_funcs = {
	.sync_proximity_state = set_proximity_gp2ap110s_state,
	.pre_enable_proximity = pre_enable_proximity_gp2ap110s,
	.open_calibration_file = open_proximity_setting_mode,
};

void *get_proximity_gp2ap110s_chipset_funcs(void)
{
	return &prox_gp2ap110s_funcs;
}

struct sensor_chipset_init_funcs prox_gp2ap110s_ops = {
	.init = init_proximity_gp2ap110s,
	.parse_dt = parse_dt_proximity_gp2ap110s,
	.get_chipset_funcs = get_proximity_gp2ap110s_chipset_funcs,
};

struct sensor_chipset_init_funcs *get_proximity_gp2ap110s_function_pointer(char *name)
{
	if (strcmp(name, GP2AP110S_NAME) != 0)
		return NULL;

	return &prox_gp2ap110s_ops;
}
