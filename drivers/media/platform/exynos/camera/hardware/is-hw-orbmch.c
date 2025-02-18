// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-orbmch.h"
#include "is-err.h"
#include "api/is-hw-api-orbmch.h"

#define LIB_MODE USE_DRIVER
#define COREX_SET COREX_DIRECT


int debug_orbmch;
module_param(debug_orbmch, int, 0644);

IS_TIMER_FUNC(__is_hw_orbmch_reset)
{
	int ret;
	struct is_hw_ip *hw_ip = from_timer(hw_ip, (struct timer_list *)data, orbmch_stuck_timer);
	struct is_hw_orbmch *hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	struct is_frame *frame = NULL;
	struct is_framemgr *framemgr;
	ulong flags = 0;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 hw_fcount = atomic_read(&hw_ip->fcount);

	if (!test_and_clear_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("stuck timer interrupt happened without start interrupt", instance, hw_ip);
		return;
	}
	msinfo_hw("[F:%d] %s: orbmch stuck occurs. Reset and init\n", instance, hw_ip, hw_fcount, __func__);

	ret = orbmch_hw_s_reset(hw_ip->regs[REG_SETA]);
	if (ret)
		mserr_hw("sw reset fail", instance, hw_ip);

	orbmch_hw_s_init(hw_ip->regs[REG_SETA]);
	orbmch_hw_s_int_mask(hw_ip->regs[REG_SETA]);

	framemgr = hw_ip->framemgr;

	framemgr_e_barrier_irqs(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
	framemgr_x_barrier_irqr(framemgr, 0, flags);

	if (frame) {
		/* Notify user that this buffer is invalid. */
		hw_orbmch->invalid_flag[instance] = true;
		frame->shot_ext->node_group.leader.result = 0;
		is_hardware_frame_done(hw_ip, frame, -1, IS_HW_CORE_END, IS_SHOT_UNPROCESSED, false);
	} else
		mserr_hw("frame is NULL", instance, hw_ip);
}

static int __is_hw_orbmch_get_timeout_time(struct is_hw_ip *hw_ip, u32 instance)
{
	int cur_lv;
	u32 process_time;
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	cur_lv = is_dvfs_get_cur_lv(ORBMCH_DVFS_DOMAIN);
	process_time = orbmch_hw_g_process_time(cur_lv);
	hw_orbmch->timeout_time = process_time + ORBMCH_PROC_SW_MARGIN;

	msdbg_hw(2, "current dvfs level(%d) orbmch timeout time is %dms\n", instance, hw_ip, cur_lv, hw_orbmch->timeout_time);

	return 0;
}

static void __is_hw_orbmch_free_iq_set(struct is_hw_ip *hw_ip, u32 instance)
{
	int i;
	struct is_hw_orbmch *hw_orbmch;

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (hw_orbmch->iq_set[i].regs) {
			vfree(hw_orbmch->iq_set[i].regs);
			hw_orbmch->iq_set[i].regs = NULL;
		}
	}
	for (i = 0; i < COREX_MAX; i++) {
		if (hw_orbmch->cur_iq_set[i].regs) {
			vfree(hw_orbmch->cur_iq_set[i].regs);
			hw_orbmch->cur_iq_set[i].regs = NULL;
		}
	}
}

static int __is_hw_orbmch_alloc_iq_set(struct is_hw_ip *hw_ip, u32 instance)
{
	int i, ret = 0;
	struct is_hw_orbmch *hw_orbmch;
	u32 reg_cnt = orbmch_hw_get_reg_cnt();

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	memset(hw_orbmch->iq_set, 0, sizeof(struct is_hw_orbmch_iq) * IS_STREAM_COUNT);
	memset(hw_orbmch->cur_iq_set, 0, sizeof(struct is_hw_orbmch_iq) * COREX_MAX);
	/* init iq_set */
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		hw_orbmch->iq_set[i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_orbmch->iq_set[i].regs) {
			mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
			ret = -ENOMEM;
			goto err_regs_alloc;
		}
		clear_bit(CR_SET_CONFIG, &hw_orbmch->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_orbmch->iq_set[i].state);
		spin_lock_init(&hw_orbmch->iq_set[i].slock);
	}
	/* init cur_iq_set */
	for (i = 0; i < COREX_MAX; i++) {
		hw_orbmch->cur_iq_set[i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_orbmch->cur_iq_set[i].regs) {
			mserr_hw("failed to alloc cur_iq_set regs", instance, hw_ip);
			ret = -ENOMEM;
			goto err_regs_alloc;
		}
	}
	return 0;

