/*
 * s2mf301_pmeter.c - S2MF301 Power Meter Driver
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
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include "s2mf301_pmeter.h"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
#include <linux/usb/typec/slsi/s2mf301/s2mf301-water.h>
#endif

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static bool s2mf301_pm_rid_is_enable(void *_data);
static int s2mf301_pm_control_rid_adc(void *_data, bool enable);
static int s2mf301_pm_get_rid_adc(void *_data);
static int s2mf301_pm_mask_rid_change(void *_data, bool enable);
static struct s2mf301_pm_rid_ops _s2mf301_pm_rid_ops = {
	.rid_is_enable			= s2mf301_pm_rid_is_enable,
	.control_rid_adc		= s2mf301_pm_control_rid_adc,
	.get_rid_adc			= s2mf301_pm_get_rid_adc,
	.mask_rid_change		= s2mf301_pm_mask_rid_change,
};
#endif

static enum power_supply_property s2mf301_pmeter_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if 0
static void s2mf301_pm_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	int cnt = 0;

	for (i = 0x08; i <= 0xe2; i++) {
		s2mf301_read_reg(i2c, i, &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);

		if (cnt > 20) {
			pr_info("%s: %s\n", __func__, str);
			memset(str, 0, sizeof(str));
			cnt = 0;
		}
	}
	pr_info("%s: %s\n", __func__, str);
}
#endif

static int s2mf301_pm_enable(struct s2mf301_pmeter_data *pmeter, int mode, int enable, enum pm_type type)
{
	u8 addr, r_data, w_data;
	int shift = 7 - (type & 0x111);

	if (mode > REQUEST_RESPONSE_MODE) {
		pr_info("%s, invalid mode(%d)\n", __func__, mode);
		return -EINVAL;
	}

	switch (type) {
	case S2MF301_PM_TYPE_VCHGIN:
	case S2MF301_PM_TYPE_VWCIN:
	case S2MF301_PM_TYPE_VBYP:
	case S2MF301_PM_TYPE_VSYS:
	case S2MF301_PM_TYPE_VBAT:
	case S2MF301_PM_TYPE_VCC1:
	case S2MF301_PM_TYPE_VCC2:
	case S2MF301_PM_TYPE_TDIE:
		if (mode == CONTINUOUS_MODE)
			addr = S2MF301_REG_PM_ADCEN_CONTINU1;
		else
			addr = S2MF301_REG_PM_ADCEN_1TIME_REQ1;
		break;
	case S2MF301_PM_TYPE_ICHGIN:
	case S2MF301_PM_TYPE_IWCIN:
	case S2MF301_PM_TYPE_IOTG:
	case S2MF301_PM_TYPE_ITX:
	case S2MF301_PM_TYPE_GPADC1:
	case S2MF301_PM_TYPE_GPADC2:
	case S2MF301_PM_TYPE_GPADC3:
	case S2MF301_PM_TYPE_VDCIN:
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	case S2MF301_PM_TYPE_GPADC12:
#endif
		if (mode == CONTINUOUS_MODE)
			addr = S2MF301_REG_PM_ADCEN_CONTINU2;
		else
			addr = S2MF301_REG_PM_ADCEN_1TIME_REQ2;
		break;
	default:
		pr_info("%s, invalid type(%d)\n", __func__, type);
		return -EINVAL;
	}

	s2mf301_read_reg(pmeter->i2c, addr, &r_data);
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	if (type == S2MF301_PM_TYPE_GPADC12) {
		pr_info("%s, %s gpadc12 %s\n", __func__, ((enable)?"Enable":"Disable"),
					((mode == CONTINUOUS_MODE)?"CO":"RR"));
		if (enable)
			w_data = r_data | 0x0C;
		else
			w_data = r_data & ~(0x0C);
	} else
#endif
		if (enable)
			w_data = r_data | (1 << shift);
		else
			w_data = r_data & ~(1 << shift);
	s2mf301_write_reg(pmeter->i2c, addr, w_data);

	pr_info("%s, reg(0x%x) : (0x%x) -> (0x%x)\n", __func__, addr, r_data, w_data);

	return 0;
}

static int s2mf301_pm_get_value(struct s2mf301_pmeter_data *pmeter, int type)
{
	u8 addr1, addr2;
	u8 data1, data2;
	int charge_voltage = 0;

	switch (type) {
	case S2MF301_PM_TYPE_VCHGIN:
		addr1 = S2MF301_REG_PM_VADC1_CHGIN;
		addr2 = S2MF301_REG_PM_VADC2_CHGIN;
		break;
	case S2MF301_PM_TYPE_VWCIN:
		addr1 = S2MF301_REG_PM_VADC1_WCIN;
		addr2 = S2MF301_REG_PM_VADC2_WCIN;
		break;
	case S2MF301_PM_TYPE_VBYP:
		addr1 = S2MF301_REG_PM_VADC1_BYP;
		addr2 = S2MF301_REG_PM_VADC2_BYP;
		break;
	case S2MF301_PM_TYPE_VSYS:
		addr1 = S2MF301_REG_PM_VADC1_SYS;
		addr2 = S2MF301_REG_PM_VADC2_SYS;
		break;
	case S2MF301_PM_TYPE_VBAT:
		addr1 = S2MF301_REG_PM_VADC1_BAT;
		addr2 = S2MF301_REG_PM_VADC2_BAT;
		break;
	case S2MF301_PM_TYPE_VCC1:
		addr1 = S2MF301_REG_PM_VADC1_CC1;
		addr2 = S2MF301_REG_PM_VADC2_CC1;
		break;
	case S2MF301_PM_TYPE_VCC2:
		addr1 = S2MF301_REG_PM_VADC1_CC2;
		addr2 = S2MF301_REG_PM_VADC2_CC2;
		break;
	case S2MF301_PM_TYPE_ICHGIN:
		addr1 = S2MF301_REG_PM_IADC1_CHGIN;
		addr2 = S2MF301_REG_PM_IADC2_CHGIN;
		break;
	case S2MF301_PM_TYPE_IWCIN:
		addr1 = S2MF301_REG_PM_IADC1_WCIN;
		addr2 = S2MF301_REG_PM_IADC2_WCIN;
		break;
	case S2MF301_PM_TYPE_IOTG:
		addr1 = S2MF301_REG_PM_IADC1_OTG;
		addr2 = S2MF301_REG_PM_IADC2_OTG;
		break;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	case S2MF301_PM_TYPE_GPADC1:
		addr1 = S2MF301_REG_PM_VADC1_GPADC1;
		addr2 = S2MF301_REG_PM_VADC2_GPADC1;
		break;
	case S2MF301_PM_TYPE_GPADC2:
		addr1 = S2MF301_REG_PM_VADC1_GPADC2;
		addr2 = S2MF301_REG_PM_VADC2_GPADC2;
		break;
#endif
	case S2MF301_PM_TYPE_GPADC3:
	case S2MF301_PM_TYPE_VDCIN:
	case S2MF301_PM_TYPE_ITX:
	case S2MF301_PM_TYPE_TDIE:
	default:
		pr_info("%s, invalid type(%d)\n", __func__, type);
		return -EINVAL;
	}

	s2mf301_read_reg(pmeter->i2c, addr1, &data1);
	s2mf301_read_reg(pmeter->i2c, addr2, &data2);

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	if (type == S2MF301_PM_TYPE_GPADC1 || type == S2MF301_PM_TYPE_GPADC2) {
		if (pmeter->water_adc_mode == S2MF301_GPADC_RMODE)
			charge_voltage = (data1<<8) | data2;
		else //Vmode
			charge_voltage = (((data1<<8)|data2) * 1000) >> 11;
	} else
#endif
		charge_voltage = (((data1<<8)|data2) * 1000) >> 11;

	return charge_voltage;
}

#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
int s2mf301_pm_ops_pm_enable(void *data, int mode, int enable, int type)
{
	struct s2mf301_pmeter_data *pmeter = (struct s2mf301_pmeter_data *)data;
	int t = (enum pm_type) type;

	return s2mf301_pm_enable(pmeter, mode, enable, t);
}

int	s2mf301_pm_ops_pm_get_value(void *data, int type)
{
	struct s2mf301_pmeter_data *pmeter = (struct s2mf301_pmeter_data *)data;

	return s2mf301_pm_get_value(pmeter, type);
}

void s2mf301_pm_water_irq_masking(void *data, int masking, int mask)
{
	struct s2mf301_pmeter_data *pmeter = data;
	u8 r_data, w_data;
	u8 reg[3];

	/*
	 * masking==true -> disable irq
	 * masking==false -> enable irq
	 */
	const char *mask_to_str[] = {
		"CHANGE",
		"WATER",
		"RR",
	};

	pr_info("%s, %s %s\n", __func__, ((masking)?"masking":"unmasking"),
			mask_to_str[mask]);

	switch (mask) {
		case S2MF301_IRQ_TYPE_CHANGE:
			s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT2_MASK, &r_data);
			if (masking) {
				w_data = r_data & ~(0x0c);
				w_data |= 0x0c;
			} else {
				w_data = r_data & ~(0x0c);
			}
			s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT2_MASK, w_data);
			break;
		case S2MF301_IRQ_TYPE_WATER:
			s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE4_MASK, &r_data);
			if (masking) {
				w_data = r_data & ~(0x03);
				w_data |= 0x03;
			} else {
				w_data = r_data & ~(0x03);
			}
			s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE4_MASK, w_data);
			break;
		case S2MF301_IRQ_TYPE_RR:
			s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE2_MASK, &r_data);
			if (masking) {
				w_data = r_data & ~(0x0c);
				w_data |= 0x0c;
			} else {
				w_data = r_data & ~(0x0c);
			}
			s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE2_MASK, w_data);
			break;
		default:
			break;
	}


	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT2_MASK, &reg[0]);
	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE4_MASK, &reg[1]);
	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE2_MASK, &reg[2]);

	pr_info("%s, change[0x%x], water[0x%x], rr[0x%x]\n", __func__, reg[0], reg[1], reg[2]);
}

