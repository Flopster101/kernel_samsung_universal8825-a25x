// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-common.h"
#include "is-hw-api-dns.h"
#include "sfr/is-sfr-dns-v1_20.h"

/* IP register direct read/write */
#define DNS_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &dns_regs[R], val)
#define DNS_SET_R(base, set, R, val) \
	is_hw_set_reg(base + GET_COREX_OFFSET(set), &dns_regs[R], val)
#define DNS_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base, &dns_regs[R])
#define DNS_GET_R(base, set, R) \
	is_hw_get_reg(base + GET_COREX_OFFSET(set), &dns_regs[R])

#define DNS_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &dns_regs[R], &dns_fields[F], val)
#define DNS_SET_F(base, set, R, F, val) \
	is_hw_set_field(base + GET_COREX_OFFSET(set), &dns_regs[R], &dns_fields[F], val)
#define DNS_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &dns_regs[R], &dns_fields[F])
#define DNS_GET_F(base, set, R, F) \
	is_hw_get_field(base, GET_COREX_OFFSET(set), &dns_regs[R], &dns_fields[F])

#define DNS_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &dns_fields[F], val)
#define DNS_GET_V(val, F) \
	is_hw_get_field_value(val, &dns_fields[F])

#define DNS_TRY_COUNT			(10000)

/* Guided value */
#define DNS_MIN_HBLNK			(51)
#define DNS_T1_INTERVAL			(32)

/* Constraints */
#define DNS_MAX_WIDTH			(2880)

u32 __dns_hw_is_occurred_int1(u32 status, enum dns_event_type type)
{
	u32 mask;

	switch (type) {
	case DNS_EVENT_FRAME_START:
		mask = 1 << INTR1_DNS_FRAME_START;
		break;
	case DNS_EVENT_FRAME_END:
		mask = 1 << INTR1_DNS_FRAME_END_INTERRUPT;
		break;
	case DNS_EVENT_ERR:
		mask = INT1_ERR_MASK;
		break;
	default:
		return 0;
	}
	return status & mask;
}

u32 __dns_hw_is_occurred_int2(unsigned int status, enum dns_event_type type)
{
	u32 mask;

	switch (type) {
	case DNS_EVENT_ERR:
		mask = INT2_ERR_MASK;
		break;
	default:
		return 0;
	}
	return status & mask;
}

int dns_hw_s_reset(void __iomem *base)
{
	u32 try_cnt = 0;

	/* 2: Core + Configuration register reset for FRO controller */
	DNS_SET_R_DIRECT(base, DNS_R_FRO_SW_RESET, 2);

	DNS_SET_R_DIRECT(base, DNS_R_SW_RESET, 0x1);

	while (DNS_GET_R_DIRECT(base, DNS_R_SW_RESET)) {
		udelay(3); /* 3us * 10000 = 30ms */

		if (++try_cnt >= DNS_TRY_COUNT) {
			err_hw("[DNS] Failed to sw reset");
			return -EBUSY;
		}
	}

	return 0;
}

