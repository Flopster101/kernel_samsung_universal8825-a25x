/*
 *  sm5714_fuelgauge.c
 *  Samsung sm5714 Fuel Gauge Driver
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define BATTERY_LOG_MESSAGE */
#include "sm5714_fuelgauge.h"
#include <linux/of_gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static enum power_supply_property sm5714_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *sm5714_fg_supplied_to[] = {
	"sm5714-fuelgauge",
};

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MAXVAL(a, b) ((a > b) ? a : b)

#define LIMIT_N_CURR_MIXFACTOR -2000
#define TABLE_READ_COUNT 2
#define FG_ABNORMAL_RESET -1
#define IGNORE_N_I_OFFSET 1

#define SM5714_FG_FULL_DEBUG 1
#define I2C_ERROR_COUNT_MAX 5
#if defined(CONFIG_SEC_FACTORY)
#define FG_REG_CHECK_TABLE_LEN 11
#endif

#define SM5714_FUELGAUGE_VERSION  "WF1"

/* void sm5714_adabt_full_offset(struct sm5714_fuelgauge_data *fuelgauge); */
static bool sm5714_fg_init(struct sm5714_fuelgauge_data *fuelgauge, bool is_surge);


static int sm5714_device_id = -1;
#if !defined(CONFIG_SEC_FACTORY)
static int sm5714_fg_debug_print;
#endif

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);

enum sm5714_battery_table_type {
	DISCHARGE_TABLE = 0,
	SOC_TABLE,
	TABLE_MAX,
};

static int sm5714_regs[] = {
	0x0000, 0x0001, 0x0003, 0x0004, 0x0010, 0x0013, 0x0020, 0x0021, 0x0022, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F
};

static int sm5714_srams[] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A,
	0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x0087, 0x008A, 0x008B,
	0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
	0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
	0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x00D8, 0x00DF, 0x00ED, 0x00EE
};

#if defined(CONFIG_SEC_FACTORY)
static int sm5714_reg_check_table[2][11] = {
	{0x60, 0x61, 0x62, 0x69, 0x6A, 0x6B, 0x6C, 0x6E, 0x6F, 0x7B, 0x7C},
	{0x0800, 0x0000, 0x0800, 0x0000, 0x0800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0733, 0x0002},
};
#endif

static int sm5714_get_lpmode(void) { return lpcharge; }
static unsigned int sm5714_get_facmode(void) { return factory_mode; }

static int sm5714_fg_find_addr(int addr)
{
	int r_size = (int)ARRAY_SIZE(sm5714_regs);
	int i;

	for (i = 0; i < r_size; i++) {
		if (sm5714_regs[i] == addr) {
			pr_info("%s: addr: 0x%x == 0x%x is founded\n", __func__, sm5714_regs[i], addr);
			return 1;
		}
	}

	pr_info("%s: addr: 0x%x doesn't exist, cannot write data\n", __func__, addr);
	return 0;
}

static int sm5714_fg_find_sram_addr(int addr)
{
	int r_size = (int)ARRAY_SIZE(sm5714_srams);
	int i;

	for (i = 0; i < r_size; i++) {
		if (sm5714_srams[i] == addr) {
			pr_info("%s: addr: 0x%x == 0x%x is founded\n", __func__, sm5714_srams[i], addr);
			return 1;
		}
	}

	pr_info("%s: addr: 0x%x doesn't exist, cannot write data\n", __func__, addr);
	return 0;
}

static int sm5714_fg_read_sram(struct sm5714_fuelgauge_data *fuelgauge, int sram_addr)
{
	int data;
#if defined(CONFIG_SEC_FACTORY)
	int addr;
#endif

	mutex_lock(&fuelgauge->fg_lock);

	if (sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_RADDR, sram_addr)) {
		pr_info("%s: SM5714_FG_REG_SRAM_RADDR write ERROR!!!!!\n", __func__);
		mutex_unlock(&fuelgauge->fg_lock);
		return -1;
	}

	data = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_RDATA);
#if defined(CONFIG_SEC_FACTORY)
	addr = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_RADDR);
	if (sram_addr != addr)
		pr_info("%s: SM5714_FG_REG_SRAM_RADDR read ERROR!!!!!(0x%x, 0x%x)\n",
			__func__, sram_addr, addr);
#endif

	mutex_unlock(&fuelgauge->fg_lock);

	return data;
}

static int sm5714_fg_write_sram(struct sm5714_fuelgauge_data *fuelgauge, int sram_addr, int data)
{
	mutex_lock(&fuelgauge->fg_lock);

	if (sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_WADDR, sram_addr)) {
		pr_info("%s: SM5714_FG_REG_SRAM_WADDR write ERROR!!!!!\n", __func__);
		mutex_unlock(&fuelgauge->fg_lock);
		return -1;
	}
	if (sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_WDATA, data)) {
		pr_info("%s: SM5714_FG_REG_SRAM_WDATA write ERROR!!!!!\n", __func__);
		mutex_unlock(&fuelgauge->fg_lock);
		return -1;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	return 1;
}

bool sm5714_fg_fuelalert_init(struct sm5714_fuelgauge_data *fuelgauge,
			int soc);

#if !defined(CONFIG_SEC_FACTORY)
static void sm5714_dump_all(struct sm5714_fuelgauge_data *fuelgauge)
{
	int val;
	int i;
	int r_size = (int)ARRAY_SIZE(sm5714_srams)/4;
	char temp_buf[660] = {0,};

	switch (sm5714_fg_debug_print%5) {
	case 0:
		for (i = 0; i < (int)ARRAY_SIZE(sm5714_regs); i++) {
			val = -1;
			val = sm5714_read_word(fuelgauge->i2c, sm5714_regs[i]);
			sprintf(temp_buf+strlen(temp_buf), "%02x:%04x,", sm5714_regs[i], val);
		}
		pr_info("[sm5714_fg_all_regs] %s\n", temp_buf);
		memset(temp_buf, 0x0, sizeof(temp_buf));
		sm5714_fg_debug_print++;
		break;
	case 1:
		for (i = 0; i < r_size; i++) {
			val = -1;
			val = sm5714_fg_read_sram(fuelgauge, sm5714_srams[i]);
			sprintf(temp_buf+strlen(temp_buf), "%02x:%04x,", sm5714_srams[i], val);
		}
		pr_info("[sm5714_fg_all_srams_1] %s\n", temp_buf);
		memset(temp_buf, 0x0, sizeof(temp_buf));
		sm5714_fg_debug_print++;
		break;
	case 2:
		for (i = r_size; i < r_size*2; i++) {
			val = -1;
			val = sm5714_fg_read_sram(fuelgauge, sm5714_srams[i]);
			sprintf(temp_buf+strlen(temp_buf), "%02x:%04x,", sm5714_srams[i], val);
		}
		pr_info("[sm5714_fg_all_srams_2] %s\n", temp_buf);
		memset(temp_buf, 0x0, sizeof(temp_buf));
		sm5714_fg_debug_print++;
		break;
	case 3:
		for (i = r_size*2; i < r_size*3; i++) {
			val = -1;
			val = sm5714_fg_read_sram(fuelgauge, sm5714_srams[i]);
			sprintf(temp_buf+strlen(temp_buf), "%02x:%04x,", sm5714_srams[i], val);
		}
		pr_info("[sm5714_fg_all_srams_3] %s\n", temp_buf);
		memset(temp_buf, 0x0, sizeof(temp_buf));
		sm5714_fg_debug_print++;
		break;
	case 4:
		for (i = r_size*3; i < r_size*4; i++) {
			val = -1;
			val = sm5714_fg_read_sram(fuelgauge, sm5714_srams[i]);
			sprintf(temp_buf+strlen(temp_buf), "%02x:%04x,", sm5714_srams[i], val);
		}
		pr_info("[sm5714_fg_all_srams_4] %s\n", temp_buf);
		memset(temp_buf, 0x0, sizeof(temp_buf));
		sm5714_fg_debug_print = 0;
		break;


	}
}
#else
static void sm5714_fg_reg_dump(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret_data = 0;
	int i;
	char *str = NULL;

	str = kzalloc(sizeof(char) * 2056, GFP_KERNEL);
	if (!str)
		return;

	for (i = 0x00; i <= 0xff; i++) {
		ret_data = -1;
		ret_data = sm5714_read_word(fuelgauge->i2c, i);
		sprintf(str + strlen(str), "%04x, ", ret_data);

		if (((i + 0x1) % 0x20 == 0) && (i != 0xff))
			sprintf(str + strlen(str), "\n");
	}
	pr_info("%s\n", str);
	kfree(str);
}

static void sm5714_fuel_gauge_abnormal_reg_check(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret_data = 0, ret_addr = 0;
	int addr, data;
	int i, abnormal_reg = 0, retry_cnt = 0;

	pr_info("%s\n", __func__);
	sm5714_fg_reg_dump(fuelgauge);

	for (i = 0; i < FG_REG_CHECK_TABLE_LEN; i++) {
		retry_cnt = 0;
		addr = sm5714_reg_check_table[0][i];
		data = sm5714_reg_check_table[1][i];
		ret_data = sm5714_fg_read_sram(fuelgauge, addr);

		if (ret_data != data) {
			abnormal_reg = 1;
			pr_info("addr:0x%02x, data:0x%04x, comp data:0x%04x\n",
				addr, data, ret_data);
			do {
				mutex_lock(&fuelgauge->fg_lock);
				ret_addr = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_RADDR);
				mutex_unlock(&fuelgauge->fg_lock);
				pr_info("addr:0x%02x, data:0x%04x, comp addr:0x%04x (retry %d)\n",
					addr, data, ret_addr, retry_cnt++);
				msleep(20);
			} while (ret_addr != addr && retry_cnt <= 10);

			mutex_lock(&fuelgauge->fg_lock);
			ret_data = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SRAM_RDATA);
			mutex_unlock(&fuelgauge->fg_lock);
			pr_info("addr:0x%02x, data:0x%04x, comp data:0x%04x\n",
				addr, data, ret_data);
		}
	}

	if (abnormal_reg) {
		sm5714_fg_reg_dump(fuelgauge);
		panic("sm5714-fg abnormal reg");
	}
}
#endif

static bool sm5714_fg_check_reg_init_need(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS);

	if ((ret & INIT_CHECK_MASK) == DISABLE_RE_INIT) {
		pr_info("%s: SM5714_REG_FG_SYSTEM_STATUS : 0x%x , return FALSE NO init need\n", __func__, ret);
		return 0;
	}

	pr_info("%s: SM5714_REG_FG_SYSTEM_STATUS : 0x%x , return TRUE init need!!!!\n", __func__, ret);
	return 1;
}

static unsigned int sm5714_get_vbat(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret1 = 0, ret2 = 0;
	unsigned int vbat = 0, vbat_avg = 0; /* = 3500; 3500 means 3500mV*/

	ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VBAT);
	ret2 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VBAT_AVG);

	/* auto value change to vsys by jig */
	if (sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000) {
		if (fuelgauge->isjigmoderealvbat) {
			ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VSYS);
			pr_info("%s : nENQ4 high JIG_ON, BUT need real VBAT, return VSYS_REG 0x%x\n", __func__, ret1);
		} else
			pr_info("%s : nENQ4 high JIG_ON, return VBAT_REG 0x%x, VSYS_REG 0x%x\n", __func__,
				ret1, sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VSYS));
	} else if (sm5714_get_facmode()) {
		vbat = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VSYS);
		pr_info("%s : nENQ4 low but factory_mode, VBAT_REG 0x%x, return VSYS_REG 0x%x\n", __func__, ret1, vbat);
		ret1 = vbat;
	}

	if (ret1 < 0) {
		pr_err("%s: read vbat reg fail", __func__);
		vbat = 4000;
	} else {
		if (ret1 & 0x8000)
			vbat = 2700 - (((ret1 & 0x7fff) * 10) / 109);
		else
			vbat = ((ret1 * 10) / 109) + 2700;
	}
	fuelgauge->info.batt_voltage = vbat;

	if (ret2 < 0) {
		pr_err("%s: read vbat_avg reg fail", __func__);
		vbat_avg = vbat;
	} else {
		if (ret2 & 0x8000)
			vbat_avg = 2700 - (((ret2 & 0x7fff) * 10) / 109);
		else
			vbat_avg = ((ret2 * 10) / 109) + 2700;
	}
	fuelgauge->info.batt_avgvoltage = vbat_avg;

	if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) &&
		(vbat >= fuelgauge->battery_data->sw_v_empty_recover_vol)) {
		fuelgauge->vempty_mode = VEMPTY_MODE_SW_RECOVERY;

		sm5714_fg_fuelalert_init(fuelgauge,
			fuelgauge->pdata->fuel_alert_soc);
		pr_info("%s : Recoverd from SW V EMPTY Activation\n", __func__);
	}

	return vbat;
}

static unsigned int sm5714_get_ocv(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int ocv; /* = 3500; *//*3500 means 3500mV*/

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_OCV);
	if (ret < 0) {
		pr_err("%s: read ocv reg fail\n", __func__);
		ocv = 4000;
	} else {
		ocv = (ret * 1000) >> 11;
	}

	fuelgauge->info.batt_ocv = ocv;

	return ocv;
}

