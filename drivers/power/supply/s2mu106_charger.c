/*
 * s2mu106_charger.c - S2MU106 Charger Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/power/s2mu106_charger.h>
#include <linux/version.h>

static char *s2mu106_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mu106_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property s2mu106_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mu106_get_charging_health(struct s2mu106_charger_data *charger);

static void s2mu106_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x08; i <= 0x24; i++) {
		s2mu106_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
	s2mu106_read_reg(i2c, 0x2F, &data);
	sprintf(str+strlen(str), "0x2F:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x33, &data);
	sprintf(str+strlen(str), "0x33:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x35, &data);
	sprintf(str+strlen(str), "0x35:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x3D, &data);
	sprintf(str+strlen(str), "0x3D:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x71, &data);
	sprintf(str+strlen(str), "0x71:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x7A, &data);
	sprintf(str+strlen(str), "0x7A:0x%02x, ", data);

	s2mu106_read_reg(i2c, 0x8B, &data);
	pr_err("%s: %s0x8B:0x%02x\n", __func__, str, data);

}

static int s2mu106_charger_otg_control(struct s2mu106_charger_data *charger, bool enable)
{
	u8 chg_sts2, chg_ctrl0;

	pr_info("%s: called charger otg control : %s\n", __func__, enable ? "ON" : "OFF");

	mutex_lock(&charger->charger_mutex);
	if (charger->is_charging) {
		pr_info("%s: Charger is enabled and OTG noti received!!!\n", __func__);
		pr_info("%s: is_charging: %d, otg_on: %d", __func__, charger->is_charging, charger->otg_on);
		s2mu106_test_read(charger->i2c);
		goto out;
	}

	if (charger->otg_on == enable)
		goto out;

	if (!enable)
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, CHG_MODE, REG_MODE_MASK);
	else {
		s2mu106_update_reg(charger->i2c,
				S2MU106_CHG_CTRL3, S2MU106_SET_OTG_OCP_1500mA << SET_OTG_OCP_SHIFT, SET_OTG_OCP_MASK);
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, OTG_BST_MODE, REG_MODE_MASK);
		charger->cable_type = POWER_SUPPLY_TYPE_OTG;
	}
	charger->otg_on = enable;

out:
	mutex_unlock(&charger->charger_mutex);

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS2, &chg_sts2);
	s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL0, &chg_ctrl0);
	pr_info("%s S2MU106_CHG_STATUS2: 0x%x\n", __func__, chg_sts2);
	pr_info("%s S2MU106_CHG_CTRL0: 0x%x\n", __func__, chg_ctrl0);

	power_supply_changed(charger->psy_otg);
	return enable;
}

static void s2mu106_enable_charger_switch(struct s2mu106_charger_data *charger, int onoff)
{
	mutex_lock(&charger->charger_mutex);
	if (charger->otg_on) {
		pr_info("[DEBUG] %s: skipped set(%d) : OTG is on\n", __func__, onoff);
		charger->is_charging = false;
		goto out;
	}

	if(!(charger->pdata->erd)) {
		pr_info("[DEBUG][SMDK]%s: turn off charger\n", __func__);
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, BUCK_MODE, REG_MODE_MASK);
	}
	else{
		if (onoff > 0) {
			pr_info("[DEBUG]%s: turn on charger\n", __func__);
			s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, CHG_MODE, REG_MODE_MASK);

			/* timer fault set 16hr(max) */
			s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL13,
					S2MU106_FC_CHG_TIMER_16hr << SET_TIME_FC_CHG_SHIFT, SET_TIME_FC_CHG_MASK);

		} else {
			pr_info("[DEBUG]%s: turn off charger\n", __func__);

			s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, BUCK_MODE, REG_MODE_MASK);
		}
	}
out:
	mutex_unlock(&charger->charger_mutex);
}

