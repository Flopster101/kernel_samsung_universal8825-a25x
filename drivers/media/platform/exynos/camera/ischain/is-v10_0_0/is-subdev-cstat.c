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

#include "is-devicemgr.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

static void cstat_ctl_cfg(struct is_device_ischain *device,
		struct is_group *group,
		enum is_param pindex,
		IS_DECLARE_PMAP(pmap))
{
	struct param_control *control = is_itf_g_param(device, NULL, pindex);

	if (test_bit(IS_GROUP_START, &group->state)) {
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
	} else {
		control->cmd = CONTROL_COMMAND_STOP;
		control->bypass = CONTROL_BYPASS_DISABLE;
	}

	set_bit(pindex, pmap);
}

static int cstat_otf_in_cfg(struct is_device_ischain *device,
		struct is_group *group,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		enum is_param pindex,
		IS_DECLARE_PMAP(pmap))
{
	struct is_device_sensor *sensor;
	struct param_otf_input *otf_in;
	struct is_crop *incrop, *otcrop;
	struct is_param_region *param_region;
	u32 hw_order;

	FIMC_BUG(!node);

	sensor = device->sensor;

	otf_in = is_itf_g_param(device, ldr_frame, pindex);
	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_in->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(pindex, pmap);

	if (!otf_in->cmd)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid, ldr_frame->fcount);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d] otcrop is NULL", device, node->vid, ldr_frame->fcount);
		return -EINVAL;
	}

	if ((otf_in->bayer_crop_width != incrop->w) ||
			(otf_in->bayer_crop_height != incrop->h))
		mlvinfo("[F%d] OTF in bcrop[%d, %d, %d, %d]\n",
				device, node->vid, ldr_frame->fcount,
				incrop->x, incrop->y,
				incrop->w, incrop->h);

	param_region = &device->is_region->parameter;
	hw_order = param_region->sensor.config.pixel_order;

	otf_in->width = is_sensor_g_bns_width(sensor);
	otf_in->height = is_sensor_g_bns_height(sensor);
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_BAYER;
	otf_in->order = hw_order;
	otf_in->bayer_crop_offset_x = incrop->x;
	otf_in->bayer_crop_offset_y = incrop->y;
	otf_in->bayer_crop_width = incrop->w;
	otf_in->bayer_crop_height = incrop->h;

	if ((sensor->cfg && sensor->cfg->input[CSI_VIRTUAL_CH_0].hwformat == HW_FORMAT_RAW12)
	    /* || (sensor->obte_config & BIT(EX_12BIT)) legacy code EX_12BIT*/)
		otf_in->bitwidth = OTF_INPUT_BIT_WIDTH_14BIT;
	else
		otf_in->bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;

	return 0;
}

static int cstat_dma_in_cfg(struct is_device_ischain *device,
		struct is_group *group,
		struct is_subdev *leader,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		enum is_param pindex,
		IS_DECLARE_PMAP(pmap))
{
	struct param_dma_input *dma_in;
	struct is_crop *incrop, *otcrop;
	struct is_fmt *fmt;

	dma_in = is_itf_g_param(device, ldr_frame, pindex);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_in->cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		if (dma_in->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device, node->vid,
					ldr_frame->fcount, dma_in->cmd, node->request);

		dma_in->cmd = node->request;
	}

	set_bit(pindex, pmap);

	if (!dma_in->cmd)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid, ldr_frame->fcount);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d] otcrop is NULL", device, node->vid, ldr_frame->fcount);
		return -EINVAL;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[F%d] pixelformat(%c%c%c%c) is not found", device, node->vid,
				ldr_frame->fcount,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}

	if (dma_in->format != fmt->hw_format)
		mlvinfo("[F%d] RDMA format: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_in->format, fmt->hw_format);
	if (dma_in->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d] RDMA bitwidth: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_in->bitwidth, fmt->hw_bitwidth);
	if ((dma_in->dma_crop_width != incrop->w) ||
			(dma_in->dma_crop_height != incrop->h))
		mlvinfo("[F%d] RDMA bcrop[%d, %d, %d, %d]\n", device, node->vid,
				ldr_frame->fcount, incrop->x, incrop->y, incrop->w, incrop->h);

	dma_in->format = fmt->hw_format;
	dma_in->bitwidth = fmt->hw_bitwidth;
	dma_in->msb = fmt->bitsperpixel[0] - 1;
	dma_in->order = DMA_INOUT_ORDER_GR_BG;
	dma_in->plane = 1;

	dma_in->width = leader->input.width;
	dma_in->height = leader->input.height;
	dma_in->dma_crop_offset = 0;
	dma_in->dma_crop_width = leader->input.width;
	dma_in->dma_crop_height = leader->input.height;
	dma_in->bayer_crop_offset_x = incrop->x;
	dma_in->bayer_crop_offset_y = incrop->y;
	dma_in->bayer_crop_width = incrop->w;
	dma_in->bayer_crop_height = incrop->h;

	return 0;
}

