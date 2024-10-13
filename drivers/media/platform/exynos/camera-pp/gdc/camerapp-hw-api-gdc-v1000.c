// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing GDC driver
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "camerapp-hw-api-gdc.h"
#include "camerapp-hw-reg-gdc-v1000.h"

static const struct gdc_variant gdc_variant_evt0[] = {
	{
		.limit_input = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 8192,
		},
		.version		= 0,
	},
};

static const struct gdc_variant gdc_variant_evt1[] = {
	{
		.limit_input = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 12288,
		},
		.limit_output = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 12288,
		},
		.version		= 0,
	},
};

const struct gdc_variant *camerapp_hw_gdc_get_size_constraints(void __iomem *base_addr)
{
	const struct gdc_variant *constraints;
	u32 version = camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_IP_VERSION]);

	if (version == IP_VERSION_EVT0)
		constraints = gdc_variant_evt0;
	else
		constraints = gdc_variant_evt1;

	return constraints;
}

u32 camerapp_hw_gdc_get_ver(void __iomem *base_addr)
{
	return camerapp_sfr_get_reg(base_addr, &gdc_regs[GDC_R_IP_VERSION]);
}

void camerapp_hw_gdc_start(void __iomem *base_addr)
{
	/* oneshot */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_ADD_TO_QUEUE_0],
		&gdc_fields[GDC_F_CMDQ_ADD_TO_QUEUE_0], 0x1);
}

void camerapp_hw_gdc_stop(void __iomem *base_addr)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_ENABLE],
		&gdc_fields[GDC_F_CMDQ_ENABLE], 0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_IP_PROCESSING],
		&gdc_fields[GDC_F_IP_PROCESSING], 0);
}

u32 camerapp_hw_gdc_sw_reset(void __iomem *base_addr)
{
	u32 reset_count = 0;

	/* request to gdc hw */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_SW_RESET], &gdc_fields[GDC_F_SW_RESET], 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (camerapp_sfr_get_field(base_addr, &gdc_regs[GDC_R_SW_RESET],
		&gdc_fields[GDC_F_SW_RESET]) != 0);

	return 0;
}

void camerapp_hw_gdc_clear_intr_all(void __iomem *base_addr)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_INT_REQ_INT0_CLEAR],
		&gdc_fields[GDC_F_INT_REQ_INT0_CLEAR], GDC_INT0_EN);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_INT_REQ_INT1_CLEAR],
		&gdc_fields[GDC_F_INT_REQ_INT1_CLEAR], GDC_INT1_EN);
}

u32 camerapp_hw_gdc_get_intr_status_and_clear(void __iomem *base_addr)
{
	u32 int0_status, int1_status;

	int0_status = camerapp_sfr_get_reg(base_addr,
		&gdc_regs[GDC_R_INT_REQ_INT0]);
	camerapp_sfr_set_reg(base_addr,
		&gdc_regs[GDC_R_INT_REQ_INT0_CLEAR], int0_status);

	int1_status = camerapp_sfr_get_reg(base_addr,
		&gdc_regs[GDC_R_INT_REQ_INT1]);
	camerapp_sfr_set_reg(base_addr,
		&gdc_regs[GDC_R_INT_REQ_INT1_CLEAR], int1_status);

	gdc_dbg("int0(0x%x), int1(0x%x)\n", int0_status, int1_status);

	return int0_status;
}

u32 camerapp_hw_gdc_get_int_frame_start(void)
{
	return GDC_INT_FRAME_START;
}

u32 camerapp_hw_gdc_get_int_frame_end(void)
{
	return GDC_INT_FRAME_END;
}

/* DMA Configuration */
static void camerapp_hw_gdc_set_rdma_enable(void __iomem *base_addr,
	u32 en_y, u32 en_uv)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAY_EN],
		&gdc_fields[GDC_F_YUV_RDMAY_EN], en_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAUV_EN],
		&gdc_fields[GDC_F_YUV_RDMAUV_EN], en_uv);
}

static void camerapp_hw_gdc_set_wdma_enable(void __iomem *base_addr,
	u32 enable)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_WDMA_EN],
		&gdc_fields[GDC_F_YUV_WDMA_EN], enable);
}

static void camerapp_hw_gdc_set_rdma_sbwc_enable(void __iomem *base_addr,
	u32 enable)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_RDMAY_SBWC_EN], enable);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_RDMAUV_SBWC_EN], enable);
}

static void camerapp_hw_gdc_set_wdma_sbwc_enable(void __iomem *base_addr,
	u32 enable)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_WDMA_SBWC_EN], enable);
}

static void camerapp_hw_gdc_set_rdma_sbwc_64b_align(void __iomem *base_addr,
	u32 align_64b)
{
	/*
	 * align_64b
	 * 0 : 32byte align, 1 : 64byte align
	 */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_RDMAY_SBWC_64B_ALIGN], align_64b);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_RDMAUV_SBWC_64B_ALIGN], align_64b);
}

static void camerapp_hw_gdc_set_wdma_sbwc_64b_align(void __iomem *base_addr,
	u32 align_64b)
{
	/*
	 * align_64b
	 * 0 : 32byte align, 1 : 64byte align
	 */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_COMP_CONTROL],
		&gdc_fields[GDC_F_YUV_WDMA_SBWC_64B_ALIGN], align_64b);
}

