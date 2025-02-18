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

#include "is-hw-lme-v2.h"
#include "is-err.h"
#include "api/is-hw-api-lme-v2_0.h"

#define LIB_MODE LME_USE_DRIVER
spinlock_t lme_out_slock;

int debug_lme;
module_param(debug_lme, int, 0644);

#if IS_ENABLED(CONFIG_LME_FRAME_END_EVENT_NOTIFICATION)
static inline void __is_hw_lme_end_notifier(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme;
	void *data;
	u32 fcount, strip_index;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	fcount = atomic_read(&hw_ip->fcount);
	data = (void *)&hw_lme->data;
	strip_index = atomic_read(&hw_lme->strip_index);

	set_bit(HW_END, &hw_ip->state);

	is_lib_isp_event_notifier(hw_ip, &hw_lme->lib[instance],
		instance, fcount, LME_EVENT_MOTION_DATA, 0, data);

	is_lib_isp_event_notifier(hw_ip, &hw_lme->lib[instance],
			instance, fcount, LME_EVENT_FRAME_END, strip_index, NULL);

	msdbg_hw(3, "%s: hw_mv: kva 0x%lx byte %d\n", instance, hw_ip, __func__,
		hw_lme->data.hw_mv_kva, hw_lme->data.hw_mv_bytes);

	msdbg_hw(3, "%s: hw_sad: kva 0x%lx byte %d\n", instance, hw_ip, __func__,
		hw_lme->data.hw_sad_kva, hw_lme->data.hw_sad_bytes);

	msdbg_hw(3, "%s: sw_mv: kva 0x%lx byte %d\n", instance, hw_ip, __func__,
		hw_lme->data.sw_mv_kva, hw_lme->data.sw_mv_bytes);

	msdbg_hw(3, "%s: sw_hdr_mv: kva 0x%lx byte %d\n", instance, hw_ip, __func__,
		hw_lme->data.sw_hdr_mv_kva, hw_lme->data.sw_hdr_mv_bytes);
}

static void __is_hw_lme_end_tasklet(unsigned long data)
{
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)data;
	struct is_hw_lme *hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	u32 instance = atomic_read(&hw_ip->instance);

	msdbg_hw(2, "%s: src 0x%lx\n",
		instance, hw_ip, __func__, hw_lme->irq_state[LME_INTR]);

	__is_hw_lme_end_notifier(hw_ip, instance);
}

static void __is_hw_lme_s_buffer_info(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme;
	struct is_lme_data *lme_data;
	struct lme_param_set *param_set;
	u32 img_width, img_height;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	lme_data = &hw_lme->data;
	param_set = &hw_lme->param_set[instance];

	img_width = param_set->dma.prev_input_width;
	img_height = param_set->dma.prev_input_height;

	lme_data->hw_mv_kva = param_set->output_kva_motion_pure[0];
	lme_data->hw_mv_bytes = (img_width / IS_LME_BLOCK_PER_W_PIXELS)
		* (img_height / IS_LME_BLOCK_PER_H_PIXELS) * IS_LME_MV_SIZE_PER_BLOCK;

	lme_data->hw_sad_kva = param_set->output_kva_sad[0];
	lme_data->hw_sad_bytes = (img_width / IS_LME_BLOCK_PER_W_PIXELS)
		* (img_height / IS_LME_BLOCK_PER_H_PIXELS) * IS_LME_SAD_SIZE_PER_BLOCK;

	lme_data->sw_mv_kva = param_set->output_kva_motion_processed[0];
	lme_data->sw_mv_bytes = (img_width / IS_LME_BLOCK_PER_W_PIXELS)
		* (img_height / IS_LME_BLOCK_PER_H_PIXELS) * IS_LME_MV_SIZE_PER_BLOCK;

	lme_data->sw_hdr_mv_kva = param_set->output_kva_motion_processed_hdr[0];
	lme_data->sw_hdr_mv_bytes = (img_width / IS_LME_BLOCK_PER_W_PIXELS)
		* (img_height / IS_LME_BLOCK_PER_H_PIXELS) * IS_LME_MV_SIZE_PER_BLOCK;
}
#endif

