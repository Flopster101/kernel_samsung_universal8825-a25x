#include "stm_dev.h"
#include "stm_reg.h"

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#define SUPPORT_FW_SIGNED
#endif

#ifdef SUPPORT_FW_SIGNED
#include <linux/spu-verify.h>
#endif

#define STM_TS_FILE_SIGNATURE 		0xAA55AA55

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	TSP_SIGNED_SDCARD,
	TSP_SPU,
	TSP_VERIFICATION,
};

/**
 * @brief  Type definitions - header structure of firmware file (ftb)
 */

struct stm_ts_header {
	u32	signature;
	u32	ftb_ver;
	u32	target;
	u32	fw_id;
	u32	fw_ver;
	u32	cfg_id;
	u32	cfg_ver;
	u8	fw_area_bs;	// bs : block size
	u8	panel_area_bs;
	u8	cx_area_bs;
	u8	cfg_area_bs;
	u32	reserved1;
	u32	ext_release_ver;
	u8	project_id;
	u8	ic_name;
	u8	module_ver;
	u8	reserved2;
	u32	sec0_size;
	u32	sec1_size;
	u32	sec2_size;
	u32	sec3_size;
	u32	hdr_crc;
} __packed;

#define FW_HEADER_SIZE  64

#define WRITE_CHUNK_SIZE 1024
#define DRAM_SIZE (32 * 1024) // 64kB
#define	SIGNEDKEY_SIZE		(256)

int stm_ts_check_dma_startanddone(struct stm_ts_data *ts)
{
	int timeout = 60;
	u8 reg[6] = { STM_TS_CMD_REG_W, 0x20, 0x00, 0x00, 0x71, 0xC0 };
	u8 val[1];
	int ret;

	ret = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write\n", __func__);
		return ret;
	}

	sec_delay(10);

	do {
		reg[0] = STM_TS_CMD_REG_R;
		ret = ts->stm_ts_read(ts, &reg[0], 5, (u8 *)val, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
			return ret;
		}

		if ((val[0] & 0x80) != 0x80)
			break;

		sec_delay(50);
		timeout--;
	} while (timeout != 0);

	if (timeout == 0) {
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
		return -EIO;
	}

	return 0;
}

static int stm_ts_check_erase_done(struct stm_ts_data *ts)
{
	int timeout = 60;  // 3 sec timeout
	u8 reg[5] = { STM_TS_CMD_REG_R, 0x20, 0x00, 0x00, 0x6A };
	u8 val[1];
	int ret;

	do {
		ret = ts->stm_ts_read(ts, &reg[0], 5, (u8 *)val, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
			return ret;
		}

		if ((val[0] & 0x80) != 0x80)
			break;

		sec_delay(50);
		timeout--;
	} while (timeout != 0);

	if (timeout == 0) {
		input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
		return -EIO;
	}

	return 0;
}