int dns_hw_s_control_init(void __iomem *base, u32 set_id)
{
	DNS_SET_R_DIRECT(base, DNS_R_IP_PROCESSING, 1);

	DNS_SET_R(base, set_id, DNS_R_IP_POST_FRAME_GAP, 0);

	/*
	 * 0- CINFIFO end interrupt will be triggered by last data
	 *  1- CINFIFO end interrupt will be triggered by VVALID fall
	 */
	DNS_SET_R(base, set_id, DNS_R_IP_CINFIFO_END_ON_VSYNC_FALL, 0);

	/*
	 * To make corrupt interrupt. It is masked following interrupt.
	 * [13] main_cout_columns_error
	 * [12] main_cout_lines_error
	 * [11] SBWC_ERR
	 * [4] Frame_start_before_frame_end
	 * [3] cinfifo_overflow_error
	 * [2] cinfifo_column_error
	 * [1] cinfifo_line_error
	 * [0] cinfifo_total_size_error
	 */
	DNS_SET_R(base, set_id, DNS_R_IP_CORRUPTED_INTERRUPT_ENABLE, 0xFFFF);

	/*
	 * ROL enable interrupts-
	 * [13] main_cout_columns_error
	 * [12] main_cout_lines_error
	 * [11] SBWC_ERR
	 * [4] Frame_start_before_frame_end
	 * [3] cinfifo_overflow_error
	 * [2] cinfifo_column_error
	 * [1] cinfifo_line_error
	 * [0] cinfifo_total_size_error
	 */
	DNS_SET_R(base, set_id, DNS_R_IP_ROL_SELECT, 0xFFFF);

	/*
	 * 0-  none- no ROL operation.
	 * 1-  single mode- when one of the selected interrupts occurs for the first time, the state will be captured.
	 * 2-  continuous mode- every time one of the selected interrupts occurs, the state will be re-captured.
	 */
	DNS_SET_R(base, set_id, DNS_R_IP_ROL_MODE, 1);

	DNS_SET_R_DIRECT(base, DNS_R_IP_HBLANK, DNS_MIN_HBLNK);

	return 0;
}

int dns_hw_s_control_config(void __iomem *base, u32 set_id, struct dns_control_config *config)
{
	u32 input_sel;
	u32 cin_frame_start;
	u32 val;

	switch (config->input_type) {
	case DNS_INPUT_OTF:
		cin_frame_start = 1;
		input_sel = 0;
		break;
	case DNS_INPUT_DMA:
		cin_frame_start = 0;
		input_sel = 2;
		break;
	case DNS_INPUT_STRGEN:
		cin_frame_start = 0;
		input_sel = 1;
		break;
	default:
		err_hw("[DNS] not supported input type (%d) ", config->input_type);
		return -EINVAL;
	};

	DNS_SET_R(base, set_id, DNS_R_IP_USE_CINFIFO_FRAME_START_IN, cin_frame_start);

	val = 0;
	val = DNS_SET_V(val, DNS_F_IP_CHAIN_INPUT_SELECT, input_sel);
	val = DNS_SET_V(val, DNS_F_IP_TOP_OTF_IF0_SELECT, config->rdma_dirty_bayer_en);
	DNS_SET_R(base, set_id, DNS_R_IP_CHAIN_INPUT_SELECT, val);

	val = 0;
	val = DNS_SET_V(val, DNS_F_OTF_IF_0_EN, config->dirty_bayer_sel_dns);
	val = DNS_SET_V(val, DNS_F_OTF_IF_1_EN, config->ww_lc_en);
	val = DNS_SET_V(val, DNS_F_OTF_IF_2_EN, 1);
	val = DNS_SET_V(val, DNS_F_OTF_IF_3_EN, 1);
	val = DNS_SET_V(val, DNS_F_OTF_IF_4_EN, 1);
	DNS_SET_R(base, set_id, DNS_R_IP_OTF_IF_ENABLE, val);

	DNS_SET_R(base, set_id, DNS_R_IP_TNR_INPUT_EN, config->hqdns_tnr_input_en);
	DNS_SET_F(base, set_id, DNS_R_HQDNS_CONTROLS, DNS_F_HQDNS_TNR_INPUT_EN, config->hqdns_tnr_input_en);

	if (!config->hqdns_tnr_input_en && !config->dirty_bayer_sel_dns)
		err_hw("[DNS] no diry bayer signal to nfltr");

	return 0;
}

void dns_hw_s_control_debug(void __iomem *base, u32 set_id)
{
	DNS_SET_R(base, set_id, DNS_R_IP_OTF_DEBUG_EN, 1);
	DNS_SET_R(base, set_id, DNS_R_IP_OTF_DEBUG_SEL, 2);
}

