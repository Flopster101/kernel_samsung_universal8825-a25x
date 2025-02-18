// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/time.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <scsi/scsi_proto.h>
#include <trace/hooks/ufshcd.h>
#include <linux/reboot.h>
#include <linux/sec_debug.h>

#include "ufs-sec-feature.h"
#include "ufs-sec-sysfs.h"

struct ufs_vendor_dev_info ufs_vdi;

struct ufs_sec_err_info ufs_err_info;
struct ufs_sec_err_info ufs_err_info_backup;
struct ufs_sec_err_info ufs_err_hist;

struct ufs_sec_wb_info ufs_wb;
struct ufs_sec_feature_info ufs_sec_features;

#define NOTI_WORK_DELAY_MS 500

#define set_wb_state(wb, s) \
	({(wb)->state = (s); (wb)->state_ts = jiffies; })

#define SEC_WB_may_on(wb, write_blocks, write_rqs) \
	((write_blocks) > ((wb)->wb_off ? (wb)->lp_up_threshold_block : (wb)->up_threshold_block) || \
	 (write_rqs) > ((wb)->wb_off ? (wb)->lp_up_threshold_rqs : (wb)->up_threshold_rqs))
#define SEC_WB_may_off(wb, write_blocks, write_rqs) \
	((write_blocks) < ((wb)->wb_off ? (wb)->lp_down_threshold_block : (wb)->down_threshold_block) && \
	 (write_rqs) < ((wb)->wb_off ? (wb)->lp_down_threshold_rqs : (wb)->down_threshold_rqs))
#define SEC_WB_check_on_delay(wb)	\
	(time_after_eq(jiffies,		\
	 (wb)->state_ts + ((wb)->wb_off ? (wb)->lp_on_delay : (wb)->on_delay)))
#define SEC_WB_check_off_delay(wb)	\
	(time_after_eq(jiffies,		\
	 (wb)->state_ts + ((wb)->wb_off ? (wb)->lp_off_delay : (wb)->off_delay)))

static void ufs_sec_set_unique_number(struct ufs_hba *hba, u8 *desc_buf)
{
	u8 manid;
	u8 serial_num_index;
	u8 snum_buf[UFS_UN_MAX_DIGITS];
	int buff_len;
	u8 *str_desc_buf = NULL;
	int err;

	/* read string desc */
	buff_len = QUERY_DESC_MAX_SIZE;
	str_desc_buf = kzalloc(buff_len, GFP_KERNEL);
	if (!str_desc_buf)
		goto out;

	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];

	/* spec is unicode but sec uses hex data */
	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_STRING, serial_num_index, 0,
			str_desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading string descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* setup unique_number */
	manid = desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	memset(snum_buf, 0, sizeof(snum_buf));
	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);
	memset(ufs_vdi.unique_number, 0, sizeof(ufs_vdi.unique_number));

	sprintf(ufs_vdi.unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			manid,
			desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE + 1],
			snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5], snum_buf[6]);

	/* Null terminate the unique number string */
	ufs_vdi.unique_number[UFS_UN_20_DIGITS] = '\0';

	dev_dbg(hba->dev, "%s: ufs un : %s\n", __func__, ufs_vdi.unique_number);
out:
	if (str_desc_buf)
		kfree(str_desc_buf);
}

void ufs_sec_get_health_desc(struct ufs_hba *hba)
{
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	buff_len = hba->desc_size[QUERY_DESC_IDN_HEALTH];
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_HEALTH, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading health descriptor. err %d",
				__func__, err);
		goto out;
	}

	/* getting Life Time at Device Health DESC*/
	ufs_vdi.lifetime = desc_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];

	dev_info(hba->dev, "LT: 0x%02x\n", (desc_buf[3] << 4) | desc_buf[4]);
out:
	if (desc_buf)
		kfree(desc_buf);
}

inline bool ufs_sec_is_wb_allowed(void)
{
	return ufs_wb.wb_support;
}

static void ufs_sec_wb_update_summary_stats(struct ufs_sec_wb_info *wb_info)
{
	int idx;

	if (unlikely(!wb_info->wb_curr_issued_block))
		return;

	if (wb_info->wb_curr_issued_max_block < wb_info->wb_curr_issued_block)
		wb_info->wb_curr_issued_max_block = wb_info->wb_curr_issued_block;
	if (wb_info->wb_curr_issued_min_block > wb_info->wb_curr_issued_block)
		wb_info->wb_curr_issued_min_block = wb_info->wb_curr_issued_block;

	/*
	 * count up index
	 * 0 : wb_curr_issued_block < 4GB
	 * 1 : 4GB <= wb_curr_issued_block < 8GB
	 * 2 : 8GB <= wb_curr_issued_block < 16GB
	 * 3 : 16GB <= wb_curr_issued_block
	 */
	idx = fls(wb_info->wb_curr_issued_block >> 20);
	idx = (idx < 4) ? idx : 3;
	wb_info->wb_issued_size_cnt[idx]++;

	/*
	 * wb_curr_issued_block : per 4KB
	 * wb_total_issued_mb : MB
	 */
	wb_info->wb_total_issued_mb += (wb_info->wb_curr_issued_block >> 8);

	wb_info->wb_curr_issued_block = 0;
}

