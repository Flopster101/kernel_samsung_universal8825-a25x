/*
 *  sec_step_charging.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"

#define STEP_CHARGING_CONDITION_VOLTAGE			0x01
#define STEP_CHARGING_CONDITION_SOC				0x02
#define STEP_CHARGING_CONDITION_CHARGE_POWER 	0x04
#define STEP_CHARGING_CONDITION_ONLINE 			0x08
#define STEP_CHARGING_CONDITION_CURRENT_NOW		0x10
#define STEP_CHARGING_CONDITION_FLOAT_VOLTAGE	0x20
#define STEP_CHARGING_CONDITION_INPUT_CURRENT		0x40
#define STEP_CHARGING_CONDITION_SOC_INIT_ONLY		0x80 /* use this to consider SOC to decide starting step only */
#define STEP_CHARGING_CONDITION_FORCE_SOC		0x100
#define STEP_CHARGING_CONDITION_FG_CURRENT		0x200

#define STEP_CHARGING_CONDITION_DC_INIT		(STEP_CHARGING_CONDITION_VOLTAGE | STEP_CHARGING_CONDITION_SOC | STEP_CHARGING_CONDITION_SOC_INIT_ONLY)

#define DIRECT_CHARGING_FLOAT_VOLTAGE_MARGIN		20
#define DIRECT_CHARGING_FORCE_SOC_MARGIN			10

void sec_bat_reset_step_charging(struct sec_battery_info *battery)
{
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval val = {0, };

	if (battery->pdata->step_chg_vsublim) {
		val.intval = battery->pdata->chg_float_voltage - battery->pdata->limiter_aging_float_offset;
		pr_info("%s: SUB limiter chg voltage = %d\n", __func__, val.intval);
		psy_do_property(battery->pdata->sub_limiter_name, set,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
	}
#endif
	pr_info("%s\n", __func__);
	battery->step_chg_status = -1;
#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
	battery->wpc_step_chg_status = -1;
#endif
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	battery->dc_float_voltage_set = false;
#endif
}
EXPORT_SYMBOL(sec_bat_reset_step_charging);

void sec_bat_exit_step_charging(struct sec_battery_info *battery)
{
	sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, false, 0);
	if (battery->step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
		sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, false, 0);
	sec_bat_reset_step_charging(battery);
}
EXPORT_SYMBOL(sec_bat_exit_step_charging);

void sec_bat_exit_wpc_step_charging(struct sec_battery_info *battery)
{
	sec_vote(battery->fcc_vote, VOTER_WPC_STEP_CHARGE, false, 0);
	if (battery->step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
		sec_vote(battery->fv_vote, VOTER_WPC_STEP_CHARGE, false, 0);
	sec_bat_reset_step_charging(battery);
}
EXPORT_SYMBOL(sec_bat_exit_wpc_step_charging);

/*
 * true: step is changed
 * false: not changed
 */
bool sec_bat_check_step_charging(struct sec_battery_info *battery)
{
	int i = 0, value = 0, step_condition = 0, lcd_status = 0;
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	int value_sub = 0, step_condition_sub = 0;
#endif
	static int curr_cnt;
	static bool skip_lcd_on_changed;
	int age_step = battery->pdata->age_step;
	union power_supply_propval val = {0, };
	int fpdo_sc = 0;

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
	if (is_wireless_all_type(battery->cable_type)) {
		sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, false, 0);
		sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, false, 0);
		return false;
	}
#endif
#if defined(CONFIG_SEC_FACTORY)
	if (!battery->step_chg_en_in_factory)
		return false;
#endif
	if (!battery->step_chg_type)
		return false;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	if (battery->test_charge_current)
		return false;
	if (battery->test_step_condition <= 4500)
		battery->pdata->step_chg_cond[0][0] = battery->test_step_condition;
#endif

	if (battery->siop_level < 100 || battery->lcd_status)
		lcd_status = 1;
	else
		lcd_status = 0;

	if (battery->cable_type == SEC_BATTERY_CABLE_FPDO_DC) {
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
		fpdo_sc = val.intval;
		pr_info("%s: SC for FPDO_DC(%d)", __func__, fpdo_sc);

		if (!fpdo_sc && battery->step_chg_status >= 0)
			sec_bat_reset_step_charging(battery);
	}

	if (battery->step_chg_type & STEP_CHARGING_CONDITION_ONLINE) {
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		if ((is_pd_apdo_wire_type(battery->cable_type) && !fpdo_sc) &&
			!((battery->current_event & SEC_BAT_CURRENT_EVENT_DC_ERR) &&
			(battery->ta_alert_mode == OCP_NONE))) {
			sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, false, 0);
			sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, false, 0);
			return false;
		}

		if (((is_pd_apdo_wire_type(battery->cable_type) || is_pd_apdo_wire_type(battery->wire_status)) &&
					!fpdo_sc) &&
			(battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL3)) {
			pr_info("%s: This cable type should be checked in dc step check\n", __func__);
			sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, false, 0);
			sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, false, 0);
			return false;
		}
#endif
		if (!is_hv_wire_type(battery->cable_type) && !is_pd_wire_type(battery->cable_type) &&
			(battery->sink_status.rp_currentlvl != RP_CURRENT_LEVEL3)) {
			sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, false, 0);
			sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, false, 0);
			return false;
		}
	}

	pr_info("%s\n", __func__);

	if (battery->step_chg_type & STEP_CHARGING_CONDITION_CHARGE_POWER) {
		if (battery->max_charge_power < battery->step_chg_charge_power) {
			/* In case of max_charge_power falling by AICL during step-charging ongoing */
			sec_bat_exit_step_charging(battery);
			return false;
		}
	}

	if (battery->step_charging_skip_lcd_on && lcd_status) {
		if (!skip_lcd_on_changed) {
			if (battery->step_chg_status != (battery->step_chg_step - 1)) {
				sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, true,
					battery->pdata->step_chg_curr[age_step][battery->step_chg_step - 1]);

				if (battery->step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
					pr_info("%s : float voltage = %d\n", __func__,
						battery->pdata->step_chg_vfloat[age_step][battery->step_chg_step - 1]);
					sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, true,
						battery->pdata->step_chg_vfloat[age_step][battery->step_chg_step - 1]);
				}
				pr_info("%s : skip step charging because lcd on\n", __func__);
				skip_lcd_on_changed = true;
				return true;
			}
		}
		return false;
	}

	if (battery->step_chg_status < 0) {
		i = 0;

		/* this is only for step enter condition and do not use STEP_CHARGING_CONDITION_SOC at the same time */
		if (battery->step_chg_type & STEP_CHARGING_CONDITION_SOC_INIT_ONLY) {
			int soc_condition;

			value = battery->capacity;
			while (i < battery->step_chg_step - 1) {
				soc_condition = battery->pdata->step_chg_cond_soc[age_step][i];
				if (value < soc_condition)
					break;
				i++;
			}

			pr_info("%s : set initial step(%d) by soc\n", __func__, i);
			goto check_step_change;
		}
	} else
		i = battery->step_chg_status;

	step_condition = battery->pdata->step_chg_cond[age_step][i];

	if (battery->step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		step_condition_sub = battery->pdata->step_chg_cond_sub[age_step][i];
		value = battery->voltage_avg_main;
		value_sub = battery->voltage_avg_sub;
#else
		value = battery->voltage_avg;
#endif
	} else if (battery->step_chg_type & STEP_CHARGING_CONDITION_SOC) {
		value = battery->capacity;
		if (lcd_status) {
			step_condition = battery->pdata->step_chg_cond[age_step][i] + 15;
			curr_cnt = 0;
		}
	} else {
		return false;
	}

	while (i < battery->step_chg_step - 1) {
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		if (battery->step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
			if ((value < step_condition) && (value_sub < step_condition_sub))
				break;
		} else {
			if (value < step_condition)
				break;
		}
#else
		if (value < step_condition)
			break;
#endif
		i++;

		if ((battery->step_chg_type & STEP_CHARGING_CONDITION_SOC) &&
			lcd_status)
			step_condition = battery->pdata->step_chg_cond[age_step][i] + 15;
		else {
			step_condition = battery->pdata->step_chg_cond[age_step][i];
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
			if (battery->step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE)
				step_condition_sub = battery->pdata->step_chg_cond_sub[age_step][i];
#endif
		}
		if (battery->step_chg_status != -1)
			break;
	}

