// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/sec_class.h>

#include "dw_mmc-exynos.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct mmc_sec_op_info sd_op_info;

/*
 * These are for SDcard error counting.
 *
 * sd_err_info
 *   : accumulated err sum while SDcard is presented.
 * sd_status_err_info
 *   : accumulated status err sum while SDcard is presented.
 *
 * sd_err_info_backup
 *   : backup for sd_err_info after HQM reads sd_err_info.
 * sd_status_err_info_backup
 *   : backup for sd_status_err_info after HQM reads sd_status_err_info.
 */
struct mmc_sec_err_info sd_err_info[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
struct mmc_sec_status_err_info sd_status_err_info[MAX_MMC_HOST_IDX];
struct mmc_sec_err_info sd_err_info_backup[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
struct mmc_sec_status_err_info sd_status_err_info_backup[MAX_MMC_HOST_IDX];

struct device *sd_detection_cmd_dev;
struct device *sd_data_cmd_dev;
struct device *sd_info_cmd_dev;

static int mmc_sec_sdcard_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	int host_idx;
	int retval;
	bool card_exist;

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		card_exist = true;
		host_idx = host->slot->mmc->index;
	} else
		card_exist = false;

	retval = add_uevent_var(env, "IOERROR=%s", card_exist ? (
			((sd_status_err_info[host_idx].ge_cnt && !(sd_status_err_info[host_idx].ge_cnt % 1000)) ||
			 (sd_status_err_info[host_idx].ecc_cnt && !(sd_status_err_info[host_idx].ecc_cnt % 1000)) ||
			 (sd_status_err_info[host_idx].wp_cnt && !(sd_status_err_info[host_idx].wp_cnt % 100)) ||
			 (sd_status_err_info[host_idx].oor_cnt && !(sd_status_err_info[host_idx].oor_cnt % 100)))
			? "YES" : "NO")	: "NoCard");

	return retval;
}

