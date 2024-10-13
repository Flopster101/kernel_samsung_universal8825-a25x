// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo Image Subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw-3aa-v2.h"
#include "is-hw-api-3aa.h"
#include "is-err.h"
#include "is-hw.h"
#include "pablo-obte.h"

static ulong debug_3aa;
module_param(debug_3aa, ulong, 0644);

static void __is_hw_3aa_get_stat_buf(struct is_hw_ip *hw_ip, enum taa_internal_buf_id buf_id,
	struct param_dma_output *dma_out, u32 *dma_out_dva, u32 instance, u32 fcount)
{
	struct is_hw_3aa *hw;
	struct is_framemgr *sd_framemgr;
	struct is_subdev *sd;
	struct is_frame *frame;
	struct taa_param_internal_set *p_internal_set;
	unsigned long flags;

	msdbg_hw(2, "[F%d]%s: buf_id(%d)\n", instance, hw_ip, fcount, __func__, buf_id);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	p_internal_set = &hw->param_internal_set[instance];

	sd = &hw->subdev[buf_id];
	if (!test_bit(IS_SUBDEV_START, &sd->state)) {
		mswarn_hw("[%s] Not in START state 0x%x", instance, hw_ip,
				sd->name, sd->state);
		return;
	}

	sd_framemgr = GET_SUBDEV_I_FRAMEMGR(sd);

	framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
	frame = get_frame(sd_framemgr, FS_FREE);
	if (!frame) {
		frame = get_frame(sd_framemgr, FS_PROCESS);
		if (frame) {
			mswarn_hw("[F%d][%s] overwrite F%d buffer", instance, hw_ip,
					fcount, sd->name, frame->fcount);
		} else {
			mswarn_hw("[F%d][%s] no free buffer", instance, hw_ip,
					fcount, sd->name);
			frame_manager_print_queues(sd_framemgr);
			dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}
	}
	framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);

	if (!frame)
		return;

	/* param set <- is_frame */
	*dma_out_dva = frame->dvaddr_buffer[0];
	/* is_frame <- param_set */
	frame->data_size[0] = dma_out->width * dma_out->height;
	frame->fcount = fcount;

	msdbg_hw(2, "[F%d][%s] index:%d dva %x kva 0x%lx size %d\n", instance, hw_ip,
			fcount, sd->name,
			frame->index, frame->dvaddr_buffer[0],
			frame->kvaddr_buffer[0], frame->data_size[0]);

	framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
	put_frame(sd_framemgr, frame, FS_PROCESS);
	framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);

}

static inline void __is_hw_3aa_put_stat_buf(struct is_hw_ip *hw_ip, enum taa_internal_buf_id buf_id,
	u32 instance, u32 end_fcount)
{
	struct is_hw_3aa *hw;
	struct is_subdev *sd;
	struct is_framemgr *sd_framemgr;
	struct is_frame *frame;
	struct is_priv_buf *pb;
	struct taa_event_data e_data;
	unsigned long flags;
	enum taa_event_id e_id;
	u32 fcount;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	sd = &hw->subdev[buf_id];
	sd_framemgr = GET_SUBDEV_I_FRAMEMGR(sd);

	framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
	while (1) {
		frame = peek_frame(sd_framemgr, FS_PROCESS);

		if (!frame) {
			msdbg_hw(2, "[F%d][%s]: no processing frame\n", instance, hw_ip,
				end_fcount, sd->name);
			framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);
			return;
		}

		fcount = frame->fcount;
		if (fcount == end_fcount) {
			frame = get_frame(sd_framemgr, FS_PROCESS);
			msdbg_hw(2, "[F%d][%s]: find matched buffer", instance, hw_ip,
				end_fcount, sd->name);
			break;
		} else if (fcount < end_fcount) {
			trans_frame(sd_framemgr, frame, FS_FREE);
			mswarn_hw("[F%d][%s]: drop buffer F%d\n", instance, hw_ip,
				end_fcount, sd->name, fcount);
		} else {
			mswarn_hw("[F%d][%s]: no matched buffer F%d", instance, hw_ip,
				end_fcount, sd->name, fcount);
			framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);
			return;
		}
	}
	framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);

	/* cache invalidate */
	pb = frame->pb_output;
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);
	msdbg_hw(2, "[F%d][%s] cache invalidate: kva 0x%lx, size: %d", instance, hw_ip,
		 end_fcount, sd->name, frame->pb_output->kva, frame->pb_output->size);

	e_data.kva = frame->kvaddr_buffer[0];
	e_data.bytes = frame->data_size[0];

	switch (buf_id) {
	case TAA_INTERNAL_BUF_THUMB_PRE:
		e_id = EVENT_TAA_THSTAT_PRE;
		break;
	case TAA_INTERNAL_BUF_THUMB:
		e_id = EVENT_TAA_THSTAT;
		break;
	case TAA_INTERNAL_BUF_RGBYHIST:
		e_id = EVENT_TAA_RGBYHIST;
		break;
	default:
		/* Skip */
		return;
	};

	is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
			fcount, e_id, 0, &e_data);

	msdbg_hw(2, "[F%d][%s] index:%d dva %x kva 0x%lx size %d\n", instance, hw_ip,
			fcount, sd->name,
			frame->index, frame->dvaddr_buffer[0],
			frame->kvaddr_buffer[0], frame->data_size[0]);

	framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
	put_frame(sd_framemgr, frame, FS_FREE);
	framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);

}

static void __is_hw_3aa_flush_stat_buf(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw;
	struct is_subdev *sd;
	struct is_framemgr *sd_framemgr;
	struct is_frame *frame;
	enum taa_internal_buf_id buf_id;
	unsigned long flags;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	for (buf_id = TAA_INTERNAL_BUF_CDAF; buf_id < TAA_INTERNAL_BUF_MAX; buf_id++) {
		sd = &hw->subdev[buf_id];
		if (!test_bit(IS_SUBDEV_START, &sd->state))
			continue;

		sd_framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
		msinfo_hw("[%s] flush stat bufs (%d %d)", instance, hw_ip,
				sd->name,
				sd_framemgr->queued_count[FS_FREE],
				sd_framemgr->queued_count[FS_PROCESS]);

		framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
		frame = peek_frame(sd_framemgr, FS_PROCESS);
		while (frame) {
			trans_frame(sd_framemgr, frame, FS_FREE);
			frame = peek_frame(sd_framemgr, FS_PROCESS);
		}
		framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);
	}
}

static void is_hw_3aa_end_tasklet(unsigned long data)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_3aa *hw;
	enum taa_internal_buf_id buf_id;
	u32 instance, fcount;

	hw_ip = (struct is_hw_ip *)data;
	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	instance = atomic_read(&hw_ip->instance);
	fcount = atomic_read(&hw->end_tasklet_fcount);

	msdbg_hw(2, "[F%d][E]%s:\n", instance, hw_ip, fcount, __func__);

	for (buf_id = TAA_INTERNAL_BUF_THUMB_PRE; buf_id < TAA_INTERNAL_BUF_MAX; buf_id++)
		__is_hw_3aa_put_stat_buf(hw_ip, buf_id, instance, fcount);

	atomic_set(&hw->end_tasklet_fcount, 0);

	msdbg_hw(2, "[F%d][X]%s:\n", instance, hw_ip, fcount, __func__);

}

static inline void __is_hw_3aa_frame_start(struct is_hw_ip *hw_ip, u32 instance, u32 fcount)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_3aa *hw = (struct is_hw_3aa *)hw_ip->priv_info;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	/* shot_fcount -> start_fcount */
	atomic_set(&hw->start_fcount, fcount);

	msdbg_hw(1, "[F%d][E] F.S\n", instance, hw_ip, fcount);

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
				fcount, EVENT_FRAME_START, /* strip_index */ 0, NULL);
	} else {
		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_FRAME_START);
		atomic_add(1, &hw_ip->count.fs);

		if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
			msinfo_hw("[F%d]F.S\n", instance, hw_ip, fcount);

		is_hardware_frame_start(hw_ip, instance);
	}

	msdbg_hw(1, "[F%d][X] F.S\n", instance, hw_ip, fcount);

}

static inline void __is_hw_3aa_frame_end(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_3aa *hw = (struct is_hw_3aa *)hw_ip->priv_info;
	struct is_group *head;
	u32 fcount;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);
	/* start_fcount -> end_fcount */
	fcount = atomic_read(&hw->start_fcount);
	atomic_set(&hw->end_fcount, fcount);

	msdbg_hw(1, "[F%d][E] F.E\n", instance, hw_ip, fcount);

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, hw_ip->group[instance]);
	if (!test_bit(IS_GROUP_OTF_INPUT, &head->state))
		taa_hw_s_global_enable(hw_ip->regs[REG_SETA], false, hw->hw_fro_en);

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
				fcount, EVENT_FRAME_END, /* strip_index */ 0, NULL);
		if (!atomic_read(&hw->end_tasklet_fcount)) {
			atomic_set(&hw->end_tasklet_fcount, fcount);
			tasklet_schedule(&hw->end_tasklet);
		} else {
			mswarn_hw("[F%d] skip end_tasklet\n", instance, hw_ip, fcount);
		}
	} else {
		_is_hw_frame_dbg_trace(hw_ip, fcount, DEBUG_POINT_FRAME_END);
		atomic_add(1, &hw_ip->count.fe);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F%d]F.E\n", instance, hw_ip, fcount);

		is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
				IS_SHOT_SUCCESS, true);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe))
			mserr_hw("[F%d]fs %d fe %d", instance, hw_ip,
					fcount,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe));

		wake_up(&hw_ip->status.wait_queue);
	}

	msdbg_hw(1, "[F%d][X] F.E\n", instance, hw_ip, fcount);

}

