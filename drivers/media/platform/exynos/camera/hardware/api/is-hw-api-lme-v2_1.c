// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * lme HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-lme-v2_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-lme-v2_1.h"

#define LME_SET_F(base, R, F, val) \
	is_hw_set_field(base, &lme_regs[R], &lme_fields[F], val)
#define LME_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &lme_regs[R], &lme_fields[F], val)
#define LME_SET_R(base, R, val) \
	is_hw_set_reg(base, &lme_regs[R], val)
#define LME_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &lme_regs[R], val)
#define LME_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &lme_fields[F], val)

#define LME_GET_F(base, R, F) \
	is_hw_get_field(base, &lme_regs[R], &lme_fields[F])
#define LME_GET_R(base, R) \
	is_hw_get_reg(base, &lme_regs[R])
#define LME_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &lme_regs[R])
#define LME_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &lme_fields[F])

unsigned int lme_hw_is_occurred(unsigned int status, enum lme_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR_LME_FRAME_START;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR_LME_FRAME_END;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR_LME_COREX_END_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR_LME_COREX_END_1;
		break;
	case INTR_ERR:
		mask = LME_INT_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int lme_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp;
	u32 ret;

	LME_SET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_SW_RESET, 0x1);

	while (1) {
		temp = LME_GET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > LME_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	/* Set non-secure master for LME DMAs with SMC_CALL */
	ret = pablo_smc(SMC_CMD_PREAPRE_PD_ONOFF, 0x1, 0x17610204, 0x2); /* D_TZPC_LME */
	if (ret)
		err("[SMC] SMC_CMD_PREPARE_PD_ONOFF fail: %d", ret);

	info_hw("[LME] %s done.\n", __func__);

	return ret;
}

void lme_hw_s_init(void __iomem *base)
{
	/* LME_SET_R(base, LME_R_AUTO_MASK_PREADY, 0x1); */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_IP_PROCESSING, LME_F_IP_PROCESSING, 0x1);
}

int lme_hw_wait_idle(void __iomem *base, u32 set_id)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = LME_GET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);
	int_all = LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1_STATUS);

	info_hw("[LME] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = LME_GET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= LME_TRY_COUNT) {
			err_hw("[LME] timeout waiting idle - disable fail");
			lme_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1_STATUS);

	info_hw("[LME] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			idle, int_all);

	return ret;
}

void lme_hw_dump(void __iomem *base)
{
	info_hw("[LME] SFR DUMP (v2.1)\n");
	info_hw("[LME] COREX_DIRECT\n");
	is_hw_dump_regs(base + GET_LME_COREX_OFFSET(COREX_DIRECT), lme_regs, LME_REG_CNT);
}

static void lme_hw_s_common(void __iomem *base, u32 set_id)
{
	u32 val;

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_IP_POST_FRAME_GAP, LME_F_IP_POST_FRAME_GAP, 0x0);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_IP_CORRUPTED_INTERRUPT_ENABLE, LME_F_IP_CORRUPTED_INTERRUPT_ENABLE, 0x7);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_RD_SLOT_LEN_0, 0xb); /* 2 clients */
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_WR_SLOT_LEN_0, 0x10); /* 2 clients */

	val = 0;
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_0, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_1, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_2, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_3, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_4, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_RD_0_5, 0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_SLOT_REG_RD_0_0, val);

	val = 0;
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_0, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_1, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_2, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_3, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_4, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_5, 1);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_SLOT_REG_WR_0_0, val);

	val = 0;
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_6, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_7, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_8, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_9, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_10, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_11, 1);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_SLOT_REG_WR_0_1, val);

	val = 0;
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_12, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_13, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_14, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_15, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_16, 2);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P1P0_SLOT_REG_WR_0_17, 0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_SLOT_REG_WR_0_2, val);


	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_WR_ADDR_FIFO_DEPTH_0,
		LME_F_DMA_SIPULME2P1P0_WR_ADDR_FIFO_DEPTH_0, 0x60);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_WR_DATA_FIFO_DEPTH_0,
		LME_F_DMA_SIPULME2P1P0_WR_DATA_FIFO_DEPTH_0, 0x60);

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P1P0_WR_ADDR_MO_LIMIT_0,
		LME_F_DMA_SIPULME2P1P0_WR_ADDR_MO_LIMIT_0, 0x60);
}

