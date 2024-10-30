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

#include "hardware/api/is-hw-api-yuvp-v1_0.h"
#include "hardware/api/is-hw-api-drcp-v1_0.h"
#include "hardware/sfr/is-sfr-drcp-v1_0.h"

/* Define the test cases. */

static void pablo_hw_api_drcp_hw_wait_corex_idle_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	_drcp_hw_wait_corex_idle(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_drc_grid_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 grid_size_x = 0;
	u32 grid_size_y = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_g_drc_grid_size(test_addr, set_id, &grid_size_x, &grid_size_y);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_hist_grid_num_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 grid_x_num = 0;
	u32 grid_y_num = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_g_hist_grid_num(test_addr, set_id, &grid_x_num, &grid_y_num);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_int_mask_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = drcp_hw_g_int_mask(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_int_mask1_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = drcp_hw_g_int_mask1(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_int_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	state = drcp_hw_g_int_state(test_addr, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_g_int_state1_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	state = drcp_hw_g_int_state1(test_addr, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_is_occured_kunit_test(struct kunit *test)
{
	unsigned int occured;
	unsigned int status = 0;
	enum yuvp_event_type type = INTR_FRAME_START;

	occured = drcp_hw_is_occured(status, type);
	KUNIT_EXPECT_EQ(test, occured, (unsigned int)0);
}

static void pablo_hw_api_drcp_hw_is_occured1_kunit_test(struct kunit *test)
{
	unsigned int occured;
	unsigned int status = 0;
	enum yuvp_event_type type = INTR_FRAME_START;

	occured = drcp_hw_is_occured1(status, type);
	KUNIT_EXPECT_EQ(test, occured, (unsigned int)0);
}

static void pablo_hw_api_drcp_hw_print_chain_debug_cnt_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_print_chain_debug_cnt(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_block_bypass(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_clahe_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_clahe_bypass(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_corex_init(test_addr, enable);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = drcp_hw_s_corex_update_type(test_addr, set_id);
	KUNIT_EXPECT_EQ(test, ret,  0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_rdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_rdma_corex_id(&dma, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_yuv444to422_coeff_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_yuv444to422_coeff(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_drcp_hw_s_yuvtorgb_coef_kunit_testf(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	drcp_hw_s_yuvtorgb_coeff(test_addr, set_id);

	kunit_kfree(test, test_addr);
}


static struct kunit_case pablo_hw_api_drcp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_drcp_hw_wait_corex_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_drc_grid_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_hist_grid_num_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_int_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_int_mask1_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_int_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_g_int_state1_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_is_occured_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_is_occured1_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_print_chain_debug_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_clahe_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_rdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_yuv444to422_coeff_kunit_test),
	KUNIT_CASE(pablo_hw_api_drcp_hw_s_yuvtorgb_coef_kunit_testf),
	{},
};

struct kunit_suite pablo_hw_api_drcp_kunit_test_suite = {
	.name = "pablo-hw-api-drcp-kunit-test",
	.test_cases = pablo_hw_api_drcp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_drcp_kunit_test_suite);

MODULE_LICENSE("GPL");
