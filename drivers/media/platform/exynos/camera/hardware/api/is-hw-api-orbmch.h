/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * lme HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_ORBMCH_H
#define IS_HW_API_ORBMCH_H

#include "is-hw-common-dma.h"
#include "is-hw-orbmch.h"

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define dbg_orbmch(level, fmt, args...) \
	dbg_common(debug_orbmch >= (level), \
		"[ORBMCH]", fmt, ##args)

extern int debug_orbmch;
enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum orbmch_event_type {
	ORBMCH_INTR_FRAME_START,
	ORBMCH_INTR_FRAME_END,
	ORBMCH_INTR_COREX_END_0,
	ORBMCH_INTR_COREX_END_1,
	ORBMCH_INTR_ERR
};

struct is_orbmch_timeout_t {
	const u32 freq;
	const u32 process_time;
};

unsigned int orbmch_hw_g_process_time(int cur_lv);
unsigned int orbmch_hw_is_occurred(unsigned int status, enum orbmch_event_type type);
int orbmch_hw_s_reset(void __iomem *base);
void orbmch_hw_s_init(void __iomem *base);
void orbmch_hw_s_int_mask(void __iomem *base);
int orbmch_hw_wait_idle(void __iomem *base);
void orbmch_hw_s_core(void __iomem *base, u32 set_id);
int orbmch_hw_s_rdma_init(struct is_common_dma *dma, struct orbmch_param_set *param_set, u32 enable, u32 set_id);
int orbmch_hw_rdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id);
int orbmch_hw_s_rdma_addr(struct is_common_dma *dma, dma_addr_t addr, u32 set_id);
int orbmch_hw_s_wdma_init(struct is_common_dma *dma, struct orbmch_param_set *param_set, u32 enable, u32 set_id);
int orbmch_hw_wdma_create(struct is_common_dma *dma, void __iomem *base, u32 dma_id);
int orbmch_hw_s_wdma_addr(struct is_common_dma *dma, dma_addr_t addr, u32 set_id);
void orbmch_hw_s_enable(void __iomem *base);
int orbmch_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type);
void orbmch_hw_s_corex_init(void __iomem *base, bool enable);
void orbmch_hw_s_corex_start(void __iomem *base, bool enable);
void orbmch_hw_wait_corex_idle(void __iomem *base);
unsigned int orbmch_hw_g_int_status(void __iomem *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int orbmch_hw_g_int_mask(void __iomem *base);
void orbmch_hw_s_block_bypass(void __iomem *base, u32 set_id, u32 width, u32 height);
void orbmch_hw_s_frame_done(void __iomem *base, u32 instance, u32 fcount,
	struct orbmch_param_set *param_set, struct orbmch_data *data);
void orbmch_hw_dump(void __iomem *base);
void orbmch_hw_dma_dump(struct is_common_dma *dma);
struct is_reg *orbmch_hw_get_reg_struct(void);
unsigned int orbmch_hw_get_reg_cnt(void);
#endif
