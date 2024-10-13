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
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/sec_class.h>

#include "dw_mmc-exynos.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

static ssize_t sd_detection_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (priv->sec_sd_slot_type > SEC_NO_DET_SD_SLOT && !gpio_is_valid(priv->cd_gpio))
		goto gpio_error;

	if (gpio_get_value(priv->cd_gpio) ^ (host->pdata->use_gpio_invert)
			&& (priv->sec_sd_slot_type == SEC_HYBRID_SD_SLOT)) {
		dev_info(host->dev, "SD slot tray Removed.\n");
		return sprintf(buf, "Notray\n");
	}

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		dev_info(host->dev, "SD card inserted.\n");
		return sprintf(buf, "Insert\n");
	}
	dev_info(host->dev, "SD card removed.\n");
	return sprintf(buf, "Remove\n");

gpio_error:
	dev_info(host->dev, "%s : External SD detect pin Error\n", __func__);
	return  sprintf(buf, "Error\n");
}

static ssize_t sd_detection_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_info(dev, "%s : CD count is = %u\n", __func__, sd_op_info.card_detect_cnt);
	return sprintf(buf, "%u\n", sd_op_info.card_detect_cnt);
}

static ssize_t sd_detection_maxmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	const char *uhs_bus_speed_mode = "";
	struct device_node *np = host->dev->of_node;

	if (of_find_property(np, "sd-uhs-sdr104", NULL))
		uhs_bus_speed_mode = "SDR104";
	else if (of_find_property(np, "sd-uhs-ddr50", NULL))
		uhs_bus_speed_mode = "DDR50";
	else if (of_find_property(np, "sd-uhs-sdr50", NULL))
		uhs_bus_speed_mode = "SDR50";
	else if (of_find_property(np, "sd-uhs-sdr25", NULL))
		uhs_bus_speed_mode = "SDR25";
	else if (of_find_property(np, "sd-uhs-sdr12", NULL))
		uhs_bus_speed_mode = "SDR12";
	else
		uhs_bus_speed_mode = "HS";

	dev_info(host->dev, "%s : Max supported Host Speed Mode = %s\n", __func__, uhs_bus_speed_mode);
	return  sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sd_detection_curmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);

	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12",
		[UHS_SDR25_BUS_SPEED] = "SDR25",
		[UHS_SDR50_BUS_SPEED] = "SDR50",
		[UHS_SDR104_BUS_SPEED] = "SDR104",
		[UHS_DDR50_BUS_SPEED] = "DDR50",
	};

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		if (mmc_card_uhs(host->slot->mmc->card))
			uhs_bus_speed_mode = uhs_speeds[host->slot->mmc->card->sd_bus_speed];
		else
			uhs_bus_speed_mode = "HS";
	} else
		uhs_bus_speed_mode = "No Card";

	dev_info(host->dev, "%s : Current SD Card Speed = %s\n", __func__, uhs_bus_speed_mode);
	return  sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sdcard_summary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_card *card;
	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12",
		[UHS_SDR25_BUS_SPEED] = "SDR25",
		[UHS_SDR50_BUS_SPEED] = "SDR50",
		[UHS_SDR104_BUS_SPEED] = "SDR104",
		[UHS_DDR50_BUS_SPEED] = "DDR50",
	};
	static const char *const unit[] = {"KB", "MB", "GB", "TB"};
	unsigned int size, serial;
	int	digit = 1;
	char ret_size[6];

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		card = host->slot->mmc->card;

		/* MANID */
		/* SERIAL */
		serial = card->cid.serial & (0x0000FFFF);

		/*SIZE*/
		if (card->csd.read_blkbits == 9)			/* 1 Sector = 512 Bytes */
			size = (card->csd.capacity) >> 1;
		else if (card->csd.read_blkbits == 11)	/* 1 Sector = 2048 Bytes */
			size = (card->csd.capacity) << 1;
		else								/* 1 Sector = 1024 Bytes */
			size = card->csd.capacity;

		if (size >= 380000000 && size <= 410000000) {	/* QUIRK 400GB SD Card */
			sprintf(ret_size, "400GB");
		} else if (size >= 190000000 && size <= 210000000) {	/* QUIRK 200GB SD Card */
			sprintf(ret_size, "200GB");
		} else {
			while ((size >> 1) > 0) {
				size = size >> 1;
				digit++;
			}
			sprintf(ret_size, "%d%s", 1 << (digit%10), unit[digit/10]);
		}

		/* SPEEDMODE */
		if (mmc_card_uhs(card))
			uhs_bus_speed_mode = uhs_speeds[card->sd_bus_speed];
		else if (mmc_card_hs(card))
			uhs_bus_speed_mode = "HS";
		else
			uhs_bus_speed_mode = "DS";

		/* SUMMARY */
		dev_info(host->dev, "MANID : 0x%02X, SERIAL : %04X, SIZE : %s, SPEEDMODE : %s\n",
				card->cid.manfid, serial, ret_size, uhs_bus_speed_mode);
		return sprintf(buf, "\"MANID\":\"0x%02X\",\"SERIAL\":\"%04X\""\
				",\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\",\"NOTI\":\"%d\"\n",
				card->cid.manfid, serial, ret_size, uhs_bus_speed_mode,
				sd_status_err_info[host->slot->mmc->index].noti_cnt);
	} else {
		/* SUMMARY : No SD Card Case */
		dev_info(host->dev, "%s : No SD Card\n", __func__);
		return sprintf(buf, "\"MANID\":\"NoCard\",\"SERIAL\":\"NoCard\""\
			",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\",\"NOTI\":\"NoCard\"\n");
	}
}

