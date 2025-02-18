/* drivers/input/sec_input/slsi/slsi_cmd.c
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "slsi_dev.h"
#include "slsi_reg.h"

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[256] = { 0 };

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &ts->client->dev,
			"%s: id: %d\n", __func__, ts->plat_data->gesture_id);
#else
	input_info(true, &ts->client->dev,
			"%s: id: %d, X:%d, Y:%d\n", __func__,
			ts->plat_data->gesture_id, ts->plat_data->gesture_x, ts->plat_data->gesture_y);
#endif
	snprintf(buff, sizeof(buff), "%d %d %d", ts->plat_data->gesture_id,
			ts->plat_data->gesture_x, ts->plat_data->gesture_y);

	ts->plat_data->gesture_x = 0;
	ts->plat_data->gesture_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

/* for bigdata */
/* read param */
static ssize_t hardware_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_INPUT_HW_PARAM_SIZE];
	char tbuff[SEC_CMD_STR_LEN];
	char temp[SEC_CMD_STR_LEN];

	memset(buff, 0x00, sizeof(buff));

	sec_input_get_common_hw_param(ts->plat_data, buff);

	/* module_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), ",\"TMOD\":\"SE%02X%02X%02X%c%01X\"",
			ts->plat_data->img_version_of_bin[1], ts->plat_data->img_version_of_bin[2],
			ts->plat_data->img_version_of_bin[3],
#ifdef TCLM_CONCEPT
			ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].s_name,
			ts->tdata->nvdata.cal_count & 0xF);
#else
			'0', 0);
#endif
	strlcat(buff, tbuff, sizeof(buff));

	/* vendor_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	if (ts->plat_data->firmware_name) {
		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, 5, "%s", ts->plat_data->firmware_name);

		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"LSI_%s\"", temp);
	} else {
		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"LSI\"");
	}
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

/* clear param */
static ssize_t hardware_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	sec_input_clear_common_hw_param(ts->plat_data);

	return count;
}

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u16 dump_start, dump_end, dump_cnt;
	int i, ret, dump_area, dump_gain;
	unsigned char *sec_spg_dat;
	u8 dump_clear_packet[3] = {0x01,0x00,0x01};

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: Reset is ongoing!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Reset is ongoing");
	}

	/* preparing dump buffer */
	sec_spg_dat = vmalloc(SEC_TS_MAX_SPONGE_DUMP_BUFFER);
	if (!sec_spg_dat) {
		input_err(true, &ts->client->dev, "%s : Failed!!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "vmalloc failed");
	}
	memset(sec_spg_dat, 0, SEC_TS_MAX_SPONGE_DUMP_BUFFER);

	disable_irq(ts->client->irq);

	string_data[0] = SEC_TS_CMD_SPONGE_LP_DUMP_CUR_IDX;
	string_data[1] = 0;

	ret = ts->slsi_ts_read_sponge(ts, string_data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
		snprintf(buf, SEC_CMD_BUF_SIZE, "NG, Failed to read rect");
		goto out;
	}

	if (ts->sponge_inf_dump)
		dump_gain = 2;
	else
		dump_gain = 1;

	current_index = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
	dump_start = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
	dump_end = dump_start + (ts->sponge_dump_format * ((ts->sponge_dump_event * dump_gain) - 1));

	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &ts->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	/* legacy get_lp_dump */
	input_info(true, &ts->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
			__func__, ts->sponge_dump_format, ts->sponge_dump_event, dump_start, dump_end, current_index);

	for (i = (ts->sponge_dump_event * dump_gain) - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (ts->sponge_dump_format * i))
			string_addr = (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain) + current_index - (ts->sponge_dump_format * i);
		else
			string_addr = current_index - (ts->sponge_dump_format * i);

		if (string_addr < dump_start)
			string_addr += (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain);

		string_data[0] = string_addr & 0xFF;
		string_data[1] = (string_addr & 0xFF00) >> 8;

		ret = ts->slsi_ts_read_sponge(ts, string_data, ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to read sponge\n", __func__);
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
			if (ts->sponge_dump_format == 10) {
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

	if (ts->sponge_inf_dump) {
		if (current_index >= ts->sponge_dump_border) {
			dump_cnt = ((current_index - (ts->sponge_dump_border)) / ts->sponge_dump_format) + 1;
			dump_area = 1;
			sec_spg_dat[0] = ts->sponge_dump_border_lsb;
			sec_spg_dat[1] = ts->sponge_dump_border_msb;
		} else {
			dump_cnt = ((current_index - SEC_TS_CMD_SPONGE_LP_DUMP_EVENT) / ts->sponge_dump_format) + 1;
			dump_area = 0;
			sec_spg_dat[0] = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
			sec_spg_dat[1] = 0;
		}

		ret = ts->slsi_ts_read_sponge(ts, sec_spg_dat, dump_cnt * ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read sponge\n", __func__);
			goto out;
		}

		for (i = 0 ; i <= dump_cnt ; i++) {
			int e_offset = i * ts->sponge_dump_format;
			char ibuff[30] = {0, };
			u16 edata[5];
			
			edata[0] = (sec_spg_dat[1 + e_offset] & 0xFF) << 8 | (sec_spg_dat[0 + e_offset] & 0xFF);
			edata[1] = (sec_spg_dat[3 + e_offset] & 0xFF) << 8 | (sec_spg_dat[2 + e_offset] & 0xFF);
			edata[2] = (sec_spg_dat[5 + e_offset] & 0xFF) << 8 | (sec_spg_dat[4 + e_offset] & 0xFF);
			edata[3] = (sec_spg_dat[7 + e_offset] & 0xFF) << 8 | (sec_spg_dat[6 + e_offset] & 0xFF);
			edata[4] = (sec_spg_dat[9 + e_offset] & 0xFF) << 8 | (sec_spg_dat[8 + e_offset] & 0xFF);

			if (edata[0] || edata[1] || edata[2] || edata[3] || edata[4]) {
				snprintf(ibuff, sizeof(ibuff), "%03d: %04x%04x%04x%04x%04x\n",
						i + (ts->sponge_dump_event * dump_area),
						edata[0], edata[1], edata[2], edata[3], edata[4]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
				sec_tsp_sponge_log(ibuff);
#endif
			}
		}

		ts->sponge_dump_delayed_flag = false;
		ret = ts->slsi_ts_write_sponge(ts, dump_clear_packet, 3);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to clear sponge dump\n", __func__);
		}
	}
out:
	vfree(sec_spg_dat);
	enable_irq(ts->client->irq);
	return strlen(buf);
}

static ssize_t ic_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[512] = { 0 };
	char temp[128] = { 0 };
	u8 data[2] = { 0 };
	int ret;

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_TOUCHFUNCTION, data, 2);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "mutual:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_MUTUAL ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "hover:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_HOVER ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "cover:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_COVER ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "glove:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_GLOVE ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "stylus:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_STYLUS ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "palm:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_PALM ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "wet:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_WET ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "prox:%d, ", data[0] & SLSI_TS_BIT_SETFUNC_PROXIMITY ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	data[1] = 0;
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "npm:%d, ", data[0] == TO_TOUCH_MODE);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "lpm:%d, ", data[0] == TO_LOWPOWER_MODE);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "test:%d, ", data[0] == TO_SELFTEST_MODE);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "flash:%d, ", data[0] == TO_FLASH_MODE);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	ret = ts->slsi_ts_i2c_read(ts, SET_TS_CMD_SET_CHARGER_MODE, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "no_charger:%d, ",
			data[0] == SLSI_TS_BIT_CHARGER_MODE_NO);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "wired_charger:%d, ",
			data[0] == SLSI_TS_BIT_CHARGER_MODE_WIRE_CHARGER);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "wireless_charger:%d, ",
			data[0] == SLSI_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	ret = ts->slsi_ts_i2c_read(ts, SET_TS_CMD_SET_NOISE_MODE, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "noise:%d, ", data[0] & 0x0F);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "supported_noise_mode:%d, ", (data[0] >> 4) & 0x0F);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_MONITOR_NOISE_MODE, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "monitor_noise:%d, ", data[0]);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_COVERTYPE, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "cover_type:%d, ", data[0]);
	strlcat(buff, temp, sizeof(buff));

	data[0] = 0;
	ret = ts->slsi_ts_read_sponge(ts, data, 1);
	if (ret < 0)
		goto out;

	snprintf(temp, sizeof(temp), "aod:%d, ", data[0] & SEC_TS_MODE_SPONGE_AOD ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "aot:%d", data[0] & SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "spay:%d, ", data[0] & SEC_TS_MODE_SPONGE_SWIPE ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "fod:%d, ", data[0] & SEC_TS_MODE_SPONGE_PRESS ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));
	snprintf(temp, sizeof(temp), "singletap:%d", data[0] & SEC_TS_MODE_SPONGE_SINGLE_TAP ? 1 : 0);
	strlcat(buff, temp, sizeof(buff));

out:
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

