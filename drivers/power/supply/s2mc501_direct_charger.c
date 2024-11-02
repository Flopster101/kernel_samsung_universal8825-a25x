/*
 * s2mc501_direct_charger.c - S2MC501 Charger Driver
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
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
#include <linux/power/s2mc501_direct_charger.h>
#include <linux/power/s2m_chg_manager.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/ifconn/ifconn_notifier.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

static char *s2mc501_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mc501_dc_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

extern int s2mc501_pmeter_init(struct s2mc501_dc_data *charger);
static int s2mc501_dc_is_enable(struct s2mc501_dc_data *charger);
static void s2mc501_dc_enable(struct s2mc501_dc_data *charger, int onoff);
static void s2mc501_dc_pm_enable(struct s2mc501_dc_data *charger);
static int s2mc501_dc_get_float_voltage(struct s2mc501_dc_data *charger);
static void s2mc501_dc_set_float_voltage(struct s2mc501_dc_data *charger, int float_voltage);
static int s2mc501_dc_get_input_current(struct s2mc501_dc_data *charger);
static void s2mc501_dc_set_input_current(struct s2mc501_dc_data *charger, int input_current);
//static void s2mc501_dc_set_charging_current(struct s2mc501_dc_data *charger,
//				int charging_current, s2mc501_dc_cc_mode mode);
static int s2mc501_dc_irq_clear(struct s2mc501_dc_data *charger);
static void s2mc501_dc_state_manager(struct s2mc501_dc_data *charger, s2mc501_dc_trans_t trans);
static int s2mc501_dc_get_pmeter(struct s2mc501_dc_data *charger, s2mc501_pmeter_id_t id);
//static void s2mc501_dc_forced_enable(struct s2mc501_dc_data *charger, int onoff);
//static void s2mc501_dc_set_target_curr(struct s2mc501_dc_data *charger);
//static int s2mc501_dc_send_fenced_pps(struct s2mc501_dc_data *charger);
//static int s2mc501_dc_get_fg_value(struct s2mc501_dc_data *charger, enum power_supply_property prop);
static void s2mc501_dc_set_irq_unmask(struct s2mc501_dc_data *charger);
static void s2mc501_dc_state_trans_enqueue(struct s2mc501_dc_data *charger, s2mc501_dc_trans_t trans);
//static void s2mc501_dc_set_sc_prop(struct s2mc501_dc_data *charger, enum power_supply_property psp, int val);
//static int s2mc501_dc_send_verify(struct s2mc501_dc_data *charger);
//static void s2mc501_dc_refresh_auto_pps(struct s2mc501_dc_data *charger, unsigned int vol, unsigned int iin);
//static void s2mc501_dc_vchgin_compensation(struct s2mc501_dc_data *charger);

int s2mc501_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mc501_dc_data *s2mc501 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mc501->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mc501->i2c_lock);
	if (ret < 0) {
		pr_err("%s : reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mc501_read_reg);

int s2mc501_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mc501_dc_data *s2mc501 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mc501->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mc501->i2c_lock);
	if (ret < 0)
		pr_err("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mc501_write_reg);

int s2mc501_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mc501_dc_data *s2mc501 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&s2mc501->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mc501->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mc501_update_reg);

static const char *pmeter_to_str(s2mc501_pmeter_id_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(PMETER_ID_VDCIN, ret);
	CONV_STR(PMETER_ID_VOUT, ret);
	CONV_STR(PMETER_ID_VCELL, ret);
	CONV_STR(PMETER_ID_IIN, ret);
	CONV_STR(PMETER_ID_IINREV, ret);
	CONV_STR(PMETER_ID_TDIE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *state_to_str(s2mc501_dc_state_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_STATE_OFF, ret);
	CONV_STR(DC_STATE_CHECK_VBAT, ret);
	CONV_STR(DC_STATE_PRESET, ret);
//	CONV_STR(DC_STATE_START_CC, ret);
	CONV_STR(DC_STATE_CC, ret);
//	CONV_STR(DC_STATE_SLEEP_CC, ret);
//	CONV_STR(DC_STATE_ADJUSTED_CC, ret);
	CONV_STR(DC_STATE_CV, ret);
//	CONV_STR(DC_STATE_WAKEUP_CC, ret);
	CONV_STR(DC_STATE_DONE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *trans_to_str(s2mc501_dc_trans_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_TRANS_INVALID, ret);
	CONV_STR(DC_TRANS_NO_COND, ret);
	CONV_STR(DC_TRANS_VBATT_RETRY, ret);
	CONV_STR(DC_TRANS_WAKEUP_DONE, ret);
	CONV_STR(DC_TRANS_RAMPUP, ret);
	CONV_STR(DC_TRANS_RAMPUP_FINISHED, ret);
	CONV_STR(DC_TRANS_CHG_ENABLED, ret);
	CONV_STR(DC_TRANS_BATTERY_OK, ret);
	CONV_STR(DC_TRANS_BATTERY_NG, ret);
	CONV_STR(DC_TRANS_CHGIN_OKB_INT, ret);
	CONV_STR(DC_TRANS_CC_WAKEUP, ret);
	CONV_STR(DC_TRANS_BAT_OKB_INT, ret);
	CONV_STR(DC_TRANS_NORMAL_CHG_INT, ret);
	CONV_STR(DC_TRANS_DC_DONE_INT, ret);
	CONV_STR(DC_TRANS_CC_STABLED, ret);
	CONV_STR(DC_TRANS_DETACH, ret);
	CONV_STR(DC_TRANS_FAIL_INT, ret);
	CONV_STR(DC_TRANS_FLOAT_VBATT, ret);
	CONV_STR(DC_TRANS_RAMPUP_OVER_VBATT, ret);
	CONV_STR(DC_TRANS_RAMPUP_OVER_CURRENT, ret);
	CONV_STR(DC_TRANS_SET_CURRENT_MAX, ret);
	CONV_STR(DC_TRANS_TOP_OFF_CURRENT, ret);
	CONV_STR(DC_TRANS_DC_OFF_INT, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *prop_to_str(enum power_supply_property n)
{
	char *ret;
	switch (n) {
	CONV_STR(POWER_SUPPLY_PROP_VOLTAGE_AVG, ret);
	CONV_STR(POWER_SUPPLY_PROP_CURRENT_AVG, ret);
	CONV_STR(POWER_SUPPLY_PROP_CURRENT_NOW, ret);
	CONV_STR(POWER_SUPPLY_PROP_CAPACITY, ret);
	default:
		return "invalid";
	}
	return ret;
}

static void s2mc501_dc_test_read(struct i2c_client *i2c)
{
	static int reg_list[] = {
		0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
		0x11, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x2A, 0x30, 0x3A,
		0x3B, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
		0x5A, 0x5B, 0x5C, 0x70, 0x71, 0xF5
	};

	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
                s2mc501_read_reg(i2c, reg_list[i], &data);
                sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
        }

	/* print buffer */
	pr_info("[DC]%s: %s\n", __func__, str);

}

