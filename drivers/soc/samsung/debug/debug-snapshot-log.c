/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/clk-provider.h>
#include <linux/sched/clock.h>
#include <linux/ftrace.h>
#include <linux/kernel_stat.h>
#include <linux/irqnr.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <asm/stacktrace.h>
#include <soc/samsung/debug-snapshot.h>
#include "debug-snapshot-local.h"

#include <trace/events/power.h>
#include <trace/events/timer.h>
#include <trace/events/workqueue.h>
#include <trace/events/irq.h>
#include <trace/events/sched.h>
#include <trace/events/rwmmio.h>

struct dbg_snapshot_log_item dss_log_items[] = {
	[DSS_LOG_TASK_ID]	= {DSS_LOG_TASK,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_WORK_ID]	= {DSS_LOG_WORK,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_CPUIDLE_ID]	= {DSS_LOG_CPUIDLE,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_SUSPEND_ID]	= {DSS_LOG_SUSPEND,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_IRQ_ID]	= {DSS_LOG_IRQ,		{0, 0, 0, false}, 0, false},
	[DSS_LOG_HRTIMER_ID]	= {DSS_LOG_HRTIMER,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_REG_ID]	= {DSS_LOG_REG,		{0, 0, 0, false}, 0, false},
	[DSS_LOG_CLK_ID]	= {DSS_LOG_CLK,		{0, 0, 0, false}, 0, false},
	[DSS_LOG_PMU_ID]	= {DSS_LOG_PMU,		{0, 0, 0, false}, 0, false},
	[DSS_LOG_FREQ_ID]	= {DSS_LOG_FREQ,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_DM_ID]		= {DSS_LOG_DM,		{0, 0, 0, false}, 0, false},
	[DSS_LOG_REGULATOR_ID]	= {DSS_LOG_REGULATOR,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_THERMAL_ID]	= {DSS_LOG_THERMAL,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_ACPM_ID]	= {DSS_LOG_ACPM,	{0, 0, 0, false}, 0, false},
	[DSS_LOG_PRINTK_ID]	= {DSS_LOG_PRINTK,	{0, 0, 0, false}, 0, false},
};

struct dbg_snapshot_log_misc dss_log_misc;
static char dss_freq_name[SZ_32][SZ_8];
static unsigned int dss_freq_size = 0;

