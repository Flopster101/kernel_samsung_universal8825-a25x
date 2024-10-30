// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MCSC HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-api-mcscaler-v3.h"
#if IS_ENABLED(CONFIG_MC_SCALER_V10_1)
#include "sfr/is-sfr-mcsc-v10_1.h"
#else
#include "sfr/is-sfr-mcsc-v10_0.h"
#endif
#include "is-hw.h"
#include "is-hw-control.h"
#include "is-param.h"
#include "is-hw-common-dma.h"

const struct mcsc_v_coef v_coef_4tap[7] = {
	/* x8/8 */
	{
		{  0, -15, -25, -31, -33, -33, -31, -27, -23},
		{512, 508, 495, 473, 443, 408, 367, 324, 279},
		{  0,  20,  45,  75, 110, 148, 190, 234, 279},
		{  0,  -1,  -3,  -5,  -8, -11, -14, -19, -23},
	},
	/* x7/8 */
	{
		{ 32,  17,   3,  -7, -14, -18, -20, -20, -19},
		{448, 446, 437, 421, 399, 373, 343, 310, 275},
		{ 32,  55,  79, 107, 138, 170, 204, 240, 275},
		{  0,  -6,  -7,  -9, -11, -13, -15, -18, -19},
	},
	/* x6/8 */
	{
		{ 61,  46,  31,  19,   9,   2,  -3,  -7,  -9},
		{390, 390, 383, 371, 356, 337, 315, 291, 265},
		{ 61,  83, 106, 130, 156, 183, 210, 238, 265},
		{  0,  -7,  -8,  -8,  -9, -10, -10, -10,  -9},
	},
	/* x5/8 */
	{
		{ 85,  71,  56,  43,  32,  23,  16,   9,   5},
		{341, 341, 336, 328, 317, 304, 288, 271, 251},
		{ 86, 105, 124, 145, 166, 187, 209, 231, 251},
		{  0,  -5,  -4,  -4,  -3,  -2,  -1,   1,   5},
	},
	/* x4/8 */
	{
		{104,  89,  76,  63,  52,  42,  33,  26,  20},
		{304, 302, 298, 293, 285, 275, 264, 251, 236},
		{104, 120, 136, 153, 170, 188, 205, 221, 236},
		{  0,   1,   2,   3,   5,   7,  10,  14,  20},
	},
	/* x3/8 */
	{
		{118, 103,  90,  78,  67,  57,  48,  40,  33},
		{276, 273, 270, 266, 260, 253, 244, 234, 223},
		{118, 129, 143, 157, 171, 185, 199, 211, 223},
		{  0,   7,   9,  11,  14,  17,  21,  27,  33},
	},
	/* x2/8 */
	{
		{127, 111, 100,  88,  78,  68,  59,  50,  43},
		{258, 252, 250, 247, 242, 237, 230, 222, 213},
		{127, 135, 147, 159, 171, 182, 193, 204, 213},
		{  0,  14,  15,  18,  21,  25,  30,  36,  43},
	}
};

const struct scaler_filter_h_coef_cfg h_coef_8tap[7] = {
	/* x8/8 */
	{
		{  0,  -2,  -4,  -5,  -6,  -6,  -6,  -6,  -5},
		{  0,   8,  14,  20,  23,  25,  26,  25,  23},
		{  0, -25, -46, -62, -73, -80, -83, -82, -78},
		{512, 509, 499, 482, 458, 429, 395, 357, 316},
		{  0,  30,  64, 101, 142, 185, 228, 273, 316},
		{  0,  -9, -19, -30, -41, -53, -63, -71, -78},
		{  0,   2,   5,   8,  12,  15,  19,  21,  23},
		{  0,  -1,  -1,  -2,  -3,  -3,  -4,  -5,  -5},
	},
	/* x7/8 */
	{
		{ 12,   9,   7,   5,   3,   2,   1,   0,  -1},
		{-32, -24, -16,  -9,  -3,   2,   7,  10,  13},
		{ 56,  29,   6, -14, -30, -43, -53, -60, -65},
		{444, 445, 438, 426, 410, 390, 365, 338, 309},
		{ 52,  82, 112, 144, 177, 211, 244, 277, 309},
		{-32, -39, -46, -52, -58, -63, -66, -66, -65},
		{ 12,  13,  14,  15,  16,  16,  16,  15,  13},
		{  0,  -3,	-3,  -3,  -3,  -3,  -2,  -2,  -1},
	},
	/* x6/8 */
	{
		{  8,   9,   8,   8,   8,   7,   7,   5,   5},
		{-44, -40, -36, -32, -27, -22, -18, -13,  -9},
		{100,  77,  57,  38,  20,   5,  -9, -20, -30},
		{384, 382, 377, 369, 358, 344, 329, 310, 290},
		{100, 123, 147, 171, 196, 221, 245, 268, 290},
		{-44, -47, -49, -49, -48, -47, -43, -37, -30},
		{  8,   8,   7,   5,   3,   1,  -2,  -5,  -9},
		{  0,   0,   1,   2,   2,   3,   3,   4,   5},
	},
	/* x5/8 */
	{
		{ -3,  -3,  -1,   0,   1,   2,   2,   3,   3},
		{-31, -32, -33, -32, -31, -30, -28, -25, -23},
		{130, 113,  97,  81,  66,  52,  38,  26,  15},
		{320, 319, 315, 311, 304, 296, 286, 274, 261},
		{130, 147, 165, 182, 199, 216, 232, 247, 261},
		{-31, -29, -26, -22, -17, -11,  -3,   5,  15},
		{ -3,  -6,  -8, -11, -13, -16, -18, -21, -23},
		{  0,   3,   3,   3,   3,   3,   3,   3,   3},
	},
	/* x4/8 */
	{
		{-11, -10,  -9,  -8,  -7,  -6,  -5,  -5,  -4},
		{  0,  -4,  -7, -10, -12, -14, -15, -16, -17},
		{140, 129, 117, 106,  95,  85,  74,  64,  55},
		{255, 254, 253, 250, 246, 241, 236, 229, 222},
		{140, 151, 163, 174, 185, 195, 204, 214, 222},
		{  0,   5,  10,  16,  22,  29,  37,  46,  55},
		{-12, -13, -14, -15, -16, -16, -17, -17, -17},
		{  0,   0,  -1,  -1,  -1,  -2,  -2,  -3,  -4},
	},
	/* x3/8 */
	{
		{ -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5},
		{ 31,  27,  23,  19,  16,  12,  10,   7,   5},
		{133, 126, 119, 112, 105,  98,  91,  84,  78},
		{195, 195, 194, 193, 191, 189, 185, 182, 178},
		{133, 139, 146, 152, 158, 163, 169, 174, 178},
		{ 31,  37,  41,  47,  53,  59,  65,  71,  78},
		{ -6,  -4,  -3,  -2,  -2,   0,   1,   3,   5},
		{  0,  -3,  -3,  -4,  -4,  -4,  -4,  -4,  -5},
	},
	/* x2/8 */
	{
		{ 10,   9,   7,   6,   5,   4,   4,   3,   2},
		{ 52,  48,  45,  41,  38,  35,  31,  29,  26},
		{118, 114, 110, 106, 102,  98,  94,  89,  85},
		{152, 152, 151, 150, 149, 148, 146, 145, 143},
		{118, 122, 125, 129, 132, 135, 138, 140, 143},
		{ 52,  56,  60,  64,  68,  72,  77,  81,  85},
		{ 10,  11,  13,  15,  17,  19,  21,  23,  26},
		{  0,   0,   1,   1,   1,   1,   1,   2,   2},
	}
};

static void is_scaler_add_to_queue(void __iomem *base_addr, u32 queue_num)
{
	switch (queue_num) {
	case MCSC_QUEUE0:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_CMDQ_ADD_TO_QUEUE_0],
			&mcsc_fields[MCSC_F_CMDQ_ADD_TO_QUEUE_0], 0x1);
		break;
	case MCSC_QUEUE1:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_CMDQ_ADD_TO_QUEUE_1],
			&mcsc_fields[MCSC_F_CMDQ_ADD_TO_QUEUE_1], 0x1);
		break;
	case MCSC_QUEUE_URGENT:
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_CMDQ_ADD_TO_QUEUE_URGENT],
			&mcsc_fields[MCSC_F_CMDQ_ADD_TO_QUEUE_URGENT], 0x1);
		break;
	default:
		warn_hw("invalid queue num(%d) for MCSC api\n", queue_num);
		break;
	}

}

static void is_scaler_set_mode(void __iomem *base_addr, u32 mode)
{
	u32 val;

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_CMDQ_QUE_CMD_H],
		&mcsc_fields[MCSC_F_CMDQ_QUE_CMD_BASE_ADDR], 0x0);

	val = 0;
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_HEADER_NUM], 0x0);
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_SETTING_MODE], mode);
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_HOLD_MODE], 0x0);
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_FRAME_ID], 0x0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_CMDQ_QUE_CMD_M], val);

	val = 0;
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE], 0xFF);
	val = is_hw_set_field_value(val, &mcsc_fields[MCSC_F_CMDQ_QUE_CMD_FRO_INDEX], 0x0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_CMDQ_QUE_CMD_L], val);
}

static void is_scaler_set_cinfifo_ctrl(void __iomem *base_addr)
{
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_IP_USE_OTF_PATH], 0x1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_IP_USE_CINFIFO_NEW_FRAME_IN],
		&mcsc_fields[MCSC_F_IP_USE_CINFIFO_NEW_FRAME_IN], 0x1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_CINFIFO_ENABLE],
		&mcsc_fields[MCSC_F_YUV_CINFIFO_ENABLE], 0x1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_CINFIFO_INTERVALS],
		&mcsc_fields[MCSC_F_YUV_CINFIFO_INTERVAL_HBLANK], HBLANK_CYCLE);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_CINFIFO_CONFIG], 0x1);
}

static void is_hw_set_crc(void __iomem *base_addr, u32 seed)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_CINFIFO_STREAM_CRC],
		&mcsc_fields[MCSC_F_YUV_CINFIFO_CRC_SEED], seed);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_CRC_EN],
		&mcsc_fields[MCSC_F_YUV_CRC_SEED], seed);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_CRC_EN],
		&mcsc_fields[MCSC_F_YUV_CRC_EN], 1);
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void pablo_kunit_scaler_hw_set_crc(void __iomem *base_addr, u32 seed)
{
	is_hw_set_crc(base_addr, seed);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_scaler_hw_set_crc);
#endif

void is_scaler_start(void __iomem *base_addr, u32 hw_id)
{
	u32 seed;

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		is_hw_set_crc(base_addr, seed);

	/* cmdq control : 3(apb direct) */
	is_scaler_set_mode(base_addr, 0x3);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_CMDQ_ENABLE],
		&mcsc_fields[MCSC_F_CMDQ_ENABLE], 0x1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_IP_PROCESSING],
		&mcsc_fields[MCSC_F_IP_PROCESSING], 0x1);

	is_scaler_add_to_queue(base_addr, MCSC_QUEUE0);
}

void is_scaler_stop(void __iomem *base_addr, u32 hw_id)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_IP_PROCESSING],
		&mcsc_fields[MCSC_F_IP_PROCESSING], 0);
}

u32 is_scaler_sw_reset(void __iomem *base_addr, u32 hw_id, u32 global, u32 partial)
{
	u32 reset_count = 0;

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SW_RESET],
		&mcsc_fields[MCSC_F_SW_RESET], 0x1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > MCSC_TRY_COUNT)
			return reset_count;
	} while (is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SW_RESET],
		&mcsc_fields[MCSC_F_SW_RESET]) != 0);

	return 0;
}

void is_scaler_clear_intr_all(void __iomem *base_addr, u32 hw_id)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_INT_REQ_INT0_CLEAR],
		INT1_MCSC_EN_MASK);
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_INT_REQ_INT1_CLEAR],
		INT2_MCSC_EN_MASK);
}

void is_scaler_disable_intr(void __iomem *base_addr, u32 hw_id)
{
	u32 reg_val = 0x0;

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_INT_REQ_INT0_ENABLE], reg_val);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_INT_REQ_INT1_ENABLE], reg_val);
}

void is_scaler_mask_intr(void __iomem *base_addr, u32 hw_id, u32 intr_mask)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_INT_REQ_INT0_ENABLE],
		intr_mask);
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_INT_REQ_INT1_ENABLE],
		INT2_MCSC_EN_MASK);
}

u32 is_scaler_get_mask_intr(void)
{
	return INT1_MCSC_EN_MASK;
}

unsigned int is_scaler_intr_occurred0(unsigned int status, enum mcsc_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR1_MCSC_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR1_MCSC_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR1_MCSC_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR1_MCSC_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = INT1_MCSC_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

void is_scaler_set_shadow_ctrl(void __iomem *base_addr, u32 hw_id, enum mcsc_shadow_ctrl ctrl)
{
	/* TODO */
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_shadow_ctrl);

void is_scaler_clear_shadow_ctrl(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
}

void is_scaler_get_input_status(void __iomem *base_addr, u32 hw_id, u32 *hl, u32 *vl)
{
	/* TODO */
	*hl = 0;
	*vl = 0;
}

void is_scaler_set_input_source(void __iomem *base_addr, u32 hw_id, u32 rdma)
{
	/* 0: No input , 1: OTF input, 2: RDMA input */
	switch (rdma) {
	case OTF_INPUT:
		is_scaler_set_cinfifo_ctrl(base_addr);
		break;
	case DMA_INPUT:
		err_hw("Not supported in this version, rdma (%d)", rdma);
		break;
	default:
		err_hw("No input source (%d)", rdma);
		break;
	}
}

u32 is_scaler_get_input_source(void __iomem *base_addr, u32 hw_id)
{
	/* not support */
	return 0;
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_input_source);

void is_scaler_set_input_img_size(void __iomem *base_addr, u32 hw_id, u32 width, u32 height)
{

	if (width % MCSC_WIDTH_ALIGN != 0) {
		err_hw("INPUT_IMG_HSIZE_%d(%d) should be multiple of 2", hw_id, width);
		width = ALIGN_DOWN(width, MCSC_WIDTH_ALIGN);
	}

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH],
		&mcsc_fields[MCSC_F_YUV_IN_IMG_SZ_WIDTH], width);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_IN_IMG_SZ_HEIGHT],
		&mcsc_fields[MCSC_F_YUV_IN_IMG_SZ_HEIGHT], height);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_input_img_size);

void is_scaler_get_input_img_size(void __iomem *base_addr, u32 hw_id, u32 *width, u32 *height)
{
	*width = is_hw_get_field(base_addr,
		&mcsc_regs[MCSC_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH],
		&mcsc_fields[MCSC_F_YUV_IN_IMG_SZ_WIDTH]);
	*height = is_hw_get_field(base_addr,
		&mcsc_regs[MCSC_R_YUV_MAIN_CTRL_IN_IMG_SZ_HEIGHT],
		&mcsc_fields[MCSC_F_YUV_IN_IMG_SZ_HEIGHT]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_input_img_size);

u32 is_scaler_get_scaler_path(void __iomem *base_addr, u32 hw_id, u32 output_id)
{
	u32 enable_poly, enable_post, enable_dma;
	u32 sc_ctrl_reg, pc_ctrl_reg, wdma_enable_reg;
	u32 sc_enable_field, pc_enable_field, wdma_en_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC0_ENABLE;
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC0_ENABLE;
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W0_ENABLE;
		wdma_en_field = MCSC_F_YUV_WDMASC_W0_EN;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC1_ENABLE;
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC1_ENABLE;
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W1_ENABLE;
		wdma_en_field = MCSC_F_YUV_WDMASC_W1_EN;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC2_ENABLE;
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC2_ENABLE;
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W2_ENABLE;
		wdma_en_field = MCSC_F_YUV_WDMASC_W2_EN;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC3_ENABLE;
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W3_ENABLE;
		wdma_en_field = MCSC_F_YUV_WDMASC_W3_EN;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC4_ENABLE;
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W4_ENABLE;
		wdma_en_field = MCSC_F_YUV_WDMASC_W4_EN;
		break;
	default:
		return 0;
	}

	enable_poly = is_hw_get_field(base_addr,
		&mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_enable_field]);

	if (output_id <= MCSC_OUTPUT2) {
		enable_post = is_hw_get_field(base_addr,
			&mcsc_regs[pc_ctrl_reg],
			&mcsc_fields[pc_enable_field]);
	} else
		enable_post = 0;

	enable_dma = is_hw_get_field(base_addr,
		&mcsc_regs[wdma_enable_reg],
		&mcsc_fields[wdma_en_field]);

	dbg_hw(2, "[ID:%d]%s:[OUT:%d], en(poly:%d, post:%d), dma(%d)\n",
		hw_id, __func__, output_id, enable_poly, enable_post, enable_dma);

	return DEV_HW_MCSC0;
}

void is_scaler_set_poly_scaler_enable(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 enable)
{
	u32 sc_ctrl_reg, sc_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC0_ENABLE;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC1_ENABLE;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC2_ENABLE;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC3_ENABLE;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_enable_field = MCSC_F_YUV_POLY_SC4_ENABLE;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_enable_field], enable);
}

