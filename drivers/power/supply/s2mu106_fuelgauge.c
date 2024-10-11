/*
 * s2mu106_fuelgauge.c - S2MU106 Fuel Gauge Driver
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
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

#define SINGLE_BYTE	1
#define TABLE_SIZE	22

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/power/s2mu106_fuelgauge.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/ntc_thermistor.h>

static enum power_supply_property s2mu106_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mu106_get_vbat(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_ocv(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_current(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_avgcurrent(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_avgvbat(struct s2mu106_fuelgauge_data *fuelgauge);

static void fg_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void fg_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static int fg_set_wake_lock(struct s2mu106_fuelgauge_data *fuelgauge)
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

static int s2mu106_read_reg_byte(struct i2c_client *client, int reg, void *data)
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

static int s2mu106_write_and_verify_reg_byte(struct i2c_client *client, int reg, u8 data)
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
	if ((reg == 0xee) || (reg == 0xef) || (reg == 0xf2) || (reg == 0xf3) ||
		(reg == 0x0C) || (reg == 0x1e) || (reg == 0x1f) || (reg == 0x27)) {
		return ret;
	}

	s2mu106_read_reg_byte(client, reg, &temp);
	while ((temp != data) && (i2c_corrupted_cnt < 5)) {
		dev_err(&client->dev,
			"%s: I2C write Incorrect! REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, reg, data, temp);
		ret = i2c_smbus_write_byte_data(client, reg, data);
		s2mu106_read_reg_byte(client, reg, &temp);
		i2c_corrupted_cnt++;
	}

	if (i2c_corrupted_cnt == 5)
		dev_err(&client->dev,
			"%s: I2C write failed REG: 0x%x Expected: 0x%x\n",
			__func__, reg, data);

	return ret;
}

static int s2mu106_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if IS_ENABLED(SINGLE_BYTE)
	int ret = 0;

	s2mu106_write_and_verify_reg_byte(client, reg, buf[0]);
	s2mu106_write_and_verify_reg_byte(client, reg+1, buf[1]);
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

static int s2mu106_read_reg(struct i2c_client *client, int reg, u8 *buf)
{

#if IS_ENABLED(SINGLE_BYTE)
	int ret = 0;
	u8 data1 = 0, data2 = 0;

	s2mu106_read_reg_byte(client, reg, &data1);
	s2mu106_read_reg_byte(client, reg+1, &data2);
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

static void s2mu106_fg_test_read(struct i2c_client *client)
{
	static int reg_list[] = {
		0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0E, 0x0F,
		0x10, 0x11, 0x14, 0x1A, 0x1B, 0x1E, 0x1F, 0x24, 0x25, 0x26,
		0x27, 0x28, 0x29, 0x40, 0x41, 0x43, 0x44, 0x45, 0x48, 0x49,
		0x4A, 0x4B, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0x5B,
		0x5C, 0x67
	};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mu106_read_reg_byte(client, reg_list[i], &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
	}

	/* print buffer */
	pr_info("[FG]%s: %s\n", __func__, str);
}

static void s2mu106_reset_fg(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int i;
	u8 temp = 0;

	mutex_lock(&fuelgauge->fg_lock);

	/* step 0: [Surge test] initialize register of FG */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0E, fuelgauge->info.batcap[0]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0F, fuelgauge->info.batcap[1]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x10, fuelgauge->info.batcap[2]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x11, fuelgauge->info.batcap[3]);

	/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
	temp |= 0x40;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);

	for (i = 0x92; i <= 0xe9; i++)
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i, fuelgauge->info.battery_table3[i - 0x92]);
	for (i = 0xea; i <= 0xff; i++)
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i, fuelgauge->info.battery_table4[i - 0xea]);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x14, 0x67);

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x45, &temp);
	temp &= 0xF0;
	temp |= fuelgauge->info.accum[0];
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x45, temp);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x44, fuelgauge->info.accum[1]);

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
	temp &= 0x8F;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x40, 0x08);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x41, 0x04);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x5C, 0x1A);

	/* Dumpdone. Re-calculate SOC */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
	mdelay(300);

	/* Update battery parameter version */
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &temp);
	temp &= 0xF0;
	temp |= (fuelgauge->info.battery_param_ver & 0x0F);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, temp);
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &temp);

	pr_info("%s: S2MU106_REG_FG_ID = 0x%02x, data ver. = 0x%x\n", __func__,
			temp, fuelgauge->info.battery_param_ver);

	/* If it was voltage mode, recover it */
	if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
		s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
		temp |= 0x70;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	pr_info("%s: Reset FG completed\n", __func__);
}

static int s2mu106_fix_rawsoc_reset_fg(struct s2mu106_fuelgauge_data *fuelgauge)
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
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, data);

	s2mu106_reset_fg(fuelgauge);

	/* Disable rawsoc fix */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x00);

	pr_info("%s: Finish\n", __func__);

	return ret;
}

