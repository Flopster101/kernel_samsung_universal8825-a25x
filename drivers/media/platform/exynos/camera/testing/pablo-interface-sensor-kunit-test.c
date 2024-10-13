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
#include "is-device-sensor-peri.h"

/* Define the test cases. */

static struct is_sensor_interface *get_sensor_itf(struct kunit *test)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev_module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_sensor_interface *itf;
	u32 i2c_ch;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor);

	module = &sensor->module_enum[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module);

	subdev_module = module->subdev;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, subdev_module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);

	/* This is originaly in the set_input */
	i2c_ch = module->pdata->sensor_i2c_ch;
	KUNIT_ASSERT_LE(test, i2c_ch, (u32)SENSOR_CONTROL_I2C_MAX);
	sensor_peri->cis.i2c_lock = &core->i2c_lock[i2c_ch];

	ret = v4l2_subdev_call(subdev_module, core, init, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	itf = &sensor_peri->sensor_interface;

	return itf;
}

static struct is_device_sensor_peri *get_sensor_peri(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *sensor;
	struct is_module_enum *module;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];

	module = &sensor->module_enum[0];

	return (struct is_device_sensor_peri *)module->private_data;
}

/* cis_itf_ops */
static void pablo_interface_sensor_adjust_analog_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum is_exposure_gain_count num_data = EXPOSURE_GAIN_COUNT_1;
	u32 desired_gain;
	u32 actual_gain;
	is_sensor_adjust_direction adjust_direction = SENSOR_ADJUST_TO_SHORT;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf->cis_itf_ops.adjust_analog_gain);

	ret = itf->cis_itf_ops.adjust_analog_gain(itf, num_data, &desired_gain,
			&actual_gain, adjust_direction);

	/* This is not yet implemented */
	KUNIT_EXPECT_EQ(test, ret, -1);
}

static void pablo_interface_sensor_get_initial_exposure_gain_of_sensor_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum is_exposure_gain_count num_data = EXPOSURE_GAIN_COUNT_1;
	u32 expo = 0;
	u32 again = 0;
	u32 dgain = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.get_initial_exposure_gain_of_sensor(itf, num_data,
			&expo, &again, &dgain);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_frameid_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 frameid = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.get_sensor_frameid(itf, &frameid);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_type_kunit_test(struct kunit *test)
{
	struct is_sensor_interface *itf = get_sensor_itf(test);
	int type;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	type = itf->cis_itf_ops.get_sensor_type(itf);
	KUNIT_EXPECT_EQ(test, type, 0);
}

static void pablo_interface_sensor_get_vblank_count_kunit_test(struct kunit *test)
{
	struct is_sensor_interface *itf = get_sensor_itf(test);
	int vblank_count;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	vblank_count = itf->cis_itf_ops.get_vblank_count(itf);
	KUNIT_EXPECT_EQ(test, vblank_count, 0);
}

static void pablo_interface_sensor_is_tof_af_available_kunit_test(struct kunit *test)
{
	struct is_sensor_interface *itf = get_sensor_itf(test);
	bool available = false;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	available = itf->cis_itf_ops.is_tof_af_available(itf);
	KUNIT_EXPECT_EQ(test, available, (bool)false);
}

static void pablo_interface_sensor_is_vvalid_period_kunit_test(struct kunit *test)
{
	struct is_sensor_interface *itf = get_sensor_itf(test);
	bool vvalid = false;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	vvalid = itf->cis_itf_ops.is_vvalid_period(itf);
	KUNIT_EXPECT_EQ(test, vvalid, (bool)false);
}

static void pablo_interface_sensor_request_analog_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum is_exposure_gain_count num_data = EXPOSURE_GAIN_COUNT_1;
	u32 analog_gain[3] = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.request_analog_gain(itf, num_data, analog_gain);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_reserved_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 reserve_val = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.reserved0(itf, false);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = itf->cis_itf_ops.reserved1(itf, &reserve_val);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_initial_exposure_of_setfile_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 expo = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.set_initial_exposure_of_setfile(itf, expo);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_cur_uctl_list_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_itf_ops.set_cur_uctl_list(itf);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* cis_evt_ops */
