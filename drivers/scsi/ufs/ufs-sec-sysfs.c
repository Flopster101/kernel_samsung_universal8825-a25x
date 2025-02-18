// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/sysfs.h>

#include "ufs-sec-sysfs.h"

/* sec specific vendor sysfs nodes */
struct device *sec_ufs_cmd_dev;

/* UFS info nodes : begin */
static ssize_t ufs_sec_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", ufs_vdi.unique_number);
}
static DEVICE_ATTR(un, 0440, ufs_sec_unique_number_show, NULL);

static ssize_t ufs_sec_lt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = ufs_vdi.hba;
	if (!hba) {
		dev_err(dev, "skipping ufs lt read\n");
		ufs_vdi.lifetime = 0;
	} else if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
		pm_runtime_get_sync(hba->dev);
		ufs_sec_get_health_desc(hba);
		pm_runtime_put(hba->dev);
	} else {
		/* return previous LT value if not operational */
		dev_info(hba->dev, "ufshcd_state : %d, old LT: %01x\n",
				hba->ufshcd_state, ufs_vdi.lifetime);
	}
	return snprintf(buf, PAGE_SIZE, "%01x\n", ufs_vdi.lifetime);
}
static DEVICE_ATTR(lt, 0444, ufs_sec_lt_show, NULL);

static ssize_t ufs_sec_lc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ufs_vdi.lc_info);
}

static ssize_t ufs_sec_lc_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	ufs_vdi.lc_info = value;

	return count;
}
static DEVICE_ATTR(lc, 0664, ufs_sec_lc_info_show, ufs_sec_lc_info_store);

static ssize_t ufs_sec_hist_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return SEC_UFS_ERR_HIST_SUM(buf);
}

static bool is_valid_hist_info(const char *buf, size_t count)
{
	int i;

	if (count != ERR_SUM_SIZE)
		return false;

	if (buf[0] != 'U' || buf[2] != 'I' || buf[4] != 'H' ||
	    buf[6] != 'L' || buf[8] != 'X' || buf[10] != 'Q' ||
	    buf[12] != 'R' || buf[14] != 'W' || buf[16] != 'F' ||
	    buf[18] != 'S' || buf[19] != 'M' || buf[21] != 'S' ||
	    buf[22] != 'H')
		return false;

	for (i = 1; i < ERR_SUM_SIZE; i += 2) {
		if (buf[i] - '0' < 0 || buf[i] - '0' >= 10)
			return false;
		/* increase index for "SM", "SH" */
		if (i == 17 || i == 20)
			i++;
	}

	return true;
}

static ssize_t ufs_sec_hist_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (!is_valid_hist_info(buf, count)) {
		pr_err("%s: %s, len(%lu)\n", __func__, buf, count);
		return -EINVAL;
	}

	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_count, UTP_err, buf[1]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UIC_err_count, UIC_err, buf[3]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(op_count, HW_RESET_count, buf[5]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(op_count, link_startup_count, buf[7]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(Fatal_err_count, LLE, buf[9]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_count, UTMR_query_task_count, buf[11]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_count, UTR_read_err, buf[13]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(UTP_count, UTR_write_err, buf[15]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(Fatal_err_count, DFE, buf[17]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(sense_count, scsi_medium_err, buf[20]);
	SEC_UFS_ERR_INFO_HIST_SET_VALUE(sense_count, scsi_hw_err, buf[23]);

	return count;
}
static DEVICE_ATTR(hist, 0664, ufs_sec_hist_info_show, ufs_sec_hist_info_store);

static ssize_t ufs_sec_man_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba;

	hba = ufs_vdi.hba;
	if (!hba) {
		dev_err(dev, "skipping ufs manid read\n");
		return -EINVAL;
	} else {
		return snprintf(buf, PAGE_SIZE, "%04x\n", hba->dev_info.wmanufacturerid);
	}
}
static DEVICE_ATTR(man_id, 0444, ufs_sec_man_id_show, NULL);

static ssize_t ufs_sec_stid_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", streamid_is_enabled() ? "enabled" : "disabled");
}

static ssize_t ufs_sec_stid_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	bool set;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int ret = 0;

	if (kstrtobool(buf, &set))
		return -EINVAL;

	ret = ufs_sec_streamid_ctrl(hba, set);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR(stid, 0664, ufs_sec_stid_info_show, ufs_sec_stid_info_store);

void ufs_sec_create_sysfs(struct ufs_hba *hba)
{
	/* sec specific vendor sysfs nodes */
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev))
		pr_err("Fail to create sysfs dev\n");
	else {
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_un) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lt) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lc) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_man_id) < 0)
			pr_err("Fail to create status sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_stid) < 0)
			pr_err("Fail to create stream id sysfs file\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_hist) < 0)
			pr_err("Fail to create hist sysfs file\n");
	}
}
/* UFS info nodes : end */

