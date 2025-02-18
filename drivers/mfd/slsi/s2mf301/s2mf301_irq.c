/*
 * s2mf301-irq.c - Interrupt controller support for s2mf301
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/mfd/slsi/s2mf301/s2mf301_log.h>
#include <linux/err.h>

#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
#include "../../../battery/charger/s2mf301_charger/s2mf301_charger.h"
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
#include "../../../battery/fuelgauge/s2mf301_fuelgauge/s2mf301_fuelgauge.h"
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
#include "../../../battery/charger/s2mf301_charger/s2mf301_pmeter.h"
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
#include <linux/leds-s2mf301.h>
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
#include <linux/muic/slsi/s2mf301/s2mf301-muic.h>
#endif
#if IS_ENABLED(CONFIG_TOP_S2MF301)
#include <linux/muic/slsi/s2mf301/s2mf301_top.h>
#endif
static const u8 s2mf301_mask_reg[] = {
#if IS_ENABLED(CONFIG_TOP_S2MF301)
	[DC_INT] = S2MF301_TOP_REG_DC_AUTO_PPS_INT_MASK,
	[PM_RID_INT] = S2MF301_TOP_REG_TOP_PM_RID_INT_MASK,
	[CC_RID_INT] = S2MF301_TOP_REG_TOP_TC_RID_INT_MASK,
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	[FG_INT] = S2MF301_REG_FG_IMTM,
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	[FLED_INT] = S2MF301_FLED_INT_MASK,
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	[AFC_INT] = S2MF301_REG_AFC_INT_MASK,
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	[MUIC_INT] = S2MF301_REG_MUIC_INT_MASK,
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	[PM_ADC_REQ_DONE1] = S2MF301_REG_PM_ADC_REQ_DONE1_MASK,
	[PM_ADC_REQ_DONE2] = S2MF301_REG_PM_ADC_REQ_DONE2_MASK,
	[PM_ADC_REQ_DONE4] = S2MF301_REG_PM_ADC_REQ_DONE4_MASK,
	[PM_ADC_CHANGE_INT1] = S2MF301_REG_PM_ADC_CHANGE_INT1_MASK,
	[PM_ADC_CHANGE_INT2] = S2MF301_REG_PM_ADC_CHANGE_INT2_MASK,
	[PM_ADC_CHANGE_INT3] = S2MF301_REG_PM_ADC_CHANGE_INT3_MASK,
	[PM_ADC_CHANGE_INT4] = S2MF301_REG_PM_ADC_CHANGE_INT4_MASK,
#endif
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	[CHG_INT0] = S2MF301_CHG_INT0M,
	[CHG_INT1] = S2MF301_CHG_INT1M,
	[CHG_INT2] = S2MF301_CHG_INT2M,
	[CHG_INT3] = S2MF301_CHG_INT3M,
	[CHG_INT4] = S2MF301_CHG_INT4M,
	[CHG_INT5] = S2MF301_CHG_INT5M,
	[CHG_INT6] = S2MF301_CHG_INT6M,
#endif
};

struct s2mf301_irq_data {
	int mask;
	enum s2mf301_irq_source group;
};

static struct i2c_client *get_i2c(struct s2mf301_dev *s2mf301, enum s2mf301_irq_source src)
{
	switch (src) {
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	case FG_INT:
		return s2mf301->fg;
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	case FLED_INT:
		return s2mf301->i2c;
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	case AFC_INT:
		return s2mf301->muic;
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	case MUIC_INT:
		return s2mf301->muic;
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	case PM_ADC_REQ_DONE1 ... PM_ADC_CHANGE_INT4:
		return s2mf301->pm;
#endif
#if IS_ENABLED(CONFIG_TOP_S2MF301)
	case DC_INT ... CC_RID_INT :
		return s2mf301->i2c;
#endif
#if IS_ENABLED(CONFIG_REGULATOR_S2MF301)
	case HBST_INT:
		return s2mf301->i2c;
#endif
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	case CHG_INT0 ... CHG_INT6:
		return s2mf301->chg;
#endif
	default:
		return ERR_PTR(-EINVAL);
	}
}

#define DECLARE_IRQ(idx, _group, _mask) \
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mf301_irq_data s2mf301_irqs[] = {
#if IS_ENABLED(CONFIG_TOP_S2MF301)
	DECLARE_IRQ(S2MF301_TOP_DC_IRQ_RAMP_UP_DONE,			DC_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_TOP_DC_IRQ_RAMP_UP_FAIL,			DC_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_TOP_DC_IRQ_THERMAL_CONTROL,			DC_INT, 1 << 2),
	DECLARE_IRQ(S2MF301_TOP_DC_IRQ_CHARGING_STATE_CHAGNE,	DC_INT, 1 << 3),
	DECLARE_IRQ(S2MF301_TOP_DC_IRQ_CHARGING_DONE,			DC_INT, 1 << 4),

	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_56K,				PM_RID_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_255K,			PM_RID_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_301K,			PM_RID_INT, 1 << 2),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_523K,			PM_RID_INT, 1 << 3),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_619K,			PM_RID_INT, 1 << 4),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_OTG,				PM_RID_INT, 1 << 5),
	DECLARE_IRQ(S2MF301_TOP_PM_RID_IRQ_RID_MASK_ALL,		PM_RID_INT, 1 << 7),

	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_255K,			CC_RID_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_301K,			CC_RID_INT,	1 << 2),
	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_523K,			CC_RID_INT,	1 << 3),
	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_619K,			CC_RID_INT,	1 << 4),
	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_OTG,				CC_RID_INT,	1 << 5),
	DECLARE_IRQ(S2MF301_TOP_CC_RID_IRQ_RID_MASK_ALL,		CC_RID_INT,	1 << 7),
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	DECLARE_IRQ(S2MF301_FG_IRQ_LOW_SOC,			FG_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_FG_IRQ_LOW_VBAT,		FG_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_FG_IRQ_HIGH_TEMP,		FG_INT, 1 << 2),
	DECLARE_IRQ(S2MF301_FG_IRQ_LOW_TEMP,		FG_INT, 1 << 3),
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	DECLARE_IRQ(S2MF301_FLED_IRQ_C2F_Vbyp_ovp_prot,	FLED_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_FLED_IRQ_C2F_Vbyp_OK_Warning,	FLED_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_FLED_IRQ_TORCH_ON,			FLED_INT, 1 << 2),
	DECLARE_IRQ(S2MF301_FLED_IRQ_LED_ON_TA_Detach,			FLED_INT, 1 << 3),
	DECLARE_IRQ(S2MF301_FLED_IRQ_CH1_ON,		FLED_INT, 1 << 5),
	DECLARE_IRQ(S2MF301_FLED_IRQ_OPEN_CH1,		FLED_INT, 1 << 6),
	DECLARE_IRQ(S2MF301_FLED_IRQ_SHORT_CH1,		FLED_INT, 1 << 7),
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	DECLARE_IRQ(S2MF301_AFC_IRQ_AFC_LOOP,			AFC_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_AFC_IRQ_VDNMon,			AFC_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_AFC_IRQ_DNRes,			AFC_INT, 1 << 2),
	DECLARE_IRQ(S2MF301_AFC_IRQ_MPNack,			AFC_INT, 1 << 3),
	DECLARE_IRQ(S2MF301_AFC_IRQ_MRxBufOw,			AFC_INT, 1 << 4),
	DECLARE_IRQ(S2MF301_AFC_IRQ_MRxTrf,			AFC_INT, 1 << 5),
	DECLARE_IRQ(S2MF301_AFC_IRQ_MRxPerr,			AFC_INT, 1 << 6),
	DECLARE_IRQ(S2MF301_AFC_IRQ_MRxRdy,			AFC_INT, 1 << 7),
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	DECLARE_IRQ(S2MF301_MUIC_IRQ_ATTATCH,			MUIC_INT, 1 << 0),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_DETACH,			MUIC_INT, 1 << 1),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_USB_OVP,			MUIC_INT, 1 << 3),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_VBUS_ON,			MUIC_INT, 1 << 4),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_VBUS_OFF,			MUIC_INT, 1 << 5),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_USB_Killer,		MUIC_INT, 1 << 6),
	DECLARE_IRQ(S2MF301_MUIC_IRQ_GP_OVP,			MUIC_INT, 1 << 7),
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VCC2UP,			PM_ADC_REQ_DONE1, 1 << 1),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VCC1UP,			PM_ADC_REQ_DONE1, 1 << 2),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VBATUP,			PM_ADC_REQ_DONE1, 1 << 3),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VSYSUP,			PM_ADC_REQ_DONE1, 1 << 4),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VBYPUP,			PM_ADC_REQ_DONE1, 1 << 5),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VWCINUP,			PM_ADC_REQ_DONE1, 1 << 6),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE1_VCHGINUP,			PM_ADC_REQ_DONE1, 1 << 7),

	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_GPADC3UP,			PM_ADC_REQ_DONE2, 1 << 1),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_GPADC2UP,			PM_ADC_REQ_DONE2, 1 << 2),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_GPADC1UP,			PM_ADC_REQ_DONE2, 1 << 3),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_ITXUP,			PM_ADC_REQ_DONE2, 1 << 4),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_IOTGUP,			PM_ADC_REQ_DONE2, 1 << 5),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_IWCINUP,			PM_ADC_REQ_DONE2, 1 << 6),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE2_ICHGINUP,			PM_ADC_REQ_DONE2, 1 << 7),

	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE4_WET_STATUS_GPADC2,		PM_ADC_REQ_DONE4, 1 << 0),
	DECLARE_IRQ(S2MF301_PM_ADC_REQ_DONE4_WET_STATUS_GPADC1,		PM_ADC_REQ_DONE4, 1 << 1),

	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VCC2UP,			PM_ADC_CHANGE_INT1, 1 << 1),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VCC1UP,			PM_ADC_CHANGE_INT1, 1 << 2),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VBATUP,			PM_ADC_CHANGE_INT1, 1 << 3),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VSYSUP,			PM_ADC_CHANGE_INT1, 1 << 4),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VBYPUP,			PM_ADC_CHANGE_INT1, 1 << 5),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VWCINUP,			PM_ADC_CHANGE_INT1, 1 << 6),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT1_VCHGINUP,			PM_ADC_CHANGE_INT1, 1 << 7),

	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_GPADC3UP,			PM_ADC_CHANGE_INT2, 1 << 1),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_GPADC2UP,			PM_ADC_CHANGE_INT2, 1 << 2),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_GPADC1UP,			PM_ADC_CHANGE_INT2, 1 << 3),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_ITXUP,			PM_ADC_CHANGE_INT2, 1 << 4),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_IOTGUP,			PM_ADC_CHANGE_INT2, 1 << 5),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_IWCINUP,			PM_ADC_CHANGE_INT2, 1 << 6),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT2_ICHGINUP,			PM_ADC_CHANGE_INT2, 1 << 7),

	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT4_ICHGIN_TH,			PM_ADC_CHANGE_INT4, 1 << 2),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT4_PM_RID_ATTACH,			PM_ADC_CHANGE_INT4, 1 << 5),
	DECLARE_IRQ(S2MF301_PM_ADC_CHANGE_INT4_PM_RID_DETACH,			PM_ADC_CHANGE_INT4, 1 << 4),
#endif
#if IS_ENABLED(CONFIG_REGULATOR_S2MF301)
	DECLARE_IRQ(S2MF301_HBST_IRQ_OFF,			HBST_INT, 1 << 0);
	DECLARE_IRQ(S2MF301_HBST_IRQ_ON,			HBST_INT, 1 << 1);
	DECLARE_IRQ(S2MF301_HBST_IRQ_SCP,			HBST_INT, 1 << 2);
#endif
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	DECLARE_IRQ(S2MF301_CHG0_IRQ_CHGIN_UVLOB,			CHG_INT0, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_CHGIN2BATB,			CHG_INT0, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_CHGIN_OVP,			CHG_INT0, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_VBUS_DET,			CHG_INT0, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_BATID,			CHG_INT0, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_BUCK_CHG_MODE,			CHG_INT0, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_BUCK_ONLY_MODE,			CHG_INT0, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG0_IRQ_CHGIN_SS_END,			CHG_INT0, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG1_IRQ_RECHARGING,			CHG_INT1, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_DONE,			CHG_INT1, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_TOPOFF,			CHG_INT1, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_CV,			CHG_INT1, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_SC,			CHG_INT1, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_LC,			CHG_INT1, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_TRICKLE,			CHG_INT1, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG1_IRQ_PRECHG,			CHG_INT1, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG2_IRQ_IVR,			CHG_INT2, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_ICR,			CHG_INT2, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_VOLTAGE_LOOP,			CHG_INT2, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_SC_CC_LOOP,			CHG_INT2, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_BST_START_END,			CHG_INT2, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_OTG_SS_END,			CHG_INT2, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_OTG_LBAT,			CHG_INT2, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG2_IRQ_OTG_OCP_IN2BAT,			CHG_INT2, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG3_IRQ_TOPOFF_TIMER_FAULT,			CHG_INT3, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_LC_SC_CV_CHG_TIMER_FAULT,			CHG_INT3, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_PRE_TRICKLE_CHG_TIMER_FAULT,			CHG_INT3, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_BAT_OCP,			CHG_INT3, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_INPUT_OCP,			CHG_INT3, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_WDT_TIMER_FAULT,			CHG_INT3, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_VSYS_OVP,			CHG_INT3, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG3_IRQ_VSYS_SCP,			CHG_INT3, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG4_IRQ_QBAT_OFF,			CHG_INT4, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_TFB,			CHG_INT4, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_TSD,			CHG_INT4, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_VSYS_SCP_TIMER_FAULT,			CHG_INT4, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_PVDD_SHORT,			CHG_INT4, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_CHGIN_LDO_DOWN,			CHG_INT4, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_OTG_OCP,			CHG_INT4, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG4_IRQ_OTG_SCP,			CHG_INT4, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG5_IRQ_BYP_ON,			CHG_INT5, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_SHIP_OR_OFF,			CHG_INT5, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_BYP_SCP,			CHG_INT5, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_BYP_ON_FALL,			CHG_INT5, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_HS_ON,			CHG_INT5, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_FWD_BYP_VIN_DROP,			CHG_INT5, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_FWD_BYP_IN2BAT_OK,			CHG_INT5, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG5_IRQ_FWD_BYP_OCP,			CHG_INT5, 1 << 7),

	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_0_LOW,			CHG_INT6, 1 << 0),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_10_10,			CHG_INT6, 1 << 1),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_20_10,			CHG_INT6, 1 << 2),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_45_20,			CHG_INT6, 1 << 3),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_60_45,			CHG_INT6, 1 << 4),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_JEITA_60_HIGH,			CHG_INT6, 1 << 5),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_CHG_TIMER_FAULT,			CHG_INT6, 1 << 6),
	DECLARE_IRQ(S2MF301_CHG6_IRQ_REG_MODE_MISMATCH,			CHG_INT6, 1 << 7),
#endif
};

static void s2mf301_irq_lock(struct irq_data *data)
{
	struct s2mf301_dev *s2mf301 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mf301->irqlock);
}

static void s2mf301_irq_sync_unlock(struct irq_data *data)
{
	struct s2mf301_dev *s2mf301 = irq_get_chip_data(data->irq);
	struct i2c_client *i2c;
	int i;
	u8 mask_reg;

	if (s2mf301->change_irq_mask == false)
		goto skip;

	for (i = 0; i < S2MF301_IRQ_GROUP_NR; i++) {
		if (i >= 0 && i < ARRAY_SIZE(s2mf301_mask_reg))
			mask_reg = s2mf301_mask_reg[i];
		i2c = get_i2c(s2mf301, i);

		if (mask_reg == S2MF301_REG_INVALID || IS_ERR_OR_NULL(i2c))
			continue;

		s2mf301->irq_masks_cache[i] = s2mf301->irq_masks_cur[i];
		s2mf301_write_reg(i2c, s2mf301_mask_reg[i], s2mf301->irq_masks_cur[i]);
	}

	s2mf301->change_irq_mask = false;
skip:
	mutex_unlock(&s2mf301->irqlock);
}

static const inline struct s2mf301_irq_data *irq_to_s2mf301_irq(struct s2mf301_dev *s2mf301, int irq)
{
	return &s2mf301_irqs[irq - s2mf301->irq_base];
}

static void s2mf301_irq_mask(struct irq_data *data)
{
	struct s2mf301_dev *s2mf301 = irq_get_chip_data(data->irq);
	const struct s2mf301_irq_data *irq_data = irq_to_s2mf301_irq(s2mf301, data->irq);

	if (irq_data->group >= S2MF301_IRQ_GROUP_NR)
		return;

	s2mf301->irq_masks_cur[irq_data->group] |= irq_data->mask;
	s2mf301->change_irq_mask = true;
}

static void s2mf301_irq_unmask(struct irq_data *data)
{
	struct s2mf301_dev *s2mf301 = irq_get_chip_data(data->irq);
	const struct s2mf301_irq_data *irq_data = irq_to_s2mf301_irq(s2mf301, data->irq);

	if (irq_data->group >= S2MF301_IRQ_GROUP_NR)
		return;

	s2mf301->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	s2mf301->change_irq_mask = true;
}

static struct irq_chip s2mf301_irq_chip = {
	.name			= MFD_DEV_NAME_,
	.irq_bus_lock		= s2mf301_irq_lock,
	.irq_bus_sync_unlock	= s2mf301_irq_sync_unlock,
	.irq_mask		= s2mf301_irq_mask,
	.irq_unmask		= s2mf301_irq_unmask,
};

static irqreturn_t s2mf301_irq_thread(int irq, void *data)
{
	struct s2mf301_dev *s2mf301 = data;
	u8 irq_src;
	u8 irq_src2;
#if IS_ENABLED(CONFIG_CHARGER_S2MF301) || IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH) || \
	IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC) || IS_ENABLED(CONFIG_MUIC_S2MF301) || \
	IS_ENABLED(CONFIG_PM_S2MF301) || IS_ENABLED(CONFIG_REGULATOR_S2MF301) || \
	IS_ENABLED(CONFIG_FUELGAUGE_S2MF301) || IS_ENABLED(CONFIG_TOP_S2MF301)
	u8 irq_reg[S2MF301_IRQ_GROUP_NR] = {0, };
	int i;
#endif
	int ret;

	ret = s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_IPINT, &irq_src);
	if (ret) {
		s2mf301_err("%s:%s Failed to read interrupt source: %d\n", MFD_DEV_NAME_, __func__, ret);
		return IRQ_NONE;
	}
	s2mf301_info("%s: Top interrupt(0x%02x)\n", __func__, irq_src);
	ret = s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_IPINT2, &irq_src2);
	if (ret) {
		s2mf301_err("%s:%s Failed to read interrupt source: %d\n", MFD_DEV_NAME_, __func__, ret);
		return IRQ_NONE;
	}
	s2mf301_info("%s: Top interrupt RID(0x%02x)\n", __func__, irq_src2);

#if IS_ENABLED(CONFIG_TOP_S2MF301)
	if (irq_src & S2MF301_IRQSRC_DC) {
		s2mf301_read_reg(s2mf301->i2c, S2MF301_TOP_REG_DC_AUTO_PPS_INT, &irq_reg[DC_INT]);
		s2mf301_info("%s: DC interrupt(0x%02x)\n", __func__, irq_reg[DC_INT]);

	}

	if (s2mf301->evt0) {
		if (irq_src & S2MF301_IRQSRC_RID || irq_src & S2MF301_IRQSRC_FLED) {
			s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_IPINT2, &irq_reg[PM_RID_INT]);
			s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_FLEDINT, &irq_reg[FLED_INT]);
			s2mf301_read_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT, &irq_reg[PM_RID_INT]);
			s2mf301_read_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_TC_RID_INT, &irq_reg[CC_RID_INT]);
			s2mf301_info("%s: top_rid interrupt(0x%02x, 0x%02x, 0x%02x)\n",
					__func__, irq_reg[FLED_INT], irq_reg[PM_RID_INT], irq_reg[CC_RID_INT]);
		}
	} else {
		if (irq_src2 & S2MF301_IRQSRC_CC_RID) {
			s2mf301_read_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_TC_RID_INT, &irq_reg[CC_RID_INT]);
			s2mf301_info("%s: CC RID interrupt(0x%02x)\n", __func__,
					irq_reg[CC_RID_INT]);
		}
		if (irq_src2 & S2MF301_IRQSRC_PM_RID) {
			s2mf301_read_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT, &irq_reg[PM_RID_INT]);
			s2mf301_info("%s: PM RID interrupt(0x%02x)\n", __func__,
					irq_reg[PM_RID_INT]);
		}
	}
#endif

#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	if (irq_src & S2MF301_IRQSRC_CHG) {
		ret = s2mf301_bulk_read(s2mf301->chg, S2MF301_CHG_INT0, S2MF301_NUM_IRQ_CHG_REGS,
				&irq_reg[CHG_INT0]);
		if (ret) {
			s2mf301_err("%s:%s Failed to read charger interrupt: %d\n", MFD_DEV_NAME_, __func__, ret);
			return IRQ_NONE;
		}
		s2mf301_info("%s() CHARGER interrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
				__func__, irq_reg[CHG_INT0], irq_reg[CHG_INT1], irq_reg[CHG_INT2],
				irq_reg[CHG_INT3], irq_reg[CHG_INT4], irq_reg[CHG_INT5], irq_reg[CHG_INT6]);
	}
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	if (irq_src & S2MF301_IRQSRC_FG) {
		s2mf301_read_reg(s2mf301->fg, S2MF301_REG_FG_INT, &irq_reg[FG_INT]);
		s2mf301_info("%s: S2MF301_IRQSRC_FG(0x%02x)\n", __func__, irq_reg[FG_INT]);
	}
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	if (irq_src & S2MF301_IRQSRC_FLED && !s2mf301->evt0) {
		s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_FLEDINT, &irq_reg[FLED_INT]);
		s2mf301_info("%s: S2MF301_IRQSRC_FLED(0x%02x)\n", __func__, irq_reg[FLED_INT]);
	}
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	if (irq_src & S2MF301_IRQSRC_AFC) {
		s2mf301_read_reg(s2mf301->muic, S2MF301_REG_AFC_INT, &irq_reg[AFC_INT]);
		s2mf301_info("%s: AFC interrupt(0x%02x)\n", __func__, irq_reg[AFC_INT]);
	}
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	if (irq_src & S2MF301_IRQSRC_MUIC) {
		s2mf301_read_reg(s2mf301->muic, S2MF301_REG_MUIC_INT, &irq_reg[MUIC_INT]);
		s2mf301_info("%s: muic interrupt(0x%02x)\n", __func__, irq_reg[MUIC_INT]);
	}
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	if (irq_src & S2MF301_IRQSRC_PM) {
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_REQ_DONE1, &irq_reg[PM_ADC_REQ_DONE1]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_REQ_DONE2, &irq_reg[PM_ADC_REQ_DONE2]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_REQ_DONE3, &irq_reg[PM_ADC_REQ_DONE3]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_REQ_DONE4, &irq_reg[PM_ADC_REQ_DONE4]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_CHANGE_INT1, &irq_reg[PM_ADC_CHANGE_INT1]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_CHANGE_INT2, &irq_reg[PM_ADC_CHANGE_INT2]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_CHANGE_INT3, &irq_reg[PM_ADC_CHANGE_INT3]);
		s2mf301_read_reg(s2mf301->pm, S2MF301_REG_PM_ADC_CHANGE_INT4, &irq_reg[PM_ADC_CHANGE_INT4]);

		s2mf301_info("%s: powermeter interrupt RR(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[PM_ADC_REQ_DONE1], irq_reg[PM_ADC_REQ_DONE2], irq_reg[PM_ADC_REQ_DONE3], irq_reg[PM_ADC_REQ_DONE4]);

		s2mf301_info("%s: powermeter interrupt CO(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[PM_ADC_CHANGE_INT1], irq_reg[PM_ADC_CHANGE_INT1], irq_reg[PM_ADC_CHANGE_INT3], irq_reg[PM_ADC_CHANGE_INT4]);
	}
#endif

#if IS_ENABLED(CONFIG_REGULATOR_S2MF301)
	if (irq_src & S2MF301_IRQSRC_HBST)
		s2mf301_info("%s: S2MF301_IRQSRC_HBST\n", __func__);
#endif

#if IS_ENABLED(CONFIG_CHARGER_S2MF301) || IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH) || \
	IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC) || IS_ENABLED(CONFIG_MUIC_S2MF301) || \
	IS_ENABLED(CONFIG_PM_S2MF301) || IS_ENABLED(CONFIG_REGULATOR_S2MF301) || \
	IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	/* Apply masking */
	for (i = 0; i < S2MF301_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mf301->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MF301_IRQ_NR; i++) {
		if (irq_reg[s2mf301_irqs[i].group] & s2mf301_irqs[i].mask)
			handle_nested_irq(s2mf301->irq_base + i);
	}