static void s2mu106_set_buck(struct s2mu106_charger_data *charger, int enable)
{

	if (enable) {
		pr_info("[DEBUG]%s: set buck on\n", __func__);
		s2mu106_enable_charger_switch(charger, charger->is_charging);
	} else {
		pr_info("[DEBUG]%s: set buck off (charger off mode)\n", __func__);
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL0, CHARGER_OFF_MODE, REG_MODE_MASK);
	}
}

static void s2mu106_set_regulation_voltage(struct s2mu106_charger_data *charger, int float_voltage)
{
	u8 data;

	pr_info("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);
	if (float_voltage <= 3900)
		data = 0;
	else if (float_voltage > 3900 && float_voltage <= 4530)
		data = (float_voltage - 3900) / 5;
	else
		data = 0x7f;

	s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL5, data << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);
}

static int s2mu106_get_regulation_voltage(struct s2mu106_charger_data *charger)
{
	u8 reg_data = 0;
	int float_voltage;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL5, &reg_data);
	reg_data &= 0x7F;
	float_voltage = reg_data * 5 + 3900;

	pr_debug("%s: battery cv reg : 0x%x, float voltage val : %d\n", __func__, reg_data, float_voltage);

	return float_voltage;
}

static void s2mu106_set_input_current_limit(struct s2mu106_charger_data *charger, int charging_current)
{
	u8 data;

	if (charging_current <= 100)
		data = 0x02;
	else if (charging_current > 100 && charging_current <= 3000)
		data = (charging_current - 50) / 25;
	else
		data = 0x62;

	s2mu106_update_reg(charger->i2c,
			   S2MU106_CHG_CTRL1, data << INPUT_CURRENT_LIMIT_SHIFT, INPUT_CURRENT_LIMIT_MASK);

	pr_info("[DEBUG]%s: current  %d, 0x%x\n", __func__, charging_current, data);

	//s2mu106_test_read(charger->i2c);
}

static int s2mu106_get_input_current_limit(struct s2mu106_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL1, &data);
	if (ret < 0)
		return ret;

	data = data & INPUT_CURRENT_LIMIT_MASK;
	if (data > 0x76) {
		pr_err("%s: Invalid current limit in register\n", __func__);
		data = 0x76;
	}
	return  data * 25 + 50;
}

//TODO: need to set wcin current
static void s2mu106_set_fast_charging_current(struct s2mu106_charger_data *charger, int charging_current)
{
	u8 data;

	if (charging_current <= 100)
		data = 0x01;
	else if (charging_current > 100 && charging_current <= 3200)
		data = (charging_current / 50) - 1;
	else
		data = 0x3D;

	s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL7,
			   data << FAST_CHARGING_CURRENT_SHIFT, FAST_CHARGING_CURRENT_MASK);

	pr_info("[DEBUG]%s: current  %d, 0x%02x\n", __func__, charging_current, data);

	//s2mu106_test_read(charger->i2c);
}

static int s2mu106_get_fast_charging_current(struct s2mu106_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL7, &data);
	if (ret < 0)
		return ret;

	data = data & FAST_CHARGING_CURRENT_MASK;

	if (data > 0x3F) {
		pr_err("%s: Invalid fast charging current in register\n", __func__);
		data = 0x3F;
	}
	return (data + 1) * 50;
}

static void s2mu106_set_topoff_current(struct s2mu106_charger_data *charger, int eoc_1st_2nd, int current_limit)
{
	int data;

	pr_info("[DEBUG]%s: current  %d\n", __func__, current_limit);
	if (current_limit <= 100)
		data = 0;
	else if (current_limit > 100 && current_limit <= 475)
		data = (current_limit - 100) / 25;
	else
		data = 0x0F;

	switch (eoc_1st_2nd) {
	case 1:
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL10,
				data << FIRST_TOPOFF_CURRENT_SHIFT, FIRST_TOPOFF_CURRENT_MASK);
		break;
	case 2:
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL10,
				data << SECOND_TOPOFF_CURRENT_SHIFT, SECOND_TOPOFF_CURRENT_MASK);
		break;
	default:
		break;
	}
}

