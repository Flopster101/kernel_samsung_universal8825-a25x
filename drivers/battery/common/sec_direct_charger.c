/*
 *  sec_direct_charger.c
 *  Samsung Mobile Charger Driver
 *
 *  Copyright (C) 2020 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include "sec_direct_charger.h"
#include "battery_logger.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

char *sec_direct_chg_mode_str[] = {
	"OFF", //SEC_DIRECT_CHG_MODE_DIRECT_OFF
	"CHECK_VBAT", //SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT
	"PRESET", //SEC_DIRECT_CHG_MODE_DIRECT_PRESET
	"ON_ADJUST", // SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST
	"ON", //SEC_DIRECT_CHG_MODE_DIRECT_ON
	"DONE", //SEC_DIRECT_CHG_MODE_DIRECT_DONE
	"BYPASS", //SEC_DIRECT_CHG_MODE_DIRECT_BYPASS
};

char *sec_direct_charger_mode_str[] = {
	"Buck-Off",
	"Buck-Off/Linear-On",
	"Charging-Off",
	"Pass-Through",
	"Charging-On",
	"OTG-On",
	"OTG-Off",
	"UNO-On",
	"UNO-Off",
	"UNO-Only",
	"Not-Set",
	"Max",
};

#if IS_ENABLED(CONFIG_SEC_ABC)
void sec_direct_abc_check(struct sec_direct_charger_info *charger)
{
	if ((charger->charging_source != SEC_CHARGING_SOURCE_DIRECT) ||
		!is_pd_apdo_wire_type(charger->cable_type) || !charger->now_isApdo) {
		charger->abc_dc_current_cnt = 0;

		return;
	}

	if (charger->dc_input_current < 900) {
		if (charger->abc_dc_current_cnt <= ABC_DC_CNT)
			charger->abc_dc_current_cnt++;
		if (charger->abc_dc_current_cnt == ABC_DC_CNT)
			sec_abc_send_event("MODULE=battery@WARN=dc_current");
	} else {
		charger->abc_dc_current_cnt = 0;
	}
}
#else
void sec_direct_abc_check(struct sec_direct_charger_info *charger) {}
#endif

void sec_direct_chg_monitor(struct sec_direct_charger_info *charger)
{
	int ret = 0;
	union power_supply_propval dc_state = {0, };

	dc_state.strval = "NO_CHARGING";
	ret = psy_do_property(charger->pdata->direct_charger_name, get,
		POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS, dc_state);

	if (ret < 0) {
		pr_info("%s: Failed to get dc_chg status", __func__);
	} else if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
		pr_info("%s: Src(%s), direct(%s), switching(%s), Imax(%dmA), Ichg(%dmA), dc_input(%dmA), dc_state(%s)\n",
			__func__, charger->charging_source ? "DIRECT" : "SWITCHING",
			sec_direct_charger_mode_str[charger->charger_mode_direct],
			sec_direct_charger_mode_str[charger->charger_mode_main],
			charger->input_current, charger->charging_current, charger->dc_input_current, dc_state.strval);
	}
	sec_direct_abc_check(charger);

	sb_pt_monitor(charger->pt, charger->charging_source);
}

static bool sec_direct_chg_set_direct_charge(
		struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	union power_supply_propval value = {0,};

	if (charger->ta_alert_wa) {
		psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
		charger->ta_alert_mode =  value.intval;
	}

	if (charger->charger_mode_direct == charger_mode && !(charger->dc_retry_cnt) &&
		(charger->ta_alert_mode == OCP_NONE)) {
		pr_info("%s: charger_mode is same(%s)\n", __func__,
			sec_direct_charger_mode_str[charger->charger_mode_direct]);
		return false;
	}

	pr_info("%s: charger_mode(%s->%s)\n", __func__,
		sec_direct_charger_mode_str[charger->charger_mode_direct],
		sec_direct_charger_mode_str[charger_mode]);
	charger->charger_mode_direct = charger_mode;

	if (charger_mode == SEC_BAT_CHG_MODE_CHARGING ||
		charger_mode == SEC_BAT_CHG_MODE_PASS_THROUGH)
		value.intval = true;
	else
		value.intval = false;

	psy_do_property(charger->pdata->direct_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	return true;
}

static bool sec_direct_chg_set_switching_charge(
		struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	union power_supply_propval value = {0,};

	pr_info("%s: charger_mode(%s->%s)\n", __func__,
		sec_direct_charger_mode_str[charger->charger_mode_main],
		sec_direct_charger_mode_str[charger_mode]);

	if (charger_mode == SEC_BAT_CHG_MODE_PASS_THROUGH)
		charger_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	charger->charger_mode_main = charger_mode;

	value.intval = charger_mode;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	return true;
}
static bool sec_direct_chg_check_temp(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};
	int batt_temp = 0, mix_limit = 0;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	int sub_batt_temp = 0;
#endif

	/* check mix limit */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_MIX_LIMIT, value);
	mix_limit = value.intval;
	if (mix_limit) {
		pr_info("%s:  S/C was selected! mix_limit(%d)\n", __func__, value.intval);
		return true;
	}

	if (charger->pdata->dchg_dc_in_swelling) {
		/* do not check batt temp for DC */
		return false;
	}

	value.intval = THM_INFO_BAT;
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_TEMP_CHECK_TYPE, value);
	if (value.intval) {
		/* check Tbat temperature */
		psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, value);
		batt_temp = value.intval;
		if (batt_temp <= charger->pdata->dchg_temp_low_threshold ||
				batt_temp >= charger->pdata->dchg_temp_high_threshold) {
			pr_info("%s:  S/C was selected! Tbat(%d)\n", __func__, batt_temp);
			return true;
		}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		/* check Tsub temperature */
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_SUB_TEMP, value);
		sub_batt_temp = value.intval;
		if (sub_batt_temp <= charger->pdata->dchg_temp_low_threshold ||
				sub_batt_temp >= charger->pdata->dchg_temp_high_threshold) {
			pr_info("%s:  S/C was selected! Tsub(%d)\n", __func__, sub_batt_temp);
			return true;
		}
