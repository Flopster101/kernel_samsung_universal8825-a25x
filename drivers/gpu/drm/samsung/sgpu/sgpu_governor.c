/*
* @file sgpu_governor.c
* @copyright 2020 Samsung Electronics
*/

#include <linux/devfreq.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_governor.h"
#include "sgpu_utilization.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/cal-if.h>
#include <linux/notifier.h>
#include <soc/samsung/exynos-migov.h>
#include "exynos_gpu_interface.h"

#if IS_ENABLED(CONFIG_GPU_THERMAL)
#include "exynos_tmu.h"
#endif /*CONFIG_GPU_THERMAL */
#endif /* CONFIG_DRM_SGPU_EXYNOS */

static char *gpu_dvfs_min_threshold = "0 303000:60 404000:65 711000:78";
static char *gpu_dvfs_max_threshold = "75 303000:80 404000:85 605000:90 711000:95";
static char *gpu_dvfs_downstay_time = "32 500000:64 605000:96 903000:160";

/* get frequency and delay time data from string */
unsigned int *sgpu_get_array_data(struct devfreq_dev_profile *dp, const char *buf)
{
	const char *cp;
	int i, j;
	int ntokens = 1;
	unsigned int *tokenized_data, *array_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	array_data = kmalloc(dp->max_state * sizeof(unsigned int), GFP_KERNEL);
	if (!array_data) {
		err = -ENOMEM;
		goto err_kfree;
	}

	for (i = dp->max_state - 1, j = 0; i >= 0; i--) {
		while(j < ntokens - 1 && dp->freq_table[i] >= tokenized_data[j + 1])
			j += 2;
		array_data[i] = tokenized_data[j];
	}
	kfree(tokenized_data);

	return array_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

typedef enum {
	SGPU_DVFS_GOVERNOR_STATIC = 0,
	SGPU_DVFS_GOVERNOR_CONSERVATIVE,
	SGPU_DVFS_GOVERNOR_INTERACTIVE,
	SGPU_DVFS_GOVERNOR_AMIGO,
	SGPU_MAX_GOVERNOR_NUM,
} gpu_governor_type;

struct sgpu_governor_info {
	uint64_t	id;
	char		*name;
	int (*get_target)(struct devfreq *df, uint32_t *level);
	int (*clear)(struct devfreq *df, uint32_t level);
};

int exynos_amigo_is_joint_gov(struct devfreq *df)
{
	struct sgpu_governor_data *gdata = df->data;
	return (gdata->governor->id == SGPU_DVFS_GOVERNOR_AMIGO)? 1 : 0;
}

void sgpu_dvfs_governor_major_current(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	uint32_t lv_low = data->fine_grained_low_level;
	uint32_t lv_high = data->fine_grained_high_level;
	uint32_t step = data->fine_grained_step;

	if (*level > lv_low && *level < lv_high + 1)
		*level = *level - (*level - lv_low) % step;
}
void sgpu_dvfs_governor_major_level_up(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	uint32_t lv_low = data->fine_grained_low_level;
	uint32_t lv_high = data->fine_grained_high_level;
	uint32_t step = data->fine_grained_step;

	if (*level < lv_low)
		*level = *level + 1;
	else if (*level < lv_high + 1)
		*level = *level - (*level - lv_low) % step + step;
	else if (*level < df->profile->max_state - 1)
		*level = *level + 1;
}

void sgpu_dvfs_governor_major_level_down(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	uint32_t lv_low = data->fine_grained_low_level;
	uint32_t lv_high = data->fine_grained_high_level;
	uint32_t step = data->fine_grained_step;

	if (*level > lv_high + 1)
		*level = *level - 1;
	else if (*level > lv_low)
		*level = *level + (step - (*level - lv_low) % step) % step - step;
	else if (*level > 0)
		*level = *level - 1;
}

bool sgpu_dvfs_governor_major_level_check(struct devfreq *df, unsigned long freq)
{
	int level;

	if (!df || !df->profile || !df->profile->freq_table)
		return false;

	if (freq == 0 || freq == PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE)
		return true;

	level = df->profile->max_state - 1;
	sgpu_dvfs_governor_major_current(df, &level);

	while (level >= 0) {
		if (df->profile->freq_table[level] == freq)
			return true;

		if (level != 0)
			sgpu_dvfs_governor_major_level_down(df, &level);
		else
			break;
	}

	return false;
}

static uint64_t calc_utilization(struct devfreq *df)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct sgpu_governor_data *gdata = df->data;
	struct utilization_data *udata = stat->private_data;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];
	unsigned long cu_busy_time = sw_info->cu_busy_time;

	udata->last_util = div64_u64(cu_busy_time *
				      (gdata->compute_weight - 100) +
				      sw_info->busy_time * 100LL,
				     sw_info->total_time);

	if (udata->last_util > 100)
		udata->last_util = 100;

	udata->last_cu_util = div64_u64(cu_busy_time * 100LL, sw_info->total_time);

	if (udata->last_util && udata->last_util == udata->last_cu_util)
		gdata->cl_boost_status = true;
	else
		gdata->cl_boost_status = false;

	return udata->last_util;
}