static int s2mu106_get_topoff_setting(struct s2mu106_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL10, &data);
	if (ret < 0)
		return ret;

	data = data & FIRST_TOPOFF_CURRENT_MASK;

	if (data > 0x0F)
		data = 0x0F;
	return data * 25 + 100;
}

static bool s2mu106_chg_init(struct s2mu106_charger_data *charger)
{
	/* Set default regulation voltage 4.35v */
	//s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL5, 0x5A << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);

	s2mu106_update_reg(charger->i2c, 0x8b, 0x00, 0x01 << 4);

	/* Set battery OCP 7A */
	s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL9, S2MU106_SET_BAT_OCP_7000mA, BAT_OCP_MASK);

	/* Type-C reset off */
	s2mu106_update_reg(charger->i2c, 0xEC, 0x00, 0x80);

	/* MRSTB 1s set */
	s2mu106_write_reg(charger->i2c, 0xE5, 0x08);

	if(!(charger->pdata->erd)) {
		/* QBAT_Diode_Fix_Mode=0 setting */
		s2mu106_update_reg(charger->i2c, S2MU106_CHG_SC_OTP34, 0x00, 0x08);

		s2mu106_enable_charger_switch(charger, 0);
		pr_info("[DEBUG][SMDK]%s\n", __func__);
	}

	return true;
}

static int s2mu106_get_charging_status(
		struct s2mu106_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts0;
	union power_supply_propval value;
	struct power_supply *psy;

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS0, &chg_sts0);

	psy = power_supply_get_by_name(charger->pdata->fuelgauge_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (ret < 0)
		return status;

	if (chg_sts0 & 0x03 || chg_sts0 & 0x05)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else if (chg_sts0 & 0x02)
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

#if IS_ENABLED(EN_TEST_READ)
	s2mu106_test_read(charger->i2c);
#endif
	return status;
}

static int s2mu106_get_charge_type(struct s2mu106_charger_data *charger)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 data;

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS3, &data);
	if (ret < 0)
		pr_err("%s fail\n", __func__);

	switch ((data & BAT_STATUS_MASK) >> BAT_STATUS_SHIFT) {
	case 0x6:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case 0x2:
	case 0x3:
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}
	return status;
}

static bool s2mu106_get_batt_present(struct s2mu106_charger_data *charger)
{
	int ret;
	u8 data;

	/* Enable charger CLK for BAT_ID check */
	s2mu106_update_reg(charger->i2c, 0xF1, 0x01, 0x01);
	msleep(30);

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS3, &data);
	if (ret < 0)
		return false;

	s2mu106_update_reg(charger->i2c, 0xF1, 0x00, 0x01);

	return (data & DET_BAT_STATUS_MASK) ? true : false;
}

static void s2mu106_wdt_clear(struct s2mu106_charger_data *charger)
{
	u8 reg_data, chg_fault_status;

	/* watchdog kick */
	s2mu106_update_reg(charger->i2c, S2MU106_CHG_CTRL12,
			0x1 << WDT_CLR_SHIFT, WDT_CLR_MASK);

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS1, &reg_data);
	chg_fault_status = (reg_data & CHG_FAULT_STATUS_MASK) >> CHG_FAULT_STATUS_SHIFT;

	if ((chg_fault_status == CHG_STATUS_WD_SUSPEND) || (chg_fault_status == CHG_STATUS_WD_RST)) {
		pr_info("%s: watchdog error status(0x%02x,%d)\n", __func__, reg_data, chg_fault_status);
		if (charger->is_charging) {
			pr_info("%s: toggle charger\n", __func__);
			s2mu106_enable_charger_switch(charger, false);
			s2mu106_enable_charger_switch(charger, true);
		}
	}
}

