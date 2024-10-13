// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/debugfs.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-device.h"
#include "hardware/P0/dsp-hw-p0-system.h"
#include "hardware/P0/dsp-hw-p0-memory.h"
#include "dsp-hw-common-debug.h"
#include "hardware/P0/dsp-hw-p0-debug.h"

static int dsp_hw_debug_userdefined_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = debug->sys;

	seq_printf(file, "USERDEFINED : base_addr(%#x/%#x)/size(%u words)\n",
			(int)(sys->sfr_pa + DSP_P0_SM_USERDEFINED_BASE),
			DSP_P0_HW_BASE_ADDR + DSP_P0_SM_USERDEFINED_BASE,
			DSP_P0_SM_USERDEFINED_COUNT);

	seq_puts(file, "Command to change userdefined value\n");
	seq_puts(file, " echo {num} {0x(val)} > /d/dsp/hardware/userdefined\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_userdefined_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_userdefined_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_userdefined_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int num, val;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->sys->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u 0x%x", &num, &val);
	if (ret != 2) {
		dsp_err("Failed to get userdefined parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (num >= DSP_P0_SM_USERDEFINED_COUNT) {
		ret = -EINVAL;
		dsp_err("num(%u) of command is invalid(0 ~ %u)\n",
				num, DSP_P0_SM_USERDEFINED_COUNT - 1);
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev)) {
		dsp_info("USERDEFINED(%u) is changed from %#x to %#x\n",
				num,
				dsp_ctrl_sm_readl(DSP_P0_SM_USERDEFINED(num)),
				val);
		dsp_ctrl_sm_writel(DSP_P0_SM_USERDEFINED(num), val);
	} else {
		dsp_err("Failed to change userdefined as power is off\n");
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_userdefined_fops = {
	.open		= dsp_hw_debug_userdefined_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_userdefined_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_p0_debug_probe(struct dsp_hw_debug *debug, void *sys,
		void *root)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_debug_probe(debug, sys, root);
	if (ret)
		goto p_err;

	debug->userdefined = debugfs_create_file("userdefined", 0640,
			debug->root, debug, &dsp_hw_debug_userdefined_fops);
	if (!debug->userdefined)
		dsp_warn("Failed to create userdefined debugfs file\n");

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_p0_debug_remove(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_hw_common_debug_remove(debug);
	dsp_leave();
}

static const struct dsp_hw_debug_ops p0_hw_debug_ops = {
	.probe		= dsp_hw_p0_debug_probe,
	.remove		= dsp_hw_p0_debug_remove,
};

int dsp_hw_p0_debug_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_HW_DEBUG,
			&p0_hw_debug_ops,
			sizeof(p0_hw_debug_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
