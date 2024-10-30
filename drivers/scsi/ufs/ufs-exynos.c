/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include "ufs-cal-if.h"
#include "ufs-vs-mmio.h"
#include "ufs-vs-regs.h"
#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufshci.h"
#include "unipro.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "ufs-exynos.h"
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/bitfield.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <scsi/scsi.h>
#include <trace/hooks/ufshcd.h>
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
#include "ufs-sec-feature.h"
#endif
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif

#include <trace/events/ufs_exynos_perf.h>
#include <soc/samsung/debug-snapshot.h>

#include "../scsi_priv.h"
/* Performance */
#include "ufs-exynos-perf.h"

#define IS_C_STATE_ON(h) ((h)->c_state == C_ON)
#define PRINT_STATES(h)							\
	dev_err((h)->dev, "%s: prev h_state %d, cur c_state %d\n",	\
				__func__, (h)->h_state, (h)->c_state);

static struct exynos_ufs *ufs_host_backup[1];
static int ufs_host_index = 0;
static const char *res_token[2] = {
	"passes",
	"fails",
};

enum {
	UFS_S_TOKEN_FAIL,
	UFS_S_TOKEN_NUM,
};

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),
};

static const char *ufs_s_str_token[UFS_S_TOKEN_NUM] = {
	"fail to",
};

static const char *ufs_pmu_token = "ufs-phy-iso";
static const char *ufs_ext_blks[EXT_BLK_MAX][2] = {
	{"samsung,sysreg-phandle", "ufs-iocc"},	/* sysreg */
};
static int ufs_ext_ignore[EXT_BLK_MAX] = {0};

/*
 * This type makes 1st DW and another DW be logged.
 * The second one is the head of CDB for COMMAND UPIU and
 * the head of data for DATA UPIU.
 */
static const int __cport_log_type = 0x22;

/* Functions to map registers or to something by other modules */
static void ufs_udelay(u32 n)
{
	udelay(n);
}

static inline void ufs_map_vs_regions(struct exynos_ufs *ufs)
{
	ufs->handle.hci = ufs->reg_hci;
	ufs->handle.ufsp = ufs->reg_ufsp;
	ufs->handle.unipro = ufs->reg_unipro;
	ufs->handle.pma = ufs->reg_phy;
	ufs->handle.cport = ufs->reg_cport;
	ufs->handle.udelay = ufs_udelay;
}

/* Helper for UFS CAL interface */
int ufs_call_cal(struct exynos_ufs *ufs, int init, void *func)
{
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret;
	u32 reg;
	cal_if_func_init fn_init;
	cal_if_func fn;

	/* Enable MPHY APB */
	reg = hci_readl(handle, HCI_CLKSTOP_CTRL);
	reg |= (MPHY_APBCLK_STOP | UNIPRO_MCLK_STOP);
	hci_writel(handle, reg, HCI_CLKSTOP_CTRL);

	reg = hci_readl(&ufs->handle, HCI_FORCE_HCS);
	reg &= ~UNIPRO_MCLK_STOP_EN;
	hci_writel(&ufs->handle, reg & ~MPHY_APBCLK_STOP_EN, HCI_FORCE_HCS);

	if (init) {
		fn_init = (cal_if_func_init)func;
		ret = fn_init(p, ufs_host_index);
	} else {
		fn = (cal_if_func)func;
		ret = fn(p);
	}
	if (ret != UFS_CAL_NO_ERROR) {
		dev_err(ufs->dev, "%s: %d\n", __func__, ret);
		ret = -EPERM;
	}
	/* Disable MPHY APB */
	hci_writel(&ufs->handle, reg | MPHY_APBCLK_STOP_EN, HCI_FORCE_HCS);

	return ret;

}

static void exynos_ufs_update_active_lanes(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_cal_param *p = &ufs->cal_param;
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 active_tx_lane = 0;
	u32 active_rx_lane = 0;

	active_tx_lane = unipro_readl(handle, UNIP_PA_ACTIVETXDATALENS);
	active_rx_lane = unipro_readl(handle, UNIP_PA_ACTIVERXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (!active_rx_lane || !active_tx_lane || active_rx_lane != active_tx_lane) {
		dev_err(hba->dev, "%s: invalid active lanes. rx=%d, tx=%d\n",
			__func__, active_rx_lane, active_tx_lane);
		WARN_ON(1);
	}
	p->active_rx_lane = (u8)active_rx_lane;
	p->active_tx_lane = (u8)active_tx_lane;

	dev_info(ufs->dev, "PA_ActiveTxDataLanes(%d), PA_ActiveRxDataLanes(%d)\n",
		 active_tx_lane, active_rx_lane);
}

static void exynos_ufs_update_max_gear(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;
	struct ufs_cal_param *p = &ufs->cal_param;
	u32 max_rx_hs_gear = 0;

	max_rx_hs_gear = unipro_readl(&ufs->handle, UNIP_PA_MAXRXHSGEAR);

	p->max_gear = min_t(u8, max_rx_hs_gear, pmd->gear);

	dev_info(ufs->dev, "max_gear(%d), PA_MaxRxHSGear(%d)\n", p->max_gear, max_rx_hs_gear);

	/* set for sysfs */
	ufs->params[UFS_S_PARAM_EOM_SZ] =
			EOM_PH_SEL_MAX * EOM_DEF_VREF_MAX *
			ufs_s_eom_repeat[ufs->cal_param.max_gear];
}

static inline void exynos_ufs_ctrl_phy_pwr(struct exynos_ufs *ufs, bool en)
{
	struct ext_cxt *cxt = &ufs->cxt_phy_iso;

	exynos_pmu_update(cxt->offset, cxt->mask, (en ? 1 : 0) ? cxt->val : 0);
}

static inline void __thaw_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, __cport_log_type, 0x114);
	hci_writel(handle, 1, 0x110);
}

static inline void __freeze_cport_logger(struct ufs_vs_handle *handle)
{
	hci_writel(handle, 0, 0x110);
}

static int exynos_ufs_check_ah8_fsm_state(struct ufs_hba *hba, u32 state)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int cnt = 50;
	u32 reg;

	do {
		udelay(100);
		reg = hci_readl(&ufs->handle, HCI_AH8_STATE);
	} while (cnt-- && !(reg & state) && !(reg & HCI_AH8_STATE_ERROR));

	dev_info(hba->dev, "%s: cnt = %d, state = %08X, reg = %08X\n", __func__, cnt, state, reg);

	if (!(reg & state)) {
		dev_err(hba->dev,
			"AH8 FSM state is not at required state: req_state = %08X, reg_val = %08X\n",
			state, reg);

		return -EINVAL;
	}

	return 0;
}

/*
 * Exynos debugging main function
 */
static void exynos_ufs_dump_debug_info(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	/* start cs, not permit overlapped dump */
	if (test_and_set_bit(EXYNOS_UFS_BIT_DBG_DUMP, &ufs->flag))
		goto out;

	pr_info("%s: ah8_enter count: %d, ah8_exit count: %d\n", __func__,
			ufs->hibern8_enter_cnt, ufs->hibern8_exit_cnt);

	/* freeze cport logger */
	__freeze_cport_logger(handle);

	exynos_ufs_dump_info(hba, handle, ufs->dev);

	if (!(hba->saved_uic_err & (1 << 6)))
		exynos_ufs_fmp_dump_info(hba);

	/* thaw cport logger */
	__thaw_cport_logger(handle);

	/* finish cs */
	clear_bit(EXYNOS_UFS_BIT_DBG_DUMP, &ufs->flag);
out:
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
	if (hba->saved_err & SYSTEM_BUS_FATAL_ERROR)
		dbg_snapshot_expire_watchdog();
#endif
#endif
	return;
}

/*
static void exynos_ufs_dbg_command_log(struct ufs_hba *hba,
				struct scsi_cmnd *cmd, const char *str, int tag)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;

	if (!strcmp(str, "send")) {
		exynos_ufs_cmd_log_start(handle, hba, cmd);
	} else if (!strcmp(str, "complete"))
		exynos_ufs_cmd_log_end(handle, hba, tag);
}
*/

inline void exynos_ufs_ctrl_auto_hci_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(&ufs->handle, HCI_FORCE_HCS);

	if (en)
		hci_writel(&ufs->handle, reg | HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
	else
		hci_writel(&ufs->handle, reg & ~HCI_CORECLK_STOP_EN, HCI_FORCE_HCS);
}

