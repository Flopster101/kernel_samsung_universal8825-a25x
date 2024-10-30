/*
* @file sgpu_utilization.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_UTILIZATION_H_
#define _SGPU_UTILIZATION_H_

#include <linux/devfreq.h>

struct sgpu_utilization_info;

typedef enum {
	SGPU_DVFS_SRC_JIFFIES = 0,
	SGPU_DVFS_SRC_HW_COUNTER,
	SGPU_DVFS_SRC_FAKE_RANDOM,
	SGPU_DVFS_SRC_FAKE_SWING,
	SGPU_DVFS_SRC_FAKE_HW_COUNTER,
	SGPU_MAX_SRC_NUM,
} gpu_utilization_src_type;

struct sgpu_utilization_info {
	uint64_t			id;
	char				*name;
	bool				hw_source_valid;
	int (*sgpu_get_status)(struct devfreq_dev_status *stat, bool interval);
};

typedef enum {
	SGPU_TIMEINFO_SW = 0,
	SGPU_TIMEINFO_HW,
	SGPU_TIMEINFO_NUM,
} utilization_timeinfo_type;

struct utilization_timeinfo {
	unsigned long			busy_time;
	unsigned long			cu_busy_time;
	unsigned long			total_time;
	unsigned long			prev_busy_time;
	unsigned long			cu_prev_busy_time;
	unsigned long			prev_total_time;
};

struct utilization_data {
	struct devfreq			*devfreq;
	struct amdgpu_device		*adev;
	spinlock_t			lock;

	/* current state */
	struct sgpu_utilization_info	*utilization_src;
	int				active;
	int				cu_active;
	uint64_t			last_time;
	uint64_t			cu_last_time;
	uint64_t			trace_time;
	uint32_t			last_util;
	uint32_t			last_cu_util;
	struct utilization_timeinfo	timeinfo[SGPU_TIMEINFO_NUM];
};

struct dpm_hw_utilization_funcs {
	void (*init)(struct amdgpu_device *adev);
	void (*reset)(struct amdgpu_device *adev);
	int (*get_hw_time)(struct amdgpu_device *adev,
			   uint32_t *busy, uint32_t *total);
};
struct dpm_hw_utilization {
	const struct dpm_hw_utilization_funcs *funcs;
};

int sgpu_utilization_job_start(struct devfreq *df, uint32_t job_count, bool cu_job);
int sgpu_utilization_job_end(struct devfreq *df, uint32_t job_count, bool cu_job);
void sgpu_utilization_trace_stop(struct devfreq_dev_status *stat);
void sgpu_utilization_trace_start(struct devfreq_dev_status *stat);
void sgpu_utilization_trace_before(struct devfreq_dev_status *stat, unsigned long freq);
void sgpu_utilization_trace_after(struct devfreq_dev_status *stat, unsigned long freq);
int sgpu_utilization_capture(struct devfreq_dev_status *stat);
int sgpu_utilization_init(struct amdgpu_device *adev, struct devfreq *df);
void sgpu_utilization_deinit(struct devfreq *df);
int sgpu_utilization_src_change(struct devfreq *df, char *buf);
ssize_t sgpu_utilization_current_src_show(struct devfreq *df, char *buf);
ssize_t sgpu_utilization_all_src_show(struct devfreq *df, char *buf);

#endif
