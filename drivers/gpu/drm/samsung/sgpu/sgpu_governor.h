/*
* @file sgpu_governor.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_GOVERNOR_H_
#define _SGPU_GOVERNOR_H_

#include <linux/pm_qos.h>
#include "../../../../devfreq/governor.h"

#define HZ_PER_KHZ			1000
#define WINDOW_MAX_SIZE			12
#define WEIGHT_TABLE_MAX_SIZE		11
#define WEIGHT_TABLE_IDX_NUM		2

struct sgpu_governor_data {
	struct devfreq			*devfreq;
	struct amdgpu_device		*adev;
	struct notifier_block           nb_trans;
#ifdef CONFIG_DRM_SGPU_EXYNOS
	struct notifier_block		nb_tmu;
#endif
	struct dev_pm_qos_request	sys_pm_qos_min;
	struct dev_pm_qos_request	sys_pm_qos_max;
	unsigned long			sys_max_freq;
	unsigned long			sys_min_freq;

	struct timer_list		task_timer;
	struct task_struct		*update_task;
	/* msec */
	unsigned int			valid_time;
	bool				wakeup_lock;

	unsigned long			highspeed_freq;
	uint32_t			highspeed_load;
	uint32_t			highspeed_delay;
	uint32_t			highspeed_level;
	uint32_t			*downstay_times;
	uint32_t			*min_thresholds;
	uint32_t			*max_thresholds;

	/* current state */
	bool				in_suspend;
	struct sgpu_governor_info	*governor;
	unsigned long			min_freq;
	unsigned long			max_freq;
	int				current_level;
	unsigned long			expire_jiffies;
	unsigned long			expire_highspeed_delay;
	uint32_t			power_ratio;
	struct mutex			lock;

	/* utilization window table */
	uint32_t			window[WINDOW_MAX_SIZE];
	uint32_t			window_idx;
	uint32_t			weight_table_idx[WEIGHT_TABLE_IDX_NUM];
	int				freq_margin;
	/* %, additional weight for compute scenario */
	int				compute_weight;
	void __iomem			*bg3d_dvfs;

	/* cl_boost */
	bool				cl_boost_disable;
	bool				cl_boost_status;
	uint32_t			cl_boost_level;
	unsigned long			mm_min_clock;

	/* local_minlock */
#if IS_ENABLED(CONFIG_GPU_THERMAL)
	uint32_t			tmu_id;
	bool				local_minlock_status;
	uint32_t			local_minlock_level;
	uint32_t			local_minlock_util;
	uint32_t			local_minlock_temp;
#endif
	/* fine-grained mode control */
	uint32_t			fine_grained_step;
	uint32_t			fine_grained_low_level;
	uint32_t			fine_grained_high_level;
	uint32_t			major_state;

	/* minlock limitation */
	bool				minlock_limit;
	unsigned long			minlock_limit_freq;
};

ssize_t sgpu_governor_all_info_show(struct devfreq *df, char *buf);
ssize_t sgpu_governor_current_info_show(struct devfreq *df, char *buf);
int sgpu_governor_change(struct devfreq *df, char *str_governor);
int sgpu_governor_init(struct device *dev, struct devfreq_dev_profile *dp,
		       struct sgpu_governor_data **governor_data);
void sgpu_governor_deinit(struct devfreq *df);
unsigned int *sgpu_get_array_data(struct devfreq_dev_profile *dp, const char *buf);

void sgpu_dvfs_governor_major_current(struct devfreq *df, uint32_t *level);
void sgpu_dvfs_governor_major_level_up(struct devfreq *df, uint32_t *level);
void sgpu_dvfs_governor_major_level_down(struct devfreq *df, uint32_t *level);
bool sgpu_dvfs_governor_major_level_check(struct devfreq *df, unsigned long freq);
#endif