static void ufs_sec_wb_update_state(struct ufs_hba *hba)
{
	if (hba->pm_op_in_progress) {
		pr_err("%s: pm_op_in_progress.\n", __func__);
		return;
	}

	if (!ufs_sec_is_wb_allowed())
		return;

	switch (ufs_wb.state) {
	case WB_OFF:
		if (SEC_WB_may_on(&ufs_wb, ufs_wb.wb_current_block, ufs_wb.wb_current_rqs))
			set_wb_state(&ufs_wb, WB_ON_READY);
		break;
	case WB_ON_READY:
		if (SEC_WB_may_off(&ufs_wb, ufs_wb.wb_current_block, ufs_wb.wb_current_rqs))
			set_wb_state(&ufs_wb, WB_OFF);
		else if (SEC_WB_check_on_delay(&ufs_wb)) {
			set_wb_state(&ufs_wb, WB_ON);
			// queue work to WB on
			queue_work(ufs_wb.wb_workq, &ufs_wb.wb_on_work);
		}
		break;
	case WB_OFF_READY:
		if (SEC_WB_may_on(&ufs_wb, ufs_wb.wb_current_block, ufs_wb.wb_current_rqs))
			set_wb_state(&ufs_wb, WB_ON);
		else if (SEC_WB_check_off_delay(&ufs_wb)) {
			set_wb_state(&ufs_wb, WB_OFF);
			// queue work to WB off
			queue_work(ufs_wb.wb_workq, &ufs_wb.wb_off_work);
			ufs_sec_wb_update_summary_stats(&ufs_wb);
		}
		break;
	case WB_ON:
		if (SEC_WB_may_off(&ufs_wb, ufs_wb.wb_current_block, ufs_wb.wb_current_rqs))
			set_wb_state(&ufs_wb, WB_OFF_READY);
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static int ufs_sec_wb_ctrl(struct ufs_hba *hba, bool enable, bool force)
{
	int ret = 0;
	u8 index;
	enum query_opcode opcode;

	/*
	 * Do not issue query, return immediately and set prev. state
	 * when workqueue run in suspend/resume
	 */
	if (hba->pm_op_in_progress) {
		pr_err("%s: pm_op_in_progress.\n", __func__);
		return -EBUSY;
	}

	/*
	 * Return error when ufshcd_state is not operational
	 */
	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
		pr_err("%s: UFS Host state=%d.\n", __func__, hba->ufshcd_state);
		return -EBUSY;
	}

	/*
	 * Return error when workqueue run in WB disabled state
	 */
	if (!ufs_sec_is_wb_allowed() && !force) {
		pr_err("%s: not allowed.\n", __func__);
		return 0;
	}

	if (!(enable ^ hba->wb_enabled)) {
		pr_info("%s: write booster is already %s\n",
				__func__, enable ? "enabled" : "disabled");
		return 0;
	}

	if (enable)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	index = ufshcd_wb_get_query_index(hba);

	pm_runtime_get_sync(hba->dev);

	ret = ufshcd_query_flag_retry(hba, opcode,
				      QUERY_FLAG_IDN_WB_EN, index, NULL);

	pm_runtime_put(hba->dev);

	if (!ret) {
		hba->wb_enabled = enable;
		pr_info("%s: SEC write booster %s, current WB state is %d.\n",
				__func__, enable ? "enable" : "disable",
				ufs_wb.state);
	}

	return ret;
}

static void ufs_sec_wb_on_work_func(struct work_struct *work)
{
	struct ufs_hba *hba = ufs_wb.hba;
	int ret = 0;

	ret = ufs_sec_wb_ctrl(hba, true, false);

	/* error case, except pm_op_in_progress and no supports */
	if (ret) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);

		dev_err(hba->dev, "%s: write booster on failed %d, now WB is %s, state is %d.\n", __func__,
				ret, hba->wb_enabled ? "on" : "off",
				ufs_wb.state);

		/*
		 * check only WB_ON state
		 *   WB_OFF_READY : WB may off after this condition
		 *   WB_OFF or WB_ON_READY : in WB_OFF, off_work should be queued
		 */
		/* set WB state to WB_ON_READY to trigger WB ON again */
		if (ufs_wb.state == WB_ON)
			set_wb_state(&ufs_wb, WB_ON_READY);

		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	dev_dbg(hba->dev, "%s: WB %s, count %d, ret %d.\n", __func__,
			hba->wb_enabled ? "on" : "off",
			ufs_wb.wb_current_rqs, ret);
}

static void ufs_sec_wb_off_work_func(struct work_struct *work)
{
	struct ufs_hba *hba = ufs_wb.hba;
	int ret = 0;

	ret = ufs_sec_wb_ctrl(hba, false, false);

	/* error case, except pm_op_in_progress and no supports */
	if (ret) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);

		dev_err(hba->dev, "%s: write booster off failed %d, now WB is %s, state is %d.\n", __func__,
				ret, hba->wb_enabled ? "on" : "off",
				ufs_wb.state);

		/*
		 * check only WB_OFF state
		 *   WB_ON_READY : WB may on after this condition
		 *   WB_ON or WB_OFF_READY : in WB_ON, on_work should be queued
		 */
		/* set WB state to WB_OFF_READY to trigger WB OFF again */
		if (ufs_wb.state == WB_OFF)
			set_wb_state(&ufs_wb, WB_OFF_READY);

		spin_unlock_irqrestore(hba->host->host_lock, flags);
	} else if (ufs_vdi.lifetime >= (u8)ufs_wb.wb_disable_threshold_lt) {
		pr_err("%s: disable WB by LT %u.\n", __func__, ufs_vdi.lifetime);
		ufs_wb.wb_support = false;
	}

	dev_dbg(hba->dev, "%s: WB %s, count %d, ret %d.\n", __func__,
			hba->wb_enabled ? "on" : "off",
			ufs_wb.wb_current_rqs, ret);
}

void ufs_sec_wb_force_off(struct ufs_hba *hba)
{
	if (!ufs_sec_is_wb_allowed())
		return;

	set_wb_state(&ufs_wb, WB_OFF);

	/* reset wb state and off */
	if (ufshcd_is_link_hibern8(hba) && ufs_sec_is_wb_allowed()) {
		/* reset wb disable count and enable wb */
		atomic_set(&ufs_wb.wb_off_cnt, 0);
		ufs_wb.wb_off = false;

		queue_work(ufs_wb.wb_workq, &ufs_wb.wb_off_work);
	}
}

bool ufs_sec_parse_wb_info(struct ufs_hba *hba)
{
	struct device_node *node = hba->dev->of_node;
	int temp_delay_ms_value;

	ufs_wb.wb_support = of_property_read_bool(node, "sec,wb-enable");
	if (!ufs_wb.wb_support)
		return false;

	ufs_wb.hba = hba;

	if (of_property_read_u32(node, "sec,wb-up-threshold-block", &ufs_wb.up_threshold_block))
		ufs_wb.up_threshold_block = 3072;

	if (of_property_read_u32(node, "sec,wb-up-threshold-rqs", &ufs_wb.up_threshold_rqs))
		ufs_wb.up_threshold_rqs = 30;

	if (of_property_read_u32(node, "sec,wb-down-threshold-block", &ufs_wb.down_threshold_block))
		ufs_wb.down_threshold_block = 1536;

	if (of_property_read_u32(node, "sec,wb-down-threshold-rqs", &ufs_wb.down_threshold_rqs))
		ufs_wb.down_threshold_rqs = 25;

	if (of_property_read_u32(node, "sec,wb-disable-threshold-lt", &ufs_wb.wb_disable_threshold_lt))
		ufs_wb.wb_disable_threshold_lt = 9;

	if (of_property_read_u32(node, "sec,wb-on-delay-ms", &temp_delay_ms_value))
		ufs_wb.on_delay = msecs_to_jiffies(92);
	else
		ufs_wb.on_delay = msecs_to_jiffies(temp_delay_ms_value);

	if (of_property_read_u32(node, "sec,wb-off-delay-ms", &temp_delay_ms_value))
		ufs_wb.off_delay = msecs_to_jiffies(4500);
	else
		ufs_wb.off_delay = msecs_to_jiffies(temp_delay_ms_value);

	ufs_wb.wb_curr_issued_block = 0;
	ufs_wb.wb_total_issued_mb = 0;
	ufs_wb.wb_issued_size_cnt[0] = 0;
	ufs_wb.wb_issued_size_cnt[1] = 0;
	ufs_wb.wb_issued_size_cnt[2] = 0;
	ufs_wb.wb_issued_size_cnt[3] = 0;

	ufs_wb.wb_curr_issued_min_block = INT_MAX;

	/* default values will be used when (wb_off == true) */
	ufs_wb.lp_up_threshold_block = 3072;		/* 12MB */
	ufs_wb.lp_up_threshold_rqs = 30;
	ufs_wb.lp_down_threshold_block = 3072;
	ufs_wb.lp_down_threshold_rqs = 30;
	ufs_wb.lp_on_delay = msecs_to_jiffies(200);
	ufs_wb.lp_off_delay = msecs_to_jiffies(0);

	return true;
}

