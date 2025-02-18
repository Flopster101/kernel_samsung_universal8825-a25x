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

#include "../../comm/shub_comm.h"
#include "../../sensor/proximity.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"
#include "proximity_factory.h"

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define TMD4912_NAME   "TMD4912"
#define TMD4912_VENDOR "AMS"

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", TMD4912_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", TMD4912_VENDOR);
}

static ssize_t prox_led_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	struct prox_led_test {
		u8 ret;
		int adc[4];

	} __attribute__((__packed__)) result;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_LED_TEST, 1300, NULL, 0, &buffer,
				     &buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	if (buffer_length != sizeof(result)) {
		shub_errf("buffer length error(%d)", buffer_length);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(&result, buffer, buffer_length);

	ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n", result.ret, result.adc[0], result.adc[1], result.adc[2],
		       result.adc[3]);

	kfree(buffer);

	return ret;
}

/* show calibration result */
static ssize_t prox_trim_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct proximity_data *data = (struct proximity_data *)sensor->data;
	int *cal_data;

	if (data->cal_data_len == 0)
		return -EINVAL;

	ret = sensor->funcs->open_calibration_file();
	if (ret == data->cal_data_len)
		cal_data = (int *)data->cal_data;

	// prox_cal[1] : moving sum offset
	// 1: no offset
	// 2: moving sum type2
	// 3: moving sum type3
	if (ret != data->cal_data_len)
		ret = 1;
	else
		ret = cal_data[1];

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t trim_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t prox_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct proximity_data *data = (struct proximity_data *)sensor->data;
	int cal_data[2] = {0, 1};

	if (data->cal_data_len == 0)
		return -EINVAL;

	ret = sensor->funcs->open_calibration_file();
	if (ret == data->cal_data_len)
		memcpy(cal_data, data->cal_data, sizeof(cal_data));

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", cal_data[0], cal_data[1]);
}

static ssize_t prox_cal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int64_t enable = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	struct proximity_data *data = (struct proximity_data *)get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	ret = kstrtoll(buf, 10, &enable);
	if (ret < 0)
		return ret;

	if (enable) {
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
		shub_infof("%d %d", ((int *)(data->cal_data))[0], ((int *)(data->cal_data))[1]);
	} else {
		save_proximity_calibration();
		set_proximity_calibration();
	}

	return size;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(prox_led_test);
static DEVICE_ATTR_RO(trim_check);
static DEVICE_ATTR_RO(prox_trim);
static DEVICE_ATTR(prox_cal, 0660, prox_cal_show, prox_cal_store);

static struct device_attribute *proximity_tmd4912_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_prox_led_test,
	&dev_attr_prox_cal,
	&dev_attr_prox_trim,
	&dev_attr_trim_check,
	NULL,
};

struct device_attribute **get_proximity_tmd4912_dev_attrs(char *name)
{
	if (strcmp(name, TMD4912_NAME) != 0)
		return NULL;

	return proximity_tmd4912_attrs;
}
