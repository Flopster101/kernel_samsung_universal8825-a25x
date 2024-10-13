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

#include "is-hw-mcfp.h"
#include "is-err.h"
#include "api/is-hw-api-mcfp-v10_0.h"

static int debug_mcfp;
module_param(debug_mcfp, int, 0644);

#define GET_MCFP_CAP_NODE_IDX(video_num) (video_num - IS_LVN_MCFP_PREV_YUV)

static int is_hw_mcfp_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_mcfp *hw_mcfp;
	struct mcfp_param_set *param_set;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount, strip_index, strip_total_count, hl = 0, vl = 0;
	u32 f_err;
	int hw_slot;
	u32 cur_idx, batch_num;
	bool is_first_shot, is_last_shot;

	hw_ip = (struct is_hw_ip *)context;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_mcfp->param_set[instance];
	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_total_count) {
			/* normal shot */
			is_first_shot = true;
			is_last_shot = true;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && !cur_idx) || (strip_total_count && !strip_index))
				is_first_shot = true;
			else
				is_first_shot = false;

			if ((batch_num && (batch_num - 1 == cur_idx)) ||
			(strip_total_count && (strip_total_count - 1 == strip_index)))
				is_last_shot = true;
			else
				is_last_shot = false;
		}
	} else {
		is_first_shot = true;
		is_last_shot = true;
	}

	status = mcfp_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff),
			MCFP_INTR_0, &hw_mcfp->irq_state[MCFP_INTR_0])
			& mcfp_hw_g_int_mask(hw_ip->regs[REG_SETA], MCFP_INTR_0);

	msdbg_hw(2, "MCFP interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt", instance, hw_ip);
		return 0;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		return 0;
	}

	if (mcfp_hw_is_occured(status, INTR_FRAME_START) && mcfp_hw_is_occured(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (mcfp_hw_is_occured(status, INTR_FRAME_START)) {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_mcfp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip,
						hw_fcount);

			is_hardware_frame_start(hw_ip, instance);
		}
	}

	if (mcfp_hw_is_occured(status, INTR_FRAME_END)) {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_mcfp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_END,
					strip_index, NULL);
		} else {
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.E\n", instance, hw_ip,
						hw_fcount);

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

	f_err = mcfp_hw_is_occured(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		mcfp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __is_hw_mcfp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_mcfp->cur_hw_iq_set[i].size = 0;

	return mcfp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_mcfp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp;
	u32 seed;

	if (!hw_ip) {
		err("hw_ip is NULL");
		return -ENODEV;
	}

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	msinfo_hw("reset\n", instance, hw_ip);
	if (__is_hw_mcfp_s_reset(hw_ip, hw_mcfp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	mcfp_hw_s_cmdq_init(hw_ip->regs[REG_SETA]);
	mcfp_hw_s_init(hw_ip->regs[REG_SETA]);
	mcfp_hw_s_cmdq_enable(hw_ip->regs[REG_SETA], 1);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		mcfp_hw_s_crc(hw_ip->regs[REG_SETA], seed);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_mcfp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_mcfp *hw_mcfp;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (__is_hw_mcfp_s_reset(hw_ip, hw_mcfp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	res = mcfp_hw_wait_idle(hw_ip->regs[REG_SETA]);
	if (res)
		mserr_hw("failed to mcfp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished mcfp\n", instance, hw_ip);

	return res;
}

static int __is_hw_mcfp_free_buffer(struct is_group *group, struct is_subdev *subdev_mcfp)
{
	struct is_device_ischain *device;
	int ret = 0;

	device = group->device;

	if (test_bit(IS_SUBDEV_OPEN, &subdev_mcfp->state)) {
		ret = is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, subdev_mcfp);
		if (ret)
			merr("is_subdev_internal_stop is failed(%d)", device, ret);

		ret = is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, subdev_mcfp);
		if (ret)
			merr("is_subdev_internal_close is failed(%d)", device, ret);
	}

	return ret;
}

static void __is_hw_mcfp_swap_frame_buffer(struct is_frame *cur_frame, struct is_frame *next_frame)
{
	int i;

	/* IS_LVN_MCFP_PREV_YUV -> IS_LVN_MCFP_YUV */
	for (i = 0; i < cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_YUV)].num_planes; i++) {
		next_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_YUV)].dva[i] =
			cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_YUV)].dva[i];
		cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_YUV)].dva[i] =
			next_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_YUV)].dva[i];
	}

	/* IS_LVN_MCFP_PREV_W -> IS_LVN_MCFP_W */
	for (i = 0; i < cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_W)].num_planes; i++) {
		next_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_W)].dva[i] =
			cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_W)].dva[i];
		cur_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_W)].dva[i] =
			next_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_W)].dva[i];
	}
}

static int __is_hw_mcfp_alloc_buffer(struct is_group *group, struct is_subdev *subdev_mcfp,
					struct mcfp_param_set *param_set,
					struct is_mcfp_config *config)
{
	struct is_device_ischain *device;
	struct is_frame *cur_frame, *next_frame;
	int ret;
	struct is_framemgr *internal_framemgr;
	u32 width, height;
	u32 sbwc_type, lossy_byte32num;

	device = group->device;

	if (!test_bit(IS_SUBDEV_OPEN, &subdev_mcfp->state)) {
		ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN,
						group->leader.vid, subdev_mcfp);
		if (ret) {
			merr("is_subdev_internal_open is fail(%d)", device, ret);
			return ret;
		}

