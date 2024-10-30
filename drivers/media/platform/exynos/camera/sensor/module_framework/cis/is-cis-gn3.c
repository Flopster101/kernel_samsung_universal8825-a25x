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
#include "is-cis-gn3.h"
#ifndef CONFIG_CAMERA_VENDER_MCD
#include "is-cis-gn3-setA.h"
#endif
#include "is-cis-gn3-setA-19p2.h"
#include "is-helper-i2c.h"
#include "is-sec-define.h"

#define SENSOR_NAME "S5KGN3"
/* #define DEBUG_GN3_PLL */
/* #define DEBUG_CAL_WRITE */
static const u32 *sensor_gn3_global;
static u32 sensor_gn3_global_size;
#if IS_ENABLED(CIS_CALIBRATION)
static const u32 *sensor_gn3_goldenCal;
static u32 sensor_gn3_goldenCal_size;
#endif
static const u32 **sensor_gn3_setfiles;
static const u32 *sensor_gn3_setfile_sizes;
static const struct sensor_pll_info_compact **sensor_gn3_pllinfos;
static u32 sensor_gn3_max_setfile_num;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
static const u32 *sensor_gn3_global_retention;
static u32 sensor_gn3_global_retention_size;
static const u32 **sensor_gn3_retention;
static const u32 *sensor_gn3_retention_size;
static u32 sensor_gn3_max_retention_num;
static const u32 **sensor_gn3_load_sram;
static const u32 *sensor_gn3_load_sram_size;
static int sensor_gn3_seamless_delay_count;
#endif

//#define GN3_SEAMLESS_TEST
#ifdef GN3_SEAMLESS_TEST
static int test_cnt;
#endif

/* For Recovery */
static u32 sensor_gn3_frame_duration_backup;
static struct ae_param sensor_gn3_again_backup;
static struct ae_param sensor_gn3_dgain_backup;
static struct ae_param sensor_gn3_target_exp_backup;

static bool sensor_gn3_eeprom_cal_available;
static bool sensor_gn3_first_entrance;
static bool sensor_gn3_load_retention;
static bool sensor_gn3_need_stream_on_retention;

#if IS_ENABLED(USE_SELECT_GN3_DUAL_SYNC_PIN)
static bool sensor_gn3_select_gpio1;
#endif

int sensor_gn3_cis_set_global_setting(struct v4l2_subdev *subdev);
int sensor_gn3_cis_wait_streamon(struct v4l2_subdev *subdev);
int sensor_gn3_cis_wait_streamoff(struct v4l2_subdev *subdev);
int sensor_gn3_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again);

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
void sensor_gn3_cis_set_mode_group(u32 mode)
{
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT] = mode;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_GN3_4080X3060_30FPS_CROP_R10 :
	case SENSOR_GN3_4080X3060_60FPS_R10 :
	case SENSOR_GN3_4080X3060_60FPS_LN2_R10 :
	case SENSOR_GN3_4080X3060_60FPS_LN4_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X3060_60FPS_LN2_R10;
		break;
	case SENSOR_GN3_4080X3060_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X3060_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X3060_60FPS_R12 :
	case SENSOR_GN3_4080X3060_30FPS_CROP_R12 :
	case SENSOR_GN3_4080X3060_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X3060_60FPS_LN2_R12;
		break;
	case SENSOR_GN3_4080X2720_60FPS_R10 :
	case SENSOR_GN3_4080X2720_60FPS_LN2_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2720_60FPS_LN2_R10;
		break;
	case SENSOR_GN3_4080X2720_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X2720_60FPS_R12 :
	case SENSOR_GN3_4080X2720_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2720_60FPS_LN2_R12;
		break;
	case SENSOR_GN3_4080X2296_60FPS_LN2_R10 :
	case SENSOR_GN3_4080X2296_60FPS_LN4_R10 :
	case SENSOR_GN3_4080X2296_60FPS_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2296_60FPS_LN2_R10;
		break;
	case SENSOR_GN3_4080X2296_60FPS_IDCG_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X2296_60FPS_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = SENSOR_GN3_4080X2296_60FPS_LN2_R12;
		break;
	default:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_GN3_4080X3060_30FPS_CROP_R10 :
	case SENSOR_GN3_4080X3060_60FPS_R10 :
	case SENSOR_GN3_4080X3060_60FPS_LN2_R10 :
	case SENSOR_GN3_4080X3060_60FPS_LN4_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X3060_60FPS_LN4_R10;
		break;
	case SENSOR_GN3_4080X3060_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X3060_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X3060_60FPS_R12 :
	case SENSOR_GN3_4080X3060_30FPS_CROP_R12 :
	case SENSOR_GN3_4080X3060_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X3060_60FPS_LN4_R12;
		break;
	case SENSOR_GN3_4080X2296_60FPS_LN2_R10 :
	case SENSOR_GN3_4080X2296_60FPS_LN4_R10 :
	case SENSOR_GN3_4080X2296_60FPS_R10 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X2296_60FPS_LN4_R10;
		break;
	case SENSOR_GN3_4080X2296_60FPS_IDCG_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X2296_60FPS_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = SENSOR_GN3_4080X2296_60FPS_LN4_R12;
		break;
	default:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_GN3_4080X3060_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X3060_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X3060_60FPS_R12 :
	case SENSOR_GN3_4080X3060_30FPS_CROP_R12 :
	case SENSOR_GN3_4080X3060_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X3060_60FPS_IDCG_R12;
		break;
	case SENSOR_GN3_4080X2720_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X2720_60FPS_R12 :
	case SENSOR_GN3_4080X2720_60FPS_IDCG_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X2720_60FPS_IDCG_R12;
		break;
	case SENSOR_GN3_4080X2296_60FPS_IDCG_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN4_R12 :
	case SENSOR_GN3_4080X2296_60FPS_LN2_R12 :
	case SENSOR_GN3_4080X2296_60FPS_R12 :
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = SENSOR_GN3_4080X2296_60FPS_IDCG_R12;
		break;
	default:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	}

	switch (mode) {
	case SENSOR_GN3_4080X3060_60FPS_R12:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS] = SENSOR_GN3_4080X3060_30FPS_CROP_R12;
		break;
	case SENSOR_GN3_4080X3060_60FPS_R10:
		sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS] = SENSOR_GN3_4080X3060_30FPS_CROP_R10;
		break;
	}

	info("[%s] default(%d) (%d) (%d) (%d) (%d)\n", __func__, sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT],
															sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG],
															sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2],
															sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4],
															sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS]);
}
#endif

#ifdef USE_CAMERA_EMBEDDED_HEADER
#define SENSOR_GN3_VALID_TAG 0x5A
#define SENSOR_GN3_FRAME_ID_PAGE 0
#define SENSOR_GN3_FRAME_ID_OFFSET 26

static u32 frame_id_idx = SENSOR_GN3_FRAME_ID_PAGE + SENSOR_GN3_FRAME_ID_OFFSET;

static int sensor_gn3_cis_get_frame_id(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (embedded_buf[frame_id_idx-1] == SENSOR_GN3_VALID_TAG) {
		*frame_id = embedded_buf[frame_id_idx];
		dbg_sensor(1, "frame_id(0x%x)", *frame_id);
	} else {
		err("invalid valid tag(0x%x)", embedded_buf[frame_id_idx-1]);
		*frame_id = 1;
	}

	cis->cis_data->sen_frame_id = *frame_id;

	return ret;
}
#endif

static void sensor_gn3_set_integration_min(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (mode < 0 || mode >= SENSOR_GN3_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	cis_data->min_coarse_integration_time = sensor_gn3_common_mode_attr[mode].min_coarse_integration_time;

	dbg_sensor(1, "min_coarse_integration_time(%d)\n", cis_data->min_coarse_integration_time);
}

static void sensor_gn3_set_integration_max_margin(u32 mode, cis_shared_data *cis_data)
{
	WARN_ON(!cis_data);

	if (mode < 0 || mode >= SENSOR_GN3_MODE_MAX) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	cis_data->max_margin_coarse_integration_time
		= sensor_gn3_common_mode_attr[mode].max_margin_coarse_integration_time;

	dbg_sensor(1, "max_margin_coarse_integration_time(%d)\n", cis_data->max_margin_coarse_integration_time);
}

static void sensor_gn3_cis_data_calculation(const struct sensor_pll_info_compact *pll_info_compact,
						cis_shared_data *cis_data)
{
	u64 vt_pix_clk_hz = 0;
	u32 frame_rate = 0, max_fps = 0, frame_valid_us = 0;
	u32 frame_length_lines = pll_info_compact->frame_length_lines;
	u32 mode = cis_data->sens_config_index_cur;

	WARN_ON(!pll_info_compact);

	/* 1. get pclk value from pll info */
	vt_pix_clk_hz = pll_info_compact->pclk;

	dbg_sensor(1, "ext_clock(%d), mipi_datarate(%ld), pclk(%ld)\n",
			pll_info_compact->ext_clk, pll_info_compact->mipi_datarate, pll_info_compact->pclk);

	/* 2. the time of processing one frame calculation (us) */
	cis_data->min_frame_us_time = (((u64)frame_length_lines) * pll_info_compact->line_length_pck * 1000
					/ (vt_pix_clk_hz / 1000));
	cis_data->cur_frame_us_time = cis_data->min_frame_us_time;
#ifdef CAMERA_REAR2
	cis_data->min_sync_frame_us_time = cis_data->min_frame_us_time;
#endif
	/* 3. FPS calculation */
	frame_rate = vt_pix_clk_hz / (frame_length_lines * pll_info_compact->line_length_pck);
	dbg_sensor(1, "frame_rate (%d) = vt_pix_clk_hz(%lld) / "
		KERN_CONT "(frame_length_lines(%d) * pll_info_compact->line_length_pck(%d))\n",
		frame_rate, vt_pix_clk_hz, frame_length_lines, pll_info_compact->line_length_pck);

	/* calculate max fps */
	max_fps = (vt_pix_clk_hz * 10) / (frame_length_lines * pll_info_compact->line_length_pck);
	max_fps = (max_fps % 10 >= 5 ? frame_rate + 1 : frame_rate);

	cis_data->pclk = vt_pix_clk_hz;
	cis_data->max_fps = max_fps;
	cis_data->frame_length_lines = frame_length_lines;
	cis_data->line_length_pck = pll_info_compact->line_length_pck;
	cis_data->line_readOut_time = (u64)cis_data->line_length_pck * 1000
					* 1000 * 1000 / cis_data->pclk;

	/* Frame valid time calcuration */
	frame_valid_us = (u64)cis_data->cur_height * cis_data->line_length_pck
				* 1000 * 1000 / cis_data->pclk;
	cis_data->frame_valid_us_time = (unsigned int)frame_valid_us;

	dbg_sensor(1, "%s\n", __func__);
	dbg_sensor(1, "Sensor size(%d x %d) setting: SUCCESS!\n",
					cis_data->cur_width, cis_data->cur_height);
	dbg_sensor(1, "Frame Valid(us): %d\n", frame_valid_us);

	dbg_sensor(1, "Fps: %d, max fps(%d)\n", frame_rate, cis_data->max_fps);
	dbg_sensor(1, "min_frame_time(%d us)\n", cis_data->min_frame_us_time);
	dbg_sensor(1, "Pixel rate(Kbps): %d\n", cis_data->pclk / 1000);

	/* Frame period calculation */
	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis_data->rolling_shutter_skew = (cis_data->cur_height - 1) * cis_data->line_readOut_time;

	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n", __func__,
		cis_data->frame_time, cis_data->rolling_shutter_skew);

	/* Constant values */
	cis_data->min_fine_integration_time = SENSOR_GN3_FINE_INTEGRATION_TIME_MIN;
	cis_data->max_fine_integration_time = SENSOR_GN3_FINE_INTEGRATION_TIME_MAX;

	sensor_gn3_set_integration_min(mode, cis_data);
	sensor_gn3_set_integration_max_margin(mode, cis_data);
}

