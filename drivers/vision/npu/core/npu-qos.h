/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_QOS_H_
#define _NPU_QOS_H_
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#else
#include <linux/pm_qos.h>
#endif
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <soc/samsung/bts.h>

#include "npu-scheduler.h"

#define NPU_QOS_DEFAULT_VALUE   (INT_MAX)

struct npu_qos_freq_lock {
	u32	npu_freq_maxlock;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	u32	dnc_freq_maxlock;
#endif
};

struct npu_qos_setting {
	struct mutex		npu_qos_lock;
	struct platform_device	*dvfs_npu_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	struct exynos_pm_qos_request	npu_qos_req_dnc;
	struct exynos_pm_qos_request	npu_qos_req_dnc_max;
#endif
	struct exynos_pm_qos_request	npu_qos_req_npu;
	struct exynos_pm_qos_request	npu_qos_req_dsp;
	struct exynos_pm_qos_request	npu_qos_req_mif;
	struct exynos_pm_qos_request	npu_qos_req_int;
	struct exynos_pm_qos_request	npu_qos_req_gpu;
	struct exynos_pm_qos_request	npu_qos_req_npu_max;
	struct exynos_pm_qos_request	npu_qos_req_dsp_max;
	struct exynos_pm_qos_request	npu_qos_req_mif_max;
	struct exynos_pm_qos_request	npu_qos_req_int_max;
	struct exynos_pm_qos_request	npu_qos_req_gpu_max;
	struct freq_qos_request			npu_qos_req_cpu_cl0;
	struct freq_qos_request			npu_qos_req_cpu_cl1;
	struct freq_qos_request			npu_qos_req_cpu_cl2;
	struct freq_qos_request			npu_qos_req_cpu_cl0_max;
	struct freq_qos_request			npu_qos_req_cpu_cl1_max;
	struct freq_qos_request			npu_qos_req_cpu_cl2_max;
	struct cpufreq_policy			*cl0_policy;
	struct cpufreq_policy			*cl1_policy;
	struct cpufreq_policy			*cl2_policy;
#else
	struct pm_qos_request	npu_qos_req_dnc;
	struct pm_qos_request	npu_qos_req_npu;
	struct pm_qos_request	npu_qos_req_dsp;
	struct pm_qos_request	npu_qos_req_mif;
	struct pm_qos_request	npu_qos_req_int;
	struct pm_qos_request	npu_qos_req_gpu;
	struct pm_qos_request	npu_qos_req_dnc_max;
	struct pm_qos_request	npu_qos_req_npu_max;
	struct pm_qos_request	npu_qos_req_dsp_max;
	struct pm_qos_request	npu_qos_req_mif_max;
	struct pm_qos_request	npu_qos_req_int_max;
	struct pm_qos_request	npu_qos_req_gpu_max;
	struct pm_qos_request	npu_qos_req_cpu_cl0;
	struct pm_qos_request	npu_qos_req_cpu_cl1;
	struct pm_qos_request	npu_qos_req_cpu_cl2;
	struct pm_qos_request	npu_qos_req_cpu_cl0_max;
	struct pm_qos_request	npu_qos_req_cpu_cl1_max;
	struct pm_qos_request	npu_qos_req_cpu_cl2_max;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	s32		req_dnc_freq;
#endif
	s32		req_cl0_freq;
	s32		req_cl1_freq;
	s32		req_cl2_freq;
	s32		req_npu_freq;
	s32		req_dsp_freq;
	s32		req_mif_freq;
	s32		req_int_freq;
	s32		req_gpu_freq;

	s32		req_mo_scen;
	u32		req_cpu_aff;

	u32		dsp_type;
	u32		dsp_max_freq;
	u32		npu_max_freq;

	struct notifier_block npu_qos_max_nb;
	bool		skip_max_noti;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	struct notifier_block npu_qos_dsp_min_nb;
#endif

	struct npu_scheduler_info *info;
};

struct npu_session_qos_req {
	s32		sessionUID;
	s32		req_freq;
	s32		req_mo_scen;
	__u32		eCategory;
	struct list_head list;
};

struct npu_system;
int npu_qos_probe(struct npu_system *system);
int npu_qos_release(struct npu_system *system);
int npu_qos_open(struct npu_system *system);
int npu_qos_close(struct npu_system *system);
npu_s_param_ret npu_qos_param_handler(struct npu_session *sess,
	struct vs4l_param *param, int *retval);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
extern void __npu_pm_qos_add_request(struct exynos_pm_qos_request *req,
					int exynos_pm_qos_class, s32 value);
extern void __npu_pm_qos_update_request(struct exynos_pm_qos_request *req,
							s32 new_value);
#else
extern void __npu_pm_qos_add_request(struct pm_qos_request *req,
					int pm_qos_class, s32 value);
extern void __npu_pm_qos_update_request(struct pm_qos_request *req, s32 value);
#endif

#endif	/* _NPU_QOS_H_ */
