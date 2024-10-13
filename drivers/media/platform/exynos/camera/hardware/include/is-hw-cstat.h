/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo Image Subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CSTAT_H
#define IS_HW_CSTAT_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-hw-api-cstat.h"

struct is_hw_cstat_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct is_hw_cstat_stat {
	struct cstat_pre pre;
	struct cstat_awb awb;
	struct cstat_ae ae;
	struct cstat_rgby rgby;
};

struct is_hw_cstat {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;
	struct cstat_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		dma[CSTAT_DMA_NUM];
	struct is_cstat_config		config;
	ulong				irq_state[CSTAT_INT_NUM];
	unsigned long			event_state;
	bool				hw_fro_en;

	bool				is_sfr_dumped;

	struct is_hw_cstat_iq		iq_set;
	struct is_hw_cstat_iq		cur_iq_set;
	struct is_hw_cstat_stat		stat[CSAT_STAT_BUF_NUM];

	enum cstat_input_path		input;
	struct cstat_size_cfg		size;

	struct tasklet_struct		end_tasklet;
	atomic_t			isr_run_count;
	wait_queue_head_t		isr_wait_queue;
};

#endif