struct dbg_snapshot_log_item *dbg_snapshot_get_log_item(char *name)
{
	struct dbg_snapshot_log_item *log_item;
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(dss_log_items); i++) {
		log_item = &dss_log_items[i];
		if (log_item->name && !strcmp(log_item->name, name)) {
			return log_item;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_get_log_item);

#define dss_get_log(item)						\
long dss_get_sizeof_##item##_log(void) {				\
	return sizeof(struct item##_log);				\
}									\
EXPORT_SYMBOL_GPL(dss_get_sizeof_##item##_log);				\
long dss_get_len_##item##_log(void) {					\
	return (long)dbg_snapshot_get_log_item(#item"_log")->log_num;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_len_##item##_log);				\
long dss_get_last_##item##_log_idx(void) {				\
	return (atomic_read(&dss_log_misc.item##_log_idx) - 1) %	\
			dss_get_len_##item##_log();			\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_##item##_log_idx);			\
long dss_get_first_##item##_log_idx(void) {				\
	int v = atomic_read(&dss_log_misc.item##_log_idx);		\
	return v < dss_get_len_##item##_log() ? 0 :			\
			v % dss_get_len_##item##_log();			\
}									\
EXPORT_SYMBOL_GPL(dss_get_first_##item##_log_idx);			\
struct item##_log *dss_get_last_##item##_log(void) {			\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	return &entry[dss_get_last_##item##_log_idx()];			\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_##item##_log);				\
unsigned long dss_get_last_paddr_##item##_log(void) {			\
	long last_idx = dss_get_last_##item##_log_idx();		\
	struct dbg_snapshot_log_item *log_item = 			\
			dbg_snapshot_get_log_item(#item"_log");		\
	return log_item ? log_item->entry.paddr +			\
			(last_idx * sizeof(struct item##_log)) : 0;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_paddr_##item##_log);			\
struct item##_log *dss_get_first_##item##_log(void) {			\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	return &entry[dss_get_first_##item##_log_idx()];		\
}									\
EXPORT_SYMBOL_GPL(dss_get_first_##item##_log);				\
struct item##_log *dss_get_##item##_log_by_idx(int idx) {		\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (idx < 0 || idx >= dss_get_len_##item##_log())		\
		return NULL;						\
	return &entry[idx];						\
}									\
EXPORT_SYMBOL_GPL(dss_get_##item##_log_by_idx);				\
struct item##_log *dss_get_##item##_log_iter(int idx) {			\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (!(idx % dss_get_len_##item##_log()))			\
		idx = 0;							\
	else if (idx < 0)						\
		idx = dss_get_len_##item##_log() -			\
			(abs(idx) % dss_get_len_##item##_log());	\
	else								\
		idx = idx % dss_get_len_##item##_log();			\
	return &entry[idx];						\
}									\
EXPORT_SYMBOL_GPL(dss_get_##item##_log_iter);				\
unsigned long dss_get_vaddr_##item##_log(void) {			\
	return (unsigned long)						\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_vaddr_##item##_log)

#define dss_get_log_by_cpu(item)					\
long dss_get_sizeof_##item##_log(void) {				\
	return sizeof(struct item##_log);				\
}									\
EXPORT_SYMBOL_GPL(dss_get_sizeof_##item##_log);				\
long dss_get_len_##item##_log(void) {					\
	return (long)dbg_snapshot_get_log_item(#item"_log")->arr_num;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_len_##item##_log);				\
long dss_get_len_##item##_log_by_cpu(int cpu) {				\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return (long)dbg_snapshot_get_log_item(#item"_log")->log_num;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_len_##item##_log_by_cpu);			\
long dss_get_last_##item##_log_idx(int cpu) {				\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return (atomic_read(&dss_log_misc.item##_log_idx[cpu]) - 1) %	\
			dss_get_len_##item##_log_by_cpu(cpu);		\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_##item##_log_idx);			\
long dss_get_first_##item##_log_idx(int cpu) {				\
	int v = atomic_read(&dss_log_misc.item##_log_idx[cpu]);		\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return -EINVAL;						\
	return v < dss_get_len_##item##_log_by_cpu(cpu) ? 0 :		\
		v % dss_get_len_##item##_log_by_cpu(cpu);		\
}									\
EXPORT_SYMBOL_GPL(dss_get_first_##item##_log_idx);			\
struct item##_log *dss_get_last_##item##_log(int cpu) {			\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	entry = &entry[dss_get_len_##item##_log_by_cpu(cpu) * cpu];	\
	return &entry[dss_get_last_##item##_log_idx(cpu)];		\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_##item##_log);				\
struct item##_log *dss_get_first_##item##_log(int cpu) {		\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	entry = &entry[dss_get_len_##item##_log_by_cpu(cpu) * cpu];	\
	return &entry[dss_get_first_##item##_log_idx(cpu)];		\
}									\
EXPORT_SYMBOL_GPL(dss_get_first_##item##_log);				\
unsigned long dss_get_last_paddr_##item##_log(int cpu) {		\
	long last_idx = dss_get_last_##item##_log_idx(cpu);		\
	struct dbg_snapshot_log_item *log_item = 			\
			dbg_snapshot_get_log_item(#item"_log");		\
	last_idx += cpu * dss_get_len_##item##_log_by_cpu(cpu);		\
	return log_item ? log_item->entry.paddr +			\
			(last_idx * dss_get_sizeof_##item##_log()) : 0;	\
}									\
EXPORT_SYMBOL_GPL(dss_get_last_paddr_##item##_log);			\
struct item##_log *dss_get_##item##_log_by_idx(int cpu, int idx) {	\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	if (idx < 0 || idx >= dss_get_len_##item##_log_by_cpu(cpu))	\
		return NULL;						\
	entry = &entry[dss_get_len_##item##_log_by_cpu(cpu) * cpu];	\
	return &entry[idx];						\
}									\
EXPORT_SYMBOL_GPL(dss_get_##item##_log_by_idx);				\
struct item##_log *dss_get_##item##_log_by_cpu_iter(int cpu, int idx) {	\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return NULL;						\
	entry = &entry[dss_get_len_##item##_log_by_cpu(cpu) * cpu];	\
	if (!(idx % dss_get_len_##item##_log_by_cpu(cpu)))		\
		idx = 0;						\
	else if (idx < 0)						\
		idx = dss_get_len_##item##_log_by_cpu(cpu) -		\
		(abs(idx) % dss_get_len_##item##_log_by_cpu(cpu));	\
	else								\
		idx = idx % dss_get_len_##item##_log_by_cpu(cpu);	\
	return &entry[idx];						\
}									\
EXPORT_SYMBOL_GPL(dss_get_##item##_log_by_cpu_iter);			\
unsigned long dss_get_vaddr_##item##_log_by_cpu(int cpu) {		\
	struct item##_log *entry = (struct item##_log *)		\
		dbg_snapshot_get_log_item(#item"_log")->entry.vaddr;	\
	if (cpu < 0 || cpu >= dss_get_len_##item##_log())		\
		return 0;						\
	entry = &entry[dss_get_len_##item##_log_by_cpu(cpu) * cpu];	\
	return (unsigned long)entry;					\
}									\
EXPORT_SYMBOL_GPL(dss_get_vaddr_##item##_log_by_cpu)

#ifdef CONFIG_DEBUG_SNAPSHOT_API
dss_get_log_by_cpu(task);
dss_get_log_by_cpu(work);
dss_get_log_by_cpu(cpuidle);
dss_get_log_by_cpu(freq);
dss_get_log(suspend);
dss_get_log_by_cpu(irq);
dss_get_log_by_cpu(hrtimer);
dss_get_log_by_cpu(reg);
dss_get_log(clk);
dss_get_log(pmu);
dss_get_log(dm);
dss_get_log(regulator);
dss_get_log(acpm);
dss_get_log(print);
#endif
dss_get_log(thermal);

static inline bool dbg_snapshot_is_log_item_enabled(int id)
{
	bool item_enabled = dbg_snapshot_get_item_enable(DSS_ITEM_KEVENTS);
	bool log_enabled = dss_log_items[id].entry.enabled;

	return dbg_snapshot_get_enable() && item_enabled && log_enabled;
}

void *dbg_snapshot_log_get_item_by_index(int index)
{
	if (index < 0 || index > ARRAY_SIZE(dss_log_items))
		return NULL;

	return  &dss_log_items[index];
}
EXPORT_SYMBOL_GPL(dbg_snapshot_log_get_item_by_index);

int dbg_snapshot_log_get_num_items(void)
{
	return ARRAY_SIZE(dss_log_items);
}
EXPORT_SYMBOL_GPL(dbg_snapshot_log_get_num_items);

int dbg_snapshot_get_freq_idx(const char *name)
{
	unsigned int i;

	for (i = 0; i < dss_freq_size; i++) {
		if (!strncmp(dss_freq_name[i], name, strlen(name)))
			return i;
	}

	return -EFAULT;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_get_freq_idx);

char *dbg_snapshot_get_freq_name(int idx, char *dest, size_t dest_size)
{
	if (idx >= dss_freq_size || sizeof(dss_freq_name[0]) > dest_size)
		return NULL;

	strncpy(dest, dss_freq_name[idx], sizeof(dss_freq_name[0]));
	return dest;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_get_freq_name);

void dbg_snapshot_log_output(void)
{
	unsigned long i;

	pr_info("debug-snapshot-log physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(dss_log_items); i++) {
		if (dss_log_items[i].entry.enabled)
			pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx / en:%d\n",
				dss_log_items[i].name,
				dss_log_items[i].entry.paddr,
				dss_log_items[i].entry.vaddr,
				dss_log_items[i].entry.size,
				dss_log_items[i].entry.enabled);
	}
}

void dbg_snapshot_set_policy_log_item(const char *name, bool en)
{
	struct dbg_snapshot_log_item *log_item;
	int i;

	if (!name)
		return;

	for (i = 0; i < (int)ARRAY_SIZE(dss_log_items); i++) {
		log_item = &dss_log_items[i];
		if (log_item->name && !strcmp(log_item->name, name)) {
			log_item->policy = en;
			pr_info("log item policy - %s is %sabled\n", name, en ? "en" : "dis");
			break;
		}
	}
}

static bool dbg_snapshot_get_enable_log_item(const char *name)
{
	struct dbg_snapshot_log_item *log_item;
	int i;

	if (!name)
		return false;

	for (i = 0; i < ARRAY_SIZE(dss_log_items); i++) {
		log_item = &dss_log_items[i];
		if (log_item->name &&
				!strncmp(log_item->name, name, strlen(name))) {
			return log_item->entry.enabled;
		}
	}

	return false;
}

static void dbg_snapshot_task(int cpu, struct task_struct *task)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_TASK_ID];
	struct task_log *entry = (struct task_log *)log_item->entry.vaddr;

	idx = (atomic_fetch_inc(&dss_log_misc.task_log_idx[cpu]) % log_item->log_num) +
								(cpu * log_item->log_num);
	entry[idx].time = local_clock();
	entry[idx].task = task;
	entry[idx].pid = task_pid_nr(task);
	strncpy(entry[idx].task_comm, task->comm, TASK_COMM_LEN - 1);
}

static void dbg_snapshot_sched_switch(void *ignore, bool preempt,
				      struct task_struct *prev,
				      struct task_struct *next)
{
	dbg_snapshot_task(raw_smp_processor_id(), next);
}

static void dbg_snapshot_work(work_func_t fn, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_WORK_ID];
	struct work_log *entry = (struct work_log *)log_item->entry.vaddr;
	int cpu = raw_smp_processor_id();

	idx = (atomic_fetch_inc(&dss_log_misc.work_log_idx[cpu]) % log_item->log_num) +
								(cpu * log_item->log_num);
	entry[idx].time = local_clock();
	entry[idx].fn = fn;
	entry[idx].en = en;
}

static void dbg_snapshot_wq_start(void *ignore, struct work_struct *work)
{
	dbg_snapshot_work(work->func, DSS_FLAG_IN);
}

static void dbg_snapshot_wq_end(void *ignore, struct work_struct *work,
				work_func_t func)
{
	dbg_snapshot_work(func, DSS_FLAG_OUT);
}

void dbg_snapshot_cpuidle(char *modes, unsigned int state, s64 diff, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_CPUIDLE_ID];
	struct cpuidle_log *entry = (struct cpuidle_log *)log_item->entry.vaddr;
	int cpu = raw_smp_processor_id();

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_CPUIDLE_ID))
		return;

	idx = (atomic_fetch_inc(&dss_log_misc.cpuidle_log_idx[cpu]) % log_item->log_num) +
									(cpu * log_item->log_num);
	entry[idx].time = cpu_clock(cpu);
	entry[idx].modes = modes;
	entry[idx].state = state;
	entry[idx].num_online_cpus = num_online_cpus();
	entry[idx].delta = (int)diff;
	entry[idx].en = en;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_cpuidle);

static void dbg_snapshot_irq(int irq, void *fn, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_IRQ_ID];
	struct irq_log *entry = (struct irq_log *)log_item->entry.vaddr;
	unsigned long flags = arch_local_irq_save();
	int cpu = raw_smp_processor_id();

	idx = (atomic_fetch_inc(&dss_log_misc.irq_log_idx[cpu]) % log_item->log_num) +
								(cpu * log_item->log_num);
	entry[idx].time = local_clock();
	entry[idx].irq = irq;
	entry[idx].fn = fn;
	entry[idx].desc = irq_to_desc(irq);
	entry[idx].en = en;
#if IS_ENABLED(CONFIG_SEC_DEBUG)
	entry[idx].latency = READ_ONCE(current_thread_info()->preempt_count);
#endif

	arch_local_irq_restore(flags);
}

static void dbg_snapshot_irq_entry(void *ignore, int irq,
				   struct irqaction *action)
{
	dbg_snapshot_irq(irq, action->handler, DSS_FLAG_IN);
}

static void dbg_snapshot_irq_exit(void *ignore, int irq,
				   struct irqaction *action, int ret)
{
	dbg_snapshot_irq(irq, action->handler, DSS_FLAG_OUT);
}

static void dbg_snapshot_hrtimer(struct hrtimer *timer, s64 now,
				 void *fn, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_HRTIMER_ID];
	struct hrtimer_log *entry = (struct hrtimer_log *)log_item->entry.vaddr;
	int cpu = raw_smp_processor_id();

	idx = (atomic_fetch_inc(&dss_log_misc.hrtimer_log_idx[cpu]) % log_item->log_num) +
									(cpu * log_item->log_num);
	entry[idx].time = local_clock();
	entry[idx].now = now;
	entry[idx].timer = timer;
	entry[idx].fn = fn;
	entry[idx].en = en;
}

static void dbg_snapshot_hrtimer_entry(void *ignore, struct hrtimer *timer,
				       ktime_t *now)
{
	dbg_snapshot_hrtimer(timer, *now, timer->function, DSS_FLAG_IN);
}

static void dbg_snapshot_hrtimer_exit(void *ignore, struct hrtimer *timer)
{
	dbg_snapshot_hrtimer(timer, 0, timer->function, DSS_FLAG_OUT);
}

__maybe_unused static void dbg_snapshot_reg(unsigned char io_type,
					unsigned int data_type,
					unsigned long long val, void *addr,
					unsigned long fn)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_REG_ID];
	struct reg_log *entry = (struct reg_log *)log_item->entry.vaddr;
	int cpu = raw_smp_processor_id();

	idx = (atomic_fetch_inc(&dss_log_misc.reg_log_idx[cpu]) % log_item->log_num) +
								(cpu * log_item->log_num);
	entry[idx].time = local_clock();
	entry[idx].io_type = io_type;
	entry[idx].data_type = data_type;
	entry[idx].val = val;
	entry[idx].addr = addr;
	entry[idx].caller = (void *)fn;
}

__maybe_unused static void dbg_snapshot_reg_write(void *ignore,
						unsigned long fn,
						u64 val, u8 width,
						volatile void __iomem *addr)
{
	dbg_snapshot_reg('w', width, val, (void *)addr, fn);
}

__maybe_unused static void dbg_snapshot_reg_read(void *ignore,
					unsigned long fn, u8 width,
					const volatile void __iomem *addr)
{
	dbg_snapshot_reg('r', width, 0, (void *)addr, fn);
}

__maybe_unused static void dbg_snapshot_reg_post_read(void *ignore,
					unsigned long fn, u64 val, u8 width,
					const volatile void __iomem *addr)
{
	dbg_snapshot_reg('R', width, val, (void *)addr, fn);
}

static void dbg_snapshot_suspend(const char *log, struct device *dev,
				int event, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_SUSPEND_ID];
	struct suspend_log *entry = (struct suspend_log *)log_item->entry.vaddr;

	idx = atomic_fetch_inc(&dss_log_misc.suspend_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].log = log ? log : NULL;
	entry[idx].event = event;
	entry[idx].dev = dev ? dev_name(dev) : "";
	entry[idx].core = raw_smp_processor_id();
	entry[idx].en = en;
}

static void dbg_snapshot_suspend_resume(void *ignore, const char *action,
					int event, bool start)
{
	dbg_snapshot_suspend(action, NULL, event,
			start ? DSS_FLAG_IN : DSS_FLAG_OUT);
}

static void dbg_snapshot_dev_pm_cb_start(void *ignore, struct device *dev,
					const char *info, int event)
{
	dbg_snapshot_suspend(info, dev, event, DSS_FLAG_IN);
}

static void dbg_snapshot_dev_pm_cb_end(void *ignore, struct device *dev,
					int error)
{
	dbg_snapshot_suspend(NULL, dev, error, DSS_FLAG_OUT);
}

void dbg_snapshot_regulator(unsigned long long timestamp, char* f_name,
			unsigned int addr, unsigned int volt,
			unsigned int rvolt, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_REGULATOR_ID];
	struct regulator_log *entry = (struct regulator_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_REGULATOR_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.regulator_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].cpu = raw_smp_processor_id();
	entry[idx].acpm_time = timestamp;
	strncpy(entry[idx].name, f_name, min_t(size_t, strlen(f_name), SZ_16 - 1));
	entry[idx].reg = addr;
	entry[idx].en = en;
	entry[idx].voltage = volt;
	entry[idx].raw_volt = rvolt;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_regulator);

void dbg_snapshot_thermal(void *data, unsigned int temp, char *name,
			unsigned long long max_cooling)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_THERMAL_ID];
	struct thermal_log *entry = (struct thermal_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_THERMAL_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.thermal_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].cpu = raw_smp_processor_id();
	entry[idx].data = (struct exynos_tmu_data *)data;
	entry[idx].temp = temp;
	entry[idx].cooling_device = name;
	entry[idx].cooling_state = max_cooling;

}
EXPORT_SYMBOL_GPL(dbg_snapshot_thermal);

void dbg_snapshot_clk(void *clock, const char *func_name, unsigned long arg, int mode)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_CLK_ID];
	struct clk_log *entry = (struct clk_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_CLK_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.clk_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].mode = mode;
	entry[idx].arg = arg;
	entry[idx].clk = (struct clk_hw *)clock;
	entry[idx].f_name = func_name;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_clk);

void dbg_snapshot_pmu(int id, const char *func_name, int mode)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_PMU_ID];
	struct pmu_log *entry = (struct pmu_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_PMU_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.pmu_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].mode = mode;
	entry[idx].id = id;
	entry[idx].f_name = func_name;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_pmu);

void dbg_snapshot_freq(int type, unsigned int old_freq,
			unsigned int target_freq, int en)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_FREQ_ID];
	struct freq_log *entry = (struct freq_log *)log_item->entry.vaddr;

	if (unlikely(type < 0 || type > dss_freq_size))
		return;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_FREQ_ID))
		return;

	idx = (atomic_fetch_inc(&dss_log_misc.freq_log_idx[type]) % log_item->log_num) +
								(type * log_item->log_num);

	entry[idx].time = local_clock();
	entry[idx].cpu = raw_smp_processor_id();
	entry[idx].old_freq = old_freq;
	entry[idx].target_freq = target_freq;
	entry[idx].en = en;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_freq);

void dbg_snapshot_dm(int type, unsigned int min, unsigned int max,
			s32 wait_t, s32 t)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_DM_ID];
	struct dm_log *entry = (struct dm_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_DM_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.dm_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].cpu = raw_smp_processor_id();
	entry[idx].dm_num = type;
	entry[idx].min_freq = min;
	entry[idx].max_freq = max;
	entry[idx].wait_dmt = wait_t;
	entry[idx].do_dmt = t;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_dm);

