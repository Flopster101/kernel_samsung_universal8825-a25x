/*
 * s2mf301_pmeter.h - Header of S2MF301 Powermeter Driver
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef S2MF301_PMETER_H
#define S2MF301_PMETER_H
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/time64.h>
#include "../../common/sec_charging_common.h"
#include <linux/muic/slsi/s2mf301/s2mf301-muic.h>

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

#define MAX_BUF_SIZE					(256) /* For qc solution build */

#define S2MF301_PM_ADC_ENABLE_MASK	(0x01)
#define S2MF301_PM_RID_ATTACH_MASK (1 << 5)
#define S2MF301_PM_RID_DETACH_MASK (1 << 4)
#define S2MF301_PM_RID_ATTACH_DETACH_MASK (S2MF301_PM_RID_ATTACH_MASK | S2MF301_PM_RID_DETACH_MASK)

#define S2MF301_PM_RID_STATUS_MASK	(S2MF301_PM_RID_STATUS_255K |\
		S2MF301_PM_RID_STATUS_56K |\
		S2MF301_PM_RID_STATUS_301K |\
		S2MF301_PM_RID_STATUS_523K |\
		S2MF301_PM_RID_STATUS_619K)
#define S2MF301_PM_RID_STATUS_0K	(0x01 << 7)
#define S2MF301_PM_RID_STATUS_56K	(0x01 << 6)
#define S2MF301_PM_RID_STATUS_255K	(0x01 << 5)
#define S2MF301_PM_RID_STATUS_301K	(0x01 << 4)
#define S2MF301_PM_RID_STATUS_523K	(0x01 << 3)
#define S2MF301_PM_RID_STATUS_619K	(0x01 << 2)
#define S2MF301_PM_RID_STATUS_1M	(0x01 << 1)
#define S2MF301_PM_RID_STATUS_WAKEUP	(0x01 << 0)

enum s2mf301_pm_register_type {
	S2MF301_REG_PM_ADC_REQ_DONE1			= 0x0,
	S2MF301_REG_PM_ADC_REQ_DONE2			= 0x1,
	S2MF301_REG_PM_ADC_REQ_DONE3			= 0x2,
	S2MF301_REG_PM_ADC_REQ_DONE4			= 0x3,
	S2MF301_REG_PM_ADC_CHANGE_INT1		= 0x4,
	S2MF301_REG_PM_ADC_CHANGE_INT2		= 0x5,
	S2MF301_REG_PM_ADC_CHANGE_INT3		= 0x6,
	S2MF301_REG_PM_ADC_CHANGE_INT4		= 0x7,
	S2MF301_REG_PM_ADC_REQ_DONE1_MASK	= 0x8,
	S2MF301_REG_PM_ADC_REQ_DONE2_MASK	= 0x9,
	S2MF301_REG_PM_ADC_REQ_DONE3_MASK	= 0xA,
	S2MF301_REG_PM_ADC_REQ_DONE4_MASK	= 0xB,
	S2MF301_REG_PM_ADC_CHANGE_INT1_MASK	= 0xC,
	S2MF301_REG_PM_ADC_CHANGE_INT2_MASK	= 0xD,
	S2MF301_REG_PM_ADC_CHANGE_INT3_MASK	= 0xE,
	S2MF301_REG_PM_ADC_CHANGE_INT4_MASK	= 0xF,

	S2MF301_REG_PM_VADC1_CHGIN			= 0x10,
	S2MF301_REG_PM_VADC2_CHGIN			= 0x11,
	S2MF301_REG_PM_IADC1_CHGIN			= 0x12,
	S2MF301_REG_PM_IADC2_CHGIN			= 0x13,
	S2MF301_REG_PM_VADC1_WCIN			= 0x14,
	S2MF301_REG_PM_VADC2_WCIN			= 0x15,
	S2MF301_REG_PM_IADC1_WCIN			= 0x16,
	S2MF301_REG_PM_IADC2_WCIN			= 0x17,
	S2MF301_REG_PM_VADC1_SYS				= 0x18,
	S2MF301_REG_PM_VADC2_SYS				= 0x19,
	S2MF301_REG_PM_VADC1_BAT				= 0x1A,
	S2MF301_REG_PM_VADC2_BAT				= 0x1B,
	S2MF301_REG_PM_VADC1_CC1				= 0x1C,
	S2MF301_REG_PM_VADC2_CC1				= 0x1D,
	S2MF301_REG_PM_VADC1_CC2				= 0x1E,
	S2MF301_REG_PM_VADC2_CC2				= 0x1F,

