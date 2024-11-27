/* drivers/input/sec_input/stm/stm_fn.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"
#include "stm_reg.h"

int stm_ts_set_custom_library(struct stm_ts_data *ts)
{
	u8 data[3] = { 0 };
	int ret;
	u8 force_fod_enable = 0;

#if IS_ENABLED(CONFIG_SEC_FACTORY)
		/* enable FOD when LCD on state */
	if (ts->plat_data->support_fod && ts->plat_data->enabled)
		force_fod_enable = SEC_TS_MODE_SPONGE_PRESS;
#endif

	input_err(true, &ts->client->dev, "%s: Sponge (0x%02x)%s\n",
			__func__, ts->plat_data->lowpower_mode,
			force_fod_enable ? ", force fod enable" : "");

	if (ts->plat_data->prox_power_off) {
		data[2] = (ts->plat_data->lowpower_mode | force_fod_enable) &
						~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
		input_info(true, &ts->client->dev, "%s: prox off. disable AOT\n", __func__);
	} else {
		data[2] = ts->plat_data->lowpower_mode | force_fod_enable;
	}

	ret = ts->stm_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* inf dump enable */
	data[0] = STM_TS_CMD_SPONGE_OFFSET_MODE_01;
	data[2] = ts->plat_data->sponge_mode |= SEC_TS_MODE_SPONGE_INF_DUMP;

	ret = ts->stm_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);
#endif
		/* read dump info */
	data[0] = STM_TS_CMD_SPONGE_LP_DUMP;

	ret = ts->stm_ts_read_sponge(ts, data, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to read dump_data\n", __func__);

	ts->sponge_inf_dump = (data[0] & SEC_TS_SPONGE_DUMP_INF_MASK) >> SEC_TS_SPONGE_DUMP_INF_SHIFT;
	ts->sponge_dump_format = data[0] & SEC_TS_SPONGE_DUMP_EVENT_MASK;
	ts->sponge_dump_event = data[1];
	ts->sponge_dump_border = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT
					+ (ts->sponge_dump_format * ts->sponge_dump_event);
	ts->sponge_dump_border_lsb = ts->sponge_dump_border & 0xFF;
	ts->sponge_dump_border_msb = (ts->sponge_dump_border & 0xFF00) >> 8;

	input_info(true, &ts->client->dev, "%s: sponge_inf_dump:%d, sponge_dump_format:%d, sponge_dump_event:%d\n", __func__,
				ts->sponge_inf_dump, ts->sponge_dump_format, ts->sponge_dump_event);

	return ret;
}

void stm_ts_get_custom_library(struct stm_ts_data *ts)
{
	u8 data[6] = { 0 };
	int ret, i;

	data[0] = SEC_TS_CMD_SPONGE_AOD_ACTIVE_INFO;
	ret = ts->stm_ts_read_sponge(ts, data, 6);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read aod active area\n", __func__);
		return;
	}

	for (i = 0; i < 3; i++)
		ts->plat_data->aod_data.active_area[i] = (data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);

	input_info(true, &ts->client->dev, "%s: aod_active_area - top:%d, edge:%d, bottom:%d\n",
			__func__, ts->plat_data->aod_data.active_area[0],
			ts->plat_data->aod_data.active_area[1], ts->plat_data->aod_data.active_area[2]);

	memset(data, 0x00, 6);

	data[0] = SEC_TS_CMD_SPONGE_FOD_INFO;
	ret = ts->stm_ts_read_sponge(ts, data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read fod info\n", __func__);
		return;
	}

	sec_input_set_fod_info(&ts->client->dev, data[0], data[1], data[2], data[3]);

	data[0] = STM_TS_CMD_SPONGE_OFFSET_MODE_01;
	ret = ts->stm_ts_read_sponge(ts, data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read mode 01 info\n", __func__);
		return;
	}
	ts->plat_data->sponge_mode = data[0];
	input_info(true, &ts->client->dev, "%s: sponge_mode %x\n", __func__, ts->plat_data->sponge_mode);
}

void stm_ts_set_fod_finger_merge(struct stm_ts_data *ts)
{
	int ret;
	u8 address[2] = {STM_TS_CMD_SET_FOD_FINGER_MERGE, 0};

	if (!ts->plat_data->support_fod)
		return;

	if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS)
		address[1] = 1;
	else
		address[1] = 0;

	mutex_lock(&ts->sponge_mutex);
	input_info(true, &ts->client->dev, "%s: %d\n", __func__, address[1]);

	ret = ts->stm_ts_write(ts, address, 2, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed\n", __func__);
	mutex_unlock(&ts->sponge_mutex);
}

void stm_ts_read_chip_id(struct stm_ts_data *ts)
{
	u8 address = STM_TS_READ_DEVICE_ID;
	u8 data[5] = {0};
	int ret;

	ret = ts->stm_ts_read(ts, &address, 1, &data[0], 5);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		return;
	}

	ts->chip_id = (data[2] << 16) + (data[3] << 8) + data[4];

	input_info(true, &ts->client->dev, "%s: %c %c %02X %02X %02X (%x)\n",
			__func__, data[0], data[1], data[2], data[3], data[4], ts->chip_id);
}

void stm_ts_read_chip_id_hw(struct stm_ts_data *ts)
{
	u8 address[5] = {STM_TS_CMD_REG_R, 0x20, 0x00, 0x00, 0x00 };
	u8 data[8] = {0};
	int ret;

	ret = ts->stm_ts_read(ts, address, 5, &data[0], 8);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
	}

	input_info(true, &ts->client->dev, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			__func__, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

int stm_ts_get_channel_info(struct stm_ts_data *ts)
{
	int rc = -1;
	u8 address = 0;
	u8 data[STM_TS_EVENT_BUFF_SIZE] = { 0 };

	memset(data, 0x0, STM_TS_EVENT_BUFF_SIZE);

	address = STM_TS_READ_PANEL_INFO;
	rc = ts->stm_ts_read(ts, &address, 1, data, 11);
	if (rc < 0) {
		ts->tx_count = 0;
		ts->rx_count = 0;
		input_err(true, &ts->client->dev, "%s: Get channel info Read Fail!!\n", __func__);
		return rc;
	}

	ts->tx_count = data[8]; // Number of TX CH
	ts->rx_count = data[9]; // Number of RX CH

	if (!(ts->tx_count > 0 && ts->tx_count < STM_TS_MAX_NUM_FORCE &&
		ts->rx_count > 0 && ts->rx_count < STM_TS_MAX_NUM_SENSE)) {
		ts->tx_count = STM_TS_MAX_NUM_FORCE;
		ts->rx_count = STM_TS_MAX_NUM_SENSE;
		input_err(true, &ts->client->dev, "%s: set channel num based on max value, check it!\n", __func__);
	}

	ts->plat_data->x_node_num = ts->tx_count;
	ts->plat_data->y_node_num = ts->rx_count;

	ts->resolution_x = (data[0] << 8) | data[1]; // X resolution of IC
	ts->resolution_y = (data[2] << 8) | data[3]; // Y resolution of IC

	input_info(true, &ts->client->dev, "%s: RX:Sense(%02d) TX:Force(%02d) resolution:(IC)x:%d y:%d, (DT)x:%d,y:%d\n",
		__func__, ts->rx_count, ts->tx_count,
		ts->resolution_x, ts->resolution_y, ts->plat_data->max_x, ts->plat_data->max_y);

	if (ts->resolution_x > 0 && ts->resolution_x < STM_TS_MAX_X_RESOLUTION &&
		ts->resolution_y > 0 && ts->resolution_y < STM_TS_MAX_Y_RESOLUTION &&
		ts->plat_data->max_x != ts->resolution_x && ts->plat_data->max_y != ts->resolution_y) {
		ts->plat_data->max_x = ts->resolution_x;
		ts->plat_data->max_y = ts->resolution_y;
		input_err(true, &ts->client->dev, "%s: set resolution based on ic value, check it!\n", __func__);
	}

	if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00 && data[8] == 0x00 && data[9] == 0x00) {
		input_err(true, &ts->client->dev, "%s: channel number and resoltion value is zero. return ENODEV\n", __func__);
		return -ENODEV;
	}

	if (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF && data[8] == 0xFF && data[9] == 0xFF) {
		input_err(true, &ts->client->dev, "%s: channel number and resoltion value is FF. return ENODEV\n", __func__);
		return -ENODEV;
	}

	return rc;
}

