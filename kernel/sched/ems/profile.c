/*
 * Scheduling status profiler for Exynos Mobile Scheduler
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd
 * Park Choonghoon <choong.park@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static struct system_profile_data *system_profile_data;
static DEFINE_RWLOCK(profile_sched_lock);
#define get_cpu_profile(cpu, profile)	(&system_profile_data->cp[cpu][profile]);

char *fair_causes_name[END_OF_FAIR_CAUSES] = {
	"no-candidate",
	"only-one-candidate",
	"no-fit-cpu",
	"only-one-fit-cpu",
	"sysbusy",
	"task-express",
	"lowest-energy",
	"performance",
	"sync",
	"fast-track",
	"na"
};

char *rt_causes_name[END_OF_RT_CAUSES] = {
	"only-one-candidate",
	"idle",
	"recessive",
	"na"
};

struct sched_stat {
	unsigned int fair_sum;
	unsigned int rt_sum;
	unsigned int fair[END_OF_FAIR_CAUSES];
	unsigned int rt[END_OF_RT_CAUSES];
};
static DEFINE_PER_CPU(struct sched_stat, stats);

void update_fair_stat(int cpu, enum fair_causes i)
{
	struct sched_stat *stat = per_cpu_ptr(&stats, cpu);

	if (!stat)
		return;

	stat->fair_sum++;
	stat->fair[i]++;
}

void update_rt_stat(int cpu, enum rt_causes i)
{
	struct sched_stat *stat = per_cpu_ptr(&stats, cpu);

	if (!stat)
		return;

	stat->rt_sum++;
	stat->rt[i]++;
}

static ssize_t sched_stat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value, cpu;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	for_each_possible_cpu(cpu) {
		struct sched_stat *stat = per_cpu_ptr(&stats, cpu);

		memset(stat, 0, sizeof(struct sched_stat));
	}

	return count;
}

static ssize_t sched_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sched_stat *stat;
	int ret = 0;
	int cpu, i;
	char *line = "--------------------------------------------------"
		"--------------------------------------------------"
		"---------------------";

	ret += sprintf(buf + ret, "%s\n", line);
	ret += sprintf(buf + ret, "%20s | ", "FAIR schedule");
	for_each_possible_cpu(cpu)
		ret += sprintf(buf + ret, "      CPU%d ", cpu);
	ret += sprintf(buf + ret, "\n%s\n", line);

	for (i = 0; i < END_OF_FAIR_CAUSES; i++) {
		ret += sprintf(buf + ret, "%20s | ", fair_causes_name[i]);
		for_each_possible_cpu(cpu) {
			stat = per_cpu_ptr(&stats, cpu);
			ret += sprintf(buf + ret, "%10u ", stat->fair[i]);
		}
		ret += sprintf(buf + ret, "\n");
	}

	ret += sprintf(buf + ret, "%s\n", line);
	ret += sprintf(buf + ret, "%20s | ", "total");
	for_each_possible_cpu(cpu) {
		stat = per_cpu_ptr(&stats, cpu);
		ret += sprintf(buf + ret, "%10u ", stat->fair_sum);
	}
	ret += sprintf(buf + ret, "\n%s\n", line);
\
	ret += sprintf(buf + ret, "%s\n", line);
	ret += sprintf(buf + ret, "%20s | ", "RT schedule");
	for_each_possible_cpu(cpu)
		ret += sprintf(buf + ret, "%10u ", cpu);
	ret += sprintf(buf + ret, "\n%s\n", line);

	for (i = 0; i < END_OF_RT_CAUSES; i++) {
		ret += sprintf(buf + ret, "%20s | ", rt_causes_name[i]);
		for_each_possible_cpu(cpu) {
			stat = per_cpu_ptr(&stats, cpu);
			ret += sprintf(buf + ret, "%10u ", stat->rt[i]);
		}
		ret += sprintf(buf + ret, "\n");
	}

	ret += sprintf(buf + ret, "%s\n", line);
	ret += sprintf(buf + ret, "%20s | ", "total");
	for_each_possible_cpu(cpu) {
		stat = per_cpu_ptr(&stats, cpu);
		ret += sprintf(buf + ret, "%10u ", stat->rt_sum);
	}
	ret += sprintf(buf + ret, "\n%s\n", line);
	return ret;
}
DEVICE_ATTR_RW(sched_stat);

/****************************************************************
 *		HEAVY TASK base on Active Ratio		*
 ****************************************************************/
