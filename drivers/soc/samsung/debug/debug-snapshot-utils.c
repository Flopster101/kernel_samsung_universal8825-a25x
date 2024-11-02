/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#include <linux/context_tracking.h>
#include <linux/nmi.h>
#include <linux/init_task.h>
#include <linux/reboot.h>
#include <linux/kdebug.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/input.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/console.h>

#include <asm/cputype.h>
#include <asm/stacktrace.h>
#include <asm/memory.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-smc.h>
#include "debug-snapshot-local.h"
#include "system-regs.h"

#include <trace/hooks/debug.h>

#define BACKTRACE_CPU_INVALID	(-1)

static struct cpumask cpu_dss_context_saved_mask;

#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
#define SUMMARY_BUF_MAX		64
#endif

static char *ecc_sel_str[] = {
	"DSU", "L1", "L2", NULL,
};

/*  Panic core's backtrace logging  */
static struct dbg_snapshot_backtrace_data *dss_backtrace;
atomic_t backtrace_cpu = ATOMIC_INIT(BACKTRACE_CPU_INVALID);

static unsigned long smc_pre_reading_ecc_sysreg[4];

struct dbg_snapshot_mmu_reg {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
	long ELR_EL1;
	long SP_EL0;
};

extern void flush_cache_all(void);
static struct pt_regs __percpu **dss_core_reg;
static struct dbg_snapshot_mmu_reg __percpu **dss_mmu_reg;
static struct dbg_snapshot_helper_ops dss_soc_ops;

void cache_flush_all(void)
{
	flush_cache_all();
}
EXPORT_SYMBOL_GPL(cache_flush_all);

ATOMIC_NOTIFIER_HEAD(dump_task_notifier_list);

void register_dump_one_task_notifier(struct notifier_block *nb)
{
	atomic_notifier_chain_register(&dump_task_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_dump_one_task_notifier);

static const char * dbg_snapshot_wday_to_string(int wday)
{
	static const char *day[7] = { "Sun", "Mon", "Tue", "Wed",
					"Thu", "Fri", "Sat" };
	const char *ret;

	if (wday >= 0 && wday <= 6)
		ret = day[wday];
	else
		ret = NULL;

	return ret;
}

static const char * dbg_snapshot_mon_to_string(int mon)
{
	static const char *month[12] = { "Jan", "Feb", "Mar", "Apr",
					"May", "Jun", "Jul", "Aug",
					"Sep", "Oct", "Nov", "Dec" };
	const char *ret;

	if (mon >= 0 && mon <= 11)
		ret = month[mon];
	else
		ret = NULL;

	return ret;
}

static void dbg_snapshot_backtrace_start(unsigned long pc, unsigned long lr,
					struct pt_regs *regs, void *data)
{
	struct timespec64 ts64;
	struct rtc_time tm;
	size_t size;
	u64 curr_idx;
	u64 tv_kernel;
	unsigned long rem_nsec;
	char *vaddr;

	if (dss_backtrace->stop_logging)
		return;

	size = dss_backtrace->size;
	curr_idx = dss_backtrace->curr_idx;
	vaddr = (char *)dss_backtrace->vaddr;

	ktime_get_real_ts64(&ts64);
	rtc_time64_to_tm(ts64.tv_sec - (sys_tz.tz_minuteswest * 60), &tm);

	tv_kernel = local_clock();
	rem_nsec = do_div(tv_kernel, 1000000000);
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"$** *** *** *** *** *** *** *** "
				"Fatal *** *** *** *** *** *** *** **$\n");
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"%s Log Time:[%s %s %d %02d:%02d:%02d UTC %d][%lu.%06lu]\n",
				(regs == NULL) ? "Panic" : "Exception",
				dbg_snapshot_wday_to_string(tm.tm_wday),
				dbg_snapshot_mon_to_string(tm.tm_mon),
				tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				tm.tm_year + 1900, tv_kernel, rem_nsec / 1000);
	if (regs) {
		struct die_args *args = (struct die_args *)data;

		curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
					"Internal error: %s: %x\n\n",
					args->str, args->err);
	} else {
		curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
					"Kernel panic - not syncing %s\n\n",
					(char *)data);
	}
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"PC is at %pS\n", (void *)pc);
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"LR is at %pS\n", (void *)lr);
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"Current Executing Process:\n[CPU, %d][%s, %d][AARCH%d]\n\n",
				raw_smp_processor_id(),
				current->comm, current->pid,
				compat_user_mode(task_pt_regs(current)) ? 32 : 64);
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
				"Backtrace:\n");

	dss_backtrace->curr_idx = curr_idx;
}

static void dbg_snapshot_backtrace_log(unsigned long where)
{
	size_t size;
	u64 curr_idx;
	char *vaddr;

	if (dss_backtrace->stop_logging)
		return;

	size = dss_backtrace->size;
	curr_idx = dss_backtrace->curr_idx;
	vaddr = (char *)dss_backtrace->vaddr;

	where |= (-1UL) << VA_BITS;
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
			     "%pS\n", (void *)where);
	dss_backtrace->curr_idx = curr_idx;
}

