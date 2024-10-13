// SPDX-License-Identifier: GPL-2.0
/*
 * HALO (History AnaLysis Oriented) idle governor
 *
 * Copyright (C) 2022 - 2023 Samsung Corporation
 * Author: Youngtae Lee <yt0729.lee@samsung.com>
 */

#include <linux/cpuidle.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/tick.h>
#include <linux/hrtimer.h>
#include <linux/ems.h>

#include "../sched.h"
#include "ems.h"

#include <dt-bindings/soc/samsung/ems.h>
#include <trace/events/ems_debug.h>
#include <trace/events/ems.h>

/* previous wake-up reason */
enum waker {
	WAKEUP_BY_IPI,
	WAKEUP_BY_DET,			/* determined event like periodic IRQ, timer, tick */
	WAKEUP_BY_HALO_TIMER_1ST,	/* expired time from 1st try */
	WAKEUP_BY_HALO_TIMER_2ND,	/* expired time from 2nd try */
	WAKEUP_BY_SKIP_REFLECT,
	WAKEUP_BY_POLL,
};

/* C-state selector */
enum selector {
	BY_IPI,		/* matches with IPI */
	BY_DET,
	BY_CONSTRAINT,
	BY_ECS,
	BY_SKIP_REFLECT,
};

/* C-state selector */
enum timer_type {
	TIMER_1ST,
	TIMER_2ND,
	TIMER_NUM,
};

/* time and slen table */
#define HIST_NUM	7
struct time_table {
	s64 time;
	s64 slen;
};
struct time_hist {
	s32 cur_idx;
	struct time_table tbl[HIST_NUM];
};

/* hit/miss history */
#define PULSE		1024
#define DECAY_SHIFT	2
#define IGNORE_NUM	(1 << 7)
struct hit_state {
	u64 hit;	/* hit ratio */
	u64 miss;	/* miss ratio */
};

/* c-state data */
struct cstate_data {
	u64 entry_latency_ns;
	u64 exit_latency_ns;
	u64 target_residency_ns;
};

/* IPI's history */
struct halo_ipi {
	s64 ipi_slen_ns;	/* predicted sleep length to next ipi */
	struct time_hist time_hist;
};

/* Tick info */
struct halo_tick {
	int triggered;		/* tick stopped */
	s64 last_tick_ns;	/* last tick time */
	s64 tick_slen_ns;	/* sleep length to next tick */
};

/* halo htimer */
struct halo_timer {
	struct hrtimer timer;
	u32 triggered;		/* timer expired status */
	int type;		/* timer type */
	struct hit_state hstate;
};

/* halo sched-info */
struct halo_sched {
	u32 avg_nr_run;		/* number of running cpus */
	s32 heaviness;		/* system heaviness value from sched info */
};

/* periodic irq requester */
#define IRQ_NAME_LEN	20
struct halo_irq {
	struct list_head list;
	int num;		/* irq num */
	char name[IRQ_NAME_LEN];/* irq name */
	struct cpumask cpus;	/* irq affinity mask */
	u64 cnt;		/* irq accumulate count */
	ktime_t last_update_ns;	/* last irq triggered time */
	ktime_t period_ns;	/* irq period */

	void (*fn)(u64 *cnt, ktime_t *time);	/* irq validation check function */

	int enabled;		/* irq validate status */
	bool validated;		/* irq period is valid */
};

/* periodic irq list */
struct halo_irqs {
	struct list_head list;
	raw_spinlock_t	lock;
	s64 irq_slen_ns;
};

/* halo tuanble and sysfs */
#define	RATIO_UNIT		100
struct halo_tune {
	struct cpumask cpus;
	int expired_ratio;		/* def-timer expired time ratio */
	struct kobject		kobj;
};

/*
 * struct halo_cpu - CPU data used by the HAO Governor
 */
struct halo_cpu {
	u32 last_idx;			/* last selected c-state */
	enum waker last_waker;		/* last wakeup source */
	s64 last_slen_ns;		/* last sleep length */
	s64 timer_slen_ns;		/* sleep length to next timer event without tick */
	s64 req_latency_ns;		/* requested latency */
	s64 det_slen_ns;		/* determined sleep length to next event like(timer, tick, periodic irqs) */

	struct halo_ipi hipi;		/* ipi history */
	struct halo_tick htick;		/* tick information */
	struct halo_sched hsched;	/* sched information */
	struct halo_irqs hirqs;		/* periodic irqs list */
	struct halo_timer htimer;	/* timer struct to correct c-state */
	struct halo_tune *htune;	/* for tunable */

	struct cstate_data csd[CPUIDLE_STATE_MAX];	/* dynamic cstate data */

