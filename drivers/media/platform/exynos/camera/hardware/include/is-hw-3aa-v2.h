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

#ifndef IS_HW_3AA_H
#define IS_HW_3AA_H

#include "is-hw.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-hw-api-3aa.h"

const char *irq_name[] = {
	"3AA-0",
	"3AA-1",
	"3AA-ZSL",
	"3AA-STRP",
};

enum sram_config_type {
	SRAM_CFG_N = 0,
	SRAM_CFG_R,
	SRAM_CFG_MODE_CHANGE_R,
	SRAM_CFG_BLOCK,
	SRAM_CFG_S,
	SRAM_CFG_MAX
};

enum is_hw_3aa_debug {
	TAA_DEBUG_3AA0_DUMP, /* DEV_HW_3AA0 - DEV_HW_3AA0 */
	TAA_DEBUG_3AA1_DUMP, /* DEV_HW_3AA1 - DEV_HW_3AA0 */
	TAA_DEBUG_3AA2_DUMP, /* DEV_HW_3AA2 - DEV_HW_3AA0 */
	TAA_DEBUG_3AA_DTP,
	TAA_DEBUG_SET_REG,
	TAA_DEBUG_DUMP_HEX,
	TAA_DEBUG_DISABLE_THSTAT_PRE_WDMA,
	TAA_DEBUG_DISABLE_THSTAT_WDMA,
	TAA_DEBUG_DISABLE_RGBYHIST_WDMA,
	TAA_DEBUG_DISABLE_DRC_WDMA,
	TAA_DEBUG_DISABLE_ORBDS0_WDMA,
	TAA_DEBUG_DISABLE_ORBDS1_WDMA,
	TAA_DEBUG_DISABLE_FDPIG_WDMA,
	TAA_DEBUG_DISABLE_ZSL_WDMA,
	TAA_DEBUG_DISABLE_STRP_WDMA,
};

struct is_hw_3aa_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

struct taa_param_internal_set {
	struct param_dma_output		dma_output_thstat_pre;
	struct param_dma_output		dma_output_thstat;
	struct param_dma_output		dma_output_rgbyhist;
	struct param_dma_output		dma_output_orbds0;
	struct param_dma_output		dma_output_orbds1;

	u32				output_dva_thstat_pre[IS_MAX_PLANES];
	u32				output_dva_thstat[IS_MAX_PLANES];
	u32				output_dva_rgbyhist[IS_MAX_PLANES];
	u32				output_dva_orbds0[IS_MAX_PLANES];
	u32				output_dva_orbds1[IS_MAX_PLANES];
};

struct is_hw_3aa {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct is_lib_support		*lib_support;
	struct lib_interface_func	*lib_func;

	struct taa_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		dma[TAA_DMA_MAX];
	bool				hw_fro_en;

	atomic_t			event_state;

	/* for frame count management */
	atomic_t			start_fcount;
	atomic_t			end_fcount;

	/* for ddk interface */
	struct is_3aa_config		iq_config;
	struct is_hw_3aa_iq		iq_set;
	struct is_hw_3aa_iq		cur_iq_set;

	atomic_t			end_tasklet_fcount;
	struct tasklet_struct		end_tasklet;

	/* for statistics data */
	struct taa_param_internal_set	param_internal_set[IS_STREAM_COUNT];
	struct is_subdev		subdev[TAA_INTERNAL_BUF_MAX];

	atomic_t			isr_run_count;
	wait_queue_head_t		isr_wait_queue;
};

int is_hw_3aa_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
		struct is_interface_ischain *itfc, int id, const char *name);
#endif