void dbg_snapshot_acpm(unsigned long long timestamp, const char *log,
			unsigned int data)
{
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_ACPM_ID];
	struct acpm_log *entry = (struct acpm_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_ACPM_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.acpm_log_idx) % log_item->log_num;

	entry[idx].time = local_clock();
	entry[idx].acpm_time = timestamp;
	strncpy(entry[idx].log, log, min_t(size_t, strlen(log), 8));
	entry[idx].data = data;
}
EXPORT_SYMBOL_GPL(dbg_snapshot_acpm);

void dbg_snapshot_printk(const char *fmt, ...)
{
	va_list args;
	unsigned long idx;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_PRINTK_ID];
	struct print_log *entry = (struct print_log *)log_item->entry.vaddr;

	if (!dbg_snapshot_is_log_item_enabled(DSS_LOG_PRINTK_ID))
		return;

	idx = atomic_fetch_inc(&dss_log_misc.print_log_idx) % log_item->log_num;

	va_start(args, fmt);
	vsnprintf(entry[idx].log, sizeof(entry[idx].log), fmt, args);
	va_end(args);

	entry[idx].time = local_clock();
	entry[idx].cpu = raw_smp_processor_id();
}
EXPORT_SYMBOL_GPL(dbg_snapshot_printk);

static inline void dbg_snapshot_get_sec(unsigned long long ts,
					unsigned long *sec,
					unsigned long *msec)
{
	*sec = ts / NSEC_PER_SEC;
	*msec = (ts % NSEC_PER_SEC) / USEC_PER_MSEC;
}

