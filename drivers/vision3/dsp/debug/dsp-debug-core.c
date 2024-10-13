// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 */

#include <linux/debugfs.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "dsp-system.h"
#include "dsp-hw-debug.h"
#include "dsp-debug-core.h"

static struct dsp_device *static_dspdev;

static int dsp_debug_dbg_log_show(struct seq_file *file, void *unused)
{
	dsp_enter();
	seq_printf(file, "%u\n", dsp_log_get_debug_ctrl());
	dsp_leave();
	return 0;
}

static int dsp_debug_dbg_log_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_debug_dbg_log_show, inode->i_private);
}

static ssize_t dsp_debug_dbg_log_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int debug_ctrl;

	dsp_enter();
	ret = kstrtouint_from_user(user_buf, count, 0, &debug_ctrl);
	if (ret) {
		dsp_err("Failed to get debug_log parameter(%d)\n", ret);
		goto p_err;
	}

	dsp_log_set_debug_ctrl(debug_ctrl);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_debug_dbg_log_fops = {
	.open		= dsp_debug_dbg_log_open,
	.read		= seq_read,
	.write		= dsp_debug_dbg_log_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

int dsp_debug_probe(struct dsp_debug *debug, void *data)
{
	int ret;
	struct dsp_system *sys;

	dsp_enter();
	static_dspdev = data;
	sys = &static_dspdev->system;

	debug->root = debugfs_create_dir("dsp", NULL);
	if (!debug->root) {
		ret = -EFAULT;
		dsp_err("Failed to create debug root file\n");
		goto p_err_root;
	}

	debug->debug_log = debugfs_create_file("debug_log", 0640, debug->root,
			debug, &dsp_debug_dbg_log_fops);
	if (!debug->debug_log)
		dsp_warn("Failed to create debug_log debugfs file\n");

	ret = sys->debug.ops->probe(&sys->debug, sys, debug->root);
	if (ret)
		goto p_err_hw_debug;

	dsp_leave();
	return 0;
p_err_hw_debug:
	debugfs_remove_recursive(debug->root);
p_err_root:
	return ret;
}

void dsp_debug_remove(struct dsp_debug *debug)
{
	struct dsp_system *sys;

	dsp_enter();
	sys = &static_dspdev->system;

	sys->debug.ops->remove(&sys->debug);
	debugfs_remove_recursive(debug->root);
	dsp_leave();
}
