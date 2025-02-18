/* drivers/input/sec_input/slsi/slsi_dump.c
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

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
void slsi_ts_check_rawdata(struct work_struct *work)
{
	struct slsi_ts_data *ts = container_of(work, struct slsi_ts_data, check_rawdata.work);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s: ignored ## already checking..\n", __func__);
		return;
	}
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: ignored ## IC is power off\n", __func__);
		return;
	}

	slsi_ts_run_rawdata_all(ts, true);
}

void slsi_ts_dump_tsp_log(struct device *dev)
{
	struct slsi_ts_data *ts = dev_get_drvdata(dev);

	pr_info("%s: %s %s: start\n", SLSI_TS_I2C_NAME, SECLOG, __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (lpcharge == 1) {
		pr_err("%s: %s %s: ignored ## lpm charging Mode!!\n", SLSI_TS_I2C_NAME, SECLOG, __func__);
		return;
	}
#endif
	if (!ts) {
		pr_err("%s: %s %s: ignored ## tsp probe fail!!\n", SLSI_TS_I2C_NAME, SECLOG, __func__);
		return;
	}

	schedule_delayed_work(&ts->check_rawdata, msecs_to_jiffies(100));
}

void slsi_ts_sponge_dump_flush(struct slsi_ts_data *ts, int dump_area)
{
	int i, ret;
	unsigned char *sec_spg_dat;
	input_info(true, &ts->client->dev, "%s: ++\n", __func__);

	sec_spg_dat = vmalloc(SEC_TS_MAX_SPONGE_DUMP_BUFFER);
	if (!sec_spg_dat) {
		input_err(true, &ts->client->dev, "%s : Failed!!\n", __func__);
		return;
	}
	memset(sec_spg_dat, 0, SEC_TS_MAX_SPONGE_DUMP_BUFFER);

	input_info(true, &ts->client->dev, "%s: dump area=%d\n", __func__, dump_area);

	/* check dump area */
	if (dump_area == 0) {
		sec_spg_dat[0] = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
		sec_spg_dat[1] = 0;
	} else {
		sec_spg_dat[0] = ts->sponge_dump_border_lsb;
		sec_spg_dat[1] = ts->sponge_dump_border_msb;
	}
	
	/* dump all events at once */
	ret = ts->slsi_ts_read_sponge(ts, sec_spg_dat, ts->sponge_dump_event * ts->sponge_dump_format);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read sponge\n", __func__);
		vfree(sec_spg_dat);
		return;
	}

	for (i = 0 ; i < ts->sponge_dump_event ; i++) {
		int e_offset = i * ts->sponge_dump_format;
		char buff[30] = {0, };
		u16 edata[5];
		edata[0] = (sec_spg_dat[1 + e_offset] & 0xFF) << 8 | (sec_spg_dat[0 + e_offset] & 0xFF);
		edata[1] = (sec_spg_dat[3 + e_offset] & 0xFF) << 8 | (sec_spg_dat[2 + e_offset] & 0xFF);
		edata[2] = (sec_spg_dat[5 + e_offset] & 0xFF) << 8 | (sec_spg_dat[4 + e_offset] & 0xFF);
		edata[3] = (sec_spg_dat[7 + e_offset] & 0xFF) << 8 | (sec_spg_dat[6 + e_offset] & 0xFF);
		edata[4] = (sec_spg_dat[9 + e_offset] & 0xFF) << 8 | (sec_spg_dat[8 + e_offset] & 0xFF);

		if (edata[0] || edata[1] || edata[2] || edata[3] || edata[4]) {
			snprintf(buff, sizeof(buff), "%03d: %04x%04x%04x%04x%04x\n",
					i + (ts->sponge_dump_event * dump_area), 
					edata[0], edata[1], edata[2], edata[3], edata[4]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
			sec_tsp_sponge_log(buff);
#endif
		}
	}

	vfree(sec_spg_dat);
	input_info(true, &ts->client->dev, "%s: --\n", __func__);
	return;
}
EXPORT_SYMBOL(slsi_ts_sponge_dump_flush);
#endif

static int slsi_ts_set_factory_data_type(struct slsi_ts_data *ts, u8 pos)
{
	int ret = 0;

	input_info(true, &ts->client->dev,
			"%s: set factory data type[%d]\n", __func__, pos);

	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_SET_FACTORY_DATA_TYPE, &pos, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed to set factory data type[%d]\n", __func__, pos);

	sec_delay(30);
	return ret;
}