err_regs_alloc:
	__is_hw_orbmch_free_iq_set(hw_ip, instance);

	return ret;
}

static int __is_hw_orbmch_init(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	msinfo_hw("reset\n", instance, hw_ip);

	ret = orbmch_hw_s_reset(hw_ip->regs[REG_SETA]);
	if (ret) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	msinfo_hw("init\n", instance, hw_ip);

	orbmch_hw_s_init(hw_ip->regs[REG_SETA]);
	orbmch_hw_s_int_mask(hw_ip->regs[REG_SETA]);


	return 0;
}

static int __is_hw_orbmch_enable(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	orbmch_hw_s_enable(hw_ip->regs[REG_SETA]);

	return 0;
}

static int __is_hw_orbmch_update_internal_param(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	struct is_frame *frame, u32 instance, struct is_subdev *subdev)
{
	struct is_framemgr *internal_framemgr = NULL;
	struct is_frame *prev_frame = NULL;
	struct is_frame *cur_frame = NULL;
	struct is_priv_buf *pb;
	u32 cur_idx = 0;
	u32 mchxsTargetAddress[IS_MAX_PLANES];
	u64 mchxsKTargetAddress[IS_MAX_PLANES];

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	internal_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	if (!internal_framemgr) {
		mserr_hw("Image internal framemgr is NULL", instance, hw_ip);
		return -EINVAL;
	}

	/* cur orb descriptor */
	cur_frame = &internal_framemgr->frames[0];
	if (!cur_frame) {
		mserr_hw("cur internal frame is NULL", instance, hw_ip);
		return -EINVAL;
	}
	/* prev orb descriptor */
	prev_frame = &internal_framemgr->frames[1];
	if (!prev_frame) {
		mserr_hw("prev internal frame is NULL", instance, hw_ip);
		return -EINVAL;
	}

	memset(mchxsTargetAddress, 0, sizeof(mchxsTargetAddress));
	memset(mchxsKTargetAddress, 0, sizeof(mchxsKTargetAddress));

	mchxsTargetAddress[0] = prev_frame->dvaddr_buffer[0];
	mchxsKTargetAddress[0] = prev_frame->kvaddr_buffer[0];
	mchxsTargetAddress[1] = cur_frame->dvaddr_buffer[0];
	mchxsKTargetAddress[1] = cur_frame->kvaddr_buffer[0];
	msdbg_hw(3, "[F:%d] orbmch prev descriptor rdma addr(%#x), cur descriptor wdma addr(%#x)\n",
		instance, hw_ip,
		frame->fcount,
		mchxsTargetAddress[0],
		mchxsTargetAddress[1]);

	/* cache invalidate */
	pb = cur_frame->pb_output;
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);
	msdbg_hw(2, "[F%d] orb cur descriptor cache invalidate: kva 0x%lx, size: %d", instance, hw_ip,
		frame->fcount, pb->kva, pb->size);
	pb = prev_frame->pb_output;
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);
	msdbg_hw(2, "[F%d] orb prev descriptor cache invalidate: kva 0x%lx, size: %d", instance, hw_ip,
		frame->fcount, pb->kva, pb->size);

	/* swap buffer */
	SWAP(cur_frame->dvaddr_buffer[0], prev_frame->dvaddr_buffer[0], u32);
	SWAP(cur_frame->kvaddr_buffer[0], prev_frame->kvaddr_buffer[0], u64);

	is_hardware_dma_cfg_32bit("mch_prev_desc_in", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->prev_output_dva,
			mchxsTargetAddress);
	is_hardware_dma_cfg_64bit("mch_prev_desc_in_k", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->prev_output_kva,
			mchxsKTargetAddress);

	return 0;
}

static int __is_hw_orbmch_set_control(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	u32 instance, u32 set_id)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	return 0;
}

static int __is_hw_orbmch_set_dma_cmd(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	u32 instance, struct is_orbmch_config *conf)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	return 0;
}