static int sm5714_get_curr(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret, s_stat, a_stat;
	int curr = 0, curr_avg = 0; /* = 1000; 1000 means 1000mA*/

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_CURRENT);
	s_stat = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS);
	a_stat = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_AUX_STAT);

	if (ret < 0) {
		pr_err("%s: read curr reg fail", __func__);
		curr = 0;
	} else {
		curr = ((ret&0x7fff)*1000)/2044;
		if (ret & 0x8000)
			curr *= -1;
		else if ((!(a_stat & 0x0020)) && (!fuelgauge->info.flag_chg_status))
			curr = 0;
	}

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_CURRENT_AVG);
	if (ret < 0) {
		pr_err("%s: read curr_avg reg fail", __func__);
		curr_avg = 0;
	} else {
		curr_avg = ((ret&0x7fff)*1000)/2044;
		if (ret & 0x8000)
			curr_avg *= -1;
		else if ((!(a_stat & 0x0020)) && (!fuelgauge->info.flag_chg_status))
			curr_avg = 0;
	}

	if (a_stat & 0x0400) {
		curr = curr * 25 / 10;
		curr_avg = curr_avg * 25 / 10;
	}

	if (s_stat & 0x8000) {
		curr = 0;
		curr_avg = 0;
	}

	fuelgauge->info.batt_current = curr;
	fuelgauge->info.batt_avgcurrent = curr_avg;

	return curr;
}

static int sm5714_get_temperature(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;
	int temp; /* = 250; 250 means 25.0oC*/

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_TEMPERATURE);
	if (ret < 0) {
		pr_err("%s: read temp reg fail", __func__);
		temp = 0;
	} else {
		temp = (((ret&0x7FFF)*10) * 2989) >> 11 >> 8;
		if (ret&0x8000)
			temp *= -1;
	}
	fuelgauge->info.temp_fg = temp;

	return temp;
}

static int sm5714_get_cycle(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;
	int cycle;

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_SOC_CYCLE);
	if (ret < 0) {
		pr_err("%s: read cycle reg fail", __func__);
		cycle = 0;
	} else {
		cycle = ret&0x00FF;
	}
	fuelgauge->info.batt_soc_cycle = cycle;

	return cycle;
}

static int sm5714_get_asoc(struct sm5714_fuelgauge_data *fuelgauge)
{
	int soh, pre_soh, h_flag, c_flag, delta_t, temp;

	soh = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_AGING_RATE_FILT);
	pre_soh = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_2);
	c_flag = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_SOC_CYCLE);

	pr_info("%s : asoc = %d, ic_soh = 0x%x, pre = 0x%x, cycle = %d, t = %d\n",
		__func__, fuelgauge->info.soh, soh, pre_soh, c_flag, fuelgauge->info.temperature);

	soh = soh * 100 / 2048;

	h_flag = (pre_soh & 0x80)>>7;
	pre_soh = pre_soh & 0x7F;
	c_flag = (c_flag >> 4) % 2;

	delta_t = fuelgauge->info.temperature/10 - fuelgauge->info.temp_std;
	if (delta_t >= 0) {
		if (soh < pre_soh) {
			if (c_flag != h_flag) {
				pre_soh = pre_soh-1;
				temp = (c_flag<<7) | pre_soh;
				sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_2, temp);
				pr_info("%s : pre_soh update to %d, write 0x%x\n", __func__, pre_soh, temp);
			}
		}
	}
	soh = pre_soh;

	fuelgauge->info.soh = soh;

	return fuelgauge->info.soh;
}


static void sm5714_vbatocv_check(struct sm5714_fuelgauge_data *fuelgauge)
{
	if ((fuelgauge->is_charging) && (fuelgauge->info.batt_current < (fuelgauge->info.top_off)) &&
	   (fuelgauge->info.batt_current > (fuelgauge->info.top_off/3)) && (fuelgauge->info.batt_soc >= 900)) {
		if (abs(fuelgauge->info.batt_ocv-fuelgauge->info.batt_voltage) > 30) { /* 30mV over */
			fuelgauge->info.iocv_error_count++;
		}

		pr_info("%s: sm5714 FG iocv_error_count (%d)\n", __func__, fuelgauge->info.iocv_error_count);

		if (fuelgauge->info.iocv_error_count > 3) /* prevent to overflow */
			fuelgauge->info.iocv_error_count = 4;
	} else {
		fuelgauge->info.iocv_error_count = 0;
	}

	if (fuelgauge->info.iocv_error_count > 3) {
		pr_info("%s: p_v - v = (%d)\n", __func__,
			fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage);
		if (abs(fuelgauge->info.p_batt_voltage - fuelgauge->info.batt_voltage) > 15) { /* 15mV over */
			fuelgauge->info.iocv_error_count = 0;
		} else {
			/* mode change to mix RS manual mode */
			pr_info("%s: mode change to mix RS manual mode\n", __func__);
			/* RS manual value write */
			sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX, fuelgauge->info.rs_value[0]+5);
		}
	} else {
		/* voltage mode with 3.4V */
		if (fuelgauge->vempty_mode != VEMPTY_MODE_HW) {
			if ((fuelgauge->info.p_batt_voltage < fuelgauge->info.n_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.n_tem_poff) &&
				(!fuelgauge->is_charging)) {
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				/* mode change to mix RS manual mode */
				/* RS manual value write */
				if ((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.n_tem_poff - fuelgauge->info.n_tem_poff_offset))) {
					sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX,
						fuelgauge->info.rs_value[0]+5);
				} else {
					sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX,
						(fuelgauge->info.rs_value[0]<<1)+5);
				}
			} else {
				pr_info("%s: mode change to mix RS auto mode\n", __func__);
				sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX, 0x3800);
			}
		} else { /* voltage mode with 3.25V, VEMPTY_MODE_HW mode */
			if ((fuelgauge->info.p_batt_voltage < fuelgauge->info.l_tem_poff) &&
				(fuelgauge->info.batt_voltage < fuelgauge->info.l_tem_poff) &&
				(!fuelgauge->is_charging)) {
				pr_info("%s: mode change to normal tem mix RS manual mode\n", __func__);
				/* mode change to mix RS manual mode */
				/* RS manual value write */
				if ((fuelgauge->info.p_batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset)) &&
					(fuelgauge->info.batt_voltage <
					(fuelgauge->info.l_tem_poff - fuelgauge->info.l_tem_poff_offset))) {
					sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX,
						fuelgauge->info.rs_value[0]+5);
				} else {
					sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX,
						(fuelgauge->info.rs_value[0]<<1)+5);
				}
			} else {
				pr_info("%s: mode change to mix RS auto mode\n", __func__);
				sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX, 0x3800);
			}
		}
	}
	fuelgauge->info.p_batt_voltage = fuelgauge->info.batt_voltage;
	fuelgauge->info.p_batt_current = fuelgauge->info.batt_current;
	/* iocv error case cover end */
}

static void sm5714_ical_setup(struct sm5714_fuelgauge_data *fuelgauge)
{
	if (!fuelgauge->info.i_dp_default) {
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_DP_IOFFSET, fuelgauge->info.dp_i_off);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_DP_IPSLOPE, fuelgauge->info.dp_i_pslo);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_DP_INSLOPE, fuelgauge->info.dp_i_nslo);
	}
	if (!fuelgauge->info.i_alg_default) {
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_ALG_IOFFSET, fuelgauge->info.alg_i_off);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_ALG_IPSLOPE, fuelgauge->info.alg_i_pslo);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_ALG_INSLOPE, fuelgauge->info.alg_i_nslo);
	}

	pr_info("%s: dp %d: <0x%x 0x%x 0x%x> alg %d: <0x%x 0x%x 0x%x>\n", __func__,
		fuelgauge->info.i_dp_default,
		fuelgauge->info.dp_i_off, fuelgauge->info.dp_i_pslo, fuelgauge->info.dp_i_nslo,
		fuelgauge->info.i_alg_default,
		fuelgauge->info.alg_i_off, fuelgauge->info.alg_i_pslo, fuelgauge->info.alg_i_nslo);
}

static void sm5714_vcal_setup(struct sm5714_fuelgauge_data *fuelgauge)
{
	if (!fuelgauge->info.v_default)	{
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VOFFSET, fuelgauge->info.v_off);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VSLOPE, fuelgauge->info.v_slo);
	}

	if (!fuelgauge->info.vt_default) {
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_VVT, fuelgauge->info.vtt);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_IVT, fuelgauge->info.ivt);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_IVV, fuelgauge->info.ivv);
	}

	pr_info("%s: vcal %d: <0x%x 0x%x> T %d: <0x%x 0x%x 0x%x>\n",
			__func__,
		fuelgauge->info.v_default, fuelgauge->info.v_off, fuelgauge->info.v_slo,
		fuelgauge->info.vt_default, fuelgauge->info.vtt, fuelgauge->info.ivt, fuelgauge->info.ivv);
}

static int sm5714_fg_verified_write_word(struct i2c_client *client,
		u8 reg_addr, u16 data)
{
	int ret;

	ret = sm5714_write_word(client, reg_addr, data);
	if (ret < 0) {
		msleep(50);
		pr_info("1st fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
				__func__, ret, reg_addr, data);
		ret = sm5714_write_word(client, reg_addr, data);
		if (ret < 0) {
			msleep(50);
			pr_info("2nd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
					__func__, ret, reg_addr, data);
			ret = sm5714_write_word(client, reg_addr, data);
			if (ret < 0) {
				pr_info("3rd fail i2c write %s: ret = %d, addr = 0x%x, data = 0x%x\n",
						__func__, ret, reg_addr, data);
			}
		}
	}

	return ret;
}


int sm5714_fg_calculate_iocv(struct sm5714_fuelgauge_data *fuelgauge)
{
	bool only_lb = false, sign_i_offset = 0; /*valid_cb=false, */
	int roop_start = 0, roop_max = 0, i = 0, cb_last_index = 0, cb_pre_last_index = 0;
	int lb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int lb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i_offset_margin = 0x14, i_vset_margin = 0x67;
	int v_max = 0, v_min = 0, v_sum = 0;
	int lb_v_avg = 0, cb_v_avg = 0, lb_v_set = 0, lb_i_set = 0, i_offset = 0; /* lb_v_minmax_offset=0, */
	int i_max = 0, i_min = 0, i_sum = 0;
	int lb_i_avg = 0, cb_i_avg = 0, cb_v_set = 0, cb_i_set = 0; /* lb_i_minmax_offset=0, */
	int lb_i_p_v_min = 0, lb_i_n_v_max = 0, cb_i_p_v_min = 0, cb_i_n_v_max = 0;

	int v_ret = 0, i_ret = 0, ret = 0;

	ret = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_AUX_STAT);
	pr_info("%s: iocv_status_read = addr : 0x%x , data : 0x%x\n", __func__, SM5714_FG_REG_AUX_STAT, ret);

	/* init start */
	if ((ret & 0x0800) == 0x0000)
		only_lb = true;

/*
 *	if ((ret & 0x3000) == 0x3000)
 *		valid_cb = true;
 */
/* init end */

	/* lb get start */
	roop_max = (ret & 0xF000)>>12;

	if (roop_max < 3) {
		pr_info("%s: Not enough linear buffer(%d)\n", __func__, roop_max);
		ret = ((sm5714_get_vbat(fuelgauge) << 8) / 125);
		return ret;
	}

	if (roop_max > FG_INIT_B_LEN+1)
		roop_max = FG_INIT_B_LEN+1;

	roop_start = SM5714_FG_ADDR_SRAM_START_LB_V;
	for (i = roop_start; i < roop_start + roop_max; i++) {
		v_ret = sm5714_fg_read_sram(fuelgauge, i);
		i_ret = sm5714_fg_read_sram(fuelgauge, i+0x10);

		if ((i_ret&0x8000) == 0x8000)
			i_ret = -(i_ret&0x7FFF);

		lb_v_buffer[i-roop_start] = v_ret;
		lb_i_buffer[i-roop_start] = i_ret;

		if (i == roop_start) {
			v_max = v_ret;
			v_min = v_ret;
			v_sum = v_ret;
			i_max = i_ret;
			i_min = i_ret;
			i_sum = i_ret;
		} else {
			if (v_ret > v_max)
				v_max = v_ret;
			else if (v_ret < v_min)
				v_min = v_ret;
			v_sum = v_sum + v_ret;

			if (i_ret > i_max)
				i_max = i_ret;
			else if (i_ret < i_min)
				i_min = i_ret;
			i_sum = i_sum + i_ret;
		}

		if (abs(i_ret) > i_vset_margin) {
			if (i_ret > 0) {
				if (lb_i_p_v_min == 0) {
					lb_i_p_v_min = v_ret;
				} else {
					if (v_ret < lb_i_p_v_min)
						lb_i_p_v_min = v_ret;
				}
			} else {
				if (lb_i_n_v_max == 0) {
					lb_i_n_v_max = v_ret;
				} else {
					if (v_ret > lb_i_n_v_max)
						lb_i_n_v_max = v_ret;
				}
			}
		}
	}
	v_sum = v_sum - v_max - v_min;
	i_sum = i_sum - i_max - i_min;

/*
 *	lb_v_minmax_offset = v_max - v_min;
 *	lb_i_minmax_offset = i_max - i_min;
 */

	lb_v_avg = v_sum / (roop_max-2);
	lb_i_avg = i_sum / (roop_max-2);
	/* lb get end */

	/* lb_vset start */
	if (abs(lb_i_buffer[roop_max-1]) < i_vset_margin) {
		if (abs(lb_i_buffer[roop_max-2]) < i_vset_margin) {
			lb_v_set = MAXVAL(lb_v_buffer[roop_max-2], lb_v_buffer[roop_max-1]);
			if (abs(lb_i_buffer[roop_max-3]) < i_vset_margin)
				lb_v_set = MAXVAL(lb_v_buffer[roop_max-3], lb_v_set);
		} else {
			lb_v_set = lb_v_buffer[roop_max-1];
		}
	} else {
		lb_v_set = lb_v_avg;
	}

	if (lb_i_n_v_max > 0)
		lb_v_set = MAXVAL(lb_i_n_v_max, lb_v_set);

