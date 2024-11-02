/*
 * CPUFreq boost driver
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Park Choonghoon <choong.park@samsung.com>
 */
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/reciprocal_div.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * data structure and API                                                     *
 ******************************************************************************/
/* Freqboost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	int boost_max;
	u64 boost_ts;
	struct {
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* Count of woken-up tasks on that boost group */
		unsigned wokenup_tasks;
		/* Timestamp of boost activation */
		u64 ts;
	} group[CGROUP_COUNT];
	int timeout;
};

static raw_spinlock_t __percpu *lock;

static inline bool freqboost_boost_timeout(u64 now, u64 ts, u64 timeout)
{
	return ((now - ts) > timeout);
}

static inline bool
freqboost_boost_group_active(int idx, struct boost_groups *bg, u64 now)
{
	if (bg->group[idx].tasks)
		return true;

	return !freqboost_boost_timeout(now, bg->group[idx].ts, bg->timeout);
}

static void
freqboost_group_update(struct boost_groups *bg, u64 now)
{
	int boost_max = INT_MIN;
	u64 boost_ts = 0;
	int idx;

	for (idx = 0; idx < CGROUP_COUNT; idx++) {
		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU or it has hold
		 * in effect from a previous task.
		 */
		if (!freqboost_boost_group_active(idx, bg, now))
			continue;

		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
		boost_ts = bg->group[idx].ts;
	}

	bg->boost_max = boost_max;
	if (boost_ts)
		bg->boost_ts = boost_ts;
}

struct reciprocal_value freqboost_spc_rdiv;

static long
freqboost_margin(unsigned long capacity, unsigned long signal, long boost)
{
	long long margin = 0;

	if (signal > capacity)
		return 0;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (capacity - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		margin  = capacity - signal;
		margin *= boost;
	} else
		margin = -signal * boost;

	margin  = reciprocal_divide(margin, freqboost_spc_rdiv);

	if (boost < 0)
		margin *= -1;
	return margin;
}

static unsigned long
freqboost_boosted_util(struct boost_groups *bg, int cpu, unsigned long util)
{
	u64 now;
	long boost, margin = 0;

	now = sched_clock();

	/* Check to see if we have a hold in effect */
	if (freqboost_boost_timeout(now, bg->boost_ts, bg->timeout))
		freqboost_group_update(bg, now);

	boost = bg->boost_max;
	if (!boost || boost == INT_MIN)
		goto out;

	margin = freqboost_margin(capacity_cpu(cpu), util, boost);

out:
	trace_freqboost_boosted_util(cpu, boost, util, margin);

	return util + margin;
}

/******************************************************************************
 * freq boost                                                                 *
 ******************************************************************************/
/* Boost groups affecting each CPU in the system */
static struct boost_groups __percpu *freqboost_groups;

/* We hold freqboost in effect for at least this long */
#define FREQBOOST_HOLD_NS 50000000ULL	/* 50ms */

static inline bool
freqboost_update_timestamp(struct task_struct *p)
{
	return task_has_rt_policy(p);
}

static inline void
freqboost_tasks_update(struct task_struct *p, int cpu,
				int idx, int flags, int task_count)
{
	struct boost_groups *bg = per_cpu_ptr(freqboost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	/* Update timeout on enqueue */
	if (task_count > 0) {
		u64 now = sched_clock();

		if (freqboost_update_timestamp(p))
			bg->group[idx].ts = now;

		/* Boost group activation or deactivation on that RQ */
		if (bg->group[idx].tasks == 1)
			freqboost_group_update(bg, now);
	}
}

unsigned long freqboost_cpu_boost(int cpu, unsigned long util)
{
	struct boost_groups *bg = per_cpu_ptr(freqboost_groups, cpu);

	return freqboost_boosted_util(bg, cpu, util);
}

/******************************************************************************
 * Heavy Task Boost							      *
 ******************************************************************************/
unsigned long heavytask_cpu_boost(int cpu, unsigned long util, int ratio)
{
	int boost = 0, hratio = profile_get_htask_ratio(cpu);
	long margin = 0;

	if (!hratio)
		goto out;

	boost = (hratio * ratio) >> SCHED_CAPACITY_SHIFT;
	boost = min(boost, 100);
	margin = freqboost_margin(capacity_cpu(cpu), util, boost);

out:
	trace_freqboost_htsk_boosted_util(cpu, hratio,
				ratio, boost, util, margin);
	return util + margin;
}

/******************************************************************************
 * common                                                                     *
 ******************************************************************************/
static void
freqboost_enqdeq_task(struct task_struct *p, int cpu, int flags, int type)
{
	unsigned long irq_flags;
	int idx;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(per_cpu_ptr(lock, cpu), irq_flags);

	idx = cpuctl_task_group_idx(p);

	freqboost_tasks_update(p, cpu, idx, flags, type);

	raw_spin_unlock_irqrestore(per_cpu_ptr(lock, cpu), irq_flags);
}

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags)
{
	freqboost_enqdeq_task(p, cpu, flags, ENQUEUE_TASK);
}

