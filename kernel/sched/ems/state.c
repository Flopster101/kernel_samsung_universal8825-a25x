/*
 * STT(State-Tracker) from Scheduler, CPUFreq, Idle information
 * Copyright (C) 2021 Samsung Electronics Co., Ltd
 */

#include <linux/cpumask.h>
#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>
#include <soc/samsung/gpu_cooling.h>

enum cpu_state {
	IDLE,		/* nr_running 0 && cpu util < min_cap * 025 */
	BUSY,
	STATE_MAX,
};

struct stt_policy {
	struct cpumask	cpus;
	int		state;
};

#define MIGRATED_DEQ	1
#define	MIGRATED_ENQ	2
struct stt_task {
	struct task_struct	*cp;		/* current tacking task */
	int			pid;
	int			htask_cnt;	/* cnt means part window count */
	u64			expired;	/* heavy task valid time-ns */
	u64			next_update;	/* next update time to compute active ratio */
	int			htask_ratio;	/* current task active ratio */
	int			htask_enabled_grp;	/* support htask */
	int			flag;		/* state flag */
};

struct stt_cpu {
	enum cpu_state		state;
	int			busy_ratio;	/* busy state active ratio */
	u64			last_update;	/* last cpu update ktime-ns */
	int			last_idx;	/* last PART Index */

	/* heavy task state */
	int			htask_boost;	/* heavy task boost ratio */
	struct stt_task		ctask;		/* current task */
	struct stt_task		htask;		/* saved heavy task */

	struct stt_policy *stp;
};


/* HACK: It Should be moved to EMSTune */
struct stt_tunable {
	int enabled;
	int busy_ratio_thr;
	int busy_monitor_cnt;
	int weighted_ratio;
	int htask_cnt;
	int htask_boost_step;
	int htask_enabled_grp[CGROUP_COUNT];

	/* computed value from above tunable knob*/
	int idle_ratio_thr;
	int idle_monitor_cnt;
	u64 idle_monitor_time;
	u64 htask_monitor_time;
	int weighted_sum[STATE_MAX];

	struct kobject		kobj;
};

bool stt_initialized;
static DEFINE_PER_CPU(struct stt_cpu, stt_cpu);

static struct stt_tunable stt_tunable;

#define PERIOD_NS			(NSEC_PER_SEC / HZ)
#define PART_HISTORIC_TIME		(PERIOD_NS * 10)
#define RATIO_UNIT			1000
/********************************************************************************
 *				INTERNAL HELPER					*
 ********************************************************************************/
static inline int stt_get_weighted_part_ratio(int cpu, int cur_idx, int cnt, int max)
{
	int idx, wratio = 0;
	struct stt_tunable *stt = &stt_tunable;

	for (idx = 0; idx < cnt; idx++) {
		int ratio = mlt_art_value(cpu, cur_idx);
		wratio = (ratio + ((wratio * stt->weighted_ratio) / RATIO_UNIT));
		cur_idx = mlt_prev_period(cur_idx);
	}
	return (wratio * RATIO_UNIT) / max;
}

static inline int stt_get_avg_part_ratio(int cpu, int cur_idx, int cnt)
{
	int idx, ratio = 0;

	if (!cnt)
		return ratio;

	for (idx = 0; idx < cnt; idx++) {
		ratio += mlt_art_value(cpu, cur_idx);
		cur_idx = mlt_prev_period(cur_idx);
	}
	return (ratio / cnt);
}

static inline bool stt_should_update_load(struct stt_cpu *stc, int cpu,
				int cur_idx, u64 now)
{
	return (stc->last_idx != cur_idx ||
		(stc->last_update + PART_HISTORIC_TIME) < now);
}

#define IDLE_HYSTERESIS_SHIFT	1
#define HTASK_INVALID_CNT	2
static void stt_update_tunables(void)
{
	int max_cnt, idx, wratio = 0;
	struct stt_tunable *stt = &stt_tunable;

	stt->busy_ratio_thr = min(stt->busy_ratio_thr, RATIO_UNIT);
	stt->idle_ratio_thr = stt->busy_ratio_thr >> IDLE_HYSTERESIS_SHIFT;
	stt->idle_monitor_cnt = stt->busy_monitor_cnt << IDLE_HYSTERESIS_SHIFT;
	stt->idle_monitor_time = (PERIOD_NS * stt->idle_monitor_cnt *
					(RATIO_UNIT - stt->idle_ratio_thr)) / RATIO_UNIT;

	max_cnt = max(stt->busy_monitor_cnt, stt->idle_monitor_cnt);
	for (idx = 1; idx <= max_cnt; idx++) {
		wratio = (RATIO_UNIT + ((wratio * stt->weighted_ratio) / RATIO_UNIT));
		if (idx == stt->busy_monitor_cnt)
			stt->weighted_sum[BUSY] = wratio;
		if (idx == stt->idle_monitor_cnt)
			stt->weighted_sum[IDLE] = wratio;
	}
	stt->htask_monitor_time = (PERIOD_NS * HTASK_INVALID_CNT * 95 / 100);
}