/*
 *	else if (lb_i_p_v_min > 0) {
 *		lb_v_set = MINVAL(lb_i_p_v_min, lb_v_set);
 *	}
 *	lb_vset end
 *
 *	lb offset make start
 */
	if (roop_max > 3)
		lb_i_set = (lb_i_buffer[2] + lb_i_buffer[3]) / 2;

	if ((abs(lb_i_buffer[roop_max-1]) < i_offset_margin) && (abs(lb_i_set) < i_offset_margin))
		lb_i_set = MAXVAL(lb_i_buffer[roop_max-1], lb_i_set);
	else if (abs(lb_i_buffer[roop_max-1]) < i_offset_margin)
		lb_i_set = lb_i_buffer[roop_max-1];
	else if (abs(lb_i_set) < i_offset_margin)
		i_offset = lb_i_set;
	else
		lb_i_set = 0;

	i_offset = lb_i_set;

	i_offset = i_offset + 4;	/* add extra offset */

	if (i_offset <= 0) {
		sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
		i_offset = 0;
#else
		i_offset = -i_offset;
#endif
	}

	i_offset = i_offset>>1;

	if (sign_i_offset == 0)
		i_offset = i_offset|0x0080;

	i_offset = i_offset | i_offset<<8;

	pr_info("%s: iocv_l_max=0x%x, iocv_l_min=0x%x, iocv_l_avg=0x%x, lb_v_set=0x%x, roop_max=%d\n",
			__func__, v_max, v_min, lb_v_avg, lb_v_set, roop_max);
	pr_info("%s: ioci_l_max=0x%x, ioci_l_min=0x%x, ioci_l_avg=0x%x, lb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
			__func__, i_max, i_min, lb_i_avg, lb_i_set, i_offset, sign_i_offset);

	if (!only_lb) {
		/* cb get start */
		roop_start = SM5714_FG_ADDR_SRAM_START_CB_V;
		roop_max = FG_INIT_B_LEN+1;
		for (i = roop_start; i < roop_start + roop_max; i++) {
			v_ret = sm5714_fg_read_sram(fuelgauge, i);
			i_ret = sm5714_fg_read_sram(fuelgauge, i+0x10);

			if ((i_ret&0x8000) == 0x8000)
				i_ret = -(i_ret&0x7FFF);

			cb_v_buffer[i-roop_start] = v_ret;
			cb_i_buffer[i-roop_start] = i_ret;

			if (i == roop_start) {
				v_max = v_ret;
				v_min = v_ret;
				v_sum = v_ret;
				i_max = i_ret;
				i_min = i_ret;
				i_sum = i_ret;
			} else {
				if (v_ret > v_max)
					v_max = v_ret;
				else if (v_ret < v_min)
					v_min = v_ret;
				v_sum = v_sum + v_ret;

				if (i_ret > i_max)
					i_max = i_ret;
				else if (i_ret < i_min)
					i_min = i_ret;
				i_sum = i_sum + i_ret;
			}

			if (abs(i_ret) > i_vset_margin) {
				if (i_ret > 0) {
					if (cb_i_p_v_min == 0)
						cb_i_p_v_min = v_ret;
					else if (v_ret < cb_i_p_v_min)
						cb_i_p_v_min = v_ret;
				} else {
					if (cb_i_n_v_max == 0)
						cb_i_n_v_max = v_ret;
					else if (v_ret > cb_i_n_v_max)
						cb_i_n_v_max = v_ret;
				}
			}
		}
		v_sum = v_sum - v_max - v_min;
		i_sum = i_sum - i_max - i_min;

		cb_v_avg = v_sum / (roop_max-2);
		cb_i_avg = i_sum / (roop_max-2);
		/* cb get end */

		/* cb_vset start */
		cb_last_index = ((ret & 0xF000)>>12)-(FG_INIT_B_LEN+1)-1; /*-8-1 */
		if (cb_last_index < 0)
			cb_last_index = FG_INIT_B_LEN;

		for (i = roop_max; i > 0; i--) {
			if (abs(cb_i_buffer[cb_last_index]) < i_vset_margin) {
				cb_v_set = cb_v_buffer[cb_last_index];
				if (abs(cb_i_buffer[cb_last_index]) < i_offset_margin)
					cb_i_set = cb_i_buffer[cb_last_index];

				cb_pre_last_index = cb_last_index - 1;
				if (cb_pre_last_index < 0)
					cb_pre_last_index = FG_INIT_B_LEN;

				if (abs(cb_i_buffer[cb_pre_last_index]) < i_vset_margin) {
					cb_v_set = MAXVAL(cb_v_buffer[cb_pre_last_index], cb_v_set);
					if (abs(cb_i_buffer[cb_pre_last_index]) < i_offset_margin)
						cb_i_set = MAXVAL(cb_i_buffer[cb_pre_last_index], cb_i_set);
				}
			} else {
				cb_last_index--;
				if (cb_last_index < 0)
					cb_last_index = FG_INIT_B_LEN;
			}
		}

		if (cb_v_set == 0) {
			cb_v_set = cb_v_avg;
			if (cb_i_set == 0)
				cb_i_set = cb_i_avg;
		}

		if (cb_i_n_v_max > 0)
			cb_v_set = MAXVAL(cb_i_n_v_max, cb_v_set);
/*
 *		else if(cb_i_p_v_min > 0)
 *			cb_v_set = MINVAL(cb_i_p_v_min, cb_v_set);
 *
 *		cb_vset end
 *
 *		cb offset make start
 */
		if (abs(cb_i_set) < i_offset_margin) {
			if (cb_i_set > lb_i_set) {
				i_offset = cb_i_set;
				i_offset = i_offset + 4;	/* add extra offset */

				if (i_offset <= 0) {
					sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
					i_offset = 0;
#else
					i_offset = -i_offset;
#endif
				}

				i_offset = i_offset>>1;

				if (sign_i_offset == 0)
					i_offset = i_offset|0x0080;

				i_offset = i_offset | i_offset<<8;

			}
		}
		/* cb offset make end */

		pr_info("%s: iocv_c_max=0x%x, iocv_c_min=0x%x, iocv_c_avg=0x%x, cb_v_set=0x%x, cb_last_index=%d\n",
				__func__, v_max, v_min, cb_v_avg, cb_v_set, cb_last_index);
		pr_info("%s: ioci_c_max=0x%x, ioci_c_min=0x%x, ioci_c_avg=0x%x, cb_i_set=0x%x, i_offset=0x%x, sign_i_offset=%d\n",
				__func__, i_max, i_min, cb_i_avg, cb_i_set, i_offset, sign_i_offset);

	}

	/* final set */
	if ((abs(cb_i_set) > i_vset_margin) || only_lb)
		ret = MAXVAL(lb_v_set, cb_i_n_v_max);
	else
		ret = cb_v_set;

	if (ret > fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1]) {
		pr_info("%s: iocv ret change 0x%x -> 0x%x\n", __func__,
			ret, fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1]);
		ret = fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1];
	} else if (ret < fuelgauge->info.battery_table[DISCHARGE_TABLE][0]) {
		pr_info("%s: iocv ret change 0x%x -> 0x%x\n", __func__,
			ret, (fuelgauge->info.battery_table[DISCHARGE_TABLE][0] + 0x10));
		ret = fuelgauge->info.battery_table[DISCHARGE_TABLE][0] + 0x10;
	}

	return ret;
}

void sm5714_set_aux_ctrl_cfg(struct sm5714_fuelgauge_data *fuelgauge)
{
	int rsmanvalue;

	sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_AUX_CTRL1, fuelgauge->info.aux_ctrl[0]);
	sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_AUX_CTRL2, fuelgauge->info.aux_ctrl[1]);

	rsmanvalue = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_AUTO_MAN_VALUE);

	pr_info("%s: aux_ctrl1 = 0x%x, aux_ctrl2 = 0x%x, rsmanvalue = 0x%x\n",
		__func__, fuelgauge->info.aux_ctrl[0], fuelgauge->info.aux_ctrl[1], rsmanvalue);
}

int get_v_max_index_by_cycle(struct sm5714_fuelgauge_data *fuelgauge)

{
	int cycle_index = 0, len;

	for (len = fuelgauge->pdata->num_age_step-1; len >= 0; --len) {
		if (fuelgauge->chg_full_soc == fuelgauge->pdata->age_data_soc[len]) {
			cycle_index = len;
			break;
		}
	}
	pr_info("%s: chg_full_soc = %d, index = %d\n", __func__, fuelgauge->chg_full_soc, cycle_index);

	return cycle_index;
}

static bool sm5714_check_jig_status(struct sm5714_fuelgauge_data *fuelgauge)
{
	bool ret = false;

	if (fuelgauge->pdata->jig_gpio) {
		if (fuelgauge->pdata->jig_low_active)
			ret = !gpio_get_value(fuelgauge->pdata->jig_gpio);
		else
			ret = gpio_get_value(fuelgauge->pdata->jig_gpio);
	}
	pr_info("%s: jig_gpio(%d), ret(%d)\n",
		__func__, fuelgauge->pdata->jig_gpio, ret);
	return ret;
}

static bool sm5714_fg_reg_init(struct sm5714_fuelgauge_data *fuelgauge, bool is_surge)
{
	int i, j, k, value, ret = 0;
	uint8_t table_reg;
	int write_table[TABLE_MAX][FG_TABLE_LEN+1];

	pr_info("%s: START!!\n", __func__);

	/* start first param_ctrl unlock */
	sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_TABLE_UNLOCK, FG_PARAM_UNLOCK_CODE);

	/* CAP write */
	i = get_v_max_index_by_cycle(fuelgauge);
	pr_info("%s: v_max_now is change %x -> %x\n", __func__,
		fuelgauge->info.v_max_now, fuelgauge->info.v_max_table[i]);
	pr_info("%s: q_max_now is change %x -> %x\n", __func__,
		fuelgauge->info.q_max_now, fuelgauge->info.q_max_table[i]);
	fuelgauge->info.v_max_now = fuelgauge->info.v_max_table[i];
	fuelgauge->info.q_max_now = fuelgauge->info.q_max_table[i];
	fuelgauge->info.cap = fuelgauge->info.q_max_now;

	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_Q_MAX, fuelgauge->info.cap);
	pr_info("%s: SM5714_FG_ADDR_SRAM_Q_MAX 0x%x\n",
		__func__, fuelgauge->info.cap);

	for (i = 0; i < TABLE_MAX; i++) {
		for (j = 0; j <= FG_TABLE_LEN; j++) {
			if (i == SOC_TABLE) {
				write_table[i][j] = (fuelgauge->info.battery_table[i][j] * 2) -
					(fuelgauge->info.battery_table[i][j] * fuelgauge->info.q_max_now /
					fuelgauge->info.maxcap);
				if (j == FG_TABLE_LEN) {
					write_table[i][FG_TABLE_LEN-1] = 100*256;
					write_table[i][FG_TABLE_LEN] = 100*256+26;
				}
			} else {
				write_table[i][j] = fuelgauge->info.battery_table[i][j];
				if (j == FG_TABLE_LEN-1) {
					write_table[i][FG_TABLE_LEN-1] = fuelgauge->info.v_max_now;
					if (write_table[i][FG_TABLE_LEN-1] < write_table[i][FG_TABLE_LEN-2]) {
						write_table[i][FG_TABLE_LEN-2] =
							write_table[i][FG_TABLE_LEN-1] - 0x18; // ~11.7mV
						write_table[SOC_TABLE][FG_TABLE_LEN-2] =
							(write_table[SOC_TABLE][FG_TABLE_LEN-1]*99)/100;
					}
				}
			}
		}
	}

	for (i = 0; i < TABLE_MAX; i++)	{
		table_reg = SM5714_FG_ADDR_TABLE0_0 + (i*(FG_TABLE_LEN+1));
		for (j = 0; j <= FG_TABLE_LEN; j++) {
			sm5714_fg_write_sram(fuelgauge, (table_reg + j), write_table[i][j]);
			pr_info("%s: TABLE write [%d][%d] = 0x%x : 0x%x\n",
				__func__, i, j, (table_reg + j), write_table[i][j]);
		}
	}

	/*for verify table data write*/
	for (i = 0; i < TABLE_MAX; i++) {
		table_reg = SM5714_FG_ADDR_TABLE0_0 + (i*(FG_TABLE_LEN+1));
		for (j = 0; j <= FG_TABLE_LEN; j++) {
			if (write_table[i][j] == sm5714_fg_read_sram(fuelgauge, (table_reg + j))) {
				pr_info("%s: TABLE data verify OK [%d][%d] = 0x%x : 0x%x\n",
				__func__, i, j, (table_reg + j), write_table[i][j]);
			} else {
				ret |= I2C_ERROR_CHECK;
				for (k = 1; k <= I2C_ERROR_COUNT_MAX; k++) {
					pr_info("%s: TABLE write data ERROR!!!! rewrite [%d][%d] = 0x%x : 0x%x, count=%d\n",
						__func__, i, j, (table_reg + j), write_table[i][j], k);
					sm5714_fg_write_sram(fuelgauge, (table_reg + j), write_table[i][j]);
					msleep(30);
					if (write_table[i][j] == sm5714_fg_read_sram(fuelgauge, (table_reg + j))) {
						pr_info("%s: TABLE rewrite OK [%d][%d] = 0x%x : 0x%x, count=%d\n",
						__func__, i, j, (table_reg + j), write_table[i][j], k);
						break;
					}
					if (k == I2C_ERROR_COUNT_MAX)
						ret |= I2C_ERROR_REMAIN;
				}
			}
		}
	}

	table_reg = SM5714_FG_ADDR_TABLE2_0;
	for (j = 0; j <= FG_ADD_TABLE_LEN; j++) {
		sm5714_fg_write_sram(fuelgauge, (table_reg + j), fuelgauge->info.battery_table[i][j]);
		pr_info("%s: TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
				__func__, i, j, (table_reg + j), fuelgauge->info.battery_table[i][j]);
	}

	/* RS write */
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MIN, fuelgauge->info.rs_value[1]);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_MAX, fuelgauge->info.rs_value[2]);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_FACTOR, fuelgauge->info.rs_value[3]);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_CHG_FACTOR, fuelgauge->info.rs_value[4]);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_DISCHG_FACTOR, fuelgauge->info.rs_value[5]);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_RS_AUTO_MAN_VALUE, fuelgauge->info.rs_value[6]);

	pr_info("%s: spare=0x%x, MIN=0x%x, MAX=0x%x, FACTOR=0x%x, C_FACT=0x%x, D_FACT=0x%x, MAN_VALUE=0x%x\n", __func__,
		fuelgauge->info.rs_value[0], fuelgauge->info.rs_value[1], fuelgauge->info.rs_value[2],
		fuelgauge->info.rs_value[3], fuelgauge->info.rs_value[4], fuelgauge->info.rs_value[5],
		fuelgauge->info.rs_value[6]);

	/* surge reset defence */
	if (is_surge) {
		value = ((fuelgauge->info.batt_ocv<<8)/125);
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_2, 0);
	} else {
		/* auto value change to vsys by jig */
		if (sm5714_get_facmode()
			&& !(sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000)) {
			pr_info("%s: nENQ4 low but factory mode, iocv is vsys read value\n", __func__);
			value = ((sm5714_get_vbat(fuelgauge)<<8)/125);
		} else {
			pr_info("%s: iocv is buffer value\n", __func__);
			value = sm5714_fg_calculate_iocv(fuelgauge);
		}
	}
	usleep_range(10000, 11000);
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INIT_OCV, value);
	pr_info("%s: IOCV_MAN_WRITE = %d : 0x%x\n", __func__, SM5714_FG_ADDR_SRAM_INIT_OCV, value);

	/* LOCK */
	value = FG_PARAM_LOCK_CODE;
	sm5714_write_word(fuelgauge->i2c, SM5714_FG_REG_TABLE_UNLOCK, value);
	pr_info("%s: LAST PARAM CTRL VALUE = 0x%x : 0x%x\n", __func__, SM5714_FG_REG_TABLE_UNLOCK, value);

	/* init delay */
	msleep(20);

	/* write batt data version */
	ret |= (fuelgauge->info.data_ver << 4) & DATA_VERSION;
	if (sm5714_check_jig_status(fuelgauge))
		ret |= JIG_CONNECTED;
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_1, ret);
	pr_info("%s: RESERVED = 0x%x : 0x%x\n", __func__, SM5714_FG_ADDR_SRAM_USER_RESERV_1, ret);

	return 1;
}

