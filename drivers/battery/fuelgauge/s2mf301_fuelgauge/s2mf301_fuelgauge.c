/*
 * s2mf301_fuelgauge.c - S2MF301 Fuel Gauge Driver
 *
 * Copyright (C) 2022 Samsung Electronics, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include "s2mf301_fuelgauge.h"
#include "../../common/sec_charging_common.h"
#include <linux/of_gpio.h>

#define SINGLE_BYTE	1
#define TABLE_SIZE	22
#define PROPERTY_NAME_SIZE 128


static enum power_supply_property s2mf301_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if defined(CONFIG_FUELGAUGE_S2MF301_USE_5MILLIOHM)
static void s2mf301_set_trim_5mohm(struct s2mf301_fuelgauge_data *fuelgauge);
#endif
static int s2mf301_get_vbat(struct s2mf301_fuelgauge_data *fuelgauge);
static int s2mf301_get_ocv(struct s2mf301_fuelgauge_data *fuelgauge);
static int s2mf301_get_current(struct s2mf301_fuelgauge_data *fuelgauge);
static int s2mf301_get_avgcurrent(struct s2mf301_fuelgauge_data *fuelgauge);
static int s2mf301_get_avgvbat(struct s2mf301_fuelgauge_data *fuelgauge);

static void fg_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void fg_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static int fg_set_wake_lock(struct s2mf301_fuelgauge_data *fuelgauge)
{
	struct wakeup_source *ws = NULL;

	ws = wakeup_source_register(NULL, "fuel_alerted");
	if (ws == NULL)
		goto err;

	fuelgauge->fuel_alert_ws = ws;

	return 0;
err:
	return -1;
}

static int s2mf301_read_reg_byte(struct i2c_client *client, int reg, void *data)
{
	int ret = 0;
	int cnt = 0;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		while (ret < 0 && cnt < 5) {
			ret = i2c_smbus_read_byte_data(client, reg);
			cnt++;
			dev_err(&client->dev,
					"%s: I2C read Incorrect! reg:0x%x, data:0x%x, cnt:%d\n",
					__func__, reg, *(u8 *)data, cnt);
		}
		if (cnt == 5)
			dev_err(&client->dev,
				"%s: I2C read Failed reg:0x%x, data:0x%x\n",
				__func__, reg, *(u8 *)data);
	}
	*(u8 *)data = (u8)ret;

	return ret;
}

static int s2mf301_write_and_verify_reg_byte(struct i2c_client *client, int reg, u8 data)
{
	int ret, i = 0;
	int i2c_corrupted_cnt = 0;
	u8 temp = 0;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_byte_data(client, reg, data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	/* Skip non-writable registers */
	if ((reg == 0xee) || (reg == 0xef) || (reg == 0xf2) || (reg == 0xf3) || (reg == 0x8E) ||
		(reg == 0x0C) || (reg == 0x1e) || (reg == 0x1f) || (reg == 0x27)) {
		return ret;
	}

	s2mf301_read_reg_byte(client, reg, &temp);
	while ((temp != data) && (i2c_corrupted_cnt < 5)) {
		dev_err(&client->dev,
			"%s: I2C write Incorrect! REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, reg, data, temp);
		ret = i2c_smbus_write_byte_data(client, reg, data);
		s2mf301_read_reg_byte(client, reg, &temp);
		dev_err(&client->dev,
			"%s: cnt (%d), REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, i2c_corrupted_cnt, reg, data, temp);
		i2c_corrupted_cnt++;
	}

	if (i2c_corrupted_cnt == 5)
		dev_err(&client->dev,
			"%s: I2C write failed REG: 0x%x Expected: 0x%x\n",
			__func__, reg, data);

	return ret;
}

static int s2mf301_fg_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if IS_ENABLED(SINGLE_BYTE)
	int ret = 0;

	s2mf301_write_and_verify_reg_byte(client, reg, buf[0]);
	s2mf301_write_and_verify_reg_byte(client, reg+1, buf[1]);
#else
	int ret, i = 0;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static int s2mf301_fg_read_reg(struct i2c_client *client, int reg, u8 *buf)
{

#if IS_ENABLED(SINGLE_BYTE)
	int ret = 0;
	u8 data1 = 0, data2 = 0;

	s2mf301_read_reg_byte(client, reg, &data1);
	s2mf301_read_reg_byte(client, reg+1, &data2);
	buf[0] = data1;
	buf[1] = data2;
#else
	int ret = 0, i = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static void s2mf301_fg_test_read(struct i2c_client *client)
{
	static int reg_list[] = {
		0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B, 0x1E, 0x1F,
		0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x32, 0x33, 0x34, 0x35,
		0x40, 0x41, 0x43, 0x44, 0x45, 0x48, 0x49, 0x4A, 0x4B, 0x50,
		0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x67, 0x70,
		0x71, 0x72, 0x73, 0x7B, 0x80, 0x81, 0x88, 0x89, 0x8E, 0x8F,
		0x90, 0x91
	};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mf301_read_reg_byte(client, reg_list[i], &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
	}

	/* print buffer */
	pr_info("[FG]%s: %s\n", __func__, str);
}

int s2mf301_fg_check_current_level(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int ret_val = 500;
	int temp = 0;

	if (fuelgauge->cable_type == SEC_BATTERY_CABLE_USB)
		return ret_val;

	/* topoff current * 1.6 except USB */
	temp = fuelgauge->topoff_current * 16;
	ret_val = temp / 10;

	return ret_val;
}

static void s2mf301_reset_fg(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int i;
	u8 temp = 0;

	mutex_lock(&fuelgauge->fg_lock);

	/* step 0: [Surge test] initialize register of FG */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x0E,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x0F,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x10,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x11,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3]);

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x86,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].designcap[0]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x87,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].designcap[1]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RBATCAP_OCV_NEW_IN + 1,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap_ocv_new[1]);
	temp = (fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap_ocv_new[0]) | 0x01;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RBATCAP_OCV_NEW_IN, temp);

	for (i = 0x92; i <= 0xe9; i++)
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, i,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table3[i - 0x92]);
	for (i = 0xea; i <= 0xff; i++)
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, i,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table4[i - 0xea]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x5C,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].rRS_CC0);

	/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
	temp |= 0x40;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);

	/* Set EDV voltage : 2.8V */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x14, 0x67);

	/* Set rZADJ, rZADJ_CHG, rZADJ_CHG2 */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x12, 0x00);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x13, 0x00);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x15, 0x00);

	s2mf301_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
	temp &= 0x8F;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);

	/* Set Power off voltage (under 3.35V) */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x6F, &temp);
	temp &= ~0x07;
	temp |= 0x07;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x6F, temp);

	/* Set temperature load compensation coefficient */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x71, 0x41);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x73, 0x43);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x70, 0x01);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x72, 0x0);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x6E, 0x10);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x7B, 0x0A);

	/* Dumpdone. Re-calculate SOC */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
	msleep(300);

	s2mf301_read_reg_byte(fuelgauge->i2c, 0x03, &temp);
	temp |= 0x40;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x03, temp);

	/* If it was voltage mode, recover it */
	if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
		s2mf301_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
		temp |= 0x70;
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	pr_info("%s: Reset FG completed\n", __func__);
}

static int s2mf301_fix_rawsoc_reset_fg(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int ret = 0, ui_soc = 0, f_soc = 0;
	u8 data;
	struct power_supply *psy;
	union power_supply_propval value;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	dev_info(&fuelgauge->i2c->dev, "%s: UI SOC = %d\n", __func__, value.intval);

	ui_soc = value.intval;

	f_soc = (ui_soc << 8) / 100;

	if (f_soc > 0xFF)
		f_soc = 0xFF;

	f_soc |= 0x1;

	data = (u8)f_soc;

	/* Set rawsoc fix & enable */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, data);

	s2mf301_reset_fg(fuelgauge);

	/* Disable rawsoc fix */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x00);

	pr_info("%s: Finish\n", __func__);

	return ret;
}

static void s2mf301_fg_reset_capacity_by_jig_connection(struct s2mf301_fuelgauge_data *fuelgauge)
{
	/* TODO : model data version check */
	u8 data = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_FG_ID, &data);
	data &= 0xF0;
	data |= 0x0F; //set model data version 0xF for next boot up initializing fuelgague
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_FG_ID, data);

	pr_info("%s: set Model data version (0x%x)\n", __func__, data & 0x0F);
}

static void s2mf301_restart_gauging(struct s2mf301_fuelgauge_data *fuelgauge)
{
	pr_info("%s: Re-calculate SOC and voltage\n", __func__);

	mutex_lock(&fuelgauge->fg_lock);

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
	msleep(1000);

	mutex_unlock(&fuelgauge->fg_lock);
}

static void s2mf301_init_regs(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

	pr_info("%s: s2mf301 fuelgauge initialize\n", __func__);

	/* Save register values for surge check */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x53, &temp);
	fuelgauge->reg_OTP_53 = temp;
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x52, &temp);
	fuelgauge->reg_OTP_52 = temp;

	/* Disable VM3_flag_EN */
	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, &temp);
	temp &= ~0x04;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, temp);

	pr_info("%s: s2mf301 fuelgauge initialize end\n", __func__);
}

