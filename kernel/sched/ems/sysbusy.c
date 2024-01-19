/*
 * Scheduling in busy system feature for Exynos Mobile Scheduler (EMS)
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Choonghoon Park <choong.park@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

enum sysbusy_type {
	SYSBUSY_CPN = 0,
	SYSBUSY_FAST2_SLOW6,
};

struct sysbusy {
	raw_spinlock_t lock;
	u64 last_update_time;
	u64 monitor_interval;
	u64 release_start_time;
	enum sysbusy_state state;
	struct work_struct work;
	int enabled;
	enum sysbusy_type type;
} sysbusy;

struct somac_env {
	struct rq		*dst_rq;
	struct task_struct	*target_task;
	unsigned long		time;
};
static struct somac_env __percpu *somac_env;

struct sysbusy_stat {
	int count;
	u64 last_time;
	u64 time_in_state;
} sysbusy_stats[NUM_OF_SYSBUSY_STATE];

static int cpn_next_cpu[VENDOR_NR_CPUS] = { [0 ... VENDOR_NR_CPUS - 1] = -1 };
/******************************************************************************
 * sysbusy notify                                                             *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(sysbusy_chain);

static int sysbusy_notify(int next_state, int event)
{
	int ret;

	ret = raw_notifier_call_chain(&sysbusy_chain, event, &next_state);

	return notifier_to_errno(ret);
}

int sysbusy_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&sysbusy_chain, nb);
}
EXPORT_SYMBOL_GPL(sysbusy_register_notifier);

int sysbusy_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&sysbusy_chain, nb);
}
EXPORT_SYMBOL_GPL(sysbusy_unregister_notifier);

/******************************************************************************
 *			            SOMAC			              *
 ******************************************************************************/
int sysbusy_on_somac(void)
{
	return sysbusy.state == SYSBUSY_SOMAC;
}

static int decision_somac_task(struct task_struct *p)
{
	return cpuctl_task_group_idx(p) == CGROUP_TOPAPP;
}

static struct task_struct *pick_somac_task(struct rq *rq)
{
	struct task_struct *p, *heaviest_task = NULL;
	unsigned long util, max_util = 0;
	int task_count = 0;

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (decision_somac_task(p)) {
			util = ml_task_util(p);
			if (util > max_util) {
				heaviest_task = p;
				max_util = util;
			}
		}

		if (++task_count >= TRACK_TASK_COUNT)
			break;
	}

	return heaviest_task;
}

static int somac_cpu_stop(void *data)
{
	struct somac_env *env = data;
	struct rq *src_rq = this_rq(), *dst_rq = env->dst_rq;
	struct task_struct *p = env->target_task;
	struct rq_flags rf;
	int ret;

	rq_lock_irq(src_rq, &rf);
	ret = detach_one_task(src_rq, dst_rq, p);
	src_rq->active_balance = 0;
	rq_unlock(src_rq, &rf);

	if (ret) {
		attach_one_task(dst_rq, p);
		ems_somac_dequeued(src_rq) = env->time;
		trace_sysbusy_somac(p, ml_task_util(p),
			src_rq->cpu, dst_rq->cpu, env->time);
	}

	local_irq_enable();

	return 0;
}

static DEFINE_SPINLOCK(somac_lock);
static struct cpu_stop_work __percpu *somac_work;

static bool __somac_tasks(int src_cpu, int dst_cpu, unsigned long time)
{
	struct somac_env *env = per_cpu_ptr(somac_env, src_cpu);
	struct task_struct *p = NULL;
	struct rq *src_rq = cpu_rq(src_cpu);
	unsigned long flags;

	if (!cpu_active(src_cpu) || !cpu_active(dst_cpu)) {
		trace_sysbusy_mv_somac_task(src_cpu, "not-acitve-cpu");
		return false;
	}

	raw_spin_rq_lock_irqsave(src_rq, flags);
	if (src_rq->active_balance) {
		raw_spin_rq_unlock_irqrestore(src_rq, flags);
		trace_sysbusy_mv_somac_task(src_cpu, "under-active-balancing");
		return false;
	}

	if (!src_rq->cfs.nr_running) {
		raw_spin_rq_unlock_irqrestore(src_rq, flags);
		trace_sysbusy_mv_somac_task(src_cpu, "no-cfs-tasks");
		return false;
	}

	if (ems_somac_dequeued(src_rq) == time) {
		raw_spin_rq_unlock_irqrestore(src_rq, flags);
		trace_sysbusy_mv_somac_task(src_cpu, "already-dequeued");
		return false;
	}

	p = pick_somac_task(src_rq);
	if (!p) {
		raw_spin_rq_unlock_irqrestore(src_rq, flags);
		trace_sysbusy_mv_somac_task(src_cpu, "no-somac");
		return false;
	}

	env->dst_rq = cpu_rq(dst_cpu);
	env->target_task = p;
	env->time = time;

	src_rq->active_balance = 1;

	raw_spin_rq_unlock_irqrestore(src_rq, flags);

	stop_one_cpu_nowait(src_cpu, somac_cpu_stop, env, per_cpu_ptr(somac_work, src_cpu));

	return true;
}

