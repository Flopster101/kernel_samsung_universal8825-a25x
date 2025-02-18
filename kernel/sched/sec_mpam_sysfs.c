// SPDX-License-Identifier: GPL-2.0+
/*
 * Structs and functions to support MPAM CPBM
 *
 * Copyright (C) 2021 Samsung Electronics
 */

#include "sec_mpam_sysfs.h"
#include "sec_mpam_cpbm.h"

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <soc/samsung/exynos-cpupm.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPAM Driver");
MODULE_AUTHOR("Valentin Schneider <valentin.schneider@arm.com>");
MODULE_AUTHOR("Gyeonghwan Hong <gh21.hong@samsung.com>");
MODULE_AUTHOR("Sangkyu Kim <skwith.kim@samsung.com>");

static DEFINE_SPINLOCK(mpam_lock);
static void __iomem *mpam_base;

static DEFINE_RWLOCK(notifier_lock);
static RAW_NOTIFIER_HEAD(notifier_chain);

static int is_late_init = 0;

#define MPAMCFG_PART_SEL	(0x100)
#define MPAMCFG_CPBM		(0x1000)

/* mpam-prototype sysfs hierarchy:
 * /sys/kernel/mpam (mpam_root, mpam_entry_objs)
 *   |- control (mpam_entry_obj / mpam_entry_control_attrs)
 *   | |- late_init (ReadWrite)
 *   |- ROOT (mpam_entry_obj / mpam_entry_default_attrs)
 *   | |- cpbm0 (ReadWrite: high-priority)
 *   | |- cpbm1 (ReadWrite: low-priority)
 *   | |- attr (ReadOnly: register read)
 *   | |- submit (WriteOnly)
 *   |- background
 *   | |- cpbm0
 *   | |- cpbm1
 *   | |- attr
 *   | |- submit
 *   |- camera-daemon
 *   | |- ...
 */

static const char *mpam_entry_names[] = {
	"ROOT",		/* DEFAULT_PARTID */
	"background",
	"camera-daemon",
	"system-background",
	"foreground",
	"restricted",
	"top-app",
	"dexopt",
	"audio-app"
};

/* mpam_entry_obj, mpam_entry_attr START */
struct mpam_entry_obj {
	struct kobject kobj;
	int partid; /* mpam_entry_default_groups */
	mpam_cpbm_var_t cpbm0; /* mpam_entry_default_groups */
	mpam_cpbm_var_t cpbm1; /* mpam_entry_default_groups */
	mpam_cpbm_var_t cpbm_submitted; /* mpam_entry_default_groups */
};
#define to_mpam_entry_obj(x) container_of(x, struct mpam_entry_obj, kobj)
struct mpam_entry_attr {
	struct attribute attr;
	ssize_t (*show)(
			struct mpam_entry_obj *mpam_entry_obj, struct mpam_entry_attr *attr, char *buf);
	ssize_t (*store)(
			struct mpam_entry_obj *mpam_entry_obj, struct mpam_entry_attr *attr,
			const char *buf, size_t count);
};
#define to_mpam_entry_attr(x) container_of(x, struct mpam_entry_attr, attr)
/* mpam_entry_obj, mpam_entry_attr END */

/* mpam_entry_obj operations START */
static ssize_t mpam_entry_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct mpam_entry_attr *mpam_entry_attr;
	struct mpam_entry_obj *mpam_entry_obj;

	mpam_entry_attr = to_mpam_entry_attr(attr);
	mpam_entry_obj = to_mpam_entry_obj(kobj);

	if(!mpam_entry_attr->show)
		return -EIO;
	return mpam_entry_attr->show(mpam_entry_obj, mpam_entry_attr, buf);
}
static ssize_t mpam_entry_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	struct mpam_entry_attr *mpam_entry_attr;
	struct mpam_entry_obj *mpam_entry_obj;

	mpam_entry_attr = to_mpam_entry_attr(attr);
	mpam_entry_obj = to_mpam_entry_obj(kobj);

	if(!mpam_entry_attr->store)
		return -EIO;
	return mpam_entry_attr->store(mpam_entry_obj, mpam_entry_attr, buf, len);
}