int stm_ts_get_sysinfo_data(struct stm_ts_data *ts, u8 sysinfo_addr, u8 read_cnt, u8 *data)
{
	int ret;
	int rc = 0;
	u8 *buff = NULL;

	u8 address[3] = { 0xA4, 0x06, 0x01 }; // request system information

	ret = stm_ts_wait_for_echo_event(ts, &address[0], 3, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: timeout wait for event\n", __func__);
		rc = -1;
		goto ERROR;
	}

	address[0] = 0xA6;
	address[1] = 0x00;
	address[2] = sysinfo_addr;

	buff = kzalloc(read_cnt, GFP_KERNEL);
	if (!buff) {
		rc = -2;
		goto ERROR;
	}

	ret = ts->stm_ts_read(ts, &address[0], 3, &buff[0], read_cnt);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n",
				__func__, ret);
		kfree(buff);
		rc = -3;
		goto ERROR;
	}

	memcpy(data, &buff[0], read_cnt);
	kfree(buff);

ERROR:
	return rc;
}

int stm_ts_get_version_info(struct stm_ts_data *ts)
{
	int rc;
	u8 address = STM_TS_READ_FW_VERSION;
	u8 data[STM_TS_VERSION_SIZE] = { 0 };

	memset(data, 0x0, STM_TS_VERSION_SIZE);

	rc = ts->stm_ts_read(ts, &address, 1, (u8 *)data, STM_TS_VERSION_SIZE);

	ts->fw_version_of_ic = (data[0] << 8) + data[1];
	ts->config_version_of_ic = (data[2] << 8) + data[3];
	ts->fw_main_version_of_ic = data[4] + (data[5] << 8);
	ts->project_id_of_ic = data[6];
	ts->ic_name_of_ic = data[7];
	ts->module_version_of_ic = data[8];

	ts->plat_data->img_version_of_ic[2] = ts->module_version_of_ic;
	ts->plat_data->img_version_of_ic[3] = ts->fw_main_version_of_ic & 0xFF;

	input_info(true, &ts->client->dev,
			"%s: [IC] Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X\n",
			__func__, ts->fw_version_of_ic,
			ts->config_version_of_ic, ts->fw_main_version_of_ic);
	input_info(true, &ts->client->dev,
			"%s: [IC] Project ID: 0x%02X, IC Name: 0x%02X, Module Ver: 0x%02X\n",
			__func__, ts->project_id_of_ic,
			ts->ic_name_of_ic, ts->module_version_of_ic);

	return rc;
}

