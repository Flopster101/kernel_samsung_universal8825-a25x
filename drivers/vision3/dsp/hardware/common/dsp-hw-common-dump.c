// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-dump.h"

static struct dsp_hw_dump *static_dump;

void dsp_hw_common_dump_set_value(struct dsp_hw_dump *dump,
		unsigned int dump_value)
{
	dsp_enter();
	dump->dump_value = dump_value;
	dsp_leave();
}

void dsp_hw_common_dump_print_value(struct dsp_hw_dump *dump)
{
	dsp_enter();
	dsp_info("current dump_value : 0x%x\n", dump->dump_value);
	dsp_leave();
}

int dsp_hw_common_dump_probe(struct dsp_hw_dump *dump, void *sys)
{
	dsp_enter();
	dump->sys = sys;
	dump->dump_value = dump->default_value;
	dsp_leave();
	return 0;
}

void dsp_hw_common_dump_remove(struct dsp_hw_dump *dump)
{
	dsp_check();
}

int dsp_hw_common_dump_set_ops(struct dsp_hw_dump *dump, const void *ops)
{
	dsp_enter();
	dump->ops = ops;
	static_dump = dump;
	dsp_leave();
	return 0;
}

void dsp_dump_set_value(unsigned int dump_value)
{
	dsp_check();
	static_dump->ops->set_value(static_dump, dump_value);
}

void dsp_dump_print_value(void)
{
	dsp_check();
	static_dump->ops->print_value(static_dump);
}

void dsp_dump_print_status_user(struct seq_file *file)
{
	dsp_check();
	static_dump->ops->print_status_user(static_dump, file);
}

void dsp_dump_ctrl(void)
{
	dsp_check();
	static_dump->ops->ctrl(static_dump);
}

void dsp_dump_ctrl_user(struct seq_file *file)
{
	dsp_check();
	static_dump->ops->ctrl_user(static_dump, file);
}

void dsp_dump_mailbox_pool_error(void *pool)
{
	dsp_check();
	static_dump->ops->mailbox_pool_error(static_dump, pool);
}

void dsp_dump_mailbox_pool_debug(void *pool)
{
	dsp_check();
	static_dump->ops->mailbox_pool_debug(static_dump, pool);
}

void dsp_dump_task_count(void)
{
	dsp_check();
	static_dump->ops->task_count(static_dump);
}

void dsp_dump_kernel(void)
{
	dsp_check();
	static_dump->ops->kernel(static_dump);
}
