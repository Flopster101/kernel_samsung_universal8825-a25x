/* drivers/input/sec_input/slsi/slsi_fn.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "slsi_dev.h"
#include "slsi_reg.h"

int slsi_ts_read_from_sponge(struct slsi_ts_data *ts, u8 *data, int len)
{
	int ret;

	mutex_lock(&ts->sponge_mutex);
	ret = ts->slsi_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);

	ret = ts->slsi_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_READ_PARAM, (u8 *)data, len);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);
	mutex_unlock(&ts->sponge_mutex);

	return ret;
}

int slsi_ts_write_to_sponge(struct slsi_ts_data *ts, u8 *data, int len)
{
	int ret;

	mutex_lock(&ts->sponge_mutex);
	ret = ts->slsi_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_WRITE_PARAM, data, len);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write offset\n", __func__);

	ret = ts->slsi_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_NOTIFY_PACKET, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send notify\n", __func__);
	mutex_unlock(&ts->sponge_mutex);

	return ret;
}

void slsi_ts_set_utc_sponge(struct slsi_ts_data *ts)
{
	int ret;
	u8 data[6] = {SEC_TS_CMD_SPONGE_OFFSET_UTC, 0};
	struct timespec64 current_time;

	ktime_get_real_ts64(&current_time);
	data[2] = (0xFF & (u8)((current_time.tv_sec) >> 0));
	data[3] = (0xFF & (u8)((current_time.tv_sec) >> 8));
	data[4] = (0xFF & (u8)((current_time.tv_sec) >> 16));
	data[5] = (0xFF & (u8)((current_time.tv_sec) >> 24));
	input_info(true, &ts->client->dev, "Write UTC to Sponge = %X\n", (int)(current_time.tv_sec));

	ret = ts->slsi_ts_write_sponge(ts, data, 6);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);
}

int slsi_ts_set_custom_library(struct slsi_ts_data *ts)
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

	ret = ts->slsi_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

		/* read dump info */
	data[0] = SEC_TS_CMD_SPONGE_LP_DUMP;

	ret = ts->slsi_ts_read_sponge(ts, data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read dump_data\n", __func__);
		return ret;
	}

	ts->sponge_inf_dump = (data[0] & SEC_TS_SPONGE_DUMP_INF_MASK) >> SEC_TS_SPONGE_DUMP_INF_SHIFT;
	ts->sponge_dump_format = data[0] & SEC_TS_SPONGE_DUMP_EVENT_MASK;
	ts->sponge_dump_event = data[1];
	ts->sponge_dump_border = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT 
					+ (ts->sponge_dump_format * ts->sponge_dump_event);
	ts->sponge_dump_border_lsb = ts->sponge_dump_border & 0xFF;
	ts->sponge_dump_border_msb = (ts->sponge_dump_border & 0xFF00) >> 8;

	return ret;
}

void slsi_ts_get_custom_library(struct slsi_ts_data *ts)
{
	u8 data[6] = { 0 };
	int ret, i;

	data[0] = SEC_TS_CMD_SPONGE_AOD_ACTIVE_INFO;
	ret = ts->slsi_ts_read_sponge(ts, data, 6);
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
	ret = ts->slsi_ts_read_sponge(ts, data, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read fod info\n", __func__);
		return;
	}

	sec_input_set_fod_info(&ts->client->dev, data[0], data[1], data[2]);
}

