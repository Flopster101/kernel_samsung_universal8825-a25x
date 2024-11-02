// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */
#ifndef __MMC_SEC_SYSFS_H__
#define __MMC_SEC_SYSFS_H__

#include "dw_mmc.h"

#define MAX_MMC_HOST_IDX 2
#define MAX_REQ_TYPE_INDEX  5		// sbc, cmd, data, stop, busy
#define MAX_ERR_TYPE_INDEX  2		// timeout, crc
#define MAX_ERR_LOG_INDEX   (MAX_REQ_TYPE_INDEX * MAX_ERR_TYPE_INDEX)

#define MMC_LOG_SBC_OFFSET 0
#define MMC_LOG_CMD_OFFSET 2
#define MMC_LOG_DATA_OFFSET 4
#define MMC_LOG_STOP_OFFSET 6
#define MMC_LOG_BUSY_OFFSET 8

#define STATUS_MASK (R1_ERROR | R1_CC_ERROR | R1_CARD_ECC_FAILED | \
		     R1_WP_VIOLATION | R1_OUT_OF_RANGE)

void mmc_sec_init_sysfs(struct dw_mci *host);
extern struct mmc_sec_op_info sd_op_info;

struct mmc_sec_err_info {
	char type[5];	// sbc, cmd, data, stop, busy
	int err_type;
	u32 status;
	u64 first_issue_time;
	u64 last_issue_time;
	u32 count;
};

struct mmc_sec_status_err_info {
	u32 ge_cnt;		// status[19] : general error or unknown error
	u32 cc_cnt;		// status[20] : internal card controller error
	u32 ecc_cnt;	// status[21] : ecc error
	u32 wp_cnt;		// status[26] : write protection error
	u32 oor_cnt;	// status[31] : out of range error
	u32 halt_cnt;	// cq halt / unhalt fail
	u32 cq_cnt;		// cq enable / disable fail
	u32 rpmb_cnt;	// RPMB switch fail
	u32 noti_cnt;	// uevent notification count
};

extern struct mmc_sec_err_info sd_err_info[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
extern struct mmc_sec_status_err_info sd_status_err_info[MAX_MMC_HOST_IDX];
extern struct mmc_sec_err_info sd_err_info_backup[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
extern struct mmc_sec_status_err_info sd_status_err_info_backup[MAX_MMC_HOST_IDX];

extern struct device *sd_detection_cmd_dev;
extern struct device *sd_data_cmd_dev;
extern struct device *sd_info_cmd_dev;

#endif
