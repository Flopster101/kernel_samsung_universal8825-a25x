/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is group manager functions
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
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "is-interface-wrap.h"
#include "is-votfmgr.h"
#include "is-type.h"
#include "is-core.h"
#include "is-err.h"
#include "is-video.h"
#include "is-framemgr.h"
#include "is-groupmgr.h"
#include "is-devicemgr.h"
#include "is-cmd.h"
#include "is-dvfs.h"
#include "is-debug.h"
#include "is-hw.h"
#include "is-vender.h"
#if defined(CONFIG_CAMERA_PDP)
#include "is-interface-sensor.h"
#include "is-device-sensor-peri.h"
#endif

static struct is_group_frame dummy_gframe;

static inline void smp_shot_init(struct is_group *group, u32 value)
{
	atomic_set(&group->smp_shot_count, value);
}

static inline int smp_shot_get(struct is_group *group)
{
	return atomic_read(&group->smp_shot_count);
}

static inline void smp_shot_inc(struct is_group *group)
{
	atomic_inc(&group->smp_shot_count);
}

static inline void smp_shot_dec(struct is_group *group)
{
	atomic_dec(&group->smp_shot_count);
}

static void is_gframe_s_info(struct is_group_frame *gframe,
	u32 slot, struct is_frame *frame)
{
	FIMC_BUG_VOID(!gframe);
	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(!frame->shot_ext);
	FIMC_BUG_VOID(slot >= GROUP_SLOT_MAX);

	memcpy(&gframe->group_cfg[slot], &frame->shot_ext->node_group,
		sizeof(struct camera2_node_group));
}

static void is_gframe_free_head(struct is_group_framemgr *gframemgr,
	struct is_group_frame **gframe)
{
	if (gframemgr->gframe_cnt)
		*gframe = container_of(gframemgr->gframe_head.next, struct is_group_frame, list);
	else
		*gframe = NULL;
}

static void is_gframe_s_free(struct is_group_framemgr *gframemgr,
	struct is_group_frame *gframe)
{
	FIMC_BUG_VOID(!gframemgr);
	FIMC_BUG_VOID(!gframe);

	list_add_tail(&gframe->list, &gframemgr->gframe_head);
	gframemgr->gframe_cnt++;
}

static void is_gframe_print_free(struct is_group_framemgr *gframemgr)
{
	struct is_group_frame *gframe, *temp;

	FIMC_BUG_VOID(!gframemgr);

	printk(KERN_ERR "[GFM] fre(%d) :", gframemgr->gframe_cnt);

	list_for_each_entry_safe(gframe, temp, &gframemgr->gframe_head, list) {
		printk(KERN_CONT "%d->", gframe->fcount);
	}

	printk(KERN_CONT "X\n");
}

static void is_gframe_group_head(struct is_group *group,
	struct is_group_frame **gframe)
{
	if (group->gframe_cnt)
		*gframe = container_of(group->gframe_head.next, struct is_group_frame, list);
	else
		*gframe = NULL;
}

static void is_gframe_s_group(struct is_group *group,
	struct is_group_frame *gframe)
{
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!gframe);

	list_add_tail(&gframe->list, &group->gframe_head);
	group->gframe_cnt++;
}

static void is_gframe_print_group(struct is_group *group)
{
	struct is_group_frame *gframe, *temp;

	while (group) {
		printk(KERN_ERR "[GP%d] req(%d) :", group->id, group->gframe_cnt);

		list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
			printk(KERN_CONT "%d->", gframe->fcount);
		}

		printk(KERN_CONT "X\n");

		group = group->gnext;
	}
}

static int is_gframe_check(struct is_group *gprev,
	struct is_group *group,
	struct is_group *gnext,
	struct is_group_frame *gframe,
	struct is_frame *frame)
{
	int ret = 0;
	u32 capture_id;
	struct is_device_ischain *device;
	struct is_crop *incrop, *otcrop, *canv;
	struct is_subdev *subdev, *junction;
	struct camera2_node *node;

	FIMC_BUG(!group);
	FIMC_BUG(!group->device);
	FIMC_BUG(group->slot >= GROUP_SLOT_MAX);
	FIMC_BUG(gprev && (gprev->slot >= GROUP_SLOT_MAX));

	device = group->device;

#ifndef DISABLE_CHECK_PERFRAME_FMT_SIZE
	/*
	 * perframe check
	 * 1. perframe size can't exceed s_format size
	 */
	incrop = (struct is_crop *)gframe->group_cfg[group->slot].leader.input.cropRegion;
	subdev = &group->leader;
	if ((incrop->w * incrop->h) > (subdev->input.width * subdev->input.height)) {
		mrwarn("the input size is invalid(%dx%d > %dx%d)", group, gframe,
			incrop->w, incrop->h, subdev->input.width, subdev->input.height);
		incrop->w = subdev->input.width;
		incrop->h = subdev->input.height;
		frame->shot_ext->node_group.leader.input.cropRegion[2] = incrop->w;
		frame->shot_ext->node_group.leader.input.cropRegion[3] = incrop->h;
	}
#endif

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		node = &gframe->group_cfg[group->slot].capture[capture_id];
		if (!is_subdev_check_vid(node->vid))
			continue; /* Ignore it */

		subdev = video2subdev(IS_ISCHAIN_SUBDEV, (void *)device, node->vid);
		if (!subdev) {
			mgerr("subdev is NULL", group, group);
			ret = -EINVAL;
			node->request = 0;
			node->vid = 0;
			goto p_err;
		}

#ifndef DISABLE_CHECK_PERFRAME_FMT_SIZE
		otcrop = (struct is_crop *)node->output.cropRegion;
		if ((otcrop->w * otcrop->h) > (subdev->output.width * subdev->output.height)) {
			mrwarn("[V%d][req:%d] the output size is invalid(perframe:%dx%d > subdev:%dx%d)", group, gframe,
				node->vid, node->request,
				otcrop->w, otcrop->h, subdev->output.width, subdev->output.height);
			otcrop->w = subdev->output.width;
			otcrop->h = subdev->output.height;
			frame->shot_ext->node_group.capture[capture_id].output.cropRegion[2] = otcrop->w;
			frame->shot_ext->node_group.capture[capture_id].output.cropRegion[3] = otcrop->h;
		}
#endif
		subdev->cid = capture_id;
	}

	/*
	 * junction check
	 * 1. skip if previous is empty
	 * 2. previous capture size should be bigger than current leader size
	 */
	if (!gprev)
		goto check_gnext;

	junction = gprev->tail->junction;

	if (!junction) {
		mgerr("junction is NULL", gprev, gprev);
		ret = -EINVAL;
		goto p_err;
	}

	/* LVN has no capture subdev, so we can find actual index manually */
	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		if (junction->cid < CAPTURE_NODE_MAX)
			goto p_skip;

		for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
			node = &gframe->group_cfg[group->slot].capture[capture_id];
			if (node->vid == gprev->junction_vid) {
				junction->cid = capture_id;
				mdbgd_group("%s: capture id(%d) is set by force1\n",
						group, group_id_name[group->id],
						junction->cid);
				break;
			}
		}
	}
p_skip:
	if (junction->cid >= CAPTURE_NODE_MAX) {
		mgerr("capture id(%d) is invalid", gprev, gprev, junction->cid);
		ret = -EFAULT;
		goto p_err;
	}

	canv = &gframe->canv;
	incrop = (struct is_crop *)gframe->group_cfg[group->slot].leader.input.cropRegion;
#ifndef DISABLE_CHECK_PERFRAME_FMT_SIZE
	if ((canv->w * canv->h) < (incrop->w * incrop->h)) {
		mrwarn("input crop size is bigger than output size of previous group(GP%d(%d,%d,%d,%d) < GP%d(%d,%d,%d,%d))",
			group, gframe,
			gprev->id, canv->x, canv->y, canv->w, canv->h,
			group->id, incrop->x, incrop->y, incrop->w, incrop->h);
		*incrop = *canv;
		frame->shot_ext->node_group.leader.input.cropRegion[0] = incrop->x;
		frame->shot_ext->node_group.leader.input.cropRegion[1] = incrop->y;
		frame->shot_ext->node_group.leader.input.cropRegion[2] = incrop->w;
		frame->shot_ext->node_group.leader.input.cropRegion[3] = incrop->h;
	}
#endif
	/* set input size of current group as output size of previous group */
	group->leader.input.canv = *canv;

check_gnext:
	/*
	 * junction check
	 * 1. skip if next is empty
	 * 2. current capture size should be smaller than next leader size.
	 */
	if (!gnext)
		goto p_err;

	junction = group->tail->junction;

	if (!junction) {
		mgerr("junction is NULL", group, group);
		ret = -EINVAL;
		goto p_err;
	}

	/* LVN has no capture subdev, so we can find actual index manually */
	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		if (junction->cid < CAPTURE_NODE_MAX)
			goto p_skip2;

		for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
			node = &gframe->group_cfg[group->slot].capture[capture_id];
			if (node->vid == gnext->junction_vid) {
				junction->cid = capture_id;
				mdbgd_group("%s: capture id(%d) is set by force2\n",
						group, group_id_name[group->id],
						junction->cid);
				break;
			}
		}
	}
p_skip2:
	if (junction->cid >= CAPTURE_NODE_MAX) {
		mgerr("capture id(%d) is invalid", group, group, junction->cid);
		ret = -EFAULT;
		goto p_err;
	}

	/* When dma request for next group is empty, this size doesn`t have to be checked. */
	if (gframe->group_cfg[group->slot].capture[junction->cid].request == 0)
		goto p_err;

	otcrop = (struct is_crop *)gframe->group_cfg[group->slot].capture[junction->cid].output.cropRegion;
	subdev = &gnext->leader;
#ifndef DISABLE_CHECK_PERFRAME_FMT_SIZE
	if ((otcrop->w * otcrop->h) > (subdev->input.width * subdev->input.height)) {
		mrwarn("the output size bigger than input size of next group(GP%d(%dx%d) > GP%d(%dx%d))",
			group, gframe,
			group->id, otcrop->w, otcrop->h,
			gnext->id, subdev->input.width, subdev->input.height);
		otcrop->w = subdev->input.width;
		otcrop->h = subdev->input.height;
		frame->shot_ext->node_group.capture[junction->cid].output.cropRegion[2] = otcrop->w;
		frame->shot_ext->node_group.capture[junction->cid].output.cropRegion[3] = otcrop->h;
	}
#endif

	/* set canvas size of next group as output size of currnet group */
	gframe->canv = *otcrop;

p_err:
	return ret;
}

static int is_gframe_trans_fre_to_grp(struct is_group_framemgr *gframemgr,
	struct is_group_frame *gframe,
	struct is_group *group,
	struct is_group *gnext)
{
	int ret = 0;

	FIMC_BUG(!gframemgr);
	FIMC_BUG(!gframe);
	FIMC_BUG(!group);
	FIMC_BUG(!gnext);
	FIMC_BUG(!group->tail);
	FIMC_BUG(!group->tail->junction);

	if (unlikely(!gframemgr->gframe_cnt)) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	if (gframe->group_cfg[group->slot].capture[group->tail->junction->cid].request) {
		list_del(&gframe->list);
		gframemgr->gframe_cnt--;
		is_gframe_s_group(gnext, gframe);
	}

p_err:
	return ret;
}

static int is_gframe_trans_grp_to_grp(struct is_group_framemgr *gframemgr,
	struct is_group_frame *gframe,
	struct is_group *group,
	struct is_group *gnext)
{
	int ret = 0;

	FIMC_BUG(!gframemgr);
	FIMC_BUG(!gframe);
	FIMC_BUG(!group);
	FIMC_BUG(!gnext);
	FIMC_BUG(!group->tail);
	FIMC_BUG(!group->tail->junction);

	if (unlikely(!group->gframe_cnt)) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	if (gframe->group_cfg[group->slot].capture[group->tail->junction->cid].request) {
		list_del(&gframe->list);
		group->gframe_cnt--;
		is_gframe_s_group(gnext, gframe);
	} else {
		list_del(&gframe->list);
		group->gframe_cnt--;
		is_gframe_s_free(gframemgr, gframe);
	}

p_err:
	return ret;
}

static int is_gframe_trans_grp_to_fre(struct is_group_framemgr *gframemgr,
	struct is_group_frame *gframe,
	struct is_group *group)
{
	int ret = 0;

	FIMC_BUG(!gframemgr);
	FIMC_BUG(!gframe);
	FIMC_BUG(!group);

	if (!group->gframe_cnt) {
		merr("gframe_cnt is zero", group);
		ret = -EFAULT;
		goto p_err;
	}

	list_del(&gframe->list);
	group->gframe_cnt--;
	is_gframe_s_free(gframemgr, gframe);

p_err:
	return ret;
}

void * is_gframe_rewind(struct is_groupmgr *groupmgr,
	struct is_group *group, u32 target_fcount)
{
	struct is_group_framemgr *gframemgr;
	struct is_group_frame *gframe, *temp;

	FIMC_BUG_NULL(!groupmgr);
	FIMC_BUG_NULL(!group);
	FIMC_BUG_NULL(group->instance >= IS_STREAM_COUNT);

	gframemgr = &groupmgr->gframemgr[group->instance];

	list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
		if (gframe->fcount == target_fcount)
			break;

		if (gframe->fcount > target_fcount) {
			mgwarn("qbuf fcount(%d) is smaller than expect fcount(%d)", group, group,
				target_fcount, gframe->fcount);
			break;
		}

		list_del(&gframe->list);
		group->gframe_cnt--;
		mgwarn("gframe%d is cancel(count : %d)", group, group, gframe->fcount, group->gframe_cnt);
		is_gframe_s_free(gframemgr, gframe);
	}

	if (!group->gframe_cnt) {
		merr("gframe%d can't be found", group, target_fcount);
		gframe = NULL;
	}

	return gframe;
}

void *is_gframe_group_find(struct is_group *group, u32 target_fcount)
{
	struct is_group_frame *gframe;

	FIMC_BUG_NULL(!group);
	FIMC_BUG_NULL(group->instance >= IS_STREAM_COUNT);

	list_for_each_entry(gframe, &group->gframe_head, list) {
		if (gframe->fcount == target_fcount)
			return gframe;
	}

	return NULL;
}

int is_gframe_flush(struct is_groupmgr *groupmgr,
	struct is_group *group)
{
	int ret = 0;
	unsigned long flag;
	struct is_group_framemgr *gframemgr;
	struct is_group_frame *gframe, *temp;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);

	gframemgr = &groupmgr->gframemgr[group->instance];

	spin_lock_irqsave(&gframemgr->gframe_slock, flag);

	list_for_each_entry_safe(gframe, temp, &group->gframe_head, list) {
		list_del(&gframe->list);
		group->gframe_cnt--;
		is_gframe_s_free(gframemgr, gframe);
	}

	spin_unlock_irqrestore(&gframemgr->gframe_slock, flag);

	return ret;
}

