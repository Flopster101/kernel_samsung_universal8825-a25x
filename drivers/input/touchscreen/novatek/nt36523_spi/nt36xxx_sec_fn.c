/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "nt36xxx.h"

//int nvt_ts_nt36523_ics_i2c_read(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len);
//int nvt_ts_nt36523_ics_i2c_write(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len);

#if SHOW_NOT_SUPPORT_CMD
static int nvt_ts_set_touchable_area(struct nvt_ts_data *ts)
{
	u16 smart_view_area[4] = {0, 210, TOUCH_DEFAULT_MAX_HEIGHT, 2130};
	char data[10] = { 0 };
	int ret;

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_BLOCK_AREA);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set xdata index\n",
				__func__);
		return ret;
	}

	//---set area---
	data[0] = EVENT_MAP_BLOCK_AREA;
	data[1] = smart_view_area[0] & 0xFF;
	data[2] = smart_view_area[0] >> 8;
	data[3] = smart_view_area[1] & 0xFF;
	data[4] = smart_view_area[1] >> 8;
	data[5] = smart_view_area[2] & 0xFF;
	data[6] = smart_view_area[2] >> 8;
	data[7] = smart_view_area[3] & 0xFF;
	data[8] = smart_view_area[3] >> 8;
	ret = CTP_SPI_WRITE(ts->client, data, 9);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set area\n",
				__func__);
		return ret;
	}

	return 0;
}

static int nvt_ts_set_untouchable_area(struct nvt_ts_data *ts)
{
	char data[10] = { 0 };
	u16 touchable_area[4] = {0, 0, DEFAULT_MAX_WIDTH, DEFAULT_MAX_HEIGHT};
	int ret;

	if (ts->landscape_deadzone[0])
		touchable_area[1] = ts->landscape_deadzone[0];

	if (ts->landscape_deadzone[1])
		touchable_area[3] -= ts->landscape_deadzone[1];

	//---set xdata index to EVENT BUF ADDR---
	data[0] = 0xFF;
	data[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	data[2] = (ts->mmap->EVENT_BUF_ADDR>> 8) & 0xFF;
	ret = nvt_ts_i2c_write(ts, I2C_FW_Address, data, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set xdata index\n",
				__func__);
		return ret;
	}

	//---set area---
	data[0] = EVENT_MAP_BLOCK_AREA;
	data[1] = touchable_area[0] & 0xFF;
	data[2] = touchable_area[0] >> 8;
	data[3] = touchable_area[1] & 0xFF;
	data[4] = touchable_area[1] >> 8;
	data[5] = touchable_area[2] & 0xFF;
	data[6] = touchable_area[2] >> 8;
	data[7] = touchable_area[3] & 0xFF;
	data[8] = touchable_area[3] >> 8;
	ret = nvt_ts_i2c_write(ts, I2C_FW_Address, data, 9);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set area\n",
				__func__);
		return ret;
	}

	return 0;
}

static int nvt_ts_disable_block_mode(struct nvt_ts_data *ts)
{
	char data[10] = { 0 };
	int ret;

	//---set xdata index to EVENT BUF ADDR---
	data[0] = 0xFF;
	data[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	data[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	ret = nvt_ts_i2c_write(ts, I2C_FW_Address, data, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set xdata index\n",
				__func__);
		return ret;
	}

	//---set dummy value---
	data[0] = EVENT_MAP_BLOCK_AREA;
	data[1] = 1;
	data[2] = 1;
	data[3] = 1;
	data[4] = 1;
	data[5] = 1;
	data[6] = 1;
	data[7] = 1;
	data[8] = 1;
	ret = nvt_ts_i2c_write(ts, I2C_FW_Address, data, 9);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set area\n",
				__func__);
		return ret;
	}

	return 0;
}
#endif	// end of #if SHOW_NOT_SUPPORT_CMD

