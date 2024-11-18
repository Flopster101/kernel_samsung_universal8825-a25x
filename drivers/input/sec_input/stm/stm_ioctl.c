// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"

#ifdef RAWDATA_IOCTL
#include "stm_reg.h"
#include <linux/miscdevice.h>
#include <linux/mutex.h>

struct tsp_ioctl {
	int num;
	u8 data[PAGE_SIZE];
};

static struct mutex lock;

#define IOCTL_TSP_MAP_READ		_IOR(0, 0, struct tsp_ioctl)
#define IOCTL_TSP_MAP_WRITE		_IOW(0, 0, struct tsp_ioctl)
#define IOCTL_TSP_MAP_WRITE_TEST_1	_IOW('T', 1, struct tsp_ioctl)

static long tsp_ioctl_handler(struct file *file, unsigned int cmd, void __user *p, int compat_mode)
{
	static struct tsp_ioctl t;
	u8 *copier;
	int total;

	if (!g_ts->raw_pool[0] || !g_ts->raw_pool[1] || !g_ts->raw_pool[2]) {
		input_err(true, &g_ts->client->dev, "%s: is not allocated\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&lock);

	if (cmd == IOCTL_TSP_MAP_READ) {
#if 0
		input_info(true, &g_ts->client->dev, "%s: [w] %d, [r] %d\n", __func__,
			g_ts->raw_write_index, g_ts->raw_read_index);
#endif
		mutex_lock(&g_ts->raw_lock);
		t.num = g_ts->raw_write_index - g_ts->raw_read_index;
		mutex_unlock(&g_ts->raw_lock);
		if (t.num == 0) {
			if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
				input_err(true, &g_ts->client->dev, "%s: failed to 0 copy_to_user\n",
					__func__);
				mutex_unlock(&lock);
				return -EFAULT;
			} else {
				mutex_unlock(&lock);
				return 0;
			}
		} else if (t.num < 0) {
			mutex_lock(&g_ts->raw_lock);
			t.num = RAW_VEC_NUM - g_ts->raw_read_index + g_ts->raw_write_index;
			mutex_unlock(&g_ts->raw_lock);
		}

		if (t.num > 1) {
			mutex_lock(&g_ts->raw_lock);
			t.num = 1;
			mutex_unlock(&g_ts->raw_lock);
		}

		for (total = 0; total < t.num; total++) {
			mutex_lock(&g_ts->raw_lock);
			copier = g_ts->raw_pool[g_ts->raw_read_index];
			memcpy(&t.data[g_ts->raw_len * total], copier, g_ts->raw_len);
			g_ts->raw_read_index++;
			if (g_ts->raw_read_index >= 3)
				g_ts->raw_read_index = 0;
			mutex_unlock(&g_ts->raw_lock);
		}

		if (copy_to_user(p, (void *)&t, sizeof(struct tsp_ioctl))) {
			input_err(true, &g_ts->client->dev, "%s: failed to copyt_to_user\n",
				__func__);
			mutex_unlock(&lock);
			return -EFAULT;
		}
	} else if (cmd == IOCTL_TSP_MAP_WRITE_TEST_1) {
		if (copy_from_user((void *)&t, p, sizeof(struct tsp_ioctl))) {
			input_err(true, &g_ts->client->dev, "%s: failed to copyt_from_user\n", __func__);
			mutex_unlock(&lock);
			return -EFAULT;
		}
		input_info(true, &g_ts->client->dev, "%s: TEST_1, %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
				t.data[0], t.data[1], t.data[2], t.data[3], t.data[4], t.data[5],
				t.data[6], t.data[7], t.data[8], t.data[9], t.data[10], t.data[11]);
	}

	mutex_unlock(&lock);
	return 0;
}

static long tsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return tsp_ioctl_handler(file, cmd, (void __user *)arg, 0);
}

static int tsp_open(struct inode *inode, struct file *file)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);
	return 0;
}

static int tsp_close(struct inode *inode, struct file *file)
{
	input_info(true, &g_ts->client->dev, "%s\n", __func__);

	g_ts->raw_write_index++;
	if (g_ts->raw_write_index >= RAW_VEC_NUM)
		g_ts->raw_write_index = 0;
	sysfs_notify(&g_ts->sec.fac_dev->kobj, NULL, "raw_irq");

	return 0;
}

static const struct file_operations tsp_io_fos = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tsp_ioctl,
	.open = tsp_open,
	.release = tsp_close,
};

static struct miscdevice tsp_misc = {
	.fops = &tsp_io_fos,
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tspio",
};

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);

static ssize_t raw_irq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d", g_ts->raw_write_index, g_ts->raw_read_index);
}

static DEVICE_ATTR_RO(raw_irq);

static struct attribute *rawdata_attrs[] = {
	&dev_attr_raw_irq.attr,
	NULL,
};

static struct attribute_group rawdata_attr_group = {
	.attrs = rawdata_attrs,
};

int stm_ts_rawdata_buffer_alloc(struct stm_ts_data *ts)
{
	ts->raw_pool[0] = vmalloc(ts->raw_len);
	if (!ts->raw_pool[0])
		goto alloc_out;
	ts->raw_pool[1] = vmalloc(ts->raw_len);
	if (!ts->raw_pool[1])
		goto alloc_out;
	ts->raw_pool[2] = vmalloc(ts->raw_len);
	if (!ts->raw_pool[2])
		goto alloc_out;

	ts->raw_u8 = kzalloc(ts->raw_len, GFP_KERNEL);
	if (!ts->raw_u8)
		goto alloc_out;
	ts->raw = kzalloc(ts->raw_len, GFP_KERNEL);
	if (!ts->raw)
		goto alloc_out;

	return 0;

alloc_out:
	if (ts->raw_pool[0])
		vfree(ts->raw_pool[0]);
	if (ts->raw_pool[1])
		vfree(ts->raw_pool[1]);
	if (ts->raw_pool[2])
		vfree(ts->raw_pool[2]);
	if (ts->raw_u8)
		kfree(ts->raw_u8);
	if (ts->raw)
		kfree(ts->raw);
	ts->raw_pool[0] = ts->raw_pool[1] = ts->raw_pool[2] = NULL;
	ts->raw_u8 =  NULL;
	ts->raw = NULL;
	return 0;
}


int stm_ts_rawdata_init(struct stm_ts_data *ts)
{
	int ret;

	ts->raw_len = PAGE_SIZE;
	ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &rawdata_attr_group);
	input_info(true, &ts->client->dev, "%s: sysfs_create_group: ret: %d\n", __func__, ret);

	mutex_init(&lock);

	ret = misc_register(&tsp_misc);
	input_info(true, &ts->client->dev, "%s: misc_register: ret: %d\n", __func__, ret);
	return 0;
}

void stm_ts_rawdata_buffer_remove(struct stm_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);
	if (ts->raw_pool[0])
		vfree(ts->raw_pool[0]);
	if (ts->raw_pool[1])
		vfree(ts->raw_pool[1]);
	if (ts->raw_pool[2])
		vfree(ts->raw_pool[2]);
	if (ts->raw_u8)
		kfree(ts->raw_u8);
	if (ts->raw)
		kfree(ts->raw);

	ts->raw_pool[0] = ts->raw_pool[1] = ts->raw_pool[2] = NULL;
	ts->raw_u8 =  NULL;
	ts->raw = NULL;
}
#endif //#ifdef RAWDATA_MMAP

