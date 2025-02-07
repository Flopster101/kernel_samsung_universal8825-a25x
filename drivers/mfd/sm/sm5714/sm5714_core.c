/*
 *  sm5714_core.c - mfd core driver for the SM5714.
 *
 *  Copyright (C) 2019 Samsung Electronics Co.Ltd
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sm/sm5714/sm5714_log.h>
#include <linux/mfd/sm/sm5714/sm5714.h>
#include <linux/mfd/sm/sm5714/sm5714-private.h>
#include <linux/regulator/machine.h>
#include <linux/usb_notify.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#endif /* CONFIG_OF */

static struct mfd_cell sm5714_devs[] = {
#if IS_ENABLED(CONFIG_MUIC_SM5714)
	{ .name = "sm5714-muic", },
#endif  /* CONFIG_MUIC_SM5714 */
#if IS_ENABLED(CONFIG_FUELGAUGE_SM5714)
	{ .name = "sm5714-fuelgauge", },
#endif  /* CONFIG_FUELGAUGE_SM5714 */
#if IS_ENABLED(CONFIG_CHARGER_SM5714)
	{ .name = "sm5714-charger", },
#endif  /* CONFIG_CHARGER_SM5714 */
#if IS_ENABLED(CONFIG_LEDS_SM5714)
	{ .name = "sm5714-fled", },
#endif /* CONFIG_LEDS_SM5714 */
};

int sm5714_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}
	*dest = (ret & 0xff);

    return 0;
}
EXPORT_SYMBOL_GPL(sm5714_read_reg);

int sm5714_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_bulk_read);

int sm5714_read_word(struct i2c_client *i2c, u8 reg)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_word_data(i2c, reg);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sm5714_read_word);

int sm5714_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sm5714_write_reg);

int sm5714_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_bulk_write);

int sm5714_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_write_word_data(i2c, reg, value);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	mutex_unlock(&sm5714->i2c_lock);
	if (ret < 0) {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sm5714_write_word);

int sm5714_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);
	int ret, i;
	int retry = 3;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	mutex_lock(&sm5714->i2c_lock);
	for (i = 0; i < retry; i++) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
	}
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		for (i = 0; i < retry; i++) {
			ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
			if (ret >= 0)
				break;
			sm5714_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		}
	} else {
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			inc_hw_param(o_notify, USB_CCIC_I2C_ERROR_COUNT);
#endif
	}
	mutex_unlock(&sm5714->i2c_lock);

    return ret;
}
EXPORT_SYMBOL_GPL(sm5714_update_reg);

#if defined(CONFIG_OF)
static int of_sm5714_dt(struct device *dev, struct sm5714_platform_data *pdata)
{
	struct device_node *np_sm5714 = dev->of_node;

	if (!np_sm5714)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_sm5714, "sm5714,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_sm5714, "sm5714,wakeup");

	sm5714_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_sm5714_dt(struct device *dev, struct sm5714_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int sm5714_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct sm5714_dev *sm5714;
	struct sm5714_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;
	u8 temp = 0;

	dev_info(&i2c->dev, "%s start\n", __func__);

	sm5714 = kzalloc(sizeof(struct sm5714_dev), GFP_KERNEL);
	if (!sm5714) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for sm5714\n",
								__func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5714_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err;

		}

		ret = of_sm5714_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else {
		pdata = i2c->dev.platform_data;
	}

	sm5714->dev = &i2c->dev;
	sm5714->charger = i2c;
	sm5714->irq = i2c->irq;
	sm5714->suspended = false;
	init_waitqueue_head(&sm5714->suspend_wait);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 188)
	wakeup_source_init(sm5714->irq_ws, "sm5714_mfd");   // 4.19 R
	if (!(sm5714->irq_ws)) {
		sm5714->irq_ws = wakeup_source_create("sm5714_mfd"); // 4.19 Q
		if (sm5714->irq_ws)
			wakeup_source_add(sm5714->irq_ws);
	}
#else
	sm5714->irq_ws = wakeup_source_register(NULL, "sm5714_mfd"); // 5.4 R