static int __is_hw_orbmch_set_rdma(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	u32 instance, u32 set_id, u32 dma_id)
{
	struct is_hw_orbmch *hw_orbmch;
	u32 input_dva;
	u32 dma_en = false;
	int ret;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	switch (dma_id) {
	case ORBMCH_RDMA_ORB_L1:
		input_dva = param_set->cur_input_dva[0];
		dma_en = param_set->dma.cur_input_cmd;
		break;
	case ORBMCH_RDMA_ORB_L2:
		input_dva = param_set->cur_input_dva[1];
		dma_en = param_set->dma.cur_input_cmd;
		break;
	case ORBMCH_RDMA_MCH_PREV_DESC:
		/* internal buffer */
		input_dva = param_set->prev_output_dva[0];
		dma_en = param_set->dma.cur_input_cmd;
		break;
	case ORBMCH_RDMA_MCH_CUR_DESC:
		/* internal buffer */
		input_dva = param_set->prev_output_dva[1];
		dma_en = param_set->dma.cur_input_cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: dma enable(%d) address(%#x)\n", instance, hw_ip,
		hw_orbmch->rdma[dma_id].name, dma_en, input_dva);

	ret = orbmch_hw_s_rdma_init(&hw_orbmch->rdma[dma_id], param_set, dma_en, set_id);
	if (ret) {
		mserr_hw("failed to initialize orbmch rdma(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	if (dma_en == DMA_INPUT_COMMAND_ENABLE) {
		ret = orbmch_hw_s_rdma_addr(&hw_orbmch->rdma[dma_id], input_dva, set_id);
		if (ret) {
			mserr_hw("failed to set orbmch rdma(%d)", instance, hw_ip, dma_id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_orbmch_set_wdma(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	u32 instance, u32 set_id, u32 dma_id)
{
	struct is_hw_orbmch *hw_orbmch;
	u32 output_dva;
	u32 dma_en = false;
	int ret;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	switch (dma_id) {
	case ORBMCH_WDMA_ORB_KEY:
		output_dva = param_set->output_dva[0];
		dma_en = param_set->dma.output_cmd;
		break;
	case ORBMCH_WDMA_ORB_DESC:
		output_dva = param_set->prev_output_dva[1];
		dma_en = param_set->dma.output_cmd;
		break;
	case ORBMCH_WDMA_MCH_RESULT:
		output_dva = param_set->output_dva[1];
		dma_en = param_set->dma.output_cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: dma enable(%d) address(%#x)\n", instance, hw_ip,
		hw_orbmch->wdma[dma_id].name, dma_en, output_dva);

	ret = orbmch_hw_s_wdma_init(&hw_orbmch->wdma[dma_id], param_set, dma_en, set_id);
	if (ret) {
		mserr_hw("failed to initializ orbmch wdma(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	if (dma_en == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = orbmch_hw_s_wdma_addr(&hw_orbmch->wdma[dma_id], output_dva, set_id);
		if (ret) {
			mserr_hw("failed to set orbmch wdma(%d)", instance, hw_ip, dma_id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_orbmch_set_dma(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set, u32 instance, u32 set_id)
{
	int ret, i;

	/* ORBMCH RDMA */
	for (i = ORBMCH_RDMA_ORB_L1; i < ORBMCH_RDMA_MAX; i++) {
		ret = __is_hw_orbmch_set_rdma(hw_ip, param_set, instance, set_id, i);
		if (ret) {
			mserr_hw("__is_hw_orbmch_set_rdma is fail", instance, hw_ip);
			return ret;
		}
	}
	/* ORBMCH WDMA */
	for (i = ORBMCH_WDMA_ORB_KEY; i < ORBMCH_WDMA_MAX; i++) {
		ret = __is_hw_orbmch_set_wdma(hw_ip, param_set, instance, set_id, i);
		if (ret) {
			mserr_hw("__is_hw_orbmch_set_wdma is fail", instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int __is_hw_orbmch_bypass_iq_module(struct is_hw_ip *hw_ip, struct orbmch_param_set *param_set,
	u32 instance, u32 set_id)
{
	u32 cur_width, cur_height;
	struct is_hw_orbmch *hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	memset(&hw_orbmch->iq_config[instance], 0x0, sizeof(struct is_orbmch_config));

	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;
	orbmch_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id, cur_width, cur_height);

	return 0;
}

static void __is_hw_orbmch_update_param(struct is_hw_ip *hw_ip, struct is_region *region,
	struct orbmch_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct orbmch_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!region);
	FIMC_BUG_VOID(!param_set);

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	param = &region->parameter.orbmch;
	param_set->instance_id = instance;

	/* check input/output */
	memcpy(&param_set->dma, &param->dma,
		sizeof(struct param_orbmch_dma));
}

static int __is_hw_orbmch_set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, bool update_iq_reg)
{
	struct is_hw_orbmch *hw_orbmch;
	struct is_hw_orbmch_iq *iq_set;
	struct is_hw_orbmch_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i, ret = 0;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	iq_set = &hw_orbmch->iq_set[instance];
	cur_iq_set = &hw_orbmch->cur_iq_set[set_id];

	spin_lock_irqsave(&iq_set->slock, flag);
	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;

		if (update_iq_reg || cur_iq_set->size != regs_size ||
		    memcmp(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size) != 0) {
			for (i = 0; i < regs_size; i++)
				writel_relaxed(regs[i].reg_data,
					hw_ip->regs[REG_SETA] + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size);
			cur_iq_set->size = regs_size;
		}
		set_bit(CR_SET_EMPTY, &iq_set->state);
	} else {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
				instance, hw_ip,
				atomic_read(&hw_ip->fcount),
				iq_set->fcount, iq_set->state, cur_iq_set->fcount);
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&iq_set->slock, flag);

	return ret;
}

static int __is_hw_orbmch_set_size(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	return 0;
}

static void is_hw_orbmch_handle_start_interrupt(struct is_hw_ip *hw_ip, struct is_hw_orbmch *hw_orbmch, u32 instance)
{
	struct is_hardware *hardware;
	u32 hw_fcount = atomic_read(&hw_ip->fcount);

	hardware = hw_ip->hardware;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		is_lib_isp_event_notifier(hw_ip, &hw_orbmch->lib[instance],
								instance, hw_fcount, EVENT_ORBMCH_FRAME_START, 0, NULL);
	} else {
		atomic_add(1, &hw_ip->count.fs);
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		is_hardware_frame_start(hw_ip, instance);
	}
}

static void is_hw_orbmch_handle_end_interrupt(struct is_hw_ip *hw_ip, struct is_hw_orbmch *hw_orbmch,
	u32 instance, u32 status)
{
	struct is_hardware *hardware;
	u32 hw_fcount = atomic_read(&hw_ip->fcount);

	hardware = hw_ip->hardware;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		orbmch_hw_s_frame_done(hw_ip->regs[REG_SETA], instance, hw_fcount,
			&hw_orbmch->param_set[instance], &hw_orbmch->data);

		is_lib_isp_event_notifier(hw_ip, &hw_orbmch->lib[instance],
					instance, hw_fcount, EVENT_ORBMCH_MOTION_DATA, 0, &hw_orbmch->data);

		is_lib_isp_event_notifier(hw_ip, &hw_orbmch->lib[instance],
					instance, hw_fcount, EVENT_ORBMCH_FRAME_END, 0, NULL);
	} else {
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
			mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)",
					instance, hw_ip,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe),
					atomic_read(&hw_ip->count.dma),
					status);
		}
		wake_up(&hw_ip->status.wait_queue);
	}
}

static int is_hw_orbmch_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_orbmch *hw_orbmch;
	u32 status, instance, hw_fcount;
	u32 f_err;
	int hw_slot;

	hw_ip = (struct is_hw_ip *)context;

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	if (!hw_orbmch) {
		err("failed to get ORBMCH");
		return 0;
	}
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	status = orbmch_hw_g_int_status(hw_ip->regs[REG_SETA], true,
			hw_ip->num_buffers & 0xffff,
			&hw_orbmch->irq_state[ORBMCH_INTR]);

	msdbg_hw(2, "ORBMCH interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (orbmch_hw_is_occurred(status, ORBMCH_INTR_FRAME_START)
		&& orbmch_hw_is_occurred(status, ORBMCH_INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (orbmch_hw_is_occurred(status, ORBMCH_INTR_FRAME_START)) {
		msdbg_hw(2, "[F:%d] F.S\n", instance, hw_ip, hw_fcount);
		if (!test_and_clear_bit(HW_CONFIG, &hw_ip->state))
			mswarn_hw("start interrupt(%#x) occurred without hw setting", instance, hw_ip, status);
		set_bit(HW_RUN, &hw_ip->state);
		/* To apply ORBMCH SW W/A, register timer to check orbmch stuck */
		mod_timer(&hw_ip->orbmch_stuck_timer, jiffies + msecs_to_jiffies(hw_orbmch->timeout_time));
		is_hw_orbmch_handle_start_interrupt(hw_ip, hw_orbmch, instance);
	}

	if (orbmch_hw_is_occurred(status, ORBMCH_INTR_FRAME_END)) {
		msdbg_hw(2, "[F:%d] F.E\n", instance, hw_ip, hw_fcount);
		if (!test_and_clear_bit(HW_RUN, &hw_ip->state)) {
			mserr_hw("end interrupt(%#x) occurred without start interrupt or after reset", instance, hw_ip, status);
			return 0;
		}
		/* To apply ORBMCH SW W/A */
		del_timer(&hw_ip->orbmch_stuck_timer);
		is_hw_orbmch_handle_end_interrupt(hw_ip, hw_orbmch, instance, status);
	}

	f_err = orbmch_hw_is_occurred(status, ORBMCH_INTR_ERR);
	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt status(0x%x), irq_id(%d)\n",
				instance, hw_ip, hw_fcount, status, id);
		orbmch_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __nocfi is_hw_orbmch_open(struct is_hw_ip *hw_ip, u32 instance, struct is_group *group)
{
	int ret;
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWORBMCH");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_orbmch));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (IS_ENABLED(USE_ORBMCH_WA))
		is_hw_orbmch_isr_clear_register(hw_ip->id, true);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	if (!hw_orbmch) {
		mserr_hw("failed to get hw_orbmch", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_orbmch->instance = instance;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		/* FIXME: LIB_FUNC_LME -> LIB_FUNC_ORBMCH */
		ret = get_lib_func(LIB_FUNC_LME, (void **)&hw_orbmch->lib_func);
		if (!hw_orbmch->lib_func) {
			mserr_hw("hw_orbmch->lib_func(null)", instance, hw_ip);
			is_load_clear();
			ret = -EINVAL;
			goto err_lib_func;
		}
		msinfo_hw("get_lib_func is set\n", instance, hw_ip);
	}

	hw_orbmch->lib[instance].func = hw_orbmch->lib_func;
	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_orbmch->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	ret = __is_hw_orbmch_alloc_iq_set(hw_ip, instance);
	if (ret) {
		mserr_hw("__is_hw_orbmch_alloc_iq_set fail", instance, hw_ip);
		goto err_iqset_alloc;
	}

	/* init orbmch timer */
	timer_setup(&hw_ip->orbmch_stuck_timer,
		(void (*)(struct timer_list *))(__is_hw_orbmch_reset), 0);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
				GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_orbmch->lib[instance], instance);
err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);

	return ret;
}

static int is_hw_orbmch_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret;
	struct is_device_ischain *device;
	struct is_hw_orbmch *hw_orbmch;
	u32 dma_id, f_type;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	if (!hw_orbmch) {
		mserr_hw("hw_orbmch is null ", instance, hw_ip);
		return -ENODATA;
	}

	hw_ip->lib_mode = LIB_MODE;
	hw_orbmch->lib[instance].object = NULL;
	hw_orbmch->lib[instance].func = hw_orbmch->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_orbmch->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip,
						&hw_orbmch->lib[instance], instance,
						(u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}
	/* Alloc internal buffer for previous descriptor as MCH input */
	is_hardware_alloc_internal_buffer(device, &device->orbxc, group->leader.vid,
		ORB_DESC_STRIDE, ORB_REGION_NUM, DMA_INOUT_BIT_WIDTH_8BIT,
		ORB_DESC_BUF_NUM, "ORB_DESC");

	/* Create ORBMCH RDMA */
	for (dma_id = ORBMCH_RDMA_ORB_L1; dma_id < ORBMCH_RDMA_MAX; dma_id++) {
		ret = orbmch_hw_rdma_create(&hw_orbmch->rdma[dma_id], hw_ip->regs[REG_SETA], dma_id);
		if (ret) {
			mserr_hw("orbmch_hw_rdma_create error[%d]", instance, hw_ip, dma_id);
			return -ENODEV;
		}
	}
	/* Create ORBMCH WDMA */
	for (dma_id = ORBMCH_WDMA_ORB_KEY; dma_id < ORBMCH_WDMA_MAX; dma_id++) {
		ret = orbmch_hw_wdma_create(&hw_orbmch->wdma[dma_id], hw_ip->regs[REG_SETA], dma_id);
		if (ret) {
			mserr_hw("orbmch_hw_wdma_create error[%d]", instance, hw_ip, dma_id);
			return -ENODEV;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;
}

static int is_hw_orbmch_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_orbmch *hw_orbmch;
	struct is_device_ischain *device;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_orbmch->lib[instance], instance);
		hw_orbmch->lib[instance].object = NULL;
	}

	device = hw_ip->group[instance]->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	/* Release internal buffer for previous descriptor as MCH input */
	is_hardware_free_internal_buffer(device, &device->orbxc);

	return 0;
}

static int is_hw_orbmch_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_orbmch *hw_orbmch;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	msdbg_hw(2, "[HW][%s]%d: is called\n", instance, hw_ip, __func__, __LINE__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(USE_ORBMCH_WA))
		is_hw_orbmch_isr_clear_register(hw_ip->id, false);

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_orbmch->lib[instance], instance);

	msinfo_hw("reset\n", instance, hw_ip);
	ret = orbmch_hw_s_reset(hw_ip->regs[REG_SETA]);
	if (ret)
		mserr_hw("orbmch hw sw reset fail(%d)", instance, hw_ip, ret);

	del_timer(&hw_ip->orbmch_stuck_timer);

	__is_hw_orbmch_free_iq_set(hw_ip, instance);

	vfree(hw_orbmch);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_orbmch_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	/* Initialize invalid flag per instance */
	hw_orbmch->invalid_flag[instance] = false;

	if (atomic_inc_return(&hw_ip->run_rsccount) > 1)
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	__is_hw_orbmch_init(hw_ip, instance);

	/* Initialize iq config */
	memset(&hw_orbmch->iq_config[instance], 0x0, sizeof(struct is_orbmch_config));
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_orbmch_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_orbmch *hw_orbmch;
	struct orbmch_param_set *param_set;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("orbmch_disable: Vvalid(%d)\n", instance, hw_ip,
			atomic_read(&hw_ip->status.Vvalid));

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
					!atomic_read(&hw_ip->status.Vvalid),
					IS_HW_STOP_TIMEOUT);
	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip,
				timetowait);
		ret = -ETIME;
	}

	param_set = &hw_orbmch->param_set[instance];
	param_set->fcount = 0;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		is_lib_isp_stop(hw_ip, &hw_orbmch->lib[instance], instance, 1);

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int is_hw_orbmch_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_device_ischain *device;
	struct is_hw_orbmch *hw_orbmch;
	struct is_region *region;
	struct orbmch_param_set *param_set;
	struct orbmch_param *param;
	bool update_iq_reg = false;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret, batch_num;

	msdbg_hw(2, "[HW][%s] is called\n", frame->instance, hw_ip, __func__);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	device = hw_ip->group[instance]->device;
	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	param_set = &hw_orbmch->param_set[instance];
	region = hw_ip->region[instance];
	param = &region->parameter.orbmch;
	fcount = frame->fcount;
	cur_idx = frame->cur_buf_index;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		param_set->dma.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->cur_input_dva[0] = 0x0;

		param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva[0] = 0x0;

		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);
		/*
		 * per-frame control
		 * check & update size from region
		 */
		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}

		/*set TNR operation mode */
		if (frame->shot_ext) {
			if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
			!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
				msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, frame->fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
			param_set->tnr_mode = frame->shot_ext->tnr_mode;
		} else {
			mswarn_hw("frame->shot_ext is null", instance, hw_ip);
			param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		}
	}

	__is_hw_orbmch_update_param(hw_ip, region, param_set, frame->pmap, instance);

	/* DMA settings */
	is_hardware_dma_cfg("orb_cur_in", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->dvaddr_buffer);
	is_hardware_dma_cfg_32bit("mch_mv_out", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_dva,
			frame->orbxcTargetAddress);
	is_hardware_dma_cfg_64bit("mch_pure_out", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_pure,
			frame->orbxcKTargetAddress);
	is_hardware_dma_cfg_64bit("mch_proc_out", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_processed,
			frame->orbxmKTargetAddress);