int stm_ts_fw_fillflash(struct stm_ts_data *ts, u32 address, u8 *data, int size)
{
	int remaining, index = 0;
	int towrite = 0;
	int byteblock = 0;
	int wheel = 0;
	u32 addr = 0;
	int rc;
	int delta;

	u8 buff[WRITE_CHUNK_SIZE + 5] = {0};
	u8 buff2[12] = {0};

	remaining = size;
	while (remaining > 0) {
		byteblock = 0;
		addr = 0x00100000;

		while ((byteblock < DRAM_SIZE) && remaining > 0) {
			if (remaining >= WRITE_CHUNK_SIZE) {
				if ((byteblock + WRITE_CHUNK_SIZE) <= DRAM_SIZE) {
					towrite = WRITE_CHUNK_SIZE;
					remaining -= WRITE_CHUNK_SIZE;
					byteblock += WRITE_CHUNK_SIZE;
				} else {
					delta = DRAM_SIZE - byteblock;
					towrite = delta;
					remaining -= delta;
					byteblock += delta;
				}
			} else {
				if ((byteblock + remaining) <= DRAM_SIZE) {
					towrite = remaining;
					byteblock += remaining;
					remaining = 0;
				} else {
					delta = DRAM_SIZE - byteblock;
					towrite = delta;
					remaining -= delta;
					byteblock += delta;
				}
			}

			index = 0;
			buff[index++] = 0xFA;
			buff[index++] = (u8) ((addr & 0xFF000000) >> 24);
			buff[index++] = (u8) ((addr & 0x00FF0000) >> 16);
			buff[index++] = (u8) ((addr & 0x0000FF00) >> 8);
			buff[index++] = (u8) ((addr & 0x000000FF));

			memcpy(&buff[index], data, towrite);

			rc = ts->stm_ts_write(ts, &buff[0], index + towrite, NULL, 0);
			if (rc < 0) {
				input_err(true, &ts->client->dev,
						"%s failed to write i2c register. ret:%d\n",
						__func__, rc);
				return rc;
			}
			sec_delay(5);

			addr += towrite;
			data += towrite;
		}

		input_info(true, &ts->client->dev, "%s: Write %d Bytes\n", __func__, byteblock);

		//configuring the DMA
		byteblock = byteblock / 4 - 1;

		index = 0;
		buff2[index++] = 0xFA;
		buff2[index++] = 0x20;
		buff2[index++] = 0x00;
		buff2[index++] = 0x00;
		buff2[index++] = 0x72;
		buff2[index++] = 0x00;
		buff2[index++] = 0x00;

		addr = address + ((wheel * DRAM_SIZE) / 4);
		buff2[index++] = (u8) ((addr & 0x000000FF));
		buff2[index++] = (u8) ((addr & 0x0000FF00) >> 8);
		buff2[index++] = (u8) (byteblock & 0x000000FF);
		buff2[index++] = (u8) ((byteblock & 0x0000FF00) >> 8);
		buff2[index++] = 0x00;

		rc = ts->stm_ts_write(ts, &buff2[0], index, NULL, 0);
		if (rc < 0) {
			input_err(true, &ts->client->dev,
					"%s failed to write i2c register. ret:%d\n",
					__func__, rc);
			return rc;
		}
		sec_delay(10);

		rc = stm_ts_check_dma_startanddone(ts);
		if (rc < 0)
			return rc;

		wheel++;
	}

	return 0;
}

static int stm_ts_fw_burn(struct stm_ts_data *ts, const u8 *fw_data)
{
	const struct stm_ts_header *fw_header;
	u8 *pfwdata;
	int rc;
	int i;
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0};
	int numberofmainblock = 0;
	int indexofconfigblock = 0;

	fw_header = (struct stm_ts_header *) &fw_data[0];

	if ((fw_header->fw_area_bs) > 0)
		numberofmainblock = fw_header->fw_area_bs;
	else
		numberofmainblock = 26; // original value

	input_info(true, &ts->client->dev, "%s: Number Of MainBlock: %d\n",
			__func__, numberofmainblock);

	indexofconfigblock = fw_header->fw_area_bs + fw_header->panel_area_bs + fw_header->cx_area_bs;
	input_info(true, &ts->client->dev, "%s: Index of Config Block: %d\n",
			__func__, indexofconfigblock);

	// System Reset and Hold
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x24;
	reg[5] = 0x01;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(200);

	rc = stm_ts_wire_mode_change(ts, reg);
	if (rc < 0)
		return rc;

	// Change application mode
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x25;
	reg[5] = 0x20;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// Unlock Flash
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0xDE;
	reg[5] = 0x03;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(200);

	//==================== Erase Partial Flash ====================
	input_info(true, &ts->client->dev, "%s: Start Flash(Main Block) Erasing\n", __func__);
	for (i = 0; i < numberofmainblock; i++) {
		reg[0] = STM_TS_CMD_REG_W;
		reg[1] = 0x20;
		reg[2] = 0x00;
		reg[3] = 0x00;
		reg[4] = 0x6B;
		reg[5] = 0x00;
		rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
		if (rc < 0)
			return rc;
		sec_delay(50);

		reg[0] = STM_TS_CMD_REG_W;
		reg[1] = 0x20; 
		reg[2] = 0x00;
		reg[3] = 0x00; 
		reg[4] = 0x6A;
		reg[5] = (0x80 + i) & 0xFF;
		rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
		if (rc < 0)
			return rc;
		rc = stm_ts_check_erase_done(ts);
		if (rc < 0)
			return rc;
	}

	input_info(true, &ts->client->dev, "%s: Start Flash(Config Block) Erasing\n", __func__);
	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6B;
	reg[5] = 0x00;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(50);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x6A;
#if IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SPI)
	reg[5] = (0x80 + indexofconfigblock) & 0xFF;
#else
	reg[5] = (0x80 + 31) & 0xFF;
