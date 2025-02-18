/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_LME_V2_0_H
#define IS_HW_LME_V2_0_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-common-enum.h"

#define dbg_lme(level, fmt, args...) \
	dbg_common((debug_lme) >= (level), \
		"[LME]", fmt, ##args)

#define IS_LME_BLOCK_PER_W_PIXELS	8
#define IS_LME_BLOCK_PER_H_PIXELS	4
#define IS_LME_MV_SIZE_PER_BLOCK	4 /* 2Bytes per each coordinates (x,y) */
#define IS_LME_SAD_SIZE_PER_BLOCK	2

enum is_hw_lme_lib_mode {
	LME_USE_ONLY_DDK = 0,
	LME_USE_DRIVER = 1,
};

enum is_hw_lme_rdma_index {
	LME_RDMA_CACHE_IN_0, /* video (0: prev, 1:cur) still (0: cur, 1: ref)*/
	LME_RDMA_CACHE_IN_1,
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	LME_RDMA_MBMV_IN,
#endif
	LME_RDMA_MAX
};

enum is_hw_lme_wdma_index {
	LME_WDMA_MV_OUT,
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	LME_WDMA_MBMV_OUT,
#endif
#if IS_ENABLED(CONFIG_LME_SAD)
	LME_WDMA_SAD_OUT,
#endif
	LME_WDMA_MAX
};

enum is_hw_lme_irq_src {
	LME_INTR,
	LME_INTR_MAX,
};

enum is_hw_lme_mode {
	LME_MODE_FUSION	= 0,
	LME_MODE_TNR	= 1,
};

struct is_hw_lme_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

enum is_hw_lme_event_id {
	LME_EVENT_FRAME_START = 1,
	LME_EVENT_FRAME_END,
	LME_EVENT_MOTION_DATA = 100,
};

struct is_hw_lme {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct lme_param_set		param_set[IS_STREAM_COUNT];
	struct is_lme_config		config[IS_STREAM_COUNT];
#if IS_ENABLED(CONFIG_LME_FRAME_END_EVENT_NOTIFICATION)
	struct is_lme_data		data;
#endif
	u32				irq_state[LME_INTR_MAX];
	u32				instance;
	unsigned long	state;
	atomic_t			strip_index;

	struct is_hw_lme_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_lme_iq		cur_hw_iq_set[COREX_MAX];

	u32				lme_mode;
	u32				mbmv0_dva[IS_MAX_PLANES]; /* mbmvInFwBuffAddr */
	u32				mbmv1_dva[IS_MAX_PLANES]; /* mbmvOutFwBuffAddr */
	u32				corex_set_id;

	struct tasklet_struct		end_tasklet;
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
