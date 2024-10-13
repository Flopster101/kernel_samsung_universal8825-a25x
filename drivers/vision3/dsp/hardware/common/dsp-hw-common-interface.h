/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_INTERFACE_H__
#define __HW_COMMON_DSP_HW_COMMON_INTERFACE_H__

#include "dsp-interface.h"

int dsp_hw_common_interface_set_ops(struct dsp_interface *itf, const void *ops);

#endif
