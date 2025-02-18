/*
 * s2mc501_pmeter.h - Header of S2MC501 Powermeter Driver
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

#ifndef S2MC501_PMETER_H
#define S2MC501_PMETER_H
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/power/s2mc501_direct_charger.h>

#define S2MC501_PM_VDCIN1		0x70
#define S2MC501_PM_VDCIN2		0x71
#define S2MC501_PM_VOUT1		0x72
#define S2MC501_PM_VOUT2		0x73
#define S2MC501_PM_VCELL1		0x74
#define S2MC501_PM_VCELL2		0x75
#define S2MC501_PM_IIN1			0x76
#define S2MC501_PM_IIN2			0x77
#define S2MC501_PM_IINREV1		0x78
#define S2MC501_PM_IINREV2		0x79
#define S2MC501_PM_TDIE1		0x7A
#define S2MC501_PM_TDIE2		0x7B
#define S2MC501_PM_INT0			0x7C
#define S2MC501_PM_INT1			0x7D
#define S2MC501_PM_INT_MASK1		0x7E
#define S2MC501_PM_INT_MASK2		0x7F

#define S2MC501_PM_CTRL0		0x93
#define S2MC501_PM_CTRL1		0x94
#define S2MC501_PM_CTRL2		0x95
#define S2MC501_PM_CTRL3		0x96
#define S2MC501_PM_CTRL4		0x97
#define S2MC501_PM_CTRL5		0x98
#define S2MC501_PM_CTRL6		0x99
#define S2MC501_PM_CTRL7		0x9A
#define S2MC501_PM_CTRL8		0x9B
#define S2MC501_PM_CTRL9		0x9C

enum pm_type {
	S2MC501_PM_TYPE_VDCIN = 0,
	S2MC501_PM_TYPE_VOUT,
	S2MC501_PM_TYPE_VCELL,
	S2MC501_PM_TYPE_IIN,
	S2MC501_PM_TYPE_IINREV,
	S2MC501_PM_TYPE_TDIE
};

enum {
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};

struct s2mc501_pmeter_data {
	struct i2c_client       *i2c;
	struct device *dev;

	int irq_base;
	int irq_vdcin;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex		pmeter_mutex;
};


extern int s2mc501_pmeter_init(struct s2mc501_dc_data *charger);

#endif /*S2MC501_PMETER_H*/
