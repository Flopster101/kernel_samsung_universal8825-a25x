// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * yuvp HW control APIs
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
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-yuvp-v1_0.h"

unsigned int yuvp_hw_is_occured0(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR1_YUVP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR1_YUVP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR1_YUVP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR1_YUVP_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = INT1_YUVP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

unsigned int yuvp_hw_is_occured1(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_ERR:
		mask = INT2_YUVP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_is_occured1);

int yuvp_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp;

	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_SW_RESET, 0x1);

	while (1) {
		temp = YUVP_GET_R(base, YUVP_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > YUVP_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	return 0;
}

void yuvp_hw_s_init(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_F(base, YUVP_R_CMDQ_QUE_CMD_M, YUVP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);

	YUVP_SET_R(base, YUVP_R_AUTO_IGNORE_PREADY_ENABLE, 1);
	YUVP_SET_F(base, YUVP_R_IP_PROCESSING, YUVP_F_IP_PROCESSING, 1);

	YUVP_SET_R(base, YUVP_R_CMDQ_ENABLE, 1);
}

int yuvp_hw_wait_idle(void __iomem *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = 0;
	int_all = 0;
	idle = YUVP_GET_F(base, YUVP_R_IDLENESS_STATUS, YUVP_F_IDLENESS_STATUS);
	int_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT0);

	info_hw("[YUVP] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = YUVP_GET_F(base, YUVP_R_IDLENESS_STATUS, YUVP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= YUVP_TRY_COUNT) {
			err_hw("[YUVPP] timeout waiting idle - disable fail");
			yuvp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT0);

	info_hw("[YUVP] idle status after disable (total:%d, curr:%d, idle:%d, int1:0x%X)\n",
			idle, int_all);

	return ret;
}

void yuvp_hw_dump(void __iomem *base)
{
	info_hw("[YUVP] SFR DUMP (v1.0)\n");

	is_hw_dump_regs(base, yuvp_regs, YUVP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_dump);

void yuvp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	_yuvp_hw_s_common(base);
	_yuvp_hw_s_coutfifo(base, set_id);
	_yuvp_hw_s_int_mask(base);
	_yuvp_hw_s_secure_id(base, set_id);
	_yuvp_hw_s_fro(base, num_buffers, set_id);
}

void _yuvp_hw_s_coutfifo(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_IP_USE_OTF_PATH,
			YUVP_F_IP_USE_OTF_OUT_FOR_PATH_0, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_IP_USE_OTF_PATH,
			YUVP_F_IP_USE_OTF_OUT_FOR_PATH_1, 0); /* disable path 1 for bring-up */

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_COUTFIFO0_INTERVALS,
			YUVP_F_YUV_COUTFIFO0_INTERVAL_HBLANK, HBLANK_CYCLE);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_COUTFIFO1_INTERVALS,
			YUVP_F_YUV_COUTFIFO1_INTERVAL_HBLANK, HBLANK_CYCLE);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO0_ENABLE, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_COUTFIFO0_CONFIG,
			YUVP_F_YUV_COUTFIFO0_DEBUG_EN, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO1_ENABLE, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_COUTFIFO1_CONFIG,
			YUVP_F_YUV_COUTFIFO1_DEBUG_EN, 1);
}

void yuvp_hw_s_cinfifo(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_INTERVALS,
			YUVP_F_YUV_CINFIFO0_INTERVAL_HBLANK, HBLANK_CYCLE);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO0_ENABLE, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_CONFIG,
			YUVP_F_YUV_CINFIFO0_STALL_BEFORE_FRAME_START_EN, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_CONFIG,
			YUVP_F_YUV_CINFIFO0_DEBUG_EN, 1);
}

void _yuvp_hw_s_common(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_AUTO_IGNORE_INTERRUPT_ENABLE, 1);
}

void _yuvp_hw_s_int_mask(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_INT_REQ_INT0_ENABLE, INT1_YUVP_EN_MASK);
	YUVP_SET_R(base, YUVP_R_INT_REQ_INT1_ENABLE, INT2_YUVP_EN_MASK);

	YUVP_SET_F(base, YUVP_R_CMDQ_QUE_CMD_L, YUVP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xff);
}

