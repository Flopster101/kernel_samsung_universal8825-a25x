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
#include "sfr/is-sfr-lme-v2_0.h"

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

#define UTL_ALIGN_UP(a, b)       (DIV_ROUND_UP(a, b) * (b))

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

	dbg_lme(4, "[API][%s] is called!", __func__);

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
	ret = pablo_smc(SMC_CMD_PREAPRE_PD_ONOFF, 0x1, 0x17710204, 0x2); /* D_TZPC_LME */
	if (ret)
		err("[SMC] SMC_CMD_PREPARE_PD_ONOFF fail: %d", ret);

	return ret;
}

void lme_hw_s_init(void __iomem *base)
{
	dbg_lme(4, "[API][%s] is called!", __func__);

	/* LME_SET_R(base, LME_R_AUTO_MASK_PREADY, 0x1); */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_IP_PROCESSING, LME_F_IP_PROCESSING, 0x1);
}

int lme_hw_wait_idle(void __iomem *base, u32 set_id)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	dbg_lme(4, "[API][%s] is called!", __func__);

	idle = LME_GET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);
	int_all = LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1_STATUS);

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

	int_all = LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1_STATUS);

	info_hw("[LME] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			idle, int_all);

	return ret;
}

void lme_hw_dump(void __iomem *base)
{
	info_hw("[LME] SFR DUMP (v2.0)\n");

	is_hw_dump_regs(base + GET_LME_COREX_OFFSET(COREX_DIRECT), lme_regs, LME_REG_CNT);
}

static void lme_hw_s_common(void __iomem *base, u32 set_id)
{
	u32 val = 0;

	dbg_lme(4, "[API][%s] is called!", __func__);

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_IP_POST_FRAME_GAP, LME_F_IP_POST_FRAME_GAP, 0x0);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_IP_CORRUPTED_INTERRUPT_ENABLE, LME_F_IP_CORRUPTED_INTERRUPT_ENABLE, 0x7);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P0P0_WR_SLOT_LEN_0, 1); /* 2 clients */

	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_0, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_1, 1);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_2, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_3, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_4, 0);
	val = LME_SET_V(val, LME_F_DMA_SIPULME2P0P0_SLOT_REG_WR_0_5, 0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P0P0_SLOT_REG_WR_0_0, val);

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P0P0_WR_ADDR_FIFO_DEPTH_0,
		LME_F_DMA_SIPULME2P0P0_WR_ADDR_FIFO_DEPTH_0, 0x10);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMA_SIPULME2P0P0_WR_DATA_FIFO_DEPTH_0,
		LME_F_DMA_SIPULME2P0P0_WR_DATA_FIFO_DEPTH_0, 0x10);
}

static void lme_hw_s_int_mask(void __iomem *base)
{
	dbg_lme(4, "[API][%s] is called!", __func__);

	LME_SET_F(base, LME_R_CONTINT_SIPULME2P0P0_4SETS_LEVEL_PULSE_N_SEL,
		LME_F_CONTINT_SIPULME2P0P0_4SETS_CONTINT_LEVEL_PULSE_N_SEL, INT_LEVEL + 2);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1_ENABLE,
		LME_F_CONTINT_SIPULME2P0P0_4SETS_CONTINT_INT1_ENABLE,
		LME_INT_EN_MASK);
}

static void lme_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	dbg_lme(4, "[API][%s] is called!", __func__);
	/*
	 * Set Paramer Value
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_SECU_CTRL_SEQID, LME_F_SECU_CTRL_SEQID, 0x0);
}

void lme_hw_s_block_crc(void __iomem *base, u32 set_id)
{
	dbg_lme(4, "[API][%s] is called!", __func__);

	/* Nothing to config excep AXI CRC */
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_AXICRC_SIPULME2P0P0_SEED_0, LME_F_AXICRC_SIPULME2P0P0_SEED_0, 0x37);
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_AXICRC_SIPULME2P0P0_SEED_1, LME_F_AXICRC_SIPULME2P0P0_SEED_1, 0x37);
}

