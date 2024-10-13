/*
 * exynos_amb_control.c - Samsung AMB control (Ambient Thermal Control module)
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *  Youngjin Lee <youngjin0.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <soc/samsung/exynos-cpuhp.h>
#include <uapi/linux/sched/types.h>

#include <soc/samsung/exynos_amb_control.h>
#include <soc/samsung/exynos-mcinfo.h>
#include <soc/samsung/exynos-sci.h>

#include "exynos_tmu.h"

#define AMB_TZ_NUM	(5)

#define DEFAULT_SAMPLING_RATE	1000
#define HIGH_SAMPLING_RATE	100
#define INCREASE_SAMPLING_TEMP	55000
#define DECREASE_SAMPLING_TEMP	50000
#define HOTPLUG_IN_THRESHOLD	60000
#define HOTPLUG_OUT_THRESHOLD	65000

#define TRIGGER_COND_HIGH	0
#define TRIGGER_COND_LOW	1
#define TRIGGER_COND_NOTUSED	2

static const char * trigger_cond_desc[3] = { "high", "low", "not_used" };

static struct exynos_amb_control_data *amb_control;

static int exynos_amb_control_set_polling(struct exynos_amb_control_data *data,
		unsigned int delay)
{
	kthread_mod_delayed_work(&data->amb_worker, &data->amb_dwork,
			msecs_to_jiffies(delay));
	return 0;
}

/* sysfs nodes for amb control */
#define sysfs_printf(...) count += snprintf(buf + count, PAGE_SIZE, __VA_ARGS__)
static ssize_t
exynos_amb_control_info_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0, i, j;

	mutex_lock(&data->lock);

	sysfs_printf("Exynos AMB Control Informations\n\n");
	sysfs_printf("AMB Thermal Zone Info\n");
	sysfs_printf("AMB thermal zone name : %s\n", data->amb_tzd->type);
	sysfs_printf("Default sampling rate : %u\n", data->default_sampling_rate);
	sysfs_printf("High sampling rate : %u\n", data->high_sampling_rate);
	sysfs_printf("Increase sampling temp : %u\n", data->increase_sampling_temp);
	sysfs_printf("Decrease sampling temp : %u\n", data->decrease_sampling_temp);
	sysfs_printf("==============================================\n");

	if (data->use_mif_throttle) {
		sysfs_printf("MIF throttle temp : %u\n", data->mif_down_threshold);
		sysfs_printf("MIF release temp : %u\n", data->mif_up_threshold);
		sysfs_printf("MIF throttle freq : %u\n", data->mif_throttle_freq);
		sysfs_printf("==============================================\n");
	}

	sysfs_printf("Thermal Zone Info\n");

	for (i = 0; i < data->num_amb_tz_configs; i++) {
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i];

		if (!cpumask_empty(&tz_config->cpu_domain)) {
			sysfs_printf("CPU hotplug domains : 0x%x\n", *(unsigned int *)cpumask_bits(&tz_config->cpu_domain));
			sysfs_printf("CPU hotplug out temp : %u\n", tz_config->hotplug_out_threshold);
			sysfs_printf("CPU hotplug in temp : %u\n", tz_config->hotplug_in_threshold);
		} else {
			sysfs_printf("Thermal zone %s is not using CPU hotplug\n", tz_config->tzd->type);
		}

		for (j = 0; j < tz_config->num_throttle_configs; j++) {
			struct exynos_amb_throttle_config *throttle_config = &tz_config->throttle_config[j];

			sysfs_printf("\nThrottle config #%u\n", j);
			sysfs_printf("Trip point : %u\n", throttle_config->trip_point);
			sysfs_printf("Trigger condition : %s\n", trigger_cond_desc[throttle_config->trigger_cond]);
			sysfs_printf("Trigger temp : %u\n", throttle_config->trigger_temp);
			sysfs_printf("Release temp : %u\n", throttle_config->release_temp);
			sysfs_printf("Amb trip temp : %u\n", throttle_config->amb_throttle_temp);
		}
		sysfs_printf("==============================================\n");
	}

	mutex_unlock(&data->lock);

	return count; 
}

