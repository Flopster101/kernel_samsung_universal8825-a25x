/*
 * Copyright (c) 2019 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * Author : Choonghoon Park (choong.park@samsung.com)
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
 *
 * Exynos DSUFreq driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sort.h>

#include <uapi/linux/sched/types.h>

#include <soc/samsung/cal-if.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-acme.h>
#include <soc/samsung/exynos-dm.h>

#define DSUFREQ_ENTRY_INVALID   (~0u)

struct exynos_dsufreq_dm {
	struct list_head		list;
	struct exynos_dm_constraint	c;
};

struct dsufreq_request_table {
	unsigned int			cpu_freq;
	unsigned int			dsu_freq;
};

struct dsufreq_request {
	struct cpumask			cpus;
	unsigned int			idx;
	unsigned int			cur_cpufreq;
	unsigned int			request_dsufreq;
	unsigned int			table_size;

	struct dsufreq_request_table	*cpu_dsu_freq_table;

	struct list_head		list;
};

struct dsufreq_stats {
	unsigned int			table_size;
	unsigned int			last_index;
	unsigned long long		last_time;
	unsigned long long		total_trans;

	unsigned long			*freq_table;
	unsigned long long		*time_in_state;
};

struct dsufreq_domain {
	unsigned int			min_freq;
	unsigned int			max_freq;
	unsigned int			cur_freq;
	unsigned int			min_freq_orig;
	unsigned int			max_freq_orig;
	unsigned int			target_freq;

	unsigned int			state;
	unsigned int			cal_id;
	unsigned int			dm_type;
	unsigned int			dss_type;

	raw_spinlock_t			update_lock;

	/* The next fields are for frequency change work */
	bool				work_in_progress;
	struct work_struct		work;

	struct dsufreq_stats		*stats;
	struct list_head		request_list;

	bool				initialized;

	/* list head of DVFS Manager constraints */
	struct list_head		dm_list;
};

static struct dsufreq_domain dsufreq;

/*********************************************************************
 *                           Helper Function                         *
 *********************************************************************/
static int get_dsufreq_index(struct dsufreq_stats *stats, unsigned int freq)
{
	int index;

	for (index = 0; index < stats->table_size; index++)
		if (stats->freq_table[index] == freq)
			return index;

	return -1;
}

static void update_dsufreq_time_in_state(struct dsufreq_stats *stats)
{
	unsigned long long cur_time = get_jiffies_64();

	stats->time_in_state[stats->last_index] += cur_time - stats->last_time;
	stats->last_time = cur_time;
}

static void update_dsufreq_stats(struct dsufreq_stats *stats, unsigned int freq)
{
	stats->last_index = get_dsufreq_index(stats, freq);
	stats->total_trans++;
}

static inline void dsufreq_verify_within_limits(unsigned int min, unsigned int max)
{
	if (dsufreq.min_freq < min)
		dsufreq.min_freq = min;
	if (dsufreq.max_freq < min)
		dsufreq.max_freq = min;
	if (dsufreq.min_freq > max)
		dsufreq.min_freq = max;
	if (dsufreq.max_freq > max)
		dsufreq.max_freq = max;
	if (dsufreq.min_freq > dsufreq.max_freq)
		dsufreq.min_freq = dsufreq.max_freq;
	return;
}

static inline void
dsufreq_verify_within_cpu_limits(void)
{
	dsufreq_verify_within_limits(dsufreq.min_freq_orig, dsufreq.max_freq_orig);
}

static unsigned int get_target_freq(void)
{
	unsigned int target_freq = 0;
	struct dsufreq_request *cursor_request;

	list_for_each_entry(cursor_request, &dsufreq.request_list, list) {
		if (target_freq < cursor_request->request_dsufreq)
			target_freq = cursor_request->request_dsufreq;
	}

	return target_freq;
}