static int s2mf301_set_temperature(struct s2mf301_fuelgauge_data *fuelgauge,
			int temperature)
{
	/*
	 * s2mf301 include temperature sensor so,
	 * do not need to set temperature value.
	 */
	return temperature;
}

static int s2mf301_get_temperature(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int temperature = 0;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_AVG_TEMP, data) < 0)
		goto err;

	pr_info("%s temp data = 0x%x 0x%x\n", __func__, data[0], data[1]);

	mutex_unlock(&fuelgauge->fg_lock);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		temperature = -1 * ((~compliment & 0xFFFF) + 1);
	} else {
		temperature = compliment & 0x7FFF;
	}
	temperature = ((temperature * 100) >> 8)/10;

	pr_info("%s: temperature (%d)\n", __func__, temperature);

	return temperature;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -ERANGE;
}

#if IS_ENABLED(TEMP_COMPEN)
static bool s2mf301_get_vm_status(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_STATUS, &data);

	return (data & (1 << 6)) ? true : false;
}

static int s2mf301_temperature_compensation(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2], temp = 0;
	u16 compliment;
	int i, soc_r = 0;
	int ui_soc = 0, update_soc = 0;

	mutex_lock(&fuelgauge->fg_lock);

	if (fuelgauge->init_start) {
		s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC_R_SAVE, data);
		if (data[1] == 0) {
			ui_soc = (data[1] << 8) | (data[0]);

			if (fuelgauge->temperature < fuelgauge->low_temp_limit || ui_soc == 100) {
				pr_info("%s: temperature is low or UI soc 100!", __func__);
				pr_info("use saved UI SOC(%d) for mapping, data[1] = 0x%02x, data[0] = 0x%02x\n",
					ui_soc, data[1], data[0]);

				fuelgauge->ui_soc = ui_soc;
				fuelgauge->scaled_soc = ui_soc * 10;
				fuelgauge->capacity_old = ui_soc;

				s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, &temp);
				temp = temp | TEMP_COMPEN_INC_OK_EN;
				s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, temp);

				if (ui_soc == 100)
					update_soc = 0xFFFF;
				else
					update_soc = (ui_soc * (0x1 << 16)) / 100;

				pr_info("%s : uisoc(%d) => update_soc(0x%04x)\n", __func__, ui_soc, update_soc);

				/* WRITE_EN */
				data[0] = (update_soc & 0x00FF) | 0x0001;
				data[1] = (update_soc & 0xFF00) >> 8;

				for (i = 0; i < 5; i++) {
					s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOC_R_I2C + 1, data[1]);
					usleep_range(1000, 1100);
					s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOC_R_I2C + 1, &temp);
					pr_info("%s: 0x8F write check!(read:0x%02x, write:0x%02x)! soc update!\n", __func__, temp, data[1]);
				}

				if (temp == data[1])
					s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOC_R_I2C, data[0]);
				else 
					pr_info("%s: 0x8F write fail(read:0x%02x, write:0x%02x)! skip soc update!\n", __func__, temp, data[1]);

				msleep(300);

				s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, &temp);
				temp = temp & ~TEMP_COMPEN_INC_OK_EN;
				s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_VM, temp);

			}
		}
	}

	for (i = 0; i < 50; i++) {
		if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC_R, data) < 0)
			goto err;

		if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC_R, check_data) < 0)
			goto err;

		if ((data[0] == check_data[0]) && (data[1] == check_data[1])) {
			dev_dbg(&fuelgauge->i2c->dev,
					"%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
			break;
		}
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	if (compliment & (0x1 << 15)) {
		/* Negative */
		soc_r = ((~compliment) & 0xFFFF) + 1;
		soc_r = (soc_r * (-10000)) / (0x1 << 14);
	} else {
		soc_r = compliment & 0x7FFF;
		soc_r = ((soc_r * 10000) / (0x1 << 14));
	}

	if (fuelgauge->init_start) {
		s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC_R_SAVE, data);
		if ((fuelgauge->temperature >= fuelgauge->low_temp_limit) ||
				((fuelgauge->temperature < fuelgauge->low_temp_limit) && (data[1] != 0))) {
			fuelgauge->ui_soc = soc_r / 100;
			fuelgauge->scaled_soc = fuelgauge->soc_r / 10;
			fuelgauge->capacity_old = soc_r / 100;
		}
	}

	fuelgauge->init_start = 0;

	/* Save UI SOC for maintain SOC, after low temperature reset */
	data[0] = fuelgauge->ui_soc;
	data[1] = 0;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOC_R_SAVE, data[0]);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOC_R_SAVE + 1, data[1]);

	/* Print UI SOC & saved value for debugging */
	s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC_R_SAVE, data);
	ui_soc = (data[1] << 8) | (data[0]);
	pr_info("%s: saved UI SOC = %d, data[1] = 0x%02x, data[0] = 0x%02x\n",
			__func__, ui_soc, data[1], data[0]);

	return soc_r;
err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}
#endif

#if IS_ENABLED(BATCAP_LEARN)
static int s2mf301_get_batcap_ocv(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 batcap_ocv = 0;

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RBATCAP, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	batcap_ocv = (data[0] + (data[1] << 8)) >> 2;

	return batcap_ocv;
}

static int s2mf301_get_cycle(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, cycle;

	mutex_lock(&fuelgauge->fg_lock);

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_MONOUT_SEL, 0x27);

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);

	cycle = compliment;

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	return cycle;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

void s2mf301_batcap_learning(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int bat_w = 0;
	u8 data[2] = {0, }, temp = 0;
	int range = (BAT_L_CON[5] == 0) ? 900:800;
	int gap_cap = 0;

	gap_cap = (fuelgauge->capcc * 1000) / fuelgauge->batcap_ocv;

	if ((gap_cap > range) && (gap_cap < 1100)) {
		if (BAT_L_CON[6])
			bat_w = ((fuelgauge->batcap_ocv * 75) + (fuelgauge->capcc * 25)) / 100;
		else
			bat_w = ((fuelgauge->batcap_ocv * 90) + (fuelgauge->capcc * 10)) / 100;

		if (BAT_L_CON[7]) {
			fuelgauge->batcap_ocv_fin = bat_w;
			bat_w = bat_w << 2;
			data[1] = (u8)((bat_w >> 8) & 0x00ff);
			data[0] = (u8)(bat_w & 0x00ff);

			mutex_lock(&fuelgauge->fg_lock);

			s2mf301_fg_write_reg(fuelgauge->i2c, S2MF301_REG_RBATCAP, data);
			/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
			s2mf301_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
			temp |= 0x40;
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);

			mutex_unlock(&fuelgauge->fg_lock);
		}
	}

	pr_info("%s: gap_cap = %d, capcc = %d, batcap_ocv = %d, bat_w = %d\n",
		__func__, gap_cap, fuelgauge->capcc, fuelgauge->batcap_ocv, bat_w);
}

static int s2mf301_get_cap_cc(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int cap_cc = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_CAPCC + 1, &data1);
	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_CAPCC, &data0);
	cap_cc = (data1 << 8) | data0;
	if (cap_cc & (1 << 15)) {
		cap_cc = (~cap_cc) + 1;
		cap_cc = cap_cc / 2;
		cap_cc = cap_cc * (-1);
	} else
		cap_cc /= 2;

	return cap_cc;
}

static int s2mf301_get_soh(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int original = 0, ret = -1;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOH + 1, &data1);
	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_RSOH, &data0);
	original = (data1 << 8) | data0;

	ret = (original * 100) / 0x4000;

	pr_info("%s: original batcap = %d, soh = %d\n", __func__, original, ret);

	return ret;
}
#endif

#if IS_ENABLED(BATCAP_LEARN) || IS_ENABLED(TEMP_COMPEN)
static bool s2mf301_get_bat_charging(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_STATUS, &data);
	return (data & (1 << 5)) ? true : false;
}
#endif

#if IS_ENABLED(BATCAP_LEARN) && IS_ENABLED(TEMP_COMPEN)
static int s2mf301_get_fullcharge_cap(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int batcap_ocv = s2mf301_get_batcap_ocv(fuelgauge);

	ret = ((100 - fuelgauge->comp_socr) * batcap_ocv) / 100;

	return ret;
}

static int s2mf301_get_remaining_cap(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int fcc = s2mf301_get_fullcharge_cap(fuelgauge);

	ret = (fuelgauge->soc_r) * fcc / 10000;

	pr_info("%s: fcc = %d, remaining_cap = %d\n", __func__, fcc, ret);

	return ret;
}
#endif

