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

#include "hardware/api/is-hw-api-rgbp-v1_0.h"
#include "hardware/sfr/is-sfr-rgbp-v1_0.h"

/* Define the test cases. */

static void pablo_hw_api_rgbp_hw_is_occured1_kunit_test(struct kunit *test)
{
	unsigned int occured;
	unsigned int status = 0;
	enum rgbp_event_type1 type = INTR_VOTF0_LOST_CONNECTION;

	occured = rgbp_hw_is_occurred1(status, type);
	KUNIT_EXPECT_EQ(test, occured, (unsigned int)0);
}

static void pablo_hw_api_rgbp_hw_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };

	rgbp_hw_dma_dump(&dma);
}

static void pablo_hw_api_rgbp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = rgbp_hw_s_rdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = rgbp_hw_s_wdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = 0;
	u32 type = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = rgbp_hw_s_corex_update_type(test_addr, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_corex_init(test_addr, enable);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_corex_start_kunit_test(struct kunit *test)
{
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_corex_start(test_addr, enable);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_g_int1_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	state = rgbp_hw_g_int1_state(test_addr, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_g_int1_mask_kunit_test(struct kunit *test)
{
	unsigned int mask;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	mask = rgbp_hw_g_int1_mask(test_addr);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_decomp_frame_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_decomp_frame_size(test_addr, set_id, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_sc_input_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_sc_input_size(test_addr, set_id, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_sc_src_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_sc_src_size(test_addr, set_id, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_sc_dst_size_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_sc_dst_size_size(test_addr, set_id, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_sc_output_crop_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 width = 0;
	u32 height = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_sc_output_crop_size(test_addr, set_id, width, height);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_block_bypass(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_is_scaler_get_yuvsc_src_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_get_yuvsc_src_size(test_addr, set_id, &h_size, &v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_is_scaler_get_yuvsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_get_yuvsc_dst_size(test_addr, set_id, &h_size, &v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_yuvsc_coef_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 hratio = 0;
	u32 vratio = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_yuvsc_coef(test_addr, set_id, hratio, vratio);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_yuvsc_round_mode_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 mode = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_yuvsc_round_mode(test_addr, set_id, mode);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_src_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_src_size(test_addr, set_id, h_size, v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_is_scaler_get_upsc_src_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_get_upsc_src_size(test_addr, set_id, &h_size, &v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_input_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_input_size(test_addr, set_id, h_size, v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_dst_size(test_addr, set_id, h_size, v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_is_scaler_get_upsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_get_upsc_dst_size(test_addr, set_id, &h_size, &v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_out_crop_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_out_crop_size(test_addr, set_id, h_size, v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_scaling_ratio_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 hratio = 0;
	u32 vratio = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_scaling_ratio(test_addr, set_id, hratio, vratio);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_upsc_coef_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 hratio = 0;
	u32 vratio = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_upsc_coef(test_addr, set_id, hratio, vratio);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_gamma_enable_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 enable = 0;
	u32 bypass = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_gamma_enable(test_addr, set_id, enable, bypass);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_decomp_size_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	u32 h_size = 0;
	u32 v_size = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_decomp_size(test_addr, set_id, h_size, v_size);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_decomp_iq_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_decomp_iq(test_addr, set_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_rgb_rdma_format_kunit_test(struct kunit *test)
{
	int ret;
	u32 rgb_format = DMA_FMT_RGB_BGRA1010102;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = pablo_kunit_rgbp_hw_s_rgb_rdma_format(test_addr, rgb_format);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_rgb_wdma_format_kunit_test(struct kunit *test)
{
	int ret;
	u32 rgb_format = DMA_FMT_RGB_BGRA1010102;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = pablo_kunit_rgbp_hw_s_rgb_wdma_format(test_addr, rgb_format);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_block_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_block_crc(test_addr, seed);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_rgbp_hw_s_sbwc_kunit_test(struct kunit *test)
{
	u32 set_id = 0;
	bool enable = false;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	rgbp_hw_s_sbwc(test_addr, set_id, enable);

	kunit_kfree(test, test_addr);
}

static struct kunit_case pablo_hw_api_rgbp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_rgbp_hw_is_occured1_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_start_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int1_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int1_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_frame_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_input_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_src_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_dst_size_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_output_crop_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_yuvsc_src_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_yuvsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_yuvsc_coef_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_yuvsc_round_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_src_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_upsc_src_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_input_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_upsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_out_crop_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_scaling_ratio_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_coef_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_gamma_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_iq_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rgb_rdma_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rgb_wdma_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_block_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sbwc_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_rgbp_kunit_test_suite = {
	.name = "pablo-hw-api-rgbp-kunit-test",
	.test_cases = pablo_hw_api_rgbp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_rgbp_kunit_test_suite);

MODULE_LICENSE("GPL");