void lme_hw_s_trs_disable_cfg(void)
{
	void __iomem *reg;

	dbg_lme(4, "[API][%s] VOTF trs disable all!\n", __func__);

	/* for LME */
	/* c2serv_m16_s16_trs0_enable */
	reg = ioremap(0x17740300, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* c2serv_m16_s16_trs1_enable */
	reg = ioremap(0x1774032c, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* c2serv_m16_s16_c2com_ring_enable */
	reg = ioremap(0x17740010, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* c2serv_m16_s16_c2com_ring_clk_en */
	reg = ioremap(0x1774000C, 0x4);
	writel(0, reg);
	iounmap(reg);
}

void lme_hw_s_core(void __iomem *base, u32 set_id)
{
	dbg_lme(4, "[API][%s] is called!", __func__);

	lme_hw_s_common(base, set_id);
	lme_hw_s_int_mask(base);
	lme_hw_s_secure_id(base, set_id);
	lme_hw_s_block_crc(base, set_id);
	lme_hw_s_trs_disable_cfg();
}

int lme_hw_s_rdma_init(void __iomem *base, struct lme_param_set *param_set, u32 enable,
	u32 id, u32 set_id)
{
	dbg_lme(4, "[API][%s] is called\n", __func__);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_CLIENT_ENABLE, enable);

	if (enable == 0)
		return 0;

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_OUTSTANDING_LIMIT, 0x58);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_TRACK_KEY_BYTE, 0x0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_TRACK_KEY_LINE, 0x0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_TRACK_ENABLE, 0x0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_DATA_FIFO_DEPTH, 0x14C);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_BURST_ALIGNMENT, 1);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_CACHE_IN_GEOM_BURST_LENGTH, 0xF);

	return 0;
}

int lme_hw_s_wdma_init(void __iomem *base, struct lme_param_set *param_set, u32 enable, u32 id, u32 set_id)
{
	u32 mv_width, mv_height, stride, lwidth;
	u32 val = 0;

	dbg_lme(4, "[API][%s] is called\n", __func__);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_CLIENT_ENABLE, enable);
	/* Not support SAD out */
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_SAD_OUT_CLIENT_ENABLE, 0);

	if (enable == 0)
		return 0;

	mv_width = DIV_ROUND_UP(param_set->dma.output_width, 8);
	mv_height = DIV_ROUND_UP(param_set->dma.output_height, 4);
	lwidth = DIV_ROUND_UP(mv_width * DMA_CLIENT_LME_MEMORY_MIT_WIDTH, 8);
	stride = UTL_ALIGN_UP(lwidth, DMA_CLIENT_LME_BYTE_ALIGNMENT);

	/* SAD - Not use */

	/* MV */
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_GEOM_LWIDTH, lwidth);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_GEOM_LINE_COUNT, mv_height);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_GEOM_TOTAL_WIDTH, stride);


	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_FRMT_PACKING, 16);
	val = LME_SET_V(val, LME_F_DMACLIENT_MV_OUT_FRMT_BPAD_SET, 4);
	val = LME_SET_V(val, LME_F_DMACLIENT_MV_OUT_FRMT_BPAD_TYPE, 1);
	val = LME_SET_V(val, LME_F_DMACLIENT_MV_OUT_FRMT_BSHIFT_SET, 4);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_FRMT_MNM, val);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_DMACLIENT_MV_OUT_SELF_HW_FLUSH_ENABLE, 0);

	return 0;
}

int lme_hw_s_rdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;

	dbg_lme(4, "[API][%s] is called!", __func__);

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
	}

	return ret;
}

int lme_hw_s_wdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;

	switch (id) {
	case LME_WDMA_MV_OUT:
		dbg_lme(4, "[API][%s] id: %d, address[0]: 0x%x, offset: 0x%x, 0x%x, 0x%x, 0x%x\n", __func__,
			id, addr[0], GET_LME_COREX_OFFSET(0), GET_LME_COREX_OFFSET(1), GET_LME_COREX_OFFSET(2),
			GET_LME_COREX_OFFSET(3));
		/* TOOD: Need to fix support corex */
		LME_SET_R(base + GET_LME_COREX_OFFSET(set_id),
			LME_R_DMACLIENT_MV_OUT_GEOM_BASE_ADDR_0, (dma_addr_t)(addr[0]));
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
	dbg_lme(4, "[API][%s] is called!\n", __func__);

	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_CTRL_MS_OPERATION_MODE_SELECT, LME_F_CTRL_MS_OPERATION_MODE_SELECT, 0);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_CTRL_MS_ADD_TO_QUEUE, LME_F_CTRL_MS_ADD_TO_QUEUE, 0);
}

