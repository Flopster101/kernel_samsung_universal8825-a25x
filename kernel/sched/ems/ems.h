/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ems.h>

#include "../../../drivers/android/binder_internal.h"

/* 5.15 ems porting to 5.10 */
#define raw_spin_rq_lock_irqsave(A, B) \
	raw_spin_lock_irqsave(&A->lock, B)

#define raw_spin_rq_unlock_irqrestore(A, B) \
	raw_spin_unlock_irqrestore(&A->lock, B)

#define raw_spin_rq_lock(A) \
	raw_spin_lock(&A->lock)

#define raw_spin_rq_unlock(A) \
	raw_spin_unlock(&A->lock)

enum task_cgroup {
	CGROUP_ROOT,
	CGROUP_FOREGROUND,
	CGROUP_BACKGROUND,
	CGROUP_TOPAPP,
	CGROUP_RT,
	CGROUP_SYSTEM,
	CGROUP_SYSTEM_BACKGROUND,
	CGROUP_NNAPI_HAL,
	CGROUP_CAMERA_DAEMON,
	CGROUP_MIDGROUND,
	CGROUP_COUNT,
};

/*
 * Maximum supported-by-vendor processors.  Setting this smaller saves quite a
 * bit of memory.  Use nr_cpu_ids instead of this except for static bitmaps.
 */
#ifndef CONFIG_VENDOR_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_VENDOR_NR_CPUS	1
#endif

/* Places which use this should consider cpumask_var_t. */
#define VENDOR_NR_CPUS		CONFIG_VENDOR_NR_CPUS

/* maximum count of tracking tasks in runqueue */
#define TRACK_TASK_COUNT	10

#ifndef WF_ANDROID_VENDOR
#define WF_ANDROID_VENDOR	0
#endif

/*
 * Android vendor data in use
 *
 * rq
 *  - index 0, 1 of android_vendor_data1 are used by TEX queue jump
 *  - index    6 of android_vendor_data1 are used by NR_MISFIT
 *  - index    7 of android_vendor_data1 are used by MIGRATED
 *  - index    8 of android_vendor_data1 are used by CLUSTER_INDEX
 *
 * task
 *  - index 1     of android_vendor_data1  is used by fallback rq
 *  - index 2     of android_vendor_data1  is used by process flag
 *  - index 3~5   of android_vendor_data1 are used by TEX queue jump
 *  - index 6~45  of android_vendor_data1 are used by MLT
 *  - index 46    of android_vendor_data1  is used by MISFIT
 */
#define RQ_AVD1_0(rq)			(rq->android_vendor_data1[0])
#define RQ_AVD1_1(rq)			(rq->android_vendor_data1[1])
#define RQ_AVD1_5(rq)			(rq->android_vendor_data1[5])
#define RQ_AVD1_6(rq)			(rq->android_vendor_data1[6])
#define RQ_AVD1_7(rq)			(rq->android_vendor_data1[7])
#define RQ_AVD1_8(rq)			(rq->android_vendor_data1[8])
#define RQ_AVD1_9(rq)			(rq->android_vendor_data1[9])
#define TASK_AVD1_1(task)		(task->android_vendor_data1[1])
#define TASK_AVD1_2(task)		(task->android_vendor_data1[2])
#define TASK_AVD1_3(task)		(task->android_vendor_data1[3])
#define TASK_AVD1_4(task)		(task->android_vendor_data1[4])
#define TASK_AVD1_5(task)		(task->android_vendor_data1[5])
#define TASK_AVD1_6(task)		(task->android_vendor_data1[6])
#define TASK_AVD1_46(task)		(task->android_vendor_data1[46])
#define TASK_AVD1_50(task)		(task->android_vendor_data1[50])
#define TASK_AVD1_51(task)		(task->android_vendor_data1[51])
#define TASK_AVD1_52(task)		(task->android_vendor_data1[52])
#define TASK_AVD1_53(task)		(task->android_vendor_data1[53])
#define TASK_AVD1_54(task)		(task->android_vendor_data1[54])
#define TASK_AVD1_55(task)		(task->android_vendor_data1[55])
#define TASK_AVD1_56(task)		(task->android_vendor_data1[56])
#define TASK_AVD1_57(task)		(task->android_vendor_data1[57])

/* support flag-handling for EMS */
#define EMS_PF_GET(task)		TASK_AVD1_2(task)
#define EMS_PF_SET(task, value)		(TASK_AVD1_2(task) |= (value))
#define EMS_PF_CLEAR(task, value)	(TASK_AVD1_2(task) &= ~(value))

#define EMS_PF_MULLIGAN			0x00000001	/* I'm given a mulligan */
#define EMS_PF_RUNNABLE_BUSY		0x00000002	/* Picked from runnable busy cpu */

