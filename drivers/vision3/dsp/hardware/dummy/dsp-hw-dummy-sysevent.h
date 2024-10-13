/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DUMMY_DSP_HW_DUMMY_SYSEVETN_H__
#define __HW_DUMMY_DSP_HW_DUMMY_SYSEVETN_H__

#include "dsp-sysevent.h"

int dsp_hw_dummy_sysevent_get(struct dsp_sysevent *sysevent);
int dsp_hw_dummy_sysevent_put(struct dsp_sysevent *sysevent);

int dsp_hw_dummy_sysevent_probe(struct dsp_sysevent *sysevent, void *sys);
void dsp_hw_dummy_sysevent_remove(struct dsp_sysevent *sysevent);

int dsp_hw_dummy_sysevent_register_ops(unsigned int dev_id);

#endif
