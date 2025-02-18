/*
 * Exynos Mobile Scheduler CPU selection
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#define INVALID_CPU	-1
#define cpu_selected(cpu)	(cpu > INVALID_CPU)

/******************************************************************************
 * TEX (Task EXpress)                                                         *
 ******************************************************************************/
struct {
	int enabled[CGROUP_COUNT];
	int prio;
} tex;

bool is_boosted_tex_task(struct task_struct *p)
{
	if (ems_boosted_tex(p))
		return true;
	else if (emstune_get_cur_level() == 2 && cpuctl_task_group_idx(p) == CGROUP_TOPAPP) {
		/* RenderThread and zygote boost */
		if (ems_render(p) == 1 || strcmp(p->comm, "main") == 0) {
			return true;
		}
	}

	return false;
}

static bool is_binder_tex_task(struct task_struct *p)
{
	return ems_binder_task(p);
}

static bool is_prio_tex_task(struct task_struct *p)
{
	int group_idx, sched_class;

	group_idx = cpuctl_task_group_idx(p);
	if (!tex.enabled[group_idx])
		return 0;

	sched_class = get_sched_class(p);
	if (sched_class != EMS_SCHED_FAIR)
		return 0;

	return p->prio <= tex.prio;
}

static bool is_expired_tex(struct task_struct *p)
{
	return ems_tex_chances(p) <= 0;
}

int get_tex_level(struct task_struct *p)
{
	if (is_expired_tex(p))
		return NOT_TEX;

	if (is_boosted_tex_task(p))
		return BOOSTED_TEX;

	if (is_binder_tex_task(p))
		return BINDER_TEX;

	if (is_prio_tex_task(p))
		return PRIO_TEX;

	return NOT_TEX;
}

static void tex_insert_to_qjump_list(struct task_struct *p, int cpu, bool preempt)
{
	struct list_head *qjump_list = ems_qjump_list(cpu_rq(cpu)), *node;
	struct task_struct *qjump_task;

	list_for_each(node, qjump_list) {
		qjump_task = ems_qjump_list_entry(node);

		if (ems_tex_level(p) < ems_tex_level(qjump_task))
			break;
		else if ((ems_tex_level(p) == ems_tex_level(qjump_task)) && preempt)
			break;
	}

	list_add(ems_qjump_node(p), node->prev);

	trace_tex_insert_to_qjump_list(p);
}

static void tex_remove_from_qjump_list(struct task_struct *p)
{
	list_del_init(ems_qjump_node(p));

	trace_tex_remove_from_qjump_list(p);
}

void tex_enqueue_task(struct task_struct *p, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	int tex_level = get_tex_level(p);

	if (get_sched_class(p) != EMS_SCHED_FAIR)
		return;

	ems_prio_tex(p) = is_prio_tex_task(p);
	if (ems_prio_tex(p))
		ems_rq_nr_prio_tex(rq) += 1;

	if (tex_level == NOT_TEX)
		return;

	ems_tex_level(p) = tex_level;
	tex_insert_to_qjump_list(p, cpu, task_running(rq, p));

	if (!ems_tex_runtime(p))
		ems_tex_last_update(p) = p->se.sum_exec_runtime;
}

void tex_dequeue_task(struct task_struct *p, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	bool is_tex = !list_empty(ems_qjump_node(p)) && ems_qjump_node(p)->next;

	if (get_sched_class(p) != EMS_SCHED_FAIR)
		return;

	if (ems_prio_tex(p))
		ems_rq_nr_prio_tex(rq) -= 1;

	if (is_tex) {
		tex_remove_from_qjump_list(p);
		ems_tex_level(p) = NOT_TEX;
	}

	if (p->state != TASK_RUNNING) {
		ems_tex_runtime(p) = 0;
		ems_tex_chances(p) = TEX_WINDOW_COUNT;
	}
}

void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				struct sched_entity **se_ptr, bool *repick,
				bool simple, struct task_struct *prev)
{
	struct task_struct *p = NULL;

	if (list_empty(ems_qjump_list(rq)))
		return;

	p = ems_qjump_first_entry(ems_qjump_list(rq));

	*p_ptr = p;
	*se_ptr = &p->se;
	*repick = true;

	trace_tex_qjump_pick_next_task(p);
}

static unsigned int get_idle_exit_latency(struct rq *rq)
{
	struct cpuidle_state *idle;
	unsigned int exit_latency;

	rcu_read_lock();
	idle = idle_get_state(rq);
	exit_latency =  idle ? idle->exit_latency : 0;
	rcu_read_unlock();

	return exit_latency;
}

static bool is_perf_task(struct task_struct *p)
{
	return cpuctl_task_group_idx(p) == CGROUP_TOPAPP;
}

static bool can_sync_to_this_cpu(struct tp_env *env, int this_cpu)
{
	if (uclamp_latency_sensitive(env->p))
		return false;

	if (is_perf_task(env->p) && is_perf_task(cpu_rq(this_cpu)->curr))
		return false;

	if (!cpumask_test_cpu(this_cpu, &env->cpus_allowed))
		return false;

	if (capacity_cpu_orig(this_cpu) < env->base_cap)
		return false;

	return true;
}