check_step_change:
	if ((i != battery->step_chg_status) || skip_lcd_on_changed) {
		/* this is only for no consuming current */
		if ((battery->step_chg_type & STEP_CHARGING_CONDITION_CURRENT_NOW) &&
			!lcd_status &&
			battery->step_chg_status >= 0) {
			int condition_curr;
			condition_curr = max(battery->current_avg, battery->current_now);
			if (condition_curr < battery->pdata->step_chg_cond_curr[battery->step_chg_status]) {
				curr_cnt++;
				pr_info("%s : cnt = %d, curr(%d)mA < curr cond(%d)mA\n",
					__func__, curr_cnt, condition_curr,
					battery->pdata->step_chg_cond_curr[battery->step_chg_status]);
				if (curr_cnt < 3)
					return false;
			} else {
				pr_info("%s : clear cnt, curr(%d)mA >= curr cond(%d)mA or < 0mA\n",
					__func__, condition_curr,
					battery->pdata->step_chg_cond_curr[battery->step_chg_status]);
				curr_cnt = 0;
				return false;
			}
		}

		pr_info("%s : prev=%d, new=%d, value=%d, current=%d, curr_cnt=%d\n", __func__,
			battery->step_chg_status, i, value,
			battery->pdata->step_chg_curr[age_step][i], curr_cnt);
		battery->step_chg_status = i;
		skip_lcd_on_changed = false;
		sec_vote(battery->fcc_vote, VOTER_STEP_CHARGE, true,
			battery->pdata->step_chg_curr[age_step][i]);

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		if (battery->pdata->step_chg_vsublim) {
			val.intval = battery->pdata->chg_float_voltage - battery->pdata->limiter_aging_float_offset;

			if (battery->step_chg_status != battery->step_chg_step - 1)
				val.intval = battery->pdata->step_chg_vsublim;

			pr_info("%s: step(%d), SUB limiter chg voltage = %d\n",
					__func__, battery->step_chg_status, val.intval);

			psy_do_property(battery->pdata->sub_limiter_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
		}
#endif

		if (battery->step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
			pr_info("%s: float voltage = %d\n", __func__,
				battery->pdata->step_chg_vfloat[age_step][i]);
			sec_vote(battery->fv_vote, VOTER_STEP_CHARGE, true,
				battery->pdata->step_chg_vfloat[age_step][i]);
		}
		return true;
	}
	return false;
}
EXPORT_SYMBOL(sec_bat_check_step_charging);

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
bool sec_bat_check_wpc_step_charging(struct sec_battery_info *battery)
{
	int i = 0, value = 0, step_condition = 0, lcd_status = 0;
	static int curr_cnt;
	static bool skip_lcd_on_changed;
	int age_step = battery->pdata->age_step;

#if defined(CONFIG_SEC_FACTORY)
	if (!battery->step_chg_en_in_factory)
		return false;
#endif
	if (!battery->wpc_step_chg_type)
		return false;
	if (is_not_wireless_type(battery->cable_type)) {
		sec_vote(battery->fv_vote, VOTER_WPC_STEP_CHARGE, false, 0);
		sec_vote(battery->fcc_vote, VOTER_WPC_STEP_CHARGE, false, 0);

		return false;
	}
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	if (battery->test_charge_current)
		return false;
	if (battery->test_step_condition <= 4500)
		battery->pdata->wpc_step_chg_cond[0][0] = battery->test_step_condition;
#endif

	if (battery->siop_level < 100 || battery->lcd_status)
		lcd_status = 1;
	else
		lcd_status = 0;

	if (battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_CHARGE_POWER) {
		if (battery->max_charge_power < battery->wpc_step_chg_charge_power) {
			/* In case of max_charge_power falling by AICL during step-charging ongoing */
			sec_bat_exit_wpc_step_charging(battery);
			return false;
		}
	}

	if (battery->step_charging_skip_lcd_on && lcd_status) {
		if (!skip_lcd_on_changed) {
			if (battery->wpc_step_chg_status != (battery->wpc_step_chg_step - 1)) {
				sec_vote(battery->fcc_vote, VOTER_WPC_STEP_CHARGE, true,
					battery->pdata->wpc_step_chg_curr[age_step][battery->wpc_step_chg_step - 1]);

				if (battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
					pr_info("%s : float voltage = %d\n", __func__,
						battery->pdata->wpc_step_chg_vfloat[age_step][battery->wpc_step_chg_step - 1]);
					sec_vote(battery->fv_vote, VOTER_WPC_STEP_CHARGE, true,
						battery->pdata->wpc_step_chg_vfloat[age_step][battery->wpc_step_chg_step - 1]);
				}
				pr_info("%s : skip step charging because lcd on\n", __func__);
				skip_lcd_on_changed = true;
				return true;
			}
		}
		return false;
	}

	if (battery->wpc_step_chg_status < 0)
		i = 0;
	else
		i = battery->wpc_step_chg_status;

	step_condition = battery->pdata->wpc_step_chg_cond[age_step][i];

	if (battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
		value = battery->voltage_avg;
	} else if (battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_SOC) {
		value = battery->capacity;
		if (lcd_status) {
			step_condition = battery->pdata->wpc_step_chg_cond[age_step][i] + 15;
			curr_cnt = 0;
		}
	} else {
		return false;
	}

	while (i < battery->wpc_step_chg_step - 1) {
		if (value < step_condition)
			break;
		i++;

		if ((battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_SOC) &&
			lcd_status)
			step_condition = battery->pdata->wpc_step_chg_cond[age_step][i] + 15;
		else {
			step_condition = battery->pdata->wpc_step_chg_cond[age_step][i];
		}
		if (battery->wpc_step_chg_status != -1)
			break;
	}

	/* this is only for no consuming current */
	if ((battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_CURRENT_NOW) &&
		!lcd_status &&
		battery->wpc_step_chg_status >= 0) {
		int condition_curr;
		condition_curr = max(battery->current_avg, battery->current_now);
		if (condition_curr < battery->pdata->wpc_step_chg_cond_curr[battery->wpc_step_chg_status]) {
			curr_cnt++;
			pr_info("%s : cnt = %d, curr(%d)mA < curr cond(%d)mA\n",
				__func__, curr_cnt, condition_curr,
				battery->pdata->wpc_step_chg_cond_curr[battery->wpc_step_chg_status]);
			if (curr_cnt < 3)
				return false;
		} else {
			pr_info("%s : clear cnt, curr(%d)mA >= curr cond(%d)mA or < 0mA\n",
				__func__, condition_curr,
				battery->pdata->wpc_step_chg_cond_curr[battery->wpc_step_chg_status]);
			curr_cnt = 0;
			return false;
		}
	}

	pr_info("%s : prev=%d, new=%d, value=%d, current=%d, curr_cnt=%d\n", __func__,
		battery->wpc_step_chg_status, i, value,
		battery->pdata->wpc_step_chg_curr[age_step][i], curr_cnt);
	battery->wpc_step_chg_status = i;
	skip_lcd_on_changed = false;
	sec_vote(battery->fcc_vote, VOTER_WPC_STEP_CHARGE, true,
		battery->pdata->wpc_step_chg_curr[age_step][i]);

	if (battery->wpc_step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
		pr_info("%s : float voltage = %d\n", __func__,
			battery->pdata->wpc_step_chg_vfloat[age_step][i]);
		sec_vote(battery->fv_vote, VOTER_WPC_STEP_CHARGE, true,
			battery->pdata->wpc_step_chg_vfloat[age_step][i]);
	}

	return true;
}
EXPORT_SYMBOL(sec_bat_check_wpc_step_charging);
#endif

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
bool skip_check_dc_step(struct sec_battery_info *battery)
{
	if (battery->dchg_dc_in_swelling) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE)
			return true;
	} else {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE)
			return true;
	}

	if (battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE ||
		   ((battery->current_event & SEC_BAT_CURRENT_EVENT_DC_ERR) &&
		   (battery->ta_alert_mode == OCP_NONE)) ||
		   battery->current_event & SEC_BAT_CURRENT_EVENT_SIOP_LIMIT ||
		   battery->wc_tx_enable ||
		   battery->uno_en ||
		   battery->mix_limit ||
		   battery->lrp_chg_src == SEC_CHARGING_SOURCE_SWITCHING)
		return true;
	else
		return false;
}

