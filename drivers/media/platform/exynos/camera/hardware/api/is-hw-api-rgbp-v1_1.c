// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * rgbp HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-rgbp-v1_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-rgbp-v1_1.h"

#define HBLANK_CYCLE			0x2D
#define RGBP_LUT_REG_CNT		1650

#define RGBP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &rgbp_regs[R], &rgbp_fields[F], val)
#define RGBP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &rgbp_regs[R], &rgbp_fields[F], val)
#define RGBP_SET_R(base, R, val) \
	is_hw_set_reg(base, &rgbp_regs[R], val)
#define RGBP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &rgbp_regs[R], val)
#define RGBP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &rgbp_fields[F], val)

#define RGBP_GET_F(base, R, F) \
	is_hw_get_field(base, &rgbp_regs[R], &rgbp_fields[F])
#define RGBP_GET_R(base, R) \
	is_hw_get_reg(base, &rgbp_regs[R])
#define RGBP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &rgbp_regs[R])
#define RGBP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &rgbp_fields[F])

static u32 stride_even;
const struct rgbp_v_coef rgbp_v_coef_4tap[7] = {
	/* x8/8 */
	{
		{   0,  -60, -100, -124, -132, -132, -124, -108,  -92},
		{2048, 2032, 1980, 1892, 1772, 1632, 1468, 1296, 1116},
		{   0,   80,  180,  300,  440,  592,  760,  936, 1116},
		{   0,   -4,  -12,  -20,  -32,  -44,  -56,  -76,  -92},
	},
	/* x7/8 */
	{
		{ 128,   68,   12,  -28,  -56,  -72,  -80,  -80,  -76},
		{1792, 1784, 1748, 1684, 1596, 1492, 1372, 1240, 1100},
		{ 128,  220,  316,  428,  552,  680,  816,  960, 1100},
		{   0,  -24,  -28,  -36,  -44,  -52,  -60,  -72,  -76},
	},
	/* x6/8 */
	{
		{ 244,  184,  124,   76,   36,    8,  -12,  -28,  -36},
		{1560, 1560, 1532, 1484, 1424, 1348, 1260, 1164, 1060},
		{ 244,  332,  424,  520,  624,  732,  840,  952, 1060},
		{   0,  -28,  -32,  -32,  -36,  -40,  -40,  -40,  -36},
	},
	/* x5/8 */
	{
		{ 340,  284,  224,  172,  128,   92,   64,   36,   20},
		{1364, 1364, 1344, 1312, 1268, 1216, 1152, 1084, 1004},
		{ 344,  420,  496,  580,  664,  748,  836,  924, 1004},
		{   0,  -20,  -16,  -16,  -12,   -8,   -4,    4,   20},
	},
	/* x4/8 */
	{
		{ 416,  356,  304,  252,  208,  168,  132,  104,  80},
		{1216, 1208, 1192, 1172, 1140, 1100, 1056, 1004, 944},
		{ 416,  480,  544,  612,  680,  752,  820,  884, 944},
		{   0,    4,    8,   12,   20,   28,   40,   56,  80},
	},
	/* x3/8 */
	{
		{ 472,  412,  360,  312,  268,  228,  192,  160,  132},
		{1104, 1092, 1080, 1064, 1040, 1012,  976,  936,  892},
		{ 472,  516,  572,  628,  684,  740,  796,  844,  892},
		{   0,   28,   36,   44,   56,   68,   84,  108,  132},
	},
	/* x2/8 */
	{
		{ 508,  444,  400,  352,  312,  272,  236,  200,  172},
		{1032, 1008, 1000,  988,  968,  948,  920,  888,  852},
		{ 508,  540,  588,  636,  684,  728,  772,  816,  852},
		{   0,   56,   60,   72,   84,  100,  120,  144,  172},
	}
};

const struct rgbp_h_coef rgbp_h_coef_8tap[7] = {
	/* x8/8 */
	{
		{   0,   -8,  -16,  -20,  -24,  -24,  -24,  -24,  -20},
		{   0,   32,   56,   80,   92,  100,  104,  100,   92},
		{   0, -100, -184, -248, -292, -320, -332, -328, -312},
		{2048, 2036, 1996, 1928, 1832, 1716, 1580, 1428, 1264},
		{   0,  120,  256,  404,  568,  740,  912, 1092, 1264},
		{   0,  -36,  -76, -120, -164, -212, -252, -284, -312},
		{   0,    8,   20,   32,   48,   60,   76,   84,   92},
		{   0,   -4,   -4,   -8,  -12,  -12,  -16,  -20,  -20},
	},
	/* x7/8 */
	{
		{  48,   26,   28,   20,   12,    8,    4,    0,   -4},
		{-128,  -96,  -64,  -36,  -12,    8,   28,   40,   52},
		{ 224,  116,   24,  -56, -120, -172, -212, -240, -260},
		{1776, 1780, 1752, 1704, 1640, 1560, 1460, 1352, 1236},
		{ 208,  328,  448,  576,  708,  844,  976, 1108, 1236},
		{-128, -156, -184, -208, -232, -252, -264, -264, -260},
		{  48,   52,   56,   60,   64,   64,   64,   60,   52},
		{   0,  -12,  -12,  -12,  -12,  -12,   -8,   -8,   -4},
	},
	/* x6/8 */
	{
		{  32,   36,   32,   32,   32,   28,   28,   20,   20},
		{-176, -160, -144, -128, -108,  -88,  -72,  -52,  -36},
		{ 400,  308,  228,  152,   80,   20,  -36,  -80, -120},
		{1536, 1528, 1508, 1476, 1432, 1376, 1316, 1240, 1160},
		{ 400,  492,  588,  684,  784,  884,  980, 1072, 1160},
		{-176, -188, -196, -196, -192, -188, -172, -148, -120},
		{  32,   32,   28,   20,   12,    4,   -8,  -20,  -36},
		{   0,    0,    4,    8,    8,   12,   12,   16,   20},
	},
	/* x5/8 */
	{
		{  12,  -12,  -4,     0,    4,    8,    8,   12,   12},
		{-124, -128, -132, -128, -124, -120, -112, -100,  -92},
		{ 520,  452,  388,  324,  264,  208,  152,  104,   60},
		{1280, 1276, 1260, 1244, 1216, 1184, 1144, 1096, 1044},
		{ 520,  588,  660,  728,  796,  864,  928,  988, 1044},
		{-124, -116, -104,  -88,  -68,  -44,  -12,   20,   60},
		{ -12,  -24,  -32,  -44,  -52,  -64,  -72,  -84,  -92},
		{   0,   12,   12,   12,   12,   12,   12,   12,   12},
	},
	/* x4/8 */
	{
		{ -44,  -40,  -36,  -32,  -28,  -24,  -20,  -20,  -16},
		{   0,  -16,  -28,  -40,  -48,  -56,  -60,  -64,  -68},
		{ 560,  516,  468,  424,  380,  340,  296,  256,  220},
		{1020, 1016, 1012, 1000,  984,  964,  944,  916,  888},
		{ 560,  604,  652,  696,  740,  780,  816,  856,  888},
		{   0,   20,   40,   64,   88,  116,  148,  184,  220},
		{ -48,  -52,  -56,  -60,  -64,  -64,  -68,  -68,  -68},
		{   0,    0,   -4,   -4,   -4,   -8,   -8,  -12,  -16},
	},
	/* x3/8 */
	{
		{ -20,  -20,  -20,  -20,  -20,  -20,  -20,  -20,  -20},
		{ 124,  108,   92,   76,   64,   48,   40,   28,   20},
		{ 532,  504,  476,  448,  420,  392,  364,  336,  312},
		{ 780,  780,  776,  772,  764,  756,  740,  728,  712},
		{ 532,  556,  584,  608,  632,  652,  676,  696,  712},
		{ 124,  148,  164,  188,  212,  236,  260,  284,  312},
		{ -24,  -16,  -12,   -8,   -8,    0,    4,   12,   20},
		{   0,  -12,  -12,  -16,  -16,  -16,  -16,  -16,  -20},
	},
	/* x2/8 */
	{
		{  40,   36,   28,   24,   20,   16,   16,   12,    8},
		{ 208,  192,  180,  164,  152,  140,  124,  116,  104},
		{ 472,  456,  440,  424,  408,  392,  376,  356,  340},
		{ 608,  608,  604,  600,  596,  592,  584,  580,  572},
		{ 472,  488,  500,  516,  528,  540,  552,  560,  572},
		{ 208,  224,  240,  256,  272,  288,  308,  324,  340},
		{  40,   44,   52,   60,   68,   76,   84,   92,  104},
		{   0,    0,    4,    4,    4,    4,    4,    8,    8},
	}
};