static int sm5714_abnormal_reset_check(struct sm5714_fuelgauge_data *fuelgauge)
{
	int cntl_read;

	/* abnormal case process */
	if (sm5714_fg_check_reg_init_need(fuelgauge)) {
		cntl_read = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_CTRL);
		pr_info("%s: SM5714 FG abnormal case!!!! SM5714_FG_REG_CTRL : 0x%x, is_FG_initialised : %d\n", __func__,
		cntl_read, fuelgauge->info.is_FG_initialised);

		if (fuelgauge->info.is_FG_initialised == 1) {
			/* SW reset code */
			fuelgauge->info.is_FG_initialised = 0;
			if (sm5714_fg_verified_write_word(fuelgauge->i2c, SM5714_FG_REG_RESET, SW_RESET_OTP_CODE) < 0)
				pr_info("%s: Warning!!!! SM5714 FG abnormal case.... SW reset FAIL\n", __func__);
			else
				pr_info("%s: SM5714 FG abnormal case.... SW reset OK\n", __func__);

			/* delay 100ms */
			msleep(100);
			/* init code */
			sm5714_fg_init(fuelgauge, true);
		}
		return FG_ABNORMAL_RESET;
	}
	return 0;
}

static unsigned int sm5714_get_device_id(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_DEVICE_ID);
	sm5714_device_id = ret;
	pr_info("%s: SM5714 vender_id = 0x%x\n", __func__, ret);

	return ret;
}

int sm5714_call_fg_device_id(void)
{
	pr_info("%s: extern call SM5714 fg_device_id = 0x%x\n", __func__, sm5714_device_id);

	return sm5714_device_id;
}

unsigned int sm5714_get_soc(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;
	unsigned int soc;
	int retry_cnt = 2;

	while (retry_cnt-- > 0) {
		ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_SOC);
		if (ret < 0) {
			pr_err("%s: Warning!!!! read soc reg fail\n", __func__);
			soc = 500;
		} else {
			soc = (ret * 10) >> 8;
			if (ret > 0x63ff) {
				if (((fuelgauge->info.batt_avgcurrent - fuelgauge->info.top_off) >
					(fuelgauge->info.top_off/5)) ||
					(fuelgauge->info.batt_avgcurrent <= 0)) {
					if (fuelgauge->info.batt_soc == 999)
						if (soc > 1)
							soc = soc - 1;
				}
			}
		}

		/* for preventing soc jumping issue, read soc 1 more time */
		if (abs(fuelgauge->info.batt_soc - (int)soc) > 50) {
			if (sm5714_get_facmode()
				|| (sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000)) {
				break;
			}

			pr_info("%s: Warning!!!! prev batt_soc = %d, current batt_soc = %d, ret = 0x%x\n",
				__func__, fuelgauge->info.batt_soc, soc, ret);
		} else {
			break;
		}
	}

	if (sm5714_get_facmode()
		&& !(sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000)) {
		pr_info("%s: nENQ4 low but factory mode, SOC = %d, return pre_SOC = %d\n", __func__,
		soc, fuelgauge->info.batt_soc);
		return fuelgauge->info.batt_soc;
	}

	if (sm5714_abnormal_reset_check(fuelgauge) < 0)	{
		pr_info("%s: FG init ERROR!! pre_SOC returned!!, read_SOC = %d, pre_SOC = %d\n", __func__,
		soc, fuelgauge->info.batt_soc);
		return fuelgauge->info.batt_soc;
	}

	/* ocv update */
	sm5714_get_ocv(fuelgauge);

	/* for low temp power off test */
	if (fuelgauge->info.volt_alert_flag && (fuelgauge->info.temperature < -100)) {
		pr_info("%s: volt_alert_flag is TRUE!!!! SOC make force ZERO!!!!\n", __func__);
		fuelgauge->info.batt_soc = 0;
		pr_info("%s: batt_soc = %d, soc = %d, ret = 0x%x\n", __func__, fuelgauge->info.batt_soc, soc, ret);
		return 0;
	}

	fuelgauge->info.batt_soc = soc;
	pr_info("%s: batt_soc = %d, soc = %d, ret = 0x%x\n", __func__, fuelgauge->info.batt_soc, soc, ret);

	return soc;
}

static void sm5714_update_all_value(struct sm5714_fuelgauge_data *fuelgauge)
{

	union power_supply_propval value;

	fuelgauge->is_charging = (fuelgauge->info.flag_charge_health |
		fuelgauge->ta_exist) && (fuelgauge->info.batt_current >= 30);

	/* check charger status */

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_STATUS, value);
	fuelgauge->info.flag_full_charge =
		(value.intval == POWER_SUPPLY_STATUS_FULL) ? 1 : 0;
	fuelgauge->info.flag_chg_status =
		(value.intval == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;

	/* vbat */
	sm5714_get_vbat(fuelgauge);

	/* current */
	sm5714_get_curr(fuelgauge);

	/* temperature */
	sm5714_get_temperature(fuelgauge);

	/* cycle */
	sm5714_get_cycle(fuelgauge);

	/* carc */
	sm5714_vbatocv_check(fuelgauge);

	/* soc */
	sm5714_get_soc(fuelgauge);

	pr_info("%s: chg_h=%d, chg_f=%d, chg_s=%d, is_chg=%d, ta_exist=%d\n",
		__func__, fuelgauge->info.flag_charge_health, fuelgauge->info.flag_full_charge,
		fuelgauge->info.flag_chg_status, fuelgauge->is_charging, fuelgauge->ta_exist);
	pr_info("%s: v=%d, v_avg=%d, i=%d, i_avg=%d, ocv=%d, fg_t=%d, b_t=%d, cycle=%d, soc=%d\n",
		__func__, fuelgauge->info.batt_voltage, fuelgauge->info.batt_avgvoltage,
		fuelgauge->info.batt_current, fuelgauge->info.batt_avgcurrent, fuelgauge->info.batt_ocv,
		fuelgauge->info.temp_fg, fuelgauge->info.temperature, fuelgauge->info.batt_soc_cycle,
		fuelgauge->info.batt_soc);
}

static int sm5714_fg_set_jig_mode_real_vbat(struct sm5714_fuelgauge_data *fuelgauge, int meas_mode)
{
	int stat;

	fuelgauge->isjigmoderealvbat = false;

	if (sm5714_fg_check_reg_init_need(fuelgauge)) {
		pr_info("%s: FG init fail!!\n", __func__);
		return -1;
	}

	/** meas_mode = 0 is inbat mode with jig **/
	if (meas_mode == 0)	{
		stat = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS);
		if (stat & 0x8000) {
			fuelgauge->isjigmoderealvbat = true;
			pr_info("%s: FG check jig_ON!! and after read real VBAT!!\n", __func__);
		} else
			pr_info("%s: isjigmoderealvbat = 1 but FG check nENQ4 LOW!! check jig!!\n", __func__);
	} else
		pr_info("%s: meas_mode = 1, isjigmoderealvbat = false!!\n", __func__);

	return 0;
}

void sm5714_fg_reset_capacity_by_jig_connection(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret;

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_1);
	ret &= ~JIG_CONNECTED;
	if (sm5714_get_facmode()
		| (sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000))
		ret |= JIG_CONNECTED;
	sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_1, ret);

	pr_info("%s: set JIG_CONNECTED(0x%04x) (Jig Connection or bypass), fg_reset after reboot: %d\n",
		__func__, ret, ret & JIG_CONNECTED);
}

int sm5714_fg_alert_init(struct sm5714_fuelgauge_data *fuelgauge, int soc)
{
	int ret;
	int value_soc_alarm;

	fuelgauge->is_fuel_alerted = false;

	/* check status */
	ret = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_STATUS);
	if (ret < 0) {
		pr_err("%s: Failed to read SM5714_FG_REG_STATUS\n", __func__);
		return -1;
	}

	value_soc_alarm = soc;

	pr_info("%s: fg_irq= 0x%x, INT_STATUS=0x%x, SOC_ALARM=0x%x\n",
		__func__, fuelgauge->fg_irq, ret, value_soc_alarm);

	return 1;
}

static void sm5714_asoc_init(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret, temp;

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_2);
	temp = ret;
	ret = ret & 0x7F;
	if (ret == 0) {
		fuelgauge->info.soh = 100;
		temp = temp & 0x80;
		temp = temp | fuelgauge->info.soh;
		sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_2, temp);
		pr_info("%s : soh reset %d, write 0x%x\n", __func__, fuelgauge->info.soh, temp);
	} else {
		fuelgauge->info.soh = ret;
		pr_info("%s asoc restore : %d\n", __func__, fuelgauge->info.soh);
	}
}

static irqreturn_t sm5714_jig_irq_thread(int irq, void *irq_data)
{
	struct sm5714_fuelgauge_data *fuelgauge = irq_data;

	if (sm5714_check_jig_status(fuelgauge))
		sm5714_fg_reset_capacity_by_jig_connection(fuelgauge);
	else
		pr_info("%s: jig removed\n", __func__);
	return IRQ_HANDLED;
}

static void sm5714_fg_iocv_buffer_read(struct sm5714_fuelgauge_data *fuelgauge)
{
	int ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7;

	ret0 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V);
	ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+1);
	ret2 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+2);
	ret3 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+3);
	ret4 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+4);
	ret5 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+5);
	ret6 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+6);
	ret7 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_V+7);
	pr_info("%s: sm5714 FG buffer lb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7);

	ret0 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V);
	ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+1);
	ret2 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+2);
	ret3 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+3);
	ret4 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+4);
	ret5 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+5);
	ret6 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+6);
	ret7 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_V+7);
	pr_info("%s: sm5714 FG buffer cb_V = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7);

	ret0 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I);
	ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+1);
	ret2 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+2);
	ret3 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+3);
	ret4 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+4);
	ret5 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+5);
	ret6 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+6);
	ret7 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_LB_I+7);
	pr_info("%s: sm5714 FG buffer lb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7);

	ret0 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I);
	ret1 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+1);
	ret2 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+2);
	ret3 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+3);
	ret4 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+4);
	ret5 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+5);
	ret6 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+6);
	ret7 = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_START_CB_I+7);
	pr_info("%s: sm5714 FG buffer cb_I = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		__func__, ret0, ret1, ret2, ret3, ret4, ret5, ret6, ret7);
}