	struct cpuidle_device *dev;
};

struct kobject *halo_kobj;
static DEFINE_PER_CPU(struct halo_cpu, halo_cpus);

/********************************************************************************
 *				HALO HELPER FUNC				*
 *******************************************************************************/
static inline struct halo_cpu *get_hcpu(int cpu)
{
	return per_cpu_ptr(&halo_cpus, cpu);
}

/* return next time hist index */
static int halo_get_nxt_time_hist_idx(int idx)
{
	if (++idx == HIST_NUM)
		return 0;
	return idx;
}

/* update time hist table */
static void halo_update_time_hist(struct time_hist *time_hist, s64 now, s64 slen)
{
	int idx = halo_get_nxt_time_hist_idx(time_hist->cur_idx);
	time_hist->tbl[idx].time = now;
	time_hist->tbl[idx].slen = slen;
	time_hist->cur_idx = idx;
}

/*
 * compute statics from time hist table
 * return number of valid count
 */
static void halo_get_time_hist_statics(struct time_hist *time_hist,
			s64 now, s64 *min, s64 *max, s64 *avg)
{
	s64 slen, tmin = LLONG_MAX, tmax = 0, sum = 0;
	s32 idx = 0, min_idx = 0, max_idx = 0;

	/*
	 * It is noise filter.
	 * When computing avg sleep length from IPI's intervals,
	 * remove minimum/maximum values
	 */
	for (idx = 0; idx < HIST_NUM; idx++) {
		slen = time_hist->tbl[idx].slen;
		if (slen < tmin) {
			tmin = slen;
			min_idx = idx;
		}
		if (slen > tmax) {
			tmax = slen;
			max_idx = idx;
		}
	}

	/* find second min/max */
	for (idx = 0; idx < HIST_NUM; idx++) {
		if (max_idx == idx || min_idx == idx)
			continue;

		slen = time_hist->tbl[idx].slen;
		sum += slen;
		if (slen < tmin)
			tmin = slen;
		if (slen > tmax)
			tmax = slen;
	}

	*min = tmin;
	*max = tmax;
	*avg = sum / (HIST_NUM - 2);
}


/*
 * calculate the time remaining between now and next periodic event
 */
static s64 calc_periodic_slen(s64 last_update_ns, s64 period, s64 now)
{
	s64 offset_cnt, next_wakeup_ns;

	if (unlikely((now < last_update_ns) || !period))
		return LLONG_MAX;

	offset_cnt = ((now - last_update_ns) / period) + 1;
	next_wakeup_ns = last_update_ns + offset_cnt * period;

	return next_wakeup_ns - now;
}

/********************************************************************************
 *				HALO TIMER FUNC					*
 *******************************************************************************/
/*
 * halo_timer_fn - timer function
 */
static enum hrtimer_restart halo_timer_fn(struct hrtimer *h)
{
	u32 cpu = raw_smp_processor_id();
	struct halo_cpu *hcpu = get_hcpu(cpu);
	struct halo_timer *htimer = &hcpu->htimer;

	htimer->triggered = true;
	trace_halo_timer_fn(cpu, htimer->type);
	return HRTIMER_NORESTART;
}

/*
 * halo_timer_start
 * start halo timer when need a chance to change c-state in the future.
 * we do better change deeper c-state if cpu is in the shallower c-state
 * with wrong prediction.
 */
static void halo_timer_start(struct halo_cpu *hcpu, ktime_t expired_ns)
{
	struct halo_timer *htimer = &hcpu->htimer;

	/* In this case, we don't need a timer */
	if (expired_ns <= 0) {
		htimer->type = TIMER_NUM;
		return;
	}

	/* update timer type */
	if (hcpu->last_waker == WAKEUP_BY_HALO_TIMER_1ST)
		htimer->type = TIMER_2ND;
	else
		htimer->type = TIMER_1ST;

	hrtimer_start(&htimer->timer, expired_ns, HRTIMER_MODE_REL_PINNED);
}

/*
 * halo_timer_cancel - if timer is until active, cancel timer after wake up
 */
static void halo_timer_cancel(struct halo_timer *htimer)
{
	struct hrtimer *timer = &htimer->timer;
	ktime_t time_rem = hrtimer_get_remaining(timer);
	if (time_rem <= 0)
		return;
	hrtimer_try_to_cancel(timer);
}

/*
 * halo_need_timer
 * check whether time delta between predicted wake-up time and next event time
 * is longer than deeper c-state target residency.
 * return: When NEED a timer, return expired time if NOT return 0
 */
