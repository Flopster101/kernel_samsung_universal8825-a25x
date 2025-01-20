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

int goodix_set_cmd(struct goodix_ts_core *core_data, u8 reg, u8 mode)
{
	struct goodix_ts_cmd temp_cmd;
	int ret = 0;

	temp_cmd.len = 5;
	temp_cmd.cmd = reg;
	temp_cmd.data[0] = mode;

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0)
		ts_err("set(0x%X) mode [%d] failed", reg, mode);

	return ret;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, update_type;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	mutex_lock(&core_data->modechange_mutex);
	update_type = sec->cmd_param[0];

	switch (update_type) {
	case TSP_SDCARD:
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		update_type = TSP_SIGNED_SDCARD;
#endif
	case TSP_BUILT_IN:
	case TSP_SPU:
	case TSP_VERIFICATION:
		ret = goodix_fw_update(core_data, update_type, true);
		if (ret) {
			ts_err("failed to fw update %d", ret);
			ret = -EIO;
		}
		break;
	default:
		ret = -EINVAL;
		ts_err("not supported %d", sec->cmd_param[0]);
		break;
	}

	goodix_get_custom_library(core_data);
	core_data->plat_data->init(core_data);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	ts_info("%s", buff);
	mutex_unlock(&core_data->modechange_mutex);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "GOODIX");
	ts_info("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "GT%s", core_data->fw_version.patch_pid);

	ts_info("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_hw_ops *hw_ops = core_data->hw_ops;
	struct goodix_ic_info_sec *info_sec = &core_data->ic_info.sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char model[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ret = hw_ops->read_version(core_data, &core_data->fw_version);
	if (ret) {
		ts_err("failed to read version, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = hw_ops->get_ic_info(core_data, &core_data->ic_info);
	if (ret) {
		ts_err("failed to get ic info, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "GT%02X%02X%02X%02X",
			info_sec->ic_name_list,
			info_sec->project_id,
			info_sec->module_version,
			info_sec->firmware_version);
	snprintf(model, sizeof(model), "GT%02X%02X",
			info_sec->ic_name_list,
			info_sec->project_id);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ts_info("%s", buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "GT%02X%02X%02X%02X",
			core_data->fw_info_bin.ic_name_list,
			core_data->fw_info_bin.project_id,
			core_data->fw_info_bin.module_version,
			core_data->fw_info_bin.firmware_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ts_info("%s", buff);
}


static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "GT_%08X_%02X",
			core_data->ic_info.version.config_id,
			core_data->ic_info.version.config_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ts_info("%s", buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", core_data->ic_info.parm.drv_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ts_info("%s", buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", core_data->ic_info.parm.sen_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ts_info("%s", buff);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[3] = { 0 };

	ts_info("force power off");
	core_data->hw_ops->irq_enable(core_data, false);
	goodix_ts_power_off(core_data);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[3] = { 0 };

	ts_info("force power on");
	goodix_ts_power_on(core_data);
	core_data->hw_ops->irq_enable(core_data, true);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void set_factory_level(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < OFFSET_FAC_SUB || sec->cmd_param[0] > OFFSET_FAC_MAIN) {
		ts_err("cmd data is abnormal, %d", sec->cmd_param[0]);
		goto NG;
	}

	core_data->factory_position = sec->cmd_param[0];

	ts_info("%d", core_data->factory_position);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

enum trx_short_test_type {
	TEST_NONE = 0,
	OPEN_TEST,
	SHORT_TEST,
};

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;
	int type = TEST_NONE;
	char test[32];

	char fail_buff[1024] = {0};
	char tempv[25] = {0};

	sec_cmd_set_default_result(sec);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is off");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		type = OPEN_TEST;
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 2) {
		type = SHORT_TEST;
	}

	if (type == TEST_NONE) {
		ts_err("unsupported param %d,%d", sec->cmd_param[0], sec->cmd_param[1]);
		snprintf(buff, sizeof(buff), "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	memset(core_data->test_data.open_short_test_trx_result, 0x00, OPEN_SHORT_TEST_RESULT_LEN);

	core_data->hw_ops->irq_enable(core_data, false);

	if (type == OPEN_TEST) {
		memset(&core_data->test_data.info[SEC_OPEN_TEST], 0x00, sizeof(struct goodix_test_info));

		ret = goodix_open_test(core_data);
		if (ret < 0)
			goto out;

		if (core_data->test_data.info[SEC_OPEN_TEST].data[0] == 0)
			ret = 0;
		else
			ret = -EINVAL;
	} else if (type == SHORT_TEST) {
		memset(&core_data->test_data.info[SEC_SHORT_TEST], 0x00, sizeof(struct goodix_test_info));

		ret = goodix_short_test(core_data);
		if (ret < 0)
			goto out;

		if (core_data->test_data.info[SEC_SHORT_TEST].data[0] == 0)
			ret = 0;
		else
			ret = -EINVAL;
	}

out:
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (ret < 0) {
		int i, j;
		char *result_data = &core_data->test_data.open_short_test_trx_result[0];

		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");

		if (type == OPEN_TEST) {
			snprintf(tempv, 25, " TX/RX_OPEN:");
		} else if (type == SHORT_TEST) {
			snprintf(tempv, 25, " TX/RX_SHORT:");
		}
		strlcat(fail_buff, tempv, sizeof(fail_buff));

		/* make fail result */
		/* DRV[0~55] total 7 bytes */
		for (i = 0; i < DRV_CHAN_BYTES; i++) {
			if (result_data[i]) {
				for (j = 0; j < 8; j++) {
					if (result_data[i] & (1 << j)) {
						ts_info("DRV/TX[%d] open circuit, ret=0x%X", i * 8 + j, result_data[i]);
						memset(tempv, 0x00, 25);
						snprintf(tempv, 25, "TX%d,", i * 8 + j);
						strlcat(fail_buff, tempv, sizeof(fail_buff));
					}
				}
			}
		}

		/* SEN[0~79] total 10 bytes */
		for (i = 0; i < SEN_CHAN_BYTES; i++) {
			if (result_data[i + DRV_CHAN_BYTES]) {
				for (j = 0; j < 8; j++) {
					if (result_data[i + DRV_CHAN_BYTES] & (1 << j)) {
						ts_info("SEN/RX[%d] open circuit, ret=0x%X", i * 8 + j, result_data[i + DRV_CHAN_BYTES]);
						memset(tempv, 0x00, 25);
						snprintf(tempv, 20, "RX%d,", i * 8 + j);
						strlcat(fail_buff, tempv, sizeof(fail_buff));
					}
				}
			}
		}
		ts_info("fail buff = %s", fail_buff);

		snprintf(buff, sizeof(buff), "NG%s", fail_buff);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING && type == SHORT_TEST) {
		char tmp_buff[10] = { 0 };
		if (!ret)
			snprintf(tmp_buff, sizeof(tmp_buff), "%d", GOODIX_TEST_RESULT_PASS);
		else
			snprintf(tmp_buff, sizeof(tmp_buff), "%d", GOODIX_TEST_RESULT_FAIL);
		sec_cmd_set_cmd_result_all(sec, tmp_buff, strnlen(tmp_buff, sizeof(tmp_buff)), "SHORT");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void goodix_ts_print_frame(struct goodix_ts_core *core_data, struct goodix_ts_test_rawdata *rawdata)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize = CMD_RESULT_WORD_LEN * (core_data->ic_info.parm.drv_num + 1);

	ts_raw_info("[MUTUAL] datasize:%d, min:%d, max:%d", rawdata->size, rawdata->min, rawdata->max);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < core_data->ic_info.parm.drv_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	ts_raw_info("%s", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < core_data->ic_info.parm.drv_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	ts_raw_info("%s", pStr);

	for (i = 0; i < core_data->ic_info.parm.sen_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "RX%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < core_data->ic_info.parm.drv_num; j++) {
			snprintf(pTmp, sizeof(pTmp), " %5d",
					rawdata->data[j + (i * core_data->ic_info.parm.drv_num)]);
			strlcat(pStr, pTmp, lsize);
		}
		ts_raw_info("%s", pStr);
	}
	kfree(pStr);
}

static void goodix_ts_print_channel(struct goodix_ts_core *core_data, struct goodix_ts_test_self_rawdata *rawdata)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int i = 0, j = 0, k = 0;
	int lsize = CMD_RESULT_WORD_LEN * (core_data->ic_info.parm.drv_num + 1);

	if (!core_data->ic_info.parm.drv_num)
		return;

	ts_raw_info("[SELF] datasize:%d, TX :min:%d, max:%d, RX :min:%d, max:%d",
				rawdata->size, rawdata->tx_min, rawdata->tx_max, rawdata->rx_min, rawdata->rx_max);

	pStr = vzalloc(lsize);
	if (!pStr)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " TX");
	strlcat(pStr, pTmp, lsize);

	for (k = 0; k < core_data->ic_info.parm.drv_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "    %02d", k);
		strlcat(pStr, pTmp, lsize);
	}
	ts_raw_info("%s", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (k = 0; k < core_data->ic_info.parm.drv_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, lsize);
	}
	ts_raw_info("%s", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " | ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < core_data->ic_info.parm.drv_num + core_data->ic_info.parm.sen_num; i++) {
		if (i == core_data->ic_info.parm.drv_num) {
			ts_raw_info("%s", pStr);
			ts_raw_info(" ");
			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " RX");
			strlcat(pStr, pTmp, lsize);

			for (k = 0; k < core_data->ic_info.parm.sen_num; k++) {
				snprintf(pTmp, sizeof(pTmp), "    %02d", k);
				strlcat(pStr, pTmp, lsize);
			}

			ts_raw_info("%s", pStr);

			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " +");
			strlcat(pStr, pTmp, lsize);

			for (k = 0; k < core_data->ic_info.parm.drv_num; k++) {
				snprintf(pTmp, sizeof(pTmp), "------");
				strlcat(pStr, pTmp, lsize);
			}
			ts_raw_info("%s", pStr);

			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, lsize);
		} else if (i && !(i % core_data->ic_info.parm.drv_num)) {
			ts_raw_info("%s", pStr);
			memset(pStr, 0x0, lsize);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, lsize);
		}

		snprintf(pTmp, sizeof(pTmp), " %5d", rawdata->data[i]);
		strlcat(pStr, pTmp, lsize);

		j++;
	}
	ts_raw_info("%s", pStr);
	vfree(pStr);
}

static void goodix_get_gap_data(void *device_data, int freq)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_test_rawdata *rawdata;
	char buff[16] = { 0 };
	char spec_name[SEC_CMD_STR_LEN] = { 0 };
	int ii;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	int tx_max = 0;
	int rx_max = 0;

	if (freq == FREQ_HIGH) {
		rawdata = &core_data->test_data.high_freq_rawdata;
		snprintf(spec_name, sizeof(spec_name), "HIGH_FREQ_MUTUAL_RAW_GAP");
	} else if (freq == FREQ_LOW) {
		rawdata = &core_data->test_data.low_freq_rawdata;
		snprintf(spec_name, sizeof(spec_name), "LOW_FREQ_MUTUAL_RAW_GAP");
	} else {
		rawdata = &core_data->test_data.rawdata;
		snprintf(spec_name, sizeof(spec_name), "MUTUAL_RAW_GAP");
	}

	for (ii = 0; ii < (core_data->ic_info.parm.sen_num * core_data->ic_info.parm.drv_num); ii++) {
		if ((ii + 1) % (core_data->ic_info.parm.drv_num) != 0) {
			if (rawdata->data[ii] > rawdata->data[ii + 1])
				node_gap_tx = 100 - (rawdata->data[ii + 1] * 100 / rawdata->data[ii]);
			else
				node_gap_tx = 100 - (rawdata->data[ii] * 100 / rawdata->data[ii + 1]);
			tx_max = max(tx_max, node_gap_tx);
		}

		if (ii < (core_data->ic_info.parm.sen_num - 1) * core_data->ic_info.parm.drv_num) {
			if (rawdata->data[ii] > rawdata->data[ii + core_data->ic_info.parm.drv_num])
				node_gap_rx = 100 - (rawdata->data[ii + core_data->ic_info.parm.drv_num] * 100 / rawdata->data[ii]);
			else
				node_gap_rx = 100 - (rawdata->data[ii] * 100 / rawdata->data[ii + core_data->ic_info.parm.drv_num]);
			rx_max = max(rx_max, node_gap_rx);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char temp[SEC_CMD_STR_LEN] = { 0 };

		snprintf(temp, sizeof(temp), "%s_X", spec_name);
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, temp);
		snprintf(temp, sizeof(temp), "%s_Y", spec_name);
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, temp);
		snprintf(buff, sizeof(buff), "%d,%d", 0, max(tx_max, rx_max));
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, spec_name);
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_high_frequency_gap_data(void *device_data)
{
	goodix_get_gap_data(device_data, FREQ_HIGH);
}

