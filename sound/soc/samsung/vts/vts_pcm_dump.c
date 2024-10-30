// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_pcm_dump.c
 *
 * ALSA SoC Audio Layer - Samsung VTS Internal Buffer Dumping driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <sound/samsung/abox.h>
#include <sound/samsung/vts.h>
#include <linux/pm_runtime.h>

#include "vts.h"
#include "vts_log.h"
#include "vts_dbg.h"
#include "vts_proc.h"
#include "vts_pcm_dump.h"

#define BUFFER_MAX (SZ_64)
#define NAME_LENGTH (SZ_32)

struct vts_pcm_dump_info {
	struct device *dev;
	struct list_head list;
	int id;
	char name[NAME_LENGTH];
	struct mutex lock;
	struct snd_dma_buffer buffer;
	size_t pointer;

	struct proc_dir_entry *file;
	bool file_started;
	size_t file_pointer;
	wait_queue_head_t file_waitqueue;

	bool file_created;
	struct file *filp;
};

static struct proc_dir_entry *dir_dump;
static struct device *vts_pcm_dump_dev_vts;
static struct vts_pcm_dump_info vts_pcm_dump_list[BUFFER_MAX];
static LIST_HEAD(vts_pcm_dump_list_head);

static struct vts_pcm_dump_info *vts_pcm_dump_get_info(int id)
{
	struct vts_pcm_dump_info *info;

	list_for_each_entry(info, &vts_pcm_dump_list_head, list) {
		if (info->id == id)
			return info;
	}

	return NULL;
}

static void vts_pcm_dump_request_dump(int id, bool enable)
{
	dev_dbg(vts_pcm_dump_dev_vts, "%s(%d)\n", __func__, id);
}

static ssize_t vts_pcm_dump_file_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos)
{
	struct vts_pcm_dump_info *info = file->private_data;
	struct device *dev = info->dev;
	struct vts_data *vts_data = dev_get_drvdata(dev);
	size_t end, pointer;
	ssize_t size;
	int ret;

	dev_dbg(dev, "%s(%#zx)\n", __func__, count);

	do {
		pointer = READ_ONCE(info->pointer);
		end = (info->file_pointer <= pointer) ? pointer :
				info->buffer.bytes;
		if (end > info->buffer.bytes) {
			dev_err(dev, "pointer Error=%#zx file_pointer=%#zx end=%#zx buffer.bytes=%#zx\n",
					pointer, info->file_pointer,
					end, info->buffer.bytes);
			end = info->buffer.bytes;
		}
		size = min(end - info->file_pointer, count);
		dev_dbg(dev, "pointer=%#zx file_pointer=%#zx size=%#zx\n",
				pointer, info->file_pointer, size);
		if (!size) {
			if (file->f_flags & O_NONBLOCK) {
				dev_err(dev, "%d size is 0", info->id);
				return -EAGAIN;
			}
			ret = wait_event_interruptible(info->file_waitqueue,
					pointer != READ_ONCE(info->pointer));
			if (ret < 0)
				return ret;
		}
	} while (!size);

	if (vts_data->running) {
		if (copy_to_user(data,
				info->buffer.area + info->file_pointer, size))
			return -EFAULT;

		info->file_pointer += size;
		info->file_pointer %= info->buffer.bytes;
	} else {
		dev_err(dev, "%s (%d) : Wrong VTS status RESET", __func__, info->id);
		info->file_pointer = 0;
		info->pointer = 0;
	}
	return size;
}

static int vts_pcm_dump_file_open(struct inode *i, struct file *f)
{
	struct vts_pcm_dump_info *info = vts_dump_get_data(f);
	struct device *dev = info->dev;

	dev_info(dev, "%s : %d\n", __func__, info->id);
	f->private_data = info;
	info->file_started = true;
	info->file_pointer = 0;
	info->pointer = 0;

	vts_pcm_dump_request_dump(info->id, true);
	return 0;
}

static int vts_pcm_dump_file_release(struct inode *i, struct file *f)
{
	struct vts_pcm_dump_info *info = vts_dump_get_data(f);
	struct device *dev = info->dev;

	dev_info(dev, "%s : %d\n", __func__, info->id);
	info->file_started = false;
	info->pointer = 1;
	wake_up_interruptible(&info->file_waitqueue);
	vts_pcm_dump_request_dump(info->id, false);
	return 0;
}

