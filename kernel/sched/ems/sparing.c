/*
 * Exynos Core Sparing Governor - Exynos Mobile Scheduler
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd
 * Choonghoon Park <choong.park@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static struct {
	unsigned long			update_period;

	const struct cpumask		*cpus;

	/* ecs governor */
	struct list_head		domain_list;
	struct cpumask			governor_cpus;
	struct cpumask			out_of_governing_cpus;
	struct system_profile_data	spd;
	bool				skip_update;
	bool				governor_enable;
	unsigned int			default_gov;

	struct {
		unsigned int		target_domain_id;
		unsigned int		target_stage_id;
	} ioctl_info;
} ecs;

static struct kobject *stage_kobj;

static DEFINE_RAW_SPINLOCK(sparing_lock);

#define MAX_ECS_DOMAIN	VENDOR_NR_CPUS
#define MAX_ECS_STAGE	VENDOR_NR_CPUS

static struct {
	unsigned int ratio;
} ecs_tune[MAX_ECS_DOMAIN][MAX_ECS_STAGE];

/******************************************************************************
 *                             core sparing governor                          *
 ******************************************************************************/
static inline struct ecs_stage *first_stage(struct ecs_domain *domain)
{
	return list_first_entry(&domain->stage_list, struct ecs_stage, node);
}

static inline struct ecs_stage *last_stage(struct ecs_domain *domain)
{
	return list_last_entry(&domain->stage_list, struct ecs_stage, node);
}

static struct ecs_stage *find_stage_by_id(struct ecs_domain *domain, unsigned int id)
{
	struct ecs_stage *stage;

	list_for_each_entry(stage, &domain->stage_list, node) {
		if (stage->id == id)
			return stage;
	}

	return NULL;
}

static struct ecs_domain *find_domain_by_id(unsigned int id)
{
	struct ecs_domain *domain;

	list_for_each_entry(domain, &ecs.domain_list, node)
		if (domain->id == id)
			return domain;

	return NULL;
}

#define ECS_FORWARD		(0)
#define ECS_BACKWARD		(1)
static inline int
get_busy_threshold(struct ecs_domain *domain, struct ecs_stage *stage, int direction)
{
	struct ecs_stage *threshold_stage = direction == ECS_FORWARD ?
					    stage : list_prev_entry(stage, node);
	int busy_threshold = threshold_stage->busy_threshold;
	int busy_threshold_ratio;

	switch (direction) {
	case ECS_FORWARD:
		/* Nothing to do */
		break;
	case ECS_BACKWARD:
		/* Half of current stage threshold */
		busy_threshold >>= 1;
		break;
	default:
		/* Stale direction... just use original threshold */
		break;
	}

	busy_threshold_ratio = max(ecs_tune[domain->id][stage->id].ratio,
				   domain->busy_threshold_ratio);

	return (busy_threshold * busy_threshold_ratio) / 100;
}

static bool need_backward(struct ecs_domain *domain, int monitor_util_sum)
{
	/* is current first stage? */
	if (domain->cur_stage == first_stage(domain))
		return false;

	return monitor_util_sum <
		get_busy_threshold(domain, domain->cur_stage, ECS_BACKWARD);
}

static void prev_stage(struct ecs_domain *domain, int monitor_util_sum)
{
	int threshold;
	struct cpumask prev_mask;

	while (need_backward(domain, monitor_util_sum)) {
		/* Data for tracing backward stage transition */
		threshold = get_busy_threshold(domain, domain->cur_stage, ECS_BACKWARD);
		cpumask_copy(&prev_mask, &domain->cur_stage->cpus);

		/* Update current statge backward in this domain */
		domain->cur_stage = list_prev_entry(domain->cur_stage, node);

		trace_ecs_update_domain("backward", domain->id,
					monitor_util_sum, threshold,
					*(unsigned int *)cpumask_bits(&prev_mask),
					*(unsigned int *)cpumask_bits(&domain->cur_stage->cpus));
	}
}

