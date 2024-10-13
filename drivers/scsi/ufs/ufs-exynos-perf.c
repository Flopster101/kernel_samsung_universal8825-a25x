/*
 * IO performance mode with UFS
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#include <linux/of.h>
#include "ufs-exynos-perf.h"

#define CREATE_TRACE_POINTS
#include <linux/pm_qos.h>
#include <trace/events/ufs_exynos_perf.h>

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
static struct ufs_perf *_perf;

static int ufs_perf_cpufreq_nb(struct notifier_block *nb,
		unsigned long event, void *arg)
{
	struct cpufreq_policy *policy = (struct cpufreq_policy *)arg;
	struct ufs_perf *perf = _perf;

	if (event != CPUFREQ_CREATE_POLICY)
		return NOTIFY_OK;

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	if (policy->cpu == perf->clusters[0])
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->pm_qos_cluster0, FREQ_QOS_MIN, 0);

	else if (policy->cpu == perf->clusters[1])
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->pm_qos_cluster1, FREQ_QOS_MIN, 0);
	else if (policy->cpu == perf->clusters[2])
		freq_qos_tracer_add_request(&policy->constraints,
				&perf->pm_qos_cluster2, FREQ_QOS_MIN, 0);
#else
	if (policy->cpu == perf->clusters[0])
		freq_qos_add_request(&policy->constraints,
				&perf->pm_qos_cluster0, FREQ_QOS_MIN, 0);
	else if (policy->cpu == perf->clusters[1])
		freq_qos_add_request(&policy->constraints,
				&perf->pm_qos_cluster1, FREQ_QOS_MIN, 0);
	else if (policy->cpu == perf->clusters[2])
		freq_qos_add_request(&policy->constraints,
				&perf->pm_qos_cluster2, FREQ_QOS_MIN, 0);
#endif
	return NOTIFY_OK;
}
#endif

void ufs_perf_complete(struct ufs_perf *perf)
{
	complete(&perf->completion);
}

static int ufs_perf_handler(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	unsigned long flags;
	u32 ctrl_handle[__CTRL_REQ_MAX];
	int i;
	int idle;

	init_completion(&perf->completion);

	while (true) {
		if (kthread_should_stop())
			break;

		/* get requests */
		spin_lock_irqsave(&perf->lock_handle, flags);
		for (i = 0; i < __CTRL_REQ_MAX; i++) {
			ctrl_handle[i] = perf->ctrl_handle[i];
			perf->ctrl_handle[i] = CTRL_OP_NONE;
		}
		spin_unlock_irqrestore(&perf->lock_handle, flags);

		/* execute */
		idle = 0;
		for (i = 0; i < __CTRL_REQ_MAX; i++) {
			//trace_ufs_perf("control", 0, (int)ctrl_handle[i]);	//TODO: modify
			if (ctrl_handle[i] == CTRL_OP_NONE) {
				idle++;
			} else if (perf->ctrl[i]) {
				/* TODO: implement */
				perf->ctrl[i](perf, ctrl_handle[i]);
			}
		}
		if (idle == __CTRL_REQ_MAX) {
			trace_ufs_perf_lock("sleep", ctrl_handle[0]);
			wait_for_completion_timeout(&perf->completion, 10 * HZ);
			trace_ufs_perf_lock("wake-up", ctrl_handle[0]);
		}
	}

	return 0;
}

/* EXTERNAL FUNCTIONS */
void ufs_perf_update(void *data, u32 qd, struct scsi_cmnd *cmd, enum ufs_perf_op op)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	enum policy_res res = R_OK;
	unsigned long stat_bits = (unsigned long)perf->stat_bits;
	ktime_t time = ktime_get();
	int index;
	enum policy_res res_t = R_OK;
	unsigned long lba = (cmd->cmnd[2] << 24) |
					(cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) |
					(cmd->cmnd[5] << 0);
	unsigned long len = cmd->request->__data_len / 512;
	struct ufs_perf_stat_v1 *stat = &perf->stat_v1;
	struct ufs_perf_stat_v2 *stat_v2 = &perf->stat_v2;

	if (lba == stat->last_lba + len) {
		stat->seq_continue_count++;
		stat->last_lba = lba;
	} else {
		stat->seq_continue_count = 0;
		stat->last_lba = lba;
	}
	stat_v2->count += cmd->request->__data_len;

	for_each_set_bit(index, &stat_bits, __POLICY_MAX) {
		if (!(BIT(index) & perf->stat_bits))
			continue;
		res_t = perf->update[index](perf, qd, op, UFS_PERF_ENTRY_QUEUED);
		if (res_t == R_CTRL)
			res = res_t;
	}

	/* wake-up thread */
	if (res == R_CTRL)
		complete(&perf->completion);

	trace_ufs_perf("update", op, qd, ktime_to_us(ktime_sub(ktime_get(), time)), res, len);
}

