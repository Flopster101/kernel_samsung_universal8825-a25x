/*
 * s2mf301_charger.h - Header of S2MF301 Charger Driver
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

#ifndef S2MF301_CHARGER_H
#define S2MF301_CHARGER_H
#include <linux/mfd/samsung/s2mf301.h>

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#include <linux/power/s2m_chg_manager.h>

/* define function if need */
#define ENABLE_MIVR 0

/* define IRQ function if need */
#define EN_BAT_DET_IRQ 1
#define EN_CHG1_IRQ_CHGIN 0

/* Test debug log enable */
#define EN_TEST_READ 1

/* change VF_BST if need */
#define EN_VF_BST 0

#define HEALTH_DEBOUNCE_CNT 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

#define S2MF301_CHG_INT0		0x00
#define S2MF301_CHG_INT1		0x01
#define S2MF301_CHG_INT2		0x02
#define S2MF301_CHG_INT3		0x03
#define S2MF301_CHG_INT4		0x04

#define S2MF301_CHG_INT0M		0x05
#define S2MF301_CHG_INT1M		0x06
#define S2MF301_CHG_INT2M		0x07
#define S2MF301_CHG_INT3M		0x08
#define S2MF301_CHG_INT4M		0x09

#define S2MF301_CHG_STATUS0		0x0A
#define S2MF301_CHG_STATUS1		0x0B
#define S2MF301_CHG_STATUS2		0x0C
#define S2MF301_CHG_STATUS3		0x0D
#define S2MF301_CHG_STATUS4		0x0E
#define S2MF301_CHG_STATUS5		0x0F
#define S2MF301_CHG_STATUS6		0x10
#define S2MF301_CHG_STATUS7		0x11

#define S2MF301_CHG_CTRL0		0x18
#define S2MF301_CHG_CTRL1		0x19
#define S2MF301_CHG_CTRL2		0x1A
#define S2MF301_CHG_CTRL3		0x1B
#define S2MF301_CHG_CTRL4		0x1C
#define S2MF301_CHG_CTRL5		0x1D
#define S2MF301_CHG_CTRL6		0x1E
#define S2MF301_CHG_CTRL7		0x1F
#define S2MF301_CHG_CTRL8		0x20
#define S2MF301_CHG_CTRL9		0x21
#define S2MF301_CHG_CTRL10		0x22

#define S2MF301_CHG_CTRL11		0x23
#define S2MF301_CHG_CTRL12		0x24
#define S2MF301_CHG_CTRL13		0x25
#define S2MF301_CHG_CTRL14		0x26
#define S2MF301_CHG_CTRL15		0x27
#define S2MF301_CHG_CTRL16		0x28
#define S2MF301_CHG_CTRL17		0x29
#define S2MF301_CHG_CTRL18		0x2A
#define S2MF301_CHG_CTRL19		0x2B

/* S2MF301_CHG_STATUS0 */
#define CHG_UVLOB_STATUS_SHIFT	0
#define CHG_UVLOB_STATUS_MASK		BIT(CHG_UVLOB_STATUS_SHIFT)

#define CHG_IN2BAT_STATUS_SHIFT	1
#define CHG_IN2BAT_STATUS_MASK		BIT(CHG_IN2BAT_STATUS_SHIFT)

#define CHG_OVP_STATUS_SHIFT	2
#define CHG_OVP_STATUS_MASK		BIT(CHG_OVP_STATUS_SHIFT)

#define VBUS_DET_STATUS_SHIFT	3
#define VBUS_DET_STATUS_MASK		BIT(VBUS_DET_STATUS_SHIFT)

#define BATID_STATUS_SHIFT	4
#define BATID_STATUS_MASK		BIT(BATID_STATUS_SHIFT)

/* S2MF301_CHG_STATUS1 */
#define RE_CHARGING_STATUS_SHIFT	0
#define RE_CHARGING_STATUS_MASK		BIT(RE_CHARGING_STATUS_SHIFT)

#define DONE_STATUS_SHIFT		1
#define DONE_STATUS_MASK		BIT(DONE_STATUS_SHIFT)

#define TOP_OFF_STATUS_SHIFT		2
#define TOP_OFF_STATUS_MASK		BIT(TOP_OFF_STATUS_SHIFT)

#define CV_STATUS_SHIFT		3
#define CV_STATUS_MASK		BIT(CV_STATUS_SHIFT)