void is_scaler_set_poly_src_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val;
	u32 sc_src_hpos_field, sc_src_vpos_field;
	u32 sc_src_hsize_field, sc_src_vsize_field;
	u32 sc_src_pos_reg, sc_src_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_src_hpos_field = MCSC_F_YUV_POLY_SC0_SRC_HPOS;
		sc_src_vpos_field = MCSC_F_YUV_POLY_SC0_SRC_VPOS;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC0_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC0_SRC_VSIZE;
		sc_src_pos_reg = MCSC_R_YUV_POLY_SC0_SRC_POS;
		sc_src_size_reg = MCSC_R_YUV_POLY_SC0_SRC_SIZE;
		break;
	case MCSC_OUTPUT1:
		sc_src_hpos_field = MCSC_F_YUV_POLY_SC1_SRC_HPOS;
		sc_src_vpos_field = MCSC_F_YUV_POLY_SC1_SRC_VPOS;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC1_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC1_SRC_VSIZE;
		sc_src_pos_reg = MCSC_R_YUV_POLY_SC1_SRC_POS;
		sc_src_size_reg = MCSC_R_YUV_POLY_SC1_SRC_SIZE;
		break;
	case MCSC_OUTPUT2:
		sc_src_hpos_field = MCSC_F_YUV_POLY_SC2_SRC_HPOS;
		sc_src_vpos_field = MCSC_F_YUV_POLY_SC2_SRC_VPOS;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC2_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC2_SRC_VSIZE;
		sc_src_pos_reg = MCSC_R_YUV_POLY_SC2_SRC_POS;
		sc_src_size_reg = MCSC_R_YUV_POLY_SC2_SRC_SIZE;
		break;
	case MCSC_OUTPUT3:
		sc_src_hpos_field = MCSC_F_YUV_POLY_SC3_SRC_HPOS;
		sc_src_vpos_field = MCSC_F_YUV_POLY_SC3_SRC_VPOS;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC3_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC3_SRC_VSIZE;
		sc_src_pos_reg = MCSC_R_YUV_POLY_SC3_SRC_POS;
		sc_src_size_reg = MCSC_R_YUV_POLY_SC3_SRC_SIZE;
		break;
	case MCSC_OUTPUT4:
		sc_src_hpos_field = MCSC_F_YUV_POLY_SC4_SRC_HPOS;
		sc_src_vpos_field = MCSC_F_YUV_POLY_SC4_SRC_VPOS;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC4_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC4_SRC_VSIZE;
		sc_src_pos_reg = MCSC_R_YUV_POLY_SC4_SRC_POS;
		sc_src_size_reg = MCSC_R_YUV_POLY_SC4_SRC_SIZE;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_src_hpos_field], pos_x);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_src_vpos_field], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[sc_src_pos_reg], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_src_hsize_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_src_vsize_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[sc_src_size_reg], reg_val);
}

void is_scaler_get_poly_src_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 sc_src_size_reg;
	u32 sc_src_hsize_field, sc_src_vsize_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_src_size_reg = MCSC_R_YUV_POLY_SC0_SRC_SIZE;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC0_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC0_SRC_VSIZE;
		break;
	case MCSC_OUTPUT1:
		sc_src_size_reg = MCSC_R_YUV_POLY_SC1_SRC_SIZE;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC1_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC1_SRC_VSIZE;
		break;
	case MCSC_OUTPUT2:
		sc_src_size_reg = MCSC_R_YUV_POLY_SC2_SRC_SIZE;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC2_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC2_SRC_VSIZE;
		break;
	case MCSC_OUTPUT3:
		sc_src_size_reg = MCSC_R_YUV_POLY_SC3_SRC_SIZE;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC3_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC3_SRC_VSIZE;
		break;
	case MCSC_OUTPUT4:
		sc_src_size_reg = MCSC_R_YUV_POLY_SC4_SRC_SIZE;
		sc_src_hsize_field = MCSC_F_YUV_POLY_SC4_SRC_HSIZE;
		sc_src_vsize_field = MCSC_F_YUV_POLY_SC4_SRC_VSIZE;
		break;
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[sc_src_size_reg],
		&mcsc_fields[sc_src_hsize_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[sc_src_size_reg],
		&mcsc_fields[sc_src_vsize_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_poly_src_size);

void is_scaler_set_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val;
	u32 sc_dst_hsize_field, sc_dst_vsize_field;
	u32 sc_dst_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC0_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC0_DST_VSIZE;
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC0_DST_SIZE;
		break;
	case MCSC_OUTPUT1:
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC1_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC1_DST_VSIZE;
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC1_DST_SIZE;
		break;
	case MCSC_OUTPUT2:
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC2_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC2_DST_VSIZE;
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC2_DST_SIZE;
		break;
	case MCSC_OUTPUT3:
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC3_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC3_DST_VSIZE;
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC3_DST_SIZE;
		break;
	case MCSC_OUTPUT4:
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC4_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC4_DST_VSIZE;
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC4_DST_SIZE;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_dst_hsize_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_dst_vsize_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[sc_dst_size_reg], reg_val);
}

void is_scaler_get_poly_dst_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 sc_dst_size_reg;
	u32 sc_dst_hsize_field, sc_dst_vsize_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC0_DST_SIZE;
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC0_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC0_DST_VSIZE;
		break;
	case MCSC_OUTPUT1:
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC1_DST_SIZE;
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC1_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC1_DST_VSIZE;
		break;
	case MCSC_OUTPUT2:
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC2_DST_SIZE;
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC2_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC2_DST_VSIZE;
		break;
	case MCSC_OUTPUT3:
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC3_DST_SIZE;
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC3_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC3_DST_VSIZE;
		break;
	case MCSC_OUTPUT4:
		sc_dst_size_reg = MCSC_R_YUV_POLY_SC4_DST_SIZE;
		sc_dst_hsize_field = MCSC_F_YUV_POLY_SC4_DST_HSIZE;
		sc_dst_vsize_field = MCSC_F_YUV_POLY_SC4_DST_VSIZE;
		break;
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[sc_dst_size_reg],
		&mcsc_fields[sc_dst_hsize_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[sc_dst_size_reg],
		&mcsc_fields[sc_dst_vsize_field]);
}

void is_scaler_set_poly_scaling_ratio(void __iomem *base_addr, u32 output_id, u32 hratio, u32 vratio)
{
	u32 sc_h_ratio_reg, sc_v_ratio_reg;
	u32 sc_h_ratio_field, sc_v_ratio_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_h_ratio_reg = MCSC_R_YUV_POLY_SC0_H_RATIO;
		sc_v_ratio_reg = MCSC_R_YUV_POLY_SC0_V_RATIO;
		sc_h_ratio_field = MCSC_F_YUV_POLY_SC0_H_RATIO;
		sc_v_ratio_field = MCSC_F_YUV_POLY_SC0_V_RATIO;
		break;
	case MCSC_OUTPUT1:
		sc_h_ratio_reg = MCSC_R_YUV_POLY_SC1_H_RATIO;
		sc_v_ratio_reg = MCSC_R_YUV_POLY_SC1_V_RATIO;
		sc_h_ratio_field = MCSC_F_YUV_POLY_SC1_H_RATIO;
		sc_v_ratio_field = MCSC_F_YUV_POLY_SC1_V_RATIO;
		break;
	case MCSC_OUTPUT2:
		sc_h_ratio_reg = MCSC_R_YUV_POLY_SC2_H_RATIO;
		sc_v_ratio_reg = MCSC_R_YUV_POLY_SC2_V_RATIO;
		sc_h_ratio_field = MCSC_F_YUV_POLY_SC2_H_RATIO;
		sc_v_ratio_field = MCSC_F_YUV_POLY_SC2_V_RATIO;
		break;
	case MCSC_OUTPUT3:
		sc_h_ratio_reg = MCSC_R_YUV_POLY_SC3_H_RATIO;
		sc_v_ratio_reg = MCSC_R_YUV_POLY_SC3_V_RATIO;
		sc_h_ratio_field = MCSC_F_YUV_POLY_SC3_H_RATIO;
		sc_v_ratio_field = MCSC_F_YUV_POLY_SC3_V_RATIO;
		break;
	case MCSC_OUTPUT4:
		sc_h_ratio_reg = MCSC_R_YUV_POLY_SC4_H_RATIO;
		sc_v_ratio_reg = MCSC_R_YUV_POLY_SC4_V_RATIO;
		sc_h_ratio_field = MCSC_F_YUV_POLY_SC4_H_RATIO;
		sc_v_ratio_field = MCSC_F_YUV_POLY_SC4_V_RATIO;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_h_ratio_reg],
		&mcsc_fields[sc_h_ratio_field], hratio);
	is_hw_set_field(base_addr, &mcsc_regs[sc_v_ratio_reg],
		&mcsc_fields[sc_v_ratio_field], vratio);
}

static void is_scaler_set_h_init_phase_offset(void __iomem *base_addr,
	u32 output_id, u32 h_offset)
{
	u32 sc_h_init_phase_offset_reg, sc_h_init_phase_offset_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_h_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC0_H_INIT_PHASE_OFFSET;
		sc_h_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC0_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT1:
		sc_h_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC1_H_INIT_PHASE_OFFSET;
		sc_h_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC1_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT2:
		sc_h_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC2_H_INIT_PHASE_OFFSET;
		sc_h_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC2_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT3:
		sc_h_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC3_H_INIT_PHASE_OFFSET;
		sc_h_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC3_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT4:
		sc_h_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC4_H_INIT_PHASE_OFFSET;
		sc_h_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC4_H_INIT_PHASE_OFFSET;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr,
		&mcsc_regs[sc_h_init_phase_offset_reg],
		&mcsc_fields[sc_h_init_phase_offset_field], h_offset);
}

static void is_scaler_set_v_init_phase_offset(void __iomem *base_addr,
	u32 output_id, u32 v_offset)
{
	u32 sc_v_init_phase_offset_reg, sc_v_init_phase_offset_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_v_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC0_V_INIT_PHASE_OFFSET;
		sc_v_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC0_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT1:
		sc_v_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC1_V_INIT_PHASE_OFFSET;
		sc_v_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC1_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT2:
		sc_v_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC2_V_INIT_PHASE_OFFSET;
		sc_v_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC2_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT3:
		sc_v_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC3_V_INIT_PHASE_OFFSET;
		sc_v_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC3_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT4:
		sc_v_init_phase_offset_reg =
			MCSC_R_YUV_POLY_SC4_V_INIT_PHASE_OFFSET;
		sc_v_init_phase_offset_field =
			MCSC_F_YUV_POLY_SC4_V_INIT_PHASE_OFFSET;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr,
		&mcsc_regs[sc_v_init_phase_offset_reg],
		&mcsc_fields[sc_v_init_phase_offset_field], v_offset);
}

static void is_scaler_set_poly_scaler_h_coef(void __iomem *base_addr,
	u32 output_id, struct scaler_filter_h_coef_cfg *h_coef_cfg)
{
	int index;
	u32 reg_val;
	u32 sc_h_coeff_01_reg, sc_h_coeff_23_reg;
	u32 sc_h_coeff_45_reg, sc_h_coeff_67_reg;
	u32 sc_h_coeff_0246_field = MCSC_F_YUV_POLY_SC0_H_COEFF_0_0;
	u32 sc_h_coeff_1357_field = MCSC_F_YUV_POLY_SC0_H_COEFF_0_1;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_h_coeff_01_reg = MCSC_R_YUV_POLY_SC0_H_COEFF_0_01;
		sc_h_coeff_23_reg = MCSC_R_YUV_POLY_SC0_H_COEFF_0_23;
		sc_h_coeff_45_reg = MCSC_R_YUV_POLY_SC0_H_COEFF_0_45;
		sc_h_coeff_67_reg = MCSC_R_YUV_POLY_SC0_H_COEFF_0_67;
		break;
	case MCSC_OUTPUT1:
		sc_h_coeff_01_reg = MCSC_R_YUV_POLY_SC1_H_COEFF_0_01;
		sc_h_coeff_23_reg = MCSC_R_YUV_POLY_SC1_H_COEFF_0_23;
		sc_h_coeff_45_reg = MCSC_R_YUV_POLY_SC1_H_COEFF_0_45;
		sc_h_coeff_67_reg = MCSC_R_YUV_POLY_SC1_H_COEFF_0_67;
		break;
	case MCSC_OUTPUT2:
		sc_h_coeff_01_reg = MCSC_R_YUV_POLY_SC2_H_COEFF_0_01;
		sc_h_coeff_23_reg = MCSC_R_YUV_POLY_SC2_H_COEFF_0_23;
		sc_h_coeff_45_reg = MCSC_R_YUV_POLY_SC2_H_COEFF_0_45;
		sc_h_coeff_67_reg = MCSC_R_YUV_POLY_SC2_H_COEFF_0_67;
		break;
	case MCSC_OUTPUT3:
		sc_h_coeff_01_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_01;
		sc_h_coeff_23_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_23;
		sc_h_coeff_45_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_45;
		sc_h_coeff_67_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_67;
		break;
	case MCSC_OUTPUT4:
		sc_h_coeff_01_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_01;
		sc_h_coeff_23_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_23;
		sc_h_coeff_45_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_45;
		sc_h_coeff_67_reg = MCSC_R_YUV_POLY_SC3_H_COEFF_0_67;
		break;
	default:
		return;
	}

	for (index = 0; index <= 8; index++) {
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_0246_field + (8 * index)],
			h_coef_cfg->h_coef_a[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_1357_field + (8 * index)],
			h_coef_cfg->h_coef_b[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_h_coeff_01_reg + (4 * index)],
			reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_0246_field + (8 * index)],
			h_coef_cfg->h_coef_c[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_1357_field + (8 * index)],
			h_coef_cfg->h_coef_d[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_h_coeff_23_reg + (4 * index)],
			reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_0246_field + (8 * index)],
			h_coef_cfg->h_coef_e[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_1357_field + (8 * index)],
			h_coef_cfg->h_coef_f[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_h_coeff_45_reg + (4 * index)],
			reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_0246_field + (8 * index)],
			h_coef_cfg->h_coef_g[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_h_coeff_1357_field + (8 * index)],
			h_coef_cfg->h_coef_h[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_h_coeff_67_reg + (4 * index)],
			reg_val);
	}
}

static void is_scaler_set_poly_scaler_v_coef(void __iomem *base_addr,
	u32 output_id, struct scaler_filter_v_coef_cfg *v_coef_cfg)
{
	int index;
	u32 reg_val;
	u32 sc_v_coeff_01_reg, sc_v_coeff_23_reg;
	u32 sc_v_coeff_02_field = MCSC_F_YUV_POLY_SC0_V_COEFF_0_0;
	u32 sc_v_coeff_13_field = MCSC_F_YUV_POLY_SC0_V_COEFF_0_1;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_v_coeff_01_reg = MCSC_R_YUV_POLY_SC0_V_COEFF_0_01;
		sc_v_coeff_23_reg = MCSC_R_YUV_POLY_SC0_V_COEFF_0_23;
		break;
	case MCSC_OUTPUT1:
		sc_v_coeff_01_reg = MCSC_R_YUV_POLY_SC1_V_COEFF_0_01;
		sc_v_coeff_23_reg = MCSC_R_YUV_POLY_SC1_V_COEFF_0_23;
		break;
	case MCSC_OUTPUT2:
		sc_v_coeff_01_reg = MCSC_R_YUV_POLY_SC2_V_COEFF_0_01;
		sc_v_coeff_23_reg = MCSC_R_YUV_POLY_SC2_V_COEFF_0_23;
		break;
	case MCSC_OUTPUT3:
		sc_v_coeff_01_reg = MCSC_R_YUV_POLY_SC3_V_COEFF_0_01;
		sc_v_coeff_23_reg = MCSC_R_YUV_POLY_SC3_V_COEFF_0_23;
		break;
	case MCSC_OUTPUT4:
		sc_v_coeff_01_reg = MCSC_R_YUV_POLY_SC4_V_COEFF_0_01;
		sc_v_coeff_23_reg = MCSC_R_YUV_POLY_SC4_V_COEFF_0_23;
		break;
	default:
		return;
	}

	for (index = 0; index <= 8; index++) {
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_v_coeff_02_field + (4 * index)],
			v_coef_cfg->v_coef_a[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_v_coeff_13_field + (4 * index)],
			v_coef_cfg->v_coef_b[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_v_coeff_01_reg + (2 * index)],
			reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_v_coeff_02_field + (4 * index)],
			v_coef_cfg->v_coef_c[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[sc_v_coeff_13_field + (4 * index)],
			v_coef_cfg->v_coef_d[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[sc_v_coeff_23_reg + (2 * index)],
			reg_val);
	}
}

static u32 get_scaler_coef_ver2(u32 ratio, struct scaler_filter_coef_cfg *sc_coef)
{
	u32 coef;

	if (ratio <= RATIO_X8_8)
		coef = sc_coef->ratio_x8_8;
	else if (ratio > RATIO_X8_8 && ratio <= RATIO_X7_8)
		coef = sc_coef->ratio_x7_8;
	else if (ratio > RATIO_X7_8 && ratio <= RATIO_X6_8)
		coef = sc_coef->ratio_x6_8;
	else if (ratio > RATIO_X6_8 && ratio <= RATIO_X5_8)
		coef = sc_coef->ratio_x5_8;
	else if (ratio > RATIO_X5_8 && ratio <= RATIO_X4_8)
		coef = sc_coef->ratio_x4_8;
	else if (ratio > RATIO_X4_8 && ratio <= RATIO_X3_8)
		coef = sc_coef->ratio_x3_8;
	else if (ratio > RATIO_X3_8 && ratio <= RATIO_X2_8)
		coef = sc_coef->ratio_x2_8;
	else
		coef = sc_coef->ratio_x2_8;

	return coef;
}

void is_scaler_set_poly_scaler_coef(void __iomem *base_addr, u32 output_id,
	u32 hratio, u32 vratio, struct scaler_coef_cfg *sc_coef,
	enum exynos_sensor_position sensor_position)
{
	struct scaler_filter_h_coef_cfg *h_coef = NULL;
	struct scaler_filter_v_coef_cfg *v_coef = NULL;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;
	u32 h_coef_idx, v_coef_idx;

	if (sc_coef) {
		if (sc_coef->use_poly_sc_coef) {
			h_coef = &sc_coef->poly_sc_h_coef;
			v_coef = &sc_coef->poly_sc_v_coef;
		} else {
			h_coef_idx = get_scaler_coef_ver2(hratio, &sc_coef->poly_sc_coef[0]);
			v_coef_idx = get_scaler_coef_ver2(vratio, &sc_coef->poly_sc_coef[1]);
			h_coef = (struct scaler_filter_h_coef_cfg *)&h_coef_8tap[h_coef_idx];
			v_coef = (struct scaler_filter_v_coef_cfg *)&v_coef_4tap[v_coef_idx];
		}

		is_scaler_set_poly_scaler_h_coef(base_addr, output_id, h_coef);
		is_scaler_set_poly_scaler_v_coef(base_addr, output_id, v_coef);
	} else {
		err_hw("sc_coef is NULL");
	}