static int tex_boosted_fit_cpus(struct tp_env *env)
{
	struct pe_list *pl = get_pe_list(env->init_index);
	int index, cpu = smp_processor_id(), prev_cpu = task_cpu(env->p);
	int max_spare_cap_cpu = -1, shallowest_idle_cpu = -1;
	long spare_cap, max_spare_cap = LONG_MIN, idle_max_spare_cap = 0;
	unsigned int exit_latency, min_exit_latency = UINT_MAX;
	unsigned long cpu_util;

	cpumask_clear(&env->fit_cpus);

	if (env->sync && can_sync_to_this_cpu(env, cpu)) {
		cpumask_set_cpu(cpu, &env->fit_cpus);
		goto out;
	}

	for (index = 0; index < pl->num_of_cpus; index++) {
		for_each_cpu_and(cpu, &pl->cpus[index], &env->cpus_allowed) {
			cpu_util = ml_cpu_util_without(cpu, env->p);
			cpu_util = min(cpu_util, capacity_cpu_orig(cpu));

			spare_cap = capacity_cpu_orig(cpu) - cpu_util;

			if (ems_rq_migrated(cpu_rq(cpu)))
				continue;

			if (get_tex_level(cpu_rq(cpu)->curr) == BOOSTED_TEX)
				continue;

			if (available_idle_cpu(cpu)) {
				exit_latency = get_idle_exit_latency(cpu_rq(cpu));

				if (exit_latency > min_exit_latency)
					continue;

				if (exit_latency == min_exit_latency) {
					if ((shallowest_idle_cpu == prev_cpu) ||
							(spare_cap > idle_max_spare_cap))
						continue;
				}

				shallowest_idle_cpu = cpu;
				idle_max_spare_cap = spare_cap;
				min_exit_latency = exit_latency;
			}
			else {
				if (spare_cap < max_spare_cap)
					continue;

				max_spare_cap_cpu = cpu;
				max_spare_cap = spare_cap;
			}
		}

		if ((max_spare_cap_cpu != -1) || (shallowest_idle_cpu != -1))
			break;
	}

	max_spare_cap_cpu = (shallowest_idle_cpu != -1) ? shallowest_idle_cpu : max_spare_cap_cpu;

	if (cpu_selected(max_spare_cap_cpu)) {
		cpumask_set_cpu(max_spare_cap_cpu, &env->fit_cpus);
		env->reason_of_selection = FAIR_EXPRESS;
	}

out:
	trace_tex_boosted_fit_cpus(env->p, env->sync, &env->fit_cpus);

	return cpumask_weight(&env->fit_cpus);
}

static void tex_adjust_window(struct task_struct *p)
{
	u64 remainder;
	int nr_expired = div64_u64_rem(ems_tex_runtime(p), TEX_WINDOW, &remainder);

	if (nr_expired < ems_tex_chances(p)) {
		ems_tex_chances(p) -= nr_expired;
		ems_tex_runtime(p) = remainder;
	}
	else {
		ems_tex_chances(p) = 0;
		ems_tex_runtime(p) = 0;
	}
}

void tex_update_stats(struct rq *rq, struct task_struct *p)
{
	s64 delta, now = p->se.sum_exec_runtime;

	lockdep_assert_held(&rq->lock);

	delta = now - ems_tex_last_update(p);
	if (delta < 0)
		delta = 0;

	ems_tex_runtime(p) += delta;
	ems_tex_last_update(p) = now;

	if (ems_tex_runtime(p) < TEX_WINDOW) {
		trace_tex_update_stats(p, "stay");
		return;
	}

	/* current window is expired here */
	tex_adjust_window(p);

	tex_remove_from_qjump_list(p);

	if (ems_tex_chances(p) > 0) {
		tex_insert_to_qjump_list(p, rq->cpu, false);
		trace_tex_update_stats(p, "re-queue");
	}
	else {
		ems_tex_level(p) = NOT_TEX;
		trace_tex_update_stats(p, "remove");
	}
}

void tex_check_preempt_wakeup(struct rq *rq, struct task_struct *p,
		bool *preempt, bool *ignore)
{
	struct task_struct *curr = rq->curr;
	bool is_p_tex = !list_empty(ems_qjump_node(p)) && ems_qjump_node(p)->next;
	bool is_curr_tex = !list_empty(ems_qjump_node(curr)) && ems_qjump_node(curr)->next;

	if (!is_curr_tex && !is_p_tex)
		return;

	if (!is_curr_tex && is_p_tex) {
		*preempt = true;
		goto out;
	}

	tex_update_stats(rq, curr);

	if (curr == ems_qjump_first_entry(ems_qjump_list(rq)))
		*ignore = true;
	else
		*preempt = true;

out:
	trace_tex_check_preempt_wakeup(curr, p, *preempt, *ignore);
}

void tex_update(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	bool is_tex;

	if (get_sched_class(curr) != EMS_SCHED_FAIR)
		return;

	raw_spin_lock(&rq->lock);

	is_tex = !list_empty(ems_qjump_node(curr)) && ems_qjump_node(curr)->next;

	if (!is_tex) {
		raw_spin_unlock(&rq->lock);
		return;
	}

	tex_update_stats(rq, curr);

	if ((curr != ems_qjump_first_entry(ems_qjump_list(rq))) && (rq->cfs.h_nr_running > 1))
		resched_curr(rq);

	raw_spin_unlock(&rq->lock);
}

