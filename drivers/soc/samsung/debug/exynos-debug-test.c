/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/nmi.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <asm-generic/io.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/acpm_ipc_ctrl.h>

#include "system-regs.h"
#include "debug-snapshot-local.h"

enum s2r_lock_stage {
	eS2R_NOLOCK = 0,
	eS2R_PM_SUSPEND_PREPARE,
	eS2R_DEV_SUSPEND_PREPARE,
	eS2R_DEV_SUSPEND,
	eS2R_DEV_SUSPEND_LATE,
	eS2R_DEV_SUSPEND_NOIRQ,
	eS2R_SYSCORE_SUSPEND,
	eS2R_SYSCORE_RESUME,
	eS2R_DEV_RESUME_NOIRQ,
	eS2R_DEV_RESUME_EARLY,
	eS2R_DEV_RESUME,
	eS2R_DEV_COMPLETE,
	eS2R_PM_POST_SUSPEND,
	eS2R_STAGE_CNT,
};

typedef void (*force_error_func)(char *arg);

struct debug_test_desc {
	int enabled;
	int nr_cpu;
	int nr_little_cpu;
	int nr_mid_cpu;
	int nr_big_cpu;
	int little_cpu_start;
	int mid_cpu_start;
	int big_cpu_start;
	int dram_init_bit;
	int scratch_offset;
	unsigned int ps_hold_control_offset;
	unsigned int *null_pointer_ui;
	int *null_pointer_si;
	void (*null_function)(void);
	struct dentry *debugfs_root;
	struct device *dev;
	spinlock_t debug_test_lock;
	struct mutex s2r_lock;
	const char *s2r_stage_name[eS2R_STAGE_CNT];
	enum s2r_lock_stage s2r_stage;
};

struct force_error_item {
	char errname[SZ_32];
	force_error_func errfunc;
};

static struct debug_test_desc exynos_debug_desc;

static const char *test_vector[] = {
	"KP",
	"SVC",
	"SFR 0x1ffffff0",
	"SFR 0x1ffffff0 0x12345678",
	"WP",
	"panic",
	"bug",
	"dabrt",
	"pabrt",
	"undef",
	"memcorrupt",
	"softlockup",
	"hardlockup 0",
	"hardlockup LITTLE",
	"hardlockup BIG",
	"spinlockup",
	"pcabort",
	"jumpzero",
	"writero",
	"danglingref",
	"dfree",
	"QDP",
	"spabort",
	"overflow",
	"cacheflush",
	"apmwdt",
	"arraydump",
	"halt",
	"scandump",
	"dramfail",
	"ecc",
};

/* timeout for dog bark/bite */
#define DELAY_TIME 30000

static void pull_down_other_cpus(void)
{
#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
	int cpu, ret;

	for (cpu = exynos_debug_desc.nr_cpu - 1; cpu > 0 ; cpu--) {
		ret = remove_cpu(cpu);
		if (ret)
			dev_crit(exynos_debug_desc.dev,
					"CORE%d ret: %x\n", cpu, ret);
	}
#endif
}

static int find_blank(char *arg)
{
	int i;

	/* if parameter is not one, a space between parameters is 0
	 * End of parameter is lf(10)
	 */
	for (i = 0; !isspace(arg[i]) && arg[i]; i++)
		continue;

	return i;
}

static void simulate_KP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	*exynos_debug_desc.null_pointer_ui = 0x0; /* SVACE: intended */
}

static void simulate_DP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	pull_down_other_cpus();
	dev_crit(exynos_debug_desc.dev, "%s() start to hanging\n", __func__);
	local_irq_disable();
	mdelay(DELAY_TIME);
	local_irq_enable();
	/* should not reach here */
}

static void simulate_QDP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	dbg_snapshot_expire_watchdog();
	mdelay(DELAY_TIME);
	/* should not reach here */
}

static void simulate_SVC(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("svc #0x0");
	/* should not reach here */
}