		if (device->yuv_max_width && device->yuv_max_height) {
			width = device->yuv_max_width;
			height = device->yuv_max_height;
		} else {
			width = group->subdev[ENTRY_MCFP]->input.width;
			height = group->subdev[ENTRY_MCFP]->input.height;
		}

		mginfo("%s : width %d, height %d\n", device, group, __func__, width, height);

		sbwc_type = param_set->dma_output_yuv.sbwc_type;

		if (config->img_bit == 8)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U8;
		else if (config->img_bit == 10)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U10;
		else
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U12;

		ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, subdev_mcfp,
			width, height, config->img_bit, sbwc_type, lossy_byte32num, 2, "MCFP");
		if (ret) {
			merr("is_subdev_internal_s_format is fail(%d)", device, ret);
			goto err_subdev_start;
		}

		ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, subdev_mcfp);
		if (ret) {
			merr("subdev internal start is fail(%d)", device, ret);
			goto err_subdev_start;
		}

		internal_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev_mcfp);

		cur_frame = &internal_framemgr->frames[0];
		next_frame = &internal_framemgr->frames[1];

		__is_hw_mcfp_swap_frame_buffer(cur_frame, next_frame);
	}

	return 0;

err_subdev_start:
	is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, subdev_mcfp);
	return ret;
}

static int is_hw_mcfp_set_cmdq(struct is_hw_ip *hw_ip)
{
	mcfp_hw_s_cmdq_queue(hw_ip->regs[REG_SETA]);

	return 0;
}

static int __is_hw_mcfp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame, u32 set_id)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_device_ischain *device;
	struct is_group *group;
	struct is_hardware *hardware;
	struct mcfp_param_set *param_set;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("failed to get mcfp", instance, hw_ip);
		return -ENODEV;
	}

	group = hw_ip->group[instance];
	if (!group) {
		mserr_hw("failed to get group", instance, hw_ip);
		return -ENODEV;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	param_set = &hw_mcfp->param_set[instance];

	/* mcfp context setting */
	mcfp_hw_s_core(hw_ip->regs[REG_SETA], set_id);

	return 0;
}

static void __is_hw_mcfp_free_iqset(struct is_hw_ip *hw_ip)
{
	struct is_hw_mcfp *hw = (struct is_hw_mcfp *)hw_ip->priv_info;
	u32 set_i;

	for (set_i = 0; set_i < COREX_MAX; set_i++) {
		if (hw->cur_hw_iq_set[set_i].regs) {
			vfree(hw->cur_hw_iq_set[set_i].regs);
			hw->cur_hw_iq_set[set_i].regs = NULL;
		}
	}

	for (set_i = 0; set_i < IS_STREAM_COUNT; set_i++) {
		if (hw->iq_set[set_i].regs) {
			vfree(hw->iq_set[set_i].regs);
			hw->iq_set[set_i].regs = NULL;
		}
	}
}