static inline void __is_hw_3aa_frame_caf(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_3aa *hw;
	struct is_subdev *sd;
	struct is_framemgr *sd_framemgr;
	struct is_frame *frame;
	struct taa_event_data e_data;
	unsigned long flags;
	u32 fcount;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	fcount = atomic_read(&hw->start_fcount);

	sd = &hw->subdev[TAA_INTERNAL_BUF_CDAF];
	sd_framemgr = GET_SUBDEV_I_FRAMEMGR(sd);
	framemgr_e_barrier_irqs(sd_framemgr, FMGR_IDX_0, flags);
	frame = peek_frame(sd_framemgr, FS_FREE);
	framemgr_x_barrier_irqr(sd_framemgr, FMGR_IDX_0, flags);

	if (!frame) {
		mserr_hw("[F%d][%s]%s: no free buf\n", instance, hw_ip, fcount, sd->name, __func__);
		return;
	}

	e_data.kva = frame->kvaddr_buffer[0];
	ret = taa_hw_g_cdaf_stat(hw_ip->regs[REG_SETA], (u32 *)e_data.kva, &e_data.bytes);
	if (ret) {
		mserr_hw("[F%d]%s: failed to get cdaf stat\n", instance, hw_ip, fcount, __func__);
		return;
	}

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
				fcount, EVENT_TAA_CDAF, /* strip_index */ 0, &e_data);
	}
}

static inline void __is_hw_3aa_frame_drop(struct is_hw_ip *hw_ip)
{
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	framemgr = hw_ip->framemgr;
	if (framemgr) {
		framemgr_e_barrier(framemgr, 0);
		/* Drop Current frame */
		frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
		if (frame)
			frame->result = IS_SHOT_CONFIG_LOCK_DELAY;
		/* Drop Next frame */
		frame = peek_frame(framemgr, FS_HW_CONFIGURE);
		if (frame)
			frame->result = IS_SHOT_CONFIG_LOCK_DELAY;
		framemgr_x_barrier(framemgr, 0);
	}
}

static int is_hw_3aa_isr(u32 id, void *data)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_3aa *hw;
	u32 status, instance, fcount;
	struct is_group *head;
	u32 event_state, fs, cl, fc, fe, err;
	unsigned long int_count;

	hw_ip = (struct is_hw_ip *)data;
	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		err("failed to get 3AA");
		return 0;
	}
	instance = atomic_read(&hw_ip->instance);
	fcount = atomic_read(&hw_ip->fcount);

	atomic_inc(&hw->isr_run_count);

	status = taa_hw_g_int_status(hw_ip->regs[REG_SETA], id, true, hw->hw_fro_en);

	msdbg_hw(2, "[F%d] 3AA irq id: %d, status: 0x%x\n", instance, hw_ip, fcount, id, status);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[INT] Ignore ISR before HW config 0x%lx\n",
				instance, hw_ip, status);

		goto out;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]id %d, invalid HW state 0x%x, status 0x%x\n", instance, hw_ip,
				fcount, id, hw_ip->state, status);
		goto out;
	}

	if (!hw_ip->group[instance]) {
		mserr_hw("[F%d]group is null, id:%d, status:0x%08X\n", instance, hw_ip,
				fcount, id, status);
		goto out;
	}

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, hw_ip->group[instance]);
	if (!head) {
		mserr_hw("[F%d] head group is null, id:%d, status:0x%08X\n", instance, hw_ip,
			fcount, id, status);
		goto out;
	}

	err = taa_hw_occurred(status, id, TAA_EVENT_ERR);
	if (err) {
		mserr_hw("[F%d] hw error! 0x%x\n", instance, hw_ip, fcount, status);
		taa_hw_print_error(id, err);
		taa_hw_dump(hw_ip->regs[REG_SETA]);
		taa_hw_dump_zsl(hw_ip->regs[REG_EXT2]);
		taa_hw_dump_strp(hw_ip->regs[REG_EXT3]);
	}

	fs = taa_hw_occurred(status, id, TAA_EVENT_FRAME_START);
	cl = taa_hw_occurred(status, id, TAA_EVENT_FRAME_LINE);
	fc = taa_hw_occurred(status, id, TAA_EVENT_FRAME_COREX);
	fe = taa_hw_occurred(status, id, TAA_EVENT_FRAME_END);

	int_count = hweight_long(fs | cl | fe);
	if (int_count > 1) {
		mswarn_hw("[F%d]interrupt overlapped!! fs: %d, cl: %d, fc: %d fe: %d",
			instance, hw_ip, fcount, fs > 0, cl > 0, fc > 0, fe > 0);
	}

	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		if (fc)
			int_count++;
		while (int_count--) {
			event_state = atomic_read(&hw->event_state);
			switch (event_state) {
			case TAA_EVENT_FRAME_START:
				if (cl) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_LINE);
					atomic_add(1, &hw_ip->count.cl);
					if (fc) {
						mswarn_hw("[F%d] clear invalid corex_end event",
							  instance, hw_ip, fcount);
						int_count--;
						fc = 0;
					}
					msdbg_hw(1, "[F%d][E] C.L\n", instance, hw_ip, fcount);
					is_hardware_config_lock(hw_ip, instance, fcount);
					msdbg_hw(1, "[F%d][X] C.L\n", instance, hw_ip, fcount);
					cl = 0;
				}
				break;
			case TAA_EVENT_FRAME_LINE:
				if (fc) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_COREX);
					fc = 0;
				} else if (fe) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_END);
					mserr_hw("[F%d] config lock isr is delayed",
						instance, hw_ip, fcount);
					__is_hw_3aa_frame_drop(hw_ip);
					__is_hw_3aa_frame_end(hw_ip, instance);
					fe = 0;
				}
				break;
			case TAA_EVENT_FRAME_COREX:
				if (fe) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_END);
					__is_hw_3aa_frame_end(hw_ip, instance);
					fe = 0;
				}
				break;
			case TAA_EVENT_FRAME_END:
				if (fs) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_START);
					__is_hw_3aa_frame_start(hw_ip, instance, fcount);
					fs = 0;
				}
				break;
			default:
				mswarn_hw("[F%d] invalid state: %d", instance, hw_ip,
					fcount, event_state);
				break;
			}
		}
	} else {
		fc = 0;
		while (int_count--) {
			event_state = atomic_read(&hw->event_state);
			switch (event_state) {
			case TAA_EVENT_FRAME_START:
				if (fe) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_END);
					__is_hw_3aa_frame_end(hw_ip, instance);
					fe = 0;
				}
				break;
			case TAA_EVENT_FRAME_END:
				if (fs) {
					atomic_set(&hw->event_state, TAA_EVENT_FRAME_START);
					__is_hw_3aa_frame_start(hw_ip, instance, fcount);
					fs = 0;
				}
				break;
			default:
				mswarn_hw("[F%d] invalid state: %d", instance, hw_ip,
					fcount, event_state);
				break;
			}
		}
	}

	if (fs || cl || fc || fe) {
		mserr_hw("[F%d] skip isr fs: %d, cl: %d, fc : %d, fe: %d",
		instance, hw_ip, fcount,
		fs > 0, cl > 0, fc > 0, fe > 0);
	}

	if (taa_hw_occurred(status, id, TAA_EVENT_FRAME_CDAF))
		__is_hw_3aa_frame_caf(hw_ip, instance);

out:
	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	return 0;
}

