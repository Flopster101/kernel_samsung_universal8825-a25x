/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * ARM Theodul DSU errata handling routine.
 *
 * Copyright (C) Samsung Limited, 2021.
 *
 */

#if defined(CONFIG_SOC_S5E9925)
#include <soc/samsung/exynos-smc.h>
#endif

#if defined(CONFIG_SOC_S5E9925)
static void arm_smcc_disable_clock_gating(void)
{
	exynos_smc(0x82000530, 0x10000, 0, 0);
}

static void arm_smcc_enable_clock_gating(void)
{
	exynos_smc(0x82000530, 0x10000, 1, 0);
}
#else
static void arm_smcc_disable_clock_gating(void)
{
}

static void arm_smcc_enable_clock_gating(void)
{
}
#endif
