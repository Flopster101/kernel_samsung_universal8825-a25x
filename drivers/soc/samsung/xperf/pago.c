// SPDX-License-Identifier: GPL-2.0
/*
 * power budget driver
 * updated: 2021
 */

#include <linux/of.h>
#include <linux/delay.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/cpumask.h>
#include <linux/thermal.h>
#include <soc/samsung/freq-qos-tracer.h>
#include "../../../kernel/sched/sched.h"
#include "../../../kernel/sched/ems/ems.h"

#include "xperf.h"

static const char *prefix = "pago";

static uint debug;

static uint gov;
static uint gov_ms;
static uint gov_run;
static uint gov_done = 1;
static uint power_limits[NUM_OF_SYSBUSY_STATE];

static uint multi_volt[MAX_CLUSTER];
static uint batt_temp_thd;

/******************************************************************************
 * power budget governor                                                      *
 ******************************************************************************/
static uint power_limit;
static struct freq_qos_request gqos[MAX_CLUSTER];

static int gov_thread(void *data)
{
	uint cpu;
	uint cpu_cur[MAX_CLUSTER];
	uint cpu_max[MAX_CLUSTER];
	int cluster;
	int cpu_power;
	int power_total;
	int cl_idx;
	int f_idx;
	int freq;
	int next_mgn;
	int util_ratio;
	unsigned long power;

	if (gov)
		return 0;

	gov = 1;
	while (gov) {
		for_each_cluster(cluster) {
			cpu = cls[cluster].first_cpu;
			cpu_max[cluster] = cpufreq_quick_get_max(cpu) / 1000;
			cpu_cur[cluster] = cpufreq_quick_get(cpu) / 1000;
		}
		power_total = 0;
		for_each_possible_cpu(cpu) {
			util_ratio = (cpu_util_avgs[cpu] * 100) / 1024;
			cl_idx = get_cl_idx(cpu);
			freq = cpu_cur[cl_idx] * 1000;
			f_idx = get_f_idx(cl_idx, freq);
			power = cls[cl_idx].freqs[f_idx].dyn_power + et_freq_to_spower(cpu, freq);
			cpu_power = util_ratio * power / 100;
			power_total += cpu_power;
		}
		next_mgn = (power_total > power_limit) ? -1 : 1;
		for (cl_idx = 1; cl_idx < cl_cnt; cl_idx++) {
			freq = cpu_max[cl_idx] * 1000;
			f_idx = get_f_idx(cl_idx, freq);
			f_idx += next_mgn;
			f_idx = (f_idx < 0) ? 0 : f_idx;
			f_idx = (f_idx < cls[cl_idx].freq_size) ? f_idx : cls[cl_idx].freq_size - 1;
			freq = cls[cl_idx].freqs[f_idx].freq;
			f_idx = (freq == 0) ? f_idx - 1 : f_idx;
			freq_qos_update_request(&gqos[cl_idx], cls[cl_idx].freqs[f_idx].freq);
		}
		msleep(gov_ms);
	}

	for_each_cluster(cluster)
		freq_qos_update_request(&gqos[cluster], INT_MAX);

	gov_done = 1;

	return 0;
}

static bool is_gov_enable(void)
{
	return gov_run && gov_ms;
}

/******************************************************************************
 * sysbusy notifier                                                           *
 ******************************************************************************/
static struct task_struct *gov_task;
static struct freq_qos_request sqos[MAX_CLUSTER];
static const char *batt_tz_names;

static void set_multi_idx(void)
{
	int cluster;
	unsigned int idx;

	for_each_cluster(cluster) {
		idx = cls[cluster].multi_idx;
		if (idx != cls[cluster].freq_size - 1)
			freq_qos_update_request(&sqos[cluster], cls[cluster].freqs[idx].freq);
	}
}

static void reset_multi_idx(void)
{
	int cluster;

	for_each_cluster(cluster)
		freq_qos_update_request(&sqos[cluster], FREQ_QOS_MAX_DEFAULT_VALUE);
}

static bool is_batt_hot(void)
{
	int temp;

	thermal_zone_get_temp(thermal_zone_get_zone_by_name(batt_tz_names), &temp);

	if (debug)
		pr_info("[%s] battery temp=%d\n", prefix, temp);

	return (temp > batt_temp_thd);
}

static int gov_notifier_call(struct notifier_block *nb, unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	if (state < SYSBUSY_STATE2) {
		gov = 0;
		reset_multi_idx();
		return NOTIFY_OK;
	}

	/* gov thread */
	if (is_gov_enable()) {
		power_limit = power_limits[state];
		if (gov_done) {
			gov_done = 0;
			gov_task = kthread_create(gov_thread, NULL, "xperf_gov%u", 0);
			kthread_bind_mask(gov_task, cls[0].siblings);
			wake_up_process(gov_task);
		}
	}

	/* multi volt */
	if (is_batt_hot())
		set_multi_idx();

	return NOTIFY_OK;
}

static struct notifier_block sysbusy_notifier;

/******************************************************************************
 * helper function                                                            *
 ******************************************************************************/
static void find_multi_idx(void)
{
	int cluster, i;
	unsigned int size;

	for_each_cluster(cluster) {
		if (multi_volt[cluster] < 0) {
			cls[cluster].multi_idx = 0;
			continue;
		}

		size = cls[cluster].freq_size;

		for (i = size - 1; i >= 0; i--) {
			if (multi_volt[cluster] > cls[cluster].freqs[i].volt) {
				cls[cluster].multi_idx = i;
				break;
			}
		}

		if (i < 0)
			cls[cluster].multi_idx = 0;
	}
}

