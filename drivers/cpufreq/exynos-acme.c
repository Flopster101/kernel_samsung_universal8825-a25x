/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/tick.h>
#include <linux/pm_opp.h>
#include <soc/samsung/cpu_cooling.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <uapi/linux/sched/types.h>
#include <linux/ems.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-acme.h>
#include <soc/samsung/exynos-dm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/acme.h>

#include "exynos-acme.h"

#if defined(CONFIG_ARM_EXYNOS_CLDVFS_SYSFS)
#include <asm/io.h>
#endif

#if defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_SEC_FACTORY_INTERPOSER)
#define BOOTING_BOOST_TIME	150000
#else
#define BOOTING_BOOST_TIME	60000
#endif
#else  // !defined(CONFIG_SEC_FACTORY)
#define BOOTING_BOOST_TIME	40000
#endif

#if defined(CONFIG_SEC_FACTORY)
enum {
	FLEXBOOT_LIT = 1,	// LIT Core Flex and All Level Freq Change Allow
	FLEXBOOT_MID,		// MID Core Flex and All Level Freq Change Allow
	FLEXBOOT_BIG,		// BIG Core Flex and All Level Freq Change Allow
	FLEXBOOT_FLEX_ONLY,	// All Core Flex only. max limit
	FLEXBOOT_ALL,		// All Core Flex and All Level Freq Change Allow
};

int flexable_cpu_boot;

module_param(flexable_cpu_boot, int, 0440);
EXPORT_SYMBOL(flexable_cpu_boot);
#endif
/*
 * list head of cpufreq domain
 */
static LIST_HEAD(domains);

/*
 * transition notifier if fast switch is enabled.
 */
static DEFINE_RWLOCK(exynos_cpufreq_transition_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_cpufreq_transition_notifier_list);
static void exynos_cpufreq_notify_transition(struct cpufreq_policy *policy,
					     struct cpufreq_freqs *freqs,
					     unsigned int state,
					     int retval);

/*********************************************************************
 *                          HELPER FUNCTION                          *
 *********************************************************************/
static struct exynos_cpufreq_domain *find_domain(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;

	pr_err("cannot find cpufreq domain by cpu\n");
	return NULL;
}

static void enable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = true;
	mutex_unlock(&domain->lock);
}

static void disable_domain(struct exynos_cpufreq_domain *domain)
{
	mutex_lock(&domain->lock);
	domain->enabled = false;
	mutex_unlock(&domain->lock);
}

static unsigned int resolve_freq_wo_clamp(struct cpufreq_policy *policy,
					unsigned int target_freq, int flag)
{
	unsigned int index = -1;

	if (flag == CPUFREQ_RELATION_L)
		index = cpufreq_table_find_index_al(policy, target_freq);
	else if (flag == CPUFREQ_RELATION_H)
		index = cpufreq_table_find_index_ah(policy, target_freq);

	if (index < 0) {
		pr_err("target frequency(%d) out of range\n", target_freq);
		return 0;
	}

	return policy->freq_table[index].frequency;
}

/* Find lowest freq at or above target in a table in ascending order */
static inline int exynos_cpufreq_find_index(struct exynos_cpufreq_domain *domain,
					unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = domain->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq >= target_freq)
			return idx;

		best = idx;
	}

	return best;
}

/*********************************************************************
 *                   PRE/POST HANDLING FOR SCALING                   *
 *********************************************************************/
static int pre_scale(struct cpufreq_freqs *freqs)
{
	return 0;
}

static int post_scale(struct cpufreq_freqs *freqs)
{
	return 0;
}

/*********************************************************************
 *                         FREQUENCY SCALING                         *
 *********************************************************************/
static unsigned int get_freq(struct exynos_cpufreq_domain *domain)
{
	unsigned int freq;

	/* valid_freq_flag indicates whether boot freq is in the freq table or not.
	 * so, to prevent cpufreq mulfuntion, return old freq instead of cal freq
	 * until the frequency changes even once.
	 */
	if (unlikely(!domain->valid_freq_flag && domain->old))
		return domain->old;

	freq = (unsigned int)cal_dfs_get_rate(domain->cal_id);
	if (!freq) {
		/* On changing state, CAL returns 0 */
		freq = domain->old;
	}

	return freq;
}

static int set_freq(struct exynos_cpufreq_domain *domain,
					unsigned int target_freq)
{
	int err;

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_IN);

	if (domain->fast_switch_possible && domain->dvfs_mode == NON_BLOCKING) {
		err = cal_dfs_set_rate_fast(domain->cal_id, target_freq);
	}
	else
		err = cal_dfs_set_rate(domain->cal_id, target_freq);

	if (err < 0)
		pr_err("failed to scale frequency of domain%d (%d -> %d)\n",
			domain->id, domain->old, target_freq);

	if (!domain->fast_switch_possible || domain->dvfs_mode == BLOCKING)
		trace_acme_scale_freq(domain->id, domain->old, target_freq, "end", 0);

	dbg_snapshot_printk("ID %d: %d -> %d (%d)\n",
		domain->id, domain->old, target_freq, DSS_FLAG_OUT);

	return err;
}

static int scale(struct exynos_cpufreq_domain *domain,
				struct cpufreq_policy *policy,
				unsigned int target_freq)
{
	int ret;
	struct cpufreq_freqs freqs = {
		.policy		= policy,
		.old		= domain->old,
		.new		= target_freq,
		.flags		= 0,
	};

	exynos_cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE, 0);

	dbg_snapshot_freq(domain->dss_type, domain->old, target_freq, DSS_FLAG_IN);

	ret = pre_scale(&freqs);
	if (ret)
		goto fail_scale;

	/* Scale frequency by hooked function, set_freq() */
	ret = set_freq(domain, target_freq);
	if (ret)
		goto fail_scale;

	ret = post_scale(&freqs);
	if (ret)
		goto fail_scale;

fail_scale:
	/* In scaling failure case, logs -1 to exynos snapshot */
	dbg_snapshot_freq(domain->dss_type, domain->old, target_freq,
					ret < 0 ? ret : DSS_FLAG_OUT);

	/* if error occur during frequency scaling, do not set valid_freq_flag */
	if (unlikely(!(ret || domain->valid_freq_flag)))
		domain->valid_freq_flag = true;

	exynos_cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE, ret);

	return ret;
}

/*********************************************************************
 *                   EXYNOS CPUFREQ DRIVER INTERFACE                 *
 *********************************************************************/
static int exynos_cpufreq_init(struct cpufreq_policy *policy)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);

	if (!domain)
		return -EINVAL;

	policy->fast_switch_possible = domain->fast_switch_possible;
	policy->freq_table = domain->freq_table;
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->dvfs_possible_from_any_cpu = true;
	cpumask_copy(policy->cpus, &domain->cpus);

	pr_info("Initialize cpufreq policy%d\n", policy->cpu);

	return 0;
}

