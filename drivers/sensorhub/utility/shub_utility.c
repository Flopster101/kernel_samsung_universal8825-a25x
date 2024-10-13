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
#include <linux/time64.h>
#include <linux/ktime.h>
#include <linux/rtc.h>

u64 get_current_timestamp(void)
{
	u64 timestamp;
	struct timespec64 ts;

	ts = ktime_to_timespec64(ktime_get_boottime());
	timestamp = timespec64_to_ns(&ts);

	return timestamp;
}

void get_tm(struct rtc_time *tm)
{
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, tm);
}