static const struct sysfs_ops mpam_entry_ops = {
	.show = mpam_entry_attr_show,
	.store = mpam_entry_attr_store,
};
static void mpam_entry_release(struct kobject *kobj)
{
	struct mpam_entry_obj *mpam_entry_obj;
	mpam_entry_obj = to_mpam_entry_obj(kobj);
	kfree(mpam_entry_obj);
}
/* mpam_entry_obj operations END */

/* cpbm attribute START */
#define __PAGE_SIZE 4096
static ssize_t cpbm_show(struct mpam_entry_obj *, struct mpam_entry_attr *, char *);
static ssize_t cpbm_store(struct mpam_entry_obj *, struct mpam_entry_attr *, const char *, size_t);

static struct mpam_entry_attr cpbm0_attr = __ATTR(cpbm0, 0664, cpbm_show, cpbm_store);
static struct mpam_entry_attr cpbm1_attr = __ATTR(cpbm1, 0664, cpbm_show, cpbm_store);

static mpam_cpbm_var_t attr_to_cpbm(struct mpam_entry_attr *attr, struct mpam_entry_obj *obj) {
	if(attr == &cpbm0_attr)
		return obj->cpbm0;
	else if(attr == &cpbm1_attr)
		return obj->cpbm1;
	else
		return NULL;
}
static ssize_t cpbm_show(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, char *buf)
{
	/*
	 * bitmap -> bit-range
	 * If the bitmap is complicated and its bit-range string size is over one page,
	 * the bit-range string can be truncated.
	 */
	mpam_cpbm_var_t cpbm;

	cpbm = attr_to_cpbm(attr, obj);
	if(cpbm == NULL) {
		/* cpbm not configured or not found */
		return snprintf(buf, __PAGE_SIZE, "\n");
	} else {
		/* cpbm configured */
		return snprintf(buf, __PAGE_SIZE, "%*pbl\n", mpam_cpbm_pr_args(cpbm));
	}
}
static ssize_t cpbm_store(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, const char *buf, size_t count)
{
	/* bit-range -> bitmap */
	mpam_cpbm_var_t cpbm;

	cpbm = attr_to_cpbm(attr, obj);
	if(cpbm == NULL)
		return -EINVAL;
	else {
		int ret;
		ret = mpam_cpbm_parse(buf, cpbm);
		return (ret == 0) ? count : -EIO;
	}
}
/* cpbm attribute END */

/* attr attribute START */
static ssize_t attr_show(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, char *buf)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&mpam_lock, flags);
	__raw_writel(obj->partid, mpam_base + MPAMCFG_PART_SEL);
	ret = snprintf(buf, __PAGE_SIZE, "%ld\n", __raw_readl(mpam_base + MPAMCFG_CPBM));
	spin_unlock_irqrestore(&mpam_lock, flags);

	return ret;
}
static struct mpam_entry_attr attr_attr = __ATTR(attr, 0664, attr_show, NULL);
/* cpbm attribute END */


/* submit attribute START */
static ssize_t submit_store(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, const char *buf, size_t count) 
{
	unsigned long *bit;
	unsigned long flags;

	/* if cpbm0 is not edited, cpbm1 will be submitted to the register */
	if(!mpam_cpbm_full(obj->cpbm0))
		obj->cpbm_submitted = obj->cpbm0;
	else
		obj->cpbm_submitted = obj->cpbm1;
	bit = __mpam_cpbm_bits(obj->cpbm_submitted);

	spin_lock_irqsave(&mpam_lock, flags);
	__raw_writel(obj->partid, mpam_base + MPAMCFG_PART_SEL);
	__raw_writel(*bit, mpam_base + MPAMCFG_CPBM);
	spin_unlock_irqrestore(&mpam_lock, flags);

	return count;
}
static struct mpam_entry_attr submit_attr = __ATTR(submit, 0220, NULL, submit_store);
/* submit attribute END */