static void ufs_sec_wb_toggle_flush_during_h8(struct ufs_hba *hba, bool set)
{
	int val;
	u8 index;
	int ret = 0;

	if (!(ufs_wb.wb_setup_done && ufs_wb.wb_support))
		return;

	if (set)
		val = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		val = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	/* ufshcd_wb_toggle_flush_during_h8 */
	index = ufshcd_wb_get_query_index(hba);

	ret = ufshcd_query_flag_retry(hba, val,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8,
				index, NULL);

	dev_info(hba->dev, "%s: %s WB flush during H8 is %s.\n", __func__,
			set ? "set" : "clear",
			ret ? "failed" : "done");
}

static void ufs_sec_wb_config(struct ufs_hba *hba, bool set)
{
	if (!ufs_sec_is_wb_allowed())
		return;

	set_wb_state(&ufs_wb, WB_OFF);

	if (ufs_vdi.lifetime >= (u8)ufs_wb.wb_disable_threshold_lt)
		goto wb_disabled;

	/* reset wb disable count and enable wb */
	atomic_set(&ufs_wb.wb_off_cnt, 0);
	ufs_wb.wb_off = false;

	ufs_sec_wb_toggle_flush_during_h8(hba, set);

	return;
wb_disabled:
	ufs_wb.wb_support = false;
}

static void ufs_sec_wb_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u8 lun;
	u32 d_lu_wb_buf_alloc = 0;
	int err = 0;
	char wq_name[sizeof("SEC_WB_wq")];

	if (!ufs_sec_parse_wb_info(hba))
		return;

	if (ufs_wb.wb_setup_done)
		return;

	if (hba->desc_size[QUERY_DESC_IDN_DEVICE] < DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 4)
		goto wb_disabled;

	dev_info->d_ext_ufs_feature_sup =
		get_unaligned_be32(desc_buf +
				   DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (!(dev_info->d_ext_ufs_feature_sup & UFS_DEV_WRITE_BOOSTER_SUP))
		goto wb_disabled;

	/*
	 * WB may be supported but not configured while provisioning.
	 * The spec says, in dedicated wb buffer mode,
	 * a max of 1 lun would have wb buffer configured.
	 * Now only shared buffer mode is supported.
	 */
	dev_info->b_wb_buffer_type =
		desc_buf[DEVICE_DESC_PARAM_WB_TYPE];

	dev_info->b_presrv_uspc_en =
		desc_buf[DEVICE_DESC_PARAM_WB_PRESRV_USRSPC_EN];

	if (dev_info->b_wb_buffer_type == WB_BUF_MODE_SHARED &&
			(hba->dev_info.wspecversion >= 0x310 ||
			 hba->dev_info.wspecversion == 0x220)) {
		dev_info->d_wb_alloc_units =
		get_unaligned_be32(desc_buf +
				   DEVICE_DESC_PARAM_WB_SHARED_ALLOC_UNITS);
		if (!dev_info->d_wb_alloc_units)
			goto wb_disabled;
	} else {
		int buf_len;
		u8 *desc_buf_temp;

		buf_len = hba->desc_size[QUERY_DESC_IDN_UNIT];

		desc_buf_temp = kmalloc(buf_len, GFP_KERNEL);
		if (!desc_buf_temp)
			goto wb_disabled;

		for (lun = 0; lun < UFS_UPIU_MAX_WB_LUN_ID; lun++) {
			d_lu_wb_buf_alloc = 0;
			err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					QUERY_DESC_IDN_UNIT, lun, 0,
					desc_buf_temp, &buf_len);
			if (err) {
				kfree(desc_buf_temp);
				goto wb_disabled;
			}

			memcpy((u8 *)&d_lu_wb_buf_alloc,
					&desc_buf_temp[UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS],
					sizeof(d_lu_wb_buf_alloc));
			if (d_lu_wb_buf_alloc) {
				dev_info->wb_dedicated_lu = lun;
				break;
			}
		}
		kfree(desc_buf_temp);

		if (!d_lu_wb_buf_alloc)
			goto wb_disabled;
	}

	dev_info(hba->dev, "%s: SEC WB is enabled. type=%x, size=%u.\n", __func__,
			dev_info->b_wb_buffer_type, d_lu_wb_buf_alloc);

	ufs_wb.wb_setup_done = true;

	INIT_WORK(&ufs_wb.wb_on_work, ufs_sec_wb_on_work_func);
	INIT_WORK(&ufs_wb.wb_off_work, ufs_sec_wb_off_work_func);
	snprintf(wq_name, sizeof(wq_name), "SEC_WB_wq");
	ufs_wb.wb_workq = create_freezable_workqueue(wq_name);

	return;
wb_disabled:
	ufs_wb.wb_support = false;
}

/* SEC cmd log : begin */
static void __ufs_sec_log_cmd(struct ufs_hba *hba, int str_idx,
		unsigned int tag, u8 cmd_id, u8 idn, u8 lun, u32 lba,
		int transfer_len)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log =
		ufs_sec_features.ufs_cmd_log;
	struct ufs_sec_cmd_log_entry *entry =
		&ufs_cmd_log->entries[ufs_cmd_log->pos];

	int cpu = raw_smp_processor_id();

	entry->lun = lun;
	entry->str = ufs_sec_log_str[str_idx];
	entry->cmd_id = cmd_id;
	entry->lba = lba;
	entry->transfer_len = transfer_len;
	entry->idn = idn;
	entry->tag = tag;
	entry->tstamp = cpu_clock(cpu);
	entry->outstanding_reqs = hba->outstanding_reqs;
	ufs_cmd_log->pos =
		(ufs_cmd_log->pos + 1) % UFS_SEC_CMD_LOGGING_MAX;
}

static void ufs_sec_log_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
		int str_t, struct ufs_sec_cmd_info *ufs_cmd, u8 cmd_id)
{
	u8 opcode = 0;
	u8 idn = 0;

	if (!ufs_sec_features.ufs_cmd_log)
		return;

	switch (str_t) {
	case UFS_SEC_CMD_SEND:
	case UFS_SEC_CMD_COMP:
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, ufs_cmd->opcode,
				0, lrbp->lun, ufs_cmd->lba,
				ufs_cmd->transfer_len);
		break;
	case UFS_SEC_QUERY_SEND:
	case UFS_SEC_QUERY_COMP:
		opcode = hba->dev_cmd.query.request.upiu_req.opcode;
		idn = hba->dev_cmd.query.request.upiu_req.idn;
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, opcode,
				idn, lrbp->lun, 0, 0);
		break;
	case UFS_SEC_NOP_SEND:
	case UFS_SEC_NOP_COMP:
		__ufs_sec_log_cmd(hba, str_t, lrbp->task_tag, 0,
				0, lrbp->lun, 0, 0);
		break;
	case UFS_SEC_TM_SEND:
	case UFS_SEC_TM_COMP:
	case UFS_SEC_TM_ERR:
	case UFS_SEC_UIC_SEND:
	case UFS_SEC_UIC_COMP:
		__ufs_sec_log_cmd(hba, str_t, 0, cmd_id, 0, 0, 0, 0);
		break;
	default:
		break;
	}
}