#if defined(CONFIG_FUELGAUGE_S2MF301_USE_5MILLIOHM)
static void s2mf301_set_trim_5mohm(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 temp_58 = 0, temp_59 = 0, temp_5a = 0, temp_5b = 0, temp = 0;
	u32 cslope = 0, coffset = 0;
	u32 cal_val = 5000;

/* Read registers */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

/* set current slope
 *	Val(20bit ) = ( 0x5B[7:4] << 16) + ( 0x59 [7:0] << 8) + ( 0x58[7:0])
 */
	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
/* Set current offset
 *	Val(12bit ) = ( 0x5B[3:0] << 8) + ( 0x5A[7:0]
 */
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: before cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);

	cslope = (cslope ^ 0xFFFFF) + 1;
	cslope = cslope * 1000 * 2 / cal_val;
	cslope = (cslope ^ 0xFFFFF) + 1;

	if (coffset & (1 << 11)) {
		coffset = (coffset ^ 0xFFF) + 1;
		coffset = coffset * 1000 * 2 / cal_val;
		coffset = (coffset ^ 0xFFF) + 1;
	} else
		coffset = coffset * 1000 * 2 / cal_val;

/* Set registers : 0x58, 0x59, 0x5B, 0x5A */
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c,
		0x58, (cslope & 0xFF));
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c,
		0x59, (cslope & 0xFF00) >> 8);

	temp = ((cslope & 0xF0000) >> 12) | ((coffset & 0xF00) >> 8);
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c,
		0x5B, temp);

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c,
		0x5A, (coffset & 0xFF));

	/* Check written value */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: after cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);
}
#endif

static int s2mf301_runtime_reset_wa(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	u8 temp;
	u8 por_state = 0;
	u8 reg_1E = 0;
	u8 reg_OTP_52 = 0, reg_OTP_53 = 0;
	union power_supply_propval value;
	struct power_supply *psy;
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	enum power_supply_property psp;
	bool charging_enabled = false;
#endif

	s2mf301_read_reg_byte(fuelgauge->i2c, 0x1F, &por_state);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x1E, &reg_1E);
	dev_err(&fuelgauge->i2c->dev,
			"%s: OTP 52(%02x) 53(%02x), current 52(%02x) 53(%02x), 0x1F(%02x), 0x1E(%02x)\n",
			__func__, fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53, reg_OTP_52,
			reg_OTP_53, por_state, reg_1E);

	if ((fuelgauge->probe_done == true &&
			(fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53)) ||
			(((por_state != 0x00) || (reg_1E != 0x03)) && (fuelgauge->age_reset_status == 0))) {
		/* check charging enable */
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
		psy = power_supply_get_by_name("s2mf301-charger");
		if (!psy)
			return -EINVAL;

		psp = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED;
		ret = power_supply_get_property(psy, psp, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		charging_enabled = value.intval;

		value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;

		psy = power_supply_get_by_name("s2mf301-charger");
		if (!psy)
			return -EINVAL;

		psp = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED;
		ret = power_supply_set_property(psy, psp, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
#endif

		if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
			psy = power_supply_get_by_name("s2mf301-charger");
			if (!psy)
				return -EINVAL;

			psp = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_FUELGAUGE_RESET;
			ret = power_supply_set_property(psy, psp, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
#endif
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
			msleep(50);
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);
			usleep_range(10000, 11000);

			s2mf301_read_reg_byte(fuelgauge->i2c, 0x03, &temp);
			temp |= 0x30;
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x03, temp);

			s2mf301_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
			s2mf301_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);

			dev_err(&fuelgauge->i2c->dev,
					"1st reset after %s: OTP 52(%02x) 53(%02x) current 52(%02x) 53(%02x)\n",
					__func__, fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53,
					reg_OTP_52, reg_OTP_53);

			if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
				psy = power_supply_get_by_name("s2mf301-charger");
				if (!psy)
					return -EINVAL;

				psp = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_FUELGAUGE_RESET;
				ret = power_supply_set_property(psy, psp, &value);
				if (ret < 0)
					pr_err("%s: Fail to execute property\n", __func__);
#endif
				s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
				msleep(50);
				s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);
				usleep_range(10000, 11000);

				s2mf301_read_reg_byte(fuelgauge->i2c, 0x03, &temp);
				temp |= 0x30;
				s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x03, temp);

				dev_err(&fuelgauge->i2c->dev, "%s: 2nd reset\n", __func__);
			}
		}

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		if (fuelgauge->ui_soc == 0)
			s2mf301_reset_fg(fuelgauge);
		else
			s2mf301_fix_rawsoc_reset_fg(fuelgauge);

		por_state = 0x00;
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);

#if defined(CONFIG_FUELGAUGE_S2MF301_USE_5MILLIOHM)
		s2mf301_set_trim_5mohm(fuelgauge);
#endif

#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
		/* Recover charger status after f.g reset */
		if (charging_enabled) {
			value.intval = SEC_BAT_CHG_MODE_CHARGING;

			psy = power_supply_get_by_name("s2mf301-charger");
			if (!psy)
				return -EINVAL;

			psp = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED;
			ret = power_supply_set_property(psy, psp, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif
	}
	return ret;
}

static int s2mf301_get_compen_soc(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int temp_i2c = 0;

	if (fuelgauge->pdata->use_external_temp && fuelgauge->probe_done) {
		int ret = 0;
		struct power_supply *psy;
		union power_supply_propval value;

		/* If you want to use temperature sensed by other IC,
		 * change the battery driver so that F.G driver can
		 * get the value.
		 */
		psy = power_supply_get_by_name("battery");
		if (!psy)
			return -EINVAL;
		/* Get temperature from battery driver */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		fuelgauge->temperature = value.intval;
		temp_i2c = fuelgauge->temperature / 10;
		pr_info("%s: temperature from battery: %d\n", __func__, fuelgauge->temperature);

		/* W/A use system temperature */
		if (fuelgauge->temperature >= 0)
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_TEMP_I2C, temp_i2c);
		else
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_TEMP_I2C, ~(temp_i2c & 0xFF) + 1);
	} else
		fuelgauge->temperature = s2mf301_get_temperature(fuelgauge);

#if IS_ENABLED(BATCAP_LEARN) || IS_ENABLED(TEMP_COMPEN)
	fuelgauge->bat_charging = s2mf301_get_bat_charging(fuelgauge);
#endif

#if IS_ENABLED(TEMP_COMPEN)
	fuelgauge->vm_status = s2mf301_get_vm_status(fuelgauge);
	fuelgauge->soc_r = s2mf301_temperature_compensation(fuelgauge);

	dev_info(&fuelgauge->i2c->dev,
			"%s: current_soc (%d), compen_soc (%d), previous_soc (%d), FG_mode(%s)\n", __func__,
			fuelgauge->rsoc, fuelgauge->soc_r, fuelgauge->info.soc, mode_to_str[fuelgauge->mode]);

	fuelgauge->info.soc = fuelgauge->soc_r;
#else
	dev_info(&fuelgauge->i2c->dev,
			"%s: current_soc (%d), previous_soc (%d), FG_mode(%s)\n", __func__,
			fuelgauge->rsoc, fuelgauge->info.soc, mode_to_str[fuelgauge->mode]);

	fuelgauge->info.soc = fuelgauge->rsoc;
#endif
	return fuelgauge->info.soc;
}

static int s2mf301_fg_set_mode(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 fg_mode_reg = 0;
	u8 temp = 0;
	int ret = 0, float_voltage = 0;
	struct power_supply *psy;
	union power_supply_propval value;

#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	psy = power_supply_get_by_name("s2mf301-charger");
	if (!psy)
		float_voltage = fuelgauge->pdata->float_voltage;
	else {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		float_voltage = value.intval;
	}
#else
	float_voltage = 4350;
#endif

	float_voltage = (float_voltage * 996) / 1000;

	s2mf301_read_reg_byte(fuelgauge->i2c, 0x4A, &fg_mode_reg);

	dev_info(&fuelgauge->i2c->dev,
		"%s: UI SOC=%d, scaled_soc=%d, is_charging=%d, avg_vbat=%d, float_voltage=%d, avg_current=%d, 0x4A=0x%02x\n",
		__func__, fuelgauge->ui_soc, fuelgauge->scaled_soc, fuelgauge->is_charging, fuelgauge->avg_vbat,
		float_voltage, fuelgauge->avg_curr, fg_mode_reg);

	if ((fuelgauge->is_charging == true) &&
		((fuelgauge->scaled_soc >= SCALED_SOC) || ((fuelgauge->avg_vbat > float_voltage)
		&& (fuelgauge->avg_curr < s2mf301_fg_check_current_level(fuelgauge) && fuelgauge->avg_curr > 50)))) {
		if (fuelgauge->mode == CURRENT_MODE) { /* switch to VOLTAGE_MODE */
			fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
			s2mf301_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
			temp |= 0x70;
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in high soc voltage mode\n", __func__);
		}
	} else if ((fuelgauge->avg_curr < -50) || (fuelgauge->avg_curr >= s2mf301_fg_check_current_level(fuelgauge))) {
		if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
			fuelgauge->mode = CURRENT_MODE;

			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
			s2mf301_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
			temp &= 0x8F;
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in current mode\n", __func__);
		}
	}
	return fuelgauge->mode;
}

