/*
 * sm5714-charger.c - SM5714 Charger device driver
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/muic/common/muic.h>
#include <linux/of_gpio.h>
#include "../../common/sec_charging_common.h"
#include "sm5714_charger.h"
#include <linux/battery/sec_pd.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#define HEALTH_DEBOUNCE_CNT		1
#define ENABLE_SM5714_ENBYPASS_MODE	1
#define SM5714_CHARGER_VERSION  "XB1"

#if IS_ENABLED(CONFIG_USE_POGO)
extern int sm5714_muic_get_vbus_voltage(void);
#endif

static struct device_attribute sm5714_charger_attrs[] = {
	SM5714_CHARGER_ATTR(chip_id),
	SM5714_CHARGER_ATTR(data),
	SM5714_CHARGER_ATTR(data_1),
};

static char *sm5714_supplied_to[] = {
	"sm5714-charger",
};

static enum power_supply_property sm5714_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sm5714_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);

static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);

static void sm5714_init_aicl_irq(struct sm5714_charger_data *charger);

static int sm5714_get_facmode(void) { return factory_mode; }

static int chg_get_en_shipmode(struct sm5714_charger_data *charger)
{
	u8 reg;
	bool enable;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL11, &reg);
	enable = (reg & (0x1 << 5)) ? 1 : 0;

	pr_info("sm5714-charger: %s: forced ship mode - %s\n", __func__, enable ? "Enable" : "Disable");

	return enable;
}

static void chg_set_en_shipmode(struct sm5714_charger_data *charger, bool enable)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL11, (enable << 5), (0x1 << 5));
	pr_info("sm5714-charger: %s: forced ship mode - %s\n", __func__, enable ? "Enable" : "Disable");
}

/* AUTO ship mode condition : [SHIP_FORCED = 0] & [VSYS < SHIP_AUTO] */

static void chg_set_auto_shipmode(struct sm5714_charger_data *charger, u8 vref)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL11,
		(vref << 1), (0x3 << 1));	/*	SHIP_AUTO_VREF	*/
	if (charger->is_sm5714a)
		pr_info("sm5714-charger: %s: set auto ship vref = %d mV\n",
				__func__, vref ? (3100 + (vref * 300)) : 2600);
	else
		pr_info("sm5714-charger: %s: set auto ship vref = %d mV\n", __func__, (2600 + (vref * 200)));
}

static void chg_set_auto_shipmode_time(struct sm5714_charger_data *charger, u8 deglitch_time)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL11,
		(deglitch_time << 3), (0x3 << 3));	/*	SHIP_AUTO_TIME	*/
}

#if defined(CONFIG_SHIPMODE_BY_VBAT) && !defined(CONFIG_SEC_FACTORY)
static bool chg_check_current_level(void)
{
	union power_supply_propval val_avg_curr = {0, }, val_now_curr = {0, };

	val_avg_curr.intval = SEC_BATTERY_CURRENT_MA;
	val_now_curr.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property("sm5714-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_AVG, val_avg_curr);
	psy_do_property("sm5714-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_NOW, val_now_curr);
	pr_info("sm5714-charger: %s: current: %d, %d\n",
		__func__, val_avg_curr.intval, val_now_curr.intval);

	return ((val_avg_curr.intval > 6000) && (val_now_curr.intval > 6000)) ? true : false;
}

static u8 chg_get_auto_shipmode_data(int voltage, int offset)
{
	u8 ret = AUTO_SHIP_MODE_VREF_V_2_6;

	if (voltage >= 4000)
		ret = 0x03;
	else if (voltage >= 3700)
		ret = 0x02;
	else if (voltage >= 3400)
		ret = 0x01;

	if (ret > offset)
		ret = (ret) ?
		(offset ? (ret - offset) : ret) : AUTO_SHIP_MODE_VREF_V_2_6;
	else
		ret = AUTO_SHIP_MODE_VREF_V_2_6;

	return ret;
}

static void chg_set_auto_shipmode_level(struct sm5714_charger_data *charger)
{
	union power_supply_propval value;
	int voltage = 2600;
	u8 reg_data;
	int offset = 0;
	int ari_cond = charger->spcom ? 91 : 0;

	if (!charger->is_sm5714a)
		return;

	/* case with stray voltage due to TA connection */
	if (!is_nocharge_type(charger->cable_type)) {
		if (chg_check_current_level())
			offset = 2;
		else
			offset = 1;
	}

	psy_do_property("sm5714-fuelgauge", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	if (value.intval >= 4400)
		voltage = 4000;
	else if (value.intval >= 4100)
		voltage = 3700;
	else if (value.intval >= 3800)
		voltage = 3400;

	/* Not delivered ari cnt or under 90, set 2.6v auto ship mode */
	/* no dts, but if ari cnt write,it is judged to be abnormal and set 2.6v auto ship mode */
	if (charger->ari_cnt < ari_cond)
		voltage = 2600;

	reg_data = chg_get_auto_shipmode_data(voltage, offset);
	dev_info(charger->dev, "%s: check shipmode %d, %d, 0x%x\n",
		__func__, value.intval, voltage, reg_data);
	chg_set_auto_shipmode(charger, reg_data);
}
#endif

#if defined(CONFIG_SEC_FACTORY)
static void chg_set_en_nozx(struct sm5714_charger_data *charger, bool enable)
{
	if (!charger->is_sm5714a)
		return;

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_FACTORY1,
				(enable << 0), (0x1 << 0));	/* 0: Enable NOZX, 1: Disable NOZX */
	dev_info(charger->dev, "%s: NOZX - %s\n",
		__func__, (!enable) ? "Enable" : "Disable");
}
#endif

#if defined(ENABLE_SM5714_ENBYPASS_MODE)
static void chg_set_en_ovp_bypass_mode(struct sm5714_charger_data *charger, bool enable)
{
	pr_info("sm5714-charger: %s: %s\n", __func__, enable ? "Enable" : "Disable");

	if (enable) {
		/* SUSPEND MODE */
		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL2,
				0x00, 0x0F);
		/* set JIGON HIGH - write 0x2 @0x17(JIGON_CONTROL) */
		sec_pd_manual_jig_ctrl(enable);
	} else {
		/* set JIGON LOW - write 0x3 @0x17(JIGON_CONTROL) */
		sec_pd_manual_jig_ctrl(enable);
		/* CHG ON MODE */
		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL2,
				0x05, 0x0F);
	}
}

static void chg_set_en_bypass(struct sm5714_charger_data *charger, bool enable)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_FACTORY1, (enable << 1), (0x1 << 1));
	pr_info("sm5714-charger: %s: bypass mode - %s\n", __func__, enable ? "Enable" : "Disable");
}

static void chg_set_en_bypass_mode(struct sm5714_charger_data *charger, bool enable)
{
	union power_supply_propval val = {0, };

	if (charger->pdata->ovp_bypass_mode) {
		chg_set_en_ovp_bypass_mode(charger, enable);
		return;
	}

	if (enable) {
		if (!sm5714_get_facmode())
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x24 << 0), (0x7F << 0));		/* VBUS_LIMIT	= 1000mA */

		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_FACTORY1,
			(0x1 << 4), (0x1 << 4));	/* OFFREVERSE deactivated(1) */


		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL1,
			(0x0 << 6), (0x1 << 6));	/* AICLEN_VBUS = disable(0) */

		chg_set_en_bypass(charger, 1);	/* ENBYPASS = enable(1) */

		psy_do_property("sm5714-fuelgauge", set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

	} else {
		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_FACTORY1,
			(0x0 << 4), (0x1 << 4));	/* OFFREVERSE activated(0) */

		chg_set_en_bypass(charger, 0);	/* ENBYPASS = disable(0) */

		if (!sm5714_get_facmode())
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_VBUSCNTL,
				(0x00 << 0), (0x7F << 0));		/* VBUS_LIMIT	= 100mA */
	}
	pr_info("sm5714-charger: %s: %s\n", __func__, enable ? "Enable" : "Disable");
}
#endif

