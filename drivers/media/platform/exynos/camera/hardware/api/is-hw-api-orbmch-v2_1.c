// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * ORBMCH HW control APIs
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-orbmch.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-orbmch-v2_1.h"

#define ORBMCH_SET_F(base, R, F, val) \
	is_hw_set_field(base, &orbmch_regs[R], &orbmch_fields[F], val)
#define ORBMCH_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &orbmch_regs[R], &orbmch_fields[F], val)
#define ORBMCH_SET_R(base, R, val) \
	is_hw_set_reg(base, &orbmch_regs[R], val)
#define ORBMCH_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &orbmch_regs[R], val)
#define ORBMCH_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &orbmch_fields[F], val)

#define ORBMCH_GET_F(base, R, F) \
	is_hw_get_field(base, &orbmch_regs[R], &orbmch_fields[F])
#define ORBMCH_GET_R(base, R) \
	is_hw_get_reg(base, &orbmch_regs[R])
#define ORBMCH_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &orbmch_regs[R])
#define ORBMCH_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &orbmch_fields[F])

#define ORBMCH_TIMEOUT_T_MAX	8
static struct is_orbmch_timeout_t is_orbmch_timeout_t_arr[ORBMCH_TIMEOUT_T_MAX] = {
	{
		.freq = 666,
		.process_time = 5,
	}, {
		.freq = 533,
		.process_time = 6,
	}, {
		.freq = 444,
		.process_time = 7,
	}, {
		.freq = 400,
		.process_time = 8,
	}, {
		.freq = 333,
		.process_time = 10,
	}, {
		.freq = 222,
		.process_time = 15,
	}, {
		.freq = 166,
		.process_time = 20,
	}, {
		.freq = 50,
		.process_time = 60,
	},
};

unsigned int orbmch_hw_g_process_time(int cur_lv)
{
	if (cur_lv < 0 || cur_lv >= ORBMCH_TIMEOUT_T_MAX) {
		err_hw("[ORBMCH] Invalid cur_lv(%d), out of index range", cur_lv);
		cur_lv = ORBMCH_TIMEOUT_T_MAX - 1;
	}

	return is_orbmch_timeout_t_arr[cur_lv].process_time;
}

unsigned int orbmch_hw_is_occurred(unsigned int status, enum orbmch_event_type type)
{
	u32 mask;

	switch (type) {
	case ORBMCH_INTR_FRAME_START:
		mask = 1 << INTR_STATUS_ORBMCH_ASET_FRAME_START;
		break;
	case ORBMCH_INTR_FRAME_END:
		mask = 1 << INTR_STATUS_ORBMCH_ASET_FRAME_END;
		break;
	case ORBMCH_INTR_ERR:
		mask = ORBMCH_INT_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int orbmch_hw_s_reset(void __iomem *base)
{
	u32 try_count = 0;
	u32 temp = 0;
	u32 val = 0;

	dbg_orbmch(4, "[API][%s] is called!", __func__);

	/* Disable interrupt */
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_EN, 0);

	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_RDMA_FLUSH, 1);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_WDMA_FLUSH, 1);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_DMA_FLUSH, val);

	while (1) {
		temp = ORBMCH_GET_R(base, ORBMCH_R_ORBMCH_DMA_FLUSH_DONE);
		if (temp == 0x3)
			break;

		if (try_count > ORBMCH_TRY_COUNT)
			return try_count;
		try_count++;
	}

	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_RESET_CTRL, 1);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_RESET_CTRL, 0);

	try_count = 0;
	while (1) {
		temp = ORBMCH_GET_R(base, ORBMCH_R_ORBMCH_LV1_FRAME_SIZE_A);
		if (temp == 0)
			break;

		if (try_count > ORBMCH_TRY_COUNT) {
			err_hw("[ORBMCH] reset fail");
			return try_count;
		}
		try_count++;
	}

	return 0;
}

void orbmch_hw_s_init(void __iomem *base)
{
	u32 val = 0;

	dbg_orbmch(4, "[API][%s] is called!", __func__);

	/* Clock enable ..? */
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_CG_ON, 1);

	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_QCH_OFF, 1);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_CG_OFF, 0);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_CG_OFF, val);

	val = 0;
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_ASET_NUM, 0);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_BSET_NUM, 0);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_SET_NUMBER, val);

	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_VOTF_EN, 0);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_FRO2_EN, 0);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_VOTF_USE_LEGACY, 0);
}