static void s2mu106_init_regs(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

	pr_info("%s: s2mu106 fuelgauge initialize\n", __func__);

	/* Save register values for surge check */
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &temp);
	fuelgauge->reg_OTP_53 = temp;
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &temp);
	fuelgauge->reg_OTP_52 = temp;

	/* Disable VM3_flag_EN */
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_VM, &temp);
	temp = temp & 0xFB;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_VM, temp);
}

static void s2mu106_alert_init(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting: 3.55V */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_IRQ_LVL, data);
}

static int s2mu106_set_temperature(struct s2mu106_fuelgauge_data *fuelgauge,
			int temperature)
{
	/*
	 * s2mu106 include temperature sensor so,
	 * do not need to set temperature value.
	 */
	return temperature;
}

static int s2mu106_get_temperature(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int temperature = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x18);
	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
/*	pr_info("%s temp data = 0x%x 0x%x\n", __func__, data[0], data[1]); */

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
	temperature = 250;

	return temperature;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -ERANGE;
}

#if IS_ENABLED(TEMP_COMPEN)
static bool s2mu106_get_vm_status(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &data);

	return (data & (1 << 6)) ? true : false;
}

static int s2mu106_get_comp_socr(int temperature, int avg_curr)
{
	int comp_socr = 0;
	int t_socr = 0;
	int i_socr = (-222) * avg_curr;

	if (temperature <= -10)
		t_socr = ((-223) * temperature + 6500) / 1000;
	else if (temperature <= 200)
		t_socr = ((-30) * temperature + 6500) / 1000;

	comp_socr = ((t_socr + 1) * i_socr) / 100000;

	comp_socr = comp_socr - (comp_socr % 5);

	if (comp_socr > 80)
		comp_socr = 80;
	else if (comp_socr < 0)
		comp_socr = 0;

	pr_info("%s: SOCr = %d, T_SOCr = %d, I_SOCr = %d\n", __func__,
		comp_socr, t_socr, i_socr / 100000);

	return comp_socr;
}

static int s2mu106_get_soc_map(struct s2mu106_fuelgauge_data *fuelgauge,
		bool bat_charging, int comp_socr)
{
	int soc_map = 0;

	if (bat_charging) {
		if (fuelgauge->soc0i >= 9950)
			soc_map = 10000;
		else
			soc_map =
				((10040 - fuelgauge->socni) * (fuelgauge->rsoc - fuelgauge->soc0i)) /
				(10000 - fuelgauge->soc0i) + fuelgauge->socni;
	} else {
		if (fuelgauge->soc0i < ((100 * comp_socr) + 50))
			soc_map = 0;
		else
			soc_map =
				(fuelgauge->socni * (fuelgauge->rsoc - fuelgauge->soc0i)) /
				(fuelgauge->soc0i - (100 * comp_socr)) + fuelgauge->socni;
	}

	if (soc_map > 10000)
		soc_map = 10000;
	else if (soc_map < 0)
		soc_map = 0;

	return soc_map;
}

static void s2mu106_temperature_compensation(struct s2mu106_fuelgauge_data *fuelgauge)
{
	bool flag_mapping = false;
	int soc_map = 0;
	s16 diff_soc = 0;
	u8 data[2];

	fuelgauge->comp_socr =
		s2mu106_get_comp_socr(fuelgauge->temperature, fuelgauge->avg_curr);

	if (fuelgauge->init_start) {
		flag_mapping = true;
		fuelgauge->pre_comp_socr = fuelgauge->comp_socr;
		fuelgauge->pre_vm_status = fuelgauge->vm_status;
	}

	if ((fuelgauge->pre_comp_socr != fuelgauge->comp_socr) ||
			(fuelgauge->pre_bat_charging != fuelgauge->bat_charging) ||
			(fuelgauge->pre_vm_status != fuelgauge->vm_status) ||
			(fuelgauge->pre_is_charging != fuelgauge->is_charging))
		flag_mapping = true;

	if (flag_mapping == true) {
		if (fuelgauge->init_start) {
			s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);

			if (data[1] & 0x80) {
				if (data[1] & 0x40)
					data[1] = data[1] | 0x80;
				else
					data[1] = data[1] & 0x7F;

				diff_soc = (data[1] << 8) | data[0];

				pr_info("%s: init, use saved diff_soc(%d) data[1] = 0x%02x, data[0] = 0x%02x\n",
					__func__, diff_soc, data[1], data[0]);

				fuelgauge->soc_r = fuelgauge->rsoc + diff_soc;

				if (fuelgauge->soc_r > 10000)
					fuelgauge->soc_r = 10000;
				else if (fuelgauge->soc_r < 0)
					fuelgauge->soc_r = 0;

				fuelgauge->socni = fuelgauge->soc_r;
				fuelgauge->soc0i = fuelgauge->rsoc;
			} else {
				pr_info("%s: init, diff_soc is not saved\n", __func__);

				fuelgauge->socni = fuelgauge->rsoc;
				fuelgauge->soc0i = fuelgauge->rsoc;
			}
		} else {
			/* After mapping, SOC_R is maintained.
			 * If mapping occurs continuously, SOC is not changed.
			 * So SOC_R need to be updated before mapping.
			 */
			fuelgauge->soc_r = s2mu106_get_soc_map(fuelgauge,
								fuelgauge->pre_bat_charging, fuelgauge->pre_comp_socr);

			fuelgauge->socni = fuelgauge->soc_r;
			fuelgauge->soc0i = fuelgauge->rsoc;
		}
	}

	soc_map =  s2mu106_get_soc_map(fuelgauge,
			fuelgauge->bat_charging, fuelgauge->comp_socr);