#endif
	return IRQ_HANDLED;
}
static int irq_is_enable = true;
int s2mf301_irq_init(struct s2mf301_dev *s2mf301)
{
	int i, ret, cur_irq;
	struct i2c_client *i2c = s2mf301->i2c;
	u8 i2c_data;

	if (!s2mf301->irq_gpio) {
		dev_warn(s2mf301->dev, "No interrupt specified.\n");
		s2mf301->irq_base = 0;
		return 0;
	}

	if (!s2mf301->irq_base) {
		dev_err(s2mf301->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mf301->irqlock);

	s2mf301->irq = gpio_to_irq(s2mf301->irq_gpio);
	s2mf301_err("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME_, __func__, s2mf301->irq, s2mf301->irq_gpio);

	ret = gpio_request(s2mf301->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(s2mf301->dev, "%s: failed requesting gpio %d\n", __func__, s2mf301->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mf301->irq_gpio);
	gpio_free(s2mf301->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MF301_IRQ_GROUP_NR; i++) {
		s2mf301->irq_masks_cur[i] = 0xFF;
		s2mf301->irq_masks_cache[i] = 0xFF;

		i2c = get_i2c(s2mf301, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		if (i >= 0 && i < ARRAY_SIZE(s2mf301_mask_reg)) {
			if (s2mf301_mask_reg[i] == S2MF301_REG_INVALID)
				continue;

			s2mf301_write_reg(i2c, s2mf301_mask_reg[i], 0xFF);
		}
	}

	/* Register with genirq */
	for (i = 0; i < S2MF301_IRQ_NR; i++) {
		cur_irq = i + s2mf301->irq_base;
		irq_set_chip_data(cur_irq, s2mf301);
		irq_set_chip_and_handler(cur_irq, &s2mf301_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	/* Unmask S2MF301 interrupt */
	i2c_data = 0xFF;
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_CHG);
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_FG);
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	i2c_data &= ~(S2MF301_IRQSRC_FLED);
#endif
#if IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC)
	i2c_data &= ~(S2MF301_IRQSRC_AFC);
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_MUIC);
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_PM);
#endif
#if IS_ENABLED(CONFIG_REGULATOR_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_HBST);
#endif
#if IS_ENABLED(CONFIG_TOP_S2MF301)
	i2c_data &= ~(S2MF301_IRQSRC_DC);
	i2c_data &= ~(S2MF301_IRQSRC_RID);
	s2mf301_write_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_PM_RID_INT_MASK, (u8)(~(1 << 7)));
	s2mf301_write_reg(s2mf301->i2c, S2MF301_TOP_REG_TOP_TC_RID_INT_MASK, (u8)(~(1 << 7)));
#endif
	s2mf301_write_reg(s2mf301->i2c, S2MF301_REG_IPINT_MASK, i2c_data);
	s2mf301_info("%s: %s init top-irq mask(0x%02x)\n",
		MFD_DEV_NAME_, __func__, i2c_data);
	s2mf301_info("%s: irq gpio pre-state(0x%02x)\n", __func__, gpio_get_value(s2mf301->irq_gpio));

	if (irq_is_enable) {
		ret = request_threaded_irq(s2mf301->irq, NULL,
					   s2mf301_irq_thread,
					   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					   "s2mf301-irq", s2mf301);
	}

	if (ret) {
		dev_err(s2mf301->dev, "Failed to request IRQ %d: %d\n", s2mf301->irq, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_irq_init);

void s2mf301_irq_exit(struct s2mf301_dev *s2mf301)
{
	if (s2mf301->irq)
		free_irq(s2mf301->irq, s2mf301);
}
EXPORT_SYMBOL_GPL(s2mf301_irq_exit);
