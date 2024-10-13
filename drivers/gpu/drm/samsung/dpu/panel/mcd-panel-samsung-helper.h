/*
 * include/linux/panel_modes.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2020 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PANEL_MODES_H__
#define __EXYNOS_PANEL_MODES_H__

#include "panel-samsung-drv.h"

/**
 * EXYNOS_MODE_FMT - printf string for &struct exynos_display_mode
 */
#define EXYNOS_MODE_FMT    "%lu %d %d %d %d %d %d"

/**
 * EXYNOS_MODE_ARG - printf arguments for &struct exynos_display_mode
 * @m: exynos display mode
 */
#define EXYNOS_MODE_ARG(m) \
	(m)->mode_flags, (m)->bpc, (m)->is_lp_mode, \
	(m)->dsc.enabled, (m)->dsc.dsc_count, (m)->dsc.slice_count, (m)->dsc.slice_height

/**
 * EXYNOS_PANEL_MODE_FMT - printf string for &struct exynos_panel_mode
 */
#define EXYNOS_PANEL_MODE_FMT    DRM_MODE_FMT EXYNOS_MODE_FMT

/**
 * EXYNOS_MODE_ARG - printf arguments for &struct exynos_display_mode
 * @m: exynos panel mode
 */
#define EXYNOS_PANEL_MODE_ARG(m) DRM_MODE_ARG(&(m)->mode), EXYNOS_MODE_ARG(&(m)->exynos_mode)

int drm_display_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct drm_display_mode *ddm);
int exynos_display_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct exynos_display_mode *edm);
int exynos_panel_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct exynos_panel_mode *epm);
void exynos_drm_mode_set_name(struct drm_display_mode *mode, int refresh_mode);
struct panel_display_mode *exynos_panel_find_panel_mode(struct panel_display_modes *pdms,
		const struct drm_display_mode *pmode);
struct exynos_panel_mode *exynos_panel_mode_create(struct exynos_panel *ctx);
void exynos_panel_mode_destroy(struct exynos_panel *ctx, struct exynos_panel_mode *mode);
struct exynos_panel_desc *exynos_panel_desc_create(struct exynos_panel *ctx);
void exynos_panel_desc_destroy(struct exynos_panel *ctx, struct exynos_panel_desc *desc);
struct exynos_panel_desc *
exynos_panel_desc_create_from_panel_display_modes(struct exynos_panel *ctx,
		struct panel_display_modes *pdms);
#endif /* __EXYNOS_PANEL_MODES_H__ */
