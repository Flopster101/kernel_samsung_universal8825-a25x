/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for exynos_drm_tui.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_TUI_H__
#define __EXYNOS_DRM_TUI_H__

#include <linux/types.h>
#include <drm/drm_crtc.h>

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include "../../../../misc/tui/stui_hal.h"
#include "../../../../misc/tui/stui_core.h"
#endif

struct resolution_info {
	uint32_t xres;
	uint32_t yres;
	uint32_t mode;
};

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern struct stui_buf_info *(*tui_get_buf_info)(void);
extern void (*tui_free_video_space)(void);
#endif

/* TODO: change with self_refresh */
bool is_tui_trans(const struct drm_crtc_state *crtc_state);
int exynos_drm_atomic_check_tui(struct drm_atomic_state *state);

int exynos_atomic_enter_tui(void);
int exynos_atomic_exit_tui(void);
void exynos_tui_register(struct drm_crtc *crtc);

void exynos_tui_get_resolution(struct resolution_info *res_info);
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
void exynos_tui_set_stui_funcs(struct stui_buf_info *(*func1)(void), void (*func2)(void));
#endif

int exynos_tui_get_panel_info(u64 *buf, int size);

#endif /* __EXYNOS_DRM_TUI_H__ */
