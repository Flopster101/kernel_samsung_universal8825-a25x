/*
* @file sgpu_utilization.c
* @copyright 2020 Samsung Electronics
*/

#include <linux/sched/clock.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_governor.h"
#include "sgpu_utilization.h"

static int sgpu_dvfs_get_utilization(struct devfreq_dev_status *stat, bool interval)
{
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *timeinfo = &data->timeinfo[SGPU_TIMEINFO_SW];

	stat->total_time = timeinfo->total_time;
	stat->busy_time  = timeinfo->busy_time;

	return 0;
}

#define MSB_BIT_SHIFT		31
#define UPPER_BIT_SHIFT		16
#define NORMALIZE_SHIFT		9
#define NORMALIZE_FACT		(1<<(NORMALIZE_SHIFT))
#define RANGE_SHIFT_MAX		((NORMALIZE_SHIFT) - 1)
static int sgpu_dvfs_get_fake_random_utilization(struct devfreq_dev_status *stat, bool interval)
{
	static int utilization = 0;
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *timeinfo = &data->timeinfo[SGPU_TIMEINFO_SW];
	struct devfreq_dev_profile *dp = data->devfreq->profile;
	const uint32_t random_value = get_random_u32();
	int range;
	int freq_util;
	const uint64_t max_range = NORMALIZE_FACT * (dp->freq_table[0] / HZ_PER_KHZ);

	if (!interval) {
		return 0;
	}

	range = max_range >> ((random_value >> UPPER_BIT_SHIFT) % RANGE_SHIFT_MAX);

	if (random_value & (0x1 << MSB_BIT_SHIFT))
		utilization += random_value % ((max_range - utilization < range ?
						max_range - utilization : range) + 1);
	else
		utilization -= random_value % ((utilization < range ? utilization : range) + 1);

	freq_util = utilization / (stat->current_frequency / HZ_PER_KHZ);
	stat->total_time = timeinfo->total_time;
	stat->busy_time = freq_util >= NORMALIZE_FACT ? stat->total_time :
		stat->total_time * freq_util / NORMALIZE_FACT;
	timeinfo->busy_time = stat->busy_time;

	return 0;
}

#define DURATION_MIN	1
#define DURATION_MAX	40
#define DURATION_DIFF	DURATION_MAX - DURATION_MIN
static int sgpu_dvfs_get_fake_swing_utilization(struct devfreq_dev_status *stat,
						bool interval)
{
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *timeinfo = &data->timeinfo[SGPU_TIMEINFO_SW];
	static int duration = 0;
	static int cnt = 0;
	static bool up = false;
	uint32_t random_value = get_random_u32();

	if (!interval) {
		return 0;
	}

	if (++cnt > duration) {
		cnt = 0;
		duration = random_value % DURATION_DIFF;
		duration += DURATION_MIN;
		up = up ? false : true;
	}

	stat->total_time = timeinfo->total_time;
	if (up)
		stat->busy_time = stat->total_time;
	else
		stat->busy_time = 0;

	return 0;
}

