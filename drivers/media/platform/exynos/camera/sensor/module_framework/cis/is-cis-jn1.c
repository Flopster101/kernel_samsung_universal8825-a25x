/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
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
#include <linux/videodev2_exynos_camera.h>
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
#include "is-cis-jn1.h"
#include "is-cis-jn1-setA.h"
#include "is-cis-jn1-setB.h"

#include "is-helper-i2c.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"
#include "is-interface-sensor.h"
#define SENSOR_NAME "S5KJN1"
/* #define DEBUG_JN1_PLL */

static const u32 *sensor_jn1_global;
static u32 sensor_jn1_global_size;
static const u32 **sensor_jn1_setfiles;
static const u32 *sensor_jn1_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_jn1_pllinfos;
static u32 sensor_jn1_max_setfile_num;
static const u32 *sensor_jn1_dual_master_setfile;
static u32 sensor_jn1_dual_master_size;

static void sensor_jn1_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	cis_data->max_margin_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN;
	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n",
		cis_data->max_margin_coarse_integration_time);
}

static void sensor_jn1_set_integration_min(u32 mode, cis_shared_data *cis_data)
{
	FIMC_BUG_VOID(!cis_data);

	switch (mode) {
	case SENSOR_JN1_FULL_8160x6120_10FPS:
		cis_data->min_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_FULL;
		dbg_sensor(1, "FULL min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	case SENSOR_JN1_4SUM_4080x3060_30FPS:
	case SENSOR_JN1_4SUM_4080x2296_30FPS:
		cis_data->min_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM;
		dbg_sensor(1, "4SUM4BIN min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	case SENSOR_JN1_A2A2_2032x1524_120FPS:
		cis_data->min_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM_A2A2;
		dbg_sensor(1, "2SUM2BIN min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	default:
		err("[%s] Unsupported jn1 sensor mode\n", __func__);
		cis_data->min_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM;
		dbg_sensor(1, "min_coarse_integration_time(%d)\n",
			cis_data->min_coarse_integration_time);
		break;
	}
}

static void sensor_jn1_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact, cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz;
	u32 frame_rate, frame_valid_us;
	u64 max_fps;

	FIMC_BUG_VOID(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%llu), pclk(%llu)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)pll_info_compact->frame_length_lines) * pll_info_compact->line_length_pck * 1000
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
	max_fps = ((u64)vt_pix_clk_hz * 10) / (pll_info_compact->frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = (u32)max_fps;
	cis_data->frame_length_lines = pll_info_compact->frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (int)frame_valid_us;

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
	cis_data->min_fine_integration_time = SENSOR_JN1_FINE_INTEGRATION_TIME;
	cis_data->max_fine_integration_time = cis_data->line_length_pck;
	cis_data->min_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM;
	cis_data->max_margin_coarse_integration_time = SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN;
}

/*************************************************
 *  [jn1 Analog gain formular]
 *************************************************/

u32 sensor_jn1_cis_calc_again_code(u32 permile)
{
	return (permile * 32) / 1000;
}

u32 sensor_jn1_cis_calc_again_permile(u32 code)
{
	return (code * 1000) / 32;
}

#if WRITE_SENSOR_CAL_FOR_HW_GGC
int sensor_jn1_cis_HW_GGC_write(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;

	char *rom_cal_buf = NULL;

	u16 start_addr, data_size, write_data;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	ret = is_sec_get_cal_buf(&rom_cal_buf, ROM_ID_REAR);
	if (ret < 0) {
		goto p_err;
	}

	/* Big Endian */
	start_addr = SENSOR_JN1_HW_GGC_CAL_BASE_REAR;
	data_size = SENSOR_JN1_HW_GGC_CAL_SIZE;
	rom_cal_buf += start_addr;

#if SENSOR_JN1_CAL_DEBUG
	ret = sensor_jn1_cis_cal_dump(SENSOR_JN1_GGC_DUMP_NAME, (char *)rom_cal_buf, (size_t)SENSOR_JN1_HW_GGC_CAL_SIZE);
	if (ret < 0) {
		err("sensor_jn1_cis_cal_dump fail(%d)!\n", ret);
		goto p_err;
	}
#endif

	if (rom_cal_buf[0] == 0xFF && rom_cal_buf[1] == 0x00) {
		is_sensor_write16(client, 0x6028, 0x2400);
		is_sensor_write16(client, 0x602A, 0x0CFC);
		
		for (i = 0; i < (data_size/2) ; i++) {
			write_data = ((rom_cal_buf[2*i] << 8) | rom_cal_buf[2*i + 1]);
			is_sensor_write16(client, 0x6F12, write_data);
		}
		is_sensor_write16(client, 0x6028, 0x2400);
		is_sensor_write16(client, 0x602A, 0x2138);
		is_sensor_write16(client, 0x6F12, 0x4000);
	} else {
		err("sensor_jn1_cis_GGC_write skip : (%#x, %#x) \n", rom_cal_buf[0] , rom_cal_buf[1]);
		goto p_err;
	}

	dbg_sensor(1, "[%s] HW GGC write done\n", __func__);

/* for Test
	sensor_jn1_i2c_read16(client, SENSOR_JN1_HW_GGC_CAL_BASE_REAR, 5);
*/

p_err:
	return ret;
}
#endif

int sensor_jn1_cis_check_rev(struct is_cis *cis)
{
	int ret = 0;
	u8 status = 0;
	u8 rev = 0;
	u8 data1, data2 = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	is_sensor_write16(client, 0x6028, 0x4000);

	/* Specify OTP Page Address for READ - Page0(dec) */
	is_sensor_write16(client, SENSOR_JN1_OTP_PAGE_SETUP_ADDR, 0x0001);

	/* Turn ON OTP Read MODE - Read Start */
	is_sensor_write16(client, SENSOR_JN1_OTP_READ_TRANSFER_MODE_ADDR,  0x0100);

	/* Check status - 0x00 : read ready*/
	is_sensor_read8(client, SENSOR_JN1_OTP_STATUS_REGISTER_ADDR,  &status);
	if (status != 0x0) {
		err("status fail, (%d)", status);
		msleep(40);
	}

	ret = is_sensor_read8(client, SENSOR_JN1_OTP_CHIP_REVISION_ADDR, &rev);
	if (ret < 0) {
		err("is_sensor_read8 fail (ret %d)", ret);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		return ret;
	}
	ret = is_sensor_read8(client, 0x000D, &data1);
	ret = is_sensor_read8(client, 0x000E, &data2);
	
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis->cis_data->cis_rev = rev;

	probe_info("S5KJN1 Rev:%#x  (%#x, %#x)", rev, data1, data2);
	return 0;
}

/* CIS OPS */
int sensor_jn1_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
	ktime_t st = ktime_get();

	setinfo.return_value = 0;
	setinfo.param = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	FIMC_BUG(!cis->cis_data);
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	cis->rev_flag = false;

	ret = sensor_jn1_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_jn1_check_rev is fail when cis init");
		cis->rev_flag = true;
		ret = 0;
	}

	cis->cis_data->cur_width = SENSOR_JN1_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_JN1_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;

	sensor_jn1_cis_data_calculation(sensor_jn1_pllinfos[setfile_index], cis->cis_data);
	sensor_jn1_set_integration_max_margin(setfile_index, cis->cis_data);
	sensor_jn1_set_integration_min(setfile_index, cis->cis_data);

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
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_jn1_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		return -ENODEV;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -ENODEV;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	pr_err("[SEN:DUMP] *******************************\n");
	is_sensor_read16(client, 0x0000, &data16);
	pr_err("[SEN:DUMP] model_id(%x)\n", data16);
	is_sensor_read8(client, 0x0002, &data8);
	pr_err("[SEN:DUMP] revision_number(%x)\n", data8);
	is_sensor_read8(client, 0x0005, &data8);
	pr_err("[SEN:DUMP] frame_count(%x)\n", data8);
	is_sensor_read8(client, 0x0100, &data8);
	pr_err("[SEN:DUMP] mode_select(%x)\n", data8);

	sensor_cis_dump_registers(subdev, sensor_jn1_setfiles[0], sensor_jn1_setfile_sizes[0]);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	pr_err("[SEN:DUMP] *******************************\n");

	return ret;
}