void sensor_gn3_cis_data_calc(struct v4l2_subdev *subdev, u32 mode)
{
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= sensor_gn3_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		return;
	}

	if (cis->cis_data->stream_on) {
		info("[%s] call mode change in stream on state\n", __func__);
		sensor_gn3_cis_wait_streamon(subdev);
		sensor_gn3_cis_stream_off(subdev);
		sensor_gn3_cis_wait_streamoff(subdev);
		info("[%s] stream off done\n", __func__);
	}

	sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[mode], cis->cis_data);
}

static int sensor_gn3_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	WARN_ON(!cis_data);

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

int sensor_gn3_cis_select_setfile(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 rev = 0;
	u8 feature_id = 0;
	struct is_cis *cis = NULL;
#if IS_ENABLED(USE_SELECT_GN3_DUAL_SYNC_PIN)
	struct is_rom_info *finfo = NULL;
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	rev = cis->cis_data->cis_rev;
	info("gn3 sensor revision(%#x)\n", rev);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_read8(cis->client, 0x000D, &feature_id);
	info("gn3 sensor revision 0x000D(%#x)\n", feature_id);
	ret |= is_sensor_read8(cis->client, 0x000E, &feature_id);
	info("gn3 sensor revision 0x000E(%#x)\n", feature_id);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#if IS_ENABLED(USE_SELECT_GN3_DUAL_SYNC_PIN)
	sensor_gn3_select_gpio1 = false;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
		err("eeprom read fail status, default gpio2");
	} else {
		if ((finfo->header_ver[FW_VERSION_INFO] == 'B' && finfo->header_ver[FW_PUB_NUM + 1] < '3')
			||(finfo->header_ver[FW_VERSION_INFO] == 'A' && finfo->header_ver[FW_PUB_NUM + 1] >= '4')) {
			sensor_gn3_select_gpio1 = true;
		} else if (finfo->header_ver[FW_VERSION_INFO] == 'A' && finfo->header_ver[FW_PUB_NUM + 1] < '4') {
			err("Not support sync pin, default gpio2");
		}
	}

	info("[%s] dual sync pin - gpio%d", __func__, (sensor_gn3_select_gpio1 ? 1 : 2));
#endif

	return ret;
}

/* CIS OPS */
int sensor_gn3_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	u32 setfile_index = 0;
	cis_setting_info setinfo;

	setinfo.param = NULL;
	setinfo.return_value = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	WARN_ON(!cis->cis_data);
#if !defined(CONFIG_CAMERA_VENDER_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_gn3_check_rev is fail when cis init");
		ret = -EINVAL;
		goto p_err;
	}
#endif

	sensor_gn3_first_entrance = true;
	cis->cis_data->sens_config_index_pre = SENSOR_GN3_MODE_MAX;

	sensor_gn3_cis_select_setfile(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = SENSOR_GN3_MAX_WIDTH;
	cis->cis_data->cur_height = SENSOR_GN3_MAX_HEIGHT;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->highres_capture_mode = false;
	cis->cis_data->pre_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	sensor_gn3_load_retention = false;

#ifdef GN3_SEAMLESS_TEST
	test_cnt = 0;
#endif

	sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] = MODE_GROUP_NONE;
	sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS] = MODE_GROUP_NONE;

	sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[setfile_index], cis->cis_data);

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

	/* CALL_CISOPS(cis, cis_log_status, subdev); */

p_err:
	return ret;
}

int sensor_gn3_cis_deinit(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (sensor_gn3_load_retention == false) {
		pr_info("%s: need to load retention\n", __func__);
		sensor_gn3_cis_stream_on(subdev);
		sensor_gn3_cis_wait_streamon(subdev);
		sensor_gn3_cis_stream_off(subdev);
		sensor_gn3_cis_wait_streamoff(subdev);
		pr_info("%s: complete to load retention\n", __func__);
	}

	/* retention mode CRC wait calculation */
	usleep_range(10000, 10000);
#endif

	return ret;
}

static const struct is_cis_log log_gn3[] = {
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0342, 0, "0x0342"},
	{I2C_READ, 16, 0x0344, 0, "0x0344"},
	{I2C_READ, 16, 0x0346, 0, "0x0346"},
	{I2C_READ, 16, 0x0348, 0, "0x0348"},
	{I2C_READ, 16, 0x034A, 0, "0x034A"},
	{I2C_READ, 16, 0x034C, 0, "0x034C"},
	{I2C_READ, 16, 0x034E, 0, "0x034E"},
	{I2C_READ, 16, 0x0350, 0, "0x0350"},
	{I2C_READ, 16, 0x0352, 0, "0x0352"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	/* 2001 page */
	{I2C_WRITE, 16, 0xFCFC, 0x2001, "0x2001 page"},
	{I2C_READ, 16, 0x43E8, 0, "0x43E8"},
	{I2C_READ, 16, 0x43EA, 0, "0x43EA"},
	{I2C_READ, 16, 0x43EE, 0, "0x43EE"},
	{I2C_READ, 16, 0x43F0, 0, "0x43F0"},
	{I2C_READ, 16, 0x43F2, 0, "0x43F2"},
	{I2C_READ, 16, 0x4D82, 0, "0x4D82"},
	{I2C_READ, 16, 0x4D84, 0, "0x4D84"},
	{I2C_READ, 16, 0x4D86, 0, "0x4D86"},
	{I2C_READ, 16, 0x4D88, 0, "0x4D88"},
	{I2C_READ, 16, 0x309C, 0, "0x309C"},
	{I2C_READ, 16, 0x309E, 0, "0x309E"},
	/* restore 4000 page */
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_gn3_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	WARN_ON(!subdev);

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

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	if (ext_info->use_retention_mode != SENSOR_RETENTION_UNSUPPORTED
		&& cis->cis_data->stream_on == false)
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	sensor_cis_log_status(cis, client, log_gn3,
			ARRAY_SIZE(log_gn3), (char *)__func__);

p_err:
	return ret;
}

#if USE_GROUP_PARAM_HOLD
static int sensor_gn3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (hold == cis->cis_data->group_param_hold) {
		dbg_sensor(1, "%s : already group_param_hold (%d)\n", __func__, cis->cis_data->group_param_hold);
		goto p_err;
	}

	ret = is_sensor_write8(client, 0x0104, hold); /* api_rw_general_setup_grouped_parameter_hold */
	dbg_sensor(1, "%s : hold = %d, ret = %d\n", __func__, hold, ret);
	if (ret < 0)
		goto p_err;

	cis->cis_data->group_param_hold = hold;
	ret = hold;
p_err:
	return ret;
}
#else
static inline int sensor_gn3_cis_group_param_hold_func(struct v4l2_subdev *subdev, unsigned int hold)
{ return 0; }
#endif

/* Input
 *	hold : true - hold, flase - no hold
 * Output
 *	  return: 0 - no effect(already hold or no hold)
 *		positive - setted by request
 *		negative - ERROR value
 */
int sensor_gn3_cis_group_param_hold(struct v4l2_subdev *subdev, bool hold)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret = sensor_gn3_cis_group_param_hold_func(subdev, hold);
	if (ret < 0)
		goto p_err;

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}


int sensor_gn3_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global setting internal start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret |= sensor_cis_set_registers(subdev, sensor_gn3_global, sensor_gn3_global_size);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global setting internal done\n", __func__);

	if (ext_info->use_retention_mode == SENSOR_RETENTION_UNSUPPORTED) {
#if IS_ENABLED(CIS_CALIBRATION)
		ret |= sensor_gn3_cis_set_cal(subdev);
		ret |= sensor_cis_set_registers(subdev, sensor_gn3_goldenCal, sensor_gn3_goldenCal_size);
		if (ret < 0) {
			err("sensor_gn3_cis_set_cal fail!!");
			goto p_err;
		}
#endif
	}

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
	return ret;
}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_set_global_setting_retention(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	info("[%s] global retention setting start\n", __func__);
	/* setfile global retention setting is at camera entrance */
	ret = sensor_cis_set_registers(subdev, sensor_gn3_global_retention, sensor_gn3_global_retention_size);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	info("[%s] global retention setting done\n", __func__);

p_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}

#ifdef CAMERA_REAR2
int sensor_gn3_cis_retention_crc_enable(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
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

	switch (mode) {
	default:
		/* Sensor stream on */
		is_sensor_write16(client, 0x0100, 0x0103);

		/* retention mode CRC check register enable */
		is_sensor_write8(client, 0x010E, 0x01); /* api_rw_general_setup_checksum_on_ram_enable */
		info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);

		/* Sensor stream off */
		is_sensor_write8(client, 0x0100, 0x00);
		break;
	}

p_err:
	return ret;
}
#endif
#endif

