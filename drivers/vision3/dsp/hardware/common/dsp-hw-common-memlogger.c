// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-memlogger.h"

static int dsp_hw_common_memlogger_log_status_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_memlogger_log_level_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_memlogger_log_enable_notify(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_memlogger_file_ops_completed(struct memlog_obj *obj,
		unsigned int flags)
{
	dsp_check();
	return 0;
}

int dsp_hw_common_memlogger_probe(struct dsp_memlogger *memlogger, void *sys)
{
	int ret;
	struct memlog *desc;
	struct memlog_obj *file_obj, *obj;

	dsp_enter();
	memlogger->sys = sys;

	ret = memlog_register("DSP_HW", memlogger->sys->dev, &desc);
	if (ret) {
		dsp_err("Failed to register memlogger(%d)\n", ret);
		goto p_err;
	}

	file_obj = memlog_alloc_file(desc, "log-fil", SZ_1M, SZ_1M, 100, 1);
	if (!file_obj) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc memlog log-file\n");
		goto p_err_alloc_file_obj;
	}

	obj = memlog_alloc_printf(desc, SZ_1M, file_obj, "log-mem",
			MEMLOG_UFLAG_CACHEABLE);
	if (!obj) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc memlog log-mem\n");
		goto p_err_alloc_obj;
	}

	memlogger->log_desc = desc;
	memlogger->log_file_obj = file_obj;
	memlogger->log_obj = obj;
	dsp_log_memlog_init(obj);

	desc->ops.log_status_notify = dsp_hw_common_memlogger_log_status_notify;
	desc->ops.log_level_notify = dsp_hw_common_memlogger_log_level_notify;
	desc->ops.log_enable_notify = dsp_hw_common_memlogger_log_enable_notify;
	desc->ops.file_ops_completed =
		dsp_hw_common_memlogger_file_ops_completed;

	dsp_leave();
	return 0;
p_err_alloc_obj:
	memlog_free(file_obj);
p_err_alloc_file_obj:
	memlog_unregister(desc);
p_err:
	return ret;
}

void dsp_hw_common_memlogger_remove(struct dsp_memlogger *memlogger)
{
	dsp_enter();
	memlog_free(memlogger->log_obj);
	memlog_free(memlogger->log_file_obj);
	memlog_unregister(memlogger->log_desc);
	dsp_leave();
}

int dsp_hw_common_memlogger_set_ops(struct dsp_memlogger *memlogger,
		const void *ops)
{
	dsp_enter();
	memlogger->ops = ops;
	dsp_leave();
	return 0;
}
