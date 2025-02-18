// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2_exynos_media.h>
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-votfmgr.h"
#include "votf/camerapp-votf.h"
#include "is-votf-id-table.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-stripe.h"

static int __ischain_rgbp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_VIDEO_RGBP:
		*pindex = PARAM_RGBP_DMA_INPUT;
		ret = 1;
		break;
	case IS_LVN_RGBP_HF:
		*pindex = PARAM_RGBP_HF;
		ret = 2;
		break;
	case IS_LVN_RGBP_SF:
		*pindex = PARAM_RGBP_SF;
		ret = 2;
		break;
	case IS_LVN_RGBP_YUV:
		*pindex = PARAM_RGBP_YUV;
		ret = 2;
		break;
	case IS_LVN_RGBP_RGB:
		*pindex = PARAM_RGBP_RGB;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __rgbp_otf_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	struct is_crop incrop_cfg, otcrop_cfg;
	struct is_param_region *param_region;
	u32 width;
	u32 hw_order;

	set_bit(pindex, pmap);

	group = &device->group_rgbp;

	otf_input = is_itf_g_param(device, ldr_frame, pindex);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;

	if (!otf_input->cmd)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	if (!ldr_frame->stripe_info.region_num &&
	    ((otf_input->width != incrop->w) || (otf_input->height != incrop->h)))
		mlvinfo("[F%d]OTF incrop[%d, %d, %d, %d]\n", device,
			IS_VIDEO_RGBP, ldr_frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		width = ldr_frame->stripe_info.in.crop_width;
	else
		width = incrop_cfg.w;

	param_region = &device->is_region->parameter;
	hw_order = param_region->sensor.config.pixel_order;
	otf_input->width = width;
	otf_input->height = incrop_cfg.h;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bayer_crop_offset_x = 0;
	otf_input->bayer_crop_offset_y = 0;
	otf_input->bayer_crop_width = width;
	otf_input->bayer_crop_height = incrop_cfg.h;
	otf_input->order = hw_order;

	return 0;
}

static int __rgbp_otf_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_otf_output *otf_output;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	struct is_crop incrop_cfg, otcrop_cfg;
	u32 width;

	set_bit(pindex, pmap);

	group = &device->group_rgbp;

	otf_output = is_itf_g_param(device, ldr_frame, pindex);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;

	if (!otf_output->cmd)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	if (!ldr_frame->stripe_info.region_num &&
	    ((otf_output->width != otcrop->w) || (otf_output->height != otcrop->h)))
		mlvinfo("[F%d]OTF otcrop[%d, %d, %d, %d]\n", device,
			IS_VIDEO_RGBP, ldr_frame->fcount,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		width = ldr_frame->stripe_info.out.crop_width;
	else
		width = otcrop_cfg.w;

	otf_output->width = width;
	otf_output->height = otcrop_cfg.h;
	otf_output->format = OTF_YUV_FORMAT;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;

	return 0;
}

