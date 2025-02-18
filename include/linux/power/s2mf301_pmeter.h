/*
 * s2mf301_pmeter.h - Header of S2MF301 Powermeter Driver
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

#ifndef S2MF301_PMETER_H
#define S2MF301_PMETER_H
#include <linux/mfd/samsung/s2mf301.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>

enum s2mf301_pm_register_type {
    S2MF301_REG_PM_ADC_REQ_DONE1            = 0x0,
    S2MF301_REG_PM_ADC_REQ_DONE2            = 0x1,
    S2MF301_REG_PM_ADC_REQ_DONE3            = 0x2,
    S2MF301_REG_PM_ADC_REQ_DONE4            = 0x3,
    S2MF301_REG_PM_ADC_CHANGE_INT1          = 0x4,
    S2MF301_REG_PM_ADC_CHANGE_INT2          = 0x5,
    S2MF301_REG_PM_ADC_CHANGE_INT3          = 0x6,
    S2MF301_REG_PM_ADC_CHANGE_INT4          = 0x7,
    S2MF301_REG_PM_ADC_REQ_DONE1_MASK       = 0x8,
    S2MF301_REG_PM_ADC_REQ_DONE2_MASK       = 0x9,
    S2MF301_REG_PM_ADC_REQ_DONE3_MASK       = 0xA,
    S2MF301_REG_PM_ADC_REQ_DONE4_MASK       = 0xB,
    S2MF301_REG_PM_ADC_CHANGE_INT1_MASK     = 0xC,
    S2MF301_REG_PM_ADC_CHANGE_INT2_MASK     = 0xD,
    S2MF301_REG_PM_ADC_CHANGE_INT3_MASK     = 0xE,
    S2MF301_REG_PM_ADC_CHANGE_INT4_MASK     = 0xF,

    S2MF301_REG_PM_VADC1_CHGIN              = 0x10,
    S2MF301_REG_PM_VADC2_CHGIN              = 0x11,
    S2MF301_REG_PM_IADC1_CHGIN              = 0x12,
    S2MF301_REG_PM_IADC2_CHGIN              = 0x13,
    S2MF301_REG_PM_VADC1_WCIN               = 0x14,
    S2MF301_REG_PM_VADC2_WCIN               = 0x15,
    S2MF301_REG_PM_IADC1_WCIN               = 0x16,
    S2MF301_REG_PM_IADC2_WCIN               = 0x17,
    S2MF301_REG_PM_VADC1_SYS                = 0x18,
    S2MF301_REG_PM_VADC2_SYS                = 0x19,
    S2MF301_REG_PM_VADC1_BAT                = 0x1A,
    S2MF301_REG_PM_VADC2_BAT                = 0x1B,
    S2MF301_REG_PM_VADC1_CC1                = 0x1C,
    S2MF301_REG_PM_VADC2_CC1                = 0x1D,
    S2MF301_REG_PM_VADC1_CC2                = 0x1E,
    S2MF301_REG_PM_VADC2_CC2                = 0x1F,

    S2MF301_REG_PM_VADC1_TADC_DIE           = 0x24,
    S2MF301_REG_PM_VADC2_TADC_DIE           = 0x25,
    S2MF301_REG_PM_VADC1_BYP                = 0x26,
    S2MF301_REG_PM_VADC2_BYP                = 0x27,
    S2MF301_REG_PM_VADC1_GPADC1             = 0x28,
    S2MF301_REG_PM_VADC2_GPADC1             = 0x29,
    S2MF301_REG_PM_VADC1_GPADC2             = 0x2A,
    S2MF301_REG_PM_VADC2_GPADC2             = 0x2B,
    S2MF301_REG_PM_IADC1_OTG                = 0x2C,
    S2MF301_REG_PM_IADC2_OTG                = 0x2D,
    S2MF301_REG_PM_VADC1_GPADC3             = 0x2E,
    S2MF301_REG_PM_VADC2_GPADC3             = 0x2F,

    S2MF301_REG_PM_RID_STATUS               = 0x30,
    S2MF301_REG_PM_ICHG_STATUS              = 0x31,

    S2MF301_REG_PM_ADCEN_1TIME_REQ1         = 0x4C,
    S2MF301_REG_PM_ADCEN_1TIME_REQ2         = 0x4D,

    S2MF301_REG_PM_ADCEN_CONTINU1           = 0x50,
    S2MF301_REG_PM_ADCEN_CONTINU2           = 0x51,

	S2MF301_REG_PM_HYST_LEV					= 0x6F,
};

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

    S2MF301_PM_TYPE_MAX,
};

enum {
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};

struct s2mf301_pmeter_data {
	struct i2c_client       *i2c;
	struct device *dev;
	struct s2mf301_platform_data *s2mf301_pdata;

	int irq_vchgin;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex		pmeter_mutex;
};

#endif /*S2MF301_PMETER_H*/
