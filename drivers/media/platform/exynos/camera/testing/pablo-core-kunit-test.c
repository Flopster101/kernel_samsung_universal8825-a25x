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

/* Define the test cases. */

static struct device dev;
static struct device_attribute attr;

static void __debug_attr_store(struct kunit *test,
		struct device_attribute *dev_attr,
		char *val) {
	ssize_t count;
	char *buf_w = __getname();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_w);

	/* Write Test */
	count = 1;
	count = dev_attr->store(&dev, &attr, val, count);
	KUNIT_EXPECT_GT(test, count, (ssize_t)0);

	/* Verify */
	count = dev_attr->show(&dev, &attr, buf_w);
	KUNIT_EXPECT_GT(test, count, (ssize_t)0);

	__putname(buf_w);
}

static void __debug_attr_test(struct kunit *test,
		struct device_attribute *dev_attr,
		char *val) {
	ssize_t count;
	char *buf_r = __getname();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_r);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, val);

	/* Read Current Value */
	count = dev_attr->show(&dev, &attr, buf_r);
	KUNIT_EXPECT_GT(test, count, (ssize_t)0);

	/* Write Test */
	__debug_attr_store(test, dev_attr, val);

	/* Restore */
	__debug_attr_store(test, dev_attr, buf_r);

	__putname(buf_r);

}

static void pablo_core_debug_attr_en_dvfs_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[0], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "0");
}

static void pablo_core_debug_attr_pattern_en_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[1], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_pattern_fps_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[2], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_hal_debug_mode_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[3], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_hal_debug_delay_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[4], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	{
		ssize_t count;
		char *buf_r = __getname();

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_r);

		/* Read Current Value */
		count = dev_attr->show(&dev, &attr, buf_r);
		KUNIT_EXPECT_GT(test, count, (ssize_t)0);

		/* Write Test */
		__debug_attr_store(test, dev_attr, "4");

		/* Restore */
		buf_r[1] = '\0';
		__debug_attr_store(test, dev_attr, buf_r);

		__putname(buf_r);
	}
}

static void pablo_core_debug_attr_en_debug_state_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[5], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_init_step_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[6], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_init_positions_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[7], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1 2 3 4 5");
}

static void pablo_core_debug_attr_init_delays_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[8], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_enable_fixed_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[9], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_fixed_position_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[10], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_eeprom_reload_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[11], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_eeprom_dump_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[12], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_fixed_sensor_val_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[13], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	{
		ssize_t count;
		char *buf_r = __getname();

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_r);

		/* Read Current Value */
		count = dev_attr->show(&dev, &attr, buf_r);
		KUNIT_EXPECT_GT(test, count, (ssize_t)0);

		/* Write Test */
		__debug_attr_store(test, dev_attr, "10 100000 100000 800 800 800 800");

		/* Restore */
		__debug_attr_store(test, dev_attr, "30 300000 300000 900 900 900 900");

		__putname(buf_r);
	}
}

static void pablo_core_debug_attr_fixed_sensor_fps_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[14], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	{
		ssize_t count;
		char *buf_r = __getname();
		struct device_attribute *dev_attr_en_fixed_sensor_fps;

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_r);

		dev_attr_en_fixed_sensor_fps = container_of(debug_attr->attrs[16], struct device_attribute, attr);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

		/* enable fixed_sensor_fps */
		__debug_attr_store(test, dev_attr_en_fixed_sensor_fps, "1");

		/* Read Current Value */
		count = dev_attr->show(&dev, &attr, buf_r);
		KUNIT_EXPECT_GT(test, count, (ssize_t)0);

		/* Write Test */
		__debug_attr_store(test, dev_attr, "10");

		/* Restore */
		__debug_attr_store(test, dev_attr, "20");

		__putname(buf_r);

		/* disable fixed_sensor_fps */
		__debug_attr_store(test, dev_attr_en_fixed_sensor_fps, "0");
	}
}

