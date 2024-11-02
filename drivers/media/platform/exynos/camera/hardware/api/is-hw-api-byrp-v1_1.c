// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * byrp HW control APIs
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-byrp.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-byrp-v1_1.h"

#define HBLANK_CYCLE			0x2D
#define BYRP_LUT_REG_CNT		1650 /* DRC */

#define BYRP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &byrp_regs[R], &byrp_fields[F], val)
#define BYRP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &byrp_regs[R], &byrp_fields[F], val)
#define BYRP_SET_R(base, R, val) \
	is_hw_set_reg(base, &byrp_regs[R], val)
#define BYRP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &byrp_regs[R], val)
#define BYRP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &byrp_fields[F], val)

#define BYRP_GET_F(base, R, F) \
	is_hw_get_field(base, &byrp_regs[R], &byrp_fields[F])
#define BYRP_GET_R(base, R) \
	is_hw_get_reg(base, &byrp_regs[R])
#define BYRP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &byrp_regs[R])
#define BYRP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &byrp_fields[F])

unsigned int byrp_hw_is_occurred(unsigned int status, enum byrp_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_BYRP_FRAME_START_INT;
		break;
	case INTR_FRAME_CINROW:
		mask = 1 << INTR0_BYRP_ROW_COL_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_BYRP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_BYRP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_BYRP_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = BYRP_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int byrp_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;

	BYRP_SET_R(base, BYRP_R_SW_RESET, 0x1);

	/* Changed from 1 to 0 when its operation finished */
	while (BYRP_GET_R(base, BYRP_R_SW_RESET)) {
		if (reset_count > BYRP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	return 0;
}

void byrp_hw_s_init(void __iomem *base, u32 set_id)
{
	BYRP_SET_F(base, BYRP_R_IP_PROCESSING, BYRP_F_IP_PROCESSING, 0x1);

	/* Interrupt group enable for one frame */
	BYRP_SET_F(base, BYRP_R_CMDQ_QUE_CMD_L, BYRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xFF);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	BYRP_SET_F(base, BYRP_R_CMDQ_QUE_CMD_M, BYRP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	BYRP_SET_R(base, BYRP_R_CMDQ_ENABLE, 1);
}

void byrp_hw_s_dtp(void __iomem *base, u32 set_id, bool enable, u32 width, u32 height)
{
	u32 val;

	if (enable) {
		dbg_hw(1, "[API][%s] dtp color bar pattern is enabled!\n", __func__);

		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_BYPASS,
			BYRP_F_BYR_DTPNONBYR_BYPASS, !enable);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_MODE,
			BYRP_F_BYR_DTPNONBYR_MODE, 0x8); /* color bar pattern */

		/* guide value */
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_HIGHER_LIMIT, 16383);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_PATTERN_SIZE_X, width);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_PATTERN_SIZE_Y, height);

		val = 0;
		val = BYRP_SET_V(val, BYRP_F_BYR_DTPNONBYR_LAYER_WEIGHTS_0_0, 0x0);
		val = BYRP_SET_V(val, BYRP_F_BYR_DTPNONBYR_LAYER_WEIGHTS_0_1, 0x0);
		val = BYRP_SET_V(val, BYRP_F_BYR_DTPNONBYR_LAYER_WEIGHTS_0_2, 0x0);
		val = BYRP_SET_V(val, BYRP_F_BYR_DTPNONBYR_LAYER_WEIGHTS_0_3, 0x0);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_LAYER_WEIGHTS, val);

		val = 0;
		val = BYRP_SET_V(val, BYRP_F_BYR_DTPNONBYR_LAYER_WEIGHTS_0_4, 0x20);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_LAYER_WEIGHTS_1, val);

		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_BITTAGEOUT,
			BYRP_F_BYR_BITMASK_BITTAGEOUT, 0xE); /* dtp 14bit out */
	} else {
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_BYPASS,
			BYRP_F_BYR_DTPNONBYR_BYPASS, 0x1);
	}
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_dtp);

void byrp_hw_s_path(void __iomem *base, u32 set_id, bool mpc_en)
{
	u32 val = 0;

	val = BYRP_SET_V(val, BYRP_F_IP_USE_OTF_IN_FOR_PATH_0, 0x0); /* RDMA input */
	val = BYRP_SET_V(val, BYRP_F_IP_USE_OTF_OUT_FOR_PATH_0, 0x1); /* OTF output */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_IP_USE_OTF_PATH, val);

	/* SDC ON/OFF */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_SDC_ON,
		BYRP_F_CHAIN_SDC_ON, 0x0);

	/* Belows decied the data path between SDC and MPC
	 * chain input0 select
	 * 0: Disconnect path
	 * 1: CINFIFO (Not used)
	 * 2: RDMA (SDC)
	 * 3: RDMA (MPC)
	 *
	 * chain input1 select
	 * 0: Disconnect path
	 * 1: SDC/FORMATTER
	 * 2: MPC
	 * 3: Reserved
	 */
	if (mpc_en) {
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_INPUT_0_SELECT, 0x3);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_INPUT_1_SELECT, 0x2);
	} else { /* SDC */
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_INPUT_0_SELECT, 0x2);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_INPUT_1_SELECT, 0x1);
	}

	/* chain output_0 select
	 * 0: Disconnect path
	 * 1: BLCRGB
	 * 2: RDMA
	 * 3: Reserved
	 */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_OUTPUT_0_SELECT, 0x1);

	/* chain output_1 select
	 * 0: Disconnect path
	 * 1: WBG (Not used - hw bug)
	 * 2: CGRAS
	 * 3: Reserved
	 */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_OUTPUT_1_SELECT, 0x2);

	/* chain output_2 select
	 * 0: Disconnect path
	 * 1: CINFIFO
	 * 2: BCROPZSL
	 * 3: Reserved
	 */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_CHAIN_OUTPUT_2_SELECT, 0x2);
}