int slsi_ts_wait_for_ready(struct slsi_ts_data *ts, u8 reg, u8 *data, int len, int delay)
{
	int rc;
	int retry = 0;
	u8 buf[SLSI_TS_EVENT_BUFF_SIZE] = {0,};
	u8 ack;

	switch (reg) {
	case SLSI_TS_ACK_BOOT_COMPLETE:
	case SLSI_TS_CMD_SW_RESET:
		ack = SLSI_TS_ACK_BOOT_COMPLETE;
		break;
	case SLSI_TS_CMD_SELFTEST:
		ack = SLSI_TS_VENDOR_ACK_SELF_TEST_DONE;
		break;
	case SLSI_TS_CMD_FACTORY_PANELCALIBRATION:
	case SLSI_TS_CMD_CALIBRATION_AMBIENT:
		ack = SLSI_TS_VENDOR_ACK_OFFSET_CAL_DONE;
		break;
	default:
		input_err(true, &ts->client->dev, "%s: need to decide ack for this cmd 0x%02X\n", __func__, reg);
		return SEC_ERROR;
	}

	mutex_lock(&ts->fn_mutex);

	if (reg != SLSI_TS_ACK_BOOT_COMPLETE) {
		rc = ts->slsi_ts_i2c_write(ts, reg, data, len);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: Send 0x%02X cmd failed!\n", __func__, reg);
			goto out;
		}

		sec_delay(delay);
	}

	rc = SEC_ERROR;
	while (retry <= SEC_TS_WAIT_RETRY_CNT) {
		if (gpio_get_value(ts->plat_data->irq_gpio) == 0) {
			if (ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_EVENT, buf, SLSI_TS_EVENT_BUFF_SIZE) > 0) {
				if (((buf[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_INFO) {
					if (buf[1] == ack) {
						rc = SEC_SUCCESS;
						break;
					}
				} else if (((buf[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_VENDOR_INFO) {
					if (buf[1] == ack) {
						rc = SEC_SUCCESS;
						break;
					}
				}
			} else if (!ts->probe_done) {
				break;
			}
		}
		sec_delay(20);
		retry++;
	}


	if (retry > SEC_TS_WAIT_RETRY_CNT)
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);

	input_info(true, &ts->client->dev,
			"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X [%d]\n",
			__func__, buf[0], buf[1], buf[2], buf[3],
			buf[4], buf[5], buf[6], buf[7], retry);
out:
	mutex_unlock(&ts->fn_mutex);

	return rc;
}

/*
 *	set scan mode by epen driver
 */
int slsi_ts_set_scan_mode(struct slsi_ts_data *ts, int mode)
{
	u8 w_data;
	int ret;

	if (ts->plat_data->shutdown_called)
		return 0;

	if (ts->plat_data->wirelesscharger_mode != TYPE_WIRELESS_CHARGER_NONE && mode) {
		input_info(true, &ts->client->dev, "%s: Skip change mode on wireless charger(%d)\n",
				__func__, mode);

		return mode;
	}

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev,
				"%s: fail to send status(%d), POWER_STATUS=OFF\n",
				__func__, mode);
		return mode;
	}

	w_data = mode;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_SCAN_MODE, &w_data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to send command 75", __func__);
		return -EINVAL;
	}

	input_info(true, &ts->client->dev, "%s: mode(%d) sended \n",
			__func__, mode);
	return mode;
}

int slsi_ts_set_touch_function(struct slsi_ts_data *ts)
{
	int ret = 0;

	ret = slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_TOUCHFUNCTION, (u8 *)&(ts->plat_data->touch_functions), 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
				__func__, SLSI_TS_CMD_SET_TOUCHFUNCTION);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_read_functions, msecs_to_jiffies(30));

	return ret;
}

void slsi_ts_get_touch_function(struct work_struct *work)
{
	struct slsi_ts_data *ts = container_of(work, struct slsi_ts_data,
			work_read_functions.work);
	int ret = 0;

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_TOUCHFUNCTION, (u8 *)&(ts->plat_data->ic_status), 2);
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

int slsi_ts_fix_tmode(struct slsi_ts_data *ts, u8 mode, u8 state)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_OFF};
	u8 tBuff[2] = { mode, state };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_STATEMANAGE_ON, onoff, 1);
	sec_delay(20);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CHG_SYSMODE, tBuff, sizeof(tBuff));
	sec_delay(20);

	return ret;
}

int slsi_ts_release_tmode(struct slsi_ts_data *ts)
{
	int ret;
	u8 onoff[1] = {STATE_MANAGE_ON};

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_STATEMANAGE_ON, onoff, 1);
	sec_delay(20);

	return ret;
}

int slsi_ts_set_lowpowermode(void *data, u8 mode)
{
	struct slsi_ts_data *ts = (struct slsi_ts_data *)data;
	int ret;
	int retrycnt = 0;
	char para = 0;

	input_err(true, &ts->client->dev, "%s: %s(%X)\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", ts->plat_data->lowpower_mode);

	if (mode) {
		slsi_ts_set_utc_sponge(ts);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->sponge_dump_delayed_flag) {
				slsi_ts_sponge_dump_flush(ts, ts->sponge_dump_delayed_area);
				ts->sponge_dump_delayed_flag = false;
				input_info(true, &ts->client->dev, "%s : Sponge dump flush delayed work have procceed\n", __func__);
			}
		}
