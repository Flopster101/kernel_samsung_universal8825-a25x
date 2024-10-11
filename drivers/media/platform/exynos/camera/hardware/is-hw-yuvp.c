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

#include "is-hw-yuvp.h"
#include "is-err.h"
#include "api/is-hw-api-yuvp-v1_0.h"
#include "api/is-hw-api-drcp-v1_0.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"

static int is_hw_yuvp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param *param;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount, strip_index;
	bool f_err = false;
	int hw_slot;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_yuvp->strip_index);
	param = &hw_ip->region[instance]->parameter.yuvp;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return 0;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];
	status = yuvp_hw_g_int_state0(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[YUVP0_INTR])
			& yuvp_hw_g_int_mask0(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVP0 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (yuvp_hw_is_occured0(status, INTR_FRAME_START) && yuvp_hw_is_occured0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	f_err = yuvp_hw_is_occured0(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP0][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		yuvp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int is_hw_yuvp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param *param;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount, strip_index;
	bool f_err;
	int hw_slot;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_yuvp->strip_index);
	param = &hw_ip->region[instance]->parameter.yuvp;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return 0;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];
	status = yuvp_hw_g_int_state1(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[YUVP1_INTR])
			& yuvp_hw_g_int_mask1(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "YUVP1 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	f_err = yuvp_hw_is_occured1(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP1][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		yuvp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int is_hw_drcp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount, strip_index, strip_total_count;
	bool f_err;
	int hw_slot;
	u32 cur_idx, batch_num;
	bool is_first_shot, is_last_shot;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_yuvp->param_set[instance];
	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return 0;
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

	status = drcp_hw_g_int_state0(hw_ip->regs[REG_EXT1], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[DRCP0_INTR])
			& drcp_hw_g_int_mask0(hw_ip->regs[REG_EXT1]);

	msdbg_hw(2, "DRCP0 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (drcp_hw_is_occured0(status, INTR_FRAME_START) && drcp_hw_is_occured0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (drcp_hw_is_occured0(status, INTR_FRAME_START)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_yuvp->lib[instance], instance, hw_fcount,
				YUVP_EVENT_FRAME_START, strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			is_hardware_frame_start(hw_ip, instance);
		}
	}

	if (drcp_hw_is_occured0(status, INTR_FRAME_END)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_yuvp->lib[instance], instance, hw_fcount,
				YUVP_EVENT_FRAME_END, strip_index, NULL);
		} else {
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

			if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
				mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
						atomic_read(&hw_ip->count.fs),
						atomic_read(&hw_ip->count.fe),
						atomic_read(&hw_ip->count.dma), status);
			}
			wake_up(&hw_ip->status.wait_queue);
		}
		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP))) {
			yuvp_hw_print_chain_debug_cnt(hw_ip->regs[REG_SETA]);
			drcp_hw_print_chain_debug_cnt(hw_ip->regs[REG_EXT1]);
		}
	}

	f_err = drcp_hw_is_occured0(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[DRCP0][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		drcp_hw_dump(hw_ip->regs[REG_EXT1]);
	}

	return 0;
}

static int is_hw_drcp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param *param;
	struct is_interface_hwip *itf_hw;
	u32 status, instance, hw_fcount, strip_index;
	bool f_err;
	int hw_slot;

	hw_ip = (struct is_hw_ip *)context;
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_yuvp->strip_index);
	param = &hw_ip->region[instance]->parameter.yuvp;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return 0;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];
	status = drcp_hw_g_int_state1(hw_ip->regs[REG_EXT1], true,
			(hw_ip->num_buffers & 0xffff), &hw_yuvp->irq_state[DRCP1_INTR])
			& drcp_hw_g_int_mask1(hw_ip->regs[REG_EXT1]);

	msdbg_hw(2, "DRCP1 interrupt status(0x%x)\n", instance, hw_ip, status);

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

	f_err = drcp_hw_is_occured1(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[DRCP1][F:%d] Ocurred error interrupt : status(0x%x)\n",
			instance, hw_ip, hw_fcount, status);
		drcp_hw_dump(hw_ip->regs[REG_EXT1]);
	}

	return 0;
}