int byrp_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int0_all;
	u32 int1_all;
	u32 try_cnt = 0;

	idle = BYRP_GET_F(base, BYRP_R_IDLENESS_STATUS, BYRP_F_IDLENESS_STATUS);
	int0_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT0_STATUS);
	int1_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT1_STATUS);

	info_hw("[BYRP] idle status before disable (idle: %d, int0: 0x%X, int1: 0x%X)\n",
			idle, int0_all, int1_all);

	while (!idle) {
		idle = BYRP_GET_F(base, BYRP_R_IDLENESS_STATUS, BYRP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= BYRP_TRY_COUNT) {
			err_hw("[BYRP] timeout waiting idle - disable fail");
			byrp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT0_STATUS);
	int1_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT1_STATUS);

	info_hw("[BYRP] idle status after disable (total: %d, curr: %d, idle: %d, int0: 0x%X, int1: 0x%X)\n",
			idle, int0_all, int1_all);

	return ret;
}

void byrp_hw_dump(void __iomem *base)
{
	info_hw("[BYRP] SFR DUMP (v2.1)\n");

	is_hw_dump_regs(base, byrp_regs, BYRP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_dump);

void byrp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id, struct byrp_param_set *param_set)
{
	bool cinfifo_en, coutfifo_en, bitmask_en, mono_mode_en;
	u32 bit_in, bit_out;

	u32 width, height;
	u32 pixel_order;
	u32 non_byr_pattern;
	u32 seed;

	cinfifo_en = 0; /* Use only RDMA input */
	coutfifo_en = 1; /* OTF to RGBP */
	bitmask_en = 1; /* always enable */
	non_byr_pattern = 0;
	mono_mode_en = 0;

	bit_in = param_set->dma_input.msb + 1; /* valid bittage */
	bit_out = 14; /* evt0: 10b, evt1: 14b */

	width = param_set->dma_input.dma_crop_width;
	height = param_set->dma_input.dma_crop_height;
	pixel_order = param_set->dma_input.order;

	byrp_hw_s_cinfifo(base, set_id, cinfifo_en);
	byrp_hw_s_coutfifo(base, set_id, coutfifo_en);
	byrp_hw_s_common(base, set_id, param_set, bit_in);
	byrp_hw_s_int_mask(base, set_id);
	byrp_hw_s_path(base, set_id, false); /* TODO: MPC on/off is checked from cis */
	byrp_hw_s_bitmask(base, set_id, bitmask_en, bit_in, bit_out);
	byrp_hw_s_dtp(base, set_id, false, width, height);
	byrp_hw_s_pixel_order(base, set_id, pixel_order);
	byrp_hw_s_non_byr_pattern(base, set_id, non_byr_pattern);
	byrp_hw_s_mono_mode(base, set_id, mono_mode_en);
	byrp_hw_s_secure_id(base, set_id);
	byrp_hw_s_otf_hdr(base, set_id);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		byrp_hw_s_block_crc(base, set_id, seed);
}

void byrp_hw_s_cinfifo(void __iomem *base, u32 set_id, bool enable)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id),
		BYRP_R_BYR_CINFIFO_INTERVALS,
		BYRP_F_BYR_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	/* Do not support cinfifo in BYRP with no using strgen */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CINFIFO_ENABLE, enable);
}

void byrp_hw_s_coutfifo(void __iomem *base, u32 set_id, bool enable)
{
	u32 val = 0;

	BYRP_SET_F(base + GET_COREX_OFFSET(set_id),
		BYRP_R_BYR_COUTFIFO_INTERVALS,
		BYRP_F_BYR_COUTFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_COUTFIFO_ENABLE, enable);

	/* DEBUG */
	val = BYRP_SET_V(val, BYRP_F_BYR_COUTFIFO_DEBUG_EN, 1); /* stall cnt */
	val = BYRP_SET_V(val, BYRP_F_BYR_COUTFIFO_BACK_STALL_EN, 1); /* RGBP is ready */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_COUTFIFO_CONFIG, val);
}

static void byrp_hw_g_fmt_map(int dma_id, ulong *byr_fmt_map)
{
	switch (dma_id) {
	case BYRP_RDMA_IMG: /* 0,1,2,4,5,6,7,8,9,10,11,12,13,14 */
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_PACK)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID10)
				| BIT_MASK(DMA_FMT_U12BIT_PACK)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID12)
				| BIT_MASK(DMA_FMT_U14BIT_PACK)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
				) & IS_BAYER_FORMAT_MASK;
		break;
	case BYRP_RDMA_HDR: /* 0,1,2,4,5,6,7,8,9,10,11,12,13,14,24,25,26 */
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_PACK)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID10)
				| BIT_MASK(DMA_FMT_U12BIT_PACK)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID12)
				| BIT_MASK(DMA_FMT_U14BIT_PACK)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_S12BIT_PACK)
				| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
				) & IS_BAYER_FORMAT_MASK;
		break;
	case BYRP_WDMA_BYR: /* 8,9,10,12,13,14,24,25,26,28,29,30 */
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U10BIT_PACK)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID10)
				| BIT_MASK(DMA_FMT_U12BIT_PACK)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_PACK)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_S12BIT_PACK)
				| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_S14BIT_PACK)
				| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
				) & IS_BAYER_FORMAT_MASK;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		break;
	}
}