static int __is_hw_mcfp_alloc_iqset(struct is_hw_ip *hw_ip)
{
	int ret;
	struct is_hw_mcfp *hw = (struct is_hw_mcfp *)hw_ip->priv_info;
	u32 instance = hw->instance;
	u32 reg_cnt = mcfp_hw_g_reg_cnt();
	u32 set_i;

	for (set_i = 0; set_i < IS_STREAM_COUNT; set_i++) {
		hw->iq_set[set_i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw->iq_set[set_i].regs) {
			mserr_hw("failed to alloc iq_set[%d].regs", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	for (set_i = 0; set_i < COREX_MAX; set_i++) {
		hw->cur_hw_iq_set[set_i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw->cur_hw_iq_set[set_i].regs) {
			mserr_hw("failed to alloc cur_hw_iq_set[%d].regs", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	return 0;

err_alloc:
	__is_hw_mcfp_free_iqset(hw_ip);

	return ret;
}

static int __nocfi is_hw_mcfp_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int i, ret;
	struct is_hw_mcfp *hw_mcfp;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWMCFP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_mcfp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("failed to get hw_mcfp", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_mcfp->instance = instance;

	is_fpsimd_get_func();
	ret = get_lib_func(LIB_FUNC_MCFP, (void **)&hw_mcfp->lib_func);
	is_fpsimd_put_func();

	if (hw_mcfp->lib_func == NULL) {
		mserr_hw("hw_mcfp->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_mcfp->lib_support = is_get_lib_support();

	hw_mcfp->lib[instance].func = hw_mcfp->lib_func;
	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_mcfp->lib[instance],
				instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	if (__is_hw_mcfp_alloc_iqset(hw_ip))
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_mcfp->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_mcfp->iq_set[i].state);
		spin_lock_init(&hw_mcfp->iq_set[i].slock);
	}

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	if (IS_ENABLED(MCFP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_mcfp->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_mcfp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret;
	struct is_hw_mcfp *hw_mcfp;
	struct is_device_ischain *device;
	u32 input_id, f_type;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("hw_mcfp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}
	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	hw_mcfp->lib[instance].object = NULL;
	hw_mcfp->lib[instance].func   = hw_mcfp->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_mcfp->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip,
					&hw_mcfp->lib[instance], instance,
					(u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	for (input_id = MCFP_RDMA_CUR_IN_Y; input_id < MCFP_RDMA_MAX; input_id++) {
		ret = mcfp_hw_rdma_create(&hw_mcfp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("mcfp_hw_rdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	for (input_id = MCFP_WDMA_PREV_OUT_Y; input_id < MCFP_WDMA_MAX; input_id++) {
		ret = mcfp_hw_wdma_create(&hw_mcfp->wdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("mcfp_hw_wdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_mcfp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_mcfp *hw_mcfp;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_mcfp->lib[instance],
				instance);
		hw_mcfp->lib[instance].object = NULL;
	}

	return ret;
}

static int is_hw_mcfp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp = NULL;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	FIMC_BUG(!hw_mcfp->lib_support);

	if (IS_ENABLED(MCFP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_mcfp->lib[instance],
				instance);

	__is_hw_mcfp_clear_common(hw_ip, instance);

	__is_hw_mcfp_free_iqset(hw_ip);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);


	return 0;
}

static int is_hw_mcfp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	__is_hw_mcfp_s_common_reg(hw_ip, instance);

	memset(&hw_mcfp->config[instance], 0x0, sizeof(struct is_mcfp_config));

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_mcfp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_mcfp *hw_mcfp;
	struct mcfp_param_set *param_set;
	struct is_group *group;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("mcfp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait)
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);

	param_set = &hw_mcfp->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(MCFP_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_mcfp->lib[instance],
					instance, 1);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	group = hw_ip->group[instance];

	ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_STILL]);
	if (ret)
		mserr_hw("__is_hw_mcfp_free_buffer(still) is failed(%d)",
			instance, hw_ip, ret);

	ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_VIDEO]);
	if (ret)
		mserr_hw("__is_hw_mcfp_free_buffer(video) is failed(%d)",
			instance, hw_ip, ret);

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void __is_hw_mcfp_set_dma_cmd(struct mcfp_param_set *param_set, u32 instance,
					struct is_mcfp_config *conf)
{
	if (conf->mixer_enable) {
		if (conf->still_en) {
			/* MF-Still */
			param_set->dma_input_sf.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
		} else {
			/* Video TNR */
			param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_ENABLE;
			switch (conf->mixer_mode) {
			case MCFP_TNR_MODE_PREPARE:
				param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				break;
			case MCFP_TNR_MODE_FIRST:
				param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			case MCFP_TNR_MODE_NORMAL:
				param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			default:
				break;
			}

			/* 1/2 dma */
			if (conf->skip_wdma && (conf->mixer_mode == MCFP_TNR_MODE_NORMAL)) {
				param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			}
		}
	} else {
		param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
}

static int __is_hw_mcfp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp,
	struct mcfp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0;
	struct param_dma_input *dma_input;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->dma_input.width;
	frame_height = param_set->dma_input.height;

	switch (id) {
	case MCFP_RDMA_CUR_IN_Y:
	case MCFP_RDMA_CUR_IN_UV:
		input_dva = param_set->input_dva;
		dma_input = &param_set->dma_input;
		break;
	case MCFP_RDMA_PRE_MV:
	case MCFP_RDMA_GEOMATCH_MV:
	case MCFP_RDMA_MIXER_MV:
		input_dva = param_set->input_dva_mv;
		dma_input = &param_set->dma_input_mv;
		break;
	case MCFP_RDMA_CUR_DRC:
		input_dva = param_set->input_dva_drc_cur_map;
		dma_input = &param_set->dma_input_drc_cur_map;
		break;
	case MCFP_RDMA_PREV_DRC:
		input_dva = param_set->input_dva_drc_prv_map;
		dma_input = &param_set->dma_input_drc_prv_map;
		break;
	case MCFP_RDMA_SAT_FLAG:
		input_dva = param_set->input_dva_sf;
		dma_input = &param_set->dma_input_sf;
		break;
	case MCFP_RDMA_PREV_IN0_Y:
	case MCFP_RDMA_PREV_IN0_UV:
	case MCFP_RDMA_PREV_IN1_Y:
	case MCFP_RDMA_PREV_IN1_UV:
		input_dva = param_set->input_dva_prv_yuv;
		dma_input = &param_set->dma_input_prev_yuv;
		break;
	case MCFP_RDMA_PREV_IN_WGT:
		input_dva = param_set->input_dva_prv_w;
		dma_input = &param_set->dma_input_prev_w;
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mcfp->rdma[id].name, dma_input->cmd);

	mcfp_hw_s_dma_corex_id(&hw_mcfp->rdma[id], set_id);

	ret = mcfp_hw_s_rdma_init(&hw_mcfp->rdma[id], dma_input,
		&param_set->stripe_input,
		frame_width, frame_height,
		&comp_sbwc_en, &payload_size, &strip_offset, &hw_mcfp->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MCFP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_input->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mcfp_hw_s_rdma_addr(&hw_mcfp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size, strip_offset);
		if (ret) {
			mserr_hw("failed to set MCFP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mcfp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_mcfp *hw_mcfp,
	struct mcfp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *output_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0, header_offset = 0;
	struct param_dma_output *dma_output;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->dma_input.width;
	frame_height = param_set->dma_input.height;

	switch (id) {
	case MCFP_WDMA_PREV_OUT_Y:
	case MCFP_WDMA_PREV_OUT_UV:
		output_dva = param_set->output_dva_yuv;
		dma_output = &param_set->dma_output_yuv;
		break;
	case MCFP_WDMA_PREV_OUT_WGT:
		output_dva = param_set->output_dva_w;
		dma_output = &param_set->dma_output_w;
		break;
	default:
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mcfp->wdma[id].name, dma_output->cmd);

	mcfp_hw_s_dma_corex_id(&hw_mcfp->wdma[id], set_id);

	ret = mcfp_hw_s_wdma_init(&hw_mcfp->wdma[id], dma_output,
		&param_set->stripe_input,
		frame_width, frame_height,
		&comp_sbwc_en, &payload_size, &strip_offset, &header_offset,
		&hw_mcfp->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MCFP_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_output->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mcfp_hw_s_wdma_addr(&hw_mcfp->wdma[id], output_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MCFP_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mcfp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	mcfp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_mcfp_update_block_reg(struct is_hw_ip *hw_ip,
	struct mcfp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_mcfp *hw_mcfp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (hw_mcfp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_mcfp->instance);
		hw_mcfp->instance = instance;
	}

	param_set->dma_input_prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_input_prev_w.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_input_mv.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	param_set->dma_output_w.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	return __is_hw_mcfp_bypass(hw_ip, set_id);
}

static void __is_hw_mcfp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct mcfp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct mcfp_param *param;

	param = &p_region->mcfp;
	param_set->instance_id = instance;

	if (test_bit(PARAM_MCFP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
				sizeof(struct param_otf_input));

	if (test_bit(PARAM_MCFP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_MCFP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
				sizeof(struct param_dma_input));

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_MCFP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));

	if (test_bit(PARAM_MCFP_PREV_YUV, pmap))
		memcpy(&param_set->dma_input_prev_yuv, &param->prev_yuv,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_PREV_W, pmap))
		memcpy(&param_set->dma_input_prev_w, &param->prev_w,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_DRC, pmap))
		memcpy(&param_set->dma_input_drc_cur_map, &param->drc,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_DP, pmap))
		memcpy(&param_set->dma_input_drc_prv_map, &param->dp,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_MV, pmap))
		memcpy(&param_set->dma_input_mv, &param->mv,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_SF, pmap))
		memcpy(&param_set->dma_input_sf, &param->sf,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_MCFP_W, pmap))
		memcpy(&param_set->dma_output_w, &param->w,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_MCFP_YUV, pmap))
		memcpy(&param_set->dma_output_yuv, &param->yuv,
				sizeof(struct param_dma_output));
}

static int is_hw_mcfp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	hw_mcfp->instance = IS_STREAM_COUNT;

	return 0;
}

static int __is_hw_mcfp_set_rta_regs(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_mcfp_iq *iq_set;
	struct is_hw_mcfp_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i;
	bool configured = false;

	mdbg_hw(4, "%s\n", instance, __func__);

	iq_set = &hw_mcfp->iq_set[instance];
	cur_iq_set = &hw_mcfp->cur_hw_iq_set[set_id];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;
		if (cur_iq_set->size != regs_size ||
			memcmp(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel_relaxed(regs[i].reg_data,
						base + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size);
			cur_iq_set->size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
				instance, hw_ip,
				atomic_read(&hw_ip->fcount),
				iq_set->fcount, iq_set->state, cur_iq_set->fcount);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_mcfp_set_size_regs(struct is_hw_ip *hw_ip, struct mcfp_param_set *param_set,
	u32 instance, const struct is_frame *frame, u32 set_id)
{
	struct is_device_ischain *device;
	struct is_hw_mcfp *hw_mcfp;
	struct is_mcfp_config *mcfp_config;
	struct is_param_region *param_region;
	u32 strip_enable;
	u32 crop_img_width, crop_wgt_width;
	u32 crop_img_x, crop_wgt_x;
	u32 crop_img_start_x;
	u32 crop_margin_for_align;
	u32 dma_width, dma_height;
	u32 frame_width;
	u32 strip_start_pos;
	u32 stripe_index;
	u32 region_id;
	u32 img_shift_bit;
	u32 wgt_shift_bit;
	struct mcfp_radial_cfg radial_cfg;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	mcfp_config = &hw_mcfp->config[instance];

	device = hw_ip->group[instance]->device;
	param_region = &device->is_region->parameter;
	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;

	region_id = param_set->stripe_input.index;
	frame_width = param_set->stripe_input.full_width;
	dma_width = param_set->dma_input.width;
	dma_height = param_set->dma_input.height;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : dma_width;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos = (stripe_index) ? (param_set->stripe_input.start_pos_x) : 0;

	radial_cfg.sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	radial_cfg.sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	radial_cfg.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	radial_cfg.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	radial_cfg.taa_crop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	radial_cfg.taa_crop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	radial_cfg.taa_crop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	radial_cfg.taa_crop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	radial_cfg.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	radial_cfg.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	radial_cfg.bns_binning_x = frame->shot->udm.frame_info.bns_binning[0];
	radial_cfg.bns_binning_y = frame->shot->udm.frame_info.bns_binning[1];

	mcfp_hw_s_otf_input(hw_ip->regs[REG_SETA], set_id, param_set->otf_input.cmd);
	mcfp_hw_s_nr_otf_input(hw_ip->regs[REG_SETA], set_id, 0);
	mcfp_hw_s_otf_output(hw_ip->regs[REG_SETA], set_id, param_set->otf_output.cmd);

	mcfp_hw_s_input_size(hw_ip->regs[REG_SETA], set_id, dma_width, dma_height);
	mcfp_hw_s_geomatch_size(hw_ip->regs[REG_SETA], set_id, frame_width,
				dma_width, dma_height,
				strip_enable, strip_start_pos,
				mcfp_config);
	mcfp_hw_s_mixer_size(hw_ip->regs[REG_SETA], set_id, frame_width,
		dma_width, dma_height, strip_enable, strip_start_pos,
		&radial_cfg, mcfp_config);

	mcfp_hw_s_crop_clean_img_otf(hw_ip->regs[REG_SETA], set_id, 0, dma_width);
	mcfp_hw_s_crop_wgt_otf(hw_ip->regs[REG_SETA], set_id, 0, dma_width);

	if (strip_enable) {
		crop_img_start_x = ALIGN_DOWN(strip_start_pos + param_set->stripe_input.left_margin,
						MCFP_COMP_BLOCK_WIDTH * 2);
		crop_margin_for_align = strip_start_pos + param_set->stripe_input.left_margin -
						 crop_img_start_x;
		crop_img_x = param_set->stripe_input.left_margin - crop_margin_for_align;
		crop_img_width = dma_width - param_set->stripe_input.left_margin -
				param_set->stripe_input.right_margin + crop_margin_for_align;

		crop_wgt_x = param_set->stripe_input.left_margin >> 1;
		crop_wgt_width = (dma_width - param_set->stripe_input.left_margin -
					param_set->stripe_input.right_margin) >> 1;
	} else {
		crop_img_start_x = 0;
		crop_img_x = 0;
		crop_img_width = dma_width;

		crop_wgt_x = 0;
		crop_wgt_width = crop_img_width >> 1;
	}

	param_set->dma_output_yuv.dma_crop_offset_x = crop_img_start_x;
	param_set->dma_output_yuv.dma_crop_width = crop_img_width;

	mcfp_hw_s_crop_clean_img_dma(hw_ip->regs[REG_SETA], set_id,
					crop_img_x, crop_img_width, dma_height,
					!strip_enable);
	mcfp_hw_s_crop_wgt_dma(hw_ip->regs[REG_SETA], set_id,
				crop_wgt_x, crop_wgt_width, dma_height >> 1,
				!strip_enable);

	img_shift_bit = DMA_INOUT_BIT_WIDTH_12BIT - mcfp_config->img_bit;
	wgt_shift_bit = DMA_INOUT_BIT_WIDTH_8BIT - mcfp_config->wgt_bit;
	mcfp_hw_s_img_bitshift(hw_ip->regs[REG_SETA], set_id, img_shift_bit);
	mcfp_hw_s_wgt_bitshift(hw_ip->regs[REG_SETA], set_id, wgt_shift_bit);

	return 0;
}

static int is_hw_mcfp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_param_region *param_region;
	struct mcfp_param_set *param_set;
	struct is_frame *dma_frame;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret, i, batch_num;
	u32 cmd_input, cmd_mv_in, cmd_drc_in, cmd_dp_in;
	u32 cmd_sf_in, cmd_prev_yuv_in, cmd_prev_wgt_in;
	u32 cmd_yuv_out, cmd_wgt_out;
	u32 strip_enable;
	u32 stripe_index;
	struct is_group *group;
	struct is_framemgr *internal_framemgr;
	struct is_subdev *subdev_mcfp;
	struct is_mcfp_config *config;
	bool do_blk_cfg = true;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	bool skip_isp_shot;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot(%d)\n", instance, hw_ip, frame->fcount, frame->cur_buf_index);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	param_set = &hw_mcfp->param_set[instance];
	param_region = (struct is_param_region *)frame->shot->ctl.vendor_entry.parameter;

	atomic_set(&hw_mcfp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;

	if (hw_ip->internal_fcount[instance] != 0)
		hw_ip->internal_fcount[instance] = 0;

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

	__is_hw_mcfp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	cmd_input = is_hardware_dma_cfg("mcfp", hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			frame->dvaddr_buffer);

	cmd_mv_in = is_hardware_dma_cfg_32bit("mv", hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_mv.cmd,
			param_set->dma_input_mv.plane,
			param_set->input_dva_mv,
			frame->dva_mcfp_motion);

	cmd_drc_in = is_hardware_dma_cfg_32bit("drc", hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc_cur_map.cmd,
			param_set->dma_input_drc_cur_map.plane,
			param_set->input_dva_drc_cur_map,
			frame->dva_mcfp_cur_drc);

	cmd_dp_in = is_hardware_dma_cfg_32bit("dp", hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc_prv_map.cmd,
			param_set->dma_input_drc_prv_map.plane,
			param_set->input_dva_drc_prv_map,
			frame->dva_mcfp_prev_drc);

	cmd_sf_in = is_hardware_dma_cfg_32bit("sf", hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_sf.cmd,
			param_set->dma_input_sf.plane,
			param_set->input_dva_sf,
			frame->dva_mcfp_sat_flag);

	cmd_prev_wgt_in = is_hardware_dma_cfg_32bit("prev_wgt",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_prev_w.cmd,
			param_set->dma_input_prev_w.plane,
			param_set->input_dva_prv_w,
			frame->dva_mcfp_prev_wgt);

	cmd_prev_yuv_in = param_set->dma_input_prev_yuv.cmd;
	cmd_yuv_out = param_set->dma_output_yuv.cmd;

	cmd_wgt_out = is_hardware_dma_cfg_32bit("wgt_out",
			hw_ip, frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_w.cmd,
			param_set->dma_output_w.plane,
			param_set->output_dva_w,
			frame->dva_mcfp_wgt);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;
	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= cur_idx << CURR_INDEX_SHIFT;
	} else {
		batch_num = 0;
	}

	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;
	stripe_index = param_set->stripe_input.index;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_enable) {
			/* normal shot */
			skip_isp_shot = false;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && !cur_idx) || (strip_enable && !stripe_index))
				skip_isp_shot = false;
			else
				skip_isp_shot = true;
		}
	} else {
		skip_isp_shot = false;
	}

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_mcfp->lib[instance], frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}
	}

	/* temp direct set */
	set_id = COREX_DIRECT;

	ret = __is_hw_mcfp_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_mcfp_init_config is fail", instance, hw_ip);
		return ret;
	}

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_shot(hw_ip, &hw_mcfp->lib[instance], param_set,
					frame->shot);
			if (ret)
				return ret;

			if (likely(!test_bit(hw_ip->id, &debug_iq))) {
				_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
				ret = __is_hw_mcfp_set_rta_regs(hw_ip, set_id, instance);
				_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_X);
				if (ret)
					msinfo_hw("set_regs is not called from ddk\n", instance, hw_ip);
				else
					do_blk_cfg = false;
			} else {
				msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
			}
		} else {
			if (likely(!test_bit(hw_ip->id, &debug_iq)))
				do_blk_cfg = false;
		}
	}

	group = hw_ip->group[instance];
	config = &hw_mcfp->config[instance];

	if (unlikely(do_blk_cfg))
		__is_hw_mcfp_update_block_reg(hw_ip, param_set, instance, set_id);
	else
		__is_hw_mcfp_set_dma_cmd(param_set, instance, config);

	if (config->geomatch_en && !config->geomatch_bypass && !cmd_mv_in) {
		mswarn_hw("LMC is enabled, but mv buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	ret = __is_hw_mcfp_set_size_regs(hw_ip, param_set, instance, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_mcfp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (config->mixer_enable) {
		if (config->still_en) {
			ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_VIDEO]);
			if (ret)
				mserr_hw("__is_hw_mcfp_free_buffer(video) is failed(%d)",
					instance, hw_ip, ret);

			subdev_mcfp = group->subdev[ENTRY_MCFP_STILL];
		} else {
			ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_STILL]);
			if (ret)
				mserr_hw("__is_hw_mcfp_free_buffer(still) is failed(%d)",
					instance, hw_ip, ret);

			subdev_mcfp = group->subdev[ENTRY_MCFP_VIDEO];
		}

		ret = __is_hw_mcfp_alloc_buffer(group, subdev_mcfp, param_set, config);
		if (ret) {
			mserr_hw("__is_hw_mcfp_alloc_buffer is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}

		internal_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev_mcfp);

		/*PEEK*/
		/* Case 1 -  1/2 dma in video tnr normal mode */
		/* Case 2 -  0 ~ N-2 frame in N strip procssing. (swap in last striped-frame) */
		if ((config->skip_wdma && !config->still_en &&
		    (config->mixer_mode == MCFP_TNR_MODE_NORMAL)) ||
		    (strip_enable && stripe_index < (param_set->stripe_input.total_count - 1))) {
			dma_frame = peek_frame(internal_framemgr, FS_FREE);
		} else {
			dma_frame = get_frame(internal_framemgr, FS_FREE);
			put_frame(internal_framemgr, dma_frame, FS_FREE);
		}

		if (!dma_frame) {
			mserr_hw("There is no FREE internal DMA frames. %d\n", instance, hw_ip,
					internal_framemgr->queued_count[FS_FREE]);
			goto shot_fail_recovery;
		}

		/* mv rdma */
		/* enable mv rdma even if zero motion */
		if (config->geomatch_bypass) {
			param_set->dma_input_mv.plane =
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_MV)].num_planes;
			is_hardware_dma_cfg("mv", hw_ip, frame, 0, frame->num_buffers,
				&param_set->dma_input_mv.cmd,
				param_set->dma_input_mv.plane,
				param_set->input_dva_mv,
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_MV)].dva);
		}

		if (config->mixer_enable && config->still_en && !cmd_sf_in) {
			param_set->dma_input_sf.plane =
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_SF)].num_planes;
			is_hardware_dma_cfg("sf", hw_ip, frame, 0, frame->num_buffers,
				&param_set->dma_input_sf.cmd,
				param_set->dma_input_sf.plane,
				param_set->input_dva_sf,
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_SF)].dva);
		}

		/* prev wgt plane */
		param_set->dma_input_prev_w.plane =
			dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_W)].num_planes;
		is_hardware_dma_cfg("prev_wgt", hw_ip, frame, 0, frame->num_buffers,
			&param_set->dma_input_prev_w.cmd,
			param_set->dma_input_prev_w.plane,
			param_set->input_dva_prv_w,
			dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_W)].dva);

		/* out wgt */
		param_set->dma_output_w.plane =
			dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_W)].num_planes;
		is_hardware_dma_cfg("wgt_out", hw_ip, frame, 0, frame->num_buffers,
			&param_set->dma_output_w.cmd,
			param_set->dma_output_w.plane,
			param_set->output_dva_w,
			dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_W)].dva);

		if (!config->still_en) {
			param_set->dma_input_prev_yuv.sbwc_type =
				param_set->dma_output_yuv.sbwc_type;
			/* prev yuv */
			param_set->dma_input_prev_yuv.plane =
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_YUV)].num_planes;
			is_hardware_dma_cfg("prev_yuv", hw_ip, frame, 0, frame->num_buffers,
				&param_set->dma_input_prev_yuv.cmd,
				param_set->dma_input_prev_yuv.plane,
				param_set->input_dva_prv_yuv,
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_PREV_YUV)].dva);

			/* output yuv */
			param_set->dma_output_yuv.plane =
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_YUV)].num_planes;
			is_hardware_dma_cfg("yuv_out", hw_ip, frame, 0, frame->num_buffers,
				&param_set->dma_output_yuv.cmd,
				param_set->dma_output_yuv.plane,
				param_set->output_dva_yuv,
				dma_frame->cap_node.sframe[GET_MCFP_CAP_NODE_IDX(IS_LVN_MCFP_YUV)].dva);
		} else {
			is_hardware_dma_cfg_32bit("prev_yuv",
						hw_ip, frame, cur_idx, frame->num_buffers,
						&param_set->dma_input_prev_yuv.cmd,
						param_set->dma_input_prev_yuv.plane,
						param_set->input_dva_prv_yuv,
						frame->dva_mcfp_prev_yuv);

			is_hardware_dma_cfg_32bit("yuv_out",
						hw_ip, frame, cur_idx, frame->num_buffers,
						&param_set->dma_output_yuv.cmd,
						param_set->dma_output_yuv.plane,
						param_set->output_dva_yuv,
						frame->dva_mcfp_yuv);
		}
	} else {
		ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_STILL]);
		if (ret)
			mserr_hw("__is_hw_mcfp_free_buffer(still) is failed(%d)",
				instance, hw_ip, ret);

		ret = __is_hw_mcfp_free_buffer(group, group->subdev[ENTRY_MCFP_VIDEO]);
		if (ret)
			mserr_hw("__is_hw_mcfp_free_buffer(video) is failed(%d)",
				instance, hw_ip, ret);
	}

	for (i = MCFP_RDMA_CUR_IN_Y; i < MCFP_RDMA_MAX; i++) {
		ret = __is_hw_mcfp_set_rdma(hw_ip, hw_mcfp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mcfp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = MCFP_WDMA_PREV_OUT_Y; i < MCFP_WDMA_MAX; i++) {
		ret = __is_hw_mcfp_set_wdma(hw_ip, hw_mcfp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mcfp_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	ret = is_hw_mcfp_set_cmdq(hw_ip);
	if (ret) {
		mserr_hw("is_hw_mcfp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	/* Restore CMDs in param_set. */
	param_set->dma_input_prev_yuv.cmd = cmd_prev_yuv_in;
	param_set->dma_input_mv.cmd  = cmd_mv_in;
	param_set->dma_input_sf.cmd  = cmd_sf_in;
	param_set->dma_input_prev_w.cmd = cmd_prev_wgt_in;
	param_set->dma_output_yuv.cmd = cmd_yuv_out;
	param_set->dma_output_w.cmd = cmd_wgt_out;

	if (unlikely(debug_mcfp))
		mcfp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(MCFP_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_mcfp->lib[instance], instance);

	return ret;
}

static int is_hw_mcfp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_mcfp *hw_mcfp;

	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (unlikely(!hw_mcfp)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_mcfp->lib[frame->instance],
				frame);
		if (ret) {
			mserr_hw("get_meta fail", frame->instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int is_hw_mcfp_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_mcfp->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail ret(%d)", instance, hw_ip, ret);
	}

	return ret;
}

static int is_hw_mcfp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, -1,
			output_id, done_type, false);
	}

	return ret;
}

static int is_hw_mcfp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag;
	ulong addr;
	u32 size, index;
	struct is_hw_mcfp *hw_mcfp;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

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

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
				setfile->using_count);
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_mcfp->lib[instance],
					addr, size, index, flag, instance);

			set_bit(index, &hw_mcfp->lib[instance].tune_count);
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_mcfp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index;
	struct is_hw_mcfp *hw_mcfp;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

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

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (IS_ENABLED(MCFP_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_mcfp->lib[instance],
				setfile_index, instance, scenario);

	return ret;
}

