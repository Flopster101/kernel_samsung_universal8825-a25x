/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_MEMLOGGER_H__
#define __HW_COMMON_DSP_HW_COMMON_MEMLOGGER_H__

#include "dsp-log.h"
#include "dsp-memlogger.h"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
int dsp_hw_common_memlogger_probe(struct dsp_memlogger *memlogger, void *sys);
void dsp_hw_common_memlogger_remove(struct dsp_memlogger *memlogger);

int dsp_hw_common_memlogger_set_ops(struct dsp_memlogger *memlogger,
		const void *ops);
#else
static inline int dsp_hw_common_memlogger_set_ops(
		struct dsp_memlogger *memlogger, const void *ops)
{
	dsp_check();
	memlogger->ops = ops;
	return 0;
}
#endif

#endif
