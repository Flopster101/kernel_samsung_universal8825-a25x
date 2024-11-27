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
#define DRV_CHANNEL_FLAG		    		0x80

extern struct goodix_module goodix_modules;
static bool module_initialized;

static u32 short_parms[] = {10, 800, 800, 800, 800, 800, 30};

typedef struct __attribute__((packed)) {
	u8 result;
	u8 drv_drv_num;
	u8 sen_sen_num;
	u8 drv_sen_num;
	u8 drv_gnd_avdd_num;
	u8 sen_gnd_avdd_num;
	u16 checksum;
} test_result_t;

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

static int ts_test_read(struct goodix_ts_core *cd, unsigned int addr,
		 unsigned char *data, unsigned int len)
{
	return cd->bus->read(cd->bus->dev, addr, data, len);
}

static int ts_test_write(struct goodix_ts_core *cd, unsigned int addr,
		unsigned char *data, unsigned int len)
{
	return cd->bus->write(cd->bus->dev, addr, data, len);
}

static void goodix_data_statistics(s16 *data, int data_size,
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

static int cal_cha_to_cha_res(struct goodix_ts_core *cd, int v1, int v2)
{
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B)
		return (v1 - v2) * 74 / v2 + 20;
	else if (cd->bus->ic_type == IC_TYPE_BERLIN_D)
		return (v1 / v2 - 1) * 55 + 45;
	else {
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
		return (v1 - v2) * 74 / v2 + 20;
	}
}

static int cal_cha_to_avdd_res(struct goodix_ts_core *cd, int v1, int v2)
{
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B)
		return 64 * (2 * v2 - 25) * 99 / v1 - 60;
	else if (cd->bus->ic_type == IC_TYPE_BERLIN_D)
		return 64 * (2 * v2 - 25) * 93 / v1 - 20;
	else {
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
		return 64 * (2 * v2 - 25) * 99 / v1 - 60;
	}
}

static int cal_cha_to_gnd_res(struct goodix_ts_core *cd, int v)
{
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B)
		return 150500 / v - 60;
	else if (cd->bus->ic_type == IC_TYPE_BERLIN_D)
		return 120000 / v - 16;
	else {
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
		return 150500 / v - 60;
	}
}

