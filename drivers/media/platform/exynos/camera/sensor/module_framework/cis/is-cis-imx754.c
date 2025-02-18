// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-imx754.h"
#include "is-vender.h"
#ifndef CONFIG_CAMERA_VENDER_MCD
#include "is-cis-imx754-setA.h"
#include "is-cis-imx754-setB.h"
#endif
#include "is-cis-imx754-setA-19p2.h"
#include "is-cis-imx754-setB-19p2.h"
#include "is-helper-i2c.h"
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
#include "is-sec-define.h"
#include "is-vender.h"
#include "is-vender-specific.h"
#endif

#define SENSOR_NAME "IMX754"

static const struct v4l2_subdev_ops subdev_ops;

#ifdef CAMERA_IMX754_10X_FLIP
static const u32 *sensor_imx754_10x_flip_global;
static u32 sensor_imx754_10x_flip_global_size;
static const u32 **sensor_imx754_10x_flip_setfiles;
static const u32 *sensor_imx754_10x_flip_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_imx754_10x_flip_pllinfos;
static u32 sensor_imx754_10x_flip_max_setfile_num;
#endif

static const u32 *sensor_imx754_global;
static u32 sensor_imx754_global_size;
static const u32 **sensor_imx754_setfiles;
static const u32 *sensor_imx754_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_imx754_pllinfos;
static u32 sensor_imx754_max_setfile_num;

int sensor_imx754_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps);

u32 sensor_imx754_cis_calc_again_code(u32 permille)
{
	return 16384 - (16384000 / permille);
}

u32 sensor_imx754_cis_calc_again_permile(u32 code)
{
	return 16384000 / (16384 - code);
}

static void sensor_imx754_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
	cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz;
	u32 frame_rate, max_fps, frame_valid_us;

	FIMC_BUG_VOID(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%llu), pclk(%llu)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info_compact->frame_length_lines) *
			pll_info_compact->line_length_pck * 1000
					/ (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif

	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%llu) / "
		KERN_CONT "(pll_info_compact->frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, pll_info_compact->frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (unsigned int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);
	dbg_sensor(1, "rolling_shutter_skew: %lld\n", cis_data->rolling_shutter_skew);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_IMX754_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_IMX754_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time = SENSOR_IMX754_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_IMX754_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

void sensor_imx754_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;
	u32 max_setfile_num = sensor_imx754_max_setfile_num;
	const struct sensor_pll_info_compact **pllinfos = sensor_imx754_pllinfos;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		max_setfile_num = sensor_imx754_10x_flip_max_setfile_num;
		pllinfos = sensor_imx754_10x_flip_pllinfos;
	}
#endif

	if (mode >= max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	sensor_imx754_cis_data_calculation(pllinfos[mode], cis->cis_data);
}

static int sensor_imx754_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	FIMC_BUG(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

/* CIS OPS */
int sensor_imx754_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 idx;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
	const struct sensor_pll_info_compact **pllinfos = sensor_imx754_pllinfos;

	ktime_t st = ktime_get();

	setinfo.param = NULL;
	setinfo.return_value = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!cis->cis_data);
#if !defined(CONFIG_CAMERA_VENDER_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_imx754_check_rev is fail when cis init, ret(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}
#endif

	cis->cis_data->cur_width = SENSOR_IMX754_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_IMX754_MAX_HEIGHT;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_enable = false;
#ifdef USE_CAMERA_ADAPTIVE_MIPI
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
#endif
	for (idx = 0; idx < SENSOR_IMX754_MODE_MAX; ++idx)
		start_pos_infos[idx].x_start = start_pos_infos[idx].y_start = 0;

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		pllinfos = sensor_imx754_10x_flip_pllinfos;
	}
#endif

	sensor_imx754_cis_data_calculation(pllinfos[setfile_index], cis->cis_data);

	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_exposure_time, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max exposure time : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, setinfo.return_value);
	setinfo.return_value = 0;
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &setinfo.return_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, setinfo.return_value);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

static const struct is_cis_log log_imx754[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0018, 0, "0x0018"},
	{I2C_READ, 8, 0x0A04, 0, "0x0A04"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0204, 0, "0x0204"},
	{I2C_READ, 16, 0x020E, 0, "0x020E"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 8, 0x3150, 0, "0x3150"},
	{I2C_READ, 8, 0x3151, 0, "0x3151"},
	{I2C_READ, 8, 0x3860, 0, "0x3860"},
};