void byrp_hw_s_common(void __iomem *base, u32 set_id, struct byrp_param_set *param_set, u32 bit_in)
{
	u32 val;
	u32 img_fmt;

	/* 0: start frame asap, 1; start frame upon cinfifo vvalid rise */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id),
		BYRP_R_IP_USE_CINFIFO_NEW_FRAME_IN,
		BYRP_F_IP_USE_CINFIFO_NEW_FRAME_IN, 0x1);

	switch (bit_in) {
	case 8:
		img_fmt = BYRP_IMG_FMT_8BIT;
		break;
	case 10:
		img_fmt = BYRP_IMG_FMT_10BIT;
		break;
	case 12:
		img_fmt = BYRP_IMG_FMT_12BIT;
		break;
	case 14:
		img_fmt = BYRP_IMG_FMT_14BIT;
		break;
	case 9:
		img_fmt = BYRP_IMG_FMT_9BIT;
		break;
	case 11:
		img_fmt = BYRP_IMG_FMT_11BIT;
		break;
	case 13:
		img_fmt = BYRP_IMG_FMT_13BIT;
		break;
	default:
		err_hw("[BYRP] invalid image_fmt: %d -> 10b (default)", bit_in);
		img_fmt = BYRP_IMG_FMT_10BIT; /* default: 10b */
	}

	val = 0;
	/* 0: 8b, 1: 10b, 2: 12b, 3: 14b, 4: 9b, 5: 11b, 6: 13b */
	val = BYRP_SET_V(val, BYRP_F_BYR_FMT_IMG_FORMAT, img_fmt);
	val = BYRP_SET_V(val, BYRP_F_BYR_FMT_IMG_ALIGN, 0); /* 0: LSB, 1: MSB */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FMT_IMG, val);

	/* TODO: Need to check value */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FMT_LINE_GAP, 0x0);
}

void byrp_hw_s_int_mask(void __iomem *base, u32 set_id)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_INT_REQ_INT0_ENABLE,
		BYRP_F_INT_REQ_INT0_ENABLE, BYRP_INT0_EN_MASK);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_INT_REQ_INT1_ENABLE,
		BYRP_F_INT_REQ_INT1_ENABLE, BYRP_INT1_EN_MASK);
}

void byrp_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_SECU_CTRL_SEQID, BYRP_F_SECU_CTRL_SEQID, 0x0);
}

void byrp_hw_s_otf_hdr(void __iomem *base, u32 set_id)
{
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_RATE_CONTROL_ENABLE, 0x0);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_LINE_GAP_EN, 0x0);
}

void byrp_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0x0);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_dma_dump);

int byrp_hw_s_rdma_init(struct is_common_dma *dma, struct byrp_param_set *param_set,
	u32 enable, struct is_crop *dma_crop_cfg, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_input *dma_input;
	u32 comp_sbwc_en, comp_64b_align = 1, lossy_byte32num = 0;
	u32 dma_stride_1p, dma_header_stride_1p = 0, stride_align = 16;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 dma_width, dma_height;
	u32 format, en_votf, bus_info;
	int ret;
	u32 strip_enable;
	u32 full_width, full_height;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	switch (dma->id) {
	case BYRP_RDMA_IMG:
		dma_input = &param_set->dma_input;
		dma_width = dma_crop_cfg->w;
		dma_height = dma_crop_cfg->h;
		stride_align = 32;
		break;
	case BYRP_RDMA_HDR:
		dma_input = &param_set->dma_input_hdr;
		dma_width = dma_input->width;
		dma_height = dma_input->height;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return SET_ERROR;
	}

	full_width = (strip_enable) ? param_set->stripe_input.full_width :
			dma_input->width;
	full_height = dma_input->height;
	en_votf = dma_input->v_otf_enable;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL; /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true);

	if (comp_sbwc_en == 0) {
		dma_stride_1p =	is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
								full_width, stride_align, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		dma_stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_width,
								comp_64b_align, lossy_byte32num,
								BYRP_COMP_BLOCK_WIDTH,
								BYRP_COMP_BLOCK_HEIGHT);
		dma_header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(full_width, BYRP_COMP_BLOCK_WIDTH, stride_align) : 0;
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_size, dma_width, dma_height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, dma_stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, dma_header_stride_1p, 0);
		*payload_size = DIV_ROUND_UP(full_height, BYRP_COMP_BLOCK_HEIGHT) * dma_stride_1p;
		break;
	case 2: /* There is no SBWC lossy scenario in Pamir BYRP RDMA */
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	return ret;
}

