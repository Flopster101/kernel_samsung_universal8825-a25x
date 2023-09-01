/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * CPU Part
 *
 * CPU Hotplug driver for Exynos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/fb.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-cpuhp.h>

static struct {
	/* Control cpu hotplug operation */
	bool			enabled;

	/* flag for suspend */
	bool			suspended;

	/* flag for debug print */
	bool			debug;

	/* list head for requester */
	struct list_head	req_list;

	/* default cpuhp request */
	struct cpuhp_request	default_req;

	/* cpuhp request for sysfs */
	struct cpuhp_request	sysfs_req;

	/* cpuhp control cpus */
	struct cpumask		available_mask;

	/* Synchronizes accesses to refcount and cpumask */
	struct mutex		lock;

	/* requested cpuhp mask */
	struct cpumask		mask;

	/* cpuhp kobject */
	struct kobject		*kobj;
} cpuhp = {
	.lock = __MUTEX_INITIALIZER(cpuhp.lock),
};

static char *available_cpus;
module_param(available_cpus, charp, 0);

static char *setup_cpus;
module_param(setup_cpus, charp, 0);
/******************************************************************************/
/*                             Helper functions                               */
/******************************************************************************/
static int cpuhp_do(void);

/*
 * Update pm_suspend status.
 * During suspend-resume, cpuhp driver is stop
 */
static inline void cpuhp_suspend(bool enable)
{
	/* This lock guarantees completion of cpuhp_do() */
	mutex_lock(&cpuhp.lock);
	cpuhp.suspended = enable;
	mutex_unlock(&cpuhp.lock);
}

/*
 * Update cpuhp enablestatus.
 * cpuhp driver is working when enabled big is TRUE
 */
static inline void cpuhp_enable(bool enable)
{
	mutex_lock(&cpuhp.lock);
	cpuhp.enabled = enable;
	mutex_unlock(&cpuhp.lock);
}

/******************************************************************************/
/*                               External APIs                                */
/******************************************************************************/
int exynos_cpuhp_remove_request(struct cpuhp_request *req)
{
	mutex_lock(&cpuhp.lock);

	req->active = 0;
	list_del(&req->list);

	mutex_unlock(&cpuhp.lock);

	return cpuhp_do();
}
EXPORT_SYMBOL_GPL(exynos_cpuhp_remove_request);

int exynos_cpuhp_add_request(struct cpuhp_request *req)
{
	mutex_lock(&cpuhp.lock);

	if (req->active) {
		pr_info("cpuhp request(%s) is already added\n");
		return -EBUSY;
	}

	list_add(&req->list, &cpuhp.req_list);
	req->active = 1;

	cpumask_clear(&req->mask);
	pr_info("Register new cpuhp request (name=%s)\n", req->name);

	mutex_unlock(&cpuhp.lock);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_cpuhp_add_request);

int exynos_cpuhp_update_request(struct cpuhp_request *req,
					const struct cpumask *mask)
{
	cpumask_copy(&req->mask, mask);

	return cpuhp_do();
}
EXPORT_SYMBOL_GPL(exynos_cpuhp_update_request);

/******************************************************************************/
/*                               CPUHP handlers                               */
/******************************************************************************/
/* legacy hotplug in */
static int cpuhp_in(const struct cpumask *mask)
{
	int cpu, ret = 0;

	for_each_cpu(cpu, mask) {
		ret = add_cpu(cpu);
		if (ret < 0) {
			/*
			 * If it fails to enable cpu,
			 * it cancels cpu hotplug request and retries later.
			 */
			pr_err("Failed to hotplug in CPU%d with error %d\n",
					cpu, ret);
			continue;
		}
	}

	return ret < 0 ? ret : 0;
}

/* legacy hotplug out */
static int cpuhp_out(const struct cpumask *mask)
{
	int cpu, ret = 0;

	/*
	 * Reverse order of cpu,
	 * explore cpu7, cpu6, cpu5, ... cpu1
	 */
	for (cpu = nr_cpu_ids - 1; cpu > 0; cpu--) {
		if (!cpumask_test_cpu(cpu, mask))
			continue;

		ret = remove_cpu(cpu);
		if (ret < 0) {
			pr_err("Failed to hotplug out CPU%d with error %d\n",
					cpu, ret);
			continue;
		}
	}

	return ret < 0 ? ret : 0;
}