/* SELFTEST FAIL HISTORY : (cm1_1 + cm1_2 + cm2_1 + cm2_2 + cm3_1 + cm3_2) * 48 byte */
ssize_t get_selftest_fail_hist_dump_all(struct slsi_ts_data *ts, char *buf, u8 position)
{
	int i, j;
	int ii, jj;
	int ret;
	char tempn[40] = {0};
	char tempv[25] = {0};
	char *buff;
	u8 read_event_buff[6][48] = { { 0, } };
	u8 *rBuff;
	u8 temp_result = 0;
	u8 defect_tx, defect_rx;
	u16 defective_node_data;
	struct slsi_ts_selftest_fail_hist *p_fail_hist;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return -EBUSY;
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Reset is ongoing\n", __func__);
		return -EBUSY;
	}

	if (ts->sec.cmd_is_running) {
		input_err(true, &ts->client->dev, "%s: [ERROR] cmd is running\n", __func__);
		return -EBUSY;
	}

	/* set Factory level */
	ret = slsi_ts_write_factory_level(ts, position);
	if (ret < 0)
		goto err_exit;

	/* set Factory Data Type */
	ret = slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_SELF_FAIL);
	if (ret < 0)
		goto err_exit;

	rBuff = kzalloc(SEC_CM_HIST_DATA_SIZE, GFP_KERNEL);
	if (!rBuff)
		goto err_mem;

	buff = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buff) {
		kfree(rBuff);
		goto err_mem;
	}

	/* read full data */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_GET_FACTORY_DATA, (u8 *)read_event_buff[0], SEC_CM_HIST_DATA_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read fail history data failed!\n", __func__);
		goto err_i2c;
	}
	input_info(true, &ts->client->dev, "%s: read data read_data_size[%d]  ret[%d]\n",
								__func__, SEC_CM_HIST_DATA_SIZE, ret);

	memset(buf, 0x00, ts->proc_fail_hist_size);

	// CM 1 & 2 & 3 each 2 times
	for (i = 1 ; i < 4 ; ++i ) {
		for (j = 1 ; j < 3 ; ++j) {
			p_fail_hist = (struct slsi_ts_selftest_fail_hist *)read_event_buff[(i - 1) * 2 + (j - 1)];

			if (position == OFFSET_FW_SDC) {
				snprintf(buff, PAGE_SIZE, "%s", "SDC  ");
			} else if (position == OFFSET_FW_SUB) {
				snprintf(buff, PAGE_SIZE, "%s", "SUB  ");
			} else if (position == OFFSET_FW_MAIN) {
				snprintf(buff, PAGE_SIZE, "%s", "MAIN ");
			}
			strlcat(buf, buff, ts->proc_fail_hist_size);

			input_info(true, &ts->client->dev, "%s: CM%d #%d\n", __func__, i, j);

			if (p_fail_hist->tsp_signature == 0 || p_fail_hist->tsp_signature == 0xFFFFFFFF) {
				input_err(true, &ts->client->dev, "%s: CM%d #%d : Data empty\n", __func__, i, j);
				snprintf(buff, PAGE_SIZE, "CM%d #%d : Data empty(0x%X)\n", i, j, p_fail_hist->tsp_signature);
				strlcat(buf, buff, ts->proc_fail_hist_size);
				continue;

			} else if (p_fail_hist->tsp_signature != SEC_FAIL_HIST_SIGNATURE) {
				input_err(true, &ts->client->dev, "%s: signature is mismatched :%8X != (%8X)\n",
							__func__, p_fail_hist->tsp_signature, SEC_FAIL_HIST_SIGNATURE);
				snprintf(buff, PAGE_SIZE, "CM%d #%d : SIGNATURE mismatched(0x%X)\n", i, j, p_fail_hist->tsp_signature);
				strlcat(buf, buff, ts->proc_fail_hist_size);
				continue;
			}

			input_info(true, &ts->client->dev, "%s: PRINT CM%d #%d INFO\n", __func__, i, j);
			input_info(true, &ts->client->dev, "%s: FW VER : 0x%X, Selftest Parm : 0x%X, fail_cnt1/2 : %d/%d, Test Result : 0x%X\n",
								__func__, p_fail_hist->tsp_fw_version, p_fail_hist->selftest_exec_parm,
								p_fail_hist->fail_cnt1, p_fail_hist->fail_cnt2, p_fail_hist->test_result);

			snprintf(buff, PAGE_SIZE, "CM%d #%d :  FW VER : 0x%X, Selftest Parm : 0x%X, fail_cnt1/2 : %d/%d, Test Result : 0x%X\n",
							i, j, p_fail_hist->tsp_fw_version, p_fail_hist->selftest_exec_parm,
							p_fail_hist->fail_cnt1, p_fail_hist->fail_cnt2, p_fail_hist->test_result);
			strlcat(buf, buff, ts->proc_fail_hist_size);
			
			input_info(true, &ts->client->dev, "%s: Primary Failure Type : 0x%X\n", __func__, p_fail_hist->fail_type);
			snprintf(buff, PAGE_SIZE, "Fail Type : 0x%X\n", p_fail_hist->fail_type);
			strlcat(buf, buff, ts->proc_fail_hist_size);

			memset(buff, 0x00, PAGE_SIZE);
			memset(tempn, 0x00, 40);

			if (p_fail_hist->fail_type == 0x01)
				snprintf(tempn, 40, "S2S Short Fail (Tx/Rx to Tx/Rx):");
			else if (p_fail_hist->fail_type == 0x02)
				snprintf(tempn, 40, "S2G Short Fail (Tx/Rx to GND):");
			else if (p_fail_hist->fail_type == 0x03)
				snprintf(tempn, 40, "S2O Short Fail (Tx/Rx to Rx/Tx):");
			else if (p_fail_hist->fail_type == 0x04)
				snprintf(tempn, 40, "Open Fail:");
			else if (p_fail_hist->fail_type == 0x05)
				snprintf(tempn, 40, "Slope Fail:");
			else if (p_fail_hist->fail_type == 0x06)
				snprintf(tempn, 40, "CM2 Fail:");
			else if (p_fail_hist->fail_type == 0x07)
				snprintf(tempn, 40, "CM3 Fail:");
			else if (p_fail_hist->fail_type == 0x16)
				snprintf(tempn, 40, "S2S Short in Boundary Range:");

			strlcat(buff, tempn, PAGE_SIZE);

			for (ii = 0; ii < 8; ii++) {
				temp_result = p_fail_hist->fail_data[ii];
				for (jj = ii * 8; jj < (ii + 1) * 8; jj++) {
					memset(tempv, 0x00, 25);
					if (temp_result & 0x1)
						snprintf(tempv, 20, "%s%d,",
								jj < ts->tx_count ? "TX":"RX",
								jj < ts->tx_count ? jj : jj - ts->tx_count);
					strlcat(buff, tempv, PAGE_SIZE);
					temp_result = temp_result >> 1;
				}
			}
			strlcat(buff, "\n", PAGE_SIZE);
			input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

			strlcat(buf, buff, ts->proc_fail_hist_size);

			//factory failure data
			for (ii = 0 ; ii < 5 ; ii++) {
				defect_tx = p_fail_hist->defective_data[ii] & 0xFF;
				defect_rx = (p_fail_hist->defective_data[ii] >> 8 ) & 0xFF;
				defective_node_data = (p_fail_hist->defective_data[ii] >> 16) & 0xFFFF;

				input_info(true, &ts->client->dev, "%s: RX : %d, TX : %d, Data : %d\n", __func__, 
									defect_rx, defect_tx, defective_node_data);

				snprintf(buff, PAGE_SIZE, "RX,TX[%d,%d] %d\n", defect_rx, defect_tx, defective_node_data);
				strlcat(buf, buff, ts->proc_fail_hist_size);
			}
		}
	}

	input_info(true, &ts->client->dev, "%s: total buf size:%zu\n", __func__, strlen(buf));

	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);
	slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_NO);
	kfree(buff);
	kfree(rBuff);
	return 0;