static s64 halo_need_timer(struct cpuidle_driver *drv, struct cpuidle_device *dev,
			struct halo_cpu *hcpu, s64 det_slen_ns,
			enum selector selector, u32 candi_idx)
{
	s64 delta_time, expired_time;
	u32 i, idx = candi_idx;

	/* we dont't need a timer when selected c-state is the deepest */
	if (idx == drv->state_count - 1)
		return 0;

	/* timer must work to change c-state when prediction will be failed */
	if (selector == BY_CONSTRAINT)
		return 0;

	/*
	 * check a delta time when preidcton will be failed and then timer will be expired,
	 * whether delta time is enough to enter deeper c-state or not.
	 * To check it, need to compute delta time and timer expired time.
	 * expired_time : htimer expired time to change c-state
	 * delta_time : delta time between timer wakeup and next wakeup event when add a timer
	 */
	expired_time = 4 * NSEC_PER_MSEC;

	delta_time = ktime_sub(det_slen_ns, expired_time);
	if (delta_time <= 0 )
		return 0;

	/*
	 * When delta_time  > deeper_cstate_target_residency,
	 * we have enough time to enter deeper c-state, so need a timer to change c-state
	 */
	for (i = candi_idx + 1; i < drv->state_count; i++) {
		u64 target_residency_ns = hcpu->csd[i].target_residency_ns;

		if (dev->states_usage[i].disable)
			continue;

		if (target_residency_ns < delta_time)
			idx = i;
	}

	if (idx == candi_idx)
		return 0;

	return expired_time;
}

/*
 * halo_timer_update_hstate
 * Update timer hit/miss statics to decide whether use 2nd time or not.
 * When 1st timer is expired and 2nd time hit ratio higher than miss ratio, use 2nd timer.
 */
static void halo_timer_update_hstate(int cpu, struct halo_timer *htimer)
{
	struct hit_state *hstate = &htimer->hstate;

	/* no timer */
	if (htimer->type >= TIMER_NUM)
		return;

	hstate->hit -= (hstate->hit >> DECAY_SHIFT);
	if (hstate->hit <= IGNORE_NUM)
		hstate->hit = 0;

	hstate->miss -= (hstate->miss >> DECAY_SHIFT);
	if (hstate->miss <= IGNORE_NUM)
		hstate->miss = 0;

	if (htimer->triggered)
		hstate->miss += PULSE;
	else
		hstate->hit += PULSE;

	trace_halo_hstate(cpu, 0, htimer->hstate.hit, htimer->hstate.miss);
}

/********************************************************************************
 *			HALO Scheduler Information				*
 *******************************************************************************/
/*
 * halo_update_sched_info - update sched information for more accurate prediction
 * When system more heavy than before, increase heaviness value
 */
#define VALID_NR_RUN_DIFF	(NR_RUN_UNIT >> 2)
static void halo_update_sched_info(struct cpuidle_device *dev, struct halo_cpu *hcpu)
{
	struct halo_sched *hsched = &hcpu->hsched;
	s32 avg_nr_run = mlt_avg_nr_run(cpu_rq(dev->cpu));
	s32 avg_nr_run_diff = avg_nr_run - hsched->avg_nr_run;

	hsched->avg_nr_run = avg_nr_run;

	if (abs(avg_nr_run_diff) < VALID_NR_RUN_DIFF) {
		hcpu->hsched.heaviness = 0;
		return;
	}

	hcpu->hsched.heaviness = min(avg_nr_run_diff, NR_RUN_UNIT);
}

/********************************************************************************
 *				HALO IPI INFO					*
 *******************************************************************************/
/*
 * halo_update_ipi - update IPI's history
 * Gather residency when task wake-up by IPI except tick, IRQ, timer event
 */
static void halo_update_ipi_history(int cpu, struct cpuidle_driver *drv,
					struct halo_cpu *hcpu, s64 now)
{
	struct halo_ipi *hipi = &hcpu->hipi;
	struct time_hist *time_hist = &hipi->time_hist;
	s64 min_ns = 0, max_ns = 0, avg_ns = 0;
	s64 correction_ns = 0;

	/*
	 * When last wake-up reason is IPI, need to update IPI history,
	 * Others, we don't need to update IPI history. Exclude last sleep time
	 * from IPI's history.
	 */
	if (hcpu->last_waker != WAKEUP_BY_IPI)
		return;

	/* applying last sleep length */
	halo_update_time_hist(time_hist, now, hcpu->last_slen_ns);

	/* get statics from time hist table with periodic duration, if there are valid data */
	halo_get_time_hist_statics(time_hist, now, &min_ns, &max_ns, &avg_ns);

