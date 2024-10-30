/* SPDX-License-Identifier: GPL-2.0-only
 *
 * MIPI-DSI based Samsung common panel driver header
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PANEL_SAMSUNG_DRV_
#define _PANEL_SAMSUNG_DRV_

#include <video/mipi_display.h>
#include <linux/printk.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/backlight.h>

#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_property.h>
#include <drm/drm_mipi_dsi.h>

#include "../exynos_drm_connector.h"
#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
#include "../exynos_drm_drv.h"
#include "panel_drv.h"
#endif
#include "../exynos_drm_debug.h"

int get_panel_log_level(void);
#define panel_info(panel, fmt, ...)	\
dpu_pr_info(drv_name((panel)), 0, get_panel_log_level(), fmt, ##__VA_ARGS__)

#define panel_warn(panel, fmt, ...)	\
dpu_pr_warn(drv_name((panel)), 0, get_panel_log_level(), fmt, ##__VA_ARGS__)

#define panel_err(panel, fmt, ...)	\
dpu_pr_err(drv_name((panel)), 0, get_panel_log_level(), fmt, ##__VA_ARGS__)

#define panel_debug(panel, fmt, ...)	\
dpu_pr_debug(drv_name((panel)), 0, get_panel_log_level(), fmt, ##__VA_ARGS__)

#define MAX_REGULATORS		3
#define MAX_HDR_FORMATS		4

#define exynos_connector_to_panel(c)			\
	container_of((c), struct exynos_panel, exynos_connector)
#define connector_to_exynos_panel(c)			\
	exynos_connector_to_panel(to_exynos_connector(c))
#define bridge_to_exynos_panel(b)			\
	container_of((b), struct exynos_panel, bridge)

struct exynos_panel;

#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
enum exynos_panel_drm_state {
	PANEL_DRM_STATE_DISABLED,
	PANEL_DRM_STATE_LPM_DISABLED,
	PANEL_DRM_STATE_ENABLED,
	PANEL_DRM_STATE_LPM_ENABLED,
};

#define PANEL_DRM_STATE_IS_ENABLED(_state_) \
	((_state_) == PANEL_DRM_STATE_ENABLED || \
	(_state_) == PANEL_DRM_STATE_LPM_ENABLED)

#define PANEL_DRM_STATE_IS_DISABLED(_state_) \
	((_state_) == PANEL_DRM_STATE_DISABLED || \
	(_state_) == PANEL_DRM_STATE_LPM_DISABLED)

#define PANEL_DRM_STATE_IS_LPM(_state_) \
	((_state_) == PANEL_DRM_STATE_LPM_DISABLED || \
	(_state_) == PANEL_DRM_STATE_LPM_ENABLED)

enum {
	MCD_DRM_DRV_WQ_VSYNC = 0,
	MCD_DRM_DRV_WQ_FSYNC = 1,
	MCD_DRM_DRV_WQ_DISPON = 2,
	MCD_DRM_DRV_WQ_PANEL_PROBE = 3,
	MAX_MCD_DRM_DRV_WQ,
};

enum {
	PANEL_CMD_LOG_DSI_TX,
	PANEL_CMD_LOG_DSI_RX,
};

struct mcd_drm_drv_wq {
	int index;
	wait_queue_head_t wait;
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	atomic_t count;
	void *data;
	int ret;
	char *name;
};

#define wq_to_exynos_panel(w)	\
	container_of(w, struct exynos_panel, wqs[((w)->index)])

extern int bypass_display;
extern int panel_cmd_log_level;
extern int commit_retry;
#endif

/*
 * struct exynos_panel_mode - panel mode info
 */
struct exynos_panel_mode {
	/* @mode: drm display mode info */
	struct drm_display_mode mode;
	/* @exynos_mode: exynos driver specific mode info */
	struct exynos_display_mode exynos_mode;
};

struct exynos_panel_funcs {
	int (*set_brightness)(struct exynos_panel *exynos_panel, u16 br);

	/**
	 * @set_lp_mode:
	 *
	 * This callback is used to handle command sequences to enter low power modes.
	 */
	void (*set_lp_mode)(struct exynos_panel *exynos_panel,
			    const struct exynos_panel_mode *mode);

	void (*set_hbm_mode)(struct exynos_panel *exynos_panel, bool hbm_mode);
	/**
	 * @mode_set:
	 *
	 * This callback is used to perform driver specific logic for mode_set.
	 * This could be called while display is on or off, should check internal
	 * state to perform appropriate mode set configuration depending on this state.
	 */
	void (*mode_set)(struct exynos_panel *exynos_panel,
		const struct exynos_panel_mode *mode, unsigned int modeset_flags);
	/*
	 *@ set_clcok_info:
	 *
	 */
	void (*req_set_clock)(struct exynos_panel *exynos_panel, void *arg);

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	/**
	 * @set_fingermask_layer:
	 *
	 * This callback is used to handle a mask_layer request.
	 * Panel-drv's mask_layer func will be called.
	 */
	int (*set_fingermask_layer)(struct exynos_panel *ctx, u32 after);
#endif
};

