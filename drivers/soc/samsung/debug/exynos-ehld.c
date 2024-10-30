// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Early Hard Lockup Detector
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu_pm.h>
#include <linux/sched/clock.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <soc/samsung/exynos-ehld.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-cpupm.h>

#include "coresight_regs.h"

//#define DEBUG

/* PPC Register */
#define	PPC_PMNC			(0x4)
#define	PPC_CNTENS			(0x8)
#define	PPC_CNTENC			(0xC)
#define PPC_INTENS			(0x10)
#define PPC_INTENC			(0x14)
#define PPC_FLAG			(0x18)
#define PPC_CNT_RESET			(0x2C)

#define PPC_CIG_CFG0			(0x1C)
#define PPC_CIG_CFG1			(0x20)
#define PPC_CIG_CFG2			(0x24)

#define PPC_PMCNT0_LOW			(0x34)
#define PPC_PMCNT0_HIGH			(0x4C)

#define PPC_PMCNT1_LOW			(0x38)
#define PPC_PMCNT1_HIGH			(0x50)

#define PPC_PMCNT2_LOW			(0x3C)
#define PPC_PMCNT2_HIGH			(0x54)

#define PPC_PMCNT3_LOW			(0x40)
#define PPC_PMCNT3_HIGH			(0x44)

#define PPC_CCNT_LOW			(0x48)
#define PPC_CCNT_HIGH			(0x58)

#define PPC_CMCNT0			(1 << 0)
#define PPC_CMCNT1			(1 << 1)
#define PPC_CMCNT2			(1 << 2)
#define PPC_CMCNT3			(1 << 3)
#define PPC_CCNT			(1 << 31)

#define EHLD_V3				(3)
#define UPDATE_INTERVAL_TYP		(10)
#define UPDATE_CONT			(1)

#ifdef DEBUG
#define ehld_info(f, str...) pr_info(str)
#define ehld_err(f, str...) pr_info(str)
#else
#define ehld_info(f, str...) do { if (f == 1) pr_info(str); } while (0)
#define ehld_err(f, str...)  do { if (f == 1) pr_info(str); } while (0)
#endif

#define MSB_MASKING		(0x0000FF0000000000)
#define MSB_PADDING		(0xFFFFFF0000000000)
#define DBG_UNLOCK(base)	\
	do { isb(); __raw_writel(OSLOCK_MAGIC, base + DBGLAR); } while (0)
#define DBG_LOCK(base)		\
	do { __raw_writel(0x1, base + DBGLAR); isb(); } while (0)
#define DBG_OS_UNLOCK(base)	\
	do { isb(); __raw_writel(0, base + DBGOSLAR); } while (0)
#define DBG_OS_LOCK(base)	\
	do { __raw_writel(0x1, base + DBGOSLAR); isb(); } while (0)

extern struct atomic_notifier_head hardlockup_notifier_list;
extern struct atomic_notifier_head hardlockup_handler_notifier_list;

struct ehld_dbgc {
	unsigned int			support;
	unsigned int			enabled;
	unsigned int			judge;
	unsigned int			interval;
	unsigned int			threshold;
	unsigned int			warn_count;
};

struct ehld_dev {
	struct device			*dev;
	struct ehld_dbgc		dbgc;
	unsigned int			cs_base;
	void __iomem			*reg_instret[2];
	void __iomem			*reg_instrun[2];
	struct cpumask			cpu_mask;
	int				sjtag_en;
	int				ver;
};

struct ehld_ctrl {
	struct ehld_data		data;
	void __iomem			*reg_instret;
	void __iomem			*reg_instrun;
	void __iomem			*dbg_base;
	raw_spinlock_t			lock;
	bool				in_c2;
};

static struct ehld_ctrl __percpu *ehld_ctrl;
static struct ehld_dev *ehld;

