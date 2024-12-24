// SPDX-License-Identifier: GPL-2.0
/*
 * Goodix Touchscreen Driver
 * Copyright (C) 2020 - 2021 Goodix, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include "goodix_ts_core.h"

#define BUS_TYPE_SPI					1
#define BUS_TYPE_I2C					0

#define GOODIX_BUS_RETRY_TIMES			3

#define FW_HEADER_SIZE					512
#define FW_SUBSYS_INFO_SIZE				10
#define FW_SUBSYS_INFO_OFFSET			42
#define FW_SUBSYS_MAX_NUM				47

#define ISP_MAX_BUFFERSIZE				4096

#define FW_PID_LEN						8
#define FW_VID_LEN						4
#define FLASH_CMD_LEN					11

#define FW_FILE_CHECKSUM_OFFSET			8
#define CONFIG_DATA_TYPE				4
#define HW_REG_CPU_RUN_FROM				0x10000
#define HOLD_CPU_REG_W					0x0002
#define HOLD_CPU_REG_R					0x2000
#define ESD_KEY_REG						0xCC58

#define FLASH_CMD_TYPE_READ				0xAA
#define FLASH_CMD_TYPE_WRITE			0xBB
#define FLASH_CMD_ACK_CHK_PASS			0xEE
#define FLASH_CMD_ACK_CHK_ERROR			0x33
#define FLASH_CMD_ACK_IDLE				0x11
#define FLASH_CMD_W_STATUS_CHK_PASS		0x22
#define FLASH_CMD_W_STATUS_CHK_FAIL		0x33
#define FLASH_CMD_W_STATUS_ADDR_ERR		0x44
#define FLASH_CMD_W_STATUS_WRITE_ERR	0x55
#define FLASH_CMD_W_STATUS_WRITE_OK		0xEE


struct update_info_t {
	unsigned int isp_ram_reg;
	unsigned int flash_cmd_reg;
	unsigned int isp_buffer_reg;
	unsigned int config_data_reg;
	unsigned int misctl_reg;
	unsigned int watch_dog_reg;
	unsigned int config_id_reg;
	unsigned int enable_misctl_val;
};

/**
 * fw_subsys_info - subsytem firmware information
 * @type: sybsystem type
 * @size: firmware size
 * @flash_addr: flash address
 * @data: firmware data
 */
struct fw_subsys_info {
	u8 type;
	u32 size;
	u32 flash_addr;
	const u8 *data;
};

/**
 *  firmware_summary
 * @size: fw total length
 * @checksum: checksum of fw
 * @hw_pid: mask pid string
 * @hw_pid: mask vid code
 * @fw_pid: fw pid string
 * @fw_vid: fw vid code
 * @subsys_num: number of fw subsystem
 * @chip_type: chip type
 * @protocol_ver: firmware packing
 *   protocol version
 * @bus_type: 0 represent I2C, 1 for SPI
 * @subsys: sybsystem info
 */
#pragma pack(1)
struct  firmware_summary {
	u32 size;
	u32 checksum;
	u8 hw_pid[6];
	u8 hw_vid[3];
	u8 fw_pid[FW_PID_LEN];
	u8 fw_vid[FW_VID_LEN];
	u8 subsys_num;
	u8 chip_type;
	u8 protocol_ver;
	u8 bus_type;
	u8 flash_protect;
	// u8 reserved[8];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};
#pragma pack()

/**
 * firmware_data - firmware data structure
 * @fw_summary: firmware information
 * @firmware: firmware data structure
 */
struct firmware_data {
	struct firmware_summary fw_summary;
	const struct firmware *firmware;
	struct firmware fw;
};

struct config_data {
	u8 *data;
	int size;
};

#pragma pack(1)
struct goodix_flash_cmd {
	union {
		struct {
			u8 status;
			u8 ack;
			u8 len;
			u8 cmd;
			u8 fw_type;
			u16 fw_len;
			u32 fw_addr;
			//u16 checksum;
		};
		u8 buf[16];
	};
};
#pragma pack()

enum update_status {
	UPSTA_NOTWORK = 0,
	UPSTA_PREPARING,
	UPSTA_UPDATING,
	UPSTA_SUCCESS,
	UPSTA_FAILED
};

enum compare_status {
	COMPARE_EQUAL = 0,
	COMPARE_NOCODE,
	COMPARE_PIDMISMATCH,
	COMPARE_FW_NOTEQUAL,
	COMPARE_CFG_NOTEQUAL,
};

/**
 * fw_update_ctrl - structure used to control the
 *  firmware update process
 * @initialized: struct init state
 * @mode: indicate weather reflash config or not, fw data source,
 *        and run on block mode or not.
 * @status: update status
 * @progress: indicate the progress of update
 * @fw_data: firmware data
 * @attr_fwimage: sysfs bin attrs, for storing fw image
 * @fw_data_src: firmware data source form sysfs, request or head file
 * @kobj: pointer to the sysfs kobject
 */
struct fw_update_ctrl {
	unsigned int cfg_checksum;
	unsigned int fw_checksum;
	enum update_status status;
	int spend_time;

