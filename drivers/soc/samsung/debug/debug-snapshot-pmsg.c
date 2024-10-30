// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <soc/samsung/debug-snapshot.h>
#include "debug-snapshot-local.h"

#define LOGGER_LEVEL_HEADER	(1)
#define LOGGER_LEVEL_PREFIX	(2)
#define LOGGER_LEVEL_TEXT	(3)
#define LOGGER_HEADER_SIZE	(68)

#define LOG_ID_MAIN		(0)
#define LOG_ID_RADIO		(1)
#define LOG_ID_EVENTS		(2)
#define LOG_ID_SYSTEM		(3)
#define LOG_ID_CRASH		(4)
#define LOG_ID_KERNEL		(5)

struct pmsg_log_header {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
}__attribute__((__packed__));

struct android_log_header {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
}__attribute__((__packed__));

struct pmsg_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		prio;
	char		*buffer;
};

static DEFINE_MUTEX(pmsg_lock);
static struct pmsg_logger logger;
static const char *kPrioChars = "udVDIWEFs";

static void dbg_snapshot_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	struct tm tmBuf;
	u64 tv_kernel = local_clock();
	unsigned long rem_nsec = do_div(tv_kernel, 1000000000);
	unsigned int logbuf_len;
	unsigned char prio;

	if (logger.id == LOG_ID_EVENTS)
		return;

	switch (level) {
	case LOGGER_LEVEL_HEADER:
		tv_kernel = local_clock();
		rem_nsec = do_div(tv_kernel, 1000000000);
		time64_to_tm(logger.tv_sec, 0, &tmBuf);
		logbuf_len = snprintf(logbuf, LOGGER_HEADER_SIZE,
				"\n[%5llu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
				tv_kernel, rem_nsec / 1000, raw_smp_processor_id(), current->comm,
				tmBuf.tm_mon + 1, tmBuf.tm_mday,
				tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
				logger.tv_nsec / 1000000, logger.pid, logger.tid);

		dbg_snapshot_hook_logger(logbuf, logbuf_len - 1, DSS_ITEM_PLATFORM_ID);
		break;
	case LOGGER_LEVEL_PREFIX:
		prio = buffer[0];

		logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
		logbuf[1] = ' ';

		dbg_snapshot_hook_logger(logbuf, LOGGER_LEVEL_PREFIX, DSS_ITEM_PLATFORM_ID);
		break;
	case LOGGER_LEVEL_TEXT:
		buffer[count - 1] = ' ';
		dbg_snapshot_hook_logger(buffer, count, DSS_ITEM_PLATFORM_ID);
		break;
	default:
		break;
	}
	return;
}

static int dbg_snapshot_hook_pmsg(const char __user *buf, size_t count)
{
	char *buffer = logger.buffer;
	struct android_log_header *header;
	struct pmsg_log_header *pmsg_header;

	if (__copy_from_user(buffer, buf, count))
		return -EFAULT;

	switch (count) {
	case sizeof(struct android_log_header):
		header = (struct android_log_header *)buffer;
		logger.id = header->id;
		logger.tid = header->tid;
		logger.tv_sec = header->tv_sec;
		logger.tv_nsec  = header->tv_nsec;
		dbg_snapshot_combine_pmsg(buffer, count,
				logger.id > 7 ? LOGGER_LEVEL_TEXT : LOGGER_LEVEL_HEADER);
		break;
	case sizeof(unsigned char):
		dbg_snapshot_combine_pmsg(buffer, count, LOGGER_LEVEL_PREFIX);
		break;
	case sizeof(struct pmsg_log_header):
		pmsg_header = (struct pmsg_log_header *)buffer;
		if (pmsg_header->magic == 'l') {
			logger.pid = pmsg_header->pid;
			logger.uid = pmsg_header->uid;
			logger.len = pmsg_header->len;
			break;
		}
	default:
		dbg_snapshot_combine_pmsg(buffer, count, LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}

static ssize_t dbg_snapshot_write_pmsg(struct file *file, const char __user *buf,
			  	       size_t count, loff_t *ppos)
{
	int ret;

	if (!count)
		return 0;

	/* check outside lock, page in any data. write_user also checks */
	if (!access_ok(buf, count))
		return -EFAULT;

	if (count > PAGE_SIZE - 1)
		return -EOVERFLOW;

	mutex_lock(&pmsg_lock);
	ret = dbg_snapshot_hook_pmsg(buf, count);
	mutex_unlock(&pmsg_lock);
	return ret ? ret : count;
}

static const struct file_operations dss_pmsg_fops = {
	.owner		= THIS_MODULE,
	.llseek		= noop_llseek,
	.write		= dbg_snapshot_write_pmsg,
};

static struct class *dss_pmsg_class;
static int dss_pmsg_major;
#define PMSG_NAME "pmsg"

static char *dbg_snapshot_pmsg_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0220;
	return NULL;
}

void dbg_snapshot_init_pmsg(void)
{
	struct device *pmsg_device;

	logger.buffer = vmalloc(PAGE_SIZE);
	if (!logger.buffer)
		goto err;

	dss_pmsg_major = register_chrdev(0, PMSG_NAME, &dss_pmsg_fops);
	if (dss_pmsg_major < 0) {
		pr_err("register_chrdev failed\n");
		goto err;
	}

	dss_pmsg_class = class_create(THIS_MODULE, PMSG_NAME);
	if (IS_ERR(dss_pmsg_class)) {
		pr_err("device class file already in use\n");
		goto err_class;
	}
	dss_pmsg_class->devnode = dbg_snapshot_pmsg_devnode;
	pmsg_device = device_create(dss_pmsg_class, NULL, MKDEV(dss_pmsg_major, 0),
					NULL, "%s%d", PMSG_NAME, 0);
	if (IS_ERR(pmsg_device)) {
		pr_err("failed to create device\n");
		goto err_device;
	}

	pr_info("debug-snapshot: Register pmsg0 successful\n");
	return;

err_device:
	class_destroy(dss_pmsg_class);
err_class:
	unregister_chrdev(dss_pmsg_major, PMSG_NAME);
err:
	pr_info("debug-snapshot: Register pmsg0 failed\n");
	return;
}