err_i2c:
	kfree(buff);
	kfree(rBuff);
err_mem:
	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);
	slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_NO);
err_exit:
	snprintf(buf, ts->proc_cmoffset_size, "NG, error");
	return 0;
}

ssize_t get_miscal_dump(struct slsi_ts_data *ts, char *buf)
{
	u8 *rBuff;
	int i, j, ret;
	int value;
	u16 gap_max, cal_cnt, status;
	int max_node = 8 + ts->tx_count * ts->rx_count;
	char buff[80] = {0, };
	char data[6] = {0, };

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return -EBUSY;
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Reset is ongoing\n", __func__);
		return -EBUSY;
	}

	if (ts->sec.cmd_is_running) {
		input_err(true, &ts->client->dev, "%s: [ERROR] cmd is running\n", __func__);
		return -EBUSY;
	}

	input_info(true, &ts->client->dev, "%s: set power mode to test mode\n", __func__);
	data[0] = 0x02;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test mode failed\n", __func__);
	}

	input_info(true, &ts->client->dev, "%s: clear event stack\n", __func__);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
	}

	sec_delay(30);

	memset(buf, 0x00, ts->proc_cmoffset_size);
	snprintf(buff, sizeof(buff), "%s", "MISCAL  ");
	strlcat(buf, buff, ts->proc_cmoffset_size);

	/* set Factory Data Type */
	ret = slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_MISCAL);
	if (ret < 0)
		goto err_exit;
	sec_delay(30);

	rBuff = kzalloc(max_node, GFP_KERNEL);
	if (!rBuff)
		goto err_mem;

	/* read full data */
	ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_GET_FACTORY_DATA, rBuff, max_node);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
		goto err_i2c;
	}

	/* check Header */
	value = rBuff[3] << 24 | rBuff[2] << 16 | rBuff[1] << 8 | rBuff[0];
	input_info(true, &ts->client->dev, "%s: signature:%8X (%8X)\n", __func__, value, SEC_MISCAL_SIGNATURE);

	if (value == 0 || value == 0xFFFFFFFF) {
		input_err(true, &ts->client->dev, "%s: miscal type[%d]: Data empty\n", __func__, OFFSET_FAC_DATA_MISCAL);
		snprintf(buff, sizeof(buff), "%s: miscal %d : Data empty\n", __func__, OFFSET_FAC_DATA_MISCAL);
		strlcat(buf, buff, ts->proc_cmoffset_size);
	} else if (value != SEC_MISCAL_SIGNATURE) {
		input_err(true, &ts->client->dev, "%s: msicalsignature is mismatched %08X != %08X\n",
						__func__, value, SEC_MISCAL_SIGNATURE);
		snprintf(buff, sizeof(buff), "miscal : signature is mismatched %08X != %08X\n", value, SEC_MISCAL_SIGNATURE);
		strlcat(buf, buff, ts->proc_cmoffset_size);
	}

	status = rBuff[4];
	cal_cnt = rBuff[5];
	gap_max = rBuff[7] << 8 | rBuff[6];
	input_info(true, &ts->client->dev, "%s: miscal Gap_max:0x%X, Cal_count:%d, Status:%d\n",
									__func__, gap_max, cal_cnt, status);

	snprintf(buff, sizeof(buff), "Gap_max:%d, Cal_count:%d, Status:%d\n", gap_max, cal_cnt, status);
	strlcat(buf, buff, ts->proc_cmoffset_size);

	for (i = 0; i < ts->rx_count; i++) {
		for (j = 0; j < ts->tx_count; j++) {
			snprintf(buff, sizeof(buff), "%4d", rBuff[8 + (j * ts->rx_count) + i]);
			strlcat(buf, buff, ts->proc_cmoffset_size);
		}
		snprintf(buff, sizeof(buff), "\n");
		strlcat(buf, buff, ts->proc_cmoffset_size);
	}
	input_err(true, &ts->client->dev, "%s: total buf size:%zu\n", __func__, strlen(buf));

	input_info(true, &ts->client->dev, "%s: set power mode to normal mode\n", __func__);
	slsi_ts_locked_release_all_finger(ts);
	data[0] = 0x00;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test normal failed\n", __func__);
	}

	kfree(rBuff);
	return 0;