static void pablo_core_debug_attr_max_sensor_fps_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[15], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	{
		ssize_t count;
		char *buf_r = __getname();
		struct device_attribute *dev_attr_en_fixed_sensor_fps;

		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf_r);

		dev_attr_en_fixed_sensor_fps = container_of(debug_attr->attrs[16], struct device_attribute, attr);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

		/* enable fixed_sensor_fps */
		__debug_attr_store(test, dev_attr_en_fixed_sensor_fps, "1");

		/* Read Current Value */
		count = dev_attr->show(&dev, &attr, buf_r);
		KUNIT_EXPECT_GT(test, count, (ssize_t)0);

		/* Write Test */
		__debug_attr_store(test, dev_attr, "15");

		/* Restore */
		__debug_attr_store(test, dev_attr, "20");

		__putname(buf_r);

		/* disable fixed_sensor_fps */
		__debug_attr_store(test, dev_attr_en_fixed_sensor_fps, "0");
	}
}

static void pablo_core_debug_attr_en_fixed_sensor_fps_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[16], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_debug_attr_en_fixed_sensor_kunit_test(struct kunit *test)
{
	struct attribute_group *debug_attr = pablo_kunit_get_debug_attr_group();
	struct device_attribute *dev_attr;
	struct is_core *core = is_get_is_core();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, core);

	dev_set_drvdata(&dev, core);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, debug_attr);

	dev_attr = container_of(debug_attr->attrs[17], struct device_attribute, attr);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev_attr);

	__debug_attr_test(test, dev_attr, "1");
}

static void pablo_core_wq_func_print_clk_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	work_func_t func;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	func = core->wq_data_print_clk.func;
	if (func) {
		func(&core->wq_data_print_clk);
	}
}

static void pablo_core_is_get_ischain_device_kunit_test(struct kunit *test)
{
	struct is_device_ischain *ischain;

	ischain = is_get_ischain_device(0);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, ischain);

}

static void pablo_core_is_get_sensor_device_kunit_test(struct kunit *test)
{
	u32 device_id = 0;
	struct is_device_sensor *sensor;

	sensor = is_get_sensor_device(device_id);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, sensor);
}

static void pablo_core_is_cleanup_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *device;
	int i;
	bool saveState[IS_SENSOR_COUNT] = { false };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	/* save state */
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (device && test_bit(IS_SENSOR_PROBE, &device->state)) {
			mutex_lock(&device->mutex_reboot);
			saveState[i] = device->reboot;
			mutex_unlock(&device->mutex_reboot);
		}
	}

	is_cleanup(core);

	/* restore state */
	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (device && test_bit(IS_SENSOR_PROBE, &device->state)) {
			mutex_lock(&device->mutex_reboot);
			device->reboot = saveState[i];
			mutex_unlock(&device->mutex_reboot);
		}
	}
}

static void pablo_core_is_print_frame_dva_kunit_test(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor;
	struct is_subdev *subdev;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	sensor = &core->sensor[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor);

	subdev = &sensor->group_sensor.leader;

	is_print_frame_dva(subdev);
}

static void pablo_core_is_secure_func_kunit_test(struct kunit *test)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	sensor = &core->sensor[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor);

	ret = is_secure_func(core, sensor, IS_SECURE_CAMERA_IRIS, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_secure_func(core, sensor, IS_SECURE_CAMERA_FACE, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_core_kunit_test_cases[] = {
	KUNIT_CASE(pablo_core_debug_attr_en_dvfs_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_pattern_en_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_pattern_fps_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_hal_debug_mode_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_hal_debug_delay_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_en_debug_state_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_init_step_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_init_positions_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_init_delays_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_enable_fixed_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_fixed_position_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_eeprom_reload_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_eeprom_dump_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_fixed_sensor_val_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_fixed_sensor_fps_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_max_sensor_fps_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_en_fixed_sensor_fps_kunit_test),
	KUNIT_CASE(pablo_core_debug_attr_en_fixed_sensor_kunit_test),
	KUNIT_CASE(pablo_core_wq_func_print_clk_kunit_test),
	KUNIT_CASE(pablo_core_is_get_ischain_device_kunit_test),
	KUNIT_CASE(pablo_core_is_get_sensor_device_kunit_test),
	KUNIT_CASE(pablo_core_is_cleanup_kunit_test),
	KUNIT_CASE(pablo_core_is_print_frame_dva_kunit_test),
	KUNIT_CASE(pablo_core_is_secure_func_kunit_test),
	{},
};

struct kunit_suite pablo_core_kunit_test_suite = {
	.name = "pablo-core-kunit-test",
	.test_cases = pablo_core_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_core_kunit_test_suite);

MODULE_LICENSE("GPL");
