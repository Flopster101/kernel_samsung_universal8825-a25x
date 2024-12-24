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

#define TS_BIN_VERSION_START_INDEX		5
#define TS_BIN_VERSION_LEN				4
#define TS_CFG_BIN_HEAD_RESERVED_LEN	6
#define TS_CFG_OFFSET_LEN				2
#define TS_IC_TYPE_NAME_MAX_LEN			15
#define TS_CFG_BIN_HEAD_LEN		(sizeof(struct goodix_cfg_bin_head) + \
		TS_CFG_BIN_HEAD_RESERVED_LEN)
#define TS_PKG_CONST_INFO_LEN	(sizeof(struct goodix_cfg_pkg_const_info))
#define TS_PKG_REG_INFO_LEN		(sizeof(struct goodix_cfg_pkg_reg_info))
#define TS_PKG_HEAD_LEN			(TS_PKG_CONST_INFO_LEN + TS_PKG_REG_INFO_LEN)

/*cfg block definitin*/
#define TS_CFG_BLOCK_PID_LEN		8
#define TS_CFG_BLOCK_VID_LEN		8
#define TS_CFG_BLOCK_FW_MASK_LEN	9
#define TS_CFG_BLOCK_FW_PATCH_LEN	4
#define TS_CFG_BLOCK_RESERVED_LEN	9

#define TS_NORMAL_CFG				0x01
#define TS_HIGH_SENSE_CFG			0x03
#define TS_RQST_FW_RETRY_TIMES		2

#pragma pack(1)
struct goodix_cfg_pkg_reg {
	u16 addr;
	u8 reserved1;
	u8 reserved2;
};

struct goodix_cfg_pkg_const_info {
	u32 pkg_len;
	u8 ic_type[TS_IC_TYPE_NAME_MAX_LEN];
	u8 cfg_type;
	u8 sensor_id;
	u8 hw_pid[TS_CFG_BLOCK_PID_LEN];
	u8 hw_vid[TS_CFG_BLOCK_VID_LEN];
	u8 fw_mask[TS_CFG_BLOCK_FW_MASK_LEN];
	u8 fw_patch[TS_CFG_BLOCK_FW_PATCH_LEN];
	u16 x_res_offset;
	u16 y_res_offset;
	u16 trigger_offset;
};

struct goodix_cfg_pkg_reg_info {
	struct goodix_cfg_pkg_reg cfg_send_flag;
	struct goodix_cfg_pkg_reg version_base;
	struct goodix_cfg_pkg_reg pid;
	struct goodix_cfg_pkg_reg vid;
	struct goodix_cfg_pkg_reg sensor_id;
	struct goodix_cfg_pkg_reg fw_mask;
	struct goodix_cfg_pkg_reg fw_status;
	struct goodix_cfg_pkg_reg cfg_addr;
	struct goodix_cfg_pkg_reg esd;
	struct goodix_cfg_pkg_reg command;
	struct goodix_cfg_pkg_reg coor;
	struct goodix_cfg_pkg_reg gesture;
	struct goodix_cfg_pkg_reg fw_request;
	struct goodix_cfg_pkg_reg proximity;
	u8 reserved[TS_CFG_BLOCK_RESERVED_LEN];
};

struct goodix_cfg_bin_head {
	u32 bin_len;
	u8 checksum;
	u8 bin_version[TS_BIN_VERSION_LEN];
	u8 pkg_num;
};

#pragma pack()

struct goodix_cfg_package {
	struct goodix_cfg_pkg_const_info cnst_info;
	struct goodix_cfg_pkg_reg_info reg_info;
	const u8 *cfg;
	u32 pkg_len;
};

struct goodix_cfg_bin {
	unsigned char *bin_data;
	unsigned int bin_data_len;
	struct goodix_cfg_bin_head head;
	struct goodix_cfg_package *cfg_pkgs;
};

#define BIN_CFG_START_LOCAL	6
static int goodix_read_cfg_bin(const struct firmware *firmware,
		struct goodix_cfg_bin *cfg_bin)
{
	int cfgPackageLen;
	u16 cfg_checksum;
	u16 checksum = 0;
	int i;

	if (!firmware) {
		ts_err("firmware is null");
		return -EINVAL;
	}

	if (firmware->size <= 0) {
		ts_err("request_firmware, cfg_bin length ERROR,len:%zu",
				firmware->size);
		return -EINVAL;
	}

	cfgPackageLen = be32_to_cpup((__be32 *)&firmware->data[0]) + BIN_CFG_START_LOCAL;
	ts_info("config package len:%d", cfgPackageLen);