/*
 * Vendor data handling for TEX queue jump
 *
 * rq  's android_vendor_data1 indices 0, 1    are used
 * task's android_vendor_data1 indices 3, 4, 5 are used
 */
#define ems_qjump_list(rq)		((struct list_head *)&(RQ_AVD1_0(rq)))
#define ems_qjump_node(task)		((struct list_head *)&(TASK_AVD1_3(task)))
#define ems_tex_level(task)		(TASK_AVD1_5(task))
#define ems_tex_last_update(task)	(TASK_AVD1_50(task))
#define ems_tex_runtime(task)		(TASK_AVD1_51(task))
#define ems_tex_chances(task)		(TASK_AVD1_52(task))
#define ems_boosted_tex(task)		(TASK_AVD1_53(task))
#define ems_binder_task(task)		(TASK_AVD1_54(task))
#define ems_render(task)		(TASK_AVD1_56(task))
#define ems_somac_dequeued(rq)		(RQ_AVD1_5(rq))
#define ems_task_misfited(task)		(TASK_AVD1_46(task))
#define ems_rq_nr_misfited(rq)		(RQ_AVD1_6(rq))
#define ems_rq_migrated(rq)		(RQ_AVD1_7(rq))
#define ems_rq_cluster_idx(rq)		(RQ_AVD1_8(rq))
#define ems_prio_tex(task)			(TASK_AVD1_55(task))
#define ems_rq_nr_prio_tex(rq)		(RQ_AVD1_9(rq))
#define ems_last_waked(task)		(TASK_AVD1_57(task))

#define ems_qjump_list_entry(list)	({						\
	void *__mptr = (void *)(list);						\
	(struct task_struct *)(__mptr -							\
				offsetof(struct task_struct, android_vendor_data1[3]));	\
})

#define ems_qjump_first_entry(list)	({						\
	void *__mptr = (void *)(list->next);						\
	(struct task_struct *)(__mptr -							\
				offsetof(struct task_struct, android_vendor_data1[3]));	\
})

/* structure for task placement environment */
struct tp_env {
	struct task_struct *p;
	int cgroup_idx;
	int per_cpu_kthread;

	int sched_policy;
	int cl_sync;
	int sync;
	int prev_cpu;

	int task_class;
	int init_index;
	int end_index;
	unsigned long base_cap;

	int reason_of_selection;

	struct cpumask cpus_allowed;
	struct cpumask fit_cpus;
	struct cpumask overcap_cpus;
	struct cpumask migrating_cpus;

	unsigned long weight[VENDOR_NR_CPUS];	/* efficiency weight */

	unsigned long task_util;
	unsigned long task_util_clamped;
	unsigned long task_load_avg;
	struct {
		unsigned long util;
		unsigned long util_wo;
		unsigned long util_with;
		unsigned long runnable;
		unsigned long load_avg;
		unsigned long rt_util;
		unsigned long dl_util;
		unsigned long nr_running;
		unsigned long target_util_wo;
		unsigned long target_util_with;
		int idle;
	} cpu_stat[VENDOR_NR_CPUS];
};

/* cpu selection domain */
struct cs_domain {
	struct list_head list;
	struct cpumask cpus;
};

/* core */
enum {
	NON_IDLE = 0,
	IDLE_C1,
	IDLE_C2,
};

extern int ems_select_task_rq_fair(struct task_struct *p,
		int prev_cpu, int sd_flag, int wake_flag);
extern int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag);
extern int ems_select_task_rq_rt(struct task_struct *p,
		int prev_cpu, int sd_flag, int wake_flag);
extern int ems_select_fallback_rq(struct task_struct *p, int target_cpu);
extern void ems_tick_entry(struct rq *rq);
extern void ems_tick(struct rq *rq);
extern void ems_enqueue_task(struct rq *rq, struct task_struct *p, int flags);
extern void ems_dequeue_task(struct rq *rq, struct task_struct *p, int flags);
extern void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev);
extern void ems_cpu_cgroup_can_attach(struct cgroup_taskset *tset, int can_attach);
extern int ems_load_balance(struct rq *rq);
extern void ems_post_init_entity_util_avg(struct sched_entity *se);
extern int ems_find_new_ilb(struct cpumask *nohz_idle_cpus_mask);
extern void ems_sched_fork_init(struct task_struct *p);
extern void ems_idle_enter(int cpu, int *state);
extern void ems_idle_exit(int cpu, int state);
extern void ems_schedule(struct task_struct *prev, struct task_struct *next, struct rq *rq);
extern void ems_set_task_cpu(struct task_struct *p, unsigned int new_cpu);
extern void ems_check_preempt_wakeup(struct rq *rq, struct task_struct *p,
		bool *preempt, bool *ignore);
extern void ems_do_sched_yield(struct rq *rq);

extern int core_init(struct device_node *ems_dn);
extern int hook_init(void);

int find_cpus_allowed(struct tp_env *env);

