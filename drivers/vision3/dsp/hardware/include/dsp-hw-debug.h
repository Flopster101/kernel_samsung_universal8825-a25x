/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_HW_DEBUG_H__
#define __HW_DSP_HW_DEBUG_H__

struct dsp_system;
struct dsp_hw_debug;

enum dsp_hw_debug_dbg_mode {
	DSP_DEBUG_MODE_BOOT_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_TASK_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_RESET_TIMEOUT_PANIC,
	DSP_DEBUG_MODE_NUM,
};

struct dsp_hw_debug_ops {
	int (*probe)(struct dsp_hw_debug *debug, void *sys, void *root);
	void (*remove)(struct dsp_hw_debug *debug);
};

struct dsp_hw_debug {
	struct dentry			*root;
	struct dentry			*power;
	struct dentry			*clk;
	struct dentry			*devfreq;
	struct dentry			*sfr;
	struct dentry			*mem;
	struct dentry			*fw_log;
	struct dentry			*wait_ctrl;
	struct dentry			*layer_range;
	struct dentry			*mailbox;
	struct dentry			*userdefined;
	struct dentry			*dump_value;
	struct dentry			*firmware_mode;
	struct dentry			*bus;
	struct dentry			*governor;
	struct dentry			*llc;
	struct dentry			*debug_mode;
	void				*sub_data;

	const struct dsp_hw_debug_ops	*ops;
	struct dsp_system		*sys;
};

#endif