#define NORMALIZE_SHIFT (10)
#define NORMALIZE_FACT  (1<<(NORMALIZE_SHIFT))
#define NORMALIZE_FACT3 (1<<((NORMALIZE_SHIFT)*3))
#define ITERATION_MAX	(10)

static uint64_t cube_root(uint64_t value)
{
	uint32_t index, iter;
	uint64_t cube, cur, prev = 0;

	if (value == 0)
		return 0;

	index = fls64(value);
	index = (index - 1)/3 + 1;

	/* Implementation of Newton-Raphson method for approximating
	   the cube root */
	iter = ITERATION_MAX;

	cur = (1 << index);
	cube = cur*cur*cur;

	while (iter) {
		if ((cube-value == 0) || (prev == cur))
			return cur;
		prev = cur;
		cur = (value + 2*cube) / (3*cur*cur);
		cube = cur*cur*cur;
		iter--;
	}

	return prev;
}

static int sgpu_conservative_get_threshold(struct devfreq *df,
					   uint32_t *max, uint32_t *min)
{
	struct sgpu_governor_data *gdata = df->data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];

	uint64_t coefficient, ratio;
	uint32_t power_ratio;
	unsigned long sw_busy_time;
	uint32_t max_threshold, min_threshold;


	sw_busy_time = sw_info->busy_time;
	power_ratio  = gdata->power_ratio;
	max_threshold = *max;
	min_threshold = *min;

	if (sw_busy_time == 0)
		coefficient = 1;
	else
		coefficient = div64_u64(sw_busy_time * 100 * NORMALIZE_FACT3,
					sw_busy_time * 100);

	if (coefficient == 1)
		ratio = NORMALIZE_FACT;
	else
		ratio = cube_root(coefficient);

	if(ratio == 0)
		ratio = NORMALIZE_FACT;

	*max = div64_u64(max_threshold * NORMALIZE_FACT, ratio);
	*min = div64_u64(min_threshold * NORMALIZE_FACT, ratio);

	trace_sgpu_utilization_sw_source_data(sw_info, power_ratio, ratio,
							NORMALIZE_FACT);
	trace_sgpu_governor_conservative_threshold(*max, *min, max_threshold,
						   min_threshold);

	return 0;

}

static int sgpu_dvfs_governor_conservative_get_target(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	uint32_t max_threshold = data->max_thresholds[*level];
	uint32_t min_threshold = data->min_thresholds[*level];
	uint64_t utilization = calc_utilization(df);

	if (df->previous_freq < data->highspeed_freq &&
	    utilization > data->highspeed_load) {
		if (time_after(jiffies, data->expire_highspeed_delay)) {
			*level = data->highspeed_level;
			return 0;
		}
	} else {
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);
	}

	if (udata->utilization_src->hw_source_valid) {
		sgpu_conservative_get_threshold(df, &max_threshold,
						&min_threshold);
	}

	if (utilization > max_threshold &&
	    *level > 0) {
		sgpu_dvfs_governor_major_level_down(df, level);
	} else if (utilization < min_threshold) {
		if (time_after(jiffies, data->expire_jiffies) &&
		    *level < df->profile->max_state - 1 ) {
			sgpu_dvfs_governor_major_level_up(df, level);
		}
	} else {
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[*level]);
	}
	return 0;
}

static int sgpu_dvfs_governor_conservative_clear(struct devfreq *df, uint32_t level)
{
	struct sgpu_governor_data *data = df->data;

	data->expire_jiffies = jiffies +
		msecs_to_jiffies(data->downstay_times[level]);
	if (data->current_level == level ||
	    (data->current_level >= data->highspeed_level && level < data->highspeed_level))
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);

	return 0;
}


unsigned long sgpu_interactive_target_freq(struct devfreq *df,
					   uint64_t utilization,
					   uint32_t target_load)
{
	struct sgpu_governor_data *gdata = df->data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];

	unsigned long target_freq = 0;
	uint64_t coefficient, freq_ratio;
	uint32_t power_ratio, new_target_load;
	unsigned long sw_busy_time;


	sw_busy_time = sw_info->busy_time;

	power_ratio  = gdata->power_ratio;

	if (sw_busy_time == 0)
		coefficient = NORMALIZE_FACT3;
	else
		coefficient = div64_u64(sw_busy_time * 100 * NORMALIZE_FACT3,
					sw_busy_time * 100);

	freq_ratio = cube_root(coefficient);

	if(freq_ratio == 0)
		freq_ratio = NORMALIZE_FACT;

	target_freq = div64_u64(freq_ratio * utilization * df->previous_freq,
				target_load * NORMALIZE_FACT);

	new_target_load = div64_u64(target_load * NORMALIZE_FACT, freq_ratio);

	trace_sgpu_utilization_sw_source_data(sw_info, power_ratio,
					      freq_ratio, NORMALIZE_FACT);
	trace_sgpu_governor_interactive_freq(df, utilization, target_load,
					     target_freq, new_target_load);

	return target_freq;
}