void tex_do_yield(struct task_struct *p)
{
	bool is_tex = !list_empty(ems_qjump_node(p)) && ems_qjump_node(p)->next;

	if (get_sched_class(p) != EMS_SCHED_FAIR)
		return;

	if (!is_tex)
		return;

	tex_remove_from_qjump_list(p);

	ems_tex_level(p) = NOT_TEX;
	ems_tex_runtime(p) = 0;
	ems_tex_chances(p) = TEX_WINDOW_COUNT;
}

static int tex_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		tex.enabled[i] = cur_set->tex.enabled[i];

	tex.prio = cur_set->tex.prio;

	return NOTIFY_OK;
}

static struct notifier_block tex_emstune_notifier = {
	.notifier_call = tex_emstune_notifier_call,
};

void tex_task_init(struct task_struct *p)
{
	INIT_LIST_HEAD(ems_qjump_node(p));

	ems_tex_level(p) = NOT_TEX;
	ems_tex_last_update(p) = 0;
	ems_tex_runtime(p) = 0;
	ems_tex_chances(p) = TEX_WINDOW_COUNT;
	ems_prio_tex(p) = 0;

	ems_boosted_tex(p) = 0;
	ems_binder_task(p) = 0;
}

static void tex_init(void)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		tex.enabled[i] = 0;
	tex.prio = 0;

	emstune_register_notifier(&tex_emstune_notifier);
}

/******************************************************************************
 * cpus_binding                                                               *
 ******************************************************************************/
struct {
	unsigned long target_sched_class;
	struct cpumask mask[CGROUP_COUNT];
} cpus_binding;

const struct cpumask *cpus_binding_mask(struct task_struct *p)
{
	int group_idx;
	int sched_class;

	sched_class = get_sched_class(p);
	if (!(cpus_binding.target_sched_class & sched_class))
		return cpu_active_mask;

	group_idx = cpuctl_task_group_idx(p);
	if (unlikely(cpumask_empty(&cpus_binding.mask[group_idx])))
		return cpu_active_mask;

	return &cpus_binding.mask[group_idx];
}

static int cpus_binding_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	cpus_binding.target_sched_class =
			cur_set->cpus_binding.target_sched_class;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_copy(&cpus_binding.mask[i],
			&cur_set->cpus_binding.mask[i]);

	return NOTIFY_OK;
}

static struct notifier_block cpus_binding_emstune_notifier = {
	.notifier_call = cpus_binding_emstune_notifier_call,
};

static void cpus_binding_init(void)
{
	int i;

	cpus_binding.target_sched_class = 0;

	for (i = 0; i < CGROUP_COUNT; i++)
		cpumask_setall(&cpus_binding.mask[i]);

	emstune_register_notifier(&cpus_binding_emstune_notifier);
}

/******************************************************************************
 * cpu weight / idle weight                                                   *
 ******************************************************************************/
static int active_weight[CGROUP_COUNT][VENDOR_NR_CPUS];
static int idle_weight[CGROUP_COUNT][VENDOR_NR_CPUS];

static int cpu_weight_get(struct task_struct *p, int cpu, int idle)
{
	int group_idx = cpuctl_task_group_idx(p);

	if (NON_IDLE)
		return active_weight[group_idx][cpu];
	else
		return idle_weight[group_idx][cpu];
}

