/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "focaltech_core.h"
#include "focaltech_pramtest_ft3519.h"

enum ALL_NODE_TEST_TYPE {
	ALL_NODE_TYPE_SHORT,
	ALL_NODE_TYPE_SCAP_CB,
	ALL_NODE_TYPE_SCAP_RAWDATA,
	ALL_NODE_TYPE_RAWDATA,
	ALL_NODE_TYPE_PANEL_DIFFER,
	ALL_NODE_TYPE_NOISE,
	ALL_NODE_TYPE_PRAM,
};

static int fts_wait_test_done(u8 cmd, u8 mode, int delayms)
{
	int i, ret;
	u8 rbuff;
	u8 read_addr = 0;
	u8 status_done = 0;
	ktime_t start_time = ktime_get();

	switch (cmd) {
	case DEVICE_MODE_ADDR:
		read_addr = cmd;
		if (mode == FTS_REG_WORKMODE)
			status_done = FTS_REG_WORKMODE;
		else
			status_done = FTS_REG_WORKMODE_FACTORY_VALUE;
		break;
	case FACTORY_REG_SHORT_TEST_EN:
		read_addr = FACTORY_REG_SHORT_TEST_STATE;
		status_done = TEST_RETVAL_AA;
		break;
	case FACTORY_REG_OPEN_START:
		read_addr = FACTORY_REG_OPEN_STATE;
		status_done = TEST_RETVAL_AA;
		break;
	case FACTORY_REG_CLB:
		read_addr = FACTORY_REG_CLB;
		status_done = FACTORY_REG_CLB_DONE;
		break;
	case FACTORY_REG_LCD_NOISE_START:
		read_addr = FACTORY_REG_LCD_NOISE_TEST_STATE;
		status_done = TEST_RETVAL_AA;
		break;

	case FACTROY_REG_SHORT_TEST_EN:
		read_addr = FACTROY_REG_SHORT_TEST_EN;
		status_done = TEST_RETVAL_00;
		break;
	}

	FTS_INFO("write: 0x%02X 0x%02X, need reply: 0x%02X 0x%02X", cmd, mode, read_addr, status_done);

	ret = fts_write_reg(cmd, mode);
	if (ret < 0) {
		FTS_ERROR("failed to write 0x%02X to cmd 0x%02X", mode, cmd);
		return ret;
	}

	sec_delay(delayms);

	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		ret = fts_read_reg(read_addr, &rbuff);
		if (ret < 0) {
			FTS_ERROR("failed to read 0x%02X", read_addr);
			return ret;
		}

		if (rbuff == status_done) {
			FTS_INFO("read 0x%02X and get 0x%02X success, %lldms taken",
					read_addr, status_done, ktime_ms_delta(ktime_get(), start_time));
			return 0;
		}

		sec_delay(FACTORY_TEST_DELAY);
	}

	FTS_ERROR("[TIMEOUT %lldms] read 0x%02X but failed to get 0x%02X, current:0x%02X",
			ktime_ms_delta(ktime_get(), start_time), read_addr, status_done, rbuff);
	return -EIO;
}

static int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;

	FTS_FUNC_ENTER();

	ret = fts_read_reg(DEVICE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE))
		return 0;

	return fts_wait_test_done(DEVICE_MODE_ADDR, FTS_REG_WORKMODE, 200);
}

static int enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;

	FTS_FUNC_ENTER();

	ret = fts_read_reg(DEVICE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE_FACTORY_VALUE))
		return 0;

	return fts_wait_test_done(DEVICE_MODE_ADDR, FTS_REG_WORKMODE_FACTORY_VALUE, 200);
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[256] = { 0 };

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	FTS_INFO("id: %d", ts_data->pdata->gesture_id);
#else
	FTS_INFO("id: %d, X:%d, Y:%d", ts_data->pdata->gesture_id, ts_data->pdata->gesture_x, ts_data->pdata->gesture_y);
