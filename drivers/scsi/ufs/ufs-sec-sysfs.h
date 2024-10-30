// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_SYSFS_H__
#define __UFS_SEC_SYSFS_H__

#include <linux/sec_class.h>
#include <linux/sec_debug.h>

#include "ufs-sec-feature.h"

void ufs_sysfs_add_sec_nodes(struct ufs_hba *hba);
void ufs_sysfs_remove_sec_nodes(struct ufs_hba *hba);

extern struct ufs_vendor_dev_info ufs_vdi;
extern struct ufs_sec_err_info ufs_err_info;
extern struct ufs_sec_err_info ufs_err_info_backup;
extern struct ufs_sec_err_info ufs_err_hist;
extern struct ufs_sec_wb_info ufs_wb;
extern struct ufs_sec_feature_info ufs_sec_features;

/* UFSHCD states : in ufshcd.c */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED_FATAL,
	UFSHCD_STATE_EH_SCHEDULED_NON_FATAL,
};

/* UFSHCD UIC layer error flags : in ufshcd.c */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSHCD_UIC_PA_GENERIC_ERROR = (1 << 6), /* Generic PA error */
};
struct SEC_UFS_op_count {
	unsigned int HW_RESET_count;
	unsigned int link_startup_count;
	unsigned int Hibern8_enter_count;
	unsigned int Hibern8_exit_count;
	unsigned int op_err;
};

struct SEC_UFS_UIC_cmd_count {
	u8 DME_GET_err;
	u8 DME_SET_err;
	u8 DME_PEER_GET_err;
	u8 DME_PEER_SET_err;
	u8 DME_POWERON_err;
	u8 DME_POWEROFF_err;
	u8 DME_ENABLE_err;
	u8 DME_RESET_err;
	u8 DME_END_PT_RST_err;
	u8 DME_LINK_STARTUP_err;
	u8 DME_HIBER_ENTER_err;
	u8 DME_HIBER_EXIT_err;
	u8 DME_TEST_MODE_err;
	unsigned int UIC_cmd_err;
};

struct SEC_UFS_UIC_err_count {
	u8 PA_ERR_cnt;
	u8 DL_PA_INIT_ERROR_cnt;
	u8 DL_NAC_RECEIVED_ERROR_cnt;
	u8 DL_TC_REPLAY_ERROR_cnt;
	u8 NL_ERROR_cnt;
	u8 TL_ERROR_cnt;
	u8 DME_ERROR_cnt;
	unsigned int UIC_err;
	unsigned int PA_ERR_linereset;
	unsigned int PA_ERR_lane[3];
};

struct SEC_UFS_Fatal_err_count {
	u8 DFE;         // Device_Fatal
	u8 CFE;         // Controller_Fatal
	u8 SBFE;        // System_Bus_Fatal
	u8 CEFE;        // Crypto_Engine_Fatal
	u8 LLE;         // Link Lost
	unsigned int Fatal_err;
};

struct SEC_UFS_UTP_count {
	u8 UTMR_query_task_count;
	u8 UTMR_abort_task_count;
	u8 UTMR_logical_reset_count;
	u8 UTR_read_err;
	u8 UTR_write_err;
	u8 UTR_sync_cache_err;
	u8 UTR_unmap_err;
	u8 UTR_etc_err;
	unsigned int UTP_err;
};

struct SEC_UFS_QUERY_count {
	u8 NOP_err;
	u8 R_Desc_err;
	u8 W_Desc_err;
	u8 R_Attr_err;
	u8 W_Attr_err;
	u8 R_Flag_err;
	u8 Set_Flag_err;
	u8 Clear_Flag_err;
	u8 Toggle_Flag_err;
	unsigned int Query_err;
};

struct SEC_SCSI_SENSE_count {
	unsigned int scsi_medium_err;
	unsigned int scsi_hw_err;
};

#define SEC_MAX_LBA_LOGGING     10
#define SEC_ISSUE_REGION_STEP   (250*1024/4)    /* 250MB : 1 LBA = 4KB */
struct SEC_SCSI_SENSE_err_log {
	unsigned long issue_LBA_list[SEC_MAX_LBA_LOGGING];
	unsigned int issue_LBA_count;
	u64 issue_region_map;
};

struct ufs_sec_err_info {
	struct SEC_UFS_op_count op_count;
	struct SEC_UFS_UIC_cmd_count UIC_cmd_count;
	struct SEC_UFS_UIC_err_count UIC_err_count;
	struct SEC_UFS_Fatal_err_count Fatal_err_count;
	struct SEC_UFS_UTP_count UTP_count;
	struct SEC_UFS_QUERY_count query_count;
	struct SEC_SCSI_SENSE_count sense_count;
	struct SEC_SCSI_SENSE_err_log sense_err_log;
};

#define SEC_UFS_ERR_INFO_BACKUP(err_count, member) ({                                     \
		ufs_err_info_backup.err_count.member += ufs_err_info.err_count.member;     \
		ufs_err_info.err_count.member = 0; })

/* Get the sum of error count about current booting */
#define SEC_UFS_ERR_INFO_GET_VALUE(err_count, member)                                     \
	(ufs_err_info_backup.err_count.member + ufs_err_info.err_count.member)

/*  Get the sum of error count about current and previous booting */
#define SEC_UFS_ERR_INFO_HIST_SUM_GET_VALUE(err_count, member)                                \
	(SEC_UFS_ERR_INFO_GET_VALUE(err_count, member) + ufs_err_hist.err_count.member)

#define SEC_UFS_ERR_INFO_HIST_SET_VALUE(err_count, member, value)                                \
	(ufs_err_hist.err_count.member = (value - '0'))