#endif
		ret = slsi_ts_set_custom_library(ts);
		if (ret < 0)
			goto i2c_error;
	} else {
		if (!ts->plat_data->shutdown_called)
			schedule_work(&ts->work_read_functions.work);
	}

retry_pmode:
	ret = slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &mode, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed\n", __func__);
		goto i2c_error;
	}

	sec_delay(50);

	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read power mode failed!\n", __func__);
		goto i2c_error;
	} else {
		input_info(true, &ts->client->dev, "%s: power mode - write(%d) read(%d)\n", __func__, mode, para);
	}

	if (mode != para) {
		retrycnt++;
		ts->plat_data->hw_param.mode_change_failed_count++;
		if (retrycnt < 5)
			goto retry_pmode;
	}

	if (mode) {
		ret = slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);
			goto i2c_error;
		}
	}

	slsi_ts_locked_release_all_finger(ts);

	if (device_may_wakeup(&ts->client->dev)) {
		if (mode)
			enable_irq_wake(ts->client->irq);
		else
			disable_irq_wake(ts->client->irq);
	}

	if (mode == TO_LOWPOWER_MODE)
		ts->plat_data->power_state = SEC_INPUT_STATE_LPM;
	else
		ts->plat_data->power_state = SEC_INPUT_STATE_POWER_ON;

i2c_error:
	input_info(true, &ts->client->dev, "%s: end %d\n", __func__, ret);

	return ret;
}

void slsi_ts_unlocked_release_all_finger(struct slsi_ts_data *ts)
{
	sec_input_release_all_finger(&ts->client->dev);
}

void slsi_ts_locked_release_all_finger(struct slsi_ts_data *ts)
{
	mutex_lock(&ts->eventlock);

	slsi_ts_unlocked_release_all_finger(ts);

	mutex_unlock(&ts->eventlock);
}

void slsi_ts_reset_work(struct work_struct *work)
{
	struct slsi_ts_data *ts = container_of(work, struct slsi_ts_data,
			reset_work.work);
	int ret;
	char result[32];
	char test[32];

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: secure touch enabled\n", __func__);
		return;
	}
#endif
	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: reset is ongoing\n", __func__);
		return;
	}

	mutex_lock(&ts->modechange);
	__pm_stay_awake(ts->plat_data->sec_ws);

	ts->reset_is_on_going = true;
	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->plat_data->stop_device(ts);

	sec_delay(30);

	ret = ts->plat_data->start_device(ts);
	if (ret < 0) {
		/* for ACT i2c recovery fail test */
		snprintf(test, sizeof(test), "TEST=RECOVERY");
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(&ts->sec, test, result);

		input_err(true, &ts->client->dev, "%s: failed to reset, ret:%d\n", __func__, ret);
		ts->reset_is_on_going = false;
		cancel_delayed_work(&ts->reset_work);
		if (!ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
		mutex_unlock(&ts->modechange);

		if (ts->debug_flag & SEC_TS_DEBUG_SEND_UEVENT) {
			snprintf(result, sizeof(result), "RESULT=RESET");
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}

		__pm_relax(ts->plat_data->sec_ws);

		return;
	}

	if (!ts->plat_data->enabled) {
		input_err(true, &ts->client->dev, "%s: call input_close\n", __func__);

		if (ts->plat_data->lowpower_mode || ts->plat_data->ed_enable) {
			ret = ts->plat_data->lpmode(ts, TO_LOWPOWER_MODE);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: failed to reset, ret:%d\n", __func__, ret);
				ts->reset_is_on_going = false;
				cancel_delayed_work(&ts->reset_work);
				if (!ts->plat_data->shutdown_called)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
				mutex_unlock(&ts->modechange);
				__pm_relax(ts->plat_data->sec_ws);
				return;
			}
			if (ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_AOD)
				slsi_ts_set_aod_rect(ts);
		} else {
			ts->plat_data->stop_device(ts);
		}
	}

	ts->reset_is_on_going = false;
	mutex_unlock(&ts->modechange);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_ON) {
		if (ts->fix_active_mode)
			slsi_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
	}

	if (ts->debug_flag & SEC_TS_DEBUG_SEND_UEVENT) {
		char result[32];

		snprintf(result, sizeof(result), "RESULT=RESET");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);
	}

	__pm_relax(ts->plat_data->sec_ws);
}

