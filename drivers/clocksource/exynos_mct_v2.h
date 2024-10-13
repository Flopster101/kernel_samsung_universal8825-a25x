/* SPDX-License-Identifier: GPL-2.0 */
/**
 * exynos_mct_v2.h - Samsung Exynos MCT(Multi-Core Timer) Driver Header file
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_MCT_V2_H__
#define __EXYNOS_MCT_V2_H__

#define EXYNOS_MCTREG(x)		(x)
#define EXYNOS_MCT_MCT_CFG		EXYNOS_MCTREG(0x000)
#define EXYNOS_MCT_MCT_INCR_RTCCLK	EXYNOS_MCTREG(0x004)
#define EXYNOS_MCT_MCT_FRC_ENABLE	EXYNOS_MCTREG(0x100)
#define EXYNOS_MCT_CNT_L		EXYNOS_MCTREG(0x110)
#define EXYNOS_MCT_CNT_U		EXYNOS_MCTREG(0x114)
#define EXYNOS_MCT_CLKMUX_SEL		EXYNOS_MCTREG(0x120)
#define EXYNOS_MCT_COMPENSATE_VALUE	EXYNOS_MCTREG(0x124)
#define EXYNOS_MCT_COMP_L(i)		EXYNOS_MCTREG(0x200 + ((i) * 0x10))
#define EXYNOS_MCT_COMP_U(i)		EXYNOS_MCTREG(0x204 + ((i) * 0x10))
#define EXYNOS_MCT_INT_ENB		EXYNOS_MCTREG(0x300)
#define EXYNOS_MCT_COMP_PERIOD(i)	EXYNOS_MCTREG(0x400 + ((i) * 0x10))
#define EXYNOS_MCT_COMP_MODE		EXYNOS_MCTREG(0x304)
#define EXYNOS_MCT_COMP_ENABLE		EXYNOS_MCTREG(0x30C)
#define EXYNOS_MCT_INT_CSTAT		EXYNOS_MCTREG(0x310)

#define EXYNOS_MCT_PREPARE_CSTAT       EXYNOS_MCT_COMP_PERIOD(1)

#define MCT_FRC_ENABLE			(0x1)

#define DEFAULT_RTC_CLK_RATE		32768 // 32.768Khz
#define DEFAULT_CLK_DIV			3     // 1/3

#define WAIT_MCT_CNT			10
#define TIMEOUT_LOOP_COUNT		10
#define RETRY_CNT			3

enum {
	MCT_INT_SPI,
	MCT_INT_PPI
};

enum {
	// for global timer
	MCT_COMP0,
	MCT_COMP1,
	MCT_COMP2,
	MCT_COMP3,

	// for local timer
	MCT_COMP4,
	MCT_COMP5,
	MCT_COMP6,
	MCT_COMP7,
	MCT_COMP8,
	MCT_COMP9,
	MCT_COMP10,
	MCT_COMP11,

	MCT_NR_COMPS,
};

struct mct_clock_event_device {
	struct clock_event_device evt;
	char name[10];
	unsigned int comp_index;
	unsigned int disable_time;
};

extern int irq_do_set_affinity(struct irq_data *data, const struct cpumask *mask, bool force);

#endif /* __EXYNOS_MCT_V2_H__ */