	struct firmware_data fw_data;
	struct goodix_ic_config *ic_config;
	struct goodix_ts_data *ts;
	struct update_info_t update_info;

	struct bin_attribute attr_fwimage;
	struct kobject *kobj;
};
static struct fw_update_ctrl goodix_fw_update_ctrl;

static int goodix_fw_update_reset(int delay)
{
	struct goodix_ts_hw_ops *hw_ops;

	hw_ops = goodix_fw_update_ctrl.ts->hw_ops;
	return hw_ops->reset(goodix_fw_update_ctrl.ts, delay);
}

#define SOFT_RESET_REG	0xD808
static int goodix_soft_reset(int delay)
{
	struct goodix_ts_hw_ops *hw_ops;
	u8 val = 0;
	int ret;

	hw_ops = goodix_fw_update_ctrl.ts->hw_ops;
	ret = hw_ops->write(goodix_fw_update_ctrl.ts, SOFT_RESET_REG, &val, 1);
	if (ret < 0)
		return ret;

	sec_delay(delay);
	return 0;
}

static int get_fw_version_info(struct goodix_fw_version *fw_version)
{
	struct goodix_ts_hw_ops *hw_ops =
		goodix_fw_update_ctrl.ts->hw_ops;

	return hw_ops->read_version(goodix_fw_update_ctrl.ts,
			fw_version);
}

static int goodix_reg_write(unsigned int addr,
		unsigned char *data, size_t len)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_fw_update_ctrl.ts->hw_ops;

	return hw_ops->write(goodix_fw_update_ctrl.ts, addr, data, len);
}

static int goodix_reg_read(unsigned int addr,
		unsigned char *data, size_t len)
{
	struct goodix_ts_hw_ops *hw_ops = goodix_fw_update_ctrl.ts->hw_ops;

	return hw_ops->read(goodix_fw_update_ctrl.ts, addr, data, len);
}

/**
 * goodix_parse_firmware - parse firmware header information
 *	and subsystem information from firmware data buffer
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data.
 * return: 0 - OK, < 0 - error
 */
/* sizeof(length) + sizeof(checksum) */