#endif
	} else {
		pr_info("%s: Temperature Control Disabled!\n", __func__);
	}
	return false;
}

static bool sec_direct_chg_check_event(
	struct sec_direct_charger_info *charger, unsigned int current_event, unsigned int tx_retry_case)
{
	union power_supply_propval value = {0,};
	int batt_volt = 0;
	int dc_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (charger->pdata->dchg_dc_in_swelling) {
		if (current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING) {
			/* check Tbat temperature */
			psy_do_property("battery", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
			batt_volt = value.intval / 1000;
			psy_do_property(charger->pdata->direct_charger_name, get,
				POWER_SUPPLY_PROP_STATUS, value);
			dc_status = value.intval;
			if ((batt_volt >= charger->pdata->swelling_high_rechg_voltage) &&
				(dc_status != POWER_SUPPLY_STATUS_CHARGING) &&
				!charger->pdata->chgen_over_swell_rechg_vol) {
				pr_info("%s : volt(%d) rechg_voltage(%d) dc_status(%d)\n", __func__,
					batt_volt, charger->pdata->swelling_high_rechg_voltage, dc_status);
				return true;
			}
			if (charger->dc_rcp) {
				pr_info("%s : swelling and rcp(%d)\n", __func__,
					charger->dc_rcp);
				return true;
			}
		} else
			charger->dc_rcp = false;
		if (current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE)
			return true;
	} else {
		if (current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE)
			return true;
	}
	if (current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE ||
		current_event & SEC_BAT_CURRENT_EVENT_SIOP_LIMIT ||
		current_event & SEC_BAT_CURRENT_EVENT_SEND_UVDM ||
		(current_event & SEC_BAT_CURRENT_EVENT_DC_ERR && charger->ta_alert_mode == OCP_NONE))
		return true;

	if (tx_retry_case & SEC_BAT_TX_RETRY_MISALIGN ||
		tx_retry_case & SEC_BAT_TX_RETRY_OCP)
		return true;

	return false;
}

static bool sec_direct_fpdo_dc_check(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};
	int voltage = 0;

	/* Works only in FPDO DC */
	if (charger->cable_type != SEC_BATTERY_CABLE_FPDO_DC)
		return false;

	/* check fdpo dc start vbat condition */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	voltage = value.intval / 1000;
	if (voltage < charger->pdata->fpdo_dc_min_vbat) {
		pr_info("%s: FPDO DC, S/C was selected! low vbat(%dmV)\n", __func__, voltage);
		return true;
	}

	if (charger->charging_source == SEC_CHARGING_SOURCE_SWITCHING) {
		/* check fdpo dc vbat max condition */
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_MAIN, value);
		voltage = value.intval;
		if (voltage >= charger->pdata->fpdo_dc_max_main_vbat) {
			pr_info("%s: FPDO DC, S/C was selected! high main vbat(%dmV/%dmV)\n", __func__,
					voltage, charger->pdata->fpdo_dc_max_main_vbat);
			return true;
		}

		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_SUB, value);
		voltage = value.intval;
		if (voltage >= charger->pdata->fpdo_dc_max_sub_vbat) {
			pr_info("%s: FPDO DC, S/C was selected! high sub vbat(%dmV/%dmV)\n", __func__,
					voltage, charger->pdata->fpdo_dc_max_sub_vbat);
			return true;
		}
#else
		psy_do_property("battery", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
		voltage = value.intval / 1000;
		if (voltage >= charger->pdata->fpdo_dc_max_vbat) {
			pr_info("%s: FPDO DC, S/C was selected! high vbat(%dmV)\n", __func__, voltage);
			return true;
		}
#endif
	}

	/* check fpdo dc thermal condition check */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_FPDO_DC_THERMAL_CHECK, value);
	if (value.intval) {
		pr_info("%s:  S/C was selected! FPDO_DC_THERMAL_CHECK(%d)\n", __func__, value.intval);
		return true;
	}

	return false;
}