static int is_hw_csis_dma_isr(u32 id, void *data)
{
	int ret;
	struct is_hw_ip *hw_ip;
	struct is_hw_3aa *hw;
	u32 status, instance, hw_fcount;
	void __iomem *base;
	enum taa_dma_id dma_id;
	struct param_dma_output dma_output;

	hw_ip = (struct is_hw_ip *)data;
	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		err("failed to get 3AA");
		return 0;
	}
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	atomic_inc(&hw->isr_run_count);

	if (id == INTR_HWIP3) {
		base = hw_ip->regs[REG_EXT2];
		dma_id = TAA_WDMA_ZSL;
	} else if (id == INTR_HWIP4) {
		base = hw_ip->regs[REG_EXT3];
		dma_id = TAA_WDMA_STRP;
	} else {
		mserr_hw("%s invalid id (%d)\n", instance, hw_ip, __func__, id);
		ret = -EINVAL;
		goto err_out;
	}

	status = taa_hw_g_csis_int_status(base, id, true);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[DMA] Ignore ISR before HW config 0x%lx\n",
				instance, hw_ip, status);
		goto out;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("[F%d] %s invalid HW state 0x%x status 0x%x\n", instance, hw_ip,
				hw_fcount, irq_name[id], hw_ip->state, status);
		goto out;
	}

	if (taa_hw_occurred(status, id, TAA_EVENT_FRAME_END)) {
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END, id - INTR_HWIP3);
		msdbg_hw(2, "[F%d] %s dma end! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);
	}

	if (taa_hw_occurred(status, id, TAA_EVENT_FRAME_START)) {
		_is_hw_frame_dbg_ext_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START, id - INTR_HWIP3);
		msdbg_hw(2, "[F%d] %s dma start! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);
	}

	if (taa_hw_occurred(status, id, TAA_EVENT_WDMA_ABORT_DONE))
		msinfo_hw("[F%d] %s dma abort done! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);

	if (taa_hw_occurred(status, id, TAA_EVENT_WDMA_FRAME_DROP))
		msinfo_hw("[F%d] %s dma frame drop! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);

	if (taa_hw_occurred(status, id, TAA_EVENT_WDMA_CFG_ERR)) {
		mserr_hw("[F%d] %s dma cfg error! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);
		/* disable dma */
		memset(&dma_output, 0, sizeof(dma_output));
		ret = taa_hw_s_wdma(&hw->dma[dma_id], base, &dma_output, NULL, 0);
		if (ret)
			mserr_hw("failed to disable wdma(%d)", instance, hw_ip, dma_id);
		/* abort dma */
		taa_hw_abort_wdma(&hw->dma[dma_id], base);
	}

	if (taa_hw_occurred(status, id, TAA_EVENT_WDMA_FIFOFULL_ERR))
		mserr_hw("[F%d] %s dma fifo full! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);

	if (taa_hw_occurred(status, id, TAA_EVENT_WDMA_FSTART_IN_FLUSH_ERR))
		mserr_hw("[F%d] %s dma fstart in flush! 0x%x\n", instance, hw_ip,
			hw_fcount, irq_name[id], status);

out:
	ret = 0;

err_out:
	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	return ret;
}

static void is_hw_3aa_wait_isr_done(struct is_hw_ip *hw_ip)
{
	struct is_hw_3aa *hw = (struct is_hw_3aa *)hw_ip->priv_info;
	long timetowait;

	taa_hw_wait_isr_clear(hw_ip->regs[REG_SETA], hw_ip->regs[REG_EXT2],
			hw_ip->regs[REG_EXT3], hw->hw_fro_en);

	timetowait = wait_event_timeout(hw->isr_wait_queue,
			!atomic_read(&hw->isr_run_count),
			IS_HW_STOP_TIMEOUT);
	if (!timetowait)
		mserr_hw("wait isr done timeout (%ld)",
				atomic_read(&hw_ip->instance), hw_ip, timetowait);
}

static void __is_hw_3aa_core_init(struct is_hw_ip *hw_ip)
{
	taa_hw_s_corex_init(hw_ip->regs[REG_SETA]);
	taa_hw_s_control_init(hw_ip->regs[REG_SETA]);
	taa_hw_enable_int(hw_ip->regs[REG_SETA]);
	taa_hw_enable_csis_int(hw_ip->regs[REG_EXT2]);
	taa_hw_enable_csis_int(hw_ip->regs[REG_EXT3]);
	taa_hw_s_cinfifo_init(hw_ip->regs[REG_SETA]);
	taa_hw_s_module_init(hw_ip->regs[REG_SETA]);
}

static int __nocfi is_hw_3aa_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret;
	struct is_hw_3aa *hw;
	u32 reg_cnt = taa_hw_g_reg_cnt();
	u32 dma_id;
	struct is_device_ischain *device;
	struct is_subdev *sd;
	struct taa_buf_info info;
	enum taa_internal_buf_id buf_id;

	msdbg_hw(2, "[%s] core: 0x%lx context0: 0x%lx, zsl: 0x%lx, strp: 0x%lx\n", instance, hw_ip,
		hw_ip->regs[REG_SETA], hw_ip->regs[REG_EXT1],
		hw_ip->regs[REG_EXT2], hw_ip->regs[REG_EXT3]);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HW3AA");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw = vzalloc(sizeof(struct is_hw_3aa));
	if (!hw) {
		mserr_hw("failed to alloc is_hw_3aa", instance, hw_ip);
		ret = -ENOMEM;
		goto err_hw_alloc;
	}
	hw_ip->priv_info = (void *)hw;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = get_lib_func(LIB_FUNC_3AA, (void **)&hw->lib_func);
		if (hw->lib_func == NULL) {
			mserr_hw("failed to get lib_func", instance, hw_ip);
			is_load_clear();
			ret = -EINVAL;
			goto err_lib_func;
		}
	}

	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw->lib_support = is_get_lib_support();

	hw->lib[instance].func = hw->lib_func;
	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = is_lib_isp_chain_create(hw_ip, &hw->lib[instance], instance);
		if (ret) {
			mserr_hw("chain create fail", instance, hw_ip);
			ret = -EINVAL;
			goto err_chain_create;
		}
	}

	hw->iq_set.regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
	if (!hw->iq_set.regs) {
		mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_regs_alloc;
	}

	hw->cur_iq_set.regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
	if (!hw->cur_iq_set.regs) {
		mserr_hw("failed to alloc cur_iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_cur_regs_alloc;
	}

	for (dma_id = TAA_WDMA_THSTAT_PRE; dma_id < TAA_DMA_MAX; dma_id++) {
		if (taa_hw_create_dma(hw_ip->regs[REG_SETA], dma_id,
				&hw->dma[dma_id])) {
			mserr_hw("[D%d] create_dma error", instance, hw_ip,
					dma_id);
			ret = -ENODATA;
			goto err_dma_create;
		}
	}

	for (buf_id = TAA_INTERNAL_BUF_CDAF; buf_id < TAA_INTERNAL_BUF_MAX; buf_id++) {
		sd = &hw->subdev[buf_id];

		if (taa_hw_g_buf_info(buf_id, &info))
			continue;

		ret =  is_subdev_probe(sd, instance, ENTRY_INTERNAL,
			taa_internal_buf_name[buf_id], NULL);
		if (ret) {
			mserr_hw("[%s] failed to is_subdev_probe. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd;
		}

		ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN,
						group->leader.vid, sd);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_open. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd;
		}

		ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, sd,
				info.w, info.h, info.bpp, NONE, 0, info.num, sd->name);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_s_format. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd;
		}

		ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, sd);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_start. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd;
		}

		msdbg_hw(2, "[%s] is_subdev_internal_start", instance, hw_ip,
					sd->name, ret);
	}

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);

	hw->hw_fro_en = false;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw->isr_run_count, 0);
	init_waitqueue_head(&hw->isr_wait_queue);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_sd:
	while (buf_id-- > 0) {
		sd = &hw->subdev[buf_id];
		is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, sd);
		is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, sd);
	}

err_dma_create:
	vfree(hw->cur_iq_set.regs);
	hw->cur_iq_set.regs = NULL;

err_cur_regs_alloc:
	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;

err_regs_alloc:
	if (IS_ENABLED(TAA_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw);
	hw_ip->priv_info = NULL;

err_hw_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_3aa_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	struct is_device_ischain *device;
	struct is_hw_3aa *hw;
	u32 seed, f_type;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("is_hw_3aa is null ", instance, hw_ip);
		return -ENODATA;
	}

	hw->lib[instance].object = NULL;
	hw->lib[instance].func = hw->lib_func;
	hw->param_set[instance].reprocessing = flag;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		if (is_lib_isp_object_create(hw_ip, &hw->lib[instance], instance,
				(u32)flag, f_type)) {
			mserr_hw("object create fail", instance, hw_ip);
			return -EINVAL;
		}
	}

	group->hw_ip = hw_ip;
	msinfo_hw("[%s] Binding\n", instance, hw_ip, group_id_name[group->id]);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_init_3aa(instance, flag);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		taa_hw_s_crc(hw_ip->regs[REG_SETA], seed);

	set_bit(HW_INIT, &hw_ip->state);

	return 0;
}

static int is_hw_3aa_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("is_hw_3aa is null ", instance, hw_ip);
		return -ENODATA;
	}

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		hw = (struct is_hw_3aa *)hw_ip->priv_info;
		is_lib_isp_object_destroy(hw_ip, &hw->lib[instance], instance);

		hw->lib[instance].object = NULL;
	}

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_deinit_3aa(instance);

	return 0;
}

static int is_hw_3aa_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw;
	struct is_hardware *hardware;
	struct is_group *group;
	struct is_device_ischain *device;
	struct is_subdev *sd;
	enum taa_internal_buf_id buf_id;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("is_hw_3aa is null ", instance, hw_ip);
		return -ENODATA;
	}

	hardware = hw_ip->hardware;
	group = hw_ip->group[instance];

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	if (IS_ENABLED(TAA_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw->lib[instance], instance);

	/* disable interrupt */
	taa_hw_disable_int(hw_ip->regs[REG_SETA]);
	taa_hw_disable_csis_int(hw_ip->regs[REG_EXT2]);
	taa_hw_disable_csis_int(hw_ip->regs[REG_EXT3]);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	for (buf_id = TAA_INTERNAL_BUF_CDAF; buf_id < TAA_INTERNAL_BUF_MAX; buf_id++) {
		sd = &hw->subdev[buf_id];
		is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN, sd);
		is_subdev_internal_close(device, IS_DEVICE_ISCHAIN, sd);
	}

	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;
	vfree(hw->cur_iq_set.regs);
	hw->cur_iq_set.regs = NULL;
	vfree(hw);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	if (atomic_read(&hardware->slot_rsccount[group->slot]) <= 1) {
		if (taa_hw_s_c0_reset(hw_ip->regs[REG_EXT1]))
			mserr_hw("reset fail", instance, hw_ip);
	}

	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_3aa_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int i, max_num;
	struct is_hw_3aa *hw;
	struct is_hardware *hardware;
	struct exynos_platform_is *pdata;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	hardware = hw_ip->hardware;
	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.taa;

	for (i = 0; i < max_num; i++) {
		int hw_id = DEV_HW_3AA0 + i;
		int hw_slot = is_hw_slot_id(hw_id);
		struct is_hw_ip *hw_ip_3aa;

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			return -EINVAL;
		}

		hw_ip_3aa = &hardware->hw_ip[hw_slot];

		if (hw_ip_3aa && test_bit(HW_RUN, &hw_ip_3aa->state))
			break;
	}
	if (i == max_num) {
		if (taa_hw_s_c0_reset(hw_ip->regs[REG_EXT1])) {
			mserr_hw("reset fail", instance, hw_ip);
			return -EINVAL;
		}
		taa_hw_s_c0_control_init(hw_ip->regs[REG_EXT1]);
	}
	__is_hw_3aa_core_init(hw_ip);

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	atomic_set(&hw->event_state, TAA_EVENT_FRAME_END);
	atomic_set(&hw->start_fcount, 0);
	atomic_set(&hw->end_fcount, 0);
	atomic_set(&hw->end_tasklet_fcount, 0);
	tasklet_init(&hw->end_tasklet, is_hw_3aa_end_tasklet, (unsigned long)hw_ip);

	set_bit(HW_RUN, &hw_ip->state);

	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_3aa_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 i;
	long timetowait;
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("%s: Vvalid(%d)\n", instance, hw_ip, __func__,
		atomic_read(&hw_ip->status.Vvalid));

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	/*
	 * Vvalid state is only needed to be checked when HW works at same instance.
	 * If HW works at different instance, skip Vvalid checking.
	 */
	if (atomic_read(&hw_ip->instance) == instance) {
		if (!hw->hw_fro_en) {
			timetowait = wait_event_timeout(hw_ip->status.wait_queue,
					!atomic_read(&hw_ip->status.Vvalid),
					IS_HW_STOP_TIMEOUT);

			if (!timetowait) {
				mserr_hw("wait FRAME_END timeout (%ld)", instance,
						hw_ip, timetowait);
				ret = -ETIME;
			}
		}

		is_hw_3aa_wait_isr_done(hw_ip);
	}

	/* TODO: need to divide each task index */
	for (i = 0; i < TASK_INDEX_MAX; i++) {
		FIMC_BUG(!hw->lib_support);
		if (hw->lib_support->task_taaisp[i].task == NULL)
			serr_hw("task is null", hw_ip);
		else
			kthread_flush_worker(&hw->lib_support->task_taaisp[i].worker);
	}

	/*
	 * It doesn't matter, if object_stop command is always called.
	 * That is because DDK can prevent object_stop that is over called.
	 */
	if (IS_ENABLED(TAA_DDK_LIB_CALL))
		is_lib_isp_stop(hw_ip, &hw->lib[instance], instance, 1);

	if (atomic_read(&hw_ip->run_rsccount) == 0) {
		mswarn_hw("run_rsccount is not paired.\n", instance, hw_ip);
		return 0;
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	taa_hw_s_fro_reset(hw_ip->regs[REG_SETA], hw_ip->regs[REG_EXT2],
			hw_ip->regs[REG_EXT3]);

	taa_hw_s_global_enable(hw_ip->regs[REG_SETA], false, hw->hw_fro_en);

	tasklet_kill(&hw->end_tasklet);
	__is_hw_3aa_flush_stat_buf(hw_ip, instance);

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void __is_hw_3aa_update_param(struct is_hw_ip *hw_ip,
		struct is_param_region *param_region, struct taa_param_set *param_set,
		IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct is_hw_3aa *hw;
	struct taa_param *param;
	struct taa_param_internal_set *p_internal_set;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	param = &param_region->taa;
	p_internal_set = &hw->param_internal_set[instance];

	param_set->instance_id = instance;

	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&param_set->sensor_config, &param_region->sensor.config,
			sizeof(struct param_sensor_config));

	/* check input */
	if (test_bit(PARAM_3AA_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));

	/* check output*/
	if (test_bit(PARAM_3AA_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_3AA_VDMA4_OUTPUT, pmap))
		memcpy(&param_set->dma_output_before_bds, &param->vdma4_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_VDMA2_OUTPUT, pmap))
		memcpy(&param_set->dma_output_after_bds, &param->vdma2_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_FDDMA_OUTPUT, pmap))
		memcpy(&param_set->dma_output_efd, &param->efd_output,
			sizeof(struct param_dma_output));

	if (test_bit(PARAM_3AA_MRGDMA_OUTPUT, pmap))
		memcpy(&param_set->dma_output_mrg, &param->mrg_output,
			sizeof(struct param_dma_output));