bool sec_bat_check_dc_step_charging(struct sec_battery_info *battery)
{
	int i, value;
	int step = -1, step_vol = -1, step_input = -1, step_soc = -1, soc_condition = 0;
	int force_step_soc = 0, step_fg_current = -1;
	bool force_change_step = false;
	union power_supply_propval val = {0, };
	int age_step = battery->pdata->age_step;
	unsigned int dc_step_chg_type;

	if (battery->cable_type == SEC_BATTERY_CABLE_FPDO_DC) {
		sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
		sec_vote(battery->fcc_vote, VOTER_CABLE, true,
			battery->pdata->charging_current[SEC_BATTERY_CABLE_FPDO_DC].fast_charging_current);
		sec_vote_refresh(battery->fcc_vote);

		return false;
	}

	i = (battery->step_chg_status < 0 ? 0 : battery->step_chg_status);
	dc_step_chg_type = battery->dc_step_chg_type[i];

	if (!dc_step_chg_type) {
		sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
		return false;
	}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_CHARGE_POWER)
		if (battery->charge_power < battery->dc_step_chg_charge_power) {
			sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
			return false;
		}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_ONLINE) {
		if (!is_pd_apdo_wire_type(battery->cable_type)) {
			sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
			return false;
		}
	}
	if (skip_check_dc_step(battery)) {
		if (battery->step_chg_status >= 0)
			sec_bat_reset_step_charging(battery);
		sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
		return false;
	}

	if (!(dc_step_chg_type & STEP_CHARGING_CONDITION_DC_INIT)) {
		pr_info("%s : cond_vol and cond_soc are both empty\n", __func__);
		sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, false, 0);
		return false;
	}

	/* this is only for step enter condition and do not use STEP_CHARGING_CONDITION_SOC at the same time */
	if (dc_step_chg_type & STEP_CHARGING_CONDITION_SOC_INIT_ONLY) {
		if (battery->step_chg_status < 0) {
			step_soc = i;
			value = battery->capacity;
			while (step_soc < battery->dc_step_chg_step - 1) {
				soc_condition = battery->pdata->dc_step_chg_cond_soc[age_step][step_soc];
				if (value < soc_condition)
					break;
				step_soc++;
			}

			if ((step_soc < step) || (step < 0))
				step = step_soc;

			pr_info("%s : set initial step(%d) by soc\n", __func__, step_soc);
			goto check_dc_step_change;
		} else
			step_soc = battery->dc_step_chg_step - 1;
	}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_SOC) {
		step_soc = i;
		value = battery->capacity;
		while (step_soc < battery->dc_step_chg_step - 1) {
			soc_condition = battery->pdata->dc_step_chg_cond_soc[age_step][step_soc];
			if (battery->step_chg_status >= 0 &&
				(battery->siop_level < 100 || battery->lcd_status)) {
				soc_condition += DIRECT_CHARGING_FORCE_SOC_MARGIN;
				force_change_step = true;
			}
			if (value < soc_condition)
				break;
			step_soc++;
			if (battery->step_chg_status >= 0)
				break;
		}

		if ((step_soc < step) || (step < 0))
			step = step_soc;

		if (battery->step_chg_status < 0) {
			pr_info("%s : set initial step(%d) by soc\n", __func__, step_soc);
			goto check_dc_step_change;
		}
		if (force_change_step) {
			pr_info("%s : force check step(%d) by soc\n", __func__, step_soc);
			step_vol = step_input = step_soc;
			battery->dc_step_chg_iin_cnt = battery->pdata->dc_step_chg_iin_check_cnt;
			goto check_dc_step_change;
		}
	} else
		step_soc = battery->dc_step_chg_step - 1;

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
		step_vol = i;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		value = max((battery->voltage_avg_main - battery->pdata->dc_step_cond_v_margin_main),
					(battery->voltage_avg_sub - battery->pdata->dc_step_cond_v_margin_sub));
		/* (charging current)step down when main or sub voltage condition meets */
		while (step_vol < battery->dc_step_chg_step - 1) {
			if (battery->voltage_avg_main < battery->pdata->dc_step_chg_cond_vol[age_step][step_vol] &&
				battery->voltage_avg_sub < battery->pdata->dc_step_chg_cond_vol_sub[age_step][step_vol])
				break;
			step_vol++;
			if (battery->step_chg_status >= 0)
				break;
		}
#else
		if (dc_step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
			value = battery->voltage_now + battery->pdata->dc_step_chg_cond_v_margin;
		else
			value = battery->voltage_avg;
		while (step_vol < battery->dc_step_chg_step - 1) {
			if (value < battery->pdata->dc_step_chg_cond_vol[age_step][step_vol])
				break;
			step_vol++;
			if (battery->step_chg_status >= 0)
				break;
		}
#endif
		if ((step_vol < step) || (step < 0))
			step = step_vol;

		if (battery->step_chg_status < 0) {
			pr_info("%s : set initial step(%d) by vol\n", __func__, step_vol);
			goto check_dc_step_change;
		}
	} else
		step_vol = battery->dc_step_chg_step - 1;

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_INPUT_CURRENT) {
		step_input = i;
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, val);
		if (val.intval != SEC_DIRECT_CHG_MODE_DIRECT_ON) {
			pr_info("%s : dc no charging status = %d\n", __func__, val.intval);
			battery->dc_step_chg_iin_cnt = 0;
			return false;
		} else if (battery->siop_level >= 100 && !battery->lcd_status) {
			val.intval = SEC_BATTERY_IIN_MA;
			psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, val);
			value = val.intval;

			while (step_input < battery->dc_step_chg_step - 1) {
				if (value > battery->pdata->dc_step_chg_cond_iin[step_input])
					break;
				step_input++;

				if (battery->step_chg_status >= 0) {
					battery->dc_step_chg_iin_cnt++;
					break;
				} else {
					battery->dc_step_chg_iin_cnt = 0;
				}
			}
		} else {
			/*
			 * Do not check input current when lcd is on or siop is not 100
			 * since there might be quite big system current
			 */
			step_input = battery->dc_step_chg_step - 1;
		}

		if ((step_input < step) || (step < 0))
			step = step_input;
	} else
		step_input = battery->dc_step_chg_step - 1;

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_FG_CURRENT) {
		step_fg_current = i;
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, val);
		if (val.intval != SEC_DIRECT_CHG_MODE_DIRECT_ON) {
			pr_info("%s : dc no charging status = %d\n", __func__, val.intval);
			battery->dc_step_chg_iin_cnt = 0;
			return false;
		} else if (battery->siop_level >= 100 && !battery->lcd_status) {
			int current_now, current_avg;

			val.intval = SEC_BATTERY_CURRENT_MA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_NOW, val);
			current_now = val.intval;
			val.intval = SEC_BATTERY_CURRENT_MA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_AVG, val);
			current_avg = val.intval;
			value = max(current_now, current_avg) / 2;

			while (step_fg_current < battery->dc_step_chg_step - 1) {
				if (value > battery->pdata->dc_step_chg_cond_iin[step_fg_current])
					break;
				step_fg_current++;

				if (battery->step_chg_status >= 0) {
					battery->dc_step_chg_iin_cnt++;
					break;
				}
				battery->dc_step_chg_iin_cnt = 0;
			}
		} else {
			/*
			 * Do not check input current when lcd is on or siop is not 100
			 * since there might be quite big system current
			 */
			step_fg_current = battery->dc_step_chg_step - 1;
		}

		if ((step_fg_current < step) || (step < 0))
			step = step_fg_current;
	} else
		step_fg_current = battery->dc_step_chg_step - 1;


	if (dc_step_chg_type & STEP_CHARGING_CONDITION_FORCE_SOC) {
		force_step_soc = i;
		if (battery->capacity >= battery->pdata->dc_step_chg_cond_soc[age_step][i]) {
			if (++force_step_soc > step)
				step = force_step_soc;
			pr_info("%s : SOC(%d) cond_soc(%d) step(%d) force_step_soc(%d)\n", __func__,
				battery->capacity, battery->pdata->dc_step_chg_cond_soc[age_step][i],
				step, force_step_soc);
		} else
			force_step_soc = 0;
	} else
		force_step_soc = 0;