int byrp_hw_s_wdma_init(struct is_common_dma *dma, struct byrp_param_set *param_set,
	void __iomem *base, u32 set_id,
	u32 enable, u32 out_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_input *dma_input;
	struct param_dma_output *dma_output;
	u32 comp_sbwc_en, comp_64b_align = 1, lossy_byte32num = 0;
	u32 stride_1p, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 format, en_votf, bus_info;
	u32 strip_enable;
	int ret;
	int gain_shift = 4, lower_limit = -16383, higher_limit = 16383;
	u32 i, val;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	switch (dma->id) {
	case BYRP_WDMA_BYR:
		dma_input = &param_set->dma_input;
		dma_output = &param_set->dma_output_byr;
		if (strip_enable)
			width = dma_input->width - param_set->stripe_input.left_margin -
					param_set->stripe_input.right_margin;
		else
			width = dma_output->width;

		height = dma_output->height;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return SET_ERROR;
	}

	en_votf = dma_output->v_otf_enable;
	hwformat = dma_output->format;
	sbwc_type = dma_output->sbwc_type;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	if (pixelsize < 12)
		pixelsize = 12; /* Pamir Evt1: Loweset pixel size is 12b */

	bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL; /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true);

	if (comp_sbwc_en == 0) {
		if (strip_enable)
			stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				param_set->stripe_input.full_width, 16, true);
		else
			stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
					width, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
								comp_64b_align, lossy_byte32num,
								BYRP_COMP_BLOCK_WIDTH,
								BYRP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, BYRP_COMP_BLOCK_WIDTH, 16) : 0;
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + BYRP_COMP_BLOCK_HEIGHT - 1) / BYRP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	/* [TEST] BLCZSL for s15 -> u10/16, u12/16, s13/13 */
	if (!IS_ENABLED(BYRP_DDK_LIB_CALL)) {
		BYRP_SET_R(base, BYRP_R_BYR_BLCNONBYRZSL_BYPASS, 0);

		val = 0;
		val = BYRP_SET_V(val, BYRP_F_BYR_BLCNONBYRZSL_PERIODIC_GAINS_EN, 1);
		val = BYRP_SET_V(val, BYRP_F_BYR_BLCNONBYRZSL_PERIODIC_OFFSETS_BEFORE_GAIN_EN, 0);
		val = BYRP_SET_V(val, BYRP_F_BYR_BLCNONBYRZSL_PERIODIC_OFFSETS_AFTER_GAIN_EN, 0);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_CONFIG, val);

		val = 0;
		for (i = 0; i < 30; ++i) {
			val = BYRP_SET_V(val, BYRP_F_BYR_BLCNONBYRZSL_PERIODIC_GAINS_0_0, 1024);
			val = BYRP_SET_V(val, BYRP_F_BYR_BLCNONBYRZSL_PERIODIC_GAINS_0_1, 1024);
			BYRP_SET_R(base + GET_COREX_OFFSET(set_id),
				BYRP_R_BYR_BLCNONBYRZSL_PERIODIC_GAINS_0 + i, val);
		}

		if (dma_output->bitwidth == 16) {
			if (dma_output->msb == 9) { /* DNG capture: unpack u10 */
				gain_shift = 2;
				lower_limit = 0;
				higher_limit = 1023;
			} else if (dma_output->msb == 11) { /* DNG capture: unpack u12 */
				gain_shift = 4;
				lower_limit = 0;
				higher_limit = 4095;
			}
		} else if (dma_output->bitwidth == 13) { /* super night capture: packed 13b */
			if (dma_output->msb == 12) {
				gain_shift = 4;
				lower_limit = -4095;
				higher_limit = 4095;
			}
		}

		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_PERIODIC_GAIN_SHIFT, gain_shift);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_LOWER_LIMIT, lower_limit);
		BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_HIGHER_LIMIT, higher_limit);

		dbg_hw(1, "[BYRP][BLCZSL] bitwidth: %d, msb: %d\n", dma_output->bitwidth, dma_output->msb);
		dbg_hw(1, "[BYRP][BLCZSL] gain_shift: %d, lower_limit: %d, higher_limit: %d\n",
			gain_shift, lower_limit, higher_limit);
	}

	return ret;
}

int byrp_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[BYRP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (dma_id) {
	case BYRP_RDMA_IMG:
		base_reg = base + byrp_regs[BYRP_R_BYR_RDMABYRIN_EN].sfr_offset;
		byrp_hw_g_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_RDMA_BYR_EN");
		break;
	case BYRP_RDMA_HDR:
		base_reg = base + byrp_regs[BYRP_R_BYR_RDMAHDRIN_EN].sfr_offset;
		byrp_hw_g_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_RDMA_HDR_EN");
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		ret = SET_ERROR;
		goto err_dma_create;
	}
	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name, available_bayer_format_map, 0, 0, 0);

err_dma_create:
	__putname(name);

	return ret;
}

void byrp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int byrp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size,
	u32 image_addr_offset, u32 header_addr_offset)
{
	int ret;
	u32 i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t header_addr[IS_MAX_FRO];

	switch (dma->id) {
	case BYRP_RDMA_IMG:
	case BYRP_RDMA_HDR:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)(addr[i] + image_addr_offset);

		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case BYRP_RDMA_IMG:
		case BYRP_RDMA_HDR:
			for (i = 0; i < num_buffers; i++)
				header_addr[i] = addr[i] + payload_size + header_addr_offset;
			break;
		default:
			break;
		}
		ret = CALL_DMA_OPS(dma, dma_set_header_addr, header_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}