	/*
	 * Computing predicted_sleep_length
	 * correction_ns is correction weight for predicted_sleep_length according to sched_info
	 * correction_ns decrease as the sched-load(nr_running) increases,
	 * on the other way it increase as the sched-load decreases.
	 */
	if (hcpu->hsched.heaviness > 0)
		correction_ns = (min_ns - avg_ns) * hcpu->hsched.heaviness / NR_RUN_UNIT;
	else
		correction_ns = (avg_ns - max_ns) * hcpu->hsched.heaviness / NR_RUN_UNIT;

	hipi->ipi_slen_ns = max((s64)0, (avg_ns + correction_ns));

	trace_halo_ipi(cpu, hcpu->hsched.heaviness, min_ns, max_ns,
		hcpu->last_slen_ns, avg_ns, correction_ns, hipi->ipi_slen_ns);
}

/********************************************************************************
 *				HALO TICK Information				*
 *******************************************************************************/
void halo_tick(struct rq *rq)
{
	struct halo_cpu *hcpu = get_hcpu(cpu_of(rq));
	struct halo_tick *htick = &hcpu->htick;

	if (unlikely(!hcpu->dev))
		return;

	/*
	 * When last_state_idx is -1 and called this function, it means cpu is waking by tick
	 * last_state_idx updated like below.
	 * 1. exit idle
	 * 2. if wake-up by tick, progress related work (execute this function)
	 * 3. halo_reflect (update lats_state_idx to last_state 0 or 1)
	 * 4. execute some jobs and done
	 * 5. halo_select (select cstate and update last_state_idx to -1)
	 * 6. enter idle
	 */
	if (hcpu->dev->last_state_idx < 0) {
		htick->triggered = true;
		htick->last_tick_ns = local_clock();
	}
	trace_halo_tick(cpu_of(rq), htick->triggered);
}

/**
 * halo_update_tick - Update tick info(next tick time and stopped status) after wakeup
 */
static void halo_update_tick(struct halo_cpu *hcpu, s64 now)
{
	struct halo_tick *htick = &hcpu->htick;

	htick->tick_slen_ns = calc_periodic_slen(htick->last_tick_ns, TICK_NSEC, now);
}

/********************************************************************************
 *				HALO Periodic IRQ				*
 *******************************************************************************/
/*
 * halo_align_periodic_irq
 * align irq's last_update_ns and accomulate cnt
 * and compute whether period is valide from count and time delta
 * if period is in error range, return TRUE, if NOT return FALSE
 */
#define ERROR_RATIO	10	/* 10% */
static bool check_irq_validation(struct halo_cpu *hcpu, struct halo_irq *hirq)
{
	u64 delta_cnt, prev_cnt = hirq->cnt;
	ktime_t delta_ns, prev_ns = hirq->last_update_ns;
	ktime_t error_range_ns, error_ns;
	ktime_t real_period_ns;
	unsigned long flags;

	raw_spin_lock_irqsave(&hcpu->hirqs.lock, flags);

	/* get recent cnt, last_update_ns */
	hirq->fn(&hirq->cnt, &hirq->last_update_ns);

	/* check whether period is validated or not */
	delta_cnt = hirq->cnt - prev_cnt;
	delta_ns = hirq->last_update_ns - prev_ns;
	real_period_ns = delta_ns / delta_cnt;

	error_range_ns = hirq->period_ns * ERROR_RATIO / 100;
	error_ns = abs(real_period_ns - hirq->period_ns);
	if (error_ns > error_range_ns) {
		hirq->validated = false;
		raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
		return false;
	}

	hirq->validated = true;
	raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
	return true;
}

static void halo_update_irq_slen(struct halo_cpu *hcpu, s64 now)
{
	struct halo_irq *hirq;
	s64 min_slen = LLONG_MAX;

	list_for_each_entry(hirq, &hcpu->hirqs.list, list) {
		s64 slen;
		if (!hirq->enabled || hirq->period_ns == 0)
			continue;

		/* If last_update_ns is before 1 sec, try to align time */
		if ((now - hirq->last_update_ns) > NSEC_PER_SEC) {
			/* if irq is not validated, don't use this irq */
			if (!check_irq_validation(hcpu, hirq))
				continue;
		}

		slen = calc_periodic_slen(hirq->last_update_ns, hirq->period_ns, now);
		if (slen < min_slen)
			min_slen = slen;
	}
	hcpu->hirqs.irq_slen_ns = min_slen;;
}

/*
 * check_irq_registable - return whether this irq is registable or not
 */
static int check_irq_registable(struct halo_cpu *hcpu, int cpu, int irq_num,
			struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time))
{
	struct halo_irq *hirq;

	if (cpumask_empty(cpus) || !fn)
		return 0;

	list_for_each_entry(hirq, &hcpu->hirqs.list, list)
		if (hirq->num == irq_num) {
			pr_info("%s: cpu%d already registered duplicated irq(%d)\n", cpu, irq_num);
			return -1;
		}
	return 0;
}