static ssize_t
exynos_amb_control_set_config_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0, i;

	sysfs_printf("Usage for set_config node\n");

	sysfs_printf("[0] AMB thermal zone config (sampling rate/temp)\n");
	sysfs_printf("\t# echo 0 [type] [value] > amb_control_set_config\n");
	sysfs_printf("\ttype : 0/1/2/3=default_sampling_rate/high_sampling_rate/increase_sampling_temp/decrease_sampling_temp\n");

	sysfs_printf("[1] CPU Hotplug config (hotplug temp/domain)\n");
	sysfs_printf("\t# echo 1 [zone] [type] [value] > amb_control_set_config\n");
	sysfs_printf("\tzone : ");
	for (i = 0; i < data->num_amb_tz_configs - 1; i++)
		sysfs_printf("%u/", i);
	sysfs_printf("%u=", i);
	sysfs_printf("\ttype : 0/1/2/3=hotplug_in_threshold/hotplug_out_threshold/hotplug_enable/cpu_domain(hex)\n");

	sysfs_printf("[2] Throttle config (throttle temp, trip porint)\n");
	sysfs_printf("\t# echo 2 [zone] [trip_point] [type] [value] > amb_control_set_config\n");
	sysfs_printf("\tzone : ");
	for (i = 0; i < data->num_amb_tz_configs - 1; i++)
		sysfs_printf("%u/", i);
	sysfs_printf("%u=", i);
	for (i = 0; i < data->num_amb_tz_configs - 1; i++)
		sysfs_printf("%s/", data->amb_tz_config[i].tzd->type);
	sysfs_printf("%s\n", data->amb_tz_config[i].tzd->type);
	sysfs_printf("\ttype : 0/1/2/3=trigger_condition/trigger_temp/release_temp/amb_throttle_temp\n");

	return count;
}

static ssize_t
exynos_amb_control_set_config_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	struct ambient_thermal_zone_config *tz_config = NULL;
	struct exynos_amb_throttle_config *throttle_config = NULL;
	unsigned int i, id, zone, type, value, trip_point;
	char string[20];

	mutex_lock(&data->lock);

	if (sscanf(buf, "%d ", &id) != 1)
		goto out;

	if (id == 0) {
		if (sscanf(buf, "%u %u %u", &id, &type, &value) != 3)
			goto out;

		switch (type) {
			case 0:
				data->default_sampling_rate = value;
				break;
			case 1:
				data->high_sampling_rate = value;
				break;
			case 2:
				data->increase_sampling_temp = value;
				break;
			case 3:
				data->decrease_sampling_temp = value;
				break;
			default:
				break;
		}
	} else if (id == 1) {
		if (sscanf(buf, "%u %u %u ", &id, &zone, &type) != 3)
			goto out;

		if (type == 3) {
			if (sscanf(buf, "%u %u %u 0x%x", &id, &zone, &type, &value) != 4)
				goto out;
		} else {
			if (sscanf(buf, "%u %u %u %u", &id, &zone, &type, &value) != 4)
				goto out;
		}

		if (zone >= data->num_amb_tz_configs)
			goto out;

		tz_config = &data->amb_tz_config[zone];

		switch (type) {
			case 0:
				if (value < 125000)
					tz_config->hotplug_in_threshold = value;
				break;
			case 1:
				if (value < 125000)
					tz_config->hotplug_out_threshold = value;
				break;
			case 2:
				tz_config->hotplug_enable = !!value;
				break;
			case 3:
				snprintf(string, sizeof(string), "%x", value);
				cpumask_parse(string, &tz_config->cpu_domain);
				break;
			default:
				break;
		}
	} else if (id == 2) {
		if (sscanf(buf, "%u %u %u %u %u", &id, &zone, &trip_point, &type, &value) != 5)
			goto out;

		if (zone >= data->num_amb_tz_configs)
			goto out;

		tz_config = &data->amb_tz_config[zone];

		for (i = 0; i < tz_config->num_throttle_configs; i++) {
			if (tz_config->throttle_config[i].trip_point == trip_point) {
				throttle_config = &tz_config->throttle_config[i];
				break;
			}
		}

		if (throttle_config) {
			switch (type) {
				case 0:
					if (value < 3)
						throttle_config->trigger_cond = value;
					break;
				case 1:
					if (value < 125000)
						throttle_config->trigger_temp = value;
					break;
				case 2:
					if (value < 125000)
						throttle_config->release_temp = value;
					break;
				case 3:
					if (value < 125000)
						throttle_config->amb_throttle_temp = value;
					break;
			default:
				break;

			}
		}
	}