	/* scale up case */
	if (hratio < RATIO_X8_8)
		h_phase_offset = hratio >> 1;
	if (vratio < RATIO_X8_8)
		v_phase_offset = vratio >> 1;

	is_scaler_set_h_init_phase_offset(base_addr, output_id, h_phase_offset);
	is_scaler_set_v_init_phase_offset(base_addr, output_id, v_phase_offset);
}

void is_scaler_set_poly_round_mode(void __iomem *base_addr, u32 output_id, u32 mode)
{
	u32 sc_round_mode_reg, sc_round_mode_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_round_mode_reg = MCSC_R_YUV_POLY_SC0_ROUND_MODE;
		sc_round_mode_field = MCSC_F_YUV_POLY_SC0_ROUND_MODE;
		break;
	case MCSC_OUTPUT1:
		sc_round_mode_reg = MCSC_R_YUV_POLY_SC1_ROUND_MODE;
		sc_round_mode_field = MCSC_F_YUV_POLY_SC1_ROUND_MODE;
		break;
	case MCSC_OUTPUT2:
		sc_round_mode_reg = MCSC_R_YUV_POLY_SC2_ROUND_MODE;
		sc_round_mode_field = MCSC_F_YUV_POLY_SC2_ROUND_MODE;
		break;
	case MCSC_OUTPUT3:
		sc_round_mode_reg = MCSC_R_YUV_POLY_SC3_ROUND_MODE;
		sc_round_mode_field = MCSC_F_YUV_POLY_SC3_ROUND_MODE;
		break;
	case MCSC_OUTPUT4:
		sc_round_mode_reg = MCSC_R_YUV_POLY_SC4_ROUND_MODE;
		sc_round_mode_field = MCSC_F_YUV_POLY_SC4_ROUND_MODE;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_round_mode_reg],
		&mcsc_fields[sc_round_mode_field], mode);
}

void is_scaler_set_post_scaler_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	u32 pc_ctrl_reg, pc_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC0_ENABLE;
		break;
	case MCSC_OUTPUT1:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC1_ENABLE;
		break;
	case MCSC_OUTPUT2:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_enable_field = MCSC_F_YUV_POST_PC2_ENABLE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_ctrl_reg],
		&mcsc_fields[pc_enable_field], enable);
}

void is_scaler_set_post_img_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val;
	u32 pc_img_hsize_field, pc_img_vsize_field;
	u32 pc_img_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC0_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC0_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC0_IMG_SIZE;
		break;
	case MCSC_OUTPUT1:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC1_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC1_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC1_IMG_SIZE;
		break;
	case MCSC_OUTPUT2:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC2_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC2_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC2_IMG_SIZE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_img_hsize_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_img_vsize_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_img_size_reg], reg_val);
}

void is_scaler_get_post_img_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 pc_img_hsize_field, pc_img_vsize_field;
	u32 pc_img_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC0_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC0_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC0_IMG_SIZE;
		break;
	case MCSC_OUTPUT1:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC1_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC1_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC1_IMG_SIZE;
		break;
	case MCSC_OUTPUT2:
		pc_img_hsize_field = MCSC_F_YUV_POST_PC2_IMG_HSIZE;
		pc_img_vsize_field = MCSC_F_YUV_POST_PC2_IMG_VSIZE;
		pc_img_size_reg = MCSC_R_YUV_POST_PC2_IMG_SIZE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[pc_img_size_reg],
		&mcsc_fields[pc_img_hsize_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[pc_img_size_reg],
		&mcsc_fields[pc_img_vsize_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_post_img_size);

void is_scaler_set_post_dst_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 reg_val;
	u32 pc_dst_hsize_field, pc_dst_vsize_field;
	u32 pc_dst_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC0_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC0_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC0_DST_SIZE;
		break;
	case MCSC_OUTPUT1:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC1_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC1_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC1_DST_SIZE;
		break;
	case MCSC_OUTPUT2:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC2_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC2_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC2_DST_SIZE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_dst_hsize_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_dst_vsize_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_dst_size_reg], reg_val);

}

void is_scaler_get_post_dst_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 pc_dst_hsize_field, pc_dst_vsize_field;
	u32 pc_dst_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC0_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC0_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC0_DST_SIZE;
		break;
	case MCSC_OUTPUT1:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC1_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC1_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC1_DST_SIZE;
		break;
	case MCSC_OUTPUT2:
		pc_dst_hsize_field = MCSC_F_YUV_POST_PC2_DST_HSIZE;
		pc_dst_vsize_field = MCSC_F_YUV_POST_PC2_DST_VSIZE;
		pc_dst_size_reg = MCSC_R_YUV_POST_PC2_DST_SIZE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[pc_dst_size_reg],
		&mcsc_fields[pc_dst_hsize_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[pc_dst_size_reg],
		&mcsc_fields[pc_dst_vsize_field]);
}

void is_scaler_set_post_scaling_ratio(void __iomem *base_addr, u32 output_id, u32 hratio, u32 vratio)
{
	u32 pc_h_ratio_field, pc_v_ratio_field;
	u32 pc_h_ratio_reg, pc_v_ratio_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_h_ratio_field = MCSC_F_YUV_POST_PC0_H_RATIO;
		pc_h_ratio_reg = MCSC_R_YUV_POST_PC0_H_RATIO;
		pc_v_ratio_field = MCSC_F_YUV_POST_PC0_V_RATIO;
		pc_v_ratio_reg = MCSC_R_YUV_POST_PC0_V_RATIO;
		break;
	case MCSC_OUTPUT1:
		pc_h_ratio_field = MCSC_F_YUV_POST_PC1_H_RATIO;
		pc_h_ratio_reg = MCSC_R_YUV_POST_PC1_H_RATIO;
		pc_v_ratio_field = MCSC_F_YUV_POST_PC1_V_RATIO;
		pc_v_ratio_reg = MCSC_R_YUV_POST_PC1_V_RATIO;
		break;
	case MCSC_OUTPUT2:
		pc_h_ratio_field = MCSC_F_YUV_POST_PC2_H_RATIO;
		pc_h_ratio_reg = MCSC_R_YUV_POST_PC2_H_RATIO;
		pc_v_ratio_field = MCSC_F_YUV_POST_PC2_V_RATIO;
		pc_v_ratio_reg = MCSC_R_YUV_POST_PC2_V_RATIO;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_h_ratio_reg],
		&mcsc_fields[pc_h_ratio_field], hratio);
	is_hw_set_field(base_addr, &mcsc_regs[pc_v_ratio_reg],
		&mcsc_fields[pc_v_ratio_field], vratio);
}


static void is_scaler_set_post_h_init_phase_offset(void __iomem *base_addr,
	u32 output_id, u32 h_offset)
{
	u32 pc_h_init_phase_offset_reg, pc_h_init_phase_offset_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_h_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC0_H_INIT_PHASE_OFFSET;
		pc_h_init_phase_offset_field =
			MCSC_F_YUV_POST_PC0_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT1:
		pc_h_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC1_H_INIT_PHASE_OFFSET;
		pc_h_init_phase_offset_field =
			MCSC_F_YUV_POST_PC1_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT2:
		pc_h_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC2_H_INIT_PHASE_OFFSET;
		pc_h_init_phase_offset_field =
			MCSC_F_YUV_POST_PC2_H_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_h_init_phase_offset_reg],
		&mcsc_fields[pc_h_init_phase_offset_field], h_offset);
}

static void is_scaler_set_post_v_init_phase_offset(void __iomem *base_addr,
	u32 output_id, u32 v_offset)
{
	u32 pc_v_init_phase_offset_reg, pc_v_init_phase_offset_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_v_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC0_V_INIT_PHASE_OFFSET;
		pc_v_init_phase_offset_field =
			MCSC_F_YUV_POST_PC0_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT1:
		pc_v_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC1_V_INIT_PHASE_OFFSET;
		pc_v_init_phase_offset_field =
			MCSC_F_YUV_POST_PC1_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT2:
		pc_v_init_phase_offset_reg =
			MCSC_R_YUV_POST_PC2_V_INIT_PHASE_OFFSET;
		pc_v_init_phase_offset_field =
			MCSC_F_YUV_POST_PC2_V_INIT_PHASE_OFFSET;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_v_init_phase_offset_reg],
		&mcsc_fields[pc_v_init_phase_offset_field], v_offset);
}

static void is_scaler_set_post_scaler_h_v_coef(void __iomem *base_addr,
	u32 output_id, u32 h_sel, u32 v_sel)
{
	u32 reg_val;
	u32 pc_h_coeff_sel_field, pc_v_coeff_sel_field;
	u32 pc_coeff_ctrl_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_h_coeff_sel_field = MCSC_F_YUV_POST_PC0_H_COEFF_SEL;
		pc_v_coeff_sel_field = MCSC_F_YUV_POST_PC0_V_COEFF_SEL;
		pc_coeff_ctrl_reg = MCSC_R_YUV_POST_PC0_COEFF_CTRL;
		break;
	case MCSC_OUTPUT1:
		pc_h_coeff_sel_field = MCSC_F_YUV_POST_PC1_H_COEFF_SEL;
		pc_v_coeff_sel_field = MCSC_F_YUV_POST_PC1_V_COEFF_SEL;
		pc_coeff_ctrl_reg = MCSC_R_YUV_POST_PC1_COEFF_CTRL;
		break;
	case MCSC_OUTPUT2:
		pc_h_coeff_sel_field = MCSC_F_YUV_POST_PC2_H_COEFF_SEL;
		pc_v_coeff_sel_field = MCSC_F_YUV_POST_PC2_V_COEFF_SEL;
		pc_coeff_ctrl_reg = MCSC_R_YUV_POST_PC2_COEFF_CTRL;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_h_coeff_sel_field], h_sel);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_v_coeff_sel_field], v_sel);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_coeff_ctrl_reg], reg_val);

}

void is_scaler_set_post_scaler_coef(void __iomem *base_addr, u32 output_id,
	u32 hratio, u32 vratio, struct scaler_coef_cfg *sc_coef)
{
	u32 h_coef = 0, v_coef = 0;
	/* this value equals 0 - scale-down operation */
	u32 h_phase_offset = 0, v_phase_offset = 0;

	if (sc_coef) {
		h_coef = get_scaler_coef_ver2(hratio, &sc_coef->post_sc_coef[0]);
		v_coef = get_scaler_coef_ver2(vratio, &sc_coef->post_sc_coef[1]);
	}

	/* scale up case */
	if (hratio < RATIO_X8_8)
		h_phase_offset = hratio >> 1;
	if (vratio < RATIO_X8_8)
		v_phase_offset = vratio >> 1;

	is_scaler_set_post_h_init_phase_offset(base_addr, output_id, h_phase_offset);
	is_scaler_set_post_v_init_phase_offset(base_addr, output_id, v_phase_offset);
	is_scaler_set_post_scaler_h_v_coef(base_addr, output_id, h_coef, v_coef);
}

void is_scaler_set_post_round_mode(void __iomem *base_addr, u32 output_id, u32 mode)
{
	u32 pc_round_mode_reg, pc_reound_mode_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_round_mode_reg = MCSC_R_YUV_POST_PC0_ROUND_MODE;
		pc_reound_mode_field = MCSC_F_YUV_POST_PC0_ROUND_MODE;
		break;
	case MCSC_OUTPUT1:
		pc_round_mode_reg = MCSC_R_YUV_POST_PC1_ROUND_MODE;
		pc_reound_mode_field = MCSC_F_YUV_POST_PC1_ROUND_MODE;
		break;
	case MCSC_OUTPUT2:
		pc_round_mode_reg = MCSC_R_YUV_POST_PC2_ROUND_MODE;
		pc_reound_mode_field = MCSC_F_YUV_POST_PC2_ROUND_MODE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_round_mode_reg],
		&mcsc_fields[pc_reound_mode_field], mode);
}

void is_scaler_set_420_conversion(void __iomem *base_addr, u32 output_id, u32 conv420_weight, u32 conv420_en)
{
	u32 reg_val;
	u32 pc_conv420_enable;
	u32 pc_conv420_ctrl_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_conv420_enable = MCSC_F_YUV_POST_PC0_CONV420_ENABLE;
		pc_conv420_ctrl_reg = MCSC_R_YUV_POST_PC0_CONV420_CTRL;
		break;
	case MCSC_OUTPUT1:
		pc_conv420_enable = MCSC_F_YUV_POST_PC1_CONV420_ENABLE;
		pc_conv420_ctrl_reg = MCSC_R_YUV_POST_PC1_CONV420_CTRL;
		break;
	case MCSC_OUTPUT2:
		pc_conv420_enable = MCSC_F_YUV_POST_PC2_CONV420_ENABLE;
		pc_conv420_ctrl_reg = MCSC_R_YUV_POST_PC2_CONV420_CTRL;
		break;
	case MCSC_OUTPUT3:
		pc_conv420_enable = MCSC_F_YUV_POST_PC3_CONV420_ENABLE;
		pc_conv420_ctrl_reg = MCSC_R_YUV_POST_PC3_CONV420_CTRL;
		break;
	case MCSC_OUTPUT4:
		pc_conv420_enable = MCSC_F_YUV_POST_PC4_CONV420_ENABLE;
		pc_conv420_ctrl_reg = MCSC_R_YUV_POST_PC4_CONV420_CTRL;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_conv420_enable], conv420_en);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_conv420_ctrl_reg], reg_val);

}

void is_scaler_set_bchs_enable(void __iomem *base_addr, u32 output_id, bool bchs_en)
{
	u32 pc_bchs_ctrl_reg, pc_bchs_enable_field;
	u32 pc_bchs_clamp_y_reg, pc_bchs_clamp_c_reg;
	u32 pc_bchs_clamp_y_field = 0x03FF0000;
	u32 pc_bchs_clamp_c_field = 0x03FF0000;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_bchs_ctrl_reg = MCSC_R_YUV_POST_PC0_BCHS_CTRL;
		pc_bchs_enable_field = MCSC_F_YUV_POST_PC0_BCHS_ENABLE;
		pc_bchs_clamp_y_reg = MCSC_R_YUV_POST_PC0_BCHS_CLAMP_Y;
		pc_bchs_clamp_c_reg = MCSC_R_YUV_POST_PC0_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT1:
		pc_bchs_ctrl_reg = MCSC_R_YUV_POST_PC1_BCHS_CTRL;
		pc_bchs_enable_field = MCSC_F_YUV_POST_PC1_BCHS_ENABLE;
		pc_bchs_clamp_y_reg = MCSC_R_YUV_POST_PC1_BCHS_CLAMP_Y;
		pc_bchs_clamp_c_reg = MCSC_R_YUV_POST_PC1_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT2:
		pc_bchs_ctrl_reg = MCSC_R_YUV_POST_PC2_BCHS_CTRL;
		pc_bchs_enable_field = MCSC_F_YUV_POST_PC2_BCHS_ENABLE;
		pc_bchs_clamp_y_reg = MCSC_R_YUV_POST_PC2_BCHS_CLAMP_Y;
		pc_bchs_clamp_c_reg = MCSC_R_YUV_POST_PC2_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT3:
		pc_bchs_ctrl_reg = MCSC_R_YUV_POST_PC3_BCHS_CTRL;
		pc_bchs_enable_field = MCSC_F_YUV_POST_PC3_BCHS_ENABLE;
		pc_bchs_clamp_y_reg = MCSC_R_YUV_POST_PC3_BCHS_CLAMP_Y;
		pc_bchs_clamp_c_reg = MCSC_R_YUV_POST_PC3_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT4:
		pc_bchs_ctrl_reg = MCSC_R_YUV_POST_PC4_BCHS_CTRL;
		pc_bchs_enable_field = MCSC_F_YUV_POST_PC4_BCHS_ENABLE;
		pc_bchs_clamp_y_reg = MCSC_R_YUV_POST_PC4_BCHS_CLAMP_Y;
		pc_bchs_clamp_c_reg = MCSC_R_YUV_POST_PC4_BCHS_CLAMP_C;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_bchs_ctrl_reg],
		&mcsc_fields[pc_bchs_enable_field], bchs_en);
	/* default BCHS clamp value */
	is_hw_set_reg(base_addr,
		&mcsc_regs[pc_bchs_clamp_y_reg], pc_bchs_clamp_y_field);
	is_hw_set_reg(base_addr,
		&mcsc_regs[pc_bchs_clamp_c_reg], pc_bchs_clamp_c_field);

}

/* brightness/contrast control */
void is_scaler_set_b_c(void __iomem *base_addr, u32 output_id, u32 y_offset, u32 y_gain)
{
	u32 reg_val;
	u32 pc_bchs_y_offset_field, pc_bchs_y_gain_field;
	u32 pc_bchs_bc_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_bchs_y_offset_field = MCSC_F_YUV_POST_PC0_BCHS_Y_OFFSET;
		pc_bchs_y_gain_field = MCSC_F_YUV_POST_PC0_BCHS_Y_GAIN;
		pc_bchs_bc_reg = MCSC_R_YUV_POST_PC0_BCHS_BC;
		break;
	case MCSC_OUTPUT1:
		pc_bchs_y_offset_field = MCSC_F_YUV_POST_PC1_BCHS_Y_OFFSET;
		pc_bchs_y_gain_field = MCSC_F_YUV_POST_PC1_BCHS_Y_GAIN;
		pc_bchs_bc_reg = MCSC_R_YUV_POST_PC1_BCHS_BC;
		break;
	case MCSC_OUTPUT2:
		pc_bchs_y_offset_field = MCSC_F_YUV_POST_PC2_BCHS_Y_OFFSET;
		pc_bchs_y_gain_field = MCSC_F_YUV_POST_PC2_BCHS_Y_GAIN;
		pc_bchs_bc_reg = MCSC_R_YUV_POST_PC2_BCHS_BC;
		break;
	case MCSC_OUTPUT3:
		pc_bchs_y_offset_field = MCSC_F_YUV_POST_PC3_BCHS_Y_OFFSET;
		pc_bchs_y_gain_field = MCSC_F_YUV_POST_PC3_BCHS_Y_GAIN;
		pc_bchs_bc_reg = MCSC_R_YUV_POST_PC3_BCHS_BC;
		break;
	case MCSC_OUTPUT4:
		pc_bchs_y_offset_field = MCSC_F_YUV_POST_PC4_BCHS_Y_OFFSET;
		pc_bchs_y_gain_field = MCSC_F_YUV_POST_PC4_BCHS_Y_GAIN;
		pc_bchs_bc_reg = MCSC_R_YUV_POST_PC4_BCHS_BC;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_y_offset_field], y_offset);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_y_gain_field], y_gain);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_bchs_bc_reg], reg_val);

}

