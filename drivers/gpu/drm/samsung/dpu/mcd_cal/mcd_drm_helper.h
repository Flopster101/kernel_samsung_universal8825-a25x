/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/mcd_cal/mcd_drm_dsim.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung MCD DRM Helper.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_DRM_HELPER_H__
#define __MCD_DRM_HELPER_H__

#include <linux/time64.h>
#include <linux/rtc.h>

/* Add header */
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_property.h>
#include <drm/drm_panel.h>
#include <video/videomode.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_crtc.h>


#define RTC_STR_BUF_SIZE 32

bool mcd_drm_check_commit_skip(struct exynos_drm_crtc *exynos_crtc, const char *caller);
bool mcd_drm_check_commit_retry(struct exynos_drm_crtc *exynos_crtc, const char *caller);

#if IS_ENABLED(CONFIG_RTC_LIB)
int get_str_cur_rtc(char *buf, size_t size);
#else
static inline int get_str_cur_rtc(char *buf, size_t size) { return -EINVAL; }
#endif

enum disp_panic_reason {
	DISP_PANIC_REASON_DECON_STUCK,
	DISP_PANIC_REASON_DSIM_STUCK,
	DISP_PANIC_REASON_PANEL_NO_TE,
	DISP_PANIC_REASON_UNKNOWN,
	DISP_PANIC_REASON_RECOVERY_FAIL,
	MAX_DISP_PANIC_REASON,
};

enum disp_panic_bigdata_key {
    DISP_PANIC_BIGDATA_KEY_ID,
    DISP_PANIC_BIGDATA_KEY_REASON,
    DISP_PANIC_BIGDATA_KEY_RECOVERYCNT,
    DISP_PANIC_BIGDATA_KEY_DISPCLK,
    MAX_DISP_PANIC_BIGDATA_KEY,
};

int snprintf_disp_panic_decon_id(char *buf, size_t size, unsigned int decon_id);
int snprintf_disp_panic_reason(char *buf, size_t size, enum disp_panic_reason reason);
int snprintf_disp_panic_recovery_count(char *buf, size_t size, unsigned int recovery_count);
int snprintf_disp_panic_disp_clock(char *buf, size_t size, u64 disp_clock);
bool customer_condition_check(const struct drm_crtc *crtc);
#endif //__MCD_DRM_HELPER_H__