struct exynos_panel_desc {
	u32 data_lane_cnt;
	u32 hdr_formats; /* supported HDR formats bitmask */
	u32 max_luminance;
	u32 max_avg_luminance;
	u32 min_luminance;
	u32 max_brightness;
	u32 dft_brightness; /* default brightness */
	const struct exynos_panel_mode *modes;
	size_t num_modes;
	/* @lp_mode: provides a low power mode if available, otherwise null */
	const struct exynos_panel_mode *lp_modes;
	size_t num_lp_modes;
	const struct drm_panel_funcs *panel_func;
	const struct exynos_panel_funcs *exynos_panel_func;
	const char *xml_suffix;
	bool lp11_reset;
};

#define MAX_CMDSET_NUM 32
#define MAX_CMDSET_PAYLOAD 2048

struct exynos_panel {
	struct device *dev;
	struct drm_panel panel;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enable_gpio;
	struct regulator *vci;
	struct regulator *vddi;
	struct exynos_drm_connector exynos_connector;
	struct drm_bridge bridge;
	const struct exynos_panel_desc *desc;
	const struct exynos_panel_mode *current_mode;
	struct backlight_device *bl;
	bool enabled;
	bool hbm_mode;

	struct mipi_dsi_msg msg[MAX_CMDSET_NUM];
	int cmdset_msg_total;
	int cmdset_payload_total;
#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
	bool mcd_panel_probed;
	struct mutex probe_lock;
	struct panel_device *mcd_panel_dev;
	enum exynos_panel_drm_state panel_drm_state;
	struct rw_semaphore panel_drm_state_lock;
	struct mcd_drm_drv_wq wqs[MAX_MCD_DRM_DRV_WQ];
	struct ddi_properties ddi_props;
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	bool fingerprint_mask;
#endif
#endif
};

static inline int exynos_dcs_write(struct exynos_panel *ctx, const void *data,
		size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return mipi_dsi_dcs_write_buffer(dsi, data, len);
}

/**
 * exynos_dcs_compression_mode() - sends a compression mode command
 * @ctx: exynos panel device
 * @enable: Whether to enable or disable the DSC
 *
 * Return: 0 on success or a negative error code on failure.
 */
static inline int exynos_dcs_compression_mode(struct exynos_panel *ctx, bool enable)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return mipi_dsi_compression_mode(dsi, enable);
}

static inline int exynos_dcs_set_brightness(struct exynos_panel *ctx, u16 br)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return mipi_dsi_dcs_set_display_brightness(dsi, br);
}

static inline int exynos_dcs_get_brightness(struct exynos_panel *ctx, u16 *br)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return mipi_dsi_dcs_get_display_brightness(dsi, br);
}

#define EXYNOS_DCS_WRITE_SEQ(ctx, seq...) do {				\
	u8 d[] = { seq };						\
	int ret;							\
	ret = exynos_dcs_write(ctx, d, ARRAY_SIZE(d));			\
	if (ret < 0)							\
		dev_err(ctx->dev, "failed to write cmd(%d)\n", ret);	\
} while (0)

#define EXYNOS_DCS_WRITE_SEQ_DELAY(ctx, delay, seq...) do {		\
	EXYNOS_DCS_WRITE_SEQ(ctx, seq);					\
	msleep(delay);							\
} while (0)

#define EXYNOS_DCS_WRITE_TABLE(ctx, table) do {				\
	int ret;							\
	ret = exynos_dcs_write(ctx, table, ARRAY_SIZE(table));		\
	if (ret < 0)							\
		dev_err(ctx->dev, "failed to write cmd(%d)\n", ret);	\
} while (0)

#define EXYNOS_PPS_LONG_WRITE_TABLE(ctx, table) do {			\
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);	\
	int ret;							\
	ret = mipi_dsi_picture_parameter_set(dsi,			\
			(struct drm_dsc_picture_parameter_set*)table);	\
	if (ret < 0)							\
		dev_err(ctx->dev, "failed to write cmd(%d)\n", ret);	\
} while (0)

void exynos_panel_active_off(struct exynos_panel *panel);
int exynos_panel_get_modes(struct drm_panel *panel, struct drm_connector *conn);
void exynos_panel_reset(struct exynos_panel *ctx);
int exynos_panel_set_power(struct exynos_panel *ctx, bool on);
void exynos_panel_set_lp_mode(struct exynos_panel *ctx, const struct exynos_panel_mode *pmode);

int dsim_host_cmdset_transfer(struct mipi_dsi_host *host,
			      struct mipi_dsi_msg *msg, int cmd_cnt,
			      bool wait_vsync, bool wait_fifo);
int exynos_drm_cmdset_add(struct exynos_panel *ctx, u8 type, size_t size, const u8 *data);
int exynos_drm_cmdset_cleanup(struct exynos_panel *ctx);
int exynos_drm_cmdset_flush(struct exynos_panel *ctx, bool wait_vsync,
							bool wait_fifo);

int exynos_panel_probe(struct mipi_dsi_device *dsi);
int exynos_panel_remove(struct mipi_dsi_device *dsi);

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
int drm_mipi_fcmd_write(void *_ctx, const u8 *payload, int size, u32 align);
#endif

#endif /* _PANEL_SAMSUNG_DRV_ */
