// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics.
 *
 */

#include "modem_utils.h"

struct cpif_memlog {
	struct memlog *desc;
	struct memlog_obj *log_obj_file;
	struct memlog_obj *log_obj;
	unsigned int log_enable;
};

#define CPIF_MEMLOG_SIZE	(1024 * 1024)

struct cpif_memlog cplog;

int pr_buffer_memlog(const char *tag, const char *data, size_t data_len, size_t max_len)
{
	size_t len = min(data_len, max_len);
	unsigned char str[PR_BUFFER_SIZE * 3]; /* 1 <= sizeof <= max_len*3 */
	struct utc_time utc;
	static unsigned long count;

	if (len > PR_BUFFER_SIZE)
		len = PR_BUFFER_SIZE;

	dump2hex(str, (len ? len * 3 : 1), data, len);

	/* print UTC for every 64 packets */
	if ((count & 0x3F) == 0) {
		get_utc_time(&utc);
		pr_memlog_level(MEMLOG_LEVEL_NOTICE, "[%02d:%02d:%02d.%03d] %s(%ld): %s%s\n",
				utc.hour, utc.min, utc.sec, us2ms(utc.us), tag, (long)data_len,
				str, (len == data_len) ? "" : " ...");
	} else {
		pr_memlog_level(MEMLOG_LEVEL_NOTICE, "%s(%ld): %s%s\n", tag, (long)data_len, str,
				(len == data_len) ? "" : " ...");
	}

	count++;

	return 0;
}
EXPORT_SYMBOL(pr_buffer_memlog);

unsigned int cpif_memlog_log_enabled(void)
{
	return cplog.log_enable;
}
EXPORT_SYMBOL(cpif_memlog_log_enabled);

struct memlog_obj *cpif_memlog_log_obj(void)
{
	return cplog.log_obj;
}
EXPORT_SYMBOL(cpif_memlog_log_obj);

static int cpif_memlog_ops_dummy(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static const struct memlog_ops cpif_memlog_ops = {
	.file_ops_completed = cpif_memlog_ops_dummy,
	.log_status_notify = cpif_memlog_ops_dummy,
	.log_level_notify = cpif_memlog_ops_dummy,
	.log_enable_notify = cpif_memlog_ops_dummy,
};

void cpif_memlog_init(struct platform_device *pdev)
{
	int err;

	mif_info("+++\n");

	err = memlog_register("CPIF", &pdev->dev, &cplog.desc);
	if (err) {
		mif_err("failed to register memlog\n");
		return;
	}

	cplog.desc->ops = cpif_memlog_ops;

	cplog.log_obj = memlog_alloc_printf(cplog.desc, CPIF_MEMLOG_SIZE, NULL, "log-mem", 0);
	if (!cplog.log_obj) {
		mif_err("failed to alloc memlog memory for log\n");
		return;
	}

	cplog.log_enable = 1;

	mif_info("---\n");
}
EXPORT_SYMBOL(cpif_memlog_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung CPIF memlogger driver");