static bool need_forward(struct ecs_domain *domain, int monitor_util_sum)
{
	/* is current last stage? */
	if (domain->cur_stage == last_stage(domain))
		return false;

	return monitor_util_sum >
		get_busy_threshold(domain, domain->cur_stage, ECS_FORWARD);
}

static void next_stage(struct ecs_domain *domain, int monitor_util_sum)
{
	int threshold;
	struct cpumask prev_mask;

	while (need_forward(domain, monitor_util_sum)) {
		/* Data for tracing forward stage transition */
		threshold = get_busy_threshold(domain, domain->cur_stage, ECS_FORWARD);
		cpumask_copy(&prev_mask, &domain->cur_stage->cpus);

		/* Update current statge forward in this domain */
		domain->cur_stage = list_next_entry(domain->cur_stage, node);

		trace_ecs_update_domain("forward", domain->id,
					monitor_util_sum, threshold,
					*(unsigned int *)cpumask_bits(&prev_mask),
					*(unsigned int *)cpumask_bits(&domain->cur_stage->cpus));
	}
}

static void __update_ecs_domain(struct ecs_domain *domain, int monitor_util_sum)
{
	next_stage(domain, monitor_util_sum);
	prev_stage(domain, monitor_util_sum);
}

static void update_ecs_domain(struct ecs_domain *domain)
{
	int cpu;
	int monitor_util_sum = 0;
	struct cpumask mask;
	struct system_profile_data *spd = &ecs.spd;

	if (!domain->cur_stage)
		return;

	cpumask_and(&mask, ecs.cpus, cpu_online_mask);
	cpumask_and(&mask, &mask, &domain->cpus);
	if (cpumask_empty(&mask))
		return;

	for_each_cpu(cpu, &mask)
		monitor_util_sum += spd->cp[cpu][CPU_UTIL].value;

	__update_ecs_domain(domain, monitor_util_sum);
}

static void reflect_fastest_cpus(struct cpumask *governor_cpus)
{
	int cpu, misfit_task_count = ecs.spd.misfit_task_count;
	struct cpumask prev_governor_cpus;

	if (misfit_task_count <= 0)
		return;

	cpumask_copy(&prev_governor_cpus, governor_cpus);

	for_each_cpu_and(cpu, cpu_fastest_mask(), cpu_active_mask) {
		cpumask_set_cpu(cpu, governor_cpus);

		if (--misfit_task_count <= 0)
			break;
	}

	if (!cpumask_equal(&prev_governor_cpus, governor_cpus))
		trace_ecs_reflect_fastest_cpus(ecs.spd.misfit_task_count,
						*(unsigned int *)cpumask_bits(&prev_governor_cpus),
						*(unsigned int *)cpumask_bits(governor_cpus));
}

static void update_ecs_governor_cpus(void)
{
	struct ecs_domain *domain;
	struct cpumask governor_cpus;

	get_system_sched_data(&ecs.spd);

	cpumask_clear(&governor_cpus);

	list_for_each_entry(domain, &ecs.domain_list, node) {
		update_ecs_domain(domain);
		cpumask_or(&governor_cpus,
			   &governor_cpus, &domain->cur_stage->cpus);
	}

	cpumask_or(&governor_cpus, &governor_cpus, &ecs.out_of_governing_cpus);

	reflect_fastest_cpus(&governor_cpus);

	if (!cpumask_equal(&ecs.governor_cpus, &governor_cpus)) {
		trace_ecs_update_governor_cpus("governor",
				*(unsigned int *)cpumask_bits(&ecs.governor_cpus),
				*(unsigned int *)cpumask_bits(&governor_cpus));

		cpumask_copy(&ecs.governor_cpus, &governor_cpus);
		update_ecs_cpus();
	}
}