check_dc_step_change:
	pr_info("%s : curr_step(%d), step_vol(%d), step_soc(%d), step_input(%d, %d), curr_cnt(%d/%d) force_step_soc(%d)\n",
		__func__, step, step_vol, step_soc, step_input, step_fg_current,
		battery->dc_step_chg_iin_cnt, battery->pdata->dc_step_chg_iin_check_cnt, force_step_soc);

	if (battery->step_chg_status < 0 || force_step_soc ||
		(step != battery->step_chg_status &&
		step == min(min(step_vol, step_soc), min(step_input, step_fg_current)))) {
		if ((dc_step_chg_type &
			(STEP_CHARGING_CONDITION_INPUT_CURRENT | STEP_CHARGING_CONDITION_FG_CURRENT)) &&
			(battery->step_chg_status >= 0)) {
			if ((battery->dc_step_chg_iin_cnt < battery->pdata->dc_step_chg_iin_check_cnt) &&
				(battery->siop_level >= 100 && !battery->lcd_status) && !force_step_soc) {
				pr_info("%s : keep step(%d), curr_cnt(%d/%d)\n",
					__func__, battery->step_chg_status,
					battery->dc_step_chg_iin_cnt, battery->pdata->dc_step_chg_iin_check_cnt);
				return false;
			}
		}

		pr_info("%s : cable(%d), soc(%d), step changed(%d->%d), current(%dmA) force_step_soc(%d)\n",
			__func__, battery->cable_type, battery->capacity, battery->step_chg_status, step,
			battery->pdata->dc_step_chg_val_iout[age_step][step], force_step_soc);
		/* set charging current */
		battery->pdata->charging_current[battery->cable_type].fast_charging_current =
			battery->pdata->dc_step_chg_val_iout[age_step][step];

		if (dc_step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
			if (battery->step_chg_status < 0) {
				pr_info("%s : step float voltage = %d\n", __func__,
					battery->pdata->dc_step_chg_val_vfloat[age_step][step]);
				sec_vote(battery->dc_fv_vote, VOTER_DC_STEP_CHARGE, true,
					battery->pdata->dc_step_chg_val_vfloat[age_step][step]);
			}
			battery->dc_float_voltage_set = true;
		}

		if (battery->step_chg_status < 0) {
			pr_info("%s : step input current = %d\n", __func__,
				battery->pdata->dc_step_chg_val_iout[age_step][step] / 2);
			val.intval = battery->pdata->dc_step_chg_val_iout[age_step][step] / 2;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX, val);
		}

		battery->step_chg_status = step;
		battery->dc_step_chg_iin_cnt = 0;

		sec_vote(battery->fcc_vote, VOTER_CABLE, true,
			battery->pdata->dc_step_chg_val_iout[age_step][step]);
		sec_vote_refresh(battery->fcc_vote);

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		if (battery->pdata->dc_step_chg_vsublim) {
			val.intval = battery->pdata->dc_step_chg_vsublim[battery->step_chg_status];

			pr_info("%s: step(%d), SUB limiter chg voltage = %d\n",
					__func__, battery->step_chg_status, val.intval);

			psy_do_property(battery->pdata->sub_limiter_name, set,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
		}
#endif

		return true;
	} else {
		battery->dc_step_chg_iin_cnt = 0;
	}

	return false;
}
EXPORT_SYMBOL(sec_bat_check_dc_step_charging);

int sec_dc_step_charging_dt(struct sec_battery_info *battery, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0, len = 0;
	sec_battery_platform_data_t *pdata = battery->pdata;
	unsigned int i = 0, j = 0, dc_step_chg_type = 0;
	const u32 *p;
	char str[128] = {0,};
	u32 *soc_cond_temp, *vol_cond_temp, *vfloat_temp, *iout_temp;
	int age_step = battery->pdata->age_step;
	int num_age_step = battery->pdata->num_age_step;
	battery->dchg_dc_in_swelling = of_property_read_bool(np,
						     "battery,dchg_dc_in_swelling");
	pr_info("%s: dchg_dc_in_swelling(%d)\n", __func__, battery->dchg_dc_in_swelling);

	ret = of_property_read_u32(np, "battery,dc_step_chg_step",
			&battery->dc_step_chg_step);
	if (ret) {
		pr_err("%s: dc_step_chg_step is Empty\n", __func__);
		battery->dc_step_chg_step = 0;
		goto dc_step_charging_dt_error;
	} else {
		pr_err("%s: dc_step_chg_step is %d\n",
			__func__, battery->dc_step_chg_step);
	}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	pdata->dc_step_chg_vsublim = kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);

	ret = of_property_read_u32_array(np, "battery,dc_step_chg_vsublim",
			pdata->dc_step_chg_vsublim, battery->dc_step_chg_step);
	if (ret) {
		pr_info("%s: dc_step_chg_vsublim is empty\n", __func__);
		pdata->dc_step_chg_vsublim = NULL;
	} else {
		memset(str, 0x0, sizeof(str));
		sprintf(str + strlen(str), "dc_step_chg_vsublim arr :");
		for (i = 0; i < battery->dc_step_chg_step; i++)
			sprintf(str + strlen(str), " %d", pdata->dc_step_chg_vsublim[i]);
		pr_info("%s: %s\n", __func__, str);
	}