static struct halo_irq *find_hirq(struct halo_cpu *hcpu, int irq_num)
{
	struct halo_irq *hirq;

	list_for_each_entry(hirq, &hcpu->hirqs.list, list)
		if (hirq->num == irq_num)
			return hirq;
	return NULL;
}

/*
 * halo_register_periodic_irq - register irq to periodic irq list
 */
void halo_register_periodic_irq(int irq_num, const char *name,
		struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time))
{
	unsigned long flags;
	int cpu;

	for_each_cpu(cpu, cpus){
		struct halo_cpu *hcpu = get_hcpu(cpu);
		struct halo_irq *hirq;

		raw_spin_lock_irqsave(&hcpu->hirqs.lock, flags);

		/* check irq registable */
		if (check_irq_registable(hcpu, cpu, irq_num, cpus, fn)) {
			raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
			continue;
		}

		/* allocate irq information */
		hirq = kzalloc(sizeof(struct halo_irq), GFP_KERNEL);
		if (!hirq) {
			pr_err("%s: failed to allocate cpu%d irq(%d)\n", cpu, irq_num);
			raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
			continue;
		}
		hirq->num = irq_num;
		strcpy(hirq->name, name);
		cpumask_copy(&hirq->cpus, cpus);
		hirq->fn = fn;
		hirq->enabled = false;
		hirq->validated = false;

		/* reigster irq to list */
		list_add(&hirq->list, &hcpu->hirqs.list);

		raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
	}
}
EXPORT_SYMBOL_GPL(halo_register_periodic_irq);

/*
 * halo_update_periodic_irq - Requester notifies irq operation information
 */
void halo_update_periodic_irq(int irq_num, u64 cnt,
		ktime_t last_update_ns, ktime_t period_ns)
{
	unsigned long flags;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct halo_cpu *hcpu = get_hcpu(cpu);
		struct halo_irq *hirq;

		raw_spin_lock_irqsave(&hcpu->hirqs.lock, flags);
		hirq = find_hirq(hcpu, irq_num);
		if (!hirq) {
			raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
			continue;
		}

		/* update irq recent information */
		hirq->cnt = cnt;
		hirq->last_update_ns = last_update_ns;
		hirq->period_ns = period_ns;

		/* enable periodic irq */
		if (period_ns) {
			hirq->enabled = true;
			hirq->validated = true;
		} else {
			hirq->enabled = false;
			hirq->validated = false;
		}

		raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
	}
}
EXPORT_SYMBOL_GPL(halo_update_periodic_irq);

/*
 * halo_unregister_periodic_irq - unregister irq to periodic irq list
 */
void halo_unregister_periodic_irq(int irq_num)
{
	unsigned long flags;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct halo_cpu *hcpu = get_hcpu(cpu);
		struct halo_irq *hirq;

		raw_spin_lock_irqsave(&hcpu->hirqs.lock, flags);
		hirq = find_hirq(hcpu, irq_num);
		if (!hirq) {
			raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
			continue;
		}

		list_del(&hirq->list);
		kfree(hirq);
		raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
	}
}
EXPORT_SYMBOL_GPL(halo_unregister_periodic_irq);

/********************************************************************************
 *				HALO MAIN					*
 *******************************************************************************/
/* halo_get_cstate_data - gathering residency, entry/exit latency */
static void halo_get_cstate_data(struct cpuidle_driver *drv,
		struct cpuidle_device *dev, struct halo_cpu *hcpu)
{
	int i;

	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];
		u64 val;

		val = fv_get_residency(dev->cpu, i);
		hcpu->csd[i].target_residency_ns = val ? val : s->target_residency_ns;

		val = fv_get_exit_latency(dev->cpu, i);
		hcpu->csd[i].exit_latency_ns = val ? val : s->exit_latency_ns;
	}
}

/*
 * halo_update - Update CPU metrics after wakeup.
 * @drv: cpuidle driver containing state data.
 * @dev: Target CPU.
 */
static void halo_update(struct cpuidle_driver *drv, struct cpuidle_device *dev, s64 now)
{
	struct halo_cpu *hcpu = get_hcpu(dev->cpu);
	s64 tick_slen_ns;

	/* get latency */
	hcpu->req_latency_ns = cpuidle_governor_latency_req(dev->cpu);

	/* sleep length to next timer event */
	hcpu->timer_slen_ns = tick_nohz_get_sleep_length(&tick_slen_ns);

	/* Update Sched-Tick information */
	halo_update_tick(hcpu, now);

	/* update periodic irq sleep length */
	halo_update_irq_slen(hcpu, now);

	/* compute det_slen_ns */
	hcpu->det_slen_ns = min(hcpu->timer_slen_ns, hcpu->hirqs.irq_slen_ns);

	/* Update sched information */
	halo_update_sched_info(dev, hcpu);

	/* Update IPI's history */
	halo_update_ipi_history(dev->cpu, drv, hcpu, now);
}

