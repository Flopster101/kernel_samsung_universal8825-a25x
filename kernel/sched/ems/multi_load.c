/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include "../sched.h"
#include "../sched-pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

int ml_task_hungry(struct task_struct *p)
{
	return 0; /* HACK */
}

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return max(ml_task_util(p), _ml_task_util_est(p));
}

/*
 * ml_task_load_avg - task load_avg
 */
unsigned long ml_task_load_avg(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.load_avg);
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * ml_cpu_util - cpu utilization
 */
unsigned long ml_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

/*
 * ml_cpu_util_est - return cpu util_est
 */
unsigned long ml_cpu_util_est(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util_est;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

	return min_t(unsigned long, util_est, capacity_cpu_orig(cpu));
}

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/*
 * ml_cpu_util_without - cpu utilization without waking task
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	unsigned long util, util_est;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return ml_cpu_util(cpu);

	cfs_rq = &cpu_rq(cpu)->cfs;

	/* Calculate util avg */
	util = READ_ONCE(cfs_rq->avg.util_avg);
	lsub_positive(&util, ml_task_util(p));

	/* Calcuate util est */
	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);
	if (task_on_rq_queued(p) || current == p)
		lsub_positive(&util_est, _ml_task_util_est(p));

	util = max(util, util_est);

	return util;
}

/*
 * ml_cpu_util_with - cpu utilization with waking task
 *
 * When task is queued,
 * 1) task_cpu(p) == cpu
 *    => The contribution is already present on target cpu
 * 2) task_cpu(p) != cpu
 *    => The contribution is not present on target cpu
 *
 * When task is not queued,
 * 3) task_cpu(p) == cpu
 *    => The contribution is already applied to util_avg,
 *       but is not applied to util_est
 * 4) task_cpu(p) != cpu
 *    => The contribution is not present on target cpu
 */
unsigned long ml_cpu_util_with(struct task_struct *p, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util, util_est;

	/* Calculate util avg */
	util = READ_ONCE(cfs_rq->avg.util_avg);
	if (task_cpu(p) != cpu)
		util += ml_task_util(p);

	/* Calcuate util est */
	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);
	if (!task_on_rq_queued(p) || task_cpu(p) != cpu)
		util_est += _ml_task_util_est(p);

	util = max(util, util_est);
	util = max(util, (unsigned long)1);

	return util;
}

/*
 * ml_cpu_load_avg - cpu load_avg
 */
unsigned long ml_cpu_load_avg(int cpu)
{
	return cpu_rq(cpu)->cfs.avg.load_avg;
}

/*
 * ml_cpu_util_est_with - cpu util_est with waking task
 */
unsigned long ml_cpu_util_est_with(struct task_struct *p, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util_est;

	/* Calculate util est */
	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);
	if (!task_on_rq_queued(p) || task_cpu(p) != cpu)
		util_est += max(_ml_task_util_est(p), (unsigned long)1);

	return util_est;
}

/*
 * ml_cpu_util_est_without - cpu util_est without waking task
 */
unsigned long ml_cpu_util_est_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	unsigned long util_est;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return ml_cpu_util_est(cpu);

	cfs_rq = &cpu_rq(cpu)->cfs;

	/* Calculate util est */
	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);
	if (task_on_rq_queued(p) || current == p)
		lsub_positive(&util_est, _ml_task_util_est(p));

	return util_est;
}

/******************************************************************************
 *                     New task utilization init                              *
 ******************************************************************************/
static int ntu_ratio[CGROUP_COUNT];

void ntu_apply(struct sched_entity *se)
{
	struct sched_avg *sa = &se->avg;
	int grp_idx = cpuctl_task_group_idx(task_of(se));
	int ratio = ntu_ratio[grp_idx];

	sa->util_avg = sa->util_avg * ratio / 100;
	sa->runnable_avg = sa->runnable_avg * ratio / 100;
}

static int
ntu_emstune_notifier_call(struct notifier_block *nb,
					   unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		ntu_ratio[i] = cur_set->ntu.ratio[i];

	return NOTIFY_OK;
}

static struct notifier_block ntu_emstune_notifier = {
	.notifier_call = ntu_emstune_notifier_call,
};