static bool sm5714_fg_init(struct sm5714_fuelgauge_data *fuelgauge, bool is_surge)
{
	int error_remain, ret;

	fuelgauge->info.is_FG_initialised = 0;

	if (sm5714_get_device_id(fuelgauge) < 0)
		return false;

	if (fuelgauge->pdata->jig_gpio) {
		int ret;
		/* if (fuelgauge->pdata->jig_low_active) { */
		if (0) {
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
				NULL, sm5714_jig_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"jig-irq", fuelgauge);
		} else {
			ret = request_threaded_irq(fuelgauge->pdata->jig_irq,
				NULL, sm5714_jig_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"jig-irq", fuelgauge);
		}
		if (ret) {
			pr_info("%s: Failed to Request IRQ\n",
				__func__);
		}
		pr_info("%s: jig_result : %d\n", __func__, sm5714_check_jig_status(fuelgauge));

		/* initial check for the JIG */
		if (sm5714_check_jig_status(fuelgauge))
			sm5714_fg_reset_capacity_by_jig_connection(fuelgauge);
	}

	fuelgauge->info.q_max_now = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_Q_MAX);
	pr_info("%s: q_max_now = 0x%x\n", __func__, fuelgauge->info.q_max_now);

	ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_USER_RESERV_1);
	error_remain = (ret & I2C_ERROR_REMAIN) ? 1 : 0;
	pr_info("%s: reserv_1 = 0x%x\n", __func__, ret);

	if (sm5714_fg_check_reg_init_need(fuelgauge) || error_remain) {
		if (sm5714_fg_reg_init(fuelgauge, is_surge))
			pr_info("%s: boot time kernel init DONE!\n", __func__);
		else
			pr_info("%s: ERROR!! boot time kernel init ERROR!!\n", __func__);
	}

	// factory mode initial SOC make;
	if (sm5714_get_facmode()
		&& !(sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_SYSTEM_STATUS) & 0x8000)) {
		ret = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_SOC);
		if (ret < 0) {
			pr_err("%s: Warning!!!! read soc reg fail\n", __func__);
			fuelgauge->info.batt_soc = 500;
		} else {
			fuelgauge->info.batt_soc = (ret * 10) >> 8;
		}
		pr_info("%s: nENQ4 low but factory mode, initial SOC make= %d\n", __func__, fuelgauge->info.batt_soc);
	}

    // curr_cal setup
	sm5714_ical_setup(fuelgauge);
	sm5714_vcal_setup(fuelgauge);

	/* write aux_ctrl cfg */
	sm5714_set_aux_ctrl_cfg(fuelgauge);

	sm5714_asoc_init(fuelgauge);

	fuelgauge->info.is_FG_initialised = 1;

	sm5714_fg_iocv_buffer_read(fuelgauge);

	return true;
}

bool sm5714_fg_fuelalert_init(struct sm5714_fuelgauge_data *fuelgauge,
				int soc)
{
	/* 1. Set sm5714 alert configuration. */
	if (sm5714_fg_alert_init(fuelgauge, soc) > 0)
		return true;
	else
		return false;
}

void sm5714_fg_fuelalert_set(struct sm5714_fuelgauge_data *fuelgauge,
				int enable)
{
	u16 ret;

	ret = sm5714_read_word(fuelgauge->i2c, SM5714_FG_REG_STATUS);
	pr_info("%s: SM5714_FG_REG_STATUS(0x%x)\n",
		__func__, ret);

	if (ret & ENABLE_VL_ALARM && !sm5714_get_lpmode() && !fuelgauge->is_charging) {
		pr_info("%s : Battery Voltage is Very Low!! SW V EMPTY ENABLE\n", __func__);

		if (fuelgauge->vempty_mode == VEMPTY_MODE_SW ||
					fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT) {
			fuelgauge->vempty_mode = VEMPTY_MODE_SW_VALERT;
		} else {
			union power_supply_propval value;

			value.intval = fuelgauge->vempty_mode;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_VOLTAGE_MIN, value);
		}
	}
}

bool sm5714_fg_fuelalert_process(void *irq_data)
{
	struct sm5714_fuelgauge_data *fuelgauge =
		(struct sm5714_fuelgauge_data *)irq_data;

	sm5714_fg_fuelalert_set(fuelgauge, 0);

	return true;
}

bool sm5714_fg_reset(struct sm5714_fuelgauge_data *fuelgauge, bool is_quickstart)
{
	if (fuelgauge->info.is_FG_initialised == 0) {
		pr_info("%s: Not work reset! prev init working! return!\n", __func__);
		return true;
	}

	pr_info("%s: Start fg reset\n", __func__);
	/* SW reset code */
	fuelgauge->info.is_FG_initialised = 0;
	sm5714_fg_verified_write_word(fuelgauge->i2c, SM5714_FG_REG_RESET, SW_RESET_CODE);
	/* delay 1000ms */
	msleep(1000);

	if (is_quickstart) {
		if (sm5714_fg_init(fuelgauge, false)) {
			pr_info("%s: Quick Start !!\n", __func__);
		} else {
			pr_info("%s: sm5714_fg_init ERROR!!!!\n", __func__);
			return false;
		}
	}
	else {
		if (sm5714_fg_init(fuelgauge, true)) {
			pr_info("%s: BATT_LONG_LIFE reset !!\n", __func__);
		} else {
			pr_info("%s: sm5714_fg_init ERROR!!!!\n", __func__);
			return false;
		}
	}

	pr_info("%s: End fg reset\n", __func__);

	return true;
}

#if defined(CONFIG_UI_SOC_PROLONGING)
static void sm5714_fg_adjust_capacity_max(
	struct sm5714_fuelgauge_data *fuelgauge, int curr_raw_soc)
{
	int diff = 0;

	if (fuelgauge->is_charging && fuelgauge->capacity_max_conv) {
		diff = curr_raw_soc - fuelgauge->prev_raw_soc;

		if ((diff >= 1) && (fuelgauge->capacity_max < fuelgauge->g_capacity_max)) {
			fuelgauge->capacity_max++;
		} else if ((fuelgauge->capacity_max >= fuelgauge->g_capacity_max) || (curr_raw_soc == 1000)) {
			fuelgauge->g_capacity_max = 0;
			fuelgauge->capacity_max_conv = false;
		}
		pr_info("%s: curr_raw_soc(%d) prev_raw_soc(%d) capacity_max_conv(%d) Capacity Max(%d | %d)\n",
			__func__, curr_raw_soc, fuelgauge->prev_raw_soc, fuelgauge->capacity_max_conv,
			fuelgauge->capacity_max, fuelgauge->g_capacity_max);
	}

	fuelgauge->prev_raw_soc = curr_raw_soc;
}
#endif

static void sm5714_fg_get_scaled_capacity(
	struct sm5714_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	int raw_soc = val->intval;

	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_info("%s: capacity max (%d) scaled capacity (%d.%d) raw_soc (%d.%d)\n",
		__func__, fuelgauge->capacity_max, val->intval / 10, val->intval % 10,
		raw_soc / 10, raw_soc % 10);
}


/* capacity is integer */
static void sm5714_fg_get_atomic_capacity(
	struct sm5714_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{

	pr_debug("%s : NOW(%d), OLD(%d)\n",
		__func__, val->intval, fuelgauge->capacity_old);

	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		if (fuelgauge->capacity_old < val->intval)
			val->intval = fuelgauge->capacity_old + 1;
		else if (fuelgauge->capacity_old > val->intval)
			val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s: capacity (old %d : new %d)\n",
			__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int sm5714_fg_check_capacity_max(
	int capacity_max, int cap_max, int cap_margin)
{
	int cap_min = 0;

	cap_min = (cap_max - cap_margin);

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

#if defined(CONFIG_UI_SOC_PROLONGING)
static void sm5714_fg_calculate_dynamic_scale(
	struct sm5714_fuelgauge_data *fuelgauge, int capacity, bool scale_by_full)
{
	union power_supply_propval raw_soc_val;
	int min_cap = fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin;
	int scaling_factor = 1;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc_val.intval = sm5714_get_soc(fuelgauge);

	if (capacity < 100)
		fuelgauge->capacity_max_conv = false;  //Force full sequence , need to decrease capacity_max

	if ((raw_soc_val.intval < min_cap) || (fuelgauge->capacity_max_conv)) {
		pr_info("%s: skip routine - raw_soc(%d), min_cap(%d), cap_max_conv(%d)\n",
				__func__, raw_soc_val.intval, min_cap, fuelgauge->capacity_max_conv);
		return;
}

	if (capacity == 100)
		scaling_factor = 2;

	fuelgauge->capacity_max = (raw_soc_val.intval * 100 / (capacity + scaling_factor));
	fuelgauge->capacity_old = capacity;

	fuelgauge->capacity_max =
		sm5714_fg_check_capacity_max(
			fuelgauge->capacity_max,
			fuelgauge->pdata->capacity_max,
			fuelgauge->pdata->capacity_max_margin);

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);
	if ((capacity == 100) && !fuelgauge->capacity_max_conv && scale_by_full) {
		fuelgauge->capacity_max_conv = true;
		fuelgauge->g_capacity_max = 990;
		pr_info("%s: Goal capacity max %d\n", __func__, fuelgauge->g_capacity_max);
	}
}
#else
static void sm5714_fg_calculate_dynamic_scale(
	struct sm5714_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	int min_cap = fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc_val.intval = sm5714_get_soc(fuelgauge);

	if (capacity < 100)
		fuelgauge->capacity_max_updated = false;  //Force full sequence , need to decrease capacity_max

	if (raw_soc_val.intval < min_cap || (fuelgauge->capacity_max_updated)) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val.intval);
		return;
	}

	fuelgauge->capacity_max =
		(raw_soc_val.intval * 100 / (capacity + 1));
	fuelgauge->capacity_old = capacity;

	fuelgauge->capacity_max =
		sm5714_fg_check_capacity_max(
			fuelgauge->capacity_max,
			fuelgauge->pdata->capacity_max,
			fuelgauge->pdata->capacity_max_margin);

	fuelgauge->capacity_max_updated = true;

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);
}
#endif

#if defined(CONFIG_EN_OOPS)
static void sm5714_set_full_value(struct sm5714_fuelgauge_data *fuelgauge,
					int cable_type)
{
	pr_info("%s : sm5714 todo\n",
		__func__);
}
#endif

static void sm5714_fg_set_vempty(struct sm5714_fuelgauge_data *fuelgauge, int vempty_mode)
{
	u16 data = 0;
	u32 value_v_alarm = 0;

	if (!fuelgauge->using_temp_compensation) {
		pr_info("%s: does not use temp compensation, default hw vempty\n", __func__);
		vempty_mode = VEMPTY_MODE_HW;
	}

	fuelgauge->vempty_mode = vempty_mode;

	switch (vempty_mode) {
	case VEMPTY_MODE_SW:
		/* HW Vempty Disable */

		/* set volt alert threshold */
		value_v_alarm = (fuelgauge->battery_data->sw_v_empty_vol - 2700)*109/10;

		if (sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL, value_v_alarm) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL);
		pr_info("%s: HW V EMPTY Disable, SW V EMPTY Enable with %d mV (%d)\n",
			__func__, fuelgauge->battery_data->sw_v_empty_vol, data);
		break;
	default:
		/* HW Vempty works together with CISD v_alarm */
		value_v_alarm = (fuelgauge->battery_data->sw_v_empty_vol_cisd - 2700)*109/10;

		if (sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL, value_v_alarm) < 0) {
			pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
			return;
		}
		data = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL);
		pr_info("%s: HW V EMPTY Enable, SW V EMPTY Disable %d mV (%d)\n",
			__func__, fuelgauge->battery_data->sw_v_empty_vol_cisd, data);
		break;
	}

	/* for v_alarm Hysteresis */
	value_v_alarm = (fuelgauge->info.value_v_alarm_hys)*109/10;
	if (sm5714_fg_write_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL_HYS, value_v_alarm) < 0) {
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return;
	}
	data = sm5714_fg_read_sram(fuelgauge, SM5714_FG_ADDR_SRAM_INTR_VL_HYS);
	pr_info("%s: VALRT_THRESHOLD hysteresis set %d mV (0x%x)\n",
		__func__, fuelgauge->info.value_v_alarm_hys, data);
}

static int sm5714_get_bat_id(int bat_id[], int bat_gpio_cnt)
{
	int battery_id = 0;
	int i = 0;

	for (i = (bat_gpio_cnt - 1); i >= 0; i--)
		battery_id += bat_id[i] << i;

	return battery_id;
}

static void sm5714_reset_bat_id(struct sm5714_fuelgauge_data *fuelgauge)
{
	int bat_id[BAT_GPIO_NO] = {0, };
	int i = 0;

	for (i = 0; i < fuelgauge->pdata->bat_gpio_cnt; i++)
		bat_id[i] = gpio_get_value(fuelgauge->pdata->bat_id_gpio[i]);

	fuelgauge->battery_data->battery_id =
			sm5714_get_bat_id(bat_id, fuelgauge->pdata->bat_gpio_cnt);
}

static void sm5714_fg_bd_log(struct sm5714_fuelgauge_data *fuelgauge)
{
	memset(fuelgauge->d_buf, 0x0, sizeof(fuelgauge->d_buf));

	snprintf(fuelgauge->d_buf + strlen(fuelgauge->d_buf), sizeof(fuelgauge->d_buf) - strlen(fuelgauge->d_buf),
		"%d,%d,%d",
		fuelgauge->info.batt_ocv,
		fuelgauge->info.batt_soc * 10,
		fuelgauge->capacity_max);
}

static int sm5714_fg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sm5714_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

#if defined(CONFIG_NO_BATTERY)
	return 0;
#endif

