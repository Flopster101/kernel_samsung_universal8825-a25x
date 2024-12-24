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
#include <linux/rtc.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#define PROC_NODE_NAME					"goodix_ts/get_rawdata"

#define JITTER1_TEST_ITEM					"jitter100-test"
#define JITTER2_TEST_ITEM					"jitter1000-test"
#define SRAM_TEST_ITEM						"sram-test"
#define SHORT_TEST_ITEM						"short-test"
#define OPEN_TEST_ITEM						"open-test"

#define SEC_TEST_OK					0
#define SEC_TEST_NG					1

#define MAX_TEST_TIME_MS				15000
#define DEFAULT_TEST_TIME_MS				7000

#define CHN_VDD								0xFF
#define CHN_GND								0x7F
#define DRV_CHANNEL_FLAG					0x80

extern struct goodix_module goodix_modules;
static bool module_initialized;

static u32 short_parms[] = {10, 800, 800, 800, 800, 800, 30};

typedef struct {
	u8 result;
	u8 drv_drv_num;
	u8 sen_sen_num;
	u8 drv_sen_num;
	u8 drv_gnd_avdd_num;
	u8 sen_gnd_avdd_num;
	u16 checksum;
} __packed test_result_t;

struct goodix_jitter_data {
	short mutualDiffMax;
	short mutualDiffMin;
	short selfDiffMax;
	short selfDiffMin;
	/* 1000 frames */
	short min_of_min_matrix;
	short max_of_min_matrix;
	short min_of_max_matrix;
	short max_of_max_matrix;
	short min_of_avg_matrix;
	short max_of_avg_matrix;
};

static int ts_test_read(struct goodix_ts_data *ts, unsigned int addr,
		 unsigned char *data, unsigned int len)
{
	return ts->bus->read(ts->bus->dev, addr, data, len);
}

static int ts_test_write(struct goodix_ts_data *ts, unsigned int addr,
		unsigned char *data, unsigned int len)
{
	return ts->bus->write(ts->bus->dev, addr, data, len);
}

void goodix_data_statistics(s16 *data, int data_size,
		int *max, int *min)
{
	int i;

	*min = data[0];
	*max = data[0];
	for (i = 0; i < data_size; i++) {
		*max = *max < data[i] ? data[i] : *max;
		*min = *min > data[i] ? data[i] : *min;
	}
}

static int cal_cha_to_cha_res(struct goodix_ts_data *ts, int v1, int v2)
{
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B)
		return (v1 - v2) * 74 / v2 + 20;
	else if (ts->bus->ic_type == IC_TYPE_BERLIN_D)
		return (v1 / v2 - 1) * 55 + 45;
	else if (ts->bus->ic_type == IC_TYPE_GT9916K)
		return (v1 / v2 - 1) * 70 + 59;
	else {
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
		return (v1 - v2) * 74 / v2 + 20;
	}
}

static int cal_cha_to_avdd_res(struct goodix_ts_data *ts, int v1, int v2)
{
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B)
		return 64 * (2 * v2 - 25) * 99 / v1 - 60;
	else if (ts->bus->ic_type == IC_TYPE_BERLIN_D)
		return 64 * (2 * v2 - 25) * 76 / v1 - 15;
	else if (ts->bus->ic_type == IC_TYPE_GT9916K)
		return 64 * (2 * v2 - 25) * 93 / v1 - 20;
	else {
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
		return 64 * (2 * v2 - 25) * 99 / v1 - 60;
	}
}

static int cal_cha_to_gnd_res(struct goodix_ts_data *ts, int v)
{
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B)
		return 150500 / v - 60;
	else if (ts->bus->ic_type == IC_TYPE_BERLIN_D)
		return 120000 / v - 16;
	else if (ts->bus->ic_type == IC_TYPE_GT9916K)
		return 145000 / v - 15;
	else {
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
		return 150500 / v - 60;
	}
}