#endif

	if (pdata) {
		sm5714->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, SM5714_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			sm5714_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else {
			sm5714->irq_base = pdata->irq_base;
		}

		sm5714->irq_gpio = pdata->irq_gpio;
		sm5714->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&sm5714->i2c_lock);

	i2c_set_clientdata(i2c, sm5714);

    /* Get Device ID & Check Charger I2C transmission */
    ret = sm5714_read_reg(i2c, SM5714_CHG_REG_DEVICEID, &temp);
	if (ret < 0 || (temp & 0x7) != 0x1) {
		sm5714_err("%s:%s device not found on this channel (reg_data=0x%x)\n", MFD_DEV_NAME, __func__, temp);
		ret = -ENODEV;
		goto err_w_lock;
	}
    sm5714->vender_id = (temp & 0x7);
    sm5714->pmic_rev = ((temp >> 3) & 0x1F);

    sm5714_info("%s:%s v_id=0x%x, rev=0x%x\n",
		MFD_DEV_NAME, __func__, sm5714->vender_id, sm5714->pmic_rev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	sm5714->fuelgauge = i2c_new_dummy(i2c->adapter, SM5714_I2C_SADR_FG);
#else
	sm5714->fuelgauge = i2c_new_dummy_device(i2c->adapter, SM5714_I2C_SADR_FG);
#endif
	i2c_set_clientdata(sm5714->fuelgauge, sm5714);
    /* Check FG I2C transmission */
    ret = sm5714_read_word(sm5714->fuelgauge, SM5714_FG_REG_DEVICE_ID);
    if ((unsigned int)ret > 0xFF) {
		sm5714_err("%s:%s fail to setup FG I2C transmission (ret=0x%x)\n", MFD_DEV_NAME, __func__, ret);
		ret = -ENODEV;
		goto err_w_lock;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	sm5714->muic = i2c_new_dummy(i2c->adapter, SM5714_I2C_SADR_MUIC);
#else
	sm5714->muic = i2c_new_dummy_device(i2c->adapter, SM5714_I2C_SADR_MUIC);
#endif
	i2c_set_clientdata(sm5714->muic, sm5714);
    /* Check MUIC I2C transmission */
    ret = sm5714_read_reg(sm5714->muic, SM5714_MUIC_REG_DeviceID, &temp);
	if (ret < 0 || temp != 0x1) {
		sm5714_err("%s:%s fail to setup MUIC I2C transmission (reg_data=0x%x)\n", MFD_DEV_NAME, __func__, temp);
		ret = -ENODEV;
		goto err_w_lock;
	}

	ret = sm5714_irq_init(sm5714);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(sm5714->dev, -1, sm5714_devs, ARRAY_SIZE(sm5714_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(sm5714->dev, pdata->wakeup);

    sm5714_info("%s: %s done\n", MFD_DEV_NAME, __func__);

	return ret;

err_mfd:
	mfd_remove_devices(sm5714->dev);
    sm5714_irq_exit(sm5714);
err_irq_init:
	i2c_unregister_device(sm5714->fuelgauge);
    i2c_unregister_device(sm5714->muic);
err_w_lock:
	mutex_destroy(&sm5714->i2c_lock);
err:
	kfree(sm5714);

    return ret;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void sm5714_i2c_remove(struct i2c_client *i2c)
#else
static int sm5714_i2c_remove(struct i2c_client *i2c)
#endif
{
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);

	mfd_remove_devices(sm5714->dev);
    sm5714_irq_exit(sm5714);

	i2c_unregister_device(sm5714->muic);
    i2c_unregister_device(sm5714->fuelgauge);

    mutex_destroy(&sm5714->i2c_lock);

    kfree(sm5714);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
    return 0;
#endif
}

static const struct i2c_device_id sm5714_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_SM5714 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5714_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sm5714_i2c_dt_ids[] = {
	{ .compatible = "siliconmitus,sm5714mfd" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5714_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5714_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);

	sm5714->suspended =  true;

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5714->irq);

#if !defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_MEDIATEK)
	disable_irq(sm5714->irq);
#endif

	return 0;
}

static int sm5714_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5714_dev *sm5714 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	sm5714_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	sm5714->suspended =  false;
#if defined(CONFIG_ARCH_QCOM) || defined(CONFIG_ARCH_MEDIATEK)
	wake_up(&sm5714->suspend_wait);
#endif

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5714->irq);

#if !defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_MEDIATEK)
	enable_irq(sm5714->irq);
#endif

	return 0;
}
#else
#define sm5714_suspend	    NULL
#define sm5714_resume		NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops sm5714_pm = {
	.suspend    = sm5714_suspend,
	.resume     = sm5714_resume,
};

static struct i2c_driver sm5714_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	    = &sm5714_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= sm5714_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= sm5714_i2c_probe,
	.remove		= sm5714_i2c_remove,
	.id_table	= sm5714_i2c_id,
};

static int __init sm5714_i2c_init(void)
{
	sm5714_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&sm5714_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(sm5714_i2c_init);

static void __exit sm5714_i2c_exit(void)
{
	i2c_del_driver(&sm5714_i2c_driver);
}
module_exit(sm5714_i2c_exit);

MODULE_DESCRIPTION("SM5714 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