static int nvt_ts_clear_fw_status(struct nvt_ts_data *ts)
{
	u8 buf[8] = {0};
	int i = 0;
	const int retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		msleep(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev,"failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

static int nvt_ts_check_fw_status(struct nvt_ts_data *ts)
{
	u8 buf[8] = {0};
	int i = 0;
	const int retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		msleep(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev,"failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

static void nvt_ts_read_mdata(struct nvt_ts_data *ts, int *buff, u32 xdata_addr, s32 xdata_btn_addr)
{
	u8 buf[BUS_TRANSFER_LENGTH + 1] = { 0 };
	u8 *rawdata_buf;
	u32 head_addr = 0;
	int dummy_len = 0;
	int data_len = 0;
	int residual_len = 0;
	int i, j, k;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->platdata->x_num * ts->platdata->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	rawdata_buf = vzalloc(ts->platdata->x_num * ts->platdata->y_num * 2 + dummy_len + XDATA_SECTOR_SIZE);
	if (!rawdata_buf)
		return;

	input_info(true, &ts->client->dev,"head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read data : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read xdata---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				rawdata_buf[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				rawdata_buf[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++)
		buff[i] = (s16)(rawdata_buf[dummy_len + i * 2] + 256 * rawdata_buf[dummy_len + i * 2 + 1]);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	vfree(rawdata_buf);
}

static void nvt_ts_change_mode(struct nvt_ts_data *ts, u8 mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_SPI_WRITE(ts->client, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		msleep(20);
	}
}

static u8 nvt_ts_get_fw_pipe(struct nvt_ts_data *ts)
{
	u8 buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);

	//input_info(true, &ts->client->dev,"FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

static int nvt_ts_hand_shake_status(struct nvt_ts_data *ts)
{
	u8 buf[8] = {0};
	const int retry = 150;
	int i;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		msleep(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "%s: failed to hand shake status, buf[1]=0x%02X\n",
			__func__, buf[1]);

		// Read back 5 bytes from offset EVENT_MAP_HOST_CMD for debug check
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);
		input_err(true, &ts->client->dev, "%s: read back 5 bytes from offset EVENT_MAP_HOST_CMD: "
			"0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			__func__, buf[1], buf[2], buf[3], buf[4], buf[5]);

		return -EPERM;
	}

	return 0;
}

static int nvt_ts_switch_freqhops(struct nvt_ts_data *ts, u8 freqhop)
{
	u8 buf[8] = {0};
	u8 retry = 0;

	for (retry = 0; retry < 20; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		//---switch freqhop---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = freqhop;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(35);

		buf[0] = EVENT_MAP_HOST_CMD;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(retry == 20)) {
		input_err(true, &ts->client->dev, "%s: failed to switch freq hop 0x%02X, buf[1]=0x%02X\n",
			__func__, freqhop, buf[1]);
		return -EPERM;
	}

	return 0;
}

static void nvt_ts_calc_buff(struct nvt_ts_data *ts, int *buff, int *min, int *max)
{
	int i, j;

	for (i = 0; i < ts->platdata->y_num; i++) {
		for (j = 0; j < ts->platdata->x_num; j++) {
			*min = MIN(*min, buff[i * ts->platdata->x_num + j]);
			*max = MAX(*max, buff[i * ts->platdata->x_num + j]);
		}
	}
}

static void nvt_ts_print_buff(struct nvt_ts_data *ts, int *buff, int *min, int *max)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int offset;
	int i, j;
	int lsize = 7 * (ts->platdata->x_num + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		return;

	snprintf(pTmp, sizeof(pTmp), "  X   ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "  %02d  ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->platdata->y_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Y%02d |", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->platdata->x_num; j++) {
			offset = i * ts->platdata->x_num + j;

			*min = MIN(*min, buff[offset]);
			*max = MAX(*max, buff[offset]);

			snprintf(pTmp, sizeof(pTmp), " %5d", buff[offset]);

			strlcat(pStr, pTmp, lsize);
		}

		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}

	kfree(pStr);
}

static void nvt_ts_print_buff_fdm(struct nvt_ts_data *ts, int *buff, int *min, int *max)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int offset;
	int i, j;
	int lsize = 7 * (ts->platdata->fdm_x_num + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		return;

	snprintf(pTmp, sizeof(pTmp), "  X   ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->fdm_x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "  %02d  ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->fdm_x_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->platdata->y_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Y%02d |", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->platdata->fdm_x_num; j++) {
			offset = i * ts->platdata->fdm_x_num + j;

			*min = MIN(*min, buff[offset]);
			*max = MAX(*max, buff[offset]);

			snprintf(pTmp, sizeof(pTmp), " %5d", buff[offset]);

			strlcat(pStr, pTmp, lsize);
		}

		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}

	kfree(pStr);
}

static void nvt_ts_print_gap_buff(struct nvt_ts_data *ts, int *buff, int *min, int *max)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int offset;
	int i, j;
	int lsize = 7 * (ts->platdata->x_num + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (!pStr)
		return;

	snprintf(pTmp, sizeof(pTmp), "  X   ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->x_num - 2; i++) {
		snprintf(pTmp, sizeof(pTmp), "  %02d  ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->platdata->x_num - 2; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->platdata->y_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Y%02d |", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 1; j < ts->platdata->x_num - 1; j++) {
			offset = i * ts->platdata->x_num + j;

			*min = MIN(*min, buff[offset]);
			*max = MAX(*max, buff[offset]);

			snprintf(pTmp, sizeof(pTmp), " %5d", buff[offset]);

			strlcat(pStr, pTmp, lsize);
		}

		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}

	kfree(pStr);
}

static int nvt_ts_lpwg_rawdata_read(struct nvt_ts_data *ts, int *buff)
{
	int offset;
	int x, y;
	int ret;

	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);
	msleep(100);

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: switch frequency hopping disable failed!\n", __func__);
		return ret;
	}

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: check fw reset state failed!\n", __func__);
		return ret;
	}

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: clear fw status failed!\n", __func__);
		return ret;
	}

	nvt_ts_change_mode(ts, TEST_MODE_2);

	ret = nvt_ts_check_fw_status(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: check fw status failed!\n", __func__);
		return ret;
	}

	ret = nvt_get_fw_info();
	if (ret) {
		input_err(true, &ts->client->dev, "%s: get fw info failed!\n", __func__);
		return ret;
	}

	if (nvt_ts_get_fw_pipe(ts) == 0)
		nvt_ts_read_mdata(ts, buff, ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_ts_read_mdata(ts, buff, ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			buff[offset] = (s16)buff[offset];
		}
	}

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_lpwg_noise_read(struct nvt_ts_data *ts, int *min_buff, int *max_buff)
{
	u8 buf[8] = { 0 };
	int frame_num;
	u32 x, y;
	int offset;
	int ret;
	int *rawdata_buf;

	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);
	msleep(100);

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: switch frequency hopping disable failed!\n", __func__);
		return ret;
	}

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: check fw reset state failed!\n", __func__);
		return ret;
	}

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: clear fw status failed!\n", __func__);
		return ret;
	}

	frame_num = ts->platdata->diff_test_frame / 10;
	if (frame_num <= 0)
		frame_num = 1;

	input_info(true, &ts->client->dev, "%s: frame_num %d\n", __func__, frame_num);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);

	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	ret = nvt_get_fw_info();
	if (ret) {
		input_err(true, &ts->client->dev, "%s: get fw info failed!\n", __func__);
		return ret;
	}

	rawdata_buf = kzalloc(ts->platdata->x_num * ts->platdata->y_num * 2 * sizeof(int), GFP_KERNEL);
	if (!rawdata_buf)
		return -ENOMEM;

	if (!nvt_ts_get_fw_pipe(ts))
		nvt_ts_read_mdata(ts, rawdata_buf, ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_ts_read_mdata(ts, rawdata_buf, ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			max_buff[offset] = (s8)((rawdata_buf[offset] >> 8) & 0xFF);
			min_buff[offset] = (s8)(rawdata_buf[offset] & 0xFF);
		}
	}

	kfree(rawdata_buf);

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static void nvt_enable_fdm_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x4B;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int nvt_ts_fdm_rawdata_read(struct nvt_ts_data *ts, int *buff)
{
	uint8_t buf[128] = {0};
	int x, y;
	int ret;
	int32_t iArrayIndex = 0;
	int frame_num;

	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);
	msleep(100);

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: switch frequency hopping disable failed!\n", __func__);
		return ret;
	}

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: check fw reset state failed!\n", __func__);
		return ret;
	}

	msleep(100);

	//---Enter Test Mode---
	frame_num = ts->platdata->diff_test_frame / 10;
	if (frame_num <= 0)
		frame_num = 1;

	input_info(true, &ts->client->dev, "%s: fdm_frame_num %d\n", __func__, frame_num);

	nvt_enable_fdm_noise_collect(frame_num);
	// need wait PS_Config_FDM_Noise_Test_Frame * 8.3ms
	msleep(frame_num * 83 * 2);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	for (x = 0; x < ts->platdata->fdm_x_num; x++) {
		//---set xdata index---
		nvt_set_page(ts->mmap->DOZE_GM_S1D_SCAN_RAW_ADDR
						+ ts->platdata->y_num * ts->platdata->fdm_x_num * x);

		//---read data---
		buf[0] = (ts->mmap->DOZE_GM_S1D_SCAN_RAW_ADDR
						+ ts->platdata->y_num * ts->platdata->fdm_x_num * x) & 0xFF;
		CTP_SPI_READ(ts->client, buf, ts->platdata->y_num * 2 + 1);
		for (y = 0; y < ts->platdata->y_num; y++)
			buff[y * 2 + x] = (uint16_t)(buf[y * 2 + 1] + 256 * buf[y * 2 + 2]);
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->fdm_x_num; x++) {
			iArrayIndex = y * ts->platdata->fdm_x_num + x;
			buff[iArrayIndex] = (int16_t)buff[iArrayIndex];
		}
	}

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_fdm_noise_read(struct nvt_ts_data *ts, int *min_buff, int *max_buff)
{
	uint8_t buf[128] = {0};
	int frame_num;
	u32 x, y;
	int ret;
	int *rawdata_buf;
	int32_t iArrayIndex = 0;

	nvt_write_addr(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD, 0x13);
	msleep(100);

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: switch frequency hopping disable failed!\n", __func__);
		return ret;
	}

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: check fw reset state failed!\n", __func__);
		return ret;
	}

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: clear fw status failed!\n", __func__);
		return ret;
	}

	frame_num = ts->platdata->diff_test_frame / 10;
	if (frame_num <= 0)
		frame_num = 1;

	input_info(true, &ts->client->dev, "%s: fdm_frame_num %d\n", __func__, frame_num);

	nvt_enable_fdm_noise_collect(frame_num);
	// need wait PS_Config_FDM_Noise_Test_Frame * 8.3ms
	msleep(frame_num * 83 * 2);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	rawdata_buf = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * 2 * sizeof(int), GFP_KERNEL);
	if (!rawdata_buf)
		return -ENOMEM;

	for (x = 0; x < ts->platdata->fdm_x_num; x++) {
		//---change xdata index---
		nvt_set_page(ts->mmap->DIFF_PIPE0_ADDR + ts->platdata->y_num * ts->platdata->fdm_x_num * x);

		//---read data---
		buf[0] = (ts->mmap->DIFF_PIPE0_ADDR + ts->platdata->y_num * ts->platdata->fdm_x_num * x) & 0xFF;
		CTP_SPI_READ(ts->client, buf, ts->platdata->y_num * 2 + 1);

		for (y = 0; y < ts->platdata->y_num; y++)
			rawdata_buf[y * ts->platdata->fdm_x_num + x] = (uint16_t)(buf[y * ts->platdata->fdm_x_num + 1]
				+ 256 * buf[y * ts->platdata->fdm_x_num + 2]);
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->fdm_x_num; x++) {
			iArrayIndex = y * ts->platdata->fdm_x_num + x;
			max_buff[iArrayIndex] = (int8_t)((rawdata_buf[iArrayIndex] >> 8) & 0xFF) * 4;	//scaling up
			min_buff[iArrayIndex] = (int8_t)(rawdata_buf[iArrayIndex] & 0xFF) * 4;
		}
	}

	kfree(rawdata_buf);

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_noise_read(struct nvt_ts_data *ts, int *min_buff, int *max_buff)
{
	u8 buf[8] = { 0 };
	int frame_num;
	u32 x, y;
	int offset;
	int ret;
	int *rawdata_buf;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret)
		return ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret)
		return ret;

	frame_num = ts->platdata->diff_test_frame / 10;
	if (frame_num <= 0)
		frame_num = 1;

	input_info(true, &ts->client->dev, "%s: frame_num %d\n", __func__, frame_num);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);

	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	rawdata_buf = kzalloc(ts->platdata->x_num * ts->platdata->y_num * 2 * sizeof(int), GFP_KERNEL);
	if (!rawdata_buf)
		return -ENOMEM;

	if (!nvt_ts_get_fw_pipe(ts))
		nvt_ts_read_mdata(ts, rawdata_buf, ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_ts_read_mdata(ts, rawdata_buf, ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			max_buff[offset] = (s8)((rawdata_buf[offset] >> 8) & 0xFF);
			min_buff[offset] = (s8)(rawdata_buf[offset] & 0xFF);
		}
	}

	kfree(rawdata_buf);

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_ccdata_read(struct nvt_ts_data *ts, int *buff)
{
	u32 x, y;
	int offset;
	int ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret)
		return ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret)
		return ret;

	nvt_ts_change_mode(ts, MP_MODE_CC);

	ret = nvt_ts_check_fw_status(ts);
	if (ret)
		return ret;

	if (!nvt_ts_get_fw_pipe(ts))
		nvt_ts_read_mdata(ts, buff, ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	else
		nvt_ts_read_mdata(ts, buff, ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			buff[offset] = (u16)buff[offset];
		}
	}

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_rawdata_read(struct nvt_ts_data *ts, int *buff)
{
	int offset;
	int x, y;
	int ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret)
		return ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret)
		return ret;

	nvt_ts_change_mode(ts, MP_MODE_CC);

	ret = nvt_ts_check_fw_status(ts);
	if (ret)
		return ret;

	nvt_ts_read_mdata(ts, buff, ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			buff[offset] = (s16)buff[offset];
		}
	}

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_short_read(struct nvt_ts_data *ts, int *buff)
{
	u8 buf[128] = { 0 };
	u8 *rawdata_buf;
	u32 raw_pipe_addr;
	u32 x, y;
	int offset;
	int ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret)
		return ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret)
		return ret;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	rawdata_buf = kzalloc(ts->platdata->x_num * ts->platdata->y_num * 2, GFP_KERNEL);
	if (!rawdata_buf)
		return -ENOMEM;

	if (nvt_ts_get_fw_pipe(ts) == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < ts->platdata->y_num; y++) {
		offset = y * ts->platdata->x_num * 2;
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + offset);

		buf[0] = (u8)((raw_pipe_addr + offset) & 0xFF);
		CTP_SPI_READ(ts->client, buf, ts->platdata->x_num * 2 + 1);

		memcpy(rawdata_buf + offset, buf + 1, ts->platdata->x_num * 2);
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			buff[offset] = (s16)(rawdata_buf[offset * 2]
					+ 256 * rawdata_buf[offset * 2 + 1]);
		}
	}

	kfree(rawdata_buf);

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s: Raw_Data_Short\n", __func__);

	return 0;
}

static int nvt_ts_open_read(struct nvt_ts_data *ts, int *buff)
{
	u8 buf[128] = { 0 };
	u8 *rawdata_buf;
	u32 raw_pipe_addr;
	u32 x, y;
	int offset;
	int ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	ret = nvt_ts_switch_freqhops(ts, FREQ_HOP_DISABLE);
	if (ret)
		return ret;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		return ret;

	msleep(100);

	//---Enter Test Mode---
	ret = nvt_ts_clear_fw_status(ts);
	if (ret)
		return ret;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable open test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);

	ret = nvt_ts_hand_shake_status(ts);
	if (ret)
		return ret;

	rawdata_buf = kzalloc(ts->platdata->x_num * ts->platdata->y_num * 2, GFP_KERNEL);
	if (!rawdata_buf)
		return -ENOMEM;

	if (!nvt_ts_get_fw_pipe(ts))
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < ts->platdata->y_num; y++) {
		offset = y * ts->platdata->x_num * 2;
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + offset);

		buf[0] = (uint8_t)((raw_pipe_addr + offset) & 0xFF);
		CTP_SPI_READ(ts->client, buf, ts->platdata->x_num  * 2 + 1);

		memcpy(rawdata_buf + offset, buf + 1, ts->platdata->x_num * 2);
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			buff[offset] = (s16)((rawdata_buf[(offset) * 2]
					+ 256 * rawdata_buf[(offset) * 2 + 1]));
		}
	}

	kfree(rawdata_buf);

	//---Leave Test Mode---
	nvt_ts_change_mode(ts, NORMAL_MODE);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;
}