void ufs_perf_reset(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;
	enum policy_res res = R_OK;
	enum policy_res res_t = R_OK;
	int index;

	for (index = 0; index < __UPDATE_MAX; index++) {
		if (!(BIT(index) & perf->stat_bits))
			continue;
		res_t = perf->update[index](perf, 0, UFS_PERF_OP_NONE, UFS_PERF_ENTRY_RESET);
		if (res_t == R_CTRL)
			res = res_t;
	}

	/* wake-up thread */
	if (res_t == R_CTRL)
		complete(&perf->completion);
}

/* sysfs*/
struct __sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct ufs_perf *perf, char *buf);
	int (*store)(struct ufs_perf *perf, u32 value);
};

#define __SYSFS_NODE(_name)							\
static ssize_t __sysfs_##_name##_show(struct ufs_perf *perf,			\
						   char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, "%d\n", perf->val_##_name);		\
};										\
static int __sysfs_##_name##_store(struct ufs_perf *perf, u32 value)		\
{										\
	perf->val_##_name = (s32) value;					\
										\
	return 0;								\
};										\
static struct __sysfs_attr __sysfs_node_##_name = {				\
	.attr = { .name = #_name, .mode = 0666 },				\
	.show = __sysfs_##_name##_show,						\
	.store = __sysfs_##_name##_store,					\
}										\

__SYSFS_NODE(pm_qos_int);
__SYSFS_NODE(pm_qos_mif);
__SYSFS_NODE(pm_qos_cluster0);
__SYSFS_NODE(pm_qos_cluster1);
__SYSFS_NODE(pm_qos_cluster2);

const static struct attribute *__sysfs_attrs[] = {
	&__sysfs_node_pm_qos_int.attr,
	&__sysfs_node_pm_qos_mif.attr,
	&__sysfs_node_pm_qos_cluster0.attr,
	&__sysfs_node_pm_qos_cluster1.attr,
	&__sysfs_node_pm_qos_cluster2.attr,
	NULL,
};

static ssize_t __sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct ufs_perf *perf = container_of(kobj, struct ufs_perf, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr, struct __sysfs_attr, attr);

	return param->show(perf, buf);
}

static ssize_t __sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct ufs_perf *perf = container_of(kobj, struct ufs_perf, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr, struct __sysfs_attr, attr);
	u32 val;
	int ret = 0;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	ret = param->store(perf, val);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops __sysfs_ops = {
	.show	= __sysfs_show,
	.store	= __sysfs_store,
};

static struct kobj_type __sysfs_ktype = {
	.sysfs_ops	= &__sysfs_ops,
	.release	= NULL,
};