static bool stt_enabled(void)
{
	return ((likely(stt_initialized)) && (stt_tunable.enabled));
}

static void stt_enable_htask_grp(int idx, bool on)
{
	stt_tunable.htask_enabled_grp[idx] = on;
}

static int stt_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct stt_tunable *stt = &stt_tunable;
	int group;

	stt->htask_cnt = cur_set->stt.htask_cnt;
	stt->htask_boost_step = cur_set->stt.boost_step;
	for (group = 0; group < CGROUP_COUNT; group++)
		stt_enable_htask_grp(group, cur_set->stt.htask_enable[group]);

	stt_update_tunables();

	return NOTIFY_OK;
}

static struct notifier_block stt_mode_update_notifier = {
	.notifier_call = stt_mode_update_callback,
};

/********************************************************************************
 *				HEAVY TASK BOOST				*
 ********************************************************************************/
#define HEAVY_TSK_MONITOR_CNT		2
#define HEAVY_TSK_RATIO			950
#define HEAVY_TASK_BOOST_STEP		20

/*
 * When saved heavy task is valid and more heavy than current task,
 * current heavy task is ignored
 * When saved heavy is valid like belows
 * 1) task is valid
 * 2) doesn't over the expired time
 */
static bool stt_prev_htask_is_valid(struct stt_task *htask, u64 now)
{
	return (htask->pid != -1 && htask->expired > now);
}

/*
 * current task is valid until expired time
 */
static bool stt_cur_htask_is_valid(struct stt_task *ctask, u64 now)
{
	return (ctask->expired > now);
}

static void stt_reset_tracking_task(struct stt_cpu *stc, struct task_struct *cp, u64 now)
{
	struct stt_task *cur = &stc->ctask;
	int group_idx = cpuctl_task_group_idx(cp);;

	cur->cp = cp;
	cur->pid = cp->pid;
	cur->next_update = now + stt_tunable.htask_monitor_time;
	cur->htask_enabled_grp = stt_tunable.htask_enabled_grp[group_idx];
	cur->htask_cnt = 0;
}

static bool gmc_flag;
static int gmc_boost_cnt;
static int gmc_boost_step;
static void __stt_compute_htask_boost(struct stt_cpu *stc, u64 now)
{
	int prev_htask_cnt = 0, cur_htask_cnt = 0, htask_cnt;
	struct stt_task *cur= &stc->ctask;
	struct stt_task *htask = &stc->htask;
	struct stt_tunable *stt = &stt_tunable;

	if (stt_prev_htask_is_valid(htask, now))
		prev_htask_cnt = htask->htask_cnt;
	if (stt_cur_htask_is_valid(cur, now))
		cur_htask_cnt = cur->htask_cnt;

	htask_cnt = max(prev_htask_cnt, cur_htask_cnt);

	if (htask_cnt) {
		/* Follow the smaller boost step value */
		if (gmc_flag && gmc_boost_step < stt->htask_boost_step)
			stc->htask_boost = (htask_cnt + gmc_boost_cnt) * gmc_boost_step;
		else
			stc->htask_boost = min(((htask_cnt + stt->htask_cnt) * stt->htask_boost_step), 100);
	}
	else
		stc->htask_boost = 0;
}

/*
 * if this cpu is in tickless state with boost,
 * need to update boost by other cpu to release boost
 */
static inline void stt_compute_htask_boost(struct stt_cpu *stc)
{
	struct stt_tunable *stt;
	u64 now;

	if (!stc->htask_boost)
		return;

	now = sched_clock();
	stt = &stt_tunable;
	if (now < (stc->last_update + stt->htask_monitor_time))
		return;

	__stt_compute_htask_boost(stc, now);
}

void stt_acquire_gmc_flag(int step, int cnt)
{
	gmc_flag = true;
	gmc_boost_cnt = cnt;
	gmc_boost_step = step;
}
EXPORT_SYMBOL_GPL(stt_acquire_gmc_flag);

