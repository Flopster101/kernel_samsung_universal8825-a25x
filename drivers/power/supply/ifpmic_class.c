// SPDX-License-Identifier: GPL-2.0
/*
 * ifpmic_class.c
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 *
 * IFPMIC Class Driver
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/power/ifpmic_class.h>
#include <linux/module.h>

/* CAUTION : Do not be declared as external ifpmic_class  */
static struct class *ifpmic_class;
static atomic_t ifpmic_dev;

struct device *ifpmic_device_create(void *drvdata, const char *fmt)
{
	struct device *dev;

	if (!ifpmic_class) {
		pr_err("Not yet created class(ifpmic)!\n");
		WARN_ON(0);
	}

	if (IS_ERR(ifpmic_class)) {
		pr_err("Failed to create class(ifpmic) %ld\n", PTR_ERR(ifpmic_class));
		WARN_ON(0);
	}

	dev = device_create(ifpmic_class, NULL, atomic_inc_return(&ifpmic_dev), drvdata, "%s", fmt);
	if (IS_ERR(dev))
		pr_err("Failed to create device %s %ld\n", fmt, PTR_ERR(dev));
	else
		pr_debug("%s : %s : %d\n", __func__, fmt, dev->devt);

	return dev;
}
EXPORT_SYMBOL_GPL(ifpmic_device_create);

void ifpmic_device_destroy(dev_t devt)
{
	pr_info("%s : %d\n", __func__, devt);
	device_destroy(ifpmic_class, devt);
}
EXPORT_SYMBOL_GPL(ifpmic_device_destroy);

static int __init ifpmic_class_create(void)
{
	ifpmic_class = class_create(THIS_MODULE, "ifpmic");

	if (IS_ERR(ifpmic_class)) {
		pr_err("Failed to create class(ifpmic) %ld\n", PTR_ERR(ifpmic_class));
		return PTR_ERR(ifpmic_class);
	}

	return 0;
}
arch_initcall(ifpmic_class_create);

MODULE_DESCRIPTION("ifpmic sysfs");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