/*
 * pattern
 * smiadtp_test_pattern_mode - select the pattern:
 * 0 - no pattern (default)
 * 1 - solid color
 * 2 - 100% color bars
 * 3 - "fade to grey" over color bars
 * 4 - PN9
 * 5...255 - reserved
 * 256 - Macbeth color chart
 * 257 - PN12
 * 258...511 - reserved
 */
void rgbp_hw_s_dtp(void __iomem *base, u32 set_id, u32 pattern)
{
	RGBP_SET_R(base, RGBP_R_BYR_DTP_TEST_PATTERN_MODE, pattern);
}

void rgbp_hw_s_path(void __iomem *base, u32 set_id, struct is_rgbp_config *config)
{
	struct is_rgbp_chain_set chain_set;

	/* TODO : set path from DDK */
	chain_set.mux_luma_sel = 0x0;
	chain_set.demux_dmsc_en = 0x3;
	chain_set.demux_yuvsc_en = 0x3;
	chain_set.satflg_en = 0x0;

	/* RGBP_SET_F(base  , RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_IN_FOR_PATH_0, 1); */
	/* RGBP_SET_F(base  , RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_OUT_FOR_PATH_0, 1); */
	/* TODO : modify reg set for EVT1 */
	RGBP_SET_F(base, RGBP_R_CHAIN_MUX_SELECT, RGBP_F_MUX_LUMASHADING_SELECT, chain_set.mux_luma_sel);
	RGBP_SET_F(base, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_DMSC_ENABLE, chain_set.demux_dmsc_en);
	RGBP_SET_F(base, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_YUVSC_ENABLE, chain_set.demux_yuvsc_en);
	RGBP_SET_F(base, RGBP_R_CHAIN_SATFLAG_ENABLE, RGBP_F_CHAIN_SATFLAG_ENABLE, chain_set.satflg_en);
}

unsigned int rgbp_hw_is_occurred0(unsigned int status, enum rgbp_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_RGBP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_RGBP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_RGBP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_RGBP_COREX_END_INT_1;
		break;
	case INTR_ERR0:
		mask = RGBP_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

unsigned int rgbp_hw_is_occurred1(unsigned int status, enum rgbp_event_type1 type)
{
	u32 mask;

	switch (type) {
	case INTR_ERR1:
		mask = RGBP_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_is_occurred1);

int rgbp_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;

	RGBP_SET_R(base, RGBP_R_SW_RESET, 0x1);

	while (RGBP_GET_R(base, RGBP_R_SW_RESET)) {
		if (reset_count > RGBP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	return 0;
}

void rgbp_hw_s_init(void __iomem *base)
{
	RGBP_SET_F(base, RGBP_R_IP_PROCESSING, RGBP_F_IP_PROCESSING, 1);
	/* Interrupt group enable for one frame */
	RGBP_SET_F(base, RGBP_R_CMDQ_QUE_CMD_L, RGBP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xFF);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	RGBP_SET_F(base, RGBP_R_CMDQ_QUE_CMD_M, RGBP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	RGBP_SET_R(base, RGBP_R_CMDQ_ENABLE, 1);
}

int rgbp_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int0_all;
	u32 int1_all;
	u32 try_cnt = 0;

	idle = RGBP_GET_F(base, RGBP_R_IDLENESS_STATUS, RGBP_F_IDLENESS_STATUS);
	int0_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT0_STATUS);
	int1_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT1_STATUS);

	while (!idle) {
		idle = RGBP_GET_F(base, RGBP_R_IDLENESS_STATUS, RGBP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= RGBP_TRY_COUNT) {
			err_hw("[RGBP] timeout waiting idle - disable fail");
			rgbp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT0_STATUS);
	int1_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT1_STATUS);

	return ret;
}

void rgbp_hw_dump(void __iomem *base)
{
	info_hw("[RGBP] SFR DUMP (v1.0)\n");
	is_hw_dump_regs(base, rgbp_regs, RGBP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_dump);

void rgbp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id,
	struct is_rgbp_config *config, struct rgbp_param_set *param_set)
{
	u32 pixel_order;
	u32 seed;

	pixel_order = param_set->otf_input.order;

	rgbp_hw_s_cin_fifo(base, set_id);
	rgbp_hw_s_common(base);
	rgbp_hw_s_int_mask(base);
	rgbp_hw_s_path(base, set_id, config);
	rgbp_hw_s_dtp(base, set_id, 0);
	rgbp_hw_s_pixel_order(base, set_id, pixel_order);
	rgbp_hw_s_secure_id(base, set_id);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		rgbp_hw_s_block_crc(base, seed);
}

void rgbp_hw_s_cin_fifo(void __iomem *base, u32 set_id)
{
	RGBP_SET_F(base, RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_IN_FOR_PATH_0, 1);
	/* RGBP_SET_R(base, RGBP_R_COUTFIFO_OUTPUT_COUNT_AT_STALL, val); */

	/* RGBP_SET_R(base  , RGBP_R_COUTFIFO_OUTPUT_T3_INTERVAL, HBLANK_CYCLE); */

	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_CONFIG,
			RGBP_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_DEBUG_EN, 1);
	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_AUTO_RECOVERY_EN, 0);

	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_INTERVALS, RGBP_F_BYR_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	RGBP_SET_F(base, RGBP_R_CHAIN_LBCTRL_HBLANK, RGBP_F_CHAIN_LBCTRL_HBLANK, HBLANK_CYCLE);
	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_ENABLE, RGBP_F_BYR_CINFIFO_ENABLE, 1);
}

void rgbp_hw_s_cout_fifo(void __iomem *base, u32 set_id, bool enable)
{
	RGBP_SET_F(base + GET_COREX_OFFSET(set_id), RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_OUT_FOR_PATH_0, 1);
	/* RGBP_SET_R(base, RGBP_R_COUTFIFO_OUTPUT_COUNT_AT_STALL, val); */

	/* RGBP_SET_R(base + GET_COREX_OFFSET(set_id), RGBP_R_COUTFIFO_OUTPUT_T3_INTERVAL, HBLANK_CYCLE); */

	RGBP_SET_F(base + GET_COREX_OFFSET(set_id),
			RGBP_R_YUV_COUTFIFO_INTERVALS,
			RGBP_F_YUV_COUTFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	RGBP_SET_F(base, RGBP_R_YUV_COUTFIFO_CONFIG, RGBP_F_YUV_COUTFIFO_DEBUG_EN, 1);
	RGBP_SET_F(base, RGBP_R_YUV_COUTFIFO_CONFIG, RGBP_F_YUV_COUTFIFO_BACK_STALL_EN, 1);
	RGBP_SET_F(base + GET_COREX_OFFSET(set_id),
			RGBP_R_YUV_COUTFIFO_ENABLE,
			RGBP_F_YUV_COUTFIFO_ENABLE, enable);
}

void rgbp_hw_s_common(void __iomem *base)
{
	RGBP_SET_R(base, RGBP_R_AUTO_IGNORE_INTERRUPT_ENABLE, 1);
}

void rgbp_hw_s_int_mask(void __iomem *base)
{
	/* RGBP_SET_F(base, RGBP_R_CONTINT_LEVEL_PULSE_N_SEL, RGBP_F_CONTINT_LEVEL_PULSE_N_SEL, INT_LEVEL); */
	RGBP_SET_R(base, RGBP_R_INT_REQ_INT0_ENABLE, RGBP_INT0_EN_MASK);
	RGBP_SET_R(base, RGBP_R_INT_REQ_INT1_ENABLE, RGBP_INT1_EN_MASK);
}

/*
 * Context: O
 * CR type: Shadow/Dual
 */
void rgbp_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	RGBP_SET_F(base + GET_COREX_OFFSET(set_id), RGBP_R_SECU_CTRL_SEQID, RGBP_F_SECU_CTRL_SEQID, 0);
}