static int sgpu_dvfs_governor_interactive_get_target(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	unsigned long target_freq;
	uint32_t target_load, next_major_level, cur_major_level;
	uint64_t utilization = calc_utilization(df);

	if (df->previous_freq < data->highspeed_freq &&
	    utilization > data->highspeed_load) {
		if (time_after(jiffies, data->expire_highspeed_delay)) {
			*level = data->highspeed_level;
			return 0;
		}
	} else {
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);
	}
	target_load = data->max_thresholds[*level];

	if (udata->utilization_src->hw_source_valid)
		target_freq = sgpu_interactive_target_freq(df, utilization,
							   target_load);
	else
		target_freq = div64_u64(utilization * df->previous_freq,
					target_load);

	if (target_freq > df->previous_freq) {
		while (df->profile->freq_table[*level] < target_freq && *level > 0)
			sgpu_dvfs_governor_major_level_down(df, level);

		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[*level]);
	} else {
		while (df->profile->freq_table[*level] > target_freq &&
		       *level < df->profile->max_state - 1)
			sgpu_dvfs_governor_major_level_up(df, level);
		if (df->profile->freq_table[*level] < target_freq)
			sgpu_dvfs_governor_major_level_down(df, level);

		next_major_level = data->current_level;
		sgpu_dvfs_governor_major_level_up(df, &next_major_level);
		cur_major_level = next_major_level;
		sgpu_dvfs_governor_major_level_down(df, &cur_major_level);

		if (*level > next_major_level) {
			target_load = data->max_thresholds[*level];
			if (div64_u64(utilization *
				      df->profile->freq_table[cur_major_level],
				      df->profile->freq_table[*level]) > target_load) {
				sgpu_dvfs_governor_major_level_down(df, level);
			}
		}

		if (*level == cur_major_level) {
			data->expire_jiffies = jiffies +
				msecs_to_jiffies(data->downstay_times[*level]);
		} else if (time_before(jiffies, data->expire_jiffies)) {
			*level = cur_major_level;
			return 0;
		}
	}


	return 0;
}

static int sgpu_dvfs_governor_interactive_clear(struct devfreq *df, uint32_t level)
{
	struct sgpu_governor_data *data = df->data;
	int target_load;
	uint64_t downstay_jiffies;

	target_load = data->max_thresholds[level];
	downstay_jiffies = msecs_to_jiffies(data->downstay_times[level]);

	if (level > data->current_level && df->profile->freq_table[level] != data->max_freq)
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->valid_time);
	else
		data->expire_jiffies = jiffies + downstay_jiffies;
	if (data->current_level == level ||
	    (data->current_level >= data->highspeed_level && level < data->highspeed_level))
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);

	return 0;
}

static int sgpu_dvfs_governor_static_get_target(struct devfreq *df, uint32_t *level)
{
	static uint32_t updown = 0;
	struct sgpu_governor_data *data = df->data;

	if (!(updown & 0x1)) {
		if (df->profile->freq_table[*level] < data->max_freq && *level > 0)
			sgpu_dvfs_governor_major_level_down(df, level);
	} else {
		if (df->profile->freq_table[*level] > data->min_freq &&
		    *level < df->profile->max_state - 1)
			sgpu_dvfs_governor_major_level_up(df, level);
	}
	if (data->current_level == *level) {
		/* change up and down direction */
		if ((updown & 0x1)) {
			if (df->profile->freq_table[*level] < data->max_freq && *level > 0)
				sgpu_dvfs_governor_major_level_down(df, level);
		} else {
			if (df->profile->freq_table[*level] > data->min_freq &&
			    *level < df->profile->max_state - 1)
				sgpu_dvfs_governor_major_level_up(df, level);
		}
		if (data->current_level != *level) {
			/* increase direction change count */
			updown++;
		}
	}

	return 0;
}

static uint32_t weight_table[WEIGHT_TABLE_MAX_SIZE][WINDOW_MAX_SIZE + 1] = {
	{  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4,  312},
	{ 100,  10,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,  111},
	{ 200,  40,   8,   2,   1,   0,   0,   0,   0,   0,   0,   0,  251},
	{ 300,  90,  27,   8,   2,   1,   0,   0,   0,   0,   0,   0,  428},
	{ 400, 160,  64,  26,  10,   4,   2,   1,   0,   0,   0,   0,  667},
	{ 500, 250, 125,  63,  31,  16,   8,   4,   2,   1,   0,   0, 1000},
	{ 600, 360, 216, 130,  78,  47,  28,  17,  10,   6,   4,   2, 1498},
	{ 700, 490, 343, 240, 168, 118,  82,  58,  40,  28,  20,  14, 2301},
	{ 800, 640, 512, 410, 328, 262, 210, 168, 134, 107,  86,  69, 3726},
	{ 900, 810, 729, 656, 590, 531, 478, 430, 387, 349, 314, 282, 6456},
	{  48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4,  312}
};