static unsigned int exynos_cpufreq_fast_switch(struct cpufreq_policy *policy,
					       unsigned int target_freq)
{
	struct exynos_cpufreq_domain *domain;
	unsigned long fast_switch_freq = (unsigned long)target_freq;
	unsigned long flags;

	domain = find_domain(policy->cpu);
	if (!domain)
		return 0;

	raw_spin_lock_irqsave(&domain->fast_switch_update_lock, flags);
	if (domain->dvfs_mode == NON_BLOCKING) {
		int ret;
		unsigned long old = domain->old;

		trace_acme_scale_freq(domain->id, old, fast_switch_freq, "start", 0);
		ret = DM_CALL(domain->dm_type, &fast_switch_freq);
		if (ret) {
			trace_acme_scale_freq(domain->id, old, fast_switch_freq, "fail", 0);
			fast_switch_freq = 0;
		}
	} else {
		if (!domain->work_in_progress) {
			domain->work_in_progress = true;
			domain->cached_fast_switch_freq = fast_switch_freq;
			trace_acme_scale_freq(domain->id, domain->old, fast_switch_freq, "start", 0);
			irq_work_queue(&domain->fast_switch_irq_work);
		} else
			fast_switch_freq = 0;
	}
	raw_spin_unlock_irqrestore(&domain->fast_switch_update_lock, flags);

	return fast_switch_freq;
}

static unsigned int exynos_cpufreq_resolve_freq(struct cpufreq_policy *policy,
						unsigned int target_freq)
{
	unsigned int index;

	index = cpufreq_frequency_table_target(policy, target_freq, CPUFREQ_RELATION_L);
	if (index < 0) {
		pr_err("target frequency(%d) out of range\n", target_freq);
		return 0;
	}

	return policy->freq_table[index].frequency;
}

static int exynos_cpufreq_verify(struct cpufreq_policy_data *new_policy)
{
	int policy_cpu = new_policy->cpu;
	struct exynos_cpufreq_domain *domain;
	unsigned long max_capacity, capacity;
	struct cpufreq_policy *policy;
	unsigned int min = new_policy->min, max = new_policy->max;

	domain = find_domain(policy_cpu);
	if (!domain)
		return -EINVAL;

	policy = cpufreq_cpu_get(policy_cpu);
	if (!policy)
		goto verify_freq_range;

	/* if minimum frequency is updated, find validate frequency from the table */
	if (min != policy->min)
		min = resolve_freq_wo_clamp(policy, min, CPUFREQ_RELATION_L);

	/* if maximum frequency is updated, find validate frequency from the table */
	if (max != policy->max)
		max = resolve_freq_wo_clamp(policy, max, CPUFREQ_RELATION_H);

	/*
	 * if corrected the minimum frequency is higher than the maximum frequency,
	 * replace it maximum. we don't want that the minimum overs the maximum anytime
	 */
	if (min > max)
		min = max;

	new_policy->min = min;
	new_policy->max = max;

	cpufreq_cpu_put(policy);

verify_freq_range:
	/* clamp with cpuinfo.max/min and check whether valid frequency exist or not */
	cpufreq_frequency_table_verify(new_policy, new_policy->freq_table);

	policy_update_call_to_DM(domain->dm_type,
			new_policy->min, new_policy->max);

	max_capacity = arch_scale_cpu_capacity(policy_cpu);
	capacity = new_policy->max * max_capacity
			/ new_policy->cpuinfo.max_freq;
	arch_set_thermal_pressure(&domain->cpus, max_capacity - capacity);

	return 0;
}

static int __exynos_cpufreq_target(struct cpufreq_policy *policy,
				  unsigned int target_freq,
				  unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	int ret = 0;

	if (!domain)
		return -EINVAL;

	if (!domain->fast_switch_possible || domain->dvfs_mode == BLOCKING)
		mutex_lock(&domain->lock);

	if (!domain->enabled) {
		ret = -EINVAL;
		goto out;
	}

	target_freq = cpufreq_driver_resolve_freq(policy, target_freq);

	/* Target is same as current, skip scaling */
	if (domain->old == target_freq) {
		ret = -EINVAL;
		goto out;
	}

#define TEN_MHZ (10000)
	if (!domain->fast_switch_possible
	    && abs(domain->old - get_freq(domain)) > TEN_MHZ) {
		pr_err("oops, inconsistency between domain->old:%d, real clk:%d\n",
			domain->old, get_freq(domain));
//		BUG_ON(1);
	}
#undef TEN_MHZ

	ret = scale(domain, policy, target_freq);
	if (ret)
		goto out;

	pr_debug("CPUFREQ domain%d frequency change %u kHz -> %u kHz\n",
			domain->id, domain->old, target_freq);

	domain->old = target_freq;

out:
	if (!domain->fast_switch_possible || domain->dvfs_mode == BLOCKING)
		mutex_unlock(&domain->lock);

	return ret;
}

static int exynos_cpufreq_target(struct cpufreq_policy *policy,
					unsigned int target_freq,
					unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = find_domain(policy->cpu);
	unsigned long freq;

	if (!domain)
		return -EINVAL;

	trace_acme_scale_freq(domain->id, domain->old, target_freq, "start", 0);

	if (list_empty(&domain->dm_list))
		return __exynos_cpufreq_target(policy, target_freq, relation);

	freq = (unsigned long)target_freq;

	return DM_CALL(domain->dm_type, &freq);
}

static unsigned int exynos_cpufreq_get(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain = find_domain(cpu);

	if (!domain)
		return 0;

	return get_freq(domain);
}

static int __exynos_cpufreq_suspend(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	unsigned int freq;
	struct work_struct *update_work = &policy->update;

	if (!domain)
		return 0;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq = domain->resume_freq;

	freq_qos_update_request(policy->min_freq_req, freq);
	freq_qos_update_request(policy->max_freq_req, freq);

	flush_work(update_work);

	return 0;
}

static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_suspend(policy, find_domain(policy->cpu));
}

static int __exynos_cpufreq_resume(struct cpufreq_policy *policy,
				struct exynos_cpufreq_domain *domain)
{
	if (!domain)
		return -EINVAL;

	mutex_lock(&domain->lock);
	mutex_unlock(&domain->lock);

	freq_qos_update_request(policy->max_freq_req, domain->max_freq);
	freq_qos_update_request(policy->min_freq_req, domain->min_freq);

	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	return __exynos_cpufreq_resume(policy, find_domain(policy->cpu));
}

static int exynos_cpufreq_update_limit(struct cpufreq_policy *policy)
{
	bool own;
	unsigned long owner = atomic_long_read(&policy->rwsem.owner);

	owner = owner & ~0x7;
	own = (struct task_struct *)owner == current;

	if (own)
		refresh_frequency_limits(policy);
	else {
		down_write(&policy->rwsem);
		refresh_frequency_limits(policy);
		up_write(&policy->rwsem);
	}

	return 0;
}

static int exynos_cpufreq_notifier_min(struct notifier_block *nb,
				       unsigned long freq,
				       void *data)
{
	struct cpufreq_policy *policy =
			container_of(nb, struct cpufreq_policy, nb_min);

	return exynos_cpufreq_update_limit(policy);
}

static int exynos_cpufreq_notifier_max(struct notifier_block *nb,
				       unsigned long freq,
				       void *data)
{
	struct cpufreq_policy *policy =
			container_of(nb, struct cpufreq_policy, nb_max);

	return exynos_cpufreq_update_limit(policy);
}