static void chg_set_aicl(struct sm5714_charger_data *charger, bool enable, u8 aicl)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL11, (aicl << 6), (0x3 << 6));
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL1, (enable << 6), (0x1 << 6));
}

static void chg_set_dischg_limit(struct sm5714_charger_data *charger, u8 dischg)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL6, (dischg << 1), (0x7 << 1));
}
static void chg_set_ocp_current(struct sm5714_charger_data *charger, u32 ocp_current)
{
	u8 dischg = DISCHG_LIMIT_C_5_4;

	if (ocp_current >= 9000)
		dischg = DISCHG_LIMIT_C_9_0;
	else if (ocp_current >= 8400)
		dischg = DISCHG_LIMIT_C_8_4;
	else if (ocp_current >= 7800)
		dischg = DISCHG_LIMIT_C_7_8;
	else if (ocp_current >= 7200)
		dischg = DISCHG_LIMIT_C_7_2;
	else if (ocp_current >= 6600)
		dischg = DISCHG_LIMIT_C_6_6;
	else if (ocp_current >= 6000)
		dischg = DISCHG_LIMIT_C_6_0;
	else if (ocp_current >= 5400)
		dischg = DISCHG_LIMIT_C_5_4;
	else
		dischg = DISCHG_LIMIT_DISABLE;

	chg_set_dischg_limit(charger, dischg);
}

static void chg_set_batreg(struct sm5714_charger_data *charger, u16 float_voltage)
{
	u8 offset;

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip batreg Control\n", __func__);
		return;
	}

	if (float_voltage <= 3700)
		offset = 0x0;
	else if (float_voltage < 3900)
		offset = ((float_voltage - 3700) / 50);    /* BATREG = 3.70 ~ 3.85V in 0.05V steps */
	else if (float_voltage < 4050)
		offset = (((float_voltage - 3900) / 100) + 4);    /* BATREG = 3.90, 4.0V in 0.1V steps */
	else if (float_voltage < 4630)
		offset = (((float_voltage - 4050) / 10) + 6);    /* BATREG = 4.05 ~ 4.62V in 0.01V steps */
	else {
		dev_err(charger->dev, "%s: can't support BATREG at over voltage 4.62V (mV=%d)\n",
			__func__, float_voltage);
		offset = 0x15;    /* default Offset : 4.2V */
	}

	pr_info("%s:  set as  (mV=%d) batreg Control\n", __func__, float_voltage);

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL4, ((offset & 0x3F) << 0), (0x3F << 0));
}


static void chg_set_wdt_timer(struct sm5714_charger_data *charger, u8 wdt_timer)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_WDTCNTL, (wdt_timer << 1), (0x3 << 1));
}

static void chg_set_wdt_tmr_reset(struct sm5714_charger_data *charger)
{
	dev_info(charger->dev, "%s: wdt kick\n", __func__);
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_WDTCNTL, (0x1 << 3), (0x1 << 3));
}

static void chg_set_wdt_enable(struct sm5714_charger_data *charger, bool enable)
{
	dev_info(charger->dev, "%s: wdt enable(%d)\n", __func__, enable);
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_WDTCNTL, (enable << 0), (0x1 << 0));
	if (enable)
		chg_set_wdt_tmr_reset(charger);
}

static void chg_set_wdtcntl_reset(struct sm5714_charger_data *charger)
{
	dev_info(charger->dev, "%s: clear wdt expired\n", __func__);
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_WDTCNTL, (0x1 << 6), (0x1 << 6));
}

static void chg_set_input_current_limit(struct sm5714_charger_data *charger, int mA)
{
	u8 offset;

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip current limit Control\n", __func__);
		return;
	}

	mutex_lock(&charger->charger_mutex);
	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_FACTORY1, &offset);
	if (offset & 0x1)
		dev_info(charger->dev, "enabled FACTORY mode, skipped VBUS_LIMIT setting\n");
	else {
		if (mA < 100)
			offset = 0x00;
		else
			offset = ((mA - 100) / 25) & 0x7F;

		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_VBUSCNTL, (offset << 0), (0x7F << 0));
		charger->input_current = mA;
	}
	mutex_unlock(&charger->charger_mutex);
}

static void chg_set_charging_current(struct sm5714_charger_data *charger, int mA)
{
	u8 offset;
	int uA;

	uA = mA * 1000;

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip charging current Control\n", __func__);
		return;
	}

	if (uA < 109375)		// 109.375 mA
		offset = 0x07;
	else if (uA > 3500000)	//	3500.000 mA
		offset = 0xE0;
	else
		offset = (7 + ((uA - 109375) / 15625)) & 0xFF;

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL2, (offset << 0), (0xFF << 0));
}

static void chg_set_topoff_current(struct sm5714_charger_data *charger, int mA)
{
	u8 offset;

	if (mA < 100)
		offset = 0x0;               /* Topoff = 100mA */
	else if (mA < 800)
		offset = (mA - 100) / 25;   /* Topoff = 125mA ~ 775mA in 25mA steps */
	else
		offset = 0x1C;              /* Topoff = 800mA */

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL5, (offset << 0), (0x1F << 0));
}

static void chg_set_topoff_timer(struct sm5714_charger_data *charger, u8 tmr_offset)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL7, (tmr_offset << 3), (0x3 << 3));
}

static void chg_set_autostop(struct sm5714_charger_data *charger, bool enable)
{
	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL4, (enable << 6), (0x1 << 6));
}

static void chg_set_lxslope(struct sm5714_charger_data *charger, u8 value)
{
	/* 00 : 1.58 V/ns */
	/* 01 : 3 V/ns */
	/* 10 : 4.38 V/ns */
	/* 11 : 5.43 V/ns */

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL8, (value << 0), (0x3 << 0));
	pr_info("sm5714-charger: %s: %d\n", __func__, value);
}

static int chg_get_input_current_limit(struct sm5714_charger_data *charger)
{
	u8 reg;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_VBUSCNTL, &reg);

	return ((reg & 0x7F) * 25) + 100;
}

static int chg_get_charging_current(struct sm5714_charger_data *charger)
{
	u8 reg;
	int fast_curr_ua;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL2, &reg);

	if ((reg & 0xFF) <= 0x07)
		fast_curr_ua = 109000;
	else if ((reg & 0xFF) >= 0xE0)
		fast_curr_ua = 3500000;
	else
		fast_curr_ua = 109375 + ((reg & 0xFF) - 7) * 15625;

	return (fast_curr_ua / 1000);
}

static void chg_set_enq4fet(struct sm5714_charger_data *charger, bool enable)
{
	int vbuslimit;
	u8 msec;

	dev_info(charger->dev, "%s: ENQ4FET(%d)\n", __func__, enable);
	if (enable) {
		vbuslimit = chg_get_input_current_limit(charger);
		if (vbuslimit > 500) {
			msec = (vbuslimit - 500) / 250; /* 250mA/ms */
			chg_set_input_current_limit(charger, 500);
			msleep(msec);
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL1, (1 << 3), (0x1 << 3));
			chg_set_input_current_limit(charger, vbuslimit);
		} else {
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL1, (1 << 3), (0x1 << 3));
		}
	} else {
		sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL1, (0 << 3), (0x1 << 3));
	}
}

/*
 *static int chg_get_tricklecharging_current(struct sm5714_charger_data *charger)
 *{
 *	u8 reg;
 *	int trk_curr_ua;
 *
 *	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL3, &reg);
 *
 *	if ((reg & 0xFF) <= 0x07)
 *		trk_curr_ua = 109000;
 *	else if ((reg & 0xFF) >= 0xE0)
 *		trk_curr_ua = 3500000;
 *	else
 *		trk_curr_ua = 109375 + ((reg & 0xFF) - 7) * 15625;
 *
 *	return trk_curr_ua;	// uA
 *}
 */

