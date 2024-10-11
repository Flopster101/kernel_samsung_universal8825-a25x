/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * User Frequency & Cstate Control driver
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 * Author : Park Bumgyu <bumgyu.park@samsung.com>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/ems.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-ufcc.h>
#include <soc/samsung/freq-qos-tracer.h>

/*********************************************************************
 *                         USER CSTATE CONTROL                       *
 *********************************************************************/
static int ucc_initialized = false;

/*
 * struct ucc_config
 *
 * - index : index of cstate config
 * - cstate : mask indicating whether each cpu supports cstate
 *    - mask bit set : cstate enable
 *    - mask bit unset : cstate disable
 */
struct ucc_config {
	int		index;
	struct cpumask	cstate;
};

static struct ucc_config *ucc_configs;
static int ucc_config_count;

/*
 * cur_cstate has cpu cstate config corresponding to maximum index among ucc
 * request. Whenever maximum requested index is changed, cur_state is updated.
 */
static struct cpumask cur_cstate;

static LIST_HEAD(ucc_req_list);

static DEFINE_SPINLOCK(ucc_lock);

static int ucc_get_value(void)
{
	struct ucc_req *req;

	req = list_first_entry_or_null(&ucc_req_list, struct ucc_req, list);

	return req ? req->value : -1;
}

static void update_cur_cstate(void)
{
	int value = ucc_get_value();

	if (value < 0) {
		cpumask_copy(&cur_cstate, cpu_possible_mask);
		return;
	}

	cpumask_copy(&cur_cstate, &ucc_configs[value].cstate);
}

enum {
	UCC_REMOVE_REQ,
	UCC_UPDATE_REQ,
	UCC_ADD_REQ,
};

static void list_add_priority(struct ucc_req *new, struct list_head *head)
{
	struct list_head temp;
	struct ucc_req *pivot;

	/*
	 * If new request's value is bigger than first entry,
	 * add new request to first entry.
	 */
	pivot = list_first_entry(head, struct ucc_req, list);
	if (pivot->value <= new->value) {
		list_add(&new->list, head);
		return;
	}

	/*
	 * If new request's value is smaller than last entry,
	 * add new request to last entry.
	 */
	pivot = list_last_entry(head, struct ucc_req, list);
	if (pivot->value >= new->value) {
		list_add_tail(&new->list, head);
		return;
	}

	/*
	 * Find first entry that smaller than new request.
	 * And add new request before that entry.
	 */
	list_for_each_entry(pivot, head, list) {
		if (pivot->value < new->value)
			break;
	}

	list_cut_before(&temp, head, &pivot->list);
	list_add(&new->list, head);
	list_splice(&temp, head);
}

static void ucc_update(struct ucc_req *req, int value, int action)
{
	int prev_value = ucc_get_value();

	switch (action) {
	case UCC_REMOVE_REQ:
		list_del(&req->list);
		req->active = 0;
		break;
	case UCC_UPDATE_REQ:
		list_del(&req->list);
	case UCC_ADD_REQ:
		req->value = value;
		list_add_priority(req, &ucc_req_list);
		req->active = 1;
		break;
	}

	if (prev_value != ucc_get_value())
		update_cur_cstate();
}

void ucc_add_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_ADD_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_add_request);

void ucc_update_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_UPDATE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_update_request);

void ucc_remove_request(struct ucc_req *req)
{
	unsigned long flags;

	if (!ucc_initialized)
		return;

	spin_lock_irqsave(&ucc_lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, 0, UCC_REMOVE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_remove_request);

static ssize_t ucc_requests_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ucc_req *req;
	int ret = 0;

	list_for_each_entry(req, &ucc_req_list, list)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"request : %d (%s)\n", req->value, req->name);

	return ret;
}
DEVICE_ATTR_RO(ucc_requests);

static struct ucc_req ucc_req =
{
	.name = "ufcc",
};

static int ucc_requested;
static int ucc_requested_val;

static void ucc_control(int value)
{
	ucc_update_request(&ucc_req, value);
	ucc_requested_val = value;
}

static ssize_t cstate_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ucc_requested);
}

static ssize_t cstate_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	input = !!input;
	if (input == ucc_requested)
		return count;

	ucc_requested = input;

	if (ucc_requested)
		ucc_add_request(&ucc_req, ucc_requested_val);
	else
		ucc_remove_request(&ucc_req);

	return count;
}
DEVICE_ATTR_RW(cstate_control);

static int cstate_control_level;
static ssize_t cstate_control_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d (0=CPD, 1=C2)\n", cstate_control_level);
}

static ssize_t cstate_control_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	cstate_control_level = input;

	return count;
}
DEVICE_ATTR_RW(cstate_control_level);

static struct attribute *exynos_ucc_attrs[] = {
	&dev_attr_ucc_requests.attr,
	&dev_attr_cstate_control.attr,
	&dev_attr_cstate_control_level.attr,
	NULL,
};

static struct attribute_group exynos_ucc_group = {
	.name = "ucc",
	.attrs = exynos_ucc_attrs,
};

enum {
	CPD_BLOCK,
	C2_BLOCK
};