#define SENSITIVITY_POINT_CNT	9	/* ~ davinci : 5 ea => 9 ea */
static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	char value_result[SENSITIVITY_POINT_CNT * 2] = { 0 };
	int value[SENSITIVITY_POINT_CNT] = { 0 };
	int ret, i;
	char tempv[10] = { 0 };
	char buff[SENSITIVITY_POINT_CNT * 10] = { 0 };

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_SENSITIVITY_VALUE, value_result, SENSITIVITY_POINT_CNT * 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c fail!, %d\n", __func__, ret);
		return ret;
	}

	for (i = 0 ; i < SENSITIVITY_POINT_CNT ; ++i) {
		value[i] = value_result[i * 2] << 8 | value_result[i * 2 + 1];

		if (i != 0)
			strlcat(buff, ",", sizeof(buff));
		snprintf(tempv, 10, "%d", value[i]);
		strlcat(buff, tempv, sizeof(buff));
	}

	input_info(true, &ts->client->dev, "%s: sensitivity mode : %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	int ret;
	u8 temp;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

	input_err(true, &ts->client->dev, "%s: enable:%ld\n", __func__, value);

	if (value == 1) {
		temp = 0x1;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSITIVITY_MODE, &temp, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode on fail!\n", __func__);
			return ret;
		}
		sec_delay(30);
		input_info(true, &ts->client->dev, "%s: enable end\n", __func__);
	} else {
		temp = 0x0;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSITIVITY_MODE, &temp, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode off fail!\n", __func__);
			return ret;
		}
		input_info(true, &ts->client->dev, "%s: disable end\n", __func__);
	}

	input_info(true, &ts->client->dev, "%s: done\n", __func__);

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
			ts->plat_data->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->plat_data->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, data);

	ts->plat_data->prox_power_off = data;

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %d%s%s%s%s%s%s%s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PRESSURE ? " pressure" : "",
			feature & INPUT_FEATURE_ENABLE_SYNC_RR120 ? " RR120hz" : "",
			feature & INPUT_FEATURE_ENABLE_VRR ? " vrr" : "",
			feature & INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST ? " openshort" : "",
			feature & INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST ? " miscal" : "",
			feature & INPUT_FEATURE_SUPPORT_WIRELESS_TX ? " wirelesstx" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t slsi_ts_fod_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	u8 data[255] = { SEC_TS_CMD_SPONGE_FOD_POSITION, };
	char buff[3] = { 0 };
	int i, ret;

	if (!ts->plat_data->support_fod) {
		input_err(true, &ts->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	if (!ts->plat_data->fod_data.vi_size) {
		input_err(true, &ts->client->dev, "%s: not read fod_info yet\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ret = ts->slsi_ts_read_sponge(ts, data, ts->plat_data->fod_data.vi_size);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	for (i = 0; i < ts->plat_data->fod_data.vi_size; i++) {
		snprintf(buff, 3, "%02X", data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t slsi_ts_fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	return sec_input_get_fod_info(&ts->client->dev, buf);
}

static ssize_t aod_active_area(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
			__func__, ts->plat_data->aod_data.active_area[0],
			ts->plat_data->aod_data.active_area[1], ts->plat_data->aod_data.active_area[2]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d",
			ts->plat_data->aod_data.active_area[0], ts->plat_data->aod_data.active_area[1],
			ts->plat_data->aod_data.active_area[2]);
}

static DEVICE_ATTR(scrub_pos, 0444, scrub_position_show, NULL);
static DEVICE_ATTR(hw_param, 0664, hardware_param_show, hardware_param_store); /* for bigdata */
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);
static DEVICE_ATTR(status, 0444, ic_status_show, NULL);
static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(fod_pos, 0444, slsi_ts_fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, slsi_ts_fod_info_show, NULL);
static DEVICE_ATTR(aod_active_area, 0444, aod_active_area, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_hw_param.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_status.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_aod_active_area.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int slsi_ts_write_factory_level(struct slsi_ts_data *ts, u8 pos)
{
	int ret = 0;

	input_info(true, &ts->client->dev,
			"%s: set factory level[%d]\n", __func__, pos);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_FACTORY_LEVEL, &pos, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set factory level,%d\n", __func__, pos);

	sec_delay(30);
	return ret;
}

static int execute_selftest(struct slsi_ts_data *ts, bool save_result)
{
	u8 pStr[50] = {0};
	u8 pTmp[20];
	int rc = 0;
	u8 tpara[2] = {0x23, 0x40};
	u8 *rBuff;
	int i;
	int result_size = SLSI_TS_SELFTEST_REPORT_SIZE + ts->tx_count * ts->rx_count * 2;
	int nlength;

	/* set Factory level */
	if (ts->factory_level) {
		rc = slsi_ts_write_factory_level(ts, ts->factory_position);
		if (rc < 0)
			goto err_set_level;
	}

	/* save selftest result in flash */
	if (save_result)
		tpara[0] = 0x23;
	else
		tpara[0] = 0xA3;

	rBuff = kzalloc(result_size, GFP_KERNEL);
	if (!rBuff) {
		rc = SEC_ERROR;
		goto err_mem;
	}

	input_info(true, &ts->client->dev, "%s: Self test start!\n", __func__);
	rc = slsi_ts_wait_for_ready(ts, SLSI_TS_CMD_SELFTEST, tpara, 2, 350);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_exit;
	}

	input_raw_info(true, &ts->client->dev, "%s: Self test done!\n", __func__);

	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_SELFTEST_RESULT, rBuff, result_size);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_exit;
	}

	nlength = SLSI_TS_SELFTEST_REPORT_SIZE - (SLSI_TS_SELFTEST_REPORT_SIZE % 4);

	for (i = 0; i < nlength; i += 4) {
		swap(rBuff[i], rBuff[i + 3]);
		swap(rBuff[i + 1], rBuff[i + 2]);
	}

	for (i = 0; i < 80; i += 4) {
		if (i / 4 == 0)
			strlcat(pStr, "SIG ", sizeof(pStr));
		else if (i / 4 == 1)
			strlcat(pStr, "VER ", sizeof(pStr));
		else if (i / 4 == 2)
			strlcat(pStr, "SIZ ", sizeof(pStr));
		else if (i / 4 == 3)
			strlcat(pStr, "CRC ", sizeof(pStr));
		else if (i / 4 == 4)
			strlcat(pStr, "RES ", sizeof(pStr));
		else if (i / 4 == 5)
			strlcat(pStr, "COU ", sizeof(pStr));
		else if (i / 4 == 6)
			strlcat(pStr, "PAS ", sizeof(pStr));
		else if (i / 4 == 7)
			strlcat(pStr, "FAI ", sizeof(pStr));
		else if (i / 4 == 8)
			strlcat(pStr, "CHA ", sizeof(pStr));
		else if (i / 4 == 9)
			strlcat(pStr, "AMB ", sizeof(pStr));
		else if (i / 4 == 10)
			strlcat(pStr, "RXS ", sizeof(pStr));
		else if (i / 4 == 11)
			strlcat(pStr, "TXS ", sizeof(pStr));
		else if (i / 4 == 12)
			strlcat(pStr, "RXO ", sizeof(pStr));
		else if (i / 4 == 13)
			strlcat(pStr, "TXO ", sizeof(pStr));
		else if (i / 4 == 14)
			strlcat(pStr, "RXG ", sizeof(pStr));
		else if (i / 4 == 15)
			strlcat(pStr, "TXG ", sizeof(pStr));
		else if (i / 4 == 16)
			strlcat(pStr, "RXR ", sizeof(pStr));
		else if (i / 4 == 17)
			strlcat(pStr, "TXT ", sizeof(pStr));
		else if (i / 4 == 18)
			strlcat(pStr, "RXT ", sizeof(pStr));
		else if (i / 4 == 19)
			strlcat(pStr, "TXR ", sizeof(pStr));

		snprintf(pTmp, sizeof(pTmp), "%2X, %2X, %2X, %2X",
			rBuff[i], rBuff[i + 1], rBuff[i + 2], rBuff[i + 3]);
		strlcat(pStr, pTmp, sizeof(pStr));

		if (i / 4 == 4) {
			if ((rBuff[i + 3] & 0x30) != 0)// RX, RX open check.
				rc = 0;
			else
				rc = 1;

			ts->plat_data->hw_param.ito_test[0] = rBuff[i];
			ts->plat_data->hw_param.ito_test[1] = rBuff[i + 1];
			ts->plat_data->hw_param.ito_test[2] = rBuff[i + 2];
			ts->plat_data->hw_param.ito_test[3] = rBuff[i + 3];
		}
		if (i % 8 == 4) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			memset(pStr, 0x00, sizeof(pStr));
		} else {
			strlcat(pStr, "  ", sizeof(pStr));
		}
	}

err_exit:
	kfree(rBuff);
err_mem:
	if (ts->factory_level)
		slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);
err_set_level:
	return rc;
}

static void slsi_ts_print_frame(struct slsi_ts_data *ts, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize = 7 * (ts->tx_count + 1);

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->tx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->tx_count; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->rx_count; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Rx%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->tx_count; j++) {
			snprintf(pTmp, sizeof(pTmp), " %3d", ts->pFrame[(j * ts->rx_count) + i]);

			if (ts->pFrame[(j * ts->rx_count) + i] < *min)
				*min = ts->pFrame[(j * ts->rx_count) + i];

			if (ts->pFrame[(j * ts->rx_count) + i] > *max)
				*max = ts->pFrame[(j * ts->rx_count) + i];

			strlcat(pStr, pTmp, lsize);
		}
		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}
	kfree(pStr);
}

static int slsi_ts_read_frame(struct slsi_ts_data *ts, u8 type, short *min,
		short *max, bool save_result)
{
	unsigned int readbytes = 0xFF;
	unsigned char *pRead = NULL;
	u8 mode = TYPE_INVALID_DATA;
	int ret = 0;
	int i = 0;
	int j = 0;
	short *temp = NULL;

	input_raw_info(true, &ts->client->dev, "%s: type %d\n", __func__, type);

	/* set data length, allocation buffer memory */
	readbytes = ts->rx_count * ts->tx_count * 2;

	pRead = kzalloc(readbytes, GFP_KERNEL);
	if (!pRead)
		return -ENOMEM;

	/* set OPCODE and data type */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_MUTU_RAW_TYPE, &type, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Set rawdata type failed\n", __func__);
		goto ErrorExit;
	}

	sec_delay(50);

	if (type == TYPE_OFFSET_DATA_SDC) {
		/* excute selftest for real cap offset data, because real cap data is not memory data in normal touch. */
		char para = TO_TOUCH_MODE;

		disable_irq(ts->client->irq);

		ret = execute_selftest(ts, save_result);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: execute_selftest failed\n", __func__);
			enable_irq(ts->client->irq);
			goto ErrorRelease;
		}

		slsi_ts_locked_release_all_finger(ts);
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Set rawdata type failed\n", __func__);
			enable_irq(ts->client->irq);
			goto ErrorRelease;
		}

		enable_irq(ts->client->irq);
	}

	/* read data */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_TOUCH_RAWDATA, pRead, readbytes);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		goto ErrorRelease;
	}

	memset(ts->pFrame, 0x00, readbytes);

	for (i = 0; i < readbytes; i += 2)
		ts->pFrame[i / 2] = pRead[i + 1] + (pRead[i] << 8);

	*min = *max = ts->pFrame[0];

#ifdef DEBUG_MSG
	input_info(true, &ts->client->dev, "%s: 02X%02X%02X readbytes=%d\n", __func__,
			pRead[0], pRead[1], pRead[2], readbytes);
#endif
	slsi_ts_print_frame(ts, min, max);

	temp = kzalloc(readbytes, GFP_KERNEL);
	if (!temp)
		goto ErrorRelease;

	memcpy(temp, ts->pFrame, ts->tx_count * ts->rx_count * 2);
	memset(ts->pFrame, 0x00, ts->tx_count * ts->rx_count * 2);

	for (i = 0; i < ts->tx_count; i++) {
		for (j = 0; j < ts->rx_count; j++)
			ts->pFrame[(j * ts->tx_count) + i] = temp[(i * ts->rx_count) + j];
	}

	kfree(temp);

ErrorRelease:
	/* release data monitory (unprepare AFE data memory) */
	if (ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_MUTU_RAW_TYPE, &mode, 1) < 0) {
		input_err(true, &ts->client->dev, "%s: Set rawdata type failed\n", __func__);
		ret = SEC_ERROR;
	}

ErrorExit:
	kfree(pRead);

	return ret;
}

static void slsi_ts_print_channel(struct slsi_ts_data *ts)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int i = 0, j = 0, k = 0;

	if (!ts->tx_count)
		return;

	pStr = vzalloc(7 * (ts->tx_count + 1));
	if (!pStr)
		return;

	memset(pStr, 0x0, 7 * (ts->tx_count + 1));
	snprintf(pTmp, sizeof(pTmp), " TX");
	strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

	for (k = 0; k < ts->tx_count; k++) {
		snprintf(pTmp, sizeof(pTmp), "    %02d", k);
		strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 7 * (ts->tx_count + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

	for (k = 0; k < ts->tx_count; k++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, 7 * (ts->tx_count + 1));
	snprintf(pTmp, sizeof(pTmp), " | ");
	strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

	for (i = 0; i < (ts->tx_count + ts->rx_count) * 2; i += 2) {
		if (j == ts->tx_count) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			input_raw_info(true, &ts->client->dev, "\n");
			memset(pStr, 0x0, 7 * (ts->tx_count + 1));
			snprintf(pTmp, sizeof(pTmp), " RX");
			strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

			for (k = 0; k < ts->tx_count; k++) {
				snprintf(pTmp, sizeof(pTmp), "    %02d", k);
				strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
			}

			input_raw_info(true, &ts->client->dev, "%s\n", pStr);

			memset(pStr, 0x0, 7 * (ts->tx_count + 1));
			snprintf(pTmp, sizeof(pTmp), " +");
			strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

			for (k = 0; k < ts->tx_count; k++) {
				snprintf(pTmp, sizeof(pTmp), "------");
				strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
			}
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);

			memset(pStr, 0x0, 7 * (ts->tx_count + 1));
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
		} else if (j && !(j % ts->tx_count)) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			memset(pStr, 0x0, 7 * (ts->tx_count + 1));
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));
		}

		snprintf(pTmp, sizeof(pTmp), " %5d", ts->pFrame[j]);
		strlcat(pStr, pTmp, 7 * (ts->tx_count + 1));

		j++;
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	vfree(pStr);
}

static int slsi_ts_read_channel(struct slsi_ts_data *ts, u8 type,
				short *min, short *max, bool save_result)
{
	unsigned char *pRead = NULL;
	u8 mode = TYPE_INVALID_DATA;
	int ret = 0;
	int ii = 0;
	int jj = 0;
	unsigned int data_length = (ts->tx_count + ts->rx_count) * 2;
	u8 w_data;

	input_raw_info(true, &ts->client->dev, "%s: type %d\n", __func__, type);

	pRead = kzalloc(data_length, GFP_KERNEL);
	if (!pRead)
		return -ENOMEM;

	/* set OPCODE and data type */
	w_data = type;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SELF_RAW_TYPE, &w_data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Set rawdata type failed\n", __func__);
		goto out_read_channel;
	}

	sec_delay(50);

	if (type == TYPE_OFFSET_DATA_SDC) {
		/* execute selftest for real cap offset data,
		 * because real cap data is not memory data in normal touch.
		 */
		char para = TO_TOUCH_MODE;
		disable_irq(ts->client->irq);
		ret = execute_selftest(ts, save_result);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: execute_selftest failed!\n", __func__);
			enable_irq(ts->client->irq);
			goto err_read_data;
		}

		slsi_ts_locked_release_all_finger(ts);

		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);
			enable_irq(ts->client->irq);
			goto err_read_data;
		}
		enable_irq(ts->client->irq);
		/* end */
	}
	/* read data */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_TOUCH_SELF_RAWDATA, pRead, data_length);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		goto err_read_data;
	}

	/* clear all pFrame data */
	memset(ts->pFrame, 0x00, data_length);

	for (ii = 0; ii < data_length; ii += 2) {
		ts->pFrame[jj] = ((pRead[ii] << 8) | pRead[ii + 1]);

		if (ii == 0)
			*min = *max = ts->pFrame[jj];

		*min = min(*min, ts->pFrame[jj]);
		*max = max(*max, ts->pFrame[jj]);

		jj++;
	}

	slsi_ts_print_channel(ts);