#endif
	snprintf(buff, sizeof(buff), "%d %d %d", ts_data->pdata->gesture_id,
			ts_data->pdata->gesture_x, ts_data->pdata->gesture_y);

	ts_data->pdata->gesture_x = 0;
	ts_data->pdata->gesture_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;
	u8 cmd = FTS_REG_SENSITIVITY_VALUE;
	u8 data[18] = { 0 };
	short diff[9] = { 0 };
	char buff[10 * 9] = { 0 };

	ret = fts_write_reg(FTS_REG_SNR_BUFF_COUNTER, 0);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
	}

	ret = fts_read(&cmd, 1, data, 18);
	if (ret < 0)
		FTS_ERROR("read sensitivity data failed");

	for (i = 0; i < 9; i++)
		diff[i] = (data[2 * i] << 8) + data[2 * i + 1];

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
			diff[0], diff[1], diff[2],
			diff[3], diff[4], diff[5],
			diff[6], diff[7], diff[8]);
	FTS_INFO("%s", buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int ret;
	u8 enable;
	int retry = 3;

	ret = kstrtoint(buf, 10, &data);
	if (ret < 0)
		return ret;

	if (data != 0 && data != 1) {
		FTS_ERROR("invalid param %d", data);
		return -EINVAL;
	}

	enable = data & 0xFF;

	if (enable) {
		ret = fts_write_reg(FTS_REG_CODE_BANK_MODE, 0x1);
		if (ret < 0) {
			FTS_ERROR("failed to write FTS_REG_CODE_BANK_MODE, ret=%d", ret);
			goto out;
		}
		sec_delay(50);

		ret = fts_write_reg(FTS_REG_BANK_MODE, 0x0);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode %d, ret=%d", data, ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_POWER_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write power mode 0x00, ret=%d", ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_SENSITIVITY_MODE, 0x01);
		if (ret < 0) {
			FTS_ERROR("failed to write sensitivity mode %d, ret=%d", data, ret);
			goto out;
		}
	} else {
		ret = fts_write_reg(FTS_REG_SENSITIVITY_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write sensitivity mode %d, ret=%d", data, ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_CODE_BANK_MODE, 0x0);
		if (ret < 0) {
			FTS_ERROR("failed to write FTS_REG_CODE_BANK_MODE, ret=%d", ret);
			goto out;
		}
		sec_delay(50);
	}

out:
	FTS_INFO("%s %s", data ? "on" : "off", (ret < 0) ? "fail" : "success");

	if ((ret < 0) && (retry > 0)) {
		retry--;

		ret = fts_write_reg(FTS_REG_SENSITIVITY_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write sensitivity mode %d, ret=%d", data, ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_CODE_BANK_MODE, 0x0);
		if (ret < 0) {
			FTS_ERROR("failed to write FTS_REG_CODE_BANK_MODE, ret=%d", ret);
			goto out;
		}
		sec_delay(50);
	}

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("%d", ts->pdata->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->pdata->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	int data;
	int ret;

	ret = kstrtoint(buf, 10, &data);
	if (ret < 0)
		return ret;

	ts->pdata->prox_power_off = data;

	FTS_INFO("%d", ts->pdata->prox_power_off);

	return count;
}

static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("hover = %d", ts_data->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts_data->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	FTS_INFO("read parm = %d", data);

	if (data != 0 && data != 1) {
		FTS_ERROR("incorrect parm data(%d)", data);
		return -EINVAL;
	}

	ts_data->pdata->ed_enable = data;
	FTS_INFO("ear detect mode(%d)", ts_data->pdata->ed_enable);

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		return count;
	}

	ret = fts_write_reg(FTS_REG_PROXIMITY_MODE, ts_data->pdata->ed_enable);
	if (ret < 0) {
		FTS_ERROR("send ear detect cmd failed");
	}

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	u32 feature = 0;

	if (ts->pdata->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->pdata->sync_reportrate_120)
		feature |= INPUT_FEATURE_ENABLE_SYNC_RR120;

	if (ts->pdata->support_vrr)
		feature |= INPUT_FEATURE_ENABLE_VRR;

	if (ts->pdata->support_open_short_test)
		feature |= INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST;

	if (ts->pdata->support_mis_calibration_test)
		feature |= INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST;

	if (ts->pdata->support_wireless_tx)
		feature |= INPUT_FEATURE_SUPPORT_WIRELESS_TX;

	if (ts->pdata->support_input_monitor)
		feature |= INPUT_FEATURE_SUPPORT_INPUT_MONITOR;

	if (ts->pdata->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	FTS_INFO("%d%s%s%s%s%s%s%s%s%s",
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

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("%d", ts_data->pdata->enabled);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts_data->pdata->enabled);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	struct input_dev *input_dev = ts->pdata->input_dev;
	int buff[2];
	int ret;

	if (!ts->pdata->enable_sysinput_enabled)
		return count;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		FTS_ERROR("failed read params [%d]", ret);
		return -EINVAL;
	}

	if (buff[0] == DISPLAY_STATE_ON && buff[1] == DISPLAY_EVENT_LATE) {
		if (ts->pdata->enabled) {
			FTS_INFO("device already enabled");
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_OFF && buff[1] == DISPLAY_EVENT_EARLY) {
		if (!ts->pdata->enabled) {
			FTS_INFO("device already disabled");
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
	} else if (buff[0] == DISPLAY_STATE_FORCE_ON) {
		if (ts->pdata->enabled) {
			FTS_INFO("device already enabled");
			goto out;
		}

		ret = sec_input_enable_device(input_dev);
		FTS_INFO("DISPLAY_STATE_FORCE_ON(%d)", ret);
	} else if (buff[0] == DISPLAY_STATE_FORCE_OFF) {
		if (!ts->pdata->enabled) {
			FTS_INFO("device already disabled");
			goto out;
		}

		ret = sec_input_disable_device(input_dev);
		FTS_INFO("DISPLAY_STATE_FORCE_OFF(%d)", ret);
	}

	if (ret)
		return ret;

out:
	return count;

}

static ssize_t fod_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[3] = { 0 };
	int i, ret;

	if (!ts_data->pdata->support_fod) {
		FTS_ERROR("fod is not supported");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NA");
	}

	if (!ts_data->pdata->fod_data.vi_size) {
		FTS_ERROR("not read fod_info yet");
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ts_data->pdata->fod_data.vi_data[0] = FOD_VI_DATA;
	ret = fts_read(ts_data->pdata->fod_data.vi_data, 1, ts_data->pdata->fod_data.vi_data, ts_data->pdata->fod_data.vi_size);
	if (ret < 0) {
		FTS_ERROR("read fod vi data fail");
		return ret;
	}

	for (i = 0; i < ts_data->pdata->fod_data.vi_size; i++) {
		snprintf(buff, 3, "%02X", ts_data->pdata->fod_data.vi_data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	return sec_input_get_fod_info(ts_data->dev, buf);
}

static int fts_burst_read(u8 addr, u8 *buf, int len)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = len;
	int byte_per_time = 128;

	packet_num = byte_num / byte_per_time;
	packet_remainder = byte_num % byte_per_time;
	if (packet_remainder)
		packet_num++;

	if (byte_num < byte_per_time) {
		packet_length = byte_num;
	} else {
		packet_length = byte_per_time;
	}
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	ret = fts_read(&addr, 1, &buf[offset], packet_length);
	if (ret < 0) {
		FTS_ERROR("read buffer fail");
		return ret;
	}
	for (i = 1; i < packet_num; i++) {
		offset += packet_length;
		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}

		ret = fts_read(NULL, 0, &buf[offset], packet_length);
		if (ret < 0) {
			FTS_ERROR("read buffer fail");
			return ret;
		}
	}

	return 0;
}

static ssize_t get_lp_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	int ret = 0;
	int count = 0;
	int start_index = 0;
	u8 event_num = 0;
	u8 data_buf[512] = {0};
	u8 *frame_data = NULL;

	ret = fts_burst_read(0xD9, data_buf, 502);
	if (ret < 0) {
		FTS_ERROR("Failed to read lp dump");
		count += snprintf(buf + count, PAGE_SIZE, "NG, Failed to read lp dump\n");
		goto out;
	}

	event_num = data_buf[0];
	start_index = data_buf[1];
	if (start_index > 100) {
		count += snprintf(buf + count, PAGE_SIZE, "no event\n");
		goto out;
	}

	frame_data = data_buf + 2;
	count += snprintf(buf + count, PAGE_SIZE, "The number of event is %d, start index %d\n", event_num, start_index);
	for (i = 0; i < 100; i++) {
		if (start_index < 0)
			start_index = 99;

		count += snprintf(buf + count, PAGE_SIZE, "%03d : %02X%02X%02X%02X%02X\n", i,
							frame_data[start_index * 5],
							frame_data[start_index * 5 + 1],
							frame_data[start_index * 5 + 2],
							frame_data[start_index * 5 + 3],
							frame_data[start_index * 5 + 4]);
		start_index--;
	}

out:
	return count;
}

static DEVICE_ATTR(scrub_pos, 0444, scrub_position_show, NULL);
static DEVICE_ATTR(sensitivity_mode, 0644, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0644, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);
static DEVICE_ATTR(fod_pos, 0444, fod_pos_show, NULL);
static DEVICE_ATTR(fod_info, 0444, fod_info_show, NULL);
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_enabled.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_get_lp_dump.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};


static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, update_type;

	sec_cmd_set_default_result(sec);

	if (ts_data->suspended) {
		FTS_ERROR("IC is suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	fts_release_all_finger();

	update_type = sec->cmd_param[0];

	switch (update_type) {
	case TSP_SDCARD:
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		update_type = TSP_SIGNED_SDCARD;
#endif
	case TSP_BUILT_IN:
	case TSP_SPU:
	case TSP_VERIFICATION:
		ret = fts_fwupg_func(update_type, true);
		break;
	default:
		ret = -EINVAL;
		FTS_ERROR("not supported %d", update_type);
		break;
	}

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		fts_wait_tp_to_valid();
		fts_ex_mode_recovery(ts_data);

		if (ts_data->gesture_mode || ts_data->spay_enable) {
			fts_gesture_resume(ts_data);
		}

		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	FTS_INFO("%s", buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char model[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ret = fts_get_fw_ver(ts_data);
	if (ret < 0) {
		FTS_ERROR("failed to read version, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "FT%02X%02X%02X%02X",
			ts_data->ic_fw_ver.ic_name,
			ts_data->ic_fw_ver.project_name,
			ts_data->ic_fw_ver.module_id,
			ts_data->ic_fw_ver.fw_ver);
	snprintf(model, sizeof(model), "FT%02X%02X",
			ts_data->ic_fw_ver.ic_name,
			ts_data->ic_fw_ver.project_name);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "FT%02X%02X%02X%02X",
			ts_data->bin_fw_ver.ic_name,
			ts_data->bin_fw_ver.project_name,
			ts_data->bin_fw_ver.module_id,
			ts_data->bin_fw_ver.fw_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "FOCALTECH");
	FTS_INFO("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 rbuff, rbuff2;
	rbuff = rbuff2 = 0;

	sec_cmd_set_default_result(sec);

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FTS_REG_SEC_CHIP_NAME_H, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read CHIP NAME H, ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FTS_REG_SEC_CHIP_NAME_L, &rbuff2);
	if (ret < 0) {
		FTS_ERROR("failed to read CHIP NAME L, ret=%d", ret);
		goto out;
	}

out:
	snprintf(buff, sizeof(buff), "FT%02X%02X", rbuff, rbuff2);
	FTS_INFO("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts_data->tx_num);
	FTS_INFO("%s", buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}
static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts_data->rx_num);
	FTS_INFO("%s", buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void fts_print_frame(struct fts_ts_data *ts, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize = 10 * (ts->tx_num + 1);

	if (ts->rx_num > ts->tx_num)
		lsize = 10 * (ts->rx_num + 1);

	FTS_FUNC_ENTER();

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      RX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->rx_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}
	FTS_RAW_INFO("%s", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->rx_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	FTS_RAW_INFO("%s", pStr);

	*min = *max = ts->pFrame[0];

	for (i = 0; i < ts->tx_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->rx_num; j++) {
			snprintf(pTmp, sizeof(pTmp), " %3d", ts->pFrame[(i * ts->rx_num) + j]);

			if (ts->pFrame[(i * ts->rx_num) + j] < *min)
				*min = ts->pFrame[(i * ts->rx_num) + j];

			if (ts->pFrame[(i * ts->rx_num) + j] > *max)
				*max = ts->pFrame[(i * ts->rx_num) + j];

			strlcat(pStr, pTmp, lsize);
		}
		FTS_RAW_INFO("%s", pStr);
	}
	kfree(pStr);

	FTS_RAW_INFO("min:%d, max:%d", *min, *max);
}

static void fts_print_scap_frame(int *data, int *min, int *max)
{
	int i = 0;
	int node_num = 0;
	char *pStr = NULL;
	int size = 0;
	int cnt = 0;
	struct fts_ts_data *ts = fts_data;

	size = (ts->rx_num + ts->tx_num) * 10;
	pStr = kzalloc(size, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, size);
	node_num = ts->rx_num + ts->tx_num;
	*min = *max = data[0];
	FTS_RAW_INFO("Rx: ");
	for (i = 0; i < ts->rx_num; i++) {
		cnt += sprintf(pStr + cnt, "%6d, ", data[i]);
		if(*min > data[i]) {
			*min = data[i];
		}

		if(*max < data[i]) {
			*max = data[i];
		}
	}
	FTS_RAW_INFO("%s", pStr);

	FTS_RAW_INFO("Tx: ");
	cnt = 0;
	memset(pStr, 0x0, size);
	for (i = ts->rx_num; i < node_num; i++) {
		cnt += sprintf(pStr + cnt, "%6d, ", data[i]);
		if(*min > data[i]) {
			*min = data[i];
		}

		if(*max < data[i]) {
			*max = data[i];
		}
	}
	FTS_RAW_INFO("%s", pStr);
	FTS_RAW_INFO("min:%d, max:%d", *min, *max);
	kfree(pStr);
}

int fts_test_read(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME) {
		packet_length = byte_num;
	} else {
		packet_length = BYTES_PER_TIME;
	}
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
	if (ret < 0) {
		FTS_ERROR("read buffer fail");
		return ret;
	}
	for (i = 1; i < packet_num; i++) {
		offset += packet_length;
		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}

		ret = fts_read(NULL, 0, &readbuf[offset], packet_length);
		if (ret < 0) {
			FTS_ERROR("read buffer fail");
			return ret;
		}
	}

	return 0;
}


int read_mass_data(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	data = (u8 *)kzalloc(byte_num * sizeof(u8), GFP_KERNEL);
	if (NULL == data) {
		FTS_ERROR("mass data buffer malloc fail");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	FTS_INFO("mass data len:%d", byte_num);
	ret = fts_test_read(addr, data, byte_num);
	if (ret < 0) {
		FTS_ERROR("read mass data fail");
		goto read_massdata_err;
	}

	for (i = 0; i < byte_num; i = i + 2) {
		buf[i >> 1] = (int)(short)((data[i] << 8) + data[i + 1]);
	}

	ret = 0;
read_massdata_err:
	if (data)
		kfree(data);
	return ret;
}


static int fts_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

int get_cb_sc(u8 wp, int byte_num, int *cb_buf)
{
	int ret = 0;
	int i = 0;
	int offset = 0;
	uint8_t cb_addr = FACTORY_REG_MC_SC_CB_ADDR;
	uint8_t off_addr = FACTORY_REG_MC_SC_CB_ADDR_OFF;
	uint8_t *cb = NULL;

	cb = (uint8_t *)kzalloc(byte_num * sizeof(uint8_t), GFP_KERNEL);
	if (NULL == cb) {
		FTS_ERROR("malloc memory for cb buffer fail");
		return -1;
	}

	ret = fts_write_reg(FACTORY_REG_MC_SC_MODE, wp);
	if (ret < 0) {
		FTS_ERROR("get mc_sc mode fail");
		goto cb_err;
	}

	ret = fts_wait_test_done(0x00, 0xc0, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto cb_err;
	}

	ret = fts_write_reg(off_addr, offset);
	if (ret < 0) {
		FTS_ERROR("write cb addr offset fail");
		goto cb_err;
	}
	
	fts_read(&cb_addr, 1, cb, byte_num);

	for (i = 0; i < byte_num; i = i + 2) {
		cb_buf[i >> 1] = (int)(((int)(cb[i]) << 8) + cb[i + 1]);
	}

	ret = 0;
cb_err:
	if (cb)
		kfree(cb);
	return ret;
}

static void get_short(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, i;
	int min_value, max_value;
	u8 short_delay = 0;
	u16 offset_val = 0;
	u8 offset_buf[2] = {0};
	int denominator = 0;
	int code = 0;
	int code_1 = 1437;
	int min_cc = 1200;
	int numerator = 0;
	int ch_num = 0;
	int *adc_data = NULL;
	int *short_data = NULL;
	u8 cmd = 0;
	int result = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}
	ch_num = ts_data->tx_num + ts_data->rx_num;
	memset(ts_data->pFrame, 0x00, ts_data->pFrame_size);
	adc_data = ts_data->pFrame;
	short_data = kzalloc(ch_num * sizeof(int), GFP_KERNEL);
	ret = fts_read_reg(FACTROY_REG_SHORT_RES_LEVEL, &short_delay);
	if (ret < 0) {
		FTS_ERROR("read 5A fails");
		goto out;
	}

	ret = fts_wait_test_done(FACTROY_REG_SHORT_TEST_EN, FACTROY_REG_SHORT_OFFSET, 500);
	if (ret < 0) {
		FTS_ERROR("failed to read offset");
		goto out;
	}

	cmd = FACTORY_REG_SHORT_ADDR_MC;
	ret = fts_read(&cmd, 1, offset_buf, 2);
	if (ret < 0) {
		FTS_ERROR("write short test mode fail");
		goto out;
	}

	offset_val = (offset_buf[0] << 8) + offset_buf[1];
	offset_val -= 1024;

	ret = fts_write_reg(FACTROY_REG_SHORT_RES_LEVEL, 0x01);
	if (ret < 0) {
		FTS_ERROR("write 5A fails,ret:%d", ret);
		goto out;
	}

	ret = fts_wait_test_done(FACTROY_REG_SHORT_TEST_EN, FACTROY_REG_SHORT_CA, 500);
	if (ret < 0) {
		FTS_ERROR("failed to do short test");
		goto out;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR_MC, (ch_num + 1) * 2, adc_data);
	if (ret < 0) {
		FTS_ERROR("get short(adc) data fail");
	}

	/* calculate resistor */
	for (i = 0; i < ch_num; i++) {
		code = adc_data[i];
		denominator = code_1 - code;
		if (denominator <= 0) {
			short_data[i] = min_cc;
		} else {
			numerator = (code - offset_val + 395) * 111;
			short_data[i] = fts_abs(numerator / denominator - 3);
		}
	}

	FTS_RAW_INFO("%s", __func__);
	fts_print_scap_frame(adc_data, &min_value, &max_value);
	fts_print_scap_frame(short_data, &min_value, &max_value);

	if (min_value >= min_cc) {
		result = 1;
		FTS_INFO("Short test result %d, PASS", result);
	} else {
		result = 0;
		FTS_INFO("Short test result %d, NG", result);
	}

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d", result);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SHORT");
	FTS_INFO("%s", buff);
	if (short_data)
		kfree(short_data);
}

static void get_scap_cb(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	int min_value, max_value;

	u8 sc_mode = 0;
	int byte_num = 0;
	int *scap_cb = NULL;
	int *scb_tmp = NULL;
	int scb_cnt = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}
	
	memset(ts_data->pFrame, 0x00, ts_data->pFrame_size);
	scap_cb = ts_data->pFrame;
	byte_num = (ts_data->tx_num + ts_data->rx_num) * 2;

	ret = fts_read_reg(FACTORY_REG_MC_SC_MODE, &sc_mode);
	if (ret < 0) {
		FTS_ERROR("read sc_mode fail,ret=%d", ret);
		goto out;
	}

	/* water proof on check */
	scb_tmp = scap_cb + scb_cnt;
	/* 1:waterproof 0:non-waterproof */
	ret = get_cb_sc(1, byte_num, scb_tmp);
	if (ret < 0) {
		FTS_ERROR("read sc_cb fail,ret=%d", ret);
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	/* show Scap CB */
	FTS_RAW_INFO("scap_cb in waterproof on mode:");
	fts_print_scap_frame(scb_tmp, &min_value, &max_value);
	
out_testmode:
	if (fts_write_reg(0x44, 0x01) < 0) {
		FTS_ERROR("failed to disable cb test");
		ret = -EIO;
	}

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SCAP_CB");
	FTS_INFO("%s", buff);
}

static void get_scap_rawdata(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	int min_value, max_value;
	
	int byte_num = 0;
	int *scap_rwadata = NULL;
	int *sraw_tmp = NULL;
	int scb_cnt = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}
	
	memset(ts_data->pFrame, 0x00, ts_data->pFrame_size);
	scap_rwadata = ts_data->pFrame;

	ret = fts_wait_test_done(DEVICE_MODE_ADDR, FACTORY_REG_START_SCAN, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto out;
	}

	/* water proof on check */
	sraw_tmp = scap_rwadata + scb_cnt;
	byte_num = (ts_data->tx_num + ts_data->rx_num) * 2;
	/* 1:waterproof 0:non-waterproof */
	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, 0xAC);
	if (ret < 0) {
		FTS_ERROR("wirte line/start addr fail");
		goto out;
	}
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR_MC_SC,byte_num, sraw_tmp);
	if (ret < 0) {
		FTS_ERROR("read sc_rawdata fail,ret=%d", ret);
		goto out;
	}

	FTS_RAW_INFO("%s", __func__);
	/* show Scap rawdata */
	FTS_RAW_INFO("scap_cb in waterproof on mode:");
	fts_print_scap_frame(sraw_tmp, &min_value, &max_value);
