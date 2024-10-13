/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_HW_LOG_H__
#define __HW_DSP_HW_LOG_H__

#include "dsp-util.h"

#define DSP_HW_LOG_LINE_SIZE		(128)
#define DSP_HW_LOG_TIME			(1)

struct dsp_system;
struct dsp_hw_log;

struct dsp_hw_log_ops {
	void (*flush)(struct dsp_hw_log *log);
	int (*start)(struct dsp_hw_log *log);
	int (*stop)(struct dsp_hw_log *log);

	int (*open)(struct dsp_hw_log *log);
	int (*close)(struct dsp_hw_log *log);
	int (*probe)(struct dsp_hw_log *log, void *sys);
	void (*remove)(struct dsp_hw_log *log);
};

struct dsp_hw_log {
	struct timer_list		timer;
	struct dsp_util_queue		*queue;

	const struct dsp_hw_log_ops	*ops;
	struct dsp_system		*sys;
};

#endif