out:
	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_control_mode_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	mutex_lock(&data->lock);

	sysfs_printf("%s\n", data->amb_disabled ? "disabled" : "enabled");

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_control_mode_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	int new_mode, prev_mode;

	mutex_lock(&data->lock);

	sscanf(buf, "%d", &new_mode);

	prev_mode = data->amb_disabled;
	data->amb_disabled = !!new_mode;

	mutex_unlock(&data->lock);

	if (data->amb_disabled && !prev_mode) {
		kthread_cancel_delayed_work_sync(&data->amb_dwork);
		pr_info("%s: amb control is disabled\n", __func__);
	} else if (!data->amb_disabled && prev_mode) {
		exynos_amb_control_set_polling(data, data->current_sampling_rate);
		pr_info("%s: amb control is enabled\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(amb_control_info, S_IRUGO, exynos_amb_control_info_show, NULL);
static DEVICE_ATTR(amb_control_set_config, S_IWUSR | S_IRUGO,
		exynos_amb_control_set_config_show, exynos_amb_control_set_config_store);
static DEVICE_ATTR(amb_control_disable, S_IWUSR | S_IRUGO,
		exynos_amb_control_mode_show, exynos_amb_control_mode_store);

static struct attribute *exynos_amb_control_attrs[] = {
	&dev_attr_amb_control_info.attr,
	&dev_attr_amb_control_set_config.attr,
	&dev_attr_amb_control_disable.attr,
	NULL,
};

static const struct attribute_group exynos_amb_control_attr_group = {
	.attrs = exynos_amb_control_attrs,
};

/* what does amb do ?
 *
 * set hotplug in/out theshold when amb temp is reaches on given value
 * change control temp of cpu
 * change throttle temp of ISP
 */
static int exynos_amb_controller(struct exynos_amb_control_data *data)
{
	unsigned int i, j;
	int amb_temp = 0;
	struct cpumask mask;

	mutex_lock(&data->lock);

	if (data->in_suspend || data->amb_disabled) {
		pr_info("%s: amb control is %s\n", __func__, data->in_suspend ? "suspended" : "disabled");
		goto out;
	}

	/* Get temperature */
	thermal_zone_get_temp(data->amb_tzd, &amb_temp);

	data->amb_temp = amb_temp;

	if (amb_temp == 0) {
		data->current_sampling_rate = data->high_sampling_rate;
		goto polling_out;
	}

	exynos_mcinfo_set_ambient_status(amb_temp > data->mcinfo_threshold);

	cpumask_copy(&mask, cpu_possible_mask);

	/* Check MIF throttle condition */
	if (data->use_mif_throttle) {
		if (amb_temp > data->mif_down_threshold)
			exynos_pm_qos_update_request(&data->amb_mif_qos, data->mif_throttle_freq);
		else if (amb_temp <= data->mif_up_threshold)
			exynos_pm_qos_update_request(&data->amb_mif_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	}

	/* Check LLC throttle condition */
	if (data->use_llc_throttle) {
		if (amb_temp > data->llc_off_threshold)
			llc_disable_force(true);
		else if (amb_temp <= data->llc_on_threshold)
			llc_disable_force(false);
	}

	/* Traverse all amb tz config */
	for (i = 0; i < data->num_amb_tz_configs; i++) {
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i];

		/* Check hotplug condition */
		if (!cpumask_empty(&tz_config->cpu_domain)) {
			if (tz_config->is_cpu_hotplugged_out)
				cpumask_andnot(&mask, &mask, &tz_config->cpu_domain);

			if (!tz_config->is_cpu_hotplugged_out && amb_temp > tz_config->hotplug_out_threshold) {
				tz_config->is_cpu_hotplugged_out = true;
				cpumask_andnot(&mask, &mask, &tz_config->cpu_domain);
			} else if (tz_config->is_cpu_hotplugged_out && amb_temp <= tz_config->hotplug_in_threshold) {
				tz_config->is_cpu_hotplugged_out = false;
				cpumask_or(&mask, &mask, &tz_config->cpu_domain);
			}
		}

		/* Change throttle temp */
		for (j = 0; j < tz_config->num_throttle_configs; j++) {
			struct exynos_amb_throttle_config *throttle_config =
				&tz_config->throttle_config[j];
			struct exynos_tmu_data *tmu_data = exynos_tmu_get_data_from_tz(tz_config->tzd);

			if (throttle_config->trigger_cond) {
				// set amb throttle temp when amb temp is higher than trigger
				if (amb_temp > throttle_config->trigger_temp && !throttle_config->status) {
					if (!tz_config->amb_mode_cnt)
						exynos_tmu_set_boost_mode(tmu_data, AMBIENT_MODE, true);
					tz_config->amb_mode_cnt++;

					tz_config->tzd->ops->set_trip_temp(
							tz_config->tzd, throttle_config->trip_point,
							throttle_config->amb_throttle_temp);
					throttle_config->status = true;
				} else if (amb_temp <= throttle_config->release_temp && throttle_config->status) {
					tz_config->amb_mode_cnt--;
					if (!tz_config->amb_mode_cnt)
						exynos_tmu_set_boost_mode(tmu_data, THROTTLE_MODE, true);
					throttle_config->status = false;
				}
			} else {
				// set amb throttle temp when amb temp is lower than trigger
				if (amb_temp <= throttle_config->trigger_temp && !throttle_config->status) {
					if (!tz_config->amb_mode_cnt)
						exynos_tmu_set_boost_mode(tmu_data, AMBIENT_MODE, true);
					tz_config->amb_mode_cnt++;

					tz_config->tzd->ops->set_trip_temp(
							tz_config->tzd, throttle_config->trip_point,
							throttle_config->amb_throttle_temp);
					throttle_config->status = true;
				} else if (amb_temp > throttle_config->release_temp && throttle_config->status) {
					tz_config->amb_mode_cnt--;
					if (!tz_config->amb_mode_cnt)
						exynos_tmu_set_boost_mode(tmu_data, THROTTLE_MODE, true);
					throttle_config->status = false;
				}
			}
		}
	}

	if (!cpumask_equal(&data->cpuhp_req_mask, &mask)) {
		exynos_cpuhp_update_request(&data->cpuhp_req, &mask);
		cpumask_copy(&data->cpuhp_req_mask, &mask);
	}

	if (amb_temp > data->increase_sampling_temp)
		data->current_sampling_rate = data->high_sampling_rate;
	else if (amb_temp <= data->decrease_sampling_temp)
		data->current_sampling_rate = data->default_sampling_rate;
polling_out:
	exynos_amb_control_set_polling(data, data->current_sampling_rate);
out:
	mutex_unlock(&data->lock);

	return 0;
}

static void exynos_amb_control_work_func(struct kthread_work *work)
{
	struct exynos_amb_control_data *data =
			container_of(work, struct exynos_amb_control_data, amb_dwork.work);

	exynos_amb_controller(data);
};

static int exynos_amb_control_work_init(struct platform_device *pdev)
{
	struct cpumask mask;
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	struct task_struct *thread;
	int ret = 0;

	kthread_init_worker(&data->amb_worker);
	thread = kthread_create(kthread_worker_fn, &data->amb_worker,
			"thermal_amb");
	if (IS_ERR(thread)) {
		dev_err(&pdev->dev, "failed to create amb worker thread: %ld\n",
				PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		dev_warn(&pdev->dev, "failed to set amb worker thread as SCHED_FIFO\n");
		return ret;
	}

	kthread_init_delayed_work(&data->amb_dwork, exynos_amb_control_work_func);
	exynos_amb_control_set_polling(data, 0);

	wake_up_process(thread);

	return ret;
}

static int exynos_amb_control_parse_dt(struct platform_device *pdev)
{
	const char *buf;
	struct device_node *np, *child;
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	int i = 0;

	np = pdev->dev.of_node;

	/* Get common amb_control configs */
	if (of_property_read_string(np, "amb_tz_name", &buf)) {
		dev_err(&pdev->dev, "failed to get amb_tz_name\n");
		return -ENODEV;
	} else {
		data->amb_tzd = thermal_zone_get_zone_by_name(buf);
		if (!data->amb_tzd) {
			dev_err(&pdev->dev, "failed to get amb_tzd\n");
			return -ENODEV;
		}

		dev_info(&pdev->dev, "Amb tz name %s\n", data->amb_tzd->type);
	}

	if (of_property_read_u32(np, "default_sampling_rate", &data->default_sampling_rate))
		data->default_sampling_rate = DEFAULT_SAMPLING_RATE;

	dev_info(&pdev->dev, "Default sampling rate (%u)\n", data->default_sampling_rate);

	if (of_property_read_u32(np, "high_sampling_rate", &data->high_sampling_rate))
		data->high_sampling_rate = HIGH_SAMPLING_RATE;
	dev_info(&pdev->dev, "High sampling rate (%u)\n", data->high_sampling_rate);

	if (of_property_read_u32(np, "increase_sampling_temp", &data->increase_sampling_temp))
		data->increase_sampling_temp = INCREASE_SAMPLING_TEMP;
	dev_info(&pdev->dev, "Increase sampling temp (%u)\n", data->increase_sampling_temp);

	if (of_property_read_u32(np, "decrease_sampling_temp", &data->decrease_sampling_temp))
		data->decrease_sampling_temp = DECREASE_SAMPLING_TEMP;
	dev_info(&pdev->dev, "decrease sampling temp (%u)\n", data->decrease_sampling_temp);

	if (of_property_read_u32(np, "mcinfo_threshold", &data->mcinfo_threshold))
		data->mcinfo_threshold = MCINFO_THRESHOLD_TEMP;
	dev_info(&pdev->dev, "decrease sampling temp (%u)\n", data->mcinfo_threshold);

	data->current_sampling_rate = data->default_sampling_rate;
	dev_info(&pdev->dev, "Current sampling rate (%u)\n", data->current_sampling_rate);

	data->use_mif_throttle = of_property_read_bool(np, "use_mif_throttle");
	dev_info(&pdev->dev, "use mif throttle (%s)\n", data->use_mif_throttle ? "true" : "false");
	if (data->use_mif_throttle) {
		exynos_pm_qos_add_request(&data->amb_mif_qos,
				PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);

		if (of_property_read_u32(np, "mif_down_threshold", &data->mif_down_threshold))
			data->mif_down_threshold = MIF_DOWN_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "mif down threshold (%u)\n", data->mif_down_threshold);

		if (of_property_read_u32(np, "mif_up_threshold", &data->mif_up_threshold))
			data->mif_up_threshold = MIF_UP_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "mif up threshold (%u)\n", data->mif_up_threshold);

		if (of_property_read_u32(np, "mif_throttle_freq", &data->mif_throttle_freq))
			data->mif_throttle_freq = MIF_THROTTLE_FREQ;

		dev_info(&pdev->dev, "mif throttle freq (%u)\n", data->mif_throttle_freq);
	}

	data->use_llc_throttle = of_property_read_bool(np, "use_llc_throttle");
	dev_info(&pdev->dev, "use llc throttle (%s)\n", data->use_llc_throttle ? "true" : "false");
	if (data->use_llc_throttle) {
		if (of_property_read_u32(np, "llc_off_threshold", &data->llc_off_threshold))
			data->llc_off_threshold = LLC_OFF_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "llc off threshold (%u)\n", data->llc_off_threshold);

		if (of_property_read_u32(np, "llc_on_threshold", &data->llc_on_threshold))
			data->llc_on_threshold = LLC_ON_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "llc on threshold (%u)\n", data->llc_on_threshold);
	}

	/* Get configs for each thermal zones */
	data->num_amb_tz_configs = of_get_child_count(np);
	data->amb_tz_config = kzalloc(sizeof(struct ambient_thermal_zone_config) *
			data->num_amb_tz_configs, GFP_KERNEL);

	for_each_available_child_of_node(np, child) {
		struct device_node *trips, *tz_np;
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i++];

		tz_np = of_parse_phandle(child, "tz", 0);
		tz_config->tzd = thermal_zone_get_zone_by_name(tz_np->name);

		dev_info(&pdev->dev, "Config for %s thermal zone\n", tz_config->tzd->type);

		// set cpu hotplug
		if (!of_property_read_string(child, "hotplug_cpu_list", &buf)) {
			cpulist_parse(buf, &tz_config->cpu_domain);
			dev_info(&pdev->dev, "Hotplug control for CPU %s\n", buf);
			if (of_property_read_u32(child, "hotplug_in_threshold",
					&tz_config->hotplug_in_threshold))
				tz_config->hotplug_in_threshold = HOTPLUG_IN_THRESHOLD;
			dev_info(&pdev->dev, "Hotplug in threshold (%u)\n",
					tz_config->hotplug_in_threshold);
			if (of_property_read_u32(child, "hotplug_out_threshold",
					&tz_config->hotplug_out_threshold))
				tz_config->hotplug_out_threshold = HOTPLUG_OUT_THRESHOLD;
			dev_info(&pdev->dev, "Hotplug out threshold (%u)\n",
					tz_config->hotplug_out_threshold);
		}

		// Change throttle temp of tz
		trips = of_get_child_by_name(child, "trip_set");
		if (trips) {
			struct device_node *trip_child;
			int j = 0;

			tz_config->num_throttle_configs = of_get_child_count(trips);
			tz_config->throttle_config =
				kzalloc(sizeof(struct exynos_amb_throttle_config) *
						tz_config->num_throttle_configs, GFP_KERNEL);

			dev_info(&pdev->dev, "Trip conf for %s thermal zone\n", tz_np->name);
			for_each_available_child_of_node (trips, trip_child) {
				struct exynos_amb_throttle_config *throttle_conf =
					&tz_config->throttle_config[j++];

				dev_info(&pdev->dev, "#%u trip config\n", j);
				if (of_property_read_u32(trip_child, "trigger_cond",
						&throttle_conf->trigger_cond))
					throttle_conf->trigger_cond = 2;
				if (throttle_conf->trigger_cond > 2)
					throttle_conf->trigger_cond = 2;

				if (of_property_read_u32(trip_child, "trip_point",
						&throttle_conf->trip_point))
					throttle_conf->trigger_cond = 2;
				dev_info(&pdev->dev, "trip point (%u)\n",
						throttle_conf->trip_point);

				if (of_property_read_u32(trip_child, "trigger_temp",
						&throttle_conf->trigger_temp))
					throttle_conf->trigger_cond = 2;
				dev_info(&pdev->dev, "trigger temp (%u)\n",
						throttle_conf->trigger_temp);

				if (of_property_read_u32(trip_child, "release_temp",
						&throttle_conf->release_temp))
					throttle_conf->trigger_cond = 2;
				dev_info(&pdev->dev, "release temp (%u)\n",
						throttle_conf->release_temp);

				if (of_property_read_u32(trip_child, "throttle_temp",
						&throttle_conf->amb_throttle_temp))
					throttle_conf->trigger_cond = 2;
				dev_info(&pdev->dev, "throttle temp (%u)\n",
						throttle_conf->amb_throttle_temp);

				dev_info(&pdev->dev, "Trigger cond (%s)\n",
						trigger_cond_desc[throttle_conf->trigger_cond]);
			}
		}
	}

	snprintf(data->cpuhp_req.name, THERMAL_NAME_LENGTH, "amb_cpuhp");
	exynos_cpuhp_add_request(&data->cpuhp_req);

	return 0;
}
static int exynos_amb_control_check_devs_dep(struct platform_device *pdev)
{
	const char *buf;
	struct device_node *np, *child;

	np = pdev->dev.of_node;

	/* Check dependency of all drivers */
	if (!of_property_read_string(np, "amb_tz_name", &buf)) {
		if (PTR_ERR(thermal_zone_get_zone_by_name(buf)) == -ENODEV) {
			dev_err(&pdev->dev, "amb thermal zone is not registered!\n");
			return -EPROBE_DEFER;
		}
	} else {
		dev_err(&pdev->dev, "ambient thermal zone is not defined!\n");
		return -ENODEV;
	}

	for_each_available_child_of_node(np, child) {
		struct device_node *tz_np = of_parse_phandle(child, "tz", 0);

		if (tz_np == NULL) {
			dev_err(&pdev->dev, "thermal zone is not defined!\n");
			return -ENODEV;
		}

		if (PTR_ERR(thermal_zone_get_zone_by_name(tz_np->name)) == -ENODEV) {
			dev_err(&pdev->dev, "%s thermal zone is not registered!\n", tz_np->name);
			return -EPROBE_DEFER;
		}
	}

	return 0;
}