int byrp_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[BYRP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (dma_id) {
	case BYRP_WDMA_BYR:
		base_reg = base + byrp_regs[BYRP_R_BYR_WDMAZSL_EN].sfr_offset;
		byrp_hw_g_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMABYR_EN");
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		ret = SET_ERROR;
		goto err_dma_create;
	}
	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name, available_bayer_format_map, 0, 0, 0);

err_dma_create:
	__putname(name);

	return ret;
}

void byrp_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int byrp_hw_s_wdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
		int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 image_addr_offset, u32 header_addr_offset)
{
	int ret;
	u32 i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t header_addr[IS_MAX_FRO];

	switch (dma->id) {
	case BYRP_WDMA_BYR:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)(addr[i] + image_addr_offset);

		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case BYRP_WDMA_BYR:
			for (i = 0; i < num_buffers; i++)
				header_addr[i] = (dma_addr_t)(addr[i] + payload_size + header_addr_offset);
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, header_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_wdma_addr);

int byrp_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type)
{
	int ret = 0;

	switch (set_id) {
	case COREX_SET_A:
	case COREX_SET_B:
		BYRP_SET_F(base, BYRP_R_COREX_UPDATE_TYPE_0, BYRP_F_COREX_UPDATE_TYPE_0, type);
		break;
	case COREX_DIRECT:
		BYRP_SET_F(base, BYRP_R_COREX_UPDATE_TYPE_0, BYRP_F_COREX_UPDATE_TYPE_0, COREX_IGNORE);
		break;
	default:
		err_hw("[BYRP] invalid corex set_id");
		ret = -EINVAL;
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_corex_update_type);

void byrp_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= BYRP_TRY_COUNT) {
			err_hw("[BYRP] fail to wait corex idle");
			break;
		}

		busy = BYRP_GET_F(base, BYRP_R_COREX_STATUS_0, BYRP_F_COREX_BUSY_0);
		dbg_hw(1, "[BYRP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_wait_corex_idle);

void byrp_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	BYRP_SET_R(base, BYRP_R_CMDQ_ADD_TO_QUEUE_0, 1);
}

void byrp_hw_s_corex_init(void __iomem *base, bool enable)
{
	u32 i;
	u32 reset_count = 0;

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		BYRP_SET_F(base, BYRP_R_COREX_UPDATE_MODE_0, BYRP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);

		byrp_hw_wait_corex_idle(base);

		BYRP_SET_F(base, BYRP_R_COREX_ENABLE, BYRP_F_COREX_ENABLE, 0x0);

		info_hw("[BYRP] %s disable done\n", __func__);
		return;
	}

	/*
	 * Set Fixed Value
	 */
	BYRP_SET_F(base, BYRP_R_COREX_ENABLE, BYRP_F_COREX_ENABLE, 0x1);
	BYRP_SET_F(base, BYRP_R_COREX_RESET, BYRP_F_COREX_RESET, 0x1);

	/* Changed from 1 to 0 when its operation finished */
	while (BYRP_GET_R(base, BYRP_R_COREX_RESET)) {
		if (reset_count > BYRP_TRY_COUNT) {
			err_hw("[BYRP] fail to wait corex reset");
			break;
		}
		reset_count++;
	}

	/*
	 * Type selection
	 * Only type0 will be used.
	 */
	BYRP_SET_F(base, BYRP_R_COREX_TYPE_WRITE_TRIGGER, BYRP_F_COREX_TYPE_WRITE_TRIGGER, 0x1);

	/*
	 * write COREX_TYPE_WRITE to 1 means set type size in 0x80(128) range.
	 */
	for (i = 0; i < (23 * 2); i++)
		BYRP_SET_F(base, BYRP_R_COREX_TYPE_WRITE, BYRP_F_COREX_TYPE_WRITE, 0x0);

	/*
	 * config type0
	 * @BYRP_R_COREX_UPDATE_TYPE_0: 0: ignore, 1: copy from SRAM, 2: swap
	 * @BYRP_R_COREX_UPDATE_MODE_0: 0: HW trigger, 1: SW trigger
	 */
	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_TYPE_0,
		BYRP_F_COREX_UPDATE_TYPE_0, COREX_COPY); /* Use Copy mode */
	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_TYPE_1,
		BYRP_F_COREX_UPDATE_TYPE_1, COREX_IGNORE); /* Do not use TYPE1 */
	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_MODE_0,
		BYRP_F_COREX_UPDATE_MODE_0, SW_TRIGGER); /* 1st frame uses SW Trigger, others use H/W Trigger */
	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_MODE_1,
		BYRP_F_COREX_UPDATE_MODE_1, SW_TRIGGER); /* 1st frame uses SW Trigger, others use H/W Trigger */

	/* initialize type0 */
	BYRP_SET_F(base, BYRP_R_COREX_COPY_FROM_IP_0, BYRP_F_COREX_COPY_FROM_IP_0, 0x1);

	/*
	 * Check COREX idleness, again.
	 */
	byrp_hw_wait_corex_idle(base);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_corex_init);

/*
 * Context: O
 * CR type: No Corex
 */