static void simulate_SFR(char *arg)
{
	int ret, index = 0;
	unsigned long reg, val;
	char *tmp, *tmparg;
	void __iomem *addr;

	dev_crit(exynos_debug_desc.dev, "%s() start\n", __func__);

	index = find_blank(arg);
	if (index > PAGE_SIZE)
		return;

	tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	memcpy(tmp, arg, index);
	tmp[index] = '\0';

	ret = kstrtoul(tmp, 16, &reg);
	addr = ioremap(reg, 0x10);
	if (!addr) {
		dev_crit(exynos_debug_desc.dev, "failed to remap 0x%lx, quit\n", reg);
		kfree(tmp);
		return;
	}
	dev_crit(exynos_debug_desc.dev, "1st parameter: 0x%lx\n", reg);

	tmparg = &arg[index + 1];
	index = find_blank(tmparg);
	if (index == 0) {
		dev_crit(exynos_debug_desc.dev, "there is no 2nd parameter\n");
		dev_crit(exynos_debug_desc.dev, "try to read 0x%lx\n", reg);

		ret = __raw_readl(addr);
		dev_crit(exynos_debug_desc.dev, "result : 0x%x\n", ret);

	} else {
		if (index > PAGE_SIZE) {
			kfree(tmp);
			return;
		}
		memcpy(tmp, tmparg, index);
		tmp[index] = '\0';
		ret = kstrtoul(tmp, 16, &val);
		dev_crit(exynos_debug_desc.dev, "2nd parameter: 0x%lx\n", val);
		dev_crit(exynos_debug_desc.dev, "try to write 0x%lx to 0x%lx\n", val, reg);

		__raw_writel(val, addr);
	}
	kfree(tmp);
	/* should not reach here */
}

static void simulate_WP(char *arg)
{
	unsigned int ps_hold_control;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	if (!exynos_debug_desc.ps_hold_control_offset)
		return;
	exynos_pmu_read(exynos_debug_desc.ps_hold_control_offset, &ps_hold_control);
	exynos_pmu_write(exynos_debug_desc.ps_hold_control_offset,
			ps_hold_control & 0xFFFFFEFF);
}

static void simulate_PANIC(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	panic("simulate_panic");
}

static void simulate_BUG(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	BUG();
}

static void simulate_WARN(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	WARN_ON(1);
}

static void simulate_DABRT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	*exynos_debug_desc.null_pointer_si = 0; /* SVACE: intended */
}

static void simulate_PABRT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s() call address=[%llx]\n", __func__,
			(unsigned long long)exynos_debug_desc.null_function);

	exynos_debug_desc.null_function(); /* SVACE: intended */
}

static void simulate_UNDEF(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm volatile(".word 0xe7f001f2\n\t");
	unreachable();
}

static void simulate_DFREE(char *arg)
{
	unsigned int *p;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!p)
		return;

	*p = 0x0;
	kfree(p);
	msleep(1000);
	kfree(p); /* SVACE: intended */
}

static void simulate_DREF(char *arg)
{
	unsigned int *p;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!p)
		return;

	kfree(p);
	*p = 0x1234; /* SVACE: intended */
}

static void simulate_MCRPT(char *arg)
{
	int *ptr;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	if (!ptr)
		return;

	*ptr++ = 4;
	*ptr = 2;
	panic("MEMORY CORRUPTION");
}

static void simulate_LOMEM(char *arg)
{
	int i = 0;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	dev_crit(exynos_debug_desc.dev, "Allocating memory until failure!\n");
	while (kmalloc(128 * 1024, GFP_KERNEL)) /* SVACE: intended */
		i++;
	dev_crit(exynos_debug_desc.dev, "Allocated %d KB!\n", i * 128);
}

static void simulate_SOFT_LOCKUP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	local_irq_disable();
	preempt_disable();
	local_irq_enable();
	asm("b .");
	preempt_enable();
}