/* late_init attribute START */
int mpam_late_init_notifier_register(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&notifier_lock, flags);
	ret = raw_notifier_chain_register(&notifier_chain, nb);
	write_unlock_irqrestore(&notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(mpam_late_init_notifier_register);

static ssize_t late_init_show(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, char *buf)
{
	int ret;

	ret = snprintf(buf, __PAGE_SIZE, "%d\n", is_late_init);
	return ret;
}

static ssize_t late_init_store(struct mpam_entry_obj *obj, struct mpam_entry_attr *attr, const char *buf, size_t count) 
{
	if (is_late_init == 0) {
		is_late_init = 1;
		read_lock(&notifier_lock);
		raw_notifier_call_chain(&notifier_chain, is_late_init, NULL);
		read_unlock(&notifier_lock);
	}

	return count;
}

static struct mpam_entry_attr late_init_attr = __ATTR(late_init, 0664, late_init_show, late_init_store);
/* late_init attribute END */

/* mpam attribute groups START */
static struct attribute *mpam_entry_default_attrs[] = {
	&cpbm0_attr.attr,
	&cpbm1_attr.attr,
	&attr_attr.attr,
	&submit_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(mpam_entry_default);

static struct attribute *mpam_entry_control_attrs[] = {
	&late_init_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(mpam_entry_control);
/* mpam attribute groups END */

/* mpam hierarchy START */
static struct kobj_type mpam_default_ktype = {
	.sysfs_ops = &mpam_entry_ops,
	.release = mpam_entry_release,
	.default_groups = mpam_entry_default_groups
};
static struct kobj_type mpam_control_ktype = {
	.sysfs_ops = &mpam_entry_ops,
	.release = mpam_entry_release,
	.default_groups = mpam_entry_control_groups
};
#define NUM_MPAM_ENTRIES 9
static struct kset *mpam_root;
static struct mpam_entry_obj *mpam_entry_objs[NUM_MPAM_ENTRIES];
static struct mpam_entry_obj *mpam_control_entry_obj;
/* mpam hierarchy END */
static struct notifier_block cpu_power_mode_nb;

static void mpam_restore_attr()
{
	int i;
	unsigned long flags, *bit;
	struct mpam_entry_obj *mpam_obj;

	spin_lock_irqsave(&mpam_lock, flags);
	for (i = 0; i < NUM_MPAM_ENTRIES; ++i) {
		mpam_obj = mpam_entry_objs[i];

		if (mpam_obj == NULL)
			continue;

		bit = __mpam_cpbm_bits(mpam_obj->cpbm_submitted);

		if (bit == NULL)
			continue;

		__raw_writel(mpam_obj->partid, mpam_base + MPAMCFG_PART_SEL);
		__raw_writel(*bit, mpam_base + MPAMCFG_CPBM);
	}
	spin_unlock_irqrestore(&mpam_lock, flags);
}

static int mpam_cpu_power_mode_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	switch (event) {
		case SICD_EXIT:
		case DSUPD_EXIT:
			mpam_restore_attr();
			break;
		default:
			ret = NOTIFY_DONE;
	}

	return notifier_from_errno(ret);
}

static int mpam_cpu_power_mode_notifier_register()
{
	cpu_power_mode_nb.notifier_call = mpam_cpu_power_mode_notifier;
	cpu_power_mode_nb.next = NULL;
	cpu_power_mode_nb.priority = 0;

	return exynos_cpupm_notifier_register(&cpu_power_mode_nb);
}

/* mpam_entry_obj create/destroy START */
static struct mpam_entry_obj *create_mpam_entry_obj(
		const char *name, struct kobj_type *ktype)
{
	struct mpam_entry_obj *entry = NULL;
	int ret;

	/* Allocate mpam_entry and its attribute values */
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if(!entry)
		goto kobj_alloc_error;

	entry->kobj.kset = mpam_root;

	ret = (int)alloc_mpam_cpbm_var(&entry->cpbm0, GFP_KERNEL);
	if(!ret)
		goto cpbm0_alloc_error;

	ret = (int)alloc_mpam_cpbm_var(&entry->cpbm1, GFP_KERNEL);
	if(!ret)
		goto cpbm1_alloc_error;

	ret = kobject_init_and_add(&entry->kobj, ktype, NULL, "%s", name);
	if(ret)
		goto kobj_init_error;

	/* Initialize attributes */
	mpam_cpbm_setall(entry->cpbm0);
	mpam_cpbm_setall(entry->cpbm1);
	entry->cpbm_submitted = NULL;

	kobject_uevent(&entry->kobj, KOBJ_ADD);
	return entry;

kobj_init_error:
	free_mpam_cpbm_var(entry->cpbm1);
cpbm1_alloc_error:
	free_mpam_cpbm_var(entry->cpbm0);
cpbm0_alloc_error:
	kobject_put(&entry->kobj);
kobj_alloc_error:
	return entry;
}

static void destroy_mpam_entry_obj(struct mpam_entry_obj *entry)
{
	free_mpam_cpbm_var(entry->cpbm1);
	free_mpam_cpbm_var(entry->cpbm0);
	kobject_put(&entry->kobj);
}
/* mpam_entry_obj create/destroy END */

static int mpam_init_remap()
{
	mpam_base = ioremap(0x1d810000, SZ_64K);
	if (mpam_base == NULL)
		return -ENODEV;

	return 0;
}

/* mpam_sysfs init/exit START */
int mpam_sysfs_init(void)
{
	const char *name;
	struct mpam_entry_obj *obj;
	int partid;
	int ret = -EINVAL;

	mpam_root = kset_create_and_add("mpam", NULL, kernel_kobj);
	if(!mpam_root) {
		ret = -ENOMEM;
		goto kset_error;
	}

	/* entries with mpam_control_ktype */
	name = "control";
	obj = create_mpam_entry_obj(name, &mpam_control_ktype);
	if(!obj)
		goto control_entry_error;
	mpam_control_entry_obj = obj;

	/* entries with mpam_default_ktype */
	for(partid = 0; partid < NUM_MPAM_ENTRIES; partid++) {
		name = mpam_entry_names[partid];

		obj = create_mpam_entry_obj(name, &mpam_default_ktype);
		if(!obj)
			goto default_entry_error;
		obj->partid = partid;
		mpam_entry_objs[partid] = obj;
	}

	/* remap */
	ret = mpam_init_remap();
	if (ret) {
		pr_info("MPAM : Couldn't initialized for remap");
		goto remap_error;
	}

	ret = mpam_cpu_power_mode_notifier_register();
	if (ret) {
		ret = -EINVAL;
		goto power_mode_notifier_error;
	}

	return 0;

power_mode_notifier_error:
remap_error:
default_entry_error:
	for(partid--; partid >= 0; partid--) {
		destroy_mpam_entry_obj(mpam_entry_objs[partid]);
	}
	destroy_mpam_entry_obj(mpam_control_entry_obj);
control_entry_error:
	kset_unregister(mpam_root);
kset_error:
	return ret;
}
EXPORT_SYMBOL_GPL(mpam_sysfs_init);

void mpam_sysfs_exit(void)
{
	int partid;
	for(partid = 0; partid < NUM_MPAM_ENTRIES; partid++) {
		destroy_mpam_entry_obj(mpam_entry_objs[partid]);
	}
	destroy_mpam_entry_obj(mpam_control_entry_obj);
	kset_unregister(mpam_root);
}
EXPORT_SYMBOL_GPL(mpam_sysfs_exit);
/* mpam_sysfs init/exit END */