static int somac_interval = 1; /* 1tick = 4ms */
static int somac_turn = 0;

void somac_tasks(void)
{
	static unsigned long last_somac_time;
	bool somac_finished[VENDOR_NR_CPUS] = { 0 };
	unsigned long now = jiffies;
	unsigned long flags;
	int busy_cpu = -1, idle_cpu = -1;
	int cpu;
	int ret;

	if (!spin_trylock_irqsave(&somac_lock, flags))
		return;

	if (!sysbusy.enabled)
		goto unlock;

	raw_spin_lock(&sysbusy.lock);
	ret = sysbusy_on_somac();
	raw_spin_unlock(&sysbusy.lock);
	if (!ret)
		goto unlock;

	if (now < last_somac_time + somac_interval)
		goto unlock;

	if (num_active_cpus() != VENDOR_NR_CPUS ||
			cpumask_weight(ecs_cpus_allowed(NULL)) != VENDOR_NR_CPUS)
		goto unlock;

	for_each_online_cpu(cpu) {
		int nr_running = cpu_rq(cpu)->nr_running;

		if (nr_running > 1)
			busy_cpu = cpu;
		else if (nr_running == 0)
			idle_cpu = cpu;
	}

	if (busy_cpu >= 0 && idle_cpu >= 0) {
		if (__somac_tasks(busy_cpu, idle_cpu, now))
			trace_sysbusy_somac_reason(busy_cpu, idle_cpu, "BUSY -> IDLE");
		goto out;
	}

	if (sysbusy.type == SYSBUSY_CPN) {
		for_each_cpu(cpu, cpu_slowest_mask()) {
			int src_cpu, dst_cpu;

			if (somac_finished[cpu])
				continue;

			src_cpu = cpu;
			dst_cpu = cpn_next_cpu[src_cpu];

			do {
				if (src_cpu == -1 || dst_cpu == -1)
					break;

				if (__somac_tasks(src_cpu, dst_cpu, now)) {
					somac_finished[src_cpu] = true;
					trace_sysbusy_somac_reason(src_cpu, dst_cpu, "ROTATION");
				} else {
					somac_finished[src_cpu] = true;
					somac_finished[dst_cpu] = true;
					trace_sysbusy_somac_reason(src_cpu, dst_cpu, "SKIP");
					break;
				}

				src_cpu = dst_cpu;
				dst_cpu = cpn_next_cpu[src_cpu];
			} while (src_cpu != cpu);
		}
	} else if (sysbusy.type == SYSBUSY_FAST2_SLOW6) {
		for_each_cpu(cpu, cpu_slowest_mask()) {
			int src_cpu, dst_cpu;

			if (somac_finished[cpu])
				continue;

			if (cpu == somac_turn * 2) {
				src_cpu = cpu;
				dst_cpu = 6;
			} else if (cpu == ((somac_turn * 2) + 1)) {
				src_cpu = cpu;
				dst_cpu = 7;
			} else {
				continue;
			}

			/* big 2 + little 6
			 * For reduce migration cost, little workload keep 3 turn
			 * (1st) 0/1 <-> 6/7, (2nd) 2/3 <-> 6/7, (3rd) 4/5 <-> 6/7, ...
			 */
			if (__somac_tasks(src_cpu, dst_cpu, now)) {
				somac_finished[src_cpu] = true;
				trace_sysbusy_somac_reason(src_cpu, dst_cpu, "ROTATION");
			} else {
				somac_finished[src_cpu] = true;
				somac_finished[dst_cpu] = true;
				trace_sysbusy_somac_reason(src_cpu, dst_cpu, "SKIP");
				continue;
			}

			if (__somac_tasks(dst_cpu, src_cpu, now)) {
				somac_finished[dst_cpu] = true;
				trace_sysbusy_somac_reason(dst_cpu, src_cpu, "ROTATION");
			} else {
				somac_finished[src_cpu] = true;
				somac_finished[dst_cpu] = true;
				trace_sysbusy_somac_reason(dst_cpu, src_cpu, "SKIP");
				continue;
			}
		}

		somac_turn++;
		if (somac_turn > 2)
			somac_turn = 0;
	}
out:
	last_somac_time = now;
unlock:
	spin_unlock_irqrestore(&somac_lock, flags);
}