static int ucc_cpupm_notifier(struct notifier_block *nb,
				unsigned long event, void *val)
{
	int cpu = smp_processor_id();

	if (!ucc_initialized)
		return NOTIFY_OK;

	if (event == C2_ENTER)
		if (cstate_control_level == C2_BLOCK)
			goto check;

	if (event == CPD_ENTER)
		if (cstate_control_level == CPD_BLOCK)
			goto check;

	return NOTIFY_OK;

check:
	if (!cpumask_test_cpu(cpu, &cur_cstate)) {
		/* not allow enter C2 */
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static struct notifier_block ucc_cpupm_nb = {
	.notifier_call = ucc_cpupm_notifier,
};

static int exynos_ucc_init(struct platform_device *pdev)
{
	struct device_node *dn, *child;
	int ret, i = 0;

	dn = of_find_node_by_name(pdev->dev.of_node, "ucc");
	ucc_config_count = of_get_child_count(dn);
	if (ucc_config_count == 0) {
		pr_info("There is no ucc-config in DT\n");
		return 0;
	}

	ucc_configs = kcalloc(ucc_config_count,
			sizeof(struct ucc_config), GFP_KERNEL);
	if (!ucc_configs) {
		pr_err("Failed to alloc ucc_configs\n");
		return -ENOMEM;
	}

	for_each_child_of_node(dn, child) {
		const char *mask;

		of_property_read_u32(child, "index", &ucc_configs[i].index);

		if (of_property_read_string(child, "cstate", &mask))
			cpumask_clear(&ucc_configs[i].cstate);
		else
			cpulist_parse(mask, &ucc_configs[i].cstate);

		i++;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_ucc_group);
	if (ret) {
		pr_err("Failed to create cstate_control node\n");
		kfree(ucc_configs);
		return ret;
	}

	ret = exynos_cpupm_notifier_register(&ucc_cpupm_nb);
	if (ret)
		return ret;

	cpumask_setall(&cur_cstate);
	ucc_initialized = true;

	return 0;
}

/*********************************************************************
 *                      USER FREQUENCY CONTROL                       *
 *********************************************************************/
static struct exynos_ufc {
	unsigned int		total_table_row; /* table row without validation */
	unsigned int		table_row; /* table row with validation*/

	unsigned int		table_col;
	unsigned int		lit_table_row;
	unsigned int		col_vfreq;
	unsigned int		col_big;
	unsigned int		col_mid;
	unsigned int		col_lit;
	unsigned int		col_emstune;
	unsigned int		col_over_limit;

	int			boot_domain_auto_table;

	int			max_vfreq;
	int			last_min_wo_boost_input;

			/* The priority value of vfreq input */
	int			prio_vfreq[TYPE_END];
			/* The determined max vfreq from max_strict and max*/
	int			curr_max_vfreq;

	int			strict_enabled;

	ktime_t			last_update_time;

	u32			**ufc_lit_table;
	struct list_head	ufc_domain_list;
	struct list_head	ufc_table_list;

	struct cpumask		active_cpus;
	struct mutex		lock;
} ufc = {
	.lock = __MUTEX_INITIALIZER(ufc.lock),
};

enum {
	UFC_STATUS_NORMAL = 0,
	UFC_STATUS_STRICT,
};

struct user_input {
	char user_name[20];
	int freq[TYPE_END];
	u64 residency_time[TYPE_END];
};

static struct user_input ufc_req[USER_END];
static struct emstune_mode_request ufc_emstune_req;

/*********************************************************************
 *                          HELPER FUNCTION                          *
 *********************************************************************/
static void ufc_emstune_control(int level)
{
	emstune_update_request(&ufc_emstune_req, level);
}

static char *get_user_type_string(enum ufc_user_type user)
{
	switch (user) {
	case USERSPACE:
		return "USERSPACE";
	case UFC_INPUT:
		return "UFC_INPUT";
	case FINGER:
		return "FINGER";
	case ARGOS:
		return "ARGOS";
	default:
		return NULL;
	}

	return NULL;
}

static char *get_ctrl_type_string(enum ufc_ctrl_type type)
{
	switch (type) {
	case PM_QOS_MIN_LIMIT:
		return "PM_QOS_MIN_LIMIT";
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		return "PM_QOS_MIN_LIMIT_WO_BOOST";
	case PM_QOS_MAX_LIMIT:
		return "PM_QOS_MAX_LIMIT";
	case PM_QOS_LITTLE_MAX_LIMIT:
		return "PM_QOS_LITTLE_MAX_LIMIT";
	case PM_QOS_OVER_LIMIT:
		return "PM_QOS_OVER_LIMIT";
	default:
		return NULL;
	}

	return NULL;
}

static void
disable_domain_cpus(struct ufc_domain *ufc_dom)
{
	if (!ufc_dom->allow_disable_cpus)
		return;

	cpumask_andnot(&ufc.active_cpus, &ufc.active_cpus, &ufc_dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

static void
enable_domain_cpus(struct ufc_domain *ufc_dom)
{
	if (!ufc_dom->allow_disable_cpus)
		return;

	cpumask_or(&ufc.active_cpus, &ufc.active_cpus, &ufc_dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

unsigned int get_cpufreq_max_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct cpufreq_policy *policy;
	unsigned int freq_qos_max;

	/* Big --> Mid --> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
		if (!policy)
			return -EINVAL;

		/* get value of maximum PM QoS */
		freq_qos_max = policy->max;

		if (freq_qos_max > 0) {
			freq_qos_max = min(freq_qos_max, ufc_dom->max_freq);
			freq_qos_max = max(freq_qos_max, ufc_dom->min_freq);

			return freq_qos_max;
		}
	}

	/* no maximum PM QoS */
	return 0;
}
EXPORT_SYMBOL(get_cpufreq_max_limit);

static unsigned int ufc_get_table_col(struct device_node *child)
{
	struct device_node *dn;
	int count_list = 0, ret = 0, col_idx;

	dn = of_get_child_by_name(child, "table-info");
	if (!dn)
		return -EINVAL;

	count_list = of_property_count_strings(dn, "table-order");
	if (count_list <= 0)
		return -EINVAL;

	ufc.table_col = count_list;
	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		const char *col_name;

		ret = of_property_read_string_index(dn, "table-order", col_idx, &col_name);
		if (ret < 0)
			return -EINVAL;

		if (!strcmp("vfreq", col_name))
			ufc.col_vfreq = col_idx;
		else if (!strcmp("big", col_name))
			ufc.col_big = col_idx;
		else if (!strcmp("mid", col_name))
			ufc.col_mid = col_idx;
		else if (!strcmp("lit", col_name))
			ufc.col_lit = col_idx;
		else if (!strcmp("emstune", col_name))
			ufc.col_emstune = col_idx;
		else if (!strcmp("strict", col_name))
			ufc.col_over_limit = col_idx;
	}

	return ret;
}

static unsigned int ufc_clamp_vfreq(unsigned int vfreq,
				struct ufc_table_info *table_info)
{
	unsigned int max_vfreq;
	unsigned int min_vfreq;

	max_vfreq = table_info->ufc_table[ufc.col_vfreq][0];
	min_vfreq = table_info->ufc_table[ufc.col_vfreq][ufc.table_row + ufc.lit_table_row - 1];

	if (max_vfreq < vfreq)
		return max_vfreq;
	if (min_vfreq > vfreq)
		return min_vfreq;

	return vfreq;
}

static int ufc_get_proper_min_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;
	if (table_info->ufc_table[0][index] < vfreq)
		index--;

	return index;
}

static int ufc_get_proper_max_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info)
{
	int index;

	vfreq = ufc_clamp_vfreq(vfreq, table_info);

	for (index = 0; table_info->ufc_table[0][index] > vfreq; index++)
		;

	return index;
}

static int ufc_get_proper_table_index(unsigned int vfreq,
		struct ufc_table_info *table_info, int ctrl_type)
{
	int target_idx = 0;

	switch (ctrl_type) {
	case PM_QOS_MIN_LIMIT:
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		target_idx = ufc_get_proper_min_table_index(vfreq, table_info);
		break;
	case PM_QOS_MAX_LIMIT:
	case PM_QOS_LITTLE_MAX_LIMIT:
	case PM_QOS_OVER_LIMIT:
		target_idx = ufc_get_proper_max_table_index(vfreq, table_info);
		break;
	}
	return target_idx;
}

static unsigned int
ufc_adjust_freq(unsigned int freq, struct ufc_domain *dom, int ctrl_type)
{
	if (freq > dom->max_freq)
		return dom->max_freq;

	if (freq < dom->min_freq) {
		switch(ctrl_type) {
		case PM_QOS_MIN_LIMIT:
		case PM_QOS_MIN_LIMIT_WO_BOOST:
			return dom->min_freq;
		case PM_QOS_MAX_LIMIT:
		case PM_QOS_LITTLE_MAX_LIMIT:
		case PM_QOS_OVER_LIMIT:
			if (freq == 0)
				return 0;
		return dom->min_freq;
		}
	}

	return freq;
}

static unsigned int
ufc_get_release_freq(struct ufc_domain *dom, int ctrl_type)
{
	switch(ctrl_type) {
	case PM_QOS_MIN_LIMIT:
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		return dom->min_freq;
	case PM_QOS_MAX_LIMIT:
	case PM_QOS_LITTLE_MAX_LIMIT:
	case PM_QOS_OVER_LIMIT:
		return dom->max_freq;
	return 0;
	}
	return 0;
}

static struct freq_qos_request *
ufc_get_freq_qos_req(struct ufc_domain *dom, int ctrl_type)
{
	switch(ctrl_type) {
	case PM_QOS_MIN_LIMIT:
		return &dom->user_min_qos_req;
	case PM_QOS_MIN_LIMIT_WO_BOOST:
		return &dom->user_min_qos_wo_boost_req;
	case PM_QOS_MAX_LIMIT:
	case PM_QOS_OVER_LIMIT:
		return &dom->user_max_qos_req;
	case PM_QOS_LITTLE_MAX_LIMIT:
		return &dom->user_lit_max_qos_req;
	return 0;
	}
	return 0;
}

static struct ufc_table_info *get_table_info(int ctrl_type)
{
	struct ufc_table_info *table_info;

	list_for_each_entry(table_info, &ufc.ufc_table_list, list)
		if (table_info->ctrl_type == ctrl_type)
			return table_info;

	return NULL;
}

/*********************************************************************
 *                     MULTI REQUEST MANAGEMENT                      *
 *********************************************************************/
static void ufc_update_max_limit(void);
static int ufc_update_min_limit(void);
static int ufc_update_little_max_limit(void);

static void reset_limit_stat() {
	int user, type;

	for (user = 0; user < USER_END; user++) {
		for (type = 0; type < TYPE_END; type++) {
			ufc_req[user].residency_time[type] = 0;
		}
	}
}

static void update_limit_stat(){
	int user, type, freq;
	u64 duration = ktime_to_ms(ktime_get() - ufc.last_update_time);

	ufc.last_update_time = ktime_get();

	for (user = 0; user < USER_END; user++) {
		for (type = 0; type < TYPE_END; type++) {
			if (type == PM_QOS_OVER_LIMIT)
				continue;

			freq =  ufc_req[user].freq[type];
			if (freq > 0)
				ufc_req[user].residency_time[type]
					+= duration;
		}
	}

	if (ufc.strict_enabled != UFC_STATUS_STRICT ||
		ufc.prio_vfreq[PM_QOS_MAX_LIMIT] == RELEASE ||
		ufc.prio_vfreq[PM_QOS_OVER_LIMIT] == RELEASE)
		return;

	for (user = 0; user < USER_END; user++) {
		freq = ufc_req[user].freq[PM_QOS_MIN_LIMIT];
		if (freq > 0)
			ufc_req[user].residency_time[PM_QOS_OVER_LIMIT]
				+= duration;
	}
}

int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type, s32 new_freq)
{
	s32 prev_freq;
	int status_change = 0;

	if (user < 0 || user >= USER_END || type < 0 || type >= TYPE_END)
		return -EINVAL;

	mutex_lock(&ufc.lock);

	update_limit_stat();
	prev_freq = ufc_req[user].freq[type];
	ufc_req[user].freq[type] = new_freq;

	/* Update Min Limit */
	if (type == PM_QOS_MIN_LIMIT)
		status_change = ufc_update_min_limit();

	/* Update Max Limit */
	if (type == PM_QOS_MAX_LIMIT || type == PM_QOS_OVER_LIMIT || status_change)
		ufc_update_max_limit();

	if (type == PM_QOS_LITTLE_MAX_LIMIT)
		ufc_update_little_max_limit();

	mutex_unlock(&ufc.lock);

	return 0;
}
EXPORT_SYMBOL_GPL(ufc_update_request);