err_i2c:
	kfree(rBuff);
err_mem:
	slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_NO);

	input_info(true, &ts->client->dev, "%s: set power mode to normal mode\n", __func__);
	slsi_ts_locked_release_all_finger(ts);
	data[0] = 0x00;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test normal failed\n", __func__);
	}
err_exit:
	snprintf(buf, ts->proc_cmoffset_size, "NG, error");
	return 0;
}

ssize_t get_cmoffset_dump_all(struct slsi_ts_data *ts, char *buf, u8 position)
{
	u8 *rBuff;
	int i, j, ret, type;
	int value;
	int signature_val = 0;
	u16 avg, try_cnt, status;
	int max_node = 8 + ts->tx_count * ts->rx_count;
	char buff[80] = {0, };
	char data[6] = {0, };
	u16 temp;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return -EBUSY;
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Reset is ongoing\n", __func__);
		return -EBUSY;
	}

	if (ts->sec.cmd_is_running) {
		input_err(true, &ts->client->dev, "%s: [ERROR] cmd is running\n", __func__);
		return -EBUSY;
	}

	input_info(true, &ts->client->dev, "%s: set power mode to test mode\n", __func__);
	data[0] = 0x02;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test mode failed\n", __func__);
	}

	input_info(true, &ts->client->dev, "%s: clear event stack\n", __func__);
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: clear event stack failed\n", __func__);
	}

	/* set Factory level */
	ret = slsi_ts_write_factory_level(ts, position);
	if (ret < 0)
		goto err_exit;
	sec_delay(30);

	memset(buf, 0x00, ts->proc_cmoffset_size);

	type = OFFSET_FAC_DATA_CM;

	for( ; type <= OFFSET_FAC_DATA_CM3; type++) {

		if (position == OFFSET_FW_SDC) {
			snprintf(buff, sizeof(buff), "%s", "SDC  ");
		} else if (position == OFFSET_FW_SUB) {
			snprintf(buff, sizeof(buff), "%s", "SUB  ");
		} else if (position == OFFSET_FW_MAIN) {
			snprintf(buff, sizeof(buff), "%s", "MAIN ");
		}
		strlcat(buf, buff, ts->proc_cmoffset_size);

		/* set Factory Data Type */
		ret = slsi_ts_set_factory_data_type(ts, type);
		if (ret < 0)
			goto err_exit;
		sec_delay(30);

		rBuff = kzalloc(max_node, GFP_KERNEL);
		if (!rBuff)
			goto err_mem;

		/* read full data */
		ret = ts->slsi_ts_i2c_read(ts, SLSI_TS_GET_FACTORY_DATA, rBuff, max_node);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read rawdata failed!\n", __func__);
			goto err_i2c;
		}

		if (type == OFFSET_FAC_DATA_CM) {
			signature_val = SEC_OFFSET_SIGNATURE;
		} else if (type == OFFSET_FAC_DATA_CM2) {
			signature_val = SEC_CM2_SIGNATURE;
		} else if (type == OFFSET_FAC_DATA_CM3) {
			signature_val = SEC_CM3_SIGNATURE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmoffset pos[%d] type[%d], type is abnormal\n",
						__func__, position, type);
			snprintf(buff, sizeof(buff), "CM%d is abnormal\n", type);
			strlcat(buf, buff, ts->proc_cmoffset_size);
			goto err_invalid;
		}

		/* check Header */
		value = rBuff[3] << 24 | rBuff[2] << 16 | rBuff[1] << 8 | rBuff[0];
		input_info(true, &ts->client->dev, "%s: signature:%8X (%8X)\n", __func__, value, signature_val);

		if (value == 0 || value == 0xFFFFFFFF) {
			input_err(true, &ts->client->dev, "%s: cmoffset pos[%d] type[%d]: Data empty\n", __func__, position, type);
			snprintf(buff, sizeof(buff), "CM%d : Data empty\n", type);
			strlcat(buf, buff, ts->proc_cmoffset_size);
			continue;

		} else if (value != signature_val) {
			input_err(true, &ts->client->dev, "%s: cmoffset pos[%d] type[%d], signature is mismatched %08X != %08X\n",
							__func__, position, type, value, signature_val);
			
			snprintf(buff, sizeof(buff), "CM%d : signature is mismatched %08X != %08X\n", type, value, signature_val);
			strlcat(buf, buff, ts->proc_cmoffset_size);
			continue;
		}

		status = rBuff[4];
		try_cnt = rBuff[5];
		avg = rBuff[7] << 8 | rBuff[6];
		input_info(true, &ts->client->dev, "%s: CM%d pos[%d], AVG:0x%X, Try cnt:%d, Status:%d\n",
										__func__, type, position, avg, try_cnt, status);

		snprintf(buff, sizeof(buff), "CM%d pos[%d], AVG:%d, Try cnt:%d, Status:%d\n", type, position, avg, try_cnt, status);
		strlcat(buf, buff, ts->proc_cmoffset_size);

		for (i = 0; i < ts->rx_count; i++) {
			for (j = 0; j < ts->tx_count; j++) {
				temp = rBuff[8 + (j * ts->rx_count) + i];

				if (temp == 127)
					temp = 4095;
				else if (temp == 128)
					temp = 0;
				else if (temp > 127)
					temp = avg + (temp - 256) * 2;
				else
					temp = avg + temp * 2;
				snprintf(buff, sizeof(buff), "%4d", temp);
				strlcat(buf, buff, ts->proc_cmoffset_size);
			}
			snprintf(buff, sizeof(buff), "\n");
			strlcat(buf, buff, ts->proc_cmoffset_size);
		}
	}
	input_err(true, &ts->client->dev, "%s: total buf size:%zu\n", __func__, strlen(buf));

