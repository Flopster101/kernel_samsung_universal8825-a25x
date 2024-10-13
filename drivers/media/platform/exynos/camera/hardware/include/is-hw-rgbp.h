// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_RGBP_H
#define IS_HW_RGBP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

enum is_hw_rgbp_rdma_index {
	RGBP_RDMA_CL,
#if IS_ENABLED(CONFIG_RGBP_V1_1)
	RGBP_RDMA_RGB_EVEN,
	RGBP_RDMA_RGB_ODD,
#endif
	RGBP_RDMA_MAX,
};

enum is_hw_rgbp_wdma_index {
	RGBP_WDMA_HF,
	RGBP_WDMA_SF,
#if IS_ENABLED(CONFIG_RGBP_V1_1)
	RGBP_WDMA_Y,
	RGBP_WDMA_UV,
	RGBP_WDMA_RGB_EVEN,
	RGBP_WDMA_RGB_ODD,
#else
	RGBP_WDMA_R,
	RGBP_WDMA_G,
	RGBP_WDMA_B,
#endif
	RGBP_WDMA_MAX,
};

enum is_hw_rgbp_irq_src {
	RGBP_INTR0,
	RGBP_INTR1,
	RGBP_INTR_MAX,
};

enum is_rgbp_rgb_format {
	RGBP_DMA_FMT_RGB_RGBA8888 = 0,
	RGBP_DMA_FMT_RGB_ARGB8888,
	RGBP_DMA_FMT_RGB_RGBA1010102,
	RGBP_DMA_FMT_RGB_ARGB1010102,
	RGBP_DMA_FMT_RGB_RGBA16161616,
	RGBP_DMA_FMT_RGB_ARGB16161616,
	RGBP_DMA_FMT_RGB_BGRA8888 = 8,
	RGBP_DMA_FMT_RGB_ABGR8888,
	RGBP_DMA_FMT_RGB_BGRA1010102,
	RGBP_DMA_FMT_RGB_ABGR1010102,
	RGBP_DMA_FMT_RGB_BGRA16161616,
	RGBP_DMA_FMT_RGB_ABGR16161616,
};

struct is_hw_rgbp_sc_size {
	u32	input_h_size;
	u32	input_v_size;
	u32	dst_h_size;
	u32	dst_v_size;
};

struct is_hw_rgbp_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_rgbp_chain_set {
	u32	src_img_width;
	u32	src_img_height;
	u32	dst_img_width;
	u32	dst_img_height;
	u32	mux_dtp_sel;
	u32	mux_luma_sel;
	u32	wdma_rep_sel;
	u32	demux_dmsc_en;
	u32	demux_yuvsc_en;
	u32	satflg_en;
};

struct is_hw_rgbp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct rgbp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[RGBP_RDMA_MAX];
	struct is_common_dma		wdma[RGBP_WDMA_MAX];
	struct is_rgbp_config		config;
	u32				irq_state[RGBP_INTR_MAX];
	u32				instance;
	unsigned long			state;
	atomic_t			strip_index;

	struct is_hw_rgbp_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_rgbp_iq		cur_hw_iq_set[COREX_MAX];

};

#endif