void slsi_ts_print_info_work(struct work_struct *work)
{
	struct slsi_ts_data *ts = container_of(work, struct slsi_ts_data,
			work_print_info.work);

	if (!ts)
		return;

	if (!ts->client)
		return;

	sec_input_print_info(&ts->client->dev, ts->tdata);

	if (ts->sec.cmd_is_running)
		input_err(true, &ts->client->dev, "%s: skip set temperature, cmd running\n", __func__);
	else
		sec_input_set_temperature(&ts->client->dev, SEC_INPUT_SET_TEMPERATURE_NORMAL);

	if (!ts->plat_data->shutdown_called)
		schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

void slsi_ts_read_info_work(struct work_struct *work)
{
	struct slsi_ts_data *ts = container_of(work, struct slsi_ts_data,
			work_read_info.work);
#ifdef TCLM_CONCEPT
	int ret;

	disable_irq(ts->client->irq);

	ret = sec_tclm_check_cal_case(ts->tdata);
	input_info(true, &ts->client->dev, "%s: sec_tclm_check_cal_case ret: %d \n", __func__, ret);

	enable_irq(ts->client->irq);
#endif
	ts->nv = get_tsp_nvm_data(ts, SLSI_TS_NVM_OFFSET_FAC_RESULT);
	input_info(true, &ts->client->dev, "%s: fac_nv:%02X\n", __func__, ts->nv);
	input_log_fix();

	slsi_ts_run_rawdata_all(ts, false);

	/* read cmoffset & fail history data at booting time */
	if (ts->proc_cmoffset_size) {
		get_cmoffset_dump_all(ts, ts->cmoffset_sdc_proc, OFFSET_FW_SDC);
		get_cmoffset_dump_all(ts, ts->cmoffset_main_proc, OFFSET_FW_MAIN);
		get_miscal_dump(ts, ts->miscal_proc);
	} else {
		input_err(true, &ts->client->dev, "%s: read cmoffset fail : alloc fail\n", __func__);
	}

	if (ts->proc_fail_hist_size) {
		get_selftest_fail_hist_dump_all(ts, ts->fail_hist_sdc_proc, OFFSET_FW_SDC);
		get_selftest_fail_hist_dump_all(ts, ts->fail_hist_sub_proc, OFFSET_FW_SUB);
		get_selftest_fail_hist_dump_all(ts, ts->fail_hist_main_proc, OFFSET_FW_MAIN);
	} else {
		input_err(true, &ts->client->dev, "%s: read fail-history fail : alloc fail\n", __func__);
	}

	ts->info_work_done = true;

	if (ts->plat_data->shutdown_called) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
		return;
	} else {
		schedule_work(&ts->work_print_info.work);
	}
}

int slsi_ts_set_cover_type(struct slsi_ts_data *ts, bool enable)
{
	int ret;
	u8 cover_cmd;

	input_info(true, &ts->client->dev, "%s: %s, type:%d\n",
			__func__, enable ? "close" : "open", ts->plat_data->cover_type);

	cover_cmd = sec_input_check_cover_type(&ts->client->dev) & 0xFF;

	if (enable)
		ts->plat_data->touch_functions |= SLSI_TS_BIT_SETFUNC_COVER;
	else
		ts->plat_data->touch_functions &= ~SLSI_TS_BIT_SETFUNC_COVER;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: pwr off, close:%d, touch_fn:%x\n", __func__,
				enable, ts->plat_data->touch_functions);
		return -ENODEV;
	}

	if (enable) {
		ret = slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_COVERTYPE, &cover_cmd, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to send covertype command: %d, ret:%d/n",
					__func__, cover_cmd, ret);
			return ret;
		}
	}

	ret = slsi_ts_set_touch_function(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to set touch function, ret:%d/n", __func__, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(slsi_ts_set_cover_type);

int slsi_ts_set_temperature(struct device *dev, u8 temperature_data)
{
	struct slsi_ts_data *ts = dev_get_drvdata(dev);

	return ts->slsi_ts_i2c_write(ts, SET_TS_CMD_SET_LOWTEMPERATURE_MODE, &temperature_data, 1);
}

int slsi_ts_set_aod_rect(struct slsi_ts_data *ts)
{
	u8 data[10] = {0x02, 0};
	int ret, i;

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = ts->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 3] = (ts->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->slsi_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
		if (ts->plat_data->aod_data.rect_data[0] == 0 && ts->plat_data->aod_data.rect_data[1] == 0 &&
			ts->plat_data->aod_data.rect_data[2] == 0 && ts->plat_data->aod_data.rect_data[3] == 0)
			data[0] = SLSI_TS_CMD_LPM_AOD_OFF;
		else
			data[0] = SLSI_TS_CMD_LPM_AOD_ON;

		ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_LPM_AOD_OFF_ON, &data[0], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to send aod off_on cmd\n", __func__);
	}

	return ret;
}

