/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for LCD partial update.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_PARTIAL_H__
#define __EXYNOS_DRM_PARTIAL_H__

#include <drm/drm_rect.h>
#include <drm/drm_modes.h>
#include <drm/drm_plane.h>
/* TODO remove decon dependency */

struct exynos_partial;
struct exynos_drm_crtc_state;
struct exynos_drm_crtc;
struct exynos_display_mode;

struct exynos_partial_funcs {
	int (*init)(struct exynos_partial *partial,
			const struct drm_display_mode *mode,
			const struct exynos_display_mode *exynos_mode);
	int (*adjust_partial_region)(struct exynos_partial *partial,
			const struct drm_display_mode *mode,
			const struct drm_rect *req, const struct drm_rect *r);
	bool (*check)(struct exynos_partial *partial,
			struct exynos_drm_crtc_state *exynos_crtc_state);
	void (*reconfig_coords)(struct exynos_partial *partial,
			struct exynos_drm_crtc_state *exynos_crtc_state);
	int (*send_partial_command)(struct exynos_partial *partial,
			const struct drm_rect *partial_r);
	void (*set_partial_size)(struct exynos_partial *partial,
			const struct drm_rect *partial_r);
};

struct exynos_partial {
	bool enabled;
	u32 min_w;
	u32 min_h;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_partial_funcs *funcs;
};

struct exynos_partial *
exynos_partial_register(struct exynos_drm_crtc *exynos_crtc);
void exynos_partial_initialize(struct exynos_partial *partial,
			const struct drm_display_mode *mode,
			const struct exynos_display_mode *exynos_mode);
void exynos_partial_prepare(struct exynos_partial *partial,
		struct exynos_drm_crtc_state *old_exynos_crtc_state,
		struct exynos_drm_crtc_state *new_exynos_crtc_state);
void exynos_partial_reconfig_coords(struct exynos_partial *partial,
		struct drm_plane_state *plane_state,
		const struct drm_rect *partial_r);
void exynos_partial_update(struct exynos_partial *partial,
		const struct drm_rect *old_partial_region,
		struct drm_rect *new_partial_region);
void exynos_partial_restore(struct exynos_partial *partial);
void exynos_partial_set_full(const struct drm_display_mode *mode,
		struct drm_rect *partial_r);

#endif /* __EXYNOS_DRM_PARTIAL_H__ */