void stm_ts_command(struct stm_ts_data *ts, u8 cmd, bool checkecho)
{
	int ret = 0;


	if (checkecho)
		ret = stm_ts_wait_for_echo_event(ts, &cmd, 1, 100);
	else
		ret = ts->stm_ts_write(ts, &cmd, 1, 0, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to write command(0x%02X), ret = %d\n", __func__, cmd, ret);

}

int stm_ts_systemreset(struct stm_ts_data *ts, unsigned int msec)
{
	u8 address = STM_TS_CMD_REG_W;
	u8 data[5] = { 0x20, 0x00, 0x00, 0x24, 0x81 };
	int rc;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	disable_irq(ts->irq);

	ts->stm_ts_write(ts, &address, 1, &data[0], 5);

	sec_delay(msec + 10);

	rc = stm_ts_wait_for_ready(ts);

	stm_ts_release_all_finger(ts);

	enable_irq(ts->irq);

	return rc;
}

int stm_ts_fix_active_mode(struct stm_ts_data *ts, int mode)
{
	u8 address[3] = {0xA0, 0x00, 0x00};
	int ret = -EINVAL;

	switch (mode) {
	case STM_TS_ACTIVE_FALSE:
		address[1] = 0x00;
		address[2] = 0x01;
		ts->stm_ts_command(ts, STM_TS_CMD_SENSE_OFF, false);
		sec_delay(10);
		break;
	case STM_TS_ACTIVE_TRUE:
		address[1] = 0x03;
		address[2] = 0x00;
		break;
	case STM_TS_ACTIVE_FALSE_SNR:
		address[1] = 0x00;
		address[2] = 0x01;
		break;
	default:
		input_err(true, &ts->client->dev, "%s: err data mode: %d\n", __func__, mode);
		return ret;
	}

	ret = ts->stm_ts_write(ts, &address[0], 3, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: err: %d, mode: %d\n", __func__, ret, mode);
	else
		input_info(true, &ts->client->dev, "%s: %d STM_TS_ACTIVE_%s\n", __func__, mode,
					(mode == STM_TS_ACTIVE_TRUE) ? "TRUE" :
					(mode == STM_TS_ACTIVE_FALSE) ? "FALSE" : "FALSE_SNR");

	sec_delay(10);
	return ret;
}

void stm_ts_change_scan_rate(struct stm_ts_data *ts, u8 rate)
{
	u8 address = STM_TS_CMD_SET_GET_REPORT_RATE;
	u8 data = rate;
	int ret = 0;

	ret = ts->stm_ts_write(ts, &address, 1, &data, 1);

	input_dbg(true, &ts->client->dev, "%s: scan rate (%d Hz), ret = %d\n", __func__, address, ret);
}

int stm_ts_fw_corruption_check(struct stm_ts_data *ts)
{
	u8 address[6] = { 0, };
	u8 val = 0;
	int ret;

	ret = ts->stm_ts_systemreset(ts, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: stm_ts_systemreset fail (%d)\n", __func__, ret);
		goto out;
	}
	
	/* Firmware Corruption Check */
	address[0] = STM_TS_CMD_REG_R;
	address[1] = 0x20;
	address[2] = 0x00;
	address[3] = 0x00;
	address[4] = 0x78;
	ret = ts->stm_ts_read(ts, address, 5, &val, 1);
	if (ret < 0) {
		ret = -STM_TS_I2C_ERROR;
		goto out;
	}
	if (val & 0x03) { // Check if crc error
		input_err(true, &ts->client->dev, "%s: firmware corruption. CRC status:%02X\n",
				__func__, val & 0x03);
		ret = -STM_TS_ERROR_FW_CORRUPTION;
	} else {
		ret = 0;
	}

out:
	return ret;
}

int stm_ts_wait_for_ready(struct stm_ts_data *ts)
{
	struct stm_ts_event_status *p_event_status;
	int rc;
	u8 address = STM_TS_READ_ONE_EVENT;
	u8 data[STM_TS_EVENT_BUFF_SIZE];
	int retry = 0;
	int err_cnt = 0;

	mutex_lock(&ts->fn_mutex);

	memset(data, 0x0, STM_TS_EVENT_BUFF_SIZE);

	rc = -1;
	while (ts->stm_ts_read(ts, &address, 1, (u8 *)data, STM_TS_EVENT_BUFF_SIZE) >= 0) {
		p_event_status = (struct stm_ts_event_status *) &data[0];

		if ((p_event_status->stype == STM_TS_EVENT_STATUSTYPE_INFO) &&
				(p_event_status->status_id == STM_TS_INFO_READY_STATUS)) {
			rc = 0;
			break;
		}

		if (data[0] == 0xff && data[1] == 0xff && data[2] == 0xff) {
			rc = -STM_TS_ERROR_BROKEN_FW;
			break;
		}

		if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			input_err(true, &ts->client->dev,
					"%s: Err detected %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

			// check if config / cx / panel configuration area is corrupted
			if (((data[1] >= 0x20) && (data[1] <= 0x23)) || ((data[1] >= 0xA0) && (data[1] <= 0xA8))) {
				rc = -STM_TS_ERROR_FW_CORRUPTION;
				ts->plat_data->hw_param.checksum_result = 1;
				ts->fw_corruption = true;
				input_err(true, &ts->client->dev, "%s: flash corruption\n", __func__);
				break;
			}
			if (data[1] == 0x24 || data[1] ==  0x25 || data[1] ==  0x29 || data[1] ==  0x2A || data[1] == 0x2D || data[1] == 0x34) {
				input_err(true, &ts->client->dev, "%s: osc trim is broken\n", __func__);
				rc = -STM_TS_ERROR_BROKEN_OSC_TRIM;
				ts->fw_corruption = true;
				break;
			}

			if (err_cnt++ > 32) {
				rc = -STM_TS_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 15) {
			rc = -STM_TS_ERROR_TIMEOUT;
			if (data[0] == 0 && data[1] == 0 && data[2] == 0)
				rc = -STM_TS_ERROR_TIMEOUT_ZERO;

			input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);

			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM)
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
			break;
		}
		sec_delay(20);
	}

	input_info(true, &ts->client->dev,
			"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
			__func__, data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);

	mutex_unlock(&ts->fn_mutex);

	return rc;

}

int stm_ts_wait_for_echo_event(struct stm_ts_data *ts, u8 *cmd, u8 cmd_cnt, int delay)
{
	int rc;
	int i;
	bool matched = false;
	u8 reg = STM_TS_READ_ONE_EVENT;
	u8 data[STM_TS_EVENT_BUFF_SIZE];
	int retry = 0;

	mutex_lock(&ts->fn_mutex);
	disable_irq(ts->irq);

	rc = ts->stm_ts_write(ts, cmd, cmd_cnt, NULL, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write command\n", __func__);
		enable_irq(ts->irq);
		mutex_unlock(&ts->fn_mutex);
		return rc;
	}

	if (delay)
		sec_delay(delay);
	else
		sec_delay(5);

	memset(data, 0x0, STM_TS_EVENT_BUFF_SIZE);

	rc = -EIO;

	while (ts->stm_ts_read(ts, &reg, 1, (u8 *)data, STM_TS_EVENT_BUFF_SIZE) >= 0) {
		if (data[0] != 0x00)
			input_info(true, &ts->client->dev,
					"%s: event %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X,"
					" %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7],
					data[8], data[9], data[10], data[11],
					data[12], data[13], data[14], data[15]);

		if ((data[0] == STM_TS_EVENT_STATUS_REPORT) && (data[1] == 0x01)) {  // Check command ECHO
			int loop_cnt;

			if (cmd_cnt > 4)
				loop_cnt = 4;
			else
				loop_cnt = cmd_cnt;

			for (i = 0; i < loop_cnt; i++) {
				if (data[i + 2] != cmd[i]) {
					matched = false;
					break;
				}
				matched = true;
			}

			if (matched == true) {
				rc = 0;
				break;
			}
		} else if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			input_info(true, &ts->client->dev, "%s: Error detected %02X,%02X,%02X,%02X,%02X,%02X\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);

			if (retry >= STM_TS_RETRY_COUNT)
				break;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 50) {
			input_err(true, &ts->client->dev, "%s: Time Over (%02X,%02X,%02X,%02X,%02X,%02X)\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);
			break;
		}
		sec_delay(20);
	}

	enable_irq(ts->irq);
	mutex_unlock(&ts->fn_mutex);

	return rc;
}

int stm_ts_set_scanmode(struct stm_ts_data *ts, u8 scan_mode)
{
	u8 address[3] = { 0xA0, 0x00, scan_mode };
	int rc;

	rc = stm_ts_wait_for_echo_event(ts, &address[0], 3, 20);
	if (rc < 0) {
		input_info(true, &ts->client->dev, "%s: timeout, ret = %d\n", __func__, rc);
		return rc;
	}

	input_info(true, &ts->client->dev, "%s: 0x%02X\n", __func__, scan_mode);

	return 0;

}

/* optional reg : SEC_TS_CMD_LPM_AOD_OFF_ON(0x9B)	*/
/* 0 : Async base scan (default on lp mode)		*/
/* 1 : sync base scan				*/
int stm_ts_set_hsync_scanmode(struct stm_ts_data *ts, u8 scan_mode)
{
	u8 address[2] = { STM_TS_CMD_SET_LPM_AOD_OFF_ON, 0};
	int rc;

	input_info(true, &ts->client->dev, "%s: mode:%x\n", __func__, scan_mode);
	address[1] = scan_mode;

	rc = ts->stm_ts_write(ts, &address[0], 2, NULL, 0);
	if (rc < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send command: %x/%x",
					__func__, address[0], address[1]);
	return rc;
}

int stm_ts_set_touch_function(struct stm_ts_data *ts)
{
	int ret = 0;
	u8 address = 0;
	u8 data[2] = { 0 };

	address = STM_TS_CMD_SET_GET_TOUCHTYPE;
	data[0] = (u8)(ts->plat_data->touch_functions & 0xFF);
	data[1] = (u8)(ts->plat_data->touch_functions >> 8);
	ret = ts->stm_ts_write(ts, &address, 1, data, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
				__func__, STM_TS_CMD_SET_GET_TOUCHTYPE);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_functions, msecs_to_jiffies(30));

	return ret;
}

void stm_ts_get_touch_function(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			work_read_functions.work);
	int ret = 0;
	u8 address = 0;
	u8 data[2] = { 0 };

	address = STM_TS_CMD_SET_GET_TOUCHTYPE;
	data[0] = (u8)(ts->plat_data->touch_functions & 0xFF);
	data[1] = (u8)(ts->plat_data->touch_functions >> 8);
	ret = ts->stm_ts_read(ts, &address, 1, (u8 *)&(ts->plat_data->ic_status), 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to read touch functions(%d)\n",
				__func__, ret);
		return;
	}

	input_info(true, &ts->client->dev,
			"%s: touch_functions:%x ic_status:%x\n", __func__,
			ts->plat_data->touch_functions, ts->plat_data->ic_status);
}


int stm_ts_osc_trim_recovery(struct stm_ts_data *ts)
{
	u8 address[3];
	int rc;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	/*  OSC trim error recovery command. */
	address[0] = 0xA4;
	address[1] = 0x00;
	address[2] = 0x05;

	rc = stm_ts_wait_for_echo_event(ts, &address[0], 3, 800);
	if (rc < 0) {
		rc = -STM_TS_ERROR_BROKEN_OSC_TRIM;
		goto out;
	}

	/* save panel configuration area */
	address[0] = 0xA4;
	address[1] = 0x05;
	address[2] = 0x04;

	rc = stm_ts_wait_for_echo_event(ts, &address[0], 3, 100);
	if (rc < 0) {
		rc = -STM_TS_ERROR_BROKEN_OSC_TRIM;
		goto out;
	}

	sec_delay(500);
	rc = ts->stm_ts_systemreset(ts, 0);
	sec_delay(50);

out:
	return rc;
}

int stm_ts_set_vvc_mode(struct stm_ts_data *ts, bool enable)
{
	int ret = 0;
	u8 data[3];

	/* register */
	data[0] = 0xC1;
	data[1] = 0x17;
	if (enable)
		data[2] = ts->vvc_mode;
	else
		data[2] = 0x00;

	ret = ts->stm_ts_write(ts, data, 3, NULL, 0);
	input_info(true, &ts->client->dev, "%s: vvc mode write: %02X, ret: %d\n", __func__, data[2], ret);
	return ret;
}

