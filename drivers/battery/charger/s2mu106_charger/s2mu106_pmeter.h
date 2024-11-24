/*
 * s2mu106_pmeter.h - Header of S2MU106 Powermeter Driver
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
#ifndef S2MU106_PMETER_H
#define S2MU106_PMETER_H
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include "linux/mfd/slsi/s2mu106/s2mu106.h"
#if defined(CONFIG_S2MU106_TYPEC_WATER)
#include "linux/usb/typec/slsi/s2mu106/usbpd-s2mu106.h"
#include <linux/time64.h>
#endif
#include "../../common/sec_charging_common.h"

#define S2MU106_PM_VALUP1	0x03
#define S2MU106_PM_VALUP2	0x04
#define S2MU106_PM_INT1		0x05
#define S2MU106_PM_INT2		0x06
#define S2MU106_PM_VALUP1_MASK	0x0B
#define S2MU106_PM_VALUP2_MASK	0x0C
#define S2MU106_PM_INT1_MASK	0x0D
#define S2MU106_PM_INT2_MASK	0x0E

#define S2MU106_PM_CO_MASK1	0x59
#define S2MU106_PM_CO_MASK2	0x5A
#define S2MU106_PM_CAL_DIS1	0x5B
#define S2MU106_PM_CAL_DIS2	0x5C
#define S2MU106_PM_REQ_BOX_RR1	0x5D
#define S2MU106_PM_REQ_BOX_RR2	0x5E
#define S2MU106_PM_REQ_BOX_CO1	0x5F
#define S2MU106_PM_REQ_BOX_CO2	0x60
#define S2MU106_PM_CTRL1	0x61
#define S2MU106_PM_CTRL2	0x62
#define S2MU106_PM_CTRL3	0x63
#define S2MU106_PM_CTRL4	0x64

#define S2MU106_PM_VAL1_VCHGIN	0x12
#define S2MU106_PM_VAL2_VCHGIN	0x13
#define S2MU106_PM_VAL1_VWCIN	0x14
#define S2MU106_PM_VAL2_VWCIN	0x15
#define S2MU106_PM_VAL1_VBYP	0x16
#define S2MU106_PM_VAL2_VBYP	0x17
#define S2MU106_PM_VAL1_VSYS	0x18
#define S2MU106_PM_VAL2_VSYS	0x19
#define S2MU106_PM_VAL1_VBAT	0x1A
#define S2MU106_PM_VAL2_VBAT	0x1B
#define S2MU106_PM_VAL1_VCC1	0x1C
#define S2MU106_PM_VAL2_VCC1	0x1D
#define S2MU106_PM_VAL1_VGPADC	0x1E
#define S2MU106_PM_VAL2_VGPADC	0x1F
#define S2MU106_PM_VAL1_ICHGIN	0x20
#define S2MU106_PM_VAL2_ICHGIN	0x21
#define S2MU106_PM_VAL1_IWCIN	0x22
#define S2MU106_PM_VAL2_IWCIN	0x23
#define S2MU106_PM_VAL1_VCC2	0x24
#define S2MU106_PM_VAL2_VCC2	0x25
#define S2MU106_PM_VAL1_IOTG	0x26
#define S2MU106_PM_VAL2_IOTG	0x27
#define S2MU106_PM_VAL1_ITX	0x28
#define S2MU106_PM_VAL2_ITX	0x29
#define S2MU106_PM_HYST_LEVEL1	0xA3
#define S2MU106_PM_HYST_LEVEL4	0xA6
#define S2MU106_PM_HYST_CONTI1	0xA9

/* Register Description */
/* 0x0D PM_INT_MASK1 */
#define S2MU106_PM_INT_MASK1_VGPADC_SHIFT	1
#define S2MU106_PM_INT_MASK1_VGPADC		(0x1 << S2MU106_PM_INT_MASK1_VGPADC_SHIFT)

/* 0x5F REQ_BOX_CO1  */
#define S2MU106_PM_REQ_BOX_CO1_VGPADCC_SHIFT	1
#define S2MU106_PM_REQ_BOX_CO1_VGPADCC		(0x1 << S2MU106_PM_INT_MASK1_VGPADC_SHIFT)

/* 0x64 PM_CTRL4 */
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT	5
#define S2MU106_PM_CTRL4_PM_IOUT_SEL		(0x3 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_RMODE	(0x1 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)
#define S2MU106_PM_CTRL4_PM_IOUT_SEL_VMODE	(0x3 << S2MU106_PM_CTRL4_PM_IOUT_SEL_SHIFT)

#define IS_INVALID_CODE(x)	(x > 2450)

#define IS_VCHGIN_IN(x)		(x >= 2000)
#define IS_VBUS_LOW(x)		(x < 4000)


/* GPADC Table */
/* Vadc table */
#define PM_VWATER		(100)
#define IS_VGPADC_OPEN(x)	(x >= 0 && x < PM_VWATER)
#define IS_VGPADC_WATER(x)	(x >= PM_VWATER)
#define IS_VGPADC_VBUS_SHORT(x)	(x >= 500)
#define IS_VGPADC_GND(x)	(x < 100)

/* Radc table */
#define PM_RWATER		(1500)
#define IS_RGPADC_WATER(x)	((x >= 1) && (x <= PM_RWATER))
#define IS_RGPADC_OPEN(x)	(x > 5000)
#define IS_RGPADC_GND(x)	(x <= 50)
/* pull-down
 * uct100 : 0 ohm
 * uct300 : 200 kohm
 */