void ntu_init(struct kobject *ems_kobj)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		ntu_ratio[i] = 100;

	emstune_register_notifier(&ntu_emstune_notifier);
}

/******************************************************************************
 *                             Multi load tracking                            *
 ******************************************************************************/
struct mlt __percpu *pcpu_mlt;		/* active ratio tracking */
static struct mlt __percpu *pcpu_mlt_cs;	/* c-state tracking */

static inline int mlt_move_period(int period, int count)
{
	return (period + count + MLT_PERIOD_COUNT) % MLT_PERIOD_COUNT;
}

enum track_type {
	TRACK_TIME,
	TRACK_IPC,
	TRACK_MSPC,
};

static void mlt_update_periods(enum track_type type, struct mlt *mlt,
				int cur_period, int count, int value)
{
	int **periods;
	int i;

	switch (type) {
	case TRACK_TIME:
		periods = mlt->periods;
		break;
	case TRACK_IPC:
		periods = mlt->uarch->ipc_periods;
		break;
	case TRACK_MSPC:
		periods = mlt->uarch->mspc_periods;
		break;
	}

	while (count--) {
		cur_period = mlt_move_period(cur_period, 1);
		for (i = 0; i < mlt->state_count; i++) {
			if (i == mlt->state)
				periods[i][cur_period] = value;
			else
				periods[i][cur_period] = 0;
		}
	}
}

static void mlt_recent_elapsed(enum track_type type,
				struct mlt *mlt, int cur_period)
{
	int i;
	int **periods;
	int *recent;
	u64 *recent_sum;

	switch (type) {
	case TRACK_TIME:
		periods = mlt->periods;
		recent = mlt->recent;
		recent_sum = mlt->recent_sum;
		break;
	case TRACK_IPC:
		periods = mlt->uarch->ipc_periods;
		recent = mlt->uarch->ipc_recent;
		recent_sum = mlt->uarch->inst_ret_sum;
		break;
	case TRACK_MSPC:
		periods = mlt->uarch->mspc_periods;
		recent = mlt->uarch->mspc_recent;
		recent_sum = mlt->uarch->mem_stall_sum;
		break;
	}

	for (i = 0; i < mlt->state_count; i++)
		periods[i][cur_period] = recent[i];

	memset(recent_sum, 0, sizeof(u64) * mlt->state_count);
	memset(recent, 0, sizeof(int) * mlt->state_count);
}

static inline u64 mlt_contrib(u64 value, u64 contrib, u64 total)
{
	if (unlikely(!total))
		return 0;

	/* multiply by 1024 to minimize dropping decimal place */
	return div64_u64(value * contrib << SCHED_CAPACITY_SHIFT, total);
}

static void mlt_update_recent(enum track_type type, struct mlt *mlt,
				u64 contrib, u64 period_size)
{
	int state = mlt->state;
	int *recent;
	u64 *recent_sum;

	if (state < 0)
		return;

	switch (type) {
	case TRACK_TIME:
		recent = mlt->recent;
		recent_sum = mlt->recent_sum;
		break;
	case TRACK_IPC:
		recent = mlt->uarch->ipc_recent;
		recent_sum = mlt->uarch->inst_ret_sum;
		break;
	case TRACK_MSPC:
		recent = mlt->uarch->mspc_recent;
		recent_sum = mlt->uarch->mem_stall_sum;
		break;
	}

	recent_sum[state] += contrib;
	recent[state] = div64_u64(
		recent_sum[state] << SCHED_CAPACITY_SHIFT, period_size);
}