static void exynos_cpufreq_ready(struct cpufreq_policy *policy)
{
	down_write(&policy->rwsem);
	policy->nb_min.notifier_call = exynos_cpufreq_notifier_min;
	policy->nb_max.notifier_call = exynos_cpufreq_notifier_max;
	up_write(&policy->rwsem);
}

static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct exynos_cpufreq_domain *domain;
	struct cpufreq_policy *policy;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		list_for_each_entry_reverse(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_suspend(policy, domain)) {
				cpufreq_cpu_put(policy);
				return NOTIFY_BAD;
			}
			cpufreq_cpu_put(policy);
		}
		break;
	case PM_POST_SUSPEND:
		list_for_each_entry(domain, &domains, list) {
			policy = cpufreq_cpu_get(cpumask_any(&domain->cpus));
			if (!policy)
				continue;
			if (__exynos_cpufreq_resume(policy, domain)) {
				cpufreq_cpu_put(policy);
				return NOTIFY_BAD;
			}
			cpufreq_cpu_put(policy);
		}
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_pm = {
	.notifier_call = exynos_cpufreq_pm_notifier,
	.priority = INT_MAX,
};

static struct cpufreq_driver exynos_driver = {
	.name		= "exynos_cpufreq",
	.flags		= CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.init		= exynos_cpufreq_init,
	.verify		= exynos_cpufreq_verify,
	.target		= exynos_cpufreq_target,
	.get		= exynos_cpufreq_get,
	.fast_switch	= exynos_cpufreq_fast_switch,
	.resolve_freq	= exynos_cpufreq_resolve_freq,
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
	.ready		= exynos_cpufreq_ready,
	.attr		= cpufreq_generic_attr,
};

/*********************************************************************
 *                       CPUFREQ SYSFS			             *
 *********************************************************************/
#define show_store_freq_qos(type)					\
static ssize_t show_freq_qos_##type(struct device *dev,			\
		struct device_attribute *attr, char *buf)		\
{									\
	ssize_t count = 0;						\
	struct exynos_cpufreq_domain *domain;				\
									\
	list_for_each_entry(domain, &domains, list)			\
		count += snprintf(buf + count, 30,			\
			"policy%d: qos_%s: %d\n",			\
			cpumask_first(&domain->cpus), #type,		\
			domain->user_##type##_qos_req.pnode.prio);	\
									\
	return count;							\
}									\
									\
static ssize_t store_freq_qos_##type(struct device *dev,		\
				struct device_attribute *attr,		\
				const char *buf, size_t count)		\
{									\
	int freq, cpu;							\
	struct exynos_cpufreq_domain *domain;				\
									\
	if (!sscanf(buf, "%d %8d", &cpu, &freq))			\
		return -EINVAL;						\
									\
	if (cpu < 0 || cpu >= NR_CPUS || freq < 0)			\
		return -EINVAL;						\
									\
	domain = find_domain(cpu);					\
	if (!domain)							\
		return -EINVAL;						\
									\
	freq_qos_update_request(&domain->user_##type##_qos_req, freq);	\
									\
	return count;							\
}

show_store_freq_qos(min);
show_store_freq_qos(max);

static ssize_t show_dvfs_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_cpufreq_domain *domain;

	list_for_each_entry(domain, &domains, list)
		count += snprintf(buf + count, PAGE_SIZE, "policy%d: DVFS Mode: %s\n",
				cpumask_first(&domain->cpus),
				!!domain->dvfs_mode ? "BLOCKING":"NON_BLOCKING");
	count += snprintf(buf + count, PAGE_SIZE, "Usage: echo [cpu] [mode] > dvfs_mode (mode = 0: NON_BLOCKING/1: BLOCKING)\n");

	return count;
}

static ssize_t store_dvfs_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int mode, cpu;
	struct exynos_cpufreq_domain *domain;

	if (!sscanf(buf, "%d %8d", &cpu, &mode))
		return -EINVAL;

	if (cpu < 0 || cpu >= NR_CPUS || mode < 0)
		return -EINVAL;

	domain = find_domain(cpu);
	if (!domain)
		return -EINVAL;

	domain->dvfs_mode = mode;

	return count;
}

static DEVICE_ATTR(freq_qos_max, S_IRUGO | S_IWUSR,
		show_freq_qos_max, store_freq_qos_max);
static DEVICE_ATTR(freq_qos_min, S_IRUGO | S_IWUSR,
		show_freq_qos_min, store_freq_qos_min);
static DEVICE_ATTR(dvfs_mode, S_IRUGO | S_IWUSR,
		show_dvfs_mode, store_dvfs_mode);

#if defined(CONFIG_ARM_EXYNOS_CLDVFS_SYSFS)
/****************************************************************/
/*			CL-DVFS SYSFS INTERFACE				*/
/****************************************************************/
u32 cldc, cldp, cldol;
#define CLDVFS_BASE		0x1A320000
#define CLDVFS_CONTROL_OFFSET		0xE10
#define CLDVFS_PMICCAL_OFFSET		0xE14
#define CLDVFS_OUTERLOG_OFFSET		0xE18

static void __iomem *cldvfs_base;

static ssize_t cldc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 cldc_read = 0;

	cldc_read = __raw_readl(cldvfs_base + CLDVFS_CONTROL_OFFSET);

	return snprintf(buf, 30, "0x%x\n", cldc_read);
}

static ssize_t cldc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 input;

	if (kstrtouint(buf, 0, &input))
		return -EINVAL;

	cldc = input;
	__raw_writel(cldc, cldvfs_base + CLDVFS_CONTROL_OFFSET);

	return count;
}
DEVICE_ATTR_RW(cldc);

static ssize_t cldp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 cldp_read = 0;

	cldp_read = __raw_readl(cldvfs_base + CLDVFS_PMICCAL_OFFSET);

	return snprintf(buf, 30, "0x%x\n", cldp_read);
}

static ssize_t cldp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 input;

	if (kstrtouint(buf, 0, &input))
		return -EINVAL;

	cldp = input;
	__raw_writel(cldp, cldvfs_base + CLDVFS_PMICCAL_OFFSET);

	return count;
}
DEVICE_ATTR_RW(cldp);

static ssize_t cldol_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 cldol_read = 0;

	cldol_read = __raw_readl(cldvfs_base + CLDVFS_OUTERLOG_OFFSET);

	return snprintf(buf, 30, "0x%x\n", cldol_read);
}

static ssize_t cldol_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 input;

	if (kstrtouint(buf, 0, &input))
		return -EINVAL;

	cldol = input;
	__raw_writel(cldol, cldvfs_base + CLDVFS_OUTERLOG_OFFSET);

return count;
}
DEVICE_ATTR_RW(cldol);

static struct attribute *exynos_cldvfs_attrs[] = {
	&dev_attr_cldc.attr,
	&dev_attr_cldp.attr,
	&dev_attr_cldol.attr,
	NULL,
};

static struct attribute_group exynos_cldvfs_group = {
	.name = "cldvfs",
	.attrs = exynos_cldvfs_attrs,
};
#endif

/*********************************************************************
 *                        TRANSITION NOTIFIER                        *
 *********************************************************************/
