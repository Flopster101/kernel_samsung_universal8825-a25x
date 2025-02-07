/*
 * s2mf301_charger.c - S2MF301A Charger Driver
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include "s2mf301_charger.h"
#if IS_ENABLED(CONFIG_TOP_S2MF301)
#include <linux/muic/slsi/s2mf301/s2mf301_top.h>
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
#include "../../../battery/charger/s2mf301_charger/s2mf301_pmeter.h"
#endif
#include <linux/version.h>

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);

static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);

static char *s2mf301_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mf301_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property s2mf301_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mf301_get_charging_health(struct s2mf301_charger_data *charger);

static void s2mf301_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x05; i <= 0x32; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
	pr_info("%s: %s\n", __func__, str);
}

static void s2mf301_set_regmode(struct s2mf301_charger_data *charger, int mode)
{
	u8 data = mode & 0xFF;
	u8 reg[3] = { 0x36, 0x18, 0x36 };
	u8 val[3] = { 0x00, 0x03, 0x04 };
	u8 check_val[2] = { 0, };

	if (charger->keystring && data == CHG_MODE) {
		pr_info("%s: Skip in keystring mode during chg mode\n", __func__);
		return;
	}

	mutex_lock(&charger->regmode_mutex);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL0, &val[1]);
	val[1] &= ~REG_MODE_MASK;
	val[1] |= data & REG_MODE_MASK;

	pr_info("%s: regmode set start(0x%02x)\n", __func__, data);
	s2mf301_atomic_write(charger->i2c, reg, val, 3);
	pr_info("%s: regmode set finish\n", __func__);

	s2mf301_read_reg(charger->i2c, reg[0], &check_val[0]);
	s2mf301_read_reg(charger->i2c, reg[1], &check_val[1]);
	pr_info("%s: 0x7A%02x(%02x), 0x7A%02x(%02x)", __func__, reg[0], check_val[0], reg[1], check_val[1]);

	mutex_unlock(&charger->regmode_mutex);
}

static int s2mf301_get_en_shipmode(struct s2mf301_charger_data *charger)
{
	u8 reg;
	bool enable;
	/* S2MF301A Ship mode 0x62[4] */
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, &reg);
	enable = ((reg & 0x10) == 0x10) ? 1 : 0;

	pr_info("s2mf301-charger: %s: forced ship mode - %s\n", __func__, enable ? "Enable" : "Disable");

	return enable;
}

static void s2mf301_set_en_shipmode(struct s2mf301_charger_data *charger, bool enable)
{
	if (enable)
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, 0x10, 0x10);
	else
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, 0x00, 0x10);

	pr_info("s2mf301-charger: %s: forced ship mode - %s\n", __func__, enable ? "Enable" : "Disable");
}

static void s2mf301_set_auto_shipmode(struct s2mf301_charger_data *charger, bool enable)
{
	u8 data = (enable ? 0x40 : 0x00);

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, data, 0x40);

	pr_info("s2mf301-charger: %s: auto ship mode - %s\n", __func__, enable ? "Enable" : "Disable");
}

static void s2mf301_set_time_bat2ship_db(struct s2mf301_charger_data *charger, int time)
{
	u8 reg_data;

	pr_info("%s: shipmode batt2ship db time %d\n", __func__, time);

	if (time > 32)
		reg_data = 0x03;
	else
		reg_data = (time / 16);

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP3,
		reg_data << TIME_BAT2SHIP_DB_SHIFT, TIME_BAT2SHIP_DB_MASK);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_OPEN_OTP3, &reg_data);
	pr_info("%s: S2MF301_CHG_OPEN_OTP3: 0x%x\n", __func__, reg_data);
}

static void s2mf301_set_auto_shipmode_level(struct s2mf301_charger_data *charger, u8 reg_data)
{
	u8 read_data = 0;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_SC_STRL24, reg_data << SET_DBAT_SHIFT, SET_DBAT_MASK);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_SC_STRL24, &read_data);
	pr_info("%s: set shipmode 0x%x, 0x%x\n", __func__, reg_data, read_data);
}

#if defined(CONFIG_SHIPMODE_BY_VBAT) && !defined(CONFIG_SEC_FACTORY)
static bool s2mf301_check_current_level(void)
{
	union power_supply_propval val_avg_curr = {0, }, val_now_curr = {0, };

	val_avg_curr.intval = SEC_BATTERY_CURRENT_MA;
	val_now_curr.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property("s2mf301-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_AVG, val_avg_curr);
	psy_do_property("s2mf301-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_NOW, val_now_curr);
	pr_info("%s: current: %d, %d\n",
		__func__, val_avg_curr.intval, val_now_curr.intval);

	return ((val_avg_curr.intval > 6000) && (val_now_curr.intval > 6000)) ? true : false;

}

static u8 s2mf301_get_auto_shipmode_data(int voltage, bool offset)
{
	u8 ret = 0x00;

	if (voltage >= 4000)
		ret = 0x03;
	else if (voltage >= 3700)
		ret = 0x02;
	else if (voltage >= 3400)
		ret = 0x01;

	if (ret > offset)
		ret = (ret) ?
			(offset ? (ret - offset) : ret) : 0;
	else
		ret = 0x00;

	return ret;
}

static u8 s2mf301_check_auto_shipmode_level(struct s2mf301_charger_data *charger, bool offset)
{
	union power_supply_propval value = { 0, };
	int voltage = 2600;
	u8 reg_data = 0;
	int ari_cond = charger->spcom ? 91 : 0;

	psy_do_property("s2mf301-fuelgauge", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	if (value.intval >= 4200)
		voltage = 4000;
	else if (value.intval >= 3900)
		voltage = 3700;
	else if (value.intval >= 3700)
		voltage = 3400;

	/* Not delivered ari cnt or under 90, set 2.6v auto ship mode
	 * no dts, but if ari cnt write,
	 * it is judged to be abnormal and set 2.6v auto ship mode
	 */
	if (charger->ari_cnt < ari_cond)
		voltage = 2600;

	reg_data = s2mf301_get_auto_shipmode_data(voltage, offset);
	pr_info("%s: check shipmode %d, %d, 0x%x\n", __func__, value.intval, voltage, reg_data);

	return reg_data;
}
#endif

static int s2mf301_charger_otg_control(struct s2mf301_charger_data *charger, bool enable)
{
	u8 chg_sts2, chg_ctrl0;

	pr_info("%s: called charger otg control : %s\n", __func__, enable ? "ON" : "OFF");

	mutex_lock(&charger->charger_mutex);
	if (charger->is_charging) {
		pr_info("%s: Charger is enabled and OTG noti received!!!\n", __func__);
		pr_info("%s: is_charging: %d, otg_on: %d", __func__, charger->is_charging, charger->otg_on);
		s2mf301_test_read(charger->i2c);
		goto out;
	}

	if (charger->otg_on == enable || lpcharge)
		goto out;

	if (!enable) {
		s2mf301_set_regmode(charger, CHG_MODE);
	}
	else {
		s2mf301_update_reg(charger->i2c,
			S2MF301_CHG_CTRL4, S2MF301_SET_OTG_OCP_900mA << SET_OTG_OCP_SHIFT, SET_OTG_OCP_MASK);
		s2mf301_set_regmode(charger, OTG_MODE);
	}
	charger->otg_on = enable;

out:
	mutex_unlock(&charger->charger_mutex);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &chg_sts2);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL0, &chg_ctrl0);
	pr_info("%s S2MF301_CHG_STATUS2: 0x%x\n", __func__, chg_sts2);
	pr_info("%s S2MF301_CHG_CTRL0: 0x%x\n", __func__, chg_ctrl0);

	power_supply_changed(charger->psy_otg);
	return enable;
}

static void s2mf301_set_input_current_limit(struct s2mf301_charger_data *charger, int charging_current)
{
	u8 data;

	if (charger->keystring) {
		pr_info("%s: Skip in keystring mode\n", __func__);
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x7F, INPUT_CURRENT_LIMIT_MASK);
		return;
	}
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	if (charging_current <= 100)
		data = 0x03;
	else if (charging_current > 100 && charging_current <= 3000)
		data = (charging_current - 25) / 25;
	else
		data = 0x77;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL2,
			data << INPUT_CURRENT_LIMIT_SHIFT, INPUT_CURRENT_LIMIT_MASK);
	pr_info("[DEBUG]%s: current %d, 0x%x\n", __func__, charging_current, data);
}