#endif

	battery->dc_step_chg_type = kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
	p = of_get_property(np, "battery,dc_step_chg_type", &len);
	if (!p) {
		pr_info("%s: dc_step_chg_type is Empty\n", __func__);
		return -1;
	}
	len = len / sizeof(u32);
	ret = of_property_read_u32_array(np, "battery,dc_step_chg_type",
			battery->dc_step_chg_type, len);
	if (len != battery->dc_step_chg_step) {
		pr_err("%s not match size of dc_step_chg_type: %d\n", __func__, len);
		for (i = 1; i < battery->dc_step_chg_step; i++)
			battery->dc_step_chg_type[i] = battery->dc_step_chg_type[0];
		dc_step_chg_type = battery->dc_step_chg_type[0];
	} else {
		for (i = 0; i < battery->dc_step_chg_step; i++)
			dc_step_chg_type |= battery->dc_step_chg_type[i];
	}

	memset(str, 0x0, sizeof(str));
	sprintf(str + strlen(str), "dc_step_chg_type arr :");
	for (i = 0; i < battery->dc_step_chg_step; i++)
		sprintf(str + strlen(str), " 0x%x", battery->dc_step_chg_type[i]);
	pr_info("%s: %s 0x%x\n", __func__, str, dc_step_chg_type);

	ret = of_property_read_u32(np, "battery,dc_step_chg_charge_power",
			&battery->dc_step_chg_charge_power);
	if (ret) {
		pr_err("%s: dc_step_chg_charge_power is Empty\n", __func__);
		battery->dc_step_chg_charge_power = 20000;
	}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
		p = of_get_property(np, "battery,dc_step_chg_cond_vol", &len);
		if (!p) {
			pr_err("%s: dc_step_chg_cond_vol is Empty, type(0x%X->0x%X)\n",
				__func__, dc_step_chg_type,
				dc_step_chg_type & ~STEP_CHARGING_CONDITION_VOLTAGE);
			for (i = 0; i < battery->dc_step_chg_step; i++)
				battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_VOLTAGE;
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), dc_step_chg_cond_vol len(%d)\n",
				__func__, battery->dc_step_chg_step, num_age_step, len);

			vol_cond_temp = kcalloc(battery->dc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_vol",
						vol_cond_temp, battery->dc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->dc_step_chg_cond_vol = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->dc_step_chg_cond_vol[i] =
					kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					pdata->dc_step_chg_cond_vol[i][j] =
						vol_cond_temp[i*battery->dc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->dc_step_chg_step * num_age_step != len) {
				pr_err("%s: len of dc_step_chg_cond_vol is not matched\n", __func__);

				ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_vol",
						*pdata->dc_step_chg_cond_vol, battery->dc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->dc_step_chg_step; j++)
						pdata->dc_step_chg_cond_vol[i][j] =
							pdata->dc_step_chg_cond_vol[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "vol arr[%d]:", i);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->dc_step_chg_cond_vol[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), dc_step_chg_cond_vol_sub len(%d)\n",
				__func__, battery->dc_step_chg_step, num_age_step, len);

			vol_cond_temp = kcalloc(battery->dc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_vol_sub",
						vol_cond_temp, battery->dc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->dc_step_chg_cond_vol_sub = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->dc_step_chg_cond_vol_sub[i] =
					kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					pdata->dc_step_chg_cond_vol_sub[i][j] =
						vol_cond_temp[i*battery->dc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->dc_step_chg_step * num_age_step != len) {
				pr_err("%s: len of dc_step_chg_cond_vol_sub is not matched\n", __func__);

				ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_vol_sub",
						*pdata->dc_step_chg_cond_vol_sub, battery->dc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->dc_step_chg_step; j++)
						pdata->dc_step_chg_cond_vol_sub[i][j] =
							pdata->dc_step_chg_cond_vol_sub[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "vol_sub arr[%d]:", i);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->dc_step_chg_cond_vol_sub[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}
#endif
			if (ret) {
				pr_info("%s : dc_step_chg_cond_vol read fail\n", __func__);
				for (i = 0; i < battery->dc_step_chg_step; i++)
					battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_VOLTAGE;
			}
			kfree(vol_cond_temp);

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
			ret = of_property_read_u32(np, "battery,dc_step_cond_v_margin_main",
					&battery->pdata->dc_step_cond_v_margin_main);
			if (ret)
				battery->pdata->dc_step_cond_v_margin_main = 0;

			ret = of_property_read_u32(np, "battery,dc_step_cond_v_margin_sub",
					&battery->pdata->dc_step_cond_v_margin_sub);
			if (ret)
				battery->pdata->dc_step_cond_v_margin_sub = 0;

			ret = of_property_read_u32(np, "battery,sc_vbat_thresh_main",
					&battery->pdata->sc_vbat_thresh_main);
			if (ret)
				battery->pdata->sc_vbat_thresh_main = 4420;

			ret = of_property_read_u32(np, "battery,sc_vbat_thresh_sub",
					&battery->pdata->sc_vbat_thresh_sub);
			if (ret)
				battery->pdata->sc_vbat_thresh_sub = battery->pdata->sc_vbat_thresh_main;
#endif
		}
	}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_SOC ||
		dc_step_chg_type & STEP_CHARGING_CONDITION_SOC_INIT_ONLY) {
		p = of_get_property(np, "battery,dc_step_chg_cond_soc", &len);
		if (!p) {
			pr_err("%s: dc_step_chg_cond_soc is Empty, type(0x%X->0x%x)\n",
				__func__, dc_step_chg_type,
				dc_step_chg_type & ~(STEP_CHARGING_CONDITION_SOC |
									STEP_CHARGING_CONDITION_SOC_INIT_ONLY));
			for (i = 0; i < battery->dc_step_chg_step; i++)
				battery->dc_step_chg_type[i] &= ~(STEP_CHARGING_CONDITION_SOC |
									STEP_CHARGING_CONDITION_SOC_INIT_ONLY);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), dc_step_chg_cond_soc len(%d)\n",
				__func__, battery->dc_step_chg_step, num_age_step, len);

			/* get dt to buff */
			soc_cond_temp = kcalloc(battery->dc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_soc",
					soc_cond_temp, battery->dc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->dc_step_chg_cond_soc = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->dc_step_chg_cond_soc[i] =
					kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					pdata->dc_step_chg_cond_soc[i][j] = soc_cond_temp[i*battery->dc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->dc_step_chg_step * num_age_step != len) {
				pr_err("%s: len of dc_step_chg_cond_soc is not matched\n", __func__);

				ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_soc",
						*pdata->dc_step_chg_cond_soc, battery->dc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->dc_step_chg_step; j++)
						pdata->dc_step_chg_cond_soc[i][j] = pdata->dc_step_chg_cond_soc[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "soc arr[%d]:", i);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->dc_step_chg_cond_soc[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret) {
				pr_info("%s : dc_step_chg_cond_soc read fail\n", __func__);
				for (i = 0; i < battery->dc_step_chg_step; i++)
					battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_SOC;
			}

			kfree(soc_cond_temp);

			if (dc_step_chg_type & STEP_CHARGING_CONDITION_SOC &&
				dc_step_chg_type & STEP_CHARGING_CONDITION_SOC_INIT_ONLY) {
				pr_info("%s : do not set SOC and SOC_INIT_ONLY at the same time\n", __func__);
				for (i = 0; i < battery->dc_step_chg_step; i++)
					battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_SOC;
			}
		}
	}

	if (dc_step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
		p = of_get_property(np, "battery,dc_step_chg_val_vfloat", &len);
		if (!p) {
			pr_err("%s: dc_step_chg_val_vfloat is Empty, type(0x%X->0x%x)\n",
				__func__, dc_step_chg_type,
				dc_step_chg_type & ~STEP_CHARGING_CONDITION_FLOAT_VOLTAGE);
			for (i = 0; i < battery->dc_step_chg_step; i++)
				battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_FLOAT_VOLTAGE;
		} else {
			ret = of_property_read_u32(np, "battery,dc_step_chg_cond_v_margin",
					&battery->pdata->dc_step_chg_cond_v_margin);
			if (ret)
				battery->pdata->dc_step_chg_cond_v_margin = DIRECT_CHARGING_FLOAT_VOLTAGE_MARGIN;

			pr_err("%s: dc_step_chg_cond_v_margin is %d\n",
				__func__, battery->pdata->dc_step_chg_cond_v_margin);

			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), dc_step_chg_val_vfloat len(%d)\n",
				__func__, battery->dc_step_chg_step, num_age_step, len);

			vfloat_temp = kcalloc(battery->dc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_val_vfloat",
						vfloat_temp, battery->dc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->dc_step_chg_val_vfloat = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->dc_step_chg_val_vfloat[i] =
					kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					pdata->dc_step_chg_val_vfloat[i][j] =
						vfloat_temp[i*battery->dc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->dc_step_chg_step * num_age_step != len) {
				pr_err("%s: len of dc_step_chg_val_vfloat is not matched\n", __func__);

				ret = of_property_read_u32_array(np, "battery,dc_step_chg_val_vfloat",
						*pdata->dc_step_chg_val_vfloat, battery->dc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->dc_step_chg_step; j++)
						pdata->dc_step_chg_val_vfloat[i][j] =
							pdata->dc_step_chg_val_vfloat[0][j];
				}
			}
			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "vfloat arr[%d]:", i);
				for (j = 0; j < battery->dc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->dc_step_chg_val_vfloat[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret) {
				pr_info("%s : dc_step_chg_val_vfloat read fail\n", __func__);
				for (i = 0; i < battery->dc_step_chg_step; i++)
					battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_FLOAT_VOLTAGE;
			}
			kfree(vfloat_temp);

			pdata->dc_step_chg_vol_offset = kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_vol_offset",
						pdata->dc_step_chg_vol_offset, battery->dc_step_chg_step);
			if (ret) {
				pr_info("%s: dc_step_chg_vol_offset is empty\n", __func__);
				/* Fill-up use one-dimensional offset table */
				for (j = 0; j < battery->dc_step_chg_step; j++)
					if (pdata->dc_step_chg_val_vfloat[0][j] > battery->pdata->chg_float_voltage)
						pdata->dc_step_chg_vol_offset[j] =
							pdata->dc_step_chg_val_vfloat[0][j] -
								battery->pdata->chg_float_voltage;
			}

			memset(str, 0x0, sizeof(str));
			sprintf(str + strlen(str), "dc_step_chg_vol_offset arr :");
			for (i = 0; i < battery->dc_step_chg_step; i++)
				sprintf(str + strlen(str), " %d", pdata->dc_step_chg_vol_offset[i]);
			pr_info("%s: %s\n", __func__, str);
		}
	}

	p = of_get_property(np, "battery,dc_step_chg_val_iout", &len);
	if (!p) {
		pr_err("%s: dc_step_chg_val_iout is Empty\n", __func__);
		for (i = 0; i < battery->dc_step_chg_step; i++)
			battery->dc_step_chg_type[i] = 0;
		return -1;
	} else {
		len = len / sizeof(u32);
		pr_info("%s: step(%d) * age_step(%d), dc_step_chg_val_iout len(%d)\n",
			__func__, battery->dc_step_chg_step, num_age_step, len);

		iout_temp = kcalloc(battery->dc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,dc_step_chg_val_iout",
					iout_temp, battery->dc_step_chg_step * num_age_step);

		/* copy buff to 2d arr */
		pdata->dc_step_chg_val_iout = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
		for (i = 0; i < num_age_step; i++) {
			pdata->dc_step_chg_val_iout[i] =
				kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
			for (j = 0; j < battery->dc_step_chg_step; j++)
				pdata->dc_step_chg_val_iout[i][j] = iout_temp[i*battery->dc_step_chg_step + j];
		}

		/* if there are only 1 dimentional array of value, get the same value */
		if (battery->dc_step_chg_step * num_age_step != len) {
			pr_err("%s: len of dc_step_chg_val_iout is not matched\n", __func__);

			ret = of_property_read_u32_array(np, "battery,dc_step_chg_val_iout",
					*pdata->dc_step_chg_val_iout, battery->dc_step_chg_step);

			for (i = 1; i < num_age_step; i++) {
				for (j = 0; j < battery->dc_step_chg_step; j++)
					pdata->dc_step_chg_val_iout[i][j] = pdata->dc_step_chg_val_iout[0][j];
			}
		}

		/* debug log */
		for (i = 0; i < num_age_step; i++) {
			memset(str, 0x0, sizeof(str));
			sprintf(str + strlen(str), "iout arr[%d]:", i);
			for (j = 0; j < battery->dc_step_chg_step; j++)
				sprintf(str + strlen(str), " %d", pdata->dc_step_chg_val_iout[i][j]);
			pr_info("%s: %s\n", __func__, str);
		}

		if (ret) {
			pr_info("%s : dc_step_chg_val_iout read fail\n", __func__);
		}
		kfree(iout_temp);
	}

	if ((dc_step_chg_type & STEP_CHARGING_CONDITION_INPUT_CURRENT) ||
		(dc_step_chg_type & STEP_CHARGING_CONDITION_FG_CURRENT)) {
		p = of_get_property(np, "battery,dc_step_chg_cond_iin", &len);
		if (!p) {
			pr_info("%s: dc_step_chg_cond_iin is Empty, set default (Iout / 2)\n", __func__);
			pdata->dc_step_chg_cond_iin =
				kcalloc(battery->dc_step_chg_step, sizeof(u32), GFP_KERNEL);
			for (i = 0; i < (battery->dc_step_chg_step - 1); i++) {
				pdata->dc_step_chg_cond_iin[i] = pdata->dc_step_chg_val_iout[age_step][i+1] / 2;
				pr_info("%s: Condition Iin [step %d] %dmA",
					__func__, i, pdata->dc_step_chg_cond_iin[i]);
			}
			pdata->dc_step_chg_cond_iin[i] = 0;
		} else {
			len = len / sizeof(u32);

			if (len != battery->dc_step_chg_step) {
/* [dchg] TODO: do some error handling */
				pr_err("%s: len of dc_step_chg_cond_iin is not matched, len(%d/%d)\n",
					__func__, len, battery->dc_step_chg_step);
			}

			pdata->dc_step_chg_cond_iin = kcalloc(len, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,dc_step_chg_cond_iin",
					pdata->dc_step_chg_cond_iin, len);
			if (ret) {
				pr_info("%s : dc_step_chg_cond_iin read fail\n", __func__);
				for (i = 0; i < battery->dc_step_chg_step; i++)
					battery->dc_step_chg_type[i] &= ~STEP_CHARGING_CONDITION_INPUT_CURRENT;
			}
		}

		ret = of_property_read_u32(np, "battery,dc_step_chg_iin_check_cnt",
				&battery->pdata->dc_step_chg_iin_check_cnt);
		if (ret) {
			pr_err("%s: dc_step_chg_iin_check_cnt is Empty\n", __func__);
			battery->pdata->dc_step_chg_iin_check_cnt = 2;
		} else {
			pr_err("%s: dc_step_chg_iin_check_cnt is %d\n",
				__func__, battery->pdata->dc_step_chg_iin_check_cnt);
		}
	}

	// print dc step charging information
	for (i = 0; i < battery->dc_step_chg_step; i++) {
		memset(str, 0x0, sizeof(str));
		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_VOLTAGE)
			sprintf(str + strlen(str), "cond_vol: %dmV, ", pdata->dc_step_chg_cond_vol[age_step][i]);
		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_SOC)
			sprintf(str + strlen(str), "cond_soc: %d%%, ", pdata->dc_step_chg_cond_soc[age_step][i]);
		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_INPUT_CURRENT)
			sprintf(str + strlen(str), "cond_iin: %dmA, ", pdata->dc_step_chg_cond_iin[i]);
		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
			sprintf(str + strlen(str), "vfloat: %dmV, ", pdata->dc_step_chg_val_vfloat[age_step][i]);

		sprintf(str + strlen(str), "iout: %dmA,", pdata->dc_step_chg_val_iout[age_step][i]);
		pr_info("%s : step [%d] %s\n", __func__, i, str);
	}

	return 0;