void rgbp_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_dma_dump);

static int rgbp_hw_g_rgb_format(u32 rgb_format, u32 *converted_format)
{
	u32 ret = 0;

	switch (rgb_format) {
	case DMA_FMT_RGB_RGBA8888:
		*converted_format = RGBP_DMA_FMT_RGB_RGBA8888;
		break;
	case DMA_FMT_RGB_ABGR8888:
		*converted_format = RGBP_DMA_FMT_RGB_ABGR8888;
		break;
	case DMA_FMT_RGB_ARGB8888:
		*converted_format = RGBP_DMA_FMT_RGB_ARGB8888;
		break;
	case DMA_FMT_RGB_BGRA8888:
		*converted_format = RGBP_DMA_FMT_RGB_BGRA8888;
		break;
	case DMA_FMT_RGB_RGBA1010102:
		*converted_format = RGBP_DMA_FMT_RGB_RGBA1010102;
		break;
	case DMA_FMT_RGB_ABGR1010102:
		*converted_format = RGBP_DMA_FMT_RGB_ABGR1010102;
		break;
	case DMA_FMT_RGB_ARGB1010102:
		*converted_format = RGBP_DMA_FMT_RGB_ARGB1010102;
		break;
	case DMA_FMT_RGB_BGRA1010102:
		*converted_format = RGBP_DMA_FMT_RGB_BGRA1010102;
		break;
	/*
	 * case DMA_FMT_RGB_RGBA16161616:
	 *	*converted_format = RGBP_DMA_FMT_RGB_RGBA16161616;
	 *	break;
	 * case DMA_FMT_RGB_ABGR16161616:
	 *	*converted_format = RGBP_DMA_FMT_RGB_ABGR16161616;
	 *	break;
	 * case DMA_FMT_RGB_ARGB16161616:
	 *	*converted_format = RGBP_DMA_FMT_RGB_ARGB16161616;
	 *	break;
	 * case DMA_FMT_RGB_BGRA16161616:
	 *	*converted_format = RGBP_DMA_FMT_RGB_BGRA16161616;
	 *	break;
	 */
	default:
		err_hw("[RGBP] invalid rgb_format[%d]", rgb_format);
		return -EINVAL;
	}
	return ret;
}

static int rgbp_hw_s_rgb_rdma_format(void __iomem *base, u32 rgb_format)
{
	u32 ret = 0;
	u32 converted_format;

	ret = rgbp_hw_g_rgb_format(rgb_format, &converted_format);
	if (ret) {
		err_hw("[RGBP] fail to set rgb_rdma_format[%d]", rgb_format);
		return -EINVAL;
	}
	RGBP_SET_F(base, RGBP_R_RGB1P_FORMAT, RGBP_F_RDMA_RGB1P_FORMAT, converted_format);
	return ret;
}

u32 *rgbp_hw_g_input_dva(struct rgbp_param_set *param_set, u32 instance, u32 id,  u32 *cmd)
{
	u32 *input_dva = NULL;

	switch (id) {
	case RGBP_RDMA_CL:
		input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	case RGBP_RDMA_RGB_EVEN:
	case RGBP_RDMA_RGB_ODD:
		input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		break;
	}

	return input_dva;
}

u32 *rgbp_hw_g_output_dva(struct rgbp_param_set *param_set, struct is_param_region *param,
								u32 instance, u32 id, u32 *cmd)
{
	struct mcs_param *mcs_param;
	u32 *output_dva = NULL;

	mcs_param = &param->mcs;

	switch (id) {
	case RGBP_WDMA_HF:
		output_dva = param_set->output_dva_hf;
		if (mcs_param->output[MCSC_OUTPUT5].dma_cmd == DMA_OUTPUT_VOTF_ENABLE)
			*cmd = mcs_param->output[MCSC_OUTPUT5].dma_cmd;
		else
			*cmd = param_set->dma_output_hf.cmd;
		break;
	case RGBP_WDMA_SF:
		output_dva = param_set->output_dva_sf;
		*cmd = param_set->dma_output_sf.cmd;
		break;
	case RGBP_WDMA_Y:
	case RGBP_WDMA_UV:
		output_dva = param_set->output_dva_yuv;
		*cmd = param_set->dma_output_yuv.cmd;
		break;
	case RGBP_WDMA_RGB_EVEN:
	case RGBP_WDMA_RGB_ODD:
		output_dva = param_set->output_dva_rgb;
		*cmd = param_set->dma_output_rgb.cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		break;
	}

	return output_dva;
}

void rgbp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int rgbp_hw_s_rdma_init(struct is_hw_ip *hw_ip, struct is_common_dma *dma, struct rgbp_param_set *param_set,
	u32 enable, u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_input *dma_input;
	u32 comp_sbwc_en = 0, comp_64b_align = 1, lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32  width, height;
	u32 format, en_votf, bus_info;
	int ret;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case RGBP_RDMA_CL:
	case RGBP_RDMA_RGB_EVEN:
	case RGBP_RDMA_RGB_ODD:
		dma_input = &param_set->dma_input;
		width = dma_input->width;
		height = dma_input->height;
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	break;
	}

	en_votf = dma_input->v_otf_enable;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL;  /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true);
	if (comp_sbwc_en == 0)
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				width, 16, true);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
							comp_64b_align, lossy_byte32num,
							RGBP_COMP_BLOCK_WIDTH,
							RGBP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, RGBP_COMP_BLOCK_WIDTH, 16) : 0;
	} else {
		return SET_ERROR;
	}

	if (dma->id == RGBP_RDMA_RGB_EVEN)
		ret |= rgbp_hw_s_rgb_rdma_format(hw_ip->regs[REG_SETA], format);
	else if (dma->id == RGBP_RDMA_RGB_ODD)
		ret |= rgbp_hw_s_rgb_rdma_format(hw_ip->regs[REG_SETA], format);
	else
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + RGBP_COMP_BLOCK_HEIGHT - 1) / RGBP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	return ret;
}

int rgbp_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[RGBP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case RGBP_RDMA_CL:
		base_reg = base + rgbp_regs[RGBP_R_STAT_RDMACL_EN].sfr_offset;
		/* Bayer: 0 */
		available_bayer_format_map = 0x0 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "RGBP_RDMA_CL");
		break;
	case RGBP_RDMA_RGB_EVEN:
		base_reg = base + rgbp_regs[RGBP_R_STAT_RDMACL_EN].sfr_offset;
		/* is_rgbp_rgb_format: 0,1,2,3,4,5,6,8,9,10,11,12,13 */
		available_bayer_format_map = 0x3F3F;
		snprintf(name, PATH_MAX, "RGBP_RDMA_RGB_EVEN");
		break;
	case RGBP_RDMA_RGB_ODD:
		base_reg = base + rgbp_regs[RGBP_R_STAT_RDMACL_EN].sfr_offset;
		/* is_rgbp_rgb_format: 0,1,2,3,4,5,6,8,9,10,11,12,13 */
		available_bayer_format_map = 0x3F3F;
		snprintf(name, PATH_MAX, "RGBP_RDMA_RGB_ODD");
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		ret = SET_ERROR;
		goto err_dma_create;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, input_id, name, available_bayer_format_map, 0, 0, 0);

