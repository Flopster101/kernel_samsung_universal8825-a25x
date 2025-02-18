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

int is_30c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AC;
	video->subdev_ofs = offsetof(struct is_device_ischain, txc);
	video->buf_rdycount = VIDEO_3XC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XC_NAME(0),
		IS_VIDEO_30C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AC;
	video->subdev_ofs = offsetof(struct is_device_ischain, txc);
	video->buf_rdycount = VIDEO_3XC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XC_NAME(1),
		IS_VIDEO_31C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AC;
	video->subdev_ofs = offsetof(struct is_device_ischain, txc);
	video->buf_rdycount = VIDEO_3XC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XC_NAME(2),
		IS_VIDEO_32C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AC;
	video->subdev_ofs = offsetof(struct is_device_ischain, txc);
	video->buf_rdycount = VIDEO_3XC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XC_NAME(3),
		IS_VIDEO_33C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_30f_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30f;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AF;
	video->subdev_ofs = offsetof(struct is_device_ischain, txf);
	video->buf_rdycount = VIDEO_3XF_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XF_NAME(0),
		IS_VIDEO_30F_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31f_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31f;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AF;
	video->subdev_ofs = offsetof(struct is_device_ischain, txf);
	video->buf_rdycount = VIDEO_3XF_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XF_NAME(1),
		IS_VIDEO_31F_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32f_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32f;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AF;
	video->subdev_ofs = offsetof(struct is_device_ischain, txf);
	video->buf_rdycount = VIDEO_3XF_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XF_NAME(2),
		IS_VIDEO_32F_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33f_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33f;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AF;
	video->subdev_ofs = offsetof(struct is_device_ischain, txf);
	video->buf_rdycount = VIDEO_3XF_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XF_NAME(3),
		IS_VIDEO_33F_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_30g_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30g;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AG;
	video->subdev_ofs = offsetof(struct is_device_ischain, txg);
	video->buf_rdycount = VIDEO_3XG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XG_NAME(0),
		IS_VIDEO_30G_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31g_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31g;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AG;
	video->subdev_ofs = offsetof(struct is_device_ischain, txg);
	video->buf_rdycount = VIDEO_3XG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XG_NAME(1),
		IS_VIDEO_31G_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32g_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32g;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AG;
	video->subdev_ofs = offsetof(struct is_device_ischain, txg);
	video->buf_rdycount = VIDEO_3XG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XG_NAME(2),
		IS_VIDEO_32G_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33g_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33g;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AG;
	video->subdev_ofs = offsetof(struct is_device_ischain, txg);
	video->buf_rdycount = VIDEO_3XG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XG_NAME(3),
		IS_VIDEO_33G_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_30l_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30l;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AL;
	video->subdev_ofs = offsetof(struct is_device_ischain, txl);
	video->buf_rdycount = VIDEO_3XL_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XL_NAME(0),
		IS_VIDEO_30L_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31l_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31l;
	video->resourcemgr = &core->resourcemgr;

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AL;
	video->subdev_ofs = offsetof(struct is_device_ischain, txl);
	video->buf_rdycount = VIDEO_3XL_READY_BUFFERS;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_video_probe(video,
		IS_VIDEO_3XL_NAME(1),
		IS_VIDEO_31L_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32l_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32l;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AL;
	video->subdev_ofs = offsetof(struct is_device_ischain, txl);
	video->buf_rdycount = VIDEO_3XL_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XL_NAME(2),
		IS_VIDEO_32L_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33l_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33l;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AL;
	video->subdev_ofs = offsetof(struct is_device_ischain, txl);
	video->buf_rdycount = VIDEO_3XL_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XL_NAME(3),
		IS_VIDEO_33L_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_30o_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30o;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AO;
	video->subdev_ofs = offsetof(struct is_device_ischain, txo);
	video->buf_rdycount = VIDEO_3XO_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XO_NAME(0),
		IS_VIDEO_30O_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31o_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31o;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AO;
	video->subdev_ofs = offsetof(struct is_device_ischain, txo);
	video->buf_rdycount = VIDEO_3XO_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XO_NAME(1),
		IS_VIDEO_31O_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32o_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32o;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AO;
	video->subdev_ofs = offsetof(struct is_device_ischain, txo);
	video->buf_rdycount = VIDEO_3XO_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XO_NAME(2),
		IS_VIDEO_32O_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33o_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33o;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AO;
	video->subdev_ofs = offsetof(struct is_device_ischain, txo);
	video->buf_rdycount = VIDEO_3XO_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XO_NAME(3),
		IS_VIDEO_33O_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_30p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AP;
	video->subdev_ofs = offsetof(struct is_device_ischain, txp);
	video->buf_rdycount = VIDEO_3XP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XP_NAME(0),
		IS_VIDEO_30P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AP;
	video->subdev_ofs = offsetof(struct is_device_ischain, txp);
	video->buf_rdycount = VIDEO_3XP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XP_NAME(1),
		IS_VIDEO_31P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AP;
	video->subdev_ofs = offsetof(struct is_device_ischain, txp);
	video->buf_rdycount = VIDEO_3XP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XP_NAME(2),
		IS_VIDEO_32P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AP;
	video->subdev_ofs = offsetof(struct is_device_ischain, txp);
	video->buf_rdycount = VIDEO_3XP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XP_NAME(3),
		IS_VIDEO_33P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

static int is_orbxc_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = vq->drv_priv;
	struct is_queue *iq = &ivc->queue;

	set_bit(IS_QUEUE_NEED_TO_KMAP, &iq->state);

	return is_queue_setup(vq, nbuffers, nplanes, sizes, alloc_devs);
}

const struct vb2_ops is_orbxc_vb2_ops = {
	.queue_setup		= is_orbxc_queue_setup,
	.wait_prepare		= is_wait_prepare,
	.wait_finish		= is_wait_finish,
	.buf_init		= is_buf_init,
	.buf_prepare		= is_buf_prepare,
	.buf_finish		= is_buf_finish,
	.buf_cleanup		= is_buf_cleanup,
	.start_streaming	= is_start_streaming,
	.stop_streaming		= is_stop_streaming,
	.buf_queue		= is_buf_queue,
};

int is_orb0c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_orb0c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_ORBXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, orbxc);
	video->buf_rdycount = VIDEO_ORBXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_ORBXC_NAME(0),
		IS_VIDEO_ORB0C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		NULL,
		&is_orbxc_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_orb1c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_orb1c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_ORBXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, orbxc);
	video->buf_rdycount = VIDEO_ORBXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_ORBXC_NAME(1),
		IS_VIDEO_ORB1C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		NULL,
		&is_orbxc_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