/* Find lowest freq at or above target in a table in ascending order */
static int dsufreq_table_find_index_al(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = 0;
	int idx;

	for (idx = 0 ; idx < dsufreq.stats->table_size; idx++) {
		if (freq_table[idx] == DSUFREQ_ENTRY_INVALID)
			continue;

		if (freq_table[idx] >= target_freq)
			return freq_table[idx];

		best_freq = freq_table[idx];
	}

	return best_freq;
}

/* Find highest freq at or below target in a table in ascending order */
static int dsufreq_table_find_index_ah(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = 0;
	int idx;

	for (idx = 0; idx < dsufreq.stats->table_size; idx++) {
		if (freq_table[idx] == DSUFREQ_ENTRY_INVALID)
			continue;

		if (freq_table[idx] <= target_freq) {
			best_freq = freq_table[idx];
			continue;
		}

		return best_freq;
	}

	return best_freq;
}

static unsigned int resolve_dsufreq(unsigned int target_freq, unsigned int relation)
{
	unsigned int resolve_freq;

	switch (relation) {
	case EXYNOS_DM_RELATION_L:
		resolve_freq = dsufreq_table_find_index_al(target_freq);
		break;
	case EXYNOS_DM_RELATION_H:
		resolve_freq = dsufreq_table_find_index_ah(target_freq);
		break;
	default:
		pr_err("%s: Invalid relation: %d\n", __func__, relation);
		return -EINVAL;
	}

	return resolve_freq;
}

/*********************************************************************
 *                          Scaling Function                         *
 *********************************************************************/
static int scale_dsufreq(unsigned int target_freq, unsigned int relation)
{
	int ret;

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq, DSS_FLAG_IN);

	ret = cal_dfs_set_rate(dsufreq.cal_id, target_freq);

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq,
			  ret < 0 ? ret : DSS_FLAG_OUT);
	return ret;
}

static void schedule_dsu_work(int target_freq)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&dsufreq.update_lock, flags);

	if (dsufreq.work_in_progress) {
		raw_spin_unlock_irqrestore(&dsufreq.update_lock, flags);
		return;
	}

	if (target_freq < 0)
		dsufreq.target_freq = get_target_freq();
	else
		dsufreq.target_freq = target_freq;

	if (dsufreq.target_freq != dsufreq.cur_freq) {
		dsufreq.work_in_progress = true;
		schedule_work_on(smp_processor_id(), &dsufreq.work);
	}

	raw_spin_unlock_irqrestore(&dsufreq.update_lock, flags);
}

static int dsufreq_cpufreq_scaling_callback(struct notifier_block *nb,
				unsigned long event, void *val)
{
	int index;
	struct cpufreq_freqs *freqs = val;
	struct dsufreq_request *cursor_request, *request = NULL;
	struct dsufreq_request_table * table;

	if (unlikely(!dsufreq.initialized))
		return NOTIFY_DONE;

	switch(event) {
	case CPUFREQ_POSTCHANGE:
		/* Find a request of scaling cluster */
		list_for_each_entry(cursor_request, &dsufreq.request_list, list) {
			if (!cpumask_test_cpu(freqs->policy->cpu, &cursor_request->cpus))
				continue;

			request = cursor_request;
			break;
		}

		if (unlikely(!request))
			break;

		table = request->cpu_dsu_freq_table;
		for (index = 0; index < request->table_size; index++)
			if (table[index].cpu_freq >= freqs->new)
				break;

		if (index < 0)
			index = 0;

		request->cur_cpufreq = table[index].cpu_freq;
		request->request_dsufreq = table[index].dsu_freq;

		/* Check DSUFreq scaling is needed. */
		schedule_dsu_work(-1);

		break;
	default:
		;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dsufreq_cpufreq_scale_notifier = {
	.notifier_call = dsufreq_cpufreq_scaling_callback,
};

/*********************************************************************
 *                      SUPPORT for DVFS MANAGER                     *
 *********************************************************************/
static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	int ret;
	unsigned int resolve_freq;

	resolve_freq = resolve_dsufreq(target_freq, relation);
	if (!resolve_freq) {
		pr_err("%s: Couldn't find proper frequency target_freq %u, resolve_freq %u\n",
				__func__, target_freq, resolve_freq);
		return -EINVAL;
	}

	if (resolve_freq == dsufreq.cur_freq)
		return 0;

	ret = scale_dsufreq(resolve_freq, relation);
	if (ret) {
		pr_err("failed to scale frequency of DSU (%d -> %d)\n",
				dsufreq.cur_freq, resolve_freq);
		return ret;
	}

	update_dsufreq_time_in_state(dsufreq.stats);
	update_dsufreq_stats(dsufreq.stats, resolve_freq);
	dsufreq.cur_freq = resolve_freq;

	return ret;
}

/*********************************************************************
 *                          Sysfs function                           *
 *********************************************************************/
static ssize_t dsufreq_show_min_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.min_freq);
}

