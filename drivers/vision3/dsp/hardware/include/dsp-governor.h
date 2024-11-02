/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_GOVERNOR_H__
#define __HW_DSP_GOVERNOR_H__

#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/spinlock.h>

#include "dsp-control.h"

#define DSP_GOVERNOR_DEFAULT	(INT_MAX)

struct dsp_system;
struct dsp_governor;

typedef int (*request_handler_t)(struct dsp_governor *,
		struct dsp_control_preset *);

struct dsp_governor_ops {
	int (*request)(struct dsp_governor *governor,
			struct dsp_control_preset *req);
	int (*check_done)(struct dsp_governor *governor);
	int (*flush_work)(struct dsp_governor *governor);

	int (*open)(struct dsp_governor *governor);
	int (*close)(struct dsp_governor *governor);
	int (*probe)(struct dsp_governor *governor, void *sys);
	void (*remove)(struct dsp_governor *governor);
};

struct dsp_governor {
	struct kthread_worker		worker;
	struct task_struct		*task;

	struct kthread_work		work;
	spinlock_t			slock;
	int				async_status;
	struct dsp_control_preset	async_req;
	request_handler_t		handler;

	const struct dsp_governor_ops	*ops;
	struct dsp_system		*sys;
};

#endif