static inline void exynos_ufs_ctrl_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(&ufs->handle, HCI_FORCE_HCS);

	if (en)
		hci_writel(&ufs->handle, reg | CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
	else
		hci_writel(&ufs->handle, reg & ~CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
}

static inline void exynos_ufs_gate_clk(struct exynos_ufs *ufs, bool en)
{

	u32 reg = hci_readl(&ufs->handle, HCI_CLKSTOP_CTRL);

	if (en)
		hci_writel(&ufs->handle, reg | CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
	else
		hci_writel(&ufs->handle, reg & ~CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
}

static void exynos_ufs_set_unipro_mclk(struct exynos_ufs *ufs)
{
	int ret;
	u32 val;

	val = (u32)clk_get_rate(ufs->clk_unipro);

	if (val != ufs->mclk_rate) {
		ret = clk_set_rate(ufs->clk_unipro, ufs->mclk_rate);
		dev_info(ufs->dev, "previous mclk: %u, ret: %d\n", val, ret);
	}
	dev_info(ufs->dev, "mclk: %u\n", ufs->mclk_rate);
}

static void exynos_ufs_fit_aggr_timeout(struct exynos_ufs *ufs)
{
	u32 cnt_val;
	unsigned long nVal;

	/* IA_TICK_SEL : 1(1us_TO_CNT_VAL) */
	nVal = hci_readl(&ufs->handle, HCI_UFSHCI_V2P1_CTRL);
	nVal |= IA_TICK_SEL;
	hci_writel(&ufs->handle, nVal, HCI_UFSHCI_V2P1_CTRL);

	cnt_val = ufs->mclk_rate / 1000000 ;
	hci_writel(&ufs->handle, cnt_val & CNT_VAL_1US_MASK, HCI_1US_TO_CNT_VAL);
}

static void exynos_ufs_init_pmc_req(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{

	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *req_pmd = &ufs->req_pmd_parm;
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware.
	 * pwr_max comes from core driver, i.e. ufshcd. That keeps the number
	 * of connected lanes.
	 */
	if (pwr_max->lane_rx != pwr_max->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes. rx=%d, tx=%d\n",
			__func__, pwr_max->lane_rx, pwr_max->lane_tx);
		WARN_ON(1);
	}

	/* gear change parameters to core driver */
	pwr_req->gear_rx
		= act_pmd->gear= min_t(u8, pwr_max->gear_rx, req_pmd->gear);
	pwr_req->gear_tx
		= act_pmd->gear = min_t(u8, pwr_max->gear_tx, req_pmd->gear);
	pwr_req->lane_rx
		= act_pmd->lane = min_t(u8, pwr_max->lane_rx, req_pmd->lane);
	pwr_req->lane_tx
		= act_pmd->lane = min_t(u8, pwr_max->lane_tx, req_pmd->lane);
	pwr_req->pwr_rx = act_pmd->mode = req_pmd->mode;
	pwr_req->pwr_tx = act_pmd->mode = req_pmd->mode;
	pwr_req->hs_rate = act_pmd->hs_series = req_pmd->hs_series;
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* bit[1] for resetn */
	hci_writel(&ufs->handle, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(&ufs->handle, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_config_host(struct exynos_ufs *ufs)
{
	u32 reg;

	/* internal clock control */
	exynos_ufs_ctrl_auto_hci_clk(ufs, false);
	exynos_ufs_set_unipro_mclk(ufs);

	/* period for interrupt aggregation */
	exynos_ufs_fit_aggr_timeout(ufs);

	/* misc HCI configurations */
	hci_writel(&ufs->handle, 0xA, HCI_DATA_REORDER);
	hci_writel(&ufs->handle, PRDT_PREFECT_EN | PRDT_SET_SIZE(12),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(&ufs->handle, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);

	/* I_T_L_Q isn't used at the beginning */
	ufs->nexus = 0xFFFFFFFF;
	hci_writel(&ufs->handle, ufs->nexus, HCI_UTRL_NEXUS_TYPE);
	hci_writel(&ufs->handle, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);

	reg = hci_readl(&ufs->handle, HCI_AXIDMA_RWDATA_BURST_LEN) &
					~BURST_LEN(0);
	hci_writel(&ufs->handle, WLU_EN | BURST_LEN(3),
					HCI_AXIDMA_RWDATA_BURST_LEN);

	/*
	 * enable HWACG control by IOP
	 *
	 * default value 1->0 at KC.
	 * always "0"(controlled by UFS_ACG_DISABLE)
	 */
	reg = hci_readl(&ufs->handle, HCI_IOP_ACG_DISABLE);
	hci_writel(&ufs->handle, reg & (~HCI_IOP_ACG_DISABLE_EN), HCI_IOP_ACG_DISABLE);
}

static int exynos_ufs_config_externals(struct exynos_ufs *ufs)
{
	int ret = 0;
	int i;
	struct regmap **p = NULL;
	struct ext_cxt *q = NULL;

	/* PHY isolation bypass */
	exynos_ufs_ctrl_phy_pwr(ufs, true);

	/* Set for UFS iocc */
	for (i = EXT_SYSREG, p = &ufs->regmap_sys, q = &ufs->cxt_iocc;
			i < EXT_BLK_MAX; i++, p++, q++) {
		if (IS_ERR_OR_NULL(*p)) {
			if (!ufs_ext_ignore[i])
				ret = -EINVAL;
			else
				continue;
			dev_err(ufs->dev, "Unable to control %s\n",
					ufs_ext_blks[i][1]);
			goto out;
		}
		regmap_update_bits(*p, q->offset, q->mask, q->val);
	}
out:
	return ret;
}

static int exynos_ufs_get_clks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct list_head *head = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	int i = 0;

	if (!head || list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		/*
		 * get clock with an order listed in device tree
		 *
		 * hci, unipro
		 */
		if (i == 0) {
			ufs->clk_hci = clki->clk;
		} else if (i == 1) {
			ufs->clk_unipro = clki->clk;
			ufs->mclk_rate = (u32)clk_get_rate(ufs->clk_unipro);
		}
		i++;
	}
out:
	if (!ufs->clk_hci || !ufs->clk_unipro)
		return -EINVAL;

	return 0;
}

static void exynos_ufs_set_features(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct device_node *np = hba->dev->of_node;

	/* caps */
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	hba->caps = UFSHCD_CAP_CLK_GATING;
#else
	hba->caps = UFSHCD_CAP_WB_EN | UFSHCD_CAP_CLK_GATING;
#endif
	if (!ufs->ah8_ahit)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	/* quirks of common driver */
	hba->quirks = UFSHCD_QUIRK_PRDT_BYTE_GRAN |
			UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR |
			UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
			UFSHCD_QUIRK_BROKEN_OCS_FATAL_ERROR |
			UFSHCD_QUIRK_ALIGN_SG_WITH_PAGE_SIZE |
			UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL |
			UFSHCD_QUIRK_SKIP_DEF_UNIPRO_TIMEOUT_SETTING;

	if(of_find_property(np, "fixed-prdt-req_list-ocs", NULL)) {
		hba->quirks &= ~(UFSHCD_QUIRK_PRDT_BYTE_GRAN |
				UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
				UFSHCD_QUIRK_BROKEN_OCS_FATAL_ERROR);
	}
}

/*
 * Exynos-specific callback functions
 */
static int exynos_ufs_init(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	/* refer to hba */
	ufs->hba = hba;

	/* configure externals */
	ret = exynos_ufs_config_externals(ufs);
	if (ret)
		return ret;

	/* to read standard hci registers */
	ufs->handle.std = hba->mmio_base;

	/* get some clock sources and debug infomation structures */
	ret = exynos_ufs_get_clks(hba);
	if (ret)
		return ret;

	/* set features, such as caps or quirks */
	exynos_ufs_set_features(hba);

	exynos_ufs_fmp_init(hba);

	exynos_ufs_srpmb_config(hba);

	/* init io perf stat, need an identifier later */
	if (!ufs_perf_init(&ufs->perf, ufs->hba))
		dev_err(ufs->dev, "Not enable UFS performance mode\n");

	hba->ahit = ufs->ah8_ahit;

	return 0;
}

static int exynos_ufs_wait_for_register(struct ufs_vs_handle *handle, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((std_readl(handle, reg) & mask) != val) {
		usleep_range(interval_us, interval_us + 50);
		if (time_after(jiffies, timeout)) {
			if ((std_readl(handle, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

/* This is same code with ufshcd_clear_cmd() */
static int exynos_ufs_clear_cmd(struct ufs_hba *hba, int tag)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int err = 0;
	u32 mask = 1 << tag;

	/* clear outstanding transaction before retry */
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		std_writel(handle, (1 << tag), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
	else
		std_writel(handle, ~(1 << tag),
				REG_UTP_TRANSFER_REQ_LIST_CLEAR);

	/*
	 * wait for for h/w to clear corresponding bit in door-bell.
	 * max. wait is 1 sec.
	 */
	err = exynos_ufs_wait_for_register(handle,
			REG_UTP_TRANSFER_REQ_DOOR_BELL,
			mask, ~mask, 1000, 1000);

	if (!err && (hba->ufs_version >= ufshci_version(3, 0)))
		std_writel(handle, 1UL << tag, REG_UTP_TRANSFER_REQ_LIST_COMPL);

	return err;
}

static void __requeue_after_reset(struct ufs_hba *hba, bool reset)
{
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;
	unsigned long completed_reqs = hba->outstanding_reqs;
	int index, err = 0;

	pr_err("%s: outstanding reqs=0x%lx\n", __func__, hba->outstanding_reqs);
	for_each_set_bit(index, &completed_reqs, hba->nutrs) {
		lrbp = &hba->lrb[index];
		lrbp->compl_time_stamp = ktime_get();
		cmd = lrbp->cmd;
		if (!cmd)
			continue;
		if (!test_and_clear_bit(index, &hba->outstanding_reqs))
			continue;

		if (!reset) {
			err = exynos_ufs_clear_cmd(hba, index);
			if (err)
				pr_err("%s: Failed to clear tag = %d\n",
						__func__, index);
		}
		trace_android_vh_ufs_compl_command(hba, lrbp);
		scsi_dma_unmap(cmd);
		if (cmd->cmnd[0] == START_STOP) {
			set_driver_byte(cmd, SAM_STAT_TASK_ABORTED);
			set_host_byte(cmd, DID_ABORT);
			pr_err("%s: tag %d, cmd %02x aborted\n", __func__,
					index, cmd->cmnd[0]);
		} else {
			set_host_byte(cmd, DID_REQUEUE);
			pr_err("%s: tag %d, cmd %02x requeued\n", __func__,
					index, cmd->cmnd[0]);
		}
		ufshcd_crypto_clear_prdt(hba, lrbp);
		lrbp->cmd = NULL;
		ufshcd_release(hba);
		cmd->scsi_done(cmd);
	}

	pr_info("%s: clk_gating.active_reqs: %d\n", __func__, hba->clk_gating.active_reqs);
}

static void exynos_ufs_init_host(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);
	int err = 0;
	u32 reg;

	/* host reset */
	hci_writel(&ufs->handle, UFS_SW_RST_MASK, HCI_SW_RST);

	do {
		if (!(hci_readl(&ufs->handle, HCI_SW_RST) & UFS_SW_RST_MASK))
			goto success;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout host sw-reset\n");
	err = -ETIMEDOUT;

	exynos_ufs_dump_info(hba, &ufs->handle, ufs->dev);
	exynos_ufs_fmp_dump_info(hba);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
#ifndef CONFIG_SCSI_UFS_EXYNOS_BLOCK_WDT_RST
	dbg_snapshot_expire_watchdog();
#endif
#endif
	goto out;

success:
	/* report to IS.UE reg when UIC error happens during AH8 */
	if (ufshcd_is_auto_hibern8_supported(hba)) {
		reg = hci_readl(&ufs->handle, HCI_VENDOR_SPECIFIC_IE);
		hci_writel(&ufs->handle, (reg | AH8_ERR_REPORT_UE), HCI_VENDOR_SPECIFIC_IE);
	}

	/* performance */
	if (ufs->perf)
		ufs_perf_reset(ufs->perf);

	/* configure host */
	exynos_ufs_config_host(ufs);
	exynos_ufs_fmp_set_crypto_cfg(hba);

	__requeue_after_reset(hba, true);


	ufs->suspend_done = false;
out:
	if (!err)
		ufs_sec_check_device_stuck();
	return;
}

static void hibern8_enter_stat(struct exynos_ufs *ufs)
{
	u32 upmcrs;

	/*
	 * hibern8 enter results are comprised of upmcrs and uic result,
	 * but you may not get the uic result because an interrupt
	 * for ah8 error is raised and ISR handles this before this is called.
	 * Honestly, upmcrs may also be the same case because UFS driver
	 * considers ah8 error as fatal and host reset is asserted subsequently.
	 * So you don't trust this return value.
	 */
	upmcrs = __get_upmcrs(ufs);
	trace_ufshcd_profile_hibern8(dev_name(ufs->dev), "enter", 0, upmcrs);
	ufs->hibern8_enter_cnt++;
	ufs->hibern8_state = UFS_STATE_AH8;
}

static void hibern8_exit_stat(struct exynos_ufs *ufs)
{

	/*
	 * this is called before actual hibern8 exit,
	 * so return value is meaningless
	 */
	trace_ufshcd_profile_hibern8(dev_name(ufs->dev), "exit", 0, 0);
	ufs->hibern8_exit_cnt++;
	ufs->hibern8_state = UFS_STATE_IDLE;
}

static int exynos_ufs_setup_clocks(struct ufs_hba *hba, bool on,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	if (on) {
		if (notify == PRE_CHANGE) {
			/* Clear for SICD */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
			exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
			/* PM Qos hold for stability */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, ufs->pm_qos_int_value);
#endif
		} else {
			hibern8_exit_stat(ufs);
			ufs->c_state = C_ON;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->c_state = C_OFF;
			hibern8_enter_stat(ufs);

			/* reset perf context to start again */
			if (ufs->perf)
				ufs_perf_reset(ufs->perf);
		} else {
			/* PM Qos Release for stability */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
			exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif
			/* Set for SICD */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
			exynos_update_ip_idle_status(ufs->idle_ip_index, 1);
#endif
		}
	}

	return ret;
}

static int exynos_ufs_get_available_lane(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	int ret = -EINVAL;

	/* Get the available lane count */
	ufs->available_lane_tx = unipro_readl(handle, UNIP_PA_AVAILTXDATALENS);
	ufs->available_lane_rx = unipro_readl(handle, UNIP_PA_AVAILRXDATALENS);

	/*
	 * Exynos driver doesn't consider asymmetric lanes, e.g. rx=2, tx=1
	 * so, for the cases, panic occurs to detect when you face new hardware
	 */
	if (!ufs->available_lane_rx || !ufs->available_lane_tx ||
			(ufs->available_lane_rx != ufs->available_lane_tx)) {
		dev_err(hba->dev, "%s: invalid host available lanes. rx=%d, tx=%d\n",
				__func__, ufs->available_lane_rx, ufs->available_lane_tx);
		BUG_ON(1);
		goto out;
	}
	ret = exynos_ufs_dbg_set_lanes(handle, ufs->dev, ufs->available_lane_rx);
	if (ret)
		goto out;

	ufs->num_rx_lanes = ufs->available_lane_rx;
	ufs->num_tx_lanes = ufs->available_lane_tx;
	ret = 0;
out:
	return ret;
}

static void exynos_ufs_override_hba_params(struct ufs_hba *hba)
{
	hba->spm_lvl = UFS_PM_LVL_5;
}

static int exynos_ufs_hce_enable_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	PRINT_STATES(ufs);

	switch (notify) {
	case PRE_CHANGE:
		/*
		 * This function is called in ufshcd_hba_enable,
		 * maybe boot, wake-up or link start-up failure cases.
		 * To start safely, reset of entire logics of host
		 * is required
		 */
		exynos_ufs_init_host(hba);

		/* device reset */
		exynos_ufs_dev_hw_reset(hba);
		break;
	case POST_CHANGE:
		exynos_ufs_ctrl_clk(ufs, true);
		exynos_ufs_gate_clk(ufs, false);

		ret = exynos_ufs_get_available_lane(hba);

		/* freeze cport logger */
		__thaw_cport_logger(&ufs->handle);

		ufs->h_state = H_RESET;

		unipro_writel(&ufs->handle, DBG_SUITE1_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_1);
		unipro_writel(&ufs->handle, DBG_SUITE2_ENABLE,
				UNIP_PA_DBG_OPTION_SUITE_2);

		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_cal_param *p = &ufs->cal_param;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		/* override some parameters from core driver */
		exynos_ufs_override_hba_params(hba);

		if (!IS_C_STATE_ON(ufs) ||
				ufs->h_state != H_RESET)
			PRINT_STATES(ufs);

		/* hci */
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_DL_ERRS, HCI_ERROR_EN_DL_LAYER);
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_N_ERRS, HCI_ERROR_EN_N_LAYER);
		hci_writel(&ufs->handle, DFES_ERR_EN | DFES_DEF_T_ERRS, HCI_ERROR_EN_T_LAYER);

		/* cal */
		p->mclk_rate = ufs->mclk_rate;
		p->available_lane = ufs->num_rx_lanes;
		ret = ufs_call_cal(ufs, 0, ufs_cal_pre_link);
		break;
	case POST_CHANGE:
		/* update max gear after link*/
		exynos_ufs_update_max_gear(ufs->hba);

		p->connected_tx_lane = unipro_readl(&ufs->handle, UNIP_PA_CONNECTEDTXDATALENS);
		p->connected_rx_lane = unipro_readl(&ufs->handle, UNIP_PA_CONNECTEDRXDATALENS);
		/* cal */
		ret = ufs_call_cal(ufs, 0, ufs_cal_post_link);

		/* print link start-up result */
		dev_info(ufs->dev, "UFS link start-up %s\n",
					(!ret) ? res_token[0] : res_token[1]);

		ufs->h_state = H_LINK_UP;
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_pwr_change_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status notify,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
	struct ufs_pa_layer_attr *pwr_info = &hba->max_pwr_info.info;
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		if (!IS_C_STATE_ON(ufs) ||
				ufs->h_state != H_REQ_BUSY)
			PRINT_STATES(ufs);

		/* Set PMC parameters to be requested */
		exynos_ufs_init_pmc_req(hba, pwr_max, pwr_req);

		/* cal */
		ufs->cal_param.pmd = act_pmd;
		ret = ufs_call_cal(ufs, 0, ufs_cal_pre_pmc);

		break;
	case POST_CHANGE:
		/* update active lanes after pmc */
		exynos_ufs_update_active_lanes(hba);

		/* cal */
		ret = ufs_call_cal(ufs, 0, ufs_cal_post_pmc);

		dev_info(ufs->dev,
				"Power mode change(%d): M(%d)G(%d)L(%d)HS-series(%d)\n",
				ret, act_pmd->mode, act_pmd->gear,
				act_pmd->lane, act_pmd->hs_series);
		/*
		 * print gear change result.
		 * Exynos driver always considers gear change to
		 * HS-B and fast mode.
		 */
		if (ufs->req_pmd_parm.mode == FAST_MODE &&
				ufs->req_pmd_parm.hs_series == PA_HS_MODE_B)
			dev_info(ufs->dev, "HS mode config %s\n",
					(!ret) ? res_token[0] : res_token[1]);

		ufs->h_state = H_LINK_BOOST;

		/*
		 * W/A for AH8
		 * have to use dme_peer cmd after send uic cmd
		 */
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_tx);
		ufs->skip_flush = false;
		break;
	default:
		break;
	}

	return ret;
}

/*
 * Translating a bit-wise variable to a count essentially requires
 * requires an iteration that sometimes lead to a big cost.
 * This function remove the iteration and reduce time complexbility
 * up to O(1). Now, even if the doorbell becomes biggers,
 * there will be no change of time.
 */
static u32 __ufs_cal_set_bits(u32 reqs)
{
	/* Hamming Weight Algorithm */
	reqs = reqs - ((reqs >> 1) & 0x55555555);
	reqs = (reqs & 0x33333333) + ((reqs >> 2) & 0x33333333);
	reqs = (reqs + (reqs >> 4)) & 0x0F0F0F0F;
	return (reqs * 0x01010101) >> 24;
}

static void exynos_ufs_set_nexus_t_xfer_req(struct ufs_hba *hba,
				int tag, bool cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	enum ufs_perf_op op = UFS_PERF_OP_NONE;
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 qd;
	/*
	 * Wait up to 50us, seen as safe value for nexus configuration.
	 * Accessing HCI_UTRL_NEXUS_TYPE takes hundreds of nanoseconds
	 * given w/ simulation but we don't know when the previous access
	 * will finish. To reduce its polling latency, we use 10ns.
	 */
	int timeout_cnt = 50000 / 10;
	int wait_ns = 10;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_UP &&
			ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	/* perf */
	lrbp = &hba->lrb[tag];
	if (lrbp && lrbp->cmd) {
		scmd = lrbp->cmd;

		/* performance, only for SCSI */
		if (scmd->cmnd[0] == 0x28)
			op = UFS_PERF_OP_R;
		else if (scmd->cmnd[0] == 0x2A)
			op = UFS_PERF_OP_W;
		else if (scmd->cmnd[0] == 0x35)
			op = UFS_PERF_OP_S;
		if (ufs->perf) {
			qd = __ufs_cal_set_bits(hba->outstanding_reqs);
			ufs_perf_update(ufs->perf, qd, scmd, op);
		}

		/* cmd_logging */
		exynos_ufs_cmd_log_start(handle, hba, scmd);
	}

	/* check if an update is needed */
	while (test_and_set_bit(EXYNOS_UFS_BIT_CHK_NEXUS, &ufs->flag)
	       && timeout_cnt--)
		ndelay(wait_ns);

	if (cmd) {
		if (test_and_set_bit(tag, &ufs->nexus))
			goto out;
	} else {
		if (!test_and_clear_bit(tag, &ufs->nexus))
			goto out;
	}
	hci_writel(&ufs->handle, (u32)ufs->nexus, HCI_UTRL_NEXUS_TYPE);
out:
	clear_bit(EXYNOS_UFS_BIT_CHK_NEXUS, &ufs->flag);
	ufs->h_state = H_REQ_BUSY;
}

static void exynos_ufs_check_uac(struct ufs_hba *hba, int tag, bool cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *scmd;
	struct utp_upiu_rsp *ucd_rsp_ptr;
	int result = 0;

	if (!cmd)
		return;

	lrbp = &hba->lrb[tag];
	ucd_rsp_ptr = lrbp->ucd_rsp_ptr;
	scmd = lrbp->cmd;

	result = be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;
	if (result != UPIU_TRANSACTION_RESPONSE)
		return;

	result = be32_to_cpu(ucd_rsp_ptr->header.dword_1);
	if (result & SAM_STAT_CHECK_CONDITION) {
		result &= ~SAM_STAT_CHECK_CONDITION;
		result |= SAM_STAT_BUSY;
		ucd_rsp_ptr->header.dword_1 = cpu_to_be32(result);

		scmd->result |= (DID_SOFT_ERROR << 16);
	}

	ufs->resume_state = 0;
}

static void exynos_ufs_compl_nexus_t_xfer_req(void *data, struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	unsigned long completed_reqs;
	u32 tr_doorbell;
	int tag;

	if (!lrbp) {
		dev_err(hba->dev, "%s: lrbp: local reference block is null\n", __func__);
		return;
	}

	tag = lrbp->task_tag;

	/* When it's first command completion after resume, check uac. */
	if (ufs->resume_state || (lrbp->lun != 0))
		exynos_ufs_check_uac(hba, tag, (lrbp->cmd ? true : false));

	/* cmd_logging */
	if (lrbp->cmd)
		exynos_ufs_cmd_log_end(handle, hba, tag);

	tr_doorbell = std_readl(handle, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = tr_doorbell ^ hba->outstanding_reqs;
	if (!(hba->outstanding_reqs^completed_reqs)) {
		if (ufs->perf)
			ufs_perf_reset(ufs->perf);
	}
}

static void exynos_ufs_set_nexus_t_task_mgmt(struct ufs_hba *hba, int tag, u8 tm_func)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 type;

	if (!IS_C_STATE_ON(ufs) ||
			(ufs->h_state != H_LINK_BOOST &&
			ufs->h_state != H_TM_BUSY &&
			ufs->h_state != H_REQ_BUSY))
		PRINT_STATES(ufs);

	type =  hci_readl(&ufs->handle, HCI_UTMRL_NEXUS_TYPE);

	switch (tm_func) {
	case UFS_ABORT_TASK:
	case UFS_QUERY_TASK:
		type |= (1 << tag);
		break;
	case UFS_ABORT_TASK_SET:
	case UFS_CLEAR_TASK_SET:
	case UFS_LOGICAL_RESET:
	case UFS_QUERY_TASK_SET:
		type &= ~(1 << tag);
		break;
	}

	hci_writel(&ufs->handle, type, HCI_UTMRL_NEXUS_TYPE);

	ufs->h_state = H_TM_BUSY;
}

static void __check_int_errors(void *data, struct ufs_hba *hba, bool queue_eh_work)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	u32 reg;

	reg = hci_readl(&ufs->handle, HCI_AH8_STATE);

	if ((hba->errors & UIC_ERROR) && (reg & HCI_AH8_STATE_ERROR)) {
		dev_err(hba->dev,
			"%s: ufs uic error: 0x%x, ah8 state err: 0x%x\n",
			__func__, (hba->errors & UIC_ERROR), (reg & HCI_AH8_STATE_ERROR));
		ufshcd_set_link_broken(hba);
	}

	if (hba->pm_op_in_progress && queue_eh_work && !ufs->suspend_done) {
		pr_err("%s: reset during suspend\n", __func__);
		__requeue_after_reset(hba, false);
	}

	if (ufshcd_is_auto_hibern8_supported(hba))
		hci_writel(&ufs->handle, AH8_ERR_REPORT_UE,
			HCI_VENDOR_SPECIFIC_IS);
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				enum ufs_notify_change_status notify)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	if (cmd == UIC_CMD_DME_HIBER_ENTER) {
		if (!IS_C_STATE_ON(ufs) ||
				(ufs->h_state != H_LINK_UP &&
				 ufs->h_state != H_REQ_BUSY &&
				 ufs->h_state != H_LINK_BOOST))
			PRINT_STATES(ufs);

		if (notify == PRE_CHANGE) {
			if (ufshcd_is_auto_hibern8_supported(hba) && ufs->ah8_ahit) {
				ufshcd_auto_hibern8_update(hba, 0);
				ret = exynos_ufs_check_ah8_fsm_state(hba, HCI_AH8_IDLE_STATE);
				if (ret)
					dev_err(hba->dev,
						"%s: exynos_ufs_check_ah8_fsm_state return value is %d\n",
						__func__, ret);
			}
		} else {
			/* cal */
			ufs_call_cal(ufs, 0, ufs_cal_post_h8_enter);
			/* Internal clock off */
			exynos_ufs_gate_clk(ufs, true);

			ufs->h_state_prev = ufs->h_state;
			ufs->h_state = H_HIBERN8;
		}
	} else {
		if (notify == PRE_CHANGE) {
			ufs->h_state = ufs->h_state_prev;

			/* Internal clock on */
			exynos_ufs_gate_clk(ufs, false);
			/* cal */
			ufs_call_cal(ufs, 0, ufs_cal_pre_h8_exit);
		} else {
			int h8_delay_ms_ovly =
				ufs->params[UFS_S_PARAM_H8_D_MS];

			/* override h8 enter delay */
			if (h8_delay_ms_ovly)
				hba->clk_gating.delay_ms =
					(unsigned long)h8_delay_ms_ovly;
		}
	}
}

static int __exynos_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_HIBERN8)
		PRINT_STATES(ufs);

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	if (pm_op == UFS_SHUTDOWN_PM)
		ufs_sec_print_err_info(hba);
	else
		ufs_sec_print_err();
#endif

	/* Make sure AH8 FSM is at Hibern State.
	 * When doing SW H8 Enter UIC CMD, don't need to check this state.
	 */
	if (ufshcd_is_auto_hibern8_enabled(hba)) {
		ret = exynos_ufs_check_ah8_fsm_state(hba, HCI_AH8_HIBERNATION_STATE);
		if (ret) {
			dev_err(hba->dev, "%s: exynos_ufs_check_ah8_fsm_state return value = %d\n",
					__func__, ret);
			exynos_ufs_dump_debug_info(hba);
			ufshcd_set_link_off(hba);
			return ret;
		}
	}

	hci_writel(&ufs->handle, 0 << 0, HCI_GPIO_OUT);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	 exynos_pm_qos_update_request(&ufs->pm_qos_int, 0);
#endif

	ufs->suspend_done = true;

	ufs->h_state = H_SUSPEND;
	return 0;
}

static int __exynos_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	if (!IS_C_STATE_ON(ufs) ||
			ufs->h_state != H_SUSPEND)
		PRINT_STATES(ufs);

	/* system init */
	ret = exynos_ufs_config_externals(ufs);
	if (ret)
		return ret;

	exynos_ufs_fmp_resume(hba);

	ufs->resume_state = 1;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/*
	 * Change WB state to WB_OFF to default in resume sequence.
	 * In system PM, the link is "link off state" or "hibern8".
	 * In case of link off state,
	 *	just reset the WB state because UFS device needs to setup link.
	 * In Hibern8 state,
	 *	wb_off reset and WB off are required.
	 */
	if (ufshcd_is_system_pm(pm_op))
		ufs_sec_wb_force_off(hba);
#endif

	ufshcd_set_link_off(hba);

	return 0;
}

#if 0
static void exynos_ufs_perf_mode(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	enum ufs_perf_op op = UFS_PERF_OP_NONE;

	/* performance, only for SCSI */
	if (cmd->cmnd[0] == 0x28)
		op = UFS_PERF_OP_R;
	else if (cmd->cmnd[0] == 0x2A)
		op = UFS_PERF_OP_W;
	ufs_perf_update_stat(ufs->perf, cmd->request->__data_len, op);
}
#endif

static int __apply_dev_quirks(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 peer_hibern8time;
	struct ufs_cal_param *p = &ufs->cal_param;
	/* 50us is a heuristic value, so it could change later */
	u32 ref_gate_margin = (hba->dev_info.wspecversion >= 0x300) ?
		hba->dev_info.clk_gating_wait_us : 50;

	/*
	 * we're here, that means the sequence up to fDeviceinit
	 * is done successfully.
	 */
	dev_info(ufs->dev, "UFS device initialized\n");

	peer_hibern8time = unipro_readl(handle, UNIP_PA_HIBERN8TIME);
	unipro_writel(handle, peer_hibern8time + 1, UNIP_PA_HIBERN8TIME);
	p->ah8_thinern8_time =
		unipro_readl(handle, UNIP_PA_HIBERN8TIME) * H8T_GRANULARITY;
	p->ah8_brefclkgatingwaittime = ref_gate_margin;

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* check only at the first init */
	if (!(hba->eh_flags || hba->pm_op_in_progress)) {
		/* sec special features */
		ufs_set_sec_features(hba);
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		dev_info(hba->dev, "UFS test mode enabled\n");
#endif
	}

	ufs_sec_feature_config(hba);
#endif

	return 0;
}

static void __fixup_dev_quirks(struct ufs_hba *hba)
{
	struct device_node *np = hba->dev->of_node;

	hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS);

	if(of_find_property(np, "support-extended-features", NULL))
		hba->dev_quirks |= UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES;
}

static void exynos_ufs_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_compl_command(exynos_ufs_compl_nexus_t_xfer_req, NULL);
	register_trace_android_vh_ufs_check_int_errors(__check_int_errors, NULL);
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	/* register vendor hooks */
	ufs_sec_register_vendor_hooks();
#endif
}