/* List of process element */
struct pe_list {
	struct cpumask *cpus;
	int num_of_cpus;
};

extern struct pe_list *get_pe_list(int index);
extern int get_pe_list_size(void);

/* cpus binding */
const struct cpumask *cpus_binding_mask(struct task_struct *p);

/* energy table */
struct energy_state {
	unsigned long capacity;
	unsigned long util;
	unsigned long weight;
	unsigned long frequency;
	unsigned long voltage;
	unsigned long dynamic_power;
	unsigned long static_power;
	int idle;
	int temperature;
};

struct energy_backup {
	unsigned long frequency;
	unsigned long voltage;
	unsigned long energy;
};

extern int et_init(struct kobject *ems_kobj);
extern void et_init_table(struct cpufreq_policy *policy);
extern unsigned int et_cur_freq_idx(int cpu);
extern unsigned long et_cur_cap(int cpu);
extern unsigned long et_max_cap(int cpu);
extern unsigned long et_max_dpower(int cpu);
extern unsigned long et_min_dpower(int cpu);
extern unsigned long et_freq_to_cap(int cpu, unsigned long freq);
extern unsigned long et_freq_to_dpower(int cpu, unsigned long freq);
extern unsigned long et_freq_to_spower(int cpu, unsigned long freq);
extern unsigned long et_dpower_to_cap(int cpu, unsigned long power);
extern void et_update_freq(int cpu, unsigned long freq);
extern void et_get_orig_state(int cpu, unsigned long freq, struct energy_state *state);
extern void et_fill_energy_state(struct tp_env *env, struct cpumask *cpus,
		struct energy_state *states, unsigned long capacity, int dsu_cpu);
extern unsigned long et_compute_cpu_energy(const struct cpumask *cpus,
		struct energy_state *states);
extern unsigned long et_compute_system_energy(const struct list_head *csd_head,
		struct energy_state *states, int target_cpu, struct energy_backup *backup);
extern void et_arch_set_freq_scale(const struct cpumask *cpus, unsigned long freq,
					unsigned long max, unsigned long *scale);

/* multi load */
extern struct mlt __percpu *pcpu_mlt;		/* active ratio tracking */

#define NR_RUN_UNIT		100
#define NR_RUN_ROUNDS_UNIT	50
#define NR_RUN_UP_UNIT 99
extern int mlt_avg_nr_run(struct rq *rq);
extern void mlt_enqueue_task(struct rq *rq);
extern void mlt_dequeue_task(struct rq *rq);
extern void mlt_update_task(struct task_struct *p, int next_state, u64 now);
extern void mlt_idle_enter(int cpu, int cstate);
extern void mlt_idle_exit(int cpu);
extern void mlt_tick(struct rq *rq);
extern void mlt_task_switch(int cpu, struct task_struct *prev, struct task_struct *next);
extern void mlt_task_migration(struct task_struct *p, int new_cpu);
extern int mlt_cur_period(int cpu);
extern int mlt_prev_period(int period);
extern int mlt_period_with_delta(int idx, int delta);
extern int mlt_art_value(int cpu, int idx);
extern int mlt_art_recent(int cpu);
extern int mlt_art_last_value(int cpu);
extern int mlt_art_cgroup_value(int cpu, int idx, int cgroup);
extern int mlt_cst_value(int cpu, int idx, int cstate);
extern u64 mlt_art_last_update_time(int cpu);
extern void mlt_task_init(struct task_struct *p);
extern int mlt_task_value(struct task_struct *p, int idx);
extern int mlt_task_recent(struct task_struct *p);
extern int mlt_task_avg(struct task_struct *p);
extern int mlt_task_cur_period(struct task_struct *p);
extern int mlt_init(struct kobject *ems_kobj, struct device_node *dn);
extern void ntu_apply(struct sched_entity *se);
extern void ntu_init(struct kobject *ems_kobj);
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_task_load_avg(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long ml_cpu_util_with(struct task_struct *p, int dst_cpu);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_load_avg(int cpu);
extern unsigned long ml_cpu_util_est(int cpu);
extern unsigned long ml_cpu_util_est_with(struct task_struct *p, int cpu);
extern unsigned long ml_cpu_util_est_without(int cpu, struct task_struct *p);

#define MLT_PERIOD_SIZE		(4 * NSEC_PER_MSEC)
#define MLT_PERIOD_COUNT	8
#define MLT_PERIOD_SUM		(MLT_PERIOD_COUNT * SCHED_CAPACITY_SCALE)
#define MLT_STATE_NOCHANGE	0xBABE
#define MLT_TASK_SIZE		(sizeof(u64) * 39)
#define MLT_IDLE_THR_TIME	(8 * NSEC_PER_MSEC)

enum {
	CLKOFF = 0,
	PWROFF,
	CSTATE_MAX,
};

struct uarch_data {
	u64		last_cycle;
	u64		last_inst_ret;
	u64		last_mem_stall;

	int		**ipc_periods;
	int		**mspc_periods;
	int		*ipc_recent;
	int		*mspc_recent;
	u64		*cycle_sum;
	u64		*inst_ret_sum;
	u64		*mem_stall_sum;
};

struct mlt {
	int		cpu;

	int		cur_period;
	u64		period_start;
	u64		last_updated;

	int		state;
	int		state_count;

	int		**periods;
	int		*recent;
	u64		*recent_sum;

	struct uarch_data *uarch;
};

struct mlt_nr_run {
	int		avg_nr_run;
	u64		accomulate_time;
	u64		nr_run_ctrb;
	u64		last_ctrb_updated;

	raw_spinlock_t	lock;
};

/* ontime migration */
struct ontime_dom {
	struct list_head	node;

	unsigned long		upper_boundary;
	int			upper_ratio;
	unsigned long		lower_boundary;
	int			lower_ratio;

	struct cpumask		cpus;
};

extern int ontime_init(struct kobject *ems_kobj);
extern int ontime_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern void ontime_migration(void);

/* cpufreq */
struct cpufreq_hook_data {
	int (*func_get_next_cap)(struct tp_env *env, struct cpumask *cpus, int dst_cpu);
	int (*func_need_slack_timer)(void);
};

extern void cpufreq_register_hook(int cpu, int (*func_get_next_cap)(struct tp_env *env,
			struct cpumask *cpus, int dst_cpu),
		int (*func_need_slack_timer)(void));
extern void cpufreq_unregister_hook(int cpu);
extern int cpufreq_init(void);

extern int cpufreq_get_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu);
extern void slack_timer_cpufreq(int cpu, bool idle_start, bool idle_end);

