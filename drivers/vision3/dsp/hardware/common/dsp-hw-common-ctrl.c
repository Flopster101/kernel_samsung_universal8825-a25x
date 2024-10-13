// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-reg.h"
#include "dsp-hw-common-ctrl.h"

static struct dsp_ctrl *static_ctrl;

unsigned int dsp_hw_common_ctrl_remap_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	int ret;
	void __iomem *regs;
	unsigned int val;

	dsp_enter();
	regs = devm_ioremap(ctrl->dev, addr, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap once(%#x, %d)\n", addr, ret);
		return 0xdead0010;
	}

	val = readl(regs);
	devm_iounmap(ctrl->dev, regs);
	dsp_leave();
	return val;
}

int dsp_hw_common_ctrl_remap_writel(struct dsp_ctrl *ctrl, unsigned int addr,
		int val)
{
	int ret;
	void __iomem *regs;

	dsp_enter();
	regs = devm_ioremap(ctrl->dev, addr, 0x4);
	if (IS_ERR(regs)) {
		ret = PTR_ERR(regs);
		dsp_err("Failed to remap once(%#x, %d)\n", addr, ret);
		return ret;
	}

	writel(val, regs);
	devm_iounmap(ctrl->dev, regs);
	dsp_leave();
	return 0;
}

unsigned int dsp_hw_common_ctrl_sm_readl(struct dsp_ctrl *ctrl,
		unsigned int addr)
{
	dsp_check();
	return readl(ctrl->sfr + addr);
}

int dsp_hw_common_ctrl_sm_writel(struct dsp_ctrl *ctrl,
		unsigned int addr, int val)
{
	dsp_enter();
	writel(val, ctrl->sfr + addr);
	dsp_leave();
	return 0;
}

unsigned int dsp_hw_common_ctrl_offset_readl(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset)
{
	unsigned int addr;

	dsp_check();
	if (reg_id >= ctrl->reg_count) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, ctrl->reg_count);
		return -EINVAL;
	}

	if (ctrl->reg[reg_id].flag & REG_SEC_DENY)
		return 0xdeadc000;
	else if (ctrl->reg[reg_id].flag & REG_WRONLY)
		return 0xdeadc001;

	addr = ctrl->reg[reg_id].base + ctrl->reg[reg_id].offset +
		offset * ctrl->reg[reg_id].interval;

	if (ctrl->reg[reg_id].flag & REG_SEC_R)
		return ctrl->ops->secure_readl(ctrl, addr);
	else
		return readl(ctrl->sfr + addr);
}

int dsp_hw_common_ctrl_offset_writel(struct dsp_ctrl *ctrl,
		unsigned int reg_id, unsigned int offset, int val)
{
	unsigned int addr;

	dsp_enter();
	if (reg_id >= ctrl->reg_count) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, ctrl->reg_count);
		return -EINVAL;
	}

	if (ctrl->reg[reg_id].flag & (REG_SEC_DENY | REG_WRONLY))
		return -EACCES;

	addr = ctrl->reg[reg_id].base + ctrl->reg[reg_id].offset +
		offset * ctrl->reg[reg_id].interval;

	if (ctrl->reg[reg_id].flag & REG_SEC_W)
		ctrl->ops->secure_writel(ctrl, addr, val);
	else
		writel(val, ctrl->sfr + addr);
	dsp_leave();
	return 0;
}