static int sec_direct_chg_check_charging_source(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};
	int ret = SEC_CHARGING_SOURCE_SWITCHING;
	int has_apdo = 0, cable_type = 0, voltage_avg = 0;
	unsigned int current_event = 0, lrp_chg_src = SEC_CHARGING_SOURCE_DIRECT, tx_retry_case = 0;
	int flash_state = 0, mst_en = 0, abnormal_ta = 0;
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	int mtk_fg_init = 0;
#endif

	pr_info("%s: dc_retry_cnt(%d)\n", __func__, charger->dc_retry_cnt);

	if (charger->dc_err) {
		if (charger->ta_alert_wa) {
			psy_do_property("battery", get,
					POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
			charger->ta_alert_mode =  value.intval;
		}

		pr_info("%s: dc_err(%d), ta_alert_mode(%d)\n", __func__, charger->dc_err, charger->ta_alert_mode);
		value.intval = SEC_BAT_CURRENT_EVENT_DC_ERR;
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_CURRENT_EVENT, value);
		if (!charger->ta_alert_wa || (charger->ta_alert_mode == OCP_NONE)) {
			pr_info("%s:  S/C was selected! ta_alert_mode(%d)\n", __func__, charger->ta_alert_mode);
			goto end_chg_src;
		}
	}
	if ((charger->charger_mode != SEC_BAT_CHG_MODE_CHARGING) &&
		(charger->charger_mode != SEC_BAT_CHG_MODE_PASS_THROUGH)) {
		pr_info("%s:  S/C was selected! charger_mode(%d)\n", __func__, charger->charger_mode);
		goto end_chg_src;
	}

#if defined(CONFIG_WIRELESS_TX_MODE)
	/* check TX enable*/
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE, value);
	charger->wc_tx_enable = value.intval;
	if (charger->wc_tx_enable) {
		pr_info("@TX_Mode %s: Source Switching charger during Tx mode\n", __func__);
		goto end_chg_src;
	}
#endif

	if (sec_direct_chg_check_temp(charger))
		goto end_chg_src;

	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_LRP_CHG_SRC, value);
	lrp_chg_src = value.intval;
	if (lrp_chg_src == SEC_CHARGING_SOURCE_SWITCHING) {
		pr_info("%s:  S/C was selected! lrp_chg_src is S/C\n", __func__);
		goto end_chg_src;
	}

	/* check current event */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_CURRENT_EVENT, value);
	current_event = value.intval;
	psy_do_property("wireless", get, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_RETRY_CASE, value);
	tx_retry_case = value.intval;
	if (sec_direct_chg_check_event(charger, current_event, tx_retry_case)) {
		pr_info("%s:  S/C was selected! current_event(0x%x), tx_retry_case(0x%x)\n",
			__func__, current_event, tx_retry_case);
		goto end_chg_src;
	}

	/* check test mode */
	if (charger->test_mode_source == SEC_CHARGING_SOURCE_SWITCHING) {
		pr_info("%s:  S/C was selected! test_mode_source(%d)\n", __func__, charger->test_mode_source);
		goto end_chg_src;
	}

	/* check apdo */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
	cable_type = value.intval;
	if (!is_pd_apdo_wire_type(charger->cable_type) || !is_pd_apdo_wire_type(cable_type)) {
		pr_info("%s:  S/C was selected! Not APDO(%d, %d)\n",
				__func__, charger->cable_type, cable_type);
		goto end_chg_src;
	}

	/* check battery->status */
	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
	charger->batt_status = value.intval;
	if (charger->batt_status == POWER_SUPPLY_STATUS_FULL ||
		charger->batt_status == POWER_SUPPLY_STATUS_NOT_CHARGING ||
		charger->batt_status == POWER_SUPPLY_STATUS_DISCHARGING) {
		pr_info("%s:  S/C was selected! battery->status(%d)\n",
				__func__, charger->batt_status);
		goto end_chg_src;
	}

	/* check charging status */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_DIRECT_HAS_APDO, value);
	has_apdo = value.intval;
	if (charger->cable_type == SEC_BATTERY_CABLE_FPDO_DC)
		has_apdo = 1;
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_FLASH_STATE, value);
	flash_state = value.intval; /* check only for MTK */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_MST_EN, value);
	mst_en = value.intval; /* check only for MTK */
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_ABNORMAL_TA, value);
	abnormal_ta = value.intval;

#if IS_ENABLED(CONFIG_MTK_CHARGER)
	psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_MTK_FG_INIT, value);
	mtk_fg_init = value.intval; /* check only for MTK */