static struct cpumask cpuhp_get_new_mask(void)
{
	struct cpumask mask;
	struct cpuhp_request *req;

	cpumask_copy(&mask, &cpuhp.available_mask);

	list_for_each_entry(req, &cpuhp.req_list, list)
		cpumask_and(&mask, &mask, &req->mask);

	if (cpumask_empty(&mask))
		pr_warn("Requsted cpuhp mask is empty\n");

	return mask;
}

/*
 * Executes cpu_up
 */
static int cpuhp_cpu_up(struct cpumask *enable_cpus)
{
	if (cpumask_empty(enable_cpus))
		return 0;

	return cpuhp_in(enable_cpus);
}

/*
 * Executes cpu_down
 */
static int cpuhp_cpu_down(struct cpumask *disable_cpus)
{
	if (cpumask_empty(disable_cpus))
		return 0;

	return cpuhp_out(disable_cpus);
}

/* print cpu control informatoin for deubgging */
static void cpuhp_print_debug_info(struct cpumask *new_mask)
{
	char new_buf[10], cur_buf[10];

	scnprintf(cur_buf, sizeof(cur_buf), "%*pbl", cpumask_pr_args(&cpuhp.mask));
	scnprintf(new_buf, sizeof(new_buf), "%*pbl", cpumask_pr_args(new_mask));
	dbg_snapshot_printk("%s: %s -> %s\n", __func__, cur_buf, new_buf);

	/* print cpu control information */
	if (cpuhp.debug)
		pr_info("%s: %s -> %s\n", __func__, cur_buf, new_buf);
}

static int __cpuhp_do(struct cpumask *req_mask)
{
	struct cpumask incoming_cpus, outgoing_cpus;
	int ret = 0;

	/* get the new online cpus mask */
	cpumask_andnot(&incoming_cpus, req_mask, &cpuhp.mask);
	/* get the new offline cpus mask */
	cpumask_andnot(&outgoing_cpus, &cpuhp.mask, req_mask);

	if (!cpumask_empty(&incoming_cpus))
		ret = cpuhp_cpu_up(&incoming_cpus);
	if (ret)
		return ret;

	if (!cpumask_empty(&outgoing_cpus))
		ret = cpuhp_cpu_down(&outgoing_cpus);

	return ret;
}

/*
 * cpuhp_do() is the main function for cpu hotplug. Only this function
 * enables or disables cpus, so all APIs in this driver call cpuhp_do()
 * eventually.
 */
static int cpuhp_do(void)
{
	struct cpumask new_mask;
	int ret = 0;

	mutex_lock(&cpuhp.lock);
	/*
	 * If cpu hotplug is disabled or suspended,
	 * cpuhp_do() do nothing.
	 */
	if (!cpuhp.enabled || cpuhp.suspended) {
		mutex_unlock(&cpuhp.lock);
		return 0;
	}

	new_mask = cpuhp_get_new_mask();
	cpuhp_print_debug_info(&new_mask);

	/* if there is no mask change, skip */
	if (cpumask_empty(&new_mask) ||
		cpumask_equal(&cpuhp.mask, &new_mask))
		goto out;

	ret = __cpuhp_do(&new_mask);
	if (ret)
		goto out;

	cpumask_and(&cpuhp.mask, &new_mask, cpu_online_mask);

out:
	mutex_unlock(&cpuhp.lock);

	return ret;
}

static int cpuhp_control(bool enable)
{
	struct cpumask mask;
	int ret = 0;

	if (enable) {
		cpuhp_enable(true);
		cpuhp_do();
	} else {
		mutex_lock(&cpuhp.lock);

		cpumask_copy(&mask, cpu_possible_mask);
		cpumask_andnot(&mask, &mask, cpu_online_mask);

		/*
		 * If it success to enable all CPUs, clear cpuhp.enabled flag.
		 * Since then all hotplug requests are ignored.
		 */
		ret = cpuhp_in(&mask);
		if (!ret) {
			/*
			 * In this position, can't use cpuhp_enable()
			 * because already taken cpuhp.lock
			 */
			cpuhp.enabled = false;
		} else {
			pr_err("Failed to disable cpu hotplug, please try again\n");
		}

		mutex_unlock(&cpuhp.lock);
	}

	return ret;
}

