// SPDX-License-Identifier: GPL-2.0-only
/*
 * Gear scale with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/of.h>
#include "ufs-exynos-perf.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#include "ufs-vs-regs.h"
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <linux/reboot.h>

#include <trace/events/ufs_exynos_perf.h>

static int exynos_ufs_reboot_handler(struct notifier_block *nb,	unsigned long l, void *p)
{
	struct ufs_perf_stat_v2 *stat = container_of(nb, struct ufs_perf_stat_v2, reboot_nb);

	stat->exynos_stat = UFS_EXYNOS_REBOOT;
	return 0;
}

static int exynos_gear_change_pmc(struct ufs_hba *hba, struct uic_pwr_mode *pmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	/* info gear update */
	hba->pwr_info.gear_rx = pmd->gear;
	hba->pwr_info.gear_tx = pmd->gear;
	hba->pwr_info.lane_rx = pmd->lane;
	hba->pwr_info.lane_tx = pmd->lane;
	hba->pwr_info.pwr_rx = FAST_MODE;
	hba->pwr_info.pwr_tx = FAST_MODE;
	hba->pwr_info.hs_rate = PA_HS_MODE_B;

	/* Unipro Attribute set */
	unipro_writel(&ufs->handle, pmd->gear, UNIP_PA_RXGEAR); /* PA_RxGear */
	unipro_writel(&ufs->handle, pmd->gear, UNIP_PA_TXGEAR); /* PA_TxGear */
	unipro_writel(&ufs->handle, pmd->lane, UNIP_PA_ACTIVERXDATALENS); /* PA_ActiveRxDataLanes */
	unipro_writel(&ufs->handle, pmd->lane, UNIP_PA_ACTIVETXDATALENS); /* PA_ActiveTxDataLanes */
	unipro_writel(&ufs->handle, 1, UNIP_PA_RXTERMINATION); /* PA_RxTermination */
	unipro_writel(&ufs->handle, 1, UNIP_PA_TXTERMINATION); /* PA_TxTermination */
	unipro_writel(&ufs->handle, pmd->hs_series, UNIP_PA_HSSERIES); /* PA_HSSeries */

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODE), (1 << 4 | 1 << 0));

	return ret;
};

int ufs_gear_change(struct ufs_hba *hba, bool en)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
	struct ufs_pa_layer_attr *pwr_info = &hba->max_pwr_info.info;
	int res = 0;
	u32 set;

	if (en) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (ufs->pm_qos_int_value)
			exynos_pm_qos_update_request(&ufs->pm_qos_int,
					ufs->pm_qos_int_value);
#endif
		pmd->gear = ufs->cal_param.max_gear;
	} else {
		pmd->gear = UFS_PWM_G1;
	}

	pmd->mode = act_pmd->mode;
	pmd->hs_series = act_pmd->hs_series;
	pmd->lane = act_pmd->lane;

	/* pre pmc */
	ufs->cal_param.pmd = pmd;

	ufshcd_auto_hibern8_update(hba, 0);
	res = ufs_call_cal(ufs, 0, ufs_cal_pre_pmc);
	if (res) {
		dev_info(ufs->dev, "cal pre pmc fail\n");
		goto out;
	}

	set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
	set &= ~(UIC_POWER_MODE);
	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);

	/* gear change */
	res = exynos_gear_change_pmc(hba, pmd);
	if (res) {
		dev_info(ufs->dev, "pmc set fail\n");
		goto out;
	} else {
		set = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		set &= ~(UIC_POWER_MODE);
		ufshcd_writel(hba, set, REG_INTERRUPT_STATUS);
	}

	/* post pmc */
	res = ufs_call_cal(ufs, 0, ufs_cal_post_pmc);
	if (res) {
		dev_info(ufs->dev, "cal post pmc fail\n");
		goto out;
	}

	/*
	 * W/A for AH8
	 * have to use dme_peer cmd after send uic cmd
	 */
	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_tx);

	ufshcd_auto_hibern8_update(hba, ufs->ah8_ahit);
	dev_info(ufs->dev, "ufs power mode change: m(%d)g(%d)l(%d)hs-series(%d)\n",
			(pmd->mode & 0xF), pmd->gear, pmd->lane, pmd->hs_series);
out:
	if (pmd->gear != ufs->cal_param.max_gear) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
		if (ufs->pm_qos_int_value)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif
	}

	return res;
}

static int __ctrl_gear(struct ufs_perf *perf, enum ctrl_op op)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	struct ufs_hba *hba = perf->hba;

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)
		return 0;

	if (stat->exynos_stat != UFS_EXYNOS_PERF_OPERATIONAL)
		return 0;

	ufs_gear_change(hba, stat->g_scale_en);
	return 0;
}

static enum policy_res __update_v2(struct ufs_perf *perf, u32 qd,
			enum ufs_perf_op op, enum ufs_perf_entry entry)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;
	enum __traffic traffic = 0;
	enum policy_res res = R_OK;
	s64 diff;
	ktime_t time = ktime_get();

	if (stat->start_count_time == -1LL) {
		stat->start_count_time = time;
		traffic = TRAFFIC_NONE;
	} else {
		diff = ktime_to_ms(ktime_sub(time,
					stat->start_count_time));
		if (diff >= stat->th_duration) {
			if (stat->count / diff >= stat->th_count) {
				stat->start_count_time = -1LL;
				stat->g_scale_en = 1;
				stat->count = 0;
				traffic = TRAFFIC_HIGH;

				res = R_CTRL;
			} else {
				stat->start_count_time = -1LL;
				stat->count = 0;
				stat->g_scale_en = 0;
				traffic = TRAFFIC_LOW;

				res = R_CTRL;
			}
		} else {
			traffic = TRAFFIC_NONE;
		}
	}

	if (traffic != TRAFFIC_NONE) {
		if (stat->o_traffic != traffic) {
			perf->ctrl_handle[__CTRL_REQ_GEAR] = stat->g_scale_en ?
				CTRL_OP_UP : CTRL_OP_DOWN;
			stat->o_traffic = traffic;
		} else {
			perf->ctrl_handle[__CTRL_REQ_GEAR] = CTRL_OP_NONE;
		}

	}

	return res;
}

void ufs_gear_scale_init(struct ufs_perf *perf)
{
	struct ufs_perf_stat_v2 *stat = &perf->stat_v2;

	/* register callbacks */
	perf->update[__UPDATE_GEAR] = __update_v2;
	perf->ctrl[__CTRL_REQ_GEAR] = __ctrl_gear;

	/* default thresholds for stats */
	stat->start_count_time = -1LL;
	stat->th_duration = 100;
	stat->th_count = 180000;
	stat->o_traffic = TRAFFIC_HIGH;
	stat->exynos_stat = UFS_EXYNOS_PERF_OPERATIONAL;

	stat->reboot_nb.notifier_call = exynos_ufs_reboot_handler;
	register_reboot_notifier(&stat->reboot_nb);
}

MODULE_DESCRIPTION("Exynos UFS gear scale");
MODULE_AUTHOR("Hoyoung Seo <hy50.seo@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