#define SC_STATUS_SHIFT		4
#define SC_STATUS_MASK		BIT(SC_STATUS_SHIFT)

#define LC_STATUS_SHIFT		5
#define LC_STATUS_MASK		BIT(LC_STATUS_SHIFT)

#define TRICKLE_STATUS_SHIFT		6
#define TRICKLE_STATUS_MASK		BIT(TRICKLE_STATUS_SHIFT)

#define PRE_CHG_STATUS_SHIFT		7
#define PRE_CHG_STATUS_MASK		BIT(PRE_CHG_STATUS_SHIFT)

/* S2MF301_CHG_STATUS2 */
#define IVR_STATUS_SHIFT		0
#define IVR_STATUS_MASK			BIT(IVR_STATUS_SHIFT)

#define ICR_STATUS_SHIFT		1
#define ICR_STATUS_MASK			BIT(ICR_STATUS_SHIFT)

#define BST_ON_STATUS_SHIFT		4
#define BST_ON_STATUS_MASK			BIT(BST_ON_STATUS_SHIFT)

#define OTG_ON_OFF_STATUS_SHIFT		5
#define OTG_ON_OFF_STATUS_MASK		BIT(OTG_ON_OFF_STATUS_SHIFT)

#define BST_LABAT_STATUS_SHIFT		6
#define BST_LABAT_STATUS_MASK		BIT(BST_LABAT_STATUS_SHIFT)

#define OTG_TO_BAT_STATUS_SHIFT		7
#define OTG_TO_BAT_STATUS_MASK		BIT(OTG_TO_BAT_STATUS_SHIFT)

/* S2MF301_CHG_STATUS3 */
#define TOP_OFF_TIMER_FAULT_STATUS_SHIFT		0
#define TOP_OFF_TIMER_FAULT_STATUS_MASK			BIT(TOP_OFF_TIMER_FAULT_STATUS_SHIFT)

#define COOL_FAST_CHG_TIMER_FAULT_STATUS_SHIFT		1
#define COOL_FAST_CHG_TIMER_FAULT_STATUS_MASK			BIT(COOL_FAST_CHG_TIMER_FAULT_STATUS_SHIFT)

#define PRE_TRICKLE_CHG_TIMER_FAULT_STATUS_SHIFT		2
#define PRE_TRICKLE_CHG_TIMER_FAULT_STATUS_MASK			BIT(PRE_TRICKLE_CHG_TIMER_FAULT_STATUS_SHIFT)

#define WDT_AP_RESET_STATUS_SHIFT		4
#define WDT_AP_RESET_STATUS_MASK			BIT(WDT_AP_RESET_STATUS_SHIFT)

#define WDT_SUSPEND_STATUS_SHIFT		5
#define WDT_SUSPEND_STATUS_MASK			BIT(WDT_SUSPEND_STATUS_SHIFT)

/* S2MF301_CHG_CTRL0 */
#define REG_MODE_SHIFT			0
#define REG_MODE_WIDTH			4
#define REG_MODE_MASK			MASK(REG_MODE_WIDTH, REG_MODE_SHIFT)

#define CHARGER_OFF_MODE		0
#define BUCK_MODE			1
#define CHG_MODE			3
#define OTG_BST_MODE			6

/* S2MF301_CHG_CTRL1 */
#define SET_CHGIN_IVR_SHIFT		4
#define SET_CHGIN_IVR_WIDTH		4
#define SET_CHGIN_IVR_MASK		MASK(SET_CHGIN_IVR_WIDTH,\
					SET_CHGIN_IVR_SHIFT)

/* S2MF301_CHG_CTRL2 */
#define INPUT_CURRENT_LIMIT_SHIFT	0
#define INPUT_CURRENT_LIMIT_WIDTH	7
#define INPUT_CURRENT_LIMIT_MASK	MASK(INPUT_CURRENT_LIMIT_WIDTH,\
					INPUT_CURRENT_LIMIT_SHIFT)

/* S2MF301_CHG_CTRL4 */
#define SET_OTG_OCP_SHIFT		0
#define SET_OTG_OCP_WIDTH		7
#define SET_OTG_OCP_MASK		MASK(SET_OTG_OCP_WIDTH, SET_OTG_OCP_SHIFT)

/* S2MF301_CHG_CTRL7 */
#define SET_VF_VBAT_SHIFT		0
#define SET_VF_VBAT_WIDTH		8
#define SET_VF_VBAT_MASK		MASK(SET_VF_VBAT_WIDTH, SET_VF_VBAT_SHIFT)

