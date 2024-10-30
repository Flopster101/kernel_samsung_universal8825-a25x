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

#include <linux/module.h>

#include "is-core.h"
#include "is-param.h"
#include "is-device-ischain.h"
#include "is-debug.h"
#include "pablo-mem.h"
#include "is-hw-common-dma.h"

bool is_subdev_internal_use_shared_framemgr(const struct is_subdev *subdev)
{
	switch (subdev->id) {
	case ENTRY_MCSC:
		return true;
	default:
		return false;
	}
}

void is_subdev_internal_get_sbwc_type(const struct is_subdev *subdev,
					u32 *sbwc_type, u32 *lossy_byte32num)
{
	*lossy_byte32num = 0;

	switch (subdev->id) {
	case ENTRY_MCSC:
		*sbwc_type = NONE;
		break;
	case ENTRY_SSVC0:
		*sbwc_type = COMP;
		break;
	default:
		*sbwc_type = NONE;
	}
}

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	struct is_core *core = is_get_is_core();

	if (!core) {
		mserr("core is NULL", subdev, subdev);
		return -ENODEV;
	}

	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	case ENTRY_MCSC:
		*width = subdev->output.width;
		*height = subdev->output.height;
		if (core->vender.isp_max_width)
			*width = core->vender.isp_max_width;
		if (core->vender.isp_max_height)
			*height = core->vender.isp_max_height;
		break;
	case ENTRY_MCFP_VIDEO:
	case ENTRY_MCFP_STILL:
		*width = subdev->output.width;
		*height = subdev->output.height;
		if (sbwc_block_width)
			*sbwc_block_width = MCFP_COMP_BLOCK_WIDTH;
		if (sbwc_block_height)
			*sbwc_block_height = MCFP_COMP_BLOCK_HEIGHT;
		break;
	case ENTRY_SSVC0:
		*width = subdev->output.width;
		*height = subdev->output.height;
		*sbwc_block_width = CSIS_COMP_BLOCK_WIDTH;
		*sbwc_block_height = CSIS_COMP_BLOCK_HEIGHT;
		break;
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		break;
	}

	return 0;
}

void is_subdev_internal_lock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	int group_id;

	switch (subdev->id) {
	case ENTRY_MCSC:
		group_id = GROUP_ID_MCS0;
		break;
	default:
		return;
	}

	if (subdev->use_shared_framemgr) {
		core = is_get_is_core();
		if (!core) {
			mserr("There is no pablo core device", subdev, subdev);
			return;
		}

		groupmgr = &core->groupmgr;

		mutex_lock(&groupmgr->shared_framemgr[group_id].mlock);
	}
}

void is_subdev_internal_unlock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	int group_id;

	switch (subdev->id) {
	case ENTRY_MCSC:
		group_id = GROUP_ID_MCS0;
		break;
	default:
		return;
	}

	if (subdev->use_shared_framemgr) {
		core = is_get_is_core();
		if (!core) {
			mserr("There is no pablo core device", subdev, subdev);
			return;
		}

		groupmgr = &core->groupmgr;

		mutex_unlock(&groupmgr->shared_framemgr[group_id].mlock);
	}
}