#if IS_ENABLED(GN3_BURST_WRITE)
int sensor_gn3_cis_write16_burst(struct i2c_client *client, u16 addr, u8 *val, u32 num, bool endian)
{
	int ret = 0;
	struct i2c_msg msg[1];
	int i = 0;
	u8 *wbuf;

	if (val == NULL) {
		pr_err("val array is null\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (!client->adapter) {
		pr_err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	wbuf = kmalloc((2 + (num * 2)), GFP_KERNEL);
	if (!wbuf) {
		pr_err("failed to alloc buffer for burst i2c\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2 + (num * 2);
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	if (endian == GN3_BIG_ENDIAN) {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2)] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2) + 1] & 0xFF);
			i2c_info("I2CW16(%d) [0x%04x] : 0x%02x%02x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	} else {
		for (i = 0; i < num; i++) {
			wbuf[(i * 2) + 2] = (val[(i * 2) + 1] & 0xFF);
			wbuf[(i * 2) + 3] = (val[(i * 2)] & 0xFF);
			i2c_info("I2CW16(%d) [0x%04x] : 0x%02x%02x\n", client->addr, addr, (val[(i * 2)] & 0xFF), (val[(i * 2) + 1] & 0xFF));
		}
	}

	ret = is_i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		pr_err("i2c transfer fail(%d)", ret);
		goto p_err_free;
	}

	kfree(wbuf);
	return 0;

p_err_free:
	kfree(wbuf);
p_err:
	return ret;
}
#endif

#if IS_ENABLED(CIS_CALIBRATION)
int sensor_gn3_cis_set_cal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_rom_info *finfo = NULL;
	char *cal_buf = NULL;
	struct is_cis *cis = NULL;
	bool endian = GN3_BIG_ENDIAN;
	int start_addr = 0, end_addr = 0;
#if IS_ENABLED(GN3_BURST_WRITE)
	int cal_size = 0;
#endif
	int i = 0;
	u16 val = 0;
	int len = 0;

	info("[%s] E", __func__);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	if (sensor_gn3_eeprom_cal_available)
		return 0;
#endif

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
		err("eeprom read fail status, skip set cal");
		sensor_gn3_eeprom_cal_available = false;
		return 0;
	}

	info("[%s] eeprom read, start set cal\n", __func__);
	is_sec_get_cal_buf(&cal_buf, ROM_ID_REAR);

#ifdef CAMERA_GN3_CAL_MODULE_VERSION
	if (finfo->header_ver[10] < CAMERA_GN3_CAL_MODULE_VERSION) {
		start_addr = finfo->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
		if (cal_buf[start_addr + 2] == 0xFF && cal_buf[start_addr + 3] == 0xFF &&
			cal_buf[start_addr + 4] == 0xFF && cal_buf[start_addr + 5] == 0xFF &&
			cal_buf[start_addr + 6] == 0xFF && cal_buf[start_addr + 7] == 0xFF &&
			cal_buf[start_addr + 8] == 0xFF && cal_buf[start_addr + 9] == 0xFF &&
			cal_buf[start_addr + 10] == 0xFF && cal_buf[start_addr + 11] == 0xFF &&
			cal_buf[start_addr + 12] == 0xFF && cal_buf[start_addr + 13] == 0xFF &&
			cal_buf[start_addr + 14] == 0xFF && cal_buf[start_addr + 15] == 0xFF &&
			cal_buf[start_addr + 16] == 0xFF && cal_buf[start_addr + 17] == 0xFF) {
			info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]",
				start_addr + 2, cal_buf[start_addr + 2], start_addr + 17, cal_buf[start_addr + 17]);
			info("[%s] empty Cal", __func__);
			return 0;
		}

		len = (finfo->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN) - 1;
		if (len >= 0) {
			end_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];
			if (end_addr >= 15) {
				if (cal_buf[end_addr	] == 0xFF && cal_buf[end_addr - 1] == 0xFF &&
					cal_buf[end_addr - 2] == 0xFF && cal_buf[end_addr - 3] == 0xFF &&
					cal_buf[end_addr - 4] == 0xFF && cal_buf[end_addr - 5] == 0xFF &&
					cal_buf[end_addr - 6] == 0xFF && cal_buf[end_addr - 7] == 0xFF &&
					cal_buf[end_addr - 8] == 0xFF && cal_buf[end_addr - 9] == 0xFF &&
					cal_buf[end_addr - 10] == 0xFF && cal_buf[end_addr - 11] == 0xFF &&
					cal_buf[end_addr - 12] == 0xFF && cal_buf[end_addr - 13] == 0xFF &&
					cal_buf[end_addr - 14] == 0xFF && cal_buf[end_addr - 15] == 0xFF) {
					info("empty Cal - cal offset[0x%04X] = val[0x%02X], cal offset[0x%04X] = val[0x%02X]",
						end_addr, cal_buf[end_addr], end_addr - 15, cal_buf[end_addr - 15]);
					info("[%s] empty Cal", __func__);
					return 0;
				}
			}
		}
	}
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	if (finfo->rom_xtc_cal_data_addr_list_len <= 0) {
		err("Not available DT, skip set cal");
		sensor_gn3_eeprom_cal_available = false;
		return 0;
	}

	info("[%s] XTC start\n", __func__);
	start_addr = finfo->rom_xtc_cal_data_addr_list[GN3_CAL_START_ADDR];
	if (finfo->rom_xtc_cal_endian_check) {
		if (cal_buf[start_addr] == 0xFF && cal_buf[start_addr + 1] == 0x00)
			endian = GN3_BIG_ENDIAN;
		else
			endian = GN3_LITTLE_ENDIAN;

		start_addr = start_addr + 2;
	} else {
		endian = GN3_BIG_ENDIAN;
	}

	for (len = 0; len < finfo->rom_xtc_cal_data_addr_list_len / GN3_CAL_ROW_LEN; len ++) {
		ret |= sensor_cis_set_registers(subdev, sensor_gn3_pre_XTC[len], sensor_gn3_pre_XTC_size[len]);

		dbg_sensor(1, "[%s] XTC Calibration Data E\n", __func__);
		if (len != 0) start_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_START_ADDR];
		end_addr = finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_END_ADDR];

#if IS_ENABLED(GN3_BURST_WRITE)
		if (finfo->rom_xtc_cal_data_addr_list[len * GN3_CAL_ROW_LEN + GN3_CAL_BURST_CHECK]) {
			cal_size = (end_addr - start_addr) / 2 + 1;
			dbg_sensor(1, "[%s] rom_xtc_cal burst write start(0x%X) end(0x%X) size(%d)\n", __func__, start_addr, end_addr, cal_size);
			ret = sensor_gn3_cis_write16_burst(cis->client, 0x6F12, (u8 *)&cal_buf[start_addr], cal_size, endian);
			if (ret < 0) {
				err("sensor_gn3_cis_write16_burst fail!!");
				goto p_err;
			}
		} else
#endif
		{
			for(i = start_addr; i <= end_addr; i += 2) {
				val = GN3_ENDIAN(cal_buf[i], cal_buf[i + 1], endian);
				ret = is_sensor_write16(cis->client, 0x6F12, val);
				if (ret < 0) {
					err("is_sensor_write16 fail!!");
					goto p_err;
				}
#ifdef DEBUG_CAL_WRITE
				info("cal offset[0x%04X] , val[0x%04X]", i, val);
#endif
			}
		}

		dbg_sensor(1, "[%s] XTC Calibration Data X\n", __func__);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_post_XTC[len], sensor_gn3_post_XTC_size[len]);
	}
	info("[%s] XTC end\n", __func__);

	sensor_gn3_eeprom_cal_available = true;

	info("[%s] X", __func__);

p_err:
	return ret;
}
#endif

int sensor_gn3_cis_check_aeb(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 mode = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor *device;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	mode = cis->cis_data->sens_config_index_cur;

	if (cis->cis_data->stream_on == false
		|| is_sensor_g_ex_mode(device) != EX_AEB
		|| sensor_gn3_spec_mode_attr[mode].aeb_support == false) {
		if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
			info("%s : current AEB status is enabled. need AEB disable", __func__);
			cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			/* AEB disable */
			is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			is_sensor_write8(cis->client, 0x0E00, 0x00);
			info("%s : disable AEB in not support mode\n", __func__);
		}
		ret = -1; //continue;
		goto EXIT;
	}

	if (cis->cis_data->cur_hdr_mode == cis->cis_data->pre_hdr_mode) {
		if (cis->cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2STHDR) {
			ret = -2; //continue; OFF->OFF
		} else {
			ret = 0; //return; ON->ON
		}
		goto EXIT;
	}

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		pr_info("%s : enable 2STHDR\n", __func__);
		sensor_gn3_seamless_delay_count = 3;
		//bracketing_lut_control:0x02 + bracketing_lut_mode:0x03
		ret |= is_sensor_write16(cis->client, 0x0E00, 0x0203);
		//bracketing_loop_mode_num_of_repetitions
		ret |= is_sensor_write16(cis->client, 0x0E02, 0x0000);
		//bracketing_selected_fields [0]:cit, [1]:Again, [5]:Dgain, [10]:FLL, [11]:image VC,DT, [12]:Y VC,DT
		ret |= is_sensor_write16(cis->client, 0x0E04, 0x1C23);

		//LUT1 - Long
		if (sensor_gn3_spec_mode_attr[mode].status_12bit == SENSOR_12BIT_STATE_PSEUDO_12BIT)
			ret |= is_sensor_write16(cis->client, 0x0E18, 0x002C); // image VC:0x00 + image DT:0x2C
		else
			ret |= is_sensor_write16(cis->client, 0x0E18, 0x002B); // image VC:0x00 + image DT:0x2B
		ret |= is_sensor_write16(cis->client, 0x0E1A, 0x0200); // Y VC:0x02
		ret |= is_sensor_write16(cis->client, 0x0E1C, 0x2B00); // Y DT:0x2B

		//LUT2 - Short
		if (sensor_gn3_spec_mode_attr[mode].status_12bit == SENSOR_12BIT_STATE_PSEUDO_12BIT)
			ret |= is_sensor_write16(cis->client, 0x0E26, 0x012C); // image VC:0x01 + image DT:0x2C
		else
			ret |= is_sensor_write16(cis->client, 0x0E26, 0x012B); // image VC:0x01 + image DT:0x2B
		ret |= is_sensor_write16(cis->client, 0x0E28, 0x0300); // Y VC:0x03
		ret |= is_sensor_write16(cis->client, 0x0E2A, 0x2B00); // Y DT:0x2B
	} else {
		pr_info("%s : disable AEB\n", __func__);
		sensor_gn3_seamless_delay_count = 0;
		ret = is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret = is_sensor_write8(cis->client, 0x0E00, 0x00);
	}

	cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode;