err_dma_create:
	__putname(name);

	return ret;
}

int rgbp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];

	switch (dma->id) {
	case RGBP_RDMA_CL:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_RDMA_RGB_EVEN:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_RDMA_RGB_ODD:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_rdma_addr);

static int rgbp_hw_s_rgb_wdma_format(void __iomem *base, u32 rgb_format)
{
	u32 ret;
	u32 converted_format;

	ret = rgbp_hw_g_rgb_format(rgb_format, &converted_format);
	if (ret) {
		err_hw("[RGBP] fail to set rgb_wdma_format[%d]", rgb_format);
		return -EINVAL;
	}
	RGBP_SET_F(base, RGBP_R_RGB1P_FORMAT, RGBP_F_WDMA_RGB1P_FORMAT, converted_format);
	return ret;
}

void rgbp_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int rgbp_hw_s_wdma_init(struct is_hw_ip *hw_ip, struct is_common_dma *dma, struct rgbp_param_set *param_set,
	u32 instance, u32 enable, u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_output *dma_output;
	u32 comp_sbwc_en = 0, comp_64b_align = 1, lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 format, en_votf, bus_info, dma_format;
	u32 comp_block_width = RGBP_COMP_BLOCK_WIDTH;
	u32 comp_block_height = RGBP_COMP_BLOCK_HEIGHT;
	int ret = SET_SUCCESS;
	struct is_param_region *param;
	struct mcs_param *mcs_param;
	struct param_mcs_output *mcs_output;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	param = &hw_ip->region[instance]->parameter;
	mcs_param = &param->mcs;
	mcs_output = &mcs_param->output[MCSC_OUTPUT5];

	switch (dma->id) {
	case RGBP_WDMA_HF:
		dma_output = &param_set->dma_output_hf;
		break;
	case RGBP_WDMA_SF:
		dma_output = &param_set->dma_output_sf;
		break;
	case RGBP_WDMA_RGB_EVEN:
	case RGBP_WDMA_RGB_ODD:
		dma_output = &param_set->dma_output_rgb;
		break;
	case RGBP_WDMA_Y:
	case RGBP_WDMA_UV:
		dma_output = &param_set->dma_output_yuv;
		lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U12;
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (dma->id == RGBP_WDMA_HF) {
		width = mcs_output->width;
		height = mcs_output->height;
		en_votf = enable;
		hwformat = mcs_output->dma_format;
		sbwc_type = mcs_output->sbwc_type;

		if (sbwc_type) {
			comp_block_width = MCSC_HF_COMP_BLOCK_WIDTH;
			comp_block_height = MCSC_HF_COMP_BLOCK_HEIGHT;
		}

		memory_bitwidth = mcs_output->dma_bitwidth;
		pixelsize = memory_bitwidth;
		bus_info = cache_hint << 4;  /* cache hint [6:4] */
	} else {
		width = dma_output->width;
		height = dma_output->height;
		en_votf = dma_output->v_otf_enable;
		hwformat = dma_output->format;
		sbwc_type = dma_output->sbwc_type;
		memory_bitwidth = dma_output->bitwidth;
		pixelsize = dma_output->msb + 1;
		bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL;  /* cache hint [6:4] */
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true);
	if (comp_sbwc_en == 0)
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				width, 16, true);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
							comp_64b_align, lossy_byte32num,
							comp_block_width, comp_block_height);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, comp_block_width, 16) : 0;
	} else {
		return SET_ERROR;
	}

	switch (dma->id) {
	case RGBP_WDMA_HF:
	case RGBP_WDMA_SF:
	case RGBP_WDMA_Y:
	case RGBP_WDMA_UV:
		dma_format = DMA_FMT_BAYER;
		format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
				true);
		break;
	case RGBP_WDMA_RGB_EVEN:
	case RGBP_WDMA_RGB_ODD:
		dma_format = DMA_FMT_RGB;
		/* FIXE ME */
		format = is_hw_dma_get_rgb_format(memory_bitwidth, 1, DMA_INOUT_ORDER_RGBA);
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (dma->id == RGBP_WDMA_RGB_EVEN) {
		ret |= rgbp_hw_s_rgb_wdma_format(hw_ip->regs[REG_SETA], format);
		if (memory_bitwidth == DMA_INOUT_BIT_WIDTH_32BIT)
			width = 16 * ((width + 3) / 4);
		else
			width = 16 * (width / 2);
		height = (height + 1) / 2;
		stride_1p = 2 * width;
		stride_even = stride_1p;
	} else if (dma->id == RGBP_WDMA_RGB_ODD) {
		ret |= rgbp_hw_s_rgb_wdma_format(hw_ip->regs[REG_SETA], format);
		if (memory_bitwidth == DMA_INOUT_BIT_WIDTH_32BIT)
			width = 16 * ((width + 3) / 4);
		else
			width = 16 * (width / 2);
		height = height / 2;
		stride_1p = 2 * width;
	} else {
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, dma_format);
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + comp_block_height - 1) / comp_block_height) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	return ret;
}

int rgbp_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[RGBP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case RGBP_WDMA_HF:
		base_reg = base + rgbp_regs[RGBP_R_STAT_WDMADECOMP_EN].sfr_offset;
		/* Bayer: 0,1,2 */
		available_bayer_format_map = 0x7 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "RGBP_WDMA_HF");
		break;
	case RGBP_WDMA_SF:
		base_reg = base + rgbp_regs[RGBP_R_STAT_WDMASATFLG_EN].sfr_offset;
		/* Bayer: 0 */
		available_bayer_format_map = 0x0 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "RGBP_WDMA_SF");
		break;
	case RGBP_WDMA_RGB_EVEN:
		base_reg = base + rgbp_regs[RGBP_R_RGB_WDMAREPRGBEVEN_EN].sfr_offset;
		/* is_rgbp_rgb_format: 0,1,2,3,4,5,6,8,9,10,11,12,13 */
		available_bayer_format_map = 0x3F3F;
		snprintf(name, PATH_MAX, "RGBP_WDMA_RGB_EVEN");
		break;
	case RGBP_WDMA_RGB_ODD:
		/* is_rgbp_rgb_format: 0,1,2,3,4,5,6,8,9,10,11,12,13 */
		base_reg = base + rgbp_regs[RGBP_R_RGB_WDMAREPRGBODD_EN].sfr_offset;
		available_bayer_format_map = 0x3F3F;
		snprintf(name, PATH_MAX, "RGBP_WDMA_RGB_ODD");
		break;
	case RGBP_WDMA_Y:
		base_reg = base + rgbp_regs[RGBP_R_YUV_WDMAY_EN].sfr_offset;
		/* Bayer: 0,1,2,4,5,6,8,9,10 */
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "RGBP_WDMA_Y");
		break;
	case RGBP_WDMA_UV:
		base_reg = base + rgbp_regs[RGBP_R_YUV_WDMAUV_EN].sfr_offset;
		/* Bayer: 0,1,2,4,5,6,8,9,10 */
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "RGBP_WDMA_UV");
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		ret = SET_ERROR;
		goto err_dma_create;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, input_id, name, available_bayer_format_map, 0, 0, 0);

err_dma_create:
	__putname(name);

	return ret;
}