static int chg_get_topoff_current(struct sm5714_charger_data *charger)
{
	u8 reg;
	int topoff;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL5, &reg);

	if ((reg & 0x1F) >= 0x1C)
		topoff = 800;
	else
		topoff = ((reg & 0x1F) * 25) + 100;

	return topoff;
}

static int chg_get_regulation_voltage(struct sm5714_charger_data *charger)
{
	u8 reg;
	int float_voltage;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL4, &reg);

	reg = reg & 0x3F;

	if (reg <= 0x03)	/* BATREG = 3.70 ~ 3.85V in 0.05V steps */
		float_voltage = 3700 + (reg * 50);
	else if (reg <= 0x5)	/* BATREG = 3.90, 4.00V in 0.1V steps */
		float_voltage = 3900 + ((reg - 0x4) * 100);
	else	/* BATREG = 4.05 ~ 4.62V in 0.01V steps */
		float_voltage = 4050 + ((reg - 0x6) * 10);

	return float_voltage;
}

#define PRINT_CHG_REG_NUM    32
#define PRINT_CHG_REG2_NUM   17
static void chg_print_regmap(struct sm5714_charger_data *charger)
{
	u8 regs[PRINT_CHG_REG_NUM] = {0x0, };
	u8 reg2s[PRINT_CHG_REG2_NUM] = {0x0, };
	char temp_buf[500] = {0,};
	int i;

	// READ INTMSK
	sm5714_bulk_read(charger->i2c, SM5714_CHG_REG_INTMSK1, PRINT_CHG_REG_NUM, regs);
	for (i = 0; i < PRINT_CHG_REG_NUM; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5714_CHG_REG_INTMSK1 + i, regs[i]);
		if (i % 11 == 10) {
			pr_info("sm5714-charger: regmap: %s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
	pr_info("sm5714-charger: regmap: %s\n", temp_buf);
	memset(temp_buf, 0x0, sizeof(temp_buf));

	// SINKADJ & FLED
	sm5714_bulk_read(charger->i2c, SM5714_CHG_REG_SINKADJ, PRINT_CHG_REG2_NUM, reg2s);
	for (i = 0; i < PRINT_CHG_REG2_NUM; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5714_CHG_REG_SINKADJ + i, reg2s[i]);
		if (i % 9 == 8) {
			pr_info("sm5714-charger: regmap: %s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
	pr_info("sm5714-charger: regmap: %s\n", temp_buf);

}

static int sm5714_chg_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(sm5714_charger_attrs); i++) {
		rc = device_create_file(dev, &sm5714_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sm5714_charger_attrs[i]);
	return rc;
}

ssize_t sm5714_chg_show_attrs(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5714_charger_data *charger =	power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sm5714_charger_attrs;
	int i = 0;
	u8 addr, reg_data;

	switch (offset) {
	case CHIP_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", "SM5714");
		break;
	case DATA:
		for (addr = 0x07; addr <= 0x26; addr++) {
			sm5714_read_reg(charger->i2c, addr, &reg_data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"0x%02X[0x%02X], ", addr, reg_data);
		}
		for (addr = 0x40; addr <= 0x50; addr++) {
			sm5714_read_reg(charger->i2c, addr, &reg_data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"0x%02X[0x%02X], ", addr, reg_data);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i,
					"\n");
		break;
	case DATA_1:
		sm5714_read_reg(charger->i2c, charger->read_reg, &reg_data);
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%02X : 0x%02X\n", charger->read_reg, reg_data);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t sm5714_chg_store_attrs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sm5714_charger_data *charger =	power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sm5714_charger_attrs;
	int ret = 0;
	int x, y;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= 0x07 && x <= 0x50) {
				u8 addr = x;
				u8 data = y;

				if (sm5714_update_reg(charger->i2c, addr, data, 0xFF) < 0) {
					dev_info(charger->dev,
						"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(charger->dev,
					"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case DATA_1:
		if (sscanf(buf, "0x%8x", &x) == 1)
			charger->read_reg = x;
		ret = count;
		break;
#if defined(ENABLE_SM5714_ENBYPASS_MODE)
	case EN_BYPASS_MODE:
		chg_set_en_bypass_mode(charger, 1);
		break;
#endif
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int psy_chg_get_online(struct sm5714_charger_data *charger)
{
	u8 reg;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS1, &reg);

	return (reg & 0x1) ? 1 : 0;
}

static int psy_chg_get_status(struct sm5714_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_st1, reg_st2, reg_st3;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS1, &reg_st1);
	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg_st2);
	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS3, &reg_st3);
	dev_info(charger->dev, "%s: STATUS1(0x%02x), STATUS2(0x%02x), STATUS3(0x%02x)\n",
		__func__, reg_st1, reg_st2, reg_st3);

	if (reg_st2 & (0x1 << 5)) { /* check: Top-off */
		status = POWER_SUPPLY_STATUS_FULL;
	} else if (reg_st2 & (0x1 << 3)) {  /* check: Charging ON */
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (reg_st1 & (0x1 << 0)) { /* check: VBUS_POK */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5714_charger_data *charger)
{
	u8 reg;
	int health = POWER_SUPPLY_HEALTH_GOOD;

	if (charger->is_charging)
		chg_set_wdt_tmr_reset(charger);

	chg_print_regmap(charger);  /* please keep this log message */

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS1, &reg);

	if (reg & (0x1 << 0)) {
		charger->unhealth_cnt = 0;
		health = POWER_SUPPLY_HEALTH_GOOD;
	} else {
		if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT) {
			health = POWER_SUPPLY_HEALTH_GOOD;
			charger->unhealth_cnt++;
		} else {
			if (reg & (0x1 << 2))
				health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			else if (reg & (0x1 << 1))
				health = POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE;
		}
	}

	return health;
}

static int psy_chg_get_charge_type(struct sm5714_charger_data *charger)
{
	int charge_type;

	if (charger->is_charging) {
		if (charger->slow_rate_chg_mode) {
			dev_info(charger->dev, "%s: slow rate charge mode\n", __func__);
			charge_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		} else {
			charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
	} else {
		charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return charge_type;
}

static int psy_chg_get_present(struct sm5714_charger_data *charger)
{
	u8 reg;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg);

	return (reg & (0x1 << 2)) ? 0 : 1;
}

static int sm5714_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sm5714_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg = 0;

#if defined(CONFIG_NO_BATTERY)
	return 0;
#endif

	dev_info(charger->dev, "%s: psp=%d\n", __func__, psp);

	if (atomic_read(&charger->shutdown_cnt) > 0) {
		dev_info(charger->dev, "%s: charger already shutdown\n", __func__);
		return -EINVAL;
	}

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = psy_chg_get_online(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = psy_chg_get_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = psy_chg_get_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = psy_chg_get_charge_type(charger);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = chg_get_regulation_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = psy_chg_get_present(charger);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			chg_print_regmap(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
			val->intval = chg_get_en_shipmode(charger);
			pr_info("%s: manual ship mode set as %s\n", __func__, val->intval ? "enable" : "disable");
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = charger->charge_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
			if (sm5714_read_reg(charger->i2c, SM5714_CHG_REG_DEVICEID, &reg) >= 0) {
				val->intval = (reg &= 0x3);
				pr_info("%s: SM5714_CHG_DEVICEID 0x%x\n", __func__, reg);
			} else {
				val->intval = 0;
				pr_info("%s: SM5714_CHG_DEVICEID FAIL\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGER_IC_NAME:
			val->strval = "SM5714";
			break;
		case POWER_SUPPLY_EXT_PROP_ARI_CNT:
			val->intval = charger->ari_cnt;
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

static void sm5714_chg_buck_control(struct sm5714_charger_data *charger, bool buck_on)
{
#if defined(CONFIG_NO_BATTERY)
	buck_on = true;
#endif

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip buck_control\n", __func__);
		return;
	}

	if (buck_on) {
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_SUSPEND, 0);
		usleep_range(10000, 11000);	/* for BUCK start-up time */
	} else {
		chg_set_enq4fet(charger, 1);
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_SUSPEND, 1);
		chg_set_enq4fet(charger, 0);
	}

	pr_info("%s: buck status(%d)\n", __func__, buck_on);

}

static void sm5714_chg_charging(struct sm5714_charger_data *charger, int chg_en)
{
	u8 reg;

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip chg charging\n", __func__);
		return;
	}

	if (chg_en) {
		sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg);
		if (reg & 0x80) { /* reset wdt expired status and re-init wdt */
			chg_set_wdtcntl_reset(charger);
			chg_set_wdt_timer(charger, WDT_TIME_S_90);
		}
	}

	chg_set_enq4fet(charger, chg_en);
	chg_set_wdt_enable(charger, chg_en);

	pr_info("%s: charging en(%d)\n", __func__, chg_en);
}

/*
 * To prevent ibus peak, when switching DC -> SWC.
 * autodown_vbatreg_work  : Decrease vbatreg by (offset= vbatreg - vnow) , when switching DC -> SWC.
 * sm5714_chg_vbatreg_recovery : Restore vbatreg after the 1st topoff.
 */
static void sm5714_chg_vbatreg_recovery(struct sm5714_charger_data *charger)
{
	u16 before_batreg = 0;

	if (!charger->pdata->chg_float_voltage_down_en)
		return;

	charger->pdata->chg_float_voltage_down_offset_mv = 0;

	before_batreg = chg_get_regulation_voltage(charger);
	dev_info(charger->dev, "float voltage recovery [%dmV] -> [%dmV]\n",
				before_batreg, charger->pdata->chg_float_voltage);

	chg_set_batreg(charger, charger->pdata->chg_float_voltage);
}

static void autodown_vbatreg_work(struct work_struct *work)
{
	struct sm5714_charger_data *charger =
		container_of(work, struct sm5714_charger_data, vbatreg_autodown_work.work);
	union power_supply_propval value;
	int aub_voltage = 0, set_voltage = 0;

	if ((charger->cable_type != SEC_BATTERY_CABLE_PDIC_APDO) ||
		(charger->pdata->chg_float_voltage_down_offset_mv != 0)) {
		charger->autodown_cnt = 0;
		__pm_relax(charger->vbatreg_autodown_ws);
		return;
	}

	/* true is direct charger init done */
	psy_do_property("battery", get,	POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);

	/* check condition : switching DC to SWC */
	if ((!value.intval) ||
		(charger->pre_charge_mode != SEC_BAT_CHG_MODE_BUCK_OFF) ||
		(charger->charge_mode != SEC_BAT_CHG_MODE_CHARGING)) {
		charger->autodown_cnt = 0;
		__pm_relax(charger->vbatreg_autodown_ws);
		return;
	}

	charger->autodown_cnt++;

	psy_do_property("sm5714-fuelgauge", get,	POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	dev_info(charger->dev, "[%d]VNOW_READ[%d mV]\n", charger->autodown_cnt, value.intval);


	aub_voltage = value.intval - charger->pdata->chg_float_voltage;

	charger->pdata->chg_float_voltage_down_offset_mv =
		((aub_voltage > 10) ? 20 : (aub_voltage > 0) ? 10 : 0);

	if ((charger->pdata->chg_float_voltage_down_offset_mv == 0) && (charger->autodown_cnt < 5)) {
		queue_delayed_work(charger->wqueue, &charger->vbatreg_autodown_work, msecs_to_jiffies(2000));
		return;
	}

	dev_info(charger->dev, "[%d]Vnow[%d mV], stepdown_offset_mv[%d mV]\n",
			charger->autodown_cnt, value.intval, charger->pdata->chg_float_voltage_down_offset_mv);
	set_voltage = (charger->pdata->chg_float_voltage - charger->pdata->chg_float_voltage_down_offset_mv);

	__pm_relax(charger->vbatreg_autodown_ws);
	chg_set_batreg(charger, set_voltage);
	charger->autodown_cnt = 0;
}

static void psy_chg_set_charging_enable(struct sm5714_charger_data *charger, int charge_mode)
{
	int buck_off = false;
	bool buck_off_status =
		(sm5714_charger_oper_get_current_status() & (0x1 << SM5714_CHARGER_OP_EVENT_SUSPEND)) ? 1 : 0;

	dev_info(charger->dev, "charger_mode changed [%d] -> [%d]\n", charger->charge_mode, charge_mode);
	charger->pre_charge_mode = charger->charge_mode;
	charger->charge_mode = charge_mode;

	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip charging enable Control\n", __func__);
		return;
	}

	switch (charger->charge_mode) {
	case SEC_BAT_CHG_MODE_BUCK_OFF:
		buck_off = true;
		charger->is_charging = false;
		break;
	case SEC_BAT_CHG_MODE_CHARGING_OFF:
		charger->is_charging = false;
		break;
	case SEC_BAT_CHG_MODE_CHARGING:
		charger->is_charging = true;
		break;
	}

	sm5714_chg_charging(charger, charger->is_charging);

	if (buck_off != buck_off_status)
		sm5714_chg_buck_control(charger, (!buck_off));
}

static bool sm5714_irq_enable(int irq, bool en)
{
	bool ret = false;

	if (irq <= 0)
		return ret;

	if (en && irqd_irq_disabled(&irq_to_desc(irq)->irq_data)) {
		enable_irq(irq);
		ret = true;
	} else if (!en && !irqd_irq_disabled(&irq_to_desc(irq)->irq_data)) {
		disable_irq_nosync(irq);
		ret = true;
	}
	pr_info("%s : irq: %d, en: %d, st: %d\n", __func__, irq, en,
		irqd_irq_disabled(&irq_to_desc(irq)->irq_data));

	return ret;
}

static void sm5714_aicl_irq_enable(struct sm5714_charger_data *charger,
				bool en)
{
	u8 reg_data = 0;
	bool ret = false;

	ret = sm5714_irq_enable(charger->irq_aicl, en);

	if (ret) {
		sm5714_read_reg(charger->i2c, SM5714_CHG_REG_INTMSK2, &reg_data);
		pr_info("%s: %s aicl : 0x%x\n",
			__func__, en ? "enable" : "disable", reg_data);
	}
}

static void psy_chg_set_online(struct sm5714_charger_data *charger, int cable_type)
{
	dev_info(charger->dev, "[start] cable_type(%d->%d), op_mode(%d), op_status(0x%x)",
			charger->cable_type, cable_type, sm5714_charger_oper_get_current_op_mode(),
			sm5714_charger_oper_get_current_status());

	charger->slow_rate_chg_mode = false;
	charger->cable_type = cable_type;

	if (charger->pdata->boosting_voltage_aicl)
		sm5714_aicl_irq_enable(charger, true);

	if (charger->cable_type == SEC_BATTERY_CABLE_NONE ||
			charger->cable_type == SEC_BATTERY_CABLE_UNKNOWN) {
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_VBUSIN, 0);
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_PWR_SHAR, 0);
		sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_USB_OTG, 0);

		/* set default input current */
		chg_set_input_current_limit(charger, 500);

		if (!charger->pdata->boosting_voltage_aicl)
			sm5714_aicl_irq_enable(charger, true);
	} else {
		if (charger->cable_type != SEC_BATTERY_CABLE_OTG &&
			charger->cable_type != SEC_BATTERY_CABLE_POWER_SHARING)
			sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_VBUSIN, 1);

		if (is_hv_wire_type(charger->cable_type) ||
			(charger->cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
			if (!charger->pdata->boosting_voltage_aicl) {
				sm5714_aicl_irq_enable(charger, false);
				cancel_delayed_work_sync(&charger->aicl_work);
				__pm_relax(charger->aicl_ws);
				charger->slow_rate_chg_mode = false;
			}
		}
	}

	dev_info(charger->dev, "[end] op_mode(%d), op_status(0x%x)\n",
			sm5714_charger_oper_get_current_op_mode(),
			sm5714_charger_oper_get_current_status());
}