static ssize_t dsufreq_store_min_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%8d", &input) != 1)
		return -EINVAL;

	dsufreq.min_freq = input;
	dsufreq_verify_within_cpu_limits();

	policy_update_call_to_DM(dsufreq.dm_type, dsufreq.min_freq, dsufreq.max_freq);

	if (dsufreq.cur_freq < input)
		schedule_dsu_work(input);

	return count;
}

static ssize_t dsufreq_show_max_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.max_freq);
}

static ssize_t dsufreq_store_max_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%8d", &input) != 1)
		return -EINVAL;

	dsufreq.max_freq = input;
	dsufreq_verify_within_cpu_limits();

	policy_update_call_to_DM(dsufreq.dm_type, dsufreq.min_freq, dsufreq.max_freq);

	if (dsufreq.cur_freq > input)
		schedule_dsu_work(input);

	return count;
}

static ssize_t dsufreq_show_cur_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.cur_freq);
}

static ssize_t dsufreq_show_time_in_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	struct dsufreq_stats *stats = dsufreq.stats;

	update_dsufreq_time_in_state(stats);

	for (i = 0; i < stats->table_size; i++) {
		if (stats->freq_table[i] == DSUFREQ_ENTRY_INVALID)
			continue;

		count += snprintf(&buf[count], PAGE_SIZE - count, "%u %llu\n",
					stats->freq_table[i],
					(unsigned long long)jiffies_64_to_clock_t(stats->time_in_state[i]));
	}

	return count;
}

static ssize_t dsufreq_show_total_trans(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.stats->total_trans);
}

static DEVICE_ATTR(scaling_min_freq, S_IRUGO | S_IWUSR,
		dsufreq_show_min_freq, dsufreq_store_min_freq);
static DEVICE_ATTR(scaling_max_freq, S_IRUGO | S_IWUSR,
		dsufreq_show_max_freq, dsufreq_store_max_freq);
static DEVICE_ATTR(scaling_cur_freq, S_IRUGO,
		dsufreq_show_cur_freq, NULL);
static DEVICE_ATTR(time_in_state, S_IRUGO,
		dsufreq_show_time_in_state, NULL);
static DEVICE_ATTR(total_trans, S_IRUGO,
		dsufreq_show_total_trans, NULL);

static struct attribute *dsufreq_attrs[] = {
	&dev_attr_scaling_min_freq.attr,
	&dev_attr_scaling_max_freq.attr,
	&dev_attr_scaling_cur_freq.attr,
	&dev_attr_time_in_state.attr,
	&dev_attr_total_trans.attr,
	NULL,
};

static struct attribute_group dsufreq_group = {
	.name = "dsufreq",
	.attrs = dsufreq_attrs,
};

/*********************************************************************
 *                            Init Function                          *
 *********************************************************************/