int rgbp_hw_s_wdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case RGBP_WDMA_HF:
	case RGBP_WDMA_SF:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_WDMA_RGB_EVEN:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_WDMA_RGB_ODD:
		for (i = 0; i < num_buffers; i++) {
			address[i] = (dma_addr_t)*(addr + i);
			address[i] += stride_even / 2;
		}
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		info("set rgb wdma enable");
		break;
	case RGBP_WDMA_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_WDMA_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case RGBP_WDMA_HF:
		case RGBP_WDMA_Y:
		case RGBP_WDMA_UV:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_wdma_addr);

int rgbp_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type)
{
	switch (set_id) {
	case COREX_SET_A:
	case COREX_SET_B:
		RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_0, RGBP_F_COREX_UPDATE_TYPE_0, type);
		RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_1, RGBP_F_COREX_UPDATE_TYPE_1, type);
		break;
	case COREX_DIRECT:
		RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_0, RGBP_F_COREX_UPDATE_TYPE_0, COREX_IGNORE);
		RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_1, RGBP_F_COREX_UPDATE_TYPE_1, COREX_IGNORE);
		break;
	default:
		err_hw("[RGBP] invalid corex set_id");
		return -EINVAL;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_corex_update_type);

void rgbp_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	RGBP_SET_R(base, RGBP_R_CMDQ_ADD_TO_QUEUE_0, 1);
}

void rgbp_hw_s_corex_init(void __iomem *base, bool enable)
{
	u32 reset_count = 0;

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		/* TODO :  check COREX_UPDATE_MODE_0/1 to 1 */
		RGBP_SET_F(base, RGBP_R_COREX_UPDATE_MODE_0, RGBP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);

		rgbp_hw_wait_corex_idle(base);

		RGBP_SET_F(base, RGBP_R_COREX_ENABLE, RGBP_F_COREX_ENABLE, 0);

		info_hw("[RGBP] %s disable done\n", __func__);

		return;
	}

	/*
	 * Set Fixed Value
	 */
	RGBP_SET_F(base, RGBP_R_COREX_ENABLE, RGBP_F_COREX_ENABLE, 1);
	RGBP_SET_F(base, RGBP_R_COREX_RESET, RGBP_F_COREX_RESET, 1);

	while (RGBP_GET_R(base, RGBP_R_COREX_RESET)) {
		if (reset_count > RGBP_TRY_COUNT) {
			err_hw("[RGBP] fail to wait corex reset");
			break;
		}
		reset_count++;
	}

	/*
	 * Type selection
	 * Only type0 will be used.
	 */
	RGBP_SET_F(base, RGBP_R_COREX_TYPE_WRITE_TRIGGER, RGBP_F_COREX_TYPE_WRITE_TRIGGER, 1);
	RGBP_SET_F(base, RGBP_R_COREX_TYPE_WRITE, RGBP_F_COREX_TYPE_WRITE, 0);

	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_0, RGBP_F_COREX_UPDATE_TYPE_0, COREX_COPY);
	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_TYPE_1, RGBP_F_COREX_UPDATE_TYPE_1, COREX_IGNORE);
	// 1st frame uses SW Trigger, others use H/W Trigger
	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_MODE_0, RGBP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_MODE_1, RGBP_F_COREX_UPDATE_MODE_1, SW_TRIGGER);

	RGBP_SET_R(base, RGBP_R_COREX_COPY_FROM_IP_0, 1);
	RGBP_SET_R(base, RGBP_R_COREX_COPY_FROM_IP_1, 1);

	/*
	 * Check COREX idleness, again.
	 */
	rgbp_hw_wait_corex_idle(base);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_corex_init);

void rgbp_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= RGBP_TRY_COUNT) {
			err_hw("[RGBP] fail to wait corex idle");
			break;
		}

		busy = RGBP_GET_F(base, RGBP_R_COREX_STATUS_0, RGBP_F_COREX_BUSY_0);
		dbg_hw(1, "[RGBP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

/*
 * Context: O
 * CR type: No Corex
 */
void rgbp_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;

	/*
	 * Set Fixed Value
	 *
	 * Type0 only:
	 * @RGBP_R_COREX_START_0 - 1: puls generation
	 * @RGBP_R_COREX_UPDATE_MODE_0 - 0: HW trigger, 1: SW tirgger
	 *
	 * SW trigger should be needed before stream on
	 * because there is no HW trigger before stream on.
	 */
	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_MODE_0, RGBP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	RGBP_SET_F(base, RGBP_R_COREX_START_0, RGBP_F_COREX_START_0, 0x1);

	rgbp_hw_wait_corex_idle(base);

	RGBP_SET_F(base, RGBP_R_COREX_UPDATE_MODE_0, RGBP_F_COREX_UPDATE_MODE_0, HW_TRIGGER);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_corex_start);