/* UFS SEC WB : begin */
static ssize_t ufs_sec_wb_support_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sprintf(buf, "%s:%s\n", ufs_wb.wb_support ? "Support" : "No support",
			hba->wb_enabled ? "on" : "off");
}
static DEVICE_ATTR(sec_wb_support, 0444, ufs_sec_wb_support_show, NULL);

static ssize_t ufs_sec_wb_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags;

	if (!ufs_wb.wb_setup_done) {
		dev_err(hba->dev, "SEC WB is not ready yet.\n");
		return -ENODEV;
	}

	if (!ufs_sec_is_wb_allowed()) {
		pr_err("%s: not allowed.\n", __func__);
		return -EPERM;
	}

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	value = !!value;

	if (!value) {
		if (atomic_inc_return(&ufs_wb.wb_off_cnt) == 1) {
			ufs_wb.wb_off = true;
			pr_err("disable SEC WB : state %d.\n", ufs_wb.state);
		}
	} else {
		if (atomic_dec_and_test(&ufs_wb.wb_off_cnt)) {
			ufs_wb.wb_off = false;
			pr_err("enable SEC WB.\n");
		}
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return count;
}

static ssize_t ufs_sec_wb_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ufs_wb.wb_off ? "off" : "Enabled");
}
static DEVICE_ATTR(sec_wb_enable, 0664, ufs_sec_wb_enable_show, ufs_sec_wb_enable_store);