static u32 map_die2pin(struct goodix_ts_data *ts, u32 chn_num)
{
	int i = 0;
	u32 res = 255;
	int max_drv_num = ts->max_drv_num;
	int max_sen_num = ts->max_sen_num;

	if (chn_num & DRV_CHANNEL_FLAG)
		chn_num = (chn_num & ~DRV_CHANNEL_FLAG) + max_sen_num;

	for (i = 0; i < max_sen_num; i++) {
		if ((u8)ts->sen_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	/* res != 255 mean found the corresponding channel num */
	if (res != 255)
		return res;
	/* if cannot find in SenMap try find in DrvMap */
	for (i = 0; i < max_drv_num; i++) {
		if ((u8)ts->drv_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	if (i >= max_drv_num)
		ts_err("Faild found corrresponding channel num:%d", chn_num);
	else
		res |= DRV_CHANNEL_FLAG;

	return res;
}

#define SHORT_TEST_RUN_REG			0x10400
#define SHORT_TEST_RUN_FLAG			0xAA
#define INSPECT_FW_SWITCH_CMD			0x85
static int goodix_brl_short_test_prepare(struct goodix_ts_data *ts)
{
	struct goodix_ts_cmd tmp_cmd;
	int ret;
	int retry = 5;
	u8 status;

	ts_info("short test prepare IN");
	tmp_cmd.len = 4;
	tmp_cmd.cmd = INSPECT_FW_SWITCH_CMD;
	ret = ts->hw_ops->send_cmd(ts, &tmp_cmd);
	if (ret < 0) {
		ts_err("send test mode failed");
		return ret;
	}

	while (retry--) {
		sec_delay(40);
		ret = ts->hw_ops->read(ts, SHORT_TEST_RUN_REG, &status, 1);
		if (!ret && status == SHORT_TEST_RUN_FLAG)
			return 0;
		ts_info("short_mode_status=0x%02x ret=%d", status, ret);
	}

	return -EINVAL;
}

static int gdix_check_tx_tx_shortcircut(struct goodix_ts_data *ts,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf;
	u32 data_reg = ts->drv_drv_reg;
	int max_drv_num = ts->max_drv_num;
	int max_sen_num = ts->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_drv_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
    /* drv&drv shortcircut check */
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts, data_reg, data_buf, size);
		if (ret < 0) {
			ts_err("Failed read Drv-to-Drv short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Drv-to-Drv adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = short_parms[1];
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		short_die_num -= max_sen_num;
		if (short_die_num >= max_drv_num) {
			ts_info("invalid short pad num:%d",
				short_die_num + max_sen_num);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = short_die_num + 1; j < max_drv_num; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);

			if (adc_signal < short_parms[0])
				continue;

			short_r = (u32)cal_cha_to_cha_res(ts, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num =
					map_die2pin(ts, short_die_num + max_sen_num);
				slave_pin_num =
					map_die2pin(ts, j + max_sen_num);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));

				/* save fail ch on ts->test_data.open_short_test_trx_result[] */
				{
					int offset, num;

					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("slave_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);
				}

				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

static int gdix_check_rx_rx_shortcircut(struct goodix_ts_data *ts,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf;
	u32 data_reg = ts->sen_sen_reg;
	int max_sen_num = ts->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_sen_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&drv shortcircut check */
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts, data_reg, data_buf, size);
		if (ret) {
			ts_err("Failed read Sen-to-Sen short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Sen-to-Sen adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = short_parms[3];
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		if (short_die_num >= max_sen_num) {
			ts_info("invalid short pad num:%d",	short_die_num);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = short_die_num + 1; j < max_sen_num; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);

			if (adc_signal < short_parms[0])
				continue;

			short_r = (u32)cal_cha_to_cha_res(ts, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(ts, short_die_num);
				slave_pin_num = map_die2pin(ts, j);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));

				/* save fail ch on ts->test_data.open_short_test_trx_result[] */
				{
					int offset, num;

					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("slave_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);
				}

				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

static int gdix_check_tx_rx_shortcircut(struct goodix_ts_data *ts,
		u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf = NULL;
	u32 data_reg = ts->drv_sen_reg;
	int max_drv_num = ts->max_drv_num;
	int max_sen_num = ts->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_drv_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&sen shortcircut check */
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(ts, data_reg, data_buf, size);
		if (ret) {
			ts_err("Failed read Drv-to-Sen short rawdata");
			err = -EINVAL;
			break;
		}

		if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
			ts_err("Drv-to-Sen adc data checksum error");
			err = -EINVAL;
			break;
		}

		r_threshold = short_parms[2];
		short_die_num = le16_to_cpup((__le16 *)&data_buf[0]);
		if (short_die_num >= max_sen_num) {
			ts_info("invalid short pad num:%d",	short_die_num);
			continue;
		}

		/* TODO: j start position need recheck */
		self_capdata = le16_to_cpup((__le16 *)&data_buf[2]);
		if (self_capdata == 0xffff || self_capdata == 0) {
			ts_info("invalid self_capdata:0x%x", self_capdata);
			continue;
		}

		for (j = 0; j < max_drv_num; j++) {
			adc_signal = le16_to_cpup((__le16 *)&data_buf[4 + j * 2]);

			if (adc_signal < short_parms[0])
				continue;

			short_r = (u32)cal_cha_to_cha_res(ts, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(ts, short_die_num);
				slave_pin_num = map_die2pin(ts, j + max_sen_num);
				if (master_pin_num == 0xFF || slave_pin_num == 0xFF) {
					ts_info("WARNNING invalid pin");
					continue;
				}
				ts_err("short circut:R=%dK,R_Threshold=%dK",
							short_r, r_threshold);
				ts_err("%s%d--%s%d shortcircut",
					(master_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(master_pin_num & ~DRV_CHANNEL_FLAG),
					(slave_pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
					(slave_pin_num & ~DRV_CHANNEL_FLAG));

				/* save fail ch on ts->test_data.open_short_test_trx_result[] */
				{
					int offset, num;

					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("slave_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);
				}

				err = -EINVAL;
			}
		}
		data_reg += size;
	}

	kfree(data_buf);
	return err;
}

static int gdix_check_resistance_to_gnd(struct goodix_ts_data *ts,
		u16 adc_signal, u32 pos)
{
	long r = 0;
	u16 r_th = 0, avdd_value = 0;
	u16 chn_id_tmp = 0;
	u8 pin_num = 0;
	unsigned short short_type;
	int max_drv_num = ts->max_drv_num;
	int max_sen_num = ts->max_sen_num;

	avdd_value = short_parms[6];
	short_type = adc_signal & 0x8000;
	adc_signal &= ~0x8000;
	if (adc_signal == 0)
		adc_signal = 1;

	if (short_type == 0) {
		/* short to GND */
		r = cal_cha_to_gnd_res(ts, adc_signal);
	} else {
		/* short to VDD */
		r = cal_cha_to_avdd_res(ts, adc_signal, avdd_value);
	}

	if (pos < max_drv_num)
		r_th = short_parms[4];
	else
		r_th = short_parms[5];

	chn_id_tmp = pos;
	if (chn_id_tmp < max_drv_num)
		chn_id_tmp += max_sen_num;
	else
		chn_id_tmp -= max_drv_num;

	if (r < r_th) {
		pin_num = map_die2pin(ts, chn_id_tmp);
		ts_err("%s%d shortcircut to %s,R=%ldK,R_Threshold=%dK",
				(pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
				(pin_num & ~DRV_CHANNEL_FLAG),
				short_type ? "VDD" : "GND",
				r, r_th);

		/* save fail ch on ts->test_data.open_short_test_trx_result[] */
		{
			int offset, num;

			offset = (pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
			offset += (pin_num & ~DRV_CHANNEL_FLAG) / 8;
			num = (pin_num & ~DRV_CHANNEL_FLAG) % 8;
			ts->test_data.open_short_test_trx_result[offset] |= (1 << num);
			ts_err("pin_num=%d, offset=%d, num=%d", pin_num, offset, num);
		}

		return -EINVAL;
	}

	return 0;
}

static int gdix_check_gndvdd_shortcircut(struct goodix_ts_data *ts)
{
	int ret = 0, err = 0;
	int size = 0, i = 0;
	u16 adc_signal = 0;
	u32 data_reg = ts->diff_code_reg;
	u8 *data_buf = NULL;
	int max_drv_num = ts->max_drv_num;
	int max_sen_num = ts->max_sen_num;

	size = (max_drv_num + max_sen_num) * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/*
	 * read diff code, diff code will be used to calculate
	 * resistance between channel and GND
	 */
	ret = ts_test_read(ts, data_reg, data_buf, size);
	if (ret < 0) {
		ts_err("Failed read to-gnd rawdata");
		err = -EINVAL;
		goto err_out;
	}

	if (checksum_cmp(data_buf, size, CHECKSUM_MODE_U8_LE)) {
		ts_err("diff code checksum error");
		err = -EINVAL;
		goto err_out;
	}

	for (i = 0; i < max_drv_num + max_sen_num; i++) {
		adc_signal = le16_to_cpup((__le16 *)&data_buf[i * 2]);
		ret = gdix_check_resistance_to_gnd(ts, adc_signal, i);
		if (ret != 0) {
			ts_err("Resistance to-gnd/vdd short");
			err = ret;
		}
	}

err_out:
	kfree(data_buf);
	return err;
}

static int goodix_short_analysis(struct goodix_ts_data *ts)
{
	int ret;
	int err = 0;
	test_result_t test_result;
	unsigned int result_reg = ts->short_test_result_reg;

	ret = ts->hw_ops->read(ts, result_reg,
				(u8 *)&test_result, sizeof(test_result));
	if (ret < 0) {
		ts_err("Read TEST_RESULT_REG failed");
		return ret;
	}

	if (checksum_cmp((u8 *)&test_result, sizeof(test_result),
				CHECKSUM_MODE_U8_LE)) {
		ts_err("shrot result checksum err");
		return -EINVAL;
	}

	if (!(test_result.result & 0x0F)) {
		ts_raw_info(">>>>> No shortcircut");
		ts->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("short flag 0x%02x, drv&drv:%d, sen&sen:%d, drv&sen:%d, drv/GNDVDD:%d, sen/GNDVDD:%d",
			test_result.result, test_result.drv_drv_num, test_result.sen_sen_num,
			test_result.drv_sen_num, test_result.drv_gnd_avdd_num, test_result.sen_gnd_avdd_num);
		/* get short channel and resistance */
		if (test_result.drv_drv_num)
			err |= gdix_check_tx_tx_shortcircut(ts, test_result.drv_drv_num);
		if (test_result.sen_sen_num)
			err |= gdix_check_rx_rx_shortcircut(ts, test_result.sen_sen_num);
		if (test_result.drv_sen_num)
			err |= gdix_check_tx_rx_shortcircut(ts, test_result.drv_sen_num);
		if (test_result.drv_gnd_avdd_num || test_result.sen_gnd_avdd_num)
			err |= gdix_check_gndvdd_shortcircut(ts);
		if (err)
			ts->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_NG;
		else
			ts->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_OK;
	}
	ts->test_data.info[SEC_SHORT_TEST].isFinished = true;

	if (err)
		return -EINVAL;

	return 0;
}

#define JITTER_LEN     24
static int goodix_brl_jitter_test(struct goodix_ts_data *ts, u8 type)
{
	int ret;
	int retry;
	struct goodix_ts_cmd cmd;
	struct goodix_jitter_data raw_data;
	unsigned int jitter_addr = ts->production_test_addr;
	u8 buf[JITTER_LEN];

	/* switch test config */
		cmd.len = 5;
	cmd.cmd = ts->switch_cfg_cmd;
	cmd.data[0] = 1;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("switch test config failed");
		return ret;
	}

	sec_delay(20);

	cmd.cmd = 0x88;
	cmd.data[0] = type;
	cmd.len = 5;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("send jitter test cmd failed");
		return ret;
	}
	if (type == JITTER_100_FRAMES)
		sec_delay(100);
	else
		sec_delay(8000);

	retry = 20;
	while (retry--) {
		ret = ts->hw_ops->read(ts, jitter_addr, buf, (int)sizeof(buf));
		if (ret < 0) {
			ts_err("read raw data failed");
			return ret;
		}
		if (type == JITTER_100_FRAMES) {
			if (buf[0] == 0xAA && buf[1] == 0xAA)
				break;
		} else {
			if (buf[0] == 0xBB && buf[1] == 0xBB)
				break;
		}
		sec_delay(50);
	}
	if (retry < 0) {
		ts_err("raw data not ready, status = %x%x", buf[0], buf[1]);
		return -EINVAL;
	}

	if (type == JITTER_100_FRAMES) {
		raw_data.mutualDiffMax = le16_to_cpup((__le16 *)&buf[2]);
		raw_data.mutualDiffMin = le16_to_cpup((__le16 *)&buf[4]);
		raw_data.selfDiffMax = le16_to_cpup((__le16 *)&buf[6]);
		raw_data.selfDiffMin = le16_to_cpup((__le16 *)&buf[8]);
		ts->test_data.info[SEC_JITTER1_TEST].data[0] = raw_data.mutualDiffMax;
		ts->test_data.info[SEC_JITTER1_TEST].data[1] = raw_data.mutualDiffMin;
		ts->test_data.info[SEC_JITTER1_TEST].data[2] = raw_data.selfDiffMax;
		ts->test_data.info[SEC_JITTER1_TEST].data[3] = raw_data.selfDiffMin;
		ts->test_data.info[SEC_JITTER1_TEST].isFinished = true;
		ts_raw_info("mutualdiff[%d, %d] selfdiff[%d, %d]",
				raw_data.mutualDiffMin, raw_data.mutualDiffMax,
				raw_data.selfDiffMin, raw_data.selfDiffMax);
	} else {
		if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
			raw_data.min_of_min_matrix = le16_to_cpup((__le16 *)&buf[10]);
			raw_data.max_of_min_matrix = le16_to_cpup((__le16 *)&buf[12]);
			raw_data.min_of_max_matrix = le16_to_cpup((__le16 *)&buf[14]);
			raw_data.max_of_max_matrix = le16_to_cpup((__le16 *)&buf[16]);
			raw_data.min_of_avg_matrix = le16_to_cpup((__le16 *)&buf[18]);
			raw_data.max_of_avg_matrix = le16_to_cpup((__le16 *)&buf[20]);
		} else if (ts->bus->ic_type == IC_TYPE_BERLIN_D ||
				ts->bus->ic_type == IC_TYPE_GT9916K) {
			raw_data.min_of_min_matrix = le16_to_cpup((__le16 *)&buf[2]);
			raw_data.max_of_min_matrix = le16_to_cpup((__le16 *)&buf[4]);
			raw_data.min_of_max_matrix = le16_to_cpup((__le16 *)&buf[6]);
			raw_data.max_of_max_matrix = le16_to_cpup((__le16 *)&buf[8]);
			raw_data.min_of_avg_matrix = le16_to_cpup((__le16 *)&buf[10]);
			raw_data.max_of_avg_matrix = le16_to_cpup((__le16 *)&buf[12]);
		} else {
			raw_data.min_of_min_matrix = le16_to_cpup((__le16 *)&buf[10]);
			raw_data.max_of_min_matrix = le16_to_cpup((__le16 *)&buf[12]);
			raw_data.min_of_max_matrix = le16_to_cpup((__le16 *)&buf[14]);
			raw_data.max_of_max_matrix = le16_to_cpup((__le16 *)&buf[16]);
			raw_data.min_of_avg_matrix = le16_to_cpup((__le16 *)&buf[18]);
			raw_data.max_of_avg_matrix = le16_to_cpup((__le16 *)&buf[20]);
			ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
		}

		ts->test_data.info[SEC_JITTER2_TEST].data[0] = raw_data.min_of_min_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].data[1] = raw_data.max_of_min_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].data[2] = raw_data.min_of_max_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].data[3] = raw_data.max_of_max_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].data[4] = raw_data.min_of_avg_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].data[5] = raw_data.max_of_avg_matrix;
		ts->test_data.info[SEC_JITTER2_TEST].isFinished = true;
		ts_raw_info("min_matrix[%d, %d] max_matrix[%d, %d] avg_matrix[%d, %d]",
				raw_data.min_of_min_matrix, raw_data.max_of_min_matrix,
				raw_data.min_of_max_matrix, raw_data.max_of_max_matrix,
				raw_data.min_of_avg_matrix, raw_data.max_of_avg_matrix);
	}

	return 0;
}

int goodix_jitter_test(struct goodix_ts_data *ts, u8 type)
{
	int ret;

	if (type == JITTER_100_FRAMES)
		ts->test_data.info[SEC_JITTER1_TEST].item_name = JITTER1_TEST_ITEM;
	else
		ts->test_data.info[SEC_JITTER2_TEST].item_name = JITTER2_TEST_ITEM;

	ret = goodix_brl_jitter_test(ts, type);
	if (ret < 0)
		ts_err("jitter test process failed");
	return ret;
}

static int goodix_brl_open_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 20;
	int i;
	struct goodix_ts_cmd cmd;
	unsigned char buf[OPEN_TEST_RESULT_LEN];

	cmd.cmd = 0x89;
	cmd.len = 4;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("send open test cmd failed");
		return ret;
	}
	sec_delay(400);
	while (retry--) {
		ret = ts->hw_ops->read(ts, OPEN_TEST_RESULT, buf, (int)sizeof(buf));
		if (ret < 0) {
			ts_err("read open test result failed");
			return ret;
		}
		if (buf[0] == 0xAA && buf[1] == 0xAA)
			break;
		sec_delay(50);
	}
	if (retry < 0) {
		ts_err("open test not ready, status = %x%x", buf[0], buf[1]);
		return -EINVAL;
	}

	ret = checksum_cmp(buf + 2, OPEN_TEST_RESULT_LEN - 2, CHECKSUM_MODE_U8_LE);
	if (ret) {
		ts_err("open test result checksum error %*ph", OPEN_TEST_RESULT_LEN, buf);
		return -EINVAL;
	}

	if (buf[2] == 0) {
		ts_raw_info("open test pass");
		ret = 0;
		ts->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("open test failed");

		/* save result data */
		for (i = 0; i < OPEN_SHORT_TEST_RESULT_LEN ; i++)
			ts->test_data.open_short_test_trx_result[i] =  buf[3 + i];

		ts->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_NG;
		/* DRV[0~55] total 7 bytes */
		for (i = 0; i < DRV_CHAN_BYTES; i++) {
			if (buf[i + 3])
				ts_raw_info("DRV[%d~%d] open circuit, ret=0x%X", i * 8, i * 8 + 7, buf[i + 3]);
		}
		/* SEN[0~79] total 10 bytes */
		for (i = 0; i < SEN_CHAN_BYTES; i++) {
			if (buf[i + 10])
				ts_raw_info("SEN[%d~%d] open circuit, ret=0x%X", i * 8, i * 8 + 7, buf[i + 10]);
		}
		ret = -EINVAL;
	}
	ts->test_data.info[SEC_OPEN_TEST].isFinished = true;

	return ret;
}

static int goodix_brld_open_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 20;
	int i;
	struct goodix_ts_cmd cmd;
	unsigned char buf[24];
	unsigned int result_addr = ts->production_test_addr;

	cmd.cmd = 0x63;
	cmd.len = 4;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("send open test cmd failed");
		return ret;
	}
	sec_delay(200);

	while (retry--) {
		ret = ts->hw_ops->read(ts, result_addr, buf, (int)sizeof(buf));
		if (ret < 0) {
			ts_err("read open test result failed");
			return ret;
		}
		if (buf[0] == 0xCC && buf[1] == 0xCC)
			break;
		sec_delay(50);
	}
	if (retry < 0) {
		ts_err("open test not ready, status = %x%x", buf[0], buf[1]);
		return -EINVAL;
	}

	ret = checksum_cmp(buf, sizeof(buf), CHECKSUM_MODE_U8_LE);
	if (ret) {
		ts_err("open test result checksum error");
		return -EINVAL;
	}

	if (buf[2] == 0) {
		ts_raw_info("open test pass");
		ret = 0;
		ts->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("open test failed");

		/* save result data */
		for (i = 0; i < OPEN_SHORT_TEST_RESULT_LEN ; i++)
			ts->test_data.open_short_test_trx_result[i] =  buf[3 + i];

		ts->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_NG;
		/* DRV[0~55] total 7 bytes */
		for (i = 0; i < DRV_CHAN_BYTES; i++) {
			if (buf[i + 3])
				ts_raw_info("DRV[%d~%d] open circuit, ret=0x%X", i * 8, i * 8 + 7, buf[i + 3]);
		}
		/* SEN[0~79] total 10 bytes */
		for (i = 0; i < SEN_CHAN_BYTES; i++) {
			if (buf[i + 10])
				ts_raw_info("SEN[%d~%d] open circuit, ret=0x%X", i * 8, i * 8 + 7, buf[i + 10]);
		}
		ret = -EINVAL;
	}

	ts->test_data.info[SEC_OPEN_TEST].isFinished = true;

	return ret;
}