enum ehld_ipc_custom_cmd {
	CUSTOM_CMD_UPDATE_COUNT,
	CUSTOM_CMD_SET_THRESHOLD,
	CUSTOM_CMD_SET_JUDGE,
};

struct ehld_data *ehld_get_ctrl_data(int cpu)
{
	struct ehld_ctrl *ctrl = per_cpu_ptr(ehld_ctrl, cpu);

	return &ctrl->data;
}
EXPORT_SYMBOL(ehld_get_ctrl_data);

static void ehld_event_update(int cpu)
{
	struct ehld_ctrl *ctrl;
	struct ehld_data *data;
	unsigned long flags, count;

	if (!ehld->dbgc.enabled)
		return;

	ctrl = per_cpu_ptr(ehld_ctrl, cpu);

	raw_spin_lock_irqsave(&ctrl->lock, flags);
	data = &ctrl->data;
	count = ++data->data_ptr & (NUM_TRACE - 1);
	data->time[count] = cpu_clock(cpu);
	data->instret[count] = readl(ctrl->reg_instret +
				(PPC_PMCNT0_LOW + ((cpu % SZ_4) * SZ_4)));
	if (ehld->ver == EHLD_V3)
		data->instrun[count] = (readl(ctrl->reg_instrun +
				(PPC_PMCNT0_LOW + ((cpu % SZ_4) * SZ_4))) +
				ehld->dbgc.threshold);
	else
		data->instrun[count] = readl(ctrl->reg_instrun +
				(PPC_PMCNT0_LOW + ((cpu % SZ_4) * SZ_4)));
	if (cpu_is_offline(cpu)) {
		data->pmpcsr[count] = 0x5FF;
	} else if (ctrl->in_c2) {
		data->pmpcsr[count] = 0xC2;
	} else if (ehld->sjtag_en) {
		data->pmpcsr[count] = 0x8EC;
	} else {
		unsigned long long val;

		DBG_UNLOCK(ctrl->dbg_base + PMU_OFFSET);
		val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
		val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
		if (MSB_MASKING == (MSB_MASKING & val))
			val |= MSB_PADDING;
		data->pmpcsr[count] = val;
		DBG_LOCK(ctrl->dbg_base + PMU_OFFSET);
	}

	ehld_info(0, "%s: cpu%d: time:%llu, instret:0x%llx, instrun:0x%llx, pmpcsr:%x, c2:%ld\n",
			__func__, cpu,
			data->time[count],
			data->instret[count],
			data->instrun[count],
			data->pmpcsr[count],
			ctrl->in_c2);

	raw_spin_unlock_irqrestore(&ctrl->lock, flags);
}

void ehld_event_update_allcpu(void)
{
	int cpu;

	for_each_cpu(cpu, &ehld->cpu_mask)
		ehld_event_update(cpu);
}
EXPORT_SYMBOL(ehld_event_update_allcpu);

static void ehld_output_header(void)
{
	ehld_info(1, "--------------------------------------------------------------------------\n");
	ehld_info(1, "  Exynos Early Lockup Detector Information\n\n");
	if (ehld->ver == EHLD_V3)
		ehld_info(1, "  CPU NUM TIME             Instruction        Stall Count        PC\n\n");
	else
		ehld_info(1, "  CPU NUM TIME             INSTRET            INSTRUN            PC\n\n");
}