void byrp_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;

	/*
	 * Set Fixed Value
	 *
	 * Type0 only:
	 *
	 * @BYRP_R_COREX_START_0 - 1: puls generation
	 * @BYRP_R_COREX_UPDATE_MODE_0 - 0: HW trigger, 1: SW tirgger
	 *
	 * SW trigger should be needed before stream on
	 * because there is no HW trigger before stream on.
	 */
	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_MODE_0, BYRP_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	BYRP_SET_F(base, BYRP_R_COREX_START_0, BYRP_F_COREX_START_0, 0x1);

	byrp_hw_wait_corex_idle(base);

	BYRP_SET_F(base, BYRP_R_COREX_UPDATE_MODE_0, BYRP_F_COREX_UPDATE_MODE_0, HW_TRIGGER);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_corex_start);

unsigned int byrp_hw_g_int0_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src_all;

	/*
	 * src_all: per-frame based byrp IRQ status
	 * src_fro: FRO based byrp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT0);

	if (clear)
		BYRP_SET_R(base, BYRP_R_INT_REQ_INT0_CLEAR, src_all);

	*irq_state = src_all;

	return src_all;
}

unsigned int byrp_hw_g_int0_mask(void __iomem *base)
{
	return BYRP_GET_R(base, BYRP_R_INT_REQ_INT0_ENABLE);
}

unsigned int byrp_hw_g_int1_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src_all;

	/*
	 * src_all: per-frame based byrp IRQ status
	 * src_fro: FRO based byrp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT1);

	if (clear)
		BYRP_SET_R(base, BYRP_R_INT_REQ_INT1_CLEAR, src_all);

	*irq_state = src_all;

	return src_all;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_int1_state);

unsigned int byrp_hw_g_int1_mask(void __iomem *base)
{
	return BYRP_GET_R(base, BYRP_R_INT_REQ_INT1_ENABLE);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_int1_mask);

void byrp_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	/* DEBUG */
	/* Should not be controlled */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_BYPASS,
		BYRP_F_BYR_BITMASK_BYPASS, 0x0);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_BYPASS,
		BYRP_F_BYR_CROPIN_BYPASS, 0x0);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_BYPASS,
		BYRP_F_BYR_CROPZSL_BYPASS, 0x0);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_BYPASS,
		BYRP_F_BYR_CROPOUT_BYPASS, 0x0);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_BYPASS,
		BYRP_F_BYR_DISPARITY_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_BYPASS,
		BYRP_F_BYR_BPCDIRDETECTOR_BYPASS_DD, 0x0);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_BYPASS,
		BYRP_F_BYR_OTFHDR_BYPASS, 0x1);

	/* belows are controlled by nonbayer flag */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRPED_BYPASS,
		BYRP_F_BYR_BLCNONBYRPED_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRRGB_BYPASS,
		BYRP_F_BYR_BLCNONBYRRGB_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_BYPASS,
		BYRP_F_BYR_BLCNONBYRZSL_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_WBGNONBYR_BYPASS,
		BYRP_F_BYR_WBGNONBYR_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_WBGNONBYR_WB_LIMIT_MAIN_EN,
		BYRP_F_BYR_WBGNONBYR_B2BITREDUCTION, 0x0); /* This does not depends on bypass */

	/* Can be controlled bypass on/off */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_BYPASS,
		BYRP_F_BYR_MPCBYRP0_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_AFIDENTBPC_BYPASS,
		BYRP_F_BYR_AFIDENTBPC_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GAMMASENSOR_BYPASS,
		BYRP_F_BYR_GAMMASENSOR_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_BYPASS_REG,
		BYRP_F_BYR_CGRAS_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_BYPASS,
		BYRP_F_BYR_BPCSUSPMAP_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GGC_BYPASS,
		BYRP_F_BYR_BPCGGC_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FLAT_BYPASS,
		BYRP_F_BYR_BPCFLATDETECTOR_BYPASS, 0x1);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_BYPASS,
		BYRP_F_BYR_BPCDIRDETECTOR_BYPASS, 0x1);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_block_bypass);

void byrp_hw_s_block_crc(void __iomem *base, u32 set_id, u32 seed)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CINFIFO_STREAM_CRC,
		BYRP_F_BYR_CINFIFO_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRPED_STREAM_CRC,
		BYRP_F_BYR_BLCNONBYRPED_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRRGB_STREAM_CRC,
		BYRP_F_BYR_BLCNONBYRRGB_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_STREAM_CRC,
		BYRP_F_BYR_BLCNONBYRZSL_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_STREAM_CRC,
		BYRP_F_BYR_CROPIN_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_CRC,
		BYRP_F_BYR_BITMASK_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_STREAM_CRC,
		BYRP_F_BYR_DTPNONBYR_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_AFIDENTBPC_STREAM_CRC,
		BYRP_F_BYR_AFIDENTBPC_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GAMMASENSOR_STREAM_CRC,
		BYRP_F_BYR_GAMMASENSOR_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_CRC,
		BYRP_F_BYR_CGRAS_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_WBGNONBYR_STREAM_CRC,
		BYRP_F_BYR_WBGNONBYR_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_STREAM_CRC,
		BYRP_F_BYR_CROPOUT_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_STREAM_CRC,
		BYRP_F_BYR_OTFHDR_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_STREAM_CRC,
		BYRP_F_BYR_CROPZSL_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_STREAM_CRC,
		BYRP_F_BYR_MPCBYRP0_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_CRC_SEED,
		BYRP_F_BYR_BPCSUSPMAP_CRC_SEED, seed);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_CRC_SEED,
		BYRP_F_BYR_BPCDIRDETECTOR_CRC_SEED, seed);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_block_crc);

