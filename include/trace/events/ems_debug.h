/*
 *  Copyright (C) 2019 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems_debug

#if !defined(_TRACE_EMS_DEBUG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_DEBUG_H

#include <linux/tracepoint.h>

struct mlt;
struct rq;
struct tp_env;
struct cpu_profile;

/*
 * Tracepoint for multiple test
 */
TRACE_EVENT(ems_test,

	TP_PROTO(struct task_struct *p, int cpu, u64 arg1, u64 arg2, char *event),

	TP_ARGS(p, cpu, arg1, arg2, event),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	u64,		arg1			)
		__field(	u64,		arg2			)
		__array(	char,		event,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->cpu			= cpu;
		__entry->arg1			= arg1;
		__entry->arg2			= arg2;
		strncpy(__entry->event, event ? event : "no event", 63);
	),

	TP_printk("comm=%s pid=%d cpu=%d arg1=%llu arg2=%llu event=%s",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->arg1, __entry->arg2, __entry->event)
);

/*
 * Tracepoint for taking util snapshot
 */
TRACE_EVENT(ems_take_util_snapshot,

	TP_PROTO(int cpu, struct tp_env *env),

	TP_ARGS(cpu, env),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	cpu)
		__field(	unsigned long,	task_util)
		__field(	unsigned long,	task_util_clamped)
		__field(	unsigned long,	task_load_avg)
		__field(	unsigned long,	util)
		__field(	unsigned long,	util_wo)
		__field(	unsigned long,	util_with)
		__field(	unsigned long,	runnable)
		__field(	unsigned long,	load_avg)
		__field(	unsigned long,	rt_util)
		__field(	unsigned long,	dl_util)
		__field(	unsigned int,	nr_running)
		__field(	int,		idle)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->cpu       		= cpu;
		__entry->task_util		= env->task_util;
		__entry->task_util_clamped	= env->task_util_clamped;
		__entry->task_load_avg		= env->task_load_avg;
		__entry->util			= env->cpu_stat[cpu].util;
		__entry->util_wo   		= env->cpu_stat[cpu].util_wo;
		__entry->util_with 		= env->cpu_stat[cpu].util_with;
		__entry->runnable 		= env->cpu_stat[cpu].runnable;
		__entry->load_avg 		= env->cpu_stat[cpu].load_avg;
		__entry->rt_util   		= env->cpu_stat[cpu].rt_util;
		__entry->dl_util   		= env->cpu_stat[cpu].dl_util;
		__entry->nr_running		= env->cpu_stat[cpu].nr_running;
		__entry->idle			= env->cpu_stat[cpu].idle;
	),

	TP_printk("comm=%s pid=%d cpu=%u "
		  "task_util=%lu task_util_clamped=%lu task_load_avg=%lu "
		  "util=%lu util_wo=%lu util_with=%lu runnable=%lu load_avg=%lu "
		  "rt_util=%lu dl_util=%lu nr_running=%u idle=%d",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->task_util, __entry->task_util_clamped,
		  __entry->task_load_avg,
		  __entry->util, __entry->util_wo, __entry->util_with,
		  __entry->runnable, __entry->load_avg,
		  __entry->rt_util, __entry->dl_util,
		  __entry->nr_running, __entry->idle)
);

/*
 * Tracepoint for setting fit_cpus by prio pinning
 */
TRACE_EVENT(tex_boosted_fit_cpus,

	TP_PROTO(struct task_struct *p, int sync, struct cpumask *fit_cpus),

	TP_ARGS(p, sync, fit_cpus),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prev_cpu			)
		__field(	int,		sync			)
		__field(	unsigned int,	fit_cpus		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prev_cpu	= task_cpu(p);
		__entry->sync		= sync;
		__entry->fit_cpus	= *(unsigned int *)cpumask_bits(fit_cpus);
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d sync=%d fit_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->prev_cpu, __entry->sync,
		  __entry->fit_cpus)
);

/*
 * Tracepoint for prio pinning schedule
 */
TRACE_EVENT(tex_pinning_schedule,

	TP_PROTO(int target_cpu, struct cpumask *candidates),

	TP_ARGS(target_cpu, candidates),

	TP_STRUCT__entry(
		__field(	int,		target_cpu	)
		__field(	unsigned int,	candidates	)
	),

	TP_fast_assign(
		__entry->target_cpu	= target_cpu;
		__entry->candidates	= *(unsigned int *)cpumask_bits(candidates);
	),

	TP_printk("target_cpu=%d candidates=%#x",
		__entry->target_cpu, __entry->candidates)
);

/*
 * Tracepoint for tex update stats
 */
TRACE_EVENT(tex_update_stats,

	TP_PROTO(struct task_struct *p, char *reason),

	TP_ARGS(p, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		tex_lv			)
		__field(	int,		prio			)
		__field(	u64,		tex_runtime			)
		__field(	u64,		tex_chances			)
		__array( char,		reason,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->tex_lv		= get_tex_level(p);
		__entry->prio		= p->prio;
		__entry->tex_runtime	= ems_tex_runtime(p);
		__entry->tex_chances	= ems_tex_chances(p);
		strncpy(__entry->reason, reason, 63);
	),

	TP_printk("comm=%s pid=%d tex_lv=%d prio=%d tex_runtime=%llu tex_chances=%llu reason=%s",
		  __entry->comm, __entry->pid, __entry->tex_lv,
		  __entry->prio,  __entry->tex_runtime, __entry->tex_chances,
		  __entry->reason)
);

/*
 * Template for tex task
 */
DECLARE_EVENT_CLASS(tex_task_trace_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		tex_lv			)
		__field(	int,		prio			)
		__field(	u64,		tex_runtime			)
		__field(	u64,		tex_chances			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->tex_lv		= get_tex_level(p);
		__entry->prio		= p->prio;
		__entry->tex_runtime	= ems_tex_runtime(p);
		__entry->tex_chances	= ems_tex_chances(p);
	),

	TP_printk("comm=%s pid=%d tex_lv=%d prio=%d tex_runtime=%llu tex_chances=%llu",
		  __entry->comm, __entry->pid, __entry->tex_lv,
		  __entry->prio,  __entry->tex_runtime, __entry->tex_chances)
);