static u32 map_die2pin(struct goodix_ts_core *cd, u32 chn_num)
{
	int i = 0;
	u32 res = 255;
	int max_drv_num = cd->max_drv_num;
	int max_sen_num = cd->max_sen_num;

	if (chn_num & DRV_CHANNEL_FLAG)
		chn_num = (chn_num & ~DRV_CHANNEL_FLAG) + max_sen_num;

	for (i = 0; i < max_sen_num; i++) {
		if ((u8)cd->sen_map[i] == chn_num) {
			res = i;
			break;
		}
	}
	/* res != 255 mean found the corresponding channel num */
	if (res != 255)
		return res;
	/* if cannot find in SenMap try find in DrvMap */
	for (i = 0; i < max_drv_num; i++) {
		if ((u8)cd->drv_map[i] == chn_num) {
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
static int goodix_brl_short_test_prepare(struct goodix_ts_core *cd)
{
	struct goodix_ts_cmd tmp_cmd;
	int ret;
	int retry = 5;
	u8 status;

	ts_info("short test prepare IN");
	tmp_cmd.len = 4;
	tmp_cmd.cmd = INSPECT_FW_SWITCH_CMD;
	ret = cd->hw_ops->send_cmd(cd, &tmp_cmd);
	if (ret < 0) {
		ts_err("send test mode failed");
		return ret;
	}

	while (retry--) {
		sec_delay(40);
		ret = cd->hw_ops->read(cd, SHORT_TEST_RUN_REG, &status, 1);
		if (!ret && status == SHORT_TEST_RUN_FLAG)
			return 0;
		ts_info("short_mode_status=0x%02x ret=%d", status, ret);
	}

	return -EINVAL;
}

static int gdix_check_tx_tx_shortcircut(struct goodix_ts_core *cd,
        u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf;
	u32 data_reg = cd->drv_drv_reg;
	int max_drv_num = cd->max_drv_num;
	int max_sen_num = cd->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_drv_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
    /* drv&drv shortcircut check */
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(cd, data_reg, data_buf, size);
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

			short_r = (u32)cal_cha_to_cha_res(cd, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num =
					map_die2pin(cd, short_die_num + max_sen_num);
				slave_pin_num =
					map_die2pin(cd, j + max_sen_num);
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

				/* save fail ch on cd->test_data.open_short_test_trx_result[] */
				{
					int offset, num;
					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
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

static int gdix_check_rx_rx_shortcircut(struct goodix_ts_core *cd,
        u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;
	u8 *data_buf;
	u32 data_reg = cd->sen_sen_reg;
	int max_sen_num = cd->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_sen_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&drv shortcircut check */
    for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(cd, data_reg, data_buf, size);
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

			short_r = (u32)cal_cha_to_cha_res(cd, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(cd, short_die_num);
				slave_pin_num = map_die2pin(cd, j);
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

				/* save fail ch on cd->test_data.open_short_test_trx_result[] */
				{
					int offset, num;
					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
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

static int gdix_check_tx_rx_shortcircut(struct goodix_ts_core *cd,
        u8 short_ch_num)
{
	int ret = 0, err = 0;
	u32 r_threshold = 0, short_r = 0;	
	int size = 0, i = 0, j = 0;
	u16 adc_signal = 0;
	u8 master_pin_num, slave_pin_num;	
	u8 *data_buf = NULL;
	u32 data_reg = cd->drv_sen_reg;
	int max_drv_num = cd->max_drv_num;
	int max_sen_num = cd->max_sen_num;
	u16 self_capdata, short_die_num = 0;

	size = 4 + max_drv_num * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* drv&sen shortcircut check */
	for (i = 0; i < short_ch_num; i++) {
		ret = ts_test_read(cd, data_reg, data_buf, size);
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

			short_r = (u32)cal_cha_to_cha_res(cd, self_capdata, adc_signal);
			if (short_r < r_threshold) {
				master_pin_num = map_die2pin(cd, short_die_num);
				slave_pin_num = map_die2pin(cd, j + max_sen_num);
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

				/* save fail ch on cd->test_data.open_short_test_trx_result[] */
				{
					int offset, num;
					offset = (master_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (master_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (master_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
					ts_err("master_pin_num=%d, offset=%d, num=%d", master_pin_num, offset, num);

					offset = (slave_pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
					offset += (slave_pin_num & ~DRV_CHANNEL_FLAG) / 8;
					num = (slave_pin_num & ~DRV_CHANNEL_FLAG) % 8;
					cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
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

static int gdix_check_resistance_to_gnd(struct goodix_ts_core *cd,
        u16 adc_signal, u32 pos)
{
	long r = 0;
	u16 r_th = 0, avdd_value = 0;
	u16 chn_id_tmp = 0;
	u8 pin_num = 0;
	unsigned short short_type;
	int max_drv_num = cd->max_drv_num;
	int max_sen_num = cd->max_sen_num;

	avdd_value = short_parms[6];
	short_type = adc_signal & 0x8000;
	adc_signal &= ~0x8000;
	if (adc_signal == 0)
		adc_signal = 1;

	if (short_type == 0) {
		/* short to GND */
		r = cal_cha_to_gnd_res(cd, adc_signal);
	} else {
		/* short to VDD */
		r = cal_cha_to_avdd_res(cd, adc_signal, avdd_value);
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
		pin_num = map_die2pin(cd, chn_id_tmp);
		ts_err("%s%d shortcircut to %s,R=%ldK,R_Threshold=%dK",
				(pin_num & DRV_CHANNEL_FLAG) ? "DRV" : "SEN",
				(pin_num & ~DRV_CHANNEL_FLAG),
				short_type ? "VDD" : "GND",
				r, r_th);

		/* save fail ch on cd->test_data.open_short_test_trx_result[] */
		{
			int offset, num;
			offset = (pin_num & DRV_CHANNEL_FLAG) ? 0 : DRV_CHAN_BYTES;
			offset += (pin_num & ~DRV_CHANNEL_FLAG) / 8;
			num = (pin_num & ~DRV_CHANNEL_FLAG) % 8;
			cd->test_data.open_short_test_trx_result[offset] |= (1 << num);
			ts_err("pin_num=%d, offset=%d, num=%d", pin_num, offset, num);
		}

		return -EINVAL;
	}

	return 0;
}

static int gdix_check_gndvdd_shortcircut(struct goodix_ts_core *cd)
{
	int ret = 0, err = 0;
	int size = 0, i = 0;
	u16 adc_signal = 0;
	u32 data_reg = cd->diff_code_reg;
	u8 *data_buf = NULL;
	int max_drv_num = cd->max_drv_num;
	int max_sen_num = cd->max_sen_num;

	size = (max_drv_num + max_sen_num) * 2 + 2;
	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf) {
		ts_err("Failed to alloc memory");
		return -ENOMEM;
	}
	/* read diff code, diff code will be used to calculate
		* resistance between channel and GND */
	ret = ts_test_read(cd, data_reg, data_buf, size);
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
		ret = gdix_check_resistance_to_gnd(cd, adc_signal, i);
		if (ret != 0) {
			ts_err("Resistance to-gnd/vdd short");
			err = ret;
		}
	}

err_out:
	kfree(data_buf);
	return err;
}

static int goodix_short_analysis(struct goodix_ts_core *cd)
{
	int ret;
	int err = 0;
	test_result_t test_result;
	unsigned int result_reg = cd->short_test_result_reg;

	ret = cd->hw_ops->read(cd, result_reg,
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
		cd->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("short flag 0x%02x, drv&drv:%d, sen&sen:%d, drv&sen:%d, drv/GNDVDD:%d, sen/GNDVDD:%d",
			test_result.result, test_result.drv_drv_num, test_result.sen_sen_num,
			test_result.drv_sen_num, test_result.drv_gnd_avdd_num, test_result.sen_gnd_avdd_num);		
		/* get short channel and resistance */
		if (test_result.drv_drv_num)
			err |= gdix_check_tx_tx_shortcircut(cd, test_result.drv_drv_num);
		if (test_result.sen_sen_num)
			err |= gdix_check_rx_rx_shortcircut(cd, test_result.sen_sen_num);
		if (test_result.drv_sen_num)
			err |= gdix_check_tx_rx_shortcircut(cd, test_result.drv_sen_num);
		if (test_result.drv_gnd_avdd_num || test_result.sen_gnd_avdd_num)
			err |= gdix_check_gndvdd_shortcircut(cd);
		if (err)
			cd->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_NG;
		else
			cd->test_data.info[SEC_SHORT_TEST].data[0] = SEC_TEST_OK;
	}
	cd->test_data.info[SEC_SHORT_TEST].isFinished = true;

	if (err)
		return -EINVAL;

	return 0;
}

#define JITTER_LEN     24
static int goodix_brl_jitter_test(struct goodix_ts_core *cd, u8 type)
{
	int ret;
	int retry;
	struct goodix_ts_cmd cmd;
	struct goodix_jitter_data raw_data;
	unsigned int jitter_addr = cd->production_test_addr;
	u8 buf[JITTER_LEN];

	/* switch test config */
		cmd.len = 5;
	cmd.cmd = cd->switch_cfg_cmd;
	cmd.data[0] = 1;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
	if (ret < 0) {
		ts_err("switch test config failed");
		return ret;
	}

	sec_delay(20);

	cmd.cmd = 0x88;
	cmd.data[0] = type;
	cmd.len = 5;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
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
		ret = cd->hw_ops->read(cd, jitter_addr, buf, (int)sizeof(buf));
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
		cd->test_data.info[SEC_JITTER1_TEST].data[0] = raw_data.mutualDiffMax;
		cd->test_data.info[SEC_JITTER1_TEST].data[1] = raw_data.mutualDiffMin;
		cd->test_data.info[SEC_JITTER1_TEST].data[2] = raw_data.selfDiffMax;
		cd->test_data.info[SEC_JITTER1_TEST].data[3] = raw_data.selfDiffMin;
		cd->test_data.info[SEC_JITTER1_TEST].isFinished = true;
		ts_raw_info("mutualdiff[%d, %d] selfdiff[%d, %d]",
				raw_data.mutualDiffMin, raw_data.mutualDiffMax,
				raw_data.selfDiffMin, raw_data.selfDiffMax);
	} else {
		if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
			raw_data.min_of_min_matrix = le16_to_cpup((__le16 *)&buf[10]);
			raw_data.max_of_min_matrix = le16_to_cpup((__le16 *)&buf[12]);
			raw_data.min_of_max_matrix = le16_to_cpup((__le16 *)&buf[14]);
			raw_data.max_of_max_matrix = le16_to_cpup((__le16 *)&buf[16]);
			raw_data.min_of_avg_matrix = le16_to_cpup((__le16 *)&buf[18]);
			raw_data.max_of_avg_matrix = le16_to_cpup((__le16 *)&buf[20]);
		} else if (cd->bus->ic_type == IC_TYPE_BERLIN_D) {
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
			ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
		}

		cd->test_data.info[SEC_JITTER2_TEST].data[0] = raw_data.min_of_min_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].data[1] = raw_data.max_of_min_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].data[2] = raw_data.min_of_max_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].data[3] = raw_data.max_of_max_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].data[4] = raw_data.min_of_avg_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].data[5] = raw_data.max_of_avg_matrix;
		cd->test_data.info[SEC_JITTER2_TEST].isFinished = true;
		ts_raw_info("min_matrix[%d, %d] max_matrix[%d, %d] avg_matrix[%d, %d]",
				raw_data.min_of_min_matrix, raw_data.max_of_min_matrix,
				raw_data.min_of_max_matrix, raw_data.max_of_max_matrix,
				raw_data.min_of_avg_matrix, raw_data.max_of_avg_matrix);
	}

	return 0;
}

int goodix_jitter_test(struct goodix_ts_core *cd, u8 type)
{
	int ret;

	if (type == JITTER_100_FRAMES)
		cd->test_data.info[SEC_JITTER1_TEST].item_name = JITTER1_TEST_ITEM;
	else
		cd->test_data.info[SEC_JITTER2_TEST].item_name = JITTER2_TEST_ITEM;

	ret = goodix_brl_jitter_test(cd, type);
	if (ret < 0)
		ts_err("jitter test process failed");
	return ret;
}

static int goodix_brl_open_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry = 20;
	int i;
	struct goodix_ts_cmd cmd;
	unsigned char buf[OPEN_TEST_RESULT_LEN];

	cmd.cmd = 0x89;
	cmd.len = 4;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
	if (ret < 0) {
		ts_err("send open test cmd failed");
		return ret;
	}
	sec_delay(400);
	while (retry--) {
		ret = cd->hw_ops->read(cd, OPEN_TEST_RESULT, buf, (int)sizeof(buf));
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
		cd->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("open test failed");

		/* save result data */
		for (i = 0; i < OPEN_SHORT_TEST_RESULT_LEN ; i++) {
			cd->test_data.open_short_test_trx_result[i] =  buf[3 + i];
		}

		cd->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_NG;
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
	cd->test_data.info[SEC_OPEN_TEST].isFinished = true;

	return ret;
}

static int goodix_brld_open_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry = 20;
	int i;
	struct goodix_ts_cmd cmd;
	unsigned char buf[24];
	unsigned int result_addr = cd->production_test_addr;

	cmd.cmd = 0x63;
	cmd.len = 4;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
	if (ret < 0) {
		ts_err("send open test cmd failed");
		return ret;
	}
	sec_delay(200);

	while (retry--) {
		ret = cd->hw_ops->read(cd, result_addr, buf, (int)sizeof(buf));
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
		cd->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("open test failed");

		/* save result data */
		for (i = 0; i < OPEN_SHORT_TEST_RESULT_LEN ; i++) {
			cd->test_data.open_short_test_trx_result[i] =  buf[3 + i];
		}

		cd->test_data.info[SEC_OPEN_TEST].data[0] = SEC_TEST_NG;
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

	cd->test_data.info[SEC_OPEN_TEST].isFinished = true;

	return ret;
}

int goodix_open_test(struct goodix_ts_core *cd)
{
	int ret = 0;
	int retry = 2;
	struct goodix_ts_cmd temp_cmd;

	cd->test_data.info[SEC_OPEN_TEST].item_name = OPEN_TEST_ITEM;

restart:
	/* switch test config */
	temp_cmd.len = 5;
	temp_cmd.cmd = cd->switch_cfg_cmd;
	temp_cmd.data[0] = 1;
	ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("switch test cfg failed, exit");
		return ret;
	}
	sec_delay(20);

	if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
		ret = goodix_brl_open_test(cd);
	} else if (cd->bus->ic_type == IC_TYPE_BERLIN_D) {
		ret = goodix_brld_open_test(cd);
	} else {
		ret = goodix_brl_open_test(cd);
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
	}

	if (ret && retry--) {
		ts_info("open test failed, retry[%d]", 2 - retry);
		cd->hw_ops->reset(cd, 200);
		goto restart;
	}

	return ret;
}
static int goodix_brl_sram_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry;
	struct goodix_ts_cmd cmd;
	u8 buf[2];

	cmd.cmd = 0x87;
	cmd.len = 4;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
	if (ret < 0) {
		ts_err("send SRAM test cmd failed");
		return ret;
	}
	sec_delay(50);

	/* FW 1 test */
	ret = cd->hw_ops->read(cd, 0xD8D0, buf, 1);
	if (ret < 0 || buf[0] != 0xAA) {
		ts_err("FW1 status[0x%02x] != 0xAA", buf[0]);
		return -EINVAL;
	}

	buf[0] = 0;
	cd->hw_ops->write(cd, 0xD8D0, buf, 1);

	retry = 20;
	while (retry--) {
		sec_delay(50);
		ret = cd->hw_ops->read(cd, 0xD8D0, buf, 1);
		if (!ret && buf[0] == 0xBB)
			break;
	}
	if (retry < 0) {
		ts_err("FW1 test not ready, status = 0x%02x", buf[0]);
		return -EINVAL;
	}

	/* FW 2 test */
	buf[0] = 0;
	cd->hw_ops->write(cd, 0xD8D0, buf, 1);

	sec_delay(50);
	ret = cd->hw_ops->read(cd, 0xD8D0, buf, 1);
	if (ret < 0 || buf[0] != 0xAA) {
		ts_err("FW2 status[0x%02x] != 0xAA", buf[0]);
		return -EINVAL;
	}

	buf[0] = 0;
	cd->hw_ops->write(cd, 0xD8D0, buf, 1);

	retry = 20;
	while (retry--) {
		sec_delay(50);
		ret = cd->hw_ops->read(cd, 0xD8D0, buf, 2);
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
		cd->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("SRAM test NG");
		ret = -EINVAL;
		cd->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_NG;
	}
	cd->test_data.info[SEC_SRAM_TEST].isFinished = true;

	return ret;
}

static int goodix_brld_sram_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd cmd;
	u8 buf[2];

	cmd.cmd = 0x61;
	cmd.len = 4;
	ret = cd->hw_ops->send_cmd(cd, &cmd);
	if (ret < 0) {
		ts_err("send SRAM test cmd failed");
		return ret;
	}
	sec_delay(50);

	while (retry--) {
		ret = cd->hw_ops->read(cd, 0xD8D0, buf, 2);
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
		cd->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_OK;
	} else {
		ts_raw_info("SRAM test NG");
		ret = -EINVAL;
		cd->test_data.info[SEC_SRAM_TEST].data[0] = SEC_TEST_NG;
	}
	cd->test_data.info[SEC_SRAM_TEST].isFinished = true;

	return ret;
}

int goodix_sram_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry = 2;

	cd->test_data.info[SEC_SRAM_TEST].item_name = SRAM_TEST_ITEM;

restart:
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
		ret = goodix_brl_sram_test(cd);
	} else if (cd->bus->ic_type == IC_TYPE_BERLIN_D) {
		ret = goodix_brld_sram_test(cd);
	} else {
		ret = goodix_brl_sram_test(cd);
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
	}

	if (ret && retry--) {
		ts_info("sram test failed, retry[%d]", 2 - retry);
		cd->hw_ops->reset(cd, 200);
		goto restart;
	}

	return ret;
}

#define SHORT_TEST_FINISH_FLAG			0x88
static int goodix_brl_short_test(struct goodix_ts_core *cd)
{
	int ret = 0;
	int retry;
	u16 test_time;
	u8 status;
	unsigned int time_reg = cd->short_test_time_reg;
	unsigned int result_reg = cd->short_test_status_reg;

	ret = goodix_brl_short_test_prepare(cd);
	if (ret < 0) {
		ts_err("Failed enter short test mode");
		return ret;
	}

	sec_delay(300);

	/* get short test time */
	ret = cd->hw_ops->read(cd, time_reg, (u8 *)&test_time, 2);
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
	cd->hw_ops->write(cd, SHORT_TEST_RUN_REG, &status, 1);

	/* wait short test finish */
	if (test_time > 0)
		sec_delay(test_time);

	retry = 50;
	while (retry--) {
		ret = cd->hw_ops->read(cd, result_reg, &status, 1);
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
	ret = goodix_short_analysis(cd);

	return ret;
}

int goodix_short_test(struct goodix_ts_core *cd)
{
	int ret;
	int retry = 2;

	cd->test_data.info[SEC_SHORT_TEST].item_name = SHORT_TEST_ITEM;

restart:
	ret = goodix_brl_short_test(cd);
	if (ret && retry--) {
		ts_info("short test failed, retry[%d]", 2 - retry);
		cd->hw_ops->reset(cd, 200);
		goto restart;
	}

	return ret;
}

#define GOODIX_TOUCH_EVENT				0x80
#define DISCARD_FRAMES_NUM				3
int goodix_cache_rawdata(struct goodix_ts_core *cd, int test_type, u8 freq)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd temp_cmd;
	unsigned int freq_cmd = cd->switch_freq_cmd;
	uint32_t flag_addr;
	uint32_t raw_addr;
	uint32_t diff_addr;
	uint32_t selfraw_addr;
	uint32_t selfdiff_addr;
	int tx = 0;
	int rx = 0;
	u8 val;
	int i;

	if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW || test_type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		memset(&temp_cmd, 0x00, sizeof(struct goodix_ts_cmd));
		/* switch test config */
		temp_cmd.len = 5;
		temp_cmd.cmd = cd->switch_cfg_cmd;
		temp_cmd.data[0] = 1;
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0) {
			ts_err("switch test config failed");
			return ret;
		}
		sec_delay(20);
	}

	memset(&temp_cmd, 0x00, sizeof(struct goodix_ts_cmd));

	tx = cd->ic_info.parm.drv_num;
	rx = cd->ic_info.parm.sen_num;
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
		flag_addr = cd->ic_info.misc.touch_data_addr;
		raw_addr = cd->ic_info.misc.mutual_rawdata_addr;
		diff_addr = cd->ic_info.misc.mutual_diffdata_addr;
		selfraw_addr = cd->ic_info.misc.self_rawdata_addr;
		selfdiff_addr = cd->ic_info.misc.self_diffdata_addr;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0x84;
		temp_cmd.len = 5;
	} else if (cd->bus->ic_type == IC_TYPE_BERLIN_D) {
		flag_addr = cd->ic_info.misc.frame_data_addr;
		raw_addr = 0x104B2;
		selfraw_addr = cd->ic_info.misc.frame_data_addr +
				cd->ic_info.misc.frame_data_head_len +
				cd->ic_info.misc.fw_attr_len +
				cd->ic_info.misc.fw_log_len +
				cd->ic_info.misc.mutual_struct_len + 10;
		diff_addr = raw_addr;
		selfdiff_addr = selfraw_addr;
		if (test_type == RAWDATA_TEST_TYPE_SELF_RAW || test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x84;
			temp_cmd.len = 5;
		} else {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x82;
			temp_cmd.len = 5;
		}
	} else {
		ts_err("abnormal tsp ic type(%d)", cd->bus->ic_type);
		flag_addr = cd->ic_info.misc.touch_data_addr;
		raw_addr = cd->ic_info.misc.mutual_rawdata_addr;
		diff_addr = cd->ic_info.misc.mutual_diffdata_addr;
		selfraw_addr = cd->ic_info.misc.self_rawdata_addr;
		selfdiff_addr = cd->ic_info.misc.self_diffdata_addr;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0x84;
		temp_cmd.len = 5;
	}


	ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("switch rawdata mode failed, exit!");
		return ret;
	}

	if (freq == FREQ_HIGH) {
		temp_cmd.len = 6;
		temp_cmd.cmd = freq_cmd;
		temp_cmd.data[0] = 0x36;
		temp_cmd.data[1] = 0x13;
		cd->hw_ops->send_cmd(cd, &temp_cmd);
		sec_delay(50);
	} else if (freq == FREQ_LOW) {
		temp_cmd.len = 6;
		temp_cmd.cmd = freq_cmd;
		temp_cmd.data[0] = 0x67;
		temp_cmd.data[1] = 0x06;
		cd->hw_ops->send_cmd(cd, &temp_cmd);
		sec_delay(50);
	}

	/* discard the first few frames */
	for (i = 0; i < DISCARD_FRAMES_NUM; i++) {
		sec_delay(20);
		val = 0;
		ret = ts_test_write(cd, flag_addr, &val, 1);
		if (ret < 0) {
			ts_err("clean touch event failed, exit!");
			return ret;
		}
	}

	while (retry--) {
		sec_delay(5);
		ret = ts_test_read(cd, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("rawdata is not ready val:0x%02x, exit!", val);
		ret = -EIO;
		return ret;
	}

	if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		if (freq == FREQ_HIGH) {
			cd->test_data.high_freq_rawdata.size = tx * rx;
			ret = ts_test_read(cd, raw_addr, (u8 *)cd->test_data.high_freq_rawdata.data,
					cd->test_data.high_freq_rawdata.size * 2);
			goodix_rotate_abcd2cbad(tx, rx, cd->test_data.high_freq_rawdata.data);
			goodix_data_statistics(cd->test_data.high_freq_rawdata.data, cd->test_data.high_freq_rawdata.size,
					&cd->test_data.high_freq_rawdata.max, &cd->test_data.high_freq_rawdata.min);
			cd->test_data.high_freq_rawdata.delta = cd->test_data.high_freq_rawdata.max - cd->test_data.high_freq_rawdata.min;
		} else if (freq == FREQ_LOW) {
			cd->test_data.low_freq_rawdata.size = tx * rx;
			ret = ts_test_read(cd, raw_addr, (u8 *)cd->test_data.low_freq_rawdata.data,
					cd->test_data.low_freq_rawdata.size * 2);
			goodix_rotate_abcd2cbad(tx, rx, cd->test_data.low_freq_rawdata.data);
			goodix_data_statistics(cd->test_data.low_freq_rawdata.data, cd->test_data.low_freq_rawdata.size,
					&cd->test_data.low_freq_rawdata.max, &cd->test_data.low_freq_rawdata.min);
			cd->test_data.low_freq_rawdata.delta = cd->test_data.low_freq_rawdata.max - cd->test_data.low_freq_rawdata.min;
		} else {
			/* read raw data */
			cd->test_data.rawdata.size = tx * rx;
			ret = ts_test_read(cd, raw_addr, (u8 *)cd->test_data.rawdata.data,
					cd->test_data.rawdata.size * 2);
			if (ret < 0) {
				ts_err("obtian rawdata failed, exit!");
				return ret;
			}
			goodix_rotate_abcd2cbad(tx, rx, cd->test_data.rawdata.data);
			goodix_data_statistics(cd->test_data.rawdata.data, cd->test_data.rawdata.size,
					&cd->test_data.rawdata.max, &cd->test_data.rawdata.min);
			cd->test_data.rawdata.delta = cd->test_data.rawdata.max - cd->test_data.rawdata.min;
		}
	} else if (test_type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		/* read diff data */
		cd->test_data.diffdata.size = tx * rx;
		ret = ts_test_read(cd, diff_addr, (u8 *)cd->test_data.diffdata.data,
			cd->test_data.diffdata.size * 2);
		if (ret < 0) {
			ts_err("obtian diffdata failed, exit!");
			return ret;
		}
		goodix_rotate_abcd2cbad(tx, rx, cd->test_data.diffdata.data);
		goodix_data_statistics(cd->test_data.diffdata.data, cd->test_data.diffdata.size,
				&cd->test_data.diffdata.max, &cd->test_data.diffdata.min);
	} else if (test_type == RAWDATA_TEST_TYPE_SELF_RAW) {
		/* read selfraw */
		cd->test_data.selfraw.size = tx + rx;
		ret = ts_test_read(cd, selfraw_addr, (u8 *)cd->test_data.selfraw.data,
				cd->test_data.selfraw.size * 2);
		if (ret < 0) {
			ts_err("obtian selfraw data failed, exit!");
			return ret;
		}
		goodix_data_statistics(cd->test_data.selfraw.data, tx,
				&cd->test_data.selfraw.tx_max, &cd->test_data.selfraw.tx_min);
		goodix_data_statistics(cd->test_data.selfraw.data + tx, rx,
				&cd->test_data.selfraw.rx_max, &cd->test_data.selfraw.rx_min);
	} else if (test_type == RAWDATA_TEST_TYPE_SELF_DIFF) {
		/* read selfdiff */
		cd->test_data.selfdiff.size = tx + rx;
		ret = ts_test_read(cd, selfdiff_addr, (u8 *)cd->test_data.selfdiff.data,
				cd->test_data.selfdiff.size * 2);
		if (ret < 0) {
			ts_err("obtian selfdiff data failed, exit!");
			return ret;
		}
		goodix_data_statistics(cd->test_data.selfdiff.data, cd->test_data.selfdiff.size,
				&cd->test_data.selfdiff.tx_max, &cd->test_data.selfdiff.tx_min);
	} else {
		ts_err("invalid test_type %d", test_type);
		return -EINVAL;
	}

	ts_raw_info("test_type:%d, done", test_type);

	return 0;
}

