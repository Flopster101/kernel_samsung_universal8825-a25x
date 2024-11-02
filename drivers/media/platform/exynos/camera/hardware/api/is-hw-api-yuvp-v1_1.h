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

#ifndef IS_HW_API_YUVP_V1_1_H
#define IS_HW_API_YUVP_V1_1_H

#include "is-hw-yuvp-v2.h"
#include "is-hw-common-dma.h"

#define YUVP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &yuvp_regs[R], &yuvp_fields[F], val)
#define YUVP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &yuvp_regs[R], &yuvp_fields[F], val)
#define YUVP_SET_R(base, R, val) \
	is_hw_set_reg(base, &yuvp_regs[R], val)
#define YUVP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &yuvp_regs[R], val)
#define YUVP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &yuvp_fields[F], val)

#define YUVP_GET_F(base, R, F) \
	is_hw_get_field(base, &yuvp_regs[R], &yuvp_fields[F])
#define YUVP_GET_R(base, R) \
	is_hw_get_reg(base, &yuvp_regs[R])
#define YUVP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &yuvp_regs[R])
#define YUVP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &yuvp_fields[F])

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define HBLANK_CYCLE			(0x2D)

enum set_status {
	SET_SUCCESS,
	SET_ERROR,
};

enum yuvp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ERR,
};

enum yuvp_input_path_type {
	YUVP_INPUT_RDMA = 0,
	YUVP_INPUT_OTF = 1,
};

enum yuvp_mux_dtp_type {
	YUVP_MUX_DTP_CINFIFO = 0,
	YUVP_MUX_DTP_RDMA_YUV = 1,
	YUVP_MUX_DTP_RDMA_MONO = 2,
};

enum yuvp_chain_demux_type {
	YUVP_DEMUX_DITHER_COUTFIFO_0 = (1 << 0),
	YUVP_DEMUX_DITHER_WDMA = (1 << 1),
	YUVP_DEMUX_YUVNR_YUV422TO444 = (1 << 8),
	YUVP_DEMUX_YUVNR_COUTFIFIO_1 = (1 << 9),
	YUVP_DEMUX_SEGCONF_YUVNR = (1 << 16),
	YUVP_DEMUX_SEGCONF_CCM = (1 << 17),
	YUVP_DEMUX_SEGCONF_SHARPEN = (1 << 18),
};

enum yuvp_dtp_pattern {
	YUVP_DTP_PATTERN_SOLID = 1,
	YUVP_DTP_PATTERN_COLORBAR = 2,
};

enum yuvp_chain_debug {
	YUVP_DTP_DBG_CNT_ISP_IN = 0,
	YUVP_DTP_DBG_CNT_ISP_OUT,
	YUVP_YUVNR_DBG_CNT_ISP_IN0,
	YUVP_YUVNR_DBG_CNT_ISP_IN1,
	YUVP_YUVNR_DBG_CNT_ISP_OUT,
	YUV422TO444_DBG_CNT_ISP_IN,
	YUV422TO444_DBG_CNT_ISP_OUT,
	YUV2RGB_DBG_CNT_ISP_IN,
	YUV2RGB_DBG_CNT_ISP_OUT,
	DEGAMMA_DBG_CNT_ISP_IN,
	DEGAMMA_DBG_CNT_ISP_OUT,
	INVCCM_DBG_CNT_ISP_IN,
	INVCCM_DBG_CNT_ISP_OUT,
	DRC_DSTR_DBG_CNT_ISP_IN0,
	DRC_DSTR_DBG_CNT_ISP_IN1,
	DRC_DSTR_DBG_CNT_ISP_IN2,
	DRC_DSTR_DBG_CNT_ISP_OUT,
	DRC_TMC_DBG_CNT_ISP_IN,
	DRC_TMC_DBG_CNT_ISP_OUT,
	CCM_DBG_CNT_ISP_IN0,
	CCM_DBG_CNT_ISP_IN1,
	CCM_DBG_CNT_ISP_OUT,
	RGB_GAMMA_DBG_CNT_ISP_IN,
	RGB_GAMMA_DBG_CNT_ISP_OUT,
	CLAHE_DBG_CNT_ISP_IN,
	CLAHE_DBG_CNT_ISP_OUT,
	PRC_DBG_CNT_ISP_IN,
	PRC_DBG_CNT_ISP_OUT,
	RGB2YUV_DBG_CNT_ISP_IN,
	RGB2YUV_DBG_CNT_ISP_OUT,
	YUVP_SHARPEN_DBG_CNT_ISP_IN0,
	YUVP_SHARPEN_DBG_CNT_ISP_IN1,
	YUVP_SHARPEN_DBG_CNT_ISP_OUT,
	OETF_GAMMA_DBG_CNT_ISP_IN,
	OETF_GAMMA_DBG_CNT_ISP_OUT,
	YUV444TO422_DBG_CNT_ISP_IN,
	YUV444TO422_DBG_CNT_ISP_OUT,
	DITHER_DBG_CNT_ISP_IN,
	DITHER_DBG_CNT_ISP_OUT,
	YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_IN,
	YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT0,
	YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT1,
	YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT2,
	YUVP_CHAIN_DEBUG_COUNTER_MAX,
};