static void dbg_snapshot_backtrace_stop(void)
{
	size_t size;
	u64 curr_idx;
	char *vaddr;

	if (dss_backtrace->stop_logging)
		return;

	size = dss_backtrace->size;
	curr_idx = dss_backtrace->curr_idx;
	vaddr = (char *)dss_backtrace->vaddr;

	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
			     "0xffffffffffffffff\n");
	curr_idx += scnprintf(vaddr + curr_idx, size - curr_idx,
			     "$** *** *** *** *** *** *** *** "
			     "Fatal *** *** *** *** *** *** *** **$\n");
	dss_backtrace->curr_idx = curr_idx;
	if (curr_idx >= size)
		vaddr[size - 1] = 0;
	dss_backtrace->stop_logging = true;

	dbg_snapshot_set_val64_offset(dss_backtrace->paddr,
					DSS_OFFSET_BACKTRACE_PADDR);
	dbg_snapshot_set_val64_offset(dss_backtrace->size,
					DSS_OFFSET_BACKTRACE_SIZE);
	dbg_snapshot_set_val_offset(raw_smp_processor_id(),
					DSS_OFFSET_BACKTRACE_CPU);
	dbg_snapshot_set_val_offset(DSS_BACKTRACE_MAGIC,
					DSS_OFFSET_BACKTRACE_MAGIC);
}

static int dbg_snapshot_on_accessable_stack(const struct task_struct *tsk,
						unsigned long sp)
{
	struct stack_info __maybe_unused info;

	if (on_task_stack(tsk, sp, &info))
		return true;
	if (tsk != current || preemptible())
		return false;
	if (sp < UINT_MAX)
		return false;

	return true;
}

static int dbg_snapshot_unwind_frame(struct task_struct *tsk,
					struct stackframe *frame)
{
	unsigned long fp = frame->fp;

	if (fp & 0xf)
		return -EINVAL;

	if (!is_vmalloc_addr((const void *)fp) && !virt_addr_valid(fp))
		return -EINVAL;

	if (!tsk)
		tsk = current;

	if (!dbg_snapshot_on_accessable_stack(tsk, fp))
		return -EINVAL;

	if (fp == frame->prev_fp)
		return -EINVAL;

	/*
	 * Many stack exception processing is missing. Exception may occur
	 * during processing, but try only once to prevent recursive error.
	 */

	/*
	 * Record this frame record's values and location. The prev_fp and
	 * prev_type are only meaningful to the next unwind_frame() invocation.
	 */
	frame->fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp));
	frame->pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));
	frame->prev_fp = fp;

	/*
	 * Frames created upon entry from EL0 have NULL FP and PC values, so
	 * don't bother reporting these. Frames created by __noreturn functions
	 * might have a valid FP even if PC is bogus, so only terminate where
	 * both are NULL.
	 */
	if (!frame->fp && !frame->pc)
		return -EINVAL;

	return 0;
}

#define MAX_UNWINDING_LOOP 256
#define reset_unwinding_loop_cnt(cnt) do { cnt = 0; } while(0)
#define check_over_max_unwidning_loop_cnt(cnt) (++cnt > MAX_UNWINDING_LOOP)

void dbg_snapshot_dump_backtrace(struct pt_regs *regs,
				struct task_struct *tsk, void *data)
{
	struct stackframe frame;
	int skip = 0;
	unsigned long pc = 0;
	unsigned long lr = 0;
	int unwind_loop_cnt;

	if (regs) {
		if (user_mode(regs))
			return;
		skip = 1;
	} else {
		return;
	}

	if (!tsk)
		tsk = current;

	if (!try_get_task_stack(tsk))
		return;

	if (tsk == current) {
		start_backtrace(&frame,
				(unsigned long)__builtin_frame_address(0),
				(unsigned long)dbg_snapshot_dump_backtrace);
	} else {
		/*
		 * task blocked in __switch_to
		 */
		start_backtrace(&frame,
				thread_saved_fp(tsk),
				thread_saved_pc(tsk));
	}

	reset_unwinding_loop_cnt(unwind_loop_cnt);
	do {
		if (!skip) {
			pc = (unsigned long)frame.pc;
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			pc = (unsigned long)regs->pc;
		}
	} while (!pc && !dbg_snapshot_unwind_frame(tsk, &frame) &&
		!check_over_max_unwidning_loop_cnt(unwind_loop_cnt));

	reset_unwinding_loop_cnt(unwind_loop_cnt);
	while (!lr && !dbg_snapshot_unwind_frame(tsk, &frame)) {
		if (check_over_max_unwidning_loop_cnt(unwind_loop_cnt))
			break;
		if (!skip) {
			lr = (unsigned long)frame.pc;
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			lr = (unsigned long)regs->pc;
		}
	}

	dbg_snapshot_backtrace_start(pc, lr, regs, data);
	dbg_snapshot_backtrace_log(pc);
	dbg_snapshot_backtrace_log(lr);

	reset_unwinding_loop_cnt(unwind_loop_cnt);
	while (!dbg_snapshot_unwind_frame(tsk, &frame)) {
		if (check_over_max_unwidning_loop_cnt(unwind_loop_cnt))
			break;
		/* skip until specified stack frame */
		if (!skip) {
			dbg_snapshot_backtrace_log((unsigned long)frame.pc);
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			/*
			 * Mostly, this is the case where this function is
			 * called in panic/abort. As exception handler's
			 * stack frame does not contain the corresponding pc
			 * at which an exception has taken place, use regs->pc
			 * instead.
			 */
			dbg_snapshot_backtrace_log((unsigned long)regs->pc);
		}
	}

	dbg_snapshot_backtrace_stop();
}

