// SPDX-License-Identifier: GPL-2.0+
/*
 * Module prototype to drive MPAM registers based on cgroup membership.
 *
 * Copyright (C) 2021 Arm Ltd.
 * Copyright (C) 2021 Samsung Electronics
 */

#include "sched.h"
#include "sec_mpam_sysfs.h"

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/cgroup-defs.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <asm/sysreg.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/cgroup.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPAM Driver");
MODULE_AUTHOR("Valentin Schneider <valentin.schneider@arm.com>");
MODULE_AUTHOR("Gyeonghwan Hong <gh21.hong@samsung.com>");
MODULE_AUTHOR("Sangkyu Kim <skwith.kim@samsung.com>");

//#define ENABLE_MPAM_DEBUG /* enabled for debugging */

#define SYS_MPAM1_EL1		sys_reg(3, 0, 10, 5, 0)
#define SYS_MPAM0_EL1		sys_reg(3, 0, 10, 5, 1)

#define MPAM_EN			(0x8000000000000000)
#define MPAM_PART_D_SHIFT	(16)
#define MPAM_PART_I_SHIFT	(0)

#define MPAM_PARTID_MAP_SIZE	(50)

/*
 * cgroup path -> PARTID map
 *
 * Could be made better with cgroupv2: we could have some files that userspace
 * could write into which would give us a {path, PARTID} pair. We could then
 * translate the path to a cgroup with cgroup_get_from_path(), and save the
 * css->id mapping then.
 */
static const int DEFAULT_PARTID = 0;
static char *mpam_path_partid_map[] = {
	"/",		/* DEFAULT_PARTID */
	"/background",
	"/camera-daemon",
	"/system-background",
	"/foreground",
	"/restricted",
	"/top-app",
	"/dexopt",
	"/audio-app"
};

/*
 * cgroup css->id -> PARTID cache
 *
 * Not sure how stable those IDs are supposed to be. If we are supposed to
 * support cgroups being deleted, we may need more hooks to cache that.
 */
static int mpam_css_partid_map[MPAM_PARTID_MAP_SIZE] = { [0 ... 49] = -1 };

/* The MPAM0_EL1.PARTID_D in use by a given CPU */
//static int __percpu *mpam_local_partid;

/* Get the css:partid mapping */
static int mpam_map_css_partid(struct cgroup_subsys_state *css)
{
	int partid;

	if (!css)
		goto no_match;

	if (css->id >= MPAM_PARTID_MAP_SIZE)
		goto no_match;

	/*
	 * The map is stable post init so no risk of two concurrent tasks
	 * cobbling each other
	 */
	partid = READ_ONCE(mpam_css_partid_map[css->id]);
	if (partid >= 0) {
#ifdef ENABLE_MPAM_DEBUG
		trace_printk("mpam_map_css_partid: css-id=%d, partid=%d\n", css->id, partid);
#endif
		return partid;
	}
#ifdef ENABLE_MPAM_DEBUG
		trace_printk("mpam_map_css_partid: css-id=%d, invalid partid=%d\n", css->id, partid);
#endif

no_match:
	/* No match, use sensible default */
	return DEFAULT_PARTID;
}

/*
 * Get the task_struct:partid mapping
 * This is the place to add special logic for a task-specific (rather than
 * cgroup-wide) PARTID.
 */
static int mpam_map_task_partid(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	int partid;

	rcu_read_lock();
	css = task_css(p, cpuset_cgrp_id);
	partid = mpam_map_css_partid(css);
	rcu_read_unlock();

	return partid;
}

/*
 * Write the PARTID to use on the local CPU.
 */
static void mpam_write_partid(int partid)
{
	long write_value = (partid << MPAM_PART_D_SHIFT) | (partid << MPAM_PART_I_SHIFT);

	write_sysreg_s(write_value, SYS_MPAM0_EL1);
	write_sysreg_s(write_value | MPAM_EN, SYS_MPAM1_EL1);

#ifdef ENABLE_MPAM_DEBUG
	trace_printk("mpam_write_partid: %ld\n", write_value);
#endif
}

/*
 * Sync @p's associated PARTID with this CPU's register.
 */
static void mpam_sync_task(struct task_struct *p)
{
	mpam_write_partid(mpam_map_task_partid(p));
}

/*
 * Same as mpam_sync_task(), with a pre-filter for the current task.
 */
static void mpam_sync_current(void *task)
{
	if (task && task != current)
		return;

	mpam_sync_task(current);
}

/*
 * Same as mpam_sync_current(), with an explicit mb for partid mapping changes.
 * Note: technically not required for arm64+GIC since we get explicit barriers
 * when raising and handling an IPI. See:
 * f86c4fbd930f ("irqchip/gic: Ensure ordering between read of INTACK and shared data")
 */
static void mpam_sync_current_mb(void *task)
{
	if (task && task != current)
		return;

	/* Pairs with smp_wmb() following mpam_cgroup_partid_map[] updates */
	smp_rmb();
	mpam_sync_task(current);
}