static int s2mc501_dc_is_enable(struct s2mc501_dc_data *charger)
{
	/* TODO : DC status? check */
	u8 data;
	int ret;

	ret = s2mc501_read_reg(charger->i2c, S2MC501_DC_ENABLE, &data);
	if (ret < 0)
		return ret;

	return (data & DC_EN_MASK) ? 1 : 0;
}

static void s2mc501_dc_enable(struct s2mc501_dc_data *charger, int onoff)
{
	if (onoff > 0) {
		pr_info("%s, direct charger enable\n", __func__);
		__pm_stay_awake(charger->ws);
		charger->is_charging = true;
		s2mc501_update_reg(charger->i2c, S2MC501_DC_ENABLE, DC_EN_MASK, DC_EN_MASK);
	} else {
		pr_info("%s, direct charger disable\n", __func__);
		s2mc501_update_reg(charger->i2c, S2MC501_DC_ENABLE, 0, DC_EN_MASK);
		charger->is_charging = false;
	}
}

static void s2mc501_dc_pm_enable(struct s2mc501_dc_data *charger)
{
#if 0
	union power_supply_propval value;

        value.intval = PM_TYPE_VCHGIN | PM_TYPE_VBAT | PM_TYPE_ICHGIN | PM_TYPE_TDIE |
                PM_TYPE_VWCIN | PM_TYPE_IWCIN | PM_TYPE_VBYP | PM_TYPE_VSYS |
                PM_TYPE_VCC1 | PM_TYPE_VCC2 | PM_TYPE_ICHGIN | PM_TYPE_ITX;
	psy_do_property(charger->pdata->pm_name, set,
				POWER_SUPPLY_PROP_CO_ENABLE, value);
#endif
}
#if 0
static int s2mc501_dc_get_topoff_current(struct s2mc501_dc_data *charger)
{
	u8 data = 0x00;
	int ret, topoff_current = 0;

	ret = s2mc501_read_reg(charger->i2c, S2MC501_DC_CTRL15, &data);
	if (ret < 0)
		return ret;
	data = (data & DC_TOPOFF_MASK) >> DC_TOPOFF_SHIFT;

	if (data > 0x0F)
		data = 0x0F;

	topoff_current = data * 100 + 500;
	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	return topoff_current;
}

static void s2mc501_dc_set_topoff_current(struct s2mc501_dc_data *charger, int topoff_current)
{
	u8 data = 0x00;

	if (topoff_current <= 500)
		data = 0x00;
	else if (topoff_current > 500 && topoff_current <= 2000)
		data = (topoff_current - 500) / 100;
	else
		data = 0x0F;

	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL15,
				data << DC_TOPOFF_SHIFT, DC_TOPOFF_MASK);
}
#endif
static int s2mc501_dc_get_float_voltage(struct s2mc501_dc_data *charger)
{
	u8 data = 0x00;
	int ret, float_voltage = 0;

	ret = s2mc501_read_reg(charger->i2c, S2MC501_DC_CTRL1, &data);
	if (ret < 0)
		return data;

	data = data & SET_CV_MASK;

	if (data >= 0x7F)
		float_voltage = 4870;
	else if (data <= 0x00)
		float_voltage = 3600;
	else
		float_voltage = data * 10 + 3600;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);

	return float_voltage;
}

static void s2mc501_dc_set_float_voltage(struct s2mc501_dc_data *charger, int float_voltage)
{
	u8 data = 0x00;

	if (float_voltage <= 3600)
		data = 0x00;
	else if (float_voltage > 3600 && float_voltage <= 4870)
		data = (float_voltage - 3600) / 10;
	else
		data = 0x7F;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);
	charger->floatVol = float_voltage;

	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL1,
				data << SET_CV_SHIFT, SET_CV_MASK);
}

static int s2mc501_dc_get_input_current(struct s2mc501_dc_data *charger)
{
	u8 data = 0x00;
	int ret, input_current = 0;

	ret = s2mc501_read_reg(charger->i2c, S2MC501_DC_CTRL0, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_CHGIN_ICHG_MASK;

	input_current = data * 50 + 50;

	pr_info("%s,input_current : %d(0x%2x)\n", __func__, input_current, data);
	return input_current;
}

static void s2mc501_dc_set_input_current(struct s2mc501_dc_data *charger, int input_current)
{
	u8 data = 0x00;
	data = (input_current - 50) / 50;
	if (data >= 0x3F)
		data = 0x3F;
	pr_info("%s, input_current : %d(0x%2x)\n", __func__, input_current, data);

	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL0,
				data << DC_SET_CHGIN_ICHG_SHIFT, DC_SET_CHGIN_ICHG_MASK);
}

static int s2mc501_dc_get_rcp(struct s2mc501_dc_data *charger)
{
	u8 data = 0x00;
	int ret, rcp = 0;

	ret = s2mc501_read_reg(charger->i2c, S2MC501_DC_CTRL2, &data);
	if (ret < 0)
		return data;
	data = data & DC_RCP_MASK;

	if (data == 0x00)
		rcp = 100;
	else if (data <= 0x07)
		rcp = (data * 500) + 500;

	pr_info("%s, RCP : %dmA(0x%2x)\n", __func__, rcp, data);
	return rcp;
}