static void __mlt_update(enum track_type type, struct mlt *mlt,
				int period, int period_count, u64 contrib,
				u64 remain, u64 period_size, u64 period_value)
{
	/*
	 * [before]
	 *           last_updated                                 now
	 *                 ↓                                       ↓
	 *                 |<---1,2--->|<-------3------->|<---4--->|
	 * timeline --|----------------|-----------------|-----------------|-->
	 *               recent period
	 *
	 * Update recent period first(1) and quit sequence if the recent
	 * period has not elapsed, otherwise store recent period in the period
	 * history(2). If more than 1 period has elapsed, fill in the elapsed
	 * period with an appropriate value (running = 1024, idle = 0) and
	 * store in the period history(3). And store remain time in new recent
	 * period(4).
	 *
	 * [after]
	 *                                                   last_updated
	 *                                                         ↓
	 *                                                         |
	 * timeline --|----------------|-----------------|-----------------|-->
	 *                                 last period      recent period
	 */

	/* (1) update recent period */
	mlt_update_recent(type, mlt, contrib, period_size);

	if (period_count) {
		int count = period_count;

		/* (2) store recent period to period history */
		period = mlt_move_period(period, 1);
		mlt_recent_elapsed(type, mlt, period);
		count--;

		/* (3) update fully elapsed period */
		count = min(count, MLT_PERIOD_COUNT);
		mlt_update_periods(type, mlt, period, count, period_value);

		/* (4) store remain time to recent period */
		mlt_update_recent(type, mlt, remain, period_size);
	}
}

static void mlt_update_uarch(struct mlt *mlt, int period,
				int period_count, u64 contrib, u64 remain)
{
	struct uarch_data *ud = mlt->uarch;
	u64 core_cycle, inst_ret, mem_stall;
	u64 cc_delta, ir_delta, ms_delta, ipc = 0, mspc = 0;
	u64 total_time;
	int state = mlt->state;

	if (state < 0)
		return;

	core_cycle = amu_core_cycles();
	inst_ret = amu_inst_ret();
	mem_stall = amu_mem_stall();

	cc_delta = core_cycle - ud->last_cycle;
	ir_delta = inst_ret - ud->last_inst_ret;
	ms_delta = mem_stall - ud->last_mem_stall;

	if (likely(cc_delta)) {
		ipc = div64_u64(ir_delta << SCHED_CAPACITY_SHIFT, cc_delta);
		mspc = div64_u64(ms_delta << SCHED_CAPACITY_SHIFT, cc_delta);
	}

	total_time = contrib + period_count * MLT_PERIOD_SIZE + remain;

	ud->cycle_sum[state] += mlt_contrib(cc_delta,
						contrib, total_time);

	__mlt_update(TRACK_IPC, mlt, period, period_count,
			mlt_contrib(ir_delta, contrib, total_time),
			mlt_contrib(ir_delta, remain, total_time),
			ud->cycle_sum[state], ipc);

	__mlt_update(TRACK_MSPC, mlt, period, period_count,
			mlt_contrib(ms_delta, contrib, total_time),
			mlt_contrib(ms_delta, remain, total_time),
			ud->cycle_sum[state], mspc);

	if (period_count) {
		memset(ud->cycle_sum, 0, sizeof(u64) * mlt->state_count);
		ud->cycle_sum[state] = mlt_contrib(cc_delta,
							remain, total_time);
	}

	ud->last_cycle = core_cycle;
	ud->last_inst_ret = inst_ret;
	ud->last_mem_stall = mem_stall;
}

static void mlt_update(struct mlt *mlt, u64 now)
{
	u64 contrib = 0, remain = 0;
	int period_count = 0;

	if (likely(now > mlt->last_updated)) {
		contrib = min(now, mlt->period_start + MLT_PERIOD_SIZE);
		contrib -= mlt->last_updated;
	}

	if (likely(now > mlt->period_start)) {
		period_count = div64_u64_rem(now - mlt->period_start,
				MLT_PERIOD_SIZE, &remain);
	}

	__mlt_update(TRACK_TIME, mlt, mlt->cur_period, period_count,
			contrib, remain, MLT_PERIOD_SIZE,
			SCHED_CAPACITY_SCALE);

	if (mlt->uarch)
		mlt_update_uarch(mlt, mlt->cur_period,
				period_count, contrib, remain);

	mlt->cur_period = mlt_move_period(mlt->cur_period, period_count);
	mlt->period_start += MLT_PERIOD_SIZE * period_count;
	mlt->last_updated = now;
}

#define INACTIVE	(-1)
static inline void mlt_set_state(struct mlt *mlt, int state)
{
	if (state == MLT_STATE_NOCHANGE)
		return;

	mlt->state = state;
}