unsigned long is_group_lock(struct is_group *group,
		enum is_device_type device_type,
		bool leader_lock)
{
	u32 entry;
	unsigned long flags = 0;
	struct is_subdev *subdev;
	struct is_framemgr *ldr_framemgr, *sub_framemgr;
	struct is_group *pos;

	FIMC_BUG(!group);

	if (leader_lock) {
		ldr_framemgr = GET_HEAD_GROUP_FRAMEMGR(group, 0);
		if (!ldr_framemgr) {
			mgerr("ldr_framemgr is NULL", group, group);
			BUG();
		}
		framemgr_e_barrier_irqs(ldr_framemgr, FMGR_IDX_20, flags);
	}

	for_each_group_child(pos, group) {
		/* skip the lock by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		for (entry = 0; entry < ENTRY_END; ++entry) {
			subdev = pos->subdev[entry];
			pos->locked_sub_framemgr[entry] = NULL;

			if (!subdev)
				continue;

			if (&pos->leader == subdev)
				continue;

			sub_framemgr = GET_SUBDEV_FRAMEMGR(subdev);
			if (!sub_framemgr)
				continue;

			if (!test_bit(IS_SUBDEV_START, &subdev->state))
				continue;

			framemgr_e_barrier(sub_framemgr, FMGR_IDX_19);
			pos->locked_sub_framemgr[entry] = sub_framemgr;
		}
	}

	return flags;
}

void is_group_unlock(struct is_group *group, unsigned long flags,
		enum is_device_type device_type,
		bool leader_lock)
{
	u32 entry;
	struct is_framemgr *ldr_framemgr;
	struct is_group *pos;

	FIMC_BUG_VOID(!group);

	if (leader_lock) {
		ldr_framemgr = GET_HEAD_GROUP_FRAMEMGR(group, 0);
		if (!ldr_framemgr) {
			mgerr("ldr_framemgr is NULL", group, group);
			BUG();
		}
	}

	for_each_group_child(pos, group) {
		/* skip the unlock by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		for (entry = 0; entry < ENTRY_END; ++entry) {
			if (pos->locked_sub_framemgr[entry]) {
				framemgr_x_barrier(pos->locked_sub_framemgr[entry],
							FMGR_IDX_19);
				pos->locked_sub_framemgr[entry] = NULL;
			}
		}
	}

	if (leader_lock)
		framemgr_x_barrier_irqr(ldr_framemgr, FMGR_IDX_20, flags);
}

void is_group_subdev_cancel(struct is_group *group,
		struct is_frame *ldr_frame,
		enum is_device_type device_type,
		enum is_frame_state frame_state,
		bool flush)
{
	struct is_subdev *subdev;
	struct is_video_ctx *sub_vctx;
	struct is_framemgr *sub_framemgr;
	struct is_frame *sub_frame;
	struct is_group *pos;

	for_each_group_child(pos, group) {
		/* skip the subdev device by device type */
		if (pos->device_type != device_type &&
				device_type != IS_DEVICE_MAX)
			break;

		list_for_each_entry(subdev, &pos->subdev_list, list) {
			sub_vctx = subdev->vctx;
			if (!sub_vctx)
				continue;

			sub_framemgr = GET_FRAMEMGR(sub_vctx);
			if (!sub_framemgr)
				continue;

			if (!test_bit(IS_SUBDEV_START, &subdev->state))
				continue;

			if (subdev->cid >= CAPTURE_NODE_MAX)
				continue;

			if (!flush && ldr_frame->shot_ext->node_group.capture[subdev->cid].request == 0)
				continue;

			do {
				sub_frame = peek_frame(sub_framemgr, frame_state);
				if (sub_frame) {
					if (sub_frame->fcount > ldr_frame->fcount) {
						msrinfo("[WRN] SKIP CANCEL(SBUF:%d > LDRF:%d)(%d, %d)\n",
								pos, subdev, ldr_frame,
								sub_frame->fcount, ldr_frame->fcount,
								sub_frame->index, sub_frame->state);
						break;
					}
					if (sub_frame->stream) {
						sub_frame->stream->fvalid = 0;
						sub_frame->stream->fcount = ldr_frame->fcount;
						sub_frame->stream->rcount = ldr_frame->rcount;
					}
					clear_bit(subdev->id, &ldr_frame->out_flag);
					trans_frame(sub_framemgr, sub_frame, FS_COMPLETE);
					msrinfo("[ERR] CANCEL(%d, %d)\n", pos, subdev, ldr_frame,
								sub_frame->index, sub_frame->state);
					CALL_VOPS(sub_vctx, done, sub_frame->index, VB2_BUF_STATE_ERROR);
				} else {
					msrinfo("[ERR] There's no frame\n", pos, subdev, ldr_frame);
#ifdef DEBUG
					frame_manager_print_queues(sub_framemgr);
#endif
				}
			} while (sub_frame && flush);

			if (sub_vctx->video->try_smp)
				up(&sub_vctx->video->smp_multi_input);
		}
	}
}

static void is_group_cancel(struct is_group *group,
	struct is_frame *ldr_frame)
{
	u32 wait_count = 300;
	unsigned long flags;
	struct is_video_ctx *ldr_vctx;
	struct is_framemgr *ldr_framemgr;
	struct is_frame *prev_frame, *next_frame;

	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!ldr_frame);

	ldr_vctx = group->head->leader.vctx;
	ldr_framemgr = GET_FRAMEMGR(ldr_vctx);
	if (!ldr_framemgr) {
		mgerr("ldr_framemgr is NULL", group, group);
		BUG();
	}

p_retry:
	if (!ldr_framemgr->num_frames) {
		mgerr("ldr_framemgr is already closed", group, group);
		return;
	}

	flags = is_group_lock(group, IS_DEVICE_MAX, true);

	next_frame = peek_frame_tail(ldr_framemgr, FS_FREE);
	if (wait_count && next_frame && next_frame->out_flag) {
		mginfo("next frame(F%d) is on process1(%lX %lX), waiting...\n", group, group,
			next_frame->fcount, next_frame->bak_flag, next_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	next_frame = peek_frame_tail(ldr_framemgr, FS_COMPLETE);
	if (wait_count && next_frame && next_frame->out_flag) {
		mginfo("next frame(F%d) is on process2(%lX %lX), waiting...\n", group, group,
			next_frame->fcount, next_frame->bak_flag, next_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	prev_frame = peek_frame(ldr_framemgr, FS_PROCESS);
	if (wait_count && prev_frame && prev_frame->bak_flag != prev_frame->out_flag) {
		mginfo("prev frame(F%d) is on process(%lX %lX), waiting...\n", group, group,
			prev_frame->fcount, prev_frame->bak_flag, prev_frame->out_flag);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		usleep_range(1000, 1000);
		wait_count--;
		goto p_retry;
	}

	if (ldr_frame->state == FS_COMPLETE || ldr_frame->state == FS_FREE) {
		mgrwarn("Can't cancel this frame because state(%s) is invalid, skip buffer done",
			group, group, ldr_frame,
			frame_state_name[ldr_frame->state]);
		is_group_unlock(group, flags, IS_DEVICE_MAX, true);
		return;
	}

	is_group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	clear_bit(group->leader.id, &ldr_frame->out_flag);
	trans_frame(ldr_framemgr, ldr_frame, FS_COMPLETE);

	mgrinfo("[ERR] CANCEL(i%d)(R%d, P%d, C%d)\n", group, group, ldr_frame,
		ldr_frame->index,
		ldr_framemgr->queued_count[FS_REQUEST],
		ldr_framemgr->queued_count[FS_PROCESS],
		ldr_framemgr->queued_count[FS_COMPLETE]);

	CALL_VOPS(ldr_vctx, done, ldr_frame->index, VB2_BUF_STATE_ERROR);

	is_group_unlock(group, flags, IS_DEVICE_MAX, true);
}

static void is_group_s_leader(struct is_group *group,
	struct is_subdev *leader, bool force)
{
	struct is_subdev *subdev;

	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!leader);

	subdev = &group->leader;
	subdev->leader = leader;

	list_for_each_entry(subdev, &group->subdev_list, list) {
		/*
		 * TODO: Remove this error check logic.
		 * For MC-scaler group, this warning message could be printed
		 * because each capture node is shared by different output node.
		 */
		if (leader->vctx && subdev->vctx &&
			(leader->vctx->refcount < subdev->vctx->refcount)) {
			mgwarn("Invalid subdev instance (%s(%u) < %s(%u))",
				group, group,
				leader->name, leader->vctx->refcount,
				subdev->name, subdev->vctx->refcount);
		}

		if (force || test_bit(IS_SUBDEV_OPEN, &subdev->state))
			subdev->leader = leader;
	}
}

static void is_stream_status(struct is_groupmgr *groupmgr,
	struct is_group *group)
{
	unsigned long flags;
	struct is_queue *queue;
	struct is_framemgr *framemgr;

	FIMC_BUG_VOID(!groupmgr);
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(group->id >= GROUP_ID_MAX);

	while (group) {
		FIMC_BUG_VOID(!group->leader.vctx);

		queue = GET_SUBDEV_QUEUE(&group->leader);
		framemgr = &queue->framemgr;

		mginfo(" ginfo(res %d, rcnt %d, pos %d)\n", group, group,
			groupmgr->gtask[group->id].smp_resource.count,
			atomic_read(&group->rcount),
			group->pcount);
		mginfo(" vinfo(req %d, pre %d, que %d, com %d, dqe %d)\n", group, group,
			queue->buf_req,
			queue->buf_pre,
			queue->buf_que,
			queue->buf_com,
			queue->buf_dqe);

		/* print framemgr's frame info */
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		frame_manager_print_queues(framemgr);
		framemgr_x_barrier_irqr(framemgr, 0, flags);

		group = group->gnext;
	}
}

#ifdef DEBUG_AA
static void is_group_debug_aa_shot(struct is_group *group,
	struct is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->ctl.aa.vendor_aeflashMode != group->flashmode) {
		group->flashmode = ldr_frame->shot->ctl.aa.vendor_aeflashMode;
		info("flash ctl : %d(%d)\n", group->flashmode, ldr_frame->fcount);
	}
#endif
}

static void is_group_debug_aa_done(struct is_group *group,
	struct is_frame *ldr_frame)
{
	if (group->prev)
		return;

#ifdef DEBUG_FLASH
	if (ldr_frame->shot->dm.flash.vendor_firingStable != group->flash.vendor_firingStable) {
		group->flash.vendor_firingStable = ldr_frame->shot->dm.flash.vendor_firingStable;
		info("flash stable : %d(%d)\n", group->flash.vendor_firingStable, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.vendor_flashReady!= group->flash.vendor_flashReady) {
		group->flash.vendor_flashReady = ldr_frame->shot->dm.flash.vendor_flashReady;
		info("flash ready : %d(%d)\n", group->flash.vendor_flashReady, ldr_frame->fcount);
	}

	if (ldr_frame->shot->dm.flash.vendor_flashOffReady!= group->flash.vendor_flashOffReady) {
		group->flash.vendor_flashOffReady = ldr_frame->shot->dm.flash.vendor_flashOffReady;
		info("flash off : %d(%d)\n", group->flash.vendor_flashOffReady, ldr_frame->fcount);
	}
#endif
}
#endif

static void is_group_set_torch(struct is_group *group,
	struct is_frame *ldr_frame)
{
	if (group->prev)
		return;

	if (group->aeflashMode != ldr_frame->shot->ctl.aa.vendor_aeflashMode) {
		group->aeflashMode = ldr_frame->shot->ctl.aa.vendor_aeflashMode;
		is_vender_set_torch(ldr_frame->shot);
	}

	return;
}

static bool is_group_kthread_queue_work(struct is_group_task *gtask, struct is_group *group, struct is_frame *frame)
{
	TIME_SHOT(TMS_Q);
#ifdef VH_FPSIMD_API
	is_fpsimd_set_task_using(gtask->task, &gtask->fp_state);
#endif
	return kthread_queue_work(&gtask->worker, &frame->work);
}

#ifdef ENABLE_SYNC_REPROCESSING
struct is_device_ischain *main_dev = NULL;

IS_TIMER_FUNC(is_group_trigger_timer)
{
	struct is_groupmgr *groupmgr = from_timer(groupmgr, (struct timer_list *)data, trigger_timer);
	struct is_frame *rframe = NULL;
	struct is_group_task *gtask;
	struct is_group *group;
	u32 instance;
	unsigned long flag;

	instance = groupmgr->trigger_instance;
	group = groupmgr->group[instance][GROUP_SLOT_ISP];

	gtask = &groupmgr->gtask[group->id];
	instance = group->instance;

	spin_lock_irqsave(&groupmgr->trigger_slock, flag);
	if (!list_empty(&gtask->sync_list)) {
		rframe = list_first_entry(&gtask->sync_list, struct is_frame, sync_list);
		list_del(&rframe->sync_list);
		is_group_kthread_queue_work(gtask, group, rframe);
	}
	spin_unlock_irqrestore(&groupmgr->trigger_slock, flag);
}
#endif
static void is_group_start_trigger(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame)
{
	struct is_group_task *gtask;
#ifdef ENABLE_SYNC_REPROCESSING
	struct is_frame *rframe = NULL;
	struct is_device_sensor *sensor;
	struct is_device_ischain *device;
	struct is_core* core;
	int i;
	int loop_cnt;
	unsigned long flag;
#endif

	FIMC_BUG_VOID(!groupmgr);
	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!frame);
	FIMC_BUG_VOID(group->id >= GROUP_ID_MAX);

	atomic_inc(&group->rcount);

	gtask = &groupmgr->gtask[group->id];
#ifdef ENABLE_SYNC_REPROCESSING
	sensor = group->sensor;
	device = group->device;
	core = (struct is_core *)device->interface->core;

	if ((atomic_read(&gtask->refcount) > 1)
		&& !test_bit(IS_GROUP_OTF_INPUT, &group->state) &&(group->id == GROUP_ID_ISP0)) {
		if (test_bit(IS_ISCHAIN_REPROCESSING, &group->device->state)) {
			list_add_tail(&frame->sync_list, &gtask->sync_list);
			groupmgr->trigger_instance = group->instance;
			mod_timer(&groupmgr->trigger_timer, jiffies + msecs_to_jiffies(300));
			return;
		} else {
			/* trigger_timer reset in preview path */
			if (timer_pending(&groupmgr->trigger_timer))
				del_timer(&groupmgr->trigger_timer);

			/* main */
			if (device == main_dev) {
				is_group_kthread_queue_work(gtask, group, frame);
				for (i = 0; i < IS_STREAM_COUNT; i++) {
					if (i == group->instance)
						continue;
					loop_cnt = 0;
					/* process Preview queue */
					if (!list_empty(&gtask->preview_list[i])) {
						loop_cnt = (int)core->ischain[i].sensor->cfg->framerate / (int)main_dev->sensor->cfg->framerate;
						/* main fps <= sub fps */
						if (loop_cnt) {
							while (loop_cnt-- && !list_empty(&gtask->preview_list[i])) {
								atomic_dec(&gtask->preview_cnt[i]);
								rframe = list_first_entry(&gtask->preview_list[i], struct is_frame, preview_list);
								list_del(&rframe->preview_list);
								is_group_kthread_queue_work(gtask, group, rframe);
							}
						} else {
							if (list_empty(&gtask->preview_list[i]))
								break;
							atomic_dec(&gtask->preview_cnt[i]);
							rframe = list_first_entry(&gtask->preview_list[i], struct is_frame, preview_list);
							list_del(&rframe->preview_list);
							is_group_kthread_queue_work(gtask, group, rframe);
						}
					}
				}
				/* process Capture queue */
				spin_lock_irqsave(&groupmgr->trigger_slock, flag);
				if (!list_empty(&gtask->sync_list)) {
					rframe = list_first_entry(&gtask->sync_list, struct is_frame, sync_list);
					list_del(&rframe->sync_list);
					is_group_kthread_queue_work(gtask, group, rframe);
				}
				spin_unlock_irqrestore(&groupmgr->trigger_slock, flag);
			} else {
				loop_cnt = (int)core->ischain[group->instance].sensor->cfg->framerate / (int)main_dev->sensor->cfg->framerate;
				if ((!list_empty(&gtask->preview_list[group->instance]))
					&& atomic_read(&gtask->preview_cnt[group->instance]) >= loop_cnt) {
						atomic_dec(&gtask->preview_cnt[group->instance]);
						rframe = list_first_entry(&gtask->preview_list[group->instance], struct is_frame, preview_list);
						list_del(&rframe->preview_list);
						is_group_kthread_queue_work(gtask, group, rframe);
				}

				atomic_inc(&gtask->preview_cnt[group->instance]);
				list_add_tail(&frame->preview_list, &gtask->preview_list[group->instance]);
			}
		}
	}
	else
		is_group_kthread_queue_work(gtask, group, frame);
#else
	is_group_kthread_queue_work(gtask, group, frame);
#endif
}

static int is_group_task_probe(struct is_group_task *gtask,
	u32 id)
{
	int ret = 0;

	FIMC_BUG(!gtask);
	FIMC_BUG(id >= GROUP_ID_MAX);

	gtask->id = id;
	atomic_set(&gtask->refcount, 0);
	clear_bit(IS_GTASK_START, &gtask->state);
	clear_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	spin_lock_init(&gtask->gtask_slock);

	return ret;
}

static int is_group_task_start(struct is_groupmgr *groupmgr,
	struct is_group_task *gtask,
	int slot)
{
	int ret = 0;
	char name[30];
	struct sched_param param;
#ifdef ENABLE_SYNC_REPROCESSING
	int i;
#endif
	FIMC_BUG(!groupmgr);
	FIMC_BUG(!gtask);

	if (test_bit(IS_GTASK_START, &gtask->state))
		goto p_work;

	sema_init(&gtask->smp_resource, 0);
	kthread_init_worker(&gtask->worker);
	snprintf(name, sizeof(name), "is_gw%d", gtask->id);
	gtask->task = kthread_run(kthread_worker_fn, &gtask->worker, name);
	if (IS_ERR(gtask->task)) {
		err("failed to create group_task%d, err(%ld)\n",
			gtask->id, PTR_ERR(gtask->task));
		ret = PTR_ERR(gtask->task);
		goto p_err;
	}

	param.sched_priority = (slot == GROUP_SLOT_SENSOR) ?
		TASK_GRP_OTF_INPUT_PRIO : TASK_GRP_DMA_INPUT_PRIO;

	ret = sched_setscheduler_nocheck(gtask->task, SCHED_FIFO, &param);
	if (ret) {
		err("sched_setscheduler_nocheck is fail(%d)", ret);
		goto p_err;
	}

#ifndef VH_FPSIMD_API
	is_fpsimd_set_task_using(gtask->task, NULL);
#endif

#ifdef ENABLE_SYNC_REPROCESSING
	atomic_set(&gtask->rep_tick, 0);
	INIT_LIST_HEAD(&gtask->sync_list);
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		atomic_set(&gtask->preview_cnt[i], 0);
		INIT_LIST_HEAD(&gtask->preview_list[i]);
	}
#endif

	/* default gtask's smp_resource value is 1 for guerrentee m2m IP operation */
	sema_init(&gtask->smp_resource, 1);
	set_bit(IS_GTASK_START, &gtask->state);

p_work:
	atomic_inc(&gtask->refcount);

p_err:
	return ret;
}