static int nvt_ts_open_test(struct nvt_ts_data *ts)
{
	int *raw_buff;
	int ret = 0;
	int min, max;

	min = 0x7FFFFFFF;
	max = 0x80000000;

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		return -ENOMEM;

	ret = nvt_ts_open_read(ts, raw_buff);
	if (ret)
		goto out;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (min < (int)ts->platdata->open_test_spec[0])
		ret = -EPERM;

	if (max > (int)ts->platdata->open_test_spec[1])
		ret = -EPERM;

	input_raw_info(true, &ts->client->dev, "%s: min(%d,%d), max(%d,%d)\n",
		__func__, min, ts->platdata->open_test_spec[0],
		max, ts->platdata->open_test_spec[1]);
out:
	//---Reset IC---
	nvt_bootloader_reset();

	kfree(raw_buff);

	return ret;
}

static int nvt_ts_short_test(struct nvt_ts_data *ts)
{
	int *raw_buff;
	int ret = 0;
	int min, max;

	min = 0x7FFFFFFF;
	max = 0x80000000;

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		return -ENOMEM;

	ret = nvt_ts_short_read(ts, raw_buff);
	if (ret)
		goto out;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (min < (int)ts->platdata->short_test_spec[0])
		ret = -EPERM;

	if (max > (int)ts->platdata->short_test_spec[1])
		ret = -EPERM;

	input_raw_info(true, &ts->client->dev, "%s: min(%d,%d), max(%d,%d)\n",
		__func__, min, ts->platdata->short_test_spec[0],
		max, ts->platdata->short_test_spec[1]);
out:
	//---Reset IC---
	nvt_bootloader_reset();

	kfree(raw_buff);

	return ret;
}

#if SHOW_NOT_SUPPORT_CMD
static int nvt_ts_nt36523_sram_test(struct nvt_ts_data *ts)
{
	u8 test_pattern[4] = {0xFF, 0x00, 0x55, 0xAA};
	u8 buf[10];
	int ret = TEST_RESULT_PASS;
	int i;

	//---SRAM TEST (ICM) ---

	// SW Reset & Idle
	nvt_ts_sw_reset_idle(ts);

	//---set xdata index to SRAM Test Registers---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->MBT_MUX_CTL0 >> 16) & 0xFF;
	buf[2] = (ts->mmap->MBT_MUX_CTL0 >> 8) & 0xFF;
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 3);

	//---set control registers---
	buf[0] = (u8)(ts->mmap->MBT_MUX_CTL0 & 0xFF);
	buf[1] = 0x18;
	buf[2] = 0x5B;
	buf[3] = 0x52;
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 4);

	//---set mode registers---
	buf[0] = (u8)(ts->mmap->MBT_MODE & 0xFF);
	buf[1] = 0x00;
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

	for (i = 0; i < sizeof(test_pattern); i++) {
		//---set test pattern---
		buf[0] = (u8)(ts->mmap->MBT_DB & 0xFF);
		buf[1] = test_pattern[i];
		nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

		//---set operation as WRITE test---
		buf[0] = (u8)(ts->mmap->MBT_OP & 0xFF);
		buf[1] = 0xE1;
		nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

		//---enable test---
		buf[0] = (u8)(ts->mmap->MBT_EN & 0xFF);
		buf[1] = 0x01;
		nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = (u8)(ts->mmap->MBT_STATUS & 0xFF);
		buf[1] = 0;
		nvt_ts_i2c_read(ts, I2C_FW_Address, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: ICM WRITE fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}

		//---set operation as READ test---
		buf[0] = (u8)(ts->mmap->MBT_OP & 0xFF);
		buf[1] = 0xA1;
		nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

		//---enable test---
		buf[0] = (u8)(ts->mmap->MBT_EN & 0xFF);
		buf[1] = 0x01;
		nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = (u8)(ts->mmap->MBT_STATUS & 0xFF);
		buf[1] = 0x00;
		nvt_ts_i2c_read(ts, I2C_FW_Address, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: ICM READ fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}
	}


	//---SRAM TEST (ICS) ---

	// SW Reset & Idle
	nvt_ts_sw_reset_idle(ts);

	//---set ICS DMA RX speed to 1X---
	buf[0] = 0x00;	//dummy
	buf[1] = 0x00;
	nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->RX_DMA_2X_EN, buf, 2);

	//---set ICM DMA TX speed to 1X---
	nvt_ts_set_page(ts, I2C_FW_Address, ts->mmap->TX_DMA_2X_EN);
	buf[0] = (u8)(ts->mmap->TX_DMA_2X_EN & 0xFF);
	buf[1] = 0x00;
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 2);

	msleep(10);

	//---set control registers---
	buf[0] = 0x00;	//dummy
	buf[1] = 0x18;
	buf[2] = 0x5B;
	buf[3] = 0x52;
	nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_MUX_CTL0, buf, 4);

	//---set mode registers---
	buf[0] = 0x00;	//dummy
	buf[1] = 0x00;
	nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_MODE, buf, 2);

	for (i = 0; i < sizeof(test_pattern); i++) {
		//---set test pattern---
		buf[0] = 0x00;	//dummy
		buf[1] = test_pattern[i];
		nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_DB, buf, 2);

		//---set operation as WRITE test---
		buf[0] = 0x00;	//dummy
		buf[1] = 0xE1;
		nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_OP, buf, 2);

		//---enable test---
		buf[0] = 0x00;	//dummy
		buf[1] = 0x01;
		nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_EN, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = 0x00;	//dummy
		buf[1] = 0x00;
		nvt_ts_nt36523_ics_i2c_read(ts, ts->mmap->MBT_STATUS, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: ICS WRITE fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}

		//---set operation as READ test---
		buf[0] = 0x00;	//dummy
		buf[1] = 0xA1;
		nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_OP, buf, 2);

		//---enable test---
		buf[0] = 0x00;	//dummy
		buf[1] = 0x01;
		nvt_ts_nt36523_ics_i2c_write(ts, ts->mmap->MBT_EN, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = 0x00;	//dummy
		buf[1] = 0x00;
		nvt_ts_nt36523_ics_i2c_read(ts, ts->mmap->MBT_STATUS, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: ICS READ fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}
	}

	if (ret == TEST_RESULT_PASS)
		input_info(true, &ts->client->dev, "%s: pass.\n", __func__);

	//---Reset IC---
	//nvt_bootloader_reset();

	return ret;
}
#endif	// end of #if SHOW_NOT_SUPPORT_CMD

static int nvt_ts_sram_test(struct nvt_ts_data *ts)
{
	u8 test_pattern[4] = {0xFF, 0x00, 0x55, 0xAA};
	u8 buf[10];
	int ret = TEST_RESULT_PASS;
	int i;

	nvt_sw_reset_idle();
	nvt_set_page(ts->mmap->MBT_MUX_CTL0);

	//---set control registers---
	buf[0] = (u8)(ts->mmap->MBT_MUX_CTL0 & 0xFF);
	buf[1] = 0x18;
	buf[2] = 0x5B;
	buf[3] = 0x52;
	CTP_SPI_WRITE(ts->client, buf, 4);

	//---set mode registers---
	buf[0] = (u8)(ts->mmap->MBT_MODE & 0xFF);
	buf[1] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 2);

	for (i = 0; i < sizeof(test_pattern); i++) {
		//---set test pattern---
		buf[0] = (u8)(ts->mmap->MBT_DB & 0xFF);
		buf[1] = test_pattern[i];
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---set operation as WRITE test---
		buf[0] = (u8)(ts->mmap->MBT_OP & 0xFF);
		buf[1] = 0xE1;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---enable test---
		buf[0] = (u8)(ts->mmap->MBT_EN & 0xFF);
		buf[1] = 0x01;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = (u8)(ts->mmap->MBT_STATUS & 0xFF);
		buf[1] = 0;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: IC WRITE fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}

		//---set operation as READ test---
		buf[0] = (u8)(ts->mmap->MBT_OP & 0xFF);
		buf[1] = 0xA1;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---enable test---
		buf[0] = (u8)(ts->mmap->MBT_EN & 0xFF);
		buf[1] = 0x01;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(100);

		//---get test result---
		buf[0] = (u8)(ts->mmap->MBT_STATUS & 0xFF);
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] != 0x02) {
			input_info(true, &ts->client->dev, "%s: IC READ fail at pattern=0x%02X, MBT_STATUS=0x%02X\n",
				__func__, test_pattern[i], buf[1]);
			ret = TEST_RESULT_FAIL;
		}
	}

	if (ret == TEST_RESULT_PASS)
		input_info(true, &ts->client->dev, "%s: pass.\n", __func__);

	return ret;
}

u16 nvt_ts_mode_read(struct nvt_ts_data *ts)
{
	u8 buf[4] = {0};
	int mode_masked;

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return -EIO;
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FUNCT_STATE);

	//---read cmd status---
	buf[0] = EVENT_MAP_FUNCT_STATE;
	CTP_SPI_READ(ts->client, buf, 4);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	mode_masked =  (buf[2] << 8 | buf[1]) & FUNCT_ALL_MASK;

	input_info(true, &ts->client->dev, "%s: 0x%02X%02X, masked:0x%04X\n",
					__func__, buf[2], buf[1], mode_masked);

	ts->noise_mode = (mode_masked & NOISE_MASK) ? 1 : 0;
	ts->prox_in_aot = ((buf[3] << 16) & PROX_IN_AOT_MASK) ? 1 : 0;

	return mode_masked;
}

