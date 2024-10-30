/*
 * Frequency variant cpufreq driver
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/reciprocal_div.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

/******************************************************************************
 * emstune mode/level/set management                                          *
 ******************************************************************************/
struct {
	struct emstune_mode *modes;
	struct emstune_mode *modes_backup;

	int cur_mode;
	int cur_level;

	int mode_count;
	int level_count;

	int boost_level;

	struct {
		int ongoing;
		struct emstune_set set;
		struct delayed_work work;
	} boot;

	struct device			*ems_dev;
	struct notifier_block		nb;
	struct emstune_mode_request	level_req;
} emstune;

static inline struct emstune_set *emstune_cur_set(void)
{
	if (!emstune.modes)
		return NULL;

	if (emstune.boot.ongoing)
		return &emstune.boot.set;

	return &emstune.modes[emstune.cur_mode].sets[emstune.cur_level];
}

static int emstune_notify(void);

static DEFINE_SPINLOCK(emstune_lock);

static void emstune_mode_change(int next_mode)
{
	unsigned long flags;
	char msg[32];

	spin_lock_irqsave(&emstune_lock, flags);

	if (emstune.cur_mode == next_mode) {
		spin_unlock_irqrestore(&emstune_lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_mode = next_mode;
	emstune_notify();

	spin_unlock_irqrestore(&emstune_lock, flags);

	snprintf(msg, sizeof(msg), "NOTI_MODE=%d", emstune.cur_mode);
	send_uevent(msg);
}

static void emstune_level_change(int next_level)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);

	if (emstune.cur_level == next_level) {
		spin_unlock_irqrestore(&emstune_lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_level = next_level;
	emstune_notify();

	spin_unlock_irqrestore(&emstune_lock, flags);
}

static void emstune_reset(int mode, int level)
{
	memcpy(&emstune.modes[mode].sets[level],
	       &emstune.modes_backup[mode].sets[level],
	       sizeof(struct emstune_set));
}

/******************************************************************************
 * emstune level request                                                      *
 ******************************************************************************/
static int emstune_pm_qos_notifier(struct notifier_block *nb,
				unsigned long level, void *data)
{
	emstune_level_change(level);

	return 0;
}

#define DEFAULT_LEVEL	(0)
void __emstune_add_request(struct emstune_mode_request *req,
					char *func, unsigned int line)
{
	if (dev_pm_qos_add_request(emstune.ems_dev, &req->dev_req,
			DEV_PM_QOS_MIN_FREQUENCY, DEFAULT_LEVEL))
		return;

	req->func = func;
	req->line = line;
}
EXPORT_SYMBOL_GPL(__emstune_add_request);

void emstune_remove_request(struct emstune_mode_request *req)
{
	dev_pm_qos_remove_request(&req->dev_req);
}
EXPORT_SYMBOL_GPL(emstune_remove_request);

void emstune_update_request(struct emstune_mode_request *req, s32 new_level)
{
	/* ignore if the value is out of range except boost level */
	if ((new_level < 0 || new_level >= emstune.level_count))
		return;

	dev_pm_qos_update_request(&req->dev_req, new_level);
}
EXPORT_SYMBOL_GPL(emstune_update_request);

void emstune_boost(struct emstune_mode_request *req, int enable)
{
	if (enable)
		emstune_update_request(req, emstune.boost_level);
	else
		emstune_update_request(req, 0);
}
EXPORT_SYMBOL_GPL(emstune_boost);

int emstune_get_cur_mode(void)
{
	return emstune.cur_mode;
}
EXPORT_SYMBOL_GPL(emstune_get_cur_mode);

int emstune_get_cur_level(void)
{
	return emstune.cur_level;
}
EXPORT_SYMBOL_GPL(emstune_get_cur_level);

/******************************************************************************
 * emstune notify                                                             *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(emstune_chain);

/* emstune_lock *MUST* be held before notifying */
static int emstune_notify(void)
{
	struct emstune_set *cur_set = emstune_cur_set();

	if (!cur_set)
		return 0;

	return raw_notifier_call_chain(&emstune_chain, 0, cur_set);
}

int emstune_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&emstune_chain, nb);
}
EXPORT_SYMBOL_GPL(emstune_register_notifier);

int emstune_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&emstune_chain, nb);
}
EXPORT_SYMBOL_GPL(emstune_unregister_notifier);

/******************************************************************************
 * initializing and parsing parameter                                         *
 ******************************************************************************/
static char *task_cgroup_name[] = {
	"root",
	"foreground",
	"background",
	"top-app",
	"rt",
	"system",
	"system-background",
	"nnapi-hal",
	"camera-daemon",
	"midground",
};

static inline void
parse_coregroup_field(struct device_node *dn, const char *name,
				int (field)[VENDOR_NR_CPUS],
				int default_value)
{
	int count, cpu, cursor;
	u32 val[VENDOR_NR_CPUS];

	for (count = 0; count < VENDOR_NR_CPUS; count++)
		val[count] = default_value;

	count = of_property_count_u32_elems(dn, name);
	if (count < 0)
		goto skip_parse_value;

	if (of_property_read_u32_array(dn, name, (u32 *)&val, count))
		goto skip_parse_value;

skip_parse_value:
	count = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			field[cursor] = val[count];

		count++;
	}
}

static inline void
parse_cgroup_field(struct device_node *dn, int *field, int default_value)
{
	int group, val;

	for (group = 0; group < CGROUP_COUNT; group++) {
		if (!of_property_read_u32(dn, task_cgroup_name[group], &val))
			field[group] = val;
		else
			field[group] = default_value;
	}
}

static inline int
parse_cgroup_field_mask(struct device_node *dn, struct cpumask *mask)
{
	int group;
	const char *buf;

	for (group = 0; group < CGROUP_COUNT; group++) {
		if (!of_property_read_string(dn, task_cgroup_name[group], &buf))
			cpulist_parse(buf, &mask[group]);
		else {
			/* if it failed to parse mask, set mask all */
			cpumask_copy(&mask[group], cpu_possible_mask);
		}
	}

	return 0;
}

static inline void
parse_cgroup_coregroup_field(struct device_node *dn,
			int (field)[CGROUP_COUNT][VENDOR_NR_CPUS],
			int default_value)
{
	int group;

	for (group = 0; group < CGROUP_COUNT; group++)
		parse_coregroup_field(dn,
			task_cgroup_name[group], field[group], default_value);
}

/* energy table */
static void parse_specific_energy_table(struct device_node *dn,
			       struct emstune_specific_energy_table *specific_et)
{
	parse_coregroup_field(dn, "capacity-mips-mhzs", specific_et->mips_mhz, 0);
	parse_coregroup_field(dn, "dynamic-power-coeffs", specific_et->dynamic_coeff, 0);
}