unsigned int rgbp_hw_g_int0_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, /*src_fro,*/ src_err;

	/*
	 * src_all: per-frame based rgbp IRQ status
	 * src_fro: FRO based rgbp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT0);

	if (clear)
		RGBP_SET_R(base, RGBP_R_INT_REQ_INT0_CLEAR, src_all);

	src_err = src_all & RGBP_INT0_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

unsigned int rgbp_hw_g_int0_mask(void __iomem *base)
{
	return RGBP_GET_R(base, RGBP_R_INT_REQ_INT0_ENABLE);
}

unsigned int rgbp_hw_g_int1_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, /*src_fro,*/ src_err;

	/*
	 * src_all: per-frame based rgbp IRQ status
	 * src_fro: FRO based rgbp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = RGBP_GET_R(base, RGBP_R_INT_REQ_INT1);

	if (clear)
		RGBP_SET_R(base, RGBP_R_INT_REQ_INT1_CLEAR, src_all);

	src_err = src_all & RGBP_INT1_ERR_MASK;

	*irq_state = src_all;

	src = src_all;
	return src | src_err;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_g_int1_state);

unsigned int rgbp_hw_g_int1_mask(void __iomem *base)
{
	return RGBP_GET_R(base, RGBP_R_INT_REQ_INT1_ENABLE);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_g_int1_mask);

void rgbp_hw_s_block_crc(void __iomem *base, u32 seed)
{
	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_STREAM_CRC, RGBP_F_BYR_CINFIFO_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_BYR_CINFIFO_STREAM_CRC, RGBP_F_BYR_CINFIFO_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_BYR_DTP_STREAM_CRC, RGBP_F_BYR_DTP_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_BYR_DNS_STREAM_CRC, RGBP_F_BYR_DNS_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_BYR_DMSC_STREAM_CRC, RGBP_F_BYR_DMSC_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_RGB_LUMASHADING_STREAM_CRC, RGBP_F_RGB_LUMASHADING_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_RGB_GAMMARGB_STREAM_CRC, RGBP_F_RGB_GAMMARGB_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_RGB_GTM_STREAM_CRC, RGBP_F_RGB_GTM_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_RGB_RGBTOYUV_STREAM_CRC, RGBP_F_RGB_RGBTOYUV_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_YUV_YUV444TO422_STREAM_CRC, RGBP_F_YUV_YUV444TO422_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_BYR_SATFLAG_CRC_RESULT, RGBP_F_BYR_SATFLAG_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_STREAM_CRC, RGBP_F_Y_DECOMP_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_RGB_CCM33_CRC, RGBP_F_RGB_CCM33_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_YUV_SC_STREAM_CRC, RGBP_F_YUV_SC_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_Y_GAMMALR_STREAM_CRC, RGBP_F_Y_GAMMALR_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_Y_UPSC_STREAM_CRC, RGBP_F_Y_UPSC_CRC_SEED, seed);
	RGBP_SET_F(base, RGBP_R_Y_GAMMAHR_STREAM_CRC, RGBP_F_Y_GAMMAHR_CRC_SEED, seed);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_block_crc);

void rgbp_hw_s_pixel_order(void __iomem *base, u32 set_id, u32 pixel_order)
{
	RGBP_SET_R(base + GET_COREX_OFFSET(set_id), RGBP_R_BYR_DTP_PIXEL_ORDER, pixel_order);
	RGBP_SET_R(base + GET_COREX_OFFSET(set_id), RGBP_R_BYR_DNS_PIXEL_ORDER, pixel_order);
	RGBP_SET_R(base + GET_COREX_OFFSET(set_id), RGBP_R_BYR_DMSC_PIXEL_ORDER, pixel_order);
	RGBP_SET_R(base + GET_COREX_OFFSET(set_id), RGBP_R_BYR_SATFLAG_PIXEL_ORDER, pixel_order);
}

void rgbp_hw_s_chain_src_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	RGBP_SET_F(base, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_WIDTH, width);
	RGBP_SET_F(base, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_HEIGHT, height);
}

void rgbp_hw_s_chain_dst_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	RGBP_SET_F(base, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_WIDTH, width);
	RGBP_SET_F(base, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_HEIGHT, height);
}

void rgbp_hw_s_dtp_output_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	RGBP_SET_F(base, RGBP_R_BYR_DTP_X_OUTPUT_SIZE, RGBP_F_BYR_DTP_X_OUTPUT_SIZE, width);
	RGBP_SET_F(base, RGBP_R_BYR_DTP_Y_OUTPUT_SIZE, RGBP_F_BYR_DTP_Y_OUTPUT_SIZE, height);
}

void rgbp_hw_s_decomp_frame_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_FRAME_SIZE, RGBP_F_Y_DECOMP_FRAME_WIDTH, width);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_FRAME_SIZE, RGBP_F_Y_DECOMP_FRAME_HEIGHT, height);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_decomp_frame_size);

void rgbp_hw_s_sc_dst_size_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_HSIZE, width);
	RGBP_SET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_VSIZE, height);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_sc_dst_size_size);

void rgbp_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	/* default : otf in/out setting */
	RGBP_SET_F(base, RGBP_R_BYR_DNS_BYPASS, RGBP_F_BYR_DNS_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_BYR_DMSC_BYPASS, RGBP_F_BYR_DMSC_BYPASS, 0);
	RGBP_SET_F(base, RGBP_R_RGB_LUMASHADING_BYPASS, RGBP_F_RGB_LUMASHADING_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_RGB_GAMMARGB_BYPASS, RGBP_F_RGB_GAMMARGB_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_RGB_GTM_BYPASS, RGBP_F_RGB_GTM_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_RGB_RGBTOYUV_BYPASS, RGBP_F_RGB_RGBTOYUV_BYPASS, 0);
	RGBP_SET_F(base, RGBP_R_YUV_YUV444TO422_ISP_BYPASS, RGBP_F_YUV_YUV444TO422_BYPASS, 0);

	RGBP_SET_F(base, RGBP_R_BYR_SATFLAG_BYPASS, RGBP_F_BYR_SATFLAG_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_BYPASS, RGBP_F_Y_DECOMP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_Y_GAMMALR_BYPASS, RGBP_F_Y_GAMMALR_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_Y_GAMMAHR_BYPASS, RGBP_F_Y_GAMMAHR_BYPASS, 1);

	RGBP_SET_F(base, RGBP_R_YUV_SC_CTRL0, RGBP_F_YUV_SC_TOP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_OTF_CROP_CTRL, RGBP_F_RGB_DMSCCROP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_DMA_CROP_CTRL, RGBP_F_STAT_DECOMPCROP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_DMA_CROP_CTRL, RGBP_F_STAT_SATFLGCROP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_DMA_CROP_CTRL, RGBP_F_YUV_SCCROP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_DMA_CROP_CTRL, RGBP_F_RGB_REPCROP_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_TETRA_TDMSC_BYPASS, RGBP_F_TETRA_TDMSC_BYPASS, 1);
	RGBP_SET_F(base, RGBP_R_RGB_CCM33_BYPASS, RGBP_F_RGB_CCM33_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_block_bypass);

void rgbp_hw_s_dns_size(void __iomem *base, u32 set_id,
	u32 width, u32 height, bool strip_enable, u32 strip_start_pos,
	struct rgbp_radial_cfg *radial_cfg, struct is_rgbp_config *rgbp_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;
	u32 biquad_scale_shift_adder;

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x * 1024ULL / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y * 1024ULL / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
			(radial_cfg->bns_binning_x * (radial_cfg->taa_crop_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
			(radial_cfg->bns_binning_y * radial_cfg->taa_crop_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	biquad_scale_shift_adder = rgbp_config->biquad_scale_shift_adder;

	if (biquad_scale_shift_adder >= 8) {
		if (biquad_scale_shift_adder < 10) {
			binning_x = shift_right_round(binning_x, 1);
			binning_y = shift_right_round(binning_y, 1);
			radial_center_x = shift_right_round(radial_center_x, 1);
			radial_center_y = shift_right_round(radial_center_y, 1);
			biquad_scale_shift_adder = biquad_scale_shift_adder - 2;
		} else {
			binning_x = shift_right_round(binning_x, 2);
			binning_y = shift_right_round(binning_y, 2);
			radial_center_x = shift_right_round(radial_center_x, 2);
			radial_center_y = shift_right_round(radial_center_y, 2);
			biquad_scale_shift_adder = biquad_scale_shift_adder - 4;
		}
	}

	RGBP_SET_F(base, RGBP_R_BYR_DNS_BIQUAD_SCALE_SHIFT_ADDER,
			RGBP_F_BYR_DNS_BIQUAD_SCALE_SHIFT_ADDER, biquad_scale_shift_adder);
	RGBP_SET_F(base, RGBP_R_BYR_DNS_BINNING, RGBP_F_BYR_DNS_BINNING_X, binning_x);
	RGBP_SET_F(base, RGBP_R_BYR_DNS_BINNING, RGBP_F_BYR_DNS_BINNING_Y, binning_y);
	RGBP_SET_F(base, RGBP_R_BYR_DNS_RADIAL_CENTER, RGBP_F_BYR_DNS_RADIAL_CENTER_X, radial_center_x);
	RGBP_SET_F(base, RGBP_R_BYR_DNS_RADIAL_CENTER, RGBP_F_BYR_DNS_RADIAL_CENTER_Y, radial_center_y);
}

u32 rgbp_hw_get_scaler_coef(u32 ratio)
{
	u32 coef;

	if (ratio <= RGBP_RATIO_X8_8)
		coef = RGBP_COEFF_X8_8;
	else if (ratio > RGBP_RATIO_X8_8 && ratio <= RGBP_RATIO_X7_8)
		coef = RGBP_COEFF_X7_8;
	else if (ratio > RGBP_RATIO_X7_8 && ratio <= RGBP_RATIO_X6_8)
		coef = RGBP_COEFF_X6_8;
	else if (ratio > RGBP_RATIO_X6_8 && ratio <= RGBP_RATIO_X5_8)
		coef = RGBP_COEFF_X5_8;
	else if (ratio > RGBP_RATIO_X5_8 && ratio <= RGBP_RATIO_X4_8)
		coef = RGBP_COEFF_X4_8;
	else if (ratio > RGBP_RATIO_X4_8 && ratio <= RGBP_RATIO_X3_8)
		coef = RGBP_COEFF_X3_8;
	else if (ratio > RGBP_RATIO_X3_8 && ratio <= RGBP_RATIO_X2_8)
		coef = RGBP_COEFF_X2_8;
	else
		coef = RGBP_COEFF_X2_8;

	return coef;
}

void rgbp_hw_s_yuvsc_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_CTRL0, RGBP_F_YUV_SC_TOP_BYPASS, bypass);
	RGBP_SET_F(base, RGBP_R_YUV_SC_CTRL0, RGBP_F_YUV_SC_BYPASS, bypass);
	RGBP_SET_F(base, RGBP_R_YUV_SC_CTRL1, RGBP_F_YUV_SC_LR_HR_MERGE_SPLIT_ON, 1);
}

void rgbp_hw_s_yuvsc_src_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	/* Register removed from v1.1 */
}