static int __is_hw_yuvp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp)
{
	int i, ret;

	sdbg_hw(4, "[HW] %s\n", hw_ip, __func__);
	for (i = 0; i < COREX_MAX; i++) {
		hw_yuvp->cur_hw_iq_set[i].size_yuvp = 0;
		hw_yuvp->cur_hw_iq_set[i].size_drcp = 0;
	}

	ret = drcp_hw_s_reset(hw_ip->regs[REG_EXT1]);
	if (ret)
		return ret;

	return yuvp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_yuvp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	msinfo_hw("reset\n", instance, hw_ip);

	if (__is_hw_yuvp_s_reset(hw_ip, hw_yuvp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	/* temp corex disable*/
	yuvp_hw_s_corex_init(hw_ip->regs[REG_SETA], 0);

	drcp_hw_s_init(hw_ip->regs[REG_EXT1]);
	yuvp_hw_s_init(hw_ip->regs[REG_SETA]);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_yuvp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (__is_hw_yuvp_s_reset(hw_ip, hw_yuvp)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	if (drcp_hw_wait_idle(hw_ip->regs[REG_EXT1]))
		mserr_hw("failed to drcp_hw_wait_idle", instance, hw_ip);


	if (yuvp_hw_wait_idle(hw_ip->regs[REG_SETA]))
		mserr_hw("failed to yuvp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished yuvp\n", instance, hw_ip);

	return 0;
}

static int is_hw_yuvp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	/*
	 * add to command queue
	 * 1. set corex_update_type_0_setX
	 * normal shot: SETA(wide), SETB(tele, front), SETC(uw)
	 * reprocessing shot: SETD
	 * 2. set cq_frm_start_type to 0. only at first time?
	 * 3. set ms_add_to_queue(set id, frame id).
	 */
	int ret;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	ret = yuvp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id);
	if (ret)
		return ret;

	drcp_hw_s_cmdq(hw_ip->regs[REG_EXT1], set_id);
	yuvp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __is_hw_yuvp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame, u32 set_id)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_device_ischain *device;
	struct is_group *group;
	struct is_hardware *hardware;
	struct yuvp_param_set *param_set;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("failed to get yuvp", instance, hw_ip);
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

	param_set = &hw_yuvp->param_set[instance];

	/* yuvp context setting */
	drcp_hw_s_core(hw_ip->regs[REG_EXT1], frame->num_buffers, set_id);
	yuvp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id);

	return 0;
}

static void is_hw_yuvp_free_iqset(struct is_hw_ip *hw_ip)
{
	struct is_hw_yuvp *hw = (struct is_hw_yuvp *)hw_ip->priv_info;
	u32 set_i;

	for (set_i = 0; set_i < COREX_MAX; set_i++) {
		if (hw->cur_hw_iq_set[set_i].regs_yuvp) {
			vfree(hw->cur_hw_iq_set[set_i].regs_yuvp);
			hw->cur_hw_iq_set[set_i].regs_yuvp = NULL;
		}

		if (hw->cur_hw_iq_set[set_i].regs_drcp) {
			vfree(hw->cur_hw_iq_set[set_i].regs_drcp);
			hw->cur_hw_iq_set[set_i].regs_drcp = NULL;
		}
	}

	for (set_i = 0; set_i < IS_STREAM_COUNT; set_i++) {
		if (hw->iq_set[set_i].regs_yuvp) {
			vfree(hw->iq_set[set_i].regs_yuvp);
			hw->iq_set[set_i].regs_yuvp = NULL;
		}

		if (hw->iq_set[set_i].regs_drcp) {
			vfree(hw->iq_set[set_i].regs_drcp);
			hw->iq_set[set_i].regs_drcp = NULL;
		}
	}
}