/*
 *	static int abnormal_current_cnt = 0;
 *	union power_supply_propval value;
 */

	if (atomic_read(&fuelgauge->shutdown_cnt) > 0) {
		dev_info(fuelgauge->dev, "%s: fuelgauge already shutdown\n", __func__);
		return -EINVAL;
	}

	switch ((int)psp) {
	/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		sm5714_get_vbat(fuelgauge);
		val->intval = fuelgauge->info.batt_voltage;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = fuelgauge->info.batt_avgvoltage;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = fuelgauge->info.batt_ocv;
		break;
	/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		sm5714_get_curr(fuelgauge);
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_current * 1000;
		else
			val->intval = fuelgauge->info.batt_current;
		break;
	/* Average Current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = fuelgauge->info.batt_avgcurrent * 1000;
		else
			val->intval = fuelgauge->info.batt_avgcurrent;
		break;
	/* Full Capacity */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			sm5714_get_cycle(fuelgauge);
			val->intval = fuelgauge->info.batt_soc_cycle;
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = fuelgauge->pdata->capacity_full;
			break;
		default:
			val->intval = -1;
			break;
		}
		break;
	/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		sm5714_update_all_value(fuelgauge);
#if !defined(CONFIG_SEC_FACTORY)
		/* for_debug */
		sm5714_dump_all(fuelgauge);
#else
		sm5714_fuel_gauge_abnormal_reg_check(fuelgauge);
#endif
		/* SM5714 F/G unit is 0.1%, raw ==> convert the unit to 0.01% */
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = fuelgauge->info.batt_soc * 10;
			break;
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			val->intval = fuelgauge->raw_capacity;
			break;
		}

		val->intval = fuelgauge->info.batt_soc;

#if defined(CONFIG_UI_SOC_PROLONGING)
		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE | SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {

			sm5714_fg_adjust_capacity_max(fuelgauge, val->intval);
			sm5714_fg_get_scaled_capacity(fuelgauge, val);

			if (val->intval > 1020) {
				pr_info("%s: scaled capacity (%d)\n", __func__, val->intval);
				sm5714_fg_calculate_dynamic_scale(fuelgauge, 100, false);
			}
		}

#else
		if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE | SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
			sm5714_fg_get_scaled_capacity(fuelgauge, val);
#endif

		/* capacity should be between 0% and 100%
		 * (0.1% degree)
		 */
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		fuelgauge->raw_capacity = val->intval;

		/* get only integer part */
		val->intval /= 10;

		/* SW/HW V Empty setting */
		if (fuelgauge->using_hw_vempty) {
			if (fuelgauge->info.temperature <= (int)fuelgauge->low_temp_limit) {
				if (fuelgauge->raw_capacity <= 50 &&
						fuelgauge->vempty_mode != VEMPTY_MODE_HW)
					sm5714_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
				else if (fuelgauge->raw_capacity > 50 &&
						fuelgauge->vempty_mode == VEMPTY_MODE_HW) {
					sm5714_fg_set_vempty(fuelgauge, VEMPTY_MODE_SW);
				}
			} else if (fuelgauge->vempty_mode != VEMPTY_MODE_HW) {
				sm5714_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
			}
		}

		if (!fuelgauge->is_charging &&
				fuelgauge->vempty_mode == VEMPTY_MODE_SW_VALERT && !sm5714_get_lpmode()) {
			pr_info("%s : SW V EMPTY. Decrease SOC\n", __func__);
			if (fuelgauge->capacity_old > 0)
				val->intval = fuelgauge->capacity_old - 1;
			else
				val->intval = 0;
		} else if ((fuelgauge->vempty_mode == VEMPTY_MODE_SW_RECOVERY) &&
				(val->intval == fuelgauge->capacity_old)) {
			fuelgauge->vempty_mode = VEMPTY_MODE_SW;
		}

		/* check whether doing the wake_unlock */
		if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
				fuelgauge->is_fuel_alerted) {
			sm5714_fg_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc);
		}

#if defined(CONFIG_UI_SOC_PROLONGING)
		/* Check UI soc reached 100% from 99% , no need to adjust now */
		if ((val->intval == 100) && (fuelgauge->capacity_old < 100) &&
			(fuelgauge->capacity_max_conv == true))
			fuelgauge->capacity_max_conv = false;
#else
/* Check UI soc reached 100% from 99% via forced charge sequence,
 * capacity_max needs to be updated again after cable disconnection.
 */
		if ((val->intval == 100) && (fuelgauge->capacity_old < 100) &&
			(fuelgauge->capacity_max_updated == true))
			fuelgauge->capacity_max_updated = false;
