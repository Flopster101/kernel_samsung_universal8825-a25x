// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-dummy-llc.h"

int dsp_hw_dummy_llc_get_by_name(struct dsp_llc *llc, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_put_by_name(struct dsp_llc *llc, unsigned char *name)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_get_by_id(struct dsp_llc *llc, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_put_by_id(struct dsp_llc *llc, unsigned int id)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_all_put(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_open(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_close(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_dummy_llc_probe(struct dsp_llc *llc, void *sys)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

void dsp_hw_dummy_llc_remove(struct dsp_llc *llc)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_llc_ops dummy_llc_ops = {
	.get_by_name	= dsp_hw_dummy_llc_get_by_name,
	.put_by_name	= dsp_hw_dummy_llc_put_by_name,
	.get_by_id	= dsp_hw_dummy_llc_get_by_id,
	.put_by_id	= dsp_hw_dummy_llc_put_by_id,
	.all_put	= dsp_hw_dummy_llc_all_put,

	.open		= dsp_hw_dummy_llc_open,
	.close		= dsp_hw_dummy_llc_close,
	.probe		= dsp_hw_dummy_llc_probe,
	.remove		= dsp_hw_dummy_llc_remove,
};

int dsp_hw_dummy_llc_register_ops(unsigned int dev_id)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(dev_id, DSP_HW_OPS_LLC, &dummy_llc_ops,
			sizeof(dummy_llc_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	dsp_err("Failed to register dummy llc ops(%d)\n", ret);
	return ret;
}