/* hue/saturation control */
void is_scaler_set_h_s(void __iomem *base_addr, u32 output_id,
	u32 c_gain00, u32 c_gain01, u32 c_gain10, u32 c_gain11)
{
	u32 reg_val;
	u32 pc_bchs_hs1_reg, pc_bchs_hs2_reg;
	u32 pc_bchs_c_gain_0_field = MCSC_F_YUV_POST_PC0_BCHS_C_GAIN_00;
	u32 pc_bchs_c_gain_1_field = MCSC_F_YUV_POST_PC0_BCHS_C_GAIN_01;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_bchs_hs1_reg = MCSC_R_YUV_POST_PC0_BCHS_HS1;
		pc_bchs_hs2_reg = MCSC_R_YUV_POST_PC0_BCHS_HS2;
		break;
	case MCSC_OUTPUT1:
		pc_bchs_hs1_reg = MCSC_R_YUV_POST_PC1_BCHS_HS1;
		pc_bchs_hs2_reg = MCSC_R_YUV_POST_PC1_BCHS_HS2;
		break;
	case MCSC_OUTPUT2:
		pc_bchs_hs1_reg = MCSC_R_YUV_POST_PC2_BCHS_HS1;
		pc_bchs_hs2_reg = MCSC_R_YUV_POST_PC2_BCHS_HS2;
		break;
	case MCSC_OUTPUT3:
		pc_bchs_hs1_reg = MCSC_R_YUV_POST_PC3_BCHS_HS1;
		pc_bchs_hs2_reg = MCSC_R_YUV_POST_PC3_BCHS_HS2;
		break;
	case MCSC_OUTPUT4:
		pc_bchs_hs1_reg = MCSC_R_YUV_POST_PC4_BCHS_HS1;
		pc_bchs_hs2_reg = MCSC_R_YUV_POST_PC4_BCHS_HS2;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_c_gain_0_field], c_gain00);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_c_gain_1_field], c_gain01);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_bchs_hs1_reg], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_c_gain_0_field], c_gain10);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_bchs_c_gain_1_field], c_gain11);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_bchs_hs2_reg], reg_val);

}

void is_scaler_set_bchs_clamp(void __iomem *base_addr, u32 output_id,
	u32 y_max, u32 y_min, u32 c_max, u32 c_min)
{
	u32 reg_val_y, reg_idx_y;
	u32 reg_val_c, reg_idx_c;
	u32 pc_bchs_clamp_max_field =
		MCSC_F_YUV_POST_PC0_BCHS_Y_CLAMP_MAX;
	u32 pc_bchs_clamp_min_field =
		MCSC_F_YUV_POST_PC0_BCHS_Y_CLAMP_MIN;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg_idx_y = MCSC_R_YUV_POST_PC0_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_YUV_POST_PC0_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT1:
		reg_idx_y = MCSC_R_YUV_POST_PC1_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_YUV_POST_PC1_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT2:
		reg_idx_y = MCSC_R_YUV_POST_PC2_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_YUV_POST_PC2_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT3:
		reg_idx_y = MCSC_R_YUV_POST_PC3_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_YUV_POST_PC3_BCHS_CLAMP_C;
		break;
	case MCSC_OUTPUT4:
		reg_idx_y = MCSC_R_YUV_POST_PC4_BCHS_CLAMP_Y;
		reg_idx_c = MCSC_R_YUV_POST_PC4_BCHS_CLAMP_C;
		break;
	default:
		return;
	}

	reg_val_y = reg_val_c = 0;
	reg_val_y = is_hw_set_field_value(reg_val_y,
		&mcsc_fields[pc_bchs_clamp_max_field], y_max);
	reg_val_y = is_hw_set_field_value(reg_val_y,
		&mcsc_fields[pc_bchs_clamp_min_field], y_min);
	reg_val_c = is_hw_set_field_value(reg_val_c,
		&mcsc_fields[pc_bchs_clamp_max_field], c_max);
	reg_val_c = is_hw_set_field_value(reg_val_c,
		&mcsc_fields[pc_bchs_clamp_min_field], c_min);
	is_hw_set_reg(base_addr, &mcsc_regs[reg_idx_y], reg_val_y);
	is_hw_set_reg(base_addr, &mcsc_regs[reg_idx_c], reg_val_c);

	dbg_hw(2, "[OUT:%d]set_bchs_clamp: Y:[%#x]%#x, C:[%#x]%#x\n", output_id,
			mcsc_regs[reg_idx_y].sfr_offset, is_hw_get_reg(base_addr, &mcsc_regs[reg_idx_y]),
			mcsc_regs[reg_idx_c].sfr_offset, is_hw_get_reg(base_addr, &mcsc_regs[reg_idx_c]));
}

void is_scaler_set_rdma_enable(void __iomem *base_addr, u32 hw_id, bool dma_in_en)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_ENABLE],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_EN], (u32)dma_in_en);
}

void is_scaler_set_dma_out_enable(void __iomem *base_addr, u32 output_id, bool dma_out_en)
{
	u32 wdma_enable_reg, wdma_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W0_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W0_EN;
		break;
	case MCSC_OUTPUT1:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W1_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W1_EN;
		break;
	case MCSC_OUTPUT2:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W2_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W2_EN;
		break;
	case MCSC_OUTPUT3:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W3_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W3_EN;
		break;
	case MCSC_OUTPUT4:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W4_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W4_EN;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_enable_reg],
		&mcsc_fields[wdma_enable_field], (u32)dma_out_en);
}

u32 is_scaler_get_dma_out_enable(void __iomem *base_addr, u32 output_id)
{
	u32 wdma_enable_reg, wdma_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W0_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W0_EN;
		break;
	case MCSC_OUTPUT1:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W1_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W1_EN;
		break;
	case MCSC_OUTPUT2:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W2_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W2_EN;
		break;
	case MCSC_OUTPUT3:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W3_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W3_EN;
		break;
	case MCSC_OUTPUT4:
		wdma_enable_reg = MCSC_R_YUV_WDMASC_W4_ENABLE;
		wdma_enable_field = MCSC_F_YUV_WDMASC_W4_EN;
		break;
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[wdma_enable_reg],
		&mcsc_fields[wdma_enable_field]);
}

/* SBWC */
static void is_scaler_set_wdma_sbwc_enable(void __iomem *base_addr,
	u32 output_id, u32 sbwc_en)
{
	u32 wdma_comp_ctrl_reg, wdma_comp_en_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_comp_ctrl_reg = MCSC_R_YUV_WDMASC_W0_COMP_CTRL;
		wdma_comp_en_field = MCSC_F_YUV_WDMASC_W0_SBWC_EN;
		break;
	case MCSC_OUTPUT1:
		wdma_comp_ctrl_reg = MCSC_R_YUV_WDMASC_W1_COMP_CTRL;
		wdma_comp_en_field = MCSC_F_YUV_WDMASC_W1_SBWC_EN;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_comp_ctrl_reg],
		&mcsc_fields[wdma_comp_en_field], sbwc_en);
}

static void is_scaler_set_rdma_sbwc_enable(void __iomem *base_addr, u32 sbwc_en)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_COMP_CTRL],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_SBWC_EN], sbwc_en);
}

static u32 is_scaler_get_wdma_sbwc_enable(void __iomem *base_addr, u32 output_id)
{
	u32 reg, field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg = MCSC_R_YUV_WDMASC_W0_COMP_CTRL;
		field = MCSC_F_YUV_WDMASC_W0_SBWC_EN;
		break;
	case MCSC_OUTPUT1:
		reg = MCSC_R_YUV_WDMASC_W1_COMP_CTRL;
		field = MCSC_F_YUV_WDMASC_W1_SBWC_EN;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[reg], &mcsc_fields[field]);
}


static void is_scaler_set_wdma_sbwc_64b_align(void __iomem *base_addr,
	u32 output_id, u32 align_64b)
{
	u32 wdma_comp_ctrl_reg, wdma_sbwc_64b_align_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_comp_ctrl_reg = MCSC_R_YUV_WDMASC_W0_COMP_CTRL;
		wdma_sbwc_64b_align_field =
			MCSC_F_YUV_WDMASC_W0_SBWC_64B_ALIGN;
		break;
	case MCSC_OUTPUT1:
		wdma_comp_ctrl_reg = MCSC_R_YUV_WDMASC_W1_COMP_CTRL;
		wdma_sbwc_64b_align_field =
			MCSC_F_YUV_WDMASC_W1_SBWC_64B_ALIGN;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}
	is_hw_set_field(base_addr, &mcsc_regs[wdma_comp_ctrl_reg],
		&mcsc_fields[wdma_sbwc_64b_align_field], align_64b);
}

static void is_scaler_set_rdma_sbwc_64b_align(void __iomem *base_addr, u32 align_64b)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_COMP_CTRL],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_SBWC_64B_ALIGN], align_64b);
}

static void is_scaler_set_wdma_sbwc_lossy_rate(void __iomem *base_addr,
	u32 output_id, u32 lossy_rate)
{
	u32 reg, field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg = MCSC_R_YUV_WDMASC_W0_COMP_LOSSY_BYTE32NUM;
		field = MCSC_F_YUV_WDMASC_W0_COMP_LOSSY_BYTE32NUM;
		break;
	case MCSC_OUTPUT1:
		reg = MCSC_R_YUV_WDMASC_W1_COMP_LOSSY_BYTE32NUM;
		field = MCSC_F_YUV_WDMASC_W1_COMP_LOSSY_BYTE32NUM;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[reg],
		&mcsc_fields[field], lossy_rate);
}

static u32 is_scaler_get_sbwc_sram_offset(void __iomem *base_addr, u32 wdma1_w)
{
	u32 w0_offset = 0, w1_offset;
	u32 wdma0_w, wdma0_h;

	if (is_scaler_get_wdma_sbwc_enable(base_addr, MCSC_OUTPUT0) &&
		is_scaler_get_wdma_sbwc_enable(base_addr, MCSC_OUTPUT1)) {
		is_scaler_get_wdma_size(base_addr, MCSC_OUTPUT0, &wdma0_w, &wdma0_h);
		w0_offset = ALIGN(wdma0_w, 32);
		/* constraint check */
		w1_offset = ALIGN(wdma1_w, 32);
		if ((w0_offset + w1_offset) > 9216)
			err_hw("Invalid MCSC image size, w0(%d) + w1(%d) must be less than (%d)",
				w0_offset, w1_offset, 9216);
	}

	return w0_offset;
}

static void is_scaler_set_wdma_sbwc_sram_offset(void __iomem *base_addr,
	u32 output_id, u32 out_width)
{
	u32 reg, field, value;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg = MCSC_R_YUV_WDMASC_W0_COMP_SRAM_START_ADDR;
		field = MCSC_F_YUV_WDMASC_W0_COMP_SRAM_START_ADDR;
		value = 0; /* HW recommend*/
		break;
	case MCSC_OUTPUT1:
		reg = MCSC_R_YUV_WDMASC_W1_COMP_SRAM_START_ADDR;
		field = MCSC_F_YUV_WDMASC_W1_COMP_SRAM_START_ADDR;
		value = is_scaler_get_sbwc_sram_offset(base_addr, out_width);
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[reg], &mcsc_fields[field], value);
}

static int is_scaler_get_sbwc_pixelsize(u32 sbwc_enable, u32 format,
	u32 bitwidth, u32 *pixelsize)
{
	int ret = 0;

	if (sbwc_enable) {
		if (!(format == DMA_INOUT_FORMAT_YUV422
			|| format == DMA_INOUT_FORMAT_YUV420)) {
			ret = -EINVAL;
		}

		if (bitwidth == DMA_INOUT_BIT_WIDTH_8BIT)
			*pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		else if (bitwidth == DMA_INOUT_BIT_WIDTH_16BIT)
			*pixelsize = DMA_INOUT_BIT_WIDTH_10BIT;
		else
			ret = -EINVAL;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_MC_SCALER_V10_1)
static void is_scaler_set_sbwc_order(void __iomem *base_addr, u32 output_id, u32 order)
{
	u32 reg, field, value = MCSC_U_FIRST;

	if (order == DMA_INOUT_ORDER_CrCb)
		value = MCSC_V_FIRST;

	switch (output_id) {
	case MCSC_OUTPUT0:
		reg = MCSC_R_YUV_MAIN_CTRL_SBWC_CR_FIRST;
		field = MCSC_F_YUV_SBWC_ENC_CR_FIRST_WDMA_0;
		break;
	case MCSC_OUTPUT1:
		reg = MCSC_R_YUV_MAIN_CTRL_SBWC_CR_FIRST;
		field = MCSC_F_YUV_SBWC_ENC_CR_FIRST_WDMA_1;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[reg], &mcsc_fields[field], value);
}
#endif

u32 is_scaler_g_dma_offset(struct param_mcs_output *output,
	u32 start_pos_x, u32 plane_i)
{
	u32 sbwc_type, comp_64b_align, comp_sbwc_en;
	u32 lossy_byte32num = 2;  /* 8bit : 50%, 10bit : 40%*/
	u32 dma_offset = 0, bitwidth;
	unsigned long bitsperpixel;

	sbwc_type = output->sbwc_type;
	bitwidth = output->dma_bitwidth;
	bitsperpixel = output->bitsperpixel;
	comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type,
			&comp_64b_align);

	switch (comp_sbwc_en) {
	case COMP:
	case COMP_LOSS:
		dma_offset = is_hw_dma_get_payload_stride(
			comp_sbwc_en, bitwidth, start_pos_x,
			comp_64b_align, lossy_byte32num,
			MCSC_COMP_BLOCK_WIDTH,
			MCSC_COMP_BLOCK_HEIGHT);
		break;
	default:
		if (plane_i <= 3) {
			bitsperpixel = (output->bitsperpixel >> (plane_i * 8)) & 0xFF;

			dma_offset =
				start_pos_x * bitsperpixel / BITS_PER_BYTE;
		} else {
			warn_hw("invalid plane_i(%d)\n", plane_i);
		}

		break;
	}

	return dma_offset;
}

void is_scaler_set_wdma_sbwc_config(void __iomem *base_addr,
	struct param_mcs_output *output, u32 output_id,
	u32 *y_stride, u32 *uv_stride, u32 *y_2bit_stride, u32 *uv_2bit_stride)
{
	int ret;
	u32 sbwc_type;
	u32 comp_sbwc_en, comp_64b_align, lossy_byte32num = 0;
	u32 out_width, format, bitwidth, pixelsize = 0;
	u32 order;

	sbwc_type = output->sbwc_type;
	format = output->dma_format;
	bitwidth = output->dma_bitwidth;
	out_width = output->full_output_width;
	order = output->dma_order;

	comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	ret = is_scaler_get_sbwc_pixelsize(comp_sbwc_en, format, bitwidth, &pixelsize);
	if (ret) {
		err_hw("Invalid MCSC SBWC WDMA Output format(%d) bitwidth(%d)", format, bitwidth);
		return;
	}

	if (comp_sbwc_en == COMP) {
		*y_stride = is_hw_dma_get_payload_stride(comp_sbwc_en, bitwidth, out_width,
								comp_64b_align, lossy_byte32num,
								MCSC_COMP_BLOCK_WIDTH,
								MCSC_COMP_BLOCK_HEIGHT);
		*uv_stride = is_hw_dma_get_payload_stride(comp_sbwc_en, bitwidth, out_width,
								comp_64b_align, lossy_byte32num,
								MCSC_COMP_BLOCK_WIDTH,
								MCSC_COMP_BLOCK_HEIGHT);
		*y_2bit_stride = is_hw_dma_get_header_stride(out_width,	MCSC_COMP_BLOCK_WIDTH,
								16);
		*uv_2bit_stride = is_hw_dma_get_header_stride(out_width, MCSC_COMP_BLOCK_WIDTH,
								16);
	} else if (comp_sbwc_en == COMP_LOSS) {
		if (IS_ENABLED(CONFIG_MC_SCALER_V10_1)) {
			/* 8bit : 50%, 10bit : 40% */
			lossy_byte32num = 2;
		} else {
			if (pixelsize == OTF_OUTPUT_BIT_WIDTH_8BIT)
				lossy_byte32num = 2; /* 8bit : 50% */
			else
				lossy_byte32num = 3; /* 10bit : 60% */
		}

		*y_stride = is_hw_dma_get_payload_stride(comp_sbwc_en, bitwidth, out_width,
			comp_64b_align, lossy_byte32num, MCSC_COMP_BLOCK_WIDTH, MCSC_COMP_BLOCK_HEIGHT);
		*uv_stride = is_hw_dma_get_payload_stride(comp_sbwc_en, bitwidth, out_width,
			comp_64b_align, lossy_byte32num, MCSC_COMP_BLOCK_WIDTH, MCSC_COMP_BLOCK_HEIGHT);
		*y_2bit_stride = *uv_2bit_stride = 0;
	} else {
		*y_2bit_stride = 0;
		*uv_2bit_stride = 0;
	}

#if IS_ENABLED(CONFIG_MC_SCALER_V10_1)
	is_scaler_set_sbwc_order(base_addr, output_id, order);
#endif
	is_scaler_set_wdma_sbwc_64b_align(base_addr, output_id,
		comp_64b_align);
	is_scaler_set_wdma_sbwc_lossy_rate(base_addr, output_id,
		lossy_byte32num);
	is_scaler_set_wdma_sbwc_enable(base_addr, output_id, comp_sbwc_en);
	is_scaler_set_wdma_sbwc_sram_offset(base_addr, output_id, out_width);
}