SEC_UFS_WB_DATA_ATTR(wb_up_threshold_block, "%d\n", up_threshold_block);
SEC_UFS_WB_DATA_ATTR(wb_up_threshold_rqs, "%d\n", up_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(wb_down_threshold_block, "%d\n", down_threshold_block);
SEC_UFS_WB_DATA_ATTR(wb_down_threshold_rqs, "%d\n", down_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(lp_wb_up_threshold_block, "%d\n", lp_up_threshold_block);
SEC_UFS_WB_DATA_ATTR(lp_wb_up_threshold_rqs, "%d\n", lp_up_threshold_rqs);
SEC_UFS_WB_DATA_ATTR(lp_wb_down_threshold_block, "%d\n", lp_down_threshold_block);
SEC_UFS_WB_DATA_ATTR(lp_wb_down_threshold_rqs, "%d\n", lp_down_threshold_rqs);

SEC_UFS_WB_TIME_ATTR(wb_on_delay_ms, "%d\n", on_delay);
SEC_UFS_WB_TIME_ATTR(wb_off_delay_ms, "%d\n", off_delay);
SEC_UFS_WB_TIME_ATTR(lp_wb_on_delay_ms, "%d\n", lp_on_delay);
SEC_UFS_WB_TIME_ATTR(lp_wb_off_delay_ms, "%d\n", lp_off_delay);

SEC_UFS_WB_DATA_RO_ATTR(wb_state, "%d,%u\n", ufs_wb.state, jiffies_to_msecs(jiffies - ufs_wb.state_ts));
SEC_UFS_WB_DATA_RO_ATTR(wb_current_stat, "current : block %d, rqs %d, issued blocks %d\n",
		ufs_wb.wb_current_block, ufs_wb.wb_current_rqs, ufs_wb.wb_curr_issued_block);
SEC_UFS_WB_DATA_RO_ATTR(wb_current_min_max_stat, "current issued blocks : min %d, max %d.\n",
		(ufs_wb.wb_curr_issued_min_block == INT_MAX) ? 0 : ufs_wb.wb_curr_issued_min_block,
		ufs_wb.wb_curr_issued_max_block);
SEC_UFS_WB_DATA_RO_ATTR(wb_total_stat, "total : %dMB\n\t<  4GB:%d\n\t<  8GB:%d\n\t< 16GB:%d\n\t>=16GB:%d\n",
		ufs_wb.wb_total_issued_mb,
		ufs_wb.wb_issued_size_cnt[0],
		ufs_wb.wb_issued_size_cnt[1],
		ufs_wb.wb_issued_size_cnt[2],
		ufs_wb.wb_issued_size_cnt[3]);

static void ufs_sec_wb_init_sysfs(struct ufs_hba *hba)
{
	if (!ufs_wb.wb_setup_done)
		return;

	/* sec specific vendor sysfs nodes */
	if (!sec_ufs_cmd_dev)
		sec_ufs_cmd_dev = sec_device_create(hba, "ufs");

	if (IS_ERR(sec_ufs_cmd_dev))
		pr_err("Fail to create sec ufs sysfs dev for WB\n");
	else {
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_sec_wb_support) < 0)
			pr_err("Fail to create status sysfs file : sec_wb_support\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_sec_wb_enable) < 0)
			pr_err("Fail to create status sysfs file : sec_wb_enable\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_up_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : wb_up_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_up_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : wb_up_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_down_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : wb_down_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_down_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : wb_down_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_up_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_up_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_up_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_up_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_down_threshold_block) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_down_threshold_block\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_down_threshold_rqs) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_down_threshold_rqs\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_on_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : wb_on_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_off_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : wb_off_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_on_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_on_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_lp_wb_off_delay_ms) < 0)
			pr_err("Fail to create status sysfs file : lp_wb_off_delay_ms\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_state) < 0)
			pr_err("Fail to create status sysfs file : wb_state\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_current_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_current_stat\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_current_min_max_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_current_min_max_stat\n");
		if (device_create_file(sec_ufs_cmd_dev,
					&dev_attr_wb_total_stat) < 0)
			pr_err("Fail to create status sysfs file : wb_total_stat\n");
	}
}
/* UFS SEC WB : end */

/* UFS error info : begin */
static ssize_t SEC_UFS_op_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_count, HW_RESET_count);
	SEC_UFS_ERR_INFO_BACKUP(op_count, link_startup_count);
	SEC_UFS_ERR_INFO_BACKUP(op_count, Hibern8_enter_count);
	SEC_UFS_ERR_INFO_BACKUP(op_count, Hibern8_exit_count);

	return count;
}

static ssize_t SEC_UFS_uic_cmd_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_TEST_MODE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_PEER_GET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_PEER_SET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_POWERON_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_POWEROFF_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_ENABLE_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_RESET_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_END_PT_RST_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_LINK_STARTUP_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_HIBER_ENTER_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, DME_HIBER_EXIT_err);

	return count;
}

static ssize_t SEC_UFS_uic_err_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, PA_ERR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, DL_PA_INIT_ERROR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, DL_NAC_RECEIVED_ERROR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, DL_TC_REPLAY_ERROR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, NL_ERROR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, TL_ERROR_cnt);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, DME_ERROR_cnt);

	return count;
}

static ssize_t SEC_UFS_fatal_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, DFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, CFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, SBFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, CEFE);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, LLE);

	return count;
}

static ssize_t SEC_UFS_utp_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTMR_query_task_count);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTMR_abort_task_count);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTR_read_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTR_write_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTR_sync_cache_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTR_unmap_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTR_etc_err);

	return count;
}

static ssize_t SEC_UFS_query_cnt_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(query_count, NOP_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, R_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, W_Desc_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, R_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, W_Attr_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, R_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, Set_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, Clear_Flag_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, Toggle_Flag_err);

	return count;
}

static ssize_t SEC_UFS_err_sum_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(op_count, op_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_cmd_count, UIC_cmd_err);
	SEC_UFS_ERR_INFO_BACKUP(UIC_err_count, UIC_err);
	SEC_UFS_ERR_INFO_BACKUP(Fatal_err_count, Fatal_err);
	SEC_UFS_ERR_INFO_BACKUP(UTP_count, UTP_err);
	SEC_UFS_ERR_INFO_BACKUP(query_count, Query_err);

	return count;
}

