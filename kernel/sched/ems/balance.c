#include <dt-bindings/soc/samsung/ems.h>

#include "../sched.h"
#include "ems.h"

#include <trace/hooks/sched.h>
#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#define TINY_TASK_RATIO_SHIFT	3	/* 12.5% */
#define BUSY_GROUP_RATIO_SHIFT	2	/* 25% */

struct lb_env {
	struct rq *src_rq;
	struct rq *dst_rq;
	struct task_struct *push_task;
	u64 flags;
};
static struct lb_env __percpu *lb_env;
static struct cpu_stop_work __percpu *lb_work;

static bool slowest_has_misfit(int dst_cpu)
{
	int cpu;

	if (cpumask_test_cpu(dst_cpu, cpu_slowest_mask()))
			return false;

	for_each_cpu(cpu, cpu_slowest_mask())
		if (ems_rq_nr_misfited(cpu_rq(cpu)))
			return true;

	return false;
}

void lb_enqueue_misfit_task(struct task_struct *p, struct rq *rq)
{
	bool cur_misfit = !ems_task_fits_max_cap(p, cpu_of(rq));

	if (cur_misfit)
		ems_rq_update_nr_misfited(rq, true);
	else
		ems_task_misfited(p) = false;

	trace_lb_update_misfit(p, true, cur_misfit,
		cpu_of(rq), ems_rq_nr_misfited(rq), "enqueue");
}

void lb_dequeue_misfit_task(struct task_struct *p, struct rq *rq)
{
	ems_rq_update_nr_misfited(rq, false);

	trace_lb_update_misfit(p, true, true,
		cpu_of(rq), ems_rq_nr_misfited(rq), "dequeue");
}

void lb_update_misfit_status(struct task_struct *p,
			struct rq *rq, bool *need_update)
{
	bool old_misfit, cur_misfit;

	if (!p)
		return;

	old_misfit = ems_task_misfited(p);
	cur_misfit = !ems_task_fits_max_cap(p, rq->cpu);

	if (cur_misfit != old_misfit) {
		ems_task_misfited(p) = cur_misfit;
		ems_rq_update_nr_misfited(rq, cur_misfit);
	}
	trace_lb_update_misfit(p, old_misfit, cur_misfit,
		cpu_of(rq), ems_rq_nr_misfited(rq), "update");
}

void lb_nohz_balancer_kick(struct rq *rq, unsigned int *flag, int *done)
{
	*done = true;

	/*
	 * tick path migration takes care of misfit task.
	 * so we have to check for nr_running >= 2 here.
	 */
	if (rq->nr_running >= 2 && cpu_overutilized(rq->cpu)) {
		*flag = NOHZ_KICK_MASK;
		trace_lb_nohz_balancer_kick(rq);
	}
}

static bool _lb_can_migrate_task(struct task_struct *p, int src_cpu, int dst_cpu)
{
	/* boosted task can't migrate to slower cpu */
	if (capacity_orig_of(dst_cpu) < capacity_orig_of(src_cpu)) {
		if (is_boosted_tex_task(p)) {
			trace_lb_can_migrate_task(p, dst_cpu, false, "tex");
			return false;
		}
		if (sysbusy_boost_task(p)) {
			trace_lb_can_migrate_task(p, dst_cpu, false, "sysbusy-boost");
			return false;
		}
	}

	if (!ontime_can_migrate_task(p, dst_cpu)) {
		trace_lb_can_migrate_task(p, dst_cpu, false, "ontime");
		return false;
	}

	if (!cpumask_test_cpu(dst_cpu, cpus_binding_mask(p))) {
		trace_lb_can_migrate_task(p, dst_cpu, false, "ems-binded");
		return false;
	}

	if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr)) {
		trace_lb_can_migrate_task(p, dst_cpu, false, "cpus-allowed");
		return false;
	}

	trace_lb_can_migrate_task(p, dst_cpu, true, "can-migrate");

	return true;
}

