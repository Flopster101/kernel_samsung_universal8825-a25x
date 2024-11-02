/*
 * Exynos Core Sparing Governor - Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 * Youngtae Lee <yt0729.lee@samsung.com>
 */

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static struct {
	const struct cpumask		*cpus;

	struct list_head		domain_list;
	struct cpumask			governor_cpus;
	int				governor_disabled;
	unsigned int			default_gov;
	unsigned long			last_update_jiffies;

	int cpu_ar[VENDOR_NR_CPUS];

	struct kobject *kobj;

} dynamic;

static DEFINE_RAW_SPINLOCK(dynamic_lock);

/* Governor Disabled Type */
#define GOV_CTRL_BY_USER	(1 << 0)
#define GOV_CTRL_BY_SYSBUSY	(1 << 1)
#define GOV_CTRL_BY_RATIO	(1 << 2)
#define GOV_CTRL_BY_INIT	(1 << 3)

/* Domain status */
#define ECS_OPTIMIZED		(0)
#define ECS_UNDER_NRRUN		(1 << 0)
#define	ECS_OVER_NRRUN		(1 << 1)	/* avg nr_running overs 1 */
#define ECS_HAS_IDLECPU		(1 << 2)	/* idle cpu with avg_nr_run less than 0.5 */
#define ECS_AR_UNDERLOAD	(1 << 3)	/* avg active ratio unders 35% */
#define ECS_AR_OVERLOAD		(1 << 4)	/* avg active ratio overs 70% */
#define ECS_UTIL_OVERLOAD	(1 << 5)	/* util overs 70% of capacity */
#define	ECS_MISFITED		(1 << 6)	/* slower has a misfited task */

#define DEFAULT_BUSY_RATIO	(70)

static void update_governor_cpus(const struct cpumask *new_cpus)
{
	struct dynamic_dom *domain;
	struct cpumask spared_cpus;

	/* update governor cpus */
	cpumask_copy(&dynamic.governor_cpus, new_cpus);

	/* update wether domain has spared_cpus or not */
	cpumask_complement(&spared_cpus, &dynamic.governor_cpus);
	list_for_each_entry(domain, &dynamic.domain_list, node)
		domain->has_spared_cpu = cpumask_intersects(&domain->cpus, &spared_cpus);

	update_ecs_cpus();
}

static struct dynamic_dom *dynamic_get_cpu_domain(int cpu)
{
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;
	}
	return NULL;
}

static struct dynamic_dom *dynamic_get_domain(int id)
{
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		if (domain->id == id)
			return domain;
	}
	return NULL;
}

/* Should be called with ECS LOCK */
static void dynamic_control_governor(int user, int enable)
{
	unsigned long flags;
	bool prev_mode, new_mode;

	raw_spin_lock_irqsave(&dynamic_lock, flags);

	prev_mode = dynamic.governor_disabled ? false : true;

	if (enable)
		dynamic.governor_disabled = dynamic.governor_disabled & (~user);
	else
		dynamic.governor_disabled = dynamic.governor_disabled | user;

	new_mode = dynamic.governor_disabled ? false : true;

	if (prev_mode == new_mode) {
		raw_spin_unlock_irqrestore(&dynamic_lock, flags);
		return;
	}

	trace_ecs_update_governor_cpus("governor-ctrl",
			*(unsigned int *)cpumask_bits(&dynamic.governor_cpus),
			*(unsigned int *)cpumask_bits(cpu_possible_mask));

	update_governor_cpus(cpu_possible_mask);

	raw_spin_unlock_irqrestore(&dynamic_lock, flags);

}

static void _dynamic_release_cpus(struct dynamic_dom *domain, int cnt)
{
	struct cpumask spare_cpus, target_cpus;
	int cpu;

	cpumask_copy(&target_cpus, &domain->governor_cpus);
	cpumask_andnot(&spare_cpus, &domain->cpus, &domain->governor_cpus);

	for_each_cpu(cpu, &spare_cpus) {
		 if (--cnt < 0)
			break;

		cpumask_set_cpu(cpu, &target_cpus);
	}

	cpumask_copy(&domain->governor_cpus, &target_cpus);
}

