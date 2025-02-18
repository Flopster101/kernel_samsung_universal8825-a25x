/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#ifndef _UFS_PERF_V1_H_
#define _UFS_PERF_V1_H_

enum {
	__POLICY_HEAVY = 0,
	__POLICY_MAX,
};
#define POLICY_HEAVY	BIT(__POLICY_HEAVY)

enum __hat_state {
	HAT_RARE = 0,
	HAT_PRE_DWELL,
	HAT_REACH,				/* to control */
	HAT_DWELL,
	HAT_PRE_RARE,
	HAT_DROP,				/* to control */
};

enum __freq_state {
	FREQ_RARE = 0,
	FREQ_PRE_DWELL,
	FREQ_REACH,				/* to control */
	FREQ_DWELL,
	FREQ_PRE_RARE,
	FREQ_DROP,				/* to control */
};

struct ufs_perf_stat_v1 {
	/* enable bits */
	u32 policy_bits;
	u32 req_bits[__POLICY_MAX];

	/* reset timer */
	struct timer_list reset_timer;	/* stat reset timer */

	/* sysfs */
	struct kobject sysfs_kobj;

	/* threshold for hat, acccessible through sysfs */
	u32 th_qd_max;
	u32 th_qd_min;
	s64 th_dwell_in_high;			/* ms */
	s64 th_reach_up_to_high;		/* ms */

	/* threshold for random, acccessible through sysfs */
	s64 th_duration;
	s64 th_detect_dwell;
	s64 th_detect_rare;
	u32 th_count;

	/* threshold for reset, acccessible through sysfs */
	u32 th_reset_in_ms;

	/* sync */
	spinlock_t lock;

	/* stats for hat */
	s64 last_reach_time;
	s64 last_drop_time;
	s64 last_hat_transit_time;
	enum __hat_state hat_state;
	enum __freq_state freq_state;

	/* stats for random */
	s64 last_freq_transit_time;
	s64 start_count_time;
	u32 count;
	u64 last_lba;
	u32 seq_continue_count;
};

#endif /* _UFS_PERF_V1_H_ */
