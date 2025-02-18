// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * DRCP HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include <linux/soc/samsung/exynos-soc.h>
#include "is-hw-api-yuvp-v1_0.h"
#include "is-hw-api-drcp-v1_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-drcp-v1_0.h"

#define DRCP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &drcp_regs[R], &drcp_fields[F], val)
#define DRCP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &drcp_regs[R], &drcp_fields[F], val)
#define DRCP_SET_R(base, R, val) \
	is_hw_set_reg(base, &drcp_regs[R], val)
#define DRCP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &drcp_regs[R], val)
#define DRCP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &drcp_fields[F], val)

#define DRCP_GET_F(base, R, F) \
	is_hw_get_field(base, &drcp_regs[R], &drcp_fields[F])
#define DRCP_GET_R(base, R) \
	is_hw_get_reg(base, &drcp_regs[R])
#define DRCP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &drcp_regs[R])
#define DRCP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &drcp_fields[F])

#define DRCP_LUT_REG_CNT	2560
#define DRCP_LUT_NUM		2	/* DRC, Face DRC */

unsigned int drcp_hw_is_occured0(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR1_DRCP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR1_DRCP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR1_DRCP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR1_DRCP_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = INT1_DRCP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

unsigned int drcp_hw_is_occured1(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_ERR:
		mask = INT2_DRCP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(drcp_hw_is_occured1);

unsigned int drcp_hw_g_int_state0(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, src_err;

	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * src_all: per-frame based yuvp IRQ status
	 * src_fro: FRO based yuvp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = DRCP_GET_R(base, DRCP_R_INT_REQ_INT1);

	if (clear)
		DRCP_SET_R(base, DRCP_R_INT_REQ_INT1_CLEAR, src_all);

	src_err = src_all & INT1_DRCP_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

unsigned int drcp_hw_g_int_state1(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, src_err;

	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * src_all: per-frame based yuvp IRQ status
	 * src_fro: FRO based yuvp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = DRCP_GET_R(base, DRCP_R_INT_REQ_INT2);

	if (clear)
		DRCP_SET_R(base, DRCP_R_INT_REQ_INT2_CLEAR, src_all);

	src_err = src_all & INT2_DRCP_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_int_state1);

unsigned int drcp_hw_g_int_mask0(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return DRCP_GET_R(base, DRCP_R_INT_REQ_INT1_ENABLE);
}

unsigned int drcp_hw_g_int_mask1(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return DRCP_GET_R(base, DRCP_R_INT_REQ_INT2_ENABLE);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_int_mask1);

int drcp_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp;

	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base, DRCP_R_SW_RESET, 0x1);

	while (1) {
		temp = DRCP_GET_R(base, DRCP_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > DRCP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	return 0;
}

void drcp_hw_s_init(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_F(base, DRCP_R_CMDQ_QUE_CMD_M, DRCP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);

	DRCP_SET_R(base, DRCP_R_AUTO_IGNORE_PREADY_ENABLE, 1);
	DRCP_SET_F(base, DRCP_R_IP_PROCESSING, DRCP_F_IP_PROCESSING, 1);

	DRCP_SET_R(base, DRCP_R_CMDQ_ENABLE, 1);
}

int drcp_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = DRCP_GET_F(base, DRCP_R_IDLENESS_STATUS, DRCP_F_IDLENESS_STATUS);
	int_all = DRCP_GET_R(base, DRCP_R_INT_REQ_INT1);

	info_hw("[DRCP] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = DRCP_GET_F(base, DRCP_R_IDLENESS_STATUS, DRCP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= DRCP_TRY_COUNT) {
			err_hw("[DRCP] timeout waiting idle - disable fail");
			drcp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = DRCP_GET_R(base, DRCP_R_INT_REQ_INT1);

	info_hw("[DRCP] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			idle, int_all);

	return ret;
}

void drcp_hw_dump(void __iomem *base)
{
	info_hw("[DRCP] SFR DUMP (v1.0)\n");

	is_hw_dump_regs(base, drcp_regs, DRCP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_dump);

void drcp_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_CROP_CTRL, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCDIST_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCTMC_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CONTDET_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_SHARPADDER_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_GAMMARGB_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_GAMMAOETF_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CCM_BYPASS, 1);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_PRC_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_block_bypass);

void drcp_hw_s_clahe_bypass(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_clahe_bypass);

void drcp_hw_s_yuv444to422_coeff(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_F(base, DRCP_R_YUV_YUV444TO422_COEFFS, DRCP_F_YUV_YUV444TO422_MCOEFFS_0_0, 0x0);
	DRCP_SET_F(base, DRCP_R_YUV_YUV444TO422_COEFFS, DRCP_F_YUV_YUV444TO422_MCOEFFS_0_1, 0x20);
	DRCP_SET_F(base, DRCP_R_YUV_YUV444TO422_COEFFS, DRCP_F_YUV_YUV444TO422_MCOEFFS_0_2, 0x40);
	DRCP_SET_F(base, DRCP_R_YUV_YUV444TO422_COEFFS, DRCP_F_YUV_YUV444TO422_MCOEFFS_0_3, 0x20);

	DRCP_SET_F(base, DRCP_R_YUV_YUV444TO422_COEFFS_1, DRCP_F_YUV_YUV444TO422_MCOEFFS_0_4, 0x0);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_yuv444to422_coeff);

void drcp_hw_s_yuvtorgb_coeff(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_0_0, 0x1000);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_0_1, 0x1000);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_0_2, 0x1000);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_1_0, 0x0);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_1_1, 0x3a7f);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_1_2, 0x1c5a);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_2_0, 0x166e);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_2_1, 0x3493);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_COEFF_2_2, 0x0);

	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_OFFSET_0_0, 0x0);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_OFFSET_0_1, 0x1800);
	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_OFFSET_0_2, 0x1800);

	DRCP_SET_R(base, DRCP_R_YUV_YUVTORGB_LSHIFT, 0x1);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_yuvtorgb_coeff);