uint64_t sgpu_weight_prediction_utilization(struct devfreq *df, uint64_t utilization)
{
	struct sgpu_governor_data *data = df->data;
	unsigned long cur_freq = df->profile->freq_table[data->current_level];
	unsigned long max_freq = df->profile->freq_table[0];
	uint64_t weight_util[WEIGHT_TABLE_IDX_NUM];
	uint64_t normalized_util, util_conv;
	uint32_t window_idx, table_row, table_col;
	uint32_t i, j;

	normalized_util = ((utilization * cur_freq) << NORMALIZE_SHIFT) / max_freq;

	window_idx = data->window_idx;
	data->window_idx = (window_idx + 1) % WINDOW_MAX_SIZE;
	data->window[window_idx] = normalized_util;

	for (i = 0; i < WEIGHT_TABLE_IDX_NUM; i++) {
		weight_util[i] = 0;
		table_row = data->weight_table_idx[i];
		table_col = WINDOW_MAX_SIZE - 1;

		for(j = window_idx+1; j <= window_idx + WINDOW_MAX_SIZE; j++){
			weight_util[i] += data->window[j%WINDOW_MAX_SIZE] *
					weight_table[table_row][table_col--];
		}
		weight_util[i] /= weight_table[table_row][WINDOW_MAX_SIZE];
	}

	for (i = 1; i < WEIGHT_TABLE_IDX_NUM; i++)
		weight_util[0] = max(weight_util[0], weight_util[i]);
	util_conv = weight_util[0] * max_freq / cur_freq;

	return util_conv;
}

static int sgpu_dvfs_governor_amigo_get_target(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	unsigned long cur_freq = df->profile->freq_table[*level];
	unsigned long max_freq = df->profile->freq_table[0];
	unsigned long target_freq;
	uint64_t weight_util, utilT;
	uint64_t utilization = calc_utilization(df);
	long target_freq_signed;

	weight_util = sgpu_weight_prediction_utilization(df, utilization);
	utilT = ((weight_util) * cur_freq / 100 ) >> NORMALIZE_SHIFT;
	target_freq_signed = (long)utilT + (long)((max_freq - utilT) / 1000) * (long)data->freq_margin;

	if (target_freq_signed < 0)
		target_freq = 0;
	else
		target_freq = target_freq_signed;

	if (target_freq > cur_freq) {
		while (df->profile->freq_table[*level] < target_freq && *level > 0)
			sgpu_dvfs_governor_major_level_down(df, level);
	} else {
		while (df->profile->freq_table[*level] > target_freq &&
		      *level < df->profile->max_state - 1)
			sgpu_dvfs_governor_major_level_up(df, level);
		if (df->profile->freq_table[*level] < target_freq)
			sgpu_dvfs_governor_major_level_down(df, level);
	}

	return 0;
}


static struct sgpu_governor_info governor_info[SGPU_MAX_GOVERNOR_NUM] = {
	{
		SGPU_DVFS_GOVERNOR_STATIC,
		"static",
		sgpu_dvfs_governor_static_get_target,
		NULL,
	},
	{
		SGPU_DVFS_GOVERNOR_CONSERVATIVE,
		"conservative",
		sgpu_dvfs_governor_conservative_get_target,
		sgpu_dvfs_governor_conservative_clear,
	},
	{
		SGPU_DVFS_GOVERNOR_INTERACTIVE,
		"interactive",
		sgpu_dvfs_governor_interactive_get_target,
		sgpu_dvfs_governor_interactive_clear,
	},
	{
		SGPU_DVFS_GOVERNOR_AMIGO,
		"amigo",
		sgpu_dvfs_governor_amigo_get_target,
		NULL,
	},
};

#if IS_ENABLED(CONFIG_GPU_THERMAL)
static bool sgpu_governor_local_minlock_change(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->data;
	struct utilization_data *udata = df->last_status.private_data;
	unsigned long orig_target = df->profile->freq_table[*level];
	int cur_temp = 0;
	bool ret = false;

	if (df->profile->freq_table[*level] >=
	    df->profile->freq_table[data->local_minlock_level]) {
		return ret;
	}

	/* GPU TZID=3 */
	cur_temp = exynos_tmu_extern_get_temp(data->tmu_id);

	if (cur_temp >= data->local_minlock_temp &&
	    udata->last_util >= data->local_minlock_util) {
		*level = data->local_minlock_level;
		ret = true;

		if (!data->local_minlock_status) {
			DRM_INFO("%s: orig_target=%lu, min_target=%lu",
				 __func__, orig_target,
				 df->profile->freq_table[*level]);
		}
	}

	return ret;
}
#endif /* CONFIG_GPU_THERMAL */