int slsi_ts_set_press_property(struct slsi_ts_data *ts)
{
	u8 data[3] = { SEC_TS_CMD_SPONGE_PRESS_PROPERTY, 0 };
	int ret;

	if (!ts->plat_data->support_fod)
		return 0;

	data[2] = ts->plat_data->fod_data.press_prop;

	ret = ts->slsi_ts_write_sponge(ts, data, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->plat_data->fod_data.press_prop);

	return ret;
}

int slsi_ts_set_fod_rect(struct slsi_ts_data *ts)
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

	ret = ts->slsi_ts_write_sponge(ts, data, 10);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to write sponge\n", __func__);

	return ret;
}

int slsi_ts_set_charger_mode(struct slsi_ts_data *ts)
{
	int ret;
	u8 buff;

	if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER_NONE) {
		buff = SLSI_TS_BIT_CHARGER_MODE_NO;
	} else if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER) {
		buff = SLSI_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER;
	} else if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_BATTERY_PACK) {
		buff = SLSI_TS_BIT_CHARGER_MODE_WIRELESS_BATTERY_PACK;
	} else {
		input_err(true, &ts->client->dev, "%s: not supported mode %d\n",
				__func__, ts->plat_data->wirelesscharger_mode);
		return SEC_ERROR;
	}

	ret = ts->slsi_ts_i2c_write(ts, SET_TS_CMD_SET_CHARGER_MODE, &buff, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: Failed to write mode 0x%02X (cmd:%d), ret=%d\n",
				__func__, buff, ts->plat_data->wirelesscharger_mode, ret);
	else
		input_info(true, &ts->client->dev, "%s: %sabled, mode=%d\n", __func__,
				ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER_NONE ? "dis" : "en",
				ts->plat_data->wirelesscharger_mode);

	return ret;
}

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

void slsi_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct slsi_ts_data *ts = dev_get_drvdata(dev);

	u8 data[8] = { 0 };

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
		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_EDGE_HANDLER, data, 4);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X\n",
				__func__, SLSI_TS_CMD_EDGE_HANDLER, data[0], data[1], data[2], data[3]);
	}

	if (flag & G_SET_EDGE_ZONE) {
		data[0] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		data[1] = ts->plat_data->grip_data.edge_range  & 0xFF;
		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_EDGE_AREA, data, 2);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X\n",
				__func__, SLSI_TS_CMD_EDGE_AREA, data[0], data[1]);
	}

	if (flag & G_SET_NORMAL_MODE) {
		data[0] = ts->plat_data->grip_data.deadzone_up_x & 0xFF;
		data[1] = ts->plat_data->grip_data.deadzone_dn_x & 0xFF;
		data[2] = (ts->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		data[3] = ts->plat_data->grip_data.deadzone_y & 0xFF;
		data[4] = ts->plat_data->grip_data.deadzone_dn2_x & 0xFF;
		data[5] = (ts->plat_data->grip_data.deadzone_dn_y >> 8) & 0xFF;
		data[6] = ts->plat_data->grip_data.deadzone_dn_y & 0xFF;
		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_DEAD_ZONE, data, 7);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
				__func__, SLSI_TS_CMD_DEAD_ZONE, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		data[0] = ts->plat_data->grip_data.landscape_mode & 0x1;
		data[1] = (ts->plat_data->grip_data.landscape_edge >> 4) & 0xFF;
		data[2] = (ts->plat_data->grip_data.landscape_edge << 4 & 0xF0)
				| ((ts->plat_data->grip_data.landscape_deadzone >> 8) & 0xF);
		data[3] = ts->plat_data->grip_data.landscape_deadzone & 0xFF;
		data[4] = ts->plat_data->grip_data.landscape_top_deadzone & 0xFF;
		data[5] = ts->plat_data->grip_data.landscape_bottom_deadzone & 0xFF;
		data[6] = ts->plat_data->grip_data.landscape_top_gripzone & 0xFF;
		data[7] = ts->plat_data->grip_data.landscape_bottom_gripzone & 0xFF;
		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_LANDSCAPE_MODE, data, 8);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X,%02X,%02X,%02X, %02X,%02X,%02X,%02X\n",
				__func__, SLSI_TS_CMD_LANDSCAPE_MODE, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		data[0] = ts->plat_data->grip_data.landscape_mode;
		ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_LANDSCAPE_MODE, data, 1);
		input_info(true, &ts->client->dev, "%s: 0x%02X %02X\n",
				__func__, SLSI_TS_CMD_LANDSCAPE_MODE, data[0]);
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
 * noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = SLSI_TS_CMD_SET_MONITOR_NOISE_MODE;
 */