EXIT:
	return ret;
}

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_check_12bit(cis_shared_data *cis_data,
								u32 *next_mode,
								u16 *next_fast_change_idx,
								u32 *load_sram_idx)
{
	int ret = 0;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG];
		*next_fast_change_idx =
			sensor_gn3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_check_lownoise(cis_shared_data *cis_data,
								u32 *next_mode,
								u16 *next_fast_change_idx,
								u32 *load_sram_idx)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if ((sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4] == MODE_GROUP_NONE)) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		temp_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2];
		break;
	case IS_CIS_LN4:
		temp_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4];
		break;
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE) {
		ret = -1;
	}

	if (ret == 0) {
		*next_mode = temp_mode;
		*next_fast_change_idx =
			sensor_gn3_spec_mode_retention_attr[*next_mode].fast_change_idx;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_check_cropped_remosaic(cis_shared_data *cis_data,
											u32 cur_fast_change_idx,
											u32 *next_mode,
											u16 *next_fast_change_idx,
											u32 *load_sram_idx)
{
	int ret = 0;
	u32 zoom_ratio = 0;

	if (sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	zoom_ratio = cis_data->cur_remosaic_zoom_ratio;

	if (zoom_ratio != 200) {
		ret = -1; //goto default
		goto EXIT;
	}

	*next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS];

	if (zoom_ratio == cis_data->pre_remosaic_zoom_ratio) {
		*next_fast_change_idx = cur_fast_change_idx;
		*load_sram_idx = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
		ret = -2; //keep cropped remosaic
		dbg_sensor(1, "%s : cur zoom ratio is same pre zoom ratio (%u)", __func__, zoom_ratio);
		goto EXIT;
	}

	info("%s : zoom_ratio %u turn on cropped remosaic", __func__, zoom_ratio);

	if (cur_fast_change_idx == GN3_FCI_NONE) {
		err("%s : fast change idx is none!!", __func__);
		ret = -1;
	} else {
		//apply ABS_1
		*next_fast_change_idx = SENSOR_GN3_ABS_1_FAST_CHANGE_IDX;
		*load_sram_idx = sensor_gn3_spec_mode_retention_attr[*next_mode].load_sram_idx;
	}

EXIT:
	return ret;
}

int sensor_gn3_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	unsigned int mode = 0;
	u16 cur_fast_change_idx = GN3_FCI_NONE;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
	unsigned int next_mode = 0;
	u16 next_fast_change_idx = 0;
	u32 load_sram_idx = 0;
	u32 dummy_value = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	mode = cis->cis_data->sens_config_index_cur;
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	module = sensor_peri->module;
	WARN_ON(!module);

	if (sensor_gn3_seamless_delay_count > 0)
		sensor_gn3_seamless_delay_count--;

	ext_info = &module->ext;
	if (ext_info->use_retention_mode != SENSOR_RETENTION_ACTIVATED) {
		err("retention is not activated");
		ret = -1;
		return ret;
	}

	next_mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		ret = -1;
		return ret;
	}
	next_fast_change_idx = sensor_gn3_spec_mode_retention_attr[next_mode].fast_change_idx;
	if (next_fast_change_idx == GN3_FCI_NONE) {
		dbg_sensor(1, "[%s] fast_change_idx is none", __func__);
		return ret;
	}

	if (cis->cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF) {
		dbg_sensor(1, "[%s] cur_pattern_mode (%d)", __func__, cis->cis_data->cur_pattern_mode);
		return ret;
	}

	load_sram_idx = sensor_gn3_spec_mode_retention_attr[next_mode].load_sram_idx;

#ifdef GN3_SEAMLESS_TEST
	test_cnt++;
#if 1// AEB
	if(test_cnt % 100 == 0) {
		if(cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR)
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		else
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_2STHDR;
		err("test_cnt(%d), cur_hdr_mode changed = %d", test_cnt, cis->cis_data->cur_hdr_mode);
	}
#endif
#if 0 // IDCG
	if(test_cnt % 100 == 0) {
		if(cis->cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT)
			cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_PSEUDO_12BIT;
		else
			cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_REAL_12BIT;
		err("test_cnt(%d), cur_12bit_mode changed = %d", test_cnt, cis->cis_data->cur_12bit_mode);
	}
#endif
#if 0 // LN
	if(test_cnt % 100 == 0) {
		if(cis->cis_data->cur_lownoise_mode == IS_CIS_LNOFF)
			cis->cis_data->cur_lownoise_mode = IS_CIS_LN2;
		else if(cis->cis_data->cur_lownoise_mode == IS_CIS_LN2)
			cis->cis_data->cur_lownoise_mode = IS_CIS_LN4;
		else
			cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
		err("test_cnt(%d), cur_lownoise_mode changed = %d", test_cnt, cis->cis_data->cur_lownoise_mode);
	}
#endif
#if 0 // remosaic crop
	if(test_cnt % 350 == 0) {
		if(cis->cis_data->cur_remosaic_zoom_ratio != 200) {
			cis->cis_data->cur_remosaic_zoom_ratio = 200;
		} else {
			cis->cis_data->cur_remosaic_zoom_ratio = 0;
		}
		err("test_cnt(%d), cur_remosaic_zoom_ratio changed = %d", test_cnt, cis->cis_data->cur_remosaic_zoom_ratio);
	}
#endif
#endif

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (sensor_gn3_cis_check_aeb(subdev) == 0) {
		goto p_i2c_unlock;;
	}

	sensor_gn3_cis_check_lownoise(cis->cis_data,
									&next_mode,
									&next_fast_change_idx,
									&load_sram_idx);
	sensor_gn3_cis_check_12bit(cis->cis_data,
								&next_mode,
								&next_fast_change_idx,
								&load_sram_idx);

	is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	is_sensor_read16(cis->client, 0x0B30, &cur_fast_change_idx);

	sensor_gn3_cis_check_cropped_remosaic(cis->cis_data,
											cur_fast_change_idx,
											&next_mode,
											&next_fast_change_idx,
											&load_sram_idx);

	if ((mode == next_mode && cur_fast_change_idx == next_fast_change_idx)
		|| next_mode == MODE_GROUP_NONE) {
			goto p_i2c_unlock;;
	}

	if (load_sram_idx != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
		ret = sensor_cis_set_registers(subdev,
			sensor_gn3_load_sram[load_sram_idx],
			sensor_gn3_load_sram_size[load_sram_idx]);
		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
		}
	}

	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);

	if (cis->cis_data->stream_on == true)
		ret |= is_sensor_write16(cis->client, 0x0B32, 0x0100);

	ret |= is_sensor_write16(cis->client, 0x0B30, next_fast_change_idx);
	ret |= is_sensor_write16(cis->client, 0x6000, 0x0085);
	if (ret < 0)
		err("sensor_gn3_set_registers fail!!");

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	sensor_gn3_cis_data_calculation(sensor_gn3_pllinfos[next_mode], cis->cis_data);

	if (cis->cis_data->cur_lownoise_mode > 0
		|| cis->cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT) {
		sensor_gn3_seamless_delay_count = 3;
	}

	info("[%s] pre(%d)->cur(%d), 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => load_sram_idx[%d] fast_change_idx[0x%x]\n",
		__func__,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		load_sram_idx,
		next_fast_change_idx);

	CALL_CISOPS(cis, cis_get_min_analog_gain, subdev, &dummy_value);
	dbg_sensor(1, "[%s] min again : %d\n", __func__, dummy_value);
	CALL_CISOPS(cis, cis_get_max_analog_gain, subdev, &dummy_value);
	dbg_sensor(1, "[%s] max again : %d\n", __func__, dummy_value);
	CALL_CISOPS(cis, cis_get_min_digital_gain, subdev, &dummy_value);
	dbg_sensor(1, "[%s] min dgain : %d\n", __func__, dummy_value);
	CALL_CISOPS(cis, cis_get_max_digital_gain, subdev, &dummy_value);
	dbg_sensor(1, "[%s] max dgain : %d\n", __func__, dummy_value);
	cis->cis_data->pre_remosaic_zoom_ratio = cis->cis_data->cur_remosaic_zoom_ratio;
	cis->cis_data->pre_12bit_mode = cis->cis_data->cur_12bit_mode;
	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;

p_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	return ret;
}
#endif

