/*
 *  Copyright (C) 2017 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ems

#if !defined(_TRACE_EMS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EMS_H

#include <linux/tracepoint.h>

struct tp_env;

/*
 * Tracepoint for finding cpus allowed
 */
TRACE_EVENT(ems_find_cpus_allowed,

	TP_PROTO(struct tp_env *env, struct cpumask *mask),

	TP_ARGS(env, mask),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	bool,		isa_32			)
		__field(	bool,		per_cpu_kthread		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	active_mask		)
		__field(	unsigned int,	ecs_cpus_allowed	)
		__field(	unsigned int,	cpus_ptr		)
		__field(	unsigned int,	cpus_binding_mask	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->isa_32			= test_ti_thread_flag(&env->p->thread_info, TIF_32BIT);
		__entry->per_cpu_kthread	= env->per_cpu_kthread;
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->active_mask		= *(unsigned int *)cpumask_bits(&mask[0]);
		__entry->ecs_cpus_allowed	= *(unsigned int *)cpumask_bits(&mask[1]);
		__entry->cpus_ptr		= *(unsigned int *)cpumask_bits(&mask[2]);
		__entry->cpus_binding_mask	= *(unsigned int *)cpumask_bits(&mask[3]);
	),

	TP_printk("comm=%s pid=%d isa_32=%d per_cpu_kthread=%d cpus_allowed=%#x active_mask=%#x "
		  "ecs_cpus_allowed=%#x cpus_ptr=%#x cpus_binding_mask=%#x",
		  __entry->comm, __entry->pid, __entry->isa_32, __entry->per_cpu_kthread,
		  __entry->cpus_allowed, __entry->active_mask, __entry->ecs_cpus_allowed,
		  __entry->cpus_ptr, __entry->cpus_binding_mask)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_find_fit_cpus,

	TP_PROTO(struct tp_env *env),

	TP_ARGS(env),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		prev_cpu		)
		__field(	int,		cl_sync			)
		__field(	int,		sync			)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	task_util_clamped	)
		__field(	int,		task_class		)
		__field(	int,		sched_policy		)
		__field(	int,		init_index		)
		__field(	unsigned int,	cpus_allowed		)
		__field(	unsigned int,	fit_cpus		)
		__field(	unsigned int,	overcap_cpus		)
		__field(	unsigned int,	migrating_cpus		)

	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid			= env->p->pid;
		__entry->prev_cpu		= task_cpu(env->p);
		__entry->cl_sync		= env->cl_sync;
		__entry->sync			= env->sync;
		__entry->task_util		= env->task_util;
		__entry->task_util_clamped	= env->task_util_clamped;
		__entry->task_class		= env->task_class;
		__entry->sched_policy		= env->sched_policy;
		__entry->init_index		= env->init_index;
		__entry->cpus_allowed		= *(unsigned int *)cpumask_bits(&env->cpus_allowed);
		__entry->fit_cpus		= *(unsigned int *)cpumask_bits(&env->fit_cpus);
		__entry->overcap_cpus		= *(unsigned int *)cpumask_bits(&env->overcap_cpus);
		__entry->migrating_cpus		= *(unsigned int *)cpumask_bits(&env->migrating_cpus);
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d cl_sync=%d sync=%d "
		  "task_util=%lu task_util_clamped=%lu "
		  "task_class=%d, sched_policy=%d, init_index=%d "
		  "fit_cpus=%#x cpus_allowed=%#x overcap_cpus=%#x "
		  "migrating_cpus=%#x",
		  __entry->comm, __entry->pid, __entry->prev_cpu, __entry->cl_sync, __entry->sync,
		  __entry->task_util, __entry->task_util_clamped,
		  __entry->task_class, __entry->sched_policy, __entry->init_index,
		  __entry->fit_cpus, __entry->cpus_allowed, __entry->overcap_cpus, __entry->migrating_cpus)
);

/*
 * Tracepoint for wakeup balance
 */