static void psy_chg_set_otg_control(struct sm5714_charger_data *charger, bool enable)
{
	if (enable == charger->otg_on)
		return;

	sm5714_charger_oper_push_event(SM5714_CHARGER_OP_EVENT_USB_OTG, enable);
	charger->otg_on = enable;
	power_supply_changed(charger->psy_otg);
}

static int sm5714_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5714_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	dev_info(charger->dev, "%s: psp=%d\n", __func__, psp);

	if (atomic_read(&charger->shutdown_cnt) > 0) {
		dev_info(charger->dev, "%s: charger already shutdown\n", __func__);
		return -EINVAL;
	}

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		psy_chg_set_online(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		dev_info(charger->dev, "input limit changed [%dmA] -> [%dmA]\n",
			charger->input_current, val->intval);
		chg_set_input_current_limit(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		dev_info(charger->dev, "charging current changed [%dmA] -> [%dmA]\n",
			charger->charging_current, val->intval);
		charger->charging_current = val->intval;
		chg_set_charging_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		dev_info(charger->dev, "float voltage changed [%dmV] -> [%dmV]\n",
			charger->pdata->chg_float_voltage, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		chg_set_batreg(charger, charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* if jig attached, */
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	{
		int get_topoff_curr = 0;

		get_topoff_curr = chg_get_topoff_current(charger);
		if (val->intval < get_topoff_curr)
			sm5714_chg_vbatreg_recovery(charger);

		chg_set_topoff_current(charger, val->intval);
	}
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
	{
		u8 reg;
		sm5714_init_aicl_irq(charger);
		sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg);
		if (reg & (0x1 << 0))
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
				msecs_to_jiffies(charger->pdata->aicl_work_delay));
	}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION:
		{
			u8 offset;
			u16 float_voltage;

			float_voltage = val->intval;
			pr_info("%s: factory voltage regulation (%d)\n", __func__, float_voltage);
			/*chg_set_batreg(charger, val->intval);*/
			if (float_voltage <= 3700)
				offset = 0x0;
			else if (float_voltage < 3900)
				offset = ((float_voltage - 3700) / 50); /* BATREG = 3.70 ~ 3.85V in 0.05V steps */
			else if (float_voltage < 4050)
				offset = (((float_voltage - 3900) / 100) + 4); /* BATREG = 3.90, 4.0V in 0.1V steps */
			else if (float_voltage < 4630)
				offset = (((float_voltage - 4050) / 10) + 6); /* BATREG = 4.05 ~ 4.62V in 0.01V steps */
			else {
				dev_err(charger->dev, "%s: can't support BATREG at over voltage 4.62V (mV=%d)\n",
					__func__, float_voltage);
				offset = 0x15;    /* default Offset : 4.2V */
			}
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CHGCNTL4, ((offset & 0x3F) << 0), (0x3F << 0));
		}
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			pr_info("%s: bypass mode is %s\n", __func__, val->intval ? "enable" : "disable");
			chg_set_en_bypass_mode(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
			pr_info("%s: manual ship mode is %s\n", __func__, val->intval ? "enable" : "disable");
			chg_set_en_shipmode(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			psy_chg_set_charging_enable(charger, val->intval);
			if (charger->pdata->chg_float_voltage_down_en) {
				__pm_stay_awake(charger->vbatreg_autodown_ws);
				queue_delayed_work(charger->wqueue,
						&charger->vbatreg_autodown_work, msecs_to_jiffies(2000));
			}
			chg_print_regmap(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION:
			if (val->intval)
				chg_set_en_bypass_mode(charger, val->intval);
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
#if defined(CONFIG_SEC_FACTORY)
		case POWER_SUPPLY_EXT_PROP_NOZX_CTRL:
			if (charger->is_sm5714a) {
				dev_info(charger->dev, "%s: NOZX is %s\n",
					__func__, (!val->intval) ? "Enable" : "Disable");
				chg_set_en_nozx(charger, val->intval);
			} else {
				dev_info(charger->dev, "%s: not support nozx ctrl via sysfs",
					__func__);
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

static int sm5714_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sm5714_charger_data *charger =
		power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->otg_on;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sm5714_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sm5714_charger_data *charger =
		power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		dev_info(charger->dev, "%s: OTG %s\n", __func__,
			val->intval ? "ON" : "OFF");
		psy_chg_set_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("POWER_SUPPLY_PROP_VOLTAGE_MAX, set otg current limit %dmA\n", (val->intval) ? 1500 : 900);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline u8 _calc_limit_current_offset_to_mA(unsigned short mA)
{
	unsigned char offset;

	if (mA < 100)
		offset = 0x00;
	else
		offset = ((mA - 100) / 25) & 0x7F;

	return offset;
}

static inline int _reduce_input_limit_current(struct sm5714_charger_data *charger)
{
	int input_limit = chg_get_input_current_limit(charger);
	u8 offset = _calc_limit_current_offset_to_mA(input_limit - REDUCE_CURRENT_STEP);

	sm5714_update_reg(charger->i2c, SM5714_CHG_REG_VBUSCNTL, ((offset & 0x7F) << 0), (0x7F << 0));

	charger->input_current = chg_get_input_current_limit(charger);

	dev_info(charger->dev, "reduce input-limit: [%dmA] to [%dmA]\n",
			input_limit, charger->input_current);

	return charger->input_current;
}

static inline void _check_slow_rate_charging(struct sm5714_charger_data *charger)
{
	union power_supply_propval value;

	if (charger->input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
			charger->cable_type != SEC_BATTERY_CABLE_NONE) {

		dev_info(charger->dev, "slow-rate charging on : input current(%dmA), cable-type(%d)\n",
			charger->input_current, charger->cable_type);

		charger->slow_rate_chg_mode = true;
		psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	}

	dev_info(charger->dev, "%s - done\n", __func__);
}


static void aicl_work(struct work_struct *work)
{
	struct sm5714_charger_data *charger = container_of(work, struct sm5714_charger_data, aicl_work.work);
	int input_limit;
	bool aicl_on = false;
	u8 reg, aicl_cnt = 0, sleep_cnt = 0;

	while (charger->cable_type == SEC_BATTERY_CABLE_NONE && sleep_cnt++ < 4) {
		dev_info(charger->dev, "%s - CABLE_NONE.. waiting 250ms %d\n", __func__, sleep_cnt);
		msleep(250); // 250ms Delay for Cable Work to be completed.
	}

	dev_info(charger->dev, "%s - start\n", __func__);

	mutex_lock(&charger->charger_mutex);
	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg);
	while ((reg & (0x1 << 0)) && charger->cable_type != SEC_BATTERY_CABLE_NONE &&
		!irqd_irq_disabled(&irq_to_desc(charger->irq_aicl)->irq_data)) {
		if (++aicl_cnt >= 2) {
			input_limit = _reduce_input_limit_current(charger);
			aicl_on = true;
			if (input_limit <= MINIMUM_INPUT_CURRENT) {
				sm5714_aicl_irq_enable(charger, false);
				break;
			}
			aicl_cnt = 0;
		}
		msleep(50);
		sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS2, &reg);
		dev_info(charger->dev, "%s - STATUS2 [0x%x]\n", __func__, reg);
	}

	mutex_unlock(&charger->charger_mutex);

	dev_info(charger->dev, "%s - aicl_on(%d)\n", __func__, aicl_on);
	if (aicl_on) {
		union power_supply_propval value;

		value.intval = input_limit;
		psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
	}
	_check_slow_rate_charging(charger);

	__pm_relax(charger->aicl_ws);

	dev_info(charger->dev, "%s - done\n", __func__);
}

static irqreturn_t chg_vbuspok_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_aicl_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	__pm_stay_awake(charger->aicl_ws);
	queue_delayed_work(charger->wqueue, &charger->aicl_work, msecs_to_jiffies(charger->pdata->aicl_work_delay));

	return IRQ_HANDLED;
}

static void sm5714_init_aicl_irq(struct sm5714_charger_data *charger)
{
	int ret;

	charger->irq_aicl = charger->sm5714_pdata->irq_base + SM5714_CHG_IRQ_INT2_AICL;
	ret = request_threaded_irq(charger->irq_aicl, NULL,
			chg_aicl_isr, 0, "aicl-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request aicl IRQ: %d: %d\n",
		       __func__, charger->irq_aicl, ret);
	}
	pr_info("%s: %d\n", __func__, irqd_irq_disabled(&irq_to_desc(charger->irq_aicl)->irq_data));
}

static irqreturn_t chg_done_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);
	if (sm5714_get_facmode()) {
		pr_info("%s: Factory Mode Skip chg done\n", __func__);
		return IRQ_HANDLED;
	}

	/* Toggle ENQ4FET for Re-cycling charger loop */
	chg_set_enq4fet(charger, 0);
	usleep_range(10000, 11000);
	chg_set_enq4fet(charger, 1);

	return IRQ_HANDLED;
}