static void simulate_HARD_LOCKUP_handler(void *info)
{
	unsigned long flags;

	dev_crit(exynos_debug_desc.dev, "Lockup CPU%d\n",
			raw_smp_processor_id());

	spin_lock_irqsave(&exynos_debug_desc.debug_test_lock, flags);
	asm("b .");
}

static void simulate_HARD_LOCKUP(char *arg)
{
	int cpu;
	int start = -1;
	int end;
	int curr_cpu;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	if (!arg) {
		start = 0;
		end = exynos_debug_desc.nr_cpu;

		curr_cpu = raw_smp_processor_id();

		for (cpu = start; cpu < end; cpu++) {
			if (cpu == curr_cpu)
				continue;
			smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
		}
		simulate_HARD_LOCKUP_handler(NULL);
	}

	if (!strncmp(arg, "LITTLE", strlen("LITTLE"))) {
		if (exynos_debug_desc.little_cpu_start < 0 ||
				exynos_debug_desc.nr_little_cpu < 0) {
			dev_info(exynos_debug_desc.dev, " no little cpu info\n");
			return;
		}
		start = exynos_debug_desc.little_cpu_start;
		end = start + exynos_debug_desc.nr_little_cpu - 1;
	} else if (!strncmp(arg, "MID", strlen("MID"))) {
		if (exynos_debug_desc.mid_cpu_start < 0 ||
					exynos_debug_desc.nr_mid_cpu < 0) {
			dev_info(exynos_debug_desc.dev, "no mid cpu info\n");
			return;
		}
		start = exynos_debug_desc.mid_cpu_start;
		end = start + exynos_debug_desc.nr_mid_cpu - 1;
	} else if (!strncmp(arg, "BIG", strlen("BIG"))) {
		if (exynos_debug_desc.big_cpu_start < 0 ||
					exynos_debug_desc.nr_big_cpu < 0) {
			dev_info(exynos_debug_desc.dev, "no big cpu info\n");
			return;
		}
		start = exynos_debug_desc.big_cpu_start;
		end = start + exynos_debug_desc.nr_big_cpu - 1;
	}

	if (start >= 0) {
		curr_cpu = raw_smp_processor_id();
		for (cpu = start; cpu <= end; cpu++) {
			if (cpu == curr_cpu)
				continue;
			smp_call_function_single(cpu,
					simulate_HARD_LOCKUP_handler, 0, 0);
		}
		if (curr_cpu >= start && curr_cpu <= end) {
			simulate_HARD_LOCKUP_handler(NULL);
		}
		return;
	}

	if (kstrtoint(arg, 10, &cpu)) {
		dev_err(exynos_debug_desc.dev, "%s() input is invalid\n", __func__);
		return;
	}

	if (cpu < 0 || cpu >= exynos_debug_desc.nr_cpu) {
		dev_info(exynos_debug_desc.dev, "%s() input is invalid\n", __func__);
		return;
	}
	smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
}

static void simulate_SPIN_LOCKUP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	spin_lock(&exynos_debug_desc.debug_test_lock);
	spin_lock(&exynos_debug_desc.debug_test_lock);
}

static void simulate_PC_ABORT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("add x30, x30, #0x1\n\t"
	    "ret");
}

static void simulate_SP_ABORT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("mov x29, #0xff00\n\t"
	    "mov sp, #0xff00\n\t"
	    "ret");
}

static void simulate_JUMP_ZERO(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("mov x0, #0x0\n\t"
	    "br x0");
}

static void simulate_UNALIGNED(char *arg)
{
	static u8 data[5] __aligned(4) = {1, 2, 3, 4, 5};
	u32 *p;
	u32 val = 0x12345678;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = (u32 *)(data + 1);
	dev_crit(exynos_debug_desc.dev, "data=[0x%llx] p=[0x%llx]\n",
				(unsigned long long)data, (unsigned long long)p);
	if (*p == 0)
		val = 0x87654321;
	*p = val;
	dev_crit(exynos_debug_desc.dev, "val = [0x%x] *p = [0x%x]\n", val, *p);
}