int goodix_open_test(struct goodix_ts_data *ts)
{
	int ret = 0;
	int retry = 2;
	struct goodix_ts_cmd temp_cmd;

	ts->test_data.info[SEC_OPEN_TEST].item_name = OPEN_TEST_ITEM;

restart:
	/* switch test config */
	temp_cmd.len = 5;
	temp_cmd.cmd = ts->switch_cfg_cmd;
	temp_cmd.data[0] = 1;
	ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
	if (ret < 0) {
		ts_err("switch test cfg failed, exit");
		return ret;
	}
	sec_delay(20);

	if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
		ret = goodix_brl_open_test(ts);
	} else if (ts->bus->ic_type == IC_TYPE_BERLIN_D || ts->bus->ic_type == IC_TYPE_GT9916K) {
		ret = goodix_brld_open_test(ts);
	} else {
		ret = goodix_brl_open_test(ts);
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
	}

	if (ret && retry--) {
		ts_info("open test failed, retry[%d]", 2 - retry);
		ts->hw_ops->reset(ts, 200);
		goto restart;
	}

	return ret;
}
static int goodix_brl_sram_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry;
	struct goodix_ts_cmd cmd;
	u8 buf[2];

	cmd.cmd = 0x87;
	cmd.len = 4;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("send SRAM test cmd failed");
		return ret;
	}
	sec_delay(50);

	/* FW 1 test */
	ret = ts->hw_ops->read(ts, 0xD8D0, buf, 1);
	if (ret < 0 || buf[0] != 0xAA) {
		ts_err("FW1 status[0x%02x] != 0xAA", buf[0]);
		return -EINVAL;
	}

	buf[0] = 0;
	ts->hw_ops->write(ts, 0xD8D0, buf, 1);

	retry = 20;
	while (retry--) {
		sec_delay(50);
		ret = ts->hw_ops->read(ts, 0xD8D0, buf, 1);
		if (!ret && buf[0] == 0xBB)
			break;
	}
	if (retry < 0) {
		ts_err("FW1 test not ready, status = 0x%02x", buf[0]);
		return -EINVAL;
	}

	/* FW 2 test */
	buf[0] = 0;
	ts->hw_ops->write(ts, 0xD8D0, buf, 1);

	sec_delay(50);
	ret = ts->hw_ops->read(ts, 0xD8D0, buf, 1);
	if (ret < 0 || buf[0] != 0xAA) {
		ts_err("FW2 status[0x%02x] != 0xAA", buf[0]);
		return -EINVAL;
	}

	buf[0] = 0;
	ts->hw_ops->write(ts, 0xD8D0, buf, 1);

	retry = 20;
	while (retry--) {
		sec_delay(50);
		ret = ts->hw_ops->read(ts, 0xD8D0, buf, 2);
		if (!ret && buf[0] == 0x66)
			break;
	}
	if (retry < 0) {
		ts_err("FW2 test not ready, status = 0x%02x", buf[0]);
		return -EINVAL;
	}

	if (buf[1] == 0) {
		ts_raw_info("SRAM test OK");
		ret = 0;
		ts->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("SRAM test NG");
		ret = -EINVAL;
		ts->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_NG;
	}
	ts->test_data.info[SEC_SRAM_TEST].isFinished = true;

	return ret;
}