static int sgpu_dvfs_get_fake_hw_utilization(struct devfreq_dev_status *stat,
					     bool interval)
{
	static int utilization = 0;
	struct utilization_data *data = stat->private_data;
	struct devfreq_dev_profile *dp = data->devfreq->profile;
	const uint32_t random_value = get_random_u32();
	uint32_t hw_random_value = get_random_u32();
	int range;
	int freq_util;
	const uint64_t max_range = NORMALIZE_FACT * (dp->freq_table[0] / HZ_PER_KHZ);

	if (!interval)
		return 0;

	range = max_range >> ((random_value >> UPPER_BIT_SHIFT) % RANGE_SHIFT_MAX);

	if (random_value & (0x1 << MSB_BIT_SHIFT))
		utilization += random_value % ((max_range - utilization < range ?
						max_range - utilization : range) + 1);
	else
		utilization -= random_value % ((utilization < range ? utilization : range) + 1);

	hw_random_value %= 101;
	freq_util = utilization / (stat->current_frequency / HZ_PER_KHZ);

	data->timeinfo[SGPU_TIMEINFO_SW].busy_time = freq_util >= NORMALIZE_FACT ?
		data->timeinfo[SGPU_TIMEINFO_SW].total_time :
		data->timeinfo[SGPU_TIMEINFO_SW].total_time * freq_util / NORMALIZE_FACT;

	data->timeinfo[SGPU_TIMEINFO_HW].total_time =
		data->timeinfo[SGPU_TIMEINFO_SW].total_time;
	data->timeinfo[SGPU_TIMEINFO_HW].busy_time =
		data->timeinfo[SGPU_TIMEINFO_SW].busy_time * hw_random_value / 100;

	stat->total_time = data->timeinfo[SGPU_TIMEINFO_SW].total_time;
	stat->busy_time  = data->timeinfo[SGPU_TIMEINFO_SW].busy_time;

	return 0;
}

struct sgpu_utilization_info utilization_src_info[SGPU_MAX_SRC_NUM] = {
	{
		.id = SGPU_DVFS_SRC_JIFFIES,
		.name = "jiffies",
		.hw_source_valid = false,
		.sgpu_get_status = sgpu_dvfs_get_utilization,
	},
	{
		.id = SGPU_DVFS_SRC_HW_COUNTER,
		.name = "hw_counter",
		.hw_source_valid = false,
		.sgpu_get_status = sgpu_dvfs_get_utilization,
	},
	{
		.id = SGPU_DVFS_SRC_FAKE_RANDOM,
		.name = "random",
		.hw_source_valid = false,
		.sgpu_get_status = sgpu_dvfs_get_fake_random_utilization,
	},
	{
		.id = SGPU_DVFS_SRC_FAKE_SWING,
		.name = "swing",
		.hw_source_valid = false,
		.sgpu_get_status = sgpu_dvfs_get_fake_swing_utilization,
	},
	{
		.id = SGPU_DVFS_SRC_FAKE_HW_COUNTER,
		.name = "fake_hw_counter",
		.hw_source_valid = false,
		.sgpu_get_status = sgpu_dvfs_get_fake_hw_utilization,
	},
};

int sgpu_utilization_job_start(struct devfreq *df, uint32_t job_count, bool cu_job)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *sw_info = &data->timeinfo[SGPU_TIMEINFO_SW];
	struct amdgpu_device *adev = data->adev;
	int ret = 0;
	uint64_t current_time;
	unsigned long flags;

	if (!job_count)
		return 0;

	spin_lock_irqsave(&data->lock, flags);

	if (data->active == 0) {
		/* starting point */
		current_time = sched_clock();
		sw_info->total_time += current_time - data->last_time;
		data->last_time = current_time;
		if (cu_job)
			data->cu_last_time = current_time;
	} else if (cu_job && data->cu_active == 0) {
		/* compute(CL) job starting point (other jobs are still running) */
		current_time = sched_clock();
		data->cu_last_time = current_time;
	}

	data->active += job_count;
	if (cu_job)
		data->cu_active += job_count;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
		 "amdgpu_ib_schedule active_cnt %d, cu_active_cnt %d, usage_cnt %d",
		 data->active, data->cu_active, data->adev->dev->power.usage_count);

	spin_unlock_irqrestore(&data->lock, flags);
	if (data->active < 0)
		dev_err(df->dev.parent, "%s: active count %d\n", __func__, data->active);
	return ret;
}

