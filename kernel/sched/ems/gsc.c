/*
 * Exynos Mobile Scheduler Group Scheduling
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>
#include "../sched.h"
#include "../pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * GSC (Group Scheduling based on Cgroup)                                     *
 ******************************************************************************/
#define GSC_DEACTIVATED			(0)
#define GSC_ACTIVATED			(1)
#define GSC_DISABLED			(1024)
#define GSC_BOOSTED				(0)
#define DEFAULT_UP_THRESHOLD		(GSC_DISABLED)
#define DEFAULT_DOWN_THRESHOLD		(GSC_DISABLED)
#define DEFAULT_MIN_INTERVAL_NS		(16 * NSEC_PER_MSEC)
#define DEFAULT_MIN_RETAIN_NS		(16 * NSEC_PER_MSEC)

struct gsc_group {
	int			id;
	int			enabled;
	int			status;
	u64			last_update_time;
	u64			deactivated_t;
	u64			activated_t;
	raw_spinlock_t		lock;
} *gsc_groups[CGROUP_COUNT];

struct gsc_stat {
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int min_interval_ns;
	unsigned int min_retain_ns;
} gsc;

static DEFINE_SPINLOCK(gsc_update_lock);

static struct gsc_group *get_gsc_group(struct task_struct *p)
{
	int index = cpuctl_task_group_idx(p);

	return gsc_groups[index];
}

bool is_gsc_task(struct task_struct *p)
{
	struct gsc_group *grp = get_gsc_group(p);

	if (gsc.up_threshold == GSC_DISABLED)
		return false;

	if (!grp->enabled)
		return false;

	if (gsc.up_threshold == GSC_BOOSTED)
		return true;

	return grp->status == GSC_ACTIVATED;
}

void gsc_update_status(struct gsc_group *grp, u64 group_load, u64 now)
{
	s64 delta;

	if (grp->status == GSC_DEACTIVATED) {
		if (group_load > gsc.up_threshold)
			grp->status = GSC_ACTIVATED;
		goto out;
	}

	/* GSC is activated here */
	if (group_load < gsc.down_threshold) {
		if (!grp->deactivated_t) {
			grp->deactivated_t = grp->last_update_time;
			goto out;
		}

		delta = grp->last_update_time - grp->deactivated_t;

		if (delta >	gsc.min_retain_ns) {
			grp->deactivated_t = 0;
			grp->status = GSC_DEACTIVATED;
		}
	} else if (grp->deactivated_t)
		grp->deactivated_t = 0;

out:
	grp->last_update_time = now;
}

static u64 lookup_table[MLT_PERIOD_COUNT] = { 1024, 922, 829, 746, 672, 605, 544, 490 };

static int _gsc_get_cpu_group_load(struct gsc_group *grp, int cpu)
{
	int i, curr = mlt_cur_period(cpu);
	u64 group_load = 0, load;

	/* calc average group load */
	for (i = 0; i < MLT_PERIOD_COUNT; i++) {
		load = mult_frac(mlt_art_cgroup_value(cpu, curr, grp->id), lookup_table[i],
				SCHED_CAPACITY_SCALE);

		group_load += load;

		curr--;
		if (curr < 0)
			curr = MLT_PERIOD_COUNT -1;
	}

	group_load = div64_u64(group_load, MLT_PERIOD_COUNT);

	return group_load;
}

static void _gsc_decision_activate(struct gsc_group *grp, u64 now)
{
	int cpu, prev_status = grp->status;
	u64 group_load = 0, nr_cpus = 0, load;

	if (now < grp->last_update_time + gsc.min_interval_ns)
		return;

	if (gsc.up_threshold == GSC_BOOSTED) {
		grp->status = GSC_ACTIVATED;
		grp->deactivated_t = 0;
		grp->activated_t = now;
		grp->last_update_time = now;

		trace_gsc_decision_activate(grp->id, group_load, now, grp->last_update_time, "boosted");
		return;
	}

	/* calc cpu's group load */
	for_each_cpu_and(cpu, cpu_active_mask, ecs_cpus_allowed(NULL)) {
		load = mult_frac(_gsc_get_cpu_group_load(grp, cpu), capacity_cpu_orig(cpu),
				SCHED_CAPACITY_SCALE);

		group_load += load;
		nr_cpus++;

		trace_gsc_cpu_group_load(cpu, grp->id, load);
	}

	if (unlikely(!nr_cpus))
		return;

	group_load = div64_u64(group_load, nr_cpus);

	gsc_update_status(grp, group_load, now);

	if (grp->status != prev_status)
		grp->activated_t = (grp->status == GSC_ACTIVATED) ? now : 0;

	trace_gsc_decision_activate(grp->id, group_load, now, grp->last_update_time,
					  grp->status ? "activated" : "deactivated");
}