int is_subdev_internal_get_shared_framemgr(struct is_subdev *subdev,
	struct is_framemgr **framemgr, u32 width, u32 height)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	int prev_refcount;
	int group_id;
	struct is_shared_framemgr *shared_framemgr;

	switch (subdev->id) {
	case ENTRY_MCSC:
		group_id = GROUP_ID_MCS0;
		break;
	default:
		*framemgr = NULL;
		return 0;
	}

	if (subdev->use_shared_framemgr) {
		core = is_get_is_core();
		if (!core) {
			mserr("core is NULL", subdev, subdev);
			return -ENODEV;
		}

		groupmgr = &core->groupmgr;
		shared_framemgr = &groupmgr->shared_framemgr[group_id];

		prev_refcount = refcount_read(&shared_framemgr->refcount);
		if (!prev_refcount) {
			ret = frame_manager_open(&shared_framemgr->framemgr,
					subdev->buffer_num);
			if (ret < 0) {
				mserr("frame_manager_open is fail(%d)",
						subdev, subdev, ret);
				return ret;
			}
			shared_framemgr->width = width;
			shared_framemgr->height = height;
			refcount_set(&shared_framemgr->refcount, 1);
		} else {
			if (shared_framemgr->width != width ||
					shared_framemgr->height != height) {
				mserr("shared_framemgr size(%d x %d) is not matched with (%d x %d)\n",
						subdev, subdev,
						shared_framemgr->width, shared_framemgr->height,
						width, height);
				return -EINVAL;
			}
			refcount_inc(&shared_framemgr->refcount);
		}
		*framemgr = &shared_framemgr->framemgr;

		return prev_refcount;
	}

	return ret;
}

int is_subdev_internal_put_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	struct is_shared_framemgr *shared_framemgr;
	int group_id;

	switch (subdev->id) {
	case ENTRY_MCSC:
		group_id = GROUP_ID_MCS0;
		break;
	default:
		return 0;
	}

	if (subdev->use_shared_framemgr) {
		core = is_get_is_core();
		if (!core) {
			mserr("core is NULL", subdev, subdev);
			return -ENODEV;
		}

		groupmgr = &core->groupmgr;
		shared_framemgr = &groupmgr->shared_framemgr[GROUP_ID_MCS0];

		if (refcount_dec_and_test(&shared_framemgr->refcount))
			frame_manager_close(&shared_framemgr->framemgr);

		return refcount_read(&shared_framemgr->refcount);
	}

	return 0;
}

int is_subdev_internal_get_cap_node_num(const struct is_subdev *subdev)
{
	switch (subdev->id) {
	case ENTRY_MCSC:
		return 6;
	case ENTRY_MCFP_VIDEO:
	case ENTRY_MCFP_STILL:
		return 8;
	case ENTRY_LME_MBMV:
		return 2;
	default:
		return 0;
	}
}

