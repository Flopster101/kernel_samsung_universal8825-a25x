/*
 * s2mc501_charger.h - Header of S2MC501 Charger Driver
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

#ifndef S2MC501_DIRECT_CHARGER_H
#define S2MC501_DIRECT_CHARGER_H
#include "s2m_chg_manager.h"

/* Test debug log enable */
#define EN_TEST_READ 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

#define S2MC501_DC_CHIP_SET		0x00
#define S2MC501_DC_INT0			0x01
#define S2MC501_DC_INT1			0x02
#define S2MC501_DC_INT2			0x03

#define S2MC501_DC_INT0_MASK		0x04
#define S2MC501_DC_INT1_MASK		0x05
#define S2MC501_DC_INT2_MASK		0x06

#define S2MC501_DC_STATUS0		0x07
#define S2MC501_DC_STATUS1		0x08
#define S2MC501_DC_STATUS2		0x09

#define S2MC501_DC_ENABLE		0x10
#define S2MC501_CD_OTP0			0x19

#define S2MC501_DC_TIMER_SET		0x2A

#define S2MC501_DC_OTP7			0x57
#define S2MC501_DC_OTP8			0x58
#define S2MC501_DC_CTRL0		0x5A
#define S2MC501_DC_CTRL1		0x5B
#define S2MC501_DC_CTRL2		0x5C

#define	S2MC501_ES_NO			0xF5

#define FAKE_BAT_LEVEL          50

/* DC_ENABLE 0x10 Reg */
#define DC_EN_MASK			0x01
#define SYSCLK_EN_MASK			0x40

/* DC_OTP7 0x57 Reg */
#define DC_ENB_CC			0x80
#define DC_ENB_CV			0x40

/* DC_OTP8 0x58 Reg */
#define DC_OTP_RCP_DEB			0x01

/* DC_CTRL0 0x5A Reg */
#define DC_SET_CHGIN_ICHG_MASK		0x3F
#define DC_SET_CHGIN_ICHG_SHIFT		0

/* DC_CTRL1 0x0B Reg */
#define SET_CV_MASK			0x7F
#define SET_CV_SHIFT			0

/* DC_CTRL2 0x5C Reg */
#define DC_RCP_MASK			0x07
#define DC_RCP_SHIFT			0


#define DC_CV_4400mV			0xBE
#define DC_CV_4800mV			0xE6


/* IRQ */
#define DC_INT0_IN2OUTFAIL		0x01
#define DC_INT0_DCOUTFAIL		0x02
#define DC_INT0_CLSHORT			0x04
#define DC_INT0_PLUGOUT			0x08
#define DC_INT0_IN2OUTOVP		0x10
#define DC_INT0_FC_P_UVLO		0x20
#define DC_INT0_VOUTOVP			0x40
#define DC_INT0_TSD				0x80
#define DC_INT1_CHARGING		0x01
//#define DC_INT1_BOOSTING		0x02
#define DC_INT1_CFLYSHORT		0x04
#define DC_INT1_DCINOVP			0x08
#define DC_INT1_INPUTOCP		0x20
#define DC_INT1_DET_REGULATION	0x40
#define DC_INT2_RCP				0x01
#define DC_INT2_CLSHORT			0x04
#define DC_INT2_CFLY_OVP		0x08

enum {
	DC_RCP_100mA = 0,
	DC_RCP_1000mA,
	DC_RCP_1500mA,
	DC_RCP_2000mA,
	DC_RCP_2500mA,
	DC_RCP_3000mA,
	DC_RCP_3500mA,
	DC_RCP_4000mA,
};

typedef enum {
	DC_STATE_OFF = 0,
	DC_STATE_CHECK_VBAT,
	DC_STATE_PRESET,
	DC_STATE_START_CC,
	DC_STATE_CC,
	DC_STATE_SLEEP_CC,
	DC_STATE_ADJUSTED_CC,
	DC_STATE_CV,
	DC_STATE_WAKEUP_CC,
	DC_STATE_DONE,
	DC_STATE_MAX_NUM,
} s2mc501_dc_state_t;