/* Tracepoint for task picked by queue jump */
DEFINE_EVENT(tex_task_trace_template, tex_qjump_pick_next_task,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/* Tracepoint for insert tex task to qjump list */
DEFINE_EVENT(tex_task_trace_template, tex_insert_to_qjump_list,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/* Tracepoint for remove tex task from qjump list*/
DEFINE_EVENT(tex_task_trace_template, tex_remove_from_qjump_list,
		TP_PROTO(struct task_struct *p), TP_ARGS(p));

/*
 * Tracepoint for check preempt
 */
TRACE_EVENT(tex_check_preempt_wakeup,

	TP_PROTO(struct task_struct *curr, struct task_struct *p, bool preempt, bool ignore),

	TP_ARGS(curr, p, preempt, ignore),

	TP_STRUCT__entry(
		__array(	char,		curr_comm,	TASK_COMM_LEN	)
		__field(	int,		curr_tex			)
		__array(	char,		p_comm,	TASK_COMM_LEN	)
		__field(	int,		p_tex			)
		__field(	bool,		preempt			)
		__field(	bool,		ignore			)
	),

	TP_fast_assign(
		memcpy(__entry->curr_comm, curr->comm, TASK_COMM_LEN);
		__entry->curr_tex		= get_tex_level(curr);
		memcpy(__entry->p_comm, p->comm, TASK_COMM_LEN);
		__entry->p_tex		= get_tex_level(p);
		__entry->preempt		= preempt;
		__entry->ignore		= ignore;
	),

	TP_printk("curr_comm=%s curr_tex=%d p_comm=%s p_tex=%d preempt=%d ignore=%d",
		  __entry->curr_comm, __entry->curr_tex, __entry->p_comm, __entry->p_tex,
		  __entry->preempt, __entry->ignore)
);

/*
 * Tracepoint for finding best perf cpu
 */
TRACE_EVENT(ems_find_best_perf_cpu,

	TP_PROTO(struct tp_env *env, int best_cpu),

	TP_ARGS(env, best_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		task_util		)
		__field(	int,		best_cpu		)
		__field(	int,		idle			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid		= env->p->pid;
		__entry->task_util	= env->task_util;
		__entry->best_cpu	= best_cpu;
		__entry->idle		= best_cpu < 0 ? -1 : env->cpu_stat[best_cpu].idle;
	),

	TP_printk("comm=%s pid=%d tsk_util=%d best_cpu=%d idle=%d",
		__entry->comm, __entry->pid, __entry->task_util,
		__entry->best_cpu, __entry->idle)
);

/*
 * Tracepoint for finding lowest energy cpu
 */
TRACE_EVENT(ems_find_energy_cpu,

	TP_PROTO(struct task_struct *p, struct cpumask *candidates,
				int energy_cpu, int adv_energy_cpu),

	TP_ARGS(p, candidates, energy_cpu, adv_energy_cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
		__field(	int,		energy_cpu		)
		__field(	int,		adv_energy_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->candidates		= *(unsigned int *)cpumask_bits(candidates);
		__entry->energy_cpu		= energy_cpu;
		__entry->adv_energy_cpu		= adv_energy_cpu;
	),

	TP_printk("comm=%s pid=%d candidates=%#x energy_cpu=%d adv_energy_cpu=%d",
		  __entry->comm, __entry->pid, __entry->candidates,
		  __entry->energy_cpu, __entry->adv_energy_cpu)
);

/*
 * Tracepoint for computing energy
 */
TRACE_EVENT(ems_compute_energy,

	TP_PROTO(int cpu, unsigned long util, unsigned long cap,
		unsigned long f, unsigned long v, unsigned long t,
		unsigned long dp, unsigned long sp, unsigned long energy),

	TP_ARGS(cpu, util, cap, f, v, t, dp, sp, energy),

	TP_STRUCT__entry(
		__field(	int,		cpu			)
		__field(	unsigned long,	util			)
		__field(	unsigned long,	cap			)
		__field(	unsigned long,	f			)
		__field(	unsigned long,	v			)
		__field(	unsigned long,	t			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	sp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->util		= util;
		__entry->cap		= cap;
		__entry->f		= f;
		__entry->v		= v;
		__entry->t		= t;
		__entry->dp		= dp;
		__entry->sp		= sp;
		__entry->energy		= energy;
	),

	TP_printk("cpu=%d util=%lu cap=%lu f=%lu v=%lu t=%lu dp=%lu sp=%lu energy=%lu",
		__entry->cpu, __entry->util, __entry->cap,
		__entry->f, __entry->v, __entry->t, __entry->dp, __entry->sp, __entry->energy)
);

/*
 * Tracepoint for computing DSU energy
 */
TRACE_EVENT(ems_compute_sl_energy,

	TP_PROTO(struct cpumask *cpus, unsigned long sp, unsigned long energy),

	TP_ARGS(cpus, sp, energy),

	TP_STRUCT__entry(
		__field(	unsigned int,	cpus			)
		__field(	unsigned long,	sp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->cpus		= *(unsigned int *)cpumask_bits(cpus);
		__entry->sp		= sp;
		__entry->energy		= energy;
	),

	TP_printk("cpus=%#x sp=%lu energy=%lu",
		__entry->cpus, __entry->sp, __entry->energy)
);

/*
 * Tracepoint for computing DSU energy
 */
TRACE_EVENT(ems_compute_dsu_energy,

	TP_PROTO(unsigned long freq, unsigned long volt,
		unsigned long dp, unsigned long sp, unsigned long energy),

	TP_ARGS(freq, volt, dp, sp, energy),

	TP_STRUCT__entry(
		__field(	unsigned long,	freq			)
		__field(	unsigned long,	volt			)
		__field(	unsigned long,	dp			)
		__field(	unsigned long,	sp			)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		__entry->freq		= freq;
		__entry->volt		= volt;
		__entry->dp		= dp;
		__entry->sp		= sp;
		__entry->energy		= energy;
	),

	TP_printk("freq=%lu volt=%lu dp=%lu sp=%lu energy=%lu",
		__entry->freq, __entry->volt, __entry->dp, __entry->sp, __entry->energy)
);

/*
 * Tracepoint for computing system energy
 */
TRACE_EVENT(ems_compute_system_energy,

	TP_PROTO(struct task_struct *p, const struct cpumask *candidates,
			int target_cpu, unsigned long energy),

	TP_ARGS(p, candidates, target_cpu, energy),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned int,	candidates		)
		__field(	int,		target_cpu		)
		__field(	unsigned long,	energy			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->candidates	= *(unsigned int *)cpumask_bits(candidates);
		__entry->target_cpu	= target_cpu;
		__entry->energy		= energy;
	),

	TP_printk("comm=%s pid=%d candidates=%#x target_cpu=%d energy=%lu",
		__entry->comm, __entry->pid, __entry->candidates,
		__entry->target_cpu, __entry->energy)
);

/*
 * Tracepoint for core selection by sysbusy schedule
 */
TRACE_EVENT(sysbusy_schedule,

	TP_PROTO(struct task_struct *p, int state, int target_cpu),

	TP_ARGS(p, state, target_cpu),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field(unsigned long,	util			)
		__field(int,		state			)
		__field(int,		target_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= p->se.avg.util_avg;
		__entry->state		= state;
		__entry->target_cpu	= target_cpu;
	),

	TP_printk("comm=%s pid=%d util=%ld state=%d target_cpu=%d",
		__entry->comm, __entry->pid, __entry->util,
		__entry->state, __entry->target_cpu)
);

/*
 * Tracepoint for monitoring sysbusy state
 */
TRACE_EVENT(ems_cpu_profile,

	TP_PROTO(int cpu, struct cpu_profile *cs, int ar_avg, int nr_running),

	TP_ARGS(cpu, cs, ar_avg, nr_running),

	TP_STRUCT__entry(
		__field(	s32,	cpu			)
		__field(	u64,	cpu_util		)
		__field(	u64,	cpu_util_busy		)
		__field(	u64,	cpu_wratio		)
		__field(	u64,	cpu_wratio_busy		)
		__field(	u64,	hratio			)
		__field(	u64,	pid			)
		__field(	int,	ar_avg			)
		__field(	int,	nr_running		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cpu_util		= cs[CPU_UTIL].value;
		__entry->cpu_util_busy		= cs[CPU_UTIL].data;
		__entry->cpu_wratio		= cs[CPU_WRATIO].value;
		__entry->cpu_wratio_busy	= cs[CPU_WRATIO].data;
		__entry->hratio			= cs[CPU_HTSK].value;
		__entry->pid			= cs[CPU_HTSK].data;
		__entry->ar_avg			= ar_avg;
		__entry->nr_running		= nr_running;
	),

	TP_printk("cpu=%d cpu_util=%lld(busy=%lld), cpu_wratio=%lld(busy=%lld), hratio=%lld (pid=%lld), "
		  "ar_avg=%d, nr_running=%d",
			__entry->cpu, __entry->cpu_util, __entry->cpu_util_busy,
			__entry->cpu_wratio, __entry->cpu_wratio_busy,
			__entry->hratio, __entry->pid, __entry->ar_avg, __entry->nr_running)
);


/*
 * Tracepoint for tracking tasks in system
 */
TRACE_EVENT(ems_profile_tasks,

	TP_PROTO(int busy_cpu_count, unsigned long cpu_util_sum,
		int heavy_task_count, unsigned long heavy_task_util_sum,
		int misfit_task_count, unsigned long misfit_task_util_sum,
		int pd_nr_running),

	TP_ARGS(busy_cpu_count, cpu_util_sum,
		heavy_task_count, heavy_task_util_sum,
		misfit_task_count, misfit_task_util_sum,
		pd_nr_running),

	TP_STRUCT__entry(
		__field(	int,		busy_cpu_count		)
		__field(	unsigned long,	cpu_util_sum		)
		__field(	int,		heavy_task_count	)
		__field(	unsigned long,	heavy_task_util_sum	)
		__field(	int,		misfit_task_count	)
		__field(	unsigned long,	misfit_task_util_sum	)
		__field(	int,		pd_nr_running)
	),

	TP_fast_assign(
		__entry->busy_cpu_count		= busy_cpu_count;
		__entry->cpu_util_sum		= cpu_util_sum;
		__entry->heavy_task_count	= heavy_task_count;
		__entry->heavy_task_util_sum	= heavy_task_util_sum;
		__entry->misfit_task_count	= misfit_task_count;
		__entry->misfit_task_util_sum	= misfit_task_util_sum;
		__entry->pd_nr_running		= pd_nr_running;
	),

	TP_printk("busy_cpu_count=%d cpu_util_sum=%lu "
		  "heavy_task_count=%d heavy_task_util_sum=%lu "
		  "misfit_task_count=%d misfit_task_util_sum=%lu "
		  "pd_nr_running=%d",
		  __entry->busy_cpu_count, __entry->cpu_util_sum,
		  __entry->heavy_task_count, __entry->heavy_task_util_sum,
		  __entry->misfit_task_count, __entry->misfit_task_util_sum,
		  __entry->pd_nr_running)
);

/*
 * Tracepoint for somac scheduling
 */
TRACE_EVENT(sysbusy_somac,

	TP_PROTO(struct task_struct *p, unsigned long util,
		int src_cpu, int dst_cpu, unsigned long time),

	TP_ARGS(p, util, src_cpu, dst_cpu, time),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	util			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__field(	unsigned long,	time			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->util		= util;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->time		= time;
	),

	TP_printk("comm=%s pid=%d util_avg=%lu src_cpu=%d dst_cpu=%d time=%lu",
		__entry->comm, __entry->pid, __entry->util,
		__entry->src_cpu, __entry->dst_cpu, __entry->time)
);

/*
 * Tracepoint for move somac task
 */
TRACE_EVENT(sysbusy_mv_somac_task,

	TP_PROTO(int src_cpu, char *label),

	TP_ARGS(src_cpu, label),

	TP_STRUCT__entry(
		__field( int,		src_cpu			)
		__field( int,		cfs_nr			)
		__field( int,		cfs_h_nr		)
		__field( int,		active_bal		)
		__field( u64,		somac_deq		)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		__entry->src_cpu		= src_cpu;
		__entry->cfs_nr		= cpu_rq(src_cpu)->cfs.nr_running;
		__entry->cfs_h_nr		= cpu_rq(src_cpu)->cfs.h_nr_running;
		__entry->active_bal		= cpu_rq(src_cpu)->active_balance;
		__entry->somac_deq		= ems_somac_dequeued(cpu_rq(src_cpu));
		strncpy(__entry->label, label, 63);
	),

	TP_printk("src_cpu=%d cfs_nr=%d cfs_h_nr=%d active_bal=%d somac_deq=%d reason=%s",
		__entry->src_cpu, __entry->cfs_nr, __entry->cfs_h_nr, __entry->active_bal,
		__entry->somac_deq, __entry->label)
);

/*
 * Tracepoint for somac reason
 */
TRACE_EVENT(sysbusy_somac_reason,

	TP_PROTO(int src_cpu, int dst_cpu, char *label),

	TP_ARGS(src_cpu, dst_cpu, label),

	TP_STRUCT__entry(
		__field( int,		src_cpu			)
		__field( int,		dst_cpu			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		__entry->src_cpu		= src_cpu;
		__entry->dst_cpu		= dst_cpu;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("src_cpu=%d dst_cpu=%d reason=%s",
		__entry->src_cpu, __entry->dst_cpu, __entry->label)
);

/*
 * Tracepoint for the STT
 */
TRACE_EVENT(stt_update_cpu,

	TP_PROTO(int cpu, int cur_ar, int htsk_ar, int pid, int ht_en, u64 next_update,
		int hcnt, u64 last_update, int htask_boost, int state),

	TP_ARGS(cpu, cur_ar, htsk_ar, pid, ht_en, next_update, hcnt, last_update, htask_boost, state),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_ar			)
		__field( int,	htsk_ar			)
		__field( int,	pid			)
		__field( int,	ht_en			)
		__field( u64,	next_update		)
		__field( int,	hcnt			)
		__field( u64,	last_update		)
		__field( int,	htask_boost		)
		__field( int,	state			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cur_ar			= cur_ar;
		__entry->htsk_ar		= htsk_ar;
		__entry->pid			= pid;
		__entry->ht_en			= ht_en;
		__entry->next_update		= next_update;
		__entry->hcnt			= hcnt;
		__entry->last_update		= last_update;
		__entry->htask_boost		= htask_boost;
		__entry->state			= state;
	),

	TP_printk("cpu=%d cur_ar=%d htask_ar=%d pid=%d(hten=%d) nextup=%lu hcnt=%d lastup=%lu hboost=%d state=%d",
		__entry->cpu, __entry->cur_ar, __entry->htsk_ar,
		__entry->pid, __entry->ht_en, __entry->next_update, __entry->hcnt,
		__entry->last_update, __entry->htask_boost, __entry->state)
);

TRACE_EVENT(stt_update_htsk_heaviness,

	TP_PROTO(int cpu, int pid, int idx, int ratio, int cnt),

	TP_ARGS(cpu, pid, idx, ratio, cnt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	pid			)
		__field( int,	idx			)
		__field( int,	ratio			)
		__field( int,	cnt			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pid			= pid;
		__entry->idx			= idx;
		__entry->ratio			= ratio;
		__entry->cnt			= cnt;
	),

	TP_printk("cpu=%d pid=%d idx=%d ratio=%d cnt=%d",
		__entry->cpu, __entry->pid, __entry->idx, __entry->ratio, __entry->cnt)
);

/*
 * Tracepoint for EGO
 */
TRACE_EVENT(ego_sched_util,

	TP_PROTO(int cpu, unsigned long util, unsigned long cfs, unsigned long rt,
		unsigned long dl, unsigned long bwdl, unsigned long irq),

	TP_ARGS(cpu, util, cfs, rt, dl, bwdl, irq),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	util				)
		__field( unsigned long,	cfs				)
		__field( unsigned long,	rt				)
		__field( unsigned long,	dl				)
		__field( unsigned long,	bwdl				)
		__field( unsigned long,	irq				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->util			= util;
		__entry->cfs			= cfs;
		__entry->rt			= rt;
		__entry->dl			= dl;
		__entry->bwdl			= bwdl;
		__entry->irq			= irq;
	),

	TP_printk("cpu=%d util=%ld cfs=%ld rt=%ld dl=%ld dlbw=%ld irq=%ld",
		__entry->cpu, __entry->util, __entry->cfs, __entry->rt,
		__entry->dl, __entry->bwdl, __entry->irq)
);

TRACE_EVENT(ego_cpu_util,

	TP_PROTO(int cpu, unsigned long pelt_boost, unsigned long util, unsigned long io_util, unsigned long bst_util),

	TP_ARGS(cpu, pelt_boost, util, io_util, bst_util),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	pelt_boost			)
		__field( unsigned long,	util				)
		__field( unsigned long,	io_util				)
		__field( unsigned long,	bst_util			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pelt_boost		= pelt_boost;
		__entry->util			= util;
		__entry->io_util		= io_util;
		__entry->bst_util		= bst_util;
	),

	TP_printk("cpu=%d pelt_boost=%ld, util=%ld io_util=%ld bst_util=%ld",
		__entry->cpu, __entry->pelt_boost, __entry->util, __entry->io_util,
		__entry->bst_util)
);

TRACE_EVENT(ego_cpu_idle_ratio,

	TP_PROTO(int cpu, int flag, int idx, u64 s1, u64 s2, u64 s3, u64 s4, int last_idx),

	TP_ARGS(cpu, flag, idx, s1, s2, s3, s4, last_idx),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( int,		flag				)
		__field( int,		idx				)
		__field( u64,		s1				)
		__field( u64,		s2				)
		__field( u64,		s3				)
		__field( u64,		s4				)
		__field( int,		last_idx			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->flag			= flag;
		__entry->idx			= idx;
		__entry->s1			= s1;
		__entry->s2			= s2;
		__entry->s3			= s3;
		__entry->s4			= s4;
		__entry->last_idx		= last_idx;
	),

	TP_printk("cpu=%d f=%2d(lidx=%d, %ld->%ld) idx=%d clkoff=%ld pwroff=%ld",
		__entry->cpu, __entry->flag, __entry->last_idx, __entry->s3, __entry->s4,
		__entry->idx, __entry->s1, __entry->s2)
);

TRACE_EVENT(ego_cpu_eng,

	TP_PROTO(int cpu, unsigned long cap, unsigned int dyn,
		unsigned long sta, unsigned long idle_sum,
		unsigned long active_eng, unsigned long idle_eng),

	TP_ARGS(cpu, cap, dyn, sta, idle_sum, active_eng, idle_eng),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned long,	cap				)
		__field( unsigned long,	dyn				)
		__field( unsigned long,	sta				)
		__field( unsigned long,	idle_sum			)
		__field( unsigned long,	active_eng			)
		__field( unsigned long,	idle_eng			)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->cap			= cap;
		__entry->dyn			= dyn;
		__entry->sta			= sta;
		__entry->idle_sum		= idle_sum;
		__entry->active_eng		= active_eng;
		__entry->idle_eng		= idle_eng;
	),

	TP_printk("cpu=%d cap=%lu dyn=%lu sta=%lu isum=%lu aeng=%lu ieng=%lu eng=%lu",
		__entry->cpu, __entry->cap, __entry->dyn,
		__entry->sta, __entry->idle_sum,
		__entry->active_eng, __entry->idle_eng,
		(__entry->active_eng + __entry->idle_eng))
);

/*
 * Tracepoint for slack timer
 */
TRACE_EVENT(slack_timer,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d slack timer expired", __entry->cpu)
);

/*
 * Tracepoint for checking need for slack timer
 */
TRACE_EVENT(ego_need_slack_timer,

	TP_PROTO(int cpu, unsigned long boosted_util, unsigned long min_cap,
		int heaviest_cpu, unsigned int cur_freq, unsigned int min_freq,
		unsigned int energy_freq, unsigned int orig_freq, int need),

	TP_ARGS(cpu, boosted_util, min_cap, heaviest_cpu, cur_freq,
		min_freq, energy_freq, orig_freq, need),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned long,	boosted_util)
		__field(unsigned long,	min_cap)
		__field(int,		heaviest_cpu)
		__field(unsigned int,	cur_freq)
		__field(unsigned int,	min_freq)
		__field(unsigned int,	energy_freq)
		__field(unsigned int,	orig_freq)
		__field(int,		need)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->boosted_util	= boosted_util;
		__entry->min_cap	= min_cap;
		__entry->heaviest_cpu	= heaviest_cpu;
		__entry->cur_freq	= cur_freq;
		__entry->min_freq	= min_freq;
		__entry->energy_freq	= energy_freq;
		__entry->orig_freq	= orig_freq;
		__entry->need		= need;
	),

	TP_printk("cpu=%d boosted_util=%lu min_cap=%lu heaviest_cpu=%d cur_freq=%u min_freq=%u energy_freq=%u orig_freq=%u need=%d",
		  __entry->cpu, __entry->boosted_util, __entry->min_cap,
		  __entry->heaviest_cpu, __entry->cur_freq, __entry->min_freq,
		  __entry->energy_freq, __entry->orig_freq, __entry->need)
);

