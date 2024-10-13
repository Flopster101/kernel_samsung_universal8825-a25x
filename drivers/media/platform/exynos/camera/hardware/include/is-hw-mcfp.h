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

#ifndef IS_HW_MCFP_H
#define IS_HW_MCFP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

enum is_hw_mcfp_rdma_index {
	MCFP_RDMA_CUR_IN_Y,
	MCFP_RDMA_CUR_IN_UV,
	MCFP_RDMA_PRE_MV,
	MCFP_RDMA_GEOMATCH_MV,
	MCFP_RDMA_MIXER_MV,
	MCFP_RDMA_CUR_DRC,
	MCFP_RDMA_PREV_DRC,
	MCFP_RDMA_SAT_FLAG,
	MCFP_RDMA_PREV_IN0_Y,
	MCFP_RDMA_PREV_IN0_UV,
	MCFP_RDMA_PREV_IN1_Y,
	MCFP_RDMA_PREV_IN1_UV,
	MCFP_RDMA_PREV_IN_WGT,
	MCFP_RDMA_MAX
};

enum is_hw_mcfp_wdma_index {
	MCFP_WDMA_PREV_OUT_Y,
	MCFP_WDMA_PREV_OUT_UV,
	MCFP_WDMA_PREV_OUT_WGT,
	MCFP_WDMA_MAX
};

enum is_hw_mcfp_irq_src {
	MCFP_INTR_0,
	MCFP_INTR_1,
	MCFP_INTR_MAX,
};

enum is_hw_tnr_mode {
	MCFP_TNR_MODE_PREPARE,
	MCFP_TNR_MODE_FIRST,
	MCFP_TNR_MODE_NORMAL,
	MCFP_TNR_MODE_FUSION,
};

struct is_hw_mcfp_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_hw_mcfp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct mcfp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[MCFP_RDMA_MAX];
	struct is_common_dma		wdma[MCFP_WDMA_MAX];
	struct is_mcfp_config		config[IS_STREAM_COUNT];
	u32				irq_state[MCFP_INTR_MAX];
	u32				instance;
	unsigned long			state;
	atomic_t			strip_index;

	struct is_hw_mcfp_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_mcfp_iq		cur_hw_iq_set[COREX_MAX];
};

#endif