/* S2MF301_CHG_CTRL9 */
#define SET_VSYS_SHIFT			4
#define SET_VSYS_WIDTH			3
#define SET_VSYS_MASK			MASK(SET_VSYS_WIDTH, SET_VSYS_SHIFT)

/* S2MF301_CHG_CTRL10 */
#define FAST_CHARGING_CURRENT_SHIFT	0
#define FAST_CHARGING_CURRENT_WIDTH	7
#define FAST_CHARGING_CURRENT_MASK	MASK(FAST_CHARGING_CURRENT_WIDTH,\
					FAST_CHARGING_CURRENT_SHIFT)

/* S2MF301_CHG_CTRL11 */
#define BAT_OCP_SHIFT			0
#define BAT_OCP_WIDTH			5
#define BAT_OCP_MASK			MASK(BAT_OCP_WIDTH, BAT_OCP_SHIFT)

/* S2MF301_CHG_CTRL14 */
#define FIRST_TOPOFF_CURRENT_SHIFT	0
#define FIRST_TOPOFF_CURRENT_WIDTH	5
#define FIRST_TOPOFF_CURRENT_MASK	MASK(FIRST_TOPOFF_CURRENT_WIDTH,\
					FIRST_TOPOFF_CURRENT_SHIFT)

/* S2MF301_CHG_CTRL15 */
#define SECOND_TOPOFF_CURRENT_SHIFT	0
#define SECOND_TOPOFF_CURRENT_WIDTH	5
#define SECOND_TOPOFF_CURRENT_MASK	MASK(SECOND_TOPOFF_CURRENT_WIDTH,\
					SECOND_TOPOFF_CURRENT_SHIFT)

/* S2MF301_CHG_CTRL16 */
#define SET_EN_WDT_AP_RESET_SHIFT	5
#define SET_EN_WDT_AP_RESET_MASK	BIT(SET_EN_WDT_AP_RESET_SHIFT)

#define SET_EN_WDT_SHIFT		4
#define SET_EN_WDT_MASK			BIT(SET_EN_WDT_SHIFT)

#define WDT_TIME_SHIFT			1
#define WDT_TIME_WIDTH			3
#define WDT_TIME_MASK			MASK(WDT_TIME_WIDTH, WDT_TIME_SHIFT)

#define WDT_CLR_SHIFT			0
#define WDT_CLR_MASK			BIT(WDT_CLR_SHIFT)

/* S2MF301_CHG_CTRL17 */
#define SET_TIME_FC_CHG_SHIFT		3
#define SET_TIME_FC_CHG_WIDTH		3
#define SET_TIME_FC_CHG_MASK		MASK(SET_TIME_FC_CHG_WIDTH, SET_TIME_FC_CHG_SHIFT)

/* S2MF301_CHG_CTRL18 */
#define TOP_OFF_TIME_SHIFT		0
#define TOP_OFF_TIME_WIDTH		3
#define TOP_OFF_TIME_MASK		MASK(TOP_OFF_TIME_WIDTH, TOP_OFF_TIME_SHIFT)

#define HV_MAINS_IVR_INPUT		2000
#define HV_MAINS_IVR_STEP		300

enum {
	CHIP_ID = 0,
};

