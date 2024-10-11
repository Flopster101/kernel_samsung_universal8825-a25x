// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MCFP HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "sfr/is-sfr-mcfp-v10_20.h"
#include "is-hw-api-mcfp-v10_20.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"

#define MCFP0_SET_F(base, R, F, val) \
	is_hw_set_field(base, &mcfp0_regs[R], &mcfp0_fields[F], val)
#define MCFP0_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &mcfp0_regs[R], &mcfp0_fields[F], val)
#define MCFP0_SET_F_COREX(base, set_id, R, F, val) \
	is_hw_set_field(base + GET_COREX_OFFSET(set_id), &mcfp0_regs[R], &mcfp0_fields[F], val)
#define MCFP0_SET_R(base, R, val) \
	is_hw_set_reg(base, &mcfp0_regs[R], val)
#define MCFP0_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &mcfp0_regs[R], val)
#define MCFP0_SET_R_COREX(base, set_id, R, val) \
	is_hw_set_reg(base + GET_COREX_OFFSET(set_id), &mcfp0_regs[R], val)
#define MCFP0_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &mcfp0_fields[F], val)

#define MCFP0_GET_F(base, R, F) \
	is_hw_get_field(base, &mcfp0_regs[R], &mcfp0_fields[F])
#define MCFP0_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &mcfp0_regs[R], &mcfp0_fields[F])
#define MCFP0_GET_F_COREX(base, set_id, R, F) \
	is_hw_get_field(base + GET_COREX_OFFSET(set_id), &mcfp0_regs[R], &mcfp0_fields[F])
#define MCFP0_GET_R(base, R) \
	is_hw_get_reg(base, &mcfp0_regs[R])
#define MCFP0_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base, &mcfp0_regs[R])
#define MCFP0_GET_R_COREX(base, set_id, R) \
	is_hw_get_reg(base + GET_COREX_OFFSET(set_id), &mcfp0_regs[R])
#define MCFP0_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &mcfp0_fields[F])

#define MCFP1_SET_F(base, R, F, val) \
	is_hw_set_field(base, &mcfp1_regs[R], &mcfp1_fields[F], val)
#define MCFP1_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &mcfp1_regs[R], &mcfp1_fields[F], val)
#define MCFP1_SET_F_COREX(base, set_id, R, F, val) \
	is_hw_set_field(base + GET_COREX_OFFSET(set_id), &mcfp1_regs[R], &mcfp1_fields[F], val)
#define MCFP1_SET_R(base, R, val) \
	is_hw_set_reg(base, &mcfp1_regs[R], val)
#define MCFP1_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &mcfp1_regs[R], val)
#define MCFP1_SET_R_COREX(base, set_id, R, val) \
	is_hw_set_reg(base + GET_COREX_OFFSET(set_id), &mcfp1_regs[R], val)
#define MCFP1_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &mcfp1_fields[F], val)

#define MCFP1_GET_F(base, R, F) \
	is_hw_get_field(base, &mcfp1_regs[R], &mcfp1_fields[F])
#define MCFP1_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &mcfp1_regs[R], &mcfp1_fields[F])
#define MCFP1_GET_R(base, R) \
	is_hw_get_reg(base, &mcfp1_regs[R])
#define MCFP1_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base, &mcfp1_regs[R])
#define MCFP1_GET_R_COREX(base, set_id, R) \
	is_hw_get_reg(base + GET_COREX_OFFSET(set_id), &mcfp1_regs[R])
#define MCFP1_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &mcfp1_fields[F])

#define TNR_BGDC_MAX_WIDTH 8192
#define TNR_BGDC_MAX_HEIGHT 6144

#define VBLANK_CYCLE			(0x20)
#define HBLANK_CYCLE			(0x2E)
#define PBLANK_CYCLE			(0)

int mcfp0_hw_create_rdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id);
int mcfp0_hw_create_wdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id);
static void __mcfp0_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= MCFP_TRY_COUNT) {
			err_hw("[MCFP] fail to wait corex idle");
			break;
		}

		busy = MCFP0_GET_F_DIRECT(base, MCFP0_R_COREX_STATUS_0, MCFP0_F_COREX_BUSY_0);
		dbg_hw(1, "[MCFP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

static void __mcfp1_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= MCFP_TRY_COUNT) {
			err_hw("[MCFP] fail to wait corex idle");
			break;
		}

		busy = MCFP1_GET_F(base, MCFP1_R_COREX_STATUS_0, MCFP0_F_COREX_BUSY_0);
		dbg_hw(1, "[MCFP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

void mcfp0_hw_s_int_init(void __iomem *base)
{
	/*
	 * [0]: INT1 - Level (1), PULSE (0)
	 * [1]: INT2 - Level (1), PULSE (0)
	 */
	MCFP0_SET_R_DIRECT(base, MCFP0_R_CONTINT_MCFP_LEVEL_PULSE_N_SEL, 3);

	MCFP0_SET_R_DIRECT(base, MCFP0_R_CONTINT_MCFP_INT1_CLEAR, (1 << INTR1_MCFP0_MAX) - 1);
	MCFP0_SET_R_DIRECT(base, MCFP0_R_FRO_INT0_CLEAR, (1 << INTR1_MCFP0_MAX) - 1);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_CONTINT_MCFP_INT1_ENABLE, MCFP0_F_CONTINT_MCFP_INT1_ENABLE,
			MCFP_INT1_EN_MASK);
	MCFP0_SET_R_DIRECT(base, MCFP0_R_CONTINT_MCFP_INT2_CLEAR, (1 << INTR2_MCFP0_MAX) - 1);
	MCFP0_SET_R_DIRECT(base, MCFP0_R_FRO_INT1_CLEAR, (1 << INTR2_MCFP0_MAX) - 1);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_CONTINT_MCFP_INT2_ENABLE, MCFP0_F_CONTINT_MCFP_INT2_ENABLE,
			MCFP_INT2_EN_MASK);
}

/*
 * Context: O
 * CR type: Shadow/Dual
 */
/*
 * Set Paramer Value
 *
 * scenario
 * 0: Non-secure,  1: Secure
 */
/* TODO: get secure scenario */
/*
 * static void __mcfp0_hw_s_secure_id(void __iomem *base, u32 set_id)
 * {
 *		MCFP0_SET_F(base + GET_COREX_OFFSET(set_id),
 *		MCFP0_R_SECU_CTRL_SEQID,
 *		MCFP0_F_SECU_CTRL_SEQID, 0);
 * }
 */

u32 __mcfp0_hw_is_occurred_int1(unsigned int status, enum mcfp_event_type type)
{
	u32 mask;

	switch (type) {
	case MCFP_EVENT_FRAME_START:
		mask = 1 << INTR1_MCFP0_FRAME_START;
		break;
	case MCFP_EVENT_FRAME_END:
		mask = 1 << INTR1_MCFP0_FRAME_END;
		break;
	case MCFP_EVENT_ERR:
		mask = MCFP_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

u32 __mcfp0_hw_is_occurred_int2(unsigned int status, enum mcfp_event_type type)
{
	u32 mask;

	switch (type) {
	case MCFP_EVENT_ERR:
		mask = MCFP_INT2_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

u32 mcfp0_hw_is_occurred(unsigned int status, u32 irq_id, enum mcfp_event_type type)
{
	u32 ret = 0;

	switch (irq_id) {
	case INTR_HWIP3:
		ret = __mcfp0_hw_is_occurred_int1(status, type);
		break;
	case INTR_HWIP4:
		ret = __mcfp0_hw_is_occurred_int2(status, type);
		break;
	default:
		err_hw("[MCFP] invalid irq_id (%d)", irq_id);
		break;
	}
	return ret;
}

int mcfp0_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp = 0;

	MCFP0_SET_R_DIRECT(base, MCFP0_R_SW_RESET, 1);

	while (1) {
		temp = MCFP0_GET_R_DIRECT(base, MCFP0_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > MCFP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	/*
	 * Set FRO_SW_RESET
	 * 0: N/A
	 * 1: Core for FRO controller
	 * 2: Core and Configuration register for FRO controller
	 */
	MCFP0_SET_R_DIRECT(base, MCFP0_R_FRO_SW_RESET, 2);

	info_hw("[MCFP] %s done.\n", __func__);

	return 0;
}

void mcfp0_hw_s_control_init(void __iomem *base, u32 set_id)
{
	/* MCFP0 */
	MCFP0_SET_R_DIRECT(base, MCFP0_R_IP_PROCESSING, 1);

	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_IP_CORRUPTED_INTERRUPT_ENABLE, 0xFFFF);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_IP_CHAIN_INPUT_SELECT, 2);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_TOP_BCROP_STALL_EN, 1);
}

void mcfp1_hw_s_control_init(void __iomem *base, u32 set_id)
{
	/* MCFP1 */
	MCFP1_SET_R_DIRECT(base, MCFP1_R_IP_PROCESSING, 1);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_LINE_GAP, 70);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_BCROP_STALL_EN, 1);
}

