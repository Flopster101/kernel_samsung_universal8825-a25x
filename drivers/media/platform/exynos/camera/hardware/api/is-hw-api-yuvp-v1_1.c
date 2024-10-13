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
#include "is-hw-api-yuvp-v1_1.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-yuvp-v1_1.h"

#define YUVP_LUT_REG_CNT	2560
#define YUVP_LUT_NUM		2	/* DRC, Face DRC */

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

	YUVP_SET_R(base, YUVP_R_DEBUG_CLOCK_ENABLE, 1);
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
	info_hw("[YUVP] SFR DUMP (v1.1)\n");

	is_hw_dump_regs(base, yuvp_regs, YUVP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_dump);

void yuvp_hw_s_coutfifo(void __iomem *base, u32 set_id)
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
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO1_ENABLE, 0);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_COUTFIFO1_CONFIG,
			YUVP_F_YUV_COUTFIFO1_DEBUG_EN, 0);
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

void yuvp_hw_s_strgen(void __iomem *base, u32 set_id)
{
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_CONFIG, YUVP_F_YUV_CINFIFO0_STRGEN_MODE_EN, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_CONFIG, YUVP_F_YUV_CINFIFO0_STRGEN_MODE_DATA_TYPE, 0);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id),
			YUVP_R_YUV_CINFIFO0_CONFIG, YUVP_F_YUV_CINFIFO0_STRGEN_MODE_DATA, 128);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_IP_USE_CINFIFO_NEW_FRAME_IN, 0);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_strgen);

static void _yuvp_hw_s_common(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_AUTO_IGNORE_INTERRUPT_ENABLE, 1);
}

static void _yuvp_hw_s_int_mask(void __iomem *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_INT_REQ_INT0_ENABLE, INT1_YUVP_EN_MASK);
	YUVP_SET_R(base, YUVP_R_INT_REQ_INT1_ENABLE, INT2_YUVP_EN_MASK);

	YUVP_SET_F(base, YUVP_R_CMDQ_QUE_CMD_L, YUVP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xff);
}

static void _yuvp_hw_s_secure_id(void __iomem *base, u32 set_id)
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

static void _yuvp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);
}

void yuvp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	_yuvp_hw_s_common(base);
	_yuvp_hw_s_int_mask(base);
	_yuvp_hw_s_secure_id(base, set_id);
	_yuvp_hw_s_fro(base, num_buffers, set_id);
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
	case YUVP_RDMA_SEG:
		dma_input = &param_set->dma_input_seg;
		width = dma_input->width;
		height = dma_input->height;
		img_flag = false;
		break;
	case YUVP_RDMA_DRC0:
	case YUVP_RDMA_DRC1:
		dma_input = &param_set->dma_input_drc;
		width = drc_grid_w * 4;
		height = drc_grid_h;
		img_flag = false;
		break;
	case YUVP_RDMA_CLAHE:
		dma_input = &param_set->dma_input_svhist;
		width = 1024;
		height = vhist_grid_num;
		img_flag = false;
		break;
	case YUVP_WDMA_Y:
	case YUVP_WDMA_UV:
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
	case YUVP_RDMA_SEG:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMASEG_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_SEG0");
		break;
	case YUVP_WDMA_Y:
		base_reg = base + yuvp_regs[YUVP_R_YUV_WDMAY_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_WDMA_Y");
		break;
	case YUVP_WDMA_UV:
		base_reg = base + yuvp_regs[YUVP_R_YUV_WDMAUV_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_WDMA_UV");
		break;
	case YUVP_RDMA_DRC0:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMADRC_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_DRC0");
		break;
	case YUVP_RDMA_DRC1:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMADRC1_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_DRC1");
		break;
	case YUVP_RDMA_CLAHE:
		base_reg = base + yuvp_regs[YUVP_R_STAT_RDMACLAHE_EN].sfr_offset;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_CLAHE");
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
	case YUVP_WDMA_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_UV:
	case YUVP_WDMA_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_SEG:
	case YUVP_RDMA_DRC0:
	case YUVP_RDMA_DRC1:
	case YUVP_RDMA_CLAHE:
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
		case YUVP_WDMA_Y:
		case YUVP_WDMA_UV:
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
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_int_mask0);

void yuvp_hw_s_int_mask0(void __iomem *base, u32 mask)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_INT_REQ_INT0_ENABLE, mask);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_int_mask0);

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
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVNR_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DITHER_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_INVCCM33_CONFIG, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DEGAMMARGB_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_GAMMARGB_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_GAMMAOETF_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CCM_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_PRC_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCDIST_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCTMC_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_block_bypass);

