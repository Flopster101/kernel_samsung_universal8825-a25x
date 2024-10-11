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
#include <asm/cacheflush.h>

#include "is-core.h"
#include "is-param.h"
#include "is-device-ischain.h"
#include "is-debug.h"
#include "pablo-mem.h"
#if defined(SDC_HEADER_GEN)
#include "is-sdc-header.h"
#endif
#include "is-hw-common-dma.h"

bool is_subdev_check_vid(uint32_t vid)
{
	bool is_valid = true;

	if (vid == 0)
		is_valid = false;
	else if (vid >= IS_VIDEO_MAX_NUM)
		is_valid = false;
	else if (IS_ENABLED(LOGICAL_VIDEO_NODE)
			&& (vid >= IS_VIDEO_LVN_BASE))
		is_valid = false;

	return is_valid;
}

struct is_subdev * video2subdev(enum is_subdev_device_type device_type,
	void *device, u32 vid)
{
	struct is_subdev *subdev = NULL;
	struct is_device_sensor *sensor = NULL;
	struct is_device_ischain *ischain = NULL;

	if (device_type == IS_SENSOR_SUBDEV) {
		sensor = (struct is_device_sensor *)device;
	} else {
		ischain = (struct is_device_ischain *)device;
		sensor = ischain->sensor;
	}

	switch (vid) {
	case IS_VIDEO_SS0VC0_NUM:
	case IS_VIDEO_SS1VC0_NUM:
	case IS_VIDEO_SS2VC0_NUM:
	case IS_VIDEO_SS3VC0_NUM:
	case IS_VIDEO_SS4VC0_NUM:
	case IS_VIDEO_SS5VC0_NUM:
		subdev = &sensor->ssvc0;
		break;
	case IS_VIDEO_SS0VC1_NUM:
	case IS_VIDEO_SS1VC1_NUM:
	case IS_VIDEO_SS2VC1_NUM:
	case IS_VIDEO_SS3VC1_NUM:
	case IS_VIDEO_SS4VC1_NUM:
	case IS_VIDEO_SS5VC1_NUM:
		subdev = &sensor->ssvc1;
		break;
	case IS_VIDEO_SS0VC2_NUM:
	case IS_VIDEO_SS1VC2_NUM:
	case IS_VIDEO_SS2VC2_NUM:
	case IS_VIDEO_SS3VC2_NUM:
	case IS_VIDEO_SS4VC2_NUM:
	case IS_VIDEO_SS5VC2_NUM:
		subdev = &sensor->ssvc2;
		break;
	case IS_VIDEO_SS0VC3_NUM:
	case IS_VIDEO_SS1VC3_NUM:
	case IS_VIDEO_SS2VC3_NUM:
	case IS_VIDEO_SS3VC3_NUM:
	case IS_VIDEO_SS4VC3_NUM:
	case IS_VIDEO_SS5VC3_NUM:
		subdev = &sensor->ssvc3;
		break;
	case IS_VIDEO_30S_NUM:
	case IS_VIDEO_31S_NUM:
	case IS_VIDEO_32S_NUM:
	case IS_VIDEO_33S_NUM:
		subdev = &ischain->group_3aa.leader;
		break;
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_32C_NUM:
	case IS_VIDEO_33C_NUM:
		subdev = &ischain->txc;
		break;
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_32P_NUM:
	case IS_VIDEO_33P_NUM:
		subdev = &ischain->txp;
		break;
	case IS_VIDEO_30F_NUM:
	case IS_VIDEO_31F_NUM:
	case IS_VIDEO_32F_NUM:
	case IS_VIDEO_33F_NUM:
		subdev = &ischain->txf;
		break;
	case IS_VIDEO_30G_NUM:
	case IS_VIDEO_31G_NUM:
	case IS_VIDEO_32G_NUM:
	case IS_VIDEO_33G_NUM:
		subdev = &ischain->txg;
		break;
	case IS_VIDEO_30O_NUM:
	case IS_VIDEO_31O_NUM:
	case IS_VIDEO_32O_NUM:
	case IS_VIDEO_33O_NUM:
		subdev = &ischain->txo;
		break;
	case IS_VIDEO_30L_NUM:
	case IS_VIDEO_31L_NUM:
	case IS_VIDEO_32L_NUM:
	case IS_VIDEO_33L_NUM:
		subdev = &ischain->txl;
		break;
	case IS_VIDEO_LME0_NUM:
	case IS_VIDEO_LME1_NUM:
		subdev = &ischain->group_lme.leader;
		break;
	case IS_VIDEO_LME0S_NUM:
	case IS_VIDEO_LME1S_NUM:
		subdev = &ischain->lmes;
		break;
	case IS_VIDEO_LME0C_NUM:
	case IS_VIDEO_LME1C_NUM:
		subdev = &ischain->lmec;
		break;
	case IS_VIDEO_ORB0_NUM:
	case IS_VIDEO_ORB1_NUM:
		subdev = &ischain->group_orb.leader;
		break;
	case IS_VIDEO_ORB0C_NUM:
	case IS_VIDEO_ORB1C_NUM:
		subdev = &ischain->orbxc;
		break;
	case IS_VIDEO_I0S_NUM:
	case IS_VIDEO_I1S_NUM:
		subdev = &ischain->group_isp.leader;
		break;
	case IS_VIDEO_I0C_NUM:
	case IS_VIDEO_I1C_NUM:
		subdev = &ischain->ixc;
		break;
	case IS_VIDEO_I0P_NUM:
	case IS_VIDEO_I1P_NUM:
		subdev = &ischain->ixp;
		break;
	case IS_VIDEO_I0T_NUM:
		subdev = &ischain->ixt;
		break;
	case IS_VIDEO_I0G_NUM:
		subdev = &ischain->ixg;
		break;
	case IS_VIDEO_I0V_NUM:
		subdev = &ischain->ixv;
		break;
	case IS_VIDEO_I0W_NUM:
		subdev = &ischain->ixw;
		break;
	case IS_VIDEO_ME0C_NUM:
	case IS_VIDEO_ME1C_NUM:
		subdev = &ischain->mexc;
		break;
	case IS_VIDEO_M0S_NUM:
	case IS_VIDEO_M1S_NUM:
		subdev = &ischain->group_mcs.leader;
		break;
	case IS_VIDEO_M0P_NUM:
		subdev = &ischain->m0p;
		break;
	case IS_VIDEO_M1P_NUM:
		subdev = &ischain->m1p;
		break;
	case IS_VIDEO_M2P_NUM:
		subdev = &ischain->m2p;
		break;
	case IS_VIDEO_M3P_NUM:
		subdev = &ischain->m3p;
		break;
	case IS_VIDEO_M4P_NUM:
		subdev = &ischain->m4p;
		break;
	case IS_VIDEO_M5P_NUM:
		subdev = &ischain->m5p;
		break;
	case IS_VIDEO_VRA_NUM:
		subdev = &ischain->group_vra.leader;
		break;
	default:
		err("[%d] vid %d is NOT found", ((device_type == IS_SENSOR_SUBDEV) ?
				 (ischain ? ischain->instance : 0) : (sensor ? sensor->device_id : 0)), vid);
		break;
	}

	return subdev;
}