int mcfp0_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int0_all, int1_all;
	u32 try_cnt = 0;

	idle = MCFP0_GET_F(base, MCFP0_R_IDLENESS_STATUS, MCFP0_F_IDLENESS_STATUS);
	int0_all = MCFP0_GET_R(base, MCFP0_R_CONTINT_MCFP_INT1_STATUS);
	int1_all = MCFP0_GET_R(base, MCFP0_R_CONTINT_MCFP_INT2_STATUS);

	info_hw("[MCFP] idle status before disable (idle:%d, int:0x%X, 0x%X)\n",
			idle, int0_all, int1_all);

	while (!idle) {
		idle = MCFP0_GET_F(base, MCFP0_R_IDLENESS_STATUS, MCFP0_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= MCFP_TRY_COUNT) {
			err_hw("[MCFP] timeout waiting idle - disable fail");
			mcfp0_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = MCFP0_GET_R(base, MCFP0_R_CONTINT_MCFP_INT1_STATUS);
	int1_all = MCFP0_GET_R(base, MCFP0_R_CONTINT_MCFP_INT2_STATUS);

	info_hw("[MCFP] idle status after disable (total:%d, curr:%d, idle:%d, int:0x%X, 0x%X)\n",
			idle, int0_all, int1_all);

	return ret;
}

void mcfp0_hw_dump(void __iomem *base)
{
	info_hw("[MCFP0] SFR DUMP (v10.20)\n");
	is_hw_dump_regs(base, mcfp0_regs, MCFP0_REG_CNT);
}

void mcfp1_hw_dump(void __iomem *base)
{
	info_hw("[MCFP1] SFR DUMP (v10.20)\n");
	is_hw_dump_regs(base, mcfp1_regs, MCFP1_REG_CNT);
}

void mcfp0_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

int mcfp0_hw_create_rdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char name[64];

	switch (dma_id) {
	case MCFP_RDMA_CURR_BAYER:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_BAYER_CURR_EN].sfr_offset;
		available_bayer_format_map = (0
						| BIT_MASK(DMA_FMT_U8BIT_PACK)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_PACK)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_PACK)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_PACK)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_PACK)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_PACK)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_PACK)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_PACK)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
						) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_CUR_BAYER", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_MOTION0:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_MOTION0_PREV_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_MOTION0", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_MOTION1:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_MOTION1_PREV_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_MOTION1", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_WGT:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_WGT_PREV_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_WGT", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_BAYER0:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_BAYER_PREV0_EN].sfr_offset;
		available_bayer_format_map = (0
						| BIT_MASK(DMA_FMT_U8BIT_PACK)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_PACK)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_PACK)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_PACK)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_PACK)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_PACK)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_PACK)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_PACK)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
						) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_BAYER0", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_BAYER1:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_BAYER_PREV1_EN].sfr_offset;
		available_bayer_format_map = (0
						| BIT_MASK(DMA_FMT_U8BIT_PACK)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_PACK)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_PACK)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_PACK)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_PACK)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_PACK)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_PACK)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_PACK)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
						) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_BAYER1", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_BAYER2:
		base_reg = base + mcfp0_regs[MCFP0_R_RDMA_BAYER_PREV2_EN].sfr_offset;
		available_bayer_format_map = (0
						| BIT_MASK(DMA_FMT_U8BIT_PACK)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_PACK)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_PACK)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_PACK)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_PACK)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_PACK)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_PACK)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_PACK)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
						) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_BAYER2", sizeof(name) - 1);
		break;
	default:
		err_hw("[MCFP] invalid input_id[%d]", dma_id);
		return SET_ERROR;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name, available_bayer_format_map, 0, 0, 0);

	return ret;
}

int mcfp0_hw_create_wdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char name[64];

	switch (dma_id) {
	case MCFP_WDMA_PREV_BAYER:
		base_reg = base + mcfp0_regs[MCFP0_R_WDMA_BAYER_EN].sfr_offset;
		available_bayer_format_map = (0
						| BIT_MASK(DMA_FMT_U8BIT_PACK)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_PACK)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_PACK)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_PACK)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_PACK)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S8BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_PACK)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S10BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_PACK)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S12BIT_UNPACK_MSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_PACK)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_LSB_ZERO)
						| BIT_MASK(DMA_FMT_S14BIT_UNPACK_MSB_ZERO)
						) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_WDMA_PREV_BAYER", sizeof(name) - 1);
		break;
	case MCFP_WDMA_PREV_WGT:
		base_reg = base + mcfp0_regs[MCFP0_R_WDMA_WGT_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_WDMA_PREV_WGT", sizeof(name) - 1);
		break;
	default:
		err_hw("[MCFP] invalid output_id[%d]", dma_id);
		return SET_ERROR;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name, available_bayer_format_map, 0, 0, 0);

	return ret;
}