static void s2mf301_pm_water_set_gpadc_mode(void *data, int mode)
{
	struct s2mf301_pmeter_data *pmeter = (struct s2mf301_pmeter_data *)data;
	u8 reg;

	pmeter->water_adc_mode = mode;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_CTRL1, &reg);
	switch (mode){
	case S2MF301_GPADC_RMODE:
		pr_info("%s, GPADC12 RMode\n", __func__);
		reg &= ~(0xf0);
		reg |= 0x50;
		break;
	case S2MF301_GPADC_VMODE:
		pr_info("%s, GPADC12 VMode\n", __func__);
		reg &= ~(0xf0);
		reg |= 0xa0;
		break;
	default:
		break;
	}
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_CTRL1, reg);
}

static int s2mf301_pm_water_get_status_reg(struct s2mf301_pmeter_data *pmeter)
{
	u8 data;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ICHG_STATUS, &data);
	pr_info("[WATER] %s, reg[0x%x]\n", __func__, data);

	if ((data & 0xA0) == 0xA0)
		return S2MF301_WATER_GPADC12;
	else if ((data & 0xA0) == 0x80)
		return S2MF301_WATER_GPADC1;
	else if ((data & 0xA0) == 0x20)
		return S2MF301_WATER_GPADC2;
	else
		return S2MF301_WATER_GPADC_DRY;
}