config:
	if (!param_set->dma.cur_input_cmd) {
		mswarn_hw("orb input buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}
	if (!param_set->dma.output_cmd) {
		mswarn_hw("mv buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;

	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	}

	/* Check previous frame is invalid by HW stuck */
	if (hw_orbmch->invalid_flag[instance]) {
		mswarn_hw("prev descriptor buffer is disabled", instance, hw_ip);
		param_set->dma.prev_output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		update_iq_reg = true;
	} else {
		param_set->dma.prev_output_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	}
	hw_orbmch->invalid_flag[instance] = false;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		if (frame->shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_orbmch->lib[instance], frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}
	}

	/* temp direct set */
	set_id = COREX_SET;
	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		ret = is_lib_isp_shot(hw_ip, &hw_orbmch->lib[instance], param_set, frame->shot);
		if (ret)
			return ret;

		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
		ret = __is_hw_orbmch_set_rta_regs(hw_ip, instance, set_id, update_iq_reg);
		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_X);
		if (ret > 0) {
			msinfo_hw("set_regs is not called from ddk\n", instance, hw_ip);
			__is_hw_orbmch_bypass_iq_module(hw_ip, param_set, instance, set_id);
		} else if (ret) {
			mserr_hw("__is_hw_orbmch_set_rta_regs is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	} else {
		__is_hw_orbmch_bypass_iq_module(hw_ip, param_set, instance, set_id);
	}

	ret = __is_hw_orbmch_update_internal_param(hw_ip, param_set, frame, instance, &device->orbxc);
	if (ret) {
		mserr_hw("__is_hw_orbmch_update_internal_param is fail", instance, hw_ip);
		return ret;
	}

	ret = __is_hw_orbmch_set_control(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_orbmch_init_config is fail", instance, hw_ip);
		return ret;
	}

	ret = __is_hw_orbmch_set_size(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_orbmch_set_size is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_orbmch_set_dma(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_orbmch_set_dma is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_orbmch_get_timeout_time(hw_ip, instance);
	if (ret) {
		mserr_hw("__is_hw_orbmch_get_timeout_time is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	ret = __is_hw_orbmch_enable(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_orbmch_enable is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(debug_orbmch))
		orbmch_hw_dump(hw_ip->regs[REG_SETA]);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_orbmch->lib[instance], instance);

	return ret;
}

static int is_hw_orbmch_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	return 0;
}

static int is_hw_orbmch_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	int ret;
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", frame->instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_orbmch->lib[frame->instance], frame);
		if (ret) {
			mserr_hw("get_meta fail", frame->instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int is_hw_orbmch_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_orbmch *hw_orbmch;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	if (unlikely(!hw_orbmch)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_orbmch->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail", instance, hw_ip);
	}

	return ret;
}

static int is_hw_orbmch_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	return is_hardware_frame_done(hw_ip, frame, -1, IS_HW_CORE_END, done_type, false);
}

static int is_hw_orbmch_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag;
	ulong addr;
	u32 size, index;
	struct is_hw_orbmch *hw_orbmch;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	switch (setfile->version) {
	case SETFILE_V2:
		flag = false;
		break;
	case SETFILE_V3:
		flag = true;
		break;
	default:
		mserr_hw("invalid version (%d)", instance, hw_ip,
			setfile->version);
		return -EINVAL;
	}

	FIMC_BUG(!hw_ip->priv_info);
	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
				setfile->using_count);
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_orbmch->lib[instance],
				addr, size, index, flag, instance);

			set_bit(index, &hw_orbmch->lib[instance].tune_count);
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_orbmch_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario, u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index;
	struct is_hw_orbmch *hw_orbmch;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
			instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip,
			setfile_index, scenario);

	FIMC_BUG(!hw_ip->priv_info);
	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_orbmch->lib[instance],
				setfile_index, instance, scenario);

	return ret;
}