int sensor_gn3_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info = NULL;
	struct is_device_sensor *device;
	u32 ex_mode;
	u32 dummy_max_again;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	int load_sram_idx = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
	int load_sram_idx_ln2 = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
	int load_sram_idx_ln4 = SENSOR_GN3_LOAD_SRAM_IDX_NONE;
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	if (unlikely(!device)) {
		err("device sensor is null");
		return -EINVAL;
	}

	if (mode >= sensor_gn3_max_setfile_num) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_seamless_delay_count = 0;

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	info("[%s] E\n", __func__);

	sensor_gn3_cis_get_max_analog_gain(subdev, &dummy_max_again);

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ex_mode = is_sensor_g_ex_mode(device);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* Retention mode sensor mode select */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_gn3_load_retention = false;

		sensor_gn3_cis_set_mode_group(mode);

		fast_change_idx = sensor_gn3_spec_mode_retention_attr[mode].fast_change_idx;

		if (fast_change_idx != GN3_FCI_NONE) {
			load_sram_idx = sensor_gn3_spec_mode_retention_attr[mode].load_sram_idx;

			if(mode != SENSOR_GN3_2040X1148_480FPS) {
				ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
				ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);
				ret |= is_sensor_write16(cis->client, 0x0118, 0x0000);
				ret |= is_sensor_write16(cis->client, 0x0A52, 0x0000);
				ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2001);
				ret |= is_sensor_write16(cis->client, 0x1130, 0x0000);
				ret |= is_sensor_write16(cis->client, 0x5670, 0x0000);
			}

			if (load_sram_idx != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_set_registers(subdev,
					sensor_gn3_load_sram[load_sram_idx],
					sensor_gn3_load_sram_size[load_sram_idx]);
				if (ret < 0) {
					err("sensor_gn3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			if (load_sram_idx_ln2 != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_set_registers(subdev,
					sensor_gn3_load_sram[load_sram_idx_ln2],
					sensor_gn3_load_sram_size[load_sram_idx_ln2]);
				if (ret < 0) {
					err("sensor_gn3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			if (load_sram_idx_ln4 != SENSOR_GN3_LOAD_SRAM_IDX_NONE) {
				ret = sensor_cis_set_registers(subdev,
					sensor_gn3_load_sram[load_sram_idx_ln4],
					sensor_gn3_load_sram_size[load_sram_idx_ln4]);
				if (ret < 0) {
					err("sensor_gn3_set_registers fail!!");
					goto p_err_i2c_unlock;
				}
			}

			info("[%s] load_sram_idx(%d) ln2(%d) ln4(%d)\n", __func__, load_sram_idx, load_sram_idx_ln2, load_sram_idx_ln4);

			if(mode == SENSOR_GN3_2040X1148_480FPS) {
				info("[%s]SSM additional setting", __func__);
				ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
				ret |= is_sensor_write16(cis->client, 0x0340, 0x2D20);
				ret |= is_sensor_write16(cis->client, 0x0118, 0x0101);
				ret |= is_sensor_write16(cis->client, 0x0A52, 0x0001);
				ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2001);
				ret |= is_sensor_write16(cis->client, 0x1130, 0x0003);
				ret |= is_sensor_write16(cis->client, 0x5670, 0x0001);
			}

			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x0B30, fast_change_idx);
			ret |= is_sensor_write16(cis->client, 0x6000, 0x0085);

			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		} else {
			info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

			//Fast Change disable
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);
			ret |= is_sensor_write16(cis->client, 0x0B30, 0x00FF);

			ret |= sensor_cis_set_registers(subdev, sensor_gn3_setfiles[mode],
								sensor_gn3_setfile_sizes[mode]);

			if (ret < 0) {
				err("sensor_gn3_set_registers fail!!");
				goto p_err_i2c_unlock;
			}
		}
	} else
#endif
	{
		info("[%s] not support retention sensor mode(%d)\n", __func__, mode);

		//Fast Change disable
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);
		ret |= is_sensor_write16(cis->client, 0x0B30, 0x00FF);

		ret |= sensor_cis_set_registers(subdev, sensor_gn3_setfiles[mode],
								sensor_gn3_setfile_sizes[mode]);

		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err_i2c_unlock;
		}
	}

	cis->cis_data->sens_config_index_pre = mode;

	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(cis->client, 0x6028, 0x2000);
	ret |= is_sensor_write16(cis->client, 0x602A, 0xEB60);
	if (ex_mode == EX_AI_REMOSAIC) {
		ret |= is_sensor_write16(cis->client, 0x6F12, 0x0000); // Remosaic off - Tetra Pattern
	} else {
		ret |= is_sensor_write16(cis->client, 0x6F12, 0x0200); // Remosaic on - Bayer Pattern
	}

	cis->cis_data->highres_capture_mode = sensor_gn3_spec_mode_attr[mode].highres_capture_mode;

	cis->cis_data->pre_12bit_mode = sensor_gn3_spec_mode_attr[mode].status_12bit;
	cis->cis_data->cur_12bit_mode = sensor_gn3_spec_mode_attr[mode].status_12bit;

	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	cis->cis_data->pre_remosaic_zoom_ratio = 0;
	cis->cis_data->cur_remosaic_zoom_ratio = 0;

	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	/* AEB disable */
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write8(cis->client, 0x0E00, 0x00);

	info("[%s] mode[%d] 12bit[%d] LN[%d] ZOOM[%d] AEB[%d] => load_sram_idx[%d] fast_change_idx[0x%x]\n",
		__func__, mode,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_remosaic_zoom_ratio,
		cis->cis_data->cur_hdr_mode,
		load_sram_idx,
		fast_change_idx);

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_gn3_cis_update_seamless_mode(subdev);
#endif

p_err:
	/* sensor_gn3_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_gn3_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	/* setfile global setting is at camera entrance */
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE) {
		sensor_gn3_cis_set_global_setting_internal(subdev);
		sensor_gn3_cis_retention_prepare(subdev);
	} else if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
		sensor_gn3_cis_retention_crc_check(subdev);
	} else { /* SENSOR_RETENTION_UNSUPPORTED */
		sensor_gn3_eeprom_cal_available = false;
		sensor_gn3_cis_set_global_setting_internal(subdev);
	}
#else
	WARN_ON(!subdev);

	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_gn3_cis_set_global_setting_internal(subdev);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}
	info("[%s] global setting done\n", __func__);
#endif

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
#else
p_err:
#endif
	return ret;
}


#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_retention_prepare(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
	u32 wait_cnt = 0, time_out_cnt = 250;
	u16 check = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);

	I2C_MUTEX_LOCK(cis->i2c_lock);
#if IS_ENABLED(CIS_CALIBRATION)
	ret |= sensor_gn3_cis_set_cal(subdev);
	ret |= sensor_cis_set_registers(subdev, sensor_gn3_goldenCal, sensor_gn3_goldenCal_size);
	if (ret < 0) {
		err("sensor_gn3_cis_set_cal fail!!");
		goto p_err;
	}
#endif

	for (i = 0; i < sensor_gn3_max_retention_num; i++) {
		ret = sensor_cis_set_registers(subdev, sensor_gn3_retention[i], sensor_gn3_retention_size[i]);
		if (ret < 0) {
			err("sensor_gn3_set_registers fail!!");
			goto p_err;
		}
	}

	//FAST AE Setting
	ret = sensor_cis_set_registers(subdev, sensor_gn3_setfiles[SENSOR_GN3_2040X1532_120FPS],
						sensor_gn3_setfile_sizes[SENSOR_GN3_2040X1532_120FPS]);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_err;
	}

	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(cis->client, 0x6000, 0x0005);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x2000);
	ret |= is_sensor_write16(cis->client, 0x0F74, 0x0000);
	ret |= is_sensor_write16(cis->client, 0x0F78, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F7A, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F7C, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F7E, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F80, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F82, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F84, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F86, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F88, 0x2002);
	ret |= is_sensor_write16(cis->client, 0x0F98, 0x5920);
	ret |= is_sensor_write16(cis->client, 0x0F9A, 0x5F60);
	ret |= is_sensor_write16(cis->client, 0x0F9C, 0x65A0);
	ret |= is_sensor_write16(cis->client, 0x0F9E, 0x6BE0);
	ret |= is_sensor_write16(cis->client, 0x0FA0, 0x7220);
	ret |= is_sensor_write16(cis->client, 0x0FA2, 0x7860);
	ret |= is_sensor_write16(cis->client, 0x0FA4, 0xA3D0);
	ret |= is_sensor_write16(cis->client, 0x0FA6, 0xAA10);
	ret |= is_sensor_write16(cis->client, 0x0FA8, 0xB050);
	ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(cis->client, 0x0B30, 0x01FF);
	ret |= is_sensor_write16(cis->client, 0x6000, 0x0085);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (sensor_gn3_need_stream_on_retention) {
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_write16(cis->client, 0x0100, 0x0103); //stream on
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		sensor_gn3_cis_wait_streamon(subdev);
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_write16(cis->client, 0x010E, 0x0100); //Retention checksum enable
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_write16(cis->client, 0x0100, 0x0003); //stream off
		I2C_MUTEX_UNLOCK(cis->i2c_lock);

		sensor_gn3_cis_wait_streamoff(subdev);
		sensor_gn3_need_stream_on_retention = false;
	}

	if (ret < 0) {
		err("is_sensor_write fail!!");
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_read16(cis->client, 0x19C4, &check);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	while (check != 0x0100) {
		usleep_range(500, 500);
		wait_cnt++;
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_read16(cis->client, 0x19C4, &check);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x19C4 (0x%x) fail!!");
			goto p_err;
		}
	}

	ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;

	info("[%s] retention sensor RAM write done\n", __func__);

p_err:

	return ret;
}

int sensor_gn3_cis_retention_crc_check(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 crc_check = 0;
	struct is_cis *cis = NULL;
	u32 wait_cnt = 0, time_out_cnt = 10;
	u16 check = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	is_sensor_write16(cis->client, 0xFCFC, 0x4000);
	is_sensor_write16(cis->client, 0x6000, 0x0005);
	is_sensor_read16(cis->client, 0x010E, &check);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	while (check != 0x0000) {
		usleep_range(1500, 1500);
		wait_cnt++;
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_read16(cis->client, 0x010E, &check);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x010E (0x%x) fail!!");
			break;
		}
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* retention mode CRC check */
	is_sensor_read16(cis->client, 0x19C2, &crc_check); /* api_ro_checksum_on_ram_passed */

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	if (crc_check == 0x0100) {
		info("[%s] retention SRAM CRC check: pass!\n", __func__);

		/* init pattern */
		is_sensor_write16(cis->client, 0x0600, 0x0000);
	} else {
		info("[%s] retention SRAM CRC check: fail!\n", __func__);
		info("retention CRC Check register value: 0x%x\n", crc_check);
		info("[%s] rewrite retention modes to SRAM\n", __func__);

		ret = sensor_gn3_cis_set_global_setting_internal(subdev);
		if (ret < 0) {
			err("CRC error recover: rewrite sensor global setting failed");
			goto p_err;
		}

		sensor_gn3_eeprom_cal_available = false;

		ret = sensor_gn3_cis_retention_prepare(subdev);
		if (ret < 0) {
			err("CRC error recover: retention prepare failed");
			goto p_err;
		}
	}

p_err:
	return ret;
}
#endif

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_gn3_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x = 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);

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
	ret = sensor_gn3_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (SENSOR_GN3_MAX_WIDTH / cis_data->cur_width);
		ratio_h = (SENSOR_GN3_MAX_HEIGHT / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > SENSOR_GN3_MAX_WIDTH) ||
		((cis_data->cur_height * ratio_h) > SENSOR_GN3_MAX_HEIGHT)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	/* 1. page_select */
	ret = is_sensor_write16(client, 0xFCFC, 0x4000);
	if (ret < 0)
		goto p_err_i2c_unlock;

	/* 2. pixel address region setting */
	start_x = ((SENSOR_GN3_MAX_WIDTH - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((SENSOR_GN3_MAX_HEIGHT - cis_data->cur_height * ratio_h) / 2) & (~0x1);
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

#if 0
	/* 5. binnig setting */
	ret = is_sensor_write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = is_sensor_write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;
#endif

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full, 4:Separate vertical) */
	ret = is_sensor_write16(client, 0x0402, 0x0000);
	if (ret < 0)
		goto p_err_i2c_unlock;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed))
	 * down scale factor = down_scale_m / down_scale_n
	 */
	ret = is_sensor_write16(client, 0x0404, 0x10);
	if (ret < 0)
		goto p_err_i2c_unlock;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n",
		__func__, cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	int retry_count = 0;
	int max_retry_count = 10;

	WARN_ON(!subdev);

	info("%s : E\n", __func__);

	if (sensor_gn3_need_stream_on_retention) {
		for (retry_count = 0; retry_count < max_retry_count; retry_count++) {
			ret = sensor_cis_wait_streamon(subdev);
			if (ret < 0) {
				err("[%s] wait failed retry %d", __func__, retry_count);
			} else {
				break;
			}
		}
		if (ret < 0)
			goto p_err;
	} else {
		ret = sensor_cis_wait_streamon(subdev);
		if (ret < 0)
			goto p_err;
	}