/******************************************************************************/
/*                             SYSFS Interface                                */
/******************************************************************************/
/*
 * User can change the number of online cpu by using min_online_cpu and
 * max_online_cpu sysfs node. User input minimum and maxinum online cpu
 * to this node as below:
 *
 * #echo mask > /sys/power/cpuhp/set_online_cpu
 */
#define STR_LEN 6
static ssize_t set_online_cpu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "set online cpu : 0x%x\n",
		*(unsigned int *)cpumask_bits(&cpuhp.sysfs_req.mask));
}

static ssize_t set_online_cpu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cpumask online_cpus;
	char str[STR_LEN], re_str[STR_LEN];
	unsigned int cpumask_value;

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%5s", str))
		return -EINVAL;

	if (str[0] == '0' && toupper(str[1]) == 'X')
		/* Move str pointer to remove "0x" */
		cpumask_parse(str + 2, &online_cpus);
	else {
		if (!sscanf(str, "%d", &cpumask_value))
			return -EINVAL;

		snprintf(re_str, STR_LEN - 1, "%x", cpumask_value);
		cpumask_parse(re_str, &online_cpus);
	}

	if (!cpumask_test_cpu(0, &online_cpus)) {
		pr_warn("wrong format\n");
		return -EINVAL;
	}

	exynos_cpuhp_update_request(&cpuhp.sysfs_req, &online_cpus);

	return count;
}
DEVICE_ATTR_RW(set_online_cpu);

/*
 * It shows cpuhp request information(name, requesting cpu_mask)
 * registered in cpuhp req list
 *
 * #cat /sys/power/cpuhp/reqs
 */
static ssize_t reqs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpuhp_request *req;
	ssize_t ret = 0;

	list_for_each_entry(req, &cpuhp.req_list, list)
		ret += scnprintf(&buf[ret], 30, "%-16s: (0x%x)\n",
			req->name, *(unsigned int *)cpumask_bits(&req->mask));

	ret += snprintf(&buf[ret], 30, "available cpu : 0x%x\n",
			*(unsigned int *)cpumask_bits(&cpuhp.available_mask));
	ret += snprintf(&buf[ret], 30, "\nrequested cpu: 0x%x\n",
			*(unsigned int *)cpumask_bits(&cpuhp.mask));

	return ret;
}
DEVICE_ATTR_RO(reqs);

/*
 * User can control the cpu hotplug operation as below:
 *
 * #echo 1 > /sys/power/cpuhp/enabled => enable
 * #echo 0 > /sys/power/cpuhp/enabled => disable
 *
 * If enabled become 0, hotplug driver enable the all cpus and no hotplug
 * operation happen from hotplug driver.
 */
static ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", cpuhp.enabled);
}

static ssize_t enabled_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	cpuhp_control(!!input);

	return count;
}
DEVICE_ATTR_RW(enabled);

/*
 * User can control en/disable debug mode
 *
 * #echo 1 > /sys/power/cpuhp/debug => enable
 * #echo 0 > /sys/power/cpuhp/debug => disable
 *
 * When it is enabled, information is printed every time there is a cpu control
 */
static ssize_t debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", cpuhp.debug);
}

static ssize_t debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	cpuhp.debug = !!input;

	return count;
}
DEVICE_ATTR_RW(debug);

static struct attribute *exynos_cpuhp_attrs[] = {
	&dev_attr_set_online_cpu.attr,
	&dev_attr_reqs.attr,
	&dev_attr_enabled.attr,
	&dev_attr_debug.attr,
	NULL,
};

static struct attribute_group exynos_cpuhp_group = {
	.name = "cpuhp",
	.attrs = exynos_cpuhp_attrs,
};

