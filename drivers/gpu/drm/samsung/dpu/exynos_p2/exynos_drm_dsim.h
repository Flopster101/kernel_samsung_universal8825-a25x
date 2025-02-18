/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/exynos_drm_dsim.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Samsung MIPI DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DSI_H__
#define __EXYNOS_DRM_DSI_H__

/* Add header */
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_property.h>
#include <drm/drm_panel.h>
#include <video/videomode.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_crtc.h>
#include <dsim_cal.h>

enum dsim_state {
	DSIM_STATE_INIT,	/* already enabled state froam LK display */
	DSIM_STATE_HSCLKEN,	/* enable-state */
	DSIM_STATE_ULPS,        /* ulps-state by hibernation */
	DSIM_STATE_SUSPEND      /* disable-state */
};

struct dsim_pll_params {
	unsigned int num_modes;
	struct dsim_pll_param **params;
};

struct dsim_resources {
	void __iomem *regs;
	void __iomem *phy_regs;
	void __iomem *phy_regs_ex;
	void __iomem *ss_reg_base;
	struct phy *phy;
	struct phy *phy_ex;
};

struct dsim_fb_handover {
	/* true  - fb reserved     */
	/* false - fb not reserved */
	bool reserved;
	phys_addr_t phys_addr;
	size_t phys_size;
	struct reserved_mem *rmem;
	int lk_fb_win_id;
};
struct exynos_dsim_cmd_set {
	u32 cnt;
	u32 index[32];
};
struct dsim_burst_cmd {
	bool init_done;
	u32 pl_count;
	u32 line_count;
};
struct dsim_freq_hop {
	bool enabled;
	u32 target_m; /* will be applied to DPHY */
	u32 target_k; /* will be applied to DPHY */
	u32 request_m; /* user requested m value */
	u32 request_k; /* user requested m value */
};

struct dsim_device {
	struct drm_encoder encoder;
	struct mipi_dsi_host dsi_host;
	struct device *dev;
	struct drm_bridge *panel_bridge;
	struct mipi_dsi_device *dsi_device;

	enum exynos_drm_output_type output_type;

	struct dsim_resources res;
	struct clk **clks;
	struct dsim_pll_params *pll_params;
	int irq;
	int id;
	spinlock_t slock;
	struct mutex cmd_lock;
	struct mutex freq_hop_lock;
	struct completion ph_wr_comp;
	struct completion rd_comp;
	struct timer_list cmd_timer;
	struct work_struct cmd_work;

	enum dsim_state state;
	bool lp_mode_state;

	/* set bist mode by sysfs */
	unsigned int bist_mode;

	/* FIXME: dsim cal structure */
	struct dsim_reg_config config;
	struct dsim_clks clk_param;

	struct dsim_freq_hop freq_hop;
	int idle_ip_index;
	struct dsim_fb_handover fb_handover;

	struct dsim_burst_cmd burst_cmd;
	bool lp11_reset;
};

extern struct dsim_device *dsim_drvdata[MAX_DSI_CNT];

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	if (id < MAX_DSI_CNT)
		return dsim_drvdata[id];

	return NULL;
}

#define to_dsi(nm)	container_of(nm, struct dsim_device, nm)

#define MIPI_WR_TIMEOUT				msecs_to_jiffies(50)
#define MIPI_RD_TIMEOUT				msecs_to_jiffies(100)

#define DSIM_RX_FIFO_MAX_DEPTH			64
#define DSIM_PL_FIFO_THRESHOLD			2048	/*this value depends on H/W */

struct decon_device;

static inline struct exynos_drm_crtc *
dsim_get_exynos_crtc(const struct dsim_device *dsim)
{
	/* TODO: change to &drm_connector_state.crtc */
	const struct drm_crtc *crtc = dsim->encoder.crtc;

	if (!crtc)
		return NULL;

	return to_exynos_crtc(crtc);
}

static inline const struct decon_device *
dsim_get_decon(const struct dsim_device *dsim)
{
	/* TODO: change to &drm_connector_state.crtc */
	const struct drm_crtc *crtc = dsim->encoder.crtc;

	if (!crtc)
		return NULL;

	return to_exynos_crtc(crtc)->ctx;
}

static inline bool dsim_is_fb_reserved(const struct dsim_device *dsim)
{
	return dsim != NULL && dsim->fb_handover.reserved;
}

void dsim_enter_ulps(struct dsim_device *dsim);
void dsim_exit_ulps(struct dsim_device *dsim);
void dsim_wait_pending_vblank(struct dsim_device *dsim);
void dsim_dump(struct dsim_device *dsim);
int dsim_free_fb_resource(struct dsim_device *dsim);
void dsim_atomic_activate(struct drm_encoder *encoder);
#endif /* __EXYNOS_DRM_DSI_H__ */