void orbmch_hw_s_int_mask(void __iomem *base)
{
	dbg_orbmch(4, "[API][%s] is called!", __func__);

	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_EN, ORBMCH_INT_EN_MASK);
}

unsigned int orbmch_hw_g_int_mask(void __iomem *base)
{
	return ORBMCH_GET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_EN);
}

int orbmch_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;

	return ret;
}

int orbmch_hw_s_rdma_init(struct is_common_dma *dma, struct orbmch_param_set *param_set, u32 enable, u32 set_id)
{
	dbg_orbmch(4, "[API][%s] is called\n", __func__);

	if (enable == 0)
		return 0;

	switch (dma->id) {
	case ORBMCH_RDMA_ORB_L1:
	case ORBMCH_RDMA_ORB_L2:
		/* Set orb rdma mo  */
		ORBMCH_SET_F(dma->base, ORBMCH_R_ORBMCH_ORB_DMA_MO_A, ORBMCH_F_ORBMCH_ORB_RDMA0_MAX_MO_A, 0x100);
		ORBMCH_SET_F(dma->base, ORBMCH_R_ORBMCH_ORB_DMA_MO_A, ORBMCH_F_ORBMCH_ORB_RDMA1_MAX_MO_A, 0x100);
		break;
	case ORBMCH_RDMA_MCH_PREV_DESC:
	case ORBMCH_RDMA_MCH_CUR_DESC:
		/* Set mch wdma stride */
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_MCH_RDMA0_ADDR_STRIDE_A, ORB_DESC_STRIDE);
		/* Set mch wdma mo */
		ORBMCH_SET_F(dma->base, ORBMCH_R_ORBMCH_MCH_DMA_MO_A, ORBMCH_F_ORBMCH_MCH_RDMA0_MAX_MO_A, 0x100);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	return 0;
}

int orbmch_hw_s_wdma_init(struct is_common_dma *dma, struct orbmch_param_set *param_set, u32 enable, u32 set_id)
{
	u32 stride;
	u32 mch_output_mode_a;

	dbg_orbmch(4, "[API][%s] is called\n", __func__);

	if (enable == 0)
		return 0;

	switch (dma->id) {
	case ORBMCH_WDMA_ORB_KEY:
	case ORBMCH_WDMA_ORB_DESC:
		/* Set orb wdma stride */
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_WDMA0_ADDR_STRIDE_A, ORB_KEY_STRIDE);
		/* Set orb wdma mo */
		ORBMCH_SET_F(dma->base, ORBMCH_R_ORBMCH_ORB_DMA_MO_A, ORBMCH_F_ORBMCH_ORB_WDMA0_MAX_MO_A, 0x100);
		break;
	case ORBMCH_WDMA_MCH_RESULT:
		/* Set mch wdma stride */
		mch_output_mode_a = ORBMCH_GET_R(dma->base, ORBMCH_R_ORBMCH_MCH_OUTPUT_MODE_A);
		if (mch_output_mode_a == 0)
			stride = ORB_DB_MAX_SIZE * MCH_MAX_MATCH_NUM * MCH_MAX_OUTPUT_SIZE / 2;
		else
			stride = ORB_DB_MAX_SIZE * MCH_MAX_MATCH_NUM * MCH_MAX_OUTPUT_SIZE;

		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_MCH_WDMA0_ADDR_STRIDE_A, stride);
		/* Set orb wdma mo */
		ORBMCH_SET_F(dma->base, ORBMCH_R_ORBMCH_MCH_DMA_MO_A, ORBMCH_F_ORBMCH_MCH_WDMA0_MAX_MO_A, 0x100);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	return 0;
}

int orbmch_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	int ret = SET_SUCCESS;
	char name[64];

	dbg_orbmch(4, "[API][%s] is called!(%d)", __func__, dma_id);

	switch (dma_id) {
	case ORBMCH_RDMA_ORB_L1:
		strncpy(name, "ORBMCH_RDMA_ORB_L1", sizeof(name) - 1);
		break;
	case ORBMCH_RDMA_ORB_L2:
		strncpy(name, "ORBMCH_RDMA_ORB_L2", sizeof(name) - 1);
		break;
	case ORBMCH_RDMA_MCH_PREV_DESC:
		strncpy(name, "ORBMCH_RDMA_MCH_PREV_DESC", sizeof(name) - 1);
		break;
	case ORBMCH_RDMA_MCH_CUR_DESC:
		strncpy(name, "ORBMCH_RDMA_MCH_CUR_DESC", sizeof(name) - 1);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma_id);
		return SET_ERROR;
	}
	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base, dma_id, name, 0, 0, 0, 0);
	return ret;
}