/*********************************************************************
 *                          PM QOS CONTROL                           *
 *********************************************************************/
static void ufc_release_freq(enum ufc_ctrl_type type)
{
	struct ufc_domain *ufc_dom;
	struct freq_qos_request *req;
	unsigned int target_freq;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		if (type == PM_QOS_LITTLE_MAX_LIMIT
			&& ufc_dom->table_col_idx != ufc.col_lit)
			continue;

		target_freq = ufc_get_release_freq(ufc_dom, type);
		req = ufc_get_freq_qos_req(ufc_dom, type);
		if (!target_freq || !req)
			return;

		freq_qos_update_request(req, target_freq);
	}
}

static int ufc_get_min_limit(void)
{
	int user, freq, min_freq = RELEASE;

	for (user = 0; user < USER_END; user++) {
		freq = ufc_req[user].freq[PM_QOS_MIN_LIMIT];
		if (freq == RELEASE)
			continue;

		/* Max Value among min_limit requests */
		if (min_freq == RELEASE || freq > min_freq)
			min_freq = freq;
	}

	return min_freq;
}

static int ufc_update_min_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx, level;
	int prev_status;

	table_info = get_table_info(PM_QOS_MIN_LIMIT);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return -EINVAL;
	}
	prev_status = ufc.strict_enabled;

	target_freq = ufc_get_min_limit();
	ufc.prio_vfreq[PM_QOS_MIN_LIMIT] = target_freq;

	if (target_freq != RELEASE) {
		target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MIN_LIMIT);

		/* Big ----> Lit */
		list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
			unsigned int col_idx = ufc_dom->table_col_idx;

			target_freq = table_info->ufc_table[col_idx][target_idx];
			target_freq = ufc_adjust_freq(target_freq, ufc_dom, PM_QOS_MIN_LIMIT);

			freq_qos_update_request(&ufc_dom->user_min_qos_req, target_freq);
		}

		level = table_info->ufc_table[ufc.col_emstune][target_idx];

		if (table_info->ufc_table[ufc.col_over_limit][target_idx])
			ufc.strict_enabled = UFC_STATUS_STRICT;
		else
			ufc.strict_enabled = UFC_STATUS_NORMAL;
	} else {
		ufc_release_freq(PM_QOS_MIN_LIMIT);

		level = 0;

		ufc.strict_enabled = UFC_STATUS_NORMAL;
	}

	ufc_emstune_control(level);
	ucc_control(level);

	return (prev_status != ufc.strict_enabled);
}

