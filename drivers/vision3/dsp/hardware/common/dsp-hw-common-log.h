/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_LOG_H__
#define __HW_COMMON_DSP_HW_COMMON_LOG_H__

#include "dsp-hw-log.h"

void dsp_hw_common_log_flush(struct dsp_hw_log *log);
int dsp_hw_common_log_start(struct dsp_hw_log *log);
int dsp_hw_common_log_stop(struct dsp_hw_log *log);

int dsp_hw_common_log_open(struct dsp_hw_log *log);
int dsp_hw_common_log_close(struct dsp_hw_log *log);
int dsp_hw_common_log_probe(struct dsp_hw_log *log, void *sys);
void dsp_hw_common_log_remove(struct dsp_hw_log *log);

int dsp_hw_common_log_set_ops(struct dsp_hw_log *log, const void *ops);

#endif