/* halo_check_valid_state - return the available state number */
static int halo_get_valid_state(struct cpuidle_driver *drv,
				struct cpuidle_device *dev, int *valid_idx)
{
	int idx = 0, valid_cnt = 0;

	/* Check if there is any choice in the first place. */
	if (unlikely(drv->state_count < 2)) {
		*valid_idx = 0;
		valid_cnt = 1;
		return valid_cnt;
	}

	/* check enabled state */
	for (idx = 0; idx < drv->state_count; idx++) {
		if (!dev->states_usage[idx].disable) {
			*valid_idx = idx;
			valid_cnt++;
		}
	}
	return valid_cnt;
}

/*
 * halo_compute_pred_slen - computing predicted sleep length
 */
static inline s64 halo_compute_pred_slen(struct halo_cpu *hcpu, struct cpuidle_driver *drv)
{
	if (hcpu->last_waker == WAKEUP_BY_IPI ||
		hcpu->last_waker == WAKEUP_BY_DET ||
		hcpu->last_waker == WAKEUP_BY_SKIP_REFLECT)
		return hcpu->hipi.ipi_slen_ns;
	else if (hcpu->last_waker == WAKEUP_BY_HALO_TIMER_1ST) {
	/*
	 * When previous IPI prediction failed and second time timer hit ratio is high,
	 * give a change to use predicted_sleep_length one more
	 */
		struct hit_state *hstate = &hcpu->htimer.hstate;
		struct halo_sched *hsched = &hcpu->hsched;
		if (hstate->hit > hstate->miss &&
			hsched->avg_nr_run > VALID_NR_RUN_DIFF)
			return hcpu->hipi.ipi_slen_ns;
	}

	return LLONG_MAX;
}

/*
 * apply tunable value
 *
 * EXPIRED_RATIO tunes def-timere expired time length
 * default value is 100%. it means expired time is same with deeper c-state target rsidency.
 * The higher the ratio, higher the lower c-state entry rate.
 * The lower ratio, lower the high c-state entry rate.
 */
static s64 apply_tunable(struct halo_cpu *hcpu, s64 expird_time)
{
	struct halo_tune *htune = hcpu->htune;
	if (unlikely(!htune))
		return expird_time;
	return expird_time * hcpu->htune->expired_ratio / RATIO_UNIT;
}

/*
 * halo_select - Selects the next idle state to enter.
 * @drv: cpuidle driver containing state data.
 * @dev: Target CPU.
 * @stop_tick: Indication on whether or not to stop the scheduler tick.
 */
