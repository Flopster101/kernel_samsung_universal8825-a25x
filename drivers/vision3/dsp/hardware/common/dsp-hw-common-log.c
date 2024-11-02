// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/version.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-log.h"

static void __dsp_hw_common_log_print(struct dsp_hw_log *log)
{
	int ret;
	char line[DSP_HW_LOG_LINE_SIZE];
	int count = 0;

	while (true) {
		/* Restricted from working too long */
		if (count == dsp_util_queue_read_count(log->queue))
			break;

		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[timer(%4d)] %s", readl(&log->queue->front), line);
		count++;
	}

	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_LOG_TIME));
}

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
static void dsp_hw_common_log_print(unsigned long data)
{
	struct dsp_hw_log *log;

	log = (struct dsp_hw_log *)data;
	__dsp_hw_common_log_print(log);
}
#else
static void dsp_hw_common_log_print(struct timer_list *t)
{
	struct dsp_hw_log *log;

	log = from_timer(log, t, timer);
	__dsp_hw_common_log_print(log);
}
#endif

void dsp_hw_common_log_flush(struct dsp_hw_log *log)
{
	int ret;
	char line[DSP_HW_LOG_LINE_SIZE];

	dsp_enter();
	if (!log->queue)
		return;

	while (true) {
		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[flush(%4d)] %s", readl(&log->queue->front), line);
	}
	dsp_leave();
}

int dsp_hw_common_log_start(struct dsp_hw_log *log)
{
	struct dsp_memory *mem;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	mem = &log->sys->memory;

	pmem = &mem->priv_mem[mem->shared_id.fw_log];
	dsp_util_queue_init(log->queue, DSP_HW_LOG_LINE_SIZE,
			pmem->size, (unsigned int)pmem->iova,
			(unsigned long long)pmem->kvaddr);

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	add_timer(&log->timer);
#else
	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_LOG_TIME));
#endif
	dsp_leave();
	return 0;
}

int dsp_hw_common_log_stop(struct dsp_hw_log *log)
{
	dsp_enter();
	del_timer_sync(&log->timer);
	log->ops->flush(log);
	log->queue = NULL;
	dsp_leave();
	return 0;
}

int dsp_hw_common_log_open(struct dsp_hw_log *log)
{
	dsp_check();
	return 0;
}

int dsp_hw_common_log_close(struct dsp_hw_log *log)
{
	dsp_check();
	return 0;
}

int dsp_hw_common_log_probe(struct dsp_hw_log *log, void *sys)
{
	dsp_enter();
	log->sys = sys;

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	init_timer(&log->timer);
	log->timer.expires = jiffies + msecs_to_jiffies(DSP_HW_LOG_TIME);
	log->timer.data = (unsigned long)log;
	log->timer.function = dsp_hw_common_log_print;
#else
	timer_setup(&log->timer, dsp_hw_common_log_print, 0);
#endif

	dsp_leave();
	return 0;
}

void dsp_hw_common_log_remove(struct dsp_hw_log *log)
{
	dsp_check();
}

int dsp_hw_common_log_set_ops(struct dsp_hw_log *log, const void *ops)
{
	dsp_enter();
	log->ops = ops;
	dsp_leave();
	return 0;
}