err_invalid:
	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);
	slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_NO);

	input_info(true, &ts->client->dev, "%s: set power mode to normal mode\n", __func__);
	slsi_ts_locked_release_all_finger(ts);
	data[0] = 0x00;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test normal failed\n", __func__);
	}

	kfree(rBuff);
	return 0;

err_i2c:
	kfree(rBuff);
err_mem:
	slsi_ts_write_factory_level(ts, OFFSET_FW_NOSAVE);
	slsi_ts_set_factory_data_type(ts, OFFSET_FAC_DATA_NO);

	input_info(true, &ts->client->dev, "%s: set power mode to normal mode\n", __func__);
	slsi_ts_locked_release_all_finger(ts);
	data[0] = 0x00;
	ret = ts->slsi_ts_i2c_write(ts, SLSI_TS_CMD_SET_POWER_MODE, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: set test normal failed\n", __func__);
	}
err_exit:
	snprintf(buf, ts->proc_cmoffset_size, "NG, error");
	return 0;
}

static ssize_t slsi_ts_tsp_cmoffset_all_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	struct slsi_ts_data *ts = dev_get_drvdata(ptsp);
	static ssize_t retlen = 0;
	ssize_t retlen_sdc = 0, retlen_miscal = 0, retlen_main = 0;
	ssize_t count = 0;
	loff_t pos = *offset;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	int ret;