static int cstat_dma_out_cfg(struct is_device_ischain *device,
		struct is_subdev *leader,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		enum is_param pindex,
		IS_DECLARE_PMAP(pmap))
{
	struct param_dma_output *dma_out;
	struct is_crop *incrop, *otcrop;
	struct is_fmt *fmt;

	dma_out = is_itf_g_param(device, ldr_frame, pindex);
	if (dma_out->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_out->cmd, node->request);

	dma_out->cmd = node->request;

	set_bit(pindex, pmap);

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid, ldr_frame->fcount);
		incrop->x = incrop->y = 0;
		incrop->w = leader->input.width;
		incrop->h = leader->input.height;
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d] otcrop is NULL", device, node->vid, ldr_frame->fcount);
		otcrop->x = otcrop->y = 0;
		otcrop->w = leader->input.width;
		otcrop->h = leader->input.height;
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[F%d] pixelformat(%c%c%c%c) is not found", device, node->vid,
				ldr_frame->fcount,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	if (dma_out->format != fmt->hw_format)
		mlvinfo("[F%d] WDMA format: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_out->format, fmt->hw_format);
	if (dma_out->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d] WDMA bitwidth: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_out->bitwidth, fmt->hw_bitwidth);
	if ((dma_out->dma_crop_width != incrop->w) || (dma_out->dma_crop_height != incrop->h))
		mlvinfo("[F%d] WDMA incrop[%d, %d, %d, %d]\n", device, node->vid,
				ldr_frame->fcount, incrop->x, incrop->y, incrop->w, incrop->h);
	if ((dma_out->width != otcrop->w) || (dma_out->height != otcrop->h)) {
		/* FDPIG usually changes its size between 320x240 and 640x480 */
		if ((node->vid == IS_LVN_CSTAT0_FDPIG) ||
		    (node->vid == IS_LVN_CSTAT1_FDPIG) ||
		    (node->vid == IS_LVN_CSTAT2_FDPIG) ||
		    (node->vid == IS_LVN_CSTAT3_FDPIG)) {
			mdbgs_ischain(4, "[F%d] WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid,
					ldr_frame->fcount, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		} else {
			mlvinfo("[F%d] WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid,
					ldr_frame->fcount, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}
	}

	dma_out->format = fmt->hw_format;
	dma_out->bitwidth = fmt->hw_bitwidth;
	dma_out->msb = fmt->bitsperpixel[0] - 1;
	dma_out->order = fmt->hw_order;
	dma_out->plane = fmt->hw_plane;
	dma_out->dma_crop_offset_x = incrop->x;
	dma_out->dma_crop_offset_y = incrop->y;
	dma_out->dma_crop_width = incrop->w;
	dma_out->dma_crop_height = incrop->h;
	dma_out->width = otcrop->w;
	dma_out->height = otcrop->h;
	dma_out->stride_plane0 = node->width;
	dma_out->stride_plane1 = node->width;
	dma_out->stride_plane2 = node->width;
	dma_out->crop_enable = 0;

	if (otcrop->x || otcrop->y)
		mlvwarn("[F%d] Ignore crop pos(%d, %d)", device, node->vid,
				otcrop->x, otcrop->y);

	return 0;
}

static int is_ischain_cstat_is_valid_vid(u32 l_vid, u32 c_vid)
{
	u32 min_vid, max_vid;

	switch (l_vid) {
	case IS_VIDEO_CSTAT0:
		min_vid = IS_LVN_CSTAT0_LME_DS0;
		max_vid = IS_LVN_CSTAT0_CDS;
		break;
	case IS_VIDEO_CSTAT1:
		min_vid = IS_LVN_CSTAT1_LME_DS0;
		max_vid = IS_LVN_CSTAT1_CDS;
		break;
	case IS_VIDEO_CSTAT2:
		min_vid = IS_LVN_CSTAT2_LME_DS0;
		max_vid = IS_LVN_CSTAT2_CDS;
		break;
	case IS_VIDEO_CSTAT3:
		min_vid = IS_LVN_CSTAT3_LME_DS0;
		max_vid = IS_LVN_CSTAT3_CDS;
		break;
	default:
		return 0;
	}

	return (c_vid >= min_vid && c_vid <= max_vid);
}

static int is_ischain_cstat_g_pindex(u32 vid, u32 *pindex)
{
	switch (vid) {
	case IS_LVN_CSTAT0_LME_DS0:
	case IS_LVN_CSTAT1_LME_DS0:
	case IS_LVN_CSTAT2_LME_DS0:
	case IS_LVN_CSTAT3_LME_DS0:
		*pindex = PARAM_CSTAT_LME_DS0;
		return 2;
	case IS_LVN_CSTAT0_LME_DS1:
	case IS_LVN_CSTAT1_LME_DS1:
	case IS_LVN_CSTAT2_LME_DS1:
	case IS_LVN_CSTAT3_LME_DS1:
		*pindex = PARAM_CSTAT_LME_DS1;
		return 2;
	case IS_LVN_CSTAT0_FDPIG:
	case IS_LVN_CSTAT1_FDPIG:
	case IS_LVN_CSTAT2_FDPIG:
	case IS_LVN_CSTAT3_FDPIG:
		*pindex = PARAM_CSTAT_FDPIG;
		return 2;
	case IS_LVN_CSTAT0_RGBHIST:
	case IS_LVN_CSTAT1_RGBHIST:
	case IS_LVN_CSTAT2_RGBHIST:
	case IS_LVN_CSTAT3_RGBHIST:
		*pindex = PARAM_CSTAT_RGBHIST;
		return 2;
	case IS_LVN_CSTAT0_SVHIST:
	case IS_LVN_CSTAT1_SVHIST:
	case IS_LVN_CSTAT2_SVHIST:
	case IS_LVN_CSTAT3_SVHIST:
		*pindex = PARAM_CSTAT_SVHIST;
		return 2;
	case IS_LVN_CSTAT0_DRC:
	case IS_LVN_CSTAT1_DRC:
	case IS_LVN_CSTAT2_DRC:
	case IS_LVN_CSTAT3_DRC:
		*pindex = PARAM_CSTAT_DRC;
		return 2;
	case IS_LVN_CSTAT0_CDS:
	case IS_LVN_CSTAT1_CDS:
	case IS_LVN_CSTAT2_CDS:
	case IS_LVN_CSTAT3_CDS:
		*pindex = PARAM_CSTAT_CDS;
		return 2;
	default:
		*pindex = 0;
		return 0;
	};
}

static int is_ischain_cstat_tag(struct is_subdev *subdev,
		void *device_data,
		struct is_frame *frame,
		struct camera2_node *l_node)
{
	int ret;
	struct is_video_ctx *ivc;
	struct is_device_ischain *device = (struct is_device_ischain *)device_data;
	struct is_group *group;
	struct is_crop *incrop;
	struct camera2_node *node;
	enum is_param pindex;
	struct is_sub_frame *sframe;
	struct is_param_region *param_region;
	u32 calibrated_width, calibrated_height;
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;
	u32 bns_binning_ratio_x, bns_binning_ratio_y;
	u32 binned_sensor_width, binned_sensor_height;
	u32 cap_i, num_plane, s_p, d_p;
	u32 *dva;
	IS_DECLARE_PMAP(pmap);

	ivc = container_of(GET_SUBDEV_QUEUE(subdev), struct is_video_ctx, queue);
	group = ivc->group;
	incrop = (struct is_crop *)l_node->input.cropRegion;
	IS_INIT_PMAP(pmap);

	mdbgs_ischain(4, "CSTAT TAG\n", device);

	if (!test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		frame->cstat_ctx = 0;

	/* control cfg */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_CONTROL, frame->cstat_ctx);
	cstat_ctl_cfg(device, group, pindex, pmap);

	/* OTF in cfg */
	node = &frame->shot_ext->node_group.leader;
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_OTF_INPUT, frame->cstat_ctx);
	ret = cstat_otf_in_cfg(device, group, frame, node, pindex, pmap);
	if (ret) {
		mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid,
				frame->fcount, ret);
		return ret;
	}

	/* DMA in cfg */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_DMA_INPUT, frame->cstat_ctx);
	ret = cstat_dma_in_cfg(device, group, subdev, frame, node, pindex, pmap);
	if (ret) {
		mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid,
				frame->fcount, ret);
		return ret;
	}

	/* DMA out cfg */
	for (cap_i = 0; cap_i < CAPTURE_NODE_MAX; cap_i++) {
		node = &frame->shot_ext->node_group.capture[cap_i];
		if (!is_ischain_cstat_is_valid_vid(subdev->vid, node->vid) ||
				!is_ischain_cstat_g_pindex(node->vid, &pindex))
			continue;

		pindex = GET_CSTAT_CTX_PINDEX(pindex, frame->cstat_ctx);

		if (cstat_dma_out_cfg(device, subdev, frame, node, pindex, pmap))
			continue;

		if (!node->request)
			continue;

		sframe = &frame->cap_node.sframe[cap_i];
		dva = NULL;
		if (is_hw_get_capture_slot(frame, &dva, NULL, node->vid)) {
			mlverr("[F%d] Invalid capture slot", device, node->vid, frame->fcount);
			continue;
		} else if (!dva) {
			mlverr("[F%d] Failed to get dva", device, node->vid, frame->fcount);
			continue;
		}

		num_plane = sframe->num_planes / frame->num_shots;
		for (d_p = 0, s_p = num_plane * frame->cur_shot_idx;
				d_p < num_plane; d_p++, s_p++)
			dva[d_p] = sframe->dva[s_p];
	}

	frame->cstat_ctx++;

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param fail. ret %d", device, frame, ret);
		return ret;
	}

	/* Update Sensor Size meta */
	param_region = &device->is_region->parameter;
	calibrated_width = param_region->sensor.config.calibrated_width;
	calibrated_height = param_region->sensor.config.calibrated_height;
	sensor_binning_ratio_x = param_region->sensor.config.sensor_binning_ratio_x;
	sensor_binning_ratio_y = param_region->sensor.config.sensor_binning_ratio_y;
	binned_sensor_width = calibrated_width * 1000 / sensor_binning_ratio_x;
	binned_sensor_height = calibrated_height * 1000 / sensor_binning_ratio_y;
	bns_binning_ratio_x = param_region->sensor.config.bns_binning_ratio_x;
	bns_binning_ratio_y = param_region->sensor.config.bns_binning_ratio_y;

	frame->shot->udm.frame_info.sensor_size[0] = calibrated_width;
	frame->shot->udm.frame_info.sensor_size[1] = calibrated_height;
	frame->shot->udm.frame_info.sensor_binning[0] = sensor_binning_ratio_x;
	frame->shot->udm.frame_info.sensor_binning[1] = sensor_binning_ratio_y;
	frame->shot->udm.frame_info.bns_binning[0] = bns_binning_ratio_x;
	frame->shot->udm.frame_info.bns_binning[1] = bns_binning_ratio_y;

	/* freeform - use physical sensor crop offset */
	if (param_region->sensor.config.freeform_sensor_crop_enable == true) {
		frame->shot->udm.frame_info.sensor_crop_region[0] =
			(param_region->sensor.config.freeform_sensor_crop_offset_x) & (~0x1);
		frame->shot->udm.frame_info.sensor_crop_region[1] =
			(param_region->sensor.config.freeform_sensor_crop_offset_y) & (~0x1);
	} else { /* center-aglign - use logical sensor crop offset */
		frame->shot->udm.frame_info.sensor_crop_region[0] =
			((binned_sensor_width - param_region->sensor.config.width) >> 1) & (~0x1);
		frame->shot->udm.frame_info.sensor_crop_region[1] =
			((binned_sensor_height - param_region->sensor.config.height) >> 1) & (~0x1);
	}
	frame->shot->udm.frame_info.sensor_crop_region[2] = param_region->sensor.config.width;
	frame->shot->udm.frame_info.sensor_crop_region[3] = param_region->sensor.config.height;

	return 0;
}

const struct is_subdev_ops is_subdev_3aa_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= is_ischain_cstat_tag,
};
