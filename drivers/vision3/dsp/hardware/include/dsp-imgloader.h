/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_IMGLOADER_H__
#define __HW_DSP_IMGLOADER_H__

#include <linux/types.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#endif

struct dsp_system;
struct dsp_imgloader;

struct dsp_imgloader_ops {
	int (*boot)(struct dsp_imgloader *imgloader);
	int (*shutdown)(struct dsp_imgloader *imgloader);

	int (*probe)(struct dsp_imgloader *imgloader, void *sys);
	void (*remove)(struct dsp_imgloader *imgloader);
};

struct dsp_imgloader {
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc		desc;
#endif

	const struct dsp_imgloader_ops	*ops;
	struct dsp_system		*sys;
};

#endif
