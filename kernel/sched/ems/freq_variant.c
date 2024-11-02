// SPDX-License-Identifier: GPL-2.0
/*
 * FV (Frequency Variant) for EMS features
 *
 * Copyright (C) 2022 - 2023 Samsung Corporation
 * Author: Youngtae Lee <yt0729.lee@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <trace/hooks/cpufreq.h>

#include "../sched.h"
#include "ems.h"

#define EXIT_RATIO	10

struct resi_state {
	u32 freq;
	u32 resi;
};

struct resi_table {
	int nr_state;
	s64 exit_latency;
	struct resi_state *states;
};

struct fv_policy {
	struct resi_table tbl;

	struct cpumask cpus;
	struct kobject kobj;
};

struct fv_cpu {
	struct fv_policy *pol;
};

struct kobject *fv_kobj;
static DEFINE_PER_CPU(struct fv_cpu, fv_cpus);

/* return policy data if fv initialized done */
static inline struct fv_policy *fv_get_policy(int cpu)
{
	struct fv_policy *pol = per_cpu_ptr(&fv_cpus, cpu)->pol;

	if (unlikely(!pol) || unlikely(!pol->tbl.states))
		return NULL;
	return pol;
}

/********************************************************************************
 *				FV SERVICE API's				*
 *******************************************************************************/
u64 fv_get_residency(int cpu, int state)
{
	struct fv_policy *pol = fv_get_policy(cpu);
	int cur_idx = et_cur_freq_idx(cpu);

	if (unlikely(!pol))
		return TICK_NSEC;

	/* WFI */
	if (!state)
		return 1;

	return pol->tbl.states[cur_idx].resi;
}
EXPORT_SYMBOL_GPL(fv_get_residency);

u64 fv_get_exit_latency(int cpu, int state)
{
	struct fv_policy *pol = fv_get_policy(cpu);

	if (unlikely(!pol))
		return TICK_NSEC;

	if (!state)
		return 1;

	return pol->tbl.exit_latency;
}
EXPORT_SYMBOL_GPL(fv_get_exit_latency);

/********************************************************************************
 *				FV SYSFS					*
 *******************************************************************************/
struct fv_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

#define fv_attr_rw(name)				\
static struct fv_attr name##_attr =			\
__ATTR(name, 0644, show_##name, store_##name)

static ssize_t show_resi_table(struct kobject *k, char *buf)
{
	struct fv_policy *pol = container_of(k, struct fv_policy, kobj);
	struct resi_table *table = &pol->tbl;
	int idx, ret = 0;

	ret += sprintf(buf + ret, "         Freq     Resi(us)\n");
	for (idx = 0; idx < table->nr_state; idx++)
		ret += sprintf(buf + ret, "lv%2d: %8d %6d us\n", idx,
			table->states[idx].freq,
			table->states[idx].resi / NSEC_PER_USEC);
	return ret;
}
static ssize_t store_resi_table(struct kobject *k, const char *buf, size_t count)
{
	struct fv_policy *pol = container_of(k, struct fv_policy, kobj);
	u32 idx, resi;

	if (sscanf(buf, "%d %d", &idx, &resi) != 2)
		return -EINVAL;

	if (!pol->tbl.states)
		return -EINVAL;

	if (idx >= pol->tbl.nr_state)
		return -EINVAL;

	pol->tbl.states[idx].resi = resi * NSEC_PER_USEC;

	return count;
}
fv_attr_rw(resi_table);

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct fv_attr *attr = container_of(at, struct fv_attr, attr);
	return attr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct fv_attr *attr = container_of(at, struct fv_attr, attr);
	return attr->store(kobj, buf, count);
}

static const struct sysfs_ops fv_sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct attribute *fv_attrs[] = {
	&resi_table_attr.attr,
	NULL
};

static struct kobj_type ktype_fv = {
	.sysfs_ops	= &fv_sysfs_ops,
	.default_attrs	= fv_attrs,
};

/* fv_init_table
 * When cpufreq policy initialized, this function will be called.
 * To get the accurate cpufreq information, we need a cpufreq driver
 */
