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

#ifndef IS_HW_YUVP_H
#define IS_HW_YUVP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

#define dbg_yuvp(level, fmt, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_YUVP)) >= (level), \
		"[YUVP]", fmt, ##args)

#define	YPP_UNIT_TEST	0

enum is_hw_yuvp_dma_index {
	YUVP_RDMA_Y,
	YUVP_RDMA_UV,
	YUVP_RDMA_SEG,
	YUVP_RDMA_DRC0,
	YUVP_RDMA_DRC1,
	YUVP_RDMA_CLAHE,
	YUVP_WDMA_Y,
	YUVP_WDMA_UV,
	YUVP_DMA_MAX
};

enum is_hw_yuvp_irq_src {
	YUVP0_INTR,
	YUVP1_INTR,
	YUVP_INTR_MAX,
};

struct is_hw_yuvp_iq {
	struct cr_set	*regs_yuvp;
	u32		size_yuvp;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

enum is_hw_yuvp_event_id {
     YUVP_EVENT_FRAME_START = 1,
     YUVP_EVENT_FRAME_END
};

struct is_hw_yuvp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct yuvp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[YUVP_DMA_MAX];
	struct is_yuvp_config		config;
	u32				irq_state[YUVP_INTR_MAX];
	u32				instance;
	unsigned long 			state;
	atomic_t			strip_index;

	struct is_hw_yuvp_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_yuvp_iq		cur_hw_iq_set[COREX_MAX];
};

int is_hw_yuvp_test(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