/* to make device state active */
static int __device_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	hba->ahit = ufs->ah8_ahit;

	ufs->hibern8_enter_cnt = 0;
	ufs->hibern8_exit_cnt = 0;

	reg = hci_readl(handle, HCI_AH8_STATE);
	if (reg & HCI_AH8_STATE_ERROR) {
		reg = std_readl(handle, REG_INTERRUPT_STATUS);
		if (reg & UFSHCD_AH8_CHECK_IS) {
			std_writel(handle, reg, REG_INTERRUPT_STATUS);
			reg = std_readl(handle, REG_INTERRUPT_STATUS);
		}

		if (reg & UFSHCD_AH8_CHECK_IS)
			pr_info("%s: can't handle AH8_RESET, IS:%08x\n", __func__, reg);
		else {
			pr_info("%s: before AH8_RESET, AH8_STATE:%08x\n", __func__,
				hci_readl(handle, HCI_AH8_STATE));
			hci_writel(handle, HCI_CLEAR_AH8_FSM, HCI_AH8_RESET);
			pr_info("%s: after AH8_RESET, AH8_STATE:%08x\n", __func__,
				hci_readl(handle, HCI_AH8_STATE));
		}
	}

	/* guarantee device internal cache flush */
	if (hba->eh_flags && !ufs->skip_flush) {
		dev_info(hba->dev, "%s: Waiting device internal cache flush\n",
			__func__);
		ssleep(2);
		ufs->skip_flush = true;
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
		ufs_sec_check_hwrst_cnt();
#endif
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
		/* no dump for some cases, get dump before recovery */
		exynos_ufs_dump_debug_info(hba);
#endif
	}

	return 0;
}