/* sched policy */
static void parse_sched_policy(struct device_node *dn,
			struct emstune_sched_policy *sched_policy)
{
	parse_cgroup_field(dn, sched_policy->policy, 0);
}

/* active weight */
static void parse_active_weight(struct device_node *dn,
			struct emstune_active_weight *active_weight)
{
	parse_cgroup_coregroup_field(dn, active_weight->ratio, 100);
}

/* idle weight */
static void parse_idle_weight(struct device_node *dn,
			struct emstune_idle_weight *idle_weight)
{
	parse_cgroup_coregroup_field(dn, idle_weight->ratio, 100);
}

/* freqboost */
static void parse_freqboost(struct device_node *dn,
			struct emstune_freqboost *freqboost)
{
	parse_cgroup_coregroup_field(dn, freqboost->ratio, 0);
}

/* cpufreq governor parameters */
static void parse_cpufreq_gov(struct device_node *dn,
			      struct emstune_cpufreq_gov *cpufreq_gov)
{
	parse_coregroup_field(dn, "htask-boost", cpufreq_gov->htask_boost, 100);
	parse_coregroup_field(dn, "pelt-boost", cpufreq_gov->pelt_boost, 0);

	parse_coregroup_field(dn, "pelt-margin",
			      cpufreq_gov->split_pelt_margin, 25);
	parse_coregroup_field(dn, "pelt-margin-freq",
			      cpufreq_gov->split_pelt_margin_freq, INT_MAX);

	parse_coregroup_field(dn, "up-rate-limit-ms",
			      cpufreq_gov->split_up_rate_limit, 4);
	parse_coregroup_field(dn, "up-rate-limit-freq",
			      cpufreq_gov->split_up_rate_limit_freq, INT_MAX);

	if (of_property_read_u32(dn, "down-rate-limit-ms", &cpufreq_gov->down_rate_limit))
		cpufreq_gov->down_rate_limit = 4; /* 4ms */

	of_property_read_u32(dn, "rapid-scale-up", &cpufreq_gov->rapid_scale_up);
	of_property_read_u32(dn, "rapid-scale-down", &cpufreq_gov->rapid_scale_down);
	parse_coregroup_field(dn, "dis-buck-share", cpufreq_gov->dis_buck_share, 0);
}

static void parse_cpuidle_gov(struct device_node *dn,
			      struct emstune_cpuidle_gov *cpuidle_gov)
{
	parse_coregroup_field(dn, "expired-ratio", cpuidle_gov->expired_ratio, 100);
}

/* ontime migration */
#define UPPER_BOUNDARY	0
#define LOWER_BOUNDARY	1

static void __update_ontime_domain(struct ontime_dom *dom,
				struct list_head *dom_list)
{
	/*
	 * At EMS initializing time, capacity_cpu_orig() cannot be used
	 * because it returns capacity that does not reflect frequency.
	 * So, use et_max_cap() instead of capacity_cpu_orig().
	 */
	unsigned long max_cap = et_max_cap(cpumask_any(&dom->cpus));

	if (dom->upper_ratio < 0)
		dom->upper_boundary = 1024;
	else if (dom == list_last_entry(dom_list, struct ontime_dom, node)) {
		/* Upper boundary of last domain is meaningless */
		dom->upper_boundary = 1024;
	} else
		dom->upper_boundary = max_cap * dom->upper_ratio / 100;

	if (dom->lower_ratio < 0)
		dom->lower_boundary = 0;
	else if (dom == list_first_entry(dom_list, struct ontime_dom, node)) {
		/* Lower boundary of first domain is meaningless */
		dom->lower_boundary = 0;
	} else
		dom->lower_boundary = max_cap * dom->lower_ratio / 100;
}

static void update_ontime_domain(struct list_head *dom_list, int cpu,
				unsigned long ratio, int type)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, dom_list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			if (type == UPPER_BOUNDARY)
				dom->upper_ratio = ratio;
			else
				dom->lower_ratio = ratio;

			__update_ontime_domain(dom, dom_list);
		}
	}
}

void emstune_ontime_init(void)
{
	int mode, level;

	for (mode = 0; mode < emstune.mode_count; mode++) {
		for (level = 0; level < emstune.level_count; level++) {
			struct emstune_set *set;
			struct ontime_dom *dom;

			set = &emstune.modes[mode].sets[level];
			list_for_each_entry(dom, &set->ontime.dom_list, node)
				__update_ontime_domain(dom,
						&set->ontime.dom_list);
		}
	}
}

static int init_dom_list(struct device_node *dn,
				struct list_head *dom_list)
{
	struct device_node *dom_dn;

	for_each_child_of_node(dn, dom_dn) {
		struct ontime_dom *dom;
		const char *buf;

		dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
		if (!dom)
			return -ENOMEM;

		if (of_property_read_string(dom_dn, "cpus", &buf)) {
			kfree(dom);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &dom->cpus);

		if (of_property_read_u32(dom_dn, "upper-boundary",
						&dom->upper_ratio))
			dom->upper_ratio = -1;

		if (of_property_read_u32(dom_dn, "lower-boundary",
						&dom->lower_ratio))
			dom->lower_ratio = -1;

		/*
		 * Init boundary as default.
		 * Actual boundary value is set in cpufreq policy notifier.
		 */
		dom->upper_boundary = 1024;
		dom->lower_boundary = 0;

		list_add_tail(&dom->node, dom_list);
	}

	return 0;
}

static void parse_ontime(struct device_node *dn, struct emstune_ontime *ontime)
{
	parse_cgroup_field(dn, ontime->enabled, 0);

	INIT_LIST_HEAD(&ontime->dom_list);
	init_dom_list(dn, &ontime->dom_list);

	ontime->p_dom_list = &ontime->dom_list;
}

/* cpus binding */
static void parse_cpus_binding(struct device_node *dn,
			struct emstune_cpus_binding *cpus_binding)
{
	if (of_property_read_u32(dn, "target-sched-class",
			(unsigned int *)&cpus_binding->target_sched_class))
		cpus_binding->target_sched_class = 0;

	parse_cgroup_field_mask(dn, cpus_binding->mask);
}

/* task express */
static void parse_tex(struct device_node *dn, struct emstune_tex *tex)
{
	parse_cgroup_field(dn, tex->enabled, 0);

	of_property_read_u32(dn, "prio", &tex->prio);
}

/* Fluid RT scheduling */
static void parse_frt(struct device_node *dn, struct emstune_frt *frt)
{
	parse_coregroup_field(dn, "active-ratio", frt->active_ratio, 100);
}

