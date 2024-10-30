/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#ifndef _UFS_PERF_H_
#define _UFS_PERF_H_

#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include "ufshcd.h"
#if IS_ENABLED(CONFIG_CPU_FREQ)
#include <linux/cpufreq.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
#include <soc/samsung/exynos_pm_qos.h>
#endif
#if IS_ENABLED(CONFIG_ARM_EXYNOS_ACME) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
#include <soc/samsung/freq-qos-tracer.h>
#endif

#include "ufs-exynos-perf-v1.h"
#include "ufs-exynos-gear.h"

enum {
	__TOKEN_FAIL,
	__TOKEN_NUM,
};

enum __traffic {
	TRAFFIC_NONE = 0,
	TRAFFIC_HIGH,
	TRAFFIC_LOW,
};

static const char *__res_token[__TOKEN_NUM] = {
	"fail to",
};

enum ufs_perf_op {
	UFS_PERF_OP_NONE = 0,
	UFS_PERF_OP_R,
	UFS_PERF_OP_W,
	UFS_PERF_OP_S,
	UFS_PERF_OP_MAX,
};

enum ufs_perf_ctrl {
	UFS_PERF_CTRL_NONE = 0,	/* Not used to run handler */
	UFS_PERF_CTRL_LOCK,
	UFS_PERF_CTRL_RELEASE,
};

enum ufs_perf_entry {
	UFS_PERF_ENTRY_QUEUED = 0,
	UFS_PERF_ENTRY_RESET,
};

/* private */
enum policy_res {
	R_OK = 0,
	R_CTRL,
};

enum {
	__UPDATE_V1 = 0,
	__UPDATE_GEAR,
	__UPDATE_MAX,
};
#define UPDATE_V1	BIT(__UPDATE_V1)
#define UPDATE_GEAR	BIT(__UPDATE_GEAR)

enum {
	__CTRL_REQ_DVFS = 0,
	__CTRL_REQ_WB,
	__CTRL_REQ_GEAR,
	__CTRL_REQ_MAX,
};
#define CTRL_REQ_DVFS	BIT(__CTRL_REQ_DVFS)
#define CTRL_REQ_WB	BIT(__CTRL_REQ_WB)

enum ctrl_op {
	CTRL_OP_NONE = 0,
	CTRL_OP_UP,
	CTRL_OP_DOWN,
};

struct ufs_perf {
	int id;

	/* stats chosen by externals */
	u32 stat_bits;

	/* interface from request to control */
	u32 ctrl_handle[__CTRL_REQ_MAX];
	spinlock_t lock_handle;

	/* handler */
	struct task_struct *handler;
	struct completion completion;	/* wake-up source */
	enum ctrl_op ctrl_state[__CTRL_REQ_MAX];

	/* sysfs */
	struct kobject sysfs_kobj;

	struct ufs_perf_stat_v1 stat_v1;
	struct ufs_perf_stat_v2 stat_v2;

	enum policy_res (*update[__UPDATE_MAX])(struct ufs_perf *perf, u32 qd,
						enum ufs_perf_op op,
						enum ufs_perf_entry);
	int (*ctrl[__CTRL_REQ_MAX])(struct ufs_perf *perf, enum ctrl_op op);

	/* knobs */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	struct exynos_pm_qos_request	pm_qos_int;
	s32			val_pm_qos_int;
	struct exynos_pm_qos_request	pm_qos_mif;
	s32			val_pm_qos_mif;
#endif
#if IS_ENABLED(CONFIG_CPU_FREQ) || IS_ENABLED(CONFIG_ARM_FREQ_QOS_TRACER)
	struct notifier_block cpufreq_nb;
	struct freq_qos_request pm_qos_cluster0;
	s32			val_pm_qos_cluster0;
	struct freq_qos_request pm_qos_cluster1;
	s32			val_pm_qos_cluster1;
	struct freq_qos_request pm_qos_cluster2;
	s32			val_pm_qos_cluster2;
#define MAX_CLUSTERS		3
	int num_clusters;
	unsigned int clusters[MAX_CLUSTERS];
#endif

	/* handle only for wb */
	struct ufs_hba *hba;

	u8 exynos_gear_scale;
};

/* EXTERNAL FUNCTIONS */
void ufs_perf_reset(void *data);
void ufs_perf_update(void *data, u32 qd, struct scsi_cmnd *cmd, enum ufs_perf_op op);
bool ufs_perf_init(void **data, struct ufs_hba *hba);
void ufs_perf_exit(void *data);
void ufs_init_cpufreq_request(struct ufs_perf *perf, bool add_noob);

/* from stats */
int ufs_perf_init_v1(struct ufs_perf *perf);
void ufs_perf_exit_v1(struct ufs_perf *perf);

void ufs_gear_scale_init(struct ufs_perf *perf);
int ufs_gear_change(struct ufs_hba *hba, bool en);

/* to stats */
void ufs_perf_complete(struct ufs_perf *perf);

#endif /* _UFS_PERF_H_ */