static int is_hw_yuvp_alloc_iqset(struct is_hw_ip *hw_ip)
{
	int ret;
	struct is_hw_yuvp *hw = (struct is_hw_yuvp *)hw_ip->priv_info;
	u32 instance = hw->instance;
	u32 yuvp_reg_cnt = yuvp_hw_g_reg_cnt();
	u32 drcp_reg_cnt = drcp_hw_g_reg_cnt();
	u32 set_i;

	for (set_i = 0; set_i < IS_STREAM_COUNT; set_i++) {
		hw->iq_set[set_i].regs_yuvp = vzalloc(sizeof(struct cr_set) * yuvp_reg_cnt);
		if (!hw->iq_set[set_i].regs_yuvp) {
			mserr_hw("failed to alloc iq_set[%d].regs_yuvp", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}

		hw->iq_set[set_i].regs_drcp = vzalloc(sizeof(struct cr_set) * drcp_reg_cnt);
		if (!hw->iq_set[set_i].regs_drcp) {
			mserr_hw("failed to alloc iq_set[%d].regs_drcp", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	for (set_i = 0; set_i < COREX_MAX; set_i++) {
		hw->cur_hw_iq_set[set_i].regs_yuvp = vzalloc(sizeof(struct cr_set) * yuvp_reg_cnt);
		if (!hw->cur_hw_iq_set[set_i].regs_yuvp) {
			mserr_hw("failed to alloc cur_hw_iq_set[%d].regs_yuvp", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}

		hw->cur_hw_iq_set[set_i].regs_drcp = vzalloc(sizeof(struct cr_set) * drcp_reg_cnt);
		if (!hw->cur_hw_iq_set[set_i].regs_drcp) {
			mserr_hw("failed to alloc cur_hw_iq_set[%d].regs_drcp", instance, hw_ip, set_i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	return 0;

err_alloc:
	is_hw_yuvp_free_iqset(hw_ip);

	return ret;
}

static int __nocfi is_hw_yuvp_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int i,ret;
	struct is_hw_yuvp *hw_yuvp;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWYUVP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_yuvp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("failed to get hw_yuvp", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_yuvp->instance = instance;

	is_fpsimd_get_func();
	ret = get_lib_func(LIB_FUNC_YUVP, (void **)&hw_yuvp->lib_func);
	is_fpsimd_put_func();

	if (hw_yuvp->lib_func == NULL) {
		mserr_hw("hw_yuvp->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_yuvp->lib_support = is_get_lib_support();

	hw_yuvp->lib[instance].func = hw_yuvp->lib_func;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_yuvp->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	if (is_hw_yuvp_alloc_iqset(hw_ip))
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_yuvp->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_yuvp->iq_set[i].state);
		clear_bit(CR_SET_CONFIG_EXT1, &hw_yuvp->iq_set[i].state);
		set_bit(CR_SET_EMPTY_EXT1, &hw_yuvp->iq_set[i].state);
		spin_lock_init(&hw_yuvp->iq_set[i].slock);
	}

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_yuvp->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_yuvp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;
	struct is_device_ischain *device;
	u32 input_id, f_type;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("hw_yuvp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}
	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	hw_yuvp->lib[instance].object = NULL;
	hw_yuvp->lib[instance].func   = hw_yuvp->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_yuvp->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip, &hw_yuvp->lib[instance],
					instance, (u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	for (input_id = YUVP_RDMA_Y; input_id < DRCP_RDMA_DRC0; input_id++) {
		ret = yuvp_hw_dma_create(&hw_yuvp->rdma[input_id], hw_ip->regs[REG_SETA], input_id);
		if (ret) {
			mserr_hw("yuvp_hw_dma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}
	for (input_id = DRCP_RDMA_DRC0; input_id < DRCP_DMA_MAX; input_id++) {
		ret = drcp_hw_dma_create(&hw_yuvp->rdma[input_id], hw_ip->regs[REG_EXT1], input_id);
		if (ret) {
			mserr_hw("drcp_hw_dma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_yuvp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_yuvp->lib[instance], instance);
		hw_yuvp->lib[instance].object = NULL;
	}

	return 0;
}

static int is_hw_yuvp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_yuvp->lib[instance], instance);

	__is_hw_yuvp_clear_common(hw_ip, instance);

	is_hw_yuvp_free_iqset(hw_ip);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_yuvp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;

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

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	__is_hw_yuvp_s_common_reg(hw_ip, instance);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_yuvp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("yuvp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_yuvp->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(YPP_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_yuvp->lib[instance], instance, 1);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_yuvp_set_rdma_cmd(struct yuvp_param_set *param_set, u32 instance,
	struct is_yuvp_config *conf)
{
	mdbg_hw(4, "%s\n", instance, __func__);

	if (conf->clahe_bypass & 0x1)
		param_set->dma_input_svhist.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (conf->drc_bypass & 0x1)
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_DISABLE;

	/* disable seg dma until interface is implemented */
	param_set->dma_input_seg.cmd = DMA_INPUT_COMMAND_DISABLE;

	return 0;
}

static int __is_hw_yuvp_get_input_dva(u32 id, u32 *cmd, u32 **input_dva, struct yuvp_param_set *param_set,
	struct is_hw_ip *hw_ip, u32 set_id, u32 *grid_x_num, u32 *grid_y_num, u32 *grid_size_x, u32 *grid_size_y)
{
	switch (id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_UV:
		*input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	case YUVP_RDMA_SEG0:
	case YUVP_RDMA_SEG1:
		*input_dva = param_set->input_dva_seg;
		*cmd = param_set->dma_input_seg.cmd;
		break;
	case DRCP_RDMA_DRC0:
	case DRCP_RDMA_DRC1:
	case DRCP_RDMA_DRC2:
		*input_dva = param_set->input_dva_drc;
		*cmd = param_set->dma_input_drc.cmd;
		break;
	case DRCP_RDMA_CLAHE:
		*input_dva = param_set->input_dva_svhist;
		*cmd = param_set->dma_input_svhist.cmd;
		break;
	case DRCP_WDMA_Y:
	case DRCP_WDMA_UV:
		*input_dva = param_set->output_dva_yuv;
		*cmd = param_set->dma_output_yuv.cmd;
		break;
	default:
		return -1;
	}
	return 0;
}

static int __is_hw_yuvp_get_cache_hint(u32 id, u32 *cache_hint)
{
	switch (id) {
		case YUVP_RDMA_Y:
		case YUVP_RDMA_UV:
		case YUVP_RDMA_SEG0:
		case YUVP_RDMA_SEG1:
		case DRCP_RDMA_CLAHE:
		case DRCP_RDMA_DRC0:
		case DRCP_RDMA_DRC1:
		case DRCP_RDMA_DRC2:
		case DRCP_WDMA_Y:
		case DRCP_WDMA_UV:
			*cache_hint = 0x7;	// 111: last-access-read
			return 0;
	default:
		return -1;
	}
}

static int __is_hw_yuvp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp,
			struct yuvp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 grid_x_num = 0, grid_y_num = 0;
	u32 grid_size_x = 0, grid_size_y = 0;
	u32 in_crop_size_x = 0;
	u32 cache_hint;
	int ret;

	if (__is_hw_yuvp_get_input_dva(id, &cmd, &input_dva, param_set, hw_ip, set_id,
		&grid_x_num, &grid_y_num, &grid_size_x, &grid_size_y) < 0) {
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}

	if (__is_hw_yuvp_get_cache_hint(id, &cache_hint) < 0) {
		merr_hw("invalid ID (%d)", instance,  id);
		return -EINVAL;
	}


	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_yuvp->rdma[id].name, cmd);

	yuvp_hw_s_rdma_corex_id(&hw_yuvp->rdma[id], set_id);

	ret = yuvp_hw_s_rdma_init(&hw_yuvp->rdma[id], param_set, cmd,
			hw_yuvp->config.vhist_grid_num, hw_yuvp->config.drc_grid_w, hw_yuvp->config.drc_grid_h,
			in_crop_size_x, cache_hint, &comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = yuvp_hw_s_rdma_addr(&hw_yuvp->rdma[id], input_dva, 0,
			(hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_yuvp_check_size(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	msdbgs_hw(2, "hw_yuvp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		input->format, input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip,
		output->cmd, output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "[%d]<<< hw_yuvp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_yuvp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	sdbg_hw(4, "%s\n", hw_ip, __func__);

	yuvp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);
	drcp_hw_s_block_bypass(hw_ip->regs[REG_EXT1], set_id);

	if (IS_ENABLED(YPP_UNIT_TEST))
		yuvp_hw_s_dtp_pattern(hw_ip->regs[REG_SETA], 0, YUVP_DTP_PATTERN_COLORBAR);

	drcp_hw_s_yuv444to422_coeff(hw_ip->regs[REG_EXT1], set_id);
	drcp_hw_s_yuvtorgb_coeff(hw_ip->regs[REG_EXT1], set_id);

	return 0;
}


static int __is_hw_yuvp_update_block_reg(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	if (hw_yuvp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_yuvp->instance);
		hw_yuvp->instance = instance;
	}

	__is_hw_yuvp_check_size(hw_ip, param_set, instance);
	return __is_hw_yuvp_bypass(hw_ip, set_id);
}

static void __is_hw_yuvp_update_param(struct is_hw_ip *hw_ip, struct is_region *region,\
	struct yuvp_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct yuvp_param *param;

	param = &region->parameter.yuvp;
	param_set->instance_id = instance;

	/* check input */
	if (test_bit(PARAM_YUVP_OTF_INPUT, pmap)) {
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DMA_INPUT, pmap)) {
		memcpy(&param_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DMA_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DRC, pmap)) {
		memcpy(&param_set->dma_input_drc, &param->drc,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DRC\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_SVHIST, pmap)) {
		memcpy(&param_set->dma_input_svhist, &param->svhist,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_SVHIST\n", instance, hw_ip, __func__);
	}

	/* check output */
	if (test_bit(PARAM_YUVP_OTF_OUTPUT, pmap)) {
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_YUV, pmap)) {
		memcpy(&param_set->dma_output_yuv, &param->yuv,
			sizeof(struct param_dma_output));
		msdbg_hw(4, "%s : PARAM_YUVP_DMA_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_STRIPE_INPUT, pmap)) {
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
		msdbg_hw(4, "%s : PARAM_YUVP_STRIPE_INPUT\n", instance, hw_ip, __func__);
	}
}

static int is_hw_yuvp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hw_yuvp->instance = IS_STREAM_COUNT;

	param_set = &hw_yuvp->param_set[instance];
	__is_hw_yuvp_update_param(hw_ip, region, param_set, pmap, instance);

	return 0;
}

static int __is_hw_yuvp_set_rta_regs_yuvp(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_yuvp_iq *iq_set;
	struct is_hw_yuvp_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i;
	bool configured = false;

	mdbg_hw(4, "%s\n", instance, __func__);

	iq_set = &hw_yuvp->iq_set[instance];
	cur_iq_set = &hw_yuvp->cur_hw_iq_set[set_id];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs_yuvp;
		regs_size = iq_set->size_yuvp;
		if (cur_iq_set->size_yuvp != regs_size ||
			memcmp(cur_iq_set->regs_yuvp, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel_relaxed(regs[i].reg_data,
						base + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(cur_iq_set->regs_yuvp, regs, sizeof(struct cr_set) * regs_size);
			cur_iq_set->size_yuvp = regs_size;
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

static int __is_hw_yuvp_set_rta_regs_drcp(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_EXT1];
	struct is_hw_yuvp_iq *iq_set;
	struct is_hw_yuvp_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i;
	bool configured = false;

	mdbg_hw(4, "%s\n", instance, __func__);

	iq_set = &hw_yuvp->iq_set[instance];
	cur_iq_set = &hw_yuvp->cur_hw_iq_set[set_id];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG_EXT1, &iq_set->state)) {
		regs = iq_set->regs_drcp;
		regs_size = iq_set->size_drcp;
		if (cur_iq_set->size_drcp != regs_size ||
			memcmp(cur_iq_set->regs_drcp, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel_relaxed(regs[i].reg_data,
						base + GET_COREX_OFFSET(set_id) + regs[i].reg_addr);

			memcpy(cur_iq_set->regs_drcp, regs, sizeof(struct cr_set) * regs_size);
			cur_iq_set->size_drcp = regs_size;
		}

		set_bit(CR_SET_EMPTY_EXT1, &iq_set->state);
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

static int __is_hw_yuvp_set_size_regs(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 instance, u32 yuvpp_strip_start_pos, const struct is_frame *frame, u32 set_id,
	enum yuvp_input_path_type input_path, enum drcp_demux_dither_type output_path)
{
	struct is_device_ischain *device;
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *config;
	struct is_param_region *param_region;
	int ret;
	u32 strip_enable;
	u32 width, height;
	u32 frame_width;
	u32 region_id;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 taa_crop_x, taa_crop_y;
	u32 taa_crop_width, taa_crop_height;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	config = &hw_yuvp->config;

	device = hw_ip->group[instance]->device;
	param_region = &device->is_region->parameter;
	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	region_id = param_set->stripe_input.index;

	if (input_path == YUVP_INPUT_RDMA) {
		width = param_set->dma_input.width;
		height = param_set->dma_input.height;
	} else {
		width = param_set->otf_input.width;
		height = param_set->otf_input.height;
	}
	msdbg_hw(4, "%s : path %d, (w x h) = (%d x %d)\n", instance, hw_ip, __func__, input_path, width, height);

	frame_width = (strip_enable) ? param_set->stripe_input.full_width : width;

	sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	taa_crop_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	taa_crop_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	taa_crop_width = frame->shot->udm.frame_info.taa_in_crop_region[2];
	taa_crop_height = frame->shot->udm.frame_info.taa_in_crop_region[3];
	sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];

	ret = yuvp_hw_s_nsmix_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for yuvp nsmix", instance, hw_ip);
		goto err;
	}

	ret = yuvp_hw_s_yuvnr_size(hw_ip->regs[REG_SETA], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for yuvnr", instance, hw_ip);
		goto err;
	}

	ret = drcp_hw_s_drc_size(hw_ip->regs[REG_EXT1], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for drc", instance, hw_ip);
		goto err;
	}

	ret = drcp_hw_s_clahe_size(hw_ip->regs[REG_EXT1], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for clahe", instance, hw_ip);
		goto err;
	}

	ret = drcp_hw_s_sharpadder_size(hw_ip->regs[REG_EXT1], set_id, yuvpp_strip_start_pos, frame_width,
		width, height, strip_enable,
		sensor_full_width, sensor_full_height, sensor_binning_x, sensor_binning_y, sensor_crop_x, sensor_crop_y,
		taa_crop_x, taa_crop_y, taa_crop_width, taa_crop_height);
	if (ret) {
		mserr_hw("failed to set size regs for sharp adder", instance, hw_ip);
		goto err;
	}

	yuvp_hw_s_size(hw_ip->regs[REG_SETA], set_id, width, height, strip_enable);
	drcp_hw_s_size(hw_ip->regs[REG_EXT1], set_id, width, height, strip_enable);
err:
	return ret;
}

static void __is_hw_yuvp_config_path(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 set_id, enum yuvp_input_path_type *input_path, enum drcp_demux_dither_type *output_path)
{
	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*input_path = YUVP_INPUT_RDMA;
		yuvp_hw_s_mux_dtp(hw_ip->regs[REG_SETA], set_id, YUVP_MUX_DTP_RDMA_YUV);
	} else {
		*input_path = YUVP_INPUT_OTF;
		yuvp_hw_s_cinfifo(hw_ip->regs[REG_SETA], set_id);
		yuvp_hw_s_mux_dtp(hw_ip->regs[REG_SETA], set_id, YUVP_MUX_DTP_CINFIFO);
	}
	yuvp_hw_s_input_path(hw_ip->regs[REG_SETA], set_id, *input_path);

	*output_path = 0;

	if (param_set->dma_output_yuv.cmd == DMA_INPUT_COMMAND_ENABLE)
		*output_path |= DRCP_DEMUX_DITHER_WDMA;


	if (param_set->otf_output.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*output_path |= DRCP_DEMUX_DITHER_COUTFIFIO;
		drcp_hw_s_coutfifo(hw_ip->regs[REG_EXT1], set_id, 1);
		drcp_hw_s_output_path(hw_ip->regs[REG_EXT1], set_id, 1);
	}
	else
		drcp_hw_s_output_path(hw_ip->regs[REG_EXT1], set_id, 0);

	drcp_hw_s_demux_dither(hw_ip->regs[REG_EXT1], set_id, *output_path);
}

static void __is_hw_yuvp_config_dma(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	struct is_frame *frame, u32 fcount, u32 instance)
{
	struct param_dma_output *dma_output;
	struct param_dma_input *dma_input;
	u32 cur_idx;
	u32 cmd_input, cmd_input_drc, cmd_input_hist;
	int batch_num;

	dma_input = is_itf_g_param(hw_ip->group[instance]->device, frame, PARAM_YUVP_DMA_INPUT);
	dma_output = is_itf_g_param(hw_ip->group[instance]->device, frame, PARAM_YUVP_YUV);
	cur_idx = frame->cur_buf_index;

	cmd_input = is_hardware_dma_cfg("yuvp", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			frame->dvaddr_buffer);

	cmd_input = is_hardware_dma_cfg_32bit("yuvpout", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_yuv.cmd,
			param_set->dma_output_yuv.plane,
			param_set->output_dva_yuv,
			frame->ypnrdsTargetAddress);

	cmd_input_drc = is_hardware_dma_cfg_32bit("ypdga", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_drc.cmd,
			param_set->dma_input_drc.plane,
			param_set->input_dva_drc,
			frame->ypdgaTargetAddress);

	cmd_input_hist = is_hardware_dma_cfg_32bit("ypsvhist", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input_svhist.cmd,
			param_set->dma_input_svhist.plane,
			param_set->input_dva_svhist,
			frame->ypsvhistTargetAddress);

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

}

static int is_hw_yuvp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_region *region;
	struct yuvp_param_set *param_set;
	struct is_frame *dma_frame;
	u32 fcount, instance;
	u32 cur_idx, batch_num;
	u32 strip_start_pos;
	u32 set_id;
	int ret, i;
	u32 strip_index, strip_total_count;
	enum yuvp_input_path_type input_path;
	enum drcp_demux_dither_type output_path;
	bool do_blk_cfg = true;
	ulong debug_iq = is_get_debug_param(IS_DEBUG_PARAM_IQ);
	u32 cmd, bypass;
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

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	param_set = &hw_yuvp->param_set[instance];
	region = hw_ip->region[instance];

	atomic_set(&hw_yuvp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

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

	cur_idx = frame->cur_buf_index;

	__is_hw_yuvp_update_param(hw_ip, region, param_set, frame->pmap, instance);
	__is_hw_yuvp_config_dma(hw_ip, param_set, frame, fcount, instance);

	cur_idx = (hw_ip->num_buffers >> CURR_INDEX_SHIFT) & 0xff;
	batch_num = (hw_ip->num_buffers >> SW_FRO_NUM_SHIFT) & 0xff;

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	strip_start_pos = (strip_index) ? (param_set->stripe_input.start_pos_x) : 0;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (!batch_num && !strip_total_count) {
			/* normal shot */
			skip_isp_shot = false;
		} else {
			/* SW batch mode or Stripe processing */
			if ((batch_num && !cur_idx) || (strip_total_count && !strip_index))
				skip_isp_shot = false;
			else
				skip_isp_shot = true;
		}
	} else {
		skip_isp_shot = false;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		if (frame->shot) {
			if (!skip_isp_shot) {
				ret = is_lib_isp_set_ctrl(hw_ip, &hw_yuvp->lib[instance], frame);
				if (ret)
					mserr_hw("set_ctrl fail", instance, hw_ip);
			}
		}
	}

	/* temp direct set*/
	set_id = COREX_DIRECT;

	__is_hw_yuvp_config_path(hw_ip, param_set, set_id, &input_path, &output_path);
	ret = __is_hw_yuvp_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_yuvp_init_config is fail", instance, hw_ip);
		return ret;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_shot(hw_ip, &hw_yuvp->lib[instance], param_set,
						frame->shot);
			if (ret)
				return ret;

			if (likely(!test_bit(hw_ip->id, &debug_iq))) {
				_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
				ret = __is_hw_yuvp_set_rta_regs_yuvp(hw_ip, set_id, instance);
				_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_X);

				if (ret)
					mserr_hw("__is_hw_yuvp_set_rta_regs_yuvp is fail",
							instance, hw_ip);
				else
					do_blk_cfg = false;

				ret = __is_hw_yuvp_set_rta_regs_drcp(hw_ip, set_id, instance);

				if (ret)
					mserr_hw("__is_hw_yuvp_set_rta_regs_drcp is fail",
							instance, hw_ip);
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

	if (unlikely(do_blk_cfg))
		__is_hw_yuvp_update_block_reg(hw_ip, param_set, instance, set_id);

	__is_hw_yuvp_set_rdma_cmd(param_set, instance, &hw_yuvp->config);

	ret = __is_hw_yuvp_set_size_regs(hw_ip, param_set, instance, strip_start_pos, frame,
					 set_id, input_path, output_path);
	if (ret) {
		mserr_hw("__is_hw_yuvp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = YUVP_RDMA_Y; i < DRCP_DMA_MAX; i++) {
		ret = __is_hw_yuvp_set_rdma(hw_ip, hw_yuvp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_yuvp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	cmd = param_set->dma_input_svhist.cmd;
	bypass = hw_yuvp->config.clahe_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass) {
		mserr_hw("clahe rdma setting mismatched : rdma %d, bypass %d\n",
			instance, hw_ip, cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	cmd = param_set->dma_input_drc.cmd;
	bypass = hw_yuvp->config.drc_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass) {
		mserr_hw("drc rdma setting mismatched : rdma %d, bypass %d\n",
			instance, hw_ip, cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	ret = is_hw_yuvp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_yuvp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP))) {
		yuvp_hw_dump(hw_ip->regs[REG_SETA]);
		drcp_hw_dump(hw_ip->regs[REG_EXT1]);
	}
	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_yuvp->lib[instance], instance);

	return ret;
}

static int is_hw_yuvp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (unlikely(!hw_yuvp)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_yuvp->lib[frame->instance], frame);
		if (ret)
			mserr_hw("get_meta fail", frame->instance, hw_ip);
	}

	return ret;
}


static int is_hw_yuvp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, -1,
			IS_HW_CORE_END, done_type, false);
	}

	return ret;
}


static int is_hw_yuvp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret, flag;
	ulong addr;
	u32 size, index;
	struct is_hw_yuvp *hw_yuvp;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

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
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
				setfile->using_count);
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_yuvp->lib[instance],
				addr, size, index, flag, instance);

			set_bit(index, &hw_yuvp->lib[instance].tune_count);
		}
	}
	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_yuvp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret;
	u32 setfile_index;
	struct is_hw_yuvp *hw_yuvp;
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

	FIMC_BUG(!hw_ip->priv_info);
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_yuvp->lib[instance], setfile_index,
			instance, scenario);

	return ret;
}

static int is_hw_yuvp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;
	int ret = 0;
	int i;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

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
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_yuvp->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_yuvp->lib[instance],
					(u32)i, instance);
				clear_bit(i, &hw_yuvp->lib[instance].tune_count);
			}
		}
	}
	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_yuvp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;
	struct is_group *group;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;

	msinfo_hw("hw_yuvp_reset\n", instance, hw_ip);

	if (__is_hw_yuvp_s_common_reg(hw_ip, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	group = hw_ip->group[instance];

	if (IS_ENABLED(YPP_DDK_LIB_CALL)) {
		ret = is_lib_isp_reset_recovery(hw_ip, &hw_yuvp->lib[instance], instance);
		if (ret) {
			mserr_hw("is_lib_yuvp_reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
		}
	}

	return ret;
}

static int is_hw_yuvp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_hw_yuvp_iq *iq_set;
	ulong flag;

	dbg_yuvp(4, "[HW] %s\n", __func__);
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	iq_set = &hw_yuvp->iq_set[instance];

	if (chain_id == 0) {
		if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
			return -EBUSY;

		spin_lock_irqsave(&iq_set->slock, flag);

		iq_set->size_yuvp = regs_size;
		memcpy((void *)iq_set->regs_yuvp, (void *)regs, sizeof(struct cr_set) * regs_size);

		spin_unlock_irqrestore(&iq_set->slock, flag);
		set_bit(CR_SET_CONFIG, &iq_set->state);
	} else if (chain_id == EXT1_CHAIN_OFFSET) {
		if (!test_and_clear_bit(CR_SET_EMPTY_EXT1, &iq_set->state))
			return -EBUSY;

		spin_lock_irqsave(&iq_set->slock, flag);

		iq_set->size_drcp= regs_size;
		memcpy((void *)iq_set->regs_drcp, (void *)regs, sizeof(struct cr_set) * regs_size);

		spin_unlock_irqrestore(&iq_set->slock, flag);
		set_bit(CR_SET_CONFIG_EXT1, &iq_set->state);
	}

	msdbg_hw(2, "Store IQ regs set: %p, chain: %d, size(%d)\n", instance, hw_ip, regs, chain_id, regs_size);

	return 0;
}

int is_hw_yuvp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size,
	enum is_reg_dump_type dump_type)
{
	void __iomem *base;