#endif

		/* (Only for atomic capacity)
		 * In initial time, capacity_old is 0.
		 * and in resume from sleep,
		 * capacity_old is too different from actual soc.
		 * should update capacity_old
		 * by val->intval in booting or resume.
		 */
		if (fuelgauge->initial_update_of_soc) {
			fuelgauge->initial_update_of_soc = false;
			if (fuelgauge->vempty_mode != VEMPTY_MODE_SW_VALERT) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				break;
			}
		}

		if (fuelgauge->sleep_initial_update_of_soc) {
			fuelgauge->sleep_initial_update_of_soc = false;
			/* updated old capacity in case of resume */
			if (fuelgauge->is_charging ||
					((!fuelgauge->is_charging) && (fuelgauge->capacity_old >= val->intval))) {
				fuelgauge->capacity_old = val->intval;
				break;
			}
		}

		if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
				 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL)){
			sm5714_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		sm5714_get_temperature(fuelgauge);
		val->intval = fuelgauge->info.temp_fg;
		break;
#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = sm5714_get_asoc(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval =
			fuelgauge->battery_data->Capacity * fuelgauge->raw_capacity;
		pr_info("%s: charge_counter = %d\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		return -ENODATA;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_JIG_GPIO:
			if (fuelgauge->pdata->jig_gpio)
				val->intval = gpio_get_value(fuelgauge->pdata->jig_gpio);
			else
				val->intval = -1;
			pr_info("%s: jig gpio = %d\n", __func__, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			break;
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			if (!val->intval) {
				if (fuelgauge->pdata->bat_gpio_cnt > 0)
					sm5714_reset_bat_id(fuelgauge);
				val->intval = fuelgauge->battery_data->battery_id;
				pr_info("%s: bat_id_gpio = %d\n", __func__, val->intval);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_DUMP:
			sm5714_fg_bd_log(fuelgauge);
			val->strval = fuelgauge->d_buf;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int sm5714_fuelgauge_parse_dt(struct sm5714_fuelgauge_data *fuelgauge);
#endif
static int sm5714_fg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sm5714_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	/*u8 data[2] = {0, 0}; */

	if (atomic_read(&fuelgauge->shutdown_cnt) > 0) {
		dev_info(fuelgauge->dev, "%s: fuelgauge already shutdown\n", __func__);
		return -EINVAL;
	}

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL) {
			pr_info("%s: POWER_SUPPLY_PROP_CHARGE_FULL : q_max_now = 0x%x\n",
				__func__, fuelgauge->info.q_max_now);
			if (fuelgauge->info.q_max_now !=
				fuelgauge->info.q_max_table[get_v_max_index_by_cycle(fuelgauge)]) {
				if (!sm5714_fg_reset(fuelgauge, false))
					return -EINVAL;
			}
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
#if defined(CONFIG_UI_SOC_PROLONGING)
			sm5714_fg_calculate_dynamic_scale(fuelgauge, val->intval, true);
#else
			sm5714_fg_calculate_dynamic_scale(fuelgauge, val->intval);
#endif
		break;

#if defined(CONFIG_EN_OOPS)
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		sm5714_set_full_value(fuelgauge, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (is_nocharge_type(fuelgauge->cable_type)) {
			fuelgauge->ta_exist = false;
			fuelgauge->is_charging = false;
		} else {
			fuelgauge->ta_exist = true;
			fuelgauge->is_charging = true;

			/* enable alert */
			if (fuelgauge->vempty_mode >= VEMPTY_MODE_SW_VALERT) {
				sm5714_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);
				fuelgauge->initial_update_of_soc = true;
				sm5714_fg_fuelalert_init(fuelgauge,
							fuelgauge->pdata->fuel_alert_soc);
			}
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			fuelgauge->initial_update_of_soc = true;
			if (!sm5714_fg_reset(fuelgauge, true))
				return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		fuelgauge->info.temperature = val->intval;
		if (val->intval < 0)
			pr_info("%s: set the low temp reset! temp : %d\n",
				__func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		fuelgauge->info.flag_charge_health =
		(val->intval == POWER_SUPPLY_HEALTH_GOOD) ? 1 : 0;
		pr_info("%s: charge health from charger = 0x%x\n", __func__, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		sm5714_fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		pr_info("%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max =
			sm5714_fg_check_capacity_max(
				val->intval,
				fuelgauge->pdata->capacity_max,
				fuelgauge->pdata->capacity_max_margin);
		fuelgauge->initial_update_of_soc = true;
#if defined(CONFIG_UI_SOC_PROLONGING)
		fuelgauge->g_capacity_max = 990;
		fuelgauge->capacity_max_conv = true;
#endif
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		pr_info("%s: full condition soc changed, %d -> %d\n",
			__func__, fuelgauge->chg_full_soc, val->intval);
		fuelgauge->chg_full_soc = val->intval;
		break;

#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case POWER_SUPPLY_PROP_POWER_DESIGN:
		sm5714_fuelgauge_parse_dt(fuelgauge);
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			sm5714_fg_set_jig_mode_real_vbat(fuelgauge, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY:
			if (val->intval) {
				pr_info("%s: bypass mode is enabled\n", __func__);
				sm5714_fg_reset_capacity_by_jig_connection(fuelgauge);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void sm5714_fg_isr_work(struct work_struct *work)
{
	struct sm5714_fuelgauge_data *fuelgauge =
		container_of(work, struct sm5714_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	sm5714_fg_fuelalert_process(fuelgauge);

	__pm_relax(fuelgauge->fuel_alert_ws);
}

static irqreturn_t sm5714_fg_irq_thread(int irq, void *irq_data)
{
	struct sm5714_fuelgauge_data *fuelgauge = irq_data;

	pr_info("%s\n", __func__);

	if (fuelgauge->is_fuel_alerted)
		return IRQ_HANDLED;

	__pm_stay_awake(fuelgauge->fuel_alert_ws);
	fuelgauge->is_fuel_alerted = true;
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

static int sm5714_fuelgauge_debugfs_show(struct seq_file *s, void *data)
{
	struct sm5714_fuelgauge_data *fuelgauge = s->private;
	int i;
	u8 reg;
	u8 reg_data;

	seq_puts(s, "SM5714 FUELGAUGE IC :\n");
	seq_puts(s, "===================\n");
	for (i = 0; i < 16; i++) {
		if (i == 12)
			continue;
		for (reg = 0; reg < 0x10; reg++) {
			reg_data = sm5714_read_word(fuelgauge->i2c, reg + i * 0x10);
			seq_printf(s, "0x%02x:\t0x%04x\n", reg + i * 0x10, reg_data);
		}
		if (i == 4)
			i = 10;
	}
	seq_puts(s, "\n");
	return 0;
}

static int sm5714_fuelgauge_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, sm5714_fuelgauge_debugfs_show, inode->i_private);
}

static const struct file_operations sm5714_fuelgauge_debugfs_fops = {
	.open			= sm5714_fuelgauge_debugfs_open,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s: IRQ(%d):%s free done\n", \
				__func__, _irq, _name);			\
	}								\
} while (0)

static void sm5714_fuelgauge_free_irqs(struct sm5714_fuelgauge_data *fuelgauge)
{

	if (fuelgauge->fg_irq) {
		pr_info("%s: start\n", __func__);

		/* free fuelgauge IRQ */
		FREE_IRQ(fuelgauge->fg_irq, fuelgauge, "fuelgauge-irq");

		pr_info("%s: end\n", __func__);
	}
}

#ifdef CONFIG_OF
#define PROPERTY_NAME_SIZE 128

#define PINFO(format, args...) \
	pr_info("%s() line-%d: " format, __func__, __LINE__, ## args)

static void temp_parse_dt(struct sm5714_fuelgauge_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "battery");
	int ret = 0, len = 0;
	unsigned int i = 0;
	bool age_data_by_common_offset = of_property_read_bool(np, "battery,age_data_by_common_offset");

	pr_info("%s: age_data_by_common_offset is %s", __func__, (age_data_by_common_offset ? "true" : "false"));

	if (age_data_by_common_offset) {
		char *age_data_soc_str = "battery,age_data_full_condition_soc";
		len = of_property_count_u32_elems(np, age_data_soc_str);
		if (len > 0) {
			pdata->num_age_step = len;
			pdata->age_data_soc = kcalloc(pdata->num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, age_data_soc_str, pdata->age_data_soc, len);
		} else {
			pr_info("%s : Error in Calculating %s Size - %d\n", __func__, age_data_soc_str, len);
		}

	} else {
		sec_age_data_t *age_data_temp;

		len = of_property_count_u32_elems(np, "battery,age_data");
		if (len > 0) {
			pdata->num_age_step = (sizeof(u32) * len) / sizeof(sec_age_data_t);
			age_data_temp = kzalloc((sizeof(u32) * len), GFP_KERNEL);
			pdata->age_data_soc = kcalloc(pdata->num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,age_data", (u32 *)age_data_temp, len);
			if (!ret) {
				for (i = 0; i < pdata->num_age_step; ++i)
					pdata->age_data_soc[i] = age_data_temp[i].full_condition_soc;
			}
			kfree(age_data_temp);
		}
	}
	if (ret) {
		pr_err("%s failed to read battery->pdata->age_data: %d\n", __func__, ret);
		kfree(pdata->age_data_soc);
		pdata->age_data_soc = NULL;
		pdata->num_age_step = 0;
	} else {
		pr_err("%s num_age_step : %d\n", __func__, pdata->num_age_step);
		for (i = 0; i < pdata->num_age_step; ++i)
			pr_err("%s [%d/%d] soc:%d", __func__, i, pdata->num_age_step-1, pdata->age_data_soc[i]);
	}
}

static int sm5714_fuelgauge_parse_dt(struct sm5714_fuelgauge_data *fuelgauge)
{
	struct sm5714_fuelgauge_platform_data *pdata = fuelgauge->pdata;
	char prop_name[PROPERTY_NAME_SIZE];
	int battery_id = -1;
	int table[24];
	int v_max_table[5];
	int q_max_table[5];
	int rs_value[7];
	int battery_type[3];
	int v_alarm[2];
	int set_temp_poff[4];
	int i_cal[8];
	int v_cal[7];
	int aux_ctrl[2];

	int ret;
	int i, j;
	int bat_id[BAT_GPIO_NO] = {0, };

	struct device_node *np = of_find_node_by_name(NULL, "sm5714-fuelgauge");

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0) {
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);
			fuelgauge->pdata->capacity_max_margin = 300;
		}
		ret = of_property_read_u32(np, "fuelgauge,capacity",
				&fuelgauge->pdata->capacity_full);
		if (ret < 0)
			pr_err("%s error reading capacity %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		pr_info("%s: capacity_max: %d, capacity_min: %d, capacity_max_margin: %d\n",
			__func__, fuelgauge->pdata->capacity_max, fuelgauge->pdata->capacity_min,
			fuelgauge->pdata->capacity_max_margin);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgaguge,repeated_fuelalert");

		pr_info("%s: calculation_type: 0x%x, fuel_alert_soc: %d,\n"
				"repeated_fuelalert: %d\n", __func__,
				fuelgauge->pdata->capacity_calculation_type,
				fuelgauge->pdata->fuel_alert_soc, fuelgauge->pdata->repeated_fuelalert);


		fuelgauge->using_temp_compensation = of_property_read_bool(np,
							"fuelgauge,using_temp_compensation");
		if (fuelgauge->using_temp_compensation) {
			ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
						&fuelgauge->low_temp_limit);
			if (ret < 0)
				pr_err("%s error reading low temp limit %d\n", __func__, ret);

			pr_info("%s : LOW TEMP LIMIT(%d)\n",
				__func__, fuelgauge->low_temp_limit);
		}

		fuelgauge->using_hw_vempty = of_property_read_bool(np,
									"fuelgauge,using_hw_vempty");
		if (fuelgauge->using_hw_vempty) {
			ret = of_property_read_u32(np, "fuelgauge,v_empty",
						&fuelgauge->battery_data->V_empty);
			if (ret < 0)
				pr_err("%s error reading v_empty %d\n",
						__func__, ret);

			ret = of_property_read_u32(np, "fuelgauge,v_empty_origin",
						&fuelgauge->battery_data->V_empty_origin);
			if (ret < 0)
				pr_err("%s error reading v_empty_origin %d\n",
						__func__, ret);

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage_cisd",
					&fuelgauge->battery_data->sw_v_empty_vol_cisd);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_default_vol_cise %d\n",
						__func__, ret);
				fuelgauge->battery_data->sw_v_empty_vol_cisd = 3100;
			}

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_voltage",
						   &fuelgauge->battery_data->sw_v_empty_vol);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_default_vol %d\n",
					   __func__, ret);
				fuelgauge->battery_data->sw_v_empty_vol	= 3200;
			}

			ret = of_property_read_u32(np, "fuelgauge,sw_v_empty_recover_voltage",
						   &fuelgauge->battery_data->sw_v_empty_recover_vol);
			if (ret < 0) {
				pr_err("%s error reading sw_v_empty_recover_vol %d\n",
					   __func__, ret);
				fuelgauge->battery_data->sw_v_empty_recover_vol = 3480;
			}

			ret = of_property_read_u32(np, "fuelgauge,vbat_ovp",
						   &fuelgauge->battery_data->vbat_ovp);
			if (ret < 0) {
				pr_err("%s error reading vbat_ovp %d\n",
					   __func__, ret);
				fuelgauge->battery_data->vbat_ovp = 4400;
			}

			pr_info("%s : SW V Empty (%d)mV,  SW V Empty recover (%d)mV, CISD V_Alarm (%d)mV, Vbat OVP (%d)mV\n",
				__func__,
				fuelgauge->battery_data->sw_v_empty_vol,
				fuelgauge->battery_data->sw_v_empty_recover_vol,
				fuelgauge->battery_data->sw_v_empty_vol_cisd,
				fuelgauge->battery_data->vbat_ovp);
		}

		fuelgauge->pdata->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
		if (fuelgauge->pdata->jig_gpio < 0) {
			pr_err("%s error reading jig_gpio = %d\n",
					__func__, fuelgauge->pdata->jig_gpio);
			fuelgauge->pdata->jig_gpio = 0;
		} else {
			fuelgauge->pdata->jig_irq = gpio_to_irq(fuelgauge->pdata->jig_gpio);
		}
/*
 *		if (fuelgauge->pdata->jig_gpio) {
 *			ret = of_property_read_u32(np, "fuelgauge,jig_low_active",
 *						&fuelgauge->pdata->jig_low_active);

 *			if (ret < 0) {
 *				pr_err("%s error reading jig_low_active %d\n", __func__, ret);
 *				fuelgauge->pdata->jig_low_active = 0;
 *			}
 *		}
 */
		ret = of_property_read_u32(np, "fuelgauge,fg_resistor",
				&fuelgauge->fg_resistor);
		if (ret < 0) {
			pr_err("%s error reading fg_resistor %d\n",
					__func__, ret);
			fuelgauge->fg_resistor = 1;
		}


#if defined(CONFIG_EN_OOPS)
		/* todo cap redesign */
#endif

		ret = of_property_read_u32(np, "fuelgauge,capacity",
						&fuelgauge->battery_data->Capacity);
		if (ret < 0)
			pr_err("%s error reading Capacity %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "battery,full_condition_soc",
			&fuelgauge->pdata->full_condition_soc);
		if (ret) {
			fuelgauge->pdata->full_condition_soc = 93;
			pr_info("%s : Full condition soc is Empty\n", __func__);
		}
	}

	pdata->bat_gpio_cnt = of_gpio_named_count(np, "fuelgauge,bat_id_gpio");
	/* not run if gpio gpio cnt is less than 1 */
	if (pdata->bat_gpio_cnt > 0) {
		pr_info("%s: Has %d bat_id_gpios\n", __func__, pdata->bat_gpio_cnt);
		if (pdata->bat_gpio_cnt > BAT_GPIO_NO) {
			pr_err("%s: bat_id_gpio, catch out-of bounds array read\n",
					__func__);
			pdata->bat_gpio_cnt = BAT_GPIO_NO;
		}
		for (i = 0; i < pdata->bat_gpio_cnt; i++) {
			pdata->bat_id_gpio[i] = of_get_named_gpio(np, "fuelgauge,bat_id_gpio", i);
			if (pdata->bat_id_gpio[i] >= 0) {
				bat_id[i] = gpio_get_value(pdata->bat_id_gpio[i]);
			} else {
				pr_err("%s: error reading bat_id_gpio = %d\n",
					__func__, pdata->bat_id_gpio[i]);
				bat_id[i] = 0;
			}
		}
		fuelgauge->battery_data->battery_id =
				sm5714_get_bat_id(bat_id, pdata->bat_gpio_cnt);

		pr_info("%s: battery_id (gpio) = %d\n", __func__, fuelgauge->battery_data->battery_id);
	} else {
		fuelgauge->battery_data->battery_id = 0;
	}

	/* get battery_params node for reg init */
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		PINFO("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	if (fuelgauge->battery_data->battery_id >= 0) {
		battery_id = fuelgauge->battery_data->battery_id;

		snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
		ret = of_property_read_u32_array(np, prop_name, battery_type, 2);
		if (ret < 0) {
			PINFO("no battery_id(%d) data\n", battery_id);
			/* get battery_id */
			battery_id = 0;
			if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
				PINFO("not battery,id property\n");
			PINFO("use battery default id = %d\n", battery_id);
		}
	} else {
		if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
			PINFO("not battery,id property\n");
	}
	PINFO("battery id = %d\n", battery_id);

	/* get battery_table */
	for (i = DISCHARGE_TABLE; i < TABLE_MAX; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE,
			"battery%d,%s%d", battery_id, "battery_table", i);

		ret = of_property_read_u32_array(np, prop_name, table, 24);
		if (ret < 0)
			PINFO("Can get prop %s (%d)\n", prop_name, ret);

		for (j = 0; j <= FG_TABLE_LEN; j++) {
			fuelgauge->info.battery_table[i][j] = table[j];
			PINFO("%s = <table[%d][%d] 0x%x>\n", prop_name, i, j, table[j]);
		}
	}

	snprintf(prop_name, PROPERTY_NAME_SIZE,
			"battery%d,%s%d", battery_id, "battery_table", i);

	ret = of_property_read_u32_array(np, prop_name, table, 16);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);

	for (j = 0; j <= FG_ADD_TABLE_LEN; j++) {
		fuelgauge->info.battery_table[i][j] = table[j];
		PINFO("%s = <table[%d][%d] 0x%x>\n", prop_name, i, j, table[j]);
	}

	/* get rs_value */
	for (i = 0; i < 7; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rs_value");
		ret = of_property_read_u32_array(np, prop_name, rs_value, 7);
		if (ret < 0)
			PINFO("Can get prop %s (%d)\n", prop_name, ret);

		fuelgauge->info.rs_value[i] = rs_value[i];
	}
	/*spare min max factor chg_factor dischg_factor manvalue*/
	PINFO("%s = <0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x>\n", prop_name,
		rs_value[0], rs_value[1], rs_value[2], rs_value[3], rs_value[4], rs_value[5], rs_value[6]);

	/* battery_type */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
	ret = of_property_read_u32_array(np, prop_name, battery_type, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.batt_v_max = battery_type[0];
	fuelgauge->info.cap = battery_type[1];
	fuelgauge->info.maxcap = battery_type[1];

	PINFO("%s = <%d 0x%x>\n", prop_name,
		fuelgauge->info.batt_v_max, fuelgauge->info.cap);

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_max_table");
	ret = of_property_read_u32_array(np, prop_name, v_max_table, fuelgauge->pdata->num_age_step);

	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);

		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.v_max_table[i] = fuelgauge->info.battery_table[DISCHARGE_TABLE][FG_TABLE_LEN-1];
			PINFO("%s = <v_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.v_max_table[i]);
		}
	} else {
		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.v_max_table[i] = v_max_table[i];
			PINFO("%s = <v_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.v_max_table[i]);
		}
	}

	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "q_max_table");
	ret = of_property_read_u32_array(np, prop_name, q_max_table, fuelgauge->pdata->num_age_step);

	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);

		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.q_max_table[i] = fuelgauge->info.cap;
			PINFO("%s = <q_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.q_max_table[i]);
		}
	} else {
		for (i = 0; i < fuelgauge->pdata->num_age_step; i++) {
			fuelgauge->info.q_max_table[i] = q_max_table[i];
			PINFO("%s = <q_max_table[%d] 0x%x>\n", prop_name, i, fuelgauge->info.q_max_table[i]);
		}
	}
	fuelgauge->chg_full_soc = fuelgauge->pdata->age_data_soc ?
		fuelgauge->pdata->age_data_soc[0] : fuelgauge->pdata->full_condition_soc;
	fuelgauge->info.v_max_now = fuelgauge->info.v_max_table[0];
	fuelgauge->info.q_max_now = fuelgauge->info.q_max_table[0];
	PINFO("%s = <v_max_now = 0x%x>, <q_max_now = 0x%x>, <chg_full_soc = %d>\n", prop_name,
		fuelgauge->info.v_max_now, fuelgauge->info.q_max_now, fuelgauge->chg_full_soc);

	/* V_ALARM */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_alarm");
	ret = of_property_read_u32_array(np, prop_name, v_alarm, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.value_v_alarm = v_alarm[0];
	fuelgauge->info.value_v_alarm_hys = v_alarm[1];
	PINFO("%s = <%d %d>\n", prop_name, fuelgauge->info.value_v_alarm, fuelgauge->info.value_v_alarm_hys);

    /* TOP OFF */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "topoff");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.top_off, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%d>\n", prop_name, fuelgauge->info.top_off);

	/* i CAL */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "i_cal");
	ret = of_property_read_u32_array(np, prop_name, i_cal, 8);
	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	} else {
		fuelgauge->info.i_dp_default = i_cal[0];
		fuelgauge->info.dp_i_off = i_cal[1];
		fuelgauge->info.dp_i_pslo = i_cal[2];
		fuelgauge->info.dp_i_nslo = i_cal[3];
		fuelgauge->info.i_alg_default = i_cal[4];
		fuelgauge->info.alg_i_off = i_cal[5];
		fuelgauge->info.alg_i_pslo = i_cal[6];
		fuelgauge->info.alg_i_nslo = i_cal[7];
	}
	PINFO("%s = dp : [%d]<0x%x 0x%x 0x%x> alg : [%d]<0x%x 0x%x 0x%x>\n", prop_name,
		fuelgauge->info.i_dp_default, fuelgauge->info.dp_i_off,
		fuelgauge->info.dp_i_pslo, fuelgauge->info.dp_i_nslo,
		fuelgauge->info.i_alg_default, fuelgauge->info.alg_i_off,
		fuelgauge->info.alg_i_pslo, fuelgauge->info.alg_i_nslo);

	/* v CAL */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "v_cal");
	ret = of_property_read_u32_array(np, prop_name, v_cal, 7);
	if (ret < 0) {
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	} else {
		fuelgauge->info.v_default = v_cal[0];
		fuelgauge->info.v_off = v_cal[1];
		fuelgauge->info.v_slo = v_cal[2];
		fuelgauge->info.vt_default = v_cal[3];
		fuelgauge->info.vtt = v_cal[4];
		fuelgauge->info.ivt = v_cal[5];
		fuelgauge->info.ivv = v_cal[6];
	}
	PINFO("%s = v : [%d]<0x%x 0x%x> vt : [%d]<0x%x 0x%x 0x%x>\n", prop_name,
		fuelgauge->info.v_default, fuelgauge->info.v_off, fuelgauge->info.v_slo
		, fuelgauge->info.vt_default, fuelgauge->info.vtt, fuelgauge->info.ivt, fuelgauge->info.ivv);

	/* temp_std */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_std");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.temp_std, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%d>\n", prop_name, fuelgauge->info.temp_std);

	/* tem poff level */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tem_poff");
	ret = of_property_read_u32_array(np, prop_name, set_temp_poff, 4);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.n_tem_poff = set_temp_poff[0];
	fuelgauge->info.n_tem_poff_offset = set_temp_poff[1];
	fuelgauge->info.l_tem_poff = set_temp_poff[2];
	fuelgauge->info.l_tem_poff_offset = set_temp_poff[3];

	PINFO("%s = <%d, %d, %d, %d>\n",
		prop_name,
		fuelgauge->info.n_tem_poff, fuelgauge->info.n_tem_poff_offset,
		fuelgauge->info.l_tem_poff, fuelgauge->info.l_tem_poff_offset);

	/* aux_ctrl setting */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "aux_ctrl");
	ret = of_property_read_u32_array(np, prop_name, aux_ctrl, 2);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	fuelgauge->info.aux_ctrl[0] = aux_ctrl[0];
	fuelgauge->info.aux_ctrl[1] = aux_ctrl[1];

	/* batt data version */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "data_ver");
	ret = of_property_read_u32_array(np, prop_name, &fuelgauge->info.data_ver, 1);
	if (ret < 0)
		PINFO("Can get prop %s (%d)\n", prop_name, ret);
	PINFO("%s = <%d>\n", prop_name, fuelgauge->info.data_ver);


	return 0;
}
#endif