static void ecs_control_governor(bool enable)
{
	struct ecs_domain *domain;
	struct cpumask governor_cpus;

	/*
	 * In case of enabling, nothing to do.
	 * Governor will work at next scheduler tick.
	 */
	if (enable)
		return;

	cpumask_clear(&governor_cpus);

	/* Update cur stage to last stage */
	list_for_each_entry(domain, &ecs.domain_list, node) {
		if (domain->cur_stage != last_stage(domain))
			domain->cur_stage = last_stage(domain);

		cpumask_or(&governor_cpus, &governor_cpus,
					   &domain->cur_stage->cpus);
	}

	/* Include cpus which governor doesn't consider */
	cpumask_or(&governor_cpus, &governor_cpus, &ecs.out_of_governing_cpus);

	if (!cpumask_equal(&ecs.governor_cpus, &governor_cpus)) {
		trace_ecs_update_governor_cpus("disabled",
				*(unsigned int *)cpumask_bits(&ecs.governor_cpus),
				*(unsigned int *)cpumask_bits(&governor_cpus));

		cpumask_copy(&ecs.governor_cpus, &governor_cpus);
		update_ecs_cpus();
	}
}

/******************************************************************************
 *                       emstune mode update notifier                         *
 ******************************************************************************/
static int ecs_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	unsigned long flags;
	struct ecs_domain *domain, *emstune_domain;

	raw_spin_lock_irqsave(&sparing_lock, flags);
	if (!list_empty(&cur_set->ecs.domain_list)) {
		list_for_each_entry(emstune_domain, &cur_set->ecs.domain_list, node) {
			domain = find_domain_by_id(emstune_domain->id);
			if (!domain)
				continue;
			domain->busy_threshold_ratio = emstune_domain->busy_threshold_ratio;
		}
	} else {
		list_for_each_entry(domain, &ecs.domain_list, node)
			domain->busy_threshold_ratio = 0;
	}

	if (!ecs.skip_update && ecs.governor_enable)
		update_ecs_governor_cpus();
	raw_spin_unlock_irqrestore(&sparing_lock, flags);

	return NOTIFY_OK;
}

static struct notifier_block ecs_mode_update_notifier = {
	.notifier_call = ecs_mode_update_callback,
};

/******************************************************************************
 *                      sysbusy state change notifier                         *
 ******************************************************************************/
static int ecs_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;
	bool old_skip_update;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	raw_spin_lock(&sparing_lock);

	old_skip_update = ecs.skip_update;
	ecs.skip_update = (state > SYSBUSY_STATE0);

	if ((old_skip_update != ecs.skip_update) && ecs.governor_enable)
		ecs_control_governor(!ecs.skip_update);

	raw_spin_unlock(&sparing_lock);

	return NOTIFY_OK;
}

static struct notifier_block ecs_sysbusy_notifier = {
	.notifier_call = ecs_sysbusy_notifier_call,
};

/******************************************************************************
 *                            ioctl event handler                             *
 ******************************************************************************/
int ecs_ioctl_get_domain_count(void)
{
	int count = 0;
	struct list_head *cursor;

	list_for_each(cursor, &ecs.domain_list)
		count++;

	return count;
}

static void ecs_ioctl_get_cd_ratio(int cpu, int *cd_dp_ratio, int *cd_cp_ratio)
{
	struct device_node *np, *child;

	*cd_dp_ratio = 1000;
	*cd_cp_ratio = 1000;

	np = of_find_node_by_path("/power-data/cpu");
	if (!np)
		return;

	for_each_child_of_node(np, child) {
		const char *buf;
		struct cpumask cpus;

		if (of_property_read_string(child, "cpus", &buf))
			continue;

		cpulist_parse(buf, &cpus);
		if (cpumask_test_cpu(cpu, &cpus)) {
			of_property_read_u32(child,
				"dhry-to-clang-dp-ratio", cd_dp_ratio);
			of_property_read_u32(child,
				"dhry-to-clang-cap-ratio", cd_cp_ratio);
			break;
		}
	}

	of_node_put(np);
}