static void ufs_sec_init_cmd_logging(struct device *dev)
{
	struct ufs_sec_cmd_log_info *ufs_cmd_log = NULL;

	ufs_cmd_log = devm_kzalloc(dev, sizeof(struct ufs_sec_cmd_log_info),
			GFP_KERNEL);
	if (!ufs_cmd_log) {
		dev_err(dev, "%s: Failed allocating ufs_cmd_log(%lu)",
				__func__,
				sizeof(struct ufs_sec_cmd_log_info));
		return;
	}

	ufs_cmd_log->entries = devm_kcalloc(dev, UFS_SEC_CMD_LOGGING_MAX,
			sizeof(struct ufs_sec_cmd_log_entry), GFP_KERNEL);
	if (!ufs_cmd_log->entries) {
		dev_err(dev, "%s: Failed allocating cmd log entry(%lu)",
				__func__,
				sizeof(struct ufs_sec_cmd_log_entry)
				* UFS_SEC_CMD_LOGGING_MAX);
		devm_kfree(dev, ufs_cmd_log);
		return;
	}

	pr_info("SEC UFS cmd logging is initialized.\n");

	ufs_sec_features.ufs_cmd_log = ufs_cmd_log;

	ufs_sec_features.ucmd_complete = true;
	ufs_sec_features.qcmd_complete = true;

}
/* SEC cmd log : end */

/*stream id*/
inline bool streamid_is_enabled(void)
{
	return ufs_vdi.stid_en;
}

static inline void set_streamid_state(bool set)
{
	ufs_vdi.stid_en = set;
}

/*ufs device support or not*/
static inline bool streamid_is_allowed(void)
{
	return ufs_vdi.stid_support;
}

/*context id*/
static inline bool contextid_is_allowed(void)
{
	return ufs_vdi.conid_support;
}

static void ufs_sec_streamid_probe(struct ufs_hba *hba, u8 *desc_buf)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u32 ext_ufs_feature_sup = 0;
	struct device_node *node = hba->dev->of_node;

	/*check stream id feature*/
	if (hba->desc_size[QUERY_DESC_IDN_DEVICE] <
			DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 4)
		goto not_support;

	ext_ufs_feature_sup =
		get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP);

	if (!(dev_info->d_ext_ufs_feature_sup & UFS_SEC_DEV_STREAMID_SUP)) {
		dev_info(hba->dev, "stream id is not supported in UFS device\n");
		goto not_support;
	}

	/* get dt */
	ufs_vdi.stid_dt = of_property_read_bool(node, "sec,streamid-enable");

	/* read fStreamIdEn */
	if (!ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_STREAMID_EN, 0, &ufs_vdi.stid_en)) {
		dev_info(hba->dev, "streamid_en flag is %s\n",
				ufs_vdi.stid_en ? "enabled" : "disabled");
	} else
		goto not_support;

	ufs_vdi.stid_support = true;
	return;

not_support:
	ufs_vdi.stid_support = false;
	set_streamid_state(false);
}

int ufs_sec_streamid_ctrl(struct ufs_hba *hba, bool value)
{
	bool set = value;
	int ret = 0;
	enum query_opcode opcode;

	if (contextid_is_allowed() || !streamid_is_allowed()) {
		dev_info(hba->dev, "%s: stream id is not supported\n", __func__);
		return -ENOTSUPP;
	}

	if (hba->pm_op_in_progress) {
		dev_err(hba->dev, "%s: pm_op_in_progress.\n", __func__);
		return -EBUSY;
	}

	if (streamid_is_enabled() != set) {
		pm_runtime_get_sync(hba->dev);

		if (set)
			opcode = UPIU_QUERY_OPCODE_SET_FLAG;
		else
			opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

		ret = ufshcd_query_flag_retry(hba, opcode,
				QUERY_FLAG_IDN_STREAMID_EN, 0, NULL);
		if (ret)
			dev_info(hba->dev, "%s stream id is failed\n", set ? "Enabling" : "Disabling");
		else {
			set_streamid_state(set);
			dev_info(hba->dev, "stream id is %s\n", set ? "enabled" : "disabled");
		}

		pm_runtime_put(hba->dev);
	}

	return ret;
}

static void ufs_sec_contextid_probe(struct ufs_hba *hba)
{
	u8 lun = 0;
	int ret = 0;

	for (lun = 0; lun < UFS_SEC_MAX_LUN_ID; lun++) {
		if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun,
					UNIT_DESC_PARAM_CONTEXT_CAP)) {
			continue;
		}
		ret = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				UNIT_DESC_PARAM_CONTEXT_CAP, (u8 *)&ufs_vdi.conid_cap[lun], 2);

		if (!ret && ufs_vdi.conid_cap[lun]) {
			dev_info(hba->dev, "LUN[%d] has context capability. value is 0x%x\n",
					lun, ufs_vdi.conid_cap[lun]);
			if (!ufs_vdi.conid_support)
				ufs_vdi.conid_support = true;
		}
	}
}

/* panic notifier : begin */
static void ufs_sec_print_evt(struct ufs_hba *hba, u32 id,
			     char *err_name)
{
	int i;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &hba->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		// do not print if dev_reset[0]'s tstamp is in 2 sec.
		// because that happened on booting seq.
		if ((id == UFS_EVT_DEV_RESET) && (p == 0) &&
				(ktime_to_us(e->tstamp[p]) < 2000000))
			continue;

		if (e->tstamp[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, p,
			e->val[p], ktime_to_us(e->tstamp[p]));
	}
}

static void ufs_sec_print_evt_hist(struct ufs_hba *hba)
{
	ufs_sec_print_evt(hba, UFS_EVT_PA_ERR, "pa_err");
	ufs_sec_print_evt(hba, UFS_EVT_DL_ERR, "dl_err");
	ufs_sec_print_evt(hba, UFS_EVT_NL_ERR, "nl_err");
	ufs_sec_print_evt(hba, UFS_EVT_TL_ERR, "tl_err");
	ufs_sec_print_evt(hba, UFS_EVT_DME_ERR, "dme_err");
	ufs_sec_print_evt(hba, UFS_EVT_AUTO_HIBERN8_ERR,
			 "auto_hibern8_err");
	ufs_sec_print_evt(hba, UFS_EVT_FATAL_ERR, "fatal_err");
	ufs_sec_print_evt(hba, UFS_EVT_LINK_STARTUP_FAIL,
			 "link_startup_fail");
	ufs_sec_print_evt(hba, UFS_EVT_RESUME_ERR, "resume_fail");
	ufs_sec_print_evt(hba, UFS_EVT_SUSPEND_ERR,
			 "suspend_fail");
	ufs_sec_print_evt(hba, UFS_EVT_DEV_RESET, "dev_reset");
	ufs_sec_print_evt(hba, UFS_EVT_HOST_RESET, "host_reset");
	ufs_sec_print_evt(hba, UFS_EVT_ABORT, "task_abort");
}