bool lb_busiest_queue_pre_condition(struct rq *rq, bool check_overutil)
{
	/* if there is no fair task, we can't balance */
	if (!rq->cfs.h_nr_running)
		return false;

	/* if rq is in the active balance, will be less busy */
	if (rq->active_balance)
		return false;

	if (check_overutil && !cpu_overutilized(cpu_of(rq)))
		return false;

	return true;
}

/*
 * This function called when src rq has only one task,
 * and need to check whether need an active balance or not
 */
bool lb_queue_need_active_mgt(struct rq *src, struct rq *dst)
{
	if (!ems_rq_nr_misfited(src))
		return false;

	if (!available_idle_cpu(cpu_of(dst)))
		return false;

	return true;
}


bool lb_group_is_busy(struct cpumask *cpus,
			int nr_task, unsigned long util)
{
	unsigned long capacity = capacity_orig_of(cpumask_first(cpus));
	int nr_cpus = cpumask_weight(cpus);
	/*
	 * if there is a available cpu in the group,
	 * this group is not busy
	 */
	if (nr_task <= nr_cpus)
		return false;

	if ((util + (util >> BUSY_GROUP_RATIO_SHIFT)) < (capacity * nr_cpus))
		return false;

	return true;
}

static void lb_find_busiest_faster_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	int src_cpu, busiest_cpu = -1;
	struct cpumask candidate_mask;
	unsigned long util, busiest_util = 0;
	unsigned long util_sum = 0, nr_task_sum = 0;
	unsigned long tiny_task_util;

	tiny_task_util = capacity_orig_of(cpumask_first(src_cpus)) >> TINY_TASK_RATIO_SHIFT;
	cpumask_and(&candidate_mask, src_cpus, cpu_active_mask);
	cpumask_and(&candidate_mask, &candidate_mask, ecs_available_cpus());
	if (cpumask_empty(&candidate_mask))
		return;

	for_each_cpu(src_cpu, &candidate_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "faster");

		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		util_sum += util;
		nr_task_sum += src_rq->cfs.h_nr_running;

		if (!lb_busiest_queue_pre_condition(src_rq, true))
			continue;

		if (src_rq->cfs.h_nr_running < 2)
			continue;

		if (src_rq->cfs.h_nr_running == 2 &&
			ml_task_util(src_rq->curr) < tiny_task_util)
			continue;

		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
	}

	/*
	 * Don't allow migrating to lower cluster unless
	 * this faster cluster is sufficiently loaded.
	 */
	if (!lb_group_is_busy(&candidate_mask, nr_task_sum, util_sum))
		return;

	if (busiest_cpu != -1)
		*busiest = cpu_rq(busiest_cpu);
}

static void lb_find_busiest_slower_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	int src_cpu, busiest_cpu = -1;
	unsigned long util, busiest_util = 0;


	for_each_cpu_and(src_cpu, src_cpus, cpu_active_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "slower");

		if (!lb_busiest_queue_pre_condition(src_rq, true))
			continue;

		if (src_rq->cfs.h_nr_running == 1 &&
			!lb_queue_need_active_mgt(src_rq, cpu_rq(dst_cpu)))
			continue;

		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
	}

	if (busiest_cpu != -1)
		*busiest = cpu_rq(busiest_cpu);
}

static void lb_find_busiest_equivalent_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	int src_cpu, busiest_cpu = -1;
	unsigned long util, busiest_util = 0;

	for_each_cpu_and(src_cpu, src_cpus, cpu_active_mask) {
		struct rq *src_rq = cpu_rq(src_cpu);

		trace_lb_cpu_util(src_cpu, "equal");

		if (!lb_busiest_queue_pre_condition(src_rq, false))
			continue;

		if (src_rq->nr_running < 2)
			continue;

		/* find highest util cpu */
		util = ml_cpu_util(src_cpu) + cpu_util_rt(src_rq);
		if (util < busiest_util)
			continue;

		busiest_util = util;
		busiest_cpu = src_cpu;
	}

	if (busiest_cpu != -1)
		*busiest = cpu_rq(busiest_cpu);
}

static void __lb_find_busiest_queue(int dst_cpu,
		struct cpumask *src_cpus, struct rq **busiest)
{
	unsigned long dst_capacity, src_capacity;

