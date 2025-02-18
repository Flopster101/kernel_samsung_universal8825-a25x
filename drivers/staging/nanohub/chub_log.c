// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IF Driver Log
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "chub_log.h"
#include "chub_dbg.h"
#include "chub_exynos.h"
#include "ipc_chub.h"

/* cat /d/nanohub/log */
#define NAME_PREFIX "nanohub-ipc"
#define SIZE_OF_BUFFER (SZ_2M - SZ_128)
#define LOGFILE_NUM_SIZE (26)
#define S_IRWUG (0660)
#define DEFAULT_FLUSH_MS (1000)

static struct dentry *dbg_root_dir __read_mostly;
static LIST_HEAD(log_list_head);
static struct log_buffer_info *print_info;

static int handle_log_kthread(void *arg)
{
	struct contexthub_ipc_info *chub = (struct contexthub_ipc_info *)arg;

	while (!kthread_should_stop()) {
		wait_event(chub->log_kthread_wait,
			   (!atomic_read(&chub->chub_sleep) &&
			   atomic_read(&chub->chub_status) == CHUB_ST_RUN &&
			   ipc_logbuf_filled()) || atomic_read(&chub->in_reset));
		if (atomic_read(&chub->in_reset))
			continue;
		if (contexthub_get_token(chub)) { // lock for chub reset
			chub_wait_event(&chub->reset_lock, WAIT_TIMEOUT_MS * 2);
			continue;
		}
		atomic_set(&chub->in_log, 1); // lock for AP suspend(power gating)
		ipc_logbuf_flush_on(1); // lock for ipc (chub and AP)
		if (ipc_logbuf_outprint(chub))
			chub->err_cnt[CHUB_ERR_NANOHUB_LOG]++;
		ipc_logbuf_flush_on(0);
		contexthub_put_token(chub);
		atomic_set(&chub->in_log, 0);
		chub_wake_event(&chub->log_lock);
	}
	return 0;
}

static int log_file_open(struct inode *inode, struct file *file)
{
	struct log_buffer_info *info = inode->i_private;

	dev_dbg(info->dev, "%s\n", __func__);

	file->private_data = inode->i_private;
	info->log_file_index = -1;

	return 0;
}

static ssize_t log_file_read(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct log_buffer_info *info = file->private_data;
	struct runtimelog_buf *rt_buf = info->rt_log;
	size_t end, size;
	bool first = (info->log_file_index < 0);
	int result;

	dev_dbg(info->dev, "%s(%zu, %lld)\n", __func__, count, *ppos);

	mutex_lock(&info->lock);
	if (info->log_file_index < 0) {
		info->log_file_index =
		    likely(rt_buf->wrap) ? rt_buf->write_index : 0;
	}

	do {
		end = ((info->log_file_index < rt_buf->write_index) ||
		       ((info->log_file_index == rt_buf->write_index) && !first)) ?
			rt_buf->write_index : rt_buf->buffer_size;
		size = min(end - info->log_file_index, count);
		if (size == 0) {
			mutex_unlock(&info->lock);
			if (file->f_flags & O_NONBLOCK) {
				dev_dbg(info->dev, "non block\n");
				return -EAGAIN;
			}
			rt_buf->updated = false;

			result = wait_event_interruptible(rt_buf->wq, rt_buf->updated);
			if (result != 0) {
				dev_dbg(info->dev, "interrupted\n");
				return result;
			}
			mutex_lock(&info->lock);
		}
	} while (size == 0);

	dev_dbg(info->dev, "start=%zd, end=%zd size=%zd\n",
		info->log_file_index, end, size);
	if (copy_to_user
	    (buf, rt_buf->buffer + info->log_file_index, size)) {
		mutex_unlock(&info->lock);
		return -EFAULT;
	}

	info->log_file_index += size;
	if (info->log_file_index >= SIZE_OF_BUFFER)
		info->log_file_index = 0;

	mutex_unlock(&info->lock);
	dev_dbg(info->dev, "%s: size = %zd\n", __func__, size);

	return size;
}