static void gsc_decision_activate(struct gsc_group *grp, u64 now)
{
	raw_spin_lock(&grp->lock);
	_gsc_decision_activate(grp, now);
	raw_spin_unlock(&grp->lock);
}

void gsc_update_tick(void)
{
	int i;
	u64 now;

	if (!spin_trylock(&gsc_update_lock))
		return;

	now = sched_clock();

	if (gsc.up_threshold == GSC_DISABLED)
		goto unlock;

	for (i = 0; i < CGROUP_COUNT; i++) {
		struct gsc_group *grp = gsc_groups[i];

		if (!grp->enabled)
			continue;

		if (now < grp->last_update_time + gsc.min_interval_ns)
			continue;


		gsc_decision_activate(grp, now);
	}

unlock:
	spin_unlock(&gsc_update_lock);
}

/**********************************************************************
 *			    SYSFS support			      *
 **********************************************************************/
static struct kobject *gsc_kobj;

/* up_threshold */
static ssize_t show_up_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gsc.up_threshold);
}

static ssize_t store_up_threshold(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc.up_threshold = input;

	return count;
}

static struct kobj_attribute up_threshold_attr =
__ATTR(up_threshold, 0644, show_up_threshold, store_up_threshold);

/* down_threshold */
static ssize_t show_down_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gsc.down_threshold);
}

static ssize_t store_down_threshold(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc.down_threshold = input;

	return count;
}

static struct kobj_attribute down_threshold_attr =
__ATTR(down_threshold, 0644, show_down_threshold, store_down_threshold);

/* min_interval_ns */
static ssize_t show_min_interval_ns(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d\n", gsc.min_interval_ns);
}

static ssize_t store_min_interval_ns(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc.min_interval_ns = input;

	return count;
}

static struct kobj_attribute min_interval_ns_attr =
__ATTR(min_interval_ns, 0644, show_min_interval_ns, store_min_interval_ns);

/* min_retain_ns */
static ssize_t show_min_retain_us(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d\n", gsc.min_retain_ns);
}

static ssize_t store_min_retain_us(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc.min_retain_ns = input;

	return count;
}

static struct kobj_attribute min_retain_us_attr =
__ATTR(min_retain_ns, 0644, show_min_retain_us, store_min_retain_us);

static int gsc_sysfs_init(struct kobject *ems_kobj)
{
	int ret;

	gsc_kobj = kobject_create_and_add("gsc", ems_kobj);
	if (!gsc_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(gsc_kobj, &min_interval_ns_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &up_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &down_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &min_retain_us_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	return ret;
}

static void init_gsc_stat(void)
{
	gsc.up_threshold = DEFAULT_UP_THRESHOLD;
	gsc.down_threshold = DEFAULT_DOWN_THRESHOLD;
	gsc.min_interval_ns = DEFAULT_MIN_INTERVAL_NS;
	gsc.min_retain_ns = DEFAULT_MIN_RETAIN_NS;
}

static void init_gsc_group(void)
{
	struct gsc_group *grp;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++) {
		grp = kzalloc(sizeof(struct gsc_group), GFP_ATOMIC | GFP_NOWAIT);
		if (!grp) {
			pr_err("%s: Fail to allocate memory for gsc. grp=%p\n", __func__, grp);
			WARN_ON(!grp);
			return;
		}

		grp->id = i;
		grp->enabled = (i == CGROUP_TOPAPP) ? 1 : 0;
		grp->status = GSC_DEACTIVATED;
		grp->last_update_time = 0;
		grp->deactivated_t = 0;
		grp->activated_t = 0;
		raw_spin_lock_init(&grp->lock);

		gsc_groups[i] = grp;
	}
}

static int gsc_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		gsc_groups[i]->enabled = cur_set->gsc.enabled[i];

	gsc.up_threshold = cur_set->gsc.up_threshold;
	gsc.down_threshold = cur_set->gsc.down_threshold;

	return NOTIFY_OK;
}

static struct notifier_block gsc_emstune_notifier = {
	.notifier_call = gsc_emstune_notifier_call,
};

void gsc_init(struct kobject *ems_kobj)
{
	gsc_sysfs_init(ems_kobj);

	init_gsc_stat();

	init_gsc_group();

	emstune_register_notifier(&gsc_emstune_notifier);

	pr_info("EMS: Initialized GSC\n");
}