void stt_release_gmc_flag(void)
{
	gmc_flag = false;
}
EXPORT_SYMBOL_GPL(stt_release_gmc_flag);

static inline void stt_migrated_htask_enq(int dst, int src, struct stt_cpu *stc, u64 now)
{
	struct stt_task *src_htask = &per_cpu(stt_cpu, src).htask;
	struct stt_task *dst_htask = &stc->htask;

	/*
	 * check it is real migrated task with expired time
	 * expired time of migrated htask is 0
	 */
	if (src_htask->flag != MIGRATED_DEQ) {
		trace_stt_tracking_htsk(dst, src_htask->pid,
			src_htask->htask_cnt, src_htask->expired, "MIGRATE_ENQ FAILED");
		return;
	}

	dst_htask->cp = src_htask->cp;
	dst_htask->pid = src_htask->pid;
	dst_htask->htask_cnt = src_htask->htask_cnt;
	dst_htask->expired = now + stt_tunable.htask_monitor_time;
	dst_htask->htask_enabled_grp = src_htask->htask_enabled_grp;
	dst_htask->flag = MIGRATED_ENQ;

	/* set flag invalid for src htask */
	src_htask->pid = -1;
	src_htask->flag = 0;

	__stt_compute_htask_boost(stc, now);

	trace_stt_tracking_htsk(dst, dst_htask->pid,
		dst_htask->htask_cnt, dst_htask->expired, "MIGRATE_ENQ");
}

static inline int stt_migrated_htsk_is_enqueueing(int dst, struct task_struct *p)
{
	int cpu;

	if (p->on_rq != TASK_ON_RQ_MIGRATING)
		return -1;

	for_each_online_cpu(cpu) {
		if (cpu == dst)
			continue;
		if (per_cpu(stt_cpu, cpu).htask.pid == p->pid)
			return cpu;
	}
	return -1;
}

/* save current heavy task */
static inline void
stt_backup_htask(int cpu, struct stt_cpu *stc, u64 expired)
{
	struct stt_task *htask = &stc->htask;
	struct stt_task *cur = &stc->ctask;

	htask->cp = cur->cp;
	htask->pid = cur->pid;
	htask->htask_cnt = cur->htask_cnt;
	htask->expired = expired;
	htask->htask_enabled_grp = cur->htask_enabled_grp;

	/* if expired is 0, it is migated task */
	htask->flag = expired ? 0 : MIGRATED_DEQ;

	trace_stt_tracking_htsk(cpu, htask->pid,
		htask->htask_cnt, htask->expired, "BACKUP");
}

static inline void
stt_migrated_htask_deq(int cpu, struct stt_cpu *stc, struct task_struct *cp, u64 now)
{
	stt_backup_htask(cpu, stc, 0);

	stt_reset_tracking_task(stc, cp, now);

	__stt_compute_htask_boost(stc, now);

	trace_stt_tracking_htsk(cpu, stc->htask.pid,
		stc->htask.htask_cnt, stc->htask.expired, "MIGRATE_DEQ");
}

static bool stt_migrated_htsk_is_dequeueing(struct stt_cpu *stc, struct task_struct *p)
{
	struct stt_task *cur = &stc->ctask;
	return (cur->htask_cnt && cur->pid == p->pid &&
			p->on_rq == TASK_ON_RQ_MIGRATING);
}


/* decide wether we should backup current task or not */
static inline bool
stt_need_backup_cur_task(struct stt_cpu *stc, u64 now)
{
	struct stt_task *cur = &stc->ctask;
	struct stt_task *htask = &stc->htask;

	/* if cur task is not heavy, we don't need to backup */
	if (!cur->htask_cnt)
		return false;

	/*
	 * current task is heavy but prev htask is more heavy than
	 * current task we don't need to back-up
	 */
	if (stt_prev_htask_is_valid(htask, now) &&
			htask->htask_cnt > cur->htask_cnt)
		return false;

	return true;
}

