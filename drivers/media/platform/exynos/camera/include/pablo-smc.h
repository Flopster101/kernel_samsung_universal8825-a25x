/* SPDX-License-Identifier: GPL */
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

#ifndef PABLO_SMC_H
#define PABLO_SMC_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/exynos-smc.h>
#else
#include <linux/smc.h>
#endif

unsigned long pablo_smc(unsigned long cmd, unsigned long arg0,
		unsigned long arg1, unsigned long arg2);

#endif
