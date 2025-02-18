/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_HW_DUMP_H__
#define __HW_DSP_HW_DUMP_H__

#include "dsp-kernel.h"
#include "dsp-task.h"
#include "dsp-ctrl.h"
#include "dsp-mailbox.h"

struct dsp_system;
struct dsp_hw_dump;

struct dsp_hw_dump_ops {
	void (*set_value)(struct dsp_hw_dump *dump, unsigned int dump_value);
	void (*print_value)(struct dsp_hw_dump *dump);
	void (*print_status_user)(struct dsp_hw_dump *dump,
			struct seq_file *file);
	void (*ctrl)(struct dsp_hw_dump *dump);
	void (*ctrl_user)(struct dsp_hw_dump *dump, struct seq_file *file);

	void (*mailbox_pool_error)(struct dsp_hw_dump *dump, void *pool);
	void (*mailbox_pool_debug)(struct dsp_hw_dump *dump, void *pool);
	void (*task_count)(struct dsp_hw_dump *dump);
	void (*kernel)(struct dsp_hw_dump *dump);

	int (*probe)(struct dsp_hw_dump *dump, void *sys);
	void (*remove)(struct dsp_hw_dump *dump);
};

struct dsp_hw_dump {
	unsigned int			dump_count;
	unsigned int			default_value;
	unsigned int			dump_value;

	const struct dsp_hw_dump_ops	*ops;
	struct dsp_system		*sys;
};

#endif