static void lme_hw_s_int_mask(void __iomem *base, u32 set_id)
{
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CONTINT_SIPULME2P1P0_4SETS_LEVEL_PULSE_N_SEL,
		LME_F_CONTINT_SIPULME2P1P0_4SETS_CONTINT_LEVEL_PULSE_N_SEL,
		INT_LEVEL + 2);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1_ENABLE,
		LME_F_CONTINT_SIPULME2P1P0_4SETS_CONTINT_INT1_ENABLE,
		LME_INT_EN_MASK);
}

static void lme_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_SECU_CTRL_SEQID, LME_F_SECU_CTRL_SEQID, 0x0);
}

void lme_hw_s_block_crc(void __iomem *base, u32 set_id)
{
	/* Nothing to config excep AXI CRC */
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_AXICRC_SIPULME2P1P0_SEED_0, LME_F_AXICRC_SIPULME2P1P0_SEED_0, 0x37);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_AXICRC_SIPULME2P1P0_SEED_1, LME_F_AXICRC_SIPULME2P1P0_SEED_1, 0x37);
}

void lme_hw_s_core(void __iomem *base, u32 set_id)
{
	lme_hw_s_common(base, set_id);
	lme_hw_s_int_mask(base, set_id);
	lme_hw_s_secure_id(base, set_id);
	lme_hw_s_block_crc(base, set_id);
}

int lme_hw_s_rdma_init(void __iomem *base, struct lme_param_set *param_set,
	u32 enable, u32 id, u32 set_id)
{
	u32 mv_height, lwidth;
	u32 val = 0;

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_CLIENT_ENABLE, enable);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_DATA_FIFO_DEPTH, 0x7d);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_GEOM_BURST_LENGTH, 0xF);
		break;
	case LME_RDMA_CACHE_IN_1:
		break;
	case LME_RDMA_MBMV_IN:
		/* MBMV IN */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_CLIENT_ENABLE, enable);

		if (enable == 0)
			return 0;

		lwidth = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
		mv_height = DIV_ROUND_UP(param_set->dma.output_height, 16);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_GEOM_BURST_LENGTH, 0x2);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_GEOM_LWIDTH, lwidth);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_GEOM_LINE_COUNT, mv_height);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_LWIDTH, lwidth);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_LINEGAP, 0x14);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_PREGAP, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_POSTGAP, 0x14);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_PIXELGAP, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_STALLGAP, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_PACKING, 0x8);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_GEOM_LINE_DIRECTION_HW, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_PACKING, 0x8);

		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_MNM, val);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_FRMT_CH_MIX_1, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_OUTSTANDING_LIMIT, 0x2);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MBMV_IN_CLIENT_FLUSH, 0);
		break;
	}

	return 0;
}

int lme_hw_s_wdma_init(void __iomem *base,
	struct lme_param_set *param_set, u32 enable, u32 id, u32 set_id)
{
	u32 mv_height, lwidth;
	u32 val = 0;

	switch (id) {
	case LME_WDMA_MV_OUT:
		/* MV OUT */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_CLIENT_ENABLE, enable);

		if (enable == 0)
			return 0;

