/*
 * s2mf301_fuelgauge.h - Header of S2MF301 Fuel Gauge
 *
 * Copyright (C) 2022 Samsung Electronics, Inc.
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

#ifndef __S2MF301_FUELGAUGE_H
#define __S2MF301_FUELGAUGE_H __FILE__

#if IS_ENABLED(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif
#include "../../common/sec_charging_common.h"
#include <linux/mfd/slsi/s2mf301/s2mf301.h>

/* Secondary address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define TEMP_COMPEN			1
#define BATCAP_LEARN			1
#define BAT_GPIO_NO	        2

#define S2MF301_REG_STATUS		0x00
#define S2MF301_REG_FG_INT			0x02
#define S2MF301_REG_FG_IMTM			0x03
#define S2MF301_REG_RVBAT		0x04
#define S2MF301_REG_RCUR_CC		0x06
#define S2MF301_REG_RSOC		0x08
#define S2MF301_REG_MONOUT		0x0A
#define S2MF301_REG_MONOUT_SEL		0x0C
#define S2MF301_REG_RBATCAP		0x0E
#define S2MF301_REG_BATCAP		0x10
#define S2MF301_REG_CAPCC		0x3E
#define S2MF301_REG_RSOC_R_SAVE		0x2A
#define S2MF301_REG_RSOC_R		0x80
#define S2MF301_REG_RSOH		0x84
#define S2MF301_REG_RSOC_R_I2C	0x8E

#if defined(CONFIG_SEC_FACTORY)
#define S2MF301_REG_VBAT_TIME_SEL	0x0D
#define S2MF301_REG_FS				0x6F
#define FG_ON_MASK					0x01
#define FS_MANUAL_EN				0x40
#endif

#define S2MF301_REG_RZADJ		0x12
#define S2MF301_REG_RBATZ0		0x16
#define S2MF301_REG_RBATZ1		0x18
#define S2MF301_REG_IRQ_LVL		0x1A
#define S2MF301_REG_START		0x1E

#define S2MF301_REG_VBAT_I2C	0x25
#define S2MF301_REG_TEMP_I2C	0x28
#define S2MF301_REG_AVG_VBAT	0x2E
#define S2MF301_REG_AVG_CURR	0x30
#define S2MF301_REG_AVG_TEMP	0x32
#define S2MF301_REG_TEMP4FG		0x34

#define BATT_TEMP_CONSTANT		250

/* Use reserved register region 0x48[3:0]
 * For battery parameter version check
 */
#define S2MF301_REG_FG_ID		0x48

#define S2MF301_REG_VM			0x67

#define S2MF301_REG_RBATCAP_OCV_NEW_IN 0x90

/* For temperature compensation */
#define TEMP_COMPEN_INC_OK_EN	(1 << 7)

#define FG_BATT_DUMP_SIZE 128

#define SCALED_SOC 977

enum {
	CURRENT_MODE = 0,
	LOW_SOC_VOLTAGE_MODE, // not used
	HIGH_SOC_VOLTAGE_MODE,
	END_MODE,
};

static char *mode_to_str[] = {
	"CC_MODE",
	"VOLTAGE_MODE",	// not used
	"VOLTAGE_MODE",
	"END",
};

struct fg_info {
	/* battery info */
	int soc;
	int battery_profile_index;
	int battery_param_ver;
};

typedef struct fg_age_data_info {
	int battery_table3[88]; // evt2
	int battery_table4[22]; // evt2
	int batcap[4];
	int designcap[2];
	int batcap_ocv_new[2];
	int soc_arr_val[22];
	int ocv_arr_val[22];
	int rRS_CC0;
} fg_age_data_info_t;

struct s2mf301_fuelgauge_platform_data {
	int fuel_alert_soc;
	int fuel_alert_vol;
	int fg_irq;

	unsigned int capacity_full;

	char *fuelgauge_name;

	struct sec_charging_current *charging_current;

	int capacity_max;
	int capacity_max_margin;
	int capacity_min;
	int capacity_calculation_type;
	bool use_external_temp;
	unsigned int float_voltage;

	int bat_id_gpio[BAT_GPIO_NO];
	int bat_gpio_cnt;
};

struct s2mf301_fuelgauge_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *top;
	struct mutex fuelgauge_mutex;
	struct s2mf301_fuelgauge_platform_data *pdata;
	struct power_supply	*psy_fg;
	/* struct delayed_work isr_work; */

	int cable_type;
	bool is_charging; /* charging is enabled */
	int rsoc;
	int mode;
	u8 revision;

	u8 battery_id;

	/* HW-dedicated fuelgauge info structure
	 * used in individual fuelgauge file only
	 * (ex. dummy_fuelgauge.c)
	 */
	struct fg_info info;

	fg_age_data_info_t *age_data_info;
	int fg_num_age_step;
	int fg_age_step;
	int age_reset_status;
	struct mutex fg_reset_lock;
	int change_step;

	bool is_fuel_alerted;
	struct wakeup_source *fuel_alert_ws;

	unsigned int ui_soc;
	unsigned int scaled_soc;

	unsigned int capacity_old;
	unsigned int capacity_max;
#if defined(CONFIG_UI_SOC_PROLONGING)
	unsigned int g_capacity_max;	/* only for dynamic calculation */
	bool capacity_max_conv;
	int prev_raw_soc;
#endif
	int raw_capacity;

	bool initial_update_of_soc;
	bool sleep_initial_update_of_soc;
	struct mutex fg_lock;
	struct delayed_work isr_work;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];
	u8 reg_OTP_53;
	u8 reg_OTP_52;

	int low_voltage_limit_lowtemp;
	int low_voltage_limit;
	int low_temp_limit;
	int temperature;

	int low_soc;
	int low_vbat;
	int high_temp;
	int low_temp;
	bool probe_done;
#if IS_ENABLED(TEMP_COMPEN) || IS_ENABLED(BATCAP_LEARN)
	bool bat_charging; /* battery is charging */
#endif
#if IS_ENABLED(TEMP_COMPEN) && IS_ENABLED(BATCAP_LEARN)
	int fcc;
	int rmc;
#endif
#if IS_ENABLED(TEMP_COMPEN)
	bool vm_status; /* Now voltage mode or not */
	bool pre_vm_status;
	bool pre_is_charging;
	bool pre_bat_charging;
	bool flag_mapping;

	int socni;
	int soc0i;
	int comp_socr; /* 1% unit */
	int pre_comp_socr; /* 1% unit */
	int init_start;
	int soc_r;
	int avg_curr;
	int avg_vbat;
	int curr;
	int vbat;
	int topoff_current;

	int i_socr_coeff;
	int t_socr_coeff;
#endif
#if IS_ENABLED(BATCAP_LEARN)
	bool learn_start;
	bool cond1_ok;
	int c1_count;
	int c2_count;
	int capcc;
	int batcap_ocv;
	int batcap_ocv_fin;
	int cycle;
	int soh;
#endif
	char d_buf[FG_BATT_DUMP_SIZE];
	int bd_vfocv;
	int bd_raw_soc;
};

#if IS_ENABLED(BATCAP_LEARN)
/* cycle, rLOW_EN, rC1_num, rC2_num, rC1_CURR, rWide_lrn_EN, Fast_lrn_EN, Auto_lrn_EN */
int BAT_L_CON[8] = {2, 0, 10, 10, 500, 0, 0, 1};
#endif

#endif /* __S2MF301_FUELGAUGE_H */
