/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_P0_DSP_HW_P0_PM_H__
#define __HW_P0_DSP_HW_P0_PM_H__

#include "dsp-pm.h"

enum dsp_p0_devfreq_id {
	DSP_P0_DEVFREQ_DNC,
	DSP_P0_DEVFREQ_DSP,
	DSP_P0_DEVFREQ_COUNT,
};

enum dsp_p0_extra_devfreq_id {
	//TODO
	//DSP_P0_EXTRA_DEVFREQ_CL2,
	//DSP_P0_EXTRA_DEVFREQ_CL1,
	//DSP_P0_EXTRA_DEVFREQ_CL0,
	DSP_P0_EXTRA_DEVFREQ_INT,
	DSP_P0_EXTRA_DEVFREQ_MIF,
	DSP_P0_EXTRA_DEVFREQ_COUNT,
};

int dsp_hw_p0_pm_register_ops(void);

#endif
