/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/exynos_drm_hibernation.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Display Hibernation Feature.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_HIBERNATION__
#define __EXYNOS_DRM_HIBERNATION__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/io.h>

struct exynos_drm_crtc;
struct decon_device;
struct dsim_device;
struct exynos_hibernation;
struct writeback_device;

struct exynos_hibernation_funcs {
	void (*enter)(struct exynos_hibernation *hiber);
	void (*exit)(struct exynos_hibernation *hiber);
	bool (*check)(struct exynos_hibernation *hiber);
};

struct exynos_hiber_profile {
	/* entry time to hibernation */
	ktime_t hiber_entry_time;
	/* total time in hibernation */
	s64 hiber_time;
	/* start time of profiling */
	ktime_t profile_start_time;
	/* total profiling time */
	s64 profile_time;
	/* hibernation entry count during profiling */
	u32 profile_enter_cnt;
	/* hibernation exit count during profiling */
	u32 profile_exit_cnt;
	/* if true, profiling of hibernation entry ratio will be started */
	bool started;
	u32 frame_cnt;
};

struct exynos_hibernation {
	atomic_t trig_cnt;
	atomic_t block_cnt;
	/* register to check whether camera is operating or not */
	void __iomem *cam_op_reg;
	struct mutex lock;
	struct kthread_work work;
	struct decon_device *decon;
	struct dsim_device *dsim;
	struct writeback_device *wb;
	const struct exynos_hibernation_funcs *funcs;

	bool early_wakeup_enable;
	struct task_struct *exit_thread;
	struct kthread_worker exit_worker;
	struct kthread_work exit_work;
	/* hibernation exit count only increasing through sysfs */
	u32 early_wakeup_cnt;

	struct exynos_hiber_profile profile;
};

static inline bool is_hibernaton_blocked(struct exynos_hibernation *hiber)
{
	return (atomic_read(&hiber->block_cnt) > 0);
}

static inline void hibernation_block(struct exynos_hibernation *hiber)
{
	if (!hiber)
		return;

	atomic_inc(&hiber->block_cnt);
}
void hibernation_block_exit(struct exynos_hibernation *hiber);

static inline void hibernation_unblock(struct exynos_hibernation *hiber)
{
	if (!hiber)
		return;

	atomic_add_unless(&hiber->block_cnt, -1, 0);
}

struct exynos_hibernation *
exynos_hibernation_register(struct exynos_drm_crtc *exynos_crtc);
void exynos_hibernation_destroy(struct exynos_hibernation *hiber);

#endif /* __EXYNOS_DRM_HIBERNATION__ */