out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SCAP_RAWDATA");
	FTS_INFO("%s", buff);
}


static void get_rawdata(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	int byte_num = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	byte_num = 2 * (ts_data->rx_num * ts_data->tx_num);
	ret = fts_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_ERROR("read normalize fail,ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret < 0) {
		FTS_ERROR("read 0x0A fail,ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FACTORY_REG_FIR, &fir);
	if (ret < 0) {
		FTS_ERROR("read 0xFB error,ret=%d", ret);
		goto out;
	}

	sec_delay(100);

	ret = fts_wait_test_done(DEVICE_MODE_ADDR, FACTORY_REG_START_SCAN, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, FACTORY_REG_RAWADDR_SET);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata addr setting");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR_MC_SC, byte_num, ts_data->pFrame);
	if (ret < 0) {
		FTS_INFO("failed to read rawdata");
		goto out_testmode;
	}

	fts_print_frame(ts_data, &min_value, &max_value);

out_testmode:
	ret = fts_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_ERROR("restore normalize fail,ret=%d", ret);
	}

	ret = fts_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_ERROR("restore 0x0A fail,ret=%d", ret);
	}

	ret = fts_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_ERROR("restore 0xFB fail,ret=%d", ret);
	}

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "RAWDATA");
	FTS_INFO("%s", buff);
}