struct fclamp_data {
	int freq;
	int target_period;
	int target_ratio;
	int type;
};

extern unsigned int fclamp_apply(struct cpufreq_policy *policy, unsigned int orig_freq);

/* cpufreq governor */
extern int get_gov_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu, bool apply_clamp);

/* ego */
extern int ego_pre_init(struct kobject *ems_kobj);

/* halo */
#ifdef CONFIG_CPU_IDLE_GOV_HALO
extern int halo_governor_init(struct kobject *ems_kobj);
extern void halo_tick(struct rq *rq);
#else
static inline int halo_governor_init(struct kobject *ems_kobj) { return 0; }
static inline void halo_tick(struct rq *rq) {}
#endif

/* freqboost */
extern int freqboost_init(void);
extern void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags);
extern void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags);
extern int freqboost_can_attach(struct cgroup_taskset *tset);
extern unsigned long freqboost_cpu_boost(int cpu, unsigned long util);
extern int freqboost_get_task_ratio(struct task_struct *p);
extern unsigned long heavytask_cpu_boost(int cpu, unsigned long util, int ratio);

/* frt */
extern int frt_init(struct kobject *ems_kobj);
extern void frt_update_available_cpus(struct rq *rq);
extern int frt_find_lowest_rq(struct task_struct *task, struct cpumask *lowest_mask);
extern int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags);

/* fvr */
extern void fv_init_table(struct cpufreq_policy *policy);
extern int fv_init(struct kobject *ems_kobj);
extern u64 fv_get_residency(int cpu, int state);
extern u64 fv_get_exit_latency(int cpu, int state);

/* For ecs governor - stage */
struct ecs_stage {
	struct list_head	node;

	unsigned int		id;
	unsigned int		busy_threshold;

	struct cpumask		cpus;
};

struct ecs_domain {
	struct list_head	node;

	unsigned int		id;
	unsigned int		busy_threshold_ratio;

	struct cpumask		cpus;

	struct ecs_stage	*cur_stage;
	struct list_head	stage_list;
};

/* For ecs governor - dynamic */
struct dynamic_dom {
	struct list_head	node;
	unsigned int		id;
	unsigned int		busy_ratio;
	struct cpumask		cpus;
	struct cpumask		default_cpus;
	struct cpumask		governor_cpus;
	bool			has_spared_cpu;

	/* raw data */
	int flag;
	int nr_cpu;
	int nr_busy_cpu;
	int avg_nr_run;
	int slower_misfit;
	int active_ratio;
	unsigned long util;
	unsigned long cap;
	int throttle_cnt;
};

#define ECS_USER_NAME_LEN 	(16)
struct ecs_governor {
	char name[ECS_USER_NAME_LEN];
	struct list_head	list;
	void (*update)(void);
	void (*enqueue_update)(int prev_cpu, struct task_struct *p);
	void (*start)(const struct cpumask *cpus);
	void (*stop)(void);
	const struct cpumask *(*get_target_cpus)(void);
};

