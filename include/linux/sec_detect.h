/*
 * Author: @Flopster101
 * Based on AkiraNoSushi's work for the Mi439 project.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SEC_H
#define _LINUX_SEC_H

#include <linux/types.h>

#define SEC_DETECT_LOG(fmt, ...) printk(KERN_INFO "sec_detect: " fmt, ##__VA_ARGS__)
static const char *sec_detect_label = "sec_detect: ";

enum SEC_devices {
	DEVICE_UNKNOWN = -1,
	SEC_A25,
	SEC_A33,
	SEC_A53,
	SEC_M33,
	SEC_M34,
	SEC_GTA4XLS
};

extern enum SEC_devices sec_devices;
extern int sec_current_device;
extern bool sec_needs_decon;
extern bool sec_needs_blic;
extern bool sec_doze;

#ifdef CONFIG_SEC_DETECT_SYSFS
extern char sec_current_device_name[32];
#endif

#endif /* _LINUX_SEC_H */