static void get_panel_differ(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	int byte_num = 0;
	int i = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	byte_num = 2 * (ts_data->rx_num * ts_data->tx_num);
	/* save origin value */
	ret = fts_read_reg(FACTORY_REG_NORMALIZE, &normalize);
	if (ret < 0) {
		FTS_ERROR("read normalize fail,ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FACTORY_REG_FRE_LIST, &fre);
	if (ret < 0) {
		FTS_ERROR("read 0x0A fail,ret=%d", ret);
		goto out;
	}

	ret = fts_read_reg(FACTORY_REG_FIR, &fir);
	if (ret < 0) {
		FTS_ERROR("read 0xFB fail,ret=%d", ret);
		goto out;
	}

	/* set to overall normalize */
	if (normalize != 0) {
		ret = fts_write_reg(FACTORY_REG_NORMALIZE, 0);
		if (ret < 0) {
			FTS_ERROR("write normalize fail,ret=%d", ret);
			goto out_testmode;
		}
	}

	/* set frequecy high */
	ret = fts_write_reg(FACTORY_REG_FRE_LIST, 0x81);
	if (ret < 0) {
		FTS_ERROR("set frequecy fail,ret=%d", ret);
		goto out_testmode;
	}

	/* fir disable */
	ret = fts_write_reg(FACTORY_REG_FIR, 0);
	if (ret < 0) {
		FTS_ERROR("set fir fail,ret=%d", ret);
		goto out_testmode;
	}

	sec_delay(100);

	ret = fts_wait_test_done(DEVICE_MODE_ADDR, FACTORY_REG_START_SCAN, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, FACTORY_REG_RAWADDR_SET);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata addr setting");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR_MC_SC, byte_num, ts_data->pFrame);
	if (ret < 0) {
		FTS_INFO("failed to read rawdata");
		goto out_testmode;
	}

	for (i = 0; i < ts_data->rx_num * ts_data->tx_num; i++) {
		ts_data->pFrame[i] = ts_data->pFrame[i] / 10;
	}

	fts_print_frame(ts_data, &min_value, &max_value);

out_testmode:
	ret = fts_write_reg(FACTORY_REG_NORMALIZE, normalize);
	if (ret < 0) {
		FTS_ERROR("restore normalize fail,ret=%d", ret);
	}

	ret = fts_write_reg(FACTORY_REG_FRE_LIST, fre);
	if (ret < 0) {
		FTS_ERROR("restore 0x0A fail,ret=%d", ret);
	}

	ret = fts_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_ERROR("restore 0xFB fail,ret=%d", ret);
	}

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "PANEL_DIFFER");
	FTS_INFO("%s", buff);
}


static void get_noise(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;
	u8 reg06_val = 0;
	u8 fir = 0;
	int byte_num = 0;
	min_value = max_value = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	byte_num = 2 * (ts_data->rx_num * ts_data->tx_num);
	/* save origin value */
	ret = fts_read_reg(FACTORY_REG_DATA_SELECT, &reg06_val);
	if (ret < 0) {
		FTS_ERROR("read reg06 fail,ret=%d", ret);
		goto out;
	}
	FTS_INFO("reg06_val = [%d]", reg06_val);

	ret = fts_read_reg(FACTORY_REG_FIR, &fir);
	if (ret < 0) {
		FTS_ERROR("read fir error,ret=%d", ret);
		goto out;
	}
	FTS_INFO("fir = [%d]", fir);

	ret = fts_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_ERROR("set reg06 fail,ret=%d", ret);
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_FIR, 0x01);
	if (ret < 0) {
		FTS_ERROR("set fir fail,ret=%d", ret);
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_FRAME_NUM, 0x64);
	if (ret < 0) {
		FTS_ERROR("set frame fail,ret=%d", ret);
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_MAX_DIFF, 0x01);
	if (ret < 0) {
		FTS_ERROR("write 0x1B fail,ret=%d", ret);
		goto out_testmode;
	}
	ret = fts_wait_test_done(DEVICE_MODE_ADDR, FACTORY_REG_START_SCAN, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, FACTORY_REG_RAWADDR_SET);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata addr setting");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = read_mass_data(FACTORY_REG_RAWDATA_ADDR_MC_SC, byte_num, ts_data->pFrame);
	if (ret < 0) {
		FTS_ERROR("failed to read noise");
		goto out_testmode;
	}

	fts_print_frame(ts_data, &min_value, &max_value);

out_testmode:
	/* set the origin value */
	ret = fts_write_reg(FACTORY_REG_DATA_SELECT, reg06_val);
	if (ret < 0) {
		FTS_ERROR("restore normalize fail,ret=%d", ret);
	}

	ret = fts_write_reg(FACTORY_REG_FIR, fir);
	if (ret < 0) {
		FTS_ERROR("restore 0xFB fail,ret=%d", ret);
	}

	ret = fts_write_reg(0x1b, 0x0);
	if (ret < 0) {
		FTS_ERROR("restore 0x1b fail,ret=%d", ret);
	}
out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE");
	FTS_INFO("%s", buff);
}

extern int ft3519_pram_test_write_buf(u8 *buf, u32 len);
extern int ft3519_pram_test_ecc_cal(u32 saddr, u32 len);
extern int fts_fwupg_reset_in_boot(void);

static void get_pram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	int ecc_host = 0;
	int ecc_tp = 0;
	u8 cmd[4] = { 0 };
	u8 val[5] = { 0 };
	u8 *pb_test_buf = ft3519_pram_test_file;
	u32 pb_test_len = sizeof(ft3519_pram_test_file);
	int result = 0;
	u8 mode_temp = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	ret = fts_read_reg(DEVICE_MODE_ADDR, &mode_temp);
	if (ret < 0) {
		FTS_ERROR("failed to read mode,ret=%d", ret);
		goto out;
	}

	fts_irq_disable();

	ret = enter_work_mode();
	if (ret < 0) {
		FTS_ERROR("failed to enter work mode,ret=%d", ret);
		goto test_reset;
	}
	
	//enter into boot
	ret = fts_write_reg(0xFC, 0xAA);
	if (ret < 0) {
		FTS_ERROR("write FC AA fails");
		goto test_reset;
	}
	sec_delay(10);
	ret = fts_write_reg(0xFC, 0x55);
	if (ret < 0) {
		FTS_ERROR("write FC 88 fails");
		goto test_reset;
	}

	sec_delay(80);
	cmd[0] = 0x55;
	ret = fts_write(cmd, 1);
	if (ret < 0) {
		FTS_ERROR("write 55 fails");
		goto test_reset;
	}

	cmd[0] = 0x90;
	ret = fts_read(cmd, 1, val, 3);
	if (ret < 0) {
		FTS_ERROR("read 90 fails");
		goto test_reset;
	}
	if (val[0] == 0x54 && val[1] == 0x52 && val[2] == 0xAA) {
		FTS_INFO("pram test enter into romboot success");
	} else {
		FTS_ERROR("pram test enter into romboot fail");
		ret = -EINVAL;
		goto test_reset;
	}

	cmd[0] = 0x09;
	cmd[1] = 0x0A;
	ret = fts_write(cmd, 2);
	if (ret < 0) {
		FTS_ERROR("write (09 0A) cmd fail");
		goto test_reset;
	}
	sec_delay(2);

	ecc_host = ft3519_pram_test_write_buf(pb_test_buf, pb_test_len);
	if (ecc_host < 0) {
		FTS_ERROR( "write pramboot fail");
		goto test_reset;
	}

	ecc_tp = ft3519_pram_test_ecc_cal(0, pb_test_len);
	if (ecc_tp < 0) {
		FTS_ERROR( "read pramboot ecc fail");
		goto test_reset;
	}

	// remap pramboot
	cmd[0] = 0x08;
	ret = fts_write(cmd, 1);
	if (ret < 0) {
		FTS_ERROR("write start pram cmd fail");
		goto test_reset;
	}
	sec_delay(100);

	cmd[0] = 0x01;
	ret = fts_read(cmd, 1, val, 5);
	if (ret < 0) {
		FTS_ERROR("read pram test result fail");
		goto test_reset;
	}

	if (val[0] == 2) {
		FTS_INFO("pram test result %d, PASS", val[0]);
		result = 1;
	} else {
		FTS_INFO("pram test result %d, addr:0x%x%x%x%x NG", val[0], val[1], val[2], val[3], val[4]);
		result = 0;
	}
	