void is_scaler_get_yuvsc_src_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_yuvsc_src_size);

void rgbp_hw_s_yuvsc_input_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	/* Register removed from v1.1 */
}

void rgbp_hw_s_sc_input_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_sc_input_size);

void rgbp_hw_s_sc_src_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_sc_src_size);

void rgbp_hw_s_yuvsc_dst_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_HSIZE, h_size);
	RGBP_SET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_VSIZE, v_size);
}

void rgbp_hw_s_sc_output_crop_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_sc_output_crop_size);

void is_scaler_get_yuvsc_dst_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size)
{
	*h_size = RGBP_GET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_HSIZE);
	*v_size = RGBP_GET_F(base, RGBP_R_YUV_SC_DST_SIZE, RGBP_F_YUV_SC_DST_VSIZE);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_yuvsc_dst_size);

void rgbp_hw_s_yuvsc_out_crop_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
}

void rgbp_hw_s_yuvsc_scaling_ratio(void __iomem *base, u32 set_id, u32 hratio, u32 vratio)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_H_RATIO, RGBP_F_YUV_SC_H_RATIO, hratio);
	RGBP_SET_F(base, RGBP_R_YUV_SC_V_RATIO, RGBP_F_YUV_SC_V_RATIO, vratio);
}

void rgbp_hw_s_h_init_phase_offset(void __iomem *base, u32 set_id, u32 h_offset)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_H_INIT_PHASE_OFFSET, RGBP_F_YUV_SC_H_INIT_PHASE_OFFSET, h_offset);
}

void rgbp_hw_s_v_init_phase_offset(void __iomem *base, u32 set_id, u32 v_offset)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_V_INIT_PHASE_OFFSET, RGBP_F_YUV_SC_V_INIT_PHASE_OFFSET, v_offset);
}

void yuvsc_set_poly_scaler_h_coef(void __iomem *base, u32 set_id, u32 h_sel)
{
	int index;

	for (index = 0; index <= 8; index++) {
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_01 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_0 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_a[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_01 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_1 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_b[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_23 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_2 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_c[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_23 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_3 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_d[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_45 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_4 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_e[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_45 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_5 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_f[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_67 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_6 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_g[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_H_COEFF_0_67 + (4 * index), RGBP_F_YUV_SC_H_COEFF_0_7 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_h[index]);
	}
}

void yuvsc_set_poly_scaler_v_coef(void __iomem *base, u32 set_id, u32 v_sel)
{
	int index;

	for (index = 0; index <= 8; index++) {
		RGBP_SET_F(base, RGBP_R_YUV_SC_V_COEFF_0_01 + (2 * index), RGBP_F_YUV_SC_V_COEFF_0_0 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_a[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_V_COEFF_0_01 + (2 * index), RGBP_F_YUV_SC_V_COEFF_0_1 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_b[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_V_COEFF_0_23 + (2 * index), RGBP_F_YUV_SC_V_COEFF_0_2 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_c[index]);
		RGBP_SET_F(base, RGBP_R_YUV_SC_V_COEFF_0_23 + (2 * index), RGBP_F_YUV_SC_V_COEFF_0_3 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_d[index]);
	}
}

void rgbp_hw_s_yuvsc_coef(void __iomem *base, u32 set_id, u32 hratio, u32 vratio)
{
	u32 h_coef = 0, v_coef = 0;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;

	h_coef = rgbp_hw_get_scaler_coef(hratio);
	v_coef = rgbp_hw_get_scaler_coef(vratio);

	/* scale up case */
	if (vratio < RGBP_RATIO_X8_8)
		v_phase_offset = vratio >> 1;

	rgbp_hw_s_h_init_phase_offset(base, set_id, h_phase_offset);
	rgbp_hw_s_v_init_phase_offset(base, set_id, v_phase_offset);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_yuvsc_coef);

void rgbp_hw_s_yuvsc_round_mode(void __iomem *base, u32 set_id, u32 mode)
{
	RGBP_SET_F(base, RGBP_R_YUV_SC_ROUND_TH, RGBP_F_YUV_SC_ROUND_TH, mode);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_yuvsc_round_mode);

/* upsc */
void rgbp_hw_s_upsc_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass)
{
	RGBP_SET_F(base, RGBP_R_Y_UPSC_CTRL0, RGBP_F_Y_UPSC_ENABLE, enable);
	RGBP_SET_F(base, RGBP_R_Y_UPSC_CTRL0, RGBP_F_Y_UPSC_BYPASS, bypass);
}

void rgbp_hw_s_upsc_src_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_src_size);

void is_scaler_get_upsc_src_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_upsc_src_size);

void rgbp_hw_s_upsc_input_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_input_size);

void rgbp_hw_s_upsc_dst_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	RGBP_SET_F(base, RGBP_R_Y_UPSC_DST_SIZE, RGBP_F_Y_UPSC_DST_HSIZE, h_size);
	RGBP_SET_F(base, RGBP_R_Y_UPSC_DST_SIZE, RGBP_F_Y_UPSC_DST_VSIZE, v_size);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_dst_size);

void is_scaler_get_upsc_dst_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size)
{
	*h_size = RGBP_GET_F(base, RGBP_R_Y_UPSC_DST_SIZE, RGBP_F_Y_UPSC_DST_HSIZE);
	*v_size = RGBP_GET_F(base, RGBP_R_Y_UPSC_DST_SIZE, RGBP_F_Y_UPSC_DST_VSIZE);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_upsc_dst_size);

void rgbp_hw_s_upsc_out_crop_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	/* Register removed from v1.1 */
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_out_crop_size);

void rgbp_hw_s_ds_y_upsc_us(void __iomem *base, u32 set_id, u32 set)
{
	/* Register removed from v1.1 */
}

void rgbp_hw_s_upsc_scaling_ratio(void __iomem *base, u32 set_id, u32 hratio, u32 vratio)
{
	RGBP_SET_F(base, RGBP_R_Y_UPSC_H_RATIO, RGBP_F_Y_UPSC_H_RATIO, hratio);
	RGBP_SET_F(base, RGBP_R_Y_UPSC_V_RATIO, RGBP_F_Y_UPSC_V_RATIO, vratio);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_scaling_ratio);

void upsc_set_poly_scaler_h_coef(void __iomem *base, u32 set_id, u32 h_sel)
{
	int index;

	for (index = 0; index <= 8; index++) {
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_01 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_0 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_a[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_01 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_1 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_b[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_23 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_2 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_c[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_23 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_3 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_d[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_45 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_4 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_e[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_45 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_5 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_f[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_67 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_6 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_g[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_H_COEFF_0_67 + (4 * index), RGBP_F_Y_UPSC_H_COEFF_0_7 + (8 * index),
			rgbp_h_coef_8tap[h_sel].h_coef_h[index]);
	}
}

void upsc_set_poly_scaler_v_coef(void __iomem *base, u32 set_id, u32 v_sel)
{
	int index;

	for (index = 0; index <= 8; index++) {
		RGBP_SET_F(base, RGBP_R_Y_UPSC_V_COEFF_0_01 + (2 * index), RGBP_F_Y_UPSC_V_COEFF_0_0 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_a[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_V_COEFF_0_01 + (2 * index), RGBP_F_Y_UPSC_V_COEFF_0_1 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_b[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_V_COEFF_0_23 + (2 * index), RGBP_F_Y_UPSC_V_COEFF_0_2 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_c[index]);
		RGBP_SET_F(base, RGBP_R_Y_UPSC_V_COEFF_0_23 + (2 * index), RGBP_F_Y_UPSC_V_COEFF_0_3 + (4 * index),
			rgbp_v_coef_4tap[v_sel].v_coef_d[index]);
		}
}

void rgbp_hw_s_upsc_coef(void __iomem *base, u32 set_id, u32 hratio, u32 vratio)
{
	u32 h_coef, v_coef;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;

	h_coef = rgbp_hw_get_scaler_coef(hratio);
	v_coef = rgbp_hw_get_scaler_coef(vratio);

	/* TODO : scale up case */
	rgbp_hw_s_h_init_phase_offset(base, set_id, h_phase_offset);
	rgbp_hw_s_v_init_phase_offset(base, set_id, v_phase_offset);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_upsc_coef);

void rgbp_hw_s_gamma_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass)
{
	RGBP_SET_F(base, RGBP_R_Y_GAMMALR_BYPASS, RGBP_F_Y_GAMMALR_BYPASS, bypass);
	RGBP_SET_F(base, RGBP_R_Y_GAMMAHR_BYPASS, RGBP_F_Y_GAMMAHR_BYPASS, bypass);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_gamma_enable);

void rgbp_hw_s_decomp_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass)
{
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_BYPASS, RGBP_F_Y_DECOMP_BYPASS, bypass);
}

void rgbp_hw_s_decomp_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size)
{
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_FRAME_SIZE, RGBP_F_Y_DECOMP_FRAME_WIDTH, h_size);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_FRAME_SIZE, RGBP_F_Y_DECOMP_FRAME_HEIGHT, v_size);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_decomp_size);

void rgbp_hw_s_decomp_iq(void __iomem *base, u32 set_id)
{
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_RESIDUAL_WEIGHT, RGBP_F_Y_DECOMP_RESIDUAL_WEIGHT, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_FINE_WEIGHT, RGBP_F_Y_DECOMP_FINE_WEIGHT, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_MEDIUM_WEIGHT, RGBP_F_Y_DECOMP_MEDIUM_WEIGHT, 256);

	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN0_POS, RGBP_F_Y_DECOMP_CORING_GAIN0_POS, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN1_POS, RGBP_F_Y_DECOMP_CORING_GAIN1_POS, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN2_POS, RGBP_F_Y_DECOMP_CORING_GAIN2_POS, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN0_NEG, RGBP_F_Y_DECOMP_CORING_GAIN0_NEG, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN1_NEG, RGBP_F_Y_DECOMP_CORING_GAIN1_NEG, 256);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_GAIN2_NEG, RGBP_F_Y_DECOMP_CORING_GAIN2_NEG, 256);

	RGBP_SET_F(base, RGBP_R_Y_DECOMP_POST_GAIN, RGBP_F_Y_DECOMP_POST_GAIN, 256);

	RGBP_SET_F(base, RGBP_R_Y_DECOMP_CORING_THR1, RGBP_F_Y_DECOMP_CORING_THR1, 4095);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_LIMIT_PARAM, RGBP_F_Y_DECOMP_LIMIT_POS, 4095);
	RGBP_SET_F(base, RGBP_R_Y_DECOMP_LIMIT_PARAM, RGBP_F_Y_DECOMP_LIMIT_NEG, 4095);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_decomp_iq);