/******************************************************************************/
/*                            Initialize Driver                               */
/******************************************************************************/
static int parse_cpumask(char *str_cpus, struct cpumask *dest_mask)
{
	if (str_cpus[0] == '0' && toupper(str_cpus[1]) == 'X')
		/* Move str pointer to remove "0x" */
		cpumask_parse(str_cpus + 2, dest_mask);
	else
		cpumask_parse(str_cpus, dest_mask);

	cpumask_and(dest_mask, dest_mask, cpu_possible_mask);
	cpumask_and(dest_mask, dest_mask, &cpuhp.available_mask);

	if (!cpumask_test_cpu(0, dest_mask)) {
		pr_warn("wrong format\n");
		return -1;
	}

	return 0;
}

static void init_setup_cpus(void)
{
	struct cpumask setup_mask;
	int ret;

	ret = parse_cpumask(setup_cpus, &setup_mask);
	if (ret)
		return;

	mutex_lock(&cpuhp.lock);
	ret = __cpuhp_do(&setup_mask);
	mutex_unlock(&cpuhp.lock);

	if (ret)
		pr_warn("%s: Failed setup_cpus %*pbl", cpumask_pr_args(&setup_mask));
	else
		pr_info("%s: bootargs setup_cpus = %s\n", __func__, setup_cpus);
}

static void init_available_cpus(void)
{
	struct cpumask mask;
	int ret;

	cpumask_copy(&cpuhp.available_mask, cpu_possible_mask);

	if (!available_cpus)
		return;

	ret = parse_cpumask(available_cpus, &mask);
	if (ret)
		return;

	cpumask_copy(&cpuhp.available_mask, &mask);

	pr_info("%s: bootargs available_cpus = %s\n", __func__, available_cpus);
}

static void cpuhp_init(void)
{
	/* init req list */
	INIT_LIST_HEAD(&cpuhp.req_list);

	/* register default cpuhp request */
	strcpy(cpuhp.default_req.name, "cpuhp_default");
	exynos_cpuhp_add_request(&cpuhp.default_req);
	exynos_cpuhp_update_request(&cpuhp.default_req, cpu_possible_mask);

	/* register cpuhp request for sysfs */
	strcpy(cpuhp.sysfs_req.name, "cpuhp_sysfs");
	exynos_cpuhp_add_request(&cpuhp.sysfs_req);
	exynos_cpuhp_update_request(&cpuhp.sysfs_req, cpu_possible_mask);

	cpumask_copy(&cpuhp.mask, cpu_online_mask);

	init_available_cpus();

	cpuhp_enable(true);

	if (setup_cpus)
		init_setup_cpus();
}

static int exynos_cpuhp_probe(struct platform_device *pdev)
{
	/* Create CPUHP sysfs */
	if (sysfs_create_group(&pdev->dev.kobj, &exynos_cpuhp_group))
		pr_err("Failed to create sysfs for CPUHP\n");

	/* Link CPUHP sysfs to /sys/devices/system/cpu/cpuhp */
	if (sysfs_create_link(&cpu_subsys.dev_root->kobj,
				&pdev->dev.kobj, "cpuhp"))
		pr_err("Failed to link CPUHP sysfs to cpuctrl\n");

	cpuhp_init();

	pr_info("Exynos CPUHP driver probe done!\n");

	return 0;
}

static const struct of_device_id of_exynos_cpuhp_match[] = {
	{ .compatible = "samsung,exynos-cpuhp", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_cpuhp_match);

static struct platform_driver exynos_cpuhp_driver = {
	.driver = {
		.name = "exynos-cpuhp",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_cpuhp_match,
	},
	.probe		= exynos_cpuhp_probe,
};

static int __init exynos_cpuhp_init(void)
{
	return platform_driver_register(&exynos_cpuhp_driver);
}
arch_initcall(exynos_cpuhp_init);

static void __exit exynos_cpuhp_exit(void)
{
	platform_driver_unregister(&exynos_cpuhp_driver);
}
module_exit(exynos_cpuhp_exit);

MODULE_DESCRIPTION("Exynos CPUHP driver");
MODULE_LICENSE("GPL");