int mcfp0_hw_s_rdma_init(struct is_common_dma *dma, void __iomem *base,
	struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *sbwc_en, u32 *payload_size,
	u32 *strip_dma_offset)
{
	u32 comp_sbwc_en = 0, comp_64b_align = 0, lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, msb_align, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_width;
	u32 format;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 stripe_idx = stripe_input->index;
	u32 strip_start_pos_x;
	int ret = SET_SUCCESS;

	full_width = (strip_enable) ? stripe_input->full_width : dma_input->width;
	strip_start_pos_x = strip_enable ?
		stripe_input->stripe_roi_start_pos_x[stripe_idx] - stripe_input->left_margin : 0;
	switch (dma->id) {
	case MCFP_RDMA_CURR_BAYER:
		width = dma_input->width;
		height = dma_input->height;
		hwformat = dma_input->format;
		sbwc_type = 0;
		memory_bitwidth = dma_input->bitwidth;
		pixelsize = dma_input->msb + 1;
		msb_align = 1;
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				full_width, 16, true);
		break;
	case MCFP_RDMA_PREV_BAYER0:
	case MCFP_RDMA_PREV_BAYER1:
	case MCFP_RDMA_PREV_BAYER2:
		width = full_width;
		height = dma_input->height;
		hwformat = dma_input->format;
		sbwc_type = dma_input->sbwc_type;
		/*
		 * If Bayer reduction mode is enabled, Use s11 dma format (img_bit: 11)
		 * img_bit can be 9, 11, 13, but use 11, 13
		 * because bayer format (w/ sbwc) can be s11, s13
		 */
		if (!(config->img_bit == DMA_INOUT_BIT_WIDTH_9BIT ||
			config->img_bit == DMA_INOUT_BIT_WIDTH_11BIT ||
			config->img_bit == DMA_INOUT_BIT_WIDTH_13BIT)) {
			err_hw("[MCFP] invalid setconfig img_bit[%d], forcely change img_bit %d -> %d", config->img_bit,
				config->img_bit, DMA_INOUT_BIT_WIDTH_13BIT);
			config->img_bit = DMA_INOUT_BIT_WIDTH_13BIT;
			memory_bitwidth = dma_input->bitwidth;
			pixelsize = dma_input->msb + 1;
		} else {
			memory_bitwidth = config->img_bit;
			pixelsize = config->img_bit;
		}
		msb_align = dma_input->bitwidth - (config->img_bit) ? 1 : 0;
		/*
		 * HW Guide
		 * sbwc lossyByte32Num value - S11 : 3 [3~5]
		 * sbwc lossyByte32Num value - S13 : 4 [3~6]
		 */
		lossy_byte32num = config->img_bit < DMA_INOUT_BIT_WIDTH_13BIT ?
							COMP_LOSSYBYTE32NUM_32X4_U12 : COMP_LOSSYBYTE32NUM_32X4_U14;
		if (sbwc_type != 0 && config->img_bit == DMA_INOUT_BIT_WIDTH_9BIT) {
			err_hw("[MCFP] invalid setconfig img_bit[%d], not support format with sbwc simultaneously",
					config->img_bit);
			sbwc_type = 0;
		}
		break;
	case MCFP_RDMA_PREV_MOTION0:
	case MCFP_RDMA_PREV_MOTION1:
		/* Motion vector width/height */
		width = DIV_ROUND_UP(dma_input->width, 32) * 4;
		height = DIV_ROUND_UP(dma_input->height, 32);
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = 0;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		msb_align = 0;
		/* Motion stride is the full motion vector width(DIV_ROUND_UP(full_width, 32)) * 4 */
		stride_1p = DIV_ROUND_UP(full_width, 32) * 4;
		if (!IS_ALIGNED(strip_start_pos_x, 32))
			err_hw("[MCFP] prev motion start_pos_x(%d) is not multiple of 32", strip_start_pos_x);
		*strip_dma_offset = strip_start_pos_x / 32 * 4;
		break;
	case MCFP_RDMA_PREV_WGT:
		width = (config->wgt_bit == DMA_INOUT_BIT_WIDTH_4BIT) ? full_width / 4 : full_width / 2;
		height = dma_input->height / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = 0;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		msb_align = 0;
		stride_1p = (config->wgt_bit == DMA_INOUT_BIT_WIDTH_4BIT) ? ALIGN(full_width / 4, 2) : full_width / 2;
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true);

	if (comp_sbwc_en == 0) {
		if (stride_1p == 0)
			stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
					width, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
			comp_64b_align, lossy_byte32num, MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, MCFP_COMP_BLOCK_WIDTH, 16) : 0;
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_error, COMP_ERROR_MODE_USE_0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_msb_align, msb_align);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MCFP_COMP_BLOCK_HEIGHT - 1) / MCFP_COMP_BLOCK_HEIGHT) * stride_1p;
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

int mcfp0_hw_s_wdma_init(struct is_common_dma *dma,
	struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *sbwc_en, u32 *payload_size,
	u32 *strip_dma_offset, u32 *strip_hdr_dma_offset)
{
	u32 comp_sbwc_en = 0, comp_64b_align = 0, lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, msb_align, pixelsize, sbwc_type;
	u32 strip_width, width, height;
	u32 full_width;
	u32 strip_start_pos_x, strip_margin;
	u32 format;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	int ret = SET_SUCCESS;

	full_width = (strip_enable) ? stripe_input->full_width : dma_input->width;
	strip_start_pos_x = (strip_enable) ? stripe_input->stripe_roi_start_pos_x[stripe_input->index] : 0;
	switch (dma->id) {
	case MCFP_WDMA_PREV_BAYER:
		strip_margin = (strip_enable) ?
			stripe_input->left_margin + stripe_input->right_margin : 0;
		width = dma_input->width - strip_margin;
		height = dma_input->height;
		hwformat = dma_input->format;
		sbwc_type = dma_input->sbwc_type;
		/*
		 * If Bayer reduction mode is enabled, Use s11 dma format (img_bit: 11)
		 * img_bit can be 9, 11, 13, but use 11, 13
		 * because bayer format (w/ sbwc) can be s11, s13
		 */
		if (!(config->img_bit == DMA_INOUT_BIT_WIDTH_9BIT ||
			config->img_bit == DMA_INOUT_BIT_WIDTH_11BIT ||
			config->img_bit == DMA_INOUT_BIT_WIDTH_13BIT)) {
			err_hw("[MCFP] invalid setconfig img_bit[%d], forcely change img_bit %d -> %d", config->img_bit,
				config->img_bit, DMA_INOUT_BIT_WIDTH_13BIT);
			config->img_bit = DMA_INOUT_BIT_WIDTH_13BIT;
			memory_bitwidth = dma_input->bitwidth;
			pixelsize = dma_input->msb + 1;
		} else {
			memory_bitwidth = config->img_bit;
			pixelsize = config->img_bit;
		}
		msb_align = 0;
		/*
		 * HW Guide
		 * sbwc lossyByte32Num value - S11 : 3 [3~5]
		 * sbwc lossyByte32Num value - S13 : 4 [3~6]
		 */
		lossy_byte32num = config->img_bit < DMA_INOUT_BIT_WIDTH_13BIT ?
							COMP_LOSSYBYTE32NUM_32X4_U12 : COMP_LOSSYBYTE32NUM_32X4_U14;
		if (sbwc_type != 0 && config->img_bit == DMA_INOUT_BIT_WIDTH_9BIT) {
			err_hw("[MCFP] invalid setconfig img_bit[%d], not support format with sbwc simultaneously",
					config->img_bit);
			sbwc_type = 0;
		}
		break;
	case MCFP_WDMA_PREV_WGT:
		strip_margin = (strip_enable) ?
			stripe_input->left_margin + stripe_input->right_margin : 0;
		strip_width = dma_input->width - strip_margin;

		width = (config->wgt_bit == DMA_INOUT_BIT_WIDTH_4BIT) ? ALIGN(strip_width / 4, 2) : strip_width / 2;
		height = dma_input->height / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = 0;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		msb_align = 0;
		stride_1p = (config->wgt_bit == DMA_INOUT_BIT_WIDTH_4BIT) ? ALIGN(full_width / 4, 2) : full_width / 2;
		*strip_dma_offset = (config->wgt_bit == DMA_INOUT_BIT_WIDTH_4BIT) ?
							ALIGN(strip_start_pos_x / 4, 2) : strip_start_pos_x / 2;
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true);

	switch (comp_sbwc_en) {
	case 2:
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_width,
			comp_64b_align, lossy_byte32num, MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT);
		header_stride_1p = 0;

		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);

		*strip_dma_offset = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, strip_start_pos_x,
			comp_64b_align, lossy_byte32num, MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT);
		*strip_hdr_dma_offset = 0;
		*payload_size = ((height + MCFP_COMP_BLOCK_HEIGHT - 1) / MCFP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 1:
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_width,
			comp_64b_align, lossy_byte32num, MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT);
		header_stride_1p = is_hw_dma_get_header_stride(full_width, MCFP_COMP_BLOCK_WIDTH, 16);

		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);

		*strip_dma_offset = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, strip_start_pos_x,
			comp_64b_align, lossy_byte32num, MCFP_COMP_BLOCK_WIDTH, MCFP_COMP_BLOCK_HEIGHT);
		*strip_hdr_dma_offset = is_hw_dma_get_header_stride(strip_start_pos_x, MCFP_COMP_BLOCK_WIDTH, 0);
		*payload_size = ((height + MCFP_COMP_BLOCK_HEIGHT - 1) / MCFP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 0:
		if (stride_1p == 0)
			stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
					full_width, 16, true);
		if (*strip_dma_offset == 0)
			*strip_dma_offset = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
									strip_start_pos_x, 1, true);
		*strip_hdr_dma_offset = 0;
		*payload_size = 0;
		break;
	default:
		err_hw("[MCFP] invalid comp_sbwc_en[%d]", comp_sbwc_en);
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_error, COMP_ERROR_MODE_USE_0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_msb_align, msb_align);

	return ret;
}