#if !IS_ENABLED(INC_OK_EN)
	/* Use is_charging flag for prevent SOC increase when not charging */
	if ((fuelgauge->is_charging == false) && (soc_map > fuelgauge->soc_r)) {
		if (fuelgauge->init_start)
			fuelgauge->soc_r = soc_map;
		else
			pr_info("%s: Not charging, do not reflect SOC increase. soc_map = %d, soc_r = %d\n",
				__func__, soc_map, fuelgauge->soc_r);
	} else
		fuelgauge->soc_r = soc_map;
#else
	fuelgauge->soc_r = soc_map;
#endif

	if (fuelgauge->vm_status && (fuelgauge->soc_r > fuelgauge->rsoc) &&
	    (fuelgauge->temperature <= fuelgauge->low_temp_limit))
		fuelgauge->soc_r = fuelgauge->rsoc;

#if !IS_ENABLED(BATCAP_LEARN)
	pr_info("%s: SOC_M = %d, Chg_stat = %d, VM = %d, flag_mapping = %d, avgCURR = %d\n", __func__,
		fuelgauge->rsoc, fuelgauge->bat_charging, fuelgauge->vm_status, flag_mapping, fuelgauge->avg_curr);
	pr_info("%s: avgTEMP = %d, SOCni = %d, SOC0i = %d, SOCr = %d, SOC_R = %d\n", __func__,
		fuelgauge->temperature, fuelgauge->socni, fuelgauge->soc0i, fuelgauge->comp_socr, fuelgauge->soc_r);
#endif
	fuelgauge->init_start = 0;
	fuelgauge->pre_comp_socr = fuelgauge->comp_socr;
	fuelgauge->pre_vm_status = fuelgauge->vm_status;
	fuelgauge->pre_is_charging = fuelgauge->is_charging;
	fuelgauge->pre_bat_charging = fuelgauge->bat_charging;

	/* Save diff_soc for maintain SOC, after reboot */
	diff_soc = fuelgauge->soc_r - fuelgauge->rsoc;

	if (diff_soc > 10000)
		diff_soc = 10000;
	else if (diff_soc < -10000)
		diff_soc = -10000;

	data[1] = ((diff_soc & 0xFF00) >> 8) & 0xFF;
	data[1] = data[1] | 0x80;
	data[0] = diff_soc & 0xFF;

	s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);

	/* TODO: Print diff_soc & saved value for debugging */
	s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);

	if (data[1] & 0x40)
		data[1] = data[1] | 0x80;
	else
		data[1] = data[1] & 0x7F;

	diff_soc = (data[1] << 8) | data[0];

	pr_info("%s: diff_soc = %d, data[1] = 0x%02x, data[0] = 0x%02x\n", __func__, diff_soc, data[1], data[0]);
}
#endif

#if IS_ENABLED(BATCAP_LEARN)
static int s2mu106_get_batcap_ocv(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 batcap_ocv = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RBATCAP, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	batcap_ocv = (data[0] + (data[1] << 8)) >> 2;

	return batcap_ocv;
}

static int s2mu106_get_cycle(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, cycle;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x27);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);

	cycle = compliment;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	return cycle;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

void s2mu106_batcap_learning(struct s2mu106_fuelgauge_data *fuelgauge)
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

			s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_RBATCAP, data);
			/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
			temp |= 0x40;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);

			mutex_unlock(&fuelgauge->fg_lock);
		}
	}

	pr_info("%s: gap_cap = %d, capcc = %d, batcap_ocv = %d, bat_w = %d\n",
		__func__, gap_cap, fuelgauge->capcc, fuelgauge->batcap_ocv, bat_w);
}

static int s2mu106_get_cap_cc(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int cap_cc = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_CAPCC + 1, &data1);
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_CAPCC, &data0);
	cap_cc = (data1 << 8) | data0;
	if (cap_cc & (1 << 15)) {
		cap_cc = (~cap_cc) + 1;
		cap_cc = cap_cc / 2;
		cap_cc = cap_cc * (-1);
	} else
		cap_cc /= 2;

	return cap_cc;
}

static int s2mu106_get_soh(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int original = 0, ret = -1;
	int batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge);

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_BATCAP + 1, &data1);
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_BATCAP, &data0);
	original = (data1 << 8) | data0;

	if (original != 0) {
		ret = (batcap_ocv * 100) / original;
		if (ret > 100)
			ret = 100;
	} else
		ret = 100;

	pr_info("%s: original batcap = %d, new_batcap = %d, soh = %d\n", __func__, original, batcap_ocv, ret);

	return ret;
}
#endif