/* restore heavy task state */
static inline void stt_restore_htask(int cpu, struct stt_cpu *stc, struct task_struct *cp, u64 now)
{
	struct stt_task *cur = &stc->ctask;
	struct stt_task *htask = &stc->htask;
	int group_idx = cpuctl_task_group_idx(cp);

	cur->cp = cp;
	cur->pid = cp->pid;
	cur->htask_cnt = htask->htask_cnt;
	cur->htask_enabled_grp = stt_tunable.htask_enabled_grp[group_idx];
	cur->next_update = 0;

	/* if current task is migrated task, sinc-up boost value */
	if (htask->flag == MIGRATED_ENQ) {
		htask->flag = 0;
		cur->expired = cur->next_update = htask->expired;
	}
	htask->pid = -1;			/* set invalid flag */

	trace_stt_tracking_htsk(cpu, cur->pid,
		cur->htask_cnt, htask->expired, "RESTORE");
}

/*
 * If new task is backuped heavy task and it is valid
 * we need to restore boost value
 */
static inline bool stt_need_restore_htask(struct stt_cpu *stc, int pid, u64 now)
{
	struct stt_task *htask = &stc->htask;
	struct stt_task *cur = &stc->ctask;

	/*
	 * Even if there is a backuped heavy task, current task is more heavy than backuped heavy task
	 * we don't need to restore it
	 */
	if (!stt_prev_htask_is_valid(htask, now) ||
			htask->htask_cnt < cur->htask_cnt)
		return false;

	return true;
}

/* if prev htask is expired, set invalid state */
static inline void stt_update_backuped_htask(struct stt_cpu *stc, u64 now)
{
	struct stt_task *htask = &stc->htask;

	/* check wether htask is still valid or not */
	if (htask->pid != -1 && htask->expired < now) {
		htask->flag = 0;
		htask->pid = -1;
	}
}

/*
 * Calculate how long the current task has been running
 */
static inline void
stt_compute_htask_heaviness(int cpu, struct stt_task *cur,
			struct stt_tunable *stt, int cur_idx, u64 now)
{
	cur->htask_ratio = stt_get_avg_part_ratio(cpu, cur_idx, stt->htask_cnt);

	if (cur->htask_ratio > HEAVY_TSK_RATIO)
		cur->htask_cnt = min(cur->htask_cnt + 1, 10);
	else
		cur->htask_cnt = cur->htask_cnt * cur->htask_ratio / 1000;

	if (cur->htask_cnt)
		cur->expired = now + stt_tunable.htask_monitor_time;

	trace_stt_update_htsk_heaviness(cpu, cur->pid,
			cur_idx, cur->htask_ratio, cur->htask_cnt);
}

/*
 * Called with sched tick happend
 * It checks the task continuty with tick unit
 * if decide wether current task is heavy or not by continually working time
 * if find tracked heavy task is go to idle, decide wether backup it or not
 * if find prev htask come back, decide wether restore prev boost count or not
 */
static inline void
stt_update_task_continuty(int cpu, int cur_idx,
	struct stt_cpu *stc, struct task_struct *cp, u64 now)
{
	int pid = cp->pid;
	struct stt_tunable *stt = &stt_tunable;
	struct stt_task *cur = &stc->ctask;

	stt_update_backuped_htask(stc, now);

	if (cur->pid != pid) {
		/* save the current heavy task */
		if (stt_need_backup_cur_task(stc, now)) {
			stt_compute_htask_heaviness(cpu, cur, stt, cur_idx, now);
			stt_backup_htask(cpu, stc, now + stt->htask_monitor_time);
			stt_reset_tracking_task(stc, cp, now);
		} else if (stt_need_restore_htask(stc, pid, now)) {
		/* if current pid is heav_task, restore boost */
			stt_restore_htask(cpu, stc, cp, now);
		} else {
			stt_reset_tracking_task(stc, cp, now);
		}
	}

	if (!cur->htask_enabled_grp)
		return;

	/* if task was changed, we should wait at least htask_monitor_time */
	if (cur->next_update > now)
		return;

	stt_compute_htask_heaviness(cpu, cur, stt, cur_idx, now);

	return;
}

static inline void stt_update_heavy_task_state(struct stt_cpu *stc,
		int cpu, int cur_idx, struct task_struct *cp, u64 now)
{
	/* update heavy task state */
	stt_update_task_continuty(cpu, cur_idx, stc, cp, now);

	/* compute heavy task boost */
	__stt_compute_htask_boost(stc, now);
}

/********************************************************************************
 *				CPU BUSY STATE					*
 ********************************************************************************/
#define BUSY_THR_RATIO		250
#define BUSY_MONITOR_CNT	4
#define WEIGHTED_RATIO		900

static inline bool stt_cpu_busy(struct stt_cpu *stc, u64 idle_monitor_time, u64 now)
{
	/* When cpu is in IDLE with state BUSY, we should check up idle time */
	return stc->state && ((stc->last_update + idle_monitor_time) > now);
}

