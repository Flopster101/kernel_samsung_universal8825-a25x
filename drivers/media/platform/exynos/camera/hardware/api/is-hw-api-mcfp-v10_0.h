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

#ifndef IS_HW_API_MCFP_V10_0_H
#define IS_HW_API_MCFP_V10_0_H

#include "is-hw-mcfp.h"
#include "is-hw-common-dma.h"


#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

enum set_status {
	SET_SUCCESS,
	SET_ERROR,
};

enum mcfp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ERR,
};

struct mcfp_radial_cfg {
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

int mcfp_hw_s_reset(void __iomem *base);
void mcfp_hw_s_init(void __iomem *base);
u32 mcfp_hw_is_occured(unsigned int status, enum mcfp_event_type type);
int mcfp_hw_wait_idle(void __iomem *base);
void mcfp_hw_dump(void __iomem *base);
void mcfp_hw_s_core(void __iomem *base, u32 set_id);
void mcfp_hw_dma_dump(struct is_common_dma *dma);
void mcfp_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id);
int mcfp_hw_s_rdma_init(struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	u32 frame_width, u32 frame_height,
	u32 *sbwc_en, u32 *payload_size, u32 *strip_offset, struct is_mcfp_config *config);
int mcfp_hw_s_rdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset);
int mcfp_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input,
	u32 frame_width, u32 frame_height,
	u32 *sbwc_en, u32 *payload_size, u32 *strip_offset, u32 *header_offset,
	struct is_mcfp_config *config);
int mcfp_hw_s_wdma_addr(struct is_common_dma *dma, u32 *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset);
void mcfp_hw_s_cmdq_queue(void __iomem *base);
void mcfp_hw_s_cmdq_init(void __iomem *base);
void mcfp_hw_s_cmdq_enable(void __iomem *base, u32 enable);
unsigned int mcfp_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers, u32 irq_index, u32 *irq_state);
unsigned int mcfp_hw_g_int_mask(void __iomem *base, u32 irq_index);
void mcfp_hw_s_otf_input(void __iomem *base, u32 set_id, u32 enable);
void mcfp_hw_s_nr_otf_input(void __iomem *base, u32 set_id, u32 enable);
void mcfp_hw_s_otf_output(void __iomem *base, u32 set_id, u32 enable);
void mcfp_hw_s_input_size(void __iomem *base, u32 set_id, u32 width, u32 height);
int mcfp_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
int mcfp_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 input_id);
void mcfp_hw_s_block_bypass(void __iomem *base, u32 set_id);
void mcfp_hw_s_geomatch_size(void __iomem *base, u32 set_id,
				u32 frame_width, u32 dma_width, u32 height,
				bool strip_enable, u32 strip_start_pos,
				struct is_mcfp_config *mcfp_config);
void mcfp_hw_s_geomatch_bypass(void __iomem *base, u32 set_id, bool geomatch_bypass);
void mcfp_hw_s_mixer_size(void __iomem *base, u32 set_id,
		u32 frame_width, u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
		struct mcfp_radial_cfg *radial_cfg, struct is_mcfp_config *mcfp_config);
void mcfp_hw_s_crop_clean_img_otf(void __iomem *base, u32 set_id, u32 start_x, u32 width);
void mcfp_hw_s_crop_wgt_otf(void __iomem *base, u32 set_id, u32 start_x, u32 width);
void mcfp_hw_s_crop_clean_img_dma(void __iomem *base, u32 set_id,
					u32 start_x, u32 width, u32 height,
					bool bypass);
void mcfp_hw_s_crop_wgt_dma(void __iomem *base, u32 set_id,
					u32 start_x, u32 width, u32 height,
					bool bypass);
void mcfp_hw_s_img_bitshift(void __iomem *base, u32 set_id, u32 img_shift_bit);
void mcfp_hw_s_wgt_bitshift(void __iomem *base, u32 set_id, u32 wgt_shift_bit);
void mcfp_hw_s_crc(void __iomem *base, u32 seed);

u32 mcfp_hw_g_reg_cnt(void);
#endif