	src_capacity = capacity_orig_of(cpumask_first(src_cpus));
	dst_capacity = capacity_orig_of(dst_cpu);
	if (dst_capacity == src_capacity)
		lb_find_busiest_equivalent_queue(dst_cpu, src_cpus, busiest);
	else if (dst_capacity > src_capacity)
		lb_find_busiest_slower_queue(dst_cpu, src_cpus, busiest);
	else
		lb_find_busiest_faster_queue(dst_cpu, src_cpus, busiest);
}

void lb_find_busiest_queue(int dst_cpu, struct sched_group *group,
		struct cpumask *env_cpus, struct rq **busiest, int *done)
{
	int cpu = -1;
	struct cpumask src_cpus;

	*done = true;

	cpumask_and(&src_cpus, sched_group_span(group), env_cpus);

	/* if group has only one cpu, don't need to traverse group */
	if (cpumask_weight(&src_cpus) == 1)
		*busiest = cpu_rq(cpumask_first(&src_cpus));
	else
		__lb_find_busiest_queue(dst_cpu, &src_cpus, busiest);

	if (*busiest)
		cpu = cpu_of(*busiest);

	trace_lb_find_busiest_queue(dst_cpu, cpu, &src_cpus);
}

void lb_can_migrate_task(struct task_struct *p, int dst_cpu, int *can_migrate)
{
	int src_cpu = task_cpu(p);

	if (_lb_can_migrate_task(p, src_cpu, dst_cpu))
		return;

	*can_migrate = false;
}

static int lb_active_migration_stop(void *data)
{
	struct lb_env *env = data;
	struct rq *src_rq = env->src_rq, *dst_rq = env->dst_rq;
	struct rq_flags rf;
	int ret;

	rq_lock_irq(src_rq, &rf);
	ret = detach_one_task(src_rq, dst_rq, env->push_task);
	src_rq->active_balance = 0;
	ems_rq_migrated(dst_rq) = false;
	rq_unlock(src_rq, &rf);

	if (ret)
		attach_one_task(dst_rq, env->push_task);

	local_irq_enable();

	return 0;
}

static bool lb_task_need_active_mgt(struct task_struct *p, int dst_cpu, int src_cpu)
{
	if (capacity_orig_of(dst_cpu) <= capacity_orig_of(src_cpu))
		return false;

	if (!ems_task_misfited(p))
		return false;

	return true;
}

static int lb_idle_pull_tasks(int dst_cpu, int src_cpu)
{
	struct lb_env *env = per_cpu_ptr(lb_env, src_cpu);
	struct rq *dst_rq = cpu_rq(dst_cpu);
	struct rq *src_rq = cpu_rq(src_cpu);
	struct task_struct *pulled_task = NULL, *p;
	bool active_balance = false;
	unsigned long flags;

	raw_spin_rq_lock_irqsave(src_rq, flags);
	list_for_each_entry_reverse(p, &src_rq->cfs_tasks, se.group_node) {
		if (!_lb_can_migrate_task(p, src_cpu, dst_cpu))
			continue;

		if (task_running(src_rq, p)) {
			if (lb_task_need_active_mgt(p, dst_cpu, src_cpu)) {
				pulled_task = p;
				active_balance = true;
				break;
			}
			continue;
		}

		if (can_migrate(p, dst_cpu)) {
			update_rq_clock(src_rq);
			detach_task(src_rq, dst_rq, p);
			pulled_task = p;
			break;
		}
	}

	if (!pulled_task) {
		raw_spin_rq_unlock_irqrestore(src_rq, flags);
		return 0;
	}

	if (active_balance) {
		if (src_rq->active_balance) {
			raw_spin_rq_unlock_irqrestore(src_rq, flags);
			return 0;
		}

		src_rq->active_balance = 1;
		ems_rq_migrated(dst_rq) = true;
		env->src_rq = src_rq;
		env->dst_rq = dst_rq;
		env->push_task = pulled_task;

		/* lock must be dropped before waking the stopper */
		raw_spin_rq_unlock_irqrestore(src_rq, flags);

		trace_lb_active_migration(p, src_cpu, dst_cpu, "idle");
		stop_one_cpu_nowait(src_cpu, lb_active_migration_stop,
				env, per_cpu_ptr(lb_work, src_cpu));

		/* we did not pull any task here */
		return 0;
	}

	raw_spin_rq_unlock(src_rq);

	attach_one_task(dst_rq, pulled_task);

	local_irq_restore(flags);

	return 1;
}

