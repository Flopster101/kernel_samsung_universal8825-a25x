/* linux/drivers/soc/samsung/exynos-dm.c
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung Exynos SoC series DVFS Manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <soc/samsung/debug-snapshot.h>
#include <linux/sched/clock.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <soc/samsung/acpm_ipc_ctrl.h>
#include <soc/samsung/exynos-cpupm.h>

#include <soc/samsung/exynos-dm.h>

#define DM_EMPTY	0xFF
static struct exynos_dm_device *exynos_dm;
static ATOMIC_NOTIFIER_HEAD(exynos_dm_fast_switch_notifier);

static int dm_idle_ip_index;
static int dm_fast_switch_idle_ip_index;
static spinlock_t fast_switch_glb_lock;

void exynos_dm_dynamic_disable(int flag);

/*
 * SYSFS for Debugging
 */
static ssize_t show_available(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		count += snprintf(buf + count, PAGE_SIZE,
				"dm_type: %d(%s), available = %s\n",
				dm->dm_data[i].dm_type, dm->dm_data[i].dm_type_name,
				dm->dm_data[i].available ? "true" : "false");
	}

	return count;
}

static ssize_t show_constraint_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct list_head *constraint_list;
	struct exynos_dm_constraint *constraint;
	struct exynos_dm_data *dm;
	struct exynos_dm_attrs *dm_attrs;
	ssize_t count = 0;
	int i;

	dm_attrs = container_of(attr, struct exynos_dm_attrs, attr);
	dm = container_of(dm_attrs, struct exynos_dm_data, constraint_table_attr);

	if (!dm->available) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type is not available\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "dm_type: %s\n",
				dm->dm_type_name);

	constraint_list = &dm->min_slaves;
	if (list_empty(constraint_list)) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type have not min constraint tables\n\n");
		goto next;
	}

	list_for_each_entry(constraint, constraint_list, master_domain) {
		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
		count += snprintf(buf + count, PAGE_SIZE,
				"constraint_dm_type = %s\n", constraint->dm_type_name);
		count += snprintf(buf + count, PAGE_SIZE, "constraint_type: %s\n",
				constraint->constraint_type ? "MAX" : "MIN");
		count += snprintf(buf + count, PAGE_SIZE, "guidance: %s\n",
				constraint->guidance ? "true" : "false");
		count += snprintf(buf + count, PAGE_SIZE,
				"const_freq = %u, gov_freq =%u\n",
				constraint->const_freq, constraint->gov_freq);		\
		count += snprintf(buf + count, PAGE_SIZE,
				"master_freq\t constraint_freq\n");
		for (i = 0; i < constraint->table_length; i++)
			count += snprintf(buf + count, PAGE_SIZE, "%10u\t %10u\n",
					constraint->freq_table[i].master_freq,
					constraint->freq_table[i].slave_freq);
		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
	}

next:
	constraint_list = &dm->max_slaves;
	if (list_empty(constraint_list)) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type have not max constraint tables\n\n");
		return count;
	}

	list_for_each_entry(constraint, constraint_list, master_domain) {
		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
		count += snprintf(buf + count, PAGE_SIZE,
				"constraint_dm_type = %s\n", constraint->dm_type_name);
		count += snprintf(buf + count, PAGE_SIZE, "constraint_type: %s\n",
				constraint->constraint_type ? "MAX" : "MIN");
		count += snprintf(buf + count, PAGE_SIZE, "guidance: %s\n",
				constraint->guidance ? "true" : "false");
		count += snprintf(buf + count, PAGE_SIZE,
				"max_freq =%u\n",
				constraint->const_freq);
		count += snprintf(buf + count, PAGE_SIZE,
				"master_freq\t constraint_freq\n");
		for (i = 0; i < constraint->table_length; i++)
			count += snprintf(buf + count, PAGE_SIZE, "%10u\t %10u\n",
					constraint->freq_table[i].master_freq,
					constraint->freq_table[i].slave_freq);
		count += snprintf(buf + count, PAGE_SIZE,
				"-------------------------------------------------\n");
	}

	return count;
}