void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags)
{
	freqboost_enqdeq_task(p, cpu, flags, DEQUEUE_TASK);
}

int freqboost_can_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct boost_groups *bg;
	struct rq_flags rq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;
	u64 now;

	cgroup_taskset_for_each(task, css, tset) {
		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = task_rq_lock(task, &rq_flags);

		if (!task->on_rq) {
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * Boost group accouting is protected by a per-cpu lock and requires
		 * interrupt to be disabled to avoid race conditions on...
		 */
		cpu = cpu_of(rq);
		bg = per_cpu_ptr(freqboost_groups, cpu);
		raw_spin_lock(per_cpu_ptr(lock, cpu));

		dst_bg = css->id - 1;

		/* if customer add new group, use the last group */
		if (dst_bg >= CGROUP_COUNT)
			dst_bg = CGROUP_COUNT - 1;

		src_bg = cpuctl_task_group_idx(task);

		/*
		 * Current task is not changing boostgroup, which can
		 * happen when the new hierarchy is in use.
		 */
		if (unlikely(dst_bg == src_bg)) {
			raw_spin_unlock(per_cpu_ptr(lock, cpu));
			task_rq_unlock(rq, task, &rq_flags);
			continue;
		}

		/*
		 * This is the case of a RUNNABLE task which is switching its
		 * current boost group.
		 */

		/* Move task from src to dst boost group */
		tasks = bg->group[src_bg].tasks - 1;
		bg->group[src_bg].tasks = max(0, tasks);
		bg->group[dst_bg].tasks += 1;

		/* Update boost hold start for this group */
		now = sched_clock();
		bg->group[dst_bg].ts = now;

		/* Force boost group re-evaluation at next boost check */
		bg->boost_ts = now - FREQBOOST_HOLD_NS;

		raw_spin_unlock(per_cpu_ptr(lock, cpu));
		task_rq_unlock(rq, task, &rq_flags);
	}

	return 0;
}

int freqboost_get_task_ratio(struct task_struct *p)
{
	struct boost_groups *fbg = per_cpu_ptr(freqboost_groups, task_cpu(p));
	int st_idx;

	st_idx = cpuctl_task_group_idx(p);

	return fbg->group[st_idx].boost;
}

static int freqboost_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct boost_groups *fbg;
	int i, cpu;

	for_each_possible_cpu(cpu) {
		fbg = per_cpu_ptr(freqboost_groups, cpu);

		for (i = 0; i < CGROUP_COUNT; i++)
			fbg->group[i].boost = cur_set->freqboost.ratio[i][cpu];
	}

	return NOTIFY_OK;
}

static struct notifier_block freqboost_emstune_notifier = {
	.notifier_call = freqboost_emstune_notifier_call,
};

static inline void
freqboost_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = per_cpu_ptr(freqboost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));
		bg->timeout = FREQBOOST_HOLD_NS;

		raw_spin_lock_init(per_cpu_ptr(lock, cpu));
	}
}

/*
 * Initialize the cgroup structures
 */
int freqboost_init(void)
{
	lock = alloc_percpu(raw_spinlock_t);
	freqboost_groups = alloc_percpu(struct boost_groups);

	freqboost_spc_rdiv = reciprocal_value(100);
	freqboost_init_cgroups();

	emstune_register_notifier(&freqboost_emstune_notifier);

	return 0;
}