struct yuvp_chain_debug_counter {
	enum yuvp_chain_debug	counter;
	char		 	*name;
};

static const struct yuvp_chain_debug_counter yuvp_counter[YUVP_CHAIN_DEBUG_COUNTER_MAX] = {
	{YUVP_DTP_DBG_CNT_ISP_IN, "YUVP_DTP_DBG_CNT_ISP_IN"},
	{YUVP_DTP_DBG_CNT_ISP_OUT, "YUVP_DTP_DBG_CNT_ISP_OUT"},
	{YUVP_YUVNR_DBG_CNT_ISP_IN0, "YUVP_YUVNR_DBG_CNT_ISP_IN0"},
	{YUVP_YUVNR_DBG_CNT_ISP_IN1, "YUVP_YUVNR_DBG_CNT_ISP_IN1"},
	{YUVP_YUVNR_DBG_CNT_ISP_OUT, "YUVP_YUVNR_DBG_CNT_ISP_OUT"},
	{YUV422TO444_DBG_CNT_ISP_IN, "YUV422TO444_DBG_CNT_ISP_IN"},
	{YUV422TO444_DBG_CNT_ISP_OUT, "YUV422TO444_DBG_CNT_ISP_OUT"},
	{YUV2RGB_DBG_CNT_ISP_IN, "YUV2RGB_DBG_CNT_ISP_IN"},
	{YUV2RGB_DBG_CNT_ISP_OUT, "YUV2RGB_DBG_CNT_ISP_OUT"},
	{DEGAMMA_DBG_CNT_ISP_IN, "DEGAMMA_DBG_CNT_ISP_IN"},
	{DEGAMMA_DBG_CNT_ISP_OUT, "DEGAMMA_DBG_CNT_ISP_OUT"},
	{INVCCM_DBG_CNT_ISP_IN, "INVCCM_DBG_CNT_ISP_IN"},
	{INVCCM_DBG_CNT_ISP_OUT, "INVCCM_DBG_CNT_ISP_OUT"},
	{DRC_DSTR_DBG_CNT_ISP_IN0, "DRC_DSTR_DBG_CNT_ISP_IN0"},
	{DRC_DSTR_DBG_CNT_ISP_IN1, "DRC_DSTR_DBG_CNT_ISP_IN1"},
	{DRC_DSTR_DBG_CNT_ISP_IN2, "DRC_DSTR_DBG_CNT_ISP_IN2"},
	{DRC_DSTR_DBG_CNT_ISP_OUT, "DRC_DSTR_DBG_CNT_ISP_OUT"},
	{DRC_TMC_DBG_CNT_ISP_IN, "DRC_TMC_DBG_CNT_ISP_IN"},
	{DRC_TMC_DBG_CNT_ISP_OUT, "DRC_TMC_DBG_CNT_ISP_OUT"},
	{CCM_DBG_CNT_ISP_IN0, "CCM_DBG_CNT_ISP_IN0"},
	{CCM_DBG_CNT_ISP_IN1, "CCM_DBG_CNT_ISP_IN1"},
	{CCM_DBG_CNT_ISP_OUT, "CCM_DBG_CNT_ISP_OUT"},
	{RGB_GAMMA_DBG_CNT_ISP_IN, "RGB_GAMMA_DBG_CNT_ISP_IN"},
	{RGB_GAMMA_DBG_CNT_ISP_OUT, "RGB_GAMMA_DBG_CNT_ISP_OUT"},
	{CLAHE_DBG_CNT_ISP_IN, "CLAHE_DBG_CNT_ISP_IN"},
	{CLAHE_DBG_CNT_ISP_OUT, "CLAHE_DBG_CNT_ISP_OUT"},
	{PRC_DBG_CNT_ISP_IN, "PRC_DBG_CNT_ISP_IN"},
	{PRC_DBG_CNT_ISP_OUT, "PRC_DBG_CNT_ISP_OUT"},
	{RGB2YUV_DBG_CNT_ISP_IN, "RGB2YUV_DBG_CNT_ISP_IN"},
	{RGB2YUV_DBG_CNT_ISP_OUT, "RGB2YUV_DBG_CNT_ISP_OUT"},
	{YUVP_SHARPEN_DBG_CNT_ISP_IN0, "YUVP_SHARPEN_DBG_CNT_ISP_IN0"},
	{YUVP_SHARPEN_DBG_CNT_ISP_IN1, "YUVP_SHARPEN_DBG_CNT_ISP_IN1"},
	{YUVP_SHARPEN_DBG_CNT_ISP_OUT, "YUVP_SHARPEN_DBG_CNT_ISP_OUT"},
	{OETF_GAMMA_DBG_CNT_ISP_IN, "OETF_GAMMA_DBG_CNT_ISP_IN"},
	{OETF_GAMMA_DBG_CNT_ISP_OUT, "OETF_GAMMA_DBG_CNT_ISP_OUT"},
	{YUV444TO422_DBG_CNT_ISP_IN, "YUV444TO422_DBG_CNT_ISP_IN"},
	{YUV444TO422_DBG_CNT_ISP_OUT, "YUV444TO422_DBG_CNT_ISP_OUT"},
	{DITHER_DBG_CNT_ISP_IN, "DITHER_DBG_CNT_ISP_IN"},
	{DITHER_DBG_CNT_ISP_OUT, "DITHER_DBG_CNT_ISP_OUT"},
	{YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_IN, "YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_IN"},
	{YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT0, "YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT0"},
	{YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT1, "YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT1"},
	{YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT2, "YUVP_SYNCFIFO_SEG_DBG_CNT_ISP_OUT2"},
};