static int is_hw_orbmch_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_orbmch *hw_orbmch;
	int i, ret = 0;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	if (setfile->using_count == 0)
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_orbmch->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_orbmch->lib[instance],
						(u32)i, instance);
				if (ret)
					mserr_hw("is_lib_isp_delete_tune_set fail", instance, hw_ip);

				clear_bit(i, &hw_orbmch->lib[instance].tune_count);
			}
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_orbmch_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_orbmch_init(hw_ip, instance)) {
		mserr_hw("__is_hw_orbmch_init fail", instance, hw_ip);
		return -ENODEV;
	}

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL)) {
		ret = is_lib_isp_reset_recovery(hw_ip, &hw_orbmch->lib[instance], instance);
		if (ret) {
			mserr_hw("is_lib_isp_reset_recovery fail ret(%d)", instance, hw_ip, ret);

			return ret;
		}
	}

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return 0;
}

int is_hw_orbmch_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance,
	u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_orbmch *hw_orbmch;
	struct is_hw_orbmch_iq *iq_set;
	ulong flag = 0;

	msdbg_hw(2, "[HW][%s] is called : chain_id (%d)\n", instance, hw_ip, __func__, chain_id);

	FIMC_BUG(!regs);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	iq_set = &hw_orbmch->iq_set[instance];

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, sizeof(struct cr_set) * regs_size);

	spin_unlock_irqrestore(&iq_set->slock, flag);
	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "Store IQ regs set: %p, size(%d)\n", instance, hw_ip, regs, regs_size);

	return 0;
}


