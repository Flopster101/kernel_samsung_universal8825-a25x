// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-p0-pm.h"
#include "dsp-hw-p0-llc.h"
#include "dsp-hw-common-governor.h"
#include "dsp-hw-p0-governor.h"

static int dsp_hw_p0_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req)
{
	dsp_enter();
	//TODO bring-up check
	//if (req->big_core_level >= 0)
	//	governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
	//			DSP_P0_EXTRA_DEVFREQ_CL2, req->big_core_level);

	//if (req->middle_core_level >= 0)
	//	governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
	//			DSP_P0_EXTRA_DEVFREQ_CL1,
	//			req->middle_core_level);

	//if (req->little_core_level >= 0)
	//	governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
	//			DSP_P0_EXTRA_DEVFREQ_CL0,
	//			req->little_core_level);

	if (req->mif_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_P0_EXTRA_DEVFREQ_MIF, req->mif_level);

	if (req->int_level >= 0)
		governor->sys->pm.ops->update_extra_devfreq(&governor->sys->pm,
				DSP_P0_EXTRA_DEVFREQ_INT, req->int_level);

	if (req->mo_scenario_id == DSP_GOVERNOR_DEFAULT)
		governor->sys->bus.ops->mo_all_put(&governor->sys->bus);
	else if (req->mo_scenario_id >= 0)
		governor->sys->bus.ops->mo_get_by_id(&governor->sys->bus,
				req->mo_scenario_id);

	if (req->llc_scenario_id == DSP_GOVERNOR_DEFAULT)
		governor->sys->llc.ops->all_put(&governor->sys->llc);
	else if (req->llc_scenario_id >= 0)
		governor->sys->llc.ops->get_by_id(&governor->sys->llc,
				req->llc_scenario_id);

	if (req->dvfs_ctrl == DSP_GOVERNOR_DEFAULT)
		governor->sys->pm.ops->dvfs_enable(&governor->sys->pm,
				req->dvfs_ctrl);
	else if (req->dvfs_ctrl >= 0)
		governor->sys->pm.ops->dvfs_disable(&governor->sys->pm,
				req->dvfs_ctrl);

	dsp_leave();
	return 0;
}

static int dsp_hw_p0_governor_probe(struct dsp_governor *governor, void *sys)
{
	int ret;

	dsp_enter();
	governor->handler = dsp_hw_p0_governor_request;

	ret = dsp_hw_common_governor_probe(governor, sys);
	if (ret) {
		dsp_err("Failed to probe common governor(%d)\n", ret);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_governor_ops p0_governor_ops = {
	.request	= dsp_hw_common_governor_request,
	.check_done	= dsp_hw_common_governor_check_done,
	.flush_work	= dsp_hw_common_governor_flush_work,

	.open		= dsp_hw_common_governor_open,
	.close		= dsp_hw_common_governor_close,
	.probe		= dsp_hw_p0_governor_probe,
	.remove		= dsp_hw_common_governor_remove,
};

int dsp_hw_p0_governor_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_GOVERNOR,
			&p0_governor_ops,
			sizeof(p0_governor_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