/******************************************************************************
 *			       sysbusy schedule			              *
 ******************************************************************************/
int sysbusy_activated(void)
{
	return sysbusy.state > SYSBUSY_STATE0;
}

static unsigned long
get_remained_task_util(int cpu, struct tp_env *env)
{
	struct task_struct *p, *excluded = env->p;
	struct rq *rq = cpu_rq(cpu);
	unsigned long util = capacity_cpu_orig(cpu), util_sum = 0;
	int task_count = 0;
	unsigned long flags;
	int need_rq_lock = !excluded->on_rq || cpu != task_cpu(excluded);

	if (need_rq_lock)
		raw_spin_rq_lock_irqsave(rq, flags);

	if (!rq->cfs.curr) {
		util = 0;
		goto unlock;
	}

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (p == excluded)
			continue;

		util = ml_task_util(p);
		util_sum += util;

		if (++task_count >= TRACK_TASK_COUNT)
			break;
	}

unlock:
	if (need_rq_lock)
		raw_spin_rq_unlock_irqrestore(rq, flags);

	return min_t(unsigned long, util_sum, capacity_cpu_orig(cpu));
}

static int sysbusy_find_fastest_cpu(struct tp_env *env)
{
	struct cpumask mask;
	unsigned long cpu_util;
	int cpu;

	cpumask_and(&mask, cpu_fastest_mask(), &env->cpus_allowed);
	if (cpumask_empty(&mask))
		cpu = cpumask_last(&env->cpus_allowed);
	else
		cpu = cpumask_any(&mask);

	if (!cpu_rq(cpu)->nr_running)
		return cpu;

	cpu_util = ml_cpu_util_without(cpu, env->p) + cpu_util_rt(cpu_rq(cpu)) + cpu_util_dl(cpu_rq(cpu));
	cpu_util = min(cpu_util, capacity_cpu_orig(cpu));

	return check_busy(cpu_util, capacity_cpu(cpu)) ? -1 : cpu;
}