static inline void
fill_ecs_domain_info_state(int cpu, int state_index,
			   struct energy_state *state,
			   struct ems_ioctl_ecs_domain_info *info)
{
	info->states[cpu][state_index].frequency = state->frequency;
	info->states[cpu][state_index].capacity = state->capacity;
	info->states[cpu][state_index].v_square = state->voltage * state->voltage;
	info->states[cpu][state_index].dynamic_power = state->dynamic_power;
	info->states[cpu][state_index].static_power = state->static_power;
}

int ecs_ioctl_get_ecs_domain_info(struct ems_ioctl_ecs_domain_info *info)
{
	unsigned int domain_id = ecs.ioctl_info.target_domain_id;
	struct ecs_domain *domain;
	struct ecs_stage *stage;
	int stage_index = 0;
	int cpu;

	domain = find_domain_by_id(domain_id);
	if (!domain)
		return -ENODEV;

	info->cpus = *(unsigned long *)cpumask_bits(&domain->cpus);

	info->num_of_state = INT_MAX;

	for_each_cpu_and(cpu, &domain->cpus, cpu_online_mask) {
		int energy_state_index;
		struct cpufreq_policy *policy;
		struct cpufreq_frequency_table *pos;
		struct energy_state state;

		ecs_ioctl_get_cd_ratio(cpu, &info->cd_dp_ratio[cpu],
					    &info->cd_cp_ratio[cpu]);

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("EMS: %s: No cpufreq policy for CPU%d\n", __func__, cpu);
			continue;
		}

		energy_state_index = 0;
		cpufreq_for_each_valid_entry(pos, policy->freq_table) {
			et_get_orig_state(cpu, pos->frequency, &state);

			fill_ecs_domain_info_state(cpu, energy_state_index, &state, info);

			energy_state_index++;

			if (energy_state_index >= NR_ECS_ENERGY_STATES)
				break;
		}

		info->num_of_state = min(info->num_of_state, energy_state_index);

		cpufreq_cpu_put(policy);
	}

	list_for_each_entry(stage, &domain->stage_list, node) {
		info->cpus_each_stage[stage_index] = *(unsigned long *)cpumask_bits(&stage->cpus);
		stage_index++;
	}

	info->num_of_stage = stage_index;

	return 0;
}

void ecs_ioctl_set_target_domain(unsigned int domain_id)
{
	ecs.ioctl_info.target_domain_id = domain_id;
}

void ecs_ioctl_set_target_stage(unsigned int stage_id)
{
	ecs.ioctl_info.target_stage_id = stage_id;
}

void ecs_ioctl_set_stage_threshold(unsigned int threshold)
{
	unsigned int domain_id = ecs.ioctl_info.target_domain_id;
	unsigned int stage_id = ecs.ioctl_info.target_stage_id;
	struct ecs_domain *domain;
	struct ecs_stage *stage;

	domain = find_domain_by_id(domain_id);
	if (!domain)
		return;

	stage = find_stage_by_id(domain, stage_id);
	if (!stage)
		return;

	stage->busy_threshold = threshold;
}

/******************************************************************************
 *                                    sysfs                                   *
 ******************************************************************************/
static ssize_t show_ecs_update_period(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", ecs.update_period);
}

static ssize_t store_ecs_update_period(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long period;

	if (!sscanf(buf, "%d", &period))
		return -EINVAL;

	ecs.update_period = period;

	return count;
}

static struct kobj_attribute ecs_update_period_attr =
__ATTR(update_period, 0644, show_ecs_update_period, store_ecs_update_period);