void byrp_hw_s_bitmask(void __iomem *base, u32 set_id, bool enable, u32 bit_in, u32 bit_out)
{
	/* Pamir Evt0 should use bitmask 14b->10b, Evt1 handles 14b->14b */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_BYPASS,
		BYRP_F_BYR_BITMASK_BYPASS, !enable);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_BITTAGEIN,
		BYRP_F_BYR_BITMASK_BITTAGEIN, bit_in);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BITMASK_BITTAGEOUT,
		BYRP_F_BYR_BITMASK_BITTAGEOUT, bit_out);
}

void byrp_hw_s_pixel_order(void __iomem *base, u32 set_id, u32 pixel_order)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_PIXEL_ORDER,
		 BYRP_F_BYR_DTPNONBYR_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_WBGNONBYR_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GGC_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FLAT_PIXEL_ORDER, pixel_order);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_PIXEL_ORDER, pixel_order);
}

void byrp_hw_s_non_byr_pattern(void __iomem *base, u32 set_id, u32 non_byr_pattern)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DTPNONBYR_PIXEL_ORDER,
		 BYRP_F_BYR_DTPNONBYR_NON_BYR_PATTERN, non_byr_pattern);

	/* 0: Bayer, 1: Tetra(only) */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_BYPASS, non_byr_pattern == 1 ? 1 : 0);

	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_CONFIG,
		 BYRP_F_BYR_CGRAS_NON_BYR_MODE_EN, non_byr_pattern ? 1 : 0);

	/* 0: Normal Bayer, 1: 2x2(Tetra), 2: 3x3(Nona), 3: 4x4 pattern */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_CONFIG,
		 BYRP_F_BYR_CGRAS_NON_BYR_PATTERN, non_byr_pattern);

	/* 0: Normal Bayer, 1: 2x2(Tetra), 2: 3x3(Nona), 3: 4x4 pattern */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_WBGNONBYR_PIXEL_ORDER,
		 BYRP_F_BYR_WBGNONBYR_NON_BYR_PATTERN, non_byr_pattern);

	/* 0: Bayer, 1: Tetra(only) */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_OP_MODE, non_byr_pattern == 1 ? 1 : 0);

	/* 0: Bayer, 1: Tetra(only) */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_CFA_TYPE, non_byr_pattern == 1 ? 1 : 0);

	/* Caution! 1: Bayer, 2: Tetra(only) */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GGC_PHASE_SHIFT_ENABLE,
		 BYRP_F_BYR_BPCGGC_PHASE_SHIFT_MODE, non_byr_pattern == 1 ? 2 : 1);

	/* 0: Bayer, 1: Tetra(only) */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FLAT_CFA_TYPE, non_byr_pattern == 1 ? 1 : 0);

	/* 0: Bayer, 1: Tetra(only) */
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_CFA_TYPE, non_byr_pattern == 1 ? 1 : 0);
}

void byrp_hw_s_mono_mode(void __iomem *base, u32 set_id, bool enable)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CGRAS_CONFIG,
		BYRP_F_BYR_CGRAS_MONO_MODE_EN, enable);

	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRPED_CONFIG,
		BYRP_F_BYR_BLCNONBYRPED_MONO_MODE_EN, enable);

	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRRGB_CONFIG,
		BYRP_F_BYR_BLCNONBYRRGB_MONO_MODE_EN, enable);

	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_BLCNONBYRZSL_CONFIG,
		BYRP_F_BYR_BLCNONBYRZSL_MONO_MODE_EN, enable);

	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_MONO_MODE_EN, enable);
}

void byrp_hw_s_sdc_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_SDC_CRC_0, 0x37);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_SDC_CRC_1, 0x37);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_SDC_CRC_2, 0x37);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_SDC_CRC_3, 0x37);
}

void byrp_hw_s_bcrop_size(void __iomem *base, u32 set_id, u32 bcrop_num, u32 x, u32 y, u32 width, u32 height)
{
	switch (bcrop_num) {
	case BYRP_BCROP_BYR:
		dbg_hw(1, "[API][%s] BYRP_BCROP_BYR -> x(%d), y(%d), w(%d), h(%d)\n",
			__func__, x, y, width, height);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_BYPASS,
			BYRP_F_BYR_CROPIN_BYPASS, 0x0);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_START_X,
			BYRP_F_BYR_CROPIN_START_X, x);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_START_Y,
			BYRP_F_BYR_CROPIN_START_Y, y);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_SIZE_X,
			BYRP_F_BYR_CROPIN_SIZE_X, width);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPIN_SIZE_Y,
			BYRP_F_BYR_CROPIN_SIZE_Y, height);
		break;
	case BYRP_BCROP_ZSL:
		dbg_hw(1, "[API][%s] BYRP_BCROP_ZSL -> x(%d), y(%d), w(%d), h(%d)\n",
			__func__, x, y, width, height);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_BYPASS,
			BYRP_F_BYR_CROPZSL_BYPASS, 0x0);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_START_X,
			BYRP_F_BYR_CROPZSL_START_X, x);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_START_Y,
			BYRP_F_BYR_CROPZSL_START_Y, y);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_SIZE_X,
			BYRP_F_BYR_CROPZSL_SIZE_X, width);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPZSL_SIZE_Y,
			BYRP_F_BYR_CROPZSL_SIZE_Y, height);
		break;
	case BYRP_BCROP_RGB:
		dbg_hw(1, "[API][%s] BYRP_BCROP_RGB -> x(%d), y(%d), w(%d), h(%d)\n",
			__func__, x, y, width, height);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_BYPASS,
			BYRP_F_BYR_CROPOUT_BYPASS, 0x0);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_START_X,
			BYRP_F_BYR_CROPOUT_START_X, x);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_START_Y,
			BYRP_F_BYR_CROPOUT_START_Y, y);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_SIZE_X,
			BYRP_F_BYR_CROPOUT_SIZE_X, width);
		BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_CROPOUT_SIZE_Y,
			BYRP_F_BYR_CROPOUT_SIZE_Y, height);
		break;
	case BYRP_BCROP_STRP: /* Not exist in EVT1 */
		break;
	default:
		err_hw("[BYRP] invalid bcrop number[%d]", bcrop_num);
		break;
	}
}

