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

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_en,
	u32 *comp_64b_align, u32 *lossy_byte32num,
	u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!sbwc_en) {
		mserr("sbwc_en is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		*sbwc_en = NONE;
		break;
	}

	return 0;
}

void is_subdev_internal_lock_shared_framemgr(struct is_subdev *subdev)
{
}

void is_subdev_internal_unlock_shared_framemgr(struct is_subdev *subdev)
{
}

int is_subdev_internal_get_shared_framemgr(struct is_subdev *subdev,
	struct is_framemgr **framemgr, u32 width, u32 height)
{
	*framemgr = NULL;

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
	return 0;
}

int is_subdev_internal_get_out_node_info(const struct is_subdev *subdev, u32 *num_planes,
	u32 scenario, char *heapname)
{
	switch (subdev->id) {
	default:
		*num_planes = 1;
		break;
	}

	return 0;
}