static void is_scaler_set_rdma_sbwc_config(void __iomem *base_addr,
	struct param_mcs_output *output, u32 *y_stride, u32 *payload_size)
{
	u32 comp_sbwc_en, comp_64b_align;

	comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(output->sbwc_type, &comp_64b_align);
	if (comp_sbwc_en == COMP) {
		*y_stride = is_hw_dma_get_payload_stride(comp_sbwc_en, output->dma_bitwidth,
				output->width, comp_64b_align, 0,
				MCSC_HF_COMP_BLOCK_WIDTH, MCSC_HF_COMP_BLOCK_HEIGHT);
		*payload_size = ((output->height + MCSC_HF_COMP_BLOCK_HEIGHT - 1) /
				MCSC_HF_COMP_BLOCK_HEIGHT) * (*y_stride);

		dbg_hw(2, "[%s] y_stride(%x), payload_size(%x)\n", __func__, *y_stride, *payload_size);
	} else if (comp_sbwc_en == COMP_LOSS) {
		err_hw("MCSC doesn't support lossy in HF(%d)", comp_sbwc_en);
		return;
	}

	is_scaler_set_rdma_sbwc_64b_align(base_addr, comp_64b_align);
	is_scaler_set_rdma_sbwc_enable(base_addr, comp_sbwc_en);
}

void is_scaler_set_rdma_sbwc_header_addr(void __iomem *base_addr, u32 haddr, int buf_index)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_STAT_RDMAHF_HEADER_BASE_ADDR_1P_FRO_0_0 + buf_index], haddr);
}

void is_scaler_set_rdma_sbwc_header_stride(void __iomem *base_addr, u32 width)
{
	u32 sbwc_header_stride = is_hw_dma_get_header_stride(width, MCSC_HF_COMP_BLOCK_WIDTH, 16);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_STRIDE_HEADER_1P],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_STRIDE_HEADER_1P], sbwc_header_stride);
}

/* VOTF */
static void is_scaler_set_rdma_votf_enable(void __iomem *base_addr, u32 votf_en)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_VOTF_EN],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_VOTF_EN], votf_en);
}

void is_scaler_set_rdma_format(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	u32 reg_val = 0;

	/* 0 : u8(NO SBWC) 1 : u10(SBWC) */
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_BAYER], 0);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_RGB], 0);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_YUV], 0);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_MSBALIGN], 0);
	/* 0 : ip data is equal to dma_out */
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_MSBALIGN_UNSIGN], 0);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_STAT_RDMAHF_DATA_FORMAT_BIT_CROP_OFF], 0);

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_DATA_FORMAT], reg_val);
}

static u32 is_scaler_get_mono_ctrl(void __iomem *base_addr)
{
	u32 fmt = 0;

	if (is_hw_get_field(base_addr,
		&mcsc_regs[MCSC_R_YUV_MAIN_CTRL_INPUT_MONO_EN],
		&mcsc_fields[MCSC_F_YUV_INPUT_MONO_EN]))
		fmt = MCSC_MONO_Y8;

	return fmt;
}

void is_scaler_set_mono_ctrl(void __iomem *base_addr, u32 hw_id, u32 in_fmt)
{
	u32 en = 0;

	if (in_fmt == MCSC_MONO_Y8)
		en = 1;

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_INPUT_MONO_EN],
		&mcsc_fields[MCSC_F_YUV_INPUT_MONO_EN], en);
}

void is_scaler_set_rdma_10bit_type(void __iomem *base_addr, u32 dma_in_10bit_type)
{
	/* not support */
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_10bit_type);

static void is_scaler_set_wdma_mono_ctrl(void __iomem *base_addr, u32 output_id, bool en)
{
	u32 wdma_mono_en_field, wdma_mono_ctrl_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_mono_ctrl_reg = MCSC_R_YUV_WDMASC_W0_MONO_CTRL;
		wdma_mono_en_field = MCSC_F_YUV_WDMASC_W0_MONO_EN;
		break;
	case MCSC_OUTPUT1:
		wdma_mono_ctrl_reg = MCSC_R_YUV_WDMASC_W1_MONO_CTRL;
		wdma_mono_en_field = MCSC_F_YUV_WDMASC_W1_MONO_EN;
		break;
	case MCSC_OUTPUT2:
		wdma_mono_ctrl_reg = MCSC_R_YUV_WDMASC_W2_MONO_CTRL;
		wdma_mono_en_field = MCSC_F_YUV_WDMASC_W2_MONO_EN;
		break;
	case MCSC_OUTPUT3:
		wdma_mono_ctrl_reg = MCSC_R_YUV_WDMASC_W3_MONO_CTRL;
		wdma_mono_en_field = MCSC_F_YUV_WDMASC_W3_MONO_EN;
		break;
	case MCSC_OUTPUT4:
		wdma_mono_ctrl_reg = MCSC_R_YUV_WDMASC_W4_MONO_CTRL;
		wdma_mono_en_field = MCSC_F_YUV_WDMASC_W4_MONO_EN;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_mono_ctrl_reg],
		&mcsc_fields[wdma_mono_en_field], (u32)en);
}

static void is_scaler_set_wdma_rgb_coefficient(void __iomem *base_addr)
{
	u32 reg_val = 0;

	/* this value is valid only YUV422 only. wide coefficient is used in JPEG, Pictures, preview. */
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_SRC_Y_OFFSET], 0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_OFFSET], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C00], 512);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C10], 0);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_COEF_0], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C20], 738);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C01], 512);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_COEF_1], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C11], -82);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C21], -286);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_COEF_2], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C02], 512);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C12], 942);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_COEF_3], reg_val);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_WDMASC_RGB_COEF_4],
		&mcsc_fields[MCSC_F_YUV_WDMASC_RGB_COEF_C22], 0);
}

static void is_scaler_set_wdma_format_type(void __iomem *base_addr,
	u32 output_id, u32 format_type)
{
	u32 wdma_data_format_reg, wdma_data_format_type_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W0_DATA_FORMAT;
		wdma_data_format_type_field =
			MCSC_F_YUV_WDMASC_W0_DATA_FORMAT_TYPE;
		break;
	case MCSC_OUTPUT1:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W1_DATA_FORMAT;
		wdma_data_format_type_field =
			MCSC_F_YUV_WDMASC_W1_DATA_FORMAT_TYPE;
		break;
	case MCSC_OUTPUT2:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W2_DATA_FORMAT;
		wdma_data_format_type_field =
			MCSC_F_YUV_WDMASC_W2_DATA_FORMAT_TYPE;
		break;
	case MCSC_OUTPUT3:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W3_DATA_FORMAT;
		wdma_data_format_type_field =
			MCSC_F_YUV_WDMASC_W3_DATA_FORMAT_TYPE;
		break;
	case MCSC_OUTPUT4:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W4_DATA_FORMAT;
		wdma_data_format_type_field =
			MCSC_F_YUV_WDMASC_W4_DATA_FORMAT_TYPE;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_data_format_reg],
		&mcsc_fields[wdma_data_format_type_field], format_type);
}
static void is_scaler_set_wdma_rgb_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	u32 wdma_data_format_reg, wdma_data_format_rgb_field;

	switch (output_id) {
	case MCSC_OUTPUT2:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W2_DATA_FORMAT;
		wdma_data_format_rgb_field =
			MCSC_F_YUV_WDMASC_W2_DATA_FORMAT_RGB;
		break;
	case MCSC_OUTPUT0:
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_data_format_reg],
		&mcsc_fields[wdma_data_format_rgb_field], out_fmt);
}

static void is_scaler_set_wdma_yuv_format(void __iomem *base_addr, u32 output_id, u32 out_fmt)
{
	u32 wdma_data_format_reg, wdma_data_format_yuv_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W0_DATA_FORMAT;
		wdma_data_format_yuv_field =
			MCSC_F_YUV_WDMASC_W0_DATA_FORMAT_YUV;
		break;
	case MCSC_OUTPUT1:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W1_DATA_FORMAT;
		wdma_data_format_yuv_field =
			MCSC_F_YUV_WDMASC_W1_DATA_FORMAT_YUV;
		break;
	case MCSC_OUTPUT2:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W2_DATA_FORMAT;
		wdma_data_format_yuv_field =
			MCSC_F_YUV_WDMASC_W2_DATA_FORMAT_YUV;
		break;
	case MCSC_OUTPUT3:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W3_DATA_FORMAT;
		wdma_data_format_yuv_field =
			MCSC_F_YUV_WDMASC_W3_DATA_FORMAT_YUV;
		break;
	case MCSC_OUTPUT4:
		wdma_data_format_reg = MCSC_R_YUV_WDMASC_W4_DATA_FORMAT;
		wdma_data_format_yuv_field =
			MCSC_F_YUV_WDMASC_W4_DATA_FORMAT_YUV;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_data_format_reg],
		&mcsc_fields[wdma_data_format_yuv_field], out_fmt);
}

void is_scaler_set_wdma_format(void __iomem *base_addr, u32 hw_id, u32 output_id, u32 plane, u32 out_fmt)
{
	u32 in_fmt;
	u32 format_type;

	in_fmt = is_scaler_get_mono_ctrl(base_addr);
	if (in_fmt == MCSC_MONO_Y8 && out_fmt != MCSC_MONO_Y8) {
		warn_hw("[ID:%d]Input format(MONO), out_format(%d) is changed to MONO format!\n",
			hw_id, out_fmt);
		out_fmt = MCSC_MONO_Y8;
	}

	/* format_type 0 : YUV , 1 : RGB */
	switch (out_fmt) {
	case MCSC_RGB_ARGB8888:
	case MCSC_RGB_BGRA8888:
	case MCSC_RGB_RGBA8888:
	case MCSC_RGB_ABGR8888:
	case MCSC_RGB_ABGR1010102:
	case MCSC_RGB_RGBA1010102:
		is_scaler_set_wdma_rgb_coefficient(base_addr);
		out_fmt = out_fmt - MCSC_RGB_RGBA8888 + 1;
		format_type = 1;
		break;
	default:
		format_type = 0;
		break;
	}

	if (out_fmt == MCSC_MONO_Y8)
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, true);
	else
		is_scaler_set_wdma_mono_ctrl(base_addr, output_id, false);

	is_scaler_set_wdma_format_type(base_addr, output_id, format_type);
	if (format_type) {
		if (output_id == MCSC_OUTPUT2)
			is_scaler_set_wdma_rgb_format(base_addr, output_id, out_fmt);
		else
			err_hw("RGB Conversion support only WDMA2");
	} else
		is_scaler_set_wdma_yuv_format(base_addr, output_id, out_fmt);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_wdma_format);

void is_scaler_set_wdma_dither(void __iomem *base_addr, u32 output_id,
	u32 fmt, u32 bitwidth, u32 plane)
{
	u32 reg_val;
	u32 round_en, dither_en;
	u32 wdma_dither_en_y_field, wdma_dither_en_c_field;
	u32 wdma_round_en_field;
	u32 wdma_dither_reg;

	if (fmt == DMA_INOUT_FORMAT_RGB && bitwidth == DMA_INOUT_BIT_WIDTH_8BIT) {
		dither_en = 0;
		round_en = 1;
	} else if (bitwidth == DMA_INOUT_BIT_WIDTH_8BIT || plane == 4) {
		dither_en = 1;
		round_en = 0;
	} else {
		dither_en = 0;
		round_en = 0;
	}

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_dither_en_y_field = MCSC_F_YUV_WDMASC_W0_DITHER_EN_Y;
		wdma_dither_en_c_field = MCSC_F_YUV_WDMASC_W0_DITHER_EN_C;
		wdma_round_en_field = MCSC_F_YUV_WDMASC_W0_ROUND_EN;
		wdma_dither_reg = MCSC_R_YUV_WDMASC_W0_DITHER;
		break;
	case MCSC_OUTPUT1:
		wdma_dither_en_y_field = MCSC_F_YUV_WDMASC_W1_DITHER_EN_Y;
		wdma_dither_en_c_field = MCSC_F_YUV_WDMASC_W1_DITHER_EN_C;
		wdma_round_en_field = MCSC_F_YUV_WDMASC_W1_ROUND_EN;
		wdma_dither_reg = MCSC_R_YUV_WDMASC_W1_DITHER;
		break;
	case MCSC_OUTPUT2:
		wdma_dither_en_y_field = MCSC_F_YUV_WDMASC_W2_DITHER_EN_Y;
		wdma_dither_en_c_field = MCSC_F_YUV_WDMASC_W2_DITHER_EN_C;
		wdma_round_en_field = MCSC_F_YUV_WDMASC_W2_ROUND_EN;
		wdma_dither_reg = MCSC_R_YUV_WDMASC_W2_DITHER;
		break;
	case MCSC_OUTPUT3:
		wdma_dither_en_y_field = MCSC_F_YUV_WDMASC_W3_DITHER_EN_Y;
		wdma_dither_en_c_field = MCSC_F_YUV_WDMASC_W3_DITHER_EN_C;
		wdma_round_en_field = MCSC_F_YUV_WDMASC_W3_ROUND_EN;
		wdma_dither_reg = MCSC_R_YUV_WDMASC_W3_DITHER;
		break;
	case MCSC_OUTPUT4:
		wdma_dither_en_y_field = MCSC_F_YUV_WDMASC_W4_DITHER_EN_Y;
		wdma_dither_en_c_field = MCSC_F_YUV_WDMASC_W4_DITHER_EN_C;
		wdma_round_en_field = MCSC_F_YUV_WDMASC_W4_ROUND_EN;
		wdma_dither_reg = MCSC_R_YUV_WDMASC_W4_DITHER;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[wdma_dither_en_y_field], dither_en);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[wdma_dither_en_c_field], dither_en);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[wdma_round_en_field], round_en);
	is_hw_set_reg(base_addr, &mcsc_regs[wdma_dither_reg], reg_val);
}

void is_scaler_set_flip_mode(void __iomem *base_addr, u32 output_id, u32 flip)
{
	u32 wdma_flip_control_reg, wdma_flip_control_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_flip_control_reg = MCSC_R_YUV_WDMASC_W0_FLIP_CONTROL;
		wdma_flip_control_field = MCSC_F_YUV_WDMASC_W0_FLIP_CONTROL;
		break;
	case MCSC_OUTPUT1:
		wdma_flip_control_reg = MCSC_R_YUV_WDMASC_W1_FLIP_CONTROL;
		wdma_flip_control_field = MCSC_F_YUV_WDMASC_W1_FLIP_CONTROL;
		break;
	case MCSC_OUTPUT2:
		wdma_flip_control_reg = MCSC_R_YUV_WDMASC_W2_FLIP_CONTROL;
		wdma_flip_control_field = MCSC_F_YUV_WDMASC_W2_FLIP_CONTROL;
		break;
	case MCSC_OUTPUT3:
		wdma_flip_control_reg = MCSC_R_YUV_WDMASC_W3_FLIP_CONTROL;
		wdma_flip_control_field = MCSC_F_YUV_WDMASC_W3_FLIP_CONTROL;
		break;
	case MCSC_OUTPUT4:
		wdma_flip_control_reg = MCSC_R_YUV_WDMASC_W4_FLIP_CONTROL;
		wdma_flip_control_field = MCSC_F_YUV_WDMASC_W4_FLIP_CONTROL;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_flip_control_reg],
		&mcsc_fields[wdma_flip_control_field], flip);
}

void is_scaler_set_rdma_size(void __iomem *base_addr, u32 width, u32 height)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_WIDTH],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_WIDTH], width);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_HEIGHT],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_HEIGHT], height);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_size);

void is_scaler_get_rdma_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_WIDTH],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_WIDTH]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_HEIGHT],
		&mcsc_fields[MCSC_F_STAT_RDMAHF_HEIGHT]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_rdma_size);

void is_scaler_set_wdma_size(void __iomem *base_addr, u32 output_id, u32 width, u32 height)
{
	u32 wdma_width_reg, wdma_width_field;
	u32 wdma_height_reg, wdma_height_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W0_WIDTH;
		wdma_width_field = MCSC_F_YUV_WDMASC_W0_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W0_HEIGHT;
		wdma_height_field = MCSC_F_YUV_WDMASC_W0_HEIGHT;
		break;
	case MCSC_OUTPUT1:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W1_WIDTH;
		wdma_width_field = MCSC_F_YUV_WDMASC_W1_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W1_HEIGHT;
		wdma_height_field = MCSC_F_YUV_WDMASC_W1_HEIGHT;
		break;
	case MCSC_OUTPUT2:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W2_WIDTH;
		wdma_width_field = MCSC_F_YUV_WDMASC_W2_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W2_HEIGHT;
		wdma_height_field = MCSC_F_YUV_WDMASC_W2_HEIGHT;
		break;
	case MCSC_OUTPUT3:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W3_WIDTH;
		wdma_width_field = MCSC_F_YUV_WDMASC_W3_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W3_HEIGHT;
		wdma_height_field = MCSC_F_YUV_WDMASC_W3_HEIGHT;
		break;
	case MCSC_OUTPUT4:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W4_WIDTH;
		wdma_width_field = MCSC_F_YUV_WDMASC_W4_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W4_HEIGHT;
		wdma_height_field = MCSC_F_YUV_WDMASC_W4_HEIGHT;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_width_reg],
		&mcsc_fields[wdma_width_field], width);
	is_hw_set_field(base_addr, &mcsc_regs[wdma_height_reg],
		&mcsc_fields[wdma_height_field], height);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_wdma_size);