int orbmch_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	int ret = SET_SUCCESS;
	char name[64];

	strncpy(name, "ORBMCH_WDMA", sizeof(name) - 1);
	switch (dma_id) {
	case ORBMCH_WDMA_ORB_KEY:
		strncpy(name, "ORBMCH_WDMA_ORB_KEY", sizeof(name) - 1);
		break;
	case ORBMCH_WDMA_ORB_DESC:
		strncpy(name, "ORBMCH_WDMA_ORB_DESC", sizeof(name) - 1);
		break;
	case ORBMCH_WDMA_MCH_RESULT:
		strncpy(name, "ORBMCH_WDMA_MCH_RESULT", sizeof(name) - 1);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma_id);
		return SET_ERROR;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base, dma_id, name, 0, 0, 0, 0);
	return ret;
}

void orbmch_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
}

void orbmch_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
}

int orbmch_hw_s_rdma_addr(struct is_common_dma *dma, dma_addr_t addr, u32 set_id)
{
	int ret;

	dbg_orbmch(4, "[API][%s] is called!", __func__);

	ret = SET_SUCCESS;

	switch (dma->id) {
	case ORBMCH_RDMA_ORB_L1:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_RDMA0_BASE_ADDR0_A, addr);
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_RDMA1_BASE_ADDR0_A, addr);
		break;
	case ORBMCH_RDMA_ORB_L2:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_RDMA0_BASE_ADDR1_A, addr);
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_RDMA1_BASE_ADDR1_A, addr);
		break;
	case ORBMCH_RDMA_MCH_PREV_DESC:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_MCH_RDMA0_BASE_ADDR0_A, addr);
		break;
	case ORBMCH_RDMA_MCH_CUR_DESC:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_MCH_RDMA0_BASE_ADDR1_A, addr);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma->id);
		ret = SET_ERROR;
		break;
	}

	return ret;
}

int orbmch_hw_s_wdma_addr(struct is_common_dma *dma, dma_addr_t addr, u32 set_id)
{
	int ret = SET_SUCCESS;

	switch (dma->id) {
	case ORBMCH_WDMA_ORB_KEY:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_WDMA0_BASE_ADDR0_A, addr);
		break;
	case ORBMCH_WDMA_ORB_DESC:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_ORB_WDMA0_BASE_ADDR1_A, addr);
		break;
	case ORBMCH_WDMA_MCH_RESULT:
		ORBMCH_SET_R(dma->base, ORBMCH_R_ORBMCH_MCH_WDMA0_BASE_ADDR0_A, addr);
		break;
	default:
		err_hw("[ORBMCH] invalid dma_id[%d]", dma->id);
		ret = SET_ERROR;
	}

	return ret;
}

int orbmch_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type)
{
	int ret = 0;

	switch (set_id) {
	case COREX_SET_A:
	case COREX_SET_B:
	case COREX_DIRECT:
		dbg_hw(1, "[ORBMCH] Not use corex\n", __func__);
		break;
	default:
		err_hw("[ORBMCH] invalid corex set_id");
		ret = -EINVAL;
		break;
	}

	return ret;
}

void orbmch_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= ORBMCH_TRY_COUNT) {
			err_hw("[ORBMCH] fail to wait corex idle");
			break;
		}

		busy = ORBMCH_GET_F(base, ORBMCH_R_ORBMCH_STATUS, ORBMCH_F_ORBMCH_BUSY);
		dbg_hw(1, "[ORBMCH] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

void orbmch_hw_s_enable(void __iomem *base)
{
	u32 val = 0;

	/* Enable */
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_ASET_NUM, 0);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_BSET_NUM, 0);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_SET_NUMBER, val);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_START, 1);
}

void orbmch_hw_s_corex_init(void __iomem *base, bool enable)
{
	/* Not Support */
}

/*
 * Context: O
 * CR type: No Corex
 */
void orbmch_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;

	info_itfc("[DBG] %s is called!", __func__);
	info_hw("[ORBMCH] %s done\n", __func__);
}