#define UFS_TEST_COUNT 3

static void exynos_ufs_event_notify(struct ufs_hba *hba, enum ufs_event_type evt, void *data)
{
#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	struct ufs_event_hist *e;

	e = &hba->ufs_stats.event[evt];

	if ((e->cnt > UFS_TEST_COUNT) &&
			(evt == UFS_EVT_PA_ERR ||
			evt == UFS_EVT_DL_ERR ||
			evt == UFS_EVT_LINK_STARTUP_FAIL)) {
		exynos_ufs_dump_debug_info(hba);
		BUG();
	}
#endif

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_check_op_err(hba, evt, data);
#endif
}

static struct ufs_hba_variant_ops exynos_ufs_ops = {
	.init = exynos_ufs_init,
	.setup_clocks = exynos_ufs_setup_clocks,
	.hce_enable_notify = exynos_ufs_hce_enable_notify,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.pwr_change_notify = exynos_ufs_pwr_change_notify,
	.setup_xfer_req = exynos_ufs_set_nexus_t_xfer_req,
	.setup_task_mgmt = exynos_ufs_set_nexus_t_task_mgmt,
	.hibern8_notify = exynos_ufs_hibern8_notify,
	.dbg_register_dump = exynos_ufs_dump_debug_info,
	//.dbg_command_log = exynos_ufs_dbg_command_log,
	.suspend = __exynos_ufs_suspend,
	.resume = __exynos_ufs_resume,
	//.perf_mode = exynos_ufs_perf_mode,
	.apply_dev_quirks = __apply_dev_quirks,
	.fixup_dev_quirks = __fixup_dev_quirks,
	.device_reset = __device_reset,
	.event_notify = exynos_ufs_event_notify,
};

