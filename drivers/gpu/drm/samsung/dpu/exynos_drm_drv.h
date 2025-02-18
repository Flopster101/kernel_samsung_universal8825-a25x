/* SPDX-License-Identifier: GPL-2.0-only
 * exynos_drm_drv.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_DRV_H_
#define _EXYNOS_DRM_DRV_H_

#include <drm/drm_atomic.h>
#include <drm/drm_property.h>

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <samsung_drm.h>
#include <exynos_drm_connector.h>

#define get_plane_num(drm_dev) (drm_dev->mode_config.num_total_plane)

/* this enumerates display type. */
enum exynos_drm_output_type {
	EXYNOS_DISPLAY_TYPE_NONE = 0,
	/* RGB or CPU Interface. */
	EXYNOS_DISPLAY_TYPE_DSI0 = BIT(0),
	EXYNOS_DISPLAY_TYPE_DSI1 = BIT(1),
	EXYNOS_DISPLAY_TYPE_DSI  = EXYNOS_DISPLAY_TYPE_DSI0 |
					EXYNOS_DISPLAY_TYPE_DSI1,
	/* HDMI Interface. */
	EXYNOS_DISPLAY_TYPE_HDMI = BIT(2),
	/* Virtual Display Interface. */
	EXYNOS_DISPLAY_TYPE_VIDI = BIT(3),

	/* DP Interface. */
	EXYNOS_DISPLAY_TYPE_DP0_SST1  = BIT(4),
	EXYNOS_DISPLAY_TYPE_DP0_SST2  = BIT(5),
	EXYNOS_DISPLAY_TYPE_DP0_SST3  = BIT(6),
	EXYNOS_DISPLAY_TYPE_DP0_SST4  = BIT(7),
	EXYNOS_DISPLAY_TYPE_DP1_SST1  = BIT(8),
	EXYNOS_DISPLAY_TYPE_DP1_SST2  = BIT(9),
	EXYNOS_DISPLAY_TYPE_DP1_SST3  = BIT(10),
	EXYNOS_DISPLAY_TYPE_DP1_SST4  = BIT(11),
	EXYNOS_DISPLAY_TYPE_DP0   = EXYNOS_DISPLAY_TYPE_DP0_SST1 |
					EXYNOS_DISPLAY_TYPE_DP0_SST2 |
					EXYNOS_DISPLAY_TYPE_DP0_SST3 |
					EXYNOS_DISPLAY_TYPE_DP0_SST4,
	EXYNOS_DISPLAY_TYPE_DP1   = EXYNOS_DISPLAY_TYPE_DP1_SST1 |
					EXYNOS_DISPLAY_TYPE_DP1_SST2 |
					EXYNOS_DISPLAY_TYPE_DP1_SST3 |
					EXYNOS_DISPLAY_TYPE_DP1_SST4,
};

enum exynos_drm_writeback_type {
	EXYNOS_WB_NONE,
	EXYNOS_WB_CWB,
	EXYNOS_WB_SWB,
};

struct drm_exynos_file_private {
	u32 dummy;
};

struct exynos_drm_priv_state {
	struct drm_private_state base;
	/* If need shared resource control for multi-display pipeline, add it here. */
};

static inline struct exynos_drm_priv_state *
to_exynos_priv_state(const struct drm_private_state *state)
{
	return container_of(state, struct exynos_drm_priv_state, base);
}

struct exynos_drm_crtc_properties {
	struct drm_property *color_mode;
	struct drm_property *render_intent;
	struct drm_property *adjusted_vblank;
	struct drm_property *operation_mode;
	struct drm_property *dsr_status;
	struct drm_property *modeset_only;
	struct drm_property *partial;
	struct drm_property *dqe_fd;
	struct drm_property *bts_fps;
};

struct exynos_drm_plane_properties {
	struct drm_property *restriction;
	struct drm_property *standard;
	struct drm_property *transfer;
	struct drm_property *range;
	struct drm_property *colormap;
	struct drm_property *split;
	struct drm_property *hdr_fd;
};

/*
 * Exynos drm private structure.
 *
 * @da_start: start address to device address space.
 *	with iommu, device address space starts from this address
 *	otherwise default one.
 * @da_space_size: size of device address space.
 *	if 0 then default value is used for it.
 * @pending: the crtcs that have pending updates to finish
 * @lock: mutext lock to protect available_win_mask.
 * @wait: wait an atomic commit to finish
 */
struct exynos_drm_private {
	struct drm_fb_helper *fb_helper;
	struct drm_atomic_state *suspend_state;
	struct device *iommu_client;
	struct device *dma_dev;
	void *mapping;

	/* for atomic commit */
	u32			pending;
	struct mutex		lock;
	wait_queue_head_t	wait;

	struct exynos_drm_crtc_properties crtc_props;
	struct exynos_drm_plane_properties plane_props;
	struct exynos_drm_connector_properties connector_props;
	struct drm_private_obj obj;
	unsigned int available_win_mask;
	struct timer_list debug_timer;
};

static inline struct device *to_dma_dev(struct drm_device *dev)
{
	struct exynos_drm_private *priv = dev->dev_private;

	return priv->dma_dev;
}

int exynos_atomic_commit(struct drm_device *dev, struct drm_atomic_state *state,
			 bool nonblock);
int exynos_atomic_check(struct drm_device *dev, struct drm_atomic_state *state);
void exynos_drm_init_resource_cnt(void);

inline void *
dev_get_exynos_properties(struct drm_device *dev, int obj_type);

extern struct platform_driver decon_driver;
extern struct platform_driver dsim_driver;
extern struct platform_driver dpp_driver;
extern struct platform_driver writeback_driver;
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
extern struct platform_driver dsimfc_driver;
#endif
#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DP)
extern struct platform_driver dp_driver;
#endif
#endif
