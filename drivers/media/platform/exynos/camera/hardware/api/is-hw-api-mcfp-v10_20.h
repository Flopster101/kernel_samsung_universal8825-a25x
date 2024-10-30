/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef IS_HW_API_MCFP_V10_20_H
#define IS_HW_API_MCFP_V10_20_H

#include "is-hw-common-dma.h"
#include "is-hw-api-type.h"
#include "is-interface-ddk.h"

enum mcfp_event_type {
	MCFP_EVENT_FRAME_START,
	MCFP_EVENT_FRAME_END,
	MCFP_EVENT_ERR,
};

enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum mcfp_input_type {
	MCFP_INPUT_CINFIFO = 0,
	MCFP_INPUT_DMA = 1,
	MCFP_INPUT_STRGEN = 2,
	MCFP_INPUT_MAX
};

enum mcfp_rdma_id {
	MCFP_RDMA_CURR_BAYER = 0,
	MCFP_RDMA_PREV_MOTION0 = 1,
	MCFP_RDMA_PREV_MOTION1 = 2,
	MCFP_RDMA_PREV_WGT = 3,
	MCFP_RDMA_PREV_BAYER0,
	MCFP_RDMA_PREV_BAYER1,
	MCFP_RDMA_PREV_BAYER2,
	MCFP_RDMA_MAX,
};

enum mcfp_wdma_id {
	MCFP_WDMA_PREV_BAYER = 0,
	MCFP_WDMA_PREV_WGT,
	MCFP_WDMA_MAX,
};

unsigned int mcfp0_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en);
u32 mcfp0_hw_is_occurred(unsigned int status, u32 irq_id, enum mcfp_event_type type);
int mcfp0_hw_s_reset(void __iomem *base);
void mcfp0_hw_s_control_init(void __iomem *base, u32 set_id);
void mcfp1_hw_s_control_init(void __iomem *base, u32 set_id);

void mcfp0_hw_s_int_init(void __iomem *base);
int mcfp0_hw_create_rdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id);
int mcfp0_hw_create_wdma(struct is_common_dma *dma, void __iomem *base, u32 dma_id);

int mcfp0_hw_s_rdma(struct is_common_dma *dma, void __iomem *base, u32 set_id,
	struct param_dma_input *dma_input, u32 dma_en,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *addr, u32 num_buffers);
int mcfp0_hw_s_wdma(struct is_common_dma *dma, void __iomem *base, u32 set_id,
	struct param_dma_input *dma_input, u32 dma_en,
	struct param_stripe_input *stripe_input,
	struct is_isp_config *config,
	u32 *addr, u32 num_buffers);

void mcfp0_hw_s_cmdq_queue(void __iomem *base, u32 set_id);
void mcfp0_hw_s_enable(void __iomem *base, u32 set_id);
void mcfp1_hw_s_enable(void __iomem *base, u32 set_id);
void mcfp0_hw_s_corex_init(void __iomem *base, bool enable);
void mcfp1_hw_s_corex_init(void __iomem *base, bool enable);
void mcfp0_hw_s_module_bypass(void __iomem *base, u32 set_id);
void mcfp1_hw_s_module_bypass(void __iomem *base, u32 set_id);

void mcfp0_hw_s_control_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, u32 bayer_order,
	bool prev_sbwc_enable, bool enable);
void mcfp1_hw_s_control_config(void __iomem *base, u32 set_id,
	struct is_isp_config *config, u32 bayer_order,
	struct param_stripe_input *stripe_input,
	bool prev_sbwc_enable, bool enable);

void mcfp0_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_isp_config *config,
	struct param_stripe_input *stripe_input,
	u32 width, u32 height);
void mcfp1_hw_s_module_size(void __iomem *base, u32 set_id,
	struct is_hw_size_config *config,
	struct param_stripe_input *stripe_input);

/* FRO */
void mcfp0_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers);
void mcfp1_hw_s_fro(void __iomem *base, u32 set_id, u32 num_buffers);

/* CRC */
void mcfp0_hw_s_crc(void __iomem *base, u32 seed);
void mcfp1_hw_s_crc(void __iomem *base, u32 seed);
/* DUMP */
void mcfp0_hw_dump(void __iomem *base);
void mcfp1_hw_dump(void __iomem *base);
u32 mcfp0_hw_g_reg_cnt(void);
u32 mcfp1_hw_g_reg_cnt(void);
#endif /* IS_HW_API_MCFP_V10_20_H */