static void camerapp_hw_gdc_set_rdma_data_format(void __iomem *base_addr,
	u32 yuv_format)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAY_DATA_FORMAT],
		&gdc_fields[GDC_F_YUV_RDMAY_DATA_FORMAT_YUV], yuv_format);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAUV_DATA_FORMAT],
		&gdc_fields[GDC_F_YUV_RDMAUV_DATA_FORMAT_YUV], yuv_format);
}

static void camerapp_hw_gdc_set_wdma_data_format(void __iomem *base_addr,
	u32 yuv_format)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_WDMA_DATA_FORMAT],
		&gdc_fields[GDC_F_YUV_WDMA_DATA_FORMAT_YUV], yuv_format);
}

static void camerapp_hw_gdc_set_wdma_mono_mode(void __iomem *base_addr,
	u32 mode)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_WDMA_MONO_MODE],
		&gdc_fields[GDC_F_YUV_WDMA_MONO_MODE], mode);
}

static void camerapp_hw_gdc_set_rdma_comp_rate(void __iomem *base_addr,
	u32 rate)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_COMP_LOSSY_BYTE32NUM],
		&gdc_fields[GDC_F_YUV_RDMAY_COMP_LOSSY_BYTE32NUM], rate);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_COMP_LOSSY_BYTE32NUM],
		&gdc_fields[GDC_F_YUV_RDMAUV_COMP_LOSSY_BYTE32NUM], rate);
}

static void camerapp_hw_gdc_set_wdma_comp_rate(void __iomem *base_addr,
	u32 rate)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_COMP_LOSSY_BYTE32NUM],
		&gdc_fields[GDC_R_YUV_WDMA_COMP_LOSSY_BYTE32NUM], rate);
}

static void camerapp_hw_gdc_set_rdma_img_size(void __iomem *base_addr,
	u32 width, u32 height)
{
	u32 yuv_format;

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAY_WIDTH],
		&gdc_fields[GDC_F_YUV_RDMAY_WIDTH], width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAY_HEIGHT],
		&gdc_fields[GDC_F_YUV_RDMAY_HEIGHT], height);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAUV_WIDTH],
		&gdc_fields[GDC_F_YUV_RDMAUV_WIDTH], width);

	yuv_format = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_YUV_FORMAT]);

	if (yuv_format == GDC_YUV420)
		height >>= 1;

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_RDMAUV_HEIGHT],
		&gdc_fields[GDC_F_YUV_RDMAUV_HEIGHT], height);
}

static void camerapp_hw_gdc_set_wdma_img_size(void __iomem *base_addr,
	u32 width, u32 height)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_WDMA_WIDTH],
		&gdc_fields[GDC_F_YUV_WDMA_WIDTH], width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_WDMA_HEIGHT],
		&gdc_fields[GDC_F_YUV_WDMA_HEIGHT], height);
}

static void camerapp_hw_gdc_set_rdma_stride(void __iomem *base_addr,
	u32 y_stride, u32 uv_stride)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_IMG_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_RDMAY_IMG_STRIDE_1P], y_stride);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_IMG_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_RDMAUV_IMG_STRIDE_1P], uv_stride);
}

static void camerapp_hw_gdc_set_rdma_header_stride(void __iomem *base_addr,
	u32 y_2bit_stride, u32 uv_2bit_stride)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_HEADER_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_RDMAY_HEADER_STRIDE_1P],
		y_2bit_stride);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_HEADER_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_RDMAUV_HEADER_STRIDE_1P],
		uv_2bit_stride);
}

static void camerapp_hw_gdc_set_wdma_stride(void __iomem *base_addr,
	u32 y_stride, u32 uv_stride)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_IMG_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_WDMA_IMG_STRIDE_1P], y_stride);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_IMG_STRIDE_2P],
		&gdc_fields[GDC_F_YUV_WDMA_IMG_STRIDE_2P], uv_stride);
}

static void camerapp_hw_gdc_set_wdma_header_stride(void __iomem *base_addr,
	u32 y_2bit_stride, u32 uv_2bit_stride)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_HEADER_STRIDE_1P],
		&gdc_fields[GDC_F_YUV_WDMA_HEADER_STRIDE_1P], y_2bit_stride);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_HEADER_STRIDE_2P],
		&gdc_fields[GDC_F_YUV_WDMA_HEADER_STRIDE_2P], uv_2bit_stride);
}

static void camerapp_hw_gdc_set_rdma_addr(void __iomem *base_addr,
	u32 y_addr, u32 uv_addr)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0],
		y_addr);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0],
		uv_addr);
}

static void camerapp_hw_gdc_set_wdma_addr(void __iomem *base_addr,
	u32 y_addr, u32 uv_addr)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0],
		y_addr);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0],
		uv_addr);
}

static void camerapp_hw_gdc_set_rdma_header_addr(void __iomem *base_addr,
	u32 y_addr, u32 uv_addr)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0],
		y_addr);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0],
		uv_addr);
}

static void camerapp_hw_gdc_set_wdma_header_addr(void __iomem *base_addr,
	u32 y_addr, u32 uv_addr)
{
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0],
		y_addr);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0],
		&gdc_fields[GDC_F_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0],
		uv_addr);
}

/* Core Configuration*/
static void camerapp_hw_gdc_update_grid_param(void __iomem *base_addr,
	struct gdc_crop_param *crop_param)
{
	u32 i, j;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = 0x2000;
	u32 sfr_start_y = 0x3200;

