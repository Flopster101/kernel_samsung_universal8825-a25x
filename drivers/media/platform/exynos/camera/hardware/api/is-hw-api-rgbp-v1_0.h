/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * rgbp HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_RGB_H
#define IS_HW_API_RGB_H

#include "../include/is-hw-rgbp.h"
#include "../include/is-hw-common-dma.h"

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define RGBP_RATIO_X8_8      1048576
#define RGBP_RATIO_X7_8      1198373
#define RGBP_RATIO_X6_8      1398101
#define RGBP_RATIO_X5_8      1677722
#define RGBP_RATIO_X4_8      2097152
#define RGBP_RATIO_X3_8      2796203
#define RGBP_RATIO_X2_8      4194304

enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum rgbp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2
};

enum rgbp_event_type {
	/* EVT0 INT0 */
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_CMDQ_HOLD,
	INTR_SETTING_DONE,
	INTR_C_LOADER_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ROW_COL,
	INTR_FREEZE_ONROW_COL,
	INTR_TRANS_STOP_DONE,
	INTR_CMDQ_ERROR = 10,
	INTR_C_LOADER_ERROR,
	INTR_COREX_ERROR,
	INTR_CINFIFO_OVERFLOW_ERROR,
	INTR_CINFIFO_OVERLAP_ERROR,
	INTR_CINFIFO_PIXEL_CNT_ERROR,
	INTR_CINFIFO_INPUT_PROTOCOL_ERROR,
	INTR_COUTFIFO_PIXEL_CNT_ERROR = 21,
	INTR_COUTFIFO_INPUT_PROTOCOL_ERROR,
	INTR_COUTFIFO_OVERFLOW_ERROR,
	/*
	 * INTR_VOTF_GLOBAL_ERROR = 27,
	 * INTR_VOTF_LOST_CONNECTION,
	 * INTR_OTF_SEQ_ID_ERROR,
	 */
	INTR_ERR0,
};

enum rgbp_event_type1 {
	/* EVT INT1 */
	INTR_VOTF0_LOST_CONNECTION,
	INTR_VOTF0_SLOW_RING,
	INTR_VOTF1_SLOW_RING,
	INTR_VOTF0_LOST_FLUSH,
	INTR_VOTF1_LOST_FLUSH,
	INTR_DTP_DBG_CNT_ERROR,
	INTR_BDNS_DBG_CNT_ERROR,
	INTR_DMSC_DBG_CNT_ERROR,
	INTR_LSC_DBG_CNT_ERROR,
	INTR_RGP_GAMMA_DEG_CNT_ERROR,
	INTR_GTM_DBG_CNT_ERROR,
	INTR_RGB2YUV_DBG_CNT_ERROR,
	INTR_YUV444TO422_DBG_CNT_ERROR,
	INTR_SATFLAG_DBG_CNT_ERROR,
	INTR_DECOMP_DBG_CNT_ERROR,
	INTR_CCM_DBG_CNT_ERROR,
	INTR_YUVSC_DBG_CNT_ERROR,
	INTR_SYNC_MERGE_COUTFIFO_DBG_CNT_ERROR,
	INTR_ERR1,
};

enum rgbp_filter_coeff {
	RGBP_COEFF_X8_8 = 0,	/* A (8/8 ~ ) */
	RGBP_COEFF_X7_8 = 1,	/* B (7/8 ~ ) */
	RGBP_COEFF_X6_8 = 2,	/* C (6/8 ~ ) */
	RGBP_COEFF_X5_8 = 3,	/* D (5/8 ~ ) */
	RGBP_COEFF_X4_8 = 4,	/* E (4/8 ~ ) */
	RGBP_COEFF_X3_8 = 5,	/* F (3/8 ~ ) */
	RGBP_COEFF_X2_8 = 6,	/* G (2/8 ~ ) */
	RGBP_COEFF_MAX
};

struct rgbp_v_coef {
	int v_coef_a[9];
	int v_coef_b[9];
	int v_coef_c[9];
	int v_coef_d[9];
};

struct rgbp_h_coef {
	int h_coef_a[9];
	int h_coef_b[9];
	int h_coef_c[9];
	int h_coef_d[9];
	int h_coef_e[9];
	int h_coef_f[9];
	int h_coef_g[9];
	int h_coef_h[9];
};

struct rgbp_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

struct rgbp_radial_cfg {
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

int rgbp_hw_s_reset(void __iomem *base);
void rgbp_hw_s_init(void __iomem *base);
void rgbp_hw_s_dtp(void __iomem *base, u32 set_id, u32 pattern);
void rgbp_hw_s_path(void __iomem *base, u32 set_id, struct is_rgbp_config *config);
unsigned int rgbp_hw_is_occurred0(unsigned int status, enum rgbp_event_type type);
unsigned int rgbp_hw_is_occurred1(unsigned int status, enum rgbp_event_type1 type);
int rgbp_hw_wait_idle(void __iomem *base);
void rgbp_hw_dump(void __iomem *base);
void rgbp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id,
	struct is_rgbp_config *config, struct rgbp_param_set *param_set);
void rgbp_hw_s_cin_fifo(void __iomem *base, u32 set_id);
void rgbp_hw_s_cout_fifo(void __iomem *base, u32 set_id, bool enable);
void rgbp_hw_s_common(void __iomem *base);
void rgbp_hw_s_int_mask(void __iomem *base);
void rgbp_hw_s_secure_id(void __iomem *base, u32 set_id);
void rgbp_hw_dma_dump(struct is_common_dma *dma);

u32 *rgbp_hw_g_input_dva(struct rgbp_param_set *param_set, u32 instance, u32 id, u32 *cmd);
u32 *rgbp_hw_g_output_dva(struct rgbp_param_set *param_set, struct is_param_region *param,
								u32 instance,  u32 id, u32 *cmd);