static int profile_heavy_task_ratio = 900;	/* 90% */

/* heavy task should be included in TOP-APP or FOREGOUND */
static inline bool htask_enabled_grp(struct task_struct *p)
{
	int grp = cpuctl_task_group_idx(p);

	if (grp != CGROUP_TOPAPP && grp != CGROUP_FOREGROUND)
		return false;

	return true;
}

/* get_htask_ratio - if this task is heavy task, return heaviness ratio */
static int get_htask_ratio(struct task_struct *p)
{
	int ratio, cur_ratio = 0, prev_ratio = 0, recent_ratio = 0;
	int prev_idx, cur_idx;

	if (!htask_enabled_grp(p))
		return 0;

	cur_idx = mlt_task_cur_period(p);
	prev_idx = mlt_period_with_delta(cur_idx, -1);
	recent_ratio = mlt_task_recent(p);
	cur_ratio = mlt_task_value(p, cur_idx);
	prev_ratio = mlt_task_value(p, prev_idx);

	/* 2 periods average actvie ratio should over 90% */
	if (prev_ratio < recent_ratio)
		ratio = recent_ratio + cur_ratio;
	else
		ratio = prev_ratio + cur_ratio;
	ratio = ratio >> 1;

	if (ratio < profile_heavy_task_ratio)
		return 0;

	return mlt_task_avg(p);
}

static void profile_update_cpu_htask(int cpu, unsigned long hratio, unsigned long pid)
{
	struct cpu_profile *cs = get_cpu_profile(cpu, CPU_HTSK);

	cs->value = hratio;
	cs->data = pid;
}

/*
 * profile_enqueue_task
 * check whether enqueued task is heavy or not, if it is true,
 * apply htask_ratio to this rq immediately to increase frequency latency
 */
void profile_enqueue_task(struct rq *rq, struct task_struct *p)
{
	struct cpu_profile *cp;
	int hratio;

	if (!htask_enabled_grp(p))
		return;

	cp = get_cpu_profile(cpu_of(rq), CPU_HTSK);
	hratio = get_htask_ratio(p);
	if (cp->value < hratio) {
		cp->value = hratio;
		cp->data = p->pid;
	}
}

int profile_get_htask_ratio(int cpu)
{
	struct cpu_profile *cp = get_cpu_profile(cpu, CPU_HTSK);
	return cp->value;
}

/****************************************************************
 *			CPU WEIGHTED ACTIVE RATIO		*
 ****************************************************************/
#define IDLE_HYSTERESIS_SHIFT		1
#define WEIGHTED_RATIO			900

#define B2I_THR_RATIO			250
#define I2B_THR_RATIO			(B2I_THR_RATIO >> IDLE_HYSTERESIS_SHIFT)

#define I2B_MONITOR_CNT			4
#define B2I_MONITOR_CNT			(I2B_MONITOR_CNT << IDLE_HYSTERESIS_SHIFT)

#define I2B_WSUM	4095
#define B2I_WSUM	5695