	if (crop_param->use_calculated_grid) {
		for (i = 0; i < GRID_Y_SIZE; i++) {
			for (j = 0; j < GRID_X_SIZE; j++) {
				u32 cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);

				writel((u32)crop_param->calculated_grid_x[i][j],
					base_addr + sfr_start_x + cal_sfr_offset);
				writel((u32)crop_param->calculated_grid_y[i][j],
					base_addr + sfr_start_y + cal_sfr_offset);
			}
		}
	} else {
		for (i = 0; i < GRID_Y_SIZE; i++) {
			for (j = 0; j < GRID_X_SIZE; j++) {
				u32 cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);

				writel(0, base_addr + sfr_start_x + cal_sfr_offset);
				writel(0, base_addr + sfr_start_y + cal_sfr_offset);
			}
		}
	}

}

static void camerapp_hw_gdc_set_initialization(void __iomem *base_addr)
{
	/* 1. ip processing */
	/* 2. IP in/out configuration ... check it later */
	/* 3. interrupt enable */
	/* 4. [optional] security, performance monitor*/
	/* 5. [optional] for corex ... check it later*/
	/* 6. cmdq_enable */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_IP_PROCESSING],
		&gdc_fields[GDC_F_IP_PROCESSING], 0x1);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_C_LOADER_ENABLE],
		&gdc_fields[GDC_F_C_LOADER_ENABLE], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_STAT_RDMACLOADER_EN],
		&gdc_fields[GDC_F_STAT_RDMACLOADER_EN], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_INT_REQ_INT0_ENABLE],
		&gdc_fields[GDC_F_INT_REQ_INT0_ENABLE], GDC_INT0_EN);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_INT_REQ_INT1_ENABLE],
		&gdc_fields[GDC_F_INT_REQ_INT1_ENABLE], GDC_INT1_EN);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_ENABLE],
		&gdc_fields[GDC_F_CMDQ_ENABLE], 0x1);
}

static void camerapp_hw_gdc_set_mode(void __iomem *base_addr, u32 mode)
{
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_H],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_BASE_ADDR], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_M],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_HEADER_NUM], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_M],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_SETTING_MODE], mode);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_M],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_HOLD_MODE], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_M],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_FRAME_ID], 0x0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_L],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE], 0xFF);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_CMDQ_QUE_CMD_L],
		&gdc_fields[GDC_F_CMDQ_QUE_CMD_FRO_INDEX], 0x0);
}

static void camerapp_hw_gdc_update_scale_parameters(void __iomem *base_addr,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame,
	struct gdc_crop_param *crop_param)
{
	u32 gdc_input_width;
	u32 gdc_input_height;
	u32 gdc_crop_offset_x;
	u32 gdc_crop_offset_y;
	u32 gdc_crop_width;
	u32 gdc_crop_height;
	u32 gdc_scale_shifter_x;
	u32 gdc_scale_shifter_y;
	u32 gdc_scale_x;
	u32 gdc_scale_y;
	u32 gdc_scale_width;
	u32 gdc_scale_height;

	u32 gdc_inv_scale_x;
	u32 gdc_inv_scale_y;
	u32 gdc_output_offset_x;
	u32 gdc_output_offset_y;
	u32 gdc_output_width;
	u32 gdc_output_height;

	u32 scaleShifterX;
	u32 scaleShifterY;
	u32 imagewidth;
	u32 imageheight;
	u32 out_scaled_width;
	u32 out_scaled_height;

	/* GDC original input size */
	gdc_input_width = s_frame->width;
	gdc_input_height = s_frame->height;
	gdc_output_width = d_frame->width;
	gdc_output_height = d_frame->height;

	/* Meaningful only when out_crop_bypass = 0, x <= (gdc_crop_width - gdc_image_active_width) */
	if (!IS_ALIGNED(gdc_output_width, GDC_WIDTH_ALIGN) ||
		!IS_ALIGNED(gdc_output_height, GDC_HEIGHT_ALIGN)) {
		gdc_dbg("gdc output width(%d) is not (%d)align or height(%d) is not (%d)align.\n",
				gdc_output_width, GDC_WIDTH_ALIGN,
				gdc_output_height, GDC_HEIGHT_ALIGN);
		gdc_output_width = ALIGN_DOWN(gdc_output_width, GDC_WIDTH_ALIGN);
		gdc_output_height = ALIGN_DOWN(gdc_output_height, GDC_HEIGHT_ALIGN);
	}


	if (crop_param->is_grid_mode) {
		/* output */
		gdc_scale_width = gdc_output_width;
		gdc_scale_height = gdc_output_height;
	} else {
		/* input */
		gdc_scale_width = gdc_input_width;
		gdc_scale_height = gdc_input_height;
	}

	/* GDC input crop size from original input size */
	if (crop_param->is_bypass_mode) {
		gdc_crop_width = gdc_input_width;
		gdc_crop_height = gdc_input_height;
		gdc_crop_offset_x = 0;
		gdc_crop_offset_y = 0;

	} else {
		gdc_crop_width = crop_param->crop_width;
		gdc_crop_height = crop_param->crop_height;
		gdc_crop_offset_x = crop_param->crop_start_x;
		gdc_crop_offset_y = crop_param->crop_start_y;
	}

	/* Real to virtual grid scaling factor shifters */
	scaleShifterX = DS_SHIFTER_MAX;
	imagewidth = gdc_scale_width << 1;
	while ((imagewidth <= MAX_VIRTUAL_GRID_X) && (scaleShifterX > 0)) {
		imagewidth <<= 1;
		scaleShifterX--;
	}
	gdc_scale_shifter_x = scaleShifterX;