#if IS_ENABLED(BATCAP_LEARN) || IS_ENABLED(TEMP_COMPEN)
static bool s2mu106_get_bat_charging(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &data);

	return (data & (1 << 5)) ? true : false;
}
#endif

#if IS_ENABLED(BATCAP_LEARN) && IS_ENABLED(TEMP_COMPEN)
static int s2mu106_get_fullcharge_cap(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge);

	ret = ((100 - fuelgauge->comp_socr) * batcap_ocv) / 100;

	return ret;
}

static int s2mu106_get_remaining_cap(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int fcc = s2mu106_get_fullcharge_cap(fuelgauge);

	ret = (fuelgauge->soc_r) * fcc / 10000;

	pr_info("%s: fcc = %d, remaining_cap = %d\n", __func__, fcc, ret);

	return ret;
}
#endif

static int s2mu106_get_rawsoc(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2], temp;
	u16 compliment;
	u8 por_state = 0;
	u8 reg_1E = 0;
	u8 reg_OTP_52 = 0, reg_OTP_53 = 0;
#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
	bool charging_enabled = false;
#endif
	int ret = 0;
	struct power_supply *psy;
	union power_supply_propval value;
	int float_voltage = 0;
	int avg_current = 0, avg_vbat = 0, vbat = 0, curr = 0;
	u8 fg_mode_reg = 0;
#if IS_ENABLED(BATCAP_LEARN)
	int BATCAP_L_VBAT;
#endif
	enum power_supply_property psp;

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

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x1F, &por_state);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x1E, &reg_1E);
	dev_err(&fuelgauge->i2c->dev,
		"%s: OTP 52(%02x) 53(%02x), current 52(%02x) 53(%02x), 0x1F(%02x), 0x1E(%02x)\n",
		__func__, fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53, reg_OTP_52, reg_OTP_53, por_state, reg_1E);

	if (((por_state != 0x00) || (reg_1E != 0x03)) || (fuelgauge->probe_done == true &&
		(fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53))) {
		/* check charging enable */
#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
		psy = power_supply_get_by_name("s2mu106-charger");
		if (!psy)
			return -EINVAL;

		psp = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED;
		ret = power_supply_get_property(psy, psp, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		charging_enabled = value.intval;

		value.intval = S2M_BAT_CHG_MODE_CHARGING_OFF;

		psy = power_supply_get_by_name("s2mu106-charger");
		if (!psy)
			return -EINVAL;

		psp = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED;
		ret = power_supply_set_property(psy, psp, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
#endif

		if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
			psy = power_supply_get_by_name("s2mu106-charger");
			if (!psy)
				return -EINVAL;

			psp = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_FUELGAUGE_RESET;
			ret = power_supply_set_property(psy, psp, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
#endif
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
			msleep(50);
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);

			s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);

			dev_err(&fuelgauge->i2c->dev,
				"1st reset after %s: OTP 52(%02x) 53(%02x) current 52(%02x) 53(%02x)\n", __func__,
				fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53, reg_OTP_52, reg_OTP_53);

			if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
				psy = power_supply_get_by_name("s2mu106-charger");
				if (!psy)
					return -EINVAL;

				psp = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_FUELGAUGE_RESET;
				ret = power_supply_set_property(psy, psp, &value);
				if (ret < 0)
					pr_err("%s: Fail to execute property\n", __func__);
#endif
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
				msleep(50);
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);
				dev_err(&fuelgauge->i2c->dev, "%s: 2nd reset\n", __func__);
			}
		}

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		if (fuelgauge->ui_soc == 0)
			s2mu106_reset_fg(fuelgauge);
		else
			s2mu106_fix_rawsoc_reset_fg(fuelgauge);

		por_state = 0x00;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);

#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
		/* Recover charger status after f.g reset */
		if (charging_enabled) {
			value.intval = S2M_BAT_CHG_MODE_CHARGING;

			psy = power_supply_get_by_name("s2mu106-charger");
			if (!psy)
				return -EINVAL;

			psp = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED;
			ret = power_supply_set_property(psy, psp, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif
	}

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC, data) < 0)
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

	avg_current = s2mu106_get_avgcurrent(fuelgauge);
	avg_vbat = s2mu106_get_avgvbat(fuelgauge);
	vbat = s2mu106_get_vbat(fuelgauge);
	curr = s2mu106_get_current(fuelgauge);

#if IS_ENABLED(USE_EXTERNAL_TEMP)
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
#else
	fuelgauge->temperature = s2mu106_get_temperature(fuelgauge);
#endif


#if IS_ENABLED(BATCAP_LEARN) || IS_ENABLED(TEMP_COMPEN)
	fuelgauge->bat_charging = s2mu106_get_bat_charging(fuelgauge);
#endif