#define MAX(A, B) ((A) > (B) ? (A) : (B))
static int ufc_determine_max_limit(void)
{
	int user, freq;
	int max_limit = RELEASE, over_limit = RELEASE;

	/* Update PM_QOS_MAX_LIMIT */
	for (user = 0; user < USER_END; user++) {
		freq = ufc_req[user].freq[PM_QOS_MAX_LIMIT];
		if (freq == RELEASE)
			continue;

		/* Min Value among max_limit requests */
		if (max_limit == RELEASE || freq < max_limit)
			max_limit = freq;
	}
	ufc.prio_vfreq[PM_QOS_MAX_LIMIT] = max_limit;

	/* Update PM_QOS_OVER_LIMIT */
	for (user = 0; user < USER_END; user++) {
		freq = ufc_req[user].freq[PM_QOS_OVER_LIMIT];
		if (freq == RELEASE)
			continue;

		/* Min Value among over_limit requests */
		if (over_limit == RELEASE || freq < over_limit)
			over_limit = freq;
	}
	ufc.prio_vfreq[PM_QOS_OVER_LIMIT] = over_limit;

	/* Determine the final max lmit */
	if (ufc.strict_enabled != UFC_STATUS_STRICT)
		return max_limit;

	/* if max_limit is released, over_limit is also ignored */
	if (max_limit == RELEASE)
		return max_limit;

	return MAX(max_limit, over_limit);
}

#define MIN(A, B) ((A) < (B) ? (A) : (B))
static int ufc_update_little_max_limit(void) {
	struct ufc_domain *ufc_dom;
	int user, target_freq = RELEASE, user_lit_max_freq;

	for (user = 0; user < USER_END; user++) {
		user_lit_max_freq = ufc_req[user].freq[PM_QOS_LITTLE_MAX_LIMIT];
		if (user_lit_max_freq == RELEASE)
			continue;

		/* Min Value among little_max_limit requests */
		if (target_freq ==  RELEASE || user_lit_max_freq < target_freq)
			target_freq = user_lit_max_freq;
	}

	ufc.prio_vfreq[PM_QOS_LITTLE_MAX_LIMIT] = target_freq;

	if (target_freq == RELEASE) {
		ufc_release_freq(PM_QOS_LITTLE_MAX_LIMIT);
		return 0;
	}

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;
		if (col_idx != ufc.col_lit)
		    continue;

		freq_qos_update_request(&ufc_dom->user_lit_max_qos_req, target_freq);
	}

	return 0;
}

static void freq_qos_release(struct work_struct *work)
{
	struct ufc_domain *ufc_dom;

	if (!ufc.prio_vfreq[PM_QOS_LITTLE_MIN_LIMIT])
		return;

	ufc.prio_vfreq[PM_QOS_LITTLE_MIN_LIMIT] = 0;
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		if (col_idx != ufc.col_lit)
			continue;

		freq_qos_update_request(&ufc_dom->hold_lit_max_qos_req, ufc_dom->hold_freq);
		freq_qos_update_request(&ufc_dom->user_lit_min_qos_req, ufc_dom->min_freq);
		break;
	}
}