	scaleShifterY  = DS_SHIFTER_MAX;
	imageheight = gdc_scale_height << 1;
	while ((imageheight <= MAX_VIRTUAL_GRID_Y) && (scaleShifterY > 0)) {
		imageheight <<= 1;
		scaleShifterY--;
	}
	gdc_scale_shifter_y = scaleShifterY;

	/* Real to virtual grid scaling factors */
	gdc_scale_x = MIN(65535,
		((MAX_VIRTUAL_GRID_X << (DS_FRACTION_BITS + gdc_scale_shifter_x)) + gdc_scale_width / 2) / gdc_scale_width);
	gdc_scale_y = MIN(65535,
		((MAX_VIRTUAL_GRID_Y << (DS_FRACTION_BITS + gdc_scale_shifter_y)) + gdc_scale_height / 2) / gdc_scale_height);

	/* Virtual to real grid scaling factors */
	gdc_inv_scale_x = gdc_input_width;
	gdc_inv_scale_y = ((gdc_input_height << 3) + 3) / 6;

	if ((gdc_crop_width < gdc_output_width) || crop_param->is_bypass_mode)
		gdc_output_offset_x = 0;
	else
		gdc_output_offset_x = ALIGN_DOWN((gdc_crop_width - gdc_output_width) >> 1, GDC_OFFSET_ALIGN);

	if ((gdc_crop_height < gdc_output_height) || crop_param->is_bypass_mode)
		gdc_output_offset_y = 0;
	else
		gdc_output_offset_y = ALIGN_DOWN((gdc_crop_height - gdc_output_height) >> 1, GDC_OFFSET_ALIGN);

	/* if GDC is scaled up : 8192(default) = no scaling, 4096 = 2 times scaling */
	/* now is selected no scaling. => calculation (8192 * in / out) */
	out_scaled_width = 8192;
	out_scaled_height = 8192;
	if (!crop_param->is_bypass_mode) {
		/* Only for scale up */
		if (gdc_crop_width * MAX_OUTPUT_SCALER_RATIO < gdc_output_width) {
			gdc_dbg("gdc_out_scale_x exceeds max ratio(%d). in_crop(%dx%d) out(%dx%d)\n",
					MAX_OUTPUT_SCALER_RATIO,
					gdc_crop_width, gdc_crop_height,
					gdc_output_width, gdc_output_height);
			out_scaled_width = 8192 / MAX_OUTPUT_SCALER_RATIO;
		} else if (gdc_crop_width < gdc_output_width) {
			out_scaled_width = 8192 * gdc_crop_width / gdc_output_width;
		}

		/* Only for scale up */
		if (gdc_crop_height * MAX_OUTPUT_SCALER_RATIO < gdc_output_height) {
			gdc_dbg("gdc_out_scale_y exceeds max ratio(%d). in_crop(%dx%d) out(%dx%d)\n",
					MAX_OUTPUT_SCALER_RATIO,
					gdc_crop_width, gdc_crop_height,
					gdc_output_width, gdc_output_height);
			out_scaled_height = 8192 / MAX_OUTPUT_SCALER_RATIO;
		} else if (gdc_crop_height < gdc_output_height) {
			out_scaled_height = 8192 * gdc_crop_height / gdc_output_height;
		}
	}

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_CONFIG],
		&gdc_fields[GDC_F_YUV_GDC_MIRROR_X], 0);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_CONFIG],
		&gdc_fields[GDC_F_YUV_GDC_MIRROR_Y], 0);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_ORG_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_ORG_HEIGHT], gdc_input_height);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_ORG_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_ORG_WIDTH], gdc_input_width);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_CROP_START],
		&gdc_fields[GDC_F_YUV_GDC_CROP_START_X], gdc_crop_offset_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_CROP_START],
		&gdc_fields[GDC_F_YUV_GDC_CROP_START_Y], gdc_crop_offset_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_CROP_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_CROP_WIDTH], gdc_crop_width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INPUT_CROP_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_CROP_HEIGHT], gdc_crop_height);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_SCALE_X], gdc_scale_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_SCALE_Y], gdc_scale_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_SCALE_SHIFTER],
		&gdc_fields[GDC_F_YUV_GDC_SCALE_SHIFTER_X], gdc_scale_shifter_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_SCALE_SHIFTER],
		&gdc_fields[GDC_F_YUV_GDC_SCALE_SHIFTER_Y], gdc_scale_shifter_y);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INV_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_INV_SCALE_X], gdc_inv_scale_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_INV_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_INV_SCALE_Y], gdc_inv_scale_y);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_CROP_START],
		&gdc_fields[GDC_F_YUV_GDC_IMAGE_CROP_PRE_X], gdc_output_offset_x);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_CROP_START],
		&gdc_fields[GDC_F_YUV_GDC_IMAGE_CROP_PRE_Y], gdc_output_offset_y);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_CROP_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_IMAGE_ACTIVE_WIDTH], gdc_output_width);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_CROP_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_IMAGE_ACTIVE_HEIGHT], gdc_output_height);

	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_OUT_SCALE_Y], out_scaled_height);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_OUT_SCALE],
		&gdc_fields[GDC_F_YUV_GDC_OUT_SCALE_X], out_scaled_width);

	gdc_dbg("input %dx%d in_crop %d,%d %dx%d -> out_crop %d,%d %dx%d / scale %dx%d\n",
			gdc_input_width, gdc_input_height,
			gdc_crop_offset_x, gdc_crop_offset_y, gdc_crop_width, gdc_crop_height,
			gdc_output_offset_x, gdc_output_offset_y, gdc_output_width, gdc_output_height,
			out_scaled_height, out_scaled_width);
}

