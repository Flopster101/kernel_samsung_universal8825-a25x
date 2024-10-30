// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/version.h>

#include "dsp-log.h"
#include "dsp-time.h"

void dsp_time_get_timestamp(struct dsp_time *time, int opt)
{
	struct timespec64 ts64;

	dsp_enter();
	if (opt == TIMESTAMP_START) {
		ktime_get_ts64(&ts64);
		time->start.tv_sec = ts64.tv_sec;
		time->start.tv_nsec = ts64.tv_nsec;
	} else if (opt == TIMESTAMP_END) {
		ktime_get_ts64(&ts64);
		time->end.tv_sec = ts64.tv_sec;
		time->end.tv_nsec = ts64.tv_nsec;
	} else {
		dsp_warn("time opt is invalid(%d)\n", opt);
	}
	dsp_leave();
}

void dsp_time_get_interval(struct dsp_time *time)
{
	struct dsp_common_timespec *delta = &time->interval;

	dsp_enter();
	delta->tv_sec = time->end.tv_sec - time->start.tv_sec;
	delta->tv_nsec = time->end.tv_nsec - time->start.tv_nsec;

	if (delta->tv_nsec < 0) {
		delta->tv_nsec += NSEC_PER_SEC;
		--delta->tv_sec;
	}

	dsp_leave();
}

void dsp_time_print(struct dsp_time *time, const char *f, ...)
{
	char buf[128];
	va_list args;
	int len;

	dsp_enter();
	dsp_time_get_interval(time);

	va_start(args, f);
	len = vsnprintf(buf, sizeof(buf), f, args);
	va_end(args);

	if (len > 0)
		dsp_info("[%5llu.%06llu ms] %s\n",
				time->interval.tv_sec * 1000LL +
				time->interval.tv_nsec / 1000000LL,
				(time->interval.tv_nsec % 1000000LL),
				buf);
	else
		dsp_info("[%5llu.%06llu ms] INVALID\n",
				time->interval.tv_sec * 1000LL +
				time->interval.tv_nsec / 1000000LL,
				(time->interval.tv_nsec % 1000000LL));
	dsp_leave();
}
