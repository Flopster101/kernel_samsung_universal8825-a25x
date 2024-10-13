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
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
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
#include "is-cis-hi1339.h"

#include "is-cis-hi1339-setA.h"
#include "is-cis-hi1339-setB.h"

#include "is-helper-i2c.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"

#define SENSOR_NAME "HI1339"

#define POLL_TIME_MS 5
#define STREAM_MAX_POLL_CNT 60

static const u32 *sensor_hi1339_global;
static u32 sensor_hi1339_global_size;
static const u32 **sensor_hi1339_setfiles;
static const u32 *sensor_hi1339_setfile_sizes;
static u32 sensor_hi1339_max_setfile_num;
static const struct sensor_pll_info_compact **sensor_hi1339_pllinfos;
//static struct setfile_info *sensor_hi1339_setfile_fsync_info;

static void sensor_hi1339_cis_data_calculation(const struct sensor_pll_info_compact *pll_info, cis_shared_data *cis_data)
{
	u64 pixel_rate;
	u32 total_line_length_pck;
	u32 frame_rate, max_fps, frame_valid_us;

	FIMC_BUG_VOID(!pll_info);

	/* 1. get pclk value from pll info */
	pixel_rate = pll_info->pclk; 
	total_line_length_pck = pll_info->line_length_pck;

	/* 2. FPS calculation */
	frame_rate = pixel_rate / (pll_info->frame_length_lines * total_line_length_pck);

	/* 3. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (1 * 1000 * 1000) / frame_rate;
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;

	dbg_sensor(1, "frame_duration(%d) - frame_rate (%d) = pixel_rate(%llu) / "
		KERN_CONT "(pll_info->frame_length_lines(%d) * total_line_length_pck(%d))\n",
		cis_data->min_frame_us_time, frame_rate, pixel_rate, pll_info->frame_length_lines, total_line_length_pck);

	/* calculate max fps */
	max_fps = (pixel_rate * 10) / (pll_info->frame_length_lines * total_line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = pixel_rate;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = pll_info->frame_length_lines;
	cis_data->line_length_pck = total_line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;
	cis_data->stream_on = false;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n", cis_data->cur_width, cis_data->cur_height);
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
	cis_data->min_fine_integration_time          = SENSOR_HI1339_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time          = SENSOR_HI1339_FINE_INTEGRATION_TIME_MAX;
	cis_data->min_coarse_integration_time        = SENSOR_HI1339_COARSE_INTEGRATION_TIME_MIN;
	cis_data->max_margin_coarse_integration_time = SENSOR_HI1339_COARSE_INTEGRATION_TIME_MAX_MARGIN;

	info("%s: done", __func__);
}

/*************************************************
 *  [HI1339 Analog gain formular]
 *
 *  Analog Gain = (Reg value)/16 + 1
 *
 *  Analog Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_hi1339_cis_calc_again_code(u32 permile)
{
	return ((permile - 1000) * 16 / 1000);
}

u32 sensor_hi1339_cis_calc_again_permile(u32 code)
{
	return ((code * 1000 / 16 + 1000));
}

/*************************************************
 *  [HI1339 Digital gain formular]
 *
 *  Digital Gain = bit[12:9] + bit[8:0]/512 (Gr, Gb, R, B)
 *
 *  Digital Gain Range = x1.0 to x15.99
 *
 *************************************************/

u32 sensor_hi1339_cis_calc_dgain_code(u32 permile)
{
	u32 buf[2] = {0, 0};
	buf[0] = ((permile / 1000) & 0x0F) << 9;
	buf[1] = ((((permile % 1000) * 512) / 1000) & 0x1FF);

	return (buf[0] | buf[1]);
}

u32 sensor_hi1339_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x1E00) >> 9) * 1000) + ((code & 0x01FF) * 1000 / 512);
}

/* CIS OPS */
int sensor_hi1339_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u32 setfile_index = 0;
	cis_setting_info setinfo;
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
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_hi1339_check_rev is fail when cis init");
		ret = -EINVAL;
		goto p_err;
	}

	cis->cis_data->cur_width = SENSOR_HI1339_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_HI1339_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;

	sensor_hi1339_cis_data_calculation(sensor_hi1339_pllinfos[setfile_index], cis->cis_data);

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

