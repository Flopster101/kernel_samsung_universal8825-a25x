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

#ifndef IS_HW_BYRP_H
#define IS_HW_BYRP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-hw-api-byrp.h"

#define __CEIL_DIV(x, a)	(((x)+(a)-1)/(a))

struct is_hw_byrp_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

enum is_hw_byrp_event_id {
	BYRP_EVENT_FRAME_START = 1,
	BYRP_EVENT_FRAME_END,
};

struct is_hw_byrp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct byrp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[BYRP_RDMA_MAX];
	struct is_common_dma		wdma[BYRP_WDMA_MAX];
	struct is_byrp_config		config;
	u32				irq_state[BYRP_INTR_MAX];
	u32				instance;
	unsigned long			state;
	atomic_t			strip_index;

	struct is_hw_byrp_iq		iq_set[IS_STREAM_COUNT];
	struct is_hw_byrp_iq		cur_hw_iq_set[COREX_MAX];
};

void is_hw_byrp_dump(void);
int is_hw_byrp_test(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
