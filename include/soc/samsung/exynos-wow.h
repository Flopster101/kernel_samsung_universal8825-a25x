/*
 * exynos-wow.h - Exynos Workload Watcher Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __EXYNOS_WOW_H_
#define __EXYNOS_WOW_H_

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>
#include <linux/workqueue.h>

//#define USE_PPMU

#define PPC_NAME_LENGTH	16

/* PPMU & WoW commen registers */
#define WOW_PPC_PMNC	0x4
#define WOW_PPC_CNTENS	0x8
#define WOW_PPC_CNTENC	0xC
#define WOW_PPC_INTENS	0x10
#define WOW_PPC_INENC	0x14
#define WOW_PPC_PMNC_GLB_CNT_EN 0x1
#define WOW_PPC_PMNC_RESET_CCNT_PMCNT	(0x2 | 0x4)
#define WOW_PPC_PMNC_Q_CH_MODE	(0x1 << 24)

#ifdef USE_PPMU 
/* WoW driver emulated using PPMU */
#define WOW_PPC_CCNT	0x48
#define WOW_PPC_PMCNT0	0x34	// R_ACTIVE	(EV0)
#define WOW_PPC_PMCNT1	0x3C	// RW_DATA	(EV2)
#define WOW_PPC_PMCNT2	0x40	// RW_REQUEST	(EV3)
#define WOW_PPC_PMCNT3	0xBC	// R_MO_COUNT	(EV6)

#define PPMU_PPC_EVENT_EV0_TYPE	0x200	// EV0
#define PPMU_PPC_EVENT_EV1_TYPE	0x208	// EV2
#define PPMU_PPC_EVENT_EV2_TYPE	0x20C	// EV3
#define PPMU_PPC_EVENT_EV3_TYPE	0x218	// EV6

#define PPMU_PPC_EVENT_TYPE_R_ACTIVE	0x0
#define PPMU_PPC_EVENT_TYPE_RW_DATA	0x22
#define PPMU_PPC_EVENT_TYPE_RW_REQUEST	0x21
#define PPMU_PPC_EVENT_TYPE_R_MO_COUNT	0x24

#define WOW_PPC_CNTENS_CCNT_OFFSET	31
#define WOW_PPC_CNTENS_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENS_PMCNT1_OFFSET	2
#define WOW_PPC_CNTENS_PMCNT2_OFFSET	3
#define WOW_PPC_CNTENS_PMCNT3_OFFSET	6

#define WOW_PPC_CNTENC_CCNT_OFFSET	31
#define WOW_PPC_CNTENC_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENC_PMCNT1_OFFSET	2
#define WOW_PPC_CNTENC_PMCNT2_OFFSET	3
#define WOW_PPC_CNTENC_PMCNT3_OFFSET	6
#else

/* WoW driver native WoW IP */
#define WOW_PPC_CCNT		0x48
#define WOW_PPC_PMCNT0		0x34	// RW_ACTIVE
#define WOW_PPC_PMCNT1		0x38	// RW_DATA
#define WOW_PPC_PMCNT2		0x3C	// RW_REQUEST
#define WOW_PPC_PMCNT3		0x40	// RW_MO_COUNT
#define WOW_PPC_PMCNT3_HIGH	0x44	// RW_MO_COUNT_HIGH

#define WOW_PPC_CNTENS_CCNT_OFFSET	31
#define WOW_PPC_CNTENS_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENS_PMCNT1_OFFSET	1
#define WOW_PPC_CNTENS_PMCNT2_OFFSET	2
#define WOW_PPC_CNTENS_PMCNT3_OFFSET	3

#define WOW_PPC_CNTENC_CCNT_OFFSET	31
#define WOW_PPC_CNTENC_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENC_PMCNT1_OFFSET	1
#define WOW_PPC_CNTENC_PMCNT2_OFFSET	2
#define WOW_PPC_CNTENC_PMCNT3_OFFSET	3
#endif

enum exynos_wow_event_counter {
	CCNT,
	RW_ACTIVE,
	RW_DATA,
	RW_REQUEST,
	R_MO_COUNT,
	NUM_WOW_EVENT,
};

enum exynos_wow_event_status {
	WOW_PPC_START,
	WOW_PPC_STOP,
	WOW_PPC_RESET,
};

enum exynos_wow_mode {
	WOW_DISABLED,
	WOW_ENABLED
};

struct exynos_wow_profile {
	u64 ccnt;
	u64 active;
	u64 transfer_data;
	u64 mo_count;
	u64 nr_requests;
};

struct exynos_wow_ip_data {
	void __iomem **wow_base;
	char ppc_name[PPC_NAME_LENGTH];
	u64 wow_counter[NUM_WOW_EVENT];
	unsigned int nr_base;
	unsigned int bus_width;
	unsigned int nr_ppc;

	bool monitor_only;
	bool use_latency;
};

struct exynos_wow_data {
	struct exynos_wow_ip_data *ip_data;
	struct delayed_work dwork;
	unsigned int polling_delay;

	struct exynos_wow_profile profile;

	struct mutex lock;
	unsigned int nr_ip;
	bool mode;
};

extern int exynos_wow_get_data(struct exynos_wow_profile *result);
#endif	/* __EXYNOS_WOW_H_ */