static void dbg_snapshot_print_last_irq(int cpu)
{
	unsigned long idx, sec, msec;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_IRQ_ID];
	struct irq_log *entry = (struct irq_log *)log_item->entry.vaddr;

	if (!log_item->entry.enabled)
		return;

	idx = ((atomic_read(&dss_log_misc.irq_log_idx[cpu]) - 1) % log_item->log_num) +
								(cpu * log_item->log_num);
	dbg_snapshot_get_sec(entry[idx].time, &sec, &msec);

	pr_info("%-12s: [%4ld] %10lu.%06lu sec, %10s: %pS, %8s: %8d, %10s: %2d, %s\n",
			">>> irq", idx % log_item->log_num, sec, msec,
			"handler", entry[idx].fn,
			"irq", entry[idx].irq,
			"en", entry[idx].en,
			(entry[idx].en == 1) ? "[Mismatch]" : "");
}

static void dbg_snapshot_print_last_task(int cpu)
{
	unsigned long idx, sec, msec;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_TASK_ID];
	struct task_log *entry = (struct task_log *)log_item->entry.vaddr;
	struct task_struct *task;

	if (!log_item->entry.enabled)
		return;

	idx = ((atomic_read(&dss_log_misc.task_log_idx[cpu]) - 1) % log_item->log_num) +
								(cpu * log_item->log_num);
	dbg_snapshot_get_sec(entry[idx].time, &sec, &msec);
	task = entry[idx].task;

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %-16s, %8s: 0x%-16px, %10s: %16llu\n",
			">>> task", idx % log_item->log_num, sec, msec,
			"task_comm", (task) ? task->comm : "NULL",
			"task", task,
			"exec_start", (task) ? task->se.exec_start : 0);
}