static void dbg_snapshot_backtrace(struct pt_regs *regs, void *data)
{
	int old_cpu, cpu;
	int i;
	void *where;

	if (!dss_backtrace || dss_backtrace->stop_logging)
		return;

	cpu = raw_smp_processor_id();
	old_cpu = atomic_cmpxchg(&backtrace_cpu, BACKTRACE_CPU_INVALID, cpu);

	if (old_cpu != BACKTRACE_CPU_INVALID)
		return;

	if (regs) {
		dbg_snapshot_dump_backtrace(regs, NULL, data);
	} else {
		/* skip call stack related with panic */
		dbg_snapshot_backtrace_start((unsigned long)return_address(2),
					(unsigned long)return_address(3),
					regs, data);
		i = 2;
		where = return_address(i++);

		while (where) {
			dbg_snapshot_backtrace_log((unsigned long)where);
			where = return_address(i++);
		}

		dbg_snapshot_backtrace_stop();
	}
}

static void dbg_snapshot_register_backtrace(void)
{
	struct dbg_snapshot_item *item = dbg_snapshot_get_item(DSS_ITEM_BACKTRACE);
	struct dbg_snapshot_backtrace_data *data;

	if (!dbg_snapshot_get_item_enable(DSS_ITEM_BACKTRACE) || !item)
		return;

	data = devm_kzalloc(dss_desc.dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return;

	data->paddr = (u64)item->entry.paddr;
	data->vaddr = (char *)item->entry.vaddr;
	data->size = item->entry.size;
	data->curr_idx = 0;
	data->stop_logging = false;

	memset(data->vaddr, 0, data->size);

	/* clear magic */
	dbg_snapshot_set_val_offset(0, DSS_OFFSET_BACKTRACE_MAGIC);

	dss_backtrace = data;
	dev_info(dss_desc.dev, "backtrace setup\n");
}

static void dbg_snapshot_set_core_power_stat(unsigned int val, unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_CORE_POWER_STAT + cpu * 4);
}

static unsigned int dbg_snapshot_get_core_panic_stat(unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	return header ?  __raw_readl(header + DSS_OFFSET_PANIC_STAT + cpu * 4) : 0;
}

static void dbg_snapshot_set_core_panic_stat(unsigned int val, unsigned cpu)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_PANIC_STAT + cpu * 4);
}

static void dbg_snapshot_report_reason(unsigned int val)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writel(val, header + DSS_OFFSET_EMERGENCY_REASON);
}

static unsigned int dbg_snapshot_get_report_reason(void)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	return header ? __raw_readl(header + DSS_OFFSET_EMERGENCY_REASON) : 0;
}

static void dbg_snapshot_set_wdt_caller(unsigned long addr)
{
	void __iomem *header = dbg_snapshot_get_header_vaddr();

	if (header)
		__raw_writeq(addr, header + DSS_OFFSET_WDT_CALLER);
}

int dbg_snapshot_start_watchdog(int sec)
{
	if (dss_soc_ops.start_watchdog)
		return dss_soc_ops.start_watchdog(true, 0, sec);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_start_watchdog);

int dbg_snapshot_expire_watchdog(void)
{
	unsigned long addr;

	if (!dss_soc_ops.expire_watchdog) {
		dev_emerg(dss_desc.dev, "There is no wdt functions!\n");
		return -ENODEV;
	}

	addr = (unsigned long)return_address(0);

	dbg_snapshot_set_wdt_caller(addr);
	dev_emerg(dss_desc.dev, "Caller: %pS, WDTRESET right now!\n", (void *)addr);
	if (dss_soc_ops.expire_watchdog(3, 0))
		return -ENODEV;
	dbg_snapshot_spin_func();

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_expire_watchdog);

int dbg_snapshot_expire_watchdog_safely(void)
{
	unsigned long addr;

	if (!dss_soc_ops.expire_watchdog) {
		dev_emerg(dss_desc.dev, "There is no wdt functions!\n");
		return -ENODEV;
	}

	if (!dss_soc_ops.set_safe_mode)
		dev_emerg(dss_desc.dev, "There is no safe mode function!\n");
	else
		dss_soc_ops.set_safe_mode();

	addr = (unsigned long)return_address(0);

	dbg_snapshot_set_wdt_caller(addr);
	dev_emerg(dss_desc.dev, "Caller: %pS, WDTRESET right now!\n", (void *)addr);
	if (dss_soc_ops.expire_watchdog(3, 0))
		return -ENODEV;
	dbg_snapshot_spin_func();

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_expire_watchdog_safely);

int dbg_snapshot_expire_watchdog_timeout(int tick)
{
	unsigned long addr;

	if (!dss_soc_ops.expire_watchdog) {
		dev_emerg(dss_desc.dev, "There is no wdt functions!\n");
		return -ENODEV;
	}

	addr = (unsigned long)return_address(0);

	dbg_snapshot_set_wdt_caller(addr);
	dev_emerg(dss_desc.dev, "Caller: %pS, WDTRESET right now!\n", (void *)addr);
	if (dss_soc_ops.expire_watchdog(tick, 0))
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_expire_watchdog_timeout);

int dbg_snapshot_expire_watchdog_timeout_safely(int tick)
{
	unsigned long addr;

	if (!dss_soc_ops.expire_watchdog) {
		dev_emerg(dss_desc.dev, "There is no wdt functions!\n");
		return -ENODEV;
	}

	if (!dss_soc_ops.set_safe_mode)
		dev_emerg(dss_desc.dev, "There is no safe mode function!\n");
	else
		dss_soc_ops.set_safe_mode();

	addr = (unsigned long)return_address(0);

	dbg_snapshot_set_wdt_caller(addr);
	dev_emerg(dss_desc.dev, "Caller: %pS, WDTRESET right now!\n", (void *)addr);
	if (dss_soc_ops.expire_watchdog(tick, 0))
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_expire_watchdog_timeout_safely);