int is_hw_orbmch_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_orbmch *hw_orbmch = NULL;
	u32 i;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		orbmch_hw_dump(hw_ip->regs[REG_SETA]);
		break;
	case IS_REG_DUMP_DMA:
		for (i = ORBMCH_RDMA_ORB_L1; i < ORBMCH_RDMA_MAX; i++) {
			hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
			dma = &hw_orbmch->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = ORBMCH_WDMA_ORB_KEY; i < ORBMCH_WDMA_MAX; i++) {
			hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
			dma = &hw_orbmch->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int is_hw_orbmch_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);
	FIMC_BUG(!conf);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;
	memcpy(&hw_orbmch->iq_config[instance], conf, sizeof(struct is_orbmch_config));

	return __is_hw_orbmch_set_dma_cmd(hw_ip, &hw_orbmch->param_set[instance], instance, conf);
}

static int is_hw_orbmch_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_orbmch *hw_orbmch;

	msdbg_hw(2, "[HW][%s] is called\n", instance, hw_ip, __func__);

	hw_orbmch = (struct is_hw_orbmch *)hw_ip->priv_info;

	orbmch_hw_dump(hw_ip->regs[REG_SETA]);

	if (IS_ENABLED(ORBMCH_DDK_LIB_CALL))
		return is_lib_notify_timeout(&hw_orbmch->lib[instance], instance);
	else
		return 0;
}

