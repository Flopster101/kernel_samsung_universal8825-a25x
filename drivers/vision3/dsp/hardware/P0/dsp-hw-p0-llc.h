/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_P0_DSP_HW_P0_LLC_H__
#define __HW_P0_DSP_HW_P0_LLC_H__

#include "dsp-llc.h"

enum dsp_p0_llc_scenario_id {
	DSP_P0_LLC_SCENARIO_COUNT,
};

int dsp_hw_p0_llc_register_ops(void);

#endif