int dns_hw_s_enable(void __iomem *base, bool one_shot, bool fro_en)
{
	u32 try_cnt = 0;

	if (!fro_en) {
		if (one_shot) {
			DNS_SET_R_DIRECT(base, DNS_R_ONE_SHOT_ENABLE, 1);
		} else {
			DNS_SET_R_DIRECT(base, DNS_R_GLOBAL_ENABLE, 1);
			while (!DNS_GET_R_DIRECT(base, DNS_R_GLOBAL_ENABLE)) {
				if (++try_cnt >= DNS_TRY_COUNT) {
					err_hw("[DNS] Failed to enable dns");
					return -EBUSY;
				}
			}
		}
	} else {
		if (one_shot) {
			DNS_SET_R_DIRECT(base, DNS_R_FRO_ONE_SHOT_ENABLE, 1);
		} else {
			DNS_SET_R_DIRECT(base, DNS_R_FRO_GLOBAL_ENABLE, 1);
			while (!DNS_GET_R_DIRECT(base, DNS_R_FRO_GLOBAL_ENABLE)) {
				if (++try_cnt >= DNS_TRY_COUNT) {
					err_hw("[DNS] Failed to enable dns");
					return -EBUSY;
				}
			}
		}
	}
	return 0;
}

int dns_hw_s_disable(void __iomem *base)
{
	DNS_SET_R_DIRECT(base, DNS_R_FRO_GLOBAL_ENABLE, 0);
	DNS_SET_R_DIRECT(base, DNS_R_GLOBAL_ENABLE, 0);
	return 0;
}

int dns_hw_s_int_init(void __iomem *base)
{
	/*
	 * Output format selector. 1=level, 0=pulse.
	 */
	DNS_SET_R_DIRECT(base, DNS_R_CONTINT_ITP_LEVEL_PULSE_N_SEL, 3);

	DNS_SET_R_DIRECT(base, DNS_R_CONTINT_ITP_INT1_CLEAR, (1 << INTR1_DNS_MAX) - 1);
	DNS_SET_R_DIRECT(base, DNS_R_FRO_INT0_CLEAR, (1 << INTR1_DNS_MAX) - 1);
	DNS_SET_R_DIRECT(base, DNS_R_CONTINT_ITP_INT1_ENABLE, INT1_EN_MASK);
	DNS_SET_R_DIRECT(base, DNS_R_CONTINT_ITP_INT2_CLEAR, (1 << INTR2_DNS_MAX) - 1);
	DNS_SET_R_DIRECT(base, DNS_R_FRO_INT1_CLEAR, (1 << INTR2_DNS_MAX) - 1);
	DNS_SET_R_DIRECT(base, DNS_R_CONTINT_ITP_INT2_ENABLE, INT2_EN_MASK);

	return 0;
}

u32 dns_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en)
{
	enum is_dns_reg_name int_req, int_req_clear;
	enum is_dns_reg_name fro_int_req, fro_int_req_clear;
	u32 int_status, fro_int_status;

	switch (irq_id) {
	case 0:
		int_req = DNS_R_CONTINT_ITP_INT1_STATUS;
		int_req_clear = DNS_R_CONTINT_ITP_INT1_CLEAR;
		fro_int_req = DNS_R_FRO_INT0;
		fro_int_req_clear = DNS_R_FRO_INT0_CLEAR;
		break;
	case 1:
		int_req = DNS_R_CONTINT_ITP_INT2_STATUS;
		int_req_clear = DNS_R_CONTINT_ITP_INT2_CLEAR;
		fro_int_req = DNS_R_FRO_INT1;
		fro_int_req_clear = DNS_R_FRO_INT1_CLEAR;
		break;
	default:
		err_hw("[DNS] invalid irq_id (%d)", irq_id);
		return 0;
	}

	int_status = DNS_GET_R_DIRECT(base, int_req);
	fro_int_status = DNS_GET_R_DIRECT(base, fro_int_req);

	if (clear) {
		DNS_SET_R_DIRECT(base, int_req_clear, int_status);
		DNS_SET_R_DIRECT(base, fro_int_req_clear, fro_int_status);
	}

	return fro_en ? fro_int_status : int_status;
}

u32 dns_hw_is_occurred(u32 status, u32 irq_id, enum dns_event_type type)
{
	switch (irq_id) {
	case 0:
		return __dns_hw_is_occurred_int1(status, type);
	case 1:
		return __dns_hw_is_occurred_int2(status, type);
	default:
		err_hw("[DNS] invalid irq_id (%d)", irq_id);
		return 0;
	}
}