static int s2mu106_get_charging_health(struct s2mu106_charger_data *charger)
{
	int ret;
	u8 data;
	union power_supply_propval value;
	struct power_supply *psy;

	if (charger->is_charging)
		s2mu106_wdt_clear(charger);

	ret = s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS0, &data);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	pr_info("[DEBUG] %s: S2MU106_CHG_STATUS0 0x%x\n", __func__, data);

	data = (data & (CHGIN_STATUS_MASK)) >> CHGIN_STATUS_SHIFT;

	switch (data) {
	case 0x03:
	case 0x05:
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		return POWER_SUPPLY_HEALTH_GOOD;
	default:
		break;
	}

	charger->unhealth_cnt++;
	if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
		return POWER_SUPPLY_HEALTH_GOOD;

	/* 005 need to check ovp & health count */
	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (value.intval == POWER_SUPPLY_TYPE_USB_PD)
		return POWER_SUPPLY_S2M_HEALTH_UNDERVOLTAGE;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static int s2mu106_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct s2mu106_charger_data *charger = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mu106_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mu106_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = s2mu106_get_input_current_limit(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			aicr = s2mu106_get_input_current_limit(charger);
			chg_curr = s2mu106_get_fast_charging_current(charger);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = s2mu106_get_fast_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mu106_get_charge_type(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = s2mu106_get_regulation_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mu106_get_batt_present(charger);
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED:
			val->intval = charger->is_charging;
			break;
		case POWER_SUPPLY_S2M_PROP_CURRENT_FULL:
			val->intval = s2mu106_get_topoff_setting(charger);
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

static int s2mu106_chg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mu106_charger_data *charger = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
	int buck_state = ENABLE;
	union power_supply_propval value;
	enum power_supply_property psp_t;
	int ret;
	u8 data = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type != POWER_SUPPLY_TYPE_OTG) {
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
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		{
			int input_current = val->intval;

			s2mu106_set_input_current_limit(charger, input_current);
			charger->input_current = input_current;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pr_info("[DEBUG] %s: is_charging %d\n", __func__, charger->is_charging);
		charger->charging_current = val->intval;
		/* set charging current */
		if (charger->is_charging)
			s2mu106_set_fast_charging_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("[DEBUG]%s: float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mu106_set_regulation_voltage(charger, charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		pr_info("%s: %s factory image support mode\n", __func__,
			(val->intval == S2M_BAT_FAC_MODE_VBUS) ? "VBUS" : "VBat");
		if (val->intval == S2M_BAT_FAC_MODE_VBUS) {
			s2mu106_enable_charger_switch(charger, false);
			s2mu106_set_input_current_limit(charger, 2000);
			s2mu106_set_fast_charging_current(charger, 2000);
		} else
			s2mu106_set_buck(charger, false);
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CURRENT_FULL:
			charger->topoff_current = val->intval;
			s2mu106_set_topoff_current(charger, 1, val->intval);
			break;
		case POWER_SUPPLY_S2M_PROP_CHARGE_OTG_CONTROL:
			s2mu106_charger_otg_control(charger, val->intval);
			break;
		case POWER_SUPPLY_S2M_PROP_FUELGAUGE_RESET:
			s2mu106_read_reg(charger->i2c, 0xE3, &data);
			data |= 0x03 << 6;
			s2mu106_write_reg(charger->i2c, 0xE3, data);
			msleep(1000);
			data &= ~(0x03 << 6);
			s2mu106_write_reg(charger->i2c, 0xE3, data);
			msleep(50);
			pr_info("%s: reset fuelgauge when surge occur!\n", __func__);
			break;
		case POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED:
			charger->charge_mode = val->intval;

			psy = power_supply_get_by_name("battery");
			if (!psy)
				return -EINVAL;

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			if (value.intval != POWER_SUPPLY_TYPE_OTG) {
				switch (charger->charge_mode) {
				case S2M_BAT_CHG_MODE_BUCK_OFF:
					buck_state = DISABLE;
					charger->is_charging = false;
					break;
				case S2M_BAT_CHG_MODE_CHARGING_OFF:
					charger->is_charging = false;
					break;
				case S2M_BAT_CHG_MODE_CHARGING:
					charger->is_charging = true;
					break;
				}

				if (buck_state)
					s2mu106_enable_charger_switch(charger, charger->is_charging);
				else
					s2mu106_set_buck(charger, buck_state);

				value.intval = charger->is_charging;

				psy = power_supply_get_by_name(charger->pdata->fuelgauge_name);
				if (!psy)
					return -EINVAL;
				psp_t = (enum power_supply_property) POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED;
				ret = power_supply_set_property(psy, psp_t, &value);
				if (ret < 0)
					pr_err("%s: Fail to execute property\n", __func__);

			} else
				pr_info("[DEBUG]%s: SKIP CHARGING CONTROL while OTG(%d)\n", __func__, value.intval);
			break;
		case POWER_SUPPLY_S2M_PROP_USBPD_TEST_READ:
			s2mu106_test_read(charger->i2c);
			s2mu106_read_reg(charger->i2c, 0xEC, &data);
			pr_info("%s, charger 0xEC=(%x)\n", __func__, data);
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

static int s2mu106_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mu106_charger_data *charger = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
	u8 reg;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->otg_on;
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGE_POWERED_OTG_CONTROL:
			s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS2, &reg);
			pr_info("%s: S2MU106_CHG_STATUS2 : 0x%X\n", __func__, reg);
			if ((reg & 0xC0) == 0x80)
				val->intval = 1;
			else
				val->intval = 0;
			s2mu106_read_reg(charger->i2c, S2MU106_CHG_CTRL0, &reg);
			pr_info("%s: S2MU106_CHG_CTRL0 : 0x%X\n", __func__, reg);
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

static int s2mu106_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mu106_charger_data *charger = power_supply_get_drvdata(psy);
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

		psp_t = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHARGE_OTG_CONTROL;
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

static void s2mu106_charger_otg_vbus_work(struct work_struct *work)
{
	struct s2mu106_charger_data *charger = container_of(work, struct s2mu106_charger_data, otg_vbus_work.work);

	s2mu106_write_reg(charger->i2c, S2MU106_CHG_CTRL11, 0x16);
}

#if IS_ENABLED(EN_BAT_DET_IRQ)
/* s2mu106 interrupt service routine */
static irqreturn_t s2mu106_det_bat_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	u8 val;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS3, &val);
	if ((val & DET_BAT_STATUS_MASK) == 0) {
		s2mu106_enable_charger_switch(charger, 0);
		pr_err("charger-off if battery removed\n");
	}
	return IRQ_HANDLED;
}
#endif

