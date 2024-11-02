/*
 * @file hdm.c
 * @brief HDM Support
 * Copyright (c) 2020, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/hdm.h>

#include "hdm_log.h"

int hdm_log_level = HDM_LOG_LEVEL;

int hdm_wifi_support;
EXPORT_SYMBOL(hdm_wifi_support);

void hdm_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (hdm_log_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk(KERN_INFO "%s %pV", TAG, &vaf);

	va_end(args);
}

/*
static int __init hdm_wifi_flag(void)
{
	int error = 0;
	struct file *filep = NULL;
	char cmdline[4096] = {0};
	char tmp_cmdline[4096] = {0};
	char *cmdline_p = NULL;
	char *tmp_cmdline_p = NULL;
	char *token = NULL;
	int cnt = 0;
	long val;
	int err;

	mm_segment_t old_fs;

	hdm_wifi_support = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open("/proc/bootconfig", O_RDONLY, 0);

	if (IS_ERR(filep) || (filep == NULL)) {
		hdm_info("%s fail to open cmdline\n", __func__);
		set_fs(old_fs);
		return 0;
	}

	error = kernel_read(filep, cmdline, sizeof(cmdline), (&filep->f_pos));

	if (error < 0) {
		hdm_info("%s fail to read cmdline\n", __func__);
		set_fs(old_fs);
		return 0;
	}

	set_fs(old_fs);

	cmdline_p = cmdline;

	token = strsep(&cmdline_p, " ");

	while (token) {
		if (strncmp(token, "androidboot.hdm_status=", 23) == 0) {
			hdm_info("%s %s\n", __func__, token);
			snprintf(tmp_cmdline, sizeof(tmp_cmdline), "%s", token);
			tmp_cmdline_p = tmp_cmdline;

			token = strsep(&tmp_cmdline_p, "=");
			token = strsep(&tmp_cmdline_p, "&|");
			hdm_info("%s token2 = %s\n", __func__, token);

			while (token) {
				//even = hdm applied bit
				hdm_info("%s hdm bit = %s\n", __func__, token);
				if (cnt++%2) {
					err = kstrtol(token, 16, &val);
					hdm_info("%s hdm token = 0x%x\n", __func__, val);
					if (err)
						return err;
					if (val & HDM_WIFI_SUPPORT_BIT) {
						hdm_info("%s wifi bit set applied bit = 0x%x\n", __func__, val);
						hdm_wifi_support = 1;
						break;
					}
				}
				token = strsep(&tmp_cmdline_p, "&|");
			}
			break;
		}
		token = strsep(&cmdline_p, " ");
	}
	hdm_info("%s finish\n", __func__);
	hdm_wifi_support = 1;

	return 0;
}
*/

static int __init hdm_init(void)

{

	hdm_info("HDM init!\n");
//	hdm_wifi_flag();
	hdm_wifi_support = 1;

	return 0;    // Non-zero return means that the module couldn't be loaded.

}

static void __exit hdm_cleanup(void)

{

	hdm_info("HDM cleanup!\n");

}

module_init(hdm_init);
module_exit(hdm_cleanup);

MODULE_AUTHOR("Taeho Kim <taeho81.kim@samsung.com>");
MODULE_AUTHOR("Shinjae Lee <shinjae1.lee@samsung.com>");
MODULE_DESCRIPTION("HDM driver");
MODULE_LICENSE("GPL");


