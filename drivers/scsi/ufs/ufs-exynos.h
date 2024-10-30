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

#ifndef _UFS_EXYNOS_H_
#define _UFS_EXYNOS_H_

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos_pm_qos.h>
#endif
#include <linux/platform_device.h>

#define UFS_VER_0004	4
#define UFS_VER_0005	5

#define EOM_DEF_VREF_MAX	256

#define RV_SUCCESS	0

#define H8T_GRANULARITY		100

#define BIT_POS_DBG_DL_RX_INFO_FORCE	28
#define BIT_POS_DBG_DL_RX_INFO_TYPE	18
#define DL_RX_INFO_TYPE_ERROR_DETECTED  21

enum {
	UFS_S_MON_LV1 = (1 << 0),
	UFS_S_MON_LV2 = (1 << 1),
};

struct ext_cxt {
	u32 offset;
	u32 mask;
	u32 val;
};

/*
 * H_UTP_BOOST and H_FATAL_ERR arn't in here because they were just
 * defined to enable some callback functions explanation.
 */
enum exynos_host_state {
	H_DISABLED = 0,
	H_RESET = 1,
	H_LINK_UP = 2,
	H_LINK_BOOST = 3,
	H_TM_BUSY = 4,
	H_REQ_BUSY = 5,
	H_HIBERN8 = 6,
	H_SUSPEND = 7,
};

enum exynos_clk_state {
	C_OFF = 0,
	C_ON,
};

enum exynos_ufs_ext_blks {
	EXT_SYSREG = 0,
	EXT_BLK_MAX,
#define EXT_BLK_MAX 1
};

enum exynos_ufs_param_id {
	UFS_S_PARAM_EOM_VER = 0,
	UFS_S_PARAM_EOM_SZ,
	UFS_S_PARAM_EOM_OFS,
	UFS_S_PARAM_LANE,
	UFS_S_PARAM_H8_D_MS,
	UFS_S_PARAM_MON,
	UFS_S_PARAM_NUM,
};

enum exynos_ufs_ah8_state {
	UFS_STATE_AH8,
	UFS_STATE_IDLE,
};

/* UFSHCD states */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED_FATAL,
	UFSHCD_STATE_EH_SCHEDULED_NON_FATAL,
};

struct exynos_ufs {
	struct device *dev;
	struct ufs_hba *hba;

	/*
	 * Do not change the order of iomem variables.
	 * Standard HCI regision is populated in core driver.
	 */
	void __iomem *reg_hci;			/* exynos-specific hci */
	void __iomem *reg_unipro;		/* unipro */
	void __iomem *reg_ufsp;			/* ufs protector */
	void __iomem *reg_phy;			/* phy */
	void __iomem *reg_cport;		/* cport */
#define NUM_OF_UFS_MMIO_REGIONS 5

	/*
	 * Do not change the order of remap variables.
	 */
	struct regmap *regmap_sys;		/* io coherency */
	struct ext_cxt cxt_phy_iso;
	struct ext_cxt cxt_iocc;

	/*
	 * Do not change the order of clock variables
	 */
	struct clk *clk_hci;
	struct clk *clk_unipro;

	/* exynos specific state */
	enum exynos_host_state h_state;
	enum exynos_host_state h_state_prev;
	enum exynos_clk_state c_state;

	u32 mclk_rate;

	int num_rx_lanes;
	int num_tx_lanes;

	struct uic_pwr_mode req_pmd_parm;
	struct uic_pwr_mode act_pmd_parm;

	int id;

	/* Support system power mode */
	int idle_ip_index;

	/* PM QoS for stability, not for performance */
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct exynos_pm_qos_request	pm_qos_int;
#endif
	s32			pm_qos_int_value;

	/* cal */
	struct ufs_cal_param	cal_param;

	/* performance */
	void *perf;
	struct ufs_vs_handle handle;

	u32 peer_available_lane_rx;
	u32 peer_available_lane_tx;
	u32 available_lane_rx;
	u32 available_lane_tx;

	/*
	 * This variable is to make UFS driver's operations change
	 * for specific purposes, e.g. unit test cases, or report
	 * some information to user land.
	 */
	u32 params[UFS_S_PARAM_NUM];

	/* sysfs */
	struct kobject sysfs_kobj;

	/* async resume */
	struct work_struct resume_work;

	u32 ah8_ahit;

	u32 hibern8_state;
	u32 hibern8_enter_cnt;
	u32 hibern8_exit_cnt;

	/* value 1 whenever resuming, and then we can deal with
	   UAC(Unit Attention Condition). */
	u32 resume_state;
	bool suspend_done;

	/* This variable is for featuring hw functionality */
	void *fmp;

	/* flag for runtime */
	unsigned long flag;
#define EXYNOS_UFS_BIT_DBG_DUMP			(0)
#define EXYNOS_UFS_BIT_CHK_NEXUS		(1)

	/* cache of hardware contents */
	unsigned long nexus;
	bool skip_flush;
	bool deep_suspended;
};

static inline struct exynos_ufs *to_exynos_ufs(struct ufs_hba *hba)
{
	return dev_get_platdata(hba->dev);
}

static inline u32 __get_upmcrs(struct exynos_ufs *ufs)
{
	return (std_readl(&ufs->handle, REG_CONTROLLER_STATUS) >> 8) & 0x7;
}

int exynos_ufs_init_dbg(struct ufs_vs_handle *);
int exynos_ufs_dbg_set_lanes(struct ufs_vs_handle *,
				struct device *dev, u32);
void exynos_ufs_dump_info(struct ufs_hba *, struct ufs_vs_handle *,
			  struct device *dev);
void exynos_ufs_cmd_log_start(struct ufs_vs_handle *,
				struct ufs_hba *, struct scsi_cmnd *);
void exynos_ufs_cmd_log_end(struct ufs_vs_handle *,
				struct ufs_hba *hba, int tag);

#ifdef CONFIG_SCSI_UFS_EXYNOS_FMP
void exynos_ufs_fmp_init(struct ufs_hba *hba);
void exynos_ufs_fmp_resume(struct ufs_hba *hba);
void exynos_ufs_fmp_dump_info(struct ufs_hba *hba);
#ifdef CONFIG_KEYS_IN_PRDT
static inline void exynos_ufs_fmp_set_crypto_cfg(struct ufs_hba *hba)
{
}
#else
void exynos_ufs_fmp_set_crypto_cfg(struct ufs_hba *hba);
#endif
#else  /* !CONFIG_SCSI_UFS_EXYNOS_FMP */
static inline void exynos_ufs_fmp_init(struct ufs_hba *hba)
{
}
static inline void exynos_ufs_fmp_resume(struct ufs_hba *hba)
{
}
static inline void exynos_ufs_fmp_dump_info(struct ufs_hba *hba)
{
}
static inline void exynos_ufs_fmp_set_crypto_cfg(struct ufs_hba *hba)
{
}
#endif /* ONFIG_SCSI_UFS_EXYNOS_FMP */

#if IS_ENABLED(CONFIG_SCSI_UFS_EXYNOS_SRPMB)
int exynos_ufs_srpmb_config(struct ufs_hba *hba);
struct scsi_device *exynos_ufs_srpmb_sdev(void);
#else
static inline int exynos_ufs_srpmb_config(struct ufs_hba *hba)
{
	return -1;
}
static inline struct scsi_device *exynos_ufs_srpmb_sdev(void)
{
	return NULL;
}
#endif
int exynos_ufs_init_mem_log(struct platform_device *pdev);
int ufs_call_cal(struct exynos_ufs *ufs, int init, void *func);
#endif /* _UFS_EXYNOS_H_ */