/*
 * Tracepoint for slack timer func called
 */
TRACE_EVENT(cpufreq_gov_slack_func,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu=%d SLACK EXPIRED", __entry->cpu)
);

/*
 * Tracepoint for detalis of slack timer func called
 */
TRACE_EVENT(cpufreq_gov_slack,

	TP_PROTO(int cpu, unsigned long util,
		unsigned long min, unsigned long action),

	TP_ARGS(cpu, util, min, action),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned long, min)
		__field(unsigned long, action)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->action = action;
	),

	TP_printk("cpu=%d util=%ld min=%ld action=%ld",
		__entry->cpu, __entry->util, __entry->min, __entry->action)
);

/*
 * Tracepoint for available cpus in FRT
 */
TRACE_EVENT(frt_available_cpus,

	TP_PROTO(unsigned int domain, int util_sum, int busy_thr, unsigned int available_mask),

	TP_ARGS(domain, util_sum, busy_thr, available_mask),

	TP_STRUCT__entry(
		__field(	unsigned int,	domain	)
		__field(	int,		util_sum	)
		__field(	int,		busy_thr	)
		__field(	unsigned int,	available_mask	)
	),

	TP_fast_assign(
		__entry->domain		= domain;
		__entry->util_sum	= util_sum;
		__entry->busy_thr	= busy_thr;
		__entry->available_mask	= available_mask;
	),

	TP_printk("domain=%x util_sum=%d busy_thr=%d available_mask=%x",
		__entry->domain, __entry->util_sum,
		__entry->busy_thr, __entry->available_mask)
);