static int is_hw_mcfp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_mcfp *hw_mcfp;
	int ret = 0;
	int i;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

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

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_mcfp->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_mcfp->lib[instance],
						(u32)i, instance);
				clear_bit(i, &hw_mcfp->lib[instance].tune_count);
			}
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_mcfp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_mcfp *hw_mcfp;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	msinfo_hw("hw_mcfp_reset\n", instance, hw_ip);

	if (__is_hw_mcfp_s_common_reg(hw_ip, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	if (IS_ENABLED(MCFP_DDK_LIB_CALL)) {
		ret = is_lib_isp_reset_recovery(hw_ip, &hw_mcfp->lib[instance],
				instance);
		if (ret) {
			mserr_hw("is_lib_mcfp_reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
			return ret;
		}
	}

	return 0;
}

static int is_hw_mcfp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_hw_mcfp_iq *iq_set;
	ulong flag = 0;

	FIMC_BUG(!regs);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	iq_set = &hw_mcfp->iq_set[instance];

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	memcpy((void *)iq_set->regs, (void *)regs, sizeof(struct cr_set) * regs_size);

	spin_unlock_irqrestore(&iq_set->slock, flag);
	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "Store IQ regs set: %p, size(%d)\n", instance, hw_ip, regs, regs_size);

	return 0;
}