static int halo_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		      bool *stop_tick)
{
	struct halo_cpu *hcpu = get_hcpu(dev->cpu);
	s32 i, candi_idx = 0, pred_idx = 0, const_idx = 0;
	s32 deepest_idx = drv->state_count - 1;
	s64 pred_slen_ns = 0, expired_ns = 0;
	s64 now = local_clock();
	enum selector selector = 0;

	/* if valid count lower than two, we don't need to compute next state */
	if (halo_get_valid_state(drv, dev, &candi_idx) < 2)
		goto end;

	/* get cstate data */
	halo_get_cstate_data(drv, dev, hcpu);

	/* update last history */
	if (dev->last_state_idx >= 0) {
		halo_update(drv, dev, now);
	} else {
		hcpu->last_waker = WAKEUP_BY_SKIP_REFLECT;
		selector = BY_SKIP_REFLECT;
		halo_timer_cancel(&hcpu->htimer);
	}
	dev->last_state_idx = -1;

	if (!cpumask_test_cpu(dev->cpu, ecs_available_cpus())) {
		selector = BY_ECS;
		candi_idx = deepest_idx;
		goto end;
	}

	/* traversing c-state */
	pred_slen_ns = halo_compute_pred_slen(hcpu, drv);
	deepest_idx = 0;
	for (i = 0; i < drv->state_count; i++) {
		u64 target_residency_ns = hcpu->csd[i].target_residency_ns;
		u64 exit_latency_ns = hcpu->csd[i].exit_latency_ns;

		if (dev->states_usage[i].disable)
			continue;

		/* for performance, exit_latency should be short than req_latency */
		if (exit_latency_ns < hcpu->req_latency_ns)
			const_idx = i;

		/* for power, sleep length should be longer than target residency */
		if (target_residency_ns < pred_slen_ns)
			pred_idx = i;

		/* deepest c-state with next determined sleep length */
		if (target_residency_ns < hcpu->det_slen_ns)
			deepest_idx = i;
	}

	/* find c-state selector */
	if (pred_slen_ns < hcpu->det_slen_ns) {
		selector = BY_IPI;
		candi_idx = pred_idx;
	} else {
		selector = BY_DET;
		candi_idx = deepest_idx;
	}
	/* limit c-state depth to guarantee latency */
	if (candi_idx > const_idx) {
		struct rq *rq = cpu_rq(dev->cpu);

		selector = BY_CONSTRAINT;
		candi_idx = const_idx;

		if (rq->avg_idle < hcpu->csd[const_idx].target_residency_ns)
			*stop_tick = false;
	}

	/* When c-state is not constrainted, use deferred timer to change deeper c-state later */
	expired_ns = halo_need_timer(drv, dev, hcpu, hcpu->det_slen_ns,
					selector, candi_idx);
	expired_ns = apply_tunable(hcpu, expired_ns);
	halo_timer_start(hcpu, expired_ns);

end:
	trace_halo_select(hcpu->last_waker, selector, hcpu->last_slen_ns, hcpu->req_latency_ns,
			hcpu->htick.tick_slen_ns, hcpu->timer_slen_ns, hcpu->hirqs.irq_slen_ns,
			hcpu->det_slen_ns, pred_slen_ns, expired_ns, candi_idx);

	hcpu->last_slen_ns = now;
	hcpu->last_idx = candi_idx;

	return hcpu->last_idx;;
}

/*
 * halo_reflect - Note that governor data for the CPU need to be updated.
 * @dev: Target CPU.
 * @state: Entered state.
 */
static void halo_reflect(struct cpuidle_device *dev, int state)
{
	struct halo_cpu *hcpu = get_hcpu(dev->cpu);
	struct halo_tick *htick = &hcpu->htick;
	struct halo_timer *htimer = &hcpu->htimer;
	enum waker waker;
	s64 slen_ns;

	/* update last c-stae */
	dev->last_state_idx = state;

	/* compute last sleep length between idle_selection and wake_up */
	slen_ns = local_clock() - hcpu->last_slen_ns;

	/* compute htimer hit state */
	halo_timer_update_hstate(dev->cpu, htimer);

	if (htimer->triggered) {
		/* wake up by halo timer */
		if (htimer->type == TIMER_1ST)
			waker = WAKEUP_BY_HALO_TIMER_1ST;
		else
			waker = WAKEUP_BY_HALO_TIMER_2ND;
		htimer->triggered = false;
	} else if (dev->poll_time_limit) {
		/* wake up by poll */
		waker = WAKEUP_BY_POLL;
		dev->poll_time_limit = false;
	} else if (htick->triggered || slen_ns >= hcpu->det_slen_ns) {
		/* wake up by next determined event (timer, periodic irq, tick) */
		htick->triggered = false;
		waker = WAKEUP_BY_DET;
	} else {
		waker = WAKEUP_BY_IPI;
	}

	trace_halo_reflect(dev->cpu, waker, slen_ns);
	hcpu->last_slen_ns = slen_ns;
	hcpu->last_waker = waker;

	halo_timer_cancel(htimer);
}

/* halo_mode_update_callback - change tunable valuee according to ems mode */
static int halo_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct halo_tune *htune;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct halo_cpu *hcpu = get_hcpu(cpu);
		htune = hcpu->htune;

		if (cpu != cpumask_first(&htune->cpus))
			continue;

		htune->expired_ratio = cur_set->cpuidle_gov.expired_ratio[cpu];
	}

	return NOTIFY_OK;
}

static struct notifier_block halo_mode_update_notifier = {
	.notifier_call = halo_mode_update_callback,
};

/********************************************************************************
 *				HALO SYSFS					*
 *******************************************************************************/