typedef enum {
	DC_TRANS_INVALID = 0,
	DC_TRANS_CHG_ENABLED,
	DC_TRANS_NO_COND,
	DC_TRANS_VBATT_RETRY,
	DC_TRANS_WAKEUP_DONE,
	DC_TRANS_RAMPUP,
	DC_TRANS_RAMPUP_FINISHED,
	DC_TRANS_BATTERY_OK,
	DC_TRANS_BATTERY_NG,
	DC_TRANS_CHGIN_OKB_INT,
	DC_TRANS_BAT_OKB_INT,
	DC_TRANS_NORMAL_CHG_INT,
	DC_TRANS_DC_DONE_INT,
	DC_TRANS_CC_STABLED,
	DC_TRANS_CC_WAKEUP,
	DC_TRANS_DETACH,
	DC_TRANS_FAIL_INT,
	DC_TRANS_FLOAT_VBATT,
	DC_TRANS_RAMPUP_OVER_VBATT,
	DC_TRANS_RAMPUP_OVER_CURRENT,
	DC_TRANS_SET_CURRENT_MAX,
	DC_TRANS_TOP_OFF_CURRENT,
	DC_TRANS_DC_OFF_INT,
	DC_TRANS_MAX_NUM,
} s2mc501_dc_trans_t;


typedef enum {
	S2MC501_DC_DISABLE = 0,
	S2MC501_DC_EN
} s2mc501_dc_ctrl_t;

typedef enum {
	S2MC501_DC_PPS_SIGNAL_DEC = 0,
	S2MC501_DC_PPS_SIGNAL_INC
} s2mc501_dc_pps_signal_t;

enum {
	TA_PWR_TYPE_25W = 0,
	TA_PWR_TYPE_45W,
};

typedef enum {
	S2MC501_DC_MODE_TA_CC = 0,
	S2MC501_DC_MODE_DC_CC
} s2mc501_dc_cc_mode;

typedef enum {
	S2MC501_DC_SC_STATUS_OFF,
	S2MC501_DC_SC_STATUS_CHARGE,
	S2MC501_DC_SC_STATUS_DUAL_BUCK
} s2mc501_dc_sc_status_t;

typedef enum {
	PMETER_ID_VDCIN = 0,
	PMETER_ID_VOUT,
	PMETER_ID_VCELL,
	PMETER_ID_IIN,
	PMETER_ID_IINREV,
	PMETER_ID_TDIE,
	PMETER_ID_MAX_NUM,
} s2mc501_pmeter_id_t;

enum s2mc501_irq {
	S2MC501_PM_ADC_INT1_TDIE,
	S2MC501_PM_ADC_INT1_IINREV,
	S2MC501_PM_ADC_INT1_IIN,
	S2MC501_PM_ADC_INT1_VCELL,
	S2MC501_PM_ADC_INT1_VOUT,
	S2MC501_PM_ADC_INT1_VDCIN,
};

#define SC_CHARGING_ENABLE_CHECK_VBAT	5
#define DC_TA_MIN_PPS_CURRENT		1000
#define DC_TA_START_PPS_CURR_45W	2000
#define DC_TA_START_PPS_CURR_25W	1000
#define DC_MIN_VBAT		3700
#define DC_TA_CURR_STEP_MA	50
#define DC_TA_VOL_STEP_MV	20
#define DC_MAX_SOC		95
#define DC_MAX_SHIFTING_CNT	60
#define DC_TOPOFF_CURRENT_MA	1000
#define DC_TA_VOL_END_MV	9000
#define DC_MAX_INPUT_CURRENT_MA		12000

#define CONV_STR(x, r) { case x: r = #x; break; }

ssize_t s2mc501_dc_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mc501_dc_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define S2MC501_DIRECT_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = s2mc501_dc_show_attrs,			    \
	.store = s2mc501_dc_store_attrs,			\
}

typedef struct s2mc501_dc_platform_data {
	char *dc_name;
	char *pm_name;
	char *fg_name;
	char *sc_name;
	char *sec_dc_name;
	char *top_name;

	int irq_gpio;
	int enable_gpio;

	int input_current_limit;
	int charging_current;
	int topoff_current;
	int sense_resistance;
	int recharge_vcell;
	int chg_switching_freq;
	int temperature_source; /* 0 : chip junction, 1 : external ntc for FG */
} s2mc501_dc_platform_data_t;

typedef struct s2mc501_dc_pd_data {
	unsigned int pdo_pos;
	unsigned int taMaxVol;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;
} s2mc501_dc_pd_data_t;

typedef struct s2mc501_dc_prop {
	int prop;
	union power_supply_propval value;
} s2mc501_dc_prop_t;