test_reset:
	ret = fts_fwupg_reset_in_boot();
	if (ret < 0) {
		FTS_ERROR("reset to normal boot fail");
	}
	sec_delay(200);

	ret = fts_wait_test_done(DEVICE_MODE_ADDR, mode_temp, 200);
	if (ret < 0) {
		FTS_ERROR("failed to recovery mode");
	}

	fts_irq_enable();

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d", result);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "PRAM");
	FTS_INFO("%s", buff);
	
}


#define ID_G_SEC_SPEC_TEST_TYPE			0x90 //1:rawdata test 2:short test
#define ID_G_SEC_SPEC_TEST_RESULT		0x91 // 0:fail 1:Pass
#define ID_G_SEC_SPEC_TEST_THRESHOLD	0x92 //set threshold minL minH maxL maxH

enum fw_self_test_type {
	TEST_NONE = 0,
	RAWDATA_TEST,
	SHORT_TEST,
};

static int fts_fw_self_test_entry(int type, u8 cmd, u8 mode,
									int delayms, int min, int max)
{
	int ret = 0;
	u8 threshold[5] = {0};
	u8 result = 0;

	ret = fts_write_reg(ID_G_SEC_SPEC_TEST_TYPE, (u8)type);
	if (ret < 0) {
		FTS_ERROR("write test type fail ret = %d", ret);
		goto out;
	}

	threshold[0] = ID_G_SEC_SPEC_TEST_THRESHOLD;
	threshold[1] = (min >> 8) & 0xFF;
	threshold[2] = min & 0xFF;
	threshold[3] = (max >> 8) & 0xFF;
	threshold[4] = max & 0xFF;
	ret = fts_write(threshold, sizeof(threshold));
	if (ret < 0) {
		FTS_ERROR("set min value fail ret = %d", ret);
		goto out;
	}

	ret = fts_wait_test_done(cmd, mode, delayms);
	if (ret < 0) {
		FTS_ERROR("fail to start test");
		goto out;
	}

	ret = fts_read_reg(ID_G_SEC_SPEC_TEST_RESULT, &result);
	if (ret < 0) {
		FTS_ERROR("fail to read test result");
		goto out;
	}

out:
	if (result == 1) {
		FTS_INFO("firmware self test PASS");
		return 1;
	} else {
		FTS_INFO("firmware self test NG");
		return 0;
	}
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	char test[32];

	sec_cmd_set_default_result(sec);

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is off");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	enter_factory_mode();

	switch (sec->cmd_param[1]) {
	case RAWDATA_TEST:
		ret = fts_fw_self_test_entry(RAWDATA_TEST, DEVICE_MODE_ADDR,
								FACTORY_REG_START_SCAN, 20, 3500, 15000);
		break;
	case SHORT_TEST:
		ret = fts_fw_self_test_entry(SHORT_TEST, FACTROY_REG_SHORT_TEST_EN,
								FACTROY_REG_SHORT_CA, 500, 1200, 0);
		break;
	default:
		FTS_ERROR("unsupported param %d,%d", sec->cmd_param[0], sec->cmd_param[1]);

		snprintf(buff, sizeof(buff), "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (ret) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");
		snprintf(buff, sizeof(buff), "OK");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");
		snprintf(buff, sizeof(buff), "NG");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_RAW_INFO("%s", buff);

out:
	enter_work_mode();
	return;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 cnt = 0;

	sec_cmd_set_default_result(sec);

	ret = fts_read_cnt_info(&cnt);
	if (ret < 0) {
		FTS_ERROR("nvm read error(%d)", ret);
		goto out;
	}

	FTS_INFO("nvm read disassemble_count(%d)", cnt);

	if (cnt >= 0xFF)
		cnt = 0;

	if (cnt < 0xFE)
		cnt++;

	ret = fts_write_cnt_info(cnt);
	if (ret < 0) {
		FTS_ERROR("nvm write error(%d)", ret);
	}

out:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 cnt = 0;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ret = fts_read_cnt_info(&cnt);
	if (ret < 0) {
		FTS_ERROR("nvm read error(%d)", cnt);
		cnt = 0;
	}

	if (cnt >= 0xff) {
		FTS_ERROR("clear nvm disassemble count(%X)", cnt);
		cnt = 0;
		ret = fts_write_cnt_info(cnt);
		if (ret < 0) {
			FTS_ERROR("nvm write error(%d)", ret);
		}
	}

	FTS_INFO("nvm disassemble count(%d)", cnt);

	snprintf(buff, sizeof(buff), "%d", cnt);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void run_allnode_data(void *device_data, int test_type)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int i;
	int data_num = 0;

	sec_cmd_set_default_result(sec);

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		return;
	}

	buff = kzalloc(ts_data->tx_num * ts_data->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		FTS_ERROR("failed to alloc allnode buff");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		return;
	}

	enter_factory_mode();

	FTS_INFO("test_type:%d", test_type);

	switch (test_type) {
	case ALL_NODE_TYPE_SHORT:
		get_short(sec);
		data_num = ts_data->tx_num + ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_SCAP_CB:
		get_scap_cb(sec);
		data_num = ts_data->tx_num + ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_SCAP_RAWDATA:
		get_scap_rawdata(sec);
		data_num = ts_data->tx_num + ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_RAWDATA:
		get_rawdata(sec);
		data_num = ts_data->tx_num * ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_PANEL_DIFFER:
		get_panel_differ(sec);
		data_num = ts_data->tx_num * ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_NOISE:
		get_noise(sec);
		data_num = ts_data->tx_num * ts_data->rx_num;
		break;
	case ALL_NODE_TYPE_PRAM:
		data_num = 0;
		get_pram_test(sec);
		break;
	default:
		FTS_ERROR("test type is not supported %d", test_type);
		break;
	}

	enter_work_mode();

	sec_cmd_set_default_result(sec);

	for (i = 0; i < data_num; i++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts_data->pFrame[i]);
		strlcat(buff, temp, ts_data->tx_num * ts_data->rx_num * CMD_RESULT_WORD_LEN);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);

	kfree(buff);
}

static void run_short_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_SHORT);
}

static void run_cb_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_SCAP_CB);
}

static void run_scap_rawdata_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_SCAP_RAWDATA);
}

static void run_rawdata_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_RAWDATA);
}

static void run_panel_differ_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_PANEL_DIFFER);
}


static void run_noise_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_NOISE);
}

static void run_pram_test_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_PRAM);
}


