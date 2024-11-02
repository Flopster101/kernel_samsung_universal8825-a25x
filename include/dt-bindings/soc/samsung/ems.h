/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS MOBILE SCHEDULER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_MOBILE_SCHEDULER
#define __EXYNOS_MOBILE_SCHEDULER

/* SCHED POLICY */
#define SCHED_POLICY_ENERGY		0	/* low energy */
#define SCHED_POLICY_EFF		1	/* best efficiency */
#define SCHED_POLICY_EFF_ENERGY		2	/* low energy in best efficienct domain */
#define SCHED_POLICY_EFF_TINY		3	/* best efficiency except tiny task */
#define SCHED_POLICY_SEMI_PERF		4	/* semi perf */
#define SCHED_POLICY_PERF		5	/* best perf */
#define SCHED_POLICY_EXPRESS		6	/* task express */
#define NUM_OF_SCHED_POLICY		7

#endif /* __EXYNOS_MOBILE_SCHEDULER__ */