		lwidth = 4 * DIV_ROUND_UP(param_set->dma.cur_input_width, 8);
		mv_height = DIV_ROUND_UP(param_set->dma.cur_input_height, 4);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_BURST_LENGTH, 0x4);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_LINE_COUNT, mv_height);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_LINE_DIRECTION_HW, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_FRMT_PACKING, 0x10);

		val = LME_SET_V(val, LME_F_DMACLIENT_SPS_MV_OUT_FRMT_BPAD_SET, 0x4);
		val = LME_SET_V(val, LME_F_DMACLIENT_SPS_MV_OUT_FRMT_BPAD_TYPE, 0x2);
		val = LME_SET_V(val, LME_F_DMACLIENT_SPS_MV_OUT_FRMT_BSHIFT_SET, 0x4);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_FRMT_MNM, val);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_FRMT_CH_MIX_1, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_OUTSTANDING_LIMIT, 0x3);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_DATA_FIFO_DEPTH, 0x20);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			 LME_R_DMACLIENT_SPS_MV_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_SELF_HW_FLUSH_ENABLE, 0x0);
		break;
	case LME_WDMA_SAD_OUT:
		/* SAD OUT */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_CLIENT_ENABLE, enable);

		if (enable == 0)
			return 0;

		lwidth = 2 * DIV_ROUND_UP(param_set->dma.output_width, 8);
		mv_height = DIV_ROUND_UP(param_set->dma.output_height, 4);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_BURST_LENGTH, 0x4);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_LINE_COUNT, mv_height);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_LINE_DIRECTION_HW, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_FRMT_PACKING, 0x10);

		val = LME_SET_V(val, LME_F_DMACLIENT_SAD_OUT_FRMT_BPAD_SET, 0x4);
		val = LME_SET_V(val, LME_F_DMACLIENT_SAD_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(val, LME_F_DMACLIENT_SAD_OUT_FRMT_BSHIFT_SET, 0x4);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_FRMT_MNM, val);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_OUTSTANDING_LIMIT, 0x3);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_DATA_FIFO_DEPTH, 0x20);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_SELF_HW_FLUSH_ENABLE, 0x0);
		break;
	case LME_WDMA_MBMV_OUT:
		/* MBMV OUT */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_CLIENT_ENABLE, enable);

		if (enable == 0)
			return 0;

		lwidth = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
		mv_height = DIV_ROUND_UP(param_set->dma.output_height, 16);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_BURST_LENGTH, 0x3);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_LINE_COUNT, mv_height);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_LINE_DIRECTION_HW, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_FRMT_PACKING, 0x8);

		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_FRMT_MNM, val);

		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_FRMT_CH_MIX_1, 0x1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_OUTSTANDING_LIMIT, 0x3);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_DATA_FIFO_DEPTH, 0x20);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_CLIENT_FLUSH, 0);
		break;
	default:
		break;
	}

	return 0;
}

int lme_hw_s_rdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;
	int val = 0;

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));

		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_0, addr[0]);
		break;
	case LME_RDMA_CACHE_IN_1:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));

		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_1, addr[0]);
		break;
	case LME_RDMA_MBMV_IN:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));

		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_0, addr[0]);

		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_ROTATION_SIZE, 1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_CONF, val);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_1, addr[1]);

		break;
	}

	return ret;
}

int lme_hw_s_wdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;
	int val = 0;

	switch (id) {
	case LME_WDMA_MV_OUT:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));
		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SPS_MV_OUT_GEOM_BASE_ADDR_0, (dma_addr_t)(addr[0]));
		break;
	case LME_WDMA_MBMV_OUT:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));
		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_0, (dma_addr_t)(addr[0]));


		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(val, LME_F_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_ROTATION_SIZE, 1);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_CONF, val);
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MBMV_OUT_GEOM_BASE_ADDR_1, (dma_addr_t)(addr[1]));

		break;
	case LME_WDMA_SAD_OUT:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));
		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_SAD_OUT_GEOM_BASE_ADDR_0, (dma_addr_t)(addr[0]));
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		ret = SET_ERROR;
	}

	return ret;
}

int lme_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type)
{
	int ret = 0;

	switch (set_id) {
	case COREX_SET_A:
	case COREX_SET_B:
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_UPDATE_TYPE_0, LME_F_COREX_UPDATE_TYPE_0, type);
		break;
	case COREX_DIRECT:
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_UPDATE_TYPE_0, LME_F_COREX_UPDATE_TYPE_0, COREX_IGNORE);
		break;
	default:
		err_hw("[LME] invalid corex set_id");
		ret = -EINVAL;
		break;
	}

	return ret;
}

void lme_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= LME_TRY_COUNT) {
			err_hw("[LME] fail to wait corex idle");
			break;
		}

		busy = LME_GET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_COREX_STATUS_0, LME_F_COREX_BUSY_0);
		dbg_hw(1, "[LME] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

void lme_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_CTRL_MS_ADD_TO_QUEUE, LME_F_CTRL_MS_ADD_TO_QUEUE, set_id);
}

