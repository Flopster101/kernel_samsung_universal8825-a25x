/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_TIME_H
#define IS_TIME_H

#include <linux/time.h>
#include <linux/sched/clock.h>

#include "is-time-config.h"

#define MEASURE_TIME
#define MONITOR_TIME
#define MONITOR_TIMEOUT 5000 /* 5ms */
/* #define EXTERNAL_TIME */
/* #define INTERFACE_TIME */

#define INSTANCE_MASK   0x3

#define TM_FLITE_STR	0
#define TM_FLITE_END	1
#define TM_SHOT		2
#define TM_SHOT_D	3
#define TM_META_D	4
#define TM_MAX_INDEX	5

struct is_time {
	u32 time_count;
	unsigned long long t_dq[10];
	unsigned long long t_dqq_max;
	unsigned long long t_dqq_tot;
	unsigned long long time1_max;
	unsigned long long time1_tot;
	unsigned long long time2_max;
	unsigned long long time2_tot;
	unsigned long long time3_max;
	unsigned long long time3_tot;
	unsigned long long time4_cur;
	unsigned long long time4_old;
	unsigned long long time4_tot;
};

struct is_interface_time {
	u32 cmd;
	u32 time_tot;
	u32 time_min;
	u32 time_max;
	u32 time_cnt;
};

enum is_time_launch {
	LAUNCH_DDK_LOAD,
	LAUNCH_PREPROC_LOAD,
	LAUNCH_SENSOR_INIT,
	LAUNCH_SENSOR_START,
	LAUNCH_FAST_AE,
	LAUNCH_TOTAL,
};

enum is_time_queue {
	TMQ_DQ,
	TMQ_QS,
	TMQ_QE,
	TMQ_END,
};

void TIME_STR(unsigned int index);
void TIME_END(unsigned int index, const char *name);
void is_jitter(u64 timestamp);
#define PROGRAM_COUNT(count) monitor_point(group, count)
void monitor_point(void *group_data, u32 mpoint);

void monitor_time_shot(void *group_data, void *frame_data, u32 mpoint);
void monitor_time_queue(void *vctx_data, u32 mpoint);

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
void monitor_init(struct is_time *time);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void pablo_kunit_test_monitor_report(void *group_data, void *frame_data);
#endif
#endif

#ifdef INTERFACE_TIME
void measure_init(struct is_interface_time *time, u32 cmd);
void measure_time(struct is_interface_time *time, u32 instance,
			u32 group, ktime_t start, ktime_t end);
#endif
#endif

#define TIME_LAUNCH_STR(p)							\
	do {									\
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_TIME_LAUNCH)))	\
			TIME_STR(p);						\
	} while (0)
#define TIME_LAUNCH_END(p)							\
	do {									\
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_TIME_LAUNCH))) 	\
			TIME_END(p, #p);					\
	} while (0)

#define TIME_SHOT(point)							\
	do {									\
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_TIME_SHOT) > 0))	\
			monitor_time_shot(group, frame, point);			\
	} while (0)

#define TIME_QUEUE(point)							\
	do {									\
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_TIME_QUEUE) > 0))\
			monitor_time_queue(vctx, point);			\
	} while (0)

#define PABLO_KTIME_DELTA_NOW(s)	(ktime_sub(ktime_get(), s))
#define PABLO_KTIME_US_DELTA_NOW(s)	(ktime_us_delta(ktime_get(), s))

#endif
