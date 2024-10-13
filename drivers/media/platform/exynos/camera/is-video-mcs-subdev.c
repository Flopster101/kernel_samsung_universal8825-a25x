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

static int is_mxp_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	int ret;

	ret = is_vidioc_qbuf(file, fh, b);
	if (ret)
		return ret;

	/* hwfc reset during qbuf timing */
	is_hw_s_ctrl(ivc, ivc->video->id, HW_S_CTRL_HWFC_IDX_RESET, (void *)1);

	return 0;
}

const struct v4l2_ioctl_ops is_mxp_video_ioctl_ops = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_mxp_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_g_ctrl			= is_vidioc_g_ctrl,
	.vidioc_s_ctrl			= is_vidioc_s_ctrl,
};

int is_m0p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m0p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M0P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m0p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(0),
		IS_VIDEO_M0P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_m1p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m1p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M1P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m1p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(1),
		IS_VIDEO_M1P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_m2p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m2p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M2P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m2p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(2),
		IS_VIDEO_M2P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_m3p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m3p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M3P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m3p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(3),
		IS_VIDEO_M3P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_m4p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m4p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M4P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m4p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(4),
		IS_VIDEO_M4P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_m5p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_m5p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_MCS0; /* or MCS1 */
	video->group_ofs = offsetof(struct is_device_ischain, group_mcs);
	video->subdev_id = ENTRY_M5P;
	video->subdev_ofs = offsetof(struct is_device_ischain, m5p);
	video->buf_rdycount = VIDEO_MXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MXP_NAME(5),
		IS_VIDEO_M5P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_mxp_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
