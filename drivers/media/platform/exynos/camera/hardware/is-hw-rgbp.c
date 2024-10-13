// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-rgbp.h"
#include "is-err.h"
#include "api/is-hw-api-rgbp-v1_0.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "votf/camerapp-votf.h"

static int debug_rgbp;
module_param(debug_rgbp, int, 0644);

static int is_hw_rgbp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct rgbp_param_set *param_set;
	struct is_interface_hwip *itf_hw = NULL;
	u32 status, instance, hw_fcount, strip_index, strip_total_count, hl = 0, vl = 0;
	bool f_err = false;
	int hw_slot = -1;
	u32 cur_idx, batch_num;
	bool is_first_shot, is_last_shot;

	hw_ip = (struct is_hw_ip *)context;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param_set = &hw_rgbp->param_set[instance];
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

	status = rgbp_hw_g_int0_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_rgbp->irq_state[RGBP_INTR0])
			& rgbp_hw_g_int0_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "RGBP interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_START) && rgbp_hw_is_occurred0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_START)) {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL) && is_first_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_rgbp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fs);
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msdbg_hw(2, "[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			is_hardware_frame_start(hw_ip, instance);
		}
	}

	if (rgbp_hw_is_occurred0(status, INTR_FRAME_END)) {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL) && is_last_shot) {
			is_lib_isp_event_notifier(hw_ip, &hw_rgbp->lib[instance],
					instance, hw_fcount, EVENT_FRAME_END,
					strip_index, NULL);
		} else {
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add((hw_ip->num_buffers & 0xffff), &hw_ip->count.fe);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msdbg_hw(2, "[F:%d]F.E\n", instance, hw_ip, hw_fcount);

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

	f_err = rgbp_hw_is_occurred0(status, INTR_ERR0);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;

}

static int is_hw_rgbp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_interface_hwip *itf_hw = NULL;
	u32 status, instance, hw_fcount, strip_index, hl = 0, vl = 0;
	bool f_err = false;
	int hw_slot = -1;

	hw_ip = (struct is_hw_ip *)context;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_rgbp->strip_index);

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	status = rgbp_hw_g_int1_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff), &hw_rgbp->irq_state[RGBP_INTR1])
			& rgbp_hw_g_int1_mask(hw_ip->regs[REG_SETA]);

	msdbg_hw(2, "RGBP interrupt status1(0x%x)\n", instance, hw_ip, status);

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

	f_err = rgbp_hw_is_occurred1(status, INTR_ERR1);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;

}


static int __is_hw_rgbp_s_reset(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_rgbp->cur_hw_iq_set[i].size = 0;

	return rgbp_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_rgbp_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_rgbp_s_reset(hw_ip, hw_rgbp);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}
		/* TODO : enable corex */
		/* rgbp_hw_s_corex_init(hw_ip->regs[REG_SETA], 0); */
		rgbp_hw_s_init(hw_ip->regs[REG_SETA]);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_rgbp_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	res = __is_hw_rgbp_s_reset(hw_ip, hw_rgbp);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = rgbp_hw_wait_idle(hw_ip->regs[REG_SETA]);

	if (res)
		mserr_hw("failed to rgbp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished rgbp\n", instance, hw_ip);

	return res;
}

static int is_hw_rgbp_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
{
	/*
	 * add to command queue
	 * 1. set corex_update_type_0_setX
	 * normal shot: SETA(wide), SETB(tele, front), SETC(uw)
	 * reprocessing shot: SETD
	 * 2. set cq_frm_start_type to 0. only at first time?
	 * 3. set ms_add_to_queue(set id, frame id).
	 */
	int ret = 0;

	FIMC_BUG(!hw_ip);

	/*
	 * TODO : enable corex
	ret = rgbp_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id, COREX_COPY);
	if (ret)
		return ret;
	*/
	rgbp_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __is_hw_rgbp_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame, u32 set_id)
{
	struct is_hw_rgbp *hw_rgbp;
	struct is_device_ischain *device;
	struct is_group *group;
	struct is_hardware *hardware;
	struct rgbp_param_set *param_set = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("failed to get rgbp", instance, hw_ip);
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

	param_set = &hw_rgbp->param_set[instance];

	/* rgbp context setting */
	rgbp_hw_s_core(hw_ip->regs[REG_SETA], frame->num_buffers, set_id,
			&hw_rgbp->config, param_set);

	return ret;
}

