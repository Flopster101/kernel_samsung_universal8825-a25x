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

#include "hardware/api/is-hw-api-mcscaler-v3.h"
#include "hardware/sfr/is-sfr-mcsc-v10_0.h"

/* Define the test cases. */

static void pablo_hw_api_mcsc_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_djag_strip_kunit_test(struct kunit *test)
{
	u32 pre_dst_h = 0, start_pos_h = 0;
	u32 test_result;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_set_djag_strip_config(test_addr, 0, pre_dst_h, start_pos_h);

	is_scaler_get_djag_strip_config(test_addr, 0, &pre_dst_h, &start_pos_h);
	KUNIT_EXPECT_EQ(test, pre_dst_h, (u32)0);
	KUNIT_EXPECT_EQ(test, start_pos_h, (u32)0);

	test_result = is_scaler_get_djag_strip_enable(test_addr, 0);
	KUNIT_EXPECT_EQ(test, test_result, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_set_hf_config_kunit_test(struct kunit *test)
{
	int ret;
	struct param_mcs_output hf_param = { 0 };
	struct scaler_coef_cfg sc_coef = { 0 };
	enum exynos_sensor_position sensor_position = SENSOR_POSITION_REAR;
	struct hf_cfg_by_ni hf_cfg = { 0 };
	u32 payload_size = 0;

	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	ret = is_scaler_set_hf_config(test_addr, 0, false, &hf_param,
			&sc_coef, sensor_position, &payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	is_scaler_set_djag_hf_cfg(test_addr, &hf_cfg);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_poly_kunit_test(struct kunit *test)
{
	u32 output_id = 0;
	u32 w = 0, h = 0;
	u32 pos_x = 0, pos_y = 0;
	u32 pre_dst_h = 0, start_pos_h = 0;
	u32 enable;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_set_poly_out_crop_size(test_addr, output_id, pos_x, pos_y, w, h);

	is_scaler_get_poly_out_crop_size(test_addr, output_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_get_poly_src_size(test_addr, output_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_set_poly_strip_config(test_addr, output_id, pre_dst_h, start_pos_h);
	is_scaler_get_poly_strip_config(test_addr, output_id, &pre_dst_h, &start_pos_h);
	KUNIT_EXPECT_EQ(test, pre_dst_h, (u32)0);
	KUNIT_EXPECT_EQ(test, start_pos_h, (u32)0);

	enable = is_scaler_get_poly_strip_enable(test_addr, output_id);
	KUNIT_EXPECT_EQ(test, enable, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_post_kunit_test(struct kunit *test)
{
	u32 output_id = 0;
	u32 w = 0, h = 0;
	u32 pos_x = 0, pos_y = 0;
	u32 pre_dst_h = 0, start_pos_h = 0;
	u32 enable;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_get_post_img_size(test_addr, output_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_set_post_out_crop_size(test_addr, output_id, pos_x, pos_y, w, h);

	is_scaler_get_post_out_crop_size(test_addr, output_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_set_post_strip_config(test_addr, output_id, pre_dst_h, start_pos_h);
	is_scaler_get_post_strip_config(test_addr, output_id, &pre_dst_h, &start_pos_h);
	KUNIT_EXPECT_EQ(test, pre_dst_h, (u32)0);
	KUNIT_EXPECT_EQ(test, start_pos_h, (u32)0);

	enable = is_scaler_get_post_strip_enable(test_addr, output_id);
	KUNIT_EXPECT_EQ(test, enable, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_rdma_kunit_test(struct kunit *test)
{
	u32 w = 0, h = 0;
	u32 y_stride = 0, uv_stride = 0;
	u32 y_addr = 0, cb_addr = 0, cr_addr = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_clear_rdma_addr(test_addr);

	is_scaler_set_rdma_size(test_addr, w, h);
	is_scaler_get_rdma_size(test_addr, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_set_rdma_stride(test_addr, y_stride, uv_stride);
	is_scaler_get_rdma_stride(test_addr, &y_stride, &uv_stride);
	KUNIT_EXPECT_EQ(test, y_stride, (u32)0);
	KUNIT_EXPECT_EQ(test, uv_stride, (u32)0);

	is_scaler_set_rdma_addr(test_addr, y_addr, cb_addr, cr_addr, 0);
	is_scaler_set_rdma_2bit_addr(test_addr, y_addr, cb_addr, 0);

	is_scaler_set_rdma_2bit_stride(test_addr, y_stride, uv_stride);
	is_scaler_set_rdma_10bit_type(test_addr, 0);

	is_scaler_set_rdma_frame_seq(test_addr, 0);

	is_scaler_set_votf_config(test_addr, 0, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_wdma_kunit_test(struct kunit *test)
{
	u32 output_id = 0;
	u32 w = 0, h = 0;
	u32 y_stride = 0, uv_stride = 0;
	u32 y_addr = 0, cb_addr = 0, cr_addr = 0;
	u32 plane = 0;
	u32 out_fmt = MCSC_RGB_ARGB8888;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_clear_rdma_addr(test_addr);

	is_scaler_set_wdma_size(test_addr, output_id, w, h);
	is_scaler_get_wdma_size(test_addr, output_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_set_wdma_stride(test_addr, output_id, y_stride, uv_stride);
	is_scaler_get_wdma_stride(test_addr, output_id, &y_stride, &uv_stride);
	KUNIT_EXPECT_EQ(test, y_stride, (u32)0);
	KUNIT_EXPECT_EQ(test, uv_stride, (u32)0);

	is_scaler_set_wdma_addr(test_addr, output_id, y_addr, cb_addr, cr_addr, 0);
	is_scaler_get_wdma_addr(test_addr, output_id, &y_addr, &cb_addr, &cr_addr, 0);
	KUNIT_EXPECT_EQ(test, y_addr, (u32)0);
	KUNIT_EXPECT_EQ(test, cb_addr, (u32)0);
	KUNIT_EXPECT_EQ(test, cr_addr, (u32)0);

	is_scaler_set_wdma_2bit_addr(test_addr, output_id, y_addr, cb_addr, 0);

	output_id = MCSC_OUTPUT2;
	is_scaler_set_wdma_format(test_addr, 0, output_id, plane, out_fmt);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_hwfc_kunit_test(struct kunit *test)
{
	u32 idx;
	u32 output_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	idx = is_scaler_get_hwfc_idx_bin(test_addr, output_id);
	KUNIT_EXPECT_EQ(test, idx, (u32)0);

	idx = is_scaler_get_hwfc_cur_idx(test_addr, output_id);
	KUNIT_EXPECT_EQ(test, idx, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_get_idle_status_kunit_test(struct kunit *test)
{
	u32 status;
	u32 hw_id = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	status = is_scaler_get_idle_status(test_addr, hw_id);
	KUNIT_EXPECT_EQ(test, status, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_get_version_kunit_test(struct kunit *test)
{
	u32 version;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	version = is_scaler_get_version(test_addr);
	KUNIT_EXPECT_EQ(test, version, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_input_kunit_test(struct kunit *test)
{
	u32 hw_id = 0;
	u32 w = 0, h = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_set_input_img_size(test_addr, hw_id, w, h);
	is_scaler_get_input_img_size(test_addr, hw_id, &w, &h);
	KUNIT_EXPECT_EQ(test, w, (u32)0);
	KUNIT_EXPECT_EQ(test, h, (u32)0);

	is_scaler_get_input_source(test_addr, hw_id);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_cac_kunit_test(struct kunit *test)
{
	struct cac_cfg_by_ni cfg = { 0 };
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_set_cac_enable(test_addr, 0);
	is_scaler_set_cac_map_crt_thr(test_addr, &cfg);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_set_shadow_ctrl_kunit_test(struct kunit *test)
{
	u32 hw_id = 0;
	enum mcsc_shadow_ctrl ctrl = SHADOW_WRITE_START;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	is_scaler_set_shadow_ctrl(test_addr, hw_id, ctrl);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_api_mcsc_set_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	pablo_kunit_scaler_hw_set_crc(test_addr, seed);

	kunit_kfree(test, test_addr);
}

static struct kunit_case pablo_hw_api_mcsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_mcsc_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_djag_strip_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_set_hf_config_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_poly_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_post_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_rdma_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_wdma_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_hwfc_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_get_idle_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_get_version_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_input_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_cac_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_set_shadow_ctrl_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcsc_set_crc_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_mcsc_kunit_test_suite = {
	.name = "pablo-hw-api-mcsc-kunit-test",
	.test_cases = pablo_hw_api_mcsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_mcsc_kunit_test_suite);

MODULE_LICENSE("GPL");