unsigned int orbmch_hw_g_int_status(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, src_err;

	/*
	 * src_all: per-frame based orbmch IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = ORBMCH_GET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_STATUS);

	dbg_orbmch(4, "[API][%s] src_all: 0x%x\n", __func__, src_all);

	if (src_all & 1 << INTR_STATUS_ORBMCH_SETTING_ERROR)
		ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_STATUS, 1 << INTR_STATUS_ORBMCH_SETTING_ERROR);
	if (clear)
		ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_INTERRUPT_STATUS, src_all);

	src_err = src_all & ORBMCH_INT_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

void orbmch_hw_s_block_bypass(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	int i;
	u32 val = 0;
	u32 l1_width = width;
	u32 l1_height = height;
	u32 l2_width = l1_width / 2;
	u32 l2_height = l1_height / 2;

	u32 s_x = 0, s_y = 0;
	u32 e_x, e_y;

	u32 orb_tile_w = 128;
	u32 orb_tile_h = 128;

	u32 tile_col_num_floor_a, tile_row_num_floor_a;

	dbg_orbmch(4, "[API][%s] is called!", __func__);

	dbg_hw(2, "[ORBMCH] %s: L1 size %d x %d, L2 size %d x %d\n", __func__,
		l1_width, l1_height,
		l2_width, l2_height);

	/* Set ORB RDMA Size */
	val = 0;
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_LV1_FRAME_WIDTH_A, l1_width);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_LV1_FRAME_HEIGHT_A, l1_height);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_LV1_FRAME_SIZE_A, val);

	val = 0;
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_LV2_FRAME_WIDTH_A, l2_width);
	val = ORBMCH_SET_V(val, ORBMCH_F_ORBMCH_LV2_FRAME_HEIGHT_A, l2_height);
	ORBMCH_SET_R(base, ORBMCH_R_ORBMCH_LV2_FRAME_SIZE_A, val);

	for (i = 0; i < 8; ++i) {
		s_x = (i % 4) * ((l1_width - 2 * 21) >> 2);
		s_y = (i / 4) ?  ((l1_height - 2 * 21) >> 1) : 0;
		if (l1_width > (s_x + ((l1_width - 2 * 21) >> 2) + 2 * 21))
			e_x = s_x + ((l1_width - 2 * 21) >> 2) + 2 * 21;
		else
			e_x = l1_width - 1;

		if (l1_height > (s_y + ((l1_height - 2 * 21) / 2) + 2 * 21))
			e_y = s_y + ((l1_height - 2 * 21) / 2) + 2 * 21;
		else
			e_y = l1_height - 1;
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_REG0_START_XY_A + i * 2,
			ORBMCH_F_ORBMCH_REG0_START_X_A + i * 4, s_x);
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_REG0_START_XY_A + i * 2,
			ORBMCH_F_ORBMCH_REG0_START_Y_A + i * 4, s_y);
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_REG0_END_XY_A + i * 2, ORBMCH_F_ORBMCH_REG0_END_X_A + i * 4, e_x);
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_REG0_END_XY_A + i * 2, ORBMCH_F_ORBMCH_REG0_END_Y_A + i * 4, e_y);

		tile_col_num_floor_a = ((e_x - s_x + 1) - 2 * 21) / (orb_tile_w - 2 * 21);
		tile_row_num_floor_a = ((e_y - s_y + 1) - 2 * 21) / (orb_tile_h - 2 * 21);
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_LV1_REG01_TILE_NUM_FLOOR_A + (i / 2),
				ORBMCH_F_ORBMCH_LV1_REG0_TILE_COL_NUM_FLOOR_A + i * 2, tile_col_num_floor_a);
		ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_LV1_REG01_TILE_NUM_FLOOR_A + (i / 2),
				ORBMCH_F_ORBMCH_LV1_REG0_TILE_ROW_NUM_FLOOR_A + i * 2, tile_row_num_floor_a);
	}

	tile_col_num_floor_a = (l2_width - 2 * 21) / (orb_tile_w - 2 * 21);
	tile_row_num_floor_a = (l2_height - 2 * 21) / (orb_tile_h - 2 * 21);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_LV2_TILE_NUM_FLOOR_A,
				ORBMCH_F_ORBMCH_LV2_TILE_COL_NUM_FLOOR_A, tile_col_num_floor_a);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_LV2_TILE_NUM_FLOOR_A,
				ORBMCH_F_ORBMCH_LV2_TILE_ROW_NUM_FLOOR_A, tile_row_num_floor_a);

	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_ORB_TILE_WIDTH_A, ORBMCH_F_ORBMCH_ORB_TILE_WIDTH_A, orb_tile_w);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_ORB_TILE_HEIGHT_A, ORBMCH_F_ORBMCH_ORB_TILE_HEIGHT_A, orb_tile_h);

	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_MCH_MATCH_NUM_A, ORBMCH_F_ORBMCH_MCH_MATCH_NUM_A, 3);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_ORB_DIAMETER_A, ORBMCH_F_ORBMCH_ORB_DIAMETER_A, 35);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_ORB_KPT_MAX_NUM_A, ORBMCH_F_ORBMCH_ORB_KPT_MAX_NUM_A, 300);
	ORBMCH_SET_F(base, ORBMCH_R_ORBMCH_MODE_A, ORBMCH_F_ORBMCH_MODE_A, 3);
}