static int s2mf301_get_input_current_limit(struct s2mf301_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL2, &data);
	if (data < 0)
		return data;

	data = data & INPUT_CURRENT_LIMIT_MASK;
	if (data > 0x76) {
		pr_err("%s: Invalid current limit in register\n", __func__);
		data = 0x77;
	}
	return data * 25 + 25;
}

static void s2mf301_set_regulation_vsys(struct s2mf301_charger_data *charger, int vsys)
{
	u8 data;

	pr_info("[DEBUG]%s: VSYS regulation %d\n", __func__, vsys);

	if (factory_mode) {
		vsys += 200;
		pr_info("%s: [factory_mode] Change vsys(%d)\n", __func__, vsys);
	}

	if (vsys <= 3900)
		data = 0x14;
	else if (vsys > 3900 && vsys <= 4650)
		data = (vsys - 3800) / 5;
	else
		data = 0xA0;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL8, data << SET_VSYS_SHIFT, SET_VSYS_MASK);
}

static void s2mf301_set_regulation_voltage(struct s2mf301_charger_data *charger, int float_voltage)
{
	u8 data;

	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	pr_info("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);
	if (float_voltage <= 3800)
		data = 0;
	else if (float_voltage > 3800 && float_voltage <= 4600)
		data = (float_voltage - 3800) / 5;
	else
		data = 0xA0;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL7, data << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);
}

static int s2mf301_get_regulation_voltage(struct s2mf301_charger_data *charger)
{
	u8 reg_data = 0;
	int float_voltage;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL7, &reg_data);
	reg_data &= 0xFF;
	float_voltage = reg_data * 5 + 3800;

	pr_debug("%s: battery cv reg : 0x%x, float voltage val : %d\n", __func__, reg_data, float_voltage);

	return float_voltage;
}

static void s2mf301_enable_charger_switch(struct s2mf301_charger_data *charger, int onoff)
{
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}
	mutex_lock(&charger->charger_mutex);
	if (charger->otg_on) {
		pr_info("[DEBUG] %s: skipped set(%d) : OTG is on\n", __func__, onoff);
		charger->is_charging = false;
		goto out;
	}

	if (onoff > 0) {
		s2mf301_set_regulation_voltage(charger, charger->pdata->chg_float_voltage);

		pr_info("[DEBUG]%s: turn on charger\n", __func__);
		s2mf301_set_regmode(charger, CHG_MODE);

		/* timer fault set 16hr(max) */
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL20,
				S2MF301_FC_CHG_TIMER_16hr << SET_TIME_FC_CHG_SHIFT, SET_TIME_FC_CHG_MASK);

	} else {
		pr_info("[DEBUG]%s: turn off charger\n", __func__);

		s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
	}
out:
	mutex_unlock(&charger->charger_mutex);
}

static void s2mf301_set_buck(struct s2mf301_charger_data *charger, int enable)
{
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	if (enable) {
		pr_info("[DEBUG]%s: set buck on\n", __func__);
		s2mf301_enable_charger_switch(charger, charger->is_charging);
	} else {
		pr_info("[DEBUG]%s: set buck off (charger off mode)\n", __func__);
		s2mf301_set_regmode(charger, BAT_ONLY_MODE);
	}
}

static void s2mf301_set_fast_charging_current(struct s2mf301_charger_data *charger, int charging_current)
{
	u8 data;

	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	if (charging_current <= 100)
		data = 0x01;
	else if (charging_current > 100 && charging_current <= 3500)
		data = (charging_current / 50) - 1;
	else
		data = 0x45;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL12,
			data << SC_FAST_CHARGING_CURRENT_SHIFT, SC_FAST_CHARGING_CURRENT_MASK);

	pr_info("[DEBUG]%s: current %d, 0x%02x\n", __func__, charging_current, data);
}

static int s2mf301_get_fast_charging_current(struct s2mf301_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL12, &data);
	if (data < 0)
		return data;

	data = data & SC_FAST_CHARGING_CURRENT_MASK;

	if (data > 0x3F) {
		pr_err("%s: Invalid fast charging current in register\n", __func__);
		data = 0x3F;
	}
	return (data + 1) * 50;
}

static void s2mf301_set_topoff_current(struct s2mf301_charger_data *charger, int current_limit)
{
	int data;

	pr_info("[DEBUG]%s: current %d\n", __func__, current_limit);
	if (current_limit <= 100)
		data = 0;
	else if (current_limit > 100 && current_limit <= 1000)
		data = (current_limit - 100) / 20;
	else
		data = 0x2D;

	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL16,
			data << FIRST_TOPOFF_CURRENT_SHIFT, FIRST_TOPOFF_CURRENT_MASK);
}

static int s2mf301_get_topoff_setting(struct s2mf301_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL16, &data);
	if (ret < 0)
		return ret;

	data = data & FIRST_TOPOFF_CURRENT_MASK;

	return data * 20 + 100;
}

static bool s2mf301_chg_init(struct s2mf301_charger_data *charger, struct s2mf301_dev *s2mf301)
{
	u8 sts;
	union power_supply_propval value;

	/* Set battery OCP Disable */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL13, 0x00, BAT_OCP_EN_MASK);

	/* Set topoff timer 90m */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL20,
			S2MF301_TOPOFF_TIMER_90m << SET_TIME_TOPOFF_SHIFT, SET_TIME_TOPOFF_MASK);

	/* Set watchdog timer 80s */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL18,
			S2MF301_WDT_TIMER_80s << WDT_TIME_SHIFT, WDT_TIME_MASK);

	/* Disable Auto shipmode and Enable Forced shipmode
	 * Debounce time = 0x00(0x)
	 * Auto shipmode level = 0x00(2.6V, default)
	 */
	s2mf301_set_auto_shipmode(charger, false);
	s2mf301_set_time_bat2ship_db(charger, 0);
	s2mf301_set_auto_shipmode_level(charger, 0);

	/* factory init code */
	charger->keystring = false;
	charger->bypass = false;

	if (!factory_mode) {
		s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS1, &sts);
		/* W/A SET_VF to control SYS loop on LC state */
		if (sts & LC_STATUS_MASK) {
			pr_info("%s: lc state\n", __func__);
			s2mf301_update_reg(charger->i2c, 0xD7, 0x30, 0xF0);
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x08, 0x08);
		} else {
			pr_info("%s: not lc state\n", __func__);
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x0, 0x08);
			s2mf301_update_reg(charger->i2c, 0xD7, 0xA0, 0xF0);
		}
		pr_info("%s: CHG_STATUS1(%x)\n", __func__, sts);
	} else {
		/* Switching for Flash LED to TA mode */
		value.intval = 1;
		psy_do_property("s2mf301-fled", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
	}

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_T_CHG_ON2, &sts);
	if (sts >> 7) {
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON2, 0x0, 0x80);
		s2mf301_read_reg(charger->i2c, S2MF301_CHG_T_CHG_ON2, &sts);
		pr_info("%s: 0x32(%x)\n", __func__, sts);
	}

	/* W/A SCP prevention */
	s2mf301_write_reg(charger->top, S2MF301_CHG_SET_TRIM_MUX_24, 0xFD);
	s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x04);
	s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF6, 0x04);

	return true;
}

static int s2mf301_get_charging_status(
		struct s2mf301_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts0;
	union power_supply_propval value;
	struct power_supply *psy;

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &chg_sts0);

	psy = power_supply_get_by_name(charger->pdata->fuelgauge_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (ret < 0)
		return status;

	if (!(chg_sts0 & VBUS_DET_STATUS_MASK))
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else if (chg_sts0 & CHG_UVLOB_STATUS_MASK)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;

#if IS_ENABLED(EN_TEST_READ)
	s2mf301_test_read(charger->i2c);
#endif
	return status;
}

static int s2mf301_get_charge_type(struct s2mf301_charger_data *charger)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	if (charger->is_charging) {
		if (charger->slow_charging)
			status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		else
			status = POWER_SUPPLY_CHARGE_TYPE_FAST;
	}
	return status;
}

static bool s2mf301_get_batt_present(struct s2mf301_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &data);
	if (ret < 0)
		return false;

	return (data & BATID_STATUS_MASK) ? true : false;
}

