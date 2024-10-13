// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-common-init.h"
#include "dsp-hw-p0-system.h"
#include "dsp-hw-p0-memory.h"
#include "dsp-hw-common-pm.h"
#include "dsp-hw-p0-pm.h"

static const unsigned int dnc_table[] = {
	1066000,
	1066000,
	1066000,
	935000,
	800000,
	663000,
	533000,
	332000,
	267000,
	160000,
	80000,
};

static const unsigned int dsp_table[] = {
	1400000,
	1200000,
	1066000,
	935000,
	800000,
	663000,
	533000,
	331000,
	266000,
	160000,
	80000,
};

static const unsigned int cl2_table[] = {
	//TODO
	1248000,
	1152000,
	960000,
	768000,
};

static const unsigned int cl1_table[] = {
	//TODO
	1152000,
	960000,
	768000,
};

static const unsigned int cl0_table[] = {
	//TODO
	1152000,
	960000,
	768000,
};

static const unsigned int int_table[] = {
	800000,
	663000,
	533000,
	400000,
	350000,
	200000,
	160000,
};

static const unsigned int mif_table[] = {
	3172000,
	2730000,
	2535000,
	2288000,
	2028000,
	1716000,
	1539000,
	1352000,
	1014000,
	845000,
	676000,
	546000,
	421000,
};