void mcfp0_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int mcfp0_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr,
	u32 num_buffers, int plane, int buf_idx, u32 sbwc_en, u32 payload_size,
	u32 strip_dma_offset)
{
	int ret = SET_SUCCESS, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MCFP_RDMA_CURR_BAYER:
	case MCFP_RDMA_PREV_BAYER0:
	case MCFP_RDMA_PREV_BAYER1:
	case MCFP_RDMA_PREV_BAYER2:
	case MCFP_RDMA_PREV_MOTION0:
	case MCFP_RDMA_PREV_MOTION1:
	case MCFP_RDMA_PREV_WGT:
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}
	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + i) + (dma_addr_t)strip_dma_offset;

	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);

	if (sbwc_en == COMP) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		/* Not support SBWC for current bayer */
		case MCFP_RDMA_PREV_BAYER0:
		case MCFP_RDMA_PREV_BAYER1:
		case MCFP_RDMA_PREV_BAYER2:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, (dma_addr_t *)hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}

int mcfp0_hw_s_wdma_addr(struct is_common_dma *dma, u32 *addr,
	u32 num_buffers, int plane, int buf_idx, u32 sbwc_en, u32 payload_size,
	u32 strip_dma_offset, u32 strip_hdr_dma_offset)
{
	int ret = SET_SUCCESS, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MCFP_WDMA_PREV_BAYER:
	case MCFP_WDMA_PREV_WGT:
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + i) + (dma_addr_t)strip_dma_offset;

	ret = CALL_DMA_OPS(dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);

	if (sbwc_en == COMP) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case MCFP_WDMA_PREV_BAYER:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)*(addr + i) + payload_size + strip_hdr_dma_offset;
			break;
		default:
			break;
		}
		ret = CALL_DMA_OPS(dma, dma_set_header_addr, (dma_addr_t *)hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}

int mcfp0_hw_s_rdma(struct is_common_dma *dma, void __iomem *base, u32 set_id,
	struct param_dma_input *dma_input, u32 dma_en,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *addr, u32 num_buffers)
{
	int ret;
	u32 sbwc_en, payload_size;
	u32 strip_dma_offset = 0;

	mcfp0_hw_s_dma_corex_id(dma, set_id);
	CALL_DMA_OPS(dma, dma_enable, dma_en);
	if (dma_en == 0) {
		CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, 0);
		return 0;
	}

	ret = mcfp0_hw_s_rdma_init(dma, base, dma_input, stripe_input, config,
		&sbwc_en, &payload_size, &strip_dma_offset);
	if (ret < 0)
		return ret;

	ret = mcfp0_hw_s_rdma_addr(dma, addr, num_buffers, 0, 0, sbwc_en, payload_size, strip_dma_offset);
	if (ret < 0)
		return ret;

	return 0;
}

int mcfp0_hw_s_wdma(struct is_common_dma *dma, void __iomem *base, u32 set_id,
	struct param_dma_input *dma_input, u32 dma_en,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *addr, u32 num_buffers)
{
	int ret;
	u32 sbwc_en, payload_size;
	u32 strip_dma_offset = 0, strip_hdr_dma_offset = 0;

	mcfp0_hw_s_dma_corex_id(dma, set_id);
	ret = CALL_DMA_OPS(dma, dma_enable, dma_en);
	if (dma_en == 0) {
		CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, 0);
		return 0;
	}

	ret = mcfp0_hw_s_wdma_init(dma, dma_input, stripe_input, config,
		&sbwc_en, &payload_size, &strip_dma_offset, &strip_hdr_dma_offset);
	if (ret < 0)
		return ret;

	ret = mcfp0_hw_s_wdma_addr(dma, addr, num_buffers, 0, 0, sbwc_en, payload_size,
		strip_dma_offset, strip_hdr_dma_offset);
	if (ret < 0)
		return ret;

	return 0;
}

void mcfp0_hw_s_cmdq_queue(void __iomem *base, u32 set_id)
{
	u32 val = 0;

	MCFP0_SET_F_DIRECT(base, MCFP0_R_CQ_FRM_START_TYPE, MCFP0_F_CQ_FRM_START_TYPE, 0);
	if (set_id <= COREX_SET_D)
		val = (set_id & 0x3) + (1 << 4) + (1 << 8);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_MS_ADD_TO_QUEUE, MCFP0_F_MS_ADD_TO_QUEUE, val);
}

void mcfp1_hw_s_cmdq_queue(void __iomem *base, u32 set_id)
{
	MCFP1_SET_F_DIRECT(base, MCFP1_R_MS_ADD_TO_QUEUE, MCFP1_F_MS_ADD_TO_QUEUE, 0);
}

void mcfp0_hw_s_enable(void __iomem *base, u32 set_id)
{
	/* MCFP0 Start */
	mcfp0_hw_s_cmdq_queue(base, set_id);
}

void mcfp1_hw_s_enable(void __iomem *base, u32 set_id)
{
	/* MCFP1 Start */
	mcfp1_hw_s_cmdq_queue(base, set_id);
}

void mcfp0_hw_s_corex_init(void __iomem *base, bool enable)
{
	u32 temp;
	u32 reset_count = 0;
	u32 val = 0;

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		__mcfp0_hw_wait_corex_idle(base);

		MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_ENABLE, 0);
		info_hw("[MCFP] %s disable done.\n", __func__);
		return;
	}

	MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_ENABLE, 1);
	MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_RESET, 1);

	while (1) {
		temp = MCFP0_GET_R_DIRECT(base, MCFP0_R_COREX_RESET);
		if (temp == 0)
			break;
		if (reset_count > MCFP_TRY_COUNT) {
			err_hw("[MCFP0] Corex reset time-out\n");
			return;
		}
		reset_count++;
	}

	/* fast iAPB access */
	MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_FAST_MODE, 1);

	val = MCFP0_SET_V(val, MCFP0_F_COREX_MS_COPY_FROM_IP_TO_SETA_PLUS, 1);
	val = MCFP0_SET_V(val, MCFP0_F_COREX_MS_COPY_FROM_IP_TO_SETB_PLUS, 1);
	val = MCFP0_SET_V(val, MCFP0_F_COREX_MS_COPY_FROM_IP_TO_SETC_PLUS, 1);
	val = MCFP0_SET_V(val, MCFP0_F_COREX_MS_COPY_FROM_IP_TO_SETD_PLUS, 1);
	MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_MS_COPY_FROM_IP_MULTI, val);

	MCFP0_SET_R_DIRECT(base, MCFP0_R_COREX_COPY_FROM_IP_0, 1);

	/*
	 * Update type
	 * 0 - ignore (corex not used)
	 * 1 - copy
	 */
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_0_SETA, MCFP0_F_COREX_UPDATE_TYPE_0_SETA, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_0_SETB, MCFP0_F_COREX_UPDATE_TYPE_0_SETB, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_0_SETC, MCFP0_F_COREX_UPDATE_TYPE_0_SETC, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_0_SETD, MCFP0_F_COREX_UPDATE_TYPE_0_SETD, 0);

	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_1_SETA, MCFP0_F_COREX_UPDATE_TYPE_1_SETA, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_1_SETB, MCFP0_F_COREX_UPDATE_TYPE_1_SETB, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_1_SETC, MCFP0_F_COREX_UPDATE_TYPE_1_SETC, 0);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_COREX_UPDATE_TYPE_1_SETD, MCFP0_F_COREX_UPDATE_TYPE_1_SETD, 0);
}