static int is_group_task_stop(struct is_groupmgr *groupmgr,
	struct is_group_task *gtask, u32 slot)
{
	int ret = 0;
	u32 refcount;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!gtask);
	FIMC_BUG(slot >= GROUP_SLOT_MAX);

	if (!test_bit(IS_GTASK_START, &gtask->state)) {
		err("gtask(%d) is not started", gtask->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (IS_ERR_OR_NULL(gtask->task)) {
		err("task of gtask(%d) is invalid(%p)", gtask->id, gtask->task);
		ret = -EINVAL;
		goto p_err;
	}

	refcount = atomic_dec_return(&gtask->refcount);
	if (refcount > 0)
		goto p_err;

	set_bit(IS_GTASK_REQUEST_STOP, &gtask->state);


	if (!list_empty(&gtask->smp_resource.wait_list)) {
		warn("gtask(%d) is not empty", gtask->id);
		is_kernel_log_dump(false);
		up(&gtask->smp_resource);
	}

	/*
	 * flush kthread wait until all work is complete
	 * it's dangerous if all is not finished
	 * so it's commented currently
	 * flush_kthread_worker(&groupmgr->group_worker[slot]);
	 */
	kthread_stop(gtask->task);

	clear_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	clear_bit(IS_GTASK_START, &gtask->state);

p_err:
	return ret;
}

int is_groupmgr_probe(struct platform_device *pdev,
	struct is_groupmgr *groupmgr)
{
	int ret = 0;
	u32 stream, slot, id, index;
	struct is_group_framemgr *gframemgr;

	for (stream = 0; stream < IS_STREAM_COUNT; ++stream) {
		gframemgr = &groupmgr->gframemgr[stream];
		spin_lock_init(&groupmgr->gframemgr[stream].gframe_slock);
		INIT_LIST_HEAD(&groupmgr->gframemgr[stream].gframe_head);
		groupmgr->gframemgr[stream].gframe_cnt = 0;

		gframemgr->gframes = devm_kzalloc(&pdev->dev,
					sizeof(struct is_group_frame) * IS_MAX_GFRAME,
					GFP_KERNEL);
		if (!gframemgr->gframes) {
			probe_err("failed to allocate group frames");
			ret = -ENOMEM;
			goto p_err;
		}

		for (index = 0; index < IS_MAX_GFRAME; ++index)
			is_gframe_s_free(gframemgr, &gframemgr->gframes[index]);

		groupmgr->leader[stream] = NULL;
		for (slot = 0; slot < GROUP_SLOT_MAX; ++slot)
			groupmgr->group[stream][slot] = NULL;
	}

	for (id = 0; id < GROUP_ID_MAX; ++id) {
		ret = is_group_task_probe(&groupmgr->gtask[id], id);
		if (ret) {
			err("is_group_task_probe is fail(%d)", ret);
			goto p_err;
		}

		frame_manager_probe(&groupmgr->shared_framemgr[id].framemgr, id, group_id_name[id]);
		refcount_set(&groupmgr->shared_framemgr[id].refcount, 0);
		mutex_init(&groupmgr->shared_framemgr[id].mlock);
	}

#ifdef ENABLE_SYNC_REPROCESSING
	spin_lock_init(&groupmgr->trigger_slock);
#endif
p_err:
	return ret;
}

static int is_group_path_dmsg(struct is_device_ischain *device,
			struct is_group *leader_group,
			struct is_path_info *path)
{
	struct is_group *group, *next;
	struct is_video_ctx *vctx;
	struct is_video *video;
	struct is_subdev *subdev;
	u32 source_vid;
	u32 *_path = path->group;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		is_dmsg_init();
		is_dmsg_concate("STM(R) PH:");
	} else if (test_bit(IS_GROUP_USE_MULTI_CH, &leader_group->state)) {
		is_dmsg_concate("STM(N:2) PH:");
		_path = path->group_2nd;
	} else {
		is_dmsg_init();
		is_dmsg_concate("STM(N) PH:");
	}

	/*
	 * The Meaning of Symbols.
	 *  1) -> : HAL(OTF), Driver(OTF)
	 *  2) => : HAL(M2M), Driver(M2M)
	 *  3) ~> : HAL(OTF), Driver(M2M)
	 *  4) >> : HAL(OTF), Driver(M2M)
	 *  5) *> : HAL(VOTF), Driver(VOTF)
	 *          It's Same with 3).
	 *          But HAL q/dq junction node between the groups.
	 *  6) ^> : HAL(OTF_2nd), Driver(OTF_2nd)
	 */
	if (test_bit(IS_GROUP_OTF_INPUT, &leader_group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &leader_group->state))
			is_dmsg_concate(" %02d *> ", leader_group->source_vid);
		else if (test_bit(IS_GROUP_USE_MULTI_CH, &leader_group->state))
			is_dmsg_concate(" %02d ^> ", leader_group->source_vid);
		else
			is_dmsg_concate(" %02d -> ", leader_group->source_vid);
	} else {
		is_dmsg_concate(" %02d => ", leader_group->source_vid);
	}

	group = leader_group;
	while (group) {

		next = group->next;
		if (next) {
			source_vid = next->source_vid;
			FIMC_BUG(group->slot >= GROUP_SLOT_MAX);
			FIMC_BUG(next->slot >= GROUP_SLOT_MAX);
		} else {
			source_vid = 0;
			_path[group->slot] = group->logical_id;
		}

		is_dmsg_concate("GP%s ( ", group_id_name[group->id]);
		list_for_each_entry(subdev, &group->subdev_list, list) {
			vctx = subdev->vctx;
			if (!vctx)
				continue;

			video = vctx->video;
			if (!video) {
				merr("video is NULL", device);
				BUG();
			}

			/* connection check */
			if (video->id == source_vid) {
				is_dmsg_concate("*%02d ", video->id);
				group->junction = subdev;
				_path[group->slot] = group->logical_id;
				if (next)
					_path[next->slot] = next->logical_id;
			} else {
				is_dmsg_concate("%02d ", video->id);
			}

		}
		is_dmsg_concate(")");

		if (next && !group->junction) {
			mgerr("junction subdev can NOT be found", device, group);
			return -EINVAL;
		}


		if (next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
				set_bit(IS_GROUP_OTF_OUTPUT, &group->state);

				/* In only sensor group, VOTF path is determined by sensor mode. */
				if (group->slot == GROUP_SLOT_SENSOR) {
					if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
						set_bit(IS_GROUP_VOTF_INPUT, &next->state);
					else
						clear_bit(IS_GROUP_VOTF_INPUT, &next->state);
				}

				if (test_bit(IS_GROUP_VOTF_INPUT, &next->state)) {
					set_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
					is_dmsg_concate(" *> ");
				} else {
					is_dmsg_concate(" -> ");
				}
			} else {
				is_dmsg_concate(" => ");
			}
		}

		group = next;
	}
	is_dmsg_concate("\n");

	return 0;
}

int is_groupmgr_init(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device)
{
	int ret = 0;
	struct is_path_info *path;
	struct is_subdev *leader;
	struct is_group *group, *prev, *next, *sibling, *pnext;
	struct is_group *leader_group;
	struct is_group *head_2nd = NULL;
	u32 slot, source_vid;
	u32 instance, ginstance;

	prev = NULL;
	instance = device->instance;
	path = &device->path;

	for (slot = 0; slot < GROUP_SLOT_MAX; ++slot) {
		path->group[slot] = GROUP_ID_MAX;
		path->group_2nd[slot] = GROUP_ID_MAX;
	}

	leader_group = groupmgr->leader[instance];
	if (!leader_group) {
		err("stream leader is not selected");
		ret = -EINVAL;
		goto p_err;
	}

	for (ginstance = 0; ginstance < IS_STREAM_COUNT; ginstance++) {
		prev = NULL;

		if (!test_bit(ginstance, &device->ginstance_map))
			continue;

		for (slot = 0; slot < GROUP_SLOT_MAX; ++slot) {
			group = groupmgr->group[ginstance][slot];
			if (!group)
				continue;

			if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state)
					&& (group->instance == device->instance))
				continue;

			/* A group should be initialized, only if the group was placed at the front of leader group */
			if (slot < leader_group->slot)
				continue;

			group->prev = NULL;
			group->next = NULL;
			group->gprev = NULL;
			group->gnext = NULL;
			group->parent = NULL;
			group->child = NULL;
			group->head = group;
			group->tail = group;
			group->junction = NULL;

			source_vid = group->source_vid;
			mdbgd_group("source vid : %02d\n", group, source_vid);
			if (source_vid) {
				leader = &group->leader;
				/* Set force flag to initialize every leader in subdev. */
				is_group_s_leader(group, leader, true);

				if (prev) {
					group->prev = prev;
					prev->next = group;
				} else if (!prev && test_bit(IS_GROUP_USE_MULTI_CH, &group->state)) {
					struct is_group *group_main;

					group_main = groupmgr->group[instance][slot];
					if (!group_main) {
						err("not exist main group(%d)", group->id);
						continue;
					}

					group->prev = group_main->prev;
					group_main->prev->pnext = group;
					head_2nd = group;
				}

				prev = group;
			}
		}
	}

	ret = is_group_path_dmsg(device, leader_group, path);
	if (ret) {
		mgerr("is_group_path_dmsg is failed(%d)", device, leader_group, ret);
		return ret;
	}
	if (head_2nd) {
		is_group_path_dmsg(device, head_2nd, path);
		if (ret) {
			mgerr("is_group_path_dmsg is failed #2(%d)", device, head_2nd, ret);
			return ret;
		}
	}

	group = leader_group;
	sibling = leader_group;
	next = group->next;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
			group->child = next;
			next->parent = next->prev;
			sibling->tail = next;
			next->head = sibling;
			leader = &sibling->leader;
			is_group_s_leader(next, leader, false);
		} else {
			/**
			 * HACK: Put VRA group as a isolated group.
			 * There is some case that user skips queueing VRA buffer,
			 * even though DMA out request of junction node is set.
			 * To prevent the gframe stuck issue,
			 * VRA group must not receive gframe from previous group.
			 */
			if (!next->gframe_skip) {
				sibling->gnext = next;
				next->gprev = sibling;
			}
			sibling = next;
		}

		group = next;
		next = group->next;
	}

	/* each group tail setting */
	group = leader_group;
	sibling = leader_group;
	next = group->next;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state))
			next->tail = sibling->tail;
		else
			sibling = next;

		group = next;
		next = group->next;
	}

	/* multi channel group connection setting */
	sibling = leader_group;
	group = leader_group;
	pnext = leader_group->pnext;
	if (pnext) {
		next = pnext;
		while (next) {
			if (test_bit(IS_GROUP_OTF_INPUT, &next->state)) {
				if (next != pnext)
					group->child = next;
				next->parent = next->prev;
				sibling->ptail = next;
				next->head = sibling;
				leader = &sibling->leader;

				is_group_s_leader(next, leader, false);
			} else {
				/* Not consider M2M connection if pnext use */
			}
			group = next;
			next = group->next;
		}
	}

	group = leader_group;
	sibling = leader_group;
	next = group->pnext;
	while (next) {
		if (test_bit(IS_GROUP_OTF_INPUT, &next->state))
			next->tail = sibling->ptail;
		else
			sibling = next;

		group = next;
		next = group->next;
	}

p_err:
	minfo(" =STM CFG===============\n", device);
	minfo(" %s", device, is_dmsg_print());
	minfo(" DEVICE GRAPH :", device);
	for (slot = 0; slot < GROUP_SLOT_MAX; ++slot)
		printk(KERN_CONT " %d(%s),",
			path->group[slot], group_id_name[path->group[slot]]);
	printk(KERN_CONT "X \n");
	minfo(" =======================\n", device);
	return ret;
}