static void mlt_update_cstate(int cpu, int next_cstate, u64 now)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt_cs, cpu);

	mlt_update(mlt, now);
	trace_mlt_update_cstate(mlt);

	mlt_set_state(mlt, next_cstate);
}

static void mlt_update_cpu(int cpu, int next_cgroup, u64 now)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	mlt_update(mlt, now);

	trace_mlt_update_cpu(mlt);
	if (mlt->uarch) {
		trace_mlt_update_cpu_ipc(mlt);
		trace_mlt_update_cpu_mspc(mlt);
	}

	mlt_set_state(mlt, next_cgroup);
}

#define task_mlt(p)	(struct mlt *)(&TASK_AVD1_6(p))

void mlt_update_task(struct task_struct *p, int next_state, u64 now)
{
	struct mlt *mlt = task_mlt(p);
	int state = next_state ? 0 : INACTIVE;

	if (unlikely(!mlt->state_count))
		return;

	mlt_update(mlt, now);
	trace_mlt_update_task(p, mlt);

	mlt_set_state(mlt, state);
}

void mlt_idle_enter(int cpu, int cstate)
{
	mlt_update_cstate(cpu, cstate, sched_clock());
}

void mlt_idle_exit(int cpu)
{
	mlt_update_cstate(cpu, INACTIVE, sched_clock());
}

static void mlt_update_nr_run(struct rq *rq);
void mlt_tick(struct rq *rq)
{
	u64 now = sched_clock();

	mlt_update_nr_run(rq);

	mlt_update_cpu(cpu_of(rq), MLT_STATE_NOCHANGE, now);

	if (get_sched_class(rq->curr) != EMS_SCHED_IDLE)
		mlt_update_task(rq->curr, MLT_STATE_NOCHANGE, now);
}

void mlt_task_switch(int cpu, struct task_struct *prev,
				struct task_struct *next)
{
	u64 now = sched_clock();
	int cgroup;

	if (get_sched_class(next) == EMS_SCHED_IDLE)
		cgroup = INACTIVE;
	else
		cgroup = cpuctl_task_group_idx(next);

	mlt_update_cpu(cpu, cgroup, now);

	mlt_update_task(prev, 0, now);
	mlt_update_task(next, 1, now);
}

void mlt_task_migration(struct task_struct *p, int new_cpu)
{
	struct mlt *mlt = task_mlt(p);

	if (unlikely(!mlt->state_count))
		return;

	mlt->cpu = new_cpu;
}

int mlt_cur_period(int cpu)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	return mlt->cur_period;
}

int mlt_prev_period(int period)
{
	period = period - 1;
	if (period < 0)
		return MLT_PERIOD_COUNT - 1;

	return period;
}

int mlt_period_with_delta(int idx, int delta)
{
	if (delta > 0)
		return (idx + delta) % MLT_PERIOD_COUNT;

	idx = idx + delta;
	if (idx < 0)
		return MLT_PERIOD_COUNT + idx;

	return idx;
}

/* return task's ratio of current period */
int mlt_task_cur_period(struct task_struct *p)
{
	struct mlt *mlt = task_mlt(p);
	return mlt->cur_period;
}

int __mlt_get_recent(struct mlt *mlt)
{
	int i, value = 0;

	for (i = 0; i < mlt->state_count; i++)
		value += mlt->recent[i];
	return value;
}

int __mlt_get_value(struct mlt *mlt, int target_period)
{
	int i, value = 0;

	for (i = 0; i < mlt->state_count; i++)
		value += mlt->periods[i][target_period];

	return value;
}

/* return task's ratio of target_period */
int mlt_task_value(struct task_struct *p, int target_period)
{
	struct mlt *mlt = task_mlt(p);
	return __mlt_get_value(mlt, target_period);
}

int mlt_task_recent(struct task_struct *p)
{
	struct mlt *mlt = task_mlt(p);
	return __mlt_get_recent(mlt);
}

/* return task's average ratio of all periods */
int mlt_task_avg(struct task_struct *p)
{
	int i, value = 0;

	for (i = 0; i < MLT_PERIOD_COUNT; i++)
		value += mlt_task_value(p, i);

	return (value * SCHED_CAPACITY_SCALE) / MLT_PERIOD_SUM;
}