#if IS_ENABLED(BATCAP_LEARN)
static void s2mf301_determine_batcap_learning(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int BATCAP_L_VBAT;

	fuelgauge->capcc = s2mf301_get_cap_cc(fuelgauge);
	fuelgauge->batcap_ocv = s2mf301_get_batcap_ocv(fuelgauge); // CC mode capacity
	fuelgauge->cycle = s2mf301_get_cycle(fuelgauge);
	BATCAP_L_VBAT = (BAT_L_CON[1] == 0) ? 4200:4100;

	if (fuelgauge->temperature >= 200) {
		if (fuelgauge->learn_start == false) {
			if ((fuelgauge->rsoc < 1000) && (fuelgauge->cycle >= BAT_L_CON[0]))
				fuelgauge->learn_start = true;
		} else {
			if ((fuelgauge->cond1_ok == false) && (fuelgauge->bat_charging == false))
				goto batcap_learn_init;

			if (fuelgauge->cond1_ok == false) {
				if (fuelgauge->c1_count >= BAT_L_CON[2]) {
					fuelgauge->cond1_ok = true;
					fuelgauge->c1_count = 0;
				} else {
					if ((fuelgauge->vbat >= BATCAP_L_VBAT) &&
						(fuelgauge->avg_curr < BAT_L_CON[4]) && (fuelgauge->rsoc >= 9700)) {
						fuelgauge->c1_count++;
					} else
						fuelgauge->c1_count = 0;
				}
			} else {
				if (fuelgauge->c2_count >= BAT_L_CON[3]) {
					s2mf301_batcap_learning(fuelgauge);
					goto batcap_learn_init;
				} else {
					if ((fuelgauge->vbat >= (BATCAP_L_VBAT - 100)) && (fuelgauge->avg_curr > -30) &&
						(fuelgauge->avg_curr < 30) && (fuelgauge->rsoc >= 9800)) {
						fuelgauge->c2_count++;
					} else if (fuelgauge->avg_curr <= -30) {
						fuelgauge->c2_count = 0;
						goto batcap_learn_init;
					} else
						fuelgauge->c2_count = 0;
				}
			}
		}
	} else {
batcap_learn_init:
		fuelgauge->learn_start = false;
		fuelgauge->cond1_ok = false;
		fuelgauge->c1_count = 0;
		fuelgauge->c2_count = 0;
	}
}
#endif

static void s2mf301_low_voltage_wa(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

	if (fuelgauge->temperature > fuelgauge->low_temp_limit) {
		if ((fuelgauge->avg_vbat < fuelgauge->low_voltage_limit) &&
				(fuelgauge->avg_curr < -50) && (fuelgauge->rsoc > 300)) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA. Make rawsoc 0\n", __func__);

			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x07);

			/* Dumpdone. Re-calculate SOC */
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
			msleep(300);

			s2mf301_read_reg_byte(fuelgauge->i2c, 0x29, &temp);
			temp &= 0xFE;
			s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, temp);
		}
	} else {
		if ((fuelgauge->avg_vbat < fuelgauge->low_voltage_limit_lowtemp) &&
				(fuelgauge->avg_curr < -50) && (fuelgauge->info.soc > 100)) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA. Make UI SOC 0\n", __func__);
			/* Make report SOC 0% */
			fuelgauge->info.soc = 0;
#if IS_ENABLED(TEMP_COMPEN)
			fuelgauge->soc_r = 0;
#endif
		}
	}
}

static int s2mf301_get_rawsoc(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int ret = 0, info_soc = 0, mode = 0;
	struct power_supply *psy;
	union power_supply_propval value;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -EINVAL;
	/* Get UI SOC from battery driver */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0) {
		pr_err("%s: Fail to execute property.\n", __func__);
		value.intval = 0;
	}
	fuelgauge->ui_soc = value.intval;

	if (s2mf301_runtime_reset_wa(fuelgauge) < 0) {
		pr_err("%s: Fail to execute runtime reset W/A.\n", __func__);
		goto get_rawsoc_err;
	}

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RSOC, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		fuelgauge->rsoc = ((~compliment) & 0xFFFF) + 1;
		fuelgauge->rsoc = (fuelgauge->rsoc * (-10000)) / (0x1 << 14);
	} else {
		fuelgauge->rsoc = compliment & 0x7FFF;
		fuelgauge->rsoc = ((fuelgauge->rsoc * 10000) / (0x1 << 14));
	}

	fuelgauge->avg_curr = s2mf301_get_avgcurrent(fuelgauge);
	fuelgauge->avg_vbat = s2mf301_get_avgvbat(fuelgauge);
	fuelgauge->vbat = s2mf301_get_vbat(fuelgauge);
	fuelgauge->curr = s2mf301_get_current(fuelgauge);

	info_soc = s2mf301_get_compen_soc(fuelgauge);
	if (info_soc < 0) {
		pr_err("%s: Fail to get compen_soc.\n", __func__);
		goto get_rawsoc_err;
	}

	mode = s2mf301_fg_set_mode(fuelgauge);
	if (mode < 0) {
		pr_err("%s: Fail to set fg mode.\n", __func__);
		goto get_rawsoc_err;
	}

	pr_info("%s: info.soc = %d, mode = %d\n", __func__, info_soc, mode);

#if IS_ENABLED(BATCAP_LEARN)
	s2mf301_determine_batcap_learning(fuelgauge);
#endif

	pr_info("%s: Chg_stat = %d, SOC_M = %d, avbVBAT = %d, avgCURR = %d, avgTEMP = %d\n", __func__,
		fuelgauge->bat_charging, fuelgauge->rsoc,
		fuelgauge->avg_vbat, fuelgauge->avg_curr, fuelgauge->temperature);
#if IS_ENABLED(TEMP_COMPEN)
	pr_info("%s: VM = %d, SOCr = %d, SOC_R = %d ", __func__,
		fuelgauge->vm_status, fuelgauge->comp_socr, fuelgauge->soc_r);
#endif
#if IS_ENABLED(BATCAP_LEARN)
	fuelgauge->soh = s2mf301_get_soh(fuelgauge);
	fuelgauge->capcc = s2mf301_get_cap_cc(fuelgauge);
	fuelgauge->fcc = s2mf301_get_fullcharge_cap(fuelgauge);
	fuelgauge->rmc = s2mf301_get_remaining_cap(fuelgauge);

	pr_info("%s: Learning_start = %d, C1_count = %d/%d, C2_count = %d/%d, BATCAP_OCV_new = %d ", __func__,
		fuelgauge->learn_start, fuelgauge->c1_count, BAT_L_CON[2],
		fuelgauge->c2_count, BAT_L_CON[3], fuelgauge->batcap_ocv_fin);
	pr_info("%s: SOH = %d, CAP_CC = %d, FCC = %d, RM = %d\n", __func__,
		fuelgauge->soh, fuelgauge->capcc, fuelgauge->fcc, fuelgauge->rmc);
#endif

	s2mf301_low_voltage_wa(fuelgauge);

#if IS_ENABLED(TEMP_COMPEN)
	/* Maintain UI SOC if battery is relaxing */
	if (((fuelgauge->temperature < fuelgauge->low_temp_limit) &&
		(fuelgauge->soc_r == 0) && (fuelgauge->rsoc > 500)) &&
		(((fuelgauge->avg_curr > -60) && (fuelgauge->avg_curr < 50)) ||
		((fuelgauge->curr > -100) && (fuelgauge->curr < 50)))) {
		fuelgauge->soc_r = fuelgauge->ui_soc * 100;
		fuelgauge->info.soc = fuelgauge->soc_r;
		fuelgauge->init_start = 1;

		dev_info(&fuelgauge->i2c->dev,
			"%s: Maintain UI SOC if battery is relaxing SOC_R = %d, info.soc = %d\n",
			__func__, fuelgauge->soc_r, fuelgauge->info.soc);
	}
#endif

	ret = min(fuelgauge->info.soc, 10000);
	fuelgauge->bd_raw_soc = ret;
	return ret;

get_rawsoc_err:
	pr_info("%s: Chg_stat = %d, SOC_M = %d, avbVBAT = %d, avgCURR = %d, avgTEMP = %d\n", __func__,
		fuelgauge->bat_charging, fuelgauge->rsoc,
		fuelgauge->avg_vbat, fuelgauge->avg_curr, fuelgauge->temperature);
#if IS_ENABLED(TEMP_COMPEN)
	pr_info("%s: VM = %d, SOCr = %d, SOC_R = %d ", __func__,
		fuelgauge->vm_status, fuelgauge->comp_socr, fuelgauge->soc_r);
#endif
#if IS_ENABLED(BATCAP_LEARN)
	fuelgauge->soh = s2mf301_get_soh(fuelgauge);
	fuelgauge->capcc = s2mf301_get_cap_cc(fuelgauge);
	fuelgauge->fcc = s2mf301_get_fullcharge_cap(fuelgauge);
	fuelgauge->rmc = s2mf301_get_remaining_cap(fuelgauge);

	pr_info("%s: Learning_start = %d, C1_count = %d/%d, C2_count = %d/%d, BATCAP_OCV_new = %d ", __func__,
		fuelgauge->learn_start, fuelgauge->c1_count, BAT_L_CON[2],
		fuelgauge->c2_count, BAT_L_CON[3], fuelgauge->batcap_ocv_fin);
	pr_info("%s: SOH = %d, CAP_CC = %d, FCC = %d, RM = %d\n", __func__,
		fuelgauge->soh, fuelgauge->capcc, fuelgauge->fcc, fuelgauge->rmc);