static int compute_range(int cpu, int cl_idx, bool short_idle)
{
	int range = get_pe_list(cl_idx)->num_of_cpus;
	bool slowest_misfit = slowest_has_misfit(cpu);
	bool fastest = cpumask_test_cpu(cpu, cpu_fastest_mask());
	bool slowest = cpumask_test_cpu(cpu, cpu_slowest_mask());

	if (short_idle && !slowest_misfit)
		return 1;

	if ((range >= MAX_CLUSTER_NUM)
		&& (fastest || slowest))
		range -= 1;

	return range;
}

#define NIB_AVG_IDLE_THRESHOLD	500000
static bool determine_short_idle(u64 avg_idle)
{
	u64 idle_threshold = NIB_AVG_IDLE_THRESHOLD;

	if (emstune_should_spread())
		idle_threshold >>= 2;

	return avg_idle < idle_threshold;
}

static int lb_has_pushable_tasks(struct rq *rq)
{
	return !plist_head_empty(&rq->rt.pushable_tasks);
}

static int lb_pick_rt_task(struct rq *rq, struct task_struct *p, int cpu)
{
	if (!task_running(rq, p) &&
	    cpumask_test_cpu(cpu, p->cpus_ptr))
		return 1;

	return 0;
}

static struct task_struct *lb_pick_highest_pushable_task(struct rq *rq, int cpu)
{
	struct plist_head *head = &rq->rt.pushable_tasks;
	struct task_struct *p;

	if (!lb_has_pushable_tasks(rq))
		return NULL;

	plist_for_each_entry(p, head, pushable_tasks) {
		if (lb_pick_rt_task(rq, p, cpu))
			return p;
	}

	return NULL;
}

#define MIN_RUNNABLE_THRESHOLD	(500000)
static bool lb_short_runnable(struct task_struct *p)
{
	return ktime_get_ns() - ems_last_waked(p) < MIN_RUNNABLE_THRESHOLD;
}

static int lb_idle_pull_tasks_rt(struct rq *dst_rq)
{
	struct rq *src_rq;
	struct task_struct *pulled_task = NULL;
	int cpu, src_cpu = -1, dst_cpu = dst_rq->cpu, ret = 0;

	if (sched_rt_runnable(dst_rq))
		return 0;

	for_each_possible_cpu(cpu) {
		if (lb_has_pushable_tasks(cpu_rq(cpu))) {
			src_cpu = cpu;
			break;
		}
	}

	if (src_cpu == -1)
		return 0;

	if (src_cpu == dst_cpu)
		return 0;

	src_rq = cpu_rq(src_cpu);
	double_lock_balance(dst_rq, src_rq);

	if (sched_rt_runnable(dst_rq))
		goto out;

	pulled_task = lb_pick_highest_pushable_task(src_rq, dst_cpu);
	if (!pulled_task)
		goto out;

	if (lb_short_runnable(pulled_task))
		goto out;

	deactivate_task(src_rq, pulled_task, 0);
	set_task_cpu(pulled_task, dst_cpu);
	activate_task(dst_rq, pulled_task, 0);
	ret = 1;

out:
	double_unlock_balance(dst_rq, src_rq);

	trace_lb_idle_pull_tasks_rt(src_cpu, pulled_task, ret);

	return ret;
}