#endif
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;

	rc = stm_ts_check_erase_done(ts);
	if (rc < 0)
		return rc;

	// Code Area
	if (fw_header->sec0_size > 0) {
		pfwdata = (u8 *) &fw_data[FW_HEADER_SIZE];

		input_info(true, &ts->client->dev, "%s: Start Flashing for Code\n", __func__);
		rc = stm_ts_fw_fillflash(ts, CODE_ADDR_START, &pfwdata[0], fw_header->sec0_size);
		if (rc < 0)
			return rc;

		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for Code\n",
				__func__, fw_header->sec0_size);
	}

	// Config Area
	if (fw_header->sec1_size > 0) {
		input_info(true, &ts->client->dev, "%s: Start Flashing for Config\n", __func__);
		pfwdata = (u8 *) &fw_data[FW_HEADER_SIZE + fw_header->sec0_size];
		rc = stm_ts_fw_fillflash(ts, CONFIG_ADDR_START, &pfwdata[0], fw_header->sec1_size);
		if (rc < 0)
			return rc;
		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for Config\n",
				__func__, fw_header->sec1_size);
	}

	// CX Area
	if (fw_header->sec2_size > 0) {
		input_info(true, &ts->client->dev, "%s: Start Flashing for CX\n", __func__);
		pfwdata = (u8 *) &fw_data[FW_HEADER_SIZE + fw_header->sec0_size + fw_header->sec1_size];
		rc = stm_ts_fw_fillflash(ts, CX_ADDR_START, &pfwdata[0], fw_header->sec2_size);
		if (rc < 0)
			return rc;
		input_info(true, &ts->client->dev, "%s: Finished total flashing %u Bytes for CX\n",
				__func__, fw_header->sec2_size);
	}

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x24;
	reg[5] = 0x80;

	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(200);

	// System Reset
	ts->stm_ts_systemreset(ts, 0);

	return 0;
}

static void stm_ts_set_factory_history_data(struct stm_ts_data *ts, u8 level)
{
	int ret;
	u8 regaddr[3] = { 0 };
	u8 wlevel;

	switch (level) {
	case OFFSET_FAC_NOSAVE:
		input_info(true, &ts->client->dev, "%s: not save to flash area\n", __func__);
		return;
	case OFFSET_FAC_SUB:
		wlevel = OFFSET_FW_SUB;
		break;
	case OFFSET_FAC_MAIN:
		wlevel = OFFSET_FW_MAIN;
		break;
	default:
		input_info(true, &ts->client->dev, "%s: wrong level %d\n", __func__, level);
		return;
	}

	regaddr[0] = 0xC7;
	regaddr[1] = 0x04;
	regaddr[2] = wlevel;

	ret = ts->stm_ts_write(ts, regaddr, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to write factory level %d\n", __func__, wlevel);
		return;
	}

	regaddr[0] = 0xA4;
	regaddr[1] = 0x05;
	regaddr[2] = 0x04; /* panel configuration area */

	ret = stm_ts_wait_for_echo_event(ts, regaddr, 3, 200);
	if (ret < 0)
		return;

	input_info(true, &ts->client->dev, "%s: save to flash area, level=%d\n", __func__, wlevel);
	return;
}

int stm_ts_execute_autotune(struct stm_ts_data *ts, bool issaving)
{
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0,};
	u8 datatype = 0x00;
	int rc;

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	stm_ts_set_scanmode(ts, STM_TS_SCAN_MODE_SCAN_OFF);
	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	// w A4 00 03
	if (issaving == true) {
		// full panel init
		reg[0] = 0xA4;
		reg[1] = 0x00;
		reg[2] = 0x03;

		rc = stm_ts_wait_for_echo_event(ts, &reg[0], 3, 500);
#ifdef TCLM_CONCEPT
		if (ts->tdata->nvdata.cal_fail_cnt == 0xFF)
			ts->tdata->nvdata.cal_fail_cnt = 0;
		if (rc < 0) {
			ts->tdata->nvdata.cal_fail_cnt++;
			ts->tdata->nvdata.cal_fail_falg = 0;
		} else {
			ts->tdata->nvdata.cal_fail_falg = SEC_CAL_PASS;
			ts->is_cal_done = true;
		}
		stm_tclm_data_write(ts, SEC_TCLM_NVM_ALL_DATA);
#endif
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	} else {
		// SS ATune
		//DataType = 0x0C;
		datatype = 0x3F;

		reg[0] = 0xA4;
		reg[1] = 0x03;
		reg[2] = (u8)datatype;
		reg[3] = 0x00;

		rc = stm_ts_wait_for_echo_event(ts, &reg[0], 4, 500);
		if (rc < 0) {
			input_err(true, &ts->client->dev, "%s: timeout\n", __func__);
			goto ERROR;
		}
	}

	stm_ts_set_factory_history_data(ts, ts->factory_position);
	if (issaving == true)
		stm_ts_panel_ito_test(ts, SAVE_MISCAL_REF_RAW);