static void dbg_snapshot_print_last_work(int cpu)
{
	unsigned long idx, sec, msec;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_WORK_ID];
	struct work_log *entry = (struct work_log *)log_item->entry.vaddr;

	if (!log_item->entry.enabled)
		return;

	idx = ((atomic_read(&dss_log_misc.work_log_idx[cpu]) - 1) % log_item->log_num) +
								(cpu * log_item->log_num);
	dbg_snapshot_get_sec(entry[idx].time, &sec, &msec);

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %pS, %3s: %3d %s\n",
			">>> work", idx % log_item->log_num, sec, msec,
			"work_fn", entry[idx].fn,
			"en", entry[idx].en,
			(entry[idx].en == 1) ? "[Mismatch]" : "");
}

static void dbg_snapshot_print_last_cpuidle(int cpu)
{
	unsigned long idx, sec, msec;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_CPUIDLE_ID];
	struct cpuidle_log *entry = (struct cpuidle_log *)log_item->entry.vaddr;

	if (!log_item->entry.enabled)
		return;

	idx = ((atomic_read(&dss_log_misc.cpuidle_log_idx[cpu]) - 1) % log_item->log_num) +
									(cpu * log_item->log_num);
	dbg_snapshot_get_sec(entry[idx].time, &sec, &msec);

	pr_info("%-12s: [%4lu] %10lu.%06lu sec, %10s: %s, %6s: %2d, %10s: %10d"
			", %12s: %2d, %3s: %3d %s\n",
			">>> cpuidle", idx % log_item->log_num, sec, msec,
			"modes", entry[idx].modes,
			"state", entry[idx].state,
			"stay time", entry[idx].delta,
			"online_cpus", entry[idx].num_online_cpus,
			"en", entry[idx].en,
			(entry[idx].en == 1) ? "[Mismatch]" : "");
}