#endif
	return -EINVAL;
err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mf301_get_current(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RCUR_CC, data) < 0)
		return -EINVAL;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 11;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 11;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: current (%d)mA\n", __func__, curr);

	return curr;
}

static int s2mf301_get_ocv(struct s2mf301_fuelgauge_data *fuelgauge)
{
	/* 22 values of mapping table for EVT1*/
	int *soc_arr;
	int *ocv_arr;

	int soc = fuelgauge->info.soc;
	int ocv = 0;

	int high_index = TABLE_SIZE - 1;
	int low_index = 0;
	int mid_index = 0;

	soc_arr = fuelgauge->age_data_info[fuelgauge->fg_age_step].soc_arr_val;
	ocv_arr = fuelgauge->age_data_info[fuelgauge->fg_age_step].ocv_arr_val;

	dev_err(&fuelgauge->i2c->dev,
		"%s: soc (%d) soc_arr[TABLE_SIZE-1] (%d) ocv_arr[TABLE_SIZE-1) (%d)\n",
		__func__, soc, soc_arr[TABLE_SIZE-1], ocv_arr[TABLE_SIZE-1]);
	if (soc <= soc_arr[TABLE_SIZE - 1]) {
		ocv = ocv_arr[TABLE_SIZE - 1];
		goto ocv_soc_mapping;
	} else if (soc >= soc_arr[0]) {
		ocv = ocv_arr[0];
		goto ocv_soc_mapping;
	}
	while (low_index <= high_index) {
		mid_index = (low_index + high_index) >> 1;
		if (soc_arr[mid_index] > soc)
			low_index = mid_index + 1;
		else if (soc_arr[mid_index] < soc)
			high_index = mid_index - 1;
		else {
			ocv = ocv_arr[mid_index];
			goto ocv_soc_mapping;
		}
	}

	if ((high_index >= 0 && high_index < TABLE_SIZE) && (low_index >= 0 && low_index < TABLE_SIZE)) {
		ocv = ocv_arr[high_index];
		ocv += ((ocv_arr[low_index] - ocv_arr[high_index]) *
			(soc - soc_arr[high_index])) /
			(soc_arr[low_index] - soc_arr[high_index]);
	}

ocv_soc_mapping:
	dev_info(&fuelgauge->i2c->dev, "%s: soc (%d), ocv (%d)\n", __func__, soc, ocv);
	fuelgauge->bd_vfocv = ocv;
	return ocv;
}

static int s2mf301_get_avgcurrent(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_AVG_CURR, data) < 0)
		goto err;

	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: data(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 11;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 11;
	}

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avg current (%d)mA\n", __func__, curr);

	return curr;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mf301_get_vbat(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_RVBAT, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_info(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mf301_get_avgvbat(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2], data1[2];
	u16 compliment, avg_vbat;
	int cnt = 0;

	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x40, 0x08);
	mutex_lock(&fuelgauge->fg_lock);

	while (cnt < 5) {
		cnt++;
		if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_AVG_VBAT, data)< 0)
			goto err;
		usleep_range(1000, 1100);
		if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_AVG_VBAT, data1) < 0)
			goto err;

		if ((data[0] == data1[0]) && (data[1] == data1[1]))
			break;
	}

	compliment = (data[1] << 8) | (data[0]);
	avg_vbat = (compliment * 1000) >> 12;

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avgvbat (%d : 0x%02x, 0x%02x)\n", __func__, avg_vbat, data[0], data[1]);

	return avg_vbat;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

bool s2mf301_fuelgauge_fuelalert_init(struct s2mf301_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	mutex_lock(&fuelgauge->fg_lock);

	fuelgauge->is_fuel_alerted = false;

	if (s2mf301_fg_read_reg(fuelgauge->i2c, S2MF301_REG_FG_INT, data) < 0)
		return false;

	/*Enable VBAT, SOC */
	data[1] &= ~0x03;

	/*Disable IDLE_ST, INIT_ST */
	data[1] |= 0x0c;

	s2mf301_fg_write_reg(fuelgauge->i2c, S2MF301_REG_FG_INT, data);

	mutex_unlock(&fuelgauge->fg_lock);
	dev_dbg(&fuelgauge->i2c->dev, "%s: irq_reg(%02x%02x)\n", __func__, data[1], data[0]);

	return true;
}

static int s2mf301_fg_aging_check(
		struct s2mf301_fuelgauge_data *fuelgauge, int step)
{
	u8 batcap0 = 0, batcap1 = 0, batcap2 = 0, batcap3 = 0;
	u8 por_state = 0;
	union power_supply_propval value = {0, };
	int charging_enabled = false;

	fuelgauge->fg_age_step = step;
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x0E, &batcap0);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x0F, &batcap1);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x10, &batcap2);
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x11, &batcap3);

	pr_info("%s: [Long life] orig. batcap : %02x, %02x, %02x, %02x , fg_age_step data : %02x, %02x, %02x, %02x\n",
		__func__, batcap0, batcap1, batcap2, batcap3,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3]);

	if ((batcap0 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]) ||
		(batcap1 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]) ||
		(batcap2 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2]) ||
		(batcap3 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3])) {

		pr_info("%s: [Long life] reset gauge for age forecast , step[%d]\n", __func__, fuelgauge->fg_age_step);

		fuelgauge->age_reset_status = 1;
		por_state |= 0x10;
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);
		/* check charging enable */
		psy_do_property("s2mf301-charger", get, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
		charging_enabled = value.intval;

		if (charging_enabled == true) {
			pr_info("%s: [Long life] disable charger for reset gauge age forecast\n",
				__func__);
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			psy_do_property("s2mf301-charger", set, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
		}

		s2mf301_reset_fg(fuelgauge);
#if IS_ENABLED(TEMP_COMPEN)
		fuelgauge->init_start = 1;
#endif

		if (charging_enabled == true) {
			psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
			charging_enabled = value.intval;

			if (charging_enabled == 1) { /* POWER_SUPPLY_STATUS_CHARGING 1 */
				pr_info("%s: [Long life] enable charger for reset gauge age forecast\n",
					__func__);
				value.intval = SEC_BAT_CHG_MODE_CHARGING;
				psy_do_property("s2mf301-charger",
					set, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
			}
		}

		por_state &= ~0x10;
		s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);
		fuelgauge->age_reset_status = 0;

		return 1;
	}
	return 0;
}

#if defined(CONFIG_UI_SOC_PROLONGING)
static void s2mf301_fg_adjust_capacity_max(
	struct s2mf301_fuelgauge_data *fuelgauge, int curr_raw_soc)
{
	int diff = 0;