static int exynos_cpufreq_fast_switch_count;

static void exynos_cpufreq_notify_transition_slow(struct cpufreq_policy *policy,
						  struct cpufreq_freqs *freqs,
						  unsigned int state,
						  int retval)
{
	switch (state) {
	case CPUFREQ_PRECHANGE:
		cpufreq_freq_transition_begin(policy, freqs);
		break;

	case CPUFREQ_POSTCHANGE:
		cpufreq_freq_transition_end(policy, freqs, retval);
		break;

	default:
		BUG();
	}
}

static void exynos_cpufreq_notify_transition(struct cpufreq_policy *policy,
					     struct cpufreq_freqs *freqs,
					     unsigned int state,
					     int retval)
{
	if (exynos_cpufreq_fast_switch_count == 0) {
		exynos_cpufreq_notify_transition_slow(policy, freqs, state, retval);
		return;
	}

	read_lock(&exynos_cpufreq_transition_notifier_lock);

	switch (state) {
	case CPUFREQ_PRECHANGE:
		raw_notifier_call_chain(&exynos_cpufreq_transition_notifier_list,
					CPUFREQ_PRECHANGE, freqs);
		break;

	case CPUFREQ_POSTCHANGE:
		raw_notifier_call_chain(&exynos_cpufreq_transition_notifier_list,
					CPUFREQ_POSTCHANGE, freqs);

		/* Transition failed */
		if (retval) {
			swap(freqs->old, freqs->new);
			raw_notifier_call_chain(&exynos_cpufreq_transition_notifier_list,
						CPUFREQ_PRECHANGE, freqs);
			raw_notifier_call_chain(&exynos_cpufreq_transition_notifier_list,
						CPUFREQ_POSTCHANGE, freqs);
		}

		break;

	default:
		BUG();
	}

	read_unlock(&exynos_cpufreq_transition_notifier_lock);
}

int exynos_cpufreq_register_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret;
	unsigned long flags;

	if (exynos_cpufreq_fast_switch_count == 0)
		return cpufreq_register_notifier(nb, list);

	write_lock_irqsave(&exynos_cpufreq_transition_notifier_lock, flags);

	switch (list) {
	case CPUFREQ_TRANSITION_NOTIFIER:
		ret = raw_notifier_chain_register(
				&exynos_cpufreq_transition_notifier_list, nb);
		break;
	default:
		pr_warn("Unsupported notifier (list=%u)\n", list);
		ret = -EINVAL;
	}

	write_unlock_irqrestore(&exynos_cpufreq_transition_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL(exynos_cpufreq_register_notifier);

int exynos_cpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list)
{
	int ret;
	unsigned long flags;

	if (exynos_cpufreq_fast_switch_count == 0)
		return cpufreq_unregister_notifier(nb, list);

	write_lock_irqsave(&exynos_cpufreq_transition_notifier_lock, flags);

	switch (list) {
	case CPUFREQ_TRANSITION_NOTIFIER:
		ret = raw_notifier_chain_unregister(
				&exynos_cpufreq_transition_notifier_list, nb);
		break;
	default:
		pr_warn("Unsupported notifier (list=%u)\n", list);
		ret = -EINVAL;
	}

	write_unlock_irqrestore(&exynos_cpufreq_transition_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL(exynos_cpufreq_unregister_notifier);

/*********************************************************************
 *                       CPUFREQ DEV FOPS                            *
 *********************************************************************/

static ssize_t cpufreq_fops_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct freq_qos_request *req = filp->private_data;
	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	freq_qos_update_request(req, value);

	return count;
}

static ssize_t cpufreq_fops_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value = 0;
	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static int cpufreq_fops_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct exynos_cpufreq_file_operations *fops = container_of(filp->f_op,
			struct exynos_cpufreq_file_operations,
			fops);
	struct freq_qos_request *req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	filp->private_data = req;
	ret = freq_qos_tracer_add_request(fops->freq_constraints, req, fops->req_type, fops->default_value);
	if (ret)
		return ret;

	return 0;
}

static int cpufreq_fops_release(struct inode *inode, struct file *filp)
{
	struct freq_qos_request *req = filp->private_data;

	freq_qos_tracer_remove_request(req);
	kfree(req);

	return 0;
}

/*********************************************************************
 *                          MULTI DM TABLE                           *
 *********************************************************************/
static struct exynos_cpufreq_dm *
find_multi_table_dm(struct list_head *dm_list)
{
	struct exynos_cpufreq_dm *dm;

	list_for_each_entry(dm, dm_list, list)
		if (dm->multi_table)
			return dm;

	return NULL;
}

struct work_struct dm_work;
static int dm_table_index;
static int cur_type;
static void change_dm_table_work(struct work_struct *work)
{
	struct exynos_cpufreq_domain *domain;
	struct exynos_cpufreq_dm *dm;
	int index = 0;

	if ((cur_type & EMSTUNE_MODE_TYPE_GAME) &&
			!(cur_type & EMSTUNE_BOOST_TYPE_EXTREME))
		index = 1;

	if (dm_table_index != index) {
		list_for_each_entry(domain, &domains, list) {
			dm = find_multi_table_dm(&domain->dm_list);
			if (dm)
				exynos_dm_change_freq_table(&dm->c, index);
		}
		dm_table_index = index;
	}
}

static int exynos_cpufreq_mode_update_callback(struct notifier_block *nb,
		unsigned long val, void *v)
{
	cur_type = emstune_cpu_dsu_table_index(v);

	schedule_work(&dm_work);

	return NOTIFY_OK;
}
static struct notifier_block exynos_cpufreq_mode_update_notifier = {
	.notifier_call = exynos_cpufreq_mode_update_callback,
};

/*********************************************************************
 *                       EXTERNAL EVENT HANDLER                      *
 *********************************************************************/