void _yuvp_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 * TODO: get secure scenario
	 */
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_SECU_CTRL_SEQID, 0);
}

void _yuvp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);
}

void yuvp_hw_dma_dump(struct is_common_dma *dma)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_print_info, 0);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_dma_dump);

int yuvp_hw_s_rdma_init(struct is_common_dma *dma, struct yuvp_param_set *param_set,
	u32 enable, u32 vhist_grid_num, u32 drc_grid_w, u32 drc_grid_h,
	u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size)
{
	int ret = SET_SUCCESS;
	struct param_dma_input *dma_input;
	struct param_dma_output *dma_output = NULL;
	u32 comp_sbwc_en, comp_64b_align, lossy_byte32num;
	u32 stride_1p, header_stride_1p;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 format, en_votf, bus_info;
	u32 strip_enable;
	bool img_flag = true;

	dbg_hw(4, "[API] %s\n", __func__);

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_UV:
		dma_input = &param_set->dma_input;
		lossy_byte32num = COMP_LOSSYBYTE32NUM_256X1_U10;
		width = dma_input->width;
		height = dma_input->height;
		break;
	case YUVP_RDMA_SEG0:
	case YUVP_RDMA_SEG1:
		dma_input = &param_set->dma_input_seg;
		img_flag = false;
		break;
	case DRCP_RDMA_DRC0:
	case DRCP_RDMA_DRC1:
	case DRCP_RDMA_DRC2:
		dma_input = &param_set->dma_input_drc;
		width = 16 * (drc_grid_w + 3) / 4;
		height = drc_grid_h;
		img_flag = false;
		break;
	case DRCP_RDMA_CLAHE:
		dma_input = &param_set->dma_input_svhist;
		width = 1024;
		height = vhist_grid_num;
		img_flag = false;
		break;
	case DRCP_WDMA_Y:
	case DRCP_WDMA_UV:
		dma_output = &param_set->dma_output_yuv;
		lossy_byte32num = COMP_LOSSYBYTE32NUM_256X1_U10;
		width = dma_output->width;
		height = dma_output->height;
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (dma_output) {
		en_votf = dma_output->v_otf_enable;
		hwformat = dma_output->format;
		sbwc_type = dma_output->sbwc_type;
		memory_bitwidth = dma_output->bitwidth;
		pixelsize = dma_output->msb + 1;
		bus_info = en_votf ? (cache_hint << 4) : 0x00000000;  /* cache hint [6:4] */
	} else {
		en_votf = dma_input->v_otf_enable;
		hwformat = dma_input->format;
		sbwc_type = dma_input->sbwc_type;
		memory_bitwidth = dma_input->bitwidth;
		pixelsize = dma_input->msb + 1;
		bus_info = en_votf ? (cache_hint << 4) : 0x00000000;  /* cache hint [6:4] */
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true);
	if (comp_sbwc_en == 0)
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				width, 16, img_flag);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
							comp_64b_align, lossy_byte32num,
							YUVP_COMP_BLOCK_WIDTH,
							YUVP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, YUVP_COMP_BLOCK_WIDTH, 16) : 0;
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
		*payload_size = ((height + YUVP_COMP_BLOCK_HEIGHT - 1) / YUVP_COMP_BLOCK_HEIGHT) * stride_1p;
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

