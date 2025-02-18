// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-common.h"
#include "is-hw-api-itp.h"
#include "sfr/is-sfr-itp-v1_20.h"

/* IP register direct read/write */
#define ITP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &itp_regs[R], val)
#define ITP_SET_R(base, set, R, val) \
	is_hw_set_reg(base + GET_COREX_OFFSET(set), &itp_regs[R], val)
#define ITP_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base, &itp_regs[R])
#define ITP_GET_R(base, set, R) \
	is_hw_get_reg(base + GET_COREX_OFFSET(set), &itp_regs[R])

#define ITP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &itp_regs[R], &itp_fields[F], val)
#define ITP_SET_F(base, set, R, F, val) \
	is_hw_set_field(base + GET_COREX_OFFSET(set), &itp_regs[R], &itp_fields[F], val)
#define ITP_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &itp_regs[R], &itp_fields[F])
#define ITP_GET_F(base, set, R, F) \
	is_hw_get_field(base, GET_COREX_OFFSET(set), &itp_regs[R], &itp_fields[F])

#define ITP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &itp_fields[F], val)
#define ITP_GET_V(val, F) \
	is_hw_get_field_value(val, &itp_fields[F])

/* Guided value */
#define ITP_MIN_HBLNK			(51)
#define ITP_T1_INTERVAL			(32)
#define ITP_LUT_REG_CNT			(2560)	/* DRC 32x80 */

/* Constraints */
#define ITP_MAX_WIDTH			(2880)

int itp_hw_s_control_init(void __iomem *base, u32 set_id)
{
	ITP_SET_R_DIRECT(base, ITP_R_IP_PROCESSING, 1);

	/*
	 * 0- COUTFIFO end interrupt will be triggered by last data
	 *  1- COUTFIFO end interrupt will be triggered by VVALID fall
	 */
	ITP_SET_R(base, set_id, ITP_R_IP_COUTFIFO_END_ON_VSYNC_FALL, 0);

	ITP_SET_R_DIRECT(base, ITP_R_IP_HBLANK, ITP_MIN_HBLNK);

	return 0;
}

int itp_hw_s_control_config(void __iomem *base, u32 set_id, struct itp_control_config *config)
{
	u32 val;

	val = 0;
	val = ITP_SET_V(val, ITP_F_OTF_IF_0_EN, config->dirty_bayer_sel_dns);
	val = ITP_SET_V(val, ITP_F_OTF_IF_1_EN, config->ww_lc_en);
	val = ITP_SET_V(val, ITP_F_OTF_IF_2_EN, 1);
	val = ITP_SET_V(val, ITP_F_OTF_IF_3_EN, 1);
	val = ITP_SET_V(val, ITP_F_OTF_IF_4_EN, 1);
	ITP_SET_R(base, set_id, ITP_R_IP_OTF_IF_ENABLE, val);

	return 0;
}

void itp_hw_s_control_debug(void __iomem *base, u32 set_id)
{
	ITP_SET_R(base, set_id, ITP_R_IP_OTF_DEBUG_EN, 1);
	ITP_SET_R(base, set_id, ITP_R_IP_OTF_DEBUG_SEL, 2);
}

int itp_hw_s_cinfifo_config(void __iomem *base, u32 set_id, struct itp_cin_cout_config *config)
{
	u32 val;

	if (config->enable) {
		ITP_SET_R(base, set_id, ITP_R_CINFIFO_INPUT_CINFIFO_ENABLE, 1);

		val = 0;
		val = ITP_SET_V(val, ITP_F_CINFIFO_INPUT_IMAGE_WIDTH, config->width);
		val = ITP_SET_V(val, ITP_F_CINFIFO_INPUT_IMAGE_HEIGHT, config->height);
		ITP_SET_R(base, set_id, ITP_R_CINFIFO_INPUT_IMAGE_DIMENSIONS, val);
	} else {
		ITP_SET_R(base, set_id, ITP_R_CINFIFO_INPUT_CINFIFO_ENABLE, 0);
	}

	return 0;
}