static void s2mf301_pm_water_det_en(void *data, int enable)
{
	struct s2mf301_pmeter_data *pmeter = data;
	u8 reg;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_WET_EN1, &reg);
	if (enable)
		reg |= S2MF301_REG_PM_WET_EN1_GPADC1;
	else
		reg &= ~(S2MF301_REG_PM_WET_EN1_GPADC1);
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_EN1, reg);

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_WET_EN2, &reg);
	if (enable)
		reg |= S2MF301_REG_PM_WET_EN2_GPADC2;
	else
		reg &= ~(S2MF301_REG_PM_WET_EN2_GPADC2);
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_EN2, reg);

	pr_info("%s, %s\n", __func__, (enable?"Enabled":"Disabled"));
}

static void s2mf301_pm_water_10s_en(void *data, int enable)
{
	struct s2mf301_pmeter_data *pmeter = data;
	u8 reg;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_WET_DET_CTRL2, &reg);
	if (enable) {
		reg |= 0x30;
	} else {
		reg &= ~(0x30);
	}
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_DET_CTRL2, reg);

	pr_info("%s, %s\n", __func__, (enable?"Enabled":"Disabled"));
}

static void s2mf301_pm_water_set_status(void *data, int enable)
{
	struct s2mf301_pmeter_data *pmeter = data;
	if (enable == S2M_WATER_STATUS_WATER) {
		s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_DET_CTRL1, 0xF0);
		s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_DET_CTRL1, 0x50);
	} else if (enable == S2M_WATER_STATUS_DRY) {
		s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_WET_DET_CTRL1, 0x00);
	} else
		pr_info("%s, invalid enable(%d)\n", __func__, enable);

	s2mf301_pm_water_get_status_reg(pmeter);
}