ERROR:
	stm_ts_set_scanmode(ts, ts->scan_mode);
	ts->factory_position = OFFSET_FAC_NOSAVE;

	return rc;
}

static const int stm_ts_fw_updater(struct stm_ts_data *ts, const u8 *fw_data)
{
	const struct stm_ts_header *header;
	int retval;
	int retry;
	u16 fw_main_version;

	if (!fw_data) {
		input_err(true, &ts->client->dev, "%s: Firmware data is NULL\n",
				__func__);
		return -ENODEV;
	}

	header = (struct stm_ts_header *)fw_data;
	fw_main_version = (u16)header->ext_release_ver;

	input_info(true, &ts->client->dev,
			"%s: Starting firmware update : 0x%04X\n", __func__,
			fw_main_version);

	retry = 0;

	disable_irq(ts->client->irq);

	while (1) {
		retval = stm_ts_fw_burn(ts, fw_data);
		if (retval >= 0) {
			stm_ts_get_version_info(ts);

			if (fw_main_version == ts->fw_main_version_of_ic) {
				input_info(true, &ts->client->dev,
						"%s: Success Firmware update\n",
						__func__);
				ts->fw_corruption = false;

				retval = ts->stm_ts_systemreset(ts, 0);

				if (retval == -STM_TS_ERROR_BROKEN_OSC_TRIM) {
					retval = stm_ts_osc_trim_recovery(ts);
					if (retval < 0)
						input_err(true, &ts->client->dev, "%s: Failed to recover osc trim\n", __func__);
					else
						ts->fw_corruption = false;
				}

				stm_ts_set_scanmode(ts, ts->scan_mode);
				retval = 0;
				break;
			}
		}

		if (retry++ > 3) {
			input_err(true, &ts->client->dev, "%s: Fail Firmware update\n",
					__func__);
			retval = -EIO;
			break;
		}
	}

	enable_irq(ts->client->irq);

	return retval;
}

