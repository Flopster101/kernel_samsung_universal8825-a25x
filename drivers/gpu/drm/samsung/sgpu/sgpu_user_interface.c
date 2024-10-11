/*
 * @file sgpu_user_interface.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Contains functions for user interface and sysfs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/devfreq.h>
#include "sgpu_governor.h"
#include "sgpu_user_interface.h"
#include "sgpu_utilization.h"
#include "amdgpu.h"
#include "exynos_gpu_interface.h"

static ssize_t dvfs_table_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	int i;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE,
			  "      freq voltage min max down(ms)\n");
	for (i = 0; i < df->profile->max_state; i++) {
		struct dev_pm_opp *target_opp;
		unsigned long freq = df->profile->freq_table[i];
		unsigned long volt;
		target_opp = devfreq_recommended_opp(dev->parent, &freq, 0);
		if (freq != df->profile->freq_table[i] || IS_ERR(target_opp)) {
			dev_pm_opp_put(target_opp);
			continue;
		}
		volt = dev_pm_opp_get_voltage(target_opp);
		dev_pm_opp_put(target_opp);
		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%10lu %7lu %3u %3u %7u\n",
				   freq, volt,
				   data->min_thresholds[i], data->max_thresholds[i],
				   data->downstay_times[i]);
	}

	return count;
}
static DEVICE_ATTR_RO(dvfs_table);

static ssize_t tokenized_show(char *buf, struct devfreq_dev_profile *dp, int *array)
{
	ssize_t count = 0;
	int i;
	int value;
	value = array[dp->max_state - 1];
	count += scnprintf(&buf[count], PAGE_SIZE - count, "%d ", value);
	for (i = dp->max_state - 2; i >= 0; i--) {
		if (array[i] != value) {
			value = array[i];
			count += scnprintf(&buf[count], PAGE_SIZE - count, "%lu:%d ",
					   dp->freq_table[i],
					   array[i]);
		}
	}
	if (count)
		count--;
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t min_thresholds_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return tokenized_show(buf, df->profile, data->min_thresholds);
}

static ssize_t min_thresholds_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	int *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail min_thresholds tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->min_thresholds);
	data->min_thresholds = array;
	mutex_unlock(&data->lock);
	return count;
}
static DEVICE_ATTR_RW(min_thresholds);

static ssize_t max_thresholds_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return tokenized_show(buf, df->profile, data->max_thresholds);
}

static ssize_t max_thresholds_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	int *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail max_thresholds tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->max_thresholds);
	data->max_thresholds = array;
	mutex_unlock(&data->lock);
	return count;
}
static DEVICE_ATTR_RW(max_thresholds);

static ssize_t downstay_times_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return tokenized_show(buf, df->profile, data->downstay_times);
}

static ssize_t downstay_times_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail downstay_times tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->downstay_times);
	data->downstay_times = array;
	mutex_unlock(&data->lock);

	return count;
}
static DEVICE_ATTR_RW(downstay_times);

static ssize_t available_utilization_sources_show(struct device *dev,
						  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);

	return sgpu_utilization_all_src_show(df, buf);
}
static DEVICE_ATTR_RO(available_utilization_sources);

static ssize_t utilization_source_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	ssize_t count;

	count = sgpu_utilization_current_src_show(df, buf);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t utilization_source_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	int ret;
	char str_utilization_source[DEVFREQ_NAME_LEN + 1];

	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_utilization_source);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_utilization_src_change(df, str_utilization_source);
	if (ret) {
		dev_warn(dev, "%s: utilization_source %s not started(%d)\n",
			 __func__, str_utilization_source, ret);
		return ret;
	}

	if (!ret) {
		sgpu_utilization_current_src_show(df, str_utilization_source);
		dev_info(dev, "utilization_source : %s\n", str_utilization_source);
		ret = count;
	}
	return ret;
}
static DEVICE_ATTR_RW(utilization_source);

static ssize_t valid_time_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return scnprintf(buf, PAGE_SIZE, "%u\n", data->valid_time);
}

static ssize_t valid_time_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t value;
	int ret;

	if (!df->governor)
		return -EINVAL;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->valid_time = value;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(valid_time);

static ssize_t max_freq_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
			 data->sys_max_freq);
}

static ssize_t max_freq_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	struct amdgpu_device *adev = data->adev;
	unsigned long value;
	int ret;

	ret = sscanf(buf, "%lu", &value);
	if (ret != 1)
		return -EINVAL;

	if (!dev_pm_qos_request_active(&data->sys_pm_qos_max))
		return -EINVAL;

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST sys_pm_qos=%lu", value);
	DRM_INFO("[sgpu] MAX_REQUEST sys_pm_qos=%lu", value);

	if (sgpu_dvfs_governor_major_level_check(df, value)) {
		dev_pm_qos_update_request(&data->sys_pm_qos_max, value / HZ_PER_KHZ);
		data->sys_max_freq = value;
	} else
		return -EINVAL;

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(max_freq);

static ssize_t min_freq_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
			 data->sys_min_freq);
}

static ssize_t min_freq_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	struct amdgpu_device *adev = data->adev;
	unsigned long value;
	int ret;

	ret = sscanf(buf, "%lu", &value);
	if (ret != 1)
		return -EINVAL;

	if (!dev_pm_qos_request_active(&data->sys_pm_qos_min))
		return -EINVAL;

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST sys_pm_qos=%lu", value);
	DRM_INFO("[sgpu] MIN_REQUEST sys_pm_qos=%lu", value);

	if (sgpu_dvfs_governor_major_level_check(df, value)) {
		if (data->minlock_limit && value >= data->minlock_limit_freq)
			return -EINVAL;
		dev_pm_qos_update_request(&data->sys_pm_qos_min, value / HZ_PER_KHZ);
		data->sys_min_freq = value;
	} else
		return -EINVAL;

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(min_freq);

static ssize_t wakeup_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t value;
	int ret;

	if (!df->governor)
		return -EINVAL;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->wakeup_lock = value ? true : false;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;
}

static ssize_t wakeup_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	return scnprintf(buf, PAGE_SIZE, "%d\n", data->wakeup_lock);
}
static DEVICE_ATTR_RW(wakeup);

static ssize_t highspeed_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_profile *dp = df->profile;
	struct sgpu_governor_data *data = df->data;
	unsigned long freq, i;

	if (sscanf(buf, "%lu", &freq) != 1)
		return -EINVAL;

	data->highspeed_freq = freq;
	data->highspeed_level = 0;
	for(i = 1; i < dp->max_state; i++) {
		if(dp->freq_table[i] >= data->highspeed_freq)
			data->highspeed_level = i;
		else
			break;
	}

	buf = strpbrk(buf, " ");
	buf++;
	if (sscanf(buf, "%u", &data->highspeed_load) != 1)
		return -EINVAL;
	buf = strpbrk(buf, " ");
	buf++;
	if (sscanf(buf, "%u", &data->highspeed_delay) != 1)
		return -EINVAL;
	return count;
}

static ssize_t highspeed_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE,
			  "highspeed freq load delay\n");
	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "     %9lu %4u %5u\n",
			   data->highspeed_freq, data->highspeed_load,
			   data->highspeed_delay);

	return count;
}
static DEVICE_ATTR_RW(highspeed);

static ssize_t available_governors_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);

	return sgpu_governor_all_info_show(df, buf);
}
static DEVICE_ATTR_RO(available_governors);

static ssize_t governor_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	ssize_t count;

	count = sgpu_governor_current_info_show(df, buf);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t governor_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	int ret;
	char str_governor[DEVFREQ_NAME_LEN + 1];

	if (!df->governor || !df->data)
		return -EINVAL;
	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_governor);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_governor_change(df, str_governor);
	if (ret) {
		dev_warn(dev, "%s: Governor %s not started(%d)\n",
			 __func__, df->governor->name, ret);
		return ret;
	}

	if (!ret) {
		sgpu_governor_current_info_show(df, str_governor);
		dev_info(dev, "governor : %s\n", str_governor);
		ret = count;
	}
	return ret;
}
static DEVICE_ATTR_RW(governor);

static ssize_t power_ratio_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%u\n", data->power_ratio);

	return count;
}

static ssize_t power_ratio_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t value;
	int ret;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->power_ratio = value;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;
}

static DEVICE_ATTR_RW(power_ratio);

static ssize_t current_utilization_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	uint64_t utilization = udata->last_util;

	ssize_t count;
	count = scnprintf(buf, PAGE_SIZE, "%llu\n", utilization);
	return count;
}

static DEVICE_ATTR_RO(current_utilization);

static ssize_t current_cu_utilization_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data *udata = stat->private_data;
	uint64_t cu_utilization = udata->last_cu_util;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%llu\n", cu_utilization);
	return count;
}

static DEVICE_ATTR_RO(current_cu_utilization);

static ssize_t weight_table_idx_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count = 0;
	uint32_t i;

	for (i = 0; i < WEIGHT_TABLE_IDX_NUM; i++)
		count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "weight_table_idx%d = %d\n",
				   i, data->weight_table_idx[i]);
	return count;
}

static ssize_t weight_table_idx_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t idx, value;
	int ret;

	ret = sscanf(buf, "%u", &idx);
	if (ret != 1 || idx >= WEIGHT_TABLE_IDX_NUM) {
		dev_warn(dev, "weight idx range : (0 ~ %u)\n",
			 WEIGHT_TABLE_IDX_NUM);
		return -EINVAL;
	}

	buf = strpbrk(buf, " ");
	buf++;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1 || value >= WEIGHT_TABLE_MAX_SIZE) {
		dev_warn(dev, "weight table idx range : (0 ~%u)\n",
			 WEIGHT_TABLE_MAX_SIZE);
		return -EINVAL;
	}

	data->weight_table_idx[idx] = value;

	return count;
}
static DEVICE_ATTR_RW(weight_table_idx);

static ssize_t freq_margin_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "freq_margin = %d\n",
			  data->freq_margin);

	return count;
}

static ssize_t freq_margin_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	int freq_margin, ret;

	ret = sscanf(buf, "%d", &freq_margin);
	if (ret != 1 || freq_margin > 1000 || freq_margin < -1000) {
		dev_warn(dev,"freq_margin range : (-1000 ~ 1000)\n");
		return -EINVAL;
	}
	data->freq_margin = freq_margin;

	return count;

}
static DEVICE_ATTR_RW(freq_margin);

static ssize_t job_queue_count_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev->parent);
	struct amdgpu_device *adev = ddev->dev_private;
	struct amdgpu_ring *ring = adev->gfx.gfx_ring;
	uint64_t i, total_count = 0, hw_count = 0;
	ssize_t count = 0;

	for (i = 0; i < adev->gfx.num_gfx_rings; i++) {
		total_count += atomic64_read(&ring[i].sched.job_id_count);
		hw_count += (atomic_read(&ring[i].fence_drv.last_seq) - 1) / 2;
	}

	count = scnprintf(buf, PAGE_SIZE, "total_count = %llu\n", total_count);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "hw_count = %llu\n", hw_count);

	return count;
}
static DEVICE_ATTR_RO(job_queue_count);

static ssize_t compute_weight_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "%d\n",
			  data->compute_weight);

	return count;
}

static ssize_t compute_weight_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	int compute_weight, ret;

	ret = sscanf(buf, "%d", &compute_weight);
	if (ret != 1 || compute_weight > 10000 || compute_weight < 100) {
		dev_warn(dev, "compute_weight range : (100 ~ 10000)\n");
		return -EINVAL;
	}
	data->compute_weight = compute_weight;

	return count;

}
static DEVICE_ATTR_RW(compute_weight);

static ssize_t time_in_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	uint64_t cur_time, state_time;
	int i, lev, prev_lev = 0;
	ssize_t count = 0;

	cur_time = get_jiffies_64();
	for (lev = 0; lev < df->profile->max_state; lev++) {
		if (df->previous_freq == df->profile->freq_table[lev]) {
			prev_lev = lev;
			break;
		}
	}
	if (prev_lev == df->profile->max_state)
		return 0;

	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "    FREQ    TIME(ms)\n");
	for (i = 0; i < df->profile->max_state; i++) {
		state_time = df->stats.time_in_state[i];
		if (!df->stop_polling && prev_lev == i)
			state_time += cur_time - df->stats.last_update;

		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%8lu  %10llu\n", df->profile->freq_table[i],
				   jiffies64_to_msecs(state_time));
	}

	return count;
}
static DEVICE_ATTR_RO(time_in_state);

static ssize_t local_minlock_util_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%u\n", data->local_minlock_util);

	return count;
}

static ssize_t local_minlock_util_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t value;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->local_minlock_util = value;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;

}
static DEVICE_ATTR_RW(local_minlock_util);

static ssize_t local_minlock_temp_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%u\n", data->local_minlock_temp);

	return count;

}

static ssize_t local_minlock_temp_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	uint32_t value;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->local_minlock_temp = value;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;

}
static DEVICE_ATTR_RW(local_minlock_temp);

static ssize_t local_minlock_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%u\n", data->local_minlock_status);

	return count;
}
static DEVICE_ATTR_RO(local_minlock_status);

static ssize_t total_kernel_pages_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev->parent);
	struct amdgpu_device *adev = ddev->dev_private;
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "total_kernel_pages = %zd\n", adev->num_kernel_pages);

	return count;
}
static DEVICE_ATTR_RO(total_kernel_pages);

extern int exynos_amigo_get_target_frametime(void);
static ssize_t egp_profile_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	//struct drm_device *ddev = dev_get_drvdata(dev->parent);
	//struct amdgpu_device *adev = ddev->dev_private;
	struct amigo_interframe_data *dst;
	ssize_t count = 0;
	int id = 0;
	int target_frametime = exynos_amigo_get_target_frametime();

	while ((dst = exynos_amigo_get_next_frameinfo()) != NULL){
		if (dst->nrq > 0) {
			ktime_t avg_pre = dst->sum_pre / dst->nrq;
			ktime_t avg_cpu = dst->sum_cpu / dst->nrq;
			ktime_t avg_v2s = dst->sum_v2s / dst->nrq;
			ktime_t avg_gpu = dst->sum_gpu / dst->nrq;
			ktime_t avg_v2f = dst->sum_v2f / dst->nrq;

			count += scnprintf(buf + count, PAGE_SIZE - count
				, "%4d, %6llu, %3u, %6lu,%6lu, %6lu,%6lu, %6lu,%6lu, %6lu,%6lu, %6lu,%6lu, %6lu,%6lu, %6d, %d, %7d,%7d, %7d,%7d\n"
				, id++
				, dst->vsync_interval
				, dst->nrq
				, avg_pre, dst->max_pre
				, avg_cpu, dst->max_cpu
				, avg_v2s, dst->max_v2s
				, avg_gpu, dst->max_gpu
				, avg_v2f, dst->max_v2f
				, dst->cputime, dst->gputime
				, target_frametime, dst->sdp_next_cpuid
				, dst->sdp_cur_fcpu, dst->sdp_cur_fgpu
			        , dst->sdp_next_fcpu, dst->sdp_next_fgpu
			);
		}
	}

	return count;
}
static DEVICE_ATTR_RO(egp_profile);

static struct attribute *sgpu_devfreq_sysfs_entries[] = {
	/* just show */
	&dev_attr_dvfs_table.attr,
	&dev_attr_available_governors.attr,
	&dev_attr_available_utilization_sources.attr,
	&dev_attr_current_utilization.attr,
	&dev_attr_current_cu_utilization.attr,
	&dev_attr_job_queue_count.attr,
	&dev_attr_time_in_state.attr,
	&dev_attr_total_kernel_pages.attr,
	&dev_attr_egp_profile.attr,
	&dev_attr_local_minlock_status.attr,
	/* show and set */
	&dev_attr_highspeed.attr,
	&dev_attr_utilization_source.attr,
	&dev_attr_governor.attr,
	&dev_attr_wakeup.attr,
	&dev_attr_min_freq.attr,
	&dev_attr_max_freq.attr,
	&dev_attr_valid_time.attr,
	&dev_attr_min_thresholds.attr,
	&dev_attr_max_thresholds.attr,
	&dev_attr_downstay_times.attr,
	&dev_attr_power_ratio.attr,
	&dev_attr_weight_table_idx.attr,
	&dev_attr_freq_margin.attr,
	&dev_attr_compute_weight.attr,
	&dev_attr_local_minlock_util.attr,
	&dev_attr_local_minlock_temp.attr,
	NULL,
};

static struct attribute_group sgpu_devfreq_attr_group = {
	.name = "interface",
	.attrs = sgpu_devfreq_sysfs_entries,
};

int sgpu_create_sysfs_file(struct devfreq *df)
{
	int ret = 0;
	ret = sysfs_create_group(&df->dev.kobj, &sgpu_devfreq_attr_group);
	if (ret)
		dev_err(df->dev.parent, "failed create sysfs for governor data\n");

	return ret;
}
