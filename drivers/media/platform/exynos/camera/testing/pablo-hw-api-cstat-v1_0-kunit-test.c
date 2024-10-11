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

#include "hardware/api/is-hw-api-cstat.h"
#include "hardware/sfr/is-sfr-cstat-v1_0.h"

/* Define the test cases. */

struct pablo_hw_api_cstat_test_ctx {
	void *test_addr;
} static pablo_hw_api_cstat_test_ctx;

static void pablo_hw_api_cstat_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_dump(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_g_version_kunit_test(struct kunit *test)
{
	u32 version;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	version =  cstat_hw_g_version(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, version, (u32)0);
}

static void pablo_hw_api_cstat_hw_print_err_kunit_test(struct kunit *test)
{
	char name[16] = "kunit_test";
	u32 instance = 0;
	u32 fcount = 0;
	ulong src1 = 0;
	ulong src2 = 0;

	cstat_hw_print_err(name, instance, fcount, src1, src2);
}

static void pablo_hw_api_cstat_hw_print_stall_status_kunit_test(struct kunit *test)
{
	int ch = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_print_stall_status(ctx->test_addr, ch);
}

static void pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_default_blk_cfg(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test(struct kunit *test)
{
	enum cstat_int_on_col_row_src src = CSTAT_CINFIFO;
	u32 col = 0;
	u32 row = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_freeze_on_col_row(ctx->test_addr, src, col, row);
}

static void pablo_hw_api_cstat_hw_s_fro_kunit_test(struct kunit *test)
{
	u32 num_buffer = 0;
	unsigned long grp_mask = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_fro(ctx->test_addr, num_buffer, grp_mask);
}

static u32 set_field_value(u32 reg_value, const struct is_field *field, u32 val)
{
	u32 field_mask = 0;

	field_mask = (field->bit_width >= 32) ? 0xFFFFFFFF : ((1 << field->bit_width) - 1);

	/* bit clear */
	reg_value &= ~(field_mask << field->bit_start);

	/* setting value */
	reg_value |= (val & field_mask) << (field->bit_start);

	return reg_value;
}

static void set_field(void __iomem *base_addr, const struct is_reg *reg,
		const struct is_field *field, u32 val)
{
	u32 reg_value;

	/* previous value reading */
	reg_value = readl(base_addr + (reg->sfr_offset));

	reg_value = set_field_value(reg_value, field, val);

	/* store reg value */
	writel(reg_value, base_addr + (reg->sfr_offset));
}

static void pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_teste(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set idleness status */
	set_field(ctx->test_addr, &cstat_regs[CSTAT_R_IDLENESS_STATUS],
			&cstat_fields[CSTAT_F_IDLENESS_STATUS], 1);

	ret = cstat_hw_s_one_shot_enable(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test(struct kunit *test)
{
	bool enable = false;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_sdc_enable(ctx->test_addr, enable);
}

static void pablo_hw_api_cstat_hw_s_crc_seed_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_crc_seed(ctx->test_addr, seed);
}

static struct kunit_case pablo_hw_api_cstat_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_cstat_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_version_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_err_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_stall_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_fro_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_teste),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_crc_seed_kunit_test),
	{},
};

static int pablo_hw_api_cstat_hw_kunit_test_init(struct kunit *test)
{
	pablo_hw_api_cstat_test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pablo_hw_api_cstat_test_ctx.test_addr);

	test->priv = &pablo_hw_api_cstat_test_ctx;

	return 0;
}

static void pablo_hw_api_cstat_hw_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);
}

struct kunit_suite pablo_hw_api_cstat_kunit_test_suite = {
	.name = "pablo-hw-api-cstat-kunit-test",
	.init = pablo_hw_api_cstat_hw_kunit_test_init,
	.exit = pablo_hw_api_cstat_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_cstat_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_cstat_kunit_test_suite);

MODULE_LICENSE("GPL");
