/*
 * s2m_chg_manager.h
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __S2M_BATTERY_H
#define __S2M_BATTERY_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/alarmtimer.h>

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#include <linux/muic_s2m/common/muic.h>
#endif

#define FAKE_BAT_LEVEL		50
#define DEFAULT_ALARM_INTERVAL	30
#define SLEEP_ALARM_INTERVAL	30
#define NORMAL_CURR		10

static char *bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

static char *health_str[] = {
	"Unknown",
	"Good",
	"Overheat",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"WatchdogTimerExpire",
	"SafetyTimerExpire",
	"OverCurrent",
	"CalibrationRequired",
	"Warm",
	"Cool",
	"Hot",
	"UnderVoltage",
	"ColdLimit",
	"OverheatLimit",
};

enum s2m_power_supply_type {
	POWER_SUPPLY_TYPE_MAX = 0xFF,
	POWER_SUPPLY_TYPE_OTG = 0xFF,
	POWER_SUPPLY_TYPE_HV_MAINS,
	POWER_SUPPLY_TYPE_PREPARE_TA,
	POWER_SUPPLY_TYPE_SMART_NOTG,
	POWER_SUPPLY_TYPE_USB_PD_APDO,		/* Direct Charger */
	POWER_SUPPLY_TYPE_END,
};

enum s2m_power_supply_health {
	POWER_SUPPLY_S2M_HEALTH_UNDERVOLTAGE = 0x0E,
	POWER_SUPPLY_S2M_HEALTH_COLD_LIMIT,
	POWER_SUPPLY_S2M_HEALTH_OVERHEAT_LIMIT,
	POWER_SUPPLY_S2M_HEALTH_DC_ERR,
};

enum s2m_power_supply_property {
	POWER_SUPPLY_S2M_PROP_MIN = 0xFF,
	POWER_SUPPLY_S2M_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_S2M_PROP_SOH,
	POWER_SUPPLY_S2M_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_S2M_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_S2M_PROP_CHARGE_POWERED_OTG_CONTROL,
	POWER_SUPPLY_S2M_PROP_CHARGE_TEMP,
	POWER_SUPPLY_S2M_PROP_FUELGAUGE_RESET,
	POWER_SUPPLY_S2M_PROP_CURRENT_FULL,
	POWER_SUPPLY_S2M_PROP_VCHGIN,
	POWER_SUPPLY_S2M_PROP_VWCIN,
	POWER_SUPPLY_S2M_PROP_VBAT,
	POWER_SUPPLY_S2M_PROP_VGPADC,
	POWER_SUPPLY_S2M_PROP_VCC1,
	POWER_SUPPLY_S2M_PROP_VCC2,
	POWER_SUPPLY_S2M_PROP_VDCIN,
	POWER_SUPPLY_S2M_PROP_VF_BST,
	POWER_SUPPLY_S2M_PROP_VOUT,
	POWER_SUPPLY_S2M_PROP_VCELL,
	POWER_SUPPLY_S2M_PROP_ICHGIN,
	POWER_SUPPLY_S2M_PROP_IOTG,
	POWER_SUPPLY_S2M_PROP_ITX,
	POWER_SUPPLY_S2M_PROP_IIN,
	POWER_SUPPLY_S2M_PROP_IINREV,
	POWER_SUPPLY_S2M_PROP_TDIE,
	POWER_SUPPLY_S2M_PROP_CO_ENABLE,
	POWER_SUPPLY_S2M_PROP_RR_ENABLE,
	POWER_SUPPLY_S2M_PROP_USBPD_RESET,
	POWER_SUPPLY_S2M_PROP_USBPD_TEST_READ,
	POWER_SUPPLY_S2M_PROP_DP_ENABLED,

	POWER_SUPPLY_S2M_PROP_VBYP,
	POWER_SUPPLY_S2M_PROP_VSYS,
	POWER_SUPPLY_S2M_PROP_IWCIN,

	POWER_SUPPLY_S2M_PROP_AFC_CHARGER_MODE,

	POWER_SUPPLY_S2M_PROP_CHG_EFFICIENCY,
	POWER_SUPPLY_S2M_PROP_2LV_3LV_CHG_MODE,