int dbg_snapshot_kick_watchdog(void)
{
	if (dss_soc_ops.start_watchdog)
		return dss_soc_ops.start_watchdog(false, 0, 0);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_kick_watchdog);

#define task_contributes_to_load(task)  ((task->state & TASK_UNINTERRUPTIBLE) != 0 && \
		(task->flags & PF_FROZEN) == 0 && \
		(task->state & TASK_NOLOAD) == 0)

static unsigned long dbg_snapshot_get_wchan(struct task_struct *p)
{
	unsigned long entry = 0;
	unsigned int skip = 0;

	if (p != current)
		skip = 1;	/* to skip __switch_to */

	stack_trace_save_tsk(p, &entry, 1, skip);

	return entry;
}

static void dbg_snapshot_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'X',
			'Z', 'P', 'x', 'K', 'W', 'I', 'N'};
	unsigned char idx = 0;
	unsigned long state, pc = 0;
	unsigned long wchan;
	char symname[KSYM_NAME_LEN];

	if ((tsk == NULL) || !try_get_task_stack(tsk))
		return;

	state = tsk->state | tsk->exit_state;
	pc = KSTK_EIP(tsk);
	while (state) {
		idx++;
		state >>= 1;
	}

	wchan = dbg_snapshot_get_wchan(tsk);
	snprintf(symname, KSYM_NAME_LEN, "%ps", wchan);

	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	touch_softlockup_watchdog();

	pr_info("%8d %16llu %16llu %16llu %c(%4ld) %1d %16zx %16zx %c %16s [%s]\n",
		tsk->pid, tsk->utime, tsk->stime,
		tsk->se.exec_start, state_array[idx], tsk->state,
		task_cpu(tsk), pc, (unsigned long)tsk,
		is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->on_cpu && tsk->on_rq && tsk->cpu != smp_processor_id())
		return;

	if (tsk->state == TASK_RUNNING || tsk->state == TASK_WAKING ||
			task_contributes_to_load(tsk)) {
		atomic_notifier_call_chain(&dump_task_notifier_list, 0,	(void *)tsk);
		dump_backtrace(NULL, tsk, KERN_DEFAULT);
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next, struct task_struct, thread_group);
}

static void dbg_snapshot_dump_task_info(void)
{
	struct task_struct *frst_tsk, *curr_tsk;
	struct task_struct *frst_thr, *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n",
			current->pid, current->comm);
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");
	pr_info("%8s %16s %16s %16s %6s %3s %16s %16s  %16s\n",
			"pid", "uTime", "sTime", "exec(ns)", "stat", "cpu",
			"user_pc", "task_struct", "comm");
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk) {
		dbg_snapshot_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					dbg_snapshot_dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info("--------------------------------------------------------"
			"-------------------------------------------------\n");
}

static void dbg_snapshot_save_system(void *unused)
{
	struct dbg_snapshot_mmu_reg *mmu_reg;

	mmu_reg = *per_cpu_ptr(dss_mmu_reg, raw_smp_processor_id());

	asm volatile ("mrs x1, SCTLR_EL1\n\t"	/* SCTLR_EL1 */
		"mrs x2, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
		"stp x1, x2, [%0]\n\t"
		"mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
		"mrs x2, TCR_EL1\n\t"		/* TCR_EL1 */
		"stp x1, x2, [%0, #0x10]\n\t"
		"mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
		"mrs x2, FAR_EL1\n\t"		/* FAR_EL1 */
		"stp x1, x2, [%0, #0x20]\n\t"
		"mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
		"mrs x2, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
		"stp x1, x2, [%0, #0x30]\n\t"
		"mrs x1, TPIDRRO_EL0\n\t"	/* TPIDRRO_EL0 */
		"mrs x2, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
		"stp x1, x2, [%0, #0x40]\n\t"
		"mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
		"mrs x2, ELR_EL1\n\t"		/* ELR_EL1 */
		"stp x1, x2, [%0, #0x50]\n\t"
		"mrs x1, SP_EL0\n\t"		/* SP_EL0 */
		"str x1, [%0, 0x60]\n\t" :	/* output */
		: "r"(mmu_reg)			/* input */
		: "%x1", "memory"		/* clobbered register */
	);
}

static void clear_external_ecc_err(ERXSTATUS_EL1_t erxstatus_el1)
{
	erxstatus_el1.field.CE = 0x3;
	erxstatus_el1.field.UET = 0x3;
	erxstatus_el1.field.SERR = 0x0;
	erxstatus_el1.field.IERR = 0x0;

	write_ERXSTATUS_EL1(erxstatus_el1.reg);
	write_ERXMISC0_EL1(0);
	write_ERXMISC1_EL1(0);
}

char *get_external_ecc_err(ERXSTATUS_EL1_t erxstatus_el1)
{
	const char *ext_err;

	if (erxstatus_el1.field.SERR == 0xC)
		ext_err = "Data value from (non-associative) external memory.";
	else if (erxstatus_el1.field.SERR == 0x12)
		ext_err = "Error response from secondary.";
	else
		ext_err = NULL;

	clear_external_ecc_err(erxstatus_el1);

	return (char *)ext_err;
}