#if IS_ENABLED(TEMP_COMPEN)
	fuelgauge->vm_status = s2mu106_get_vm_status(fuelgauge);
	fuelgauge->avg_curr = avg_current;
	s2mu106_temperature_compensation(fuelgauge);

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

#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
	psy = power_supply_get_by_name("s2mu106-charger");
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	float_voltage = value.intval;
#else
	float_voltage = 4350;
#endif

	float_voltage = (float_voltage * 996) / 1000;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4A, &fg_mode_reg);

	dev_info(&fuelgauge->i2c->dev,
		"%s: UI SOC=%d, is_charging=%d, avg_vbat=%d, float_voltage=%d, avg_current=%d, 0x4A=0x%02x\n", __func__,
		fuelgauge->ui_soc, fuelgauge->is_charging, avg_vbat, float_voltage, avg_current, fg_mode_reg);

	if ((fuelgauge->is_charging == true) &&
	    ((fuelgauge->ui_soc >= 98) || ((avg_vbat > float_voltage) && (avg_current < 500)))) {
		if (fuelgauge->mode == CURRENT_MODE) { /* switch to VOLTAGE_MODE */
			fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
			temp |= 0x70;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in high soc voltage mode\n", __func__);
		}
	} else if ((avg_current < -50) || (avg_current >= 550)) {
		if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
			fuelgauge->mode = CURRENT_MODE;

			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
			temp &= 0x8F;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in current mode\n", __func__);
		}
	}

#if IS_ENABLED(BATCAP_LEARN)
	fuelgauge->capcc = s2mu106_get_cap_cc(fuelgauge);
	fuelgauge->batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge); // CC mode capacity
	fuelgauge->cycle = s2mu106_get_cycle(fuelgauge);
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
					if ((vbat >= BATCAP_L_VBAT) &&
					    (avg_current < BAT_L_CON[4]) && (fuelgauge->rsoc >= 9700)) {
						fuelgauge->c1_count++;
					} else
						fuelgauge->c1_count = 0;
				}
			} else {
				if (fuelgauge->c2_count >= BAT_L_CON[3]) {
					s2mu106_batcap_learning(fuelgauge);
					goto batcap_learn_init;
				} else {
					if ((vbat >= (BATCAP_L_VBAT - 100)) && (avg_current > -30) &&
					    (avg_current < 30) && (fuelgauge->rsoc >= 9800)) {
						fuelgauge->c2_count++;
					} else if (avg_current <= -30) {
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
		fuelgauge->cond1_ok  = false;
		fuelgauge->c1_count = 0;
		fuelgauge->c2_count = 0;
	}
#endif

#if IS_ENABLED(TEMP_COMPEN) && IS_ENABLED(BATCAP_LEARN)
	fuelgauge->soh = s2mu106_get_soh(fuelgauge);
	fuelgauge->capcc = s2mu106_get_cap_cc(fuelgauge);
	fuelgauge->fcc = s2mu106_get_fullcharge_cap(fuelgauge);
	fuelgauge->rmc = s2mu106_get_remaining_cap(fuelgauge);

	pr_info("%s: SOC_M = %d, Chg_stat = %d, VM = %d, avbVBAT = %d, avgCURR = %d\n", __func__,
		fuelgauge->rsoc, fuelgauge->bat_charging, fuelgauge->vm_status, avg_vbat, avg_current);
	pr_info("%s: avgTEMP = %d, SOCni = %d, SOC0i = %d, SOCr = %d, SOC_R = %d\n", __func__,
		fuelgauge->temperature, fuelgauge->socni, fuelgauge->soc0i, fuelgauge->comp_socr, fuelgauge->soc_r);
	pr_info("%s: Learning_start = %d, C1_count = %d/%d, C2_count = %d/%d\n", __func__,
		fuelgauge->learn_start, fuelgauge->c1_count, BAT_L_CON[2], fuelgauge->c2_count, BAT_L_CON[3]);
	pr_info("%s: BATCAP_OCV_new = %d, SOH = %d, CAP_CC = %d, FCC = %d, RM = %d\n", __func__,
		fuelgauge->batcap_ocv_fin, fuelgauge->soh, fuelgauge->capcc, fuelgauge->fcc, fuelgauge->rmc);
#endif

	/* Low voltage W/A, make 0% */
	if ((avg_vbat < 3400) && (avg_current < -50) && (fuelgauge->rsoc > 100)) {
		if (fuelgauge->temperature > fuelgauge->low_temp_limit) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA. Make rawsoc 0\n", __func__);

			s2mu106_read_reg_byte(fuelgauge->i2c, 0x25, &temp);
			temp &= 0xF0;
			temp |= 0x04;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x25, temp);
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x24, 0x01);

			/* Dumpdone. Re-calculate SOC */
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
			mdelay(300);

			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x24, 0x00);

			/* Make report SOC 0% */
			fuelgauge->info.soc = 0;
#if IS_ENABLED(TEMP_COMPEN)
			fuelgauge->soc_r = 0;