static int s2mf301_pm_water_get_status(void *data)
{
	struct s2mf301_pmeter_data *pmeter = (struct s2mf301_pmeter_data *)data;

	return s2mf301_pm_water_get_status_reg(pmeter);
}

static irqreturn_t s2mf301_pm_gpadc1up_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;
	int volt;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	volt = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC1);
	pr_info("%s, vgpadc1 = %dmV\n", __func__, volt);

	water->vgpadc1 = volt;

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	if (water->vgpadc1 > 0 && water->vgpadc2 > 0) {
		pr_info("[WATER][DEBUG] %s, vgpadc1(%d), vgpadc2(%d)\n", __func__,
				water->vgpadc1, water->vgpadc2);
		complete(&water->water_rr_compl);
	}
#endif

	return IRQ_HANDLED;
}

static  irqreturn_t s2mf301_pm_gpadc2up_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;
	int volt;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	volt = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC2);
	pr_info("%s, vgpadc2 = %dmV\n", __func__, volt);

	water->vgpadc2 = volt;

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	if (water->vgpadc1 > 0 && water->vgpadc2 > 0) {
		pr_info("[WATER][DEBUG] %s, vgpadc1(%d), vgpadc2(%d)\n", __func__,
				water->vgpadc1, water->vgpadc2);
		complete(&water->water_rr_compl);
	}
#endif

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_pm_gpadc1_change_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;
	int volt;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	volt = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC1);
	pr_info("%s, vgpadc1 = %dmV\n", __func__, volt);
	//s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	//s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, 0,  S2MF301_PM_TYPE_GPADC12);

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	cancel_delayed_work(&water->state_work);
	water->event = S2M_WATER_EVENT_SBU_CHANGE_INT;
	schedule_delayed_work(&water->state_work, msecs_to_jiffies(5));
#endif

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_pm_gpadc2_change_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;
	int volt;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	volt = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC2);
	pr_info("%s, vgpadc2 = %dmV\n", __func__, volt);
	//s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	//s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, 0,  S2MF301_PM_TYPE_GPADC12);

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	cancel_delayed_work(&water->state_work);
	water->event = S2M_WATER_EVENT_SBU_CHANGE_INT;
	schedule_delayed_work(&water->state_work, msecs_to_jiffies(5));
#endif

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_pm_water1_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	pr_info("%s, \n", __func__);

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	cancel_delayed_work(&water->state_work);
	water->event = S2M_WATER_EVENT_WET_INT;
	schedule_delayed_work(&water->state_work, msecs_to_jiffies(5));
#endif

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_pm_water2_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct s2mf301_water_data *water;

	if (!pmeter->water) {
		pr_info("%s, pmeter->water is NULL\n", __func__);
		return -1;
	}
	water = pmeter->water;

	pr_info("%s, \n", __func__);

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	cancel_delayed_work(&water->state_work);
	water->event = S2M_WATER_EVENT_WET_INT;
	schedule_delayed_work(&water->state_work, msecs_to_jiffies(5));
#endif

	return IRQ_HANDLED;
}