void exynos_sdp_set_cur_freqlv(int id, int level);
static int devfreq_sgpu_func(struct devfreq *df, unsigned long *freq)
{
	int err = 0;
	struct sgpu_governor_data *data = df->data;
	struct utilization_data *udata = df->last_status.private_data;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];
	struct device *dev= df->dev.parent;
	uint32_t level = data->current_level;
	struct dev_pm_opp *target_opp;
	int32_t qos_min_freq, qos_max_freq;

	qos_max_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MAX_FREQUENCY);
	qos_min_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MIN_FREQUENCY);

	data->max_freq = min(df->scaling_max_freq,
			     (unsigned long)HZ_PER_KHZ * qos_max_freq);

	target_opp = devfreq_recommended_opp(dev, &data->max_freq,
					     DEVFREQ_FLAG_LEAST_UPPER_BOUND);
	if (IS_ERR(target_opp)) {
		dev_err(dev, "max_freq: not found valid OPP table\n");
		return PTR_ERR(target_opp);
	}
	dev_pm_opp_put(target_opp);

	data->min_freq = max(df->scaling_min_freq,
			      (unsigned long)HZ_PER_KHZ * qos_min_freq);
	data->min_freq = min(data->max_freq, data->min_freq);

	if (data->in_suspend) {
		*freq = max(data->min_freq, min(data->max_freq,	df->resume_freq));
		df->resume_freq = *freq;
		df->suspend_freq = 0;
		return 0;
	}

	err = df->profile->get_dev_status(df->dev.parent, &df->last_status);
	if (err)
		return err;

	if (sw_info->prev_total_time) {
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_notify_utilization();
#endif
		data->governor->get_target(df, &level);
	}

	if (!data->cl_boost_disable && !data->mm_min_clock &&
	    data->cl_boost_status) {
		level = data->cl_boost_level;
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[level]);
	}

#if IS_ENABLED(CONFIG_GPU_THERMAL)
	if (data->local_minlock_temp > 0)
		data->local_minlock_status =
			sgpu_governor_local_minlock_change(df, &level);
	else
		data->local_minlock_status = false;
#endif /* CONFIG_GPU_THERMAL */

	*freq = df->profile->freq_table[level];
	exynos_sdp_set_cur_freqlv(MIGOV_GPU, level);

	return err;
}

static int sgpu_governor_notifier_call(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	struct sgpu_governor_data *data = container_of(nb, struct sgpu_governor_data,
						       nb_trans);
	struct devfreq *df = data->devfreq;
	struct drm_device *ddev = adev_to_drm(data->adev);
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)ptr;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];

	/* in suspend or power_off*/
	if (ddev->switch_power_state == DRM_SWITCH_POWER_OFF ||
	    ddev->switch_power_state == DRM_SWITCH_POWER_DYNAMIC_OFF)
		return NOTIFY_DONE;

	if (freqs->old == freqs->new && !sw_info->prev_total_time)
		return NOTIFY_DONE;

	switch (event) {
	case DEVFREQ_PRECHANGE:
		sgpu_utilization_trace_before(&df->last_status, freqs->new);
		break;
	case DEVFREQ_POSTCHANGE:
		sgpu_utilization_trace_after(&df->last_status, freqs->new);
		if (data->governor->clear && freqs->old != freqs->new)
			data->governor->clear(df, data->current_level);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int devfreq_sgpu_handler(struct devfreq *df, unsigned int event, void *data)
{
	struct sgpu_governor_data *governor_data = df->data;
	int ret = 0;

	mutex_lock(&governor_data->lock);

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = sgpu_utilization_init(governor_data->adev, df);
		if (ret)
			goto out;

		governor_data->nb_trans.notifier_call = sgpu_governor_notifier_call;
		devm_devfreq_register_notifier(df->dev.parent, df, &governor_data->nb_trans,
					       DEVFREQ_TRANSITION_NOTIFIER);
		sgpu_utilization_trace_start(&df->last_status);
		if (governor_data->governor->clear)
			governor_data->governor->clear(df, governor_data->current_level);
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(0);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		devfreq_monitor_start(df);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(df);
		devm_devfreq_unregister_notifier(df->dev.parent, df, &governor_data->nb_trans,
						 DEVFREQ_TRANSITION_NOTIFIER);
		sgpu_utilization_deinit(df);
		break;
	case DEVFREQ_GOV_UPDATE_INTERVAL:
		devfreq_update_interval(df, (unsigned int*)data);
		break;
	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(df);
		if (governor_data->wakeup_lock)
			df->resume_freq = df->previous_freq;
		else
			df->resume_freq = 0;
		sgpu_utilization_trace_stop(&df->last_status);
		governor_data->in_suspend = true;
		governor_data->cl_boost_status = false;
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(df->previous_freq);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		break;
	case DEVFREQ_GOV_RESUME:
		governor_data->in_suspend = false;
		if (df->suspend_freq == 0)
			df->suspend_freq = df->profile->initial_freq;
		sgpu_utilization_trace_start(&df->last_status);
		if (governor_data->governor->clear)
			governor_data->governor->clear(df, governor_data->current_level);
		devfreq_monitor_resume(df);
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(0);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		break;
	default:
		break;
	}

out:
	mutex_unlock(&governor_data->lock);
	return ret;
}