int yuvp_hw_s_yuvnr_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg,
	u32 biquad_scale_shift_adder)
{
	u32 val;
	u32 sensor_center_x, sensor_center_y;
	int binning_x, binning_y, radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_hw(4, "[API] %s\n", __func__);
	dbg_hw(4, "[API] sensor_binning_x %d, sensor_binning_y %d\n",
		radial_cfg->sensor_binning_x, radial_cfg->sensor_binning_y);
	dbg_hw(4, "[API] sensor_full_width %d, sensor_full_height %d\n",
		radial_cfg->sensor_full_width, radial_cfg->sensor_full_height);
	dbg_hw(4, "[API] sensor_crop_x %d, sensor_crop_y %d\n",
		radial_cfg->sensor_crop_x, radial_cfg->sensor_crop_y);
	dbg_hw(4, "[API] taa_crop_width %d, taa_crop_height %d\n",
		radial_cfg->taa_crop_width, radial_cfg->taa_crop_height);
	dbg_hw(4, "[API] yuvpp_strip_start_pos %d, taa_crop_x %d, taa_crop_y %d\n",
		yuvpp_strip_start_pos, radial_cfg->taa_crop_x, radial_cfg->taa_crop_y);
	dbg_hw(4, "[API] frame_width %d, dma_width %d, dma_height %d\n",
		frame_width, dma_width, dma_height);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x * 1024ULL *
			radial_cfg->taa_crop_width / frame_width / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y * 1024ULL *
			radial_cfg->taa_crop_height / dma_height / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
			(radial_cfg->bns_binning_x * (radial_cfg->taa_crop_x + yuvpp_strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
			(radial_cfg->bns_binning_y * radial_cfg->taa_crop_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_hw(4, "[API] binning_x %d, binning_y %d\n", binning_x, binning_y);
	dbg_hw(4, "[API] radial_center_x %d, radial_center_y %d\n",
		radial_center_x, radial_center_y);
	dbg_hw(4, "[API] biquad_scale_shift_adder %d\n", biquad_scale_shift_adder);

	if (biquad_scale_shift_adder >= 8) {
		if (biquad_scale_shift_adder < 10) {
			binning_x = shift_right_round(binning_x, 1);
			binning_y = shift_right_round(binning_y, 1);
			radial_center_x = shift_right_round(radial_center_x, 1);
			radial_center_y = shift_right_round(radial_center_y, 1);
			biquad_scale_shift_adder -= 2;
		} else {
			binning_x = shift_right_round(binning_x, 2);
			binning_y = shift_right_round(binning_y, 2);
			radial_center_x = shift_right_round(radial_center_x, 2);
			radial_center_y = shift_right_round(radial_center_y, 2);
			biquad_scale_shift_adder -= 4;
		}
	}

	dbg_hw(4, "[API] binning_x %d, binning_y %d\n", binning_x, binning_y);
	dbg_hw(4, "[API] radial_center_x %d, radial_center_y %d\n",
		radial_center_x, radial_center_y);
	dbg_hw(4, "[API] biquad_scale_shift_adder %d\n", biquad_scale_shift_adder);

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_BINNING_X, binning_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_BINNING_Y, binning_y);
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_BIQUAD_SCALE_SHIFT_ADDER,
		biquad_scale_shift_adder);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVNR_BINNING, val);
	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_RADIAL_CENTER_X, radial_center_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_YUVNR_RADIAL_CENTER_Y, radial_center_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVNR_RADIAL_CENTER, val);

	return 0;

}
int yuvp_hw_s_drc_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg)
{
	dbg_hw(4, "[API] %s\n", __func__);

	if (strip_enable) {
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCDIST_STRIP_START_POSITION,
			yuvp_strip_start_pos);
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCTMC_STRIP_START_POSITION,
			yuvp_strip_start_pos);
	} else {
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCDIST_STRIP_START_POSITION, 0);
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCTMC_STRIP_START_POSITION, 0);
	}

	return 0;
}

int yuvp_hw_s_clahe_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg)
{
	u32 val;

	dbg_hw(4, "[API] %s\n", __func__);

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_RGB_CLAHE_FRAME_WIDTH, frame_width);
	val = YUVP_SET_V(val, YUVP_F_RGB_CLAHE_FRAME_HEIGHT, dma_height);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_FRAME_SIZE, val);

	if (strip_enable) {
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_STRIP_START_POSITION, yuvp_strip_start_pos);
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_STRIP_WIDTH, dma_width);
	} else {
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_STRIP_START_POSITION, 0);
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_STRIP_WIDTH, 0);
	}

	return 0;
}