#endif

	psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, value);
	charger->capacity = value.intval;
	if (charger->direct_chg_done || (charger->capacity >= charger->pdata->dchg_end_soc)
		|| !has_apdo || charger->store_mode || flash_state || mst_en || abnormal_ta
#if IS_ENABLED(CONFIG_MTK_CHARGER)
		|| !mtk_fg_init
#endif
		) {
		pr_info("%s:  S/C was selected! dc_done(%s), SoC(%d), has_apdo(%d) mst_en(%d) abnormal_ta(%d)\n",
				__func__, charger->direct_chg_done ? "TRUE" : "FALSE",
				charger->capacity, has_apdo, mst_en, abnormal_ta);
		goto end_chg_src;
	}

	if (charger->vbat_min_src != LOW_VBAT_OFF) {
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		voltage_avg = value.intval / 1000;
		if (voltage_avg < charger->pdata->dchg_min_vbat) {
			pr_info("%s:  S/C was selected! low vbat(%dmV)\n",
					__func__, voltage_avg);
			charger->vbat_min_src = LOW_VBAT_SET;
			goto end_chg_src;
		}
		charger->vbat_min_src = LOW_VBAT_OFF;
	}

	if (sec_direct_fpdo_dc_check(charger))
		goto end_chg_src;

	ret = SEC_CHARGING_SOURCE_DIRECT;

end_chg_src:
	if (charger->charging_source != ret) {
		store_battery_log("CHG_SRC:SOC(%d),BATT_ST(%d),VOLT_AVG(%d),CHG_MODE(%d)",
				charger->capacity, charger->batt_status, voltage_avg, charger->charger_mode);
		store_battery_log("CHG_SRC:SRC(%s),CT(%d,%d),CURR_EV(0x%x),DC_ERR(%d),TX(%d),HAS_APDO(%d),DC_DONE(%d)",
				ret ? "DIRECT" : "SWITCHING", cable_type, charger->cable_type, current_event, charger->dc_err,
				charger->wc_tx_enable, has_apdo, charger->direct_chg_done);
	}

	return sb_pt_check_chg_src(charger->pt, ret);
}

static int sec_direct_chg_set_charging_source(struct sec_direct_charger_info *charger,
		unsigned int charger_mode, int charging_source)
{
	union power_supply_propval value = {0,};

	mutex_lock(&charger->charger_mutex);
	if (charging_source == SEC_CHARGING_SOURCE_DIRECT) {
#ifdef CONFIG_IFPMIC_LIMITER
		sec_direct_chg_set_switching_charge(charger, SEC_BAT_CHG_MODE_BUCK_OFF_LINEAR_CHARGING);
#else
		sec_direct_chg_set_switching_charge(charger, SEC_BAT_CHG_MODE_BUCK_OFF);
#endif
		sec_direct_chg_set_direct_charge(charger, charger_mode);

		value.intval = SEC_INPUT_VOLTAGE_APDO;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);
	} else {
		if (charger->ta_alert_wa) {
			psy_do_property("battery", get,
					POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT, value);
			charger->ta_alert_mode =  value.intval;
		}

		/* Must Charging-off the DC charger before changing voltage */
		/* to prevent reverse-current into TA */
		sec_direct_chg_set_direct_charge(charger, SEC_BAT_CHG_MODE_CHARGING_OFF);

		if (charger->cable_type == SEC_BATTERY_CABLE_FPDO_DC &&
				charger->charging_source == SEC_CHARGING_SOURCE_DIRECT)
			msleep(100);

		value.intval = SEC_INPUT_VOLTAGE_9V;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);

		sec_direct_chg_set_switching_charge(charger, charger_mode);
	}

	charger->charging_source = charging_source;
	mutex_unlock(&charger->charger_mutex);

	return 0;
}

static void sec_direct_chg_set_charge(struct sec_direct_charger_info *charger, unsigned int charger_mode)
{
	int charging_source;

	charger->charger_mode = charger_mode;

	switch (charger->charger_mode) {
	case SEC_BAT_CHG_MODE_BUCK_OFF:
	case SEC_BAT_CHG_MODE_CHARGING_OFF:
	case SEC_BAT_CHG_MODE_PASS_THROUGH:
		charger->is_charging = false;
		break;
	case SEC_BAT_CHG_MODE_CHARGING:
		charger->is_charging = true;
		break;
	}

	charging_source = sec_direct_chg_check_charging_source(charger);
	sec_direct_chg_set_charging_source(charger, charger_mode, charging_source);
}

static void sec_direct_chg_do_dc_fullcharged(struct sec_direct_charger_info *charger) {
	int charging_source;

	pr_info("%s: called\n", __func__);
	charger->direct_chg_done = true;

	charging_source = sec_direct_chg_check_charging_source(charger);
	sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
}

static int sec_direct_chg_set_input_current(struct sec_direct_charger_info *charger,
			enum power_supply_property psp, int input_current) {
	union power_supply_propval value = {0,};

	pr_info("%s: called(%dmA)\n", __func__, input_current);

	value.intval = input_current;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, value);

	/* direct charger input current is based on charging current */
	return 0;
}