int sensor_imx754_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_cis_log_status(cis, client, log_imx754,
			ARRAY_SIZE(log_imx754), (char *)__func__);

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_imx754_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = is_sensor_write8(client, 0x0104, hold);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = 1;
p_err:
	return ret;
}
#else
static inline int sensor_imx754_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *      return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_imx754_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_imx754_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_imx754_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client;
	const u32 *global_setting = sensor_imx754_global;
	u32 global_size = sensor_imx754_global_size;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* setfile global setting is at camera entrance */
	info("[%d][%s] global setting enter\n", cis->id, __func__);

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		global_setting = sensor_imx754_10x_flip_global;
		global_size = sensor_imx754_10x_flip_global_size;
	}
#endif

	ret = sensor_cis_set_registers(subdev, global_setting, global_size);

	if (ret < 0) {
		err("[%d]sensor_imx754_set_registers fail!!", cis->id);
		goto p_err;
	}
	info("[%d][%s] global setting done\n", cis->id, __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
//#define TEST_SHIFT_CROP

#ifdef TEST_SHIFT_CROP
int test_crop_num = 0;
#endif

int sensor_imx754_cis_update_crop_region(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client;
	struct is_device_sensor *device;
	s16 delta_x, delta_y;
	long efs_size = 0;
	struct is_core *core;
	struct is_vender_specific *specific;
	s16 temp_delta = 0;

#ifdef TEST_SHIFT_CROP
	int16_t crop[10][2] = {
		{0,       0}, // default ROI {176, 132}
		{-176, -132}, {0, -132}, {176, -132}, // {0,   0}, {176,   0}, {352 ,  0}
		{-176,    0}, {0,    0}, {176,    0}, // {0, 132}, {176, 132}, {352, 132}
		{-176,  132}, {0,  132}, {176,  132}, // {0, 264}, {176, 264}, {352, 264}
	};
#endif

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (device == NULL) {
		err("device is NULL");
		return -1;
	}

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -1;
	}

	if (device->cfg->mode == SENSOR_IMX754_1920X1080_120FPS
		|| device->cfg->mode == SENSOR_IMX754_1920X1080_240FPS
		|| device->cfg->mode == SENSOR_IMX754_912X684_120FPS) {
		info("[%s] skip crop shift in binning & fast ae sensor mode\n", __func__);
		return 0;
	}

	core = is_get_is_core();
	specific = core->vender.private_data;

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		efs_size = specific->tilt_cal_tele2_efs_size;
		delta_x = *((s16 *)&specific->tilt_cal_tele2_efs_data[CROP_SHIFT_ADDR_X]);
		delta_y = *((s16 *)&specific->tilt_cal_tele2_efs_data[CROP_SHIFT_ADDR_Y]);
	} else
#endif
	{
		efs_size = specific->tilt_cal_tele_efs_size;
		delta_x = *((s16 *)&specific->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_X]);
		delta_y = *((s16 *)&specific->tilt_cal_tele_efs_data[CROP_SHIFT_ADDR_Y]);
	}

	if (efs_size == 0) {
		delta_x = 0;
		delta_y = 0;
	}

	info("[%s] efs_size[%d], delta_x[%d], delta_y[%d]\n", __func__, efs_size, delta_x, delta_y);

	if (delta_x % CROP_SHIFT_DELTA_ALIGN_X) {
		temp_delta = delta_x / CROP_SHIFT_DELTA_ALIGN_X;
		delta_x = temp_delta * CROP_SHIFT_DELTA_ALIGN_X;
	}

	if (delta_y % CROP_SHIFT_DELTA_ALIGN_Y) {
		temp_delta = delta_y / CROP_SHIFT_DELTA_ALIGN_Y;
		delta_y = temp_delta * CROP_SHIFT_DELTA_ALIGN_Y;
	}

	ret = is_sensor_read16(client, 0x0408, &x_start);
	ret = is_sensor_read16(client, 0x040A, &y_start);

#ifdef TEST_SHIFT_CROP
	x_start += crop[test_crop_num][0];
	y_start += crop[test_crop_num][1];

	if (test_crop_num == 9)
		test_crop_num = 0;
	else
		test_crop_num++;
#else
	x_start += delta_x;
	y_start += delta_y;