dc_step_charging_dt_error:
	return -1;
} /* sec_dc_step_charging_dt */
#endif

void sec_bat_set_aging_info_step_charging(struct sec_battery_info *battery)
{
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	union power_supply_propval val;
	int i = 0;
	unsigned int max_fv = 0;
	int float_volt;
#endif
	int age_step = battery->pdata->age_step;

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	i = (battery->step_chg_status < 0 ? 0 : battery->step_chg_status);
	if (!battery->dc_step_chg_type[i]) {
		pr_info("%s : invalid dc step chg type\n", __func__);
		return;
	}
#endif

	if (battery->step_chg_type) {
		if (battery->step_chg_type & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
			battery->pdata->step_chg_vfloat[age_step][battery->step_chg_step-1] =
				battery->pdata->chg_float_voltage;

		dev_info(battery->dev, "%s: float_v(%d)\n",
			__func__, battery->pdata->step_chg_vfloat[age_step][battery->step_chg_step-1]);
	}
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	for (i = 0; i < battery->dc_step_chg_step; i++) {
		float_volt = battery->pdata->dc_step_chg_vol_offset[i] + battery->pdata->chg_float_voltage;

		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE)
			if (battery->pdata->dc_step_chg_val_vfloat[age_step][i] > float_volt)
				battery->pdata->dc_step_chg_val_vfloat[age_step][i] = float_volt;
		max_fv = max(max_fv, battery->pdata->dc_step_chg_val_vfloat[age_step][i]);

		if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_VOLTAGE)
			if (battery->pdata->dc_step_chg_cond_vol[age_step][i] > float_volt)
				battery->pdata->dc_step_chg_cond_vol[age_step][i] = float_volt;
	}

	for (i = 0; i < battery->dc_step_chg_step; i++) {
		dev_info(battery->dev, "%s: cond_vol: %dmV, vfloat: %dmV, cond_iin: %dmA, iout: %dmA\n", __func__,
			battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_VOLTAGE ?
				battery->pdata->dc_step_chg_cond_vol[age_step][i] : 0,
			battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE ?
				battery->pdata->dc_step_chg_val_vfloat[age_step][i] : 0,
			battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_INPUT_CURRENT ?
				battery->pdata->dc_step_chg_cond_iin[i] : 0,
			battery->pdata->dc_step_chg_val_iout[age_step][i]);
	}

	i = (battery->step_chg_status < 0 ? 0 : battery->step_chg_status);
	if (battery->dc_step_chg_type[i] & STEP_CHARGING_CONDITION_FLOAT_VOLTAGE) {
		val.intval = battery->pdata->dc_step_chg_val_vfloat[age_step][battery->dc_step_chg_step-1];
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_EXT_PROP_DIRECT_CONSTANT_CHARGE_VOLTAGE_MAX, val);
	}

	sec_vote(battery->dc_fv_vote, VOTER_AGING_STEP, true, max_fv);

	sec_bat_reset_step_charging(battery);
	sec_bat_check_dc_step_charging(battery);
#endif
}
EXPORT_SYMBOL(sec_bat_set_aging_info_step_charging);