static void s2mc501_dc_set_rcp(struct s2mc501_dc_data *charger, int r_current)
{
	u8 data = 0x00;

	if (r_current <= 100)
		data = 0x00;
	else if (r_current > 4000)
		data = 0x07;
	else
		data = (r_current - 500) / 500 ;

	pr_info("%s, RCP : %d(0x%2x)\n", __func__, r_current, data);

	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL2,
				data << DC_RCP_SHIFT, DC_RCP_MASK);
}

static void s2mc501_dc_set_irq_unmask(struct s2mc501_dc_data *charger)
{
	/* TODO : IRQ unmask */
	s2mc501_write_reg(charger->i2c, 0x04, 0x00);
	s2mc501_write_reg(charger->i2c, 0x05, 0x00);
	s2mc501_write_reg(charger->i2c, 0x06, 0x00);
}

static int s2mc501_dc_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mc501_dc_data *charger = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
	union power_supply_propval value;
	int ret = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = charger->dc_chg_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		/* TODO : health check */
		pr_info("%s: health check & wdt clear\n", __func__);
		//s2mc501_dc_wdt_clear(charger);
//		if (charger->ps_health == POWER_SUPPLY_HEALTH_UNKNOWN)
//		charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
		val->intval = charger->ps_health;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = s2mc501_dc_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* FG temp? PM temp? */
		if (!charger->psy_fg)
			return -EINVAL;
		ret = power_supply_get_property(charger->psy_fg, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->step_iin_ma;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->step_iin_ma * 2;
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED:
			val->intval = s2mc501_dc_is_enable(charger);
			break;
		default:
			ret = -ENODATA;
		}
		return ret;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mc501_dc_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mc501_dc_data *charger = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;
	union power_supply_propval value;
	int ret = 0;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		/*
		 * todo :
		 * need to change top off current
		 * 1000 -> 500 mA by IIN.
		 */
		if (val->intval <= 1000)
			charger->step_iin_ma = 1050;
		else
			charger->step_iin_ma = val->intval;
		pr_info("%s, set iin : %d", __func__, charger->step_iin_ma);
		s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_SET_CURRENT_MAX);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Not Taking charging current orders from platform */
		//charger->charging_current = val->intval;
		//s2mc501_dc_set_charging_current(charger, val->intval);
		break;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED:
			if (val->intval == S2M_BAT_CHG_MODE_CHARGING_OFF) {
				charger->is_charging = false;
				if (charger->dc_state != DC_STATE_OFF)
					s2mc501_dc_state_manager(charger, DC_TRANS_DETACH);
			} else {
				if (charger->cable_type && (!charger->is_charging)) {
					charger->is_charging = true;
					charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
					s2mc501_dc_state_manager(charger, DC_TRANS_CHG_ENABLED);
				}
			}
			value.intval = charger->is_charging;
			power_supply_set_property(charger->psy_fg,
                (enum power_supply_property)s2m_psp, &value);
			break;
		default:
			ret = -ENODATA;
		}
		return ret;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mc501_dc_cal_target_value(struct s2mc501_dc_data *charger)
{
	unsigned int vbat;
	unsigned int temp;
//	union power_supply_propval value;

//	power_supply_get_property(charger->psy_fg, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
//	vbat = value.intval;

	vbat = s2mc501_dc_get_pmeter(charger, PMETER_ID_VOUT);

	temp = vbat * 2;
	charger->ppsVol = ((temp / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV) + 350;

//	temp = vbat * 2 * (1031);
//	charger->ppsVol = (((temp / 1000) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV) + 200 + (charger->vchgin_okb_retry * DC_TA_VOL_STEP_MV);

	if (charger->ppsVol > charger->pd_data->taMaxVol) {
		charger->ppsVol = charger->pd_data->taMaxVol;
	}

	pr_info("%s ppsVol : %d\n", __func__, charger->ppsVol);
}

static int s2mc501_dc_get_fg_value(struct s2mc501_dc_data *charger, enum power_supply_property prop)
{
	union power_supply_propval value;
	u32 get = 0;
	int ret;

	if (!charger->psy_fg)
		return -EINVAL;
	ret = power_supply_get_property(charger->psy_fg, prop, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	get = value.intval;
	pr_info("%s, %s : (%d)\n", __func__, prop_to_str(prop), get);
	return get;
}

static int s2mc501_dc_get_pmeter(struct s2mc501_dc_data *charger, s2mc501_pmeter_id_t id)
{
	struct power_supply *psy_pm;
	union power_supply_propval value;
	int ret = 0;

	psy_pm = power_supply_get_by_name(charger->pdata->pm_name);
	if (!psy_pm)
		return -EINVAL;

	switch (id) {
		case PMETER_ID_VDCIN:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_VDCIN, &value);
			break;
		case PMETER_ID_VOUT:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_VOUT, &value);
			break;
		case PMETER_ID_VCELL:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_VCELL, &value);
			break;
		case PMETER_ID_IIN:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_IIN, &value);
			break;
		case PMETER_ID_IINREV:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_IINREV, &value);
			break;
		case PMETER_ID_TDIE:
			ret = power_supply_get_property(psy_pm, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_TDIE, &value);
			break;
		default:
			break;
	}

	if (ret < 0) {
		pr_err("%s: fail to set power_suppy pmeter property(%d)\n",
				__func__, ret);
	} else {
		charger->pmeter[id] = value.intval;
		pr_info("%s pm[%s] : %d\n", __func__, pmeter_to_str(id), value.intval);
		return value.intval;
	}
	return -1;
}