#endif

	ret = is_sensor_write16(client, 0x0408, x_start);
	ret = is_sensor_write16(client, 0x040A, y_start);

	info("[%s] delta_x[%d], delta_y[%d] x_start(%d), y_start(%d)\n",
		__func__, delta_x, delta_y, x_start, y_start);

	start_pos_infos[device->cfg->mode].x_start = x_start;
	start_pos_infos[device->cfg->mode].y_start = y_start;

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

	return ret;
}

int sensor_imx754_cis_set_crop_region(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u16 x_start = 0;
	u16 y_start = 0;
	struct is_cis *cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	struct is_device_sensor *device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);

	if (!(device->position == SENSOR_POSITION_REAR2 ||
	    cis->id == CAMERA_IMX754_10X_FLIP))
		return 0;

	x_start = start_pos_infos[mode].x_start;
	y_start = start_pos_infos[mode].y_start;

	info("[%d][%s] final lsc crop offset : mode(%d), x_start(%d), y_start(%d)\n",
		cis->id, __func__,
		mode, x_start, y_start);

	cis->freeform_sensor_crop.enable = true;
	cis->freeform_sensor_crop.x_start = x_start;
	cis->freeform_sensor_crop.y_start = y_start;

	return ret;
}
#endif

int sensor_imx754_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device;
	struct is_device_sensor_peri *sensor_peri;
	const u32 **setfiles = sensor_imx754_setfiles;
	const u32 *setfile_sizes = sensor_imx754_setfile_sizes;
	u32 max_setfile_num = sensor_imx754_max_setfile_num;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	if (unlikely(!sensor_peri)) {
		err("sensor peri is NULL");
		return -EINVAL;
	}

#ifdef CAMERA_IMX754_10X_FLIP
	if (cis->id == CAMERA_IMX754_10X_FLIP) {
		setfiles = sensor_imx754_10x_flip_setfiles;
		setfile_sizes = sensor_imx754_10x_flip_setfile_sizes;
		max_setfile_num = sensor_imx754_10x_flip_max_setfile_num;
	}
#endif

	if (mode >= max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, setfiles[mode], setfile_sizes[mode]);
	if (ret < 0) {
		err("[%d]sensor_imx754_set_registers fail!!", cis->id);
		goto p_err_i2c_unlock;
	}

	info("[%d][%s] mode changed(%d)\n", cis->id, __func__, mode);

	/* EMB Header off */
	ret |= is_sensor_write8(cis->client, 0x3860, 0x00);

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
	if (device->position == SENSOR_POSITION_REAR2 || cis->id == CAMERA_IMX754_10X_FLIP) {
		ret = sensor_imx754_cis_update_crop_region(subdev);
		if (ret < 0) {
			err("sensor_imx754_cis_update_crop_region fail!!");
			goto p_err_i2c_unlock;
		}
	}
#endif

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	/* make first frame to 24fps for live focus sync issue */
	if (device->image.framerate == 24)
		sensor_imx754_cis_set_frame_rate(subdev, 24);

	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_imx754_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_imx754_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_IMX754_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_IMX754_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_IMX754_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_IMX754_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = is_sensor_write16(client, 0x6028, 0x2000);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_IMX754_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_IMX754_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_write16(client, 0x0344, start_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0346, start_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0348, end_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x034A, end_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 3. output address setting */
	ret = is_sensor_write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err_i2c_unlock;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = is_sensor_write16(client, 0x0380, even_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0382, odd_x);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0384, even_y);
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write16(client, 0x0386, odd_y);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 5. binnig setting */
	ret = is_sensor_write8(client, 0x0900, binning); /* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err_i2c_unlock;
	ret = is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full) */
	ret = is_sensor_write16(client, 0x0400, 0x0000);
	if (ret < 0)
		goto p_err_i2c_unlock;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed)) */
	/* down scale factor = down_scale_m / down_scale_n */
	ret = is_sensor_write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err_i2c_unlock;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	struct is_device_sensor *device;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream on");

	if (cis->cis_data->is_data.scene_mode == AA_SCENE_MODE_BOKEH_VIDEO
		|| cis->cis_data->is_data.scene_mode == AA_SCENE_MODE_LIVE_OUTFOCUS) {
		info("[%s] dual sync always master\n", __func__);
		ret = sensor_cis_set_registers(subdev, sensor_imx754_cis_dual_master_settings, sensor_imx754_cis_dual_master_settings_size);
	}

	/* Sensor stream on */
	is_sensor_write8(client, 0x0100, 0x01);

	info("[%d]%s\n", cis->id, __func__);

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0600, 0x0000);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			I2C_MUTEX_LOCK(cis->i2c_lock);
			is_sensor_write16(client, 0x0600, 0x0001);
			is_sensor_write8(client, 0x3787, 0x02);
			I2C_MUTEX_UNLOCK(cis->i2c_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

p_err:
	return ret;
}