int slsi_ts_set_external_noise_mode(struct slsi_ts_data *ts, u8 mode)
{
	int i, ret, fail_count = 0;
	u8 mode_bit_to_set, check_bit, mode_enable;
	u8 noise_mode_cmd[EXT_NOISE_MODE_MAX] = { 0 };

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
	noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = SLSI_TS_CMD_SET_MONITOR_NOISE_MODE;

	/* write mode */
	for (i = EXT_NOISE_MODE_NONE + 1; i < EXT_NOISE_MODE_MAX; i++) {
		check_bit = 1 << i;
		if (mode_bit_to_set & check_bit) {
			mode_enable = !!(ts->plat_data->external_noise_mode & check_bit);
			ret = ts->slsi_ts_i2c_write(ts, noise_mode_cmd[i], &mode_enable, 1);
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

int slsi_ts_set_touchable_area(struct slsi_ts_data *ts)
{
	int ret;

	input_info(true, &ts->client->dev,
			"%s: set 16:9 mode %s\n", __func__,
			ts->plat_data->touchable_area ? "enable" : "disable");

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_TOUCHABLE_AREA, &ts->plat_data->touchable_area, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set 16:9 mode, ret=%d\n", __func__, ret);
	return ret;
}

int slsi_ts_ear_detect_enable(struct slsi_ts_data *ts, u8 enable)
{
	int ret;
	u8 buff = enable;

	input_info(true, &ts->client->dev, "%s: set ear detect mode %d\n", __func__, enable);

	/* 00: off, 01:Mutual, 10:Self, 11: Mutual+Self */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_SET_EAR_DETECT_MODE, &buff, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set ed_enable %d, ret=%d\n", __func__, buff, ret);
	return ret;
}

int slsi_ts_p2p_tmode(struct slsi_ts_data *ts)
{
	int ret;
	u8 mode[3] = {0x2F, 0x00, 0xDE};
	char para = TO_SELFTEST_MODE;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, &para, 1);
	sec_delay(30);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_P2P_MODE, mode, sizeof(mode));
	sec_delay(30);

	return ret;
}

int execute_p2ptest(struct slsi_ts_data *ts)
{
	int ret;
	u8 test[2] = {0x00, 0x32};
	u8 tBuff[10] = {0};
	int retry = 0;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_P2P_TEST, test, sizeof(test));
	sec_delay(600);

	ret = -1;

	while (ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_EVENT, tBuff, 8)) {
		if (((tBuff[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_VENDOR_INFO) {
			if (tBuff[1] == SLSI_TS_VENDOR_ACK_CMR_TEST_DONE) {
				ts->cm_raw_set_avg_max = (tBuff[2] & 0xFF) << 8 | (tBuff[3] & 0xFF);
				ts->cm_raw_set_avg_min = (tBuff[4] & 0xFF) << 8 | (tBuff[5] & 0xFF);
				ts->cm_raw_set_p2p = (tBuff[7] & 0xC0) << 2 | (tBuff[6] & 0xFF);
			} else if (tBuff[1] == SLSI_TS_VENDOR_ACK_CSR_TX_TEST_DONE) {
				ts->self_raw_set_avg_tx_max = (tBuff[2] & 0xFF) << 8 | (tBuff[3] & 0xFF);
				ts->self_raw_set_avg_tx_min = (tBuff[4] & 0xFF) << 8 | (tBuff[5] & 0xFF);
				ts->self_raw_set_p2p_tx_diff = (tBuff[7] & 0xC0) << 2 | (tBuff[6] & 0xFF);
			} else if (tBuff[1] == SLSI_TS_VENDOR_ACK_CSR_RX_TEST_DONE) {
				ts->self_raw_set_avg_rx_max = (tBuff[2] & 0xFF) << 8 | (tBuff[3] & 0xFF);
				ts->self_raw_set_avg_rx_min = (tBuff[4] & 0xFF) << 8 | (tBuff[5] & 0xFF);
				ts->self_raw_set_p2p_rx_diff = (tBuff[7] & 0xC0) << 2 | (tBuff[6] & 0xFF);
			} else if (tBuff[1] == SLSI_TS_VENDOR_ACK_CMR_KEY_TEST_DONE) {
				ts->cm_raw_key_p2p_max = (tBuff[2] & 0xFF) << 8 | (tBuff[3] & 0xFF);
				ts->cm_raw_key_p2p_min = (tBuff[4] & 0xFF) << 8 | (tBuff[5] & 0xFF);
				ts->cm_raw_key_p2p_diff = (tBuff[7] & 0xC0) << 2 | (tBuff[6] & 0xFF);
			} else if (tBuff[1] == SLSI_TS_VENDOR_ACK_RX_NODE_GAP_TEST_DONE) {
				ts->cm_raw_set_p2p_gap_y = (tBuff[2] & 0xFF) << 8 | (tBuff[3] & 0xFF);
				ts->cm_raw_set_p2p_gap_y_result = (tBuff[4] & 0x01);
				ts->gap_max_spec = (tBuff[5] & 0xFF) << 8 | (tBuff[6] & 0xFF);
			}
		}

		if ((tBuff[7] & 0x3F) == 0x00) {
			input_info(true, &ts->client->dev, "%s: left event is 0\n", __func__);
			ret = 0;
			break;
		}

		if (retry++ > SEC_TS_WAIT_RETRY_CNT) {
			input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
			break;
		}
		sec_delay(20);
	}
	return ret;
}

/* Use TSP NV area
 * buff[0] : offset from user NVM storage
 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
 * buff[2] : write data
 * buff[..] : cont.
 */
void set_tsp_nvm_data_clear(struct slsi_ts_data *ts, u8 offset)
{
	char buff[4] = { 0 };
	int ret;

	input_dbg(true, &ts->client->dev, "%s\n", __func__);

	buff[0] = offset;

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: nvm write failed. ret: %d\n", __func__, ret);

	sec_delay(20);
}

int get_tsp_nvm_data(struct slsi_ts_data *ts, u8 offset)
{
	char buff[2] = { 0 };
	int ret;

	/* SENSE OFF -> CELAR EVENT STACK -> READ NV -> SENSE ON */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_OFF, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to write Sense_off\n", __func__);
		goto out_nvm;
	}

	input_dbg(false, &ts->client->dev, "%s: SENSE OFF\n", __func__);

	sec_delay(100);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);
		goto out_nvm;
	}

	input_dbg(false, &ts->client->dev, "%s: CLEAR EVENT STACK\n", __func__);

	sec_delay(100);

	slsi_ts_locked_release_all_finger(ts);

	/* send NV data using command
	 * Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 */
	memset(buff, 0x00, 2);
	buff[0] = offset;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 2);
	if (ret < 0) {
		buff[0] = 0;
		input_err(true, &ts->client->dev, "%s: nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	sec_delay(20);

	/* read NV data
	 * Use TSP NV area : in this model, use only one byte
	 */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_NVM, buff, 1);
	if (ret < 0) {
		buff[0] = 0;
		input_err(true, &ts->client->dev, "%s: nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	input_info(true, &ts->client->dev, "%s: offset:%u  data:%02X\n", __func__, offset, buff[0]);

out_nvm:
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);

	sec_delay(300);

	input_dbg(false, &ts->client->dev, "%s: SENSE ON\n", __func__);

	return buff[0];
}

int get_tsp_nvm_data_by_size(struct slsi_ts_data *ts, u8 offset, int length, u8 *data)
{
	char *buff = NULL;
	int ret;

	buff = kzalloc(length, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	input_info(true, &ts->client->dev, "%s: offset:%u, length:%d, size:%zu\n", __func__, offset, length, sizeof(data));

	/* SENSE OFF -> CELAR EVENT STACK -> READ NV -> SENSE ON */
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_OFF, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to write Sense_off\n", __func__);
		goto out_nvm;
	}

	input_dbg(true, &ts->client->dev, "%s: SENSE OFF\n", __func__);

	sec_delay(100);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);
		goto out_nvm;
	}

	input_dbg(true, &ts->client->dev, "%s: CLEAR EVENT STACK\n", __func__);

	sec_delay(100);

	slsi_ts_locked_release_all_finger(ts);

	/* send NV data using command
	 * Use TSP NV area : in this model, use only one byte
	 * buff[0] : offset from user NVM storage
	 * buff[1] : length of stroed data - 1 (ex. using 1byte, value is  1 - 1 = 0)
	 */
	memset(buff, 0x00, 2);
	buff[0] = offset;
	buff[1] = length - 1;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	sec_delay(20);

	/* read NV data
	 * Use TSP NV area : in this model, use only one byte
	 */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_CMD_NVM, buff, length);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: nvm send command failed. ret: %d\n", __func__, ret);
		goto out_nvm;
	}

	memcpy(data, buff, length);

