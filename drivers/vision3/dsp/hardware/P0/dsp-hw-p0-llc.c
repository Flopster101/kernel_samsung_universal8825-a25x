// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-common-llc.h"
#include "dsp-hw-p0-llc.h"

static int __dsp_hw_p0_llc_set_scen(struct dsp_llc *llc)
{
	dsp_check();
	return 0;
}

static int dsp_hw_p0_llc_probe(struct dsp_llc *llc, void *sys)
{
	int ret;

	dsp_enter();
	llc->scen_count = DSP_P0_LLC_SCENARIO_COUNT;

	ret = dsp_hw_common_llc_probe(llc, sys);
	if (ret) {
		dsp_err("Failed to probe common llc(%d)\n", ret);
		goto p_err;
	}

	ret = __dsp_hw_p0_llc_set_scen(llc);
	if (ret) {
		dsp_err("Failed to set llc scenario(%d)\n", ret);
		goto p_err_scen;
	}

	dsp_leave();
	return 0;
p_err_scen:
	dsp_hw_common_llc_remove(llc);
p_err:
	return ret;
}

static void __dsp_hw_p0_llc_unset_scen(struct dsp_llc *llc)
{
	dsp_check();
}

static void dsp_hw_p0_llc_remove(struct dsp_llc *llc)
{
	dsp_enter();
	__dsp_hw_p0_llc_unset_scen(llc);
	dsp_hw_common_llc_remove(llc);
	dsp_leave();
}

static const struct dsp_llc_ops p0_llc_ops = {
	.get_by_name	= dsp_hw_common_llc_get_by_name,
	.put_by_name	= dsp_hw_common_llc_put_by_name,
	.get_by_id	= dsp_hw_common_llc_get_by_id,
	.put_by_id	= dsp_hw_common_llc_put_by_id,
	.all_put	= dsp_hw_common_llc_all_put,

	.open		= dsp_hw_common_llc_open,
	.close		= dsp_hw_common_llc_close,
	.probe		= dsp_hw_p0_llc_probe,
	.remove		= dsp_hw_p0_llc_remove,
};

int dsp_hw_p0_llc_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_LLC,
			&p0_llc_ops, sizeof(p0_llc_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