int sensor_imx754_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	/* Sensor stream off */
	is_sensor_write8(client, 0x0100, 0x00);

	info("[%d]%s\n", cis->id, __func__);

	cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u8 coarse_integration_time_shifter = 0;

	u16 cit_shifter_array[17] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5};
	u16 cit_shifter_val = 0;
	int cit_shifter_idx = 0;
	u8 cit_denom_array[6] = {1, 2, 4, 8, 16, 32};
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%d][%s] invalid target exposure(%d, %d)\n", cis->id, __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
		default:
			if (MAX(target_exposure->long_val, target_exposure->short_val) > 250000) {
				cit_shifter_idx = MIN(MAX(MAX(target_exposure->long_val, target_exposure->short_val) / 250000, 0), 16);
				cit_shifter_val = MAX(cit_shifter_array[cit_shifter_idx], cis_data->frame_length_lines_shifter);
			} else {
				cit_shifter_val = (u16)(cis_data->frame_length_lines_shifter);
			}
			target_exposure->long_val = target_exposure->long_val / cit_denom_array[cit_shifter_val];
			target_exposure->short_val = target_exposure->short_val / cit_denom_array[cit_shifter_val];
			coarse_integration_time_shifter = ((cit_shifter_val<<8) & 0xFF00) + (cit_shifter_val & 0x00FF);
			break;
		}
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		long_coarse_int = cis_data->max_coarse_integration_time;
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int);
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		short_coarse_int = cis_data->max_coarse_integration_time;
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int);
	}

	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		long_coarse_int = cis_data->min_coarse_integration_time;
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int);
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		short_coarse_int = cis_data->min_coarse_integration_time;
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int);
	}

	if (short_coarse_int > long_coarse_int) {
		dbg_sensor(1, "[MOD:D:%d] %s, long coarse(%d), short coarse(%d) max\n", cis->id, __func__,
			long_coarse_int, short_coarse_int);
		long_coarse_int = short_coarse_int;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, frame_length_lines(%#x), long_coarse_int %#x, short_coarse_int %#x\n",
		cis->id, __func__, cis_data->frame_length_lines, long_coarse_int, short_coarse_int);

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short exposure */
	ret = is_sensor_write16(client, 0x0202, short_coarse_int);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	/* CIT shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = is_sensor_write8(client, 0x3150, coarse_integration_time_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx754_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_fine_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;

	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_fine_margin = line_length_pck - cis_data->min_fine_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)((u64)((line_length_pck * max_coarse) + max_fine) * 1000 / vt_pic_clk_freq_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time,
			cis_data->max_margin_fine_integration_time,
			cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx754_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%d][%s] Not proper exposure time(0), apply min frame duration to exposure time forcely!(%d)\n",
			cis->id, __func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (((vt_pic_clk_freq_khz * input_exposure_time) / 1000) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s] input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s] adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx754_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;

	u8 frame_length_lines_shifter = 0;

	u8 fll_shifter_array[17] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5};
	int fll_shifter_idx = 0;
	u8 fll_denom_array[6] = {1, 2, 4, 8, 16, 32};
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	cis_data->cur_frame_us_time = frame_duration;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
		default:
			if (frame_duration > 250000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 250000, 0), 16);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x00;
			}
			break;
		}
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_khz, frame_duration,
			line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_write16(client, 0x0340, frame_length_lines);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* frame duration shifter */
	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		ret = is_sensor_write8(client, 0x3151, frame_length_lines_shifter);
		if (ret < 0)
			goto p_err_i2c_unlock;
	}
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	cis_data->frame_length_lines_shifter = frame_length_lines_shifter;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_imx754_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_imx754_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	unsigned char shift_count = 0;
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
	u32 lte_expousre = 0;