void dsp_hw_common_ctrl_reg_print(struct dsp_ctrl *ctrl, unsigned int reg_id)
{
	unsigned int addr, count;
	unsigned int reg_count, interval;

	dsp_enter();
	if (reg_id >= ctrl->reg_count) {
		dsp_warn("register id is invalid(%u/%u)\n",
				reg_id, ctrl->reg_count);
		return;
	}

	addr = ctrl->reg[reg_id].base + ctrl->reg[reg_id].offset;
	reg_count = ctrl->reg[reg_id].count;

	if (reg_count) {
		for (count = 0; count < reg_count; ++count) {
			interval = ctrl->reg[reg_id].interval;
			dsp_notice("[%4d][%5x][%4x][%2x][%40s] %8x\n",
					reg_id, addr + count * interval,
					ctrl->reg[reg_id].flag, count,
					ctrl->reg[reg_id].name,
					ctrl->ops->offset_readl(ctrl, reg_id,
						count));
		}
	} else {
		dsp_notice("[%4d][%5x][%4x][--][%40s] %8x\n",
				reg_id, addr, ctrl->reg[reg_id].flag,
				ctrl->reg[reg_id].name,
				ctrl->ops->offset_readl(ctrl, reg_id, 0));
	}
	dsp_leave();
}

void dsp_hw_common_ctrl_dump(struct dsp_ctrl *ctrl)
{
	int idx;

	dsp_enter();
	dsp_notice("register dump (count:%u)\n", ctrl->reg_count);
	for (idx = 0; idx < ctrl->reg_count; ++idx)
		ctrl->ops->reg_print(ctrl, idx);

	ctrl->ops->dhcp_dump(ctrl);
	ctrl->ops->userdefined_dump(ctrl);
	ctrl->ops->fw_info_dump(ctrl);
	ctrl->ops->pc_dump(ctrl);
	dsp_leave();
}

void dsp_hw_common_ctrl_user_reg_print(struct dsp_ctrl *ctrl,
		struct seq_file *file, unsigned int reg_id)
{
	unsigned int addr, count;
	unsigned int reg_count, interval;

	dsp_enter();
	if (reg_id >= ctrl->reg_count) {
		seq_printf(file, "register id is invalid(%u/%u)\n",
				reg_id, ctrl->reg_count);
		return;
	}

	addr = ctrl->reg[reg_id].base + ctrl->reg[reg_id].offset;
	reg_count = ctrl->reg[reg_id].count;

	if (reg_count) {
		for (count = 0; count < reg_count; ++count) {
			interval = ctrl->reg[reg_id].interval;
			seq_printf(file, "[%4d][%5x][%2x][%2x][%40s] %8x\n",
					reg_id, addr + count * interval,
					ctrl->reg[reg_id].flag, count,
					ctrl->reg[reg_id].name,
					ctrl->ops->offset_readl(ctrl, reg_id,
						count));
		}
	} else {
		seq_printf(file, "[%4d][%5x][%2x][--][%40s] %8x\n",
				reg_id, addr, ctrl->reg[reg_id].flag,
				ctrl->reg[reg_id].name,
				ctrl->ops->offset_readl(ctrl, reg_id, 0));
	}
	dsp_leave();
}

void dsp_hw_common_ctrl_user_dump(struct dsp_ctrl *ctrl, struct seq_file *file)
{
	int idx;

	dsp_enter();
	seq_printf(file, "register dump (count:%u)\n", ctrl->reg_count);
	for (idx = 0; idx < ctrl->reg_count; ++idx)
		ctrl->ops->user_reg_print(ctrl, file, idx);

	ctrl->ops->user_dhcp_dump(ctrl, file);
	ctrl->ops->user_userdefined_dump(ctrl, file);
	ctrl->ops->user_fw_info_dump(ctrl, file);
	ctrl->ops->user_pc_dump(ctrl, file);
	dsp_leave();
}

int dsp_hw_common_ctrl_all_init(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	ctrl->ops->common_init(ctrl);
	ctrl->ops->extra_init(ctrl);
	dsp_leave();
	return 0;
}

int dsp_hw_common_ctrl_force_reset(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	// TODO add force reset
	ctrl->ops->reset(ctrl);
	dsp_leave();
	return 0;
}