static int goodix_brld_sram_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd cmd;
	u8 buf[2];

	cmd.cmd = 0x61;
	cmd.len = 4;
	ret = ts->hw_ops->send_cmd(ts, &cmd);
	if (ret < 0) {
		ts_err("send SRAM test cmd failed");
		return ret;
	}
	sec_delay(50);

	while (retry--) {
		ts->hw_ops->read(ts, 0xD8D0, buf, 2);
		if (buf[0] == 0xAA)
			break;
		sec_delay(40);
	}

	if (retry < 0) {
		ts_err("sram test not ready, status = 0x%02x", buf[0]);
		return -EINVAL;
	}

	if (buf[1] == 0) {
		ts_raw_info("SRAM test OK");
		ret = 0;
		ts->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("SRAM test NG");
		ret = -EINVAL;
		ts->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_NG;
	}
	ts->test_data.info[SEC_SRAM_TEST].isFinished = true;

	return ret;
}

int goodix_sram_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 2;

	ts->test_data.info[SEC_SRAM_TEST].item_name = SRAM_TEST_ITEM;

restart:
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
		ret = goodix_brl_sram_test(ts);
	} else if (ts->bus->ic_type == IC_TYPE_BERLIN_D ||
			ts->bus->ic_type == IC_TYPE_GT9916K) {
		ret = goodix_brld_sram_test(ts);
	} else {
		ret = goodix_brl_sram_test(ts);
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
	}

	if (ret && retry--) {
		ts_info("sram test failed, retry[%d]", 2 - retry);
		ts->hw_ops->reset(ts, 200);
		goto restart;
	}

	return ret;
}