#endif
	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		if (lte_mode->expo[0] > 250000) {
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
			lte_expousre = lte_mode->expo[0];
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 250000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				lte_expousre = lte_expousre / 2;
				shift_count++;
			}
			lte_mode->expo[0] = lte_expousre;
#else
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 250000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				if (cit_lshift_val > 0)
					shift_count++;
			}
			lte_mode->expo[0] = 250000;
#endif
			ret |= is_sensor_write8(cis->client, 0x3150, shift_count);
			ret |= is_sensor_write8(cis->client, 0x3151, shift_count);
		}
	} else {
		cit_lshift_val = 0;
		ret |= is_sensor_write8(cis->client, 0x3150, 0x00);
		ret = is_sensor_write8(cis->client, 0x3151, 0x00);
	}

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("%s enable(%d) shift_count(%d) exp(%d)",
		__func__, lte_mode->sen_strm_off_on_enable, shift_count, lte_mode->expo[0]);

	if (ret < 0) {
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
		return ret;
	}

	return ret;
}

int sensor_imx754_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_imx754_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

	again_permile = sensor_imx754_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx754_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 short_gain = 0;
	u16 long_gain = 0;

	ktime_t st = ktime_get();

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;

	do_gettimeofday(&st);
#endif

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	short_gain = (u16)sensor_imx754_cis_calc_again_code(again->short_val);
	long_gain = (u16)sensor_imx754_cis_calc_again_code(again->long_val);

	if (long_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to min_analog_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->min_analog_gain[0];
	}
	if (long_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to max_analog_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->max_analog_gain[0];
	}

	if (short_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to min_analog_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->min_analog_gain[0];
	}
	if (short_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to max_analog_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d),"
		KERN_CONT "input short gain = %d us, input long gain = %d us,"
		KERN_CONT "analog short gain(%#x), analog short gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count,
		again->short_val, again->long_val,
		short_gain, long_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short analog gain */
	ret = is_sensor_write16(client, 0x0204, short_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));


p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*again = sensor_imx754_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_analog_gain[0] = 0x0; /* x1 */
	cis_data->min_analog_gain[1] = sensor_imx754_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx754_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_analog_gain[0] = 0x3800; /* x8 */
	cis_data->max_analog_gain[1] = sensor_imx754_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx754_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to min_digital_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->min_digital_gain[0];
	}
	if (long_gain > cis->cis_data->max_digital_gain[0]) {
		info("[%d][%s] not proper long_gain value, reset to max_digital_gain\n", cis->id, __func__);
		long_gain = cis->cis_data->max_digital_gain[0];
	}

	if (short_gain < cis->cis_data->min_digital_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to min_digital_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->min_digital_gain[0];
	}
	if (short_gain > cis->cis_data->max_digital_gain[0]) {
		info("[%d][%s] not proper short_gain value, reset to max_digital_gain\n", cis->id, __func__);
		short_gain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s, input_dgain = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, dgain->long_val, dgain->short_val, long_gain, short_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	/* Short digital gain */
	ret = is_sensor_write16(client, 0x020E, short_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));


p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x01);
	if (hold < 0) {
		ret = hold;
		goto p_err_i2c_unlock;
	}

	ret = is_sensor_read16(client, 0x020E, &digital_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	if (hold > 0) {
		hold = sensor_imx754_cis_group_param_hold_func(subdev, 0x00);
		if (hold < 0)
			ret = hold;
	}
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_imx754_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = 0x100;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->min_digital_gain[0]);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx754_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = 0xFD9;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_imx754_cis_init,
	.cis_log_status = sensor_imx754_cis_log_status,
	.cis_group_param_hold = sensor_imx754_cis_group_param_hold,
	.cis_set_global_setting = sensor_imx754_cis_set_global_setting,
	.cis_mode_change = sensor_imx754_cis_mode_change,
	.cis_set_size = sensor_imx754_cis_set_size,
	.cis_stream_on = sensor_imx754_cis_stream_on,
	.cis_stream_off = sensor_imx754_cis_stream_off,
	.cis_set_exposure_time = sensor_imx754_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_imx754_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_imx754_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_imx754_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_imx754_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_imx754_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_imx754_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_imx754_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_imx754_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_imx754_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_imx754_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_imx754_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_imx754_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_imx754_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_imx754_cis_get_max_digital_gain,
	.cis_data_calculation = sensor_imx754_cis_data_calc,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_test_pattern = sensor_imx754_cis_set_test_pattern,
	.cis_set_long_term_exposure = sensor_imx754_cis_long_term_exposure,
	.cis_set_crop_region = sensor_imx754_cis_set_crop_region,
};