void mcfp1_hw_s_corex_init(void __iomem *base, bool enable)
{
	u32 temp;
	u32 val = 0;
	u32 reset_count = 0;

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		__mcfp1_hw_wait_corex_idle(base);

		MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_ENABLE, 0);
		info_hw("[MCFP1] %s disable done.\n", __func__);
		return;
	}

	MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_ENABLE, 1);
	MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_RESET, 1);

	while (1) {
		temp = MCFP1_GET_R_DIRECT(base, MCFP1_R_COREX_RESET);
		if (temp == 0)
			break;
		if (reset_count > MCFP_TRY_COUNT) {
			err_hw("[MCFP1] Corex reset time-out\n");
			return;
		}
		reset_count++;
	}

	/* fast iAPB access */
	MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_FAST_MODE, 1);

	val = MCFP1_SET_V(val, MCFP1_F_COREX_MS_COPY_FROM_IP_TO_SETA_PLUS, 1);
	val = MCFP1_SET_V(val, MCFP1_F_COREX_MS_COPY_FROM_IP_TO_SETB_PLUS, 1);
	val = MCFP1_SET_V(val, MCFP1_F_COREX_MS_COPY_FROM_IP_TO_SETC_PLUS, 1);
	val = MCFP1_SET_V(val, MCFP1_F_COREX_MS_COPY_FROM_IP_TO_SETD_PLUS, 1);
	MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_MS_COPY_FROM_IP_MULTI, val);

	MCFP1_SET_R_DIRECT(base, MCFP1_R_COREX_COPY_FROM_IP_0, 1);

	/*
	 * Update type
	 * 0 - ignore (corex not used)
	 * 1 - copy
	 * Currently, set
	 */
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_0_SETA, MCFP1_F_COREX_UPDATE_TYPE_0_SETA, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_0_SETB, MCFP1_F_COREX_UPDATE_TYPE_0_SETB, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_0_SETC, MCFP1_F_COREX_UPDATE_TYPE_0_SETC, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_0_SETD, MCFP1_F_COREX_UPDATE_TYPE_0_SETD, 0);

	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_1_SETA, MCFP1_F_COREX_UPDATE_TYPE_1_SETA, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_1_SETB, MCFP1_F_COREX_UPDATE_TYPE_1_SETB, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_1_SETC, MCFP1_F_COREX_UPDATE_TYPE_1_SETC, 0);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_COREX_UPDATE_TYPE_1_SETD, MCFP1_F_COREX_UPDATE_TYPE_1_SETD, 0);
}

unsigned int mcfp0_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en)
{
	u32 int_status, fro_int_status;
	enum is_mcfp0_reg_name int_req, int_req_clear;
	enum is_mcfp0_reg_name fro_int_req, fro_int_req_clear;

	switch (irq_id) {
	case INTR_HWIP3:
		int_req = MCFP0_R_CONTINT_MCFP_INT1_STATUS;
		int_req_clear = MCFP0_R_CONTINT_MCFP_INT1_CLEAR;
		fro_int_req = MCFP0_R_FRO_INT0;
		fro_int_req_clear = MCFP0_R_FRO_INT0_CLEAR;
		break;
	case INTR_HWIP4:
		int_req = MCFP0_R_CONTINT_MCFP_INT2_STATUS;
		int_req_clear = MCFP0_R_CONTINT_MCFP_INT2_CLEAR;
		fro_int_req = MCFP0_R_FRO_INT1;
		fro_int_req_clear = MCFP0_R_FRO_INT1_CLEAR;
		break;
	default:
		err_hw("[MCFP] invalid irq_id[%d]", irq_id);
		return 0;
	}

	int_status = MCFP0_GET_R_DIRECT(base, int_req);
	fro_int_status = MCFP0_GET_R_DIRECT(base, fro_int_req);

	if (clear) {
		MCFP0_SET_R_DIRECT(base, int_req_clear, int_status);
		MCFP0_SET_R_DIRECT(base, fro_int_req_clear, fro_int_status);
	}

	return fro_en ? fro_int_status : int_status;
}

unsigned int mcfp0_hw_g_int_mask(void __iomem *base, u32 irq_index)
{
	enum is_mcfp0_reg_name int_req_enable;

	switch (irq_index) {
	case 0:
		int_req_enable = MCFP0_R_CONTINT_MCFP_INT1_ENABLE;
		break;
	case 1:
		int_req_enable = MCFP0_R_CONTINT_MCFP_INT2_ENABLE;
		break;
	default:
		err_hw("[MCFP] invalid irq_index[%d]", irq_index);
		return 0;
	}

	return MCFP0_GET_R_DIRECT(base, int_req_enable);
}

void mcfp0_hw_s_module_bypass(void __iomem *base, u32 set_id)
{
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_GEOMATCH_CTRL, MCFP0_F_GEOMATCH_BYPASS_EN, 1);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_GEOMATCH_CTRL, MCFP0_F_GEOMATCH_MATCH_ENABLE, 0);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BGDC_FORMAT, MCFP0_F_BGDC_MODE_EN, 0);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BGDC_FORMAT, MCFP0_F_BGDC_FUSION, 0);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_WBG_TNR_BYPASS, MCFP0_F_WBG_TNR_BYPASS, 1);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_GAMMA_PRE_TNR_BYPASS, MCFP0_F_GAMMA_PRE_TNR_BYPASS, 1);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BLC_TNR_BYPASS, MCFP0_F_BLC_TNR_BYPASS, 1);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BILATERAL_BF_CONTROL, MCFP0_F_BILATERAL_BF_PSEUDOY_EN, 0);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_BYPASS, 1);
	MCFP0_SET_F_COREX(base, set_id,
		MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_EN, 0);
}

void mcfp1_hw_s_module_bypass(void __iomem *base, u32 set_id)
{
	MCFP1_SET_F_COREX(base, set_id,
		MCFP1_R_PREVOUT_GAMMA_PRE_TNR_BYPASS, MCFP1_F_PREVOUT_GAMMA_PRE_TNR_BYPASS, 1);
	MCFP1_SET_F_COREX(base, set_id,
		MCFP1_R_PREVOUT_BLC_TNR_BYPASS, MCFP1_F_PREVOUT_BLC_TNR_BYPASS, 1);
	MCFP1_SET_F_COREX(base, set_id,
		MCFP1_R_TNR_ENABLE, MCFP1_F_TNR_ENABLE, 0);
	MCFP1_SET_F_COREX(base, set_id,
		MCFP1_R_BCROP_BYR_BYPASS, MCFP1_F_BCROP_BYR_BYPASS, 1);
	MCFP1_SET_F_COREX(base, set_id,
		MCFP1_R_BCROP_WGT_BYPASS, MCFP1_F_BCROP_WGT_BYPASS, 1);
}

void mcfp0_hw_s_top_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, bool prev_sbwc_enable, bool enable)
{
	u32 val = 0;

	/* Set top chain select */
	/* Should be '0' */
	val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_WDMA_WGT, 0);

	if (enable && prev_sbwc_enable)
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_WDMA_BAYER, 1);
	else
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_WDMA_BAYER, 0);

	/* Should be '0' because not support curr bayer sbwc */
	val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_IN_CURR, 0);

	if (enable == false || config->tnr_mode == 0 || config->geomatch_bypass == 1) {
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_OTFOUT_PREV, 0);
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_OTFOUT_CURR, 0);
	} else if (config->geomatch_bypass == 0) {
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_OTFOUT_PREV, 1);
		val = MCFP0_SET_V(val, MCFP0_F_TOP_CHAIN_SEL_OTFOUT_CURR, 1);
	}
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_TOP_CHAIN_SELECT, val);

}