/*
 * It is fast track to release sapred cpus
 * when enqueue task and should increase frequency because can't use spared cpus,
 * release spared cpus imediately to avoid increase frequency
 */
static void dynamic_enqueue_release_cpus(int prev_cpu, struct task_struct *p)
{
	int cpu, min_cpu = 0, util_with, min_util = INT_MAX;
	struct dynamic_dom *domain;
	struct cpumask new_cpus;
	unsigned long flags;

	if (dynamic.governor_disabled)
		return;

	/* find min util cpus from available cpus */
	domain = dynamic_get_cpu_domain(prev_cpu);
	if (unlikely(!domain))
		return;

	if (!domain->has_spared_cpu)
		return;

	for_each_cpu(cpu, &domain->governor_cpus) {
		struct rq *rq = cpu_rq(cpu);
		int cpu_util = ml_cpu_util(cpu) + cpu_util_rt(rq) + cpu_util_dl(rq);

		if (cpu_util > min_util)
			continue;

		min_util = cpu_util;
		min_cpu = cpu;
	}

	util_with = min(ml_cpu_util_with(p, min_cpu), capacity_cpu_orig(min_cpu));
	util_with = util_with + (util_with >> 2);
	if (et_cur_cap(prev_cpu) >= util_with)
		return;

	raw_spin_lock_irqsave(&dynamic_lock, flags);

	/* release cpus */
	domain->throttle_cnt = 0;
	_dynamic_release_cpus(domain, 1);
	cpumask_or(&new_cpus, &dynamic.governor_cpus, &domain->governor_cpus);

	trace_ecs_update_governor_cpus("governor-enqueue",
			*(unsigned int *)cpumask_bits(&dynamic.governor_cpus),
			*(unsigned int *)cpumask_bits(&new_cpus));

	update_governor_cpus(&new_cpus);

	raw_spin_unlock_irqrestore(&dynamic_lock, flags);

}

static void dynamic_release_cpus(struct dynamic_dom *domain)
{
	int min_cnt, cnt = 0;

	/* Reset the throttle count */
	domain->throttle_cnt = 0;

	if (!domain->has_spared_cpu)
		return;

	/* Open cpu when cpu is overloaded */
	if (domain->flag & ECS_AR_OVERLOAD || domain->flag & ECS_UTIL_OVERLOAD)
		cnt++;

	/* if domain is busy or no available cpus, we should open more cpu to help slower */
	if (domain->flag & ECS_MISFITED) {
		if (domain->flag & ECS_UTIL_OVERLOAD || !domain->nr_cpu)
			cnt += ((domain->slower_misfit + 1) / 2);
	}

	min_cnt = max((domain->avg_nr_run - domain->nr_cpu), 0);
	cnt = max(min_cnt, cnt);

	_dynamic_release_cpus(domain, cnt);
}

static void dynamic_sustain_cpus(struct dynamic_dom *domain)
{
	if (domain->flag & ECS_AR_OVERLOAD || domain->flag & ECS_UTIL_OVERLOAD)
		domain->throttle_cnt = 0;
}

static void dynamic_throttle_cpus(struct dynamic_dom *domain)
{
	/* if domain is under load, throttle cpu more fast */
	int delta_cnt = domain->flag & ECS_AR_UNDERLOAD ? 2 : 1;

	if (!cpumask_weight(&domain->governor_cpus))
		return;

	if (cpumask_equal(&domain->governor_cpus, &domain->default_cpus))
		return;

	domain->throttle_cnt += delta_cnt;
	if (domain->throttle_cnt < 6)
		return;

	domain->throttle_cnt = 0;

	cpumask_clear_cpu(cpumask_last(&domain->governor_cpus),
					&domain->governor_cpus);
}