static struct devfreq_governor devfreq_governor_sgpu = {
	.name = "sgpu_governor",
	.get_target_freq = devfreq_sgpu_func,
	.event_handler = devfreq_sgpu_handler,
	.immutable = true,
};

ssize_t sgpu_governor_all_info_show(struct devfreq *df, char *buf)
{
	int i;
	ssize_t count = 0;
	if (!df->governor || !df->data)
		return -EINVAL;

	for (i = 0; i < SGPU_MAX_GOVERNOR_NUM; i++) {
		struct sgpu_governor_info *governor = &governor_info[i];
		count += scnprintf(&buf[count], (PAGE_SIZE - count - 2),
				   "%s ", governor->name);
	}
	/* Truncate the trailing space */
	if (count)
		count--;

	count += sprintf(&buf[count], "\n");

	return count;
}

ssize_t sgpu_governor_current_info_show(struct devfreq *df, char *buf)
{
	struct sgpu_governor_data *data = df->data;

	return scnprintf(buf, PAGE_SIZE, "%s", data->governor->name);
}

int sgpu_governor_change(struct devfreq *df, char *str_governor)
{
	int i;
	struct sgpu_governor_data *data = df->data;

	for (i = 0; i < SGPU_MAX_GOVERNOR_NUM; i++) {
		if (!strncmp(governor_info[i].name, str_governor, DEVFREQ_NAME_LEN)) {
			mutex_lock(&data->lock);
			if (!data->in_suspend)
				devfreq_monitor_stop(df);
			data->governor = &governor_info[i];
			if (!data->in_suspend)
				devfreq_monitor_start(df);
			mutex_unlock(&data->lock);
			return 0;
		}
	}

	return -ENODEV;
}

#define DVFS_TABLE_ROW_MAX			1
#define DEFAULT_GOVERNOR			SGPU_DVFS_GOVERNOR_CONSERVATIVE
#define DEFAULT_POLLING_MS			32
#define DEFAULT_VALID_TIME			32
#define DEFAULT_INITIAL_FREQ			26000
#define DEFAULT_HIGHSPEED_FREQ			500000
#define DEFAULT_HIGHSPEED_LOAD			99
#define DEFAULT_HIGHSPEED_DELAY			0
#define DEFAULT_POWER_RATIO			50
#define DEFAULT_CL_BOOST_FREQ			1210000
#define DEFAULT_LOCAL_MINLOCK_FREQ		404000
#define DEFAULT_LOCAL_MINLOCK_UTIL		0
#define DEFAULT_LOCAL_MINLOCK_TEMP		65
#define DEFAULT_FINE_GRAINED_LOW_FREQ		0
#define DEFAULT_FINE_GRAINED_HIGH_FREQ		1210000
#define DEFAULT_FINE_GRAINED_STEP_UNIT		10
#define DEFAULT_MINLOCK_LIMIT_FREQ		1306000

int sgpu_governor_init(struct device *dev, struct devfreq_dev_profile *dp,
		       struct sgpu_governor_data **governor_data)
{
	struct sgpu_governor_data *data;
	int ret = 0, i, j, k;
	uint32_t *array;
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;
#ifdef CONFIG_DRM_SGPU_EXYNOS
	uint32_t dt_freq;
	unsigned long max_freq, min_freq;
	struct dvfs_rate_volt *g3d_rate_volt = NULL;
	struct dvfs_rate_volt *major_table = NULL;
	int cal_get_dvfs_lv_num;
	int cal_table_size;
	unsigned long cal_maxfreq, cal_minfreq;
	unsigned long cur_freq;
#if IS_ENABLED(CONFIG_GPU_THERMAL)
	struct device_node *gpu_tmu;
	uint32_t gpu_tmuid;
#endif /* CONFIG_GPU_THERMAL */
#endif /* CONFIG_DRM_SGPU_EXYNOS*/