#endif

	if (!ts) {
		pr_err("%s %s: dev is null\n", SECLOG, __func__);
		return 0;
	}

	if (ts->proc_cmoffset_size == 0) {
		pr_err("%s %s: proc_cmoffset_size is 0\n", SECLOG, __func__);
		return 0;
	}

	mutex_lock(&ts->proc_mutex);

	if (pos == 0) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		ret = get_cmoffset_dump_all(ts, ts->cmoffset_sdc_proc, OFFSET_FW_SDC);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : slsi_ts_get_cmoffset_dump SDC fail use boot time value\n", __func__);
		ret = get_cmoffset_dump_all(ts, ts->cmoffset_main_proc, OFFSET_FW_MAIN);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : slsi_ts_get_cmoffset_dump MAIN fail use boot time value\n", __func__);

		ret = get_miscal_dump(ts, ts->miscal_proc);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : get_miscal fail use boot time value\n", __func__);
#endif
		retlen_sdc = strlen(ts->cmoffset_sdc_proc);
		retlen_main = strlen(ts->cmoffset_main_proc);
		retlen_miscal = strlen(ts->miscal_proc);

		ts->cmoffset_all_proc = kzalloc(ts->proc_cmoffset_all_size, GFP_KERNEL);
		if (!ts->cmoffset_all_proc){
			input_err(true, &ts->client->dev, "%s : kzalloc fail (cmoffset_all_proc)\n", __func__);
			mutex_unlock(&ts->proc_mutex);
			return count;
		}

		strlcat(ts->cmoffset_all_proc, ts->cmoffset_sdc_proc, ts->proc_cmoffset_all_size);
		strlcat(ts->cmoffset_all_proc, ts->cmoffset_main_proc, ts->proc_cmoffset_all_size);
		strlcat(ts->cmoffset_all_proc, ts->miscal_proc, ts->proc_cmoffset_all_size);

		retlen = strlen(ts->cmoffset_all_proc);

		input_info(true, &ts->client->dev, "%s : retlen[%zd], retlen_sdc[%zd], retlen_main[%zd] retlen_miscal[%zd]\n",
						__func__, retlen, retlen_sdc, retlen_main, retlen_miscal);
	}

	if (pos >= retlen) {
		mutex_unlock(&ts->proc_mutex);
		return 0;
	}

	count = min(len, (size_t)(retlen - pos));

	input_info(true, &ts->client->dev, "%s : total:%zd pos:%lld count:%zd\n", __func__, retlen, pos, count);

	if (copy_to_user(buf, ts->cmoffset_all_proc + pos, count)) {
		input_err(true, &ts->client->dev, "%s : copy_to_user error!\n", __func__);
		mutex_unlock(&ts->proc_mutex);
		return -EFAULT;
	}

	*offset += count;

	if (count < len) {
		input_info(true, &ts->client->dev, "%s : print all & free cmoffset_all_proc [%zd][%d]\n",
					__func__, retlen, (int)*offset);
		if (ts->cmoffset_all_proc)
			kfree(ts->cmoffset_all_proc);
		retlen = 0;
	}

	mutex_unlock(&ts->proc_mutex);

	return count;
}