static int exynos_cpu_fast_switch_notifier(struct notifier_block *notifier,
				       unsigned long domain_id, void *__data)
{
	struct exynos_cpufreq_domain *domain = NULL;
	struct exynos_dm_fast_switch_notify_data *data = __data;
	unsigned int freq = data->freq;
	ktime_t time = data->time;

	list_for_each_entry(domain, &domains, list)
		if ((domain->cal_id & 0xffff) == domain_id)
			break;

	if (unlikely(!domain))
		return NOTIFY_DONE;

	if (!domain->fast_switch_possible || domain->dvfs_mode == BLOCKING)
		return NOTIFY_BAD;

	trace_acme_scale_freq(domain->id, 0, freq, "end", (unsigned long)ktime_to_us(time));

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_fast_switch_nb = {
	.notifier_call = exynos_cpu_fast_switch_notifier,
};

/*********************************************************************
 *                      SUPPORT for DVFS MANAGER                     *
 *********************************************************************/
static int
init_constraint_table_ect(struct exynos_dm_freq *dm_table, int table_length,
						struct device_node *dn)
{
	void *block;
	struct ect_minlock_domain *ect_domain;
	const char *ect_name;
	unsigned int index, c_index;
	bool valid_row = false;
	int ret;

	ret = of_property_read_string(dn, "ect-name", &ect_name);
	if (ret)
		return ret;

	block = ect_get_block(BLOCK_MINLOCK);
	if (!block)
		return -ENODEV;

	ect_domain = ect_minlock_get_domain(block, (char *)ect_name);
	if (!ect_domain)
		return -ENODEV;

	for (index = 0; index < table_length; index++) {
		unsigned int freq = dm_table[index].master_freq;

		for (c_index = 0; c_index < ect_domain->num_of_level; c_index++) {
			/* find row same as frequency */
			if (freq == ect_domain->level[c_index].main_frequencies) {
				dm_table[index].slave_freq
					= ect_domain->level[c_index].sub_frequencies;
				valid_row = true;
				break;
			}
		}

		/*
		 * Due to higher levels of constraint_freq should not be NULL,
		 * they should be filled with highest value of sub_frequencies
		 * of ect until finding first(highest) domain frequency fit with
		 * main_frequeucy of ect.
		 */
		if (!valid_row)
			dm_table[index].slave_freq
				= ect_domain->level[0].sub_frequencies;
	}

	return 0;
}

static int
init_constraint_table_dt(struct exynos_dm_freq *dm_table, int table_length,
						struct device_node *dn)
{
	struct exynos_dm_freq *table;
	int size, table_size, index, c_index;

	/*
	 * A DM constraint table row consists of master and slave frequency
	 * value, the size of a row is 64bytes. Divide size in half when
	 * table is allocated.
	 */
	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	table_size = size / 2;
	table = kzalloc(sizeof(struct exynos_dm_freq) * table_size, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	of_property_read_u32_array(dn, "table", (unsigned int *)table, size);

	for (index = 0; index < table_length; index++) {
		unsigned int freq = dm_table[index].master_freq;

		for (c_index = 0; c_index < table_size; c_index++) {
			if (freq <= table[c_index].master_freq) {
				dm_table[index].slave_freq
					= table[c_index].slave_freq;
				break;
			}
		}
	}

	kfree(table);
	return 0;
}

void adjust_freq_table_with_min_max_freq(unsigned int *freq_table,
		int table_size, struct exynos_cpufreq_domain *domain)
{
	int index;

	for (index = 0; index < table_size; index++) {
		if (freq_table[index] == domain->min_freq)
			break;

		if (freq_table[index] > domain->min_freq) {
			freq_table[index - 1] = domain->min_freq;
			break;
		}
	}

	for (index = table_size - 1; index >= 0; index--) {
		if (freq_table[index] == domain->max_freq)
			return;

		if (freq_table[index] < domain->max_freq) {
			freq_table[index + 1] = domain->max_freq;
			return;
		}
	}
}

static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	struct exynos_cpufreq_domain *domain = devdata;
	struct cpufreq_policy *policy;
	struct cpumask mask;
	int ret;

	/* Skip scaling if all cpus of domain are hotplugged out */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_empty(&mask))
		return -ENODEV;

	policy = cpufreq_cpu_get(cpumask_first(&mask));
	if (!policy) {
		pr_err("%s: failed get cpufreq policy\n", __func__);
		return -ENODEV;
	}

	ret = __exynos_cpufreq_target(policy, target_freq, relation);

	cpufreq_cpu_put(policy);

	return ret;
}

static int init_dm(struct exynos_cpufreq_domain *domain,
				struct device_node *dn)
{
	struct exynos_cpufreq_dm *dm;
	struct device_node *root;
	struct of_phandle_iterator iter;
	int ret, err;

	ret = of_property_read_u32(dn, "dm-type", &domain->dm_type);
	if (ret)
		return ret;

	ret = exynos_dm_data_init(domain->dm_type, domain, domain->min_freq,
				domain->max_freq, domain->min_freq);
	if (ret)
		return ret;

	/* Initialize list head of DVFS Manager constraints */
	INIT_LIST_HEAD(&domain->dm_list);

	/*
	 * Initialize DVFS Manager constraints
	 * - constraint_type : minimum or maximum constraint
	 * - constraint_dm_type : cpu/mif/int/.. etc
	 * - guidance : constraint from chipset characteristic
	 * - freq_table : constraint table
	 */
	root = of_get_child_by_name(dn, "dm-constraints");
	of_for_each_phandle(&iter, err, root, "list", NULL, 0) {
		struct exynos_dm_freq *dm_table;
		int index, r_index;
		bool multi_table = false;

		if (of_property_read_bool(iter.node, "multi-table"))
			multi_table = true;

		if (multi_table) {
			dm = find_multi_table_dm(&domain->dm_list);
			if (dm)
				goto skip_init_dm;
		}

		/* allocate DM constraint */
		dm = kzalloc(sizeof(struct exynos_cpufreq_dm), GFP_KERNEL);
		if (!dm)
			goto init_fail;

		list_add_tail(&dm->list, &domain->dm_list);

		of_property_read_u32(iter.node, "const-type", &dm->c.constraint_type);
		of_property_read_u32(iter.node, "dm-slave", &dm->c.dm_slave);
		of_property_read_u32(iter.node, "master-cal-id", &dm->master_cal_id);
		of_property_read_u32(iter.node, "slave-cal-id", &dm->slave_cal_id);

		/* dynamic disable for migov control */
		if (of_property_read_bool(iter.node, "dynamic-disable"))
			dm->c.support_dynamic_disable = true;

		dm->multi_table = multi_table;

skip_init_dm:
		/* allocate DM constraint table */
		dm_table = kcalloc(domain->table_size, sizeof(struct exynos_dm_freq), GFP_KERNEL);
		if (!dm_table)
			goto init_fail;

		/*
		 * fill master freq, domain frequency table is in ascending
		 * order, but DM constraint table must be in descending
		 * order.
		 */
		index = 0;
		r_index = domain->table_size - 1;
		while (r_index >= 0) {
			dm_table[index].master_freq =
				domain->freq_table[r_index].frequency;
			index++;
			r_index--;
		}

		/* fill slave freq */
		if (of_property_read_bool(iter.node, "guidance")) {
			dm->c.guidance = true;
			if (init_constraint_table_ect(dm_table,
					domain->table_size, iter.node))
				continue;
		} else {
			if (init_constraint_table_dt(dm_table,
					domain->table_size, iter.node))
				continue;
		}

		dm->c.table_length = domain->table_size;

		if (dm->multi_table) {
			/*
			 * DM supports only 2 variable_freq_table.
			 * It should support table extension.
			 */
			if (!dm->c.variable_freq_table[0]) {
				dm->c.variable_freq_table[0] = dm_table;

				/*
				 * Do not register DM constraint
				 * unless both tables are initialized
				 */
				continue;
			} else
				dm->c.variable_freq_table[1] = dm_table;

			dm->c.support_variable_freq_table = true;
		} else
			dm->c.freq_table = dm_table;

		/* register DM constraint */
		ret = register_exynos_dm_constraint_table(domain->dm_type, &dm->c);
		if (ret)
			goto init_fail;
	}