void *s2mf301_pm_water_init(struct s2mf301_water_data *water)
{
	struct power_supply *psy;
	struct s2mf301_pmeter_data *pmeter;
	u8 irq_reg[8] = {0, };

	psy = get_power_supply_by_name("s2mf301-pmeter");
	if (!psy) {
		pr_info("%s, Fail to get psy\n", __func__);
		return NULL;
	}

	pmeter = power_supply_get_drvdata(psy);
	if (!pmeter) {
		pr_info("%s, Fail to get drv data\n", __func__);
		return NULL;
	}

	pmeter->water = water;

	pr_info("[WATER], %s\n", __func__);

	pmeter->water_adc_mode = S2MF301_GPADC_RMODE;

	/*
	 * GPADC12 hyst setting
	 * 0x73[2:0], 0x74[6:4]
	 * 2^(5+3bit)code, ADC value check
	 * 0x55 -> 2^10 code -> ADC 1024code diff -> int
	 */
	s2mf301_write_reg(pmeter->i2c, 0x73, 0x54);
	s2mf301_write_reg(pmeter->i2c, 0x74, 0x45);
	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_RR);
	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_WATER);

	s2mf301_bulk_read(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE1, 8, &irq_reg[0]);
	pr_info("%s: powermeter interrupt RR(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3]);
	pr_info("%s: powermeter interrupt CO(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[4], irq_reg[5], irq_reg[6], irq_reg[7]); 

	/* Water Threshold 0x84*8 = 1056mV */
	s2mf301_write_reg(pmeter->i2c, 0x84, 0x84);
	/* Dry Threshold 0x61*8 = 776mV */
	s2mf301_write_reg(pmeter->i2c, 0x85, 0x61);

	pmeter->water->water_irq_masking = s2mf301_pm_water_irq_masking;
	pmeter->water->water_det_en = s2mf301_pm_water_det_en;
	pmeter->water->water_10s_en = s2mf301_pm_water_10s_en;
	pmeter->water->water_set_gpadc_mode = s2mf301_pm_water_set_gpadc_mode;
	pmeter->water->water_set_status = s2mf301_pm_water_set_status;
	pmeter->water->water_get_status = s2mf301_pm_water_get_status;
	pmeter->water->pm_enable = s2mf301_pm_ops_pm_enable;
	pmeter->water->pm_get_value = s2mf301_pm_ops_pm_get_value;

	return pmeter;
}
EXPORT_SYMBOL_GPL(s2mf301_pm_water_init);

void s2mf301_pm_water_irq_init(struct s2mf301_water_data *water)
{
	int ret;
	struct power_supply *psy;
	struct s2mf301_pmeter_data *pmeter;
	struct s2mf301_dev *s2mf301;

	psy = get_power_supply_by_name("s2mf301-pmeter");
	if (!psy) {
		pr_info("%s, Fail to get psy\n", __func__);
		return;
	}

	pmeter = power_supply_get_drvdata(psy);
	if (!pmeter) {
		pr_info("%s, Fail to get drv data\n", __func__);
		return;
	}

	s2mf301 = pmeter->s2mf301;
	if (!s2mf301) {
		pr_info("%s, Fail to get mfd data\n", __func__);
		return;
	}

	pmeter->irq_comp1 = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT2_GPADC1UP;
	ret = request_threaded_irq(pmeter->irq_comp1, NULL,
			s2mf301_pm_gpadc1_change_isr, 0, "gpadc1-change-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_comp1, ret);

	pmeter->irq_comp2 = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT2_GPADC2UP;
	ret = request_threaded_irq(pmeter->irq_comp2, NULL,
			s2mf301_pm_gpadc2_change_isr, 0, "gpadc2-change-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_comp2, ret);

	pmeter->irq_water_status1 = s2mf301->pdata->irq_base + S2MF301_PM_ADC_REQ_DONE4_WET_STATUS_GPADC1 ;
	ret = request_threaded_irq(pmeter->irq_water_status1, NULL,
			s2mf301_pm_water1_isr, 0, "water1-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_water_status1, ret);

	pmeter->irq_water_status2 = s2mf301->pdata->irq_base + S2MF301_PM_ADC_REQ_DONE4_WET_STATUS_GPADC2 ;
	ret = request_threaded_irq(pmeter->irq_water_status2, NULL,
			s2mf301_pm_water2_isr, 0, "water2-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__,
				pmeter->irq_water_status2, ret);

	pmeter->irq_gpadc1up = s2mf301->pdata->irq_base + S2MF301_PM_ADC_REQ_DONE2_GPADC1UP ;
	ret = request_threaded_irq(pmeter->irq_gpadc1up, NULL,
			s2mf301_pm_gpadc1up_isr, 0, "gpadc1up-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_gpadc1up, ret);

	pmeter->irq_gpadc2up = s2mf301->pdata->irq_base + S2MF301_PM_ADC_REQ_DONE2_GPADC2UP ;
	ret = request_threaded_irq(pmeter->irq_gpadc2up, NULL,
			s2mf301_pm_gpadc2up_isr, 0, "gpadc2up-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_gpadc2up, ret);


}
EXPORT_SYMBOL_GPL(s2mf301_pm_water_irq_init);
#endif

static int s2mf301_pm_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property) psp;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	int volt[2] = {0, };
#endif

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_VCHGIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCHGIN);
			break;
		case POWER_SUPPLY_LSI_PROP_VWCIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VWCIN);
			break;
		case POWER_SUPPLY_LSI_PROP_VBYP:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VBYP);
			break;
		case POWER_SUPPLY_LSI_PROP_VSYS:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VSYS);
			break;
		case POWER_SUPPLY_LSI_PROP_VBAT:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VBAT);
			break;
		case POWER_SUPPLY_LSI_PROP_VGPADC:
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
			s2mf301_pm_water_set_gpadc_mode(pmeter, S2MF301_GPADC_VMODE);
			s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, 0,  S2MF301_PM_TYPE_GPADC12);
			s2mf301_pm_water_det_en(pmeter, false);
			s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_CHANGE);

			usleep_range(10000, 10100);

			s2mf301_pm_water_irq_masking(pmeter, false, S2MF301_IRQ_TYPE_RR);
			s2mf301_pm_enable(pmeter, REQUEST_RESPONSE_MODE, true, S2MF301_PM_TYPE_GPADC12);

			msleep(30);
			s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_RR);
			s2mf301_pm_enable(pmeter, REQUEST_RESPONSE_MODE, false, S2MF301_PM_TYPE_GPADC12);

			volt[0] = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC1);
			volt[1] = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC2);
			pr_info("%s, volt(%d, %d)mV\n", __func__, volt[0], volt[1]);

			val->intval = (volt[0] > volt[1] ? volt[0] : volt[1]);

			s2mf301_pm_water_set_gpadc_mode(pmeter, S2MF301_GPADC_RMODE);
			s2mf301_pm_water_det_en(pmeter, true);
			s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, 1,  S2MF301_PM_TYPE_GPADC12);
			msleep(20);
			s2mf301_pm_water_irq_masking(pmeter, false, S2MF301_IRQ_TYPE_CHANGE);
