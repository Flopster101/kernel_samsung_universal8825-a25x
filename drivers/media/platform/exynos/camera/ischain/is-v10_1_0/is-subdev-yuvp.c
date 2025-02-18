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
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-hw-yuvp.h"
#include "is-stripe.h"

static int __ischain_yuvp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_YUVP_YUV:
		*pindex = PARAM_YUVP_YUV;
		ret = 2;
		break;
	case IS_LVN_YUVP_DRC:
		*pindex = PARAM_YUVP_DRC;
		ret = 1;
		break;
	case IS_LVN_YUVP_SVHIST:
		*pindex = PARAM_YUVP_SVHIST;
		ret = 1;
		break;
	case IS_LVN_YUVP_SEG:
		*pindex = PARAM_YUVP_SEG;
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __yuvp_dma_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, flag_extra;
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
		return -EINVAL;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		merr("incrop is NULL", device);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otcrop->w, otcrop->h);
	width = incrop->w;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	if (dma_output->format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->format, fmt->hw_format);
	if (dma_output->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->bitwidth, fmt->hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d][%d] WDMA sbwc_type: %d -> %d\n", device,
			node->vid, frame->fcount, pindex,
			dma_output->sbwc_type, hw_sbwc);
	if (!frame->stripe_info.region_num && ((dma_output->width != otcrop->w) ||
		(dma_output->height != otcrop->h)))
		mlvinfo("[F%d][%d] WDMA otcrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount, pindex,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->bitwidth = fmt->hw_bitwidth;
	dma_output->msb = fmt->hw_bitwidth - 1;
	dma_output->format = fmt->hw_format;
	dma_output->order = fmt->hw_order;
	dma_output->plane = fmt->hw_plane;

	dma_output->dma_crop_offset_x = incrop->x;
	dma_output->dma_crop_offset_y = incrop->y;
	dma_output->dma_crop_width = incrop->w;
	dma_output->dma_crop_height = incrop->h;

	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;

	dma_output->stride_plane0 = max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	dma_output->stride_plane1 = max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	return ret;
}

static int __yuvp_otf_cfg(struct is_device_ischain *device,
		struct is_group *group,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		struct is_stripe_info *stripe_info,
		IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_in;
	struct param_otf_output *otf_out;
	struct is_crop *otcrop;

	otf_in = is_itf_g_param(device, ldr_frame, PARAM_YUVP_OTF_INPUT);
	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_in->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(PARAM_YUVP_OTF_INPUT, pmap);

	if (!otf_in->cmd)
		return 0;

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	if (!ldr_frame->stripe_info.region_num &&
	    ((otf_in->width != otcrop->w) || (otf_in->height != otcrop->h)))
		mlvinfo("[F%d] OTF incrop[%d, %d, %d, %d]\n",
				device, IS_VIDEO_YUVP, ldr_frame->fcount,
				otcrop->x, otcrop->y,
				otcrop->w, otcrop->h);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		otf_in->width = ldr_frame->stripe_info.out.crop_width;
	else
		otf_in->width = otcrop->w;
	otf_in->height = otcrop->h;
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_YUV422;
	otf_in->bayer_crop_offset_x = 0;
	otf_in->bayer_crop_offset_y = 0;
	otf_in->bayer_crop_width = otf_in->width;
	otf_in->bayer_crop_height = otf_in->height;

	otf_out = is_itf_g_param(device, ldr_frame, PARAM_YUVP_OTF_OUTPUT);
	otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	set_bit(PARAM_YUVP_OTF_OUTPUT, pmap);

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otf_in->width, otf_in->height);

	return 0;
}