static void get_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ii;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	int tx_max = 0;
	int rx_max = 0;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if ((ii + 1) % (ts->rx_num) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap_tx = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap_tx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);
			tx_max = max(tx_max, node_gap_tx);
		}

		if (ii < (ts->tx_num - 1) * ts->rx_num) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->rx_num])
				node_gap_rx = 100 - (ts->pFrame[ii + ts->rx_num] * 100 / ts->pFrame[ii]);
			else
				node_gap_rx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->rx_num]);
			rx_max = max(rx_max, node_gap_rx);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAWDATA_GAP_X");
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAWDATA_GAP_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if ((ii + 1) % (ts->rx_num) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if (ii < (ts->tx_num - 1) * ts->rx_num) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->rx_num])
				node_gap = 100 - (ts->pFrame[ii + ts->rx_num] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->rx_num]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void factory_cmd_result_all(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	sec->item_count = 0;

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	mutex_lock(&ts_data->device_lock);

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	enter_factory_mode();
	
	get_noise(sec);
	get_rawdata(sec);
	get_gap_data(sec);
	get_scap_cb(sec);
	get_scap_rawdata(sec);
	get_short(sec);
	get_panel_differ(sec);
	get_pram_test(sec);
	
	enter_work_mode();

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
	mutex_unlock(&ts_data->device_lock);

	FTS_RAW_INFO("%d%s", sec->item_count, sec->cmd_result_all);
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, i;
	u8 status = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_param[0] <= 0 || sec->cmd_param[0] > 1000) {
		FTS_ERROR("invalid param %d", sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	ret = fts_write_reg(FTS_REG_CODE_BANK_MODE, 1);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 1, ret=%d", ret);
		goto out;
	}

	sec_delay(50);

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	ret = fts_write_reg(FTS_REG_SNR_FRAME_COUNT_H, (sec->cmd_param[0] >> 8) & 0xFF);
	if (ret < 0) {
		FTS_ERROR("failed to write snr frame count High, ret=%d", ret);
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_SNR_FRAME_COUNT_L, sec->cmd_param[0] & 0xFF);
	if (ret < 0) {
		FTS_ERROR("failed to write snr frame count Low, ret=%d", ret);
		goto out_mode;
	}

	FTS_INFO("test start");
	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	ret = fts_write_reg(FTS_REG_POWER_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write power mode 0, ret=%d", ret);
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_SNR_NON_TOUCHED_TEST, 1);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
		goto out_mode;
	}

	sec_delay(8 * sec->cmd_param[0]);

	for (i = 0; i < 50; i++) {
		ret = fts_read_reg(FTS_REG_SNR_NON_TOUCHED_TEST, &status);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out_mode;
		}

		if (status == 0)
			break;

		sec_delay(100);
	}

	if (i == 50) {
		FTS_ERROR("test timeout, status=0x%02X", status);
		ret = -EIO;
		goto out_mode;
	}

out_mode:
	if (fts_write_reg(FTS_REG_CODE_BANK_MODE, 0) < 0)
		FTS_ERROR("failed to write bank mode 0");

	sec_delay(500);
out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[27 * 10] = { 0 };
	int ret = 0, i;
	u8 status = 0, cmd;
	u8 status2 = 0;
	u8 rbuff[27 * 2] = { 0 };
	short average[9] = { 0 };
	short snr1[9] = { 0 };
	short snr2[9] = { 0 };

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_param[0] <= 0 || sec->cmd_param[0] > 1000) {
		FTS_ERROR("invalid param %d", sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	ret = fts_write_reg(FTS_REG_CODE_BANK_MODE, 1);
	if (ret < 0) {
		FTS_ERROR("failed to write FTS_REG_CODE_BANK_MODE, ret=%d", ret);
		goto out;
	}

	sec_delay(50);

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	FTS_INFO("test start");
	ret = fts_write_reg(FTS_REG_SNR_TOUCHED_TEST, 1);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
		goto out_mode;
	}

	sec_delay(8 * sec->cmd_param[0]);

	for (i = 0; i < 50; i++) {
		ret = fts_read_reg(FTS_REG_SNR_TEST_STATUS, &status);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out_mode;
		}

		ret = fts_read_reg(FTS_REG_SNR_TOUCHED_TEST, &status2);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out_mode;
		}

		if (status == TEST_RETVAL_AA && status2 == 0)
			break;

		sec_delay(100);
	}

	if (i == 50) {
		FTS_ERROR("test timeout, status=0x%02X", status);
		ret = -EIO;
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_SNR_BUFF_COUNTER, 0);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
		goto out_mode;
	}

	cmd = FTS_REG_SNR_TEST_RESULT1;
	ret = fts_read(&cmd, 1, rbuff, 54);
	if (ret < 0) {
		FTS_ERROR("failed to read snr test result1, ret=%d", ret);
		goto out_mode;
	}

	for (i = 0; i < 9; i++) {
		char temp[3 * 10] = { 0 };

		average[i] = rbuff[i * 6] << 8 | rbuff[(i * 6) + 1];
		snr1[i] = rbuff[(i * 6) + 2] << 8 | rbuff[(i * 6) + 3]; 
		snr2[i] = rbuff[(i * 6) + 4] << 8 | rbuff[(i * 6) + 5];
		snprintf(temp, sizeof(temp), "%d,%d,%d%s",
				average[i], snr1[i], snr2[i], (i < 8) ? "," : "");
		strlcat(buff, temp, sizeof(buff));
	}

out_mode:
	if (fts_write_reg(FTS_REG_CODE_BANK_MODE, 0) < 0)
		FTS_ERROR("failed to write bank mode 0");

	sec_delay(500);

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[27 * 10] = { 0 };
	u16 THD_X = 0;
	short SUM_X = 0;
	u8 cmd = 0;
	u8 val[4] = {0};
	int ret = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	cmd = FTS_PROX_TOUCH_SENS;
	ret = fts_read(&cmd, 1, val, 4);
	if (ret < 0) {
		FTS_ERROR("failed to read snr test result1, ret=%d", ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	THD_X = (val[0] << 8) + val[1];
	SUM_X = (val[2] << 8) + val[3];
	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", SUM_X, THD_X);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[27 * 10] = { 0 };
	int ret = 0;
	u8 rbuff = 0;
	u16 crc = 0, crc_r = 0;
	u32 crc_result = 0;

	sec_cmd_set_default_result(sec);

	mutex_lock(&ts_data->device_lock);

	sec_delay(200);

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		return;
	}

	ret = fts_read_reg(FTS_REG_CRC_H, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc H, ret=%d", ret);
		goto out;
	}
	crc |= rbuff << 8;

	ret = fts_read_reg(FTS_REG_CRC_L, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc L, ret=%d", ret);
		goto out;
	}
	crc |= rbuff;

	ret = fts_read_reg(FTS_REG_CRC_REVERT_H, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc_r H, ret=%d", ret);
		goto out;
	}
	crc_r |= rbuff << 8;

	ret = fts_read_reg(FTS_REG_CRC_REVERT_L, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc_r L, ret=%d", ret);
		goto out;
	}
	crc_r |= rbuff;

	crc_result = crc | crc_r;
	FTS_INFO("crc:0x%04X, crc_r:0x%04X, result:0x%04X", crc, crc_r, crc_result);

	if (crc_result == 0xFFFF)
		ret = 0;
	else
		ret = -EIO;

out:
	mutex_unlock(&ts_data->device_lock);

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "OK");
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		return;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