	if (fuelgauge->is_charging && fuelgauge->capacity_max_conv) {
		diff = curr_raw_soc - fuelgauge->prev_raw_soc;

		if ((diff >= 1) && (fuelgauge->capacity_max < fuelgauge->g_capacity_max)) {
			fuelgauge->capacity_max++;
		} else if ((fuelgauge->capacity_max >= fuelgauge->g_capacity_max) || (curr_raw_soc == 100)) {
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

/* capacity is 0.1% unit */
static void s2mf301_fg_get_scaled_capacity(
		struct s2mf301_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	int rawsoc = val->intval;

	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	fuelgauge->scaled_soc = val->intval;

	dev_info(&fuelgauge->i2c->dev,
			"%s: capacity_max(%d) scaled capacity(%d.%d), raw_soc(%d.%d)\n",
			__func__, fuelgauge->capacity_max,
			val->intval/10, val->intval%10, rawsoc/10, rawsoc%10);
}

/* capacity is integer */
static void s2mf301_fg_get_atomic_capacity(
		struct s2mf301_fuelgauge_data *fuelgauge, union power_supply_propval *val)
{
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
			dev_err(&fuelgauge->i2c->dev,
					"%s: capacity (old %d : new %d)\n",
					__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int s2mf301_fg_check_capacity_max(
		struct s2mf301_fuelgauge_data *fuelgauge, int capacity_max)
{
	int cap_max, cap_min;

	cap_max = fuelgauge->pdata->capacity_max;
	cap_min =
		(fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin);

	return (capacity_max < cap_min) ? cap_min :
		((capacity_max >= cap_max) ? cap_max : capacity_max);
}

#if defined(CONFIG_UI_SOC_PROLONGING)
static void s2mf301_fg_calculate_dynamic_scale(
	struct s2mf301_fuelgauge_data *fuelgauge, int capacity, bool scale_by_full)
{
	int raw_soc_val;
	int min_cap = fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin;
	int scaling_factor = 1;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc_val = s2mf301_get_rawsoc(fuelgauge);
	if (raw_soc_val < 0) {
		pr_err("%s: failed to read raw soc\n", __func__);
		return;
	}

	raw_soc_val = (raw_soc_val) / 10;

	if (capacity < 100)
		fuelgauge->capacity_max_conv = false;  //Force full sequence , need to decrease capacity_max

	if ((raw_soc_val < min_cap) || (fuelgauge->capacity_max_conv)) {
		pr_info("%s: skip routine - raw_soc(%d), min_cap(%d), cap_max_conv(%d)\n",
				__func__, raw_soc_val, min_cap, fuelgauge->capacity_max_conv);
		return;
}

	if (capacity == 100)
		scaling_factor = 2;

	fuelgauge->capacity_max = (raw_soc_val * 100 / (capacity + scaling_factor));
	fuelgauge->capacity_old = capacity;

	fuelgauge->capacity_max =
		s2mf301_fg_check_capacity_max(fuelgauge,
			fuelgauge->capacity_max);

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);
	if ((capacity == 100) && !fuelgauge->capacity_max_conv && scale_by_full) {
		fuelgauge->capacity_max_conv = true;
		fuelgauge->g_capacity_max = 990;
		pr_info("%s: Goal capacity max %d\n", __func__, fuelgauge->g_capacity_max);
	}
}
#else
static void s2mf301_fg_calculate_dynamic_scale(
		struct s2mf301_fuelgauge_data *fuelgauge, int capacity)
{
	int raw_soc_val;
	int min_cap = fuelgauge->pdata->capacity_max - fuelgauge->pdata->capacity_max_margin;

	if ((capacity > 100) || ((capacity * 10) < min_cap)) {
		pr_err("%s: invalid capacity(%d)\n", __func__, capacity);
		return;
	}

	raw_soc_val = s2mf301_get_rawsoc(fuelgauge);
	if (raw_soc_val < 0) {
		pr_err("%s: failed to read raw soc\n", __func__);
		return;
	}

	raw_soc_val = raw_soc_val / 10;
	if (raw_soc_val < min_cap) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val);
		return;
	}

	fuelgauge->capacity_max =
		(raw_soc_val * 100 / (capacity + 1));
	fuelgauge->capacity_old = capacity;

	fuelgauge->capacity_max =
		s2mf301_fg_check_capacity_max(fuelgauge,
		fuelgauge->capacity_max);

	pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
		__func__, fuelgauge->capacity_max, capacity);
}
#endif

void s2mf301_fg_set_sys_voltage(struct s2mf301_fuelgauge_data *fuelgauge,
			int get_sys_vol)
{
	u8 temp = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_VBAT_I2C, &temp);
	temp &= ~0xF0;
	if (get_sys_vol)	/* factory mode : 0x25 [7:4] = b1111 (Get SYS voltage) */
		temp |= 0xF0;
	else	/* normal mode : 0x25 [7:4] = b0101 (Get Battery voltage by I2C Control) */
		temp |= 0x50;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, S2MF301_REG_VBAT_I2C, temp);

	pr_info("%s: SEC_BAT_INBAT_FGSRC_SWITCHING_OFF(%d) : 0x25 = %x\n",
			__func__, get_sys_vol, temp);
}
static int s2mf301_get_bat_id(int bat_id[], int bat_gpio_cnt)
{
	int battery_id = 0;
	int i = 0;

	for (i = (bat_gpio_cnt - 1); i >= 0; i--)
		battery_id += bat_id[i] << i;

	return battery_id;
}

static void s2mf301_reset_bat_id(struct s2mf301_fuelgauge_data *fuelgauge)
{
	int bat_id[BAT_GPIO_NO] = {0, };
	int i = 0;

	for (i = 0; i < fuelgauge->pdata->bat_gpio_cnt; i++)
		bat_id[i] = gpio_get_value(fuelgauge->pdata->bat_id_gpio[i]);

	fuelgauge->battery_id =
			s2mf301_get_bat_id(bat_id, fuelgauge->pdata->bat_gpio_cnt);
}

static void s2mf301_fg_bd_log(struct s2mf301_fuelgauge_data *fuelgauge)
{
	memset(fuelgauge->d_buf, 0x0, sizeof(fuelgauge->d_buf));

	snprintf(fuelgauge->d_buf + strlen(fuelgauge->d_buf), sizeof(fuelgauge->d_buf) - strlen(fuelgauge->d_buf),
		"%d,%d,%d",
		fuelgauge->bd_vfocv,
		fuelgauge->bd_raw_soc,
		fuelgauge->capacity_max);
}

static int s2mf301_fg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		return -ENODATA;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			val->intval = fuelgauge->pdata->capacity_full;
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = fuelgauge->pdata->capacity_full;
			break;
		default:
			val->intval = -1;
			break;
		}
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mf301_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = s2mf301_get_avgvbat(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = s2mf301_get_ocv(fuelgauge);
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mf301_get_current(fuelgauge) * 1000;
		else
			val->intval = s2mf301_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mf301_get_avgcurrent(fuelgauge) * 1000;
		else
			val->intval = s2mf301_get_avgcurrent(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mf301_get_rawsoc(fuelgauge);
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT) {
			val->intval = fuelgauge->raw_capacity % 10;
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
			val->intval = fuelgauge->raw_capacity;
		} else {
			val->intval = s2mf301_get_rawsoc(fuelgauge);
			if (val->intval < 0) {
				pr_err("%s: failed to get soc(%d), keep state(%d)\n",
						__func__, val->intval / 10, fuelgauge->capacity_old);
				val->intval = fuelgauge->capacity_old;
				break;
			}
			val->intval = val->intval / 10;

#if defined(CONFIG_UI_SOC_PROLONGING)
		if (fuelgauge->pdata->capacity_calculation_type &
			(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE | SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {

			s2mf301_fg_adjust_capacity_max(fuelgauge, val->intval);
			s2mf301_fg_get_scaled_capacity(fuelgauge, val);

			if (val->intval > 1020) {
				pr_info("%s: scaled capacity (%d)\n", __func__, val->intval);
				s2mf301_fg_calculate_dynamic_scale(fuelgauge, 100, false);
			}
		}
#else
			if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {
				s2mf301_fg_get_scaled_capacity(fuelgauge, val);

				if (val->intval > 1010) {
					pr_info("%s : scaled capacity (%d)\n", __func__, val->intval);
					s2mf301_fg_calculate_dynamic_scale(fuelgauge, 100);
				}
			}
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

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) && fuelgauge->is_fuel_alerted) {
				fg_wake_unlock(fuelgauge->fuel_alert_ws);
				s2mf301_fuelgauge_fuelalert_init(fuelgauge);
			}

#if defined(CONFIG_UI_SOC_PROLONGING)
			/* Check UI soc reached 100% from 99% , no need to adjust now */
			if ((val->intval == 100) && (fuelgauge->capacity_old < 100) &&
				(fuelgauge->capacity_max_conv == true))
				fuelgauge->capacity_max_conv = false;
#endif
			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc &&
					(fuelgauge->temperature > fuelgauge->low_temp_limit)) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->sleep_initial_update_of_soc) {
				/* updated old capacity in case of resume */
				if (fuelgauge->is_charging) {
					fuelgauge->capacity_old = val->intval;
					fuelgauge->sleep_initial_update_of_soc = false;
					break;
				} else if ((!fuelgauge->is_charging) &&
						(fuelgauge->capacity_old >= val->intval)) {
					fuelgauge->capacity_old = val->intval;
					fuelgauge->sleep_initial_update_of_soc = false;
					break;
				}
			}

			if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
					 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				s2mf301_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mf301_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
#if (BATCAP_LEARN)
		fuelgauge->soh = s2mf301_get_soh(fuelgauge);
		val->intval = fuelgauge->soh;
#else
		/* If battery capacity learning is not enabled,
		 * return SOH is 100%
		 */
		val->intval = 100;
#endif
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = fuelgauge->mode;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fuelgauge->pdata->capacity_full * fuelgauge->raw_capacity;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch ((int)ext_psp) {
		case POWER_SUPPLY_EXT_PROP_BATTERY_ID:
			if (!val->intval) {
				if (fuelgauge->pdata->bat_gpio_cnt > 0)
					s2mf301_reset_bat_id(fuelgauge);
				val->intval = fuelgauge->battery_id;
				pr_info("%s: bat_id_gpio = %d\n", __func__, val->intval);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_DUMP:
			s2mf301_fg_bd_log(fuelgauge);
			val->strval = fuelgauge->d_buf;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			s2mf301_fg_test_read(fuelgauge->i2c);
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

static int s2mf301_fg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			s2mf301_fg_aging_check(fuelgauge, fuelgauge->change_step);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
#if defined(CONFIG_UI_SOC_PROLONGING)
			s2mf301_fg_calculate_dynamic_scale(fuelgauge, val->intval, true);
#else
			s2mf301_fg_calculate_dynamic_scale(fuelgauge, val->intval);
#endif
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			s2mf301_restart_gauging(fuelgauge);
			fuelgauge->initial_update_of_soc = true;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		s2mf301_set_temperature(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		s2mf301_fg_reset_capacity_by_jig_connection(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		dev_info(&fuelgauge->i2c->dev,
			"%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max = s2mf301_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
#if defined(CONFIG_UI_SOC_PROLONGING)
		fuelgauge->g_capacity_max = 990;
		fuelgauge->capacity_max_conv = true;
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		fuelgauge->topoff_current = val->intval;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch ((int)ext_psp) {
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VBAT) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_VBAT)) {
				s2mf301_fg_set_sys_voltage(fuelgauge, 0);
				if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VBAT)
					s2mf301_restart_gauging(fuelgauge);
				else
					msleep(1000);
				s2mf301_fg_reset_capacity_by_jig_connection(fuelgauge);
				s2mf301_fg_test_read(fuelgauge->i2c);
			} else if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_VSYS)) {
				s2mf301_fg_set_sys_voltage(fuelgauge, 1);
				if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS)
					s2mf301_restart_gauging(fuelgauge);
				else
					msleep(1000);
				s2mf301_fg_reset_capacity_by_jig_connection(fuelgauge);
				s2mf301_fg_test_read(fuelgauge->i2c);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY:
			pr_info("%s:[DEBUG_FAC] fuelgauge\n", __func__);
			s2mf301_fg_set_sys_voltage(fuelgauge, 1);
			s2mf301_fg_reset_capacity_by_jig_connection(fuelgauge);
			break;
		case POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA:
			fuelgauge->change_step = val->intval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == SEC_BAT_CHG_MODE_CHARGING)
				fuelgauge->is_charging = true;
			else
				fuelgauge->is_charging = false;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			break;