static void stt_update_cpu_busy_state(struct stt_cpu *stc, int cpu, int cur_idx)
{
	int ratio;
	struct stt_tunable *stt = &stt_tunable;
	if (stc->state) {
	/* BUSY to IDLE ? */
		ratio = stt_get_weighted_part_ratio(cpu, cur_idx,
				stt->idle_monitor_cnt, stt->weighted_sum[IDLE]);
		if (ratio < stt->idle_ratio_thr)
			stc->state = IDLE;
	} else {
	/* IDLE to BUSY ? */
		ratio  = stt_get_weighted_part_ratio(cpu, cur_idx,
				stt->busy_monitor_cnt, stt->weighted_sum[BUSY]);
		if (ratio > stt->busy_ratio_thr)
			stc->state = BUSY;
	}

	stc->busy_ratio = ratio;
}

/********************************************************************************
 *				EXTERN API's					 *
 ********************************************************************************/
static inline void stt_update_cpu_load(struct stt_cpu *stc, int cpu,
			int cur_idx, u64 now, struct task_struct *cp)
{
	struct stt_task *cur = &stc->ctask;

	/* update cpu busy state */
	stt_update_cpu_busy_state(stc, cpu, cur_idx);

	/* update cpu heavy task state */
	stt_update_heavy_task_state(stc, cpu, cur_idx, cp, now);

	/* update the time */
	stc->last_update = now;
	stc->last_idx = cur_idx;

	trace_stt_update_cpu(cpu, stc->busy_ratio, cur->htask_ratio,
		cur->pid, cur->htask_enabled_grp, cur->next_update,
		cur->htask_cnt, stc->last_update, stc->htask_boost, stc->state);
}

/*
 * Update cpu state.
 * Should be called when Enqueu Task, Dequeu Task,
 * After updating PART.
 */
static void
__stt_update(int cpu, struct stt_cpu *stc, struct task_struct *cp, u64 now)
{
	int cur_idx;

	/* stt is initialized not yet */
	if (!stt_enabled())
		return;

	cur_idx = mlt_cur_period(cpu);

	/* if part load upated, should update cpu state also */
	if (stt_should_update_load(stc, cpu, cur_idx, now))
		stt_update_cpu_load(stc, cpu, cur_idx, now, cp);
}

void stt_update(struct rq *rq, struct task_struct *p)
{
	int cpu = cpu_of(rq);
	u64 now = sched_clock();
	struct stt_cpu *stc = &per_cpu(stt_cpu, cpu);

	__stt_update(cpu, stc, rq->curr, now);
}

void stt_enqueue_task(struct rq *rq, struct task_struct *p)
{
	int cpu = cpu_of(rq);
	struct stt_cpu *stc = &per_cpu(stt_cpu, cpu);
	u64 now = sched_clock();
	int src;

	__stt_update(cpu, stc, rq->curr, now);

	src = stt_migrated_htsk_is_enqueueing(cpu, p);
	if (src < 0)
		return;

	/* this task is heavy */
	stt_migrated_htask_enq(cpu, src, stc, now);
}

void stt_dequeue_task(struct rq *rq, struct task_struct *p)
{
	int cpu = cpu_of(rq);
	struct stt_cpu *stc = &per_cpu(stt_cpu, cpu);
	u64 now = sched_clock();

	__stt_update(cpu, stc, rq->curr, now);

	/* restore for migrated task */
	if (stt_migrated_htsk_is_dequeueing(stc, p))
		stt_migrated_htask_deq(cpu, stc, rq->curr, now);
}

int stt_heavy_tsk_boost(int cpu)
{
	struct stt_cpu *stc = &per_cpu(stt_cpu, cpu);

	stt_compute_htask_boost(stc);

	return stc->htask_boost;
}

int stt_cluster_state(int cpu)
{
	struct stt_policy *stp = per_cpu(stt_cpu, cpu).stp;
	u64 idle_monitor_time, now;

	/* stt is initialized not yet */
	if (!stt_enabled())
		return BUSY;

	now = sched_clock();

	idle_monitor_time = stt_tunable.idle_monitor_time;

	for_each_cpu(cpu, &stp->cpus) {
		struct stt_cpu *stc = &per_cpu(stt_cpu, cpu);
		if (stt_cpu_busy(stc, idle_monitor_time, now))
			return BUSY;
	}

	return IDLE;;
}

