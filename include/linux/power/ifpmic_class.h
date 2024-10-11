/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * ifpmic_class.h
 *
 * Copyright (C) 2021 Samsung Electronics Co.Ltd
 *
 * IFPMIC Class Driver header
 *
 */

#ifndef IFPMIC_CLASS_H
#define IFPMIC_CLASS_H

#if IS_ENABLED(CONFIG_SYSFS_S2MF301)
extern struct device *ifpmic_device_create(void *drvdata, const char *fmt);
extern void ifpmic_device_destroy(dev_t devt);

struct ifpmic_device_attribute {
	struct device_attribute dev_attr;
};

#define IFPMIC_ATTR(_name, _mode, _show, _store)	\
	{ .dev_attr = __ATTR(_name, _mode, _show, _store) }
#else
#define ifpmic_device_create(a, b)        (-1)
#define ifpmic_device_destroy(a)      do { } while (0)
#endif

#define I2C_BASE_TOP	0x74
#define I2C_BASE_FG	0x76
#define I2C_BASE_PD	0x78
#define I2C_BASE_CHG	0x7A
#define I2C_BASE_MUIC	0x7C
#define I2C_BASE_PM	0x7E

#endif /* IFPMIC_CLASS_H */