#if defined(CONFIG_SEC_FACTORY)
		case POWER_SUPPLY_EXT_PROP_AFC_TEST_FG_MODE:
			pr_info("%s, POWER_SUPPLY_EXT_PROP_AFC_TEST_FG_MODE(%d)\n", __func__, val->intval);
			if (val->intval) {
				s2mf301_fg_reset_capacity_by_jig_connection(fuelgauge);
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_VBAT_TIME_SEL,
									0x00, FG_ON_MASK);
				msleep(260);
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_FS,
									FS_MANUAL_EN, FS_MANUAL_EN);
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_VBAT_TIME_SEL,
									FG_ON_MASK, FG_ON_MASK);
				msleep(200);
			} else {
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_VBAT_TIME_SEL,
									0x00, FG_ON_MASK);
				msleep(70);
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_FS,
									0x00, FS_MANUAL_EN);
				s2mf301_update_reg(fuelgauge->i2c, S2MF301_REG_VBAT_TIME_SEL,
									FG_ON_MASK, FG_ON_MASK);
			}
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mf301_fg_isr_work(struct work_struct *work)
{
	struct s2mf301_fuelgauge_data *fuelgauge = container_of(work, struct s2mf301_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_STATUS, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_alert_status(0x%x)\n", __func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s: Battery Level(SOC) is very Low!\n", __func__);

	if (fg_alert_status & 0x02) {
		int voltage = s2mf301_get_vbat(fuelgauge);

		pr_info("%s: Battery Votage is very Low! (%dmV)\n", __func__, voltage);
	}

	if (!fg_alert_status) {
		fuelgauge->is_fuel_alerted = false;
		pr_info("%s: SOC or Voltage is Good!\n", __func__);
		fg_wake_unlock(fuelgauge->fuel_alert_ws);
	}
}

static irqreturn_t s2mf301_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mf301_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_FG_INT, &fg_irq);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_irq(0x%x)\n", __func__, fg_irq);

	if (fuelgauge->is_fuel_alerted)
		return IRQ_HANDLED;

	fg_wake_lock(fuelgauge->fuel_alert_ws);
	fuelgauge->is_fuel_alerted = true;
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_OF)
static int s2mf301_fuelgauge_parse_dt(struct s2mf301_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mf301-fuelgauge");
	int ret;
	int len, i;
	int battery_id = -1;
	int bat_id[BAT_GPIO_NO] = {0, };

	/* reset, irq gpio info */
	if (np == NULL)
		pr_err("%s: np NULL\n", __func__);
	else {
		ret = of_property_read_u32(np, "fuelgauge,low_voltage_limit",
				&fuelgauge->low_voltage_limit);
		if (ret < 0) {
			pr_err("%s There is no low voltage limit. Use default(3450)\n",
					__func__);
			fuelgauge->low_voltage_limit = 3450;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_voltage_limit_lowtemp",
				&fuelgauge->low_voltage_limit_lowtemp);
		if (ret < 0) {
			pr_err("%s There is no low voltage limit low temp. Use default(3100)\n",
					__func__);
			fuelgauge->low_voltage_limit_lowtemp = 3100;
		}
		fuelgauge->pdata->bat_gpio_cnt = of_gpio_named_count(np, "fuelgauge,bat_id_gpio");
		/* not run if gpio gpio cnt is less than 1 */
		if (fuelgauge->pdata->bat_gpio_cnt > 0) {
			pr_info("%s: Has %d bat_id_gpios\n", __func__, fuelgauge->pdata->bat_gpio_cnt);
			if (fuelgauge->pdata->bat_gpio_cnt > BAT_GPIO_NO) {
				pr_err("%s: bat_id_gpio, catch out-of bounds array read\n",
						__func__);
				fuelgauge->pdata->bat_gpio_cnt = BAT_GPIO_NO;
			}
			for (i = 0; i < fuelgauge->pdata->bat_gpio_cnt; i++) {
				fuelgauge->pdata->bat_id_gpio[i] = of_get_named_gpio(np, "fuelgauge,bat_id_gpio", i);
				if (fuelgauge->pdata->bat_id_gpio[i] >= 0) {
					bat_id[i] = gpio_get_value(fuelgauge->pdata->bat_id_gpio[i]);
					pr_info("%s: bat_id[%d] = %d\n", __func__, i, bat_id[i]);
				} else {
					pr_err("%s: error reading bat_id_gpio = %d\n",
						__func__, fuelgauge->pdata->bat_id_gpio[i]);
					bat_id[i] = 0;
				}
			}
			fuelgauge->battery_id =
					s2mf301_get_bat_id(bat_id, fuelgauge->pdata->bat_gpio_cnt);

			pr_info("%s: battery_id (gpio) = %d\n", __func__, fuelgauge->battery_id);
		} else {
			fuelgauge->battery_id = 0;
		}

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc", &fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s: error reading pdata->fuel_alert_soc %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_full",
				&fuelgauge->pdata->capacity_full);
		if (ret < 0)
			pr_err("%s error reading pdata->capacity_full %d\n",
					__func__, ret);

		fuelgauge->pdata->use_external_temp = of_property_read_bool(np,
												"fuelgauge,use_external_temp");

		np = of_find_node_by_name(NULL, "cable-info");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_u32(np, "full_check_current_1st",
					&fuelgauge->topoff_current);
			if (ret < 0) {
				pr_err("%s fail to get topoff current %d\n", __func__, ret);
				fuelgauge->topoff_current = 500;
			}
			pr_info("%s: topoff_current = %d\n", __func__, fuelgauge->topoff_current);
		}

		np = of_find_node_by_name(NULL, "battery");
		if (!np)
			pr_err("%s: np NULL\n", __func__);
		else {
			ret = of_property_read_string(np, "battery,fuelgauge_name",
							(char const **)&fuelgauge->pdata->fuelgauge_name);
			if (ret < 0)
				pr_err("%s error reading battery,fuelgauge_name\n", __func__);

			ret = of_property_read_u32(np, "battery,chg_float_voltage",
					(unsigned int *)&fuelgauge->pdata->float_voltage);
			if (ret) {
				pr_info("%s: chg_float_voltage is Empty\n", __func__);
				fuelgauge->pdata->float_voltage = 4350;
			}
		}

		/* get battery node */
		np = of_find_node_by_name(NULL, "battery_params");
		if (!np) {
			pr_err("%s: battery_params node NULL\n", __func__);
			return -EINVAL;
		}
		else {
			char prop_name[PROPERTY_NAME_SIZE];
			battery_id = fuelgauge->battery_id;
			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_data");
			if (!of_find_property(np, prop_name, NULL)) {
				pr_err("%s: no battery_id(%d) data\n", __func__, battery_id);
				/* get battery_id */
				battery_id = 0;
				if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
					pr_err("%s: not battery,id property\n", __func__);
				pr_err("%s: use battery default id = %d\n", __func__, battery_id);
			}

			snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_data");
			pr_err("%s: prop_name = %s\n", __func__, prop_name);

			of_get_property(np, prop_name, &len);
			pr_info("%s: length len= (%d)\n", __func__, len);
			fuelgauge->fg_num_age_step = len / sizeof(fg_age_data_info_t);
			pr_info("%s: fg_num_age_step = (%d)\n", __func__,
				fuelgauge->fg_num_age_step);
			fuelgauge->age_data_info = kzalloc(len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, prop_name,
					(int *)fuelgauge->age_data_info, len/sizeof(int));

			pr_err("%s: [Long life] fuelgauge->fg_num_age_step %d\n",
					__func__, fuelgauge->fg_num_age_step);

			if ((sizeof(fg_age_data_info_t) * fuelgauge->fg_num_age_step) != len) {
				pr_err("%s: [Long life] variables and data in device tree doesn't match\n", __func__);
				BUG();
			}

			for (i = 0 ; i < fuelgauge->fg_num_age_step; i++) {
				pr_err("%s: [Long life] age_step = %d, table3[0] %d, table4[0] %d, batcap[0] %02x, designcap[0] %02x, batcap_ocv_new[0] %02x, soc_arr[0] %d, ocv_arr[0] %d, rRS_CC0 %02x\n",
						__func__, i,
						fuelgauge->age_data_info[i].battery_table3[0],
						fuelgauge->age_data_info[i].battery_table4[0],
						fuelgauge->age_data_info[i].batcap[0],
						fuelgauge->age_data_info[i].designcap[0],
						fuelgauge->age_data_info[i].batcap_ocv_new[0],
						fuelgauge->age_data_info[i].soc_arr_val[0],
						fuelgauge->age_data_info[i].ocv_arr_val[0],
						fuelgauge->age_data_info[i].rRS_CC0);
			}
			ret = of_property_read_u32(np, "battery,low_temp_limit", &fuelgauge->low_temp_limit);
			if (ret < 0) {
				pr_err("%s: There is no low temperature limit. Use default(100)\n", __func__);
				fuelgauge->low_temp_limit = 100;
			}
		}
	}
	pr_info("%s DT file parsed successfully\n", __func__);

	return 0;
}
#else
static int s2mf301_fuelgauge_parse_dt(struct s2mf301_fuelgauge_data *fuelgauge)
{
	return 0;
}
#endif /* CONFIG_OF */