int fts_set_refresh_rate(struct fts_ts_data *ts_data)
{
	int ret;
	u8 cmd;
	
	if (ts_data->refresh_rate)
		cmd = 0x12;
	else
		cmd = 0x0c;

	ret = fts_write_reg(FTS_REG_REFRESH_RATE, cmd);

	if (ret < 0)
		FTS_ERROR("failed to set refresh rate[%d]", ts_data->refresh_rate);
	else
		FTS_INFO("set refresh rate[%d] success", ts_data->refresh_rate);

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
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!ts_data->pdata->support_refresh_rate_mode) {
		FTS_ERROR("not support cmd(%d)!", sec->cmd_param[0]);
		goto NG;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		FTS_ERROR("not support param[%d]", sec->cmd_param[0]);
		goto NG;
	}

	ts_data->refresh_rate = sec->cmd_param[0];

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		goto NG;
	}

	ret = fts_set_refresh_rate(ts_data);
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

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts_data->pdata->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts_data->pdata->lowpower_mode |= SEC_TS_MODE_SPONGE_PRESS;
	else
		ts_data->pdata->lowpower_mode &= ~SEC_TS_MODE_SPONGE_PRESS;

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_FOD;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_FOD;

	ts_data->pdata->fod_data.press_prop = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

	FTS_INFO("%s, fast:%s, strict:%s, lp:0x%02X, pm:0x%02X",
			sec->cmd_param[0] ? "on" : "off",
			ts_data->pdata->fod_data.press_prop & 1 ? "on" : "off",
			ts_data->pdata->fod_data.press_prop & 2 ? "on" : "off",
			ts_data->pdata->lowpower_mode, ts_data->power_mode);

	if (!ts_data->pdata->enabled && !ts_data->pdata->lowpower_mode && !ts_data->pdata->pocket_mode
			&& !ts_data->pdata->ed_enable && !ts_data->pdata->fod_lp_mode) {
		if (device_may_wakeup(ts_data->dev) && ts_data->pdata->power_state == SEC_INPUT_STATE_LPM)
			disable_irq_wake(ts_data->irq);

		ts_data->suspended = false;
		fts_ts_input_close(ts_data->input_dev);
	} else {
		ts_data->fod_mode = (ts_data->pdata->fod_data.press_prop << 1) | sec->cmd_param[0];
		FTS_INFO("fod_mode = %x", ts_data->fod_mode);
		fts_write_reg(FTS_REG_FOD_MODE, ts_data->fod_mode);

		if (!(ts_data->fod_mode & 0x01)) {
			ts_data->fod_state = 2;
			sec_input_gesture_report(ts_data->dev, SPONGE_EVENT_TYPE_FOD_RELEASE, 540, 2190);
		}
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts_data->pdata->support_fod_lp_mode) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	ts_data->pdata->fod_lp_mode = sec->cmd_param[0];
	FTS_INFO("%d", ts_data->pdata->fod_lp_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	FTS_INFO("l:%d, t:%d, r:%d, b:%d",
			sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts_data->pdata->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (!sec_input_set_fod_rect(ts_data->dev, sec->cmd_param))
		goto NG;

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		goto NG;
	}

	ret = fts_set_fod_rect(ts_data);
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

static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		FTS_ERROR("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_POCKET;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_POCKET;

	ts_data->pdata->pocket_mode = sec->cmd_param[0];

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		goto fail;
	}

	ret = fts_write_reg(FTS_REG_POCKET_MODE, ts_data->pdata->pocket_mode);
	if (ret < 0) {
		FTS_ERROR("send pocket mode cmd failed");
		goto fail;
	}

	FTS_INFO("%s, lp:0x%02X, pm:0x%02X", sec->cmd_param[0] ? "on" : "off", ts_data->pdata->lowpower_mode, ts_data->power_mode);

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
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!(sec->cmd_param[0] == 0 || sec->cmd_param[0] == 1 || sec->cmd_param[0] == 3)) {
		FTS_ERROR("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_EAR_DETECT;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_EAR_DETECT;

	ts_data->pdata->ed_enable = sec->cmd_param[0];

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		goto fail;
	}

	ret = fts_write_reg(FTS_REG_PROXIMITY_MODE, ts_data->pdata->ed_enable);
	if (ret < 0) {
		FTS_ERROR("send ear detect cmd failed");
		goto fail;
	}

	ts_data->hover_event = 0xFF;

	FTS_INFO("%s, lp:0x%02X, pm:0x%02X", sec->cmd_param[0] ? "on" : "off", ts_data->pdata->lowpower_mode, ts_data->power_mode);

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

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_SIP_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write sip mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_GAME_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write game mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_NOTE_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write note mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0])
		ts_data->pdata->lowpower_mode |= SEC_TS_MODE_SPONGE_SINGLE_TAP;
	else
		ts_data->pdata->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SINGLE_TAP;

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_SINGLE_TAP;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_SINGLE_TAP;

	ts_data->singletap_enable = sec->cmd_param[0];
	if (ts_data->singletap_enable)
		ts_data->gesture_mode |= GESTURE_SINGLETAP_EN;
	else
		ts_data->gesture_mode &= ~GESTURE_SINGLETAP_EN;

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ret = fts_write_reg(FTS_REG_SINGLETAP_EN, ts_data->singletap_enable);
	if (ret < 0) {
		FTS_ERROR("failed to write power mode %d", ts_data->singletap_enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s, gesture_mode:0x%02X, lp:0x%02X, pm:0x%02X", sec->cmd_param[0] ? "on" : "off",
			ts_data->gesture_mode, ts_data->pdata->lowpower_mode, ts_data->power_mode);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0])
		ts_data->pdata->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts_data->pdata->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_SWIPE;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_SWIPE;

	ts_data->spay_enable = sec->cmd_param[0];
	if (ts_data->spay_enable)
		ts_data->gesture_mode |= GESTURE_SPAY_EN;
	else
		ts_data->gesture_mode &= ~GESTURE_SPAY_EN;

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;
	ret = fts_write_reg(FTS_REG_SPAY_EN, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write spay mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s, gesture_mode:0x%02X, lp:0x%02X, pm:0x%02X", sec->cmd_param[0] ? "on" : "off",
			ts_data->gesture_mode, ts_data->pdata->lowpower_mode, ts_data->power_mode);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0])
		ts_data->pdata->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts_data->pdata->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	if (sec->cmd_param[0])
		ts_data->power_mode |= FTS_POWER_MODE_DOUBLETAP_TO_WAKEUP;
	else
		ts_data->power_mode &= ~FTS_POWER_MODE_DOUBLETAP_TO_WAKEUP;

	ts_data->aot_enable = sec->cmd_param[0];
	if (ts_data->aot_enable)
		ts_data->gesture_mode |= GESTURE_DOUBLECLICK_EN;
	else
		ts_data->gesture_mode &= ~GESTURE_DOUBLECLICK_EN;

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("IC is OFF");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;
	ret = fts_write_reg(FTS_REG_DOUBLETAP_TO_WAKEUP_EN, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write aot mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s, gesture_mode:0x%02X, lp:0x%02X, pm:0x%02X", sec->cmd_param[0] ? "on" : "off",
			ts_data->gesture_mode, ts_data->pdata->lowpower_mode, ts_data->power_mode);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts_data->glove_mode = sec->cmd_param[0];

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", ts_data->glove_mode ? "on" : "off");

	ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, ts_data->glove_mode);
	if (ret < 0) {
		FTS_ERROR("failed to write glove mode");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 enable;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = !(sec->cmd_param[0] & 0xFF);

	FTS_INFO("mode %s", enable ? "on" : "off");

	ret = fts_write_reg(FTS_REG_TEST_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write 0x9E 0x%02X", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void fts_set_grip_data_to_ic(struct fts_ts_data *ts, u8 flag)
{
	int ret, i, data_size = 0;
	u8 regAddr[9] = { 0 };
	u8 data[9] = { 0 };

	if (ts->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_INFO("ic is off now, skip 0x%02X", flag);
		return;
	}

	if (flag == G_SET_EDGE_HANDLER) {
		FTS_INFO("set edge handler");

		ret = fts_write_reg(FTS_REG_BANK_MODE, 1);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode 1, ret=%d", ret);
			return;
		}
		
		regAddr[0] = FTS_REG_EDGE_HANDLER;
		regAddr[1] = FTS_REG_EDGE_HANDLER_DIRECTION;
		regAddr[2] = FTS_REG_EDGE_HANDLER_UPPER_H;
		regAddr[3] = FTS_REG_EDGE_HANDLER_UPPER_L;
		regAddr[4] = FTS_REG_EDGE_HANDLER_LOWER_H;
		regAddr[5] = FTS_REG_EDGE_HANDLER_LOWER_L;

		data[0] = 0x00;
		data[1] = ts->grip_data.edgehandler_direction & 0xFF;
		data[2] = (ts->grip_data.edgehandler_start_y >> 8) & 0xFF;
		data[3] = ts->grip_data.edgehandler_start_y & 0xFF;
		data[4] = (ts->grip_data.edgehandler_end_y >> 8) & 0xFF;
		data[5] = ts->grip_data.edgehandler_end_y & 0xFF;
		data_size = 6;
	} else if (flag == G_SET_NORMAL_MODE) {
		FTS_INFO("set portrait mode");

		ret = fts_write_reg(FTS_REG_BANK_MODE, 1);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode 1, ret=%d", ret);
			return;
		}

		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_PORTRAIT_MODE;
		regAddr[2] = FTS_REG_GRIP_PORTRAIT_GRIP_ZONE;
		regAddr[3] = FTS_REG_GRIP_PORTRAIT_REJECT_UPPER;
		regAddr[4] = FTS_REG_GRIP_PORTRAIT_REJECT_LOWER;
		regAddr[5] = FTS_REG_GRIP_PORTRAIT_REJECT_Y_H;
		regAddr[6] = FTS_REG_GRIP_PORTRAIT_REJECT_Y_L;

		data[0] = 0x01;
		data[1] = 0x01;
		data[2] = ts->grip_data.edge_range & 0xFF;
		data[3] = ts->grip_data.deadzone_up_x & 0xFF;
		data[4] = ts->grip_data.deadzone_dn_x & 0xFF;
		data[5] = (ts->grip_data.deadzone_y >> 8) & 0xFF;
		data[6] = ts->grip_data.deadzone_y & 0xFF;
		data_size = 7;
 	} else if (flag == G_SET_LANDSCAPE_MODE) {
		FTS_INFO("set landscape mode");

		ret = fts_write_reg(FTS_REG_BANK_MODE, 2);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode 2, ret=%d", ret);
			return;
		}

		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_LANDSCAPE_MODE;
		regAddr[2] = FTS_REG_GRIP_LANDSCAPE_ENABLE;
		regAddr[3] = FTS_REG_GRIP_LANDSCAPE_GRIP_ZONE;
		regAddr[4] = FTS_REG_GRIP_LANDSCAPE_REJECT_ZONE;
		regAddr[5] = FTS_REG_GRIP_LANDSCAPE_REJECT_TOP;
		regAddr[6] = FTS_REG_GRIP_LANDSCAPE_REJECT_BOTTOM;
		regAddr[7] = FTS_REG_GRIP_LANDSCAPE_GRIP_TOP;
		regAddr[8] = FTS_REG_GRIP_LANDSCAPE_GRIP_BOTTOM;

		data[0] = 0x02;
		data[1] = 0x02;
		data[2] = ts->grip_data.landscape_mode & 0xFF;
		data[3] = ts->grip_data.landscape_edge & 0xFF;
		data[4] = ts->grip_data.landscape_deadzone & 0xFF;
		data[5] = ts->grip_data.landscape_top_deadzone & 0xFF;
		data[6] = ts->grip_data.landscape_bottom_deadzone & 0xFF;
		data[7] = ts->grip_data.landscape_top_gripzone & 0xFF;
		data[8] = ts->grip_data.landscape_bottom_gripzone & 0xFF;
		data_size = 9;
 	} else if (flag == G_CLR_LANDSCAPE_MODE) {
		FTS_INFO("clear landscape mode");

		ret = fts_write_reg(FTS_REG_BANK_MODE, 2);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode 1, ret=%d", ret);
			return;
		}
		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_LANDSCAPE_MODE;
		regAddr[2] = FTS_REG_GRIP_LANDSCAPE_ENABLE;

		data[0] = 0x01;
		data[1] = 0x02;
		data[2] = ts->grip_data.landscape_mode & 0xFF;
		data_size = 3;
	} else {
		FTS_ERROR("flag 0x%02X is invalid", flag);
		return;
	}

	for (i = 0; i < data_size; i++) {
		ret = fts_write_reg(regAddr[i], data[i]);
		if (ret < 0) {
			FTS_ERROR("failed to write 0x%02X 0x%02X", regAddr[i], data[i]);
			return;
		}
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		return;
	}
}

