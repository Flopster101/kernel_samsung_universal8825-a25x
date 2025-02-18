// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-dummy-sysevent.h"

int dsp_hw_dummy_sysevent_get(struct dsp_sysevent *sysevent)
{
	dsp_check();
	return 0;
}

int dsp_hw_dummy_sysevent_put(struct dsp_sysevent *sysevent)
{
	dsp_check();
	return 0;
}

int dsp_hw_dummy_sysevent_probe(struct dsp_sysevent *sysevent, void *sys)
{
	dsp_check();
	return 0;
}

void dsp_hw_dummy_sysevent_remove(struct dsp_sysevent *sysevent)
{
	dsp_check();
}

static const struct dsp_sysevent_ops dummy_sysevent_ops = {
	.get		= dsp_hw_dummy_sysevent_get,
	.put		= dsp_hw_dummy_sysevent_put,

	.probe		= dsp_hw_dummy_sysevent_probe,
	.remove		= dsp_hw_dummy_sysevent_remove,
};

int dsp_hw_dummy_sysevent_register_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(dev_id, DSP_HW_OPS_SYSEVENT,
			&dummy_sysevent_ops,
			sizeof(dummy_sysevent_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	dsp_err("Failed to register dummy sysevent ops(%d)\n", ret);
	return ret;
}