err_read_data:
	/* release data monitory (unprepare AFE data memory) */
	if (ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SELF_RAW_TYPE, &mode, 1) < 0) {
		input_err(true, &ts->client->dev, "%s: Set rawdata type failed\n", __func__);
		ret = SEC_ERROR;
	}

out_read_channel:
	kfree(pRead);

	return ret;
}

static void slsi_ts_read_raw_data(struct slsi_ts_data *ts,
		struct sec_cmd_data *sec, struct slsi_ts_test_mode *mode)
{
	int ii;
	int ret;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	char *item_name = "NULL";

	switch (mode->type) {
	case TYPE_OFFSET_DATA_SDC:
		item_name = "CM_OFFSET_MODULE";
		break;
	case TYPE_RAW_DATA:
		item_name = "CM_DELTA";
		break;
	case TYPE_OFFSET_DATA_SEC:
		item_name = "CM_OFFSET_SET";
		break;
	default:
		break;
	}

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_alloc_mem;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	input_raw_info(true, &ts->client->dev, "%s: %d, %s\n",
			__func__, mode->type, mode->allnode ? "ALL" : "");

	ret = slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix tmode\n",
				__func__);
		goto error_test_fail;
	}

	if (mode->frame_channel)
		ret = slsi_ts_read_channel(ts, mode->type, &mode->min, &mode->max, true);
	else
		ret = slsi_ts_read_frame(ts, mode->type, &mode->min, &mode->max, true);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read frame\n",
				__func__);
		slsi_ts_release_tmode(ts);
		goto error_test_fail;
	}

	if (mode->allnode) {
		if (mode->frame_channel) {
			for (ii = 0; ii < (ts->rx_count + ts->tx_count); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		} else {
			for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		}
	} else {
		snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	ret = slsi_ts_release_tmode(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to release tmode\n",
				__func__);
		goto error_test_fail;
	}

	if (!sec)
		goto out_rawdata;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING && (!mode->frame_channel))
		sec_cmd_set_cmd_result_all(sec, buff,
				strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN), item_name);

	sec->cmd_state = SEC_CMD_STATUS_OK;

out_rawdata:
	kfree(buff);

	slsi_ts_locked_release_all_finger(ts);

	return;

error_test_fail:
error_power_state:
	kfree(buff);
error_alloc_mem:
	if (!sec)
		return;

	snprintf(temp, SEC_CMD_STR_LEN, "NG");
	sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING && (!mode->frame_channel))
		sec_cmd_set_cmd_result_all(sec, temp, SEC_CMD_STR_LEN, item_name);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	slsi_ts_locked_release_all_finger(ts);

	return;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	mutex_lock(&ts->modechange);
	retval = slsi_ts_firmware_update_on_hidden_menu(ts, sec->cmd_param[0]);
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	mutex_unlock(&ts->modechange);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "SE%02X%02X%02X%02X",
			ts->plat_data->img_version_of_bin[0], ts->plat_data->img_version_of_bin[1],
			ts->plat_data->img_version_of_bin[2], ts->plat_data->img_version_of_bin[3]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	char model[16] = { 0 };
	int ret;
	u8 fw_ver[4];

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_IMG_VERSION, fw_ver, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: firmware version read error\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	snprintf(buff, sizeof(buff), "SE%02X%02X%02X%02X",
			fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
	snprintf(model, sizeof(model), "SE%02X%02X",
		fw_ver[0], fw_ver[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[22] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "SE_%02X%02X",
			ts->plat_data->config_version_of_ic[2], ts->plat_data->config_version_of_ic[3]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef TCLM_CONCEPT
static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[50] = { 0 };

	sec_cmd_set_default_result(sec);

	/* fixed tune version will be saved at execute autotune */
	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
		ts->tdata->nvdata.cal_count, ts->tdata->nvdata.tune_fix_ver, ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].f_name,
		(ts->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
		ts->tdata->cal_pos_hist_last3[0], ts->tdata->cal_pos_hist_last3[1],
		ts->tdata->cal_pos_hist_last3[2], ts->tdata->cal_pos_hist_last3[3],
		ts->tdata->cal_pos_hist_last3[4], ts->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_external_factory(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[22] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->tdata->external_factory = true;
	snprintf(buff, sizeof(buff), "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[20] = { 0 };
	char threshold[2] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto err;
	}

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_TOUCH_MODE_FOR_THRESHOLD, threshold, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: threshold write type failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		goto err;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_TOUCH_THRESHOLD, threshold, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read threshold fail!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto err;
	}

	input_info(true, &ts->client->dev, "0x%02X, 0x%02X\n",
			threshold[0], threshold[1]);

	snprintf(buff, sizeof(buff), "%d", (threshold[0] << 8) | threshold[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return;
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[3] = { 0 };
	int ret = 0;

	ret = ts->plat_data->stop_device(ts);

	if (ret == 0)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[3] = { 0 };
	int ret = 0;

	ret = ts->plat_data->start_device(ts);

	if (!ts->plat_data->enabled) {
		ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
	}

	if (ret == 0)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (strncmp(buff, "OK", 2) == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	strncpy(buff, "SEC", sizeof(buff));
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	if (ts->plat_data->img_version_of_ic[0] == 0x09)
		strncpy(buff, "Y661", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 0x10)
		strncpy(buff, "Y761", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 0x17)
		strncpy(buff, "Y771", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 0x23)
		strncpy(buff, "Y79A", sizeof(buff));
	else if (ts->plat_data->img_version_of_ic[0] == 0x28)
		strncpy(buff, "Y792", sizeof(buff));
	else
		strncpy(buff, "N/A", sizeof(buff));

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	char wet_mode_info = 0;
	int ret;

	sec_cmd_set_default_result(sec);

	sec_delay(300);
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_WET_MODE, &wet_mode_info, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c fail!, %d\n", __func__, ret);
		goto NG;
	}

	snprintf(buff, sizeof(buff), "%d", wet_mode_info);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->tx_count);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->rx_count);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static u32 checksum_sum(struct slsi_ts_data *ts, u8 *chunk_data, u32 size)
{
	u32 data_32 = 0;
	u32 checksum = 0;
	u8 *fd = chunk_data;
	int i;

	for (i = 0; i < size/4; i++) {
		data_32 = (((fd[(i*4)+0]&0xFF)<<0) | ((fd[(i*4)+1]&0xFF)<<8) |
					((fd[(i*4)+2]&0xFF)<<16) | ((fd[(i*4)+3]&0xFF)<<24));
		checksum += data_32;
	}

	checksum &= 0xFFFFFFFF;

	input_info(true, &ts->client->dev, "%s: checksum = [%x]\n", __func__, checksum);

	return checksum;
}

static u32 get_bin_checksum(struct slsi_ts_data *ts, const u8 *data, size_t size)
{

	int i;
	fw_header *fw_hd;
	fw_chunk *fw_ch;
	u8 *fd = (u8 *)data;
	u32 data_32;
	u32 checksum = 0;
	u32 chunk_checksum[3];
	u32 img_checksum;

	fw_hd = (fw_header *)fd;
	fd += sizeof(fw_header);

	if (fw_hd->signature != SLSI_TS_FW_HEADER_SIGN) {
		input_err(true, &ts->client->dev, "%s: firmware header error = %08X\n", __func__, fw_hd->signature);
		return 0;
	}

	for (i = 0; i < (fw_hd->totalsize/4); i++) {
		data_32 = (((data[(i*4)+0]&0xFF)<<0) | ((data[(i*4)+1]&0xFF)<<8) |
					((data[(i*4)+2]&0xFF)<<16) | ((data[(i*4)+3]&0xFF)<<24));
		checksum += data_32;
	}

	input_info(true, &ts->client->dev, "%s: fw_hd->totalsize = [%d] checksum = [%x]\n",
				__func__, fw_hd->totalsize, checksum);

	checksum &= 0xFFFFFFFF;
	if (checksum != 0) {
		input_err(true, &ts->client->dev, "%s: Checksum fail! = %08X\n", __func__, checksum);
		return 0;
	}

	for (i = 0; i < fw_hd->num_chunk; i++) {
		fw_ch = (fw_chunk *)fd;

		input_info(true, &ts->client->dev, "%s: [%d] 0x%08X, 0x%08X, 0x%08X, 0x%08X\n", __func__, i,
				fw_ch->signature, fw_ch->addr, fw_ch->size, fw_ch->reserved);

		if (fw_ch->signature != SLSI_TS_FW_CHUNK_SIGN) {
			input_err(true, &ts->client->dev, "%s: firmware chunk error = %08X\n",
						__func__, fw_ch->signature);
			return 0;
		}
		fd += sizeof(fw_chunk);

		checksum = checksum_sum(ts, fd, fw_ch->size);
		chunk_checksum[i] = checksum;
		fd += fw_ch->size;
	}

	img_checksum = chunk_checksum[0] + chunk_checksum[1];

	return img_checksum;
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	char csum_result[4] = { 0 };
	char data[5] = { 0 };
	u8 temp;
	int ret, i;

	u32 ic_checksum;
	u32 img_checksum;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	u8 fw_ver[4];

	sec_cmd_set_default_result(sec);
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		goto err;
	}

	disable_irq(ts->client->irq);

	ts->plat_data->power(&ts->client->dev, false);
	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_OFF;
	sec_delay(50);

	ts->plat_data->power(&ts->client->dev, true);
	ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;
	sec_delay(70);

	ret = slsi_ts_wait_for_ready(ts, SLSI_TS_ACK_BOOT_COMPLETE, NULL, 0, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: boot complete failed\n", __func__);
		goto err_init;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_FIRMWARE_INTEGRITY, &data[0], 1);
	if (ret < 0 || (data[0] != 0x80)) {
		input_err(true, &ts->client->dev, "%s: firmware integrity failed, ret:%d, data:%X\n",
				__func__, ret, data[0]);
		goto err_init;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_BOOT_STATUS, &data[1], 1);
	if (ret < 0 || (data[1] != SLSI_TS_STATUS_APP_MODE)) {
		input_err(true, &ts->client->dev, "%s: boot status failed, ret:%d, data:%X\n", __func__, ret, data[0]);
		goto err_init;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_TS_STATUS, &data[2], 4);
	if (ret < 0 || (data[3] == TOUCH_SYSTEM_MODE_FLASH)) {
		input_err(true, &ts->client->dev, "%s: touch status failed, ret:%d, data:%X\n", __func__, ret, data[3]);
		goto err_init;
	}

	ts->plat_data->init(ts);

	/* Sense_on */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on ret:%d\n", __func__, ret);
		goto err_init;
	}

	enable_irq(ts->client->irq);

	input_err(true, &ts->client->dev, "%s: data[0]:%X, data[1]:%X, data[3]:%X\n", __func__, data[0], data[1], data[3]);

	temp = DO_FW_CHECKSUM | DO_PARA_CHECKSUM;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_GET_CHECKSUM, &temp, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: send get_checksum_cmd fail!\n", __func__);
		goto err;
	}

	sec_delay(20);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_GET_CHECKSUM, csum_result, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read get_checksum result fail!\n", __func__);
		goto err;
	}

	ic_checksum = ((csum_result[0] & 0xFF) << 24) | ((csum_result[1] & 0xFF) << 16) |
			((csum_result[2] & 0xFF) << 8) | ((csum_result[3] & 0xFF) << 0);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_IMG_VERSION, fw_ver, 4);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: firmware version read error\n", __func__);

	for (i = 0; i < 4; i++) {
		if (ts->plat_data->img_version_of_bin[i] !=  fw_ver[i]) {
			input_err(true, &ts->client->dev, "%s: do not matched version info [%d]:[%x]!=[%x] and skip!\n",
						__func__, i, ts->plat_data->img_version_of_bin[i], fw_ver[i]);
			goto out;
		}
	}

	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);

	if (request_firmware(&fw_entry, fw_path, &ts->client->dev) !=  0) {
			input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
			goto err;
	}

	img_checksum = get_bin_checksum(ts, fw_entry->data, fw_entry->size);

	release_firmware(fw_entry);

	input_info(true, &ts->client->dev, "%s: img_checksum=[0x%X], ic_checksum=[0x%X]\n",
					__func__, img_checksum, ic_checksum);

	if (img_checksum != ic_checksum) {
		input_err(true, &ts->client->dev, "%s: img_checksum=[0x%X] != ic_checksum=[0x%X]!!!\n",
						__func__, img_checksum, ic_checksum);
		goto err;
	}