#if 0
static void s2mc501_dc_vchgin_compensation(struct s2mc501_dc_data *charger)
{
	int vdiff;
	int vchgin = s2mc501_dc_get_pmeter(charger, PMETER_ID_VCHGIN);

	pr_info("%s enter, pps vol : %d, vchgin : %d\n", __func__, charger->ppsVol, vchgin);
	if (vchgin > charger->ppsVol + 150) {
		vdiff = ((vchgin - charger->ppsVol) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV;
		charger->ppsVol -= vdiff;
		s2mc501_dc_send_verify(charger);
	} else if (vchgin < charger->ppsVol - 150) {
		vdiff = ((charger->ppsVol - vchgin) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV;
		charger->ppsVol += vdiff;
		s2mc501_dc_send_verify(charger);
	} else {
		pr_info("%s ok, pps vol : %d, vchgin : %d\n", __func__, charger->ppsVol, vchgin);
	}
	s2mc501_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
}

static void s2mc501_dc_set_target_curr(struct s2mc501_dc_data *charger)
{
	if (charger->step_iin_ma > charger->pd_data->taMaxCur)
		charger->step_iin_ma = charger->pd_data->taMaxCur;

	pr_info("%s step_iin_ma : %d\n", __func__, charger->step_iin_ma);
	charger->targetIIN = ((charger->step_iin_ma / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA) + DC_TA_CURR_STEP_MA;
	s2mc501_dc_set_charging_current(charger, (charger->targetIIN * 2), S2MC501_DC_MODE_TA_CC);
	return;
}

static void s2mc501_dc_refresh_auto_pps(struct s2mc501_dc_data *charger, unsigned int vol, unsigned int iin)
{
	mutex_lock(&charger->auto_pps_mutex);
	charger->ppsVol = vol;
	charger->targetIIN = iin;

	pr_info("%s vol : %d, iin : %d\n", __func__, vol, iin);
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MC501_DC_DISABLE);

	if (s2mc501_dc_delay(charger, 500))
		goto CANCEL_AUTO_PPS;

	charger->ppsVol = vol;
	charger->targetIIN = iin;
	s2mc501_dc_send_fenced_pps(charger);

	if (s2mc501_dc_delay(charger, 500))
		goto CANCEL_AUTO_PPS;

	charger->is_autopps_disabled = false;
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MC501_DC_EN);
CANCEL_AUTO_PPS:
	mutex_unlock(&charger->auto_pps_mutex);
}

#endif
#define DC_THRESH	1050


static void s2mc501_dc_platform_state_update(struct s2mc501_dc_data *charger)
{
        union power_supply_propval value;
        struct power_supply *psy;
        int state = -1, ret;

        psy = power_supply_get_by_name("dc-manager");
        if (!psy) {
                pr_err("%s, can't access power_supply", __func__);
                return;
        }
        switch (charger->dc_state) {
        case DC_STATE_OFF:
                state = S2M_DIRECT_CHG_MODE_OFF;
                break;
        case DC_STATE_CHECK_VBAT:
                state = S2M_DIRECT_CHG_MODE_CHECK_VBAT;
                break;
        case DC_STATE_PRESET:
                state = S2M_DIRECT_CHG_MODE_PRESET;
                break;
        case DC_STATE_CC:
		case DC_STATE_CV:
                state = S2M_DIRECT_CHG_MODE_ON;
				break;
        case DC_STATE_DONE:
                state = S2M_DIRECT_CHG_MODE_DONE;
                break;
        default:
                break;
        }

        if (state < 0) {
                pr_err("%s: Invalid state report.\n", __func__);
                return;
        }

        pr_info("%s, updated state : %d(%d)\n", __func__, state, charger->dc_state);
        ret = power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_DIRECT_CHARGER_MODE, &value);
        if (ret < 0)
                pr_err("%s: Fail to execute property\n", __func__);

        if (value.intval != state) {
                value.intval = state;
                ret = power_supply_set_property(psy,
                        (enum power_supply_property)POWER_SUPPLY_S2M_PROP_DIRECT_CHARGER_MODE, &value);
        }

}

static void s2mc501_dc_state_manager(struct s2mc501_dc_data *charger, s2mc501_dc_trans_t trans)
{
	s2mc501_dc_state_t next_state = DC_STATE_OFF;
	mutex_lock(&charger->dc_state_mutex);
	__pm_stay_awake(charger->state_manager_ws);

	pr_info("%s: %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans));

	if (trans == DC_TRANS_DETACH || trans == DC_TRANS_FAIL_INT || trans == DC_TRANS_DC_OFF_INT) {
		if (charger->dc_state != DC_STATE_OFF) {
			next_state = DC_STATE_OFF;
			goto HNDL_DC_OFF;
		} else {
			goto SKIP_HNDL_DC_OFF;
		}
	}

	if (trans == DC_TRANS_DC_DONE_INT || trans == DC_TRANS_TOP_OFF_CURRENT) {
		if (charger->dc_state != DC_STATE_DONE) {
			next_state = DC_STATE_DONE;
			goto HNDL_DC_OFF;
		} else {
			goto SKIP_HNDL_DC_OFF;
		}
	}

	switch (charger->dc_state) {
	case DC_STATE_OFF:
		if (trans == DC_TRANS_CHG_ENABLED)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_CHECK_VBAT:
		if (trans == DC_TRANS_BATTERY_OK)
			next_state = DC_STATE_PRESET;
		else if (trans == DC_TRANS_BATTERY_NG)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_PRESET:
		if (trans == DC_TRANS_NORMAL_CHG_INT)
			next_state = DC_STATE_CC;
		else if (trans == DC_TRANS_BAT_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else if (trans == DC_TRANS_CHGIN_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_CC:
		if (trans == DC_TRANS_FLOAT_VBATT)
			next_state = DC_STATE_CV;
		else
			goto ERR;
		break;
	case DC_STATE_CV:
		if (trans == DC_TRANS_SET_CURRENT_MAX)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_DONE:
		break;
	default:
		break;
	}

HNDL_DC_OFF:
	pr_info("%s: %s --> %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans),
		state_to_str(next_state));

	/* Trigger state function */
	charger->dc_state = next_state;

	/* State update */
	schedule_delayed_work(&charger->update_wqueue, 0);

	charger->dc_state_fp[next_state]((void *)charger);
	__pm_relax(charger->state_manager_ws);
	mutex_unlock(&charger->dc_state_mutex);

	return;

SKIP_HNDL_DC_OFF:
	pr_info("%s: %s --> %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans),
		state_to_str(next_state));

	/* Trigger state function */
	charger->dc_state = next_state;

	/* State update */
	schedule_delayed_work(&charger->update_wqueue, 0);

	msleep(50);
	__pm_relax(charger->state_manager_ws);
	mutex_unlock(&charger->dc_state_mutex);

	return;

ERR:
	pr_err("%s err occured, state now : %s\n", __func__,
		state_to_str(charger->dc_state));
	__pm_relax(charger->state_manager_ws);
	mutex_unlock(&charger->dc_state_mutex);

	return;
}

static void s2mc501_dc_state_trans_enqueue(struct s2mc501_dc_data *charger, s2mc501_dc_trans_t trans)
{
	mutex_lock(&charger->trans_mutex);
	charger->dc_trans = trans;
	schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
	msleep(50);
	mutex_unlock(&charger->trans_mutex);
}

static void s2mc501_dc_state_check_vbat(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	unsigned int vbat, soc, vcell;

	soc = s2mc501_dc_get_fg_value(charger, POWER_SUPPLY_PROP_CAPACITY);
	vbat = s2mc501_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_AVG);
	vcell = s2mc501_dc_get_pmeter(charger, PMETER_ID_VCELL);

	msleep(100);
	select_pdo(1);
	msleep(100);

	soc = soc / 10;

	pr_info("%s, soc : %d, vbat : %d, vcell %d\n", __func__, soc, vbat, vcell);

	if (vbat > (DC_MIN_VBAT + charger->chk_vbat_margin) && soc < DC_MAX_SOC) {
		if (charger->dc_sc_status == S2MC501_DC_SC_STATUS_CHARGE) {
			charger->dc_sc_status = S2MC501_DC_SC_STATUS_OFF;
		}
		pr_info("%s, soc : %d, vbat : %d, vcell %d\n", __func__, soc, vbat, vcell);
		s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_BATTERY_OK);
	} else {
		charger->dc_sc_status = S2MC501_DC_SC_STATUS_CHARGE;
//		schedule_delayed_work(&charger->check_vbat_wqueue, msecs_to_jiffies(1000));
	}

	s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_BATTERY_OK);
	s2mc501_dc_test_read(charger->i2c);
	return;
}

