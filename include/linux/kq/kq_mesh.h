/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * KQ(Kernel Quality) MESH driver implementation
 *  : Jaecheol Kim <jc22.kim@samsung.com>
 *    ChangHwi Seo <c.seo@samsung.com>
 */

#ifndef __KQ_MESH_H__
#define __KQ_MESH_H__

#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/smp.h>

#define KQ_MESH_IS_DRV_LOADING		(0x12344321)

#define KQ_MESH_ECC_INIT_VAR		(0)

#define KQ_MESH_VALID_PROCESS_MESH	("com.sec.sq.mesh")
#define KQ_MESH_VALID_PROCESS_BPS	("bps.ondevicebps")
#define KQ_MESH_VALID_PROCESS_LEN	(15)

/*
 * list of sysfs nodes supported in /sys/class/sec/sec_kq_mesh/
 * - func : functions by supported in mesh kernel
 * - result : result last function
 */
enum {
	KQ_MESH_SYSFS_FUNC = 0,
	KQ_MESH_SYSFS_RESULT,
	KQ_MESH_SYSFS_PANIC,
	KQ_MESH_SYSFS_SUPPORT,

	KQ_MESH_SYSFS_END,
};

/*
 * list of functions in MESH func path : /sys/class/sec/sec_kq_mesh/support
 */
enum {
	KQ_MESH_SUPPORT_INIT = 0,
	KQ_MESH_SUPPORT_ECC,
	KQ_MESH_SUPPORT_USER_NAD,

	KQ_MESH_SUPPORT_END,
};

/*
 * list of functions in MESH func path : /sys/class/sec/sec_kq_mesh/func
 * - ecc-checker
 */
enum {
	KQ_MESH_FEATURE_INIT = -1,
	KQ_MESH_FEATURE_ECC_CHECKER = 0,

	KQ_MESH_FEATURE_END,
};


static ssize_t kq_mesh_show_attrs(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t kq_mesh_store_attrs(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

#define KQ_MESH_ATTR(_name)	\
{	\
	.attr = { .name = #_name, .mode = 0664 },	\
	.show = kq_mesh_show_attrs,	\
	.store = kq_mesh_store_attrs,	\
}

/*
 * sdev : /sys/class/sec/ device class pointer
 * features : support features
 * ecc_max_cpus : number of ecc cpu counters
 */
struct kq_mesh_info {
	struct device *sdev;

	/* ecc function */
	unsigned int cpu;

	/* last run function */
	unsigned int last_func;

	/* user nad function */
	void *user_nad_info;

	/* list of mesh support func */
	unsigned int support;
};

/*
 * name : name of function in sysfs
 * type : test type index
 * func : real test function
 */
struct kq_mesh_func_name {
	const char *name;
	int type;
	void (*func)(struct kq_mesh_info *kminfo);
};

static void kq_mesh_func_ecc_checker(struct kq_mesh_info *kminfo);

#endif /* __KQ_MESH_H__ */