/*
 * This function is to define offset, mask and shift to access somewhere.
 */
static int exynos_ufs_set_context_for_access(struct device *dev,
				const char *name, struct ext_cxt *cxt)
{
	struct device_node *np;
	int ret = -EINVAL;

	np = of_get_child_by_name(dev->of_node, name);
	if (!np) {
		dev_info(dev, "get node(%s) doesn't exist\n", name);
		goto out;
	}

	ret = of_property_read_u32(np, "offset", &cxt->offset);
	if (ret == 0) {
		ret = of_property_read_u32(np, "mask", &cxt->mask);
		if (ret == 0)
			ret = of_property_read_u32(np, "val", &cxt->val);
	}
	if (ret != 0) {
		dev_err(dev, "%s set cxt(%s) val\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], name);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static int exynos_ufs_populate_dt_extern(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct regmap **reg = NULL;
	struct ext_cxt *cxt;

	bool is_dma_coherent = !!of_find_property(dev->of_node,
						"dma-coherent", NULL);

	int i;
	int ret = -EINPROGRESS;

	/*
	 * pmu for phy isolation. for the pmu, we use api from outside, not regmap
	 */
	cxt = &ufs->cxt_phy_iso;
	ret = exynos_ufs_set_context_for_access(dev, ufs_pmu_token, cxt);
	if (ret) {
		dev_err(dev, "%s: %u: %s get %s\n", __func__, __LINE__,
			ufs_s_str_token[UFS_S_TOKEN_FAIL], ufs_pmu_token);
		goto out;
	}

	/* others */

	/*
	* w/o 'dma-coherent' means the descriptors would be non-cacheable.
	* so, iocc should be disabled.
	*/
	if (!is_dma_coherent) {
		ufs_ext_ignore[EXT_SYSREG] = 1;
		dev_info(dev, "no 'dma-coherent', ufs iocc disabled\n");
	}

	for (i = 0, reg = &ufs->regmap_sys, cxt = &ufs->cxt_iocc;
			i < EXT_BLK_MAX; i++, reg++, cxt++) {
		/* look up phandle for external regions */
		*reg = syscon_regmap_lookup_by_phandle(np, ufs_ext_blks[i][0]);
		if (IS_ERR(*reg)) {
			dev_err(dev, "%s: %u: %s find %s\n",
				__func__, __LINE__,
				ufs_s_str_token[UFS_S_TOKEN_FAIL],
				ufs_ext_blks[i][0]);
			if (ufs_ext_ignore[i])
				continue;
			else
				ret = PTR_ERR(*reg);
			goto out;
		}

		/* get and pars echild nodes for external regions in ufs node */
		ret = exynos_ufs_set_context_for_access(dev,
				ufs_ext_blks[i][1], cxt);
		if (ret) {
			dev_err(dev, "%s: %u: %s get %s\n",
				__func__, __LINE__,
				ufs_s_str_token[UFS_S_TOKEN_FAIL],
				ufs_ext_blks[i][1]);
			if (ufs_ext_ignore[i]) {
				ret = 0;
				continue;
			}
			goto out;
		}

		dev_info(dev, "%s: offset 0x%x, mask 0x%x, value 0x%x\n",
				ufs_ext_blks[i][1], cxt->offset, cxt->mask, cxt->val);
	}

out:
	return ret;
}

static int exynos_ufs_get_pwr_mode(struct device_node *np,
				struct exynos_ufs *ufs)
{
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;

	pmd->mode = FAST_MODE;

	if (of_property_read_u8(np, "ufs,pmd-attr-lane", &pmd->lane))
		pmd->lane = 1;

	if (of_property_read_u8(np, "ufs,pmd-attr-gear", &pmd->gear))
		pmd->gear = 1;

	pmd->hs_series = PA_HS_MODE_B;

	return 0;
}

static int exynos_ufs_populate_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	int ret;

	/* Regmap for external regions */
	ret = exynos_ufs_populate_dt_extern(dev, ufs);
	if (ret) {
		dev_err(dev, "%s populate dt-pmu\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		goto out;
	}

	/* Get exynos-evt version for featuring. Now get main_rev instead of dt */
	ufs->cal_param.evt_ver = (u8)exynos_soc_info.main_rev;

	dev_info(dev, "exynos ufs evt version : %d\n",
			ufs->cal_param.evt_ver);


	/* PM QoS */
	child_np = of_get_child_by_name(np, "ufs-pm-qos");
	ufs->pm_qos_int_value = 0;
	if (!child_np)
		dev_info(dev, "No ufs-pm-qos node, not guarantee pm qos\n");
	else {
		of_property_read_u32(child_np, "freq-int", &ufs->pm_qos_int_value);
	}

	/* UIC specifics */
	exynos_ufs_get_pwr_mode(np, ufs);

	ufs->cal_param.board = 0;
	of_property_read_u8(np, "brd-for-cal", &ufs->cal_param.board);

//	ufs_perf_populate_dt(ufs->perf, np);

	ufs->ah8_ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 20) |
		    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 2);

	if (ufs->ah8_ahit) {
		ufs->cal_param.support_ah8_cal = true;
		ufs->cal_param.ah8_thinern8_time = 3;
		ufs->cal_param.ah8_brefclkgatingwaittime = 1;
	} else {
		ufs->cal_param.support_ah8_cal = false;
	}

out:
	return ret;
}

