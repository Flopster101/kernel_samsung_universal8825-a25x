/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_SYSEVETN_H__
#define __HW_COMMON_DSP_HW_COMMON_SYSEVETN_H__

#include "dsp-log.h"
#include "dsp-sysevent.h"

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
int dsp_hw_common_sysevent_get(struct dsp_sysevent *sysevent);
int dsp_hw_common_sysevent_put(struct dsp_sysevent *sysevent);

int dsp_hw_common_sysevent_probe(struct dsp_sysevent *sysevent, void *sys);
void dsp_hw_common_sysevent_remove(struct dsp_sysevent *sysevent);

int dsp_hw_common_sysevent_set_ops(struct dsp_sysevent *sysevent,
		const void *ops);
#else
static inline int dsp_hw_common_sysevent_set_ops(struct dsp_sysevent *sysevent,
		const void *ops)
{
	dsp_check();
	sysevent->ops = ops;
	return 0;
}
#endif

#endif