/*
 * Tracepoint for multi load tracking for cpu
 */
TRACE_EVENT(mlt_update_cpu,

	TP_PROTO(struct mlt *mlt),

	TP_ARGS(mlt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_period		)
		__field( int,	state			)
		__field( int,	g0_recent		)
		__field( int,	g1_recent		)
		__field( int,	g2_recent		)
		__field( int,	g3_recent		)
		__field( int,	g4_recent		)
		__field( int,	g5_recent		)
		__field( int,	g6_recent		)
		__field( int,	g7_recent		)
		__field( int,	g8_recent		)
		__field( int,	recent_sum		)
		__field( int,	g0_last			)
		__field( int,	g1_last			)
		__field( int,	g2_last			)
		__field( int,	g3_last			)
		__field( int,	g4_last			)
		__field( int,	g5_last			)
		__field( int,	g6_last			)
		__field( int,	g7_last			)
		__field( int,	g8_last			)
		__field( int,	last_sum		)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		__entry->cpu		= mlt->cpu;
		__entry->cur_period	= mlt->cur_period;
		__entry->state		= mlt->state;
		__entry->g0_recent	= mlt->recent[0];
		__entry->g1_recent	= mlt->recent[1];
		__entry->g2_recent	= mlt->recent[2];
		__entry->g3_recent  	= mlt->recent[3];
		__entry->g4_recent  	= mlt->recent[4];
		__entry->g5_recent  	= mlt->recent[5];
		__entry->g6_recent	= mlt->recent[6];
		__entry->g7_recent	= mlt->recent[7];
		__entry->g8_recent	= mlt->recent[8];
		__entry->recent_sum 	= mlt->recent[0] + mlt->recent[1] +
					  mlt->recent[2] + mlt->recent[3] +
					  mlt->recent[4] + mlt->recent[5] +
					  mlt->recent[6] + mlt->recent[7] +
					  mlt->recent[8];
		__entry->g0_last    	= mlt->periods[0][i];
		__entry->g1_last  	= mlt->periods[1][i];
		__entry->g2_last  	= mlt->periods[2][i];
		__entry->g3_last  	= mlt->periods[3][i];
		__entry->g4_last    	= mlt->periods[4][i];
		__entry->g5_last    	= mlt->periods[5][i];
		__entry->g6_last    	= mlt->periods[6][i];
		__entry->g7_last  	= mlt->periods[7][i];
		__entry->g8_last  	= mlt->periods[8][i];
		__entry->last_sum	= mlt->periods[0][i] + mlt->periods[1][i] +
					  mlt->periods[2][i] + mlt->periods[3][i] +
					  mlt->periods[4][i] + mlt->periods[5][i] +
					  mlt->periods[6][i] + mlt->periods[7][i] +
					  mlt->periods[8][i];
	),

	TP_printk("cpu=%d cur_period=%d state=%d recent=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d) "
			"last=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d)",
		__entry->cpu, __entry->cur_period, __entry->state,
		__entry->g0_recent, __entry->g1_recent, __entry->g2_recent,
		__entry->g3_recent, __entry->g4_recent, __entry->g5_recent,
		__entry->g6_recent, __entry->g7_recent, __entry->g8_recent, __entry->recent_sum,
		__entry->g0_last, __entry->g1_last, __entry->g2_last,
		__entry->g3_last, __entry->g4_last, __entry->g5_last,
		__entry->g6_last, __entry->g7_last, __entry->g8_last, __entry->last_sum)
);

/*
 * Tracepoint for multi load tracking for cpu IPC
 */
