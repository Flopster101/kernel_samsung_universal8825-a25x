// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU format file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_print.h>
#include <drm/drm_fourcc.h>

#include <exynos_drm_modifier.h>
#include <exynos_drm_format.h>
#include <regs-dpp.h>
#include <dpp_cal.h>

static const struct dpu_fmt dpu_formats_list[] = {
	{
		.name = "ARGB8888",
		.fmt = DRM_FORMAT_ARGB8888,
		.dma_fmt = IDMA_IMG_FORMAT_ARGB8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 32,
		.padding = 0,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 8,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "ABGR8888",
		.fmt = DRM_FORMAT_ABGR8888,
		.dma_fmt = IDMA_IMG_FORMAT_ABGR8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 32,
		.padding = 0,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 8,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "RGBA8888",
		.fmt = DRM_FORMAT_RGBA8888,
		.dma_fmt = IDMA_IMG_FORMAT_RGBA8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 32,
		.padding = 0,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 8,
		.comp_mask = BIT(FMT_COMP_AFBC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "BGRA8888",
		.fmt = DRM_FORMAT_BGRA8888,
		.dma_fmt = IDMA_IMG_FORMAT_BGRA8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 32,
		.padding = 0,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 8,
		.comp_mask = BIT(FMT_COMP_AFBC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "XRGB8888",
		.fmt = DRM_FORMAT_XRGB8888,
		.dma_fmt = IDMA_IMG_FORMAT_XRGB8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 24,
		.padding = 8,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "XBGR8888",
		.fmt = DRM_FORMAT_XBGR8888,
		.dma_fmt = IDMA_IMG_FORMAT_XBGR8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 24,
		.padding = 8,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "RGBX8888",
		.fmt = DRM_FORMAT_RGBX8888,
		.dma_fmt = IDMA_IMG_FORMAT_RGBX8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 24,
		.padding = 8,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "BGRX8888",
		.fmt = DRM_FORMAT_BGRX8888,
		.dma_fmt = IDMA_IMG_FORMAT_BGRX8888,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 24,
		.padding = 8,
		.bpc = 8,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "RGB565",
		.fmt = DRM_FORMAT_RGB565,
		.dma_fmt = IDMA_IMG_FORMAT_RGB565,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 16,
		.padding = 0,
		.bpc = (u8)DPU_UNDEF_BITS_DEPTH,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "BGR565",
		.fmt = DRM_FORMAT_BGR565,
		.dma_fmt = IDMA_IMG_FORMAT_BGR565,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8888,
		.bpp = 16,
		.padding = 0,
		.bpc = (u8)DPU_UNDEF_BITS_DEPTH,
		.num_planes = 1,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_AFBC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "ARGB2101010",
		.fmt = DRM_FORMAT_ARGB2101010,
		.dma_fmt = IDMA_IMG_FORMAT_ARGB2101010,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 32,
		.padding = 0,
		.bpc = 10,
		.num_planes = 1,
		.len_alpha = 2,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "ABGR2101010",
		.fmt = DRM_FORMAT_ABGR2101010,
		.dma_fmt = IDMA_IMG_FORMAT_ABGR2101010,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 32,
		.padding = 0,
		.bpc = 10,
		.num_planes = 1,
		.len_alpha = 2,
		.comp_mask = BIT(FMT_COMP_AFBC) | BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "RGBA1010102",
		.fmt = DRM_FORMAT_RGBA1010102,
		.dma_fmt = IDMA_IMG_FORMAT_RGBA1010102,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 32,
		.padding = 0,
		.bpc = 10,
		.num_planes = 1,
		.len_alpha = 2,
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "BGRA1010102",
		.fmt = DRM_FORMAT_BGRA1010102,
		.dma_fmt = IDMA_IMG_FORMAT_BGRA1010102,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 32,
		.padding = 0,
		.bpc = 10,
		.num_planes = 1,
		.len_alpha = 2,
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "NV12",
		.fmt = DRM_FORMAT_NV12,
		.dma_fmt = IDMA_IMG_FORMAT_NV12,
		.dpp_fmt = DPP_IMG_FORMAT_YUV420_8P,
		.bpp = 12,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_SBWC) | BIT(FMT_COMP_SBWCL),
		.cs = DPU_COLORSPACE_YUV420,
	}, {
		.name = "NV21",
		.fmt = DRM_FORMAT_NV21,
		.dma_fmt = IDMA_IMG_FORMAT_NV21,
		.dpp_fmt = DPP_IMG_FORMAT_YUV420_8P,
		.bpp = 12,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_SBWC) | BIT(FMT_COMP_SBWCL),
		.cs = DPU_COLORSPACE_YUV420,
	}, {
		.name = "P010",
		.fmt = DRM_FORMAT_P010,
		.dma_fmt = IDMA_IMG_FORMAT_YUV420_P010,
		.dpp_fmt = DPP_IMG_FORMAT_YUV420_P010,
		.bpp = 15,
		.padding = 9,
		.bpc = 10,
		.num_planes = 2,
		.len_alpha = 0,
		.comp_mask = BIT(FMT_COMP_SBWC) | BIT(FMT_COMP_SBWCL),
		.cs = DPU_COLORSPACE_YUV420,
#if IS_ENABLED(CONFIG_SOC_S5E9925) && !IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	}, {
		.name = "ARGB16161616",
		.fmt = DRM_FORMAT_ARGB16161616F,
		.dma_fmt = IDMA_IMG_FORMAT_ARGB_FP16,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 64,
		.padding = 0,
		.bpc = 16,
		.num_planes = 1,
		.len_alpha = 16,
		.cs = DPU_COLORSPACE_RGB,
	}, {
		.name = "ABGR16161616",
		.fmt = DRM_FORMAT_ABGR16161616F,
		.dma_fmt = IDMA_IMG_FORMAT_ABGR_FP16,
		.dpp_fmt = DPP_IMG_FORMAT_ARGB8101010,
		.bpp = 64,
		.padding = 0,
		.bpc = 16,
		.num_planes = 1,
		.len_alpha = 16,
		.comp_mask = BIT(FMT_COMP_SAJC),
		.cs = DPU_COLORSPACE_RGB,
#endif
	},
};

const struct dpu_fmt *dpu_find_fmt_info(u32 fmt)
{
	int i;
	struct drm_format_name_buf format_name;

	for (i = 0; i < ARRAY_SIZE(dpu_formats_list); i++)
		if (dpu_formats_list[i].fmt == fmt)
			return &dpu_formats_list[i];

	DRM_INFO("%s: can't find format(%s) in supported format list\n",
			__func__, drm_get_format_name(fmt, &format_name));

	return NULL;
}