char *get_correct_ecc_err(ERXSTATUS_EL1_t erxstatus_el1)
{
	const char *cr_err;

	switch (erxstatus_el1.field.CE) {
	case BIT(1) | BIT(0):
		cr_err = "At least persistent was corrected";
		break;
	case BIT(1):
		cr_err = "At least one error was corrected";
		break;
	case BIT(0):
		cr_err = "At least one transient error was corrected";
		break;
	default:
		cr_err = NULL;
	}

	return (char *)cr_err;
}

static void do_wa_pre_reading_ecc_sysreg(void)
{
	if (!smc_pre_reading_ecc_sysreg[0])
		return;
	exynos_smc(smc_pre_reading_ecc_sysreg[0], smc_pre_reading_ecc_sysreg[1],
		   smc_pre_reading_ecc_sysreg[2], smc_pre_reading_ecc_sysreg[3]);
}

static void _dbg_snapshot_ecc_dump(bool call_panic)
{
	ERRSELR_EL1_t errselr_el1;
	ERRIDR_EL1_t erridr_el1;
	ERXSTATUS_EL1_t erxstatus_el1;
	char *msg;
	bool is_capable_identifing_err = false;
	int i;

	do_wa_pre_reading_ecc_sysreg();
	asm volatile ("HINT #16");
	erridr_el1.reg = read_ERRIDR_EL1();

	for (i = 0; i < (int)erridr_el1.field.NUM; i++) {
		char errbuf[SZ_512] = {0, };
		int n = 0;
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
		const char *msg_overflow = "";
		const char *msg_er = "";
		const char *msg_uncorrected = "";
		const char *msg_deferred = "";
		const char *msg_corrected = "";
		const char *msg_delimiter = "";
		char msg_selstatus[SUMMARY_BUF_MAX] = "";
		char msg_addr[SUMMARY_BUF_MAX] = "";
		char msg_misc[SUMMARY_BUF_MAX] = "";
		char msg_serr[SUMMARY_BUF_MAX] = "";
#endif

		errselr_el1.reg = read_ERRSELR_EL1();
		errselr_el1.field.SEL = i;
		write_ERRSELR_EL1(errselr_el1.reg);

		isb();

		erxstatus_el1.reg = read_ERXSTATUS_EL1();
		msg = erxstatus_el1.field.Valid ? "Error" : "NO Error";

		n = scnprintf(errbuf + n, sizeof(errbuf) - n,
			"%03s: %08s: [NUM:%d][ERXSTATUS_EL1:%#016x]\n",
			ecc_sel_str[i] ? ecc_sel_str[i] : "", msg, i, erxstatus_el1.reg);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
		scnprintf(msg_selstatus, sizeof(msg_selstatus),
			"%3s status:0x%08lx",
			ecc_sel_str[i] ? ecc_sel_str[i] : "",
			erxstatus_el1.reg);
#endif

		if (!erxstatus_el1.field.Valid)
			goto output_cont;

		if (erxstatus_el1.field.AV) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ AV ] Detected(Address Valid): [ERXADDR_EL1:%#llx]\n",
				read_ERXADDR_EL1());
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			scnprintf(msg_addr, sizeof(msg_addr),
				"(Addr:0x%llx)",
				read_ERXADDR_EL1());
#endif
		}
		if (erxstatus_el1.field.OF) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ OF ] Detected(Overflow): There was more than one error has occurred\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			msg_overflow = "[Overflow]";
#endif
		}
		if (erxstatus_el1.field.ER) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ ER ] Detected(Error Report by external abort)\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			msg_er = "(Reported)";
#endif
		}
		if (erxstatus_el1.field.UE) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ UE ] Detected(Uncorrected Error): Not deferred\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			msg_uncorrected = "[Uncorrected]";
#endif
		}
		if (erxstatus_el1.field.DE) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ DE ] Detected(Deferred Error)\n");
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			msg_deferred = "[Deferred]";
#endif
		}
		if (erxstatus_el1.field.MV) {
			n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ MV ] Detected(Miscellaneous Registers Valid): [ERXMISC0_EL1:%#llx][ERXMISC1_EL1:%#llx]\n",
				read_ERXMISC0_EL1(), read_ERXMISC1_EL1());
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			scnprintf(msg_misc, sizeof(msg_misc),
				"(MISC0:0x%llx)(MISC1:0x%llx)",
				read_ERXMISC0_EL1(), read_ERXMISC1_EL1());
#endif
		}
		if (erxstatus_el1.field.CE) {
			msg = get_correct_ecc_err(erxstatus_el1);
			if (msg)
				n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [ CE ] Detected(Corrected Error): %s, [CE:%#x]\n",
				msg, erxstatus_el1.field.CE);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
			msg_corrected = "[Corrected]";
#endif
		}
		if (erxstatus_el1.field.SERR) {
			msg = get_external_ecc_err(erxstatus_el1);
			if (msg) {
				n += scnprintf(errbuf + n, sizeof(errbuf) - n,
				"\t [SERR] Detected(External ECC Error): %s, [SERR:%#x]\n",
				msg, erxstatus_el1.field.SERR);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
				scnprintf(msg_serr, sizeof(msg_serr),
					"(External Err:%#x)",
					erxstatus_el1.field.SERR);
#endif
				goto output_cont;
			}
		}
		is_capable_identifing_err = true;