static int __rgbp_dma_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct param_stripe_input *stripe_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_msb;
	u32 hw_order, flag_extra, flag_pixel_size;
	u32 width;
	struct is_crop incrop_cfg, otcrop_cfg;
	int ret = 0;
	int i;

	set_bit(pindex, pmap);

	group = &device->group_rgbp;

	dma_input = is_itf_g_param(device, ldr_frame, pindex);

	if (pindex == PARAM_RGBP_DMA_INPUT && test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		if (dma_input->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
				node->vid, ldr_frame->fcount,
				dma_input->cmd, node->request);
		dma_input->cmd = node->request;

		if (!dma_input->cmd)
			return 0;
	}

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;
	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = fmt->bitsperpixel[0] - 1;
	hw_order = fmt->hw_order;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
		&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		mdbgs_ischain(3, "in_crop[bitwidth: %d -> %d: 13 bit BAYER]\n",
				device,
				hw_bitwidth,
				DMA_INOUT_BIT_WIDTH_13BIT);
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_13BIT - 1;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		hw_msb = fmt->bitsperpixel[0];	/* consider signed format only */
		mdbgs_ischain(3, "in_crop[unpack bitwidth: %d, msb: %d]\n",
				device,
				hw_bitwidth,
				hw_msb);
	}

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	switch (node->vid) {
	/* RDMA */
	case IS_VIDEO_RGBP:
		hw_format = DMA_INOUT_FORMAT_BAYER_PACKED;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_10BIT - 1;
		hw_order = fmt->hw_order;
		break;
	default:
		break;
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num &&
	    (node->vid == IS_VIDEO_RGBP))
		is_ischain_g_stripe_cfg(ldr_frame, node, &incrop_cfg, &incrop_cfg, &otcrop_cfg);

	if (dma_input->cmd) {
		if (dma_input->format != hw_format)
			mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
				node->vid, ldr_frame->fcount,
				dma_input->format, hw_format);
		if (dma_input->bitwidth != hw_bitwidth)
			mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
				node->vid, ldr_frame->fcount, dma_input->bitwidth, hw_bitwidth);
		if (dma_input->sbwc_type != hw_sbwc)
			mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
				node->vid, ldr_frame->fcount,
				dma_input->sbwc_type, hw_sbwc);
		if (!ldr_frame->stripe_info.region_num && ((dma_input->width != incrop->w) ||
			(dma_input->height != incrop->h)))
			mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
				node->vid, ldr_frame->fcount,
				incrop->x, incrop->y, incrop->w, incrop->h);
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		width = ldr_frame->stripe_info.in.crop_width;
	else
		width = incrop_cfg.w;

	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->order = hw_order;
	dma_input->plane = fmt->hw_plane;
	dma_input->width = width;
	dma_input->height = incrop_cfg.h;
	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->stride_plane0 = incrop->w;
	dma_input->stride_plane1 = incrop->w;
	dma_input->stride_plane2 = incrop->w;
	/* VOTF of slave in is always disabled */
	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	dma_input->v_otf_token_line = VOTF_TOKEN_LINE;

	stripe_input = is_itf_g_param(device, ldr_frame, PARAM_RGBP_STRIPE_INPUT);
	if (ldr_frame->stripe_info.region_num) {
		stripe_input->index = ldr_frame->stripe_info.region_id;
		stripe_input->total_count = ldr_frame->stripe_info.region_num;
		if (!ldr_frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < ldr_frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					ldr_frame->stripe_info.h_pix_num[i - 1];
		}

		stripe_input->left_margin = ldr_frame->stripe_info.in.left_margin;
		stripe_input->right_margin = ldr_frame->stripe_info.in.right_margin;
		stripe_input->full_width = incrop->w;
		stripe_input->full_height = incrop->h;
		stripe_input->start_pos_x = ldr_frame->stripe_info.in.crop_x +
						ldr_frame->stripe_info.in.offset_x;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	set_bit(PARAM_RGBP_STRIPE_INPUT, pmap);

	return ret;
}

