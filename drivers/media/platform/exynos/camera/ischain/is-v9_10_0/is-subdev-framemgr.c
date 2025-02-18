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
	struct is_core *core;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;

	switch (subdev->id) {
	case ENTRY_YPP:
		core = is_get_is_core();
		device = GET_DEVICE(subdev->vctx);
		sensor = device->sensor;

		if (!sensor || !sensor->cfg) {
			mswarn(" sensor config is not valid", subdev, subdev);
			return false;
		}

		if (core->scenario == IS_SCENARIO_SECURE ||
			sensor->cfg->special_mode == IS_SPECIAL_MODE_FASTAE)
			return false;
		else
			return true;
		fallthrough;
	default:
		return false;
	}
}

void is_subdev_internal_get_sbwc_type(const struct is_subdev *subdev,
					u32 *sbwc_type, u32 *lossy_byte32num)
{
	*lossy_byte32num = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		*sbwc_type = COMP;
		break;
	default:
		*sbwc_type = NONE;
		break;
	}
}

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	struct is_core *core = is_get_is_core();
	u32 max_width, max_height;

	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			is_sensor_g_max_size(&max_width, &max_height);

			if (core->hardware.ypp_internal_buf_max_width)
				*width = MIN(max_width, core->hardware.ypp_internal_buf_max_width);
			else
				*width = MIN(max_width, subdev->leader->constraints_width);

			*height = max_height;

			if (core->vender.isp_max_width)
				*width = MIN(*width, core->vender.isp_max_width);
			if (core->vender.isp_max_height)
				*height = core->vender.isp_max_height;
		} else {
			*width = MIN(subdev->output.width, subdev->leader->constraints_width);
			*height = subdev->output.height;
		}
		if (sbwc_block_width)
			*sbwc_block_width = YPP_COMP_BLOCK_WIDTH;
		if (sbwc_block_height)
			*sbwc_block_height = YPP_COMP_BLOCK_HEIGHT;

		break;
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		if (sbwc_block_width)
			*sbwc_block_width = 0;
		if (sbwc_block_height)
			*sbwc_block_height = 0;

		break;
	}

	return 0;
}

void is_subdev_internal_lock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = is_get_is_core();
			groupmgr = &core->groupmgr;

			mutex_lock(&groupmgr->shared_framemgr[GROUP_ID_YPP].mlock);
		}
		break;
	default:
		break;
	}
}

void is_subdev_internal_unlock_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = is_get_is_core();
			groupmgr = &core->groupmgr;

			mutex_unlock(&groupmgr->shared_framemgr[GROUP_ID_YPP].mlock);
		}
		break;
	default:
		break;
	}
}

int is_subdev_internal_get_shared_framemgr(struct is_subdev *subdev,
	struct is_framemgr **framemgr, u32 width, u32 height)
{
	int ret = 0;
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	int prev_refcount;
	struct is_shared_framemgr *shared_framemgr;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = is_get_is_core();
			groupmgr = &core->groupmgr;
			shared_framemgr = &groupmgr->shared_framemgr[GROUP_ID_YPP];

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
		fallthrough;
	default:
		*framemgr = NULL;
		break;
	}

	return ret;
}

int is_subdev_internal_put_shared_framemgr(struct is_subdev *subdev)
{
	struct is_groupmgr *groupmgr;
	struct is_core *core;
	struct is_shared_framemgr *shared_framemgr;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			core = is_get_is_core();
			groupmgr = &core->groupmgr;
			shared_framemgr = &groupmgr->shared_framemgr[GROUP_ID_YPP];

			if (refcount_dec_and_test(&shared_framemgr->refcount))
				frame_manager_close(&shared_framemgr->framemgr);

			return refcount_read(&shared_framemgr->refcount);
		}
		fallthrough;
	default:
		break;
	}

	return 0;
}

int is_subdev_internal_get_cap_node_num(const struct is_subdev *subdev)
{
	int ret = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		if (IS_ENABLED(YPP_DDK_LIB_CALL))
			ret = 4;
		else
			ret = 0;

		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int is_subdev_internal_get_cap_node_info(const struct is_subdev *subdev, u32 *vid,
	u32 *num_planes, u32 *buffer_size, u32 index, u32 scenario, char *heapname)
{
	int ret = 0;
	u32 l0_width;
	u32 l0_height;
	u32 l2_width;
	u32 l2_height;
	u32 payload_size, header_size;

	*vid = IS_VIDEO_YND_NUM + index;
	is_subdev_internal_get_buffer_size(subdev, &l0_width, &l0_height, NULL, NULL);
	l2_width = (((l0_width + 2) >> 2) + 1);
	l2_height = (((l0_height + 2) >> 2) + 1);

	switch (subdev->id) {
	case ENTRY_YPP:
		switch (*vid) {
		case IS_VIDEO_YND_NUM:
			*num_planes = 2;
			buffer_size[0] = buffer_size[1] =
				ALIGN(DIV_ROUND_UP(l2_width * subdev->bits_per_pixel, BITS_PER_BYTE), 16) * l2_height;
			if (scenario == IS_SCENARIO_SECURE)
				snprintf(heapname, 21, SECURE_HEAPNAME);
			break;
		case IS_VIDEO_YDG_NUM:
			*num_planes = 1;
			buffer_size[0] =
				ALIGN(DIV_ROUND_UP(l2_width * subdev->bits_per_pixel, BITS_PER_BYTE), 16) * l2_height;
			break;
		case IS_VIDEO_YSH_NUM:
			*num_planes = 1;
			buffer_size[0] = 2048 * 144;
			break;
		case IS_VIDEO_YNS_NUM:
			*num_planes = 1;
			payload_size = is_hw_dma_get_payload_stride(COMP, subdev->bits_per_pixel, l0_width,
				0, 0, 256, 1) * l0_height;
			header_size = is_hw_dma_get_header_stride(l0_width, 256, 16) * l0_height;
			buffer_size[0] = payload_size + header_size;
			break;
		default:
			mserr("invalid vid(%d)",
				subdev, subdev, *vid);
			return -EINVAL;
		}
		break;
	default:
		mserr("invalid subdev id",
			subdev, subdev);
		return -EINVAL;
	}

	return ret;
}

int is_subdev_internal_get_out_node_info(const struct is_subdev *subdev, u32 *num_planes,
	u32 scenario, char *heapname)
{
	int ret = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		*num_planes = 2;
		if (scenario == IS_SCENARIO_SECURE)
			snprintf(heapname, 21, SECURE_HEAPNAME);
		break;
	default:
		*num_planes = 1;
		break;
	}

	return ret;
}