int is_subdev_probe(struct is_subdev *subdev,
	u32 instance,
	u32 id,
	const char *name,
	const struct is_subdev_ops *sops)
{
	FIMC_BUG(!subdev);
	FIMC_BUG(!name);

	subdev->id = id;
	subdev->wq_id = is_subdev_wq_id[id];
	subdev->instance = instance;
	subdev->ops = sops;
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	subdev->state = 0L;

	/* for internal use */
	frame_manager_probe(&subdev->internal_framemgr, subdev->id, name);

	return 0;
}

int is_subdev_open(struct is_subdev *subdev,
	struct is_video_ctx *vctx,
	void *ctl_data,
	u32 instance)
{
	int ret = 0;
	struct is_video *video;
	const struct param_control *init_ctl = (const struct param_control *)ctl_data;

	FIMC_BUG(!subdev);

	if (test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("already open", subdev, subdev);
		ret = -EPERM;
		goto p_err;
	}

	/* If it is internal VC, skip vctx setting. */
	if (vctx) {
		subdev->vctx = vctx;
		video = GET_VIDEO(vctx);
		subdev->vid = (video) ? video->id : 0;
	}

	subdev->instance = instance;
	subdev->cid = CAPTURE_NODE_MAX;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->input.crop.x = 0;
	subdev->input.crop.y = 0;
	subdev->input.crop.w = 0;
	subdev->input.crop.h = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = 0;
	subdev->output.crop.h = 0;
	subdev->bits_per_pixel = 0;
	subdev->memory_bitwidth = 0;
	subdev->sbwc_type = 0;
	subdev->lossy_byte32num = 0;

	if (init_ctl) {
		set_bit(IS_SUBDEV_START, &subdev->state);

		if (subdev->id == ENTRY_VRA) {
			/* vra only control by command for enabling or disabling */
			if (init_ctl->cmd == CONTROL_COMMAND_STOP)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		} else {
			if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(IS_SUBDEV_RUN, &subdev->state);
		}
	} else {
		clear_bit(IS_SUBDEV_START, &subdev->state);
		clear_bit(IS_SUBDEV_RUN, &subdev->state);
	}

	set_bit(IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int is_sensor_subdev_open(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = vctx->subdev;

	ret = is_subdev_open(subdev, vctx, NULL, device->instance);
	if (ret) {
		mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	return 0;

err_subdev_open:
	return ret;
}

int is_ischain_subdev_open(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	subdev = vctx->subdev;

	ret = is_subdev_open(subdev, vctx, NULL, device->instance);
	if (ret) {
		merr("is_subdev_open is fail(%d)", device, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	ret = is_ischain_open_wrap(device, false);
	if (ret) {
		merr("is_ischain_open_wrap is fail(%d)", device, ret);
		goto err_ischain_open;
	}

	return 0;

err_ischain_open:
	ret_err = is_subdev_close(subdev);
	if (ret_err)
		merr("is_subdev_close is fail(%d)", device, ret_err);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

err_subdev_open:
	return ret;
}

int is_subdev_close(struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is already close", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	subdev->leader = NULL;
	subdev->vctx = NULL;
	subdev->vid = 0;

	clear_bit(IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_FORCE_SET, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

	/* for internal use */
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

p_err:
	return 0;
}

int is_sensor_subdev_close(struct is_device_sensor *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("sudden close, call sensor_front_stop()\n", device);

		ret = is_sensor_front_stop(device, true);
		if (ret)
			merr("is_sensor_front_stop is fail(%d)", device, ret);
	}

	ret = is_subdev_close(subdev);
	if (ret)
		merr("is_subdev_close is fail(%d)", device, ret);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

p_err:
	return ret;
}

int is_ischain_subdev_close(struct is_device_ischain *device,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_subdev *subdev;

	FIMC_BUG(!device);
	FIMC_BUG(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = is_subdev_close(subdev);
	if (ret) {
		merr("is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state);

	ret = is_ischain_close_wrap(device);
	if (ret) {
		merr("is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int pablo_subdev_buffer_init(struct is_subdev *is, struct vb2_buffer *vb)
{
	unsigned long flags;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(is);
	struct is_frame *frame;
	unsigned int index = vb->index;
	struct is_device_ischain *idi = GET_DEVICE(is->vctx);

	if (index >= framemgr->num_frames) {
		mserr("out of range (%d >= %d)", idi, is, index, framemgr->num_frames);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	frame = &framemgr->frames[index];
	memset(&frame->prfi, 0x0, sizeof(struct pablo_rta_frame_info));
	frame->prfi.magic = 0x5F4D5246;

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

	return 0;
}

int is_subdev_buffer_queue(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	unsigned long flags;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	struct is_frame *frame;
	unsigned int index = vb->index;

	FIMC_BUG(!subdev);
	FIMC_BUG(!framemgr);
	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];

	/* 1. check frame validation */
	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		mserr("frame %d is NOT init", subdev, subdev, index);
		ret = EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FS_FREE) {
		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		mserr("frame %d is invalid state(%d)\n", subdev, subdev, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

p_err:
	return ret;
}

int is_subdev_buffer_finish(struct is_subdev *subdev,
	struct vb2_buffer *vb)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned int index = vb->index;

	if (!subdev) {
		warn("subdev is NULL(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(!test_bit(IS_SUBDEV_OPEN, &subdev->state))) {
		warn("subdev was closed..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(!subdev->vctx);

	device = GET_DEVICE(subdev->vctx);
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		warn("subdev's framemgr is null..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	FIMC_BUG(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr("frame is empty from complete", device);
		merr("frame(%d) is not com state(%d)", device,
					index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}

static int is_subdev_start(struct is_subdev *is)
{
	struct is_subdev *leader;

	FIMC_BUG(!is->leader);

	if (test_bit(IS_SUBDEV_START, &is->state)) {
		mserr("already start", is, is);
		return 0;
	}

	leader = is->leader;
	if (test_bit(IS_SUBDEV_START, &leader->state)) {
		mserr("leader%d is ALREADY started", is, is, leader->id);
		return -EINVAL;
	}

	set_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_subdev_s_fmt(struct is_subdev *is, struct is_queue *iq)
{
	u32 pixelformat, width, height;

	FIMC_BUG(!is->vctx);
	FIMC_BUG(!iq->framecfg.format);

	pixelformat = iq->framecfg.format->pixelformat;
	width = iq->framecfg.width;
	height = iq->framecfg.height;

	switch (is->id) {
	case ENTRY_M0P:
	case ENTRY_M1P:
	case ENTRY_M2P:
	case ENTRY_M3P:
	case ENTRY_M4P:
		if (width % 8)
			mserr("width(%d) of format(%c%c%c%c) is not multiple of 8: need to check size",
					is, is, width,
					(char)((pixelformat >> 0) & 0xFF),
					(char)((pixelformat >> 8) & 0xFF),
					(char)((pixelformat >> 16) & 0xFF),
					(char)((pixelformat >> 24) & 0xFF));

		break;
	default:
		break;
	}

	is->output.width = width;
	is->output.height = height;

	is->output.crop.x = 0;
	is->output.crop.y = 0;
	is->output.crop.w = is->output.width;
	is->output.crop.h = is->output.height;

	is->bits_per_pixel = iq->framecfg.format->bitsperpixel[0];
	is->memory_bitwidth = iq->framecfg.format->hw_bitwidth;
	is->sbwc_type = (iq->framecfg.hw_pixeltype & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;

	return 0;
}

static int is_sensor_subdev_start(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (!test_bit(IS_SENSOR_S_INPUT, &ids->state)) {
		mserr("device is not yet init", ids, is);
		return -EINVAL;
	}

	if (test_bit(IS_SUBDEV_START, &is->state)) {
		mserr("already start", is, is);
		return 0;
	}

	set_bit(IS_SUBDEV_START, &is->state);

	return 0;
}

static int is_sensor_subdev_stop(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	unsigned long flags;
	struct is_frame *frame;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_START, &is->state)) {
		merr("already stop", ids);
		return 0;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", ids);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &is->state);
	clear_bit(IS_SUBDEV_START, &is->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &is->state);

	return 0;
}

static int is_sensor_subdev_s_fmt(void *device, struct is_queue *iq)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;
	int ret;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &is->state)) {
		mswarn("%s: It is sharing with internal use.", is, is, __func__);

		is->bits_per_pixel = iq->framecfg.format->bitsperpixel[0];
	} else {
		ret = is_subdev_s_fmt(is, iq);
		if (ret) {
			merr("is_subdev_s_format is fail(%d)", ids, ret);
			return ret;
		}
	}

	return 0;
}

int is_sensor_subdev_reqbufs(void *device, struct is_queue *iq, u32 count)
{
	struct is_device_sensor *ids = (struct is_device_sensor *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	int i;

	if (!is) {
		merr("subdev is NULL", ids);
		return -EINVAL;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", ids);
		return -EINVAL;
	}

	for (i = 0; i < count; i++) {
		frame = &framemgr->frames[i];
		frame->subdev = is;
	}

	return 0;
}

static struct is_queue_ops is_sensor_subdev_qops = {
	.start_streaming	= is_sensor_subdev_start,
	.stop_streaming		= is_sensor_subdev_stop,
	.s_fmt			= is_sensor_subdev_s_fmt,
	.reqbufs		= is_sensor_subdev_reqbufs,
};

struct is_queue_ops *is_get_sensor_subdev_qops(void)
{
	return &is_sensor_subdev_qops;
}

static int is_ischain_subdev_start(void *device, struct is_queue *iq)
{
	int ret;
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	if (!test_bit(IS_ISCHAIN_INIT, &idi->state)) {
		mserr("device is not yet init", idi, is);
		return -EINVAL;
	}

	ret = is_subdev_start(is);
	if (ret) {
		mserr("is_subdev_start is fail(%d)", idi, is, ret);
		return ret;
	}

	return 0;
}

static int is_ischain_subdev_stop(void *device, struct is_queue *iq)
{
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;
	struct is_framemgr *framemgr;
	unsigned long flags;
	struct is_frame *frame;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_START, &is->state)) {
		merr("already stop", idi);
		return 0;
	}

	if (is->leader && test_bit(IS_SUBDEV_START, &is->leader->state)) {
		merr("leader%d is NOT stopped", idi, is->leader->id);
		return 0;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(is);
	if (!framemgr) {
		merr("framemgr is NULL", idi);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->queued_count[FS_PROCESS] > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", idi);
		return -EINVAL;
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(is->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(IS_SUBDEV_RUN, &is->state);
	clear_bit(IS_SUBDEV_START, &is->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &is->state);

	return 0;
}

static int is_ischain_subdev_s_fmt(void *device, struct is_queue *iq)
{
	int ret;
	struct is_device_ischain *idi = (struct is_device_ischain *)device;
	struct is_video_ctx *ivc = container_of(iq, struct is_video_ctx, queue);
	struct is_subdev *is = ivc->subdev;

	if (!is) {
		merr("subdev is NULL", idi);
		return -EINVAL;
	}

	ret = is_subdev_s_fmt(is, iq);
	if (ret) {
		merr("is_subdev_s_format is fail(%d)", idi, ret);
		return ret;
	}

	return 0;
}

static struct is_queue_ops is_ischain_subdev_qops = {
	.start_streaming	= is_ischain_subdev_start,
	.stop_streaming		= is_ischain_subdev_stop,
	.s_fmt			= is_ischain_subdev_s_fmt,
};

struct is_queue_ops *is_get_ischain_subdev_qops(void)
{
	return &is_ischain_subdev_qops;
}


static int is_subdev_internal_alloc_buffer(struct is_subdev *subdev)
{
	int ret;
	int i, j;
	unsigned int flags = 0;
	u32 width, height;
	u32 out_buf_size; /* single buffer */
	u32 cap_buf_size[IS_MAX_PLANES];
	u32 total_size; /* multi-buffer for FRO */
	u32 total_alloc_size = 0;
	u32 num_planes;
	struct is_core *core;
	struct is_mem *mem;
	struct is_frame *frame;
	struct is_queue *queue;
	struct is_framemgr *shared_framemgr = NULL;
	int use_shared_framemgr;
	int k;
	struct is_sub_node *snode = NULL;
	int cap_node_num;
	u32 batch_num;
	u32 payload_size, header_size;
	u32 sbwc_block_width, sbwc_block_height;
	char heapname[25];

	FIMC_BUG(!subdev);

	if (subdev->buffer_num > SUBDEV_INTERNAL_BUF_MAX || subdev->buffer_num <= 0) {
		mserr("invalid internal buffer num size(%d)",
			subdev, subdev, subdev->buffer_num);
		return -EINVAL;
	}

	core = is_get_is_core();
	if (!core) {
		mserr("core is NULL", subdev, subdev);
		return -ENODEV;
	}
	mem = is_hw_get_iommu_mem(subdev->vid);

	queue = GET_SUBDEV_QUEUE(subdev);

	subdev->use_shared_framemgr = is_subdev_internal_use_shared_framemgr(subdev);

	ret = is_subdev_internal_get_buffer_size(subdev, &width, &height,
						&sbwc_block_width, &sbwc_block_height);
	if (ret) {
		mserr("is_subdev_internal_get_buffer_size is fail(%d)", subdev, subdev, ret);
		return -EINVAL;
	}

	switch (subdev->sbwc_type) {
	case COMP:
		payload_size = is_hw_dma_get_payload_stride(
			COMP, subdev->bits_per_pixel, width,
			0, 0, sbwc_block_width, sbwc_block_height) *
			DIV_ROUND_UP(height, sbwc_block_height);
		header_size = is_hw_dma_get_header_stride(width, sbwc_block_width, 32) *
			DIV_ROUND_UP(height, sbwc_block_height);
		out_buf_size = payload_size + header_size;
		break;
	case COMP_LOSS:
		payload_size = is_hw_dma_get_payload_stride(
			COMP_LOSS, subdev->bits_per_pixel, width,
			0, subdev->lossy_byte32num, sbwc_block_width, sbwc_block_height) *
			DIV_ROUND_UP(height, sbwc_block_height);
		out_buf_size = payload_size;
		break;
	default:
		out_buf_size =
			ALIGN(DIV_ROUND_UP(width * subdev->memory_bitwidth, BITS_PER_BYTE), 32) * height;
		break;
	}

	if (out_buf_size <= 0) {
		mserr("wrong internal subdev buffer size(%d)",
					subdev, subdev, out_buf_size);
		return -EINVAL;
	}

	batch_num = subdev->batch_num;

	memset(heapname, '\0', sizeof(heapname));
	ret = is_subdev_internal_get_out_node_info(subdev, &num_planes, core->scenario, heapname);
	if (ret)
		return ret;

	cap_node_num = is_subdev_internal_get_cap_node_num(subdev);
	if (cap_node_num < 0)
		return -EINVAL;

	total_size = out_buf_size * batch_num * num_planes;

	if (test_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state)) {
		if (IS_ENABLED(USE_IOVA_EXTENSION))
			flags |= ION_EXYNOS_FLAG_IOVA_EXTENSION;
		else
			total_size *= 2;
		msinfo(" %s IOVA_EXTENSION_USE\n", subdev, subdev, __func__);
	}

	if (!strncmp(heapname, SECURE_HEAPNAME, 20)) {
		if (IS_ENABLED(CONFIG_ION)) {
			flags |= ION_EXYNOS_FLAG_PROTECTED;
			msinfo(" %s scenario(%d) ION flag(0x%x)\n", subdev, subdev,
							__func__, core->scenario, flags);
		}
	}

	ret = frame_manager_open(&subdev->internal_framemgr, subdev->buffer_num);
	if (ret) {
		mserr("is_frame_open is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto err_open_framemgr;
	}

	is_subdev_internal_lock_shared_framemgr(subdev);
	use_shared_framemgr = is_subdev_internal_get_shared_framemgr(subdev, &shared_framemgr, width, height);
	if (use_shared_framemgr < 0) {
		mserr("is_subdev_internal_get_shared_framemgr is fail(%d)", subdev, subdev, use_shared_framemgr);
		ret = -EINVAL;
		goto err_open_shared_framemgr;
	}

#if defined(USE_CAMERA_HEAP)
	if (core->scenario != IS_SCENARIO_SECURE && subdev->instance == 0) {
		memset(heapname, '\0', sizeof(heapname));
		strcpy(heapname, CAMERA_HEAP_NAME);
	}
#endif

	for (i = 0; i < subdev->buffer_num; i++) {
		if (use_shared_framemgr > 0) {
			subdev->pb_subdev[i] = shared_framemgr->frames[i].pb_output;
		} else {
			if (num_planes) {
#if defined(USE_CAMERA_HEAP)
				subdev->pb_subdev[i] =
					CALL_PTR_MEMOP(mem, alloc, mem->priv, total_size, heapname,
						flags);
#else				
				subdev->pb_subdev[i] =
					CALL_PTR_MEMOP(mem, alloc, mem->priv, total_size, heapname,
						flags);
#endif
				if (IS_ERR_OR_NULL(subdev->pb_subdev[i])) {
					mserr("failed to allocate buffer for internal subdev",
									subdev, subdev);
					subdev->pb_subdev[i] = NULL;
					ret = -ENOMEM;
					goto err_allocate_pb_subdev;
				}
			} else {
				subdev->pb_subdev[i] = NULL;
			}


			if (shared_framemgr)
				shared_framemgr->frames[i].pb_output = subdev->pb_subdev[i];
		}
		total_alloc_size += total_size;
	}

	for (i = 0; i < subdev->buffer_num; i++) {
		frame = &subdev->internal_framemgr.frames[i];
		frame->subdev = subdev;
		frame->pb_output = subdev->pb_subdev[i];

		/* TODO : support multi-plane */
		frame->planes = 1;
		frame->num_buffers = batch_num;

		if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
			snode = &frame->out_node;
			if (subdev->pb_subdev[i]) {
				snode->sframe[0].dva[0] = frame->dvaddr_buffer[0] =
					CALL_BUFOP(subdev->pb_subdev[i], dvaddr,
							subdev->pb_subdev[i]);

				if (!strncmp(heapname, SECURE_HEAPNAME, 20))
					frame->kvaddr_buffer[0] = 0;
				else
					frame->kvaddr_buffer[0] =
						CALL_BUFOP(subdev->pb_subdev[i], kvaddr,
								subdev->pb_subdev[i]);
			}

			frame->size[0] = out_buf_size;
			set_bit(FRAME_MEM_INIT, &frame->mem_state);

			for (j = 1; j < batch_num * num_planes; j++) {
				snode->sframe[0].dva[j] = frame->dvaddr_buffer[j] =
					frame->dvaddr_buffer[j - 1] + out_buf_size;
				frame->kvaddr_buffer[j] =
					frame->kvaddr_buffer[j - 1] + out_buf_size;
			}

			snode->sframe[0].id = subdev->vid;
			snode->sframe[0].num_planes = num_planes;
			if (queue)
				snode->sframe[0].hw_pixeltype =
					queue->framecfg.hw_pixeltype;

			snode = &frame->cap_node;

			for (j = 0; j < cap_node_num; j++) {
				memset(heapname, '\0', sizeof(heapname));
				ret = is_subdev_internal_get_cap_node_info(subdev,
						&snode->sframe[j].id,
						&snode->sframe[j].num_planes,
						cap_buf_size, j, core->scenario, heapname);
				if (ret) {
					subdev->pb_capture_subdev[i][j] = NULL;
					goto err_allocate_pb_capture_subdev;
				}

				if (queue)
					snode->sframe[j].hw_pixeltype =
						queue->framecfg.hw_pixeltype;

				total_size = 0;
				for (k = 0; k < snode->sframe[j].num_planes; k++)
					total_size += cap_buf_size[k];

				if (!strncmp(heapname, SECURE_HEAPNAME, 20))
					flags |= ION_EXYNOS_FLAG_PROTECTED;
				else
					flags = 0;

				if (use_shared_framemgr > 0) {
					subdev->pb_capture_subdev[i][j] =
						shared_framemgr->frames[i].pb_capture[j];
				} else {
					if (total_size) {
#if defined(USE_CAMERA_HEAP)
						if (core->scenario != IS_SCENARIO_SECURE && subdev->instance == 0)
							strcpy(heapname, CAMERA_HEAP_NAME);
#endif
						subdev->pb_capture_subdev[i][j] =
							CALL_PTR_MEMOP(mem, alloc,
							mem->priv, total_size,
							heapname, flags);
						if (IS_ERR_OR_NULL(subdev->pb_capture_subdev[i][j])) {
							mserr("failed to allocate buffer for internal subdev",
								subdev, subdev);
							subdev->pb_capture_subdev[i][j] = NULL;
							ret = -ENOMEM;
							goto err_allocate_pb_capture_subdev;
						}
					} else {
						subdev->pb_capture_subdev[i][j] = NULL;
					}

					if (shared_framemgr)
						shared_framemgr->frames[i].pb_capture[j] =
							subdev->pb_capture_subdev[i][j];
				}

				frame->pb_capture[j] = subdev->pb_capture_subdev[i][j];
				total_alloc_size += total_size;

				if (subdev->pb_capture_subdev[i][j]) {
					snode->sframe[j].backup_dva[0] = snode->sframe[j].dva[0] =
						CALL_BUFOP(subdev->pb_capture_subdev[i][j],
								dvaddr, subdev->pb_capture_subdev[i][j]);

					if (!strncmp(heapname, SECURE_HEAPNAME, 20))
						snode->sframe[j].kva[0] = 0;
					else
						snode->sframe[j].kva[0] =
							CALL_BUFOP(subdev->pb_capture_subdev[i][j],
								kvaddr, subdev->pb_capture_subdev[i][j]);
				}

				for (k = 1; k < snode->sframe[j].num_planes; k++) {
					snode->sframe[j].backup_dva[k] =
						snode->sframe[j].dva[k] =
						snode->sframe[j].dva[k - 1] + cap_buf_size[k - 1];
					snode->sframe[j].kva[k] =
						snode->sframe[j].kva[k - 1] + cap_buf_size[k - 1];
				}
			}
		} else {
			frame->dvaddr_buffer[0] =
				CALL_BUFOP(subdev->pb_subdev[i], dvaddr,
						subdev->pb_subdev[i]);
			frame->kvaddr_buffer[0] =
				CALL_BUFOP(subdev->pb_subdev[i],
						kvaddr, subdev->pb_subdev[i]);
			frame->size[0] = out_buf_size;

			set_bit(FRAME_MEM_INIT, &frame->mem_state);

			for (j = 1; j < batch_num; j++) {
				frame->dvaddr_buffer[j] =
					frame->dvaddr_buffer[j - 1] + out_buf_size;
				frame->kvaddr_buffer[j] =
					frame->kvaddr_buffer[j - 1] + out_buf_size;
			}
		}

#if defined(SDC_HEADER_GEN)
		if (subdev->id == ENTRY_PAF) {
			const u32 *header = NULL;
			u32 byte_per_line;
			u32 header_size;
			u32 width = subdev->output.width;

			if (width == SDC_WIDTH_HD)
				header = is_sdc_header_hd;
			else if (width == SDC_WIDTH_FHD)
				header = is_sdc_header_fhd;
			else
				mserr("invalid SDC size: width(%d)", subdev, subdev, width);

			if (header) {
				byte_per_line = ALIGN(width / 2 * 10 / BITS_PER_BYTE, 16);
				header_size = byte_per_line * SDC_HEADER_LINE;

				memcpy((void *)frame->kvaddr_buffer[0], header, header_size);
#if !defined(MODULE)
				__flush_dcache_area((void *)frame->kvaddr_buffer[0], header_size);
#endif
				msinfo("Write SDC header: width(%d) size(%d)\n",
					subdev, subdev, width, header_size);
			}
		}
#endif
	}
	is_subdev_internal_unlock_shared_framemgr(subdev);

	msinfo(" %s (size: %dx%d, bpp: %d, total: %d, buffer_num: %d, batch_num: %d, shared_framemgr(ref: %d))",
		subdev, subdev, __func__,
		width, height, subdev->bits_per_pixel,
		total_alloc_size, subdev->buffer_num, batch_num, use_shared_framemgr);

	return 0;

err_allocate_pb_capture_subdev:
	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		do {
			while (j-- > 0) {
				if (subdev->pb_capture_subdev[i][j])
					CALL_VOID_BUFOP(subdev->pb_capture_subdev[i][j],
							free, subdev->pb_capture_subdev[i][j]);
			}

			j = cap_node_num;
		} while (i-- > 0);
	}

err_allocate_pb_subdev:
	while (i-- > 0) {
		if (subdev->pb_subdev[i])
			CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);
	}

	is_subdev_internal_put_shared_framemgr(subdev);
err_open_shared_framemgr:
	is_subdev_internal_unlock_shared_framemgr(subdev);
	frame_manager_close(&subdev->internal_framemgr);
err_open_framemgr:
	return ret;
};

static int is_subdev_internal_free_buffer(struct is_subdev *subdev)
{
	int ret = 0;
	int i;
	int j;
	u32 cap_node_num;

	FIMC_BUG(!subdev);

	if (subdev->internal_framemgr.num_frames == 0) {
		mswarn(" already free internal buffer", subdev, subdev);
		return -EINVAL;
	}

	frame_manager_close(&subdev->internal_framemgr);

	is_subdev_internal_lock_shared_framemgr(subdev);
	ret = is_subdev_internal_put_shared_framemgr(subdev);
	if (!ret) {
		for (i = 0; i < subdev->buffer_num; i++) {
			if (subdev->pb_subdev[i])
				CALL_VOID_BUFOP(subdev->pb_subdev[i], free, subdev->pb_subdev[i]);
		}

		if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
			cap_node_num = is_subdev_internal_get_cap_node_num(subdev);
			for (i = 0; i < subdev->buffer_num; i++) {
				for (j = 0; j < cap_node_num; j++) {
					if (subdev->pb_capture_subdev[i][j])
						CALL_VOID_BUFOP(subdev->pb_capture_subdev[i][j],
								free, subdev->pb_capture_subdev[i][j]);
				}
			}
		}
	}
	is_subdev_internal_unlock_shared_framemgr(subdev);

	msinfo(" %s. shared_framemgr refcount: %d\n", subdev, subdev, __func__, ret);

	return 0;
};

static int _is_subdev_internal_start(struct is_subdev *subdev)
{
	int ret = 0;
	int j;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;

	if (test_bit(IS_SUBDEV_START, &subdev->state)) {
		mswarn(" subdev already start", subdev, subdev);
		goto p_err;
	}

	/* qbuf a setting num of buffers before stream on */
	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	for (j = 0; j < framemgr->num_frames; j++) {
		frame = &framemgr->frames[j];

		/* 1. check frame validation */
		if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
			mserr("frame %d is NOT init", subdev, subdev, j);
			ret = -EINVAL;
			goto p_err;
		}

		/* 2. update frame manager */
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

		if (frame->state != FS_FREE) {
			mserr("frame %d is invalid state(%d)\n",
				subdev, subdev, j, frame->state);
			frame_manager_print_queues(framemgr);
			ret = -EINVAL;
			goto p_err;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);
	}

	set_bit(IS_SUBDEV_START, &subdev->state);

p_err:

	return ret;
}

static int _is_subdev_internal_stop(struct is_subdev *subdev)
{
	int ret = 0;
	struct is_framemgr *framemgr;
	enum is_frame_state state;

	if (!test_bit(IS_SUBDEV_START, &subdev->state)) {
		mserr("already stopped", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (unlikely(!framemgr)) {
		mserr(" subdev's framemgr is null", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	for (state = FS_REQUEST; state <= FS_COMPLETE; state++) {
		unsigned int retry = 10;
		unsigned int q_count = framemgr->queued_count[state];

		while (--retry && q_count) {
			mswarn("%s(%d) waiting...", subdev, subdev,
				frame_state_name[state], q_count);

			usleep_range(5000, 5100);
			q_count = framemgr->queued_count[state];
		}

		if (!retry)
			mswarn("waiting(until frame empty) is fail", subdev, subdev);
	}

	clear_bit(IS_SUBDEV_START, &subdev->state);
	clear_bit(IS_SUBDEV_VOTF_USE, &subdev->state);

p_err:

	return ret;
}

int is_subdev_internal_start(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	FIMC_BUG(!device);

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state)) {
		mserr("subdev is not in s_fmt state.", subdev, subdev);
		return -EINVAL;
	}

	if (subdev->internal_framemgr.num_frames > 0) {
		mswarn("%s already internal buffer alloced, re-alloc after free",
			subdev, subdev, __func__);

		ret = is_subdev_internal_free_buffer(subdev);
		if (ret) {
			mserr("subdev internal free buffer is fail", subdev, subdev);
			ret = -EINVAL;
			goto p_err;
		}
	}

	ret = is_subdev_internal_alloc_buffer(subdev);
	if (ret) {
		mserr("ischain internal alloc buffer fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = _is_subdev_internal_start(subdev);
	if (ret) {
		mserr("_is_subdev_internal_start fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};
KUNIT_EXPORT_SYMBOL(is_subdev_internal_start);

int is_subdev_internal_stop(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	ret = _is_subdev_internal_stop(subdev);
	if (ret) {
		mserr("_is_subdev_internal_stop fail(%d)", subdev, subdev, ret);
		goto p_err;
	}

	ret = is_subdev_internal_free_buffer(subdev);
	if (ret) {
		mserr("subdev internal free buffer is fail(%d)", subdev, subdev, ret);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	msinfo(" %s(%s)(%d)\n", subdev, subdev, __func__, subdev->data_type, ret);
	return ret;
};
KUNIT_EXPORT_SYMBOL(is_subdev_internal_stop);

int is_subdev_internal_s_format(void *device, enum is_device_type type, struct is_subdev *subdev,
	u32 width, u32 height, u32 bits_per_pixel, u32 sbwc_type, u32 lossy_byte32num,
	u32 buffer_num, const char *type_name)
{
	int ret = 0;
	struct is_device_ischain *ischain;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	u32 batch_num = 1;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is not in OPEN state.", subdev, subdev);
		return -EINVAL;
	}

	if (width == 0 || height == 0) {
		mserr("wrong internal vc size(%d x %d)", subdev, subdev, width, height);
		return -EINVAL;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		break;
	case IS_DEVICE_ISCHAIN:
		FIMC_BUG(!device);
		ischain = (struct is_device_ischain *)device;

		if (subdev->id == ENTRY_PAF) {
			if (test_bit(IS_ISCHAIN_REPROCESSING, &ischain->state))
				break;

			sensor = ischain->sensor;
			if (!sensor) {
				mserr("failed to get sensor", subdev, subdev);
				return -EINVAL;
			}

			sensor_cfg = sensor->cfg;
			if (!sensor_cfg) {
				mserr("failed to get senso_cfgr", subdev, subdev);
				return -EINVAL;
			}

			if (sensor_cfg->ex_mode == EX_DUALFPS_960)
				batch_num = 16;
			else if (sensor_cfg->ex_mode == EX_DUALFPS_480)
				batch_num = 8;
		}
		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

	if (!subdev->bits_per_pixel)
		subdev->bits_per_pixel = bits_per_pixel;
	subdev->memory_bitwidth = bits_per_pixel;
	subdev->sbwc_type = sbwc_type;
	subdev->lossy_byte32num = lossy_byte32num;

	subdev->buffer_num = buffer_num;
	subdev->batch_num = batch_num;

	snprintf(subdev->data_type, sizeof(subdev->data_type), "%s", type_name);

	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

	return ret;
};

int is_subdev_internal_g_bpp(void *device, enum is_device_type type, struct is_subdev *subdev,
	struct is_sensor_cfg *sensor_cfg)
{
	int bpp;

	switch (subdev->id) {
	case ENTRY_YPP:
		return 10;
	case ENTRY_MCS:
		return 10;
	case ENTRY_PAF:
		if (!sensor_cfg)
			return BITS_PER_BYTE *2;

		switch (sensor_cfg->output[CSI_VIRTUAL_CH_0].hwformat) {
		case HW_FORMAT_RAW10:
			bpp = 10;
			break;
		case HW_FORMAT_RAW12:
			bpp = 12;
			break;
		default:
			bpp = BITS_PER_BYTE * 2;
			break;
		}

		if (sensor_cfg->votf && sensor_cfg->img_pd_ratio == IS_IMG_PD_RATIO_1_1)
			set_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state);
		else
			clear_bit(IS_SUBDEV_IOVA_EXTENSION_USE, &subdev->state);

		return bpp;
	default:
		return BITS_PER_BYTE * 2; /* 2byte = 16bits */
	}
}

int is_subdev_internal_open(void *device, enum is_device_type type, int vid,
				struct is_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;

	if (test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mswarn("already INTERNAL_USE state", subdev, subdev);
		goto p_err;
	}

	switch (type) {
	case IS_DEVICE_SENSOR:
		sensor = (struct is_device_sensor *)device;

		if (!test_bit(IS_SUBDEV_OPEN, &subdev->state)) {
			ret = is_subdev_open(subdev, NULL, NULL, sensor->instance);
			if (ret) {
				mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
				goto p_err;
			}
		}

		msinfo("[SS%d][V:%d] %s\n", sensor, subdev, sensor->device_id, vid, __func__);
		break;
	case IS_DEVICE_ISCHAIN:
		ischain = (struct is_device_ischain *)device;

		ret = is_subdev_open(subdev, NULL, NULL, ischain->instance);
		if (ret) {
			mserr("is_subdev_open is fail(%d)", subdev, subdev, ret);
			goto p_err;
		}
		msinfo("[V:%d] %s\n", subdev, subdev, vid, __func__);
		break;
	default:
		err("invalid device_type(%d)", type);
		ret = -EINVAL;
		break;
	}

	subdev->vid = vid;

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

p_err:
	return ret;
};

int is_subdev_internal_close(void *device, enum is_device_type type, struct is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state)) {
		mserr("subdev is not in INTERNAL_USE state.", subdev, subdev);
		return -EINVAL;
	}

	if (!test_bit(IS_SUBDEV_EXTERNAL_USE, &subdev->state)) {
		ret = is_subdev_close(subdev);
		if (ret)
			mserr("is_subdev_close is fail(%d)", subdev, subdev, ret);
	}

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	return ret;
};