TRACE_EVENT(mlt_update_cpu_ipc,

	TP_PROTO(struct mlt *mlt),

	TP_ARGS(mlt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_period		)
		__field( int,	state			)
		__field( int,	g0_recent		)
		__field( int,	g1_recent		)
		__field( int,	g2_recent		)
		__field( int,	g3_recent		)
		__field( int,	g4_recent		)
		__field( int,	g5_recent		)
		__field( int,	g6_recent		)
		__field( int,	g7_recent		)
		__field( int,	g8_recent		)
		__field( int,	recent_sum		)
		__field( int,	g0_last			)
		__field( int,	g1_last			)
		__field( int,	g2_last			)
		__field( int,	g3_last			)
		__field( int,	g4_last			)
		__field( int,	g5_last			)
		__field( int,	g6_last			)
		__field( int,	g7_last			)
		__field( int,	g8_last			)
		__field( int,	last_sum		)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		__entry->cpu		= mlt->cpu;
		__entry->cur_period	= mlt->cur_period;
		__entry->state		= mlt->state;
		__entry->g0_recent	= mlt->uarch->ipc_recent[0];
		__entry->g1_recent	= mlt->uarch->ipc_recent[1];
		__entry->g2_recent	= mlt->uarch->ipc_recent[2];
		__entry->g3_recent  	= mlt->uarch->ipc_recent[3];
		__entry->g4_recent  	= mlt->uarch->ipc_recent[4];
		__entry->g5_recent  	= mlt->uarch->ipc_recent[5];
		__entry->g6_recent	= mlt->uarch->ipc_recent[6];
		__entry->g7_recent	= mlt->uarch->ipc_recent[7];
		__entry->g8_recent	= mlt->uarch->ipc_recent[8];
		__entry->recent_sum 	= mlt->uarch->ipc_recent[0] +
					  mlt->uarch->ipc_recent[1] +
					  mlt->uarch->ipc_recent[2] +
					  mlt->uarch->ipc_recent[3] +
					  mlt->uarch->ipc_recent[4] +
					  mlt->uarch->ipc_recent[5] +
					  mlt->uarch->ipc_recent[6] +
					  mlt->uarch->ipc_recent[7] +
					  mlt->uarch->ipc_recent[8];
		__entry->g0_last    	= mlt->uarch->ipc_periods[0][i];
		__entry->g1_last  	= mlt->uarch->ipc_periods[1][i];
		__entry->g2_last  	= mlt->uarch->ipc_periods[2][i];
		__entry->g3_last  	= mlt->uarch->ipc_periods[3][i];
		__entry->g4_last    	= mlt->uarch->ipc_periods[4][i];
		__entry->g5_last    	= mlt->uarch->ipc_periods[5][i];
		__entry->g6_last    	= mlt->uarch->ipc_periods[6][i];
		__entry->g7_last  	= mlt->uarch->ipc_periods[7][i];
		__entry->g8_last  	= mlt->uarch->ipc_periods[8][i];
		__entry->last_sum 	= mlt->uarch->ipc_periods[0][i] +
					  mlt->uarch->ipc_periods[1][i] +
					  mlt->uarch->ipc_periods[2][i] +
					  mlt->uarch->ipc_periods[3][i] +
					  mlt->uarch->ipc_periods[4][i] +
					  mlt->uarch->ipc_periods[5][i] +
					  mlt->uarch->ipc_periods[6][i] +
					  mlt->uarch->ipc_periods[7][i] +
					  mlt->uarch->ipc_periods[8][i];
	),

	TP_printk("cpu=%d cur_period=%d state=%d recent=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d) "
			"last=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d)",
		__entry->cpu, __entry->cur_period, __entry->state,
		__entry->g0_recent, __entry->g1_recent, __entry->g2_recent,
		__entry->g3_recent, __entry->g4_recent, __entry->g5_recent,
		__entry->g6_recent, __entry->g7_recent, __entry->g8_recent, __entry->recent_sum,
		__entry->g0_last, __entry->g1_last, __entry->g2_last,
		__entry->g3_last, __entry->g4_last, __entry->g5_last,
		__entry->g6_last, __entry->g7_last, __entry->g8_last, __entry->last_sum)
);

/*
 * Tracepoint for multi load tracking for cpu MSPC
 */
TRACE_EVENT(mlt_update_cpu_mspc,

	TP_PROTO(struct mlt *mlt),

	TP_ARGS(mlt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_period		)
		__field( int,	state			)
		__field( int,	g0_recent		)
		__field( int,	g1_recent		)
		__field( int,	g2_recent		)
		__field( int,	g3_recent		)
		__field( int,	g4_recent		)
		__field( int,	g5_recent		)
		__field( int,	g6_recent		)
		__field( int,	g7_recent		)
		__field( int,	g8_recent		)
		__field( int,	recent_sum		)
		__field( int,	g0_last			)
		__field( int,	g1_last			)
		__field( int,	g2_last			)
		__field( int,	g3_last			)
		__field( int,	g4_last			)
		__field( int,	g5_last			)
		__field( int,	g6_last			)
		__field( int,	g7_last			)
		__field( int,	g8_last			)
		__field( int,	last_sum		)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		__entry->cpu		= mlt->cpu;
		__entry->cur_period	= mlt->cur_period;
		__entry->state		= mlt->state;
		__entry->g0_recent	= mlt->uarch->mspc_recent[0];
		__entry->g1_recent	= mlt->uarch->mspc_recent[1];
		__entry->g2_recent	= mlt->uarch->mspc_recent[2];
		__entry->g3_recent  	= mlt->uarch->mspc_recent[3];
		__entry->g4_recent  	= mlt->uarch->mspc_recent[4];
		__entry->g5_recent  	= mlt->uarch->mspc_recent[5];
		__entry->g6_recent	= mlt->uarch->mspc_recent[6];
		__entry->g7_recent	= mlt->uarch->mspc_recent[7];
		__entry->g8_recent	= mlt->uarch->mspc_recent[8];
		__entry->recent_sum 	= mlt->uarch->mspc_recent[0] +
					  mlt->uarch->mspc_recent[1] +
					  mlt->uarch->mspc_recent[2] +
					  mlt->uarch->mspc_recent[3] +
					  mlt->uarch->mspc_recent[4] +
					  mlt->uarch->mspc_recent[5] +
					  mlt->uarch->mspc_recent[6] +
					  mlt->uarch->mspc_recent[7] +
					  mlt->uarch->mspc_recent[8];
		__entry->g0_last    	= mlt->uarch->mspc_periods[0][i];
		__entry->g1_last  	= mlt->uarch->mspc_periods[1][i];
		__entry->g2_last  	= mlt->uarch->mspc_periods[2][i];
		__entry->g3_last  	= mlt->uarch->mspc_periods[3][i];
		__entry->g4_last    	= mlt->uarch->mspc_periods[4][i];
		__entry->g5_last    	= mlt->uarch->mspc_periods[5][i];
		__entry->g6_last    	= mlt->uarch->mspc_periods[6][i];
		__entry->g7_last  	= mlt->uarch->mspc_periods[7][i];
		__entry->g8_last  	= mlt->uarch->mspc_periods[8][i];
		__entry->last_sum 	= mlt->uarch->mspc_periods[0][i] +
					  mlt->uarch->mspc_periods[1][i] +
					  mlt->uarch->mspc_periods[2][i] +
					  mlt->uarch->mspc_periods[3][i] +
					  mlt->uarch->mspc_periods[4][i] +
					  mlt->uarch->mspc_periods[5][i] +
					  mlt->uarch->mspc_periods[6][i] +
					  mlt->uarch->mspc_periods[7][i] +
					  mlt->uarch->mspc_periods[8][i];
	),

	TP_printk("cpu=%d cur_period=%d state=%d recent=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d) "
			"last=%d,%d,%d,%d,%d,%d,%d,%d,%d(%d)",
		__entry->cpu, __entry->cur_period, __entry->state,
		__entry->g0_recent, __entry->g1_recent, __entry->g2_recent,
		__entry->g3_recent, __entry->g4_recent, __entry->g5_recent,
		__entry->g6_recent, __entry->g7_recent, __entry->g8_recent, __entry->recent_sum,
		__entry->g0_last, __entry->g1_last, __entry->g2_last,
		__entry->g3_last, __entry->g4_last, __entry->g5_last,
		__entry->g6_last, __entry->g7_last, __entry->g8_last, __entry->last_sum)
);

/*
 * Tracepoint for multi load tracking for cstate
 */
TRACE_EVENT(mlt_update_cstate,

	TP_PROTO(struct mlt *mlt),

	TP_ARGS(mlt),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	cur_period		)
		__field( int,	state			)
		__field( int,	c0_recent		)
		__field( int,	c1_recent		)
		__field( int,	recent_sum		)
		__field( int,	c0_last			)
		__field( int,	c1_last			)
		__field( int,	last_sum		)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		__entry->cpu		= mlt->cpu;
		__entry->cur_period	= mlt->cur_period;
		__entry->state		= mlt->state;
		__entry->c0_recent  	= mlt->recent[0];
		__entry->c1_recent  	= mlt->recent[1];
		__entry->recent_sum	= mlt->recent[0] + mlt->recent[1];
		__entry->c0_last  	= mlt->periods[0][i];
		__entry->c1_last  	= mlt->periods[1][i];
		__entry->last_sum	= mlt->periods[0][i] + mlt->periods[1][i];
	),

	TP_printk("cpu=%d cur_period=%d state=%d recent=%d,%d(%d) last=%d,%d(%d)",
		__entry->cpu, __entry->cur_period, __entry->state,
		__entry->c0_recent, __entry->c1_recent, __entry->recent_sum,
		__entry->c0_last, __entry->c1_last, __entry->last_sum)
);

