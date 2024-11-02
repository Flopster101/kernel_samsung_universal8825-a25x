/*
 *  Copyright (C) 2018, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "../sensorhub/shub_device.h"
#include "../comm/shub_iio.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../vendor/shub_vendor.h"

#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define SENSORHUB_DUMP_NOTI_EVENT 0xFC

char *sensorhub_dump;
int sensorhub_dump_size;

static ssize_t shub_dump_read(struct file *file, struct kobject *kobj, struct bin_attribute *battr, char *buf,
			      loff_t off, size_t size)
{
	memcpy_fromio(buf, battr->private + off, size);
	return size;
}

BIN_ATTR_RO(shub_dump, 0);

struct bin_attribute *shub_dump_bin_attrs[] = {
	&bin_attr_shub_dump,
};

static int create_dump_bin_file(void)
{
	int ret;
	uint64_t i;
	struct shub_data_t *data = get_shub_data();

	shub_infof();

	sensorhub_dump = vzalloc(sensorhub_dump_size);
	if (ZERO_OR_NULL_PTR(sensorhub_dump)) {
		shub_infof("fail to alloc memory");
		return -EINVAL;
	}

	shub_infof("dump size %d", sensorhub_dump_size);

	bin_attr_shub_dump.size = sensorhub_dump_size;
	bin_attr_shub_dump.private = sensorhub_dump;

	for (i = 0; i < ARRAY_SIZE(shub_dump_bin_attrs); i++) {
		struct bin_attribute *battr = shub_dump_bin_attrs[i];

		ret = device_create_bin_file(data->sysfs_dev, battr);
		if (ret < 0) {
			shub_errf("Failed to create file: %s %d", battr->attr.name, ret);
			break;
		}
	}

	if (ret < 0) {
		kvfree(sensorhub_dump);
		sensorhub_dump = NULL;
		sensorhub_dump_size = 0;
	}

	return ret;
}

void write_shub_dump_file(char *dump, int dumpsize, int type, int count)
{
	char buffer[3];

	if (!dump) {
		shub_errf("dump is NULL");
		return;
	} else if (dumpsize != sensorhub_dump_size) {
		shub_errf("dump size is wrong %d(%d)", dumpsize, sensorhub_dump_size);
		return;
	}

	memcpy_fromio(sensorhub_dump, dump, sensorhub_dump_size);

	shub_infof("%d %d", type, count);

	buffer[0] = SENSORHUB_DUMP_NOTI_EVENT;
	buffer[1] = type;
	buffer[2] = count;
	shub_report_sensordata(SENSOR_TYPE_SENSORHUB, get_current_timestamp(), buffer, sizeof(buffer));
}

void initialize_shub_dump(void)
{
	shub_infof();

	sensorhub_dump_size = sensorhub_get_dump_size();
	create_dump_bin_file();
}

void remove_shub_dump(void)
{
	uint64_t i;
	struct shub_data_t *data = get_shub_data();

	if (ZERO_OR_NULL_PTR(sensorhub_dump))
		return;

	kvfree(sensorhub_dump);
	for (i = 0; i < ARRAY_SIZE(shub_dump_bin_attrs); i++)
		device_remove_bin_file(data->sysfs_dev, shub_dump_bin_attrs[i]);
}