extern int ecs_init(struct kobject *ems_kobj);
extern int ecs_gov_stage_init(struct kobject *ems_kobj);
extern int ecs_gov_dynamic_init(struct kobject *ems_kobj);
extern const struct cpumask *ecs_cpus_allowed(struct task_struct *p);
extern const struct cpumask *ecs_available_cpus(void);
extern void ecs_update(void);
extern void ecs_enqueue_update(int prev_cpu, struct task_struct *p);
extern int ecs_cpu_available(int cpu, struct task_struct *p);
extern void ecs_governor_register(struct ecs_governor *gov, bool default_gov);
extern void update_ecs_cpus(void);
extern struct kobject *ecs_get_governor_object(void);

/*
 * Priority-pinning
 */
enum {
	BOOSTED_TEX = 0,
	BINDER_TEX,
	PRIO_TEX,
	NOT_TEX,
};

#define TEX_WINDOW			(4000000)
#define TEX_WINDOW_COUNT	(3)
#define TEX_FULL_WINDOWS	(TEX_WINDOW * TEX_WINDOW_COUNT)

extern bool is_boosted_tex_task(struct task_struct *p);
extern void tex_enqueue_task(struct task_struct *p, int cpu);
extern void tex_dequeue_task(struct task_struct *p, int cpu);
extern void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev);
extern int get_tex_level(struct task_struct *p);
extern void tex_check_preempt_wakeup(struct rq *rq, struct task_struct *p,
		bool *preempt, bool *ignore);
extern void tex_update(struct rq *rq);
extern void tex_do_yield(struct task_struct *p);
extern void tex_task_init(struct task_struct *p);

/* Binder */
extern void ems_set_binder_task(struct task_struct *p, bool sync,
		struct binder_proc *proc);
extern void ems_clear_binder_task(struct task_struct *p);
extern void ems_set_binder_priority(struct binder_transaction *t, struct task_struct *p);
extern void ems_restore_binder_priority(struct binder_transaction *t, struct task_struct *p);


/* EMSTune */
extern int emstune_init(struct kobject *ems_kobj, struct device_node *dn, struct device *dev);
extern void emstune_ontime_init(void);

/* emstune - energy table */
struct emstune_specific_energy_table {
	unsigned int mips_mhz[VENDOR_NR_CPUS];
	unsigned int dynamic_coeff[VENDOR_NR_CPUS];
};

/* emstune - sched policy */
struct emstune_sched_policy {
	int policy[CGROUP_COUNT];
};

/* emstune - active weight */
struct emstune_active_weight {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - idle weight */
struct emstune_idle_weight {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - freq boost */
struct emstune_freqboost {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - cpufreq governor parameters */
struct emstune_cpufreq_gov {
	int htask_boost[VENDOR_NR_CPUS];
	int pelt_boost[VENDOR_NR_CPUS];
	int split_pelt_margin[VENDOR_NR_CPUS];
	unsigned int split_pelt_margin_freq[VENDOR_NR_CPUS];

	int split_up_rate_limit[VENDOR_NR_CPUS];
	unsigned int split_up_rate_limit_freq[VENDOR_NR_CPUS];
	int down_rate_limit;
	int rapid_scale_up;
	int rapid_scale_down;
	int dis_buck_share[VENDOR_NR_CPUS];
};

/* emstune - cpuidle governor parameters */
struct emstune_cpuidle_gov {
	int expired_ratio[VENDOR_NR_CPUS];
};

/* emstune - ontime migration */
struct emstune_ontime {
	int enabled[CGROUP_COUNT];
	struct list_head *p_dom_list;
	struct list_head dom_list;
};

/* emstune - priority pinning */
struct emstune_tex {
	int enabled[CGROUP_COUNT];
	int prio;
};

/* emstune - fclamp */
struct emstune_fclamp {
	int min_freq[VENDOR_NR_CPUS];
	int min_target_period[VENDOR_NR_CPUS];
	int min_target_ratio[VENDOR_NR_CPUS];

	int max_freq[VENDOR_NR_CPUS];
	int max_target_period[VENDOR_NR_CPUS];
	int max_target_ratio[VENDOR_NR_CPUS];

	int monitor_group[CGROUP_COUNT];
};

/* SCHED CLASS */
#define EMS_SCHED_STOP		(1 << 0)
#define EMS_SCHED_DL		(1 << 1)
#define EMS_SCHED_RT		(1 << 2)
#define EMS_SCHED_FAIR		(1 << 3)
#define EMS_SCHED_IDLE		(1 << 4)
#define NUM_OF_SCHED_CLASS	5

#define EMS_SCHED_CLASS_MASK	(0x1F)

/* emstune - cpus allowed */
struct emstune_cpus_binding {
	unsigned long target_sched_class;
	struct cpumask mask[CGROUP_COUNT];
};

/* emstune - fluid rt */
struct emstune_frt {
	int active_ratio[VENDOR_NR_CPUS];
};

/* emstune - core sparing */
struct emstune_ecs {
	struct list_head domain_list;
};

struct emstune_ecs_dynamic {
	int dynamic_busy_ratio[VENDOR_NR_CPUS];
};

/* emstune - new task util */
struct emstune_ntu {
	int ratio[CGROUP_COUNT];
};

struct emstune_support_uclamp {
	int enabled;
};

struct emstune_gsc {
	int enabled[CGROUP_COUNT];
	unsigned int up_threshold;
	unsigned int down_threshold;
};

struct emstune_should_spread {
	int enabled;
};

struct emstune_set {
	int					type;
	int					cpu_dsu_table_index;