static int __yuvp_dma_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, height, flag_extra;
	int ret = 0;

	set_bit(pindex, pmap);

	group = &device->group_yuvp;

	dma_input = is_itf_g_param(device, frame, pindex);

	if (!IS_ENABLED(YPP_DDK_LIB_CALL))	{
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
		return 0;
	}

	if (pindex == PARAM_YUVP_DMA_INPUT && test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		if (dma_input->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->cmd, node->request);
		dma_input->cmd = node->request;

		if (!dma_input->cmd)
			return 0;
	}

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		merr("incrop is NULL", device);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num)
		width = frame->stripe_info.out.crop_width;
	else
		width = otcrop->w;

	height = otcrop->h;

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, width, height);

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num &&
	    (node->vid == IS_VIDEO_YUVP))
		is_ischain_g_stripe_cfg(frame, node, incrop, incrop, otcrop);

	if (dma_input->cmd) {
		if (dma_input->sbwc_type != hw_sbwc)
			mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->sbwc_type, hw_sbwc);
		if (!frame->stripe_info.region_num && ((dma_input->width != width) ||
				(dma_input->height != height)))
			mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
				node->vid, frame->fcount,
				incrop->x, incrop->y, width, height);
	}

	/* DRC and SV hist have own format enum */
	switch (node->vid) {
	/* RDMA */
	case IS_LVN_YUVP_YUV:
		hw_plane = DMA_INOUT_PLANE_2;
		break;
	case IS_LVN_YUVP_DRC:
		hw_format = DMA_INOUT_FORMAT_DRCGRID;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_LVN_YUVP_SVHIST:
		hw_format = DMA_INOUT_FORMAT_SVHIST;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	case IS_VIDEO_YNS_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	case IS_LVN_YUVP_SEG:
		hw_format = DMA_INOUT_FORMAT_SEGCONF;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	default:
		if (dma_input->bitwidth != hw_bitwidth)
			mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->bitwidth, hw_bitwidth);

		if (dma_input->format != hw_format)
			mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->format, hw_format);
		break;
	}

	dma_input->sbwc_type= hw_sbwc;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->order = hw_order;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->plane = hw_plane;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = height;
	dma_input->bayer_crop_offset_x = otcrop->x;
	dma_input->bayer_crop_offset_y = otcrop->y;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = height;
	dma_input->stride_plane0 = width;
	dma_input->stride_plane1 = width;
	dma_input->stride_plane2 = width;
	dma_input->v_otf_token_line = VOTF_TOKEN_LINE;

	return ret;
}

static int __yuvp_stripe_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_stripe_input *stripe_input;
	struct is_crop *otcrop;
	int i;

	set_bit(pindex, pmap);

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	stripe_input = is_itf_g_param(device, frame, pindex);
	if (frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					frame->stripe_info.h_pix_num[i - 1];
		}

		stripe_input->left_margin = frame->stripe_info.out.left_margin;
		stripe_input->right_margin = frame->stripe_info.out.right_margin;
		stripe_input->full_width = otcrop->w;
		stripe_input->full_height = otcrop->h;
		stripe_input->start_pos_x = frame->stripe_info.in.crop_x +
						frame->stripe_info.in.offset_x;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	return 0;
}

