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

#include "hardware/include/is-hw-mcscaler-v3.h"
#include "hardware/api/is-hw-api-mcscaler-v3.h"

#define MCSC_TRY_COUNT	20000

struct pablo_hw_mcsc_kunit_test_ctx {
	struct is_hw_ip *hw_ip;
	u32 instance;
	struct is_group group;
	struct is_core *core;
	struct is_framemgr framemgr;
	struct is_device_ischain *device;
	struct is_hardware *hardware;
	int hw_slot;
	struct is_region region;
	struct is_param_region *param_region;
	struct is_frame frame;
	struct camera2_shot shot;
	void *addr;
} static test_ctx;

/* Define the test cases. */

static void pablo_hw_mcsc_open_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance, &tctx->group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, MCSC_TRY_COUNT + 1);
}

static void pablo_hw_mcsc_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance, &tctx->group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, &tctx->group, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, MCSC_TRY_COUNT + 1);
}

static void pablo_hw_mcsc_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance, &tctx->group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, &tctx->group, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, MCSC_TRY_COUNT + 1);
}

static void pablo_hw_mcsc_set_param_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance, &tctx->group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, &tctx->group, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, tctx->device->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, MCSC_TRY_COUNT + 1);
}

static void pablo_hw_mcsc_shot_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance, &tctx->group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, &tctx->group, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, tctx->device->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, MCSC_TRY_COUNT + 1);
}

static int pablo_hw_mcsc_kunit_test_init(struct kunit *test)
{
	test_ctx.core = is_get_is_core();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.core);
	test_ctx.hardware = &test_ctx.core->hardware;
	test_ctx.hw_slot = pablo_hw_slot_id_wrap(DEV_HW_MCSC0);
	test_ctx.hw_ip = &test_ctx.hardware->hw_ip[test_ctx.hw_slot];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.hw_ip);
	test_ctx.hw_ip->framemgr = &test_ctx.framemgr;
	test_ctx.group.id = 0;
	test_ctx.device = &test_ctx.core->ischain[0];
	test_ctx.device->is_region = &test_ctx.region;
	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.param_region = (struct is_param_region *)test_ctx.shot.ctl.vendor_entry.parameter;

	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);
	test_ctx.hw_ip->regs[REG_SETA] = test_ctx.addr;
	test->priv = (struct pablo_hw_mcsc_kunit_test_ctx *)&test_ctx;

	return 0;
}

static void pablo_hw_mcsc_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *test_ctx = test->priv;

	kunit_kfree(test, test_ctx->addr);
}

static struct kunit_case pablo_hw_mcsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_mcsc_open_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_init_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_shot_kunit_test),
	{},
};

struct kunit_suite pablo_hw_mcsc_kunit_test_suite = {
	.name = "pablo-hw-mcsc-kunit-test",
	.init = pablo_hw_mcsc_kunit_test_init,
	.exit = pablo_hw_mcsc_kunit_test_exit,
	.test_cases = pablo_hw_mcsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_mcsc_kunit_test_suite);

MODULE_LICENSE("GPL");