int goodix_read_realtime(struct goodix_ts_core *cd, int test_type)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd temp_cmd;
	uint32_t flag_addr;
	uint32_t raw_addr;
	uint32_t diff_addr;
	int tx = cd->ic_info.parm.drv_num;
	int rx = cd->ic_info.parm.sen_num;
	u8 val;

	if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		/* switch test config */
		temp_cmd.len = 5;
		temp_cmd.cmd = cd->switch_cfg_cmd;
		temp_cmd.data[0] = 1;
		ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
		if (ret < 0) {
			ts_err("switch test config failed");
			return ret;
		}
		sec_delay(20);
	}

	if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
		flag_addr = cd->ic_info.misc.touch_data_addr;
		raw_addr = cd->ic_info.misc.mutual_rawdata_addr;
		diff_addr = cd->ic_info.misc.mutual_diffdata_addr;
		temp_cmd.cmd = 0x01;
		temp_cmd.len = 4;
	} else {
		flag_addr = cd->ic_info.misc.frame_data_addr;
		raw_addr = 0x104B2;
		diff_addr = raw_addr;
		if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x81;
			temp_cmd.len = 5;
		} else {
			temp_cmd.cmd = 0x90;
			temp_cmd.data[0] = 0x82;
			temp_cmd.len = 5;
		}
	}
	ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("switch rawdata mode failed, exit!");
		goto exit;
	}

	val = 0;
	ts_test_write(cd, flag_addr, &val, 1);

	while (retry--) {
		sec_delay(5);
		ret = ts_test_read(cd, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("rawdata is not ready val:0x%02x, exit!", val);
		ret = -EIO;
		goto exit;
	}

	if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		memset(cd->test_data.rawdata.data, 0, sizeof(cd->test_data.rawdata.data));
		cd->test_data.rawdata.size = tx * rx;
		ts_test_read(cd, raw_addr, (u8 *)cd->test_data.rawdata.data,
				cd->test_data.rawdata.size * 2);
		goodix_rotate_abcd2cbad(tx, rx, cd->test_data.rawdata.data);
	} else if (test_type == RAWDATA_TEST_TYPE_MUTUAL_DIFF) {
		memset(cd->test_data.diffdata.data, 0, sizeof(cd->test_data.diffdata.data));
		cd->test_data.diffdata.size = tx * rx;
		ts_test_read(cd, diff_addr, (u8 *)cd->test_data.diffdata.data,
			cd->test_data.diffdata.size * 2);
		goodix_rotate_abcd2cbad(tx, rx, cd->test_data.diffdata.data);
	} else {
		ts_err("invalid test_type %d", test_type);
		ret = -EINVAL;
	}

	val = 0;
	ts_test_write(cd, flag_addr, &val, 1);