	return register_exynos_dm_freq_scaler(domain->dm_type, dm_scaler);

init_fail:
	while (!list_empty(&domain->dm_list)) {
		dm = list_last_entry(&domain->dm_list,
				struct exynos_cpufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SEC_BOOTSTAT)
void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;
	int cluster;
	struct exynos_cpufreq_domain *domain;

	get_online_cpus();
	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu) {
		domain = find_domain(cpu);
		if (!domain)
			continue;
		pr_err("%s, dm type = %d\n", __func__, domain->dm_type);
		cluster = 0;
		if (domain->dm_type == DM_CPU_CL1)
			cluster = 1;
		else if (domain->dm_type == DM_CPU_CL2)
			cluster = 2;

		freq[cluster] = get_freq(domain);
	}
	put_online_cpus();
}
EXPORT_SYMBOL(sec_bootstat_get_cpuinfo);
#endif

/*********************************************************************
 *                         CPU HOTPLUG CALLBACK                      *
 *********************************************************************/
static int exynos_cpufreq_cpu_up_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed before cpufreq_resume() is called.
	 * Therefore, if it is called by enable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(cpu);
	if (!domain)
		return 0;

	/*
	 * The first incoming cpu in domain enables frequency scaling
	 * and clears limit of frequency.
	 */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&mask) == 1) {
		enable_domain(domain);
		freq_qos_update_request(&domain->max_qos_req, domain->max_freq);
	}

	return 0;
}

static int exynos_cpufreq_cpu_down_callback(unsigned int cpu)
{
	struct exynos_cpufreq_domain *domain;
	struct cpumask mask;

	/*
	 * CPU frequency is not changed after cpufreq_suspend() is called.
	 * Therefore, if it is called by disable_nonboot_cpus(),
	 * it is ignored.
	 */
	if (cpuhp_tasks_frozen)
		return 0;

	domain = find_domain(cpu);
	if (!domain)
		return 0;

	/*
	 * The last outgoing cpu in domain limits frequency to minimum
	 * and disables frequency scaling.
	 */
	cpumask_and(&mask, &domain->cpus, cpu_online_mask);
	if (cpumask_weight(&mask) == 1) {
		freq_qos_update_request(&domain->max_qos_req, domain->min_freq);
		disable_domain(domain);
	}

	return 0;
}

/*********************************************************************
 *                  INITIALIZE EXYNOS CPUFREQ DRIVER                 *
 *********************************************************************/
static void print_domain_info(struct exynos_cpufreq_domain *domain)
{
	int i;
	char buf[10];

	pr_info("CPUFREQ of domain%d cal-id : %#x\n",
			domain->id, domain->cal_id);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&domain->cpus));
	pr_info("CPUFREQ of domain%d sibling cpus : %s\n",
			domain->id, buf);

	pr_info("CPUFREQ of domain%d boot freq = %d kHz resume freq = %d kHz\n",
			domain->id, domain->boot_freq, domain->resume_freq);

	pr_info("CPUFREQ of domain%d max freq : %d kHz, min freq : %d kHz\n",
			domain->id,
			domain->max_freq, domain->min_freq);

	pr_info("CPUFREQ of domain%d table size = %d\n",
			domain->id, domain->table_size);

	for (i = 0; i < domain->table_size; i++) {
		if (domain->freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		pr_info("CPUFREQ of domain%d : L%-2d  %7d kHz\n",
			domain->id,
			domain->freq_table[i].driver_data,
			domain->freq_table[i].frequency);
	}
}

static void init_sysfs(struct kobject *kobj)
{
	if (sysfs_create_file(kobj, &dev_attr_freq_qos_max.attr))
		pr_err("failed to create user_max node\n");

	if (sysfs_create_file(kobj, &dev_attr_freq_qos_min.attr))
		pr_err("failed to create user_min node\n");

	if (sysfs_create_file(kobj, &dev_attr_dvfs_mode.attr))
		pr_err("failed to create dvfs_mode node\n");
}

static void freq_qos_release(struct work_struct *work)
{
	struct exynos_cpufreq_domain *domain = container_of(to_delayed_work(work),
						  struct exynos_cpufreq_domain,
						  work);

	freq_qos_update_request(&domain->min_qos_req, domain->min_freq);
	freq_qos_update_request(&domain->max_qos_req, domain->max_freq);
}

static int
init_freq_qos(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy)
{
	unsigned int boot_qos, val;
	struct device_node *dn = domain->dn;
	int ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->user_min_qos_req,
				FREQ_QOS_MIN, domain->min_freq);
	if (ret < 0)
		return ret;

	ret = freq_qos_tracer_add_request(&policy->constraints, &domain->user_max_qos_req,
				FREQ_QOS_MAX, domain->max_freq);
	if (ret < 0)
		return ret;

	/*
	 * Basically booting pm_qos is set to max frequency of domain.
	 * But if pm_qos-booting exists in device tree,
	 * booting pm_qos is selected to smaller one
	 * between max frequency of domain and the value defined in device tree.
	 */
	boot_qos = domain->max_freq;
	if (!of_property_read_u32(dn, "pm_qos-booting", &val))
		boot_qos = min(boot_qos, val);

#if defined(CONFIG_SEC_FACTORY)
	pr_info("%s: flexable_cpu_boot = %d\n", __func__, flexable_cpu_boot);

	if (flexable_cpu_boot == FLEXBOOT_ALL) {
		pr_info("All skip boot cpu[%d] max qos lock\n", domain->id);
	} else if ((flexable_cpu_boot >= FLEXBOOT_LIT) && (flexable_cpu_boot <= FLEXBOOT_BIG)) {
		if (domain->id == (flexable_cpu_boot - 1))
			pr_info("skip boot cpu[%d] max qos lock\n", domain->id);
		else
			freq_qos_update_request(&domain->max_qos_req, boot_qos);
	} else if (flexable_cpu_boot == FLEXBOOT_FLEX_ONLY) {
		freq_qos_update_request(&domain->max_qos_req, boot_qos);
	} else {
		freq_qos_update_request(&domain->min_qos_req, boot_qos);
		freq_qos_update_request(&domain->max_qos_req, boot_qos);
	}
#else
	freq_qos_update_request(&domain->min_qos_req, boot_qos);
	freq_qos_update_request(&domain->max_qos_req, boot_qos);
#endif
	pr_info("domain%d operates at %dKHz for %d secs\n", domain->id, boot_qos, BOOTING_BOOST_TIME/1000);

	/* booting boost, it is expired after BOOTING_BOOST_TIME */
	INIT_DELAYED_WORK(&domain->work, freq_qos_release);
	schedule_delayed_work(&domain->work, msecs_to_jiffies(BOOTING_BOOST_TIME));

	return 0;
}