static void update_domain_cpus(struct dynamic_dom *domain, struct cpumask *new_cpus)
{
	if (domain->flag & ECS_OVER_NRRUN || domain->flag & ECS_MISFITED)
		dynamic_release_cpus(domain);
	else if (domain->flag & ECS_UNDER_NRRUN && domain->flag & ECS_HAS_IDLECPU)
		dynamic_throttle_cpus(domain);
	else
		dynamic_sustain_cpus(domain);

	cpumask_or(new_cpus, new_cpus, &domain->governor_cpus);
}

static int update_domain_info(struct dynamic_dom *domain, int slower_misfit_cnt)
{
	int nr_busy_cpu = 0, avg_nr_run = 0, misfit = 0, active_ratio= 0;
	unsigned long util = 0, cap = 0;
	int cpu, flag = ECS_OPTIMIZED;
	int thr_ratio = domain->busy_ratio * 10;

	for_each_cpu(cpu, &domain->cpus) {
		struct rq *rq = cpu_rq(cpu);
		int avg_nr, cur_ar = 0, prev_ar = 0, recent_ar = 0;
		int cur_idx = mlt_cur_period(cpu);
		int prev_idx = mlt_period_with_delta(cur_idx, -1);

		/* 1. compute average avg_nr_run */
		avg_nr = mlt_avg_nr_run(rq);
		avg_nr_run += avg_nr;

		/*
		 * 2. compute busy cpu
		 * If cpu average nr running overs 0.5, this cpu is busy
		 */
		nr_busy_cpu += min(((avg_nr + NR_RUN_ROUNDS_UNIT) / NR_RUN_UNIT), 1);

		/* 3. compute misfit */
		misfit += ems_rq_nr_misfited(rq);

		/* 4. compute util */
		util += (ml_cpu_util(cpu) + cpu_util_rt(rq));

		/* 5. compute average active ratio */
		cur_ar = mlt_art_value(cpu, cur_idx);
		prev_ar = mlt_art_value(cpu, prev_idx);
		recent_ar = mlt_art_recent(cpu);

		/*
		 * For fast response about recent active ratio, select higher value
		 * between prev active ratio and recent active ratio
		 */
		prev_ar = prev_ar > recent_ar ? prev_ar : recent_ar;
		active_ratio += dynamic.cpu_ar[cpu] = ((cur_ar + prev_ar) >> 1);

		/* 6. compute capacity of available cpu */
		if (!cpumask_test_cpu(cpu, &domain->governor_cpus))
			continue;
		cap += capacity_orig_of(cpu);
	}

	domain->nr_cpu = cpumask_weight(&domain->governor_cpus);
	domain->active_ratio = active_ratio /= domain->nr_cpu;
	avg_nr_run = (avg_nr_run + NR_RUN_UP_UNIT) / NR_RUN_UNIT;
	domain->avg_nr_run = avg_nr_run;
	domain->nr_busy_cpu = nr_busy_cpu;
	domain->slower_misfit = slower_misfit_cnt;
	domain->util = util;
	domain->cap = cap;

	/* nr_running is basic information */
	if (avg_nr_run > domain->nr_cpu)
		flag = ECS_OVER_NRRUN;
	else if (avg_nr_run < domain->nr_cpu)
		flag = ECS_UNDER_NRRUN;

	if (nr_busy_cpu < domain->nr_cpu)
		flag |= ECS_HAS_IDLECPU;

	if (active_ratio > thr_ratio)
		flag |= ECS_AR_OVERLOAD;
	else if (active_ratio < (thr_ratio >> 1))
		flag |= ECS_AR_UNDERLOAD;

	if (util > (cap * thr_ratio / 1000))
		flag |= ECS_UTIL_OVERLOAD;

	if (slower_misfit_cnt)
		flag |= ECS_MISFITED;

	domain->flag = flag;

	trace_dynamic_domain_info(cpumask_first(&domain->cpus), domain);

	return misfit;
}