int is_groupmgr_start(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device)
{
	int ret = 0;
	u32 instance;
	u32 width, height;
	IS_DECLARE_PMAP(pmap);
	struct is_group *group, *prev, *pnext;
	struct is_subdev *leader, *subdev;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!device);

	width = 0;
	height = 0;
	IS_INIT_PMAP(pmap);
	instance = device->instance;
	group = groupmgr->leader[instance];
	if (!group) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}
	pnext = group->pnext;

	minfo(" =GRP CFG===============\n", device);
	while(group) {
		leader = &group->leader;
		prev = group->prev;

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
			!test_bit(IS_GROUP_START, &group->state)) {
			merr("GP%d is NOT started", device, group->id);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
#ifdef CONFIG_USE_SENSOR_GROUP
			if (group->slot == GROUP_SLOT_SENSOR) {
				width = is_sensor_g_width(device->sensor);
				height = is_sensor_g_height(device->sensor);
				leader->input.width = width;
				leader->input.height = height;
			} else if (group->slot == GROUP_SLOT_3AA)
#else
			if (group->slot == GROUP_SLOT_3AA)
#endif
			{
				if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
					width = leader->leader->input.width;
					height = leader->leader->input.height;
				} else {
					width = is_sensor_g_bns_width(device->sensor);
					height = is_sensor_g_bns_height(device->sensor);
				}
				leader->input.width = width;
				leader->input.height = height;
			} else {
				if (prev && prev->junction) {
					if (!IS_ENABLED(CHAIN_STRIPE_PROCESSING)) {
						/* FIXME, Max size constrains */
						if (width > leader->constraints_width) {
							mgwarn(" width(%d) > constraints_width(%d),"
								" set constraints width",
								device, group, width,
								leader->constraints_width);
							width = leader->constraints_width;
						}

						if (height > leader->constraints_height) {
							mgwarn(" height(%d) > constraints_height(%d),"
								" set constraints height",
								device, group, height,
								leader->constraints_height);
							height = leader->constraints_height;
						}
					}

					leader->input.width = width;
					leader->input.height = height;
					prev->junction->output.width = width;
					prev->junction->output.height = height;
				} else {
					mgerr("previous group is NULL", group, group);
					BUG();
				}
			}
		} else {
			if (group->slot == GROUP_SLOT_3AA) {
				width = leader->input.width;
				height = leader->input.height;
			} else {
				width = leader->input.width;
				height = leader->input.height;
				leader->input.canv.x = 0;
				leader->input.canv.y = 0;
				leader->input.canv.w = leader->input.width;
				leader->input.canv.h = leader->input.height;
			}
		}

		mginfo(" SRC%02d:%04dx%04d\n", device, group, leader->vid,
			leader->input.width, leader->input.height);
		list_for_each_entry(subdev, &group->subdev_list, list) {
			if (subdev->vctx && test_bit(IS_SUBDEV_START, &subdev->state)) {
				mginfo(" CAP%2d:%04dx%04d\n", device, group, subdev->vid,
					subdev->output.width, subdev->output.height);

				if (!group->junction && (subdev != leader))
					group->junction = subdev;
			}
		}

		if (prev && !test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (!prev->junction) {
				mgerr("prev group is existed but junction is NULL", device, group);
				ret = -EINVAL;
				goto p_err;
			}
			/* skip size checking when using LVN */
			if (!IS_ENABLED(LOGICAL_VIDEO_NODE) &&
				((prev->junction->output.width != group->leader.input.width) ||
				(prev->junction->output.height != group->leader.input.height))) {
				mwarn("%s(%d x %d) != %s(%d x %d)", device,
					prev->junction->name,
					prev->junction->output.width,
					prev->junction->output.height,
					group->leader.name,
					group->leader.input.width,
					group->leader.input.height);
			}
		}

		group = group->next;
		if (!group && pnext) {
			group = pnext;
			pnext = NULL;
			width = group->leader.leader->input.width;
			height = group->leader.leader->input.height;
			minfo(" =GRP CFG2==============\n", device);
		}
	}
	minfo(" =======================\n", device);

#ifdef ENABLE_SYNC_REPROCESSING
	timer_setup(&groupmgr->trigger_timer, (void (*)(struct timer_list *))is_group_trigger_timer, 0);
	/* find main_dev for sync processing */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) && !main_dev) {
		main_dev = device;
		minfo("SYNC : set main device\n", device);
	}
#endif
p_err:
	return ret;
}

int is_groupmgr_stop(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device)
{
	int ret = 0;
	u32 instance;
	struct is_group *group;
#ifdef ENABLE_SYNC_REPROCESSING
	int i;
	struct is_core* core;
	u32 main_instance;
#endif
	FIMC_BUG(!groupmgr);
	FIMC_BUG(!device);

	instance = device->instance;
	group = groupmgr->leader[instance];
	if (!group) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

#ifdef ENABLE_SYNC_REPROCESSING
	core = (struct is_core *)device->interface->core;
	/* reset main device for sync processing */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) && main_dev == device) {
		main_instance = main_dev->instance;
		main_dev = NULL;
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			if (!test_bit(IS_ISCHAIN_REPROCESSING, &core->ischain[i].state)
					&& test_bit(IS_ISCHAIN_START, &core->ischain[i].state)
					&& (i != main_instance)) {
				main_dev = &core->ischain[i];
				minfo("SYNC : reset main device(%d)\n", device, main_dev->instance);
				break;
			}
		}
	}

	del_timer(&groupmgr->trigger_timer);
#endif
	if (test_bit(IS_GROUP_START, &group->state)) {
		merr("stream leader is NOT stopped", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int is_group_probe(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_device_sensor *sensor,
	struct is_device_ischain *device,
	is_shot_callback shot_callback,
	u32 slot,
	u32 id,
	char *name,
	const struct is_subdev_ops *sops)
{
	int ret = 0;
	struct is_subdev *leader;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);

	if (slot > GROUP_SLOT_MAX)
		return 0;

	leader = &group->leader;
	group->id = GROUP_ID_MAX;
	group->slot = slot;
	group->shot_callback = shot_callback;
	group->device = device;
	group->sensor = sensor;
	if (group->device)
		group->instance = device->instance;
	else
		group->instance = IS_STREAM_COUNT;

	mutex_init(&group->mlock_votf);
	spin_lock_init(&group->slock_s_ctrl);

	frame_manager_probe(&group->framemgr, group->id, name);

	ret = is_hw_group_cfg(group);
	if (ret) {
		merr("is_hw_group_cfg is fail(%d)", group, ret);
		goto p_err;
	}

	clear_bit(IS_GROUP_OPEN, &group->state);
	clear_bit(IS_GROUP_INIT, &group->state);
	clear_bit(IS_GROUP_START, &group->state);
	clear_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(IS_GROUP_OTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_STANDBY, &group->state);
	clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);

	if (group->device) {
		group->device_type = IS_DEVICE_ISCHAIN;
		is_subdev_probe(leader, device->instance, id, name, sops);
	} else if (group->sensor) {
		group->device_type = IS_DEVICE_SENSOR;
		is_subdev_probe(leader, sensor->device_id, id, name, sops);
	} else {
		err("device and sensor are NULL(%d)", ret);
	}

p_err:
	return ret;
}

int is_group_open(struct is_groupmgr *groupmgr,
	struct is_group *group, u32 id,
	struct is_video_ctx *vctx)
{
	int ret = 0;
	int ret_err = 0;
	u32 stream, slot;
	struct is_group_task *gtask;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!vctx);
	FIMC_BUG(id >= GROUP_ID_MAX);

	if (test_bit(IS_GROUP_OPEN, &group->state)) {
		if (group->id == id) {
			mgerr("already open", group, group);
			ret = -EMFILE;
		} else {
			mgwarn("need to map other device's group", group, group);
			ret = -EAGAIN;
		}
		goto err_state;
	}

	group->logical_id = id;
	group->id = id;
	stream = group->instance;
	slot = group->slot;
	gtask = &groupmgr->gtask[id];

	/* 1. Init Group */
	clear_bit(IS_GROUP_INIT, &group->state);
	clear_bit(IS_GROUP_START, &group->state);
	clear_bit(IS_GROUP_SHOT, &group->state);
	clear_bit(IS_GROUP_REQUEST_FSTOP, &group->state);
	clear_bit(IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(IS_GROUP_OTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_STANDBY, &group->state);
	clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);

	group->prev = NULL;
	group->next = NULL;
	group->gprev = NULL;
	group->gnext = NULL;
	group->parent = NULL;
	group->child = NULL;
	group->head = NULL;
	group->tail = NULL;
	group->junction = NULL;
	group->pnext = NULL;
	group->ptail = NULL;
	group->source_vid = 0;
	group->fcount = 0;
	group->pcount = 0;
	group->aeflashMode = 0; /* Flash Mode Control */
	group->remainIntentCount = 0;
	atomic_set(&group->scount, 0);
	atomic_set(&group->rcount, 0);
	atomic_set(&group->backup_fcount, 0);
	atomic_set(&group->sensor_fcount, 1);
	sema_init(&group->smp_trigger, 0);

	INIT_LIST_HEAD(&group->gframe_head);
	group->gframe_cnt = 0;
#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	monitor_init(&group->time);
#endif
#endif

	/* 2. start kthread */
	ret = is_group_task_start(groupmgr, gtask, group->slot);
	if (ret) {
		mgerr("is_group_task_start is fail(%d)", group, group, ret);
		goto err_group_task_start;
	}

	/* 3. Subdev Init */
	ret = is_subdev_open(&group->leader, vctx, NULL, stream);
	if (ret) {
		mgerr("is_subdev_open is fail(%d)", group, group, ret);
		goto err_subdev_open;
	}

	set_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

	/* 4. group hw Init */
	ret = is_hw_group_open(group);
	if (ret) {
		mgerr("is_hw_group_open is fail(%d)", group, group, ret);
		goto err_hw_group_open;
	}

	/* 5. Open Group Frame Manager */
	group->framemgr.id = id;
	ret = frame_manager_open(&group->framemgr, IS_MAX_PLANES);
	if (ret) {
		mgerr("frame_manager_open is fail(%d)", group, group, ret);
		goto err_hw_group_open;
	}

	/* 6. Update Group Manager */
	groupmgr->group[stream][slot] = group;
	set_bit(IS_GROUP_OPEN, &group->state);

	mdbgd_group("%s(%d) E\n", group, __func__, ret);
	return 0;

err_hw_group_open:
	ret_err = is_subdev_close(&group->leader);
	if (ret_err)
		mgerr("is_subdev_close is fail(%d)", group, group, ret_err);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

err_subdev_open:
	ret_err = is_group_task_stop(groupmgr, gtask, slot);
	if (ret_err)
		mgerr("is_group_task_stop is fail(%d)", group, group, ret_err);
err_group_task_start:
err_state:
	return ret;
}

int is_group_close(struct is_groupmgr *groupmgr,
	struct is_group *group)
{
	int ret = 0;
	bool all_slot_empty = true;
	u32 stream, slot, i;
	struct is_group_task *gtask;
	struct is_group_framemgr *gframemgr;
	int state_vlink;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	slot = group->slot;
	stream = group->instance;
	gtask = &groupmgr->gtask[group->id];

	if (!test_bit(IS_GROUP_OPEN, &group->state)) {
		mgerr("already close", group, group);
		return -EMFILE;
	}

	ret = is_group_task_stop(groupmgr, gtask, slot);
	if (ret)
		mgerr("is_group_task_stop is fail(%d)", group, group, ret);

	mutex_lock(&group->mlock_votf);
	state_vlink = test_and_clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);
	mutex_unlock(&group->mlock_votf);

	if (state_vlink) {
		mginfo("destroy votf_link forcely at group_close", group, group);

		if (group->head && group->head->device_type != IS_DEVICE_SENSOR) {
			ret = is_votf_destroy_link(group);
			if (ret)
				mgerr("is_votf_destroy_link is fail(%d)", group, group, ret);
		}
	}

	ret = is_subdev_close(&group->leader);
	if (ret)
		mgerr("is_subdev_close is fail(%d)", group, group, ret);

	clear_bit(IS_SUBDEV_EXTERNAL_USE, &group->leader.state);

	frame_manager_close(&group->framemgr);

	clear_bit(IS_GROUP_USE_MULTI_CH, &group->state);
	clear_bit(IS_GROUP_INIT, &group->state);
	clear_bit(IS_GROUP_OPEN, &group->state);
	groupmgr->group[stream][slot] = NULL;

	for (i = 0; i < GROUP_SLOT_MAX; i++) {
		if (groupmgr->group[stream][i]) {
			all_slot_empty = false;
			break;
		}
	}

	if (all_slot_empty) {
		gframemgr = &groupmgr->gframemgr[stream];

		if (gframemgr->gframe_cnt != IS_MAX_GFRAME) {
			mwarn("gframemgr free count is invalid(%d)", group, gframemgr->gframe_cnt);
			INIT_LIST_HEAD(&gframemgr->gframe_head);
			gframemgr->gframe_cnt = 0;
			for (i = 0; i < IS_MAX_GFRAME; ++i) {
				gframemgr->gframes[i].fcount = 0;
				is_gframe_s_free(gframemgr, &gframemgr->gframes[i]);
			}
		}
	}

	mdbgd_group("%s(ref %d, %d)", group, __func__, atomic_read(&gtask->refcount), ret);

	/* reset after using it */
	group->id = GROUP_ID_MAX;
	group->logical_id = GROUP_ID_MAX;

	return ret;
}

static void is_group_clear_bits(struct is_group *group)
{
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	clear_bit(IS_GROUP_OTF_OUTPUT, &group->state);
	clear_bit(IS_GROUP_VOTF_INPUT, &group->state);
	clear_bit(IS_GROUP_VOTF_OUTPUT, &group->state);
}

static int is_group_isp0_lvn_source(struct is_group *group, u32 vid)
{
	switch (vid) {
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_30G_NUM:
		group->source_vid = IS_VIDEO_30S_NUM;
		break;
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_31G_NUM:
		group->source_vid = IS_VIDEO_31S_NUM;
		break;
	case IS_VIDEO_32C_NUM:
	case IS_VIDEO_32P_NUM:
	case IS_VIDEO_32G_NUM:
		group->source_vid = IS_VIDEO_32S_NUM;
		break;
	case IS_VIDEO_33C_NUM:
	case IS_VIDEO_33P_NUM:
	case IS_VIDEO_33G_NUM:
		group->source_vid = IS_VIDEO_33S_NUM;
		break;
	case IS_VIDEO_LME0S_NUM:
	case IS_VIDEO_LME0C_NUM:
		group->source_vid = IS_VIDEO_LME0_NUM;
		break;
	case IS_VIDEO_LME1S_NUM:
	case IS_VIDEO_LME1C_NUM:
		group->source_vid = IS_VIDEO_LME1_NUM;
		break;
	case IS_VIDEO_ORB0C_NUM:
		group->source_vid = IS_VIDEO_ORB0_NUM;
		break;
	case IS_VIDEO_ORB1C_NUM:
		group->source_vid = IS_VIDEO_ORB1_NUM;
		break;
	default:
		mgerr("actual source node is invalid!(%d)", group, group, vid);
		return -EINVAL;
	}

	return 0;
}

static int is_group_lme_lvn_source(struct is_group *group, u32 vid)
{
	switch (vid) {
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_30G_NUM:
	case IS_VIDEO_30O_NUM:
	case IS_VIDEO_30L_NUM:
		group->source_vid = IS_VIDEO_30S_NUM;
		break;
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_31G_NUM:
	case IS_VIDEO_31O_NUM:
	case IS_VIDEO_31L_NUM:
		group->source_vid = IS_VIDEO_31S_NUM;
		break;
	case IS_VIDEO_32C_NUM:
	case IS_VIDEO_32P_NUM:
	case IS_VIDEO_32G_NUM:
	case IS_VIDEO_32O_NUM:
	case IS_VIDEO_32L_NUM:
		group->source_vid = IS_VIDEO_32S_NUM;
		break;
	case IS_VIDEO_33C_NUM:
	case IS_VIDEO_33P_NUM:
	case IS_VIDEO_33G_NUM:
	case IS_VIDEO_33O_NUM:
	case IS_VIDEO_33L_NUM:
		group->source_vid = IS_VIDEO_33S_NUM;
		break;
	default:
		mgerr("actual source node is invalid!(%d)", group, group, vid);
		return -EINVAL;
	}

	return 0;
}

