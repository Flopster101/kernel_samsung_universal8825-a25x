// SPDX-License-Identifier: GPL-2.0-only
/* mcd_drm_heper.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 * Authors:
 *	Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/unaligned.h>
#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_vblank.h>
#include <exynos_display_common.h>
#include <exynos_drm_decon.h>
#include <panel/panel-samsung-drv.h>
#include <mcd_drm_helper.h>

extern int no_display;
extern int bypass_display;
extern int commit_retry;

bool mcd_drm_check_commit_skip(struct exynos_drm_crtc *exynos_crtc, const char *caller)
{
	bool ret = false;

	if (no_display || bypass_display) {
		pr_info("%s: no_display: %d, bypass_display: %d\n", caller, no_display, bypass_display);
		ret = true;
	}

	return ret;
}

bool mcd_drm_check_commit_retry(struct exynos_drm_crtc *exynos_crtc, const char *caller)
{
	bool ret = false;

	if (commit_retry) {
		pr_info("%s: commit_retry: %d\n", caller, commit_retry);
		ret = true;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_RTC_LIB)
int get_str_cur_rtc(char *buf, size_t size)
{
	int str_size;
	struct rtc_time tm;
	struct timespec64 tv;
	unsigned long local_time;

	if ((!buf) || (size < RTC_STR_BUF_SIZE)) {
		pr_err("%s: invalid parameter size: %d\n", __func__, size);
		return -EINVAL;
	}

	ktime_get_real_ts64(&tv);

	local_time = (tv.tv_sec - (sys_tz.tz_minuteswest * 60));
	rtc_time64_to_tm(local_time, &tm);

	str_size = sprintf(buf, "%02d-%02d %02d:%02d:%02d",
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return 0;
}
#endif

static const char * const get_disp_panic_reason_string(enum disp_panic_reason reason) 
{
	static const char * const disp_panic_reason_string[MAX_DISP_PANIC_REASON] = {
		[DISP_PANIC_REASON_DECON_STUCK] = "DECON_STUCK",
		[DISP_PANIC_REASON_DSIM_STUCK] = "DSIM_STUCK",
		[DISP_PANIC_REASON_PANEL_NO_TE] = "PANEL_NO_TE",
		[DISP_PANIC_REASON_UNKNOWN] = "UNKNOWN",
		[DISP_PANIC_REASON_RECOVERY_FAIL] = "RECOVERY_FAIL",
	};

	if (reason >= MAX_DISP_PANIC_REASON)
		return NULL;

    return disp_panic_reason_string[reason];
}

static const char * const get_disp_panic_bigdata_key_string(enum disp_panic_bigdata_key key)
{
	static const char * const disp_panic_bigdata_key_string[MAX_DISP_PANIC_BIGDATA_KEY] = {
		[DISP_PANIC_BIGDATA_KEY_ID] = "ID",
		[DISP_PANIC_BIGDATA_KEY_REASON] = "REASON",
		[DISP_PANIC_BIGDATA_KEY_RECOVERYCNT] = "RECOVERYCNT",
		[DISP_PANIC_BIGDATA_KEY_DISPCLK] = "DISPCLK",
	};

	if (key >= MAX_DISP_PANIC_BIGDATA_KEY)
		return NULL;

    return disp_panic_bigdata_key_string[key];
}

int snprintf_disp_panic_decon_id(char *buf, size_t size, unsigned int decon_id)
{
	int len = 0;

	if (!buf || !size)
		return 0;

	return snprintf(buf + len, size - len, "%s:%d ",
			get_disp_panic_bigdata_key_string(DISP_PANIC_BIGDATA_KEY_ID), decon_id);
}

int snprintf_disp_panic_reason(char *buf, size_t size, enum disp_panic_reason reason)
{
	int len = 0;

	if (!buf || !size)
		return 0;

	return snprintf(buf + len, size - len, "%s:%s ",
			get_disp_panic_bigdata_key_string(DISP_PANIC_BIGDATA_KEY_REASON),
			get_disp_panic_reason_string(reason));
}

int snprintf_disp_panic_recovery_count(char *buf, size_t size, unsigned int recovery_count)
{
	int len = 0;

	if (!buf || !size)
		return 0;

	return snprintf(buf + len, size - len, "%s:%d ",
			get_disp_panic_bigdata_key_string(DISP_PANIC_BIGDATA_KEY_RECOVERYCNT),
			recovery_count);
}

int snprintf_disp_panic_disp_clock(char *buf, size_t size, u64 disp_clock)
{
	int len = 0;

	if (!buf || !size)
		return 0;

	return snprintf(buf + len, size - len, "%s:%llu ",
			get_disp_panic_bigdata_key_string(DISP_PANIC_BIGDATA_KEY_DISPCLK), disp_clock);
}

static bool mcd_drm_is_no_te_interrupt(struct exynos_drm_crtc *exynos_crtc)
{
	int eint_pend;

	if (!exynos_crtc)
		return false;

	if (exynos_crtc->d.eint_pend) {
		eint_pend = readl(exynos_crtc->d.eint_pend);
		pr_info("%s: eint_pend: 0x%02x\n", __func__, eint_pend);
		if (eint_pend == 0)
			return true;
	}
	return false;
}

bool customer_condition_check(const struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc;

	if (!crtc)
		return false;

	exynos_crtc = to_exynos_crtc(crtc);
	if (!exynos_crtc || !exynos_crtc->ctx)
		return false;

	if (mcd_drm_is_no_te_interrupt(exynos_crtc))
		return true;
	return false;
}