static void get_low_frequency_gap_data(void *device_data)
{
	goodix_get_gap_data(device_data, FREQ_LOW);
}

static void get_gap_data(void *device_data)
{
	goodix_get_gap_data(device_data, FREQ_NORMAL);
}

static void goodix_get_gap_data_all(void *device_data, int freq)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_test_rawdata *rawdata;
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(core_data->ic_info.parm.drv_num * core_data->ic_info.parm.sen_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	if (freq == FREQ_HIGH)
		rawdata = &core_data->test_data.high_freq_rawdata;
	else if (freq == FREQ_LOW)
		rawdata = &core_data->test_data.low_freq_rawdata;
	else
		rawdata = &core_data->test_data.rawdata;

	for (ii = 0; ii < (core_data->ic_info.parm.sen_num * core_data->ic_info.parm.drv_num); ii++) {
		node_gap = node_gap_tx = node_gap_rx = 0;

		if ((ii + 1) % (core_data->ic_info.parm.drv_num) != 0) {
			if (rawdata->data[ii] > rawdata->data[ii + 1])
				node_gap_tx = 100 - (rawdata->data[ii + 1] * 100 / rawdata->data[ii]);
			else
				node_gap_tx = 100 - (rawdata->data[ii] * 100 / rawdata->data[ii + 1]);
		}

		if (ii < (core_data->ic_info.parm.sen_num - 1) * core_data->ic_info.parm.drv_num) {
			if (rawdata->data[ii] > rawdata->data[ii + core_data->ic_info.parm.drv_num])
				node_gap_rx = 100 - (rawdata->data[ii + core_data->ic_info.parm.drv_num] * 100 / rawdata->data[ii]);
			else
				node_gap_rx = 100 - (rawdata->data[ii] * 100 / rawdata->data[ii + core_data->ic_info.parm.drv_num]);
		}
		node_gap = max(node_gap_tx, node_gap_rx);
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
		strlcat(buff, temp, core_data->ic_info.parm.drv_num * core_data->ic_info.parm.sen_num * CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, core_data->ic_info.parm.drv_num * core_data->ic_info.parm.sen_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_high_frequency_gap_data_all(void *device_data)
{
	goodix_get_gap_data_all(device_data, FREQ_HIGH);
}

static void get_low_frequency_gap_data_all(void *device_data)
{
	goodix_get_gap_data_all(device_data, FREQ_LOW);
}

static void get_gap_data_all(void *device_data)
{
	goodix_get_gap_data_all(device_data, FREQ_NORMAL);
}

static void goodix_run_rawdata_read(void *device_data, int freq)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_test_rawdata *rawdata;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char spec_name[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	ts_raw_info("[FREQ] %s", (freq == FREQ_HIGH) ? "HIGH" : (freq == FREQ_LOW) ? "LOW" : "NORMAL");

	if (freq == FREQ_HIGH) {
		rawdata = &core_data->test_data.high_freq_rawdata;
		snprintf(spec_name, sizeof(spec_name), "HIGH_FREQ_MUTUAL_RAW");
	} else if (freq == FREQ_LOW) {
		rawdata = &core_data->test_data.low_freq_rawdata;
		snprintf(spec_name, sizeof(spec_name), "LOW_FREQ_MUTUAL_RAW");
	} else {
		rawdata = &core_data->test_data.rawdata;
		snprintf(spec_name, sizeof(spec_name), "MUTUAL_RAW");
	}

	memset(rawdata, 0x00, sizeof(struct goodix_ts_test_rawdata));

	ret = goodix_cache_rawdata(core_data, RAWDATA_TEST_TYPE_MUTUAL_RAW, freq);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d", rawdata->min, rawdata->max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	goodix_ts_print_frame(core_data, rawdata);

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), spec_name);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}
static void run_high_frequency_rawdata_read(void *device_data)
{
	goodix_run_rawdata_read(device_data, FREQ_HIGH);
}

static void run_low_frequency_rawdata_read(void *device_data)
{
	goodix_run_rawdata_read(device_data, FREQ_LOW);
}

static void run_rawdata_read(void *device_data)
{
	goodix_run_rawdata_read(device_data, FREQ_NORMAL);
}

static void run_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_rawdata_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.rawdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.rawdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.rawdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_cs_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);
	goodix_read_realtime(core_data, RAWDATA_TEST_TYPE_MUTUAL_RAW);
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->irq_enable(core_data, true);

	goodix_ts_print_frame(core_data, &core_data->test_data.rawdata);

	if (core_data->test_data.rawdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.rawdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.rawdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_cs_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);
	goodix_read_realtime(core_data, RAWDATA_TEST_TYPE_MUTUAL_DIFF);
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->irq_enable(core_data, true);

	goodix_ts_print_frame(core_data, &core_data->test_data.diffdata);

	if (core_data->test_data.diffdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.diffdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.diffdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_high_frequency_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_high_frequency_rawdata_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.high_freq_rawdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.high_freq_rawdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.high_freq_rawdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_low_frequency_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_low_frequency_rawdata_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.low_freq_rawdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.low_freq_rawdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.low_freq_rawdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_diffdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.diffdata, 0x00, sizeof(core_data->test_data.diffdata));

	ret = goodix_cache_rawdata(core_data, RAWDATA_TEST_TYPE_MUTUAL_DIFF, FREQ_NORMAL);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d",
				core_data->test_data.diffdata.min,
				core_data->test_data.diffdata.max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	goodix_ts_print_frame(core_data, &core_data->test_data.diffdata);

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MUTUAL_DIFF");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_diffdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = tx * rx * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_diffdata_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.diffdata.size != (tx * rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.diffdata.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.diffdata.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_self_rawdata_tx_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.selfraw, 0x00, sizeof(core_data->test_data.selfraw));

	ret = goodix_cache_rawdata(core_data, RAWDATA_TEST_TYPE_SELF_RAW, FREQ_NORMAL);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d", core_data->test_data.selfraw.tx_min, core_data->test_data.selfraw.tx_max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	goodix_ts_print_channel(core_data, &core_data->test_data.selfraw);

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_RAW_TX");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_self_rawdata_tx_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = (tx + rx) * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_self_rawdata_tx_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.selfraw.size != (tx + rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < tx; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.selfraw.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_self_rawdata_rx_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.selfraw, 0x00, sizeof(core_data->test_data.selfraw));

	ret = goodix_cache_rawdata(core_data, RAWDATA_TEST_TYPE_SELF_RAW, FREQ_NORMAL);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d", core_data->test_data.selfraw.rx_min, core_data->test_data.selfraw.rx_max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	goodix_ts_print_channel(core_data, &core_data->test_data.selfraw);

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_RAW_RX");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_self_rawdata_rx_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = (tx + rx) * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_self_rawdata_rx_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.selfraw.size != (tx + rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = tx; i < (tx + rx); i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.selfraw.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_self_diffdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.selfdiff, 0x00, sizeof(core_data->test_data.selfdiff));

	ret = goodix_cache_rawdata(core_data, RAWDATA_TEST_TYPE_SELF_DIFF, FREQ_NORMAL);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
	
		min = core_data->test_data.selfdiff.tx_min > core_data->test_data.selfdiff.rx_min ? core_data->test_data.selfdiff.rx_min : core_data->test_data.selfdiff.tx_min;
		max = core_data->test_data.selfdiff.tx_max > core_data->test_data.selfdiff.rx_max ? core_data->test_data.selfdiff.tx_max : core_data->test_data.selfdiff.rx_max;
		ts_raw_info("%s : selfdiff tx: %d,%d rx: %d,%d\n", __func__,
					core_data->test_data.selfdiff.tx_min, core_data->test_data.selfdiff.tx_max,
					core_data->test_data.selfdiff.rx_min, core_data->test_data.selfdiff.rx_max);
		
		snprintf(buff, sizeof(buff), "%d,%d", min, max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	goodix_ts_print_channel(core_data, &core_data->test_data.selfdiff);

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_DIFF");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_self_diffdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *rawdata_buff;
	u8 tx = core_data->ic_info.parm.drv_num;
	u8 rx = core_data->ic_info.parm.sen_num;
	int buff_size = (tx + rx) * 7;
	int i;

	sec_cmd_set_default_result(sec);

	rawdata_buff = vzalloc(buff_size);
	if (!rawdata_buff) {
		ts_err("failed to alloc rawdata_buff");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	run_self_diffdata_read(sec);

	sec_cmd_set_default_result(sec);

	if (core_data->test_data.selfdiff.size != (tx + rx)) {
		ts_err("test result is NG");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		vfree(rawdata_buff);
		return;
	}

	for (i = 0; i < core_data->test_data.selfdiff.size; i++) {
		snprintf(buff, sizeof(buff), "%d,", core_data->test_data.selfdiff.data[i]);
		strlcat(rawdata_buff, buff, buff_size);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, rawdata_buff, buff_size);
	ts_info("%s", rawdata_buff);
	vfree(rawdata_buff);
}

static void run_jitter_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;
	int mutual[2] = { 0 };
	int self[2] = { 0 };
	int min_idx = 0;
	int max_idx = 1;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.info[SEC_JITTER1_TEST], 0x00, sizeof(struct goodix_test_info));

	ret = goodix_jitter_test(core_data, JITTER_100_FRAMES);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		goto out;
	}

	if (!core_data->test_data.info[SEC_JITTER1_TEST].isFinished) {
		ts_err("test is not finished");
		ret = -EIO;
		goto out;
	}

	mutual[min_idx] = core_data->test_data.info[SEC_JITTER1_TEST].data[1];
	mutual[max_idx] = core_data->test_data.info[SEC_JITTER1_TEST].data[0];
	self[min_idx] = core_data->test_data.info[SEC_JITTER1_TEST].data[3];
	self[max_idx] = core_data->test_data.info[SEC_JITTER1_TEST].data[2];
	ts_raw_info("mutual: %d, %d | self: %d, %d",
			mutual[min_idx], mutual[max_idx], self[min_idx], self[max_idx]);