static int cpu_weight_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i, cpu;

	for (i = 0; i < CGROUP_COUNT; i++) {
		for_each_possible_cpu(cpu) {
			active_weight[i][cpu] = cur_set->active_weight.ratio[i][cpu];
			idle_weight[i][cpu] = cur_set->idle_weight.ratio[i][cpu];
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block cpu_weight_emstune_notifier = {
	.notifier_call = cpu_weight_emstune_notifier_call,
};

#define DEFAULT_WEIGHT	(100)
static void cpu_weight_init(void)
{
	int i, cpu;

	for (i = 0; i < CGROUP_COUNT; i++) {
		for_each_possible_cpu(cpu) {
			active_weight[i][cpu] = DEFAULT_WEIGHT;
			idle_weight[i][cpu] = DEFAULT_WEIGHT;
		}
	}

	emstune_register_notifier(&cpu_weight_emstune_notifier);
}

/******************************************************************************
 * best energy cpu selection                                                  *
 ******************************************************************************/
static LIST_HEAD(csd_head);

struct cs_domain __percpu **__pcpu_csd;
#define pcpu_csd(cpu)	(*per_cpu_ptr(__pcpu_csd, cpu))

static int get_idle_state(int cpu)
{
	struct cpuidle_state *state;
	int ret = NON_IDLE;

	rcu_read_lock();
	state = idle_get_state(cpu_rq(cpu));

	if (!state)
		goto out;

	if (strcmp(state->name, "WFI"))
		ret = IDLE_C1;
	else
		ret = IDLE_C2;

out:
	rcu_read_unlock();
	return ret;
}

static void take_util_snapshot(struct tp_env *env)
{
	int cpu;

	/*
	 * We don't agree setting 0 for task util
	 * Because we do better apply active power of task
	 * when get the energy
	 */
	env->task_util = ml_task_util_est(env->p);
	env->task_util_clamped = ml_uclamp_task_util(env->p);
	env->task_load_avg = ml_task_load_avg(env->p);

	/* fill cpu util */
	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct cfs_rq *cfs_rq = &rq->cfs;
		unsigned long capacity = capacity_cpu_orig(cpu);
		unsigned long extra_util;

		env->cpu_stat[cpu].rt_util = cpu_util_rt(rq);
		env->cpu_stat[cpu].dl_util = cpu_util_dl(rq);
		extra_util = env->cpu_stat[cpu].rt_util + env->cpu_stat[cpu].dl_util;

		env->cpu_stat[cpu].util_wo = min(ml_cpu_util_est_without(cpu, env->p) + extra_util,
			    capacity);
		env->cpu_stat[cpu].util_with = min(ml_cpu_util_est_with(env->p, cpu) + extra_util,
			    capacity);

		env->cpu_stat[cpu].runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
		env->cpu_stat[cpu].load_avg = ml_cpu_load_avg(cpu);

		if (cpu == env->prev_cpu)
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_with;
		else
			env->cpu_stat[cpu].util = env->cpu_stat[cpu].util_wo;

		env->cpu_stat[cpu].nr_running = rq->nr_running;
		env->cpu_stat[cpu].idle = get_idle_state(cpu);
		env->weight[cpu] = cpu_weight_get(env->p, cpu, env->cpu_stat[cpu].idle);

		trace_ems_take_util_snapshot(cpu, env);
	}
}

static unsigned long prev_cpu_advantage(unsigned long cpu_util, unsigned long task_util)
{
	long util = cpu_util;

	/*
	 * subtract cpu util by 12.5% of task util to give advantage to
	 * prev cpu when computing energy.
	 */
	util -= (task_util >> 3);

	return max(util, (long)0);
}

static unsigned int compute_system_energy(struct tp_env *env, int dst_cpu,
		struct energy_backup *backup)
{
	struct cs_domain *csd;
	struct energy_state states[VENDOR_NR_CPUS] = { 0, };

	list_for_each_entry(csd, &csd_head, list) {
		unsigned long capacity;

		capacity = cpufreq_get_next_cap(env, &csd->cpus, dst_cpu);
		et_fill_energy_state(env, &csd->cpus, states, capacity, dst_cpu);
	}

	if (env->prev_cpu == dst_cpu)
		states[dst_cpu].util = prev_cpu_advantage(states[dst_cpu].util, env->task_util);

	return et_compute_system_energy(&csd_head, states, dst_cpu, backup);
}

static int find_min_util_cpu(struct tp_env *env, const struct cpumask *mask, bool among_idle)
{
	int cpu, min_cpu = INVALID_CPU;
	unsigned long min_util = ULONG_MAX;

	for_each_cpu_and(cpu, &env->fit_cpus, mask) {
		unsigned long cpu_util;

		/*
		 * If among_idle is true, find min util cpu among idle cpu.
		 * Skip non-idle cpu.
		 */
		if (among_idle && !env->cpu_stat[cpu].idle)
			continue;

		cpu_util = env->cpu_stat[cpu].util_with;
		if (cpu == env->prev_cpu)
			cpu_util = prev_cpu_advantage(cpu_util, env->task_util);

		if (cpu_util < min_util) {
			min_util = cpu_util;
			min_cpu = cpu;
		}
	}

	return min_cpu;
}

static int __find_energy_cpu(struct tp_env *env, const struct cpumask *candidates)
{
	struct energy_backup backup[VENDOR_NR_CPUS] = { 0, };
	int cpu, energy_cpu = INVALID_CPU, min_util = INT_MAX;
	unsigned int min_energy = UINT_MAX;

	for_each_cpu(cpu, candidates) {
		unsigned int energy;
		int cpu_util = env->cpu_stat[cpu].util_with;

		energy = compute_system_energy(env, cpu, backup);

		trace_ems_compute_system_energy(env->p, candidates, cpu, energy);

		if (energy > min_energy)
			continue;
		else if (energy < min_energy)
			goto found_min_util_cpu;

		if (cpu_util >= min_util)
			continue;

found_min_util_cpu:
		/*
		 * energy_cpu has the lowest energy,
		 * or the lowest util among the same energy.
		 */
		min_energy = energy;
		min_util = cpu_util;
		energy_cpu = cpu;
	}

	return energy_cpu;
}

static int get_best_idle_cpu(struct tp_env *env)
{
	int cpu, best_idle_cpu = -1, best_idle_state = INT_MAX;
	unsigned long best_idle_util = ULONG_MAX;

	for_each_cpu_and(cpu, &env->fit_cpus, cpu_slowest_mask()) {
		unsigned long util = env->cpu_stat[cpu].util_wo;
		int state = env->cpu_stat[cpu].idle;

		if (!available_idle_cpu(cpu))
			continue;

		if (util > best_idle_util)
			continue;

		if ((util == best_idle_util) && (state > best_idle_state))
			continue;

		best_idle_cpu = cpu;
		best_idle_util = util;
		best_idle_state = state;
	}

	return best_idle_cpu;
}

static int find_energy_cpu(struct tp_env *env)
{
	struct cs_domain *csd;
	struct cpumask candidates;
	int energy_cpu, adv_energy_cpu = INVALID_CPU;

	take_util_snapshot(env);

	/* set candidates cpu to find energy cpu */
	cpumask_clear(&candidates);

	/* Pick minimum utilization cpu from each domain */
	list_for_each_entry(csd, &csd_head, list) {
		int min_cpu = find_min_util_cpu(env, &csd->cpus, false);

		if (cpu_selected(min_cpu))
			cpumask_set_cpu(min_cpu, &candidates);
	}

	if (cpumask_weight(&candidates) == 1) {
		energy_cpu = cpumask_any(&candidates);
		goto out;
	}

	/* find min energy cpu */
	energy_cpu = __find_energy_cpu(env, &candidates);

	/*
	 * Slowest cpumask is usually the coregroup that includes the boot
	 * processor(cpu0), has low power consumption but also low performance
	 * efficiency. If selected cpu belongs to slowest cpumask and task is
	 * tiny enough not to increase system energy, reselect min energy cpu
	 * among idle cpu within slowest cpumask for faster task processing.
	 * (tiny task criteria = task util < 12.5% of slowest cpu capacity)
	 */
	if (cpumask_test_cpu(energy_cpu, cpu_slowest_mask())) {
		int best_idle_cpu = get_best_idle_cpu(env);

		if (is_perf_task(env->p) && cpu_selected(best_idle_cpu))
			adv_energy_cpu = best_idle_cpu;
		else if(env->task_util < (capacity_cpu(0) >> 3))
			adv_energy_cpu = find_min_util_cpu(env, cpu_slowest_mask(), true);
	}

out:
	trace_ems_find_energy_cpu(env->p, &candidates, energy_cpu, adv_energy_cpu);

	if (cpu_selected(adv_energy_cpu) || cpu_selected(energy_cpu))
		env->reason_of_selection =  FAIR_ENERGY;

	if (cpu_selected(adv_energy_cpu))
		return adv_energy_cpu;

	return energy_cpu;
}

/******************************************************************************
 * best performance cpu selection                                             *
 ******************************************************************************/
static int find_best_perf_cpu(struct tp_env *env)
{
	struct pe_list *pe_list = get_pe_list(env->init_index);
	int cluster, cpu;
	int best_cpu = INVALID_CPU;
	int best_idle_cpu = INVALID_CPU;
	int best_active_cpu = INVALID_CPU;
	unsigned long max_active_spare = 0;
	bool is_prio_tex = is_prio_tex_task(env->p);

	for (cluster = 0; cluster < pe_list->num_of_cpus; cluster++) {
		unsigned long long cluster_min_exit_latency = ULLONG_MAX;
		unsigned long cluster_max_idle_spare = 0, cluster_max_active_spare = 0;
		int cluster_best_idle_cpu = INVALID_CPU;
		int cluster_best_active_cpu = INVALID_CPU;
		int cluster_best_nr_prio_tex = INT_MAX;

		rcu_read_lock();

		for_each_cpu_and(cpu, &env->fit_cpus, &pe_list->cpus[cluster]) {
			struct rq *rq = cpu_rq(cpu);
			unsigned long extra_util = cpu_util_rt(rq) + cpu_util_dl(rq);
			unsigned long util = ml_cpu_util_without(cpu, env->p);
			unsigned long capacity = capacity_cpu_orig(cpu);
			unsigned long spare;
			int nr_prio_tex_tasks = ems_rq_nr_prio_tex(rq);

			util = min(util + extra_util, capacity);
			spare = capacity - util;

			if (get_tex_level(env->p) == NOT_TEX &&
					get_tex_level(rq->curr) != NOT_TEX)
				continue;

			if (available_idle_cpu(cpu)) {
				unsigned int exit_latency = get_idle_exit_latency(cpu_rq(cpu));

				if (exit_latency < cluster_min_exit_latency) {
					cluster_max_idle_spare = spare;
					cluster_min_exit_latency = exit_latency;
					cluster_best_idle_cpu = cpu;
				} else if (exit_latency == cluster_min_exit_latency) {
					if (spare > cluster_max_idle_spare) {
						cluster_max_idle_spare = spare;
						cluster_best_idle_cpu = cpu;
					}
				}
			} else {
				/* Spread prio_tex tasks */
				if (is_prio_tex) {
					if (nr_prio_tex_tasks > cluster_best_nr_prio_tex)
						continue;

					if ((nr_prio_tex_tasks == cluster_best_nr_prio_tex) &&
							(spare < cluster_max_active_spare))
						continue;
				}

				if (spare > cluster_max_active_spare) {
					cluster_max_active_spare = spare;
					cluster_best_active_cpu = cpu;
					cluster_best_nr_prio_tex = nr_prio_tex_tasks;
				}
			}
		}

		rcu_read_unlock();

		if (cpu_selected(cluster_best_idle_cpu))
			best_idle_cpu = cluster_best_idle_cpu;
		else if (cpu_selected(cluster_best_active_cpu)) {
			if (cluster_max_active_spare > max_active_spare) {
				best_active_cpu = cluster_best_active_cpu;
				max_active_spare = cluster_max_active_spare;
			}
		}

		if (cpu_selected(best_idle_cpu))
			break;

		if ((cluster >= env->end_index) &&
				(cpu_selected(best_idle_cpu) || cpu_selected(best_active_cpu)))
			break;
	}

	if (cpu_selected(best_idle_cpu))
		best_cpu = best_idle_cpu;
	else if (cpu_selected(best_active_cpu))
		best_cpu = best_active_cpu;

	trace_ems_find_best_perf_cpu(env, best_cpu);

	if (cpu_selected(best_cpu))
		env->reason_of_selection = FAIR_PERFORMANCE;

	return best_cpu;
}

/******************************************************************************
 * best cpu selection                                                         *
 ******************************************************************************/
static int find_best_cpu(struct tp_env *env)
{
	int best_cpu;

	switch (env->sched_policy) {
	case SCHED_POLICY_PERF:
		best_cpu = find_best_perf_cpu(env);
		break;
	case SCHED_POLICY_ENERGY:
		best_cpu = find_energy_cpu(env);
		break;
	default:
		best_cpu = INVALID_CPU;
	}

	return best_cpu;
}

static void find_overcap_cpus(struct tp_env *env)
{
	struct rq *rq;
	unsigned long cpu_util_with, cpu_util_wo, extra_util, capacity;
	unsigned long spare_cap, max_spare_cap = 0;
	int cpu, max_spare_cap_cpu = -1;

	cpumask_clear(&env->overcap_cpus);

	/*
	 * Find cpus that becomes over capacity with a given task.
	 * overcap_cpus = cpu capacity < cpu util + task util
	 */
	for_each_cpu(cpu, &env->cpus_allowed) {
		rq = cpu_rq(cpu);
		capacity = capacity_cpu_orig(cpu);
		extra_util = cpu_util_rt(rq) + cpu_util_dl(rq);

		cpu_util_wo = ml_cpu_util_without(cpu, env->p) + extra_util;
		cpu_util_wo = min(cpu_util_wo, capacity);
		spare_cap = capacity - cpu_util_wo;

		if (spare_cap > max_spare_cap) {
			max_spare_cap = spare_cap;
			max_spare_cap_cpu = cpu;
		}

		cpu_util_with = ml_cpu_util_with(env->p, cpu) + extra_util;
		if (cpu_util_with > capacity)
			cpumask_set_cpu(cpu, &env->overcap_cpus);
	}

	if (cpu_selected(max_spare_cap_cpu) &&
			cpumask_equal(&env->overcap_cpus, &env->cpus_allowed))
		cpumask_clear_cpu(max_spare_cap_cpu, &env->overcap_cpus);
}

static void find_migrating_cpus(struct tp_env *env)
{
	struct rq *rq;
	int cpu;

	/* Find cpus that will receive the task through migration */
	cpumask_clear(&env->migrating_cpus);
	for_each_cpu(cpu, &env->fit_cpus) {
		rq = cpu_rq(cpu);
		if (!ems_rq_migrated(rq))
			continue;

		cpumask_set_cpu(cpu, &env->migrating_cpus);
	}
}

enum task_class {
	BOOSTED_TEX_CLASS,
	BOOSTED_CLASS,
	NORMAL_CLASS,
};

static bool is_boosted_task(struct task_struct *p)
{
	if (emstune_sched_boost() && is_perf_task(p))
		return true;

	/* if prio > DEFAULT_PRIO and should_spread, skip boost task in level 2 */
	if (emstune_get_cur_level() == 2 && p->prio > DEFAULT_PRIO && emstune_should_spread())
		return false;

	if (is_gsc_task(p))
		return true;

	if (is_prio_tex_task(p))
		return true;

	if (emstune_support_uclamp()) {
		if (uclamp_boosted(p) && !is_small_task(p))
			return true;
	}

	return false;
}

static enum task_class get_task_class(struct task_struct *p)
{
	if (is_boosted_tex_task(p))
		return BOOSTED_TEX_CLASS;
	else if (is_boosted_task(p))
		return BOOSTED_CLASS;

	return NORMAL_CLASS;
}

static int get_init_index(struct tp_env *env)
{
	struct cpumask mask;
	int s_index = 0;
	int e_index = get_pe_list_size();
	int ot_index = 0, index;

	if (e_index == 1) {
		/* 1 cluster */
		return s_index;
	} else if (env->sched_policy == SCHED_POLICY_EXPRESS) {
		s_index = e_index - 1;
		return s_index;
	} else if (env->sched_policy == SCHED_POLICY_PERF) {
		s_index = 1;
	}

	/* Adjust ontime_fit_cpus to start_index of pe_list */
	ontime_select_fit_cpus(env->p, &mask);
	for (index = 0; index < e_index; index++) {
		struct pe_list *pe_list = get_pe_list(index);

		if (cpumask_intersects(&mask, &pe_list->cpus[0])) {
			ot_index = index;
			break;
		}
	}
	s_index = max(s_index, ot_index);
	cpumask_and(&env->fit_cpus, &env->fit_cpus, &mask);

	/* Raise the start_index until the capacity is enough */
	for (index = s_index; index < e_index; index++) {
		struct pe_list *pe_list = get_pe_list(index);
		unsigned long capacity = capacity_orig_of(cpumask_first(&pe_list->cpus[0]));

		if (ems_task_fits_capacity(env->p, capacity))
			break;
	}

	return min(index, e_index - 1);
}

static int get_end_index(struct tp_env *env)
{
	if (!emstune_should_spread())
		return 0;

	if (env->sched_policy != SCHED_POLICY_PERF)
		return 0;

	return 1;
}

static unsigned long get_base_cap(struct tp_env *env)
{
	struct pe_list *pl = get_pe_list(env->init_index);
	int cpu = cpumask_first(&pl->cpus[0]);

	return capacity_cpu_orig(cpu);
}

static void update_tp_env(struct tp_env *env)
{
	env->task_class = get_task_class(env->p);

	switch (env->task_class) {
		case BOOSTED_TEX_CLASS:
			env->sched_policy = SCHED_POLICY_EXPRESS;
			break;
		case BOOSTED_CLASS:
			env->sched_policy = SCHED_POLICY_PERF;
			break;
		case NORMAL_CLASS:
		default:
			env->sched_policy = SCHED_POLICY_ENERGY;
			break;
	}

	cpumask_copy(&env->fit_cpus, &env->cpus_allowed);

	env->init_index = get_init_index(env);
	env->end_index = get_end_index(env);
	env->base_cap = get_base_cap(env);
}

static int sysbusy_fit_cpus(struct tp_env *env)
{
	int target_cpu;

	if (!sysbusy_activated())
		return 0;

	cpumask_clear(&env->fit_cpus);
	target_cpu = sysbusy_schedule(env);
	if (cpu_selected(target_cpu)) {
		cpumask_set_cpu(target_cpu, &env->fit_cpus);
		env->reason_of_selection = FAIR_SYSBUSY;
	}

	return cpumask_weight(&env->fit_cpus);
}

#define PRIO_FOR_PERF 110

static bool can_use_fast_track(struct tp_env *env)
{
	if (env->sched_policy == SCHED_POLICY_ENERGY)
		return false;

	if (!cpumask_test_cpu(env->prev_cpu, &env->cpus_allowed))
		return false;

	if (!available_idle_cpu(env->prev_cpu))
		return false;

	if (capacity_cpu_orig(env->prev_cpu) != env->base_cap)
		return false;

	if (cpumask_test_cpu(env->prev_cpu, cpu_slowest_mask())) {
		if (is_perf_task(env->p))
			return false;
		if ((env->cgroup_idx == CGROUP_FOREGROUND) && (env->p->prio <= PRIO_FOR_PERF))
			return false;
	}

	return true;
}

static int normal_fit_cpus(struct tp_env *env)
{
	struct cpumask *fit_cpus = &env->fit_cpus;
	int this_cpu = smp_processor_id(), cpu;

	/*
	 * If cl_sync is true and the coregroup of this cpu is suitable for fit_cpus,
	 * select it as fit_cpus.
	 */
	if (env->cl_sync) {
		struct cpumask *coregroup_cpus = &pcpu_csd(this_cpu)->cpus;

		if (cpumask_intersects(fit_cpus, coregroup_cpus)) {
			cpumask_and(fit_cpus, fit_cpus, coregroup_cpus);
			if (cpumask_weight(fit_cpus) == 1)
				goto out;
		}
	}

	/* Handle sync flag */
	if (env->sync && can_sync_to_this_cpu(env, this_cpu)) {
		cpumask_clear(fit_cpus);
		cpumask_set_cpu(this_cpu, fit_cpus);
		env->reason_of_selection = FAIR_SYNC;
		goto out;
	}

	if (can_use_fast_track(env)) {
		cpumask_clear(fit_cpus);
		cpumask_set_cpu(task_cpu(env->p), fit_cpus);
		env->reason_of_selection = FAIR_FAST_TRACK;
		goto out;
	}

	/*
	 * Exclude overcap cpu from cpus_allowed. If there is only one or no
	 * fit cpus, it does not need to find fit cpus anymore.
	 */
	find_overcap_cpus(env);
	cpumask_andnot(&env->fit_cpus, &env->fit_cpus, &env->overcap_cpus);
	if (cpumask_weight(&env->fit_cpus) <= 1)
		goto out;

	/* Exclude migrating cpus from fit cpus */
	find_migrating_cpus(env);
	cpumask_andnot(&env->fit_cpus, &env->fit_cpus, &env->migrating_cpus);
	if (!cpumask_weight(&env->fit_cpus))
		cpumask_or(&env->fit_cpus, &env->fit_cpus, &env->migrating_cpus);

	for_each_cpu(cpu, &env->fit_cpus) {
		struct task_struct *curr = cpu_rq(cpu)->curr;

		if (is_boosted_tex_task(curr))
			cpumask_clear_cpu(cpu, &env->fit_cpus);
	}

out:
	return cpumask_weight(fit_cpus);
}

static int find_fit_cpus(struct tp_env *env)
{
	int num_of_cpus;

	update_tp_env(env);

	if (env->sched_policy == SCHED_POLICY_EXPRESS)
		num_of_cpus = tex_boosted_fit_cpus(env);
	else
		num_of_cpus = normal_fit_cpus(env);

	trace_ems_find_fit_cpus(env);

	return num_of_cpus;
}

/*
 * Return number of CPUs allowed.
 */
int find_cpus_allowed(struct tp_env *env)
{
	struct cpumask mask[4];

	/*
	 * take a snapshot of cpumask to get CPUs allowed
	 * - mask0 : p->cpus_ptr
	 * - mask1 : cpu_active_mask
	 * - mask2 : ecs_cpus_allowed
	 * - mask3 : cpus_binding_mask
	 */
	cpumask_copy(&mask[0], env->p->cpus_ptr);
	cpumask_copy(&mask[1], cpu_active_mask);
	cpumask_copy(&mask[2], ecs_cpus_allowed(env->p));
	cpumask_copy(&mask[3], cpus_binding_mask(env->p));

	cpumask_copy(&env->cpus_allowed, &mask[0]);
	if (env->per_cpu_kthread)
		goto out;

	if (!cpumask_intersects(&env->cpus_allowed, &mask[1]))
		goto out;
	cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask[1]);

	if (cpumask_intersects(&env->cpus_allowed, &mask[2]))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask[2]);

	if (cpumask_intersects(&env->cpus_allowed, &mask[3]))
		cpumask_and(&env->cpus_allowed, &env->cpus_allowed, &mask[3]);