int sgpu_utilization_job_end(struct devfreq *df, uint32_t job_count, bool cu_job)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *sw_info = &data->timeinfo[SGPU_TIMEINFO_SW];
	struct sgpu_governor_data *gdata = df->data;
	struct amdgpu_device *adev = gdata->adev;
	uint64_t current_time;
	int ret = 0;
	unsigned long flags;

	if (!job_count)
		return 0;

	spin_lock_irqsave(&data->lock, flags);

	data->active -= job_count;
	if (cu_job)
		data->cu_active -= job_count;

	if (data->active == 0) {
		/* end point */
		current_time = sched_clock();
		sw_info->busy_time += current_time - data->last_time;
		sw_info->total_time += current_time - data->last_time;
		data->last_time = current_time;
		if (cu_job) {
			sw_info->cu_busy_time += current_time - data->cu_last_time;
			data->cu_last_time = current_time;
		}
	} else if (cu_job && data->cu_active == 0) {
		/* compute(CL) job end point (other jobs are still running) */
		current_time = sched_clock();
		sw_info->cu_busy_time += current_time - data->cu_last_time;
		data->cu_last_time = current_time;
	}
	SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
		 "amdgpu_fence_process active_cnt %d cu active cnt %d, usage_cnt %d",
		 data->active, data->cu_active, data->adev->dev->power.usage_count);

	spin_unlock_irqrestore(&data->lock, flags);
	if (data->active < 0)
		dev_err(df->dev.parent, "%s: active count %d\n", __func__, data->active);
	return ret;
}

int sgpu_utilization_capture(struct devfreq_dev_status *stat)
{
	struct utilization_data *data = stat->private_data;
	struct utilization_timeinfo *sw_info = &data->timeinfo[SGPU_TIMEINFO_SW];
	struct sgpu_governor_data *governor_data = data->devfreq->data;

	uint64_t current_time;
	int ret = 0;
	unsigned long flags;

	current_time = sched_clock();

	spin_lock_irqsave(&data->lock, flags);

	sw_info->total_time += current_time - data->last_time;

	if (data->active > 0) {
		sw_info->busy_time += current_time - data->last_time;
		if (data->cu_active > 0)
			sw_info->cu_busy_time += current_time - data->cu_last_time;
	}
	data->last_time = current_time;
	data->cu_last_time = current_time;

	if (sw_info->total_time - sw_info->prev_total_time >=
	    governor_data->valid_time * NSEC_PER_MSEC) {
		sw_info->total_time -= sw_info->prev_total_time;
		sw_info->prev_total_time = sw_info->total_time;
		sw_info->busy_time -= sw_info->prev_busy_time;
		sw_info->prev_busy_time = sw_info->busy_time;

		sw_info->cu_busy_time -= sw_info->cu_prev_busy_time;
		sw_info->cu_prev_busy_time = sw_info->cu_busy_time;

		data->utilization_src->sgpu_get_status(stat, true);
	} else if (sw_info->prev_total_time) {
		data->utilization_src->sgpu_get_status(stat, false);
	}

	trace_sgpu_devfreq_utilization(sw_info, stat->current_frequency);

	spin_unlock_irqrestore(&data->lock, flags);
	return ret;
}

static void sgpu_utilization_reset(struct devfreq_dev_status *stat)
{
	struct utilization_data *data = stat->private_data;
	int i = 0;

	for (i = SGPU_TIMEINFO_SW; i < SGPU_TIMEINFO_NUM; i++) {
		data->timeinfo[i].prev_total_time = 0;
		data->timeinfo[i].prev_busy_time = 0;
		data->timeinfo[i].cu_prev_busy_time = 0;
		data->timeinfo[i].total_time = 0;
		data->timeinfo[i].busy_time = 0;
		data->timeinfo[i].cu_busy_time = 0;
	}
}

void sgpu_utilization_trace_start(struct devfreq_dev_status *stat)
{
	struct utilization_data *data = stat->private_data;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	sgpu_utilization_reset(stat);
	data->trace_time = data->last_time = data->cu_last_time = sched_clock();
	spin_unlock_irqrestore(&data->lock, flags);
}