static ssize_t sense_err_count_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	SEC_UFS_ERR_INFO_BACKUP(sense_count, scsi_medium_err);
	SEC_UFS_ERR_INFO_BACKUP(sense_count, scsi_hw_err);

	return count;
}

SEC_UFS_DATA_ATTR_RW(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\",\"H8ENTERFAIL\":\"%u\""
		",\"H8EXITFAIL\":\"%u\"\n",
		ufs_err_info.op_count.HW_RESET_count,
		ufs_err_info.op_count.link_startup_count,
		ufs_err_info.op_count.Hibern8_enter_count,
		ufs_err_info.op_count.Hibern8_exit_count);

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%u\",\"DME_GET\":\"%u\",\"DME_SET\":\"%u\""
		",\"DME_PGET\":\"%u\",\"DME_PSET\":\"%u\",\"PWRON\":\"%u\",\"PWROFF\":\"%u\""
		",\"DME_EN\":\"%u\",\"DME_RST\":\"%u\",\"EPRST\":\"%u\",\"LINKSTARTUP\":\"%u\""
		",\"H8ENTER\":\"%u\",\"H8EXIT\":\"%u\"\n",
		ufs_err_info.UIC_cmd_count.DME_TEST_MODE_err,		// TEST_MODE
		ufs_err_info.UIC_cmd_count.DME_GET_err,			// DME_GET
		ufs_err_info.UIC_cmd_count.DME_SET_err,			// DME_SET
		ufs_err_info.UIC_cmd_count.DME_PEER_GET_err,		// DME_PEER_GET
		ufs_err_info.UIC_cmd_count.DME_PEER_SET_err,		// DME_PEER_SET
		ufs_err_info.UIC_cmd_count.DME_POWERON_err,		// DME_POWERON
		ufs_err_info.UIC_cmd_count.DME_POWEROFF_err,		// DME_POWEROFF
		ufs_err_info.UIC_cmd_count.DME_ENABLE_err,		// DME_ENABLE
		ufs_err_info.UIC_cmd_count.DME_RESET_err,		// DME_RESET
		ufs_err_info.UIC_cmd_count.DME_END_PT_RST_err,		// DME_END_PT_RST
		ufs_err_info.UIC_cmd_count.DME_LINK_STARTUP_err,	// DME_LINK_STARTUP
		ufs_err_info.UIC_cmd_count.DME_HIBER_ENTER_err,		// DME_HIBERN8_ENTER
		ufs_err_info.UIC_cmd_count.DME_HIBER_EXIT_err);		// DME_HIBERN8_EXIT

SEC_UFS_DATA_ATTR_RW(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%u\",\"DLPAINITERROR\":\"%u\",\"DLNAC\":\"%u\""
		",\"DLTCREPLAY\":\"%u\",\"NLERR\":\"%u\",\"TLERR\":\"%u\",\"DMEERR\":\"%u\"\n",
		ufs_err_info.UIC_err_count.PA_ERR_cnt,			// PA_ERR
		ufs_err_info.UIC_err_count.DL_PA_INIT_ERROR_cnt,	// DL_PA_INIT_ERROR
		ufs_err_info.UIC_err_count.DL_NAC_RECEIVED_ERROR_cnt,	// DL_NAC_RECEIVED
		ufs_err_info.UIC_err_count.DL_TC_REPLAY_ERROR_cnt,	// DL_TCx_REPLAY_ERROR
		ufs_err_info.UIC_err_count.NL_ERROR_cnt,		// NL_ERROR
		ufs_err_info.UIC_err_count.TL_ERROR_cnt,		// TL_ERROR
		ufs_err_info.UIC_err_count.DME_ERROR_cnt);		// DME_ERROR

SEC_UFS_DATA_ATTR_RW(SEC_UFS_fatal_cnt, "\"DFE\":\"%u\",\"CFE\":\"%u\",\"SBFE\":\"%u\""
		",\"CEFE\":\"%u\",\"LLE\":\"%u\"\n",
		ufs_err_info.Fatal_err_count.DFE,		// Device_Fatal
		ufs_err_info.Fatal_err_count.CFE,		// Controller_Fatal
		ufs_err_info.Fatal_err_count.SBFE,		// System_Bus_Fatal
		ufs_err_info.Fatal_err_count.CEFE,		// Crypto_Engine_Fatal
		ufs_err_info.Fatal_err_count.LLE);		// Link_Lost