static int exynos_ufs_ioremap(struct exynos_ufs *ufs, struct platform_device *pdev)
{
	/* Indicators for logs */
	static const char *ufs_region_names[NUM_OF_UFS_MMIO_REGIONS + 1] = {
		"",			/* standard hci */
		"reg_hci",		/* exynos-specific hci */
		"reg_unipro",		/* unipro */
		"reg_ufsp",		/* ufs protector */
		"reg_phy",		/* phy */
		"reg_cport",		/* cport */
	};
	struct device *dev = &pdev->dev;
	struct resource *res;
	void **p = NULL;
	int i = 0;
	int ret = 0;

	for (i = 1, p = &ufs->reg_hci;
			i < NUM_OF_UFS_MMIO_REGIONS + 1; i++, p++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			ret = -ENOMEM;
			break;
		}
		*p = devm_ioremap_resource(dev, res);
		if (!*p) {
			ret = -ENOMEM;
			break;
		}
		dev_info(dev, "%-10s 0x%llx\n", ufs_region_names[i], *p);
	}

	if (ret)
		dev_err(dev, "%s ioremap for %s, 0x%llx\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], ufs_region_names[i]);
	dev_info(dev, "\n");
	return ret;
}

/* sysfs to support utc, eom or whatever */
struct exynos_ufs_sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct exynos_ufs *ufs, char *buf, enum exynos_ufs_param_id id);
	int (*store)(struct exynos_ufs *ufs, u32 value, enum exynos_ufs_param_id id);
	int id;
};

static ssize_t exynos_ufs_sysfs_default_show(struct exynos_ufs *ufs, char *buf,
					     enum exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", ufs->params[id]);
}

#define UFS_S_RO(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0444 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

#define UFS_S_RW(_name, _id)						\
	static struct exynos_ufs_sysfs_attr ufs_s_##_name = {		\
		.attr = { .name = #_name, .mode = 0666 },		\
		.id = _id,						\
		.show = exynos_ufs_sysfs_default_show,			\
	}

UFS_S_RO(eom_version, UFS_S_PARAM_EOM_VER);
UFS_S_RO(eom_size, UFS_S_PARAM_EOM_SZ);