static int __sysfs_init(struct ufs_perf *perf)
{
	int error;

	/* create a path of /sys/kernel/ufs_perf_x */
	kobject_init(&perf->sysfs_kobj, &__sysfs_ktype);
	error = kobject_add(&perf->sysfs_kobj, kernel_kobj, "ufs_perf_%c", (char)('0'));	//
	if (error) {
		pr_err("%s register sysfs directory: %d\n",
		       __res_token[__TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&perf->sysfs_kobj, __sysfs_attrs);
	if (error) {
		pr_err("%s create sysfs files: %d\n",
		       __res_token[__TOKEN_FAIL], error);
		goto fail_kobj;
	}

	return 0;

fail_kobj:
	kobject_put(&perf->sysfs_kobj);
	return error;
}

static inline void __sysfs_exit(struct ufs_perf *perf)
{
	kobject_put(&perf->sysfs_kobj);
}

int ufs_perf_parse_cpu_clusters(unsigned int *clusters) {
	struct device_node *cpus, *map, *cluster, *cpu_node;
	char name[20];
	int num_clusters;

	cpus = of_find_node_by_path("/cpus");
	if (!cpus) {
		pr_err("No CPU information found in DT\n");
		return -1;
	}

	map = of_get_child_by_name(cpus, "cpu-map");
	if (!map) {
		pr_err("No CPU MAP node found in DT\n");
		return -1;
	}

	num_clusters = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", num_clusters);
		cluster = of_get_child_by_name(map, name);
		if (cluster) {
			cpu_node = of_get_child_by_name(cluster, "core0");
			if (cpu_node) {
				cpu_node = of_parse_phandle(cpu_node, "cpu", 0);
				clusters[num_clusters] = of_cpu_node_to_id(cpu_node);
			}
			num_clusters++;
		}
	} while (cluster);

	return num_clusters;
}

void ufs_init_cpufreq_request(struct ufs_perf *perf, bool add_noob) {
	s32 *values[MAX_CLUSTERS] = {&perf->val_pm_qos_cluster0,
				     &perf->val_pm_qos_cluster1,
				     &perf->val_pm_qos_cluster2};

	unsigned int wished[MAX_CLUSTERS] = {0, 0, 0};
	int table_len, ret;
	unsigned int i;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *pos, *table;
	struct device *dev = perf->hba->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;

	perf->num_clusters = ufs_perf_parse_cpu_clusters(perf->clusters);
	if(perf->num_clusters == 2)
		perf->clusters[2] = 0xFF;
	else if(perf->num_clusters == -1) {
		pr_err("%s: failed to parse CPU DT\n", __func__);
		return;
	}

	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	if (!child_np)
		dev_info(dev, "%s: No ufs-pm-qos node, not quarantee pm qos\n", __func__);
	else {
		ret = of_property_count_u32_elems(child_np, "cpufreq-qos-levels");
		if (!ret)
			dev_info(dev, "%s: No ufs-pm-qos node, not quarantee pm qos\n", __func__);
		else if (ret >0)
			of_property_read_u32_array(child_np, "cpufreq-qos-levels", wished, ret);
	}

	for (i=0; i<perf->num_clusters; ++i) {
		policy = cpufreq_cpu_get(perf->clusters[i]);
		if (!policy)
			continue;

		if (add_noob)
			ufs_perf_cpufreq_nb(NULL, CPUFREQ_CREATE_POLICY, policy);

		table = policy->freq_table;
		table_len = 0;
		cpufreq_for_each_valid_entry(pos, table) {
			table_len++;
		}

		if (table_len > wished[i])
			pos = table + wished[i];
		else
			pos = table + (table_len - 1);

		*values[i] = pos->frequency;
	}
}

void ufs_init_devfreq_request(struct ufs_perf *perf, struct ufs_hba *hba) {
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;

	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	perf->val_pm_qos_int = 0;
	perf->val_pm_qos_mif = 0;

	if (!child_np)
		dev_info(dev, "%s: No ufs-pm-qos node, not guarantee pm qos\n", __func__);
	else {
		of_property_read_u32(child_np, "perf-int", &perf->val_pm_qos_int);
		of_property_read_u32(child_np, "perf-mif", &perf->val_pm_qos_mif);
	}

	exynos_pm_qos_add_request(&perf->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&perf->pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);
}

bool ufs_perf_init(void **data, struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	struct ufs_perf *perf;
	bool ret = false;

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	struct cpufreq_policy pol;
#endif

	/* perf and perf->handler is used to check using performance mode */
	*data = devm_kzalloc(hba->dev, sizeof(struct ufs_perf), GFP_KERNEL);
	if (*data == NULL)
		goto out;

	perf = (struct ufs_perf *)(*data);

	spin_lock_init(&perf->lock_handle);

	perf->hba = hba;
	perf->handler = kthread_run(ufs_perf_handler, perf,
				"ufs_perf_%d", 0);
	if (IS_ERR(perf->handler))
		goto out;

	/* initial values, TODO: add sysfs nodes */
	perf->val_pm_qos_cluster0 = 0;
	perf->val_pm_qos_cluster1 = 0;
	perf->val_pm_qos_cluster2 = 0;

	perf->exynos_gear_scale = 0;
	if (of_find_property(np, "samsung,ufs-gear-scale", NULL)) {
		dev_info(dev, "%s: enable ufs-gear-scale\n", __func__);
		perf->exynos_gear_scale = 1;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	ufs_init_devfreq_request(perf, hba);
#endif

#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	_perf = perf;
	if (cpufreq_get_policy(&pol, 0) != 0) {
		perf->cpufreq_nb.notifier_call = ufs_perf_cpufreq_nb;
		cpufreq_register_notifier(&perf->cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
	} else {
		ufs_init_cpufreq_request(perf, true);
	}
#endif

	/* initial values, TODO: */
	perf->stat_bits = UPDATE_V1;
	if (perf->exynos_gear_scale)
		perf->stat_bits |= UPDATE_GEAR;
	/* sysfs */
	ret = __sysfs_init(perf);
	if (ret)
		goto out;

	/* register updates and ctrls */
	ufs_perf_init_v1(perf);
	ufs_gear_scale_init(perf);
	ret = true;
out:
	return ret;
}

void ufs_perf_exit(void *data)
{
	struct ufs_perf *perf = (struct ufs_perf *)data;

	ufs_perf_exit_v1(perf);

	__sysfs_exit(perf);

	if (perf && !IS_ERR(perf->handler)) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		exynos_pm_qos_remove_request(&perf->pm_qos_int);
		exynos_pm_qos_remove_request(&perf->pm_qos_mif);
#endif

#if IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
		freq_qos_tracer_remove_request(&perf->pm_qos_cluster0);
		freq_qos_tracer_remove_request(&perf->pm_qos_cluster1);
		if (perf->num_clusters == 3)
			freq_qos_tracer_remove_request(&perf->pm_qos_cluster2);
#elif IS_ENABLED(CONFIG_ARM_EXYNOS_ACME)
		freq_qos_remove_request(&perf->pm_qos_cluster0);
		freq_qos_remove_request(&perf->pm_qos_cluster1);
		if (perf->num_clusters == 3)
			freq_qos_remove_request(&perf->pm_qos_cluster2);
#endif

		complete(&perf->completion);
		kthread_stop(perf->handler);
	}
}
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos UFS performance booster");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
