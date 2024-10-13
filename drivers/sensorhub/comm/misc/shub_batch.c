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
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>

#include "../../debug/shub_system_checker.h"
#include "../../utility/shub_utility.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../sensormanager/shub_sensor_type.h"

struct miscdevice batch_io_device;

struct batch_config {
	int64_t timeout;
	int64_t delay;
	int flag;
};

#define BATCH_IOCTL_MAGIC       0xFC

static long shub_batch_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct batch_config batch;
	void __user *argp = (void __user *)arg;
	int retries = 2;
	int ret;
	int sensor_type;
	int sampling_period_ms, report_latency_ms = 0;

	sensor_type = (cmd & 0xFF);

	shub_infof("cmd = 0x%x, sensor_type = %d", cmd, sensor_type);

	if ((cmd >> 8 & 0xFF) != BATCH_IOCTL_MAGIC) {
		shub_err("Invalid BATCH CMD %x", cmd);
		return -EINVAL;
	}

	if (sensor_type >= SENSOR_TYPE_LEGACY_MAX) {
		shub_err("Invalid sensor_type %d", sensor_type);
		return -EINVAL;
	}

	while (retries--) {
		ret = copy_from_user(&batch, argp, sizeof(batch));
		if (likely(!ret)) {
		break;
		}
	}

	if (unlikely(ret)) {
		shub_err("batch ioctl err(%d)", ret);
		return -EINVAL;
	}

	sampling_period_ms = div_s64(batch.delay, NSEC_PER_MSEC);
	report_latency_ms = div_s64(batch.timeout, NSEC_PER_MSEC);

#ifdef CONFIG_SHUB_DEBUG
	if (is_system_checking())
		return -EINVAL;
#endif
	ret = batch_sensor(sensor_type, sampling_period_ms, report_latency_ms);

	return ret;
}

static struct file_operations shub_batch_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = shub_batch_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = shub_batch_ioctl,
#endif
};

int register_misc_dev_batch(bool en)
{
	int res = 0;

	if (en) {
		batch_io_device.minor = MISC_DYNAMIC_MINOR;
		batch_io_device.name = "batch_io";
		batch_io_device.fops = &shub_batch_fops;
		res = misc_register(&batch_io_device);
	} else {
		shub_batch_fops.unlocked_ioctl = NULL;
#ifdef CONFIG_COMPAT
		shub_batch_fops.compat_ioctl = NULL;
#endif
		misc_deregister(&batch_io_device);
	}

	return res;
}