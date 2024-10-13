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

#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../../comm/shub_cmd.h"
#include "../../sensor/scontext.h"
#include "../../sensorhub/shub_device.h"
#include "../../utility/shub_utility.h"

struct miscdevice scontext_device;

static ssize_t shub_scontext_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buffer;

	if (!is_shub_working()) {
		shub_errf("stop sending library data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		shub_errf("library data length err(%d)", (int)count);
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

	shub_scontext_log(__func__, buffer, count);

	if (buffer[0] == SCONTEXT_INST_LIB_NOTI)
		ret = shub_scontext_send_cmd(buffer, count);
	else
		ret = shub_scontext_send_instruction(buffer, count);

	if (ret < 0)
		shub_errf("send library data err(%d)", ret);

	kfree(buffer);
	return (ret == 0) ? count : ret;
}

static const struct file_operations shub_scontext_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = shub_scontext_write,
};

int register_misc_dev_scontext(bool en)
{
	int res = 0;

	if (en) {
		scontext_device.minor = MISC_DYNAMIC_MINOR;
		scontext_device.name = "shub_sensorhub";
		scontext_device.fops = &shub_scontext_fops;
		res = misc_register(&scontext_device);
	} else {
		misc_deregister(&scontext_device);
	}

	return res;
}