	if (dump_type != IS_REG_DUMP_TO_LOG)
		return -EINVAL;

	base = hw_ip->regs[REG_SETA];
	yuvp_hw_dump(base);

	base = hw_ip->regs[REG_EXT1];
	drcp_hw_dump(base);

	return 0;
}

static int is_hw_yuvp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *conf = (struct is_yuvp_config *)config;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	hw_yuvp->config = *conf;

	return 0;
}

static int is_hw_yuvp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	hw_yuvp = (struct is_hw_yuvp *)hw_ip->priv_info;
	if (!hw_yuvp) {
		mserr_hw("failed to get HW YUVP", instance, hw_ip);
		return -ENODEV;
	}

	yuvp_hw_dump(hw_ip->regs[REG_SETA]);
	drcp_hw_dump(hw_ip->regs[REG_EXT1]);

	ret = is_lib_notify_timeout(&hw_yuvp->lib[instance], instance);

	return ret;
}

const struct is_hw_ip_ops is_hw_yuvp_ops = {
	.open			= is_hw_yuvp_open,
	.init			= is_hw_yuvp_init,
	.deinit			= is_hw_yuvp_deinit,
	.close			= is_hw_yuvp_close,
	.enable			= is_hw_yuvp_enable,
	.disable		= is_hw_yuvp_disable,
	.shot			= is_hw_yuvp_shot,
	.set_param		= is_hw_yuvp_set_param,
	.get_meta		= is_hw_yuvp_get_meta,
	.frame_ndone		= is_hw_yuvp_frame_ndone,
	.load_setfile		= is_hw_yuvp_load_setfile,
	.apply_setfile		= is_hw_yuvp_apply_setfile,
	.delete_setfile		= is_hw_yuvp_delete_setfile,
	.restore		= is_hw_yuvp_restore,
	.set_regs		= is_hw_yuvp_set_regs,
	.dump_regs		= is_hw_yuvp_dump_regs,
	.set_config		= is_hw_yuvp_set_config,
	.notify_timeout		= is_hw_yuvp_notify_timeout,
};

int is_hw_yuvp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_yuvp_ops;

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_yuvp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_yuvp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].handler = &is_hw_drcp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].id = INTR_HWIP3;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].handler = &is_hw_drcp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].id = INTR_HWIP4;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].valid = true;

	if (IS_ENABLED(YPP_UNIT_TEST))
		is_hw_yuvp_test(hw_ip, itf, itfc, id, name);

	return 0;
}