static bool __task_curr(struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

static void mpam_kick_task(struct task_struct *p)
{
	/*
	 * If @p is no longer on the task_cpu(p) we see here when the smp_call
	 * actually runs, then it had a context switch, so it doesn't need the
	 * explicit update - no need to chase after it.
	 */
	if (__task_curr(p))
		smp_call_function_single(task_cpu(p), mpam_sync_current, p, 1);
}

static void mpam_hook_attach(void __always_unused *data,
		struct cgroup_subsys *ss, struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct task_struct *p;

#ifdef ENABLE_MPAM_DEBUG
	trace_printk("mpam_hook_attach\n");
#endif

	if (ss->id != cpuset_cgrp_id)
		return;

	cgroup_taskset_first(tset, &css);
	cgroup_taskset_for_each(p, css, tset)
		mpam_kick_task(p);
}

static void mpam_hook_switch(void __always_unused *data,
		struct task_struct *prev, struct task_struct *next)
{
#ifdef ENABLE_MPAM_DEBUG
	trace_printk("mpam_hook_switch\n");
#endif
	mpam_sync_task(next);
}

/* Check if css' path matches any in mpam_path_partid_map and cache that */
static void __map_css_partid(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	int partid;

	cgroup_path(css->cgroup, tmp, pathlen);

	for (partid = 0; partid < ARRAY_SIZE(mpam_path_partid_map); partid++) {
		if (!strcmp(mpam_path_partid_map[partid], tmp)) {
			printk("cpuset:mpam mapping path=%s (css_id=%d) to partid=%d\n", tmp, css->id, partid);
			WRITE_ONCE(mpam_css_partid_map[css->id], partid);
		}
	}
}

/* Recursive DFS */
static void __map_css_children(struct cgroup_subsys_state *css, char *tmp, int pathlen)
{
	struct cgroup_subsys_state *child;

	list_for_each_entry_rcu(child, &css->children, sibling) {
		if (!child || !child->cgroup)
			continue;

		__map_css_partid(child, tmp, pathlen);
		__map_css_children(child, tmp, pathlen);
	}
}

static int mpam_init_cgroup_partid_map(void)
{
	struct cgroup_subsys_state *css;
	struct cgroup *cgroup;
	char buf[200];
	int ret = 0;

	rcu_read_lock();
	/*
	 * cgroup_get_from_path() would be much cleaner, but that seems to be v2
	 * only. Getting current's cgroup is only a means to get a cgroup handle,
	 * use that to get to the root. Clearly doesn't work if several roots
	 * are involved.
	 */
	cgroup = task_cgroup(current, cpuset_cgrp_id);
	if (IS_ERR(cgroup)) {
		ret = PTR_ERR(cgroup);
		goto out_unlock;
	}

	cgroup = &cgroup->root->cgrp;
	if (cpuset_cgrp_id >= CGROUP_SUBSYS_COUNT)
		goto out_unlock;

	css = rcu_dereference(cgroup->subsys[cpuset_cgrp_id]);
	if (IS_ERR_OR_NULL(css)) {
		ret = -ENOENT;
		goto out_unlock;
	}

	__map_css_partid(css, buf, 50);
	__map_css_children(css, buf, 50);

out_unlock:
	rcu_read_unlock();
	return ret;
}

static struct notifier_block late_init_nb;

static int mpam_late_init_notifier(struct notifier_block *nb,
		unsigned long late_init, void *data)
{
	int ret = NOTIFY_DONE;

	if (late_init) {
		mpam_init_cgroup_partid_map();
	}

	return notifier_from_errno(ret);
}

static int mpam_late_init_register()
{
	late_init_nb.notifier_call = mpam_late_init_notifier;
	late_init_nb.next = NULL;
	late_init_nb.priority = 0;

	return mpam_late_init_notifier_register(&late_init_nb);
}

static int __init sec_mpam_init(void)
{
	int ret;

	ret = register_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
	if (ret)
		goto out;

	ret = register_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	if (ret)
		goto out_attach;

	/*
	 * Ensure the partid map update is visible before kicking the CPUs.
	 * Pairs with smp_rmb() in mpam_sync_current_mb().
	 */
	smp_wmb();
	/*
	 * Hooks are registered, kick every CPU to force sync currently running
	 * tasks.
	 */
	smp_call_function(mpam_sync_current_mb, NULL, 1);

	ret = mpam_late_init_register();
	if (ret)
		goto out_late_init;

	mpam_init_cgroup_partid_map();

	mpam_sysfs_init();

	return 0;

out_late_init:
	unregister_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
out_attach:
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);
out:
	return ret;
}

static void mpam_reset_partid(void __always_unused *info)
{
	mpam_write_partid(DEFAULT_PARTID);
}

static void __exit sec_mpam_exit(void)
{
	mpam_sysfs_exit();

	unregister_trace_android_vh_is_fpsimd_save(mpam_hook_switch, NULL);
	unregister_trace_android_vh_cgroup_attach(mpam_hook_attach, NULL);

	smp_call_function(mpam_reset_partid, NULL, 1);
}

module_init(sec_mpam_init);
module_exit(sec_mpam_exit);
