/*
 * s2mps25.c - mfd core driver for the s2mps25
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
#include <linux/mfd/samsung/s2mps25.h>
#include <linux/mfd/samsung/s2mps25-regulator.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define MAIN_CHANNEL	0
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_VGPIO		0x00
#define I2C_ADDR_COM		0x03
#define I2C_ADDR_RTC		0x04
#define I2C_ADDR_PMIC1		0x05
#define I2C_ADDR_PMIC2		0x06
#define I2C_ADDR_CLOSE1		0x0E
#define I2C_ADDR_CLOSE2		0x0F
#define I2C_ADDR_ADC		0x0A
#define I2C_ADDR_TEMP		0x0C

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static struct device_node *acpm_mfd_node;
#endif
static struct mfd_cell s2mps25_devs[] = {
	{ .name = "s2mps25-regulator", },
	{ .name = "s2mps25-rtc", },
#if IS_ENABLED(CONFIG_KEYBOARD_S2MPS25)
	{ .name = "s2mps25-power-keys", },
#endif
	{ .name = "s2mps25-gpadc", },
};

int s2mps25_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps25->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_CHANNEL, i2c->addr, reg, dest);
	mutex_unlock(&s2mps25->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps25_read_reg);

int s2mps25_bulk_read(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps25->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, MAIN_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps25->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps25_bulk_read);

int s2mps25_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps25->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MAIN_CHANNEL, i2c->addr, reg, value);
	mutex_unlock(&s2mps25->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps25_write_reg);

int s2mps25_bulk_write(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps25->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, MAIN_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps25->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps25_bulk_write);

int s2mps25_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps25->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, MAIN_CHANNEL, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mps25->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps25_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps25_dt(struct device *dev,
			 struct s2mps25_platform_data *pdata,
			 struct s2mps25_dev *s2mps25)
{
	struct device_node *np = dev->of_node;
	int ret, strlen;
	const char *status;
	uint32_t val;

	if (!np)
		return -EINVAL;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif

	status = of_get_property(np, "s2mps25,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	/* WTSR, SMPL */
	pdata->wtsr_smpl = devm_kzalloc(dev, sizeof(*pdata->wtsr_smpl),
			GFP_KERNEL);
	if (!pdata->wtsr_smpl)
		return -ENOMEM;

	status = of_get_property(np, "wtsr_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->wtsr_en = true;
		else
			pdata->wtsr_smpl->wtsr_en = false;
	}

	status = of_get_property(np, "smpl_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->smpl_en = true;
		else
			pdata->wtsr_smpl->smpl_en = false;
	}

	ret = of_property_read_u32(np, "wtsr_timer_val",
			&pdata->wtsr_smpl->wtsr_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "smpl_timer_val",
			&pdata->wtsr_smpl->smpl_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "check_jigon", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_smpl->check_jigon = !!val;

	/* init time */
	pdata->init_time = devm_kzalloc(dev, sizeof(*pdata->init_time), GFP_KERNEL);
	if (!pdata->init_time)
		return -ENOMEM;

	ret = of_property_read_u32(np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return -EINVAL;

	/* rtc optimize */
	ret = of_property_read_u32(np, "osc-bias-up", &val);
	if (!ret)
		pdata->osc_bias_up = val;
	else
		pdata->osc_bias_up = -1;

	ret = of_property_read_u32(np, "rtc_cap_sel", &val);
	if (!ret)
		pdata->cap_sel = val;
	else
		pdata->cap_sel = -1;

	ret = of_property_read_u32(np, "rtc_osc_xin", &val);
	if (!ret)
		pdata->osc_xin = val;
	else
		pdata->osc_xin = -1;

	ret = of_property_read_u32(np, "rtc_osc_xout", &val);
	if (!ret)
		pdata->osc_xout = val;
	else
		pdata->osc_xout = -1;

	return 0;
}
#else
static int of_s2mps25_dt(struct device *dev,
			 struct s2mps25_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps25_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct s2mps25_dev *s2mps25;
	struct s2mps25_platform_data *pdata = i2c->dev.platform_data;
	uint8_t reg_data;
	int ret = 0;

	pr_info("[PMIC] %s: start\n", __func__);

	s2mps25 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps25_dev), GFP_KERNEL);
	if (!s2mps25)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
				     sizeof(struct s2mps25_platform_data),
				     GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = of_s2mps25_dt(&i2c->dev, pdata, s2mps25);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;


	s2mps25->dev = &i2c->dev;
	i2c->addr = I2C_ADDR_COM;	/* forced COMMON address */
	s2mps25->i2c = i2c;
	s2mps25->device_type = S2MPS25X;
	s2mps25->irq = i2c->irq;

	if (pdata) {
		s2mps25->pdata = pdata;
		s2mps25->wakeup = pdata->wakeup;

		pdata->irq_base = devm_irq_alloc_descs(s2mps25->dev, -1, 0, S2MPS25_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s: %s devm_irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			s2mps25->irq_base = pdata->irq_base;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mps25->i2c_lock);

	i2c_set_clientdata(i2c, s2mps25);

	ret = s2mps25_read_reg(i2c, S2MPS25_REG_CHIP_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mps25->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else
		s2mps25->pmic_rev = reg_data; /* print rev */

	s2mps25->vgpio = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_TEMP);
	s2mps25->vgpio->addr = I2C_ADDR_VGPIO;
	s2mps25->pmic1 = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_PMIC1);
	s2mps25->pmic2 = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_PMIC2);
	s2mps25->rtc = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_RTC);
	s2mps25->close1 = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_CLOSE1);
	s2mps25->close2 = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_CLOSE2);
	s2mps25->adc_i2c = devm_i2c_new_dummy_device(s2mps25->dev, i2c->adapter, I2C_ADDR_ADC);

	i2c_set_clientdata(s2mps25->vgpio, s2mps25);
	i2c_set_clientdata(s2mps25->pmic1, s2mps25);
	i2c_set_clientdata(s2mps25->pmic2, s2mps25);
	i2c_set_clientdata(s2mps25->rtc, s2mps25);
	i2c_set_clientdata(s2mps25->close1, s2mps25);
	i2c_set_clientdata(s2mps25->close2, s2mps25);
	i2c_set_clientdata(s2mps25->adc_i2c, s2mps25);

	pr_info("%s: device found: rev.0x%02hhx\n", __func__, s2mps25->pmic_rev);

	ret = s2mps25_irq_init(s2mps25);
	if (ret < 0)
		goto err_w_lock;

	ret = devm_mfd_add_devices(s2mps25->dev, -1, s2mps25_devs,
			      ARRAY_SIZE(s2mps25_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_w_lock;

	ret = device_init_wakeup(s2mps25->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	pr_info("[PMIC] %s: end\n", __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mps25->i2c_lock);
err:
	return ret;
}

static int s2mps25_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

	if (s2mps25->pdata->wakeup)
		device_init_wakeup(s2mps25->dev, false);

	s2mps25_irq_exit(s2mps25);
	mutex_destroy(&s2mps25->i2c_lock);

	return 0;
}

static const struct i2c_device_id s2mps25_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPS25 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps25_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps25_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps25mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static int s2mps25_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

	dev->power.must_resume = true;

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mps25->irq);

	disable_irq(s2mps25->irq);

	return 0;
}

static int s2mps25_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct s2mps25_dev *s2mps25 = i2c_get_clientdata(i2c);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s: %s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mps25->irq);

	enable_irq(s2mps25->irq);

	return 0;
}
#else
#define s2mps25_suspend	NULL
#define s2mps25_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s2mps25_pm = {
	.suspend_late = s2mps25_suspend,
	.resume_early = s2mps25_resume,
};

static struct i2c_driver s2mps25_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mps25_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps25_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps25_i2c_probe,
	.remove		= s2mps25_i2c_remove,
	.id_table	= s2mps25_i2c_id,
};

static int __init s2mps25_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps25_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps25_i2c_init);

static void __exit s2mps25_i2c_exit(void)
{
	i2c_del_driver(&s2mps25_i2c_driver);
}
module_exit(s2mps25_i2c_exit);

MODULE_DESCRIPTION("s2mps25 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
