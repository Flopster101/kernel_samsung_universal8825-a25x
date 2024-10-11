/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_INTERFACE_H__
#define __HW_DUMMY_DSP_HW_DUMMY_INTERFACE_H__

#include "dsp-interface.h"

int dsp_hw_dummy_interface_start(struct dsp_interface *itf);
int dsp_hw_dummy_interface_stop(struct dsp_interface *itf);

#endif
