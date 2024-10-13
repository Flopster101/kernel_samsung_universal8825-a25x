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

#ifndef IS_HW_API_DRCP_V1_0_H
#define IS_HW_API_DRCP_V1_0_H

#include "is-hw-yuvp.h"
#include "is-hw-common-dma.h"


#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define HBLANK_CYCLE			(0x2D)

enum drcp_chain_debug {
	YUV422TO444_DBG_CNT_ISP_IN = 0,
	YUV422TO444_DBG_CNT_ISP_OUT,
	YUV2RGB_DBG_CNT_ISP_IN,
	YUV2RGB_DBG_CNT_ISP_OUT,
	DRC_DSTR_DBG_CNT_ISP_IN0,
	DRC_DSTR_DBG_CNT_ISP_IN1,
	DRC_DSTR_DBG_CNT_ISP_IN2,
	DRC_DSTR_DBG_CNT_ISP_IN3,
	DRC_DSTR_DBG_CNT_ISP_OUT,
	DRC_TMC_DBG_CNT_ISP_IN,
	DRC_TMC_DBG_CNT_ISP_OUT0,
	DRC_TMC_DBG_CNT_ISP_OUT1,
	CLAHE_DBG_CNT_ISP_IN,
	CLAHE_DBG_CNT_ISP_OUT,
	CCM_DBG_CNT_ISP_IN0,
	CCM_DBG_CNT_ISP_IN1,
	CCM_DBG_CNT_ISP_OUT0,
	CCM_DBG_CNT_ISP_OUT1,
	RGB_GAMMA_DBG_CNT_ISP_IN,
	RGB_GAMMA_DBG_CNT_ISP_OUT,
	PRC_DBG_CNT_ISP_IN,
	PRC_DBG_CNT_ISP_OUT,
	CONT_DET_DBG_CNT_ISP_IN0,
	CONT_DET_DBG_CNT_ISP_IN1,
	CONT_DET_DBG_CNT_ISP_OUT,
	SNADD_DBG_CNT_ISP_IN0,
	SNADD_DBG_CNT_ISP_IN1,
	SNADD_DBG_CNT_ISP_IN2,
	SNADD_DBG_CNT_ISP_IN3,
	SNADD_DBG_CNT_ISP_OUT,
	OETF_GAMMA_DBG_CNT_ISP_IN,
	OETF_GAMMA_DBG_CNT_ISP_OUT,
	RGB2YUV_DBG_CNT_ISP_IN,
	RGB2YUV_DBG_CNT_ISP_OUT,
	YUV444TO422_DBG_CNT_ISP_IN,
	YUV444TO422_DBG_CNT_ISP_OUT,
	DITHER_DBG_CNT_ISP_IN,
	DITHER_DBG_CNT_ISP_OUT,
	DRCP_CHAIN_DEBUG_COUNTER_MAX,
};

struct drcp_chain_debug_counter {
	enum drcp_chain_debug	counter;
	char		 	*name;
};

static const struct drcp_chain_debug_counter drcp_counter[DRCP_CHAIN_DEBUG_COUNTER_MAX] = {
	{YUV422TO444_DBG_CNT_ISP_IN, "YUV422TO444_DBG_CNT_ISP_IN"},
	{YUV422TO444_DBG_CNT_ISP_OUT, "YUV422TO444_DBG_CNT_ISP_OUT"},
	{YUV2RGB_DBG_CNT_ISP_IN, "YUV2RGB_DBG_CNT_ISP_IN"},
	{YUV2RGB_DBG_CNT_ISP_OUT, "YUV2RGB_DBG_CNT_ISP_OUT"},
	{DRC_DSTR_DBG_CNT_ISP_IN0, "DRC_DSTR_DBG_CNT_ISP_IN0"},
	{DRC_DSTR_DBG_CNT_ISP_IN1, "DRC_DSTR_DBG_CNT_ISP_IN1"},
	{DRC_DSTR_DBG_CNT_ISP_IN2, "DRC_DSTR_DBG_CNT_ISP_IN2"},
	{DRC_DSTR_DBG_CNT_ISP_IN3, "DRC_DSTR_DBG_CNT_ISP_IN3"},
	{DRC_DSTR_DBG_CNT_ISP_OUT, "DRC_DSTR_DBG_CNT_ISP_OUT"},
	{DRC_TMC_DBG_CNT_ISP_IN, "DRC_TMC_DBG_CNT_ISP_IN"},
	{DRC_TMC_DBG_CNT_ISP_OUT0, "DRC_TMC_DBG_CNT_ISP_OUT0"},
	{DRC_TMC_DBG_CNT_ISP_OUT1, "DRC_TMC_DBG_CNT_ISP_OUT1"},
	{CLAHE_DBG_CNT_ISP_IN, "CLAHE_DBG_CNT_ISP_IN"},
	{CLAHE_DBG_CNT_ISP_OUT, "CLAHE_DBG_CNT_ISP_OUT"},
	{CCM_DBG_CNT_ISP_IN0, "CCM_DBG_CNT_ISP_IN0"},
	{CCM_DBG_CNT_ISP_IN1, "CCM_DBG_CNT_ISP_IN1"},
	{CCM_DBG_CNT_ISP_OUT0, "CCM_DBG_CNT_ISP_OUT0"},
	{CCM_DBG_CNT_ISP_OUT1, "CCM_DBG_CNT_ISP_OUT1"},
	{RGB_GAMMA_DBG_CNT_ISP_IN, "RGB_GAMMA_DBG_CNT_ISP_IN"},
	{RGB_GAMMA_DBG_CNT_ISP_OUT, "RGB_GAMMA_DBG_CNT_ISP_OUT"},
	{PRC_DBG_CNT_ISP_IN, "PRC_DBG_CNT_ISP_IN"},
	{PRC_DBG_CNT_ISP_OUT, "PRC_DBG_CNT_ISP_OUT"},
	{CONT_DET_DBG_CNT_ISP_IN0, "CONT_DET_DBG_CNT_ISP_IN0"},
	{CONT_DET_DBG_CNT_ISP_IN1, "CONT_DET_DBG_CNT_ISP_IN1"},
	{CONT_DET_DBG_CNT_ISP_OUT, "CONT_DET_DBG_CNT_ISP_OUT"},
	{SNADD_DBG_CNT_ISP_IN0, "SNADD_DBG_CNT_ISP_IN0"},
	{SNADD_DBG_CNT_ISP_IN1, "SNADD_DBG_CNT_ISP_IN1"},
	{SNADD_DBG_CNT_ISP_IN2, "SNADD_DBG_CNT_ISP_IN2"},
	{SNADD_DBG_CNT_ISP_IN3, "SNADD_DBG_CNT_ISP_IN3"},
	{SNADD_DBG_CNT_ISP_OUT, "SNADD_DBG_CNT_ISP_OUT"},
	{OETF_GAMMA_DBG_CNT_ISP_IN, "OETF_GAMMA_DBG_CNT_ISP_IN"},
	{OETF_GAMMA_DBG_CNT_ISP_OUT, "OETF_GAMMA_DBG_CNT_ISP_OUT"},
	{RGB2YUV_DBG_CNT_ISP_IN, "RGB2YUV_DBG_CNT_ISP_IN"},
	{RGB2YUV_DBG_CNT_ISP_OUT, "RGB2YUV_DBG_CNT_ISP_OUT"},
	{YUV444TO422_DBG_CNT_ISP_IN, "YUV444TO422_DBG_CNT_ISP_IN"},
	{YUV444TO422_DBG_CNT_ISP_OUT, "YUV444TO422_DBG_CNT_ISP_OUT"},
	{DITHER_DBG_CNT_ISP_IN, "DITHER_DBG_CNT_ISP_IN"},
	{DITHER_DBG_CNT_ISP_OUT, "DITHER_DBG_CNT_ISP_OUT"},
};