static int is_ischain_yuvp_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_dma_input *dma_input;
	struct param_otf_input *otf_input;
	struct param_dma_output *dma_output;
	struct param_otf_output *otf_output;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_order = DMA_INOUT_ORDER_YCbYCr;
	u32 hw_plane = 2;
	u32 flag_extra;
	u32 hw_sbwc = 0;
	bool chg_format = false;
	struct is_crop incrop_cfg, otcrop_cfg;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);

	sensor = device->sensor;
	group = &device->group_yuvp;
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

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
		hw_order = queue->framecfg.format->hw_order;
		hw_plane = queue->framecfg.format->hw_plane;
		flag_extra = (queue->framecfg.hw_pixeltype
				& PIXEL_TYPE_EXTRA_MASK)
				>> PIXEL_TYPE_EXTRA_SHIFT;

		if (flag_extra) {
			hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);
			chg_format = true;
		}

		if (chg_format)
			msdbgs(3, "in_crop[bitwidth %d sbwc 0x%x]\n",
					device, leader,
					hw_bitwidth,
					hw_sbwc);
	}

	/* Configure Conrtol */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_YUVP_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}

		set_bit(PARAM_YUVP_CONTROL, pmap);
	}

	dma_input = is_itf_g_param(device, frame, PARAM_YUVP_DMA_INPUT);
	otf_input = is_itf_g_param(device, frame, PARAM_YUVP_OTF_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_ENABLE;

			dma_input->format = hw_format;
			dma_input->bitwidth = hw_bitwidth;
			dma_input->msb = hw_bitwidth - 1;
			dma_input->order = hw_order;
			dma_input->sbwc_type = hw_sbwc;
			dma_input->plane = hw_plane;
			dma_input->width = incrop_cfg.w;
			dma_input->height = incrop_cfg.h;

			dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
			dma_input->dma_crop_width = incrop_cfg.w;
			dma_input->dma_crop_height = incrop_cfg.h;
			dma_input->bayer_crop_offset_x = incrop_cfg.x;
			dma_input->bayer_crop_offset_y = incrop_cfg.y;
			dma_input->bayer_crop_width = incrop_cfg.w;
			dma_input->bayer_crop_height = incrop_cfg.h;
			dma_input->stride_plane0 = incrop->w;

			set_bit(PARAM_YUVP_DMA_INPUT, pmap);
		} else {
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

			otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
			otf_input->format = hw_format;
			otf_input->bitwidth = hw_bitwidth;
			otf_input->order = hw_order;
			otf_input->width = incrop_cfg.w;
			otf_input->height = incrop_cfg.h;

			otf_input->bayer_crop_offset_x = incrop_cfg.x;
			otf_input->bayer_crop_offset_y = incrop_cfg.y;
			otf_input->bayer_crop_width = incrop_cfg.w;
			otf_input->bayer_crop_height = incrop_cfg.h;

			set_bit(PARAM_YUVP_OTF_INPUT, pmap);
		}
	} else {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

		dma_input->format = hw_format;
		dma_input->bitwidth = hw_bitwidth;
		dma_input->msb = hw_bitwidth - 1;
		dma_input->order = hw_order;
		dma_input->sbwc_type = hw_sbwc;
		dma_input->plane = hw_plane;
		dma_input->width = incrop_cfg.w;
		dma_input->height = incrop_cfg.h;

		dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
		dma_input->dma_crop_width = incrop_cfg.w;
		dma_input->dma_crop_height = incrop_cfg.h;
		dma_input->bayer_crop_offset_x = incrop_cfg.x;
		dma_input->bayer_crop_offset_y = incrop_cfg.y;
		dma_input->bayer_crop_width = incrop_cfg.w;
		dma_input->bayer_crop_height = incrop_cfg.h;
		dma_input->stride_plane0 = incrop->w;

		set_bit(PARAM_YUVP_DMA_INPUT, pmap);
	}

	dma_output = is_itf_g_param(device, frame, PARAM_YUVP_YUV);
	otf_output = is_itf_g_param(device, frame, PARAM_YUVP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		otf_output = is_itf_g_param(device, frame, PARAM_YUVP_OTF_OUTPUT);
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		otf_output->width = incrop_cfg.w;
		otf_output->height = incrop_cfg.h;
		otf_output->format = OTF_OUTPUT_FORMAT_YUV422;
		otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_10BIT;
		otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;

		set_bit(PARAM_YUVP_OTF_OUTPUT, pmap);
	} else {
		dma_output = is_itf_g_param(device, frame, PARAM_YUVP_YUV);
		dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
		dma_output->format = hw_format;
		dma_output->bitwidth = hw_bitwidth;
		dma_output->msb = hw_bitwidth - 1;
		dma_output->order = hw_order;
		dma_output->sbwc_type = hw_sbwc;
		dma_output->plane = hw_plane;
		dma_output->width = incrop_cfg.w;
		dma_output->height = incrop_cfg.h;

		set_bit(PARAM_YUVP_YUV, pmap);
	}

	stripe_input = is_itf_g_param(device, frame, PARAM_YUVP_STRIPE_INPUT);
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

	set_bit(PARAM_YUVP_STRIPE_INPUT, pmap);

p_err:
	return ret;
}

