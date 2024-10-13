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

#include "hardware/api/is-hw-api-byrp.h"
#include "hardware/sfr/is-sfr-byrp-v1_0.h"

/* Define the test cases. */

static void pablo_hw_api_byrp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };

	byrp_hw_dma_dump(&dma);
}

static void pablo_hw_api_byrp_hw_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_g_int1_mask_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = byrp_hw_g_int1_mask(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_g_int1_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	state = byrp_hw_g_int1_state(test_addr, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_s_block_bypass(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_s_corex_init(test_addr, enable);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_corex_start_kunit_test(struct kunit *test)
{
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_s_corex_start(test_addr, enable);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = COREX_DIRECT;
	u32 type = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = byrp_hw_s_corex_update_type(test_addr, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 addr[8] = { 0 };
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;
	u32 image_addr_offset = 0;
	u32 header_addr_offset = 0;

	dma.id = BYRP_WDMA_BYR;

	ret = byrp_hw_s_wdma_addr(&dma, addr, plane, num_buffers, buf_idx,
			comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_byrp_hw_wait_corex_idle_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_wait_corex_idle(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_dtp_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	bool enable = true;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_s_dtp(test_addr, set_id, enable, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_byrp_hw_s_block_crc_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 seed = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	byrp_hw_s_block_crc(test_addr, set_id, seed);

	kunit_kfree(test, test_addr);
}

static struct kunit_case pablo_hw_api_byrp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_byrp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int1_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int1_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_corex_start_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_wait_corex_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_block_crc_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_byrp_kunit_test_suite = {
	.name = "pablo-hw-api-byrp-kunit-test",
	.test_cases = pablo_hw_api_byrp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_byrp_kunit_test_suite);

MODULE_LICENSE("GPL");