static void simulate_WRITE_RO(char *arg)
{
	unsigned long *ptr;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	ptr = (unsigned long *)simulate_WRITE_RO;
	*ptr ^= 0x12345678;
}

#define BUFFER_SIZE SZ_1K

static int recursive_loop(int remaining)
{
	char buf[BUFFER_SIZE];

	dev_crit(exynos_debug_desc.dev, "%s() remainig = [%d]\n", __func__, remaining);

	/* Make sure compiler does not optimize this away. */
	memset(buf, (remaining & 0xff) | 0x1, BUFFER_SIZE);
	if (remaining)
		return recursive_loop(remaining - 1);
	return 0;
}

static void simulate_OVERFLOW(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	recursive_loop(600);
}

static char *buffer[NR_CPUS];
static void simulate_CACHE_FLUSH_handler(void *info)
{
	unsigned long i = 0;
	unsigned int cpu = raw_smp_processor_id();
	u64 addr = virt_to_phys((void *)(buffer[cpu]));
	local_irq_disable();

	memset(buffer[cpu], 0x5A, PAGE_SIZE * 2);
	dbg_snapshot_set_debug_test_buffer_addr(addr, cpu);

	i = cpu << 16;
	asm volatile("mov x0, %0\n\t"
			"add x1, x0, #1\n\t"
			"add x2, x0, #2\n\t"
			"add x3, x0, #3\n\t"
			"add x4, x0, #4\n\t"
			"add x5, x0, #5\n\t"
			"add x6, x0, #6\n\t"
			"add x7, x0, #7\n\t"
			"add x8, x0, #8\n\t"
			"add x9, x0, #9\n\t"
			"add x10, x0, #10\n\t"
			"add x11, x0, #11\n\t"
			"add x12, x0, #12\n\t"
			"add x13, x0, #13\n\t"
			"add x14, x0, #14\n\t"
			"add x15, x0, #15\n\t"
			"add x16, x0, #16\n\t"
			"add x17, x0, #17\n\t"
			"add x18, x0, #18\n\t"
			"add x19, x0, #19\n\t"
			"add x20, x0, #20\n\t"
			"add x21, x0, #21\n\t"
			"add x22, x0, #22\n\t"
			"add x23, x0, #23\n\t"
			"add x24, x0, #24\n\t"
			"add x25, x0, #25\n\t"
			"add x26, x0, #26\n\t"
			"add x27, x0, #27\n\t"
			"add x28, x0, #28\n\t"
			"add x29, x0, #29\n\t"
			"add x30, x0, #30\n\t"
			"b ." : : "r" (i));
}

static void simulate_CACHE_FLUSH_ALL(void *info)
{
	cache_flush_all();
}

static void simulate_CACHE_FLUSH(char *arg)
{
	int cpu;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	for_each_possible_cpu(cpu) {
		dbg_snapshot_set_debug_test_buffer_addr(0, cpu);
		buffer[cpu] = kmalloc(PAGE_SIZE * 2, GFP_KERNEL);
		memset(buffer[cpu], 0x3B, PAGE_SIZE * 2);
	}

	smp_call_function(simulate_CACHE_FLUSH_ALL, NULL, 1);
	cache_flush_all();

	smp_call_function(simulate_CACHE_FLUSH_handler, NULL, 0);
	for_each_possible_cpu(cpu) {
		if (cpu == raw_smp_processor_id())
			continue;

		while (!dbg_snapshot_get_debug_test_buffer_addr(cpu));
		dev_crit(exynos_debug_desc.dev, "CPU %d STOPPING\n", cpu);
	}

	dbg_snapshot_expire_watchdog_timeout(500);
	simulate_CACHE_FLUSH_handler(NULL);
}