int itp_hw_s_coutfifo_init(void __iomem *base, u32 set_id)
{
	u32 val;

	val = 0;
	/*
	 * 0x0 - t2 interval will last at least until frame_start signal will arrive
	 * 0x1 - t2 interval wait for frame_start is disabled
	 */
	val = ITP_SET_V(val, ITP_F_COUTFIFO_MAIN_OUTPUT_T2_DISABLE_WAIT_FOR_FS, 0);
	/*
	 * 0x0 - t2 counter will not reset at frame_start
	 * 0x1 - if  cinfifo_t2_disable_wait_for_fs=0 t2 counter resets at frame_start
	 */
	val = ITP_SET_V(val, ITP_F_COUTFIFO_MAIN_OUTPUT_T2_RESET_COUNTER_AT_FS, 0);
	ITP_SET_R_DIRECT(base, ITP_R_COUTFIFO_MAIN_OUTPUT_T2_WAIT_FOR_FS, val);

	ITP_SET_R_DIRECT(base, ITP_R_COUTFIFO_MAIN_OUTPUT_COUNT_AT_STALL, 1);

	ITP_SET_R(base, set_id, ITP_R_COUTFIFO_MAIN_OUTPUT_T1_INTERVAL, ITP_T1_INTERVAL);
	ITP_SET_R(base, set_id, ITP_R_COUTFIFO_MAIN_OUTPUT_T3_INTERVAL, ITP_MIN_HBLNK);

	return 0;
}

int itp_hw_s_coutfifo_config(void __iomem *base, u32 set_id, struct itp_cin_cout_config *config)
{
	u32 val;

	if (config->enable) {
		ITP_SET_R(base, set_id, ITP_R_COUTFIFO_MAIN_OUTPUT_COUTFIFO_ENABLE, 1);

		val = 0;
		val = ITP_SET_V(val, ITP_F_COUTFIFO_MAIN_OUTPUT_IMAGE_WIDTH, config->width);
		val = ITP_SET_V(val, ITP_F_COUTFIFO_MAIN_OUTPUT_IMAGE_HEIGHT, config->height);
		ITP_SET_R(base, set_id, ITP_R_COUTFIFO_MAIN_OUTPUT_IMAGE_DIMENSIONS, val);
	} else {
		ITP_SET_R(base, set_id, ITP_R_COUTFIFO_MAIN_OUTPUT_COUTFIFO_ENABLE, 0);
	}

	return 0;
}
int itp_hw_s_module_init(void __iomem *base, u32 set_id)
{
	u32 val;

	/* STK */
	ITP_SET_R(base, set_id, ITP_R_STK_HBLANK_DURATION, ITP_MIN_HBLNK + 3);

	/* RGB2YUV */
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_BYPASS, 0);

	/* YUV444TO422 */
	ITP_SET_R(base, set_id, ITP_R_YUV444TO422_ISP_BYPASS, 0);

	val = 0;
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_ORDER, 0);
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_CH3_INV_EN, 1);
	ITP_SET_R(base, set_id, ITP_R_YUV444TO422_CONTROL, val);

	return 0;
}

