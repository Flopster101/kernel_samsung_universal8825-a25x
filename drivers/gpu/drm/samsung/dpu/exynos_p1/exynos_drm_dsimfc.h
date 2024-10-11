/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_dsimfc.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_DSIMFC_H_
#define _EXYNOS_DRM_DSIMFC_H_

#include <samsung_drm.h>
#include <exynos_drm_drv.h>

#include <dsimfc_cal.h>

#define MAX_DSIMFC_CNT		3

struct dsimfc_device;
struct dsimfc_config;
struct exynos_dsimfc_ops {
	int (*atomic_config)(struct dsimfc_device *dsimfc,
				struct dsimfc_config *config);
	int (*atomic_start)(struct dsimfc_device *dsimfc);
	int (*atomic_stop)(struct dsimfc_device *dsimfc);
};

struct dsimfc_resources {
	struct clk *aclk;
	void __iomem *regs;
	int irq;
};

struct dsimfc_config {
	u8 di;
	u32 size;
	u32 unit;
	dma_addr_t buf;
	u8 done;
};

struct dsimfc_device {
	int id;
	int port;
	const struct exynos_dsimfc_ops	*ops;
	struct device *dev;
	struct dsimfc_resources res;
	struct dsimfc_config *config;
	wait_queue_head_t xferdone_wq;
	spinlock_t slock;
	struct mutex lock;
};

extern struct dsimfc_device *dsimfc_drvdata[MAX_DSIMFC_CNT];

static inline struct dsimfc_device *get_dsimfc_drvdata(u32 id)
{
	if (id < MAX_DSIMFC_CNT)
		return dsimfc_drvdata[id];

	return NULL;
}

void dsimfc_dump(struct dsimfc_device *dsimfc);

#define FCMD_SET_CONFIG			_IOW('C', 0, struct dsimfc_config)
#define FCMD_START			_IOW('C', 1, u32)
#define FCMD_STOP			_IOW('C', 2, u32)
#define FCMD_DUMP			_IOW('C', 3, u32)
#define FCMD_GET_PORT_NUM		_IOR('C', 4, u32)

#endif