int mlt_art_value(int cpu, int target_period)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
	return __mlt_get_value(mlt, target_period);
}

int mlt_art_recent(int cpu)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
	return __mlt_get_recent(mlt);
}

int mlt_art_last_value(int cpu)
{
	return mlt_art_value(cpu, per_cpu_ptr(pcpu_mlt, cpu)->cur_period);
}

u64 mlt_art_last_update_time(int cpu)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
	return mlt->last_updated;
}

int mlt_art_cgroup_value(int cpu, int target_period, int cgroup)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	return mlt->periods[cgroup][target_period];
}

int mlt_cst_value(int cpu, int target_period, int cstate)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt_cs, cpu);

	return mlt->periods[cstate][target_period];
}

static char *cg_name[] = {
	"root",
	"fg",
	"bg",
	"ta",
	"rt",
	"sy",
	"syb",
	"n-h",
	"c-d",
};

/******************************************************************************
 *                           MULTI LOAD for NR_RUNNING			      *
 ******************************************************************************/
#define NR_RUN_PERIOD		(4 * NSEC_PER_MSEC)

static struct mlt_nr_run __percpu *mlt_nr_run;		/* nr_run tracking */

int mlt_avg_nr_run(struct rq *rq)
{
	int cpu = cpu_of(rq);
	struct mlt_nr_run *mnr = per_cpu_ptr(mlt_nr_run, cpu);
	u64 now = sched_clock();

	if ((mnr->last_ctrb_updated + MLT_IDLE_THR_TIME) < now)
		return 0;

	return mnr->avg_nr_run;
}

static void mlt_update_nr_run_ctrb(struct rq *rq)
{
	int cpu = cpu_of(rq);
	struct mlt_nr_run *mnr = per_cpu_ptr(mlt_nr_run, cpu);
	int nr_run = rq->nr_running * NR_RUN_UNIT;
	u64 now = sched_clock();
	s64 delta;

	if (unlikely(now < mnr->last_ctrb_updated))
		return;

	delta = now - mnr->last_ctrb_updated;

	mnr->nr_run_ctrb += (nr_run * delta);
	mnr->accomulate_time += delta;
	mnr->last_ctrb_updated = now;
}

static void mlt_update_nr_run(struct rq *rq)
{
	int cpu = cpu_of(rq);
	struct mlt_nr_run *mnr = per_cpu_ptr(mlt_nr_run, cpu);

	mlt_update_nr_run_ctrb(rq);

	mnr->avg_nr_run = mnr->nr_run_ctrb / mnr->accomulate_time;

	trace_mlt_nr_run_update(cpu, mnr->accomulate_time, mnr->nr_run_ctrb,
				rq->nr_running, mnr->avg_nr_run);

	mnr->nr_run_ctrb = 0;
	mnr->accomulate_time = 0;
}

/* It will be called before increase nr_run */
void mlt_enqueue_task(struct rq *rq)
{
	mlt_update_nr_run_ctrb(rq);
}

/* It will be called before decrease nr_run */
void mlt_dequeue_task(struct rq *rq)
{
	mlt_update_nr_run_ctrb(rq);
}

int init_mlt_nr_run(void)
{
	int cpu;

	mlt_nr_run = alloc_percpu(struct mlt_nr_run);
	if (!mlt_nr_run) {
		pr_err("failed to allocate mlt_nr_run\n");
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		struct mlt_nr_run *mnr = per_cpu_ptr(mlt_nr_run, cpu);
		raw_spin_lock_init(&mnr->lock);
	}

	return 0;
}