static void dbg_snapshot_print_lastinfo(void)
{
	int cpu;

	pr_info("<last info>\n");
	for (cpu = 0; cpu < DSS_NR_CPUS; cpu++) {
		pr_info("CPU ID: %d ----------------------------------\n", cpu);
		dbg_snapshot_print_last_task(cpu);
		dbg_snapshot_print_last_work(cpu);
		dbg_snapshot_print_last_irq(cpu);
		dbg_snapshot_print_last_cpuidle(cpu);
	}
}

static void dbg_snapshot_print_freqinfo(void)
{
	unsigned int i;
	struct dbg_snapshot_log_item *log_item = &dss_log_items[DSS_LOG_FREQ_ID];
	struct freq_log *entry = (struct freq_log *)log_item->entry.vaddr;

	if (!log_item->entry.enabled)
		return;

	pr_info("\n<last freq info>\n");
	for (i = 0; i < dss_freq_size; i++) {
		unsigned long idx, sec, msec;
		unsigned long old_freq, target_freq;

		if (!atomic_read(&dss_log_misc.freq_log_idx[i])) {
			pr_info("%10s: no infomation\n", dss_freq_name[i]);
			continue;
		}
		idx = ((atomic_read(&dss_log_misc.freq_log_idx[i]) - 1) % log_item->log_num) +
									(i * log_item->log_num);
		dbg_snapshot_get_sec(entry[idx].time, &sec, &msec);
		old_freq = entry[idx].old_freq;
		target_freq = entry[idx].target_freq;
		pr_info("%10s[%4lu]: %10lu.%06lu sec, %12s: %5luMhz, %12s: %5luMhz, %3s: %d %s\n",
				dss_freq_name[i], idx % log_item->log_num, sec, msec,
				"old_freq", old_freq / 1000,
				"target_freq", target_freq / 1000,
				"en", entry[idx].en,
				(entry[idx].en == 1) ? "[Mismatch]" : "");
	}
}