void rgbp_hw_s_rdma_corex_id(struct is_common_dma *rdma, u32 set_id);
int rgbp_hw_s_rdma_init(struct is_hw_ip *hw_ip, struct is_common_dma *rdma, struct rgbp_param_set *param_set,
	u32 enable, u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size);
int rgbp_hw_rdma_create(struct is_common_dma *rdma, void __iomem *base, u32 input_id);
int rgbp_hw_s_rdma_addr(struct is_common_dma *rdma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size);

void rgbp_hw_s_wdma_corex_id(struct is_common_dma *wdma, u32 set_id);
int rgbp_hw_s_wdma_init(struct is_hw_ip *hw_ip, struct is_common_dma *wdma, struct rgbp_param_set *param_set,
	u32 instance, u32 enable, u32 in_crop_size_x, u32 cache_hint, u32 *sbwc_en, u32 *payload_size);
int rgbp_hw_wdma_create(struct is_common_dma *wdma, void __iomem *base, u32 input_id);
int rgbp_hw_s_wdma_addr(struct is_common_dma *wdma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size);

void rgbp_hw_s_cmdq(void __iomem *base, u32 set_id);
void rgbp_hw_wait_corex_idle(void __iomem *base);
int rgbp_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type);
void rgbp_hw_s_corex_init(void __iomem *base, bool enable);
void rgbp_hw_s_corex_start(void __iomem *base, bool enable);
unsigned int rgbp_hw_g_int0_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int rgbp_hw_g_int0_mask(void __iomem *base);
unsigned int rgbp_hw_g_int1_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int rgbp_hw_g_int1_mask(void __iomem *base);
void rgbp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void rgbp_hw_s_block_crc(void __iomem *base, u32 seed);
void rgbp_hw_s_pixel_order(void __iomem *base, u32 set_id, u32 pixel_order);
void rgbp_hw_s_chain_src_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_chain_dst_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_dtp_output_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_decomp_frame_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void rgbp_hw_s_dns_size(void __iomem *base, u32 set_id,
		u32 width, u32 height, bool strip_enable, u32 strip_start_pos,
		struct rgbp_radial_cfg *radial_cfg, struct is_rgbp_config *rgbp_config);

/* yuvsc */
void rgbp_hw_s_yuvsc_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass);
void rgbp_hw_s_yuvsc_src_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void is_scaler_get_yuvsc_src_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size);
void rgbp_hw_s_yuvsc_input_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void rgbp_hw_s_yuvsc_dst_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void is_scaler_get_yuvsc_dst_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size);
void rgbp_hw_s_yuvsc_out_crop_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void rgbp_hw_s_yuvsc_scaling_ratio(void __iomem *base, u32 set_id, u32 hratio, u32 vratio);
void rgbp_hw_s_h_init_phase_offset(void __iomem *base, u32 set_id, u32 h_offset);
void rgbp_hw_s_v_init_phase_offset(void __iomem *base, u32 set_id, u32 v_offset);
void rgbp_hw_s_yuvsc_coef(void __iomem *base, u32 set_id,
	u32 hratio, u32 vratio);
void rgbp_hw_s_yuvsc_round_mode(void __iomem *base, u32 set_id, u32 mode);
void rgbp_hw_s_ds_y_upsc_us(void __iomem *base, u32 set_id, u32 set);

/* upsc */
void rgbp_hw_s_upsc_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass);
void rgbp_hw_s_upsc_src_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void is_scaler_get_upsc_src_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size);
void rgbp_hw_s_upsc_input_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void rgbp_hw_s_upsc_dst_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void is_scaler_get_upsc_dst_size(void __iomem *base, u32 set_id, u32 *h_size, u32 *v_size);
void rgbp_hw_s_upsc_out_crop_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void rgbp_hw_s_upsc_scaling_ratio(void __iomem *base, u32 set_id, u32 hratio, u32 vratio);
void rgbp_hw_s_h_init_phase_offset(void __iomem *base, u32 set_id, u32 h_offset);
void rgbp_hw_s_v_init_phase_offset(void __iomem *base, u32 set_id, u32 v_offset);
void rgbp_hw_s_upsc_coef(void __iomem *base, u32 set_id, u32 hratio, u32 vratio);
void rgbp_hw_s_upsc_round_mode(void __iomem *base, u32 set_id, u32 mode);

void rgbp_hw_s_gamma_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass);

void rgbp_hw_s_decomp_enable(void __iomem *base, u32 set_id, u32 enable, u32 bypass);
void rgbp_hw_s_decomp_size(void __iomem *base, u32 set_id, u32 h_size, u32 v_size);
void rgbp_hw_s_decomp_iq(void __iomem *base, u32 set_id);

void rgbp_hw_s_grid_cfg(void __iomem *base, struct rgbp_grid_cfg *cfg);

void rgbp_hw_s_votf(void __iomem *base, u32 set_id, bool enable, bool stall);
void rgbp_hw_s_sbwc(void __iomem *base, u32 set_id, bool enable);

void rgbp_hw_s_crop(void __iomem *base, int in_width, int in_height, int out_width, int out_height);
u32 rgbp_hw_g_reg_cnt(void);

void rgbp_hw_s_sc_input_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_sc_src_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_sc_dst_size_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void rgbp_hw_s_sc_output_crop_size(void __iomem *base, u32 set_id, u32 width, u32 height);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_rgbp_hw_s_rgb_rdma_format(void __iomem *base, u32 rgb_format);
int pablo_kunit_rgbp_hw_s_rgb_wdma_format(void __iomem *base, u32 rgb_format);
#endif
#endif