output_cont:
		pr_emerg("%s", errbuf);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
		if (!erxstatus_el1.field.Valid)
			continue;

		if (msg_serr[0] || msg_er[0] || msg_addr[0] || msg_misc[0])
			msg_delimiter = "/";

		pr_auto(ASL6, "ECC CPU%u %s %s%s%s%s%s%s%s%s%s\n",
				raw_smp_processor_id(),
				msg_selstatus,
				msg_overflow, msg_uncorrected, msg_deferred, msg_corrected,
				msg_delimiter,
				msg_serr, msg_er, msg_addr, msg_misc);
#endif
	}

	if (call_panic && is_capable_identifing_err)
		panic("RAS(ECC) error occurred");
}

void dbg_snapshot_ecc_dump(bool call_panic)
{
	switch (read_cpuid_part_number()) {
	case ARM_CPU_PART_CORTEX_A55:
	case ARM_CPU_PART_CORTEX_A76:
	case ARM_CPU_PART_CORTEX_A77:
	case ARM_CPU_PART_CORTEX_A78:
	case ARM_CPU_PART_CORTEX_X1:
	case ARM_CPU_PART_KLEIN:
	case ARM_CPU_PART_MATTERHORN:
	case ARM_CPU_PART_MATTERHORN_ELP:
		_dbg_snapshot_ecc_dump(call_panic);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(dbg_snapshot_ecc_dump);

static inline void dbg_snapshot_save_core(struct pt_regs *regs)
{
	unsigned int cpu = raw_smp_processor_id();
	struct pt_regs *core_reg = *per_cpu_ptr(dss_core_reg, cpu);

	if (!core_reg) {
		pr_err("Core reg is null\n");
		return;
	}
	if (!regs) {
		asm volatile ("str x0, [%0, #0]\n\t"
				"mov x0, %0\n\t"
				"stp x1, x2, [x0, #0x8]\n\t"
				"stp x3, x4, [x0, #0x18]\n\t"
				"stp x5, x6, [x0, #0x28]\n\t"
				"stp x7, x8, [x0, #0x38]\n\t"
				"stp x9, x10, [x0, #0x48]\n\t"
				"stp x11, x12, [x0, #0x58]\n\t"
				"stp x13, x14, [x0, #0x68]\n\t"
				"stp x15, x16, [x0, #0x78]\n\t"
				"stp x17, x18, [x0, #0x88]\n\t"
				"stp x19, x20, [x0, #0x98]\n\t"
				"stp x21, x22, [x0, #0xa8]\n\t"
				"stp x23, x24, [x0, #0xb8]\n\t"
				"stp x25, x26, [x0, #0xc8]\n\t"
				"stp x27, x28, [x0, #0xd8]\n\t"
				"stp x29, x30, [x0, #0xe8]\n\t" :
				: "r"(core_reg));
		core_reg->sp = core_reg->regs[29];
		core_reg->pc =
			(unsigned long)(core_reg->regs[30] - sizeof(unsigned int));
	} else {
		memcpy(core_reg, regs, sizeof(struct user_pt_regs));
	}

	dev_emerg(dss_desc.dev, "core register saved(CPU:%d)\n", cpu);
}

static void dbg_snapshot_save_context(struct pt_regs *regs, bool stack_dump)
{
	int cpu = raw_smp_processor_id();
	unsigned long flags;

	if (!dbg_snapshot_get_enable())
		return;

	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);

	/* If it was already saved the context information, it should be skipped */
	if (dbg_snapshot_get_core_panic_stat(cpu) !=  DSS_SIGN_PANIC) {
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_PANIC, cpu);
		dbg_snapshot_save_system(NULL);
		dbg_snapshot_save_core(regs);
		dbg_snapshot_ecc_dump(false);
		dev_emerg(dss_desc.dev, "context saved(CPU:%d)\n", cpu);
		set_bit(cpu, cpumask_bits(&cpu_dss_context_saved_mask));
	} else
		dev_emerg(dss_desc.dev, "skip context saved(CPU:%d)\n", cpu);

	if (stack_dump)
		dump_stack();

	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);

	flush_cache_all();
}

static void dbg_snapshot_dump_panic(char *str, size_t len)
{
	/*  This function is only one which runs in panic funcion */
	if (str && len && len < SZ_512)
		memcpy(dbg_snapshot_get_header_vaddr() + DSS_OFFSET_PANIC_STRING,
				str, len);
}

static int dbg_snapshot_pre_panic_handler(struct notifier_block *nb,
					  unsigned long l, void *buf)
{
	static int in_panic;
	static int cpu = PANIC_CPU_INVALID;

	if (is_console_locked())
		console_unlock();

	dbg_snapshot_report_reason(DSS_SIGN_PANIC);

	if (in_panic++ && cpu == raw_smp_processor_id()) {
		dev_err(dss_desc.dev, "Possible infinite panic\n");
		dbg_snapshot_expire_watchdog();
	}

	cpu = raw_smp_processor_id();

	return 0;
}