p_err:
	info("%s : X ret[%d] retry_count[%d]\n", __func__, ret, retry_count);

	return ret;
}

int sensor_gn3_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_cis *cis;
	u32 wait_cnt = 0, time_out_cnt = 4;
	u16 check = 0;
#endif

	WARN_ON(!subdev);

	info("%s : E\n", __func__);

	ret = sensor_cis_wait_streamoff(subdev);
	if (ret < 0)
		goto p_err;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (unlikely(!cis)) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_read16(cis->client, 0x19C4, &check);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	wait_cnt = 0;
	while (check != 0x0100) {
		usleep_range(500, 500);
		wait_cnt++;
		I2C_MUTEX_LOCK(cis->i2c_lock);
		ret |= is_sensor_read16(cis->client, 0x19C4, &check);
		I2C_MUTEX_UNLOCK(cis->i2c_lock);
		if (wait_cnt >= time_out_cnt) {
			err("check 0x19C4 (0x%x) fail!!");
			goto p_err;
		}
	}
#endif

p_err:
	info("%s : X, ret[%d] wait_cnt[%d]\n", __func__, ret, wait_cnt);

	return ret;
}

int sensor_gn3_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_device_sensor *device;
	u16 fast_change_idx = 0x00FF;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	WARN_ON(!sensor_peri);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	I2C_MUTEX_LOCK(cis->i2c_lock);

#ifdef DEBUG_GN3_PLL
	{
		u16 pll;

		is_sensor_read16(client, 0x0300, &pll);
		dbg_sensor(1, "______ vt_pix_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0302, &pll);
		dbg_sensor(1, "______ vt_sys_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0304, &pll);
		dbg_sensor(1, "______ vt_pre_pll_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x0306, &pll);
		dbg_sensor(1, "______ vt_pll_multiplier(0x%x)\n", pll);
		is_sensor_read16(client, 0x0308, &pll);
		dbg_sensor(1, "______ op_pix_clk_div(0x%x)\n", pll);
		is_sensor_read16(client, 0x030a, &pll);
		dbg_sensor(1, "______ op_sys_clk_div(0x%x)\n", pll);

		is_sensor_read16(client, 0x030c, &pll);
		dbg_sensor(1, "______ vt_pll_post_scaler(0x%x)\n", pll);
		is_sensor_read16(client, 0x030e, &pll);
		dbg_sensor(1, "______ op_pre_pll_clk_dv(0x%x)\n", pll);
		is_sensor_read16(client, 0x0310, &pll);
		dbg_sensor(1, "______ op_pll_multiplier(0x%x)\n", pll);
		is_sensor_read16(client, 0x0312, &pll);
		dbg_sensor(1, "______ op_pll_post_scalar(0x%x)\n", pll);

		is_sensor_read16(client, 0x0340, &pll);
		dbg_sensor(1, "______ frame_length_lines(0x%x)\n", pll);
		is_sensor_read16(client, 0x0342, &pll);
		dbg_sensor(1, "______ line_length_pck(0x%x)\n", pll);
	}
#endif

	/*
	 * If a companion is used,
	 * then 8 ms waiting is needed before the StreamOn of a sensor (S5KGN3).
	 */
	if (test_bit(IS_SENSOR_PREPROCESSOR_AVAILABLE, &sensor_peri->peri_state))
		mdelay(8);

	is_sensor_write16(client, 0xFCFC, 0x4000);
	is_sensor_write16(client, 0x6000, 0x0005);

#if 0
#ifndef CONFIG_SEC_FACTORY
	if (!sensor_gn3_eeprom_cal_available) {
		is_sensor_write8(client, 0x0D0B, 0x00); //PDXTC
		is_sensor_write8(client, 0x0D0A, 0x00); //GGC
		is_sensor_write8(client, 0x0B00, 0x00); //NonaXTC
	}
#endif
#endif

	info("[%s] dual sync default master\n", __func__);
	ret = sensor_cis_set_registers(subdev, sensor_gn3_cis_dual_master_settings, sensor_gn3_cis_dual_master_settings_size);
	cis->cis_data->dual_slave = false;

#if IS_ENABLED(USE_SELECT_GN3_DUAL_SYNC_PIN)
	if (sensor_gn3_select_gpio1) {
		is_sensor_write16(cis->client, 0x6028, 0x2000);
		is_sensor_write16(cis->client, 0x602A, 0x0F24);
		is_sensor_write16(cis->client, 0x6F12, 0x0001);
	}
#endif

	is_sensor_read16(client, 0x0B30, &fast_change_idx);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	is_sensor_write16(client, 0x19C4, 0x0000);
	is_sensor_write16(client, 0x0B32, 0x0000);
#endif
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	/* update mipi rate */
	is_vendor_set_mipi_clock(device);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	is_sensor_write16(client, 0x6000, 0x0085);

	/* Sensor stream on */
	info("%s - set_cal_available(%d), fast_change_idx(0x%x)\n",
			__func__, sensor_gn3_eeprom_cal_available, fast_change_idx);
	is_sensor_write16(client, 0x0100, 0x0103);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = true;
	sensor_gn3_load_retention = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_wait_seamless_update_delay(struct v4l2_subdev *subdev)
{
	/* LN Off -> LN2 -> N+2 frame -> Stream Off */
	/* AEB Off -> AEB ON -> N+2 frame -> Stream Off */
	if (sensor_gn3_seamless_delay_count > 0) {
		info("%s: sensor_gn3_seamless_delay_count : %d ->(%d ms)\n",
			__func__, sensor_gn3_seamless_delay_count, 100 * sensor_gn3_seamless_delay_count);
		msleep(100 * sensor_gn3_seamless_delay_count);
	}

	return 0;
}

int sensor_gn3_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = 0x00FF;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	WARN_ON(!ext_info);
#endif

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = sensor_gn3_cis_group_param_hold(subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR
		&& cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		info("%s : current AEB status is enabled. need AEB disable", __func__);
		cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		/* AEB disable */
		is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		is_sensor_write8(cis->client, 0x0E00, 0x00);
		info("%s : disable AEB\n", __func__);
	}

	is_sensor_read8(client, 0x0005, &cur_frame_count);
	is_sensor_write16(client, 0xFCFC, 0x4000);
	is_sensor_write16(client, 0x6000, 0x0005);
	is_sensor_read16(client, 0x0B30, &fast_change_idx);
	info("%s: frame_count(0x%x), fast_change_idx(0x%x)\n", __func__, cur_frame_count, fast_change_idx);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	/* retention mode CRC check register enable */
	is_sensor_write16(client, 0x010E, 0x0100);
	is_sensor_write16(client, 0x19C2, 0x0000);
	if (ext_info->use_retention_mode == SENSOR_RETENTION_INACTIVE) {
		ext_info->use_retention_mode = SENSOR_RETENTION_ACTIVATED;
	}

	info("[MOD:D:%d] %s : retention enable CRC check\n", cis->id, __func__);
#endif

	is_sensor_write16(client, 0x6000, 0x0085);
	is_sensor_write16(client, 0x0100, 0x0003);

	I2C_MUTEX_UNLOCK(cis->i2c_lock);

	cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	unsigned int mode = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u16 coarse_integration_time_shifter = 0;

	u16 cit_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	u16 cit_shifter_val = 0;
	int cit_shifter_idx = 0;
	u8 cit_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_target_exp_backup.short_val = target_exposure->short_val;
	sensor_gn3_target_exp_backup.long_val = target_exposure->long_val;

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
		default:
			if (MAX(target_exposure->long_val, target_exposure->short_val) > 160000) {
				cit_shifter_idx = MIN(MAX(MAX(target_exposure->long_val, target_exposure->short_val) / 160000, 0), 32);
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

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	long_coarse_int = ALIGN_DOWN(long_coarse_int, sensor_gn3_spec_mode_attr[mode].cit_align);
	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}

	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int)
											/ line_length_pck;
	short_coarse_int = ALIGN_DOWN(short_coarse_int, sensor_gn3_spec_mode_attr[mode].cit_align);
	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	cis_data->cur_long_exposure_coarse = long_coarse_int;
	cis_data->cur_short_exposure_coarse = short_coarse_int;

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_gn3_load_retention = false;

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		ret |= is_sensor_write16(client, AEB_GN3_LUT0 + AEB_GN3_OFFSET_CIT, long_coarse_int);
		ret |= is_sensor_write16(client, AEB_GN3_LUT1 + AEB_GN3_OFFSET_CIT, short_coarse_int);

		if (ret < 0)
			goto p_err_i2c_unlock;

		dbg_sensor(1, "%s, vsync_cnt(%d), fll(%#x), lcit %#x, scit %#x cit_shift %#x\n",
			__func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
			long_coarse_int, short_coarse_int, coarse_integration_time_shifter);
	} else {
		/* Short exposure */
		ret = is_sensor_write16(client, 0x0202, short_coarse_int);
		if (ret < 0)
			goto p_err_i2c_unlock;

		/* CIT shifter */
		if (cis->long_term_mode.sen_strm_off_on_enable == false) {
			ret = is_sensor_write16(client, 0x0704, coarse_integration_time_shifter);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
			KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n",
			cis->id, __func__, cis_data->sen_vsync_count, vt_pic_clk_freq_khz/1000,
			line_length_pck, min_fine_int);
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
			KERN_CONT "long_coarse_int %#x, short_coarse_int %#x coarse_integration_time_shifter %#x\n",
			cis->id, __func__, cis_data->sen_vsync_count, cis_data->frame_length_lines,
			long_coarse_int, short_coarse_int, coarse_integration_time_shifter);
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_exposure_time(struct v4l2_subdev *subdev, u32 *min_expo)
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

	WARN_ON(!subdev);
	WARN_ON(!min_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
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

int sensor_gn3_cis_get_max_exposure_time(struct v4l2_subdev *subdev, u32 *max_expo)
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

	WARN_ON(!subdev);
	WARN_ON(!max_expo);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	if (vt_pic_clk_freq_khz == 0) {
		pr_err("[MOD:D:%d] %s, Invalid vt_pic_clk_freq_khz(%d)\n", cis->id, __func__, vt_pic_clk_freq_khz/1000);
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
			cis_data->max_margin_fine_integration_time, cis_data->max_coarse_integration_time);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_gn3_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
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

	WARN_ON(!subdev);
	WARN_ON(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

#if (!IS_ENABLED(USE_CAMERA_SENSOR_RETENTION))
	I2C_MUTEX_LOCK(cis->i2c_lock);
	sensor_gn3_cis_check_aeb(subdev);
	I2C_MUTEX_UNLOCK(cis->i2c_lock);
#endif

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000
						- cis_data->min_fine_integration_time) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count,
			input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);
	} else {
		*target_duration = frame_duration;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u8 frame_length_lines_shifter = 0;

	u8 fll_shifter_array[33] = {0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6};
	int fll_shifter_idx = 0;
	u8 fll_denom_array[7] = {1, 2, 4, 8, 16, 32, 64};
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		if (frame_duration < cis_data->min_frame_us_time) {
			dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
			frame_duration = cis_data->min_frame_us_time;
		}
	}

	sensor_gn3_frame_duration_backup = frame_duration;
	cis_data->cur_frame_us_time = frame_duration;

	if (cis->long_term_mode.sen_strm_off_on_enable == false) {
		switch(cis_data->sens_config_index_cur) {
		default:
			if (frame_duration > 160000) {
				fll_shifter_idx = MIN(MAX(frame_duration / 160000, 0), 32);
				frame_length_lines_shifter = fll_shifter_array[fll_shifter_idx];
				frame_duration = frame_duration / fll_denom_array[frame_length_lines_shifter];
			} else {
				frame_length_lines_shifter = 0x0;
			}
			break;
		}
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	I2C_MUTEX_LOCK(cis->i2c_lock);

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		frame_duration /= 2;
		frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

		ret |= is_sensor_write16(client, AEB_GN3_LUT0 + AEB_GN3_OFFSET_FLL, frame_length_lines);
		ret |= is_sensor_write16(client, AEB_GN3_LUT1 + AEB_GN3_OFFSET_FLL, frame_length_lines);

		dbg_sensor(1, "%s, vsync_cnt(%d), frame_duration = %d us, fll %#x\n",
			__func__, cis_data->sen_vsync_count, frame_duration, frame_length_lines);
	} else {
		ret |= is_sensor_write16(client, 0x0340, frame_length_lines);

		if (ret < 0)
			goto p_err_i2c_unlock;

		/* frame duration shifter */
		if (cis->long_term_mode.sen_strm_off_on_enable == false) {
			ret = is_sensor_write8(client, 0x0702, frame_length_lines_shifter);
			if (ret < 0)
				goto p_err_i2c_unlock;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
			KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x), frame_length_lines_shifter(%#x)\n",
			cis->id, __func__, vt_pic_clk_freq_khz/1000, frame_duration,
			line_length_pck, frame_length_lines, frame_length_lines_shifter);
	}

	cis_data->frame_length_lines = frame_length_lines;
	cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	cis_data->frame_length_lines_shifter = frame_length_lines_shifter;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_set_frame_rate(struct v4l2_subdev *subdev, u32 min_fps)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 frame_duration = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

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

	ret = sensor_gn3_cis_set_frame_duration(subdev, frame_duration);
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