static struct device_attribute sm5714_attrs[] = {
	sm5714_FG_ATTR(chip_id),
	sm5714_FG_ATTR(data),
	sm5714_FG_ATTR(data_1),
	sm5714_FG_ATTR(data_sram),
	sm5714_FG_ATTR(data_sram_1),
};

static int sm5714_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(sm5714_attrs); i++) {
		rc = device_create_file(dev, &sm5714_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sm5714_attrs[i]);
	return rc;
}

ssize_t sm5714_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5714_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sm5714_attrs;
	int i = 0;
	u16 data;
	u8 addr;

	switch (offset) {
	case CHIP_ID:
	case DATA:
		for (addr = 0; addr < (int)ARRAY_SIZE(sm5714_regs); addr++) {
			data = -1;
			data = sm5714_read_word(fuelgauge->i2c, sm5714_regs[addr]);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02X[0x%04X],", sm5714_regs[addr], data);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
		break;
	case DATA_1:
		data = sm5714_read_word(fuelgauge->i2c, fuelgauge->read_reg);
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%02X : 0x%04X\n", fuelgauge->read_reg, data);
		break;
	case DATA_SRAM:
		for (addr = 0; addr < (int)ARRAY_SIZE(sm5714_srams); addr++) {
			data = -1;
			data = sm5714_fg_read_sram(fuelgauge, sm5714_srams[addr]);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02X[0x%04X],", sm5714_srams[addr], data);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
		break;
	case DATA_SRAM_1:
		data = sm5714_fg_read_sram(fuelgauge, fuelgauge->read_sram_reg);
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%02X : 0x%04X\n", fuelgauge->read_sram_reg, data);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t sm5714_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5714_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sm5714_attrs;
	int ret = 0;
	int addr, data;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &addr, &data) == 2) {
			dev_info(fuelgauge->dev,
					"%s: addr: 0x%x, data: 0x%x\n", __func__, addr, data);
			if (sm5714_fg_find_addr(addr)) {
				if (sm5714_write_word(fuelgauge->i2c, addr, data) < 0) {
					dev_info(fuelgauge->dev,
						"%s: addr: 0x%x write fail\n", __func__, addr);
					ret = -EINVAL;
				}
			} else {
				dev_info(fuelgauge->dev,
					"%s: addr: 0x%x is wrong\n", __func__, addr);
				ret = -EINVAL;
			}
		}
		ret = count;
		break;
	case DATA_1:
		if (sscanf(buf, "0x%8x", &addr) == 1)
			fuelgauge->read_reg = addr;
		ret = count;
		break;
	case DATA_SRAM:
		if (sscanf(buf, "0x%8x 0x%8x", &addr, &data) == 2) {
			dev_info(fuelgauge->dev,
					"%s: addr: 0x%x, data: 0x%x\n", __func__, addr, data);
			if (sm5714_fg_find_sram_addr(addr)) {
				if (sm5714_fg_write_sram(fuelgauge, addr, data) < 0) {
					dev_info(fuelgauge->dev,
						"%s: addr: 0x%x write fail\n", __func__, addr);
					ret = -EINVAL;
				}
			} else {
				dev_info(fuelgauge->dev,
					"%s: addr: 0x%x is wrong\n", __func__, addr);
				ret = -EINVAL;
			}
		}
		ret = count;
		break;
	case DATA_SRAM_1:
		if (sscanf(buf, "0x%8x", &addr) == 1)
			fuelgauge->read_sram_reg = addr;
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}


static const struct power_supply_desc sm5714_fuelgauge_power_supply_desc = {
	.name = "sm5714-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sm5714_fuelgauge_props,
	.num_properties = ARRAY_SIZE(sm5714_fuelgauge_props),
	.get_property = sm5714_fg_get_property,
	.set_property = sm5714_fg_set_property,
};

static int sm5714_fuelgauge_probe(struct platform_device *pdev)
{
	struct sm5714_dev *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_platform_data *pdata = dev_get_platdata(sm5714->dev);
	struct sm5714_fuelgauge_data *fuelgauge;
	struct sm5714_fuelgauge_platform_data *fuelgauge_data;
	/* struct power_supply_config fuelgauge_cfg = {}; */
	struct power_supply_config psy_fg = {};
	int ret = 0;
	union power_supply_propval raw_soc_val;
#if defined(CONFIG_DISABLE_SAVE_CAPACITY_MAX)
	u16 reg_data;
#endif

	pr_info("%s: SM5714 Fuelgauge Driver Loading probe start\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	fuelgauge_data = kzalloc(sizeof(struct sm5714_fuelgauge_platform_data), GFP_KERNEL);
	if (!fuelgauge_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->pdata = fuelgauge_data;
	fuelgauge->i2c = sm5714->fuelgauge;
	/* fuelgauge->pmic = sm5714->i2c; */
	fuelgauge->sm5714_pdata = pdata;

	fuelgauge->pmic_rev = sm5714->pmic_rev;
	fuelgauge->vender_id = sm5714->vender_id;

	atomic_set(&fuelgauge->shutdown_cnt, 0);

	temp_parse_dt(fuelgauge->pdata);

#if defined(CONFIG_OF)
	fuelgauge->battery_data = kzalloc(sizeof(struct battery_data_t),
					GFP_KERNEL);
	if (!fuelgauge->battery_data) {
		ret = -ENOMEM;
		goto err_pdata_free;
	}
	ret = sm5714_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0) {
		pr_err("%s not found fuelgauge dt! ret[%d]\n",
				__func__, ret);
		goto err_data_free;
	}
#endif

	/* initialize value */
	fuelgauge->isjigmoderealvbat = false;

	platform_set_drvdata(pdev, fuelgauge);

	(void) debugfs_create_file("sm5714-fuelgauge-regs",
		0444, NULL, (void *)fuelgauge, &sm5714_fuelgauge_debugfs_fops);

	if (!sm5714_fg_init(fuelgauge, false)) {
		pr_err("%s: Failed to Initialize Fuelgauge\n", __func__);
		goto err_data_free;
	}

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = sm5714_get_soc(fuelgauge);

#if defined(CONFIG_DISABLE_SAVE_CAPACITY_MAX)
	pr_info("%s : CONFIG_DISABLE_SAVE_CAPACITY_MAX\n",
		__func__);
#endif

#if defined(CONFIG_UI_SOC_PROLONGING)
	if (raw_soc_val.intval > fuelgauge->capacity_max)
		sm5714_fg_calculate_dynamic_scale(fuelgauge, 100, false);
#else
	if (raw_soc_val.intval > fuelgauge->capacity_max)
		sm5714_fg_calculate_dynamic_scale(fuelgauge, 100);
#endif

	/* SW/HW init code. SW/HW V Empty mode must be opposite ! */
	fuelgauge->info.temperature = 300; /* default value */
	pr_info("%s: SW/HW V empty init\n", __func__);
	sm5714_fg_set_vempty(fuelgauge, VEMPTY_MODE_HW);

	psy_fg.drv_data = fuelgauge;
	psy_fg.supplied_to = sm5714_fg_supplied_to;
	psy_fg.num_supplicants = ARRAY_SIZE(sm5714_fg_supplied_to),

	fuelgauge->psy_fg = power_supply_register(&pdev->dev, &sm5714_fuelgauge_power_supply_desc, &psy_fg);
	if (!fuelgauge->psy_fg) {
		dev_err(&pdev->dev, "%s: failed to power supply fg register", __func__);
		goto err_data_free;
	}

	fuelgauge->fg_irq = pdata->irq_base + SM5714_FG_IRQ_INT_LOW_VOLTAGE;
	pr_info("[%s]IRQ_BASE(%d) FG_IRQ(%d)\n",
		__func__, pdata->irq_base, fuelgauge->fg_irq);

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (sm5714_fg_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc)) {
			fuelgauge->fuel_alert_ws = wakeup_source_register(&pdev->dev, "fuel_alerted");
			if (fuelgauge->fg_irq) {
				INIT_DELAYED_WORK(&fuelgauge->isr_work, sm5714_fg_isr_work);

				ret = request_threaded_irq(fuelgauge->fg_irq,
						NULL, sm5714_fg_irq_thread,
						0,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					pr_err("%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}
			}
		} else {
			pr_err("%s: Failed to Initialize Fuel-alert\n",
					__func__);
			goto err_supply_unreg;
		}
	}

	fuelgauge->sleep_initial_update_of_soc = false;
	fuelgauge->initial_update_of_soc = true;
	ret = sm5714_fg_create_attrs(&fuelgauge->psy_fg->dev);
	if (ret) {
		dev_err(sm5714->dev,
			"%s : Failed to create_attrs\n", __func__);
	}

	sec_chg_set_dev_init(SC_DEV_FG);

	pr_info("%s: SM5714 Fuelgauge Driver[%s] Loaded\n", __func__, SM5714_FUELGAUGE_VERSION);
	return 0;

err_supply_unreg:
	sm5714_fuelgauge_free_irqs(fuelgauge);
	power_supply_unregister(fuelgauge->psy_fg);
err_data_free:
#if defined(CONFIG_OF)
	kfree(fuelgauge->battery_data);
#endif
err_pdata_free:
	kfree(fuelgauge_data);
	mutex_destroy(&fuelgauge->fg_lock);
err_free:
	kfree(fuelgauge);

	return ret;
}

static int sm5714_fuelgauge_remove(struct platform_device *pdev)
{
	struct sm5714_fuelgauge_data *fuelgauge =
		platform_get_drvdata(pdev);

	sm5714_fuelgauge_free_irqs(fuelgauge);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wakeup_source_unregister(fuelgauge->fuel_alert_ws);

	mutex_destroy(&fuelgauge->fg_lock);

	kfree(fuelgauge);

	return 0;
}

static int sm5714_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int sm5714_fuelgauge_resume(struct device *dev)
{
	struct sm5714_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->sleep_initial_update_of_soc = true;

	return 0;
}

static void sm5714_fuelgauge_shutdown(struct platform_device *pdev)
{
	struct sm5714_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	atomic_inc(&fuelgauge->shutdown_cnt);

	sm5714_fuelgauge_free_irqs(fuelgauge);

	if (fuelgauge->using_hw_vempty)
		sm5714_fg_set_vempty(fuelgauge, false);

	pr_info("%s: --\n", __func__);
}

static SIMPLE_DEV_PM_OPS(sm5714_fuelgauge_pm_ops, sm5714_fuelgauge_suspend,
			sm5714_fuelgauge_resume);

static struct platform_driver sm5714_fuelgauge_driver = {
	.driver = {
			.name = "sm5714-fuelgauge",
			.owner = THIS_MODULE,
#ifdef CONFIG_PM
			.pm = &sm5714_fuelgauge_pm_ops,
#endif
	},
	.probe  = sm5714_fuelgauge_probe,
	.remove = sm5714_fuelgauge_remove,
	.shutdown = sm5714_fuelgauge_shutdown,
};

static int __init sm5714_fuelgauge_init(void)
{
	pr_info("%s:\n", __func__);
	return platform_driver_register(&sm5714_fuelgauge_driver);
}

static void __exit sm5714_fuelgauge_exit(void)
{
	platform_driver_unregister(&sm5714_fuelgauge_driver);
}
module_init(sm5714_fuelgauge_init);
module_exit(sm5714_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung SM5714 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