static void update_dynamic_governor_cpus(void)
{
	struct dynamic_dom *domain;
	struct cpumask new_cpus;
	int prev_misfit = 0;

	cpumask_clear(&new_cpus);

	list_for_each_entry(domain, &dynamic.domain_list, node)
		prev_misfit = update_domain_info(domain, prev_misfit);

	list_for_each_entry_reverse(domain, &dynamic.domain_list, node)
		update_domain_cpus(domain, &new_cpus);

	if (cpumask_equal(&dynamic.governor_cpus, &new_cpus))
		return;

	trace_ecs_update_governor_cpus("governor",
			*(unsigned int *)cpumask_bits(&dynamic.governor_cpus),
			*(unsigned int *)cpumask_bits(&new_cpus));

	update_governor_cpus(&new_cpus);
}

/******************************************************************************
 *                       emstune mode update notifier                         *
 ******************************************************************************/
static int dynamic_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct dynamic_dom *dom;
	int prev_en, new_en = false;

	list_for_each_entry(dom, &dynamic.domain_list, node) {
		int cpu = cpumask_first(&dom->cpus);
		dom->busy_ratio = cur_set->ecs_dynamic.dynamic_busy_ratio[cpu];
		if (dom->busy_ratio)
			new_en = true;
	}

	prev_en = !(dynamic.governor_disabled & GOV_CTRL_BY_RATIO);
	if (new_en != prev_en)
		dynamic_control_governor(GOV_CTRL_BY_RATIO, new_en);

	return NOTIFY_OK;
}

static struct notifier_block dynamic_mode_update_notifier = {
	.notifier_call = dynamic_mode_update_callback,
};

/******************************************************************************
 *                      sysbusy state change notifier                         *
 ******************************************************************************/
static int dynamic_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;
	int enable;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	enable = (state == SYSBUSY_STATE0);
	dynamic_control_governor(GOV_CTRL_BY_SYSBUSY, enable);

	return NOTIFY_OK;
}

static struct notifier_block dynamic_sysbusy_notifier = {
	.notifier_call = dynamic_sysbusy_notifier_call,
};

/******************************************************************************
 *				SYSFS		                              *
 ******************************************************************************/
static ssize_t show_dynamic_dom(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct dynamic_dom *domain;

	if (list_empty(&dynamic.domain_list)) {
		ret += sprintf(buf + ret, "domain list empty\n");
		return ret;
	}

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		ret += sprintf(buf + ret, "[domain%d]\n", domain->id);
		ret += sprintf(buf + ret, "cpus : %*pbl\n",
					cpumask_pr_args(&domain->cpus));
		ret += sprintf(buf + ret, "governor_cpus : %*pbl\n",
					cpumask_pr_args(&domain->governor_cpus));
		ret += sprintf(buf + ret, "emstune busy ratio : %u\n",
					domain->busy_ratio);
		ret += sprintf(buf + ret, " --------------------------------\n");
	}

	return ret;
}

static struct kobj_attribute dynamic_dom_attr =
__ATTR(domains, 0444, show_dynamic_dom, NULL);

static ssize_t show_dynamic_governor_disabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "disabled=%d\n", dynamic.governor_disabled);
}

static ssize_t store_dynamic_governor_disabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int disable;

	if (sscanf(buf, "%d", &disable) != 1)
		return -EINVAL;

	dynamic_control_governor(GOV_CTRL_BY_USER, !disable);

	return count;
}

static struct kobj_attribute dynamic_governor_enable_attr =
__ATTR(governor_disabled, 0644, show_dynamic_governor_disabled,
				store_dynamic_governor_disabled);

static ssize_t show_dynamic_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"domain=%d busy-ratio=%d\n",
				domain->id, domain->busy_ratio);
	}

	return ret;
}

static ssize_t store_dynamic_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int domain_id, ratio;
	struct dynamic_dom *domain;

	if (sscanf(buf, "%d %d", &domain_id, &ratio) != 2)
		return -EINVAL;

	if (domain_id < 0 || domain_id >= VENDOR_NR_CPUS)
		return -EINVAL;

	domain = dynamic_get_domain(domain_id);
	if (!domain)
		return -EINVAL;

	domain->busy_ratio = ratio;

	return count;
}

static struct kobj_attribute dynamic_ratio =
__ATTR(ratio, 0644, show_dynamic_ratio, store_dynamic_ratio);