out:
	input_info(true, &ts->client->dev, "%s: checksum = %02X\n", __func__, ic_checksum);
	snprintf(buff, sizeof(buff), "%02X", ic_checksum);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
err_init:
	enable_irq(ts->client->irq);
err:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void run_jitter_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[64] = { 0 };
	int ret = 0;
	int retry = SEC_TS_WAIT_RETRY_CNT / 2;
	u8 mode[3] = { 0x2F, 0x00, 0xC6 };
	u8 test[2] = { 0x00, 0x32 };
	u8 data[8] = { 0 };
	char para;
	short jitter_max = 0;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	disable_irq(ts->client->irq);

	slsi_ts_locked_release_all_finger(ts);

	para = TO_SELFTEST_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		goto jitter_ng;

	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0)
		goto jitter_ng;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_P2P_MODE, mode, sizeof(mode));
	if (ret < 0)
		goto jitter_ng;

	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_P2P_TEST, test, sizeof(test));
	if (ret < 0)
		goto jitter_ng;

	sec_delay(600);

	do {
		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_EVENT, data, sizeof(data));
		if (ret < 0)
			goto jitter_ng;

		if (((data[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_VENDOR_INFO) {
			if (data[1] == 0x42) {
				jitter_max = data[2] << 8 | data[3];
				break;
			}
		}
		sec_delay(20);
	} while (retry--);

	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to set nomal mode\n", __func__);

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%d", jitter_max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;

jitter_ng:
	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to set nomal mode\n", __func__);

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return;
}

static int get_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	int ii;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	int tx_max = 0;
	int rx_max = 0;

	for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
		if ((ii + 1) % (ts->tx_count) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap_tx = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap_tx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);
			tx_max = max(tx_max, node_gap_tx);
		}

		if (ii < (ts->rx_count - 1) * ts->tx_count) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->tx_count])
				node_gap_rx = 100 - (ts->pFrame[ii + ts->tx_count] * 100 / ts->pFrame[ii]);
			else
				node_gap_rx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->tx_count]);
			rx_max = max(rx_max, node_gap_rx);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA_TX");
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA_RX");
		snprintf(buff, sizeof(buff), "%d,%d", 0, max(tx_max, rx_max));
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "GAP_DATA");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	return 0;
}

static void get_gap_data_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	int node_gap_tx = 0;
	int node_gap_rx = 0;

	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
		node_gap = node_gap_tx = node_gap_rx = 0;

		if ((ii + 1) % (ts->tx_count) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap_tx = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap_tx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);
		}

		if (ii < (ts->rx_count - 1) * ts->tx_count) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->tx_count])
				node_gap_rx = 100 - (ts->pFrame[ii + ts->tx_count] * 100 / ts->pFrame[ii]);
			else
				node_gap_rx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->tx_count]);
		}
		node_gap = max(node_gap_tx, node_gap_rx);
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
		strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
		if ((ii + 1) % (ts->tx_count) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
		if (ii < (ts->rx_count - 1) * ts->tx_count) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->tx_count])
				node_gap = 100 - (ts->pFrame[ii + ts->tx_count] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->tx_count]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static int get_self_channel_data(void *device_data, u8 type)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	int ii;
	short tx_min = 0;
	short rx_min = 0;
	short tx_max = 0;
	short rx_max = 0;
	char *item_name = "NULL";
	char temp[SEC_CMD_STR_LEN] = { 0 };

	switch (type) {
	case TYPE_OFFSET_DATA_SDC:
		item_name = "CS_OFFSET_MODULE";
		break;
	case TYPE_RAW_DATA:
		item_name = "CS_DELTA";
		break;
	case TYPE_OFFSET_DATA_SEC:
		item_name = "CS_OFFSET_SET";
		break;
	default:
		break;
	}

	for (ii = 0; ii < ts->tx_count; ii++) {
		if (ii == 0)
			tx_min = tx_max = ts->pFrame[ii];

		tx_min = min(tx_min, ts->pFrame[ii]);
		tx_max = max(tx_max, ts->pFrame[ii]);
	}
	snprintf(buff, sizeof(buff), "%d,%d", tx_min, tx_max);
	snprintf(temp, sizeof(temp), "%s%s", item_name, "_TX");
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, sizeof(buff), temp);

	memset(temp, 0x00, SEC_CMD_STR_LEN);

	for (ii = ts->tx_count; ii < ts->tx_count + ts->rx_count; ii++) {
		if (ii == ts->tx_count)
			rx_min = rx_max = ts->pFrame[ii];

		rx_min = min(rx_min, ts->pFrame[ii]);
		rx_max = max(rx_max, ts->pFrame[ii]);
	}
	snprintf(buff, sizeof(buff), "%d,%d", rx_min, rx_max);
	snprintf(temp, sizeof(temp), "%s%s", item_name, "_RX");
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, sizeof(buff), temp);

	return 0;
}

static void get_cmoffset_set_proximity(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };
	u8 buf[17] = { 0 };
	u8 test_result = 0;
	int ret;

	sec_cmd_set_default_result(sec);

	buf[0] = 0x01;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_READ_CALIBRATION_REPORT, &buf[0], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write SLSI_TS_READ_CALIBRATION_REPORT 1(%d)\n", __func__, ret);
		goto NG;
	}
	sec_delay(30);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_CALIBRATION_REPORT, buf, 17);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read SLSI_TS_READ_CALIBRATION_REPORT(%d)\n", __func__, ret);
		goto NG;
	}
	test_result = buf[0];

	sec_delay(30);
	buf[0] = 0x00;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_READ_CALIBRATION_REPORT, &buf[0], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write SLSI_TS_READ_CALIBRATION_REPORT 0(%d)\n", __func__, ret);
		goto NG;
	}

	input_info(true, &ts->client->dev, "%s: 0x%X, A1(%d,%d), A2(%d,%d), A3(%d,%d), A4(%d,%d)\n",
				__func__, test_result, (s16)(buf[1] << 8 | buf[2]), (s16)(buf[3] << 8 | buf[4]),
				(s16)(buf[5] << 8 | buf[6]), (s16)(buf[7] << 8 | buf[8]), (s16)(buf[9] << 8 | buf[10]),
				(s16)(buf[11] << 8 | buf[12]), (s16)(buf[13]<<8 | buf[14]), (s16)(buf[15] << 8 | buf[16]));

	snprintf(buff, sizeof(buff), "%d", test_result);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_OFFSET_SET_PROXIMITY");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_OFFSET_SET_PROXIMITY");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}

static void run_cmoffset_set_proximity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int ret = 0;
	int ii;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.allnode = TEST_MODE_ALL_NODE;

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		input_err(true, &ts->client->dev, "%s: fail to kzalloc buff\n", __func__);
		goto error_alloc_mem;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	input_raw_info(true, &ts->client->dev, "%s: called\n", __func__);

	slsi_ts_ear_detect_enable(ts, SLSI_TS_EAR_DETECT_MODE_FULL);

	sec_delay(30);

	ret = slsi_ts_read_frame(ts, TYPE_OFFSET_DATA_SEC, &mode.min, &mode.max, false);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read frame\n", __func__);
		goto error_test_fail;
	}
	sec_delay(100);

	slsi_ts_ear_detect_enable(ts, SLSI_TS_EAR_DETECT_MODE_OFF);

	for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
		strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);
	slsi_ts_locked_release_all_finger(ts);

	return;

error_test_fail:
	sec_delay(100);

	slsi_ts_ear_detect_enable(ts, SLSI_TS_EAR_DETECT_MODE_OFF);
error_power_state:
	kfree(buff);
error_alloc_mem:
	snprintf(temp, SEC_CMD_STR_LEN, "NG");
	sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	slsi_ts_locked_release_all_finger(ts);

	return;
}

static int slsi_ts_read_rawp2p_data(struct slsi_ts_data *ts,
		struct sec_cmd_data *sec, struct slsi_ts_test_mode *mode)
{
	int ii;
	int ret = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	char para = TO_TOUCH_MODE;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	buff = kzalloc(ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_alloc_mem;

	input_info(true, &ts->client->dev, "%s: %d, %s\n",
			__func__, mode->type, mode->allnode ? "ALL" : "");

	disable_irq(ts->client->irq);

	ret = slsi_ts_p2p_tmode(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p tmode\n",
				__func__);
		goto error_tmode_fail;
	}

	ret = execute_p2ptest(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p test\n",
				__func__);
		goto error_test_fail;
	}

	if (mode->frame_channel)
		ret = slsi_ts_read_channel(ts, mode->type, &mode->min, &mode->max, true);
	else
		ret = slsi_ts_read_frame(ts, mode->type, &mode->min, &mode->max, true);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read frame\n",
				__func__);
		goto error_test_fail;
	}

	if (mode->allnode) {
		if (mode->frame_channel) {
			for (ii = 0; ii < (ts->rx_count + ts->tx_count); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		} else {
			for (ii = 0; ii < (ts->rx_count * ts->tx_count); ii++) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts->pFrame[ii]);
				strlcat(buff, temp, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN);

				memset(temp, 0x00, SEC_CMD_STR_LEN);
			}
		}
	} else {
		snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	if (!sec)
		goto out_rawdata;

	slsi_ts_locked_release_all_finger(ts);
	sec_delay(30);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_count * ts->rx_count * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out_rawdata:
	enable_irq(ts->client->irq);
	kfree(buff);

	return ret;
error_test_fail:
error_tmode_fail:
	kfree(buff);
	enable_irq(ts->client->irq);
error_alloc_mem:
error_power_state:
	if (!sec)
		return ret;

	snprintf(temp, SEC_CMD_STR_LEN, "NG");
	sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return ret;
}

static int slsi_ts_read_rawp2p_data_all(struct slsi_ts_data *ts,
		struct sec_cmd_data *sec, struct slsi_ts_test_mode *mode)
{
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char para = TO_TOUCH_MODE;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	disable_irq(ts->client->irq);
	ret = slsi_ts_p2p_tmode(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p tmode\n",
				__func__);
		goto error_tmode_fail;
	}

	ret = execute_p2ptest(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p test\n",
				__func__);
		goto error_test_fail;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", ts->cm_raw_set_avg_min, ts->cm_raw_set_avg_max);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_RAW_SET_P2P_AVG");
		snprintf(buff, sizeof(buff), "%d,%d", 0, ts->cm_raw_set_p2p);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_RAW_SET_P2P_MAX");
		snprintf(buff, sizeof(buff), "%d,%d", 0, ts->cm_raw_set_p2p_gap_y);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	slsi_ts_locked_release_all_finger(ts);

	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

	enable_irq(ts->client->irq);

	return ret;

error_test_fail:
	slsi_ts_locked_release_all_finger(ts);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

error_tmode_fail:
	enable_irq(ts->client->irq);
error_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_RAW_SET_AVG");
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CM_RAW_SET_P2P");
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	}
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return ret;
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_reference_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_decoded_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_DECODED_DATA;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_delta_cm_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_SIGNAL_DATA;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_raw_p2p_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));

	/* both mutual, self for factory_cmd_result_all*/
	slsi_ts_read_rawp2p_data_all(ts, sec, &mode);
}

static void run_raw_p2p_avg_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA_P2P_AVG;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_rawp2p_data(ts, sec, &mode);
}

static void run_raw_p2p_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA_P2P_DIFF;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_rawp2p_data(ts, sec, &mode);
}

static void run_raw_p2p_node_gap_y_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA_NODE_GAP_Y;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_rawp2p_data(ts, sec, &mode);
}

