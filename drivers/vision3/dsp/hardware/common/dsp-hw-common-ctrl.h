/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_COMMON_DSP_HW_COMMON_CTRL_H__
#define __HW_COMMON_DSP_HW_COMMON_CTRL_H__

#include "dsp-ctrl.h"

unsigned int dsp_hw_common_ctrl_remap_readl(struct dsp_ctrl *ctrl,
		unsigned int addr);
int dsp_hw_common_ctrl_remap_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val);
unsigned int dsp_hw_common_ctrl_sm_readl(struct dsp_ctrl *ctrl,
		unsigned int addr);
int dsp_hw_common_ctrl_sm_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val);
unsigned int dsp_hw_common_ctrl_offset_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset);
int dsp_hw_common_ctrl_offset_writel(struct dsp_ctrl *ctrl, unsigned int reg_id,
		unsigned int offset, int val);

void dsp_hw_common_ctrl_reg_print(struct dsp_ctrl *ctrl, unsigned int reg_id);
void dsp_hw_common_ctrl_dump(struct dsp_ctrl *ctrl);

void dsp_hw_common_ctrl_user_reg_print(struct dsp_ctrl *ctrl,
		struct seq_file *file, unsigned int reg_id);
void dsp_hw_common_ctrl_user_dump(struct dsp_ctrl *ctrl, struct seq_file *file);

int dsp_hw_common_ctrl_all_init(struct dsp_ctrl *ctrl);
int dsp_hw_common_ctrl_force_reset(struct dsp_ctrl *ctrl);

int dsp_hw_common_ctrl_open(struct dsp_ctrl *ctrl);
int dsp_hw_common_ctrl_close(struct dsp_ctrl *ctrl);
int dsp_hw_common_ctrl_probe(struct dsp_ctrl *ctrl, void *sys);
void dsp_hw_common_ctrl_remove(struct dsp_ctrl *ctrl);

int dsp_hw_common_ctrl_set_ops(struct dsp_ctrl *ctrl, const void *ops);

#endif