static int is_group_orbmch_lvn_source(struct is_group *group, u32 vid)
{
	switch (vid) {
	case IS_VIDEO_30C_NUM:
	case IS_VIDEO_30P_NUM:
	case IS_VIDEO_30G_NUM:
	case IS_VIDEO_30O_NUM:
	case IS_VIDEO_30L_NUM:
		group->source_vid = IS_VIDEO_30S_NUM;
		break;
	case IS_VIDEO_31C_NUM:
	case IS_VIDEO_31P_NUM:
	case IS_VIDEO_31G_NUM:
	case IS_VIDEO_31O_NUM:
	case IS_VIDEO_31L_NUM:
		group->source_vid = IS_VIDEO_31S_NUM;
		break;
	case IS_VIDEO_32C_NUM:
	case IS_VIDEO_32P_NUM:
	case IS_VIDEO_32G_NUM:
	case IS_VIDEO_32O_NUM:
	case IS_VIDEO_32L_NUM:
		group->source_vid = IS_VIDEO_32S_NUM;
		break;
	case IS_VIDEO_33C_NUM:
	case IS_VIDEO_33P_NUM:
	case IS_VIDEO_33G_NUM:
	case IS_VIDEO_33O_NUM:
	case IS_VIDEO_33L_NUM:
		group->source_vid = IS_VIDEO_33S_NUM;
		break;
	default:
		mgerr("actual source node is invalid!(%d)", group, group, vid);
		return -EINVAL;
	}

	return 0;
}

int is_group_init(struct is_groupmgr *groupmgr,
	struct is_group *group,
	u32 input_type,
	u32 video_id,
	u32 stream_leader)
{
	int ret = 0;
	u32 slot;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);
	FIMC_BUG(video_id >= IS_VIDEO_MAX_NUM);

	if (!test_bit(IS_GROUP_OPEN, &group->state)) {
		mgerr("group is NOT open", group, group);
		ret = -EINVAL;
		goto err_open_already;
	}

	slot = group->slot;
	group->source_vid = video_id;
	group->thrott_tick = KEEP_FRAME_TICK_THROTT;

	is_group_clear_bits(group);

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		if (stream_leader > 1) {
			mginfo("set logical node group: %d", group, group, stream_leader);
			groupmgr->group_type = 1;
		} else if (stream_leader > 0) {
			groupmgr->group_type = 0;
		}
	}

	if (stream_leader)
		groupmgr->leader[group->instance] = group;

	if (input_type >= GROUP_INPUT_MAX) {
		mgerr("input type is invalid(%d)", group, group, input_type);
		ret = -EINVAL;
		goto err_input_type;
	}

	smp_shot_init(group, 1);
	group->asyn_shots = 0;
	group->skip_shots = 0;
	group->init_shots = 0;
	group->sync_shots = 1;

	if (input_type == GROUP_INPUT_OTF || input_type == GROUP_INPUT_VOTF) {
		set_bit(IS_GROUP_OTF_INPUT, &group->state);
		smp_shot_init(group, MIN_OF_SHOT_RSC);
		group->asyn_shots = MIN_OF_ASYNC_SHOTS;
		group->skip_shots = MIN_OF_ASYNC_SHOTS;
		group->init_shots = MIN_OF_ASYNC_SHOTS;
		group->sync_shots = MIN_OF_SYNC_SHOTS;
	}

	if (input_type == GROUP_INPUT_VOTF)
		set_bit(IS_GROUP_VOTF_INPUT, &group->state);

	if (IS_ENABLED(LOGICAL_VIDEO_NODE) && groupmgr->group_type) {
		if (group->id == GROUP_ID_ISP0) {
			ret = is_group_isp0_lvn_source(group, video_id);
			if (ret)
				goto err_isp0_lvn;
		} else if (group->id == GROUP_ID_LME0
				|| group->id == GROUP_ID_LME1) {
			ret = is_group_lme_lvn_source(group, video_id);
			if (ret)
				goto err_lme_lvn;
		} else if (group->id == GROUP_ID_ORB0
				|| group->id == GROUP_ID_ORB1) {
			ret = is_group_orbmch_lvn_source(group, video_id);
			if (ret)
				goto err_orbmch_lvn;
		} else if (group->id == GROUP_ID_MCS0) {
			if (IS_ENABLED(SOC_YPP) || IS_ENABLED(SOC_YUVP))
				group->source_vid = IS_VIDEO_YPP_NUM;
			else
				group->source_vid = IS_VIDEO_I0S_NUM;
		} else if (group->id == GROUP_ID_YPP) {
#if IS_ENABLED(SOC_YPP)
			group->source_vid = IS_VIDEO_I0S_NUM;
#elif IS_ENABLED(SOC_YUVP)
			group->source_vid = IS_VIDEO_YUVP;
#endif
#if IS_ENABLED(SOC_BYRP)
		} else if (group->id == GROUP_ID_BYRP) {
			group->source_vid = IS_VIDEO_BYRP;
#endif
#if IS_ENABLED(SOC_RGBP)
		} else if (group->id == GROUP_ID_RGBP) {
			group->source_vid = IS_VIDEO_RGBP;
#endif
#if IS_ENABLED(SOC_MCFP)
		} else if (group->id == GROUP_ID_MCFP) {
			group->source_vid = IS_VIDEO_MCFP;
#endif
		} else {
			goto skip_junction_vid;
		}

		group->junction_vid = video_id;
		mginfo("junction_vid(%d, %s), source_vid(%d, %s)\n", group, group,
				group->junction_vid, vn_name[group->junction_vid],
				group->source_vid, vn_name[group->source_vid]);
	}

skip_junction_vid:
	set_bit(IS_GROUP_INIT, &group->state);

err_lme_lvn:
err_orbmch_lvn:
err_isp0_lvn:
err_input_type:
err_open_already:
	mdbgd_group("%s(input : %d):%d\n", group, __func__, input_type, ret);

	return ret;
}

static void set_group_shots(struct is_group *group,
	struct is_sensor_cfg *sensor_cfg)
{
	u32 ex_mode;
	u32 max_fps;
	u32 height;

	FIMC_BUG_VOID(!sensor_cfg);

	ex_mode = sensor_cfg->ex_mode;
	max_fps = sensor_cfg->max_fps;
	height = sensor_cfg->height;

#ifdef REDUCE_COMMAND_DELAY
	group->asyn_shots = MIN_OF_ASYNC_SHOTS + 1;
	group->sync_shots = 0;
#else
	group->asyn_shots = MIN_OF_ASYNC_SHOTS + 0;
	group->sync_shots = MIN_OF_SYNC_SHOTS;
#endif
	group->init_shots = group->asyn_shots;
	group->skip_shots = group->asyn_shots;

	return;
}

int is_group_start(struct is_groupmgr *groupmgr,
	struct is_group *group)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_sensor_cfg *sensor_cfg;
	struct is_resourcemgr *resourcemgr;
	struct is_framemgr *framemgr = NULL;
	struct is_group_task *gtask;
	struct is_group *pos;
	struct is_core *core = is_get_is_core();
	u32 sensor_fcount;
	u32 framerate;
	u32 width, height;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!group->device);
	FIMC_BUG(!group->device->sensor);
	FIMC_BUG(!group->device->resourcemgr);
	FIMC_BUG(!group->leader.vctx);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	if (!test_bit(IS_GROUP_INIT, &group->state)) {
		merr("group is NOT initialized", group);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(IS_GROUP_START, &group->state)) {
		warn("already group start");
		ret = -EINVAL;
		goto p_err;
	}

	device = group->device;
	gtask = &groupmgr->gtask[group->id];
	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, 0);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		goto p_err;
	}

	atomic_set(&group->scount, 0);
	sema_init(&group->smp_trigger, 0);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		group->asyn_shots = 1;
		group->skip_shots = 0;
		group->init_shots = 0;
		group->sync_shots = 0;
		smp_shot_init(group, group->asyn_shots + group->sync_shots);
	} else {
		sensor = device->sensor;
		sensor_cfg = sensor->cfg;
		framerate = is_sensor_g_framerate(sensor);

		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			resourcemgr = device->resourcemgr;
			set_group_shots(group, sensor_cfg);

			/* frame count */
			sensor_fcount = is_sensor_g_fcount(sensor) + 1;
			atomic_set(&group->sensor_fcount, sensor_fcount);
			atomic_set(&group->backup_fcount, sensor_fcount - 1);
			group->fcount = sensor_fcount - 1;

			memset(&group->intent_ctl, 0, sizeof(struct camera2_aa_ctl));

			/* shot resource */
			sema_init(&gtask->smp_resource, group->asyn_shots + group->sync_shots);
			smp_shot_init(group, group->asyn_shots + group->sync_shots);
		} else {
			group->asyn_shots = MIN_OF_ASYNC_SHOTS;
			group->skip_shots = 0;
			group->init_shots = 0;
			group->sync_shots = 0;
			smp_shot_init(group, group->asyn_shots + group->sync_shots);
		}
	}

	width = height = 0;

	for_each_group_child(pos, group) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &pos->state)) {
			width = pos->head->leader.input.width;
			height = pos->head->leader.input.height;

			if (test_and_set_bit(IS_GROUP_VOTF_CONN_LINK, &pos->state)) {
				mgwarn("already connected votf link", group, group);
				continue;
			}

			if (!width || !height) {
				if (core &&
				    (core->vender.isp_max_width || core->vender.isp_max_height)) {
					width = core->vender.isp_max_width;
					height = core->vender.isp_max_height;
				}
			}

			ret = is_votf_create_link(pos, width, height);
			if (ret) {
				mgerr("is_votf_create_link is fail(%d)", pos, pos, ret);
				goto p_err;
			}
		}
	}

	set_bit(IS_SUBDEV_START, &group->leader.state);
	set_bit(IS_GROUP_START, &group->state);

p_err:
	mginfo("bufs: %02d, init : %d, asyn: %d, skip: %d, sync : %d\n", group, group,
		framemgr ? framemgr->num_frames : 0, group->init_shots,
		group->asyn_shots, group->skip_shots, group->sync_shots);
	return ret;
}

void wait_subdev_flush_work(struct is_device_ischain *device,
	struct is_group *group, u32 entry)
{
	struct is_interface *itf;
	u32 wq_id = WORK_MAX_MAP;
	bool ret;

	FIMC_BUG_VOID(!group);
	FIMC_BUG_VOID(!device);

	itf = device->interface;
	FIMC_BUG_VOID(!itf);

	switch (entry) {
	case ENTRY_3AC:
	case ENTRY_3AP:
	case ENTRY_3AF:
	case ENTRY_3AG:
	case ENTRY_3AO:
	case ENTRY_3AL:
	case ENTRY_ORBXC:
	case ENTRY_IXC:
	case ENTRY_IXP:
	case ENTRY_IXT:
	case ENTRY_IXG:
	case ENTRY_IXV:
	case ENTRY_IXW:
	case ENTRY_MEXC:
	case ENTRY_M0P:
	case ENTRY_M1P:
	case ENTRY_M2P:
	case ENTRY_M3P:
	case ENTRY_M4P:
	case ENTRY_M5P:
	case ENTRY_LMES:
	case ENTRY_LMEC:
		break;
	case ENTRY_SENSOR:	/* Falls Through */
	case ENTRY_PAF: 	/* Falls Through */
	case ENTRY_3AA:		/* Falls Through */
	case ENTRY_LME:		/* Falls Through */
	case ENTRY_ORB:		/* Falls Through */
	case ENTRY_ISP:		/* Falls Through */
	case ENTRY_MCS:		/* Falls Through */
	case ENTRY_VRA:
	case ENTRY_YPP:
	case ENTRY_BYRP:
	case ENTRY_RGBP:
	case ENTRY_MCFP:
		mginfo("flush SHOT_DONE wq, subdev[%d]", device, group, entry);
		break;
	default:
		return;
	}

	wq_id = is_subdev_wq_id[entry];

	ret = flush_work(&itf->work_wq[wq_id]);
	if (ret)
		mginfo(" flush_work executed! wq_id(%d)\n", device, group, wq_id);
}