static void ehld_event_dump(int cpu, bool header)
{
	struct ehld_ctrl *ctrl;
	struct ehld_data *data;
	unsigned long flags, count;
	int i;
	char symname[KSYM_NAME_LEN];

	if (header)
		ehld_output_header();

	symname[KSYM_NAME_LEN - 1] = '\0';
	ctrl = per_cpu_ptr(ehld_ctrl, cpu);
	raw_spin_lock_irqsave(&ctrl->lock, flags);
	data = &ctrl->data;
	i = 0;
	do {
		count = ++data->data_ptr & (NUM_TRACE - 1);
		symname[KSYM_NAME_LEN - 1] = '\0';
		i++;

		if (sprint_symbol(symname, data->pmpcsr[count]) < 0)
			symname[0] = '\0';

		ehld_info(1, "    %01d  %02d %015llu  0x%015llx  0x%015llx  0x%016llx(%s)\n",
				cpu, i,
				(unsigned long long)data->time[count],
				(unsigned long long)data->instret[count],
				(unsigned long long)data->instrun[count],
				(unsigned long long)data->pmpcsr[count],
				symname);

		if (i >= NUM_TRACE)
			break;
	} while (1);
	raw_spin_unlock_irqrestore(&ctrl->lock, flags);
	ehld_info(1, "--------------------------------------------------------------------------\n");
}

void ehld_event_dump_allcpu(void)
{
	int cpu;

	ehld_output_header();

	for_each_cpu(cpu, &ehld->cpu_mask)
		ehld_event_dump(cpu, false);
}

void ehld_prepare_panic(void)
{
	ehld->dbgc.support = false;
}

void ehld_do_action(int cpu, unsigned int lockup_level)
{
	switch (lockup_level) {
	case EHLD_STAT_LOCKUP_WARN:
	case EHLD_STAT_LOCKUP_SW:
		ehld_event_dump(cpu, true);
		break;
	case EHLD_STAT_LOCKUP_HW:
		ehld_event_update(cpu);
		ehld_event_dump(cpu, true);
		panic("Watchdog detected hard LOCKUP on cpu %u by EHLD", cpu);
		break;
	}
}
EXPORT_SYMBOL(ehld_do_action);

static void ehld_event_update_dbgc(u32 interval, u32 cont)
{
	if (!ehld->dbgc.enabled || ehld->ver != EHLD_V3)
		return;

	if (!interval)
		interval = ehld->dbgc.interval;

	adv_tracer_ehld_send_cmd(CUSTOM_CMD_UPDATE_COUNT, interval, cont, 0, true);
}

static int ehld_hardlockup_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	ehld_event_update_allcpu();
	ehld_event_update_dbgc(UPDATE_INTERVAL_TYP, UPDATE_CONT);
	ehld_event_dump_allcpu();

	return 0;
}

static struct notifier_block ehld_hardlockup_block = {
	.notifier_call = ehld_hardlockup_handler,
};

/* This callback is called when every 4s in each cpu */
static int ehld_hardlockup_callback_handler(struct notifier_block *nb,
						unsigned long l, void *p)
{
	int *cpu = (int *)p;

	ehld_info(0, "%s: cpu%d\n", __func__, *cpu);

	ehld_event_update_allcpu();

	return 0;
}

static struct notifier_block ehld_hardlockup_handler_block = {
		.notifier_call = ehld_hardlockup_callback_handler,
};

static int ehld_panic_handler(struct notifier_block *nb,
				unsigned long l, void *p)
{
	ehld_event_update_allcpu();
	ehld_event_update_dbgc(UPDATE_INTERVAL_TYP, UPDATE_CONT);
	ehld_event_dump_allcpu();

	return 0;
}

static struct notifier_block ehld_panic_block = {
	.notifier_call = ehld_panic_handler,
};

static int ehld_c2_pm_notifier(struct notifier_block *self,
				unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();
	struct ehld_ctrl *ctrl = per_cpu_ptr(ehld_ctrl, cpu);
	unsigned long flags;

	raw_spin_lock_irqsave(&ctrl->lock, flags);

	switch (action) {
	case C2_ENTER:
		ctrl->in_c2 = true;
		break;
	case C2_EXIT:
		ctrl->in_c2 = false;
		break;
	case SICD_ENTER:
	case DSUPD_ENTER:
		adv_tracer_ehld_set_enable(false, false);
		ehld->dbgc.enabled = 0;
		ctrl->in_c2 = true;
		break;
	case SICD_EXIT:
	case DSUPD_EXIT:
		adv_tracer_ehld_set_enable(true, false);
		ehld->dbgc.enabled = 1;
		ctrl->in_c2 = false;
		break;
	}

	raw_spin_unlock_irqrestore(&ctrl->lock, flags);

	return NOTIFY_OK;
}

