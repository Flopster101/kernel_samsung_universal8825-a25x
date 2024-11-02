#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs_exynos_perf

#if !defined(_TRACE_UFS_EXYNOS_PERF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UFS_EXYNOS_PERF_H

#include <linux/tracepoint.h>

#define CTRL_OP				\
	EM(CTRL_OP_NONE)		\
	EM(CTRL_OP_UP)			\
	EMe(CTRL_OP_DOWN)

#define PERF_OP				\
	EM(UFS_PERF_OP_R)		\
	EM(UFS_PERF_OP_W)		\
	EMe(UFS_PERF_OP_NONE)

#define POLICY_RES			\
	EM(R_OK)			\
	EMe(R_CTRL)

#define UPDATE_ENTRY			\
	EM(UFS_PERF_ENTRY_QUEUED)	\
	EMe(UFS_PERF_ENTRY_RESET)

#define HAT_STATE			\
	EM(HAT_RARE)			\
	EM(HAT_PRE_DWELL)		\
	EM(HAT_REACH)			\
	EM(HAT_DWELL)			\
	EM(HAT_PRE_RARE)		\
	EMe(HAT_DROP)

#define FREQ_STATE			\
	EM(FREQ_RARE)			\
	EM(FREQ_PRE_DWELL)		\
	EM(FREQ_REACH)			\
	EM(FREQ_DWELL)			\
	EM(FREQ_PRE_RARE)		\
	EMe(FREQ_DROP)

#undef EM
#undef EMe
#define EM(a)	TRACE_DEFINE_ENUM(a);
#define EMe(a)	TRACE_DEFINE_ENUM(a);

CTRL_OP;
PERF_OP;
POLICY_RES;
UPDATE_ENTRY;
HAT_STATE;
FREQ_STATE;

#undef EM
#undef EMe
#define EM(a)	{ a, #a },
#define EMe(a)	{ a, #a }

DECLARE_EVENT_CLASS(ufs_perf_one_int,

	TP_PROTO(int i),

	TP_ARGS(i),

	TP_STRUCT__entry(
		__field(int,	i)
	),

	TP_fast_assign(
		__entry->i = i;
	),

	TP_printk("%d", __entry->i)
);

DECLARE_EVENT_CLASS(ufs_perf_three_int,

	TP_PROTO(int i, int j, int k),

	TP_ARGS(i, j, k),

	TP_STRUCT__entry(
		__field(int,	i)
		__field(int,	j)
		__field(int,	k)
	),

	TP_fast_assign(
		__entry->i = i;
		__entry->j = j;
		__entry->k = k;
	),

	TP_printk("%d, %d, %d", __entry->i, __entry->j, __entry->k)
);

TRACE_EVENT(ufs_perf_stat,
	TP_PROTO(bool is_cp_time, unsigned long long diff, unsigned long period,
		long long cp_time, long long cur_time,
		unsigned long count, unsigned long th_count,
		int locked, int active),

	TP_ARGS(is_cp_time, diff, period, cp_time, cur_time, count, th_count, locked, active),

	TP_STRUCT__entry(
		__field(bool,	is_cp_time)
		__field(unsigned long long,	diff)
		__field(unsigned long,	period)
		__field(long long,	cp_time)
		__field(long long,	cur_time)
		__field(unsigned long,	count)
		__field(unsigned long,	th_count)
		__field(int,	locked)
		__field(int,	active)
	),

	TP_fast_assign(
		__entry->is_cp_time	= is_cp_time;
		__entry->diff	   = diff;
		__entry->period    = period;
		__entry->cp_time   = cp_time;
		__entry->cur_time  = cur_time;
		__entry->count     = count;
		__entry->th_count  = th_count;
		__entry->locked    = locked;
		__entry->active    = active;
	),

	TP_printk("%d: %llu (%lu, %lld, %lld) (%lu, %lu) | %d, %d,",
		__entry->is_cp_time,
		__entry->diff,
		__entry->period,
		__entry->cp_time,
		__entry->cur_time,
		__entry->count,
		__entry->th_count,
		__entry->locked,
		__entry->active)

);

DEFINE_EVENT(ufs_perf_one_int, ufs_perf_lock_,

	TP_PROTO(int i),

	TP_ARGS(i)
);

DEFINE_EVENT(ufs_perf_three_int, ufs_perf_issue,

	TP_PROTO(int is_big, int op, int len),

	TP_ARGS(is_big, op, len)
);

TRACE_EVENT(ufs_perf,
	TP_PROTO(const char *event, int op, u32 doorbell, s64 time, int policy_res, u32 len),

	TP_ARGS(event, op, doorbell, time, policy_res, len),

	TP_STRUCT__entry(
		__string(event, event)
		__field(int, op)
		__field(u32, doorbell)
		__field(s64, time)
		__field(int, policy_res)
		__field(u32, len)
	),

	TP_fast_assign(
		__assign_str(event, event);
		__entry->op = op;
		__entry->doorbell = doorbell;
		__entry->time = time;
		__entry->policy_res = policy_res;
		__entry->len = len;
	),

	TP_printk(
		"%s: %s: DB: %u, time: %lld usec, %s, len= %u",
		__get_str(event),
		__print_symbolic(__entry->op, PERF_OP),
		__entry->doorbell, __entry->time,
		__print_symbolic(__entry->policy_res, POLICY_RES),
		__entry->len
	)
);

TRACE_EVENT(ufs_perf_update_v1,
	TP_PROTO(const char *event, u32 count,
		 int hat_state, int freq_state, s64 time),

	TP_ARGS(event, count, hat_state, freq_state, time),

	TP_STRUCT__entry(
		__string(event, event)
		__field(u32, count)
		__field(int, hat_state)
		__field(int, freq_state)
		__field(s64, time)
	),

	TP_fast_assign(
		__assign_str(event, event);
		__entry->count = count;
		__entry->hat_state = hat_state;
		__entry->freq_state = freq_state;
		__entry->time = time;
	),

	TP_printk(
		"%s: count: %u, %s, %s, %lld usec",
		__get_str(event),
		__entry->count,
		__print_symbolic(__entry->hat_state, HAT_STATE),
		__print_symbolic(__entry->freq_state, FREQ_STATE),
		__entry->time
	)
);

TRACE_EVENT(ufs_perf_lock,

	TP_PROTO(const char *event, int i),

	TP_ARGS(event, i),

	TP_STRUCT__entry(
		__string(event, event)
		__field(int,	i)
	),

	TP_fast_assign(
		__assign_str(event, event);
		__entry->i = i;
	),

	TP_printk("%s: %s",
		__get_str(event),
		__print_symbolic(__entry->i, CTRL_OP))
);

DECLARE_EVENT_CLASS(ufshcd_profiling_template,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),

	TP_ARGS(dev_name, profile_info, time_us, err),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(profile_info, profile_info)
		__field(s64, time_us)
		__field(int, err)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(profile_info, profile_info);
		__entry->time_us = time_us;
		__entry->err = err;
	),

	TP_printk("%s: %s: took %lld usecs, err %d",
		__get_str(dev_name), __get_str(profile_info),
		__entry->time_us, __entry->err)
);

DEFINE_EVENT(ufshcd_profiling_template, ufshcd_profile_hibern8,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),
	TP_ARGS(dev_name, profile_info, time_us, err));
#endif /* _TRACE_UFS_EXYNOS_PERF_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