static int sec_direct_chg_set_charging_current(struct sec_direct_charger_info *charger,
			enum power_supply_property psp, int charging_current) {
	union power_supply_propval value = {0,};
	int charging_source, cable_type;

	psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);
	charger->now_isApdo = value.intval;

	psy_do_property("battery", get,
				POWER_SUPPLY_PROP_ONLINE, value);
	cable_type = value.intval;

	pr_info("%s: called(%dmA) now_isApdo(%d) cable_type(%d)\n",
		__func__, charging_current, charger->now_isApdo, cable_type);

#ifndef CONFIG_IFPMIC_LIMITER
	/* main charger */
	value.intval = charging_current;
	psy_do_property(charger->pdata->main_charger_name, set,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, value);
#endif

	charger->dc_charging_current = charging_current;
	charger->dc_input_current = charger->dc_charging_current / 2;

	charging_source = sec_direct_chg_check_charging_source(charger);
	value.intval = charger->dc_input_current;

	/* direct charger */
	if (is_pd_apdo_wire_type(cable_type)) {
		psy_do_property(charger->pdata->direct_charger_name, set,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, value);
		sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
	}

	return 0;
}

static void sec_direct_chg_set_initial_status(struct sec_direct_charger_info *charger)
{
	union power_supply_propval value = {0,};

	if (charger->dc_err) {
		value.intval = SEC_BAT_CURRENT_EVENT_DC_ERR;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_CURRENT_EVENT_CLEAR, value);
	}
	charger->direct_chg_done = false;

	charger->dc_charging_current = charger->pdata->dchg_min_current;
	charger->dc_input_current = charger->dc_charging_current / 2;
	charger->dc_err = false;
	charger->dc_retry_cnt = 0;
	charger->dc_rcp = false;
	charger->test_mode_source = SEC_CHARGING_SOURCE_DIRECT;
	charger->vbat_min_src = LOW_VBAT_NONE;
}