#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

static void dbg_snapshot_print_irq(void)
{
	int i, cpu;
	u64 sum = 0;

	for_each_possible_cpu(cpu)
		sum += kstat_cpu_irqs_sum(cpu);

	sum += arch_irq_stat();

	pr_info("<irq info>\n");
	pr_info("----------------------------------------------------------\n");
	pr_info("sum irq : %llu", sum);
	pr_info("----------------------------------------------------------\n");

	for_each_irq_nr(i) {
		struct irq_desc *desc = irq_to_desc(i);
		unsigned int irq_stat = 0;
		const char *name;

		if (!desc)
			continue;

		for_each_possible_cpu(cpu)
			irq_stat += *per_cpu_ptr(desc->kstat_irqs, cpu);

		if (!irq_stat)
			continue;

		if (desc->action && desc->action->name)
			name = desc->action->name;
		else
			name = "???";
		pr_info("irq-%-4d(hwirq-%-3d) : %8u %s\n",
			i, (int)desc->irq_data.hwirq, irq_stat, name);
	}
}

void dbg_snapshot_print_log_report(void)
{
	if (unlikely(!dbg_snapshot_get_enable()))
		return;

	pr_info("==========================================================\n");
	pr_info("Panic Report\n");
	pr_info("==========================================================\n");
	dbg_snapshot_print_lastinfo();
	dbg_snapshot_print_freqinfo();
	dbg_snapshot_print_irq();
	pr_info("==========================================================\n");
}

#define _log_item_set_field(log_item, item, dss_log_ptr, log_name)			\
	log_item->entry.vaddr = (size_t)(dss_log_ptr->log_name);			\
	log_item->entry.paddr = item->entry.paddr +					\
		(size_t)(dss_log_ptr->log_name) - (size_t)(dss_log_ptr);		\
	log_item->entry.size = sizeof(dss_log_ptr->log_name);				\
											\
	if (!log_item->entry.paddr || !log_item->entry.vaddr ||				\
			!log_item->entry.size || !log_item->policy)			\
		log_item->entry.enabled = false;					\
	else										\
		log_item->entry.enabled = true