int nvt_ts_mode_switch(struct nvt_ts_data *ts, u8 cmd, bool print_log)
{
	int i, retry = 5;
	u8 buf[3] = { 0 };

	input_info(true, &ts->client->dev, "%s : cmd(0x%X)\n", __func__, cmd);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	for (i = 0; i < retry; i++) {
		//---set cmd---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = cmd;
		CTP_SPI_WRITE(ts->client, buf, 2);

		if (ts->shutdown_called) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			return -EIO;
		}

		if (!nvt_ts_lcd_power_check()) {
			input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
			return -EIO;
		}

		usleep_range(15000, 16000);

		//---read cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(i == retry)) {
		input_err(true, &ts->client->dev,"failed to switch 0x%02X mode - 0x%02X\n", cmd, buf[1]);
		return -EIO;
	}

	if (print_log) {
		u16 read_ic_mode;
		msleep(20);
		read_ic_mode = nvt_ts_mode_read(ts);
		input_info(true, &ts->client->dev,"%s : cmd(0x%X) sec_function : 0x%02X & nvt_ts_mode_read : 0x%02X\n",
					__func__, cmd, ts->sec_function, read_ic_mode);
	}
	return 0;
}
#if PROXIMITY_FUNCTION
int nvt_ts_mode_switch_extended(struct nvt_ts_data *ts, u8 *cmd, u8 len, bool print_log)
{
	int i, retry = 5;
	u8 buf[4] = { 0 };

	input_info(true, &ts->client->dev, "%s : cmd(0x%X/0x%X/0x%X)\n",
				__func__, cmd[0], cmd[1], cmd[2]);

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = cmd[0];
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | cmd[0]);
	for (i = 0; i < retry; i++) {
		//---set cmd---
		CTP_SPI_WRITE(ts->client, cmd, len);

		if (ts->shutdown_called) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			return -EIO;
		}
		usleep_range(15000, 16000);

		//---read cmd status---
		CTP_SPI_READ(ts->client, buf, 2);
		if (buf[1] == 0x00)
			break;
		else
			input_err(true, &ts->client->dev, "%s, retry:%d, buf[1]:0x%x\n", __func__, i, buf[1]);
	}

	if (unlikely(i == retry)) {
		input_err(true, &ts->client->dev, "failed to switch mode - buf:0x%02X 0x%02X, cmd:0x%02X 0x%02X\n",
				buf[0], buf[1], cmd[0], cmd[1]);
		return -EIO;
	}

	if (print_log) {
		u16 read_ic_mode;
		msleep(20);
		read_ic_mode = nvt_ts_mode_read(ts);
		input_info(true, &ts->client->dev,"%s : sec_function : 0x%02X & nvt_ts_mode_read : 0x%02X\n",
				__func__, ts->sec_function, read_ic_mode);
	}

	return 0;
}
#endif

int nvt_ts_mode_restore(struct nvt_ts_data *ts)
{
	u8 cmd;
	int i;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: sec_function:0x%X\n",
				__func__, ts->sec_function);

	for (i = FUNCT_MIN; i < FUNCT_MAX; i++) {
		if ((ts->sec_function >> i) & 0x01) {
			switch(i) {
			case GLOVE:
				cmd = GLOVE_ENTER;
				break;
			case CHARGER:
				cmd = CHARGER_PLUG_AC;
				break;
			case SPAY_SWIPE:
				cmd = SPAY_SWIPE_ENTER;
				break;
			case DOUBLE_CLICK:
				cmd = DOUBLE_CLICK_ENTER;
				break;
			case HIGH_SENSITIVITY:
				cmd = HIGH_SENSITIVITY_ENTER;
				break;
			case SENSITIVITY:
				cmd = SENSITIVITY_ENTER;
				break;
			default:
				continue;
			}

#if SHOW_NOT_SUPPORT_CMD
			if (ts->touchable_area) {
				ret = nvt_ts_set_touchable_area(ts);
				if (ret < 0)
					cmd = BLOCK_AREA_LEAVE;
			}
#endif
			ret = nvt_ts_mode_switch(ts, cmd, false);
			if (ret)
				input_info(true, &ts->client->dev, "%s: failed to restore %X\n", __func__, cmd);
		}
	}

	return ret;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		ts->fw_index = NVT_TSP_FW_IDX_BIN;
		ret = nvt_ts_fw_update_from_bin(ts);
		break;
	case UMS:
		ts->fw_index = NVT_TSP_FW_IDX_UMS;
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		ret = nvt_ts_fw_update_from_external(ts, TSP_EXTERNAL_FW_SIGNED);
#else
		ret = nvt_ts_fw_update_from_external(ts, TSP_EXTERNAL_FW);
#endif
		if (ret) {
			input_err(true, &ts->client->dev, "%s: fail fw update & set fw index bin\n", __func__);
			ts->fw_index = NVT_TSP_FW_IDX_BIN;
		}
		break;
	default:
		input_err(true, &ts->client->dev, "%s: Not support command[%d]\n",
			__func__, sec->cmd_param[0]);
		ret = -EINVAL;
		break;
	}

	if (ret)
		goto out;

	nvt_get_fw_info();

	nvt_ts_mode_restore(ts);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "NO%02X%02X%02X%02X",
		ts->fw_ver_bin[0], ts->fw_ver_bin[1], ts->fw_ver_bin[2], ts->fw_ver_bin[3]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char data[4] = { 0 };
	char model[16] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out_power_off;
	}

	/* ic_name, project_id, panel, fw info */
	ret = nvt_get_fw_info();
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: nvt_ts_get_fw_info fail!\n", __func__);
		goto out;
	}
	data[0] = ts->fw_ver_ic[0];
	data[1] = ts->fw_ver_ic[1];
	data[2] = ts->fw_ver_ic[2];
	data[3] = ts->fw_ver_ic[3];

	snprintf(buff, sizeof(buff), "NO%02X%02X%02X%02X", data[0], data[1], data[2], data[3]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(model, sizeof(model), "NO%02X%02X", data[0], data[1]);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	nvt_bootloader_reset();
out_power_off:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->platdata->x_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true,  &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ts->platdata->y_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true,  &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "NOVATEK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->fw_ver_ic[0] == 0x02) {
		snprintf(buff, sizeof(buff), "%s", "NT36672C");
	} else if (ts->fw_ver_ic[0] == 0x36) {
		snprintf(buff, sizeof(buff), "%s", "NT36525B");
	} else if (ts->fw_ver_ic[0] == 0x37) {
		snprintf(buff, sizeof(buff), "%s", "NT36525C");
	} else if (ts->fw_ver_ic[0] == 0x76) {
		snprintf(buff, sizeof(buff), "%s", "NT36675");
	} else {
		snprintf(buff, sizeof(buff), "%s", "NTxxxxx");
		input_err(true, &ts->client->dev, "%s: cheack fw version!!(0x%X)\n", __func__, ts->fw_ver_ic[0]);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#if SHOW_NOT_SUPPORT_CMD
static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 csum_result[BLOCK_64KB_NUM * 4 + 1] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	// get firmware data checksum in flash ic
	if (nvt_get_checksum(ts, csum_result, sizeof(csum_result))) {
		input_err(true, &ts->client->dev, "%s: nvt_ts_get_checksum fail!\n", __func__);
		goto err;
	}

	nvt_bootloader_reset();

	snprintf(buff, sizeof(buff), "%s", csum_result);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

err:
	nvt_bootloader_reset();
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif
static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "60");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	ret = nvt_ts_open_test(ts);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", ret ? "NG" : "OK");

	sec->cmd_state =  ret ? SEC_CMD_STATUS_FAIL : SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

/*
 *	cmd_param
 *		[0], 0 disable high sensitivity mode
 *		     1 enable high sensitivity mode
 */
static void nvt_ts_high_sensitivity_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < HIGH_SENSITIVITY_DISABLE || sec->cmd_param[0] > HIGH_SENSITIVITY_ENABLE) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? HIGH_SENSITIVITY_ENTER : HIGH_SENSITIVITY_LEAVE;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	if (sec->cmd_param[0])
		ts->sec_function |= HIGH_SENSITIVITY_MASK;
	else
		ts->sec_function &= ~HIGH_SENSITIVITY_MASK;

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

/*
 *	cmd_param
 *		[0], 0 disable glove mode
 *		     1 enable glove mode
 */
static void nvt_ts_glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? GLOVE_ENTER : GLOVE_LEAVE;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	if (sec->cmd_param[0])
		ts->sec_function |= GLOVE_MASK;
	else
		ts->sec_function &= ~GLOVE_MASK;

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	if (ts->platdata->enable_glove_mode) {
		nvt_ts_glove_mode(sec);
	} else {
		nvt_ts_high_sensitivity_mode(sec);
	}
}

#ifdef PROXIMITY_FUNCTION
int set_ear_detect(struct nvt_ts_data *ts, int mode, bool print_log)
{
	int ret;
	u8 reg;
	u8 buf[3];
	u8 subcmd;

	input_info(true, &ts->client->dev, "%s: set ear mode(%d)\n", __func__, mode);

	reg = mode ? PROXIMITY_ENTER : PROXIMITY_LEAVE;

	if (mode == 1)
		subcmd = PROXIMITY_INSENSITIVITY;
	else if (mode == 3)
		subcmd = PROXIMITY_NORMAL;
	else
		subcmd = 0; //don't care this value  when the mode == PROXIMITY_LEAVE


	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = reg;
	buf[2] = subcmd;
	ret = nvt_ts_mode_switch_extended(ts, buf, 3, print_log);
	if (ret) {
		input_err(true, &ts->client->dev, "%s failed to switch ed\n", __func__);
	}

	return ret;
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!ts->platdata->support_ear_detect) {
		input_err(true, &ts->client->dev, "%s: Not support EarDetection!\n", __func__);
		goto out;
	}

	if (!(sec->cmd_param[0] == 0 || sec->cmd_param[0] == 1 || sec->cmd_param[0] == 3)) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n", __func__, sec->cmd_param[0]);
		goto out;
	} else {
		ts->ear_detect_mode = sec->cmd_param[0];
	}

	if (ts->power_status == POWER_OFF_STATUS || ts->power_status == LP_MODE_EXIT) {
		ts->ed_reset_flag = true;
		input_err(true, &ts->client->dev, "%s: not handle cmd, power state(%d)!\n",
					__func__, ts->power_status);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	ret = set_ear_detect(ts, ts->ear_detect_mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s,%d: %s\n", __func__, sec->cmd_param[0], buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s,%d: %s\n", __func__, sec->cmd_param[0], buff);
}