static ssize_t show_ecs_domain(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct ecs_domain *domain;
	struct ecs_stage *stage;

	if (!list_empty(&ecs.domain_list)) {
		list_for_each_entry(domain, &ecs.domain_list, node) {
			ret += sprintf(buf + ret, "[domain%d]\n", domain->id);
			ret += sprintf(buf + ret, " emstune threshold ratio : %u\n",
					domain->busy_threshold_ratio);
			ret += sprintf(buf + ret, " --------------------------------\n");
			list_for_each_entry(stage, &domain->stage_list, node) {
				ret += sprintf(buf + ret, "| stage%d\n",
						stage->id);
				ret += sprintf(buf + ret, "| ecs threshold ratio    : %u\n",
						ecs_tune[domain->id][stage->id].ratio);
				ret += sprintf(buf + ret, "| cpus                   : %*pbl\n",
						cpumask_pr_args(&stage->cpus));

				if (stage != first_stage(domain))
					ret += sprintf(buf + ret, "| backward threshold     : %u\n",
							get_busy_threshold(domain, stage, ECS_BACKWARD));

				if (stage != last_stage(domain))
					ret += sprintf(buf + ret, "| forward threshold      : %u\n",
							get_busy_threshold(domain, stage, ECS_FORWARD));

				ret += sprintf(buf + ret, " --------------------------------\n");
			}
		}
	} else {
		ret += sprintf(buf + ret, "domain list empty\n");
	}

	return ret;
}

static struct kobj_attribute ecs_domain_attr =
__ATTR(domains, 0444, show_ecs_domain, NULL);

static ssize_t show_ecs_governor_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ecs.governor_enable);
}

static ssize_t store_ecs_governor_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int enable;
	unsigned long flags;

	if (sscanf(buf, "%d", &enable) != 1)
		return -EINVAL;

	raw_spin_lock_irqsave(&sparing_lock, flags);
	if (enable != ecs.governor_enable)
		ecs_control_governor(enable);

	ecs.governor_enable = enable;
	raw_spin_unlock_irqrestore(&sparing_lock, flags);

	return count;
}

static struct kobj_attribute ecs_governor_enable_attr =
__ATTR(governor_enable, 0644, show_ecs_governor_enable, store_ecs_governor_enable);

static ssize_t show_ecs_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct ecs_domain *domain;
	struct ecs_stage *stage;

	list_for_each_entry(domain, &ecs.domain_list, node) {
		list_for_each_entry(stage, &domain->stage_list, node) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"domain=%d stage=%d ecs-ratio=%d emstune-ratio=%d\n",
				domain->id, stage->id,
				ecs_tune[domain->id][stage->id].ratio,
				domain->busy_threshold_ratio);
		}
	}

	return ret;
}

static ssize_t store_ecs_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int domain_id, stage_id, ratio;

	if (sscanf(buf, "%d %d %d", &domain_id, &stage_id, &ratio) != 3)
		return -EINVAL;

	if (domain_id < 0 || domain_id >= MAX_ECS_STAGE ||
	     stage_id < 0 || stage_id >= MAX_ECS_STAGE)
		return -EINVAL;

	ecs_tune[domain_id][stage_id].ratio = ratio;

	return count;
}

static struct kobj_attribute ecs_ratio =
__ATTR(ratio, 0644, show_ecs_ratio, store_ecs_ratio);

