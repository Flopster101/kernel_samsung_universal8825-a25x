/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * byrp HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_BYRP_H
#define IS_HW_API_BYRP_H

#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-hw-common-dma.h"
#include "is-type.h"

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define T1_INTERVAL			(0x20)

enum is_hw_byrp_lib_mode {
	BYRP_USE_ONLY_DDK = 1,
	BYRP_USE_DRIVER = 2,
};

enum is_hw_byrp_rdma_index {
	BYRP_RDMA_IMG = 1,
	BYRP_RDMA_HDR = 2,
	BYRP_RDMA_MAX
};

enum is_hw_byrp_wdma_index {
	BYRP_WDMA_BYR = 3,
	BYRP_WDMA_MAX
};

enum is_hw_byrp_irq_src {
	BYRP_INT0,
	BYRP_INT1,
	BYRP_INTR_MAX,
};

enum byrp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2
};

enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum byrp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_CINROW,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ERR
};

enum byrp_bcrop_type {
	BYRP_BCROP_BYR = 0, /* Dzoom */
	BYRP_BCROP_ZSL = 1, /* Not Use */
	BYRP_BCROP_RGB = 2, /* DNG capture */
	BYRP_BCROP_STRP = 3, /* Not Use */
	BYRP_BCROP_MAX
};

enum byrp_dtp_channel {
	BYRP_DTP_CH0 = 1,
	BYRP_DTP_CH1 = 2,
	BYRP_DTP_CH2 = 4,
	BYRP_DTP_CH3 = 8,
	BYRP_DTP_MAX
};

enum byrp_img_fmt {
	BYRP_IMG_FMT_8BIT	= 0,
	BYRP_IMG_FMT_10BIT	= 1,
	BYRP_IMG_FMT_12BIT	= 2,
	BYRP_IMG_FMT_14BIT	= 3,
	BYRP_IMG_FMT_9BIT	= 4,
	BYRP_IMG_FMT_11BIT	= 5,
	BYRP_IMG_FMT_13BIT	= 6,
	BYRP_IMG_FMT_MAX
};

struct byrp_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

int byrp_hw_s_reset(void __iomem *base);
void byrp_hw_s_init(void __iomem *base, u32 set_id);
void byrp_hw_s_dtp(void __iomem *base, u32 set_id, bool enable, u32 width, u32 height);
unsigned int byrp_hw_is_occurred(unsigned int status, enum byrp_event_type type);
int byrp_hw_wait_idle(void __iomem *base);
void byrp_hw_dump(void __iomem *base);
void byrp_hw_s_core(void __iomem *base, u32 num_buffers, u32 set_id, struct byrp_param_set *param_set);
void byrp_hw_s_cinfifo(void __iomem *base, u32 set_id, bool enable);
void byrp_hw_s_coutfifo(void __iomem *base, u32 set_id, bool enable);
void byrp_hw_s_common(void __iomem *base, u32 set_id, struct byrp_param_set *param_set, u32 bit_in);
void byrp_hw_s_line_row(void __iomem *base, u32 set_id, u32 fps, u32 height, bool fast_ae_en);
void byrp_hw_s_path(void __iomem *base, u32 set_id, bool mpc_en);
void byrp_hw_s_int_mask(void __iomem *base, u32 set_id);
void byrp_hw_s_secure_id(void __iomem *base, u32 set_id);
void byrp_hw_s_otf_hdr(void __iomem *base, u32 set_id);
void byrp_hw_s_fro(void __iomem *base, u32 num_buffers, u32 set_id);
void byrp_hw_dma_dump(struct is_common_dma *dma);
int byrp_hw_s_rdma_init(struct is_common_dma *dma, struct byrp_param_set *param_set,
	u32 enable, struct is_crop *dma_crop_cfg, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size);
int byrp_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void byrp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id);
int byrp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size,
	u32 image_addr_offset, u32 header_addr_offset);
int byrp_hw_s_wdma_init(struct is_common_dma *dma, struct byrp_param_set *param_set,
	void __iomem *base, u32 set_id,
	u32 enable, u32 in_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size);
int byrp_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void byrp_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id);
int byrp_hw_s_wdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers, int buf_idx,
	u32 comp_sbwc_en, u32 payload_size, u32 image_addr_offset, u32 header_addr_offset);
int byrp_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type);
void byrp_hw_s_cmdq(void __iomem *base, u32 set_id);
void byrp_hw_s_corex_init(void __iomem *base, bool enable);
unsigned int byrp_hw_g_int0_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int0_mask(void __iomem *base);
unsigned int byrp_hw_g_int1_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int1_mask(void __iomem *base);
void byrp_hw_s_corex_start(void __iomem *base, bool enable);
void byrp_hw_wait_corex_idle(void __iomem *base);
unsigned int byrp_hw_g_int0_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int0_mask(void __iomem *base);
unsigned int byrp_hw_g_int1_state(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int1_mask(void __iomem *base);
void byrp_hw_s_block_crc(void __iomem *base, u32 set_id, u32 seed);
void byrp_hw_s_bitmask(void __iomem *base, u32 set_id, bool enable, u32 bit_in, u32 bit_out);
void byrp_hw_s_pixel_order(void __iomem *base, u32 set_id, u32 pixel_order);
void byrp_hw_s_non_byr_pattern(void __iomem *base, u32 set_id, u32 non_byr_pattern);
void byrp_hw_s_mono_mode(void __iomem *base, u32 set_id, bool enable);
void byrp_hw_s_sdc_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_bcrop_size(void __iomem *base, u32 set_id, u32 bcrop_num, u32 x, u32 y, u32 width, u32 height);
void byrp_hw_s_grid_cfg(void __iomem *base, struct byrp_grid_cfg *cfg);
void byrp_hw_s_bpc_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_disparity_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_susp_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_ggc_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_chain_size(void __iomem *base, u32 set_id, u32 dma_width, u32 dma_height);
void byrp_hw_s_mpc_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_otf_hdr_size(void __iomem *base, bool enable, u32 set_id, u32 height);
void byrp_hw_s_block_bypass(void __iomem *base, u32 set_id);
u32 byrp_hw_g_reg_cnt(void);
#endif