static int __rgbp_dma_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	struct is_stripe_info *stripe_info,
	u32 pindex,
	IS_DECLARE_PMAP(pmap),
	int index)
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	struct is_crop otcrop_cfg;
	u32 votf_dst_ip = 0;
	u32 votf_dst_axi_id_m = 0;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_plane, hw_msb, hw_order, flag_extra, flag_pixel_size;
	u32 width;
	int ret = 0;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);

	dma_output = is_itf_g_param(device, frame, pindex);
	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->cmd, node->request);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(pindex, pmap);

	if (!node->request)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}
	otcrop_cfg = *otcrop;
	width = otcrop_cfg.w;
	if (IS_NULL_CROP(incrop)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixelformat(%c%c%c%c) is not found", device,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}
	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = fmt->bitsperpixel[0] - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
	    && flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_10BIT - 1;
	} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
		/* NOTICE: consider signed format only */
		hw_msb = fmt->bitsperpixel[0];
	}

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	switch (node->vid) {
	/* WDMA */
	case IS_LVN_RGBP_HF:
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1;
		break;
	case IS_LVN_RGBP_SF:
	case IS_LVN_RGBP_YUV:
		hw_format = DMA_INOUT_FORMAT_YUV422;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
		hw_msb = DMA_INOUT_BIT_WIDTH_12BIT - 1;
		break;
	case IS_LVN_RGBP_RGB:
		break;
	default:
		break;
	}

	if (dma_output->format != hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->format, hw_format);
	if (dma_output->bitwidth != hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->bitwidth, hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]WDMA sbwc_type: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_output->sbwc_type, hw_sbwc);
	if (!stripe_info->region_num && ((dma_output->width != otcrop->w) ||
		(dma_output->height != otcrop->h)))
		mlvinfo("[F%d]WDMA otcrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;

	if (test_bit(IS_GROUP_OTF_OUTPUT, &device->group_rgbp.state)) {
		if (test_bit(IS_GROUP_VOTF_OUTPUT, &device->group_rgbp.state))
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
		 else
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
	} else {
		dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
	}

	dma_output->format = hw_format;
	dma_output->bitwidth = hw_bitwidth;
	dma_output->msb = hw_msb;
	dma_output->sbwc_type = hw_sbwc;
	dma_output->order = hw_order;
	dma_output->plane = hw_plane;
	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop_cfg.w;
	dma_output->dma_crop_height = otcrop_cfg.h;

	/* VOTF of slave in is alwasy disabled */
	dma_output->v_otf_dst_ip = votf_dst_ip;
	dma_output->v_otf_dst_axi_id = votf_dst_axi_id_m;
	dma_output->v_otf_token_line = VOTF_TOKEN_LINE;

	mdbgs_ischain(4, "[F%d] , width: %d, height: %d, msb: %d, cmd : %d\n", device,
		frame->fcount, dma_output->width, dma_output->height, dma_output->msb, dma_output->cmd);

	return ret;
}

static int is_ischain_rgbp_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	struct param_dma_output *dma_output;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_crop *scc_incrop;
	struct is_crop *scp_incrop;
	struct is_device_ischain *device;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = 0, hw_msb = MSB_OF_3AA_DMA_OUT;
	u32 flag_extra, flag_pixel_size;
	u32 hw_plane = DMA_INOUT_PLANE_1;
	bool chg_format = false;
	struct is_crop incrop_cfg, otcrop_cfg;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);

	scc_incrop = scp_incrop = incrop;
	group = &device->group_rgbp;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (!queue->framecfg.format) {
			merr("format is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		hw_plane = queue->framecfg.format->hw_plane;
		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth;

		/* pixel type [0:5] : pixel size, [6:7] : extra */
		flag_pixel_size = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
		flag_extra = (queue->framecfg.hw_pixeltype
				& PIXEL_TYPE_EXTRA_MASK)
				>> PIXEL_TYPE_EXTRA_SHIFT;

		if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED
			&& flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
			hw_msb = MSB_OF_3AA_DMA_OUT + 1;
			hw_bitwidth = DMA_INOUT_BIT_WIDTH_13BIT;
			chg_format = true;
		} else if (hw_format == DMA_INOUT_FORMAT_BAYER) {
			/* consider signed format only */
			hw_msb = hw_bitwidth;
			hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
			chg_format = true;
		}

		if (flag_extra) {
			hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);
			chg_format = true;
		}

		if (chg_format)
			msdbgs(3, "in_crop[bitwidth %d msb %d sbwc 0x%x]\n",
					device, leader,
					hw_bitwidth, hw_msb,
					hw_sbwc);
	}

	/* Configure Control */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_RGBP_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;

		set_bit(PARAM_RGBP_CONTROL, pmap);
	}

	otf_input = is_itf_g_param(device, frame, PARAM_RGBP_OTF_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->width = incrop_cfg.w;
	otf_input->height = incrop_cfg.h;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bayer_crop_offset_x = 0;
	otf_input->bayer_crop_offset_y = 0;
	otf_input->bayer_crop_width = incrop_cfg.w;
	otf_input->bayer_crop_height = incrop_cfg.h;

	set_bit(PARAM_RGBP_OTF_INPUT, pmap);

	dma_input = is_itf_g_param(device, frame, PARAM_RGBP_DMA_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->plane = hw_plane;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->width = incrop_cfg.w;
	dma_input->height = incrop_cfg.h;
	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->stride_plane0 = incrop->w;

	set_bit(PARAM_RGBP_DMA_INPUT, pmap);

	otf_output = is_itf_g_param(device, frame, PARAM_RGBP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	otf_output->format = OTF_YUV_FORMAT;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;

	set_bit(PARAM_RGBP_OTF_OUTPUT, pmap);

	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state)) {
			dma_output = is_itf_g_param(device, frame, PARAM_RGBP_OTF_OUTPUT);
			dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
			dma_output->width = incrop_cfg.w;
			dma_output->height = incrop_cfg.h;
			dma_output->format = DMA_INOUT_FORMAT_YUV422;
			dma_output->bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
			dma_output->plane = DMA_INOUT_PLANE_2;

			set_bit(PARAM_RGBP_OTF_OUTPUT, pmap);
		}
	}

	stripe_input = is_itf_g_param(device, frame, PARAM_RGBP_STRIPE_INPUT);
	if (frame && frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else if (frame->stripe_info.region_id < frame->stripe_info.region_num - 1) {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = 0;
		}
		stripe_input->full_width = leader->input.width;
		stripe_input->full_height = leader->input.height;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	set_bit(PARAM_RGBP_STRIPE_INPUT, pmap);

p_err:
	return ret;
}

