// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/pm_qos.h>
#include <linux/bug.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

int is_lme0_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme0;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME0;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LME;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_LME_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LME_NAME(0),
		IS_VIDEO_LME0_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_lme1_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme1;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME1;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LME;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_LME_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LME_NAME(1),
		IS_VIDEO_LME1_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