/* CPU Sparing */
static void update_ecs_threshold_ratio(struct list_head *domain_list,
					int id, int ratio)
{
	struct ecs_domain *domain;

	list_for_each_entry(domain, domain_list, node)
		if (domain->id == id)
			domain->busy_threshold_ratio = ratio;
}

static int init_ecs_domain_list(struct device_node *ecs_dn,
		struct list_head *domain_list)
{
	struct device_node *domain_dn;
	int id = 0;

	INIT_LIST_HEAD(domain_list);

	for_each_child_of_node(ecs_dn, domain_dn) {
		struct ecs_domain *domain;

		domain = kzalloc(sizeof(struct ecs_domain), GFP_KERNEL);
		if (!domain)
			return -ENOMEM;

		if (of_property_read_u32(domain_dn, "busy-threshold-ratio",
					(u32 *)&domain->busy_threshold_ratio))
			domain->busy_threshold_ratio = 100;

		domain->id = id++;
		list_add_tail(&domain->node, domain_list);
	}

	return 0;
}

static void parse_ecs(struct device_node *dn, struct emstune_ecs *ecs)
{
	INIT_LIST_HEAD(&ecs->domain_list);
	init_ecs_domain_list(dn, &ecs->domain_list);
}

static void parse_ecs_dynamic(struct device_node *dn, struct emstune_ecs_dynamic *dynamic)
{
	parse_coregroup_field(dn, "dynamic-busy-ratio", dynamic->dynamic_busy_ratio, 0);
}

/* Margin applied to newly created task's utilization */
static void parse_ntu(struct device_node *dn,
				struct emstune_ntu *ntu)
{
	parse_cgroup_field(dn, ntu->ratio, 100);
}

/* fclamp */
static void parse_fclamp(struct device_node *dn, struct emstune_fclamp *fclamp)
{
	parse_coregroup_field(dn, "min-freq", fclamp->min_freq, 0);
	parse_coregroup_field(dn, "min-target-period", fclamp->min_target_period, 0);
	parse_coregroup_field(dn, "min-target-ratio", fclamp->min_target_ratio, 0);

	parse_coregroup_field(dn, "max-freq", fclamp->max_freq, 0);
	parse_coregroup_field(dn, "max-target-period", fclamp->max_target_period, 0);
	parse_coregroup_field(dn, "max-target-ratio", fclamp->max_target_ratio, 0);

	parse_cgroup_field(of_get_child_by_name(dn, "monitor-group"),
						fclamp->monitor_group, 0);
}

/* support uclamp */
static void parse_support_uclamp(struct device_node *dn,
		struct emstune_support_uclamp *su)
{
	if (of_property_read_u32(dn, "enabled", &su->enabled))
		su->enabled = 1;
}

/* gsc */
static void parse_gsc(struct device_node *dn,
		struct emstune_gsc *gsc)
{
	if (of_property_read_u32(dn, "up_threshold", &gsc->up_threshold))
		gsc->up_threshold = 1024;
	if (of_property_read_u32(dn, "down_threshold", &gsc->down_threshold))
		gsc->down_threshold = 1024;

	parse_cgroup_field(of_get_child_by_name(dn, "monitor-group"), gsc->enabled, 0);
}

/* should spread */
static void parse_should_spread(struct device_node *dn,
		struct emstune_should_spread *ss)
{
	if (of_property_read_u32(dn, "enabled", &ss->enabled))
		ss->enabled = 0;
}

/******************************************************************************
 * emstune sched boost                                                        *
 ******************************************************************************/
static int sched_boost;

int emstune_sched_boost(void)
{
	return sched_boost;
}

int emstune_support_uclamp(void)
{
	struct emstune_set *cur_set = emstune_cur_set();

	return cur_set->support_uclamp.enabled;
}

int emstune_should_spread(void)
{
	struct emstune_set *cur_set = emstune_cur_set();

	return cur_set->should_spread.enabled;
}

/******************************************************************************
 * sysfs for emstune                                                          *
 ******************************************************************************/
static ssize_t sched_boost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sched_boost);
}

static ssize_t sched_boost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int boost;

	if (sscanf(buf, "%d", &boost) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (boost < 0 || boost >= 100)
		return -EINVAL;

	sched_boost = boost;

	return count;
}
DEVICE_ATTR_RW(sched_boost);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, mode, level;
	unsigned long flags;

	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "mode%d : %s\n",
				mode, emstune.modes[mode].desc);

	ret += sprintf(buf + ret, "\n");
	spin_lock_irqsave(&emstune_lock, flags);

	ret += sprintf(buf + ret, "┌─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┬──────");
	ret += sprintf(buf + ret, "┐\n");

	ret += sprintf(buf + ret, "│     ");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "│ mode%d", mode);
	ret += sprintf(buf + ret, "│\n");

	ret += sprintf(buf + ret, "├─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┼──────");
	ret += sprintf(buf + ret, "┤\n");

	for (level = 0; level < emstune.level_count; level++) {
		ret += sprintf(buf + ret, "│ lv%d ", level);
		for (mode = 0; mode < emstune.mode_count; mode++) {
			if (mode == emstune.cur_mode &&
					level == emstune.cur_level)
				ret += sprintf(buf + ret, "│  curr");
			else
				ret += sprintf(buf + ret, "│      ");
		}
		ret += sprintf(buf + ret, "│\n");

		if (level == emstune.level_count - 1)
			break;

		ret += sprintf(buf + ret, "├-----");
		for (mode = 0; mode < emstune.mode_count; mode++)
			ret += sprintf(buf + ret, "┼------");
		ret += sprintf(buf + ret, "┤\n");
	}

	ret += sprintf(buf + ret, "└─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┴──────");
	ret += sprintf(buf + ret, "┘\n");

	ret += sprintf(buf + ret, "\n");
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}
DEVICE_ATTR_RO(status);

static ssize_t req_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_mode);
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}

static ssize_t req_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (mode < 0 || mode >= emstune.mode_count)
		return -EINVAL;

	emstune_mode_change(mode);

	return count;
}
DEVICE_ATTR_RW(req_mode);

static ssize_t req_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_level);
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}

static ssize_t req_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (sscanf(buf, "%d", &level) != 1)
		return -EINVAL;

	if (level < 0 || level >= emstune.level_count)
		return -EINVAL;

	emstune_update_request(&emstune.level_req, level);

	return count;
}
DEVICE_ATTR_RW(req_level);

