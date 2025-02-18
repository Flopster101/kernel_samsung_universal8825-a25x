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
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "../../sensor/proximity.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"
#include "../../utility/shub_file_manager.h"
#include "../../sensorhub/shub_device.h"
#include "proximity_factory.h"

#define GP2AP110S_NAME    "GP2AP110S"
#define GP2AP110S_VENDOR  "SHARP"

#define PROX_SETTINGS_FILE_PATH     "/efs/FactoryApp/prox_settings"

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", GP2AP110S_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", GP2AP110S_VENDOR);
}

static ssize_t proximity_modify_settings_show(struct device *dev,
					      struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct proximity_data *data = sensor->data;

	sensor->funcs->open_calibration_file();
	return snprintf(buf, PAGE_SIZE, "%d\n", data->setting_mode);
}

static ssize_t proximity_modify_settings_store(struct device *dev,
					       struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	u8 mode;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	shub_infof("%s\n", buf);

	ret = kstrtou8(buf, 10, &mode);
	if (ret < 0)
		return ret;

	if (mode <= 0 || mode > 2) {
		shub_errf("invalid value %d", mode);
		return -EINVAL;
	}

	shub_infof("prox_setting %d", mode);
	data->setting_mode = mode;

	ret = save_proximity_setting_mode();
	if (mode == 2)
		memcpy(data->prox_threshold, thd_data->prox_mode_thresh, sizeof(data->prox_threshold));

	msleep(150);

	return size;
}

static ssize_t proximity_settings_thresh_high_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", thd_data->prox_setting_thresh[PROX_THRESH_HIGH]);
}

static ssize_t proximity_settings_thresh_high_store(struct device *dev, struct device_attribute *attr, const char *buf,
						    size_t size)
{
	int ret;
	u16 settings_thresh;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	ret = kstrtou16(buf, 10, &settings_thresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed.(%d)", ret);
		return -EINVAL;
	}

	thd_data->prox_setting_thresh[PROX_THRESH_HIGH] = settings_thresh;

	shub_infof("new prox setting high threshold %u", thd_data->prox_setting_thresh[PROX_THRESH_HIGH]);

	return size;
}

static ssize_t proximity_settings_thresh_low_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", thd_data->prox_setting_thresh[PROX_THRESH_LOW]);
}

static ssize_t proximity_settings_thresh_low_store(struct device *dev, struct device_attribute *attr, const char *buf,
						   size_t size)
{
	int ret;
	u16 settings_thresh;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_gp2ap110s_data *thd_data = data->threshold_data;

	ret = kstrtou16(buf, 10, &settings_thresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed.(%d)", ret);
		return -ENOENT;
	}

	thd_data->prox_setting_thresh[PROX_THRESH_LOW] = settings_thresh;

	shub_infof("new prox setting low threshold %u", thd_data->prox_setting_thresh[PROX_THRESH_LOW]);

	return size;
}

static ssize_t prox_trim_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_length = 0;
	int trim = 0;

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
		trim = (buffer[0]) * (-1);
	else
		trim = buffer[0];

	shub_infof("%d, 0x%x, 0x%x", trim, buffer[1], buffer[0]);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", trim);

	kfree(buffer);

	return ret;
}


static ssize_t prox_cal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	int result = 0;

	ret = kstrtoint(buf, 10, &result);
	if (ret < 0)
		shub_errf("kstrtoint failed. %d", ret);

	return size;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(prox_trim);
static DEVICE_ATTR_WO(prox_cal);
static DEVICE_ATTR(modify_settings, 0664, proximity_modify_settings_show, proximity_modify_settings_store);
static DEVICE_ATTR(settings_thd_high, 0664, proximity_settings_thresh_high_show, proximity_settings_thresh_high_store);
static DEVICE_ATTR(settings_thd_low, 0664, proximity_settings_thresh_low_show, proximity_settings_thresh_low_store);

static struct device_attribute *proximity_gp2ap110s_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_modify_settings,
	&dev_attr_settings_thd_high,
	&dev_attr_settings_thd_low,
	&dev_attr_prox_trim,
	&dev_attr_prox_cal,
	NULL,
};

struct device_attribute **get_proximity_gp2ap110s_dev_attrs(char *name)
{
	if (strcmp(name, GP2AP110S_NAME) != 0)
		return NULL;

	return proximity_gp2ap110s_attrs;
}