#define log_item_set_array_field(dss_log_type, id, log_name)				\
{											\
	struct dbg_snapshot_log_item *log_item;						\
	struct dbg_snapshot_item *item = dbg_snapshot_get_item(DSS_ITEM_KEVENTS);	\
	dss_log_type *dss_log_ptr = (dss_log_type *)item->entry.vaddr;			\
											\
	log_item = &dss_log_items[DSS_LOG_##id##_ID];					\
	_log_item_set_field(log_item, item, dss_log_ptr, log_name);			\
	log_item->arr_num = (int)ARRAY_SIZE(dss_log_ptr->log_name);			\
	log_item->log_num = (int)ARRAY_SIZE(dss_log_ptr->log_name[0]);			\
}

#define log_item_set_field(dss_log_type, id, log_name)					\
{											\
	struct dbg_snapshot_log_item *log_item;						\
	struct dbg_snapshot_item *item = dbg_snapshot_get_item(DSS_ITEM_KEVENTS);	\
	dss_log_type *dss_log_ptr = (dss_log_type *)item->entry.vaddr;			\
											\
	log_item = &dss_log_items[DSS_LOG_##id##_ID];					\
	_log_item_set_field(log_item, item, dss_log_ptr, log_name);			\
	log_item->log_num = (int)ARRAY_SIZE(dss_log_ptr->log_name);			\
}

static void dbg_snapshot_set_log_item_field(void)
{
	switch (dss_kevents.type) {
	case eDSS_LOG_TYPE_DEFAULT:
		log_item_set_array_field(struct dbg_snapshot_log, TASK, task);
		log_item_set_array_field(struct dbg_snapshot_log, WORK, work);
		log_item_set_array_field(struct dbg_snapshot_log, CPUIDLE, cpuidle);
		log_item_set_field(struct dbg_snapshot_log, SUSPEND, suspend);
		log_item_set_array_field(struct dbg_snapshot_log, IRQ, irq);
		log_item_set_field(struct dbg_snapshot_log, CLK, clk);
		log_item_set_field(struct dbg_snapshot_log, PMU, pmu);
		log_item_set_array_field(struct dbg_snapshot_log, FREQ, freq);
		log_item_set_field(struct dbg_snapshot_log, DM, dm);
		log_item_set_array_field(struct dbg_snapshot_log, HRTIMER, hrtimer);
		log_item_set_array_field(struct dbg_snapshot_log, REG, reg);
		log_item_set_field(struct dbg_snapshot_log, REGULATOR, regulator);
		log_item_set_field(struct dbg_snapshot_log, THERMAL, thermal);
		log_item_set_field(struct dbg_snapshot_log, ACPM, acpm);
		log_item_set_field(struct dbg_snapshot_log, PRINTK, print);
		break;
	case eDSS_LOG_TYPE_MINIMIZED:
		log_item_set_array_field(struct dbg_snapshot_log_minimized, TASK, task);
		log_item_set_array_field(struct dbg_snapshot_log_minimized, WORK, work);
		log_item_set_array_field(struct dbg_snapshot_log_minimized, CPUIDLE, cpuidle);
		log_item_set_array_field(struct dbg_snapshot_log_minimized, IRQ, irq);
		log_item_set_array_field(struct dbg_snapshot_log_minimized, FREQ, freq);
		break;
	default:
		break;
	}
}

static bool is_valid_dss_log_type(enum dbg_snapshot_log_type type)
{
	if (type == eDSS_LOG_TYPE_DEFAULT || type == eDSS_LOG_TYPE_MINIMIZED)
		return true;
	return false;
}

void dbg_snapshot_init_log(void)
{
	struct device_node *np = dss_desc.dev->of_node;
	struct property *prop;
	const char *str;
	unsigned int i = 0;

	if (!dbg_snapshot_get_item_enable(DSS_ITEM_KEVENTS)) {
		dev_err(dss_desc.dev, "DSS logging is disabled.\n");
		return;
	}

	if (!is_valid_dss_log_type(dss_kevents.type)) {
		dev_err(dss_desc.dev, "DSS logging type is invalid(%d)\n", dss_kevents.type);
		return;
	}
	dbg_snapshot_set_log_item_field();

	if (dbg_snapshot_get_enable_log_item(DSS_LOG_TASK))
		register_trace_sched_switch(dbg_snapshot_sched_switch, NULL);

	if (dbg_snapshot_get_enable_log_item(DSS_LOG_WORK)) {
		register_trace_workqueue_execute_start(dbg_snapshot_wq_start, NULL);
		register_trace_workqueue_execute_end(dbg_snapshot_wq_end, NULL);
	}
	if (dbg_snapshot_get_enable_log_item(DSS_LOG_IRQ)) {
		register_trace_irq_handler_entry(dbg_snapshot_irq_entry, NULL);
		register_trace_irq_handler_exit(dbg_snapshot_irq_exit, NULL);
	}
	if (dbg_snapshot_get_enable_log_item(DSS_LOG_HRTIMER)) {
		register_trace_hrtimer_expire_entry(dbg_snapshot_hrtimer_entry, NULL);
		register_trace_hrtimer_expire_exit(dbg_snapshot_hrtimer_exit, NULL);
	}
	if (dbg_snapshot_get_enable_log_item(DSS_LOG_SUSPEND)) {
		register_trace_suspend_resume(dbg_snapshot_suspend_resume, NULL);
		register_trace_device_pm_callback_start(dbg_snapshot_dev_pm_cb_start, NULL);
		register_trace_device_pm_callback_end(dbg_snapshot_dev_pm_cb_end, NULL);
	}
	if (dbg_snapshot_get_enable_log_item(DSS_LOG_REG)) {
		register_rwmmio_write(dbg_snapshot_reg_write, NULL);
		register_rwmmio_read(dbg_snapshot_reg_read, NULL);
		register_rwmmio_post_read(dbg_snapshot_reg_post_read, NULL);
	}

	dss_freq_size = of_property_count_strings(np, "freq_names");
	of_property_for_each_string(np, "freq_names", prop, str) {
		strncpy(dss_freq_name[i++], str, strlen(str));
	}

	dbg_snapshot_log_output();
}