static inline void __nocfi __is_hw_lme_ddk_isr(struct is_interface_hwip *itf_hw, int handler_id)
{
	struct hwip_intr_handler *intr_hw = NULL;

	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_fpsimd_get_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_fpsimd_put_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static int is_hw_lme_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware = NULL;
	struct is_hw_ip *hw_ip = NULL;
	struct is_hw_lme *hw_lme = NULL;
	struct is_interface_hwip *itf_hw = NULL;
	u32 status, instance, hw_fcount, strip_index, set_id, hl = 0, vl = 0;
	int f_err;
	int hw_slot;

	hw_ip = (struct is_hw_ip *)context;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	strip_index = atomic_read(&hw_lme->strip_index);
	set_id = hw_lme->corex_set_id;

	hw_slot = is_hw_slot_id(hw_ip->id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return IRQ_NONE;
	}

	itf_hw = &hw_ip->itfc->itf_ip[hw_slot];

	if (hw_ip->lib_mode == LME_USE_ONLY_DDK) {
		__is_hw_lme_ddk_isr(itf_hw, INTR_HWIP1);
		return IRQ_HANDLED;
	}

	status = lme_hw_g_int_state(hw_ip->regs[REG_SETA], true,
			(hw_ip->num_buffers & 0xffff),
			&hw_lme->irq_state[LME_INTR], set_id)
			& lme_hw_g_int_mask(hw_ip->regs[REG_SETA], set_id);

	msdbg_hw(2, "LME interrupt status(0x%x)\n", instance, hw_ip, status);

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

	if (lme_hw_is_occurred(status, INTR_FRAME_START) && lme_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (lme_hw_is_occurred(status, INTR_FRAME_START)) {
		dbg_lme(4, "[%d][F:%d]F.S\n", instance, hw_fcount);

		if (IS_ENABLED(LME_DDK_LIB_CALL)) {
			is_lib_isp_event_notifier(hw_ip, &hw_lme->lib[instance],
					instance, hw_fcount, LME_EVENT_FRAME_START,
					strip_index, NULL);
		} else {
			atomic_add(1, &hw_ip->count.fs);
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_START);
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				dbg_lme(4, "[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			is_hardware_frame_start(hw_ip, instance);
		}
	}

	if (lme_hw_is_occurred(status, INTR_FRAME_END)) {
		dbg_lme(4, "[%d][F:%d]F.E\n", instance, hw_fcount);

		if (IS_ENABLED(LME_DDK_LIB_CALL)) {
#if IS_ENABLED(CONFIG_LME_FRAME_END_EVENT_NOTIFICATION)
			tasklet_schedule(&hw_lme->end_tasklet);
#else
			is_lib_isp_event_notifier(hw_ip, &hw_lme->lib[instance],
					instance, hw_fcount, LME_EVENT_FRAME_END,
					strip_index, NULL);
#endif
		} else {
			_is_hw_frame_dbg_trace(hw_ip, hw_fcount,
					DEBUG_POINT_FRAME_END);
			atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

			is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
					IS_SHOT_SUCCESS, true);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				dbg_lme(4, "[F:%d]F.E\n", instance, hw_ip, hw_fcount);

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

	f_err = lme_hw_is_occurred(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		lme_hw_dump(hw_ip->regs[REG_SETA]);
	}

	return 0;
}

static int __is_hw_lme_s_reset(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme)
{
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_lme->cur_hw_iq_set[i].size = 0;

	return lme_hw_s_reset(hw_ip->regs[REG_SETA]);
}

static int __is_hw_lme_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	int res = 0;
	struct is_hw_lme *hw_lme;
	u32 seed;

	FIMC_BUG(!hw_ip);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if (hw_ip) {
		msinfo_hw("reset\n", instance, hw_ip);

		res = __is_hw_lme_s_reset(hw_ip, hw_lme);
		if (res != 0) {
			mserr_hw("sw reset fail", instance, hw_ip);
			return -ENODEV;
		}

		lme_hw_s_corex_init(hw_ip->regs[REG_SETA], true);
		lme_hw_s_init(hw_ip->regs[REG_SETA]);

		seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
		if (unlikely(seed))
			lme_hw_s_crc(hw_ip->regs[REG_SETA], seed);
	}

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_lme_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	res = __is_hw_lme_s_reset(hw_ip, hw_lme);
	if (res != 0) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}
	res = lme_hw_wait_idle(hw_ip->regs[REG_SETA], hw_lme->corex_set_id);

	if (res)
		mserr_hw("failed to lme_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished lme\n", instance, hw_ip);

	return res;
}

static int is_hw_lme_set_cmdq(struct is_hw_ip *hw_ip, u32 instance, u32 set_id)
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
	 * ret = lme_hw_s_corex_update_type(hw_ip->regs[REG_SETA], set_id, COREX_COPY);
	 * if (ret)
	 * return ret;
	 */
	lme_hw_s_cmdq(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __is_hw_lme_init_config(struct is_hw_ip *hw_ip, u32 instance,
	struct is_frame *frame, u32 set_id)
{
	struct is_hw_lme *hw_lme;
	struct is_device_ischain *device;
	struct is_group *group;
	struct is_hardware *hardware;
	struct lme_param_set *param_set = NULL;
	int ret = 0;

	FIMC_BUG(!hw_ip);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("failed to get lme", instance, hw_ip);
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

	param_set = &hw_lme->param_set[instance];

	/* lme context setting */
	lme_hw_s_core(hw_ip->regs[REG_SETA], set_id);

	return ret;
}

static int __nocfi is_hw_lme_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int i, j, ret;
	struct is_hw_lme *hw_lme = NULL;
	u32 reg_cnt = lme_hw_get_reg_cnt();

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWLME");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw_ip->priv_info = vzalloc(sizeof(struct is_hw_lme));
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("failed to get hw_lme", instance, hw_ip);
		ret = -ENODEV;
		goto err_alloc;
	}
	hw_lme->instance = instance;

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		is_fpsimd_get_func();
		ret = get_lib_func(LIB_FUNC_LME, (void **)&hw_lme->lib_func);
		is_fpsimd_put_func();
		if (hw_lme->lib_func == NULL) {
			mserr_hw("hw_lme->lib_func(null)", instance, hw_ip);
			is_load_clear();
			ret = -EINVAL;
			goto err_lib_func;
		}

		msinfo_hw("get_lib_func is set\n", instance, hw_ip);
	}

	hw_lme->lib_support = is_get_lib_support();

	hw_lme->lib[instance].func = hw_lme->lib_func;
	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw_lme->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		hw_lme->iq_set[i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_lme->iq_set[i].regs) {
			mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
			ret = -ENOMEM;
			goto err_regs_alloc;
		}
	}

	for (j = 0; j < COREX_MAX; ++j) {
		hw_lme->cur_hw_iq_set[j].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_lme->cur_hw_iq_set[j].regs) {
			mserr_hw("failed to alloc cur_iq_set.regs", instance, hw_ip);
			ret = -ENOMEM;
			goto err_cur_regs_alloc;
		}
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_lme->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_lme->iq_set[i].state);
		spin_lock_init(&hw_lme->iq_set[i].slock);
	}

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_cur_regs_alloc:
	while (j-- > 0) {
		vfree(hw_lme->cur_hw_iq_set[j].regs);
		hw_lme->cur_hw_iq_set[j].regs = NULL;
	}

