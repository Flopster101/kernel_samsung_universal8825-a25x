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

#include "is-video.h"

/* Define the test cases. */

static void pablo_video_is_formats_kunit_test(struct kunit *test)
{
	struct is_fmt *fmt;
	ulong arr_size;
	ulong i;
	struct is_frame_cfg fc;
	unsigned int sizes[5] = { 0 };

	arr_size = pablo_kunit_get_array_size_is_formats();
	for (i = 0; i < arr_size; i++) {
		fmt = pablo_kunit_get_is_formats_struct(i);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fmt);

		fc.format = fmt;
		fc.width = 1920;
		fc.height = 1080;

		fmt->setup_plane_sz(&fc, sizes);

		/* need to verify */
	}
}

static struct kunit_case pablo_video_kunit_test_cases[] = {
	KUNIT_CASE(pablo_video_is_formats_kunit_test),
	{},
};

struct kunit_suite pablo_video_kunit_test_suite = {
	.name = "pablo-video-kunit-test",
	.test_cases = pablo_video_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_video_kunit_test_suite);

MODULE_LICENSE("GPL");