static void s2mf301_wdt_clear(struct s2mf301_charger_data *charger)
{
	u8 reg_data;

	/* watchdog kick */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_CTRL18,
			0x1 << WDT_CLR_SHIFT, WDT_CLR_MASK);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS3, &reg_data);

	if (reg_data & WDT_TIMER_FAULT_STATUS_MASK) {
		pr_info("%s: watchdog error status(0x%02x)\n", __func__, reg_data);
		if (charger->is_charging) {
			pr_info("%s: toggle charger\n", __func__);
			s2mf301_enable_charger_switch(charger, false);
			s2mf301_enable_charger_switch(charger, true);
		}
	}
}

static int s2mf301_get_charging_health(struct s2mf301_charger_data *charger)
{
	int ret;
	u8 data;

	if (charger->is_charging)
		s2mf301_wdt_clear(charger);

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &data);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	pr_info("[DEBUG] %s: S2MF301_CHG_STATUS0 0x%x\n", __func__, data);

	if ((data & VBUS_DET_STATUS_MASK) && (data & CHG_UVLOB_STATUS_MASK)) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	charger->unhealth_cnt++;
	if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
		return POWER_SUPPLY_HEALTH_GOOD;

	/* need to check ovp & health count */
	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	return POWER_SUPPLY_HEALTH_GOOD;
}

static void s2mf301_release_bypass(struct s2mf301_charger_data *charger)
{
	pr_info("%s: release Bypass mode\n", __func__);
	charger->bypass = false;

	/* CHIP2SYS ON */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF5, 0x0, 0x01);
	/* QBAT OFF DLY OFF at factory mode release */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
	/* QBAT ON */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x0, 0x02);
	/* LPM_BYPASS OFF */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, 0x0, 0x80);
	msleep(100);
	/* BUCK ONLY MODE */
	s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
	/* D2A_SC_EN_CHGIN(INPUT TR OFF) */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON0, 0x0, 0x02);
	/* EN_BYP2SYS OFF */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP5, 0x10, 0x10);
	/* MRST disable(default) */
	s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x06, 0x0F);
	/* VIO reset default */
	s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x01, 0x01);
	/* INOK_INV enable */
	s2mf301_update_reg(charger->top, S2MF301_TOP_COMMON_OTP5, 0x80, 0x80);
	/* ICR default (1.8A : 0x47) */
	s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x47);
	if (charger->keystring) {
		/* VBUS UVLO Enable */
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP0, 0x09);
		/* D2A_SC_EN_IV ON */
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x0);
	} else {
		/* VBUS UVLO disable */
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP0, 0x3F);
		/* D2A_SC_EN_IV OFF */
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x10);
	}
}

static void s2mf301_check_multi_tap_off(struct s2mf301_charger_data *charger)
{
	u8 sts2, sts7, ichgin_hi;

	if (charger->keystring || factory_mode) {
		pr_info("%s: Skip in keystring bypass Mode or factory mode\n", __func__);
		return;
	}

	pr_info("%s: check multi tap off\n", __func__);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &sts2);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS7, &sts7);
	s2mf301_read_reg(charger->pm, S2MF301_REG_PM_ICHG_STATUS, &ichgin_hi);

	if ((sts2 & IVR_STATUS_MASK) && (sts7 & DET_ASYNC_STATUS_MASK) && !(ichgin_hi & (0x1 << 3))) {
		pr_info("%s: enable chgin discharging resister\n", __func__);
		s2mf301_update_reg(charger->i2c, 0x43, 0x40, 0x40);
		s2mf301_update_reg(charger->i2c, 0x30, 0x08, 0x08);

		pr_info("%s: unmask cc, cv, icr\n", __func__);
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 0x0, (ICR_STATUS_MASK | DET_CV_STATUS_MASK | DET_CC_STATUS_MASK));
	} else {
		pr_info("%s: disable chgin discharging resister", __func__);
		s2mf301_update_reg(charger->i2c, 0x43, 0x0, 0x40);
		s2mf301_update_reg(charger->i2c, 0x30, 0x0, 0x08);

		if ((sts2 & ICR_STATUS_MASK) || (sts2 & DET_CV_STATUS_MASK) || (sts2 & DET_CC_STATUS_MASK)) {
			pr_info("%s: mask cc, cv, icr\n", __func__);
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, (ICR_STATUS_MASK | DET_CV_STATUS_MASK | DET_CC_STATUS_MASK),
					(ICR_STATUS_MASK | DET_CV_STATUS_MASK | DET_CC_STATUS_MASK));
		}
	}
}