int stm_ts_fw_update_on_probe(struct stm_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	char fw_path[STM_TS_MAX_FW_PATH];
	const struct stm_ts_header *header;
#ifdef TCLM_CONCEPT
	int ret = 0;
	bool restore_cal = false;

	if (ts->tdata->support_tclm_test) {
		ret = sec_tclm_test_on_probe(ts->tdata);
		if (ret < 0)
			input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA read fail", __func__);
	}
#endif

	if (ts->plat_data->bringup == 1)
		return STM_TS_NOT_ERROR;

 	if (!ts->plat_data->firmware_name) {
		input_err(true, &ts->client->dev, "%s: firmware name does not declair in dts\n", __func__);
		retval = -ENOENT;
		goto exit_fwload;
	}

	snprintf(fw_path, STM_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	input_info(true, &ts->client->dev, "%s: Load firmware : %s\n", __func__, fw_path);

	retval = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (retval) {
		input_err(true, &ts->client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		retval = STM_TS_NOT_ERROR;
		goto exit_fwload;
	}

	header = (struct stm_ts_header *)fw_entry->data;

	ts->fw_version_of_bin = (u16)header->fw_ver;
	ts->fw_main_version_of_bin = (u16)header->ext_release_ver;
	ts->config_version_of_bin = (u16)header->cfg_ver;
	ts->project_id_of_bin = header->project_id;
	ts->ic_name_of_bin = header->ic_name;
	ts->module_version_of_bin = header->module_ver;
	ts->plat_data->img_version_of_bin[2] = ts->module_version_of_bin;
	ts->plat_data->img_version_of_bin[3] = ts->fw_main_version_of_bin & 0xFF;

	input_info(true, &ts->client->dev,
			"%s: [BIN] Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X\n", __func__,
			ts->fw_version_of_bin,
			ts->config_version_of_bin,
			ts->fw_main_version_of_bin);
	input_info(true, &ts->client->dev,
			"%s: [BIN] Project ID: 0x%02X, IC Name: 0x%02X, Module Ver: 0x%02X\n",
			__func__, ts->project_id_of_bin,
			ts->ic_name_of_bin, ts->module_version_of_bin);

	if (ts->plat_data->bringup == 2) {
		input_err(true, &ts->client->dev, "%s: skip fw_update for bringup\n", __func__);
		retval = STM_TS_NOT_ERROR;
		goto done;
	}

	if (ts->plat_data->hw_param.checksum_result)
		retval = STM_TS_NEED_FW_UPDATE;
	else if ((ts->ic_name_of_ic == 0xFF) && (ts->project_id_of_ic == 0xFF) && (ts->module_version_of_ic == 0xFF))
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ts->ic_name_of_ic != ts->ic_name_of_bin)
		retval = STM_TS_NOT_UPDATE;
	else if (ts->project_id_of_ic != ts->project_id_of_bin)
		retval = STM_TS_NEED_FW_UPDATE;
	else if (ts->module_version_of_ic != ts->module_version_of_bin)
		retval = STM_TS_NOT_UPDATE;
	else if ((ts->fw_main_version_of_ic < ts->fw_main_version_of_bin)
			|| ((ts->config_version_of_ic < ts->config_version_of_bin))
			|| ((ts->fw_version_of_ic < ts->fw_version_of_bin)))
		retval = STM_TS_NEED_FW_UPDATE;
	else
		retval = STM_TS_NOT_ERROR;

	if ((ts->plat_data->bringup == 3) &&
			((ts->fw_main_version_of_ic != ts->fw_main_version_of_bin)
			|| (ts->config_version_of_ic != ts->config_version_of_bin)
			|| (ts->fw_version_of_ic != ts->fw_version_of_bin))) {
		input_info(true, &ts->client->dev,
				"%s: bringup 3, force update because version is different\n", __func__);
		retval = STM_TS_NEED_FW_UPDATE;
	}

	/* ic fw ver > bin fw ver && force is false */
	if (retval != STM_TS_NEED_FW_UPDATE) {
		input_err(true, &ts->client->dev, "%s: skip fw update\n", __func__);

		goto done;
	}

	retval = stm_ts_fw_updater(ts, fw_entry->data);
	if (retval < 0)
		goto done;

#ifdef TCLM_CONCEPT
	ret = stm_tclm_data_read(ts, SEC_TCLM_NVM_ALL_DATA);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: SEC_TCLM_NVM_ALL_DATA i2c read fail", __func__);
		goto done;
	}

	input_info(true, &ts->client->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
		__func__, ts->tdata->nvdata.tune_fix_ver, ts->tdata->afe_base);

	if ((ts->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
		((ts->tdata->nvdata.tune_fix_ver == 0xffff)
		|| (ts->tdata->afe_base > ts->tdata->nvdata.tune_fix_ver))) {
		/* tune version up case */
		sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TUNEUP);
		restore_cal = true;
	} else if (ts->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
		/* firmup case */
		sec_tclm_root_of_cal(ts->tdata, CALPOSITION_FIRMUP);
		restore_cal = true;
	}

	if (restore_cal) {
		input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
		if (sec_execute_tclm_package(ts->tdata, 0) < 0)
			input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);
	}
#endif

done:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	release_firmware(fw_entry);
exit_fwload:
	return retval;
}
EXPORT_SYMBOL(stm_ts_fw_update_on_probe);