static int goodix_parse_firmware(struct firmware_data *fw_data)
{
	const struct firmware *firmware;
	struct  firmware_summary *fw_summary;
	unsigned int i, fw_offset, info_offset;
	u32 checksum;
	int subsys_info_offset = FW_SUBSYS_INFO_OFFSET;
	int header_size = FW_HEADER_SIZE;
	int r = 0;

	fw_summary = &fw_data->fw_summary;

	/* copy firmware head info */
	firmware = &fw_data->fw;
	if (firmware->size < subsys_info_offset) {
		ts_err("Invalid firmware size:%zu", firmware->size);
		r = -EINVAL;
		goto err_size;
	}
	memcpy(fw_summary, firmware->data, sizeof(*fw_summary));

	/* check firmware size */
	fw_summary->size = le32_to_cpu(fw_summary->size);
	if (firmware->size != fw_summary->size + FW_FILE_CHECKSUM_OFFSET) {
		ts_err("Bad firmware, size not match, %zu != %d",
				firmware->size, fw_summary->size + 6);
		r = -EINVAL;
		goto err_size;
	}

	for (i = FW_FILE_CHECKSUM_OFFSET, checksum = 0;
			i < firmware->size; i += 2)
		checksum += firmware->data[i] + (firmware->data[i+1] << 8);

	/* byte order change, and check */
	fw_summary->checksum = le32_to_cpu(fw_summary->checksum);
	goodix_fw_update_ctrl.fw_checksum = fw_summary->checksum;
	ts_info("Firmware checksum:0x%04x", goodix_fw_update_ctrl.fw_checksum +
			goodix_fw_update_ctrl.cfg_checksum);

	if (checksum != fw_summary->checksum) {
		ts_err("Bad firmware, cheksum error");
		r = -EINVAL;
		goto err_size;
	}

	if (fw_summary->subsys_num > FW_SUBSYS_MAX_NUM) {
		ts_err("Bad firmware, invalid subsys num: %d",
				fw_summary->subsys_num);
		r = -EINVAL;
		goto err_size;
	}

	/* parse subsystem info */
	fw_offset = header_size;
	for (i = 0; i < fw_summary->subsys_num; i++) {
		info_offset = subsys_info_offset +
			i * FW_SUBSYS_INFO_SIZE;

		fw_summary->subsys[i].type = firmware->data[info_offset];
		fw_summary->subsys[i].size =
			le32_to_cpup((__le32 *)&firmware->data[info_offset + 1]);

		fw_summary->subsys[i].flash_addr =
			le32_to_cpup((__le32 *)&firmware->data[info_offset + 5]);
		if (fw_offset > firmware->size) {
			ts_err("Sybsys offset exceed Firmware size");
			goto err_size;
		}

		fw_summary->subsys[i].data = firmware->data + fw_offset;
		fw_offset += fw_summary->subsys[i].size;
	}

	ts_info("Firmware package protocol: V%u", fw_summary->protocol_ver);
	ts_info("Firmware PID:GT%s", fw_summary->fw_pid);
	ts_info("Firmware VID:%*ph", 4, fw_summary->fw_vid);
	ts_info("Firmware chip type:0x%02X", fw_summary->chip_type);
	ts_info("Firmware bus type:%s",
			(fw_summary->bus_type & BUS_TYPE_SPI) ? "SPI" : "I2C");
	ts_info("Firmware size:%u", fw_summary->size);
	ts_info("Firmware subsystem num:%u", fw_summary->subsys_num);

	for (i = 0; i < fw_summary->subsys_num; i++) {
		ts_debug("------------------------------------------");
		ts_debug("Index:%d", i);
		ts_debug("Subsystem type:%02X", fw_summary->subsys[i].type);
		ts_debug("Subsystem size:%u", fw_summary->subsys[i].size);
		ts_debug("Subsystem flash_addr:%08X",
				fw_summary->subsys[i].flash_addr);
		ts_debug("Subsystem Ptr:%pK", fw_summary->subsys[i].data);
	}

err_size:
	return r;
}

/**
 * goodix_fw_version_compare - compare the active version with
 * firmware file version.
 * @fwu_ctrl: firmware information to be compared
 * return: 0 equal, < 0 unequal
 */
static int goodix_fw_version_compare(struct fw_update_ctrl *fwu_ctrl)
{
	int ret = 0;
	struct goodix_fw_version fw_version;
	struct firmware_summary *fw_summary = &fwu_ctrl->fw_data.fw_summary;
	u32 config_id_reg = goodix_fw_update_ctrl.update_info.config_id_reg;
	u32 file_cfg_id;
	u32 ic_cfg_id;

	/* compare fw_version */
	ret = get_fw_version_info(&fw_version);
	if (ret)
		return -EINVAL;

	if (!memcmp(fw_version.rom_pid, GOODIX_NOCODE, 6) ||
			!memcmp(fw_version.patch_pid, GOODIX_NOCODE, 6)) {
		ts_info("there is no code in the chip");
		return COMPARE_NOCODE;
	}

	if (memcmp(fw_version.patch_pid, fw_summary->fw_pid, FW_PID_LEN)) {
		ts_err("Product ID mismatch:%s != %s",
				fw_version.patch_pid, fw_summary->fw_pid);
		return COMPARE_PIDMISMATCH;
	}

	ret = memcmp(fw_version.patch_vid, fw_summary->fw_vid, FW_VID_LEN);
	if (ret) {
		ts_info("active firmware version:%*ph", FW_VID_LEN,
				fw_version.patch_vid);
		ts_info("firmware file version: %*ph", FW_VID_LEN,
				fw_summary->fw_vid);
		return COMPARE_FW_NOTEQUAL;
	}
	ts_info("fw_version equal");

	/* compare config id */
	if (fwu_ctrl->ic_config && fwu_ctrl->ic_config->len > 0) {
		file_cfg_id =
			goodix_get_file_config_id(fwu_ctrl->ic_config->data);
		goodix_reg_read(config_id_reg,
				(u8 *)&ic_cfg_id, sizeof(ic_cfg_id));
		if (ic_cfg_id != file_cfg_id) {
			ts_info("ic_cfg_id:0x%x != file_cfg_id:0x%x",
					ic_cfg_id, file_cfg_id);
			return COMPARE_CFG_NOTEQUAL;
		}
		ts_info("config_id equal");
	}

	return COMPARE_EQUAL;
}