static void simulate_APM_WDT(char *arg)
{
	exynos_acpm_force_apm_wdt_reset();
	asm volatile("b .");
}

static void simulate_ARRAYDUMP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);
	dbg_snapshot_do_dpm_policy(GO_ARRAYDUMP_ID);
}

static void simulate_HALT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);
	dbg_snapshot_do_dpm_policy(GO_HALT_ID);
}

static void simulate_SCANDUMP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);
	dbg_snapshot_do_dpm_policy(GO_SCANDUMP_ID);
}

static void simulate_DRAMFAIL(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	if ((exynos_debug_desc.scratch_offset < 0) || (exynos_debug_desc.dram_init_bit < 0))
		return;

	exynos_pmu_update(exynos_debug_desc.scratch_offset, BIT(exynos_debug_desc.dram_init_bit),
			  BIT(exynos_debug_desc.dram_init_bit));

	dbg_snapshot_expire_watchdog();
	/* should not reach here */
}

static void simulate_ECC_INJECTION(void *info)
{
	unsigned long lev = *((unsigned long *)info);
	u64 count = 0x1000;
	u64 ctrl = 0x80000002;

	dev_info(exynos_debug_desc.dev,
			"CPU%d: Level%d: ECC error injection!\n",
			raw_smp_processor_id(), lev);

	write_ERRSELR_EL1(lev);
	asm volatile("msr S3_0_c5_c4_6, %0\n"
			"isb\n" :: "r"(count));
	asm volatile("msr S3_0_c5_c4_5, %0\n"
			"isb\n" :: "r"(ctrl));
}

static void simulate_ECC(char *arg)
{
	int cpu, temp;
	unsigned long lev;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	if (kstrtoint(arg, 16, &temp)) {
		dev_err(exynos_debug_desc.dev, "check input parameter\n");
		return;
	}
	cpu = (temp >> 4) & 0xf;
	lev = temp & 0xf;

	if (cpu == raw_smp_processor_id())
		simulate_ECC_INJECTION((void *)&lev);
	else
		smp_call_function_single(cpu, simulate_ECC_INJECTION, &lev, 1);
}

static void make_lockup_matched_stage(enum s2r_lock_stage stage)
{
	if (exynos_debug_desc.s2r_stage == stage)
		mutex_lock(&exynos_debug_desc.s2r_lock);
}

static int pre_s2r_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		make_lockup_matched_stage(eS2R_PM_SUSPEND_PREPARE);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block pre_s2r_pm_nb = {
	.notifier_call = pre_s2r_pm_notifier,
	.priority = INT_MAX,
};

static int post_s2r_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_POST_SUSPEND:
		make_lockup_matched_stage(eS2R_PM_POST_SUSPEND);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block post_s2r_pm_nb = {
	.notifier_call = post_s2r_pm_notifier,
	.priority = INT_MIN,
};

static int s2r_lockup_prepare(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_SUSPEND_PREPARE);
	return 0;
}

static int s2r_lockup_suspend(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_SUSPEND);
	return 0;
}

static int s2r_lockup_suspend_late(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_SUSPEND_LATE);
	return 0;
}

static int s2r_lockup_suspend_noirq(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_SUSPEND_NOIRQ);
	return 0;
}

static int s2r_lockup_resume_noirq(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_RESUME_NOIRQ);
	return 0;
}

static int s2r_lockup_resume_early(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_RESUME_EARLY);
	return 0;
}

static int s2r_lockup_resume(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_RESUME);
	return 0;
}

static void s2r_lockup_complete(struct device *dev)
{
	make_lockup_matched_stage(eS2R_DEV_COMPLETE);
}