static int sysbusy_find_min_util_cpu(struct tp_env *env)
{
	int cpu, min_cpu = -1, min_idle_cpu = -1;
	unsigned long min_util = INT_MAX, min_idle_util = INT_MAX;

	for_each_cpu(cpu, &env->cpus_allowed) {
		unsigned long cpu_util;

		if (ems_rq_migrated(cpu_rq(cpu)))
			continue;

		cpu_util = ml_cpu_util_without(cpu, env->p);
		cpu_util = min(cpu_util, capacity_cpu_orig(cpu));

		if (cpu == task_cpu(env->p) && !cpu_util)
			cpu_util = get_remained_task_util(cpu, env);

		cpu_util = min(cpu_util, capacity_cpu_orig(cpu));

		if (available_idle_cpu(cpu)) {
			if (cpu_util <= min_idle_util) {
				min_idle_cpu = cpu;
				min_idle_util = cpu_util;
			}
			continue;
		}

		/* find min util cpu with rt util */
		if (cpu_util <= min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	min_cpu = (min_idle_cpu >= 0) ? min_idle_cpu : min_cpu;

	return min_cpu;
}

#define CPU_CAPACITY_SUM	(SCHED_CAPACITY_SCALE * VENDOR_NR_CPUS)
#define ED_MIN_AR_RATIO		(SCHED_CAPACITY_SCALE / 10)
#define PD_BUSY_AR_RATIO	(SCHED_CAPACITY_SCALE * 75 / 100)

static bool is_busy_perf_domain(struct system_profile_data *sd)
{
	if (emstune_get_cur_level())
		return false;

	if (sd->ed_ar_avg > ED_MIN_AR_RATIO)
		return false;

	if (sd->pd_ar_avg < PD_BUSY_AR_RATIO)
		return false;

	if (sd->pd_nr_running < VENDOR_NR_CPUS)
		return false;

	return true;
}

static enum sysbusy_state determine_sysbusy_state(void)
{
	struct system_profile_data system_data;

	get_system_sched_data(&system_data);

	/* Determine STATE1 */
	if (system_data.busy_cpu_count == 1) {
		if (check_busy(system_data.heavy_task_util_sum,
				system_data.cpu_util_sum)) {
			return SYSBUSY_STATE1;
		}
	}

	/* Determine STATE2 or STATE3 */
	if (check_busy(system_data.heavy_task_util_sum, CPU_CAPACITY_SUM)
		|| is_busy_perf_domain(&system_data)) {
		bool is_somac;

		is_somac = system_data.misfit_task_count > (VENDOR_NR_CPUS / 2);
		if (system_data.heavy_task_count > 10)
			is_somac = false;

		if (!is_somac)
			return SYSBUSY_STATE2;

		return SYSBUSY_STATE3;
	}

	/* not sysbusy or no heavy task */
	return SYSBUSY_STATE0;
}

static void
update_sysbusy_stat(int old_state, int next_state, unsigned long now)
{
	sysbusy_stats[old_state].count++;
	sysbusy_stats[old_state].time_in_state +=
		now - sysbusy_stats[old_state].last_time;

	sysbusy_stats[next_state].last_time = now;
}

static void change_sysbusy_state(int next_state, unsigned long now)
{
	int old_state = sysbusy.state;
	struct sysbusy_param *param;

	if (old_state == next_state) {
		sysbusy.release_start_time = 0;
		return;
	}

	param = &sysbusy_params[old_state];
	if (!test_bit(next_state, &param->allowed_next_state))
		return;

	/* release sysbusy */
	if (next_state == SYSBUSY_STATE0) {
		if (!sysbusy.release_start_time)
			sysbusy.release_start_time = now;

		if (now < sysbusy.release_start_time + param->release_duration)
			return;
	}

	sysbusy.monitor_interval = sysbusy_params[next_state].monitor_interval;
	sysbusy.release_start_time = 0;
	sysbusy.state = next_state;

	schedule_work(&sysbusy.work);
	update_sysbusy_stat(old_state, next_state, now);
	trace_sysbusy_state(old_state, next_state);
}

int sysbusy_boost_task(struct task_struct *p)
{
	int grp_idx = cpuctl_task_group_idx(p);

	if (sysbusy.state != SYSBUSY_STATE1)
		return 0;

	if (grp_idx != CGROUP_TOPAPP)
		return 0;

	return is_heavy_task_util(ml_task_util_est(p));
}

void monitor_sysbusy(void)
{
	enum sysbusy_state next_state;
	unsigned long now = jiffies;
	unsigned long flags;

	if (!raw_spin_trylock_irqsave(&sysbusy.lock, flags))
		return;

	if (!sysbusy.enabled) {
		raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
		return;
	}

	if (now < sysbusy.last_update_time + sysbusy.monitor_interval) {
		raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
		return;
	}

	sysbusy.last_update_time = now;

	next_state = determine_sysbusy_state();

	change_sysbusy_state(next_state, now);
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);
}

int sysbusy_schedule(struct tp_env *env)
{
	int target_cpu = -1;

	switch (sysbusy.state) {
	case SYSBUSY_STATE1:
		if (is_heavy_task_util(env->task_util))
			target_cpu = sysbusy_find_fastest_cpu(env);
		break;
	case SYSBUSY_STATE2:
		target_cpu = sysbusy_find_min_util_cpu(env);
		break;
	case SYSBUSY_STATE3:
		if (is_heavy_task_util(env->task_util) && decision_somac_task(env->p)) {
			if (cpumask_test_cpu(task_cpu(env->p), &env->cpus_allowed))
				target_cpu = task_cpu(env->p);
		}
		else {
			target_cpu = sysbusy_find_min_util_cpu(env);
		}
		break;
	case SYSBUSY_STATE0:
	default:
		break;
	}

	trace_sysbusy_schedule(env->p, sysbusy.state, target_cpu);

	return target_cpu;
}

/**********************************************************************
 *			    SYSFS support			      *
 **********************************************************************/
static struct kobject *sysbusy_kobj;

static ssize_t show_somac_interval(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", somac_interval);
}

static ssize_t store_somac_interval(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	somac_interval = input;

	return count;
}

static struct kobj_attribute somac_interval_attr =
__ATTR(somac_interval, 0644, show_somac_interval, store_somac_interval);

static ssize_t show_sysbusy_stat(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, count = 0;

	for (i = SYSBUSY_STATE0; i < NUM_OF_SYSBUSY_STATE; i++)
		count += snprintf(buf + count, PAGE_SIZE - count,
				"[state%d] count:%d time_in_state=%ums\n",
				i, sysbusy_stats[i].count,
				jiffies_to_msecs(sysbusy_stats[i].time_in_state));

	return count;
}

static struct kobj_attribute sysbusy_stat_attr =
__ATTR(sysbusy_stat, 0644, show_sysbusy_stat, NULL);

static ssize_t show_sysbusy_control(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", sysbusy.enabled);
}

static ssize_t store_sysbusy_control(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;
	unsigned long flags;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	/* Make input 0 or 1 */
	input = !!input;

	/* No state change, just return */
	if (sysbusy.enabled == input)
		return count;

	raw_spin_lock_irqsave(&sysbusy.lock, flags);

	/* Store input value to enabled with sysbusy.lock */
	sysbusy.enabled = input;

	/* If sysbusy is disabled, transition to SYSBUSY_STATE0 */
	if (!sysbusy.enabled)
		change_sysbusy_state(SYSBUSY_STATE0, jiffies);

	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);

	return count;
}