#if defined(ENABLE_ORBDS)
	if (test_bit(PARAM_3AA_ORBDS_OUTPUT, pmap))
		memcpy(&param_set->dma_output_orbds, &param->ods_output,
			sizeof(struct param_dma_output));
#endif

#if defined(LOGICAL_VIDEO_NODE)
	if (test_bit(PARAM_3AA_DRCG_OUTPUT, pmap))
		memcpy(&param_set->dma_output_drcgrid, &param->drc_output,
			sizeof(struct param_dma_output));
#endif

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_3AA_STRIPE_INPUT, pmap))
		memcpy(&param_set->stripe_input, &param->stripe_input,
				sizeof(struct param_stripe_input));

	/* use bcrop1 instead of bcrop2 for drc */
	if (param_set->dma_output_after_bds.crop_enable) {
		param_set->otf_input.bayer_crop_offset_x = param_set->dma_output_after_bds.dma_crop_offset_x;
		param_set->otf_input.bayer_crop_offset_y = param_set->dma_output_after_bds.dma_crop_offset_y;
		param_set->otf_input.bayer_crop_width = param_set->dma_output_after_bds.dma_crop_width;
		param_set->otf_input.bayer_crop_height = param_set->dma_output_after_bds.dma_crop_height;

		param_set->dma_output_before_bds.width = param_set->dma_output_after_bds.dma_crop_width;
		param_set->dma_output_before_bds.height = param_set->dma_output_after_bds.dma_crop_height;

		param_set->dma_output_after_bds.width = param_set->dma_output_after_bds.dma_crop_width;
		param_set->dma_output_after_bds.height = param_set->dma_output_after_bds.dma_crop_height;
		param_set->dma_output_after_bds.crop_enable = 0;
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_FDPIG_WDMA, &debug_3aa)))
		param_set->dma_output_efd.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_ZSL_WDMA, &debug_3aa)))
		param_set->dma_output_before_bds.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_STRP_WDMA, &debug_3aa)))
		param_set->dma_output_after_bds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
}