int dns_hw_s_cinfifo_init(void __iomem *base, u32 set_id)
{
	u32 val;

	val = 0;
	/*
	 * 0x0 - t2 interval will last at least until frame_start signal will arrive
	 * 0x1 - t2 interval wait for frame_start is disabled
	 */
	val = DNS_SET_V(val, DNS_F_CINFIFO_INPUT_T2_DISABLE_WAIT_FOR_FS, 0);
	/*
	 * 0x0 - t2 counter will not reset at frame_start
	 * 0x1 - if  cinfifo_t2_disable_wait_for_fs=0 t2 counter resets at frame_start
	 */
	val = DNS_SET_V(val, DNS_F_CINFIFO_INPUT_T2_RESET_COUNTER_AT_FS, 0);
	DNS_SET_R_DIRECT(base, DNS_R_CINFIFO_INPUT_T2_WAIT_FOR_FS, val);

	DNS_SET_R(base, set_id, DNS_R_CINFIFO_INPUT_T1_INTERVAL, DNS_T1_INTERVAL);
	DNS_SET_R(base, set_id, DNS_R_CINFIFO_INPUT_T3_INTERVAL, DNS_MIN_HBLNK);

	return 0;
}

int dns_hw_s_cinfifo_config(void __iomem *base, u32 set_id, struct dns_cinfifo_config *config)
{
	u32 val;

	if (config->enable) {
		DNS_SET_R(base, set_id, DNS_R_CINFIFO_INPUT_CINFIFO_ENABLE, 1);

		val = 0;
		val = DNS_SET_V(val, DNS_F_CINFIFO_INPUT_IMAGE_WIDTH, config->width);
		val = DNS_SET_V(val, DNS_F_CINFIFO_INPUT_IMAGE_HEIGHT, config->height);
		DNS_SET_R(base, set_id, DNS_R_CINFIFO_INPUT_IMAGE_DIMENSIONS, val);
	} else
		DNS_SET_R(base, set_id, DNS_R_CINFIFO_INPUT_CINFIFO_ENABLE, 0);

	return 0;
}

int dns_hw_create_dma(void __iomem *base, enum dns_dma_id dma_id, struct is_common_dma *dma)
{
	int ret;
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	char name[64];

	switch (dma_id) {
	case DNS_RDMA_BAYER:
		base_reg = base + dns_regs[DNS_R_RDMA_BAYER_EN].sfr_offset;
		available_bayer_format_map =	(0
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
		strncpy(name, "DNS_RDMA_BAYER", sizeof(name) - 1);
		break;
	case DNS_RDMA_DRC0:
		base_reg = base + dns_regs[DNS_R_RDMA_DRC0_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "DNS_RDMA_DRC0", sizeof(name) - 1);
		break;
	case DNS_RDMA_DRC1:
		base_reg = base + dns_regs[DNS_R_RDMA_DRC1_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "DNS_RDMA_DRC1", sizeof(name) - 1);
		break;
	case DNS_RDMA_DRC2:
		base_reg = base + dns_regs[DNS_R_RDMA_DRC2_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		strncpy(name, "DNS_RDMA_DRC1", sizeof(name) - 1);
		break;
	default:
		err_hw("[DNS] invalid dma_id (%d)", dma_id);
		return -EINVAL;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name, available_bayer_format_map, 0, 0, 0);

	return ret;
}

int dns_hw_s_rdma(u32 set_id, struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	u32 *addr, u32 num_buffers)
{
	int ret;
	u32 stride_1p;
	u32 format, hwformat, memory_bitwidth, pixelsize;
	u32 width, height, full_width;
	u32 i;
	dma_addr_t address[IS_MAX_FRO];

	ret = CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
	ret |= CALL_DMA_OPS(dma, dma_enable, dma_input->cmd);

	if (dma_input->cmd == 0)
		return 0;

	switch (dma->id) {
	case DNS_RDMA_BAYER:
		width = dma_input->width;
		full_width = (stripe_input->total_count >= 2) ? stripe_input->full_width : dma_input->width;
		height = dma_input->height;
		hwformat = dma_input->format;
		memory_bitwidth = dma_input->bitwidth;
		pixelsize = dma_input->msb + 1;
		ret |= CALL_DMA_OPS(dma, dma_set_msb_align, 1);
		break;
	case DNS_RDMA_DRC0:
	case DNS_RDMA_DRC1:
	case DNS_RDMA_DRC2:
		width = ALIGN(dma_input->width, 4) * 4;
		full_width = width;
		height = dma_input->height;
		hwformat = DMA_INOUT_FORMAT_Y;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = 8;
		break;
	default:
		err_hw("[dns] invalid dma_id (%d)", dma->id);
		return -EINVAL;
	}

	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, 0, true);
	stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
			full_width, 16, true);

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);

	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + i);
	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, (dma_addr_t *)address, 0, 0, num_buffers);

	return ret;
}