static unsigned int log_file_poll(struct file *file, poll_table *wait)
{
	struct log_buffer_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &info->rt_log->wq, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations log_fops = {
	.open = log_file_open,
	.read = log_file_read,
	.poll = log_file_poll,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

static struct dentry *chub_dbg_get_root_dir(void)
{
	if (!dbg_root_dir)
		dbg_root_dir = debugfs_create_dir("nanohub", NULL);

	return dbg_root_dir;
}

static struct log_buffer_info *log_register_buffer
	(struct device *dev, int id, struct runtimelog_buf *rt_log, char *name, bool sram)
{
	struct log_buffer_info *info;

	if (!rt_log) {
		dev_warn(dev, "%s NULL rt_log", __func__);
		return NULL;
	}

	info = vmalloc(sizeof(struct log_buffer_info));
	if (!info) {
		dev_warn(dev, "%s info malloc fail", __func__);
		return NULL;
	}

	mutex_init(&info->lock);
	info->id = id;
	info->file_created = false;
	init_waitqueue_head(&rt_log->wq);
	info->dev = dev;
	info->rt_log = rt_log;

	info->save_file_name[0] = '\0';
	info->filp = NULL;

	debugfs_create_file(name, S_IRWUG, chub_dbg_get_root_dir(), info,
			    &log_fops);

	list_add_tail(&info->list, &log_list_head);

	print_info = info;
	info->sram_log_buffer = false;
	info->support_log_save = false;

	return info;
}

int logbuf_printf(void *chub_p, void *log_p, u32 len)
{
	u32 lenout = 0;
	struct contexthub_ipc_info *chub = chub_p;
	struct logbuf_output *log = log_p;
	struct runtimelog_buf *rt_buf;
	int dst;

	if (!chub || !log)
		return -EINVAL;

	rt_buf = &chub->chub_rt_log;

	if (!rt_buf || !rt_buf->buffer)
		return -EINVAL;

	/* ensure last char is 0 */
	if (rt_buf->write_index + len + LOGFILE_NUM_SIZE >= rt_buf->buffer_size - 1) {
		rt_buf->wrap = true;
		rt_buf->write_index = 0;
	}
	dst = rt_buf->write_index; /* for race condition */
	if (dst + len + LOGFILE_NUM_SIZE >= rt_buf->buffer_size) {
		pr_info("%s index err!", __func__);
		return -EINVAL;
	}

	lenout = snprintf(rt_buf->buffer + dst, LOGFILE_NUM_SIZE + len,
			  "%6d:%c[%6llu.%06llu]%c %s",
			  log->size, log->size ? 'F' : 'K', (log->timestamp) / 1000000,
			  (log->timestamp) % 1000000, log->level, log->buf);

	rt_buf->buffer[dst + lenout - 1] = '\n';

	rt_buf->write_index += lenout;
	if (lenout != (LOGFILE_NUM_SIZE + len))
		pr_warn("%s: %s: size-n mismatch: %d -> %d\n",
			NAME_PREFIX, __func__, LOGFILE_NUM_SIZE + len, lenout);
	rt_buf->updated = true;
	wake_up_interruptible(&rt_buf->wq);
	return 0;
}

void chub_printf(struct device *dev, int level, int fw_idx, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	struct logbuf_output log;
	int n;
	struct contexthub_ipc_info *chub;
	char buf[240];

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	switch (level) {
	case 'W':
		if (dev)
			dev_warn(dev, "%pV", &vaf);
		else
			pr_warn("nanohub: %pV", &vaf);
		break;
	case 'E':
		if (dev)
			dev_err(dev, "%pV", &vaf);
		else
			pr_err("nanohub: %pV", &vaf);
		break;
	case 'D':
		if (dev)
			dev_dbg(dev, "%pV", &vaf);
		else
			pr_debug("nanohub: %pV", &vaf);
		break;
	default:
		if (dev)
			dev_info(dev, "%pV", &vaf);
		else
			pr_info("nanohub: %pV", &vaf);
		break;
	}
	va_end(args);

	if (print_info) {
		chub = dev_get_drvdata(print_info->dev);
		if (IS_ERR_OR_NULL(chub)) {
			dev_err(print_info->dev, "%s: chub not available");
			return;
		}
		memset(&log, 0, sizeof(struct logbuf_output));
		log.timestamp = ktime_get_boottime_ns() / 1000;
		log.level = level;
		log.size = fw_idx;
		log.buf = buf;

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
		if (chub->mlog.memlog_printf_chub) {
			if (dev)
				memlog_write_printf(chub->mlog.memlog_printf_chub,
						    MEMLOG_LEVEL_ERR, "%s: %pV",
						    dev_driver_string(dev), &vaf);
			else
				memlog_write_printf(chub->mlog.memlog_printf_chub,
						    MEMLOG_LEVEL_ERR, "%pV", &vaf);
		}
#endif
		if (dev)
			n = snprintf(log.buf, 239, "%s: %pV", dev_driver_string(dev), &vaf);
		else
			n = snprintf(log.buf, 239, "%pV", &vaf);
		logbuf_printf(chub, &log, n - 1);
	}
}

void contexthub_log_stop(void *chub_p)
{
	struct contexthub_ipc_info *chub = chub_p;
	if (!chub->log_kthread)
		return;
	kthread_stop(chub->log_kthread);
	chub->log_kthread = NULL;
	dev_info(chub->dev, "%s", __func__);
}

void contexthub_log_start(void *chub_p)
{
	struct contexthub_ipc_info *chub = chub_p;
	if (chub->log_kthread)
		return;
	chub->log_kthread = kthread_run(handle_log_kthread, chub, "chub_log_kthread");
	dev_info(chub->dev, "%s", __func__);
}

int contexthub_log_init(void *chub_p)
{
	struct contexthub_ipc_info *chub = chub_p;
	dev_info(chub->dev, "%s", __func__);

	chub->log_info = log_register_buffer(chub->dev, 0,
					     &chub->chub_rt_log,
					     "log", 0);
	chub->chub_rt_log.loglevel = 0;

	/* init fw runtime log */
	chub->chub_rt_log.buffer = devm_kzalloc(chub->dev, SIZE_OF_BUFFER, GFP_KERNEL);
	if (!chub->chub_rt_log.buffer)
		return -ENOMEM;

	chub->chub_rt_log.buffer_size = SIZE_OF_BUFFER;
	chub->chub_rt_log.write_index = 0;
	chub->chub_rt_log.wrap = false;

	init_waitqueue_head(&chub->log_kthread_wait);
	contexthub_log_start(chub);

	return 0;
}