err_regs_alloc:
	while (i-- > 0) {
		vfree(hw_lme->iq_set[i].regs);
		hw_lme->iq_set[i].regs = NULL;
	}

	if (IS_ENABLED(LME_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_lme->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_lme_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;
	struct is_device_ischain *device;
	struct is_subdev *subdev_lme;
	u32 f_type;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!group);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("hw_lme is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}
	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	hw_ip->lib_mode = LIB_MODE;
	hw_lme->lib[instance].object = NULL;
	hw_lme->lib[instance].func   = hw_lme->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw_lme->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else {
		if (IS_ENABLED(LME_DDK_LIB_CALL)) {
			ret = is_lib_isp_object_create(hw_ip,
					&hw_lme->lib[instance], instance,
					(u32)flag, f_type);
			if (ret) {
				mserr_hw("object create fail", instance, hw_ip);
				return -EINVAL;
			}
		}
	}

	if (hw_ip->lib_mode == LME_USE_ONLY_DDK) {
		set_bit(HW_INIT, &hw_ip->state);
		return 0;
	}

	if (IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)) {
		subdev_lme = group->subdev[ENTRY_LME_MBMV];
		is_hardware_alloc_internal_buffer(device, subdev_lme,
			device->group_lme.leader.vid,
			LME_IMAGE_MAX_WIDTH / 16, LME_IMAGE_MAX_HEIGHT / 16,
			DMA_INOUT_BIT_WIDTH_16BIT, LME_TNR_MODE_MIN_BUFFER_NUM, "LME_MBMV");
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;
}

static int is_hw_lme_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;
	struct is_device_ischain *device;
	struct is_subdev *subdev_lme;
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		is_lib_isp_object_destroy(hw_ip, &hw_lme->lib[instance],
				instance);
		hw_lme->lib[instance].object = NULL;
	}

	device = hw_ip->group[instance]->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		ret = -ENODEV;
		goto err;
	}

	if (IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)) {
		subdev_lme = hw_ip->group[instance]->subdev[ENTRY_LME_MBMV];
		is_hardware_free_internal_buffer(device, subdev_lme);
	}

err:
	return ret;
}

