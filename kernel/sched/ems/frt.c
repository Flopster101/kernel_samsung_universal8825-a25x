#include "../sched.h"
#include "../pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

struct frt_dom {
	unsigned int		active_ratio;
	struct cpumask		cpus;

	struct list_head	list;
	struct frt_dom		*next;
	struct frt_dom		*prev;
	/* kobject for sysfs group */
	struct kobject		kobj;
};

struct cpumask available_mask;

LIST_HEAD(frt_list);
DEFINE_RAW_SPINLOCK(frt_lock);
static struct frt_dom __percpu **frt_rqs;

static struct cpumask __percpu **frt_local_cpu_mask;

#define RATIO_SCALE_SHIFT	10
#define ratio_scale(v, r) (((v) * (r) * 10) >> RATIO_SCALE_SHIFT)

#define cpu_selected(cpu)	(cpu >= 0)

static u64 frt_cpu_util(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	u64 cpu_util;

	cpu_util = ml_cpu_util(cpu);
	cpu_util += READ_ONCE(rq->avg_rt.util_avg) + READ_ONCE(rq->avg_dl.util_avg);

	return cpu_util;
}

static const struct cpumask *get_available_cpus(void)
{
	return &available_mask;
}

static int frt_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct frt_dom *dom;

	list_for_each_entry(dom, &frt_list, list) {
		int cpu = cpumask_first(&dom->cpus);

		dom->active_ratio = cur_set->frt.active_ratio[cpu];
	}

	return NOTIFY_OK;
}

static struct notifier_block frt_mode_update_notifier = {
	.notifier_call = frt_mode_update_callback,
};