static int s2mf301_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct s2mf301_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 data = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mf301_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mf301_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = s2mf301_get_input_current_limit(charger);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (charger->charging_current) {
			aicr = s2mf301_get_input_current_limit(charger);
			chg_curr = s2mf301_get_fast_charging_current(charger);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = s2mf301_get_topoff_setting(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mf301_get_charge_type(charger);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = s2mf301_get_regulation_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mf301_get_batt_present(charger);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch ((int)ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
			if (!s2mf301_read_reg(charger->i2c, S2MF301_REG_PMICID, &data)) {
				val->intval = (data > 0 && data < 0xFF);
				pr_info("%s : IF PMIC ver.0x%x\n", __func__, data);
			} else {
				val->intval = 0;
				pr_info("%s : IF PMIC I2C fail.\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			s2mf301_test_read(charger->i2c);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
			val->intval = s2mf301_get_en_shipmode(charger);
			pr_info("%s: manual ship mode set as %s\n", __func__, val->intval ? "enable" : "disable");
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = charger->is_charging;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			mutex_lock(&charger->charger_mutex);
			val->intval = charger->otg_on;
			mutex_unlock(&charger->charger_mutex);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGER_IC_NAME:
			val->strval = "S2MF301";
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			val->intval = charger->ari_cnt;
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

static int s2mf301_chg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	enum power_supply_lsi_property lsi_psp = (enum power_supply_lsi_property) psp;
	int buck_state = ENABLE;
	union power_supply_propval value;
	int ret;
	int vbat = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->slow_charging = false;
		charger->ivr_on = false;

		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_err("[DEBUG]%s:[BATT] Type Battery\n", __func__);
			value.intval = 0;
		} else
			value.intval = 1;

		psy = power_supply_get_by_name(charger->pdata->fuelgauge_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ENERGY_AVG, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		if (is_nocharge_type(charger->cable_type)) {
			/* At cable removal enable IVR IRQ if it was disabled */
			if (charger->irq_ivr_enabled == 0) {
				u8 reg_data;

				charger->irq_ivr_enabled = 1;
				/* Unmask IRQ */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 0 << IVR_M_SHIFT, IVR_M_MASK);
				enable_irq(charger->irq_ivr);
				s2mf301_read_reg(charger->i2c, S2MF301_CHG_INT2M, &reg_data);
				pr_info("%s : enable ivr : 0x%x\n", __func__, reg_data);
			}
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		{
			int input_current = val->intval;

			s2mf301_set_input_current_limit(charger, input_current);
			charger->input_current = input_current;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pr_info("[DEBUG] %s: is_charging %d\n", __func__, charger->is_charging);
		charger->charging_current = val->intval;
		s2mf301_set_fast_charging_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		charger->topoff_current = val->intval;
		s2mf301_set_topoff_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		pr_info("[DEBUG]%s: float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mf301_set_regulation_voltage(charger, charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		{
			u8 ivr_state = 0;

			s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &ivr_state);
			if (ivr_state & IVR_STATUS_MASK) {
				__pm_stay_awake(charger->ivr_ws);
				/* Mask IRQ */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 1 << IVR_M_SHIFT, IVR_M_MASK);
				queue_delayed_work(charger->charger_wqueue, &charger->ivr_work,
					msecs_to_jiffies(IVR_WORK_DELAY));
			}
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		pr_info("%s: Set FLED MODE (charge mode: %d)\n", __func__, charger->charge_mode);
		if (charger->otg_on) {
			pr_info("%s: otg operating, skip boost mode\n", __func__);
		} else {
			if (val->intval) {
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x01, 0x01);
				s2mf301_set_regmode(charger, BOOST_ONLY_MODE);
				usleep_range(1000, 1100);
			} else {
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x00, 0x01);
				switch (charger->charge_mode) {
				case SEC_BAT_CHG_MODE_BUCK_OFF:
					s2mf301_set_regmode(charger, BAT_ONLY_MODE);
					break;
				case SEC_BAT_CHG_MODE_CHARGING_OFF:
					s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
					break;
				case SEC_BAT_CHG_MODE_CHARGING:
					s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
					usleep_range(1000, 1100);
					s2mf301_set_regmode(charger, CHG_MODE);
					break;
				}
			}
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (!factory_mode) {
			pr_info("%s: masking lc, sc, buck interrupt\n", __func__);
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT0M,
					BUCK_ONLY_MODE_MASK, BUCK_ONLY_MODE_MASK);
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT1M,
					LC_STATUS_MASK|SC_STATUS_MASK, LC_STATUS_MASK|SC_STATUS_MASK);
		}

		if (val->intval == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC) {
			if (charger->bypass) {
				pr_info("%s: Skip Factory Mode in bypass mode\n", __func__);
				break;
			}
			pr_info("%s: Set Factory Mode (vbus + 523K)\n", __func__);

			/* forced set buck on /charge off */
			s2mf301_enable_charger_switch(charger, 0);

			/* ICR MAX */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x7F);
			/* RD_OR_VBUS_MUX_SEL */
			value.intval = 0;
			psy_do_property("usbpd-manager", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
			/* IN2BAT OFF */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_CHG_OPTION0, 0x01, 0x01);
			msleep(400);
			/* QBAT OFF DLY ON at factory mode release */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x20, 0x20);
			/* BAT2SYS OFF at factory mode release */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON5, 0x0, 0x40);
			/* QBAT OFF */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x02, 0x02);
			/* CHIP2SYS OFF */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF5, 0x01, 0x01);
			/* EN_MRST, SET_MRSTBTMR 1.0s */
			s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x08, 0x0F);
			/* VIO RESET OFF */
			s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x0, 0x01);
			/* SYS Regulation 4.4V(default) */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL8, 0x78);
			/* EN_FAC_CHG_301k Enable */
			s2mf301_update_reg(charger->top, S2MF301_TOP_AUTO_FAC_CHG, 0x44, 0x44);

			/* Switching for fuel gauge to get SYS voltage */
			value.intval = SEC_BAT_FGSRC_SWITCHING_VSYS;
			psy_do_property("s2mf301-fuelgauge", set, POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);

			/* Switching for Flash LED to TA mode */
			value.intval = 1;
			psy_do_property("s2mf301-fled", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);

			factory_mode = 1;
		} else if (val->intval == ATTACHED_DEV_JIG_USB_ON_MUIC) {
			if (charger->bypass) {
				pr_info("%s: Skip Factory Mode in bypass mode\n", __func__);
				break;
			}
			pr_info("%s: Set Factory Mode (vbus + 301K)\n", __func__);

			/* forced set buck on /charge off */
			s2mf301_enable_charger_switch(charger, 0);

			/* ICR MAX */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x7F);
			/* QBAT ON */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x0, 0x02);
			/* CHIP2SYS ON */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF5, 0x0, 0x01);
			/* RD_ONLY_MUX_SEL */
			value.intval = 2;
			psy_do_property("usbpd-manager", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
			/* IN2BAT ON */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_CHG_OPTION0, 0x0, 0x01);
			if (!factory_mode) {
				/* QBAT OFF DLY OFF at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
				/* BAT2SYS ON at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON5, 0x40, 0x40);
			} else {
				/* QBAT OFF DLY ON at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x20, 0x20);
				/* BAT2SYS OFF at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON5, 0x0, 0x40);
			}
			/* EN_MRST,SET_MRSTBTMR 1.0s */
			s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x08, 0x0F);
			/* SYS regulation 4.2V */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL8, 0x50);
			/* EN Factory CHG 301K disable */
			s2mf301_update_reg(charger->top, S2MF301_TOP_AUTO_FAC_CHG, 0x0, 0x44);
			/* VIO RESET ON */
			s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x01, 0x01);

			/* FGSRC = VBAT when BAT is connected */
			psy = power_supply_get_by_name("s2mf301-pmeter");
			if (!psy) {
				pr_info("%s, Fail to get pmeter psy\n", __func__);
				return -EINVAL;
			}
			ret = power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_LSI_PROP_VBAT, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property(VBAT)\n", __func__);
			vbat = value.intval;

			pr_info("%s: battery voltage(%d)\n", __func__, vbat);
			psy = power_supply_get_by_name(charger->pdata->fuelgauge_name);
			if (!psy) {
				pr_info("%s, Fail to get FG psy\n", __func__);
				return -EINVAL;
			}
			if (vbat >= 3200) {
				pr_info("%s: battery exist: FGSRC = VBAT\n", __func__);
				value.intval = SEC_BAT_FGSRC_SWITCHING_VBAT;
			} else {
				pr_info("%s: battery not exist: FGSRC = VSYS\n", __func__);
				value.intval = SEC_BAT_FGSRC_SWITCHING_VSYS;
			}
			psy_do_property("s2mf301-fuelgauge", set, POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);

			factory_mode = 1;
		} else {
			if (charger->bypass) {
				if (val->intval == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
					pr_info("%s: Bypass + 255K\n", __func__);
					/* RD_ONLY_MUX_SEL */
					value.intval = 2;
					psy_do_property("usbpd-manager", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
					/* MRST disable(default) */
					s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x06, 0x0F);
					/* Type-C VIO reset disable */
					s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x0, 0x40);
					/* VIO RESET ON */
					s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x01, 0x01);
					/* QBAT OFF DLY OFF at factory mode release */
					s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
					/* INOK_INV enable */
					s2mf301_update_reg(charger->top, S2MF301_TOP_COMMON_OTP5, 0x80, 0x80);
				} else
					pr_info("%s: Bypass + 619K, do not enter normal code\n", __func__);
				break;
			}
			if (val->intval == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
				pr_info("%s: vbus + 255K\n", __func__);
				/* RD_ONLY_MUX_SEL */
				value.intval = 2;
				psy_do_property("usbpd-manager", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
				/* MRST disable(default) */
				s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x06, 0x0F);
				/* Type-C VIO reset disable */
				s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x0, 0x40);
				/* VIO RESET ON */
				s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x01, 0x01);
				/* QBAT OFF DLY OFF at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
				/* ICR MAX */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x7F);
				/* VBUS UVLO Disable(VBUS Input IR Drop) */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP0, 0x3F);
				/* D2A_SC_EN_IV OFF */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x10);
				break;
			}

			pr_info("%s: Release Factory Mode (vbus + 619K)\n", __func__);

			charger->keystring = false;
			/* ICR default 1.8A */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x47);
			/* QBAT ON */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x0, 0x02);
			/* CHIP2SYS ON */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF5, 0x0, 0x01);
			/* MRST disable(default) */
			s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x06, 0x0F);
			/* VIO reset default */
			s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x01, 0x01);
			/* IN2BAT OFF */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_CHG_OPTION0, 0x01, 0x01);
			/* SYS Regulation 4.4V(default) */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL8, 0x78);
			/* EN_FAC_CHG_Default */
			s2mf301_write_reg(charger->top, S2MF301_TOP_AUTO_FAC_CHG, 0x66);
			/* QBAT OFF DLY OFF at factory mode release */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
			/* BAT2SYS OFF at factory mode release */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON5, 0x0, 0x40);
			/* RD_VBUS_MUX_SEL */
			value.intval = 0;
			psy_do_property("usbpd-manager", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
			/* D2A_SC_EN_IV OFF */
			s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x10);

			/* Switching for fuel gauge to get Battery voltage */
			value.intval = SEC_BAT_FGSRC_SWITCHING_VBAT;
			psy_do_property("s2mf301-fuelgauge", set, POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);
			factory_mode = 0;

			/* Switching for Flash LED to AUTO mode */
			value.intval = 0;
			psy_do_property("s2mf301-fled", set, POWER_SUPPLY_PROP_ENERGY_NOW, value);
		}
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		if (val->intval) {
			pr_info("%s: set Bypass mode for leakage current(0,0,2,0 power off)\n", __func__);
			/* EN Factory CHG default */
			s2mf301_write_reg(charger->top, S2MF301_TOP_AUTO_FAC_CHG, 0x66);
		} else {
			s2mf301_release_bypass(charger);

			/* Acquired Battery voltage */
			value.intval = SEC_BAT_FGSRC_SWITCHING_VBAT;
			psy_do_property("s2mf301-fuelgauge", set, POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION:
			if (val->intval) {
				pr_info("%s: set Bypass mode for leakage current(0,0,1,0,power off)\n", __func__);
				charger->bypass = true;
				/* set BUCK ONLY MODE */
				s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
				/* VBUS UVLO Disable(VBUS Input IR Drop) */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP0, 0x3F);
				/* D2A_SC_EN_IV OFF */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x10);
				/* D2A_SC_EN_CHGIN(INPUT TR ON) */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON0, 0x02, 0x02);
				/* EN_MRST, SET_MRSTBTMR 7.0s */
				s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x0E, 0x0F);
				/* INOK_INV disable */
				s2mf301_update_reg(charger->top, S2MF301_TOP_COMMON_OTP5, 0x0, 0x80);
				/* LPM_BYPASS */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, 0x80, 0x80);
				/* EN_BYP2SYS */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP5, 0x00, 0x10);
				/* DC_BYPASS */
				s2mf301_set_regmode(charger, DC_BUCK_MODE);
				/* PM Disable */
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_TA_MASK1, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_TA_MASK2, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_OTG_MASK1, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_OTG_MASK2, 0xFF);
				/* EN Factory CHG disable */
				s2mf301_write_reg(charger->top, S2MF301_TOP_AUTO_FAC_CHG, 0x00);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION:
			pr_info("%s: factory voltage regulation (%d)\n", __func__, val->intval);
			s2mf301_set_regulation_vsys(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			if (val->intval) {
				if (charger->bypass) {
					pr_info("%s: Skip keystring Mode in bypass mode\n", __func__);
					break;
				}
				pr_info("%s: Skip ivr in keystring mode\n", __func__);
				cancel_delayed_work(&charger->ivr_work);
				__pm_relax(charger->ivr_ws);

				pr_info("%s: set Bypass mode for current measure(power on)\n", __func__);

				charger->keystring = true;

				/* detach */
				value.intval = 1;
				psy_do_property("muic-manager", set, POWER_SUPPLY_LSI_PROP_PM_FACTORY, value);

				/* VBUS UVLO Disable(VBUS Input IR Drop)*/
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP0, 0x3F);
				/* D2A_SC_EN_IV OFF */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF2, 0x10);
				/* set BUCK ONLY MODE */
				s2mf301_set_regmode(charger, BUCK_ONLY_MODE);
				msleep(400);
				/* ICR MAX */
				s2mf301_write_reg(charger->i2c, S2MF301_CHG_CTRL2, 0x7F);
				/* D2A_SC_EN_CHGIN(INPUT TR ON) */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON0, 0x02, 0x02);
				/* EN_MRST, SET_MRSTBTMR 1.0s */
				s2mf301_update_reg(charger->top, S2MF301_TOP_MRSTB_RESET, 0x08, 0x0F);
				/* QBAT OFF */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON6, 0x02, 0x02);
				/* CHIP2SYS OFF */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_OFF5, 0x01, 0x01);
				/* VIO RESET OFF */
				s2mf301_update_reg(charger->top, S2MF301_TOP_I2C_RESET_CTRL, 0x0, 0x01);
				/* QBAT OFF DLY OFF at factory mode release */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_OPEN_OTP0, 0x0, 0x20);
				/* LPM_BYPASS */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_SHIP_MODE_CTRL, 0x80, 0x80);
				/* EN_BYP2SYS */
				s2mf301_update_reg(charger->i2c, S2MF301_CHG_D2A_SC_OTP5, 0x00, 0x10);
				/* DC_BYPASS */
				s2mf301_set_regmode(charger, DC_BUCK_MODE);
				/* PM Disable */
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_TA_MASK1, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_TA_MASK2, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_OTG_MASK1, 0xFF);
				s2mf301_write_reg(charger->pm, S2MF301_REG_PM_ADCEN_BY_OTG_MASK2, 0xFF);
			} else {
				s2mf301_release_bypass(charger);
				charger->keystring = false;

				/* Acquired Battery voltage */
				value.intval = SEC_BAT_FGSRC_SWITCHING_VBAT;
				psy_do_property("s2mf301-fuelgauge", set, POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			s2mf301_charger_otg_control(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
			pr_info("%s: manual ship mode is %s\n", __func__, val->intval ? "enable" : "disable");
			s2mf301_set_en_shipmode(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			charger->charge_mode = val->intval;

			psy = power_supply_get_by_name("battery");
			if (!psy)
				return -EINVAL;

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			switch (charger->charge_mode) {
				case SEC_BAT_CHG_MODE_BUCK_OFF:
					buck_state = DISABLE;
					charger->is_charging = false;
					break;
				case SEC_BAT_CHG_MODE_CHARGING_OFF:
					charger->is_charging = false;
					break;
				case SEC_BAT_CHG_MODE_CHARGING:
					charger->is_charging = true;
					break;
			}

			if (buck_state)
				s2mf301_enable_charger_switch(charger, charger->is_charging);
			else
				s2mf301_set_buck(charger, buck_state);
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			if (charger->spcom) {
				charger->ari_cnt = val->intval;
				dev_info(charger->dev, "%s: ari cnt:%d\n",
						__func__, charger->ari_cnt);
			} else {
				charger->ari_cnt = -1;
				dev_info(charger->dev, "%s: not support ari cnt: %d\n",
					__func__, val->intval);
			}
			break;
		default:
			switch (lsi_psp) {
			case POWER_SUPPLY_LSI_PROP_ICHGIN:
				s2mf301_check_multi_tap_off(charger);
				break;
			default:
				return -EINVAL;
			}
			return 0;
		}
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mf301_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->otg_on;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL:
			s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &reg);
			pr_info("%s: S2MF301_CHG_STATUS2 : 0x%X\n", __func__, reg);
			if (!(reg & OTG_SS_END_STATUS_MASK) && (reg & BST_START_END_STATUS_MASK))
				val->intval = 1;
			else
				val->intval = 0;
			s2mf301_read_reg(charger->i2c, S2MF301_CHG_CTRL0, &reg);
			pr_info("%s: S2MF301_CHG_CTRL0 : 0x%X\n", __func__, reg);
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

static int s2mf301_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_charger_data *charger = power_supply_get_drvdata(psy);
	union power_supply_propval value;
	enum power_supply_property psp_t;
	int ret;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		value.intval = val->intval;
		pr_info("%s: OTG %s\n", __func__, value.intval > 0 ? "ON" : "OFF");

		psy = power_supply_get_by_name(charger->pdata->charger_name);
		if (!psy)
			return -EINVAL;

		psp_t = (enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL;
		ret = power_supply_set_property(psy, psp_t, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		power_supply_changed(charger->psy_otg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#if IS_ENABLED(EN_BAT_DET_IRQ)
/* s2mf301 interrupt service routine */
static irqreturn_t s2mf301_det_bat_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	u8 val;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &val);
	if (!(val & BATID_STATUS_MASK)) {
		s2mf301_enable_charger_switch(charger, 0);
		pr_err("charger-off if battery removed\n");
	}
	return IRQ_HANDLED;
}
#endif

static irqreturn_t s2mf301_done_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	u8 val;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS1, &val);
	pr_info("%s , %02x\n", __func__, val);
	if (val & (DONE_STATUS_MASK)) {
		pr_err("add self chg done\n");
		/* add chg done code here */
	}
	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_chg_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	u8 val;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &val);
	pr_info("%s , %02x\n", __func__, val);

	if (!(val & CHG_UVLOB_STATUS_MASK)) {
		pr_info("%s: disable chgin discharging resister\n", __func__);
		s2mf301_update_reg(charger->i2c, 0x43, 0x0, 0x40);
		s2mf301_update_reg(charger->i2c, 0x30, 0x0, 0x08);

		pr_info("%s: mask cc, cv, icr\n", __func__);
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, (ICR_STATUS_MASK | DET_CV_STATUS_MASK | DET_CC_STATUS_MASK),
				(ICR_STATUS_MASK | DET_CV_STATUS_MASK | DET_CC_STATUS_MASK));
	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_event_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	union power_supply_propval value;
	enum power_supply_property psp_t;
	struct power_supply *psy;
	u8 val;
	int ret = 0;
	u8 sts0, sts1, sts2, sts3, sts4, sts5, sts6, sts7;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &sts0);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS1, &sts1);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &sts2);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS3, &sts3);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS4, &sts4);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS5, &sts5);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS6, &sts6);
	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS7, &sts7);

	pr_info("[IRQ] %s, STATUS0~7 0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		__func__, sts0, sts1, sts2, sts3, sts4, sts5, sts6, sts7);

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS3, &val);
	pr_info("%s , %02x\n", __func__, val);

	if ((val & WDT_TIMER_FAULT_STATUS_MASK) || (val & INPUT_OCP_STATUS_MASK)) {
		value.intval = 1;
		pr_info("%s, reset USBPD\n", __func__);

		psy = power_supply_get_by_name("usbpd-manager");
		if (!psy)
			return -EINVAL;

		psp_t = (enum power_supply_property)POWER_SUPPLY_LSI_PROP_USBPD_RESET;
		ret = power_supply_set_property(psy, psp_t, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
	}

	return IRQ_HANDLED;
}