/*
 * ufs_sec_check_and_is_err - Check and compare UFS Error counts
 * @buf: has to be filled by using SEC_UFS_ERR_SUM() or SEC_UFS_ERR_HIST_SUM()
 *
 * Returns
 *  0     - If the buf has no error counts
 *  other - If the buf is invalid or has error count value
 */
static int ufs_sec_check_and_is_err(char *buf)
{
	const char *no_err = "U0I0H0L0X0Q0R0W0F0SM0SH0";

	if (!buf || strlen(buf) < strlen(no_err))
		return -EINVAL;

	return strncmp(buf, no_err, strlen(no_err));
}

/**
 * ufs_sec_panic_callback - Print and Send UFS Error Information to AP
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
static int ufs_sec_panic_callback(struct notifier_block *nfb,
		unsigned long event, void *panic_msg)
{
	char err_buf[ERR_SUM_SIZE];
	char hist_buf[ERR_HIST_SUM_SIZE];
	char *str = (char *)panic_msg;
	bool is_FS_panic = !strncmp(str, "F2FS", 4) || !strncmp(str, "EXT4", 4);

	SEC_UFS_ERR_SUM(err_buf);
	SEC_UFS_ERR_HIST_SUM(hist_buf);
	pr_info("ufs: %s hist: %s", err_buf, hist_buf);

	secdbg_exin_set_ufs(err_buf);

	if (ufs_vdi.hba && is_FS_panic && ufs_sec_check_and_is_err(err_buf))
		ufs_sec_print_evt_hist(ufs_vdi.hba);

	return NOTIFY_OK;
}

static struct notifier_block ufs_sec_panic_notifier = {
	.notifier_call = ufs_sec_panic_callback,
	.priority = 1,
};

/* panic notifier : end */

/* reboot notifier : begin */
static int ufs_sec_reboot_notify(struct notifier_block *notify_block,
		unsigned long event, void *unused)
{
	char buf[ERR_HIST_SUM_SIZE];

	SEC_UFS_ERR_HIST_SUM(buf);
	pr_info("%s: UFS Info : %s", __func__, buf);

	return NOTIFY_OK;
}
/* reboot notifier : end */

/* I/O error uevent : begin */
static int ufs_sec_check_no_err(char *buf)
{
	const char *no_err = "U0I0H0L0X0Q0R0W0F0SM0SH0";

	if (!buf || !strncmp(buf, no_err, sizeof(no_err) - 1))
		return -EINVAL;

	return 0;
}

void ufs_sec_print_err(void)
{
	char err_buf[ERR_SUM_SIZE];
	char hist_buf[ERR_HIST_SUM_SIZE];

	SEC_UFS_ERR_SUM(err_buf);
	SEC_UFS_ERR_HIST_SUM(hist_buf);

	if (!ufs_sec_check_no_err(hist_buf))
		pr_info("ufs: %s, hist : %s", err_buf, hist_buf);
}

static void ufs_sec_err_noti_work(struct work_struct *work)
{
	int ret;
	char buf[ERR_SUM_SIZE];

	SEC_UFS_ERR_SUM(buf);
	pr_info("%s: UFS Info: %s\n", __func__, buf);

	ret = kobject_uevent(&sec_ufs_cmd_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to send uevent with err %d\n", __func__, ret);
}

static int ufs_sec_err_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	char buf[ERR_SUM_SIZE];

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);
	add_uevent_var(env, "NAME=UFSINFO");

	SEC_UFS_ERR_SUM(buf);
	return add_uevent_var(env, "DATA=%s", buf);
}

static struct device_type ufs_type = {
	.uevent = ufs_sec_err_uevent,
};

static inline bool ufs_sec_is_uevent_condition(u32 hw_rst_count)
{
	return ((hw_rst_count == 1) || !(hw_rst_count % 10));
}

static void ufs_sec_trigger_err_noti_uevent(struct ufs_hba *hba)
{
	const char *no_err = "U0I0H0L0X0Q0R0W0F0SM0SH0";
	u32 hw_rst_count = SEC_UFS_ERR_INFO_GET_VALUE(op_count, HW_RESET_count);
	char buf[ERR_SUM_SIZE];

	/* eh_flags is only set during error handling */
	if (!hba->eh_flags)
		return;

	SEC_UFS_ERR_SUM(buf);

	if (!strncmp(buf, no_err, sizeof(buf) - 1))
		return;

	if (!ufs_sec_is_uevent_condition(hw_rst_count))
		return;

	if (sec_ufs_cmd_dev)
		schedule_delayed_work(&ufs_sec_features.noti_work,
				msecs_to_jiffies(NOTI_WORK_DELAY_MS));
}
/* I/O error uevent : end */

void ufs_sec_init_logging(struct device *dev)
{
	ufs_sec_init_cmd_logging(dev);
}

void ufs_set_sec_features(struct ufs_hba *hba)
{
	int buff_len;
	u8 *desc_buf = NULL;
	int err;

	if (ufs_vdi.hba)
		return;

	ufs_vdi.hba = hba;

	/* read device desc */
	buff_len = hba->desc_size[QUERY_DESC_IDN_DEVICE];
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return;

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_DEVICE, 0, 0,
			desc_buf, &buff_len);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading device descriptor. err %d",
				__func__, err);
		goto out;
	}

	ufs_sec_set_unique_number(hba, desc_buf);
	ufs_sec_get_health_desc(hba);
	ufs_sec_wb_probe(hba, desc_buf);

	ufs_sec_contextid_probe(hba);
	ufs_sec_streamid_probe(hba, desc_buf);

	ufs_sysfs_add_sec_nodes(hba);

	atomic_notifier_chain_register(&panic_notifier_list,
			&ufs_sec_panic_notifier);

	ufs_sec_features.reboot_notify.notifier_call = ufs_sec_reboot_notify;
	register_reboot_notifier(&ufs_sec_features.reboot_notify);
	sec_ufs_cmd_dev->type = &ufs_type;
	INIT_DELAYED_WORK(&ufs_sec_features.noti_work, ufs_sec_err_noti_work);

out:
	if (desc_buf)
		kfree(desc_buf);
}

void ufs_sec_feature_config(struct ufs_hba *hba)
{
	ufs_sec_wb_config(hba, true);
	ufs_sec_trigger_err_noti_uevent(hba);

	if (contextid_is_allowed() && streamid_is_enabled())
		ufs_sec_streamid_ctrl(hba, false);
	else {
		/* set streamid_en flag with dt value*/
		ufs_sec_streamid_ctrl(hba, ufs_vdi.stid_dt);
	}
}

