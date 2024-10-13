// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "pablo-smc.h"

unsigned long pablo_smc(unsigned long cmd, unsigned long arg0,
		unsigned long arg1, unsigned long arg2)
{
	return exynos_smc(cmd, arg0, arg1, arg2);
}
EXPORT_SYMBOL_GPL(pablo_smc);

static int __init pablo_smc_init(void)
{
	return 0;
}
module_init(pablo_smc_init);

static void __exit pablo_smc_exit(void)
{
}
module_exit(pablo_smc_exit)

MODULE_DESCRIPTION("Exynos PABLO SMC controller");
MODULE_LICENSE("GPL");