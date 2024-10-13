/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * lme HW control APIs
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_LME_V2_0_H
#define IS_HW_API_LME_V2_0_H

#include "is-hw-lme-v2.h"
#include "is-hw-common-dma.h"

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define QUEUE_MODE			(0)
#define SEQUENCE_MODE			(1)

#define HBLANK_CYCLE			(0x2D)

#define DMA_CLIENT_LME_MEMORY_MIT_WIDTH	32
#define DMA_CLIENT_LME_BYTE_ALIGNMENT	16 /* 16B(128b) align for perf */

#define UTL_ALIGN_UP(a, b)		(DIV_ROUND_UP(a, b) * (b))

#define GET_LME_COREX_OFFSET(SET_ID) \
	((SET_ID <= COREX_SET_D) ? (0x0 + ((SET_ID) * 0x2000)) : 0x8000)

extern int debug_lme;
enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum lme_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_ERR
};

int lme_hw_s_reset(void __iomem *base);
void lme_hw_s_init(void __iomem *base);
unsigned int lme_hw_is_occurred(unsigned int status, enum lme_event_type type);
int lme_hw_wait_idle(void __iomem *base, u32 set_id);
void lme_hw_dump(void __iomem *base);
void lme_hw_s_core(void __iomem *base, u32 set_id);
int lme_hw_s_rdma_init(void __iomem *base, struct lme_param_set *param_set, u32 enable, u32 id, u32 set_id);
int lme_hw_s_rdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id);
int lme_hw_s_wdma_init(void __iomem *base, struct lme_param_set *param_set, u32 enable, u32 id, u32 set_id);
int lme_hw_s_wdma_addr(void __iomem *base, u32 *addr, u32 id, u32 set_id);
void lme_hw_s_cmdq(void __iomem *base, u32 set_id);
int lme_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type);
void lme_hw_s_corex_init(void __iomem *base, bool enable);
void lme_hw_s_corex_start(void __iomem *base, bool enable);
void lme_hw_wait_corex_idle(void __iomem *base);
unsigned int lme_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers,
	u32 *irq_state, u32 set_id);
unsigned int lme_hw_g_int_mask(void __iomem *base, u32 set_id);
void lme_hw_s_cache(void __iomem *base, u32 set_id, bool enable, u32 prev_width, u32 cur_width);
void lme_hw_s_cache_size(void __iomem *base, u32 set_id, u32 prev_width,
	u32 prev_height, u32 cur_width, u32 cur_height);
void lme_hw_s_mvct_size(void __iomem *base, u32 set_id, u32 width, u32 height);
void lme_hw_s_mvct(void __iomem *base, u32 set_id);
void lme_hw_s_first_frame(void __iomem *base, u32 set_id, bool first_frame);
void lme_hw_s_crc(void __iomem *base, u32 seed);
struct is_reg *lme_hw_get_reg_struct(void);
unsigned int lme_hw_get_reg_cnt(void);
void lme_hw_s_block_bypass(void __iomem *base, u32 set_id, u32 width, u32 height);
unsigned int lme_hw_g_reg_cnt(void);
#endif