/* self reference : send TX power in TX channel, receive in TX channel */
static void run_self_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_reference_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SEC;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_OFFSET_DATA_SDC;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_raw_data(ts, sec, &mode);
}

static void run_self_raw_p2p_avg_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA_P2P_AVG;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_rawp2p_data(ts, sec, &mode);
}

static void run_self_raw_p2p_diff_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct slsi_ts_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct slsi_ts_test_mode));
	mode.type = TYPE_RAW_DATA_P2P_DIFF;
	mode.frame_channel = TEST_MODE_READ_CHANNEL;
	mode.allnode = TEST_MODE_ALL_NODE;

	slsi_ts_read_rawp2p_data(ts, sec, &mode);
}

void slsi_ts_get_saved_cmoffset(struct slsi_ts_data *ts)
{
	int rc;
	u8 *rBuff = NULL;
	int i;
	int result_size = SLSI_TS_SELFTEST_REPORT_SIZE + ts->tx_count * ts->rx_count * 2;
	short min, max;

	input_raw_info(true, &ts->client->dev, "%s:\n", __func__);

	rBuff = kzalloc(result_size, GFP_KERNEL);
	if (!rBuff)
		return;

	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_SELFTEST_RESULT, rBuff, result_size);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_exit;
	}

	memset(ts->pFrame, 0x00, result_size - SLSI_TS_SELFTEST_REPORT_SIZE);

	for (i = 0; i < result_size - SLSI_TS_SELFTEST_REPORT_SIZE; i += 2)
		ts->pFrame[i / 2] = (rBuff[SLSI_TS_SELFTEST_REPORT_SIZE + i + 1] << 8)
				| rBuff[SLSI_TS_SELFTEST_REPORT_SIZE + i];

	min = max = ts->pFrame[0];

	slsi_ts_print_frame(ts, &min, &max);

err_exit:
	kfree(rBuff);
}

/*
 * slsi_ts_run_rawdata_all : read all raw data
 *
 * when you want to read full raw data (full_read : true)
 * "mutual/self 3, 5, 29, 1, 19" data will be saved in log
 *
 * otherwise, (full_read : false, especially on boot time)
 * only "mutual 3, 5, 29" data will be saved in log
 */
void slsi_ts_run_rawdata_all(struct slsi_ts_data *ts, bool full_read)
{
	short min, max;
	int ret, i, read_num;
	u8 test_type[5] = {TYPE_AMBIENT_DATA, TYPE_DECODED_DATA,
		TYPE_SIGNAL_DATA, TYPE_OFFSET_DATA_SEC, TYPE_OFFSET_DATA_SDC};

	ts->tsp_dump_lock = 1;
	input_raw_data_clear();
	input_raw_info(true, &ts->client->dev,
			"%s: start (noise:%d, wet:%d)##\n",
			__func__, ts->plat_data->touch_noise_status, ts->plat_data->wet_mode);

	ret = slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix tmode\n",
				__func__);
		goto out;
	}

	if (full_read) {
		read_num = 5;
	} else {
		read_num = 3;
		test_type[read_num - 1] = TYPE_OFFSET_DATA_SDC;
	}

	for (i = 0; i < read_num; i++) {
		ret = slsi_ts_read_frame(ts, test_type[i], &min, &max, false);
		if (ret < 0) {
			input_raw_info(true, &ts->client->dev,
					"%s: mutual %d : error ## ret:%d\n",
					__func__, test_type[i], ret);
			goto out;
		} else {
			input_raw_info(true, &ts->client->dev,
					"%s: mutual %d : Max/Min %d,%d ##\n",
					__func__, test_type[i], max, min);
		}
		sec_delay(20);

		if (full_read) {
			ret = slsi_ts_read_channel(ts, test_type[i], &min, &max, false);
			if (ret < 0) {
				input_raw_info(true, &ts->client->dev,
						"%s: self %d : error ## ret:%d\n",
						__func__, test_type[i], ret);
				goto out;
			} else {
				input_raw_info(true, &ts->client->dev,
						"%s: self %d : Max/Min %d,%d ##\n",
						__func__, test_type[i], max, min);
			}
			sec_delay(20);
		}
	}

	slsi_ts_release_tmode(ts);

	if (full_read)
		slsi_ts_get_saved_cmoffset(ts);

out:
	run_cmoffset_set_proximity_read_all(&ts->sec);

	input_raw_info(true, &ts->client->dev, "%s: ito : %02X %02X %02X %02X\n",
			__func__, ts->plat_data->hw_param.ito_test[0], ts->plat_data->hw_param.ito_test[1],
			ts->plat_data->hw_param.ito_test[2], ts->plat_data->hw_param.ito_test[3]);

	input_raw_info(true, &ts->client->dev, "%s: done (noise:%d, wet:%d)##\n",
			__func__, ts->plat_data->touch_noise_status, ts->plat_data->wet_mode);
	ts->tsp_dump_lock = 0;

	slsi_ts_locked_release_all_finger(ts);
}

static void run_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s: already checking now\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	slsi_ts_run_rawdata_all(ts, true);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	struct sec_ts_test_result *result;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char r_data[1] = { 0 };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	r_data[0] = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);
	if (r_data[0] == 0xFF)
		r_data[0] = 0;

	result = (struct sec_ts_test_result *)r_data;

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		result->assy_result = sec->cmd_param[1];
		if (result->assy_count < 3)
			result->assy_count++;
	}

	if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		result->module_result = sec->cmd_param[1];
		if (result->module_count < 3)
			result->module_count++;
	}

	input_info(true, &ts->client->dev, "%s: %d, %d, %d, %d, 0x%X\n", __func__,
			result->module_result, result->module_count,
			result->assy_result, result->assy_count, result->data[0]);

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	memset(buff, 0x00, SEC_CMD_STR_LEN);
	buff[2] = *result->data;

	input_info(true, &ts->client->dev, "%s: command (1)%X, (2)%X: %X\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1], buff[2]);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: nvm write failed. ret: %d\n", __func__, ret);

	sec_delay(20);

	ts->nv = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct sec_ts_test_result *result;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	memset(buff, 0x00, SEC_CMD_STR_LEN);
	buff[0] = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);
	if (buff[0] == 0xFF) {
		set_tsp_nvm_data_clear(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);
		buff[0] = 0;
	}

	ts->nv = buff[0];

	result = (struct sec_ts_test_result *)buff;

	input_info(true, &ts->client->dev, "%s: [0x%X][0x%X] M:%d, M:%d, A:%d, A:%d\n",
			__func__, *result->data, buff[0],
			result->module_result, result->module_count,
			result->assy_result, result->assy_count);

	snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
			result->module_result == 0 ? "NONE" :
			result->module_result == 1 ? "FAIL" :
			result->module_result == 2 ? "PASS" : "A",
			result->module_count,
			result->assy_result == 0 ? "NONE" :
			result->assy_result == 1 ? "FAIL" :
			result->assy_result == 2 ? "PASS" : "A",
			result->assy_count);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void clear_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	memset(buff, 0x00, SEC_CMD_STR_LEN);
	buff[0] = SLSI_TS_NVM_OFFSET_FAC_RESULT;
	buff[1] = 0x00;
	buff[2] = 0x00;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: nvm write failed. ret: %d\n", __func__, ret);

	sec_delay(20);

	ts->nv = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[3] = { 0 };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	buff[2] = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT);

	input_info(true, &ts->client->dev, "%s: disassemble count is #1 %d\n", __func__, buff[2]);

	if (buff[2] == 0xFF)
		buff[2] = 0;

	if (buff[2] < 0xFE)
		buff[2]++;

	/* Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 * buff[2] : write data
	 */
	buff[0] = SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT;
	buff[1] = 0;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: nvm write failed. ret: %d\n", __func__, ret);

	sec_delay(20);

	memset(buff, 0x00, 3);
	buff[0] = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
	input_info(true, &ts->client->dev, "%s: check disassemble count: %d\n", __func__, buff[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	memset(buff, 0x00, SEC_CMD_STR_LEN);
	buff[0] = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
	if (buff[0] == 0xFF) {
		set_tsp_nvm_data_clear(ts, SLSI_TS_NVM_OFFSET_DISASSEMBLE_COUNT);
		buff[0] = 0;
	}

	input_info(true, &ts->client->dev, "%s: read disassemble count: %d\n", __func__, buff[0]);

	snprintf(buff, sizeof(buff), "%d", buff[0]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

/*
 * bit		| [0]	[1]	..	[8]	[9]	..	[16]	 [17]	..	[24]	..	[31]
 * byte[0]	| TX0	TX1	..	TX8	TX9	..	TX16 RX0	..	RX7	..	RX14
 * byte[1]	| RX15	RX16	..	RX24	RX31	..	RX34 F0 F1 F2	..
 */

#define OPEN_SHORT_TEST		1
#define CRACK_TEST			2
#define BRIDGE_SHORT_TEST	3
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	char tempn[40] = {0};
	char tempv[25] = {0};
	int rc;
	int size = SLSI_TS_SELFTEST_REPORT_SIZE + (ts->tx_count * ts->rx_count
			+ 6 * (ts->tx_count + ts->rx_count)) * 2 + 6;
	int self_data_offset = SLSI_TS_SELFTEST_REPORT_SIZE + (ts->tx_count * ts->rx_count
			+ 2 * (ts->tx_count + ts->rx_count)) * 2 + 6;
	unsigned int data_length = (ts->tx_count + ts->rx_count) * 2;
	char para = TO_TOUCH_MODE;
	u8 *rBuff = NULL;
	int ii, jj;
	u8 data[32] = { 0 };
	int len = 0;
	int delay = 0;
	int checklen = 0;
	int start_point = 0;
	int sum = 0;
	char test[32];
	char result[32];
	u64 temp_result;
	u8 *test_result_buff;

	sec_cmd_set_default_result(sec);

	/* Remove useless CRACK_TEST ( ~ y771-davinci) */
	if (sec->cmd_param[0] == CRACK_TEST) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[0] == OPEN_SHORT_TEST && sec->cmd_param[1] == 0) {
		input_err(true, &ts->client->dev,
				"%s: %s: seperate cm1 test open / short test result\n", __func__, buff);

		snprintf(buff, sizeof(buff), "%s", "CONT");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		return;
	}

	test_result_buff = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!test_result_buff) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	rBuff = kzalloc(size, GFP_KERNEL);
	if (!rBuff) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(test_result_buff);
		return;
	}

	memset(rBuff, 0x00, size);
	memset(data, 0x00, 32);

	disable_irq(ts->client->irq);

	/*
	 * old version remaining
	 * old version is not send parameter
	 */
	if (sec->cmd_param[0] == 0) {
		rc = execute_selftest(ts, false);

		slsi_ts_locked_release_all_finger(ts);

		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
		enable_irq(ts->client->irq);

		if (rc > 0) {
			snprintf(buff, sizeof(buff), "OK");
			sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
			sec->cmd_state = SEC_CMD_STATUS_OK;
		} else {
			snprintf(buff, sizeof(buff), "NG");
			sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		}

		input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
		kfree(rBuff);
		kfree(test_result_buff);
		return;
	}

	input_info(true, &ts->client->dev, "%s: set power mode to test mode\n", __func__);
	data[0] = 0x02;
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: set test mode failed\n", __func__);
		goto err_trx_short;
	}

	input_info(true, &ts->client->dev, "%s: clear event stack\n", __func__);
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		goto err_trx_short;
	}

	if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
			sec->cmd_param[1] == CHECK_ONLY_OPEN_TEST) {
		data[0] = 0xB7;
		len = 1;
		delay = 700;
		checklen = 8;
		start_point = 0;
	} else if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
			sec->cmd_param[1] == CHECK_ONLY_SHORT_TEST) {
		data[0] = 0xB7;
		len = 1;
		delay = 700;
		checklen = 8 * 4;
		start_point = 8;
	} else if (sec->cmd_param[0] == CRACK_TEST) {
		data[0] = 0x81;
		data[1] = 0x01;
		len = 2;
		delay = 200;
		checklen = 8;
		start_point = 0;
	} else if (sec->cmd_param[0] == BRIDGE_SHORT_TEST) {
		data[0] = 0x81;
		data[1] = 0x02;
		len = 2;
		delay = 1000;
		checklen = 8;
		start_point = 0;
	}

	/* set Factory level */
	if (ts->factory_level) {
		rc = slsi_ts_write_factory_level(ts, ts->factory_position);
		if (rc < 0)
			goto err_trx_short;
	}

	input_info(true, &ts->client->dev, "%s: self test start\n", __func__);
	rc = slsi_ts_wait_for_ready(ts, SLSI_TS_CMD_SELFTEST, data, len, delay);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_trx_short;
	}

	input_info(true, &ts->client->dev, "%s: self test done\n", __func__);

	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_SELFTEST_RESULT, rBuff, size);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_trx_short;
	}

	slsi_ts_locked_release_all_finger(ts);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: mode changed failed!\n", __func__);
		goto err_trx_short;
	}

	ts->plat_data->init(ts);

	input_info(true, &ts->client->dev, "%s: %02X, %02X, %02X, %02X\n",
			__func__, rBuff[16], rBuff[17], rBuff[18], rBuff[19]);

	if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
				sec->cmd_param[1] == CHECK_ONLY_SHORT_TEST) {
		for (jj = 0 ; jj < 3 ; jj++) {
			input_info(true, &ts->client->dev, "%s: %s data offset : %d / data_length : %d / size : %d\n",
					__func__, jj == 0 ? "S2G" : (jj == 1 ? "S2S" : "S2O"), self_data_offset, data_length, size);

			for (ii = 0; ii < data_length ; ii += 2) {
				ts->pFrame[ii / 2] = ((rBuff[ii + self_data_offset + 1] << 8) | rBuff[ii + self_data_offset]);
			}

			slsi_ts_print_channel(ts);
			self_data_offset = self_data_offset + data_length;
		}
	} else {
		short min, max;
		input_info(true, &ts->client->dev, "%s: #29\n", __func__);

		for (ii = 0; ii < (ts->tx_count * ts->rx_count * 2); ii += 2)
			ts->pFrame[ii / 2] = (rBuff[SLSI_TS_SELFTEST_REPORT_SIZE + ii + 1] << 8) + rBuff[SLSI_TS_SELFTEST_REPORT_SIZE + ii];
		
		slsi_ts_print_frame(ts, &min, &max);
	}

	enable_irq(ts->client->irq);

	memcpy(data, &rBuff[48], 32);

	for (ii = start_point ; ii < checklen; ii++)
		sum += data[ii];

	if (!sum)
		goto test_ok;

	// set start check point for cm1 short test
	for (ii = start_point ; ii < checklen; ii += 8) {
		int jj;
		long long lldata = 0;

		memcpy(&lldata, &data[ii], 8);
		if (!lldata)
			continue;

		memset(tempn, 0x00, 40);

		if (sec->cmd_param[0] == OPEN_SHORT_TEST) {
			if (ii / 8 == 0)
				snprintf(tempn, 40, " TX/RX_OPEN:");
			else if (ii / 8 == 1)
				snprintf(tempn, 40, " TX/RX_SHORT_TO_GND:");
			else if (ii / 8 == 2)
				snprintf(tempn, 40, " TX/RX_SHORT_TO_TX/RX:");
			else if (ii / 8 == 3)
				snprintf(tempn, 40, " TX/RX_SHORT_TO_RX/TX:");
		} else if (sec->cmd_param[0] == CRACK_TEST) {
			snprintf(tempn, 40, " CRACK:");
		} else if (sec->cmd_param[0] == BRIDGE_SHORT_TEST) {
			snprintf(tempn, 40, "BRIDGE_SHORT:");
		}
		strlcat(test_result_buff, tempn, PAGE_SIZE);
		memcpy(&temp_result, &data[ii], 8);

		for (jj = 0; jj < ts->tx_count + ts->rx_count; jj++) {
			memset(tempv, 0x00, 25);
			if (temp_result & 0x1)
				snprintf(tempv, 20, "%cX%d,",
						jj < ts->tx_count ? 'T':'R',
						jj < ts->tx_count ? jj : jj - ts->tx_count);
			strlcat(test_result_buff, tempv, PAGE_SIZE);
			temp_result = temp_result >> 1;
		}
	}

	sec_cmd_set_cmd_result(sec, test_result_buff, strnlen(buff, PAGE_SIZE));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, test_result_buff);

	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);

	kfree(rBuff);
	kfree(test_result_buff);
	snprintf(result, sizeof(result), "RESULT=FAIL");
	sec_cmd_send_event_to_user(&ts->sec, test, result);
	return;