exit:
	if (cd->bus->ic_type == IC_TYPE_BERLIN_B) {
		temp_cmd.len = 4;
		temp_cmd.cmd = 0x00;
	} else {
		temp_cmd.len = 5;
		temp_cmd.cmd = 0x90;
		temp_cmd.data[0] = 0;
	}
	cd->hw_ops->send_cmd(cd, &temp_cmd);

	if (test_type == RAWDATA_TEST_TYPE_MUTUAL_RAW) {
		temp_cmd.len = 5;
		temp_cmd.cmd = cd->switch_cfg_cmd;
		temp_cmd.data[0] = 0;
		cd->hw_ops->send_cmd(cd, &temp_cmd);
	}

	return ret;
}

int goodix_snr_test(struct goodix_ts_core *cd, int type, int frames)
{
	int ret;
	int retry = 50;
	struct goodix_ts_cmd temp_cmd;
	unsigned char temp_buf[58];
	int i;
	unsigned char snr_cmd = cd->snr_cmd;
	unsigned int snr_addr = cd->production_test_addr;

	if (type == SNR_TEST_NON_TOUCH) {
		ts_info("run_snr_non_touched, %d", frames);
		temp_cmd.len = 7;
		temp_cmd.cmd = snr_cmd;
		temp_cmd.data[0] = 0x01;
		temp_cmd.data[1] = (u8)frames;
		temp_cmd.data[2] = (u8)(frames >> 8);
		memset(cd->test_data.snr_result, 0, sizeof(cd->test_data.snr_result));
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

	ret = cd->hw_ops->send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("send snr_test cmd failed");
		return ret;
	}

	sec_delay(frames * 10);

	while (retry--) {
		sec_delay(100);
		ret = cd->hw_ops->read(cd, snr_addr, temp_buf, 2);
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

		ret = cd->hw_ops->read(cd, snr_addr, temp_buf, sizeof(temp_buf));
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
			cd->test_data.snr_result[i * 3] = (s16)le16_to_cpup((__le16 *)&temp_buf[2 + i * 6]);
			cd->test_data.snr_result[i * 3 + 1] = (s16)le16_to_cpup((__le16 *)&temp_buf[4 + i * 6]);
			cd->test_data.snr_result[i * 3 + 2] = (s16)le16_to_cpup((__le16 *)&temp_buf[6 + i * 6]);
			ts_raw_info("run_snr_touched, frame(%d) #%d: avg:%d snr1:%d snr2:%d",
						frames, i, cd->test_data.snr_result[i * 3],
						cd->test_data.snr_result[i * 3 + 1], cd->test_data.snr_result[i * 3 + 2]);
		}

		return 0;
	}
	ts_err("abnormal type(%d)", type);
	return -EINVAL;
}