static void dsufreq_work_fn(struct work_struct *work)
{
	unsigned long freq, flags;

	if (unlikely(!dsufreq.initialized))
		return;

	raw_spin_lock_irqsave(&dsufreq.update_lock, flags);
	freq = (unsigned long)dsufreq.target_freq;
	dsufreq.work_in_progress = false;
	raw_spin_unlock_irqrestore(&dsufreq.update_lock, flags);

	DM_CALL(dsufreq.dm_type, &freq);
}

static int dsufreq_init_domain(struct device_node *dn)
{
	int ret;
	unsigned int val;

	ret = of_property_read_u32(dn, "cal-id", &dsufreq.cal_id);
	if (ret)
		return ret;

	/* Get min/max/cur DSUFreq */
	dsufreq.max_freq = dsufreq.max_freq_orig = cal_dfs_get_max_freq(dsufreq.cal_id);
	dsufreq.min_freq = dsufreq.min_freq_orig = cal_dfs_get_min_freq(dsufreq.cal_id);

	if (!of_property_read_u32(dn, "max-freq", &val))
		dsufreq.max_freq = dsufreq.max_freq_orig = min(dsufreq.max_freq, val);
	if (!of_property_read_u32(dn, "min-freq", &val))
		dsufreq.min_freq = dsufreq.min_freq_orig = max(dsufreq.min_freq, val);

	dsufreq.cur_freq = cal_dfs_get_rate(dsufreq.cal_id);
	WARN_ON(dsufreq.cur_freq < dsufreq.min_freq || dsufreq.max_freq < dsufreq.cur_freq);
	dsufreq.cur_freq = clamp_val(dsufreq.cur_freq, dsufreq.min_freq, dsufreq.max_freq);

	ret = of_property_read_u32(dn, "dss-type", &val);
	if (ret)
		return ret;
	dsufreq.dss_type = val;

	INIT_LIST_HEAD(&dsufreq.request_list);
	raw_spin_lock_init(&dsufreq.update_lock);
	INIT_WORK(&dsufreq.work, dsufreq_work_fn);

	return ret;
}

static int __dsufreq_init_request(struct device_node *dn,
				struct dsufreq_request *req)
{
	int size;
	const char *buf;
	struct dsufreq_request_table *table;
	struct device_node *constraint_dn;

	/* Get cpumask which belongs to this domain */
	if (of_property_read_string(dn, "sibling-cpus", &buf))
		return -EINVAL;
	cpulist_parse(buf, &req->cpus);
	cpumask_and(&req->cpus, &req->cpus, cpu_online_mask);
	if (cpumask_weight(&req->cpus) == 0)
		return -ENODEV;

	constraint_dn = of_parse_phandle(dn, "constraint", 0);
	if (!constraint_dn)
		return -ENODEV;

	/* Get CPU-DSU freq table */
	size = of_property_count_u32_elems(constraint_dn, "table");
	if (size < 0)
		return size;