static int profile_update_cpu_wratio(int cpu)
{
	int cnt, monitor_cnt, wratio_thr, wratio_sum, ar_sum = 0;
	int cpu_ar_monitor_cnt = I2B_MONITOR_CNT + 1;
	int idx, wratio = 0;
	u64 now = sched_clock();
	struct cpu_profile *cs = get_cpu_profile(cpu, CPU_WRATIO);

	/* Update for tickless core */
	if ((mlt_art_last_update_time(cpu) + MLT_IDLE_THR_TIME)< now ) {
		cs->value = 0;
		cs->data = PROFILE_CPU_IDLE;
		return 0;
	}

	if (cs->data == PROFILE_CPU_BUSY) {
		wratio_sum = B2I_WSUM;
		monitor_cnt = B2I_MONITOR_CNT;
		wratio_thr = B2I_THR_RATIO;
	} else {
		wratio_sum = I2B_WSUM;
		monitor_cnt = I2B_MONITOR_CNT;
		wratio_thr = I2B_THR_RATIO;
	}

	/* computing weighted active ratio */
	idx = mlt_period_with_delta(mlt_cur_period(cpu), -(monitor_cnt - 1));
	for (cnt = 0; cnt < monitor_cnt; cnt++) {
		int ratio = mlt_art_value(cpu, idx);

		wratio = (ratio + ((wratio * WEIGHTED_RATIO) >> SCHED_CAPACITY_SHIFT));
		idx = mlt_period_with_delta(idx, 1);

		if (cnt < cpu_ar_monitor_cnt)
			ar_sum += ratio;
	}

	cs->value = (wratio << SCHED_CAPACITY_SHIFT) / wratio_sum;

	if (cs->data == PROFILE_CPU_BUSY) {
		if (cs->value < wratio_thr)
			cs->data = PROFILE_CPU_IDLE;
	} else {
		if (cs->value > wratio_thr)
			cs->data = PROFILE_CPU_BUSY;
	}

	return ar_sum ? (ar_sum / cpu_ar_monitor_cnt) : 0;
}

u64 profile_get_cpu_wratio_busy(int cpu)
{
	struct cpu_profile *cp = get_cpu_profile(cpu, CPU_WRATIO);
	return cp->data;
}

/****************************************************************
 *			CPU UTIL				*
 ****************************************************************/
static void profile_update_cpu_util(int cpu, int *busy_cnt, unsigned long *util_sum)
{
	int cpu_util;
	struct cpu_profile *cs = get_cpu_profile(cpu, CPU_UTIL);

	cpu_util = ml_cpu_util(cpu) + cpu_util_rt(cpu_rq(cpu));
	(*util_sum) += cpu_util;

	if (check_busy(cpu_util, capacity_cpu(cpu))) {
		cs->data = PROFILE_CPU_BUSY;
		(*busy_cnt) += 1;
	} else {
		cs->data = PROFILE_CPU_IDLE;
	}

	cs->value = cpu_util;
}

/****************************************************************
 *			External APIs				*
 ****************************************************************/
static u64 last_profile_time;
static int profile_interval = 1; /* 1 tick = 4ms */
int profile_sched_data(void)
{
	unsigned long flags;
	unsigned long now = jiffies;
	unsigned long cpu_util_sum = 0;
	unsigned long heavy_task_util_sum = 0;
	unsigned long misfit_task_util_sum = 0;
	unsigned long heaviest_task_util = 0;
	int busy_cpu_count = 0;
	int heavy_task_count = 0;
	int misfit_task_count = 0;
	int pd_nr_running = 0;
	int ed_ar_avg_sum = 0;
	int pd_ar_avg_sum = 0;
	int perf_cpu_nr = (VENDOR_NR_CPUS - cpumask_weight(cpu_slowest_mask()));
	int perf_cap_scale = (SCHED_CAPACITY_SCALE * perf_cpu_nr);
	int slowest_cap_scale = (SCHED_CAPACITY_SCALE * cpumask_weight(cpu_slowest_mask()));
	int cpu;

	if (!write_trylock_irqsave(&profile_sched_lock, flags))
		return -EBUSY;

	if (now < last_profile_time + profile_interval)
		goto unlock;

	last_profile_time = now;

	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct task_struct *p;
		unsigned long task_util;
		u64 max_hratio = 0, max_hratio_pid = 0;
		int track_count;
		int cpu_ar_avg = 0;

		/* update cpu util */
		profile_update_cpu_util(cpu, &busy_cpu_count, &cpu_util_sum);

		/* update weighted cpu active ratio */
		cpu_ar_avg = profile_update_cpu_wratio(cpu);

		raw_spin_rq_lock(rq);

		if (!rq->cfs.curr)
			goto rq_unlock;

		/* Explictly clear count */
		track_count = 0;

		list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
			/* update heavy task base on active ratio */
			u64 task_hratio = get_htask_ratio(p);
			if (task_hratio > max_hratio) {
				max_hratio_pid = p->pid;
				max_hratio = task_hratio;
			}

			/* update heavy task base on cpu util */
			task_util = ml_task_util(p);
			if (is_heavy_task_util(task_util)) {
				heavy_task_count++;
				heavy_task_util_sum += task_util;
			}

			if (is_misfit_task_util(task_util)) {
				misfit_task_count++;
				misfit_task_util_sum += task_util;
			}

			if (heaviest_task_util < task_util)
				heaviest_task_util = task_util;

			if (++track_count >= TRACK_TASK_COUNT)
				break;
		}