int stm_ts_set_opmode(struct stm_ts_data *ts, u8 mode)
{
	int ret;
	u8 address[2] = {STM_TS_CMD_SET_GET_OPMODE, mode};

	ret = ts->stm_ts_write(ts, &address[0], 2, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write opmode", __func__);

	sec_delay(5);

	if (ts->plat_data->lowpower_mode) {
		address[0] = STM_TS_CMD_WRITE_WAKEUP_GESTURE;
		address[1] = 0x02;
		ret = ts->stm_ts_write(ts, &address[0], 1, &address[1], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to send lowpower flag command", __func__);
	}

	input_info(true, &ts->client->dev, "%s: opmode %d", __func__, mode);

	return ret;
}


void  stm_ts_set_utc_sponge(struct stm_ts_data *ts)
{
	struct timespec64 current_time;
	u8 data[6] = {STM_TS_CMD_SPONGE_OFFSET_UTC, 0};
	int ret = 0;

	ktime_get_real_ts64(&current_time);
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[4] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[5] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	input_info(true, &ts->client->dev, "Write UTC to Sponge = %X\n", (int)(current_time.tv_sec));


	ret = ts->stm_ts_write_sponge(ts, data, 6);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);
}


int stm_ts_set_lowpowermode(void *data, u8 mode)
{
	struct stm_ts_data *ts = (struct stm_ts_data *)data;
	int ret;
	int retrycnt = 0;
	char para = 0;
	u8 address = 0;

	input_err(true, &ts->client->dev, "%s: %s(%X)\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", ts->plat_data->lowpower_mode);

	if (mode) {
		stm_ts_set_utc_sponge(ts);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->sponge_dump_delayed_flag) {
				stm_ts_sponge_dump_flush(ts, ts->sponge_dump_delayed_area);
				ts->sponge_dump_delayed_flag = false;
				input_info(true, &ts->client->dev, "%s : Sponge dump flush delayed work have procceed\n", __func__);
			}
		}
#endif
		ret = stm_ts_set_custom_library(ts);
		if (ret < 0)
			goto error;
	} else {
		if (!ts->plat_data->shutdown_called)
			stm_ts_get_touch_function(&ts->work_read_functions.work);
	}

retry_pmode:
	if (mode) {
		stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, false);
		ret = stm_ts_set_opmode(ts, STM_TS_OPMODE_LOWPOWER);
	} else {
		ret = stm_ts_set_opmode(ts, STM_TS_OPMODE_NORMAL);
	}
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: stm_ts_set_opmode failed!\n", __func__);
		goto error;
	}

	sec_delay(ts->lpmode_change_delay);

	address = STM_TS_CMD_SET_GET_OPMODE;
	ret = ts->stm_ts_read(ts, &address, 1, &para, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read power mode failed!\n", __func__);
		retrycnt++;
		if (retrycnt < 10)
			goto retry_pmode;
	}

	input_info(true, &ts->client->dev, "%s: write(%d) read(%d) retry %d\n", __func__, mode, para, retrycnt);

	if (mode != para) {
		retrycnt++;
		ts->plat_data->hw_param.mode_change_failed_count++;
		if (retrycnt < 10)
			goto retry_pmode;
	}

	stm_ts_locked_release_all_finger(ts);

	if (device_may_wakeup(&ts->client->dev)) {
		if (mode)
			enable_irq_wake(ts->irq);
		else
			disable_irq_wake(ts->irq);
	}

	if (mode == TO_LOWPOWER_MODE)
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
	else
		ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

error:
	input_info(true, &ts->client->dev, "%s: end %d\n", __func__, ret);

	return ret;
}

void stm_ts_release_all_finger(struct stm_ts_data *ts)
{
	sec_input_release_all_finger(&ts->client->dev);
}

void stm_ts_locked_release_all_finger(struct stm_ts_data *ts)
{
	mutex_lock(&ts->eventlock);

	stm_ts_release_all_finger(ts);

	mutex_unlock(&ts->eventlock);
}

void stm_ts_reset(struct stm_ts_data *ts, unsigned int ms)
{
	input_info(true, &ts->client->dev, "%s: Recover IC, discharge time:%d\n", __func__, ms);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, false);

	sec_delay(ms);

	if (ts->plat_data->power)
		ts->plat_data->power(&ts->client->dev, true);

	sec_delay(TOUCH_POWER_ON_DWORK_TIME);
}

int stm_ts_reset_work_from_preparation_to_completion(struct stm_ts_data *ts, bool prepare, bool need_to_reset)
{
	if (prepare) {
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: secure touch enabled\n", __func__);
			return -1;
		}
#endif
		if (ts->reset_is_on_going) {
			input_err(true, &ts->client->dev, "%s: reset is ongoing\n", __func__);
			return -1;
		}

		mutex_lock(&ts->modechange);
		__pm_stay_awake(ts->plat_data->sec_ws);

		ts->reset_is_on_going = true;
	} else {
		char result[32];

		ts->reset_is_on_going = false;

		if (need_to_reset) {
			cancel_delayed_work(&ts->reset_work);
			if (!ts->plat_data->shutdown_called)
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
		}

		snprintf(result, sizeof(result), "RESULT=RESET");
		if (ts->probe_done)
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);

		mutex_unlock(&ts->modechange);
		__pm_relax(ts->plat_data->sec_ws);
	}

	return 0;
}

void stm_ts_reset_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data, reset_work.work);
	int ret;
	bool prepare = true;
	bool need_to_reset = false;

	ret = stm_ts_reset_work_from_preparation_to_completion(ts, prepare, need_to_reset);
	if (ret < 0)
		return;

	prepare = false;
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->stop_device(ts);
	sec_delay(TOUCH_POWER_ON_DWORK_TIME);

	ret = ts->plat_data->start_device(ts);
	if (ret < 0) {
		/* for ACT recovery fail test */
		char result[32];
		char test[32];
		snprintf(test, sizeof(test), "TEST=RECOVERY");
		snprintf(result, sizeof(result), "RESULT=FAIL");
		if (ts->probe_done)
			sec_cmd_send_event_to_user(&ts->sec, test, result);

		input_err(true, &ts->client->dev, "%s: Reset failure, ret:%d\n", __func__, ret);
		need_to_reset = true;
		goto reset_work_completion;
	}

	if (!ts->plat_data->enabled) {
		input_err(true, &ts->client->dev, "%s: call input_close\n", __func__);

		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable ||
			ts->plat_data->pocket_mode || ts->plat_data->fod_lp_mode) {
			ret = ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: Reset failure, ret:%d\n", __func__, ret);
				need_to_reset = true;
				goto reset_work_completion;
			}

			if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
				stm_ts_set_aod_rect(ts);
		} else {
			ts->plat_data->stop_device(ts);
		}
	}

	if ((ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) && (ts->fix_active_mode))
		stm_ts_fix_active_mode(ts, STM_TS_ACTIVE_TRUE);

reset_work_completion:
	stm_ts_reset_work_from_preparation_to_completion(ts, prepare, need_to_reset);
}

void stm_ts_print_info_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			work_print_info.work);

	sec_input_print_info(&ts->client->dev, ts->tdata);

	if (ts->sec.cmd_is_running)
		input_err(true, &ts->client->dev, "%s: skip set temperature, cmd running\n", __func__);
	else
		sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_NORMAL);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

#ifdef ENABLE_RAWDATA_SERVICE
void stm_ts_read_rawdata_address(struct stm_ts_data *ts)
{
	u8 reg[8];
	u8 header[16];
	int ret;
	int retry = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	disable_irq(ts->irq);

	reg[0] = 0xA4;
	reg[1] = 0x06;
	reg[2] = 0x01;

	ret = ts->stm_ts_write(ts, reg, 3, NULL, 0);
	sec_delay(50);
	do {
		memset(header, 0x00, 16);
		reg[0] = 0xA7;
		reg[1] = 0x00;
		reg[2] = 0x00;
		ret = ts->stm_ts_read(ts, reg, 3, header, 16);
		if (header[0] == 0xA5 && header[1] == 0x1)
			break;
		sec_delay(30);
	} while (retry--);

	reg[0] = 0xA7;
	reg[1] = 0x00;
	if (ts->raw_mode == 2)//RAW
		reg[2] = 0x88;
	else if (ts->raw_mode == 1)//STRENGTH
		reg[2] = 0x8C;
	memset(header, 0x00, 16);
	ret = ts->stm_ts_read(ts, reg, 3, header, 2);

	ts->raw_addr_h = header[1];
	ts->raw_addr_l = header[0];

	reg[0] = 0xA7;
	reg[1] = ts->raw_addr_h;
	reg[2] = ts->raw_addr_l;

	ret = ts->stm_ts_read(ts, reg, 3, ts->raw_u8, ts->tx_count * ts->rx_count * 2);
	enable_irq(ts->irq);
}
#endif

