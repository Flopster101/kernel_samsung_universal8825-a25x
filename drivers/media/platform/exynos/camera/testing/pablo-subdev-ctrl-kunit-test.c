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
#include "is-subdev-ctrl.h"
#include "is-device-sensor.h"

/* Define the test cases. */

static struct is_device_sensor device;
static struct is_subdev subdev;

static void pablo_subdev_ctrl_internal_start_kunit_test(struct kunit *test)
{
	int ret;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	device.resourcemgr = &core->resourcemgr;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, device.resourcemgr);

	device.private_data = core;
	device.groupmgr = &core->groupmgr;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, device.groupmgr);
	device.devicemgr = &core->devicemgr;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, device.devicemgr);

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev.state);

	subdev.output.width = 1920;
	subdev.output.height = 1080;
	subdev.output.crop.x = 0;
	subdev.output.crop.y = 0;
	subdev.output.crop.w = subdev.output.width;
	subdev.output.crop.h = subdev.output.height;
	subdev.bits_per_pixel = 10;
	subdev.buffer_num = 4;
	subdev.batch_num = 1;
	subdev.memory_bitwidth = 12;
	subdev.sbwc_type = NONE;
	subdev.vid = IS_VIDEO_SS0_NUM;
	snprintf(subdev.data_type, sizeof(subdev.data_type), "%s", "UNIT_TEST");
	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev.state);

	ret = is_subdev_internal_start((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_internal_stop_kunit_test(struct kunit *test)
{
	int ret;

	ret = is_subdev_internal_stop((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_internal_start_negative_kunit_test(struct kunit *test)
{
	int ret;

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev.state);

	ret = is_subdev_internal_start((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev.state);
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev.state);

	ret = is_subdev_internal_start((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static void pablo_subdev_ctrl_internal_stop_negative_kunit_test(struct kunit *test)
{
	int ret;

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev.state);

	ret = is_subdev_internal_stop((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev.state);
	clear_bit(IS_SUBDEV_START, &subdev.state);

	ret = is_subdev_internal_stop((void *)&device, IS_DEVICE_SENSOR, &subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static struct kunit_case pablo_subdev_ctrl_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_ctrl_internal_start_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_stop_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_start_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_stop_negative_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_ctrl_kunit_test_suite = {
	.name = "pablo-subdev-ctrl-kunit-test",
	.test_cases = pablo_subdev_ctrl_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_ctrl_kunit_test_suite);

MODULE_LICENSE("GPL");