TRACE_EVENT(ems_select_task_rq,

	TP_PROTO(struct tp_env *env, int target_cpu, char *state),

	TP_ARGS(env, target_cpu, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cgroup_idx		)
		__field(	int,		target_cpu		)
		__field(	int,		wakeup			)
		__field(	u64,		task_util		)
		__field(	u32,		nr_running		)
		__array(	char,		state,		30	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, env->p->comm, TASK_COMM_LEN);
		__entry->pid		= env->p->pid;
		__entry->cgroup_idx	= env->cgroup_idx;
		__entry->wakeup		= !env->p->on_rq;
		__entry->task_util	= env->task_util;
		__entry->target_cpu	= target_cpu;
		__entry->nr_running	= cpu_rq(target_cpu)->nr_running;
		memcpy(__entry->state, state, 30);
	),

	TP_printk("comm=%s pid=%d cgrp_idx=%d wakeup=%d util=%lld target_cpu=%d nr_run=%u state=%s",
		  __entry->comm, __entry->pid, __entry->cgroup_idx,
		  __entry->wakeup, __entry->task_util,
		 __entry->target_cpu, __entry->nr_running, __entry->state)
);

/*
 * Tracepoint for the EGO
 */
TRACE_EVENT(ego_req_freq,

	TP_PROTO(int cpu, unsigned int target_freq, unsigned int min_freq,
		unsigned int max_freq, unsigned int org_freq, unsigned int eng_freq,
		unsigned long util, unsigned long max),

	TP_ARGS(cpu, target_freq, min_freq, max_freq,
		org_freq, eng_freq, util, max),

	TP_STRUCT__entry(
		__field( int,		cpu				)
		__field( unsigned int,	target_freq			)
		__field( unsigned int,	min_freq			)
		__field( unsigned int,	max_freq			)
		__field( unsigned int,	org_freq			)
		__field( unsigned int,	eng_freq			)
		__field( unsigned long,	util				)
		__field( unsigned long,	max				)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->target_freq		= target_freq;
		__entry->min_freq		= min_freq;
		__entry->max_freq		= max_freq;
		__entry->org_freq		= org_freq;
		__entry->eng_freq		= eng_freq;
		__entry->util			= util;
		__entry->max			= max;
	),

	TP_printk("cpu=%d target_f=%d min_f=%d max_f=%d org_f=%d eng_f=%d util=%ld max=%ld",
		__entry->cpu, __entry->target_freq, __entry->min_freq,
		__entry->max_freq, __entry->org_freq, __entry->eng_freq,
		__entry->util, __entry->max)
);

/*
 * Tracepoint for the STT
 */
TRACE_EVENT(stt_tracking_htsk,

	TP_PROTO(int cpu, int pid, int hcnt, u64 expired, char *tag),

	TP_ARGS(cpu, pid, hcnt, expired, tag),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( int,	pid			)
		__field( int,	hcnt			)
		__field( u64,	expired			)
		__array( char,	tag,		30	)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->pid			= pid;
		__entry->hcnt			= hcnt;
		__entry->expired		= expired;
		memcpy(__entry->tag, tag, 30);
	),

	TP_printk("cpu=%d pid=%d hcnt=%d expired=%lu, tag=%s",
		__entry->cpu, __entry->pid, __entry->hcnt,
		__entry->expired, __entry->tag)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
				int src_cpu, int dst_cpu, char *reason),

	TP_ARGS(p, load, src_cpu, dst_cpu, reason),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__array(	char,		reason,		16	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		memcpy(__entry->reason, reason, 16);
	),

	TP_printk("comm=%s pid=%d ontime_load_avg=%lu src_cpu=%d dst_cpu=%d reason=%s",
		__entry->comm, __entry->pid, __entry->load,
		__entry->src_cpu, __entry->dst_cpu, __entry->reason)
);

/*
 * Tracepoint for emstune mode
 */