void drcp_hw_g_hist_grid_num(void __iomem *base, u32 set_id, u32 *grid_x_num, u32 *grid_y_num)
{
	dbg_hw(4, "[API] %s\n", __func__);

	*grid_x_num = DRCP_GET_F(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_GRID_NUM, DRCP_F_RGB_CLAHE_GRID_X_NUM);
	*grid_y_num = DRCP_GET_F(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_GRID_NUM, DRCP_F_RGB_CLAHE_GRID_Y_NUM);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_hist_grid_num);

void drcp_hw_g_drc_grid_size(void __iomem *base, u32 set_id, u32 *grid_size_x, u32 *grid_size_y)
{
	dbg_hw(4, "[API] %s\n", __func__);

	*grid_size_x = DRCP_GET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCDIST_DMA_GRID_SIZE_X);
	*grid_size_y = DRCP_GET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCDIST_DMA_GRID_SIZE_Y);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_drc_grid_size);

unsigned int drcp_hw_is_occured(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR1_DRCP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR1_DRCP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR1_DRCP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR1_DRCP_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = INT1_DRCP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(drcp_hw_is_occured);

void drcp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	_drcp_hw_s_common(base);
	_drcp_hw_s_cinfifo(base, set_id);
	_drcp_hw_s_int_mask(base);
	_drcp_hw_s_secure_id(base, set_id);
	_drcp_hw_s_fro(base, num_buffers, set_id);
}

void drcp_hw_s_coutfifo(void __iomem *base, u32 set_id, u32 enable)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_COUTFIFO_INTERVALS,
			DRCP_F_YUV_COUTFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_COUTFIFO_ENABLE, enable);
	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_COUTFIFO_CONFIG,
			DRCP_F_YUV_COUTFIFO_DEBUG_EN, 1);
}

void _drcp_hw_s_cinfifo(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_IP_USE_OTF_PATH,
			DRCP_F_IP_USE_OTF_IN_FOR_PATH_0, 1);

	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_CINFIFO_INTERVALS,
			DRCP_F_YUV_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_CINFIFO_ENABLE, 1);
	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_CINFIFO_CONFIG,
			DRCP_F_YUV_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_CINFIFO_CONFIG,
			DRCP_F_YUV_CINFIFO_DEBUG_EN, 1);
	DRCP_SET_F(base + GET_COREX_OFFSET(set_id),
			DRCP_R_YUV_CINFIFO_CONFIG,
			DRCP_F_YUV_CINFIFO_AUTO_RECOVERY_EN, 0);
}

void _drcp_hw_s_common(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base, DRCP_R_AUTO_IGNORE_INTERRUPT_ENABLE, 1);
}

void _drcp_hw_s_int_mask(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base, DRCP_R_INT_REQ_INT1_ENABLE, INT1_DRCP_EN_MASK);
	DRCP_SET_F(base, DRCP_R_CMDQ_QUE_CMD_L, DRCP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xff);
}

void _drcp_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 * TODO: get secure scenario
	 */
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_SECU_CTRL_SEQID, 0);
}

void _drcp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);
}

