/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos Pablo video functions
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

int is_ypp_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;
	char *video_name;
	u32 video_number;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_ypp;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_YPP;
	video->group_ofs = offsetof(struct is_device_ischain, group_ypp);
	video->subdev_id = ENTRY_YPP;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_YPP_READY_BUFFERS;

#if IS_ENABLED(SOC_YUVP)
	video_name = IS_VIDEO_YUVP_NAME;
	video_number = IS_VIDEO_YUVP;
#else
	video_name = IS_VIDEO_YPP_NAME;
	video_number = IS_VIDEO_YPP_NUM;
#endif

	ret = is_video_probe(video,
		video_name,
		video_number,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