static void is_hw_rgbp_free_iqset(struct is_hw_ip *hw_ip)
{
	struct is_hw_rgbp *hw = (struct is_hw_rgbp *)hw_ip->priv_info;
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

static int is_hw_rgbp_alloc_iqset(struct is_hw_ip *hw_ip)
{
	int ret;
	struct is_hw_rgbp *hw = (struct is_hw_rgbp *)hw_ip->priv_info;
	u32 instance = hw->instance;
	u32 reg_cnt = rgbp_hw_g_reg_cnt();
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
	is_hw_rgbp_free_iqset(hw_ip);

	return ret;
}

static int __nocfi is_hw_rgbp_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int i, ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWRGBP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_rgbp));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("failed to get hw_rgbp", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_rgbp->instance = instance;

	ret = get_lib_func(LIB_FUNC_RGBP, (void **)&hw_rgbp->lib_func);

	if (hw_rgbp->lib_func == NULL) {
		mserr_hw("hw_rgbp->lib_func(null)", instance, hw_ip);
		is_load_clear();
		ret = -EINVAL;
		goto err_lib_func;
	}

	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw_rgbp->lib_support = is_get_lib_support();

	hw_rgbp->lib[instance].func = hw_rgbp->lib_func;
	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_rgbp->lib[instance],
				instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	if (is_hw_rgbp_alloc_iqset(hw_ip))
		goto err_iqset_alloc;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_rgbp->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_rgbp->iq_set[i].state);
		spin_lock_init(&hw_rgbp->iq_set[i].slock);
	}

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_iqset_alloc:
	if (IS_ENABLED(RGBP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_rgbp->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_rgbp_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_device_ischain *device;
	u32 idx, f_type;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("hw_rgbp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}
	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	hw_rgbp->lib[instance].object = NULL;
	hw_rgbp->lib[instance].func = hw_rgbp->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_rgbp->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip,
					&hw_rgbp->lib[instance], instance,
					(u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	/* set RDMA */
	for (idx = RGBP_RDMA_CL; idx < RGBP_RDMA_MAX; idx++) {
		ret = rgbp_hw_rdma_create(&hw_rgbp->rdma[idx], hw_ip->regs[REG_SETA], idx);
		if (ret) {
			mserr_hw("rgbp_hw_rdma_create error[%d]", instance, hw_ip, idx);
			ret = -ENODATA;
			goto err;
		}
	}

	/* set WDMA */
	for (idx = RGBP_WDMA_HF; idx < RGBP_WDMA_MAX; idx++) {
		ret = rgbp_hw_wdma_create(&hw_rgbp->wdma[idx], hw_ip->regs[REG_SETA], idx);
		if (ret) {
			mserr_hw("rgbp_hw_wdma_create error[%d]", instance, hw_ip, idx);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;

}

static int is_hw_rgbp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_rgbp->lib[instance],
				instance);
		hw_rgbp->lib[instance].object = NULL;
	}

	return ret;
}

static int is_hw_rgbp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	FIMC_BUG(!hw_rgbp->lib_support);

	if (IS_ENABLED(RGBP_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_rgbp->lib[instance],
				instance);

	__is_hw_rgbp_clear_common(hw_ip, instance);

	is_hw_rgbp_free_iqset(hw_ip);

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_rgbp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

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

	__is_hw_rgbp_s_common_reg(hw_ip, instance);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_rgbp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_rgbp *hw_rgbp;
	struct rgbp_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("rgbp_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_rgbp->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(RGBP_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_rgbp->lib[instance],
					instance, 1);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_rgbp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp, struct rgbp_param_set *param_set,
										u32 instance, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint = 0x7;
	int ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_rgbp);
	FIMC_BUG(!param_set);

	input_dva = rgbp_hw_g_input_dva(param_set, instance, id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_rgbp->rdma[id].name, cmd);

	rgbp_hw_s_rdma_corex_id(&hw_rgbp->rdma[id], set_id);

	ret = rgbp_hw_s_rdma_init(hw_ip, &hw_rgbp->rdma[id], param_set, cmd,
		in_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize RGBP_DMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_rdma_addr(&hw_rgbp->rdma[id], input_dva, 0, (hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set RGBP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_rgbp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_rgbp *hw_rgbp, struct rgbp_param_set *param_set,
							u32 instance, u32 id, u32 set_id, struct is_frame *dma_frame)
{
	u32 *output_dva = NULL;
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 out_crop_size_x = 0;
	u32 cache_hint = 0x7;
	bool sbwc_en;
	int ret = 0;
	struct is_param_region *param;
	struct mcs_param *mcs_param;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_rgbp);
	FIMC_BUG(!param_set);

	param = &hw_ip->region[instance]->parameter;
	mcs_param = &param->mcs;

	output_dva = rgbp_hw_g_output_dva(param_set, param, instance, id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_rgbp->wdma[id].name, cmd);

	rgbp_hw_s_wdma_corex_id(&hw_rgbp->wdma[id], set_id);

	ret = rgbp_hw_s_wdma_init(hw_ip, &hw_rgbp->wdma[id], param_set, instance, cmd,
		out_crop_size_x, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("failed to initialize RGBP_DMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_wdma_addr(&hw_rgbp->wdma[id], output_dva, 0, (hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set RGBP_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
		sbwc_en = comp_sbwc_en ? true : false;
		rgbp_hw_s_sbwc(hw_ip->regs[REG_SETA], set_id, sbwc_en);
	}

	return 0;
}


static void __is_hw_rgbp_check_size(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set, u32 instance)
{
	struct param_dma_output *output_dma = &param_set->dma_output_yuv;
	struct param_otf_input *input = &param_set->otf_input;
	struct param_otf_output *output = &param_set->otf_output;

	FIMC_BUG_VOID(!param_set);

	msdbgs_hw(2, "hw_rgbp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "otf_input: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		input->bayer_crop_offset_x, input->bayer_crop_offset_y,
		input->bayer_crop_width, input->bayer_crop_height,
		input->width, input->height);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	msdbgs_hw(2, "dma_output: format(%d),crop_size(%dx%d)\n", instance, hw_ip,
		output_dma->format, output_dma->dma_crop_width, output_dma->dma_crop_height);
	msdbgs_hw(2, "[%d]<<< hw_rgbp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_rgbp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	FIMC_BUG(!hw_ip);

	rgbp_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id);

	return 0;
}


static int __is_hw_rgbp_update_block_reg(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
									u32 instance, u32 set_id)
{
	struct is_hw_rgbp *hw_rgbp;
	int ret = 0;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (hw_rgbp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_rgbp->instance);
		hw_rgbp->instance = instance;
	}

	__is_hw_rgbp_check_size(hw_ip, param_set, instance);
	ret = __is_hw_rgbp_bypass(hw_ip, set_id);

	return ret;
}

static void __is_hw_rgbp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct rgbp_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct rgbp_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!param_set);

	param = &p_region->rgbp;
	param_set->instance_id = instance;
	param_set->byrp_param = &p_region->byrp;

	/* check input */
	if (test_bit(PARAM_RGBP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
				sizeof(struct param_otf_input));

	/* check output */
	if (test_bit(PARAM_RGBP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
				sizeof(struct param_otf_output));

	if (test_bit(PARAM_RGBP_YUV, pmap))
		memcpy(&param_set->dma_output_yuv, &param->yuv,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_HF, pmap))
		memcpy(&param_set->dma_output_hf, &param->hf,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_SF, pmap))
		memcpy(&param_set->dma_output_sf, &param->sf,
				sizeof(struct param_dma_output));

	if (test_bit(PARAM_RGBP_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));

#if IS_ENABLED(CONFIG_RGBP_V1_1)
	if (test_bit(PARAM_RGBP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
				sizeof(struct param_dma_input));

	if (test_bit(PARAM_RGBP_RGB, pmap))
		memcpy(&param_set->dma_output_rgb, &param->rgb,
				sizeof(struct param_dma_output));
#endif

}

static int is_hw_rgbp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hw_rgbp->instance = IS_STREAM_COUNT;

	return 0;
}

static int __is_hw_rgbp_set_rta_regs(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	struct is_hw_rgbp *hw_rgbp = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_rgbp_iq *iq_set;
	struct is_hw_rgbp_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i;
	bool configured = false;

	mdbg_hw(4, "%s\n", instance, __func__);

	iq_set = &hw_rgbp->iq_set[instance];
	cur_iq_set = &hw_rgbp->cur_hw_iq_set[set_id];

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

static void is_hw_rgbp_s_size_regs(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
	u32 instance, const struct is_frame *frame)
{
	struct is_hw_rgbp *hw;
	void __iomem *base;
	struct rgbp_grid_cfg grid_cfg;
	struct is_rgbp_config *cfg;
	u32 strip_enable, start_pos_x, start_pos_y;
	u32 sensor_full_width, sensor_full_height;
	u32 sensor_binning_x, sensor_binning_y;
	u32 sensor_crop_x, sensor_crop_y;
	u32 bns_binning_x, bns_binning_y;

	hw = (struct is_hw_rgbp *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	cfg = &hw->config;

	/* GRID configuration for LSC */
	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	bns_binning_x = frame->shot->udm.frame_info.bns_binning[0];
	bns_binning_y = frame->shot->udm.frame_info.bns_binning[1];

	/* Center */
	if (cfg->sensor_center_x == 0 || cfg->sensor_center_y == 0) {
		cfg->sensor_center_x = sensor_full_width >> 1;
		cfg->sensor_center_y = sensor_full_height >> 1;
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)",
				instance, hw_ip, __func__,
				cfg->sensor_center_x, cfg->sensor_center_y);
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	grid_cfg.binning_x = (1024ULL * sensor_binning_x * bns_binning_x) / 1000 / 1000;
	grid_cfg.binning_y = (1024ULL * sensor_binning_y * bns_binning_y) / 1000 / 1000;
	if (grid_cfg.binning_x == 0 || grid_cfg.binning_y == 0) {
		grid_cfg.binning_x = 1024;
		grid_cfg.binning_y = 1024;
		msdbg_hw(2, "%s:[LSC] calculated binning(0,0). Fix to (%d,%d)",
				instance, hw_ip, __func__, 1024, 1024);
	}

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned(Stripe_start_position + BYRP_bcrop) + unbinned(sensor_crop) */
	sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	start_pos_x = (strip_enable) ? param_set->stripe_input.start_pos_x : 0;
	start_pos_x = (((start_pos_x + param_set->byrp_param->dma_input.bayer_crop_offset_x) *
			grid_cfg.binning_x) / 1024) +
			(sensor_crop_x * sensor_binning_x) / 1000;
	start_pos_y = ((param_set->byrp_param->dma_input.bayer_crop_offset_y * grid_cfg.binning_y) / 1024) +
			(sensor_crop_y * sensor_binning_y) / 1000;
	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (cfg->sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (cfg->sensor_center_y - start_pos_y));

	rgbp_hw_s_grid_cfg(base, &grid_cfg);

	msdbg_hw(2, "%s:[LSC] stripe: enable(%d), pos_x(%d), full_size(%dx%d)\n",
			instance, hw_ip, __func__,
			strip_enable, param_set->stripe_input.start_pos_x,
			param_set->stripe_input.full_width,
			param_set->stripe_input.full_height);

	msdbg_hw(2, "%s:[LSC] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			instance, hw_ip, __func__,
			sensor_full_width, sensor_full_height,
			cfg->sensor_center_x,
			cfg->sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[LSC] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			instance, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);
}

static int __is_hw_rgbp_set_size_regs(struct is_hw_ip *hw_ip, struct rgbp_param_set *param_set,
	u32 instance, u32 rgbp_strip_start_pos, const struct is_frame *frame, u32 set_id)
{
	struct is_hw_rgbp *hw_rgbp;
	struct is_rgbp_config *config;
	struct is_hw_rgbp_sc_size yuvsc_size, upsc_size;
	struct is_param_region *param;
	struct mcs_param *mcs_param;
	struct rgbp_radial_cfg radial_cfg;
	int ret = 0;
	u32 strip_enable;
	u32 stripe_index;
	u32 strip_start_pos_x;
	u32 in_width, in_height;
	u32 out_width, out_height;
	u32 vratio, hratio;
	u32 yuvsc_enable, yuvsc_bypass;
	u32 upsc_enable, upsc_bypass;
	u32 decomp_enable, decomp_bypass;
	u32 gamma_enable, gamma_bypass;
	u32 y_ds_upsc_us;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	config = &hw_rgbp->config;

	in_width = param_set->otf_input.width;
	in_height = param_set->otf_input.height;
	if (param_set->otf_output.cmd) {
		out_width = param_set->otf_output.width;
		out_height = param_set->otf_output.height;
	} else if (param_set->dma_output_yuv.cmd) {
		out_width = param_set->dma_output_yuv.width;
		out_height = param_set->dma_output_yuv.height;
	} else {
		out_width = in_width;
		out_height = in_height;
	}

	rgbp_hw_s_chain_src_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height);
	rgbp_hw_s_chain_dst_size(hw_ip->regs[REG_SETA], set_id, out_width, out_height);

	rgbp_hw_s_dtp_output_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height);

	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos_x = (stripe_index) ?
		(param_set->stripe_input.start_pos_x - param_set->stripe_input.left_margin) : 0;

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

	rgbp_hw_s_dns_size(hw_ip->regs[REG_SETA], set_id, in_width, in_height,
			strip_enable, strip_start_pos_x, &radial_cfg, config);

	yuvsc_size.input_h_size = in_width;
	yuvsc_size.input_v_size = in_height;
	yuvsc_size.dst_h_size = out_width;
	yuvsc_size.dst_v_size = out_height;

	yuvsc_enable = 0x1;
	yuvsc_bypass = 0x1;
	if ((in_width != out_width) || (in_height != out_height)) {
		msdbgs_hw(2, "[F:%d][%s] YUVSC enable incrop(%d, %d), otcrop(%d, %d)\n",
					instance, hw_ip, frame->fcount, __func__,
					in_width, in_height, out_width, out_height);
		yuvsc_bypass = 0x0;
	}

	rgbp_hw_s_cout_fifo(hw_ip->regs[REG_SETA], set_id, param_set->otf_output.cmd);

	rgbp_hw_s_yuvsc_enable(hw_ip->regs[REG_SETA], set_id, yuvsc_enable, yuvsc_bypass);

	rgbp_hw_s_yuvsc_input_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.input_h_size, yuvsc_size.input_v_size);
	rgbp_hw_s_yuvsc_src_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.input_h_size, yuvsc_size.input_v_size);
	rgbp_hw_s_yuvsc_dst_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.dst_h_size, yuvsc_size.dst_v_size);
	rgbp_hw_s_yuvsc_out_crop_size(hw_ip->regs[REG_SETA], set_id, yuvsc_size.dst_h_size, yuvsc_size.dst_v_size);

	hratio = ((u64)yuvsc_size.input_h_size << 20) / yuvsc_size.dst_h_size;
	vratio = ((u64)yuvsc_size.input_v_size << 20) / yuvsc_size.dst_v_size;

	rgbp_hw_s_yuvsc_scaling_ratio(hw_ip->regs[REG_SETA], set_id, hratio, vratio);

	/* set_crop */
	rgbp_hw_s_crop(hw_ip->regs[REG_SETA], in_width, in_height, out_width, out_height);

	param = &hw_ip->region[instance]->parameter;
	mcs_param = &param->mcs;

	upsc_enable = 0x0;
	upsc_bypass = 0x0;
	decomp_enable = 0x0;
	decomp_bypass = 0x0;
	y_ds_upsc_us = 0x0;
	gamma_enable = 0x0;
	gamma_bypass = 0x1;

	/* for HF */
	if (mcs_param->output[MCSC_OUTPUT5].dma_cmd == DMA_OUTPUT_VOTF_ENABLE) {
		if ((param_set->otf_input.width == param_set->otf_output.width) &&
			(param_set->otf_input.height == param_set->otf_output.height)) {
			mserr_hw("not support HF in the same otf in/output size\n", instance, hw_ip);
			goto hf_disable;
		}

		/* set UPSC */
		upsc_size.input_h_size = yuvsc_size.dst_h_size;
		upsc_size.input_v_size = yuvsc_size.dst_v_size;
		upsc_size.dst_h_size = yuvsc_size.input_h_size;
		upsc_size.dst_v_size = yuvsc_size.input_v_size;

		upsc_enable = 0x1;
		rgbp_hw_s_upsc_input_size(hw_ip->regs[REG_SETA], set_id, upsc_size.input_h_size,
									upsc_size.input_v_size);
		rgbp_hw_s_upsc_src_size(hw_ip->regs[REG_SETA], set_id, upsc_size.input_h_size, upsc_size.input_v_size);
		rgbp_hw_s_upsc_dst_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size, upsc_size.dst_v_size);
		rgbp_hw_s_upsc_out_crop_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size,
									upsc_size.dst_v_size);

		hratio = (upsc_size.input_h_size << 20) / upsc_size.dst_h_size;
		vratio = (upsc_size.input_v_size << 20) / upsc_size.dst_v_size;

		rgbp_hw_s_upsc_scaling_ratio(hw_ip->regs[REG_SETA], set_id, hratio, vratio);
		rgbp_hw_s_upsc_coef(hw_ip->regs[REG_SETA], set_id, hratio, vratio);

		gamma_enable = 0x1;
		gamma_bypass = 0x0;

		/* set Decomp */
		decomp_enable = 0x1;
		rgbp_hw_s_decomp_size(hw_ip->regs[REG_SETA], set_id, upsc_size.dst_h_size, upsc_size.dst_v_size);

		y_ds_upsc_us = 1;

		rgbp_hw_s_votf(hw_ip->regs[REG_SETA], set_id, 0x1, 0x1);
	} else
		rgbp_hw_s_votf(hw_ip->regs[REG_SETA], set_id, 0x0, 0x1);