void lme_hw_s_corex_init(void __iomem *base, bool enable)
{
	u32 i;
	u32 reset;
	u32 reset_count = 0;

	info_itfc("[DBG] _lme_hw_s_corex_init is called!");

	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER);

		lme_hw_wait_corex_idle(base);

		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_ENABLE, LME_F_COREX_ENABLE, 0x0);

		info_hw("[3AA] %s disable done\n", __func__);
		return;
	}

	/*
	 * Set Fixed Value
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_COREX_ENABLE, LME_F_COREX_ENABLE, 0x1);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_COREX_RESET, LME_F_COREX_RESET, 0x1);

	while (1) {
		reset = LME_GET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_COREX_RESET);
		if (reset == 0)
			break;
		if (reset_count > LME_TRY_COUNT) {
			err_hw("[LME] fail to wait corex reset");
			break;
		}
		reset_count++;
	}

	/*
	 * Type selection
	 * Only type0 will be used.
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_TYPE_WRITE_TRIGGER, LME_F_COREX_TYPE_WRITE_TRIGGER, 0x1);

	/*
	 * write COREX_TYPE_WRITE to 1 means set type size in 0x80(128) range.
	 */
	for (i = 0; i < (23 * 2); i++)
		LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_COREX_TYPE_WRITE, LME_F_COREX_TYPE_WRITE, 0x0);

	/*
	 * config type0
	 * @TAA_R_COREX_UPDATE_TYPE_0: 0: ignore, 1: copy from SRAM, 2: swap
	 * @TAA_R_COREX_UPDATE_MODE_0: 0: HW trigger, 1: SW trigger
	 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_TYPE_0, LME_F_COREX_UPDATE_TYPE_0, COREX_COPY); // Use Copy mode
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_TYPE_1, LME_F_COREX_UPDATE_TYPE_1, COREX_IGNORE); // Do not use TYPE1
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER); // 1st frame uses SW Trigger, others use H/W Trigger
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_UPDATE_MODE_1, LME_F_COREX_UPDATE_MODE_1, SW_TRIGGER); // 1st frame uses SW Trigger, others use H/W Trigger

	/* initialize type0 */
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_COPY_FROM_IP_0, LME_F_COREX_COPY_FROM_IP_0, 0x1);
	LME_SET_F(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
		LME_R_COREX_COPY_FROM_IP_1, LME_F_COREX_COPY_FROM_IP_1, 0x1);

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

	info_itfc("[DBG] %s is called!", __func__);

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
	src_all = LME_GET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT), LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1);

	dbg_lme(4, "[API][%s] src_all: 0x%x\n", __func__, src_all);

	if (clear)
		LME_SET_R(base + GET_LME_COREX_OFFSET(COREX_DIRECT),
			LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1_CLEAR, src_all);

	src_err = src_all & LME_INT_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

unsigned int lme_hw_g_int_mask(void __iomem *base, u32 set_id)
{
	return LME_GET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CONTINT_SIPULME2P0P0_4SETS_INT1_ENABLE);
}

void lme_hw_s_cache(void __iomem *base, u32 set_id, bool enable, u32 prev_width, u32 cur_width)
{
	u32 val = 0;
	u32 mode;
	u32 ignore_prefetch;
	u32 prev_width_jump, cur_width_jump;
	u32 prev_pix_gain, cur_pix_gain;
	u32 prev_pix_offset, cur_pix_offset;

	dbg_lme(4, "[API][%s] is called!", __func__);

	/* Common */
	enable = 1;
	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_LME_BYPASS, !enable);

	mode = 1; /* 0: Fusion, 1: TNR */
	ignore_prefetch = 0; /* use prefetch for performance */

	val = LME_SET_V(val, LME_F_CACHE_8BIT_LME_MODE, mode);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_IGNORE_PREFETCH, ignore_prefetch);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_DATA_REQ_CNT_EN, 1);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PRE_REQ_CNT_EN, 1);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_CACHE_UTILIZATION_EN, 1);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_LME_CONFIG, val);

	/* previous frame */
	prev_width_jump = ALIGN(prev_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_0, prev_width_jump);

	prev_pix_gain = 0x40;
	prev_pix_offset = 0;

	val = 0;
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_GAIN_0, prev_pix_gain);
	val = LME_SET_V(val, LME_F_CACHE_8BIT_PIX_OFFSET_0, prev_pix_offset);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_PIX_CONFIG_0, val);

	/* current frame */
	cur_width_jump = ALIGN(cur_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);

	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_1, cur_width_jump);

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

	dbg_lme(4, "[API][%s] is called - prev(%dx%d), cur(%dx%d)\n", __func__,
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
}