/*
 * Tracepoint for multi load tracking for task
 */
TRACE_EVENT(mlt_update_task,

	TP_PROTO(struct task_struct *p, struct mlt *mlt),

	TP_ARGS(p, mlt),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	cpu			)
		__field(	int,	cur_period		)
		__field(	int,	state			)
		__field(	int,	recent			)
		__field(	int,	last			)
		__field(	int,	ipc_recent		)
		__field(	int,	ipc_last		)
		__field(	int,	mspc_recent		)
		__field(	int,	mspc_last		)
	),

	TP_fast_assign(
		int i = mlt->cur_period;

		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= mlt->cpu;
		__entry->cur_period	= mlt->cur_period;
		__entry->state		= mlt->state;
		__entry->recent		= mlt->recent[0];
		__entry->last		= mlt->periods[0][i];
		__entry->ipc_recent	= mlt->uarch ? mlt->uarch->ipc_recent[0] : 0;
		__entry->ipc_last	= mlt->uarch ? mlt->uarch->ipc_periods[0][i] : 0;
		__entry->mspc_recent	= mlt->uarch ? mlt->uarch->mspc_recent[0] : 0;
		__entry->mspc_last	= mlt->uarch ? mlt->uarch->mspc_periods[0][i] : 0;
	),

	TP_printk("comm=%s pid=%d cpu=%d cur_period=%d state=%d recent=%d last=%d "
			"ipc_recent=%d ipc_last=%d mspc_recent=%d mspc_last=%d",
		__entry->comm, __entry->pid, __entry->cpu, __entry->cur_period,
		__entry->state, __entry->recent, __entry->last, __entry->ipc_recent,
		__entry->ipc_last, __entry->mspc_recent, __entry->mspc_last)
);

/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(ontime_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		src_cpu			)
		__field( int,		dst_cpu			)
		__field( int,		migrate			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d src_cpu=%d dst_cpu=%d migrate=%d reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

/*
* Tracepoint for ecs
*/
struct dynamic_dom;
TRACE_EVENT(dynamic_domain_info,

	TP_PROTO(int cpu, struct dynamic_dom *domain),

	TP_ARGS(cpu, domain),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned int,  cpus			)
		__field( int,		nr_busy_cpu		)
		__field( int,		avg_nr_run		)
		__field( int,		slower_misfit		)
		__field( int,		active_ratio		)
		__field( u64,		util			)
		__field( u64,		cap			)
		__field( int,		throttle_cnt		)
		__field( int,		flag			)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->cpus = *(unsigned int *)cpumask_bits(&domain->governor_cpus);
		__entry->avg_nr_run = domain->avg_nr_run;
		__entry->nr_busy_cpu = domain->nr_busy_cpu;
		__entry->slower_misfit = domain->slower_misfit;
		__entry->active_ratio = domain->active_ratio;
		__entry->util = domain->util;
		__entry->cap = domain->cap;
		__entry->throttle_cnt = domain->throttle_cnt;
		__entry->flag = domain->flag;
	),

	TP_printk("cpu=%d cpus=%#x avg_nr_run=%d nr_busy_cpu=%d slower_misfit=%d ar=%d util=%llu cap=%llu tcnt=%d flag=%d",
		__entry->cpu, __entry->cpus, __entry->avg_nr_run, __entry->nr_busy_cpu,
		__entry->slower_misfit, __entry->active_ratio, __entry->util,
		__entry->cap, __entry->throttle_cnt, __entry->flag)
);

/*
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update_domain,

	TP_PROTO(char *move, unsigned int id,
		unsigned int util_sum, unsigned int threshold,
		unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(move, id, util_sum, threshold, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__array(	char,		move,	64		)
		__field(	unsigned int,	id			)
		__field(	unsigned int,	util_sum		)
		__field(	unsigned int,	threshold		)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		strncpy(__entry->move, move, 63);
		__entry->id			= id;
		__entry->util_sum		= util_sum;
		__entry->threshold		= threshold;
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("move=%s id=%d util_sum=%u threshold=%u cpus=%#x->%#x",
		__entry->move, __entry->id,
		__entry->util_sum, __entry->threshold,
		__entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for updating spared cpus with fastest cpus
 */
TRACE_EVENT(ecs_reflect_fastest_cpus,

	TP_PROTO(int heavy_task_count,
		 unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(heavy_task_count, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__field(		 int,	heavy_task_count	)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		__entry->heavy_task_count	= heavy_task_count;
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("heavy_task_count=%d cpus=%#x->%#x",
		  __entry->heavy_task_count,
		  __entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for updating spared cpus
 */
TRACE_EVENT(ecs_update_governor_cpus,

	TP_PROTO(char *reason, unsigned int prev_cpus, unsigned int next_cpus),

	TP_ARGS(reason, prev_cpus, next_cpus),

	TP_STRUCT__entry(
		__array(	char,		reason,		64	)
		__field(	unsigned int,	prev_cpus		)
		__field(	unsigned int,	next_cpus		)
	),

	TP_fast_assign(
		strncpy(__entry->reason, reason, 63);
		__entry->prev_cpus		= prev_cpus;
		__entry->next_cpus		= next_cpus;
	),

	TP_printk("reason=%s cpus=%#x->%#x",
		  __entry->reason, __entry->prev_cpus, __entry->next_cpus)
);

/*
 * Tracepoint for frequency boost
 */
TRACE_EVENT(freqboost_boosted_util,

	TP_PROTO(int cpu, int ratio, unsigned long util, long margin),

	TP_ARGS(cpu, ratio, util, margin),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		ratio			)
		__field( unsigned long,	util			)
		__field( long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->ratio		= ratio;
		__entry->util		= util;
		__entry->margin		= margin;
	),

	TP_printk("cpu=%d ratio=%d util=%lu margin=%ld",
		  __entry->cpu, __entry->ratio, __entry->util, __entry->margin)
);

TRACE_EVENT(freqboost_htsk_boosted_util,

	TP_PROTO(int cpu, int hratio, int ratio, int boost,
				unsigned long util, long margin),

	TP_ARGS(cpu, hratio, ratio, boost, util, margin),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		hratio			)
		__field( int,		ratio			)
		__field( int,		boost			)
		__field( unsigned long,	util			)
		__field( long,		margin			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->hratio		= hratio;
		__entry->ratio		= ratio;
		__entry->boost		= boost;
		__entry->util		= util;
		__entry->margin		= margin;
	),

	TP_printk("cpu=%d hratio=%d ratio=%d boost=%d util=%lu margin=%ld",
		__entry->cpu, __entry->hratio, __entry->ratio,
		__entry->boost, __entry->util,  __entry->margin)
);

struct fclamp_data;

/*
 * Tracepoint for fclamp applying result
 */
TRACE_EVENT(fclamp_apply,

	TP_PROTO(int cpu, unsigned int orig_freq, unsigned int new_freq,
				int boost, struct fclamp_data *fcd),

	TP_ARGS(cpu, orig_freq, new_freq, boost, fcd),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned int,	orig_freq		)
		__field( unsigned int,	new_freq		)
		__field( int,		boost			)
		__field( int,		clamp_freq		)
		__field( int,		target_period		)
		__field( int,		target_ratio		)
		__field( int,		type			)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->orig_freq	= orig_freq;
		__entry->new_freq	= new_freq;
		__entry->boost		= boost;
		__entry->clamp_freq	= fcd->freq;
		__entry->target_period	= fcd->target_period;
		__entry->target_ratio	= fcd->target_ratio;
		__entry->type		= fcd->type;
	),

	TP_printk("cpu=%d orig_freq=%u new_freq=%u boost=%d clamp_freq=%d "
		   "target_period=%d target_ratio=%d type=%s",
		  __entry->cpu, __entry->orig_freq, __entry->new_freq, __entry->boost,
		  __entry->clamp_freq, __entry->target_period, __entry->target_ratio,
		  __entry->type ? "MAX" : "MIN")
);

/*
 * Tracepoint for checking fclamp release
 */