static const struct dev_pm_ops __maybe_unused s2r_lockup_pm_ops = {
	.prepare = s2r_lockup_prepare,
	.suspend = s2r_lockup_suspend,
	.suspend_late = s2r_lockup_suspend_late,
	.suspend_noirq = s2r_lockup_suspend_noirq,
	.resume_noirq = s2r_lockup_resume_noirq,
	.resume_early = s2r_lockup_resume_early,
	.resume = s2r_lockup_resume,
	.complete = s2r_lockup_complete,
};

static int s2r_lockup_syscore_suspend(void)
{
	make_lockup_matched_stage(eS2R_SYSCORE_SUSPEND);
	return 0;
}

static void s2r_lockup_syscore_resume(void)
{
	make_lockup_matched_stage(eS2R_SYSCORE_RESUME);
}

static struct syscore_ops s2r_lockup_syscore_ops = {
	.suspend = s2r_lockup_syscore_suspend,
	.resume = s2r_lockup_syscore_resume,
};

static void simulate_S2RLOCKUP(char *arg)
{
	static int enabled = 0;
	int i;

	if (!arg)
		return;

	if (enabled && !strcmp(arg, "unlock")) {
		enabled = 0;
		exynos_debug_desc.s2r_stage = eS2R_NOLOCK;
		mutex_unlock(&exynos_debug_desc.s2r_lock);
		return;
	}

	for (i = eS2R_PM_SUSPEND_PREPARE; i < eS2R_STAGE_CNT; i++) {
		if (!strcmp(arg, exynos_debug_desc.s2r_stage_name[i])) {
			if (!enabled)
				mutex_lock(&exynos_debug_desc.s2r_lock);
			enabled = 1;
			exynos_debug_desc.s2r_stage = i;
			break;
		}
	}
}

static atomic_t multi_panic_cpu_cnt;

static void simulate_MULTI_PANIC_handler(void *info)
{
	unsigned int cnt;

	pr_emerg("CPU%d %s called\n", raw_smp_processor_id(), __func__);

	atomic_inc(&multi_panic_cpu_cnt);

	do {
		cnt = atomic_read(&multi_panic_cpu_cnt);
	} while (cnt != num_active_cpus());

	panic("CPU%d panic, active cpu num = %u", raw_smp_processor_id(), cnt);
}

static void simulate_MULTI_PANIC(char *arg)
{
	int this_cpu, cpu;

	pr_emerg("%s called\n", __func__);

	atomic_set(&multi_panic_cpu_cnt, 0);
	this_cpu = get_cpu();
	for_each_possible_cpu(cpu) {
		if (cpu == this_cpu)
			continue;

		smp_call_function_single(cpu, simulate_MULTI_PANIC_handler, NULL, 0);
	}
	simulate_MULTI_PANIC_handler(NULL);
	put_cpu();
}

static struct force_error_item force_error_vector[] = {
	{"KP",		&simulate_KP},
	{"DP",		&simulate_DP},
	{"QDP",		&simulate_QDP},
	{"SVC",		&simulate_SVC},
	{"SFR",		&simulate_SFR},
	{"WP",		&simulate_WP},
	{"panic",	&simulate_PANIC},
	{"bug",		&simulate_BUG},
	{"warn",	&simulate_WARN},
	{"dabrt",	&simulate_DABRT},
	{"pabrt",	&simulate_PABRT},
	{"undef",	&simulate_UNDEF},
	{"dfree",	&simulate_DFREE},
	{"danglingref",	&simulate_DREF},
	{"memcorrupt",	&simulate_MCRPT},
	{"lowmem",	&simulate_LOMEM},
	{"softlockup",	&simulate_SOFT_LOCKUP},
	{"hardlockup",	&simulate_HARD_LOCKUP},
	{"spinlockup",	&simulate_SPIN_LOCKUP},
	{"pcabort",	&simulate_PC_ABORT},
	{"spabort",	&simulate_SP_ABORT},
	{"jumpzero",	&simulate_JUMP_ZERO},
	{"unaligned",	&simulate_UNALIGNED},
	{"writero",	&simulate_WRITE_RO},
	{"overflow",	&simulate_OVERFLOW},
	{"cacheflush",	&simulate_CACHE_FLUSH},
	{"apmwdt",	&simulate_APM_WDT},
	{"arraydump",	&simulate_ARRAYDUMP},
	{"halt",	&simulate_HALT},
	{"scandump",	&simulate_SCANDUMP},
	{"dramfail",	&simulate_DRAMFAIL},
	{"ecc",		&simulate_ECC},
	{"s2rlockup",	&simulate_S2RLOCKUP},
	{"multiPanic",	&simulate_MULTI_PANIC},
};