static int exynos_amb_control_pm_notify(struct notifier_block *nb,
			     unsigned long mode, void *_unused)
{
	struct exynos_amb_control_data *data = container_of(nb,
			struct exynos_amb_control_data, pm_notify);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		mutex_lock(&data->lock);
		data->in_suspend = true;
		mutex_unlock(&data->lock);
		kthread_cancel_delayed_work_sync(&data->amb_dwork);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&data->lock);
		data->in_suspend = false;
		mutex_unlock(&data->lock);
		exynos_amb_control_set_polling(data, data->current_sampling_rate);
		break;
	default:
		break;
	}
	return 0;
}

static int exynos_amb_control_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = exynos_amb_control_check_devs_dep(pdev);

	if (ret)
		return ret;

	amb_control = kzalloc(sizeof(struct exynos_amb_control_data), GFP_KERNEL);
	platform_set_drvdata(pdev, amb_control);

	ret = exynos_amb_control_parse_dt(pdev);

	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt (%ld)\n", ret);
		kfree(amb_control);
		amb_control = NULL;
		return ret;
	}

	mutex_init(&amb_control->lock);

	exynos_amb_control_work_init(pdev);

	amb_control->pm_notify.notifier_call = exynos_amb_control_pm_notify;
	register_pm_notifier(&amb_control->pm_notify);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_amb_control_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos amb control attr group");

	dev_info(&pdev->dev, "Probe exynos amb controller successfully\n");

	return 0;
}

static int exynos_amb_control_remove(struct platform_device *pdev)
{
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);

	data->amb_disabled = true;

	mutex_unlock(&data->lock);

	kthread_cancel_delayed_work_sync(&data->amb_dwork);

	return 0;
}

static const struct of_device_id exynos_amb_control_match[] = {
	{ .compatible = "samsung,exynos-amb-control", },
	{ /* sentinel */ },
};

static struct platform_driver exynos_amb_control_driver = {
	.driver = {
		.name   = "exynos-amb-control",
		.of_match_table = exynos_amb_control_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_amb_control_probe,
	.remove	= exynos_amb_control_remove,
};
module_platform_driver(exynos_amb_control_driver);

MODULE_DEVICE_TABLE(of, exynos_amb_control_match);

MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("EXYNOS AMB CONTROL Driver");
MODULE_LICENSE("GPL");