static struct kobj_attribute sysbusy_control_attr =
__ATTR(sysbusy_control, 0644, show_sysbusy_control, store_sysbusy_control);

static int sysbusy_sysfs_init(struct kobject *ems_kobj)
{
	int ret;

	sysbusy_kobj = kobject_create_and_add("sysbusy", ems_kobj);
	if (!sysbusy_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(sysbusy_kobj, &somac_interval_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(sysbusy_kobj, &sysbusy_stat_attr.attr);
	if (ret)
		pr_warn("%s: failed to create somac sysfs\n", __func__);

	ret = sysfs_create_file(sysbusy_kobj, &sysbusy_control_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysbusy sysfs\n", __func__);

	return ret;
}

/**********************************************************************
 *			    Initialization			      *
 **********************************************************************/
static void sysbusy_boost_fn(struct work_struct *work)
{
	int cur_state;
	unsigned long flags;

	raw_spin_lock_irqsave(&sysbusy.lock, flags);
	cur_state = sysbusy.state;
	raw_spin_unlock_irqrestore(&sysbusy.lock, flags);

	if (cur_state > SYSBUSY_STATE0) {
		/*
		 * if sysbusy is activated, notifies whether it is
		 * okay to trigger sysbusy boost.
		 */
		if (sysbusy_notify(cur_state, SYSBUSY_CHECK_BOOST))
			return;
	}

	sysbusy_notify(cur_state, SYSBUSY_STATE_CHANGE);
}

#define MAX_CPN			(30)
#define MAX_NR_CLUSTERS		(3)

static int get_cpn_candidates(int *candidates)
{
	bool invalid[MAX_CPN] = { 0 };
	int size = 0;
	int i, j;

	for (i = 2; i < MAX_CPN; i++) {
		if (invalid[i])
			continue;

		candidates[size++] = i;

		for (j = i + i; j < MAX_CPN; j += i)
			invalid[j] = true;
	}

	return size;
}

static bool check_cpn_candidate(int num, int *slow_to_slow, int *slow_to_fast)
{
	bool visited_cpus[VENDOR_NR_CPUS] = { 0 };
	bool visited_clusters[MAX_NR_CLUSTERS] = { 0 };
	int visited_clusters_cnt = 0;
	int start_cpu;
	int nr_clusters;

	*slow_to_slow = 0;
	*slow_to_fast = 0;
	nr_clusters = topology_physical_package_id(cpumask_any(cpu_fastest_mask())) + 1;

	for_each_cpu(start_cpu, cpu_slowest_mask()) {
		int src_cpu, dsu_cpu;
		int cycle;

		if (visited_cpus[start_cpu])
			continue;

		cycle = 2;
		src_cpu = start_cpu;
		dsu_cpu = (src_cpu + num) % VENDOR_NR_CPUS;

		while (cycle) {
			int cluster_id = topology_physical_package_id(src_cpu);

			if (!visited_clusters[cluster_id]) {
				visited_clusters[cluster_id] = true;
				visited_clusters_cnt++;
			}
			visited_cpus[src_cpu] = true;

			if (cpumask_test_cpu(src_cpu, cpu_slowest_mask())) {
				if (cpumask_test_cpu(dsu_cpu, cpu_slowest_mask()))
					*slow_to_slow += 1;
				else if (cpumask_test_cpu(dsu_cpu, cpu_fastest_mask()))
					*slow_to_fast += 1;
			}

			if (dsu_cpu == start_cpu) {
				if (visited_clusters_cnt != nr_clusters)
					return false;

				cycle--;
			}

			src_cpu = dsu_cpu;
			dsu_cpu = (src_cpu + num) % VENDOR_NR_CPUS;
		}
	}

	return true;
}

static bool sysbusy_check_roundrobin(void)
{
	int cpu = 0;
	int slow_cpus = 0;
	int fast_cpus = 0;
	int i;

	for_each_cpu(cpu, cpu_possible_mask) {
		if (cpumask_test_cpu(cpu, cpu_slowest_mask()))
			slow_cpus++;
		else
			fast_cpus++;
	}

	/* use round robin */
	if (slow_cpus == fast_cpus) {
		for (i = 0; i < VENDOR_NR_CPUS; i++) {
			if (cpumask_test_cpu(i, cpu_slowest_mask()))
				cpn_next_cpu[i] = i + slow_cpus;
			else
				cpn_next_cpu[i] = (i - 1) % slow_cpus;
		}

		return true;
	} else if (slow_cpus == 6 && fast_cpus == 2) {
		sysbusy.type = SYSBUSY_FAST2_SLOW6;

		return true;
	}

	return false;
}

static int sysbusy_calculate_cpn(void)
{
	int candidates[MAX_CPN];
	int best_slow_to_slow = INT_MAX;
	int best_slow_to_fast = -1;
	int best = 0;
	int i, size;

	sysbusy.type = SYSBUSY_CPN;

	if (sysbusy_check_roundrobin())
		return 0;

	size = get_cpn_candidates(candidates);

	for (i = 0; i < size; i++) {
		int slow_to_slow, slow_to_fast;
		int cand = candidates[i];

		if (!check_cpn_candidate(cand, &slow_to_slow, &slow_to_fast))
			continue;

		if (slow_to_slow < best_slow_to_slow) {
			best_slow_to_slow = slow_to_slow;
			best_slow_to_fast = slow_to_fast;
			best = cand;
		} else if (slow_to_slow == best_slow_to_slow) {
			if (slow_to_fast > best_slow_to_fast) {
				best_slow_to_fast = slow_to_fast;
				best = cand;
			}
		}
	}

	if (!best)
		return -EINVAL;

	for (i = 0; i < VENDOR_NR_CPUS; i++)
		cpn_next_cpu[i] = (i + best) % VENDOR_NR_CPUS;

	return 0;
}

int sysbusy_init(struct kobject *ems_kobj)
{
	int ret;

	somac_env = alloc_percpu(struct somac_env);
	if (!somac_env) {
		pr_err("failed to allocate somac_env\n");
		return -ENOMEM;
	}

	somac_work = alloc_percpu(struct cpu_stop_work);
	if (!somac_work) {
		pr_err("failed to allocate somac_work\n");
		free_percpu(somac_env);
		return -ENOMEM;
	}

	raw_spin_lock_init(&sysbusy.lock);
	INIT_WORK(&sysbusy.work, sysbusy_boost_fn);

	ret = sysbusy_sysfs_init(ems_kobj);
	if (ret)
		pr_err("failed to init sysfs for sysbusy\n");

	ret = sysbusy_calculate_cpn();
	if (ret)
		pr_err("failed to calcaulate cpn\n");

	if (!ret)
		sysbusy.enabled = 1;

	return ret;
}