static int is_hw_lme_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_lme *hw_lme = NULL;
	int ret = 0;
	ulong flag = 0;
	u32 i;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	FIMC_BUG(!hw_lme->lib_support);

	if (IS_ENABLED(LME_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw_lme->lib[instance],
				instance);

	if (hw_ip->lib_mode == LME_USE_DRIVER) {
		spin_lock_irqsave(&lme_out_slock, flag);
		__is_hw_lme_clear_common(hw_ip, instance);
		spin_unlock_irqrestore(&lme_out_slock, flag);
	}

	for (i = 0; i < COREX_MAX; ++i) {
		if (hw_lme->cur_hw_iq_set[i].regs) {
			vfree(hw_lme->cur_hw_iq_set[i].regs);
			hw_lme->cur_hw_iq_set[i].regs = NULL;
		}
	}

	for (i = 0; i < IS_STREAM_COUNT; ++i) {
		if (hw_lme->iq_set[i].regs) {
			vfree(hw_lme->iq_set[i].regs);
			hw_lme->iq_set[i].regs = NULL;
		}
	}

	vfree(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_lme_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_lme *hw_lme = NULL;
	int ret = 0;
	ulong flag = 0;

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

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

#if IS_ENABLED(CONFIG_LME_FRAME_END_EVENT_NOTIFICATION)
	tasklet_init(&hw_lme->end_tasklet, __is_hw_lme_end_tasklet, (unsigned long)hw_ip);
#endif

	if (hw_ip->lib_mode == LME_USE_ONLY_DDK) {
		set_bit(HW_RUN, &hw_ip->state);
		return ret;
	}

	spin_lock_irqsave(&lme_out_slock, flag);
	__is_hw_lme_s_common_reg(hw_ip, instance);
	spin_unlock_irqrestore(&lme_out_slock, flag);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return ret;
}

static int is_hw_lme_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("lme_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
		!atomic_read(&hw_ip->status.Vvalid),
		IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance,
			hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_lme->param_set[instance];
	param_set->fcount = 0;
	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(LME_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw_lme->lib[instance],
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

static int __is_hw_lme_set_dma_cfg(struct lme_param_set *param_set, u32 instance,
	struct is_lme_config *config)
{
	dbg_lme(4, "[HW][%s] is called\n", __func__);

	FIMC_BUG(!param_set);
	FIMC_BUG(!config);

	dbg_lme(4, "[HW][%s] set_config lmeds size (%d x %d)\n", __func__,
		config->lme_in_w, config->lme_in_h);

	return 0;
}

static int __is_hw_lme_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme,
	struct lme_param_set *param_set, u32 instance, u32 is_reprocessing, u32 id, u32 set_id)
{
	u32 *input_dva = NULL;
	u32 cmd;
	int ret = 0;
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	u32 mbmv_dva[2] = {0, 0};
	u32 total_width, line_count;
#endif
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_lme);
	FIMC_BUG(!param_set);

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		if (is_reprocessing) {
			input_dva = param_set->cur_input_dva;
			cmd = param_set->dma.cur_input_cmd;
			dbg_lme(4, "[HW][%s] MF STILL CUR cmd: %d, input_dva[0]: 0x%x\n",
			__func__, cmd, input_dva[0]);
		} else {
			input_dva = param_set->prev_input_dva;
			cmd = param_set->dma.prev_input_cmd;
		}
		break;
	case LME_RDMA_CACHE_IN_1:
		if (is_reprocessing) {
			input_dva = param_set->prev_input_dva;
			cmd = param_set->dma.prev_input_cmd;
			dbg_lme(4, "[HW][%s] MF STILL REF cmd: %d, input_dva[0]: 0x%x\n",
				__func__, cmd, input_dva[0]);
		} else {
			input_dva = param_set->cur_input_dva;
			cmd = param_set->dma.cur_input_cmd;
		}
		break;
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	case LME_RDMA_MBMV_IN:
		input_dva = mbmv_dva;
		if (hw_lme->lme_mode == LME_MODE_TNR) {
			total_width = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
			line_count = DIV_ROUND_UP(param_set->dma.output_height, 16);

			input_dva[0] = hw_lme->mbmv1_dva[0] + total_width * (line_count - 1);
			input_dva[1] = hw_lme->mbmv0_dva[0];
		}
		cmd = param_set->dma.cur_input_cmd;
		break;
#endif
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	dbg_lme(4, "[HW][%s] RDMA id: %d, cmd: %d, input_dva[0]: 0x%x, input_dva[1]: 0x%x\n",
		__func__, id, cmd, input_dva[0], input_dva[1]);

	ret = lme_hw_s_rdma_init(hw_ip->regs[REG_SETA], param_set, cmd, id, set_id);

	if (ret) {
		mserr_hw("failed to initialize LME_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = lme_hw_s_rdma_addr(hw_ip->regs[REG_SETA], input_dva, id, set_id);
		if (ret) {
			mserr_hw("failed to set LME_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_lme_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_lme *hw_lme,
	struct lme_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	u32 *output_dva = NULL;
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	u32 mbmv_dva[2] = {0, 0};
	u32 total_width, line_count;
#endif
	u32 cmd = 0;
	int ret;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_lme);
	FIMC_BUG(!param_set);

	switch (id) {
	case LME_WDMA_MV_OUT:
		output_dva = param_set->output_dva;
		cmd = param_set->dma.output_cmd;
		break;
#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	case LME_WDMA_MBMV_OUT:
		output_dva = mbmv_dva;
		if (hw_lme->lme_mode == LME_MODE_TNR) {
			total_width = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
			line_count = DIV_ROUND_UP(param_set->dma.output_height, 16);

			output_dva[0] = hw_lme->mbmv1_dva[0];
			output_dva[1] = hw_lme->mbmv0_dva[0] + total_width * (line_count - 1);
		}
		cmd = param_set->dma.output_cmd;
		break;
#endif
#if IS_ENABLED(CONFIG_LME_SAD)
	case LME_WDMA_SAD_OUT:
		output_dva = param_set->output_dva_sad;
		cmd = param_set->dma.sad_output_cmd;
		break;
#endif
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	dbg_lme(4, "[HW][%s] WDMA id: %d, cmd: %d, output_dva[0]: 0x%x, output_dva[1]: 0x%x\n",
		__func__, id, cmd, output_dva[0], output_dva[1]);

	ret = lme_hw_s_wdma_init(hw_ip->regs[REG_SETA], param_set, cmd, id, set_id);

	if (ret) {
		mserr_hw("failed to initializ LME_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = lme_hw_s_wdma_addr(hw_ip->regs[REG_SETA], output_dva, id, set_id);
		if (ret) {
			mserr_hw("failed to set LME_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_lme_bypass(struct is_hw_ip *hw_ip, u32 set_id, struct lme_param_set *param_set)
{
	u32 cur_width, cur_height;

	FIMC_BUG(!hw_ip);

	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;
	lme_hw_s_block_bypass(hw_ip->regs[REG_SETA], set_id, cur_width, cur_height);

	return 0;
}

static int __is_hw_lme_update_block_reg(struct is_hw_ip *hw_ip,
	struct lme_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_lme *hw_lme;
	int ret = 0;
	u32 cache_en;
	u32 prev_width, cur_width, cur_height;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if (hw_lme->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_lme->instance);
		hw_lme->instance = instance;
	}

	cache_en = 1;
	prev_width = param_set->dma.prev_input_width;
	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;

	/* Block Config */
	lme_hw_s_cache(hw_ip->regs[REG_SETA], set_id, cache_en, prev_width, cur_width);
	ret = __is_hw_lme_bypass(hw_ip, set_id, param_set);

	return ret;
}

static void __is_hw_lme_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
					struct lme_param_set *param_set, IS_DECLARE_PMAP(pmap),
					u32 instance)
{
	struct lme_param *param;

	FIMC_BUG_VOID(!hw_ip);
	FIMC_BUG_VOID(!param_set);

	param = &p_region->lme;
	param_set->instance_id = instance;

	/* check input/output */
	if (test_bit(PARAM_LME_DMA, pmap)) {
		memcpy(&param_set->dma, &param->dma,
			sizeof(struct param_lme_dma));
	}

	dbg_lme(4, "[DBG][%s] param->cur_input_cmd: %d, prev_input_cmd: %d, output_cmd: %d\n", __func__,
		param->dma.cur_input_cmd, param->dma.prev_input_cmd, param->dma.output_cmd);

	dbg_lme(4, "[DBG][%s] param_set->cur_input_cmd: %d, prev_input_cmd: %d, output_cmd: %d\n", __func__,
		param_set->dma.cur_input_cmd, param_set->dma.prev_input_cmd, param_set->dma.output_cmd);
}

static int is_hw_lme_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;

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

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info,
	hw_lme->instance = IS_STREAM_COUNT;

	param_set = &hw_lme->param_set[instance];

	__is_hw_lme_update_param(hw_ip, &region->parameter, param_set, pmap, instance);

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		ret = is_lib_isp_set_param(hw_ip, &hw_lme->lib[instance], param_set);
		if (ret)
			mserr_hw("set_param fail", instance, hw_ip);
	}

	return ret;
}

static int __is_hw_lme_set_rta_regs(struct is_hw_ip *hw_ip, u32 set_id, u32 instance)
{
	struct is_hw_lme *hw_lme = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_lme_iq *iq_set;
	struct is_hw_lme_iq *cur_iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size;
	int i;
	bool configured = false;

	mdbg_hw(4, "%s\n", instance, __func__);

	iq_set = &hw_lme->iq_set[instance];
	cur_iq_set = &hw_lme->cur_hw_iq_set[set_id];

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;
		if (cur_iq_set->size != regs_size ||
			memcmp(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++)
				writel_relaxed(regs[i].reg_data,
					base + GET_LME_COREX_OFFSET(set_id)
					+ regs[i].reg_addr);

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

static int __is_hw_lme_set_size_regs(struct is_hw_ip *hw_ip, struct lme_param_set *param_set,
	u32 instance, u32 set_id)
{
	struct is_device_ischain *device;
	struct is_hw_lme *hw_lme;
	struct is_lme_config *config;
	struct is_param_region *param_region;
	unsigned long flag;
	int ret = 0;
	u32 prev_width, prev_height;
	u32 cur_width, cur_height;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param_set);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	config = &hw_lme->config[instance];
	device = hw_ip->group[instance]->device;
	param_region = &device->is_region->parameter;

	if (config->lme_in_w == 0 || config->lme_in_h == 0)
		return -EINVAL;

	param_set->dma.prev_input_width = config->lme_in_w;
	param_set->dma.prev_input_height = config->lme_in_h;
	param_set->dma.cur_input_width = config->lme_in_w;
	param_set->dma.cur_input_height = config->lme_in_h;
	param_set->dma.output_width = config->lme_in_w;
	param_set->dma.output_height = config->lme_in_h;

	prev_width = param_set->dma.prev_input_width;
	prev_height = param_set->dma.prev_input_height;
	cur_width = param_set->dma.cur_input_width;
	cur_height = param_set->dma.cur_input_height;

	spin_lock_irqsave(&lme_out_slock, flag);

	/* TODO: Block size configuration */
	lme_hw_s_cache_size(hw_ip->regs[REG_SETA], set_id, prev_width, prev_height, cur_width, cur_height);
	lme_hw_s_mvct_size(hw_ip->regs[REG_SETA], set_id, cur_width, cur_height);

#if (DDK_INTERFACE_VER == 0x1010)
	lme_hw_s_first_frame(hw_ip->regs[REG_SETA], set_id, config->first_frame);
#endif

	spin_unlock_irqrestore(&lme_out_slock, flag);
	return ret;
}

static int is_hw_lme_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	struct is_device_ischain *device;
	struct is_hw_lme *hw_lme;
	struct lme_param_set *param_set;
	struct is_param_region *param_region;
	struct lme_param *param;
	u32 fcount, instance;
	bool is_reprocessing;
	ulong flag = 0;
	u32 cur_idx;
	u32 set_id;
	int ret = 0, i = 0, batch_num;
	u32 cmd_cur_input, cmd_prev_input, cmd_output;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	param_set = &hw_lme->param_set[instance];
	param_region = (struct is_param_region *)frame->shot->ctl.vendor_entry.parameter;

	param = &param_region->lme;
	cur_idx = frame->cur_buf_index;
	fcount = frame->fcount;
	device = hw_ip->group[instance]->device;

	is_reprocessing = test_bit(IS_ISCHAIN_REPROCESSING, &device->state);

	if (frame->type == SHOT_TYPE_INTERNAL) {
		cmd_cur_input = param_set->dma.cur_input_cmd;
		param_set->dma.cur_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->cur_input_dva[0] = 0x0;

		cmd_prev_input = param_set->dma.prev_input_cmd;
		param_set->dma.prev_input_cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->prev_input_dva[0] = 0x0;

		cmd_output = param_set->dma.output_cmd;
		param_set->dma.output_cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->output_dva[0] = 0x0;

		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		hw_ip->internal_fcount[instance] = fcount;
		goto config;
	} else {
		FIMC_BUG(!frame->shot);
		/* per-frame control
		 * check & update size from region
		 */
		if (hw_ip->internal_fcount[instance] != 0) {
			hw_ip->internal_fcount[instance] = 0;
			param_set->dma.output_cmd = param->dma.output_cmd;
		}

		/*set TNR operation mode */
		if (frame->shot_ext) {
			param_set->tnr_mode = frame->shot_ext->tnr_mode;
		} else {
			mswarn_hw("frame->shot_ext is null", instance, hw_ip);
			param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
		}
	}

	__is_hw_lme_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */

	/* Only support TNR mode (video mode) */
	hw_lme->lme_mode = LME_MODE_TNR;

	cmd_cur_input = is_hardware_dma_cfg("lme_cur", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				param_set->cur_input_dva,
				frame->dvaddr_buffer);

	dbg_lme(4, "[HW][%s][CUR_IN] cmd: %d, input_dva: 0x%x, dvaddr_buffer[0]: 0x%x\n",
		__func__, param_set->dma.cur_input_cmd,
		param_set->cur_input_dva[0], frame->dvaddr_buffer[0]);

	if (is_reprocessing && !param_set->dma.prev_input_cmd) {
		/* MF STILL after 1st frame. Keep ref addr */
		cmd_prev_input = param_set->dma.prev_input_cmd = 1;
	} else {
		cmd_prev_input = is_hardware_dma_cfg_32bit("lme_prev", hw_ip,
					frame, cur_idx, frame->num_buffers,
					&param_set->dma.prev_input_cmd,
					param_set->dma.input_plane,
					param_set->prev_input_dva,
					frame->lmesTargetAddress);
	}

	/* For 1st frame, use current buffer as prev */
	if (!is_reprocessing && !param_set->dma.prev_input_cmd) {
		param_set->dma.prev_input_cmd = param_set->dma.cur_input_cmd;
		param_set->prev_input_dva[0] = param_set->cur_input_dva[0];
		cmd_prev_input = param_set->dma.prev_input_cmd;

		dbg_lme(4, "[HW][%s] VIDEO 1st frame prev_input_dva = cur_input_dva = 0x%x\n",
			__func__, param_set->cur_input_dva[0]);
	}

	dbg_lme(4, "[HW][%s][PREV_IN] cmd: %d, input_dva: 0x%x, lemdsTargetAddress[0]: 0x%x\n",
		__func__, param_set->dma.prev_input_cmd,
		param_set->prev_input_dva[0], frame->lmesTargetAddress[0]);

#if IS_ENABLED(CONFIG_LME_MBMV_INTERNAL_BUFFER)
	is_hardware_dma_cfg_32bit("lme_mbmv0", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.cur_input_cmd,
			param_set->dma.input_plane,
			hw_lme->mbmv0_dva,
			frame->lmembmv0TargetAddress);
	dbg_lme(4, "[HW][%s][MBMV0_IN_OUT] cmd: %d, mbmv0_dva[0]: 0x%x, lmembmv0TargetAddress[0]: 0x%x\n", __func__,
		param_set->dma.cur_input_cmd, hw_lme->mbmv0_dva[0], frame->lmembmv0TargetAddress[0]);

	if (hw_lme->lme_mode == LME_MODE_TNR) {
		is_hardware_dma_cfg_32bit("lme_mbmv1", hw_ip,
				frame, cur_idx, frame->num_buffers,
				&param_set->dma.cur_input_cmd,
				param_set->dma.input_plane,
				hw_lme->mbmv1_dva,
				frame->lmembmv1TargetAddress);

		dbg_lme(4, "[HW][%s][MBMV1_IN_OUT] cmd: %d, mbmv1_dva[0]: 0x%x, lmembmv1TargetAddress[0]: 0x%x\n",
			__func__, param_set->dma.cur_input_cmd, hw_lme->mbmv1_dva[0], frame->lmembmv1TargetAddress[0]);
	}
#endif

	cmd_output = is_hardware_dma_cfg_32bit("lme_pure", hw_ip,
			frame, cur_idx, frame->num_buffers,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_dva,
			frame->lmecTargetAddress);

	dbg_lme(4, "[HW][%s][MV_OUT] cmd: %d, output_dva: 0x%x, lmecTargetAddress[0]: 0x%x\n",
		__func__, param_set->dma.output_cmd, param_set->output_dva[0], frame->lmecTargetAddress[0]);

	is_hardware_dma_cfg_64bit("lme_pure", hw_ip,
			frame, cur_idx,
			&param_set->dma.output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_pure,
			frame->lmecKTargetAddress);

	dbg_lme(4, "[HW][%s][MV_OUT_KVA] output_kva_motion_pure[0]: 0x%lx, lmecKTargetAddress[0]: 0x%lx\n",
		__func__, param_set->output_kva_motion_pure[0], frame->lmecKTargetAddress[0]);

#if IS_ENABLED(CONFIG_LME_SAD)
	is_hardware_dma_cfg_32bit("lme_sad", hw_ip,
		frame, cur_idx, frame->num_buffers,
		&param_set->dma.sad_output_cmd,
		param_set->dma.output_plane,
		param_set->output_dva_sad,
		frame->lmesadTargetAddress);

	dbg_lme(4, "[HW][%s][SAD_OUT] cmd: %d, output_dva_sad[0]: 0x%x, lmesadTargetAddressa[0]:0x%x,\n", __func__,
		param_set->dma.sad_output_cmd, param_set->output_dva_sad[0], frame->lmesadTargetAddress[0]);
#endif

	is_hardware_dma_cfg_64bit("lme_proc", hw_ip,
			frame, cur_idx,
			&param_set->dma.processed_output_cmd,
			param_set->dma.output_plane,
			param_set->output_kva_motion_processed,
			frame->lmemKTargetAddress);

	dbg_lme(4, "[HW][%s][RTA_MV_OUT_KVA] output_kva_motion_processed[0]: 0x%lx, lmemKTargetAddress[0]: 0x%lx\n",
		__func__, param_set->output_kva_motion_processed[0], frame->lmemKTargetAddress[0]);

config:
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

	if (frame->type == SHOT_TYPE_INTERNAL) {
		is_log_write("[@][DRV][%d]lme_shot [T:%d][F:%d][IN:0x%x] [%d][OUT:0x%x]\n",
			param_set->instance_id, frame->type,
			param_set->fcount, param_set->cur_input_dva[0],
			param_set->dma.output_cmd, param_set->output_dva[0]);
	}

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		if (frame->shot) {
			ret = is_lib_isp_set_ctrl(hw_ip, &hw_lme->lib[instance], frame);
			if (ret)
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}

		param_set->dma.cur_input_cmd = cmd_cur_input;
		param_set->dma.prev_input_cmd = cmd_prev_input;
		param_set->dma.output_cmd = cmd_output;
	} else {
		/* mod_timer(&hw_ip->lme_frame_start_timer, jiffies + msecs_to_jiffies(0)); */
	}

	/* Use only setA */
	hw_lme->corex_set_id = COREX_SET_A;
	set_id = hw_lme->corex_set_id;

	dbg_lme(4, "[HW][%s] COREX_SET : %d\n", __func__, set_id);

	ret = __is_hw_lme_init_config(hw_ip, instance, frame, set_id);
	if (ret) {
		mserr_hw("is_hw_lme_init_config is fail", instance, hw_ip);
		return ret;
	}

	spin_lock_irqsave(&lme_out_slock, flag);

	__is_hw_lme_update_block_reg(hw_ip, param_set, instance, set_id);

	spin_unlock_irqrestore(&lme_out_slock, flag);

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		ret = is_lib_isp_shot(hw_ip, &hw_lme->lib[instance], param_set,
				frame->shot);
		if (ret)
			return ret;

		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_RTA_REGS_E);
			ret = __is_hw_lme_set_rta_regs(hw_ip, set_id, instance);
			if (ret) {
				mserr_hw("__is_hw_lme_set_rta_regs is fail", instance, hw_ip);
				goto shot_fail_recovery;
			}
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}
#if IS_ENABLED(CONFIG_LME_FRAME_END_EVENT_NOTIFICATION)
		__is_hw_lme_s_buffer_info(hw_ip, instance);
#endif
	} else {
		dbg_lme(4, "[HW][%s] Not ddk call\n", __func__);
		hw_lme->config[instance].lme_in_w = param_set->dma.cur_input_width;
		hw_lme->config[instance].lme_in_h = param_set->dma.cur_input_height;

		lme_hw_s_mvct(hw_ip->regs[REG_SETA], set_id);
	}

	ret = __is_hw_lme_set_size_regs(hw_ip, param_set, instance, set_id);
	if (ret) {
		mserr_hw("__is_hw_lme_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = LME_RDMA_CACHE_IN_0; i < LME_RDMA_MAX; i++) {
		ret = __is_hw_lme_set_rdma(hw_ip, hw_lme, param_set, instance,
			is_reprocessing, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_lme_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = LME_WDMA_MV_OUT; i < LME_WDMA_MAX; i++) {
		ret = __is_hw_lme_set_wdma(hw_ip, hw_lme, param_set, instance,
				i, set_id);
		if (ret) {
			mserr_hw("__is_hw_lme_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	};

	if (unlikely(debug_lme))
		lme_hw_dump(hw_ip->regs[REG_SETA]);

	ret = is_hw_lme_set_cmdq(hw_ip, instance, set_id);
	if (ret < 0) {
		mserr_hw("is_hw_lme_set_cmdq is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return ret;

shot_fail_recovery:
	if (IS_ENABLED(LME_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw_lme->lib[instance], instance);

	return ret;
}

static int is_hw_lme_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (unlikely(!hw_lme)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_meta(hw_ip, &hw_lme->lib[frame->instance],
				frame);
		if (ret)
			mserr_hw("get_meta fail", frame->instance, hw_ip);
	}

	return ret;
}

static int is_hw_lme_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_lme *hw_lme;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (unlikely(!hw_lme)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(LME_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_lme->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail", instance, hw_ip);
	}

	return ret;
}

static int is_hw_lme_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
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

static int is_hw_lme_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int flag = 0, ret = 0;
	struct is_hw_lme *hw_lme = NULL;
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
	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_lme_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	return ret;
}

static int is_hw_lme_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "Not initialized\n", instance, hw_ip);
		return 0;
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_lme_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	ret = is_lib_isp_reset_recovery(hw_ip, &hw_lme->lib[instance], instance);
	if (ret)
		mserr_hw("is_lib_lme_reset_recovery fail ret(%d)", instance, hw_ip, ret);

	return ret;
}

static int is_hw_lme_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_lme *hw_lme = NULL;
	struct is_hw_lme_iq *iq_set = NULL;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	iq_set = &hw_lme->iq_set[instance];

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

static int is_hw_lme_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	lme_hw_dump(hw_ip->regs[REG_SETA]);

	return 0;
}

static int is_hw_lme_set_config(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, void *config)
{
	struct is_hw_lme *hw_lme = NULL;
	struct is_lme_config *lme_config = (struct is_lme_config *)config;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!config);

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;

	if ((hw_lme->config[instance].lme_in_w != lme_config->lme_in_w)
		|| (hw_lme->config[instance].lme_in_h != lme_config->lme_in_h))
		msinfo_hw("[F:%d] LME in size changed: %d x %d  -> %d x %d", instance, hw_ip, fcount,
			hw_lme->config[instance].lme_in_w, hw_lme->config[instance].lme_in_h,
			lme_config->lme_in_w, lme_config->lme_in_h);

#if (DDK_INTERFACE_VER == 0x1010)
	if (hw_lme->config[instance].first_frame != lme_config->first_frame)
		msinfo_hw("[F:%d] LME first_frame changed: %d -> %d", instance, hw_ip, fcount,
			hw_lme->config[instance].first_frame, lme_config->first_frame);
#endif

	memcpy(&hw_lme->config[instance], config, sizeof(struct is_lme_config));

	return __is_hw_lme_set_dma_cfg(&hw_lme->param_set[instance], instance, config);
}

static int is_hw_lme_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_lme *hw_lme = NULL;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	if (!hw_lme) {
		mserr_hw("failed to get HW LME", instance, hw_ip);
		return -ENODEV;
	}

	if (test_bit(HW_END, &hw_ip->state))
		msinfo_hw("HW end interrupt occur but no end callback", instance, hw_ip);

	/* DEBUG */
	lme_hw_dump(hw_ip->regs[REG_SETA]);
	if (IS_ENABLED(LME_DDK_LIB_CALL))
		ret = is_lib_notify_timeout(&hw_lme->lib[instance], instance);

	return ret;
}

const struct is_hw_ip_ops is_hw_lme_ops = {
	.open			= is_hw_lme_open,
	.init			= is_hw_lme_init,
	.deinit			= is_hw_lme_deinit,
	.close			= is_hw_lme_close,
	.enable			= is_hw_lme_enable,
	.disable		= is_hw_lme_disable,
	.shot			= is_hw_lme_shot,
	.set_param		= is_hw_lme_set_param,
	.get_meta		= is_hw_lme_get_meta,
	.get_cap_meta		= is_hw_lme_get_cap_meta,
	.frame_ndone		= is_hw_lme_frame_ndone,
	.load_setfile		= is_hw_lme_load_setfile,
	.apply_setfile		= is_hw_lme_apply_setfile,
	.delete_setfile		= is_hw_lme_delete_setfile,
	.restore		= is_hw_lme_restore,
	.set_regs		= is_hw_lme_set_regs,
	.dump_regs		= is_hw_lme_dump_regs,
	.set_config		= is_hw_lme_set_config,
	.notify_timeout		= is_hw_lme_notify_timeout,
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;

	hw_ip->ops  = &is_hw_lme_ops;
	hw_ip->dump_for_each_reg = lme_hw_get_reg_struct();
	hw_ip->dump_reg_list_size = lme_hw_get_reg_cnt();

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_lme_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	spin_lock_init(&lme_out_slock);

	return 0;
}