#endif
		} else {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA. Make UI SOC 0\n", __func__);

			/* Make report SOC 0% */
			fuelgauge->info.soc = 0;
#if IS_ENABLED(TEMP_COMPEN)
			fuelgauge->soc_r = 0;
#endif
		}
	}

#if IS_ENABLED(TEMP_COMPEN)
	/* Maintain UI SOC if battery is relaxing */
	if (((fuelgauge->temperature < fuelgauge->low_temp_limit) &&
		(fuelgauge->soc_r == 0) && (fuelgauge->rsoc > 500)) &&
		(((avg_current > -60) && (avg_current < 50)) || ((curr > -100) && (curr < 50)))) {
		fuelgauge->soc_r = fuelgauge->ui_soc * 100;
		fuelgauge->info.soc = fuelgauge->soc_r;
		fuelgauge->init_start = 1;

		dev_info(&fuelgauge->i2c->dev,
			"%s:  Maintain UI SOC if battery is relaxing SOC_R = %d, info.soc = %d\n",
			__func__, fuelgauge->soc_r, fuelgauge->info.soc);
	}
#endif

	/* S2MU106 FG debug */
	s2mu106_fg_test_read(fuelgauge->i2c);

	return min(fuelgauge->info.soc, 10000);

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu106_get_current(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RCUR_CC, data) < 0)
		return -EINVAL;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: current (%d)mA\n", __func__, curr);

	return curr;
}

static int s2mu106_get_ocv(struct s2mu106_fuelgauge_data *fuelgauge)
{
	/* 22 values of mapping table for EVT1*/
	int *soc_arr;
	int *ocv_arr;

	int soc = fuelgauge->info.soc;
	int ocv = 0;

	int high_index = TABLE_SIZE - 1;
	int low_index = 0;
	int mid_index = 0;

	soc_arr = fuelgauge->info.soc_arr_val;
	ocv_arr = fuelgauge->info.ocv_arr_val;

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
	return ocv;
}