int is_group_stop(struct is_groupmgr *groupmgr,
	struct is_group *group)
{
	int ret = 0;
	int errcnt = 0;
	int retry;
	u32 rcount;
	unsigned long flags;
	struct is_framemgr *framemgr;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	struct is_group *head;
	struct is_group *pos, *tmp;
	struct is_subdev *subdev;
	struct is_group_task *gtask;
	int state_vlink;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!group->device);
	FIMC_BUG(!group->leader.vctx);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	head = group->head;
	if (!head) {
		mgwarn("head group is NULL", group, group);
		return -EPERM;
	}

	if (!test_bit(IS_GROUP_START, &group->state) &&
	    !test_bit(IS_GROUP_START, &head->state)) {
		mgwarn("already group stop", group, group);
		return -EPERM;
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, 0);
	if (!framemgr) {
		mgerr("framemgr is NULL", group, group);
		goto p_err;
	}

	/* force stop set if only HEAD group OTF input */
	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		if (test_bit(IS_GROUP_REQUEST_FSTOP, &group->state))
			set_bit(IS_GROUP_FORCE_STOP, &group->state);
	}
	clear_bit(IS_GROUP_REQUEST_FSTOP, &group->state);

	gtask = &groupmgr->gtask[head->id];
	device = group->device;

	retry = 150;
	while (--retry && framemgr->queued_count[FS_REQUEST]) {
		if (test_bit(IS_GROUP_OTF_INPUT, &head->state) &&
			!list_empty(&head->smp_trigger.wait_list)) {

			sensor = device->sensor;
			if (!sensor) {
				mwarn(" sensor is NULL, forcely trigger(pc %d)", device, head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_OPEN, &head->state)) {
				mwarn(" sensor is closed, forcely trigger(pc %d)", device, head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
				mwarn(" front is stopped, forcely trigger(pc %d)", device, head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (!test_bit(IS_SENSOR_BACK_START, &sensor->state)) {
				mwarn(" back is stopped, forcely trigger(pc %d)", device, head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else if (retry < 100) {
				merr(" sensor is working but no trigger(pc %d)", device, head->pcount);
				set_bit(IS_GROUP_FORCE_STOP, &head->state);
				up(&head->smp_trigger);
			} else {
				mwarn(" wating for sensor trigger(pc %d)", device, head->pcount);
			}
#ifdef ENABLE_SYNC_REPROCESSING
		} else if (!test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
			if (!list_empty(&gtask->sync_list)) {
				struct is_frame *rframe;
				rframe = list_first_entry(&gtask->sync_list, struct is_frame, sync_list);
				list_del(&rframe->sync_list);
				mgrinfo("flush SYNC capture(%d)\n", head, head, rframe, rframe->index);
				is_group_kthread_queue_work(gtask, head, rframe);
			}

			if (!list_empty(&gtask->preview_list[head->instance])) {
				struct is_frame *rframe;
				atomic_dec(&gtask->preview_cnt[head->instance]);
				rframe = list_first_entry(&gtask->preview_list[head->instance], struct is_frame, preview_list);
				list_del(&rframe->preview_list);
				mgrinfo("flush SYNC preview(%d)\n", head, head, rframe, rframe->index);
				is_group_kthread_queue_work(gtask, head, rframe);
			}
#endif
		}

		mgwarn(" %d reqs waiting1...(pc %d) smp_resource(%d)", device, head,
				framemgr->queued_count[FS_REQUEST], head->pcount,
				list_empty(&gtask->smp_resource.wait_list));
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until request empty) is fail(pc %d)", device, head, head->pcount);
		errcnt++;
	}

	/* ensure that request cancel work is complete fully */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_21, flags);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_21, flags);

	retry = 150;
	while (--retry && test_bit(IS_GROUP_SHOT, &head->state)) {
		mgwarn(" thread stop waiting...(pc %d)", device, head, head->pcount);
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until thread stop) is fail(pc %d)", device, head, head->pcount);
		errcnt++;
	}

	/* Stop sub ischain instance for MULTI_CH scenario */
	for_each_group_child_double_path(pos, tmp, head) {
		if (test_bit(IS_GROUP_FORCE_STOP, &group->state)) {
			ret = is_itf_force_stop(device, GROUP_ID(pos->id));
			if (ret) {
				mgerr(" is_itf_force_stop is fail(%d)", device, pos, ret);
				errcnt++;
			}
		} else {
			ret = is_itf_process_stop(device, GROUP_ID(pos->id));
			if (ret) {
				mgerr(" is_itf_process_stop is fail(%d)", device, pos, ret);
				errcnt++;
			}
		}

		if (test_bit(IS_GROUP_VOTF_INPUT, &pos->state)) {
			mutex_lock(&pos->mlock_votf);
			state_vlink = test_and_clear_bit(IS_GROUP_VOTF_CONN_LINK, &pos->state);
			mutex_unlock(&pos->mlock_votf);

			if (!state_vlink) {
				mgwarn("already destroy votf link", pos, pos);
				continue;
			}

			ret = is_votf_destroy_link(pos);
			if (ret)
				mgerr("is_votf_destroy_link is fail(%d)", pos, pos, ret);
		}
	}

	retry = 150;
	while (--retry && framemgr->queued_count[FS_PROCESS]) {
		mgwarn(" %d pros waiting...(pc %d)", device, head, framemgr->queued_count[FS_PROCESS], head->pcount);
		msleep(20);
	}

	if (!retry) {
		mgerr(" waiting(until process empty) is fail(pc %d)", device, head, head->pcount);
		errcnt++;
	}

	/* After stopped, wait until remained req_list frame is flushed by group shot cancel */
	retry = 150;
	while (--retry && framemgr->queued_count[FS_REQUEST]) {
		mgwarn(" %d req waiting2...(pc %d)", device, head, framemgr->queued_count[FS_REQUEST], head->pcount);
		usleep_range(1000, 1001);
	}

	if (!retry) {
		mgerr(" waiting(until process empty) is fail(pc %d)", device, head, head->pcount);
		errcnt++;
	}

	rcount = atomic_read(&head->rcount);
	if (rcount) {
		mgerr(" request is NOT empty(%d) (pc %d)", device, head, rcount, head->pcount);
		errcnt++;

		/*
		 * Extinctionize pending works in worker to avoid the work_list corruption.
		 * When user calls 'vb2_stop_streaming()' that calls 'group_stop()',
		 * 'v4l2_reqbufs()' can be called for another stream
		 * and it means every work in frame is going to be initialized.
		 */
		spin_lock_irqsave(&gtask->gtask_slock, flags);
		INIT_LIST_HEAD(&gtask->worker.work_list);
		INIT_LIST_HEAD(&gtask->worker.delayed_work_list);
		spin_unlock_irqrestore(&gtask->gtask_slock, flags);
	}
	/* the count of request should be clear for next streaming */
	atomic_set(&head->rcount, 0);

	for_each_group_child_double_path(pos, tmp, head) {
		list_for_each_entry(subdev, &pos->subdev_list, list) {
			if (IS_ERR_OR_NULL(subdev))
				continue;

			if (subdev->vctx && test_bit(IS_SUBDEV_START, &subdev->state)) {
				wait_subdev_flush_work(device, pos, subdev->id);

				framemgr = GET_SUBDEV_FRAMEMGR(subdev);
				if (!framemgr) {
					mserr("framemgr is NULL", subdev, subdev);
					goto p_err;
				}

				retry = 150;
				while (--retry && framemgr->queued_count[FS_PROCESS]) {
					mgwarn(" subdev[%d] stop waiting...", device, head, subdev->vid);
					msleep(20);
				}

				if (!retry) {
					mgerr(" waiting(subdev stop) is fail", device, head);
					errcnt++;
				}

				clear_bit(IS_SUBDEV_RUN, &subdev->state);
			}
			/*
			 *
			 * For subdev only to be control by driver (no video node)
			 * In "process-stop" state of a group which have these subdevs,
			 * subdev's state can be invalid like tdnr, odc or drc etc.
			 * The wrong state problem can be happened in just stream-on/off ->
			 * stream-on case.
			 * ex.  previous stream on : state = IS_SUBDEV_RUN && bypass = 0
			 *      next stream on     : state = IS_SUBDEV_RUN && bypass = 0
			 * In this case, the subdev's function can't be worked.
			 * Because driver skips the function if it's state is IS_SUBDEV_RUN.
			 */
			clear_bit(IS_SUBDEV_RUN, &subdev->state);
		}
	}

	is_gframe_flush(groupmgr, head);

	if (test_bit(IS_GROUP_OTF_INPUT, &head->state))
		mginfo(" sensor fcount: %d, fcount: %d\n", device, head,
			atomic_read(&head->sensor_fcount), head->fcount);

	clear_bit(IS_GROUP_FORCE_STOP, &group->state);
	clear_bit(IS_SUBDEV_START, &group->leader.state);
	clear_bit(IS_GROUP_START, &group->state);
	clear_bit(IS_SUBDEV_RUN, &group->leader.state);
	clear_bit(IS_GROUP_VOTF_CONN_LINK, &group->state);

p_err:
	return -errcnt;
}

static void is_group_override_meta(struct is_group *group,
				struct is_framemgr *framemgr,
				struct is_frame *frame,
				struct is_resourcemgr *resourcemgr,
				struct is_device_ischain *device,
				struct is_device_sensor *sensor)
{
	if (IS_ENABLED(FIXED_FPS_DEBUG)) {
		frame->shot->ctl.aa.aeTargetFpsRange[0] = FIXED_MAX_FPS_VALUE;
		frame->shot->ctl.aa.aeTargetFpsRange[1] = FIXED_MAX_FPS_VALUE;
		frame->shot->ctl.sensor.frameDuration = 1000000000 / FIXED_MAX_FPS_VALUE;
	}

	if (resourcemgr->limited_fps) {
		frame->shot->ctl.aa.aeTargetFpsRange[0] = resourcemgr->limited_fps;
		frame->shot->ctl.aa.aeTargetFpsRange[1] = resourcemgr->limited_fps;
		frame->shot->ctl.sensor.frameDuration = 1000000000/resourcemgr->limited_fps;
	}

	/* preview HW running fps */
	if (IS_ENABLED(HW_RUNNING_FPS)) {
		u32 ex_mode = is_sensor_g_ex_mode(sensor);

		if (ex_mode == EX_DUALFPS_960 || ex_mode == EX_DUALFPS_480) {
			frame->shot->ctl.aa.ispHwTargetFpsRange[0] = 60;
			frame->shot->ctl.aa.ispHwTargetFpsRange[1] = 60;
		} else {
			if (frame->shot->ctl.aa.vendor_fpsHintResult == 0) {
				frame->shot->ctl.aa.ispHwTargetFpsRange[0] =
					frame->shot->ctl.aa.aeTargetFpsRange[0];
				frame->shot->ctl.aa.ispHwTargetFpsRange[1] =
					frame->shot->ctl.aa.aeTargetFpsRange[1];
			}  else {
				frame->shot->ctl.aa.ispHwTargetFpsRange[0] =
					frame->shot->ctl.aa.vendor_fpsHintResult;
				frame->shot->ctl.aa.ispHwTargetFpsRange[1] =
					frame->shot->ctl.aa.vendor_fpsHintResult;
			}
		}
	}

#if IS_ENABLED(CONFIG_CAMERA_PDP)
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		/* PAF */
		if (sensor->cfg->pd_mode == PD_NONE)
			frame->shot->uctl.isModeUd.paf_mode = CAMERA_PAF_OFF;
		else
			frame->shot->uctl.isModeUd.paf_mode = CAMERA_PAF_ON;
	}
#endif
}

static void is_group_override_sensor_req(struct is_group *group,
				struct is_framemgr *framemgr,
				struct is_frame *frame)
{
	int req_cnt = 0;
	struct is_frame *prev;

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		return;

	list_for_each_entry_reverse(prev, &framemgr->queued_list[FS_REQUEST], list) {
		if (++req_cnt > SENSOR_REQUEST_DELAY)
			break;

		if (frame->shot->ctl.aa.aeMode == AA_AEMODE_OFF
		    || frame->shot->ctl.aa.mode == AA_CONTROL_OFF) {
			prev->shot->ctl.sensor.exposureTime	= frame->shot->ctl.sensor.exposureTime;
			prev->shot->ctl.sensor.frameDuration	= frame->shot->ctl.sensor.frameDuration;
			prev->shot->ctl.sensor.sensitivity	= frame->shot->ctl.sensor.sensitivity;
			prev->shot->ctl.aa.vendor_isoValue	= frame->shot->ctl.aa.vendor_isoValue;
			prev->shot->ctl.aa.vendor_isoMode	= frame->shot->ctl.aa.vendor_isoMode;
			prev->shot->ctl.aa.aeMode		= frame->shot->ctl.aa.aeMode;
		}

		/*
		 * Flash capture has 2 Frame delays due to DDK constraints.
		 * N + 1: The DDK uploads the best shot and streams off.
		 * N + 2: HAL used the buffer of the next of best shot as a flash image.
		 */
		if (frame->shot->ctl.aa.vendor_aeflashMode == AA_FLASHMODE_CAPTURE)
			prev->shot->ctl.aa.vendor_aeflashMode	= frame->shot->ctl.aa.vendor_aeflashMode;

		prev->shot->ctl.aa.aeExpCompensation		= frame->shot->ctl.aa.aeExpCompensation;
		prev->shot->ctl.aa.aeLock			= frame->shot->ctl.aa.aeLock;
		prev->shot->ctl.lens.opticalStabilizationMode	= frame->shot->ctl.lens.opticalStabilizationMode;

		if (frame->shot->ctl.aa.sceneMode == AA_SCENE_MODE_PRO_MODE
		    && frame->shot->ctl.aa.captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT) {
			prev->shot->ctl.aa.sceneMode = frame->shot->ctl.aa.sceneMode;
			prev->shot->ctl.aa.captureIntent = frame->shot->ctl.aa.captureIntent;
			prev->shot->ctl.aa.vendor_captureExposureTime = frame->shot->ctl.aa.vendor_captureExposureTime;
			prev->shot->ctl.aa.vendor_captureCount = frame->shot->ctl.aa.vendor_captureCount;
		}
	}
}

static void is_group_update_stripe(struct is_group *group,
				struct is_frame *frame,
				struct is_resourcemgr *resourcemgr)
{
	struct is_group *next;
	int i, max_width, min_const_width;
	int sram_sum = 0;
	struct camera2_stream *stream = (struct camera2_stream *)frame->shot_ext;
	struct is_group *pos;
	int dev_region_num;
	int max_region_num;

	if (!IS_ENABLED(CHAIN_STRIPE_PROCESSING))
		return;

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (group->slot == GROUP_SLOT_PAF || group->slot == GROUP_SLOT_3AA)
			sram_sum = atomic_read(&resourcemgr->lic_sram.taa_sram_sum);
	}

	/* Trigger stripe processing for remosaic capture request. */
	if (IS_ENABLED(CHAIN_USE_STRIPE_REGION_NUM_META) && stream->stripe_region_num) {
		frame->stripe_info.region_num = stream->stripe_region_num;
	} else {
		if (IS_ENABLED(CHECK_STRIPE_EACH_GROUP)) {
			max_region_num = 0;
			dev_region_num = 0;

			for_each_group_child(pos, group) {
				CALL_SOPS(&pos->leader, get, group->device, frame,
						PSGT_REGION_NUM, &dev_region_num);

				if (max_region_num < dev_region_num)
					max_region_num = dev_region_num;
			}
			frame->stripe_info.region_num = max_region_num;
		} else {
			max_width = frame->shot_ext->node_group.leader.input.cropRegion[2];
#ifdef ENABLE_HF
			if (group->id == GROUP_ID_ISP0 || group->id == GROUP_ID_YPP) {
				for (i = 0; i < CAPTURE_NODE_MAX; i++) {
					if (frame->shot_ext->node_group.capture[i].vid != MCSC_HF_ID)
						continue;
					if (max_width < frame->shot_ext->node_group.capture[i].output.cropRegion[2])
						max_width = frame->shot_ext->node_group.capture[i].output.cropRegion[2];
				}
			}
#endif
			if ((max_width > (group->leader.constraints_width - sram_sum))
				&& (group->id != GROUP_ID_VRA0)) {
				/* Find max_width in group */
				max_width = frame->shot_ext->node_group.leader.input.cropRegion[2];
				for (i = 0; i < CAPTURE_NODE_MAX; i++) {
					if (frame->shot_ext->node_group.capture[i].vid == 0)
						continue;
					if (max_width < frame->shot_ext->node_group.capture[i].output.cropRegion[2])
						max_width = frame->shot_ext->node_group.capture[i].output.cropRegion[2];
				}

				/* Find min_constraints_width in stream */
				min_const_width = group->leader.constraints_width;
				next = group->next;
				while (next) {
					if ((min_const_width > next->leader.constraints_width)
						 && (next->id != GROUP_ID_VRA0))
						min_const_width = next->leader.constraints_width;

					next = next->next;
				}
				frame->stripe_info.region_num = DIV_ROUND_UP(max_width - STRIPE_MARGIN_WIDTH * 2,
					ALIGN_DOWN(min_const_width - STRIPE_MARGIN_WIDTH * 2, STRIPE_WIDTH_ALIGN));
			}
		}
	}
	mgrdbgs(3, "set stripe_region_num %d\n", group, group, frame,
			frame->stripe_info.region_num);
}

int pablo_group_buffer_init(struct is_groupmgr *grpmgr, struct is_group *ig, u32 index)
{
	unsigned long flags;
	struct is_device_ischain *idi = ig->device;
	struct is_framemgr *framemgr = GET_HEAD_GROUP_FRAMEMGR(ig, 0);
	struct is_frame *frame;

	if (!framemgr) {
		mgerr("framemgr is NULL", idi, ig);
		return -EINVAL;
	}

	if (index >= framemgr->num_frames) {
		mgerr("out of range (%d >= %d)", idi, ig, index, framemgr->num_frames);
		return -EINVAL;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	frame = &framemgr->frames[index];
	frame->groupmgr = grpmgr;
	frame->group = ig;
	memset(&frame->prfi, 0x0, sizeof(struct pablo_rta_frame_info));
	frame->prfi.magic = 0x5F4D5246;

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	return 0;
}

int pablo_group_update_prfi(struct is_group *group, struct is_frame *frame)
{
       struct is_group *pos;

       for_each_group_child(pos, group)
               CALL_SOPS(&pos->leader, get, group->device, frame, PSGT_RTA_FRAME_INFO, NULL);

       return 0;
}

int is_group_buffer_queue(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_queue *queue,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct is_resourcemgr *resourcemgr;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_device_sensor *sensor = NULL;
	struct is_sysfs_debug *sysfs_debug;
	int i, p;
	u32 cur_shot_idx = 0, pos_meta_p;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!group->device);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);
	FIMC_BUG(!queue);

	device = group->device;
	resourcemgr = device->resourcemgr;
	framemgr = &queue->framemgr;
	sensor = device->sensor;

	FIMC_BUG(index >= framemgr->num_frames);
	FIMC_BUG(!sensor);

	/* 1. check frame validation */
	frame = &framemgr->frames[index];

	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto p_err;
	}

	pos_meta_p = frame->planes;
	frame->num_buffers /= frame->num_shots;
	frame->planes /= frame->num_shots;

	if (group->device_type == IS_DEVICE_SENSOR)
		is_sensor_s_batch_mode(sensor, frame);