static void prox_lp_scan_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = 0;
	int ret = 0;
	u8 buf[3] = {0};
	int retry = 10;

	sec_cmd_set_default_result(sec);

	if (!ts->platdata->prox_lp_scan_enabled) {
		input_err(true, &ts->client->dev, "%s: Not support LPSCAN!\n", __func__);
		goto out;
	}

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: Not LP_MODE_STATUS!\n", __func__);
		goto out;
	}

	if (!ts->ear_detect_mode) {
		input_err(true, &ts->client->dev, "%s: ED mode off skip!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? PROX_SLEEP_OUT : PROX_SLEEP_IN;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	while(retry) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = EXTENDED_CUSTOMIZED_CMD;
		buf[2] = mode;

		ret = nvt_ts_mode_switch_extended(ts, buf, 3, true);
		if (ret) {
			input_err(true, &ts->client->dev, "%s, retry:%d \n", __func__, retry);
			retry--;
		} else {
			break;
		}
	}
	if (ret) {
		input_err(true, &ts->client->dev, "%s : failed to switch %s mode\n",
					__func__, (mode == PROX_SLEEP_IN) ? "SLEEP_IN" : "SLEEP_OUT");
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s : switch to %s mode OK\n",
					__func__, (mode == PROX_SLEEP_IN) ? "SLEEP_IN" : "SLEEP_OUT");

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s : failed to switch %d mode\n", __func__, mode);
}

#endif

/*
 *	cmd_param
 *		[0], 0 normal debounce
 *		     1 lower debounce
 */
static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int reti;
	u8 mode;
	u8 buf[4];

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < DEBOUNCE_NORMAL || sec->cmd_param[0] > DEBOUNCE_LOWER) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0];
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: use %s touch debounce, cmd_param=%d\n", 
		__func__, sec->cmd_param[0] ? "lower" : "normal", sec->cmd_param[0]);
	
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = EXTENDED_CUSTOMIZED_CMD;
	buf[2] = SET_TOUCH_DEBOUNCE;
	buf[3] = (u8)sec->cmd_param[0];
	reti = nvt_ts_mode_switch_extended(ts, buf, 4, true);
	if (reti) {
		input_err(true, &ts->client->dev, "%s failed to switch sip mode - 0x%02X\n", __func__, buf[3]);
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

/*
*	0 disable game mode
*	1 enable game mode
*/
static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = 0;
	u8 buf[4] = { 0 };

	sec_cmd_set_default_result(sec);
	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < GAME_MODE_DISABLE || sec->cmd_param[0] > GAME_MODE_ENABLE) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n", __func__, sec->cmd_param[0]);
		goto out;
	}

	mode = sec->cmd_param[0];

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n", __func__);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: %s touch game mode, cmd_param=%d\n",
			__func__, sec->cmd_param[0] ? "enable" : "disable", sec->cmd_param[0]);

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = EXTENDED_CUSTOMIZED_CMD;
	buf[2] = SET_GAME_MODE;
	buf[3] = (u8)sec->cmd_param[0];

	if (nvt_ts_mode_switch_extended(ts, buf, 4, true)) {
		input_err(true, &ts->client->dev, "%s failed to switch game mode\n", __func__);
		mutex_unlock(&ts->lock);
		goto out;
	}
	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (!ts->platdata->enable_settings_aot) {
		input_err(true, &ts->client->dev, "%s: Not support AOT(%d)!\n", __func__, sec->cmd_param[0]);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	}

	if (sec->cmd_param[0]) {
		ts->sec_function |= DOUBLE_CLICK_MASK;
		ts->lowpower_mode |= LP_AOT;
		mode = DOUBLE_CLICK_ENTER;
	} else {
		ts->sec_function &= ~DOUBLE_CLICK_MASK;
		ts->lowpower_mode &= ~LP_AOT;
		mode = DOUBLE_CLICK_LEAVE;
	}

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->lowpower_mode);

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev,"%s: another task is running\n",
			__func__);
		goto out;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	}

	if (sec->cmd_param[0])
		ts->sec_function |= EDGE_PIXEL_MASK;
	else
		ts->sec_function &= ~EDGE_PIXEL_MASK;

	mode = sec->cmd_param[0] ? EDGE_AREA_ENTER : EDGE_AREA_LEAVE;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#if SEC_LPWG_DUMP
/*
 *	cmd_param
 *		[0], 0 disable
 *		     1 enable
 */
static void set_lpwg_dump(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int reti;
	u8 buf[4] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < LPWG_DUMP_DISABLE || sec->cmd_param[0] > LPWG_DUMP_ENABLE) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: %s LPWG dump, cmd_param=%d\n",
		__func__, sec->cmd_param[0] ? "ENABLE" : "DISABLE", sec->cmd_param[0]);

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = EXTENDED_CUSTOMIZED_CMD;
	buf[2] = SET_LPWG_DUMP;
	buf[3] = (u8)sec->cmd_param[0];
	reti = nvt_ts_mode_switch_extended(ts, buf, 4, false);
	if (reti) {
		input_err(true, &ts->client->dev, "%s failed to switch lpwg dump - 0x%02X\n", __func__, buf[3]);
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif
static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	}

	if (sec->cmd_param[0]) {
		ts->sec_function |= SPAY_SWIPE_MASK;
		ts->lowpower_mode |= LP_SPAY;
		mode = SPAY_SWIPE_ENTER;
	} else {
		ts->sec_function &= ~SPAY_SWIPE_MASK;
		ts->lowpower_mode &= ~LP_SPAY;
		mode = SPAY_SWIPE_LEAVE;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#if SHOW_NOT_SUPPORT_CMD
static void set_touchable_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? BLOCK_AREA_ENTER: BLOCK_AREA_LEAVE;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	if (mode == BLOCK_AREA_ENTER) {
		ret = nvt_ts_set_touchable_area(ts);
		if (ret < 0)
			goto err;
	} else if (mode == BLOCK_AREA_LEAVE) {
		ret = nvt_ts_disable_block_mode(ts);
		if (ret < 0)
			goto err;
	}

	input_info(true, &ts->client->dev, "%s: %02X(%d)\n", __func__, mode, ts->untouchable_area);

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret)
		goto err;

	mutex_unlock(&ts->lock);

	ts->touchable_area = !!sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	mutex_unlock(&ts->lock);
out:
	ts->touchable_area = false;

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_untouchable_area_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (!sec->cmd_param[0] && !sec->cmd_param[1] && !sec->cmd_param[2] && !sec->cmd_param[3])
		mode = BLOCK_AREA_LEAVE;
	else
		mode = BLOCK_AREA_ENTER;

	ts->untouchable_area = (mode == BLOCK_AREA_ENTER ? true : false);

	if (!sec->cmd_param[1])
		ts->landscape_deadzone[0] = sec->cmd_param[3];

	if (sec->cmd_param[3] == DEFAULT_MAX_HEIGHT)
		ts->landscape_deadzone[1] = sec->cmd_param[3] - sec->cmd_param[1];

	input_info(true, &ts->client->dev, "%s: %d,%d %d,%d %d,%d\n", __func__,
		sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], sec->cmd_param[3],
		ts->landscape_deadzone[0], ts->landscape_deadzone[1]);

	if (ts->touchable_area) {
		input_err(true, &ts->client->dev, "%s: set_touchable_area is enabled\n", __func__);
		goto out_for_touchable;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	if (mode == BLOCK_AREA_ENTER) {
		ret = nvt_ts_set_untouchable_area(ts);
		if (ret < 0)
			goto err;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret)
		goto err;

	mutex_unlock(&ts->lock);

	ts->untouchable_area = (mode == BLOCK_AREA_ENTER ? true : false);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	mutex_unlock(&ts->lock);
out:
	ts->untouchable_area = false;
out_for_touchable:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

/*
 *	cmd_param
 *		[1], 0 disable
 *		     1 enable only left side
 *		     2 enable only right side
 *
 *		[2], upper Y coordinate
 *
 *		[3], lower Y coordinate
 */
static void nvt_ts_set_grip_exception_zone(struct sec_cmd_data *sec)
{
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	u8 buf[8];

	switch (sec->cmd_param[1]) {
		case 0:	//disable
			input_info(true, &ts->client->dev, "%s: disable\n", __func__);
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = EXTENDED_CUSTOMIZED_CMD;
			buf[2] = SET_GRIP_EXECPTION_ZONE;
			buf[3] = (u8)sec->cmd_param[1];
			buf[4] = 0;
			buf[5] = 0;
			buf[6] = 0;
			buf[7] = 0;
			nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 8);
			goto out;
		case 1:	//enable left
			input_info(true, &ts->client->dev, "%s: enable left side\n", __func__);
			break;
		case 2: //enable right
			input_info(true, &ts->client->dev, "%s: enable right side\n", __func__);
			break;
		default:
			input_err(true, &ts->client->dev, "%s: not support parameter, cmd_param[1]=0x02X\n", __func__, sec->cmd_param[1]);
			goto err;
	}

	//enable left or right
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = EXTENDED_CUSTOMIZED_CMD;
	buf[2] = SET_GRIP_EXECPTION_ZONE;
	buf[3] = (u8)sec->cmd_param[1];
	buf[4] = (u8)(sec->cmd_param[2] & 0xFF);
	buf[5] = (u8)((sec->cmd_param[2] >> 8) & 0xFF);
	buf[6] = (u8)(sec->cmd_param[3] & 0xFF);
	buf[7] = (u8)((sec->cmd_param[3] >> 8) & 0xFF);
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 8);

out:
	msleep(20);
err:
	return;
}

/*
 *	cmd_param
 *		[1], grip zone (both long side)
 *
 *		[2], upper reject zone (both long side)
 *
 *		[3], lower reject zone (both long side)
 *
 *		[4], reject zone boundary of Y (divide upper and lower)
 */
static void nvt_ts_set_grip_portrait_mode(struct sec_cmd_data *sec)
{
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	u8 buf[8];

	input_info(true, &ts->client->dev, "%s: set portrait mode parameters\n", __func__);

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = EXTENDED_CUSTOMIZED_CMD;
	buf[2] = SET_GRIP_PORTRAIT_MODE;
	buf[3] = (u8)sec->cmd_param[1];
	buf[4] = (u8)sec->cmd_param[2];
	buf[5] = (u8)sec->cmd_param[3];
	buf[6] = (u8)(sec->cmd_param[4] & 0xFF);
	buf[7] = (u8)((sec->cmd_param[4] >> 8) & 0xFF);
	nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 8);

	msleep(20);
}

