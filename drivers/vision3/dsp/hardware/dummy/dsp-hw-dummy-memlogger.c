// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-dummy-memlogger.h"

int dsp_hw_dummy_memlogger_probe(struct dsp_memlogger *memlogger, void *sys)
{
	dsp_check();
	return 0;
}

void dsp_hw_dummy_memlogger_remove(struct dsp_memlogger *memlogger)
{
	dsp_check();
}

static const struct dsp_memlogger_ops dummy_memlogger_ops = {
	.probe		= dsp_hw_dummy_memlogger_probe,
	.remove		= dsp_hw_dummy_memlogger_remove,
};

int dsp_hw_dummy_memlogger_register_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(dev_id, DSP_HW_OPS_MEMLOGGER,
			&dummy_memlogger_ops,
			sizeof(dummy_memlogger_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	dsp_err("Failed to register dummy memlogger ops(%d)\n", ret);
	return ret;
}
