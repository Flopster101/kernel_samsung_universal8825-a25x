/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_MEMLOGGER_H__
#define __HW_DSP_MEMLOGGER_H__

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/memlogger.h>
#endif

struct dsp_system;
struct dsp_memlogger;

struct dsp_memlogger_ops {
	int (*probe)(struct dsp_memlogger *memlogger, void *sys);
	void (*remove)(struct dsp_memlogger *memlogger);
};

struct dsp_memlogger {
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	struct memlog			*log_desc;
	struct memlog_obj		*log_file_obj;
	struct memlog_obj		*log_obj;
#endif

	const struct dsp_memlogger_ops	*ops;
	struct dsp_system		*sys;
};

#endif