static void mmc_sec_sdcard_noti_work(struct work_struct *work)
{
	struct mmc_sec_op_info *op_info = container_of(work, struct mmc_sec_op_info, noti_work);
	int host_idx = op_info->mmc->index;
	int ret;

	if (!op_info->mmc->card)
		return;

	pr_info("%s: Send Notification about SD Card IO Error\n", mmc_hostname(op_info->mmc));
	sd_status_err_info[host_idx].noti_cnt++;

	ret = kobject_uevent(&sd_detection_cmd_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to Send Uevent with err %d\n", __func__, ret);
}

static struct device_type sdcard_type = {
	.uevent = mmc_sec_sdcard_uevent,
};

static void mmc_error_count_log(int host_idx, int index, int error, u32 status)
{
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	int i = 0;
	int cpu = raw_smp_processor_id();

	if (!error)
		return;

	/*
	 * -EIO (-5) : SDMMC_INT_RESP_ERR error case. So log as CRC.
	 * -ENOMEDIUM (-123), etc : SW timeout and other error. So log as TIMEOUT.
	 */
	switch (error) {
	case -EIO:
		error = -EILSEQ;
		break;
	case -EILSEQ:
		break;
	default:
		error = -ETIMEDOUT;
		break;
	}

	for (i = 0; i < MAX_ERR_TYPE_INDEX; i++) {
		if (err_log[index + i].err_type == error) {
			index += i;
			break;
		}
	}
	if (i >= MAX_ERR_TYPE_INDEX)
		return;

	// 1st status logging or not trans
	if (!err_log[index].status || !(R1_CURRENT_STATE(status) & R1_STATE_TRAN))
		err_log[index].status = status;
	if (!err_log[index].first_issue_time)
		err_log[index].first_issue_time = cpu_clock(cpu);
	err_log[index].last_issue_time = cpu_clock(cpu);
	err_log[index].count++;
}

static void mmc_check_status_err(struct mmc_card *card, int host_idx, u32 status)
{
	bool noti = false;

	if (status & R1_ERROR) {
		sd_status_err_info[host_idx].ge_cnt++;
		if (!(sd_status_err_info[host_idx].ge_cnt % 1000))
			noti = true;
	}
	if (status & R1_CC_ERROR)
		sd_status_err_info[host_idx].cc_cnt++;
	if (status & R1_CARD_ECC_FAILED) {
		sd_status_err_info[host_idx].ecc_cnt++;
		if (!(sd_status_err_info[host_idx].ecc_cnt % 1000))
			noti = true;
	}
	if (status & R1_WP_VIOLATION) {
		sd_status_err_info[host_idx].wp_cnt++;
		if (!(sd_status_err_info[host_idx].wp_cnt % 100))
			noti = true;
	}
	if (status & R1_OUT_OF_RANGE) {
		sd_status_err_info[host_idx].oor_cnt++;
		if (!(sd_status_err_info[host_idx].oor_cnt % 100))
			noti = true;
	}

	/*
	 * Make Notification about SD Card Errors
	 *
	 * Condition :
	 *   GE, ECC : Every 1000 errors
	 *   WP, OOR : Every  100 errors
	 */
	if (noti && mmc_card_sd(card) && sd_detection_cmd_dev)
		schedule_work(&sd_op_info.noti_work);
}

static void mmc_card_error_logging(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int host_idx = mmc->index;
	u32 status = 0;

	status = (mrq->sbc ? mrq->sbc->resp[0] : 0) |
		(mrq->stop ? mrq->stop->resp[0] : 0) |
		(mrq->cmd ? mrq->cmd->resp[0] : 0);

	if (status & STATUS_MASK)
		mmc_check_status_err(mmc->card, host_idx, status);

	if (mrq->sbc && mrq->sbc->error)
		mmc_error_count_log(host_idx, MMC_LOG_SBC_OFFSET, mrq->sbc->error, status);
	if (mrq->cmd && mrq->cmd->error)
		mmc_error_count_log(host_idx, MMC_LOG_CMD_OFFSET, mrq->cmd->error, status);
	if (mrq->data && mrq->data->error)
		mmc_error_count_log(host_idx, MMC_LOG_DATA_OFFSET, mrq->data->error, status);
	if (mrq->stop && mrq->stop->error)
		mmc_error_count_log(host_idx, MMC_LOG_STOP_OFFSET, mrq->stop->error, status);
	/*
	 * in block.c
	 *    #define MMC_BLK_TIMEOUT_MS  (20 * 1000)
	 *    refer to card_busy_detect()
	 * so, check CMD13's response(status)
	 *    if there is no other CMD for 18secs or more.
	 */
	if (mrq->cmd->opcode == MMC_SEND_STATUS &&
		time_after(jiffies, sd_op_info.tstamp_last_cmd + msecs_to_jiffies(18 * 1000))) {
		if (status && (!(status & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(status) == R1_STATE_PRG))) {
			// card stuck in prg state
			mmc_error_count_log(host_idx, MMC_LOG_BUSY_OFFSET, -ETIMEDOUT, status);
			// not to check card busy again
			sd_op_info.tstamp_last_cmd = jiffies;
		}
	}
}

void mmc_sec_check_request_error(struct dw_mci *host, struct mmc_request *mrq)
{
	struct mmc_host *mmc = host->slot->mmc;

	if (!mmc->card || !mrq || !mrq->cmd)
		return;

	/* return if the cmd is tuning block */
	if (((mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK) ||
			(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200)))
		return;

	/* set CMD(except CMD13) timestamp to check card stuck */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		sd_op_info.tstamp_last_cmd = jiffies;

	/*
	 * cmd->flags info
	 * #define MMC_CMD_MASK    (3 << 5) : non-SPI command type
	 * #define MMC_CMD_AC      (0 << 5) : Addressed (point-to-point) commands
	 * #define MMC_CMD_ADTC    (1 << 5) : Addressed (point-to-point) data transfer commands
	 * #define MMC_CMD_BC      (2 << 5) : Broadcast commands
	 * #define MMC_CMD_BCR     (3 << 5) : Broadcast commands with response
	 *
	 * We should log the errors in case of AC or ADTC type
	 */
	if ((mrq->cmd->flags & MMC_RSP_PRESENT) &&
			!(mrq->cmd->flags & MMC_CMD_BC)) {
		/* no need to check cmd flag has MMC_RSP_136 set */
		/*
		 * return immediately if the cmd->opcode is MMC_APP_CMD
		 *    in SDcard case, CMD55 is sent with MMC_CMD_AC flag
		 *    in eMMC case, CMD55 is not used
		 */
		if ((mrq->cmd->flags & MMC_RSP_136) ||
				(mrq->cmd->opcode == MMC_APP_CMD))
			return;

		mmc_card_error_logging(mmc, mrq);
	}
}

void mmc_sec_sdcard_clear_err_count(struct mmc_host *host)
{
	int host_idx = host->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	int i = 0;

	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		err_log[i].status = 0;
		err_log[i].first_issue_time = 0;
		err_log[i].last_issue_time = 0;
		err_log[i].count = 0;
	}

	memset(sd_err_info_backup[host_idx], 0,
			sizeof(struct mmc_sec_err_info) * MAX_ERR_LOG_INDEX);
	memset(&sd_status_err_info[host_idx], 0, sizeof(struct mmc_sec_status_err_info));
	memset(&sd_status_err_info_backup[host_idx], 0, sizeof(struct mmc_sec_status_err_info));
}

