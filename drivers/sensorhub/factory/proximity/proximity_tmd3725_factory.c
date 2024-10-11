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

#define TMD3725_NAME   "TMD3725"
#define TMD3725_VENDOR "AMS"

static int prox_trim;

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", TMD3725_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", TMD3725_VENDOR);
}

static ssize_t thresh_detect_high_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;

	shub_infof("high detect - %u", thd_data->prox_thresh_detect[PROX_THRESH_HIGH]);

	return sprintf(buf, "%u\n", thd_data->prox_thresh_detect[PROX_THRESH_HIGH]);
}

static ssize_t thresh_detect_high_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i = 0;
	u16 uNewThresh;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;

	while (i < PROX_ADC_BITS_NUM) {
		prox_bits_mask += (1 << i++);
	}

	while (i < 16) {
		prox_non_bits_mask += (1 << i++);
	}

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed - %d", ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			shub_errf("allow %ubits - %d", PROX_ADC_BITS_NUM, uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			thd_data->prox_thresh_detect[PROX_THRESH_HIGH] = uNewThresh;
		}
	}

	shub_infof("new prox threshold : high detect - %u", thd_data->prox_thresh_detect[PROX_THRESH_HIGH]);

	return ret;
}

static ssize_t thresh_detect_low_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;

	shub_infof("low detect - %u", thd_data->prox_thresh_detect[PROX_THRESH_LOW]);

	return sprintf(buf, "%u\n", thd_data->prox_thresh_detect[PROX_THRESH_LOW]);
}

static ssize_t thresh_detect_low_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i = 0;
	u16 uNewThresh;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_tmd3725_data *thd_data = data->threshold_data;

	while (i < PROX_ADC_BITS_NUM) {
		prox_bits_mask += (1 << i++);
	}

	while (i < 16) {
		prox_non_bits_mask += (1 << i++);
	}

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed - %d", ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			shub_errf("allow %ubits - %d", PROX_ADC_BITS_NUM, uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			thd_data->prox_thresh_detect[PROX_THRESH_LOW] = uNewThresh;
		}
	}

	shub_infof("new prox threshold : low detect - %u", thd_data->prox_thresh_detect[PROX_THRESH_LOW]);

	return ret;
}

static ssize_t prox_trim_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY) || !is_shub_working()) {
		shub_infof("proximity sensor is not connected");
		return -EINVAL;
	}

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_OFFSET, 1000, NULL, 0, &buffer,
				     &buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	if (buffer_length != 2) {
		shub_errf("buffer length error %d", buffer_length);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		if (buffer != NULL)
			kfree(buffer);

		return -EINVAL;
	}

	if (buffer[1] > 0)
		prox_trim = (buffer[0]) * (-1);
	else
		prox_trim = buffer[0];

	shub_infof("%d, 0x%x, 0x%x", prox_trim, buffer[1], buffer[0]);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", prox_trim);

	kfree(buffer);

	return ret;
}

static ssize_t prox_trim_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", prox_trim);
}

static ssize_t prox_cal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY) || !is_shub_working()) {
		shub_infof("proximity sensor is not connected");
		return -EINVAL;
	}

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_OFFSET, 1000, NULL, 0, &buffer,
				     &buffer_length, true);
	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		return ret;
	}

	if (buffer_length != 4) {
		shub_errf("buffer length error %d", buffer_length);
		ret = snprintf(buf, PAGE_SIZE, "-1,0,0,0,0,0,0,0,0,0,0\n");
		if (buffer != NULL)
			kfree(buffer);

		return -EINVAL;
	}

	if (buffer[1] > 0)
		prox_trim = (buffer[0]) * (-1);
	else
		prox_trim = buffer[0];

	shub_infof("%d, 0x%x, 0x%x, (%d,%d)", prox_trim, buffer[1], buffer[0], buffer[2], buffer[3]);

	ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", prox_trim, buffer[2], buffer[3]);

	kfree(buffer);

	return ret;
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
static DEVICE_ATTR(thresh_detect_high, 0664, thresh_detect_high_show, thresh_detect_high_store);
static DEVICE_ATTR(thresh_detect_low, 0664, thresh_detect_low_show, thresh_detect_low_store);
static DEVICE_ATTR_RO(prox_trim);
static DEVICE_ATTR_RO(prox_trim_check);
static DEVICE_ATTR(prox_cal, 0660, prox_cal_show, prox_cal_store);

static struct device_attribute *proximity_tmd3725_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
	&dev_attr_prox_cal,
	&dev_attr_prox_trim,
	&dev_attr_prox_trim_check,
	NULL,
};

struct device_attribute **get_proximity_tmd3725_dev_attrs(char *name)
{
	if (strcmp(name, TMD3725_NAME) != 0)
		return NULL;

	return proximity_tmd3725_attrs;
}