static irqreturn_t s2mu106_done_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	u8 val;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS1, &val);
	pr_info("%s , %02x\n", __func__, val);
	if (val & (DONE_STATUS_MASK)) {
		pr_err("add self chg done\n");
		/* add chg done code here */
	}
	return IRQ_HANDLED;
}

static irqreturn_t s2mu106_chg_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	union power_supply_propval value;
	u8 val;

	value.intval = POWER_SUPPLY_HEALTH_GOOD;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS0, &val);
	pr_info("%s , %02x\n", __func__, val);
	return IRQ_HANDLED;
}

static irqreturn_t s2mu106_event_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	union power_supply_propval value;
	enum power_supply_property psp_t;
	struct power_supply *psy;
	u8 val;
	u8 fault;
	int ret = 0;
	u8 sts0, sts1, sts2, sts3, sts5;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS0, &sts0);
	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS1, &sts1);
	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS2, &sts2);
	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS3, &sts3);
	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS5, &sts5);

	pr_info("[IRQ] %s, STATUS0:0x%02x, STATUS1:0x%02x, STATUS2:0x%02x, STATUS3:0x%02x, STATUS5:0x%02x\n",
		__func__, sts0, sts1, sts2, sts3, sts5);

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS1, &val);
	pr_info("%s , %02x\n", __func__, val);

	fault = (val & CHG_FAULT_STATUS_MASK) >> CHG_FAULT_STATUS_SHIFT;

	if (fault == CHG_STATUS_WD_SUSPEND || fault == CHG_STATUS_WD_RST) {
		value.intval = 1;
		pr_info("%s, reset USBPD\n", __func__);

		psy = power_supply_get_by_name("s2mu106-usbpd");
		if (!psy)
			return -EINVAL;

		psp_t = (enum power_supply_property)POWER_SUPPLY_S2M_PROP_USBPD_RESET;
		ret = power_supply_set_property(psy, psp_t, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mu106_ivr_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	union power_supply_propval value;
	struct power_supply *psy;
	int ret = 0;
	u8 sts;
	int input_curr;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS5, &sts);

	pr_info("%s: IVR interrupt! STATUS5:0x%02x\n", __func__, sts);

	psy = power_supply_get_by_name("s2mu106-pmeter");
	if (!psy)
		return -EINVAL;

	ret = power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_ICHGIN, &value);
	if (ret < 0) {
		pr_err("%s: Fail to execute property\n", __func__);
		return IRQ_HANDLED;
	}

	input_curr = value.intval;
	pr_info("%s: input_curr = %d, cable_type = %d\n",
			__func__, input_curr, charger->cable_type);

	/* cable_type / status check */
	if (charger->cable_type == POWER_SUPPLY_TYPE_HV_MAINS && (sts & IVR_STATUS_MASK)) {
		input_curr = input_curr - (input_curr % 100);
		input_curr -= HV_MAINS_IVR_STEP;

		if (input_curr < HV_MAINS_IVR_INPUT)
			input_curr = HV_MAINS_IVR_INPUT;

		s2mu106_set_input_current_limit(charger, input_curr);
	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mu106_ovp_isr(int irq, void *data)
{
	struct s2mu106_charger_data *charger = data;
	u8 val;

	s2mu106_read_reg(charger->i2c, S2MU106_CHG_STATUS0, &val);
	pr_info("%s ovp %02x\n", __func__, val);

	return IRQ_HANDLED;
}

static int s2mu106_charger_parse_dt(struct device *dev, struct s2mu106_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu106-charger");
	size_t size;
	int ret = 0, len;

	if (!np)
		pr_err("%s np NULL(s2mu106-charger)\n", __func__);
	else {
		ret = of_property_read_u32(np, "battery,chg_switching_freq", &pdata->chg_switching_freq);
		if (ret < 0)
			pr_info("%s: Charger switching FRQ is Empty\n", __func__);

		ret = of_property_read_u32(np, "ERD_board", &len);
		if (ret == 0)
			pdata->erd = 1;
		else
			pdata->erd = 0;
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		unsigned int i;
		const u32 *p;

		ret = of_property_read_string(np, "battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
		if (ret < 0)
			pr_info("%s: Fuel-gauge name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_float_voltage", &pdata->chg_float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4200;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n", __func__, pdata->chg_float_voltage);

		pdata->chg_eoc_dualpath = of_property_read_bool(np, "battery,chg_eoc_dualpath");

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p) {
			pr_err("%s: battery,input_current_limit parsing fail\n", __func__);
			return 1;
		}

		len = len / sizeof(u32);

		size = sizeof(s2m_charging_current_t) * len;
		pdata->charging_current = kzalloc(size, GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np, "battery,input_current_limit", i,
							&pdata->charging_current[i].input_current_limit);
			if (ret)
				pr_info("%s: Input_current_limit is Empty\n", __func__);

			ret = of_property_read_u32_index(np, "battery,fast_charging_current", i,
							&pdata->charging_current[i].fast_charging_current);
			if (ret)
				pr_info("%s: Fast charging current is Empty\n", __func__);

			ret = of_property_read_u32_index(np, "battery,full_check_current", i,
							&pdata->charging_current[i].full_check_current);
			if (ret)
				pr_info("%s: Full check current is Empty\n", __func__);
		}
	}

	pr_info("%s DT file parsed succesfully, %d\n", __func__, ret);
	return ret;
}

/* if need to set s2mu106 pdata */
static const struct of_device_id s2mu106_charger_match_table[] = {
	{ .compatible = "samsung,s2mu106-charger",},
	{},
};

static int s2mu106_charger_probe(struct platform_device *pdev)
{
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_platform_data *pdata = dev_get_platdata(s2mu106->dev);
	struct s2mu106_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MU106 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->charger_mutex);
	charger->otg_on = false;

	charger->dev = &pdev->dev;
	charger->i2c = s2mu106->i2c;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)), GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mu106_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0) {
		pr_err("%s: s2mu106_charger_parse_dt fail\n", __func__);
		goto err_parse_dt;
	}

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "s2mu106-charger";
	if (charger->pdata->fuelgauge_name == NULL)
		charger->pdata->fuelgauge_name = "s2mu106-fuelgauge";

	charger->psy_chg_desc.name           = charger->pdata->charger_name;
	charger->psy_chg_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg_desc.get_property   = s2mu106_chg_get_property;
	charger->psy_chg_desc.set_property   = s2mu106_chg_set_property;
	charger->psy_chg_desc.properties     = s2mu106_charger_props;
	charger->psy_chg_desc.num_properties = ARRAY_SIZE(s2mu106_charger_props);

	charger->psy_otg_desc.name           = "otg";
	charger->psy_otg_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_otg_desc.get_property   = s2mu106_otg_get_property;
	charger->psy_otg_desc.set_property   = s2mu106_otg_set_property;
	charger->psy_otg_desc.properties     = s2mu106_otg_props;
	charger->psy_otg_desc.num_properties = ARRAY_SIZE(s2mu106_otg_props);

	s2mu106_chg_init(charger);
	charger->input_current = s2mu106_get_input_current_limit(charger);
	charger->charging_current = s2mu106_get_fast_charging_current(charger);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mu106_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mu106_supplied_to);

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

	charger->charger_wqueue = create_singlethread_workqueue("charger-wq");
	if (!charger->charger_wqueue) {
		pr_info("%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}

	/*
	 * irq request
	 * if you need to add irq , please refer below code.
	 */
	charger->irq_sys = pdata->irq_base + S2MU106_CHG1_IRQ_SYS;
	ret = request_threaded_irq(charger->irq_sys, NULL, s2mu106_ovp_isr, 0, "sys-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request SYS in IRQ: %d: %d\n", __func__, charger->irq_sys, ret);
		goto err_reg_irq;
	}

	charger->irq_bat = pdata->irq_base + S2MU106_CHG2_IRQ_BAT;
	ret = request_threaded_irq(charger->irq_bat, NULL, s2mu106_event_isr, 0, "bat-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request BAT in IRQ: %d: %d\n", __func__, charger->irq_bat, ret);
		goto err_reg_irq;
	}

	charger->irq_micd = pdata->irq_base + S2MU106_CHG2_IRQ_AICL;
	ret = request_threaded_irq(charger->irq_micd, NULL, s2mu106_event_isr, 0, "micd-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request MICD in IRQ: %d: %d\n", __func__, charger->irq_micd, ret);
		goto err_reg_irq;
	}

	charger->irq_ivr = pdata->irq_base + S2MU106_CHG2_IRQ_IVR;
	ret = request_threaded_irq(charger->irq_ivr, NULL, s2mu106_ivr_isr, 0, "ivr-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request IVR in IRQ: %d: %d\n", __func__, charger->irq_ivr, ret);
		goto err_reg_irq;
	}

