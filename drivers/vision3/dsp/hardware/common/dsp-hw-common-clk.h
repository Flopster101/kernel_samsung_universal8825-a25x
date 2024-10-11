/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_CLK_H__
#define __HW_COMMON_DSP_HW_COMMON_CLK_H__

#include "dsp-clk.h"

void dsp_hw_common_clk_dump(struct dsp_clk *clk);
void dsp_hw_common_clk_user_dump(struct dsp_clk *clk, struct seq_file *file);
int dsp_hw_common_clk_enable(struct dsp_clk *clk);
int dsp_hw_common_clk_disable(struct dsp_clk *clk);

int dsp_hw_common_clk_open(struct dsp_clk *clk);
int dsp_hw_common_clk_close(struct dsp_clk *clk);
int dsp_hw_common_clk_probe(struct dsp_clk *clk, void *sys);
void dsp_hw_common_clk_remove(struct dsp_clk *clk);

int dsp_hw_common_clk_set_ops(struct dsp_clk *clk, const void *ops);

#endif