static ssize_t req_level_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, count = 0;
	struct plist_node *pos;

	plist_for_each(pos, &dev->power.qos->freq.min_freq.list) {
		struct freq_qos_request *f_req;
		struct dev_pm_qos_request *d_req;
		struct emstune_mode_request *req;

		f_req = container_of(pos, struct freq_qos_request, pnode);
		d_req = container_of(f_req, struct dev_pm_qos_request,
							data.freq);
		req = container_of(d_req, struct emstune_mode_request,
							dev_req);

		count++;
		ret += snprintf(buf + ret, PAGE_SIZE, "%d: %d (%s:%d)\n",
					count, f_req->pnode.prio,
					req->func, req->line);
	}

	return ret;
}
DEVICE_ATTR_RO(req_level_debug);

static ssize_t reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "usage\n# <mode> <level> > reset\n");
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int mode, level;

	if (sscanf(buf, "%d %d", &mode, &level) != 2)
		return -EINVAL;

	if (mode < 0 || mode >= emstune.mode_count ||
	    level < 0 || level >= emstune.level_count)
		return -EINVAL;

	emstune_reset(mode, level);

	return count;
}
DEVICE_ATTR_RW(reset);

enum aio_tuner_item_type {
	TYPE1,		/* cpu */
	TYPE2,		/* group */
	TYPE3,		/* cpu,group */
	TYPE4,		/* cpu,option */
	TYPE5,		/* option */
	TYPE6,		/* nop */
};

#define	I(x, type, comm)	x,
#define ITEM_LIST							\
	I(sched_policy,			TYPE2,	"")			\
	I(active_weight,		TYPE3,	"")			\
	I(idle_weight,			TYPE3,	"")			\
	I(freqboost,			TYPE3,	"")			\
	I(cpufreq_gov_pelt_boost,	TYPE1,	"")			\
	I(cpufreq_gov_htask_boost,	TYPE1,	"")			\
	I(cpufreq_gov_dis_buck_share,	TYPE1,	"")			\
	I(cpufreq_gov_pelt_margin,	TYPE1,	"")			\
	I(cpufreq_gov_pelt_margin_freq,	TYPE1,	"")			\
	I(cpufreq_gov_rate_limit,	TYPE1,	"")			\
	I(cpufreq_gov_rate_limit_freq,	TYPE1,	"")			\
	I(fclamp_freq,			TYPE4,	"option:min=0,max=1")	\
	I(fclamp_period,		TYPE4,	"option:min=0,max=1")	\
	I(fclamp_ratio,			TYPE4,	"option:min=0,max=1")	\
	I(fclamp_monitor_group,		TYPE2,	"")			\
	I(ontime_en,			TYPE2,	"")			\
	I(ontime_boundary,		TYPE4,	"option:up=0,lo=1")	\
	I(cpus_binding_tsc,		TYPE6,	"val:mask")		\
	I(cpus_binding_cpumask,		TYPE2,	"val:mask")		\
	I(tex_en,			TYPE2,	"")			\
	I(tex_prio,			TYPE6,	"")			\
	I(ecs_threshold_ratio,		TYPE5,	"option:stage_id")	\
	I(ecs_dynamic_busy_ratio,	TYPE1,	"")			\
	I(ntu_ratio,			TYPE2,	"")			\
	I(frt_active_ratio,		TYPE1,	"")			\
	I(cpuidle_gov_expired_ratio,	TYPE1,	"")	\
	I(support_uclamp,           TYPE6,  "")	\
	I(gsc_en,			TYPE2,	"")			\
	I(gsc_up_threshold,           TYPE6,  "")	\
	I(gsc_down_threshold,           TYPE6,  "")	\
	I(should_spread,           TYPE6,  "")

enum { ITEM_LIST field_count };

struct aio_tuner_item {
	int		id;
	const char	*name;
	int		param_type;
	const char	*comment;
} aio_tuner_items[field_count];

static void aio_tuner_items_init(void)
{
	int i;

#undef	I
#define	I(x, type, comm) #x,
	const char * const item_name[] = { ITEM_LIST };

#undef	I
#define	I(x, type, comm) type,
	const int item_type[] = { ITEM_LIST };

#undef	I
#define	I(x, type, comm) comm,
	const char * const item_comm[] = { ITEM_LIST };

	for (i = 0; i < field_count; i++) {
		aio_tuner_items[i].id = i;
		aio_tuner_items[i].name = item_name[i];
		aio_tuner_items[i].param_type = item_type[i];
		aio_tuner_items[i].comment = item_comm[i];
	}
}

static ssize_t func_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	for (i = 0; i < field_count; i++) {
		ret += sprintf(buf + ret, "%d:%s\n",
			aio_tuner_items[i].id, aio_tuner_items[i].name);
	}

	return ret;
}
DEVICE_ATTR_RO(func_list);

static ssize_t aio_tuner_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	ret = sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "echo <func,mode,level,...> <value> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");

	for (i = 0; i < field_count; i++) {
		struct aio_tuner_item *item = &aio_tuner_items[i];
		char key[20] = { 0 };

		switch (item->param_type) {
		case TYPE1:
			strcpy(key, ",cpu");
			break;
		case TYPE2:
			strcpy(key, ",group");
			break;
		case TYPE3:
			strcpy(key, ",cpu,group");
			break;
		case TYPE4:
			strcpy(key, ",cpu,option");
			break;
		case TYPE5:
			strcpy(key, ",option");
			break;
		case TYPE6:
			break;
		};

		ret += sprintf(buf + ret, "[%d] %s\n", item->id, item->name);
		ret += sprintf(buf + ret, "	# echo {%d,mode,level%s} {val} > aio_tuner\n",
									item->id, key);
		if (strlen(item->comment))
			ret += sprintf(buf + ret, "	(%s)\n", item->comment);
		ret += sprintf(buf + ret, "\n");
	}

	return ret;
}

#define STR_LEN 10
static int cut_hexa_prefix(char *str)
{
	int i;

	if (!(str[0] == '0' && str[1] == 'x'))
		return -EINVAL;

	for (i = 0; i + 2 < STR_LEN; i++) {
		str[i] = str[i + 2];
		str[i + 2] = '\n';
	}

	return 0;
}

static int sanity_check_default(int field, int mode, int level)
{
	if (field < 0 || field >= field_count)
		return -EINVAL;
	if (mode < 0 || mode >= emstune.mode_count)
		return -EINVAL;
	if (level < 0 || level >= emstune.level_count)
		return -EINVAL;

	return 0;
}

static int sanity_check_option(int cpu, int group, int type)
{
	if (cpu < 0 || cpu > NR_CPUS)
		return -EINVAL;
	if (group < 0)
		return -EINVAL;
	if (!(type == 0 || type == 1))
		return -EINVAL;

	return 0;
}

enum val_type {
	VAL_TYPE_ONOFF,
	VAL_TYPE_RATIO,
	VAL_TYPE_RATIO_NEGATIVE,
	VAL_TYPE_LEVEL,
	VAL_TYPE_CPU,
	VAL_TYPE_MASK,
};