#if IS_ENABLED(EN_BAT_DET_IRQ)
	charger->irq_det_bat = pdata->irq_base + S2MU106_CHG2_IRQ_DET_BAT;
	ret = request_threaded_irq(charger->irq_det_bat, NULL, s2mu106_det_bat_isr, 0, "det_bat-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev,
			"%s: Fail to request DET_BAT in IRQ: %d: %d\n", __func__, charger->irq_det_bat, ret);
		goto err_reg_irq;
	}
#endif

#if IS_ENABLED(EN_CHG1_IRQ_CHGIN)
	charger->irq_chgin = pdata->irq_base + S2MU106_CHG1_IRQ_CHGIN;
	ret = request_threaded_irq(charger->irq_chgin, NULL, s2mu106_chg_isr, 0, "chgin-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request CHGIN in IRQ: %d: %d\n", __func__, charger->irq_chgin, ret);
		goto err_reg_irq;
	}
#endif

	charger->irq_rst = pdata->irq_base + S2MU106_CHG1_IRQ_CHG_RSTART;
	ret = request_threaded_irq(charger->irq_rst, NULL, s2mu106_chg_isr, 0, "restart-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev,
			"%s: Fail to request CHG_Restart in IRQ: %d: %d\n", __func__, charger->irq_rst, ret);
		goto err_reg_irq;
	}

	charger->irq_done = pdata->irq_base + S2MU106_CHG1_IRQ_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL, s2mu106_done_isr, 0, "done-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev, "%s: Fail to request DONE in IRQ: %d: %d\n", __func__, charger->irq_done, ret);
		goto err_reg_irq;
	}

	charger->irq_chg_fault = pdata->irq_base + S2MU106_CHG1_IRQ_CHG_Fault;
	ret = request_threaded_irq(charger->irq_chg_fault, NULL, s2mu106_event_isr, 0, "chg_fault-irq", charger);
	if (ret < 0) {
		dev_err(s2mu106->dev,
			"%s: Fail to request CHG_Fault in IRQ: %d: %d\n", __func__, charger->irq_chg_fault, ret);
		goto err_reg_irq;
	}

	INIT_DELAYED_WORK(&charger->otg_vbus_work, s2mu106_charger_otg_vbus_work);

	/* Do max charging by freq. change, when duty is max */
	s2mu106_update_reg(charger->i2c, 0x7A, 0x1 << 4, 0x1 << 4);
