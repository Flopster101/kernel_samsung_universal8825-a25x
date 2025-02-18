// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU migov file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos_drm_migov.h>
#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/xperf.h>
#include <exynos_drm_crtc.h>

#define MIGOV_PERIOD_NSEC	100000000UL
#define FPS_UNIT_NSEC		1000000000UL	/* 1sec */

/* only valid for LCD display crtc */
static struct exynos_drm_crtc *primary_exynos_crtc;

static void get_frame_vsync_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_migov *migov = primary_exynos_crtc->migov;
	const struct drm_crtc_state *crtc_state = primary_exynos_crtc->base.state;
	u64 lcd_fps;
	u64 vsync_period_nsec;
	static u64 vsync_cnt;
	static ktime_t prev_ktime;
	ktime_t vsync_time;
	ktime_t cur_ktime = ktime_get();
	ktime_t ktime_diff;

	lcd_fps = drm_mode_vrefresh(&crtc_state->mode);
	if (!lcd_fps) {
		pr_info("%s: there is no vrefresh\n", __func__);
		return;
	}
	vsync_period_nsec = FPS_UNIT_NSEC / lcd_fps;

	if (vsync_cnt) {
		if (prev_ktime) {
			ktime_diff = cur_ktime - prev_ktime;
			vsync_cnt += DIV_ROUND_CLOSEST(lcd_fps * ktime_diff, FPS_UNIT_NSEC);
		} else
			vsync_cnt += lcd_fps * MIGOV_PERIOD_NSEC / FPS_UNIT_NSEC;
	} else
		vsync_cnt = migov->frame_vsync_cnt;

	if ((cur_ktime - migov->frame_vsync_cnt_time) > vsync_period_nsec)
		vsync_time = migov->frame_vsync_cnt_time + vsync_period_nsec;
	else
		vsync_time = migov->frame_vsync_cnt_time;

	*cnt = vsync_cnt;
	*time = vsync_time;
	prev_ktime = cur_ktime;
}

static void get_ems_frame_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_migov *migov = primary_exynos_crtc->migov;

	*cnt = migov->ems_frame_cnt;
	*time = migov->ems_frame_cnt_time;
}

static void get_ems_fence_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_migov *migov = primary_exynos_crtc->migov;

	*cnt = migov->ems_fence_cnt;
	*time = migov->ems_fence_cnt_time;
}

#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
void exynos_stats_set_vsync(ktime_t time_real_us);
#endif
void exynos_migov_update_vsync_cnt(struct exynos_migov *migov)
{
	migov->frame_vsync_cnt++;
	migov->frame_vsync_cnt_time = ktime_get();
#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
	exynos_stats_set_vsync(ktime_get_real() / 1000UL);
#endif
}

void exynos_migov_update_ems_frame_cnt(struct exynos_migov *migov)
{
	migov->ems_frame_cnt++;
	migov->ems_frame_cnt_time = ktime_get();
}

void exynos_migov_update_ems_fence_cnt(struct exynos_migov *migov)
{
	migov->ems_fence_cnt++;
	migov->ems_fence_cnt_time = ktime_get();
}

struct exynos_migov *exynos_migov_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_migov *migov;
	struct drm_crtc *crtc = &exynos_crtc->base;

	/* migov support only lcd crtc(index == 0) */
	if (!IS_ENABLED(CONFIG_EXYNOS_MIGOV) ||	crtc->index)
		return NULL;

	migov = kzalloc(sizeof(struct exynos_migov), GFP_KERNEL);

	primary_exynos_crtc = exynos_crtc;

	exynos_migov_register_vsync_cnt(get_frame_vsync_cnt);
	exynos_migov_register_frame_cnt(get_ems_frame_cnt);
	exynos_migov_register_fence_cnt(get_ems_fence_cnt);
	exynos_gmc_register_frame_cnt(get_ems_frame_cnt);

	pr_info("%s[%d]: migov supported\n", crtc->name, crtc->index);

	return migov;
}