#define SEC_UFS_DATA_ATTR_RO(name, fmt, args...)                                              \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf)      \
{                                                                                             \
	return sprintf(buf, fmt, args);                                                       \
}                                                                                             \
static DEVICE_ATTR_RO(name)

/* store function has to be defined */
#define SEC_UFS_DATA_ATTR_RW(name, fmt, args...)                                              \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf)      \
{                                                                                             \
	return sprintf(buf, fmt, args);                                                       \
}                                                                                             \
static DEVICE_ATTR(name, 0664, name##_show, name##_store)

#define SEC_UFS_ERR_COUNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

/* UFS SEC WB */
#define SEC_UFS_WB_DATA_ATTR(name, fmt, member)				\
static ssize_t ufs_sec_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	struct ufs_sec_wb_info *wb_info = &ufs_wb;			\
	return sprintf(buf, fmt, wb_info->member);			\
}									\
static ssize_t ufs_sec_##name##_store(struct device *dev,		\
		struct device_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	u32 value;							\
	struct ufs_sec_wb_info *wb_info = &ufs_wb;			\
									\
	if (kstrtou32(buf, 0, &value))					\
		return -EINVAL;						\
									\
	wb_info->member = value;					\
									\
	return count;							\
}									\
static DEVICE_ATTR(name, 0664, ufs_sec_##name##_show, ufs_sec_##name##_store)

#define SEC_UFS_WB_TIME_ATTR(name, fmt, member)				\
static ssize_t ufs_sec_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	struct ufs_sec_wb_info *wb_info = &ufs_wb;			\
	return sprintf(buf, fmt, jiffies_to_msecs(wb_info->member));	\
}									\
static ssize_t ufs_sec_##name##_store(struct device *dev,		\
		struct device_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	u32 value;							\
	struct ufs_sec_wb_info *wb_info = &ufs_wb;			\
									\
	if (kstrtou32(buf, 0, &value))					\
		return -EINVAL;						\
									\
	wb_info->member = msecs_to_jiffies(value);			\
									\
	return count;							\
}									\
static DEVICE_ATTR(name, 0664, ufs_sec_##name##_show, ufs_sec_##name##_store)

#define SEC_UFS_WB_DATA_RO_ATTR(name, fmt, args...)			\
static ssize_t ufs_sec_##name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
{									\
	return sprintf(buf, fmt, args);					\
}									\
static DEVICE_ATTR(name, 0444, ufs_sec_##name##_show, NULL)

#define get_min_errinfo(type, min_val, err_count, member)	\
	min_t(type, min_val, SEC_UFS_ERR_INFO_GET_VALUE(err_count, member))

#define get_min_errinfo_hist(type, min_val, err_count, member)	\
	min_t(type, min_val, SEC_UFS_ERR_INFO_HIST_SUM_GET_VALUE(err_count, member))

#define ERR_SUM_SIZE 25
#define ERR_HIST_SUM_SIZE 26
/**
 * UFS Error Information
 *
 * Format : U0I0H0L0X0Q0R0W0F0SM0SH0
 * U : UTP cmd error count
 * I : UIC error count
 * H : HWRESET count
 * L : Link startup failure count
 * X : Link Lost Error count
 * Q : UTMR QUERY_TASK error count
 * R : READ error count
 * W : WRITE error count
 * F : Device Fatal Error count
 * SM : Sense Medium error count
 * SH : Sense Hardware error count
 **/
#define SEC_UFS_ERR_SUM(buf) \
	sprintf(buf, "U%uI%uH%uL%uX%uQ%uR%uW%uF%uSM%uSH%u", \
			get_min_errinfo(u32, 9, UTP_count, UTP_err), \
			get_min_errinfo(u32, 9, UIC_err_count, UIC_err), \
			get_min_errinfo(u32, 9, op_count, HW_RESET_count), \
			get_min_errinfo(u32, 9, op_count, link_startup_count), \
			get_min_errinfo(u8, 9, Fatal_err_count, LLE), \
			get_min_errinfo(u8, 9, UTP_count, UTMR_query_task_count), \
			get_min_errinfo(u8, 9, UTP_count, UTR_read_err), \
			get_min_errinfo(u8, 9, UTP_count, UTR_write_err), \
			get_min_errinfo(u8, 9, Fatal_err_count, DFE), \
			get_min_errinfo(u32, 9, sense_count, scsi_medium_err), \
			get_min_errinfo(u32, 9, sense_count, scsi_hw_err))
/**
 * UFS Error Information
 * previous boot's error count + current boot's error count
 **/
#define SEC_UFS_ERR_HIST_SUM(buf) \
	sprintf(buf, "U%uI%uH%uL%uX%uQ%uR%uW%uF%uSM%uSH%u\n", \
			get_min_errinfo_hist(u32, 9, UTP_count, UTP_err), \
			get_min_errinfo_hist(u32, 9, UIC_err_count, UIC_err), \
			get_min_errinfo_hist(u32, 9, op_count, HW_RESET_count), \
			get_min_errinfo_hist(u32, 9, op_count, link_startup_count), \
			get_min_errinfo_hist(u8, 9, Fatal_err_count, LLE), \
			get_min_errinfo_hist(u8, 9, UTP_count, UTMR_query_task_count), \
			get_min_errinfo_hist(u8, 9, UTP_count, UTR_read_err), \
			get_min_errinfo_hist(u8, 9, UTP_count, UTR_write_err), \
			get_min_errinfo_hist(u8, 9, Fatal_err_count, DFE), \
			get_min_errinfo_hist(u32, 9, sense_count, scsi_medium_err), \
			get_min_errinfo_hist(u32, 9, sense_count, scsi_hw_err))

#endif
