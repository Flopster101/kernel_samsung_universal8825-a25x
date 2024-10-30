// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos Mobile CPUFreq
 *
 * Copyright (C) 2021, Intel Corporation
 * Author: Samsung Electronic. S.LSI CPU Part.
 */

#include <trace/events/ems_debug.h>

#include "../sched.h"
#include "ems.h"

static struct cpufreq_hook_data __percpu *cpufreq_hook_data;

/* register call back when cpufreq governor initialized */
void cpufreq_register_hook(int cpu, int (*func_get_next_cap)(struct tp_env *env,
				struct cpumask *cpus, int dst_cpu),
				int (*func_need_slack_timer)(void))
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data)
		return;

	data->func_get_next_cap = func_get_next_cap;
	data->func_need_slack_timer = func_need_slack_timer;
}

/* unregister call back when cpufreq governor initialized */
void cpufreq_unregister_hook(int cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data)
		return;

	data->func_get_next_cap = NULL;
	data->func_need_slack_timer = NULL;
}

/******************************************************************************
 *                           Next capacity callback                           *
 ******************************************************************************/
/*
 * default next capacity for cpu selection
 * it computs next capacity without governor specificaions
 */
static int default_get_next_cap(struct tp_env *env,
			struct cpumask *cpus, int dst_cpu)
{
	unsigned long next_cap = 0;
	int cpu;

	for_each_cpu_and(cpu, cpus, cpu_active_mask) {
		unsigned long util;

		if (cpu == dst_cpu) /* util with task */
			util = env->cpu_stat[cpu].util_with;
		else /* util without task */
			util = env->cpu_stat[cpu].util_wo;

		/*
		 * The cpu in the coregroup has same capacity and the
		 * capacity depends on the cpu with biggest utilization.
		 * Find biggest utilization in the coregroup and use it
		 * as max floor to know what capacity the cpu will have.
		 */
		if (util > next_cap)
			next_cap = util;
	}

	return next_cap;
}

/* return next cap */
int cpufreq_get_next_cap(struct tp_env *env,
			struct cpumask *cpus, int dst_cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data,
						cpumask_any(cpus));

	if (data && data->func_get_next_cap) {
		int cap = data->func_get_next_cap(env, cpus, dst_cpu);

		if (cap > 0)
			return cap;
	}

	return default_get_next_cap(env, cpus, dst_cpu);
}

/******************************************************************************
 *                                Slack timer                                 *
 ******************************************************************************/
static struct timer_list __percpu *slack_timer;

static void slack_timer_add(struct timer_list *timer, int cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data || !data->func_need_slack_timer)
		return;

	if (data->func_need_slack_timer()) {
		/* slack expired time = 20ms */
		timer->expires = jiffies + msecs_to_jiffies(20);
		add_timer_on(timer, cpu);
	}
}

void slack_timer_cpufreq(int cpu, bool idle_start, bool idle_end)
{
	struct timer_list *timer = per_cpu_ptr(slack_timer, cpu);

	if (idle_start == idle_end)
		return;

	if (timer_pending(timer))
		del_timer_sync(timer);

	if (idle_start)
		slack_timer_add(timer, cpu);
}

static void slack_timer_func(struct timer_list *timer)
{
	int this_cpu = smp_processor_id();

	/*
	 * The purpose of slack-timer is to wake up the CPU from IDLE, in order
	 * to decrease its frequency if it is not set to minimum already.
	 *
	 * This is important for platforms where CPU with higher frequencies
	 * consume higher power even at IDLE.
	 */
	trace_slack_timer(this_cpu);

	/* If slack timer is still needed, add slack timer again */
	slack_timer_add(timer, this_cpu);
}

static void slack_timer_init(void)
{
	int cpu;

	slack_timer = alloc_percpu(struct timer_list);

	for_each_cpu(cpu, cpu_possible_mask)
		timer_setup(per_cpu_ptr(slack_timer, cpu),
				slack_timer_func, TIMER_PINNED);
}

/******************************************************************************
 *                                 Freq clamp                                 *
 ******************************************************************************/
#define FCLAMP_MIN	0
#define FCLAMP_MAX	1

struct fclamp {
	struct fclamp_data fclamp_min;
	struct fclamp_data fclamp_max;
} __percpu **fclamp;

#define per_cpu_fc(cpu)		(*per_cpu_ptr(fclamp, cpu))

static int fclamp_monitor_group[CGROUP_COUNT];

static struct fclamp_data boost_fcd;
static int fclamp_boost;

static int fclamp_skip_monitor_group(int boost, int group)
{
	/* Monitor all cgroups during boost. */
	if (boost)
		return 0;

	return !fclamp_monitor_group[group];
}