void sec_step_charging_dt(struct sec_battery_info *battery, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret, len;
	sec_battery_platform_data_t *pdata = battery->pdata;
	unsigned int i = 0, j = 0;
	const u32 *p;
	char str[128] = {0,};
	u32 *soc_cond_temp, *vfloat_temp, *curr_temp;
	int num_age_step = battery->pdata->num_age_step;

	battery->step_charging_skip_lcd_on = of_property_read_bool(np,
						     "battery,step_charging_skip_lcd_on");

	battery->step_chg_en_in_factory = of_property_read_bool(np,
						     "battery,step_chg_en_in_factory");

	ret = of_property_read_u32(np, "battery,step_chg_step",
			&battery->step_chg_step);
	if (ret) {
		pr_err("%s: step_chg_step is Empty\n", __func__);
		battery->step_chg_step = 0;
	} else {
		pr_err("%s: step_chg_step is %d\n",
			__func__, battery->step_chg_step);
	}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	ret = of_property_read_u32(np, "battery,step_chg_vsublim", &pdata->step_chg_vsublim);

	if (ret)
		pdata->step_chg_vsublim = 0;

	pr_err("%s: step_chg_vsublim is %d\n", __func__, pdata->step_chg_vsublim);
#endif

	ret = of_property_read_u32(np, "battery,step_chg_charge_power",
			&battery->step_chg_charge_power);
	if (ret) {
		pr_err("%s: step_chg_charge_power is Empty\n", __func__);
		battery->step_chg_charge_power = 20000;
	}

	p = of_get_property(np, "battery,step_chg_cond", &len);
	if (!p) {
		battery->step_chg_step = 0;
	} else {
		len = len / sizeof(u32);
		pr_info("%s: step(%d) * age_step(%d), step_chg_cond len(%d)\n",
			__func__, battery->step_chg_step, num_age_step, len);

		/* get dt to buff */
		soc_cond_temp = kcalloc(battery->step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,step_chg_cond",
				soc_cond_temp, battery->step_chg_step * num_age_step);

		/* copy buff to 2d arr */
		pdata->step_chg_cond = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
		for (i = 0; i < num_age_step; i++) {
			pdata->step_chg_cond[i] =
				kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);
			for (j = 0; j < battery->step_chg_step; j++)
				pdata->step_chg_cond[i][j] = soc_cond_temp[i*battery->step_chg_step + j];
		}

		/* if there are only 1 dimentional array of value, get the same value */
		if (battery->step_chg_step * num_age_step != len) {
			ret = of_property_read_u32_array(np, "battery,step_chg_cond",
				*pdata->step_chg_cond, battery->step_chg_step);
			for (i = 0; i < num_age_step; i++) {
				for (j = 0; j < battery->step_chg_step; j++)
					pdata->step_chg_cond[i][j] = pdata->step_chg_cond[0][j];
			}
		}

		/* debug log */
		for (i = 0; i < num_age_step; i++) {
			memset(str, 0x0, sizeof(str));
			sprintf(str + strlen(str), "step_chg_cond arr[%d]:", i);
			for (j = 0; j < battery->step_chg_step; j++)
				sprintf(str + strlen(str), " %d", pdata->step_chg_cond[i][j]);
			pr_info("%s: %s\n", __func__, str);
		}

		if (ret) {
			pr_info("%s : step_chg_cond read fail\n", __func__);
			battery->step_chg_step = 0;
		}

		kfree(soc_cond_temp);

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		if (battery->step_chg_type & STEP_CHARGING_CONDITION_VOLTAGE) {
			p = of_get_property(np, "battery,step_chg_cond_sub", &len);
			if (!p) {
				pr_err("%s: step_chg_cond_sub is Empty\n", __func__);
				pdata->step_chg_cond_sub = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
				for (i = 0; i < num_age_step; i++) {
					pdata->step_chg_cond_sub[i] =
						kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);
					for (j = 0; j < battery->step_chg_step; j++)
						pdata->step_chg_cond_sub[i][j] = pdata->step_chg_cond[i][j];
				}
			} else {
				len = len / sizeof(u32);
				pr_info("%s: step(%d) * age_step(%d), step_chg_cond_sub len(%d)\n",
					__func__, battery->step_chg_step, num_age_step, len);
				/* get dt to buff */
				soc_cond_temp = kcalloc(battery->step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
				ret = of_property_read_u32_array(np, "battery,step_chg_cond_sub",
						soc_cond_temp, battery->step_chg_step * num_age_step);

				/* copy buff to 2d arr */
				pdata->step_chg_cond_sub = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
				for (i = 0; i < num_age_step; i++) {
					pdata->step_chg_cond_sub[i] =
						kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);
					for (j = 0; j < battery->step_chg_step; j++)
						pdata->step_chg_cond_sub[i][j] = soc_cond_temp[i*battery->step_chg_step + j];
				}

				/* if there are only 1 dimentional array of value, get the same value */
				if (battery->step_chg_step * num_age_step != len) {
					ret = of_property_read_u32_array(np, "battery,step_chg_cond_sub",
						*pdata->step_chg_cond_sub, battery->step_chg_step);
					for (i = 0; i < num_age_step; i++) {
						for (j = 0; j < battery->step_chg_step; j++)
							pdata->step_chg_cond_sub[i][j] = pdata->step_chg_cond_sub[0][j];
					}
				}

				/* debug log */
				for (i = 0; i < num_age_step; i++) {
					memset(str, 0x0, sizeof(str));
					sprintf(str + strlen(str), "step_chg_cond_sub arr[%d]:", i);
					for (j = 0; j < battery->step_chg_step; j++)
						sprintf(str + strlen(str), " %d", pdata->step_chg_cond_sub[i][j]);
					pr_info("%s: %s\n", __func__, str);
				}

				if (ret)
					pr_info("%s : step_chg_cond_sub read fail\n", __func__);

				kfree(soc_cond_temp);
			}
		}