void is_scaler_get_wdma_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 wdma_width_reg, wdma_height_reg;
	u32 wdma_width_field = MCSC_F_YUV_WDMASC_W0_WIDTH;
	u32 wdma_height_field = MCSC_F_YUV_WDMASC_W0_HEIGHT;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W0_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W0_HEIGHT;
		break;
	case MCSC_OUTPUT1:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W1_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W1_HEIGHT;
		break;
	case MCSC_OUTPUT2:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W2_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W2_HEIGHT;
		break;
	case MCSC_OUTPUT3:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W3_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W3_HEIGHT;
		break;
	case MCSC_OUTPUT4:
		wdma_width_reg = MCSC_R_YUV_WDMASC_W4_WIDTH;
		wdma_height_reg = MCSC_R_YUV_WDMASC_W4_HEIGHT;
		break;
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[wdma_width_reg],
		&mcsc_fields[wdma_width_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[wdma_height_reg],
		&mcsc_fields[wdma_height_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_wdma_size);

void is_scaler_set_rdma_stride(void __iomem *base_addr, u32 y_stride, u32 uv_stride)
{
	/* RDMA format - Bayer */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_STRIDE_1P],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_STRIDE_1P], y_stride);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_stride);

void is_scaler_set_rdma_2bit_stride(void __iomem *base_addr, u32 y_2bit_stride, u32 uv_2bit_stride)
{
	/* RDMA format - Bayer */
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_STRIDE_HEADER_1P],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_STRIDE_HEADER_1P], y_2bit_stride);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_2bit_stride);

void is_scaler_get_rdma_stride(void __iomem *base_addr, u32 *y_stride, u32 *uv_stride)
{
	/* RDMA format - Bayer */
	*y_stride = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_STAT_RDMAHF_STRIDE_1P],
			&mcsc_fields[MCSC_F_STAT_RDMAHF_STRIDE_1P]);
	*uv_stride = 0;
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_rdma_stride);

void is_scaler_set_wdma_stride(void __iomem *base_addr, u32 output_id, u32 y_stride, u32 uv_stride)
{
	u32 wdma_stride_1p_reg;
	u32 wdma_stride_2p_reg;
	u32 wdma_stride_3p_reg;
	u32 wdma_stride_123p_field = MCSC_F_YUV_WDMASC_W0_STRIDE_1P;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W0_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W0_STRIDE_2P;
		wdma_stride_3p_reg = MCSC_R_YUV_WDMASC_W0_STRIDE_3P;
		break;
	case MCSC_OUTPUT1:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W1_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W1_STRIDE_2P;
		wdma_stride_3p_reg = MCSC_R_YUV_WDMASC_W1_STRIDE_3P;
		break;
	case MCSC_OUTPUT2:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W2_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W2_STRIDE_2P;
		wdma_stride_3p_reg = MCSC_R_YUV_WDMASC_W2_STRIDE_3P;
		break;
	case MCSC_OUTPUT3:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W3_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W3_STRIDE_2P;
		wdma_stride_3p_reg = MCSC_R_YUV_WDMASC_W3_STRIDE_3P;
		break;
	case MCSC_OUTPUT4:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W4_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W4_STRIDE_2P;
		wdma_stride_3p_reg = MCSC_R_YUV_WDMASC_W4_STRIDE_3P;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_stride_1p_reg],
		&mcsc_fields[wdma_stride_123p_field], y_stride);
	is_hw_set_field(base_addr, &mcsc_regs[wdma_stride_2p_reg],
		&mcsc_fields[wdma_stride_123p_field], uv_stride);
	is_hw_set_field(base_addr, &mcsc_regs[wdma_stride_3p_reg],
		&mcsc_fields[wdma_stride_123p_field], uv_stride);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_wdma_stride);

void is_scaler_set_wdma_2bit_stride(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_stride, u32 uv_2bit_stride)
{
	u32 wdma_stride_header_1p_reg, wdma_stride_header_1p_field;
	u32 wdma_stride_header_2p_reg, wdma_stride_header_2p_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_stride_header_1p_reg =
			MCSC_R_YUV_WDMASC_W0_STRIDE_HEADER_1P;
		wdma_stride_header_1p_field =
			MCSC_F_YUV_WDMASC_W0_STRIDE_HEADER_1P;
		wdma_stride_header_2p_reg =
			MCSC_R_YUV_WDMASC_W0_STRIDE_HEADER_2P;
		wdma_stride_header_2p_field =
			MCSC_F_YUV_WDMASC_W0_STRIDE_HEADER_2P;
		break;
	case MCSC_OUTPUT1:
		wdma_stride_header_1p_reg =
			MCSC_R_YUV_WDMASC_W1_STRIDE_HEADER_1P;
		wdma_stride_header_1p_field =
			MCSC_F_YUV_WDMASC_W1_STRIDE_HEADER_1P;
		wdma_stride_header_2p_reg =
			MCSC_R_YUV_WDMASC_W1_STRIDE_HEADER_2P;
		wdma_stride_header_2p_field =
			MCSC_F_YUV_WDMASC_W1_STRIDE_HEADER_2P;
		break;
	case MCSC_OUTPUT2:
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[wdma_stride_header_1p_reg],
		&mcsc_fields[wdma_stride_header_1p_field], y_2bit_stride);
	is_hw_set_field(base_addr, &mcsc_regs[wdma_stride_header_2p_reg],
		&mcsc_fields[wdma_stride_header_2p_field], uv_2bit_stride);
}


void is_scaler_get_wdma_stride(void __iomem *base_addr, u32 output_id, u32 *y_stride, u32 *uv_stride)
{
	u32 wdma_stride_1p_reg, wdma_stride_1p_field;
	u32 wdma_stride_2p_reg, wdma_stride_2p_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W0_STRIDE_1P;
		wdma_stride_1p_field = MCSC_F_YUV_WDMASC_W0_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W0_STRIDE_2P;
		wdma_stride_2p_field = MCSC_F_YUV_WDMASC_W0_STRIDE_2P;
		break;
	case MCSC_OUTPUT1:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W1_STRIDE_1P;
		wdma_stride_1p_field = MCSC_F_YUV_WDMASC_W1_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W1_STRIDE_2P;
		wdma_stride_2p_field = MCSC_F_YUV_WDMASC_W1_STRIDE_2P;
		break;
	case MCSC_OUTPUT2:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W2_STRIDE_1P;
		wdma_stride_1p_field = MCSC_F_YUV_WDMASC_W2_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W2_STRIDE_2P;
		wdma_stride_2p_field = MCSC_F_YUV_WDMASC_W2_STRIDE_2P;
		break;
	case MCSC_OUTPUT3:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W3_STRIDE_1P;
		wdma_stride_1p_field = MCSC_F_YUV_WDMASC_W3_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W3_STRIDE_2P;
		wdma_stride_2p_field = MCSC_F_YUV_WDMASC_W3_STRIDE_2P;
		break;
	case MCSC_OUTPUT4:
		wdma_stride_1p_reg = MCSC_R_YUV_WDMASC_W4_STRIDE_1P;
		wdma_stride_1p_field = MCSC_F_YUV_WDMASC_W4_STRIDE_1P;
		wdma_stride_2p_reg = MCSC_R_YUV_WDMASC_W4_STRIDE_2P;
		wdma_stride_2p_field = MCSC_F_YUV_WDMASC_W4_STRIDE_2P;
		break;
	default:
		*y_stride = 0;
		*uv_stride = 0;
		return;
	}

	*y_stride = is_hw_get_field(base_addr,
		&mcsc_regs[wdma_stride_1p_reg],
		&mcsc_fields[wdma_stride_1p_field]);
	*uv_stride = is_hw_get_field(base_addr,
		&mcsc_regs[wdma_stride_2p_reg],
		&mcsc_fields[wdma_stride_2p_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_wdma_stride);

void is_scaler_set_rdma_frame_seq(void __iomem *base_addr, u32 frame_seq)
{
	/* not support */
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_frame_seq);

void is_scaler_set_rdma_addr(void __iomem *base_addr,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_STAT_RDMAHF_IMG_BASE_ADDR_1P_FRO_0_0 + buf_index],
		y_addr);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_addr);

void is_scaler_set_rdma_2bit_addr(void __iomem *base_addr,
	u32 y_2bit_addr, u32 cbcr_2bit_addr, int buf_index)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_STAT_RDMAHF_HEADER_BASE_ADDR_1P_FRO_0_0 + buf_index],
		y_2bit_addr);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_rdma_2bit_addr);

static void get_wdma_addr_arr(u32 output_id, u32 *addr)
{
	switch (output_id) {
	case MCSC_OUTPUT0:
		addr[0] = MCSC_R_YUV_WDMASC_W0_IMG_BASE_ADDR_1P_FRO_0_0;
		addr[1] = MCSC_R_YUV_WDMASC_W0_IMG_BASE_ADDR_2P_FRO_0_0;
		addr[2] = MCSC_R_YUV_WDMASC_W0_IMG_BASE_ADDR_3P_FRO_0_0;
		addr[3] = MCSC_R_YUV_WDMASC_W0_HEADER_BASE_ADDR_1P_FRO_0_0;
		addr[4] = MCSC_R_YUV_WDMASC_W0_HEADER_BASE_ADDR_2P_FRO_0_0;
		break;
	case MCSC_OUTPUT1:
		addr[0] = MCSC_R_YUV_WDMASC_W1_IMG_BASE_ADDR_1P_FRO_0_0;
		addr[1] = MCSC_R_YUV_WDMASC_W1_IMG_BASE_ADDR_2P_FRO_0_0;
		addr[2] = MCSC_R_YUV_WDMASC_W1_IMG_BASE_ADDR_3P_FRO_0_0;
		addr[3] = MCSC_R_YUV_WDMASC_W1_HEADER_BASE_ADDR_1P_FRO_0_0;
		addr[4] = MCSC_R_YUV_WDMASC_W1_HEADER_BASE_ADDR_2P_FRO_0_0;
		break;
	case MCSC_OUTPUT2:
		addr[0] = MCSC_R_YUV_WDMASC_W2_IMG_BASE_ADDR_1P_FRO_0_0;
		addr[1] = MCSC_R_YUV_WDMASC_W2_IMG_BASE_ADDR_2P_FRO_0_0;
		addr[2] = MCSC_R_YUV_WDMASC_W2_IMG_BASE_ADDR_3P_FRO_0_0;
		addr[3] = 0;
		addr[4] = 0;
		break;
	case MCSC_OUTPUT3:
		addr[0] = MCSC_R_YUV_WDMASC_W3_IMG_BASE_ADDR_1P_FRO_0_0;
		addr[1] = MCSC_R_YUV_WDMASC_W3_IMG_BASE_ADDR_2P_FRO_0_0;
		addr[2] = MCSC_R_YUV_WDMASC_W3_IMG_BASE_ADDR_3P_FRO_0_0;
		addr[3] = 0;
		addr[4] = 0;
		break;
	case MCSC_OUTPUT4:
		addr[0] = MCSC_R_YUV_WDMASC_W4_IMG_BASE_ADDR_1P_FRO_0_0;
		addr[1] = MCSC_R_YUV_WDMASC_W4_IMG_BASE_ADDR_2P_FRO_0_0;
		addr[2] = MCSC_R_YUV_WDMASC_W4_IMG_BASE_ADDR_3P_FRO_0_0;
		addr[3] = 0;
		addr[4] = 0;
		break;
	default:
		panic("invalid output_id(%d)", output_id);
		break;
	}
}

void is_scaler_set_wdma_addr(void __iomem *base_addr, u32 output_id,
	u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	is_hw_set_reg(base_addr, &mcsc_regs[addr[0] + buf_index], y_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[1] + buf_index], cb_addr);
	is_hw_set_reg(base_addr, &mcsc_regs[addr[2] + buf_index], cr_addr);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_wdma_addr);

void is_scaler_set_wdma_2bit_addr(void __iomem *base_addr, u32 output_id,
	u32 y_2bit_addr, u32 cbcr_2bit_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	if (output_id == MCSC_OUTPUT0 || output_id == MCSC_OUTPUT1) {
		is_hw_set_reg(base_addr, &mcsc_regs[addr[3] + buf_index], y_2bit_addr);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[4] + buf_index], cbcr_2bit_addr);
	}
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_wdma_2bit_addr);

void is_scaler_get_wdma_addr(void __iomem *base_addr, u32 output_id,
	u32 *y_addr, u32 *cb_addr, u32 *cr_addr, int buf_index)
{
	u32 addr[8] = {0, };

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	*y_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[0] + buf_index]);
	*cb_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[1] + buf_index]);
	*cr_addr = is_hw_get_reg(base_addr, &mcsc_regs[addr[2] + buf_index]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_wdma_addr);

void is_scaler_clear_rdma_addr(void __iomem *base_addr)
{
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_STAT_RDMAHF_IMG_BASE_ADDR_1P_FRO_0_0], 0x0);
	is_hw_set_reg(base_addr,
		&mcsc_regs[MCSC_R_STAT_RDMAHF_HEADER_BASE_ADDR_1P_FRO_0_0], 0x0);
}
KUNIT_EXPORT_SYMBOL(is_scaler_clear_rdma_addr);

void is_scaler_clear_wdma_addr(void __iomem *base_addr, u32 output_id)
{
	u32 addr[8] = {0, }, buf_index;

	get_wdma_addr_arr(output_id, addr);
	if (!addr[0])
		return;

	for (buf_index = 0; buf_index < 8; buf_index++) {
		is_hw_set_reg(base_addr, &mcsc_regs[addr[0] + buf_index], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[1] + buf_index], 0x0);
		is_hw_set_reg(base_addr, &mcsc_regs[addr[2] + buf_index], 0x0);

		/* DMA Header Y, CR address clear */
		if (output_id == MCSC_OUTPUT0 || output_id == MCSC_OUTPUT1) {
			is_hw_set_reg(base_addr, &mcsc_regs[addr[3] + buf_index], 0x0);
			is_hw_set_reg(base_addr, &mcsc_regs[addr[4] + buf_index], 0x0);
		}
	}
}

/* for hwfc */
void is_scaler_set_hwfc_mode(void __iomem *base_addr, u32 hwfc_output_ids)
{
	u32 val = MCSC_HWFC_MODE_OFF;
	u32 read_val;

	if (hwfc_output_ids & (1 << MCSC_OUTPUT3))
		is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_HWFC_FRAME_START_SELECT],
			&mcsc_fields[MCSC_F_YUV_HWFC_FRAME_START_SELECT], 0x0);

	read_val = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_YUV_HWFC_MODE],
			&mcsc_fields[MCSC_F_YUV_HWFC_MODE]);

	if ((hwfc_output_ids & (1 << MCSC_OUTPUT3)) &&
		(hwfc_output_ids & (1 << MCSC_OUTPUT4))) {
		val = MCSC_HWFC_MODE_REGION_A_B_PORT;
	} else if (hwfc_output_ids & (1 << MCSC_OUTPUT3)) {
		val = MCSC_HWFC_MODE_REGION_A_PORT;
	} else if (hwfc_output_ids & (1 << MCSC_OUTPUT4)) {
		err_hw("set_hwfc_mode: invalid output_ids(0x%x)\n", hwfc_output_ids);
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_HWFC_MODE],
			&mcsc_fields[MCSC_F_YUV_HWFC_MODE], val);

	dbg_hw(2, "set_hwfc_mode: regs(0x%x)(0x%x), hwfc_ids(0x%x)\n",
		read_val, val, hwfc_output_ids);
}