static int fclamp_can_release(int cpu, struct fclamp_data *fcd, unsigned int boost)
{
	int period;
	int target_period, target_ratio, type, active_ratio = 0;

	target_period = fcd->target_period;
	target_ratio = fcd->target_ratio;
	type = fcd->type;

	/*
	 * Let's traversal consecutive windows of active ratio by target
	 * period to check whether there's a busy or non-busy window.
	 */
	period = mlt_cur_period(cpu);
	while (target_period) {
		int group;

		for (group = 0; group < CGROUP_COUNT; group++) {
			if (fclamp_skip_monitor_group(boost, group))
				continue;

			active_ratio += mlt_art_cgroup_value(cpu,
						period, group);
		}

		trace_fclamp_can_release(cpu, type, period, target_period,
					target_ratio, active_ratio);

		target_period--;
		period = mlt_prev_period(period);
	};

	/* convert ratio normalized to 1024 to percentage and get average */
	active_ratio = active_ratio * 100 / SCHED_CAPACITY_SCALE;
	active_ratio /= fcd->target_period;

	/*
	 * Release fclamp max if cpu is busier than target and
	 * release fclamp min if cpu is more idle than target.
	 */
	if (type == FCLAMP_MAX && active_ratio > target_ratio)
		return 1;
	if (type == FCLAMP_MIN && active_ratio < target_ratio)
		return 1;

	return 0;
}

unsigned int
fclamp_apply(struct cpufreq_policy *policy, unsigned int orig_freq)
{
	struct fclamp *fc = per_cpu_fc(policy->cpu);
	struct fclamp_data *fcd;
	int cpu, type;
	unsigned int new_freq, count = 0, boost = fclamp_boost;

	/* Select fclamp data according to boost or origin util */
	if (boost) {
		/*
		 * If orig freq is same as max freq, it does not need to
		 * handle fclamp boost
		 */
		if (orig_freq == policy->cpuinfo.max_freq)
			return orig_freq;
		fcd = &boost_fcd;
	} else if (orig_freq > fc->fclamp_max.freq)
		fcd = &fc->fclamp_max;
	else if (orig_freq < fc->fclamp_min.freq)
		fcd = &fc->fclamp_min;
	else
		return orig_freq;

	if (!fcd->target_period)
		return orig_freq;

	type = fcd->type;
	new_freq = fcd->freq;

	for_each_cpu(cpu, policy->cpus) {
		/* check whether release clamping or not */
		if (fclamp_can_release(cpu, fcd, boost)) {
			count++;

			if (type == FCLAMP_MAX)
				break;
		}
	}

	/* Release fclamp min when all cpus in policy are idler than target */
	if (type == FCLAMP_MIN && count == cpumask_weight(policy->cpus))
		new_freq = orig_freq;
	/* Release fclamp max when any cpu in policy is busier than target */
	if (type == FCLAMP_MAX && count > 0)
		new_freq = orig_freq;

	new_freq = clamp_val(new_freq, policy->cpuinfo.min_freq,
				       policy->cpuinfo.max_freq);

	trace_fclamp_apply(policy->cpu, orig_freq, new_freq, boost, fcd);

	return new_freq;
}

static int fclamp_emstune_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i, cpu;

	for_each_possible_cpu(cpu) {
		struct fclamp *fc = per_cpu_fc(cpu);

		fc->fclamp_min.freq = cur_set->fclamp.min_freq[cpu];
		fc->fclamp_min.target_period =
				cur_set->fclamp.min_target_period[cpu];
		fc->fclamp_min.target_ratio =
				cur_set->fclamp.min_target_ratio[cpu];

		fc->fclamp_max.freq = cur_set->fclamp.max_freq[cpu];
		fc->fclamp_max.target_period =
				cur_set->fclamp.max_target_period[cpu];
		fc->fclamp_max.target_ratio =
				cur_set->fclamp.max_target_ratio[cpu];
	}

	for (i = 0; i < CGROUP_COUNT; i++)
		fclamp_monitor_group[i] = cur_set->fclamp.monitor_group[i];

	return NOTIFY_OK;
}

static struct notifier_block fclamp_emstune_notifier = {
	.notifier_call = fclamp_emstune_notifier_call,
};

static int fclamp_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	fclamp_boost = state;

	return NOTIFY_OK;
}

static struct notifier_block fclamp_sysbusy_notifier = {
	.notifier_call = fclamp_sysbusy_notifier_call,
};

static int fclamp_init(void)
{
	int cpu;

	fclamp = alloc_percpu(struct fclamp *);
	if (!fclamp) {
		pr_err("failed to allocate fclamp\n");
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		struct fclamp *fc;
		int i;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		fc = kzalloc(sizeof(struct fclamp), GFP_KERNEL);
		if (!fc)
			return -ENOMEM;

		fc->fclamp_min.type = FCLAMP_MIN;
		fc->fclamp_max.type = FCLAMP_MAX;

		for_each_cpu(i, cpu_coregroup_mask(cpu))
			per_cpu_fc(i) = fc;
	}

	/* monitor topapp as default */
	fclamp_monitor_group[CGROUP_TOPAPP] = 1;

	boost_fcd.freq = INT_MAX;
	boost_fcd.target_period = 1;
	boost_fcd.target_ratio = 80;
	boost_fcd.type = FCLAMP_MIN;

	emstune_register_notifier(&fclamp_emstune_notifier);
	sysbusy_register_notifier(&fclamp_sysbusy_notifier);

	return 0;
}

int cpufreq_init(void)
{
	cpufreq_hook_data = alloc_percpu(struct cpufreq_hook_data);
	if (!cpufreq_hook_data) {
		pr_err("Failed to allocate cpufreq_hook_data\n");
		return -ENOMEM;
	}

	slack_timer_init();
	fclamp_init();

	return 0;
}
