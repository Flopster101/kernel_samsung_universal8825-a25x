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

#include <linux/device.h>

#include "shub_utility.h"
#include "../sensorhub/shub_device.h"

extern struct class *sensors_class;

int sensor_device_create(struct device **pdev, void *drvdata, char *name)
{
	struct device* dev;

	if (!sensors_class) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class)) {
			return PTR_ERR(sensors_class);
		}
	}

	if (*pdev)
		return 0;

	dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(dev)) {
		int ret = 0;
		ret = PTR_ERR(dev);
		shub_errf("device_create failed! ret = %d", ret);
		return ret;
	} else {
		*pdev = dev;
	}

	return 0;
}

int add_sensor_device_attr(struct device *dev, struct device_attribute *attributes[])
{
	int i, ret = 0, err_idx = -1;

	for (i = 0; attributes[i] != NULL; i++) {
		ret = device_create_file(dev, attributes[i]);
		if (ret < 0) {
			shub_errf("%s[%d] failed ret = %d", dev->init_name, i, ret);
			err_idx = i;
			break;
		}
	}

	if (err_idx >= 0) {
		for (i = 0; i < err_idx ; i++)
			device_remove_file(dev, attributes[i]);
	}

	return ret;
}

int add_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[])
{
	int i, ret, err_idx = -1;

	for (i = 0; attributes[i] != NULL; i++) {
		ret = device_create_bin_file(dev, attributes[i]);
		if (ret < 0) {
			shub_errf("%s[%d] failed ret = %d", dev->init_name, i, ret);
			err_idx = i;
			break;
		}
	}

	if (err_idx >= 0) {
		for (i = 0; i < err_idx ; i++)
			device_remove_bin_file(dev, attributes[i]);
	}

	return ret;
}

void remove_sensor_device_attr(struct device *dev, struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++) {
		device_remove_file(dev, attributes[i]);
	}
}

void remove_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++) {
		device_remove_bin_file(dev, attributes[i]);
	}
}

void sensor_device_destroy(struct device *dev)
{
	//device_destroy(sensors_class, dev->devt);
	// device_create 할떄 dev_t 안넣어줘서 destroy 못함
	// need to check again, not probed sensor device have to be destroyed
}