static int sensor_jn1_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
#if USE_GROUP_PARAM_HOLD
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
#else
	return 0;
#endif
}

/*
  hold control register for updating multiple-parameters within the same frame. 
  true : hold, flase : no hold/release
*/
#if USE_GROUP_PARAM_HOLD
int sensor_jn1_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u32 mode;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);
 
	if (cis->cis_data->stream_on == false && hold == true) {
		ret = 0;
		dbg_sensor(1, "%s : sensor stream off skip group_param_hold", __func__);
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (mode == SENSOR_JN1_A2A2_2032x1524_120FPS) {
		ret = 0;
		dbg_sensor(1, "%s : fast ae skip group_param_hold", __func__);
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_jn1_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err_unlock;
	
p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

int sensor_jn1_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_jn1_global, sensor_jn1_global_size);

	if (ret < 0) {
		err("sensor_jn1_set_registers fail!!");
		goto p_err;
	}

	dbg_sensor(1, "[%s] global setting done\n", __func__);

#ifdef WRITE_SENSOR_CAL_FOR_HW_GGC
	sensor_jn1_cis_HW_GGC_write(subdev);
#endif

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (mode > sensor_jn1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	sensor_jn1_cis_data_calculation(sensor_jn1_pllinfos[mode], cis->cis_data);
	sensor_jn1_set_integration_max_margin(mode, cis->cis_data);
	sensor_jn1_set_integration_min(mode, cis->cis_data);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_cis_set_registers(subdev, sensor_jn1_setfiles[mode], sensor_jn1_setfile_sizes[mode]);
	if (ret < 0) {
		err("sensor_jn1_setfiles fail!!");
		goto p_err;
	}

#ifdef DISABLE_JN1_PDAF_TAIL_MODE
	ret = sensor_cis_set_registers(subdev, sensor_jn1_pdaf_off_setfile_B, ARRAY_SIZE(sensor_jn1_pdaf_off_setfile_B));
	if (ret < 0) {
		err("sensor_jn1_pdaf_off_setfiles fail!!");
		goto p_err;
	}
#endif

	cis->cis_data->frame_time = (cis->cis_data->line_readOut_time * cis->cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

/* Deprecated */
int sensor_jn1_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	return 0;
}

int sensor_jn1_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (cis->stream_state != CIS_STREAM_SET_DONE) {
		err("%s: cis stream state id %d", __func__, cis->stream_state);
		return -EINVAL;
	}

	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef DEBUG_JN1_PLL
	{
	u16 pll;

	is_sensor_read16(client, 0x0300, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	is_sensor_read16(client, 0x0302, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	is_sensor_read16(client, 0x0304, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	is_sensor_read16(client, 0x0306, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	is_sensor_read16(client, 0x0308, &pll);
	dbg_sensor(1, "______ op_pix_clk_div(%x)\n", pll);
	is_sensor_read16(client, 0x030a, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);

	is_sensor_read16(client, 0x030c, &pll);
	dbg_sensor(1, "______ secnd_pre_pll_clk_div(%x)\n", pll);
	is_sensor_read16(client, 0x030e, &pll);
	dbg_sensor(1, "______ secnd_pll_multiplier(%x)\n", pll);
	is_sensor_read16(client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	is_sensor_read16(client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

	/* Sensor stream on */
	is_sensor_write16(client, 0x6028, 0x4000);
	is_sensor_write8(client, 0x0100, 0x01);

	/* TODO: WDR */

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_stream_off(struct v4l2_subdev *subdev)
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
		return -EINVAL;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis_data->stream_on = false; /* for not working group_param_hold after stream off */

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = sensor_jn1_cis_group_param_hold_func(subdev, 0x00);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	/* Sensor stream off */
	is_sensor_write16(client, 0x6028, 0x4000);
	is_sensor_write8(client, 0x0100, 0x00);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	ktime_t st = ktime_get();

	u32 target_exp = 0;
	u32 target_frame_duration = 0;
	u16 frame_length_lines = 0;
	u64 max_fll_frame_time;

	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;

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
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	target_exp = target_exposure->val;
	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz (%d), line_length_pck(%d), min_fine_int (%d)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);

	max_fll_frame_time = (65535 * (u64)line_length_pck) / (u64)(vt_pic_clk_freq_khz / 1000);

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night Hyper Laps: min exp. is 1.5sec)
	 * If frame duration over than 1sec, then sequence is same as below
	 * 1. set CIT_LSHFIT
	 * 2. set COARSE_INTEGRATION_TIME
	 * 3. set FRM_LENGTH_LINES
	 */
	if ((cis_data->is_data.scene_mode == AA_SCENE_MODE_SUPER_NIGHT || cis_data->is_data.scene_mode == AA_SCENE_MODE_HYPERLAPSE)
		&& lte_mode->sen_strm_off_on_enable == false && cis_data ->cur_frame_us_time > max_fll_frame_time) {
		target_frame_duration = cis_data->cur_frame_us_time;
		dbg_sensor(1, "[MOD:D:%d] %s, input frame duration(%d) for CIT SHIFT \n",
			cis->id, __func__, target_frame_duration);

		if (target_frame_duration > 100000) {
			cit_lshift_val = (unsigned char)(target_frame_duration / 100000);
			while(cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_frame_duration /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			if (cit_lshift_count > SENSOR_JN1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_JN1_MAX_CIT_LSHIFT_VALUE;
		}

		frame_length_lines = (u16)((vt_pic_clk_freq_khz / 1000 * target_frame_duration) / (line_length_pck));

		cis_data->frame_length_lines = frame_length_lines;
		cis_data->frame_length_lines_shifter = cit_lshift_count;
		cis_data->max_coarse_integration_time =
			frame_length_lines - cis_data->max_margin_coarse_integration_time;

		dbg_sensor(1, "[MOD:D:%d] %s, target_frame_duration(%d), frame_length_line(%d), cit_lshift_count(%d)\n",
			cis->id, __func__, target_frame_duration, frame_length_lines, cit_lshift_count);
	}

	short_coarse_int = ((target_exp * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		if (cit_lshift_count > 0) {
			ret = is_sensor_write8(client, SENSOR_JN1_CIT_LSHIFT_ADDR, cit_lshift_count);
			if (ret < 0)
				goto i2c_err;

			ret = is_sensor_write8(client, SENSOR_JN1_FRM_LENGTH_LINE_LSHIFT_ADDR, cit_lshift_count);
			if (ret < 0)
				goto i2c_err;
		}
	}

	/* Short exposure */
	ret = is_sensor_write16(client, SENSOR_JN1_COARSE_INTEG_TIME_ADDR, short_coarse_int);
	if (ret < 0)
		goto i2c_err;

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		ret = is_sensor_write16(client, SENSOR_JN1_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
		if (ret < 0)
			goto i2c_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, pixelrate(%zu), llp(%d), fll(%d), min_fine_int(%d) cit(%d)\n", cis->id, __func__,
		vt_pic_clk_freq_khz, line_length_pck, cis_data->frame_length_lines, min_fine_int, short_coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_jn1_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
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
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz (%llu)\n", cis->id, __func__, vt_pic_clk_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)((u64)((line_length_pck * min_coarse) + min_fine) * 1000 / vt_pic_clk_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_jn1_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz (%llu)\n", cis->id, __func__, vt_pic_clk_freq_khz);
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

	/* TODO: Is this values update here? */
	cis_data->max_margin_fine_integration_time = max_fine_margin;
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max margin fine integration %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_margin_fine_integration_time,
			cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_jn1_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
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
		input_exposure_time = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
									- cis_data->min_fine_integration_time) / line_length_pck);

	if (cis->min_fps == cis->max_fps && cis->max_fps <= 60) {
		/*
		 * If input exposure is setted as 33333us@30fps from ddk
		 * then calculated frame duration is larger than 33333us because of CIT MARGIN.
		 */
		dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

		if (coarse_integ_time > cis_data->max_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input exp(%d) coarse(%u) max(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, input_exposure_time, coarse_integ_time, cis_data->max_coarse_integration_time);
			coarse_integ_time = cis_data->max_coarse_integration_time;
		}

		if (coarse_integ_time < cis_data->min_coarse_integration_time) {
			dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), input exp(%d) coarse(%u) min(%u)\n", cis->id, __func__,
				cis_data->sen_vsync_count, input_exposure_time, coarse_integ_time, cis_data->min_coarse_integration_time);
			coarse_integ_time = cis_data->min_coarse_integration_time;
		}
	}

	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;
	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duration(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	} else {
		*target_duration = frame_duration;
	}
	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_long_term_expo_mode *lte_mode;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u64 max_fll_frame_time;
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
	lte_mode = &cis->long_term_mode;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	max_fll_frame_time = (65535 * (u64)line_length_pck) / (u64)(vt_pic_clk_freq_khz / 1000);

	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night HyperLaps)
	 * If frame duration over than 1sec, then it has to be applied CIT shift.
	 * In this case, frame_duration is setted in set_exposure_time with CIT shift.
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data ->min_frame_us_time > max_fll_frame_time) {
		cis_data->cur_frame_us_time = frame_duration;
		dbg_sensor(1, "[MOD:D:%d][%s] Skip set frame duration(%d) for CIT SHIFT.\n",
			cis->id, __func__, frame_duration);
		return ret;
	}

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->frame_length_lines_shifter > 0) {
		cis_data->frame_length_lines_shifter = 0;
		ret = is_sensor_write8(client, SENSOR_JN1_CIT_LSHIFT_ADDR, 0);
		if (ret < 0) {
			goto i2c_err;
		}
		ret = is_sensor_write8(client, SENSOR_JN1_FRM_LENGTH_LINE_LSHIFT_ADDR, 0);
		if (ret < 0)
			goto i2c_err;
	}

	ret = is_sensor_write16(client, SENSOR_JN1_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0)
		goto i2c_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	if (lte_mode->sen_strm_off_on_enable) {
		cis_data->max_coarse_integration_time =
			SENSOR_JN1_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL - cis_data->max_margin_coarse_integration_time;
	} else {
		cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_jn1_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
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
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n",
			cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_jn1_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

#ifdef CAMERA_REAR2
	cis_data->min_frame_us_time = MAX(frame_duration, cis_data->min_sync_frame_us_time);
#else
	cis_data->min_frame_us_time = frame_duration;
#endif

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

int sensor_jn1_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
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

	again_code = sensor_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0])
		again_code = cis_data->max_analog_gain[0];
	else if (again_code < cis_data->min_analog_gain[0])
		again_code = cis_data->min_analog_gain[0];

	again_permile = sensor_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	int max_again = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0x4000;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0])
		analog_gain = cis->cis_data->min_analog_gain[0];

	if (cis->cis_data->sens_config_index_cur == SENSOR_JN1_FULL_8160x6120_10FPS)
		max_again = 0x200; /* not support x32,x64 again in full resolution */
	else
		max_again = cis->cis_data->max_analog_gain[0];

	if (analog_gain > max_again)
		analog_gain = max_again;

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write16(client, SENSOR_JN1_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0x4000;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, SENSOR_JN1_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_again_code = 0x20;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_analog_gain[0] = min_again_code;
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(min_again_code);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_again_code = 0x800;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_analog_gain[0] = max_again_code;
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(max_again_code);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 dgain_code = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0])
			dgain_code = cis->cis_data->min_digital_gain[0];

	if (dgain_code > cis->cis_data->max_digital_gain[0])
			dgain_code = cis->cis_data->max_digital_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us, dgain_code(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->long_val, dgain->short_val, dgain_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_write16(client, SENSOR_JN1_DIG_GAIN_ADDR, dgain_code);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_jn1_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0x4000;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, SENSOR_JN1_DIG_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_err;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_jn1_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 min_dgain_code = 0x0100;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->min_digital_gain[0] = min_dgain_code;
	cis_data->min_digital_gain[1] = sensor_cis_calc_dgain_permile(min_dgain_code);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_jn1_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u16 max_dgain_code = 0x8000;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;
	cis_data->max_digital_gain[0] = max_dgain_code;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(max_dgain_code);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static int sensor_jn1_cis_set_dual_master_setting(struct is_cis *cis)
{
	int ret = 0;
	struct i2c_client *client;

	FIMC_BUG(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		return -EINVAL;
	}

	info("[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* page 0x2000*/
	ret = is_sensor_write16(client, 0x6028, 0x4000);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x6028, 0x4000, ret);
	/* dual sync enable */
	ret = is_sensor_write16(client, 0x0A70, 0x0001);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x0A70, 0x0001, ret);
	/* master mode select */
	ret = is_sensor_write16(client, 0x0A72, 0x0100);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x0A72, 0x0100, ret);
	/* page 0x2000*/
	ret = is_sensor_write16(client, 0x6028, 0x2000);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x6028, 0x2000, ret);
	/* dual sync out index */
	ret = is_sensor_write16(client, 0x602A, 0x106A);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x602A, 0x106A, ret);
	ret = is_sensor_write16(client, 0x6F12, 0x0003);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x6F12, 0x0003, ret);
	/* master vsync out sel */
	ret = is_sensor_write16(client, 0x602A, 0x2BC2);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x602A, 0x2BC2, ret);
	ret = is_sensor_write16(client, 0x6F12, 0x0003);
	if (unlikely(ret))
		err("i2c treansfer fail addr(%x), val(%x), ret(%d)\n", 0x6F12, 0x0003, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_jn1_cis_set_dual_setting(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);

	switch (mode) {
	case DUAL_SYNC_MASTER:
		ret = sensor_jn1_cis_set_dual_master_setting(cis);
		if (ret)
			err("jn1 dual master setting fail");
		break;
	case DUAL_SYNC_SLAVE:
		break;
	case DUAL_SYNC_STANDALONE:
		break;
	default:
		err("%s: invalid mode(%d)\n", __func__, mode);
		ret = -EINVAL;
	}

	return ret;
}