SEC_UFS_DATA_ATTR_RW(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%u\",\"UTMRATASK\":\"%u\",\"UTRR\":\"%u\""
		",\"UTRW\":\"%u\",\"UTRSYNCCACHE\":\"%u\",\"UTRUNMAP\":\"%u\",\"UTRETC\":\"%u\"\n",
		ufs_err_info.UTP_count.UTMR_query_task_count,	// QUERY_TASK
		ufs_err_info.UTP_count.UTMR_abort_task_count,	// ABORT_TASK
		ufs_err_info.UTP_count.UTR_read_err,		// READ_10
		ufs_err_info.UTP_count.UTR_write_err,		// WRITE_10
		ufs_err_info.UTP_count.UTR_sync_cache_err,	// SYNC_CACHE
		ufs_err_info.UTP_count.UTR_unmap_err,		// UNMAP
		ufs_err_info.UTP_count.UTR_etc_err);		// etc

SEC_UFS_DATA_ATTR_RW(SEC_UFS_query_cnt, "\"NOPERR\":\"%u\",\"R_DESC\":\"%u\",\"W_DESC\":\"%u\""
		",\"R_ATTR\":\"%u\",\"W_ATTR\":\"%u\",\"R_FLAG\":\"%u\",\"S_FLAG\":\"%u\""
		",\"C_FLAG\":\"%u\",\"T_FLAG\":\"%u\"\n",
		ufs_err_info.query_count.NOP_err,
		ufs_err_info.query_count.R_Desc_err,		// R_Desc
		ufs_err_info.query_count.W_Desc_err,		// W_Desc
		ufs_err_info.query_count.R_Attr_err,		// R_Attr
		ufs_err_info.query_count.W_Attr_err,		// W_Attr
		ufs_err_info.query_count.R_Flag_err,		// R_Flag
		ufs_err_info.query_count.Set_Flag_err,		// Set_Flag
		ufs_err_info.query_count.Clear_Flag_err,	// Clear_Flag
		ufs_err_info.query_count.Toggle_Flag_err);	// Toggle_Flag

SEC_UFS_DATA_ATTR_RW(sense_err_count, "\"MEDIUM\":\"%u\",\"HWERR\":\"%u\"\n",
		ufs_err_info.sense_count.scsi_medium_err,
		ufs_err_info.sense_count.scsi_hw_err);

/* daily err sum */
SEC_UFS_DATA_ATTR_RW(SEC_UFS_err_sum, "\"OPERR\":\"%u\",\"UICCMD\":\"%u\",\"UICERR\":\"%u\""
		",\"FATALERR\":\"%u\",\"UTPERR\":\"%u\",\"QUERYERR\":\"%u\"\n",
		ufs_err_info.op_count.op_err,
		ufs_err_info.UIC_cmd_count.UIC_cmd_err,
		ufs_err_info.UIC_err_count.UIC_err,
		ufs_err_info.Fatal_err_count.Fatal_err,
		ufs_err_info.UTP_count.UTP_err,
		ufs_err_info.query_count.Query_err);

/* accumulated err sum */
SEC_UFS_DATA_ATTR_RO(SEC_UFS_err_summary,
		"OPERR : %u, UICCMD : %u, UICERR : %u, FATALERR : %u, UTPERR : %u, QUERYERR : %u\n"
		"MEDIUM : %u, HWERR : %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(op_count, op_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_cmd_count, UIC_cmd_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_err_count, UIC_err),
		SEC_UFS_ERR_INFO_GET_VALUE(Fatal_err_count, Fatal_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UTP_count, UTP_err),
		SEC_UFS_ERR_INFO_GET_VALUE(query_count, Query_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_count, scsi_medium_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_count, scsi_hw_err));