void mmc_sec_init_err_count(struct dw_mci *host)
{
	struct mmc_host *mmc = host->slot->mmc;
	int host_idx = mmc->index;
	static const char *const req_types[] = {
		"sbc  ", "cmd  ", "data ", "stop ", "busy "
	};
	int i = 0;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];

	/*
	 * Init err_log[]
	 * //sbc
	 * card->err_log[0].err_type = -EILSEQ;
	 * card->err_log[1].err_type = -ETIMEDOUT;
	 * ...
	 */
	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		snprintf(err_log[i].type, sizeof(char) * 5, "%s",
				req_types[i / MAX_ERR_TYPE_INDEX]);

		err_log[i].err_type =
			(i % MAX_ERR_TYPE_INDEX == 0) ?	-EILSEQ : -ETIMEDOUT;
	}
}

void mmc_sec_detect_interrupt(struct dw_mci *host)
{
	struct mmc_host *mmc;

	if (sd_op_info.card_detect_cnt < UINT_MAX)
		sd_op_info.card_detect_cnt++;

	if (host->slot && host->slot->mmc) {
		mmc = host->slot->mmc;
		mmc->trigger_card_event = true;

		mmc_sec_sdcard_clear_err_count(mmc);
	}
}

void mmc_set_sec_features(struct platform_device *pdev)
{
	struct dw_mci *host = dev_get_drvdata(&pdev->dev);
	struct device_node *np = host->dev->of_node;
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (of_get_property(np, "sec-sd-slot-type", NULL))
		of_property_read_u32(np,
				"sec-sd-slot-type", &priv->sec_sd_slot_type);
	else {
		if (priv->cd_gpio != -1) /* treat default SD slot if cd_gpio is defined */
			priv->sec_sd_slot_type = SEC_HOTPLUG_SD_SLOT;
		else
			priv->sec_sd_slot_type = -1;
	}

	mmc_sec_init_err_count(host);

	mmc_sec_init_sysfs(host);

	/* SDcard uevent register. */
	if ((priv->sec_sd_slot_type) >= SEC_NO_DET_SD_SLOT) {
		sd_op_info.mmc = host->slot->mmc;
		sd_detection_cmd_dev->type = &sdcard_type;
		INIT_WORK(&sd_op_info.noti_work, mmc_sec_sdcard_noti_work);
	}
}