int sensor_gn3_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	u32 again_code = 0;
	u32 again_permile = 0;

	WARN_ON(!subdev);
	WARN_ON(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

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

	return ret;
}

int sensor_gn3_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	u16 short_analog_gain = 0;

	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_again_backup.short_val = again->short_val;
	sensor_gn3_again_backup.long_val = again->long_val;

	analog_gain = (u16)sensor_cis_calc_again_code(again->val);
	short_analog_gain = (u16)sensor_cis_calc_again_code(again->short_val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	if (short_analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper short_analog_gain value, reset to min_analog_gain\n", __func__);
		short_analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (short_analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper short_analog_gain value, reset to max_analog_gain\n", __func__);
		short_analog_gain = cis->cis_data->max_analog_gain[0];
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis->cis_data->stream_on == false)
		sensor_gn3_load_retention = false;

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		ret |= is_sensor_write16(client, AEB_GN3_LUT0 + AEB_GN3_OFFSET_AGAIN, analog_gain);
		ret |= is_sensor_write16(client, AEB_GN3_LUT1 + AEB_GN3_OFFSET_AGAIN, short_analog_gain);

		dbg_sensor(1, "%s, vsync_cnt(%d), input_again = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			__func__, cis->cis_data->sen_vsync_count, again->long_val, again->short_val, analog_gain, short_analog_gain);
	} else {
		ret = is_sensor_write16(client, 0x0204, analog_gain);
		if (ret < 0)
			goto p_err_i2c_unlock;

		dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	ret = is_sensor_read16(client, 0x0204, &analog_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*again = sensor_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_analog_gain(struct v4l2_subdev *subdev, u32 *min_again)
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
	cis_data->min_analog_gain[0] = 0x20; /* x1, gain=x/0x20 */
	cis_data->min_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->min_analog_gain[0]);

	*min_again = cis_data->min_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_analog_gain[0],
		cis_data->min_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_get_max_analog_gain(struct v4l2_subdev *subdev, u32 *max_again)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();
	u32 mode;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	bool aeb_on;
#endif

	WARN_ON(!subdev);
	WARN_ON(!max_again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	aeb_on = sensor_gn3_spec_mode_attr[mode].aeb_support && (cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR);
	if (!aeb_on) {
		if (cis_data->cur_remosaic_zoom_ratio >= 200
			&& cis_data->cur_remosaic_zoom_ratio <= 290) {
			mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_ABS];
		} else if (cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT) {
			mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_IDCG];
		} else if (cis_data->cur_lownoise_mode == IS_CIS_LN2) {
			mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN2];
		} else if (cis_data->cur_lownoise_mode == IS_CIS_LN4) {
			mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_LN4];
		} else {
			mode = sensor_gn3_mode_groups[SENSOR_GN3_MODE_DEFAULT];
		}

		if (mode == MODE_GROUP_NONE) {
			mode = cis_data->sens_config_index_cur;
		}
	}