static void s2mc501_dc_state_preset(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	union power_supply_propval value;

	unsigned int pdo_pos = 0;
	unsigned int taMaxVol = 4400 * 2;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;
	int ret = 0;

	pr_info("%s, start\n", __func__);

	s2mc501_dc_test_read(charger->i2c);
	/* get APDO */
	pd_get_apdo_max_power(&pdo_pos, &taMaxVol, &taMaxCur, &taMaxPwr);
	charger->pd_data->pdo_pos	= pdo_pos;
	charger->pd_data->taMaxVol	= taMaxVol - 700;
	charger->pd_data->taMaxCur	= taMaxCur;
	charger->pd_data->taMaxPwr 	= taMaxPwr;

	/* Determine the Initial Voltage Here */
	s2mc501_dc_cal_target_value(charger);
	charger->minIIN		= DC_TA_MIN_PPS_CURRENT;
	charger->adjustIIN 	= ((charger->minIIN / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA) + DC_TA_CURR_STEP_MA;
	select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);

	value.intval = 1;
	ret = power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_EN_STEP_CC, &value);

	/* ENB_CC, ENB_CV */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_OTP7,
			DC_ENB_CC | DC_ENB_CV, DC_ENB_CC | DC_ENB_CV);

	/* RCP setting */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL2,
			DC_RCP_100mA << DC_RCP_SHIFT, DC_RCP_MASK);

	msleep(200);

	/* PPS Setting */
	pd_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->adjustIIN, S2MC501_DC_EN);

	/* RCP setting */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_CTRL2,
			DC_RCP_100mA << DC_RCP_SHIFT, DC_RCP_MASK);

	/* RCP_DEB setting */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_OTP8, DC_OTP_RCP_DEB, DC_OTP_RCP_DEB);

	s2mc501_dc_irq_clear(charger);

	/* DC CCR */
	s2mc501_dc_set_input_current(charger, 3000);

	/* Auto PPS*/
	value.intval = 1;
	ret = power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START, &value);

	/* DC enable */
	s2mc501_dc_enable(charger, 1);

	/* CV */
	value.intval = DC_CV_4400mV;
	ret = power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_DC_CV, &value);

	s2mc501_dc_test_read(charger->i2c);
	pr_info("%s End\n", __func__);

	s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_NORMAL_CHG_INT);
	return;
}

static void s2mc501_dc_state_cc(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	pr_info("%s\n", __func__);
	charger->dc_chg_status = POWER_SUPPLY_STATUS_CHARGING;
	s2mc501_dc_test_read(charger->i2c);
	return;
}

static void s2mc501_dc_state_cv(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	pr_info("%s\n", __func__);
	s2mc501_dc_test_read(charger->i2c);
}

static void s2mc501_dc_state_done(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	struct	power_supply *psy;
	union power_supply_propval value;
	pr_info("%s\n", __func__);
	charger->is_charging = false;
	charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (charger->pd_data->pdo_pos) {
		pd_pps_enable(charger->pd_data->pdo_pos,
			9000, charger->targetIIN, 0);
		charger->pd_data->pdo_pos = 0;

	}

	/* Auto PPS*/
	value.intval = 0;
	power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START, &value);

	s2mc501_dc_enable(charger, 0);

	psy = power_supply_get_by_name("dc-manager");
	if (!psy) {
		pr_err("%s, can't access power_supply", __func__);
		return;
	}

	value.intval = 1;
    power_supply_set_property(psy,
    	(enum power_supply_property)POWER_SUPPLY_S2M_PROP_DIRECT_CHARGE_DONE, &value);

	s2mc501_dc_test_read(charger->i2c);
}

