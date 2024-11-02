/*
 * s2mf301_pmeter.c - S2MF301 Power Meter Driver
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
#include <linux/mfd/samsung/s2mf301.h>
#include <linux/power/s2mf301_pmeter.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

static enum power_supply_property s2mf301_pmeter_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int s2mf301_pm_enable(struct s2mf301_pmeter_data *pmeter, int mode, enum pm_type type)
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
    w_data = r_data | (1 << shift);
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
    case S2MF301_PM_TYPE_GPADC1:
    case S2MF301_PM_TYPE_GPADC2:
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

	charge_voltage = (((data1<<8)|data2) * 1000) >> 11;

	pr_info("%s, %x, %x -> %x\n", __func__, data1, data2, charge_voltage);

	return charge_voltage;
}

static int s2mf301_pm_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mf301_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_VCHGIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCHGIN);
			break;
		case POWER_SUPPLY_S2M_PROP_VWCIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VWCIN);
			break;
		case POWER_SUPPLY_S2M_PROP_VBYP:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VBYP);
			break;
		case POWER_SUPPLY_S2M_PROP_VSYS:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VSYS);
			break;
		case POWER_SUPPLY_S2M_PROP_VBAT:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VBAT);
			break;
		case POWER_SUPPLY_S2M_PROP_VGPADC:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_GPADC1);
			break;
		case POWER_SUPPLY_S2M_PROP_VCC1:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCC1);
			break;
		case POWER_SUPPLY_S2M_PROP_VCC2:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCC2);
			break;
		case POWER_SUPPLY_S2M_PROP_ICHGIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_ICHGIN);
			break;
		case POWER_SUPPLY_S2M_PROP_IWCIN:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_IWCIN);
			break;
		case POWER_SUPPLY_S2M_PROP_IOTG:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_IOTG);
			break;
		case POWER_SUPPLY_S2M_PROP_ITX:
			val->intval = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_ITX);
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
	enum s2m_power_supply_property s2m_psp = (enum s2m_power_supply_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("[DEBUG]%s: POWER_SUPPLY_PROP_ONLINE\n", __func__);
		return 1;
	case POWER_SUPPLY_S2M_PROP_MIN ... POWER_SUPPLY_S2M_PROP_MAX:
		switch (s2m_psp) {
		case POWER_SUPPLY_S2M_PROP_CO_ENABLE:
			s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, val->intval);
			break;
		case POWER_SUPPLY_S2M_PROP_RR_ENABLE:
			s2mf301_pm_enable(pmeter, REQUEST_RESPONSE_MODE, val->intval);
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

static irqreturn_t s2mf301_vchgin_isr(int irq, void *data)
{
	struct s2mf301_pmeter_data *pmeter = data;
	struct power_supply *psy;
	union power_supply_propval value;
	int voltage, ret = 0;


	voltage = s2mf301_pm_get_value(pmeter, S2MF301_PM_TYPE_VCHGIN);

	pr_info("%s voltage : %d", __func__, voltage);

	psy = power_supply_get_by_name("s2mf301-charger");

	if (!psy) {
		pr_info("%s, Fail to get psy-charger\n", __func__);
		return IRQ_HANDLED;
	} else {
		if (voltage >= 6500) {
			value.intval = 1;
			ret = power_supply_set_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHG_EFFICIENCY, &value);
		} else {
			value.intval = 0;
			ret = power_supply_set_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_CHG_EFFICIENCY, &value);
		}
	}

	return IRQ_HANDLED;
}

static const struct of_device_id s2mf301_pmeter_match_table[] = {
	{ .compatible = "samsung,s2mf301-pmeter",},
	{},
};

static void s2mf301_powermeter_initial(struct s2mf301_pmeter_data *pmeter)
{
	u8 data;

	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, S2MF301_PM_TYPE_ICHGIN);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, S2MF301_PM_TYPE_VCHGIN);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, S2MF301_PM_TYPE_VCC1);
	s2mf301_pm_enable(pmeter, CONTINUOUS_MODE, S2MF301_PM_TYPE_VCC2);

	/* 2048 code(1.0V) */
	s2mf301_read_reg(pmeter->i2c, S2MF301_REG_PM_HYST_LEV, &data);
	data &= ~0x70;
	data |= 0x60;
	s2mf301_write_reg(pmeter->i2c, S2MF301_REG_PM_HYST_LEV, data);
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
    /* in mf301, PM uses 0x7E slave address */
	pmeter->i2c = s2mf301->pm;

	platform_set_drvdata(pdev, pmeter);

	pmeter->psy_pm_desc.name           = "s2mf301-pmeter"; //pmeter->pdata->powermeter_name;
	pmeter->psy_pm_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	pmeter->psy_pm_desc.get_property   = s2mf301_pm_get_property;
	pmeter->psy_pm_desc.set_property   = s2mf301_pm_set_property;
	pmeter->psy_pm_desc.properties     = s2mf301_pmeter_props;
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

static void s2mf301_pmeter_shutdown(struct device *dev)
{
	pr_info("%s: S2MF301 PowerMeter driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mf301_pmeter_pm_ops, s2mf301_pmeter_suspend,
		s2mf301_pmeter_resume);

static struct platform_driver s2mf301_pmeter_driver = {
	.driver         = {
		.name   = "s2mf301-powermeter",
		.owner  = THIS_MODULE,
		.of_match_table = s2mf301_pmeter_match_table,
		.pm     = &s2mf301_pmeter_pm_ops,
		.shutdown   =   s2mf301_pmeter_shutdown,
	},
	.probe          = s2mf301_pmeter_probe,
	.remove		= s2mf301_pmeter_remove,
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
MODULE_SOFTDEP("post: s2m_muic_module");
