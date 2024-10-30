// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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

static int __lme_dma_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	struct param_lme_dma *dma,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_control *control;
	struct is_group *group;
	struct is_crop *incrop, *otcrop;
	u32 hw_msb, hw_order, hw_plane;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	int ret = 0;
	u32 i;
	u32 *dma_cmd;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	switch (node->vid) {
	case IS_VIDEO_LME: /* curr */
		dma_cmd = &dma->cur_input_cmd;
		break;
	case IS_LVN_LME_PREV: /* prev */
		dma_cmd = &dma->prev_input_cmd;
		break;
	case IS_LVN_LME_PURE: /* mv out */
		dma_cmd = &dma->output_cmd;
		break;
	case IS_LVN_LME_SAD: /* sad out */
		dma_cmd = &dma->sad_output_cmd;
		break;
	case IS_LVN_LME_PROCESSED: /* proc mv from rta */
		dma_cmd = &dma->processed_output_cmd;
		break;
	case IS_LVN_LME_MBMV0: /* MBMV0 */
	case IS_LVN_LME_MBMV1: /* MBMV1 */
	case IS_LVN_LME_PROCESSED_HDR: /* S/W LME out for stitch hdr */
		return 0;
	default:
		merr("vid(%d) is invalid", device, node->vid);
		return -EINVAL;
	}

	if (*dma_cmd != node->request)
		mlvinfo("[F%d] DMA enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			*dma_cmd, node->request);

	if (!node->request) {
		*dma_cmd = DMA_OUTPUT_COMMAND_DISABLE;

		if (node->vid == IS_LVN_LME_PURE) {
			for (i = 0; i < IS_MAX_PLANES; i++)
				ldr_frame->lmecKTargetAddress[i] = 0;
		}

		set_bit(PARAM_LME_DMA, pmap);

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

	group = &device->group_lme;
	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;

	switch (node->vid) {
	case IS_VIDEO_LME:
		control = is_itf_g_param(device, ldr_frame, PARAM_LME_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		set_bit(PARAM_LME_CONTROL, pmap);
		dma->cur_input_cmd = DMA_INPUT_COMMAND_ENABLE;
		dma->input_format = DMA_INOUT_FORMAT_Y;
		dma->input_bitwidth = hw_bitwidth;
		dma->input_order = hw_order;
		dma->input_msb = hw_msb;
		dma->input_plane = hw_plane;
		dma->cur_input_width = incrop->w;
		dma->cur_input_height = incrop->h;
		set_bit(pindex, pmap);
		break;
	case IS_LVN_LME_PREV:
		dma->prev_input_cmd = DMA_INPUT_COMMAND_ENABLE;
		dma->prev_input_width = incrop->w;
		dma->prev_input_height = incrop->h;
		break;
	case IS_LVN_LME_PURE:
		dma->output_cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma->output_order = hw_order;
		dma->output_bitwidth = hw_bitwidth;
		dma->output_plane = hw_plane;
		dma->output_msb = hw_msb;
		dma->output_width = otcrop->w;
		dma->output_height = otcrop->h;
		break;
	case IS_LVN_LME_SAD:
		dma->sad_output_cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma->sad_output_plane = hw_plane;
		break;
	case IS_LVN_LME_PROCESSED:
		dma->processed_output_cmd = DMA_OUTPUT_COMMAND_ENABLE;
		break;
	}

	return ret;
}

static int is_ischain_lme_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_lme_dma *dma_input;
	struct param_control *control;
	struct is_device_ischain *device;
	u32 hw_format = DMA_INOUT_FORMAT_Y;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	u32 hw_plane = DMA_INOUT_PLANE_1;
	u32 hw_order = DMA_INOUT_ORDER_NO;
	u32 width, height;


	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);

	width = incrop->w;
	height = incrop->h;
	group = &device->group_lme;
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
	}

	/* Configure Conrtol */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_LME_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}

		set_bit(PARAM_LME_CONTROL, pmap);
	}

	dma_input = is_itf_g_param(device, frame, PARAM_LME_DMA);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->cur_input_cmd = DMA_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->cur_input_cmd = DMA_INPUT_COMMAND_ENABLE;
	}

	dma_input->input_format = hw_format;
	dma_input->input_bitwidth = hw_bitwidth;
	dma_input->input_order = hw_order;
	dma_input->input_msb = hw_bitwidth - 1;
	dma_input->input_plane = hw_plane;
	dma_input->cur_input_width = width;
	dma_input->cur_input_height = height;

	set_bit(PARAM_LME_DMA, pmap);

p_err:
	return ret;
}