static const struct power_supply_desc s2mf301_fuelgauge_power_supply_desc = {
	.name = "s2mf301-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2mf301_fuelgauge_props,
	.num_properties = ARRAY_SIZE(s2mf301_fuelgauge_props),
	.get_property = s2mf301_fg_get_property,
	.set_property = s2mf301_fg_set_property,
};

static int s2mf301_fuelgauge_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_platform_data *pdata = dev_get_platdata(s2mf301->dev);
	struct s2mf301_fuelgauge_data *fuelgauge;
	int raw_soc_val;
	struct power_supply_config fuelgauge_cfg = {};
	int ret = 0;
	u8 temp = 0;

	pr_info("%s: S2MF301 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->i2c = s2mf301->fg;
	fuelgauge->top = s2mf301->i2c;

	fuelgauge->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fuelgauge->pdata)), GFP_KERNEL);
	if (!fuelgauge->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mf301_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0) {
		pr_err("%s not found fuelgauge dt! ret[%d]\n", __func__, ret);
		goto err_parse_dt;
	}

	platform_set_drvdata(pdev, fuelgauge);

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "s2mf301-fuelgauge";

	/* I2C enable */
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x03, &temp);
	temp |= 0x30;
	s2mf301_write_and_verify_reg_byte(fuelgauge->i2c, 0x03, temp);

	fuelgauge->revision = 0;
	s2mf301_read_reg_byte(fuelgauge->i2c, 0x48, &temp);
	fuelgauge->revision = (temp & 0xF0) >> 4;

	s2mf301_read_reg(fuelgauge->top, S2MF301_REG_ES_NO_REV_NO, &temp);
	pr_info("%s: S2MF301 Fuelgauge revision: 0x%x, reg 0x48 = 0x%x\n", __func__, fuelgauge->revision, temp);

	fuelgauge->info.soc = 0;
#if IS_ENABLED(TEMP_COMPEN)
	fuelgauge->init_start = 1;
#endif

	raw_soc_val = s2mf301_get_rawsoc(fuelgauge);
	if (raw_soc_val >= 0) {
		raw_soc_val = raw_soc_val / 10;

#if defined(CONFIG_UI_SOC_PROLONGING)
		if (raw_soc_val > fuelgauge->capacity_max)
			s2mf301_fg_calculate_dynamic_scale(fuelgauge, 100, false);
#else
		if (raw_soc_val > fuelgauge->capacity_max)
			s2mf301_fg_calculate_dynamic_scale(fuelgauge, 100);
#endif
	}

	s2mf301_read_reg_byte(fuelgauge->i2c, 0x4A, &temp);
	pr_info("%s: 0x4A = 0x%02x, rawsoc = %d\n", __func__, temp, raw_soc_val);
	if (temp == 0x10)
		fuelgauge->mode = CURRENT_MODE;
	else if (temp == 0xFF)
		fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

#if IS_ENABLED(BATCAP_LEARN)
	fuelgauge->learn_start = false;
	fuelgauge->cond1_ok = false;
	fuelgauge->c1_count = 0;
	fuelgauge->c2_count = 0;
#endif

	s2mf301_init_regs(fuelgauge);

	fuelgauge_cfg.drv_data = fuelgauge;

	fuelgauge->psy_fg = power_supply_register(&pdev->dev, &s2mf301_fuelgauge_power_supply_desc, &fuelgauge_cfg);
	if (IS_ERR(fuelgauge->psy_fg)) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_data_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mf301_fuelgauge_fuelalert_init(fuelgauge);
		/* Set wake_lock */
		if (fg_set_wake_lock(fuelgauge) < 0) {
			pr_err("%s: fg_set_wake_lock fail\n", __func__);
			goto err_wake_lock;
		}

		INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mf301_fg_isr_work);

		fuelgauge->low_soc = pdata->irq_base + S2MF301_FG_IRQ_LOW_SOC;
		ret = request_threaded_irq(fuelgauge->low_soc, NULL, s2mf301_fg_irq_thread, 0,
			"low_soc-irq", fuelgauge);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request LOW_SOC in IRQ: %d: %d\n", __func__,
			fuelgauge->low_soc, ret);
			goto err_supply_unreg;
		}

		fuelgauge->low_vbat = pdata->irq_base + S2MF301_FG_IRQ_LOW_VBAT;
		ret = request_threaded_irq(fuelgauge->low_vbat, NULL, s2mf301_fg_irq_thread, 0,
			"low_vbat-irq", fuelgauge);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request LOW_VBAT in IRQ: %d: %d\n", __func__,
			fuelgauge->low_vbat, ret);
			goto err_supply_unreg;
		}

		fuelgauge->high_temp = pdata->irq_base + S2MF301_FG_IRQ_HIGH_TEMP;
		ret = request_threaded_irq(fuelgauge->high_temp, NULL, s2mf301_fg_irq_thread, 0,
			"high_temp-irq", fuelgauge);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request HIGH_TEMP in IRQ: %d: %d\n", __func__,
			fuelgauge->high_temp, ret);
			goto err_supply_unreg;
		}

		fuelgauge->low_temp = pdata->irq_base + S2MF301_FG_IRQ_LOW_TEMP;
		ret = request_threaded_irq(fuelgauge->low_temp, NULL, s2mf301_fg_irq_thread, 0,
			"low_temp-irq", fuelgauge);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request LOW_TEMP in IRQ: %d: %d\n", __func__,
			fuelgauge->low_temp, ret);
			goto err_supply_unreg;
		}
	}

	fuelgauge->cable_type = SEC_BATTERY_CABLE_NONE;
	fuelgauge->sleep_initial_update_of_soc = false;
	fuelgauge->initial_update_of_soc = true;
#if IS_ENABLED(TEMP_COMPEN) || IS_ENABLED(BATCAP_LEARN)
	fuelgauge->bat_charging = false;
#endif
	fuelgauge->probe_done = true;

	s2mf301_read_reg_byte(fuelgauge->i2c, S2MF301_REG_FG_ID, &temp);
	pr_info("%s: parameter ver. in IC: 0x%02x, in kernel: 0x%02x\n",
		__func__, temp & 0x0F, fuelgauge->info.battery_param_ver);

	sec_chg_set_dev_init(SC_DEV_FG);

	pr_info("%s: S2MF301 Fuelgauge Driver Loaded\n", __func__);
	return 0;
err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
err_wake_lock:
	wakeup_source_unregister(fuelgauge->fuel_alert_ws);
err_data_free:
	kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static int s2mf301_fuelgauge_remove(struct platform_device *pdev)
{
	struct s2mf301_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		device_init_wakeup(&fuelgauge->i2c->dev, false);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mf301_fuelgauge_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mf301_fuelgauge_suspend NULL
#define s2mf301_fuelgauge_resume NULL
#endif

static void s2mf301_fuelgauge_shutdown(struct platform_device *dev)
{
	pr_info("%s: S2MF301 Fuelgauge driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mf301_fuelgauge_pm_ops, s2mf301_fuelgauge_suspend,
		s2mf301_fuelgauge_resume);

static struct platform_driver s2mf301_fuelgauge_driver = {
	.driver = {
		.name = "s2mf301-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mf301_fuelgauge_pm_ops,
	},
	.probe = s2mf301_fuelgauge_probe,
	.remove = s2mf301_fuelgauge_remove,
	.shutdown = s2mf301_fuelgauge_shutdown,
};

static int __init s2mf301_fuelgauge_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mf301_fuelgauge_driver);
}

static void __exit s2mf301_fuelgauge_exit(void)
{
	platform_driver_unregister(&s2mf301_fuelgauge_driver);
}
module_init(s2mf301_fuelgauge_init);
module_exit(s2mf301_fuelgauge_exit);
MODULE_DESCRIPTION("Samsung S2MF301 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_SOFTDEP("post: s2mf301_charger");
MODULE_LICENSE("GPL");
