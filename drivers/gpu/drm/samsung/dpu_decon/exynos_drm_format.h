/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Format Header file for Exynos DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_FORMAT_H__
#define __EXYNOS_FORMAT_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/videodev2_exynos_media.h>

#define DPU_UNDEF_BITS_DEPTH		0x0
#define has_all_bits(bits, mask)	(((bits) & (mask)) == (bits))

enum dpu_colorspace {
	DPU_COLORSPACE_RGB,
	DPU_COLORSPACE_YUV420,
	DPU_COLORSPACE_YUV422,
};

enum format_comp_type {
	FMT_COMP_NONE = 0,
	FMT_COMP_AFBC,
	FMT_COMP_SAJC,
	FMT_COMP_SBWC,		/* lossless */
	FMT_COMP_SBWCL,		/* lossy */
};

struct dpu_fmt {
	const char *name;
	u32 fmt;		   /* user-interfaced color format */
	u32 dma_fmt;		   /* applied color format to DPU_DMA(In) */
	u32 dpp_fmt;		   /* applied color format to DPP(Out) */
	u8 bpp;			   /* bits per pixel */
	u8 padding;		   /* padding bits per pixel */
	u8 bpc;			   /* bits per each color component */
	u8 num_planes;		   /* plane(s) count of color format */
	u8 len_alpha;		   /* length of alpha bits */
	u32 comp_mask;		   /* mask for supported comp type */
	enum dpu_colorspace cs;
};

/* format */
#define IS_YUV420(f)		((f)->cs == DPU_COLORSPACE_YUV420)
#define IS_YUV422(f)		((f)->cs == DPU_COLORSPACE_YUV422)
#define IS_YUV(f)		(((f)->cs == DPU_COLORSPACE_YUV420) ||	\
				((f)->cs == DPU_COLORSPACE_YUV422))
#define IS_YUV10(f)		(IS_YUV(f) && ((f)->bpc == 10))
#define IS_RGB(f)		((f)->cs == DPU_COLORSPACE_RGB)
#define IS_RGB32(f)	\
	(((f)->cs == DPU_COLORSPACE_RGB) && (((f)->bpp + (f)->padding) == 32))
#define IS_10BPC(f)		((f)->bpc == 10)
#define IS_OPAQUE(f)		((f)->len_alpha == 0)

#define Y_SIZE_8P2(w, h)	(NV12N_10B_Y_8B_SIZE(w, h) +		\
					NV12N_10B_Y_2B_SIZE(w, h))
#define UV_SIZE_8P2(w, h)	(NV12N_10B_CBCR_8B_SIZE(w, h) +		\
					NV12N_10B_CBCR_2B_SIZE(w, h))
#define Y_SIZE_SBWC_8B(w, h)	(SBWC_8B_Y_SIZE(w, h) +			\
					SBWC_8B_Y_HEADER_SIZE(w, h))
#define UV_SIZE_SBWC_8B(w, h)	(SBWC_8B_CBCR_SIZE(w, h) +		\
					SBWC_8B_CBCR_HEADER_SIZE(w, h))
#define Y_SIZE_SBWC_10B(w, h)	(SBWC_10B_Y_SIZE(w, h) +		\
					SBWC_10B_Y_HEADER_SIZE(w, h))
#define UV_SIZE_SBWC_10B(w, h)	(SBWC_10B_CBCR_SIZE(w, h) +		\
					SBWC_10B_CBCR_HEADER_SIZE(w, h))

#define Y_SIZE_SBWC(w, h, bpc)	((bpc) ? Y_SIZE_SBWC_10B(w, h) :	\
					Y_SIZE_SBWC_8B(w, h))
#define UV_SIZE_SBWC(w, h, bpc)	((bpc) ? UV_SIZE_SBWC_10B(w, h) :	\
					UV_SIZE_SBWC_8B(w, h))
#define Y_PL_SIZE_SBWC(w, h, bpc)	((bpc) ? SBWC_10B_Y_SIZE(w, h) :	\
						SBWC_8B_Y_SIZE(w, h))
#define UV_PL_SIZE_SBWC(w, h, bpc)	((bpc) ? SBWC_10B_CBCR_SIZE(w, h) :	\
						SBWC_8B_CBCR_SIZE(w, h))

#define HD_STRIDE_SIZE_SBWC(w)		(SBWC_HEADER_STRIDE(w))
#define PL_STRIDE_SIZE_SBWC(w, bpc)	((bpc) ? SBWC_10B_STRIDE(w) :		\
						SBWC_8B_STRIDE(w))

#define Y_SIZE_SBWCL(w, h, r, bpc)	((bpc) ? SBWCL_10B_Y_SIZE(w, h, r) :	\
					SBWCL_8B_Y_SIZE(w, h, r))
#define UV_SIZE_SBWCL(w, h, r, bpc)	((bpc) ? SBWCL_10B_CBCR_SIZE(w, h, r) :	\
					SBWCL_8B_CBCR_SIZE(w, h, r))

#define PL_STRIDE_SIZE_SBWCL(w, r, bpc)	((bpc) ? SBWCL_10B_STRIDE(w, r) :	\
						SBWCL_8B_STRIDE(w, r))

const struct dpu_fmt *dpu_find_fmt_info(u32 fmt);

#endif /* __EXYNOS_FORMAT_H__ */