#define SHORT_TEST_FINISH_FLAG			0x88
static int goodix_brl_short_test(struct goodix_ts_data *ts)
{
	int ret = 0;
	int retry;
	u16 test_time;
	u8 status;
	unsigned int time_reg = ts->short_test_time_reg;
	unsigned int result_reg = ts->short_test_status_reg;

	ret = goodix_brl_short_test_prepare(ts);
	if (ret < 0) {
		ts_err("Failed enter short test mode");
		return ret;
	}

	sec_delay(300);

	/* get short test time */
	ret = ts->hw_ops->read(ts, time_reg, (u8 *)&test_time, 2);
	if (ret < 0) {
		ts_err("Failed to get test_time, default %dms", DEFAULT_TEST_TIME_MS);
		test_time = DEFAULT_TEST_TIME_MS;
	} else {
		if (test_time > MAX_TEST_TIME_MS) {
			ts_info("test time too long %d > %d",
					test_time, MAX_TEST_TIME_MS);
					test_time = MAX_TEST_TIME_MS;
		}
		/* if predict time is 0, means fast test pass */
		ts_info("get test time %dms", test_time);
	}

	status = 0;
	ts->hw_ops->write(ts, SHORT_TEST_RUN_REG, &status, 1);

	/* wait short test finish */
	if (test_time > 0)
		sec_delay(test_time);

	retry = 50;
	while (retry--) {
		ret = ts->hw_ops->read(ts, result_reg, &status, 1);
		if (!ret && status == SHORT_TEST_FINISH_FLAG)
			break;
		sec_delay(50);
	}
	if (retry < 0) {
		ts_err("short test failed, status:0x%02x", status);
		return -EINVAL;
	}

	/* start analysis short result */
	ts_info("short_test finished, start analysis");
	ret = goodix_short_analysis(ts);

	return ret;
}

int goodix_short_test(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 2;

	ts->test_data.info[SEC_SHORT_TEST].item_name = SHORT_TEST_ITEM;

restart:
	ret = goodix_brl_short_test(ts);
	if (ret && retry--) {
		ts_info("short test failed, retry[%d]", 2 - retry);
		ts->hw_ops->reset(ts, 200);
		goto restart;
	}

	return ret;
}