static int s2mu106_get_avgcurrent(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x17);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: MONOUT(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avg current (%d)mA\n", __func__, curr);

	return curr;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu106_get_vbat(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RVBAT, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_info(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mu106_get_avgvbat(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, avg_vbat;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x40, 0x08);
	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x16);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);

	avg_vbat = (compliment * 1000) >> 12;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n", __func__, avg_vbat);

	return avg_vbat;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

bool s2mu106_fuelgauge_fuelalert_init(struct i2c_client *client, int soc)
{
	struct s2mu106_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	fuelgauge->is_fuel_alerted = false;

	/* 1. Set s2mu106 alert configuration. */
	s2mu106_alert_init(fuelgauge);

	if (s2mu106_read_reg(client, S2MU106_REG_IRQ, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= 0xfc;

	/*Disable IDLE_ST, INIT)ST */
	data[1] |= 0x0c;

	s2mu106_write_reg(client, S2MU106_REG_IRQ, data);

	dev_dbg(&client->dev, "%s: irq_reg(%02x%02x) irq(%d)\n", __func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}

static int s2mu106_fg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mu106_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		/* Remaining capacity unit is uAh */
		val->intval = fuelgauge->rmc * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = fuelgauge->fcc;
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mu106_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case S2M_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mu106_get_avgvbat(fuelgauge);
			break;
		case S2M_BATTERY_VOLTAGE_OCV:
			val->intval = s2mu106_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == S2M_BATTERY_CURRENT_UA)
			val->intval = s2mu106_get_current(fuelgauge) * 1000;
		else
			val->intval = s2mu106_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == S2M_BATTERY_CURRENT_UA)
			val->intval = s2mu106_get_avgcurrent(fuelgauge) * 1000;
		else
			val->intval = s2mu106_get_avgcurrent(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = s2mu106_get_rawsoc(fuelgauge) / 10;

		/* capacity should be between 0% and 100%
		 * (0.1% degree)
		 */
		if (val->intval > 1000)
			val->intval = 1000;
		if (val->intval < 0)
			val->intval = 0;

		/* check whether doing the wake_unlock */
		if (((val->intval / 10) > fuelgauge->pdata->fuel_alert_soc) && fuelgauge->is_fuel_alerted) {
			fg_wake_unlock(fuelgauge->fuel_alert_ws);
			s2mu106_fuelgauge_fuelalert_init(fuelgauge->i2c, fuelgauge->pdata->fuel_alert_soc);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = s2mu106_get_temperature(fuelgauge);
		break;
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mu106_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = fuelgauge->mode;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGE_TEMP:
			val->intval = s2mu106_get_temperature(fuelgauge);
			break;
		case POWER_SUPPLY_S2M_PROP_SOH:
#if IS_ENABLED(BATCAP_LEARN)
			fuelgauge->soh = s2mu106_get_soh(fuelgauge);
			val->intval = fuelgauge->soh;
#else
			/* If battery capacity learning is not enabled,
			 * return SOH is 100%
			 */
			val->intval = 100;
#endif
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mu106_fg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mu106_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		s2mu106_set_temperature(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED:
			if (val->intval)
				fuelgauge->is_charging = true;
			else
				fuelgauge->is_charging = false;
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mu106_fg_isr_work(struct work_struct *work)
{
	struct s2mu106_fuelgauge_data *fuelgauge = container_of(work, struct s2mu106_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_alert_status(0x%x)\n", __func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s: Battery Level(SOC) is very Low!\n", __func__);

	if (fg_alert_status & 0x02) {
		int voltage = s2mu106_get_vbat(fuelgauge);

		pr_info("%s: Battery Votage is very Low! (%dmV)\n", __func__, voltage);
	}

	if (!fg_alert_status) {
		fuelgauge->is_fuel_alerted = false;
		pr_info("%s: SOC or Voltage is Good!\n", __func__);
		fg_wake_unlock(fuelgauge->fuel_alert_ws);
	}
}

static irqreturn_t s2mu106_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mu106_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_IRQ, &fg_irq);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_irq(0x%x)\n", __func__, fg_irq);

	if (fuelgauge->is_fuel_alerted)
		return IRQ_HANDLED;

	fg_wake_lock(fuelgauge->fuel_alert_ws);
	fuelgauge->is_fuel_alerted = true;
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_OF)
static int s2mu106_fuelgauge_parse_dt(struct s2mu106_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu106-fuelgauge");
	int ret;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s: np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_vol", &fuelgauge->pdata->fuel_alert_vol);
		if (ret < 0) {
			fuelgauge->pdata->fuel_alert_vol = 3300;
			pr_err("%s: Default value of fuel_alert_vol: %d\n", __func__, fuelgauge->pdata->fuel_alert_vol);
		}

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc", &fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s: error reading pdata->fuel_alert_soc %d\n", __func__, ret);

		np = of_find_node_by_name(NULL, "battery");
		if (!np)
			pr_err("%s: np NULL\n", __func__);
		else {
			ret = of_property_read_string(np, "battery,fuelgauge_name",
							(char const **)&fuelgauge->pdata->fuelgauge_name);
			if (ret < 0)
				pr_err("%s error reading battery,fuelgauge_name\n", __func__);
		}

		/* get battery node */
		np = of_find_node_by_name(NULL, "battery");
		if (!np) {
			pr_err("%s: battery node NULL\n", __func__);
		} else {
			/* get battery data */
			ret = of_property_read_u32_array(np, "battery,battery_table3",
							fuelgauge->info.battery_table3, 88);
			if (ret < 0)
				pr_err("%s: error reading battery,battery_table3\n", __func__);

			ret = of_property_read_u32_array(np, "battery,battery_table4",
							 fuelgauge->info.battery_table4, 22);
			if (ret < 0)
				pr_err("%s: error reading battery,battery_table4\n", __func__);

			ret = of_property_read_u32_array(np, "battery,batcap",
							 fuelgauge->info.batcap, 4);
			if (ret < 0)
				pr_err("%s: error reading battery,batcap\n", __func__);

			ret = of_property_read_u32_array(np, "battery,soc_arr_val",
							 fuelgauge->info.soc_arr_val, 22);
			if (ret < 0)
				pr_err("%s: error reading battery,soc_arr_val\n", __func__);

			ret = of_property_read_u32_array(np, "battery,ocv_arr_val",
							 fuelgauge->info.ocv_arr_val, 22);
			if (ret < 0)
				pr_err("%s: error reading battery,ocv_arr_val\n", __func__);

			ret = of_property_read_u32_array(np, "battery,accum",
							 fuelgauge->info.accum, 2);
			if (ret < 0) {
				fuelgauge->info.accum[1] = 0x00; // REG 0x44
				fuelgauge->info.accum[0] = 0x08; // REG 0x45
				pr_err("%s: There is no cell1 accumulative rate in DT. Use default value(0x800)\n",
					__func__);
			}

			ret = of_property_read_u32(np, "battery,battery_param_ver", &fuelgauge->info.battery_param_ver);
			if (ret < 0)
				pr_err("%s: There is no battery parameter version\n", __func__);

			ret = of_property_read_u32(np, "battery,low_temp_limit", &fuelgauge->low_temp_limit);
			if (ret < 0) {
				pr_err("%s: There is no low temperature limit. Use default(100)\n", __func__);
				fuelgauge->low_temp_limit = 100;
			}
		}
	}

	return 0;
}

static const struct of_device_id s2mu106_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mu106-fuelgauge",},
	{},
};
#else
static int s2mu106_fuelgauge_parse_dt(struct s2mu106_fuelgauge_data *fuelgauge)
{
	return 0;
}

#define s2mu106_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static const struct power_supply_desc s2mu106_fuelgauge_power_supply_desc = {
	.name = "s2mu106-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2mu106_fuelgauge_props,
	.num_properties = ARRAY_SIZE(s2mu106_fuelgauge_props),
	.get_property = s2mu106_fg_get_property,
	.set_property = s2mu106_fg_set_property,
};