void mcfp0_hw_s_top_size(void __iomem *base, u32 set_id,
	u32 width, u32 height)
{
	/* Set top frame size */
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_TOP_FRAME_WIDTH, width);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_TOP_FRAME_HEIGHT, height);
}

void mcfp0_hw_s_geomatch_config(void __iomem *base, u32 set_id, u32 bayer_order)
{
	u32 val = 0;

	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_PIXEL_ORDER_PREV, bayer_order);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_PIXEL_ORDER_CURR, bayer_order);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_PIXEL_ORDER, val);
}

void mcfp0_hw_s_geomatch_size(void __iomem *base, u32 set_id,
	struct is_isp_config *config,
	u32 width, u32 height,
	u32 stripe_en, u32 stripe_full_width, u32 stripe_start_x)
{
	u32 val = 0;
	u32 sch_img_width, sch_roi_start_x;
	u32 mv_width, mv_height;

	/* Full field of view image for Current */
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_IMG_WIDTH, width);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_IMG_HEIGHT, height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_REF_SIZE, val);

	/* Full field of view image for Previous */
	sch_img_width = stripe_en ? stripe_full_width : width;
	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_SCH_IMG_WIDTH, sch_img_width);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_SCH_IMG_HEIGHT, height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_SCH_SIZE, val);

	/* ROI for Current image */
	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_ROI_START_X, 0);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_ROI_START_Y, 0);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_REF_ROI_START, val);

	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_ROI_SIZE_X, width);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_REF_ROI_SIZE_Y, height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_REF_ROI_SIZE, val);

	/* ROI for Previous image */
	sch_roi_start_x = stripe_en ? stripe_start_x : 0;
	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_SCH_ROI_START_X, sch_roi_start_x);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_SCH_ROI_START_Y, 0);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_SCH_ROI_START, val);

	/* Motion vector width/height */
	mv_width = DIV_ROUND_UP(width, 32);
	mv_height = DIV_ROUND_UP(height, 32);
	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_MV_WIDTH, mv_width);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_MV_HEIGHT, mv_height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_MV_SIZE, val);

	/* Use fixed value */
	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_SENSOR_WIDTH, 0);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_SENSOR_HEIGHT, 0);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_RADIAL_SENSOR_SIZE, val);

	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_START_X, 0);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_START_Y, 0);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_RADIAL_START, val);

	val = 0;
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_STEP_X, 256);
	val = MCFP0_SET_V(val, MCFP0_F_GEOMATCH_RADIAL_STEP_Y, 256);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_GEOMATCH_RADIAL_STEP, val);
}

void mcfp0_hw_s_bgdc_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, bool prev_sbwc_enable, bool enable)
{
	u32 val = 0;
	u32 bgdc_comp_enable = enable ? prev_sbwc_enable : 0;
	u32 img_shift_bit = DMA_INOUT_BIT_WIDTH_13BIT - config->img_bit;
	u32 wgt_shift_bit = DMA_INOUT_BIT_WIDTH_8BIT - config->wgt_bit;

	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BGDC_COMPRESSOR, bgdc_comp_enable);

	/* Set shifter using config */
	if (enable && wgt_shift_bit)
		val = MCFP0_SET_V(val, MCFP0_F_BGDC_RDMA_SHIFTER_WGT_EN, 1);
	else
		val = MCFP0_SET_V(val, MCFP0_F_BGDC_RDMA_SHIFTER_WGT_EN, 0);
	val = MCFP0_SET_V(val, MCFP0_F_BGDC_RDMA_SHIFTER_WGT_LSHIFT, wgt_shift_bit);
	val = MCFP0_SET_V(val, MCFP0_F_BGDC_RDMA_SHIFTER_BAYER_DITHER, 0);
	val = MCFP0_SET_V(val, MCFP0_F_BGDC_RDMA_SHIFTER_BAYER_LSHIFT, img_shift_bit);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BGDC_RDMA_SHIFTER, val);
}

void mcfp0_hw_s_bgdc_size(void __iomem *base, u32 set_id,
	u32 width, u32 height,
	u32 stripe_en, u32 stripe_full_width, u32 stripe_start_x)
{
	u32 scale_shifter_x = 7;
	u32 scale_shifter_y = 7;
	u32 bgdc_img_crop_pre_x;
	u32 temp_width, temp_height;
	u32 bgdc_img_w, bgdc_img_h;
	u32 bgdc_core_w, bgdc_core_h;
	u32 bgdc_scale_x, bgdc_scale_y;
	u32 bgdc_inv_scale_x, bgdc_inv_scale_y;

	/* Input original */
	bgdc_img_w = stripe_en ? stripe_full_width : width;
	bgdc_img_h = height;
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INPUT_ORG_SIZE, MCFP0_F_BGDC_IMG_WIDTH, bgdc_img_w);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INPUT_ORG_SIZE, MCFP0_F_BGDC_IMG_HEIGHT, bgdc_img_h);

	bgdc_core_w = bgdc_img_w / 2;
	bgdc_core_h = bgdc_img_h / 2;
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INPUT_BAYER_SIZE, MCFP0_F_BGDC_CORE_WIDTH, bgdc_core_w);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INPUT_BAYER_SIZE, MCFP0_F_BGDC_CORE_HEIGHT, bgdc_core_h);

	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_OUT_CROP_SIZE, MCFP0_F_BGDC_IMG_ACTIVE_WIDTH, width);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_OUT_CROP_SIZE, MCFP0_F_BGDC_IMG_ACTIVE_HEIGHT, height);

	bgdc_img_crop_pre_x = stripe_en ? stripe_start_x : 0;
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_OUT_CROP_START, MCFP0_F_BGDC_IMG_CROP_PRE_X, bgdc_img_crop_pre_x);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_OUT_CROP_START, MCFP0_F_BGDC_IMG_CROP_PRE_Y, 0);

	temp_width = bgdc_img_w;
	while (temp_width <= TNR_BGDC_MAX_WIDTH) {
		scale_shifter_x--;
		temp_width = temp_width << 1;
		if (scale_shifter_x == 0)
			break;
	}
	temp_height = bgdc_img_h;
	while (temp_height <= TNR_BGDC_MAX_HEIGHT) {
		scale_shifter_y--;
		temp_height = temp_height << 1;
		if (scale_shifter_y == 0)
			break;
	}
	bgdc_scale_x = ((TNR_BGDC_MAX_WIDTH << (8 + scale_shifter_x)) + bgdc_core_w / 2) / bgdc_core_w;
	bgdc_scale_y = ((TNR_BGDC_MAX_HEIGHT << (8 + scale_shifter_y)) + bgdc_core_h / 2) / bgdc_core_h;
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_SCALE, MCFP0_F_BGDC_SCALE_X, bgdc_scale_x);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_SCALE, MCFP0_F_BGDC_SCALE_Y, bgdc_scale_y);

	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_SCALE_SHIFTER, MCFP0_F_BGDC_SCALE_SHIFTER_X, scale_shifter_x);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_SCALE_SHIFTER, MCFP0_F_BGDC_SCALE_SHIFTER_Y, scale_shifter_y);

	bgdc_inv_scale_x = bgdc_core_w;
	bgdc_inv_scale_y = (bgdc_core_h * 8 + 3) / 6;
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INV_SCALE, MCFP0_F_BGDC_INV_SCALE_X, bgdc_inv_scale_x);
	MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BGDC_INV_SCALE, MCFP0_F_BGDC_INV_SCALE_Y, bgdc_inv_scale_y);
}

void mcfp0_hw_s_wbg_tnr_config(void __iomem *base, u32 set_id, u32 bayer_order)
{
	/* Bayer pixel order-0 - GRBG1 - RGGB2 - BGGR3 - GBRG */
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_WBG_TNR_PIXEL_ORDER, bayer_order);
}