/*
 *	cmd_param
 *		[1], 0 use previous portrait grip and reject settings
 *		     1 enable landscape mode
 *
 *		[2], reject zone (both long side)
 *
 *		[3], grip zone (both long side)
 *
 *		[4], reject zone (top short side)
 *
 *		[5], reject zone (bottom short side)
 *
 *		[6], grip zone (top short side)
 *
 *		[7], grip zone (bottom short side)
 */
static void nvt_ts_set_grip_landscape_mode(struct sec_cmd_data *sec)
{
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	u8 buf[12];

	switch (sec->cmd_param[1]) {
		case 0: //use previous portrait setting
			input_info(true, &ts->client->dev, "%s: use previous portrait settings\n", __func__);
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = EXTENDED_CUSTOMIZED_CMD;
			buf[2] = SET_GRIP_LANDSCAPE_MODE;
			buf[3] = (u8)sec->cmd_param[1];
			nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 4);
			goto out;
		case 1: //enable landscape mode
			input_info(true, &ts->client->dev, "%s: set landscape mode parameters\n", __func__);
			buf[0] = EVENT_MAP_HOST_CMD;
			buf[1] = EXTENDED_CUSTOMIZED_CMD;
			buf[2] = SET_GRIP_LANDSCAPE_MODE;
			buf[3] = (u8)sec->cmd_param[1];
			buf[4] = (u8)sec->cmd_param[2];
			buf[5] = (u8)sec->cmd_param[3];
			buf[6] = (u8)sec->cmd_param[4];
			buf[7] = (u8)sec->cmd_param[5];
			buf[8] = (u8)sec->cmd_param[6];
			buf[9] = (u8)sec->cmd_param[7];
			nvt_ts_i2c_write(ts, I2C_FW_Address, buf, 10);
			goto out;
		default:
			input_err(true, &ts->client->dev, "%s: not support parameter, cmd_param[1]=0x02X\n", __func__, sec->cmd_param[1]);
			goto err;
	}

out:
	msleep(20);
err:
	return;
}

/*
 *	cmd_param
 *		[0], 0 edge/grip execption zone
 *		     1 portrait mode
 *		     2 landscape mode
 */
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	// print parameters (debug use)
	input_info(true, &ts->client->dev, "%s: 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X\n",
		__func__, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], sec->cmd_param[3],
		sec->cmd_param[4], sec->cmd_param[5], sec->cmd_param[6], sec->cmd_param[7]);

	switch (sec->cmd_param[0]) {
		case 0:
			input_info(true, &ts->client->dev, "%s: EDGE_GRIP_EXCEPTION_ZONE\n", __func__);
			nvt_ts_set_grip_exception_zone(sec);
			break;
		case 1:
			input_info(true, &ts->client->dev, "%s: PORTRAIT_MODE\n", __func__);
			nvt_ts_set_grip_portrait_mode(sec);
			break;
		case 2:
			input_info(true, &ts->client->dev, "%s: LANDSCAPE_MODE\n", __func__);
			nvt_ts_set_grip_landscape_mode(sec);
			break;
		default:
			input_err(true, &ts->client->dev, "%s: not support mode, cmd_param[0]=0x02X\n", __func__, sec->cmd_param[0]);
			goto err;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void lcd_orientation(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 && sec->cmd_param[0] > 3) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		switch (sec->cmd_param[0]) {
		case ORIENTATION_0:
			mode = EDGE_REJ_VERTICLE_MODE;
			input_info(true, &ts->client->dev, "%s: Get 0 degree LCD orientation.\n", __func__);
			break;
		case ORIENTATION_90:
			mode = EDGE_REJ_RIGHT_UP_MODE;
			input_info(true, &ts->client->dev, "%s: Get 90 degree LCD orientation.\n", __func__);
			break;
		case ORIENTATION_180:
			mode = EDGE_REJ_VERTICLE_MODE;
			input_info(true, &ts->client->dev, "%s: Get 180 degree LCD orientation.\n", __func__);
			break;
		case ORIENTATION_270:
			mode = EDGE_REJ_LEFT_UP_MODE;
			input_info(true, &ts->client->dev, "%s: Get 270 degree LCD orientation.\n", __func__);
			break;
		default:
			input_err(true, &ts->client->dev, "%s: LCD orientation value error.\n", __func__);
			goto out;
			break;
		}
	}

	input_info(true, &ts->client->dev, "%s: %d,%02X\n", __func__, sec->cmd_param[0], mode);

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);
	if (ret) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif	// end of #if SHOW_NOT_SUPPORT_CMD

static void run_self_lpwg_rawdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		goto err_alloc_mem;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_rawdata_read(ts, raw_buff))
		goto err;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_RAW_DATA");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_RAW_DATA");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_lpwg_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_buff;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		goto err_alloc_mem;

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_buff);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_rawdata_read(ts, raw_buff))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_buff[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_buff);
err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_lpwg_noise_max_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_noise_read(ts, raw_min, raw_max))
		goto err;

	nvt_ts_print_buff(ts, raw_max, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_NOISE_MAX");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_NOISE_MAX");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_lpwg_noise_max_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_min, *raw_max;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_min);
		kfree(raw_max);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_noise_read(ts, raw_min, raw_max))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_max[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_lpwg_noise_min_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_noise_read(ts, raw_min, raw_max))
		goto err;

	nvt_ts_print_buff(ts, raw_min, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_NOISE_MIN");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "LP_NOISE_MIN");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_lpwg_noise_min_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_min, *raw_max;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_min);
		kfree(raw_max);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_lpwg_noise_read(ts, raw_min, raw_max))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_min[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_fdm_rawdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		goto err_alloc_mem;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_rawdata_read(ts, raw_buff))
		goto err;

	nvt_ts_print_buff_fdm(ts, raw_buff, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_RAW_DATA");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

err_alloc_mem:
	mutex_unlock(&ts->lock);

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_RAW_DATA");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_fdm_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_buff;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff)
		goto err_alloc_mem;

	buff = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_buff);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_rawdata_read(ts, raw_buff))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->fdm_x_num; x++) {
			offset = y * ts->platdata->fdm_x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_buff[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_buff);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_fdm_noise_max_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_noise_read(ts, raw_min, raw_max))
		goto err;

	nvt_ts_print_buff_fdm(ts, raw_max, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_NOISE_MAX");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_NOISE_MAX");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_fdm_noise_max_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_min, *raw_max;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	buff = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_min);
		kfree(raw_max);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_noise_read(ts, raw_min, raw_max))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->fdm_x_num; x++) {
			offset = y * ts->platdata->fdm_x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_max[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_fdm_noise_min_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_noise_read(ts, raw_min, raw_max))
		goto err;

	nvt_ts_print_buff_fdm(ts, raw_min, &min, &max);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_NOISE_MIN");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FDM_NOISE_MIN");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_fdm_noise_min_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *buff;
	int *raw_min, *raw_max;

	char tmp[SEC_CMD_STR_LEN] = { 0 };
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status != LP_MODE_STATUS) {
		input_err(true, &ts->client->dev, "%s: invalid POWER_STATUS : not LPM!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	buff = kzalloc(ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		kfree(raw_min);
		kfree(raw_max);
		goto err_alloc_mem;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_fdm_noise_read(ts, raw_min, raw_max))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->fdm_x_num; x++) {
			offset = y * ts->platdata->fdm_x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_min[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->fdm_x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(buff);
	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_open_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char tmp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int *raw_buff;
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto err;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_open_read(ts, raw_buff)) {
		kfree(buff);
		goto err;
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_buff[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff,
			strnlen(buff, ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(raw_buff);
	kfree(buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	kfree(raw_buff);

out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_open_raw_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_open_read(ts, raw_buff))
		goto err;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OPEN_RAW");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OPEN_RAW");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_open_uni_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char tmp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int *raw_buff;
	int x, y;
	int offset, data;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto err;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_open_read(ts, raw_buff)) {
		kfree(buff);
		goto err;
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 1; x < ts->platdata->x_num / 2; x++) {
			offset = y * ts->platdata->x_num + x;
			data = (int)(raw_buff[offset] - raw_buff[offset - 1]) * 100 / raw_buff[offset];
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", data);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}

		for (; x < ts->platdata->x_num - 1; x++) {
			offset = y * ts->platdata->x_num + x;
			data = (int)(raw_buff[offset + 1] - raw_buff[offset]) * 100 / raw_buff[offset];
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", data);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff,
			strnlen(buff, ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);
	kfree(raw_buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	kfree(raw_buff);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_open_uni_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff, *uni_buff;
	int x, y;
	int min, max;
	int offset, data;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	uni_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!uni_buff) {
		mutex_unlock(&ts->lock);
		kfree(raw_buff);
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_open_read(ts, raw_buff))
		goto err;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 1; x < ts->platdata->x_num / 2; x++) {
			offset = y * ts->platdata->x_num + x;
			data = (int)(raw_buff[offset] - raw_buff[offset - 1]) * 100 / raw_buff[offset];
			uni_buff[offset] = data;
		}

		for (; x < ts->platdata->x_num - 1; x++) {
			offset = y * ts->platdata->x_num + x;
			data = (int)(raw_buff[offset + 1] - raw_buff[offset]) * 100 / raw_buff[offset];
			uni_buff[offset] = data;
		}
	}

	if (ts->cmd_result_all_onboot)
		nvt_ts_calc_buff(ts, uni_buff, &min, &max);
	else
		nvt_ts_print_gap_buff(ts, uni_buff, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(uni_buff);
	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OPEN_UNIFORMITY");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(uni_buff);
	kfree(raw_buff);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OPEN_UNIFORMITY");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_short_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_short_read(ts, raw_buff))
		goto err;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SHORT_TEST");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SHORT_TEST");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char tmp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;

	int *raw_buff;
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto err;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_rawdata_read(ts, raw_buff))
		goto error_rawdata_read;

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_buff[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff,
				ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);
	kfree(raw_buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

error_rawdata_read:
	kfree(buff);
err:
	kfree(raw_buff);

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_rawdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_rawdata_read(ts, raw_buff))
		goto err;

	if (ts->cmd_result_all_onboot)
		nvt_ts_calc_buff(ts, raw_buff, &min, &max);
	else
		nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_RAW_DATA");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_RAW_DATA");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_ccdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char tmp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int *raw_buff;
	int x, y;
	int offset;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto err;

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_ccdata_read(ts, raw_buff)) {
		kfree(buff);
		goto err;
	}

	for (y = 0; y < ts->platdata->y_num; y++) {
		for (x = 0; x < ts->platdata->x_num; x++) {
			offset = y * ts->platdata->x_num + x;
			snprintf(tmp, CMD_RESULT_WORD_LEN, "%d,", raw_buff[offset]);
			strncat(buff, tmp, CMD_RESULT_WORD_LEN);
			memset(tmp, 0x00, CMD_RESULT_WORD_LEN);
		}
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	sec_cmd_set_cmd_result(sec, buff,
			strnlen(buff, ts->platdata->x_num * ts->platdata->y_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	kfree(buff);
	kfree(raw_buff);

	snprintf(tmp, sizeof(tmp), "%s", "OK");
	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	kfree(raw_buff);
out:
	snprintf(tmp, sizeof(tmp), "%s", "NG");
	sec_cmd_set_cmd_result(sec, tmp, strnlen(tmp, sizeof(tmp)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s", __func__, tmp);
}

static void run_self_ccdata_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_buff;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_buff = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_buff) {
		mutex_unlock(&ts->lock);
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_ccdata_read(ts, raw_buff))
		goto err;

	nvt_ts_print_buff(ts, raw_buff, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_CC");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_buff);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_CC");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_noise_max_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_noise_read(ts, raw_min, raw_max))
		goto err;

	if (ts->cmd_result_all_onboot)
		nvt_ts_calc_buff(ts, raw_max, &min, &max);
	else
		nvt_ts_print_buff(ts, raw_max, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE_MAX");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE_MAX");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_self_noise_min_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int *raw_min, *raw_max;
	int min, max;

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	min = 0x7FFFFFFF;
	max = 0x80000000;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	raw_min = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_min)
		goto err_alloc_mem;
	raw_max = kzalloc(ts->platdata->x_num * ts->platdata->y_num * sizeof(int), GFP_KERNEL);
	if (!raw_max) {
		kfree(raw_min);
		goto err_alloc_mem;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download MP FW---
		nvt_ts_fw_update_from_mp_bin(ts, true);

	if (nvt_ts_noise_read(ts, raw_min, raw_max))
		goto err;

	if (ts->cmd_result_all_onboot)
		nvt_ts_calc_buff(ts, raw_max, &min, &max);
	else
		nvt_ts_print_buff(ts, raw_min, &min, &max);

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE_MIN");

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	kfree(raw_min);
	kfree(raw_max);
err_alloc_mem:
	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE_MIN");

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char test[32];
	char result[32];

	sec_cmd_set_default_result(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	if (ts->mmap->MBT_EN) {
		if (nvt_ts_sram_test(ts))
			goto err;
	} else {
		input_info(true, &ts->client->dev, "%s: not support SRAM test.\n", __func__);
		goto err;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		//---Download Normal FW---
		nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%d", TEST_RESULT_PASS);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	snprintf(result, sizeof(result), "RESULT=PASS");
	sec_cmd_send_event_to_user(&ts->sec, test, result);

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;

err:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	snprintf(result, sizeof(result), "RESULT=FAIL");
	sec_cmd_send_event_to_user(&ts->sec, test, result);

	input_raw_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	sec->item_count = 0;

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	mutex_lock(&ts->lock);
	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);
	mutex_unlock(&ts->lock);

	run_self_open_raw_read(sec);
	run_self_open_uni_read(sec);
	run_self_rawdata_read(sec);
	run_self_ccdata_read(sec);
	run_self_short_read(sec);
	run_self_noise_max_read(sec);
	run_self_noise_min_read(sec);
	run_sram_test(sec);

	mutex_lock(&ts->lock);
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);
	mutex_unlock(&ts->lock);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

void read_tsp_info_onboot(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	sec->item_count = 0;

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;
	ts->cmd_result_all_onboot = true;

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	mutex_lock(&ts->lock);
	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);
	mutex_unlock(&ts->lock);

	run_self_open_raw_read(sec);
	run_self_open_uni_read(sec);
	run_self_rawdata_read(sec);
	run_self_ccdata_read(sec);
	run_self_short_read(sec);
	run_self_noise_max_read(sec);
	run_self_noise_min_read(sec);
	run_sram_test(sec);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	mutex_lock(&ts->lock);
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);
	mutex_unlock(&ts->lock);

out:
	ts->cmd_result_all_onboot = false;
	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void incell_power_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		ts->lcdoff_test = !!sec->cmd_param[0];
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void factory_lcdoff_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec->item_count = 0;

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_self_lpwg_rawdata_read(sec);
	run_self_lpwg_noise_min_read(sec);
	run_self_lpwg_noise_max_read(sec);

	run_self_fdm_rawdata_read(sec);
	run_self_fdm_noise_min_read(sec);
	run_self_fdm_noise_max_read(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char test[32];
	char result[32];
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] != OPEN_SHORT_TEST) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		goto out;
	}

	//---Download MP FW---
	nvt_ts_fw_update_from_mp_bin(ts, true);

	if (sec->cmd_param[1] == CHECK_ONLY_OPEN_TEST) {
		ret = nvt_ts_open_test(ts);
	} else if (sec->cmd_param[1] == CHECK_ONLY_SHORT_TEST) {
		ret = nvt_ts_short_test(ts);
	} else {
		input_err(true, &ts->client->dev, "%s: invalid parameter option %d\n",
			__func__, sec->cmd_param[1]);
		goto out;
	}

	if (ret) {
		goto out;
	}

	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(result, sizeof(result), "RESULT=PASS");
	sec_cmd_send_event_to_user(&ts->sec, test, result);

	return;