static int exynos_ufs_sysfs_lane_store(struct exynos_ufs *ufs, u32 value,
				       enum exynos_ufs_param_id id)
{
	if (value >= ufs->num_rx_lanes) {
		dev_err(ufs->dev, "%s set lane to %u. Its max is %u\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], value, ufs->num_rx_lanes);
		return -EINVAL;
	}

	ufs->params[id] = value;

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_lane = {
	.attr = { .name = "lane", .mode = 0666 },
	.id = UFS_S_PARAM_LANE,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_lane_store,
};

static int exynos_ufs_sysfs_mon_store(struct exynos_ufs *ufs, u32 value,
				      enum exynos_ufs_param_id id)
{
	struct ufs_vs_handle *handle = &ufs->handle;
	u32 reg;

	if (value & UFS_S_MON_LV1) {
		/* Trigger HCI error */
		dev_info(ufs->dev, "Interface error test\n");
		unipro_writel(handle, (1 << BIT_POS_DBG_DL_RX_INFO_FORCE |
			DL_RX_INFO_TYPE_ERROR_DETECTED << BIT_POS_DBG_DL_RX_INFO_TYPE |
			1 << 15), UNIP_DBG_RX_INFO_CONTROL_DIRECT);

	} else if (value & UFS_S_MON_LV2) {
		/* Block all the interrupts */
		dev_info(ufs->dev, "Device error test\n");

		reg = std_readl(handle, REG_INTERRUPT_ENABLE);
		std_writel(handle, (reg & ~UTP_TRANSFER_REQ_COMPL), REG_INTERRUPT_ENABLE);
	} else {
		dev_err(ufs->dev, "Undefined level\n");
		return -EINVAL;
	}

	ufs->params[id] = value;
	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_monitor = {
	.attr = { .name = "monitor", .mode = 0666 },
	.id = UFS_S_PARAM_MON,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_mon_store,
};

static int exynos_ufs_sysfs_eom_offset_store(struct exynos_ufs *ufs, u32 offset,
					     enum exynos_ufs_param_id id)
{
	u32 num_per_lane = ufs->params[UFS_S_PARAM_EOM_SZ];

	if (offset >= num_per_lane) {
		dev_err(ufs->dev,
			"%s set ofs to %u. The available offset is up to %u\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL],
			offset, num_per_lane - 1);
		return -EINVAL;
	}

	ufs->params[id] = offset;

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom_offset = {
	.attr = { .name = "eom_offset", .mode = 0666 },
	.id = UFS_S_PARAM_EOM_OFS,
	.show = exynos_ufs_sysfs_default_show,
	.store = exynos_ufs_sysfs_eom_offset_store,
};

static ssize_t exynos_ufs_sysfs_show_h8_delay(struct exynos_ufs *ufs,
					      char *buf,
					      enum exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "%lu\n", ufs->hba->clk_gating.delay_ms);
}

static struct exynos_ufs_sysfs_attr ufs_s_h8_delay_ms = {
	.attr = { .name = "h8_delay_ms", .mode = 0666 },
	.id = UFS_S_PARAM_H8_D_MS,
	.show = exynos_ufs_sysfs_show_h8_delay,
};

static ssize_t exynos_ufs_sysfs_show_ah8_cnt(struct exynos_ufs *ufs,
					      char *buf,
					      enum exynos_ufs_param_id id)
{
	return snprintf(buf, PAGE_SIZE, "AH8_Enter_cnt: %d, AH8_Exit_cnt: %d\n",
			ufs->hibern8_enter_cnt, ufs->hibern8_exit_cnt);
}

static struct exynos_ufs_sysfs_attr ufs_s_ah8_cnt = {
	.attr = { .name = "ah8_cnt_show", .mode = 0666 },
	.show = exynos_ufs_sysfs_show_ah8_cnt,
};

static int exynos_ufs_sysfs_eom_store(struct exynos_ufs *ufs, u32 value,
				      enum exynos_ufs_param_id id)
{
	ssize_t ret;

	ret = ufs_call_cal(ufs, 0, ufs_cal_eom);
	if (ret)
		dev_err(ufs->dev, "%s store eom data\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);

	return ret;
}

static struct exynos_ufs_sysfs_attr ufs_s_eom = {
	.attr = { .name = "eom", .mode = 0222 },
	.store = exynos_ufs_sysfs_eom_store,
};

/* Convert Auto-Hibernate Idle Timer register value to microseconds */
static int exynos_ufs_ahit_to_us(u32 ahit)
{
	int timer = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK, ahit);
	int scale = FIELD_GET(UFSHCI_AHIBERN8_SCALE_MASK, ahit);

	for (; scale > 0; --scale)
		timer *= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return timer;
}

/* Convert microseconds to Auto-Hibernate Idle Timer register value */
static u32 exynos_ufs_us_to_ahit(unsigned int timer)
{
	unsigned int scale;

	for (scale = 0; timer > UFSHCI_AHIBERN8_TIMER_MASK; ++scale)
		timer /= UFSHCI_AHIBERN8_SCALE_FACTOR;

	return FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, timer) |
	       FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, scale);
}

static ssize_t exynos_ufs_auto_hibern8_show(struct exynos_ufs *ufs,
					    char *buf,
					    enum exynos_ufs_param_id id)
{
	u32 ahit;
	struct ufs_hba *hba = ufs->hba;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);
	ahit = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
	ufshcd_release(hba);
	pm_runtime_put_sync(hba->dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", exynos_ufs_ahit_to_us(ahit));
}

static int exynos_ufs_auto_hibern8_store(struct exynos_ufs *ufs, u32 value,
					     enum exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return -EOPNOTSUPP;

	if (value > UFSHCI_AHIBERN8_MAX)
		return -EINVAL;

	ufshcd_auto_hibern8_update(hba, exynos_ufs_us_to_ahit(value));

	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_auto_hibern8 = {
	.attr = {.name = "auto_hibern8", .mode = 0666},
	.show = exynos_ufs_auto_hibern8_show,
	.store = exynos_ufs_auto_hibern8_store,
};

static void exynos_ufs_release(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.active_reqs--;

	if (hba->clk_gating.active_reqs || hba->clk_gating.is_suspended ||
	    hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL ||
	    hba->outstanding_tasks ||
	    hba->active_uic_cmd || hba->uic_async_done ||
	    hba->clk_gating.state == CLKS_OFF)
		return;

	hba->clk_gating.state = REQ_CLKS_OFF;
	queue_delayed_work(hba->clk_gating.clk_gating_workq,
			   &hba->clk_gating.gate_work,
			   msecs_to_jiffies(hba->clk_gating.delay_ms));
}

static ssize_t exynos_ufs_clkgate_enable_show(struct exynos_ufs *ufs,
					      char *buf,
					      enum exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_gating.is_enabled);
}

static int exynos_ufs_clkgate_enable_store(struct exynos_ufs *ufs, u32 value,
						enum exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;
	unsigned long flags;

	value = !!value;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (value == hba->clk_gating.is_enabled)
		goto out;

	if (value)
		exynos_ufs_release(hba);
	else
		hba->clk_gating.active_reqs++;

	hba->clk_gating.is_enabled = value;
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

static struct exynos_ufs_sysfs_attr ufs_s_clkgate_enable = {
	.attr = {.name = "clkgate_enable", .mode = 0644},
	.show = exynos_ufs_clkgate_enable_show,
	.store = exynos_ufs_clkgate_enable_store,
};

static ssize_t ufs_exynos_gear_scale_show(struct exynos_ufs *ufs, char *buf,
		enum exynos_ufs_param_id id)
{
	struct uic_pwr_mode *pmd = &ufs->req_pmd_parm;

	return snprintf(buf, PAGE_SIZE, "%d\n", pmd->gear);
}

static int ufs_exynos_gear_scale_store(struct exynos_ufs *ufs, u32 value,
		enum exynos_ufs_param_id id)
{
	struct ufs_hba *hba = ufs->hba;
	ssize_t ret = 0;

	value = !!value;
	ret = ufs_gear_change(hba, value);

	return ret;
}

static struct exynos_ufs_sysfs_attr ufs_s_gear_scale = {
	.attr = { .name = "gear_scale", .mode = 0666 },
	.store = ufs_exynos_gear_scale_store,
	.show = ufs_exynos_gear_scale_show,
};

#define UFS_S_EOM_RO(_name)							\
static ssize_t exynos_ufs_sysfs_eom_##_name##_show(struct exynos_ufs *ufs,	\
						   char *buf,			\
						   enum exynos_ufs_param_id id)	\
{										\
	struct ufs_eom_result_s *p =						\
			ufs->cal_param.eom[ufs->params[UFS_S_PARAM_LANE]] +	\
			ufs->params[UFS_S_PARAM_EOM_OFS];			\
	return snprintf(buf, PAGE_SIZE, "%u", p->v_##_name);			\
};										\
static struct exynos_ufs_sysfs_attr ufs_s_eom_##_name = {			\
	.attr = { .name = "eom_"#_name, .mode = 0444 },				\
	.id = -1,								\
	.show = exynos_ufs_sysfs_eom_##_name##_show,				\
}

UFS_S_EOM_RO(phase);
UFS_S_EOM_RO(vref);
UFS_S_EOM_RO(err);

const static struct attribute *ufs_s_sysfs_attrs[] = {
	&ufs_s_eom_version.attr,
	&ufs_s_eom_size.attr,
	&ufs_s_eom_offset.attr,
	&ufs_s_eom.attr,
	&ufs_s_eom_phase.attr,
	&ufs_s_eom_vref.attr,
	&ufs_s_eom_err.attr,
	&ufs_s_lane.attr,
	&ufs_s_h8_delay_ms.attr,
	&ufs_s_monitor.attr,
	&ufs_s_auto_hibern8.attr,
	&ufs_s_clkgate_enable.attr,
	&ufs_s_ah8_cnt.attr,
	&ufs_s_gear_scale.attr,
	NULL,
};

static ssize_t exynos_ufs_sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);

