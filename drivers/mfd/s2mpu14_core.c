/*
 * s2mpu14.c - mfd core driver for the s2mpu14
 *
 * Copyright (C) 2021 Samsung Electronics
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mpu14.h>
#include <linux/mfd/samsung/s2mpu14-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SUB_CHANNEL	1
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_TOP	0x00
#define I2C_ADDR_PMIC	0x01
#define I2C_ADDR_DEBUG	0x0F
#define I2C_ADDR_ADC	0x0A
#define I2C_ADDR_GPIO	0x0B

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static struct device_node *acpm_mfd_node;
#endif
static struct mfd_cell s2mpu14_devs[] = {
	{ .name = "s2mpu14-regulator", },
	{ .name = "s2mpu14-gpio", },
};

int s2mpu14_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu14->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, dest);
	mutex_unlock(&s2mpu14->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu14_read_reg);

int s2mpu14_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu14->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpu14->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu14_bulk_read);

int s2mpu14_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mpu14->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, value);
	mutex_unlock(&s2mpu14->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu14_write_reg);

int s2mpu14_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mpu14->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpu14->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu14_bulk_write);

int s2mpu14_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mpu14->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mpu14->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu14_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpu14_dt(struct device *dev,
			 struct s2mpu14_platform_data *pdata,
			 struct s2mpu14_dev *s2mpu14)
{
	struct device_node *np = dev->of_node;
	int strlen;
	const char *status;

	if (!np)
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif
	status = of_get_property(np, "s2mpu14,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	return 0;
}
#else
static int of_s2mpu14_dt(struct device *dev,
			 struct s2mpu14_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpu14_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *dev_id)
{
	struct s2mpu14_dev *s2mpu14;
	struct s2mpu14_platform_data *pdata = i2c->dev.platform_data;
	u8 reg_data;
	int ret = 0;

	pr_info("[PMIC] %s: start\n", __func__);

	s2mpu14 = devm_kzalloc(&i2c->dev, sizeof(struct s2mpu14_dev), GFP_KERNEL);
	if (!s2mpu14)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
				     sizeof(struct s2mpu14_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mpu14_dt(&i2c->dev, pdata, s2mpu14);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	s2mpu14->dev = &i2c->dev;
	i2c->addr = I2C_ADDR_TOP;	/* forced COMMON address For GKI-R */
	s2mpu14->i2c = i2c;
	s2mpu14->device_type = S2MPU14X;

	if (pdata) {
		s2mpu14->pdata = pdata;
		s2mpu14->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mpu14->i2c_lock);

	i2c_set_clientdata(i2c, s2mpu14);

	ret = s2mpu14_read_reg(i2c, S2MPU14_PMIC_REG_PMIC_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mpu14->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		s2mpu14->pmic_rev = reg_data;
	}

	s2mpu14->pmic = devm_i2c_new_dummy_device(s2mpu14->dev, i2c->adapter, I2C_ADDR_PMIC);
	s2mpu14->debug_i2c = devm_i2c_new_dummy_device(s2mpu14->dev, i2c->adapter, I2C_ADDR_DEBUG);
	s2mpu14->adc_i2c = devm_i2c_new_dummy_device(s2mpu14->dev, i2c->adapter, I2C_ADDR_ADC);
	s2mpu14->gpio_i2c = devm_i2c_new_dummy_device(s2mpu14->dev, i2c->adapter, I2C_ADDR_GPIO);

	i2c_set_clientdata(s2mpu14->pmic, s2mpu14);
	i2c_set_clientdata(s2mpu14->debug_i2c, s2mpu14);
	i2c_set_clientdata(s2mpu14->adc_i2c, s2mpu14);
	i2c_set_clientdata(s2mpu14->gpio_i2c, s2mpu14);

	pr_info("%s: device found: rev.0x%02hhx\n", __func__, s2mpu14->pmic_rev);

	/* init sub PMIC notifier */
	ret = s2mpu14_notifier_init(s2mpu14);
	if (ret < 0)
		pr_err("%s: s2mpu14_notifier_init fail\n", __func__);

	ret = devm_mfd_add_devices(s2mpu14->dev, -1, s2mpu14_devs,
			      ARRAY_SIZE(s2mpu14_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_w_lock;

	ret = device_init_wakeup(s2mpu14->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	pr_info("[PMIC] %s: end\n", __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mpu14->i2c_lock);
err:
	return ret;
}

static int s2mpu14_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpu14_dev *s2mpu14 = i2c_get_clientdata(i2c);

	if (s2mpu14->pdata->wakeup)
		device_init_wakeup(s2mpu14->dev, false);

	mutex_destroy(&s2mpu14->i2c_lock);

	return 0;
}

static const struct i2c_device_id s2mpu14_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPU14 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpu14_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id s2mpu14_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpu14mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static int s2mpu14_suspend(struct device *dev)
{
	return 0;
}

static int s2mpu14_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mpu14_suspend	NULL
#define s2mpu14_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s2mpu14_pm = {
	.suspend_late = s2mpu14_suspend,
	.resume_early = s2mpu14_resume,
};

static struct i2c_driver s2mpu14_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mpu14_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mpu14_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mpu14_i2c_probe,
	.remove		= s2mpu14_i2c_remove,
	.id_table	= s2mpu14_i2c_id,
};

static int __init s2mpu14_i2c_init(void)
{
	pr_info("[PMIC] %s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpu14_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpu14_i2c_init);

static void __exit s2mpu14_i2c_exit(void)
{
	i2c_del_driver(&s2mpu14_i2c_driver);
}
module_exit(s2mpu14_i2c_exit);

MODULE_DESCRIPTION("s2mpu14 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