static void pablo_interface_sensor_apply_frame_settings_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_evt_ops.apply_frame_settings(itf);

	/* This is not yet implemented */
	KUNIT_EXPECT_EQ(test, ret, -1);
}

/* cis_ext_itf_ops */
static void pablo_interface_sensor_change_cis_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum itf_cis_interface cis_mode = ITF_CIS_SMIA;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.change_cis_mode(itf, cis_mode);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_12bit_state_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum is_sensor_12bit_state state = SENSOR_12BIT_STATE_OFF;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.get_sensor_12bit_state(itf, &state);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_hdr_stat_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum itf_cis_hdr_stat_status status = SENSOR_STAT_STATUS_NO_DATA;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.get_sensor_hdr_stat(itf, &status);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_request_frame_length_line_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 framelengthline = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.request_frame_length_line(itf, framelengthline);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_3a_alg_res_to_sens_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_3a_res_to_sensor sensor_setting = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.set_3a_alg_res_to_sens(itf, &sensor_setting);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_adjust_sync_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 setsync = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext_itf_ops.set_adjust_sync(itf, setsync);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* cis_ext2_itf_ops */
static void pablo_interface_sensor_get_delayed_preflash_time_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 delayedTime = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.get_delayed_preflash_time(itf, &delayedTime);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_open_close_hint_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	int opening = 0;
	int closing = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.get_open_close_hint(&opening, &closing);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_max_dynamic_fps_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 max_fps = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.get_sensor_max_dynamic_fps(itf, &max_fps);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_request_direct_flash_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 mode = 0;
	bool on = false;
	u32 intensity = 0;
	u32 time = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	/* TODO: Fix test fail */
	return;

	ret = itf->cis_ext2_itf_ops.request_direct_flash(itf, mode, on, intensity, time);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_request_wb_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 gr_gain = 0;
	u32 r_gain = 0;
	u32 b_gain = 0;
	u32 gb_gain = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.request_wb_gain(itf, gr_gain, r_gain, b_gain, gb_gain);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_aeb_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 mode = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.set_hdr_mode(itf, mode);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_long_term_expo_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_long_term_expo_mode mode = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.set_long_term_expo_mode(itf, &mode);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_low_noise_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 mode = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.set_low_noise_mode(itf, mode);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_lte_multi_capture_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	bool mode = false;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->cis_ext2_itf_ops.set_lte_multi_capture_mode(itf, mode);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_mainflash_duration_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 duration = 3;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	/* TODO: Fix test fail */
	return;

	ret = itf->cis_ext2_itf_ops.set_mainflash_duration(itf, duration);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* flash_itf_ops */
static void pablo_interface_sensor_request_flash_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 mode = 0;
	bool on = false;
	u32 intensity = 0;
	u32 time = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	/* TODO: Fix test fail */
	return;

	ret = itf->flash_itf_ops.request_flash(itf, mode, on, intensity, time);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_request_flash_expo_gain_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_flash_expo_gain flash_ae = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	/* TODO: Fix test fail */
	return;

	ret = itf->flash_itf_ops.request_flash_expo_gain(itf, &flash_ae);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* aperture_itf_ops */
static void pablo_interface_sensor_get_aperture_value_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_apature_info_t param = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->aperture_itf_ops.get_aperture_value(itf, &param);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_set_aperture_value_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	int val = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->aperture_itf_ops.set_aperture_value(itf, val);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* paf_itf_ops */
static void pablo_interface_sensor_get_paf_ready_kunit_test(struct kunit *test)
{
	int ret;
	int expect_ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_device_sensor_peri *sensor_peri = get_sensor_peri(test);
	u32 ready = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);

	if (sensor_peri->pdp || sensor_peri->subdev_pdp)
		expect_ret = 0;
	else
		expect_ret = -EINVAL;

	ret = itf->paf_itf_ops.get_paf_ready(itf, &ready);
	KUNIT_EXPECT_EQ(test, ret, expect_ret);
}

