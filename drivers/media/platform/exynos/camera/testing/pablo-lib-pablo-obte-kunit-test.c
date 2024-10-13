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

#include "pablo-obte.h"

/* Define the test cases. */

/* this struct from pablo-obte.c */
typedef struct lib_interface_func_for_tuning {
	int (*json_readwrite_ctl)(void *ispObject, u32 instance_id,
				  u32 json_type, u32 tuning_id, ulong addr, u32 *size_bytes);
	int (*set_tuning_config)(void *tuning_config);

	int (*reserved_func_ptr_1)(void);

	int magic_number; /* must set TUNING_DRV_MAGIC_NUMBER */

	int (*reserved_cb_func_ptr_1)(void);
	int (*reserved_cb_func_ptr_2)(void);
	int (*reserved_cb_func_ptr_3)(void);
	int (*reserved_cb_func_ptr_4)(void);
} lib_interface_func_for_tuning_t;

static int json_readwrite_ctl(void *ispObject, u32 instance_id,
		u32 json_type, u32 tuning_id, ulong addr, u32 *size_bytes)
{
	return 0;
}

static int set_tuning_config(void *tuning_config)
{
	return 0;
}

static lib_interface_func_for_tuning_t lib_interface = {
	.json_readwrite_ctl = json_readwrite_ctl,
	.set_tuning_config = set_tuning_config,
	.reserved_func_ptr_1 = NULL,
	.reserved_cb_func_ptr_1 = NULL,
	.reserved_cb_func_ptr_2 = NULL,
	.reserved_cb_func_ptr_3 = NULL,
	.reserved_cb_func_ptr_4 = NULL
};

static void pablo_pablo_obte_init_3aa_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	bool reprocessing = false;

	pablo_kunit_obte_set_interface(&lib_interface);

	ret = pablo_obte_init_3aa(instance, reprocessing);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_obte_deinit_3aa(instance);

	pablo_kunit_obte_set_interface(NULL);
}

static struct kunit_case pablo_pablo_obte_kunit_test_cases[] = {
	KUNIT_CASE(pablo_pablo_obte_init_3aa_kunit_test),
	{},
};

struct kunit_suite pablo_pablo_obte_kunit_test_suite = {
	.name = "pablo-pablo-obte-kunit-test",
	.test_cases = pablo_pablo_obte_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_pablo_obte_kunit_test_suite);

MODULE_LICENSE("GPL");