static void camerapp_hw_gdc_calculate_stride(struct gdc_frame *frame, u32 dma_width,
	u32 *lum_w_header, u32 *chroma_w_header,
	u32 *lum_w, u32 *chroma_w, u32 bytesperline[])
{
	/* calculate DMA stride size */
	if (frame->extra == COMP) {
		*lum_w_header = (u32)(SBWC_HEADER_STRIDE(dma_width));
		*chroma_w_header = (u32)(SBWC_HEADER_STRIDE(dma_width));
		if (frame->pixel_size == CAMERAPP_PIXEL_SIZE_8BIT) {
			*lum_w = (u32)(SBWC_8B_STRIDE(dma_width));
			*chroma_w = (u32)(SBWC_8B_STRIDE(dma_width));
		} else if (frame->pixel_size == CAMERAPP_PIXEL_SIZE_10BIT) {
			*lum_w = (u32)(SBWC_10B_STRIDE(dma_width));
			*chroma_w = (u32)(SBWC_10B_STRIDE(dma_width));
		}
	} else if (frame->extra == COMP_LOSS) {
		/*
		 * Assume that lossy compression rate is
		 * 8bit 50%, EVT0 : 10bit 60%, EVT1 : 10bit 40%
		 */
		if (frame->pixel_size == CAMERAPP_PIXEL_SIZE_8BIT) {
			*lum_w = (u32)(SBWCL_8B_STRIDE(dma_width, frame->comp_rate));
			*chroma_w = (u32)(SBWCL_8B_STRIDE(dma_width, frame->comp_rate));
		} else if (frame->pixel_size == CAMERAPP_PIXEL_SIZE_10BIT) {
			*lum_w = (u32)(SBWCL_10B_STRIDE(dma_width, frame->comp_rate));
			*chroma_w = (u32)(SBWCL_10B_STRIDE(dma_width, frame->comp_rate));
		}
	} else if ((frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_NV12M_P010)
			|| (frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_NV21M_P010)
			|| (frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_NV16M_P210)
			|| (frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_NV61M_P210)) {
		*lum_w = max(bytesperline[0], ALIGN(dma_width * 2, 16));
		*chroma_w = max(bytesperline[1], ALIGN(dma_width * 2, 16));
	} else {
		*lum_w = max(bytesperline[0], ALIGN(dma_width, 16));
		*chroma_w = max(bytesperline[1], ALIGN(dma_width, 16));
	}
}

static void camerapp_hw_gdc_update_dma_size(void __iomem *base_addr,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame,
	struct gdc_crop_param *crop_param)
{
	u32 input_stride_lum_w = 0, input_stride_chroma_w = 0;
	u32 output_stride_lum_w = 0, output_stride_chroma_w = 0;
	u32 output_stride_lum_w_header = 0, output_stride_chroma_w_header = 0;
	u32 input_stride_lum_w_header = 0, input_stride_chroma_w_header = 0;
	u32 in_dma_width, out_dma_width;
	u32 gdc_input_width, gdc_input_height;
	u32 gdc_output_width, gdc_output_height;
/*
 * supported format
 * - input 8bit : YUV422_2P_8bit, YUV420_2P_8bit
 * - input 10bit : YUV422_2P_10bit, YUV420_2P_10bit, P010(P210), 8+2bit
 * - output 8bit : YUV422_2P_8bit, YUV420_2P_8bit
 * - output 10bit : YUV422_2P_10bit, YUV420_2P_10bit, P010(P210), 8+2bit
 * - output SBWC : YUV_CbCr_422_2P_SBWC_8bit/10bit, YUV_CbCr_420_2P_SBWC_8bit/10bit
 */

	/* GDC input / output image size */
	gdc_input_width = s_frame->width;
	gdc_input_height = s_frame->height;
	gdc_output_width = d_frame->width;
	gdc_output_height = d_frame->height;

	out_dma_width = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_OUT_CROP_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_IMAGE_ACTIVE_WIDTH]);
	in_dma_width = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_INPUT_ORG_SIZE],
		&gdc_fields[GDC_F_YUV_GDC_ORG_WIDTH]);

	camerapp_hw_gdc_calculate_stride(s_frame, in_dma_width,
		&input_stride_lum_w_header, &input_stride_chroma_w_header,
		&input_stride_lum_w, &input_stride_chroma_w,
		crop_param->src_bytesperline);

	camerapp_hw_gdc_calculate_stride(d_frame, out_dma_width,
		&output_stride_lum_w_header, &output_stride_chroma_w_header,
		&output_stride_lum_w, &output_stride_chroma_w,
		crop_param->dst_bytesperline);

