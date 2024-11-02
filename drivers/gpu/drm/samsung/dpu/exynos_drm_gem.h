/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 * Authors:
 *	Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __EXYNOS_DRM_GEM__
#define __EXYNOS_DRM_GEM__

#include <drm/drm_gem.h>
#include <drm/drm_file.h>
#include <drm/drm_device.h>
#include <drm/drm_mode.h>
#include <linux/dma-buf.h>

#define EXYNOS_DRM_GEM_FLAG_COLORMAP	BIT(0)
#define EXYNOS_DRM_GEM_FLAG_DUMB_BUF	BIT(1)

struct exynos_drm_gem {
	struct drm_gem_object base;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	unsigned int flags;
#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
	void *vaddr;
	int acq_fence;
	u64 checksum64;
#endif
};

int exynos_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args);
int exynos_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
int exynos_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset);
void exynos_drm_gem_free_object(struct drm_gem_object *obj);
struct exynos_drm_gem *exynos_drm_gem_alloc(struct drm_device *dev,
					    size_t size, unsigned int flags);
struct drm_gem_object *
exynos_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt);

int exynos_drm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle);
struct drm_gem_object *exynos_drm_gem_prime_import(struct drm_device *dev,
						   struct dma_buf *dma_buf);
#define to_exynos_gem(x)    container_of(x, struct exynos_drm_gem, base)

void exynos_drm_gem_timeout_detector(struct timer_list *arg);
#endif /* __EXYNOS_DRM_GEM__ */