void stm_ts_read_info_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
			work_read_info.work);
	int ret;

#ifdef TCLM_CONCEPT
	ret = sec_tclm_check_cal_case(ts->tdata);
	input_info(true, &ts->client->dev, "%s: sec_tclm_check_cal_case ret: %d \n", __func__, ret);
#endif
	ret = stm_ts_get_tsp_test_result(ts);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get result\n",
				__func__);
	input_raw_info_d(true, &ts->client->dev, "%s: fac test result %02X\n",
				__func__, ts->test_result.data[0]);

	stm_ts_run_rawdata_all(ts);

	/* read cmoffset & fail history data at booting time */
	input_info(true, &ts->client->dev, "%s: read cm data in tsp ic\n", __func__);
	if (ts->plat_data->bringup == 0) {
		get_cmoffset_dump(ts, ts->cmoffset_sdc_proc, OFFSET_FW_SDC);
		get_cmoffset_dump(ts, ts->cmoffset_sub_proc, OFFSET_FW_SUB);
		get_cmoffset_dump(ts, ts->cmoffset_main_proc, OFFSET_FW_MAIN);
	}

	ts->info_work_done = true;

	/* reinit */
	ts->plat_data->init(ts);

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
		return;
	}

	schedule_work(&ts->work_print_info.work);

	if (ts->change_flip_status) {
		input_info(true, &ts->client->dev, "%s: re-try switching after reading info\n", __func__);
		schedule_work(&ts->switching_work.work);
	}
}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
void stm_ts_interrupt_notify(struct work_struct *work)
{
	struct sec_ts_plat_data *pdata = container_of(work, struct sec_ts_plat_data,
			interrupt_notify_work.work);
	struct sec_input_notify_data data;
	data.dual_policy = MAIN_TOUCHSCREEN;
	if (pdata->touch_count > 0)
		sec_input_notify(NULL, NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST, &data);
	else
		sec_input_notify(NULL, NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE, &data);
}
#endif

void stm_ts_set_cover_type(struct stm_ts_data *ts, bool enable)
{
	int ret;
	u8 address;
	u8 cover_cmd;
	u8 data[2] = { 0 };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->cover_type);

	cover_cmd = sec_input_check_cover_type(&ts->client->dev) & 0xFF;

	if (enable) {
		address = STM_TS_CMD_SET_GET_COVERTYPE;
		data[0] = cover_cmd;
		ret = ts->stm_ts_write(ts, &address, 1, data, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to send covertype command: %d",
					__func__, cover_cmd);
		}

		ts->plat_data->touch_functions = ts->plat_data->touch_functions | STM_TS_TOUCHTYPE_BIT_COVER;

	} else {
		ts->plat_data->touch_functions = (ts->plat_data->touch_functions & (~STM_TS_TOUCHTYPE_BIT_COVER));
	}

	ret = stm_ts_set_touch_function(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send touch type command: 0x%02X%02X",
				__func__, data[0], data[1]);
	}

}
EXPORT_SYMBOL(stm_ts_set_cover_type);

int stm_ts_set_temperature(struct device *dev, u8 temperature_data)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	u8 address;

	address = STM_TS_CMD_SET_LOWTEMPERATURE_MODE;

	return ts->stm_ts_write(ts, &address, 1,  &temperature_data, 1);
}
int stm_ts_set_aod_rect(struct stm_ts_data *ts)
{
	u8 data[10] = {0x02, 0};
	int ret, i;

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 3] = (ts->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->stm_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	return ret;
}

int stm_ts_set_press_property(struct stm_ts_data *ts)
{
	u8 data[3] = { SEC_TS_CMD_SPONGE_PRESS_PROPERTY, 0 };
	int ret;

	if (!ts->plat_data->support_fod)
		return 0;

	data[2] = ts->plat_data->fod_data.press_prop;

	ret = ts->stm_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->fod_data.press_prop);

	return ret;
}

int stm_ts_set_fod_rect(struct stm_ts_data *ts)
{
	u8 data[10] = {0x4b, 0};
	int ret, i;

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
		__func__, ts->plat_data->fod_data.rect_data[0], ts->plat_data->fod_data.rect_data[1],
		ts->plat_data->fod_data.rect_data[2], ts->plat_data->fod_data.rect_data[3]);

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->plat_data->fod_data.rect_data[i] & 0xFF;
		data[i * 2 + 3] = (ts->plat_data->fod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->stm_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	return ret;
}

int stm_ts_set_wirelesscharger_mode(struct stm_ts_data *ts)
{
	int ret;
	u8 address;
	u8 data;

	if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER_NONE) {
		data = STM_TS_BIT_CHARGER_MODE_NORMAL;
	} else if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER) {
		data = STM_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER;
	} else if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_BATTERY_PACK) {
		data = STM_TS_BIT_CHARGER_MODE_WIRELESS_BATTERY_PACK;
	} else {
		input_err(true, &ts->client->dev, "%s: not supported mode %d\n",
				__func__, ts->plat_data->wirelesscharger_mode);
		return SEC_ERROR;
	}

	address = STM_TS_CMD_SET_GET_WIRELESSCHARGER_MODE;
	ret = ts->stm_ts_write(ts, &address, 1, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: Failed to write mode 0x%02X (cmd:%d), ret=%d\n",
				__func__, address, ts->plat_data->wirelesscharger_mode, ret);
	else
		input_info(true, &ts->client->dev, "%s: %sabled, mode=%d\n", __func__,
				ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER_NONE ? "dis" : "en",
				ts->plat_data->wirelesscharger_mode);

	return ret;
}