/*
 * src_10bit_format == packed10bit (10bit + 10bit + 10bit... no padding)
 * input_stride_lum_w = (u32)(((in_dma_width * 10 + 7) / 8 + 15) / 16 * 16);
 * input_stride_chroma_w = (u32)(((in_dma_width * 10 + 7) / 8 + 15) / 16 * 16);
 */
	gdc_dbg("s_w = %d, lum stride_w = %d, stride_header_w = %d\n",
		s_frame->width, input_stride_lum_w, input_stride_lum_w_header);
	gdc_dbg("s_w = %d, chroma stride_w = %d, stride_header_w = %d\n",
		s_frame->width, input_stride_chroma_w, input_stride_chroma_w_header);
	gdc_dbg("d_w = %d, lum stride_w = %d, stride_header_w = %d\n",
		d_frame->width, output_stride_lum_w, output_stride_lum_w_header);
	gdc_dbg("d_w = %d, chroma stride_w = %d, stride_header_w = %d\n",
		d_frame->width, output_stride_chroma_w, output_stride_chroma_w_header);

	camerapp_hw_gdc_set_wdma_img_size(base_addr, gdc_output_width, gdc_output_height);
	camerapp_hw_gdc_set_rdma_img_size(base_addr, gdc_input_width, gdc_input_height);

	/* about write DMA stride size : It should be aligned with 16 bytes */
	camerapp_hw_gdc_set_wdma_stride(base_addr,
		output_stride_lum_w, output_stride_chroma_w);
	camerapp_hw_gdc_set_wdma_header_stride(base_addr,
		output_stride_lum_w_header, output_stride_chroma_w_header);

	/* about read DMA stride size */
	camerapp_hw_gdc_set_rdma_stride(base_addr,
		input_stride_lum_w, input_stride_chroma_w);
	camerapp_hw_gdc_set_rdma_header_stride(base_addr,
		input_stride_lum_w_header, input_stride_chroma_w_header);
}

static void camerapp_hw_gdc_set_dma_address(void __iomem *base_addr,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 mono_format = (s_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY)
		&& (d_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY);

	camerapp_hw_gdc_set_wdma_enable(base_addr, 1);
	camerapp_hw_gdc_set_rdma_enable(base_addr, 1, !mono_format);

	/* WDMA address */
	camerapp_hw_gdc_set_wdma_addr(base_addr,
		d_frame->addr.y, d_frame->addr.cb);
	camerapp_hw_gdc_set_wdma_header_addr(base_addr,
		d_frame->addr.y_2bit, d_frame->addr.cbcr_2bit);

	/* RDMA address */
	camerapp_hw_gdc_set_rdma_addr(base_addr,
		s_frame->addr.y, s_frame->addr.cb);
	camerapp_hw_gdc_set_rdma_header_addr(base_addr,
		s_frame->addr.y_2bit, s_frame->addr.cbcr_2bit);
}


static int camerapp_hw_gdc_adjust_fmt(u32 pixelformat,
	u32 *yuv_format, u32 *dma_format, u32 *bit_depth_format)
{
	int ret = 0;

	switch (pixelformat) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M_SBWCL_8B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV12M_P010:
	case V4L2_PIX_FMT_NV12M_SBWCL_10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST_P010;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV21M_P010:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST_P010;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST_PACKED10;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV21M_S10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST_PACKED10;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV16:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV61:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV16M_P210:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST_P210;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV61M_P210:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST_P210;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST_PACKED10;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV61M_S10B:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST_PACKED10;
		*bit_depth_format = CAMERAPP_PIXEL_SIZE_PACKED_10BIT;
		break;
	default:
		yuv_format = 0;
		dma_format = 0;
		bit_depth_format = 0;
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void camerapp_hw_gdc_set_format(void __iomem *base_addr,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 input_10bit_format, output_10bit_format;
	u32 input_yuv_format, output_yuv_format;
	u32 input_yuv_dma_format, output_yuv_dma_format;
	u32 mono_format;

	/* PIXEL_FORMAT:	0: NV12 (2plane Y/UV order), 1: NV21 (2plane Y/VU order) */
	/* YUV bit depth:	0: 8bit, 1: P010 , 2: reserved, 3: packed 10bit */
	/* YUV format:		0: YUV422, 1: YUV420 */
	/* MONO format:		0: YCbCr, 1: Y only(ignore UV related SFRs) */

	/* Check MONO format */
	mono_format = (s_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY)
			&& (d_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY);
	if (mono_format) {
		gdc_dbg("gdc mono_mode enabled\n");
		camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
			&gdc_fields[GDC_F_YUV_GDC_MONO_MODE], mono_format);
		camerapp_hw_gdc_set_wdma_mono_mode(base_addr, mono_format);
		return;
	}

	/* input */
	if (camerapp_hw_gdc_adjust_fmt(s_frame->gdc_fmt->pixelformat,
		&input_yuv_format, &input_yuv_dma_format, &input_10bit_format)) {
		gdc_info("not supported src pixelformat(%c%c%c%c)\n",
			(char)((s_frame->gdc_fmt->pixelformat >> 0) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 8) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 16) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 24) & 0xFF));
	}

	/* output */
	if (camerapp_hw_gdc_adjust_fmt(d_frame->gdc_fmt->pixelformat,
		&output_yuv_format, &output_yuv_dma_format, &output_10bit_format)) {
		gdc_info("not supported dst pixelformat(%c%c%c%c)\n",
			(char)((d_frame->gdc_fmt->pixelformat >> 0) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 8) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 16) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 24) & 0xFF));
	}

	gdc_dbg("gdc format (10bit, pix, yuv) : in(%d, %d, %d), out(%d, %d, %d), mono(%d)\n",
		input_10bit_format, input_yuv_dma_format, input_yuv_format,
		output_10bit_format, output_yuv_dma_format, output_yuv_format,
		mono_format);

	/* IN/OUT Format */
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_DST_10BIT_FORMAT], output_10bit_format);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_SRC_10BIT_FORMAT], input_10bit_format);
	camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_YUV_FORMAT], input_yuv_format);

	camerapp_hw_gdc_set_wdma_data_format(base_addr, output_yuv_dma_format);
	camerapp_hw_gdc_set_rdma_data_format(base_addr, input_yuv_dma_format);
}

