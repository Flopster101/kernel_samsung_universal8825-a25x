/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Authors:
 *	Hwangjae Lee <hj-yo.lee@samsung.com>
 *
 * Headef file for DPU FREQ HOP.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_FREQ_HOP_H__
#define __EXYNOS_DRM_FREQ_HOP_H__

#include <linux/types.h>

struct dsim_device;
struct exynos_drm_crtc;
void dpu_init_freq_hop(struct dsim_device *dsim);
void dpu_freq_hop_debugfs(struct exynos_drm_crtc *exynos_crtc);

struct dpu_freq_hop_ops {
	void (*set_freq_hop)(struct exynos_drm_crtc *exynos_crtc, bool en);
	void (*update_freq_hop)(struct exynos_drm_crtc *exynos_crtc);
};
#endif /* __EXYNOS_DRM_FREQ_HOP_H__ */