static struct notifier_block ehld_c2_pm_nb = {
	.notifier_call = ehld_c2_pm_notifier,
};

static void ehld_setup(void)
{
	/* register cpu pm notifier for C2 */
	exynos_cpupm_notifier_register(&ehld_c2_pm_nb);

	/* panic_notifier_list */
	atomic_notifier_chain_register(&panic_notifier_list,
					&ehld_panic_block);
	/* hardlockup_notifier_list */
	atomic_notifier_chain_register(&hardlockup_notifier_list,
					&ehld_hardlockup_block);
	/* hardlockup_handler_notifier_list */
	atomic_notifier_chain_register(&hardlockup_handler_notifier_list,
					&ehld_hardlockup_handler_block);

	ehld->sjtag_en = dbg_snapshot_get_sjtag_status();
}

static int ehld_init_dt_parse(struct device_node *np)
{
	struct device_node *child;
	int ret = 0, cpu = 0;
	unsigned int offset, base, val = 0;
	char name[SZ_16];
	struct ehld_ctrl *ctrl;

	if (of_property_read_u32(np, "cs_base", &base)) {
		ehld_info(1, "ehld: no coresight base address\n");
		return -EINVAL;
	}
	ehld->cs_base = base;

	of_property_read_u32(np, "version", &val);
	ehld_info(1, "ehld: version %x.0\n", val);
	ehld->ver = val;

	ehld->reg_instret[0] = of_iomap(np, 0);
	if (!ehld->reg_instret[0]) {
		ehld_info(1, "ehld: no instret reg[0] address\n");
		return -EINVAL;
	}

	ehld->reg_instret[1] = of_iomap(np, 1);
	if (!ehld->reg_instret[1]) {
		ehld_info(1, "ehld: no instret reg[1] address\n");
		return -EINVAL;
	}

	ehld->reg_instrun[0] = of_iomap(np, 2);
	if (!ehld->reg_instrun[0]) {
		ehld_info(1, "ehld: no instrun reg[0] address\n");
		return -EINVAL;
	}

	ehld->reg_instrun[1] = of_iomap(np, 3);
	if (!ehld->reg_instrun[1]) {
		ehld_info(1, "ehld: no instrun reg[1] address\n");
		return -EINVAL;
	}

	child = of_get_child_by_name(np, "dbgc");
	if (!child) {
		ehld_info(1, "ehld: dbgc dt is not supported\n");
		return -EINVAL;
	}

	if (!of_property_read_u32(child, "interval", &base)) {
		ehld->dbgc.interval = base;
		ehld_info(1, "Support dbgc interval:%u ms\n", base);
	}

	if (!of_property_read_u32(child, "judge", &base)) {
		ehld->dbgc.judge = base;
		ehld_info(1, "Support dbgc judge: 0x%X\n", base);
	}

	if (ehld->ver == EHLD_V3) {
		if (!of_property_read_u32(child, "threshold", &base)) {
			ehld->dbgc.threshold = base;
			ehld_info(1, "Support dbgc threshold: 0x%X\n", base);
		}
	} else {
		if (!of_property_read_u32(child, "warn-count", &base)) {
			ehld->dbgc.warn_count = base;
			ehld_info(1, "Support dbgc warnning count: %u\n", base);
		}
	}

	if (adv_tracer_ehld_init((void *)child) < 0) {
		ehld_info(1, "ehld: failed to init ehld ipc driver\n");
		return -EINVAL;
	}

	of_node_put(child);

	ehld_ctrl = alloc_percpu(struct ehld_ctrl);
	if (!ehld_ctrl) {
		ehld_err(1, "ehld: alloc_percpu is failed\n", cpu);
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		snprintf(name, sizeof(name), "cpu%d", cpu);
		child = of_get_child_by_name(np, (const char *)name);

		if (!child) {
			ehld_err(1, "ehld: no cpu dbg info - cpu%d\n", cpu);
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "dbg-offset", &offset);
		if (ret) {
			ehld_err(1, "ehld: no cpu dbg-offset info - cpu%d\n", cpu);
			return -EINVAL;
		}

		ctrl = per_cpu_ptr(ehld_ctrl, cpu);
		ctrl->dbg_base = ioremap(ehld->cs_base + offset, SZ_256K);

		if (!ctrl->dbg_base) {
			ehld_err(1, "ehld: fail ioremap for dbg_base of cpu%d\n", cpu);
			return -ENOMEM;
		}

		memset((void *)&ctrl->data, 0, sizeof(struct ehld_data));
		raw_spin_lock_init(&ctrl->lock);

		if (cpu < 4) {
			ctrl->reg_instret = ehld->reg_instret[0];
			ctrl->reg_instrun = ehld->reg_instrun[0];
		} else {
			ctrl->reg_instret = ehld->reg_instret[1];
			ctrl->reg_instrun = ehld->reg_instrun[1];
		}

		ehld_info(1, "ehld: cpu#%d, dbg_base:0x%x, total:0x%x, ioremap:0x%lx\n",
				cpu, offset, ehld->cs_base + offset,
				(unsigned long)ctrl->dbg_base);

		of_node_put(child);
	}

	return 0;
}

