/*
 * drivers/samsung/sec_mparam.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>

/* sec_mparam.mparam_ex=n from kernel command line */
int __read_mostly mparam_ex;
module_param(mparam_ex, int, S_IRUGO);

EXPORT_SYMBOL(mparam_ex);

unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, S_IRUGO);
EXPORT_SYMBOL(lpcharge);

int __read_mostly fg_reset;
module_param(fg_reset, int, S_IRUGO);
EXPORT_SYMBOL(fg_reset);

int __read_mostly factory_mode;
module_param(factory_mode, int, S_IRUGO);
EXPORT_SYMBOL(factory_mode);

static int sec_mparam_init(void)
{
	pr_err("%s done!!\n", __func__);
	pr_info("%s: lpcharge=%d, fg_reset=%d, factory_mode=%d\n",
		__func__, lpcharge, fg_reset, factory_mode);
	return 0;
}

module_init(sec_mparam_init);
MODULE_LICENSE("GPL");
