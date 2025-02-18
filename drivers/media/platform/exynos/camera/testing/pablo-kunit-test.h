/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef PABLO_KUNIT_TEST_H
#define PABLO_KUNIT_TEST_H

#include <kunit/test.h>

#define __pablo_kunit_test_suites(unique_array, unique_suites, ...)		\
	static struct kunit_suite *unique_array[] = { __VA_ARGS__, NULL };	\
	static struct kunit_suite **unique_suites				\
	__used __section("pablo_kunit_test_suites") = unique_array

#define define_pablo_kunit_test_suites(__suites...)			\
	__pablo_kunit_test_suites(__UNIQUE_ID(array),			\
			__UNIQUE_ID(suites),				\
			##__suites)

#define __pablo_kunit_test_suites_end(unique_array, unique_suites, ...)	\
	static struct kunit_suite *unique_array[] = { __VA_ARGS__, NULL };	\
	static struct kunit_suite **unique_suites				\
	__used __section("__end_pablo_kunit_test_suite") = unique_array

#define define_pablo_kunit_test_suites_end(__suites...)			\
	__pablo_kunit_test_suites_end(__UNIQUE_ID(array),			\
			__UNIQUE_ID(suites),					\
			##__suites)

#endif /* PABLO_KUNIT_TEST_H */