#define MSG_SIZE 9216
static ssize_t multi_load_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	char *msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	ssize_t msg_size, count = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
		int group;

		count += sprintf(msg + count, "   CPU%d |   Recent |   Latest                     "
				"                             Oldest\n", cpu);
		count += sprintf(msg + count, "---------------------------------------------------"
				"-----------------------------------\n");

		for (group = 0; group < mlt->state_count; group++) {
			int p, c;

			if (group == mlt->state)
				count += sprintf(msg + count, ">> ");
			else
				count += sprintf(msg + count, "   ");
			count += sprintf(msg + count, "%4s | ",  cg_name[group]);

			count += sprintf(msg + count, "%8d | ", mlt->recent[group]);

			for (c = 0; c < MLT_PERIOD_COUNT; c++) {
				p = mlt_move_period(mlt->cur_period, -c);
				count += sprintf(msg + count, "%8d", mlt->periods[group][p]);
			}
			count += sprintf(msg + count, "\n");
		}
		count += sprintf(msg + count, "\n");
	}

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	kfree(msg);

	return msg_size;
}
BIN_ATTR_RO(multi_load, 0);

static int mlt_uarch_data_free(struct uarch_data *ud, int state_count)
{
	int i;

	if (ud->ipc_periods)
		for (i = 0; i < state_count; i++)
			kfree(ud->ipc_periods[i]);
	if (ud->mspc_periods)
		for (i = 0; i < state_count; i++)
			kfree(ud->mspc_periods[i]);
	kfree(ud->ipc_periods);
	kfree(ud->mspc_periods);
	kfree(ud->ipc_recent);
	kfree(ud->mspc_recent);
	kfree(ud->cycle_sum);
	kfree(ud->inst_ret_sum);
	kfree(ud->mem_stall_sum);
	kfree(ud);

	return -ENOMEM;
}

static int mlt_uarch_data_init(struct mlt *mlt)
{
	struct uarch_data *ud;
	int i, state_count = mlt->state_count;

	ud = kzalloc(sizeof(struct uarch_data), GFP_KERNEL);
	if (!ud)
		return -ENOMEM;

	ud->ipc_periods = kcalloc(state_count, sizeof(int *), GFP_KERNEL);
	if (!ud->ipc_periods)
		return mlt_uarch_data_free(ud, state_count);

	ud->mspc_periods = kcalloc(state_count, sizeof(int *), GFP_KERNEL);
	if (!ud->mspc_periods)
		return mlt_uarch_data_free(ud, state_count);

	for (i = 0; i < state_count; i++) {
		ud->ipc_periods[i] = kcalloc(MLT_PERIOD_COUNT, sizeof(int), GFP_KERNEL);
		if (!ud->ipc_periods[i])
			return mlt_uarch_data_free(ud, state_count);

		ud->mspc_periods[i] = kcalloc(MLT_PERIOD_COUNT, sizeof(int), GFP_KERNEL);
		if (!ud->mspc_periods[i])
			return mlt_uarch_data_free(ud, state_count);
	}

	ud->ipc_recent = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!ud->ipc_recent)
		return mlt_uarch_data_free(ud, state_count);

	ud->mspc_recent = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!ud->mspc_recent)
		return mlt_uarch_data_free(ud, state_count);

	ud->cycle_sum = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!ud->cycle_sum)
		return mlt_uarch_data_free(ud, state_count);

	ud->inst_ret_sum = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!ud->inst_ret_sum)
		return mlt_uarch_data_free(ud, state_count);

	ud->mem_stall_sum = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!ud->mem_stall_sum)
		return mlt_uarch_data_free(ud, state_count);

	mlt->uarch = ud;

	return 0;
}

static int mlt_alloc_and_init(struct mlt *mlt, int cpu,
				int state_count, u64 now, bool uarch)
{
	int i;

	mlt->cpu = cpu;
	mlt->cur_period = 0;
	mlt->period_start = now;
	mlt->last_updated = now;
	mlt->state = -1;
	mlt->state_count = state_count;

	mlt->periods = kcalloc(state_count, sizeof(int *), GFP_KERNEL);
	if (!mlt->periods)
		return -ENOMEM;

	for (i = 0; i < state_count; i++) {
		mlt->periods[i] = kcalloc(MLT_PERIOD_COUNT,
					sizeof(int), GFP_KERNEL);
		if (!mlt->periods[i])
			goto fail_alloc;
	}

	mlt->recent = kcalloc(state_count, sizeof(int), GFP_KERNEL);
	if (!mlt->recent)
		goto fail_alloc;

	mlt->recent_sum = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!mlt->recent_sum)
		goto fail_alloc;

	if (uarch) {
		if (mlt_uarch_data_init(mlt))
			goto fail_alloc;
	} else
		mlt->uarch = NULL;

	return 0;