trigger_multi_grp_shot:
	if (cur_shot_idx) {
		struct is_frame *grp_frame = frame;

		/*
		 * Use group frame for the remaining multi grp shots
		 * instead of subdev frame
		 */
		framemgr = &group->framemgr;
		grp_frame = peek_frame(framemgr, FS_FREE);
		if (!grp_frame) {
			err("failed to get grp_frame");
			frame_manager_print_queues(framemgr);
			ret = -EINVAL;
			goto p_err;
		}

		grp_frame->index = frame->index;
		grp_frame->cur_shot_idx = cur_shot_idx;
		grp_frame->num_buffers = frame->num_buffers;
		grp_frame->planes = frame->planes;
		grp_frame->num_shots = frame->num_shots;
		grp_frame->shot_ext = (struct camera2_shot_ext *)
			queue->buf_kva[index][pos_meta_p + cur_shot_idx];
		grp_frame->shot = &frame->shot_ext->shot;
		grp_frame->shot_size = frame->shot_size;

		for (i = 0, p = (frame->planes * cur_shot_idx);
			i < frame->planes; i++, p++)
			grp_frame->dvaddr_buffer[i] = frame->dvaddr_buffer[p];

		memcpy(&grp_frame->out_node, &frame->out_node,
			sizeof(struct is_sub_node));
		memcpy(&grp_frame->cap_node, &frame->cap_node,
			sizeof(struct is_sub_node));

		frame = grp_frame;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_22, flags);

	if (frame->state == FS_FREE) {
		if (unlikely(frame->out_flag)) {
			mgwarn("output(0x%lX) is NOT completed", device, group, frame->out_flag);
			frame->out_flag = 0;
		}

		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
			(framemgr->queued_count[FS_REQUEST] >= DIV_ROUND_UP(framemgr->num_frames, 2))) {
				mgwarn(" request bufs : %d", device, group, framemgr->queued_count[FS_REQUEST]);
				frame_manager_print_queues(framemgr);
				sysfs_debug = is_get_sysfs_debug();
				if (test_bit(IS_HAL_DEBUG_PILE_REQ_BUF, &sysfs_debug->hal_debug_mode)) {
					mdelay(sysfs_debug->hal_debug_delay);
					panic("HAL panic for request bufs");
				}
		}

		IS_INIT_PMAP(frame->pmap);
		frame->result = 0;
		frame->fcount = frame->shot->dm.request.frameCount;
		frame->rcount = frame->shot->ctl.request.frameCount;

		is_group_override_meta(group, framemgr, frame, resourcemgr, device, sensor);

		is_group_override_sensor_req(group, framemgr, frame);

#ifdef ENABLE_MODECHANGE_CAPTURE
		if ((GET_DEVICE_TYPE_BY_GRP(group->id) == IS_DEVICE_SENSOR)
				&& (device->sensor && !test_bit(IS_SENSOR_FRONT_START, &device->sensor->state))) {
			device->sensor->mode_chg_frame = NULL;

			if (is_vendor_check_remosaic_mode_change(frame)) {
				clear_bit(IS_SENSOR_OTF_OUTPUT, &device->sensor->state);
				device->sensor->mode_chg_frame = frame;
			} else {
				if (group->child)
					set_bit(IS_SENSOR_OTF_OUTPUT, &device->sensor->state);
			}
		}
#endif

		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		err("frame(%d) is invalid state(%d)\n", index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_22, flags);

	is_group_update_stripe(group, frame, resourcemgr);

	pablo_group_update_prfi(group, frame);

	is_group_start_trigger(groupmgr, group, frame);

	if (cur_shot_idx++ < (frame->num_shots - 1))
		goto trigger_multi_grp_shot;

p_err:
	return ret;
}

int is_group_buffer_finish(struct is_groupmgr *groupmgr,
	struct is_group *group, u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!group->device);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);

	device = group->device;

	if (unlikely(!test_bit(IS_GROUP_OPEN, &group->state))) {
		warn("group was closed..(%d)", index);
		return ret;
	}

	if (unlikely(!group->leader.vctx)) {
		mgerr("leder vctx is null(%d)", device, group, index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(group->id >= GROUP_ID_MAX)) {
		mgerr("group id is invalid(%d)", device, group, index);
		ret = -EINVAL;
		return ret;
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group, 0);
	FIMC_BUG(index >= (framemgr ? framemgr->num_frames : 0));

	frame = &framemgr->frames[index];
	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_23, flags);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);

		frame->shot_ext->free_cnt = framemgr->queued_count[FS_FREE];
		frame->shot_ext->request_cnt = framemgr->queued_count[FS_REQUEST];
		frame->shot_ext->process_cnt = framemgr->queued_count[FS_PROCESS];
		frame->shot_ext->complete_cnt = framemgr->queued_count[FS_COMPLETE];
	} else {
		mgerr("frame(%d) is not com state(%d)", device, group, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_23, flags);

	PROGRAM_COUNT(15);
	TIME_SHOT(TMS_DQ);

	return ret;
}

static int is_group_check_pre(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device,
	struct is_group *gprev,
	struct is_group *group,
	struct is_group *gnext,
	struct is_frame *frame,
	struct is_group_frame **result)
{
	int ret = 0;
	struct is_group *group_leader;
	struct is_group_framemgr *gframemgr;
	struct is_group_frame *gframe;
	ulong flags;
	u32 capture_id;
	struct camera2_node *node;
	struct is_subdev *subdev;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!device);
	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(!frame->shot_ext);

	gframemgr = &groupmgr->gframemgr[device->instance];
	group_leader = groupmgr->leader[device->instance];

	/* invalid shot can be processed only on memory input */
	if (unlikely(!test_bit(IS_GROUP_OTF_INPUT, &group->state) &&
		frame->shot_ext->invalid)) {
		mgrerr("invalid shot", device, group, frame);
		ret = -EINVAL;
		goto p_err;
	}

	spin_lock_irqsave(&gframemgr->gframe_slock, flags);

	if (gprev && !gnext) {
		/* tailer */
		is_gframe_group_head(group, &gframe);
		if (unlikely(!gframe)) {
			mgrerr("gframe is NULL1", device, group, frame);
			is_gframe_print_free(gframemgr);
			is_gframe_print_group(group_leader);
			spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
			is_stream_status(groupmgr, group_leader);
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(frame->fcount != gframe->fcount)) {
			mgwarn("(gprev && !gnext) shot mismatch(%d != %d)", device, group,
				frame->fcount, gframe->fcount);
			gframe = is_gframe_rewind(groupmgr, group, frame->fcount);
			if (!gframe) {
				spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		is_gframe_s_info(gframe, group->slot, frame);
		is_gframe_check(gprev, group, gnext, gframe, frame);
	} else if (!gprev && gnext) {
		/* leader */
		group->fcount++;

		is_gframe_free_head(gframemgr, &gframe);
		if (unlikely(!gframe)) {
			mgerr("gframe is NULL2", device, group);
			is_gframe_print_free(gframemgr);
			is_gframe_print_group(group_leader);
			spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
			is_stream_status(groupmgr, group_leader);
			group->fcount--;
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) &&
			(frame->fcount != group->fcount))) {
			if (frame->fcount > group->fcount) {
				mgwarn("(!gprev && gnext) shot mismatch(%d, %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount = frame->fcount;
			} else {
				spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
				mgerr("!gprev && gnext) shot mismatch(%d, %d)", device, group,
					frame->fcount, group->fcount);
				group->fcount--;
				ret = -EINVAL;
				goto p_err;
			}
		}

		gframe->fcount = frame->fcount;
		is_gframe_s_info(gframe, group->slot, frame);
		is_gframe_check(gprev, group, gnext, gframe, frame);
	} else if (gprev && gnext) {
		/* middler */
		is_gframe_group_head(group, &gframe);
		if (unlikely(!gframe)) {
			mgrerr("gframe is NULL3", device, group, frame);
			is_gframe_print_free(gframemgr);
			is_gframe_print_group(group_leader);
			spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
			is_stream_status(groupmgr, group_leader);
			ret = -EINVAL;
			goto p_err;
		}

		if (unlikely(frame->fcount != gframe->fcount)) {
			mgwarn("(gprev && gnext) shot mismatch(%d != %d)", device, group,
				frame->fcount, gframe->fcount);
			gframe = is_gframe_rewind(groupmgr, group, frame->fcount);
			if (!gframe) {
				spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
				merr("rewinding is fail,can't recovery", group);
				goto p_err;
			}
		}

		is_gframe_s_info(gframe, group->slot, frame);
		is_gframe_check(gprev, group, gnext, gframe, frame);
	} else {
		if (group->gframe_skip) {
			/* VRA: Skip gframe logic. */
#ifndef DISABLE_CHECK_PERFRAME_FMT_SIZE
			struct is_crop *incrop
				= (struct is_crop *)frame->shot_ext->node_group.leader.input.cropRegion;
			subdev = &group->leader;

			if ((incrop->w * incrop->h) > (subdev->input.width * subdev->input.height)) {
				mrwarn("the input size is invalid(%dx%d > %dx%d)", group, frame,
						incrop->w, incrop->h,
						subdev->input.width, subdev->input.height);
				incrop->w = subdev->input.width;
				incrop->h = subdev->input.height;
			}
#endif
			for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
				node = &frame->shot_ext->node_group.capture[capture_id];
				if (!is_subdev_check_vid(node->vid))
					continue; /* Ignore it */

				subdev = video2subdev(IS_ISCHAIN_SUBDEV, (void *)device, node->vid);
				if (subdev)
					subdev->cid = capture_id;
			}
			gframe = &dummy_gframe;
		} else {
			/* single */
			group->fcount++;

			is_gframe_free_head(gframemgr, &gframe);
			if (unlikely(!gframe)) {
				mgerr("gframe is NULL4", device, group);
				is_gframe_print_free(gframemgr);
				is_gframe_print_group(group_leader);
				spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
				is_stream_status(groupmgr, group_leader);
				ret = -EINVAL;
				goto p_err;
			}

			if (unlikely(!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) &&
				(frame->fcount != group->fcount))) {
				if (frame->fcount > group->fcount) {
					mgwarn("shot mismatch(%d != %d)", device, group,
						frame->fcount, group->fcount);
					group->fcount = frame->fcount;
				} else {
					spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
					mgerr("shot mismatch(%d, %d)", device, group,
						frame->fcount, group->fcount);
					group->fcount--;
					ret = -EINVAL;
					goto p_err;
				}
			}

			is_gframe_s_info(gframe, group->slot, frame);
			is_gframe_check(gprev, group, gnext, gframe, frame);
		}
	}

	*result = gframe;

	spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);

p_err:
	return ret;
}

static int is_group_check_post(struct is_groupmgr *groupmgr,
	struct is_device_ischain *device,
	struct is_group *gprev,
	struct is_group *group,
	struct is_group *gnext,
	struct is_frame *frame,
	struct is_group_frame *gframe)
{
	int ret = 0;
	struct is_group *tail;
	struct is_group_framemgr *gframemgr;
	ulong flags;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!device);
	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(!gframe);
	FIMC_BUG(group->slot >= GROUP_SLOT_MAX);

	/* Skip gframe transition when it is doing stripe processing. */
	if (frame->state == FS_STRIPE_PROCESS)
		return ret;

	tail = group->tail;
	gframemgr = &groupmgr->gframemgr[group->instance];

	spin_lock_irqsave(&gframemgr->gframe_slock, flags);

	if (gprev && !gnext) {
		/* tailer */
		ret = is_gframe_trans_grp_to_fre(gframemgr, gframe, group);
		if (ret) {
			mgerr("is_gframe_trans_grp_to_fre is fail(%d)", device, group, ret);
			BUG();
		}
	} else if (!gprev && gnext) {
		/* leader */
		if (!tail || !tail->junction) {
			mgerr("junction is NULL", device, group);
			BUG();
		}

		if (gframe->group_cfg[group->slot].capture[tail->junction->cid].request) {
			ret = is_gframe_trans_fre_to_grp(gframemgr, gframe, group, gnext);
			if (ret) {
				mgerr("is_gframe_trans_fre_to_grp is fail(%d)", device, group, ret);
				BUG();
			}
		}
	} else if (gprev && gnext) {
		/* middler */
		if (!tail || !group->junction) {
			mgerr("junction is NULL", device, group);
			BUG();
		}

		/* gframe should be destroyed if the request of junction is zero, so need to check first */
		if (gframe->group_cfg[group->slot].capture[tail->junction->cid].request) {
			ret = is_gframe_trans_grp_to_grp(gframemgr, gframe, group, gnext);
			if (ret) {
				mgerr("is_gframe_trans_grp_to_grp is fail(%d)", device, group, ret);
				BUG();
			}
		} else {
			ret = is_gframe_trans_grp_to_fre(gframemgr, gframe, group);
			if (ret) {
				mgerr("is_gframe_trans_grp_to_fre is fail(%d)", device, group, ret);
				BUG();
			}
		}
	} else {
		/* VRA, CLAHE : Skip gframe logic. */
		if (!group->gframe_skip)
			/* single */
			gframe->fcount = frame->fcount;
	}

	spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);

	return ret;
}

#if IS_ENABLED(ENABLE_DVFS)
static bool is_group_throttling_tick_check(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *frame)
{
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;
	bool ret = false;
	int current_fps;

	if (!resourcemgr->limited_fps) {
		/* throttling release event */
		mgrinfo("throttling restore scenario(%d)-[%s]\n", device, group, frame,
			static_ctrl->cur_scenario_id,
			static_ctrl->scenario_nm);
		return true;
	}

	if (GET_DEVICE_TYPE_BY_GRP(group->id) == IS_DEVICE_SENSOR) {
		current_fps = frame->shot->ctl.aa.aeTargetFpsRange[1];

		if ((group->thrott_tick > 0) &&
			(current_fps == 3 || current_fps == 5 || current_fps == 15))
			group->thrott_tick--;

		if (!group->thrott_tick) {
			set_bit(IS_DVFS_TICK_THROTT, &resourcemgr->dvfs_ctrl.state);
			ret = true;
			mgrinfo("throttling scenario(%d)-[%s]\n", device, group, frame,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenario_nm);
		}

		mgrdbgs(3, "limited_fps(%d), current_fps(%d) tick(%d)\n",
			device, group, frame,
			resourcemgr->limited_fps, current_fps, group->thrott_tick);
	}

	return ret;
}

static void is_group_update_dvfs(struct is_device_ischain *device,
				struct is_group *group,
				struct is_frame *frame)
{
	struct is_resourcemgr *resourcemgr = device->resourcemgr;
	struct is_sysfs_debug *sysfs_debug;

	mutex_lock(&resourcemgr->dvfs_ctrl.lock);
	sysfs_debug = is_get_sysfs_debug();

