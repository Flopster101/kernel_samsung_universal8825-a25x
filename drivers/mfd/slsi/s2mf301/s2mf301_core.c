/*
 * s2mf301.c - mfd core driver for the s2mf301
 *
 * Copyright (C) 2019 Samsung Electronics
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

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/slsi/s2mf301/s2mf301.h>
#include <linux/mfd/slsi/s2mf301/s2mf301_log.h>
#include <linux/of_gpio.h>

#define I2C_RETRY_CNT	3

static struct mfd_cell s2mf301_devs[] = {
#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	{ .name = "s2mf301-charger", },
#endif
#if IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
	{ .name = "s2mf301-fuelgauge", },
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	{ .name = "leds-s2mf301", },
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MF301)
	{ .name = "s2mf301-muic", },
#endif
#if IS_ENABLED(CONFIG_AFC_S2MF301)
	{ .name = "s2mf301-afc", },
#endif
#if IS_ENABLED(CONFIG_PM_S2MF301)
	{ .name = "s2mf301-powermeter", },
#endif
#if IS_ENABLED(CONFIG_TOP_S2MF301)
	{ .name = "s2mf301-top", },
#endif
#if IS_ENABLED(CONFIG_REGULATOR_S2MF301)
	{ .name = "s2mf301-regulator", },
#endif
};

int s2mf301_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0) {
		s2mf301_err("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME_,
				__func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_read_reg);

int s2mf301_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_bulk_read);

int s2mf301_read_word(struct i2c_client *i2c, u8 reg)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_word_data(i2c, reg);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(s2mf301_read_word);

int s2mf301_atomic_write(struct i2c_client *i2c, u8 reg[], u8 value[], int count)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret;
	int i;

	mutex_lock(&s2mf301->i2c_lock);
	for(i = 0; i < count; i++) {
		ret = i2c_smbus_write_byte_data(i2c, reg[i], value[i]);
		if (ret < 0) {
			pr_err("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME_, __func__, reg[i], ret);
			break;
		}
	}
	mutex_unlock(&s2mf301->i2c_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mf301_atomic_write);

int s2mf301_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0)
		s2mf301_err("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mf301_write_reg);

int s2mf301_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_bulk_write);

int s2mf301_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_word_data(i2c, reg, value);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&s2mf301->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mf301_write_word);

int s2mf301_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);
	int ret, i;
	u8 old_val, new_val;

	mutex_lock(&s2mf301->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
				MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		for (i = 0; i < I2C_RETRY_CNT; ++i) {
			ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
			if (ret >= 0)
				break;
			s2mf301_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
					MFD_DEV_NAME_, __func__, reg, ret, i + 1, I2C_RETRY_CNT);
		}
	}
	mutex_unlock(&s2mf301->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mf301_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mf301_dt(struct device *dev,
				struct s2mf301_platform_data *pdata)
{
	struct device_node *np_s2mf301 = dev->of_node;
	int ret = 0;

	if (!np_s2mf301)
		return -EINVAL;

	ret = of_get_named_gpio(np_s2mf301, "s2mf301,irq-gpio", 0);
	if (!gpio_is_valid(ret))
		return ret;

	pdata->irq_gpio = ret;
	pdata->wakeup = of_property_read_bool(np_s2mf301, "s2mf301,wakeup");

	pr_debug("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_s2mf301_dt(struct device *dev,
				struct s2mf301_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mf301_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *dev_id)
{
	struct s2mf301_dev *s2mf301;
	struct s2mf301_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;
	u8 temp = 0;

	s2mf301_info("%s: start\n", __func__);

	s2mf301 = devm_kzalloc(&i2c->dev, sizeof(struct s2mf301_dev), GFP_KERNEL);
	if (!s2mf301) {
		ret = -ENOMEM;
		goto err;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
			 sizeof(struct s2mf301_platform_data), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mf301_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	s2mf301->dev = &i2c->dev;
	s2mf301->i2c = i2c;
	s2mf301->irq = i2c->irq;
	if (pdata) {
		s2mf301->pdata = pdata;
#if IS_ENABLED(CONFIG_CHARGER_S2MF301) || IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH) || \
	IS_ENABLED(CONFIG_HV_MUIC_S2MF301_AFC) || IS_ENABLED(CONFIG_MUIC_S2MF301) || \
	IS_ENABLED(CONFIG_PM_S2MF301) || IS_ENABLED(CONFIG_REGULATOR_S2MF301) || \
	IS_ENABLED(CONFIG_FUELGAUGE_S2MF301)
		pdata->irq_base = irq_alloc_descs(-1, 0, S2MF301_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			s2mf301_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME_, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			s2mf301->irq_base = pdata->irq_base;
#endif
		s2mf301->irq_gpio = pdata->irq_gpio;
		s2mf301->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mf301->i2c_lock);

	i2c_set_clientdata(i2c, s2mf301);

	s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_PMICID, &temp);

	s2mf301->pmic_ver = temp;
	s2mf301_err("%s : pmic ver = 0x%x\n", __func__, s2mf301->pmic_ver);

	s2mf301_read_reg(s2mf301->i2c, S2MF301_REG_ES_NO_REV_NO, &temp);
	if ((temp & 0x0F) == 0x04)
		s2mf301->evt0 = true;
	else
		s2mf301->evt0 = false;

	/* I2C enable for MUIC, AFC */
	s2mf301->muic = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_7C_SLAVE);
	i2c_set_clientdata(s2mf301->muic, s2mf301);