static ssize_t show_dm_policy(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct list_head *constraint_list;
	struct exynos_dm_constraint *constraint;
	struct exynos_dm_data *dm;
	struct exynos_dm_attrs *dm_attrs;
	ssize_t count = 0;
	u32 find;
	int i;

	dm_attrs = container_of(attr, struct exynos_dm_attrs, attr);
	dm = container_of(dm_attrs, struct exynos_dm_data, dm_policy_attr);

	if (!dm->available) {
		count += snprintf(buf + count, PAGE_SIZE,
				"This dm_type is not available\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE, "dm_type: %s\n",
				dm->dm_type_name);

	count += snprintf(buf + count, PAGE_SIZE,
			"policy_min = %u, policy_max = %u\n",
			dm->policy_min, dm->policy_max);
	count += snprintf(buf + count, PAGE_SIZE,
			"const_min = %u, const_max = %u\n",
			dm->const_min, dm->const_max);
	count += snprintf(buf + count, PAGE_SIZE,
			"gov_min = %u, governor_freq = %u\n", dm->gov_min, dm->governor_freq);
	count += snprintf(buf + count, PAGE_SIZE, "current_freq = %u\n", dm->cur_freq);
	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");
	count += snprintf(buf + count, PAGE_SIZE, "min constraint by\n");
	find = 0;

	for (i = 0; i < exynos_dm->domain_count; i++) {
		if (!exynos_dm->dm_data[i].available)
			continue;

		constraint_list = &exynos_dm->dm_data[i].min_slaves;
		if (list_empty(constraint_list))
			continue;
		list_for_each_entry(constraint, constraint_list, master_domain) {
			if (constraint->dm_slave == dm->dm_type) {
				count += snprintf(buf + count, PAGE_SIZE,
					"%s ---> %s\n"
					"policy_min(%u), const_min(%u) ---> const_freq(%u)\n"
					"gov_min(%u), gov_freq(%u) ---> gov_freq(%u)\n",
					exynos_dm->dm_data[i].dm_type_name,
					dm->dm_type_name,
					exynos_dm->dm_data[i].policy_min,
					exynos_dm->dm_data[i].const_min,
					constraint->const_freq,
					exynos_dm->dm_data[i].gov_min,
					exynos_dm->dm_data[i].governor_freq,
					constraint->gov_freq);
				if (constraint->guidance)
					count += snprintf(buf+count, PAGE_SIZE,
						" [guidance]\n");
				else
					count += snprintf(buf+count, PAGE_SIZE, "\n");
				find = max(find, constraint->const_freq);
			}
		}
	}
	if (find == 0)
		count += snprintf(buf + count, PAGE_SIZE,
				"There is no min constraint\n\n");
	else
		count += snprintf(buf + count, PAGE_SIZE,
				"min constraint freq = %u\n", find);
	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");
	count += snprintf(buf + count, PAGE_SIZE, "max constraint by\n");
	find = INT_MAX;

	for (i = 0; i < exynos_dm->domain_count; i++) {
		if (!exynos_dm->dm_data[i].available)
			continue;

		constraint_list = &exynos_dm->dm_data[i].max_slaves;
		if (list_empty(constraint_list))
			continue;
		list_for_each_entry(constraint, constraint_list, master_domain) {
			if (constraint->dm_slave == dm->dm_type) {
				count += snprintf(buf + count, PAGE_SIZE,
					"%s ---> %s\n"
					"policy_min(%u), const_min(%u) ---> const_freq(%u)\n",
					exynos_dm->dm_data[i].dm_type_name,
					dm->dm_type_name,
					exynos_dm->dm_data[i].policy_max,
					exynos_dm->dm_data[i].const_max,
					constraint->const_freq);
				if (constraint->guidance)
					count += snprintf(buf+count, PAGE_SIZE,
						" [guidance]\n");
				else
					count += snprintf(buf+count, PAGE_SIZE, "\n");
				find = min(find, constraint->const_freq);
			}
		}
	}
	if (find == INT_MAX)
		count += snprintf(buf + count, PAGE_SIZE,
				"There is no max constraint\n\n");
	else
		count += snprintf(buf + count, PAGE_SIZE,
				"max constraint freq = %u\n", find);
	count += snprintf(buf + count, PAGE_SIZE,
			"-------------------------------------------------\n");
	return count;
}

static ssize_t show_dynamic_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE,
			"%s\n", dm->dynamic_disable ? "true" : "false");

	return count;
}

static ssize_t store_dynamic_disable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int flag, ret = 0;

	ret = sscanf(buf, "%u", &flag);
	if (ret != 1)
		return -EINVAL;

	exynos_dm_dynamic_disable(flag);

	return count;
}

static ssize_t show_fast_switch(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);
	struct exynos_dm_data *data;
	ssize_t count = 0;
	int i;

	count += snprintf(buf + count, PAGE_SIZE,
			"DomainName(ID):     Mode\n");
	for (i = 0; i < dm->domain_count; i++) {
		data = &dm->dm_data[i];

		if (!data->available)
			continue;
		count += snprintf(buf + count, PAGE_SIZE,
				"%10s(%2d): %8s\n", data->dm_type_name, data->dm_type, data->fast_switch ? "enabled" : "disabled");
	}

	count += snprintf(buf + count, PAGE_SIZE,
			"Usage: echo [id] [mode] > fast_switch (mode = 0/1)\n");

	return count;
}

static ssize_t store_fast_switch(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int domain, flag, ret = 0;
	struct exynos_dm_data *data;

	ret = sscanf(buf, "%u %u", &domain, &flag);
	if (ret != 2)
		return -EINVAL;

	if (domain < 0 || domain >= exynos_dm->domain_count)
		return count;

	data = &exynos_dm->dm_data[domain];

	mutex_lock(&data->fast_switch_lock);
	data->fast_switch = !!flag;
	mutex_unlock(&data->fast_switch_lock);

	return count;
}

static DEVICE_ATTR(available, 0440, show_available, NULL);
static DEVICE_ATTR(dynamic_disable, 0640, show_dynamic_disable, store_dynamic_disable);
static DEVICE_ATTR(fast_switch, 0640, show_fast_switch, store_fast_switch);

static struct attribute *exynos_dm_sysfs_entries[] = {
	&dev_attr_available.attr,
	&dev_attr_dynamic_disable.attr,
	&dev_attr_fast_switch.attr,
	NULL,
};

static struct attribute_group exynos_dm_attr_group = {
	.name	= "exynos_dm",
	.attrs	= exynos_dm_sysfs_entries,
};
/*
 * SYSFS for Debugging end
 */


void exynos_dm_dynamic_disable(int flag)
{
	mutex_lock(&exynos_dm->lock);

	exynos_dm->dynamic_disable = !!flag;

	mutex_unlock(&exynos_dm->lock);
}
EXPORT_SYMBOL(exynos_dm_dynamic_disable);

static void print_available_dm_data(struct exynos_dm_device *dm)
{
	int i;

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		dev_info(dm->dev, "dm_type: %d(%s), available = %s\n",
				dm->dm_data[i].dm_type, dm->dm_data[i].dm_type_name,
				dm->dm_data[i].available ? "true" : "false");
	}
}

static int exynos_dm_index_validate(int index)
{
	if (index < 0) {
		dev_err(exynos_dm->dev, "invalid dm_index (%d)\n", index);
		return -EINVAL;
	}

	return 0;
}

int exynos_dm_fast_switch_notifier_register(struct notifier_block *n)
{
	return atomic_notifier_chain_register(&exynos_dm_fast_switch_notifier, n);
}
EXPORT_SYMBOL_GPL(exynos_dm_fast_switch_notifier_register);