out:
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MUTUAL_JITTER");
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SELF_JITTER");
		}
	} else {
		snprintf(buff, sizeof(buff), "%d,%d", mutual[min_idx], mutual[max_idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			char buffer[SEC_CMD_STR_LEN] = { 0 };

			snprintf(buffer, sizeof(buffer), "%d,%d", mutual[min_idx], mutual[max_idx]);
			sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "MUTUAL_JITTER");

			memset(buffer, 0x00, sizeof(buffer));
			snprintf(buffer, sizeof(buffer), "%d,%d", self[min_idx], self[max_idx]);
			sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "SELF_JITTER");
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_jitter_delta_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;
	int min_matrix[2] = { 0 };
	int max_matrix[2] = { 0 };
	int avg_matrix[2] = { 0 };
	int min_idx = 0;
	int max_idx = 1;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.info[SEC_JITTER2_TEST], 0x00, sizeof(struct goodix_test_info));

	ret = goodix_jitter_test(core_data, JITTER_1000_FRAMES);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		goto out;
	}

	if (!core_data->test_data.info[SEC_JITTER2_TEST].isFinished) {
		ts_err("test is not finished");
		ret = -EIO;
		goto out;
	}

	min_matrix[min_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[0];
	min_matrix[max_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[1];
	max_matrix[min_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[2];
	max_matrix[max_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[3];
	avg_matrix[min_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[4];
	avg_matrix[max_idx] = core_data->test_data.info[SEC_JITTER2_TEST].data[5];
	ts_raw_info("min: %d, %d | max: %d, %d | avg: %d, %d",
			min_matrix[min_idx], min_matrix[max_idx],
			max_matrix[min_idx], max_matrix[max_idx],
			avg_matrix[min_idx], avg_matrix[max_idx]);
out:
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "JITTER_DELTA_MIN");
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "JITTER_DELTA_MAX");
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "JITTER_DELTA_AVG");
		}
	} else {
		snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d",
			min_matrix[min_idx], min_matrix[max_idx],
			max_matrix[min_idx], max_matrix[max_idx],
			avg_matrix[min_idx], avg_matrix[max_idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;

		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			char buffer[SEC_CMD_STR_LEN] = { 0 };

			snprintf(buffer, sizeof(buffer), "%d,%d", min_matrix[min_idx], min_matrix[max_idx]);
			sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MIN");

			memset(buffer, 0x00, sizeof(buffer));
			snprintf(buffer, sizeof(buffer), "%d,%d", max_matrix[min_idx], max_matrix[max_idx]);
			sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MAX");

			memset(buffer, 0x00, sizeof(buffer));
			snprintf(buffer, sizeof(buffer), "%d,%d", avg_matrix[min_idx], avg_matrix[max_idx]);
			sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_AVG");
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_interrupt_gpio_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = -EIO;
	uint32_t gio_reg;
	uint32_t reg_data;
	uint32_t int_bit;
	uint8_t temp_buf[16] = {0};
	int retry = 20;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	/* fix to idle */
	ret = goodix_set_cmd(core_data, 0x9F, 0x00);
	if (ret < 0) {
		ts_err("failed to send idle cmd");
		goto out;
	}

	/* close watching dog */
	core_data->hw_ops->write(core_data, WATCH_DOG_REG, temp_buf, 1);

	while (retry--)	{
		memset(temp_buf, 0, sizeof(temp_buf));
		temp_buf[2] = 0x01;
		temp_buf[3] = 0x00;
		core_data->hw_ops->write(core_data, 0x0000, temp_buf, 4);
		core_data->hw_ops->read(core_data, 0x2000, &temp_buf[4], 4);
		core_data->hw_ops->read(core_data, 0x2000, &temp_buf[8], 4);
		core_data->hw_ops->read(core_data, 0x2000, &temp_buf[12], 4);
		if (!memcmp(&temp_buf[4], &temp_buf[8], 4) && !memcmp(&temp_buf[4], &temp_buf[12], 4))
			break;

		sec_delay(2);
	}

	if (retry < 0) {
		ts_err("Failed to hold CPU");
		ret = -EIO;
		goto out;
	}

	if (core_data->bus->ic_type == IC_TYPE_BERLIN_B) {
		gio_reg = GIO_REG_BB;
		int_bit = 0x001;
	} else {
		gio_reg = GIO_REG_BD;
		int_bit = 0x200;
	}

	core_data->hw_ops->read(core_data, gio_reg, (uint8_t *)&reg_data, 4);
	retry = 3;
	while (retry--)	{
		reg_data |= int_bit; // set int gpio to high
		core_data->hw_ops->write(core_data, gio_reg, (uint8_t *)&reg_data, 4);
		sec_delay(1);
		if (!gpio_get_value(core_data->plat_data->irq_gpio)) {
			ts_err("int gpio is LOW, should HIGH");
			snprintf(buff, sizeof(buff), "%s", "1:LOW");
			ret = -EIO;
			goto out;
		}
		reg_data &= ~int_bit; // set int gpio to low
		core_data->hw_ops->write(core_data, gio_reg, (uint8_t *)&reg_data, 4);
		sec_delay(1);
		if (gpio_get_value(core_data->plat_data->irq_gpio)) {
			ts_err("int gpio is HIGH, should LOW");
			snprintf(buff, sizeof(buff), "%s", "1:HIGH");
			ret = -EIO;
			goto out;
		}
	}
	ret = 0;
	ts_info("INT test ok");

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "0");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");

	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);
}