static int is_ischain_rgbp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct rgbp_param *rgbp_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	struct is_fmt *format, *tmp_format;
	struct is_queue *queue;
	u32 fmt_pixelsize;
	bool chg_fmt_size = false;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	u32 *targetAddr;
	u64 *targetAddr_k;
	u32 pindex = 0, num_planes;
	int j, i, p, dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "RGBP TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_rgbp;
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	rgbp_param = &device->is_region->parameter.rgbp;

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		queue = GET_SUBDEV_QUEUE(leader);
		if (!queue) {
			merr("queue is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (queue->mode == CAMERA_NODE_NORMAL)
			goto p_skip;

		out_node = &frame->shot_ext->node_group.leader;
		ret = __rgbp_dma_in_cfg(device, subdev, frame, out_node,
					PARAM_RGBP_DMA_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __rgbp_otf_in_cfg(device, subdev, frame, out_node,
					PARAM_RGBP_OTF_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] otf_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __rgbp_otf_out_cfg(device, subdev, frame, out_node,
					PARAM_RGBP_OTF_OUTPUT, pmap);
		if (ret) {
			mlverr("[F%d] otf_out_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			cap_node = &frame->shot_ext->node_group.capture[i];

			if (!cap_node->vid)
				continue;
			dma_type = __ischain_rgbp_slot(cap_node, &pindex);
			if (dma_type == 1)
				ret = __rgbp_dma_in_cfg(device, subdev, frame, cap_node,
						pindex, pmap);
			else if (dma_type == 2)
				ret = __rgbp_dma_out_cfg(device, subdev, frame, cap_node,
						&frame->stripe_info, pindex, pmap, i);

			if (ret) {
				mlverr("[F%d] dma_%s_cfg error\n", device, cap_node->vid,
						frame->fcount,
						(dma_type == 1) ? "in" : "out");
				return ret;
			}
		}

		/* buffer tagging */
#ifdef ENABLE_LVN_DUMMYOUTPUT
		for (i = 0; i < frame->out_node.sframe[0].num_planes; i++) {
			if (!frame->out_node.sframe[0].id) {
				mrerr("Invalid video id(%d)", device, frame,
						frame->out_node.sframe[0].id);
				return -EINVAL;
			}

			frame->dvaddr_buffer[i] = frame->out_node.sframe[0].dva[i];
			frame->kvaddr_buffer[i] = frame->out_node.sframe[0].kva[i];
		}
#endif
		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			sframe = &frame->cap_node.sframe[i];
			if (!sframe->id)
				continue;

			targetAddr = NULL;
			targetAddr_k = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr, &targetAddr_k, sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device, frame,
						sframe->id);
				return -EINVAL;
			}

			num_planes = sframe->num_planes / frame->num_shots;
			if (targetAddr) {
				for (j = 0, p = num_planes * frame->cur_shot_idx;
					j < num_planes; j++, p++) {
					targetAddr[j] = sframe->dva[p];
				}
			}

			if (targetAddr_k) {
				for (j = 0, p = num_planes * frame->cur_shot_idx; j < num_planes; j++, p++)
					targetAddr_k[j] = sframe->kva[p];
			}
		}

		ret = is_itf_s_param(device, frame, pmap);
		if (ret) {
			mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
			goto p_err;
		}

		return ret;
	}

