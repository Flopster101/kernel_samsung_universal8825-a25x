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

static int is_3aa_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	switch (ctrl->id) {
	case V4L2_CID_IS_FAST_CTL_LENS_POS:
		{
			struct fast_control_mgr *fastctlmgr = &device->fastctlmgr;
			struct is_fast_ctl *fast_ctl = NULL;
			unsigned long flags;
			u32 state;

			spin_lock_irqsave(&fastctlmgr->slock, flags);

			state = IS_FAST_CTL_FREE;
			if (fastctlmgr->queued_count[state]) {
				/* get free list */
				fast_ctl = list_first_entry(&fastctlmgr->queued_list[state],
					struct is_fast_ctl, list);
				list_del(&fast_ctl->list);
				fastctlmgr->queued_count[state]--;

				/* Write fast_ctl: lens */
				if (ctrl->id == V4L2_CID_IS_FAST_CTL_LENS_POS) {
					fast_ctl->lens_pos = ctrl->value;
					fast_ctl->lens_pos_flag = true;
				}

				/* TODO: Here is place for additional fast_ctl. */

				/* set req list */
				state = IS_FAST_CTL_REQUEST;
				fast_ctl->state = state;
				list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[state]);
				fastctlmgr->queued_count[state]++;
			} else {
				mwarn("not enough fast_ctl free queue\n", device, ctrl->value);
			}

			spin_unlock_irqrestore(&fastctlmgr->slock, flags);

			if (fast_ctl)
				mdbgv_3aa("%s: uctl.lensUd.pos(%d)\n", vctx, __func__, ctrl->value);
		}
		break;
	default:
		ret = is_vidioc_s_ctrl(file, priv, ctrl);
		if (ret) {
			merr("is_vidioc_s_ctrl is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_3aa_video_s_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_queue *queue;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	struct nfd_info *nfd_data;
#if IS_ENABLED(NFD_OBJECT_DETECT)
	struct od_info *od_data;
#endif
	unsigned long flags;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	FIMC_BUG(!GET_QUEUE(vctx));
	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;

	mdbgv_3aa("%s\n", vctx, __func__);

	if (ctrls->which != V4L2_CTRL_CLASS_CAMERA) {
		merr("Invalid control class(%d)", device, ctrls->which);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
#ifdef ENABLE_ULTRA_FAST_SHOT
		case V4L2_CID_IS_FAST_CAPTURE_CONTROL:
			{
				struct fast_ctl_capture *fast_capture =
					(struct fast_ctl_capture *)&device->is_region->fast_ctl.fast_capture;
				ret = copy_from_user(fast_capture, ext_ctrl->ptr, sizeof(struct fast_ctl_capture));
				if (ret) {
					merr("copy_from_user is fail(%d)", device, ret);
					goto p_err;
				}

				fast_capture->ready = 1;
				device->fastctlmgr.fast_capture_count = 2;

				vb2_ion_sync_for_device(
					device->imemory.fw_cookie,
					(ulong)fast_capture - device->imemory.kvaddr,
					sizeof(struct fast_ctl_capture),
					DMA_TO_DEVICE);

				mvinfo("Fast capture control(Intent:%d, count:%d, exposureTime:%d)\n",
					vctx, vctx->video, fast_capture->capture_intent, fast_capture->capture_count,
					fast_capture->capture_exposureTime);
			}
			break;
#endif
		case V4L2_CID_IS_S_NFD_DATA:
			nfd_data = (struct nfd_info *)&device->is_region->fd_info;
			if (access_ok(ext_ctrl->ptr, sizeof(struct nfd_info))) {
				spin_lock_irqsave(&device->is_region->fd_info_slock, flags);
				ret = __copy_from_user_inatomic(nfd_data, ext_ctrl->ptr,
								sizeof(struct nfd_info));
				if (ret) {
					merr("__copy_from_user_inatomic of nfd_info is fail(%d byte)",
								device, ret);
					memset(nfd_data, 0, sizeof(struct nfd_info));
				}
				if ((nfd_data->face_num < 0) || (nfd_data->face_num > MAX_FACE_COUNT))
					nfd_data->face_num = 0;
				spin_unlock_irqrestore(&device->is_region->fd_info_slock, flags);

				mdbgv_3aa("[F%d] face num(%d)\n", vctx,
					nfd_data->frame_count, nfd_data->face_num);
			} else {
				merr("ext_ctrl->ptr of nfd_info is not valid", device);
			}
			break;

#if IS_ENABLED(NFD_OBJECT_DETECT)
		case V4L2_CID_IS_S_OD_DATA:
			od_data = (struct od_info *)&device->is_region->od_info;
			if (access_ok(ext_ctrl->ptr, sizeof(struct od_info))) {
				spin_lock_irqsave(&device->is_region->fd_info_slock, flags);
				ret = __copy_from_user_inatomic(od_data, ext_ctrl->ptr,
								sizeof(struct od_info));
				if (ret) {
					merr("__copy_from_user_inatomic of od_info is fail(%d byte)",
								device, ret);
					memset(od_data, 0, sizeof(struct od_info));
				}
				spin_unlock_irqrestore(&device->is_region->fd_info_slock, flags);
			} else {
				merr("ext_ctrl->ptr of od_info is not valid", device);
			}
			break;
#endif
		case V4L2_CID_IS_G_SETFILE_VERSION:
			ret = is_ischain_g_ddk_setfile_version(device, ext_ctrl->ptr);
			break;
		case V4L2_CID_SENSOR_SET_FLASH_FASTSHOT:
		{
			struct is_group *head;

			head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_paf);

			ret = copy_from_user(&head->flash_ctl, ext_ctrl->ptr, sizeof(enum camera_flash_mode));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}
			head->remainFlashCtlCount = 5;
			break;
		}
		case V4L2_CID_SENSOR_SET_CAPTURE_INTENT_INFO:
		{
			struct is_group *head;
			struct capture_intent_info_t info;

			ret = copy_from_user(&info, ext_ctrl->ptr, sizeof(struct capture_intent_info_t));
			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}

			head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);
			
			spin_lock_irqsave(&head->slock_s_ctrl, flags);

			head->intent_ctl.captureIntent = info.captureIntent;
			head->intent_ctl.vendor_captureCount = info.captureCount;
			head->intent_ctl.vendor_captureEV = info.captureEV;
			head->intent_ctl.vendor_captureExtraInfo = info.captureExtraInfo;
			memcpy(&(head->intent_ctl.vendor_multiFrameEvList), &(info.captureMultiEVList), EV_LIST_SIZE);

			if (info.captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI
				|| info.captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_GALAXY_RAW_DYNAMIC_SHOT) {
				head->remainIntentCount = 3 + INTENT_RETRY_CNT;
			} else {
				head->remainIntentCount = 1 + INTENT_RETRY_CNT;
			}
			
			spin_unlock_irqrestore(&head->slock_s_ctrl, flags);

			mginfo("s_ext_ctrl SET_CAPTURE_INTENT_INFO, intent(%d) count(%d) captureEV(%d) remainIntentCount(%d)\n",
				head, head, info.captureIntent, info.captureCount, info.captureEV, head->remainIntentCount);
			break;
		}
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = is_vidioc_s_ctrl(file, priv, &ctrl);
			if (ret) {
				merr("is_vidioc_s_ctrl is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_3aa_video_ioctl_ops = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_prepare_buf		= is_vidioc_prepare_buf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_s_ctrl			= is_3aa_video_s_ctrl,
	.vidioc_s_ext_ctrls		= is_3aa_video_s_ext_ctrl,
	.vidioc_g_ext_ctrls		= is_vidioc_g_ext_ctrls,
};

int is_30s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_30s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AA;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_3XS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(0),
		IS_VIDEO_30S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_3aa_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_31s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_31s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA1;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AA;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_3XS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(1),
		IS_VIDEO_31S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_3aa_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_32s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_32s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA2;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AA;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_3XS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(2),
		IS_VIDEO_32S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_3aa_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int is_33s_video_probe(void *data)
{
	int ret = 0;
	struct is_core *core;
	struct is_video *video;

	FIMC_BUG(!data);

	core = (struct is_core *)data;
	video = &core->video_33s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_3AA3;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AA;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_3XS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XS_NAME(3),
		IS_VIDEO_33S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_3aa_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
