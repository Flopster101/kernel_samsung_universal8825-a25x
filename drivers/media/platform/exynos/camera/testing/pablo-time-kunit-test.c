// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-time.h"
#include "is-groupmgr.h"
#include "is-core.h"

/* Define the test cases. */

static struct is_group group;
static struct is_frame time_frame;
static struct is_video_ctx vctx;

static void pablo_time_monitor_time_shot_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	group.device = &core->ischain[0];
	monitor_time_shot((void *)&group, (void *)&time_frame, 0);
}

static void pablo_time_monitor_time_queue_kunit_test(struct kunit *test)
{
	monitor_time_queue((void *)&vctx, 0);
}

static void pablo_time_start_end_kunit_test(struct kunit *test)
{
	TIME_STR(0);
	TIME_END(0, "KUNIT_TIME_TEST");
}

static void pablo_time_monitor_report_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	group.device = &core->ischain[0];
	pablo_kunit_test_monitor_report((void *)&group, (void *)&time_frame);
}

static void pablo_time_is_jitter_kunit_test(struct kunit *test)
{
	is_jitter(1000);
}

static struct kunit_case pablo_time_kunit_test_cases[] = {
	KUNIT_CASE(pablo_time_monitor_time_shot_kunit_test),
	KUNIT_CASE(pablo_time_monitor_time_queue_kunit_test),
	KUNIT_CASE(pablo_time_start_end_kunit_test),
	KUNIT_CASE(pablo_time_monitor_report_kunit_test),
	KUNIT_CASE(pablo_time_is_jitter_kunit_test),
	{},
};

struct kunit_suite pablo_time_kunit_test_suite = {
	.name = "pablo-time-kunit-test",
	.test_cases = pablo_time_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_time_kunit_test_suite);

MODULE_LICENSE("GPL");
