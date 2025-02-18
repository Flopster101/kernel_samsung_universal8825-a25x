/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef IS_HW_ORBMCH_H
#define IS_HW_ORBMCH_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-dvfs.h"

enum is_hw_orbmch_lib_mode {
	ORBMCH_USE_ONLY_DDK = 0,
	ORBMCH_USE_DRIVER = 1,
};

enum is_hw_orbmch_rdma_index {
	ORBMCH_RDMA_ORB_L1,
	ORBMCH_RDMA_ORB_L2,
	ORBMCH_RDMA_MCH_PREV_DESC,
	ORBMCH_RDMA_MCH_CUR_DESC,
	ORBMCH_RDMA_MAX
};

enum is_hw_orbmch_wdma_index {
	ORBMCH_WDMA_ORB_KEY,
	ORBMCH_WDMA_ORB_DESC,
	ORBMCH_WDMA_MCH_RESULT,
	ORBMCH_WDMA_MAX
};

enum is_hw_orbmch_irq_src {
	ORBMCH_INTR,
	ORBMCH_INTR_MAX,
};

struct is_hw_orbmch_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_hw_orbmch {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct orbmch_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[ORBMCH_RDMA_MAX];
	struct is_common_dma		wdma[ORBMCH_WDMA_MAX];
	struct is_orbmch_config		iq_config[IS_STREAM_COUNT];
	struct orbmch_data		data;
	u32				irq_state[ORBMCH_INTR_MAX];
	u32				instance;
	u32				timeout_time;	/* ORBMCH SW W/A: determine orbmch stuck if over timeout_time(ms) */
	unsigned long			state;
	atomic_t			strip_index;
	bool				invalid_flag[IS_STREAM_COUNT];	/* ORBMCH SW W/A: invalid flag for previous descriptor */

	struct is_hw_orbmch_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_orbmch_iq		cur_iq_set[COREX_MAX];
};

int is_hw_orbmch_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif /* IS_HW_ORBMCH_H */
