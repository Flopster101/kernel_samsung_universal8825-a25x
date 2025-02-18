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

#include "hardware/api/is-hw-api-yuvp-v1_1.h"
#include "hardware/sfr/is-sfr-yuvp-v1_1.h"

/* Define the test cases. */

static void pablo_hw_api_yuvp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };

	yuvp_hw_dma_dump(&dma);
}

static void pablo_hw_api_yuvp_hw_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_g_int_mask1_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = yuvp_hw_g_int_mask1(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_g_int_state1_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	state = yuvp_hw_g_int_state1(test_addr, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_is_occured1_kunit_test(struct kunit *test)
{
	unsigned int occured;
	unsigned int status = 0;
	enum yuvp_event_type type = INTR_FRAME_START;

	occured = yuvp_hw_is_occured1(status, type);
	KUNIT_EXPECT_EQ(test, occured, (unsigned int)0);
}

static void pablo_hw_api_yuvp_hw_print_chain_debug_cnt_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_print_chain_debug_cnt(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	void *test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_s_block_bypass(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_s_dtp_pattern_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	enum yuvp_dtp_pattern pattern = YUVP_DTP_PATTERN_SOLID;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_s_dtp_pattern(test_addr, set_id, pattern);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_s_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_s_crc(test_addr, seed);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_g_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = yuvp_hw_g_int_mask0(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_s_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_s_int_mask0(test_addr, mask);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_yuvp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	yuvp_hw_s_strgen(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static struct kunit_case pablo_hw_api_yuvp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_yuvp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_mask1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_state1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_is_occured1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_print_chain_debug_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_dtp_pattern_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_strgen_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_yuvp_kunit_test_suite = {
	.name = "pablo-hw-api-yuvp-kunit-test",
	.test_cases = pablo_hw_api_yuvp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_yuvp_kunit_test_suite);

MODULE_LICENSE("GPL");