	dp->initial_freq = DEFAULT_INITIAL_FREQ;
	dp->polling_ms = DEFAULT_POLLING_MS;
	dp->max_state = DVFS_TABLE_ROW_MAX;
	data = kzalloc(sizeof(struct sgpu_governor_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	*governor_data = data;
	data->governor = &governor_info[DEFAULT_GOVERNOR];
	data->highspeed_freq = DEFAULT_HIGHSPEED_FREQ;
	data->highspeed_load = DEFAULT_HIGHSPEED_LOAD;
	data->highspeed_delay = DEFAULT_HIGHSPEED_DELAY;
	data->wakeup_lock = true;
	data->valid_time = DEFAULT_VALID_TIME;
	data->in_suspend = false;
	data->adev = adev;
	data->power_ratio = DEFAULT_POWER_RATIO;

	data->freq_margin = 10;
	data->window_idx = 0;
	for (i = 0; i < WINDOW_MAX_SIZE; i++)
		data->window[i] = 0;
	for (i = 0; i < WEIGHT_TABLE_IDX_NUM; i++)
		data->weight_table_idx[i] = 0;

	data->cl_boost_disable = false;
	data->cl_boost_status = false;
	data->cl_boost_level = 0;
	data->mm_min_clock = 0;

	data->fine_grained_step = DEFAULT_FINE_GRAINED_STEP_UNIT;
	data->fine_grained_low_level = 0;
	data->fine_grained_high_level = 0;
	data->major_state = 0;

	data->minlock_limit = false;
	data->minlock_limit_freq = PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE;

	mutex_init(&data->lock);

#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_GPU_THERMAL)
	gpu_tmu = of_parse_phandle(adev->pldev->dev.of_node, "gpu_tmu", 0);
	ret = of_property_read_u32(gpu_tmu, "id", &gpu_tmuid);
	if (ret)
		data->tmu_id = -1;
	else
		data->tmu_id = gpu_tmuid;

	data->local_minlock_util = DEFAULT_LOCAL_MINLOCK_UTIL;
	data->local_minlock_temp = DEFAULT_LOCAL_MINLOCK_TEMP;
	data->local_minlock_status = false;
#endif /* CONFIG_GPU_THERMAL */

	cal_get_dvfs_lv_num = cal_dfs_get_lv_num(adev->cal_id);
	dp->max_state = cal_get_dvfs_lv_num;

	g3d_rate_volt = kzalloc(sizeof(struct dvfs_rate_volt) * cal_get_dvfs_lv_num,
				GFP_KERNEL);
	if (!g3d_rate_volt) {
		ret = -ENOMEM;
		goto err_kfree1;
	}

	major_table = kzalloc(sizeof(struct dvfs_rate_volt) * cal_get_dvfs_lv_num,
				     GFP_KERNEL);
	if (!major_table) {
		ret = -ENOMEM;
		goto err_kfree1;
	}

	cal_table_size = cal_dfs_get_rate_asv_table(adev->cal_id, g3d_rate_volt);
	dp->initial_freq = cal_dfs_get_boot_freq(adev->cal_id);
	cal_maxfreq = cal_dfs_get_max_freq(adev->cal_id);
	cal_minfreq = cal_dfs_get_min_freq(adev->cal_id);

	ret = of_property_read_u32(dev->of_node, "compute_weight",
				   &data->compute_weight);
	if (ret)
		data->compute_weight = 100;

	ret = of_property_read_u32(dev->of_node, "max_freq", &dt_freq);
	if (!ret) {
		max_freq = (unsigned long)dt_freq;
		max_freq = min(max_freq, cal_maxfreq);
	} else {
		max_freq = cal_maxfreq;
	}

	ret = of_property_read_u32(dev->of_node, "min_freq", &dt_freq);
	if (!ret) {
		min_freq = (unsigned long)dt_freq;
		min_freq = max(min_freq, cal_minfreq);
	} else {
		min_freq = cal_minfreq;
	}

	min_freq = min(max_freq, min_freq);

	adev->gpu_dss_freq_id = 0;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	adev->gpu_dss_freq_id = dbg_snapshot_get_freq_idx("G3D");
#endif

#endif /* CONFIG_DRM_SGPU_EXYNOS */

	dp->freq_table = kzalloc(sizeof(*(dp->freq_table)) *
				 (dp->max_state * data->fine_grained_step),
				 GFP_KERNEL);
	if (!dp->freq_table) {
		ret = -ENOMEM;
		goto err_kfree1;
	}

	for (i = 0, j = 0; i < dp->max_state; i++) {
		uint32_t freq, volt;

#ifdef CONFIG_DRM_SGPU_EXYNOS
		if (g3d_rate_volt[i].rate > max_freq ||
		    g3d_rate_volt[i].rate < min_freq)
			continue;

		freq =  g3d_rate_volt[i].rate;
		volt =  g3d_rate_volt[i].volt;
		major_table[data->major_state].rate = freq;
		major_table[data->major_state].volt = volt;
#else
		freq = dp->initial_freq;
		volt = 0;
#endif

		if (freq >= dp->initial_freq) {
			data->current_level = j;
		}

		if (freq >= data->highspeed_freq) {
			data->highspeed_level = j;
		}
#if IS_ENABLED(CONFIG_GPU_THERMAL)
		if (freq >= DEFAULT_LOCAL_MINLOCK_FREQ)
			data->local_minlock_level = j;
#endif /* CONFIG_GPU_THERMAL */

		if (freq >= DEFAULT_CL_BOOST_FREQ)
			data->cl_boost_level = j;

		if (freq >= DEFAULT_FINE_GRAINED_HIGH_FREQ)
			data->fine_grained_low_level = j;

		if (freq >= DEFAULT_MINLOCK_LIMIT_FREQ) {
			data->minlock_limit = true;
			data->minlock_limit_freq = freq;
		}

		dp->freq_table[j] = freq;
		ret = dev_pm_opp_add(dev, freq, volt);
		if (ret) {
			dev_err(dev, "failed to add opp entries\n");
			goto err_kfree2;
		}
		j++;
		data->major_state++;

		if ((i < dp->max_state - 1) &&
		    (g3d_rate_volt[i + 1].rate >= min_freq) &&
		    freq <= DEFAULT_FINE_GRAINED_HIGH_FREQ &&
		    freq >= DEFAULT_FINE_GRAINED_LOW_FREQ) {
			unsigned long step = (g3d_rate_volt[i].rate -
					      g3d_rate_volt[i+1].rate) /
						data->fine_grained_step;
			for (k = 1; k < data->fine_grained_step; k++) {
				unsigned long nomalize_freq =
					(freq - k * step + HZ_PER_KHZ / 2) / HZ_PER_KHZ * HZ_PER_KHZ;
				dp->freq_table[j] = nomalize_freq;
				ret = dev_pm_opp_add(dev, nomalize_freq, volt);
				if (ret) {
					dev_err(dev, "failed to add opp entries\n");
					goto err_kfree2;
				}
				j++;
			}
			data->fine_grained_high_level = j - 1;
		}
	}
	dp->max_state = j;
	dp->initial_freq = dp->freq_table[data->current_level];

#ifdef CONFIG_DRM_SGPU_EXYNOS
	gpu_dvfs_init_table(major_table, data->major_state);
	gpu_dvfs_init_utilization_notifier_list();

	cur_freq = cal_dfs_cached_get_rate(adev->cal_id);

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, dp->initial_freq, DSS_FLAG_IN);
#endif
	cal_dfs_set_rate(adev->cal_id, dp->initial_freq);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, dp->initial_freq, DSS_FLAG_OUT);
#endif
	cur_freq = cal_dfs_cached_get_rate(adev->cal_id);

#ifndef CONFIG_SOC_S5E9925_EVT0
	data->bg3d_dvfs = devm_ioremap(dev, 0x16cd0000, 0x1000);
	/* BG3D_DVFS_CTL (EVT1) 0x058 enable */
	writel(0x1, data->bg3d_dvfs + 0x58);
#endif /*CONFIG_SOC_S5E9925_EVT0 */
#endif

