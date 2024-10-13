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

#include "../../sensor/proximity.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../comm/shub_comm.h"
#include "../../sensorhub/shub_device.h"
#include "../../utility/shub_utility.h"
#include "proximity_factory.h"
#include "../../others/shub_panel.h"

#define STK33910_NAME "STK33910"
#define STK33915_NAME "STK33915"
#define STK_VENDOR "Sitronix"

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	return sprintf(buf, "%s\n", sensor->spec.name);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", STK_VENDOR);
}

static ssize_t prox_trim_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = (struct proximity_data *)get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	shub_infof("prox_trim_show : %d", data->setting_mode);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->setting_mode);
}

static int proximity_get_calibration_data(void)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	struct proximity_data *data = (struct proximity_data *)get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, CAL_DATA, 1000, NULL, 0, &buffer,
					&buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	if (buffer_length != data->cal_data_len) {
		shub_errf("buffer length error(%d)", buffer_length);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(data->cal_data, buffer, data->cal_data_len);

	save_proximity_calibration();

	kfree(buffer);

	return 0;
}

static int proximity_get_setting_mode(void)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	struct proximity_data *data = (struct proximity_data *)get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_SETTING_MODE, 1000, NULL,
					0, &buffer, &buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	if (buffer_length != sizeof(data->setting_mode)) {
		shub_errf("buffer length error(%d)", buffer_length);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(&data->setting_mode, buffer, sizeof(data->setting_mode));
	save_proximity_setting_mode();

	kfree(buffer);

	return 0;
}

static ssize_t proximity_cal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	struct proximity_data *data = (struct proximity_data *)get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	save_panel_lcd_type();

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, PROX_SUBCMD_CALIBRATION_START, NULL, 0);
	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	msleep(500);

	proximity_get_setting_mode();
	proximity_get_calibration_data();

	shub_infof("ADC : %u, mode : %u", *((u16 *)(data->cal_data)), data->setting_mode);

	return size;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(prox_trim);
static DEVICE_ATTR(prox_cal, 0220, NULL, proximity_cal_store);

static struct device_attribute *proximity_stk3391x_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_prox_trim,
	&dev_attr_prox_cal,
	NULL,
};

struct device_attribute **get_proximity_stk3391x_dev_attrs(char *name)
{
	if (strcmp(name, STK33910_NAME) != 0 && strcmp(name, STK33915_NAME) != 0)
		return NULL;

	return proximity_stk3391x_attrs;
}