static void s2mc501_dc_state_off(void *data)
{
	struct s2mc501_dc_data *charger = (struct s2mc501_dc_data *)data;
	union power_supply_propval value;

	pr_info("%s\n", __func__);
	charger->is_charging = false;
	charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (charger->pd_data->pdo_pos) {
		pd_pps_enable(charger->pd_data->pdo_pos,
			9000, charger->targetIIN, 0);
		charger->pd_data->pdo_pos = 0;

	}

	s2mc501_dc_enable(charger, 0);
	/* Auto PPS*/
	value.intval = 0;
	power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START, &value);

	s2mc501_dc_test_read(charger->i2c);
}
#if 0
static int s2mc501_dc_send_fenced_pps(struct s2mc501_dc_data *charger)
{
	if (charger->ppsVol > charger->pd_data->taMaxVol)
		charger->ppsVol = charger->pd_data->taMaxVol;

	if (charger->targetIIN > charger->pd_data->taMaxCur)
		charger->targetIIN = charger->pd_data->taMaxCur;

	if (charger->targetIIN < charger->minIIN)
		charger->targetIIN = charger->minIIN;

	pr_info("%s vol : %d, curr : %d, duration : %ld\n",
		__func__, charger->ppsVol, charger->targetIIN,
		s2mc501_dc_update_time_chk(charger));
	if (charger->is_charging)
		sec_pd_select_pps(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN);
	else
		return 0;

	s2mc501_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mc501_dc_get_pmeter(charger, PMETER_ID_IWCIN);

	return 1;
}

static int s2mc501_dc_send_verify(struct s2mc501_dc_data *charger)
{
	int wait_ret = 0;
	s2mc501_dc_send_fenced_pps(charger);

	charger->is_pps_ready = false;
	wait_ret = wait_event_interruptible_timeout(charger->wait,
			charger->is_pps_ready,
			msecs_to_jiffies(500));
	pr_info("%s, wait %d ms.\n", __func__, wait_ret);
	return wait_ret;
}
#endif

static void s2mc501_dc_state_manager_work(struct work_struct *work)
{
	struct s2mc501_dc_data *charger = container_of(work, struct s2mc501_dc_data,
						 state_manager_wqueue.work);

	pr_info("%s\n", __func__);
	s2mc501_dc_state_manager(charger, charger->dc_trans);
}

static int s2mc501_dc_irq_clear(struct s2mc501_dc_data *charger)
{
	u8 data[3];

	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT0, &data[0]);
	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT1, &data[1]);
	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT2, &data[2]);

	return 0;
}

static void s2mc501_isr_work(struct work_struct *work)
{
	struct s2mc501_dc_data *charger = container_of(work, struct s2mc501_dc_data, isr_work.work);
	u8 data[3];
	struct	power_supply *psy;
	union power_supply_propval value;

	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT0, &data[0]);
	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT1, &data[1]);
	s2mc501_read_reg(charger->i2c, S2MC501_DC_INT2, &data[2]);
	pr_info("%s : %02x, %02x, %02x\n", __func__, data[0], data[1], data[2]);


	if ((data[0] & DC_INT0_IN2OUTOVP) || (data[1] & DC_INT2_CFLY_OVP) ||
			(data[0] & DC_INT0_IN2OUTFAIL) || (data[0] & DC_INT0_DCOUTFAIL) ||
			(data[0] & DC_INT0_CLSHORT) || (data[0] & DC_INT0_FC_P_UVLO) ||
			(data[0] & DC_INT0_VOUTOVP) || (data[0] & DC_INT0_TSD) ||
			(data[1] & DC_INT1_DCINOVP) || (data[1] & DC_INT1_INPUTOCP) ||
			(data[2] & DC_INT2_RCP) || (data[2] & DC_INT2_CLSHORT) ||
			(data[2] & DC_INT2_CFLY_OVP)) {

		charger->ps_health = POWER_SUPPLY_S2M_HEALTH_DC_ERR;
		charger->is_charging = false;

		psy = power_supply_get_by_name("dc-manager");
		if (!psy) {
			pr_err("%s, can't access power_supply", __func__);
			return;
		}

		value.intval = charger->ps_health;
		power_supply_set_property(psy,
				(enum power_supply_property) POWER_SUPPLY_PROP_HEALTH, &value);

		s2mc501_dc_state_manager(charger, DC_TRANS_FAIL_INT);
	}
	else if (data[1] & DC_INT1_CHARGING) {
		pr_info("%s Normal Charger detected\n", __func__);
		usleep_range(10000,11000);
		s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_NORMAL_CHG_INT);
		goto out;
	} else if (data[0] & DC_INT0_PLUGOUT) {
		s2mc501_dc_state_trans_enqueue(charger, DC_TRANS_DC_OFF_INT);
		goto out;
	}

	s2mc501_dc_test_read(charger->i2c);
out:
	mutex_unlock(&charger->irq_lock);
}