TRACE_EVENT(fclamp_can_release,

	TP_PROTO(int cpu, int type, int period, int period_count,
			int target_ratio, int active_ratio),

	TP_ARGS(cpu, type, period, period_count, target_ratio, active_ratio),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( int,		type			)
		__field( int,		period			)
		__field( int,		period_count		)
		__field( int,		target_ratio		)
		__field( int,		active_ratio		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->type		= type;
		__entry->period		= period;
		__entry->period_count	= period_count;
		__entry->target_ratio	= target_ratio;
		__entry->active_ratio	= active_ratio;
	),

	TP_printk("cpu=%d type=%s period=%d period_count=%d target_ratio=%d active_ratio=%d",
		  __entry->cpu, __entry->type ? "MAX" : "MIN",
		  __entry->period, __entry->period_count,
		  __entry->target_ratio, __entry->active_ratio)
);

/*
 * Tracepoint for cfs_rq load tracking:
 */
TRACE_EVENT(sched_load_cfs_rq,

	TP_PROTO(struct cfs_rq *cfs_rq),

	TP_ARGS(cfs_rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  load                    )
		__field(        unsigned long,  rbl_load                )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		struct rq *rq = __trace_get_rq(cfs_rq);

		__entry->cpu            = rq->cpu;
		__entry->load           = cfs_rq->avg.load_avg;
		__entry->rbl_load       = cfs_rq->avg.runnable_avg;
		__entry->util           = cfs_rq->avg.util_avg;
	),

	TP_printk("cpu=%d load=%lu rbl_load=%lu util=%lu",
			__entry->cpu, __entry->load,
			__entry->rbl_load,__entry->util)
);

/*
 * Tracepoint for rt_rq load tracking:
 */
TRACE_EVENT(sched_load_rt_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_rt.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

/*
 * Tracepoint for dl_rq load tracking:
 */
TRACE_EVENT(sched_load_dl_rq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_dl.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
/*
 * Tracepoint for irq load tracking:
 */
TRACE_EVENT(sched_load_irq,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field(        int,            cpu                     )
		__field(        unsigned long,  util                    )
	),

	TP_fast_assign(
		__entry->cpu    = rq->cpu;
		__entry->util   = rq->avg_irq.util_avg;
	),

	TP_printk("cpu=%d util=%lu", __entry->cpu,
		__entry->util)
);
#endif

#ifdef CREATE_TRACE_POINTS
static inline
struct rq * __trace_get_rq(struct cfs_rq *cfs_rq)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return cfs_rq->rq;
#else
	return container_of(cfs_rq, struct rq, cfs);
#endif
}
#endif /* CREATE_TRACE_POINTS */

/*
 * Tracepoint for sched_entity load tracking:
 */
TRACE_EVENT(sched_load_se,

	TP_PROTO(struct sched_entity *se),

	TP_ARGS(se),

	TP_STRUCT__entry(
		__field(        int,            cpu                           )
		__array(        char,           comm,   TASK_COMM_LEN         )
		__field(        pid_t,          pid                           )
		__field(        unsigned long,  load                          )
		__field(        unsigned long,  rbl_load                      )
		__field(        unsigned long,  util                          )
	),

	TP_fast_assign(
#ifdef CONFIG_FAIR_GROUP_SCHED
		struct cfs_rq *gcfs_rq = se->my_q;
#else
		struct cfs_rq *gcfs_rq = NULL;
#endif
		struct task_struct *p = gcfs_rq ? NULL
				: container_of(se, struct task_struct, se);
		struct rq *rq = gcfs_rq ? __trace_get_rq(gcfs_rq) : NULL;

		__entry->cpu            = rq ? rq->cpu
					: task_cpu((container_of(se, struct task_struct, se)));
		memcpy(__entry->comm, p ? p->comm : "(null)",
			p ? TASK_COMM_LEN : sizeof("(null)"));
		__entry->pid = p ? p->pid : -1;
		__entry->load = se->avg.load_avg;
		__entry->rbl_load = se->avg.runnable_avg;
		__entry->util = se->avg.util_avg;
	),

	TP_printk("cpu=%d comm=%s pid=%d load=%lu rbl_load=%lu util=%lu",
			__entry->cpu, __entry->comm, __entry->pid,
			__entry->load, __entry->rbl_load, __entry->util)
);

/*
 * Tracepoint for system overutilized flag
 */
TRACE_EVENT(sched_overutilized,

	TP_PROTO(int overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field( int,  overutilized    )
	),

	TP_fast_assign(
		__entry->overutilized   = overutilized;
	),

	TP_printk("overutilized=%d",
		__entry->overutilized)
);

TRACE_EVENT(gsc_cpu_group_load,

	TP_PROTO(int cpu, int grp_id, u64 load),

	TP_ARGS(cpu, grp_id, load),

	TP_STRUCT__entry(
		__field( int,			cpu			)
		__field( int,			grp_id		)
		__field( u64,			load		)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->grp_id			= grp_id;
		__entry->load		= load;
	),

	TP_printk("cpu=%d grp_id=%d load=%llu",
		__entry->cpu, __entry->grp_id, __entry->load)
);

TRACE_EVENT(gsc_decision_activate,

	TP_PROTO(int gid, u64 group_load, u64 now, u64 last_update_time, char *label),

	TP_ARGS(gid, group_load, now, last_update_time, label),

	TP_STRUCT__entry(
		__field( int,			gid			)
		__field( u64,			group_load		)
		__field( u64,			now			)
		__field( u64,			last_update_time	)
		__array( char,			label,	64		)
	),

	TP_fast_assign(
		__entry->gid			= gid;
		__entry->group_load		= group_load;
		__entry->now			= now;
		__entry->last_update_time	= last_update_time;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("group_id=%d group_load=%llu now=%llu last_uptime=%llu reason=%s",
		__entry->gid,  __entry->group_load, __entry->now,
		__entry->last_update_time, __entry->label)
);

/* CPUIDLE Governor */
TRACE_EVENT(halo_reflect,

	TP_PROTO(u32 cpu, u32 waker, u64 slen),

	TP_ARGS(cpu, waker, slen),

	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, waker)
		__field(u64, slen)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->waker = waker;
		__entry->slen = slen;
	),

	TP_printk("cpu=%d waker=%d slen=%lld) ",
		__entry->cpu, __entry->waker, __entry->slen)
);

TRACE_EVENT(halo_timer_fn,

	TP_PROTO(u32 cpu, u32 worked),

	TP_ARGS(cpu, worked),

	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, worked)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->worked = worked;
	),

	TP_printk("cpu=%d worked=%d", __entry->cpu, __entry->worked)
);

TRACE_EVENT(halo_tick,

	TP_PROTO(u32 cpu, s64 now),

	TP_ARGS(cpu, now),

	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(s64, now)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->now = now;
	),

	TP_printk("cpu=%d now=%d", __entry->cpu, __entry->now)
);

TRACE_EVENT(halo_fps,

	TP_PROTO(s64 vtime, s64 dtime, s64 dcnt, s64 vrr, s64 period, s64 next_ns, s64 slen_ns),

	TP_ARGS(vtime, dtime, dcnt, vrr, period, next_ns, slen_ns),

	TP_STRUCT__entry(
		__field(s64, vtime)
		__field(s64, dtime)
		__field(s64, dcnt)
		__field(s64, vrr)
		__field(s64, period)
		__field(s64, next_ns)
		__field(s64, slen_ns)
	),

	TP_fast_assign(
		__entry->vtime = vtime;
		__entry->dtime = dtime;
		__entry->dcnt = dcnt;
		__entry->vrr = vrr;
		__entry->period = period;
		__entry->next_ns = next_ns;
		__entry->slen_ns = slen_ns;
	),

	TP_printk("vtime=%lld dtime=%lld dcnt=%lld vrr=%lld period=%lld nxt_ns=%lld, slen_ns=%lld",
		 __entry->vtime, __entry->dtime, __entry->dcnt,
		__entry->vrr, __entry->period, __entry->next_ns, __entry->slen_ns)
);


TRACE_EVENT(halo_ipi,

	TP_PROTO(s32 cpu, s32 heavy, s64 min, s64 max, s64 cur, s64 avg, s64 correct, s64 pred),

	TP_ARGS(cpu, heavy, min, max, cur, avg, correct, pred),

	TP_STRUCT__entry(
		__field(s32, cpu)
		__field(s32, heavy)
		__field(s64, min)
		__field(s64, max)
		__field(s64, cur)
		__field(s64, avg)
		__field(s64, correct)
		__field(s64, pred)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->heavy = heavy;
		__entry->min = min;
		__entry->max = max;
		__entry->cur = cur;
		__entry->avg = avg;
		__entry->correct = correct;
		__entry->pred = pred;
	),

	TP_printk("cpu=%d heavy=%d min=%lld max=%lld cur=%lld avg=%lld corrrect=%lld pred=%lld",
		  __entry->cpu, __entry->heavy, __entry->min, __entry->max, __entry->cur,
		__entry->avg, __entry->correct, __entry->pred)
);