static void camerapp_hw_gdc_set_core_param(void __iomem *base_addr,
	struct gdc_crop_param *crop_param)
{
	u32 luma_min, chroma_min;
	u32 luma_max, chroma_max;
	u32 input_bit_depth;

	luma_min = chroma_min = 0;

	/* interpolation type */
	/* 0 -all closest / 1 - Y bilinear, UV closest / 2 - all bilinear / 3 - Y bi-cubic, UV bilinear */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_INTERPOLATION],
		&gdc_fields[GDC_F_YUV_GDC_INTERP_TYPE], 0x3);
	/* Clamping type: 0 - none / 1 - min/max / 2 - near pixels min/max */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_INTERPOLATION],
		&gdc_fields[GDC_F_YUV_GDC_CLAMP_TYPE], 0x2);

	/* gdc boundary option setting : 0 - constant / 1 - mirroring */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_BOUNDARY_OPTION],
		&gdc_fields[GDC_F_YUV_GDC_BOUNDARY_OPTION], 1);

	/* gdc grid mode : 0 - input / 1 - output */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_GRID_MODE],
		&gdc_fields[GDC_F_YUV_GDC_GRID_MODE],
		crop_param->is_grid_mode);

	/* gdc bypass mode: : 0 - off / 1 - on */
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_BYPASS_MODE],
		crop_param->is_bypass_mode);

	/* output pixcel min/max value clipping */
	input_bit_depth = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_YUV_FORMAT],
		&gdc_fields[GDC_F_YUV_GDC_SRC_10BIT_FORMAT]);
	if (input_bit_depth == CAMERAPP_PIXEL_SIZE_8BIT)
		luma_max = chroma_max = 0xFF;
	else
		luma_max = chroma_max = 0x3FF;

	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_LUMA_MINMAX],
		&gdc_fields[GDC_F_YUV_GDC_LUMA_MIN], luma_min);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_LUMA_MINMAX],
		&gdc_fields[GDC_F_YUV_GDC_LUMA_MAX], luma_max);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_CHROMA_MINMAX],
		&gdc_fields[GDC_F_YUV_GDC_CHROMA_MIN], chroma_min);
	camerapp_sfr_set_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_CHROMA_MINMAX],
		&gdc_fields[GDC_F_YUV_GDC_CHROMA_MAX], chroma_max);
}

static void camerapp_hw_gdc_get_sbwc_type(struct gdc_frame *frame,
	u32 *comp_type, u32 *byte32num)
{
	switch (frame->extra) {
	case COMP:
		*comp_type = COMP;
		break;
	case COMP_LOSS:
		*comp_type = COMP_LOSS;
		if (frame->pixel_size && frame->comp_rate == GDC_LOSSY_COMP_RATE_60) {
			/* EVT0 10bit : 60% */
			*byte32num = 3;
		} else {
			/* 8bit : 50%, EVT1 10bit : 40% */
			*byte32num = 2;
		}
		break;
	default:
		*comp_type = NONE;
		break;
	}
}

static void camerapp_hw_gdc_set_compressor(void __iomem *base_addr,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame,
	struct gdc_crop_param *crop_param)
{
	u32 s_comp_type = 0;
	u32 d_comp_type = 0;
	u32 s_byte32num = 0;
	u32 d_byte32num = 0;

	camerapp_hw_gdc_get_sbwc_type(s_frame,
		&s_comp_type, &s_byte32num);
	camerapp_hw_gdc_get_sbwc_type(d_frame,
		&d_comp_type, &d_byte32num);

	gdc_dbg("src_comp_type(%d) src_byte32num(%d) dst_comp_type(%d) dst_byte32num(%d)\n",
		s_comp_type, s_byte32num, d_comp_type, d_byte32num);

	if (s_comp_type) {
		camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_COMPRESSOR],
		&gdc_fields[GDC_F_YUV_GDC_SRC_COMPRESSOR], s_comp_type);
		camerapp_hw_gdc_set_rdma_sbwc_enable(base_addr, s_comp_type);
		/* temporary 32b align */
		camerapp_hw_gdc_set_rdma_sbwc_64b_align(base_addr, 0);
		if (s_comp_type == COMP_LOSS)
			camerapp_hw_gdc_set_rdma_comp_rate(base_addr, s_byte32num);
	}

	if (d_comp_type) {
		camerapp_sfr_set_field(base_addr, &gdc_regs[GDC_R_YUV_GDC_COMPRESSOR],
		&gdc_fields[GDC_F_YUV_GDC_DST_COMPRESSOR], d_comp_type);
		camerapp_hw_gdc_set_wdma_sbwc_enable(base_addr, d_comp_type);
		/* temporary 32b align */
		camerapp_hw_gdc_set_wdma_sbwc_64b_align(base_addr, 0);
		if (d_comp_type == COMP_LOSS)
			camerapp_hw_gdc_set_wdma_comp_rate(base_addr, d_byte32num);
	}
}

