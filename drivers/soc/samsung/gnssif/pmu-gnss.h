/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PMU_GNSS_H
#define __EXYNOS_PMU_GNSS_H __FILE__

#define MEMBASE_ADDR_SHIFT	12
#define MEMBASE_ADDR_MASK	(GENMASK(23, 0))
#define MEMBASE_ADDR_OFFSET	0

/* Base address in the point of GNSS view */
#define MEMBASE_GNSS_ADDR	(0x60000000)
#define MEMBASE_GNSS_ADDR_2ND	(0xA0000000)

#if IS_ENABLED(CONFIG_SOC_EXYNOS9630)
#define EXYNOS_PMU_GNSS_CTRL_NS		0x3290
#define EXYNOS_PMU_GNSS_CTRL_S		0x3294
#define EXYNOS_PMU_GNSS_STAT	0x0048
#define EXYNOS_PMU_GNSS_DEBUG	0x004C
#define GNSS_ACTIVE_REQ_CLR	BIT(8)
#elif IS_ENABLED(CONFIG_SOC_EXYNOS3830)
#define EXYNOS_PMU_GNSS_CTRL_NS		0x3090
#define EXYNOS_PMU_GNSS_CTRL_S		0x3094
#define EXYNOS_PMU_GNSS_STAT	0x0048
#define EXYNOS_PMU_GNSS_DEBUG	0x004C
#elif IS_ENABLED(CONFIG_SOC_S5E9815)
#define EXYNOS_PMU_GNSS_CTRL_NS		0x3890
#define EXYNOS_PMU_GNSS_CTRL_S		0x3894
#define EXYNOS_PMU_GNSS_STAT	0x0048
#define EXYNOS_PMU_GNSS_DEBUG	0x004C
#elif IS_ENABLED(CONFIG_SOC_S5E9925)
#define EXYNOS_PMU_GNSS_CTRL_NS		0x3990
#define EXYNOS_PMU_GNSS_CTRL_S		0x3994
#define EXYNOS_PMU_GNSS_STAT	0x0048
#define EXYNOS_PMU_GNSS_DEBUG	0x004C
#elif IS_ENABLED(CONFIG_SOC_S5E8825)
#define EXYNOS_PMU_GNSS_CTRL_NS	0x3590
#define EXYNOS_PMU_GNSS_CTRL_S	0x3594
#define EXYNOS_PMU_GNSS_STAT	0x0048
#define EXYNOS_PMU_GNSS_DEBUG	0x004C
#endif

enum gnss_mode {
	GNSS_POWER_OFF,
	GNSS_POWER_ON,
	GNSS_RESET,
	NUM_GNSS_MODE,
};

enum gnss_int_clear {
	GNSS_INT_WDT_RESET_CLEAR,
	GNSS_INT_ACTIVE_CLEAR,
	GNSS_INT_WAKEUP_CLEAR,
};

enum gnss_tcxo_mode {
	TCXO_SHARED_MODE = 0,
	TCXO_NON_SHARED_MODE = 1,
};

struct gnss_swreg {
	u32 swreg_0;
	u32 swreg_1;
	u32 swreg_2;
	u32 swreg_3;
	u32 swreg_4;
	u32 swreg_5;
};

struct gnss_apreg {
	u32 CTRL_NS;
	u32 CTRL_S;
	u32 STAT;
	u32 DEBUG;
};

struct gnss_ctl;

struct gnssctl_pmu_ops {
	int (*init_conf)(struct gnss_ctl *gc);
	int (*hold_reset)(void);
	int (*release_reset)(void);
	int (*power_on)(enum gnss_mode mode);
	int (*clear_int)(enum gnss_int_clear int_clear);
	int (*change_tcxo_mode)(enum gnss_tcxo_mode tcxo_mode);
	int (*req_security)(void);
	void (*req_baaw)(void);
	void (*get_swreg)(struct gnss_swreg *swreg);
	void (*get_apreg)(struct gnss_apreg *apreg);
};

void gnss_get_pmu_ops(struct gnss_ctl *gc);
#endif /* __EXYNOS_PMU_GNSS_H */