static int s2mu106_fuelgauge_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s2mu106_fuelgauge_data *fuelgauge;
	int raw_soc_val;
	struct power_supply_config fuelgauge_cfg = {};
	int ret = 0;
	u8 temp = 0;

	pr_info("%s: S2MU106 Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->i2c = client;

	if (client->dev.of_node) {
		fuelgauge->pdata = devm_kzalloc(&client->dev, sizeof(*(fuelgauge->pdata)), GFP_KERNEL);
		if (!fuelgauge->pdata) {
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = s2mu106_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0)
			goto err_parse_dt;
	} else
		fuelgauge->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "s2mu106-fuelgauge";

	fuelgauge_cfg.drv_data = fuelgauge;

	fuelgauge->revision = 0;
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x48, &temp);
	fuelgauge->revision = (temp & 0xF0) >> 4;

	pr_info("%s: S2MU106 Fuelgauge revision: 0x%x, reg 0x48 = 0x%x\n", __func__, fuelgauge->revision, temp);

	fuelgauge->info.soc = 0;

	raw_soc_val = s2mu106_get_rawsoc(fuelgauge);

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4A, &temp);
	pr_info("%s: 0x4A = 0x%02x, rawsoc = %d\n", __func__, temp, raw_soc_val);
	if (temp == 0x10)
		fuelgauge->mode = CURRENT_MODE;
	else if (temp == 0xFF)
		fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

#if IS_ENABLED(TEMP_COMPEN)
	fuelgauge->init_start = 1;
#endif
#if IS_ENABLED(BATCAP_LEARN)
	fuelgauge->learn_start = false;
	fuelgauge->cond1_ok = false;
	fuelgauge->c1_count = 0;
	fuelgauge->c2_count = 0;
#endif

	s2mu106_init_regs(fuelgauge);

	fuelgauge->psy_fg = power_supply_register(&client->dev, &s2mu106_fuelgauge_power_supply_desc, &fuelgauge_cfg);
	if (!fuelgauge->psy_fg) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_data_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mu106_fuelgauge_fuelalert_init(fuelgauge->i2c, fuelgauge->pdata->fuel_alert_soc);

		/* Set wake_lock */
		if (fg_set_wake_lock(fuelgauge) < 0) {
			pr_err("%s: fg_set_wake_lock fail\n", __func__);
			goto err_wake_lock;
		}

		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mu106_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			dev_info(&client->dev, "%s: fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
							NULL, s2mu106_fg_irq_thread,
							IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
							"fuelgauge-irq", fuelgauge);
				if (ret) {
					dev_err(&client->dev, "%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}

				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					dev_err(&client->dev,
						"%s: Failed to Enable Wakeup Source(%d)\n", __func__, ret);
			} else {
				dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n", __func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

#if IS_ENABLED(TEMP_COMPEN) || IS_ENABLED(BATCAP_LEARN)
	fuelgauge->bat_charging = false;
#endif
	fuelgauge->probe_done = true;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &temp);
	pr_info("%s: parameter ver. in IC: 0x%02x, in kernel: 0x%02x\n",
		__func__, temp & 0x0F, fuelgauge->info.battery_param_ver);

	pr_info("%s: S2MU106 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
err_wake_lock:
	wakeup_source_unregister(fuelgauge->fuel_alert_ws);
err_data_free:
	if (client->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id s2mu106_fuelgauge_id[] = {
	{"s2mu106-fuelgauge", 0},
	{}
};

static void s2mu106_fuelgauge_shutdown(struct i2c_client *client)
{

}

static int s2mu106_fuelgauge_remove(struct i2c_client *client)
{
	struct s2mu106_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		device_init_wakeup(&fuelgauge->i2c->dev, false);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mu106_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mu106_fuelgauge_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mu106_fuelgauge_suspend NULL
#define s2mu106_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mu106_fuelgauge_pm_ops, s2mu106_fuelgauge_suspend,
		s2mu106_fuelgauge_resume);

static struct i2c_driver s2mu106_fuelgauge_driver = {
	.driver = {
		.name = "s2mu106-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mu106_fuelgauge_pm_ops,
		.of_match_table = s2mu106_fuelgauge_match_table,
	},
	.probe = s2mu106_fuelgauge_probe,
	.remove = s2mu106_fuelgauge_remove,
	.shutdown = s2mu106_fuelgauge_shutdown,
	.id_table = s2mu106_fuelgauge_id,
};

static int __init s2mu106_fuelgauge_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&s2mu106_fuelgauge_driver);
}

static void __exit s2mu106_fuelgauge_exit(void)
{
	i2c_del_driver(&s2mu106_fuelgauge_driver);
}
module_init(s2mu106_fuelgauge_init);
module_exit(s2mu106_fuelgauge_exit);
MODULE_DESCRIPTION("Samsung S2MU106 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_SOFTDEP("post: s2mu106_charger");
MODULE_LICENSE("GPL");