static int dbg_snapshot_post_panic_handler(struct notifier_block *nb,
					   unsigned long l, void *buf)
{
	unsigned long cpu;

	if (!dbg_snapshot_get_enable())
		return 0;

	/* Again disable log_kevents */
	dbg_snapshot_set_item_enable("log_kevents", false);
	dbg_snapshot_dump_panic(buf, strlen((char *)buf));
	for_each_possible_cpu(cpu) {
		if (cpu_is_offline(cpu))
			dbg_snapshot_set_core_power_stat(DSS_SIGN_DEAD, cpu);
		else
			dbg_snapshot_set_core_power_stat(DSS_SIGN_ALIVE, cpu);
	}

	dbg_snapshot_backtrace(NULL, buf);
	dbg_snapshot_dump_task_info();
	dbg_snapshot_output();
	dbg_snapshot_log_output();
	dbg_snapshot_print_log_report();
	dbg_snapshot_save_context(NULL, false);

	if (dss_desc.panic_to_wdt ||
		(num_active_cpus() != cpumask_weight(&cpu_dss_context_saved_mask))) {
		pr_warn("Watchdog reset triggered due to secondary CPUs lock up\n");
		pr_warn("CPUs context saved %*pbl | active CPUs %*pbl\n",
						cpumask_pr_args(&cpu_dss_context_saved_mask),
						cpumask_pr_args(cpu_active_mask));

		dbg_snapshot_expire_watchdog();
	}

	return 0;
}

static long probe_kernel_addr(void *dst, const void *src, size_t size)
{
	long ret;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	pagefault_disable();
	ret = __copy_from_user_inatomic(dst, src, size);
	pagefault_enable();
	set_fs(old_fs);

	return ret;
}

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int i, j, nlines;
	u32 *p, data;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	if (!strncmp("PC", name, strlen(name)) || !strncmp("LR", name, strlen(name)))
		pr_info("\n%s: \n", name);
	else
		pr_info("\n%s: %#lx:\n", name, addr);
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		pr_cont("%04lx :", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++, p++) {
			if (probe_kernel_addr(&data, p, sizeof(data)))
				pr_cont(" ********");
			else
				pr_cont(" %08X", data);
		}
		pr_cont("\n");
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	unsigned int i;
	unsigned long flags;
	mm_segment_t fs;

	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);
	fs = get_fs();
	set_fs(KERNEL_DS);

	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}

	set_fs(fs);
	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);
}

static int dbg_snapshot_pre_die_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (user_mode(regs))
		return NOTIFY_DONE;

	dbg_snapshot_save_context(regs, false);
	dbg_snapshot_set_item_enable("log_kevents", false);

	return NOTIFY_DONE;
}

static int dbg_snapshot_post_die_handler(struct notifier_block *nb,
					unsigned long l, void *buf)
{
	struct die_args *args = (struct die_args *)buf;
	struct pt_regs *regs = args->regs;

	if (user_mode(regs))
		return NOTIFY_DONE;

	dbg_snapshot_backtrace(regs, buf);
	show_extra_register_data(regs, 256);

	return NOTIFY_DONE;
}

static int dbg_snapshot_restart_handler(struct notifier_block *nb,
				    unsigned long mode, void *cmd)
{
	int cpu;

	if (!dbg_snapshot_get_enable())
		return NOTIFY_DONE;

	if (dbg_snapshot_get_report_reason() == DSS_SIGN_PANIC)
		return NOTIFY_DONE;

	dev_emerg(dss_desc.dev, "normal reboot starting\n");
	dbg_snapshot_report_reason(DSS_SIGN_NORMAL_REBOOT);
	dbg_snapshot_scratch_clear();
	dev_emerg(dss_desc.dev, "normal reboot done\n");

	/* clear DSS_SIGN_PANIC when normal reboot */
	for_each_possible_cpu(cpu) {
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_RESET, cpu);
	}

	flush_cache_all();

	return NOTIFY_DONE;
}

static struct notifier_block nb_restart_block = {
	.notifier_call = dbg_snapshot_restart_handler,
	.priority = INT_MAX,
};

static struct notifier_block nb_pre_panic_block = {
	.notifier_call = dbg_snapshot_pre_panic_handler,
	.priority = INT_MAX,
};

static struct notifier_block nb_post_panic_block = {
	.notifier_call = dbg_snapshot_post_panic_handler,
	.priority = INT_MIN,
};

static struct notifier_block nb_pre_die_block = {
	.notifier_call = dbg_snapshot_pre_die_handler,
	.priority = INT_MAX,
};

static struct notifier_block nb_post_die_block = {
	.notifier_call = dbg_snapshot_post_die_handler,
	.priority = INT_MIN,
};

void dbg_snapshot_do_dpm_policy(unsigned int policy)
{
	switch(policy) {
	case GO_DEFAULT_ID:
		break;
	case GO_PANIC_ID:
		panic("%pS", return_address(0));
		break;
	case GO_WATCHDOG_ID:
	case GO_S2D_ID:
		if (dbg_snapshot_expire_watchdog())
			panic("WDT rst fail for s2d, wdt device not probed");
		dbg_snapshot_spin_func();
		break;
	case GO_ARRAYDUMP_ID:
		if (dss_soc_ops.run_arraydump)
			dss_soc_ops.run_arraydump();
		break;
	case GO_SCANDUMP_ID:
		if (dss_soc_ops.run_scandump_mode)
			dss_soc_ops.run_scandump_mode();
		break;
	}
}
EXPORT_SYMBOL_GPL(dbg_snapshot_do_dpm_policy);

void dbg_snapshot_register_wdt_ops(void *start, void *expire, void *stop)
{
	if (start)
		dss_soc_ops.start_watchdog = start;
	if (expire)
		dss_soc_ops.expire_watchdog = expire;
	if (stop)
		dss_soc_ops.stop_watchdog = stop;

	dev_info(dss_desc.dev, "Add %s%s%sfuntions from %pS\n",
			start ? "(wdt start) " : "",
			expire ? "(wdt expire), " : "",
			stop ? "(wdt stop) " : "",
			return_address(0));
}
EXPORT_SYMBOL_GPL(dbg_snapshot_register_wdt_ops);