static bool s2mf301_check_slow_charging(struct s2mf301_charger_data *charger, int input_current)
{
	pr_info("%s: charger->cable_type %d, input_current %d\n",
		__func__, charger->cable_type, input_current);

	/* under 400mA considered as slow charging concept for VZW */
	if (input_current <= charger->pdata->slow_charging_current &&
		!is_nocharge_type(charger->cable_type)) {
		union power_supply_propval value;

		charger->slow_charging = true;
		pr_info("%s: slow charging on : input current(%dmA), cable type(%d)\n",
			__func__, input_current, charger->cable_type);
		value.intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	} else
		charger->slow_charging = false;

	return charger->slow_charging;
}

static void s2mf301_reduce_input_current(struct s2mf301_charger_data *charger)
{
	int old_input_current, new_input_current;

	old_input_current = s2mf301_get_input_current_limit(charger);
	new_input_current = (old_input_current > MINIMUM_INPUT_CURRENT + REDUCE_CURRENT_STEP) ?
		(old_input_current - REDUCE_CURRENT_STEP) : MINIMUM_INPUT_CURRENT;

	if (old_input_current <= new_input_current) {
		pr_info("%s: Same or less new input current:(%d, %d, %d)\n", __func__,
			old_input_current, new_input_current, charger->input_current);
	} else {
		pr_info("%s: input currents:(%d, %d, %d)\n", __func__,
			old_input_current, new_input_current, charger->input_current);
		s2mf301_set_input_current_limit(charger, new_input_current);
		charger->input_current = s2mf301_get_input_current_limit(charger);
	}
	charger->ivr_on = true;
}