void lme_hw_s_corex_init(void __iomem *base, bool enable)
{
	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER);

		lme_hw_wait_corex_idle(base);

		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_ENABLE, LME_F_COREX_ENABLE, 0x0);

		info_hw("[LME] %s disable done\n", __func__);
		return;
	}

	/*
	 * Set Fixed Value
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_CTRL_MS_OPERATION_MODE_SELECT, LME_F_CTRL_MS_OPERATION_MODE_SELECT, QUEUE_MODE);

	/*
	 * config type0
	 * @TAA_R_COREX_UPDATE_TYPE_0: 0: ignore, 1: copy from SRAM, 2: swap
	 * @TAA_R_COREX_UPDATE_MODE_0: 0: HW trigger, 1: SW trigger
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_TYPE_0, LME_F_COREX_UPDATE_TYPE_0, COREX_COPY);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_TYPE_1, LME_F_COREX_UPDATE_TYPE_1, COREX_IGNORE);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, HW_TRIGGER);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_1, LME_F_COREX_UPDATE_MODE_1, HW_TRIGGER);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_START_0, LME_F_COREX_START_0, 0);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_START_1, LME_F_COREX_START_1, 0);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_MULTISET_ENABLE, LME_F_COREX_MULTISET_ENABLE, 0x1);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_COREX_ENABLE, LME_F_COREX_ENABLE, 0x1);

	/* initialize type0 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_COPY_FROM_IP_0, LME_F_COREX_COPY_FROM_IP_0, 0x1);

	/*
	 * Check COREX idleness, again.
	 */
	lme_hw_wait_corex_idle(base);

	info_hw("[LME] %s done\n", __func__);
}

/*
 * Context: O
 * CR type: No Corex
 */
void lme_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;

	/*
	 * Set Fixed Value
	 *
	 * Type0 only:
	 *
	 * @TAA_R_COREX_START_0 - 1: puls generation
	 * @TAA_R_COREX_UPDATE_MODE_0 - 0: HW trigger, 1: SW tirgger
	 *
	 * SW trigger should be needed before stream on
	 * because there is no HW trigger before stream on.
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_START_0, LME_F_COREX_START_0, 0x1);

	lme_hw_wait_corex_idle(base);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, HW_TRIGGER);

	info_hw("[LME] %s done\n", __func__);
}

unsigned int lme_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers,
	u32 *irq_state, u32 set_id)
{
	u32 src, src_all, src_err;

	/*
	 * src_all: per-frame based lme IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1);

	dbg_lme(4, "[API][%s] src_all: 0x%x\n", __func__, src_all);

	if (clear)
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1_CLEAR, src_all);

	src_err = src_all & LME_INT_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

unsigned int lme_hw_g_int_mask(void __iomem *base, u32 set_id)
{
	return LME_GET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_CONTINT_SIPULME2P1P0_4SETS_INT1_ENABLE);
}

void lme_hw_s_cache(void __iomem *base, u32 set_id, bool enable, u32 prev_width, u32 cur_width)
{
	u32 val = 0;
	u32 mode;
	u32 ignore_prefetch;
	u32 prev_pix_gain, cur_pix_gain;
	u32 prev_pix_offset, cur_pix_offset;

	/* Common */
	enable = 1;
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_LME_BYPASS, !enable);

	mode = 1; /* 0: Fusion, 1: TNR */
	ignore_prefetch = 0; /* use prefetch for performance */

	val = LME_SET_V(val, LME_F_CACHE_8BIT_IGNORE_PREFETCH, ignore_prefetch);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_DATA_REQ_CNT_EN, 1);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PRE_REQ_CNT_EN, 1);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CACHE_UTILIZATION_EN, 1);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CACHE_CADDR_OFFSET, 0x8);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_LME_BYPASS, val);

	prev_pix_gain = 0x40;
	prev_pix_offset = 0;

	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_GAIN_0, prev_pix_gain);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_OFFSET_0, prev_pix_offset);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_PIX_CONFIG_0, val);

	cur_pix_gain = 0x40;
	cur_pix_offset = 0;

	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_GAIN_1, cur_pix_gain);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_OFFSET_1, cur_pix_offset);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_PIX_CONFIG_1, val);
}