static irqreturn_t s2mc501_irq_thread(int irq, void *irq_data)
{
	struct s2mc501_dc_data *charger = irq_data;

	mutex_lock(&charger->irq_lock);
	schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static void s2mc501_dc_update_work(struct work_struct *work)
{
	struct s2mc501_dc_data *charger = container_of(work, struct s2mc501_dc_data, update_wqueue.work);
	s2mc501_dc_platform_state_update(charger);
}

static int s2mc501_dc_irq_init(struct s2mc501_dc_data *charger)
{
	int ret = 0;
	pr_info("%s: s2mc501 irq = %d\n", __func__, charger->irq_gpio);

	if (charger->pdata->irq_gpio > 0) {
		charger->irq_gpio = gpio_to_irq(charger->pdata->irq_gpio);
		pr_info("%s: s2mc501 irq = %d\n", __func__, charger->irq_gpio);
		if (charger->irq_gpio > 0) {
			ret = request_threaded_irq(charger->irq_gpio,
					NULL, s2mc501_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"s2mc501-irq", charger);
			if (ret) {
				pr_err("%s: Failed to Request IRQ\n", __func__);

			}

			ret = enable_irq_wake(charger->irq_gpio);
			if (ret < 0)
				pr_err("%s: Failed to Enable Wakeup Source(%d)\n", __func__, ret);
		} else {
			pr_err("%s: Failed gpio_to_irq(%d)\n", __func__, charger->irq_gpio);
		}
	}

	s2mc501_write_reg(charger->i2c, S2MC501_DC_INT0_MASK, 0x00);
	s2mc501_write_reg(charger->i2c, S2MC501_DC_INT1_MASK, 0x00);
	s2mc501_write_reg(charger->i2c, S2MC501_DC_INT2_MASK, 0x00);
	s2mc501_dc_set_irq_unmask(charger);

	INIT_DELAYED_WORK(&charger->isr_work, s2mc501_isr_work);

	return 0;
}


static bool s2mc501_dc_init(struct s2mc501_dc_data *charger)
{
	int float_voltage, input_current, rev_current;

	/* enable */
	gpio_request(charger->pdata->enable_gpio, "s2mc501_enable");
	gpio_direction_output(charger->pdata->enable_gpio, 1);
	msleep(500);

	/* DC EN */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_CHIP_SET,
					DC_EN_MASK, DC_EN_MASK);

	msleep(20);
	/* DC CLK EN */
	s2mc501_update_reg(charger->i2c, S2MC501_DC_CHIP_SET,
					SYSCLK_EN_MASK, SYSCLK_EN_MASK);
	msleep(500);

	s2mc501_update_reg(charger->i2c, S2MC501_CD_OTP0, 0x00, 0x84);

	float_voltage = s2mc501_dc_get_float_voltage(charger);
	input_current = s2mc501_dc_get_input_current(charger);
	rev_current	= s2mc501_dc_get_rcp(charger);

	s2mc501_dc_set_float_voltage(charger, float_voltage);
	s2mc501_dc_set_input_current(charger, input_current);
	s2mc501_dc_set_rcp(charger, rev_current);
	s2mc501_dc_is_enable(charger);
	s2mc501_dc_pm_enable(charger);
	s2mc501_dc_enable(charger, 0);

	charger->dc_state = DC_STATE_OFF;
	charger->dc_state_fp[DC_STATE_OFF]	 	= s2mc501_dc_state_off;
	charger->dc_state_fp[DC_STATE_CHECK_VBAT] 	= s2mc501_dc_state_check_vbat;
	charger->dc_state_fp[DC_STATE_PRESET] 		= s2mc501_dc_state_preset;
	charger->dc_state_fp[DC_STATE_CC] 		= s2mc501_dc_state_cc;
	charger->dc_state_fp[DC_STATE_CV]		= s2mc501_dc_state_cv;
	charger->dc_state_fp[DC_STATE_DONE] 		= s2mc501_dc_state_done;
	charger->vchgin_okb_retry = 0;

	charger->is_pps_ready = false;
	charger->rise_speed_dly_ms = 10;
	charger->step_vbatt_mv = 4150;
	charger->step_iin_ma = 2000;
	charger->step_cv_cnt = 0;
	charger->mon_chk_time = 0;
	charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	charger->dc_sc_status = S2MC501_DC_SC_STATUS_OFF;
	charger->chk_vbat_margin = 0;
	charger->is_rampup_adjusted = false;
	charger->is_plugout_mask = false;
	charger->is_autopps_disabled = false;
	charger->chk_vbat_charged = 0;
	charger->chk_vbat_prev = 0;
	charger->chk_iin_prev = 0;

	return true;
}

static int s2mc501_dc_parse_dt(struct device *dev,
		struct s2mc501_dc_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mc501-direct-charger");
	int ret = 0;

	if (!np) {
		pr_err("%s np NULL(s2mc501-direct-charger)\n", __func__);
		ret = -1;
		goto err;
	}

	ret = of_property_read_string(np, "dc,direct_charger_name",
				(char const **)&pdata->dc_name);
	if (ret < 0)
		pr_info("%s: Direct Charger name is Empty\n", __func__);

	ret = of_property_read_u32(np, "dc,input_current_limit",
			&pdata->input_current_limit);
	if (ret) {
		pr_info("%s: dc,input_current_limit Empty default 4000\n", __func__);
		pdata->input_current_limit = DC_MAX_INPUT_CURRENT_MA;
	}

	ret = of_property_read_u32(np, "dc,topoff_current",
			&pdata->topoff_current);
	if (ret) {
		pr_info("%s: dc,topoff_current Empty default 2000\n", __func__);
		pdata->topoff_current = DC_TOPOFF_CURRENT_MA;
	}

	ret = of_get_named_gpio(np, "s2mc501,irq-gpio", 0);
	if (!gpio_is_valid(ret))
		return ret;
	pdata->irq_gpio = ret;

	ret = of_get_named_gpio(np, "s2mc501,enable-gpio", 0);
	if (!gpio_is_valid(ret))
		return ret;
	pdata->enable_gpio = ret;

	pr_info("%s DT file parsed succesfully\n", __func__);
	return 0;

err:
	pr_info("%s, direct charger parsing failed\n", __func__);
	return -1;
}

/* if need to set s2mc501 pdata */
static const struct of_device_id s2mc501_direct_charger_match_table[] = {
	{ .compatible = "samsung,s2mc501-direct-charger",},
	{},
};

static int s2mc501_direct_charger_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct s2mc501_dc_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	union power_supply_propval value;
	s2mc501_dc_pd_data_t *pd_data;

	pr_info("%s: S2MC501 Direct Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	pd_data = kzalloc(sizeof(*pd_data), GFP_KERNEL);
	if (unlikely(!pd_data)) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_pd_data_alloc;
	}
	charger->pd_data = pd_data;
	charger->i2c = client;

	mutex_init(&charger->dc_state_mutex);
	mutex_init(&charger->dc_mutex);
	mutex_init(&charger->i2c_lock);
	mutex_init(&charger->irq_lock);
	mutex_init(&charger->trans_mutex);
//	battery_wakeup_source_init(&client->dev, &charger->state_manager_ws, "state_man_wake");

	charger->dev = &client->dev;
	charger->i2c = client;