ssize_t s2mf301_chg_show_attrs(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t s2mf301_chg_store_attrs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

#define S2MF301_CHARGER_ATTR(_name)			\
{							\
	.attr = {.name = #_name, .mode = 0664},		\
	.show = s2mf301_chg_show_attrs,			\
	.store = s2mf301_chg_store_attrs,		\
}

enum {
	CHG_REG = 0,
	CHG_DATA,
	CHG_REGS,
};

enum {
	S2MF301_TOPOFF_TIMER_500us	= 0x0,
	S2MF301_TOPOFF_TIMER_5m		= 0x1,
	S2MF301_TOPOFF_TIMER_10m	= 0x2,
	S2MF301_TOPOFF_TIMER_30m	= 0x3,
	S2MF301_TOPOFF_TIMER_50m	= 0x4,
	S2MF301_TOPOFF_TIMER_70m	= 0x5,
	S2MF301_TOPOFF_TIMER_90m	= 0x6,
	S2MF301_TOPOFF_TIMER_DIS	= 0x7,
};

enum {
	S2MF301_WDT_TIMER_40s	= 0x1,
	S2MF301_WDT_TIMER_50s	= 0x2,
	S2MF301_WDT_TIMER_60s	= 0x3,
	S2MF301_WDT_TIMER_70s	= 0x4,
	S2MF301_WDT_TIMER_80s	= 0x5,
	S2MF301_WDT_TIMER_90s	= 0x6,
	S2MF301_WDT_TIMER_100s	= 0x7,
};

enum {
	S2MF301_FC_CHG_TIMER_4hr	= 0x1,
	S2MF301_FC_CHG_TIMER_6hr	= 0x2,
	S2MF301_FC_CHG_TIMER_8hr	= 0x3,
	S2MF301_FC_CHG_TIMER_10hr	= 0x4,
	S2MF301_FC_CHG_TIMER_12hr	= 0x5,
	S2MF301_FC_CHG_TIMER_14hr	= 0x6,
	S2MF301_FC_CHG_TIMER_16hr	= 0x7,
};

enum {
	S2MF301_SET_OTG_OCP_500mA   = 0x13,
	S2MF301_SET_OTG_OCP_900mA   = 0x23,
	S2MF301_SET_OTG_OCP_1200mA  = 0x2F,
	S2MF301_SET_OTG_OCP_1500mA  = 0x3B,
	S2MF301_SET_OTG_OCP_2000mA  = 0x4F,
};

enum {
	S2MF301_SET_BAT_OCP_3500mA   = 0x06,
	S2MF301_SET_BAT_OCP_4000mA   = 0x07,
	S2MF301_SET_BAT_OCP_4500mA   = 0x08,
	S2MF301_SET_BAT_OCP_5000mA   = 0x09,
	S2MF301_SET_BAT_OCP_5500mA   = 0x0A,
	S2MF301_SET_BAT_OCP_6000mA   = 0x0B,
	S2MF301_SET_BAT_OCP_6500mA   = 0x0C,
	S2MF301_SET_BAT_OCP_7000mA   = 0x0D,
	S2MF301_SET_BAT_OCP_7500mA   = 0x0E,
	S2MF301_SET_BAT_OCP_8000mA   = 0x0F,
	S2MF301_SET_BAT_OCP_8500mA   = 0x10,
	S2MF301_SET_BAT_OCP_9000mA   = 0x11,
	S2MF301_SET_BAT_OCP_9500mA   = 0x12,
	S2MF301_SET_BAT_OCP_10000mA   = 0x13,
};

struct s2mf301_charger_platform_data {
	s2m_charging_current_t *charging_current_table;
	s2m_charging_current_t *charging_current;
	s2m_charging_current_t *charging_current_expand;
	int chg_float_voltage;
	char *charger_name;
	char *fuelgauge_name;
	char *pmeter_name;
	bool chg_eoc_dualpath;
	int recharge_vcell;
	uint32_t is_1MHz_switching:1;
	int chg_switching_freq;
	int erd;
};


struct s2mf301_charger_data {
	struct i2c_client       *i2c;	/* Slave addr = 0x7A */
	struct i2c_client       *top;	/* Slave addr = 0x74 */
	struct device *dev;
	struct s2mf301_platform_data *s2mf301_pdata;
	struct delayed_work otg_vbus_work;
	struct delayed_work pmeter_2lv_work;
	struct delayed_work pmeter_3lv_work;

	struct workqueue_struct *charger_wqueue;
	struct power_supply *psy_chg;
	struct power_supply_desc psy_chg_desc;
	struct power_supply *psy_otg;
	struct power_supply_desc psy_otg_desc;
	struct power_supply *psy_pm;

	struct s2mf301_charger_platform_data *pdata;
	int dev_id;
	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;
	struct mutex charger_mutex;

	bool ovp;
	bool otg_on;

	int unhealth_cnt;
	int status;
	int health;

	/* s2mf301 */
	int irq_ovp;
	int irq_ivr;
	int irq_det_bat;
	int irq_done;
	int irq_topoff;

	int irq_wdt_ap_reset;
	int irq_wdt_suspend;
	int irq_pre_trickle_chg_timer_fault;
	int irq_cool_fast_chg_timer_fault;
	int irq_topoff_timer_fault;
	int irq_tfb;
	int irq_tsd;
	int irq_prechg;
	int irq_trickle;
	int irq_lc;
	int irq_sc;
	int irq_bat_ocp;

	int irq_uvlob;
	int irq_in2bat;
	int irq_rechg;

	int charge_mode;

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif
};

#endif /*S2MF301_CHARGER_H*/