static int debug_force_error(const char *val)
{
	int i;
	char *temp;
	char *ptr;

	for (i = 0; i < (int)ARRAY_SIZE(force_error_vector); i++) {
		if (!strncmp(val, force_error_vector[i].errname,
				strlen(force_error_vector[i].errname))) {
			temp = (char *)val;
			ptr = strsep(&temp, " ");	/* ignore the first token */
			ptr = strsep(&temp, " ");	/* take the second token */
			force_error_vector[i].errfunc(ptr);
			break;
		}
	}

	if (i == (int)ARRAY_SIZE(force_error_vector))
		dev_info(exynos_debug_desc.dev, "%s(): INVALID TEST CMD = [%s]\n",
				__func__, val);

	return 0;
}

static ssize_t exynos_debug_test_write(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf;
	size_t buf_size;
	int i;

	buf_size = ((count + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
	if (buf_size <= 0)
		return 0;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return 0;

	if (copy_from_user(buf, user_buf, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';

	for (i = 0; buf[i] != '\0'; i++)
		if (buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}

	dev_info(exynos_debug_desc.dev, "%s() user_buf=[%s]\n", __func__, buf);
	debug_force_error(buf);

	kfree(buf);
	return count;
}

static ssize_t exynos_debug_test_read(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	char *buf;
	size_t copy_cnt;
	int i;
	int ret = 0;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return ret;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"========== DEBUG TEST EXAMPLES ==========\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "INPUT_ARG(S)\n");

	for (i = 0; i < (int)ARRAY_SIZE(test_vector); i++)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				"%-30s\n", test_vector[i]);

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"=========================================\n");

	copy_cnt = ret;
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, copy_cnt);

	kfree(buf);
	return ret;
}
static const struct file_operations exynos_debug_test_file_fops = {
	.open	= simple_open,
	.read	= exynos_debug_test_read,
	.write	= exynos_debug_test_write,
	.llseek	= default_llseek,
};

static int exynos_debug_test_parsing_dt(struct device_node *np)
{
	int ret = 0;

	/* get data from device tree */
	ret = of_property_read_u32(np, "ps_hold_control_offset",
			&exynos_debug_desc.ps_hold_control_offset);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(ps_hold_control offset)\n");
		exynos_debug_desc.ps_hold_control_offset = 0;
	}

	ret = of_property_read_u32(np, "nr_cpu", &exynos_debug_desc.nr_cpu);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(nr_cpu)\n");
		goto edt_desc_init_out;
	}

	ret = of_property_read_u32(np, "little_cpu_start",
			&exynos_debug_desc.little_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(little_cpu_start)\n");
		exynos_debug_desc.little_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_little_cpu",	&exynos_debug_desc.nr_little_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_little_cpu)\n");
		exynos_debug_desc.nr_little_cpu = -1;
	}

	ret = of_property_read_u32(np, "mid_cpu_start",	&exynos_debug_desc.mid_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(mid_cpu_start)\n");
		exynos_debug_desc.mid_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_mid_cpu", &exynos_debug_desc.nr_mid_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_mid_cpu)\n");
		exynos_debug_desc.nr_mid_cpu = -1;
	}

	ret = of_property_read_u32(np, "big_cpu_start", &exynos_debug_desc.big_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(big_cpu_start)\n");
		exynos_debug_desc.big_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_big_cpu", &exynos_debug_desc.nr_big_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_big_cpu)\n");
		exynos_debug_desc.nr_big_cpu = -1;
	}

	ret = of_property_read_u32(np, "scratch-offset", &exynos_debug_desc.scratch_offset);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(scratch offset)\n");
		exynos_debug_desc.scratch_offset = -1;
	}

	ret = of_property_read_u32(np, "dram-init-bit", &exynos_debug_desc.dram_init_bit);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(dram init bit)\n");
		exynos_debug_desc.dram_init_bit = -1;
	}