int is_subdev_internal_get_cap_node_info(const struct is_subdev *subdev, u32 *vid,
	u32 *num_planes, u32 *buffer_size, u32 index, u32 scenario, char *heapname)
{
	int ret = 0;
	u32 width;
	u32 height;
	u32 payload_size, header_size;
	u32 sbwc_block_width, sbwc_block_height;
	u32 sbwc_type, sbwc_en, comp_64b_align;

	is_subdev_internal_get_buffer_size(subdev, &width, &height,
						&sbwc_block_width, &sbwc_block_height);

	switch (subdev->id) {
	case ENTRY_MCSC:
		*vid = IS_LVN_MCSC_P0 + index;
		switch (*vid) {
		case IS_LVN_MCSC_P0:
		case IS_LVN_MCSC_P1:
		case IS_LVN_MCSC_P2:
		case IS_LVN_MCSC_P3:
		case IS_LVN_MCSC_P4:
			*num_planes = 0;
			buffer_size[0] = 0;
			break;
		case IS_LVN_MCSC_P5:
			*num_planes = 1;
			buffer_size[0] = width * height;
			break;
		default:
			mserr("invalid vid(%d)",
					subdev, subdev, *vid);
			return -EINVAL;
		}
		break;
	case ENTRY_MCFP_VIDEO:
	case ENTRY_MCFP_STILL:
		*vid = IS_LVN_MCFP_PREV_YUV + index;
		buffer_size[0] = buffer_size[1] = 0;
		switch (*vid) {
		case IS_LVN_MCFP_PREV_YUV:
			*num_planes = 2;
			if (subdev->id == ENTRY_MCFP_VIDEO) {
				/* Y plane */
				sbwc_type = subdev->sbwc_type;
				if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
					sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
				else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
					sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

				sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);

				if (sbwc_en == NONE) {
					sbwc_en = COMP;
					comp_64b_align = 1;
				}

				payload_size = is_hw_dma_get_payload_stride(
					sbwc_en, subdev->bits_per_pixel, width,
					comp_64b_align, subdev->lossy_byte32num,
					sbwc_block_width, sbwc_block_height) *
					DIV_ROUND_UP(height, sbwc_block_height);
				header_size = (sbwc_en == 1) ? is_hw_dma_get_header_stride(width,
						sbwc_block_width, 16) *
						DIV_ROUND_UP(height, sbwc_block_height) : 0;
				buffer_size[0] = payload_size + header_size;

				/* UV plane */
				sbwc_type = subdev->sbwc_type;
				if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
					sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
				else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
					sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

				sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);

				if (sbwc_en == NONE) {
					sbwc_en = COMP;
					comp_64b_align = 1;
				}

				payload_size = is_hw_dma_get_payload_stride(
					sbwc_en, subdev->bits_per_pixel, width,
					comp_64b_align, subdev->lossy_byte32num,
					sbwc_block_width, sbwc_block_height) *
					DIV_ROUND_UP(height, sbwc_block_height);
				header_size = (sbwc_en == 1) ? is_hw_dma_get_header_stride(width,
						sbwc_block_width, 16) *
						DIV_ROUND_UP(height, sbwc_block_height) : 0;
				buffer_size[1] = payload_size + header_size;
			}

			if (scenario == IS_SCENARIO_SECURE)
				snprintf(heapname, 21, SECURE_HEAPNAME);

			break;
		case IS_LVN_MCFP_PREV_W:
			*num_planes = 1;
			buffer_size[0] = ALIGN(width / 2, 16) * ((height / 2) + 4);
			break;
		case IS_LVN_MCFP_DRC:
		case IS_LVN_MCFP_DP:
			break;
		case IS_LVN_MCFP_MV:
			*num_planes = 0;
			break;
		case IS_LVN_MCFP_SF:
			if (subdev->id == ENTRY_MCFP_VIDEO) {
				*num_planes = 0;
			} else {
				*num_planes = 1;
				buffer_size[0] = ALIGN((width + 7) / 8, 16) * height;
			}
			break;
		case IS_LVN_MCFP_W:
			*num_planes = 1;
			break;
		case IS_LVN_MCFP_YUV:
			*num_planes = 2;
			break;
		default:
			mserr("invalid vid(%d)",
				subdev, subdev, *vid);
			return -EINVAL;
		}

		if (*num_planes == 1)
			msinfo("[vid:%d]MCFP internal buffer size %d\n",
				subdev, subdev, *vid, buffer_size[0]);
		else if (*num_planes == 2)
			msinfo("[vid:%d]MCFP internal buffer size Y : %d, UV : %d\n",
				subdev, subdev, *vid, buffer_size[0], buffer_size[1]);
		break;
	case ENTRY_LME_MBMV:
		*vid = IS_LVN_LME_MBMV0 + index;
		*num_planes = 1;
		/* 2 byte per 16x16 pixels */
		buffer_size[0] = LME_IMAGE_MAX_WIDTH/16 * LME_IMAGE_MAX_HEIGHT/16 * 2;
		break;
	default:
		mserr("invalid subdev id", subdev, subdev);
		return -EINVAL;
	}

	return ret;
}

int is_subdev_internal_get_out_node_info(const struct is_subdev *subdev, u32 *num_planes,
	u32 scenario, char *heapname)
{
	switch (subdev->id) {
	case ENTRY_MCSC:
		*num_planes = 1;
		break;
	case ENTRY_MCFP_VIDEO:
	case ENTRY_MCFP_STILL:
		*num_planes = 0;
		break;
	case ENTRY_LME_MBMV:
		*num_planes = 1;
		break;
	default:
		*num_planes = 1;
		break;
	}

	return 0;
}