SEC_UFS_DATA_ATTR_RO(sense_err_logging, "\"LBA0\":\"%lx\",\"LBA1\":\"%lx\",\"LBA2\":\"%lx\""
		",\"LBA3\":\"%lx\",\"LBA4\":\"%lx\",\"LBA5\":\"%lx\""
		",\"LBA6\":\"%lx\",\"LBA7\":\"%lx\",\"LBA8\":\"%lx\",\"LBA9\":\"%lx\""
		",\"REGIONMAP\":\"%016llx\"\n",
		ufs_err_info.sense_err_log.issue_LBA_list[0],
		ufs_err_info.sense_err_log.issue_LBA_list[1],
		ufs_err_info.sense_err_log.issue_LBA_list[2],
		ufs_err_info.sense_err_log.issue_LBA_list[3],
		ufs_err_info.sense_err_log.issue_LBA_list[4],
		ufs_err_info.sense_err_log.issue_LBA_list[5],
		ufs_err_info.sense_err_log.issue_LBA_list[6],
		ufs_err_info.sense_err_log.issue_LBA_list[7],
		ufs_err_info.sense_err_log.issue_LBA_list[8],
		ufs_err_info.sense_err_log.issue_LBA_list[9],
		ufs_err_info.sense_err_log.issue_region_map);

/* SEC cmd log : begin */
static ssize_t ufs_sec_cmd_log_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry = NULL;
	int i = (ufs_cmd_log->pos + UFS_SEC_CMD_LOGGING_MAX
			- UFS_SEC_CMD_LOGNODE_MAX);
	int idx = 0;
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"%2s: %10s: %2s %3s %4s %9s %6s %16s\n",
			"No", "log string", "lu", "tag",
			"c_id", "lba", "length", "time");

	for (idx = 0; idx < UFS_SEC_CMD_LOGNODE_MAX; idx++, i++) {
		i %= UFS_SEC_CMD_LOGGING_MAX;
		entry = &ufs_cmd_log->entries[i];
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%2d: %10s: %2d %3d 0x%02x %9u %6d %16llu\n",
				idx,
				entry->str, entry->lun, entry->tag,
				entry->cmd_id, entry->lba,
				entry->transfer_len, entry->tstamp);
	}

	return len;
}
static DEVICE_ATTR(cmd_log, 0440, ufs_sec_cmd_log_show, NULL);

static struct attribute *sec_ufs_error_attributes[] = {
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_err_sum.attr,
	&dev_attr_sense_err_count.attr,
	&dev_attr_sense_err_logging.attr,
	&dev_attr_SEC_UFS_err_summary.attr,
	NULL
};

static struct attribute *sec_ufs_cmd_log_attributes[] = {
	&dev_attr_cmd_log.attr,
	NULL
};

static struct attribute_group sec_ufs_cmd_log_attribute_group = {
	.attrs	= sec_ufs_cmd_log_attributes,
};

static struct attribute_group sec_ufs_error_attribute_group = {
	.attrs	= sec_ufs_error_attributes,
};

void ufs_sec_create_cmd_log_sysfs(struct ufs_hba *hba)
{
	int ret = 0;

	ret = sysfs_create_group(&sec_ufs_cmd_dev->kobj,
			&sec_ufs_cmd_log_attribute_group);
	if (ret)
		dev_err(hba->dev, "%s: Failed to create sec_ufs_cmd_log sysfs group, %d\n",
				__func__, ret);
}

void ufs_sysfs_add_sec_nodes(struct ufs_hba *hba)
{
	int ret;
	struct device *shost_dev = &(hba->host->shost_dev);

	ret = sysfs_create_group(&shost_dev->kobj, &sec_ufs_error_attribute_group);
	if (ret)
		dev_err(hba->dev, "cannot create sec error sysfs group err: %d\n", ret);

	/* sec specific vendor sysfs nodes */
	ufs_sec_create_sysfs(hba);

	/* create WB sysfs-nodes */
	ufs_sec_wb_init_sysfs(hba);

	if (ufs_sec_features.ufs_cmd_log)
		ufs_sec_create_cmd_log_sysfs(hba);
}

void ufs_sysfs_remove_sec_nodes(struct ufs_hba *hba)
{
	struct device *shost_dev = &(hba->host->shost_dev);

	sysfs_remove_group(&shost_dev->kobj, &sec_ufs_error_attribute_group);
}
/* UFS error info : end */