int frt_init(struct kobject *ems_kobj)
{
	struct frt_dom *cur, *prev = NULL, *head = NULL;
	struct device_node *dn;
	int cpu, tcpu;

	dn = of_find_node_by_path("/ems");
	if (!dn)
		return 0;

	frt_rqs = alloc_percpu(struct frt_dom *);
	if (!frt_rqs) {
		pr_err("failed to alloc frt_rqs\n");
		return -ENOMEM;
	}

	frt_local_cpu_mask = alloc_percpu(struct cpumask *);
	if (!frt_local_cpu_mask) {
		pr_err("failed to alloc frt_local_cpu_mask\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&frt_list);
	cpumask_setall(&available_mask);

	for_each_possible_cpu(cpu) {
		*per_cpu_ptr(frt_local_cpu_mask, cpu) = NULL;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		cur = kzalloc(sizeof(struct frt_dom), GFP_KERNEL);
		if (!cur) {
			pr_err("FRT(%s): failed to allocate dom\n", __func__);
			goto put_node;
		}

		cpumask_copy(&cur->cpus, cpu_coregroup_mask(cpu));

		/* make linke between rt domains */
		if (head == NULL)
			head = cur;

		if (prev) {
			prev->next = cur;
			cur->prev = prev;
		}
		cur->next = head;
		head->prev = cur;

		prev = cur;

		for_each_cpu(tcpu, &cur->cpus)
			*per_cpu_ptr(frt_rqs, tcpu) = cur;

		list_add_tail(&cur->list, &frt_list);
	}

	emstune_register_notifier(&frt_mode_update_notifier);

	pr_info("%s: frt initialized complete!\n", __func__);

put_node:
	of_node_put(dn);

	return 0;
}

/*****************************************************************************/
/*				SELECT WAKEUP CPU			     */
/*****************************************************************************/
void frt_update_available_cpus(struct rq *rq)
{
	struct frt_dom *dom;
	struct cpumask mask, active_cpus;
	unsigned long flags;
	unsigned long capacity, dom_util_sum = 0;
	int cpu;

	if (rq->cpu != 0)
		return;

	if (!raw_spin_trylock_irqsave(&frt_lock, flags))
		return;

	cpumask_clear(&mask);
	list_for_each_entry(dom, &frt_list, list) {
		cpumask_and(&active_cpus, &dom->cpus, cpu_active_mask);

		/* this domain is off */
		if (cpumask_empty(&active_cpus))
			continue;

		for_each_cpu(cpu, &active_cpus)
			dom_util_sum += frt_cpu_util(cpu);

		capacity = capacity_cpu_orig(cpumask_any(&active_cpus)) * cpumask_weight(&active_cpus);

		/*
		 * domains with util greater than 80% of domain capacity are excluded
		 * from available_mask.
		 */
		if (check_busy(dom_util_sum, capacity))
			continue;

		cpumask_or(&mask, &mask, &dom->cpus);

		trace_frt_available_cpus(*(unsigned int *)cpumask_bits(&active_cpus), dom_util_sum,
				capacity * dom->active_ratio / 100,
				*(unsigned int *)cpumask_bits(&mask));

		/*
		 * If the percentage of domain util is less than active_ratio,
		 * this domain is idle.
		 */
		if (dom_util_sum * 100 < capacity * dom->active_ratio)
			break;
	}

	cpumask_copy(&available_mask, &mask);

	raw_spin_unlock_irqrestore(&frt_lock, flags);
}

static int find_idle_cpu(struct task_struct *task)
{
	int cpu, best_cpu = -1;
	int cpu_prio, max_prio = -1;
	u64 cpu_util, min_util = ULLONG_MAX;
	struct cpumask candidate_cpus;
	struct frt_dom *dom, *prefer_dom;
	int nr_cpus_allowed;
	struct tp_env env = {
		.p = task,
	};

	nr_cpus_allowed = find_cpus_allowed(&env);
	if (!nr_cpus_allowed)
		return best_cpu;

	if (nr_cpus_allowed == 1)
		return cpumask_any(&env.cpus_allowed);

	cpumask_and(&candidate_cpus, &env.cpus_allowed, get_available_cpus());
	if (unlikely(cpumask_empty(&candidate_cpus)))
		cpumask_copy(&candidate_cpus, &env.cpus_allowed);

	prefer_dom = dom = *per_cpu_ptr(frt_rqs, 0);
	do {
		for_each_cpu_and(cpu, &dom->cpus, &candidate_cpus) {
			if (!available_idle_cpu(cpu))
				continue;

			cpu_prio = cpu_rq(cpu)->rt.highest_prio.curr;
			if (cpu_prio < max_prio)
				continue;

			cpu_util = frt_cpu_util(cpu);

			if ((cpu_prio > max_prio)
				|| (cpu_util < min_util)
				|| (cpu_util == min_util && task_cpu(task) == cpu)) {
				min_util = cpu_util;
				max_prio = cpu_prio;
				best_cpu = cpu;
			}
		}

		if (cpu_selected(best_cpu)) {
			trace_frt_select_task_rq(task, best_cpu, "IDLE-FIRST");
			return best_cpu;
		}

		dom = dom->next;
	} while (dom != prefer_dom);

	return best_cpu;
}

static int find_recessive_cpu(struct task_struct *task,
				struct cpumask *lowest_mask)
{
	int cpu, best_cpu = -1;
	u64 cpu_util, min_util= ULLONG_MAX;
	struct cpumask candidate_cpus;
	struct frt_dom *dom, *prefer_dom;
	int nr_cpus_allowed;
	int cpu_prio, max_prio = -1;
	struct tp_env env = {
		.p = task,
	};

	if (unlikely(*this_cpu_ptr(frt_local_cpu_mask) == NULL)) {
		if (lowest_mask) {
			/* Save lowest_mask per CPU once */
			*this_cpu_ptr(frt_local_cpu_mask) = lowest_mask;
		} else {
			/* Make sure the mask is initialized first */
			trace_frt_select_task_rq(task, best_cpu, "NA LOWESTMSK");
			return best_cpu;
		}
	}

	lowest_mask = *this_cpu_ptr(frt_local_cpu_mask);

	nr_cpus_allowed = find_cpus_allowed(&env);
	if (!nr_cpus_allowed)
		return best_cpu;

	if (nr_cpus_allowed == 1)
		return cpumask_any(&env.cpus_allowed);

	cpumask_and(&candidate_cpus, &env.cpus_allowed, get_available_cpus());
	if (unlikely(cpumask_empty(&candidate_cpus)))
		return best_cpu;

	if (cpumask_intersects(&candidate_cpus, lowest_mask))
		cpumask_and(&candidate_cpus, &candidate_cpus, lowest_mask);

	prefer_dom = dom = *per_cpu_ptr(frt_rqs, 0);
	do {
		for_each_cpu_and(cpu, &dom->cpus, &candidate_cpus) {
			cpu_util = frt_cpu_util(cpu);

			cpu_prio = cpu_rq(cpu)->rt.highest_prio.curr;
			if (cpu_prio < max_prio)
				continue;

			cpu_util = frt_cpu_util(cpu);
			if ((cpu_prio > max_prio)
				|| (cpu_prio == max_prio && cpu_util < min_util)) {
				min_util = cpu_util;
				max_prio = cpu_prio;
				best_cpu = cpu;
			}
		}

		if (cpu_selected(best_cpu)) {
			trace_frt_select_task_rq(task, best_cpu,
				rt_task(cpu_rq(best_cpu)->curr) ? "RT-RECESS" : "FAIR-RECESS");
			return best_cpu;
		}

		dom = dom->next;
	} while (dom != prefer_dom);

	return best_cpu;
}

int frt_find_lowest_rq(struct task_struct *task, struct cpumask *lowest_mask)
{
	int best_cpu = -1;

	if (task->nr_cpus_allowed == 1) {
		trace_frt_select_task_rq(task, best_cpu, "NA ALLOWED");
		update_rt_stat(smp_processor_id(), RT_ALLOWED_1);
		return best_cpu;
	}

	if (!rt_task(task))
		return best_cpu;
	/*
	 * Fluid Sched Core selection procedure:
	 *
	 * 1. idle CPU selection
	 * 2. recessive task first
	 */

	/* 1. idle CPU selection */
	best_cpu = find_idle_cpu(task);
	if (cpu_selected(best_cpu)) {
		update_rt_stat(smp_processor_id(), RT_IDLE);
		goto out;
	}

	/* 2. recessive task first */
	best_cpu = find_recessive_cpu(task, lowest_mask);
	if (cpu_selected(best_cpu)) {
		update_rt_stat(smp_processor_id(), RT_RECESSIVE);
		goto out;
	}

out:
	if (!cpu_selected(best_cpu)) {
		best_cpu = task_rq(task)->cpu;
		update_rt_stat(smp_processor_id(), RT_FAILED);
	}

	if (!cpumask_test_cpu(best_cpu, cpu_active_mask)) {
		trace_frt_select_task_rq(task, best_cpu, "NOTHING_VALID");
		best_cpu = -1;
	}

	return best_cpu;
}

static inline bool frt_can_sync_to_cur_cpu(struct rq *cur_rq, struct task_struct *p,
					   int cur_cpu)
{
	if (!cpu_active(cur_cpu))
		return false;

	if (!cpumask_test_cpu(cur_cpu, p->cpus_ptr))
		return false;

	if (p->prio > cur_rq->rt.highest_prio.next)
		return false;

	if (cur_rq->rt.rt_nr_running > 2)
		return false;

	return true;

}

int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu,
			  int sd_flag, int wake_flags)
{
	struct task_struct *curr;
	struct rq *rq, *cur_rq;
	bool sync = !!(wake_flags & WF_SYNC);
	int target_cpu = -1, cur_cpu;

	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK)
		goto out;

	cur_cpu = raw_smp_processor_id();
	cur_rq = cpu_rq(cur_cpu);

	/* Handle sync flag */
	if (sync && frt_can_sync_to_cur_cpu(cur_rq, p, cur_cpu)) {
		target_cpu = cur_cpu;
		goto out;
	}

	rq = cpu_rq(prev_cpu);

	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */
	if (curr) {
		/*
		 * Even though the destination CPU is running
		 * a higher priority task, FluidRT can bother moving it
		 * when its utilization is very small, and the other CPU is too busy
		 * to accomodate the p in the point of priority and utilization.
		 *
		 * BTW, if the curr has higher priority than p, FluidRT tries to find
		 * the other CPUs first. In the worst case, curr can be victim, if it
		 * has very small utilization.
		 */
		int cpu = frt_find_lowest_rq(p, NULL);

		if (likely(cpu >= 0))
			target_cpu = cpu;
		else
			target_cpu = prev_cpu;
	}

	rcu_read_unlock();

out:
	return target_cpu;
}