static int ehld_dbgc_setup(void)
{
	int val, cpu;
	u32 online_mask = 0;

	cpumask_copy(&ehld->cpu_mask, cpu_possible_mask);

	for_each_cpu(cpu, &ehld->cpu_mask)
		online_mask |= BIT(cpu);

	if (ehld->ver == EHLD_V3) {
		val = adv_tracer_ehld_set_init_val(ehld->dbgc.threshold,
							ehld->dbgc.judge,
							online_mask);
	} else {
		val = adv_tracer_ehld_set_init_val(ehld->dbgc.interval,
							(ehld->dbgc.warn_count << 16) |
							ehld->dbgc.judge,
							online_mask);
	}
	if (val < 0)
		return -EINVAL;

	val = adv_tracer_ehld_set_enable(true, true);
	if (val < 0)
		return -EINVAL;

	val = adv_tracer_ehld_get_enable();
	if (val >= 0)
		ehld->dbgc.enabled = val;
	else
		return -EINVAL;

	return 0;
}

static ssize_t ehld_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	long val;
	int ret;

	if ((kstrtol(buf, SZ_16, &val) != 0) && (val != 0 && val != 1))
		return -EINVAL;

	ret = adv_tracer_ehld_set_enable((int)val, true);
	if (ret < 0)
		return -EIO;

	ehld->dbgc.enabled = val;

	return size;
}

static ssize_t ehld_judge_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%sabled\n", ehld->dbgc.judge ? "en" : "dis");
}

static ssize_t ehld_judge_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	long val;
	int ret;

	if ((kstrtol(buf, SZ_16, &val) != 0) && (val != 0 && val != 1))
		return -EINVAL;

	ret = adv_tracer_ehld_send_cmd(CUSTOM_CMD_SET_JUDGE, val, 0, 0, false);
	if (ret < 0)
		return -EIO;

	ehld->dbgc.judge = val;

	return size;
}

static ssize_t ehld_warn_count_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%#X\n", ehld->dbgc.warn_count);
}

static ssize_t ehld_warn_count_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	long val;
	int ret;

	if (ehld->ver == EHLD_V3)
		return -EINVAL;

	if ((kstrtol(buf, SZ_16, &val) != 0) && (val != 0 && val != 1))
		return -EINVAL;

	ret = adv_tracer_ehld_set_warn_count(val);
	if (ret < 0)
		return -EIO;

	ehld->dbgc.warn_count = val;

	return size;
}