int itp_hw_s_module_bypass(void __iomem *base, u32 set_id)
{
	u32 val;

	ITP_SET_R(base, set_id, ITP_R_EDGEDET_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_SHARP_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_NFLTR_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_NSMIX_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_STK_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_GAMMA_POST_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_CCM9_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_DRCTMC_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_GAMMA_RGB_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_SKIND_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_SNADD_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_GAMMA_OETF_BYPASS, 1);
	ITP_SET_R(base, set_id, ITP_R_DITHER_BYPASS, 1);

	ITP_SET_R(base, set_id, ITP_R_DMSC_BYPASS, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_BASE_CONFIG, 0x37E);
	ITP_SET_R(base, set_id, ITP_R_DMSC_PRE_DEMOSAICING_1, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_PRE_DEMOSAICING_2, 0x35F5F);
	ITP_SET_R(base, set_id, ITP_R_DMSC_PRE_DEMOSAICING_3, 0x3);
	ITP_SET_R(base, set_id, ITP_R_DMSC_HPF_POWER, 0xA);
	ITP_SET_R(base, set_id, ITP_R_DMSC_IGNORE_RB, 0x101000);
	ITP_SET_R(base, set_id, ITP_R_DMSC_IGNORE_RB_1, 0x480048);
	ITP_SET_R(base, set_id, ITP_R_DMSC_DIR_DETECT, 0x53C1848);
	ITP_SET_R(base, set_id, ITP_R_DMSC_DIR_DETECT_HV_LINEAR, 0x1EEF0008);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CONTRAST_MINIMUM, 0x100);
	ITP_SET_R(base, set_id, ITP_R_DMSC_MERGE_DIR, 0x800A4);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EXTRACT_COLORS_CONFIG, 0x7F);
	ITP_SET_R(base, set_id, ITP_R_DMSC_ALIAS_DETECT_NORM, 0x80078);
	ITP_SET_R(base, set_id, ITP_R_DMSC_DESAT_LIMIT, 0xD);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EXTRACT_SAT, 0x8010);
	ITP_SET_R(base, set_id, ITP_R_DMSC_GREEN_DETECT, 0x28);
	ITP_SET_R(base, set_id, ITP_R_DMSC_GREEN_HUE, 0x14000C0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_GREEN_SAT, 0xDAC0140);
	ITP_SET_R(base, set_id, ITP_R_DMSC_GREEN_SELECTIVITY, 0x1);
	ITP_SET_R(base, set_id, ITP_R_DMSC_POST_PROCESS_CONFIG, 0xFFF);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EXTRACT_DIR_GRID, 0x501450);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EXTRACT_DIR_HV_THR, 0x14);
	ITP_SET_R(base, set_id, ITP_R_DMSC_DIR_DETECT_SELECTION, 0x508);
	ITP_SET_R(base, set_id, ITP_R_DMSC_ADD_COLORS, 0x187678);
	ITP_SET_R(base, set_id, ITP_R_DMSC_ADD_COLORS_GREEN, 0x803E8);
	ITP_SET_R(base, set_id, ITP_R_DMSC_FALSE_COLORS, 0x103);
	ITP_SET_R(base, set_id, ITP_R_DMSC_SHARPENING_CONFIG, 0x7);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_FSHARP_EN, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_T_LH, 0x400040);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_T_HL, 0x400040);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_T_HH, 0x400040);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_E_LH, 0x1000100);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_E_HL, 0x1000100);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GAIN_E_HH, 0x1000100);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_THR_LH, 0x800080);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_THR_HL, 0x800080);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_THR_HH, 0x800080);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_POST_GAIN, 0x1000100);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_LIMIT, 0xFFF0FFF);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_GRID_DIR, 0x320080);
	ITP_SET_R(base, set_id, ITP_R_DMSC_CLEAN_F_LIMIT_GRID_DIR, 0xFFF);
	ITP_SET_R(base, set_id, ITP_R_DMSC_NEAR_EDGE_DESAT_EN, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_FSHARP_GAIN, 0xC000C0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_FSHARP_THRES, 0x400040);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_OGSHARP_GAIN, 0xC000C0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_OGSHARP_THRES, 0x400040);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_LIMIT, 0xC0001C1);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_RED_PRESERVE_EN, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_RED_PRESERVE_GAIN, 0x95);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_RED_PRESERVE_THRES, 0);
	ITP_SET_R(base, set_id, ITP_R_DMSC_EDGE_DESAT_RED_PRESERVE_LIMIT, 0x800);
	ITP_SET_R(base, set_id, ITP_R_DMSC_DESAT_LUMA_GAIN, 0);

	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_R1, 0x1323);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_R2, 0x7535);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_R3, 0x2000);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_G1, 0x2591);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_G2, 0x6ACD);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_G3, 0x6535);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_B1, 0x74C);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_B2, 0x2000);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_B3, 0x7ACD);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_YOS, 0);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_YMIN, 0);
	ITP_SET_R(base, set_id, ITP_R_RGB2YUV_YMAX, 16383);

	val = 0;
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_COEFFS_0_0, 0);
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_COEFFS_0_1, 32);
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_COEFFS_0_2, 64);
	val = ITP_SET_V(val, ITP_F_YUV444TO422_ISP_COEFFS_0_3, 32);
	ITP_SET_R(base, set_id, ITP_R_YUV444TO422_COEFFS, val);

	return 0;
}