#if IS_ENABLED(EN_TEST_READ)
	s2mu106_test_read(charger->i2c);
#endif
	pr_info("%s:[BATT] S2MU106 charger driver loaded OK\n", __func__);

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
	kfree(charger);
	return ret;
}

static int s2mu106_charger_remove(struct platform_device *pdev)
{
	struct s2mu106_charger_data *charger = platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mu106_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mu106_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mu106_charger_suspend NULL
#define s2mu106_charger_resume NULL
#endif

static void s2mu106_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MU106 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mu106_charger_pm_ops, s2mu106_charger_suspend, s2mu106_charger_resume);

static struct platform_driver s2mu106_charger_driver = {
	.driver         = {
		.name		= "s2mu106-charger",
		.owner		= THIS_MODULE,
		.of_match_table = s2mu106_charger_match_table,
		.pm		= &s2mu106_charger_pm_ops,
		.shutdown	= s2mu106_charger_shutdown,
	},
	.probe		= s2mu106_charger_probe,
	.remove		= s2mu106_charger_remove,
};

static int __init s2mu106_charger_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mu106_charger_driver);
}
module_init(s2mu106_charger_init);

static void __exit s2mu106_charger_exit(void)
{
	platform_driver_unregister(&s2mu106_charger_driver);
}
module_exit(s2mu106_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MU106");
MODULE_SOFTDEP("post: s2m_pdic_module");