int stm_ts_set_wirecharger_mode(struct stm_ts_data *ts)
{
	int ret;
	u8 address;
	u8 data;

	if (ts->charger_mode == TYPE_WIRE_CHARGER_NONE) {
		data = STM_TS_BIT_CHARGER_MODE_NORMAL;
	} else if (ts->charger_mode == TYPE_WIRE_CHARGER) {
		data = STM_TS_BIT_CHARGER_MODE_WIRE_CHARGER;
	} else {
		input_err(true, &ts->client->dev, "%s: not supported mode %d\n",
				__func__, ts->plat_data->wirelesscharger_mode);
		return SEC_ERROR;
	}

	address = STM_TS_CMD_SET_GET_WIRECHARGER_MODE;
	ret = ts->stm_ts_write(ts, &address, 1, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: Failed to write mode 0x%02X (cmd:%d), ret=%d\n",
				__func__, address, ts->charger_mode, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
int stm_ts_vbus_notification(struct notifier_block *nb, unsigned long cmd, void *data)
{
	struct stm_ts_data *ts = container_of(nb, struct stm_ts_data, vbus_nb);
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	if (ts->plat_data->shutdown_called)
		return 0;

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		ts->charger_mode = TYPE_WIRE_CHARGER;
		break;
	case STATUS_VBUS_LOW:
		ts->charger_mode = TYPE_WIRE_CHARGER_NONE;
		break;
	default:
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: %sabled\n", __func__,
				ts->charger_mode == TYPE_WIRE_CHARGER_NONE ? "dis" : "en");

	stm_ts_set_wirecharger_mode(ts);

out:
	return 0;
}
#endif

/*
 *	flag     1  :  set edge handler
 *		2  :  set (portrait, normal) edge zone data
 *		4  :  set (portrait, normal) dead zone data
 *		8  :  set landscape mode data
 *		16 :  mode clear
 *	data
 *		0xAA, FFF (y start), FFF (y end),  FF(direction)
 *		0xAB, FFFF (edge zone)
 *		0xAC, FF (up x), FF (down x), FFFF (up y), FF (bottom x), FFFF (down y)
 *		0xAD, FF (mode), FFF (edge), FFF (dead zone x), FF (dead zone top y), FF (dead zone bottom y)
 *	case
 *		edge handler set :  0xAA....
 *		booting time :  0xAA...  + 0xAB...
 *		normal mode : 0xAC...  (+0xAB...)
 *		landscape mode : 0xAD...
 *		landscape -> normal (if same with old data) : 0xAD, 0
 *		landscape -> normal (etc) : 0xAC....  + 0xAD, 0
 */

void stm_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	u8 data[9] = { 0 };
	u8 address[2] = {STM_TS_CMD_SET_FUNCTION_ONOFF,};

	input_info(true, &ts->client->dev, "%s: flag: %02X (clr,lan,nor,edg,han)\n", __func__, flag);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->plat_data->grip_data.edgehandler_direction == 0) {
			data[0] = 0x0;
			data[1] = 0x0;
			data[2] = 0x0;
			data[3] = 0x0;
		} else {
			data[0] = (ts->plat_data->grip_data.edgehandler_start_y >> 4) & 0xFF;
			data[1] = (ts->plat_data->grip_data.edgehandler_start_y << 4 & 0xF0)
					| ((ts->plat_data->grip_data.edgehandler_end_y >> 8) & 0xF);
			data[2] = ts->plat_data->grip_data.edgehandler_end_y & 0xFF;
			data[3] = ts->plat_data->grip_data.edgehandler_direction & 0x3;
		}
		address[1] = STM_TS_FUNCTION_EDGE_HANDLER;
		ts->stm_ts_write(ts, address, 2, data, 4);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X\n",
				__func__, STM_TS_FUNCTION_EDGE_HANDLER, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_EDGE_ZONE) {
		data[0] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		data[1] = ts->plat_data->grip_data.edge_range  & 0xFF;
		data[2] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		data[3] = ts->plat_data->grip_data.edge_range  & 0xFF;
		address[1] = STM_TS_FUNCTION_EDGE_AREA;
		ts->stm_ts_write(ts, address, 2, data, 4);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X\n",
				__func__, STM_TS_FUNCTION_EDGE_AREA, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_NORMAL_MODE) {
		data[0] = ts->plat_data->grip_data.deadzone_up_x & 0xFF;
		data[1] = ts->plat_data->grip_data.deadzone_dn_x & 0xFF;
		data[2] = (ts->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		data[3] = ts->plat_data->grip_data.deadzone_y & 0xFF;
		address[1] = STM_TS_FUNCTION_DEAD_ZONE;
		ts->stm_ts_write(ts, address, 2, data, 4);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X\n",
				__func__, STM_TS_FUNCTION_DEAD_ZONE, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		data[0] = ts->plat_data->grip_data.landscape_mode;
		data[1] = (ts->plat_data->grip_data.landscape_edge >> 8) & 0xFF;
		data[2] = ts->plat_data->grip_data.landscape_edge & 0xFF;
		data[3] = (ts->plat_data->grip_data.landscape_edge >> 8) & 0xFF;
		data[4] = ts->plat_data->grip_data.landscape_edge & 0xFF;
		data[5] = (ts->plat_data->grip_data.landscape_deadzone >> 8) & 0xFF;
		data[6] = ts->plat_data->grip_data.landscape_deadzone & 0xFF;

		address[1] = STM_TS_FUNCTION_LANDSCAPE_MODE;
		ts->stm_ts_write(ts, address, 2, data, 7);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X, %02X,%02X,%02X\n",
				__func__, STM_TS_FUNCTION_LANDSCAPE_MODE, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);


	
		data[0] = ts->plat_data->grip_data.landscape_mode;
		data[1] = (ts->plat_data->grip_data.landscape_top_gripzone >> 8)& 0xFF;
		data[2] = ts->plat_data->grip_data.landscape_top_gripzone & 0xFF;
		data[3] = (ts->plat_data->grip_data.landscape_bottom_gripzone >> 8)& 0xFF;
		data[4] = ts->plat_data->grip_data.landscape_bottom_gripzone & 0xFF;
		data[5] = (ts->plat_data->grip_data.landscape_top_deadzone >> 8)& 0xFF;
		data[6] = ts->plat_data->grip_data.landscape_top_deadzone & 0xFF;
		data[7] = (ts->plat_data->grip_data.landscape_bottom_deadzone >> 8)& 0xFF;
		data[8] = ts->plat_data->grip_data.landscape_bottom_deadzone & 0xFF;
	
		address[1] = STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM;
		ts->stm_ts_write(ts, address, 2, data, 9);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X, %02X,%02X,%02X,%02X,%02X\n",
				__func__, STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM, data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7], data[8]);
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		memset(data, 0x00, 9);
		data[0] = ts->plat_data->grip_data.landscape_mode;
		address[1] = STM_TS_FUNCTION_LANDSCAPE_MODE;
		ts->stm_ts_write(ts, address, 2, data, 7);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n",
				__func__, STM_TS_FUNCTION_LANDSCAPE_MODE, data[0]);

		address[1] = STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM;
		ts->stm_ts_write(ts, address, 2, data, 9);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n",
				__func__, STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM, data[0]);
	}
}

/*
 * Enable or disable external_noise_mode
 *
 * If mode has EXT_NOISE_MODE_MAX,
 * then write enable cmd for all enabled mode. (set as ts->plat_data->external_noise_mode bit value)
 * This routine need after IC power reset. TSP IC need to be re-wrote all enabled modes.
 *
 * Else if mode has specific value like EXT_NOISE_MODE_MONITOR,
 * then write enable/disable cmd about for that mode's latest setting value.
 *
 * If you want to add new mode,
 * please define new enum value like EXT_NOISE_MODE_MONITOR,
 * then set cmd for that mode like below. (it is in this function)
 * noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = stm_TS_CMD_SET_MONITOR_NOISE_MODE;
 */
int stm_ts_set_external_noise_mode(struct stm_ts_data *ts, u8 mode)
{
	int i, ret, fail_count = 0;
	u8 mode_bit_to_set, check_bit, mode_enable;
	u8 noise_mode_cmd[EXT_NOISE_MODE_MAX] = { 0 };
	u8 address = STM_TS_CMD_SET_FUNCTION_ONOFF;
	u8 data[2] = { 0 };

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (mode == EXT_NOISE_MODE_MAX) {
		/* write all enabled mode */
		mode_bit_to_set = ts->plat_data->external_noise_mode;
	} else {
		/* make enable or disable the specific mode */
		mode_bit_to_set = 1 << mode;
	}

	input_info(true, &ts->client->dev, "%s: %sable %d\n", __func__,
			ts->plat_data->external_noise_mode & mode_bit_to_set ? "en" : "dis", mode_bit_to_set);

	/* set cmd for each mode */
	noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = STM_TS_FUNCTION_SET_MONITOR_NOISE_MODE;

	/* write mode */
	for (i = EXT_NOISE_MODE_NONE + 1; i < EXT_NOISE_MODE_MAX; i++) {
		check_bit = 1 << i;
		if (mode_bit_to_set & check_bit) {
			mode_enable = !!(ts->plat_data->external_noise_mode & check_bit);
			data[0] = noise_mode_cmd[i];
			data[1] = mode_enable;
			ret = ts->stm_ts_write(ts, &address, 1, data, 2);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: failed to set 0x%02X %d\n",
						__func__, noise_mode_cmd[i], mode_enable);
				fail_count++;
			}
		}
	}

	if (fail_count != 0)
		return -EIO;
	else
		return 0;
}