#endif
			break;
		case POWER_SUPPLY_LSI_PROP_VGPADC1:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC1);
			break;
		case POWER_SUPPLY_LSI_PROP_VGPADC2:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC2);
			break;
		case POWER_SUPPLY_LSI_PROP_VCC1:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCC1);
			break;
		case POWER_SUPPLY_LSI_PROP_VCC2:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCC2);
			break;
		case POWER_SUPPLY_LSI_PROP_ICHGIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_ICHGIN);
			break;
		case POWER_SUPPLY_LSI_PROP_IWCIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_IWCIN);
			break;
		case POWER_SUPPLY_LSI_PROP_IOTG:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_IOTG);
			break;
		case POWER_SUPPLY_LSI_PROP_ITX:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_ITX);
			break;
		case POWER_SUPPLY_LSI_PROP_RID_OPS:
#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
			_s2mf301_pm_rid_ops._data = pmeter;
			pmeter->muic_data = (struct s2mf301_muic_data *)val->strval;
			val->strval = (const char *)&_s2mf301_pm_rid_ops;
#endif
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mf301_pm_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mf301_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property) psp;
	u8 reg[6] = {0, };

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		s2mf301_read_reg(pmeter->i2c, 0x3c, &reg[0]);
		s2mf301_read_reg(pmeter->i2c, 0x3d, &reg[1]);
		s2mf301_read_reg(pmeter->i2c, 0x40, &reg[2]);
		s2mf301_read_reg(pmeter->i2c, 0x41, &reg[3]);
		s2mf301_read_reg(pmeter->i2c, 0x50, &reg[4]);
		s2mf301_read_reg(pmeter->i2c, 0x51, &reg[5]);
		pr_info("%s, 3c[0x%x], 3d[0x%x], 40[0x%x], 41[0x%x], 50[0x%x], 51[0x%x]\n",
				__func__, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
		return 1;
	case POWER_SUPPLY_LSI_PROP_MIN ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
		case POWER_SUPPLY_LSI_PROP_CO_ENABLE:
			s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, true, val->intval);
			break;
		case POWER_SUPPLY_LSI_PROP_RR_ENABLE:
			s2mf301_pm_enable(pmeter, REQUEST_RESPONSE_MODE, true, val->intval);
			break;
		case POWER_SUPPLY_LSI_PROP_ENABLE_WATER:
			break;
		default:
			return -EINVAL;
		}
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
static bool s2mf301_pm_rid_is_enable(void *_data)
{
	struct s2mf301_pmeter_data *pmeter;
	bool ret = false;
	u8 reg_data = 0;

	if (!_data) {
		pr_info("%s, _data is NULL\n", __func__);
		return 0;
	}
	pmeter = _data;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CTRL8, &reg_data);
	pr_info("%s, adc_ctrl8 : 0x%x\n", __func__, reg_data);

	if (reg_data & S2MF301_PM_ADC_ENABLE_MASK)
		ret = true;
	else
		ret = false;

	return ret;
}