static int __is_hw_3aa_update_internal_param(struct is_hw_ip *hw_ip, u32 instance, u32 fcount)
{
	int ret;
	struct is_hw_3aa *hw;
	struct taa_param_set *p_set;
	struct taa_param_internal_set *p_internal_set;
	struct is_3aa_config *iq_config;
	struct param_dma_output *dma_out;
	enum taa_dma_id dma_id;
	enum taa_internal_buf_id buf_id;
	u32 *dma_out_dva;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];
	p_internal_set = &hw->param_internal_set[instance];
	iq_config = &hw->iq_config;

	/* default : disable */
	dma_out = &p_internal_set->dma_output_thstat_pre;
	if (!iq_config->thstatpre_bypass) {
		dma_id = TAA_WDMA_THSTAT_PRE;
		buf_id = TAA_INTERNAL_BUF_THUMB_PRE;
		dma_out_dva = &p_internal_set->output_dva_thstat_pre[0];

		ret = taa_hw_g_stat_param(dma_id, iq_config->thstatpre_grid_w,
			iq_config->thstatpre_grid_h, dma_out);
		if (!ret) {
			dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			__is_hw_3aa_get_stat_buf(hw_ip, buf_id, dma_out, dma_out_dva,
				instance, fcount);
		}
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_THSTAT_PRE_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	/* default : disable */
	dma_out = &p_internal_set->dma_output_thstat;
	if (!iq_config->thstat_bypass) {
		dma_id = TAA_WDMA_THSTAT;
		buf_id = TAA_INTERNAL_BUF_THUMB;
		dma_out_dva = &p_internal_set->output_dva_thstat[0];

		ret = taa_hw_g_stat_param(dma_id, iq_config->thstat_grid_w,
			iq_config->thstat_grid_h, dma_out);
		if (!ret) {
			dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			__is_hw_3aa_get_stat_buf(hw_ip, buf_id, dma_out, dma_out_dva,
				instance, fcount);
		}
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_THSTAT_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	/* default : disable */
	dma_out = &p_internal_set->dma_output_rgbyhist;
	if (!iq_config->rgbyhist_bypass) {
		dma_id = TAA_WDMA_RGBYHIST;
		buf_id = TAA_INTERNAL_BUF_RGBYHIST;
		dma_out_dva = &p_internal_set->output_dva_rgbyhist[0];

		ret = taa_hw_g_stat_param(dma_id, iq_config->rgby_hist_num,
			iq_config->rgby_bin_num, dma_out);
		if (!ret) {
			dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			__is_hw_3aa_get_stat_buf(hw_ip, buf_id, dma_out, dma_out_dva,
				instance, fcount);
		}
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_RGBYHIST_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	/* default : from hal */
	dma_out = &p_set->dma_output_drcgrid;
	if (iq_config->drcclct_bypass)
		p_set->dma_output_drcgrid.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	else {
		dma_id = TAA_WDMA_DRC;
		ret = taa_hw_g_stat_param(dma_id, iq_config->drc_grid_w,
			iq_config->drc_grid_h, dma_out);
		if (ret)
			dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_DRC_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	/* default : from hal */
	dma_out = &p_internal_set->dma_output_orbds0;
	if (iq_config->orbds_bypass) {
		p_internal_set->dma_output_orbds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	} else {
		dma_id = TAA_WDMA_ORBDS0;
		ret = taa_hw_g_stat_param(dma_id, iq_config->orbds0_w,
			iq_config->orbds0_h, dma_out);
		if (ret)
			dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		else
			p_internal_set->output_dva_orbds0[0] = p_set->output_dva_orbds[0];
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_ORBDS0_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	/* default : from hal */
	dma_out = &p_internal_set->dma_output_orbds1;
	if (iq_config->orbds_bypass) {
		p_internal_set->dma_output_orbds1.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	} else {
		dma_id = TAA_WDMA_ORBDS1;
		ret = taa_hw_g_stat_param(dma_id, iq_config->orbds1_w,
			iq_config->orbds1_h, dma_out);
		if (ret)
			dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		else
			p_internal_set->output_dva_orbds1[0] = p_set->output_dva_orbds[1];
	}

	if (unlikely(test_bit(TAA_DEBUG_DISABLE_ORBDS1_WDMA, &debug_3aa)))
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	return 0;
}

static void __is_hw_3aa_s_iq_regs(struct is_hw_ip *hw_ip, struct is_hw_3aa *hw)
{
	void __iomem *base, *corex_base;
	struct is_hw_3aa_iq *iq_set;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size, i;

	dbg_hw(2, "[%s] is called\n", __func__);

	corex_base = hw_ip->regs[REG_SETA] + GET_COREX_OFFSET(COREX_SET_A);
	base = hw_ip->regs[REG_SETA];
	iq_set = &hw->iq_set;

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		dbg_hw(3, "curr iq set size: %d-> new iq set size: %d\n",
			hw->cur_iq_set.size, iq_set->size);

		regs = iq_set->regs;
		regs_size = iq_set->size;
		if (hw->cur_iq_set.size != regs_size ||
			memcmp(hw->cur_iq_set.regs, regs,
				sizeof(struct cr_set) * regs_size) != 0) {

			if (unlikely(test_bit(TAA_DEBUG_SET_REG, &debug_3aa))) {
				for (i = 0; i < regs_size; i++)
					info_hw("%s: [0x%08x] 0x%08x\n", __func__,
						iq_set->regs[i].reg_addr, iq_set->regs[i].reg_data);
			}

			for (i = 0; i < regs_size; i++) {
				if (taa_hw_is_corex(regs[i].reg_addr))
					writel_relaxed(regs[i].reg_data, corex_base + regs[i].reg_addr);
				else
					writel_relaxed(regs[i].reg_data, base + regs[i].reg_addr);
			}

			memcpy(hw->cur_iq_set.regs, regs,
					sizeof(struct cr_set) * regs_size);

			hw->cur_iq_set.size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);
}

static void __is_hw_3aa_bypass_iq_module(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw;
	struct taa_param_set *p_set;
	struct taa_param_internal_set *p_internal_set;
	struct is_3aa_config *iq_config;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];
	p_internal_set = &hw->param_internal_set[instance];
	iq_config = &hw->iq_config;

	memset(&hw->iq_config, 0x0, sizeof(struct is_3aa_config));
	iq_config->thstatpre_bypass = 1;
	iq_config->thstat_bypass = 1;
	iq_config->rgbyhist_bypass = 1;
	iq_config->drcclct_bypass = 1;
	iq_config->orbds_bypass = 1;

	taa_hw_s_module_bypass(hw_ip->regs[REG_SETA]);
}

static int __is_hw_3aa_set_fro(struct is_hw_ip *hw_ip, u32 instance)
{
	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	return taa_hw_s_fro(hw_ip->regs[REG_SETA], (hw_ip->num_buffers & 0xffff));
}

#define GET_HEIGHT_RATIO(height, percent) (height * percent / 100)

static int __is_hw_3aa_set_control(struct is_hw_ip *hw_ip, struct taa_param_set *param_set,
	u32 instance)
{
	struct taa_control_config config;
	struct is_group *head;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (param_set->dma_output_mrg.cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		config.zsl_input_type = TAA_ZSL_INPUT_RECON;
		config.zsl_en = true;
	} else if (param_set->dma_output_before_bds.cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		config.zsl_input_type = TAA_ZSL_INPUT_BCROP1;
		config.zsl_en = true;
	} else {
		config.zsl_input_type = TAA_ZSL_INPUT_BCROP1;
		config.zsl_en = false;
	}

	if (param_set->dma_output_after_bds.cmd == DMA_OUTPUT_COMMAND_ENABLE)
		config.strp_en = true;
	else
		config.strp_en = false;

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, hw_ip->group[instance]);
	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		if (param_set->sensor_config.max_target_fps < 45)
			config.int_row_cord = GET_HEIGHT_RATIO(param_set->otf_input.height, 60);
		else
			config.int_row_cord = GET_HEIGHT_RATIO(param_set->otf_input.height, 50);
	} else {
		config.int_row_cord = 0;
	}

	taa_hw_s_control_config(hw_ip->regs[REG_SETA], &config);

	return 0;
}

static int __is_hw_3aa_s_size_regs(struct is_hw_ip *hw_ip, struct taa_param_set *param_set,
				   u32 instance)
{
	struct is_hw_3aa *hw;
	void __iomem *base;
	struct is_3aa_config *iq_config;
	struct taa_size_config config;
	u32 calibrated_width, calibrated_height;
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;
	u32 binned_sensor_width, binned_sensor_height;
	u32 sensor_crop_width, sensor_crop_height;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	iq_config = &hw->iq_config;

	calibrated_width = param_set->sensor_config.calibrated_width;
	calibrated_height = param_set->sensor_config.calibrated_height;
	sensor_binning_ratio_x = param_set->sensor_config.sensor_binning_ratio_x;
	sensor_binning_ratio_y = param_set->sensor_config.sensor_binning_ratio_y;
	binned_sensor_width = calibrated_width * 1000 / sensor_binning_ratio_x;
	binned_sensor_height = calibrated_height * 1000 / sensor_binning_ratio_y;
	sensor_crop_width = param_set->sensor_config.width;
	sensor_crop_height = param_set->sensor_config.height;

	config.sensor_calibrated.w = calibrated_width;
	config.sensor_calibrated.h = calibrated_height;
	if ((iq_config->sensor_center_x < calibrated_width) && iq_config->sensor_center_x)
		config.sensor_center_x = iq_config->sensor_center_x;
	else
		config.sensor_center_x = calibrated_width / 2;
	if ((iq_config->sensor_center_y < calibrated_height) &&
		iq_config->sensor_center_y)
		config.sensor_center_y = iq_config->sensor_center_y;
	else
		config.sensor_center_y = calibrated_height / 2;
	config.sensor_binning_x = sensor_binning_ratio_x;
	config.sensor_binning_y = sensor_binning_ratio_y;
	config.sensor_crop.x =
		((binned_sensor_width - sensor_crop_width) >> 1) & (~0x1);
	config.sensor_crop.y =
		((binned_sensor_height - sensor_crop_height) >> 1) & (~0x1);
	config.sensor_crop.w = sensor_crop_width;
	config.sensor_crop.h = sensor_crop_height;

	config.bcrop0.x = 0;
	config.bcrop0.y = 0;
	config.bcrop0.w = param_set->otf_input.width;
	config.bcrop0.h = param_set->otf_input.height;

	config.bcrop1.x = param_set->otf_input.bayer_crop_offset_x;
	config.bcrop1.y = param_set->otf_input.bayer_crop_offset_y;
	config.bcrop1.w = param_set->otf_input.bayer_crop_width;
	config.bcrop1.h = param_set->otf_input.bayer_crop_height;

	if (param_set->dma_output_after_bds.cmd == DMA_OUTPUT_COMMAND_ENABLE) {
		config.bds.w = param_set->dma_output_after_bds.width;
		config.bds.h = param_set->dma_output_after_bds.height;
	} else {
		config.bds.w = config.bcrop1.w;
		config.bds.h = config.bcrop1.h;
	}

	config.bcrop2.x = 0;
	config.bcrop2.y = 0;
	config.bcrop2.w = config.bds.w;
	config.bcrop2.h = config.bds.h;

	config.fdpig_en = param_set->dma_output_efd.cmd;
	config.fdpig_crop_en = (config.fdpig_en) ? param_set->dma_output_efd.crop_enable : 0;
	config.fdpig_crop.x = param_set->dma_output_efd.dma_crop_offset_x;
	config.fdpig_crop.y = param_set->dma_output_efd.dma_crop_offset_y;
	config.fdpig_crop.w = param_set->dma_output_efd.dma_crop_width;
	config.fdpig_crop.h = param_set->dma_output_efd.dma_crop_height;
	config.fdpig.w = param_set->dma_output_efd.width;
	config.fdpig.h = param_set->dma_output_efd.height;

	msdbg_hw(2, "[SENSOR] calibrated: %dx%d (center %d, %d) -> binning %dx%d ->\n",
		instance, hw_ip,
		config.sensor_calibrated.w, config.sensor_calibrated.h,
		config.sensor_center_x, config.sensor_center_y,
		config.sensor_binning_x, config.sensor_binning_y);
	msdbg_hw(2, "[SENSOR] crop %d, %d, %dx%d",
		instance, hw_ip,
		config.sensor_crop.x, config.sensor_crop.y,
		config.sensor_crop.w, config.sensor_crop.h);
	msdbg_hw(2, "[BCROP0] %d, %d, %dx%d\n",
		instance, hw_ip,
		config.bcrop0.x, config.bcrop0.y,
		config.bcrop0.w, config.bcrop0.h);
	msdbg_hw(2, "[BCROP1] %d, %d, %dx%d\n",
		instance, hw_ip,
		config.bcrop1.x, config.bcrop1.y,
		config.bcrop1.w, config.bcrop1.h);
	msdbg_hw(2, "[BDS] %dx%d\n",
		instance, hw_ip,
		config.bds.w, config.bds.h);
	msdbg_hw(2, "[BCROP2] %d, %d, %dx%d\n",
		instance, hw_ip,
		config.bcrop2.x, config.bcrop2.y,
		config.bcrop2.w, config.bcrop2.h);
	msdbg_hw(2, "[FDPIG] EN:%d -> CROP:%d %d, %d, %dx%d -> %dx%d\n",
		instance, hw_ip,
		config.fdpig_en,
		config.fdpig_crop_en,
		config.fdpig_crop.x, config.fdpig_crop.y,
		config.fdpig_crop.w, config.fdpig_crop.h,
		config.fdpig.w, config.fdpig.h);

	taa_hw_s_module_size(base, &config);
	return 0;
}

static int __is_hw_3aa_s_format_regs(struct is_hw_ip *hw_ip, struct taa_param_set *param_set)
{
	void __iomem *base;
	struct taa_format_config config;

	base = hw_ip->regs[REG_SETA];

	config.order = param_set->otf_input.order;

	return taa_hw_s_module_format(base, &config);
}

static int __is_hw_3aa_set_otf_input(struct is_hw_ip *hw_ip, struct taa_param_set *param_set,
	u32 instance)
{
	u32 ret;
	struct taa_cinfifo_config config;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		config.enable = true;
		switch (param_set->otf_input.bitwidth) {
		case OTF_INPUT_BIT_WIDTH_14BIT:
			config.bit14_mode = true;
			break;
		case OTF_INPUT_BIT_WIDTH_10BIT:
			config.bit14_mode = false;
			break;
		default:
			config.bit14_mode = false;
			mserr_hw("invalid otf input bitwidth (%d)", instance, hw_ip,
				param_set->otf_input.bitwidth);
			break;
		}
		config.width = param_set->otf_input.width;
		config.height = param_set->otf_input.height;
	} else {
		config.enable = false;
	}

	ret =  taa_hw_s_cinfifo_config(hw_ip->regs[REG_SETA], &config);
	if (ret) {
		mserr_hw("failed to taa_hw_s_cinfifo_config(%d)", instance, hw_ip, ret);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_3aa_set_wdma(struct is_hw_ip *hw_ip, struct taa_param_set *param_set,
	u32 instance, u32 dma_id)
{
	struct is_hw_3aa *hw;
	struct taa_param_internal_set *p_internal_set;
	void __iomem *base;
	u32 *output_dva;
	struct param_dma_output *dma_output;
	int ret;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	p_internal_set = &hw->param_internal_set[instance];

	switch (dma_id) {
	case TAA_WDMA_THSTAT_PRE:
		output_dva = p_internal_set->output_dva_thstat_pre;
		dma_output = &p_internal_set->dma_output_thstat_pre;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_THSTAT:
		output_dva = p_internal_set->output_dva_thstat;
		dma_output = &p_internal_set->dma_output_thstat;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_DRC:
		output_dva = param_set->output_dva_drcgrid;
		dma_output = &param_set->dma_output_drcgrid;
		base = hw_ip->regs[REG_SETA];
		if (dma_output->cmd == DMA_OUTPUT_COMMAND_DISABLE) {
			ret = taa_hw_bypass_drc(base);
			if (ret)
				hw->cur_iq_set.size = 0;
		}
		break;
	case TAA_WDMA_ORBDS0:
		output_dva = p_internal_set->output_dva_orbds0;
		dma_output = &p_internal_set->dma_output_orbds0;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_ORBDS1:
		output_dva = p_internal_set->output_dva_orbds1;
		dma_output = &p_internal_set->dma_output_orbds1;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_RGBYHIST:
		output_dva = p_internal_set->output_dva_rgbyhist;
		dma_output = &p_internal_set->dma_output_rgbyhist;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_FDPIG:
		output_dva = param_set->output_dva_efd;
		dma_output = &param_set->dma_output_efd;
		base = hw_ip->regs[REG_SETA];
		break;
	case TAA_WDMA_ZSL:
		if (param_set->dma_output_mrg.cmd) {
			output_dva = param_set->output_dva_mrg;
			dma_output = &param_set->dma_output_mrg;
		} else {
			output_dva = param_set->output_dva_before_bds;
			dma_output = &param_set->dma_output_before_bds;
		}
		base = hw_ip->regs[REG_EXT2];
		break;
	case TAA_WDMA_STRP:
		output_dva = param_set->output_dva_after_bds;
		dma_output = &param_set->dma_output_after_bds;
		base = hw_ip->regs[REG_EXT3];
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: cmd (%d), dva0 (0x%08X), dva1 (0x%08X), dva2 (0x%08X)\n", instance, hw_ip,
		hw->dma[dma_id].name, dma_output->cmd, output_dva[0], output_dva[1], output_dva[2]);

	ret = taa_hw_s_wdma(&hw->dma[dma_id], base, dma_output, output_dva,
		(hw_ip->num_buffers & 0xffff));
	if (ret) {
		mserr_hw("failed to initialize 3AA_DMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	return 0;
}

static int __is_hw_3aa_s_dma(struct is_hw_ip *hw_ip, struct taa_param_set *param_set, u32 instance)
{
	u32 dma_id;

	/* WDMA cfg */
	for (dma_id = TAA_WDMA_THSTAT_PRE; dma_id < TAA_WDMA_MAX; dma_id++) {
		if (__is_hw_3aa_set_wdma(hw_ip, param_set, instance, dma_id))
			return -EINVAL;
	}

	return 0;
}

static int __is_hw_3aa_get_change_state(struct is_hw_ip *hw_ip, int instance)
{
	struct is_hardware *hw = NULL;
	struct is_group *head;
	int i, ref_cnt = 0, ret = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->hardware);

	hw = hw_ip->hardware;
	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, hw_ip->group[instance]);

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		if (atomic_read(&hw->streaming[i]) & BIT(HW_ISCHAIN_STREAMING))
			ref_cnt++;
	}

	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		/* OTF or vOTF case */
		if (ref_cnt) {
			ret = SRAM_CFG_BLOCK;
		} else {
			/*
			 * If LIC SRAM_CFG_N already called before stream on,
			 * Skip for not call duplicate lic offset setting
			 */
			if (atomic_inc_return(&hw->lic_updated) == 1)
				ret = SRAM_CFG_N;
			else
				ret = SRAM_CFG_BLOCK;
		}
	} else {
		/* Reprocessing case */
		if (ref_cnt)
			ret = SRAM_CFG_BLOCK;
		else
			ret = SRAM_CFG_R;
	}

#if IS_ENABLED(TAA_SRAM_STATIC_CONFIG)
	if (hw->lic_offset_def[0] == 0) {
		mswarn_hw("lic_offset is 0 with STATIC_CONFIG, use dynamic mode", instance, hw_ip);
	} else {
		if (ret == SRAM_CFG_N)
			ret = SRAM_CFG_S;
		else
			ret = SRAM_CFG_BLOCK;
	}
#endif

	if (!atomic_read(&hw->streaming[hw->sensor_position[instance]]))
		msinfo_hw("LIC change state: %d (refcnt(streaming:%d/updated:%d))", instance, hw_ip,
			ret, ref_cnt, atomic_read(&hw->lic_updated));

	return ret;
}

