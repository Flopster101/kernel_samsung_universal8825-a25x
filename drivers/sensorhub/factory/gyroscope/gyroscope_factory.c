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

#include "../../sensor/gyroscope.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"
#include "../../comm/shub_comm.h"
#include "gyroscope_factory.h"

#include <linux/slab.h>

#if defined(CONFIG_SHUB_KUNIT)
#include <kunit/mock.h>
#define __mockable __weak
#define __visible_for_testing
#else
#define __mockable
#define __visible_for_testing static
#endif

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define SELFTEST_REVISED 1

static struct device *gyro_sysfs_device;
static struct device_attribute **chipset_attrs;

static ssize_t power_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	shub_infof();

	return sprintf(buf, "%d\n", 1);
}

static ssize_t power_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	shub_infof();

	return sprintf(buf, "%d\n", 1);
}

static ssize_t temperature_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	unsigned char reg[2] = {0, };
	short temperature = 0;
	int ret = 0;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_GYROSCOPE, GYROSCOPE_TEMPERATURE_FACTORY,
				     3000, NULL, 0, &buffer, &buffer_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		goto exit;
	}

	if (buffer_length < 2) {
		shub_errf("length err %d", buffer_length);
		ret = -EINVAL;
		goto exit;
	}

	reg[0] = buffer[1];
	reg[1] = buffer[0];
	temperature = (short)(((reg[0]) << 8) | reg[1]);
	shub_infof("%d", temperature);

	ret = sprintf(buf, "%d\n", temperature);

exit:
	kfree(buffer);

	return ret;
}

static ssize_t selftest_dps_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!buf)
		return -EINVAL;

	return sprintf(buf, "0\n");
}

static ssize_t selftest_dps_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	shub_info("Do not support gyro dps selftest");
	return size;
}

static ssize_t selftest_revised_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (!buf)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%d\n", SELFTEST_REVISED);
}

static DEVICE_ATTR_RO(power_off);
static DEVICE_ATTR_RO(power_on);
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RO(selftest_revised);
static DEVICE_ATTR(selftest_dps, 0664, selftest_dps_show, selftest_dps_store);

__visible_for_testing struct device_attribute *gyro_attrs[] = {
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	&dev_attr_selftest_dps,
	&dev_attr_selftest_revised,
	NULL,
};

typedef struct device_attribute** (*get_chipset_dev_attrs)(char *);
get_chipset_dev_attrs get_gyro_chipset_dev_attrs[] = {
	get_gyroscope_icm42605m_dev_attrs,
	get_gyroscope_lsm6dsl_dev_attrs,
	get_gyroscope_lsm6dsotr_dev_attrs,
	get_gyroscope_lsm6dsvtr_dev_attrs,
	get_gyroscope_icm42632m_dev_attrs,
};

void initialize_gyroscope_sysfs(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);
	int ret;
	uint64_t i;

	ret = sensor_device_create(&gyro_sysfs_device, NULL, "gyro_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	ret = add_sensor_device_attr(gyro_sysfs_device, gyro_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(get_gyro_chipset_dev_attrs); i++) {
		chipset_attrs = get_gyro_chipset_dev_attrs[i](sensor->spec.name);
		if (chipset_attrs) {
			ret = add_sensor_device_attr(gyro_sysfs_device, chipset_attrs);
			if (ret < 0) {
				shub_errf("fail to add sysfs chipset device attr(%d)", (int)i);
				return;
			}
			break;
		}
	}
}

void remove_gyroscope_sysfs(void)
{
	if (chipset_attrs)
		remove_sensor_device_attr(gyro_sysfs_device, chipset_attrs);
	remove_sensor_device_attr(gyro_sysfs_device, gyro_attrs);
	sensor_device_destroy(gyro_sysfs_device);
	gyro_sysfs_device = NULL;
}

void initialize_gyroscope_factory(bool en)
{
	if (!get_sensor(SENSOR_TYPE_GYROSCOPE))
		return;
	if (en)
		initialize_gyroscope_sysfs();
	else
		remove_gyroscope_sysfs();
}
