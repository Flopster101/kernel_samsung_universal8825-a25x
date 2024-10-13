/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/exynos_drm_recovery.h
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for recovery Feature.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_RECOVERY__
#define __EXYNOS_DRM_RECOVERY__

#include <linux/kthread.h>

#define RECOVERY_NAME_LEN	16
#define RECOVERY_MODE_MAX	16
#define INTERNAL_MAX_QUEUE_SZ	64
#define RECOVERY_REQ_ALL	"dsim|customer"
#define RECOVERY_REQ_LSI	"dsim"
#define RECOVERY_REQ_FORCE	"force"
#define RECOVERY_REQ_CUSTOMER	"customer"

enum recovery_state {
	RECOVERY_NOT_SUPPORTED = 0,
	RECOVERY_IDLE,
	RECOVERY_TRIGGER,
	RECOVERY_BEGIN,
	RECOVERY_RESTORE,
	RECOVERY_SMAX,
	RECOVERY_STATE_NUM,
};

enum recovery_mode_idx {
	RECOVERY_MODE_IDX_FORCE = 0,
	RECOVERY_MODE_IDX_DSIM,
	RECOVERY_MODE_IDX_CUSTOMER,
	RECOVERY_MODE_IDX_ALWAYS,
	RECOVERY_MODE_IDX_MAX,

	RECOVERY_RANGE_IDX_DPU = 16,
	RECOVERY_RANGE_IDX_PANEL,
	RECOVERY_RANGE_IDX_MAX = 31,
};

struct decon_device;
struct exynos_recovery;

struct exynos_recovery_funcs {
	void (*set_state)(struct exynos_recovery *recovery, enum recovery_state state);
	enum recovery_state (*get_state)(struct exynos_recovery *recovery);
	void (*set_mode)(struct exynos_recovery *recovery, char *mode);
};

struct recovery_queue {
	ktime_t queue[INTERNAL_MAX_QUEUE_SZ];
	int front;
	int rear;
	int fsz;
};

struct exynos_recovery_cond {
	int id;
	char name[RECOVERY_NAME_LEN];
	int count;
	int max_recovery_count;
	int limits_num_in_time;
	ktime_t in_time;
	struct recovery_queue rq;
	bool refresh_panel;
	bool reset_vblank;
	bool reset_phy;
	bool (*func)(const struct drm_crtc *crtc);
};

struct exynos_recovery {
	struct work_struct work;

	int modes;
	int count;
	bool always;
	int always_id;

	struct mutex r_lock;
	u32 req_mode;
	u32 max_req_mode;
	int req_mode_id;
	enum recovery_state r_state;

	struct exynos_recovery_cond r_cond[RECOVERY_MODE_MAX];
	const struct exynos_recovery_funcs *funcs;
};

/*
 * add condition check prototype
 */
bool dsim_condition_check(const struct drm_crtc *crtc);
bool customer_condition_check(const struct drm_crtc *crtc);

void exynos_recovery_register(struct decon_device *decon);
int exynos_recovery_set_state(struct decon_device *decon, enum recovery_state state);
enum recovery_state exynos_recovery_get_state(struct decon_device *decon);
int exynos_recovery_set_mode(struct decon_device *decon, char *mode);


#endif /* __EXYNOS_DRM_RECOVERY__ */