static irqreturn_t chg_vsysovp_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t chg_vbusuvlo_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;
	u8 reg;

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_FACTORY1, &reg);
	if (reg & 0x02) {
		dev_info(charger->dev, "%s: bypass mode enabled\n",
			__func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t chg_otgfail_isr(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;
	u8 reg;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;

	o_notify = get_otg_notify();
#endif

	dev_info(charger->dev, "%s: irq=%d\n", __func__, irq);

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_STATUS3, &reg);
	if (reg & 0x04) {
		dev_info(charger->dev, "%s: otg overcurrent limit\n",
			__func__);
		/* send otg ocp noti */
#ifdef CONFIG_USB_HOST_NOTIFY
		if (o_notify)
			send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
		psy_chg_set_otg_control(charger, false);
	}

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_USE_POGO)
static irqreturn_t pogo_irq_thread(int irq, void *data)
{
	struct sm5714_charger_data *charger = data;

	pr_info("%s: irq(%d), pogo_int(%d)\n", __func__,
		irq, gpio_get_value(charger->pdata->gpio_pogo_int));

	__pm_stay_awake(charger->pogo_det_ws);
	queue_delayed_work(charger->wqueue, &charger->pogo_detect_work, msecs_to_jiffies(50));

	return IRQ_HANDLED;
}

static void pogo_init_work(struct work_struct *work)
{
	struct sm5714_charger_data *charger =
		container_of(work, struct sm5714_charger_data, pogo_init_work.work);
	int ret = 0;

	ret = request_threaded_irq(charger->pdata->irq_pogo_int, NULL,
		pogo_irq_thread, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
		IRQF_ONESHOT, "charger-pogo-int", charger);
	if (ret < 0)
		pr_err("%s: failed to request pogo_int irq(ret = %d)\n", __func__, ret);

	pr_info("%s: pogo_int(%d)\n", __func__,
		gpio_get_value(charger->pdata->gpio_pogo_int));

	__pm_stay_awake(charger->pogo_det_ws);
	queue_delayed_work(charger->wqueue, &charger->pogo_detect_work, msecs_to_jiffies(50));

	dev_info(charger->dev, "%s - done\n", __func__);
}

static void pogo_detect_work(struct work_struct *work)
{
	struct sm5714_charger_data *charger =
		container_of(work, struct sm5714_charger_data, pogo_detect_work.work);
	union power_supply_propval val = {0, };
	int vbus;

	dev_info(charger->dev, "%s - start\n", __func__);

	if (!gpio_get_value(charger->pdata->gpio_pogo_int)) {
		vbus = sm5714_muic_get_vbus_voltage();
		val.intval = (vbus >= 8000 ? 2 : 1);
		pr_info("%s: vbus(%d)\n", __func__, vbus);
	} else
		val.intval = 0;

	psy_do_property("pogo", set, POWER_SUPPLY_PROP_ONLINE, val);
	__pm_relax(charger->pogo_det_ws);

	dev_info(charger->dev, "%s - done\n", __func__);
}
#endif

static inline void sm5714_chg_init(struct sm5714_charger_data *charger)
{
	chg_set_aicl(charger, 1, AICL_TH_V_4_5);
	chg_set_ocp_current(charger, charger->pdata->chg_ocp_current);
	chg_set_batreg(charger, charger->pdata->chg_float_voltage);
	chg_set_wdt_timer(charger, WDT_TIME_S_90);
	chg_set_topoff_timer(charger, TOPOFF_TIME_M_45);
	chg_set_autostop(charger, 1);
	chg_set_auto_shipmode(charger, AUTO_SHIP_MODE_VREF_V_2_6);
	chg_set_auto_shipmode_time(charger, AUTO_SHIP_MODE_TIME_S_4_0);
	chg_set_lxslope(charger, charger->pdata->chg_lxslope);
	chg_print_regmap(charger);

	dev_info(charger->dev, "%s: init done.\n", __func__);
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s: IRQ(%d):%s free done\n", \
				__func__, _irq, _name);			\
	}								\
} while (0)

static void sm5714_charger_free_irqs(struct sm5714_charger_data *charger)
{
	pr_info("%s: start\n", __func__);

	/* free Charger IRQ */
	FREE_IRQ(charger->irq_vbuspok, charger, "vbuspok-irq");
	FREE_IRQ(charger->irq_done, charger, "done-irq");
	FREE_IRQ(charger->irq_vsysovp, charger, "vsysovp-irq");
	FREE_IRQ(charger->irq_vbusuvlo, charger, "vbusuvlo-irq");
	FREE_IRQ(charger->irq_otgfail, charger, "irq_otgfail-irq");
	FREE_IRQ(charger->irq_aicl, charger, "aicl-irq-irq");

	pr_info("%s: end\n", __func__);
}

static int sm5714_charger_parse_dt(struct device *dev,
	struct sm5714_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "sm5714-charger");
	struct device_node *spss_region_dn;
	struct sm5714_charger_data *charger = container_of(&pdata, struct sm5714_charger_data, pdata);
	int ret = 0;

	spss_region_dn = of_find_node_by_name(NULL, "qcom,spcom");
	if (spss_region_dn == NULL) {
#if IS_ENABLED(CONFIG_QCOM_SPSS)
		pr_info("[%s] coudln't find qcom,spcom, config enabled\n", __func__);
#else
		pr_info("[%s] coudln't find qcom,spcom\n", __func__);
#endif
		charger->spcom = false;
	} else {
		charger->spcom = true;
		pr_info("[%s] found spcom\n", __func__);
	}

	ret = of_property_read_u32(np, "sm5714,chg_lxslope",
					&pdata->chg_lxslope);
	if (ret) {
		pr_info("%s: sm5714,chg_lxslope is Empty\n", __func__);
		pdata->chg_lxslope = 1; /* b01 : default */
	}
	pr_info("%s: sm5714,chg_lxslope is %d\n", __func__,
		pdata->chg_lxslope);

#if IS_ENABLED(CONFIG_USE_POGO)
	ret = of_get_named_gpio(np, "charger,pogo_int", 0);
	if (ret >= 0) {
		pdata->gpio_pogo_int = ret;
		pdata->irq_pogo_int = gpio_to_irq(pdata->gpio_pogo_int);
		pr_info("%s: gpio_pogo_int = 0x%x, irq_pogo_int = 0x%x\n",
			__func__, pdata->gpio_pogo_int, pdata->irq_pogo_int);
	} else
		pr_err("%s: failed to get pogo_int\n", __func__);
#endif

	pdata->chg_float_voltage_down_en = of_property_read_bool(np, "sm5714,chg_float_voltage_down_en");
	pr_info("%s: sm5714,chg_float_voltage_down_en is %d\n", __func__,
		pdata->chg_float_voltage_down_en);

	if (pdata->chg_float_voltage_down_en) {
		ret = of_property_read_u32(np, "sm5714,chg_float_voltage_down_offset_mv",
				&pdata->chg_float_voltage_down_offset_mv);
	}
	ret = of_property_read_u32(np, "sm5714,aicl_work_delay", &pdata->aicl_work_delay);
	if (ret) {
		pdata->aicl_work_delay = 50; /*50 ms is default value*/
	}

	/* The offset value is 0 when vbatreg auto-down, regardless of the dt file setting. */
	pdata->chg_float_voltage_down_offset_mv = 0;

	pr_info("%s: sm5714,chg_float_voltage_down_offset_mv is %d\n", __func__,
		pdata->chg_float_voltage_down_offset_mv);

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		dev_err(dev, "%s: can't find battery node\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
						&pdata->chg_float_voltage);
		if (ret) {
			dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4350;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n",
			__func__, pdata->chg_float_voltage);

		ret = of_property_read_u32(np, "battery,chg_ocp_current",
					&pdata->chg_ocp_current);
		if (ret) {
			pr_info("%s: battery,chg_ocp_current is Empty\n", __func__);
			pdata->chg_ocp_current = 5400; /* mA */
		}
		pr_info("%s: battery,chg_ocp_current is %d\n", __func__,
			pdata->chg_ocp_current);

		pdata->boosting_voltage_aicl = of_property_read_bool(np,
			     "battery,boosting_voltage_aicl");

		pdata->ovp_bypass_mode = of_property_read_bool(np,
			     "battery,ovp_bypass_mode");
	}

	dev_info(dev, "%s: parse dt done.\n", __func__);
	return 0;
}