static int ufc_update_little_min_limit(int target_freq)
{
	struct ufc_domain *ufc_dom;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		if (col_idx != ufc.col_lit)
			continue;

		if (target_freq) {
			freq_qos_update_request(&ufc_dom->hold_lit_max_qos_req, ufc_dom->max_freq);
			freq_qos_update_request(&ufc_dom->user_lit_min_qos_req, target_freq);

			if (!ufc_dom->little_min_timeout)
				break;

			/* set a boosting timeout */
			if (delayed_work_pending(&ufc_dom->work))
				cancel_delayed_work_sync(&ufc_dom->work);

			schedule_delayed_work(&ufc_dom->work,
				msecs_to_jiffies(ufc_dom->little_min_timeout));
		} else {
			freq_qos_update_request(&ufc_dom->hold_lit_max_qos_req, ufc_dom->hold_freq);
			freq_qos_update_request(&ufc_dom->user_lit_min_qos_req, ufc_dom->min_freq);
		}

		break;
	}

	return 0;
}

static void ufc_update_max_limit(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx;

	table_info = get_table_info(PM_QOS_MAX_LIMIT);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return;
	}

	target_freq = ufc_determine_max_limit();
	ufc.curr_max_vfreq = target_freq;

	/* If Max Limit is released, Max limit should be set as the orig max value */
	if (target_freq == RELEASE) {
		ufc_release_freq(PM_QOS_MAX_LIMIT);
		return;
	}

	target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MAX_LIMIT);

	/* Big ----> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		target_freq = table_info->ufc_table[col_idx][target_idx];
		target_freq = ufc_adjust_freq(target_freq, ufc_dom, PM_QOS_MAX_LIMIT);

		if (target_freq == 0) {
			freq_qos_update_request(&ufc_dom->user_max_qos_req, ufc_dom->min_freq);
			disable_domain_cpus(ufc_dom);
			continue;
		}

		enable_domain_cpus(ufc_dom);
		freq_qos_update_request(&ufc_dom->user_max_qos_req, target_freq);
	}
}

static void ufc_update_min_limit_wo_boost(void)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	int target_freq, target_idx;

	table_info = get_table_info(PM_QOS_MIN_LIMIT);
	if (!table_info) {
		pr_err("failed to find target table\n");
		return;
	}

	target_freq = ufc.last_min_wo_boost_input;

	/* clear min limit */
	if (target_freq == RELEASE) {
		ufc_release_freq(PM_QOS_MIN_LIMIT_WO_BOOST);
		return;
	}

	target_idx = ufc_get_proper_table_index(target_freq,
				table_info, PM_QOS_MIN_LIMIT_WO_BOOST);

	/* Big ----> Lit */
	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		unsigned int col_idx = ufc_dom->table_col_idx;

		target_freq = table_info->ufc_table[col_idx][target_idx];
		target_freq = ufc_adjust_freq(target_freq, ufc_dom,
						PM_QOS_MIN_LIMIT_WO_BOOST);

		freq_qos_update_request(&ufc_dom->user_min_qos_wo_boost_req,
						target_freq);
	}
}

/*
 * sysfs function
 */
static ssize_t cpufreq_table_show(struct kobject *kobj, char *buf)
{
	struct ufc_table_info *table_info;
	ssize_t count = 0;
	int r_idx;

	if (list_empty(&ufc.ufc_table_list))
		return count;

	table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++)
		count += snprintf(&buf[count], 10, "%d ",
				table_info->ufc_table[0][r_idx]);
	count += snprintf(&buf[count], 10, "\n");

	return count;
}

static ssize_t cpufreq_max_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.prio_vfreq[PM_QOS_MAX_LIMIT]);
}

static ssize_t cpufreq_max_limit_store(struct kobject *kobj,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;
	ufc_update_request(USERSPACE, PM_QOS_MAX_LIMIT, input);

	return count;
}

static ssize_t cpufreq_min_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.prio_vfreq[PM_QOS_MIN_LIMIT]);
}

static ssize_t limit_stat_store(struct kobject *kobj,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	mutex_lock(&ufc.lock);
	reset_limit_stat();
	mutex_unlock(&ufc.lock);
	return count;
}


static ssize_t limit_stat_show(struct kobject *kobj, char *buf)
{
	ssize_t count = 0;
	int user, type;

	mutex_lock(&ufc.lock);
	update_limit_stat();

	for (user = 0; user < USER_END; user++) {
		for (type = 0; type < TYPE_END; type++) {
			if (type == PM_QOS_MIN_LIMIT_WO_BOOST)
				continue;

			count += snprintf(&buf[count], 10, "%llu ",
					ufc_req[user].residency_time[type]);
		}
		count += snprintf(&buf[count], 10, "\n");
	}
	mutex_unlock(&ufc.lock);

	return count;
}

static ssize_t cpufreq_min_limit_store(struct kobject *kobj,
				const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_MIN_LIMIT, input);

	return count;
}

static ssize_t cpufreq_min_limit_wo_boost_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.last_min_wo_boost_input);
}

static ssize_t cpufreq_min_limit_wo_boost_store(struct kobject *kobj,
		                                        const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.last_min_wo_boost_input = input;
	ufc_update_min_limit_wo_boost();

	return count;
}

static ssize_t little_max_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.prio_vfreq[PM_QOS_LITTLE_MAX_LIMIT]);
}

static ssize_t little_max_limit_store(struct kobject *kobj, const char *buf,
								size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_LITTLE_MAX_LIMIT, input);

	return count;
}

static ssize_t little_min_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.prio_vfreq[PM_QOS_LITTLE_MIN_LIMIT]);
}

static ssize_t little_min_limit_store(struct kobject *kobj, const char *buf,
								size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.prio_vfreq[PM_QOS_LITTLE_MIN_LIMIT] = input;
	ufc_update_little_min_limit(input);
	return count;
}


static ssize_t over_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.prio_vfreq[PM_QOS_OVER_LIMIT]);
}

static ssize_t over_limit_store(struct kobject *kobj, const char *buf,
								size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.prio_vfreq[PM_QOS_OVER_LIMIT] = input;
	ufc_update_request(USERSPACE, PM_QOS_OVER_LIMIT, input);

	return count;
}

