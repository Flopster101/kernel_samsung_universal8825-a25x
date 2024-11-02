/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_HARDWARE_H__
#define __HW_DSP_HARDWARE_H__

int dsp_hardware_set_ops(unsigned int dev_id, void *data);
int dsp_hardware_register_ops(void);

#endif
