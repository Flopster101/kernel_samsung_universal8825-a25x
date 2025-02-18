/*
 * sec_pm_debug.h - SEC Power Management Debug Support
 *
 *  Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_SEC_PON_ALARM_H
#define __LINUX_SEC_PON_ALARM_H __FILE__

struct alarm_timespec {
	char alarm[14];
};

#define ANDROID_ALARM_BASE_CMD(cmd)		(cmd & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_SET_ALARM_BOOT	_IOW('a', 7, struct alarm_timespec)

#endif /* __LINUX_SEC_PON_ALARM_H */
