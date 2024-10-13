/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
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

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-param.h"

int is_paf0s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_paf0s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_PAF0;
	video->group_ofs = offsetof(struct is_device_ischain, group_paf);
	video->subdev_id = ENTRY_PAF;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_PAFXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_PAFXS_NAME(0),
		IS_VIDEO_PAF0S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_paf1s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_paf1s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_PAF1;
	video->group_ofs = offsetof(struct is_device_ischain, group_paf);
	video->subdev_id = ENTRY_PAF;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_PAFXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_PAFXS_NAME(1),
		IS_VIDEO_PAF1S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_paf2s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_paf2s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_PAF2;
	video->group_ofs = offsetof(struct is_device_ischain, group_paf);
	video->subdev_id = ENTRY_PAF;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_PAFXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_PAFXS_NAME(2),
		IS_VIDEO_PAF2S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_paf3s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_paf3s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_PAF3;
	video->group_ofs = offsetof(struct is_device_ischain, group_paf);
	video->subdev_id = ENTRY_PAF;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_PAFXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_PAFXS_NAME(3),
		IS_VIDEO_PAF3S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