	/* s2asl01 */
	POWER_SUPPLY_S2M_PROP_CHGIN_OK,
	POWER_SUPPLY_S2M_PROP_SUPLLEMENT_MODE,
	POWER_SUPPLY_S2M_PROP_DISCHG_MODE,
	POWER_SUPPLY_S2M_PROP_CHG_MODE,
	POWER_SUPPLY_S2M_PROP_CHG_VOLTAGE,
	POWER_SUPPLY_S2M_PROP_BAT_VOLTAGE,
	POWER_SUPPLY_S2M_PROP_CHG_CURRENT,
	POWER_SUPPLY_S2M_PROP_DISCHG_CURRENT,
	POWER_SUPPLY_S2M_PROP_FASTCHG_LIMIT_CURRENT,
	POWER_SUPPLY_S2M_PROP_TRICKLECHG_LIMIT_CURRENT,
	POWER_SUPPLY_S2M_PROP_DISCHG_LIMIT_CURRENT,
	POWER_SUPPLY_S2M_PROP_RECHG_VOLTAGE,
	POWER_SUPPLY_S2M_PROP_EOC_VOLTAGE,
	POWER_SUPPLY_S2M_PROP_EOC_CURRENT,
	POWER_SUPPLY_S2M_PROP_POWERMETER_ENABLE,
	POWER_SUPPLY_S2M_PROP_RECHG_ON,
	POWER_SUPPLY_S2M_PROP_EOC_ON,

    /* s2mf301_top for 501 dc */
    POWER_SUPPLY_S2M_PROP_TOP_DC_INT_MASK,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_EN,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_SELECTION,
	POWER_SUPPLY_S2M_PROP_TOP_CC_ICHG_SEL,
	POWER_SUPPLY_S2M_PROP_TOP_DC_PDO_MAX_CUR_SEL,
	POWER_SUPPLY_S2M_PROP_TOP_EN_STEP_CC,
	POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_START,
	POWER_SUPPLY_S2M_PROP_TOP_DC_DONE_SOC,
	POWER_SUPPLY_S2M_PROP_TOP_DC_TOP_OFF_CURRENT,
	POWER_SUPPLY_S2M_PROP_TOP_DC_CV,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_END,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_START,
	POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT1,
	POWER_SUPPLY_S2M_PROP_TOP_CC_STEP_VBAT2,
	POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_1,
	POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_2,
	POWER_SUPPLY_S2M_PROP_TOP_DC_CC_STEP_3,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_RECOVERY_WAITING,
	POWER_SUPPLY_S2M_PROP_TOP_THERMAL_CONTROL_WAITING,
	POWER_SUPPLY_S2M_PROP_TOP_AUTO_PPS_STATUS,

	POWER_SUPPLY_S2M_PROP_DIRECT_CHARGER_MODE,
	POWER_SUPPLY_S2M_PROP_DIRECT_CHARGE_DONE,

	POWER_SUPPLY_S2M_PROP_PCP_CLK,

	POWER_SUPPLY_S2M_PROP_MAX,
};

static enum power_supply_property s2m_chg_manager_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static enum power_supply_property s2m_dc_manager_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property s2m_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *s2m_supplied_to[] = {
	"s2m-battery",
};

enum sec_battery_rp_curr {
	RP_CURRENT1 = 500,
	RP_CURRENT2 = 1500,
	RP_CURRENT3 = 3000,
};

enum s2m_battery_voltage_mode {
	S2M_BATTERY_VOLTAGE_AVERAGE = 0,
	S2M_BATTERY_VOLTAGE_OCV,
};

enum s2m_battery_current_mode {
	S2M_BATTERY_CURRENT_UA = 0,
	S2M_BATTERY_CURRENT_MA,
};

enum s2m_battery_voltage_type {
	/* uA */
	S2M_BATTERY_VOLTAGE_UV = 0,
	/* mA */
	S2M_BATTERY_VOLTAGE_MV,
};

enum s2m_battery_charger_mode {
	S2M_BAT_CHG_MODE_CHARGING = 0,
	S2M_BAT_CHG_MODE_CHARGING_OFF,
	S2M_BAT_CHG_MODE_BUCK_OFF,
};

enum s2m_battery_factory_mode {
	S2M_BAT_FAC_MODE_VBAT = 0,
	S2M_BAT_FAC_MODE_VBUS,
	S2M_BAT_FAC_MODE_NORMAL,
};

#if IS_ENABLED(CONFIG_DIRECT_CHARGER)
enum s2m_charging_source {
	S2M_SC = 0,
	S2M_DC,
};

enum s2m_direct_charging_mode {
	S2M_DIRECT_CHG_MODE_OFF = 0,
	S2M_DIRECT_CHG_MODE_CHECK_VBAT,
	S2M_DIRECT_CHG_MODE_PRESET,
	S2M_DIRECT_CHG_MODE_ON,
	S2M_DIRECT_CHG_MODE_DONE,
};
#endif

struct s2m_charging_current {
#if IS_ENABLED(CONFIG_OF)
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
	unsigned int full_check_current;
#else
	int input_current_limit;
	int fast_charging_current;
	int full_check_current;
#endif
};

#define s2m_charging_current_t struct s2m_charging_current

struct s2m_chg_manager_platform_data {
	s2m_charging_current_t *charging_current;
	s2m_charging_current_t *charging_current_expand;