void is_scaler_set_hwfc_config(void __iomem *base_addr,
	u32 output_id, u32 fmt, u32 plane, u32 dma_idx, u32 width, u32 height)
{
	u32 val;
	u32 img_resol = width * height;
	u32 total_img_byte0 = 0;
	u32 total_img_byte1 = 0;
	u32 total_img_byte2 = 0;
	u32 total_width_byte0 = 0;
	u32 total_width_byte1 = 0;
	u32 total_width_byte2 = 0;
	u32 hwfc_config_image_reg;
	u32 hwfc_format_field, hwfc_plane_field;
	u32 hwfc_id0_field, hwfc_id1_field, hwfc_id2_field;
	u32 hwfc_config_image;
	u32 hwfc_total_image_byte0_reg;
	u32 hwfc_total_image_byte1_reg;
	u32 hwfc_total_image_byte2_reg;
	u32 hwfc_total_width_byte0_reg;
	u32 hwfc_total_width_byte1_reg;
	u32 hwfc_total_width_byte2_reg;
	u32 hwfc_master_sel_reg;
	u32 hwfc_master_sel_field;
	enum is_mcsc_hwfc_format hwfc_fmt = 0;

	switch (fmt) {
	case DMA_INOUT_FORMAT_YUV422:
		switch (plane) {
		case 3:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 1;
			total_img_byte2 = img_resol >> 1;
			total_width_byte0 = width;
			total_width_byte1 = width >> 1;
			total_width_byte2 = width >> 1;
			break;
		case 2:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol;
			total_width_byte0 = width;
			total_width_byte1 = width;
			break;
		case 1:
			total_img_byte0 = img_resol << 1;
			total_width_byte0 = width << 1;
			break;
		default:
			err_hw("invalid hwfc plane (%d, %d, %dx%d)",
				fmt, plane, width, height);
			return;
		}

		hwfc_fmt = MCSC_HWFC_FMT_YUV444_YUV422;
		break;
	case DMA_INOUT_FORMAT_YUV420:
		switch (plane) {
		case 3:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 2;
			total_img_byte2 = img_resol >> 2;
			total_width_byte0 = width;
			total_width_byte1 = width >> 2;
			total_width_byte2 = width >> 2;
			break;
		case 2:
			total_img_byte0 = img_resol;
			total_img_byte1 = img_resol >> 1;
			total_width_byte0 = width;
			total_width_byte1 = width >> 1;
			break;
		default:
			err_hw("invalid hwfc plane (%d, %d, %dx%d)",
				fmt, plane, width, height);
			return;
		}

		hwfc_fmt = MCSC_HWFC_FMT_YUV420;
		break;
	default:
		err_hw("invalid hwfc format (%d, %d, %dx%d)",
			fmt, plane, width, height);
		return;
	}

	switch (output_id) {
	case MCSC_OUTPUT3:
		hwfc_config_image_reg = MCSC_R_YUV_HWFC_CONFIG_IMAGE_A;
		hwfc_format_field = MCSC_F_YUV_HWFC_FORMAT_A;
		hwfc_plane_field = MCSC_F_YUV_HWFC_PLANE_A;
		hwfc_id0_field = MCSC_F_YUV_HWFC_ID0_A;
		hwfc_id1_field = MCSC_F_YUV_HWFC_ID1_A;
		hwfc_id2_field = MCSC_F_YUV_HWFC_ID2_A;
		hwfc_config_image = MCSC_R_YUV_HWFC_CONFIG_IMAGE_A;
		hwfc_total_image_byte0_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE0_A;
		hwfc_total_image_byte1_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE1_A;
		hwfc_total_image_byte2_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE2_A;
		hwfc_total_width_byte0_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE0_A;
		hwfc_total_width_byte1_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE1_A;
		hwfc_total_width_byte2_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE2_A;
		hwfc_master_sel_reg = MCSC_R_YUV_HWFC_MASTER_SEL;
		hwfc_master_sel_field = MCSC_F_YUV_HWFC_MASTER_SEL_A;
		break;
	case MCSC_OUTPUT4:
		hwfc_config_image_reg = MCSC_R_YUV_HWFC_CONFIG_IMAGE_B;
		hwfc_format_field = MCSC_F_YUV_HWFC_FORMAT_B;
		hwfc_plane_field = MCSC_F_YUV_HWFC_PLANE_B;
		hwfc_id0_field = MCSC_F_YUV_HWFC_ID0_B;
		hwfc_id1_field = MCSC_F_YUV_HWFC_ID1_B;
		hwfc_id2_field = MCSC_F_YUV_HWFC_ID2_B;
		hwfc_config_image = MCSC_R_YUV_HWFC_CONFIG_IMAGE_B;
		hwfc_total_image_byte0_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE0_B;
		hwfc_total_image_byte1_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE1_B;
		hwfc_total_image_byte2_reg =
			MCSC_R_YUV_HWFC_TOTAL_IMAGE_BYTE2_B;
		hwfc_total_width_byte0_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE0_B;
		hwfc_total_width_byte1_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE1_B;
		hwfc_total_width_byte2_reg =
			MCSC_R_YUV_HWFC_TOTAL_WIDTH_BYTE2_B;
		hwfc_master_sel_reg = MCSC_R_YUV_HWFC_MASTER_SEL;
		hwfc_master_sel_field = MCSC_F_YUV_HWFC_MASTER_SEL_B;
		break;
	case MCSC_OUTPUT0:
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	default:
		return;
	}

	val = is_hw_get_reg(base_addr, &mcsc_regs[hwfc_config_image_reg]);
	/* format */
	val = is_hw_set_field_value(val, &mcsc_fields[hwfc_format_field], hwfc_fmt);
	/* plane */
	val = is_hw_set_field_value(val, &mcsc_fields[hwfc_plane_field], plane);
	/* dma idx */
	val = is_hw_set_field_value(val, &mcsc_fields[hwfc_id0_field], dma_idx);
	val = is_hw_set_field_value(val, &mcsc_fields[hwfc_id1_field], dma_idx+2);
	val = is_hw_set_field_value(val, &mcsc_fields[hwfc_id2_field], dma_idx+4);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_config_image], val);
	/* total pixel/byte */
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_image_byte0_reg], total_img_byte0);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_image_byte1_reg], total_img_byte1);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_image_byte2_reg], total_img_byte2);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_width_byte0_reg], total_width_byte0);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_width_byte1_reg], total_width_byte1);
	is_hw_set_reg(base_addr, &mcsc_regs[hwfc_total_width_byte2_reg], total_width_byte2);
	/* WDMA master */
	is_hw_set_field(base_addr, &mcsc_regs[hwfc_master_sel_reg],
		&mcsc_fields[hwfc_master_sel_field], output_id);
}

u32 is_scaler_get_hwfc_idx_bin(void __iomem *base_addr, u32 output_id)
{
	u32 hwfc_region_idx_bin_field;
	u32 hwfc_region_idx_bin_reg = MCSC_R_YUV_HWFC_REGION_IDX_BIN;

	switch (output_id) {
	case MCSC_OUTPUT3:
		hwfc_region_idx_bin_field = MCSC_F_YUV_HWFC_REGION_IDX_BIN_A;
		break;
	case MCSC_OUTPUT4:
		hwfc_region_idx_bin_field = MCSC_F_YUV_HWFC_REGION_IDX_BIN_B;
		break;
	case MCSC_OUTPUT0:
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[hwfc_region_idx_bin_reg],
		&mcsc_fields[hwfc_region_idx_bin_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_hwfc_idx_bin);

u32 is_scaler_get_hwfc_cur_idx(void __iomem *base_addr, u32 output_id)
{
	u32 hwfc_curr_region_field;
	u32 hwfc_curr_region_reg = MCSC_R_YUV_HWFC_CURR_REGION;


	switch (output_id) {
	case MCSC_OUTPUT3:
		hwfc_curr_region_field = MCSC_F_YUV_HWFC_CURR_REGION_A;
		break;
	case MCSC_OUTPUT4:
		hwfc_curr_region_field = MCSC_F_YUV_HWFC_CURR_REGION_B;
		break;
	case MCSC_OUTPUT0:
	case MCSC_OUTPUT1:
	case MCSC_OUTPUT2:
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[hwfc_curr_region_reg],
		&mcsc_fields[hwfc_curr_region_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_hwfc_cur_idx);

/* for DJAG */
void is_scaler_set_djag_enable(void __iomem *base_addr, u32 djag_enable)
{
	u32 reg_val = 0;

	reg_val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CTRL]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_ENABLE], djag_enable);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_ENABLE], djag_enable);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_EZPOST_ENABLE], djag_enable);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CTRL], reg_val);
}

void is_scaler_set_djag_input_size(void __iomem *base_addr, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_INPUT_IMG_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_INPUT_IMG_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_IMG_SIZE], reg_val);
}

void is_scaler_set_djag_src_size(void __iomem *base_addr, u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_SRC_HPOS], pos_x);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_SRC_VPOS], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_SRC_POS], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_SRC_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_SRC_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_SRC_SIZE], reg_val);
}

void is_scaler_set_djag_hf_enable(void __iomem *base_addr, u32 hf_enable, u32 hf_scaler_enable)
{
	u32 reg_val = 0;
	u32 djag_enable = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CTRL],
			&mcsc_fields[MCSC_F_YUV_DJAG_ENABLE]);
	if (hf_enable && !djag_enable)
		err_hw("To enable HF block, DJAG should be enabled. Force HF block off\n");
	else
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_ENABLE_A],
			hf_enable);

	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_CTRL], reg_val);
}

void is_scaler_set_djag_hf_cfg(void __iomem *base_addr, struct hf_cfg_by_ni *hf_cfg)
{
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_WEIGHT], hf_cfg->hf_weight);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_djag_hf_cfg);

static void is_scaler_set_djag_hf_img_size(void __iomem *base_addr, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_INPUT_IMG_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_INPUT_IMG_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_IMG_SIZE], reg_val);
}

static void is_scaler_set_djag_hf_radial_center(void __iomem *base_addr, u32 pos_x, u32 pos_y)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_CENTER_X], pos_x);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_CENTER_Y], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_CENTER], reg_val);
}

static void is_scaler_set_djag_hf_radial_binning(void __iomem *base_addr, u32 pos_x, u32 pos_y)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_BINNING_X], pos_x);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_BINNING_Y], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_BINNING], reg_val);
}

static void is_scaler_set_djag_hf_radial_biquad_factor(void __iomem *base_addr,
	u32 factor_a, u32 factor_b)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_BIQUAD_FACTOR_A],
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_BIQUAD_FACTOR_A], factor_a);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_BIQUAD_FACTOR_B],
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_BIQUAD_FACTOR_B], factor_b);
}

static void is_scaler_set_djag_hf_radial_gain(void __iomem *base_addr,
	u32 enable, u32 increment)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_GAIN_ENABLE],
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_GAIN_ENABLE], enable);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_GAIN_INCREMENT],
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_GAIN_INCREMENT], increment);
}

static void is_scaler_set_djag_hf_radial_biquad_scal_shift_adder(void __iomem *base_addr,
	u32 value)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_RECOM_RADIAL_BIQUAD_SCAL_SHIFT_ADDER],
		&mcsc_fields[MCSC_F_YUV_DJAG_RECOM_RADIAL_BIQUAD_SCAL_SHIFT_ADDER], value);
}

#if IS_ENABLED(USE_DJAG_COEFF_CONFIG)
static void is_scaler_set_djag_scaler_v_coef(void __iomem *base_addr,
	const struct djag_sc_v_coef_cfg *v_coef_cfg)
{
	int index;
	u32 reg_val;

	for (index = 0; index <= 8; index++) {
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_V_COEFF_0_0 + (4 * index)],
			v_coef_cfg->v_coef_a[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_V_COEFF_0_1 + (4 * index)],
			v_coef_cfg->v_coef_b[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_V_COEFF_0_01 + (2 * index)], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_V_COEFF_0_2 + (4 * index)],
			v_coef_cfg->v_coef_c[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_V_COEFF_0_3 + (4 * index)],
			v_coef_cfg->v_coef_d[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_V_COEFF_0_23 + (2 * index)], reg_val);
	}
}

static void is_scaler_set_djag_scaler_h_coef(void __iomem *base_addr,
	const struct djag_sc_h_coef_cfg *h_coef_cfg)
{
	int index;
	u32 reg_val;

	for (index = 0; index <= 8; index++) {
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_0 + (8 * index)],
			h_coef_cfg->h_coef_a[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_1 + (8 * index)],
			h_coef_cfg->h_coef_b[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_H_COEFF_0_01 + (4 * index)], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_2 + (8 * index)],
			h_coef_cfg->h_coef_c[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_3 + (8 * index)],
			h_coef_cfg->h_coef_d[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_H_COEFF_0_23 + (4 * index)], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_4 + (8 * index)],
			h_coef_cfg->h_coef_e[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_5 + (8 * index)],
			h_coef_cfg->h_coef_f[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_H_COEFF_0_45 + (4 * index)], reg_val);
		reg_val = 0;
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_6 + (8 * index)],
			h_coef_cfg->h_coef_g[index]);
		reg_val = is_hw_set_field_value(reg_val,
			&mcsc_fields[MCSC_F_YUV_DJAG_SC_H_COEFF_0_7 + (8 * index)],
			h_coef_cfg->h_coef_h[index]);
		is_hw_set_reg(base_addr,
			&mcsc_regs[MCSC_R_YUV_DJAG_SC_H_COEFF_0_67 + (4 * index)], reg_val);
	}
}
#endif

void is_scaler_set_djag_dst_size(void __iomem *base_addr, u32 width, u32 height)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_DST_HSIZE], width);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_PS_DST_VSIZE], height);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_DST_SIZE], reg_val);

	/* High frequency component size should be same with pre-scaled image size in DJAG. */
	is_scaler_set_djag_hf_img_size(base_addr, width, height);
}

void is_scaler_set_djag_scaling_ratio(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_H_RATIO],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_H_RATIO], hratio);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_V_RATIO],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_V_RATIO], vratio);
}

void is_scaler_set_djag_init_phase_offset(void __iomem *base_addr, u32 h_offset, u32 v_offset)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_H_INIT_PHASE_OFFSET],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_H_INIT_PHASE_OFFSET], h_offset);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_V_INIT_PHASE_OFFSET],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_V_INIT_PHASE_OFFSET], v_offset);
}

void is_scaler_set_djag_round_mode(void __iomem *base_addr, u32 round_enable)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_ROUND_MODE],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_ROUND_MODE], round_enable);
}

void is_scaler_get_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	*pre_dst_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_STRIP_PRE_DST_SIZE],
			&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_PRE_DST_SIZE_H]);
	*start_pos_h = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_STRIP_IN_START_POS],
			&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_IN_START_POS_H]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_djag_strip_config);

void is_scaler_set_djag_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_STRIP_PRE_DST_SIZE],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_PRE_DST_SIZE_H], pre_dst_h);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_PS_STRIP_IN_START_POS],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_IN_START_POS_H], start_pos_h);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_djag_strip_config);

void is_scaler_set_djag_tuning_param(void __iomem *base_addr,
	const struct djag_scaling_ratio_depended_config *djag_tune)
{
	u32 reg_val = 0;

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_XFILTER_DEJAGGING_WEIGHT0],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_dejagging_weight0);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_XFILTER_DEJAGGING_WEIGHT1],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_dejagging_weight1);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_XFILTER_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.xfilter_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CENTER_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.center_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DIAGONAL_HF_BOOST_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.diagonal_hf_boost_weight);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CENTER_WEIGHTED_MEAN_WEIGHT],
		djag_tune->xfilter_dejagging_coeff_cfg.center_weighted_mean_weight);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_XFILTER_DEJAGGING_COEFF], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_1X5_MATCHING_SAD],
		djag_tune->thres_1x5_matching_cfg.thres_1x5_matching_sad);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_1X5_ABSHF],
		djag_tune->thres_1x5_matching_cfg.thres_1x5_abshf);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_THRES_1X5_MATCHING], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_SHOOTING_LLCRR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_llcrr);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_SHOOTING_LCR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_lcr);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_SHOOTING_NEIGHBOR],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_neighbor);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_THRES_SHOOTING_DETECT_0], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_SHOOTING_UUCDD],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_uucdd);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_THRES_SHOOTING_UCD],
		djag_tune->thres_shooting_detect_cfg.thres_shooting_ucd);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_MIN_MAX_WEIGHT],
		djag_tune->thres_shooting_detect_cfg.min_max_weight);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_THRES_SHOOTING_DETECT_1], reg_val);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_LFSR_SEED_0],
		&mcsc_fields[MCSC_F_YUV_DJAG_LFSR_SEED_0], djag_tune->lfsr_seed_cfg.lfsr_seed_0);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_LFSR_SEED_1],
		&mcsc_fields[MCSC_F_YUV_DJAG_LFSR_SEED_1], djag_tune->lfsr_seed_cfg.lfsr_seed_1);
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_LFSR_SEED_2],
		&mcsc_fields[MCSC_F_YUV_DJAG_LFSR_SEED_2], djag_tune->lfsr_seed_cfg.lfsr_seed_2);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_0],
		djag_tune->dither_cfg.dither_value[0]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_1],
		djag_tune->dither_cfg.dither_value[1]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_2],
		djag_tune->dither_cfg.dither_value[2]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_3],
		djag_tune->dither_cfg.dither_value[3]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_4],
		djag_tune->dither_cfg.dither_value[4]);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_DITHER_VALUE_04], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_5],
		djag_tune->dither_cfg.dither_value[5]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_6],
		djag_tune->dither_cfg.dither_value[6]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_7],
		djag_tune->dither_cfg.dither_value[7]);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_VALUE_8],
		djag_tune->dither_cfg.dither_value[8]);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_DITHER_VALUE_58], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_SAT_CTRL],
		djag_tune->dither_cfg.sat_ctrl);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_THRES],
		djag_tune->dither_cfg.dither_thres);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_DITHER_THRES], reg_val);

	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CP_HF_THRES],
		&mcsc_fields[MCSC_F_YUV_DJAG_CP_HF_THRES], djag_tune->cp_cfg.cp_hf_thres);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CP_ARBI_MAX_COV_OFFSET],
		djag_tune->cp_cfg.cp_arbi_max_cov_offset);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CP_ARBI_MAX_COV_SHIFT],
		djag_tune->cp_cfg.cp_arbi_max_cov_shift);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CP_ARBI_DENOM],
		djag_tune->cp_cfg.cp_arbi_denom);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_CP_ARBI_MODE],
		djag_tune->cp_cfg.cp_arbi_mode);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CP_ARBI], reg_val);
#if IS_ENABLED(USE_DJAG_COEFF_CONFIG)
	if (djag_tune->djag_sc_coef.use_djag_sc_coef) {
		is_scaler_set_djag_scaler_h_coef(base_addr,
			&djag_tune->djag_sc_coef.djag_sc_h_coef);
		is_scaler_set_djag_scaler_v_coef(base_addr,
			&djag_tune->djag_sc_coef.djag_sc_v_coef);
	}
#endif
}

void is_scaler_set_djag_dither_wb(void __iomem *base_addr, struct djag_wb_thres_cfg *djag_wb, u32 wht, u32 blk)
{
	u32 reg_val;

	if (!djag_wb)
		return;

	reg_val = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_DITHER_WB]);

	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_WHITE_LEVEL], wht);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_BLACK_LEVEL], blk);
	reg_val = is_hw_set_field_value(reg_val, &mcsc_fields[MCSC_F_YUV_DJAG_DITHER_WB_THRES],
		djag_wb->dither_wb_thres);
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_DITHER_WB], reg_val);
}

/* for CAC */
void is_scaler_set_cac_enable(void __iomem *base_addr, u32 en)
{
	/* not supported */
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_cac_enable);

void is_scaler_set_cac_map_crt_thr(void __iomem *base_addr, struct cac_cfg_by_ni *cfg)
{
	/* not supported */
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_cac_map_crt_thr);

/* LFRO : Less Fast Read Out */
void is_scaler_set_lfro_mode_enable(void __iomem *base_addr, u32 hw_id, u32 lfro_enable, u32 lfro_total_fnum)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_MAIN_CTRL_FRO_EN],
		&mcsc_fields[MCSC_F_YUV_FRO_ENABLE], lfro_enable);
}

u32 is_scaler_get_stripe_align(u32 sbwc_type)
{
	return sbwc_type ? MCSC_COMP_BLOCK_WIDTH : MCSC_WIDTH_ALIGN;
}