hf_disable:
	rgbp_hw_s_upsc_enable(hw_ip->regs[REG_SETA], set_id, upsc_enable, upsc_bypass);
	rgbp_hw_s_decomp_enable(hw_ip->regs[REG_SETA], set_id, decomp_enable, decomp_bypass);
	rgbp_hw_s_ds_y_upsc_us(hw_ip->regs[REG_SETA], set_id, y_ds_upsc_us);

	if (!IS_ENABLED(RGBP_DDK_LIB_CALL))
		gamma_bypass = 0x1;

	/* set gamma */
	rgbp_hw_s_gamma_enable(hw_ip->regs[REG_SETA], set_id, gamma_enable, gamma_bypass);

	return ret;
}

static int is_hw_rgbp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_param_region *param_region;
	struct rgbp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	struct mcs_param *mcs_param;
	u32 fcount, instance;
	u32 strip_start_pos = 0;
	u32 cur_idx;
	u32 set_id;
	int ret, i, batch_num;
	u32 cmd_input;
	u32 cmd_output_hf, cmd_output_sf, cmd_output_yuv, cmd_output_rgb;
	u32 strip_enable;
	u32 stripe_index;
	bool do_blk_cfg = true;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	bool skip_isp_shot;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot(%d)\n", instance, hw_ip, frame->fcount, frame->cur_buf_index);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	FIMC_BUG(!hw_ip->priv_info);
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	param_set = &hw_rgbp->param_set[instance];
	param_region = (struct is_param_region *)frame->shot->ctl.vendor_entry.parameter;

	atomic_set(&hw_rgbp->strip_index, frame->stripe_info.region_id);
	fcount = frame->fcount;
	dma_frame = frame;

	FIMC_BUG(!frame->shot);

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

	__is_hw_rgbp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	mcs_param = &param_region->mcs;

	cmd_input = is_hardware_dma_cfg("rgbp", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_input.cmd,
			param_set->dma_input.plane,
			param_set->input_dva,
			dma_frame->dvaddr_buffer);

	if (mcs_param->output[MCSC_OUTPUT5].dma_cmd == DMA_OUTPUT_VOTF_ENABLE) {
		struct is_device_ischain *device;
		struct is_group *group;

		group = hw_ip->group[instance];
		device = group->device;
		dma_frame = is_votf_get_frame(group, TWS, device->m5p.id, 0);
		if (!dma_frame) {
			mserr_hw("is_votf_get_frame is NULL", instance, hw_ip);
			return -EINVAL;
		}
	}

	/* RGBP HF WDMA use VOTF */
	if (mcs_param->output[MCSC_OUTPUT5].dma_cmd == DMA_OUTPUT_VOTF_ENABLE) {
		cmd_output_hf = is_hardware_dma_cfg("rgbphf", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&mcs_param->output[MCSC_OUTPUT5].dma_cmd,
				mcs_param->output[MCSC_OUTPUT5].plane,
				param_set->output_dva_hf,
				dma_frame->dvaddr_buffer);

	} else {
		cmd_output_hf = is_hardware_dma_cfg_32bit("rgbphf", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma_output_hf.cmd,
				param_set->dma_output_hf.plane,
				param_set->output_dva_hf,
				dma_frame->dva_rgbp_hf);
	}

	cmd_output_sf = is_hardware_dma_cfg_32bit("rgbpsf", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_sf.cmd,
			param_set->dma_output_sf.plane,
			param_set->output_dva_sf,
			dma_frame->dva_rgbp_sf);

	cmd_output_yuv = is_hardware_dma_cfg_32bit("rgbpyuv", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_yuv.cmd,
			param_set->dma_output_yuv.plane,
			param_set->output_dva_yuv,
			dma_frame->dva_rgbp_yuv);

	cmd_output_rgb = is_hardware_dma_cfg_32bit("rgbprgb", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma_output_rgb.cmd,
			param_set->dma_output_rgb.plane,
			param_set->output_dva_rgb,
			dma_frame->dva_rgbp_rgb);

	msdbg_hw(2, "[F:%d][%s] hf:%d, sf:%d, yuv:%d, rgb:%d\n", instance, hw_ip, dma_frame->fcount, __func__,
							mcs_param->output[MCSC_OUTPUT5].dma_cmd,
							param_set->dma_output_sf.cmd,
							param_set->dma_output_yuv.cmd,
							param_set->dma_output_rgb.cmd);

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
	strip_start_pos = (stripe_index) ?
		(param_set->stripe_input.start_pos_x - param_set->stripe_input.left_margin) : 0;

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


	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		if (frame->shot) {
			if (!skip_isp_shot) {
				ret = is_lib_isp_set_ctrl(hw_ip, &hw_rgbp->lib[instance], frame);
				if (ret)
					mserr_hw("set_ctrl fail", instance, hw_ip);
			}
		}
	}

	set_id = COREX_DIRECT;

	ret = __is_hw_rgbp_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_rgbp_init_config is fail", instance, hw_ip);
		return ret;
	}

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		if (!skip_isp_shot) {
			ret = is_lib_isp_shot(hw_ip, &hw_rgbp->lib[instance], param_set,
					frame->shot);
			if (ret)
				return ret;

			if (likely(!test_bit(hw_ip->id, &debug_iq))) {
				_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
				ret = __is_hw_rgbp_set_rta_regs(hw_ip, set_id, instance);
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

	if (unlikely(do_blk_cfg))
		__is_hw_rgbp_update_block_reg(hw_ip, param_set, instance, set_id);

	is_hw_rgbp_s_size_regs(hw_ip, param_set, instance, frame);

	ret = __is_hw_rgbp_set_size_regs(hw_ip, param_set, instance,
			strip_start_pos, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_rgbp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = RGBP_RDMA_CL; i < RGBP_RDMA_MAX; i++) {
		ret = __is_hw_rgbp_set_rdma(hw_ip, hw_rgbp, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_rgbp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = RGBP_WDMA_HF; i < RGBP_WDMA_MAX; i++) {
		ret = __is_hw_rgbp_set_wdma(hw_ip, hw_rgbp, param_set, instance,
				i, set_id, dma_frame);
		if (ret) {
			mserr_hw("__is_hw_rgbp_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	ret = is_hw_rgbp_set_cmdq(hw_ip, instance, set_id);
	if (ret) {
		mserr_hw("is_hw_rgbp_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(debug_rgbp))
		rgbp_hw_dump(hw_ip->regs[REG_SETA]);

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(RGBP_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_rgbp->lib[instance], instance);

	return ret;
}

static int is_hw_rgbp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (unlikely(!hw_rgbp)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_rgbp->lib[frame->instance],
				frame);
		if (ret)
			mserr_hw("get_meta fail", frame->instance, hw_ip);
	}

	return ret;
}


static int is_hw_rgbp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = is_hardware_frame_done(hw_ip, frame, -1,
			output_id, done_type, false);
	}

	return ret;
}


static int is_hw_rgbp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag = 0;
	ulong addr;
	u32 size, index;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	FIMC_BUG(!hw_ip);

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
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
				setfile->using_count);
		for (index = 0; index < setfile->using_count; index++) {
			addr = setfile->table[index].addr;
			size = setfile->table[index].size;
			ret = is_lib_isp_create_tune_set(&hw_rgbp->lib[instance],
					addr, size, index, flag, instance);

			set_bit(index, &hw_rgbp->lib[instance].tune_count);
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_rgbp_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 setfile_index = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

	FIMC_BUG(!hw_ip);

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
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (IS_ENABLED(RGBP_DDK_LIB_CALL))
		ret = is_lib_isp_apply_tune_set(&hw_rgbp->lib[instance],
				setfile_index, instance, scenario);

	return ret;
}

static int is_hw_rgbp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	int ret = 0;
	int i = 0;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;

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
	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw_rgbp->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw_rgbp->lib[instance],
						(u32)i, instance);
				clear_bit(i, &hw_rgbp->lib[instance].tune_count);
			}
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