static ssize_t slsi_ts_tsp_fail_hist_all_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	struct slsi_ts_data *ts = dev_get_drvdata(ptsp);

	static ssize_t retlen = 0;
	ssize_t retlen_sdc = 0, retlen_sub = 0, retlen_main = 0;
	ssize_t count = 0;
	loff_t pos = *offset;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	int ret;
#endif

	if (!ts) {
		pr_err("%s %s: dev is null\n", SECLOG, __func__);
		return 0;
	}

	if (ts->proc_fail_hist_size == 0) {
		pr_err("%s %s: proc_fail_hist_size is 0\n", SECLOG, __func__);
		return 0;
	}

	mutex_lock(&ts->proc_mutex);

	if (pos == 0) {
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		ret = get_selftest_fail_hist_dump_all(ts, ts->fail_hist_sdc_proc, OFFSET_FW_SDC);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : slsi_ts_get_fail_hist_dump SDC fail use boot time value\n", __func__);
		ret = get_selftest_fail_hist_dump_all(ts, ts->fail_hist_sub_proc, OFFSET_FW_SUB);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : slsi_ts_get_fail_hist_dump SUB fail use boot time value\n", __func__);
		ret = get_selftest_fail_hist_dump_all(ts, ts->fail_hist_main_proc, OFFSET_FW_MAIN);
		if (ret < 0)
			input_err(true, &ts->client->dev,
				"%s : slsi_ts_get_fail_hist_dump MAIN fail use boot time value\n", __func__);
#endif
		retlen_sdc = strlen(ts->fail_hist_sdc_proc);
		retlen_sub = strlen(ts->fail_hist_sub_proc);
		retlen_main = strlen(ts->fail_hist_main_proc);

		ts->fail_hist_all_proc = kzalloc(ts->proc_fail_hist_all_size, GFP_KERNEL);
		if (!ts->fail_hist_all_proc){
			input_err(true, &ts->client->dev, "%s : kzalloc fail (fail_hist_all_proc)\n", __func__);
			mutex_unlock(&ts->proc_mutex);
			return count;
		}

		strlcat(ts->fail_hist_all_proc, ts->fail_hist_sdc_proc, ts->proc_fail_hist_all_size);
		strlcat(ts->fail_hist_all_proc, ts->fail_hist_sub_proc, ts->proc_fail_hist_all_size);
		strlcat(ts->fail_hist_all_proc, ts->fail_hist_main_proc, ts->proc_fail_hist_all_size);

		retlen = strlen(ts->fail_hist_all_proc);

		input_info(true, &ts->client->dev, "%s : retlen[%zd], retlen_sdc[%zd], retlen_sub[%zd], retlen_main[%zd]\n",
						__func__, retlen, retlen_sdc, retlen_sub, retlen_main);
	}

	if (pos >= retlen) {
		mutex_unlock(&ts->proc_mutex);
		return 0;
	}

	count = min(len, (size_t)(retlen - pos));

	input_info(true, &ts->client->dev, "%s : total:%zd pos:%lld count:%zd\n", __func__, retlen, pos, count);

	if (copy_to_user(buf, ts->fail_hist_all_proc + pos, count)) {
		input_err(true, &ts->client->dev, "%s : copy_to_user error!\n", __func__);
		mutex_unlock(&ts->proc_mutex);
		return -EFAULT;
	}

	*offset += count;

	if (count < len) {
		input_info(true, &ts->client->dev, "%s : print all & free fail_hist_all_proc [%zd][%d]\n",
					__func__, retlen, (int)*offset);
		if (ts->fail_hist_all_proc)
			kfree(ts->fail_hist_all_proc);
		retlen = 0;
	}

	mutex_unlock(&ts->proc_mutex);

	return count;
}

static ssize_t slsi_ts_tsp_cmoffset_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	pr_info("[sec_input] %s called offset:%d\n", __func__, (int)*offset);
	return slsi_ts_tsp_cmoffset_all_read(file, buf, len, offset);
}

static ssize_t slsi_ts_tsp_fail_hist_read(struct file *file, char __user *buf,
					size_t len, loff_t *offset)
{
	pr_info("[sec_input] %s called fail_hist:%d\n", __func__, (int)*offset);
	return slsi_ts_tsp_fail_hist_all_read(file, buf, len, offset);
}

