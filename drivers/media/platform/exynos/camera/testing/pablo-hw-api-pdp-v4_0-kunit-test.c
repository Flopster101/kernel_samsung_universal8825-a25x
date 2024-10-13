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

#include "hardware/api/is-hw-api-pdp-v1.h"
#include "hardware/sfr/is-sfr-pdp-v4_0.h"

/* Define the test cases. */

struct pablo_hw_api_pdp_test_ctx {
	void *test_addr;
} static pablo_hw_api_pdp_test_ctx;

static void pablo_hw_api_pdp_hw_get_line_kunit_test(struct kunit *test)
{
	u32 total_line = 0;
	u32 curr_line = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_get_line(ctx->test_addr, &total_line, &curr_line);
}

static void pablo_hw_api_pdp_hw_s_one_shot_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_pdp pdp = { 0 };
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp.base = ctx->test_addr;

	ret = pdp_hw_s_one_shot_enable(&pdp);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_pdp_hw_s_corex_enable_kunit_test(struct kunit *test)
{
	bool enable = false;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_corex_enable(ctx->test_addr, enable);
}

static void pablo_hw_api_pdp_hw_s_corex_type_kunit_test(struct kunit *test)
{
	u32 type = COREX_IGNORE;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_corex_type(ctx->test_addr, type);
}

static void pablo_hw_api_pdp_hw_s_af_rdma_addr_kunit_test(struct kunit *test)
{
	dma_addr_t address[8] = { 0 };
	u32 num_buffers = 0;
	u32 direct = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_af_rdma_addr(ctx->test_addr, address, num_buffers, direct);
}

static void pablo_hw_api_pdp_hw_s_post_frame_gap_kunit_test(struct kunit *test)
{
	u32 interval = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_post_frame_gap(ctx->test_addr, interval);
}

static void pablo_hw_api_pdp_hw_strgen_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_strgen_enable(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_strgen_disable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_strgen_disable(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_s_config_default_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_config_default(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_dump(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	dma_addr_t address[8] = { 0 };
	u32 num_buffers = 0;
	u32 direct = 0;
	struct is_sensor_cfg cfg = { 0 };
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_rdma_addr(ctx->test_addr, address, num_buffers, direct, &cfg);
}

static void pablo_hw_api_pdp_hw_update_rdma_linegap_kunit_test(struct kunit *test)
{
	struct is_sensor_cfg cfg = { 0 };
	struct is_pdp pdp = { 0 };
	u32 en_votf = 0;
	u32 min_fps = 0;
	u32 max_fps = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_update_rdma_linegap(ctx->test_addr, &cfg, &pdp, en_votf, min_fps, max_fps);
}

static void pablo_hw_api_pdp_hw_g_int2_rdma_state_kunit_test(struct kunit *test)
{
	unsigned int ret;
	bool clear = false;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = pdp_hw_g_int2_rdma_state(ctx->test_addr, clear);
}

static void pablo_hw_api_pdp_hw_s_lic_bit_mode_kunit_test(struct kunit *test)
{
	u32 pixelsize = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_lic_bit_mode(ctx->test_addr, pixelsize);
}

static void pablo_hw_api_pdp_hw_rdma_wait_idle_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = pdp_hw_rdma_wait_idle(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_hw_api_pdp_hw_kunit_test_init(struct kunit *test)
{
	pablo_hw_api_pdp_test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pablo_hw_api_pdp_test_ctx.test_addr);

	test->priv = &pablo_hw_api_pdp_test_ctx;

	return 0;
}

static void pablo_hw_api_pdp_hw_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);
}

static struct kunit_case pablo_hw_api_pdp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_pdp_hw_get_line_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_one_shot_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_corex_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_corex_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_af_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_post_frame_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_strgen_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_strgen_disable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_config_default_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_update_rdma_linegap_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int2_rdma_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_lic_bit_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_rdma_wait_idle_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_pdp_kunit_test_suite = {
	.name = "pablo-hw-api-pdp-kunit-test",
	.init = pablo_hw_api_pdp_hw_kunit_test_init,
	.exit = pablo_hw_api_pdp_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_pdp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_pdp_kunit_test_suite);

MODULE_LICENSE("GPL");