int sensor_jn1_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_int = 0;
	u32 compensated_again = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 100) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			*again = compensated_again;
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}

int sensor_jn1_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	struct i2c_client *client = NULL;
	
	struct ae_param total_again;
	struct ae_param total_dgain;
	u32 cal_analog_val1 = 0;
	u32 cal_analog_val2 = 0;
	u32 cal_digital = 0;
	u32 analog_gain = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	cis_data = cis->cis_data;
	client = cis->client;
	total_again.val = again->val;
	total_again.short_val = again->short_val;
	total_dgain.long_val = dgain->val;
	total_dgain.short_val = dgain->short_val;
	ret = sensor_jn1_cis_set_exposure_time(subdev, target_exposure);
	if (ret < 0) {
		err("[%s] sensor_cis_set_exposure_time fail\n", __func__);
		goto p_err;
	}

	analog_gain = sensor_jn1_cis_calc_again_code(again->val);
	/* Set Digital gains */
	if (analog_gain == cis_data->max_analog_gain[0]) {
		total_dgain.long_val = dgain->val;
	} else {
		cal_analog_val1 = sensor_jn1_cis_calc_again_code(again->val);
		cal_analog_val2 = sensor_jn1_cis_calc_again_permile(cal_analog_val1);
		cal_digital = (again->val * SENSOR_JN1_MIN_CAL_DIGITAL) / cal_analog_val2;
		if (cal_digital < SENSOR_JN1_MIN_CAL_DIGITAL)
			cal_digital = SENSOR_JN1_MIN_CAL_DIGITAL;
		total_dgain.long_val = total_dgain.long_val * cal_digital / 1000;
		total_again.val = cal_analog_val2;
		dbg_sensor(1, "[%s] cal_analog_val1 = %d, cal_analog_val2 = %d cal_digital = %d\n",
			__func__, cal_analog_val1, cal_analog_val2, cal_digital);
		if (cal_digital < 0) {
			err("calculate dgain is fail");
			goto p_err;
		}
		dbg_sensor(1, "[%s] dgain compensation : input_again = %d, calculated_analog_gain = %d\n",
			__func__, again->val, cal_analog_val2);
	}
	
	ret = sensor_jn1_cis_set_analog_gain(subdev, &total_again);
	if (ret < 0) {
		err("[%s] sensor_cis_set_analog_gain fail\n", __func__);
		goto p_err;
	}

	ret = sensor_jn1_cis_set_digital_gain(subdev, &total_dgain);
	if (ret < 0) {
		err("[%s] sensor_cis_set_digital_gain fail\n", __func__);
		goto p_err;
	}

