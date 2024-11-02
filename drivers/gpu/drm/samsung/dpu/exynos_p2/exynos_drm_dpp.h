/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_dpp.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Seong-gyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_DPP_H_
#define _EXYNOS_DRM_DPP_H_

#include <exynos_drm_drv.h>
#include <samsung_drm.h>

#include <dpp_cal.h>
#include <exynos_drm_hdr.h>

#define plane_to_dpp(p)		container_of(p, struct dpp_device, plane)

enum dpp_state {
	DPP_STATE_OFF = 0,
	DPP_STATE_ON,
};

struct dpp_device {
	struct device *dev;

	u32 id;
	u32 port;
	unsigned long attr;
	enum dpp_state state;

	int dma_irq;
	int dpp_irq;

	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;

	struct dpp_regs	regs;
	struct dpp_params_info win_config;

	spinlock_t slock;
	spinlock_t dma_slock;

	int decon_id;		/* connected DECON id */
	bool protection;

	struct dpp_restriction restriction;
	struct dpp_dpuf_resource dpuf_resource;

	struct exynos_drm_plane plane;
	struct exynos_hdr *hdr;
};

#ifdef CONFIG_OF
struct dpp_device *of_find_dpp_by_node(struct device_node *np);
#else
static struct dpp_device *of_find_dpp_by_node(struct device_node *np)
{
	return NULL;
}
#endif

extern struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static inline struct dpp_device *get_dpp_drvdata(u32 id)
{
	if (id < MAX_DPP_CNT)
		return dpp_drvdata[id];

	return NULL;
}

void dpp_dump(struct dpp_device *dpps[], int dpp_cnt);
void dpp_print_restriction(u32 id, struct dpp_restriction *res);

void dpp_init_dpuf_resource_cnt(void);

extern const struct dpp_restriction dpp_drv_data;
#endif