static void run_snr_non_touched(void *device_data)
{
 	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 1;
	int frame_cnt = 0;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		ts_err("abnormal parm : %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	frame_cnt = sec->cmd_param[0];
	ts_raw_info("frame_cnt(%d)", frame_cnt);

	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	ret = goodix_snr_test(core_data, SNR_TEST_NON_TOUCH, frame_cnt);
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

static void run_snr_touched(void *device_data)
{
 	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 1;
	int i = 0;
	int frame_cnt = 0;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		ts_err("abnormal parm : %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	frame_cnt = sec->cmd_param[0];
	ts_raw_info("frame_cnt(%d)", frame_cnt);

	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
	ret = goodix_snr_test(core_data, SNR_TEST_TOUCH, frame_cnt);
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (i = 0 ; i < 9 ; i++) {
		ts_raw_info("[#%d] average:%d, snr1:%d, snr2:%d\n", i,
					core_data->test_data.snr_result[i * 3],
					core_data->test_data.snr_result[i * 3 + 1],
					core_data->test_data.snr_result[i * 3 + 2]);
		snprintf(tbuff, sizeof(tbuff), "%d,%d,%d,",
					core_data->test_data.snr_result[i * 3],
					core_data->test_data.snr_result[i * 3 + 1],
					core_data->test_data.snr_result[i * 3 + 2]);
		strlcat(buff, tbuff, sizeof(buff));
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("done : %s", buff);
}

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 1;

	sec_cmd_set_default_result(sec);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	core_data->hw_ops->irq_enable(core_data, false);

	memset(&core_data->test_data.info[SEC_SRAM_TEST], 0x00, sizeof(struct goodix_test_info));

	ret = goodix_sram_test(core_data);
	if (ret < 0) {
		ts_err("test failed, %d", ret);
		goto out;
	}

	if (!core_data->test_data.info[SEC_SRAM_TEST].isFinished) {
		ts_err("test is not finished");
		ret = -EIO;
		goto out;
	}

	ret = core_data->test_data.info[SEC_SRAM_TEST].data[0];

out:
	goodix_ts_release_all_finger(core_data);
	core_data->hw_ops->reset(core_data, 200);
	core_data->hw_ops->irq_enable(core_data, true);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d", ret);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	ts_raw_info("%s", buff);
}

int goodix_write_nvm_data(struct goodix_ts_core *cd, unsigned char *data, int size);
int goodix_read_nvm_data(struct goodix_ts_core *cd, unsigned char *data, int size);

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[2] = { 0 };

	sec_cmd_set_default_result(sec);

	ret = goodix_read_nvm_data(core_data, data, 1);
	if (ret < 0) {
		ts_err("nvm read error(%d)", ret);
		goto out;
	}

	ts_info("nvm read disassemble_count(%d)", data[0]);

	if (data[0] == 0xFF)
		data[0] = 0;

	if (data[0] < 0xFE)
		data[0]++;

	ret = goodix_write_nvm_data(core_data, data, 1);
	if (ret < 0) {
		ts_err("nvm write error(%d)", ret);
	}

out:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[2] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ret = goodix_read_nvm_data(core_data, data, 1);
	if (ret < 0) {
		ts_err("nvm read error(%d)", data[0]);
		data[0] = 0;
	}

	if (data[0] == 0xff) {
		ts_err("clear nvm disassemble count(%X)", data[0]);
		data[0] = 0;
		ret = goodix_write_nvm_data(core_data, data, 1);
		if (ret < 0) {
			ts_err("nvm write error(%d)", ret);
		}
	}

	ts_info("nvm disassemble count(%d)", data[0]);

	snprintf(buff, sizeof(buff), "%d", data[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	mutex_lock(&core_data->input_dev->mutex);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);
	run_interrupt_gpio_test(sec);
	run_rawdata_read(sec);
	get_gap_data(sec);
	run_high_frequency_rawdata_read(sec);
	get_high_frequency_gap_data(sec);
	run_low_frequency_rawdata_read(sec);
	get_low_frequency_gap_data(sec);
	run_self_rawdata_rx_read(sec);
	run_self_rawdata_tx_read(sec);

	run_jitter_test(sec);
	run_sram_test(sec);

	/* for short test */
	sec->cmd_param[0] = 1;
	sec->cmd_param[1] = 2;
	run_trx_short_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	mutex_unlock(&core_data->input_dev->mutex);

out:
	ts_raw_info("%d%s", sec->item_count, sec->cmd_result_all);
}

void goodix_ts_run_rawdata_all(struct goodix_ts_core *cd)
{
	struct sec_cmd_data *sec = &cd->sec;

	run_rawdata_read(sec);
}

static void factory_cmd_result_all_imagetest(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (atomic_read(&core_data->suspended)) {
		ts_err("IC is on suspend state");
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	mutex_lock(&core_data->input_dev->mutex);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_jitter_delta_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
	mutex_unlock(&core_data->input_dev->mutex);

out:
	ts_info("%d%s", sec->item_count, sec->cmd_result_all);
}

static void fix_active_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("fix active mode : %s", sec->cmd_param[0] ? "enable" : "disabled");

	if (sec->cmd_param[0]) {	//enable
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x9F;
		temp_cmd.data[0] = 2;
	} else {					//disabled
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x9F;
		temp_cmd.data[0] = 1;
	}

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send fix active mode cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

#if 0
static int goodix_ts_set_mode(struct goodix_ts_core *core_data, u8 reg, u8 data, int len)
{
	struct goodix_ts_cmd temp_cmd;
	int ret;

	temp_cmd.len = len;
	temp_cmd.cmd = reg;
	temp_cmd.data[0] = data;

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send cmd failed");
		return ret;
	}

	ts_info("reg : 0x0X, data:0x0X, len:%d", reg, data, len);
	return 0;
}
#endif

#define GOODIX_POKET_MODE_STATE_ADDR		0x71	// 0:ed, 1:pocket
static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		ts_err("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	core_data->plat_data->pocket_mode = sec->cmd_param[0];
	ts_info("pocket mode : %s", core_data->plat_data->pocket_mode ? "on" : "off");

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto fail;
	}

	ret = core_data->hw_ops->pocket_mode_enable(core_data, core_data->plat_data->pocket_mode);
	if (ret < 0) {
		ts_err("send pocket mode cmd failed");
		goto fail;
	}

	ts_info("send pocket mode cmd done");

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!(sec->cmd_param[0] == 0 || sec->cmd_param[0] == 1 || sec->cmd_param[0] == 3)) {
		ts_err("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	core_data->plat_data->ed_enable = sec->cmd_param[0];
	ts_info("ear detect mode(%d)", core_data->plat_data->ed_enable);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto fail;
	}

	ret = core_data->hw_ops->ed_enable(core_data, core_data->plat_data->ed_enable);
	if (ret < 0) {
		ts_err("send ear detect cmd failed");
		goto fail;
	}

	ts_info("send ear detect cmd done");

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

/* 0: exit LSM  1: enter LSM  2: debug  3: debug */
static void low_sensitivity_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		ts_err("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	mutex_lock(&core_data->modechange_mutex);
	core_data->plat_data->low_sensitivity_mode = sec->cmd_param[0];

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		mutex_unlock(&core_data->modechange_mutex);
		ts_err("IC is OFF");
		goto fail;
	}

	ret = goodix_set_cmd(core_data, GOODIX_LS_MODE_ADDR, core_data->plat_data->low_sensitivity_mode);
	if (ret < 0) {
		mutex_unlock(&core_data->modechange_mutex);
		ts_err("send low sensitivity mode cmd failed");
		goto fail;
	}
	mutex_unlock(&core_data->modechange_mutex);

	ts_info("set low sensitivity mode: %d", core_data->plat_data->low_sensitivity_mode);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data =
			container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct goodix_ts_cmd temp_cmd;
	int retry = 20;
	u16 sum_x, sum_y;
	u16 thd_x, thd_y;
	u8 temp_buf[11];
	int ret;

	sec_cmd_set_default_result(sec);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	/* must enabled call mode 3 firstly */
	if (core_data->plat_data->ed_enable != 3) {
		temp_cmd.cmd = GOODIX_ED_MODE_ADDR;
		temp_cmd.data[0] = 3;
		temp_cmd.len = 5;
		ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
		if (ret < 0) {
			ts_err("enable ear mode failed");
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}
	}

	temp_cmd.cmd = 0x79;
	temp_cmd.data[0] = 1;
	temp_cmd.len = 5;
	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("enable prox test failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	while (retry--) {
		sec_delay(5);
		ret = core_data->hw_ops->read(core_data, 0x15D4C, temp_buf, sizeof(temp_buf));
		if (ret == 0 && temp_buf[0] == 0xAA)
			break;
	}
	if (retry < 0) {
		ts_err("SUM value not ready, status[%X]", temp_buf[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (checksum_cmp(temp_buf + 1, sizeof(temp_buf) - 1, CHECKSUM_MODE_U8_LE)) {
		ts_err("prox data checksum error");
		ts_err("data:%*ph", (int)sizeof(temp_buf) - 1, temp_buf + 1);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sum_x = le16_to_cpup((__le16 *)(temp_buf + 1));
	sum_y = le16_to_cpup((__le16 *)(temp_buf + 3));
	thd_x = le16_to_cpup((__le16 *)(temp_buf + 5));
	thd_y = le16_to_cpup((__le16 *)(temp_buf + 7));
	ts_info("SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d", sum_x, sum_y, thd_x, thd_y);

	snprintf(buff, sizeof(buff), "SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d", sum_x, sum_y, thd_x, thd_y);
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	temp_cmd.cmd = 0x79;
	temp_cmd.data[0] = 0;
	temp_cmd.len = 5;
	core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (core_data->plat_data->ed_enable != 3) {
		temp_cmd.cmd = GOODIX_ED_MODE_ADDR;
		temp_cmd.data[0] = 0;
		temp_cmd.len = 5;
		core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void pocket_mode_state(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct goodix_ts_cmd temp_cmd;
	unsigned char status;
	int ret;

	sec_cmd_set_default_result(sec);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto fail;
	}

	temp_cmd.len = 5;
	temp_cmd.cmd = GOODIX_POKET_MODE_STATE_ADDR;
	temp_cmd.data[0] = 1;
	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_info("fail to send_cmd : %d", ret);
		goto fail;
	}
	sec_delay(20);
	ret = core_data->hw_ops->read(core_data, core_data->ic_info.misc.cmd_addr, &status, 1);
	if (ret < 0) {
		ts_info("fail to read_cmd : %d", ret);
		goto fail;
	}

	if (status == 0x91)
		snprintf(buff, sizeof(buff), "enable");
	else if (status == 0x90)
		snprintf(buff, sizeof(buff), "disable");

	ts_info("pocket mode %s", buff);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void ear_detect_state(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct goodix_ts_cmd temp_cmd;
	unsigned char status;
	int ret;

	sec_cmd_set_default_result(sec);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto fail;
	}

	temp_cmd.len = 5;
	temp_cmd.cmd = GOODIX_POKET_MODE_STATE_ADDR;
	temp_cmd.data[0] = 0;
	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_info("fail to send_cmd : %d", ret);
		goto fail;
	}
	sec_delay(20);
	ret = core_data->hw_ops->read(core_data, core_data->ic_info.misc.cmd_addr, &status, 1);
	if (ret < 0) {
		ts_info("fail to read_cmd : %d", ret);
		goto fail;
	}

	if (status == 0x91)
		snprintf(buff, sizeof(buff), "enable");
	else if (status == 0x90)
		snprintf(buff, sizeof(buff), "disable");

	ts_info("ear detect mode %s", buff);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}
#endif

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("game mode : %s", sec->cmd_param[0] ? "on" : "off");

	if (sec->cmd_param[0]) {	//enable game mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0xC2;
		temp_cmd.data[0] = 1;
	} else {					//disabled game mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0xC2;
		temp_cmd.data[0] = 0;
	}

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send game mode cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("glove mode : %s", sec->cmd_param[0] ? "on" : "off");

	if (sec->cmd_param[0])
		core_data->glove_enable = 1;
	else
		core_data->glove_enable = 0;

	ret = goodix_set_cmd(core_data, GOODIX_GLOVE_MODE_ADDR, core_data->glove_enable);
	if (ret < 0) {
		ts_err("send glove mode cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("sip mode : %s", sec->cmd_param[0] ? "on" : "off");

	if (sec->cmd_param[0]) {	//enable sip mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x73;
		temp_cmd.data[0] = 1;
	} else {					//disabled sip mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x73;
		temp_cmd.data[0] = 0;
	}

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send sip mode cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("note mode : %s", sec->cmd_param[0] ? "on" : "off");

	if (sec->cmd_param[0]) {	//enable note mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x74;
		temp_cmd.data[0] = 1;
	} else {					//disabled note mode
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x74;
		temp_cmd.data[0] = 0;
	}

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send note mode cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	ts_info(" spay %s, %02X\n",
			sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	goodix_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

int goodix_set_aod_rect(struct goodix_ts_core *ts)
{
	u8 data[8] = {0, };
	int ret = 0;
	int i;

	for (i = 0; i < 4; i++) {
		data[i * 2] = ts->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 1] = (ts->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->hw_ops->write_to_sponge(ts, 0x02, data, 8);
	if (ret < 0)
		ts_info("Failed to write sponge\n");

	return ret;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret, i;

	sec_cmd_set_default_result(sec);

	ts_info(" w:%d, h:%d, x:%d, y:%d, lowpower_mode:0x%02X\n",
			sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3], ts->plat_data->lowpower_mode);

	for (i = 0; i < 4; i++)
		ts->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto NG;
	}

	ret = goodix_set_aod_rect(ts);
	if (ret < 0)
		goto NG;

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 data[8] = {0, };
	u16 rect_data[4] = {0, };
	int ret, i;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		goto NG;
	}

	ret = ts->hw_ops->read_from_sponge(ts, 0x02, data, 8);
	if (ret < 0) {
		ts_info("Failed to read rect\n");
		goto NG;
	}

	for (i = 0; i < 4; i++)
		rect_data[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	ts_info("w:%d, h:%d, x:%d, y:%d\n",
			rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_AOD;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_AOD;

	ts_info("aod: %s, %02X\n",
			sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	goodix_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	ts_info("%s, %02X\n", sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	goodix_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

void goodix_get_custom_library(struct goodix_ts_core *ts)
{
	u8 data[6] = { 0 };
	int ret, i;

	ret = ts->hw_ops->read_from_sponge(ts, SEC_TS_CMD_SPONGE_AOD_ACTIVE_INFO, data, 6);
	if (ret < 0) {
		ts_err("Failed to read aod active area");
	}

	for (i = 0; i < 3; i++)
		ts->plat_data->aod_data.active_area[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	ts_info("aod_active_area - top:%d, edge:%d, bottom:%d",
			ts->plat_data->aod_data.active_area[0],
			ts->plat_data->aod_data.active_area[1], ts->plat_data->aod_data.active_area[2]);

	memset(data, 0x00, 6);

	ret = ts->hw_ops->read_from_sponge(ts, SEC_TS_CMD_SPONGE_FOD_INFO, data, 4);
	if (ret < 0) {
		ts_err("Failed to read fod info");
	}

	sec_input_set_fod_info(ts->bus->dev, data[0], data[1], data[2], data[3]);
}

int goodix_set_custom_library(struct goodix_ts_core *ts)
{
	u8 data[1] = { 0 };
	int ret;
	u8 force_fod_enable = 0;

#if IS_ENABLED(CONFIG_SEC_FACTORY)
		/* enable FOD when LCD on state */
	if (ts->plat_data->support_fod && ts->plat_data->enabled)
		force_fod_enable = SEC_TS_MODE_SPONGE_PRESS;
#endif

	ts_info("Sponge (0x%02x)%s\n",
			ts->plat_data->lowpower_mode,
			force_fod_enable ? ", force fod enable" : "");

	if (ts->plat_data->prox_power_off) {
		data[0] = (ts->plat_data->lowpower_mode | force_fod_enable) &
						~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
		ts_info("prox off. disable AOT");
	} else {
		data[0] = ts->plat_data->lowpower_mode | force_fod_enable;
	}

	ret = ts->hw_ops->write_to_sponge(ts, 0, data, 1);
	if (ret < 0)
		ts_err("Failed to write sponge");

	return ret;
}

int goodix_set_press_property(struct goodix_ts_core *ts)
{
	u8 data[1] = { 0 };
	int ret;

	if (!ts->plat_data->support_fod)
		return 0;

	data[0] = ts->plat_data->fod_data.press_prop;

	ret = ts->hw_ops->write_to_sponge(ts, SEC_TS_CMD_SPONGE_PRESS_PROPERTY, data, 1);
	if (ret < 0)
		ts_err("Failed to write sponge\n");

	ts_info("%d", ts->plat_data->fod_data.press_prop);

	return ret;
}

int goodix_set_fod_rect(struct goodix_ts_core *ts)
{
	u8 data[8] = { 0 };
	int ret, i;

	ts_info("l:%d, t:%d, r:%d, b:%d\n",
		ts->plat_data->fod_data.rect_data[0], ts->plat_data->fod_data.rect_data[1],
		ts->plat_data->fod_data.rect_data[2], ts->plat_data->fod_data.rect_data[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2] = ts->plat_data->fod_data.rect_data[i] & 0xFF;
		data[i * 2 + 1] = (ts->plat_data->fod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->hw_ops->write_to_sponge(ts, 0x4b, data, 8);
	if (ret < 0)
		ts_err("Failed to write sponge");

	return ret;
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_PRESS;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_PRESS;

	ts->plat_data->fod_data.press_prop = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

	ts_info("%s, fast:%s, strict:%s, %02X\n",
			sec->cmd_param[0] ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 1 ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 2 ? "on" : "off",
			ts->plat_data->lowpower_mode);

	mutex_lock(&ts->modechange_mutex);


	if (!ts->plat_data->enabled && !ts->plat_data->lowpower_mode && !ts->plat_data->pocket_mode
			&& !ts->plat_data->ed_enable && !ts->plat_data->fod_lp_mode) {
		if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM)
			disable_irq_wake(ts->irq);
		ts->hw_ops->irq_enable(ts, false);
		goodix_ts_power_off(ts);
	} else {
		goodix_set_custom_library(ts);
		goodix_set_press_property(ts);
	}

	mutex_unlock(&ts->modechange_mutex);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts->plat_data->support_fod_lp_mode) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts->plat_data->fod_lp_mode = sec->cmd_param[0];
	ts_info("%d", ts->plat_data->fod_lp_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ts_info("l:%d, t:%d, r:%d, b:%d",
			sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (!sec_input_set_fod_rect(ts->bus->dev, sec->cmd_param))
		goto NG;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("Touch is stopped! Set data at reinit()");
		goto OK;
	}

	ret = goodix_set_fod_rect(ts);
	if (ret < 0)
		goto NG;

OK:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SINGLE_TAP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SINGLE_TAP;

	ts_info("singletap: %s, %02X\n",
			sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	goodix_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	core_data->debug_flag = sec->cmd_param[0];

	ts_info("%s: debug_flag is 0x%X\n", __func__, core_data->debug_flag);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	uint32_t esd_addr = core_data->ic_info.misc.esd_addr;
	uint8_t esd_val = 0xAA;
	int ret;

	sec_cmd_set_default_result(sec);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF ||
			core_data->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		ts_err("IC is not ready(%d)", core_data->plat_data->power_state);
		goto err_out;
	}

	ret = core_data->hw_ops->write(core_data, esd_addr, &esd_val, 1);
	if (ret < 0) {
		ts_err("write esd val failed");
		goto err_out;
	}

	sec_delay(100);
	/* If the FW don't set esd_val to 0xFF, the FW has been crashed */
	ret = core_data->hw_ops->read(core_data, esd_addr, &esd_val, 1);
	if (ret < 0) {
		ts_err("read esd val failed");
		goto err_out;
	}

	if (esd_val == 0xAA) {
		ts_err("esd check failed");
		goto err_out;
	}

	ts_info("check connection OK");
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

err_out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}

// have change it
#if 0
static void set_charger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case TYPE_WIRELESS_CHARGER_NONE:
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 0;
		temp_cmd.len = 5;
		break;
	case TYPE_WIRELESS_CHARGER:
		temp_cmd.cmd = 0xAF;
		temp_cmd.data[0] = 1;
		temp_cmd.len = 5;
		break;
	default:
		ts_err("invalid param %d", sec->cmd_param[0]);
		goto NG;
	}

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send charger cmd failed");
		goto NG;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}
#endif

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] == 0) {	// clear
			core_data->plat_data->grip_data.edgehandler_direction = 0;
		} else if (sec->cmd_param[1] < 3) {
			core_data->plat_data->grip_data.edgehandler_direction = sec->cmd_param[1];
			core_data->plat_data->grip_data.edgehandler_start_y = sec->cmd_param[2];
			core_data->plat_data->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			ts_err("cmd1 is abnormal, %d", sec->cmd_param[1]);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		core_data->plat_data->set_grip_data(core_data->bus->dev, mode);
	} else if (sec->cmd_param[0] == 1) {	// portrait mode
		core_data->plat_data->grip_data.edge_range = sec->cmd_param[1];
		core_data->plat_data->grip_data.deadzone_up_x = sec->cmd_param[2];
		core_data->plat_data->grip_data.deadzone_dn_x = sec->cmd_param[3];
		core_data->plat_data->grip_data.deadzone_y = sec->cmd_param[4];
		mode = mode | G_SET_NORMAL_MODE;
		core_data->plat_data->set_grip_data(core_data->bus->dev, mode);
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// use previous portrait setting
			core_data->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			core_data->plat_data->grip_data.landscape_mode = 1;
			core_data->plat_data->grip_data.landscape_edge = sec->cmd_param[2];
			core_data->plat_data->grip_data.landscape_deadzone = sec->cmd_param[3];
			core_data->plat_data->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			core_data->plat_data->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			core_data->plat_data->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			core_data->plat_data->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = mode | G_SET_LANDSCAPE_MODE;
		} else {
			ts_err("cmd1 is abnormal, %d", sec->cmd_param[1]);
			goto err_grip_data;
		}
		core_data->plat_data->set_grip_data(core_data->bus->dev, mode);
	} else {
		ts_err("cmd0 is abnormal, %d", sec->cmd_param[0]);
		goto err_grip_data;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

int goodix_set_cover_mode(struct goodix_ts_core *core_data)
{
	struct goodix_ts_cmd temp_cmd;
	int ret = -1;

	if (core_data->plat_data->sense_off_when_cover_closed) {
		if (core_data->flip_enable) {
			ret = core_data->hw_ops->sense_off(core_data, 1);
			goodix_ts_release_all_finger(core_data);
		} else {
			ret = core_data->hw_ops->sense_off(core_data, 0);
		}

	} else {
		if (core_data->plat_data->cover_type >= 0) {
			temp_cmd.len = 6;
			temp_cmd.cmd = GOODIX_COVER_MODE_ADDR;
			temp_cmd.data[0] = core_data->flip_enable;
			temp_cmd.data[1] = sec_input_check_cover_type(core_data->bus->dev) & 0xFF;
			ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
			if (ret < 0)
				ts_err("cover mode [%d/%d] failed",
							core_data->flip_enable, core_data->plat_data->cover_type);
		} else {
			ts_err("Abnormal cover type [%d]", core_data->plat_data->cover_type);
		}
	}

	return ret;
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	ts_info("skip for factory binary");
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);
	return;
#endif

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		ts_err("not support param[%d/%d]", sec->cmd_param[0], sec->cmd_param[1]);
		goto exit;
	}

	if (sec->cmd_param[0] > 1) {
		/* cover closed */
		core_data->flip_enable = true;
	} else {
		/* cover opened */
		core_data->flip_enable = false;
	}
	core_data->plat_data->cover_type = sec->cmd_param[1];

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_info("tsp ic off, save data & set later");
		goto out;
	}

	mutex_lock(&core_data->input_dev->mutex);
	ret = goodix_set_cover_mode(core_data);
	mutex_unlock(&core_data->input_dev->mutex);

out:
	if (ret < 0) {
		ts_err("failed to set cover %s [%d]",
					core_data->flip_enable ? "close" : "open",
					core_data->plat_data->cover_type);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		ts_info("set cover %s [%d] success",
					core_data->flip_enable ? "close" : "open",
					core_data->plat_data->cover_type);
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

int set_refresh_rate_mode(struct goodix_ts_core *core_data)
{
	struct goodix_ts_cmd temp_cmd;
	int ret;

	temp_cmd.len = 5;
	temp_cmd.cmd = 0x9D;
	temp_cmd.data[0] = core_data->refresh_rate;
	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);

	if (ret < 0)
		ts_err("failed to set scan rate[%d]", core_data->refresh_rate);
	else
		ts_info("set scan rate[%d] success", core_data->refresh_rate);

	return ret;
}

/*	refresh_rate_mode
byte[0]: 60 / 90 | 120
* 0 : normal (60hz)
* 1 : adaptive
* 2 : always (90Hz/120hz)
*/
static void refresh_rate_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!core_data->refresh_rate_enable) {
		ts_err("not support cmd(%d)!", sec->cmd_param[0]);
		goto NG;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		ts_err("not support param[%d]", sec->cmd_param[0]);
		goto NG;
	}

	core_data->refresh_rate = sec->cmd_param[0];

	ret = set_refresh_rate_mode(core_data);
	if (ret < 0)
		goto NG;

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

/*
 * 0x00 express not report from 0.5mm to edge. (dead_zone_enable,1)
 * 0x01 express report from 0.5mm to edge. (dead_zone_enable,0)
 *
 */
static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_info("report edge : %s", sec->cmd_param[0] ? "off" : "on");

	temp_cmd.len = 5;
	temp_cmd.cmd = 0x75;
	if (sec->cmd_param[0] == 0)
		temp_cmd.data[0] = 1;
	else
		temp_cmd.data[0] = 0;

	ret = core_data->hw_ops->send_cmd(core_data, &temp_cmd);
	if (ret < 0) {
		ts_err("send report edge cmd failed");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	ts_info("%s", buff);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("set_factory_level", set_factory_level),},
//	{SEC_CMD("get_checksum_data", get_checksum_data),},
//	{SEC_CMD("get_crc_check", check_fw_corruption),},
	{SEC_CMD("run_cs_raw_read_all", run_cs_raw_read_all),},
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all),},
	{SEC_CMD("run_rawdata_read", run_rawdata_read),},
	{SEC_CMD("run_rawdata_read_all", run_rawdata_read_all),},
	{SEC_CMD("get_gap_data_all", get_gap_data_all),},
	{SEC_CMD("run_high_frequency_rawdata_read", run_high_frequency_rawdata_read),},
	{SEC_CMD("run_high_frequency_rawdata_read_all", run_high_frequency_rawdata_read_all),},
	{SEC_CMD("get_high_frequency_gap_data_all", get_high_frequency_gap_data_all),},
	{SEC_CMD("run_low_frequency_rawdata_read", run_low_frequency_rawdata_read),},
	{SEC_CMD("run_low_frequency_rawdata_read_all", run_low_frequency_rawdata_read_all),},
	{SEC_CMD("get_low_frequency_gap_data_all", get_low_frequency_gap_data_all),},
	{SEC_CMD("run_diffdata_read", run_diffdata_read),},
	{SEC_CMD("run_diffdata_read_all", run_diffdata_read_all),},
	{SEC_CMD("run_self_rawdata_tx_read", run_self_rawdata_tx_read),},
	{SEC_CMD("run_self_rawdata_tx_read_all", run_self_rawdata_tx_read_all),},
	{SEC_CMD("run_self_rawdata_rx_read", run_self_rawdata_rx_read),},
	{SEC_CMD("run_self_rawdata_rx_read_all", run_self_rawdata_rx_read_all),},
	{SEC_CMD("run_self_diffdata_read", run_self_diffdata_read),},
	{SEC_CMD("run_self_diffdata_read_all", run_self_diffdata_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_jitter_test", run_jitter_test),},
	{SEC_CMD("run_jitter_delta_test", run_jitter_delta_test),},
	{SEC_CMD("run_interrupt_gpio_test", run_interrupt_gpio_test),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
//	{SEC_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD_H("refresh_rate_mode", refresh_rate_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD_H("fod_enable", fod_enable),},
	{SEC_CMD("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
//	{SEC_CMD_H("external_noise_mode", external_noise_mode),},
//	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_cmd_result_all_imagetest", factory_cmd_result_all_imagetest),},
	{SEC_CMD_H("fix_active_mode", fix_active_mode),},
//	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD("low_sensitivity_mode_enable", low_sensitivity_mode_enable),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	{SEC_CMD_H("pocket_mode_state", pocket_mode_state),},
	{SEC_CMD_H("ear_detect_state", ear_detect_state),},
#endif
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);

	ts_info("%s: %d\n", __func__, core_data->plat_data->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", core_data->plat_data->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	ts_info("%s: %ld", __func__, data);

	core_data->plat_data->prox_power_off = data;

	return count;
}

/** virtual_prox **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);

	ts_info("hover = %d", core_data->ts_event.hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", core_data->ts_event.hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *core_data = container_of(sec, struct goodix_ts_core, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	ts_info("read parm = %d", data);

	if (data != 0 && data != 1) {
		ts_err("incorrect parm data(%d)", data);
		return -EINVAL;
	}

	core_data->plat_data->ed_enable = data;
	ts_info("ear detect mode(%d)", core_data->plat_data->ed_enable);

	if (core_data->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("IC is OFF");
		return count;
	}

	ret = core_data->hw_ops->ed_enable(core_data, core_data->plat_data->ed_enable);
	if (ret < 0)
		ts_err("send ear detect cmd failed");

	return count;
}

#define SENSITIVITY_POINT_CNT	9
static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *cd = container_of(sec, struct goodix_ts_core, sec);

	int value[SENSITIVITY_POINT_CNT];
	char result[SENSITIVITY_POINT_CNT * 10] = { 0 };
	char tempv[10] = {0};
	char buff[40] = {0};
	int ret, i;
	int retry = 20;
	unsigned int sen_addr = cd->production_test_addr;

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("power off in IC");
		return 0;
	}

	while (retry--) {
		ret = cd->hw_ops->read(cd, sen_addr, buff, 2);
		if (ret < 0) {
			ts_err("read sensitivity status failed");
			return ret;
		}

		if (buff[0] == 0xAA && buff[1] == 0xAA)
			break;
		ts_err("retry(%d), buff[0] = 0x%x,buff[1] = 0x%x\n", retry, buff[0], buff[1]);
		sec_delay(5);
	}
	if (retry < 0) {
		ts_err("sensitivity is not ready, status:0x%x, 0x%x", buff[0], buff[1]);
		return -EINVAL;
	}

	ret = cd->hw_ops->read(cd, sen_addr, buff, sizeof(buff));
	if (ret < 0) {
		ts_err("read sensitivity data failed");
		return ret;
	}

	if (checksum_cmp(buff + 2, sizeof(buff) - 2, CHECKSUM_MODE_U8_LE)) {
		ts_err("sensitivity data checksum error");
		ts_err("data:%*ph", (int)sizeof(buff) - 2, buff + 2);
		return -EINVAL;
	}

	memset(buff, 0, 2);
	cd->hw_ops->write(cd, sen_addr, buff, 2);

	for (i = 0 ; i < SENSITIVITY_POINT_CNT ; i++) {
		value[i] = (s16)le16_to_cpup((__le16 *)&buff[i * 4 + 2]);
		if (i != 0)
			strlcat(result, ",", sizeof(result));
		snprintf(tempv, 10, "%d", value[i]);
		strlcat(result, tempv, sizeof(result));
	}

	ts_info("sensitivity mode : %s", result);

	return snprintf(buf, PAGE_SIZE, "%s", result);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *cd = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	unsigned long value = 0;
	int ret = 0;
	unsigned char sen_cmd = cd->sensitive_cmd;

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("power off in IC");
		return 0;
	}

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (value == 1) {
		temp_cmd.len = 5;
		temp_cmd.cmd = sen_cmd;
		temp_cmd.data[0] = 1;
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0) {
			ts_err("send sensitivity mode on fail!");
			return ret;
		}
		goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);
		ts_info("enable end");
	} else {
		temp_cmd.len = 5;
		temp_cmd.cmd = sen_cmd;
		temp_cmd.data[0] = 0;
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0) {
			ts_err("send sensitivity mode off fail!");
			return ret;
		}
		goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
		ts_info("disable end");
	}

	ts_info("set sensitivity mode Done(%ld)\n", value);

	return count;
}

static ssize_t single_driving_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *cd = container_of(sec, struct goodix_ts_core, sec);
	struct goodix_ts_cmd temp_cmd;
	unsigned long value = 0;
	int ret = 0;

	if (cd->bus->ic_type != IC_TYPE_BERLIN_D) {
		ts_info("only support GT9895");
		return 0;
	}

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("power off in IC");
		return 0;
	}

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (value == 1) {
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x67;
		temp_cmd.data[0] = 1;
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0) {
			ts_err("enable single driving mode fail!");
			return ret;
		}
		ts_info("enable end");
	} else {
		goodix_ts_release_all_finger(cd);
		cd->hw_ops->reset(cd, 100);
		ts_info("disable end");
	}

	ts_info("single driving mode(%ld)", value);

	return count;
}

/*
 * read_support_feature function
 * returns the bit combination of specific feature that is supported.
 */
static ssize_t support_feature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	u32 feature = 0;

	if (ts->plat_data->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->plat_data->sync_reportrate_120)
		feature |= INPUT_FEATURE_ENABLE_SYNC_RR120;

	if (ts->plat_data->support_vrr)
		feature |= INPUT_FEATURE_ENABLE_VRR;

	if (ts->plat_data->support_open_short_test)
		feature |= INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST;

	if (ts->plat_data->support_mis_calibration_test)
		feature |= INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST;

	if (ts->plat_data->support_wireless_tx)
		feature |= INPUT_FEATURE_SUPPORT_WIRELESS_TX;

	if (ts->plat_data->support_input_monitor)
		feature |= INPUT_FEATURE_SUPPORT_INPUT_MONITOR;

	if (ts->plat_data->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	ts_info("%d%s%s%s%s%s%s%s%s%s",
			feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PRESSURE ? " pressure" : "",
			feature & INPUT_FEATURE_ENABLE_SYNC_RR120 ? " RR120hz" : "",
			feature & INPUT_FEATURE_ENABLE_VRR ? " vrr" : "",
			feature & INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST ? " openshort" : "",
			feature & INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST ? " miscal" : "",
			feature & INPUT_FEATURE_SUPPORT_WIRELESS_TX ? " wirelesstx" : "",
			feature & INPUT_FEATURE_SUPPORT_INPUT_MONITOR ? " inputmonitor" : "",
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t scrub_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *cd = container_of(sec, struct goodix_ts_core, sec);
	char buff[256] = { 0 };

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	ts_info("id: %d", cd->plat_data->gesture_id);
#else
	ts_info("id: %d, X:%d, Y:%d", cd->plat_data->gesture_id,
			cd->plat_data->gesture_x, cd->plat_data->gesture_y);
#endif
	snprintf(buff, sizeof(buff), "%d %d %d", cd->plat_data->gesture_id,
			cd->plat_data->gesture_x, cd->plat_data->gesture_y);

	cd->plat_data->gesture_x = 0;
	cd->plat_data->gesture_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t goodix_fod_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	u8 data[255] = { 0 };
	char buff[3] = { 0 };
	int i, ret;

	if (!ts->plat_data->support_fod) {
		ts_err("fod is not supported");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NA");
	}

	if (!ts->plat_data->fod_data.vi_size) {
		ts_err("not read fod_info yet");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ret = ts->hw_ops->read_from_sponge(ts, SEC_TS_CMD_SPONGE_FOD_POSITION, data, ts->plat_data->fod_data.vi_size);
	if (ret < 0) {
		ts_err("Failed to read");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	for (i = 0; i < ts->plat_data->fod_data.vi_size; i++) {
		snprintf(buff, 3, "%02X", data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t goodix_fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);

	return sec_input_get_fod_info(ts->bus->dev, buf);
}

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *cd = container_of(sec, struct goodix_ts_core, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u16 dump_start, dump_end, dump_cnt;
	int i, ret, dump_area, dump_gain;
	unsigned char *sec_spg_dat;

	if (cd->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		ts_err("Touch is stopped!");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	/* preparing dump buffer */
	sec_spg_dat = vzalloc(SEC_TS_MAX_SPONGE_DUMP_BUFFER);
	if (!sec_spg_dat)
		return snprintf(buf, SEC_CMD_BUF_SIZE, "vmalloc failed");

	cd->hw_ops->irq_enable(cd, false);

	ret = cd->hw_ops->read_from_sponge(cd, SEC_TS_CMD_SPONGE_LP_DUMP_CUR_IDX, string_data, 2);
	if (ret < 0) {
		ts_err("Failed to read lp dump cur idx");
		snprintf(buf, SEC_CMD_BUF_SIZE, "NG, Failed to read lp dump cur idx");
		goto out;
	}

	if (cd->sponge_inf_dump)
		dump_gain = 2;
	else
		dump_gain = 1;

	current_index = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
	dump_start = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
	dump_end = dump_start + (cd->sponge_dump_format * ((cd->sponge_dump_event * dump_gain) - 1));

	if (current_index > dump_end || current_index < dump_start) {
		ts_err("Failed to Sponge LP log %d", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	/* legacy get_lp_dump */
	ts_info("DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d",
			cd->sponge_dump_format, cd->sponge_dump_event, dump_start, dump_end, current_index);

	for (i = (cd->sponge_dump_event * dump_gain) - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (cd->sponge_dump_format * i))
			string_addr = (cd->sponge_dump_format * cd->sponge_dump_event * dump_gain)
					+ current_index - (cd->sponge_dump_format * i);
		else
			string_addr = current_index - (cd->sponge_dump_format * i);

		if (string_addr < dump_start)
			string_addr += (cd->sponge_dump_format * cd->sponge_dump_event * dump_gain);

		ret = cd->hw_ops->read_from_sponge(cd, string_addr, string_data, cd->sponge_dump_format);
		if (ret < 0) {
			ts_err("Failed to read sponge");
			snprintf(buf, SEC_CMD_BUF_SIZE,
					"NG, Failed to read sponge, addr=%d",
					string_addr);
			goto out;
		}

		data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
		data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
		data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
		data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);
		data4 = (string_data[9] & 0xFF) << 8 | (string_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (cd->sponge_dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3);
			}
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

	if (cd->sponge_inf_dump) {
		u16 addr = 0;
		u8 clear_data = 1;

		if (current_index >= cd->sponge_dump_border) {
			dump_cnt = ((current_index - (cd->sponge_dump_border)) / cd->sponge_dump_format) + 1;
			dump_area = 1;
			addr = cd->sponge_dump_border;
		} else {
			dump_cnt = ((current_index - SEC_TS_CMD_SPONGE_LP_DUMP_EVENT) / cd->sponge_dump_format) + 1;
			dump_area = 0;
			addr = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
		}

		ret = cd->hw_ops->read_from_sponge(cd, addr, sec_spg_dat, dump_cnt * cd->sponge_dump_format);
		if (ret < 0) {
			ts_err("Failed to read sponge");
			goto out;
		}

		for (i = 0 ; i <= dump_cnt ; i++) {
			int e_offset = i * cd->sponge_dump_format;
			char ibuff[30] = {0, };
			u16 edata[5];

			edata[0] = (sec_spg_dat[1 + e_offset] & 0xFF) << 8 | (sec_spg_dat[0 + e_offset] & 0xFF);
			edata[1] = (sec_spg_dat[3 + e_offset] & 0xFF) << 8 | (sec_spg_dat[2 + e_offset] & 0xFF);
			edata[2] = (sec_spg_dat[5 + e_offset] & 0xFF) << 8 | (sec_spg_dat[4 + e_offset] & 0xFF);
			edata[3] = (sec_spg_dat[7 + e_offset] & 0xFF) << 8 | (sec_spg_dat[6 + e_offset] & 0xFF);
			edata[4] = (sec_spg_dat[9 + e_offset] & 0xFF) << 8 | (sec_spg_dat[8 + e_offset] & 0xFF);

			if (edata[0] || edata[1] || edata[2] || edata[3] || edata[4]) {
				snprintf(ibuff, sizeof(ibuff), "%03d: %04x%04x%04x%04x%04x\n",
						i + (cd->sponge_dump_event * dump_area),
						edata[0], edata[1], edata[2], edata[3], edata[4]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
				sec_tsp_sponge_log(ibuff);
#endif
			}
		}

		cd->sponge_dump_delayed_flag = false;
		ret = cd->hw_ops->write_to_sponge(cd, SEC_TS_CMD_SPONGE_DUMP_FLUSH, &clear_data, 1);
		if (ret < 0)
			ts_err("Failed to clear sponge dump");
	}
out:
	vfree(sec_spg_dat);
	cd->hw_ops->irq_enable(cd, true);
	return strlen(buf);
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);

	if (!ts->plat_data->enable_sysinput_enabled)
		return -EINVAL;

	ts_info("%d", ts->plat_data->enabled);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->plat_data->enabled);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct goodix_ts_core *ts = container_of(sec, struct goodix_ts_core, sec);
	struct input_dev *input_dev = ts->plat_data->input_dev;
	int buff[2];
	int ret;

	if (!ts->plat_data->enable_sysinput_enabled)
		return -EINVAL;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		ts_err("failed read params [%d]\n", ret);
		return -EINVAL;
	}

	if (buff[0] == DISPLAY_STATE_ON && buff[1] == DISPLAY_EVENT_LATE) {
		if (ts->plat_data->enabled) {
			ts_info("device already enabled\n");
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_OFF && buff[1] == DISPLAY_EVENT_EARLY) {
		if (!ts->plat_data->enabled) {
			ts_info("device already disabled\n");
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_FORCE_ON) {
		if (ts->plat_data->enabled) {
			ts_info("device already enabled\n");
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
		ts_info("DISPLAY_STATE_FORCE_ON(%d)\n", ret);
	} else if (buff[0] == DISPLAY_STATE_FORCE_OFF) {
		if (!ts->plat_data->enabled) {
			ts_info("device already disabled\n");
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
		ts_info("DISPLAY_STATE_FORCE_OFF(%d)\n", ret);
	}

	if (ret)
		return ret;

out:
	return count;
}

static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(single_driving, 0220, NULL, single_driving_store);
static DEVICE_ATTR(support_feature, 0444, support_feature_show, NULL);
static DEVICE_ATTR(scrub_pos, 0444, scrub_pos_show, NULL);
static DEVICE_ATTR(fod_pos, 0444, goodix_fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, goodix_fod_info_show, NULL);
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);

static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
//	&dev_attr_hw_param.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_single_driving.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
//	&dev_attr_aod_active_area.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int goodix_ts_cmd_init(struct goodix_ts_core *ts)
{
	int retval = 0;

	retval = sec_cmd_init(&ts->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		ts_err("Failed to sec_cmd_init");
		goto exit;
	}

	retval = sysfs_create_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		ts_err("Failed to create sysfs attributes");
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts->sec.fac_dev->kobj,
			&ts->input_dev->dev.kobj, "input");
	if (retval < 0) {
		ts_err("Failed to create input symbolic link");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sec_input_sysfs_create(&ts->plat_data->input_dev->dev.kobj);
	if (retval < 0) {
		ts_err("Failed to create sec_input_sysfs attributes");
		sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	return 0;

exit:
	return retval;
}

void goodix_ts_cmd_remove(struct goodix_ts_core *ts)
{
	ts_info("called");

	sec_input_sysfs_remove(&ts->plat_data->input_dev->dev.kobj);

	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}

