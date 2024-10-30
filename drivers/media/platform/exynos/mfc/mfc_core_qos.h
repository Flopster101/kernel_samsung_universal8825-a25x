/*
 * drivers/media/platform/exynos/mfc/mfc_core_qos.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_CORE_QOS_H
#define __MFC_CORE_QOS_H __FILE__

#include "base/mfc_common.h"

#define MB_COUNT_PER_UHD_FRAME		32400
#define MAX_FPS_PER_UHD_FRAME		120
#define MIN_BW_PER_SEC			1

#define MFC_DRV_TIME			500
#define MFC_MB_PER_TABLE		550000

enum {
	MFC_QOS_TABLE_TYPE_DEFAULT	= 0,
	MFC_QOS_TABLE_TYPE_ENCODER	= 1,
};

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
#define MFC_THROUGHPUT_OFFSET	(PM_QOS_MFC1_THROUGHPUT - PM_QOS_MFC_THROUGHPUT)
void mfc_core_perf_boost_enable(struct mfc_core *core);
void mfc_core_perf_boost_disable(struct mfc_core *core);
void mfc_core_qos_on(struct mfc_core *core, struct mfc_ctx *ctx);
void mfc_core_qos_off(struct mfc_core *core, struct mfc_ctx *ctx);
void mfc_core_qos_update(struct mfc_core *core, int on);
void mfc_core_qos_ctrl_worker(struct work_struct *work);
void mfc_core_qos_int_operate(struct mfc_core *core, int enable);
#else
#define mfc_core_perf_boost_enable(core)	do {} while (0)
#define mfc_core_perf_boost_disable(core)	do {} while (0)
#define mfc_core_qos_on(core, ctx)		do {} while (0)
#define mfc_core_qos_off(core, ctx)		do {} while (0)
#define mfc_core_qos_update(core, on)		do {} while (0)
#define mfc_core_qos_ctrl_worker(work)		do {} while (0)
#define mfc_core_qos_int_operate(core, enable)	do {} while (0)
#endif

void mfc_core_qos_set_portion(struct mfc_core *core, struct mfc_ctx *ctx);
void mfc_core_qos_get_portion(struct mfc_core *core, struct mfc_ctx *ctx);
bool mfc_core_qos_mb_calculate(struct mfc_core *core, struct mfc_core_ctx *core_ctx,
		unsigned int processing_cycle, unsigned int frame_type);
void mfc_core_qos_idle_worker(struct work_struct *work);
bool mfc_core_qos_idle_trigger(struct mfc_core *core, struct mfc_ctx *ctx);

static inline int __mfc_core_get_qos_steps(struct mfc_core *core, int table_type)
{
	if (table_type == MFC_QOS_TABLE_TYPE_ENCODER)
		return core->core_pdata->num_encoder_qos_steps;
	else
		return core->core_pdata->num_default_qos_steps;
}

static inline struct mfc_qos *__mfc_core_get_qos_table(struct mfc_core *core, int table_type)
{
	if (table_type == MFC_QOS_TABLE_TYPE_ENCODER)
		return core->core_pdata->encoder_qos_table;
	else
		return core->core_pdata->default_qos_table;
}

static inline void mfc_core_qos_get_disp_ratio(struct mfc_ctx *ctx, int dec_cnt, int disp_cnt)
{
	int delta;

	delta = dec_cnt - disp_cnt;
	ctx->disp_ratio = ((dec_cnt + delta) * 100) / disp_cnt;
	if (ctx->disp_ratio > 100)
		mfc_debug(2, "[QoS] need to more performance dec %d/disp %d, disp_ratio: x%d.%d\n",
				dec_cnt, disp_cnt,
				ctx->disp_ratio / 100, ctx->disp_ratio % 100);
}

#endif /* __MFC_CORE_QOS_H */