int dns_hw_s_module_bypass(void __iomem *base, u32 set_id)
{
	DNS_SET_R(base, set_id, DNS_R_BLC_BYPASS, 1);
	DNS_SET_R(base, set_id, DNS_R_WBG_BYPASS, 1);
	DNS_SET_R(base, set_id, DNS_R_GAMMA_PRE_BYPASS, 1);
	DNS_SET_R(base, set_id, DNS_R_HQDNS_BYPASS, 1);
	DNS_SET_R(base, set_id, DNS_R_DRCDSTR_BYPASS, 1);
	DNS_SET_R(base, set_id, DNS_R_DRCCLCT_BYPASS, 1);

	return 0;
}

int dns_hw_bypass_drcdstr(void __iomem *base, u32 set_id)
{
	u32 val, rta_val;

	rta_val = DNS_GET_R(base, set_id, DNS_R_DRCDSTR_BYPASS);

	val = 1;
	DNS_SET_R(base, set_id, DNS_R_DRCDSTR_BYPASS, val);

	if (rta_val != val)
		return 1;
	else
		return 0;
}

int dns_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_hw_size_config *config, struct is_rectangle *drc_grid)
{
	u32 val;
	u32 scaling_x, scaling_y;
	u32 dma_crop_x;
	u32 sensor_center_x, sensor_center_y;
	const u64 binning_factor_x1_1024 = 1024;

	/* DTP */
	DNS_SET_R(base, set_id, DNS_R_SMIADTP_X_OUTPUT_SIZE, config->width);
	DNS_SET_R(base, set_id, DNS_R_SMIADTP_Y_OUTPUT_SIZE, config->height);

	/* HQDNS */
	scaling_x = (config->sensor_binning_x * config->bcrop_width * binning_factor_x1_1024) /
		    config->taasc_width / 1000;
	scaling_y = (config->sensor_binning_y * config->bcrop_height * binning_factor_x1_1024) /
		    config->height / 1000;
	DNS_SET_F(base, set_id, DNS_R_HQDNS_BINNING, DNS_F_HQDNS_BINNING_X, scaling_x);
	DNS_SET_F(base, set_id, DNS_R_HQDNS_BINNING, DNS_F_HQDNS_BINNING_Y, scaling_y);

	/* bds ratio*/
	dma_crop_x = config->bcrop_width * config->dma_crop_x / config->taasc_width;
	/* sensor binning ratio*/
	sensor_center_x = (-1) * (config->sensor_center_x) +
			((config->sensor_crop_x + config->bcrop_x + dma_crop_x) *
			config->sensor_binning_x / 1000);
	sensor_center_y = (-1) * (config->sensor_center_y) +
			((config->sensor_crop_y + config->bcrop_y) *
			config->sensor_binning_y / 1000);
	val = 0;
	val = DNS_SET_V(val, DNS_F_HQDNS_RADIAL_CENTER_X, sensor_center_x);
	val = DNS_SET_V(val, DNS_F_HQDNS_RADIAL_CENTER_Y, sensor_center_y);
	DNS_SET_R(base, set_id, DNS_R_HQDNS_RADIAL_CENTER, val);

	val = 0;
	val = DNS_SET_V(val, DNS_F_HQDNS_IMAGE_SIZE_X, config->width);
	val = DNS_SET_V(val, DNS_F_HQDNS_IMAGE_SIZE_Y, config->height);
	DNS_SET_R(base, set_id, DNS_R_HQDNS_IMAGE_SIZE, val);

	/* DRC DSTR */
	DNS_SET_R(base, set_id, DNS_R_DRCDSTR_STRIP_START_POSITION, config->dma_crop_x);
	DNS_SET_R(base, set_id, DNS_R_DRCDSTR_DMA_GRID_SIZE_X, drc_grid->w);
	DNS_SET_R(base, set_id, DNS_R_DRCDSTR_DMA_GRID_SIZE_Y, drc_grid->h);

	return 0;
}