void byrp_hw_s_grid_cfg(void __iomem *base, struct byrp_grid_cfg *cfg)
{
	u32 val;

	val = 0;
	val = BYRP_SET_V(val, BYRP_F_BYR_CGRAS_BINNING_X, cfg->binning_x);
	val = BYRP_SET_V(val, BYRP_F_BYR_CGRAS_BINNING_Y, cfg->binning_y);
	BYRP_SET_R(base, BYRP_R_BYR_CGRAS_BINNING_X, val);

	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_CROP_START_X,
			BYRP_F_BYR_CGRAS_CROP_START_X, cfg->crop_x);
	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_CROP_START_Y,
			BYRP_F_BYR_CGRAS_CROP_START_Y, cfg->crop_y);

	val = 0;
	val = BYRP_SET_V(val, BYRP_F_BYR_CGRAS_CROP_RADIAL_X, cfg->crop_radial_x);
	val = BYRP_SET_V(val, BYRP_F_BYR_CGRAS_CROP_RADIAL_Y, cfg->crop_radial_y);
	BYRP_SET_R(base, BYRP_R_BYR_CGRAS_CROP_START_REAL, val);
}

void byrp_hw_s_bpc_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	/* byr dir */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_WIDTH,
		BYRP_F_BYR_BPCDIRDETECTOR_WIDTH, width);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DIR_HEIGHT,
		BYRP_F_BYR_BPCDIRDETECTOR_HEIGHT, height);

	/* byr flat */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FLAT_WIDTH,
		BYRP_F_BYR_BPCFLATDETECTOR_WIDTH, width);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_FLAT_WIDTH,
		BYRP_F_BYR_BPCFLATDETECTOR_HEIGHT, height);
}

void byrp_hw_s_disparity_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	/* TODO: Need to set sensor width/height */
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_SENSOR_WIDTH,
		BYRP_F_BYR_DISPARITY_SENSOR_WIDTH, width);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_SENSOR_HEIGHT,
		BYRP_F_BYR_DISPARITY_SENSOR_HEIGHT, height);

	/* TODO: Need to get crop and binning info */
	/*
	 * BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_CROP_X,
	 *	BYRP_F_BYR_DISPARITY_CROP_X, width);
	 * BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_CROP_Y,
	 *	BYRP_F_BYR_DISPARITY_CROP_Y, height);
	 * BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_BINNING,
	 *	BYRP_F_BYR_DISPARITY_BINNING_X, binning_x);
	 * BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_DISPARITY_BINNING,
	 *	BYRP_F_BYR_DISPARITY_BINNING_Y, binning_y);
	 */
}

void byrp_hw_s_susp_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_WIDTH,
		BYRP_F_BYR_BPCSUSPMAP_WIDTH, width);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_SUSP_HEIGHT,
		BYRP_F_BYR_BPCSUSPMAP_HEIGHT, height);
}

void byrp_hw_s_ggc_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GGC_WIDTH,
		BYRP_F_BYR_BPCGGC_WIDTH, width);
	BYRP_SET_F(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_GGC_HEIGHT,
		BYRP_F_BYR_BPCGGC_HEIGHT, height);
}

void byrp_hw_s_chain_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height)
{
	u32 val = 0;

	val = BYRP_SET_V(val, BYRP_F_GLOBAL_IMG_WIDTH, dma_width);
	val = BYRP_SET_V(val, BYRP_F_GLOBAL_IMG_HEIGHT, dma_height);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_GLOBAL_IMAGE_RESOLUTION, val);
}

void byrp_hw_s_mpc_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_IMAGE_WIDTH, width);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_MPCBYRP0_IMAGE_HEIGHT, height);
}

void byrp_hw_s_otf_hdr_size(void __iomem *base, bool enable, u32 set_id, u32 height)
{
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_BYPASS, !enable);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_MARGIN_TOP, 0);
	BYRP_SET_R(base + GET_COREX_OFFSET(set_id), BYRP_R_BYR_OTFHDR_ACTIVE_HEIGHT, height);
}

u32 byrp_hw_g_reg_cnt(void)
{
	return BYRP_REG_CNT + BYRP_LUT_REG_CNT;
}
