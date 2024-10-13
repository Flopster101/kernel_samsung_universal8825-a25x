/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Display Quality Enhancer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_HDR_H__
#define __EXYNOS_DRM_HDR_H__

#include <linux/spinlock.h>
#include <hdr_cal.h>
#include <exynos_drm_plane.h>

struct exynos_hdr;

struct exynos_hdr_funcs {
	void (*dump)(struct exynos_hdr *hdr);
	void (*prepare)(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state);
	void (*update)(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state);
	void (*disable)(struct exynos_hdr *hdr);
};

enum hdr_state {
	HDR_STATE_DISABLE = 0,
	HDR_STATE_ENABLE,
};

#define MAX_HDR_CONTEXT 3 // 3 ctx buffer per layer
struct hdr_context {
	char *data;
};

struct exynos_hdr {
	u32 id;
	struct device *dev;
	struct dpp_device *dpp;

	struct hdr_regs	regs;
	size_t reg_size;

	enum hdr_state state;
	struct mutex lock;
	wait_queue_head_t wait_update;

	int64_t hdr_fd;
	struct dma_buf	*dma_buf;
	u32 *dma_vbuf;

	struct hdr_context ctx[MAX_HDR_CONTEXT];
	atomic_t ctx_no;

	const struct exynos_hdr_funcs *funcs;
	bool initialized;
#ifdef CONFIG_DRM_MCD_HDR
	bool enable;
#endif
};

void exynos_hdr_dump(struct exynos_hdr *hdr);
void exynos_hdr_prepare(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state);
void exynos_hdr_update(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state);
void exynos_hdr_disable(struct exynos_hdr *hdr);
struct exynos_hdr *exynos_hdr_register(struct dpp_device *dpp);

#endif /* __EXYNOS_DRM_HDR_H__ */