#endif

		p = of_get_property(np, "battery,step_chg_cond_curr", &len);
		if (!p) {
			pr_err("%s: step_chg_cond_curr is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pdata->step_chg_cond_curr = kcalloc(len, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,step_chg_cond_curr",
					pdata->step_chg_cond_curr, len);
			if (ret) {
				pr_info("%s : step_chg_cond_curr read fail\n", __func__);
				battery->step_chg_step = 0;
			}
		}

		p = of_get_property(np, "battery,step_chg_vfloat", &len);
		if (!p) {
			pr_err("%s: step_chg_vfloat is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), step_chg_vfloat len(%d)\n",
				__func__, battery->step_chg_step, num_age_step, len);

			vfloat_temp = kcalloc(battery->step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,step_chg_vfloat",
				vfloat_temp, battery->step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->step_chg_vfloat = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->step_chg_vfloat[i] =
					kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->step_chg_step; j++)
					pdata->step_chg_vfloat[i][j] =
						vfloat_temp[i*battery->step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->step_chg_step * num_age_step != len) {
				ret = of_property_read_u32_array(np, "battery,step_chg_vfloat",
					*pdata->step_chg_vfloat, battery->step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->step_chg_step; j++)
						pdata->step_chg_vfloat[i][j] = pdata->step_chg_vfloat[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "step_chg_vfloat arr[%d]:", i);
				for (j = 0; j < battery->step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->step_chg_vfloat[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret)
				pr_info("%s : step_chg_vfloat read fail\n", __func__);

			kfree(vfloat_temp);
		}

		p = of_get_property(np, "battery,step_chg_curr", &len);
		if (!p) {
			pr_err("%s: step_chg_curr is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), step_chg_curr len(%d)\n",
				__func__, battery->step_chg_step, num_age_step, len);

			curr_temp = kcalloc(battery->step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,step_chg_curr",
				curr_temp, battery->step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->step_chg_curr = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->step_chg_curr[i] =
					kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->step_chg_step; j++)
					pdata->step_chg_curr[i][j] = curr_temp[i*battery->step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->step_chg_step * num_age_step != len) {
				ret = of_property_read_u32_array(np, "battery,step_chg_curr",
					*pdata->step_chg_curr, battery->step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->step_chg_step; j++)
						pdata->step_chg_curr[i][j] = pdata->step_chg_curr[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "step_chg_curr arr[%d]:", i);
				for (j = 0; j < battery->step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->step_chg_curr[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret)
				pr_info("%s : step_chg_curr read fail\n", __func__);

			kfree(curr_temp);
		}

		p = of_get_property(np, "battery,step_chg_cond_soc", &len);
		if (!p) {
			pr_err("%s: step_chg_cond_soc is Empty\n", __func__);
			battery->step_chg_type =
				(battery->step_chg_type) & (~STEP_CHARGING_CONDITION_SOC_INIT_ONLY);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), step_chg_soc len(%d)\n",
				__func__, battery->step_chg_step, num_age_step, len);

			if (battery->step_chg_step * num_age_step != len) {
				pr_err("%s: mis-match len!!\n", __func__);
				battery->step_chg_type =
					(battery->step_chg_type) & (~STEP_CHARGING_CONDITION_SOC_INIT_ONLY);
				goto err_soc;
			}

			curr_temp = kcalloc(battery->step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			if (!curr_temp)
				goto err_soc;

			ret = of_property_read_u32_array(np, "battery,step_chg_cond_soc",
				curr_temp, battery->step_chg_step * num_age_step);
			if (ret) {
				pr_err("%s: failed to read chg_cond_soc(ret = %d)\n", __func__, ret);
				kfree(curr_temp);
				battery->step_chg_type =
					(battery->step_chg_type) & (~STEP_CHARGING_CONDITION_SOC_INIT_ONLY);
				goto err_soc;
			}

			pdata->step_chg_cond_soc = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->step_chg_cond_soc[i] =
					kcalloc(battery->step_chg_step, sizeof(u32), GFP_KERNEL);

				for (j = 0; j < battery->step_chg_step; j++)
					pdata->step_chg_cond_soc[i][j] = curr_temp[i*battery->step_chg_step + j];
			}

			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "step_chg_cond_soc arr[%d]:", i);
				for (j = 0; j < battery->step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->step_chg_cond_soc[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}
err_soc:
			pr_info("%s: step_chg_soc end\n", __func__);
		}
	}
}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
void sec_wpc_step_charging_dt(struct sec_battery_info *battery, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret, len;
	sec_battery_platform_data_t *pdata = battery->pdata;
	unsigned int i = 0, j = 0;
	const u32 *p;
	char str[128] = {0,};
	u32 *soc_cond_temp, *vfloat_temp, *curr_temp;
	int num_age_step = battery->pdata->num_age_step;

	ret = of_property_read_u32(np, "battery,wpc_step_chg_step",
			&battery->wpc_step_chg_step);
	if (ret) {
		pr_err("%s: wpc_step_chg_step is Empty\n", __func__);
		battery->wpc_step_chg_step = 0;
	} else {
		pr_err("%s: wpc_step_chg_step is %d\n",
			__func__, battery->wpc_step_chg_step);
	}

	ret = of_property_read_u32(np, "battery,wpc_step_chg_charge_power",
			&battery->wpc_step_chg_charge_power);
	if (ret) {
		pr_err("%s: wpc_step_chg_charge_power is Empty\n", __func__);
		battery->wpc_step_chg_charge_power = 7500;
	}

	p = of_get_property(np, "battery,wpc_step_chg_cond", &len);
	if (!p) {
		battery->wpc_step_chg_step = 0;
	} else {
		len = len / sizeof(u32);
		pr_info("%s: step(%d) * age_step(%d), step_chg_cond len(%d)\n",
			__func__, battery->wpc_step_chg_step, num_age_step, len);

		/* get dt to buff */
		soc_cond_temp = kcalloc(battery->wpc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
		ret = of_property_read_u32_array(np, "battery,wpc_step_chg_cond",
				soc_cond_temp, battery->wpc_step_chg_step * num_age_step);

		/* copy buff to 2d arr */
		pdata->wpc_step_chg_cond = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
		for (i = 0; i < num_age_step; i++) {
			pdata->wpc_step_chg_cond[i] =
				kcalloc(battery->wpc_step_chg_step, sizeof(u32), GFP_KERNEL);
			for (j = 0; j < battery->wpc_step_chg_step; j++)
				pdata->wpc_step_chg_cond[i][j] = soc_cond_temp[i*battery->wpc_step_chg_step + j];
		}

		/* if there are only 1 dimentional array of value, get the same value */
		if (battery->wpc_step_chg_step * num_age_step != len) {
			ret = of_property_read_u32_array(np, "battery,wpc_step_chg_cond",
				*pdata->wpc_step_chg_cond, battery->wpc_step_chg_step);
			for (i = 0; i < num_age_step; i++) {
				for (j = 0; j < battery->wpc_step_chg_step; j++)
					pdata->wpc_step_chg_cond[i][j] = pdata->wpc_step_chg_cond[0][j];
			}
		}

		/* debug log */
		for (i = 0; i < num_age_step; i++) {
			memset(str, 0x0, sizeof(str));
			sprintf(str + strlen(str), "wpc_step_chg_cond arr[%d]:", i);
			for (j = 0; j < battery->wpc_step_chg_step; j++)
				sprintf(str + strlen(str), " %d", pdata->wpc_step_chg_cond[i][j]);
			pr_info("%s: %s\n", __func__, str);
		}

		if (ret) {
			pr_info("%s : wpc_step_chg_cond read fail\n", __func__);
			battery->wpc_step_chg_step = 0;
		}

		kfree(soc_cond_temp);

		p = of_get_property(np, "battery,wpc_step_chg_cond_curr", &len);
		if (!p) {
			pr_err("%s: wpc_step_chg_cond_curr is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pdata->wpc_step_chg_cond_curr = kcalloc(len, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,wpc_step_chg_cond_curr",
					pdata->wpc_step_chg_cond_curr, len);
			if (ret) {
				pr_info("%s : wpc_step_chg_cond_curr read fail\n", __func__);
				battery->wpc_step_chg_step = 0;
			}
		}

		p = of_get_property(np, "battery,wpc_step_chg_vfloat", &len);
		if (!p) {
			pr_err("%s: wpc_step_chg_vfloat is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), wpc_step_chg_vfloat len(%d)\n",
				__func__, battery->wpc_step_chg_step, num_age_step, len);

			vfloat_temp = kcalloc(battery->wpc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,wpc_step_chg_vfloat",
				vfloat_temp, battery->wpc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->wpc_step_chg_vfloat = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->wpc_step_chg_vfloat[i] =
					kcalloc(battery->wpc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->wpc_step_chg_step; j++)
					pdata->wpc_step_chg_vfloat[i][j] =
						vfloat_temp[i*battery->wpc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->wpc_step_chg_step * num_age_step != len) {
				ret = of_property_read_u32_array(np, "battery,wpc_step_chg_vfloat",
					*pdata->wpc_step_chg_vfloat, battery->wpc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->wpc_step_chg_step; j++)
						pdata->wpc_step_chg_vfloat[i][j] = pdata->wpc_step_chg_vfloat[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "wpc_step_chg_vfloat arr[%d]:", i);
				for (j = 0; j < battery->wpc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->wpc_step_chg_vfloat[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret)
				pr_info("%s : wpc_step_chg_vfloat read fail\n", __func__);

			kfree(vfloat_temp);
		}

		p = of_get_property(np, "battery,wpc_step_chg_curr", &len);
		if (!p) {
			pr_err("%s: wpc_step_chg_curr is Empty\n", __func__);
		} else {
			len = len / sizeof(u32);
			pr_info("%s: step(%d) * age_step(%d), wpc_step_chg_curr len(%d)\n",
				__func__, battery->wpc_step_chg_step, num_age_step, len);

			curr_temp = kcalloc(battery->wpc_step_chg_step * num_age_step, sizeof(u32), GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,wpc_step_chg_curr",
				curr_temp, battery->wpc_step_chg_step * num_age_step);

			/* copy buff to 2d arr */
			pdata->wpc_step_chg_curr = kcalloc(num_age_step, sizeof(u32 *), GFP_KERNEL);
			for (i = 0; i < num_age_step; i++) {
				pdata->wpc_step_chg_curr[i] =
					kcalloc(battery->wpc_step_chg_step, sizeof(u32), GFP_KERNEL);
				for (j = 0; j < battery->wpc_step_chg_step; j++)
					pdata->wpc_step_chg_curr[i][j] = curr_temp[i*battery->wpc_step_chg_step + j];
			}

			/* if there are only 1 dimentional array of value, get the same value */
			if (battery->wpc_step_chg_step * num_age_step != len) {
				ret = of_property_read_u32_array(np, "battery,wpc_step_chg_curr",
					*pdata->wpc_step_chg_curr, battery->wpc_step_chg_step);

				for (i = 1; i < num_age_step; i++) {
					for (j = 0; j < battery->wpc_step_chg_step; j++)
						pdata->wpc_step_chg_curr[i][j] = pdata->wpc_step_chg_curr[0][j];
				}
			}

			/* debug log */
			for (i = 0; i < num_age_step; i++) {
				memset(str, 0x0, sizeof(str));
				sprintf(str + strlen(str), "wpc_step_chg_curr arr[%d]:", i);
				for (j = 0; j < battery->wpc_step_chg_step; j++)
					sprintf(str + strlen(str), " %d", pdata->wpc_step_chg_curr[i][j]);
				pr_info("%s: %s\n", __func__, str);
			}

			if (ret)
				pr_info("%s : wpc_step_chg_curr read fail\n", __func__);

			kfree(curr_temp);
		}
	}
}
#endif

void sec_step_charging_init(struct sec_battery_info *battery, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	battery->step_chg_status = -1;

	ret = of_property_read_u32(np, "battery,step_chg_type",
			&battery->step_chg_type);
	pr_err("%s: step_chg_type 0x%x\n", __func__, battery->step_chg_type);
	if (ret) {
		pr_err("%s: step_chg_type is Empty\n", __func__);
		battery->step_chg_type = 0;
	}

	if (battery->step_chg_type)
		sec_step_charging_dt(battery, dev);
#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
	ret = of_property_read_u32(np, "battery,wpc_step_chg_type",
			&battery->wpc_step_chg_type);
	pr_err("%s: wpc_step_chg_type 0x%x\n", __func__, battery->wpc_step_chg_type);
	if (ret) {
		pr_err("%s: wpc_step_chg_type is Empty\n", __func__);
		battery->wpc_step_chg_type = 0;
	}

	if (battery->wpc_step_chg_type)
		sec_wpc_step_charging_dt(battery, dev);
#endif
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	sec_dc_step_charging_dt(battery, dev);
#endif
}
EXPORT_SYMBOL(sec_step_charging_init);