static const struct file_operations tsp_cmoffset_all_file_ops = {
	.owner = THIS_MODULE,
	.read = slsi_ts_tsp_cmoffset_read,
	.llseek = generic_file_llseek,
};

static const struct file_operations tsp_fail_hist_all_file_ops = {
	.owner = THIS_MODULE,
	.read = slsi_ts_tsp_fail_hist_read,
	.llseek = generic_file_llseek,
};

void slsi_ts_init_proc(struct slsi_ts_data *ts)
{
	struct proc_dir_entry *entry_cmoffset_all;
	struct proc_dir_entry *entry_fail_hist_all;

	ts->proc_cmoffset_size = (ts->tx_count * ts->rx_count * 4 + 100) * 4;	/* cm1 cm2 cm3 miscal*/
	ts->proc_cmoffset_all_size = ts->proc_cmoffset_size * 2;	/* sdc main */

	ts->proc_fail_hist_size = ((ts->tx_count + ts->rx_count) * 4 + 100) * 6;	/* have to check */
	ts->proc_fail_hist_all_size = ts->proc_fail_hist_size * 3;	/* sdc sub main */

	ts->cmoffset_sdc_proc = kzalloc(ts->proc_cmoffset_size, GFP_KERNEL);
	if (!ts->cmoffset_sdc_proc)
		return;

	ts->cmoffset_main_proc = kzalloc(ts->proc_cmoffset_size, GFP_KERNEL);
	if (!ts->cmoffset_main_proc)
		goto err_alloc_main;
	
	ts->miscal_proc= kzalloc(ts->proc_cmoffset_size, GFP_KERNEL);
	if (!ts->miscal_proc)
		goto err_alloc_miscal;

	ts->fail_hist_sdc_proc = kzalloc(ts->proc_fail_hist_size, GFP_KERNEL);
	if (!ts->fail_hist_sdc_proc)
		goto err_alloc_fail_hist_sdc;

	ts->fail_hist_sub_proc = kzalloc(ts->proc_fail_hist_size, GFP_KERNEL);
	if (!ts->fail_hist_sub_proc)
		goto err_alloc_fail_hist_sub;

	ts->fail_hist_main_proc = kzalloc(ts->proc_fail_hist_size, GFP_KERNEL);
	if (!ts->fail_hist_main_proc)
		goto err_alloc_fail_hist_main;

	entry_cmoffset_all = proc_create("tsp_cmoffset_all", S_IFREG | S_IRUGO, NULL, &tsp_cmoffset_all_file_ops);
	if (!entry_cmoffset_all) {
		input_err(true, &ts->client->dev, "%s: failed to create /proc/tsp_cmoffset_all\n", __func__);
		goto err_cmoffset_proc_create;
	}

	entry_fail_hist_all = proc_create("tsp_fail_hist_all", S_IFREG | S_IRUGO, NULL, &tsp_fail_hist_all_file_ops);
	if (!entry_fail_hist_all) {
		input_err(true, &ts->client->dev, "%s: failed to create /proc/tsp_fail_hist_all\n", __func__);
		goto err_fail_hist_proc_create;
	}

	input_info(true, &ts->client->dev, "%s: done\n", __func__);
	return;

err_fail_hist_proc_create:
	proc_remove(entry_cmoffset_all);
err_cmoffset_proc_create:
	kfree(ts->fail_hist_main_proc);
err_alloc_fail_hist_main:
	kfree(ts->fail_hist_sub_proc);
err_alloc_fail_hist_sub:
	kfree(ts->fail_hist_sdc_proc);
err_alloc_fail_hist_sdc:
	kfree(ts->miscal_proc);
err_alloc_miscal:
	kfree(ts->cmoffset_main_proc);
err_alloc_main:
	kfree(ts->cmoffset_sdc_proc);

	ts->cmoffset_sdc_proc = NULL;
	ts->cmoffset_main_proc = NULL;
	ts->miscal_proc = NULL;
	ts->cmoffset_all_proc = NULL;
	ts->proc_cmoffset_size = 0;
	ts->proc_cmoffset_all_size = 0;

	ts->fail_hist_sdc_proc = NULL;
	ts->fail_hist_sub_proc = NULL;
	ts->fail_hist_main_proc = NULL;
	ts->fail_hist_all_proc = NULL;
	ts->proc_fail_hist_size = 0;
	ts->proc_fail_hist_all_size = 0;

	input_err(true, &ts->client->dev, "%s: failed\n", __func__);
}

MODULE_LICENSE("GPL");