static ssize_t ehld_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%sabled\n", ehld->dbgc.enabled ? "en" : "dis");
}

static ssize_t ehld_threshold_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	unsigned long val;
	int ret = 0;

	if (ehld->ver != EHLD_V3)
		return -EINVAL;

	if (kstrtoul(buf, SZ_16, &val) != 0 && val != (u32) val)
		return -EINVAL;

	ret = adv_tracer_ehld_send_cmd(CUSTOM_CMD_SET_THRESHOLD, val, 0, 0, false);
	if (ret < 0)
		return -EIO;

	ehld->dbgc.threshold = val;

	return size;
}

static ssize_t ehld_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%#X\n", ehld->dbgc.threshold);
}

static DEVICE_ATTR_RW(ehld_enable);
static DEVICE_ATTR_RW(ehld_judge);
static DEVICE_ATTR_RW(ehld_threshold);
static DEVICE_ATTR_RW(ehld_warn_count);

static struct attribute *ehld_sysfs_attrs[] = {
	&dev_attr_ehld_enable.attr,
	&dev_attr_ehld_judge.attr,
	&dev_attr_ehld_threshold.attr,
	&dev_attr_ehld_warn_count.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ehld_sysfs);

static int ehld_probe(struct platform_device *pdev)
{
	int err;
	struct ehld_dev *edev;

	edev = devm_kzalloc(&pdev->dev,
				sizeof(struct ehld_dev), GFP_KERNEL);
	if (!edev)
		return -ENOMEM;

	edev->dev = &pdev->dev;

	platform_set_drvdata(pdev, edev);

	err = sysfs_create_groups(&pdev->dev.kobj, ehld_sysfs_groups);
	if (err)
		ehld_err(1, "ehld: fail to register sysfs\n");

	ehld = edev;

	err = ehld_init_dt_parse(pdev->dev.of_node);
	if (err) {
		ehld_err(1, "ehld: fail setup to parse dt:%d\n", err);
		return err;
	}

	ehld_setup();

	err = ehld_dbgc_setup();
	if (err) {
		ehld_err(1, "ehld: fail setup for ehld dbgc:%d\n", err);
		return err;
	}

	ehld_info(1, "ehld: success to initialize\n");
	return 0;
}

static int ehld_remove(struct platform_device *pdev)
{
	return 0;
}

static int ehld_suspend(struct device *dev)
{
	int val = 0;

	if (ehld->dbgc.enabled)
		val = adv_tracer_ehld_set_enable(false, true);

	if (val < 0)
		ehld_err(1, "ehld: failed to suspend:%d\n", val);
	else
		ehld->dbgc.enabled = 0;

	return 0;
}

static int ehld_resume(struct device *dev)
{
	int val = 0;

	if (!ehld->dbgc.enabled)
		val = adv_tracer_ehld_set_enable(true, true);

	if (val < 0)
		ehld_err(1, "ehld: failed to resume:%d\n", val);
	else
		ehld->dbgc.enabled = 1;

	return 0;
}

static const struct dev_pm_ops ehld_pm_ops = {
	.suspend = ehld_suspend,
	.resume = ehld_resume,
};

static const struct of_device_id ehld_dt_match[] = {
	{ .compatible	= "samsung,exynos-ehld", },
	{},
};
MODULE_DEVICE_TABLE(of, ehld_dt_match);

static struct platform_driver ehld_driver = {
	.probe = ehld_probe,
	.remove = ehld_remove,
	.driver = {
		.name = "ehld",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ehld_dt_match),
		.pm = &ehld_pm_ops,
	},
};
module_platform_driver(ehld_driver);

MODULE_DESCRIPTION("Samsung Early Hard Lockup Detector");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