	S2MF301_REG_PM_VADC1_TADC_DIE		= 0x24,
	S2MF301_REG_PM_VADC2_TADC_DIE		= 0x25,
	S2MF301_REG_PM_VADC1_BYP				= 0x26,
	S2MF301_REG_PM_VADC2_BYP				= 0x27,
	S2MF301_REG_PM_VADC1_GPADC1			= 0x28,
	S2MF301_REG_PM_VADC2_GPADC1			= 0x29,
	S2MF301_REG_PM_VADC1_GPADC2			= 0x2A,
	S2MF301_REG_PM_VADC2_GPADC2			= 0x2B,
	S2MF301_REG_PM_IADC1_OTG				= 0x2C,
	S2MF301_REG_PM_IADC2_OTG				= 0x2D,
	S2MF301_REG_PM_VADC1_GPADC3			= 0x2E,
	S2MF301_REG_PM_VADC2_GPADC3			= 0x2F,

	S2MF301_REG_PM_RID_STATUS			= 0x30,
	S2MF301_REG_PM_ICHG_STATUS			= 0x31,

	S2MF301_REG_PM_ADCEN_BY_TA_MASK1	= 0x3C,
	S2MF301_REG_PM_ADCEN_BY_TA_MASK2	= 0x3D,
	S2MF301_REG_PM_ADCEN_BY_OTG_MASK1	= 0x40,
	S2MF301_REG_PM_ADCEN_BY_OTG_MASK2	= 0x41,

	S2MF301_REG_PM_ADCEN_1TIME_REQ1		= 0x4C,
	S2MF301_REG_PM_ADCEN_1TIME_REQ2		= 0x4D,

	S2MF301_REG_PM_ADCEN_CONTINU1		= 0x50,
	S2MF301_REG_PM_ADCEN_CONTINU2		= 0x51,

	S2MF301_REG_PM_CTRL1		= 0x54,

	S2MF301_REG_PM_HYST_LEV			= 0x6F,

	S2MF301_REG_PM_WET_EN1			= 0x73,
	S2MF301_REG_PM_WET_EN2			= 0x74,

	S2MF301_REG_PM_ADC_CTRL8		= 0x7A,
	S2MF301_REG_PM_WET_DET_CTRL1		= 0x8E,
	S2MF301_REG_PM_WET_DET_CTRL2		= 0x8F,
};

#define S2MF301_REG_PM_WET_EN1_GPADC1		(0x1 << 3)
#define S2MF301_REG_PM_WET_EN2_GPADC2		(0x1 << 7)

enum pm_type {
	S2MF301_PM_TYPE_VCHGIN = 0,
	S2MF301_PM_TYPE_VWCIN,
	S2MF301_PM_TYPE_VBYP,
	S2MF301_PM_TYPE_VSYS,
	S2MF301_PM_TYPE_VBAT,
	S2MF301_PM_TYPE_VCC1,
	S2MF301_PM_TYPE_VCC2,
	S2MF301_PM_TYPE_TDIE,

	S2MF301_PM_TYPE_ICHGIN,
	S2MF301_PM_TYPE_IWCIN,
	S2MF301_PM_TYPE_IOTG,
	S2MF301_PM_TYPE_ITX,
	S2MF301_PM_TYPE_GPADC1,
	S2MF301_PM_TYPE_GPADC2,
	S2MF301_PM_TYPE_GPADC3,
	S2MF301_PM_TYPE_VDCIN,

	S2MF301_PM_TYPE_GPADC12,

	S2MF301_PM_TYPE_MAX,
};

enum {
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};

enum s2mf301_water_gpadc {
	S2MF301_WATER_GPADC_DRY,
	S2MF301_WATER_GPADC12,
	S2MF301_WATER_GPADC1,
	S2MF301_WATER_GPADC2,
};

enum s2mf301_water_irq_type {
	S2MF301_IRQ_TYPE_CHANGE,
	S2MF301_IRQ_TYPE_WATER,
	S2MF301_IRQ_TYPE_RR,
};

enum s2mf301_water_gpadc_mode {
	S2MF301_GPADC_RMODE,
	S2MF301_GPADC_VMODE,
};

struct s2mf301_pmeter_data {
	struct i2c_client *i2c;
	struct device *dev;
	struct s2mf301_platform_data *s2mf301_pdata;

	int irq_vchgin;
	int irq_comp1;
	int irq_comp2;
	int irq_water_status1;
	int irq_water_status2;
	int irq_gpadc1up;
	int irq_gpadc2up;
	int irq_rid_attach;
	int irq_rid_detach;
	int irq_ichgin_th;

	int water_adc_mode;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex		pmeter_mutex;
	struct s2mf301_muic_data *muic_data;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	struct s2mf301_water_data *water;
	struct s2mf301_dev *s2mf301;
#endif
};

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
extern void *s2mf301_pm_water_init(struct s2mf301_water_data *water);
extern void s2mf301_pm_water_irq_init(struct s2mf301_water_data *water);
#endif
#endif /*S2MF301_PMETER_H*/