#define GOODIX_TOUCH_EVENT				0x80
#define DISCARD_FRAMES_NUM				3
int goodix_read_realtime(struct goodix_ts_data *ts, struct goodix_ts_test_type *test)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd temp_cmd;
	uint32_t flag_addr;
	uint32_t raw_addr;
	uint32_t diff_addr;
	uint32_t addr = 0;
	int tx = ts->ic_info.parm.drv_num;
	int rx = ts->ic_info.parm.sen_num;
	u8 val;

	if (test->type != RAWDATA_TEST_TYPE_MUTUAL_RAW && test->type != RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		ts_err("invalid type: %d", test->type);
		return -EINVAL;
	}

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		/* switch test config */
		temp_cmd.len = 5;
		temp_cmd.cmd = ts->switch_cfg_cmd;
		temp_cmd.data[0] = 1;
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0) {
			ts_err("switch test config failed");
			return ret;
		}
		sec_delay(20);
	}

	if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
		flag_addr = ts->ic_info.misc.touch_data_addr;
		raw_addr = ts->ic_info.misc.mutual_rawdata_addr;
		diff_addr = ts->ic_info.misc.mutual_diffdata_addr;
		temp_cmd.cmd = 0x01;
		temp_cmd.len = 4;
	} else {
		flag_addr = ts->ic_info.misc.frame_data_addr;
		raw_addr = ts->ic_info.misc.frame_data_addr +
				ts->ic_info.misc.frame_data_head_len +
				ts->ic_info.misc.fw_attr_len +
				ts->ic_info.misc.fw_log_len + 8;
		diff_addr = raw_addr;
		if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x81;
			temp_cmd.len = 5;
		} else {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x82;
			temp_cmd.len = 5;
		}
	}
	ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
	if (ret < 0) {
		ts_err("switch rawdata mode failed, exit!");
		goto exit;
	}

	val = 0;
	ts_test_write(ts, flag_addr, &val, 1);

	while (retry--) {
		sec_delay(5);
		ret = ts_test_read(ts, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("rawdata is not ready val:0x%02x, exit!", val);
		ret = -EIO;
		goto exit;
	}

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		addr = raw_addr;
	} else if (test->type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		addr = diff_addr;
	}

	memset(ts->test_data.rawdata.data, 0, sizeof(ts->test_data.rawdata.data));
	ts->test_data.rawdata.size = tx * rx;
	ret = ts_test_read(ts, addr, (u8 *)ts->test_data.rawdata.data,
				ts->test_data.rawdata.size * 2);
	if (ret < 0) {
		ts_err("failed to read rawdata, exit!");
		goto exit;
	}
	goodix_rotate_abcd2cbad(tx, rx, ts->test_data.rawdata.data);

	val = 0;
	ts_test_write(ts, flag_addr, &val, 1);

exit:
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
		temp_cmd.len = 4;
		temp_cmd.cmd = 0x00;
	} else {
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0;
	}
	ts->hw_ops->send_cmd(ts, &temp_cmd);

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		temp_cmd.len = 5;
		temp_cmd.cmd = ts->switch_cfg_cmd;
		temp_cmd.data[0] = 0;
		ts->hw_ops->send_cmd(ts, &temp_cmd);
	}

	ts->test_data.type = test->type;
	ts->test_data.frequency_flag = test->frequency_flag;

	return ret;
}

int goodix_snr_test(struct goodix_ts_data *ts, int type, int frames)
{
	int ret;
	int retry = 50;
	struct goodix_ts_cmd temp_cmd;
	unsigned char temp_buf[58];
	int i;
	unsigned char snr_cmd = ts->snr_cmd;
	unsigned int snr_addr = ts->production_test_addr;

	if (type == SNR_TEST_NON_TOUCH) {
		ts_info("run_snr_non_touched, %d", frames);
		temp_cmd.len = 7;
		temp_cmd.cmd = snr_cmd;
		temp_cmd.data[0] = 0x01;
		temp_cmd.data[1] = (u8)frames;
		temp_cmd.data[2] = (u8)(frames >> 8);
		memset(ts->test_data.snr_result, 0, sizeof(ts->test_data.snr_result));
	} else if (type == SNR_TEST_TOUCH) {
		ts_info("run_snr_touched, %d", frames);
		temp_cmd.len = 7;
		temp_cmd.cmd = snr_cmd;
		temp_cmd.data[0] = 0x02;
		temp_cmd.data[1] = (u8)frames;
		temp_cmd.data[2] = (u8)(frames >> 8);
	} else {
		ts_err("abnormal type(%d)", type);
		return -EINVAL;
	}

	ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
	if (ret < 0) {
		ts_err("send snr_test cmd failed");
		return ret;
	}

	sec_delay(frames * 10);

	while (retry--) {
		sec_delay(100);
		ret = ts->hw_ops->read(ts, snr_addr, temp_buf, 2);
		if (ret < 0) {
			ts_err("read snr status failed");
			return ret;
		}
		if (type == SNR_TEST_NON_TOUCH) {
			if (temp_buf[0] == 0xAA && temp_buf[1] == 0xAA)
				break;
		} else if (type == SNR_TEST_TOUCH) {
			if (temp_buf[0] == 0xBB && temp_buf[1] == 0xBB)
				break;
		}
	}
	if (retry < 0) {
		ts_err("can't read valid snr status:%x%x", temp_buf[0], temp_buf[1]);
		return -EINVAL;
	}

	if (type == SNR_TEST_NON_TOUCH) {
		ts_info("run_snr_non_touched, frames(%d): OK", frames);
		return 0;

	} else if (type == SNR_TEST_TOUCH) {

		ret = ts->hw_ops->read(ts, snr_addr, temp_buf, sizeof(temp_buf));
		if (ret < 0) {
			ts_err("read snr touched data failed");
			return ret;
		}

		if (checksum_cmp(temp_buf + 2, sizeof(temp_buf) - 2, CHECKSUM_MODE_U8_LE)) {
			ts_err("snr touched data checksum error");
			ts_err("data:%*ph", (int)sizeof(temp_buf) - 2, temp_buf + 2);
			return -EINVAL;
		}

		for (i = 0; i < 9; i++) {
			ts->test_data.snr_result[i * 3] = (s16)le16_to_cpup((__le16 *)&temp_buf[2 + i * 6]);
			ts->test_data.snr_result[i * 3 + 1] = (s16)le16_to_cpup((__le16 *)&temp_buf[4 + i * 6]);
			ts->test_data.snr_result[i * 3 + 2] = (s16)le16_to_cpup((__le16 *)&temp_buf[6 + i * 6]);
			ts_raw_info("run_snr_touched, frame(%d) #%d: avg:%d snr1:%d snr2:%d",
						frames, i, ts->test_data.snr_result[i * 3],
						ts->test_data.snr_result[i * 3 + 1], ts->test_data.snr_result[i * 3 + 2]);
		}

		return 0;
	}
	ts_err("abnormal type(%d)", type);
	return -EINVAL;
}