int sensor_hi1339_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	u16 data16 = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] *******************************\n", __func__);
	ret = is_sensor_read16(client, SENSOR_HI1339_MODEL_ID_ADDR, &data16);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1339_MODEL_ID_ADDR, ret);
		goto p_i2c_err;
	}

	info("model_id(%x)\n", data16);
	ret = is_sensor_read16(client, SENSOR_HI1339_STREAM_ONOFF_ADDR, &data16);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1339_STREAM_ONOFF_ADDR, ret);
		goto p_i2c_err;
	}

	info("mode_select(streaming: %x)\n", data16);

	sensor_cis_dump_registers(subdev, sensor_hi1339_setfiles[0],
			sensor_hi1339_setfile_sizes[0]);


p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	info("[%s] *******************************\n", __func__);

	return ret;
}

static int sensor_hi1339_cis_group_param_hold_func(struct v4l2_subdev *subdev, bool hold)
{
#if USE_GROUP_PARAM_HOLD
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u16 hold_set = 0;

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

	if (hold == (bool)cis->cis_data->group_param_hold) {
		pr_debug("already group_param_hold (%d)\n", cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = is_sensor_read16(client, SENSOR_HI1339_GROUP_PARAM_HOLD_ADDR, &hold_set);
	if (ret < 0) {
		err("[%s: is_sensor_read] fail!!", __func__);
		goto p_err;
	}

	// set hold setting, bit[8]: Grouped parameter hold
	if (hold == true) {
		hold_set |= (0x1) << 8;
	} else {
		hold_set &= ~((0x1) << 8);
	}
	dbg_sensor(1, "[%s] GPH: %s \n", __func__, (hold_set & (0x1 << 8)) == (0x1 << 8) ? "ON": "OFF");

	ret = is_sensor_write16(client, SENSOR_HI1339_GROUP_PARAM_HOLD_ADDR, hold_set);
	if (ret < 0) {
		err("[%s: is_sensor_write] fail!!", __func__);
		goto p_err;
	}

	cis->cis_data->group_param_hold = (u32)hold;
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
int sensor_hi1339_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	u32 mode = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (cis->cis_data->stream_on == false && hold == true) {
		ret = 0;
		dbg_sensor(1,"%s : sensor stream off skip group_param_hold", __func__);
		goto p_err;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (mode == SENSOR_HI1339_1000X752_120FPS) {
		ret = 0;
		dbg_sensor(1,"%s : fast ae skip group_param_hold", __func__);
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_hi1339_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err_unlock;

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}
#endif

int sensor_hi1339_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* setfile global setting is at camera entrance */
	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_hi1339_global, sensor_hi1339_global_size);
	if (ret < 0) {
		err("sensor_hi1339_set_registers fail!!");
		goto p_err_unlock;
	}

	info("[%s] global setting done\n", __func__);

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

int sensor_hi1339_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (mode > sensor_hi1339_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	info("[%s] sensor mode(%d)\n", __func__, mode);
	ret = sensor_cis_set_registers(subdev, sensor_hi1339_setfiles[mode], sensor_hi1339_setfile_sizes[mode]);
	if (ret < 0) {
		err("[%s] sensor_hi1339_set_registers fail!!", __func__);
		goto p_err_unlock;
	}

	// Can change position later
	/*info("[%s]fsync normal mode\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_hi1339_setfile_fsync_info[HI1339_FSYNC_NORMAL].file,
			sensor_hi1339_setfile_fsync_info[HI1339_FSYNC_NORMAL].file_size);
	if (ret < 0) {
		err("[%s] fsync normal fail\n", __func__);
		goto p_err_unlock;
	}*/

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
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

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:

	return ret;
}

#if 0
int sensor_hi1339_cis_enable_frame_cnt(struct i2c_client *client, bool enable)
{
	int ret = -1;
	u8 frame_cnt_ctrl = 0;

	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_read8(client, SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR,
			&frame_cnt_ctrl);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR, ret);
		goto p_err;
	}

	if (enable == true) {
		frame_cnt_ctrl |= (0x1 << 2);  //enable frame count
		frame_cnt_ctrl &= ~(0x1 << 0); //frame count reset off
	} else {
		frame_cnt_ctrl &= ~(0x1 << 2); //disable frame count
		frame_cnt_ctrl |= (0x1 << 0);  //frame count reset on
	}

	ret = is_sensor_write8(client, SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR,
			frame_cnt_ctrl);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1339_MIPI_TX_OP_MODE_ADDR, ret);
	}

p_err:
	return ret;
}
#endif