static unsigned int vts_pcm_dump_file_poll(struct file *file, poll_table *wait)
{
	struct vts_pcm_dump_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &info->file_waitqueue, wait);
	return POLLIN | POLLRDNORM;
}

static const struct proc_ops vts_pcm_dump_fops = {
	.proc_lseek = generic_file_llseek,
	.proc_read = vts_pcm_dump_file_read,
	.proc_poll = vts_pcm_dump_file_poll,
	.proc_open = vts_pcm_dump_file_open,
	.proc_release = vts_pcm_dump_file_release,
};

void vts_pcm_dump_period_elapsed(int id, size_t pointer)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);
	struct device *dev = info->dev;

	dev_dbg(dev, "%s[%d](%zx)\n", __func__, id, pointer);

	info->pointer = pointer;
	wake_up_interruptible(&info->file_waitqueue);
}

static void vts_pcm_dump_check_buffer(struct snd_dma_buffer *buffer)
{
	if (!buffer->bytes)
		buffer->bytes = SZ_64K * 5;
	if (!buffer->area) {
		/* area will be used in kernel only.
		 * kmalloc and virt_to_phys are just enough.
		 */
		buffer->area = devm_kmalloc(vts_pcm_dump_dev_vts, buffer->bytes,
				GFP_KERNEL);
		buffer->addr = virt_to_phys(buffer->area);
	}
}

struct proc_dir_entry *vts_pcm_dump_register_file(const char *name, void *data,
		const struct proc_ops *fops)
{
	if (IS_ERR_OR_NULL(dir_dump))
		dir_dump = vts_proc_mkdir("dump", NULL);

	return vts_proc_create_file(name, 0664, dir_dump, fops, data, 0);
}

void vts_pcm_dump_unregister_file(struct proc_dir_entry *file)
{
	vts_proc_remove_file(file);
}

void *vts_dump_get_data(const struct file *file)
{
	return vts_proc_data(file);
}

void vts_pcm_dump_register_work_func(struct work_struct *work)
{
	int id;
	struct vts_pcm_dump_info *info;

	for (info = &vts_pcm_dump_list[0]; (info - &vts_pcm_dump_list[0]) <
			ARRAY_SIZE(vts_pcm_dump_list); info++) {
		id = info->id;
		if (info->dev && !vts_pcm_dump_get_info(id)) {
			vts_pcm_dump_check_buffer(&info->buffer);
			dev_dbg(info->dev, "%s(%d, %s, %#zx)\n", __func__,
					id, info->name, info->buffer.bytes);
			info->file = vts_pcm_dump_register_file(info->name,
				info, &vts_pcm_dump_fops);
			init_waitqueue_head(&info->file_waitqueue);
			list_add_tail(&info->list, &vts_pcm_dump_list_head);
		}
	}
}

static DECLARE_WORK(vts_pcm_dump_register_work,
	vts_pcm_dump_register_work_func);

int vts_pcm_dump_register(struct device *dev, int id, const char *name,
		void *area, phys_addr_t addr, size_t bytes)
{
	struct vts_pcm_dump_info *info;

	dev_info(dev, "%s[%d](%s, %#zx)\n", __func__, id, name, bytes);

	if (id < 0 || id >= ARRAY_SIZE(vts_pcm_dump_list)) {
		dev_err(dev, "invalid id: %d\n", id);
		return -EINVAL;
	}

	info = &vts_pcm_dump_list[id];
	if (!strcmp(info->name, name)) {
		dev_dbg(dev, "already registered dump: %d\n", id);
		return 0;
	}

	mutex_init(&info->lock);
	info->id = id;
	strncpy(info->name, name, sizeof(info->name) - 1);
	info->buffer.area = area;
	info->buffer.addr = addr;
	info->buffer.bytes = bytes;
	vts_pcm_dump_dev_vts = info->dev = dev;
	schedule_work(&vts_pcm_dump_register_work);

	return 0;
}

void vts_pcm_dump_init(struct device *dev_vts)
{
	dev_dbg(dev_vts, "%s\n", __func__);
	vts_pcm_dump_dev_vts = dev_vts;
}

bool vts_pcm_dump_get_file_started(int id)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);

	return info->file_started;
}

void vts_pcm_dump_reset(int id)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);

	dev_info(vts_pcm_dump_dev_vts, "%s by %d", __func__, id);
	info->file_pointer = 0;
	info->pointer = 0;
}