	array = sgpu_get_array_data(dp, gpu_dvfs_min_threshold);
	if (IS_ERR(array)) {
		ret = PTR_ERR(array);
		dev_err(dev, "fail minimum threshold tokenized %d\n", ret);
		goto err_kfree2;
	}
	data->min_thresholds = array;

	array = sgpu_get_array_data(dp, gpu_dvfs_max_threshold);
	if (IS_ERR(array)) {
		ret = PTR_ERR(array);
		dev_err(dev, "fail maximum threshold tokenized %d\n", ret);
		goto err_kfree3;
	}
	data->max_thresholds = array;

	array = sgpu_get_array_data(dp, gpu_dvfs_downstay_time);
	if (IS_ERR(array)) {
		ret = PTR_ERR(array);
		dev_err(dev, "fail down stay time tokenized %d\n", ret);
		goto err_kfree4;
	}
	data->downstay_times = array;

	ret = devfreq_add_governor(&devfreq_governor_sgpu);
	if (ret) {
		dev_err(dev, "failed to add governor %d\n", ret);
		goto err_kfree5;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
	kfree(g3d_rate_volt);
	kfree(major_table);
#endif

	return ret;
err_kfree5:
	kfree(data->downstay_times);
err_kfree4:
	kfree(data->max_thresholds);
err_kfree3:
	kfree(data->min_thresholds);
err_kfree2:
	kfree(dp->freq_table);
err_kfree1:
#ifdef CONFIG_DRM_SGPU_EXYNOS
	kfree(g3d_rate_volt);
	kfree(major_table);
#endif
	kfree(data);
err:
	return ret;
}

void sgpu_governor_deinit(struct devfreq *df)
{
	int ret = 0;
	struct sgpu_governor_data *data = df->data;

	mutex_destroy(&data->lock);
	kfree(df->profile->freq_table);
	kfree(df->data);
	ret = devfreq_remove_governor(&devfreq_governor_sgpu);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}