void lme_hw_s_mvct_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	u32 val = 0;
	u32 prefetch_gap;
	u32 prefetch_en;
	const u32 cell_width = 16;

	prefetch_gap = width / cell_width / 3;
	prefetch_en = 1;
	dbg_lme(4, "[API][%s] width: %d, cell_width: %d, prefetch_gap : %d\n",
		__func__, width, cell_width, prefetch_gap);

	val = LME_SET_V(val, LME_F_MVCT_12BIT_LME_PREFETCH_GAP, prefetch_gap);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_LME_PREFETCH_EN, prefetch_en);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_LME_PREFETCH, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_12BIT_IMAGE_WIDTH, width);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_IMAGE_HEIGHT, height);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_IMAGE_DIMENTIONS, val);
}

void lme_hw_s_mvct(void __iomem *base, u32 set_id)
{
	u32 val = 0;
	const u32 sr_x = 128; /* search range: 16 ~ 128 */
	const u32 sr_y = 128;
	const u32 trs_src_init_cond = 36;
	const u32 trs_ref_init_cond = 42;
	const u32 post_frame_gap = 20;

	val = LME_SET_V(val, LME_F_MVCT_12BIT_LME_MODE, 0x1); /* 0: fusion, 1: TNR */
	val = LME_SET_V(val, LME_F_MVCT_12BIT_LME_FIRST_FRAME, 0x0);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_LME_FW_FRAME_ONLY, 0x0);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_MVMIPSMED_MODE, 0x0); /* 0: bypass */
	val = LME_SET_V(val, LME_F_MVCT_12BIT_MVMSPSMED_F_MODE, 0x0); /* 0: bypass */
	val = LME_SET_V(val, LME_F_MVCT_12BIT_MVMSPSMED_S_MODE, 0x0); /* 0: bypass */
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_LME_CONFIG, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_12BIT_USE_AD, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_USE_SAD, 0x0);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_USE_CT, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_USE_ZSAD, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_NOISE_REDUCTION_EN, 0x1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_COST_SEL, 0);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_MVE_CONFIG, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_12BIT_WEIGHT_CT, 1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_WEIGHT_AD, 5);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_WEIGHT_SAD, 1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_WEIGHT_ZSAD, 1);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_NOISE_LEVEL, 3);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_MVE_WEIGHT, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_12BIT_SR_X, sr_x);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_SR_Y, sr_y);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_MV_SR, val);

	val = 0;
	val = LME_SET_V(val, LME_F_MVCT_12BIT_TRS_SRC_INITIAL_COND_VAL, trs_src_init_cond);
	val = LME_SET_V(val, LME_F_MVCT_12BIT_TRS_REF_INITIAL_COND_VAL, trs_ref_init_cond);
	LME_SET_R(base + GET_LME_COREX_OFFSET(set_id), LME_R_MVCT_12BIT_TRS_CONFIG1, val);

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_MVCT_12BIT_POST_FRAME_GAP, LME_F_MVCT_12BIT_POST_FRAME_GAP, post_frame_gap);
}

void lme_hw_s_first_frame(void __iomem *base, u32 set_id, bool first_frame)
{

}

void lme_hw_s_crc(void __iomem *base, u32 seed)
{

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
	dbg_lme(4, "[API][%s] is called!", __func__);

	LME_SET_F(base + GET_LME_COREX_OFFSET(set_id),
		LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_LME_BYPASS, 0x0);
}
