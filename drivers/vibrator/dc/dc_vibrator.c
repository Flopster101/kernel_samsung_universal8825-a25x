// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define pr_fmt(fmt) "[VIB] dc_vib: " fmt
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include "dc_vibrator.h"
#if defined(CONFIG_SEC_KUNIT)
#include "kunit_test/dc_vibrator_test.h"
#else
#define __visible_for_testing static
#endif

__visible_for_testing int dc_vib_on(struct dc_vib_drvdata *ddata)
{
	int ret = 0;
	int is_enabled = 0;

	if (!ddata)
		return -EINVAL;
	if (ddata->running)
		return 0;
	if (ddata->pdata->regulator) {
		is_enabled = regulator_is_enabled(ddata->pdata->regulator);
		pr_info("%s: regulator on, is_enabled(%d)\n",
				__func__, is_enabled);
		if (!is_enabled)
			ret = regulator_enable(ddata->pdata->regulator);
		if (ret)
			pr_err("%s: regulator_enable err %d\n", __func__, ret);
	}
	if (gpio_is_valid(ddata->pdata->gpio_en)) {
		pr_info("%s: gpio on\n", __func__);
		gpio_direction_output(ddata->pdata->gpio_en, 1);
	}
	ddata->running = true;
	return 0;
}

__visible_for_testing int dc_vib_off(struct dc_vib_drvdata *ddata)
{
	int ret = 0;
	int is_enabled = 0;

	if (!ddata)
		return -EINVAL;
	if (!ddata->running)
		return 0;
	if (gpio_is_valid(ddata->pdata->gpio_en)) {
		pr_info("%s: gpio off\n", __func__);
		gpio_direction_output(ddata->pdata->gpio_en, 0);
	}
	if (ddata->pdata->regulator) {
		is_enabled = regulator_is_enabled(ddata->pdata->regulator);
		pr_info("%s: regulator off, is_enabled(%d)\n",
				__func__, is_enabled);
		if (is_enabled)
			ret = regulator_disable(ddata->pdata->regulator);
		if (ret)
			pr_err("%s: regulator_disable err %d\n", __func__, ret);
	}
	ddata->running = false;
	return 0;
}

__visible_for_testing int dc_vib_enable(struct device *dev, bool en)
{
	struct dc_vib_drvdata *ddata;

	if (!dev)
		return -EINVAL;

	ddata = dev_get_drvdata(dev);

	return en ? dc_vib_on(ddata) : dc_vib_off(ddata);
}

__visible_for_testing int dc_vib_get_motor_type(struct device *dev, char *buf)
{
	struct dc_vib_drvdata *ddata;

	if (!dev)
		return -EINVAL;

	ddata = dev_get_drvdata(dev);

	return snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);
}

static const struct sec_vibrator_ops dc_vib_ops = {
	.enable	= dc_vib_enable,
	.get_motor_type = dc_vib_get_motor_type,
};

#if defined(CONFIG_OF)
static struct dc_vib_pdata *dc_vib_get_dt(struct device *dev)
{
	struct device_node *node;
	struct dc_vib_pdata *pdata;
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->gpio_en = of_get_named_gpio(node, "dc_vib,gpio_en", 0);
	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "mot_ldo_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		gpio_direction_output(pdata->gpio_en, 0);
	} else {
		pr_info("%s: gpio isn't used\n", __func__);
	}

	ret = of_property_read_string(node, "dc_vib,regulator_name",
			&pdata->regulator_name);
	if (!ret) {
		pdata->regulator = regulator_get(dev, pdata->regulator_name);
		if (IS_ERR(pdata->regulator)) {
			ret = PTR_ERR(pdata->regulator);
			pdata->regulator = NULL;
			pr_err("%s: regulator get fail\n", __func__);
			goto err_out;
		}
	} else {
		pr_info("%s: regulator isn't used\n", __func__);
		pdata->regulator = NULL;
	}

	ret = of_property_read_string(node, "dc_vib,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	return pdata;

err_out:
	return ERR_PTR(ret);
}
#endif

static int dc_vib_probe(struct platform_device *pdev)
{
	struct dc_vib_pdata *pdata = pdev->dev.platform_data;
	struct dc_vib_drvdata *ddata;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = dc_vib_get_dt(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			ret = -ENODEV;
			goto err_pdata;
		}
#else
		ret = -ENODEV;
		pr_err("there is no platform data!\n");
		goto err_pdata;
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(struct dc_vib_drvdata),
			GFP_KERNEL);
	if (!ddata) {
		ret = -ENOMEM;
		pr_err("Failed to memory alloc\n");
		goto err_ddata;
	}
	ddata->pdata = pdata;
	platform_set_drvdata(pdev, ddata);
	ddata->sec_vib_ddata.dev = &pdev->dev;
	ddata->sec_vib_ddata.vib_ops = &dc_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	return 0;

err_ddata:
err_pdata:
	return ret;
}

static int dc_vib_remove(struct platform_device *pdev)
{
	struct dc_vib_drvdata *ddata = platform_get_drvdata(pdev);

	sec_vibrator_unregister(&ddata->sec_vib_ddata);
	if (ddata->pdata->regulator) {
		regulator_put(ddata->pdata->regulator);
	}
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id dc_vib_dt_ids[] = {
	{ .compatible = "samsung,dc_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, dc_vib_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver dc_vib_driver = {
	.probe		= dc_vib_probe,
	.remove		= dc_vib_remove,
	.driver		= {
		.name		= DC_VIB_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(dc_vib_dt_ids),
	},
};

static int __init dc_vib_init(void)
{
	return platform_driver_register(&dc_vib_driver);
}
module_init(dc_vib_init);

static void __exit dc_vib_exit(void)
{
	platform_driver_unregister(&dc_vib_driver);
}
module_exit(dc_vib_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("dc vibrator driver");
MODULE_LICENSE("GPL");