	table = kzalloc(sizeof(struct dsufreq_request_table) * size / 2, GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	req->table_size = size / 2;

	if (of_property_read_u32_array(constraint_dn, "table", (unsigned int *)table, size))
	        return -EINVAL;

	req->cpu_dsu_freq_table = table;

	return 0;
}

static int dsufreq_init_request(struct device_node *dsufreq_dn)
{
	int idx = 0;
	struct device_node *dn;
	struct dsufreq_request *req;

	for_each_child_of_node(dsufreq_dn, dn) {
		req = kzalloc(sizeof(struct dsufreq_request), GFP_KERNEL);
		if (!req) {
			pr_err("Failed to allocate dsufreq request\n");
			continue;
		}

		req->idx = idx++;
		if (__dsufreq_init_request(dn, req))
			continue;

		list_add(&req->list, &dsufreq.request_list);
	};

	if (list_empty(&dsufreq.request_list))
		return -ENODEV;

	return 0;
}

static int dsufreq_init_freq_table_ect(struct dsufreq_stats *stats)
{
	int index;
	unsigned long *freq_table;

	stats->table_size = cal_dfs_get_lv_num(dsufreq.cal_id);

	/* Get DSUFreq table */
	freq_table = kzalloc(sizeof(unsigned long) * stats->table_size, GFP_KERNEL);
	if (!freq_table)
		return -ENOMEM;
	cal_dfs_get_rate_table(dsufreq.cal_id, freq_table);

	/* Check out invalid frequencies */
	for (index = 0; index < stats->table_size; index++)
		if (freq_table[index] > dsufreq.max_freq
		    || freq_table[index] < dsufreq.min_freq)
			freq_table[index] = DSUFREQ_ENTRY_INVALID;

	stats->freq_table = freq_table;

	return 0;
}

static int dsufreq_init_freq_table_dt(struct device_node *dn, struct dsufreq_stats *stats)
{
	unsigned int freq_table[100];
	int i, size;

	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	stats->freq_table = kcalloc(size, sizeof(unsigned long), GFP_KERNEL);
	if (!stats->freq_table)
		return -ENOMEM;

	if (of_property_read_u32_array(dn, "table", freq_table, size))
		return -EINVAL;

	stats->table_size = size;

	for (i = 0; i < stats->table_size; i++)
		stats->freq_table[i] = freq_table[i];

	return 0;
}

static int dsufreq_init_stats(struct device_node *dn)
{
	struct dsufreq_stats *stats;
	unsigned long long *time_in_state;
	int init_with_dt;
	int ret = 0;

	stats = kzalloc(sizeof(struct dsufreq_stats), GFP_KERNEL);
	if (!stats)
		return -ENOMEM;

	if (of_property_read_u32(dn, "init-with-dt", &init_with_dt))
		init_with_dt = 0;

	if (init_with_dt)
		ret = dsufreq_init_freq_table_dt(dn, stats);
	else
		ret = dsufreq_init_freq_table_ect(stats);

	if (ret)
		return ret;

	/* Initialize DSUFreq time_in_state */
	time_in_state = kzalloc(sizeof(unsigned long long) * stats->table_size, GFP_KERNEL);
	if (!time_in_state)
		return -ENOMEM;
	stats->time_in_state = time_in_state;

	dsufreq.stats = stats;

	/*
	 * dsufreq table is in ascending order
	 * cur_freq should be at or higher than the requested booting freq(cur_freq)
	 * 0 : Find lowest freq at or above target in a table in ascending order
	 * 1 : Find highest freq at or below target in a table in ascending order
	 */
	dsufreq.cur_freq = resolve_dsufreq(dsufreq.cur_freq, 0);

	stats->last_time = get_jiffies_64();
	stats->last_index = get_dsufreq_index(stats, dsufreq.cur_freq);

	return 0;
}

static int init_constraint_table_ect(struct exynos_dsufreq_dm *dm,
					struct ect_minlock_domain *ect_domain)
{
	unsigned int index;

	for (index = 0; index < ect_domain->num_of_level; index++) {
		dm->c.freq_table[index].master_freq
			= ect_domain->level[index].main_frequencies;
		dm->c.freq_table[index].slave_freq
			= ect_domain->level[index].sub_frequencies;
	}

	return 0;
}

static struct ect_minlock_domain *get_ect_domain(struct device_node *dn)
{
	struct ect_minlock_domain *ect_domain = NULL;
	const char *ect_name;
	void *block;
	int ret;

	ret = of_property_read_string(dn, "ect-name", &ect_name);
	if (ret)
		return NULL;

	block = ect_get_block(BLOCK_MINLOCK);
	if (!block)
		return NULL;

	ect_domain = ect_minlock_get_domain(block, (char *)ect_name);
	if (!ect_domain)
		return NULL;