/**
 * goodix_reg_write_confirm - write register and confirm the value
 *  in the register.
 * @dev: pointer to touch device
 * @addr: register address
 * @data: pointer to data buffer
 * @len: data length
 * return: 0 write success and confirm ok
 *		   < 0 failed
 */
static int goodix_reg_write_confirm(unsigned int addr,
		unsigned char *data, unsigned int len)
{
	u8 *cfm = NULL;
	u8 cfm_buf[32];
	int r, i;

	if (len > sizeof(cfm_buf)) {
		cfm = kzalloc(len, GFP_KERNEL);
		if (!cfm)
			return -ENOMEM;
	} else {
		cfm = &cfm_buf[0];
	}

	for (i = 0; i < GOODIX_BUS_RETRY_TIMES; i++) {
		r = goodix_reg_write(addr, data, len);
		if (r < 0)
			goto exit;

		r = goodix_reg_read(addr, cfm, len);
		if (r < 0)
			goto exit;

		if (memcmp(data, cfm, len)) {
			r = -EINVAL;
			continue;
		} else {
			r = 0;
			break;
		}
	}

exit:
	if (cfm != &cfm_buf[0])
		kfree(cfm);
	return r;
}


/**
 * goodix_load_isp - load ISP program to device ram
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return 0 ok, <0 error
 */
static int goodix_load_isp(struct firmware_data *fw_data)
{
	struct goodix_fw_version isp_fw_version;
	struct fw_subsys_info *fw_isp;
	u32 isp_ram_reg = goodix_fw_update_ctrl.update_info.isp_ram_reg;
	u8 reg_val[8] = {0x00};
	int r;

	memset(&isp_fw_version, 0, sizeof(isp_fw_version));
	fw_isp = &fw_data->fw_summary.subsys[0];

	ts_info("Loading ISP start");
	r = goodix_reg_write_confirm(isp_ram_reg,
			(u8 *)fw_isp->data, fw_isp->size);
	if (r < 0) {
		ts_err("Loading ISP error");
		return r;
	}

	ts_info("Success send ISP data");

	/* SET BOOT OPTION TO 0X55 */
	memset(reg_val, 0x55, 8);
	r = goodix_reg_write_confirm(HW_REG_CPU_RUN_FROM, reg_val, 8);
	if (r < 0) {
		ts_err("Failed set REG_CPU_RUN_FROM flag");
		return r;
	}
	ts_info("Success write [8]0x55 to 0x%x", HW_REG_CPU_RUN_FROM);

	if (goodix_soft_reset(100))
		ts_err("soft reset abnormal");
	/*check isp state */
	if (get_fw_version_info(&isp_fw_version)) {
		ts_err("failed read isp version");
		return -2;
	}

	ts_info("patch_pid [3]:0x%02X [4]:0x%02X [5]:0x%02X",
			isp_fw_version.patch_pid[3], isp_fw_version.patch_pid[4],
			isp_fw_version.patch_pid[5]);

	if (memcmp(&isp_fw_version.patch_pid[3], "ISP", 3)) {
		ts_err("patch id error %c%c%c != %s",
				isp_fw_version.patch_pid[3], isp_fw_version.patch_pid[4],
				isp_fw_version.patch_pid[5], "ISP");
		return -3;
	}
	ts_info("ISP running successfully");
	return 0;
}

/**
 * goodix_update_prepare - update prepare, loading ISP program
 *  and make sure the ISP is running.
 * @fwu_ctrl: pointer to fimrware control structure
 * return: 0 ok, <0 error
 */