int is_hw_mcfp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_mcfp *hw_mcfp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		mcfp_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = MCFP_RDMA_CUR_IN_Y; i < MCFP_RDMA_MAX; i++) {
			hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
			dma = &hw_mcfp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = MCFP_WDMA_PREV_OUT_Y; i < MCFP_WDMA_MAX; i++) {
			hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
			dma = &hw_mcfp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_mcfp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_mcfp *hw_mcfp;
	struct is_mcfp_config *mcfp_config = (struct is_mcfp_config *)conf;

	FIMC_BUG(!conf);

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;

	if (hw_mcfp->config[instance].geomatch_en != mcfp_config->geomatch_en)
		msinfo_hw("[F:%d] MCFP geomatch enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].geomatch_en,
			mcfp_config->geomatch_en);

	if (hw_mcfp->config[instance].mixer_enable != mcfp_config->mixer_enable)
		msinfo_hw("[F:%d] MCFP mixer enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].mixer_enable,
			mcfp_config->mixer_enable);

	if (hw_mcfp->config[instance].still_en != mcfp_config->still_en)
		msinfo_hw("[F:%d] MCFP still enable: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].still_en,
			mcfp_config->still_en);

	if (hw_mcfp->config[instance].mixer_mode != mcfp_config->mixer_mode)
		msinfo_hw("[F:%d] MCFP mode changed: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].mixer_mode,
			mcfp_config->mixer_mode);

	if (hw_mcfp->config[instance].geomatch_bypass != mcfp_config->geomatch_bypass)
		msinfo_hw("[F:%d] MCFP geomatch bypass: %d -> %d", instance, hw_ip, fcount,
			hw_mcfp->config[instance].geomatch_bypass,
			mcfp_config->geomatch_bypass);

	if (!mcfp_config->still_en) {
		if (hw_mcfp->config[instance].img_bit != mcfp_config->img_bit)
			msinfo_hw("[F:%d] MCFP img bit: %d -> %d", instance, hw_ip, fcount,
				hw_mcfp->config[instance].img_bit,
				mcfp_config->img_bit);
	} else {
		mcfp_config->img_bit = 12;
	}

	if (mcfp_config->img_bit != 8 && mcfp_config->img_bit != 10 && mcfp_config->img_bit != 12) {
		mswarn_hw("[F:%d] img_bit(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->img_bit);
		mcfp_config->img_bit = 12;
	}

	if (!mcfp_config->still_en) {
		if (hw_mcfp->config[instance].wgt_bit != mcfp_config->wgt_bit)
			msinfo_hw("[F:%d] MCFP wgt bit: %d -> %d", instance, hw_ip, fcount,
				hw_mcfp->config[instance].wgt_bit,
				mcfp_config->wgt_bit);
	} else {
		mcfp_config->wgt_bit = 8;
	}

	if (mcfp_config->wgt_bit != 4 && mcfp_config->wgt_bit != 8) {
		mswarn_hw("[F:%d] wgt_bit(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->wgt_bit);
		mcfp_config->wgt_bit = 8;
	}

#if (DDK_INTERFACE_VER == 0x1010)
	if (hw_mcfp->config[instance].motion_scale_x != mcfp_config->motion_scale_x ||
	    hw_mcfp->config[instance].motion_scale_y != mcfp_config->motion_scale_y)
		msinfo_hw("[F:%d] MCFP motion_scale: (%d x %d) -> (%d x %d)",
			instance, hw_ip, fcount,
			hw_mcfp->config[instance].motion_scale_x,
			hw_mcfp->config[instance].motion_scale_y,
			mcfp_config->motion_scale_x,
			mcfp_config->motion_scale_y);

	if (mcfp_config->motion_scale_x > 2) {
		mswarn_hw("[F:%d] motion_scale_x(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->motion_scale_x);
		mcfp_config->motion_scale_x = 0;
	}

	if (mcfp_config->motion_scale_y > 3) {
		mswarn_hw("[F:%d] motion_scale_y(%d) is not valid", instance, hw_ip, fcount,
			mcfp_config->motion_scale_y);
		mcfp_config->motion_scale_y = 0;
	}
#endif

	msdbg_hw(2, "[F:%d] skip_wdma(%d)\n", instance, hw_ip, fcount, mcfp_config->skip_wdma);

	memcpy(&hw_mcfp->config[instance], conf, sizeof(struct is_mcfp_config));

	return 0;
}

static int is_hw_mcfp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mcfp *hw_mcfp;

	hw_mcfp = (struct is_hw_mcfp *)hw_ip->priv_info;
	if (!hw_mcfp) {
		mserr_hw("failed to get HW MCFP", instance, hw_ip);
		return -ENODEV;
	}

	mcfp_hw_dump(hw_ip->regs[REG_SETA]);

	return is_lib_notify_timeout(&hw_mcfp->lib[instance], instance);
}

const struct is_hw_ip_ops is_hw_mcfp_ops = {
	.open			= is_hw_mcfp_open,
	.init			= is_hw_mcfp_init,
	.deinit			= is_hw_mcfp_deinit,
	.close			= is_hw_mcfp_close,
	.enable			= is_hw_mcfp_enable,
	.disable		= is_hw_mcfp_disable,
	.shot			= is_hw_mcfp_shot,
	.set_param		= is_hw_mcfp_set_param,
	.get_meta		= is_hw_mcfp_get_meta,
	.get_cap_meta		= is_hw_mcfp_get_cap_meta,
	.frame_ndone		= is_hw_mcfp_frame_ndone,
	.load_setfile		= is_hw_mcfp_load_setfile,
	.apply_setfile		= is_hw_mcfp_apply_setfile,
	.delete_setfile		= is_hw_mcfp_delete_setfile,
	.restore		= is_hw_mcfp_restore,
	.set_regs		= is_hw_mcfp_set_regs,
	.dump_regs		= is_hw_mcfp_dump_regs,
	.set_config		= is_hw_mcfp_set_config,
	.notify_timeout		= is_hw_mcfp_notify_timeout,
};

int is_hw_mcfp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops = &is_hw_mcfp_ops;

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_mcfp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_mcfp_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