#if IS_ENABLED(CONFIG_CHARGER_S2MF301)
	/* I2C enable for Power meter */
	s2mf301->pm = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_7E_SLAVE);
	i2c_set_clientdata(s2mf301->pm, s2mf301);

	/* I2C enable for Charger */
	s2mf301->chg = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_7A_SLAVE);
	i2c_set_clientdata(s2mf301->chg, s2mf301);

	/* I2C enable for Fuel Gauge */
	s2mf301->fg = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_76_SLAVE);
	i2c_set_clientdata(s2mf301->fg, s2mf301);
#endif
	ret = s2mf301_irq_init(s2mf301);

	if (ret < 0) {
		s2mf301_err("%s : irq init fail(%d)\n", __func__, ret);
		goto err_irq_init;
	}

	ret = mfd_add_devices(s2mf301->dev, -1, s2mf301_devs,
			ARRAY_SIZE(s2mf301_devs), NULL, 0, NULL);
	if (ret < 0) {
		s2mf301_err("%s : mfd add devices fail(%d)\n", __func__, ret);
		goto err_mfd;
	}

	device_init_wakeup(s2mf301->dev, pdata->wakeup);

	s2mf301_info("%s: end\n", __func__);

	return ret;

err_mfd:
	mutex_destroy(&s2mf301->i2c_lock);
	mfd_remove_devices(s2mf301->dev);
err_irq_init:
	i2c_unregister_device(s2mf301->i2c);
err:
	kfree(s2mf301);
	i2c_set_clientdata(i2c, NULL);

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void s2mf301_i2c_remove(struct i2c_client *i2c)
#else
static int s2mf301_i2c_remove(struct i2c_client *i2c)
#endif
{
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);

	mfd_remove_devices(s2mf301->dev);
	i2c_unregister_device(s2mf301->i2c);
	i2c_set_clientdata(i2c, NULL);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

static const struct i2c_device_id s2mf301_i2c_id[] = {
	{ MFD_DEV_NAME_, TYPE_S2MF301 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mf301_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id s2mf301_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mf301mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static void s2mf301_set_irq_mask(struct s2mf301_dev *s2mf301, int enable)
{
	s2mf301_info("%s: s2mf301 interrupt masking %s\n", __func__, enable > 0 ? "enable" : "disable");

	if (enable)
		s2mf301_write_reg(s2mf301->i2c, 0x09, 0xE3);
	else
		s2mf301_write_reg(s2mf301->i2c, 0x09, 0xE1);
}

static int s2mf301_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);

	s2mf301_set_irq_mask(s2mf301, 1);

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mf301->irq);

	disable_irq(s2mf301->irq);

	return 0;
}

static int s2mf301_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mf301_dev *s2mf301 = i2c_get_clientdata(i2c);

	pr_debug("%s:%s\n", MFD_DEV_NAME_, __func__);

	s2mf301_set_irq_mask(s2mf301, 0);

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mf301->irq);

	enable_irq(s2mf301->irq);

	return 0;
}
#else
#define s2mf301_suspend	NULL
#define s2mf301_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mf301_pm = {
	.suspend = s2mf301_suspend,
	.resume = s2mf301_resume,
};

static struct i2c_driver s2mf301_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME_,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mf301_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mf301_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mf301_i2c_probe,
	.remove		= s2mf301_i2c_remove,
	.id_table	= s2mf301_i2c_id,
};

static int __init s2mf301_i2c_init(void)
{
	s2mf301_info("%s:%s\n", MFD_DEV_NAME_, __func__);
	return i2c_add_driver(&s2mf301_i2c_driver);
}
subsys_initcall(s2mf301_i2c_init);

static void __exit s2mf301_i2c_exit(void)
{
	i2c_del_driver(&s2mf301_i2c_driver);
}
module_exit(s2mf301_i2c_exit);

MODULE_DESCRIPTION("s2mf301 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
