/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_MEMLOGGER_H__
#define __HW_DUMMY_DSP_HW_DUMMY_MEMLOGGER_H__

#include "dsp-memlogger.h"

int dsp_hw_dummy_memlogger_probe(struct dsp_memlogger *memlogger, void *sys);
void dsp_hw_dummy_memlogger_remove(struct dsp_memlogger *memlogger);

int dsp_hw_dummy_memlogger_register_ops(unsigned int dev_id);

#endif