static int sec_direct_chg_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct sec_direct_charger_info *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value = {0,};
	int ret = 0;

	ret = sb_pt_psy_get_property(charger->pt, psp, val);
	if (ret) {
		pr_info("%s: prevent event for pt(ret = %d)", __func__, ret);
		return 0;
	}

	value.intval = val->intval;
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
			psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
		} else {
			psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		}
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (charger->charging_source == SEC_CHARGING_SOURCE_DIRECT) {
			psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
			if (value.intval == POWER_SUPPLY_EXT_HEALTH_DC_ERR) {
				charger->dc_retry_cnt++;
				if (charger->dc_retry_cnt > 2) {
					charger->dc_err = true;
				} else
					charger->dc_err = false;
			} else {
				charger->dc_err = false;
				charger->dc_retry_cnt = 0;
			}
#ifdef CONFIG_IFPMIC_LIMITER
			if (charger->charger_mode_main == SEC_BAT_CHG_MODE_BUCK_OFF_LINEAR_CHARGING) {
				psy_do_property(charger->pdata->main_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WDT_KICK, value);
			}
#endif
		} else {
			psy_do_property(charger->pdata->main_charger_name, get, psp, value);
			charger->dc_retry_cnt = 0;
		}
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT: /* get input current which was set */
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		if (is_direct_chg_mode_on(charger->direct_chg_mode)) {
			// NEED to CHECK
			val->intval = charger->input_current;
		} else {
			val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT: /* get charge current which was set */
		psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		if (is_direct_chg_mode_on(charger->direct_chg_mode)) {
			// NEED to CHECK
			val->intval = charger->charging_current;
		} else {
			val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		psy_do_property(charger->pdata->direct_charger_name, get, psp, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_do_property(charger->pdata->main_charger_name, get, ext_psp, value);
			if (is_pd_apdo_wire_type(charger->cable_type)) {
				psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
				val->intval = charger->vbat_min_src;
			} else
				val->intval = LOW_VBAT_NONE;
			sec_direct_chg_monitor(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			val->intval = charger->direct_chg_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC:
			psy_do_property(charger->pdata->main_charger_name, get,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
			if (value.intval == SEC_BAT_CHG_MODE_CHARGING)
				val->intval = true;
			else
				val->intval = false;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DONE:
			val->intval = charger->direct_chg_done;
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			ret = psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			val->strval = value.strval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE:
			val->intval = charger->test_mode_source;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE:
			psy_do_property(charger->pdata->direct_charger_name, get,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			ret = psy_do_property(charger->pdata->direct_charger_name, get, ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			ret = psy_do_property(charger->pdata->direct_charger_name, get,
				ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGER_IC_NAME:
			psy_do_property(charger->pdata->main_charger_name, get, ext_psp, value);
			pr_info("%s: CHARGER_IC_NAME: %s\n", __func__, value.strval);
			val->strval = value.strval;
			break;
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_OCP:
			ret = psy_do_property(charger->pdata->direct_charger_name, get,
				ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_DC_OP_MODE:
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VBUS:
			ret = psy_do_property(charger->pdata->direct_charger_name, get,
				ext_psp, value);
			val->intval = value.intval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGER_MODE_DIRECT:
			val->intval = charger->charger_mode_direct;
			break;
		case POWER_SUPPLY_EXT_PROP_DCHG_READ_BATP_BATN:
			ret = psy_do_property(charger->pdata->direct_charger_name, get,
				ext_psp, value);
			val->intval = value.intval;
			break;
		default:
			ret = psy_do_property(charger->pdata->main_charger_name, get, ext_psp, value);
			val->intval = value.intval;
			return ret;
		}
		break;
	default:
		ret = psy_do_property(charger->pdata->main_charger_name, get, psp, value);
		val->intval = value.intval;
		return ret;
	}

	return ret;
}

static int sec_direct_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct sec_direct_charger_info *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value = {0,};
	int prev_val;
	int ret = 0;

	ret = sb_pt_psy_set_property(charger->pt, psp, val);
	if (ret) {
		pr_info("%s: prevent event for pt(ret = %d)", __func__, ret);
		return 0;
	}

	value.intval = val->intval;
	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);
		charger->batt_status = val->intval;
		pr_info("%s: batt status(%d)\n", __func__, charger->batt_status);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		prev_val = charger->cable_type;
		charger->cable_type = val->intval;

		if (charger->cable_type == SEC_BATTERY_CABLE_NONE) {
			sec_direct_chg_set_initial_status(charger);
		}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		/* Dual Battery featured model turn on the ADC block during all charging not only DC */
		value.intval = (charger->cable_type == SEC_BATTERY_CABLE_NONE) ? 0 : 1;
		psy_do_property(charger->pdata->direct_charger_name, set,
						POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL, value);
#endif

		/* main charger */
		value.intval = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);

		/* direct charger */
		if (is_pd_apdo_wire_type(charger->cable_type)) {
			charger->direct_chg_mode = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
			value.intval = 1;
			psy_do_property(charger->pdata->direct_charger_name, set,
				psp, value);
		} else {
			value.intval = 0;
			psy_do_property(charger->pdata->direct_charger_name, set,
				psp, value);
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		charger->input_current = val->intval;
		sec_direct_chg_set_input_current(charger, psp, charger->input_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		charger->charging_current = val->intval;
		sec_direct_chg_set_charging_current(charger, psp, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		charger->float_voltage = val->intval;
		psy_do_property(charger->pdata->main_charger_name, set,
			psp, value);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			if (val->intval >= SEC_DIRECT_CHG_MODE_MAX) {
				pr_info("%s: abnormal direct_chg_mode(%d)\n", __func__, val->intval);
			} else {
				if (!charger->direct_chg_done) {
					pr_info("%s: direct_chg_mode:%s(%d)->%s(%d)\n", __func__,
						sec_direct_chg_mode_str[charger->direct_chg_mode], charger->direct_chg_mode,
						sec_direct_chg_mode_str[val->intval], val->intval);
					charger->direct_chg_mode = val->intval;
					if (charger->direct_chg_mode == SEC_DIRECT_CHG_MODE_DIRECT_OFF)
						charger->charger_mode_direct = SEC_BAT_CHG_MODE_CHARGING_OFF;
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC:
#if 0
			if (val->intval)
				sec_direct_chg_check_set_charge(charger, charger->charger_mode,
					SEC_BAT_CHG_MODE_BUCK_OFF, SEC_BAT_CHG_MODE_CHARGING);
			else
				sec_direct_chg_check_set_charge(charger, charger->charger_mode,
					SEC_BAT_CHG_MODE_CHARGING, SEC_BAT_CHG_MODE_CHARGING_OFF);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DONE:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_DIRECT_DONE(%d)\n", __func__, val->intval);
			if (val->intval)
				sec_direct_chg_do_dc_fullcharged(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			psy_do_property(charger->pdata->main_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE:
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE_MAX:
			psy_do_property(charger->pdata->direct_charger_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			psy_do_property(charger->pdata->direct_charger_name, set,
				ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CLEAR_ERR:
			/* If SRCCAP is changed by Src, clear DC err variables */
			charger->dc_err = false;
			charger->dc_retry_cnt = 0;
			break;
		case POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE(%d, %d)\n",
				__func__, val->strval[0], val->strval[1]);
			if (val->strval[0] == SEC_STORE_MODE)
				charger->store_mode = true;
			if (is_pd_apdo_wire_type(charger->cable_type)) {
				charger->test_mode_source = val->strval[1];

				if (charger->test_mode_source == SEC_CHARGING_SOURCE_DIRECT)
					charger->test_mode_source = sec_direct_chg_check_charging_source(charger);
#ifdef CONFIG_IFPMIC_LIMITER
				else
					sec_direct_chg_set_switching_charge(charger, SEC_BAT_CHG_MODE_CHARGING_OFF);
#endif
				sec_direct_chg_set_charging_source(charger, charger->charger_mode, charger->test_mode_source);
			} else {
				pr_info("%s: block to set charging_source (cable:%d, mode:%d, test:%d, store:%d)\n",
					__func__, charger->cable_type, charger->charger_mode,
					charger->test_mode_source, charger->store_mode);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_REFRESH_CHARGING_SOURCE:
			if (is_pd_apdo_wire_type(charger->cable_type)) {
				int charging_source;

				charging_source = sec_direct_chg_check_charging_source(charger);
				sec_direct_chg_set_charging_source(charger, charger->charger_mode, charging_source);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			sec_direct_chg_set_charge(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DC_INITIALIZE:
			sec_direct_chg_set_initial_status(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			ret = psy_do_property(charger->pdata->direct_charger_name, set, ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_D2D_REVERSE_VOLTAGE\n", __func__);
			psy_do_property(charger->pdata->direct_charger_name, set,
				psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_DC_OP_MODE:
		case POWER_SUPPLY_EXT_PROP_ADC_MODE:
			ret = psy_do_property(charger->pdata->direct_charger_name, set, ext_psp, value);
			break;
		case POWER_SUPPLY_EXT_PROP_OTG_VBUS_CTRL:
			pr_info("%s: OTG_CONTROL(%d)\n", __func__, val->intval);
			if (val->intval) {
				value.intval = 1000000;/* 1000mA */
				psy_do_property(charger->pdata->direct_charger_name, set,
					POWER_SUPPLY_EXT_PROP_DC_VIN_OVERCURRENT, value);
				value.intval = POWER_SUPPLY_DC_REVERSE_BYP;/* Reverse bypass mode */
				psy_do_property(charger->pdata->direct_charger_name, set,
					POWER_SUPPLY_EXT_PROP_DC_REVERSE_MODE, value);
			} else {
				value.intval = POWER_SUPPLY_DC_REVERSE_STOP;/* Stop reverse mode */
				psy_do_property(charger->pdata->direct_charger_name, set,
					POWER_SUPPLY_EXT_PROP_DC_REVERSE_MODE, value);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DC_RCP:
			charger->dc_rcp = val->intval;
			break;
 		default:
			ret = psy_do_property(charger->pdata->main_charger_name, set, ext_psp, value);
			return ret;
		}
		break;
	default:
		ret = psy_do_property(charger->pdata->main_charger_name, set, psp, value);
		return ret;
	}

	return ret;
}

#ifdef CONFIG_OF
static int sec_direct_charger_parse_dt(struct device *dev,
		struct sec_direct_charger_info *charger)
{
	struct device_node *np = dev->of_node;

	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return 1;
	}
	sb_of_parse_str_dt(np, "charger,battery_name", charger->pdata, battery_name);
	sb_of_parse_str_dt(np, "charger,main_charger", charger->pdata, main_charger_name);
	sb_of_parse_str_dt(np, "charger,direct_charger", charger->pdata, direct_charger_name);
	sb_of_parse_u32_dt(np, "charger,dchg_min_current", charger->pdata, dchg_min_current, SEC_DIRECT_CHG_MIN_IOUT);
	sb_of_parse_u32_dt(np, "charger,dchg_min_vbat", charger->pdata, dchg_min_vbat, SEC_DIRECT_CHG_MIN_VBAT);
	sb_of_parse_u32_dt(np, "charger,fpdo_dc_min_vbat", charger->pdata, fpdo_dc_min_vbat, FPDO_DC_MIN_VBAT);
	sb_of_parse_u32_dt(np, "charger,fpdo_dc_max_vbat", charger->pdata, fpdo_dc_max_vbat, FPDO_DC_MAX_VBAT);
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	sb_of_parse_u32_dt(np, "charger,fpdo_dc_max_main_vbat",
			charger->pdata, fpdo_dc_max_main_vbat, FPDO_DC_MAX_VBAT);
	sb_of_parse_u32_dt(np, "charger,fpdo_dc_max_sub_vbat", charger->pdata, fpdo_dc_max_sub_vbat, FPDO_DC_MAX_VBAT);
#endif
	sb_of_parse_u32_dt(np, "charger,end_soc", charger->pdata, dchg_end_soc, 95);
	sb_of_parse_bool_dt(np, "charger,ta_alert_wa", charger, ta_alert_wa);

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}
	sb_of_parse_bool_dt(np, "battery,dchg_dc_in_swelling", charger->pdata, dchg_dc_in_swelling);
	sb_of_parse_u32_dt(np, "battery,wire_normal_warm_thresh",
					charger->pdata, dchg_temp_high_threshold, 420);
	sb_of_parse_u32_dt(np, "battery,wire_cool1_normal_thresh",
					charger->pdata, dchg_temp_low_threshold, 180);
	sb_of_parse_u32_dt(np, "battery,swelling_high_rechg_voltage",
					charger->pdata, swelling_high_rechg_voltage, 4050);
	sb_of_parse_bool_dt(np, "battery,chgen_over_swell_rechg_vol", charger->pdata, chgen_over_swell_rechg_vol);

	return 0;
}
#else
static int sec_direct_charger_parse_dt(struct device *dev,
		struct sec_direct_charger_info *charger)
{
	return 0;
}
#endif /* CONFIG_OF */

static enum power_supply_property sec_direct_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sec_direct_charger_power_supply_desc = {
	.name = "sec-direct-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_direct_charger_props,
	.num_properties = ARRAY_SIZE(sec_direct_charger_props),
	.get_property = sec_direct_chg_get_property,
	.set_property = sec_direct_chg_set_property,
};

static int sec_direct_charger_probe(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger;
	struct sec_direct_charger_platform_data *pdata = NULL;
	struct power_supply_config direct_charger_cfg = {};
	int ret = 0;

	pr_info("%s: SEC Direct-Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_direct_charger_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_charger_free;
		}

		charger->pdata = pdata;
		if (sec_direct_charger_parse_dt(&pdev->dev, charger)) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec-direct-charger dt\n", __func__);
			ret = -EINVAL;
			goto err_pdata_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		charger->pdata = pdata;
	}

	/* init direct charger variables */
	charger->direct_chg_done = false;
	charger->direct_chg_mode = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
	charger->cable_type = SEC_BATTERY_CABLE_NONE;

	charger->charger_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	charger->charger_mode_direct = SEC_BAT_CHG_MODE_CHARGING_OFF;
	charger->charger_mode_main = SEC_BAT_CHG_MODE_CHARGING_OFF;
	charger->test_mode_source = SEC_CHARGING_SOURCE_DIRECT;

	charger->wc_tx_enable = false;
	charger->now_isApdo = false;
	charger->store_mode = false;
	charger->vbat_min_src = LOW_VBAT_NONE;
#if IS_ENABLED(CONFIG_SEC_ABC)
	charger->abc_dc_current_cnt = 0;
#endif

	platform_set_drvdata(pdev, charger);
	charger->dev = &pdev->dev;
	direct_charger_cfg.drv_data = charger;
	charger->ta_alert_mode = OCP_NONE;

	mutex_init(&charger->charger_mutex);

	charger->pt = sb_pt_init(charger->dev);
	if (IS_ERR(charger->pt)) {
		ret = PTR_ERR(charger->pt);
		dev_info(charger->dev, "%s: unused pass through (ret = %d)\n", __func__, ret);
		charger->pt = NULL;
	}

	charger->psy_chg = power_supply_register(&pdev->dev,
			&sec_direct_charger_power_supply_desc, &direct_charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		ret = PTR_ERR(charger->psy_chg);
		dev_err(charger->dev,
			"%s: Failed to Register psy_chg(%d)\n", __func__, ret);
		goto err_power_supply_register;
	}
	sec_chg_set_dev_init(SC_DEV_SEC_DIR_CHG);

	pr_info("%s: SEC Direct-Charger Driver Loaded(%s, %s)\n",
		__func__, charger->pdata->main_charger_name, charger->pdata->direct_charger_name);
	return 0;

err_power_supply_register:
	mutex_destroy(&charger->charger_mutex);
err_pdata_free:
	kfree(pdata);
err_charger_free:
	kfree(charger);

	return ret;
}

static int sec_direct_charger_remove(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->charger_mutex);

	dev_dbg(charger->dev, "%s: End\n", __func__);

	kfree(charger->pdata);
	kfree(charger);

	pr_info("%s: --\n", __func__);

	return 0;
}

static int sec_direct_charger_suspend(struct device *dev)
{
	return 0;
}

static int sec_direct_charger_resume(struct device *dev)
{
	return 0;
}

static void sec_direct_charger_shutdown(struct platform_device *pdev)
{
	struct sec_direct_charger_info *charger = platform_get_drvdata(pdev);
	union power_supply_propval value = {0,};

	pr_info("%s: ++\n", __func__);

	value.intval = false;
	psy_do_property(charger->pdata->direct_charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);

	value.intval = SEC_INPUT_VOLTAGE_5V;
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);

	pr_info("%s: --\n", __func__);
}

#ifdef CONFIG_OF
static struct of_device_id sec_direct_charger_dt_ids[] = {
	{ .compatible = "samsung,sec-direct-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_direct_charger_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_direct_charger_pm_ops = {
	.suspend = sec_direct_charger_suspend,
	.resume = sec_direct_charger_resume,
};

static struct platform_driver sec_direct_charger_driver = {
	.driver = {
		.name = "sec-direct-charger",
		.owner = THIS_MODULE,
		.pm = &sec_direct_charger_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_direct_charger_dt_ids,
#endif
	},
	.probe = sec_direct_charger_probe,
	.remove = sec_direct_charger_remove,
	.shutdown = sec_direct_charger_shutdown,
};

static int __init sec_direct_charger_init(void)
{
	pr_info("%s: \n", __func__);
	return platform_driver_register(&sec_direct_charger_driver);
}

static void __exit sec_direct_charger_exit(void)
{
	platform_driver_unregister(&sec_direct_charger_driver);
}

device_initcall_sync(sec_direct_charger_init);
module_exit(sec_direct_charger_exit);

MODULE_DESCRIPTION("Samsung Direct Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