int goodix_cache_rawdata(struct goodix_ts_data *ts, struct goodix_ts_test_type *test, int16_t *data)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd temp_cmd;
	unsigned int freq_cmd = ts->switch_freq_cmd;
	uint32_t flag_addr;
	uint32_t raw_addr;
	uint32_t diff_addr;
	uint32_t selfraw_addr;
	uint32_t selfdiff_addr;
	uint32_t addr = 0;
	int tx = 0;
	int rx = 0;
	u8 val;
	int i;
	int size;

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW || test->type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		memset(&temp_cmd, 0x00, sizeof(struct goodix_ts_cmd));
		/* switch test config */
		temp_cmd.len = 5;
		temp_cmd.cmd = ts->switch_cfg_cmd;
		temp_cmd.data[0] = 1;
		ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
		if (ret < 0) {
			ts_err("switch test config failed");
			return ret;
		}
		sec_delay(20);
	}

	memset(&temp_cmd, 0x00, sizeof(struct goodix_ts_cmd));

	tx = ts->ic_info.parm.drv_num;
	rx = ts->ic_info.parm.sen_num;
	if (ts->bus->ic_type == IC_TYPE_BERLIN_B) {
		flag_addr = ts->ic_info.misc.touch_data_addr;
		raw_addr = ts->ic_info.misc.mutual_rawdata_addr;
		diff_addr = ts->ic_info.misc.mutual_diffdata_addr;
		selfraw_addr = ts->ic_info.misc.self_rawdata_addr;
		selfdiff_addr = ts->ic_info.misc.self_diffdata_addr;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0x84;
		temp_cmd.len = 5;
	} else if (ts->bus->ic_type == IC_TYPE_BERLIN_D || ts->bus->ic_type == IC_TYPE_GT9916K) {
		flag_addr = ts->ic_info.misc.frame_data_addr;
		raw_addr = ts->ic_info.misc.frame_data_addr +
				ts->ic_info.misc.frame_data_head_len +
				ts->ic_info.misc.fw_attr_len +
				ts->ic_info.misc.fw_log_len + 8;
		selfraw_addr = ts->ic_info.misc.frame_data_addr +
				ts->ic_info.misc.frame_data_head_len +
				ts->ic_info.misc.fw_attr_len +
				ts->ic_info.misc.fw_log_len +
				ts->ic_info.misc.mutual_struct_len + 10;
		diff_addr = raw_addr;
		selfdiff_addr = selfraw_addr;
		if (test->type == RAWDATA_TEST_TYPE_SELF_RAW || test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x84;
			temp_cmd.len = 5;
		} else {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x82;
			temp_cmd.len = 5;
		}
	} else {
		ts_err("abnormal tsp ic type(%d)", ts->bus->ic_type);
		flag_addr = ts->ic_info.misc.touch_data_addr;
		raw_addr = ts->ic_info.misc.mutual_rawdata_addr;
		diff_addr = ts->ic_info.misc.mutual_diffdata_addr;
		selfraw_addr = ts->ic_info.misc.self_rawdata_addr;
		selfdiff_addr = ts->ic_info.misc.self_diffdata_addr;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0x84;
		temp_cmd.len = 5;
	}


	ret = ts->hw_ops->send_cmd(ts, &temp_cmd);
	if (ret < 0) {
		ts_err("switch rawdata mode failed, exit!");
		return ret;
	}

	if (test->frequency_flag == FREQ_HIGH) {
		temp_cmd.len = 6;
		temp_cmd.cmd = freq_cmd;
		temp_cmd.data[0] = 0x36;
		temp_cmd.data[1] = 0x13;
		ts->hw_ops->send_cmd(ts, &temp_cmd);
		sec_delay(50);
	} else if (test->frequency_flag == FREQ_LOW) {
		temp_cmd.len = 6;
		temp_cmd.cmd = freq_cmd;
		temp_cmd.data[0] = 0x67;
		temp_cmd.data[1] = 0x06;
		ts->hw_ops->send_cmd(ts, &temp_cmd);
		sec_delay(50);
	}

	/* discard the first few frames */
	for (i = 0; i < DISCARD_FRAMES_NUM; i++) {
		sec_delay(20);
		val = 0;
		ret = ts_test_write(ts, flag_addr, &val, 1);
		if (ret < 0) {
			ts_err("clean touch event failed, exit!");
			return ret;
		}
	}

	while (retry--) {
		sec_delay(5);
		ret = ts_test_read(ts, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("rawdata is not ready val:0x%02x, exit!", val);
		ret = -EIO;
		return ret;
	}

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		size = (tx * rx) * 2;
		addr = raw_addr;
	} else if (test->type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		size = (tx * rx) * 2;
		addr = diff_addr;
	} else if (test->type == RAWDATA_TEST_TYPE_SELF_RAW) {
		size = (tx + rx) * 2;
		addr = selfraw_addr;
	} else if (test->type == RAWDATA_TEST_TYPE_SELF_DIFF) {
		size = (tx + rx) * 2;
		addr = selfdiff_addr;
	} else {
		ts_err("invalid test_type %d", test->type);
		return -EINVAL;
	}

	ts->test_data.type = test->type;
	ts->test_data.frequency_flag = test->frequency_flag;

	ret = ts_test_read(ts, addr, (u8 *)data, size);
	if (ret < 0) {
		ts_err("obtian type[%d] data failed: %d, exit!", test->type, ret);
		return ret;
	}

	if (test->type == RAWDATA_TEST_TYPE_MUTUAL_RAW || test->type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		goodix_rotate_abcd2cbad(tx, rx, data);
	}
	ts_raw_info("test_type:%d, done", test->type);

	return 0;
}