	struct emstune_specific_energy_table	specific_energy_table;
	struct emstune_sched_policy		sched_policy;
	struct emstune_active_weight		active_weight;
	struct emstune_idle_weight		idle_weight;
	struct emstune_freqboost		freqboost;
	struct emstune_cpufreq_gov		cpufreq_gov;
	struct emstune_cpuidle_gov		cpuidle_gov;
	struct emstune_ontime			ontime;
	struct emstune_tex			tex;
	struct emstune_cpus_binding		cpus_binding;
	struct emstune_frt			frt;
	struct emstune_ecs			ecs;
	struct emstune_ecs_dynamic		ecs_dynamic;
	struct emstune_ntu			ntu;
	struct emstune_fclamp			fclamp;
	struct emstune_support_uclamp	support_uclamp;
	struct emstune_gsc			gsc;
	struct emstune_should_spread	should_spread;
};

struct emstune_mode {
	const char			*desc;
	struct emstune_set		*sets;
};

extern int emstune_sched_boost(void);
extern int emstune_support_uclamp(void);
extern int emstune_should_spread(void);

enum {
	PROFILE_CPU_IDLE,	/* cpu is not busy */
	PROFILE_CPU_BUSY,	/* cpu busy */
};

enum {
	CPU_UTIL,	/* cpu util */
	CPU_WRATIO,	/* cpu weighted active ratio */
	CPU_HTSK,	/* cpu heaviest task */
	CPU_PROFILE_MAX,
};

/* Acrive ratio based profile */
struct cpu_profile {
	u64	value;	/* util, wratio, htsk */
	u64	data;	/* busy,   busy,  pid */
};

/* profile */
struct system_profile_data {
	/* util based profile */
	int			busy_cpu_count;
	int			heavy_task_count;
	int			misfit_task_count;
	int			pd_nr_running;
	int			ed_ar_avg;
	int			pd_ar_avg;

	unsigned long		cpu_util_sum;
	unsigned long		heavy_task_util_sum;
	unsigned long		misfit_task_util_sum;
	unsigned long		heaviest_task_util;

	/* active ratio based profile */

	/* cpu profile data */
	struct cpu_profile	cp[VENDOR_NR_CPUS][CPU_PROFILE_MAX];
};

#define HEAVY_TASK_UTIL_RATIO	(40)
#define MISFIT_TASK_UTIL_RATIO	(80)
#define check_busy(util, cap)	((util * 100) >= (cap * 80))

static inline int is_heavy_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * HEAVY_TASK_UTIL_RATIO / 100);
}

static inline int is_misfit_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100);
}

extern int profile_sched_init(struct kobject *);
extern int profile_sched_data(void);
extern int profile_get_htask_ratio(int cpu);
extern u64 profile_get_cpu_wratio_busy(int cpu);
extern void profile_enqueue_task(struct rq *rq, struct task_struct *p);
extern void get_system_sched_data(struct system_profile_data *);

enum fair_causes {
	FAIR_ALLOWED_0,
	FAIR_ALLOWED_1,
	FAIR_FIT_0,
	FAIR_FIT_1,
	FAIR_SYSBUSY,
	FAIR_EXPRESS,
	FAIR_ENERGY,
	FAIR_PERFORMANCE,
	FAIR_SYNC,
	FAIR_FAST_TRACK,
	FAIR_FAILED,
	END_OF_FAIR_CAUSES,
};

enum rt_causes {
	RT_ALLOWED_1,
	RT_IDLE,
	RT_RECESSIVE,
	RT_FAILED,
	END_OF_RT_CAUSES,
};

extern void update_fair_stat(int cpu, enum fair_causes);
extern void update_rt_stat(int cpu, enum rt_causes);

/* Sysbusy */
struct sysbusy_param {
	int monitor_interval;		/* tick (1 tick = 4ms) */
	int release_duration;		/* tick (1 tick = 4ms) */
	unsigned long allowed_next_state;
};