static int dsp_hw_p0_pm_update_freq_info(struct dsp_pm *pm, int id)
{
	int ret;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	if (id < 0 || id >= pm->devfreq_count) {
		ret = -EINVAL;
		dsp_err("devfreq id(%d) for dsp is invalid\n", id);
		goto p_err;
	}

	devfreq = &pm->devfreq[id];

	if (devfreq->id == DSP_P0_DEVFREQ_DNC) {
		dsp_ctrl_dhcp_writel(
				DSP_P0_DHCP_IDX(DSP_P0_DHCP_DNC_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
		dsp_ctrl_dhcp_writel(
				DSP_P0_DHCP_IDX(DSP_P0_DHCP_DSP_FREQUENCY),
				devfreq->table[devfreq->current_qos] / 1000);
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_p0_pm_probe(struct dsp_pm *pm, void *sys)
{
	int ret, idx;
	struct dsp_pm_devfreq *devfreq;

	dsp_enter();
	pm->devfreq_count = DSP_P0_DEVFREQ_COUNT;
	pm->extra_devfreq_count = DSP_P0_EXTRA_DEVFREQ_COUNT;

	ret = dsp_hw_common_pm_probe(pm, sys, NULL);
	if (ret) {
		dsp_err("Failed to probe common pm(%d)\n", ret);
		goto p_err;
	}

	devfreq = &pm->devfreq[DSP_P0_DEVFREQ_DNC];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "dnc");
	devfreq->count = sizeof(dnc_table) / sizeof(*dnc_table);
	devfreq->table = dnc_table;
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	devfreq->class_id = PM_QOS_DNC_THROUGHPUT;
#endif

	devfreq = &pm->devfreq[DSP_P0_DEVFREQ_DSP];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "dsp");
	devfreq->count = sizeof(dsp_table) / sizeof(*dsp_table);
	devfreq->table = dsp_table;
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	devfreq->class_id = PM_QOS_DSP_THROUGHPUT;
#endif

//TODO
//	devfreq = &pm->extra_devfreq[DSP_P0_EXTRA_DEVFREQ_CL2];
//	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl2");
//	devfreq->count = sizeof(cl2_table) / sizeof(*cl2_table);
//	devfreq->table = cl2_table;
//#ifdef CONFIG_EXYNOS_DSP_HW_P0
//#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
//	devfreq->use_freq_qos = true;
//	devfreq->class_id = 7;
//#else
//	devfreq->class_id = PM_QOS_CLUSTER2_FREQ_MIN;
//#endif
//#endif
//
//	devfreq = &pm->extra_devfreq[DSP_P0_EXTRA_DEVFREQ_CL1];
//	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl1");
//	devfreq->count = sizeof(cl1_table) / sizeof(*cl1_table);
//	devfreq->table = cl1_table;
//#ifdef CONFIG_EXYNOS_DSP_HW_P0
//#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
//	devfreq->use_freq_qos = true;
//	devfreq->class_id = 4;
//#else
//	devfreq->class_id = PM_QOS_CLUSTER1_FREQ_MIN;
//#endif
//#endif
//
//	devfreq = &pm->extra_devfreq[DSP_P0_EXTRA_DEVFREQ_CL0];
//	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "cl0");
//	devfreq->count = sizeof(cl0_table) / sizeof(*cl0_table);
//	devfreq->table = cl0_table;
//#ifdef CONFIG_EXYNOS_DSP_HW_P0
//#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
//	devfreq->use_freq_qos = true;
//	devfreq->class_id = 0;
//#else
//	devfreq->class_id = PM_QOS_CLUSTER0_FREQ_MIN;
//#endif
//#endif

	devfreq = &pm->extra_devfreq[DSP_P0_EXTRA_DEVFREQ_INT];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "int");
	devfreq->count = sizeof(int_table) / sizeof(*int_table);
	devfreq->table = int_table;
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	devfreq->class_id = PM_QOS_DEVICE_THROUGHPUT;
#endif

	devfreq = &pm->extra_devfreq[DSP_P0_EXTRA_DEVFREQ_MIF];
	snprintf(devfreq->name, DSP_DEVFREQ_NAME_LEN, "mif");
	devfreq->count = sizeof(mif_table) / sizeof(*mif_table);
	devfreq->table = mif_table;
#ifdef CONFIG_EXYNOS_DSP_HW_P0
	devfreq->class_id = PM_QOS_BUS_THROUGHPUT;
#endif

	for (idx = 0; idx < pm->devfreq_count; ++idx) {
		devfreq = &pm->devfreq[idx];

		devfreq->min_qos = devfreq->count - 1;
		devfreq->dynamic_qos = devfreq->min_qos;
		devfreq->static_qos = devfreq->min_qos;
	}

	for (idx = 0; idx < pm->extra_devfreq_count; ++idx) {
		devfreq = &pm->extra_devfreq[idx];

		devfreq->min_qos = devfreq->count - 1;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dsp_pm_ops p0_pm_ops = {
	.devfreq_active			= dsp_hw_common_pm_devfreq_active,
	.update_freq_info		= dsp_hw_p0_pm_update_freq_info,
	.update_devfreq_nolock		=
		dsp_hw_common_pm_update_devfreq_nolock,
	.update_devfreq_nolock_async	=
		dsp_hw_common_pm_update_devfreq_nolock_async,
	.update_extra_devfreq		= dsp_hw_common_pm_update_extra_devfreq,
	.set_force_qos			= dsp_hw_common_pm_set_force_qos,
	.dvfs_enable			= dsp_hw_common_pm_dvfs_enable,
	.dvfs_disable			= dsp_hw_common_pm_dvfs_disable,
	.update_devfreq_busy		= dsp_hw_common_pm_update_devfreq_busy,
	.update_devfreq_idle		= dsp_hw_common_pm_update_devfreq_idle,
	.update_devfreq_boot		= dsp_hw_common_pm_update_devfreq_boot,
	.update_devfreq_max		= dsp_hw_common_pm_update_devfreq_max,
	.update_devfreq_min		= dsp_hw_common_pm_update_devfreq_min,
	.set_boot_qos			= dsp_hw_common_pm_set_boot_qos,
	.boost_enable			= dsp_hw_common_pm_boost_enable,
	.boost_disable			= dsp_hw_common_pm_boost_disable,
	.enable				= dsp_hw_common_pm_enable,
	.disable			= dsp_hw_common_pm_disable,

	.open				= dsp_hw_common_pm_open,
	.close				= dsp_hw_common_pm_close,
	.probe				= dsp_hw_p0_pm_probe,
	.remove				= dsp_hw_common_pm_remove,
};

int dsp_hw_p0_pm_register_ops(void)
{
	int ret;

	dsp_enter();
	ret = dsp_hw_common_register_ops(DSP_DEVICE_ID_P0, DSP_HW_OPS_PM,
			&p0_pm_ops, sizeof(p0_pm_ops) / sizeof(void *));
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