TRACE_EVENT(emstune_mode,

	TP_PROTO(int next_mode, int next_level),

	TP_ARGS(next_mode, next_level),

	TP_STRUCT__entry(
		__field( int,		next_mode		)
		__field( int,		next_level		)
	),

	TP_fast_assign(
		__entry->next_mode		= next_mode;
		__entry->next_level		= next_level;
	),

	TP_printk("mode=%d level=%d", __entry->next_mode, __entry->next_level)
);

/*
 * Tracepoint for logging FRT schedule activity
 */
TRACE_EVENT(frt_select_task_rq,

	TP_PROTO(struct task_struct *tsk, int best, char* str),

	TP_ARGS(tsk, best, str),

	TP_STRUCT__entry(
		__array( char,	selectby,	TASK_COMM_LEN	)
		__array( char,	targettsk,	TASK_COMM_LEN	)
		__field( pid_t,	pid				)
		__field( int,	bestcpu				)
		__field( int,	prevcpu				)
	),

	TP_fast_assign(
		memcpy(__entry->selectby, str, TASK_COMM_LEN);
		memcpy(__entry->targettsk, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->bestcpu		= best;
		__entry->prevcpu		= task_cpu(tsk);
	),

	TP_printk("frt: comm=%s pid=%d assigned to #%d from #%d by %s.",
		  __entry->targettsk,
		  __entry->pid,
		  __entry->bestcpu,
		  __entry->prevcpu,
		  __entry->selectby)
);

/*
 * Tracepoint for system busy state change
 */
TRACE_EVENT(sysbusy_state,

	TP_PROTO(int old_state, int next_state),

	TP_ARGS(old_state, next_state),

	TP_STRUCT__entry(
		__field(int,	old_state)
		__field(int,	next_state)
	),

	TP_fast_assign(
		__entry->old_state	= old_state;
		__entry->next_state	= next_state;
	),

	TP_printk("old_state=%d next_state=%d",
		__entry->old_state, __entry->next_state)
);

/* CPUIDLE Governor */
TRACE_EVENT(halo_select,

	TP_PROTO(u32 waker, u32 selector, s64 resi, s64 lat, s64 tick, s64 timer, s64 vsync, s64 det, s64 pred, s64 expired, u32 idx),

	TP_ARGS(waker, selector, resi, lat, tick, timer, vsync, det, pred, expired, idx),

	TP_STRUCT__entry(
		__field(u32, waker)
		__field(u32, selector)
		__field(s64, resi)
		__field(s64, lat)
		__field(s64, tick)
		__field(s64, timer)
		__field(s64, vsync)
		__field(s64, det)
		__field(s64, pred)
		__field(s64, expired)
		__field(u32, idx)
	),

	TP_fast_assign(
		__entry->waker = waker;
		__entry->selector = selector;
		__entry->resi = resi;
		__entry->lat = lat;
		__entry->tick = tick;
		__entry->timer = timer;
		__entry->vsync = vsync;
		__entry->det = det;
		__entry->pred = pred;
		__entry->expired = expired;
		__entry->idx = idx;
	),

	TP_printk("waker=%d sel=%d resi=%lld lat=%lld tick=%lld timer=%lld vsync=%lld det=%lld pred=%lld expired=%lld idx=%u",
		 __entry->waker,  __entry->selector, __entry->resi, __entry->lat, __entry->tick, __entry->timer,
		__entry->vsync, __entry->det, __entry->pred, __entry->expired, __entry->idx)
);

/*
 * Tracepoint for active load balance
 */
TRACE_EVENT(lb_active_migration,

	TP_PROTO(struct task_struct *p, int prev_cpu, int new_cpu, char *label),

	TP_ARGS(p, prev_cpu, new_cpu, label),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
		__field( pid_t,		pid			)
		__field( u64,		misfit			)
		__field( u64,		util			)
		__field( int,		prev_cpu			)
		__field( int,		new_cpu		)
		__array( char,		label,	64		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->misfit			= ems_task_misfited(p);
		__entry->util			= ml_uclamp_task_util(p);
		__entry->prev_cpu		= prev_cpu;
		__entry->new_cpu		= new_cpu;
		strncpy(__entry->label, label, 63);
	),

	TP_printk("comm=%s pid=%d misfit=%llu util=%llu prev_cpu=%d new_cpu=%d reason=%s",
		__entry->comm, __entry->pid, __entry->misfit, __entry->util,
		 __entry->prev_cpu, __entry->new_cpu, __entry->label)
);

/*
 * Tracepoint for find busiest queue
 */
TRACE_EVENT(lb_find_busiest_queue,

	TP_PROTO(int dst_cpu, int busiest_cpu, struct cpumask *src_cpus),

	TP_ARGS(dst_cpu, busiest_cpu, src_cpus),

	TP_STRUCT__entry(
		__field( int,		dst_cpu			)
		__field( int,		busiest_cpu		)
		__field( unsigned int, src_cpus)
	),

	TP_fast_assign(
		__entry->dst_cpu = dst_cpu;
		__entry->busiest_cpu = busiest_cpu;
		__entry->src_cpus = *(unsigned int *)cpumask_bits(src_cpus);
	),

	TP_printk("dst_cpu=%d busiest_cpu=%d src_cpus=%#x",
		__entry->dst_cpu, __entry->busiest_cpu, __entry->src_cpus)
);

/*
 * Tracepoint for ems newidle balance
 */
TRACE_EVENT(lb_newidle_balance,

	TP_PROTO(int this_cpu, int busy_cpu, int pulled, int range, bool short_idle),

	TP_ARGS(this_cpu, busy_cpu, pulled, range, short_idle),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, busy_cpu)
		__field(int, pulled)
		__field(unsigned int, nr_running)
		__field(unsigned int, rt_nr_running)
		__field(int, nr_iowait)
		__field(int, range)
		__field(u64, avg_idle)
		__field(bool, short_idle)
		__field(int, overload)
	),

	TP_fast_assign(
		__entry->cpu        = this_cpu;
		__entry->busy_cpu   = busy_cpu;
		__entry->pulled     = pulled;
		__entry->nr_running = cpu_rq(this_cpu)->nr_running;
		__entry->rt_nr_running = cpu_rq(this_cpu)->rt.rt_nr_running;
		__entry->nr_iowait  = atomic_read(&(cpu_rq(this_cpu)->nr_iowait));
		__entry->range	    = range;
		__entry->avg_idle   = cpu_rq(this_cpu)->avg_idle;
		__entry->short_idle = short_idle;
		__entry->overload   = cpu_rq(this_cpu)->rd->overload;
	),

	TP_printk("cpu=%d busy_cpu=%d pulled=%d nr_run=%u rt_nr_run=%u nr_iowait=%d range=%d avg_idle=%llu short_idle=%d overload=%d",
		__entry->cpu, __entry->busy_cpu, __entry->pulled,
		__entry->nr_running, __entry->rt_nr_running,
		__entry->nr_iowait, __entry->range,
		__entry->avg_idle, __entry->short_idle,
		__entry->overload)
);


TRACE_EVENT(mlt_nr_run_update,

	TP_PROTO(int cpu, u64 delta, u64 cntr, int nr_run, int avg_nr_run),

	TP_ARGS(cpu, delta, cntr, nr_run, avg_nr_run),

	TP_STRUCT__entry(
		__field( int,	cpu			)
		__field( u64,	delta			)
		__field( u64,	cntr			)
		__field( int,	nr_run			)
		__field( int,	avg_nr_run		)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->delta = delta;
		__entry->cntr = cntr;
		__entry->nr_run = nr_run;
		__entry->avg_nr_run = avg_nr_run;
	),

	TP_printk("cpu=%d delta=%llu cntr=%lld nr_run=%d avg_nr_run=%d",
		__entry->cpu, __entry->delta, __entry->cntr,
		__entry->nr_run, __entry->avg_nr_run)
);

#endif /* _TRACE_EMS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
