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
	return false;
}

void is_subdev_internal_get_sbwc_type(const struct is_subdev *subdev,
					u32 *sbwc_type, u32 *lossy_byte32num)
{
	*lossy_byte32num = 0;

	switch (subdev->id) {
	case ENTRY_ORBXC:
		*sbwc_type = NONE;
		break;
	case ENTRY_IXV:
		*sbwc_type = COMP_LOSS;
		break;
	case ENTRY_IXW:
		*sbwc_type = NONE;
		break;
	default:
		*sbwc_type = NONE;
		break;
	}
}

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	case ENTRY_ORBXC:
		*width = subdev->output.width;
		*height = subdev->output.height;
		break;
	case ENTRY_IXV:
		*width = subdev->output.width;
		*height = subdev->output.height;
		*sbwc_block_width = MCFP_COMP_BLOCK_WIDTH;
		*sbwc_block_height = MCFP_COMP_BLOCK_HEIGHT;
		break;
	case ENTRY_IXW:
		*width = subdev->output.width / 2;
		*height = subdev->output.height / 2 + 4;
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
	/* Not use */
}

void is_subdev_internal_unlock_shared_framemgr(struct is_subdev *subdev)
{
	/* Not use */
}

int is_subdev_internal_get_shared_framemgr(struct is_subdev *subdev,
	struct is_framemgr **framemgr, u32 width, u32 height)
{
	return 0;
}

int is_subdev_internal_put_shared_framemgr(struct is_subdev *subdev)
{
	return 0;
}

int is_subdev_internal_get_cap_node_num(const struct is_subdev *subdev)
{
	return 0;
}

int is_subdev_internal_get_cap_node_info(const struct is_subdev *subdev, u32 *vid,
	u32 *num_planes, u32 *buffer_size, u32 index, u32 scenario, char *heapname)
{
	int ret = 0;

	switch (subdev->id) {
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
	case ENTRY_IXV:
		*num_planes = 1;
		if (scenario == IS_SCENARIO_SECURE)
			snprintf(heapname, 21, SECURE_HEAPNAME);
		break;
	default:
		*num_planes = 1;
		break;
	}

	return ret;
}