struct yuvp_radial_cfg {
	u32 sensor_full_width;
	u32 sensor_full_height;
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	u32 sensor_crop_x;
	u32 sensor_crop_y;
	u32 bns_binning_x;
	u32 bns_binning_y;
	u32 taa_crop_x;
	u32 taa_crop_y;
	u32 taa_crop_width;
	u32 taa_crop_height;
};

int yuvp_hw_s_reset(void __iomem *base);
void yuvp_hw_s_init(void __iomem *base);
unsigned int yuvp_hw_is_occured0(unsigned int status, enum yuvp_event_type type);
unsigned int yuvp_hw_is_occured1(unsigned int status, enum yuvp_event_type type);
int yuvp_hw_wait_idle(void __iomem *base);
void yuvp_hw_dump(void __iomem *base);
void yuvp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id);
void yuvp_hw_s_cinfifo(void __iomem *base, u32 set_id);
void yuvp_hw_s_strgen(void __iomem *base, u32 set_id);
void yuvp_hw_s_coutfifo(void __iomem *base, u32 set_id);
void yuvp_hw_dma_dump(struct is_common_dma *dma);
void yuvp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id);
int yuvp_hw_s_rdma_init(struct is_common_dma *dma, struct yuvp_param_set *param_set,
	u32 enable, u32 vhist_grid_num, u32 drc_grid_w, u32 drc_grid_h,
	u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size);
int yuvp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size);
int yuvp_hw_s_corex_update_type(void __iomem *base, u32 set_id);
void yuvp_hw_s_cmdq(void __iomem *base, u32 set_id);
void yuvp_hw_s_corex_init(void __iomem *base, bool enable);
unsigned int yuvp_hw_g_int_state0(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int yuvp_hw_g_int_mask0(void __iomem *base);
void yuvp_hw_s_int_mask0(void __iomem *base, u32 mask);
unsigned int yuvp_hw_g_int_state1(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int yuvp_hw_g_int_mask1(void __iomem *base);
int yuvp_hw_s_yuvnr_size(void __iomem *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg,
	u32 yuvnr_hal_biquad_scale_shift_adder);
int yuvp_hw_dma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void yuvp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void yuvp_hw_s_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable);
void yuvp_hw_s_input_path(void __iomem *base, u32 set_id, enum yuvp_input_path_type path);
void yuvp_hw_s_output_path(void __iomem *base, u32 set_id, int path);
void yuvp_hw_s_demux_enable(void __iomem *base, u32 set_id, enum yuvp_chain_demux_type type);
void yuvp_hw_s_mux_dtp(void __iomem *base, u32 set_id, enum yuvp_mux_dtp_type type);
void yuvp_hw_s_dtp_pattern(void __iomem *base, u32 set_id, enum yuvp_dtp_pattern pattern);
void yuvp_hw_print_chain_debug_cnt(void __iomem *base);
int yuvp_hw_s_drc_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg);
int yuvp_hw_s_clahe_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg);
int yuvp_hw_s_sharpen_size(void __iomem *base, u32 set_id, u32 yuvp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg);

u32 yuvp_hw_g_reg_cnt(void);
void yuvp_hw_s_crc(void __iomem *base, u32 seed);

#endif