	if ((!is_pm_qos_request_active(&device->user_qos)) && (sysfs_debug->en_dvfs)) {
		int scenario_id;

		/* try to find dynamic scenario to apply */
		scenario_id = is_dvfs_sel_dynamic(device, group, frame);
		if (scenario_id > 0) {
			struct is_dvfs_scenario_ctrl *dynamic_ctrl = resourcemgr->dvfs_ctrl.dynamic_ctrl;
			mgrinfo("dynamic scenario(%d)-[%s]\n", device, group, frame,
				scenario_id,
				dynamic_ctrl->scenario_nm);
			is_set_dvfs_m2m(device, scenario_id);
		}

		if ((scenario_id < 0) && (resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_frame_tick == 0)) {
			struct is_dvfs_scenario_ctrl *static_ctrl = resourcemgr->dvfs_ctrl.static_ctrl;
			mgrinfo("restore scenario(%d)-[%s]\n", device, group, frame,
				static_ctrl->cur_scenario_id,
				static_ctrl->scenario_nm);
			is_set_dvfs_m2m(device, static_ctrl->cur_scenario_id);
		}

		if (test_bit(IS_DVFS_TMU_THROTT, &resourcemgr->dvfs_ctrl.state)) {
			if (is_group_throttling_tick_check(device, group, frame)) {
				struct is_dvfs_scenario_ctrl *static_ctrl =
					resourcemgr->dvfs_ctrl.static_ctrl;
				clear_bit(IS_DVFS_TMU_THROTT, &resourcemgr->dvfs_ctrl.state);
				is_set_dvfs_m2m(device, static_ctrl->cur_scenario_id);
			}
		} else
			group->thrott_tick = KEEP_FRAME_TICK_THROTT;
	}
	mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
}
#else
#define is_group_update_dvfs(device, group, frame)	do { } while (0)
#endif

static inline void is_group_shot_recovery(struct is_groupmgr *groupmgr,
					struct is_group *group,
					struct is_group_task *gtask,
					bool try_sdown, bool try_rdown,
					bool try_gdown[])
{
	struct is_group *pos;
	struct is_group_task *gtask_child;

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		if (!test_bit(IS_GROUP_OPEN, &pos->state))
			break;

		gtask_child = &groupmgr->gtask[pos->id];
		if (try_gdown[pos->id])
			up(&gtask_child->smp_resource);
	}

	if (try_sdown)
		smp_shot_inc(group);

	if (try_rdown)
		up(&gtask->smp_resource);

	clear_bit(IS_GROUP_SHOT, &group->state);
}

int is_group_shot(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_group *gprev, *gnext;
	struct is_group_frame *gframe;
	struct is_group_task *gtask;
	struct is_group *pos;
	struct is_group_task *gtask_child;
	bool try_sdown = false;
	bool try_rdown = false;
	bool try_gdown[GROUP_ID_MAX];
	u32 gtask_child_id;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!group->shot_callback);
	FIMC_BUG(!group->device);
	FIMC_BUG(!frame);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);

	set_bit(IS_GROUP_SHOT, &group->state);
	memset(try_gdown, 0, sizeof(try_gdown));
	device = group->device;
	gtask = &groupmgr->gtask[group->id];

	if (unlikely(test_bit(IS_GROUP_STANDBY, &group->head->state))) {
		mgwarn(" cancel by ldr group standby", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop1", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
		mgerr(" cancel by gstop1", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	PROGRAM_COUNT(2);
	smp_shot_dec(group);
	try_sdown = true;

	PROGRAM_COUNT(3);
	ret = down_interruptible(&gtask->smp_resource);
	if (ret) {
		mgerr(" down fail(%d) #2", group, group, ret);
		goto p_err_ignore;
	}
	try_rdown = true;

	/* check for group stop */
	if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
		mgwarn(" cancel by fstop2", group, group);
		ret = -EINVAL;
		goto p_err_cancel;
	}

	if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
		mgerr(" cancel by gstop2", group, group);
		ret = -EINVAL;
		goto p_err_ignore;
	}

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		gtask_child = &groupmgr->gtask[pos->id];
		gtask_child_id = pos->id;
		if (!test_bit(IS_GTASK_START, &gtask_child->state))
			continue;

		ret = down_interruptible(&gtask_child->smp_resource);
		if (ret) {
			mgerr(" down fail(%d) #2", group, group, ret);
			goto p_err_ignore;
		}
		try_gdown[gtask_child_id] = true;
	}

	if (device->sensor && !test_bit(IS_SENSOR_FRONT_START, &device->sensor->state)) {
		/*
		 * this statement is execued only at initial.
		 * automatic increase the frame count of sensor
		 * for next shot without real frame start
		 */
		if (group->init_shots > atomic_read(&group->scount)) {
			set_bit(IS_SENSOR_START, &device->sensor->state);
			pablo_set_static_dvfs(device, "", IS_DVFS_SN_END, IS_DVFS_PATH_OTF);

			frame->fcount = atomic_read(&group->sensor_fcount);
			atomic_set(&group->backup_fcount, frame->fcount);
			atomic_inc(&group->sensor_fcount);

			goto p_skip_sync;
		}
	}

	if (group->sync_shots) {
#if defined(SYNC_SHOT_ALWAYS)
		PROGRAM_COUNT(4);
		ret = down_interruptible(&group->smp_trigger);
		if (ret) {
			mgerr(" down fail(%d) #4", group, group, ret);
			goto p_err_ignore;
		}
#else
		bool try_sync_shot = false;

		if (group->asyn_shots == 0) {
			try_sync_shot = true;
		} else {
			if ((smp_shot_get(group) < MIN_OF_SYNC_SHOTS))
				try_sync_shot = true;
			else
				if (atomic_read(&group->backup_fcount) >=
					atomic_read(&group->sensor_fcount))
					try_sync_shot = true;
		}

		if (try_sync_shot) {
			PROGRAM_COUNT(4);
			ret = down_interruptible(&group->smp_trigger);
			if (ret) {
				mgerr(" down fail(%d) #4", group, group, ret);
				goto p_err_ignore;
			}
		}

#endif

		/* check for group stop */
		if (unlikely(test_bit(IS_GROUP_FORCE_STOP, &group->state))) {
			mgwarn(" cancel by fstop3", group, group);
			ret = -EINVAL;
			goto p_err_cancel;
		}

		if (unlikely(test_bit(IS_GTASK_REQUEST_STOP, &gtask->state))) {
			mgerr(" cancel by gstop3", group, group);
			ret = -EINVAL;
			goto p_err_ignore;
		}

		frame->fcount = atomic_read(&group->sensor_fcount);
		atomic_set(&group->backup_fcount, frame->fcount);

#if defined(SYNC_SHOT_ALWAYS)
		/* Nothing */
#else
		/* real automatic increase */
		if (!try_sync_shot && (smp_shot_get(group) > MIN_OF_SYNC_SHOTS)) {
			atomic_inc(&group->sensor_fcount);
		}
#endif
	} else {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			frame->fcount = atomic_read(&group->sensor_fcount);
			atomic_set(&group->backup_fcount, frame->fcount);
		}
	}

p_skip_sync:
	PROGRAM_COUNT(6);
	gnext = group->gnext;
	gprev = group->gprev;
	gframe = NULL;

	ret = is_group_check_pre(groupmgr, device, gprev, group, gnext, frame, &gframe);
	if (unlikely(ret)) {
		merr(" is_group_check_pre is fail(%d)", device, ret);
		goto p_err_cancel;
	}

	if (unlikely(!gframe)) {
		merr(" gframe is NULL", device);
		goto p_err_cancel;
	}

#ifdef DEBUG_AA
	is_group_debug_aa_shot(group, frame);
#endif

	is_group_set_torch(group, frame);

#ifdef ENABLE_SHARED_METADATA
	is_hw_shared_meta_update(device, group, frame, SHARED_META_SHOT);
#endif

	ret = group->shot_callback(device, group, frame);
	if (unlikely(ret)) {
		mgerr(" shot_callback is fail(%d)", group, group, ret);
		goto p_err_cancel;
	}

	is_group_update_dvfs(device, group, frame);

	PROGRAM_COUNT(7);

	ret = is_group_check_post(groupmgr, device, gprev, group, gnext, frame, gframe);
	if (unlikely(ret)) {
		merr(" is_group_check_post is fail(%d)", device, ret);
		goto p_err_cancel;
	}

	ret = is_itf_grp_shot(device, group, frame);
	if (unlikely(ret)) {
		merr(" is_itf_grp_shot is fail(%d)", device, ret);
		goto p_err_cancel;
	}
	atomic_inc(&group->scount);

	clear_bit(IS_GROUP_SHOT, &group->state);
	PROGRAM_COUNT(12);
	TIME_SHOT(TMS_SHOT1);

	return 0;

p_err_cancel:
	is_group_cancel(group, frame);

p_err_ignore:
	is_group_shot_recovery(groupmgr, group, gtask, try_sdown, try_rdown, try_gdown);

	PROGRAM_COUNT(12);

	return ret;
}

int is_group_done(struct is_groupmgr *groupmgr,
	struct is_group *group,
	struct is_frame *frame,
	u32 done_state)
{
	int ret = 0;
	struct is_device_ischain *device;
	struct is_group_framemgr *gframemgr;
	struct is_group_frame *gframe;
	struct is_group *gnext;
	struct is_group_task *gtask;
#if !defined(ENABLE_SHARED_METADATA)
	struct is_group *pos;
#endif
	ulong flags;
	struct is_group_task *gtask_child;
	struct is_sysfs_debug *sysfs_debug;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);
	FIMC_BUG(!frame);
	FIMC_BUG(group->instance >= IS_STREAM_COUNT);
	FIMC_BUG(group->id >= GROUP_ID_MAX);
	FIMC_BUG(!group->device);

	/* check shot & resource count validation */
	device = group->device;
	gnext = group->gnext;
	gframemgr = &groupmgr->gframemgr[group->instance];
	gtask = &groupmgr->gtask[group->id];
	sysfs_debug = is_get_sysfs_debug();

	if (unlikely(test_bit(IS_ISCHAIN_REPROCESSING, &device->state) &&
		(done_state != VB2_BUF_STATE_DONE))) {
		merr("G%d NOT DONE(reprocessing)\n", group, group->id);
		if (test_bit(IS_HAL_DEBUG_NDONE_REPROCESSING, &sysfs_debug->hal_debug_mode)) {
			mdelay(sysfs_debug->hal_debug_delay);
			panic("HAL panic for NDONE reprocessing");
		}
	}

#ifdef DEBUG_AA
	is_group_debug_aa_done(group, frame);
#endif

	/* sensor tagging */
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		is_sensor_dm_tag(device->sensor, frame);

#ifdef ENABLE_SHARED_METADATA
	is_hw_shared_meta_update(device, group, frame, SHARED_META_SHOT_DONE);
#else
	for_each_group_child(pos, group) {
		if ((pos == &device->group_3aa) || (pos->subdev[ENTRY_3AA])) {
			/* NI(Noise Index) information backup */
			device->cur_noise_idx[frame->fcount % NI_BACKUP_MAX] =
				frame->shot->udm.ni.currentFrameNoiseIndex;
			device->next_noise_idx[(frame->fcount + 2) % NI_BACKUP_MAX] =
				frame->shot->udm.ni.nextNextFrameNoiseIndex;
		}

		/* wb gain backup for initial AWB */
		if (IS_ENABLED(INIT_AWB) && device->sensor
		    && ((pos == &device->group_isp) || (pos->subdev[ENTRY_ISP])))
			memcpy(device->sensor->last_wb, frame->shot->dm.color.gains,
							sizeof(float) * WB_GAIN_COUNT);

#if !defined(FAST_FDAE)
		if ((pos == &device->group_vra) || (pos->subdev[ENTRY_VRA])) {
			/* fd information backup */
			memcpy(&device->fdUd, &frame->shot->dm.stats,
				sizeof(struct camera2_fd_uctl));
		}
#endif
	}
#endif

	/* gframe should move to free list next group is existed and not done is oocured */
	if (unlikely((done_state != VB2_BUF_STATE_DONE) && gnext)) {
		spin_lock_irqsave(&gframemgr->gframe_slock, flags);

		gframe = is_gframe_group_find(gnext, frame->fcount);
		if (gframe) {
			ret = is_gframe_trans_grp_to_fre(gframemgr, gframe, gnext);
			if (ret) {
				mgerr("is_gframe_trans_grp_to_fre is fail(%d)", device, gnext, ret);
				BUG();
			}
		}

		spin_unlock_irqrestore(&gframemgr->gframe_slock, flags);
	}

	for_each_group_child(pos, group->child) {
		if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
			break;

		gtask_child = &groupmgr->gtask[pos->id];
		if (!test_bit(IS_GTASK_START, &gtask_child->state))
			continue;

		up(&gtask_child->smp_resource);
	}

	smp_shot_inc(group);
	up(&gtask->smp_resource);

	if (unlikely(frame->result)) {
		ret = is_devicemgr_shot_done(group, frame, frame->result);
		if (ret) {
			mgerr("is_devicemgr_shot_done is fail(%d)", device, group, ret);
			return ret;
		}
	}

#ifdef ENABLE_STRIPE_SYNC_PROCESSING
	/* Re-trigger the group shot for next stripe processing. */
	if (is_vendor_check_remosaic_mode_change(frame)
		&& (frame->state == FS_STRIPE_PROCESS)) {
		is_group_start_trigger(groupmgr, group, frame);
	}
#endif

	return ret;
}

int is_group_change_chain(struct is_groupmgr *groupmgr, struct is_group *group, u32 next_id)
{
	int ret = 0;
	int curr_id;
	struct is_group_task *curr_gtask;
	struct is_group_task *next_gtask;
	struct is_group *pos;
	u32 base_id;

	FIMC_BUG(!groupmgr);
	FIMC_BUG(!group);

	if (group->slot != GROUP_SLOT_SENSOR) {
		mgerr("The group is not a sensor group.", group, group);
		return -EINVAL;
	}

	if (!test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		mgerr("The group output is not a OTF.", group, group);
		return -EINVAL;
	}

	ret = is_itf_change_chain_wrap(group->device, group, next_id);
	if (ret) {
		mgerr("is_itf_change_chain_wrap is fail (next_id: %d)", group, group, next_id);
		goto p_err;
	}

	for_each_group_child(pos, group->child) {
		if (pos->slot == GROUP_SLOT_PAF) {
			base_id = GROUP_ID_PAF0;
			curr_id = pos->id - base_id;
		} else if (pos->slot == GROUP_SLOT_3AA) {
			base_id = GROUP_ID_3AA0;
			curr_id = pos->id - base_id;
		} else {
			mgerr("A child group is invalid.", group, group);
			goto p_err;
		}

		if (curr_id == next_id)
			continue;

		curr_gtask = &groupmgr->gtask[pos->id];
		ret = is_group_task_stop(groupmgr, curr_gtask, pos->slot);
		if (ret) {
			mgerr("is_group_task_stop is fail(%d)", pos, pos, ret);
			goto p_err;
		}

		next_gtask = &groupmgr->gtask[base_id + next_id];
		ret = is_group_task_start(groupmgr, next_gtask, pos->slot);
		if (ret) {
			mgerr("is_group_task_start is fail(%d)", pos, pos, ret);
			goto p_err;
		}

		mginfo("%s: done (%d --> %d)\n", pos, pos, __func__, curr_id, next_id);

		pos->id = base_id + next_id;
	}

p_err:
	clear_bit(IS_GROUP_STANDBY, &group->state);

	return ret;
}

struct is_framemgr *is_group_get_head_framemgr(struct is_group *group,
							u32 shot_idx)
{
	if (!group || !(group->head) || !(group->head->leader.vctx))
		return NULL;

	if (shot_idx)
		return &group->head->framemgr;
	else
		return &group->head->leader.vctx->queue.framemgr;
}