static int goodix_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	u32 misctl_reg = fwu_ctrl->update_info.misctl_reg;
	u32 watch_dog_reg = fwu_ctrl->update_info.watch_dog_reg;
	u32 enable_misctl_val = fwu_ctrl->update_info.enable_misctl_val;
	u8 reg_val[4] = {0};
	u8 temp_buf[64] = {0};
	int retry = 20;
	int r;

	/*reset IC*/
	ts_info("firmware update, reset");
	if (goodix_fw_update_reset(5))
		ts_err("reset abnormal");

	retry = 100;
	/* Hold cpu*/
	do {
		reg_val[0] = 0x01;
		reg_val[1] = 0x00;
		r = goodix_reg_write(HOLD_CPU_REG_W, reg_val, 2);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[0], 4);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[4], 4);
		r |= goodix_reg_read(HOLD_CPU_REG_R, &temp_buf[8], 4);
		if (!r && !memcmp(&temp_buf[0], &temp_buf[4], 4) &&
				!memcmp(&temp_buf[4], &temp_buf[8], 4) &&
				!memcmp(&temp_buf[0], &temp_buf[8], 4)) {
			break;
		}
		sec_delay(1);
		ts_info("retry hold cpu %d", retry);
		ts_debug("data:%*ph", 12, temp_buf);
	} while (--retry);
	if (!retry) {
		ts_err("Failed to hold CPU, return =%d", r);
		return -1;
	}
	ts_info("Success hold CPU");

	/* enable misctl clock */
	if (fwu_ctrl->ts->bus->ic_type == IC_TYPE_BERLIN_D || fwu_ctrl->ts->bus->ic_type == IC_TYPE_GT9916K)
		goodix_reg_write(misctl_reg, (u8 *)&enable_misctl_val, 4);
	else
		goodix_reg_write(misctl_reg, (u8 *)&enable_misctl_val, 1);
	ts_info("enable misctl clock");

	/* disable watch dog */
	reg_val[0] = 0x00;
	r = goodix_reg_write(watch_dog_reg, reg_val, 1);
	ts_info("disable watch dog");

	/* load ISP code and run form isp */
	r = goodix_load_isp(&fwu_ctrl->fw_data);
	if (r < 0)
		ts_err("Failed load and run isp");

	return r;
}

/*	goodix_send_flash_cmd: send command to read or write flash data
 *	@flash_cmd: command need to send.
 */
static int goodix_send_flash_cmd(struct goodix_flash_cmd *flash_cmd)
{
	int i, ret, retry;
	struct goodix_flash_cmd tmp_cmd;
	u32 flash_cmd_reg = goodix_fw_update_ctrl.update_info.flash_cmd_reg;

	ts_debug("try send flash cmd:%*ph", (int)sizeof(flash_cmd->buf),
			flash_cmd->buf);
	memset(tmp_cmd.buf, 0, sizeof(tmp_cmd));
	ret = goodix_reg_write(flash_cmd_reg,
			flash_cmd->buf, sizeof(flash_cmd->buf));
	if (ret) {
		ts_err("failed send flash cmd %d", ret);
		return ret;
	}

	retry = 5;
	for (i = 0; i < retry; i++) {
		ret = goodix_reg_read(flash_cmd_reg,
				tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS)
			break;
		sec_delay(5);
		ts_info("flash cmd ack error retry %d, ack 0x%x, ret %d",
				i, tmp_cmd.ack, ret);
	}
	if (tmp_cmd.ack != FLASH_CMD_ACK_CHK_PASS) {
		ts_err("flash cmd ack error, ack 0x%x, ret %d",
				tmp_cmd.ack, ret);
		ts_err("data:%*ph", (int)sizeof(tmp_cmd.buf), tmp_cmd.buf);
		return -EINVAL;
	}
	ts_debug("flash cmd ack check pass");

	sec_delay(80);
	retry = 20;
	for (i = 0; i < retry; i++) {
		ret = goodix_reg_read(flash_cmd_reg,
				tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS &&
				tmp_cmd.status == FLASH_CMD_W_STATUS_WRITE_OK) {
			ts_debug("flash status check pass");
			return 0;
		}

		ts_info("flash cmd status not ready, retry %d, ack 0x%x, status 0x%x, ret %d",
				i, tmp_cmd.ack, tmp_cmd.status, ret);
		sec_delay(20);
	}

	ts_err("flash cmd status error %d, ack 0x%x, status 0x%x, ret %d",
			i, tmp_cmd.ack, tmp_cmd.status, ret);
	if (ret) {
		ts_info("reason: bus or paltform error");
		return -EINVAL;
	}

	switch (tmp_cmd.status) {
	case FLASH_CMD_W_STATUS_CHK_PASS:
		ts_err("data check pass, but failed get follow-up results");
		return -EFAULT;
	case FLASH_CMD_W_STATUS_CHK_FAIL:
		ts_err("data check failed, please retry");
		return -EAGAIN;
	case FLASH_CMD_W_STATUS_ADDR_ERR:
		ts_err("flash target addr error, please check");
		return -EFAULT;
	case FLASH_CMD_W_STATUS_WRITE_ERR:
		ts_err("flash data write err, please retry");
		return -EAGAIN;
	default:
		ts_err("unknown status");
		return -EFAULT;
	}
}