static int
init_fops(struct exynos_cpufreq_domain *domain, struct cpufreq_policy *policy)
{
	char *node_name_buffer;
	int ret, buffer_size;;

	buffer_size = sizeof(char [64]);
	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_min", domain->id);

	domain->min_qos_fops.fops.write		= cpufreq_fops_write;
	domain->min_qos_fops.fops.read		= cpufreq_fops_read;
	domain->min_qos_fops.fops.open		= cpufreq_fops_open;
	domain->min_qos_fops.fops.release	= cpufreq_fops_release;
	domain->min_qos_fops.fops.llseek	= noop_llseek;

	domain->min_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->min_qos_fops.miscdev.name	= node_name_buffer;
	domain->min_qos_fops.miscdev.fops	= &domain->min_qos_fops.fops;

	domain->min_qos_fops.freq_constraints	= &policy->constraints;
	domain->min_qos_fops.default_value	= FREQ_QOS_MIN_DEFAULT_VALUE;
	domain->min_qos_fops.req_type		= FREQ_QOS_MIN;

	ret = misc_register(&domain->min_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device min for domain %d", domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	node_name_buffer = kzalloc(buffer_size, GFP_KERNEL);
	if (node_name_buffer == NULL)
		return -ENOMEM;

	snprintf(node_name_buffer, buffer_size,
			"cluster%d_freq_max", domain->id);

	domain->max_qos_fops.fops.write		= cpufreq_fops_write;
	domain->max_qos_fops.fops.read		= cpufreq_fops_read;
	domain->max_qos_fops.fops.open		= cpufreq_fops_open;
	domain->max_qos_fops.fops.release	= cpufreq_fops_release;
	domain->max_qos_fops.fops.llseek	= noop_llseek;

	domain->max_qos_fops.miscdev.minor	= MISC_DYNAMIC_MINOR;
	domain->max_qos_fops.miscdev.name	= node_name_buffer;
	domain->max_qos_fops.miscdev.fops	= &domain->max_qos_fops.fops;

	domain->max_qos_fops.freq_constraints	= &policy->constraints;
	domain->max_qos_fops.default_value	= FREQ_QOS_MAX_DEFAULT_VALUE;
	domain->max_qos_fops.req_type		= FREQ_QOS_MAX;

	ret = misc_register(&domain->max_qos_fops.miscdev);
	if (ret) {
		pr_err("CPUFREQ couldn't register misc device max for domain %d", domain->id);
		kfree(node_name_buffer);
		return ret;
	}

	return 0;
}

struct freq_volt {
	unsigned int freq;
	unsigned int volt;
};

static void exynos_cpufreq_irq_work(struct irq_work *irq_work)
{
	struct exynos_cpufreq_domain *domain;

	domain = container_of(irq_work,
			      struct exynos_cpufreq_domain,
			      fast_switch_irq_work);
	if (unlikely(!domain))
		return;

	kthread_queue_work(&domain->fast_switch_worker,
			   &domain->fast_switch_work);
}

static void exynos_cpufreq_work(struct kthread_work *work)
{
	struct exynos_cpufreq_domain *domain;
	unsigned long flags;
	unsigned long freq;

	domain = container_of(work, struct exynos_cpufreq_domain, fast_switch_work);
	if (unlikely(!domain))
		return;

	raw_spin_lock_irqsave(&domain->fast_switch_update_lock, flags);
	freq = (unsigned long)domain->cached_fast_switch_freq;
	domain->work_in_progress = false;
	raw_spin_unlock_irqrestore(&domain->fast_switch_update_lock, flags);

	if (DM_CALL(domain->dm_type, &freq))
		trace_acme_scale_freq(domain->id, domain->old, freq, "fail", 0);
}

static int init_fast_switch(struct exynos_cpufreq_domain *domain,
			    struct device_node *dn)
{
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_USER_RT_PRIO / 2 };
	struct cpumask mask;
	const char *buf;
	int ret;

	domain->fast_switch_possible = of_property_read_bool(dn, "fast-switch");
	if (!domain->fast_switch_possible)
		return 0;

	init_irq_work(&domain->fast_switch_irq_work, exynos_cpufreq_irq_work);
	kthread_init_work(&domain->fast_switch_work, exynos_cpufreq_work);
	kthread_init_worker(&domain->fast_switch_worker);
	thread = kthread_create(kthread_worker_fn, &domain->fast_switch_worker,
				"fast_switch:%d", cpumask_first(&domain->cpus));
	if (IS_ERR(thread)) {
		pr_err("failed to create fast_switch thread: %ld\n", PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: failed to set SCHED_CLASS\n", __func__);
		return ret;
	}

	cpumask_copy(&mask, cpu_possible_mask);
	if (!of_property_read_string(dn, "thread-run-on", &buf))
		cpulist_parse(buf, &mask);

	set_cpus_allowed_ptr(thread, &mask);
	thread->flags |= PF_NO_SETAFFINITY;

	raw_spin_lock_init(&domain->fast_switch_update_lock);
	domain->thread = thread;

	wake_up_process(thread);

	exynos_cpufreq_fast_switch_count++;

	return 0;
}

static int init_domain(struct exynos_cpufreq_domain *domain,
					struct device_node *dn)
{
	unsigned int val, raw_table_size;
	int index, i;
	unsigned int freq_table[100];
	struct freq_volt *fv_table;
	const char *buf;
	int cpu;
	int ret;
	int cur_idx;

	/*
	 * Get cpumask which belongs to domain.
	 */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return ret;

	cpulist_parse(buf, &domain->cpus);
	cpumask_and(&domain->cpus, &domain->cpus, cpu_possible_mask);
	if (cpumask_weight(&domain->cpus) == 0)
		return -ENODEV;

	/* Get CAL ID */
	ret = of_property_read_u32(dn, "cal-id", &domain->cal_id);
	if (ret)
		return ret;

	/* Get DSS type */
	if (of_property_read_u32(dn, "dss-type", &domain->dss_type)) {
		pr_info("%s:dss_type is not initialized. domain_id replaces it.", __func__);
		domain->dss_type = domain->id;
	}

	/*
	 * Set min/max frequency.
	 * If max-freq property exists in device tree, max frequency is
	 * selected to smaller one between the value defined in device
	 * tree and CAL. In case of min-freq, min frequency is selected
	 * to bigger one.
	 */
	domain->max_freq = cal_dfs_get_max_freq(domain->cal_id);
	domain->min_freq = cal_dfs_get_min_freq(domain->cal_id);

	if (!of_property_read_u32(dn, "max-freq", &val))
		domain->max_freq = min(domain->max_freq, val);
	if (!of_property_read_u32(dn, "min-freq", &val))
		domain->min_freq = max(domain->min_freq, val);

	/* Get freq-table from device tree and cut the out of range */
	raw_table_size = of_property_count_u32_elems(dn, "freq-table");
	if (of_property_read_u32_array(dn, "freq-table",
				freq_table, raw_table_size)) {
		pr_err("%s: freq-table does not exist\n", __func__);
		return -ENODATA;
	}

	/*
	 * If the ECT's min/max frequency are asynchronous with the dts',
	 * adjust the freq table with the ECT's min/max frequency.
	 * It only supports the situations when the ECT's min is higher than the dts'
	 * or the ECT's max is lower than the dts'.
	 */
	adjust_freq_table_with_min_max_freq(freq_table, raw_table_size, domain);

	domain->table_size = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] > domain->max_freq ||
		    freq_table[index] < domain->min_freq) {
			freq_table[index] = CPUFREQ_ENTRY_INVALID;
			continue;
		}

		domain->table_size++;
	}

	/*
	 * Get volt table from CAL with given freq-table
	 * cal_dfs_get_freq_volt_table() is called by filling the desired
	 * frequency in fv_table, the corresponding volt is filled.
	 */
	fv_table = kzalloc(sizeof(struct freq_volt)
				* (domain->table_size), GFP_KERNEL);
	if (!fv_table) {
		pr_err("%s: failed to alloc fv_table\n", __func__);
		return -ENOMEM;
	}

	i = 0;
	for (index = 0; index < raw_table_size; index++) {
		if (freq_table[index] == CPUFREQ_ENTRY_INVALID)
			continue;
		fv_table[i].freq = freq_table[index];
		i++;
	}

	if (cal_dfs_get_freq_volt_table(domain->cal_id,
				fv_table, domain->table_size)) {
		pr_err("%s: failed to get fv table from CAL\n", __func__);
		kfree(fv_table);
		return -EINVAL;
	}

	/*
	 * Allocate and initialize frequency table.
	 * Last row of frequency table must be set to CPUFREQ_TABLE_END.
	 * Table size should be one larger than real table size.
	 */
	domain->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
				* (domain->table_size + 1), GFP_KERNEL);
	if (!domain->freq_table) {
		kfree(fv_table);
		return -ENOMEM;
	}

	for (index = 0; index < domain->table_size; index++) {
		domain->freq_table[index].driver_data = index;
		domain->freq_table[index].frequency = fv_table[index].freq;
	}
	domain->freq_table[index].driver_data = index;
	domain->freq_table[index].frequency = CPUFREQ_TABLE_END;

	/*
	 * Add OPP table for thermal.
	 * Thermal CPU cooling is based on the OPP table.
	 */
	for (index = domain->table_size - 1; index >= 0; index--) {
		for_each_cpu_and(cpu, &domain->cpus, cpu_possible_mask)
			dev_pm_opp_add(get_cpu_device(cpu),
				fv_table[index].freq * 1000,
				fv_table[index].volt);
	}

	kfree(fv_table);

	/*
	 * Initialize other items.
	 */
	domain->boot_freq = cal_dfs_get_boot_freq(domain->cal_id);
	domain->resume_freq = cal_dfs_get_resume_freq(domain->cal_id);
	domain->old = get_freq(domain);
	if (domain->old < domain->min_freq || domain->max_freq < domain->old) {
		WARN(1, "Out-of-range freq(%dkhz) returned for domain%d in init time\n",
					domain->old,  domain->id);
		domain->old = domain->boot_freq;
	}

	cur_idx = exynos_cpufreq_find_index(domain, domain->old);
	domain->old = domain->freq_table[cur_idx].frequency;

	mutex_init(&domain->lock);

	/*
	 * Initialize CPUFreq DVFS Manager
	 * DVFS Manager is the optional function, it does not check return value
	 */
	init_dm(domain, dn);

	/* Register EM to device of CPU in this domain */
	cpu = cpumask_first(&domain->cpus);
	dev_pm_opp_of_register_em(get_cpu_device(cpu), &domain->cpus);

	/* Initialize fields to test fast switch */
	init_fast_switch(domain, dn);

	pr_info("Complete to initialize cpufreq-domain%d\n", domain->id);

	return 0;
}