static int __debug_table_show(int ctrl_type, char *buf)
{
	struct ufc_table_info *table_info;
	int count = 0;
	int c_idx, r_idx;

	if (list_empty(&ufc.ufc_table_list))
		return 0;

	table_info = get_table_info(ctrl_type);
	if (!table_info)
		return -ENODEV;

	count += snprintf(buf + count, PAGE_SIZE - count, "It's only for debug\n");
	count += snprintf(buf + count, PAGE_SIZE - count, "Table Ctrl Type: %s(%d)\n",
					get_ctrl_type_string(table_info->ctrl_type),
					table_info->ctrl_type);

	for (r_idx = 0; r_idx < (ufc.table_row + ufc.lit_table_row); r_idx++) {
		for (c_idx = 0; c_idx < ufc.table_col; c_idx++) {
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					table_info->ufc_table[c_idx][r_idx]);
		}
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}

static ssize_t debug_max_table_show(struct kobject *kobj, char *buf)
{
	return __debug_table_show(PM_QOS_MAX_LIMIT, buf);
}

static ssize_t debug_min_table_show(struct kobject *kobj, char *buf)
{
	return __debug_table_show(PM_QOS_MIN_LIMIT, buf);
}

static ssize_t info_show(struct kobject *kobj, char *buf)
{
	ssize_t count = 0;
	int user;

	for (user = 0; user < USER_END; user++)
		count += snprintf(&buf[count], 10, "%d\n",
					ufc_req[user].freq[PM_QOS_MIN_LIMIT]);

	return count;
}

static ssize_t debug_table_col_show(struct kobject *kobj, char *buf)
{
	int ret = 0;

	ret += sprintf(buf + ret, "tatal_col:%d vfreq:%d lit:%d mid:%d big:%d emstune:%d over_limit:%d ",
			ufc.table_col, ufc.col_vfreq, ufc.col_big, ufc.col_mid, ufc.col_lit,
			ufc.col_emstune, ufc.col_over_limit);

	return ret;
}

struct ufc_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t);
};

#define UFC_ATTR_RO(name) \
static struct ufc_attr attr_##name \
= __ATTR(name, 0444, name##_show, NULL)

#define UFC_ATTR_RW(name) \
static struct ufc_attr attr_##name \
= __ATTR(name, 0644, name##_show, name##_store)

UFC_ATTR_RO(cpufreq_table);
UFC_ATTR_RW(limit_stat);
UFC_ATTR_RW(cpufreq_min_limit);
UFC_ATTR_RW(cpufreq_min_limit_wo_boost);
UFC_ATTR_RW(cpufreq_max_limit);
UFC_ATTR_RW(over_limit);
UFC_ATTR_RW(little_max_limit);
UFC_ATTR_RW(little_min_limit);
UFC_ATTR_RO(debug_max_table);
UFC_ATTR_RO(debug_min_table);
UFC_ATTR_RO(info);
UFC_ATTR_RO(debug_table_col);

static struct attribute *exynos_ufc_attrs[] = {
	&attr_cpufreq_table.attr,
	&attr_cpufreq_min_limit.attr,
	&attr_cpufreq_min_limit_wo_boost.attr,
	&attr_cpufreq_max_limit.attr,
	&attr_over_limit.attr,
	&attr_little_max_limit.attr,
	&attr_little_min_limit.attr,
	&attr_limit_stat.attr,
	&attr_debug_max_table.attr,
	&attr_debug_min_table.attr,
	&attr_info.attr,
	&attr_debug_table_col.attr,
	NULL,
};

#define attr_to_ufcattr(a) container_of(a, struct ufc_attr, attr)

static ssize_t ufc_sysfs_show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct ufc_attr *ufcattr = attr_to_ufcattr(at);
	int ret = 0;

	if (ufcattr->show)
		ret =  ufcattr->show(kobj, buf);

	return ret;
}

static ssize_t ufc_sysfs_store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct ufc_attr *ufcattr = attr_to_ufcattr(at);
	int ret = 0;

	if (ufcattr->show)
		ret = ufcattr->store(kobj, buf, count);

	return ret;
}

static const struct sysfs_ops ufc_sysfs_ops = {
	.show	= ufc_sysfs_show,
	.store	= ufc_sysfs_store,
};

static struct kobj_type ktype_ufc = {
	.sysfs_ops	= &ufc_sysfs_ops,
	.default_attrs	= exynos_ufc_attrs,
};

struct kobject ufc_kobj;

/*********************************************************************
 *                           INIT FUNCTION                           *
 *********************************************************************/
static void ufc_free_all(void)
{
	struct ufc_table_info *table_info;
	struct ufc_domain *ufc_dom;
	int col_idx;

	pr_err("Failed: can't initialize ufc table and domain");

	while(!list_empty(&ufc.ufc_table_list)) {
		table_info = list_first_entry(&ufc.ufc_table_list,
				struct ufc_table_info, list);
		list_del(&table_info->list);

		for (col_idx = 0; table_info->ufc_table[col_idx]; col_idx++)
			kfree(table_info->ufc_table[col_idx]);

		kfree(table_info->ufc_table);
		kfree(table_info);
	}

	while(!list_empty(&ufc.ufc_domain_list)) {
		ufc_dom = list_first_entry(&ufc.ufc_domain_list,
				struct ufc_domain, list);
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
	}
}

static void ufc_init_user(void)
{
	int user, type;

	for (user = 0; user < USER_END; user++) {
		strcpy(ufc_req[user].user_name, get_user_type_string(user));
		for (type = 0 ; type < TYPE_END; type++) {
			ufc_req[user].freq[type] = RELEASE;
			ufc_req[user].residency_time[type] = 0;
		}
	}
}