TRACE_EVENT(halo_hstate,

	TP_PROTO(s32 cpu, s32 idx, u64 hit, u64 miss),

	TP_ARGS(cpu, idx, hit, miss),

	TP_STRUCT__entry(
		__field(s32, cpu)
		__field(s32, idx)
		__field(u64, hit)
		__field(u64, miss)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->idx = idx;
		__entry->hit = hit;
		__entry->miss = miss;
	),

	TP_printk("cpu=%d idx=%d hit=%llu miss=%llu",
		  __entry->cpu, __entry->idx, __entry->hit, __entry->miss)
);

/*
 * Tracepoint for lb cpu util
 */
TRACE_EVENT(lb_cpu_util,

	TP_PROTO(int cpu, char *label),

	TP_ARGS(cpu, label),

	TP_STRUCT__entry(
		__field( int,           cpu                     )
		__field( int,           active_balance          )
		__field( int,           idle            )
		__field( int,           nr_running                      )
		__field( int,           cfs_nr_running          )
		__field( int,           nr_misfits              )
		__field( unsigned long,         cpu_util                )
		__field( unsigned long,         capacity_orig           )
		__array( char,          label,  64              )
		),

	TP_fast_assign(
		__entry->cpu                    = cpu;
		__entry->active_balance         = cpu_rq(cpu)->active_balance;
		__entry->idle           = available_idle_cpu(cpu);
		__entry->nr_running             = cpu_rq(cpu)->nr_running;
		__entry->cfs_nr_running         = cpu_rq(cpu)->cfs.h_nr_running;
		__entry->nr_misfits             = ems_rq_nr_misfited(cpu_rq(cpu));
		__entry->cpu_util               = ml_cpu_util(cpu);
		__entry->capacity_orig          = capacity_orig_of(cpu);
		strncpy(__entry->label, label, 63);
		),

	TP_printk("cpu=%d ab=%d idle=%d nr_running=%d cfs_nr_running=%d nr_misfits=%d cpu_util=%lu capacity=%lu reason=%s",
			__entry->cpu, __entry->active_balance, __entry->idle, __entry->nr_running,
			__entry->cfs_nr_running, __entry->nr_misfits, __entry->cpu_util, __entry->capacity_orig, __entry->label)
);


/*
 * Tracepoint for task migration control
 */
TRACE_EVENT(lb_can_migrate_task,

	TP_PROTO(struct task_struct *tsk, int dst_cpu, int migrate, char *label),

	TP_ARGS(tsk, dst_cpu, migrate, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( int,		src_cpu			)
		__field( int,		dst_cpu			)
		__field( int,		migrate			)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->src_cpu		= task_cpu(tsk);
		__entry->dst_cpu		= dst_cpu;
		__entry->migrate		= migrate;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d src_cpu=%d dst_cpu=%d migrate=%d reason=%s",
		__entry->comm, __entry->pid, __entry->src_cpu, __entry->dst_cpu,
		__entry->migrate, __entry->label)
);

 /*
 * Tracepoint for misfit
 */
TRACE_EVENT(lb_update_misfit,
	TP_PROTO(struct task_struct *p, bool old, bool cur, int cpu, u64 rq_nr_misfit, char *label),

	TP_ARGS(p, old, cur, cpu, rq_nr_misfit, label),

	TP_STRUCT__entry(
		__array( char,          comm,   TASK_COMM_LEN   )
		__field( pid_t,         pid                     )
		__field( unsigned long, util			)
		__field( bool,          old             )
		__field( bool,          cur             )
		__field( int,           cpu             )
		__field( u64,           rq_nr_misfit    )
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid = p->pid;
		__entry->util = ml_uclamp_task_util(p);
		__entry->old = old;
		__entry->cur = cur;
		__entry->cpu = cpu;
		__entry->rq_nr_misfit = rq_nr_misfit;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d util=%ld old=%d cur=%d cpu=%d rq_nr_misfit=%llu, tag=%s",
			__entry->comm, __entry->pid, __entry->util, __entry->old,
			__entry->cur, __entry->cpu, __entry->rq_nr_misfit, __entry->label)
);

/*
 * Tracepoint for nohz balancer kick
 */
TRACE_EVENT(lb_nohz_balancer_kick,

	TP_PROTO(struct rq *rq),

	TP_ARGS(rq),

	TP_STRUCT__entry(
		__field( int,		cpu			)
		__field( unsigned int,	nr_running			)
		__field( unsigned int,	cfs_nr_running		)
	),

	TP_fast_assign(
		__entry->cpu = rq->cpu;
		__entry->nr_running = rq->nr_running;
		__entry->cfs_nr_running = rq->cfs.h_nr_running;
	),

	TP_printk("cpu=%d nr_running=%u cfs_nr_running=%u",
		__entry->cpu, __entry->nr_running, __entry->cfs_nr_running)
);

/*
 * Tracepoint for lb idle pull tasks rt
 */
TRACE_EVENT(lb_idle_pull_tasks_rt,

	TP_PROTO(int src_cpu, struct task_struct *p, int pulled),

	TP_ARGS(src_cpu, p, pulled),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		src_cpu			)
		__field(	int,		pulled			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p ? p->comm : "no task", TASK_COMM_LEN);
		__entry->pid			= p ? p->pid : -1;
		__entry->src_cpu		= src_cpu;
		__entry->pulled			= pulled;
	),

	TP_printk("src_cpu=%d pulled=%d comm=%s pid=%d",
		__entry->src_cpu, __entry->pulled, __entry->comm, __entry->pid)
);

/*
 * Tracepoint for mhdvfs profile
 */
TRACE_EVENT(mhdvfs_profile,

	TP_PROTO(u64 ipc_sum, u64 mspc_sum),

	TP_ARGS(ipc_sum, mspc_sum),

	TP_STRUCT__entry(
		__field(	u64,	ipc_sum		)
		__field(	u64,	mspc_sum	)
	),

	TP_fast_assign(
		__entry->ipc_sum	= ipc_sum;
		__entry->mspc_sum	= mspc_sum;
	),

	TP_printk("ipc_sum=%llu mspc_sum=%llu",
		  __entry->ipc_sum, __entry->mspc_sum)
);

/*
 * Tracepoint for kicking dsufreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_dsufreq,

	TP_PROTO(u64 mspc, int ratio),

	TP_ARGS(mspc, ratio),

	TP_STRUCT__entry(
		__field(	u64,	mspc		)
		__field(	int,	ratio		)
	),

	TP_fast_assign(
		__entry->mspc		= mspc;
		__entry->ratio		= ratio;
	),

	TP_printk("mspc=%llu ratio=%d", __entry->mspc, __entry->ratio)
);

/*
 * Tracepoint for kicking miffreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_miffreq,

	TP_PROTO(u64 mspc, u32 ratio, u64 duration_ms),

	TP_ARGS(mspc, ratio, duration_ms),

	TP_STRUCT__entry(
		__field(	u64,	mspc		)
		__field(	u32,	ratio		)
		__field(	u64,	duration_ms	)
	),

	TP_fast_assign(
		__entry->mspc		= mspc;
		__entry->ratio		= ratio;
		__entry->duration_ms	= duration_ms;
	),

	TP_printk("mspc=%llu ratio=%u duration=%llu",
		__entry->mspc, __entry->ratio, __entry->duration_ms)
);

/*
 * Tracepoint for kicking cpufreq by mhdvfs
 */
TRACE_EVENT(mhdvfs_kick_cpufreq,

	TP_PROTO(int cpu, u64 ipc, int ratio),

	TP_ARGS(cpu, ipc, ratio),

	TP_STRUCT__entry(
		__field(	int,	cpu		)
		__field(	u64,	ipc		)
		__field(	int,	ratio		)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->ipc		= ipc;
		__entry->ratio		= ratio;
	),

	TP_printk("cpu=%d ipc=%llu ratio=%d",
		__entry->cpu, __entry->ipc, __entry->ratio)
);
#endif /* _TRACE_EMS_DEBUG_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
