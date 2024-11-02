/* drivers/samsung/pm/sec_pm_cpufreq.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Author: Minsung Kim <ms925.kim@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <soc/samsung/freq-qos-tracer.h>

struct sec_pm_cpufreq_info {
	struct device			*dev;
	struct delayed_work		release_work;
	struct notifier_block		pm_nb;
	unsigned int			policy_cpu;
	unsigned int			resume_max_freq;
	struct freq_qos_request		qos_req;
};

static void freq_qos_release(struct work_struct *work)
{
	struct sec_pm_cpufreq_info *info =
				container_of(to_delayed_work(work),
				struct sec_pm_cpufreq_info, release_work);

	freq_qos_update_request(&info->qos_req, PM_QOS_DEFAULT_VALUE);
}

static int sec_pm_cpufreq_pm_event(struct notifier_block *nb,
				   unsigned long pm_event, void *unused)
{
	struct sec_pm_cpufreq_info *info = container_of(nb,
					struct sec_pm_cpufreq_info, pm_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work(&info->release_work);
		freq_qos_update_request(&info->qos_req, PM_QOS_DEFAULT_VALUE);
		dev_info(info->dev, "%s: cancel work(%u)\n", __func__, info->policy_cpu);
		break;
	case PM_POST_SUSPEND:
		freq_qos_update_request(&info->qos_req,
						info->resume_max_freq);
		schedule_delayed_work(&info->release_work,
				      msecs_to_jiffies(ktime_to_ms(ktime_set(3, 0))));
		dev_info(info->dev, "%s: resume_max_freq(%u, %u)\n", __func__,
				info->policy_cpu, info->resume_max_freq);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int sec_pm_cpufreq_parse_dt(struct platform_device *pdev)
{
	struct sec_pm_cpufreq_info *info = platform_get_drvdata(pdev);
	struct device_node *dn;
	const char *buf;
	struct cpumask siblings;
	u32 val;

	if (!info || !pdev->dev.of_node)
		return -ENODEV;

	dn = pdev->dev.of_node;

	if (!of_property_read_string(dn, "siblings", &buf)) {
		cpulist_parse(buf, &siblings);
		info->policy_cpu = cpumask_first(&siblings);
	} else {
		dev_err(info->dev, "%s: failed to get siblings\n", __func__);
		return -EINVAL;
	}

	if (!of_property_read_u32(dn, "resume_max_freq", &val)) {
		info->resume_max_freq = val;
	} else {
		dev_err(info->dev, "%s: failed to get resume_max_freq\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int sec_pm_cpufreq_probe(struct platform_device *pdev)
{
	struct sec_pm_cpufreq_info *info;
	struct cpufreq_policy *policy;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = sec_pm_cpufreq_parse_dt(pdev);
	if (ret) {
		dev_err(info->dev, "%s: fail to parse dt(%d)\n", __func__, ret);
		return ret;
	}

	policy = cpufreq_cpu_get(info->policy_cpu);
	freq_qos_tracer_add_request_name("sec_pm_cpufreq_max_limit",
			&policy->constraints, &info->qos_req, FREQ_QOS_MAX,
			PM_QOS_DEFAULT_VALUE);

	INIT_DELAYED_WORK(&info->release_work, freq_qos_release);

	info->pm_nb.notifier_call = sec_pm_cpufreq_pm_event;
	ret = register_pm_notifier(&info->pm_nb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register PM notifier(%d)\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static int sec_pm_cpufreq_remove(struct platform_device *pdev)
{
	struct sec_pm_cpufreq_info *info = platform_get_drvdata(pdev);

	if (!info)
		return 0;

	dev_info(info->dev, "%s\n", __func__);
	unregister_pm_notifier(&info->pm_nb);
	freq_qos_tracer_remove_request(&info->qos_req);

	return 0;
}

static const struct of_device_id sec_pm_cpufreq_match[] = {
	{ .compatible = "samsung,sec-pm-cpufreq", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_cpufreq_match);

static struct platform_driver sec_pm_cpufreq_driver = {
	.driver = {
		.name = "sec-pm-cpufreq",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_cpufreq_match),
	},
	.probe = sec_pm_cpufreq_probe,
	.remove = sec_pm_cpufreq_remove,
};

module_platform_driver(sec_pm_cpufreq_driver);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM CPU Frequency Control");
MODULE_LICENSE("GPL");