static int ecs_sysfs_init(struct kobject *ecs_gov_kobj)
{
	int ret;

	stage_kobj = kobject_create_and_add("stage", ecs_gov_kobj);
	if (!stage_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(stage_kobj, &ecs_update_period_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(stage_kobj, &ecs_domain_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(stage_kobj, &ecs_governor_enable_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);
	ret = sysfs_create_file(stage_kobj, &ecs_ratio.attr);
	if (ret)
		pr_warn("%s: failed to create ecs sysfs\n", __func__);

	return ret;
}

/******************************************************************************
 *                               initialization                               *
 ******************************************************************************/
static int
init_ecs_stage(struct device_node *dn, struct ecs_domain *domain, int stage_id)
{
	const char *buf;
	struct ecs_stage *stage;

	stage = kzalloc(sizeof(struct ecs_stage), GFP_KERNEL);
	if (!stage) {
		pr_err("%s: fail to alloc ecs stage\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_string(dn, "cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -EINVAL;
	} else
		cpulist_parse(buf, &stage->cpus);

	if (of_property_read_u32(dn, "busy-threshold", &stage->busy_threshold))
		stage->busy_threshold = 0;

	stage->id = stage_id;
	list_add_tail(&stage->node, &domain->stage_list);

	return 0;
}

static int
init_ecs_domain(struct device_node *dn, struct list_head *domain_list, int domain_id)
{
	int ret = 0, stage_id = 0;
	const char *buf;
	struct ecs_domain *domain;
	struct device_node *stage_dn;

	domain = kzalloc(sizeof(struct ecs_domain), GFP_KERNEL);
	if (!domain) {
		pr_err("%s: fail to alloc ecs domain\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_string(dn, "cpus", &buf)) {
		pr_err("%s: cpus property is omitted\n", __func__);
		return -EINVAL;
	} else
		cpulist_parse(buf, &domain->cpus);

	INIT_LIST_HEAD(&domain->stage_list);

	cpumask_andnot(&ecs.out_of_governing_cpus, &ecs.out_of_governing_cpus,
						   &domain->cpus);

	for_each_child_of_node(dn, stage_dn) {
		ret = init_ecs_stage(stage_dn, domain, stage_id);
		if (ret)
			goto finish;

		stage_id++;
	}

	domain->id = domain_id;
	domain->busy_threshold_ratio = 100;
	domain->cur_stage = first_stage(domain);
	list_add_tail(&domain->node, domain_list);

finish:
	return ret;
}

static int init_ecs_domain_list(void)
{
	int ret = 0, domain_id = 0;
	struct device_node *dn, *domain_dn;

	dn = of_find_node_by_path("/ems/ecs");
	if (!dn) {
		pr_err("%s: fail to get ecs device node\n", __func__);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&ecs.domain_list);

	cpumask_copy(&ecs.out_of_governing_cpus, cpu_possible_mask);

	for_each_child_of_node(dn, domain_dn) {
		if (init_ecs_domain(domain_dn, &ecs.domain_list, domain_id)) {
			ret = -EINVAL;
			goto finish;
		}

		domain_id++;
	}

	of_property_read_u32(dn, "default-gov", &ecs.default_gov);

finish:
	return ret;
}

static void stage_start(const struct cpumask *cpus)
{
	ecs.cpus = cpus;
	ecs.governor_enable = true;
}

static unsigned long last_update_jiffies;
static void stage_update(void)
{
	unsigned long now = jiffies;

	if (!ecs.governor_enable)
		return;

	if (!raw_spin_trylock(&sparing_lock))
		return;

	if (list_empty(&ecs.domain_list))
		goto out;

	if (ecs.skip_update)
		goto out;

	if (now < last_update_jiffies + msecs_to_jiffies(ecs.update_period))
		goto out;

	update_ecs_governor_cpus();

	last_update_jiffies = now;

out:
	raw_spin_unlock(&sparing_lock);
}

static void stage_stop(void)
{
	ecs.governor_enable = false;
	ecs.cpus = NULL;
}

static const struct cpumask *stage_get_target_cpus(void)
{
	return &ecs.governor_cpus;
}

static struct ecs_governor stage_gov = {
	.name = "stage",
	.update = stage_update,
	.start = stage_start,
	.stop = stage_stop,
	.get_target_cpus = stage_get_target_cpus,
};

int ecs_gov_stage_init(struct kobject *ems_kobj)
{
	if (init_ecs_domain_list()) {
		pr_info("ECS governor will not affect scheduler\n");

		/* Reset domain list and ecs.out_of_governing_cpus */
		INIT_LIST_HEAD(&ecs.domain_list);
		cpumask_copy(&ecs.out_of_governing_cpus, cpu_possible_mask);
	}

	/* 16 msec in default */
	ecs.update_period = 16;
	cpumask_copy(&ecs.governor_cpus, cpu_possible_mask);

	ecs_sysfs_init(ecs_get_governor_object());

	emstune_register_notifier(&ecs_mode_update_notifier);
	sysbusy_register_notifier(&ecs_sysbusy_notifier);

	ecs_governor_register(&stage_gov, ecs.default_gov);

	return 0;
}