out:

	trace_ems_find_cpus_allowed(env, mask);

	return cpumask_weight(&env->cpus_allowed);
}

extern char *fair_causes_name[END_OF_FAIR_CAUSES];
int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag)
{
	struct tp_env env = {
		.p = p,
		.cgroup_idx = cpuctl_task_group_idx(p),
		.per_cpu_kthread = is_per_cpu_kthread(p),
		.cl_sync = (wake_flag & WF_ANDROID_VENDOR) &&
			   !(current->flags & PF_EXITING),
		.sync = (wake_flag & WF_SYNC) &&
			!(current->flags & PF_EXITING),
		.prev_cpu = task_cpu(p),
		.task_util = ml_task_util_est(p),
		.task_util_clamped = ml_uclamp_task_util(p),
		.reason_of_selection = FAIR_FAILED,
	};
	int target_cpu = INVALID_CPU;
	int num_of_cpus;

	/* Find mandatory conditions for task allocation */
	num_of_cpus = find_cpus_allowed(&env);
	if (num_of_cpus == 0) {
		/* There is no CPU allowed, give up find new cpu */
		env.reason_of_selection = FAIR_ALLOWED_0;
		goto out;
	} else if (num_of_cpus == 1) {
		/* There is only one CPU allowed, no need to find cpu */
		target_cpu = cpumask_any(&env.cpus_allowed);
		env.reason_of_selection = FAIR_ALLOWED_1;
		goto out;
	}

	/* When sysbusy is detected, do scheduling under other policies */
	num_of_cpus = sysbusy_fit_cpus(&env);
	if (num_of_cpus > 0) {
		target_cpu = cpumask_any(&env.fit_cpus);
		goto out;
	}

	/* Find cpu candidates suitable for task operation */
	num_of_cpus = find_fit_cpus(&env);
	if (num_of_cpus == 0) {
		/* There is no fit cpus, keep the task on prev cpu */
		if (cpumask_test_cpu(prev_cpu, &env.cpus_allowed)) {
			target_cpu = prev_cpu;
			env.reason_of_selection = FAIR_FIT_0;
			goto out;
		}

		/*
		 * If prev_cpu is not allowed,
		 * copy cpus_allowed to fit_cpus to find appropriate cpu among cpus_allowed.
		 */
		cpumask_copy(&env.fit_cpus, &env.cpus_allowed);
	} else if (num_of_cpus == 1) {
		/* Only one cpu is fit. Select this cpu. */
		target_cpu = cpumask_any(&env.fit_cpus);
		if (env.reason_of_selection == FAIR_FAILED)
			env.reason_of_selection = FAIR_FIT_1;
		goto out;
	}

	target_cpu = find_best_cpu(&env);

	if (cpu_selected(target_cpu)) {
		if (!cpumask_test_cpu(target_cpu, &env.cpus_allowed)) {
			pr_err("Disallowed cpu%d is selected (cpus_allowed=%*pbl)\n",
					target_cpu, cpumask_pr_args(&env.cpus_allowed));
			WARN_ON(1);
		}
	} else {
		if (cpumask_test_cpu(prev_cpu, &env.cpus_allowed))
			target_cpu = prev_cpu;
		else
			target_cpu = cpumask_any(&env.cpus_allowed);

		env.reason_of_selection = FAIR_FAILED;
	}

out:
	update_fair_stat(smp_processor_id(), env.reason_of_selection);
	trace_ems_select_task_rq(&env, target_cpu, fair_causes_name[env.reason_of_selection]);

	return target_cpu;
}

int core_init(struct device_node *ems_dn)
{
	struct device_node *dn, *child;
	int index;

	dn = of_find_node_by_name(ems_dn, "cpu-selection-domain");
	if (unlikely(!dn))
		return -ENODATA;

	__pcpu_csd = alloc_percpu(struct cs_domain *);
	for_each_child_of_node(dn, child) {
		struct cs_domain *csd;
		const char *buf;
		int cpu;

		if (of_property_read_string(child, "cpus", &buf))
			return -ENODATA;

		csd = kzalloc(sizeof(struct cs_domain), GFP_KERNEL);
		if (unlikely(!csd))
			return -ENOMEM;

		cpulist_parse(buf, &csd->cpus);
		list_add_tail(&csd->list, &csd_head);

		for_each_cpu(cpu, &csd->cpus)
			*per_cpu_ptr(__pcpu_csd, cpu) = csd;
	}

	tex_init();
	cpus_binding_init();
	cpu_weight_init();

	index = match_string(sched_feat_names, __SCHED_FEAT_NR, "TTWU_QUEUE");
	if (index >= 0) {
		static_key_disable(&sched_feat_keys[index]);
		sysctl_sched_features &= ~(1UL << index);
	}

	return 0;
}