static int __lme_internal_buffer_cfg(struct is_group *group,
	struct is_device_ischain *device,
	struct is_frame *frame)
{
	int ret = 0;
	struct is_framemgr *internal_framemgr = NULL;
	struct is_frame *internal_frame = NULL;
	struct is_subdev *subdev_lme;
	struct is_sub_frame *sframe;
	u32 cap_node_num = 0;
	u32 *targetAddr, num_planes;
	u64 *targetAddr_k;
	int j, i, buffer_idx;

	subdev_lme = group->subdev[ENTRY_LME_MBMV];

	internal_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev_lme);
	if (internal_framemgr) {
		for (buffer_idx = 0; buffer_idx < LME_TNR_MODE_MIN_BUFFER_NUM; buffer_idx++) {
			internal_frame = &internal_framemgr->frames[buffer_idx];
			if (!internal_frame) {
				mrerr("internal frame is NULL", device, frame);
				return -EINVAL;
			}
			cap_node_num = is_subdev_internal_get_cap_node_num(subdev_lme);
			for (i = 0; i < cap_node_num; i++) {
				sframe = &internal_frame->cap_node.sframe[i];

				if (!sframe->id || sframe->id != IS_LVN_LME_MBMV0 + buffer_idx)
					continue;

				targetAddr = NULL;
				targetAddr_k = NULL;
				ret = is_hw_get_capture_slot(frame, &targetAddr,
						&targetAddr_k, sframe->id);
				if (ret) {
					mrerr("Invalid capture slot(%d)", device, frame,
							sframe->id);
					continue;
				}
				num_planes = sframe->num_planes;

				if (targetAddr) {
					for (j = 0; j < num_planes; j++)
						targetAddr[j] = sframe->dva[j];
				}
				if (targetAddr_k) {
					for (j = 0; j < num_planes; j++)
						targetAddr_k[j] = sframe->kva[j];
				}
			}
		}
	} else
		mrerr("internal framemgr is NULL", device, frame);

	return ret;
}

static int is_ischain_lme_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct lme_param *lme_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	struct is_queue *queue;
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	struct param_lme_dma *dma = NULL;
	IS_DECLARE_PMAP(pmap);
	u32 *targetAddr, num_planes;
	u64 *targetAddr_k;
	int j, i, p;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "LME TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_lme;
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	lme_param = &device->is_region->parameter.lme;

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
		dma = is_itf_g_param(device, frame, PARAM_LME_DMA);
		ret = __lme_dma_cfg(device, subdev, frame, out_node, dma, PARAM_LME_DMA, pmap);
		if (ret) {
			mlverr("[F%d] lme_dma_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			cap_node = &frame->shot_ext->node_group.capture[i];
			if (!cap_node->vid)
				continue;

			ret = __lme_dma_cfg(device, subdev, frame, cap_node, dma, PARAM_LME_DMA, pmap);
			if (ret) {
				mlverr("[F%d] lme_dma_cfg fail. ret %d", device, node->vid,
						frame->fcount, ret);
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

			if ((sframe->id < IS_LVN_LME_PREV) ||
					(sframe->id > IS_LVN_LME_PROCESSED_HDR))
				continue;

			targetAddr = NULL;
			targetAddr_k = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr,
					&targetAddr_k, sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device, frame,
						sframe->id);
				return -EINVAL;
			}

			num_planes = sframe->num_planes / frame->num_shots;

			/* IS_LVN_LME_PREV */
			/* IS_LVN_LME_PURE */
			if (targetAddr) {
				for (j = 0, p = num_planes * frame->cur_shot_idx;
						j < num_planes; j++, p++)
					targetAddr[j] = sframe->dva[p];
			}

			/* IS_LVN_LME_PROCESSED */
			/* IS_VIDEO_LME0C_NUM */
			/* IS_VIDEO_LME0M_NUM */
			if (targetAddr_k) {
				for (j = 0, p = num_planes * frame->cur_shot_idx;
						j < num_planes; j++, p++) {
					targetAddr_k[j] = sframe->kva[p];
				}
			}
		}

		__lme_internal_buffer_cfg(group, device, frame);

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
	inparm.w = lme_param->dma.cur_input_width;
	inparm.h = lme_param->dma.cur_input_height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_lme_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			pmap);
		if (ret) {
			merr("is_ischain_lme_cfg is fail(%d)", device, ret);
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

const struct is_subdev_ops is_subdev_lme_ops = {
	.bypass	= NULL,
	.cfg	= is_ischain_lme_cfg,
	.tag	= is_ischain_lme_tag,
};