int itp_hw_s_module_size(void __iomem *base, u32 set_id, struct is_hw_size_config *config)
{
	u32 val;
	u32 dma_crop_x, crop_x, crop_y;
	u32 scaling_x, scaling_y;
	const u64 binning_factor_x1_256 = 256;
	const u64 binning_factor_x1_1024 = 1024;

	/* w/ bds ratio*/
	dma_crop_x = config->bcrop_width * config->dma_crop_x / config->taasc_width;
	/* w/ sensor binning ratio*/
	crop_x = (config->sensor_crop_x + config->bcrop_x + dma_crop_x) *
		config->sensor_binning_x / 1000;
	crop_y = (config->sensor_crop_y + config->bcrop_y) *
		config->sensor_binning_y / 1000;

	/* DMSC */
	ITP_SET_R(base, set_id, ITP_R_DMSC_IMAGE_WIDTH, config->width);

	/* NSMIX */
	val = 0;
	val = ITP_SET_V(val, ITP_F_NSMIX_SENSOR_WIDTH, config->sensor_calibrated_width);
	val = ITP_SET_V(val, ITP_F_NSMIX_SENSOR_HEIGHT, config->sensor_calibrated_height);
	ITP_SET_R(base, set_id, ITP_R_NSMIX_SENSOR, val);

	val = 0;
	val = ITP_SET_V(val, ITP_F_NSMIX_START_CROP_X, crop_x);
	val = ITP_SET_V(val, ITP_F_NSMIX_START_CROP_Y, crop_y);
	ITP_SET_R(base, set_id, ITP_R_NSMIX_START_CROP, val);

	scaling_x = (config->sensor_binning_x * config->bcrop_width * binning_factor_x1_256) /
			config->taasc_width / 1000;
	scaling_y = (config->sensor_binning_y * config->bcrop_height * binning_factor_x1_256) /
			config->height / 1000;

	val = 0;
	val = ITP_SET_V(val, ITP_F_NSMIX_STEP_X, scaling_x);
	val = ITP_SET_V(val, ITP_F_NSMIX_STEP_Y, scaling_y);
	ITP_SET_R(base, set_id, ITP_R_NSMIX_STEP, val);

	/* solution guide */
	ITP_SET_R(base, set_id, ITP_R_NSMIX_STRIP, 0);

	/* STK */
	ITP_SET_R(base, set_id, ITP_R_STK_X_IMAGE_SIZE, config->sensor_calibrated_width);
	ITP_SET_R(base, set_id, ITP_R_STK_Y_IMAGE_SIZE, config->sensor_calibrated_height);

	scaling_x = (config->sensor_binning_x * config->bcrop_width * binning_factor_x1_1024) /
			config->taasc_width / 1000;
	scaling_y = (config->sensor_binning_y * config->bcrop_height * binning_factor_x1_1024) /
			config->height  / 1000;

	ITP_SET_R(base, set_id, ITP_R_STK_XBIN, scaling_x);
	ITP_SET_R(base, set_id, ITP_R_STK_YBIN, scaling_y);

	ITP_SET_R(base, set_id, ITP_R_STK_CROPX, crop_x);
	ITP_SET_R(base, set_id, ITP_R_STK_CROPY, crop_y);

	/* SNADD */
	val = 0;
	val = ITP_SET_V(val, ITP_F_SNADD_STRIP_START_POSITION, config->dma_crop_x);
	ITP_SET_R(base, set_id, ITP_R_SNADD_STRIP_PARAMS, val);

	/* YUV CROP*/
	val = 0;
	val = ITP_SET_V(val, ITP_F_IP_YUV_CROP_BYPASS, 0);
	val = ITP_SET_V(val, ITP_F_IP_YUV_CROP_START_X, 0);
	val = ITP_SET_V(val, ITP_F_IP_YUV_CROP_SIZE_X, config->width);
	ITP_SET_R(base, set_id, ITP_R_IP_YUV_CROP_CTRL, val);

	return 0;
}

int itp_hw_s_module_format(void __iomem *base, u32 set_id, u32 bayer_order)
{
	ITP_SET_R(base, set_id, ITP_R_DMSC_PIXEL_ORDER, bayer_order);
	ITP_SET_R(base, set_id, ITP_R_SHARP_PIXEL_ORDER, bayer_order);
	ITP_SET_R(base, set_id, ITP_R_NFLTR_PIXEL_ORDER, bayer_order);

	return 0;
}

void itp_hw_s_crc(void __iomem *base, u32 seed)
{
	ITP_SET_F_DIRECT(base, ITP_R_RGB2YUV_STREAM_CRC, ITP_F_RGB2YUV_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_GAMMA_RGB_STREAM_CRC, ITP_F_GAMMA_RGB_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_PRC_STREAM_CRC, ITP_F_PRC_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_STK_STREAM_CRC, ITP_F_STK_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_CCM9_STREAM_CRC, ITP_F_CCM9_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_DMSC_STREAM_CRC, ITP_F_DMSC_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_SNADD_STREAM_CRC, ITP_F_SNADD_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_NSMIX_STREAM_CRC, ITP_F_NSMIX_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_NFLTR_STREAM_CRC, ITP_F_NFLTR_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_SHARP_STREAM_CRC, ITP_F_SHARP_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_DITHER_STREAM_CRC, ITP_F_DITHER_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_DRCTMC_STREAM_CRC, ITP_F_DRCTMC_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_EDGEDET_STREAM_CRC, ITP_F_EDGEDET_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_SKIND_STREAM_CRC, ITP_F_SKIND_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_GAMMA_POST_STREAM_CRC, ITP_F_GAMMA_POST_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_YUV444TO422_STREAM_CRC, ITP_F_YUV444TO422_ISP_CRC_SEED, seed);
	ITP_SET_F_DIRECT(base, ITP_R_GAMMA_OETF_STREAM_CRC, ITP_F_GAMMA_OETF_CRC_SEED, seed);
}

void itp_hw_dump(void __iomem *base)
{
	info_hw("[ITP] SFR DUMP (v1.20)\n");
	is_hw_dump_regs(base, itp_regs, ITP_REG_CNT);
}

u32 itp_hw_g_reg_cnt(void)
{
	return ITP_REG_CNT + ITP_LUT_REG_CNT;
}