static int goodix_flash_package(u8 subsys_type, u8 *pkg,
		u32 flash_addr, u16 pkg_len)
{
	int ret, retry;
	struct goodix_flash_cmd flash_cmd;
	u32 isp_buffer_reg = goodix_fw_update_ctrl.update_info.isp_buffer_reg;

	retry = 2;
	do {
		ret = goodix_reg_write(isp_buffer_reg, pkg, pkg_len);
		if (ret < 0) {
			ts_err("Failed to write firmware packet");
			return ret;
		}

		flash_cmd.status = 0;
		flash_cmd.ack = 0;
		flash_cmd.len = FLASH_CMD_LEN;
		flash_cmd.cmd = FLASH_CMD_TYPE_WRITE;
		flash_cmd.fw_type = subsys_type;
		flash_cmd.fw_len = cpu_to_le16(pkg_len);
		flash_cmd.fw_addr = cpu_to_le32(flash_addr);

		goodix_append_checksum(&(flash_cmd.buf[2]),
				9, CHECKSUM_MODE_U8_LE);

		ret = goodix_send_flash_cmd(&flash_cmd);
		if (!ret) {
			ts_debug("success write package to 0x%x, len %d",
					flash_addr, pkg_len - 4);
			return 0;
		}
	} while (ret == -EAGAIN && --retry);

	return ret;
}

/**
 * goodix_flash_subsystem - flash subsystem firmware,
 *  Main flow of flashing firmware.
 *	Each firmware subsystem is divided into several
 *	packets, the max size of packet is limited to
 *	@{ISP_MAX_BUFFERSIZE}
 * @dev: pointer to touch device
 * @subsys: subsystem information
 * return: 0 ok, < 0 error
 */
static int goodix_flash_subsystem(struct fw_subsys_info *subsys)
{
	u32 data_size, offset;
	u32 total_size;
	//TODO: confirm flash addr ,<< 8??
	u32 subsys_base_addr = subsys->flash_addr;
	u8 *fw_packet = NULL;
	int r = 0;

	/*
	 * if bus(i2c/spi) error occued, then exit, we will do
	 * hardware reset and re-prepare ISP and then retry
	 * flashing
	 */
	total_size = subsys->size;
	fw_packet = kzalloc(ISP_MAX_BUFFERSIZE + 4, GFP_KERNEL);
	if (!fw_packet) {
		ts_err("Failed alloc memory");
		return -EINVAL;
	}

	offset = 0;
	while (total_size > 0) {
		data_size = total_size > ISP_MAX_BUFFERSIZE ?
			ISP_MAX_BUFFERSIZE : total_size;
		ts_debug("Flash firmware to %08x,size:%u bytes",
				subsys_base_addr + offset, data_size);

		memcpy(fw_packet, &subsys->data[offset], data_size);
		/* set checksum for package data */
		goodix_append_checksum(fw_packet,
				data_size, CHECKSUM_MODE_U16_LE);

		r = goodix_flash_package(subsys->type, fw_packet,
				subsys_base_addr + offset, data_size + 4);
		if (r) {
			ts_err("failed flash to %08x,size:%u bytes",
					subsys_base_addr + offset, data_size);
			break;
		}
		offset += data_size;
		total_size -= data_size;
	} /* end while */

	kfree(fw_packet);
	return r;
}

/**
 * goodix_flash_firmware - flash firmware
 * @dev: pointer to touch device
 * @fw_data: firmware data
 * return: 0 ok, < 0 error
 */
static int goodix_flash_firmware(struct fw_update_ctrl *fw_ctrl)
{
	struct firmware_data *fw_data = &fw_ctrl->fw_data;
	struct firmware_summary *fw_summary;
	struct fw_subsys_info *fw_x;
	struct fw_subsys_info subsys_cfg = {0};
	u32 config_data_reg = fw_ctrl->update_info.config_data_reg;
	int retry = GOODIX_BUS_RETRY_TIMES;
	int i, r = 0, fw_num;

	/*	start from subsystem 1,
	 *	subsystem 0 is the ISP program
	 */

	fw_summary = &fw_data->fw_summary;
	fw_num = fw_summary->subsys_num;

	/* flash config data first if we have */
	if (fw_ctrl->ic_config && fw_ctrl->ic_config->len) {
		subsys_cfg.data = fw_ctrl->ic_config->data;
		subsys_cfg.size = fw_ctrl->ic_config->len;
		subsys_cfg.flash_addr = config_data_reg;
		subsys_cfg.type = CONFIG_DATA_TYPE;

		r = goodix_flash_subsystem(&subsys_cfg);
		if (r) {
			ts_err("failed flash config with ISP, %d", r);
			return r;
		}
		ts_info("success flash config with ISP");
	}

	for (i = 1; i < fw_num && retry;) {
		ts_info("--- Start to flash subsystem[%d] ---", i);
		fw_x = &fw_summary->subsys[i];

		r = goodix_flash_subsystem(fw_x);
		if (r == 0) {
			ts_info("--- End flash subsystem[%d]: OK ---", i);
			i++;
		} else if (r == -EAGAIN) {
			retry--;
			ts_err("--- End flash subsystem%d: Fail, errno:%d, retry:%d ---",
					i, r, GOODIX_BUS_RETRY_TIMES - retry);
		} else if (r < 0) { /* bus error */
			ts_err("--- End flash subsystem%d: Fatal error:%d exit ---",
					i, r);
			goto exit_flash;
		}
	}

exit_flash:
	return r;
}