static int acme_cpufreq_policy_notify_callback(struct notifier_block *nb,
					       unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct exynos_cpufreq_domain *domain;
	int ret;

	if (val != CPUFREQ_CREATE_POLICY)
		return NOTIFY_OK;

	domain = find_domain(policy->cpu);
	if (!domain)
		return NOTIFY_OK;

	enable_domain(domain);

#if IS_ENABLED(CONFIG_EXYNOS_CPU_THERMAL) || IS_ENABLED(CONFIG_EXYNOS_CPU_THERMAL_MODULE)
	exynos_cpufreq_cooling_register(domain->dn, policy);
#endif

	ret = init_freq_qos(domain, policy);
	if (ret) {
		pr_info("failed to init pm_qos with err %d\n", ret);
		return ret;
	}

	ret = init_fops(domain, policy);
	if (ret) {
		pr_info("failed to init fops with err %d\n", ret);
		return ret;
	}

	return NOTIFY_OK;
}

static struct notifier_block acme_cpufreq_policy_nb = {
	.notifier_call = acme_cpufreq_policy_notify_callback,
	.priority = INT_MIN,
};

extern int cpu_dvfs_notifier_register(struct notifier_block *n);

#if defined(CONFIG_ARM_EXYNOS_CLDVFS_SYSFS)
static void init_cldvfs(struct kobject *kobj)
{
	cldvfs_base = ioremap(CLDVFS_BASE, SZ_4K);
	sysfs_create_group(kobj, &exynos_cldvfs_group);
}
#endif

static int exynos_cpufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn;
	struct exynos_cpufreq_domain *domain;
	unsigned int domain_id = 0;
	int ret = 0;

	/*
	 * Pre-initialization.
	 *
	 * allocate and initialize cpufreq domain
	 */
	for_each_child_of_node(pdev->dev.of_node, dn) {
		domain = kzalloc(sizeof(struct exynos_cpufreq_domain), GFP_KERNEL);
		if (!domain) {
			pr_err("failed to allocate domain%d\n", domain_id);
			return -ENOMEM;
		}

		domain->id = domain_id++;
		if (init_domain(domain, dn)) {
			pr_err("failed to initialize cpufreq domain%d\n",
							domain->id);
			kfree(domain->freq_table);
			kfree(domain);
			continue;
		}

		domain->dn = dn;
		list_add_tail(&domain->list, &domains);

		print_domain_info(domain);
	}

	if (!domain_id) {
		pr_err("Failed to initialize cpufreq driver\n");
		return -ENOMEM;
	}

	cpufreq_register_notifier(&acme_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);

	/* Register cpufreq driver */
	ret = cpufreq_register_driver(&exynos_driver);
	if (ret) {
		pr_err("failed to register cpufreq driver\n");
		return ret;
	}

	/*
	 * Post-initialization
	 *
	 * 1. create sysfs to control frequency min/max
	 * 2. enable frequency scaling of each domain
	 * 3. initialize freq qos of each domain
	 * 4. register notifier bloack
	 */
	init_sysfs(&pdev->dev.kobj);

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "exynos:acme",
		exynos_cpufreq_cpu_up_callback, exynos_cpufreq_cpu_down_callback);

	exynos_dm_fast_switch_notifier_register(&exynos_cpu_fast_switch_nb);

	emstune_register_notifier(&exynos_cpufreq_mode_update_notifier);
	register_pm_notifier(&exynos_cpufreq_pm);

	INIT_WORK(&dm_work, change_dm_table_work);

	pr_info("Initialized Exynos cpufreq driver\n");

#if defined(CONFIG_ARM_EXYNOS_CLDVFS_SYSFS)
	init_cldvfs(&pdev->dev.kobj);
#endif

	return ret;
}

static const struct of_device_id of_exynos_cpufreq_match[] = {
	{ .compatible = "samsung,exynos-acme", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_cpufreq_match);

static struct platform_driver exynos_cpufreq_driver = {
	.driver = {
		.name	= "exynos-acme",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_cpufreq_match,
	},
	.probe		= exynos_cpufreq_probe,
};

module_platform_driver(exynos_cpufreq_driver);

MODULE_DESCRIPTION("Exynos ACME");
MODULE_LICENSE("GPL");