static int __is_hw_3aa_change_sram_offset(struct is_hw_ip *hw_ip, int instance, int mode)
{
	struct is_hw_3aa *hw = NULL;
	struct is_hardware *hardware = NULL;
	struct taa_param_set *param_set = NULL;
	u32 target_w;
	u32 offset[LIC_OFFSET_MAX] = {0, };
	u32 offsets = LIC_CHAIN_OFFSET_NUM / 2 - 1;
	u32 set_idx = offsets + 1;
	int ret = 0;
	int i, index_a, index_b;
	char *str_a, *str_b;
	int id, sum;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->hardware);
	FIMC_BUG(!hw_ip->priv_info);

	hardware = hw_ip->hardware;
	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw->param_set[instance];

	/* check condition */
	switch (mode) {
	case SRAM_CFG_BLOCK:
		return ret;
	case SRAM_CFG_N:
		for (i = LIC_OFFSET_0; i < offsets; i++) {
			offset[i] = hardware->lic_offset_def[i];
			if (!offset[i])
				offset[i] = (IS_MAX_HW_3AA_SRAM / offsets) * (1 + i);
		}

		target_w = param_set->otf_input.width;
		/* 3AA2 is not considered */
		/* TODO: set offset dynamically */
		if (hw_ip->id == DEV_HW_3AA0) {
			if (target_w > offset[LIC_OFFSET_0])
				offset[LIC_OFFSET_0] = target_w;
		} else if (hw_ip->id == DEV_HW_3AA1) {
			if (target_w > (IS_MAX_HW_3AA_SRAM - offset[LIC_OFFSET_0]))
				offset[LIC_OFFSET_0] = IS_MAX_HW_3AA_SRAM - target_w;
		}
		break;
	case SRAM_CFG_R:
	case SRAM_CFG_MODE_CHANGE_R:
		target_w = param_set->otf_input.width;

		id = hw_ip->id - DEV_HW_3AA0;
		sum = 0;
		for (i = LIC_OFFSET_0; i < offsets; i++) {
			if (id == i)
				offset[i] = sum + target_w;
			else
				offset[i] = sum + 0;

			sum = offset[i];
		}
		break;
	case SRAM_CFG_S:
		for (i = LIC_OFFSET_0; i < offsets; i++)
			offset[i] = hardware->lic_offset_def[i];
		break;
	default:
		mserr_hw("invalid mode (%d)", instance, hw_ip, mode);
		return ret;
	}

	index_a = COREX_SETA * set_idx; /* setA */
	index_b = COREX_SETB * set_idx; /* setB */

	for (i = LIC_OFFSET_0; i < offsets; i++) {
		hardware->lic_offset[0][index_a + i] = offset[i];
		hardware->lic_offset[0][index_b + i] = offset[i];
	}
	hardware->lic_offset[0][index_a + offsets] = hardware->lic_offset_def[index_a + offsets];
	hardware->lic_offset[0][index_b + offsets] = hardware->lic_offset_def[index_b + offsets];

	ret = taa_hw_s_c0_lic(hw_ip->regs[REG_EXT1], LIC_OFFSET_2, offset, offset);
	if (ret) {
		mserr_hw("taa_hw_s_c0_lic fail", instance, hw_ip);
		ret = -EINVAL;
	}

	if (in_atomic())
		goto exit_in_atomic;

	str_a = __getname();
	if (unlikely(!str_a)) {
		mserr_hw(" out of memory for str_a!", instance, hw_ip);
		goto err_alloc_str_a;
	}

	str_b = __getname();
	if (unlikely(!str_b)) {
		mserr_hw(" out of memory for str_b!", instance, hw_ip);
		goto err_alloc_str_b;
	}

	snprintf(str_a, PATH_MAX, "%d", hardware->lic_offset[0][index_a + 0]);
	snprintf(str_b, PATH_MAX, "%d", hardware->lic_offset[0][index_b + 0]);
	for (i = LIC_OFFSET_1; i <= offsets; i++) {
		snprintf(str_a, PATH_MAX, "%s, %d", str_a, hardware->lic_offset[0][index_a + i]);
		snprintf(str_b, PATH_MAX, "%s, %d", str_b, hardware->lic_offset[0][index_b + i]);
	}
	msinfo_hw("=> (%d) update 3AA SRAM offset: setA(%s), setB(%s)\n",
		instance, hw_ip, mode, str_a, str_b);

	__putname(str_b);
err_alloc_str_b:
	__putname(str_a);
err_alloc_str_a:
exit_in_atomic:

	return ret;
}

static void __is_hw_3aa_trigger_async(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	hw->hw_fro_en = (hw_ip->num_buffers & 0xffff) > 1 ? true : false;

	taa_hw_enable_corex(hw_ip->regs[REG_SETA]);

	if (unlikely(test_bit(hw_ip->id - DEV_HW_3AA0, &debug_3aa))) {
		taa_hw_dump(hw_ip->regs[REG_SETA]);
		taa_hw_dump_zsl(hw_ip->regs[REG_EXT2]);
		taa_hw_dump_strp(hw_ip->regs[REG_EXT3]);
	}

	taa_hw_s_global_enable(hw_ip->regs[REG_SETA], true, hw->hw_fro_en);
}

static int __is_hw_3aa_trigger(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_group *head;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, hw_ip->group[instance]);

	if (test_bit(IS_GROUP_OTF_INPUT, &head->state)) {
		if (!test_bit(HW_CONFIG, &hw_ip->state))
			__is_hw_3aa_trigger_async(hw_ip, instance);
		else if (atomic_read(&hw_ip->instance) == instance)
			taa_hw_enable_corex_trigger(hw_ip->regs[REG_SETA]);
		else
			mserr_hw("hw is already occupied\n", instance, hw_ip);
	} else {
		__is_hw_3aa_trigger_async(hw_ip, instance);
	}

	return 0;
}

static int is_hw_3aa_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw;
	struct taa_param_set *param_set;
	u32 instance, b_idx, batch_num;
	struct is_param_region *param_region;
	struct taa_param_internal_set *p_internal_set;
	int mode;
	bool bypass_iq;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);

	instance = frame->instance;

	msdbgs_hw(2, "[F%d][T%d]shot\n", instance, hw_ip, frame->fcount, frame->type);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	set_bit(hw_ip->id, &frame->core_flag);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw->param_set[instance];
	p_internal_set = &hw->param_internal_set[instance];
	b_idx = frame->cur_buf_index;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		/* OTF INPUT case */
		param_set->dma_output_before_bds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_after_bds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_efd.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->dma_output_mrg.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#if IS_ENABLED(LOGICAL_VIDEO_NODE)
		param_set->dma_output_drcgrid.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#endif
#if defined(ENABLE_ORBDS)
		param_set->dma_output_orbds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