test_ok:
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);

	kfree(rBuff);
	kfree(test_result_buff);
	snprintf(result, sizeof(result), "RESULT=PASS");
	sec_cmd_send_event_to_user(&ts->sec, test, result);
	return;

err_trx_short:

	slsi_ts_locked_release_all_finger(ts);

	ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);

	kfree(rBuff);
	kfree(test_result_buff);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	snprintf(result, sizeof(result), "RESULT=FAIL");
	sec_cmd_send_event_to_user(&ts->sec, test, result);
	return;
}

static void run_elvss_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	u8 mode[2] = {0x03, 0x08};
	char para;
	u8 tBuff[10] = {0};
	int retry = 0;
	u8 test_result;
	int ret;

	sec_cmd_set_default_result(sec);

	disable_irq(ts->client->irq);

	para = TO_SELFTEST_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed power mode\n", __func__);
		goto err_mode;
	}

	sec_delay(50);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed clear stack\n", __func__);
		goto err_test;
	}

	ret = ts->slsi_ts_i2c_write(ts, SET_TS_CMD_ELVSS_TEST, mode, sizeof(mode));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed elvss test\n", __func__);
		goto err_test;
	}

	sec_delay(50);

	while (retry <= SEC_TS_WAIT_RETRY_CNT) {
		if (ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_EVENT, tBuff, 8) > 0) {
			if (((tBuff[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_VENDOR_INFO) {
				if (tBuff[1] == SLSI_TS_VENDOR_ACK_ELVSS_TEST_DONE) {
					test_result = (tBuff[6] & 0x1);
					break;
				}
			}
		} else {
			goto err_test;
		}
	
		sec_delay(20);
		retry++;
	}

	if (retry > SEC_TS_WAIT_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
		goto err_test;
	}

	input_info(true, &ts->client->dev,"%s: test result %d", __func__, test_result);

	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed touch mode\n", __func__);

	enable_irq(ts->client->irq);

	if (test_result == 0)
		snprintf(buff, sizeof(buff), "OK");
	else
		snprintf(buff, sizeof(buff), "NG");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

err_test:
	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed touch mode\n", __func__);
err_mode:
	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static int slsi_ts_execute_force_calibration(struct slsi_ts_data *ts, int cal_mode)
{
	int rc = -1;
	u8 cmd;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, cal_mode);

	if (cal_mode == OFFSET_CAL_SEC)
		cmd = SLSI_TS_CMD_FACTORY_PANELCALIBRATION;
	else if (cal_mode == AMBIENT_CAL)
		cmd = SLSI_TS_CMD_CALIBRATION_AMBIENT;
	else
		return rc;

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		return rc;
	}

	rc = slsi_ts_wait_for_ready(ts, cmd, NULL, 0, 3000);

#ifdef TCLM_CONCEPT
	if (rc >= 0 && (cal_mode == OFFSET_CAL_SEC) && ts->tdata->support_tclm_test)
		ts->is_cal_done = true;

	if (rc < 0) {
		ts->tdata->nvdata.cal_fail_cnt++;
		ts->tdata->nvdata.cal_fail_falg = 0;
		ts->tdata->tclm_write(ts->tdata->client, SEC_TCLM_NVM_ALL_DATA);
		return rc;
	}

	ts->tdata->nvdata.cal_fail_falg = SEC_CAL_PASS;
	ts->tdata->tclm_write(ts->tdata->client, SEC_TCLM_NVM_ALL_DATA);
#endif
	return rc;
}

int sec_tclm_execute_force_calibration(struct i2c_client *client, int cal_mode)
{
	struct slsi_ts_data *ts = i2c_get_clientdata(client);
	int rc;
	/* cal_mode is same tclm and sec. if they are different, modify source */
	rc = slsi_ts_execute_force_calibration(ts, cal_mode);

	return rc;
}

static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal_before_irq_ctrl;
	}

	if (ts->plat_data->touch_count > 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal_before_irq_ctrl;
	}

	disable_irq(ts->client->irq);

	rc = slsi_ts_execute_force_calibration(ts, OFFSET_CAL_SEC);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_force_cal;
	}

#ifdef TCLM_CONCEPT
	/* devide tclm case */
	sec_tclm_case(ts->tdata, sec->cmd_param[0]);

	input_info(true, &ts->client->dev, "%s: param, %d, %c, %d\n", __func__,
		sec->cmd_param[0], sec->cmd_param[0], ts->tdata->root_of_calibration);

	rc = sec_execute_tclm_package(ts->tdata, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev,
					"%s: sec_execute_tclm_package\n", __func__);
	}
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

	ts->cal_status = slsi_ts_read_calibration_report(ts);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out_force_cal:
	enable_irq(ts->client->irq);

out_force_cal_before_irq_ctrl:
	/* not to enter external factory mode without setting everytime */
	ts->tdata->external_factory = false;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	rc = slsi_ts_read_calibration_report(ts);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else if (rc == SLSI_TS_STATUS_CALIBRATION_SEC) {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;
	u8 mBuff[3] = { 0x32, 0x00, 0x00 };
	u8 result[2] = { 0, 0 };

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		goto err_power_state;
	}

	disable_irq(ts->client->irq);

	rc = slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: slsi_ts_fix_tmode failed\n", __func__);
		goto err_fix_tmode;
	}

	sec_delay(10);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_MISCAL_THD, mBuff, sizeof(mBuff));
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_CMD_SET_MISCALIBRATIONTEST failed\n", __func__);
		goto err;
	}

	sec_delay(10);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_RUN_MISCAL, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_CMD_MISALIBRATIONTEST failed\n", __func__);
		goto err;
	}

	sec_delay(200);

	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_GET_MISCAL_RESULT, result, sizeof(result));
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_CMD_GET_MISCALBRATION failed\n", __func__);
		goto err;
	}

	sec_delay(10);

	rc = slsi_ts_release_tmode(ts);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_CMD_STATEMANAGE_ON failed\n", __func__);
		goto err;
	}

	if (result[0]) {
		enable_irq(ts->client->irq);
		snprintf(buff, sizeof(buff), "%d", result[1]);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

err:
	slsi_ts_release_tmode(ts);
err_fix_tmode:
	enable_irq(ts->client->irq);
err_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_factory_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char para = TO_TOUCH_MODE;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		goto error_power_state;
	}

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	disable_irq(ts->client->irq);
	ret = slsi_ts_p2p_tmode(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p tmode\n",
				__func__);
		goto error_tmode_fail;
	}

	ret = execute_p2ptest(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to fix p2p test\n",
				__func__);
		goto error_test_fail;
	}

	if (ts->cm_raw_set_p2p_gap_y_result)
		snprintf(buff, sizeof(buff), "NG,0,%d,%d", ts->cm_raw_set_p2p_gap_y, ts->gap_max_spec);
	else
		snprintf(buff, sizeof(buff), "OK,0,%d,%d", ts->cm_raw_set_p2p_gap_y, ts->gap_max_spec);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	slsi_ts_locked_release_all_finger(ts);

	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

	enable_irq(ts->client->irq);

	return;

error_test_fail:
	slsi_ts_locked_release_all_finger(ts);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

error_tmode_fail:
	enable_irq(ts->client->irq);
error_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	return;
}

static void get_idle_dvdd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;
	u8 result[6] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF)
		goto err_power_state;

	disable_irq(ts->client->irq);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		goto err;
	}

	sec_delay(30);

	rc = slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_IDLE);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: slsi_ts_fix_tmode failed\n", __func__);
		goto err;
	}

	sec_delay(200);

	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_EVENT, result, sizeof(result));
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_READ_EVENT failed\n", __func__);
		slsi_ts_release_tmode(ts);
		goto err;
	}

	sec_delay(10);

	rc = slsi_ts_release_tmode(ts);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: slsi_ts_release_tmode failed\n", __func__);
		goto err;
	}

	input_info(true, &ts->client->dev, "%s: %02X, %02X, %02X, %02X, %02X, %02X\n", __func__ ,
		result[0], result[1], result[2], result[3], result[4], result[5]);

	if (result[0] == 0x09 && result[1] == 0x00 && result[2] == 0x10
		&& result[3] == 0x00 && result[4] == 0x00 && result[5] == 0x00) {
		input_err(true, &ts->client->dev, "%s: spec out\n", __func__);
		goto err;

	}

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "1");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DVDD_VECTOR");
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