void ufs_remove_sec_features(struct ufs_hba *hba)
{
	ufs_sysfs_remove_sec_nodes(hba);
	unregister_reboot_notifier(&ufs_sec_features.reboot_notify);
}

/* check error info : begin */
void ufs_sec_check_hwrst_cnt(void)
{
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;

	SEC_UFS_ERR_COUNT_INC(op_cnt->HW_RESET_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
#if IS_ENABLED(CONFIG_SEC_ABC)
	if ((op_cnt->HW_RESET_count % 10) == 0)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=storage@INFO=ufs_hwreset_err");
#else
		sec_abc_send_event("MODULE=storage@WARN=ufs_hwreset_err");
#endif
#endif
}

static void ufs_sec_check_link_startup_error_cnt(void)
{
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;

	SEC_UFS_ERR_COUNT_INC(op_cnt->link_startup_count, UINT_MAX);
	SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
}

static void ufs_sec_check_medium_error_region(struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_err_log *sense_err_log = &ufs_err_info.sense_err_log;
	unsigned int lba_count = sense_err_log->issue_LBA_count;
	unsigned long region_bit = 0;
	int i = 0;

	if (ufs_cmd->lun == 0) {
		if (lba_count < SEC_MAX_LBA_LOGGING) {
			for (i = 0; i < SEC_MAX_LBA_LOGGING; i++) {
				if (sense_err_log->issue_LBA_list[i] == ufs_cmd->lba)
					return;
			}
			sense_err_log->issue_LBA_list[lba_count] = ufs_cmd->lba;
			sense_err_log->issue_LBA_count++;
		}

		region_bit = ufs_cmd->lba / SEC_ISSUE_REGION_STEP;
		if (region_bit > 51)
			region_bit = 52;
	} else if (ufs_cmd->lun < UFS_UPIU_WLUN_ID) {
		region_bit = (unsigned long)(64 - ufs_cmd->lun);
	}

	sense_err_log->issue_region_map |= ((u64)1 << region_bit);
}

static void ufs_sec_uic_cmd_error_check(struct ufs_hba *hba, u32 cmd,
		bool timeout)

{
	struct SEC_UFS_UIC_cmd_count *uic_cmd_cnt = &ufs_err_info.UIC_cmd_count;
	struct SEC_UFS_op_count *op_cnt = &ufs_err_info.op_count;
	int uic_cmd_result;

	uic_cmd_result = hba->active_uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;

	/* check UIC CMD result */
	if (!timeout && (uic_cmd_result == UIC_CMD_RESULT_SUCCESS))
		return;

	switch (cmd & COMMAND_OPCODE_MASK) {
	case UIC_CMD_DME_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_GET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_GET_err, U8_MAX);
		break;
	case UIC_CMD_DME_PEER_SET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_PEER_SET_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWERON:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWERON_err, U8_MAX);
		break;
	case UIC_CMD_DME_POWEROFF:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_POWEROFF_err, U8_MAX);
		break;
	case UIC_CMD_DME_ENABLE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_ENABLE_err, U8_MAX);
		break;
	case UIC_CMD_DME_RESET:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_RESET_err, U8_MAX);
		break;
	case UIC_CMD_DME_END_PT_RST:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_END_PT_RST_err, U8_MAX);
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_LINK_STARTUP_err, U8_MAX);
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_ENTER_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_enter_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_HIBER_EXIT_err, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->Hibern8_exit_count, UINT_MAX);
		SEC_UFS_ERR_COUNT_INC(op_cnt->op_err, UINT_MAX);
		break;
	case UIC_CMD_DME_TEST_MODE:
		SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->DME_TEST_MODE_err, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(uic_cmd_cnt->UIC_cmd_err, UINT_MAX);
}

static void ufs_sec_uic_fatal_check(u32 errors)
{
	struct SEC_UFS_Fatal_err_count *fatal_err_cnt = &ufs_err_info.Fatal_err_count;

	if (errors & DEVICE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->DFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CONTROLLER_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & SYSTEM_BUS_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->SBFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	if (errors & CRYPTO_ENGINE_FATAL_ERROR) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->CEFE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
	/* UIC_LINK_LOST : can not be checked in ufshcd.c */
	if (errors & UIC_LINK_LOST) {
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->LLE, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(fatal_err_cnt->Fatal_err, UINT_MAX);
	}
}

static void ufs_sec_uic_error_check(enum ufs_event_type evt, u32 reg)
{
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &ufs_err_info.UIC_err_count;
	int check_val = 0;

	switch (evt) {
	case UFS_EVT_PA_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);

		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_linereset, UINT_MAX);

		check_val = reg & UIC_PHY_ADAPTER_LAYER_LANE_ERR_MASK;
		if (check_val)
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->PA_ERR_lane[check_val - 1], UINT_MAX);
		break;
	case UFS_EVT_DL_ERR:
		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_PA_INIT_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		if (reg & UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT) {
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DL_TC_REPLAY_ERROR_cnt, U8_MAX);
			SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		}
		break;
	case UFS_EVT_NL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->NL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_TL_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->TL_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	case UFS_EVT_DME_ERR:
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->DME_ERROR_cnt, U8_MAX);
		SEC_UFS_ERR_COUNT_INC(uic_err_cnt->UIC_err, UINT_MAX);
		break;
	default:
		break;
	}
}

static void ufs_sec_tm_error_check(u8 tm_cmd)
{
	struct SEC_UFS_UTP_count *utp_err = &ufs_err_info.UTP_count;

	switch (tm_cmd) {
	case UFS_QUERY_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_query_task_count, U8_MAX);
		break;
	case UFS_ABORT_TASK:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_abort_task_count, U8_MAX);
		break;
	case UFS_LOGICAL_RESET:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTMR_logical_reset_count, U8_MAX);
		break;
	default:
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (tm_cmd == UFS_LOGICAL_RESET)
		ufs_vdi.device_stuck = true;
#endif
}

void ufs_sec_check_device_stuck(void)
{
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (ufs_vdi.device_stuck) {
		/* waiting for cache flush and make a panic */
		ssleep(2);
		panic("UFS TM ERROR\n");
	}
#endif

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	if (ufs_vdi.hba && ufs_vdi.hba->eh_flags)
		/* do not recover system if test mode is enabled */
		/* 1. reset recovery is in progress from ufshcd_err_handler */
		/* 2. exynos_ufs_init_host has been succeeded */
		BUG();
#endif
}