void fts_set_edge_handler(struct fts_ts_data *ts)
{
	u8 mode = G_NONE;

	FTS_INFO("re-init direction:%d", ts->grip_data.edgehandler_direction);

	if (ts->grip_data.edgehandler_direction != 0)
		mode = G_SET_EDGE_HANDLER;

	if (mode)
		fts_set_grip_data_to_ic(ts, mode);
}

/*
 *	index  0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone up y), X (dead zone bottom x), X (dead zone down y)
 *			ex) echo set_grip_data,1,60,10,32,926,32,3088 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y), X (edge zone top y), X (edge zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (portrait mode)
 *			ex) echo set_grip_data,2,0 > cmd
 */
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	mutex_lock(&ts->device_lock);

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] >= 0 && sec->cmd_param[1] < 3) {
			ts->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			FTS_ERROR("cmd1 is abnormal, %d (%d)", sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		mode = G_SET_EDGE_HANDLER;
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		ts->grip_data.edge_range = sec->cmd_param[1];
		ts->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->grip_data.deadzone_y = sec->cmd_param[4];
		mode = G_SET_NORMAL_MODE;
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->grip_data.landscape_mode = 0;
			mode = G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->grip_data.landscape_mode = 1;
			ts->grip_data.landscape_edge = sec->cmd_param[2];
			ts->grip_data.landscape_deadzone = sec->cmd_param[3];
			ts->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = G_SET_LANDSCAPE_MODE;
		} else {
			FTS_ERROR("cmd1 is abnormal, %d (%d)", sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
	} else {
		FTS_ERROR("cmd0 is abnormal, %d", sec->cmd_param[0]);
		goto err_grip_data;
	}

	fts_set_grip_data_to_ic(ts, mode);

	mutex_unlock(&ts->device_lock);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&ts->device_lock);

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

void fts_set_scan_off(bool scan_off)
{
	int ret;
	u8 cmd;

	if (scan_off)
		cmd = FTS_REG_POWER_MODE_SCAN_OFF;
	else
		cmd = 0x00;

	FTS_INFO("scan %s", scan_off ? "off" : "on");

	ret = fts_write_reg(FTS_REG_POWER_MODE, cmd);
	if (ret < 0)
		FTS_ERROR("failed to write scan off");
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0] > 1)
		ts_data->cover_mode = true;
	else
		ts_data->cover_mode = false;

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", ts_data->cover_mode ? "closed" : "opened");

	if (ts_data->pdata->sense_off_when_cover_closed) {
#if 0
		mutex_lock(&ts_data->device_lock);
		fts_set_scan_off(ts_data->cover_mode);
		mutex_unlock(&ts_data->device_lock);
#endif

	} else {
		ret = fts_write_reg(FTS_REG_COVER_MODE_EN, ts_data->cover_mode);
		if (ret < 0) {
			FTS_ERROR("failed to write cover mode");
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void scan_block(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->pdata->power_state == SEC_INPUT_STATE_POWER_OFF) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", sec->cmd_param[0] ? "block" : "unblock");
#if 0
	fts_set_scan_off(sec->cmd_param[0]);
#endif

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_short", get_short),},
	{SEC_CMD("run_short_all", run_short_all),},
	{SEC_CMD("get_cb", get_scap_cb),},
	{SEC_CMD("run_cb_all", run_cb_all),},
	{SEC_CMD("get_scap_rawdata", get_scap_rawdata),},
	{SEC_CMD("run_scap_rawdata_all", run_scap_rawdata_all),},
	{SEC_CMD("get_rawdata", get_rawdata),},
	{SEC_CMD("run_rawdata_all", run_rawdata_all),},
	{SEC_CMD("get_panel_diff", get_panel_differ),},
	{SEC_CMD("run_panel_diff_all", run_panel_differ_all),},
	{SEC_CMD("run_gap_data_x_all", run_gap_data_x_all),},
	{SEC_CMD("run_gap_data_y_all", run_gap_data_y_all),},
	{SEC_CMD("get_noise", get_noise),},
	{SEC_CMD("run_noise_all", run_noise_all),},
	{SEC_CMD("get_pram_test", get_pram_test),},
	{SEC_CMD("run_pram_test_all", run_pram_test_all),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
	{SEC_CMD("run_cs_raw_read_all", run_rawdata_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("scan_block", scan_block),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("refresh_rate_mode", refresh_rate_mode),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static int fts_sec_facory_test_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 rbuf;

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
	}
	ret = fts_read_reg(FTS_REG_TX_NUM, &rbuf);
	if (ret < 0) {
		FTS_ERROR("get tx_num fail");
		ts_data->tx_num = TX_NUM_MAX;
	} else {
		ts_data->tx_num = rbuf;
	}

	ret = fts_read_reg(FTS_REG_RX_NUM, &rbuf);
	if (ret < 0) {
		FTS_ERROR("get rx_num fail");
		ts_data->rx_num = RX_NUM_MAX;
	} else {
		ts_data->rx_num = rbuf;
	}

	ts_data->pdata->x_node_num = ts_data->tx_num;
	ts_data->pdata->y_node_num = ts_data->rx_num;

	ts_data->pFrame_size = ts_data->tx_num * ts_data->rx_num * sizeof(int);
	ts_data->pFrame = devm_kzalloc(ts_data->dev, ts_data->pFrame_size, GFP_KERNEL);
	if (!ts_data->pFrame) {
		FTS_ERROR("failed to alloc pFrame buff");
		return -ENOMEM;
	}

	FTS_INFO("tx_num:%d, rx_num:%d, pFrame_size:%d", ts_data->tx_num, ts_data->rx_num, ts_data->pFrame_size);
	return 0;
}

int fts_sec_cmd_init(struct fts_ts_data *ts_data)
{
	int retval = 0;

	FTS_FUNC_ENTER();

	retval = fts_sec_facory_test_init(ts_data);
	if (retval < 0) {
		FTS_ERROR("Failed to init sec_factory_test");
		goto exit;
	}

	retval = sec_cmd_init(&ts_data->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		FTS_ERROR("Failed to sec_cmd_init");
		goto exit;
	}

	ts_data->sec.wait_cmd_result_done = true;

	retval = sysfs_create_group(&ts_data->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		FTS_ERROR("Failed to create sysfs attributes");
		sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts_data->sec.fac_dev->kobj,
			&ts_data->input_dev->dev.kobj, "input");
	if (retval < 0) {
		FTS_ERROR("Failed to create input symbolic link");
		sysfs_remove_group(&ts_data->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sec_input_sysfs_create(&ts_data->pdata->input_dev->dev.kobj);
	if (retval < 0) {
		FTS_ERROR("Failed to create sec_input_sysfs attributes");
		sysfs_remove_link(&ts_data->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts_data->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	FTS_FUNC_EXIT();
	return 0;

exit:
	return retval;

}

void fts_sec_cmd_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

	sec_input_sysfs_remove(&ts_data->pdata->input_dev->dev.kobj);

	sysfs_remove_link(&ts_data->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts_data->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
}

MODULE_LICENSE("GPL");