#endif

	cis_data->max_analog_gain[0] = sensor_gn3_common_mode_attr[mode].max_analog_gain;
	cis_data->max_analog_gain[1] = sensor_cis_calc_again_permile(cis_data->max_analog_gain[0]);

	*max_again = cis_data->max_analog_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_analog_gain[0],
		cis_data->max_analog_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	sensor_gn3_dgain_backup.short_val = dgain->short_val;
	sensor_gn3_dgain_backup.long_val = dgain->long_val;

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to min_digital_gain\n", __func__);
		long_gain = cis_data->min_digital_gain[0];
	}

	if (long_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to max_digital_gain\n", __func__);
		long_gain = cis_data->max_digital_gain[0];
	}

	if (short_gain < cis_data->min_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to min_digital_gain\n", __func__);
		short_gain = cis_data->min_digital_gain[0];
	}

	if (short_gain > cis_data->max_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to max_digital_gain\n", __func__);
		short_gain = cis_data->max_digital_gain[0];
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);
	if (cis_data->stream_on == false)
		sensor_gn3_load_retention = false;

	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2STHDR) {
		ret |= is_sensor_write16(client, AEB_GN3_LUT0 + AEB_GN3_OFFSET_DGAIN, long_gain);
		ret |= is_sensor_write16(client, AEB_GN3_LUT1 + AEB_GN3_OFFSET_DGAIN, short_gain);

		dbg_sensor(1, "%s, vsync_cnt(%d), input_dgain = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			__func__, cis_data->sen_vsync_count, dgain->long_val, dgain->short_val, long_gain, short_gain);
	} else {
		/* Short digital gain */
		ret = is_sensor_write16(client, 0x020E, short_gain);
		if (ret < 0)
			goto p_err_i2c_unlock;

		dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us,"
			KERN_CONT "long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count,
			dgain->long_val, dgain->short_val, long_gain, short_gain);
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_digital_gain(struct v4l2_subdev *subdev, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 digital_gain = 0;
	ktime_t st = ktime_get();

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(cis->i2c_lock);

	/*
	 * NOTE : In S5KGN3, digital gain is long/short separated, should set 2 registers like below,
	 * Write same value to : 0x020E : short // GreenB
	 * Write same value to : 0x0214 : short // GreenR
	 * Write same value to : Need To find : long
	 */

	ret = is_sensor_read16(client, 0x020E, &digital_gain);
	if (ret < 0)
		goto p_err_i2c_unlock;

	*dgain = sensor_cis_calc_dgain_permile(digital_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_dgain = %d us, digital_gain(%#x)\n",
			cis->id, __func__, *dgain, digital_gain);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_i2c_unlock:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_get_min_digital_gain(struct v4l2_subdev *subdev, u32 *min_dgain)
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

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->min_digital_gain[0],
		cis_data->min_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_get_max_digital_gain(struct v4l2_subdev *subdev, u32 *max_dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();
	u32 mode;

	WARN_ON(!subdev);
	WARN_ON(!max_dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;
	cis_data->max_digital_gain[0] = sensor_gn3_common_mode_attr[mode].max_digital_gain;
	cis_data->max_digital_gain[1] = sensor_cis_calc_dgain_permile(cis_data->max_digital_gain[0]);

	*max_dgain = cis_data->max_digital_gain[1];

	dbg_sensor(1, "[%s] code %d, permile %d\n", __func__, cis_data->max_digital_gain[0],
		cis_data->max_digital_gain[1]);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_gn3_cis_long_term_exposure(struct v4l2_subdev *subdev)
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
		if (lte_mode->expo[0] > 125000) {
#ifdef USE_SENSOR_LONG_EXPOSURE_SHOT
			lte_expousre = lte_mode->expo[0];
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				lte_expousre = lte_expousre / 2;
				shift_count++;
			}
			lte_mode->expo[0] = lte_expousre;
#else
			cit_lshift_val = (unsigned char)(lte_mode->expo[0] / 125000);
			while (cit_lshift_val) {
				cit_lshift_val = cit_lshift_val / 2;
				if (cit_lshift_val > 0)
					shift_count++;
			}
			lte_mode->expo[0] = 125000;
#endif
			ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
			ret |= is_sensor_write8(cis->client, 0x0702, shift_count);
			ret |= is_sensor_write8(cis->client, 0x0704, shift_count);
		}
	} else {
		cit_lshift_val = 0;
		ret |= is_sensor_write16(cis->client, 0xFCFC, 0x4000);
		ret |= is_sensor_write8(cis->client, 0x0702, cit_lshift_val);
		ret |= is_sensor_write8(cis->client, 0x0704, cit_lshift_val);
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

int sensor_gn3_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	unsigned int mode = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
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
	mode = cis_data->sens_config_index_cur;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	coarse_int = ALIGN_DOWN(coarse_int, sensor_gn3_spec_mode_attr[mode].cit_align);
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 1024) {
		compensated_again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_again < cis_data->min_analog_gain[1]) {
			*again = cis_data->min_analog_gain[1];
		} else if (*again >= cis_data->max_analog_gain[1]) {
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		} else {
			//*again = compensated_again;
			*dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
		}

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_again(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_again);
	}

p_err:
	return ret;
}

int sensor_gn3_cis_set_wb_gain(struct v4l2_subdev *subdev, struct wb_gains wb_gains)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	int mode = 0;
	u16 abs_gains[3] = {0, }; /* R, G, B */
	u32 avg_g = 0;
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

	if (!cis->use_wb_gain)
		return ret;

	mode = cis->cis_data->sens_config_index_cur;
	if (!sensor_gn3_spec_mode_attr[mode].wb_gain_support)
		return 0;

	if (wb_gains.gr != wb_gains.gb) {
		err("gr, gb not euqal"); /* check DDK layer */
		return -EINVAL;
	}

	if (wb_gains.gr != 1024) {
		err("invalid gr,gb %d", wb_gains.gr); /* check DDK layer */
		return -EINVAL;
	}

	dbg_sensor(1, "[SEN:%d]%s:DDK vlaue: wb_gain_gr(%d), wb_gain_r(%d), wb_gain_b(%d), wb_gain_gb(%d)\n",
		cis->id, __func__, wb_gains.gr, wb_gains.r, wb_gains.b, wb_gains.gb);

	avg_g = (wb_gains.gr + wb_gains.gb) / 2;
	abs_gains[0] = (u16)(wb_gains.r & 0xFFFF);
	abs_gains[1] = (u16)(avg_g & 0xFFFF);
	abs_gains[2] = (u16)(wb_gains.b & 0xFFFF);

	dbg_sensor(1, "[SEN:%d]%s, abs_gain_r(0x%4X), abs_gain_g(0x%4X), abs_gain_b(0x%4X)\n",
		cis->id, __func__, abs_gains[0], abs_gains[1], abs_gains[2]);

	I2C_MUTEX_LOCK(cis->i2c_lock);
	ret |= is_sensor_write16(client, 0xFCFC, 0x4000);
	ret |= is_sensor_write16(client, 0x0D82, abs_gains[0]);
	ret |= is_sensor_write16(client, 0x0D84, abs_gains[1]);
	ret |= is_sensor_write16(client, 0x0D86, abs_gains[2]);
	if (ret < 0) {
		err("sensor_gn3_set_registers fail!!");
		goto p_i2c_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	I2C_MUTEX_UNLOCK(cis->i2c_lock);

p_err:
	return ret;
}

int sensor_gn3_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	if (ext_info->use_retention_mode != SENSOR_RETENTION_UNSUPPORTED)
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_gn3_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_set_frame_duration(subdev, sensor_gn3_frame_duration_backup);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_set_analog_gain(subdev, &sensor_gn3_again_backup);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_set_digital_gain(subdev, &sensor_gn3_dgain_backup);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_set_exposure_time(subdev, &sensor_gn3_target_exp_backup);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_stream_on(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_wait_streamon(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_gn3_cis_recover_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct sensor_open_extended *ext_info;
#endif

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	module = sensor_peri->module;
	ext_info = &module->ext;
	FIMC_BUG(!ext_info);

	if (ext_info->use_retention_mode != SENSOR_RETENTION_UNSUPPORTED)
		ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
#endif

	info("%s start\n", __func__);

	ret = sensor_gn3_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_stream_off(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_gn3_cis_wait_streamoff(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

static struct is_cis_ops cis_ops_gn3 = {
	.cis_init = sensor_gn3_cis_init,
	.cis_deinit = sensor_gn3_cis_deinit,
	.cis_log_status = sensor_gn3_cis_log_status,
	.cis_group_param_hold = sensor_gn3_cis_group_param_hold,
	.cis_set_global_setting = sensor_gn3_cis_set_global_setting,
	.cis_mode_change = sensor_gn3_cis_mode_change,
	.cis_set_size = sensor_gn3_cis_set_size,
	.cis_stream_on = sensor_gn3_cis_stream_on,
	.cis_stream_off = sensor_gn3_cis_stream_off,
	.cis_set_exposure_time = sensor_gn3_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_gn3_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_gn3_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_gn3_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_gn3_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_gn3_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_gn3_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_gn3_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_gn3_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_gn3_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_gn3_cis_get_max_analog_gain,
	.cis_set_digital_gain = sensor_gn3_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_gn3_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_gn3_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_gn3_cis_get_max_digital_gain,
	.cis_compensate_gain_for_extremely_br = sensor_gn3_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_gn3_cis_wait_streamoff,
#ifdef USE_CAMERA_EMBEDDED_HEADER
	.cis_get_frame_id = sensor_gn3_cis_get_frame_id,
#endif
	.cis_wait_streamon = sensor_gn3_cis_wait_streamon,
	.cis_set_wb_gains = sensor_gn3_cis_set_wb_gain,
	.cis_data_calculation = sensor_gn3_cis_data_calc,
	.cis_set_long_term_exposure = sensor_gn3_cis_long_term_exposure,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_gn3_cis_recover_stream_on,
//	.cis_recover_stream_off = sensor_gn3_cis_recover_stream_off,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
	.cis_update_seamless_mode = sensor_gn3_cis_update_seamless_mode,
	.cis_wait_seamless_update_delay = sensor_gn3_cis_wait_seamless_update_delay,
};

static int cis_gn3_probe(struct i2c_client *client,
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
	cis->cis_ops = &cis_ops_gn3;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->use_wb_gain = true;
	cis->use_seamless_mode = true;

	sensor_gn3_eeprom_cal_available = false;
	sensor_gn3_first_entrance = false;
	sensor_gn3_need_stream_on_retention = true;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
#if IS_ENABLED(CIS_CALIBRATION)
			sensor_gn3_goldenCal = sensor_gn3_setfile_A_19p2_Goldencal;
			sensor_gn3_goldenCal_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Goldencal);
#endif
			sensor_gn3_setfiles = sensor_gn3_setfiles_A_19p2;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_19p2_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A_19p2;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_19p2_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_19p2_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_sizes_retention);
			sensor_gn3_load_sram = sensor_gn3_setfiles_A_19p2_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_19p2_sizes_load_sram;
#endif
		} else {
			err("%s setfile index out of bound, take default (setfile_A mclk: 19.2MHz)", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
#if IS_ENABLED(CIS_CALIBRATION)
			sensor_gn3_goldenCal = sensor_gn3_setfile_A_19p2_Goldencal;
			sensor_gn3_goldenCal_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Goldencal);
#endif
			sensor_gn3_setfiles = sensor_gn3_setfiles_A_19p2;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_19p2_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A_19p2;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A_19p2);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_19p2_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_19p2_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_19p2_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_19p2_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_19p2_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfile_A_19p2_sizes_retention);
			sensor_gn3_load_sram = sensor_gn3_setfiles_A_19p2_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_19p2_sizes_load_sram;
#endif
		}
	}
#ifndef CONFIG_CAMERA_VENDER_MCD
	else {
		if (strcmp(setfile, "default") == 0 ||
				strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A\n", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfiles_A_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_sizes_load_sram;
#endif
		} else {
			err("%s setfile index out of bound, take default (setfile_A)", __func__);
			sensor_gn3_global = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_setfiles = sensor_gn3_setfiles_A;
			sensor_gn3_setfile_sizes = sensor_gn3_setfile_A_sizes;
			sensor_gn3_pllinfos = sensor_gn3_pllinfos_A;
			sensor_gn3_max_setfile_num = ARRAY_SIZE(sensor_gn3_setfiles_A);
			cis->mipi_sensor_mode = sensor_gn3_setfile_A_mipi_sensor_mode;
			cis->mipi_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_mipi_sensor_mode);
			verify_sensor_mode = sensor_gn3_setfile_A_verify_sensor_mode;
			verify_sensor_mode_size = ARRAY_SIZE(sensor_gn3_setfile_A_verify_sensor_mode);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
			sensor_gn3_global_retention = sensor_gn3_setfile_A_Global;
			sensor_gn3_global_retention_size = ARRAY_SIZE(sensor_gn3_setfile_A_Global);
			sensor_gn3_retention = sensor_gn3_setfiles_A_retention;
			sensor_gn3_retention_size = sensor_gn3_setfile_A_sizes_retention;
			sensor_gn3_max_retention_num = ARRAY_SIZE(sensor_gn3_setfiles_A_retention);
			sensor_gn3_load_sram = sensor_gn3_setfile_A_load_sram;
			sensor_gn3_load_sram_size = sensor_gn3_setfile_A_sizes_load_sram;
#endif
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

static const struct of_device_id sensor_cis_gn3_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-gn3",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_gn3_match);

static const struct i2c_device_id sensor_cis_gn3_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_gn3_driver = {
	.probe	= cis_gn3_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_gn3_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_gn3_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_gn3_driver);
#else
static int __init sensor_cis_gn3_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_gn3_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_gn3_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_gn3_init);
#endif

MODULE_LICENSE("GPL");