p_err:
	return ret;
}

void sensor_jn1_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode > sensor_jn1_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	/* If check_rev fail when cis_init, one more check_rev in mode_change */
	if (cis->rev_flag == true) {
		cis->rev_flag = false;
		ret = sensor_jn1_cis_check_rev(cis);
		if (ret < 0) {
			err("sensor_jn1_check_rev is fail: ret(%d)", ret);
			return;
		}
	}

	sensor_jn1_cis_data_calculation(sensor_jn1_pllinfos[mode], cis->cis_data);
}

int sensor_jn1_cis_long_term_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_long_term_expo_mode *lte_mode;
	unsigned char cit_lshift_val = 0;
	int cit_lshift_count = 0;
	u32 target_exp = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	lte_mode = &cis->long_term_mode;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* LTE mode or normal mode set */
	if (lte_mode->sen_strm_off_on_enable) {
		target_exp = lte_mode->expo[0];
		if (target_exp >= 125000) {
			cit_lshift_val = (unsigned char)(target_exp / 125000);
			while (cit_lshift_val > 1) {
				cit_lshift_val /= 2;
				target_exp /= 2;
				cit_lshift_count++;
			}

			lte_mode->expo[0] = target_exp;

			if (cit_lshift_count > SENSOR_JN1_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_JN1_MAX_CIT_LSHIFT_VALUE;
		}
	}

	ret = is_sensor_write8(cis->client, SENSOR_JN1_CIT_LSHIFT_ADDR, cit_lshift_count);
	ret |= is_sensor_write8(cis->client, SENSOR_JN1_FRM_LENGTH_LINE_LSHIFT_ADDR, cit_lshift_count);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	info("[%s] sen_strm_enable(%d), cit_lshift_count (%d), target_exp(%d)", __func__,
		lte_mode->sen_strm_off_on_enable, cit_lshift_count, lte_mode->expo[0]);

	if (ret < 0)
		pr_err("ERR[%s]: LTE register setting fail\n", __func__);
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_jn1_cis_init,
	.cis_log_status = sensor_jn1_cis_log_status,
#if USE_GROUP_PARAM_HOLD
	.cis_group_param_hold = sensor_jn1_cis_group_param_hold,
#endif
	.cis_set_global_setting = sensor_jn1_cis_set_global_setting,
	.cis_mode_change = sensor_jn1_cis_mode_change,
	.cis_set_size = sensor_jn1_cis_set_size,
	.cis_stream_on = sensor_jn1_cis_stream_on,
	.cis_stream_off = sensor_jn1_cis_stream_off,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_jn1_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_jn1_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_jn1_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_jn1_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_jn1_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_jn1_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_jn1_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_jn1_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_jn1_cis_get_max_analog_gain,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_jn1_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_jn1_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_jn1_cis_get_max_digital_gain,
	.cis_set_totalgain = sensor_jn1_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_jn1_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_active_test = sensor_cis_active_test,
	.cis_set_long_term_exposure = sensor_jn1_cis_long_term_exposure,
	.cis_set_dual_setting = sensor_jn1_cis_set_dual_setting,
	.cis_data_calculation = sensor_jn1_cis_data_calc,
};