fail_alloc:
	for (i = 0; i < state_count; i++)
		kfree(mlt->periods[i]);
	kfree(mlt->periods);
	kfree(mlt->recent);
	kfree(mlt->recent_sum);

	return -ENOMEM;
}

static bool uarch_supported;
void mlt_task_init(struct task_struct *p)
{
	struct mlt *mlt = task_mlt(p);
	struct mlt *mlt_cpu;
	int *pos = (int *)mlt;

	memset(mlt, 0, MLT_TASK_SIZE);
	mlt_cpu = per_cpu_ptr(pcpu_mlt, p->cpu);

	mlt->cpu = mlt_cpu->cpu;
	mlt->cur_period = mlt_cpu->cur_period;
	mlt->period_start = mlt_cpu->period_start;
	mlt->last_updated = mlt_cpu->last_updated;
	mlt->state = -1;
	mlt->state_count = 1;

	pos += (sizeof(struct mlt) / sizeof(int));

	mlt->periods			= (int **)pos;
	mlt->periods[0]			= pos + 2;
	mlt->recent			= pos + 2 + MLT_PERIOD_COUNT;
	mlt->recent_sum			= (u64 *)(pos + 4 + MLT_PERIOD_COUNT);

	if (!uarch_supported)
		return;

	mlt->uarch			= (struct uarch_data *)(pos + 6 + MLT_PERIOD_COUNT);

	pos = pos + 6 + MLT_PERIOD_COUNT;
	pos += (sizeof(struct uarch_data) / sizeof(int));

	mlt->uarch->ipc_periods		= (int **)pos;
	mlt->uarch->ipc_periods[0]	= pos + 2;
	mlt->uarch->mspc_periods	= (int **)(pos + 2 + MLT_PERIOD_COUNT);
	mlt->uarch->mspc_periods[0]	= pos + 4 + MLT_PERIOD_COUNT;
	mlt->uarch->ipc_recent		= pos + 4 + (MLT_PERIOD_COUNT * 2);
	mlt->uarch->mspc_recent		= pos + 6 + (MLT_PERIOD_COUNT * 2);
	mlt->uarch->cycle_sum		= (u64 *)(pos + 8 + (MLT_PERIOD_COUNT * 2));
	mlt->uarch->inst_ret_sum	= (u64 *)(pos + 10 + (MLT_PERIOD_COUNT * 2));
	mlt->uarch->mem_stall_sum	= (u64 *)(pos + 12 + (MLT_PERIOD_COUNT * 2));
}

int mlt_init(struct kobject *ems_kobj, struct device_node *dn)
{
	int cpu;
	u64 now;
	struct task_struct *p;

	pcpu_mlt = alloc_percpu(struct mlt);
	if (!pcpu_mlt) {
		pr_err("failed to allocate mlt\n");
		return -ENOMEM;
	}

	pcpu_mlt_cs = alloc_percpu(struct mlt);
	if (!pcpu_mlt_cs) {
		pr_err("failed to allocate mlt for c-state\n");
		return -ENOMEM;
	}

	if (of_property_read_bool(dn, "uarch-mlt"))
		uarch_supported = true;

	now = sched_clock();
	for_each_possible_cpu(cpu) {
		struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
		struct mlt *mlt_cs = per_cpu_ptr(pcpu_mlt_cs, cpu);

		mlt_alloc_and_init(mlt, cpu, CGROUP_COUNT, now, uarch_supported);
		mlt_alloc_and_init(mlt_cs, cpu, CSTATE_MAX, now, false);
	}

	rcu_read_lock();
	list_for_each_entry_rcu(p, &init_task.tasks, tasks) {
		get_task_struct(p);
		mlt_task_init(p);
		put_task_struct(p);
	}
	rcu_read_unlock();

	if (init_mlt_nr_run()) {
		pr_err("failed to init mlt avg_nr_run\n");
		return -ENOMEM;
	}

	if (sysfs_create_bin_file(ems_kobj, &bin_attr_multi_load))
		pr_warn("failed to create multi_load\n");

	return 0;
}