static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct goodix_ts_data *ts;
	struct goodix_ts_test_type test;
	int tx = 0;
	int rx = 0;
	int i;
	int tx_max;
	int tx_min;
	int rx_max;
	int rx_min;
	int max;
	int min;
	s16 *mdata;
	s16 *sdata;
	int numbers;

	if (!m || !v) {
		ts_err("%s : input null ptr", __func__);
		return -EIO;
	}

	ts = m->private;
	if (!ts) {
		ts_err("can't get core data");
		return -EIO;
	}

	/* disable irq */
	ts->hw_ops->irq_enable(ts, false);

	tx = ts->ic_info.parm.drv_num;
	rx = ts->ic_info.parm.sen_num;
	numbers = tx* rx;

	mdata = (s16 *)kzalloc(numbers * 2, GFP_KERNEL);
	if (!mdata) {
		ts_err("failed alloc");
		return 0;
	}

	test.type = RAWDATA_TEST_TYPE_MUTUAL_RAW;
	test.frequency_flag = FREQ_NORMAL;
	goodix_cache_rawdata(ts, &test, mdata);
//	goodix_rotate_abcd2cbad(tx, rx, mdata);
	goodix_data_statistics(mdata, numbers, &max, &min);
	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_printf(m, "mutual_rawdata[%d, %d, %d]:\n", max, min, max - min);
	for (i = 0; i < numbers; i++) {
		seq_printf(m, "%d,", mdata[i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}

	test.type = RAWDATA_TEST_TYPE_MUTUAL_DIFF;
	test.frequency_flag = FREQ_NORMAL;
	memset(mdata, 0x00, numbers * 2);
	goodix_cache_rawdata(ts, &test, mdata);
//	goodix_rotate_abcd2cbad(tx, rx, mdata);
	goodix_data_statistics(mdata, numbers, &max, &min);
	seq_printf(m, "mutual_diffdata[%d, %d]:\n", max, min);
	for (i = 0; i < numbers; i++) {
		seq_printf(m, "%3d,", mdata[i]);
		if ((i + 1) % tx == 0)
			seq_puts(m, "\n");
	}
	kfree(mdata);

	numbers = tx + rx;
	sdata = (s16 *)kzalloc(numbers * 2, GFP_KERNEL);
	if (!sdata) {
		ts_err("failed alloc");
		return 0;
	}

	test.type = RAWDATA_TEST_TYPE_SELF_RAW;
	test.frequency_flag = FREQ_NORMAL;
	goodix_cache_rawdata(ts, &test, sdata);
	goodix_data_statistics(sdata, tx, &tx_max, &tx_min);
	goodix_data_statistics(&sdata[tx], rx, &rx_max, &rx_min);
	seq_printf(m, "self_rawdata_tx[%d, %d]:\n",	tx_max, tx_min);
	for (i = 0; i < tx; i++)
		seq_printf(m, "%d,", sdata[i]);
	seq_printf(m, "\nself_rawdata_rx[%d, %d]:\n", rx_max, rx_min);
	for (i = tx; i < numbers; i++)
		seq_printf(m, "%d,", sdata[i]);
	seq_puts(m, "\n");

	test.type = RAWDATA_TEST_TYPE_SELF_DIFF;
	test.frequency_flag = FREQ_NORMAL;
	memset(sdata, 0x00, numbers * 2);
	goodix_cache_rawdata(ts, &test, sdata);
	goodix_data_statistics(sdata, numbers, &tx_max, &tx_min);
	seq_printf(m, "self_diffdata[%d, %d]:\n", tx_max, tx_min);
	for (i = 0; i < numbers; i++)
		seq_printf(m, "%d,", sdata[i]);
	seq_puts(m, "\n");
	kfree(sdata);

	ts->hw_ops->reset(ts, 100);
	/* jitter test */
	goodix_jitter_test(ts, JITTER_100_FRAMES);
	goodix_jitter_test(ts, JITTER_1000_FRAMES);
	/* open test */
	goodix_open_test(ts);
	/* SRAM test */
	goodix_sram_test(ts);
	ts->hw_ops->reset(ts, 100);
	/* short test */
	goodix_short_test(ts);

	for (i = 0; i < SEC_TEST_MAX_ITEM; i++) {
		if (!ts->test_data.info[i].isFinished)
			continue;
		seq_printf(m, "[%d]%s: ", i, ts->test_data.info[i].item_name);
		if (i == SEC_JITTER1_TEST) {
			seq_printf(m, "mutualDiff[%d %d] ",
					ts->test_data.info[i].data[1], ts->test_data.info[i].data[0]);
			seq_printf(m, "selfDiff[%d %d]\n",
					ts->test_data.info[i].data[3], ts->test_data.info[i].data[2]);
		} else if (i == SEC_JITTER2_TEST) {
			seq_printf(m, "min_matrix[%d %d] ",
					ts->test_data.info[i].data[0], ts->test_data.info[i].data[1]);
			seq_printf(m, "max_matrix[%d %d] ",
					ts->test_data.info[i].data[2], ts->test_data.info[i].data[3]);
			seq_printf(m, "avg_matrix[%d %d]\n",
					ts->test_data.info[i].data[4], ts->test_data.info[i].data[5]);
		} else {
			seq_printf(m, "%s\n",
					ts->test_data.info[i].data[0] == SEC_TEST_OK ? "OK" : "NG");
		}
	}

	/* reset IC */
	ts->hw_ops->reset(ts, 100);
	ts->hw_ops->irq_enable(ts, true);

	return 0;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
	return single_open_size(file, rawdata_proc_show, pde_data(inode), PAGE_SIZE * 10);
#else
	return single_open_size(file, rawdata_proc_show, PDE_DATA(inode), PAGE_SIZE * 10);
#endif
}

#if (KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE)
static const struct proc_ops rawdata_proc_fops = {
	.proc_open = rawdata_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations rawdata_proc_fops = {
	.open = rawdata_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int inspect_module_init(void)
{
	struct proc_dir_entry *proc_entry;

	proc_entry = proc_create_data(PROC_NODE_NAME, 0664, NULL,
					&rawdata_proc_fops, goodix_modules.ts);
	if (!proc_entry)
		ts_err("failed to create proc entry: %s", PROC_NODE_NAME);

	module_initialized = true;
	ts_info("inspect module init success");
	return 0;
}

void inspect_module_exit(void)
{
	ts_info("inspect module exit");
	if (!module_initialized)
		return;

	remove_proc_entry(PROC_NODE_NAME, NULL);
	module_initialized = false;
}