static int stm_ts_load_fw_from_kernel(struct stm_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	char fw_path[STM_TS_MAX_FW_PATH];

	if (ts->plat_data->bringup == 1) {
		retval = -1;
		input_info(true, &ts->client->dev, "%s: can't update for bringup:%d\n",
				__func__, ts->plat_data->bringup);
		return retval;
	}

	
	if (ts->plat_data->firmware_name)
		snprintf(fw_path, STM_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	else
		return 0;

	input_info(true, &ts->client->dev, "%s: Load firmware : %s\n", __func__,
			fw_path);

	retval = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (retval) {
		input_err(true, &ts->client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		retval = -ENOENT;
		goto done;
	}


	ts->stm_ts_systemreset(ts, 20);

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
#endif

	retval = stm_ts_fw_updater(ts, fw_entry->data);
	if (retval < 0)
		input_err(true, &ts->client->dev, "%s: failed update firmware\n",
				__func__);

#ifdef TCLM_CONCEPT
	input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
	if (sec_execute_tclm_package(ts->tdata, 0) < 0)
		input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);

	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

done:
	if (fw_entry)
		release_firmware(fw_entry);
	return retval;
}

static int stm_ts_load_fw(struct stm_ts_data *ts, int update_type)
{
	int error = 0;
	const struct stm_ts_header *header;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool is_fw_signed = false;

	switch (update_type) {
	case TSP_SDCARD:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW);
#else
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW_SIGNED);
		is_fw_signed = true;
#endif

		break;
	case TSP_SPU:
	case TSP_VERIFICATION:
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_SPU_FW_SIGNED);
		is_fw_signed = true;
		break;
	default:
		return -EINVAL;
	}

	error = request_firmware(&fw_entry, fw_path, &ts->client->dev);
	if (error) {
		input_err(true, &ts->client->dev, "%s: firmware is not available %d\n", __func__, error);
		goto err_request_fw;
	}



	header = (struct stm_ts_header *)fw_entry->data;
	if (header->signature != STM_TS_FILE_SIGNATURE) {
		error = -EIO;
		input_err(true, &ts->client->dev,
				"%s: File type is not match with stm_ts64 file. [%8x]\n",
				__func__, header->signature);
		goto done;
	}

	input_info(true, &ts->client->dev,
			"%s:%s FirmVer:0x%04X, MainVer:0x%04X,"
			" IC:0x%02X, Proj:0x%02X, Module:0x%02X\n",
			__func__, is_fw_signed ? " [SIGNED]":"",
			(u16)header->fw_ver, (u16)header->ext_release_ver,
			header->ic_name, header->project_id, header->module_ver);

#ifdef SUPPORT_FW_SIGNED
	if (is_fw_signed) {
		long spu_ret = 0, org_size = 0;
		if (update_type == TSP_VERIFICATION) {
			org_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
			spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
			if (spu_ret != org_size) {
				input_err(true, &ts->client->dev, "%s: signature verify failed, spu_ret:%ld, org_size:%ld\n",
					__func__, spu_ret, org_size);
				error = -EPERM;
			}
			goto done;
		} else if (update_type == TSP_SPU && ts->ic_name_of_ic == header->ic_name
				&& ts->project_id_of_ic == header->project_id
				&& ts->module_version_of_ic == header->module_ver) {
			if ((ts->fw_main_version_of_ic >= header->ext_release_ver)
				&& (ts->config_version_of_ic >= header->cfg_ver)
				&& (ts->fw_version_of_ic >= header->fw_ver)) {
				input_info(true, &ts->client->dev, "%s: skip spu update\n", __func__);
				error = 0;
				goto done;
			} else {
				input_info(true, &ts->client->dev, "%s: run spu update\n", __func__);
			}
		} else if (update_type == TSP_SDCARD && ts->ic_name_of_ic == header->ic_name) {
			input_info(true, &ts->client->dev, "%s: run signed fw update\n", __func__);
		} else {
			input_err(true, &ts->client->dev,
					"%s: not matched product version\n", __func__);
			error = -ENOENT;
			goto done;
		}

		/* digest 32, signature 512, TSP tag 3 */
		org_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != org_size) {
			input_err(true, &ts->client->dev,
					"%s: signature verify failed, %ld/%ld\n",
					__func__, spu_ret, org_size);
			error = -EIO;
			goto done;
		}
	}
#endif
	ts->stm_ts_systemreset(ts, 0);

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
#endif

	error = stm_ts_fw_updater(ts, fw_entry->data);

#ifdef TCLM_CONCEPT
	input_info(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
	if (sec_execute_tclm_package(ts->tdata, 0) < 0)
		input_err(true, &ts->client->dev, "%s: sec_execute_tclm_package fail\n", __func__);

	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
done:
	if (error < 0)
		input_err(true, &ts->client->dev, "%s: failed update firmware\n",
				__func__);
	release_firmware(fw_entry);
err_request_fw:
	return error;
}

int stm_ts_fw_update_on_hidden_menu(struct stm_ts_data *ts, int update_type)
{
	int retval = SEC_ERROR;

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from apk.
	 */
	switch (update_type) {
	case TSP_BUILT_IN:
		retval = stm_ts_load_fw_from_kernel(ts);
		break;

	case TSP_SDCARD:
	case TSP_SPU:
	case TSP_VERIFICATION:
		retval = stm_ts_load_fw(ts, update_type);
		break;

	default:
		input_err(true, &ts->client->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	stm_ts_get_custom_library(ts);
	stm_ts_set_custom_library(ts);

	return retval;
}
EXPORT_SYMBOL(stm_ts_fw_update_on_hidden_menu);
MODULE_LICENSE("GPL");