static int goodix_auto_test(struct goodix_ts_core *cd)
{
	int i = 0;

	/* disable irq */
	cd->hw_ops->irq_enable(cd, false);

	memset(&cd->test_data, 0, sizeof(cd->test_data));

	for (i = RAWDATA_TEST_TYPE_MUTUAL_RAW; i <= RAWDATA_TEST_TYPE_SELF_DIFF; i++) {
		/* cache rawdata */
		goodix_cache_rawdata(cd, i, FREQ_NORMAL);
	}
	cd->hw_ops->reset(cd, 100);
	/* jitter test */
	goodix_jitter_test(cd, JITTER_100_FRAMES);
	goodix_jitter_test(cd, JITTER_1000_FRAMES);
	/* open test */
	goodix_open_test(cd);
	/* SRAM test */
	goodix_sram_test(cd);
	cd->hw_ops->reset(cd, 100);
	/* short test */
	goodix_short_test(cd);

	/* reset IC */
	cd->hw_ops->reset(cd, 100);
	cd->hw_ops->irq_enable(cd, true);

	return 0;
}

static int rawdata_proc_show(struct seq_file *m, void *v)
{
	struct goodix_ts_core *cd;
	int tx = 0;
	int rx = 0;
	int i;

	if (!m || !v) {
		ts_err("rawdata_proc_show, input null ptr");
		return -EIO;
	}

	cd = m->private;
	if (!cd) {
		ts_err("can't get core data");
		return -EIO;
	}

	goodix_auto_test(cd);

	tx = cd->ic_info.parm.drv_num;
	rx = cd->ic_info.parm.sen_num;

	seq_printf(m, "TX:%d  RX:%d\n", tx, rx);
	seq_printf(m, "mutual_rawdata[%d, %d, %d]:\n",
			cd->test_data.rawdata.max, cd->test_data.rawdata.min, cd->test_data.rawdata.delta);
	for (i = 0; i < cd->test_data.rawdata.size; i++) {
		seq_printf(m, "%d,", cd->test_data.rawdata.data[i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

	/* print high freq rawdata */
	seq_printf(m, "high_freq_rawdata[%d, %d, %d]:\n",
			cd->test_data.high_freq_rawdata.max, cd->test_data.high_freq_rawdata.min, cd->test_data.high_freq_rawdata.delta);
	for (i = 0; i < cd->test_data.high_freq_rawdata.size; i++) {
		seq_printf(m, "%d,", cd->test_data.high_freq_rawdata.data[i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

	/* print low freq rawdata */
	seq_printf(m, "low_freq_rawdata[%d, %d, %d]:\n",
			cd->test_data.low_freq_rawdata.max, cd->test_data.low_freq_rawdata.min, cd->test_data.low_freq_rawdata.delta);
	for (i = 0; i < cd->test_data.low_freq_rawdata.size; i++) {
		seq_printf(m, "%d,", cd->test_data.low_freq_rawdata.data[i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}	

	seq_printf(m, "mutual_diffdata[%d, %d]:\n",
			cd->test_data.diffdata.max, cd->test_data.diffdata.min);
	for (i = 0; i < cd->test_data.diffdata.size; i++) {
		seq_printf(m, "%3d,", cd->test_data.diffdata.data[i]);
		if ((i + 1) % tx == 0)
			seq_printf(m, "\n");
	}

	seq_printf(m, "self_rawdata_tx[%d, %d]:\n",
			cd->test_data.selfraw.tx_max, cd->test_data.selfraw.tx_min);
	for (i = 0; i < tx; i++)
		seq_printf(m, "%d,", cd->test_data.selfraw.data[i]);
	seq_printf(m, "\nself_rawdata_rx[%d, %d]:\n",
			cd->test_data.selfraw.rx_max, cd->test_data.selfraw.rx_min);
	for (i = tx; i < cd->test_data.selfraw.size; i++)
		seq_printf(m, "%d,", cd->test_data.selfraw.data[i]);
	seq_printf(m, "\n");

	seq_printf(m, "self_diffdata[%d, %d]:\n",
			cd->test_data.selfdiff.tx_max, cd->test_data.selfdiff.tx_min);
	for (i = 0; i < cd->test_data.selfdiff.size; i++)
		seq_printf(m, "%d,", cd->test_data.selfdiff.data[i]);
	seq_printf(m, "\n");

	for (i = 0; i < SEC_TEST_MAX_ITEM; i++) {
		if (!cd->test_data.info[i].isFinished)
			continue;
		seq_printf(m, "[%d]%s: ", i, cd->test_data.info[i].item_name);
		if (i == SEC_JITTER1_TEST) {
			seq_printf(m, "mutualDiff[%d %d] ",
					cd->test_data.info[i].data[1], cd->test_data.info[i].data[0]);
			seq_printf(m, "selfDiff[%d %d]\n",
					cd->test_data.info[i].data[3], cd->test_data.info[i].data[2]);
		} else if (i == SEC_JITTER2_TEST) {
			seq_printf(m, "min_matrix[%d %d] ",
					cd->test_data.info[i].data[0], cd->test_data.info[i].data[1]);
			seq_printf(m, "max_matrix[%d %d] ",
					cd->test_data.info[i].data[2], cd->test_data.info[i].data[3]);
			seq_printf(m, "avg_matrix[%d %d]\n",
					cd->test_data.info[i].data[4], cd->test_data.info[i].data[5]);
		} else {
			seq_printf(m, "%s\n",
					cd->test_data.info[i].data[0] == SEC_TEST_OK ? "OK" : "NG");
		}
	}

	return 0;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, rawdata_proc_show, PDE_DATA(inode), PAGE_SIZE * 10);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
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
					&rawdata_proc_fops, goodix_modules.core_data);
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

