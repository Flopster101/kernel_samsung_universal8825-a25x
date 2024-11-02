/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_DUMP_H__
#define __HW_COMMON_DSP_HW_COMMON_DUMP_H__

#include "dsp-hw-dump.h"

void dsp_hw_common_dump_set_value(struct dsp_hw_dump *dump,
		unsigned int dump_value);
void dsp_hw_common_dump_print_value(struct dsp_hw_dump *dump);

int dsp_hw_common_dump_probe(struct dsp_hw_dump *dump, void *sys);
void dsp_hw_common_dump_remove(struct dsp_hw_dump *dump);

int dsp_hw_common_dump_set_ops(struct dsp_hw_dump *dump, const void *ops);

#endif