	if (!param->show) {
		dev_err(ufs->dev, "%s show thanks to no existence of show\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		return 0;
	}

	return param->show(ufs, buf, param->id);
}

static ssize_t exynos_ufs_sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct exynos_ufs *ufs = container_of(kobj,
			struct exynos_ufs, sysfs_kobj);
	struct exynos_ufs_sysfs_attr *param = container_of(attr,
			struct exynos_ufs_sysfs_attr, attr);
	u32 val;
	int ret = 0;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	if (!param->store) {
		ufs->params[param->id] = val;
		return length;
	}

	ret = param->store(ufs, val, param->id);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops exynos_ufs_sysfs_ops = {
	.show	= exynos_ufs_sysfs_show,
	.store	= exynos_ufs_sysfs_store,
};

static struct kobj_type ufs_s_ktype = {
	.sysfs_ops	= &exynos_ufs_sysfs_ops,
	.release	= NULL,
};
static int exynos_ufs_sysfs_init(struct exynos_ufs *ufs)
{
	int error = -ENOMEM;
	int i;
	struct ufs_eom_result_s *p;

	/* allocate memory for eom per lane */
	for (i = 0; i < MAX_LANE; i++) {
		ufs->cal_param.eom[i] =
			devm_kcalloc(ufs->dev, EOM_MAX_SIZE,
				     sizeof(struct ufs_eom_result_s), GFP_KERNEL);
		p = ufs->cal_param.eom[i];
		if (!p) {
			dev_err(ufs->dev, "%s allocate eom data\n",
				ufs_s_str_token[UFS_S_TOKEN_FAIL]);
			goto fail_mem;
		}
	}

	/* create a path of /sys/kernel/ufs_x */
	kobject_init(&ufs->sysfs_kobj, &ufs_s_ktype);
	error = kobject_add(&ufs->sysfs_kobj, kernel_kobj, "ufs_%c", (char)(ufs->id + '0'));
	if (error) {
		dev_err(ufs->dev, "%s register sysfs directory: %d\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&ufs->sysfs_kobj, ufs_s_sysfs_attrs);
	if (error) {
		dev_err(ufs->dev, "%s create sysfs files: %d\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL], error);
		goto fail_kobj;
	}

	/*
	 * Set sysfs params by default. The values could change or
	 * initial configuration could be done elsewhere in the future.
	 *
	 * As for eom_version, you have to move it to store a value
	 * from device tree when eom code is revised, even though I expect
	 * it's not gonna to happen.
	 */
	ufs->params[UFS_S_PARAM_EOM_VER] = 0;
	ufs->params[UFS_S_PARAM_MON] = 0;
	ufs->params[UFS_S_PARAM_H8_D_MS] = 4;

	return 0;

fail_kobj:
	kobject_put(&ufs->sysfs_kobj);
fail_mem:
	for (i = 0; i < MAX_LANE; i++) {
		if (ufs->cal_param.eom[i])
			devm_kfree(ufs->dev, ufs->cal_param.eom[i]);
		ufs->cal_param.eom[i] = NULL;
	}
	return error;
}

static inline void exynos_ufs_sysfs_exit(struct exynos_ufs *ufs)
{
	kobject_put(&ufs->sysfs_kobj);
}

static u64 exynos_ufs_dma_mask = DMA_BIT_MASK(32);

static void __ufs_resume_async(struct work_struct *work)
{
	struct exynos_ufs *ufs =
		container_of(work, struct exynos_ufs, resume_work);
	struct ufs_hba *hba = ufs->hba;

	/* to block incoming commands prior to completion of resuming */
	hba->ufshcd_state = UFSHCD_STATE_RESET;
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);

	/* adding delay to guarantee vcc discharge for abnormal wakeup case */
	if (!ufs->deep_suspended)
		msleep(12);

	ufshcd_system_resume(hba);
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}
static int exynos_ufs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	int ret;

	dev_info(dev, "%s: start\n", __func__);
	dev_info(dev, "===============================\n");

	/* allocate memory */
	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs) {
		ret = -ENOMEM;
		goto out;
	}
	ufs->dev = dev;
	dev->platform_data = ufs;
	dev->dma_mask = &exynos_ufs_dma_mask;

	/* remap regions */
	ret = exynos_ufs_ioremap(ufs, pdev);
	if (ret)
		goto out;
	ufs_map_vs_regions(ufs);

	/* populate device tree nodes */
	ret = exynos_ufs_populate_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "%s get dt info.\n",
			ufs_s_str_token[UFS_S_TOKEN_FAIL]);
		return ret;
	}

	/* init cal */
	ufs->cal_param.handle = &ufs->handle;
	ret = ufs_call_cal(ufs, 1, ufs_cal_init);
	if (ret)
		return ret;
	dev_info(dev, "===============================\n");

	/* idle ip nofification for SICD, disable by default */
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	ufs->idle_ip_index = exynos_get_idle_ip_index(dev_name(ufs->dev), 1);
	exynos_update_ip_idle_status(ufs->idle_ip_index, 0);
#endif
	/* register pm qos knobs */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_add_request(&ufs->pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
#endif

	/* init dbg */
	exynos_ufs_init_mem_log(pdev);
	ret = exynos_ufs_init_dbg(&ufs->handle);
	if (ret)
		return ret;

	/* store ufs host symbols to analyse later */
	ufs->id = ufs_host_index++;
	ufs_host_backup[ufs->id] = ufs;

	/* init sysfs */
	exynos_ufs_sysfs_init(ufs);

	/* init specific states */
	ufs->h_state = H_DISABLED;
	ufs->c_state = C_OFF;

	/* async resume */
	INIT_WORK(&ufs->resume_work, __ufs_resume_async);

	/* register vendor hooks: compl_commmand, etc */
	exynos_ufs_register_vendor_hooks();

#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_sec_init_logging(dev);
#endif

	/* go to core driver through the glue driver */
	ret = ufshcd_pltfrm_init(pdev, &exynos_ufs_ops);
out:
	return ret;
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	ufs_host_index--;

	exynos_ufs_sysfs_exit(ufs);
#if IS_ENABLED(CONFIG_SEC_UFS_FEATURE)
	ufs_remove_sec_features(hba);
#endif

	disable_irq(hba->irq);
	ufshcd_remove(hba);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	exynos_pm_qos_remove_request(&ufs->pm_qos_int);
#endif

	/* performance */
	if (ufs->perf)
		ufs_perf_exit(ufs->perf);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_ufs_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret = 0;

	ufs->deep_suspended = false;

	/* mainly for early wake-up cases */
	if (work_busy(&ufs->resume_work) && work_pending(&ufs->resume_work))
		flush_work(&ufs->resume_work);

	ret = ufshcd_system_suspend(hba);
	if (!ret)
		hba->ufshcd_state = UFSHCD_STATE_RESET;

	return ret;
}

static int exynos_ufs_suspend_noirq(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	ufs->deep_suspended = true;

	return 0;
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	schedule_work(&ufs->resume_work);

	return 0;
}
#else
#define exynos_ufs_suspend	NULL
#define exynos_ufs_resume	NULL
#define exynos_ufs_suspend_noirq	NULL
#endif /* CONFIG_PM_SLEEP */

static void exynos_ufs_shutdown(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	struct ufs_hba *hba;

	hba = (struct ufs_hba *)platform_get_drvdata(pdev);

	if (ufs->perf)
		ufs_perf_exit(ufs->perf);

	ufshcd_shutdown(hba);
	hba->ufshcd_state = UFSHCD_STATE_ERROR;
}

static const struct dev_pm_ops exynos_ufs_dev_pm_ops = {
	.suspend		= exynos_ufs_suspend,
	.resume			= exynos_ufs_resume,
	.suspend_noirq		= exynos_ufs_suspend_noirq,
};

static const struct of_device_id exynos_ufs_match[] = {
	{ .compatible = "samsung,exynos-ufs", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_match);

static struct platform_driver exynos_ufs_driver = {
	.driver = {
		.name = "exynos-ufs",
		.owner = THIS_MODULE,
		.pm = &exynos_ufs_dev_pm_ops,
		.of_match_table = exynos_ufs_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_ufs_probe,
	.remove = exynos_ufs_remove,
	.shutdown = exynos_ufs_shutdown,
};

module_platform_driver(exynos_ufs_driver);
MODULE_DESCRIPTION("Exynos Specific UFSHCI driver");
MODULE_AUTHOR("Seungwon Jeon <tgih.jun@samsung.com>");
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_LICENSE("GPL");