void orbmch_hw_s_frame_done(void __iomem *base, u32 instance, u32 fcount,
	struct orbmch_param_set *param_set, struct orbmch_data *data)
{
	int i = 0;
	u32 orb_cnt[ORB_REGION_NUM];
	u32 mch_cnt[ORB_REGION_NUM];

	/* Copy current descriptor data to hal output buffer */
	if (param_set->output_kva_motion_pure[0] && param_set->prev_output_kva[1])
		memcpy((void *)(param_set->output_kva_motion_pure[0] + ORB_KEY_BUF_SIZE),
			(void *)param_set->prev_output_kva[1],
			ORB_DESC_BUF_SIZE);
	else
		err_hw("[%d][F:%d][ORBMCH] Descriptor buffer or Pure motion buffer[0] is NULL (cur desc:%#p, motion buffer[0]:%#p)",
			instance,
			fcount,
			param_set->prev_output_kva[1],
			param_set->output_kva_motion_pure[0]);
	/* Write orb count and mch count on buffer for EIS */
	for (i = 0; i < ORB_REGION_NUM; ++i) {
		orb_cnt[i] = ORBMCH_GET_F(base, ORBMCH_R_ORBMCH_REG01_ORB_OUTPUT_COUNT + (int)(i / 2),
			ORBMCH_F_ORBMCH_REG0_ORB_OUTPUT_COUNT + i);
		mch_cnt[i] = ORBMCH_GET_F(base, ORBMCH_R_ORBMCH_REG01_MCH_OUTPUT_COUNT + (int)(i / 2),
			ORBMCH_F_ORBMCH_REG0_MCH_OUTPUT_COUNT + i);
	}
	dbg_hw(4, "[%d][F:%d][ORBMCH] %s orb output count(%d, %d, %d, %d, %d, %d, %d, %d, %d)\n",
		instance, fcount, __func__,
		orb_cnt[0], orb_cnt[1], orb_cnt[2], orb_cnt[3], orb_cnt[4],
		orb_cnt[5], orb_cnt[6], orb_cnt[7], orb_cnt[8]);
	dbg_hw(4, "[%d][F:%d][ORBMCH] %s mch output count(%d, %d, %d, %d, %d, %d, %d, %d, %d)\n",
		instance, fcount, __func__,
		mch_cnt[0], mch_cnt[1], mch_cnt[2], mch_cnt[3], mch_cnt[4],
		mch_cnt[5], mch_cnt[6], mch_cnt[7], mch_cnt[8]);

	if (param_set->output_kva_motion_pure[1]) {
		memcpy((void *)param_set->output_kva_motion_pure[1] + MCH_OUT_BUF_SIZE,
			(void *)orb_cnt, sizeof(orb_cnt));
		memcpy((void *)param_set->output_kva_motion_pure[1] + MCH_OUT_BUF_SIZE + sizeof(orb_cnt),
			(void *)mch_cnt, sizeof(mch_cnt));
	} else {
		err_hw("[%d][F:%d][ORBMCH] Pure motion buffer[1] is NULL", instance, fcount);
	}

	data->kpt_kva = param_set->output_kva_motion_pure[0];
	data->mch_kva = param_set->output_kva_motion_pure[1];
	data->lmc_kva = param_set->output_kva_motion_processed[0];
}

void orbmch_hw_dump(void __iomem *base)
{
	info_hw("[ORBMCH] SFR DUMP (v2.1)\n");

	is_hw_dump_regs(base, orbmch_regs, ORBMCH_REG_CNT);
}

void orbmch_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0x0);
}

struct is_reg *orbmch_hw_get_reg_struct(void)
{
	return (struct is_reg *)orbmch_regs;
}

unsigned int orbmch_hw_get_reg_cnt(void)
{
	return ORBMCH_REG_CNT;
}
