/*
 * drivers/media/platform/exynos/mfc/mfc_llc.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if IS_ENABLED(CONFIG_MFC_USES_LLC)

#include <linux/module.h>
#include <soc/samsung/exynos-sci.h>

#include "mfc_llc.h"

static void __mfc_llc_alloc(struct mfc_core *core, enum exynos_sci_llc_region_index region,
		int enable, int way)
{
	/* region_index, enable, cache way */
	llc_region_alloc(region + core->id, enable, way);

	/* 1way = 512KB */
	mfc_core_debug(2, "[LLC] %s %s %dKB\n", enable ? "alloc" : "free",
			region == LLC_REGION_MFC0_INT ? "Internal" : "DPB", way * 512);
	MFC_TRACE_CORE("[LLC] %s %s %dKB\n", enable ? "alloc" : "free",
			region == LLC_REGION_MFC0_INT ? "Internal" : "DPB", way * 512);
}

void mfc_llc_enable(struct mfc_core *core)
{
	/* default 2way(1MB) per region */
	int way = 2;

	mfc_core_debug_enter();

	if (core->dev->debugfs.llc_disable)
		return;

	/* use 1way(512KB) per region when encoder only scenraio */
	if (core->dev->num_enc_inst && !core->dev->num_dec_inst)
		way = 1;

	__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 1, way);
	__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 1, way);

	core->llc_on_status = 1;
	mfc_core_info("[LLC] enabled\n");
	MFC_TRACE_CORE("[LLC] enabled\n");

	mfc_core_debug_leave();
}

void mfc_llc_disable(struct mfc_core *core)
{
	mfc_core_debug_enter();

	__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 0, 0);
	__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 0, 0);

	core->llc_on_status = 0;
	mfc_core_info("[LLC] disabled\n");
	MFC_TRACE_CORE("[LLC] disabled\n");

	mfc_core_debug_leave();
}

void mfc_llc_flush(struct mfc_core *core)
{
	mfc_core_debug_enter();

	if (core->dev->debugfs.llc_disable)
		return;

	if (!core->need_llc_flush)
		return;

	llc_flush(LLC_REGION_MFC0_INT + core->id);
	llc_flush(LLC_REGION_MFC0_DPB + core->id);

	mfc_core_debug(2, "[LLC] flushed\n");
	MFC_TRACE_CORE("[LLC] flushed\n");

	mfc_core_debug_leave();
}

void mfc_llc_update_size(struct mfc_core *core, bool update, enum mfc_inst_type type)
{
	int way;

	mfc_core_debug_enter();

	if (core->dev->debugfs.llc_disable)
		return;

	if (type == MFCINST_DECODER) {
		/* Decreases 8K decoder from 2way to 1way for each region */
		way = update ? 1 : 2;
		__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 0, 0);
		__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 0, 0);
		__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 1, way);
		__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 1, way);
	} else {
		/* Increases 8K encoder from 1way to 2way for INT region */
		way = update ? 2 : 1;
		__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 0, 0);
		__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 1, way);
	}

	mfc_core_debug(2, "[LLC] updated\n");
	MFC_TRACE_CORE("[LLC] updated\n");

	mfc_core_debug_leave();
}

void mfc_llc_handle_resol(struct mfc_core *core, struct mfc_ctx *ctx)
{
	if (IS_8K_RES(ctx)) {
		ctx->is_8k = 1;
		mfc_core_debug(2, "[LLC] is_8k %d\n", ctx->is_8k);
		mfc_llc_update_size(core, true, ctx->type);
	}

	if (core->num_inst == 1 && UNDER_1080P_RES(ctx)) {
		mfc_core_debug(2, "[LLC] disable LLC for under FHD (%dx%d)\n",
				ctx->crop_width, ctx->crop_height);
		mfc_llc_disable(core);
	}
}
#endif