struct s2mc501_dc_data {
	struct i2c_client       *i2c;
	struct i2c_client       *i2c_common;
	struct device *dev;
	struct s2mc501_platform_data *s2mc501_pdata;
	struct power_supply *psy_fg;
	struct power_supply *psy_sc;
	struct power_supply *psy_sec_dc;

	struct delayed_work timer_wqueue;
	struct delayed_work check_vbat_wqueue;
	struct delayed_work start_cc_wqueue;
	struct delayed_work state_manager_wqueue;
	struct delayed_work set_prop_wqueue;
	struct delayed_work dc_monitor_work;
	struct delayed_work chk_valid_wqueue;
	struct delayed_work update_wqueue;
	struct delayed_work isr_work;

	/* step check monitor */
	struct delayed_work step_monitor_work;
	struct workqueue_struct *step_monitor_wqueue;
	struct wakeup_source *dc_mon_ws;
	struct alarm dc_monitor_alarm;
	unsigned int alarm_interval;
	int step_now;

	struct power_supply *psy_dc;
	struct power_supply_desc psy_dc_desc;

	s2mc501_dc_platform_data_t *pdata;
	s2mc501_dc_prop_t prop_data;

	int rev_id;
	int cable_type;
	bool is_charging;
	bool is_plugout_mask;
	int ta_pwr_type;
	struct mutex dc_mutex;
	struct mutex irq_lock;
	struct mutex i2c_lock;
	struct mutex timer_mutex;
	struct mutex dc_state_mutex;
	struct mutex pps_isr_mutex;
	struct mutex trans_mutex;
	struct mutex dc_mon_mutex;
	struct mutex auto_pps_mutex;
	struct mutex mode_mutex;
	struct wakeup_source *ws;
	struct wakeup_source *state_manager_ws;
	struct wakeup_source *mode_irq_ws;
	struct wakeup_source *fail_irq_ws;
	struct wakeup_source *thermal_irq_ws;

	s2mc501_dc_state_t dc_state;
	s2mc501_dc_trans_t dc_trans;
	void (*dc_state_fp[DC_STATE_MAX_NUM])(void *);
	s2mc501_dc_pd_data_t *pd_data;
	s2mc501_dc_sc_status_t dc_sc_status;
	struct power_supply *psy_pmeter;
	int pmeter[PMETER_ID_MAX_NUM];
	struct power_supply *psy_top;

	unsigned int targetIIN;
	unsigned int adjustIIN;
	unsigned int minIIN;
	unsigned int chgIIN;
	unsigned int ppsVol;
	unsigned int floatVol;
	unsigned int step_vbatt_mv;
	unsigned int step_iin_ma;
	int vchgin_okb_retry;
	unsigned int cc_count;
	unsigned int step_cv_cnt;
	unsigned int chk_vbat_margin;
	unsigned int chk_vbat_charged;
	unsigned int chk_vbat_prev;
	unsigned int chk_iin_prev;

	wait_queue_head_t wait;
	bool is_pps_ready;
	bool is_rampup_adjusted;
	bool is_autopps_disabled;
	int rise_speed_dly_ms;

	int dc_chg_status;
	int ps_health;

	int irq_gpio;

	/* DC_INT_0 */
	int irq_ramp_fail;
	int irq_normal_charging;
	int irq_wcin_okb;
	int irq_vchgin_okb;

	int irq_byp2out_ovp;
	int irq_byp_ovp;
	int irq_vbat_okb;
	int irq_out_ovp;

	/* DC_INT_1 */
	/* [7] rsvd */
	int irq_ocp;
	int irq_wcin_rcp;
	int irq_chgin_rcp;
	int irq_off;
	int irq_plug_out;
	int irq_wcin_diod_prot;
	int irq_chgin_diod_prot;

	/* DC_INT_2 */
	int irq_done;
	int irq_pps_fail;
	int irq_long_cc;
	int irq_thermal;
	int irq_scp;
	int irq_cv;
	int irq_wcin_icl;
	int irq_chgin_icl;

	/* DC_INT_3 */
	/* [7] rsvd */
	int irq_sc_off;
	int irq_pm_off;
	int irq_wcin_up;
	int irq_wcin_down;
	int irq_tsd;

	/* [1] rsvd */
	int irq_tfb;

	long mon_chk_time;
};

extern int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable);
extern int sec_get_pps_voltage(void);
extern int s2mc501_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mc501_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mc501_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /*S2MC501_CHARGER_H*/