static void s2mf301_ivr_irq_work(struct work_struct *work)
{
	struct s2mf301_charger_data *charger = container_of(work, struct s2mf301_charger_data, ivr_work.work);
	u8 ivr_state;
	int ret;
	int ivr_cnt = 0;

	pr_info("%s:\n", __func__);

	if (is_nocharge_type(charger->cable_type)) {
		u8 ivr_mask;

		pr_info("%s : skip\n", __func__);
		s2mf301_read_reg(charger->i2c, S2MF301_CHG_INT2M, &ivr_mask);
		if (ivr_mask & 0x01) /* Unmask IRQ */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 0 << IVR_M_SHIFT, IVR_M_MASK);
		__pm_relax(charger->ivr_ws);
		return;
	}

	ret = s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &ivr_state);
	if (ret < 0) {
		__pm_relax(charger->ivr_ws);
		pr_info("%s : I2C error\n", __func__);
		/* Unmask IRQ */
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 0 << IVR_M_SHIFT, IVR_M_MASK);
		return;
	}
	pr_info("%s: ivr_status 0x0C:0x%02x\n", __func__, ivr_state);

	s2mf301_check_multi_tap_off(charger);

	mutex_lock(&charger->ivr_mutex);

	while ((ivr_state & IVR_STATUS_MASK) && charger->cable_type != SEC_BATTERY_CABLE_NONE) {

		if (s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS2, &ivr_state)) {
			pr_err("%s: Error reading S2MF301_CHG_STATUS2\n", __func__);
			break;
		}
		pr_info("%s: ivr_status 0x0C:0x%02x\n", __func__, ivr_state);

		if (++ivr_cnt >= 2) {
			s2mf301_reduce_input_current(charger);
			ivr_cnt = 0;
		}
		msleep(IVR_WORK_DELAY);

		if (!(ivr_state & IVR_STATUS_MASK)) {
			pr_info("%s: EXIT IVR WORK: check value (0x0C:0x%02x, input current:%d)\n", __func__,
				ivr_state, charger->input_current);
			break;
		}

		if (s2mf301_get_input_current_limit(charger) <= MINIMUM_INPUT_CURRENT)
			break;
	}

	if (charger->ivr_on) {
		union power_supply_propval value;

		if (is_not_wireless_type(charger->cable_type))
			s2mf301_check_slow_charging(charger, charger->input_current);

		if ((charger->irq_ivr_enabled == 1) &&
			(charger->input_current <= MINIMUM_INPUT_CURRENT) &&
			(charger->slow_charging)) {
			/* Disable IVR IRQ, can't reduce current any more */
			u8 reg_data;

			charger->irq_ivr_enabled = 0;
			disable_irq_nosync(charger->irq_ivr);
			/* Mask IRQ */
			s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 1 << IVR_M_SHIFT, IVR_M_MASK);
			s2mf301_read_reg(charger->i2c, S2MF301_CHG_INT2M, &reg_data);
			pr_info("%s : disable ivr : 0x%x\n", __func__, reg_data);
		}

		value.intval = s2mf301_get_input_current_limit(charger);
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
	}

	if (charger->irq_ivr_enabled == 1) /* Unmask IRQ */
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 0 << IVR_M_SHIFT, IVR_M_MASK);
	mutex_unlock(&charger->ivr_mutex);
	__pm_relax(charger->ivr_ws);
}

static irqreturn_t s2mf301_ivr_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;

	if (charger->keystring) {
		pr_info("%s: Skip in keystring mode\n", __func__);
		cancel_delayed_work(&charger->ivr_work);
		__pm_relax(charger->ivr_ws);
		return IRQ_HANDLED;
	}

	pr_info("%s: irq(%d)\n", __func__, irq);
	__pm_stay_awake(charger->ivr_ws);

	/* Mask IRQ */
	s2mf301_update_reg(charger->i2c, S2MF301_CHG_INT2M, 1 << IVR_M_SHIFT, IVR_M_MASK);
	queue_delayed_work(charger->charger_wqueue, &charger->ivr_work, msecs_to_jiffies(IVR_WORK_DELAY));

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_ovp_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	u8 val;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS0, &val);
	pr_info("%s ovp %02x\n", __func__, val);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_lc_sc_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;
	u8 sts;

	s2mf301_read_reg(charger->i2c, S2MF301_CHG_STATUS1, &sts);

	/* W/A SET_VF to control SYS loop on LC state */
	if (sts & LC_STATUS_MASK) {
		pr_info("%s: lc state\n", __func__);
		s2mf301_update_reg(charger->i2c, 0xD7, 0x30, 0xF0);
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x08, 0x08);
	} else {
		pr_info("%s: not lc state\n", __func__);
		s2mf301_update_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x0, 0x08);
		s2mf301_update_reg(charger->i2c, 0xD7, 0xA0, 0xF0);
	}
	pr_info("%s: CHG_STATUS1(%x)\n", __func__, sts);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_det_loop_int_isr(int irq, void *data)
{
	struct s2mf301_charger_data *charger = data;

	pr_info("%s\n", __func__);
	s2mf301_check_multi_tap_off(charger);

	return IRQ_HANDLED;
}

