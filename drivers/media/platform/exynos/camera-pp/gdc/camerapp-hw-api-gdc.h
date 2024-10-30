/*
 * Samsung EXYNOS CAMERA PostProcessing driver
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_HW_API_GDC_H
#define CAMERAPP_HW_API_GDC_H

#include "camerapp-sfr-api-common.h"
#include "camerapp-gdc.h"

#define GDC_SETFILE_VERSION	0x12345678

#define DS_FRACTION_BITS		8
#define DS_SHIFTER_MAX			7
#define MAX_OUTPUT_SCALER_RATIO	2

#define GDC_CFG_FMT_YCBCR420_2P	(0 << 0)
#define GDC_CFG_FMT_YUYV		(0xa << 0)
#define GDC_CFG_FMT_UYVY		(0xb << 0)
#define GDC_CFG_FMT_YVYU		(9 << 0)
#define GDC_CFG_FMT_YCBCR422_2P	(2 << 0)
#define GDC_CFG_FMT_YCBCR444_2P	(3 << 0)
#define GDC_CFG_FMT_RGB565		(4 << 0)
#define GDC_CFG_FMT_ARGB1555		(5 << 0)
#define GDC_CFG_FMT_ARGB4444		(0xc << 0)
#define GDC_CFG_FMT_ARGB8888		(6 << 0)
#define GDC_CFG_FMT_RGBA8888		(0xe << 0)
#define GDC_CFG_FMT_P_ARGB8888	(7 << 0)
#define GDC_CFG_FMT_L8A8		(0xd << 0)
#define GDC_CFG_FMT_L8		(0xf << 0)
#define GDC_CFG_FMT_YCRCB420_2P	(0x10 << 0)
#define GDC_CFG_FMT_YCRCB422_2P	(0x12 << 0)
#define GDC_CFG_FMT_YCRCB444_2P	(0x13 << 0)
#define GDC_CFG_FMT_YCBCR420_3P	(0x14 << 0)
#define GDC_CFG_FMT_YCBCR422_3P	(0x16 << 0)
#define GDC_CFG_FMT_YCBCR444_3P	(0x17 << 0)
#define GDC_CFG_FMT_P010_16B_2P	(0x18 << 0)
#define GDC_CFG_FMT_P210_16B_2P	(0x19 << 0)
#define GDC_CFG_FMT_YCRCB422_8P2_2P	(0x1a << 0)
#define GDC_CFG_FMT_YCRCB420_8P2_2P	(0x1b << 0)
#define GDC_CFG_FMT_NV12M_SBWC_LOSSY_8B	(0x1c << 0)
#define GDC_CFG_FMT_NV12M_SBWC_LOSSY_10B	(0x1d << 0)

void camerapp_hw_gdc_start(void __iomem *base_addr);
void camerapp_hw_gdc_stop(void __iomem *base_addr);
u32 camerapp_hw_gdc_sw_reset(void __iomem *base_addr);
void camerapp_hw_gdc_update_param(void __iomem *base_addr, struct gdc_dev *gdc);
void camerapp_hw_gdc_status_read(void __iomem *base_addr);
void camerapp_gdc_sfr_dump(void __iomem *base_addr);
u32 camerapp_hw_gdc_get_intr_status_and_clear(void __iomem *base_addr);
u32 camerapp_hw_gdc_get_int_frame_start(void);
u32 camerapp_hw_gdc_get_int_frame_end(void);
void camerapp_gdc_grid_setting(struct gdc_dev *gdc);
void camerapp_hw_gdc_votf_enable(void __iomem *base_addr, u8 rw);
u32 camerapp_hw_gdc_get_ver(void __iomem *base_addr);
u32 camerapp_hw_get_sbwc_constraint(struct gdc_frame *frame, u32 type);
int camerapp_hw_get_comp_rate(struct gdc_dev *gdc, u32 pixfmt);
const struct gdc_variant *camerapp_hw_gdc_get_size_constraints(void __iomem *base_addr);

#endif