/****************************************************************/
/*			  SYSFS					*/
/****************************************************************/
struct stt_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define stt_attr_rw(name)				\
static struct stt_attr name##_attr =			\
__ATTR(name, 0644, show_##name, store_##name)

#define stt_show(name)								\
static ssize_t show_##name(struct kobject *k, char *buf)			\
{										\
	struct stt_tunable *stt =						\
			container_of(k, struct stt_tunable, kobj);		\
										\
	return sprintf(buf, "%d\n", stt->name);					\
}										\

#define stt_store(name)								\
static ssize_t store_##name(struct kobject *k, const char *buf, size_t count)	\
{										\
	struct stt_tunable *stt =						\
			container_of(k, struct stt_tunable, kobj);		\
	int data;								\
										\
	if (!sscanf(buf, "%d", &data))						\
		return -EINVAL;							\
										\
	stt->name = data;							\
	stt_update_tunables();							\
	return count;								\
}

stt_show(enabled);
stt_store(enabled);
stt_attr_rw(enabled);
stt_show(busy_ratio_thr);
stt_store(busy_ratio_thr);
stt_attr_rw(busy_ratio_thr);
stt_show(busy_monitor_cnt);
stt_store(busy_monitor_cnt);
stt_attr_rw(busy_monitor_cnt);
stt_show(weighted_ratio);
stt_store(weighted_ratio);
stt_attr_rw(weighted_ratio);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct stt_attr *fvattr = container_of(at, struct stt_attr, attr);
	return fvattr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct stt_attr *fvattr = container_of(at, struct stt_attr, attr);
	return fvattr->store(kobj, buf, count);
}

static const struct sysfs_ops stt_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *stt_attrs[] = {
	&enabled_attr.attr,
	&busy_ratio_thr_attr.attr,
	&busy_monitor_cnt_attr.attr,
	&weighted_ratio_attr.attr,
	NULL
};

static struct kobj_type ktype_stt = {
	.sysfs_ops	= &stt_sysfs_ops,
	.default_attrs	= stt_attrs,
};


/****************************************************************/
/*		  Initialization				*/
/****************************************************************/
static int stt_parse_dt(struct device_node *dn, struct stt_policy *stp)
{
	const char *buf;

	if (of_property_read_string(dn, "cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -1;
	} else
		cpulist_parse(buf, &stp->cpus);

	stt_tunable.busy_ratio_thr = BUSY_THR_RATIO;
	stt_tunable.busy_monitor_cnt = BUSY_MONITOR_CNT;
	stt_tunable.htask_cnt = HEAVY_TSK_MONITOR_CNT;
	stt_tunable.htask_boost_step = HEAVY_TASK_BOOST_STEP;
	stt_tunable.weighted_ratio = WEIGHTED_RATIO;

	stt_enable_htask_grp(CGROUP_TOPAPP, true);
	stt_enable_htask_grp(CGROUP_FOREGROUND, true);

	stt_tunable.enabled = true;

	stt_update_tunables();

	return 0;
}

static struct stt_policy *stt_policy_alloc(void)
{
	struct stt_policy *stp;

	stp = kzalloc(sizeof(struct stt_policy), GFP_KERNEL);
	return stp;
}

int stt_init(struct kobject *ems_kobj)
{

	struct device_node *dn, *child;
	struct stt_tunable *stt = &stt_tunable;
	int cpu;

	dn = of_find_node_by_path("/ems/stt");
	if (!dn)
		return 0;

	emstune_register_notifier(&stt_mode_update_notifier);

	for_each_child_of_node(dn, child) {
		struct stt_policy *stp = stt_policy_alloc();
		if (!stp) {
			pr_err("%s: failed to alloc stt_policy\n", __func__);
			goto fail;
		}

		/* Parse device tree */
		if (stt_parse_dt(child, stp))
			goto fail;

		for_each_cpu(cpu, &stp->cpus)
			per_cpu(stt_cpu, cpu).stp = stp;
	}

	/* Init Sysfs */
	if (kobject_init_and_add(&stt->kobj, &ktype_stt, ems_kobj, "stt"))
		goto fail;

	stt_initialized = true;

	return 0;
fail:
	for_each_possible_cpu(cpu) {
		if (per_cpu(stt_cpu, cpu).stp)
			kfree(per_cpu(stt_cpu, cpu).stp);
		per_cpu(stt_cpu, cpu).stp = NULL;
	}

	return 0;
}