int is_hw_rgbp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;

	if (IS_ENABLED(RGBP_DDK_LIB_CALL)) {
		ret = is_lib_isp_reset_recovery(hw_ip, &hw_rgbp->lib[instance],
				instance);
		if (ret) {
			mserr_hw("is_lib_rgbp_reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
		}
	}

	return ret;
}

static int is_hw_rgbp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_rgbp *hw_rgbp = NULL;
	struct is_hw_rgbp_iq *iq_set = NULL;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	iq_set = &hw_rgbp->iq_set[instance];

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

int is_hw_rgbp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_rgbp *hw_rgbp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		rgbp_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = RGBP_RDMA_CL; i < RGBP_RDMA_MAX; i++) {
			hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
			dma = &hw_rgbp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = RGBP_WDMA_HF; i < RGBP_WDMA_MAX; i++) {
			hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
			dma = &hw_rgbp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_rgbp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_rgbp *hw_rgbp = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	hw_rgbp->config = *(struct is_rgbp_config *)conf;

	return 0;
}

static int is_hw_rgbp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_rgbp *hw_rgbp = NULL;

	hw_rgbp = (struct is_hw_rgbp *)hw_ip->priv_info;
	if (!hw_rgbp) {
		mserr_hw("failed to get HW RGBP", instance, hw_ip);
		return -ENODEV;
	}
	/* DEBUG */
	rgbp_hw_dump(hw_ip->regs[REG_SETA]);

	ret = is_lib_notify_timeout(&hw_rgbp->lib[instance], instance);

	return ret;
}