/**
 * goodix_update_finish - update finished, FREE resource
 *  and reset flags---
 * @fwu_ctrl: pointer to fw_update_ctrl structrue
 * return: 0 ok, < 0 error
 */
static int goodix_update_finish(struct fw_update_ctrl *fwu_ctrl)
{
	struct goodix_ts_cmd temp_cmd;
	uint32_t total_checksum = fwu_ctrl->cfg_checksum + fwu_ctrl->fw_checksum;
	int ret;

	if (goodix_fw_update_reset(100))
		ts_err("reset abnormal");

	/* write total checksum to flash */
	temp_cmd.len = 8;
	temp_cmd.cmd = 0xF3;
	temp_cmd.data[0] = total_checksum & 0xFF;
	temp_cmd.data[1] = (total_checksum >> 8) & 0xFF;
	temp_cmd.data[2] = (total_checksum >> 16) & 0xFF;
	temp_cmd.data[3] = (total_checksum >> 24) & 0xFF;
	ts_info("%s : write checksum = 0x%X", __func__, total_checksum);
	ret = fwu_ctrl->ts->hw_ops->send_cmd(fwu_ctrl->ts, &temp_cmd);
	if (ret < 0)
		ts_err("failed write total checksum into flash");

	ret = goodix_fw_version_compare(fwu_ctrl);
	if (ret == COMPARE_EQUAL || ret == COMPARE_CFG_NOTEQUAL)
		return 0;

	return -EINVAL;
}

/**
 * goodix_fw_update_proc - firmware update process, the entry of
 *  firmware update flow
 * @fwu_ctrl: firmware control
 * return: = 0 update ok, < 0 error or NO_NEED_UPDATE
 */
int goodix_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
#define FW_UPDATE_RETRY		2
	int retry0;
	int retry1 = FW_UPDATE_RETRY;
	int ret = 0;

	ret = goodix_parse_firmware(&fwu_ctrl->fw_data);
	if (ret < 0)
		return ret;

start_update:
	fwu_ctrl->status = UPSTA_PREPARING;
	retry0 = FW_UPDATE_RETRY;
	do {
		ret = goodix_update_prepare(fwu_ctrl);
		if (ret)
			ts_err("failed prepare ISP, retry %d", FW_UPDATE_RETRY - retry0);

	} while (ret && --retry0 > 0);
	if (ret) {
		ts_err("Failed to prepare ISP, exit update:%d", ret);
		return ret;
	}

	/* progress: 20%~100% */
	fwu_ctrl->status = UPSTA_UPDATING;
	ret = goodix_flash_firmware(fwu_ctrl);
	if (ret)
		ts_err("flash fw data enter error, ret:%d", ret);
	else
		ts_info("flash fw data success, need check version");

	ret = goodix_update_finish(fwu_ctrl);
	if (ret && --retry1 > 0) {
		ts_err("update failed retry:%d", FW_UPDATE_RETRY - retry1);
		goto start_update;
	}

	if (!ret)
		ts_info("Firmware update successfully");
	else
		ts_err("Firmware update failed, ret:%d", ret);

	return ret;
}

/**
 * goodix_request_firmware - request firmware data from user space
 *
 * @fw_data: firmware struct, contains firmware header info
 *	and firmware data pointer.
 * return: 0 - OK, < 0 - error
 */