static ssize_t sd_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int host_idx = mmc->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	u64 total_cnt = 0;
	int len = 0;
	int i = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	for (i = 0; i < 6; i++) {
		if (total_cnt < U64_MAX)
			total_cnt += err_log[i].count;
	}
	len = snprintf(buf, PAGE_SIZE, "%lld\n", total_cnt);

out:
	return len;
}

static ssize_t sd_cid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	len = snprintf(buf, PAGE_SIZE,
			"%08x%08x%08x%08x\n",
			card->raw_cid[0], card->raw_cid[1],
			card->raw_cid[2], card->raw_cid[3]);
out:
	return len;
}

static inline void mmc_check_error_count(struct mmc_sec_err_info *err_log,
		unsigned long long *total_c_cnt, unsigned long long *total_t_cnt)
{
	int i = 0;
	//Only sbc(0,1)/cmd(2,3)/data(4,5) is checked.
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *total_c_cnt < U64_MAX)
			*total_c_cnt += err_log[i].count;
		if (err_log[i].err_type == -ETIMEDOUT && *total_t_cnt < U64_MAX)
			*total_t_cnt += err_log[i].count;
	}
}

static ssize_t sd_health_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int host_idx = mmc->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;

	if (!card) {
		//There should be no spaces in 'No Card'(Vold Team).
		len = snprintf(buf, PAGE_SIZE, "NOCARD\n");
		goto out;
	}

	mmc_check_error_count(err_log, &total_c_cnt, &total_t_cnt);

	if (sd_status_err_info[host_idx].ge_cnt > 100 ||
			sd_status_err_info[host_idx].ecc_cnt > 0 ||
			sd_status_err_info[host_idx].wp_cnt > 0 ||
			sd_status_err_info[host_idx].oor_cnt > 10 ||
			total_t_cnt > 100 || total_c_cnt > 100)
		len = snprintf(buf, PAGE_SIZE, "BAD\n");
	else
		len = snprintf(buf, PAGE_SIZE, "GOOD\n");

out:
	return len;
}

#define SEC_MMC_STATUS_ERR_INFO_GET_VALUE(member) ({	\
		err_status_info->member = err_status->member - err_status_backup->member; })

static inline void mmc_check_error_count_calc_current(int host_idx,
		unsigned long long *total_c_cnt, unsigned long long *total_t_cnt,
		struct mmc_sec_status_err_info *err_status_info)
{
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	struct mmc_sec_err_info *err_log_backup = sd_err_info_backup[host_idx];
	struct mmc_sec_status_err_info *err_status = &sd_status_err_info[host_idx];
	struct mmc_sec_status_err_info *err_status_backup = &sd_status_err_info_backup[host_idx];
	int i = 0;

