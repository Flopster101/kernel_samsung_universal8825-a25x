/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_SYSEVENT_H__
#define __HW_DSP_SYSEVENT_H__

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <soc/samsung/sysevent.h>
#include <soc/samsung/sysevent_notif.h>
#endif

struct dsp_system;
struct dsp_sysevent;

struct dsp_sysevent_ops {
	int (*get)(struct dsp_sysevent *sysevent);
	int (*put)(struct dsp_sysevent *sysevent);

	int (*probe)(struct dsp_sysevent *sysevent, void *sys);
	void (*remove)(struct dsp_sysevent *sysevent);
};

struct dsp_sysevent {
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
	struct sysevent_desc		desc;
	struct sysevent_device		*dev;
	bool				enabled;
#endif

	const struct dsp_sysevent_ops	*ops;
	struct dsp_system		*sys;
};

#endif