p_skip:
	inparm.x = 0;
	inparm.y = 0;
	inparm.w = rgbp_param->dma_input.width;
	inparm.h = rgbp_param->dma_input.height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* per-frame pixelformat control for changing fmt */
	format = queue->framecfg.format;
	if (node->pixelformat && format->pixelformat != node->pixelformat) {
		tmp_format = is_find_format((u32)node->pixelformat, 0);
		if (tmp_format) {
			mdbg_pframe("pixelformat is changed(%c%c%c%c->%c%c%c%c)\n",
				device, subdev, frame,
				(char)((format->pixelformat >> 0) & 0xFF),
				(char)((format->pixelformat >> 8) & 0xFF),
				(char)((format->pixelformat >> 16) & 0xFF),
				(char)((format->pixelformat >> 24) & 0xFF),
				(char)((tmp_format->pixelformat >> 0) & 0xFF),
				(char)((tmp_format->pixelformat >> 8) & 0xFF),
				(char)((tmp_format->pixelformat >> 16) & 0xFF),
				(char)((tmp_format->pixelformat >> 24) & 0xFF));
			queue->framecfg.format = format = tmp_format;
		} else {
			mdbg_pframe("pixelformat is not found(%c%c%c%c)\n",
				device, subdev, frame,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));
		}
		chg_fmt_size = true;
	}

	fmt_pixelsize = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
	if (node->pixelsize && node->pixelsize != fmt_pixelsize) {
		mdbg_pframe("pixelsize is changed(%d->%d)\n",
			device, subdev, frame,
			fmt_pixelsize, node->pixelsize);
		queue->framecfg.hw_pixeltype =
			(queue->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			| (node->pixelsize & PIXEL_TYPE_SIZE_MASK);

		chg_fmt_size = true;
	}

	if (!COMPARE_CROP(incrop, &inparm) ||
	    CHECK_STRIPE_CFG(&frame->stripe_info) ||
	    chg_fmt_size ||
	    test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_rgbp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			pmap);
		if (ret) {
			merr("is_ischain_rgbp_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		if (!COMPARE_CROP(incrop, &subdev->input.crop) || is_get_debug_param(IS_DEBUG_PARAM_STREAM)) {
			msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
					incrop->x, incrop->y, incrop->w, incrop->h);
			subdev->input.crop = *incrop;
		}
	}

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ischain_rgbp_get(struct is_subdev *subdev,
			       struct is_device_ischain *idi,
			       struct is_frame *frame,
			       enum pablo_subdev_get_type type,
			       void *result)
{
	struct is_crop *incrop;
	struct camera2_node *node;

	msrdbgs(1, "GET type: %d\n", idi, subdev, frame, type);

	switch (type) {
	case PSGT_REGION_NUM:
		node = &frame->shot_ext->node_group.leader;
		incrop = (struct is_crop *)node->input.cropRegion;
		*(int *)result = is_calc_region_num(incrop->w, subdev);
		break;
	default:
		break;
	}

	return 0;
}

const struct is_subdev_ops is_subdev_rgbp_ops = {
	.bypass	= NULL,
	.cfg	= is_ischain_rgbp_cfg,
	.tag	= is_ischain_rgbp_tag,
	.get	= is_ischain_rgbp_get,
};
