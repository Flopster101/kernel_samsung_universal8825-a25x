/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_IMGLOADER_H__
#define __HW_COMMON_DSP_HW_COMMON_IMGLOADER_H__

#include "dsp-log.h"
#include "dsp-imgloader.h"

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int dsp_hw_common_imgloader_boot(struct dsp_imgloader *imgloader);
int dsp_hw_common_imgloader_shutdown(struct dsp_imgloader *imgloader);

int dsp_hw_common_imgloader_probe(struct dsp_imgloader *imgloader, void *sys);
void dsp_hw_common_imgloader_remove(struct dsp_imgloader *imgloader);

int dsp_hw_common_imgloader_set_ops(struct dsp_imgloader *imgloader,
		const void *ops);
#else
static inline int dsp_hw_common_imgloader_set_ops(
		struct dsp_imgloader *imgloader, const void *ops)
{
	dsp_check();
	imgloader->ops = ops;
	return 0;
}
#endif

#endif