static void exynos_dm_fast_switch_callback(unsigned int *cmd, unsigned int size)
{
	struct exynos_dm_fast_switch_notify_data data;
	u32 start_time, end_time;
	unsigned long flags;

	data.domain = cmd[1];
	data.freq = cmd[2];

	start_time = cmd[3] & 0xFFFF;
	end_time = (cmd[3] >> 16) & 0xFFFF;

	data.time = acpm_time_calc(start_time, end_time);

	atomic_notifier_call_chain(&exynos_dm_fast_switch_notifier, data.domain, &data);

	spin_lock_irqsave(&fast_switch_glb_lock, flags);
	if (!is_acpm_ipc_busy(exynos_dm->fast_switch_ch)) {
		// enable sicd
		exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);
	}
	spin_unlock_irqrestore(&fast_switch_glb_lock, flags);
}

#ifdef CONFIG_OF
static int exynos_dm_parse_dt(struct device_node *np, struct exynos_dm_device *dm)
{
	struct device_node *child_np, *domain_np = NULL;
	const char *name;
	int ret = 0;

	if (!np)
		return -ENODEV;

	domain_np = of_get_child_by_name(np, "dm_domains");
	if (!domain_np)
		return -ENODEV;

	dm->domain_count = of_get_child_count(domain_np);
	if (!dm->domain_count)
		return -ENODEV;

	dm->dm_data = kzalloc(sizeof(struct exynos_dm_data) * dm->domain_count, GFP_KERNEL);
	if (!dm->dm_data) {
		dev_err(dm->dev, "failed to allocate dm_data\n");
		return -ENOMEM;
	}
	dm->domain_order = kzalloc(sizeof(int) * dm->domain_count, GFP_KERNEL);
	if (!dm->domain_order) {
		dev_err(dm->dev, "failed to allocate domain_order\n");
		return -ENOMEM;
	}

	for_each_child_of_node(domain_np, child_np) {
		int index;
		const char *available;
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
		const char *policy_use;
#endif
		if (of_property_read_u32(child_np, "dm-index", &index))
			return -ENODEV;

		ret = exynos_dm_index_validate(index);
		if (ret)
			return ret;

		if (of_property_read_string(child_np, "available", &available))
			return -ENODEV;

		if (!strcmp(available, "true")) {
			dm->dm_data[index].dm_type = index;
			dm->dm_data[index].available = true;

			if (!of_property_read_string(child_np, "dm_type_name", &name))
				strncpy(dm->dm_data[index].dm_type_name, name, EXYNOS_DM_TYPE_NAME_LEN);

			dm->dm_data[index].fast_switch = of_property_read_bool(child_np, "fast_switch");

			INIT_LIST_HEAD(&dm->dm_data[index].min_slaves);
			INIT_LIST_HEAD(&dm->dm_data[index].max_slaves);
			INIT_LIST_HEAD(&dm->dm_data[index].min_masters);
			INIT_LIST_HEAD(&dm->dm_data[index].max_masters);
		} else {
			dm->dm_data[index].available = false;
		}
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
		if (of_property_read_string(child_np, "policy_use", &policy_use)) {
			dev_info(dm->dev, "This doesn't need to send policy to ACPM\n");
		} else {
			if (!strcmp(policy_use, "true"))
				dm->dm_data[index].policy_use = true;
		}

		if (of_property_read_u32(child_np, "cal_id", &dm->dm_data[index].cal_id))
			return -ENODEV;
#endif
	}

	// For Fast Switch Notifier
	child_np = of_get_child_by_name(np, "dm-fast-switch");
	if (child_np) {
		int size;

		ret = acpm_ipc_request_channel(child_np,
				exynos_dm_fast_switch_callback,
				&dm->fast_switch_ch,
				&size);
		exynos_acpm_set_fast_switch(dm->fast_switch_ch);
	} else {
		pr_err("%s : Failed to get cpu-noti\n", __func__);
	}

	return ret;
}
#else
static int exynos_dm_parse_dt(struct device_node *np, struct exynos_dm_device *dm)
{
	return -ENODEV;
}
#endif

/*
 * This function should be called from each DVFS drivers
 * before DVFS driver registration to DVFS framework.
 * 	Initialize sequence Step.1
 */