void dbg_snapshot_register_debug_ops(void *halt, void *arraydump,
				    void *scandump, void *set_safe_mode)
{
	if (halt)
		dss_soc_ops.stop_all_cpus = halt;
	if (arraydump)
		dss_soc_ops.run_arraydump = arraydump;
	if (scandump)
		dss_soc_ops.run_scandump_mode = scandump;
	if (set_safe_mode)
		dss_soc_ops.set_safe_mode = set_safe_mode;

	dev_info(dss_desc.dev, "Add %s%s%s%sfuntions from %pS\n",
			halt ? "(halt) " : "",
			arraydump ? "(arraydump) " : "",
			scandump ? "(scandump mode) " : "",
			set_safe_mode ? "(set_safe_mode) " : "",
			return_address(0));
}
EXPORT_SYMBOL_GPL(dbg_snapshot_register_debug_ops);

static void dbg_snapshot_ipi_stop(void *ignore, struct pt_regs *regs)
{
	dbg_snapshot_save_context(regs, true);
}

static inline bool is_event_supported(unsigned int type, unsigned int code)
{
	if (!(dss_desc.hold_key && dss_desc.trigger_key))
		return false;

	return (type == EV_KEY) && (code == dss_desc.hold_key ||
			code == dss_desc.trigger_key);
}

#if !IS_ENABLED(CONFIG_SEC_KEY_NOTIFIER)
static void dbg_snanpshot_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	static bool holdkey_p;
	static int count;
	static ktime_t start;

	if (!is_event_supported(type, code))
		return;

	dev_info(dss_desc.dev, "KEY(%d) %s\n",
			code, value ? "pressed" : "released");
	/* Enter Forced Upload. Hold key first
	 * and then press trigger key twice. Other key should not be pressed.
	 */
	if (code == dss_desc.hold_key)
		holdkey_p = value ? true : false;

	if (!holdkey_p) {
		count = 0;
		holdkey_p = false;
		return;
	}

	if ((code != dss_desc.trigger_key) || !value)
		return;

	if (!count)
		start = ktime_get();
	dev_err(dss_desc.dev, "entering forced upload[%d]\n", ++count);
	if (ktime_ms_delta(ktime_get(), start) > 2 * MSEC_PER_SEC)
		count = 0;

	if (count == 2)
		panic("Crash Key");
}

static int dbg_snanpshot_connect(struct input_handler *handler,
				 struct input_dev *dev,
				 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "dss_input_handler";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

static void dbg_snanpshot_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id dbg_snanpshot_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static struct input_handler dbg_snapshot_input_handler = {
	.event		= dbg_snanpshot_event,
	.connect	= dbg_snanpshot_connect,
	.disconnect	= dbg_snanpshot_disconnect,
	.name		= "dss_input_handler",
	.id_table	= dbg_snanpshot_ids,
};
#endif

static void set_smc_pre_reading_ecc_sysreg(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	const __be32 *cur;
	int count, idx;
	u32 val;

	count = of_property_count_u32_elems(np, "wa-pre-reading-ecc-sysreg");
	if (count != 4)
		return;

	idx = 0;
	of_property_for_each_u32(np, "wa-pre-reading-ecc-sysreg", prop, cur, val)
		smc_pre_reading_ecc_sysreg[idx++] = (unsigned long)val;

	dev_info(dev, "%s:[%lx][%lx][%lx][%lx]\n", __func__, smc_pre_reading_ecc_sysreg[0],
							     smc_pre_reading_ecc_sysreg[1],
							     smc_pre_reading_ecc_sysreg[2],
							     smc_pre_reading_ecc_sysreg[3]);
}

void dbg_snapshot_init_utils(struct device *dev)
{
	int i;
	size_t vaddr = (size_t)dbg_snapshot_get_header_vaddr();

	dss_mmu_reg = alloc_percpu(struct dbg_snapshot_mmu_reg *);
	dss_core_reg = alloc_percpu(struct pt_regs *);
	for_each_possible_cpu(i) {
		*per_cpu_ptr(dss_mmu_reg, i) = (struct dbg_snapshot_mmu_reg *)
					  (vaddr + DSS_HEADER_SZ +
					   i * DSS_MMU_REG_OFFSET);
		*per_cpu_ptr(dss_core_reg, i) = (struct pt_regs *)
					   (vaddr + DSS_HEADER_SZ + DSS_MMU_REG_SZ +
					    i * DSS_CORE_REG_OFFSET);
	}

	set_smc_pre_reading_ecc_sysreg(dev);

	dbg_snapshot_register_backtrace();
	register_die_notifier(&nb_pre_die_block);
	register_die_notifier(&nb_post_die_block);
	register_restart_handler(&nb_restart_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_pre_panic_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_post_panic_block);
	register_trace_android_vh_ipi_stop(dbg_snapshot_ipi_stop, NULL);
#if !IS_ENABLED(CONFIG_SEC_KEY_NOTIFIER)
	input_register_handler(&dbg_snapshot_input_handler);
#endif

	smp_call_function(dbg_snapshot_save_system, NULL, 1);
	dbg_snapshot_save_system(NULL);
}