struct halo_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define halo_attr_r(name)				\
static struct halo_attr name##_attr =			\
__ATTR(name, 0444, show_##name, NULL)

#define halo_attr_rw(name)				\
static struct halo_attr name##_attr =			\
__ATTR(name, 0644, show_##name, store_##name)

#define halo_show(name)								\
static ssize_t show_##name(struct kobject *k, char *buf)			\
{										\
	struct halo_tune *htune =					\
			container_of(k, struct halo_tune, kobj);		\
										\
	return sprintf(buf, "%d\n", htune->name);				\
}										\

#define halo_store(name)							\
static ssize_t store_##name(struct kobject *k, const char *buf, size_t count)	\
{										\
	struct halo_tune *htune =					\
			container_of(k, struct halo_tune, kobj);		\
	int data;								\
										\
	if (!sscanf(buf, "%d", &data))						\
		return -EINVAL;							\
										\
	htune->name = data;						\
	return count;								\
}

halo_show(expired_ratio);
halo_store(expired_ratio);
halo_attr_rw(expired_ratio);

static ssize_t show_irqs_list(struct kobject *k, char *buf)
{
	struct halo_tune *htune = container_of(k, struct halo_tune, kobj);
	unsigned long flags;
	int cpu, ret = 0;

	for_each_cpu(cpu, &htune->cpus) {
		struct halo_cpu *hcpu = get_hcpu(cpu);
		struct halo_irq *hirq;

		ret += sprintf(buf + ret, "cpu%d: ", cpu);

		raw_spin_lock_irqsave(&hcpu->hirqs.lock, flags);
		list_for_each_entry(hirq, &hcpu->hirqs.list, list)
			ret += sprintf(buf + ret, "(%d, %s period=%llu en=%d v=%d) ",
					hirq->num, hirq->name, hirq->period_ns,
					hirq->enabled, hirq->validated);
		ret += sprintf(buf + ret, "\n");
		raw_spin_unlock_irqrestore(&hcpu->hirqs.lock, flags);
	}
	return ret;
}
halo_attr_r(irqs_list);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct halo_attr *attr = container_of(at, struct halo_attr, attr);
	return attr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct halo_attr *attr = container_of(at, struct halo_attr, attr);
	return attr->store(kobj, buf, count);
}

static const struct sysfs_ops halo_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *halo_attrs[] = {
	&expired_ratio_attr.attr,
	&irqs_list_attr.attr,
	NULL
};

static struct kobj_type ktype_halo = {
	.sysfs_ops	= &halo_sysfs_ops,
	.default_attrs	= halo_attrs,
};

/**
 * halo_enable_device - Initialize the governor's data for the target CPU.
 * @drv: cpuidle driver (not used).
 * @dev: Target CPU.
 */
static int halo_enable_device(struct cpuidle_driver *drv,
			     struct cpuidle_device *dev)
{
	struct halo_cpu *hcpu = get_hcpu(dev->cpu);
	hcpu->dev = dev;
	return 0;
}

static struct cpuidle_governor halo_governor = {
	.name =		"halo",
	.rating =	19,
	.enable =	halo_enable_device,
	.select =	halo_select,
	.reflect =	halo_reflect,
};

static void halo_tunable_init(struct kobject *ems_kobj)
{
	struct device_node *dn, *child;
	int cpu;

	halo_kobj = kobject_create_and_add("halo", ems_kobj);
	if (!halo_kobj)
		return;

	dn = of_find_node_by_path("/ems/halo");
	if (!dn)
		return;

	for_each_child_of_node(dn, child) {
		struct halo_tune *htune;
		const char *buf;

		htune = kzalloc(sizeof(struct halo_tune), GFP_KERNEL);
		if (!htune) {
			pr_err("%s: failed to alloc halo_tune\n", __func__);
			return;
		}

		if (of_property_read_string(child, "cpus", &buf)) {
			pr_err("%s: cpus property is omitted\n", __func__);
			return;
		} else {
			cpulist_parse(buf, &htune->cpus);
		}

		if (of_property_read_u32(child, "expired-ratio", &htune->expired_ratio))
			htune->expired_ratio = RATIO_UNIT;

		if (kobject_init_and_add(&htune->kobj, &ktype_halo, halo_kobj,
					"coregroup%d", cpumask_first(&htune->cpus)))
			return;

		for_each_cpu(cpu, &htune->cpus) {
			struct halo_cpu *hcpu = get_hcpu(cpu);
			hcpu->htune = htune;
		}
	}

	emstune_register_notifier(&halo_mode_update_notifier);
}

/*
 * halo_cpu_init - initialize halo cpu data
 */
static void halo_cpu_init(void)
{
	s32 cpu;

	for_each_possible_cpu(cpu) {
		struct halo_cpu *hcpu = get_hcpu(cpu);
		struct hrtimer *timer;

		/* init deferred time */
		timer = &hcpu->htimer.timer;
		hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		timer->function = halo_timer_fn;

		/* init periodic irq list */
		raw_spin_lock_init(&hcpu->hirqs.lock);
		INIT_LIST_HEAD(&hcpu->hirqs.list);
	}
}

int halo_governor_init(struct kobject *ems_kobj)
{
	halo_tunable_init(ems_kobj);
	halo_cpu_init();
	return cpuidle_register_governor(&halo_governor);
}