#define TICK_SEC	250
static struct sysbusy_param sysbusy_params[] = {
	{
		/* SYSBUSY_STATE0 (sysbusy inactivation) */
		.monitor_interval	= 1,
		.release_duration	= 0,
		.allowed_next_state	= (1 << SYSBUSY_STATE1) |
					  (1 << SYSBUSY_STATE2) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE1 */
		.monitor_interval	= 1,
		.release_duration	= 2,
		.allowed_next_state	= (1 << SYSBUSY_STATE0) |
					  (1 << SYSBUSY_STATE2) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE2 */
		.monitor_interval	= 6,
		.release_duration	= 2,
		.allowed_next_state	= (1 << SYSBUSY_STATE0) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE3 */
		.monitor_interval	= 25,
		.release_duration	= TICK_SEC * 9,
		.allowed_next_state	= (1 << SYSBUSY_STATE0),
	},
};

#define SYSBUSY_SOMAC	SYSBUSY_STATE3

extern int sysbusy_init(struct kobject *ems_kobj);
extern void monitor_sysbusy(void);
extern int sysbusy_schedule(struct tp_env *env);
extern int sysbusy_activated(void);
extern void somac_tasks(void);
extern int sysbusy_on_somac(void);
extern int sysbusy_boost_task(struct task_struct *p);

/* IOCTL interface */
#define EMS_IOCTL_MAGIC	'E'

#define EMS_IOCTL_R_ECS_DOMAIN_COUNT		_IOR(EMS_IOCTL_MAGIC, 0x00, int)
#define EMS_IOCTL_R_ECS_DOMAIN_INFO		_IOR(EMS_IOCTL_MAGIC, 0x01, int)
#define EMS_IOCTL_W_ECS_TARGET_DOMAIN_ID	_IOW(EMS_IOCTL_MAGIC, 0x02, int)
#define EMS_IOCTL_W_ECS_TARGET_STAGE_ID		_IOW(EMS_IOCTL_MAGIC, 0x03, int)
#define EMS_IOCTL_W_ECS_STAGE_THRESHOLD		_IOW(EMS_IOCTL_MAGIC, 0x04, int)

#define NR_ECS_ENERGY_STATES		(1 << 3)
#define NR_ECS_STAGES			(1 << 3)
struct ems_ioctl_ecs_domain_info {
	unsigned long		cpus;
	int			cd_dp_ratio[VENDOR_NR_CPUS];
	int			cd_cp_ratio[VENDOR_NR_CPUS];
	int			num_of_state;
	struct {
		unsigned long frequency;
		unsigned long capacity;
		unsigned long v_square;
		unsigned long dynamic_power;
		unsigned long static_power;
	} states[VENDOR_NR_CPUS][NR_ECS_ENERGY_STATES];
	int			num_of_stage;
	unsigned long		cpus_each_stage[NR_ECS_STAGES];
};

extern int ecs_ioctl_get_domain_count(void);
extern int ecs_ioctl_get_ecs_domain_info(struct ems_ioctl_ecs_domain_info *info);
extern void ecs_ioctl_set_target_domain(unsigned int domain_id);
extern void ecs_ioctl_set_target_stage(unsigned int stage_id);
extern void ecs_ioctl_set_stage_threshold(unsigned int threshold);

/* MH DVFS */
#ifdef CONFIG_MHDVFS
extern void mhdvfs(void);
extern void mhdvfs_init(struct kobject *ems_kobj);
#else
static inline void mhdvfs(void) { };
static inline void mhdvfs_init(struct kobject *ems_kobj) { };
#endif


/******************************************************************************
 * common API                                                                 *
 ******************************************************************************/
static inline unsigned long capacity_cpu_orig(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig;
}

static inline unsigned long capacity_cpu(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static inline int cpu_overutilized(int cpu)
{
	return (capacity_cpu(cpu) * 1024) < (ml_cpu_util(cpu) * 1280);
}

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

static inline struct sched_entity *get_task_entity(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct cfs_rq *cfs_rq = se->my_q;

	while (cfs_rq) {
		se = cfs_rq->curr;
		cfs_rq = se->my_q;
	}
#endif

	return se;
}

static inline struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return se->cfs_rq;
#else
	return &task_rq(task_of(se))->cfs;
#endif
}

