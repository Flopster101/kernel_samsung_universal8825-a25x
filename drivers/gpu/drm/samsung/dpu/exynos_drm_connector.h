// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_connector.h
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_DRM_CONNECTOR_H_
#define _EXYNOS_DRM_CONNECTOR_H_

#include <drm/drm_atomic.h>
#include <drm/drm_connector.h>

/* bit mask for modeset_flags */
#define SEAMLESS_MODESET_MRES	BIT(0)
#define SEAMLESS_MODESET_VREF	BIT(1)
#define SEAMLESS_MODESET_LP	BIT(2)

struct exynos_drm_connector;

struct exynos_drm_connector_properties {
	struct drm_property *max_luminance;
	struct drm_property *max_avg_luminance;
	struct drm_property *min_luminance;
	struct drm_property *hdr_formats;
	struct drm_property *adjusted_fps;
	struct drm_property *lp_mode;
	struct drm_property *hdr_sink_connected;
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	struct drm_property *fingerprint_mask;
#endif
};

struct exynos_display_dsc {
	bool enabled;
	unsigned int dsc_count;
	unsigned int slice_count;
	unsigned int slice_height;
};

#define EXYNOS_MODE(flag, b, lp_mode, dsc_en, dsc_cnt, slice_cnt, slice_h)	\
	.mode_flags = flag, .bpc = b, .is_lp_mode = lp_mode,			\
	.dsc = { .enabled = dsc_en, .dsc_count = dsc_cnt,			\
		.slice_count = slice_cnt, .slice_height = slice_h }		\

/* struct exynos_display_mode - exynos display specific info */
struct exynos_display_mode {
	/* @dsc: DSC parameters for the selected mode */
	struct exynos_display_dsc dsc;
	/* DSI mode flags in drm_mipi_dsi.h */
	unsigned long mode_flags;
	/* command: TE pulse time, video: vbp+vfp time */
	unsigned int vblank_usec;
	/* @bpc: display bits per component */
	unsigned int bpc;
	/* @is_lp_mode: boolean, if true it means this mode is a Low Power mode */
	bool is_lp_mode;
	/* @vscan_coef: scanning N times, in one frame */
	u16 vscan_coef;
	/* @bts_fps: fps value to calculate the DPU BW */
	unsigned int bts_fps;
};

/* struct exynos_drm_connector_state - mutable connector state */
struct exynos_drm_connector_state {
	/* @base: base connector state */
	struct drm_connector_state base;
	/* @exynos: additional mode details */
	struct exynos_display_mode exynos_mode;
	/* @seamless_modeset: flag for seamless modeset */
	unsigned int seamless_modeset;
	/* @adjusted_fps: applied fps value for display pipeline */
	unsigned int adjusted_fps;
	/* @is_lp_transition: boolean, if true it means transition
	   between doze and doze_suspend */
	bool is_lp_transition;
	/*
	 * @requested_panel_recovery : boolean, if true then panel requested
	 *  on & off during the recovery
	 */
	bool requested_panel_recovery;
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	/* @fingerprint_mask: boolead, if true it means fingerprint is on */
	bool fingerprint_mask;
#endif
};

#define to_exynos_connector_state(c)		\
	container_of((c), struct exynos_drm_connector_state, base)
#define to_exynos_connector(c)			\
	container_of((c), struct exynos_drm_connector, base)

struct exynos_drm_connector_funcs {
	void (*atomic_print_state)(struct drm_printer *p,
			const struct exynos_drm_connector_state *state);
	int (*atomic_set_property)(struct exynos_drm_connector *exynos_conn,
			struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t val);
	int (*atomic_get_property)(struct exynos_drm_connector *exynos_conn,
			const struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t *val);
	void (*query_status)(struct exynos_drm_connector *exynos_conn);
};

struct exynos_drm_connector {
	struct drm_connector base;
	const struct exynos_drm_connector_funcs *funcs;
	u32 boost_bts_fps;
	ktime_t boost_expire_time;
};

void exynos_drm_boost_bts_fps(struct exynos_drm_connector *exynos_connector,
		u32 fps, ktime_t expire_time);
bool is_exynos_drm_connector(const struct drm_connector *connector);
int exynos_drm_connector_init(struct drm_device *dev,
		struct exynos_drm_connector *exynos_connector,
		const struct exynos_drm_connector_funcs *funcs,
		int connector_type);
int exynos_drm_connector_create_properties(struct drm_device *dev);
struct exynos_drm_connector_properties *
exynos_drm_connector_get_properties(struct exynos_drm_connector *exynos_connector);

static inline struct exynos_drm_connector_state *
crtc_get_exynos_connector_state(const struct drm_atomic_state *state,
		const struct drm_crtc_state *crtc_state)
{
	const struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	int i;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		if (!(crtc_state->connector_mask & drm_connector_mask(conn)))
			continue;

		if (is_exynos_drm_connector(conn))
			return to_exynos_connector_state(conn_state);
	}

	return NULL;
}

static inline struct exynos_drm_connector *
crtc_get_exynos_connector(const struct drm_atomic_state *state,
		const struct drm_crtc_state *crtc_state)
{
	const struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	int i;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		if (!(crtc_state->connector_mask & drm_connector_mask(conn)))
			continue;

		if (is_exynos_drm_connector(conn))
			return to_exynos_connector(conn);
	}

	return NULL;
}
#endif /* _EXYNOS_DRM_CONNECTOR_H */