	char *charger_name;
	char *pmeter_name;
	char *fuelgauge_name;

#if IS_ENABLED(CONFIG_DIRECT_CHARGER)
	char *direct_charger_name;
	char *switching_charger_name;
#endif
	int default_limit_current;
	int max_input_current;
	int max_charging_current;

	int chg_float_voltage;
	/* full check */
	unsigned int full_check_count;
	unsigned int chg_recharge_vcell;
	unsigned int chg_full_vcell;

	/* Initial maximum raw SOC */
	unsigned int max_rawsoc;
	unsigned int max_rawsoc_offset;

	/* battery */
	char *vendor;
	int technology;

	/* board */
	int erd;

#if IS_ENABLED(CONFIG_SMALL_CHARGER)
	char *smallcharger_name;
	int small_input_current;
	int small_charging_current;
	int small_limit_current;
#endif

	int temp_high;
	int temp_high_recovery;
	int temp_high_limit;
	int temp_high_limit_recovery;
	int temp_low;
	int temp_low_recovery;
	int temp_low_limit;
	int temp_low_limit_recovery;
	int temp_limit_float_voltage;

#if IS_ENABLED(CONFIG_USE_S2M_PDIC)
	int pdo_max_input_vol;
	int pdo_max_chg_power;
#endif

#if IS_ENABLED(CONFIG_S2M_THERMAL)
	int *thermal_limit_level;
#endif
};
struct s2m_chg_manager_info {
	struct s2m_chg_manager_platform_data *pdata;

	struct device *dev;
	struct power_supply *psy_battery;
	struct power_supply_desc psy_battery_desc;
	struct power_supply *psy_usb;
	struct power_supply_desc psy_usb_desc;
	struct power_supply *psy_ac;
	struct power_supply_desc psy_ac_desc;
#if IS_ENABLED(CONFIG_DIRECT_CHARGER)
	struct power_supply *psy_dc_manager;
	struct power_supply_desc psy_dc_manager_desc;
	int direct_chg_mode;
	bool direct_chg_done;
	bool dc_err;
#endif
	struct mutex iolock;
	struct mutex ifconn_lock;

	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	struct delayed_work soc_control;
	struct wakeup_source *monitor_ws;
	struct wakeup_source *vbus_ws;

	struct alarm monitor_alarm;
	unsigned int monitor_alarm_interval;

	int default_limit_current;
	int input_current;
	int max_input_current;
	int charging_current;
	int max_charging_current;
	int topoff_current;
	int cable_type;
	unsigned int charging_mode;

	int vchg_current;
	int vchg_voltage;
	int full_check_cnt;

	/* charging */
	bool is_recharging;

	bool battery_valid;
	int status;
	int health;

	int voltage_now;
	int voltage_avg;
	int voltage_ocv;

	unsigned int capacity;
	unsigned int max_rawsoc;
	unsigned int max_rawsoc_offset;

	int soh;		/* State of Health (%) */

	int current_now;	/* current (mA) */
	int current_avg;	/* average current (mA) */
	int current_max;	/* input current limit (mA) */
	int current_chg;	/* charge current limit (mA) */

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
	struct notifier_block ifconn_nb;
	struct notifier_block ifconn_cc_nb;
#endif

#if IS_ENABLED(CONFIG_SMALL_CHARGER)
	int small_input;	/* input current limit (mA) */
	int small_chg;		/* charge current limit (mA) */
	int small_input_flag;
	int small_limit_current;
#endif

	/* temperature check */
#if IS_ENABLED(CONFIG_BAT_TEMP)
	bool is_temp_control;
#endif
	int temperature;	/* battery temperature(0.1 Celsius)*/
	int temp_high;
	int temp_high_recovery;
	int temp_high_limit;
	int temp_high_limit_recovery;
	int temp_low;
	int temp_low_recovery;
	int temp_low_limit;
	int temp_low_limit_recovery;

#if IS_ENABLED(CONFIG_USE_S2M_PDIC)
	struct delayed_work select_pdo_work;
	int pdo_max_input_vol;
	int pdo_max_chg_power;

	int pdo_sel_num;
	int pdo_sel_vol;
	int pdo_sel_cur;
	bool is_apdo;

	int pd_input_current;
	bool pd_attach;
	bool rp_attach;
	int rp_input_current;
	int rp_charging_current;
#if IS_ENABLED(CONFIG_DIRECT_CHARGER)
	int chg_src;
#endif
#endif

#if IS_ENABLED(CONFIG_S2M_THERMAL)
	bool thermal_enable;
	int thermal_limit;
	int thermal_limit_max;
#endif
	bool monitor_trigger;
};
#endif /* __S2M_BATTERY_H */