static int dynamic_sysfs_init(struct kobject *ecs_gov_kobj)
{
	int ret;

	dynamic.kobj = kobject_create_and_add("dynamic", ecs_gov_kobj);
	if (!dynamic.kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(dynamic.kobj, &dynamic_dom_attr.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);
	ret = sysfs_create_file(dynamic.kobj, &dynamic_governor_enable_attr.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);
	ret = sysfs_create_file(dynamic.kobj, &dynamic_ratio.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);

	return ret;
}

/******************************************************************************
 *                               initialization                               *
 ******************************************************************************/
static int
init_dynamic_dom(struct device_node *dn, struct list_head *domain_list, int domain_id)
{
	int ret = 0;
	const char *buf;
	struct dynamic_dom *domain;

	domain = kzalloc(sizeof(struct dynamic_dom), GFP_KERNEL);
	if (!domain) {
		pr_err("%s: fail to alloc dynamic domain\n", __func__);
		return -ENOMEM;
	}

	if (!of_property_read_string(dn, "default-cpus", &buf))
		cpulist_parse(buf, &domain->default_cpus);

	if (!of_property_read_string(dn, "cpus", &buf))
		cpulist_parse(buf, &domain->cpus);

	domain->id = domain_id;
	domain->busy_ratio = DEFAULT_BUSY_RATIO;
	list_add_tail(&domain->node, domain_list);

	return ret;
}

static int init_dynamic_dom_list(void)
{
	int ret = 0, domain_id = 0;
	struct device_node *dn, *domain_dn;

	dn = of_find_node_by_path("/ems/ecs_dynamic");
	if (!dn) {
		pr_err("%s: fail to get dynamic.device node\n", __func__);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&dynamic.domain_list);

	for_each_child_of_node(dn, domain_dn) {
		if (init_dynamic_dom(domain_dn, &dynamic.domain_list, domain_id)) {
			ret = -EINVAL;
			goto finish;
		}
		domain_id++;
	}

	of_property_read_u32(dn, "default-gov", &dynamic.default_gov);

finish:
	return ret;
}

static void dynamic_start(const struct cpumask *cpus)
{
	dynamic.cpus = cpus;
	dynamic_control_governor(GOV_CTRL_BY_USER, true);
}

#define UPDATE_PERIOD	(1)		/* 1 jiffies */
static void dynamic_update(void)
{
	unsigned long now = jiffies;

	if (!raw_spin_trylock(&dynamic_lock))
		return;

	if (dynamic.governor_disabled)
		goto out;

	if (now < (dynamic.last_update_jiffies + UPDATE_PERIOD))
		goto out;

	update_dynamic_governor_cpus();

	dynamic.last_update_jiffies = now;

out:
	raw_spin_unlock(&dynamic_lock);
}

static void dynamic_stop(void)
{
	dynamic_control_governor(GOV_CTRL_BY_USER, false);
	dynamic.cpus = NULL;
}

static const struct cpumask *dynamic_get_target_cpus(void)
{
	return &dynamic.governor_cpus;
}

static struct ecs_governor dynamic_gov = {
	.name = "dynamic",
	.update = dynamic_update,
	.enqueue_update = dynamic_enqueue_release_cpus,
	.start = dynamic_start,
	.stop = dynamic_stop,
	.get_target_cpus = dynamic_get_target_cpus,
};

int ecs_gov_dynamic_init(struct kobject *ems_kobj)
{
	if (init_dynamic_dom_list()) {
		pr_info("ECS governor will not affect scheduler\n");

		/* Reset domain list and dynamic.out_of_governing_cpus */
		INIT_LIST_HEAD(&dynamic.domain_list);
		/* disable governor */
		dynamic_control_governor(GOV_CTRL_BY_INIT, false);
	}

	dynamic_control_governor(GOV_CTRL_BY_USER, false);

	dynamic_sysfs_init(ecs_get_governor_object());
	emstune_register_notifier(&dynamic_mode_update_notifier);
	sysbusy_register_notifier(&dynamic_sysbusy_notifier);

	ecs_governor_register(&dynamic_gov, dynamic.default_gov);

	return 0;
}