//	charger->rev_id = s2mc501->pmic_rev;

	if (client->dev.of_node) {
		charger->pdata = devm_kzalloc(&client->dev, sizeof(*(charger->pdata)), GFP_KERNEL);
		if (!charger->pdata) {
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}

		ret = s2mc501_dc_parse_dt(&client->dev, charger->pdata);
		if (ret < 0) {
			dev_err(&client->dev,
				"%s: Failed to get direct charger dt\n", __func__);
			ret = -EINVAL;
			goto err_parse_dt;
		}
	} else {
		charger->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, charger);

	if (charger->pdata->dc_name == NULL)
		charger->pdata->dc_name = "s2mc501-direct-charger";

	if (charger->pdata->pm_name == NULL)
		charger->pdata->pm_name = "s2mc501-pmeter";

	if (charger->pdata->fg_name == NULL)
		charger->pdata->fg_name = "s2mf301-fuelgauge";

	if (charger->pdata->sc_name == NULL)
		charger->pdata->sc_name = "s2mf301-charger";

	if (charger->pdata->sec_dc_name == NULL)
		charger->pdata->sec_dc_name = "dc-manager";

	if (charger->pdata->top_name == NULL)
		charger->pdata->top_name = "s2mf301-top";

	charger->psy_dc_desc.name           = charger->pdata->dc_name;
	charger->psy_dc_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_dc_desc.get_property   = s2mc501_dc_get_property;
	charger->psy_dc_desc.set_property   = s2mc501_dc_set_property;
	charger->psy_dc_desc.properties     = s2mc501_dc_props;
	charger->psy_dc_desc.num_properties = ARRAY_SIZE(s2mc501_dc_props);

	s2mc501_dc_init(charger);
	s2mc501_pmeter_init(charger);

	charger->psy_pmeter = power_supply_get_by_name(charger->pdata->pm_name);
	if (!charger->psy_pmeter) {
		pr_err("%s: Fail to get pmeter\n", __func__);
		goto err_get_pmeter;
	}

	charger->psy_top = power_supply_get_by_name(charger->pdata->top_name);
	if (!charger->psy_top) {
		pr_err("%s: Fail to get top\n", __func__);
		goto err_get_pmeter;
	}

	/* Auto PPS*/
	value.intval = 0;
	power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START, &value);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mc501_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mc501_supplied_to);

	charger->psy_dc = power_supply_register(&client->dev, &charger->psy_dc_desc, &psy_cfg);
	if (IS_ERR(charger->psy_dc)) {
		pr_err("%s: Failed to Register psy_dc\n", __func__);
		ret = PTR_ERR(charger->psy_dc);
		goto err_power_supply_register;
	}

	charger->psy_fg = power_supply_get_by_name(charger->pdata->fg_name);
	if (!charger->psy_fg)
		goto err_get_psy;

	charger->psy_sc = power_supply_get_by_name(charger->pdata->sc_name);
	if (!charger->psy_sc)
		goto err_get_psy;

	INIT_DELAYED_WORK(&charger->state_manager_wqueue, s2mc501_dc_state_manager_work);
	INIT_DELAYED_WORK(&charger->update_wqueue, s2mc501_dc_update_work);
	s2mc501_dc_irq_init(charger);

#if EN_TEST_READ
	s2mc501_dc_test_read(charger->i2c);
#endif
//	ret = s2mc501_dc_create_attrs(&charger->psy_dc->dev);
//	if (ret) {
//		dev_err(charger->dev,
//			"%s : Failed to create_attrs\n", __func__);
//	}
	pr_info("%s: S2MC501 Direct Charger driver loaded OK\n", __func__);

	return 0;

#if defined(BRINGUP)
err_create_wq:
#endif
err_get_psy:
	power_supply_unregister(charger->psy_dc);
err_power_supply_register:
err_get_pmeter:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->i2c_lock);
	mutex_destroy(&charger->irq_lock);
	wakeup_source_unregister(charger->state_manager_ws);
	kfree(charger->pd_data);
err_pd_data_alloc:
	kfree(charger);
	return ret;
}

static int s2mc501_direct_charger_remove(struct i2c_client *client)
{
	struct s2mc501_dc_data *charger = i2c_get_clientdata(client);

	power_supply_unregister(charger->psy_dc);
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->i2c_lock);
	mutex_destroy(&charger->irq_lock);
	wakeup_source_unregister(charger->state_manager_ws);
	kfree(charger->pd_data);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mc501_direct_charger_prepare(struct device *dev)
{
	return 0;
}

static int s2mc501_direct_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mc501_direct_charger_resume(struct device *dev)
{
	return 0;
}

static void s2mc501_direct_charger_complete(struct device *dev)
{
	return;
}

#else
#define s2mc501_direct_charger_suspend NULL
#define s2mc501_direct_charger_resume NULL
#endif

static void s2mc501_direct_charger_shutdown(struct i2c_client *client)
{
	struct s2mc501_dc_data *charger = i2c_get_clientdata(client);
	union power_supply_propval value;
	pr_info("%s: S2MC501 Direct Charger driver shutdown\n", __func__);
	if (charger->pd_data->pdo_pos) {
		pd_pps_enable(charger->pd_data->pdo_pos,
			5000, charger->targetIIN, 0);
		charger->pd_data->pdo_pos = 0;

	}

	s2mc501_dc_enable(charger, 0);
	/* Auto PPS*/
	value.intval = 0;
	power_supply_set_property(charger->psy_top,
			(enum power_supply_property)POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START, &value);
}

static const struct dev_pm_ops s2mc501_direct_charger_pm_ops = {
	.prepare = s2mc501_direct_charger_prepare,
	.suspend = s2mc501_direct_charger_suspend,
	.resume = s2mc501_direct_charger_resume,
	.complete = s2mc501_direct_charger_complete,
};

static struct i2c_driver s2mc501_direct_charger_driver = {
	.driver         = {
		.name   = "s2mc501-direct-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mc501_direct_charger_match_table,
		.pm     = &s2mc501_direct_charger_pm_ops,
	},
	.probe          = s2mc501_direct_charger_probe,
	.remove		= s2mc501_direct_charger_remove,
	.shutdown   =   s2mc501_direct_charger_shutdown,
};

static int __init s2mc501_direct_charger_init(void)
{
	int ret = 0;
	pr_info("%s start\n", __func__);
	ret = i2c_add_driver(&s2mc501_direct_charger_driver);

	return ret;
}
module_init(s2mc501_direct_charger_init);

static void __exit s2mc501_direct_charger_exit(void)
{
	i2c_del_driver(&s2mc501_direct_charger_driver);
}
module_exit(s2mc501_direct_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Charger driver for S2MC501");
MODULE_SOFTDEP("pre: i2c-exynos5 post: s2m_chg_manager");