void dns_hw_s_module_format(void __iomem *base, u32 set_id, u32 bayer_order)
{
	DNS_SET_R(base, set_id, DNS_R_SMIADTP_PIXEL_ORDER, bayer_order);
	DNS_SET_R(base, set_id, DNS_R_WBG_PIXEL_ORDER, bayer_order);
	DNS_SET_R(base, set_id, DNS_R_HQDNS_PIXEL_ORDER, bayer_order);
}

void dns_hw_enable_dtp(void __iomem *base, u32 set_id, bool en)
{
	/* enable macbeth color chart dtp */
	if (en)
		DNS_SET_R(base, set_id, DNS_R_SMIADTP_TEST_PATTERN_MODE, 256);
	else
		DNS_SET_R(base, set_id, DNS_R_SMIADTP_TEST_PATTERN_MODE, 0);
}

int dns_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers)
{
	u32 prev_fro_en = DNS_GET_R_DIRECT(base, DNS_R_FRO_MODE_EN);
	u32 fro_en = (num_buffers > 1) ? 1 : 0;

	if (prev_fro_en != fro_en) {
		info_hw("[DNS] FRO: %d -> %d\n", prev_fro_en, fro_en);
		/* fro core + Configuration register reset */
		DNS_SET_R_DIRECT(base, DNS_R_FRO_SW_RESET, 3);

		DNS_SET_R_DIRECT(base, DNS_R_FRO_INT0_CLEAR, (1 << INTR1_DNS_MAX) - 1);
		DNS_SET_R_DIRECT(base, DNS_R_FRO_INT1_CLEAR, (1 << INTR2_DNS_MAX) - 1);
	}

	DNS_SET_R_DIRECT(base, DNS_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW, (num_buffers - 1));
	DNS_SET_R_DIRECT(base, DNS_R_FRO_MODE_EN, fro_en);

	return 0;
}

void dns_hw_s_crc(void __iomem *base, u32 seed)
{
	DNS_SET_F_DIRECT(base, DNS_R_STRGEN_ITP_STREAM_CRC, DNS_F_STRGEN_ITP_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_SMIADTP_STREAM_CRC, DNS_F_SMIADTP_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_BLC_STREAM_CRC, DNS_F_BLC_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_WBG_STREAM_CRC, DNS_F_WBG_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_HQDNS_STREAM_CRC, DNS_F_HQDNS_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_DRCCLCT_STREAM_CRC, DNS_F_DRCCLCT_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_DRCDSTR_STREAM_CRC, DNS_F_DRCDSTR_CRC_SEED, seed);
	DNS_SET_F_DIRECT(base, DNS_R_GAMMA_PRE_STREAM_CRC, DNS_F_GAMMA_PRE_CRC_SEED, seed);
}

void dns_hw_dump(void __iomem *base)
{
	info_hw("[DNS] SFR DUMP (v1.20)\n");
	is_hw_dump_regs(base, dns_regs, DNS_REG_CNT);
}

u32 dns_hw_g_reg_cnt(void)
{
	return DNS_REG_CNT;
}
