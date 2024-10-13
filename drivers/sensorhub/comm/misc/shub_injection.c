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

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../utility/shub_utility.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor_manager.h"

struct miscdevice injection_device;

#define INJECTION_MODE_SENSOR_DATA              0
#define INJECTION_MODE_ADDITIONAL_INFO          1

static ssize_t shub_injection_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buffer;

	if (!is_shub_working()) {
		shub_errf("stop inject data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		shub_errf("inject data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);
	if (!buffer) {
		shub_errf("fail to alloc memory");
		return -ENOMEM;
	}

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		shub_errf("memcpy for kernel buffer err");
		kfree(buffer);
		return -EFAULT;
	}

	if (buffer[0] == INJECTION_MODE_ADDITIONAL_INFO) {
		int type = (int)buffer[1];

		if (count > 2)
			ret = inject_sensor_additional_data(type, &buffer[2], count-2);
		else {
			shub_errf("type %d buffer length(%d) err", type, (int)count-2);
			ret = -EINVAL;
		}
	} else {
		shub_infof("do not support 'inject sensor data'");
		ret = -EINVAL;
	}

	if (ret < 0)
		shub_errf("inject data err(%d)", ret);

	kfree(buffer);

	return (ret == 0) ? count : ret;
}

static const struct file_operations shub_injection_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = shub_injection_write,
};

int register_misc_dev_injection(bool en)
{
	int res = 0;

	if (en) {
		injection_device.minor = MISC_DYNAMIC_MINOR;
		injection_device.name = "shub_data_injection";
		injection_device.fops = &shub_injection_fops;
		res = misc_register(&injection_device);
	} else {
		misc_deregister(&injection_device);
	}

	return res;
}