static int s2mf301_charger_parse_dt(struct s2mf301_charger_data *charger)
{
	struct s2mf301_charger_platform_data *pdata = charger->pdata;
	struct device_node *np = of_find_node_by_name(NULL, "s2mf301-charger");
	struct device_node *spss_region_dn;
	int ret = 0;

	if (!np)
		pr_err("%s np NULL(s2mf301-charger)\n", __func__);
	else {
		ret = of_property_read_u32(np, "charger,slow_charging_current", &pdata->slow_charging_current);
		if (ret) {
			pr_info("%s : slow_charging_current is Empty\n", __func__);
			pdata->slow_charging_current = SLOW_CHARGING_CURRENT_STANDARD;
		} else {
			pr_info("%s : slow_charging_current is %d\n", __func__, pdata->slow_charging_current);
		}
	}

	spss_region_dn = of_find_node_by_name(NULL, "qcom,spcom");
	if (spss_region_dn == NULL) {
#if IS_ENABLED(CONFIG_QCOM_SPSS)
		pr_info("%s: couldn't find qcom,spcom, config enabled\n", __func__);
#else
		pr_info("%s: couldn't find qcom,spcom\n", __func__);
#endif
		charger->spcom = false;
	} else {
		charger->spcom = true;
		pr_info("%s: found spcom\n", __func__);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np, "battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
		if (ret < 0)
			pr_info("%s: Fuel-gauge name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_float_voltage", &pdata->chg_float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4350;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n", __func__, pdata->chg_float_voltage);
	}

	pr_info("%s DT file parsed successfully, %d\n", __func__, ret);
	return ret;
}

/* if need to set s2mf301 pdata */
static const struct of_device_id s2mf301_charger_match_table[] = {
	{ .compatible = "samsung,s2mf301-charger",},
	{},
};

static char *s2mf301_otg_supply_list[] = {
	"otg",
};

static int s2mf301_charger_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_platform_data *pdata = dev_get_platdata(s2mf301->dev);
	struct s2mf301_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	u8 reg;

	pr_info("%s:[BATT] S2MF301 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->charger_mutex);
	mutex_init(&charger->ivr_mutex);
	mutex_init(&charger->regmode_mutex);
	charger->otg_on = false;
	charger->ivr_on = false;
	charger->slow_charging = false;

	charger->dev = &pdev->dev;
	charger->i2c = s2mf301->chg;
	charger->top = s2mf301->i2c;
	charger->fg = s2mf301->fg;
	charger->pm = s2mf301->pm;
	charger->ari_cnt = 0;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)), GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mf301_charger_parse_dt(charger);
	if (ret < 0) {
		pr_err("%s: s2mf301_charger_parse_dt fail\n", __func__);
		goto err_parse_dt;
	}

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "s2mf301-charger";
	if (charger->pdata->fuelgauge_name == NULL)
		charger->pdata->fuelgauge_name = "s2mf301-fuelgauge";

	charger->psy_chg_desc.name	= charger->pdata->charger_name;
	charger->psy_chg_desc.type	= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg_desc.get_property	= s2mf301_chg_get_property;
	charger->psy_chg_desc.set_property	= s2mf301_chg_set_property;
	charger->psy_chg_desc.properties	= s2mf301_charger_props;
	charger->psy_chg_desc.num_properties = ARRAY_SIZE(s2mf301_charger_props);

	charger->psy_otg_desc.name	= "s2mf301-otg";
	charger->psy_otg_desc.type	= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_otg_desc.get_property	= s2mf301_otg_get_property;
	charger->psy_otg_desc.set_property	= s2mf301_otg_set_property;
	charger->psy_otg_desc.properties	= s2mf301_otg_props;
	charger->psy_otg_desc.num_properties = ARRAY_SIZE(s2mf301_otg_props);

	s2mf301_chg_init(charger, s2mf301);
	charger->input_current = s2mf301_get_input_current_limit(charger);
	charger->charging_current = s2mf301_get_fast_charging_current(charger);

	s2mf301_read_reg(charger->top, S2MF301_REG_ES_NO_REV_NO, &reg);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mf301_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mf301_supplied_to);

	charger->psy_chg = power_supply_register(&pdev->dev, &charger->psy_chg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(charger->psy_chg);
		goto err_power_supply_register;
	}

	charger->psy_otg = power_supply_register(&pdev->dev, &charger->psy_otg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_otg)) {
		pr_err("%s: Failed to Register psy_otg\n", __func__);
		ret = PTR_ERR(charger->psy_otg);
		goto err_power_supply_register_otg;
	}
	charger->psy_otg->supplied_to = s2mf301_otg_supply_list;
	charger->psy_otg->num_supplicants = ARRAY_SIZE(s2mf301_otg_supply_list);

	charger->charger_wqueue = create_singlethread_workqueue("charger-wq");
	if (!charger->charger_wqueue) {
		pr_info("%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}

	charger->ivr_ws = wakeup_source_register(&pdev->dev, "charger-ivr");
	INIT_DELAYED_WORK(&charger->ivr_work, s2mf301_ivr_irq_work);

	/*
	 * irq request
	 * if you need to add irq , please refer below code.
	 */
	charger->irq_ovp = pdata->irq_base + S2MF301_CHG0_IRQ_CHGIN_OVP;
	ret = request_threaded_irq(charger->irq_ovp, NULL, s2mf301_ovp_isr, 0, "ovp-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request CHGIN_OVP in IRQ: %d: %d\n",
			__func__, charger->irq_ovp, ret);
		goto err_reg_irq;
	}

	charger->irq_topoff = pdata->irq_base + S2MF301_CHG1_IRQ_TOPOFF;
	ret = request_threaded_irq(charger->irq_topoff, NULL, s2mf301_event_isr, 0, "topoff-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request TOPOFF in IRQ: %d: %d\n",
			__func__, charger->irq_topoff, ret);
		goto err_reg_irq;
	}

	charger->irq_wdt_ap_reset = pdata->irq_base + S2MF301_CHG3_IRQ_INPUT_OCP;
	ret = request_threaded_irq(charger->irq_wdt_ap_reset, NULL, s2mf301_event_isr, 0, "wdt_ap_reset-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request WDT_AP_RESET in IRQ: %d: %d\n",
			__func__, charger->irq_wdt_ap_reset, ret);
		goto err_reg_irq;
	}

	charger->irq_wdt_suspend = pdata->irq_base + S2MF301_CHG3_IRQ_WDT_TIMER_FAULT;
	ret = request_threaded_irq(charger->irq_wdt_suspend, NULL, s2mf301_event_isr, 0, "wdt_suspend-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request WDT_SUSPEND in IRQ: %d: %d\n",
			__func__, charger->irq_wdt_suspend, ret);
		goto err_reg_irq;
	}

	charger->irq_pre_trickle_chg_timer_fault = pdata->irq_base + S2MF301_CHG3_IRQ_PRE_TRICKLE_CHG_TIMER_FAULT;
	ret = request_threaded_irq(charger->irq_pre_trickle_chg_timer_fault,
			NULL, s2mf301_event_isr, 0, "pre_trickle_chg_timer_fault-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request PRE_TRICKLE_CHG_TIMER_FAULT in IRQ: %d: %d\n",
			__func__, charger->irq_pre_trickle_chg_timer_fault, ret);
		goto err_reg_irq;
	}

	charger->irq_cool_fast_chg_timer_fault = pdata->irq_base + S2MF301_CHG3_IRQ_LC_SC_CV_CHG_TIMER_FAULT;
	ret = request_threaded_irq(charger->irq_cool_fast_chg_timer_fault,
		NULL, s2mf301_event_isr, 0, "cool_fast_chg_timer_fault-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request COOL_FAST_CHG_TIMER_FAULT in IRQ: %d: %d\n",
			__func__, charger->irq_cool_fast_chg_timer_fault, ret);
		goto err_reg_irq;
	}

	charger->irq_topoff_timer_fault = pdata->irq_base + S2MF301_CHG3_IRQ_TOPOFF_TIMER_FAULT;
	ret = request_threaded_irq(charger->irq_topoff_timer_fault,
		NULL, s2mf301_event_isr, 0, "topoff_timer_fault-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request TOPOFF_TIMER_FAULT in IRQ: %d: %d\n",
			__func__, charger->irq_topoff_timer_fault, ret);
		goto err_reg_irq;
	}

	charger->irq_tfb = pdata->irq_base + S2MF301_CHG4_IRQ_TFB;
	ret = request_threaded_irq(charger->irq_tfb, NULL, s2mf301_event_isr, 0, "tfb-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request TFB in IRQ: %d: %d\n", __func__, charger->irq_tfb, ret);
		goto err_reg_irq;
	}

	charger->irq_tsd = pdata->irq_base + S2MF301_CHG4_IRQ_TSD;
	ret = request_threaded_irq(charger->irq_tsd, NULL, s2mf301_event_isr, 0, "tsd-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request TSD IRQ: %d: %d\n", __func__, charger->irq_tsd, ret);
		goto err_reg_irq;
	}

	charger->irq_bat_ocp = pdata->irq_base + S2MF301_CHG3_IRQ_BAT_OCP;
	ret = request_threaded_irq(charger->irq_bat_ocp, NULL, s2mf301_event_isr, 0, "bat_ocp-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request OCP in IRQ: %d: %d\n", __func__, charger->irq_bat_ocp, ret);
		goto err_reg_irq;
	}

	charger->irq_icr = pdata->irq_base + S2MF301_CHG2_IRQ_ICR;
	ret = request_threaded_irq(charger->irq_icr, NULL, s2mf301_det_loop_int_isr, 0, "icr-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request ICR in IRQ: %d: %d\n", __func__, charger->irq_icr, ret);
		goto err_reg_irq;
	}

	charger->irq_cv = pdata->irq_base + S2MF301_CHG2_IRQ_VOLTAGE_LOOP;
	ret = request_threaded_irq(charger->irq_cv, NULL, s2mf301_det_loop_int_isr, 0, "cv-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request CV in IRQ: %d: %d\n", __func__, charger->irq_cv, ret);
		goto err_reg_irq;
	}

	charger->irq_cc = pdata->irq_base + S2MF301_CHG2_IRQ_SC_CC_LOOP;
	ret = request_threaded_irq(charger->irq_cc, NULL, s2mf301_det_loop_int_isr, 0, "cc-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request CC in IRQ: %d: %d\n", __func__, charger->irq_cc, ret);
		goto err_reg_irq;
	}

	charger->irq_ivr = pdata->irq_base + S2MF301_CHG2_IRQ_IVR;
	charger->irq_ivr_enabled = 1;
	ret = request_threaded_irq(charger->irq_ivr, NULL, s2mf301_ivr_isr, 0, "ivr-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request IVR in IRQ: %d: %d\n", __func__, charger->irq_ivr, ret);
		charger->irq_ivr_enabled = -1;
		goto err_reg_irq;
	}

