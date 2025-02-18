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

#define STK3X6X_NAME "STK33617"
#define STK3X6X_VENDOR "Sitronix"

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", STK3X6X_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", STK3X6X_VENDOR);
}


static void do_proximity_calibration(void)
{
	shub_infof("");
	batch_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION, 20, 0);
	enable_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION, NULL, 0);
}

static ssize_t proximity_cal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	u8 temp;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
	struct proximity_stk3x6x_data *thd_data = data->threshold_data;

	shub_infof("%s", buf);

	ret = kstrtou8(buf, 10, &temp);
	if (ret < 0) {
		shub_errf("- failed = %d", ret);
		return size;
	}

	if (temp == 1 || temp == 2) {
		thd_data->prox_cal_mode = temp;
		do_proximity_calibration();
	}

	return size;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR(prox_cal, 0220, NULL, proximity_cal_store);

static struct device_attribute *proximity_stk3x6x_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_prox_cal,
	NULL,
};

struct device_attribute **get_proximity_stk3x6x_dev_attrs(char *name)
{
	if (strcmp(name, STK3X6X_NAME) != 0)
		return NULL;

	return proximity_stk3x6x_attrs;
}