rq_unlock:
		if (!cpumask_test_cpu(cpu, cpu_slowest_mask())) {
			pd_ar_avg_sum += cpu_ar_avg;
			pd_nr_running += rq->cfs.h_nr_running;
		} else {
			ed_ar_avg_sum += cpu_ar_avg;
		}

		/* save heaviest task data */
		profile_update_cpu_htask(cpu, max_hratio, max_hratio_pid);

		trace_ems_cpu_profile(cpu, &system_profile_data->cp[cpu][0],
					cpu_ar_avg, rq->cfs.h_nr_running);

		raw_spin_rq_unlock(rq);
	}

	trace_ems_profile_tasks(busy_cpu_count, cpu_util_sum,
				heavy_task_count, heavy_task_util_sum,
				misfit_task_count, misfit_task_util_sum,
				pd_nr_running);

	/* Fill profile data */
	system_profile_data->busy_cpu_count = busy_cpu_count;
	system_profile_data->heavy_task_count = heavy_task_count;
	system_profile_data->misfit_task_count = misfit_task_count;
	system_profile_data->cpu_util_sum = cpu_util_sum;
	system_profile_data->heavy_task_util_sum = heavy_task_util_sum;
	system_profile_data->misfit_task_util_sum = misfit_task_util_sum;
	system_profile_data->heaviest_task_util = heaviest_task_util;
	system_profile_data->ed_ar_avg = (ed_ar_avg_sum * SCHED_CAPACITY_SCALE) / slowest_cap_scale;
	system_profile_data->pd_ar_avg = (pd_ar_avg_sum * SCHED_CAPACITY_SCALE) / perf_cap_scale;
	system_profile_data->pd_nr_running = pd_nr_running;

unlock:
	write_unlock_irqrestore(&profile_sched_lock, flags);

	return 0;
}

/* Caller MUST disable irq before calling this function. */
void get_system_sched_data(struct system_profile_data *data)
{
	read_lock(&profile_sched_lock);
	memcpy(data, system_profile_data, sizeof(struct system_profile_data));
	read_unlock(&profile_sched_lock);
}

/****************************************************************
 *		  sysbusy state change notifier			*
 ****************************************************************/
static int profile_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	profile_interval = sysbusy_params[state].monitor_interval;

	return NOTIFY_OK;
}

static struct notifier_block profile_sysbusy_notifier = {
	.notifier_call = profile_sysbusy_notifier_call,
};

/****************************************************************
 *			Initialization				*
 ****************************************************************/
int profile_sched_init(struct kobject *ems_kobj)
{
	system_profile_data =
		kzalloc(sizeof(struct system_profile_data), GFP_KERNEL);
	if (!system_profile_data) {
		pr_err("Failed to allocate profile_system_data\n");
		return -ENOMEM;
	}

	sysbusy_register_notifier(&profile_sysbusy_notifier);

	if (sysfs_create_file(ems_kobj, &dev_attr_sched_stat.attr))
		pr_warn("failed to create sched_stat\n");

	return 0;
}