static int is_ischain_yuvp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct yuvp_param *yuvp_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	u32 *targetAddr;
	u32 n, p, num_planes, pindex = 0;
	int dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_yuvp;
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	yuvp_param = &device->is_region->parameter.yuvp;

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		if (frame->shot_ext->node_group.leader.mode == CAMERA_NODE_NORMAL)
			goto p_skip;

		out_node = &frame->shot_ext->node_group.leader;

		ret = __yuvp_otf_cfg(device, group, frame, out_node,
				&frame->stripe_info, pmap);
		if (ret) {
			mlverr("[F%d] otf_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __yuvp_dma_in_cfg(device, subdev, frame, out_node,
				PARAM_YUVP_DMA_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __yuvp_stripe_in_cfg(device, subdev, frame, out_node,
				PARAM_YUVP_STRIPE_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] strip_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			cap_node = &frame->shot_ext->node_group.capture[n];

			if (!cap_node->vid)
				continue;

			dma_type = __ischain_yuvp_slot(cap_node, &pindex);
			if (dma_type == 1)
				ret = __yuvp_dma_in_cfg(device, subdev, frame,
						cap_node, pindex, pmap);
			else if (dma_type == 2)
				ret = __yuvp_dma_out_cfg(device, subdev, frame, cap_node,
						pindex, pmap);
			if (ret) {
				mlverr("[F%d] dma_%s_cfg error\n", device, cap_node->vid,
						frame->fcount,
						(dma_type == 1) ? "in" : "out");
				return ret;
			}
		}

#ifdef ENABLE_LVN_DUMMYOUTPUT
		for (p = 0; p < frame->out_node.sframe[0].num_planes; p++) {
			if (!frame->out_node.sframe[0].id) {
				mrerr("Invalid video id(%d)", device, frame,
						frame->out_node.sframe[0].id);
				return -EINVAL;
			}
			frame->dvaddr_buffer[p] =
				frame->out_node.sframe[0].dva[p];
			frame->kvaddr_buffer[p] =
				frame->out_node.sframe[0].kva[p];
		}
#endif

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sframe = &frame->cap_node.sframe[n];
			if (!sframe->id)
				continue;

			if ((sframe->id >= IS_LVN_MCSC_P0) &&
					(sframe->id <= IS_LVN_MCSC_P5))
				continue;

			targetAddr = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr,
					NULL, sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device,
						frame, sframe->id);
				return -EINVAL;
			}

			if (!targetAddr)
				continue;

			num_planes = sframe->num_planes * frame->num_buffers;

			for (p = 0; p < num_planes; p++)
				targetAddr[p] = sframe->dva[p];
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
	inparm.w = leader->input.width;
	inparm.h = leader->input.height;
	if (yuvp_param->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		inparm.w = yuvp_param->dma_input.width;
		inparm.h = yuvp_param->dma_input.height;
	}
	if (yuvp_param->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		inparm.w = yuvp_param->otf_input.width;
		inparm.h = yuvp_param->otf_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	/* not supported DMA input crop */
	if ((incrop->x != 0) || (incrop->y != 0))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_yuvp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			pmap);
		if (ret) {
			merr("is_ischain_yuvp_cfg is fail(%d)", device, ret);
			goto p_err;
		}
		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			is_get_debug_param(IS_DEBUG_PARAM_STREAM)) {
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

static int is_ischain_yuvp_get(struct is_subdev *subdev,
			       struct is_device_ischain *idi,
			       struct is_frame *frame,
			       enum pablo_subdev_get_type type,
			       void *result)
{
	struct camera2_node *node;
	struct is_crop *outcrop;

	switch (type) {
	case PSGT_REGION_NUM:
		node = &frame->shot_ext->node_group.leader;
		outcrop = (struct is_crop *)node->output.cropRegion;
		*(int *)result = is_calc_region_num(outcrop->w, subdev);
		break;
	default:
		break;
	}

	return 0;
}

const struct is_subdev_ops is_subdev_yuvp_ops = {
	.bypass	= NULL,
	.cfg	= is_ischain_yuvp_cfg,
	.tag	= is_ischain_yuvp_tag,
	.get	= is_ischain_yuvp_get,
};
