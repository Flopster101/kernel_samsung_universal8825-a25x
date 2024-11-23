/*
 * sm5714_fuelgauge.h
 * Samsung sm5714 Fuel Gauge Header
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This software is sm5714 under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __sm5714_FUELGAUGE_H
#define __sm5714_FUELGAUGE_H __FILE__

#include <linux/power_supply.h>
#include "../../common/sec_charging_common.h"

#if IS_ENABLED(CONFIG_MFD_SM5714)
#include <linux/mfd/core.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#include <linux/regulator/machine.h>
#else
#include "sm5714_fake_mfd_fg.h"
#endif

/* address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define PRINT_COUNT	10

#define ALERT_EN 0x04
#define CAPACITY_SCALE_DEFAULT_CURRENT 1000
#define CAPACITY_SCALE_HV_CURRENT 600

#define SW_RESET_CODE			0x00A6
#define SW_RESET_OTP_CODE		0x01A6
#define RS_MAN_CNTL				0x0800

#define FG_INIT_MARK			0xA000
#define FG_PARAM_UNLOCK_CODE	0x3700
#define FG_PARAM_LOCK_CODE	    0x0000
#define FG_TABLE_LEN			0x17 /* real table length -1 */
#define FG_ADD_TABLE_LEN		0xF /* real table length -1 */
#define FG_INIT_B_LEN		    0x7 /* real table length -1 */


/* control register value */
#define ENABLE_MIX_MODE         0x8200
#define ENABLE_TEMP_MEASURE     0x4000
#define ENABLE_TOPOFF_SOC       0x2000
#define ENABLE_RS_MAN_MODE      0x0800
#define ENABLE_MANUAL_OCV       0x0400
#define ENABLE_MODE_nENQ4       0x0200

#define ENABLE_VL_ALARM         0x0001

#define AUTO_RS_OFF             0x0000
#define AUTO_RS_100             0x0001
#define AUTO_RS_200             0x0002
#define AUTO_RS_300             0x0003

#define SM5714_FG_SOH_TH_MASK   0x0F00


#define CNTL_REG_DEFAULT_VALUE  0x2008
#define INIT_CHECK_MASK         0x1000
#define DISABLE_RE_INIT         0x1000
#define JIG_CONNECTED	0x0001
#define I2C_ERROR_REMAIN		0x0004
#define I2C_ERROR_CHECK	0x0008
#define DATA_VERSION	0x00F0

enum max77854_vempty_mode {
	VEMPTY_MODE_HW = 0,
	VEMPTY_MODE_SW,
	VEMPTY_MODE_SW_VALERT,
	VEMPTY_MODE_SW_RECOVERY,
};

enum battery_id {
	BATTERY_ID_01 = 1,
	BATTERY_ID_02,
	BATTERY_ID_03,
	BATTERY_ID_04,
};

ssize_t sm5714_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sm5714_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);
#define sm5714_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sm5714_fg_show_attrs,			\
	.store = sm5714_fg_store_attrs,			\
}
enum {
	CHIP_ID = 0,
	DATA,
	DATA_1,
	DATA_SRAM,
	DATA_SRAM_1,
};

struct sm5714_fg_info {
	/* test print count */
	int pr_cnt;
	/* full charge comp */
	struct delayed_work	full_comp_work;
	u32 previous_fullcap;
	u32 previous_vffullcap;

	/* Device_id */
	int device_id;
	/* State Of Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* battery AvgVoltage */
	int batt_avgvoltage;
	/* battery OCV */
	int batt_ocv;
	/* Current */
	int batt_current;
	/* battery Avg Current */
	int batt_avgcurrent;
	/* battery SOC cycle */
	int batt_soc_cycle;
	/* battery measure point*/
	int is_read_vpack;

	struct battery_data_t *comp_pdata;

	struct mutex param_lock;
	/* copy from platform data DTS or update by shell script */

	struct mutex io_lock;
	struct device *dev;
	int32_t temperature;; /* 0.1 deg C*/
	int32_t temp_fg;; /* 0.1 deg C*/
	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	int battery_typ;        /*SDI_BATTERY_TYPE or ATL_BATTERY_TYPE*/
	int batt_id_adc_check;
	int battery_table[3][24];
	int v_max_table[5];
	int q_max_table[5];
	int v_max_now;
	int q_max_now;
	int rs_value[7];   /*spare min max factor chg_factor dischg_factor manvalue*/
	int batt_v_max;
	int cap;
	int maxcap;
	int aux_ctrl[2];
	int soh;

	int top_off;

	int i_dp_default;
	int i_alg_default;
	int v_default;
	int v_off;
	int v_slo;
	int vt_default;
	int vtt;
	int ivt;
	int ivv;

	int alg_i_off;
	int alg_i_pslo;
	int alg_i_nslo;
	int dp_i_off;
	int dp_i_pslo;
	int dp_i_nslo;

	int temp_std;