static int s2mf301_pm_control_rid_adc(void *_data, bool enable)
{
	struct s2mf301_pmeter_data *pmeter;
	u8 reg_data = 0;

	if (!_data) {
		pr_info("%s, _data is NULL\n", __func__);
		return -EINVAL;
	}
	pmeter = _data;

	pr_info("%s, %s\n", __func__, ((enable) ? "Enable" : "Disable"));

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CTRL8, &reg_data);
	if (enable)
		reg_data |= S2MF301_PM_ADC_ENABLE_MASK;
	else
		reg_data &= ~(S2MF301_PM_ADC_ENABLE_MASK);
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CTRL8, reg_data);

	return 0;
}

static int s2mf301_pm_mask_rid_change(void *_data, bool enable)
{
	struct s2mf301_pmeter_data *pmeter;
	u8 reg_data;

	if (!_data) {
		pr_info("%s, _data is NULL\n", __func__);
		return -EINVAL;
	}
	pmeter = _data;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT4_MASK, &reg_data);
	reg_data &= ~S2MF301_PM_RID_ATTACH_DETACH_MASK;

	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT4_MASK, reg_data);

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_ADC_CHANGE_INT4_MASK, &reg_data);
	pr_info("%s, enable(%d) / always unmasking reg(0x%x)\n", __func__, enable, reg_data);

	return 0;
}

static int s2mf301_pm_get_rid_adc(void *_data)
{
	struct s2mf301_pmeter_data *pmeter;
	u8 reg_data = 0;
	int rid = 0;

	if (!_data) {
		pr_info("%s, _data is NULL\n", __func__);
		return -EINVAL;
	}
	pmeter = _data;

	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_RID_STATUS, &reg_data);
	pr_info("%s, status : 0x%x\n", __func__, reg_data);

	switch (reg_data & S2MF301_PM_RID_STATUS_MASK) {
	case S2MF301_PM_RID_STATUS_255K:
	case S2MF301_PM_RID_STATUS_56K:
		rid = ADC_JIG_USB_OFF;
		break;
	case S2MF301_PM_RID_STATUS_301K:
		rid = ADC_JIG_USB_ON;
		break;
	case S2MF301_PM_RID_STATUS_523K:
		rid = ADC_JIG_UART_OFF;
		break;
	case S2MF301_PM_RID_STATUS_619K:
		rid = ADC_JIG_UART_ON;
		break;
	default:
		pr_info("%s, invalid rid status, return ADC_GND\n", __func__);
		break;
	}

	return rid;
}

static irqreturn_t s2mf301_pm_rid_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	int ret = 0;

	pr_info("%s, \n", __func__);

	if (!pmeter->muic_data) {
		pr_info("%s, muic_data is NULL\n", __func__);
		return IRQ_HANDLED;
	}

	if (pmeter->muic_data->rid_isr)
		ret = pmeter->muic_data->rid_isr(pmeter->muic_data);
	else {
		pr_info("%s, pmeter->muic_data->rid_isr is NULL\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}
#endif

static irqreturn_t s2mf301_vchgin_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	int voltage;
	union power_supply_propval value;

	voltage = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCHGIN);

	pr_info("%s voltage : %d", __func__, voltage);

	psy_do_property("muic-manager", set,
		POWER_SUPPLY_LSI_PROP_PM_VCHGIN, value);

	return IRQ_HANDLED;
}

static irqreturn_t s2mf301_ichgin_th_isr(int irq, void *data)
{
	union power_supply_propval value;

	pr_info("%s\n", __func__);
	psy_do_property("s2mf301-charger", set,
		POWER_SUPPLY_LSI_PROP_ICHGIN, value);

	return IRQ_HANDLED;
}

static const struct of_device_id s2mf301_pmeter_match_table[] = {
	{ .compatible = "samsung,s2mf301-pmeter",},
	{},
};

static void s2mf301_powermeter_initial(struct s2mf301_pmeter_data *pmeter)
{
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_ICHGIN);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_VCHGIN);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_VCC1);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_VCC2);
}