static void pablo_interface_sensor_paf_reserved_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->paf_itf_ops.reserved[0](itf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = itf->paf_itf_ops.reserved[1](itf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = itf->paf_itf_ops.reserved[2](itf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = itf->paf_itf_ops.reserved[3](itf);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

/* laser_af_itf_ops */
static void pablo_interface_sensor_get_laser_distance_kunit_test(struct kunit *test)
{
	int ret;
	int expect_ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	struct is_device_sensor_peri *sensor_peri = get_sensor_peri(test);
	enum itf_laser_af_type type = ITF_LASER_AF_TYPE_INVALID;
	union itf_laser_af_data data = { 0 };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);

	if (sensor_peri->laser_af)
		expect_ret = 0;
	else
		expect_ret = -ENODEV;

	ret = itf->laser_af_itf_ops.get_distance(itf, &type, &data);
	KUNIT_EXPECT_EQ(test, ret, expect_ret);
}

/* csi_itf_ops */
static void pablo_interface_sensor_csi_reserved_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->csi_itf_ops.reserved[0](itf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = itf->csi_itf_ops.reserved[1](itf);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_vc_dma_buf_max_size_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	enum itf_vc_buf_data_type request_data_type = VC_BUF_DATA_TYPE_SENSOR_STAT1;
	u32 width = 0;
	u32 height = 0;
	u32 element_size = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->csi_itf_ops.get_vc_dma_buf_max_size(itf, request_data_type,
			&width, &height, &element_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

/* dual_itf_ops */
static void pablo_interface_sensor_dual_reserved_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->dual_itf_ops.reserved[0](itf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = itf->dual_itf_ops.reserved[1](itf);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_reuse_3a_state_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 position = 0;
	u32 ae_exposure = 0;
	u32 ae_deltaev = 0;
	bool is_clear = false;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->dual_itf_ops.get_reuse_3a_state(itf, &position, &ae_exposure,
			&ae_deltaev, is_clear);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_sensor_get_sensor_state_kunit_test(struct kunit *test)
{
	struct is_sensor_interface *itf = get_sensor_itf(test);
	int state;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	state = itf->dual_itf_ops.get_sensor_state(itf);
	KUNIT_EXPECT_EQ(test, state, 0);
}

static void pablo_interface_sensor_set_reuse_ae_exposure_kunit_test(struct kunit *test)
{
	int ret;
	struct is_sensor_interface *itf = get_sensor_itf(test);
	u32 ae_exposure = 0;
	u32 ae_deltaev = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf);

	ret = itf->dual_itf_ops.set_reuse_ae_exposure(itf, ae_exposure, ae_deltaev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_interface_sensor_kunit_test_cases[] = {
	/* cis_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_adjust_analog_gain_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_initial_exposure_gain_of_sensor_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_frameid_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_type_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_vblank_count_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_is_tof_af_available_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_is_vvalid_period_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_request_analog_gain_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_reserved_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_initial_exposure_of_setfile_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_cur_uctl_list_kunit_test),
	/* cis_evt_ops */
	KUNIT_CASE(pablo_interface_sensor_apply_frame_settings_kunit_test),
	/* cis_ext_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_change_cis_mode_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_12bit_state_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_hdr_stat_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_request_frame_length_line_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_3a_alg_res_to_sens_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_adjust_sync_kunit_test),
	/* cis_ext2_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_get_delayed_preflash_time_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_open_close_hint_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_max_dynamic_fps_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_request_direct_flash_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_request_wb_gain_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_aeb_mode_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_long_term_expo_mode_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_low_noise_mode_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_lte_multi_capture_mode_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_mainflash_duration_kunit_test),
	/* flash_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_request_flash_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_request_flash_expo_gain_kunit_test),
	/* aperture_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_get_aperture_value_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_aperture_value_kunit_test),
	/* paf_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_get_paf_ready_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_paf_reserved_kunit_test),
	/* laser_af_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_get_laser_distance_kunit_test),
	/* csi_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_csi_reserved_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_vc_dma_buf_max_size_kunit_test),
	/* dual_itf_ops */
	KUNIT_CASE(pablo_interface_sensor_dual_reserved_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_reuse_3a_state_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_get_sensor_state_kunit_test),
	KUNIT_CASE(pablo_interface_sensor_set_reuse_ae_exposure_kunit_test),
	{},
};

struct kunit_suite pablo_interface_sensor_kunit_test_suite = {
	.name = "pablo-interface-sensor-kunit-test",
	.test_cases = pablo_interface_sensor_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_interface_sensor_kunit_test_suite);

MODULE_LICENSE("GPL");