#if IS_ENABLED(EN_BAT_DET_IRQ)
	charger->irq_det_bat = pdata->irq_base + S2MF301_CHG0_IRQ_BATID;
	ret = request_threaded_irq(charger->irq_det_bat, NULL, s2mf301_det_bat_isr, 0, "det_bat-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev,
			"%s: Fail to request DET_BAT in IRQ: %d: %d\n", __func__, charger->irq_det_bat, ret);
		goto err_reg_irq;
	}
#endif

#if IS_ENABLED(EN_CHG1_IRQ_CHGIN)
	charger->irq_uvlob = pdata->irq_base + S2MF301_CHG0_IRQ_CHGIN_UVLOB;
	ret = request_threaded_irq(charger->irq_uvlob, NULL, s2mf301_chg_isr, 0, "uvlob-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request CHGIN_UVLOB in IRQ: %d: %d\n",
			__func__, charger->irq_uvlob, ret);
		goto err_reg_irq;
	}
#endif

	charger->irq_in2bat = pdata->irq_base + S2MF301_CHG0_IRQ_CHGIN2BATB;
	ret = request_threaded_irq(charger->irq_in2bat, NULL, s2mf301_chg_isr, 0, "in2bat-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev,
			"%s: Fail to request CHGIN2BATB in IRQ: %d: %d\n", __func__, charger->irq_in2bat, ret);
		goto err_reg_irq;
	}

	charger->irq_done = pdata->irq_base + S2MF301_CHG1_IRQ_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL, s2mf301_done_isr, 0, "done-irq", charger);
	if (ret < 0) {
		dev_err(s2mf301->dev, "%s: Fail to request DONE in IRQ: %d: %d\n", __func__, charger->irq_done, ret);
		goto err_reg_irq;
	}

	if (!factory_mode) {
		charger->irq_lc = pdata->irq_base + S2MF301_CHG1_IRQ_LC;
		ret = request_threaded_irq(charger->irq_lc, NULL, s2mf301_lc_sc_isr, 0, "lc-irq", charger);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request LC in IRQ: %d: %d\n", __func__, charger->irq_lc, ret);
			goto err_reg_irq;
		}

		charger->irq_sc = pdata->irq_base + S2MF301_CHG1_IRQ_SC;
		ret = request_threaded_irq(charger->irq_sc, NULL, s2mf301_lc_sc_isr, 0, "sc-irq", charger);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request SC in IRQ: %d: %d\n", __func__, charger->irq_sc, ret);
			goto err_reg_irq;
		}

		charger->irq_buck_only = pdata->irq_base + S2MF301_CHG0_IRQ_BUCK_ONLY_MODE;
		ret = request_threaded_irq(charger->irq_buck_only, NULL, s2mf301_lc_sc_isr, 0, "buck_only-irq", charger);
		if (ret < 0) {
			dev_err(s2mf301->dev, "%s: Fail to request BUCK_ONLY in IRQ: %d: %d\n", __func__, charger->irq_buck_only, ret);
			goto err_reg_irq;
		}
	}

	/* Do max charging by freq. change, when duty is max */
	s2mf301_update_reg(charger->i2c, 0x7A, 0x1 << 4, 0x1 << 4);
#if IS_ENABLED(EN_TEST_READ)
	s2mf301_test_read(charger->i2c);
#endif

	sec_chg_set_dev_init(SC_DEV_MAIN_CHG);

	pr_info("%s:[BATT] S2MF301 charger driver loaded OK\n", __func__);

#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	s2mf301_muic_charger_init();
#endif

	return 0;

err_reg_irq:
	destroy_workqueue(charger->charger_wqueue);
err_create_wq:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_chg);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->charger_mutex);
	mutex_destroy(&charger->ivr_mutex);
	mutex_destroy(&charger->regmode_mutex);
	kfree(charger);
	return ret;
}

static int s2mf301_charger_remove(struct platform_device *pdev)
{
	struct s2mf301_charger_data *charger = platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->charger_mutex);
	mutex_destroy(&charger->ivr_mutex);
	mutex_destroy(&charger->regmode_mutex);
	kfree(charger);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_charger_suspend(struct device *dev)
{
	struct s2mf301_charger_data *charger = dev_get_drvdata(dev);

	if (factory_mode) {
		/* buck async mode */
		pr_info("%s: buck async mode\n", __func__);
		/* for protecting normal set in 619K. this is not real Bypass */
		charger->bypass = true;
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x40);
	}
	return 0;
}

static int s2mf301_charger_resume(struct device *dev)
{
	struct s2mf301_charger_data *charger = dev_get_drvdata(dev);

	if (factory_mode) {
		/* buck sync mode */
		pr_info("%s: buck sync mode\n", __func__);
		s2mf301_write_reg(charger->i2c, S2MF301_CHG_T_CHG_ON3, 0x0);
	}
	return 0;
}
#else
#define s2mf301_charger_suspend NULL
#define s2mf301_charger_resume NULL
#endif

static void s2mf301_charger_shutdown(struct platform_device *dev)
{
	struct s2mf301_charger_data *charger = platform_get_drvdata(dev);

	s2mf301_set_regulation_voltage(charger, charger->pdata->chg_float_voltage);

#if defined(CONFIG_SHIPMODE_BY_VBAT) && !defined(CONFIG_SEC_FACTORY)
	{
		u8 auto_shipmode_level;

		if ((charger->cable_type != POWER_SUPPLY_TYPE_BATTERY &&
				charger->cable_type != POWER_SUPPLY_TYPE_UNKNOWN) || lpcharge)
			if (s2mf301_check_current_level())
				auto_shipmode_level = s2mf301_check_auto_shipmode_level(charger, 2);
			else
				auto_shipmode_level = s2mf301_check_auto_shipmode_level(charger, 1);
		else
			auto_shipmode_level = s2mf301_check_auto_shipmode_level(charger, 0);

		s2mf301_set_auto_shipmode_level(charger, auto_shipmode_level);
	}
#endif
	s2mf301_set_time_bat2ship_db(charger, 192);
	s2mf301_set_auto_shipmode(charger, true);

	pr_info("%s: S2MF301 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mf301_charger_pm_ops, s2mf301_charger_suspend, s2mf301_charger_resume);

static struct platform_driver s2mf301_charger_driver = {
	.driver = {
		.name		= "s2mf301-charger",
		.owner		= THIS_MODULE,
		.of_match_table = s2mf301_charger_match_table,
		.pm		= &s2mf301_charger_pm_ops,
	},
	.probe		= s2mf301_charger_probe,
	.remove		= s2mf301_charger_remove,
	.shutdown	= s2mf301_charger_shutdown,
};

static int __init s2mf301_charger_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mf301_charger_driver);
}
module_init(s2mf301_charger_init);

static void __exit s2mf301_charger_exit(void)
{
	platform_driver_unregister(&s2mf301_charger_driver);
}
module_exit(s2mf301_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MF301");