static int ufc_init_freq_qos(void)
{
	struct cpufreq_policy *policy;
	struct ufc_domain *ufc_dom;

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
		if (!policy)
			return -EINVAL;

		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_min_qos_req,
				FREQ_QOS_MIN, policy->cpuinfo.min_freq);
		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_max_qos_req,
				FREQ_QOS_MAX, policy->cpuinfo.max_freq);
		freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_min_qos_wo_boost_req,
				FREQ_QOS_MIN, policy->cpuinfo.min_freq);
		if (ufc_dom->table_col_idx == ufc.col_lit) {
			freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_lit_min_qos_req,
				FREQ_QOS_MIN, policy->cpuinfo.min_freq);
			freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->user_lit_max_qos_req,
				FREQ_QOS_MAX, policy->cpuinfo.max_freq);
			freq_qos_tracer_add_request(&policy->constraints, &ufc_dom->hold_lit_max_qos_req,
				FREQ_QOS_MAX, ufc_dom->hold_freq);
		}
	}
	/* Success */
	return 0;
}

static int ufc_init_sysfs(struct kobject *kobj)
{
	int ret = 0;
	/* /sys/devices/platform/exynos-ufcc/ufc/  */
	if (kobject_init_and_add(&ufc_kobj, &ktype_ufc,
				 kobj, "ufc"))
		pr_err("Failed to init exynos ufc\n");

	/* /sys/devices/system/cpu/cpufreq_limit */
	if (sysfs_create_link(&cpu_subsys.dev_root->kobj, &ufc_kobj, "cpufreq_limit"))
		pr_err("Failed to link UFC sysfs to cpuctrl \n");

	return ret;
}

static int ufc_parse_init_table(struct ufc_table_info *ufc_info,
		struct cpufreq_policy *policy, u32 *freq_table, u32 *check_valid_row)
{
	int col_idx, row_idx, invalid_row = 0, row_backup;
	struct cpufreq_frequency_table *pos;

	for (row_idx = 0; row_idx < ufc.total_table_row; row_idx++) {
		for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
			if (!check_valid_row[row_idx]) {
				invalid_row++;
				break;
			}

			ufc_info->ufc_table[col_idx][row_idx - invalid_row]
				= freq_table[(ufc.table_col * row_idx) + col_idx];
		}
	}

	if (!policy)
		return 0;

	cpufreq_for_each_entry(pos, policy->freq_table) {
		if (pos->frequency > policy->max)
			continue;
	}

	row_backup = (row_idx - invalid_row) + ufc.lit_table_row - 1;
	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		row_idx = row_backup;
		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency > policy->max)
				continue;

#define SCALE_SIZE 8
			if (col_idx == ufc.col_vfreq)
				ufc_info->ufc_table[col_idx][row_idx--]
						= pos->frequency / SCALE_SIZE;
			else if (col_idx == ufc.col_lit)
				ufc_info->ufc_table[col_idx][row_idx--]
						= pos->frequency;
			else
				ufc_info->ufc_table[col_idx][row_idx--] = 0;
		}

		ufc.max_vfreq = ufc_info->ufc_table[ufc.col_vfreq][0];
	}

	/* Success */
	return 0;
}

static struct cpufreq_policy *bootcpu_policy;
static int init_ufc_table(struct device_node *dn)
{
	struct ufc_domain *ufc_dom;
	struct ufc_table_info *table_info;
	u32 *check_valid_row, *table_wo_validation;
	int col_idx = 0, row_idx, size, loc, ret = 0;
	unsigned int max_freq, min_freq, freq, valid_row = 0;

	table_info = kzalloc(sizeof(struct ufc_table_info), GFP_KERNEL);
	if (!table_info)
		return -ENOMEM;

	list_add_tail(&table_info->list, &ufc.ufc_table_list);

	if (of_property_read_u32(dn, "ctrl-type", &table_info->ctrl_type))
		return -EINVAL;

	size = of_property_count_u32_elems(dn, "table");
	if (size < 0)
		return size;

	table_wo_validation = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!table_wo_validation)
		return -ENOMEM;

	if (of_property_read_u32_array(dn, "table", table_wo_validation, size)) {
		ret = -EINVAL;
		goto out_table;
	}

	check_valid_row = kzalloc(sizeof(u32) * (size / ufc.table_col), GFP_KERNEL);
	if (!check_valid_row) {
		ret = -ENOMEM;
		goto out_table;
	}

	list_for_each_entry(ufc_dom, &ufc.ufc_domain_list, list) {
		max_freq = ufc_dom->max_freq;
		min_freq = ufc_dom->min_freq;

		col_idx++;
		for (row_idx = 0; row_idx < ufc.total_table_row; row_idx++) {
			loc = (ufc.table_col * row_idx) + col_idx;
			freq = table_wo_validation[loc];

			if (freq > max_freq)
				table_wo_validation[loc] = max_freq;
			else if (freq < min_freq)
				table_wo_validation[loc] = min_freq;
			else if (col_idx != ufc.col_lit)
				check_valid_row[row_idx]++;
		}
	}

	for (row_idx = 0; row_idx < ufc.total_table_row; row_idx++) {
		if (check_valid_row[row_idx] > 0)
			valid_row++;
	}
	ufc.table_row = valid_row;

	table_info->ufc_table = kzalloc(sizeof(u32 *) * ufc.table_col, GFP_KERNEL);
	if (!table_info->ufc_table) {
		ret = -ENOMEM;
		goto out_all;
	}

	for (col_idx = 0; col_idx < ufc.table_col; col_idx++) {
		table_info->ufc_table[col_idx] = kzalloc(sizeof(u32) *
				(ufc.table_row + ufc.lit_table_row + 1), GFP_KERNEL);

		if (!table_info->ufc_table[col_idx]) {
			ret = -ENOMEM;
			goto out_all;
		}
	}

	ufc_parse_init_table(table_info, bootcpu_policy, table_wo_validation, check_valid_row);

out_all:
	kfree(check_valid_row);
out_table:
	kfree(table_wo_validation);

	return ret;
}