int drcp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	dbg_hw(4, "[API] %s input id : %d\n", __func__, input_id);

	name = __getname();
	if (!name) {
		err_hw("[DRCP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case DRCP_WDMA_Y:
		base_reg = base + drcp_regs[DRCP_R_YUV_WDMAY_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_WDMA_Y");
		break;
	case DRCP_WDMA_UV:
		base_reg = base + drcp_regs[DRCP_R_YUV_WDMAUV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_WDMA_UV");
		break;
	case DRCP_RDMA_DRC0:
		base_reg = base + drcp_regs[DRCP_R_STAT_RDMADRC_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_RDMA_DRC0");
		break;
	case DRCP_RDMA_DRC1:
		base_reg = base + drcp_regs[DRCP_R_STAT_RDMADRC1_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_RDMA_DRC1");
		break;
	case DRCP_RDMA_DRC2:
		base_reg = base + drcp_regs[DRCP_R_STAT_RDMADRC2_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_RDMA_DRC2");
		break;
	case DRCP_RDMA_CLAHE:
		base_reg = base + drcp_regs[DRCP_R_STAT_RDMACLAHE_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "DRCP_RDMA_CLAHE");
		break;
	default:
		err_hw("[DRCP] invalid input_id[%d]", input_id);
		ret = SET_ERROR;
		goto func_exit;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, input_id, name, available_bayer_format_map, 0, 0, 0);

func_exit:
	__putname(name);

	return ret;
}

void drcp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_rdma_corex_id);

int drcp_hw_s_corex_update_type(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return 0;
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_corex_update_type);

void drcp_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	DRCP_SET_R(base, DRCP_R_CMDQ_ADD_TO_QUEUE_0, 1);
}

void drcp_hw_s_corex_init(void __iomem *base, bool enable)
{
	info_hw("[DRCP] %s done\n", __func__);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_s_corex_init);

void _drcp_hw_wait_corex_idle(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

}
KUNIT_EXPORT_SYMBOL(_drcp_hw_wait_corex_idle);

unsigned int drcp_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src, src_all, src_err;

	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * src_all: per-frame based drcp IRQ status
	 * src_fro: FRO based drcp IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = DRCP_GET_R(base, DRCP_R_INT_REQ_INT1);

	if (clear)
		DRCP_SET_R(base, DRCP_R_INT_REQ_INT1_CLEAR, src_all);

	src_err = src_all & INT1_DRCP_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_int_state);

unsigned int drcp_hw_g_int_mask(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return DRCP_GET_R(base, DRCP_R_INT_REQ_INT1_ENABLE);
}
KUNIT_EXPORT_SYMBOL(drcp_hw_g_int_mask);

int drcp_hw_s_drc_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	dbg_hw(4, "[API] %s\n", __func__);

	if (strip_enable)
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCDIST_STRIP_START_POSITION, yuvp_strip_start_pos);
	else
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_DRCDIST_STRIP_START_POSITION, 0);

	return 0;
}

int drcp_hw_s_clahe_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	u32 val;

	dbg_hw(4, "[API] %s\n", __func__);

	val = 0;
	val = DRCP_SET_V(val, DRCP_F_RGB_CLAHE_FRAME_WIDTH, frame_width);
	val = DRCP_SET_V(val, DRCP_F_RGB_CLAHE_FRAME_HEIGHT, dma_height);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_FRAME_SIZE, val);

	if (strip_enable) {
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_STRIP_START_POSITION, yuvp_strip_start_pos);
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_STRIP_WIDTH, dma_width);
	} else {
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_STRIP_START_POSITION, 0);
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_RGB_CLAHE_STRIP_WIDTH, 0);
	}

	return 0;
}

int drcp_hw_s_sharpadder_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	dbg_hw(4, "[API] %s\n", __func__);

	if (strip_enable)
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_SHARPADDER_STRIP_PARAMS, yuvp_strip_start_pos);
	else
		DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_YUV_SHARPADDER_STRIP_PARAMS, 0);

	return 0;
}

void drcp_hw_s_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, dma_width, dma_height);

	val = 0;
	val = DRCP_SET_V(val, DRCP_F_CHAIN_IMG_WIDTH, dma_width);
	val = DRCP_SET_V(val, DRCP_F_CHAIN_IMG_HEIGHT, dma_height);
	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_CHAIN_IMG_SIZE, val);
}

void drcp_hw_s_output_path(void __iomem *base, u32 set_id, int path)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, path);

	DRCP_SET_F(base + GET_COREX_OFFSET(set_id), DRCP_R_IP_USE_OTF_PATH, DRCP_F_IP_USE_OTF_OUT_FOR_PATH_0, path);
}

void drcp_hw_s_demux_dither(void __iomem *base, u32 set_id, enum drcp_demux_dither_type type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	DRCP_SET_R(base + GET_COREX_OFFSET(set_id), DRCP_R_CHAIN_DEMUX_ENABLE, type);
}

void drcp_hw_print_chain_debug_cnt(void __iomem *base)
{
	u32 i, col, row;

	dbg_hw(4, "[API] %s\n", __func__);

	for(i = 0; i < DRCP_CHAIN_DEBUG_COUNTER_MAX; i++) {
		DRCP_SET_R(base, DRCP_R_CHAIN_DEBUG_CNT_SEL, drcp_counter[i].counter);
		row = DRCP_GET_F(base, DRCP_R_CHAIN_DEBUG_CNT_VAL, DRCP_F_CHAIN_DEBUG_ROW_CNT);
		col = DRCP_GET_F(base, DRCP_R_CHAIN_DEBUG_CNT_VAL, DRCP_F_CHAIN_DEBUG_COL_CNT);
		sfrinfo("[CHAIN_DEBUG] counter:[%s], row:[%d], col:[%d]\n",
			drcp_counter[i].name, row, col);
	}
}
KUNIT_EXPORT_SYMBOL(drcp_hw_print_chain_debug_cnt);

u32 drcp_hw_g_reg_cnt(void)
{
	return DRCP_REG_CNT + (DRCP_LUT_REG_CNT * DRCP_LUT_NUM);
}