static int cis_imx754_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	int i;
	int index;
	const int *verify_sensor_mode = NULL;
	int verify_sensor_mode_size = 0;

	ret = sensor_cis_probe(client, id, &sensor_peri);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	if (of_property_read_string(dnode, "setfile", &setfile)) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			sensor_imx754_global = sensor_imx754_setfile_A_19p2_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_A_19p2;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_A_19p2_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_A_19p2;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_imx754_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_verify_sensor_mode);
		} else if (strcmp(setfile, "setB") == 0) {
			probe_info("%s setfile_B mclk: 19.2MHz\n", __func__);
#ifdef CAMERA_IMX754_10X_FLIP
			sensor_imx754_10x_flip_global = sensor_imx754_setfile_B_19p2_Global;
			sensor_imx754_10x_flip_global_size = ARRAY_SIZE(sensor_imx754_setfile_B_19p2_Global);
			sensor_imx754_10x_flip_setfiles = sensor_imx754_setfiles_B_19p2;
			sensor_imx754_10x_flip_setfile_sizes = sensor_imx754_setfile_B_19p2_sizes;
			sensor_imx754_10x_flip_pllinfos = sensor_imx754_pllinfos_B_19p2;
			sensor_imx754_10x_flip_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_B_19p2);
#else
			sensor_imx754_global = sensor_imx754_setfile_B_19p2_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_B_19p2_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_B_19p2;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_B_19p2_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_B_19p2;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_B_19p2);
#endif
			cis->mipi_sensor_mode = sensor_imx754_setfile_B_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_B_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_B_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_B_19p2_verify_sensor_mode);
		} else {
			err("%s setfile index out of bound, take default (setfile_A)", __func__);
			sensor_imx754_global = sensor_imx754_setfile_A_19p2_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_A_19p2;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_A_19p2_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_A_19p2;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_imx754_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_19p2_verify_sensor_mode);
		}
	}
#ifndef CONFIG_CAMERA_VENDER_MCD
	else {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A\n", __func__);
			sensor_imx754_global = sensor_imx754_setfile_A_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_A_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_A;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_A_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_A;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_A);
			cis->mipi_sensor_mode = sensor_imx754_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_verify_sensor_mode);
		} else if (strcmp(setfile, "setB") == 0) {
			probe_info("%s setfile_B\n", __func__);
			sensor_imx754_global = sensor_imx754_setfile_B_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_B_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_B;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_B_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_B;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_B);
			cis->mipi_sensor_mode = sensor_imx754_setfile_B_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_B_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_B_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_B_verify_sensor_mode);
		} else {
			err("%s setfile index out of bound, take default (setfile_A)", __func__);
			sensor_imx754_global = sensor_imx754_setfile_A_Global;
			sensor_imx754_global_size = ARRAY_SIZE(sensor_imx754_setfile_A_Global);
			sensor_imx754_setfiles = sensor_imx754_setfiles_A;
			sensor_imx754_setfile_sizes = sensor_imx754_setfile_A_sizes;
			sensor_imx754_pllinfos = sensor_imx754_pllinfos_A;
			sensor_imx754_max_setfile_num = ARRAY_SIZE(sensor_imx754_setfiles_A);
			cis->mipi_sensor_mode = sensor_imx754_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_imx754_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_imx754_setfile_A_verify_sensor_mode);
		}
	}
#endif

	if (cis->vendor_use_adaptive_mipi) {
		for (i = 0; i < verify_sensor_mode_size; i++) {
			index = verify_sensor_mode[i];

			if (index >= cis->mipi_sensor_mode_size || index < 0) {
				panic("wrong mipi_sensor_mode index");
				break;
			}

			if (is_vendor_verify_mipi_channel(cis->mipi_sensor_mode[index].mipi_channel,
						cis->mipi_sensor_mode[index].mipi_channel_size)) {
				panic("wrong mipi channel");
				break;
			}
		}
	}

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_cis_imx754_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx754",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx754_match);

static const struct i2c_device_id sensor_cis_imx754_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx754_driver = {
	.probe	= cis_imx754_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx754_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx754_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx754_driver);
#else
static int __init sensor_cis_imx754_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx754_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx754_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx754_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