int stm_ts_set_touchable_area(struct stm_ts_data *ts)
{
	int ret;
	u8 address[2] = {STM_TS_CMD_SET_FUNCTION_ONOFF, STM_TS_FUNCTION_SET_TOUCHABLE_AREA};
	input_info(true, &ts->client->dev,
			"%s: set 16:9 mode %s\n", __func__,
			ts->plat_data->touchable_area ? "enable" : "disable");

	ret = ts->stm_ts_write(ts, address, 2, &ts->plat_data->touchable_area, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set 16:9 mode, ret=%d\n", __func__, ret);
	return ret;
}

int stm_ts_ear_detect_enable(struct stm_ts_data *ts, u8 enable)
{
	int ret;
	u8 address = STM_TS_CMD_SET_EAR_DETECT;
	u8 data = enable;

	input_info(true, &ts->client->dev, "%s: enable:%d\n", __func__, enable);

	/* 00: off, 01:Mutual, 10:Self, 11: Mutual+Self */
	ret = ts->stm_ts_write(ts, &address, 1, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set ed_enable %d, ret=%d\n", __func__, data, ret);
	return ret;
}

int stm_ts_pocket_mode_enable(struct stm_ts_data *ts, u8 enable)
{
	int ret;
	u8 address = STM_TS_CMD_SET_POCKET_MODE;
	u8 data = enable;

	input_info(true, &ts->client->dev, "%s: %s\n",
			__func__, ts->plat_data->pocket_mode ? "enable" : "disable");

	ret = ts->stm_ts_write(ts, &address, 1, &data, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to pocket mode%d, ret=%d\n", __func__, data, ret);
	return ret;
}

#define NVM_CMD(mtype, moffset, mlength)		.type = mtype,	.offset = moffset,	.length = mlength
struct stm_ts_nvm_data_map nvm_data[] = {
	{NVM_CMD(0,						0x00, 0),},
	{NVM_CMD(STM_TS_NVM_OFFSET_FAC_RESULT,			0x00, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_COUNT,			0x01, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT,		0x02, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_TUNE_VERSION,		0x03, 2),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_POSITION,		0x05, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT,		0x06, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP,		0x07, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO,		0x08, 20),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_FAIL_FLAG,		0x1C, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_FAIL_COUNT,		0x1D, 1),},	/* SEC */
};

int get_nvm_data(struct stm_ts_data *ts, int type, u8 *nvdata)
{
	int size = sizeof(nvm_data) / sizeof(struct stm_ts_nvm_data_map);

	if (type >= size)
		return -EINVAL;

	return get_nvm_data_by_size(ts, nvm_data[type].offset, nvm_data[type].length, nvdata);
}

int set_nvm_data(struct stm_ts_data *ts, u8 type, u8 *buf)
{
	return set_nvm_data_by_size(ts, nvm_data[type].offset, nvm_data[type].length, buf);
}


int get_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *nvdata)
{
	u8 address[3] = {0};
	u8 data[128] = { 0 };
	int ret;

	sec_delay(200);
	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO

	stm_ts_release_all_finger(ts);

	// Request SEC factory debug data from flash
	address[0] = 0xA4;
	address[1] = 0x06;
	address[2] = 0x90;

	ret = stm_ts_wait_for_echo_event(ts, &address[0], 3, 50);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: timeout. ret: %d\n", __func__, ret);
		return ret;
	}


	address[0] = STM_TS_CMD_FRM_BUFF_R;
	address[1] = 0x00;
	address[2] = offset;

	ret = ts->stm_ts_read(ts, &address[0], 3, data, length + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: read failed. ret: %d\n", __func__, ret);
		return ret;
	}

	memcpy(nvdata, &data[0], length);

	input_raw_info_d(true, &ts->client->dev, "%s: offset [%d], length [%d]\n",
			__func__, offset, length);

	return ret;
}

int set_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *buf)
{
	u8 buff[256] = { 0 };
	u8 remaining, index, sendinglength;
	int ret;

	sec_delay(200);
	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO

	stm_ts_release_all_finger(ts);

	remaining = length;
	index = 0;
	sendinglength = 0;

	while (remaining) {
		buff[0] = 0xC7;
		buff[1] = 0x01;
		buff[2] = offset + index;

		// write data up to 12 bytes available
		if (remaining < 13) {
			memcpy(&buff[3], &buf[index], remaining);
			sendinglength = remaining;
		} else {
			memcpy(&buff[3], &buf[index], 12);
			index += 12;
			sendinglength = 12;
		}

		ret = ts->stm_ts_write(ts, &buff[0], sendinglength + 3, NULL, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: write failed. ret: %d\n", __func__, ret);
			return ret;
		}
		remaining -= sendinglength;
	}

	// Save to flash
	buff[0] = 0xA4;
	buff[1] = 0x05;
	buff[2] = 0x04; // panel configuration area

	ret = stm_ts_wait_for_echo_event(ts, &buff[0], 3, 200);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to get echo. ret: %d\n", __func__, ret);
	return ret;

}

int _stm_tclm_data_read(struct stm_ts_data *ts, int address)
{
	int ret = 0;
	int i = 0;
	u8 nbuff[STM_TS_NVM_OFFSET_ALL];
	u16 ic_version;

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		ret = stm_ts_get_version_info(ts);
		ic_version = (ts->module_version_of_ic << 8) | (ts->fw_main_version_of_ic & 0xFF);
		return ic_version;
	case SEC_TCLM_NVM_ALL_DATA:
		ret = get_nvm_data_by_size(ts, nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset,
				STM_TS_NVM_OFFSET_ALL, nbuff);
		if (ret < 0)
			return ret;
		ts->tdata->nvdata.cal_count = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_COUNT].offset];
		ts->tdata->nvdata.tune_fix_ver = (nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset] << 8) |
							nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset + 1];
		ts->tdata->nvdata.cal_position = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_POSITION].offset];
		ts->tdata->nvdata.cal_pos_hist_cnt = nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT].offset];
		ts->tdata->nvdata.cal_pos_hist_lastp = nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP].offset];
		for (i = nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset;
				i < nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset +
				nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].length; i++)
			ts->tdata->nvdata.cal_pos_hist_queue[i - nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset] = nbuff[i];

		ts->tdata->nvdata.cal_fail_falg = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_FLAG].offset];
		ts->tdata->nvdata.cal_fail_cnt= nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_COUNT].offset];
		ts->fac_nv = nbuff[nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset];
		ts->disassemble_count = nbuff[nvm_data[STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT].offset];
		return ret;
	case SEC_TCLM_NVM_TEST:
		input_info(true, &ts->client->dev, "%s: dt: tclm_level [%d] afe_base [%04X]\n",
			__func__, ts->tdata->tclm_level, ts->tdata->afe_base);
		ret = get_nvm_data_by_size(ts, STM_TS_NVM_OFFSET_ALL + SEC_TCLM_NVM_OFFSET,
			SEC_TCLM_NVM_OFFSET_LENGTH, ts->tdata->tclm);
		if (ts->tdata->tclm[0] != 0xFF) {
			ts->tdata->tclm_level = ts->tdata->tclm[0];
			ts->tdata->afe_base = (ts->tdata->tclm[1] << 8) | ts->tdata->tclm[2];
			input_info(true, &ts->client->dev, "%s: nv: tclm_level [%d] afe_base [%04X]\n",
				__func__, ts->tdata->tclm_level, ts->tdata->afe_base);
		}
		return ret;
	default:
		return ret;
	}

}

int _stm_tclm_data_write(struct stm_ts_data *ts, int address)
{
	int ret = 1;
	int i = 0;
	u8 nbuff[STM_TS_NVM_OFFSET_ALL];

	switch (address) {
	case SEC_TCLM_NVM_ALL_DATA:
		memset(nbuff, 0x00, STM_TS_NVM_OFFSET_ALL);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset] = ts->fac_nv;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT].offset] = ts->disassemble_count;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_COUNT].offset] = ts->tdata->nvdata.cal_count;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset] = (u8)(ts->tdata->nvdata.tune_fix_ver >> 8);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset + 1] = (u8)(0xff & ts->tdata->nvdata.tune_fix_ver);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_POSITION].offset] = ts->tdata->nvdata.cal_position;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT].offset] = ts->tdata->nvdata.cal_pos_hist_cnt;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP].offset] = ts->tdata->nvdata.cal_pos_hist_lastp;
		for (i = nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset;
				i < nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset +
				nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].length; i++)
			nbuff[i] = ts->tdata->nvdata.cal_pos_hist_queue[i - nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset];
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_FLAG].offset] = ts->tdata->nvdata.cal_fail_falg;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_COUNT].offset] = ts->tdata->nvdata.cal_fail_cnt;
		ret = set_nvm_data_by_size(ts, nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset, STM_TS_NVM_OFFSET_ALL, nbuff);
		return ret;
	case SEC_TCLM_NVM_TEST:
		ret = set_nvm_data_by_size(ts, STM_TS_NVM_OFFSET_ALL + SEC_TCLM_NVM_OFFSET,
			SEC_TCLM_NVM_OFFSET_LENGTH, ts->tdata->tclm);
		return ret;
	default:
		return ret;

	}
}