/* for Strip */
u32 is_scaler_get_djag_strip_enable(void __iomem *base_addr, u32 output_id)
{
	return is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CTRL],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_ENABLE]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_djag_strip_enable);

void is_scaler_set_djag_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_YUV_DJAG_CTRL],
		&mcsc_fields[MCSC_F_YUV_DJAG_PS_STRIP_ENABLE], enable);
}

u32 is_scaler_get_poly_strip_enable(void __iomem *base_addr, u32 output_id)
{
	u32 sc_ctrl_reg, sc_strip_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_strip_enable_field = MCSC_F_YUV_POLY_SC0_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_strip_enable_field = MCSC_F_YUV_POLY_SC1_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_strip_enable_field = MCSC_F_YUV_POLY_SC2_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_strip_enable_field = MCSC_F_YUV_POLY_SC3_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_strip_enable_field = MCSC_F_YUV_POLY_SC4_STRIP_ENABLE;
		break;
	default:
		return -EINVAL;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_strip_enable_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_poly_strip_enable);

void is_scaler_set_poly_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	u32 sc_ctrl_reg, sc_scrip_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_scrip_enable_field = MCSC_F_YUV_POLY_SC0_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_scrip_enable_field = MCSC_F_YUV_POLY_SC1_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_scrip_enable_field = MCSC_F_YUV_POLY_SC2_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_scrip_enable_field = MCSC_F_YUV_POLY_SC3_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_scrip_enable_field = MCSC_F_YUV_POLY_SC4_STRIP_ENABLE;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_scrip_enable_field], enable);
}

void is_scaler_get_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	u32 sc_strip_pre_dst_size_reg, sc_strip_pre_dst_size_h_field;
	u32 sc_strip_in_start_pos_reg, sc_strip_in_start_pos_h_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC0_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC0_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC0_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC0_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT1:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC1_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC1_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC1_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC1_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT2:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC2_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC2_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC2_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC2_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT3:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC3_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC3_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC3_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC3_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT4:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC4_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC4_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC4_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC4_STRIP_IN_START_POS_H;
		break;
	default:
		*pre_dst_h = 0;
		*start_pos_h = 0;
		return;
	}

	*pre_dst_h = is_hw_get_field(base_addr,
		&mcsc_regs[sc_strip_pre_dst_size_reg],
		&mcsc_fields[sc_strip_pre_dst_size_h_field]);
	*start_pos_h = is_hw_get_field(base_addr,
		&mcsc_regs[sc_strip_in_start_pos_reg],
		&mcsc_fields[sc_strip_in_start_pos_h_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_poly_strip_config);

void is_scaler_set_poly_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	u32 sc_strip_pre_dst_size_reg, sc_strip_pre_dst_size_h_field;
	u32 sc_strip_in_start_pos_reg, sc_strip_in_start_pos_h_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC0_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC0_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC0_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC0_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT1:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC1_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC1_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC1_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC1_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT2:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC2_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC2_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC2_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC2_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT3:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC3_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC3_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC3_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC3_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT4:
		sc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POLY_SC4_STRIP_PRE_DST_SIZE;
		sc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POLY_SC4_STRIP_PRE_DST_SIZE_H;
		sc_strip_in_start_pos_reg =
			MCSC_R_YUV_POLY_SC4_STRIP_IN_START_POS;
		sc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POLY_SC4_STRIP_IN_START_POS_H;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_strip_pre_dst_size_reg],
		&mcsc_fields[sc_strip_pre_dst_size_h_field], pre_dst_h);
	is_hw_set_field(base_addr, &mcsc_regs[sc_strip_in_start_pos_reg],
		&mcsc_fields[sc_strip_in_start_pos_h_field], start_pos_h);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_poly_strip_config);

u32 is_scaler_get_poly_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	u32 sc_ctrl_reg, sc_out_crop_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC0_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC1_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC2_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC3_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC4_OUT_CROP_ENABLE;
		break;
	default:
		return -EINVAL;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_out_crop_enable_field]);
}

void is_scaler_set_poly_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	u32 sc_ctrl_reg, sc_out_crop_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC0_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC0_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC1_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC1_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC2_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC2_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT3:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC3_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC3_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT4:
		sc_ctrl_reg = MCSC_R_YUV_POLY_SC4_CTRL;
		sc_out_crop_enable_field =
			MCSC_F_YUV_POLY_SC4_OUT_CROP_ENABLE;
		break;
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[sc_ctrl_reg],
		&mcsc_fields[sc_out_crop_enable_field], enable);
}

void is_scaler_get_poly_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 sc_out_crop_size_reg;
	u32 sc_out_crop_size_h_field, sc_out_crop_size_v_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC0_OUT_CROP_SIZE;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT1:
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC1_OUT_CROP_SIZE;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT2:
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC2_OUT_CROP_SIZE;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT3:
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC3_OUT_CROP_SIZE;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT4:
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC4_OUT_CROP_SIZE;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_SIZE_V;
		break;
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr,
		&mcsc_regs[sc_out_crop_size_reg],
		&mcsc_fields[sc_out_crop_size_h_field]);
	*height = is_hw_get_field(base_addr,
		&mcsc_regs[sc_out_crop_size_reg],
		&mcsc_fields[sc_out_crop_size_v_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_poly_out_crop_size);

void is_scaler_set_poly_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val;
	u32 sc_out_crop_pos_h_field, sc_out_crop_pos_v_field;
	u32 sc_out_crop_size_h_field, sc_out_crop_size_v_field;
	u32 sc_out_crop_pos_reg, sc_out_crop_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		sc_out_crop_pos_h_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_POS_H;
		sc_out_crop_pos_v_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_POS_V;
		sc_out_crop_pos_reg = MCSC_R_YUV_POLY_SC0_OUT_CROP_POS;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC0_OUT_CROP_SIZE_V;
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC0_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT1:
		sc_out_crop_pos_h_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_POS_H;
		sc_out_crop_pos_v_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_POS_V;
		sc_out_crop_pos_reg = MCSC_R_YUV_POLY_SC1_OUT_CROP_POS;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC1_OUT_CROP_SIZE_V;
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC1_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT2:
		sc_out_crop_pos_h_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_POS_H;
		sc_out_crop_pos_v_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_POS_V;
		sc_out_crop_pos_reg = MCSC_R_YUV_POLY_SC2_OUT_CROP_POS;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC2_OUT_CROP_SIZE_V;
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC2_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT3:
		sc_out_crop_pos_h_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_POS_H;
		sc_out_crop_pos_v_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_POS_V;
		sc_out_crop_pos_reg = MCSC_R_YUV_POLY_SC3_OUT_CROP_POS;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC3_OUT_CROP_SIZE_V;
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC3_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT4:
		sc_out_crop_pos_h_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_POS_H;
		sc_out_crop_pos_v_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_POS_V;
		sc_out_crop_pos_reg = MCSC_R_YUV_POLY_SC4_OUT_CROP_POS;
		sc_out_crop_size_h_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_SIZE_H;
		sc_out_crop_size_v_field = MCSC_F_YUV_POLY_SC4_OUT_CROP_SIZE_V;
		sc_out_crop_size_reg = MCSC_R_YUV_POLY_SC4_OUT_CROP_SIZE;
		break;
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_out_crop_pos_h_field], pos_x);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_out_crop_pos_v_field], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[sc_out_crop_pos_reg], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_out_crop_size_h_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[sc_out_crop_size_v_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[sc_out_crop_size_reg], reg_val);

}
KUNIT_EXPORT_SYMBOL(is_scaler_set_poly_out_crop_size);

u32 is_scaler_get_post_strip_enable(void __iomem *base_addr, u32 output_id)
{
	u32 pc_ctrl_reg, pc_strip_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC0_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC1_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC2_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[pc_ctrl_reg],
		&mcsc_fields[pc_strip_enable_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_post_strip_enable);

void is_scaler_set_post_strip_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	u32 pc_ctrl_reg, pc_strip_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC0_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC1_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_strip_enable_field = MCSC_F_YUV_POST_PC2_STRIP_ENABLE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_ctrl_reg],
		&mcsc_fields[pc_strip_enable_field], enable);
}

void is_scaler_get_post_strip_config(void __iomem *base_addr, u32 output_id, u32 *pre_dst_h, u32 *start_pos_h)
{
	u32 pc_strip_pre_dst_size_reg, pc_strip_pre_dst_size_h_field;
	u32 pc_strip_in_start_pos_reg, pc_strip_in_start_pos_h_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC0_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC0_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC0_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC0_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT1:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC1_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC1_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC1_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC1_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT2:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC2_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC2_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC2_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC2_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		*pre_dst_h = 0;
		*start_pos_h = 0;
		return;
	}

	*pre_dst_h = is_hw_get_field(base_addr,
		&mcsc_regs[pc_strip_pre_dst_size_reg],
		&mcsc_fields[pc_strip_pre_dst_size_h_field]);
	*start_pos_h = is_hw_get_field(base_addr,
		&mcsc_regs[pc_strip_in_start_pos_reg],
		&mcsc_fields[pc_strip_in_start_pos_h_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_post_strip_config);

void is_scaler_set_post_strip_config(void __iomem *base_addr, u32 output_id, u32 pre_dst_h, u32 start_pos_h)
{
	u32 pc_strip_pre_dst_size_reg, pc_strip_pre_dst_size_h_field;
	u32 pc_strip_in_start_pos_reg, pc_strip_in_start_pos_h_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC0_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC0_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC0_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC0_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT1:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC1_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC1_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC1_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC1_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT2:
		pc_strip_pre_dst_size_reg =
			MCSC_R_YUV_POST_PC2_STRIP_PRE_DST_SIZE;
		pc_strip_pre_dst_size_h_field =
			MCSC_F_YUV_POST_PC2_STRIP_PRE_DST_SIZE_H;
		pc_strip_in_start_pos_reg =
			MCSC_R_YUV_POST_PC2_STRIP_IN_START_POS;
		pc_strip_in_start_pos_h_field =
			MCSC_F_YUV_POST_PC2_STRIP_IN_START_POS_H;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr,
		&mcsc_regs[pc_strip_pre_dst_size_reg],
		&mcsc_fields[pc_strip_pre_dst_size_h_field], pre_dst_h);
	is_hw_set_field(base_addr,
		&mcsc_regs[pc_strip_in_start_pos_reg],
		&mcsc_fields[pc_strip_in_start_pos_h_field], start_pos_h);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_post_strip_config);

u32 is_scaler_get_post_out_crop_enable(void __iomem *base_addr, u32 output_id)
{
	u32 pc_ctrl_reg, pc_out_crop_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC0_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC1_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC2_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return 0;
	}

	return is_hw_get_field(base_addr, &mcsc_regs[pc_ctrl_reg],
		&mcsc_fields[pc_out_crop_enable_field]);
}

void is_scaler_set_post_out_crop_enable(void __iomem *base_addr, u32 output_id, u32 enable)
{
	u32 pc_ctrl_reg, pc_out_crop_enable_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC0_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC0_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT1:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC1_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC1_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT2:
		pc_ctrl_reg = MCSC_R_YUV_POST_PC2_CTRL;
		pc_out_crop_enable_field = MCSC_F_YUV_POST_PC2_OUT_CROP_ENABLE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	is_hw_set_field(base_addr, &mcsc_regs[pc_ctrl_reg],
		&mcsc_fields[pc_out_crop_enable_field], enable);
}

void is_scaler_get_post_out_crop_size(void __iomem *base_addr, u32 output_id, u32 *width, u32 *height)
{
	u32 pc_out_crop_size_reg;
	u32 pc_out_crop_size_h_field, pc_out_crop_size_v_field;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC0_OUT_CROP_SIZE;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC0_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC0_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT1:
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC1_OUT_CROP_SIZE;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC1_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC1_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT2:
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC2_OUT_CROP_SIZE;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC2_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC2_OUT_CROP_SIZE_V;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		*width = 0;
		*height = 0;
		return;
	}

	*width = is_hw_get_field(base_addr, &mcsc_regs[pc_out_crop_size_reg],
		&mcsc_fields[pc_out_crop_size_h_field]);
	*height = is_hw_get_field(base_addr, &mcsc_regs[pc_out_crop_size_reg],
		&mcsc_fields[pc_out_crop_size_v_field]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_post_out_crop_size);

void is_scaler_set_post_out_crop_size(void __iomem *base_addr, u32 output_id,
	u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	u32 reg_val;
	u32 pc_out_crop_pos_h_field, pc_out_crop_pos_v_field;
	u32 pc_out_crop_size_h_field, pc_out_crop_size_v_field;
	u32 pc_out_crop_pos_reg, pc_out_crop_size_reg;

	switch (output_id) {
	case MCSC_OUTPUT0:
		pc_out_crop_pos_h_field = MCSC_F_YUV_POST_PC0_OUT_CROP_POS_H;
		pc_out_crop_pos_v_field = MCSC_F_YUV_POST_PC0_OUT_CROP_POS_V;
		pc_out_crop_pos_reg = MCSC_R_YUV_POST_PC0_OUT_CROP_POS;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC0_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC0_OUT_CROP_SIZE_V;
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC0_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT1:
		pc_out_crop_pos_h_field = MCSC_F_YUV_POST_PC1_OUT_CROP_POS_H;
		pc_out_crop_pos_v_field = MCSC_F_YUV_POST_PC1_OUT_CROP_POS_V;
		pc_out_crop_pos_reg = MCSC_R_YUV_POST_PC1_OUT_CROP_POS;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC1_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC1_OUT_CROP_SIZE_V;
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC1_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT2:
		pc_out_crop_pos_h_field = MCSC_F_YUV_POST_PC2_OUT_CROP_POS_H;
		pc_out_crop_pos_v_field = MCSC_F_YUV_POST_PC2_OUT_CROP_POS_V;
		pc_out_crop_pos_reg = MCSC_R_YUV_POST_PC2_OUT_CROP_POS;
		pc_out_crop_size_h_field = MCSC_F_YUV_POST_PC2_OUT_CROP_SIZE_H;
		pc_out_crop_size_v_field = MCSC_F_YUV_POST_PC2_OUT_CROP_SIZE_V;
		pc_out_crop_size_reg = MCSC_R_YUV_POST_PC2_OUT_CROP_SIZE;
		break;
	case MCSC_OUTPUT3:
	case MCSC_OUTPUT4:
		/* not support */
	default:
		return;
	}

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_out_crop_pos_h_field], pos_x);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_out_crop_pos_v_field], pos_y);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_out_crop_pos_reg], reg_val);

	reg_val = 0;
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_out_crop_size_h_field], width);
	reg_val = is_hw_set_field_value(reg_val,
		&mcsc_fields[pc_out_crop_size_v_field], height);
	is_hw_set_reg(base_addr, &mcsc_regs[pc_out_crop_size_reg], reg_val);
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_post_out_crop_size);

/* HF: High Frequency */
int is_scaler_set_hf_config(void __iomem *base_addr, u32 hw_id,
	bool hf_enable,
	struct param_mcs_output *hf_param,
	struct scaler_coef_cfg *sc_coef, enum exynos_sensor_position sensor_position,
	u32 *payload_size)
{
	u32 y_stride = hf_param->dma_stride_y;

	/* Read HF from MCSC RDMA0 */
	is_scaler_set_rdma_enable(base_addr, hw_id, hf_enable);
	is_scaler_set_rdma_format(base_addr, hw_id, hf_param->dma_format);
	is_scaler_set_rdma_sbwc_config(base_addr, hf_param, &y_stride, payload_size);
	is_scaler_set_rdma_size(base_addr, hf_param->width, hf_param->height);
	is_scaler_set_rdma_stride(base_addr, y_stride, 0);
	is_scaler_set_rdma_sbwc_header_stride(base_addr, hf_param->full_output_width);
	/* Enable DJAG HF Recomposition */
	is_scaler_set_djag_hf_enable(base_addr, hf_enable, 0);

	/* todo for HF */
	is_scaler_set_djag_hf_radial_center(base_addr, 0, 0);
	is_scaler_set_djag_hf_radial_binning(base_addr, 0, 0);
	is_scaler_set_djag_hf_radial_biquad_factor(base_addr, 0, 0);
	is_scaler_set_djag_hf_radial_gain(base_addr, 0, 0);
	is_scaler_set_djag_hf_radial_biquad_scal_shift_adder(base_addr, 0);

	return 0;
}
KUNIT_EXPORT_SYMBOL(is_scaler_set_hf_config);

int is_scaler_set_votf_config(void __iomem *base_addr, u32 hw_id, bool votf_enable)
{
	is_scaler_set_rdma_votf_enable(base_addr, votf_enable);

	return 0;
}

KUNIT_EXPORT_SYMBOL(is_scaler_set_votf_config);

void is_scaler_clear_intr_src(void __iomem *base_addr, u32 hw_id, u32 status)
{
	is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_INT_REQ_INT0_CLEAR], status);
}

u32 is_scaler_get_intr_mask(void __iomem *base_addr, u32 hw_id)
{
	int ret;

	ret = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_INT_REQ_INT0_ENABLE]);

	return ~ret;
}

u32 is_scaler_get_intr_status(void __iomem *base_addr, u32 hw_id)
{
	u32 int0;

	int0 = is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_INT_REQ_INT0]);

	dbg_hw(2, "%s : INT0(0x%x)", __func__, int0);

	return int0;
}

u32 is_scaler_handle_extended_intr(u32 status)
{
	return 0;
}

u32 is_scaler_get_version(void __iomem *base_addr)
{
	return is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_IP_VERSION]);
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_version);

u32 is_scaler_get_idle_status(void __iomem *base_addr, u32 hw_id)
{
	int ret;

	ret = is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_IDLENESS_STATUS],
		&mcsc_fields[MCSC_F_IDLENESS_STATUS]);

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_scaler_get_idle_status);

void is_scaler_dump(void __iomem *base_addr)
{
	info_hw("MCSC ver 10.0");

	is_hw_dump_regs(base_addr, mcsc_regs, MCSC_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(is_scaler_dump);