int yuvp_hw_s_sharpen_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg)
{
	u32 val;
	u32 offset_x, offset_y, step_x, step_y, start_crop_x, start_crop_y;

	dbg_hw(4, "[API] %s\n", __func__);

	step_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x * 256ULL *
			radial_cfg->taa_crop_width / frame_width / 1000 / 1000;
	step_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y * 256ULL *
			radial_cfg->taa_crop_height / dma_height / 1000 / 1000;

	offset_x = radial_cfg->sensor_crop_x +
			(radial_cfg->bns_binning_x * radial_cfg->taa_crop_x / 1000);
	offset_y = radial_cfg->sensor_crop_y +
			(radial_cfg->bns_binning_y * radial_cfg->taa_crop_y / 1000);

	start_crop_x = radial_cfg->sensor_binning_x * offset_x / 1000;
	start_crop_y = radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_hw(4, "[API] start_crop_x %d, start_crop_y %d\n", start_crop_x, start_crop_y);
	dbg_hw(4, "[API] step_x %d, step_y %d\n", step_x, step_y);

	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_SENSOR_WIDTH,
		radial_cfg->sensor_full_width);
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_SENSOR_HEIGHT,
		radial_cfg->sensor_full_height);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_SENSOR, val);
	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_START_CROP_X, start_crop_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_START_CROP_Y, start_crop_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_START_CROP, val);
	val = 0;
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_STEP_X, step_x);
	val = YUVP_SET_V(val, YUVP_F_YUV_SHARPENHANCER_STEP_Y, step_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_STEP, val);

	if (strip_enable)
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_STRIP, yuvpp_strip_start_pos);
	else
		YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_STRIP, 0);

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

void yuvp_hw_s_output_path(void __iomem *base, u32 set_id, int path)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, path);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_IP_USE_OTF_PATH, YUVP_F_IP_USE_OTF_OUT_FOR_PATH_0, path);
}

void yuvp_hw_s_demux_enable(void __iomem *base, u32 set_id, enum yuvp_chain_demux_type type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_DEMUX_ENABLE, type);
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
	return YUVP_REG_CNT + (YUVP_LUT_REG_CNT * YUVP_LUT_NUM);
}

void yuvp_hw_s_crc(void __iomem *base, u32 seed)
{
	YUVP_SET_F(base, YUVP_R_YUV_CINFIFO0_STREAM_CRC,
			YUVP_F_YUV_CINFIFO0_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_DTP_STREAM_CRC,
			YUVP_F_YUV_DTP_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUVNR_STREAM_CRC,
			YUVP_F_YUV_YUVNR_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUV422TO444_STREAM_CRC,
			YUVP_F_YUV_YUV422TO444_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUVTORGB_STREAM_CRC,
			YUVP_F_YUV_YUVTORGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_RGBTOYUV_STREAM_CRC,
			YUVP_F_RGB_RGBTOYUV_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUV444TO422_STREAM_CRC,
			YUVP_F_YUV_YUV444TO422_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_DITHER_STREAM_CRC,
			YUVP_F_YUV_DITHER_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_CLAHE_STREAM_CRC,
			YUVP_F_RGB_CLAHE_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_INVCCM33_STREAM_CRC,
			YUVP_F_RGB_INVCCM33_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DEGAMMARGB_STREAM_CRC,
			YUVP_F_RGB_DEGAMMARGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_GAMMARGB_STREAM_CRC,
			YUVP_F_RGB_GAMMARGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_GAMMAOETF_STREAM_CRC,
			YUVP_F_YUV_GAMMAOETF_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_CCM_STREAM_CRC,
			YUVP_F_RGB_CCM_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_PRC_STREAM_CRC,
			YUVP_F_RGB_PRC_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_SHARPEN_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_SHARPEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_HFMIXER_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_HFMIXER_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_NOISEGEN_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_NOISEGEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_NOISEMIXER_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_NOISEMIXER_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_CONTDET_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_CONTDET_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_STREAM_CRC,
			YUVP_F_YUV_SHARPENHANCER_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETA_LUT_CRC,
			YUVP_F_RGB_DRCTMC_SETA_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETB_LUT_CRC,
			YUVP_F_RGB_DRCTMC_SETB_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETA_FACE_LUT_CRC,
			YUVP_F_RGB_DRCTMC_SETA_FACE_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETB_FACE_LUT_CRC,
			YUVP_F_RGB_DRCTMC_SETB_FACE_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_STREAM_CRC,
			YUVP_F_RGB_DRCTMC_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCDIST_STREAM_CRC,
			YUVP_F_RGB_DRCDIST_CRC_SEED, seed);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_crc);