err:
	enable_irq(ts->client->irq);
err_power_state:
	snprintf(buff, sizeof(buff), "0");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DVDD_VECTOR");
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;
	u8 result;
	u8 para = 0x0F;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF)
		goto err_power_state;

	disable_irq(ts->client->irq);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		goto err;
	}

	sec_delay(30);
	
	rc = ts->slsi_ts_i2c_write(ts, SET_TS_CMD_SRAM_TEST, &para, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: sram test failed\n", __func__);
		goto err;
	}

	sec_delay(150);

	rc = ts->slsi_ts_i2c_read(ts, SET_TS_CMD_SRAM_TEST, &result, sizeof(result));
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: SET_TS_CMD_SRAM_TEST_RESULT failed\n", __func__);
		goto err;
	}

	sec_delay(10);

	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on ret:%d\n", __func__, rc);

	input_info(true, &ts->client->dev, "%s: result:%d\n", __func__, result);

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%d", result);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");
	return;
err:
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on ret:%d\n", __func__, rc);
	enable_irq(ts->client->irq);
err_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_differential_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int ret;
	u8 result;
	u8 para = 0x01;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF)
		goto err_power_state;

	disable_irq(ts->client->irq);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		goto err;
	}

	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_DIFFERENTIAL_TEST, &para, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: diffrential test failed\n", __func__);
		goto err;
	}

	sec_delay(500);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_DIFFERENTIAL_TEST, &result, sizeof(result));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: SLSI_TS_CMD_DIFFERENTIAL_TEST failed\n", __func__);
		goto err;
	}

	sec_delay(30);
	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);

	input_info(true, &ts->client->dev, "%s: result:%d\n", __func__, result);

	enable_irq(ts->client->irq);
	snprintf(buff, sizeof(buff), "%d", result);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DIFFERENTIAL_CHECK");
	return;

err:
	para = TO_TOUCH_MODE;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: set rawdata type failed!\n", __func__);
	enable_irq(ts->client->irq);
err_power_state:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DIFFERENTIAL_CHECK");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s: already checking now\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_rawcap_read(sec);
	get_gap_data(sec);
	run_reference_read(sec);

	run_self_rawcap_read(sec);
	get_self_channel_data(sec, TYPE_OFFSET_DATA_SDC);

	run_raw_p2p_read(sec);

	get_wet_mode(sec);
	get_cmoffset_set_proximity(sec);
	get_idle_dvdd(sec);
	run_sram_test(sec);
	run_differential_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

#ifdef TCLM_CONCEPT
static void tclm_test_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct sec_tclm_data *data = ts->tdata;
	int ret = 0;

	sec_cmd_set_default_result(sec);
	if (!ts->tdata->support_tclm_test)
		goto not_support;

	ret = tclm_test_command(data, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], buff);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

not_support:
	snprintf(buff, sizeof(buff), "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	if (!ts->tdata->support_tclm_test)
		goto not_support;
	snprintf(buff, sizeof(buff), "%d", ts->is_cal_done);

	ts->is_cal_done = false;
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

not_support:
	snprintf(buff, sizeof(buff), "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}
#endif

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	s16 value;
	char data[2] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if(!ts->proximity_thd) {
		data[0] = 4;	// proximity thd

		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_TOUCH_MODE_FOR_THRESHOLD, data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: threshold write type failed. ret: %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_TOUCH_THRESHOLD, data, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read threshold fail!\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		ts->proximity_thd = (data[0] << 8) | data[1];
		input_info(true, &ts->client->dev, "%s: proximity_thd(%d)\n", __func__, ts->proximity_thd);
	}

	if (!ts->proximity_jig_mode) {

		data[0] = 0x01;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_JIG_MODE, data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to set jig mode (%d)\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		
		}

		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_JIG_MODE, data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to get jig mode value (%d)\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		if (data[0] == 1) {
			sec_delay(20);
			ts->proximity_jig_mode = true;
			input_info(true, &ts->client->dev, "%s: Set jig mode ON (%d)\n", __func__, ret);

		} else {
			input_err(true, &ts->client->dev, "%s: failed to set jig mode data(%d)\n", __func__, data[0]);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_PROX_INTENSITY, data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to integrity check (%d)\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	value = (data[0] << 8) + data[1];
	input_err(true, &ts->client->dev, "%s: data %d (%x/%x)\n", __func__, value, data[0], data[1]);

	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", value, ts->proximity_thd);
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	unsigned char result = 0;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_FIRMWARE_INTEGRITY, &result, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to integrity check (%d)\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	} else {
		if (result == 0x80) {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
			input_info(true, &ts->client->dev, "%s: valid firmware (0x%x)\n", __func__, result);
		} else if (result == 0x40) {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &ts->client->dev, "%s: invalid firmware (0x%x)\n", __func__, result);
		} else {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &ts->client->dev, "%s: invalid integrity result (0x%x)\n", __func__, result);
		}
	}

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;
}

static void set_log_level(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };
	u8 w_data[1] = {0x00};
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if ((sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) ||
			(sec->cmd_param[1] < 0 || sec->cmd_param[1] > 1) ||
			(sec->cmd_param[2] < 0 || sec->cmd_param[2] > 1) ||
			(sec->cmd_param[3] < 0 || sec->cmd_param[3] > 1) ||
			(sec->cmd_param[4] < 0 || sec->cmd_param[4] > 1) ||
			(sec->cmd_param[5] < 0 || sec->cmd_param[5] > 1) ||
			(sec->cmd_param[6] < 0 || sec->cmd_param[6] > 1) ||
			(sec->cmd_param[7] < 0 || sec->cmd_param[7] > 1)) {
		input_err(true, &ts->client->dev, "%s: para out of range\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_STATUS_EVENT_TYPE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Read Event type enable status fail\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto err;
	}

	input_info(true, &ts->client->dev, "%s: STATUS_EVENT enable = 0x%02X, 0x%02X\n",
			__func__, tBuff[0], tBuff[1]);

	tBuff[0] = BIT_STATUS_EVENT_VENDOR_INFO(sec->cmd_param[6]);
	tBuff[1] = BIT_STATUS_EVENT_ERR(sec->cmd_param[0]) |
		BIT_STATUS_EVENT_INFO(sec->cmd_param[1]) |
		BIT_STATUS_EVENT_USER_INPUT(sec->cmd_param[2]);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_STATUS_EVENT_TYPE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Write Event type enable status fail\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto err;
	}

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1 &&
			sec->cmd_param[2] == 1 && sec->cmd_param[3] == 1 &&
			sec->cmd_param[4] == 1 && sec->cmd_param[5] == 1 &&
			sec->cmd_param[6] == 1 && sec->cmd_param[7] == 1) {
		w_data[0] = 0x1;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_VENDOR_EVENT_LEVEL, w_data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Write Vendor Event Level fail\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			goto err;
		}
	} else {
		w_data[0] = 0x0;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_VENDOR_EVENT_LEVEL, w_data, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Write Vendor Event Level fail\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			goto err;
		}
	}

	input_info(true, &ts->client->dev, "%s: ERROR : %d, INFO : %d, USER_INPUT : %d, INFO_SPONGE : %d, VENDOR_INFO : %d, VENDOR_EVENT_LEVEL : %d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], sec->cmd_param[5], sec->cmd_param[6], w_data[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;

err:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void set_factory_level(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		goto NG;
	}

	if (sec->cmd_param[0] < OFFSET_FAC_SUB || sec->cmd_param[0] > OFFSET_FAC_MAIN) {
		input_err(true, &ts->client->dev, "%s: cmd data is abnormal, %d\n", __func__, sec->cmd_param[0]);
		goto NG;
	}

	ts->factory_position = sec->cmd_param[0] + 1;	// for FW index
	ts->factory_level = true;

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = {0};
	int rc;
	int size = SLSI_TS_SELFTEST_REPORT_SIZE + ts->tx_count * ts->rx_count * 2;
	u8 *rBuff = NULL;
	int ii;
	u8 data[8] = { 0 };
	int result = 0;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	rBuff = kzalloc(size, GFP_KERNEL);
	if (!rBuff) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	memset(rBuff, 0x00, size);
	memset(data, 0x00, 8);

	disable_irq(ts->client->irq);

	slsi_ts_locked_release_all_finger(ts);

	input_info(true, &ts->client->dev, "%s: set power mode to test mode\n", __func__);
	data[0] = 0x02;
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: set test mode failed\n", __func__);
		goto err_conn_check;
	}

	input_info(true, &ts->client->dev, "%s: clear event stack\n", __func__);
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
		goto err_conn_check;
	}

	data[0] = 0xB7;
	input_info(true, &ts->client->dev, "%s: self test start\n", __func__);
	rc = slsi_ts_wait_for_ready(ts, SLSI_TS_CMD_SELFTEST, data, 1, 700);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_conn_check;
	}

	input_info(true, &ts->client->dev, "%s: self test done\n", __func__);
	rc = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_SELFTEST_RESULT, rBuff, size);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Selftest execution time out!\n", __func__);
		goto err_conn_check;
	}

	data[0] = TO_TOUCH_MODE;
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: mode changed failed!\n", __func__);
		goto err_conn_check;
	}

	input_info(true, &ts->client->dev, "%s: %02X, %02X, %02X, %02X\n",
			__func__, rBuff[16], rBuff[17], rBuff[18], rBuff[19]);

	memcpy(data, &rBuff[48], 8);

	for (ii = 0; ii < 8; ii++) {
		input_info(true, &ts->client->dev, "%s: [%d] %02X\n", __func__, ii, data[ii]);

		result += data[ii];
	}

	if (result != 0)
		goto err_conn_check;

	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(rBuff);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