int yuvp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id)
{
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	dbg_hw(4, "[API] %s\n", __func__);

	name = __getname();
	if (!name) {
		err_hw("[YUVP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case YUVP_RDMA_Y:
		base_reg = base + yuvp_regs[YUVP_R_YUV_RDMAYIN_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_Y");
		break;
	case YUVP_RDMA_UV:
		base_reg = base + yuvp_regs[YUVP_R_YUV_RDMAUVIN_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_UV");
		break;
	case YUVP_RDMA_SEG0:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMASEGYUVNR_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_SEG0");
		break;
	case YUVP_RDMA_SEG1:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMASEGNSMIX_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_SEG1");
		break;
	default:
		err_hw("[YUVP] invalid input_id[%d]", input_id);
		ret = SET_ERROR;
		goto func_exit;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, input_id, name, available_bayer_format_map, 0, 0, 0);

func_exit:
	__putname(name);

	return ret;
}

void yuvp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int yuvp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	ret = SET_SUCCESS;

	dbg_hw(4, "[API] %s (%d 0x%08x 0x%08x)\n", __func__, dma->id, addr[0], addr[1]);

	switch (dma->id) {
	case YUVP_RDMA_Y:
	case DRCP_WDMA_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_UV:
	case DRCP_WDMA_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_SEG0:
	case YUVP_RDMA_SEG1:
	case DRCP_RDMA_DRC0:
	case DRCP_RDMA_DRC1:
	case DRCP_RDMA_DRC2:
	case DRCP_RDMA_CLAHE:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr,
			(dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case YUVP_RDMA_Y:
		case YUVP_RDMA_UV:
		case DRCP_WDMA_Y:
		case DRCP_WDMA_UV:
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

int yuvp_hw_s_corex_update_type(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return 0;
}

void yuvp_hw_s_cmdq(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_CMDQ_ADD_TO_QUEUE_0, 1);
}

void yuvp_hw_s_corex_init(void __iomem *base, bool enable)
{
	dbg_hw(4, "[API] %s\n", __func__);

	info_hw("[YUVP] %s done\n", __func__);
}

void _yuvp_hw_wait_corex_idle(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);
}
KUNIT_EXPORT_SYMBOL(_yuvp_hw_wait_corex_idle);

unsigned int yuvp_hw_g_int_state0(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
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
	src_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT0);

	if (clear)
		YUVP_SET_R(base, YUVP_R_INT_REQ_INT0_CLEAR, src_all);

	src_err = src_all & INT1_YUVP_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}

unsigned int yuvp_hw_g_int_state1(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state)
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
	src_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT1);

	if (clear)
		YUVP_SET_R(base, YUVP_R_INT_REQ_INT1_CLEAR, src_all);

	src_err = src_all & INT2_YUVP_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_int_state1);

unsigned int yuvp_hw_g_int_mask0(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return YUVP_GET_R(base, YUVP_R_INT_REQ_INT0_ENABLE);
}

unsigned int yuvp_hw_g_int_mask1(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return YUVP_GET_R(base, YUVP_R_INT_REQ_INT1_ENABLE);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_int_mask1);

void yuvp_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NFLTR_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVNR_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPEN_BYPASS, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NSMIX_BYPASS, YUVP_F_YUV_NSMIX_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_block_bypass);

int yuvp_hw_s_yuvnr_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	u32 val;
	u32 yuvnr_radial_gain_en;
	u32 total_binning_x, total_binning_y;
	u32 bin_x, bin_y;
	u32 radial_x, radial_y;
	u32 total_crop_x, total_crop_y;

	dbg_hw(4, "[API] %s\n", __func__);

	total_binning_x = sensor_binning_x * frame_width / 1000 / taa_crop_width;
	total_binning_y = sensor_binning_y * dma_height / 1000 / taa_crop_height;

	bin_x = total_binning_x * 1024;
	bin_y = total_binning_y * 1024;

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_BINNING_X, bin_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_BINNING_Y, bin_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVNR_BINNING, val);

	yuvnr_radial_gain_en = YUVP_GET_F(base + GET_COREX_OFFSET(set_id),
		YUVP_R_YUV_YUVNR_RADIAL, YUVP_F_YUV_YUVNR_RADIAL_LUMA_DIV_EN);

	total_crop_x = sensor_crop_x + taa_crop_x
			+ yuvpp_strip_start_pos * frame_width / taa_crop_width;
	total_crop_y = sensor_crop_y + taa_crop_y;

	radial_x = -(sensor_full_width / 2) + (total_crop_x * sensor_binning_x / 1000);
	radial_y = -(sensor_full_height / 2) + (total_crop_y * sensor_binning_y / 1000);

	YUVP_SET_F(base, YUVP_R_YUV_YUVNR_RADIAL_CENTER,
			YUVP_F_YUV_YUVNR_RADIAL_CENTER_X, radial_x);
	YUVP_SET_F(base, YUVP_R_YUV_YUVNR_RADIAL_CENTER,
			YUVP_F_YUV_YUVNR_RADIAL_CENTER_Y, radial_y);

	return 0;
}