static int
sanity_check_convert_value(char *arg, enum val_type type, int limit, void *v)
{
	int ret = 0;
	long value;

	switch (type) {
	case VAL_TYPE_ONOFF:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		*(int *)v = !!(int)value;
		break;
	case VAL_TYPE_RATIO_NEGATIVE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < -limit || value > limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_RATIO:
	case VAL_TYPE_LEVEL:
	case VAL_TYPE_CPU:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < 0 || value > limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_MASK:
		if (cut_hexa_prefix(arg))
			return -EINVAL;
		if (cpumask_parse(arg, (struct cpumask *)v) ||
		    cpumask_empty((struct cpumask *)v))
			return -EINVAL;
		break;
	}

	return ret;
}

static void
set_value_single(int *field, char *v, enum val_type type, int limit)
{
	int value;

	if (sanity_check_convert_value(v, VAL_TYPE_LEVEL, limit, &value))
		return;

	*field = value;
}

static void
set_value_coregroup(int (field)[VENDOR_NR_CPUS], int cpu, char *v,
				enum val_type type, int limit)
{
	int i, value;

	if (sanity_check_option(cpu, 0, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[i] = value;
}

static void
set_value_cgroup(int (field)[CGROUP_COUNT], int group, char *v,
				enum val_type type, int limit)
{
	int value;

	if (sanity_check_option(0, group, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	if (group >= CGROUP_COUNT) {
		for (group = 0; group < CGROUP_COUNT; group++)
			field[group] = value;
		return;
	}

	field[group] = value;
}

static void
set_value_cgroup_coregroup(int (field)[CGROUP_COUNT][VENDOR_NR_CPUS],
				int cpu, int group, char *v,
				enum val_type type, int limit)
{
	int i, value;

	if (sanity_check_option(cpu, group, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	if (group >= CGROUP_COUNT) {
		for (group = 0; group < CGROUP_COUNT; group++)
			for_each_cpu(i, cpu_coregroup_mask(cpu))
				field[group][i] = value;
		return;
	}

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[group][i] = value;
}

#define NUM_OF_KEY	(10)

#define MAX_RATIO		10000 /* 10000% */
#define MAX_ESG_STEP		20
#define MAX_PATIENT_TICK	100
#define MAX_RATE_LIMIT		1000 /* 1000ms */

static ssize_t aio_tuner_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct emstune_set *set;
	char arg0[100], arg1[100], *_arg, *ptr;
	int keys[NUM_OF_KEY], i, ret, v;
	long val;
	int mode, level, field, cpu, group, type;
	struct cpumask mask;

	if (sscanf(buf, "%99s %99s", &arg0, &arg1) != 2)
		return -EINVAL;

	/* fill keys with default value(-1) */
	for (i = 0; i < NUM_OF_KEY; i++)
		keys[i] = -1;

	/* classify key input value by comma(,) */
	_arg = arg0;
	ptr = strsep(&_arg, ",");
	i = 0;
	while (ptr != NULL && (i < NUM_OF_KEY)) {
		ret = kstrtol(ptr, 10, &val);
		if (ret)
			return ret;

		keys[i] = (int)val;
		i++;

		ptr = strsep(&_arg, ",");
	};

	field = keys[0];
	mode = keys[1];
	level = keys[2];

	if (sanity_check_default(field, mode, level))
		return -EINVAL;

	set = &emstune.modes[mode].sets[level];

	switch (field) {
	case sched_policy:
		set_value_cgroup(set->sched_policy.policy, keys[3], arg1,
					VAL_TYPE_LEVEL, NUM_OF_SCHED_POLICY);
		break;
	case active_weight:
		set_value_cgroup_coregroup(set->active_weight.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case idle_weight:
		set_value_cgroup_coregroup(set->idle_weight.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case freqboost:
		set_value_cgroup_coregroup(set->freqboost.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_pelt_margin:
		set_value_coregroup(set->cpufreq_gov.split_pelt_margin, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_htask_boost:
		set_value_coregroup(set->cpufreq_gov.htask_boost, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_pelt_boost:
		set_value_coregroup(set->cpufreq_gov.pelt_boost, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_rate_limit:
		set_value_coregroup(set->cpufreq_gov.split_up_rate_limit, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpuidle_gov_expired_ratio:
		set_value_coregroup(set->cpuidle_gov.expired_ratio, keys[3], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case ontime_en:
		set_value_cgroup(set->ontime.enabled, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case ontime_boundary:
		cpu = keys[3];
		type = !!keys[4];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1024, &v))
			return -EINVAL;
		update_ontime_domain(&set->ontime.dom_list, cpu, v, type);
		break;
	case cpus_binding_tsc:
		if (kstrtol(arg1, 0, &val))
			return -EINVAL;
		val &= EMS_SCHED_CLASS_MASK;
		set->cpus_binding.target_sched_class = 0;
		for (i = 0; i < NUM_OF_SCHED_CLASS; i++)
			if (test_bit(i, &val))
				set_bit(i, &set->cpus_binding.target_sched_class);
		break;
	case cpus_binding_cpumask:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->cpus_binding.mask[group], &mask);
		break;
	case tex_en:
		set_value_cgroup(set->tex.enabled, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case tex_prio:
		set_value_single(&set->tex.prio, arg1,
					VAL_TYPE_LEVEL, MAX_PRIO);
		break;
	case frt_active_ratio:
		set_value_coregroup(set->frt.active_ratio, keys[3], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case ecs_threshold_ratio:
		i = keys[3];
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 10000, &v))
			return -EINVAL;
		update_ecs_threshold_ratio(&set->ecs.domain_list, i, v);
		break;
	case ecs_dynamic_busy_ratio:
		set_value_coregroup(set->ecs_dynamic.dynamic_busy_ratio, keys[3], arg1,
				VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case ntu_ratio:
		set_value_cgroup(set->ntu.ratio, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, 100);
		break;
	case fclamp_freq:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_freq, keys[3],
					arg1, VAL_TYPE_LEVEL, 10000000);
		else
			set_value_coregroup(set->fclamp.min_freq, keys[3],
					arg1, VAL_TYPE_LEVEL, 10000000);
		break;
	case fclamp_period:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_target_period,
					keys[3], arg1, VAL_TYPE_LEVEL, 100);
		else
			set_value_coregroup(set->fclamp.min_target_period,
					keys[3], arg1, VAL_TYPE_LEVEL, 100);
		break;
	case fclamp_ratio:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_target_ratio,
					keys[3], arg1, VAL_TYPE_LEVEL, 1000);
		else
			set_value_coregroup(set->fclamp.min_target_ratio,
					keys[3], arg1, VAL_TYPE_LEVEL, 1000);
		break;
	case fclamp_monitor_group:
		set_value_cgroup(set->fclamp.monitor_group, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case cpufreq_gov_pelt_margin_freq:
		set_value_coregroup(set->cpufreq_gov.split_pelt_margin_freq, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpufreq_gov_rate_limit_freq:
		set_value_coregroup(set->cpufreq_gov.split_up_rate_limit_freq, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpufreq_gov_dis_buck_share:
		set_value_coregroup(set->cpufreq_gov.dis_buck_share, keys[3], arg1,
					VAL_TYPE_LEVEL, 100);
		break;
	case support_uclamp:
		set_value_single(&set->support_uclamp.enabled, arg1, VAL_TYPE_ONOFF, 1);
		break;
	case gsc_en:
		set_value_cgroup(set->gsc.enabled, keys[3], arg1, VAL_TYPE_ONOFF, 0);
		break;
	case gsc_up_threshold:
		set_value_single(&set->gsc.up_threshold, arg1, VAL_TYPE_LEVEL, 1024);
		break;
	case gsc_down_threshold:
		set_value_single(&set->gsc.down_threshold, arg1, VAL_TYPE_LEVEL, 1024);
		break;
	case should_spread:
		set_value_single(&set->should_spread.enabled, arg1, VAL_TYPE_ONOFF, 1);
		break;
	}

	emstune_notify();

	return count;
}
DEVICE_ATTR_RW(aio_tuner);

static char *task_cgroup_simple_name[] = {
	"root",
	"fg",
	"bg",
	"ta",
	"rt",
	"sy",
	"syb",
	"n-h",
	"c-d",
	"mg",
};

static const char *get_sched_class_str(int class)
{
	if (class & EMS_SCHED_STOP)
		return "STOP";
	if (class & EMS_SCHED_DL)
		return "DL";
	if (class & EMS_SCHED_RT)
		return "RT";
	if (class & EMS_SCHED_FAIR)
		return "FAIR";
	if (class & EMS_SCHED_IDLE)
		return "IDLE";

	return NULL;
}

static int get_type_pending_bit(int type)
{
	int bit = 0;

	type >>= 1;
	while (type) {
		type >>= 1;
		bit++;
	}

	return bit;
}

#define MSG_SIZE 8192
static ssize_t cur_set_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	struct emstune_set *cur_set = emstune_cur_set();
	int cpu, group, class;
	char *msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	ssize_t count = 0, msg_size;

	/* mode/level info */
	if (emstune.boot.ongoing)
		count += sprintf(msg + count, "Booting exclusive mode\n");
	else
		count += sprintf(msg + count, "%s mode : level%d [mode=%d level=%d]\n",
				emstune.modes[emstune.cur_mode].desc,
				emstune.cur_level,
				get_type_pending_bit(EMSTUNE_MODE_TYPE(cur_set->type)),
				get_type_pending_bit(EMSTUNE_BOOST_TYPE(cur_set->type)));
	count += sprintf(msg + count, "\n");

	/* sched policy */
	count += sprintf(msg + count, "[sched-policy]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->sched_policy.policy[group]);
	count += sprintf(msg + count, "\n\n");

	/* active weight */
	count += sprintf(msg + count, "[active-weight]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->active_weight.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* idle weight */
	count += sprintf(msg + count, "[idle-weight]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->idle_weight.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* freq boost */
	count += sprintf(msg + count, "[freq-boost]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->freqboost.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* cpufreq gov */
	count += sprintf(msg + count, "[cpufreq gov]\n");
	count += sprintf(msg + count, "                    |-pelt margin--||-rate limit--|\n");
	count += sprintf(msg + count, "       pb   hb  dbs  margin    freq   rate    freq\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d: %3d  %3d %4d", cpu,
				cur_set->cpufreq_gov.pelt_boost[cpu],
				cur_set->cpufreq_gov.htask_boost[cpu],
				cur_set->cpufreq_gov.dis_buck_share[cpu]);

		if (cur_set->cpufreq_gov.split_pelt_margin_freq[cpu] == INT_MAX)
			count += sprintf(msg + count, "    %4d %7s",
				cur_set->cpufreq_gov.split_pelt_margin[cpu],
				"INF");
		else
			count += sprintf(msg + count, "    %4d %7u",
				cur_set->cpufreq_gov.split_pelt_margin[cpu],
				cur_set->cpufreq_gov.split_pelt_margin_freq[cpu]);

		if (cur_set->cpufreq_gov.split_up_rate_limit_freq[cpu] == INT_MAX)
			count += sprintf(msg + count, "   %4d %7s\n",
				cur_set->cpufreq_gov.split_up_rate_limit[cpu],
				"INF");
		else
			count += sprintf(msg + count, "   %4d %7u\n",
				cur_set->cpufreq_gov.split_up_rate_limit[cpu],
				cur_set->cpufreq_gov.split_up_rate_limit_freq[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* cpuidle gov */
	count += sprintf(msg + count, "[cpuidle gov]\n");
	count += sprintf(msg + count, "expired ratio\n");
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;
		count += sprintf(msg + count, "cpu%d: %4d\n", cpu,
				cur_set->cpuidle_gov.expired_ratio[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* ontime */
	count += sprintf(msg + count, "[ontime]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->ontime.enabled[group]);
	count += sprintf(msg + count, "\n\n");
	{
		struct ontime_dom *dom;

		count += sprintf(msg + count, "-----------------------------\n");
		if (!list_empty(cur_set->ontime.p_dom_list)) {
			list_for_each_entry(dom, cur_set->ontime.p_dom_list, node) {
				count += sprintf(msg + count, " cpus           :  %*pbl\n",
					cpumask_pr_args(&dom->cpus));
				count += sprintf(msg + count, " upper boundary : %4lu (%3d%%)\n",
					dom->upper_boundary, dom->upper_ratio);
				count += sprintf(msg + count, " lower boundary : %4lu (%3d%%)\n",
					dom->lower_boundary, dom->lower_ratio);
				count += sprintf(msg + count, "-----------------------------\n");
			}
		} else  {
			count += sprintf(msg + count, "list empty!\n");
			count += sprintf(msg + count, "-----------------------------\n");
		}
	}
	count += sprintf(msg + count, "\n");

	/* cpus binding */
	count += sprintf(msg + count, "[cpus binding]\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		count += sprintf(msg + count, "%5s", get_sched_class_str(1 << class));
	count += sprintf(msg + count, "\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		count += sprintf(msg + count, "%5d",
			(cur_set->cpus_binding.target_sched_class & (1 << class) ? 1 : 0));
	count += sprintf(msg + count, "\n\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4x",
			*(unsigned int *)cpumask_bits(&cur_set->cpus_binding.mask[group]));
	count += sprintf(msg + count, "\n\n");

	/* tex */
	count += sprintf(msg + count, "[tex]\n");
	count += sprintf(msg + count, "prio : %d\n", cur_set->tex.prio);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->tex.enabled[group]);
	count += sprintf(msg + count, "\n\n");

	/* frt */
	count += sprintf(msg + count, "[frt]\n");
	count += sprintf(msg + count, "     active\n");
	for_each_possible_cpu(cpu)
		count += sprintf(msg + count, "cpu%d: %4d%%\n",
				cpu,
				cur_set->frt.active_ratio[cpu]);
	count += sprintf(msg + count, "\n");

	/* ecs */
	count += sprintf(msg + count, "[ecs]\n");
	{
		struct ecs_domain *domain;

		count += sprintf(msg + count, "-------------------------\n");
		if (!list_empty(&cur_set->ecs.domain_list)) {
			list_for_each_entry(domain, &cur_set->ecs.domain_list, node) {
				count += sprintf(msg + count, " domain id       : %d\n",
					domain->id);
				count += sprintf(msg + count, " threshold ratio : %u\n",
					domain->busy_threshold_ratio);
				count += sprintf(msg + count, "-------------------------\n");
			}
		} else  {
			count += sprintf(msg + count, "list empty!\n");
			count += sprintf(msg + count, "-------------------------\n");
		}
	}
	count += sprintf(msg + count, "     busy_ratio\n");
	for_each_possible_cpu(cpu)
		count += sprintf(msg + count, "cpu%d: %4d%%\n",
				cpu,
				cur_set->ecs_dynamic.dynamic_busy_ratio[cpu]);
	count += sprintf(msg + count, "\n");

	/* new task util ratio */
	count += sprintf(msg + count, "[new task util ratio]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->ntu.ratio[group]);
	count += sprintf(msg + count, "\n\n");

	/* fclamp */
	count += sprintf(msg + count, "[fclamp]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->fclamp.monitor_group[group]);
	count += sprintf(msg + count, "\n\n");
	count += sprintf(msg + count, "     |-------- min -------||-------- max -------|\n");
	count += sprintf(msg + count, "        freq  period ratio    freq  period ratio\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d: %7d %6d %5d  %7d %6d %5d\n",
				cpu, cur_set->fclamp.min_freq[cpu],
				cur_set->fclamp.min_target_period[cpu],
				cur_set->fclamp.min_target_ratio[cpu],
				cur_set->fclamp.max_freq[cpu],
				cur_set->fclamp.max_target_period[cpu],
				cur_set->fclamp.max_target_ratio[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* support uclamp */
	count += sprintf(msg + count, "[support uclamp]\n");
	count += sprintf(msg + count, "enabled : %d\n", cur_set->support_uclamp.enabled);
	count += sprintf(msg + count, "\n");

	/* gsc */
	count += sprintf(msg + count, "[gsc]\n");
	count += sprintf(msg + count, "up_threshold : %d\n", cur_set->gsc.up_threshold);
	count += sprintf(msg + count, "down_threshold : %d\n", cur_set->gsc.down_threshold);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->gsc.enabled[group]);
	count += sprintf(msg + count, "\n\n");

	/* should spread */
	count += sprintf(msg + count, "[should spread]\n");
	count += sprintf(msg + count, "enabled : %d\n", cur_set->should_spread.enabled);
	count += sprintf(msg + count, "\n");

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	kfree(msg);

	return msg_size;
}
static BIN_ATTR(cur_set, 0440, cur_set_read, NULL, 0);

static DEFINE_MUTEX(task_boost_mutex);
static int task_boost = 0;

static ssize_t task_boost_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret = 0;

	mutex_lock(&task_boost_mutex);

	ret = sprintf(buf, "%d\n", task_boost);

	mutex_unlock(&task_boost_mutex);
	return ret;
}

static ssize_t task_boost_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	int tid;
	struct task_struct *task;

	mutex_lock(&task_boost_mutex);

	if (sscanf(buf, "%d", &tid) != 1) {
		mutex_unlock(&task_boost_mutex);
		return -EINVAL;
	}

	/* clear prev task_boost */
	if (task_boost != 0) {
		task = get_pid_task(find_vpid(task_boost), PIDTYPE_PID);
		if (task) {
			ems_boosted_tex(task) = 0;
			put_task_struct(task);
		}
	}

	/* set task_boost */
	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (task) {
		ems_boosted_tex(task) = 1;
		put_task_struct(task);
	}

	task_boost = tid;

	mutex_unlock(&task_boost_mutex);
	return count;
}
DEVICE_ATTR_RW(task_boost);

static ssize_t task_boost_del_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret = 0;

	return ret;
}

static ssize_t task_boost_del_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	int tid;
	struct task_struct *task;

	mutex_lock(&task_boost_mutex);

	if (sscanf(buf, "%d", &tid) != 1) {
		mutex_unlock(&task_boost_mutex);
		return -EINVAL;
	}

	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (task) {
		ems_boosted_tex(task) = 0;
		put_task_struct(task);
	}

	/* clear task_boost at last task_boost_del */
	if (task_boost == tid)
		task_boost = 0;

	mutex_unlock(&task_boost_mutex);
	return count;
}
DEVICE_ATTR_RW(task_boost_del);

static struct attribute *emstune_attrs[] = {
	&dev_attr_sched_boost.attr,
	&dev_attr_status.attr,
	&dev_attr_req_mode.attr,
	&dev_attr_req_level.attr,
	&dev_attr_req_level_debug.attr,
	&dev_attr_reset.attr,
	&dev_attr_aio_tuner.attr,
	&dev_attr_func_list.attr,
	&dev_attr_task_boost.attr,
	&dev_attr_task_boost_del.attr,
	NULL,
};

static struct bin_attribute *emstune_bin_attrs[] = {
	&bin_attr_cur_set,
	NULL,
};

static struct attribute_group emstune_attr_group = {
	.name		= "emstune",
	.attrs		= emstune_attrs,
	.bin_attrs	= emstune_bin_attrs,
};

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
static int emstune_mode_backup(void)
{
	int i;

	emstune.modes_backup = kzalloc(sizeof(struct emstune_mode)
					* emstune.mode_count, GFP_KERNEL);
	if (!emstune.modes_backup)
		return -ENOMEM;

	/* backup modes */
	memcpy(emstune.modes_backup, emstune.modes,
		sizeof(struct emstune_mode) * emstune.mode_count);

	for (i = 0; i < emstune.mode_count; i++) {
		emstune.modes_backup[i].sets =
				kzalloc(sizeof(struct emstune_set)
				* emstune.level_count, GFP_KERNEL);
		if (!emstune.modes_backup[i].sets)
			return -ENOMEM;

		/* backup sets */
		memcpy(emstune.modes_backup[i].sets, emstune.modes[i].sets,
			sizeof(struct emstune_set) * emstune.level_count);
	}

	return 0;
}

static void
emstune_set_type_init(struct emstune_set *set, int mode, int level)
{
	/* init set type */
	set->type = 0;
	set->type |= (1 << level);
	set->type |= (1 << (BEGIN_OF_MODE_TYPE + mode));
}

int emstune_cpu_dsu_table_index(void *_set)
{
	struct emstune_set *set = (struct emstune_set *)_set;

	return set->cpu_dsu_table_index;
}
EXPORT_SYMBOL_GPL(emstune_cpu_dsu_table_index);

static void emstune_perf_table_init(struct emstune_set *set,
					struct device_node *base_node,
					struct device_node *set_node)
{
	if (!of_property_read_u32(set_node, "cpu-dsu-table-index",
				&set->cpu_dsu_table_index))
		return;

	if (!of_property_read_u32(base_node, "cpu-dsu-table-index",
				&set->cpu_dsu_table_index))
		return;

	set->cpu_dsu_table_index = 0;	/* 0 = default index */
}

#define parse(_name)							\
	child = of_get_child_by_name(set_node, #_name);			\
	if (!child)							\
		child = of_get_child_by_name(base_node, #_name);	\
	parse_##_name(child, &set->_name);				\
	of_node_put(child)

static int emstune_set_init(struct device_node *base_node,
				struct device_node *set_node,
				int mode, int level, struct emstune_set *set)
{
	struct device_node *child;

	emstune_set_type_init(set, mode, level);
	emstune_perf_table_init(set, base_node, set_node);

	parse(specific_energy_table);
	parse(sched_policy);
	parse(active_weight);
	parse(idle_weight);
	parse(freqboost);
	parse(cpufreq_gov);
	parse(cpuidle_gov);
	parse(ontime);
	parse(cpus_binding);
	parse(tex);
	parse(frt);
	parse(ecs);
	parse(ecs_dynamic);
	parse(ntu);
	parse(fclamp);
	parse(support_uclamp);
	parse(gsc);
	parse(should_spread);

	return 0;
}

static void emstune_boot_done(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	emstune.boot.ongoing = 0;
	emstune_notify();
	spin_unlock_irqrestore(&emstune_lock, flags);
}

static void emstune_boot(void)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	emstune.boot.ongoing = 1;
	emstune_notify();
	spin_unlock_irqrestore(&emstune_lock, flags);

	INIT_DELAYED_WORK(&emstune.boot.work, emstune_boot_done);
	schedule_delayed_work(&emstune.boot.work, msecs_to_jiffies(40000));
}

static int emstune_mode_init(struct device_node *dn)
{
	struct device_node *emstune_node, *mode_node, *node;
	int mode_id, level, ret;

	emstune_node = of_get_child_by_name(dn, "emstune");
	if (!emstune_node)
		return -EINVAL;

	/* If boost_level does not exist in dt, emstune.boost_level is 0 */
	of_property_read_u32(emstune_node, "boost-level", &emstune.boost_level);

	emstune.mode_count = of_get_child_count(emstune_node);
	if (!emstune.mode_count)
		return -ENODATA;

	emstune.modes = kzalloc(sizeof(struct emstune_mode)
					* emstune.mode_count, GFP_KERNEL);
	if (!emstune.modes)
		return -ENOMEM;

	mode_id = 0;
	for_each_child_of_node(emstune_node, mode_node) {
		struct device_node *level_node;
		struct emstune_mode *mode = &emstune.modes[mode_id];
		struct emstune_set *sets;
		int level_count;

		if (of_property_read_string(mode_node, "desc", &mode->desc)) {
			ret = -ENODATA;
			goto fail;
		}

		level_count = of_get_child_count(mode_node);
		if (!level_count) {
			ret = -ENODATA;
			goto fail;
		}

		if (!emstune.level_count)
			emstune.level_count = level_count;
		else if (emstune.level_count != level_count) {
			/* the level count of each mode must be same */
			ret = -EINVAL;
			goto fail;
		}

		sets = kzalloc(sizeof(struct emstune_set)
					* emstune.level_count, GFP_KERNEL);
		if (!sets) {
			ret = -ENOMEM;
			goto fail;
		}

		level = 0;
		for_each_child_of_node(mode_node, level_node) {
			struct device_node *base_node =
				of_parse_phandle(level_node, "base", 0);
			struct device_node *set_node =
				of_parse_phandle(level_node, "set", 0);

			if (!base_node || !set_node) {
				ret = -ENODATA;
				goto fail;
			}

			ret = emstune_set_init(base_node, set_node,
						mode_id, level, &sets[level]);

			of_node_put(base_node);
			of_node_put(set_node);

			if (ret)
				goto fail;

			level++;
		}

		mode->sets = sets;
		mode_id++;
	}

	/* backup origin emstune */
	if (emstune_mode_backup())
		goto fail;

	node = of_parse_phandle(emstune_node, "boot-set", 0);
	if (node) {
		emstune_set_init(node, node, 0, 0, &emstune.boot.set);
		emstune_boot();
		of_node_put(node);
	} else
		emstune_mode_change(0);

	return 0;

fail:
	for (mode_id = 0; mode_id < emstune.mode_count; mode_id++) {
		struct emstune_mode *mode = &emstune.modes[mode_id];

		kfree(mode->sets);
		mode->sets = NULL;
	}

	kfree(emstune.modes);
	emstune.modes = NULL;

	return ret;
}

static int emstune_pm_qos_init(struct device *dev)
{
	int ret;

	emstune.nb.notifier_call = emstune_pm_qos_notifier;
	ret = dev_pm_qos_add_notifier(dev, &emstune.nb,
					DEV_PM_QOS_MIN_FREQUENCY);
	if (ret) {
		pr_err("Failed to register emstume qos notifier\n");
		return ret;
	}

	emstune.ems_dev = dev;

	emstune_add_request(&emstune.level_req);

	return 0;
}

int emstune_init(struct kobject *ems_kobj, struct device_node *dn,
						struct device *dev)
{
	int ret;

	ret = emstune_mode_init(dn);
	if (ret) {
		WARN(1, "failed to initialize emstune with err=%d\n", ret);
		return ret;
	}

	ret = emstune_pm_qos_init(dev);
	if (ret) {
		pr_err("Failed to initialize emstune PM QoS\n");
		return ret;
	}

	if (sysfs_create_group(ems_kobj, &emstune_attr_group))
		pr_err("failed to initialize emstune sysfs\n");

	aio_tuner_items_init();

	return 0;
}