int exynos_dm_data_init(int dm_type, void *data,
			u32 min_freq, u32 max_freq, u32 cur_freq)
{
	struct exynos_dm_data *dm;
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	mutex_lock(&exynos_dm->lock);

	dm = &exynos_dm->dm_data[dm_type];

	if (!dm->available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	dm->const_max = dm->policy_max = max_freq;
	dm->gov_min = dm->policy_min = dm->const_min = min_freq;
	dm->governor_freq = dm->cur_freq = cur_freq;

	dm->devdata = data;

out:
	mutex_unlock(&exynos_dm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_dm_data_init);

/*
 * 	Initialize sequence Step.2
 */
static void exynos_dm_topological_sort(void)
{
	int *indegree;
	int *searchQ;
	int *resultQ = exynos_dm->domain_order;
	int s_head = 0, r_head = 0, s_rear = 0;
	int poll, i;
	struct exynos_dm_constraint *t = NULL;

	indegree = kmalloc(sizeof(int) * exynos_dm->domain_count, GFP_KERNEL);
	searchQ = kmalloc(sizeof(int) * exynos_dm->domain_count, GFP_KERNEL);

	for (i = 0; i < exynos_dm->domain_count; i++) {
		// calculate Indegree of each domain
		struct exynos_dm_data *dm = &exynos_dm->dm_data[i];

		if (dm->available == false)
			continue;

		if (list_empty(&dm->min_slaves) && list_empty(&dm->max_slaves)
				&& list_empty(&dm->min_masters) && list_empty(&dm->max_masters))
			continue;

		indegree[i] = dm->indegree;

		// In indegree is 0, input to searchQ.
		// It means that the domain is not slave of any other domains.
		if (indegree[i] == 0)
			searchQ[s_head++] = i;
	}

	while (s_head != s_rear) {
		// Pick the first item of searchQ
		t = NULL;
		poll = searchQ[s_rear++];
		// Push the item which has 0 indegree into resultQ
		resultQ[r_head++] = poll;
		exynos_dm->dm_data[poll].my_order = r_head - 1;
		// Decrease the indegree which indecated by poll item.
		list_for_each_entry(t, &exynos_dm->dm_data[poll].min_slaves, master_domain) {
			if (--indegree[t->dm_slave] == 0)
				searchQ[s_head++] = t->dm_slave;
		}
	}

	// Size of result queue means the number of domains which has constraint
	exynos_dm->constraint_domain_count = r_head;
}

int exynos_dm_change_freq_table(struct exynos_dm_constraint *constraint, int idx)
{
	int ret = -EINVAL;

	mutex_lock(&exynos_dm->lock);

	if (!constraint->support_variable_freq_table || idx > 2)
		goto out;

	constraint->freq_table = constraint->variable_freq_table[idx];

	ret = 0;

out:
	mutex_unlock(&exynos_dm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_dm_change_freq_table);

int register_exynos_dm_constraint_table(int dm_type,
				struct exynos_dm_constraint *constraint)
{
	struct exynos_dm_constraint *sub_constraint;
	struct exynos_dm_data *master = &exynos_dm->dm_data[dm_type];
	struct exynos_dm_data *slave = &exynos_dm->dm_data[constraint->dm_slave];
	int i, ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	if (!constraint) {
		dev_err(exynos_dm->dev, "constraint is not valid\n");
		return -EINVAL;
	}

	/* check member invalid */
	if ((constraint->constraint_type < CONSTRAINT_MIN) ||
		(constraint->constraint_type > CONSTRAINT_MAX)) {
		dev_err(exynos_dm->dev, "constraint_type is invalid\n");
		return -EINVAL;
	}

	ret = exynos_dm_index_validate(constraint->dm_slave);
	if (ret)
		return ret;

	if (!constraint->freq_table) {
		if (constraint->support_variable_freq_table)
			constraint->freq_table = constraint->variable_freq_table[0];
		else {
			dev_err(exynos_dm->dev, "No frequency table for constraint\n");
			return -EINVAL;
		}
	}

	mutex_lock(&exynos_dm->lock);

	strncpy(constraint->dm_type_name,
			exynos_dm->dm_data[constraint->dm_slave].dm_type_name,
			EXYNOS_DM_TYPE_NAME_LEN);
	constraint->const_freq = 0;
	constraint->gov_freq = 0;
	constraint->dm_master = dm_type;

	if (constraint->constraint_type == CONSTRAINT_MIN) {
		/*
		 * In domain, min/max slaves are list of slave constraint conditions
		 * In constraint, master_domain means that which work as master.
		 * All min/max slaves in domains are linked with master_domain in constraint,
		 * and min/max masters in domains are linked with slave_domain in constraint.
		 */
		list_add(&constraint->master_domain, &master->min_slaves);
		list_add(&constraint->slave_domain, &slave->min_masters);
		slave->indegree++;
	} else if (constraint->constraint_type == CONSTRAINT_MAX) {
		list_add(&constraint->master_domain, &master->max_slaves);
		list_add(&constraint->slave_domain, &slave->max_masters);
	}

	/* check guidance and sub constraint table generations */
	if (constraint->guidance && (constraint->constraint_type == CONSTRAINT_MIN)) {
		sub_constraint = kzalloc(sizeof(struct exynos_dm_constraint), GFP_KERNEL);
		if (sub_constraint == NULL) {
			dev_err(exynos_dm->dev, "failed to allocate sub constraint\n");
			ret = -ENOMEM;
			goto err_sub_const;
		}

		// Initialize member variables
		sub_constraint->dm_master = constraint->dm_slave;
		sub_constraint->dm_slave = constraint->dm_master;
		sub_constraint->table_length = constraint->table_length;
		sub_constraint->constraint_type = CONSTRAINT_MAX;
		sub_constraint->guidance = true;
		sub_constraint->const_freq = UINT_MAX;
		sub_constraint->gov_freq = UINT_MAX;

		strncpy(sub_constraint->dm_type_name,
				exynos_dm->dm_data[sub_constraint->dm_slave].dm_type_name,
				EXYNOS_DM_TYPE_NAME_LEN);

		sub_constraint->freq_table =
			kzalloc(sizeof(struct exynos_dm_freq) * sub_constraint->table_length, GFP_KERNEL);
		if (sub_constraint->freq_table == NULL) {
			dev_err(exynos_dm->dev, "failed to allocate freq table for sub const\n");
			ret = -ENOMEM;
			goto err_freq_table;
		}

		/* generation table */
		for (i = 0; i < constraint->table_length; i++) {
			sub_constraint->freq_table[i].master_freq =
					constraint->freq_table[i].slave_freq;
			sub_constraint->freq_table[i].slave_freq =
					constraint->freq_table[i].master_freq;
		}

		list_add(&sub_constraint->master_domain, &slave->max_slaves);
		list_add(&sub_constraint->slave_domain, &master->max_masters);

		/* linked sub constraint */
		constraint->sub_constraint = sub_constraint;
	}

	exynos_dm_topological_sort();

	mutex_unlock(&exynos_dm->lock);

	return 0;

err_freq_table:
	kfree(sub_constraint);
err_sub_const:
	list_del(&constraint->master_domain);
	list_del(&constraint->slave_domain);

	mutex_unlock(&exynos_dm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(register_exynos_dm_constraint_table);

int unregister_exynos_dm_constraint_table(int dm_type,
				struct exynos_dm_constraint *constraint)
{
	struct exynos_dm_constraint *sub_constraint;
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	if (!constraint) {
		dev_err(exynos_dm->dev, "constraint is not valid\n");
		return -EINVAL;
	}

	mutex_lock(&exynos_dm->lock);

	if (constraint->sub_constraint) {
		sub_constraint = constraint->sub_constraint;
		list_del(&sub_constraint->master_domain);
		list_del(&sub_constraint->slave_domain);
		kfree(sub_constraint->freq_table);
		kfree(sub_constraint);
	}

	list_del(&constraint->master_domain);
	list_del(&constraint->slave_domain);

	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_exynos_dm_constraint_table);

/*
 * This function should be called from each DVFS driver registration function
 * before return to corresponding DVFS drvier.
 * 	Initialize sequence Step.3
 */
int register_exynos_dm_freq_scaler(int dm_type,
			int (*scaler_func)(int dm_type, void *devdata, u32 target_freq, unsigned int relation))
{
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	if (!scaler_func) {
		dev_err(exynos_dm->dev, "function is not valid\n");
		return -EINVAL;
	}

	mutex_lock(&exynos_dm->lock);

	if (!exynos_dm->dm_data[dm_type].available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	if (!exynos_dm->dm_data[dm_type].freq_scaler)
		exynos_dm->dm_data[dm_type].freq_scaler = scaler_func;

out:
	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(register_exynos_dm_freq_scaler);

int unregister_exynos_dm_freq_scaler(int dm_type)
{
	int ret = 0;

	ret = exynos_dm_index_validate(dm_type);
	if (ret)
		return ret;

	mutex_lock(&exynos_dm->lock);

	if (!exynos_dm->dm_data[dm_type].available) {
		dev_err(exynos_dm->dev,
			"This dm type(%d) is not available\n", dm_type);
		ret = -ENODEV;
		goto out;
	}

	if (exynos_dm->dm_data[dm_type].freq_scaler)
		exynos_dm->dm_data[dm_type].freq_scaler = NULL;

out:
	mutex_unlock(&exynos_dm->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_exynos_dm_freq_scaler);

/*
 * Policy Updater
 *
 * @dm_type: DVFS domain type for updating policy
 * @min_freq: Minimum frequency decided by policy
 * @max_freq: Maximum frequency decided by policy
 *
 * In this function, policy_min_freq and policy_max_freq will be changed.
 * After that, DVFS Manager will decide min/max freq. of current domain
 * and check dependent domains whether update is necessary.
 */

#define POLICY_REQ	4

/* DM Algorithm */
static int update_constraint_min(struct exynos_dm_constraint *constraint, u32 master_min)
{
	struct exynos_dm_data *dm = &exynos_dm->dm_data[constraint->dm_slave];
	struct exynos_dm_freq *const_table = constraint->freq_table;
	struct exynos_dm_constraint *t;
	int i;

	// This constraint condition could be ignored when perf lock is disabled
	if (exynos_dm->dynamic_disable && constraint->support_dynamic_disable)
		master_min = 0;

	// Find constraint condition for min relationship
	for (i = constraint->table_length - 1; i >= 0; i--) {
		if (const_table[i].master_freq >= master_min)
			break;
	}

	// If i is lesser than 0, there is no constraint condition.
	if (i < 0)
		i = 0;

	constraint->const_freq = const_table[i].slave_freq;
	dm->const_min = 0;

	// Find min constraint frequency from master domains
	list_for_each_entry(t, &dm->min_masters, slave_domain) {
		dm->const_min = max(t->const_freq, dm->const_min);
	}

	return 0;
}

static int update_constraint_max(struct exynos_dm_constraint *constraint, u32 master_max)
{
	struct exynos_dm_data *dm = &exynos_dm->dm_data[constraint->dm_slave];
	struct exynos_dm_freq *const_table = constraint->freq_table;
	struct exynos_dm_constraint *t;
	int i;

	// Find constraint condition for max relationship
	for (i = 0; i < constraint->table_length; i++) {
		if (const_table[i].master_freq <= master_max)
			break;
	}

	constraint->const_freq = const_table[i].slave_freq;
	dm->const_max = UINT_MAX;

	// Find max constraint frequency from master domains
	list_for_each_entry(t, &dm->max_masters, slave_domain) {
		dm->const_max = min(t->const_freq, dm->const_max);
	}

	return 0;
}

int policy_update_call_to_DM(int dm_type, u32 min_freq, u32 max_freq)
{
	struct exynos_dm_data *dm;
	u64 pre, before, after;
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	struct ipc_config config;
	unsigned int cmd[4];
	int size, ch_num;
#endif
	s32 time = 0, pre_time = 0;
	struct exynos_dm_data *domain;
	u32 prev_min, prev_max, new_min, new_max;
	int ret = 0, i;
	struct exynos_dm_constraint *t;
	bool update_max_policy = false;

	dbg_snapshot_dm((int)dm_type, min_freq, max_freq, pre_time, time);

	pre = sched_clock();
	mutex_lock(&exynos_dm->lock);
	before = sched_clock();

	dm = &exynos_dm->dm_data[dm_type];

	// Return if there has no min/max freq update
	if (max_freq == 0 && min_freq == 0) {
		ret = -EINVAL;
		goto out;
	}

	prev_min = max(dm->policy_min, dm->const_min);
	prev_max = min(dm->policy_max, dm->const_max);

	if (dm->policy_max == max_freq && dm->policy_min == min_freq)
		goto out;

	if (max_freq == 0)
		max_freq = dm->policy_max;

	if (min_freq == 0)
		min_freq = dm->policy_min;

	if (dm->policy_max != max_freq)
		update_max_policy = true;
	dm->policy_max = max_freq;
	dm->policy_min = min_freq;

	/*Send policy to FVP*/
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
	if (dm->policy_use && update_max_policy) {
		ret = acpm_ipc_request_channel(exynos_dm->dev->of_node, NULL, &ch_num, &size);
		if (ret) {
			dev_err(exynos_dm->dev,
					"acpm request channel is failed, id:%u, size:%u\n", ch_num, size);
			goto out;
		}
		config.cmd = cmd;
		config.response = true;
		config.indirection = false;
		config.cmd[0] = dm->cal_id;
		config.cmd[1] = max_freq;
		config.cmd[2] = POLICY_REQ;

		ret = acpm_ipc_send_data(ch_num, &config);
		if (ret) {
			dev_err(exynos_dm->dev, "Failed to send policy data to FVP");
			goto out;
		}
	}
#endif

	if (list_empty(&dm->min_slaves) && list_empty(&dm->max_slaves))
		goto out;

	new_min = max(dm->policy_min, dm->const_min);
	new_max = min(dm->policy_max, dm->const_max);
	new_min = min(new_min, new_max);

	if (new_min != prev_min) {
		int min_freq, max_freq;

		for (i = dm->my_order; i < exynos_dm->constraint_domain_count; i++) {
			domain = &exynos_dm->dm_data[exynos_dm->domain_order[i]];
			min_freq = max(domain->policy_min, domain->const_min);
			max_freq = min(domain->policy_max, domain->const_max);
			min_freq = min(min_freq, max_freq);
			list_for_each_entry(t, &domain->min_slaves, master_domain) {
				update_constraint_min(t, min_freq);
			}
		}
	}

	if (new_max != prev_max) {
		int max_freq;

		for (i = dm->my_order; i >= 0; i--) {
			domain = &exynos_dm->dm_data[exynos_dm->domain_order[i]];
			max_freq = min(domain->policy_max, domain->const_max);
			list_for_each_entry(t, &domain->max_slaves, master_domain) {
				update_constraint_max(t, max_freq);
			}
		}
	}
out:
	after = sched_clock();
	mutex_unlock(&exynos_dm->lock);

	pre_time = (unsigned int)(before - pre);
	time = (unsigned int)(after - before);

	dbg_snapshot_dm((int)dm_type, min_freq, max_freq, pre_time, time);

	return 0;
}
EXPORT_SYMBOL_GPL(policy_update_call_to_DM);

static void update_new_target(int dm_type)
{
	u32 min_freq, max_freq;
	struct exynos_dm_data *dm = &exynos_dm->dm_data[dm_type];

	if (dm->policy_max > dm->const_max)
		max_freq = dm->policy_max;

	min_freq = max3(dm->policy_min, dm->const_min, dm->gov_min);
	min_freq = max(min_freq, dm->governor_freq);
	max_freq = min(dm->policy_max, dm->const_max);
	exynos_dm->dm_data[dm_type].next_target_freq = min(min_freq, max_freq);
}

static int update_gov_min(struct exynos_dm_constraint *constraint, u32 master_freq)
{
	struct exynos_dm_data *dm = &exynos_dm->dm_data[constraint->dm_slave];
	struct exynos_dm_freq *const_table = constraint->freq_table;
	struct exynos_dm_constraint *t;
	int i;

	// This constraint condition could be ignored when perf lock is disabled
	if (exynos_dm->dynamic_disable && constraint->support_dynamic_disable)
		master_freq = 0;

	// Find constraint condition for min relationship
	for (i = constraint->table_length - 1; i >= 0; i--) {
		if (const_table[i].master_freq >= master_freq)
			break;
	}

	// If i is lesser than 0, there is no constraint condition.
	if (i < 0)
		i = 0;

	constraint->gov_freq = const_table[i].slave_freq;
	dm->gov_min = 0;

	// Find gov_min frequency from master domains
	list_for_each_entry(t, &dm->min_masters, slave_domain) {
		dm->gov_min = max(t->gov_freq, dm->gov_min);
	}

	return 0;
}

/*
 * DM CALL
 */
int __DM_CALL(int dm_type, unsigned long *target_freq)
{
	struct exynos_dm_data *target_dm;
	struct exynos_dm_data *dm;
	struct exynos_dm_constraint *t;
	u32 max_freq, min_freq;
	int i, ret = 0;
	unsigned int relation = EXYNOS_DM_RELATION_L;
	u64 pre, before, after;
	s32 time = 0, pre_time = 0;

	dbg_snapshot_dm((int)dm_type, *target_freq, 1, pre_time, time);

	pre = sched_clock();
	mutex_lock(&exynos_dm->lock);
	before = sched_clock();

	exynos_update_ip_idle_status(dm_idle_ip_index, 0);

	target_dm = &exynos_dm->dm_data[dm_type];

	if (*target_freq != 0) {
		target_dm->governor_freq = *target_freq;

		// Determine min/max frequency
		max_freq = min(target_dm->const_max, target_dm->policy_max);
		min_freq = max(target_dm->const_min, target_dm->policy_min);

		if (*target_freq < min_freq)
			*target_freq = min_freq;

		if (*target_freq >= max_freq) {
			*target_freq = max_freq;
			relation = EXYNOS_DM_RELATION_H;
		}
	}

	if (target_dm->cur_freq == *target_freq && !target_dm->fast_switch)
		goto out;

	if (list_empty(&target_dm->max_slaves) && list_empty(&target_dm->min_slaves) && target_dm->freq_scaler) {
		update_new_target(target_dm->dm_type);
		ret = target_dm->freq_scaler(target_dm->dm_type, target_dm->devdata, target_dm->next_target_freq, relation);
		if (!ret)
			target_dm->cur_freq = target_dm->next_target_freq;
		goto out;
	}

	// Propagate the influence of new terget frequencies
	for (i = 0; i < exynos_dm->constraint_domain_count; i++) {
		dm = &exynos_dm->dm_data[exynos_dm->domain_order[i]];

		// Update new target frequency from all min, max restricts.
		update_new_target(dm->dm_type);

		// Travers all slave domains to update the gov_min value.
		list_for_each_entry(t, &dm->min_slaves, master_domain) {
			update_gov_min(t, dm->next_target_freq);
		}

		// Perform frequency down scaling
		if (dm->cur_freq > dm->next_target_freq && dm->freq_scaler && !dm->fast_switch) {
			ret = dm->freq_scaler(dm->dm_type, dm->devdata, dm->next_target_freq, relation);
			if (!ret)
				dm->cur_freq = dm->next_target_freq;
		}
	}

	// Perform frequency up scaling
	for (i = exynos_dm->constraint_domain_count - 1; i >= 0; i--) {
		dm = &exynos_dm->dm_data[exynos_dm->domain_order[i]];
		if (dm->cur_freq < dm->next_target_freq && dm->freq_scaler && !dm->fast_switch) {
			ret = dm->freq_scaler(dm->dm_type, dm->devdata, dm->next_target_freq, relation);
			if (!ret)
				dm->cur_freq = dm->next_target_freq;
		}

	}

out:
	after = sched_clock();
	exynos_update_ip_idle_status(dm_idle_ip_index, 1);

	mutex_unlock(&exynos_dm->lock);

	pre_time = (unsigned int)(before - pre);
	time = (unsigned int)(after - before);

	*target_freq = target_dm->cur_freq;

	dbg_snapshot_dm((int)dm_type, *target_freq, 3, pre_time, time);

	return ret;
}

static int exynos_dm_suspend(struct device *dev)
{
	/* Suspend callback function might be registered if necessary */

	return 0;
}

static int exynos_dm_resume(struct device *dev)
{
	/* Resume callback function might be registered if necessary */

	return 0;
}

static void exynos_dm_fast_switch_post_irq_work(struct irq_work *irq_work)
{
	struct exynos_dm_data *data;

	data = container_of(irq_work,
			      struct exynos_dm_data,
			      fast_switch_post_irq_work);
	if (unlikely(!data))
		return;

	/* Run fast switch work */
	wake_up_process(data->fast_switch_post_worker);
}

static int exynos_dm_fast_switch_post_func(void *__data)
{
	struct exynos_dm_data *data = __data;
	u32 cur_freq, old_cur_freq = 0;
	unsigned long target_freq = 0;

	while (true) {
		data->fast_switch_post_in_progress = false;

		set_current_state(TASK_INTERRUPTIBLE);	/* mb paired w/ kthread_stop */

		if (kthread_should_stop()) {
			__set_current_state(TASK_RUNNING);
			return 0;
		}

		cur_freq = data->cur_freq;

		if (cur_freq != old_cur_freq) {	// if governor freq is changed after scale, retry it
			__set_current_state(TASK_RUNNING);
			target_freq = 0;
			__DM_CALL(data->dm_type, &target_freq);
			old_cur_freq = cur_freq;
		} else if (!freezing(current))
			schedule();

		try_to_freeze();
		cond_resched();
	}
}

int exynos_dm_fast_switch(int dm_type, unsigned long *target_freq)
{
	int ret = 0;
	struct exynos_dm_data *data = &exynos_dm->dm_data[dm_type];
	u32 max_freq, min_freq, relation = EXYNOS_DM_RELATION_L;
	unsigned long flags;

	max_freq = min(data->const_max, data->policy_max);
	min_freq = max(data->const_min, data->policy_min);
	data->governor_freq = *target_freq;

	if (*target_freq < min_freq)
		*target_freq = min_freq;

	if (*target_freq >= max_freq) {
		*target_freq = max_freq;
		relation = EXYNOS_DM_RELATION_H;
	}

	update_new_target(dm_type);

	if (data->next_target_freq == data->cur_freq)
		return -EINVAL;

	spin_lock_irqsave(&fast_switch_glb_lock, flags);
	if (!is_acpm_ipc_busy(exynos_dm->fast_switch_ch)) {
		// disable sicd
		exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 0);
	}

	// Call scale callback
	ret = data->freq_scaler(data->dm_type, data->devdata, data->next_target_freq, relation);
	if (ret) {
		if (!is_acpm_ipc_busy(exynos_dm->fast_switch_ch))
			exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);
		pr_err("[%s] freq request fail: %d\n", __func__, ret);
		spin_unlock_irqrestore(&fast_switch_glb_lock, flags);
		return ret;
	}
	spin_unlock_irqrestore(&fast_switch_glb_lock, flags);

	data->cur_freq = data->next_target_freq;

	if (!data->fast_switch_post_in_progress) {
		data->fast_switch_post_in_progress = true;
		irq_work_queue(&data->fast_switch_post_irq_work);
	}

	return ret;
}

int DM_CALL(int dm_type, unsigned long *target_freq)
{
	int ret = 0;
	struct exynos_dm_data *data = &exynos_dm->dm_data[dm_type];

	if (!data->fast_switch)
		__DM_CALL(dm_type, target_freq);
	else
		ret = exynos_dm_fast_switch(dm_type, target_freq);

	return ret;
}
EXPORT_SYMBOL_GPL(DM_CALL);

static int exynos_dm_fast_switch_init(struct exynos_dm_data *data)
{
	struct cpumask mask;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	struct task_struct *thread;
	int ret = 0;

	init_irq_work(&data->fast_switch_post_irq_work, exynos_dm_fast_switch_post_irq_work);

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	thread = kthread_create(exynos_dm_fast_switch_post_func, data,
			"fast_switch_post_worker_%s", data->dm_type_name);
	if (IS_ERR(thread)) {
		pr_err("%s: failed to create fast_switch_post_worker thread: %ld\n", __func__,
				PTR_ERR(thread));
		ret = PTR_ERR(thread);

		goto out;
	}
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		pr_warn("%s: dm failed to set SCHED_FIFO\n", __func__);
		goto out;
	}

	data->fast_switch_post_worker = thread;

	mutex_init(&data->fast_switch_lock);

out:
	if (ret)
		data->fast_switch = false;

	return ret;
}

static int exynos_dm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_dm_device *dm;
	int i;

	dm = kzalloc(sizeof(struct exynos_dm_device), GFP_KERNEL);
	if (dm == NULL) {
		dev_err(&pdev->dev, "failed to allocate DVFS Manager device\n");
		ret = -ENOMEM;
		goto err_device;
	}

	dm->dev = &pdev->dev;

	mutex_init(&dm->lock);

	/* parsing devfreq dts data for exynos-dvfs-manager */
	ret = exynos_dm_parse_dt(dm->dev->of_node, dm);
	if (ret) {
		dev_err(dm->dev, "failed to parse private data\n");
		goto err_parse_dt;
	}

	print_available_dm_data(dm);

	ret = sysfs_create_group(&dm->dev->kobj, &exynos_dm_attr_group);
	if (ret)
		dev_warn(dm->dev, "failed create sysfs for DVFS Manager\n");

	for (i = 0; i < dm->domain_count; i++) {
		if (!dm->dm_data[i].available)
			continue;

		snprintf(dm->dm_data[i].dm_policy_attr.name, EXYNOS_DM_ATTR_NAME_LEN,
				"dm_policy_%s", dm->dm_data[i].dm_type_name);
		sysfs_attr_init(&dm->dm_data[i].dm_policy_attr.attr.attr);
		dm->dm_data[i].dm_policy_attr.attr.attr.name =
			dm->dm_data[i].dm_policy_attr.name;
		dm->dm_data[i].dm_policy_attr.attr.attr.mode = (S_IRUSR | S_IRGRP);
		dm->dm_data[i].dm_policy_attr.attr.show = show_dm_policy;

		ret = sysfs_add_file_to_group(&dm->dev->kobj, &dm->dm_data[i].dm_policy_attr.attr.attr, exynos_dm_attr_group.name);
		if (ret)
			dev_warn(dm->dev, "failed create sysfs for DM policy %s\n", dm->dm_data[i].dm_type_name);


		snprintf(dm->dm_data[i].constraint_table_attr.name, EXYNOS_DM_ATTR_NAME_LEN,
				"constaint_table_%s", dm->dm_data[i].dm_type_name);
		sysfs_attr_init(&dm->dm_data[i].constraint_table_attr.attr.attr);
		dm->dm_data[i].constraint_table_attr.attr.attr.name =
			dm->dm_data[i].constraint_table_attr.name;
		dm->dm_data[i].constraint_table_attr.attr.attr.mode = (S_IRUSR | S_IRGRP);
		dm->dm_data[i].constraint_table_attr.attr.show = show_constraint_table;

		ret = sysfs_add_file_to_group(&dm->dev->kobj, &dm->dm_data[i].constraint_table_attr.attr.attr, exynos_dm_attr_group.name);
		if (ret)
			dev_warn(dm->dev, "failed create sysfs for constraint_table %s\n", dm->dm_data[i].dm_type_name);

		if (dm->dm_data[i].fast_switch)
			exynos_dm_fast_switch_init(&dm->dm_data[i]);
	}

	exynos_dm = dm;

	dm_idle_ip_index = exynos_get_idle_ip_index(EXYNOS_DM_MODULE_NAME, 1);
	exynos_update_ip_idle_status(dm_idle_ip_index, 1);

	dm_fast_switch_idle_ip_index = exynos_get_idle_ip_index("EXYNOS_DM_FAST_SWITCH", 1);
	exynos_update_ip_idle_status(dm_fast_switch_idle_ip_index, 1);

	spin_lock_init(&fast_switch_glb_lock);

	platform_set_drvdata(pdev, dm);

	return 0;

err_parse_dt:
	mutex_destroy(&dm->lock);
	kfree(dm);
err_device:

	return ret;
}

static int exynos_dm_remove(struct platform_device *pdev)
{
	struct exynos_dm_device *dm = platform_get_drvdata(pdev);

	sysfs_remove_group(&dm->dev->kobj, &exynos_dm_attr_group);
	mutex_destroy(&dm->lock);
	kfree(dm);

	return 0;
}

static struct platform_device_id exynos_dm_driver_ids[] = {
	{
		.name		= EXYNOS_DM_MODULE_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_dm_driver_ids);

static const struct of_device_id exynos_dm_match[] = {
	{
		.compatible	= "samsung,exynos-dvfs-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dm_match);

static const struct dev_pm_ops exynos_dm_pm_ops = {
	.suspend	= exynos_dm_suspend,
	.resume		= exynos_dm_resume,
};

static struct platform_driver exynos_dm_driver = {
	.probe		= exynos_dm_probe,
	.remove		= exynos_dm_remove,
	.id_table	= exynos_dm_driver_ids,
	.driver	= {
		.name	= EXYNOS_DM_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &exynos_dm_pm_ops,
		.of_match_table = exynos_dm_match,
	},
};

static int exynos_dm_init(void)
{
	return platform_driver_register(&exynos_dm_driver);
}
subsys_initcall(exynos_dm_init);

static void __exit exynos_dm_exit(void)
{
	platform_driver_unregister(&exynos_dm_driver);
}
module_exit(exynos_dm_exit);

MODULE_AUTHOR("Taekki Kim <taekki.kim@samsung.com>");
MODULE_AUTHOR("Eunok Jo <eunok25.jo@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS SoC series DVFS Manager");
MODULE_LICENSE("GPL");
