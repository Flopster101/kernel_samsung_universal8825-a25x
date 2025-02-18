/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_GOVERNOR_H__
#define __HW_COMMON_DSP_HW_COMMON_GOVERNOR_H__

#include "dsp-governor.h"

int dsp_hw_common_governor_request(struct dsp_governor *governor,
		struct dsp_control_preset *req);
int dsp_hw_common_governor_check_done(struct dsp_governor *governor);
int dsp_hw_common_governor_flush_work(struct dsp_governor *governor);
int dsp_hw_common_governor_open(struct dsp_governor *governor);
int dsp_hw_common_governor_close(struct dsp_governor *governor);
int dsp_hw_common_governor_probe(struct dsp_governor *governor, void *sys);
void dsp_hw_common_governor_remove(struct dsp_governor *governor);

int dsp_hw_common_governor_set_ops(struct dsp_governor *governor,
		const void *ops);

#endif