	return ect_domain;
}

static int dsufreq_init_dm_constraint_table(struct device_node *root)
{
	struct exynos_dsufreq_dm *dm;
	struct of_phandle_iterator iter;
	struct ect_minlock_domain *ect_domain;
	int ret = 0, err;

	/* Initialize list head of DVFS Manager constraints */
	INIT_LIST_HEAD(&dsufreq.dm_list);

	/*
	 * Initialize DVFS Manager constraints
	 * - constraint_type : minimum or maximum constraint
	 * - constraint_dm_type : cpu/mif/int/.. etc
	 * - guidance : constraint from chipset characteristic
	 * - freq_table : constraint table
	 */
	of_for_each_phandle(&iter, err, root, "list", NULL, 0) {
		/* allocate DM constraint */
		dm = kzalloc(sizeof(struct exynos_dsufreq_dm), GFP_KERNEL);
		if (!dm)
			goto init_fail;

		list_add_tail(&dm->list, &dsufreq.dm_list);

		of_property_read_u32(iter.node, "const-type", &dm->c.constraint_type);
		of_property_read_u32(iter.node, "dm-slave", &dm->c.dm_slave);

		if (!of_property_read_bool(iter.node, "guidance"))
			continue;

		dm->c.guidance = true;
		ect_domain = get_ect_domain(iter.node);
		dm->c.table_length = ect_domain->num_of_level;

		dm->c.freq_table = kcalloc(1, sizeof(struct exynos_dm_freq)
					* dm->c.table_length, GFP_KERNEL);
		if (!dm->c.freq_table)
			goto init_fail;

		if (init_constraint_table_ect(dm, ect_domain))
			goto init_fail;

		/*
		 *  dynamic disable for migov control
		 *  skew tables should not be disable
		 */
		if (of_property_read_bool(iter.node, "dynamic-disable"))
			dm->c.support_dynamic_disable = true;

		/* register DM constraint */
		ret = register_exynos_dm_constraint_table(dsufreq.dm_type, &dm->c);
		if (ret)
			goto init_fail;
	}

	return ret;

init_fail:
	while (!list_empty(&dsufreq.dm_list)) {
		dm = list_last_entry(&dsufreq.dm_list,
				struct exynos_dsufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	return ret;
}

static int dsufreq_init_dm(struct device_node *dn)
{
	int ret;

	ret = of_property_read_u32(dn, "dm-type", &dsufreq.dm_type);
	if (ret)
		return ret;

	ret = exynos_dm_data_init(dsufreq.dm_type, &dsufreq,
				dsufreq.min_freq, dsufreq.max_freq, dsufreq.min_freq);
	if (ret)
		return ret;

	dn = of_get_child_by_name(dn, "hw-constraints");
	if (dn) {
		ret = dsufreq_init_dm_constraint_table(dn);
		if (ret)
			return ret;
	}

	return register_exynos_dm_freq_scaler(dsufreq.dm_type, dm_scaler);
}

static int exynos_dsufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	int ret = 0;

	/* Explicitly assign false */
	dsufreq.initialized = false;

	ret = dsufreq_init_domain(dn);
	if (ret)
		return ret;

	ret = dsufreq_init_request(dn);
	if (ret)
		return ret;

	ret = dsufreq_init_stats(dn);
	if (ret)
		return ret;

	ret = dsufreq_init_dm(dn);
	if (ret) {
		/*
		 * In case of failure to init dm related data,
		 * need to get notification from CPUFreq to do DSU DVFS.
		 */
		ret = exynos_cpufreq_register_notifier(&dsufreq_cpufreq_scale_notifier,
						       CPUFREQ_TRANSITION_NOTIFIER);
		if (ret)
			return ret;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &dsufreq_group);
	if (ret)
		return ret;

	dsufreq.initialized = true;

	return ret;
}

static const struct of_device_id of_exynos_dsufreq_match[] = {
	{ .compatible = "samsung,exynos-dsufreq", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_dsufreq_match);

static struct platform_driver exynos_dsufreq_driver = {
	.driver = {
		.name	= "exynos-dsufreq",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_dsufreq_match,
	},
	.probe		= exynos_dsufreq_probe,
};

module_platform_driver(exynos_dsufreq_driver);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos DSUFreq drvier");
MODULE_LICENSE("GPL");