static inline bool can_migrate(struct task_struct *p, int dst_cpu)
{
	if (p->exit_state)
		return false;

	if (is_per_cpu_kthread(p))
		return false;

#ifdef CONFIG_SMP
	if (task_running(NULL, p))
		return false;
#endif

	if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
		return false;

	return cpu_active(dst_cpu);
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return clamp(ml_task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return ml_task_util_est(p);
}
#endif

extern int busy_cpu_ratio;
static inline bool __is_busy_cpu(unsigned long util,
				 unsigned long runnable,
				 unsigned long capacity,
				 unsigned long nr_running)
{
	if (runnable < capacity)
		return false;

	if (!nr_running)
		return false;

	if (util * busy_cpu_ratio >= runnable * 100)
		return false;

	return true;
}

static inline bool is_busy_cpu(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned long util, runnable, capacity, nr_running;

	cfs_rq = &cpu_rq(cpu)->cfs;

	util = ml_cpu_util(cpu);
	runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
	capacity = capacity_orig_of(cpu);
	nr_running = cpu_rq(cpu)->nr_running;

	return __is_busy_cpu(util, runnable, capacity, nr_running);
}

extern int get_sched_class(struct task_struct *p);
extern int cpuctl_task_group_idx(struct task_struct *p);
extern const struct cpumask *cpu_coregroup_mask(int cpu);
extern const struct cpumask *cpu_slowest_mask(void);
extern const struct cpumask *cpu_fastest_mask(void);
extern int send_uevent(char *msg);

extern void gsc_init(struct kobject *ems_kobj);
extern void gsc_update_tick(void);
extern bool is_gsc_task(struct task_struct *p);

extern void detach_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p);
extern int detach_one_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *target);
extern void attach_task(struct rq *dst_rq, struct task_struct *p);
extern void attach_one_task(struct rq *dst_rq, struct task_struct *p);

static inline u64 amu_core_cycles(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0);
}

static inline u64 amu_const_cycles(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_CONST_EL0);
}

static inline u64 amu_inst_ret(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0);
}

static inline u64 amu_mem_stall(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_MEM_STALL);
}

/* Load Balance */
#define MAX_CLUSTER_NUM		(3)
static inline void ems_rq_update_nr_misfited(struct rq *rq, bool misfit)
{
	int val = misfit ? 1 : -1;

	ems_rq_nr_misfited(rq) += val;

	BUG_ON(ems_rq_nr_misfited(rq) < 0);
}

#define MARGIN_MIGRATED_SLOWER	1280
#define MARGIN_MIGRATED_FASETR	1280
static inline bool ems_task_fits_capacity(struct task_struct *p, unsigned long capacity)
{
	unsigned int margin;

	if (capacity_orig_of(task_cpu(p)) > capacity)
		margin = MARGIN_MIGRATED_SLOWER;
	else
		margin = MARGIN_MIGRATED_FASETR;

	return capacity * SCHED_CAPACITY_SCALE > ml_uclamp_task_util(p) * margin;
}

static inline bool is_small_task(struct task_struct *p)
{
	unsigned long util = ml_task_util_est(p);
	unsigned long threshold = 25; /* FIXME */

	return util < threshold;
}

static inline bool ems_task_fits_max_cap(struct task_struct *p, int cpu)
{
	unsigned long capacity = capacity_orig_of(cpu);
	unsigned long fastest_capacity;
	unsigned long slowest_capacity = capacity_orig_of(cpumask_first(cpu_slowest_mask()));
	unsigned long task_boosted;
	struct cpumask cpus_allowed;

	cpumask_and(&cpus_allowed, cpu_active_mask, p->cpus_ptr);
	cpumask_and(&cpus_allowed, &cpus_allowed, cpus_binding_mask(p));
	if (cpumask_empty(&cpus_allowed))
		cpumask_copy(&cpus_allowed, p->cpus_ptr);
	fastest_capacity = capacity_orig_of(cpumask_last(&cpus_allowed));

	if (capacity == fastest_capacity)
		return true;

	if (capacity == slowest_capacity) {
		task_boosted = is_boosted_tex_task(p) || sysbusy_boost_task(p)
				|| (uclamp_boosted(p) && !is_small_task(p));
		if (task_boosted)
			return false;
	}
	return ems_task_fits_capacity(p, capacity);
}

extern void ems_update_misfit_status(struct task_struct *p, struct rq *rq, bool *need_update);
extern void lb_enqueue_misfit_task(struct task_struct *p, struct rq *rq);
extern void lb_dequeue_misfit_task(struct task_struct *p, struct rq *rq);
extern void lb_update_misfit_status(struct task_struct *p, struct rq *rq, bool *need_update);
extern void ems_nohz_balancer_kick(struct rq *rq, unsigned int *flag, int *done);
extern void lb_nohz_balancer_kick(struct rq *rq, unsigned int *flag, int *done);
extern void ems_can_migrate_task(struct task_struct *p, int dst_cpu, int *can_migrate);
extern void lb_can_migrate_task(struct task_struct *p, int dst_cpu, int *can_migrate);
extern void ems_find_busiest_queue(int dst_cpu, struct sched_group *group,
		struct cpumask *env_cpus, struct rq **busiest, int *done);
extern void lb_find_busiest_queue(int dst_cpu, struct sched_group *group,
		struct cpumask *env_cpus, struct rq **busiest, int *done);
extern void ems_newidle_balance(struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
extern void lb_newidle_balance(struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
extern void lb_tick(struct rq *rq);
extern void lb_init(void);
extern void ems_arch_set_freq_scale(const struct cpumask *cpus, unsigned long freq,
		unsigned long max, unsigned long *scale);