void mcfp0_hw_s_bilateral_config(void __iomem *base, u32 set_id, u32 bayer_order)
{
	u32 val = 0;
	u32 reg_val;

	reg_val = MCFP0_GET_R_COREX(base, set_id, MCFP0_R_BILATERAL_BF_CONTROL);
	val = MCFP0_SET_V(val, MCFP0_F_BILATERAL_PIXEL_ORDER, bayer_order);
	reg_val |= val;

	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BILATERAL_BF_CONTROL, reg_val);
}

void mcfp0_hw_s_bilateral_size(void __iomem *base, u32 set_id,
	u32 width, u32 height)
{
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BILATERAL_BF_IMG_WIDTH, width);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BILATERAL_BF_IMG_HEIGHT, height);
}

void mcfp0_hw_s_byrupscale_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, u32 bayer_order, bool enable)
{
	u32 bilateral_filter_en = MCFP0_GET_F_COREX(base, set_id, MCFP0_R_BILATERAL_BF_CONTROL,
									MCFP0_F_BILATERAL_BILATERAL_FILTER_EN);

	/* Currently, Bayer upscaler is not used. This is set by bilateral config */
	if (enable && (bilateral_filter_en || config->tnr_mode == 3)) {
		MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_EN, 1);
		MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_BYPASS, 0);
	} else {
		MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_EN, 0);
		MCFP0_SET_F_COREX(base, set_id, MCFP0_R_BYRUPSCALE_EN, MCFP0_F_BYRUPSCALE_BYPASS, 1);
	}

	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_PIXEL_ORDER, bayer_order);
}

void mcfp0_hw_s_byrupscale_size(void __iomem *base, u32 set_id,
	u32 width, u32 height)
{
	/* To use bilateral, Bayer upscaler should be turn on with in == out */
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_IN_IMG_WIDTH, width);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_IN_IMG_HEIGHT, height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_OUT_IMG_WIDTH, width);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_OUT_IMG_HEIGHT, height);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_HOR_RATIO, 0x100000);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_VER_RATIO, 0x100000);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_HOR_INIT_PHASE_OFFSET, 0);
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_BYRUPSCALE_VER_INIT_PHASE_OFFSET, 0);
}

void mcfp1_hw_s_top_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, bool prev_sbwc_enable, bool enable)
{
	u32 otf_if_en = 0;
	u32 otf_if_en_bayer_out = 0;
	u32 otf_if_en_bayer_tiled_out = 0;
	u32 otf_if_wgt_out = 0;
	u32 img_shift_bit = DMA_INOUT_BIT_WIDTH_13BIT - config->img_bit;
	u32 wgt_shift_bit = DMA_INOUT_BIT_WIDTH_8BIT - config->wgt_bit;
	u32 val;

	if (enable == false) {
		otf_if_en_bayer_out = 0;
		otf_if_en_bayer_tiled_out = 0;
		otf_if_wgt_out = 0;
	} else if (prev_sbwc_enable) {
		otf_if_en_bayer_out = 0;
		otf_if_en_bayer_tiled_out = 1;
		otf_if_wgt_out = 1;
	} else {
		otf_if_en_bayer_out = 1;
		otf_if_en_bayer_tiled_out = 0;
		otf_if_wgt_out = 1;
	}
	/*
	 * 0: OTF disable, 1: OTF IF 0 enable
	 * [0] : Curr (in)
	 * [1] : Prev (in)
	 * [2] : Bayer (out)
	 * [3] : Bayer-tiled (out)
	 * [4] : Weight (out)
	 */
	otf_if_en = (otf_if_wgt_out << 4 | otf_if_en_bayer_tiled_out << 3 | otf_if_en_bayer_out << 2 | 3);
	MCFP1_SET_F_COREX(base, set_id, MCFP1_R_IP_OTF_IF_ENABLE, MCFP1_F_OTF_IF_EN, otf_if_en);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_NFLBC_6LINE, config->top_nflbc_6line);
	MCFP1_SET_F_COREX(base, set_id, MCFP1_R_TOP_OTFOUT_MASK, MCFP1_F_TOP_OTFOUT_MASK_CLEAN,
					config->top_otfout_mask_clean);
	MCFP1_SET_F_COREX(base, set_id, MCFP1_R_TOP_OTFOUT_MASK, MCFP1_F_TOP_OTFOUT_MASK_DIRTY,
					config->top_otfout_mask_dirty);

	MCFP1_SET_F_COREX(base, set_id, MCFP1_R_TOP_CHAIN_SELECT, MCFP1_F_TOP_CHAIN_SEL_WDMA_BAYER,
						prev_sbwc_enable);

	if (enable == false || config->geomatch_bypass)
		MCFP1_SET_F_COREX(base, set_id, MCFP1_R_TOP_CHAIN_SELECT, MCFP1_F_TOP_CHAIN_SEL_OTFIN_PREV, 0);
	else
		MCFP1_SET_F_COREX(base, set_id, MCFP1_R_TOP_CHAIN_SELECT, MCFP1_F_TOP_CHAIN_SEL_OTFIN_PREV, 1);

	/* Set WDMA shifter */
	val = 0;
	val = MCFP1_SET_V(val, MCFP1_F_TOP_WDMA_SHIFTER_WGT_RSHIFT, wgt_shift_bit);
	if (enable == true && wgt_shift_bit)
		val = MCFP1_SET_V(val, MCFP1_F_TOP_WDMA_SHIFTER_WGT_EN, 1);
	else
		val = MCFP1_SET_V(val, MCFP1_F_TOP_WDMA_SHIFTER_WGT_EN, 0);
	val = MCFP1_SET_V(val, MCFP1_F_TOP_WDMA_SHIFTER_BAYER_RSHIFT, img_shift_bit);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_WDMA_SHIFTER, val);
}

void mcfp1_hw_s_top_size(void __iomem *base, u32 set_id,
	u32 width, u32 height)
{
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_FRAME_WIDTH, width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TOP_FRAME_HEIGHT, height);
}

void mcfp1_hw_s_tnr_config(void __iomem *base, u32 set_id, u32 bayer_order)
{
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_PIXEL_ORDER, bayer_order);
}

void mcfp1_hw_s_tnr_size(void __iomem *base, u32 set_id,
	struct is_hw_size_config *config,
	u32 width, u32 height)
{
	const u32 binning_factor_x1_256 = 256;

	u32 dma_crop_x = config->bcrop_width * config->dma_crop_x / config->taasc_width;
	u32 start_x = (config->sensor_crop_x + config->bcrop_x + dma_crop_x) *
		      config->sensor_binning_x / 1000;
	u32 start_y = (config->sensor_crop_y + config->bcrop_y) * config->sensor_binning_y / 1000;
	u32 step_x = (config->sensor_binning_x * config->bcrop_width * binning_factor_x1_256) /
		     config->taasc_width / 1000;
	u32 step_y = config->sensor_binning_y * config->bcrop_height /
		     config->height * binning_factor_x1_256 / 1000;

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_START_X, start_x);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_START_Y, start_y);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_STEP_X, step_x);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_STEP_Y, step_y);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_SENSOR_WIDTH, config->sensor_calibrated_width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_SENSOR_HEIGHT, config->sensor_calibrated_height);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_ROISTARTX, 0);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_ROISTARTY, 0);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_ROISIZEX, width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_TNR_ROISIZEY, height);
}

void mcfp1_hw_s_bcrop_byr_wgt_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config,
	u32 stripe_en, bool enable)
{
	u32 bcrop_bypass;
	u32 wgt_bypass;

	if (!stripe_en || enable == false)
		bcrop_bypass = 1;
	else
		bcrop_bypass = 0;

	if (enable == false || config->tnr_mode == 0 || config->tnr_mode == 3)
		wgt_bypass = 1;
	else
		wgt_bypass = bcrop_bypass;

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_BYPASS, bcrop_bypass);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_BYPASS, wgt_bypass);
}