void stm_chk_tsp_ic_status(struct stm_ts_data *ts, int call_pos)
{
	u8 data[3] = { 0 };

	if (!ts->probe_done) {
		input_info(true, &ts->client->dev, "%s not finished probe_done\n", __func__);
		return;
	}

	mutex_lock(&ts->status_mutex);

	input_info(true, &ts->client->dev,
			"%s: START: pos[%d] power_state[0x%X] lowpower_flag[0x%X] %sfolding\n",
			__func__, call_pos, ts->plat_data->power_state, ts->plat_data->lowpower_mode,
			ts->flip_status_current ? "" : "un");

	if (ts->plat_data->support_dual_foldable == MAIN_TOUCH) {
		if (call_pos == STM_TS_STATE_CHK_POS_OPEN) {
			input_dbg(true, &ts->client->dev, "%s(main): OPEN : Nothing\n", __func__);
		} else if (call_pos == STM_TS_STATE_CHK_POS_CLOSE) {
			input_dbg(true, &ts->client->dev, "%s(main): CLOSE: Nothing\n", __func__);
		} else if (call_pos == STM_TS_STATE_CHK_POS_HALL) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->flip_status_current == STM_TS_STATUS_FOLDING) {
				input_info(true, &ts->client->dev, "%s(main): HALL : TSP IC LP => IC OFF\n", __func__);
				ts->plat_data->stop_device(ts);

			} else {
				input_info(true, &ts->client->dev, "%s(main): HALL : Nothing\n", __func__);
			}
		} else if (call_pos == STM_TS_STATE_CHK_POS_SYSFS) {
			if (ts->flip_status_current == STM_TS_STATUS_UNFOLDING) {
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF && ts->plat_data->lowpower_mode) {
					input_info(true, &ts->client->dev, "%s(main): SYSFS: TSP IC OFF => LP mode[0x%X]\n",
									__func__, ts->plat_data->lowpower_mode);
					ts->plat_data->start_device(ts);
					ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);

				} else if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->plat_data->lowpower_mode == 0) {
					input_info(true, &ts->client->dev, "%s(main): SYSFS: LP mode [0x0] => TSP IC OFF\n",
									__func__);
					ts->plat_data->stop_device(ts);
				} else {
					input_info(true, &ts->client->dev, "%s(main): SYSFS: set lowpower_flag again[0x%X]\n",
									__func__, ts->plat_data->lowpower_mode);

					data[2] = ts->plat_data->lowpower_mode;
					ts->stm_ts_write_sponge(ts, data, 3);
				}
			} else {
				input_info(true, &ts->client->dev, "%s(main): SYSFS: folding nothing[0x%X]\n", __func__, ts->plat_data->lowpower_mode);
			}
		} else {
			input_info(true, &ts->client->dev, "%s(main): ETC  : nothing!\n", __func__);
		}
	} else if (ts->plat_data->support_dual_foldable == SUB_TOUCH) {
		if (call_pos == STM_TS_STATE_CHK_POS_OPEN) {
			input_dbg(true, &ts->client->dev, "%s(sub): OPEN  : Notthing\n", __func__);

		} else if (call_pos == STM_TS_STATE_CHK_POS_CLOSE) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->flip_status_current == STM_TS_STATUS_UNFOLDING) {
				input_info(true, &ts->client->dev, "%s(sub): HALL  : TSP IC LP => IC OFF\n", __func__);
				ts->plat_data->stop_device(ts);
			}
		} else if (call_pos == STM_TS_STATE_CHK_POS_HALL) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->flip_status_current == STM_TS_STATUS_UNFOLDING) {
				input_info(true, &ts->client->dev, "%s(sub): HALL  : TSP IC LP => IC OFF\n", __func__);
				ts->plat_data->stop_device(ts);
			} else if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF && ts->flip_status_current == STM_TS_STATUS_FOLDING && ts->plat_data->lowpower_mode != 0) {
				input_info(true, &ts->client->dev, "%s(sub): HALL  : TSP IC OFF => LP[0x%X]\n", __func__, ts->plat_data->lowpower_mode);
				ts->plat_data->start_device(ts);
				ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
			} else {
				input_info(true, &ts->client->dev, "%s(sub): HALL  : nothing!\n", __func__);
			}

		} else if (call_pos == STM_TS_STATE_CHK_POS_SYSFS) {
			if (ts->flip_status_current == STM_TS_STATUS_FOLDING) {
				if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF && ts->plat_data->lowpower_mode) {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : TSP IC OFF => LP mode[0x%X]\n", __func__, ts->plat_data->lowpower_mode);
					ts->plat_data->start_device(ts);
					ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);

				} else if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->plat_data->lowpower_mode == 0) {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : LP mode [0x0] => IC OFF\n", __func__);
					ts->plat_data->stop_device(ts);

				} else if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM && ts->plat_data->lowpower_mode != 0) {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : call LP mode again\n", __func__);
					ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);

				} else {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : nothing!\n", __func__);
				}
			} else {
				if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : rear selfie off => IC OFF\n", __func__);
					ts->plat_data->stop_device(ts);
				} else {
					input_info(true, &ts->client->dev, "%s(sub): SYSFS : unfolding nothing[0x%X]\n", __func__, ts->plat_data->lowpower_mode);
				}
			}

		} else {
			input_info(true, &ts->client->dev, "%s(sub): Abnormal case\n", __func__);
		}
	}

	input_info(true, &ts->client->dev, "%s: END  : pos[%d] power_state[0x%X] lowpower_flag[0x%X]\n",
					__func__, call_pos, ts->plat_data->power_state, ts->plat_data->lowpower_mode);

	mutex_unlock(&ts->status_mutex);
}

void stm_switching_work(struct work_struct *work)
{
	struct stm_ts_data *ts = container_of(work, struct stm_ts_data,
				switching_work.work);

	if (ts == NULL) {
		input_err(true, NULL, "%s: tsp ts is null\n", __func__);
		return;
	}

	if (ts->flip_status != ts->flip_status_current) {
		if (!ts->info_work_done) {
			input_err(true, &ts->client->dev, "%s: info_work is not done yet\n", __func__);
			ts->change_flip_status = 1;
			return;
		}
		ts->change_flip_status = 0;

		mutex_lock(&ts->switching_mutex);
		ts->flip_status = ts->flip_status_current;

		if (ts->flip_status == 0) {
			/* open : main_tsp on */
		} else {
			/* close : main_tsp off */
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
//			secure_touch_stop(ts, 1);
#endif
		}

		stm_chk_tsp_ic_status(ts, STM_TS_STATE_CHK_POS_HALL);
		mutex_unlock(&ts->switching_mutex);
	} else if (ts->plat_data->support_flex_mode && (ts->plat_data->support_dual_foldable == MAIN_TOUCH)) {
		input_info(true, &ts->client->dev, "%s support_flex_mode\n", __func__);

		mutex_lock(&ts->switching_mutex);
		stm_chk_tsp_ic_status(ts, STM_TS_STATE_CHK_POS_SYSFS);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_MAIN_TOUCH_ON, NULL);
#endif
		mutex_unlock(&ts->switching_mutex);
	}
}

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
int stm_hall_ic_notify(struct notifier_block *nb,
			unsigned long flip_cover, void *v)
{
	struct stm_ts_data *ts = container_of(nb, struct stm_ts_data,
				hall_ic_nb);
	struct hall_notifier_context *hall_notifier;

	if (ts == NULL) {
		input_err(true, NULL, "%s: tsp info is null\n", __func__);
		return 0;
	}

	hall_notifier = v;

	if (strncmp(hall_notifier->name, "flip", 4)) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, hall_notifier->name);

		return 0;
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__,
			 flip_cover ? "close" : "open");

	cancel_delayed_work(&ts->switching_work);

	ts->flip_status_current = flip_cover;

	schedule_work(&ts->switching_work.work);

	return 0;
}
#endif

MODULE_LICENSE("GPL");