static int goodix_request_firmware(struct firmware_data *fw_data, struct fw_update_ctrl *fwu_ctrl)
{
	int cfgPackageLen;
	int fwPackageLen;

	if (!fw_data->firmware) {
		ts_err("firmware is null");
		return -EINVAL;
	}

	cfgPackageLen = be32_to_cpup((__be32 *)fw_data->firmware->data) + 6;
	if (fw_data->firmware->size <= (cfgPackageLen + 16)) {
		ts_err("current firmware does not contain goodix fw");
		return -EINVAL;
	}

	fwu_ctrl->cfg_checksum = be16_to_cpup((__be16 *)&fw_data->firmware->data[4]);

	if (!(fw_data->firmware->data[cfgPackageLen + 0] == 'G' &&
			fw_data->firmware->data[cfgPackageLen + 1] == 'X' &&
			fw_data->firmware->data[cfgPackageLen + 2] == 'F' &&
			fw_data->firmware->data[cfgPackageLen + 3] == 'W')) {
		ts_err("can't find fw package");
		ts_err("Data type:%c%c%c%c != GXFW",
			fw_data->firmware->data[cfgPackageLen + 0],
			fw_data->firmware->data[cfgPackageLen + 1],
			fw_data->firmware->data[cfgPackageLen + 2],
			fw_data->firmware->data[cfgPackageLen + 3]);
		return -EINVAL;
	}

	fwPackageLen = be32_to_cpup((__be32 *)&fw_data->firmware->data[cfgPackageLen + 8]);
	ts_info("firmware package len:%d", fwPackageLen);

	if ((fwPackageLen + 16 + cfgPackageLen) > fw_data->firmware->size) {
		ts_err("bad firmware, need len[%d] != actual len[%d]",
			fwPackageLen + 16 + cfgPackageLen, (int)fw_data->firmware->size);
		return -EINVAL;
	}

	fw_data->fw.data = fw_data->firmware->data + cfgPackageLen + 16;
	fw_data->fw.size = fwPackageLen;

	ts_info("Firmware image is ready");
	return 0;
}

/**
 * relase firmware resources
 *
 */
static inline void goodix_release_firmware(struct firmware_data *fw_data)
{
	if (fw_data->firmware) {
		fw_data->firmware = NULL;
		memset(&fw_data->fw, 0, sizeof(fw_data->fw));
	}
}

int goodix_do_fw_update(struct goodix_ic_config *ic_config)
{
	struct fw_update_ctrl *fwu_ctrl = &goodix_fw_update_ctrl;
	ktime_t start, end;
	int ret = -EINVAL;

	fwu_ctrl->ic_config = ic_config;
	start = ktime_get();
	fwu_ctrl->spend_time = 0;
	fwu_ctrl->status = UPSTA_NOTWORK;

	ret = goodix_request_firmware(&fwu_ctrl->fw_data, fwu_ctrl);
	if (ret < 0)
		goto out;

	ts_debug("notify update start");
	goodix_ts_blocking_notify(NOTIFY_FWUPDATE_START, NULL);

	/* ready to update */
	ts_debug("start update proc");
	ret = goodix_fw_update_proc(fwu_ctrl);

	/* clean */
	goodix_release_firmware(&fwu_ctrl->fw_data);
out:
	if (ret) {
		ts_err("fw update failed, %d", ret);
		fwu_ctrl->status = UPSTA_FAILED;
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_FAILED, NULL);
	} else {
		ts_info("fw update success");
		fwu_ctrl->status = UPSTA_SUCCESS;
		goodix_ts_blocking_notify(NOTIFY_FWUPDATE_SUCCESS, NULL);
	}

	end = ktime_get();
	fwu_ctrl->spend_time = ktime_to_ms(ktime_sub(end, start));
	ts_info("spend_time: %dms", fwu_ctrl->spend_time);

	return ret;
}

int goodix_fw_update_init(struct goodix_ts_data *ts,
				const struct firmware *firmware)
{
	if (!ts || !ts->hw_ops) {
		ts_err("ts && hw_ops cann't be null");
		return -ENODEV;
	}

	if (!firmware) {
		ts_err("firmware is NULL");
		return -EINVAL;
	}

	goodix_fw_update_ctrl.ts = ts;
	goodix_fw_update_ctrl.fw_data.firmware = firmware;

	goodix_fw_update_ctrl.update_info.isp_ram_reg = ts->isp_ram_reg;
	goodix_fw_update_ctrl.update_info.flash_cmd_reg = ts->flash_cmd_reg;
	goodix_fw_update_ctrl.update_info.isp_buffer_reg = ts->isp_buffer_reg;
	goodix_fw_update_ctrl.update_info.config_data_reg = ts->config_data_reg;
	goodix_fw_update_ctrl.update_info.misctl_reg = ts->misctl_reg;
	goodix_fw_update_ctrl.update_info.watch_dog_reg = ts->watch_dog_reg;
	goodix_fw_update_ctrl.update_info.config_id_reg = ts->config_id_reg;
	goodix_fw_update_ctrl.update_info.enable_misctl_val = ts->enable_misctl_val;

	return 0;
}