void sgpu_utilization_trace_stop(struct devfreq_dev_status *stat)
{
	struct utilization_data *data = stat->private_data;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	sgpu_utilization_reset(stat);
	stat->total_time = 0;
	stat->busy_time = 0;
	spin_unlock_irqrestore(&data->lock, flags);
}

void sgpu_utilization_trace_before(struct devfreq_dev_status *stat, unsigned long freq)
{
	struct utilization_data *data = stat->private_data;
	struct sgpu_governor_data *governor_data = data->devfreq->data;
	uint64_t current_time;
	unsigned long flags;

	current_time = sched_clock();
	spin_lock_irqsave(&data->lock, flags);

	trace_sgpu_devfreq_monitor(data->devfreq, governor_data->min_freq,
				   governor_data->max_freq,
				   current_time - data->trace_time);
	data->trace_time = current_time;
	spin_unlock_irqrestore(&data->lock, flags);
}

void sgpu_utilization_trace_after(struct devfreq_dev_status *stat, unsigned long freq)
{
	struct utilization_data *data = stat->private_data;
	struct sgpu_governor_data *governor_data = data->devfreq->data;
	struct amdgpu_device *adev = data->adev;
	uint64_t current_time;
	unsigned long flags;

	if (stat->current_frequency == freq)
		return;
	stat->current_frequency = freq;

	current_time = sched_clock();

	spin_lock_irqsave(&data->lock, flags);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "min_freq=%8lu, max_freq=%8lu, cur_freq=%8lu",
		 governor_data->min_freq, governor_data->max_freq,
		 stat->current_frequency);

	trace_sgpu_devfreq_monitor(data->devfreq, governor_data->min_freq,
				   governor_data->max_freq,
				   current_time - data->trace_time);
	data->trace_time = data->last_time = data->cu_last_time = current_time;
	sgpu_utilization_reset(stat);
	spin_unlock_irqrestore(&data->lock, flags);
}

int sgpu_utilization_src_change(struct devfreq *df, char *buf)
{
	int i;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = stat->private_data;
	int ret = -ENODEV;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	for (i = 0; i < SGPU_MAX_SRC_NUM; i++) {
		if (!strncmp(utilization_src_info[i].name, buf, DEVFREQ_NAME_LEN)) {
			data->utilization_src = &utilization_src_info[i];
			ret = 0;
			break;
		}
	}
	sgpu_utilization_reset(stat);
	spin_unlock_irqrestore(&data->lock, flags);
	return ret;
}

ssize_t sgpu_utilization_current_src_show(struct devfreq *df, char *buf)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = stat->private_data;
	ssize_t count;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	count = scnprintf(buf, PAGE_SIZE,
			  "%s", data->utilization_src->name);
	spin_unlock_irqrestore(&data->lock, flags);
	return count;
}

ssize_t sgpu_utilization_all_src_show(struct devfreq *df, char *buf)
{
	int i;
	ssize_t count = 0;

	for (i = 0; i < SGPU_MAX_SRC_NUM; i++) {
		struct sgpu_utilization_info *src = &utilization_src_info[i];
		count += scnprintf(&buf[count], (PAGE_SIZE - count - 2),
				   "%s ", src->name);
	}
	/* Truncate the trailing space */
	if (count)
		count--;

	count += sprintf(&buf[count], "\n");

	return count;
}

int sgpu_utilization_init(struct amdgpu_device *adev, struct devfreq *df)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = kzalloc(sizeof(struct utilization_data),
						GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	spin_lock_init(&data->lock);
	data->adev = adev;
	data->active = 0;
	data->cu_active = 0;
	data->devfreq = df;
	data->utilization_src = &utilization_src_info[SGPU_DVFS_SRC_JIFFIES];

	stat->private_data = data;

	return 0;
}

void sgpu_utilization_deinit(struct devfreq *df)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *data = stat->private_data;

	kfree(data);
	stat->private_data = NULL;
}
