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

#include "is-core.h"

/* Define the test cases. */

static void pablo_interface_ischain_wq_func_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_interface *is_itf;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	is_itf = &core->interface;

	for (i = 0; i < WORK_MAX_MAP; i++) {
		work_func_t func = is_itf->work_wq[i].func;
		if (func) {
			func(&is_itf->work_wq[i]);
		}
	}

}

static struct kunit_case pablo_interface_ischain_kunit_test_cases[] = {
	KUNIT_CASE(pablo_interface_ischain_wq_func_kunit_test),
	{},
};

struct kunit_suite pablo_interface_ischain_kunit_test_suite = {
	.name = "pablo-interface-ischain-kunit-test",
	.test_cases = pablo_interface_ischain_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_interface_ischain_kunit_test_suite);

MODULE_LICENSE("GPL");
