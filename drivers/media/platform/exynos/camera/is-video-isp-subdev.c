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

int is_i0c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixc);
	video->buf_rdycount = VIDEO_IXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXC_NAME(0),
		IS_VIDEO_I0C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_i1c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i1c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP1;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixc);
	video->buf_rdycount = VIDEO_IXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXC_NAME(1),
		IS_VIDEO_I1C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/* Slave node for TNR weight */
int is_i0g_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0g;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXG;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixg);
	video->buf_rdycount = VIDEO_IXG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXG_NAME(0),
		IS_VIDEO_I0G_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_i0p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXP;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixp);
	video->buf_rdycount = VIDEO_IXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXP_NAME(0),
		IS_VIDEO_I0P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_i1p_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i1p;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP1;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXP;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixp);
	video->buf_rdycount = VIDEO_IXP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXP_NAME(1),
		IS_VIDEO_I1P_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/* Slave node for TNR */
int is_i0t_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0t;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXT;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixt);
	video->buf_rdycount = VIDEO_IXT_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXT_NAME(0),
		IS_VIDEO_I0T_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/* Slave node for TNR weight */
int is_i0v_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0v;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXV;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixv);
	video->buf_rdycount = VIDEO_IXV_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXV_NAME(0),
		IS_VIDEO_I0V_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/* Slave node for TNR weight */
int is_i0w_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_i0w;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_IXW;
	video->subdev_ofs = offsetof(struct is_device_ischain, ixw);
	video->buf_rdycount = VIDEO_IXW_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_IXW_NAME(0),
		IS_VIDEO_I0W_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

static int is_mexc_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = vq->drv_priv;
	struct is_queue *iq = &ivc->queue;

	set_bit(IS_QUEUE_NEED_TO_KMAP, &iq->state);

	return is_queue_setup(vq, nbuffers, nplanes, sizes, alloc_devs);
}

const struct vb2_ops is_mexc_vb2_ops = {
	.queue_setup		= is_mexc_queue_setup,
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

int is_me0c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_me0c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP0;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_MEXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, mexc);
	video->buf_rdycount = VIDEO_MEXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MEXC_NAME(0),
		IS_VIDEO_ME0C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		NULL,
		&is_mexc_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_me1c_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_me1c;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_ISP1;
	video->group_ofs = offsetof(struct is_device_ischain, group_isp);
	video->subdev_id = ENTRY_MEXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, mexc);
	video->buf_rdycount = VIDEO_MEXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_MEXC_NAME(1),
		IS_VIDEO_ME1C_NUM,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		NULL,
		&is_mexc_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