#endif
		p_internal_set->dma_output_orbds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		p_internal_set->dma_output_orbds1.cmd = DMA_OUTPUT_COMMAND_DISABLE;

		hw_ip->internal_fcount[instance] = frame->fcount;
	} else {
		hw_ip->internal_fcount[instance] = 0;

		param_region = (struct is_param_region *)frame->shot->ctl.vendor_entry.parameter;
		__is_hw_3aa_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

		is_hardware_dma_cfg_32bit("3xc", hw_ip, frame, b_idx, frame->num_buffers,
			&param_set->dma_output_before_bds.cmd,
			param_set->dma_output_before_bds.plane,
			param_set->output_dva_before_bds,
			frame->txcTargetAddress);

		is_hardware_dma_cfg_32bit("3xp", hw_ip, frame, b_idx, frame->num_buffers,
			&param_set->dma_output_after_bds.cmd,
			param_set->dma_output_after_bds.plane,
			param_set->output_dva_after_bds,
			frame->txpTargetAddress);

		is_hardware_dma_cfg_32bit("mrg", hw_ip, frame, b_idx, frame->num_buffers,
			&param_set->dma_output_mrg.cmd,
			param_set->dma_output_mrg.plane,
			param_set->output_dva_mrg,
			frame->mrgTargetAddress);

#if IS_ENABLED(LOGICAL_VIDEO_NODE)
		is_hardware_dma_cfg_32bit("txdrg",
			hw_ip, frame, b_idx, frame->num_buffers,
			&param_set->dma_output_drcgrid.cmd,
			param_set->dma_output_drcgrid.plane,
			param_set->output_dva_drcgrid,
			frame->txdgrTargetAddress);
#endif

		is_hardware_dma_cfg_32bit("efd", hw_ip, frame, 0, 1,
			&param_set->dma_output_efd.cmd,
			param_set->dma_output_efd.plane,
			param_set->output_dva_efd,
			frame->efdTargetAddress);

#if defined(ENABLE_ORBDS)
		is_hardware_dma_cfg_32bit("ods",
			hw_ip, frame, 0, 1,
			&param_set->dma_output_orbds.cmd,
			param_set->dma_output_orbds.plane,
			param_set->output_dva_orbds,
			frame->txodsTargetAddress);
#endif

		p_internal_set->dma_output_thstat_pre.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		p_internal_set->dma_output_rgbyhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		p_internal_set->dma_output_thstat.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		p_internal_set->dma_output_orbds0.cmd = param_set->dma_output_orbds.cmd;
		p_internal_set->dma_output_orbds1.cmd = param_set->dma_output_orbds.cmd;
	}

	param_set->instance_id = instance;
	param_set->fcount = frame->fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	batch_num = hw_ip->framemgr->batch_num;

	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= b_idx << CURR_INDEX_SHIFT;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		is_log_write("[@][DRV][%d]3aa_shot [T:%d][R:%d][F:%d][%d][C:0x%x][%d][P:0x%x]\n",
			param_set->instance_id, frame->type, param_set->reprocessing,
			param_set->fcount,
			param_set->dma_output_before_bds.cmd, param_set->output_dva_before_bds[0],
			param_set->dma_output_after_bds.cmd,  param_set->output_dva_after_bds[0]);
	}

	/* Pre-shot */
	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		if (frame->shot) {
			param_set->sensor_config.min_target_fps =
				frame->shot->ctl.aa.aeTargetFpsRange[0];
			param_set->sensor_config.max_target_fps =
				frame->shot->ctl.aa.aeTargetFpsRange[1];
			param_set->sensor_config.frametime =
				1000000 / param_set->sensor_config.min_target_fps;
			dbg_hw(2, "3aa_shot: min_fps(%d), max_fps(%d), frametime(%d)\n",
				param_set->sensor_config.min_target_fps,
				param_set->sensor_config.max_target_fps,
				param_set->sensor_config.frametime);
			if (is_lib_isp_set_ctrl(hw_ip, &hw->lib[instance], frame))
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}

		if (is_lib_isp_sensor_update_control(&hw->lib[instance], instance,
					frame->fcount, frame->shot))
			mserr_hw("sensor_update_control fail",
					instance, hw_ip);
	}

	/* LIC */
	if (frame->type != SHOT_TYPE_INTERNAL) {
		mode = __is_hw_3aa_get_change_state(hw_ip, instance);
		ret = __is_hw_3aa_change_sram_offset(hw_ip, instance, mode);
		if (ret)
			mswarn_hw("failed to 3aa_change_sram_offset. ret %d", instance, hw_ip, ret);
	}

	taa_hw_disable_corex_trigger(hw_ip->regs[REG_SETA]);

	/* Shot */
	bypass_iq = false;
	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = is_lib_isp_shot(hw_ip, &hw->lib[instance], param_set, frame->shot);
		if (ret)
			return ret;
		if (likely(!test_bit(hw_ip->id, (unsigned long *)&debug_iq))) {
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			__is_hw_3aa_s_iq_regs(hw_ip, hw);
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
			bypass_iq = true;
		}
	} else {
		bypass_iq = true;
	}

	if (unlikely(bypass_iq))
		__is_hw_3aa_bypass_iq_module(hw_ip, instance);

	ret = __is_hw_3aa_update_internal_param(hw_ip, instance, frame->fcount);
	if (ret) {
		mserr_hw("__is_hw_3aa_update_internal_param is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_s_size_regs(hw_ip, param_set, instance);
	if (ret) {
		mserr_hw("__is_hw_3aa_s_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_s_format_regs(hw_ip, param_set);
	if (ret) {
		mserr_hw("__is_hw_3aa_s_format_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_set_otf_input(hw_ip, param_set, instance);
	if (ret) {
		mserr_hw("__is_hw_3aa_set_otf_input is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_s_dma(hw_ip, param_set, instance);
	if (ret) {
		mserr_hw("s_dma fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_set_control(hw_ip, param_set, instance);
	if (ret) {
		mserr_hw("__is_hw_3aa_set_control is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = __is_hw_3aa_set_fro(hw_ip, instance);
	if (ret) {
		mserr_hw("__is_hw_3aa_set_fro is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(test_bit(TAA_DEBUG_3AA_DTP, &debug_3aa)))
		taa_hw_enable_dtp(hw_ip->regs[REG_SETA], true);

	ret = __is_hw_3aa_trigger(hw_ip, instance);
	if (ret) {
		mserr_hw("trigger fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (IS_ENABLED(TAA_DDK_LIB_CALL))
		is_lib_isp_reset_recovery(hw_ip, &hw->lib[instance], instance);

	return ret;
}

static int is_hw_3aa_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_3aa *hw;
	struct taa_param_set *param_set;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	param_set = &hw->param_set[instance];

	__is_hw_3aa_update_param(hw_ip, &region->parameter, param_set, pmap, instance);

	return 0;
}

static int is_hw_3aa_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw;
	u32 instance = frame->instance;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	instance = frame->instance;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		hw = (struct is_hw_3aa *)hw_ip->priv_info;

		ret = is_lib_isp_get_meta(hw_ip, &hw->lib[instance], frame);
		if (ret) {
			mserr_hw("get_meta fail", instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int is_hw_3aa_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_3aa *hw_3aa;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
	if (unlikely(!hw_3aa)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_3aa->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail", instance, hw_ip);
	}

	return ret;
}

static int is_hw_3aa_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	return is_hardware_frame_done(hw_ip, frame, -1,
			IS_HW_CORE_END, done_type, false);
}

static int is_hw_3aa_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag;
	struct is_hw_ip_setfile *setfile;
	struct is_hw_3aa *hw;
	ulong addr;
	u32 size, i;
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

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		msinfo_lib("create_tune_set count %d\n", instance, hw_ip,
				setfile->using_count);
		for (i = 0; i < setfile->using_count; i++) {
			addr = setfile->table[i].addr;
			size = setfile->table[i].size;

			ret = is_lib_isp_create_tune_set(&hw->lib[instance],
					addr, size, i, flag, instance);

			set_bit(i, &hw->lib[instance].tune_count);
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_3aa_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret;
	struct is_hw_3aa *hw;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;
	u32 setfile_index;
	ulong cal_addr;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

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

	if (setfile->using_count == 0) {
		mswarn_hw(" setfile using_count is 0", instance, hw_ip);
		return 0;
	}

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
				instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip,
		setfile_index, scenario);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = is_lib_isp_apply_tune_set(&hw->lib[instance],
				setfile_index, instance, scenario);
		if (ret)
			return ret;

		if (sensor_position < SENSOR_POSITION_MAX) {
			cal_addr = hw->lib_support->minfo->kvaddr_cal[sensor_position];

			msinfo_hw("load cal data, position: %d, addr: 0x%pK\n",
					instance, hw_ip, sensor_position, cal_addr);

			ret = is_lib_isp_load_cal_data(&hw->lib[instance], instance, cal_addr);
			if (ret)
				return ret;

			ret = is_lib_isp_get_cal_data(&hw->lib[instance], instance,
					&hw_ip->hardware->cal_info[sensor_position],
					CAL_TYPE_LSC_UVSP);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int is_hw_3aa_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_3aa *hw;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;
	u32 i;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

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

	hw = (struct is_hw_3aa *)hw_ip->priv_info;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		for (i = 0; i < setfile->using_count; i++) {
			if (test_bit(i, &hw->lib[instance].tune_count)) {
				ret = is_lib_isp_delete_tune_set(&hw->lib[instance], i, instance);

				clear_bit(i, &hw->lib[instance].tune_count);
			}
		}
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int is_hw_3aa_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		hw = (struct is_hw_3aa *)hw_ip->priv_info;
		ret = is_lib_isp_reset_recovery(hw_ip, &hw->lib[instance], instance);
		if (ret) {
			mserr_hw("reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
			return ret;
		}
	}

	return 0;
}

static int is_hw_3aa_sensor_start(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_3aa *hw;
	struct is_frame *frame;
	struct is_framemgr *hw_framemgr;
	struct camera2_shot *shot = NULL;
#ifdef ENABLE_MODECHANGE_CAPTURE
	struct is_device_sensor *sensor;
#endif

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("sensor_start: mode_change for sensor\n", instance, hw_ip);

	FIMC_BUG(!hw_ip->group[instance]);

	if (test_bit(IS_GROUP_OTF_INPUT, &hw_ip->group[instance]->state)) {
		/* For sensor info initialize for mode change */
		hw_framemgr = hw_ip->framemgr;
		FIMC_BUG(!hw_framemgr);

		framemgr_e_barrier(hw_framemgr, 0);
		frame = peek_frame(hw_framemgr, FS_HW_CONFIGURE);
		framemgr_x_barrier(hw_framemgr, 0);
		if (frame) {
			shot = frame->shot;
		} else {
			mswarn_hw("enable (frame:NULL)(%d)", instance, hw_ip,
				hw_framemgr->queued_count[FS_HW_CONFIGURE]);
#ifdef ENABLE_MODECHANGE_CAPTURE
			sensor = hw_ip->group[instance]->device->sensor;
			if (sensor && sensor->mode_chg_frame) {
				frame = sensor->mode_chg_frame;
				shot = frame->shot;
				msinfo_hw("[F:%d]mode_chg_frame used for REMOSAIC\n",
					instance, hw_ip, frame->fcount);
			}
#endif
		}

		if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
			hw = (struct is_hw_3aa *)hw_ip->priv_info;
			if (shot) {
				ret = is_lib_isp_sensor_info_mode_chg(&hw->lib[instance],
						instance, shot);
				if (ret < 0) {
					mserr_hw("is_lib_isp_sensor_info_mode_chg fail)",
						instance, hw_ip);
				}
			}
		}
	}

	return ret;
}

static int is_hw_3aa_change_chain(struct is_hw_ip *hw_ip, u32 instance,
	u32 next_id, struct is_hardware *hardware)
{
	int ret;
	struct is_hw_3aa *hw;
	u32 curr_id;
	u32 next_hw_id;
	int hw_slot;
	struct is_hw_ip *next_hw_ip;
	struct is_group *group;
	enum exynos_sensor_position sensor_position;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	curr_id = hw_ip->id - DEV_HW_3AA0;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip,
			curr_id, next_id);
		return 0;
	}

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("failed to get HW 3AA", instance, hw_ip);
		return -ENODEV;
	}

	/* Call DDK */
	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		ret = is_lib_isp_change_chain(hw_ip, &hw->lib[instance], instance, next_id);
		if (ret) {
			mserr_hw("is_lib_isp_change_chain", instance, hw_ip);
			return ret;
		}
	}

	next_hw_id = DEV_HW_3AA0 + next_id;
	hw_slot = is_hw_slot_id(next_hw_id);
	if (!valid_hw_slot_id(hw_slot)) {
		merr_hw("[ID:%d]invalid next hw_slot_id [SLOT:%d]", instance,
			next_hw_id, hw_slot);
		return -EINVAL;
	}

	next_hw_ip = &hardware->hw_ip[hw_slot];

	/* This is for decreasing run_rsccount */
	ret = is_hw_3aa_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_3aa_disable is fail", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Copy instance information.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be referred at close time.
	 */
	((struct is_hw_3aa *)(next_hw_ip->priv_info))->lib[instance]
		= hw->lib[instance];
	((struct is_hw_3aa *)(next_hw_ip->priv_info))->param_set[instance]
		= hw->param_set[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];

	sensor_position = hardware->sensor_position[instance];
	next_hw_ip->setfile[sensor_position] = hw_ip->setfile[sensor_position];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	/* This is for increasing run_rsccount */
	ret = is_hw_3aa_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_3aa_enable is fail", instance, next_hw_ip);
		return -EINVAL;
	}

	/*
	 * There is no change about rsccount when change_chain processed
	 * because there is no open/close operation.
	 * But if it isn't increased, abnormal situation can be occurred
	 * according to hw close order among instances.
	 */
	if (!test_bit(hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_dec(&hw_ip->rsccount);
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip,
			atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip,
			atomic_read(&next_hw_ip->rsccount));
	}

	/*
	 * Group in same stream is not change because that is logical,
	 * but hw_ip is not fixed at logical group that is physical.
	 * So, hw_ip have to be saved in group sttucture.
	 */
	group = hw_ip->group[instance];
	group->hw_ip = next_hw_ip;

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);

	return 0;
}

static int is_hw_3aa_get_offline_data(struct is_hw_ip *hw_ip, u32 instance,
	void *data_desc, int fcount)
{
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	if (IS_ENABLED(TAA_DDK_LIB_CALL)) {
		hw = (struct is_hw_3aa *)hw_ip->priv_info;
		return is_lib_get_offline_data(&hw->lib[instance], instance, data_desc, fcount);
	} else {
		return 0;
	}
}

static int is_hw_3aa_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_3aa *hw;
	struct is_hw_3aa_iq *iq_set;
	ulong flag = 0;
	u32 i;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	iq_set = &hw->iq_set;

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, (sizeof(struct cr_set) * regs_size));

	if (unlikely(test_bit(TAA_DEBUG_SET_REG, &debug_3aa))) {
		for (i = 0; i < regs_size; i++)
			msinfo_hw("%s: [0x%08x] 0x%08x\n", instance, hw_ip, __func__,
				regs[i].reg_addr, regs[i].reg_data);
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "[F%d]Store IQ regs set: %p, size(%d)\n", instance, hw_ip,
			fcount, regs, regs_size);

	return 0;
}

static int is_hw_3aa_dump_regs(struct is_hw_ip *hw_ip, u32 instance,
		u32 fcount, struct cr_set *regs, u32 regs_size,
		enum is_reg_dump_type dump_type)
{
	size_t total_size;
	struct is_common_dma *dma;
	struct is_hw_3aa *hw_3aa = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		if (unlikely(test_bit(TAA_DEBUG_DUMP_HEX, &debug_3aa))) {
			total_size = (hw_ip->regs_end[REG_SETA] - hw_ip->regs_start[REG_SETA] + 1);
			is_hw_dump_regs_hex(hw_ip->regs[REG_SETA], "3AA", total_size);

			total_size = (hw_ip->regs_end[REG_EXT2] - hw_ip->regs_start[REG_EXT2] + 1);
			is_hw_dump_regs_hex(hw_ip->regs[REG_EXT2], "ZSL", total_size);

			total_size = (hw_ip->regs_end[REG_EXT3] - hw_ip->regs_start[REG_EXT3] + 1);
			is_hw_dump_regs_hex(hw_ip->regs[REG_EXT3], "STRP", total_size);
		} else {
			taa_hw_dump(hw_ip->regs[REG_SETA]);
			taa_hw_dump_zsl(hw_ip->regs[REG_EXT2]);
			taa_hw_dump_strp(hw_ip->regs[REG_EXT3]);
		}
		break;
	case IS_REG_DUMP_DMA:
		for (i = TAA_WDMA_THSTAT_PRE; i < TAA_WDMA_ORBDS1; i++) {
			hw_3aa = (struct is_hw_3aa *)hw_ip->priv_info;
			dma = &hw_3aa->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		taa_hw_dump_zsl(hw_ip->regs[REG_EXT2]);
		taa_hw_dump_strp(hw_ip->regs[REG_EXT3]);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_3aa_set_config(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, void *conf)
{
	struct is_hw_3aa *hw;

	msdbg_hw(2, "[%s] is called\n", instance, hw_ip, __func__);

	hw = (struct is_hw_3aa *)hw_ip->priv_info;
	memcpy(&hw->iq_config, conf, sizeof(hw->iq_config));

	msdbg_hw(2, "[F%d][thstatpre] bypass %d, grid %dx%d\n", instance, hw_ip, fcount,
		hw->iq_config.thstatpre_bypass,
		hw->iq_config.thstatpre_grid_w, hw->iq_config.thstatpre_grid_h);
	msdbg_hw(2, "[F%d][thstat] bypass %d, grid %dx%d\n", instance, hw_ip, fcount,
		hw->iq_config.thstat_bypass,
		hw->iq_config.thstat_grid_w, hw->iq_config.thstat_grid_h);
	msdbg_hw(2, "[F%d][rgbyhist] bypass %d, bin %d hist %d\n", instance, hw_ip, fcount,
		hw->iq_config.rgbyhist_bypass,
		hw->iq_config.rgby_bin_num, hw->iq_config.rgby_hist_num);
	msdbg_hw(2, "[F%d][drcclct] bypass %d, grid %dx%d\n", instance, hw_ip, fcount,
		hw->iq_config.drcclct_bypass,
		hw->iq_config.drc_grid_w, hw->iq_config.drc_grid_h);
	msdbg_hw(2, "[F%d][orbds] bypass %d, ds0 %dx%d ds1 %dx%d\n", instance, hw_ip, fcount,
		hw->iq_config.orbds_bypass,
		hw->iq_config.orbds0_w, hw->iq_config.orbds0_h,
		hw->iq_config.orbds1_w, hw->iq_config.orbds1_h);

	return 0;
}

static int is_hw_3aa_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_3aa *hw = (struct is_hw_3aa *)hw_ip->priv_info;
	u32 fcount = atomic_read(&hw_ip->fcount);

	is_hw_3aa_dump_regs(hw_ip, instance, fcount, NULL, 0, IS_REG_DUMP_TO_LOG);

	if (IS_ENABLED(TAA_DDK_LIB_CALL))
		return is_lib_notify_timeout(&hw->lib[instance], instance);
	else
		return 0;
}

const static struct is_hw_ip_ops is_hw_3aa_ops = {
	.open			= is_hw_3aa_open,
	.init			= is_hw_3aa_init,
	.deinit			= is_hw_3aa_deinit,
	.close			= is_hw_3aa_close,
	.enable			= is_hw_3aa_enable,
	.disable		= is_hw_3aa_disable,
	.shot			= is_hw_3aa_shot,
	.set_param		= is_hw_3aa_set_param,
	.get_meta		= is_hw_3aa_get_meta,
	.get_cap_meta		= is_hw_3aa_get_cap_meta,
	.frame_ndone		= is_hw_3aa_frame_ndone,
	.load_setfile		= is_hw_3aa_load_setfile,
	.apply_setfile		= is_hw_3aa_apply_setfile,
	.delete_setfile		= is_hw_3aa_delete_setfile,
	.restore		= is_hw_3aa_restore,
	.sensor_start		= is_hw_3aa_sensor_start,
	.change_chain		= is_hw_3aa_change_chain,
	.get_offline_data	= is_hw_3aa_get_offline_data,
	.set_regs		= is_hw_3aa_set_regs,
	.dump_regs		= is_hw_3aa_dump_regs,
	.set_config		= is_hw_3aa_set_config,
	.notify_timeout		= is_hw_3aa_notify_timeout,
};

int is_hw_3aa_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot = -1;

	/* initialize device hardware */
	hw_ip->id = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops = &is_hw_3aa_ops;
	hw_ip->itf = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_3aa_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_3aa_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].handler = &is_hw_csis_dma_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].id = INTR_HWIP3;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP3].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].handler = &is_hw_csis_dma_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].id = INTR_HWIP4;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP4].valid = true;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);

	sinfo_hw("probe done\n", hw_ip);

	return 0;
}