out_nvm:
	if (ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SENSE_ON, NULL, 0) < 0) {
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);
		ret = SEC_ERROR;
	}

	sec_delay(300);

	input_dbg(true, &ts->client->dev, "%s: SENSE ON\n", __func__);

	kfree(buff);

	return ret;
}

int set_tsp_nvm_data_by_size(struct slsi_ts_data *ts, u8 reg, int size, u8 *data)
{
	int rc;
	u8 buff[SEC_CMD_STR_LEN] = {0};

	buff[0] = reg;
	buff[1] = size - 1;	/* 1bytes */
	memcpy(&buff[2], data, size);
	rc = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_NVM, buff, size + 2);
	if (rc < 0) {
		input_err(true, &ts->client->dev,
			"%s: nvm write failed. ret: %d\n", __func__, rc);
	}
	sec_delay(20);
	return rc;
}

int sec_tclm_data_read(struct i2c_client *client, int address)
{
	struct slsi_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;
	u8 buff[4];
	u8 nbuff[SLSI_TS_NVM_OFFSET_LENGTH - SLSI_TS_NVM_OFFSET_CAL_COUNT];

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		sec_delay(100);
		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_READ_IMG_VERSION, buff, 4);
		if (ret < 0)
			return ret;
		input_err(true, &ts->client->dev, "%s SLSI_TS_READ_IMG_VERSION buff[2]:%02X buff[3]:%02X \n",
			__func__, buff[2], buff[3]);
		ret = (buff[2] << 8) | buff[3];
		return ret;
	case SEC_TCLM_NVM_ALL_DATA:
		memset(&ts->tdata->nvdata, 0x00, sizeof(struct sec_tclm_nvdata));
		
		ret = get_tsp_nvm_data_by_size(ts, SLSI_TS_NVM_OFFSET_CAL_COUNT, sizeof(struct sec_tclm_nvdata), nbuff);
		if (ret < 0)
			return ret;

		memcpy(&ts->tdata->nvdata, nbuff, sizeof(struct sec_tclm_nvdata));
		return ret;
	case SEC_TCLM_NVM_TEST:
		input_info(true, &ts->client->dev, "%s: dt: tclm_level [%d] afe_base [%04X]\n",
			__func__, ts->tdata->tclm_level, ts->tdata->afe_base);
		ret = get_tsp_nvm_data_by_size(ts, SLSI_TS_NVM_TOTAL_OFFSET_LENGTH + SEC_TCLM_NVM_OFFSET,
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

int sec_tclm_data_write(struct i2c_client *client, int address)
{
	struct slsi_ts_data *ts = i2c_get_clientdata(client);
	int ret = 1;
	u8 nbuff[SLSI_TS_NVM_OFFSET_LENGTH - SLSI_TS_NVM_OFFSET_CAL_COUNT];

	memset(nbuff, 0x00, sizeof(struct sec_tclm_nvdata));
	switch (address) {
	case SEC_TCLM_NVM_ALL_DATA:
		memcpy(nbuff, &ts->tdata->nvdata, sizeof(struct sec_tclm_nvdata));
		ret = set_tsp_nvm_data_by_size(ts, SLSI_TS_NVM_OFFSET_CAL_COUNT, sizeof(struct sec_tclm_nvdata), nbuff);
		return ret;
	case SEC_TCLM_NVM_TEST:
		ret = set_tsp_nvm_data_by_size(ts, SLSI_TS_NVM_TOTAL_OFFSET_LENGTH + SEC_TCLM_NVM_OFFSET,
			SEC_TCLM_NVM_OFFSET_LENGTH, ts->tdata->tclm);
		return ret;
	default:
		return ret;
	}
}

MODULE_LICENSE("GPL");