edt_desc_init_out:
	return ret;
}

static void init_s2r_lockup_test(void)
{
	mutex_init(&exynos_debug_desc.s2r_lock);

	exynos_debug_desc.s2r_stage_name[eS2R_PM_SUSPEND_PREPARE] = "pm_prepare";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_SUSPEND_PREPARE] = "dev_suspend_prepare";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_SUSPEND] = "dev_suspend";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_SUSPEND_LATE] = "dev_suspend_late";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_SUSPEND_NOIRQ] = "dev_suspend_noirq";
	exynos_debug_desc.s2r_stage_name[eS2R_SYSCORE_SUSPEND] = "syscore_suspend";
	exynos_debug_desc.s2r_stage_name[eS2R_SYSCORE_RESUME] = "syscore_resume";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_RESUME_NOIRQ] = "dev_resume_noirq";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_RESUME_EARLY] = "dev_resume_early";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_RESUME] = "dev_resume";
	exynos_debug_desc.s2r_stage_name[eS2R_DEV_COMPLETE] = "dev_complete";
	exynos_debug_desc.s2r_stage_name[eS2R_PM_POST_SUSPEND] = "pm_post";

	exynos_debug_desc.s2r_stage = eS2R_NOLOCK;

	register_pm_notifier(&pre_s2r_pm_nb);
	register_pm_notifier(&post_s2r_pm_nb);
	register_syscore_ops(&s2r_lockup_syscore_ops);
}

static void exynos_debug_test_desc_init(void)
{
	exynos_debug_desc.null_function = (void (*)(void))0x1234;
	spin_lock_init(&exynos_debug_desc.debug_test_lock);
	init_s2r_lockup_test();

	/* create debugfs test file */
	debugfs_create_file("test", 0644,
			exynos_debug_desc.debugfs_root,
			NULL, &exynos_debug_test_file_fops);
	exynos_debug_desc.enabled = 1;
}

static int exynos_debug_test_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	dev_info(&pdev->dev, "%s() called\n", __func__);

	exynos_debug_desc.dev = &pdev->dev;
	ret = exynos_debug_test_parsing_dt(np);
	if (ret) {
		dev_err(&pdev->dev, "cannot parse test data from dt\n");
		goto edt_out;
	}
	exynos_debug_desc.debugfs_root =
			debugfs_create_dir("exynos-debug-test", NULL);
	if (!exynos_debug_desc.debugfs_root) {
		dev_err(&pdev->dev, "cannot create debugfs dir\n");
		ret = -ENOMEM;
		goto edt_out;
	}

	exynos_debug_test_desc_init();
edt_out:
	dev_info(exynos_debug_desc.dev, "%s() ret=[0x%x]\n", __func__, ret);
	return ret;
}

static const struct of_device_id exynos_debug_test_matches[] = {
	{.compatible = "samsung,exynos-debug-test"},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_debug_test_matches);

static struct platform_driver exynos_debug_test_driver = {
	.probe		= exynos_debug_test_probe,
	.driver		= {
		.name	= "exynos-debug-test",
		.of_match_table	= of_match_ptr(exynos_debug_test_matches),
		.pm = &s2r_lockup_pm_ops,
	},
};
module_platform_driver(exynos_debug_test_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exynos Debug Feature Test Driver");
