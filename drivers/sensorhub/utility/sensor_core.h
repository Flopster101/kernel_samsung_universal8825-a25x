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

/*
 *  Universal sensors core class
 *
 *  Author : Ryunkyun Park <ryun.park@samsung.com>
 */

#ifndef __SENSORS_CORE_H__
#define __SENSORS_CORE_H__


#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/input.h>

int sensors_create_symlink(struct input_dev *inputdev);
void sensors_remove_symlink(struct input_dev *inputdev);
int sensors_register(struct device **pdev, void *drvdata,
                     struct device_attribute *attributes[], char *name);

int sensors_device_register(struct device **pdev, void *drvdata,
                     struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
                        struct device_attribute *attributes[]);
void destroy_sensor_class(void);

extern int sensordump_notifier_register(struct notifier_block *nb);
extern int sensordump_notifier_unregister(struct notifier_block *nb);
extern int sensordump_notifier_call_chain(unsigned long val, void *v);

#endif
