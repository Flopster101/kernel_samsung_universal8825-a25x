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
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

int is_lme0c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme0c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME0;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMEC;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmec);
	video->buf_rdycount = VIDEO_LMEXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXC_NAME(0),
		IS_VIDEO_LME0C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_lme1c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme1c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME1;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMEC;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmec);
	video->buf_rdycount = VIDEO_LMEXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXC_NAME(1),
		IS_VIDEO_LME1C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_lme0s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme0s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME0;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMES;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmes);
	video->buf_rdycount = VIDEO_LMEXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXS_NAME(0),
		IS_VIDEO_LME0S_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_lme1s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_lme1s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_LME1;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMES;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmes);
	video->buf_rdycount = VIDEO_LMEXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXS_NAME(1),
		IS_VIDEO_LME1S_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