	int data_ver;
	uint32_t soc_alert_flag : 1;  /* 0 : nu-occur, 1: occur */
	uint32_t volt_alert_flag : 1; /* 0 : nu-occur, 1: occur */
	uint32_t flag_full_charge : 1; /* 0 : no , 1 : yes*/
	uint32_t flag_chg_status : 1; /* 0 : discharging, 1: charging*/
	uint32_t flag_charge_health : 1; /* 0 : no , 1 : good*/

	int32_t irq_ctrl;
	int value_v_alarm;
	int value_v_alarm_hys;

	uint32_t is_FG_initialised;
	int iocv_error_count;

	int n_tem_poff;
	int n_tem_poff_offset;
	int l_tem_poff;
	int l_tem_poff_offset;

	/* previous battery voltage current*/
	int p_batt_voltage;
	int p_batt_current;

	unsigned long fullcap_check_interval;
	int full_check_flag;
	bool is_first_check;
};

#define CURRENT_RANGE_MAX_NUM	5

struct battery_data_t {
	const int battery_type; /* 4200 or 4350 or 4400*/
	const int battery_table[3][24];
	int Capacity;
	u32 V_empty;
	u32 V_empty_origin;
	u32 sw_v_empty_vol;
	u32 sw_v_empty_vol_cisd;
	u32 sw_v_empty_recover_vol;
	u32 vbat_ovp;
	u8 battery_id;
};

/* FullCap learning setting */
#define VFFULLCAP_CHECK_INTERVAL	300 /* sec */
/* soc should be 0.1% unit */
#define VFSOC_FOR_FULLCAP_LEARNING	950
#define LOW_CURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_CURRENT_FOR_FULLCAP_LEARNING	120
#define LOW_AVGCURRENT_FOR_FULLCAP_LEARNING	20
#define HIGH_AVGCURRENT_FOR_FULLCAP_LEARNING	100

/* power off margin */
/* soc should be 0.1% unit */
#define POWER_OFF_SOC_HIGH_MARGIN	20
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500
#define POWER_OFF_VOLTAGE_LOW_MARGIN	3400

#define FG_BATT_DUMP_SIZE 128

struct cv_slope {
	int fg_current;
	int soc;
	int time;
};

/* Need to be increased if there are more than 2 BAT ID GPIOs */
#define BAT_GPIO_NO	2

struct sm5714_fuelgauge_platform_data {
	/* charging current for type (0: not use) */
	unsigned int full_check_current_1st;
	unsigned int full_check_current_2nd;

	int jig_irq;
	int jig_gpio;
	int jig_low_active;
	int bat_id_gpio[BAT_GPIO_NO];
	int bat_gpio_cnt;
	unsigned long jig_irq_attr;

	int fg_irq;
	unsigned long fg_irq_attr;
	/* fuel alert SOC (-1: not use) */
	int fuel_alert_soc;
	/* fuel alert can be repeated */
	bool repeated_fuelalert;
	sec_fuelgauge_capacity_type_t capacity_calculation_type;
	/* soc should be soc x 10 (0.1% degree)
	 * only for scaling
	 */
	int capacity_max;
	int capacity_max_margin;
	int capacity_min;
	unsigned int capacity_full;

	int num_age_step;
	int age_step;
	int age_data_length;
	unsigned int *age_data_soc;
	unsigned int full_condition_soc;
};

struct sm5714_fuelgauge_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct i2c_client       *pmic;
	struct mutex            fuelgauge_mutex;
	struct sm5714_platform_data *sm5714_pdata;
	struct sm5714_fuelgauge_platform_data *pdata; //long.vu
	struct power_supply		*psy_fg;
	struct delayed_work isr_work;

	atomic_t shutdown_cnt;

	u8 pmic_rev;
	u8 vender_id;
	u8 read_reg;
	u8 read_sram_reg;

	int cable_type;
	bool is_charging;
	bool ta_exist;

	/* HW-dedicated fuel gauge info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */
	struct sm5714_fg_info	info;
	struct battery_data_t        *battery_data;

	bool is_fuel_alerted;
	struct wakeup_source *fuel_alert_ws;

	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */
#if defined(CONFIG_UI_SOC_PROLONGING)
	unsigned int g_capacity_max;	/* only for dynamic calculation */
	bool capacity_max_conv;
	int prev_raw_soc;
#endif
	unsigned int standard_capacity;

	bool capacity_max_updated;
	bool initial_update_of_soc;
	bool sleep_initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	unsigned int pre_soc;
	int fg_irq;

	int raw_capacity;
	int current_now;
	int current_avg;

	bool using_temp_compensation;
	bool using_hw_vempty;
	unsigned int vempty_mode;

	unsigned int low_temp_limit;

	bool auto_discharge_en;
	u32 discharge_temp_threshold;
	u32 discharge_volt_threshold;
	unsigned int chg_full_soc;

	u32 fg_resistor;
	bool isjigmoderealvbat;
	char d_buf[FG_BATT_DUMP_SIZE];
};
#endif /* __SM5714_FUELGAUGE_H */
