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

#ifndef _SHUB_DEV_CORE_H__
#define _SHUB_DEV_CORE_H__

#include <linux/device.h>

int add_sensor_device_attr(struct device *dev, struct device_attribute *attributes[]);
int add_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[]);
void remove_sensor_device_attr(struct device *dev, struct device_attribute *attributes[]);
void remove_sensor_bin_attr(struct device *dev, struct bin_attribute *attributes[]);
int sensor_device_create(struct device **pdev, void *drvdata, char *name);
void sensor_device_destroy(struct device *dev);

#endif