static void ufs_sec_utp_error_check(struct ufs_hba *hba, int tag)
{
	struct SEC_UFS_UTP_count *utp_err = &ufs_err_info.UTP_count;
	struct ufshcd_lrb *lrbp = NULL;
	int opcode = 0;

	/* check tag value */
	if (tag >= hba->nutrs)
		return;

	lrbp = &hba->lrb[tag];
	/* check lrbp */
	if (!lrbp || !lrbp->cmd || (lrbp->task_tag != tag))
		return;

	opcode = lrbp->cmd->cmnd[0];

	switch (opcode) {
	case WRITE_10:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_write_err, U8_MAX);
		break;
	case READ_10:
	case READ_16:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_read_err, U8_MAX);
		break;
	case SYNCHRONIZE_CACHE:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_sync_cache_err, U8_MAX);
		break;
	case UNMAP:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_unmap_err, U8_MAX);
		break;
	default:
		SEC_UFS_ERR_COUNT_INC(utp_err->UTR_etc_err, U8_MAX);
		break;
	}

	SEC_UFS_ERR_COUNT_INC(utp_err->UTP_err, UINT_MAX);
}

static void ufs_sec_query_error_check(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
		bool timeout)
{
	struct SEC_UFS_QUERY_count *query_cnt = &ufs_err_info.query_count;
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;
	enum dev_cmd_type cmd_type = hba->dev_cmd.type;
	int ocs;

	ocs = le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;

	/* check Overall Command Status */
	if (!timeout && (ocs == OCS_SUCCESS))
		return;

	if (timeout) {
		opcode = ufs_sec_features.last_qcmd;
		cmd_type = ufs_sec_features.qcmd_type;
	}

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		SEC_UFS_ERR_COUNT_INC(query_cnt->NOP_err, U8_MAX);
	} else {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Desc_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			SEC_UFS_ERR_COUNT_INC(query_cnt->W_Attr_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->R_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Set_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Clear_Flag_err, U8_MAX);
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			SEC_UFS_ERR_COUNT_INC(query_cnt->Toggle_Flag_err, U8_MAX);
			break;
		default:
			break;
		}
	}

	SEC_UFS_ERR_COUNT_INC(query_cnt->Query_err, UINT_MAX);
}

void ufs_sec_check_op_err(struct ufs_hba *hba, enum ufs_event_type evt, void *data)
{
	u32 error_val = *(u32 *)data;

	switch (evt) {
	case UFS_EVT_LINK_STARTUP_FAIL:
		ufs_sec_check_link_startup_error_cnt();
		break;
	case UFS_EVT_DEV_RESET:
		break;
	case UFS_EVT_PA_ERR:
	case UFS_EVT_DL_ERR:
	case UFS_EVT_NL_ERR:
	case UFS_EVT_TL_ERR:
	case UFS_EVT_DME_ERR:
		if (error_val)
			ufs_sec_uic_error_check(evt, error_val);
		break;
	case UFS_EVT_FATAL_ERR:
		if (error_val)
			ufs_sec_uic_fatal_check(error_val);
		break;
	case UFS_EVT_ABORT:
		ufs_sec_utp_error_check(hba, (int)error_val);
		break;
	case UFS_EVT_HOST_RESET:
		break;
	case UFS_EVT_SUSPEND_ERR:
	case UFS_EVT_RESUME_ERR:
		break;
	case UFS_EVT_AUTO_HIBERN8_ERR:
		break;
	default:
		break;
	}
}

static void ufs_sec_sense_err_check(struct ufshcd_lrb *lrbp, struct ufs_sec_cmd_info *ufs_cmd)
{
	struct SEC_SCSI_SENSE_count *sense_err = &ufs_err_info.sense_count;
	u8 sense_key = 0;
	u8 asc = 0;
	u8 ascq = 0;
	bool secdbgMode = false;

	sense_key = lrbp->ucd_rsp_ptr->sr.sense_data[2] & 0x0F;
	if (sense_key != MEDIUM_ERROR && sense_key != HARDWARE_ERROR)
		return;

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	secdbgMode = secdbg_mode_enter_upload();
#endif
	asc = lrbp->ucd_rsp_ptr->sr.sense_data[12];
	ascq = lrbp->ucd_rsp_ptr->sr.sense_data[13];

	pr_err("UFS : sense key 0x%x(asc 0x%x, ascq 0x%x), opcode 0x%x, lba 0x%x, len 0x%x.\n",
			sense_key, asc, ascq,
			ufs_cmd->opcode, ufs_cmd->lba, ufs_cmd->transfer_len);

	if (sense_key == MEDIUM_ERROR) {
		sense_err->scsi_medium_err++;
		ufs_sec_check_medium_error_region(ufs_cmd);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=storage@INFO=ufs_medium_err");
#else
		sec_abc_send_event("MODULE=storage@WARN=ufs_medium_err");
#endif
#endif
		if (secdbgMode)
			panic("ufs medium error\n");
	} else {
		sense_err->scsi_hw_err++;
		if (secdbgMode)
			panic("ufs hardware error\n");
	}
}

void ufs_sec_print_err_info(struct ufs_hba *hba)
{
	dev_err(hba->dev, "Count: %u UIC: %u UTP: %u QUERY: %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(op_count, HW_RESET_count),
		SEC_UFS_ERR_INFO_GET_VALUE(UIC_err_count, UIC_err),
		SEC_UFS_ERR_INFO_GET_VALUE(UTP_count, UTP_err),
		SEC_UFS_ERR_INFO_GET_VALUE(query_count, Query_err));

	dev_err(hba->dev, "Sense Key: medium: %u, hw: %u\n",
		SEC_UFS_ERR_INFO_GET_VALUE(sense_count, scsi_medium_err),
		SEC_UFS_ERR_INFO_GET_VALUE(sense_count, scsi_hw_err));
}

/* check error info : end */

static bool ufs_sec_get_scsi_cmd_info(struct ufshcd_lrb *lrbp, struct ufs_sec_cmd_info *ufs_cmd)
{
	struct scsi_cmnd *cmd;

	if (!lrbp || !lrbp->cmd || !ufs_cmd)
		return false;

	cmd = lrbp->cmd;

	ufs_cmd->opcode = (u8)(*cmd->cmnd);
	ufs_cmd->lba = (cmd->cmnd[2] << 24) | (cmd->cmnd[3] << 16) | (cmd->cmnd[4] << 8) | cmd->cmnd[5];
	ufs_cmd->transfer_len = (cmd->cmnd[7] << 8) | cmd->cmnd[8];
	ufs_cmd->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);

	return true;
}

/*
 * vendor hooks
 * check include/trace/hooks/ufshcd.h
 */
static void sec_android_vh_ufs_prepare_command(void *data, struct ufs_hba *hba,
		struct request *req, struct ufshcd_lrb *lrbp, int *err)
{
	struct scsi_cmnd *cmd = lrbp->cmd;

	/* set the group number with write hint when stream id is enabled */
	if ((cmd->cmnd[0] == WRITE_10 || cmd->cmnd[0] == WRITE_16) &&
			(streamid_is_enabled() && !contextid_is_allowed()) &&
			req->bio->bi_write_hint) {
		if (cmd->cmnd[0] == WRITE_10)
			cmd->cmnd[6] = req->bio->bi_write_hint & 0x1f;
		else
			cmd->cmnd[14] = req->bio->bi_write_hint & 0x1f;
	}

