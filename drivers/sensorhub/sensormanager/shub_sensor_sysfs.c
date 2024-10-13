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

#include "../sensormanager/shub_sensor_manager.h"
#include "shub_sensor_type.h"
#include "../sensormanager/shub_sensor.h"

#include "../debug/shub_system_checker.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_dev_core.h"
#include "../utility/sensor_core.h"
#include "../sensorhub/shub_device.h"

static ssize_t show_sensors_enable(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	int i;
	uint64_t res = 0;

	for (i = 0; i < SENSOR_TYPE_LEGACY_MAX; i++) {
		if (get_sensor_enabled(i))
			res |= (1ULL << i);
	}

	return snprintf(buf, PAGE_SIZE, "%llu\n", res);
}

static ssize_t set_sensors_enable(struct device *dev,
                                  struct device_attribute *attr, const char *buf, size_t size)
{
	int type = 0;
	bool new_enable = 0;
	int ret = 0, temp = 0;

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		return -EINVAL;
#endif

	if (kstrtoint(buf, 10, &temp) < 0)
		return -EINVAL;

	type = temp / 10;
	new_enable = temp % 10;

	if (type >= SENSOR_TYPE_LEGACY_MAX || (temp % 10) > 1) {
		shub_errf("type = (%d) or new_enable = (%d) is wrong.", type, (temp % 10));
		return -EINVAL;
	}
	ret = new_enable ? enable_sensor(type, 0, 0) : disable_sensor(type, 0, 0);

	return (ret == 0) ? size : ret;
}

static ssize_t set_flush(struct device *dev,
                         struct device_attribute *attr, const char *buf, size_t size)
{
	int type, ret;

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		return -EINVAL;
#endif
	if (kstrtoint(buf, 10, &type) < 0)
		return -EINVAL;

	ret = flush_sensor(type);

	return (ret == 0) ? size : ret;
}

static ssize_t show_sensor_spec(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	shub_infof("");
	if (!is_shub_working()) {
		shub_errf("sensor hub is not working");
		return -EINVAL;
	}

	return get_total_sensor_spec(buf);
}

static ssize_t scontext_list_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return get_sensors_scontext_probe_state((uint64_t *)buf);
}

static ssize_t wakeup_reason_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return get_bigdata_wakeup_reason(buf);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
                   show_sensors_enable, set_sensors_enable);
static DEVICE_ATTR(ssp_flush, S_IWUSR | S_IWGRP,
                   NULL, set_flush);
static DEVICE_ATTR(sensor_spec, S_IRUGO, show_sensor_spec, NULL);
static DEVICE_ATTR_RO(scontext_list);
static DEVICE_ATTR_RO(wakeup_reason);

static struct device_attribute *shub_sensor_attrs[] = {
	&dev_attr_enable,
	&dev_attr_ssp_flush,
	&dev_attr_sensor_spec,
	&dev_attr_scontext_list,
	&dev_attr_wakeup_reason,
	NULL,
};

int init_shub_sensor_sysfs(void)
{
	struct shub_data_t *data = get_shub_data();
	int ret;

	ret = sensor_device_create(&data->sysfs_dev, data, "ssp_sensor");
	if (ret < 0) {
		shub_errf("fail to creat ssp_sensor device");
		return ret;
	}

	ret = add_sensor_device_attr(data->sysfs_dev, shub_sensor_attrs);
	if (ret < 0) {
		shub_errf("fail to add shub sensor attr");
	}

	return ret;
}

void remove_shub_sensor_sysfs(void)
{
	struct shub_data_t *data = get_shub_data();
	remove_sensor_device_attr(data->sysfs_dev, shub_sensor_attrs);
	sensor_device_destroy(data->sysfs_dev);
}