static int init_ufc_domain(struct device_node *dn)
{
	struct ufc_domain *ufc_dom;
	struct cpufreq_policy *policy;
	const char *buf;

	ufc_dom = kzalloc(sizeof(struct ufc_domain), GFP_KERNEL);
	if (!ufc_dom)
		return -ENOMEM;

	list_add(&ufc_dom->list, &ufc.ufc_domain_list);

	if (of_property_read_string(dn, "shared-cpus", &buf))
		return -EINVAL;

	cpulist_parse(buf, &ufc_dom->cpus);
	cpumask_and(&ufc_dom->cpus, &ufc_dom->cpus, cpu_online_mask);
	if (cpumask_empty(&ufc_dom->cpus)) {
		list_del(&ufc_dom->list);
		kfree(ufc_dom);
		return 0;
	}

	if (of_property_read_u32(dn, "table-col-idx", &ufc_dom->table_col_idx))
		return -EINVAL;

	policy = cpufreq_cpu_get(cpumask_first(&ufc_dom->cpus));
	if (!policy)
		return -EINVAL;

	of_property_read_u32(dn, "hold-freq", &ufc_dom->hold_freq);
	if (!ufc_dom->hold_freq)
		ufc_dom->hold_freq = policy->cpuinfo.max_freq;

	ufc_dom->min_freq = policy->cpuinfo.min_freq;
	ufc_dom->max_freq = policy->cpuinfo.max_freq;

	of_property_read_u32(dn, "little-min-timeout", &ufc_dom->little_min_timeout);
	if (ufc_dom->little_min_timeout)
		INIT_DELAYED_WORK(&ufc_dom->work, freq_qos_release);

#define BOOTCPU 0
	if (cpumask_test_cpu(BOOTCPU, &ufc_dom->cpus)
				&& ufc.boot_domain_auto_table) {
		struct cpufreq_frequency_table *pos;
		int lit_row = 0;

		bootcpu_policy = policy;

		cpufreq_for_each_entry(pos, policy->freq_table) {
			if (pos->frequency > policy->max)
				continue;

			lit_row++;
		}

		ufc.lit_table_row = lit_row;
	}
	cpufreq_cpu_put(policy);

	if (of_property_read_bool(dn, "allow-disable-cpus"))
		ufc_dom->allow_disable_cpus = 1;

	/* Success */
	return 0;
}

static int init_ufc(struct device_node *dn)
{
	struct device_node *table_node;
	int size, type, ret = 0;

	INIT_LIST_HEAD(&ufc.ufc_domain_list);
	INIT_LIST_HEAD(&ufc.ufc_table_list);

	ret = ufc_get_table_col(dn);
	if (ret != 0)
		return -EINVAL;

	table_node = of_find_node_by_type(dn, "ufc-table");
	size = of_property_count_u32_elems(table_node, "table");
	ufc.total_table_row = size / ufc.table_col;

	if (of_property_read_bool(dn, "boot-domain-auto-table"))
		ufc.boot_domain_auto_table = 1;

	ufc.last_min_wo_boost_input = RELEASE;

	for (type = 0; type < TYPE_END; type++)
		ufc.prio_vfreq[type] = RELEASE;
	ufc.curr_max_vfreq = RELEASE;
	ufc.strict_enabled = UFC_STATUS_NORMAL;

	emstune_add_request(&ufc_emstune_req);

	return ret;
}

static int exynos_ufc_init(struct platform_device *pdev)
{
	int cpu;
	struct device_node *dn;

	dn = of_find_node_by_name(pdev->dev.of_node, "ufc");

	/* check whether cpufreq is registered or not */
	for_each_online_cpu(cpu) {
		if (!cpufreq_cpu_get(0))
			return 0;
	}

	if (init_ufc(dn)) {
		pr_err("exynos-ufc: Failed to init init_ufc\n");
		return 0;
	}

	dn = NULL;
	while((dn = of_find_node_by_type(dn, "ufc-domain"))) {
		if (init_ufc_domain(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_domain\n");
			ufc_free_all();
			return 0;
		}
	}

	dn = NULL;
	while((dn = of_find_node_by_type(dn, "ufc-table"))) {
		if (init_ufc_table(dn)) {
			pr_err("exynos-ufc: Failed to init init_ufc_table\n");
			ufc_free_all();
			return 0;
		}
	}

	if (ufc_init_sysfs(&pdev->dev.kobj)){
		pr_err("exynos-ufc: Failed to init sysfs\n");
		ufc_free_all();
		return 0;
	}

	if (ufc_init_freq_qos()) {
		pr_err("exynos-ufc: Failed to init freq_qos\n");
		ufc_free_all();
		return 0;
	}

	cpumask_setall(&ufc.active_cpus);
	if (ecs_request_register("UFC", &ufc.active_cpus)) {
		pr_err("exynos-ufc: Failed to register ecs\n");
		ufc_free_all();
		return 0;
	}

	ufc_init_user();

	pr_info("exynos-ufc: Complete UFC driver initialization\n");

	return 0;
}

/*********************************************************************
 *                       DRIVER INITIALIZATION                       *
 *********************************************************************/
static int exynos_ufcc_probe(struct platform_device *pdev)
{
	exynos_ucc_init(pdev);
	exynos_ufc_init(pdev);

	return 0;
}

static const struct of_device_id of_exynos_ufcc_match[] = {
	{ .compatible = "samsung,exynos-ufcc", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_ufcc_match);

static struct platform_driver exynos_ufcc_driver = {
	.driver = {
		.name	= "exynos-ufcc",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_ufcc_match,
	},
	.probe		= exynos_ufcc_probe,
};

static int __init exynos_ufcc_init(void)
{
	return platform_driver_register(&exynos_ufcc_driver);
}
late_initcall(exynos_ufcc_init);

static void __exit exynos_ufcc_exit(void)
{
	platform_driver_unregister(&exynos_ufcc_driver);
}
module_exit(exynos_ufcc_exit);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos UFCC driver");
MODULE_LICENSE("GPL");