const struct is_hw_ip_ops is_hw_orbmch_ops = {
	.open			= is_hw_orbmch_open,
	.init			= is_hw_orbmch_init,
	.deinit			= is_hw_orbmch_deinit,
	.close			= is_hw_orbmch_close,
	.enable			= is_hw_orbmch_enable,
	.disable		= is_hw_orbmch_disable,
	.shot			= is_hw_orbmch_shot,
	.set_param		= is_hw_orbmch_set_param,
	.get_meta		= is_hw_orbmch_get_meta,
	.get_cap_meta		= is_hw_orbmch_get_cap_meta,
	.frame_ndone		= is_hw_orbmch_frame_ndone,
	.load_setfile		= is_hw_orbmch_load_setfile,
	.apply_setfile		= is_hw_orbmch_apply_setfile,
	.delete_setfile		= is_hw_orbmch_delete_setfile,
	.restore		= is_hw_orbmch_restore,
	.set_regs		= is_hw_orbmch_set_regs,
	.dump_regs		= is_hw_orbmch_dump_regs,
	.set_config		= is_hw_orbmch_set_config,
	.notify_timeout		= is_hw_orbmch_notify_timeout,
};

int is_hw_orbmch_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_orbmch_ops;
	hw_ip->dump_for_each_reg = orbmch_hw_get_reg_struct();
	hw_ip->dump_reg_list_size = orbmch_hw_get_reg_cnt();

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_orbmch_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	sinfo_hw("probe done\n", hw_ip);

	return 0;
}