void lme_hw_s_cache_size(void __iomem *base, u32 set_id, u32 prev_width, u32 prev_height, u32 cur_width, u32 cur_height)
{
	u32 val = 0;
	u32 prev_crop_start_x, prev_crop_start_y;
	u32 cur_crop_start_x, cur_crop_start_y;
	u32 prev_width_jump, cur_width_jump;

	dbg_lme(4, "[API][%s] prev(%dx%d), cur(%dx%d)\n", __func__,
		prev_width, prev_height, cur_width, cur_height);

	/* previous frame */
	val = LME_SET_V(val, LME_F_CACHE_8BIT_IMG_WIDTH_X_0, prev_width);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_IMG_HEIGHT_Y_0, prev_height);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_IMAGE0_CONFIG, val);

	prev_crop_start_x = 0; /* Not use crop */
	prev_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CROP_START_X_0, prev_crop_start_x);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CROP_START_Y_0, prev_crop_start_y);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_CROP_CONFIG_START_0, val);

	prev_width_jump = ALIGN(prev_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_0, prev_width_jump);

	/* current frame */
	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_IMG_WIDTH_X_1, cur_width);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_IMG_HEIGHT_Y_1, cur_height);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_IMAGE1_CONFIG, val);

	cur_crop_start_x = 0; /* Not use crop */
	cur_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CROP_START_X_1, cur_crop_start_x);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CROP_START_Y_1, cur_crop_start_y);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_CROP_CONFIG_START_1, val);

	cur_width_jump = ALIGN(cur_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_1, cur_width_jump);
}

void lme_hw_s_mvct_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	u32 val = 0;
	u32 prefetch_gap;
	u32 prefetch_en;
	const u32 cell_width = 16;

	prefetch_gap = DIV_ROUND_UP(width * 15 / 100, cell_width);
	prefetch_en = 1;
	dbg_lme(4, "[API][%s] width: %d, cell_width: %d, prefetch_gap : %d\n",
		__func__, width, cell_width, prefetch_gap);

	val = LME_SET_V(val, LME_F_MVCT_8BIT_LME_PREFETCH_GAP, prefetch_gap);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_LME_PREFETCH_EN, prefetch_en);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_LME_PREFETCH, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_8BIT_IMAGE_WIDTH, width);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_IMAGE_HEIGHT, height);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_IMAGE_DIMENTIONS, val);
}

void lme_hw_s_mvct(void __iomem *base, u32 set_id)
{
	u32 val = 0;
	const u32 sr_x = 128; /* search range: 16 ~ 128 */
	const u32 sr_y = 128;

	val = LME_SET_V(val, LME_F_MVCT_8BIT_LME_MODE, 0x1); /* 0: fusion, 1: TNR */
	val = LME_SET_V(val, LME_F_MVCT_8BIT_LME_FIRST_FRAME, 0x0);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_LME_FW_FRAME_ONLY, 0x0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_LME_CONFIG, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_8BIT_USE_AD, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_USE_SAD, 0x0);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_USE_CT, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_USE_ZSAD, 0x1);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_MVE_CONFIG, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_8BIT_WEIGHT_CT, 1);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_WEIGHT_AD, 5);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_WEIGHT_SAD, 1);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_WEIGHT_ZSAD, 1);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_NOISE_LEVEL, 3);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_MVE_WEIGHT, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_8BIT_SR_X, sr_x);
	val = LME_SET_V(val, LME_F_MVCT_8BIT_SR_Y, sr_y);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_8BIT_MV_SR, val);
}

void lme_hw_s_first_frame(void __iomem *base, u32 set_id, bool first_frame)
{
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_MVCT_8BIT_LME_CONFIG, LME_F_MVCT_8BIT_LME_FIRST_FRAME, first_frame);

	if (first_frame)
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
				LME_R_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
				LME_F_DMACLIENT_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET, 0);
}

void lme_hw_s_crc(void __iomem *base, u32 seed)
{
	LME_SET_F(base, LME_R_AXICRC_SIPULME2P1P0_SEED_0,
			LME_F_AXICRC_SIPULME2P1P0_SEED_0, seed);
	LME_SET_F(base, LME_R_AXICRC_SIPULME2P1P0_SEED_1,
			LME_F_AXICRC_SIPULME2P1P0_SEED_1, seed);
}

struct is_reg *lme_hw_get_reg_struct(void)
{
	return (struct is_reg *)lme_regs;
}

unsigned int lme_hw_get_reg_cnt(void)
{
	return LME_REG_CNT;
}

void lme_hw_s_block_bypass(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_LME_BYPASS, 0x0);
}