err_conn_check:
	data[0] = TO_TOUCH_MODE;
	ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	kfree(rBuff);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void fix_active_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts->fix_active_mode = sec->cmd_param[0];

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts->plat_data->power_state != SEC_INPUT_STATE_LPM) {
		if (ts->fix_active_mode)
			slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
		else
			slsi_ts_release_tmode(ts);
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void touch_aging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 data;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0] == 1)
		data = 5;
	else
		data = 0;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &data, 1);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void fp_int_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 data[2] = { 1, 0 }; /* byte[0]:1 - INTR2 */
	u8 enable;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		goto NG;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid param %d\n", __func__, sec->cmd_param[0]);
		goto NG;
	} else if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	enable = sec->cmd_param[0];

	if (enable) {
		ret = slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_IDLE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: slsi_ts_fix_tmode failed\n", __func__);
			goto NG;
		}
	}

	data[1] = enable;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_INPUT_GPIO_CONTROL, data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: write gpio control failed\n", __func__);
		slsi_ts_release_tmode(ts);
		goto NG;
	}

	if (!enable)
		slsi_ts_release_tmode(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %d %s\n", __func__, data[1], buff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0])
			ts->plat_data->touch_functions |= SLSI_TS_BIT_SETFUNC_GLOVE;
		else
			ts->plat_data->touch_functions &= ~SLSI_TS_BIT_SETFUNC_GLOVE;

		ret = slsi_ts_set_touch_function(ts);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed, retval = %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s cmd_param: %d\n", __func__, buff, sec->cmd_param[0]);
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s: start clear_cover_mode %s\n", __func__, buff);
	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			ts->plat_data->cover_type = sec->cmd_param[1];
			slsi_ts_set_cover_type(ts, true);
		} else {
			slsi_ts_set_cover_type(ts, false);
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char data = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		data = sec->cmd_param[0];

		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_EDGE_DEADZONE, &data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to set deadzone\n", __func__);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_set_dead_zone;
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

err_set_dead_zone:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	struct sec_input_notify_data data;

	data.dual_policy = MAIN_TOUCHSCREEN;
#endif

	sec_cmd_set_default_result(sec);

	ret = sec_input_check_wirelesscharger_mode(&ts->client->dev, sec->cmd_param[0], sec->cmd_param[1]);
	if (ret == SEC_ERROR)
		goto NG;
	else if (ret == SEC_SKIP)
		goto OK;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	if (sec->cmd_param[0])
		sec_input_notify(&ts->slsi_input_nb, NOTIFIER_LCD_VRR_LFD_OFF_REQUEST, &data);
	else
		sec_input_notify(&ts->slsi_input_nb, NOTIFIER_LCD_VRR_LFD_OFF_RELEASE, &data);
#endif

	ret = slsi_ts_set_charger_mode(ts);
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

static void set_temperature(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	ret = sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_FORCE);
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
	return;
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	slsi_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret, i;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d, lowpower_mode:0x%02X\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3], ts->plat_data->lowpower_mode);

	for (i = 0; i < 4; i++)
		ts->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	ret = slsi_ts_set_aod_rect(ts);
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
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 data[8] = {0x02, 0};
	u16 rect_data[4] = {0, };
	int ret, i;

	sec_cmd_set_default_result(sec);

	ret = ts->slsi_ts_read_sponge(ts, data, 8);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
		goto NG;
	}

	for (i = 0; i < 4; i++)
		rect_data[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

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
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	slsi_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	slsi_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (!sec_input_set_fod_rect(&ts->client->dev, sec->cmd_param))
		goto NG;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped! Set data at reinit()\n", __func__);
		goto OK;
	}

	ret = slsi_ts_set_fod_rect(ts);
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

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %s, fast:%s, strict:%s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 1 ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 2 ? "on" : "off",
			ts->plat_data->lowpower_mode);

	mutex_lock(&ts->modechange);
	if (!ts->plat_data->enabled && !ts->plat_data->lowpower_mode && !ts->plat_data->ed_enable) {
		if (device_may_wakeup(&ts->client->dev) && ts->plat_data->power_state == SEC_INPUT_STATE_LPM)
			disable_irq_wake(ts->client->irq);
		ts->plat_data->stop_device(ts);
	} else { 
		slsi_ts_set_custom_library(ts);
		slsi_ts_set_press_property(ts);
	}
	mutex_unlock(&ts->modechange);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void fod_icon_visible(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 enable;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev,
			"%s: fod icon visible %d\n", __func__, sec->cmd_param[0]);

	enable = sec->cmd_param[0] & 0xFF;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_FOD_ICON, &enable, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set fod icon visible\n", __func__);
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 jig_data[2] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->plat_data->ed_enable = sec->cmd_param[0];

	ret = slsi_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
	if (ret < 0)
		goto out;

	if (!ts->plat_data->ed_enable && ts->proximity_jig_mode) {

		jig_data[0] = 0x00;
		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_JIG_MODE, jig_data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to set jig mode (%d)\n", __func__, ret);
			goto out;
		}

		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_JIG_MODE, jig_data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to get jig mode value (%d)\n", __func__, ret);
			goto out;
		}

		if (jig_data[0] == 0x00) {
			ts->proximity_jig_mode = false;
			ts->proximity_thd = 0;
			input_info(true, &ts->client->dev, "%s: Set jig mode OFF (%d)\n", __func__, ret);
		}
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
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

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	slsi_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
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
 *			ex) echo set_grip_data,2,0  > cmd
 */

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	mutex_lock(&ts->device_mutex);

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] == 0) {	// clear
			ts->plat_data->grip_data.edgehandler_direction = 0;
		} else if (sec->cmd_param[1] < 3) {
			ts->plat_data->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->plat_data->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->plat_data->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		slsi_set_grip_data_to_ic(&ts->client->dev, mode);
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		if (ts->plat_data->grip_data.edge_range != sec->cmd_param[1])
			mode = mode | G_SET_EDGE_ZONE;

		ts->plat_data->grip_data.edge_range = sec->cmd_param[1];
		ts->plat_data->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->plat_data->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->plat_data->grip_data.deadzone_y = sec->cmd_param[4];
		ts->plat_data->grip_data.deadzone_dn2_x = sec->cmd_param[5];
		ts->plat_data->grip_data.deadzone_dn_y = sec->cmd_param[6];
		mode = mode | G_SET_NORMAL_MODE;

		if (ts->plat_data->grip_data.landscape_mode == 1) {
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		}
		slsi_set_grip_data_to_ic(&ts->client->dev, mode);
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->plat_data->grip_data.landscape_mode = 1;
			ts->plat_data->grip_data.landscape_edge = sec->cmd_param[2];
			ts->plat_data->grip_data.landscape_deadzone	= sec->cmd_param[3];
			ts->plat_data->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->plat_data->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->plat_data->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->plat_data->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = mode | G_SET_LANDSCAPE_MODE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		slsi_set_grip_data_to_ic(&ts->client->dev, mode);
	} else {
		input_err(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	mutex_unlock(&ts->device_mutex);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&ts->device_mutex);

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

/*
 * Enable or disable specific external_noise_mode (sec_cmd)
 *
 * This cmd has 2 params.
 * param 0 : the mode that you want to change.
 * param 1 : enable or disable the mode.
 *
 * For example,
 * enable EXT_NOISE_MODE_MONITOR mode,
 * write external_noise_mode,1,1
 * disable EXT_NOISE_MODE_MONITOR mode,
 * write external_noise_mode,1,0
 */
static void external_noise_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] <= EXT_NOISE_MODE_NONE || sec->cmd_param[0] >= EXT_NOISE_MODE_MAX ||
			sec->cmd_param[1] < 0 || sec->cmd_param[1] > 1) {
		input_err(true, &ts->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	if (sec->cmd_param[1] == 1)
		ts->plat_data->external_noise_mode |= 1 << sec->cmd_param[0];
	else
		ts->plat_data->external_noise_mode &= ~(1 << sec->cmd_param[0]);

	ret = slsi_ts_set_external_noise_mode(ts, sec->cmd_param[0]);
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

/*	for game mode 
	byte[0]: Setting for the Game Mode with 240Hz scan rate
		- 0: Disable
		- 1: Enable

	byte[1]: Vsycn mode
		- 0: Normal 60
		- 1: HS60
		- 2: HS120
		- 3: VSYNC 48
		- 4: VSYNC 96 
*/
static void set_scan_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	char tBuff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1 ||
			sec->cmd_param[1] < 0 || sec->cmd_param[1] > 4) {
		input_err(true, &ts->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	tBuff[0] = sec->cmd_param[0];
	tBuff[1] = sec->cmd_param[1];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_SCANRATE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set scan rate\n", __func__);
		goto NG;
	}

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_SCANRATE, tBuff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to read scan rate\n", __func__);
		goto NG;
	}

	input_info(true, &ts->client->dev,
					"%s: set scan rate %d %d\n", __func__, tBuff[0], tBuff[1]);

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

static void set_touchable_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts->plat_data->touchable_area = sec->cmd_param[0];

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ret = slsi_ts_set_touchable_area(ts);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_low_power_sensitivity(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 lp_sensitivity;

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	lp_sensitivity = sec->cmd_param[0];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_LOW_POWER_SENSITIVITY, &lp_sensitivity, 1);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: parm err(%d)\n", __func__, sec->cmd_param[0]);
		goto NG;
	}

	ts->sip_mode = sec->cmd_param[0];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SIP_MODE, &ts->sip_mode, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send aod off_on cmd\n", __func__);
		goto NG;
	}

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

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	unsigned char data;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: parm err(%d)\n", __func__, sec->cmd_param[0]);
		goto NG;
	}

	data = sec->cmd_param[0];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NOTE_MODE, &data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send  note mode cmd\n", __func__);
		goto NG;
	}

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

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	unsigned char data;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: parm err(%d)\n", __func__, sec->cmd_param[0]);
		goto NG;
	}

	data = sec->cmd_param[0];

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_GAME_MODE, &data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send game mode cmd\n", __func__);
		goto NG;
	}

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

static void sync_changed(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct slsi_ts_data *ts = container_of(sec, struct slsi_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 data;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] >= SLSI_TS_SYNC_CHANGED_MAX) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	data = sec->cmd_param[0];
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SYNC_CHANGED, &data, 1);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	input_info(true, &ts->client->dev, "%s: %d %s\n", __func__, sec->cmd_param[0], buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
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
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_wet_mode", get_wet_mode),},
	{SEC_CMD("get_idle_dvdd", get_idle_dvdd),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("run_differential_test", run_differential_test),},
	{SEC_CMD("get_cmoffset_set_proximity", get_cmoffset_set_proximity),},
	{SEC_CMD("run_cmoffset_set_proximity_read_all", run_cmoffset_set_proximity_read_all),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("run_reference_read_all", run_reference_read_all),},
	{SEC_CMD("run_rawcap_read", run_rawcap_read),},
	{SEC_CMD("run_rawcap_read_all", run_rawcap_read_all),},
	{SEC_CMD("get_gap_data_all", get_gap_data_all),},
	{SEC_CMD("get_gap_data_x_all", get_gap_data_x_all),},
	{SEC_CMD("get_gap_data_y_all", get_gap_data_y_all),},
	{SEC_CMD("run_delta_read", run_delta_read),},
	{SEC_CMD("run_delta_read_all", run_delta_read_all),},
	{SEC_CMD("run_cs_raw_read_all", run_decoded_raw_read_all),},
	{SEC_CMD("run_cs_delta_read_all", run_delta_cm_read_all),},
	{SEC_CMD("run_raw_p2p_read", run_raw_p2p_read),},
	{SEC_CMD("run_raw_p2p_avg_read_all", run_raw_p2p_avg_read_all),},
	{SEC_CMD("run_raw_p2p_read_all", run_raw_p2p_read_all),},
	{SEC_CMD("run_raw_p2p_node_gap_y_read_all", run_raw_p2p_node_gap_y_read_all),},
	{SEC_CMD("run_self_reference_read", run_self_reference_read),},
	{SEC_CMD("run_self_reference_read_all", run_self_reference_read_all),},
	{SEC_CMD("run_self_rawcap_read", run_self_rawcap_read),},
	{SEC_CMD("run_self_rawcap_read_all", run_self_rawcap_read_all),},
	{SEC_CMD("run_self_delta_read", run_self_delta_read),},
	{SEC_CMD("run_self_delta_read_all", run_self_delta_read_all),},
	{SEC_CMD("run_self_raw_p2p_avg_read_all", run_self_raw_p2p_avg_read_all),},
	{SEC_CMD("run_self_raw_p2p_diff_read_all", run_self_raw_p2p_diff_read_all),},
	{SEC_CMD("run_rawdata_read_all_for_ghost", run_rawdata_read_all),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
	{SEC_CMD("get_force_calibration", get_force_calibration),},
	{SEC_CMD("run_miscalibration", run_miscalibration),},
	{SEC_CMD("run_factory_miscalibration", run_factory_miscalibration),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_jitter_test", run_jitter_test),},
	{SEC_CMD("run_elvss_test", run_elvss_test),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("set_factory_level", set_factory_level),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD_H("fix_active_mode", fix_active_mode),},
	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("clear_tsp_test_result", clear_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("set_external_factory", set_external_factory),},
	{SEC_CMD("tclm_test_cmd", tclm_test_cmd),},
	{SEC_CMD("get_calibration", get_calibration),},
#endif	
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{SEC_CMD("set_temperature", set_temperature),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD("fod_icon_visible", fod_icon_visible),},
	{SEC_CMD_H("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD_H("external_noise_mode", external_noise_mode),},
	{SEC_CMD_H("set_scan_rate", set_scan_rate),},
	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
	{SEC_CMD_H("fp_int_control", fp_int_control),},
	{SEC_CMD_H("set_low_power_sensitivity", set_low_power_sensitivity),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("sync_changed", sync_changed),},
	{SEC_CMD("set_log_level", set_log_level),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int slsi_ts_fn_init(struct slsi_ts_data *ts)
{
	int retval = 0;

	retval = sec_cmd_init(&ts->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts->sec.fac_dev->kobj,
			&ts->plat_data->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create input symbolic link\n",
				__func__);
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sec_input_sysfs_create(&ts->plat_data->input_dev->dev.kobj);
	if (retval < 0) {
		sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		input_err(true, &ts->client->dev,
				"%s: Failed to create sec_input_sysfs attributes\n", __func__);
		goto exit;
	}

	return 0;

exit:
	return retval;
}

void slsi_ts_fn_remove(struct slsi_ts_data *ts)
{
	input_err(true, &ts->client->dev, "%s\n", __func__);

	sec_input_sysfs_remove(&ts->plat_data->input_dev->dev.kobj);

	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}

MODULE_LICENSE("GPL");