/******************************************************************************
 * sysfs interface                                                            *
 ******************************************************************************/
#define DEF_NODE_RW(name) \
	static ssize_t name##_show(struct kobject *k, struct kobj_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		ret += sprintf(buf + ret, "%d\n", name); \
		return ret; } \
	static ssize_t name##_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) \
	{ \
		if (kstrtouint(buf, 10, &name)) \
			return -EINVAL; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR_RW(name)

DEF_NODE_RW(debug);
DEF_NODE_RW(gov_run);
DEF_NODE_RW(gov_ms);
DEF_NODE_RW(batt_temp_thd);

static ssize_t power_limits_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0, i;

	for (i = 0; i < NUM_OF_SYSBUSY_STATE; i++)
		ret += sprintf(buf + ret, "%d:%d\n", i, power_limits[i]);
	return ret;
}
static ssize_t power_limits_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (sscanf(buf, "%d %d %d %d", &power_limits[0], &power_limits[1], &power_limits[2], &power_limits[3]) != 4)
		return -EINVAL;

	return count;
}
static struct kobj_attribute power_limits_attr = __ATTR_RW(power_limits);

static ssize_t multi_volt_show(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int cluster, ret = 0;
	unsigned int idx;

	for_each_cluster(cluster) {
		idx = cls[cluster].multi_idx;
		ret += sprintf(buf + ret, "cluster%d multi-volt=%-5u multi-freq=%-8u\n",
			       cluster, multi_volt[cluster], cls[cluster].freqs[idx].freq);
	}
	ret += sprintf(buf + ret, "# echo <cluster> <volt> > multi_volt\n");
	return ret;
}

static ssize_t multi_volt_store(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int cluster, val;

	if (sscanf(buf, "%d %d", &cluster, &val) != 2)
		return -EINVAL;
	if (cluster < 0 || cluster >= MAX_CLUSTER)
		return -EINVAL;
	multi_volt[cluster] = val;
	find_multi_idx();
	return count;
}
static struct kobj_attribute multi_volt_attr = __ATTR_RW(multi_volt);

static struct attribute *pago_attrs[] = {
	&debug_attr.attr,
	&gov_run_attr.attr,
	&gov_ms_attr.attr,
	&power_limits_attr.attr,
	&multi_volt_attr.attr,
	&batt_temp_thd_attr.attr,
	NULL
};
static struct attribute_group pago_group = {
	.attrs = pago_attrs,
};

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
static struct kobject *pago_kobj;

static int pago_dt_init(struct device_node *dn)
{
	struct device_node *pago_node;

	pago_node = of_get_child_by_name(dn, "pago");
	if (!pago_node)
		return -ENODATA;

	if (of_property_read_u32(pago_node, "gov-ms", &gov_ms))
		gov_run = 0;
	if (of_property_read_u32(pago_node, "gov-run", &gov_run))
		gov_run = 0;
	if (of_property_read_u32_array(pago_node, "power-limit", power_limits, NUM_OF_SYSBUSY_STATE) < 0)
		gov_run = 0;

	of_property_read_u32_array(pago_node, "multi-volt", multi_volt, cl_cnt);

	of_property_read_u32(pago_node, "batt-temp-thd", &batt_temp_thd);

	if (of_property_read_string(pago_node, "batt-tz-names", &batt_tz_names))
		return -ENODATA;

	find_multi_idx();

	return 0;
}

static int pago_sysfs_init(struct kobject *kobj)
{
	pago_kobj = kobject_create_and_add("pago", kobj);

	if (!pago_kobj) {
		pr_info("[%s] create node failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	if (sysfs_create_group(pago_kobj, &pago_group)) {
		pr_info("[%s] create group failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	return 0;
}

static int pago_fqos_init(void)
{
	int cluster;
	struct cpufreq_policy *policy;

	for_each_cluster(cluster) {
		policy = cpufreq_cpu_get(cls[cluster].first_cpu);
		if (!policy) {
			for_each_cluster(cluster) {
				freq_qos_tracer_remove_request(&gqos[cluster]);
				freq_qos_tracer_remove_request(&sqos[cluster]);
			}

			return -EINVAL;
		}

		freq_qos_tracer_add_request(&policy->constraints,
					    &gqos[cluster],
					    FREQ_QOS_MAX,
					    FREQ_QOS_MAX_DEFAULT_VALUE);
		freq_qos_tracer_add_request(&policy->constraints,
					    &sqos[cluster],
					    FREQ_QOS_MAX,
					    FREQ_QOS_MAX_DEFAULT_VALUE);

		cpufreq_cpu_put(policy);
	}

	return 0;
}

int xperf_pago_init(struct platform_device *pdev, struct kobject *kobj)
{
	if (pago_dt_init(pdev->dev.of_node)) {
		pr_info("[%s] failed to parse device tree\n", prefix);
		return -ENODATA;
	}

	if (pago_sysfs_init(kobj)) {
		pr_info("[%s] failed to initialize sysfs\n", prefix);
		return -EINVAL;
	}

	if (pago_fqos_init()) {
		pr_info("[%s] failed to initialize freq-qos\n", prefix);
		return -EINVAL;
	}

	sysbusy_notifier.notifier_call = gov_notifier_call;
	sysbusy_register_notifier(&sysbusy_notifier);

	return 0;
}