	*err = 0;
}

static void sec_android_vh_ufs_send_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	bool is_scsi_cmd = false;
	unsigned long flags;
	struct ufs_query_req *request = NULL;
	enum dev_cmd_type cmd_type;
	enum query_opcode opcode;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
		ufs_sec_log_cmd(hba, lrbp, UFS_SEC_CMD_SEND, &ufs_cmd, 0);
		if (ufs_sec_is_wb_allowed() && (ufs_cmd.opcode == WRITE_10)) {
			spin_lock_irqsave(hba->host->host_lock, flags);

			ufs_wb.wb_current_block += ufs_cmd.transfer_len;
			ufs_wb.wb_current_rqs++;

			ufs_sec_wb_update_state(hba);

			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
	} else {
		if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_NOP_SEND, NULL, 0);
		else if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_QUERY_SEND, NULL, 0);

		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.qcmd_complete)
			ufs_sec_query_error_check(hba, lrbp, true);

		request = &hba->dev_cmd.query.request;
		opcode = request->upiu_req.opcode;
		cmd_type = hba->dev_cmd.type;

		ufs_sec_features.last_qcmd = opcode;
		ufs_sec_features.qcmd_type = cmd_type;
		ufs_sec_features.qcmd_complete = false;
	}
}

static void sec_android_vh_ufs_compl_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_sec_cmd_info ufs_cmd = { 0, };
	bool is_scsi_cmd = false;
	unsigned long flags;

	is_scsi_cmd = ufs_sec_get_scsi_cmd_info(lrbp, &ufs_cmd);

	if (is_scsi_cmd) {
		ufs_sec_log_cmd(hba, lrbp, UFS_SEC_CMD_COMP, &ufs_cmd, 0);
		ufs_sec_sense_err_check(lrbp, &ufs_cmd);

		if (ufs_sec_is_wb_allowed() && (ufs_cmd.opcode == WRITE_10)) {
			spin_lock_irqsave(hba->host->host_lock, flags);

			ufs_wb.wb_current_block -= ufs_cmd.transfer_len;
			ufs_wb.wb_current_rqs--;

			if (hba->wb_enabled)
				ufs_wb.wb_curr_issued_block += (unsigned int)ufs_cmd.transfer_len;

			ufs_sec_wb_update_state(hba);

			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}

		/*
		 * check hba->req_abort_count, if the cmd is aborting
		 * it's the one way to check aborting
		 * hba->req_abort_count is cleared in queuecommand and after
		 * error handling
		 */
		if (hba->req_abort_count > 0)
			ufs_sec_utp_error_check(hba, lrbp->task_tag);
	} else {
		if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_NOP_COMP, NULL, 0);
		else if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
			ufs_sec_log_cmd(hba, lrbp, UFS_SEC_QUERY_COMP, NULL, 0);

		ufs_sec_features.qcmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_query_error_check(hba, lrbp, false);
	}
}

static void sec_android_vh_ufs_send_uic_command(void *data, struct ufs_hba *hba,  struct uic_command *ucmd,
		const char *str)
{
	u32 cmd;
	u8 cmd_id = 0;

	if (!strcmp(str, "send")) {
		/* in timeout error case, last cmd is not completed */
		if (!ufs_sec_features.ucmd_complete) {
			ufs_sec_uic_cmd_error_check(hba,
					ufs_sec_features.last_ucmd, true);
		}

		cmd = ucmd->command;
		ufs_sec_features.last_ucmd = cmd;
		ufs_sec_features.ucmd_complete = false;

		cmd_id = (u8)(cmd & COMMAND_OPCODE_MASK);
		ufs_sec_log_cmd(hba, NULL, UFS_SEC_UIC_SEND, NULL, cmd_id);
	} else {
		cmd = ufshcd_readl(hba, REG_UIC_COMMAND);

		ufs_sec_features.ucmd_complete = true;

		/* check and count error, except timeout */
		ufs_sec_uic_cmd_error_check(hba, cmd, false);

		cmd_id = (u8)(cmd & COMMAND_OPCODE_MASK);
		ufs_sec_log_cmd(hba, NULL, UFS_SEC_UIC_COMP, NULL, cmd_id);
	}
}

static void sec_android_vh_ufs_send_tm_command(void *data, struct ufs_hba *hba, int tag, const char *str)
{
	struct utp_task_req_desc treq = { { 0 }, };
	u8 tm_func = 0;
	int sec_log_str_t = 0;

	memcpy(&treq, hba->utmrdl_base_addr + tag, sizeof(treq));

	tm_func = (be32_to_cpu(treq.req_header.dword_1) >> 16) & 0xFF;

	if (!strncmp(str, "tm_send", sizeof("tm_send")))
		sec_log_str_t = UFS_SEC_TM_SEND;
	else if (!strncmp(str, "tm_complete", sizeof("tm_complete")))
		sec_log_str_t = UFS_SEC_TM_COMP;
	else if (!strncmp(str, "tm_complete_err", sizeof("tm_complete_err"))) {
		ufs_sec_tm_error_check(tm_func);
		sec_log_str_t = UFS_SEC_TM_ERR;
	} else {
		dev_err(hba->dev, "%s: undefind ufs tm cmd\n", __func__);
		return;
	}

	ufs_sec_log_cmd(hba, NULL, sec_log_str_t, NULL, tm_func);

#if IS_ENABLED(CONFIG_SCSI_UFS_TEST_MODE)
	if (!strncmp(str, "tm_complete", sizeof("tm_complete"))) {
		dev_err(hba->dev,
			"%s: ufs tm cmd is succeeded and forced BUG called\n", __func__);
		ssleep(2);
		BUG();
	}
#endif
}

static void sec_android_vh_ufs_check_int_errors(void *data, struct ufs_hba *hba, bool queue_eh_work)
{
}

static void sec_android_vh_ufs_update_sdev(void *data, struct scsi_device *sdev)
{
	/* set req timeout */
	blk_queue_rq_timeout(sdev->request_queue, SCSI_UFS_TIMEOUT);
}

static void sec_android_vh_ufs_update_sysfs(void *data, struct ufs_hba *hba)
{
}

void ufs_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_ufs_check_int_errors(sec_android_vh_ufs_check_int_errors, NULL);
	register_trace_android_vh_ufs_compl_command(sec_android_vh_ufs_compl_command, NULL);
	register_trace_android_vh_ufs_prepare_command(sec_android_vh_ufs_prepare_command, NULL);
	register_trace_android_vh_ufs_send_command(sec_android_vh_ufs_send_command, NULL);
	register_trace_android_vh_ufs_send_tm_command(sec_android_vh_ufs_send_tm_command, NULL);
	register_trace_android_vh_ufs_send_uic_command(sec_android_vh_ufs_send_uic_command, NULL);
	register_trace_android_vh_ufs_update_sdev(sec_android_vh_ufs_update_sdev, NULL);
	register_trace_android_vh_ufs_update_sysfs(sec_android_vh_ufs_update_sysfs, NULL);
}