int sensor_hi1339_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	FIMC_BUG(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);
	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/* Sensor stream on */
	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI1339_STREAM_ONOFF_ADDR, 0x0100);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI1339_STREAM_ONOFF_ADDR, ret);
		goto p_err;
	}

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1339_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
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

	ret = sensor_hi1339_cis_group_param_hold_func(subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	info("%s\n", __func__);
	ret = is_sensor_write16(client, SENSOR_HI1339_STREAM_ONOFF_ADDR, 0x0000);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", SENSOR_HI1339_STREAM_ONOFF_ADDR, 0x0000, ret);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1339_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	BUG_ON(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	do {
		ret = is_sensor_read16(client, SENSOR_HI1339_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI1339_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x \n", __func__, PLL_en);
		/* pll_bypass */
		if (!(PLL_en & 0x0100))
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);
	
	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occured after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi1339_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	BUG_ON(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	if (unlikely(!cis_data)) {
		err("cis_data is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	do {
		ret = is_sensor_read16(client, SENSOR_HI1339_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI1339_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_enable */
		if (PLL_en & 0x0100)
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occured after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi1339_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 coarse_integ_time = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;
	u32 max_frame_us_time = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	coarse_integ_time = (u32)(((pix_rate_freq_khz * input_exposure_time) / line_length_pck) / 1000);
	frame_length_lines = coarse_integ_time + cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)(frame_length_lines * line_length_pck) * 1000) / pix_rate_freq_khz);
	max_frame_us_time = 1000000/cis->min_fps;

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d), max_frame_us_time(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time, max_frame_us_time);

	dbg_sensor(1, "[%s] requested min_fps(%d), max_fps(%d) from HAL\n", __func__, cis->min_fps, cis->max_fps);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	if(cis->min_fps == cis->max_fps) {
		*target_duration = MIN(frame_duration, max_frame_us_time);
	}

	dbg_sensor(1, "[%s] calcurated frame_duration(%d), adjusted frame_duration(%d)\n", __func__, frame_duration, *target_duration);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi1339_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
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
		dbg_sensor(1, "frame duration(%d) is less than min(%d)\n", frame_duration, cis_data->min_frame_us_time);
		frame_duration = cis_data->min_frame_us_time;
	}

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((pix_rate_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, pix_rate_freq_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
			cis->id, __func__, pix_rate_freq_khz, frame_duration,
			line_length_pck, frame_length_lines);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_write16(client, SENSOR_HI1339_FRAME_LENGTH_LINE_ADDR, frame_length_lines);
	if (ret < 0) {
		goto p_err_unlock;
	}

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	if (min_fps > cis_data->max_fps) {
		err("[MOD:D:%d] %s, request FPS is too high(%d), set to max_fps(%d)\n",
			cis->id, __func__, min_fps, cis_data->max_fps);
		min_fps = cis_data->max_fps;
	}

	if (min_fps == 0) {
		err("[MOD:D:%d] %s, request FPS is 0, set to min FPS(1)\n", cis->id, __func__);
		min_fps = 1;
	}

	frame_duration = (1 * 1000 * 1000) / min_fps;

	dbg_sensor(1, "[MOD:D:%d] %s, set FPS(%d), frame duration(%d)\n",
			cis->id, __func__, min_fps, frame_duration);

	ret = sensor_hi1339_cis_set_frame_duration(subdev, frame_duration);
	if (ret < 0) {
		err("[MOD:D:%d] %s, set frame duration is fail(%d)\n",
			cis->id, __func__, ret);
		goto p_err;
	}

	cis_data->min_frame_us_time = frame_duration;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1339_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u64 pix_rate_freq_khz = 0;
	u16 coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
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
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	coarse_int = (u16)(((target_exposure->val * pix_rate_freq_khz) - min_fine_int) / (line_length_pck * 1000));

	if (coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->max_coarse_integration_time);
		coarse_int = cis_data->max_coarse_integration_time;
	}

	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_exposure_coarse = coarse_int;
	cis_data->cur_long_exposure_coarse = coarse_int;
	cis_data->cur_short_exposure_coarse = coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_write16(client, SENSOR_HI1339_COARSE_INTEG_TIME_ADDR, coarse_int);
		if (ret < 0)
			goto p_err_unlock;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d): pix_rate_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), frame_length_lines(%#x)\n", cis->id, __func__,
		cis_data->sen_vsync_count, pix_rate_freq_khz, line_length_pck, cis_data->frame_length_lines);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d): coarse_integration_time (%#x)\n",
		cis->id, __func__, cis_data->sen_vsync_count, coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	cis_shared_data *cis_data = NULL;
	u32 min_integration_time = 0;
	u32 min_coarse = 0;
	u32 min_fine = 0;
	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	if (pix_rate_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid pix_rate_freq_khz(%d)\n", cis->id, __func__, pix_rate_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	min_coarse = cis_data->min_coarse_integration_time;
	min_fine = cis_data->min_fine_integration_time;

	min_integration_time = (u32)(((line_length_pck * min_coarse) + min_fine) * 1000 / pix_rate_freq_khz);
	*min_expo = min_integration_time;

	dbg_sensor(1, "[%s] min integration time %d\n", __func__, min_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1339_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u32 max_integration_time = 0;
	u32 max_coarse_margin = 0;
	u32 max_coarse = 0;
	u32 max_fine = 0;
	u64 pix_rate_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	if (pix_rate_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid pix_rate_freq_khz(%d)\n", cis->id, __func__, pix_rate_freq_khz);
		goto p_err;
	}
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = cis_data->frame_length_lines;
	max_coarse_margin = cis_data->max_margin_coarse_integration_time;
	max_coarse = frame_length_lines - max_coarse_margin;
	max_fine = cis_data->max_fine_integration_time;

	max_integration_time = (u32)(((line_length_pck * max_coarse) + max_fine) * 1000 / pix_rate_freq_khz);

	*max_expo = max_integration_time;

	/* TODO: Is this values update hear? */
	cis_data->max_coarse_integration_time = max_coarse;

	dbg_sensor(1, "[%s] max integration time %d, max coarse integration %d\n",
			__func__, max_integration_time, cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_hi1339_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;

	u32 again_code = 0;
	u32 again_permile = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_hi1339_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_hi1339_cis_calc_again_permile(again_code);

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

int sensor_hi1339_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

	u8 analog_gain = 0;
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

	analog_gain = (u8)sensor_hi1339_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	analog_gain &= 0xFF;

	// Analog gain
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_write8(client, SENSOR_HI1339_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

	u8 analog_gain = 0;
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
	ret = is_sensor_read8(client, SENSOR_HI1339_ANALOG_GAIN_ADDR, &analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	*again = sensor_hi1339_cis_calc_again_permile((u32)analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 min_again_code = SENSOR_HI1339_MIN_ANALOG_GAIN_SET_VALUE;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_analog_gain[0] = min_again_code;
	cis_data->min_analog_gain[1] = sensor_hi1339_cis_calc_again_permile(min_again_code);
	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] min_again_code %d, main_again_permile %d\n", __func__,
		cis_data->min_analog_gain[0], cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi1339_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 max_again_code = SENSOR_HI1339_MAX_ANALOG_GAIN_SET_VALUE;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_analog_gain[0] = max_again_code;
	cis_data->max_analog_gain[1] = sensor_hi1339_cis_calc_again_permile(max_again_code);
	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] max_again_code %d, max_again_permile %d\n", __func__,
		cis_data->max_analog_gain[0], cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi1339_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
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

	dgain_code = (u16)sensor_hi1339_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_analog_gain\n", __func__);
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_analog_gain\n", __func__);
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d, dgain_code(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	I2C_MUTEX_LOCK(cis->i2c_lock);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = is_sensor_write16_array(client, SENSOR_HI1339_DIG_GAIN_ADDR, dgains, 4);
	if (ret < 0) {
		goto p_err_unlock;
	}

	ret = is_sensor_read16(client, SENSOR_HI1339_ISP_ENABLE_ADDR, &read_val);
	if (ret < 0) {
		goto p_err_unlock;
	}

	enable_dgain = read_val | (0x1 << 4); // [4]: D gain enable
	ret = is_sensor_write16(client, SENSOR_HI1339_ISP_ENABLE_ADDR, enable_dgain);
	if (ret < 0) {
		goto p_err_unlock;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	struct i2c_client *client = NULL;

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
	ret = is_sensor_read16(client, SENSOR_HI1339_DIG_GAIN_ADDR, &digital_gain);
	if (ret < 0)
		goto p_err_unlock;

	*dgain = sensor_hi1339_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, dgain_permile = %d, dgain_code(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_hi1339_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 min_dgain_code = SENSOR_HI1339_MIN_DIGITAL_GAIN_SET_VALUE;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!min_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->min_digital_gain[0] = min_dgain_code;
	cis_data->min_digital_gain[1] = sensor_hi1339_cis_calc_dgain_permile(min_dgain_code);

	*min_dgain = cis_data->min_digital_gain[1];

	dbg_sensor(1, "[%s] min_dgain_code %d, min_dgain_permile %d\n", __func__,
		cis_data->min_digital_gain[0], cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_hi1339_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	u16 max_dgain_code = SENSOR_HI1339_MAX_DIGITAL_GAIN_SET_VALUE;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	cis_data = cis->cis_data;

	cis_data->max_digital_gain[0] = max_dgain_code;
	cis_data->max_digital_gain[1] = sensor_hi1339_cis_calc_dgain_permile(max_dgain_code);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] max_dgain_code %d, max_dgain_permile %d\n", __func__,
		cis_data->max_digital_gain[0], cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
	return ret;
}


int sensor_hi1339_cis_set_analog_digital_gain(struct v4l2_subdev *subdev, struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis   = NULL;
	cis_shared_data *cis_data = NULL;
	struct i2c_client *client = NULL;
	u32 cal_analog_val1 = 0;
	u32 cal_analog_val2 = 0;
	u32 cal_digital = 0;
	u8 analog_gain = 0;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	cis_data = cis->cis_data;
	client = cis->client;

	analog_gain = (u8)sensor_hi1339_cis_calc_again_code(again->val);

	if (analog_gain < cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "%s [%d] VCNT[%d] input_again[%d] analog_gain[%d]\n",
		__func__, cis->id, cis_data->sen_vsync_count, again->val, analog_gain);

	analog_gain &= 0xFF;

	/* Set Analog gains */
	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = is_sensor_write8(client, SENSOR_HI1339_ANALOG_GAIN_ADDR, analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	/* Set Digital gains */
	if (analog_gain == cis_data->max_analog_gain[0]) {
		dgain_code = (u16)sensor_hi1339_cis_calc_dgain_code(dgain->val);
		if (dgain_code < cis_data->min_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
			dgain_code = cis_data->min_digital_gain[0];
		}
		if (dgain_code > cis_data->max_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
			dgain_code = cis_data->max_digital_gain[0];
		}
	} else {
		cal_analog_val1 = sensor_hi1339_cis_calc_again_code(again->val);
		cal_analog_val2 = sensor_hi1339_cis_calc_again_permile(cal_analog_val1);
		cal_digital = (again->val * 1000) / cal_analog_val2;
		dgain_code = (u16)sensor_hi1339_cis_calc_dgain_code(cal_digital);
		if (cal_digital < 0) {
			err("calculate dgain is fail");
			goto p_err_unlock;
		}
		dbg_sensor(1, "[%s] dgain compensation : input_again = %d, calculated_analog_gain = %d\n",
			__func__, again->val, cal_analog_val2);
	}

	dbg_sensor(1, "[%s] cal_digital = %d, dgain_code = %d(%#x)\n", __func__, cal_digital, dgain_code, dgain_code);
	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = is_sensor_write16_array(client, SENSOR_HI1339_DIG_GAIN_ADDR, dgains, 4);
	if (ret < 0)
		goto p_err_unlock;

	ret = is_sensor_read16(client, SENSOR_HI1339_ISP_ENABLE_ADDR, &read_val);
	if (ret < 0)
		goto p_err_unlock;
	enable_dgain = read_val | (0x1 << 4); /* [4]: D gain enable */
	ret = is_sensor_write16(client, SENSOR_HI1339_ISP_ENABLE_ADDR, enable_dgain);
	if (ret < 0)
		goto p_err_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

int sensor_hi1339_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
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

int sensor_hi1339_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure, 
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	
	/* Set Exposure Time */
	ret = sensor_hi1339_cis_set_exposure_time(subdev, target_exposure);

	/* Set Analog & Digital gains */
	ret |= sensor_hi1339_cis_set_analog_digital_gain(subdev, again, dgain);
	return ret;
}

void sensor_hi1339_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	FIMC_BUG_VOID(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG_VOID(!cis);
	FIMC_BUG_VOID(!cis->cis_data);

	if (mode > sensor_hi1339_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	sensor_hi1339_cis_data_calculation(sensor_hi1339_pllinfos[mode], cis->cis_data);
}

static struct is_cis_ops cis_ops_hi1339 = {
	.cis_init = sensor_hi1339_cis_init,
	.cis_log_status = sensor_hi1339_cis_log_status,
#if USE_GROUP_PARAM_HOLD
	.cis_group_param_hold = sensor_hi1339_cis_group_param_hold,
#endif
	.cis_set_global_setting = sensor_hi1339_cis_set_global_setting,
	.cis_set_size = sensor_hi1339_cis_set_size,
	.cis_mode_change = sensor_hi1339_cis_mode_change,
	.cis_stream_on = sensor_hi1339_cis_stream_on,
	.cis_stream_off = sensor_hi1339_cis_stream_off,
	.cis_wait_streamoff = sensor_hi1339_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hi1339_cis_wait_streamon,
	.cis_adjust_frame_duration = sensor_hi1339_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_hi1339_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_hi1339_cis_set_frame_rate,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_hi1339_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_hi1339_cis_get_max_exposure_time,
	.cis_adjust_analog_gain = sensor_hi1339_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_hi1339_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_hi1339_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_hi1339_cis_get_max_analog_gain,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_hi1339_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_hi1339_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_hi1339_cis_get_max_digital_gain,
	.cis_set_totalgain = sensor_hi1339_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_hi1339_cis_compensate_gain_for_extremely_br,
	.cis_data_calculation = sensor_hi1339_cis_data_calc,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
};

int cis_hi1339_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
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
	cis->cis_ops = &cis_ops_hi1339;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;

	/* Use total gain instead of using dgain */
	cis->use_dgain = false;
	cis->use_vendor_total_gain = true;

	cis->use_wb_gain = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_hi1339_global = sensor_hi1339_setfile_A_Global;
		sensor_hi1339_global_size = ARRAY_SIZE(sensor_hi1339_setfile_A_Global);
		sensor_hi1339_setfiles = sensor_hi1339_setfiles_A;
		sensor_hi1339_setfile_sizes = sensor_hi1339_setfile_A_sizes;
		sensor_hi1339_pllinfos = sensor_hi1339_pllinfos_A;
		sensor_hi1339_max_setfile_num = ARRAY_SIZE(sensor_hi1339_setfiles_A);
		//sensor_hi1339_setfile_fsync_info = sensor_hi1339_setfile_A_fsync_info;

		cis->mipi_sensor_mode = sensor_hi1339_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_hi1339_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_hi1339_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_hi1339_setfile_A_verify_sensor_mode);
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("%s setfile_B\n", __func__);
		sensor_hi1339_global = sensor_hi1339_setfile_B_Global;
		sensor_hi1339_global_size = ARRAY_SIZE(sensor_hi1339_setfile_B_Global);
		sensor_hi1339_setfiles = sensor_hi1339_setfiles_B;
		sensor_hi1339_setfile_sizes = sensor_hi1339_setfile_B_sizes;
		sensor_hi1339_pllinfos = sensor_hi1339_pllinfos_B;
		sensor_hi1339_max_setfile_num = ARRAY_SIZE(sensor_hi1339_setfiles_B);
		//sensor_hi1339_setfile_fsync_info = sensor_hi1339_setfile_B_fsync_info;
	} else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hi1339_global = sensor_hi1339_setfile_A_Global;
		sensor_hi1339_global_size = ARRAY_SIZE(sensor_hi1339_setfile_A_Global);
		sensor_hi1339_setfiles = sensor_hi1339_setfiles_A;
		sensor_hi1339_setfile_sizes = sensor_hi1339_setfile_A_sizes;
		sensor_hi1339_pllinfos = sensor_hi1339_pllinfos_A;
		sensor_hi1339_max_setfile_num = ARRAY_SIZE(sensor_hi1339_setfiles_A);
		//sensor_hi1339_setfile_fsync_info = sensor_hi1339_setfile_A_fsync_info;

		cis->mipi_sensor_mode = sensor_hi1339_setfile_A_mipi_sensor_mode;
		cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_hi1339_setfile_A_mipi_sensor_mode);
		verify_sensor_mode = sensor_hi1339_setfile_A_verify_sensor_mode;
		verify_sensor_mode_size = ARRAY_SIZE(sensor_hi1339_setfile_A_verify_sensor_mode);
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
	return ret;
}

static const struct of_device_id sensor_cis_hi1339_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hi1339",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hi1339_match);

static const struct i2c_device_id sensor_cis_hi1339_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hi1339_driver = {
	.probe	= cis_hi1339_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hi1339_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hi1339_idt
};

static int __init sensor_cis_hi1339_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hi1339_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hi1339_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hi1339_init);

MODULE_LICENSE("GPL");