out:
	//---Download Normal FW---
	nvt_ts_fw_update_from_mp_bin(ts, false);

	mutex_unlock(&ts->lock);
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	snprintf(result, sizeof(result), "RESULT=FAIL");
	sec_cmd_send_event_to_user(&ts->sec, test, result);

	input_info(true, &ts->client->dev, "%s: %s", __func__, buff);
}

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void get_func_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "0x%X", nvt_ts_mode_read(ts));

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 buf[3] = {0};
	short int sum, thd;

	sec_cmd_set_default_result(sec);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROXIMITY_SUM);

	//---read proximity sum---
	buf[0] = EVENT_MAP_PROXIMITY_SUM;
	CTP_SPI_READ(ts->client, buf, 3);

	sum = (buf[2] << 8 | buf[1]);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROXIMITY_THD);

	//---read proximity sum---
	buf[0] = EVENT_MAP_PROXIMITY_THD;
	CTP_SPI_READ(ts->client, buf, 3);

	thd = (buf[2] << 8 | buf[1]);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", sum, thd);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: buff : %s sum : %d thd : %d\n", __func__, buff, sum, thd);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
#ifdef PROXIMITY_FUNCTION
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("prox_lp_scan_mode", prox_lp_scan_mode),},
#endif
	{SEC_CMD_H("dead_zone_enable", dead_zone_enable),},
#if SEC_LPWG_DUMP
	{SEC_CMD_H("set_lpwg_dump", set_lpwg_dump),},
#endif
	{SEC_CMD_H("spay_enable", spay_enable),},
#if SHOW_NOT_SUPPORT_CMD
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
	{SEC_CMD_H("set_untouchable_area_rect", set_untouchable_area_rect),},
	{SEC_CMD_H("set_grip_data", set_grip_data),},
	{SEC_CMD_H("lcd_orientation", lcd_orientation),},
#endif	// end of #if SHOW_NOT_SUPPORT_CMD
	{SEC_CMD("run_self_lpwg_rawdata_read", run_self_lpwg_rawdata_read),},
	{SEC_CMD("run_self_lpwg_rawdata_read_all_lcdoff", run_self_lpwg_rawdata_read_all),},
	{SEC_CMD("run_self_lpwg_noise_min_read", run_self_lpwg_noise_min_read),},
	{SEC_CMD("run_self_lpwg_noise_min_read_all_lcdoff", run_self_lpwg_noise_min_read_all),},
	{SEC_CMD("run_self_lpwg_noise_max_read", run_self_lpwg_noise_max_read),},
	{SEC_CMD("run_self_lpwg_noise_max_read_all_lcdoff", run_self_lpwg_noise_max_read_all),},
	{SEC_CMD("run_self_fdm_rawdata_read", run_self_fdm_rawdata_read),},
	{SEC_CMD("run_self_fdm_rawdata_read_all_lcdoff", run_self_fdm_rawdata_read_all),},
	{SEC_CMD("run_self_fdm_noise_min_read", run_self_fdm_noise_min_read),},
	{SEC_CMD("run_self_fdm_noise_min_read_all_lcdoff", run_self_fdm_noise_min_read_all),},
	{SEC_CMD("run_self_fdm_noise_max_read", run_self_fdm_noise_max_read),},
	{SEC_CMD("run_self_fdm_noise_max_read_all_lcdoff", run_self_fdm_noise_max_read_all),},
	{SEC_CMD("run_self_open_raw_read", run_self_open_raw_read),},
	{SEC_CMD("run_self_open_raw_read_all", run_self_open_raw_read_all),},
	{SEC_CMD("run_self_open_uni_read", run_self_open_uni_read),},
	{SEC_CMD("run_self_open_uni_read_all", run_self_open_uni_read_all),},
	{SEC_CMD("run_self_short_read", run_self_short_read),},
	{SEC_CMD("run_self_rawdata_read", run_self_rawdata_read),},
	{SEC_CMD("run_self_rawdata_read_all", run_self_rawdata_read_all),},
	{SEC_CMD("run_self_ccdata_read", run_self_ccdata_read),},
	{SEC_CMD("run_self_ccdata_read_all", run_self_ccdata_read_all),},
	{SEC_CMD("run_self_noise_min_read", run_self_noise_min_read),},
	{SEC_CMD("run_self_noise_max_read", run_self_noise_max_read),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_sram_test", run_sram_test),},
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	{SEC_CMD("get_func_mode", get_func_mode),},
#endif
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("incell_power_control", incell_power_control),},
	{SEC_CMD("factory_lcdoff_cmd_result_all", factory_lcdoff_cmd_result_all),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

#if SHOW_NOT_SUPPORT_CMD
static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->multi_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->multi_count);
}