#define IS_RGPADC_FACWATER(x)	(x <= 200)

/* Water Detection Sensitivity */
#define PM_WATER_DET_CNT_LOOP		(5)
#define PM_SOURCE_WATER_DET_CNT_LOOP		(3)
#define PM_WATER_DET_CNT_SEN_LOW	(2)
#define PM_WATER_DET_CNT_SEN_MIDDLE	(3)
#define PM_WATER_DET_CNT_SEN_HIGH	(5)
#define PM_SOURCE_WATER_CNT		(1)

#define PM_WATER_WORK_RESUME_ITV	(3000)	/* 3sec */

#define DEV_DRV_VERSION			(0x33)

#define MAX_BUF_SIZE                    (256) /* For qc solution build */

enum pm_type {
	PM_TYPE_VCHGIN = 0,
	PM_TYPE_VWCIN,
	PM_TYPE_VBYP,
	PM_TYPE_VSYS,
	PM_TYPE_VBAT,
	PM_TYPE_VCC1,
	PM_TYPE_VGPADC,
	PM_TYPE_ICHGIN,
	PM_TYPE_IWCIN,
	PM_TYPE_VCC2,
	PM_TYPE_IOTG,
	PM_TYPE_ITX,
	PM_TYPE_MAX,
};

enum {
	PM_WATER_SALT,
	PM_WATER_FRESH,
};

enum {
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};

enum pm_setprop_opmode {
	PM_OPMODE_PROP_GPADC_DISABLE,
	PM_OPMODE_PROP_GPADC_ENABLE,
	PM_OPMODE_PROP_GPADC_RMEASURE,
	PM_OPMODE_PROP_GPADC_VMEASURE,
};

enum pm_mode {
	PM_MODE_NONE = 0,
	PM_RMODE,
	PM_VMODE,
};

enum pm_water_status {
	PM_DRY_IDLE = 0,
	PM_DRY_DETECTING,
	PM_WATER_IDLE,
	PM_WATER_DETECTING,
};

enum pm_water_sensitivity {
	PM_WATER_SEN_NONE = 0,
	PM_WATER_SEN_LOW,
	PM_WATER_SEN_MIDDLE,
	PM_WATER_SEN_HIGH,
	PM_WATER_SEN_CNT_MAX,
};

#define HYST_LEV_VCHGIN_SHIFT	4
#define HYST_LEV_VCHGIN_MASK	0x70

enum {
	HYST_LEV_VCHGIN_80mV	=	0x1,
	HYST_LEV_VCHGIN_160mV	=	0x2,
	HYST_LEV_VCHGIN_320mV	=	0x3,
	HYST_LEV_VCHGIN_640mV	=	0x4,
	HYST_LEV_VCHGIN_1280mV	=	0x5,
	HYST_LEV_VCHGIN_2560mV	=	0x6,
	HYST_LEV_VCHGIN_5120mV	=	0x7,
};

#define HYST_LEV_VGPADC_SHIFT	4
#define HYST_LEV_VGPADC_MASK	0x70
#define HYST_LEV_VGPADC_DEFALUT HYST_LEV_VGPADC_640mV
#define HYST_LEV_VGPADC_MAX	HYST_LEV_VGPADC_2560mV

enum {
	HYST_LEV_VGPADC_20mV	=	0x0,
	HYST_LEV_VGPADC_40mV	=	0x1,
	HYST_LEV_VGPADC_80mV	=	0x2,
	HYST_LEV_VGPADC_160mV	=	0x3,
	HYST_LEV_VGPADC_320mV	=	0x4,
	HYST_LEV_VGPADC_640mV	=	0x5,
	HYST_LEV_VGPADC_1280mV	=	0x6,
	HYST_LEV_VGPADC_2560mV	=	0x7,
};

#define HYST_CON_VCHGIN_SHIFT	6
#define HYST_CON_VCHGIN_MASK	0xC0
enum {
	HYST_CON_VCHGIN_1	=	0x1,
	HYST_CON_VCHGIN_4	=	0x2,
	HYST_CON_VCHGIN_16	=	0x3,
	HYST_CON_VCHGIN_32	=	0x4,
};

struct s2mu106_pmeter_data {
	struct i2c_client       *i2c;
	struct device *dev;
	struct s2mu106_platform_data *s2mu106_pdata;

	int irq_vchgin;
	int irq_gpadc;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex pmeter_mutex;

#if defined(CONFIG_S2MU106_TYPEC_WATER)
	struct s2mu106_dev *s2mu106;
	struct mutex water_mutex;
	struct power_supply *pdic_psy;
	struct power_supply *muic_psy;
	struct timespec64 last_gpadc_irq;
	struct delayed_work pwr_off_chk_work;
	struct delayed_work water_work;
	struct delayed_work late_init_work;
	struct wakeup_source *water_work_ws;
	enum pm_water_status water_status;
	enum pm_water_sensitivity water_sen;
	int water_det_cnt[PM_WATER_SEN_CNT_MAX];
	int water_work_call_cnt;
	bool is_pwr_off_water;
	int gpadc_mode;
#endif
#if defined(CONFIG_ARCH_QCOM)
	struct wakeup_source *gpadc_ws;
#endif
};

#endif /*S2MU106_PMETER_H*/
