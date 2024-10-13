/*
 * s2mc501_pmeter.c - S2MC501 Power Meter Driver
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
#include <linux/power/s2mc501_pmeter.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

static enum power_supply_property s2mc501_pmeter_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mc501_pm_enable(struct s2mc501_pmeter_data *pmeter, enum pm_type type)
{
	u8 r_data, w_data;
	int shift = 7 - (type & 0xFF);

	s2mc501_read_reg(pmeter->i2c, S2MC501_PM_CTRL1, &r_data);
	w_data = r_data | (1 << shift);
	s2mc501_write_reg(pmeter->i2c, S2MC501_PM_CTRL1, w_data);

	pr_info("%s, PM_CTRL1 : (0x%x) -> (0x%x)\n", __func__, r_data, w_data);

	return 0;
}

static int s2mc501_pm_get_value(struct s2mc501_pmeter_data *pmeter, int type)
{
    u8 addr1, addr2;
	u8 data1, data2;
	int charge_voltage = 0;

    switch (type) {
    case S2MC501_PM_TYPE_VDCIN:
    	addr1 = S2MC501_PM_VDCIN1;
	addr2 = S2MC501_PM_VDCIN2;
	break;
    case S2MC501_PM_TYPE_VOUT:
    	addr1 = S2MC501_PM_VOUT1;
	addr2 = S2MC501_PM_VOUT2;
	break;
    case S2MC501_PM_TYPE_VCELL:
    	addr1 = S2MC501_PM_VCELL1;
	addr2 = S2MC501_PM_VCELL2;
	break;
    case S2MC501_PM_TYPE_IIN:
    	addr1 = S2MC501_PM_IIN1;
	addr2 = S2MC501_PM_IIN2;
	break;
    case S2MC501_PM_TYPE_IINREV:
    	addr1 = S2MC501_PM_IINREV1;
	addr2 = S2MC501_PM_IINREV2;
	break;
    case S2MC501_PM_TYPE_TDIE:
    	addr1 = S2MC501_PM_TDIE1;
	addr2 = S2MC501_PM_TDIE2;
	break;
    default:
        pr_info("%s, invalid type(%d)\n", __func__, type);
        return -EINVAL;
    }

	s2mc501_read_reg(pmeter->i2c, addr1, &data1);
	s2mc501_read_reg(pmeter->i2c, addr2, &data2);

	charge_voltage = (((data2 << 8) | data1) * 1000) >> 11;

	pr_info("%s, %x, %x -> %x\n", __func__, data1, data2, charge_voltage);

	return charge_voltage;
}

static int s2mc501_pm_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mc501_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_VDCIN:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_VDCIN);
			break;
		case POWER_SUPPLY_S2M_PROP_VOUT:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_VOUT);
			break;
		case POWER_SUPPLY_S2M_PROP_VCELL:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_VCELL);
			break;
		case POWER_SUPPLY_S2M_PROP_IIN:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_IIN);
			break;
		case POWER_SUPPLY_S2M_PROP_IINREV:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_IINREV);
			break;
		case POWER_SUPPLY_S2M_PROP_TDIE:
			val->intval = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_TDIE);
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

static int s2mc501_pm_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct s2mc501_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CO_ENABLE:
			s2mc501_pm_enable(pmeter, val->intval);
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

#if 0
static irqreturn_t s2mc501_vdcin_isr(int irq, void *data)
{
	struct s2mc501_pmeter_data *pmeter = data;
	struct power_supply *psy;
	union power_supply_propval value;
	int voltage, ret = 0;

	psy = power_supply_get_by_name("s2mc501-charger");

	voltage = s2mc501_pm_get_value(pmeter, S2MC501_PM_TYPE_VCHGIN);

	pr_info("%s voltage : %d", __func__, voltage);
	if (voltage >= 6500) {
		value.intval = 1;
		ret = power_supply_set_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHG_EFFICIENCY, &value);
	} else {
		value.intval = 0;
		ret = power_supply_set_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHG_EFFICIENCY, &value);
	}
	return IRQ_HANDLED;
}
#endif
static const struct of_device_id s2mc501_pmeter_match_table[] = {
	{ .compatible = "samsung,s2mc501-pmeter",},
	{},
};

static void s2mc501_powermeter_initial(struct s2mc501_pmeter_data *pmeter)
{
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_VDCIN);
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_VOUT);
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_VCELL);
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_IIN);
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_IINREV);
	s2mc501_pm_enable(pmeter, S2MC501_PM_TYPE_TDIE);
}

int s2mc501_pmeter_init(struct s2mc501_dc_data *charger)
{
	struct s2mc501_pmeter_data *pmeter;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MC501 Power meter driver init\n", __func__);
	pmeter = kzalloc(sizeof(struct s2mc501_pmeter_data), GFP_KERNEL);
	if (!pmeter)
		return -ENOMEM;

	pmeter->i2c = charger->i2c;
//	pmeter->irq_base = charger->irq_base;

	pmeter->psy_pm_desc.name           = "s2mc501-pmeter"; //pmeter->pdata->powermeter_name;
	pmeter->psy_pm_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	pmeter->psy_pm_desc.get_property   = s2mc501_pm_get_property;
	pmeter->psy_pm_desc.set_property   = s2mc501_pm_set_property;
	pmeter->psy_pm_desc.properties     = s2mc501_pmeter_props;
	pmeter->psy_pm_desc.num_properties = ARRAY_SIZE(s2mc501_pmeter_props);

	psy_cfg.drv_data = pmeter;

	pmeter->psy_pm = power_supply_register(&pmeter->i2c->dev, &pmeter->psy_pm_desc, &psy_cfg);
	if (IS_ERR(pmeter->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(pmeter->psy_pm);
		goto err_power_supply_register;
	}
#if 0
	pmeter->irq_vdcin = pmeter->irq_base + S2MC501_PM_ADC_INT2_VDCIN;
	ret = request_threaded_irq(pmeter->irq_vdcin, NULL, s2mc501_vdcin_isr, 0, "vdcin-irq", pmeter);
	if (ret < 0)
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n", __func__, pmeter->irq_vdcin, ret);
#endif
	s2mc501_powermeter_initial(pmeter);

	pr_info("%s:[BATT] S2MC501 pmeter driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	mutex_destroy(&pmeter->pmeter_mutex);
	kfree(pmeter);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mc501_pmeter_init);