u32 camerapp_hw_get_sbwc_constraint(struct gdc_frame *frame, u32 type)
{
	/* Support NV12 format only */
	if ((frame->gdc_fmt->pixelformat != V4L2_PIX_FMT_NV12M) &&
	(frame->gdc_fmt->pixelformat != V4L2_PIX_FMT_NV12) &&
	(frame->gdc_fmt->pixelformat != V4L2_PIX_FMT_NV12M_P010) &&
	(frame->gdc_fmt->pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_8B) &&
	(frame->gdc_fmt->pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_10B)) {
		gdc_info("Can't encode the format, check if it is supported format for SBWC. (%d)\n",
			frame->gdc_fmt->pixelformat);
		return -EINVAL;
	}
	/* Size align */
	if (!IS_ALIGNED(frame->width, CAMERAPP_COMP_BLOCK_WIDTH) ||
	    !IS_ALIGNED(frame->height, CAMERAPP_COMP_BLOCK_HEIGHT)) {
		gdc_info("%dx%d of target image is not aligned by %d*%d\n",
			frame->width, frame->height,
			CAMERAPP_COMP_BLOCK_WIDTH, CAMERAPP_COMP_BLOCK_HEIGHT);
		return -EINVAL;
	}

	return 0;
}

int camerapp_hw_get_comp_rate(struct gdc_dev *gdc, u32 pixfmt)
{
	int rate;

	if (pixfmt == V4L2_PIX_FMT_NV12M_SBWCL_8B) {
		rate = GDC_LOSSY_COMP_RATE_50;
	} else { /* V4L2_PIX_FMT_NV12M_SBWCL_10 */
		if (gdc->version == IP_VERSION_EVT0)
			rate = GDC_LOSSY_COMP_RATE_60;
		else
			rate = GDC_LOSSY_COMP_RATE_40;
	}

	return rate;
}

void camerapp_hw_gdc_update_param(void __iomem *base_addr, struct gdc_dev *gdc)
{
	struct gdc_frame *d_frame, *s_frame;
	struct gdc_crop_param *crop_param;

	s_frame = &gdc->current_ctx->s_frame;
	d_frame = &gdc->current_ctx->d_frame;
	crop_param = &gdc->current_ctx->crop_param[gdc->current_ctx->cur_index];

	camerapp_hw_gdc_set_initialization(base_addr);
	camerapp_hw_gdc_set_mode(base_addr, 0x3); /* apb direct....*/

	/* gdc grid scale setting */
	camerapp_hw_gdc_update_scale_parameters(base_addr, s_frame, d_frame, crop_param);
	/* gdc grid setting */
	camerapp_hw_gdc_update_grid_param(base_addr, crop_param);
	/* gdc SBWC */
	camerapp_hw_gdc_set_compressor(base_addr,
		s_frame, d_frame, crop_param);
	/* in/out data Format */
	camerapp_hw_gdc_set_format(base_addr, s_frame, d_frame);
	/* dma buffer size & address setting */
	camerapp_hw_gdc_set_dma_address(base_addr, s_frame, d_frame);
	camerapp_hw_gdc_update_dma_size(base_addr,
		s_frame, d_frame, crop_param);
	/* gdc core */
	camerapp_hw_gdc_set_core_param(base_addr, crop_param);

	//camerapp_sfr_set_reg(base_addr, &gdc_regs[GDC_R_GDC_PROCESSING], 1);
}

void camerapp_hw_gdc_status_read(void __iomem *base_addr)
{
	u32 rdmaStatus;
	u32 wdmaStatus;

	wdmaStatus = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_BUSY],
		&gdc_fields[GDC_F_YUV_GDC_WDMA_BUSY]);
	rdmaStatus = camerapp_sfr_get_field(base_addr,
		&gdc_regs[GDC_R_YUV_GDC_BUSY],
		&gdc_fields[GDC_F_YUV_GDC_RDMA_BUSY]);
}

void camerapp_hw_gdc_votf_enable(void __iomem *base_addr, u8 rw)
{
	if (rw) {
		camerapp_sfr_set_field(base_addr,
			&gdc_regs[GDC_R_YUV_RDMAY_VOTF_EN],
			&gdc_fields[GDC_F_YUV_RDMAY_VOTF_EN], 0x1);
		camerapp_sfr_set_field(base_addr,
			&gdc_regs[GDC_R_YUV_RDMAUV_VOTF_EN],
			&gdc_fields[GDC_F_YUV_RDMAUV_VOTF_EN], 0x1);
	} else {
		camerapp_sfr_set_field(base_addr,
			&gdc_regs[GDC_R_YUV_WDMA_VOTF_EN],
			&gdc_fields[GDC_F_YUV_WDMA_VOTF_EN], 0x3);

		/* vOTF-type */
		camerapp_sfr_set_field(base_addr,
			&gdc_regs[GDC_R_YUV_WDMA_BUSINFO],
			&gdc_fields[GDC_F_YUV_WDMA_BUSINFO], 0x40);
	}
}

void camerapp_gdc_sfr_dump(void __iomem *base_addr)
{
	u32 i;
	u32 reg_value;

	gdc_dbg("gdc v10.0");

	for (i = 0; i < GDC_REG_CNT; i++) {
		reg_value = readl(base_addr + gdc_regs[i].sfr_offset);
		pr_info("[@][SFR][DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			gdc_regs[i].reg_name, gdc_regs[i].sfr_offset, reg_value);
	}
}