/* if need to set sm5714 pdata */
static const struct of_device_id sm5714_charger_match_table[] = {
	{ .compatible = "samsung,sm5714-charger",},
	{},
};

static const struct power_supply_desc sm5714_charger_power_supply_desc = {
	.name           = "sm5714-charger",
	.type           = POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property   = sm5714_chg_get_property,
	.set_property   = sm5714_chg_set_property,
	.properties     = sm5714_charger_props,
	.num_properties = ARRAY_SIZE(sm5714_charger_props),
};

static char *sm5714_otg_supply_list[] = {
	"otg",
};

static const struct power_supply_desc sm5714_otg_power_supply_desc = {
	.name			= "sm5714-otg",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5714_otg_get_property,
	.set_property	= sm5714_otg_set_property,
	.properties		= sm5714_otg_props,
	.num_properties	= ARRAY_SIZE(sm5714_otg_props),
};

static int sm5714_charger_probe(struct platform_device *pdev)
{
	struct sm5714_dev *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_platform_data *pdata = dev_get_platdata(sm5714->dev);
	struct sm5714_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	u8 reg_data = 0, reg_data1 = 0, reg_data2 = 0, reg_data3 = 0, reg_data4 = 0;

	dev_info(&pdev->dev, "%s: probe start\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->dev = &pdev->dev;
	charger->i2c = sm5714->charger;
	charger->otg_on = false;
	charger->sm5714_pdata = pdata;
	charger->ari_cnt = 0;
	atomic_set(&charger->shutdown_cnt, 0);
	mutex_init(&charger->charger_mutex);

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = sm5714_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);
	/*Check ifpmic SM5714: 0x08, SM5714A: 0x0C*/
	charger->is_sm5714a = false;
	sm5714_read_reg(charger->i2c, 0x51, &reg_data);
	if (reg_data == 0x0C)
		charger->is_sm5714a = true;

	sm5714_chg_init(charger);
	sm5714_charger_oper_table_init(sm5714);

	charger->input_current = chg_get_input_current_limit(charger);
	charger->charging_current = chg_get_charging_current(charger);

	/* W/A : for Q3 option bit write */
	sm5714_read_reg(charger->i2c, 0xEA, &reg_data1);
	sm5714_read_reg(charger->i2c, 0xED, &reg_data2);
	sm5714_read_reg(charger->i2c, 0xE4, &reg_data3);
	sm5714_read_reg(charger->i2c, 0xCB, &reg_data4);
	dev_info(&pdev->dev, "%s: read sm5714 option bits [0x%X,0x%X,0x%X,0x%X]\n",
		__func__, reg_data1, reg_data2, reg_data3, reg_data4);

	if ((reg_data1 != 0x93) || (reg_data2 != 0x10) || (reg_data3 != 0x9E) || (reg_data4 != 0x80)) {
		sm5714_write_reg(charger->i2c, 0x51, 0xEA);
		sm5714_write_reg(charger->i2c, 0x51, 0xAE);
		sm5714_update_reg(charger->i2c, 0x6B, (0x1 << 2), (0x1 << 2));
		sm5714_write_reg(charger->i2c, 0x4C, 0xFF);
		sm5714_write_reg(charger->i2c, 0x57, 0x20);
		sm5714_write_reg(charger->i2c, 0x49, 0xE8);
		sm5714_write_reg(charger->i2c, 0x4A, 0x02);
		sm5714_write_reg(charger->i2c, 0x49, 0xCB);
		sm5714_write_reg(charger->i2c, 0x4A, 0x80);
		sm5714_write_reg(charger->i2c, 0x49, 0xDA);
		sm5714_write_reg(charger->i2c, 0x4A, 0x00);
		sm5714_write_reg(charger->i2c, 0x49, 0xEA);
		sm5714_write_reg(charger->i2c, 0x4A, 0x93);
		sm5714_write_reg(charger->i2c, 0x49, 0xED);
		sm5714_write_reg(charger->i2c, 0x4A, 0x10);
		sm5714_write_reg(charger->i2c, 0x49, 0xE4);
		sm5714_write_reg(charger->i2c, 0x4A, 0x9E);
		sm5714_write_reg(charger->i2c, 0x4C, 0x3F);
		sm5714_write_reg(charger->i2c, 0x57, 0x00);
		sm5714_update_reg(charger->i2c, 0x6B, (0x0 << 2), (0x1 << 2));
		sm5714_write_reg(charger->i2c, 0x51, 0x00);
		dev_info(&pdev->dev, "%s: option bit write all\n", __func__);
	}

	// re-read, for check write.
	sm5714_read_reg(charger->i2c, 0xEA, &reg_data1);
	sm5714_read_reg(charger->i2c, 0xED, &reg_data2);
	sm5714_read_reg(charger->i2c, 0xE4, &reg_data3);
	sm5714_read_reg(charger->i2c, 0xCB, &reg_data4);
	dev_info(&pdev->dev, "%s: again read sm5714 option bits [0x%X,0x%X,0x%X,0x%X]\n",
		__func__, reg_data1, reg_data2, reg_data3, reg_data4);

	/* Re-cycle Buck contdition */
	sm5714_chg_buck_control(charger, 0);
	sm5714_chg_buck_control(charger, 1);

	/* Init work_queue, ws for Slow-rate-charging */
	charger->wqueue = create_singlethread_workqueue(dev_name(charger->dev));
	if (!charger->wqueue) {
		dev_err(charger->dev, "%s: fail to create workqueue\n", __func__);
		return -ENOMEM;
	}
	charger->slow_rate_chg_mode = false;
	INIT_DELAYED_WORK(&charger->aicl_work, aicl_work);
	charger->aicl_ws = wakeup_source_register(&pdev->dev, "charger-aicl");

	charger->autodown_cnt = 0;
	INIT_DELAYED_WORK(&charger->vbatreg_autodown_work, autodown_vbatreg_work);
	charger->vbatreg_autodown_ws = wakeup_source_register(&pdev->dev, "charger-vbatreg_autodown");

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = sm5714_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5714_supplied_to);

	charger->psy_chg = power_supply_register(&pdev->dev, &sm5714_charger_power_supply_desc, &psy_cfg);
	if (!charger->psy_chg) {
		dev_err(&pdev->dev, "%s: failed to power supply charger register", __func__);
		goto err_power_supply_register;
	}


	charger->psy_otg = power_supply_register(&pdev->dev, &sm5714_otg_power_supply_desc, &psy_cfg);
	if (!charger->psy_otg) {
		dev_err(&pdev->dev, "%s: failed to power supply otg register ", __func__);
		goto err_power_supply_register_otg;
	}
	charger->psy_otg->supplied_to = sm5714_otg_supply_list;
	charger->psy_otg->num_supplicants = ARRAY_SIZE(sm5714_otg_supply_list);

	ret = sm5714_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev, "%s : Failed to create_attrs\n", __func__);
		goto err_reg_irq;
	}

	/* Request IRQs */
	charger->irq_vbuspok = pdata->irq_base + SM5714_CHG_IRQ_INT1_VBUSPOK;
	ret = request_threaded_irq(charger->irq_vbuspok, NULL,
			chg_vbuspok_isr, 0, "vbuspok-irq", charger);
	if (ret < 0) {
		dev_err(sm5714->dev, "%s: fail to request vbuspok-irq:%d (ret=%d)\n",
					__func__, charger->irq_vbuspok, ret);
		goto err_reg_irq;
	}

	charger->irq_done = pdata->irq_base + SM5714_CHG_IRQ_INT2_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL,
			chg_done_isr, 0, "done-irq", charger);
	if (ret < 0) {
		dev_err(sm5714->dev, "%s: fail to request done-irq:%d (ret=%d)\n",
			__func__, charger->irq_done, ret);
		goto err_reg_irq;
	}

	charger->irq_vsysovp = pdata->irq_base + SM5714_CHG_IRQ_INT3_VSYSOVP;
	ret = request_threaded_irq(charger->irq_vsysovp, NULL,
			chg_vsysovp_isr, 0, "vsysovp-irq", charger);
	if (ret < 0) {
		dev_err(sm5714->dev, "%s: fail to request vsysovp-irq:%d (ret=%d)\n",
			__func__, charger->irq_vsysovp, ret);
		goto err_reg_irq;
	}

	charger->irq_vbusuvlo = pdata->irq_base + SM5714_CHG_IRQ_INT1_VBUSUVLO;
	ret = request_threaded_irq(charger->irq_vbusuvlo, NULL,
			chg_vbusuvlo_isr, 0, "vbusuvlo-irq", charger);
	if (ret < 0) {
		dev_err(sm5714->dev, "%s: fail to request vbusuvlo-irq:%d (ret=%d)\n",
			__func__, charger->irq_vbusuvlo, ret);
		goto err_reg_irq;
	}

	charger->irq_otgfail = pdata->irq_base + SM5714_CHG_IRQ_INT3_OTGFAIL;
	ret = request_threaded_irq(charger->irq_otgfail, NULL,
			chg_otgfail_isr, 0, "otgfail-irq", charger);
	if (ret < 0) {
		dev_err(sm5714->dev, "%s: fail to request otgfail-irq:%d (ret=%d)\n",
			__func__, charger->irq_otgfail, ret);
		goto err_reg_irq;
	}