void mcfp1_hw_s_bcrop_byr_wgt_size(void __iomem *base, u32 set_id,
	u32 stripe_en, u32 stripe_left_margin, u32 stripe_right_margin,
	u32 width, u32 height)
{
	u32 strip_start_x = stripe_left_margin;
	u32 strip_width = width - (stripe_left_margin + stripe_right_margin);

	if (stripe_en) {
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_START_X, strip_start_x);
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_SIZE_X, strip_width);

		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_START_X, strip_start_x / 2);
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_SIZE_X, strip_width / 2);
	} else {
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_START_X, 0);
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_SIZE_X, width);

		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_START_X, 0);
		MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_SIZE_X, width / 2);
	}

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_START_Y, 0);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_SIZE_Y, height);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_START_Y, 0);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_SIZE_Y, height / 2);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_CLEAN_SIZE_X, width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_CLEAN_SIZE_Y, height);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_CLEAN_SIZE_X, width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_WGT_CLEAN_SIZE_Y, height);

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_DIRTY_SIZE_X, width);
	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_BCROP_BYR_DIRTY_SIZE_Y, height);
}

void mcfp0_hw_s_control_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, u32 bayer_order,
	bool prev_sbwc_enable, bool enable)
{
	mcfp0_hw_s_top_config(base, set_id, config, prev_sbwc_enable, enable);
	mcfp0_hw_s_geomatch_config(base, set_id, bayer_order);
	mcfp0_hw_s_bgdc_config(base, set_id, config, prev_sbwc_enable, enable);
	mcfp0_hw_s_wbg_tnr_config(base, set_id, bayer_order);
	mcfp0_hw_s_bilateral_config(base, set_id, bayer_order);
	mcfp0_hw_s_byrupscale_config(base, set_id, config, bayer_order, enable);
}

void mcfp1_hw_s_control_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, u32 bayer_order,
	struct param_stripe_input *stripe_input,
	bool prev_sbwc_enable, bool enable)
{
	u32 stripe_en = (stripe_input->total_count < 2) ? 0 : 1;

	mcfp1_hw_s_top_config(base, set_id, config, prev_sbwc_enable, enable);
	mcfp1_hw_s_tnr_config(base, set_id, bayer_order);
	mcfp1_hw_s_bcrop_byr_wgt_config(base, set_id, config, stripe_en, enable);
}

void mcfp0_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_isp_config *config,
	struct param_stripe_input *stripe_input,
	u32 width, u32 height)
{
	u32 stripe_en = (stripe_input->total_count < 2) ? 0 : 1;
	u32 stripe_full_width = stripe_input->full_width;
	u32 stripe_idx = stripe_input->index;
	u32 stripe_start_x = stripe_en ?
		stripe_input->stripe_roi_start_pos_x[stripe_idx] - stripe_input->left_margin : 0;

	mcfp0_hw_s_top_size(base, set_id, width, height);
	mcfp0_hw_s_geomatch_size(base, set_id, config,
		width, height,
		stripe_en, stripe_full_width, stripe_start_x);
	mcfp0_hw_s_bgdc_size(base, set_id, width, height,
		stripe_en, stripe_full_width, stripe_start_x);
	mcfp0_hw_s_bilateral_size(base, set_id, width, height);
	mcfp0_hw_s_byrupscale_size(base, set_id, width, height);
}

void mcfp1_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_hw_size_config *config,
	struct param_stripe_input *stripe_input)
{
	u32 width = config->width;
	u32 height = config->height;
	u32 stripe_en = (stripe_input->total_count < 2) ? 0 : 1;
	u32 stripe_left_margin = stripe_input->left_margin;
	u32 stripe_right_margin = stripe_input->right_margin;

	mcfp1_hw_s_top_size(base, set_id, width, height);
	mcfp1_hw_s_tnr_size(base, set_id, config, width, height);
	mcfp1_hw_s_bcrop_byr_wgt_size(base, set_id, stripe_en,
		stripe_left_margin, stripe_right_margin,
		width, height);
}

void mcfp0_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers)
{
	u32 prev_fro_en = MCFP0_GET_R_DIRECT(base, MCFP0_R_FRO_MODE_EN);
	u32 fro_en = (num_buffers > 1) ? 1 : 0;

	if (prev_fro_en != fro_en) {
		info_hw("[MCFP] FRO: %d -> %d\n", prev_fro_en, fro_en);
		/* fro core reset */
		MCFP0_SET_R_DIRECT(base, MCFP0_R_FRO_SW_RESET, 1);
	}

	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
			  (num_buffers - 1));
	MCFP0_SET_R_COREX(base, set_id, MCFP0_R_FRO_MODE_EN, fro_en);
}

void mcfp1_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers)
{
	u32 fro_en = (num_buffers > 1) ? 1 : 0;

	MCFP1_SET_R_COREX(base, set_id, MCFP1_R_FRO_MODE_EN, fro_en);
}

void mcfp0_hw_s_crc(void __iomem *base, u32 seed)
{
	MCFP0_SET_F_DIRECT(base, MCFP0_R_GEOMATCH_CURR_STREAM_CRC, MCFP0_F_GEOMATCH_CURR_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_GEOMATCH_PREV_STREAM_CRC, MCFP0_F_GEOMATCH_PREV_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_GEOMATCH_WGT_STREAM_CRC, MCFP0_F_GEOMATCH_WGT_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BGDC_COLOR0_CRC, MCFP0_F_BGDC_COLOR0_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BGDC_COLOR1_CRC, MCFP0_F_BGDC_COLOR1_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BGDC_COLOR2_CRC, MCFP0_F_BGDC_COLOR2_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BGDC_COLOR3_CRC, MCFP0_F_BGDC_COLOR3_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BGDC_CRC_WEIGHT, MCFP0_F_BGDC_WEIGHT_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_WBG_TNR_STREAM_CRC, MCFP0_F_WBG_TNR_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_GAMMA_PRE_TNR_STREAM_CRC, MCFP0_F_GAMMA_PRE_TNR_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BLC_TNR_STREAM_CRC, MCFP0_F_BLC_TNR_CRC_SEED, seed);
	MCFP0_SET_F_DIRECT(base, MCFP0_R_BYRUPSCALE_CRC_SEED, MCFP0_F_BYRUPSCALE_CRC_SEED, seed);
}

void mcfp1_hw_s_crc(void __iomem *base, u32 seed)
{
	MCFP1_SET_F_DIRECT(base, MCFP1_R_TOP_CLEAN_BAYER_STREAM_CRC, MCFP1_F_TOP_CLEAN_BAYER_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_TOP_CLEAN_WGT_STREAM_CRC, MCFP1_F_TOP_CLEAN_WGT_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_TOP_DIRTY_BAYER_STREAM_CRC, MCFP1_F_TOP_DIRTY_BAYER_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_TNR_MIX_CRC, MCFP1_F_TNR_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_BCROP_BYR_STREAM_CRC, MCFP1_F_BCROP_BYR_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_BCROP_WGT_STREAM_CRC, MCFP1_F_BCROP_WGT_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_BCROP_BYR_CLEAN_STREAM_CRC, MCFP1_F_BCROP_BYR_CLEAN_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_BCROP_WGT_CLEAN_STREAM_CRC, MCFP1_F_BCROP_WGT_CLEAN_CRC_SEED, seed);
	MCFP1_SET_F_DIRECT(base, MCFP1_R_BCROP_BYR_DIRTY_STREAM_CRC, MCFP1_F_BCROP_BYR_DIRTY_CRC_SEED, seed);
}

u32 mcfp0_hw_g_reg_cnt(void)
{
	return MCFP0_REG_CNT;
}

u32 mcfp1_hw_g_reg_cnt(void)
{
	return MCFP1_REG_CNT;
}