static int cis_jn1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	int i;
	int index;
	const int *verify_sensor_mode = NULL;
	int verify_sensor_mode_size = 0;

	struct device_node *dnode = client->dev.of_node;

	probe_info("%s: sensor_cis_probe started\n", __func__);	//custom
	ret = sensor_cis_probe(client, id, &sensor_peri);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		goto p_err;			//return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	/* Use total gain instead of using dgain */
	cis->use_dgain = false;
	cis->use_vendor_total_gain = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}
	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setB") == 0) {
		is_info("%s setfile_B\n", __func__);
#if SENSOR_JN1_BURST_WRITE
		sensor_jn1_global = sensor_jn1_setfile_B_Global_Burst;
		sensor_jn1_global_size = ARRAY_SIZE(sensor_jn1_setfile_B_Global_Burst);
#else
		sensor_jn1_global = sensor_jn1_setfile_B_Global;
		sensor_jn1_global_size = ARRAY_SIZE(sensor_jn1_setfile_B_Global);
#endif
		sensor_jn1_setfiles = sensor_jn1_setfiles_B;
		sensor_jn1_setfile_sizes = sensor_jn1_setfile_B_sizes;
		sensor_jn1_pllinfos = sensor_jn1_pllinfos_B;
		sensor_jn1_max_setfile_num = ARRAY_SIZE(sensor_jn1_setfiles_B);
		sensor_jn1_dual_master_setfile = sensor_jn1_dual_master_setfile_B;
		sensor_jn1_dual_master_size = ARRAY_SIZE(sensor_jn1_dual_master_setfile_B);
	} else {
		is_info("%s setfile_A (rev = %#x : Old Version)\n", __func__, cis->cis_data->cis_rev);
		sensor_jn1_global = sensor_jn1_setfile_A_Global;
		sensor_jn1_global_size = ARRAY_SIZE(sensor_jn1_setfile_A_Global);
		sensor_jn1_setfiles = sensor_jn1_setfiles_A;
		sensor_jn1_setfile_sizes = sensor_jn1_setfile_A_sizes;
		sensor_jn1_pllinfos = sensor_jn1_pllinfos_A;
		sensor_jn1_max_setfile_num = ARRAY_SIZE(sensor_jn1_setfiles_A);
		sensor_jn1_dual_master_setfile = sensor_jn1_dual_master_setfile_A;
		sensor_jn1_dual_master_size = ARRAY_SIZE(sensor_jn1_dual_master_setfile_A);

		cis->mipi_sensor_mode = sensor_jn1_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_jn1_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_jn1_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_jn1_setfile_A_verify_sensor_mode);
	}

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

p_err:
	return ret;
}

static const struct of_device_id sensor_cis_jn1_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-jn1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_jn1_match);

static const struct i2c_device_id sensor_cis_jn1_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_jn1_driver = {
	.probe	= cis_jn1_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_jn1_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_jn1_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_jn1_driver);
#else
static int __init sensor_cis_jn1_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_jn1_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_jn1_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_jn1_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