	//Only sbc(0,1)/cmd(2,3)/data(4,5) is checked.
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *total_c_cnt < U64_MAX)
			*total_c_cnt += (err_log[i].count - err_log_backup[i].count);
		if (err_log[i].err_type == -ETIMEDOUT && *total_t_cnt < U64_MAX)
			*total_t_cnt += (err_log[i].count - err_log_backup[i].count);
	}

	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(ge_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(cc_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(ecc_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(wp_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(oor_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(halt_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(cq_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(rpmb_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(noti_cnt);
}

#define SEC_MMC_STATUS_ERR_INFO_BACKUP(member) ({		\
		err_status_backup->member = err_status->member;	})

static inline void mmc_backup_err_info(int host_idx)
{
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	struct mmc_sec_err_info *err_log_backup = sd_err_info_backup[host_idx];
	struct mmc_sec_status_err_info *err_status = &sd_status_err_info[host_idx];
	struct mmc_sec_status_err_info *err_status_backup = &sd_status_err_info_backup[host_idx];
	int i = 0;

	// save current error count
	for (i = 0; i < MAX_ERR_LOG_INDEX; i++)
		err_log_backup[i].count = err_log[i].count;

	SEC_MMC_STATUS_ERR_INFO_BACKUP(ge_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(cc_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(ecc_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(wp_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(oor_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(halt_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(cq_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(rpmb_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(noti_cnt);
}

static ssize_t sd_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int host_idx = mmc->index;
	struct mmc_sec_status_err_info status_err_info;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;

	memset(&status_err_info, 0, sizeof(struct mmc_sec_status_err_info));

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\"," \
			"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\"\n");
		goto out;
	}

	mmc_check_error_count_calc_current(host_idx, &total_c_cnt, &total_t_cnt, &status_err_info);

	len = snprintf(buf, PAGE_SIZE,
		"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\"," \
		"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\"\n",
		status_err_info.ge_cnt,
		status_err_info.cc_cnt,
		status_err_info.ecc_cnt,
		status_err_info.wp_cnt,
		status_err_info.oor_cnt,
		total_c_cnt, total_t_cnt);
out:
	return len;
}

static ssize_t sd_data_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int host_idx = mmc->index;

	if (!card)
		return -ENODEV;

	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	mmc_backup_err_info(host_idx);

	return count;
}

static ssize_t mmc_error_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int host_idx = mmc->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int total_len = 0;
	int i = 0;

	if (!card) {
		total_len = snprintf(buf, PAGE_SIZE, "It's no card error..\n");
		goto out;
	}

	total_len += snprintf(buf, PAGE_SIZE,
				"type : err    status: first_issue_time:  last_issue_time:      count\n");

	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		total_len += snprintf(buf + total_len, PAGE_SIZE - total_len,
				"%5s:%4d 0x%08x %16llu, %16llu, %10d\n",
				err_log[i].type, err_log[i].err_type,
				err_log[i].status,
				err_log[i].first_issue_time,
				err_log[i].last_issue_time,
				err_log[i].count);
	}

	mmc_check_error_count(err_log, &total_c_cnt, &total_t_cnt);

	total_len += snprintf(buf + total_len, PAGE_SIZE - total_len,
			"GE:%d,CC:%d,ECC:%d,WP:%d,OOR:%d,CRC:%lld,TMO:%lld,HALT:%d,CQEN:%d,RPMB:%d\n",
			sd_status_err_info[host_idx].ge_cnt,
			sd_status_err_info[host_idx].cc_cnt,
			sd_status_err_info[host_idx].ecc_cnt,
			sd_status_err_info[host_idx].wp_cnt,
			sd_status_err_info[host_idx].oor_cnt,
			total_c_cnt, total_t_cnt,
			sd_status_err_info[host_idx].halt_cnt,
			sd_status_err_info[host_idx].cq_cnt,
			sd_status_err_info[host_idx].rpmb_cnt);

out:
	return total_len;
}

static DEVICE_ATTR(status, 0444, sd_detection_cmd_show, NULL);
static DEVICE_ATTR(cd_cnt, 0444, sd_detection_cnt_show, NULL);
static DEVICE_ATTR(max_mode, 0444, sd_detection_maxmode_show, NULL);
static DEVICE_ATTR(current_mode, 0444, sd_detection_curmode_show, NULL);
static DEVICE_ATTR(sdcard_summary, 0444, sdcard_summary_show, NULL);
static DEVICE_ATTR(sd_count, 0444, sd_count_show, NULL);
static DEVICE_ATTR(data, 0444, sd_cid_show, NULL);
static DEVICE_ATTR(fc, 0444, sd_health_show, NULL);
static DEVICE_ATTR(sd_data, 0664, sd_data_show, sd_data_store);
static DEVICE_ATTR(err_count, 0444, mmc_error_count_show, NULL);

static void mmc_sec_device_create_file(struct device *dev,
		const struct device_attribute *dev_attr)
{
	if (device_create_file(dev, dev_attr) < 0)
		pr_err("%s : Failed to create device file(%s)!\n",
				__func__, dev_attr->attr.name);
}

void mmc_sec_init_sysfs(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	/* Initialize cd_cnt to 0. */
	sd_op_info.card_detect_cnt = 0;

	if ((priv->sec_sd_slot_type) >= SEC_NO_DET_SD_SLOT) {
		if (!sd_detection_cmd_dev) {
			sd_detection_cmd_dev = sec_device_create(host, "sdcard");
			if (IS_ERR(sd_detection_cmd_dev)) {
				pr_err("Fail to create sysfs dev\n");
				goto failed_sdcard;
			}

			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_status);
			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_max_mode);
			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_current_mode);
			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_cd_cnt);
			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_sdcard_summary);
			mmc_sec_device_create_file(sd_detection_cmd_dev, &dev_attr_err_count);
		}
failed_sdcard:
		if (!sd_info_cmd_dev) {
			sd_info_cmd_dev = sec_device_create(host, "sdinfo");
			if (IS_ERR(sd_info_cmd_dev)) {
				pr_err("Fail to create sysfs dev\n");
				goto failed_sdinfo;
			}

			mmc_sec_device_create_file(sd_info_cmd_dev, &dev_attr_sd_count);
			mmc_sec_device_create_file(sd_info_cmd_dev, &dev_attr_data);
			mmc_sec_device_create_file(sd_info_cmd_dev, &dev_attr_fc);
		}
failed_sdinfo:
		if (!sd_data_cmd_dev) {
			sd_data_cmd_dev = sec_device_create(host, "sddata");
			if (IS_ERR(sd_data_cmd_dev)) {
				pr_err("Fail to create sysfs dev\n");
				return;
			}

			mmc_sec_device_create_file(sd_data_cmd_dev, &dev_attr_sd_data);
		}
	}
}