void fv_init_table(struct cpufreq_policy *policy)
{
	int cpu = policy->cpu;
	struct fv_policy *pol = per_cpu_ptr(&fv_cpus, cpu)->pol;
	struct resi_table *init_table;
	struct cpufreq_frequency_table *cursor;
	int table_size = 0, idx, tmp_idx;
	struct resi_state *states;

	if (unlikely(!pol))
		return;

	init_table = &pol->tbl;
	if (!init_table->states) {
		pr_info("%s: CPU%d: there is no residency table\n", __func__, cpu);
		return;
	}

	/* Count valid frequency */
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		if ((cursor->frequency > policy->cpuinfo.max_freq) ||
		    (cursor->frequency < policy->cpuinfo.min_freq))
			continue;
		table_size++;
	}
	/* There is no valid frequency in the table, cancels building energy table */
	if (!table_size)
		return;

	states = kzalloc(table_size * sizeof(struct resi_state), GFP_KERNEL);
	if (!states)
		return;

	/* Fill the energy table with frequency, dynamic/static power and voltage */
	tmp_idx = idx = 0;
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		u32 freq = cursor->frequency;
		if ((freq > policy->cpuinfo.max_freq) ||
		    (freq < policy->cpuinfo.min_freq))
			continue;

		/* find initial residency from device tree table */
		while (tmp_idx < init_table->nr_state - 1) {
			if (init_table->states[tmp_idx].freq < freq)
				tmp_idx++;
			else
				break;
		}
		states[idx].freq = freq;
		states[idx].resi = init_table->states[tmp_idx].resi;
		idx++;
	}

	kfree(init_table->states);

	pol->tbl.exit_latency = states[0].resi * EXIT_RATIO / 100;
	pol->tbl.nr_state = table_size;
	pol->tbl.states = states;
}

static int fv_dt_parse(struct kobject *ems_kobj)
{
	struct device_node *dn, *child;
	int cpu;

	fv_kobj = kobject_create_and_add("freq_variant", ems_kobj);
	if (!fv_kobj)
		return -1;

	dn = of_find_node_by_path("/ems/freq-variant");
	if (!dn)
		return -1;

	for_each_child_of_node(dn, child) {
		struct fv_policy *pol;
		struct resi_state *states;
		const char *buf;
		u32 tmp_size, size;
		u32 tmp_idx, idx;
		u32 *tmp_states;

		if (of_property_read_string(child, "cpus", &buf)) {
			pr_info("%s: cpus property is omitted\n", __func__);
			return -1;
		}

		pol = kzalloc(sizeof(struct fv_policy), GFP_KERNEL);
		if (!pol) {
			pr_info("%s: failed to alloc fv policy \n", __func__);
			return -1;
		}
		cpulist_parse(buf, &pol->cpus);

		/* copy raw data from dt */
		tmp_size = of_property_count_u32_elems(child, "table");
		if (tmp_size < 0)
			return -1;
		tmp_states = kcalloc(tmp_size, sizeof(u32), GFP_KERNEL);
		if (!tmp_states)
			return -1;
		if (of_property_read_u32_array(child, "table", tmp_states, tmp_size)) {
			pr_info("%s: there is no device tree table  \n", __func__);
			return -1;
		}

		/* copy freq, resi to init table from raw table and alloc table */
		size = tmp_size / 2;
		states = kzalloc(size * sizeof(struct resi_state), GFP_KERNEL);
		if (!tmp_states)
			return -1;
		for (idx = 0, tmp_idx = 0; idx < size; idx++) {
			states[idx].freq = tmp_states[tmp_idx++];
			states[idx].resi = tmp_states[tmp_idx++] * NSEC_PER_USEC;
		}

		/* Add sysfs node */
		if (kobject_init_and_add(&pol->kobj, &ktype_fv, fv_kobj,
					"coregroup%d", cpumask_first(&pol->cpus)))
			return -1;

		/* alloc driver data */
		pol->tbl.states = states;
		pol->tbl.nr_state = size;

		for_each_cpu(cpu, &pol->cpus)
			per_cpu_ptr(&fv_cpus, cpu)->pol = pol;

		kfree(tmp_states);
	}
	return 0;
}

int fv_init(struct kobject *ems_kobj)
{
	if (fv_dt_parse(ems_kobj))
		return -1;
	return 0;
}