static ssize_t clear_multi_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	ts->multi_count = 0;

	input_info(true, &ts->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->comm_err_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->comm_err_count);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	ts->comm_err_count = 0;

	input_info(true, &ts->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[256] = { 0 };

	snprintf(buff, sizeof(buff), "NO%02X%02X%02X%02X",
		ts->fw_ver_bin[0], ts->fw_ver_bin[1], ts->fw_ver_bin[2], ts->fw_ver_bin[3]);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t read_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char *str_orig, *str;
	char buffer[21] = { 0 };
	char *tok;

	if (ts->platdata->firmware_name) {
		str_orig = kstrdup(ts->platdata->firmware_name, GFP_KERNEL);
		if (!str_orig)
			goto err;

		str = str_orig;

		tok = strsep(&str, "/");
		tok = strsep(&str, "_");

		snprintf(buffer, sizeof(buffer), "NVT_%s", tok);

		kfree(str_orig);
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buffer);
err:
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buffer);
}

static ssize_t clear_checksum_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	ts->checksum_result = 0;

	input_info(true, &ts->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_checksum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->checksum_result);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->checksum_result);
}

static ssize_t read_all_touch_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: touch:%d\n", __func__, ts->all_finger_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "\"TTCN\":\"%d\"", ts->all_finger_count);
}

static ssize_t clear_all_touch_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	ts->all_finger_count = 0;

	input_info(true, &ts->client->dev, "%s: clear\n", __func__);

	return count;
}
#endif	// end of #if SHOW_NOT_SUPPORT_CMD

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[256] = { 0 };

	snprintf(buff, sizeof(buff), "%d %d %d", ts->scrub_id, 0, 0);
	input_info(true, &ts->client->dev, "%s: scrub_id: %s\n", __func__, buff);

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char data[20] = { 0 };
	int diff[9] = { 0 };
	int i;
	int ret;

	mutex_lock(&ts->lock);
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_SENSITIVITY_DIFF);

	data[0] = EVENT_MAP_SENSITIVITY_DIFF;
	ret = CTP_SPI_READ(ts->client, data, 19);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read sensitivity",
			__func__);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
	mutex_unlock(&ts->lock);

	for (i = 0; i < 9; i++)
		diff[i] = (data[2 * i + 2] << 8) + data[2 * i + 1];

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
		diff[0], diff[1], diff[2], diff[3], diff[4], diff[5], diff[6], diff[7], diff[8]);
}

static ssize_t sensitivity_mode_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	unsigned long val = 0;
	int ret;
	u8 mode;

	if (count > 2) {
		input_err(true, &ts->client->dev, "%s: invalid parameter\n", __func__);
		return count;
	}

	ret = kstrtoul(buf, 10, &val);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to get param\n", __func__);
		return count;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return count;
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__,
			val ? "enable" : "disable");

	mode = val ? SENSITIVITY_ENTER: SENSITIVITY_LEAVE;

	if (val)
		ts->sec_function |= SENSITIVITY_MASK;
	else
		ts->sec_function &= ~SENSITIVITY_MASK;

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		return count;
	}

	ret = nvt_ts_mode_switch(ts, mode, true);

	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: %s %s\n", __func__,
			val ? "enable" : "disabled", ret ? "fail" : "done");

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, ts->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", ts->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	long data;
	int ret = 0;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail to read parm!\n", __func__);
		return ret;
	}

	ts->prox_power_off = data;

	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, data);

	return count;
}

#ifdef PROXIMITY_FUNCTION
/** virtual_prox **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, data);

	if (data != 0 && data != 1) {
		input_err(true, &ts->client->dev, "%s: incorrect data\n", __func__);
		return -EINVAL;
	}

	if (ts->power_status == POWER_OFF_STATUS) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return count;
	}

	ts->ear_detect_mode = data;

	ret = set_ear_detect(ts, ts->ear_detect_mode, true);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: set_ear_detect fail!\n", __func__);
	}

	return count;
}
#endif

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static ssize_t noise_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		return snprintf(buf, PAGE_SIZE, "busy : another task is running\n");
	}

	nvt_ts_mode_read(ts);

	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: Noise Mode status is %s\n", __func__,
			ts->noise_mode ? "ON" : "OFF");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", ts->noise_mode);
}

static ssize_t support_prox_in_aot_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
			__func__);
		return snprintf(buf, PAGE_SIZE, "busy : another task is running\n");
	}

	nvt_ts_mode_read(ts);

	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: Support proximity in AOT %s\n", __func__,
			ts->prox_in_aot ? "ON" : "OFF");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", ts->prox_in_aot);
}
#endif

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 feature = 0;

	if (ts->platdata->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->platdata->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	if (ts->platdata->prox_lp_scan_enabled)
		feature |= INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED;

	input_info(true, &ts->client->dev, "%s: %d%s%s%s\n",
				__func__, feature,
				feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
				feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "",
				feature & INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED ? " LPSCAN" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: power_status %d\n", __func__, ts->power_status);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->power_status);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	int buff[2];
	int ret;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &ts->client->dev,
				"%s: failed read params [%d]\n", __func__, ret);
		return -EINVAL;
	}

	input_info(true, &ts->client->dev, "%s: %d %d\n", __func__, buff[0], buff[1]);

	/* handle same sequence : buff[0] = DISPLAY_STATE_ON, DISPLAY_STATE_DOZE, DISPLAY_STATE_DOZE_SUSPEND */
	if (buff[0] == DISPLAY_STATE_DOZE || buff[0] == DISPLAY_STATE_DOZE_SUSPEND)
		buff[0] = DISPLAY_STATE_ON;

	if (buff[0] == DISPLAY_STATE_ON) {
		if (buff[1] == DISPLAY_EVENT_EARLY)
			nvt_ts_early_resume(&ts->client->dev);
		else if (buff[1] == DISPLAY_EVENT_LATE)
			nvt_ts_resume(&ts->client->dev);
	} else if (buff[0] == DISPLAY_STATE_OFF) {
		if (buff[1] == DISPLAY_EVENT_EARLY)
			nvt_ts_suspend(&ts->client->dev);
	} else if (buff[0] == DISPLAY_STATE_LPM_OFF) {
		input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_LPM_OFF ++\n", __func__);
		ts->power_status = POWER_OFF_STATUS;
		pinctrl_configure(ts, false);
		nvt_irq_enable(false);
		input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_LPM_OFF --\n", __func__);
	} else if (buff[0] == DISPLAY_STATE_SERVICE_SHUTDOWN) {
		nvt_irq_enable(false);
		ts->power_status = POWER_OFF_STATUS;
		pinctrl_configure(ts, false);
		input_info(true, &ts->client->dev, "%s: shutdown, cancel nvt_fwu_work and irq,pinctrl false --\n", __func__);
	}

	return count;
}

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
#if SEC_LPWG_DUMP
	nvt_ts_lpwg_dump_buf_read(buf);

	return strlen(buf);
#else
	return snprintf(buf, SEC_CMD_BUF_SIZE, "Not support lp dump!");
#endif
}

#if SHOW_NOT_SUPPORT_CMD
static DEVICE_ATTR(multi_count, 0664, read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(comm_err_count, 0664, read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(module_id, 0444, read_module_id_show, NULL);
static DEVICE_ATTR(vendor, 0444, read_vendor_show, NULL);
static DEVICE_ATTR(checksum, 0664, read_checksum_show, clear_checksum_store);
static DEVICE_ATTR(all_touch_count, 0664, read_all_touch_count_show, clear_all_touch_count_store);
#endif	// end of #if SHOW_NOT_SUPPORT_CMD
static DEVICE_ATTR(scrub_pos, 0444, scrub_position_show, NULL);
static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
#ifdef PROXIMITY_FUNCTION
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
#endif
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static DEVICE_ATTR(noise_mode, 0664, noise_mode_show, NULL);
static DEVICE_ATTR(support_prox_in_aot, 0664, support_prox_in_aot_show, NULL);
#endif
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);

static struct attribute *cmd_attributes[] = {
#if SHOW_NOT_SUPPORT_CMD
	&dev_attr_multi_count.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_vendor.attr,
	&dev_attr_checksum.attr,
	&dev_attr_all_touch_count.attr,
#endif	// end of #if SHOW_NOT_SUPPORT_CMD
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_prox_power_off.attr,
#ifdef PROXIMITY_FUNCTION
	&dev_attr_virtual_prox.attr,
#endif
	&dev_attr_support_feature.attr,
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	&dev_attr_noise_mode.attr,
	&dev_attr_support_prox_in_aot.attr,
#endif
	&dev_attr_enabled.attr,
	&dev_attr_get_lp_dump.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int nvt_sec_mp_parse_dt(struct nvt_ts_data *ts, const char *node_compatible)
{
	struct device_node *np = ts->client->dev.of_node;
	struct device_node *child = NULL;

	/* find each MP sub-nodes */
	for_each_child_of_node(np, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			np = child;
			break;
		}
	}

	if (!child) {
		input_err(true, &ts->client->dev, "%s: do not find mp criteria for %s\n",
			  __func__, node_compatible);
		return -EINVAL;
	}

	/* MP Criteria */
	if (of_property_read_u32_array(np, "open_test_spec", ts->platdata->open_test_spec, 2))
		return -EINVAL;

	if (of_property_read_u32_array(np, "short_test_spec", ts->platdata->short_test_spec, 2))
		return -EINVAL;

	if (of_property_read_u32(np, "diff_test_frame", &ts->platdata->diff_test_frame))
		return -EINVAL;

	if (of_property_read_u32(np, "fdm_x_num", &ts->platdata->fdm_x_num))
		return -EINVAL;

	input_info(true, &ts->client->dev, "%s: parse mp criteria for %s\n", __func__, node_compatible);

	return 0;
}

int nvt_ts_sec_fn_init(struct nvt_ts_data *ts)
{
	int ret;

	ret = sec_cmd_init(&ts->sec, sec_cmds, ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to sec cmd init\n",
			__func__);
		return ret;
	}

	ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to create sysfs attributes\n",
			__func__);
			goto out;
	}

	ret = sysfs_create_link(&ts->sec.fac_dev->kobj, &ts->input_dev->dev.kobj, "input");
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to creat sysfs link\n",
			__func__);
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;

out:
	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);

	return ret;
}

void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts)
{
	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}