const struct is_hw_ip_ops is_hw_rgbp_ops = {
	.open			= is_hw_rgbp_open,
	.init			= is_hw_rgbp_init,
	.deinit			= is_hw_rgbp_deinit,
	.close			= is_hw_rgbp_close,
	.enable			= is_hw_rgbp_enable,
	.disable		= is_hw_rgbp_disable,
	.shot			= is_hw_rgbp_shot,
	.set_param		= is_hw_rgbp_set_param,
	.get_meta		= is_hw_rgbp_get_meta,
	.frame_ndone		= is_hw_rgbp_frame_ndone,
	.load_setfile		= is_hw_rgbp_load_setfile,
	.apply_setfile		= is_hw_rgbp_apply_setfile,
	.delete_setfile		= is_hw_rgbp_delete_setfile,
	.restore		= is_hw_rgbp_restore,
	.set_regs		= is_hw_rgbp_set_regs,
	.dump_regs		= is_hw_rgbp_dump_regs,
	.set_config		= is_hw_rgbp_set_config,
	.notify_timeout		= is_hw_rgbp_notify_timeout,
};

int is_hw_rgbp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot = -1;

	hw_ip->ops  = &is_hw_rgbp_ops;
	hw_ip->is_leader = false;

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_rgbp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_rgbp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