void rgbp_hw_s_grid_cfg(void __iomem *base, struct rgbp_grid_cfg *cfg)
{
	u32 val;

	val = 0;
	val = RGBP_SET_V(val, RGBP_F_RGB_LUMASHADING_BINNING_X, cfg->binning_x);
	val = RGBP_SET_V(val, RGBP_F_RGB_LUMASHADING_BINNING_Y, cfg->binning_y);
	RGBP_SET_R(base, RGBP_R_RGB_LUMASHADING_BINNING, val);

	RGBP_SET_F(base, RGBP_R_RGB_LUMASHADING_CROP_START_X,
			RGBP_F_RGB_LUMASHADING_CROP_START_X, cfg->crop_x);
	RGBP_SET_F(base, RGBP_R_RGB_LUMASHADING_CROP_START_Y,
			RGBP_F_RGB_LUMASHADING_CROP_START_Y, cfg->crop_y);

	val = 0;
	val = RGBP_SET_V(val, RGBP_F_RGB_LUMASHADING_CROP_RADIAL_X, cfg->crop_radial_x);
	val = RGBP_SET_V(val, RGBP_F_RGB_LUMASHADING_CROP_RADIAL_Y, cfg->crop_radial_y);
	RGBP_SET_R(base, RGBP_R_RGB_LUMASHADING_CROP_START_REAL, val);
}

void rgbp_hw_s_votf(void __iomem *base, u32 set_id, bool enable, bool stall)
{
	u32 val;

	val = stall << 1;
	val = val | enable;
	RGBP_SET_F(base, RGBP_R_STAT_WDMADECOMP_VOTF_EN, RGBP_F_STAT_WDMADECOMP_VOTF_EN, val);
}

void rgbp_hw_s_sbwc(void __iomem *base, u32 set_id, bool enable)
{
	RGBP_SET_F(base, RGBP_R_YUV_SBWC_CTRL, RGBP_F_YUV_SBWC_EN, enable);
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_s_sbwc);

void rgbp_hw_s_crop(void __iomem *base, int in_width, int in_height, int out_width, int out_height)
{
	RGBP_SET_F(base, RGBP_R_RGB_DMSCCROP_SIZE, RGBP_F_RGB_DMSCCROP_SIZE_X, in_width);
	RGBP_SET_F(base, RGBP_R_RGB_DMSCCROP_SIZE, RGBP_F_RGB_DMSCCROP_SIZE_Y, in_height);

	RGBP_SET_F(base, RGBP_R_RGB_REPCROP_SIZE, RGBP_F_RGB_REPCROP_SIZE_X, in_width);
	RGBP_SET_F(base, RGBP_R_RGB_REPCROP_SIZE, RGBP_F_RGB_REPCROP_SIZE_Y, in_height);

	RGBP_SET_F(base, RGBP_R_YUV_SCCROP_SIZE, RGBP_F_YUV_SCCROP_SIZE_X, out_width);
	RGBP_SET_F(base, RGBP_R_YUV_SCCROP_SIZE, RGBP_F_YUV_SCCROP_SIZE_Y, out_height);

	RGBP_SET_F(base, RGBP_R_STAT_SATFLGCROP_SIZE, RGBP_F_STAT_SATFLGCROP_SIZE_X, out_width);
	RGBP_SET_F(base, RGBP_R_STAT_SATFLGCROP_SIZE, RGBP_F_STAT_SATFLGCROP_SIZE_Y, out_height);

	RGBP_SET_F(base, RGBP_R_STAT_DECOMPCROP_SIZE, RGBP_F_STAT_DECOMPCROP_SIZE_X, out_width);
	RGBP_SET_F(base, RGBP_R_STAT_DECOMPCROP_SIZE, RGBP_F_STAT_DECOMPCROP_SIZE_Y, out_height);
}

u32 rgbp_hw_g_reg_cnt(void)
{
	return RGBP_REG_CNT + RGBP_LUT_REG_CNT;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_rgbp_hw_s_rgb_rdma_format(void __iomem *base, u32 rgb_format) {
	return rgbp_hw_s_rgb_rdma_format(base, rgb_format);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_rgbp_hw_s_rgb_rdma_format);

int pablo_kunit_rgbp_hw_s_rgb_wdma_format(void __iomem *base, u32 rgb_format) {
	return rgbp_hw_s_rgb_wdma_format(base, rgb_format);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_rgbp_hw_s_rgb_wdma_format);
#endif