#if IS_ENABLED(CONFIG_USE_POGO)
	if (charger->pdata->irq_pogo_int) {
		INIT_DELAYED_WORK(&charger->pogo_init_work, pogo_init_work);
		INIT_DELAYED_WORK(&charger->pogo_detect_work, pogo_detect_work);
		charger->pogo_det_ws = wakeup_source_register(&pdev->dev, "charger-pogo-det");

		queue_delayed_work(charger->wqueue, &charger->pogo_init_work, msecs_to_jiffies(2000));
	}
	device_init_wakeup(charger->dev, 1);
#endif

	sec_chg_set_dev_init(SC_DEV_MAIN_CHG);

	dev_info(&pdev->dev, "%s: probe done[%s].\n", __func__, SM5714_CHARGER_VERSION);

	return 0;

err_reg_irq:
	sm5714_charger_free_irqs(charger);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register:
	power_supply_unregister(charger->psy_chg);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);
	return ret;
}

static int sm5714_charger_remove(struct platform_device *pdev)
{
	struct sm5714_charger_data *charger =
		platform_get_drvdata(pdev);

	sm5714_charger_free_irqs(charger);

	mutex_destroy(&charger->charger_mutex);

	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int sm5714_charger_suspend(struct device *dev)
{
#if defined(CONFIG_SEC_FACTORY) || IS_ENABLED(CONFIG_USE_POGO)
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5714_charger_data *charger = i2c_get_clientdata(i2c);
#endif

#if defined(CONFIG_SEC_FACTORY)
	u8 reg = 0;

	sm5714_read_reg(charger->i2c, SM5714_CHG_REG_FACTORY1, &reg);
	dev_info(charger->dev, "%s: REG_FACTORY1: 0x%02X\n", __func__, reg);
#endif

#if IS_ENABLED(CONFIG_USE_POGO)
	if (device_may_wakeup(dev))
		enable_irq_wake(charger->pdata->irq_pogo_int);

#if !defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_MEDIATEK)
	disable_irq(charger->pdata->irq_pogo_int);
#endif
#endif
	return 0;
}

static int sm5714_charger_resume(struct device *dev)
{
#if IS_ENABLED(CONFIG_USE_POGO)
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5714_charger_data *charger = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(charger->pdata->irq_pogo_int);

#if !defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_MEDIATEK)
	enable_irq(charger->pdata->irq_pogo_int);
#endif
#endif
	return 0;
}
#else
#define sm5714_charger_suspend NULL
#define sm5714_charger_resume NULL
#endif

static void sm5714_charger_shutdown(struct platform_device *pdev)
{
	struct sm5714_charger_data *charger =
		platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	atomic_inc(&charger->shutdown_cnt);

	sm5714_charger_free_irqs(charger);

	if (charger->i2c) {
#if defined(CONFIG_SHIPMODE_BY_VBAT) && !defined(CONFIG_SEC_FACTORY)
		if (charger->is_sm5714a) {
			chg_set_auto_shipmode_level(charger);
			chg_set_auto_shipmode_time(charger, AUTO_SHIP_MODE_TIME_S_4_0);
		}
#endif
		if (!sm5714_get_facmode()) {
			u8 reg;
			/* disable charger */
			chg_set_enq4fet(charger, false);
			sm5714_update_reg(charger->i2c, SM5714_CHG_REG_CNTL2, 0x05, 0x0F);
			/* set input current 500mA */
			chg_set_input_current_limit(charger, 500);
			/* disable bypass mode */
			sm5714_read_reg(charger->i2c, SM5714_CHG_REG_FACTORY1, &reg);
			if (reg & 0x02) {
				pr_info("%s: bypass mode is enabled\n", __func__);
				chg_set_en_bypass_mode(charger, false);
			}
		}
	} else {
		pr_err("%s: not sm5714 i2c client", __func__);
	}

	pr_info("%s: --\n", __func__);
}

static SIMPLE_DEV_PM_OPS(sm5714_charger_pm_ops, sm5714_charger_suspend,
		sm5714_charger_resume);

static struct platform_driver sm5714_charger_driver = {
	.driver = {
		.name = "sm5714-charger",
		.owner = THIS_MODULE,
		.of_match_table = sm5714_charger_match_table,
		.pm = &sm5714_charger_pm_ops,
	},
	.probe		= sm5714_charger_probe,
	.remove		= sm5714_charger_remove,
	.shutdown	= sm5714_charger_shutdown,
};

static int __init sm5714_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sm5714_charger_driver);

	return ret;
}
module_init(sm5714_charger_init);

static void __exit sm5714_charger_exit(void)
{
	platform_driver_unregister(&sm5714_charger_driver);
}
module_exit(sm5714_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for SM5714");
MODULE_VERSION(SM5714_CHARGER_VERSION);