int yuvp_hw_s_nsmix_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height)
{
	u32 val;
	u32 total_binning_x, total_binning_y;
	u32 bin_x, bin_y;
	u32 total_crop_x, total_crop_y;

	dbg_hw(4, "[API] %s\n", __func__);

	total_binning_x = sensor_binning_x * frame_width / 1000 / taa_crop_width;
	total_binning_y = sensor_binning_y * dma_height / 1000 / taa_crop_height;

	bin_x = total_binning_x * 256;
	bin_y = total_binning_y * 256;

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_STEP_X, bin_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_STEP_Y, bin_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NSMIX_STEP, val);

	val = 0;
	if (strip_enable == 0) {
		val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_SENSOR_WIDTH, sensor_full_width);
		val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_SENSOR_HEIGHT, sensor_full_height);
	} else {
		val = 0;
		val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_SENSOR_WIDTH, frame_width);
		val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_SENSOR_HEIGHT, dma_height);
	}
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NSMIX_SENSOR, val);

	/* set start crop x and y */
	val = 0;
	total_crop_x = sensor_crop_x + taa_crop_x;
	total_crop_y = sensor_crop_y + taa_crop_y;

	val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_START_CROP_X, total_crop_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_START_CROP_Y, total_crop_y);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NSMIX_START_CROP, val);

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_NSMIX_STRIP_START_POSITION, yuvpp_strip_start_pos);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_NSMIX_STRIP, val);

	return 0;
}

void yuvp_hw_s_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, dma_width, dma_height);

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_CHAIN_IMG_WIDTH, dma_width);
	val = YUVP_SET_V(val, YUVP_F_CHAIN_IMG_HEIGHT, dma_height);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_IMG_SIZE, val);
}

void yuvp_hw_s_input_path(void __iomem *base, u32 set_id, enum yuvp_input_path_type path)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, path);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_IP_USE_OTF_PATH, YUVP_F_IP_USE_OTF_IN_FOR_PATH_0, path);
}

void yuvp_hw_s_mux_dtp(void __iomem *base, u32 set_id, enum yuvp_mux_dtp_type type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_MUX_SELECT, type);
}

void yuvp_hw_s_dtp_pattern(void __iomem *base, u32 set_id, enum yuvp_dtp_pattern pattern)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, pattern);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_BYPASS, 0);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_TEST_PATTERN_MODE, pattern);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_dtp_pattern);

void yuvp_hw_print_chain_debug_cnt(void __iomem *base)
{
	u32 i, col, row;

	dbg_hw(4, "[API] %s\n", __func__);

	for(i = 0; i < YUVP_CHAIN_DEBUG_COUNTER_MAX; i++) {
		YUVP_SET_R(base, YUVP_R_CHAIN_DEBUG_CNT_SEL, yuvp_counter[i].counter);
		row = YUVP_GET_F(base, YUVP_R_CHAIN_DEBUG_CNT_VAL, YUVP_F_CHAIN_DEBUG_ROW_CNT);
		col = YUVP_GET_F(base, YUVP_R_CHAIN_DEBUG_CNT_VAL, YUVP_F_CHAIN_DEBUG_COL_CNT);
		sfrinfo("[CHAIN_DEBUG] counter:[%s], row:[%d], col:[%d]\n",
			yuvp_counter[i].name, row, col);
	}
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_print_chain_debug_cnt);

u32 yuvp_hw_g_reg_cnt(void)
{
	return YUVP_REG_CNT;
}