static int s2mf301_pmeter_probe(struct platform_device *pdev)
{
	struct s2mf301_dev *s2mf301 = dev_get_drvdata(pdev->dev.parent);
	struct s2mf301_pmeter_data *pmeter;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MF301 Power meter driver probe\n", __func__);
	pmeter = kzalloc(sizeof(struct s2mf301_pmeter_data), GFP_KERNEL);
	if (!pmeter)
		return -ENOMEM;

	pmeter->dev = &pdev->dev;
	/* in mf301, PM uses 0x7E secondary address */
	pmeter->i2c = s2mf301->pm;
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	pmeter->s2mf301 = s2mf301;
#endif

	platform_set_drvdata(pdev, pmeter);

	pmeter->psy_pm_desc.name = "s2mf301-pmeter"; //pmeter->pdata->powermeter_name;
	pmeter->psy_pm_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	pmeter->psy_pm_desc.get_property = s2mf301_pm_get_property;
	pmeter->psy_pm_desc.set_property = s2mf301_pm_set_property;
	pmeter->psy_pm_desc.properties = s2mf301_pmeter_props;
	pmeter->psy_pm_desc.num_properties = ARRAY_SIZE(s2mf301_pmeter_props);

	psy_cfg.drv_data = pmeter;

	pmeter->psy_pm = power_supply_register(&pdev->dev, &pmeter->psy_pm_desc, &psy_cfg);
	if (IS_ERR(pmeter->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(pmeter->psy_pm);
		goto err_power_supply_register;
	}

	pmeter->irq_vchgin = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT1_VCHGINUP;
	ret = request_threaded_irq(pmeter->irq_vchgin, NULL, s2mf301_vchgin_isr, 0, "vchgin-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_vchgin, ret);

#if IS_ENABLED(CONFIG_MUIC_S2MF301_RID)
	pmeter->irq_rid_attach = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT4_PM_RID_ATTACH;
	ret = request_threaded_irq(pmeter->irq_rid_attach, NULL, s2mf301_pm_rid_isr, 0, "rid_attach_isr", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_rid_attach, ret);

	pmeter->irq_rid_detach = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT4_PM_RID_DETACH;
	ret = request_threaded_irq(pmeter->irq_rid_detach, NULL, s2mf301_pm_rid_isr, 0, "rid_detach_isr", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_rid_detach, ret);
#endif

	pmeter->irq_ichgin_th = s2mf301->pdata->irq_base + S2MF301_PM_ADC_CHANGE_INT4_ICHGIN_TH;
	ret = request_threaded_irq(pmeter->irq_ichgin_th, NULL, s2mf301_ichgin_th_isr, 0, "ichgin_th_isr", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_ichgin_th, ret);

	/* mask TYPE_C_0.8_TH */
	s2mf301_update_reg(pmeter->i2c, S2MF301_REG_PM_ADC_REQ_DONE4_MASK, 0x80, 0x80);

	s2mf301_powermeter_initial(pmeter);

	pr_info("%s:[BATT] S2MF301 pmeter driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	mutex_destroy(&pmeter->pmeter_mutex);
	kfree(pmeter);

	return ret;
}

static int s2mf301_pmeter_remove(struct platform_device *pdev)
{
	struct s2mf301_pmeter_data *pmeter =
		platform_get_drvdata(pdev);

	kfree(pmeter);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int s2mf301_pmeter_suspend(struct device *dev)
{
	return 0;
}

static int s2mf301_pmeter_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mf301_pmeter_suspend NULL
#define s2mf301_pmeter_resume NULL
#endif

static void s2mf301_pmeter_shutdown(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	struct s2mf301_pmeter_data *pmeter = platform_get_drvdata(pdev);
#endif
	pr_info("%s: S2MF301 PowerMeter driver shutdown\n", __func__);
	
#if IS_ENABLED(CONFIG_S2MF301_TYPEC_WATER)
	s2mf301_pm_water_det_en(pmeter, false);
	s2mf301_pm_water_10s_en(pmeter, false);

	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_RR);
	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_WATER);
	s2mf301_pm_water_irq_masking(pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
#endif
}

static SIMPLE_DEV_PM_OPS(s2mf301_pmeter_pm_ops, s2mf301_pmeter_suspend,
		s2mf301_pmeter_resume);

static struct platform_driver s2mf301_pmeter_driver = {
	.driver = {
		.name = "s2mf301-powermeter",
		.owner = THIS_MODULE,
		.of_match_table = s2mf301_pmeter_match_table,
		.pm = &s2mf301_pmeter_pm_ops,
	},
	.probe = s2mf301_pmeter_probe,
	.remove = s2mf301_pmeter_remove,
	.shutdown = s2mf301_pmeter_shutdown,
};

static int __init s2mf301_pmeter_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	ret = platform_driver_register(&s2mf301_pmeter_driver);

	return ret;
}
module_init(s2mf301_pmeter_init);

static void __exit s2mf301_pmeter_exit(void)
{
	platform_driver_unregister(&s2mf301_pmeter_driver);
}
module_exit(s2mf301_pmeter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("PowerMeter driver for S2MF301");