	cfg_checksum = be16_to_cpup((__be16 *)&firmware->data[4]);
	for (i = BIN_CFG_START_LOCAL; i < cfgPackageLen; i++)
		checksum += firmware->data[i];
	if (checksum != cfg_checksum) {
		ts_err("BAD config, checksum[%d] != cfg_checksum[%d]", checksum, cfg_checksum);
		return -EINVAL;
	}

	cfg_bin->bin_data_len = cfgPackageLen;
	/* allocate memory for cfg_bin->bin_data */
	cfg_bin->bin_data = kzalloc(cfgPackageLen, GFP_KERNEL);
	if (!cfg_bin->bin_data)
		return -ENOMEM;

	memcpy(cfg_bin->bin_data, firmware->data, cfg_bin->bin_data_len);

	return 0;
}

#define CFG_NUM					23
#define CFG_HEAD_BYTES			32
#define CFG_INFO_BLOCK_BYTES	8
static int goodix_parse_cfg_bin(struct goodix_ts_data *ts,
		struct goodix_cfg_bin *cfg_bin, unsigned char sensor_id)
{
	u16 cfg_offset;
	int i;
	int cfg_num;
	int cfg_len;
	u8 cfg_sid;
	int cfg_type;
	bool exist = false;

	cfg_num = cfg_bin->bin_data[CFG_NUM];
	ts_info("bin_cfg_num = %d", cfg_num);

	cfg_offset = CFG_HEAD_BYTES + cfg_num * CFG_INFO_BLOCK_BYTES;
	/* find cfg's sid in cfg.bin */
	for (i = 0; i < cfg_num; i++) {
		cfg_len = be16_to_cpup((__be16 *)&cfg_bin->bin_data[CFG_HEAD_BYTES + 2 + i * CFG_INFO_BLOCK_BYTES]);
		cfg_sid = cfg_bin->bin_data[CFG_HEAD_BYTES + i * CFG_INFO_BLOCK_BYTES];
		if (cfg_sid == sensor_id) {
			cfg_type = cfg_bin->bin_data[CFG_HEAD_BYTES + 1 + i * CFG_INFO_BLOCK_BYTES];
			if (cfg_type >= GOODIX_MAX_CONFIG_GROUP) {
				ts_err("invalid cfg_type[%d]", cfg_type);
				cfg_offset += cfg_len;
				continue;
			}
			if (!ts->ic_configs[cfg_type])
				ts->ic_configs[cfg_type] = kzalloc(sizeof(struct goodix_ic_config), GFP_KERNEL);
			else
				memset(ts->ic_configs[cfg_type]->data, 0x00, GOODIX_CFG_MAX_SIZE);
			ts->ic_configs[cfg_type]->len = cfg_len;
			memcpy(ts->ic_configs[cfg_type]->data, &cfg_bin->bin_data[cfg_offset], cfg_len);
			exist = true;
			ts_info("find valid config, cfg_type[%d], cfg_len[%d]", cfg_type, cfg_len);
		} else {
			ts_info("cfg_sid[%d] != sensor_id[%d]", cfg_sid, sensor_id);
	}
		cfg_offset += cfg_len;
	}

	if (!exist)
		return -EINVAL;
	return 0;
}

static int goodix_get_config_data(struct goodix_ts_data *ts, u8 sensor_id,
					const struct firmware *firmware)
{
	struct goodix_cfg_bin cfg_bin = {0};
	int ret;

	/*get cfg_bin from file system*/
	ret = goodix_read_cfg_bin(firmware, &cfg_bin);
	if (ret) {
		ts_err("failed get valid config bin data");
		return ret;
	}

	/*parse cfg bin*/
	ret = goodix_parse_cfg_bin(ts, &cfg_bin, sensor_id);
	if (ret)
		ts_err("failed parse cfg bin");

	kfree(cfg_bin.bin_data);
	return ret;
}

#define BRL_SENSOR_ID_REG	0x1002B
#define DEFAULT_SENSOR_ID	0xFF
int goodix_get_config_proc(struct goodix_ts_data *ts,
				const struct firmware *firmware)
{
	u8 sensor_id;
	int ret;

	ret = ts->bus->read(ts->bus->dev, BRL_SENSOR_ID_REG, &sensor_id, 1);
	if (ret < 0) {
		ts_info("read sensor_id failed, use defalut[%d]", DEFAULT_SENSOR_ID);
		sensor_id = DEFAULT_SENSOR_ID;
	}

	return goodix_get_config_data(ts, sensor_id, firmware);
}