int dsp_hw_common_ctrl_open(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_ctrl_close(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_ctrl_probe(struct dsp_ctrl *ctrl, void *sys)
{

	dsp_enter();
	ctrl->sys = sys;
	ctrl->dev = ctrl->sys->dev;
	ctrl->sfr_pa = ctrl->sys->sfr_pa;
	ctrl->sfr = ctrl->sys->sfr;
	dsp_leave();
	return 0;
}

void dsp_hw_common_ctrl_remove(struct dsp_ctrl *ctrl)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_common_ctrl_set_ops(struct dsp_ctrl *ctrl, const void *ops)
{
	dsp_enter();
	ctrl->ops = ops;
	static_ctrl = ctrl;
	dsp_leave();
	return 0;
}

unsigned int dsp_ctrl_remap_readl(unsigned int addr)
{
	dsp_check();
	return static_ctrl->ops->remap_readl(static_ctrl, addr);
}

int dsp_ctrl_remap_writel(unsigned int addr, int val)
{
	dsp_check();
	return static_ctrl->ops->remap_writel(static_ctrl, addr, val);
}

unsigned int dsp_ctrl_sm_readl(unsigned int addr)
{
	dsp_check();
	return static_ctrl->ops->sm_readl(static_ctrl, addr);
}

int dsp_ctrl_sm_writel(unsigned int addr, int val)
{
	dsp_check();
	return static_ctrl->ops->sm_writel(static_ctrl, addr, val);
}

unsigned int dsp_ctrl_dhcp_readl(unsigned int addr)
{
	dsp_check();
	return static_ctrl->ops->dhcp_readl(static_ctrl, addr);
}

int dsp_ctrl_dhcp_writel(unsigned int addr, int val)
{
	dsp_check();
	return static_ctrl->ops->dhcp_writel(static_ctrl, addr, val);
}

unsigned int dsp_ctrl_offset_readl(unsigned int reg_id, unsigned int offset)
{
	dsp_check();
	return static_ctrl->ops->offset_readl(static_ctrl, reg_id, offset);
}

int dsp_ctrl_offset_writel(unsigned int reg_id, unsigned int offset, int val)
{
	dsp_check();
	return static_ctrl->ops->offset_writel(static_ctrl, reg_id, offset,
			val);
}

unsigned int dsp_ctrl_readl(unsigned int reg_id)
{
	dsp_check();
	return static_ctrl->ops->offset_readl(static_ctrl, reg_id, 0);
}

int dsp_ctrl_writel(unsigned int reg_id, int val)
{
	dsp_check();
	return static_ctrl->ops->offset_writel(static_ctrl, reg_id, 0, val);
}

void dsp_ctrl_reg_print(unsigned int reg_id)
{
	dsp_enter();
	static_ctrl->ops->reg_print(static_ctrl, reg_id);
	dsp_leave();
}

void dsp_ctrl_pc_dump(void)
{
	dsp_enter();
	static_ctrl->ops->pc_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_dhcp_dump(void)
{
	dsp_enter();
	static_ctrl->ops->dhcp_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_userdefined_dump(void)
{
	dsp_enter();
	static_ctrl->ops->userdefined_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_fw_info_dump(void)
{
	dsp_enter();
	static_ctrl->ops->fw_info_dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_dump(void)
{
	dsp_enter();
	static_ctrl->ops->dump(static_ctrl);
	dsp_leave();
}

void dsp_ctrl_user_reg_print(struct seq_file *file, unsigned int reg_id)
{
	dsp_enter();
	static_ctrl->ops->user_reg_print(static_ctrl, file, reg_id);
	dsp_leave();
}

void dsp_ctrl_user_pc_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_pc_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_dhcp_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_dhcp_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_userdefined_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_userdefined_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_fw_info_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_fw_info_dump(static_ctrl, file);
	dsp_leave();
}

void dsp_ctrl_user_dump(struct seq_file *file)
{
	dsp_enter();
	static_ctrl->ops->user_dump(static_ctrl, file);
	dsp_leave();
}