void lb_newidle_balance(struct rq *dst_rq, struct rq_flags *rf,
				int *pulled_task, int *done)
{
	int dst_cpu = dst_rq->cpu;
	int cl_idx = ems_rq_cluster_idx(dst_rq);
	bool short_idle;
	struct rq *busiest = NULL;
	int i, range = 0, src_cpu = -1;

	dst_rq->misfit_task_load = 0;
	*done = true;

	short_idle = determine_short_idle(dst_rq->avg_idle);

	dst_rq->idle_stamp = rq_clock(dst_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(dst_cpu))
		return;

	rq_unpin_lock(dst_rq, rf);

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (dst_rq->ttwu_pending)
		goto out;

	if (dst_rq->nr_running)
		goto out;

	if (lb_idle_pull_tasks_rt(dst_rq))
		goto out;

	/* final check again after drop rq lock during RT balance */
	if (dst_rq->nr_running)
		goto out;

	if (!READ_ONCE(dst_rq->rd->overload))
		goto out;

	if (atomic_read(&dst_rq->nr_iowait) && short_idle)
		goto out;

	/* if system is busy state */
	if (sysbusy_on_somac())
		goto out;

	raw_spin_rq_unlock(dst_rq);

	range = compute_range(dst_cpu, cl_idx, short_idle);
	for (i = 0; i < range; i++) {
		struct pe_list *pl = get_pe_list(cl_idx);
		if (!pl)
			break;

		/* if this rq has a task, stop idle-pull */
		if (dst_rq->nr_running > 0)
			break;

		__lb_find_busiest_queue(dst_cpu, &(pl->cpus[i]), &busiest);
		if (busiest) {
			src_cpu = cpu_of(busiest);
			if (dst_cpu != src_cpu)
				*pulled_task = lb_idle_pull_tasks(dst_cpu, src_cpu);
			if (*pulled_task)
				break;
		}
	}

	raw_spin_rq_lock(dst_rq);
out:
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (dst_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (dst_rq->nr_running != dst_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		dst_rq->idle_stamp = 0;

	rq_repin_lock(dst_rq, rf);

	trace_lb_newidle_balance(dst_cpu, src_cpu, *pulled_task, range, short_idle);
}

/*
 * Active migration to push the misfit task
 */
void lb_tick(struct rq *src_rq)
{
	int src_cpu = src_rq->cpu, dst_cpu;
	struct task_struct *p = src_rq->curr;
	struct lb_env *env = per_cpu_ptr(lb_env, src_cpu);
	struct rq *dst_rq;
	unsigned long rq_flags;

	/* if system is busy state */
	if (sysbusy_on_somac())
		return;

	/* if src cpu is idle, we don't need to push the task */
	if (available_idle_cpu(src_cpu))
		return;

	/* if there is no misfit task in this cpu */
	if (!ems_rq_nr_misfited(src_rq))
		return;

	/* if current task is not fair */
	if (get_sched_class(p) != EMS_SCHED_FAIR)
		return;

	if (!ems_task_misfited(p))
		return;

	raw_spin_rq_lock_irqsave(src_rq, rq_flags);
	dst_cpu = __ems_select_task_rq_fair(p, src_cpu, 0, 0);

	/* stop migration, if there is no dst cpu or dst cpu is not idle */
	if (dst_cpu < 0 || !available_idle_cpu(dst_cpu))
		goto out_unlock;

	/* stop migration if same/lower capacity CPU */
	if (src_cpu == dst_cpu || capacity_orig_of(dst_cpu) <= capacity_orig_of(src_cpu))
		goto out_unlock;

	if (src_rq->active_balance)
		goto out_unlock;

	dst_rq = cpu_rq(dst_cpu);
	src_rq->active_balance = 1;
	ems_rq_migrated(dst_rq) = true;
	env->src_rq = src_rq;
	env->dst_rq = dst_rq;
	env->push_task = p;
	trace_lb_active_migration(p, src_cpu, dst_cpu, "tick-balanced");

	raw_spin_rq_unlock_irqrestore(src_rq, rq_flags);

	stop_one_cpu_nowait(src_cpu, lb_active_migration_stop,
			env, per_cpu_ptr(lb_work, src_cpu));

	wake_up_if_idle(dst_cpu);

	return;

out_unlock:
	raw_spin_rq_unlock_irqrestore(src_rq, rq_flags);
	trace_lb_active_migration(p, src_cpu, dst_cpu, "tick-no-balanced");
}

void lb_init(void)
{
	lb_env = alloc_percpu(struct lb_env);
	lb_work = alloc_percpu(struct cpu_stop_work);
}