unsigned int drcp_hw_is_occured(unsigned int status, enum yuvp_event_type type);
unsigned int drcp_hw_is_occured0(unsigned int status, enum yuvp_event_type type);
unsigned int drcp_hw_is_occured1(unsigned int status, enum yuvp_event_type type);
unsigned int drcp_hw_g_int_state0(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int drcp_hw_g_int_state1(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int drcp_hw_g_int_mask0(void __iomem *base);
unsigned int drcp_hw_g_int_mask1(void __iomem *base);
int drcp_hw_s_reset(void __iomem *base);
void drcp_hw_s_init(void __iomem *base);
int drcp_hw_wait_idle(void __iomem *base);
void drcp_hw_dump(void __iomem *base);
void drcp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void drcp_hw_g_hist_grid_num(void __iomem *base, u32 set_id, u32 *grid_x_num, u32 *grid_y_num);
void drcp_hw_g_drc_grid_size(void __iomem *base, u32 set_id, u32 *grid_size_x, u32 *grid_size_y);
void drcp_hw_s_clahe_bypass(void __iomem *base, u32 set_id);
void drcp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void drcp_hw_s_yuv444to422_coeff(void __iomem *base, u32 set_id);
void drcp_hw_s_yuvtorgb_coeff(void __iomem *base, u32 set_id);
void drcp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id);
void drcp_hw_s_coutfifo(void __iomem *base, u32 set_id, u32 enable);
void _drcp_hw_s_cinfifo(void __iomem *base, u32 set_id);
void _drcp_hw_s_common(void __iomem *base);
void _drcp_hw_s_int_mask(void __iomem *base);
void _drcp_hw_s_secure_id(void __iomem *base, u32 set_id);
void _drcp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id);
int drcp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void drcp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id);
int drcp_hw_s_corex_update_type(void __iomem *base, u32 set_id);
void drcp_hw_s_cmdq(void __iomem *base, u32 set_id);
void drcp_hw_s_corex_init(void __iomem *base, bool enable);
void _drcp_hw_wait_corex_idle(void __iomem *base);
unsigned int drcp_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int drcp_hw_g_int_mask(void __iomem *base);
int drcp_hw_s_drc_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height);
int drcp_hw_s_clahe_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height);
int drcp_hw_s_sharpadder_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable,
	u32 sensor_full_width, u32 sensor_full_height,
	u32 sensor_binning_x, u32 sensor_binning_y, u32 sensor_crop_x, u32 sensor_crop_y,
	u32 taa_crop_x, u32 taa_crop_y, u32 taa_crop_width, u32 taa_crop_height);
void drcp_hw_s_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable);
void drcp_hw_s_output_path(void __iomem *base, u32 set_id, int val);
void drcp_hw_s_demux_dither(void __iomem *base, u32 set_id, enum drcp_demux_dither_type type);
void drcp_hw_print_chain_debug_cnt(void __iomem *base);

u32 drcp_hw_g_reg_cnt(void);
#endif
