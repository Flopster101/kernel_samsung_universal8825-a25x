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

#include <linux/moduleparam.h>

#include "is-hw-cstat.h"
#include "is-err.h"
#include "is-hw-api-cstat.h"
#include "is-hw.h"
#include "is-dvfs.h"
#include "is-votfmgr.h"
#include "pablo-obte.h"

static DEFINE_MUTEX(cmn_reg_lock);

enum is_hw_cstat_dbg_type {
	CSTAT_DBG_NONE = 0,
	CSTAT_DBG_REG_DUMP,
	CSTAT_DBG_S2D,
	CSTAT_DBG_WDMA_OFF,
	CSTAT_DBG_TYPE_NUM
};

struct is_hw_cstat_dbg_info {
	u32 type;
	u32 instance;
	u32 fcount;
	u32 src1;
	u32 src2;
	ulong dma_id;
};

static struct is_hw_cstat_dbg_info dbg_info = {
	.type = CSTAT_DBG_NONE,
	.instance = 0,
	.fcount = 0,
	.src1 = 0,
	.src2 = 0,
	.dma_id = 0,
};

static void is_hw_cstat_print_stall_status(struct is_hw_ip *hw_ip)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_ip *hw_ip_ctx;
	int hw_id, hw_slot;
	void __iomem *base;

	for (hw_id = DEV_HW_3AA0; hw_id <= DEV_HW_3AA3; hw_id++) {
		hw_slot = is_hw_slot_id(hw_id);
		if (!valid_hw_slot_id(hw_slot)) {
			serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
			continue;
		}

		hw_ip_ctx = &hardware->hw_ip[hw_slot];
		base = hw_ip_ctx->regs[REG_SETA];

		cstat_hw_print_stall_status(base, (hw_id - DEV_HW_3AA0));
	}
}

static inline void is_hw_cstat_check_dbg_info(struct is_hw_ip *hw_ip,
		u32 int_src1, u32 int_src2, u32 instance, u32 fcount)
{
	bool is_int;
	bool is_ins;
	bool is_fcount;

	is_int = ((int_src1 & dbg_info.src1) || (int_src2 & dbg_info.src2));
	is_ins = (instance == dbg_info.instance);
	is_fcount = (!dbg_info.fcount || (fcount == dbg_info.fcount));

	if (!is_int || !is_ins || !is_fcount)
		return;

	msinfo_hw("check_dbg_info: 0x%x(0x%x) 0x%x(0x%x) fcount %d(%d)\n", instance, hw_ip,
			int_src1, dbg_info.src1,
			int_src2, dbg_info.src2,
			fcount, dbg_info.fcount);

	switch (dbg_info.type) {
	case CSTAT_DBG_REG_DUMP:
		cstat_hw_dump(hw_ip->regs[REG_SETA]);
		break;
	case CSTAT_DBG_S2D:
		is_hw_cstat_print_stall_status(hw_ip);
		is_debug_s2d(true, "CSTAT_DBG_S2D");
		break;
	default:
		/* Do nothing */
		break;
	};
};

static int is_hw_cstat_dbg_set(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 dbg_type;
	u32 instance, fcount, src1, src2;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &dbg_type);
	if (ret || dbg_type >= CSTAT_DBG_TYPE_NUM) {
		err("Invalid dbg_type %u ret %d", dbg_type, ret);
		goto func_exit;
	}

	switch (dbg_type) {
	case CSTAT_DBG_REG_DUMP:
	case CSTAT_DBG_S2D:
		if (argc < 5) {
			err("Not enough parameters. %d < 5", argc);
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &instance);
		if (ret) {
			err("Invalid instance %d ret %d", instance, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[2], 0, &fcount);
		if (ret) {
			err("Invalid fcount %d ret %d", fcount, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[3], 0, &src1);
		if (ret) {
			err("Invalid src1 %d ret %d", src1, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[4], 0, &src2);
		if (ret) {
			err("Invalid src2 %d ret %d", src2, ret);
			goto func_exit;
		}

		dbg_info.type = dbg_type;
		dbg_info.instance = instance;
		dbg_info.fcount = fcount;
		dbg_info.src1 = src1;
		dbg_info.src2 = src2;

		info("Set regdump/scandump trigger:[T%d][I%d][F%d] 0x%x 0x%x\n",
				dbg_type, instance, fcount, src1, src2);

		break;
	case CSTAT_DBG_WDMA_OFF:
		if (argc < 2) {
			err("Not enough parameters. %d < 2", argc);
			goto func_exit;
		}

		ret = kstrtoul(argv[1], 0, &dbg_info.dma_id);
		if (ret) {
			err("Invalid dma_id %d ret %d", dbg_info.dma_id, ret);
			goto func_exit;
		}

		info("Disable WDMA: 0x%x\n", dbg_info.dma_id);

		break;
	default:
		err("NOT supported dbg_type %u", dbg_type);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int is_hw_cstat_dbg_get(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "> Set regdump/scandump trigger\n");
	ret += sprintf(buffer + ret, "  # echo <type> <instance> <fcount> <int1_msk> <int2_msk> > debug_cstat\n");
	ret += sprintf(buffer + ret, "    - type: 1=REG_DUMP, 2=S2D\n");
	ret += sprintf(buffer + ret, "    - fcount 0: Trigger for every interrupt.\n");
	ret += sprintf(buffer + ret, "    - int1/2_msk: Trigger when the specific bit is set.\n");
	ret += sprintf(buffer + ret, "================================================================\n");
	ret += sprintf(buffer + ret, "> Disable specific WDMA\n");
	ret += sprintf(buffer + ret, "  # echo 3 <dma_id> > debug_cstat\n");
	ret += sprintf(buffer + ret, "    - dma_id: 1 << Actual WDMA ID\n");
	ret += sprintf(buffer + ret, "================================================================\n");
	ret += sprintf(buffer + ret, "type %d instance %d fcount %d int1_msk 0x%x int2_msk 0x%x dma_id 0x%lx\n",
			dbg_info.type,
			dbg_info.instance,
			dbg_info.fcount,
			dbg_info.src1,
			dbg_info.src2,
			dbg_info.dma_id);

	return ret;
}

static const struct kernel_param_ops is_hw_cstat_dbg_ops = {
	.set = is_hw_cstat_dbg_set,
	.get = is_hw_cstat_dbg_get,
};

module_param_cb(debug_cstat, &is_hw_cstat_dbg_ops, NULL, 0644);

static inline void is_hw_cstat_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 hw_fcount = atomic_read(&hw_ip->fcount);

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
				hw_fcount, EVENT_FRAME_START, 0, NULL);
	} else {
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F%d]F.S\n", instance, hw_ip, hw_fcount);

		is_hardware_frame_start(hw_ip, instance);
	}
}

static inline void is_hw_cstat_frame_end(struct is_hw_ip *hw_ip, u32 instance, bool frame_done)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	void __iomem *base;
	enum cstat_event_id e_id;
	u32 hw_fcount = atomic_read(&hw_ip->fcount) - 1;
	u32 edge_score;

	base = hw_ip->regs[REG_SETA];

	if (IS_ENABLED(CSTAT_USE_COREX_SW_TRIGGER) ||
			frame_done ||
			hw->input == DMA) {
		e_id = EVENT_CSTAT_FRAME_END;
	} else {
		e_id = EVENT_CSTAT_FRAME_END_REG_WRITE_FAILURE;
		mswarn_hw("[F%d] CONFIG_LOCK_DELAY 0x%x 0x%x", instance, hw_ip,
				hw_fcount,
				hw->irq_state[CSTAT_INT1],
				hw->irq_state[CSTAT_INT2]);
	}

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		/* Edge score notifier */
		if (!cstat_hw_g_stat_data(hw_ip->regs[REG_SETA],
					CSTAT_STAT_EDGE_SCORE, (void *)&edge_score))
			is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
					hw_fcount, EVENT_CSTAT_EDGE_SCORE, 0,
					(void *)&edge_score);

		/* FE notifier */
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
				hw_fcount, e_id, 0, NULL);
		tasklet_schedule(&hw->end_tasklet);
	} else {
		_is_hw_frame_dbg_trace(hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F%d]F.E\n", instance, hw_ip, hw_fcount);

		is_hardware_frame_done(hw_ip, NULL, -1, IS_HW_CORE_END,
				IS_SHOT_SUCCESS, true);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe))
			mserr_hw("[F%d]fs %d fe %d", instance, hw_ip,
					hw_fcount,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe));

		wake_up(&hw_ip->status.wait_queue);
	}

	if (test_bit(HW_SUSPEND, &hw_ip->state))
		cstat_hw_s_corex_enable(base, false);
}

static inline void is_hw_cstat_end_notifier(struct is_hw_ip *hw_ip, u32 instance,
		enum is_cstat_subdev_id s_id)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_device_ischain *device = hw_ip->group[instance]->device;
	struct is_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_priv_buf *pb;
	struct is_hw_cstat_stat *stat;
	struct param_dma_output *dma;
	unsigned long flags;
	void *data;
	enum cstat_event_id e_id;
	u32 fcount = atomic_read(&hw_ip->fcount) - 1;
	ulong kva;
	u32 bytes;

	sd = &device->subdev_cstat[s_id];
	framemgr = GET_SUBDEV_FRAMEMGR(sd);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_0, flags);
	do {
		frame = peek_frame(framemgr, FS_PROCESS);
		if (!frame || frame->fcount > fcount) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);
			return;
		}

		trans_frame(framemgr, frame, FS_FREE);
	} while (frame->fcount < fcount);
	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);

	/* cache invalidate */
	pb = frame->pb_output;
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);
	msdbg_hw(2, "[F%d][%s] cache invalidate: kva 0x%lx, size: %d", instance, hw_ip,
		 frame->fcount, sd->name, frame->pb_output->kva, frame->pb_output->size);

	stat = &hw->stat[frame->index];

	switch (s_id) {
	case IS_CSTAT_PRE_THUMB:
		data = (void *)&stat->pre;
		e_id = EVENT_CSTAT_PRE;
		kva = stat->pre.kva;
		bytes = stat->pre.bytes;
		dma = &stat->pre.dma;
		break;
	case IS_CSTAT_AE_THUMB:
		data = (void *)&stat->ae;
		e_id = EVENT_CSTAT_AE;
		kva = stat->ae.kva;
		bytes = stat->ae.bytes;
		dma = &stat->ae.dma;
		break;
	case IS_CSTAT_AWB_THUMB:
		data = (void *)&stat->awb;
		e_id = EVENT_CSTAT_AWB;
		kva = stat->awb.kva;
		bytes = stat->awb.bytes;
		dma = &stat->awb.dma;
		break;
	case IS_CSTAT_RGBY_HIST:
		data = (void *)&stat->rgby;
		e_id = EVENT_CSTAT_RGBY;
		kva = stat->rgby.kva;
		bytes = stat->rgby.bytes;
		dma = &stat->rgby.dma;
		break;
	default:
		/* Skip */
		return;
	};

	if (dma->cmd)
		is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
					fcount, e_id, 0, data);
	else
		mswarn_hw("[%s][F%d] NDONE! Skip notify", instance, hw_ip,
				sd->name, frame->fcount);


	msdbg_hw(3, "%s:[%s][F%d][I%d] kva 0x%lx byte %d\n", instance, hw_ip, __func__,
			sd->name, frame->fcount,
			frame->index, kva, bytes);
}

static void is_hw_cstat_end_tasklet(unsigned long data)
{
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)data;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 s_id;

	msdbg_hw(2, "%s: src 0x%lx 0x%lx\n", instance, hw_ip, __func__,
			hw->irq_state[CSTAT_INT1], hw->irq_state[CSTAT_INT2]);

	for (s_id = IS_CSTAT_PRE_THUMB; s_id < IS_CSTAT_SUBDEV_NUM; s_id++)
		is_hw_cstat_end_notifier(hw_ip, instance, s_id);
}

static int is_hw_cstat_isr1(u32 hw_id, void *data)
{
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)data;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_hardware *hardware = hw_ip->hardware;
	ulong int1, int2;
	u32 instance, hw_fcount, fs, line, fe, corex_end;
	u32 int_status;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	atomic_inc(&hw->isr_run_count);

	int1 = (ulong)cstat_hw_g_int1_status(hw_ip->regs[REG_SETA], true, hw->hw_fro_en);
	int2 = (ulong)cstat_hw_g_int2_status(hw_ip->regs[REG_SETA], false, hw->hw_fro_en);
	hw->irq_state[CSTAT_INT1] = int1;
	hw->irq_state[CSTAT_INT2] = int2;

	msdbg_hw(2, "[INT1][F%d] src 0x%lx 0x%lx\n", instance, hw_ip, hw_fcount,
			int1, int2);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[INT1] Ignore ISR before HW config 0x%lx 0x%lx\n",
				instance, hw_ip, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[INT1][F%d] invalid HW state 0x%x src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, hw_ip->state, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("[INT1][F%d] during recovery. src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	/* Handle FS/LINE/FE interrupts */
	fs = cstat_hw_is_occurred(int1, CSTAT_FS);
	line = cstat_hw_is_occurred(int1, CSTAT_LINE);
	corex_end = cstat_hw_is_occurred(int1, CSTAT_COREX_END);
	fe = cstat_hw_is_occurred(int1, CSTAT_FE);

	if (fs && fe)
		mswarn_hw("[INT1][F%d] start/end overlapped!! src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);

	if ((fs && line) || (line && fe))
		mswarn_hw("[INT1][F%d] line overlapped!! src 0x%lx 0x%lx", instance, hw_ip,
				hw_fcount, int1, int2);

	int_status = fs | line | corex_end | fe;

	while (int_status) {
		switch (hw->event_state) {
		case CSTAT_FS:
			if (line) {
				atomic_add(1, &hw_ip->count.cl);
				if (corex_end) {
					mswarn_hw("[INT1][F%d] clear invalid corex_end event",
							instance, hw_ip, hw_fcount);
					int_status &= ~corex_end;
					corex_end = 0;
				}
				is_hardware_config_lock(hw_ip, instance, hw_fcount);
				hw->event_state = CSTAT_LINE;
				int_status &= ~line;
				line = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_LINE:
			if (corex_end) {
				hw->event_state = CSTAT_COREX_END;
				int_status &= ~corex_end;
				corex_end = 0;
			} else if (fe) {
				hw->event_state = CSTAT_FE;
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
				is_hw_cstat_frame_end(hw_ip, instance, false);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_COREX_END:
			if (fe) {
				hw->event_state = CSTAT_FE;
				is_hw_cstat_frame_end(hw_ip, instance, true);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_FE:
			if (fs) {
				hw->event_state = CSTAT_FS;
				is_hw_cstat_frame_start(hw_ip, instance);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case CSTAT_INIT:
			if (corex_end) {
				int_status &= ~corex_end;
				corex_end = 0;
			}
			if (fs) {
				hw->event_state = CSTAT_FS;
				is_hw_cstat_frame_start(hw_ip, instance);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		}

		/* Skip CSTAT_LINE, CSTAT_COREX_END event for DMA input */
		if (hw->input == DMA && hw->event_state == CSTAT_FS)
			hw->event_state = CSTAT_COREX_END;
	}
skip_isr_event:
	if (int_status)
		mswarn_hw("[F%d] invalid interrupt: event_state(%d), int_status(%x)",
				instance, hw_ip, hw_fcount, hw->event_state, int_status);


	if (cstat_hw_is_occurred(int1, CSTAT_ERR)) {
		cstat_hw_print_err(hw_ip->name, instance, hw_fcount, int1, int2);
		is_hw_cstat_print_stall_status(hw_ip);

		if (IS_ENABLED(USE_SKIP_DUMP_LIC_OVERFLOW)
		    && clear_gather_crc_status(instance, hw_ip) > 0) {
			set_bit(IS_SENSOR_ESD_RECOVERY,
				&hw_ip->group[instance]->device->sensor->state);
			mswarn_hw("skip to s2d dump", instance, hw_ip);
		} else {
			is_debug_s2d(true, "CSTAT HW Error");
		}
	}

	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	is_hw_cstat_check_dbg_info(hw_ip, int1, 0, instance, hw_fcount);

	return 0;
}

static inline void is_hw_cstat_cdaf(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_device_ischain *device = hw_ip->group[instance]->device;
	struct is_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct cstat_cdaf cdaf;
	u32 hw_fcount = atomic_read(&hw_ip->fcount);

	sd = &device->subdev_cstat[IS_CSTAT_CDAF];
	framemgr = GET_SUBDEV_FRAMEMGR(sd);
	frame = peek_frame(framemgr, FS_FREE);
	if (!frame) {
		mserr_hw("[CDAF] Failed to get FREE frame. num %d", instance, hw_ip,
				framemgr->num_frames);
		return;
	}

	cdaf.kva = frame->kvaddr_buffer[0];
	if (!cdaf.kva) {
		mserr_hw("[F%d] There is no valid CDAF buffer. num %d", instance, hw_ip,
				hw_fcount, framemgr->num_frames);
		return;
	}

	if (cstat_hw_g_stat_data(hw_ip->regs[REG_SETA], CSTAT_STAT_CDAF, (void *)&cdaf)) {
		mserr_hw("[F%d] Failed to get CDAF stat", instance, hw_ip,
				hw_fcount);
		return;
	}

	is_lib_isp_event_notifier(hw_ip, &hw->lib[instance], instance,
			hw_fcount, EVENT_CSTAT_CDAF, 0, &cdaf);
}

static int is_hw_cstat_isr2(u32 hw_id, void *data)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	u32 src, instance, hw_fcount;

	hw_ip = (struct is_hw_ip *)data;
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	atomic_inc(&hw->isr_run_count);

	src = cstat_hw_g_int2_status(hw_ip->regs[REG_SETA], true, hw->hw_fro_en);

	msdbg_hw(2, "[INT2][F%d] src 0x%x\n", instance, hw_ip, hw_fcount, src);

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[INT2] Ignore ISR before HW config 0x%lx\n",
				instance, hw_ip, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[INT2][F%d] invalid HW state 0x%x src 0x%x", instance, hw_ip,
				hw_fcount, hw_ip->state, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag)) {
		mserr_hw("[INT2][F%d] during recovery. src 0x%x", instance, hw_ip,
				hw_fcount, src);
		atomic_dec(&hw->isr_run_count);
		wake_up(&hw->isr_wait_queue);

		return 0;
	}

	if (cstat_hw_is_occurred(src, CSTAT_CDAF))
		is_hw_cstat_cdaf(hw_ip, instance);

	atomic_dec(&hw->isr_run_count);
	wake_up(&hw->isr_wait_queue);

	is_hw_cstat_check_dbg_info(hw_ip, 0, src, instance, hw_fcount);

	return 0;
}

static int __nocfi is_hw_cstat_open(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group)
{
	int ret;
	struct is_hw_cstat *hw;
	u32 reg_cnt = cstat_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, hw_ip->id, "HWCSTAT");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME);

	hw = vzalloc(sizeof(struct is_hw_cstat));
	if (!hw) {
		mserr_hw("failed to alloc is_hw_cstat", instance, hw_ip);
		ret = -ENOMEM;
		goto err_hw_alloc;
	}

	hw_ip->priv_info = (void *)hw;

	ret = get_lib_func(LIB_FUNC_CSTAT, (void **)&hw->lib_func);
	if (hw->lib_func == NULL) {
		mserr_hw("failed to get lib_func", instance, hw_ip);
		ret = -EINVAL;
		goto err_lib_func;
	}

	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	hw->lib_support = is_get_lib_support();

	hw->lib[instance].func = hw->lib_func;
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
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

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);

	hw->hw_fro_en = false;
	hw->input = CSTST_INPUT_PATH_NUM;

	hw->is_sfr_dumped = false;

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);

	atomic_set(&hw->isr_run_count, 0);
	init_waitqueue_head(&hw->isr_wait_queue);

	msdbg_hw(2, "open: [G:0x%lx], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	return 0;

err_cur_regs_alloc:
	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;

err_regs_alloc:
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw->lib[instance], instance);

err_chain_create:
err_lib_func:
	vfree(hw);
	hw_ip->priv_info = NULL;

err_hw_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_cstat_init(struct is_hw_ip *hw_ip, u32 instance,
	struct is_group *group, bool flag)
{
	int ret, sd_id;
	struct is_hw_cstat *hw;
	struct is_device_ischain *device;
	struct is_subdev *sd;
	struct cstat_stat_buf_info info;
	u32 dma_id, f_type;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("is_hw_cstat is null", instance, hw_ip);
		return -ENODATA;
	}

	device = group->device;
	if (!device) {
		mserr_hw("failed to get devcie", instance, hw_ip);
		return -ENODEV;
	}

	hw->lib[instance].object = NULL;
	hw->lib[instance].func = hw->lib_func;

	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		f_type = LIB_FRAME_HDR_SHORT;
	else if (is_sensor_g_aeb_mode(device->sensor))
		f_type = LIB_FRAME_HDR_LONG;
	else
		f_type = LIB_FRAME_HDR_SINGLE;

	if (hw->lib[instance].object != NULL) {
		msdbg_hw(2, "object is already created\n", instance, hw_ip);
	} else if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		if (is_lib_isp_object_create(hw_ip, &hw->lib[instance], instance,
				(u32)flag, f_type)) {
			mserr_hw("object create fail", instance, hw_ip);
			return -EINVAL;
		}
	}

	for (sd_id = 0; sd_id < IS_CSTAT_SUBDEV_NUM; sd_id++) {
		sd = &device->subdev_cstat[sd_id];

		if (cstat_hw_g_stat_size(sd_id, &info))
			continue;

		snprintf(sd->name, sizeof(sd->name), "%s", is_cstat_subdev_name[sd_id]);

		ret = is_subdev_internal_open(device, IS_DEVICE_ISCHAIN,
						group->leader.vid, sd);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_open. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd_cfg;
		}

		ret = is_subdev_internal_s_format(device, IS_DEVICE_ISCHAIN, sd,
				info.w, info.h, info.bpp, NONE, 0, info.num, sd->name);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_s_format. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd_cfg;
		}

		ret = is_subdev_internal_start(device, IS_DEVICE_ISCHAIN, sd);
		if (ret) {
			mserr_hw("[%s] failed to subdev_internal_start. ret %d", instance, hw_ip,
					sd->name, ret);
			goto err_sd_cfg;
		}
	}

	for (dma_id = CSTAT_R_CL; dma_id < CSTAT_DMA_NUM; dma_id++) {
		if (cstat_hw_create_dma(hw_ip->regs[REG_SETA], dma_id,
				&hw->dma[dma_id])) {
			mserr_hw("[D%d] create_dma error", instance, hw_ip,
					dma_id);
			ret = -ENODATA;
			goto err_dma_create;
		}
	}

	group->hw_ip = hw_ip;
	msinfo_hw("[%s] Binding\n", instance, hw_ip, group_id_name[group->id]);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_init_3aa(instance, flag);

	set_bit(HW_INIT, &hw_ip->state);

	return 0;

err_dma_create:
	while (sd_id-- > 0) {
		is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN,
				&device->subdev_cstat[sd_id]);
		is_subdev_internal_close(device, IS_DEVICE_ISCHAIN,
				&device->subdev_cstat[sd_id]);
	}

err_sd_cfg:
	is_lib_isp_object_destroy(hw_ip, &hw->lib[instance], instance);

	return ret;
}

static int is_hw_cstat_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_group *group;
	struct is_device_ischain *device;
	struct is_hw_cstat *hw;
	u32 sd_id;

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

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		hw = (struct is_hw_cstat *)hw_ip->priv_info;

		is_lib_isp_object_destroy(hw_ip, &hw->lib[instance], instance);

		hw->lib[instance].object = NULL;
	}

	for (sd_id = 0; sd_id < IS_CSTAT_SUBDEV_NUM; sd_id++) {
		is_subdev_internal_stop(device, IS_DEVICE_ISCHAIN,
				&device->subdev_cstat[sd_id]);
		is_subdev_internal_close(device, IS_DEVICE_ISCHAIN,
				&device->subdev_cstat[sd_id]);
	}

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_deinit_3aa(instance);

	return 0;
}

static int is_hw_cstat_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw;
	struct is_hardware *hardware;
	struct exynos_platform_is *pdata;
	int i, max_num;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL))
		is_lib_isp_chain_destroy(hw_ip, &hw->lib[instance], instance);

	vfree(hw->iq_set.regs);
	hw->iq_set.regs = NULL;
	vfree(hw->cur_iq_set.regs);
	hw->cur_iq_set.regs = NULL;
	vfree(hw);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.taa;

	/*
	 * For safe power off
	 * This is common register for all CSTAT channel.
	 * So, it should be set one time in final instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		int hw_id = DEV_HW_3AA0 + i;
		int hw_slot = is_hw_slot_id(hw_id);
		struct is_hw_ip *hw_ip_phys;

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			mutex_unlock(&cmn_reg_lock);
			return -EINVAL;
		}

		hw_ip_phys = &hardware->hw_ip[hw_slot];

		if (hw_ip_phys && test_bit(HW_OPEN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		if (cstat_hw_s_reset(hw_ip->regs[REG_EXT1]))
			mserr_hw("reset fail", instance, hw_ip);
	}
	mutex_unlock(&cmn_reg_lock);

	return 0;
}

/* Initialize the CSTAT core which is only valid for CTX0 */
static int is_hw_cstat_core_init(struct is_hw_ip *hw_ip, u32 instance)
{
	if (cstat_hw_s_reset(hw_ip->regs[REG_EXT1])) {
		mserr_hw("reset fail", instance, hw_ip);
		return -EINVAL;
	}

	cstat_hw_core_init(hw_ip->regs[REG_EXT1]);

	return 0;
}

/* Initialize each CSTAT context core */
static void is_hw_cstat_ctx_init(struct is_hw_ip *hw_ip)
{
	cstat_hw_ctx_init(hw_ip->regs[REG_SETA]);

	/* Put COREX in the disabled state */
	cstat_hw_s_corex_enable(hw_ip->regs[REG_SETA], false);
}

static void is_hw_cstat_s_global_enable(struct is_hw_ip *hw_ip, bool enable, bool fro_en)
{
	cstat_hw_s_global_enable(hw_ip->regs[REG_SETA], enable, fro_en);
}

static void is_hw_cstat_wait_isr_done(struct is_hw_ip *hw_ip)
{
	long timetowait;
	struct is_hw_cstat *hw;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	cstat_hw_wait_isr_clear(hw_ip->regs[REG_SETA], hw->hw_fro_en);

	timetowait = wait_event_timeout(hw->isr_wait_queue,
					!atomic_read(&hw->isr_run_count),
					IS_HW_STOP_TIMEOUT);
	if (!timetowait)
		mserr_hw("wait isr done timeout (%ld)", atomic_read(&hw_ip->instance),
							hw_ip, timetowait);
}

static void is_hw_cstat_s_rdma_byr(void *data, unsigned long id, struct is_frame *frame)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 instance;

	hw_ip = (struct is_hw_ip *)data;
	if (!hw_ip) {
		err("failed to get CSTAT hw_ip");
		return;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	instance = atomic_read(&hw_ip->instance);
	p_set = &hw->param_set[instance];

	is_hardware_dma_cfg("cstat_byr_in",
			hw_ip, frame, frame->cur_buf_index, frame->num_buffers,
			&p_set->dma_input.cmd,
			p_set->dma_input.plane,
			p_set->input_dva,
			frame->dvaddr_buffer);

	if (cstat_hw_s_rdma_cfg(&hw->dma[CSTAT_R_BYR], p_set, (hw_ip->num_buffers & 0xFFFF)))
		mserr_hw("failed to s_rdma_cfg", instance, hw_ip);
}

static const struct votf_ops cstat_votf_ops = {
	.s_addr		= is_hw_cstat_s_rdma_byr,
};

static int is_hw_cstat_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_hardware *hardware;
	struct exynos_platform_is *pdata;
	struct cstat_param *param;
	int i, max_num;

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		mswarn_hw("Not mapped. hw_map 0x%lx", instance, hw_ip, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	tasklet_init(&hw->end_tasklet, is_hw_cstat_end_tasklet, (unsigned long)hw_ip);
	hw->event_state = CSTAT_INIT;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	/* Prepare for VOTF */
	param = &hw_ip->region[instance]->parameter.cstat[0];
	if (param->dma_input.v_otf_enable) {
		struct is_group *group = hw_ip->group[instance];

		if (is_votf_register_framemgr(group, TRS, hw_ip,
					&cstat_votf_ops, group->leader.id)) {
			mserr_hw("Failed to is_votf_register_framemgr", instance, hw_ip);
			return -EINVAL;
		}
	}

	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.taa;

	/*
	 * This is common register for all CSTAT channel.
	 * So, it should be set one time in first instance.
	 */
	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		int hw_id = DEV_HW_3AA0 + i;
		int hw_slot = is_hw_slot_id(hw_id);
		struct is_hw_ip *hw_ip_phys;

		if (!valid_hw_slot_id(hw_slot)) {
			merr_hw("invalid slot (%d,%d)", instance, hw_id, hw_slot);
			mutex_unlock(&cmn_reg_lock);
			return -EINVAL;
		}

		hw_ip_phys = &hardware->hw_ip[hw_slot];

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num)
		is_hw_cstat_core_init(hw_ip, instance);

	mutex_unlock(&cmn_reg_lock);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static void is_hw_cstat_cleanup_stat_buf(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_device_ischain *device = hw_ip->group[instance]->device;
	struct is_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	u32 sd_id;

	for (sd_id = 0; sd_id < IS_CSTAT_SUBDEV_NUM; sd_id++) {
		sd = &device->subdev_cstat[sd_id];
		if (!test_bit(IS_SUBDEV_START, &sd->state))
			continue;

		framemgr = GET_SUBDEV_FRAMEMGR(sd);
		msinfo_hw("[%s] cleanup stat bufs (%d %d)", instance, hw_ip,
				sd->name,
				framemgr->queued_count[FS_FREE],
				framemgr->queued_count[FS_PROCESS]);

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_0, flags);
		frame = peek_frame(framemgr, FS_PROCESS);
		while (frame) {
			trans_frame(framemgr, frame, FS_FREE);
			frame = peek_frame(framemgr, FS_PROCESS);
		}
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);
	}
}

static int is_hw_cstat_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_cstat *hw;

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		mswarn_hw("Not mapped. hw_map 0x%lx", instance, hw_ip, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("%s: Vvalid(%d)\n", instance, hw_ip, __func__,
		atomic_read(&hw_ip->status.Vvalid));

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

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
				mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip,
						timetowait);
				ret = -ETIME;
			}
		}
		if (-ETIME != ret)
			is_hw_cstat_s_global_enable(hw_ip, false, false);
		is_hw_cstat_wait_isr_done(hw_ip);
		is_hw_cstat_cleanup_stat_buf(hw_ip, instance);
		hw->event_state = CSTAT_INIT;
		clear_bit(HW_CONFIG, &hw_ip->state);
	}

	if (test_bit(HW_RUN, &hw_ip->state)) {
		if (IS_ENABLED(CSTAT_DDK_LIB_CALL))
			is_lib_isp_stop(hw_ip, &hw->lib[instance], instance, 1);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return ret;

	tasklet_kill(&hw->end_tasklet);

	clear_bit(HW_RUN, &hw_ip->state);

	return ret;
}

static void is_hw_cstat_update_param(struct is_hw_ip *hw_ip,
		struct is_param_region *p_region, struct cstat_param_set *p_set,
		IS_DECLARE_PMAP(pmap), u32 instance, u32 ctx)
{
	struct cstat_param *param;
	u32 pindex;

	param = &p_region->cstat[ctx];
	p_set->instance_id = instance;

	/* Sensor CFG */
	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&p_set->sensor_config, &p_region->sensor.config,
			sizeof(struct param_sensor_config));

	/* Input CFG */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_OTF_INPUT, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_DMA_INPUT, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));

	/* Output CFG */
	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_LME_DS0, ctx);
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL) && test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_lme_ds0, &param->lme_ds0,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_LME_DS1, ctx);
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL) && test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_lme_ds1, &param->lme_ds1,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_FDPIG, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_fdpig, &param->fdpig,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_SVHIST, ctx);
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL) && test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_svhist, &param->svhist,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_DRC, ctx);
	if (IS_ENABLED(CSTAT_DDK_LIB_CALL) && test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_drc, &param->drc,
			sizeof(struct param_dma_output));

	pindex = GET_CSTAT_CTX_PINDEX(PARAM_CSTAT_CDS, ctx);
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_cds, &param->cds,
			sizeof(struct param_dma_output));
}

static void is_hw_cstat_internal_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct cstat_param_set *p_set)
{
	u32 instance = atomic_read(&hw_ip->instance);

	hw_ip->internal_fcount[instance] = frame->fcount;

	/* Disable ALL WDMAs */
	p_set->dma_output_svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_lme_ds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_lme_ds1.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_fdpig.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_cds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
}

static void is_hw_cstat_external_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct cstat_param_set *p_set)
{
	struct is_group *group;
	struct is_param_region *p_region;
	u32 b_idx = frame->cur_buf_index;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 ctx_idx;

	FIMC_BUG_VOID(!frame->shot);

	group = hw_ip->group[instance];
	hw_ip->internal_fcount[instance] = 0;

	/*
	 * Due to async shot,
	 * it should refer the param_region from the current frame,
	 * not from the device region.
	 */
	p_region = (struct is_param_region *)frame->shot->ctl.vendor_entry.parameter;

	/* For now, it only considers 2 contexts */
	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		ctx_idx = 1;
	else
		ctx_idx = 0;

	is_hw_cstat_update_param(hw_ip, p_region, p_set, frame->pmap, instance, ctx_idx);

	/* DMA settings */
	is_hardware_dma_cfg("cstat_byr_in",
			hw_ip, frame, b_idx, frame->num_buffers,
			&p_set->dma_input.cmd,
			p_set->dma_input.plane,
			p_set->input_dva,
			frame->dvaddr_buffer);

	is_hardware_dma_cfg_32bit("cstat_vhist",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_svhist.cmd,
			p_set->dma_output_svhist.plane,
			p_set->output_dva_svhist,
			frame->dva_cstat_vhist[ctx_idx]);

	is_hardware_dma_cfg_32bit("cstat_drc",
			hw_ip, frame, b_idx, frame->num_buffers,
			&p_set->dma_output_drc.cmd,
			p_set->dma_output_drc.plane,
			p_set->output_dva_drc,
			frame->txdgrTargetAddress[ctx_idx]);

	is_hardware_dma_cfg_32bit("cstat_lme_ds0",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_lme_ds0.cmd,
			p_set->dma_output_lme_ds0.plane,
			p_set->output_dva_lem_ds0,
			frame->txldsTargetAddress[ctx_idx]);

	is_hardware_dma_cfg_32bit("cstat_lme_ds1",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_lme_ds1.cmd,
			p_set->dma_output_lme_ds1.plane,
			p_set->output_dva_lem_ds1,
			frame->dva_cstat_lmeds1[ctx_idx]);

	is_hardware_dma_cfg_32bit("cstat_fdpig",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_fdpig.cmd,
			p_set->dma_output_fdpig.plane,
			p_set->output_dva_fdpig,
			frame->efdTargetAddress[ctx_idx]);

	is_hardware_dma_cfg_32bit("cstat_cds",
			hw_ip, frame, 0, 1,
			&p_set->dma_output_cds.cmd,
			p_set->dma_output_cds.plane,
			p_set->output_dva_cds,
			frame->txpTargetAddress[ctx_idx]);
}

/*
 * Set proper line interrupt timing for DDK
 *  The line interrupt timing should be in middle of FS & FE event.
 *  The FE means the final frame end interrupt which includes corex delay.
 * @base: CSTAT base address
 * @width: CSTAT input IMG width
 * @height: CSTAT input IMG height (== number of line)
 * @time_cfg: Sensor vvalid duration information
 * @en: Enable line interrupt
 */
static void is_hw_cstat_s_col_row(void __iomem *base,
		u32 width, u32 height,
		struct cstat_time_cfg *time_cfg,
		bool en)
{
	u32 usec_per_line, int_delay, row;

	if (!en) {
		cstat_hw_s_int_on_col_row(base, false, CSTAT_BCROP0, 0, 0);
		return;
	}

	if (time_cfg->vvalid && time_cfg->req_vvalid) {
		usec_per_line = ALIGN(time_cfg->vvalid, height) / height; /* usec */
		int_delay = time_cfg->req_vvalid / 2; /* Middle of req_vvalid. usec */
		row = int_delay / usec_per_line; /* lines */
	} else {
		row = height / 2;
	}

	/* row cannot exceed the CSTAT input IMG height */
	row = (row < height) ? row : (height - 1);

	dbg_hw(2, "[CSTAT]s_col_row: %dx%d vvalid %d req_vvalid %d row %d\n",
			width, height,
			time_cfg->vvalid, time_cfg->req_vvalid, row);

	cstat_hw_s_int_on_col_row(base, true, CSTAT_BCROP0, 0, row);
}

static int is_hw_cstat_init_config(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_param_set *p_set;
	struct param_otf_input *otf_in;
	struct param_dma_input *dma_in;
	struct cstat_lic_lut lic_lut;
	struct cstat_lic_cfg lic_cfg;
	struct param_sensor_config *sensor_cfg;
	struct cstat_time_cfg time_cfg;
	enum cstat_lic_mode lic_mode = CSTAT_LIC_DYNAMIC;
	u32 ch, width, height;
	u32 pixel_order;
	u32 seed;
	bool en_col_row;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	p_set = &hw->param_set[instance];

	otf_in = &p_set->otf_input;
	dma_in = &p_set->dma_input;
	ch = hw_ip->id - DEV_HW_3AA0;

	if (otf_in->cmd == OTF_INPUT_COMMAND_ENABLE) {
		hw->input = OTF;
		width = otf_in->width;
		height = otf_in->height;
		pixel_order = otf_in->order;
		en_col_row = true;
	} else if (dma_in->v_otf_enable == DMA_INPUT_VOTF_ENABLE) {
		hw->input = VOTF;
		width = dma_in->width;
		height = dma_in->height;
		pixel_order = dma_in->order;
		en_col_row = true;
	} else if (dma_in->cmd == DMA_INPUT_COMMAND_ENABLE) {
		hw->input = DMA;
		width = dma_in->width;
		height = dma_in->height;
		pixel_order = dma_in->order;
		en_col_row = false;
	} else {
		mserr_hw("No input", instance, hw_ip);
		return -EINVAL;
	}

	is_hw_cstat_ctx_init(hw_ip);

	sensor_cfg = &p_set->sensor_config;
	time_cfg.vvalid = sensor_cfg->vvalid_time;
	time_cfg.req_vvalid = sensor_cfg->req_vvalid_time;

	is_hw_cstat_s_col_row(base, width, height, &time_cfg, en_col_row);

	cstat_hw_s_post_frame_gap(base, 0);
	cstat_hw_s_input(base, hw->input);
	cstat_hw_s_int_enable(base);
	cstat_hw_s_seqid(base, 0);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		cstat_hw_s_crc_seed(base, seed);

	lic_lut.mode = lic_mode;

	lic_cfg.mode = lic_mode;
	lic_cfg.input = hw->input;
	lic_cfg.half_ppc_en = 0;

	cstat_hw_s_lic_cmn_cfg(base, &lic_lut, 0);
	cstat_hw_s_lic_cfg(base, &lic_cfg);
	cstat_hw_s_pixel_order(base, pixel_order);

	if (cstat_hw_s_sram_offset(hw_ip->regs[REG_EXT1], ch, width))
		return -EINVAL;

	msinfo_hw("init_config: %s_IN size %dx%d lic_mode %d\n",
			atomic_read(&hw_ip->instance), hw_ip,
			(hw->input == OTF) ? "OTF" : "DMA",
			width, height, lic_mode);

	return 0;
}

static void is_hw_cstat_s_stat_cfg(struct is_hw_ip *hw_ip, u32 instance,
		struct cstat_param_set *p_set)
{
	struct is_device_ischain *device = hw_ip->group[instance]->device;
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	struct is_subdev *sd;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct param_dma_output *dma_out;
	struct is_hw_cstat_stat *stat;
	unsigned long flags;
	u32 sd_id;

	for (sd_id = 0; sd_id < IS_CSTAT_SUBDEV_NUM; sd_id++) {
		sd = &device->subdev_cstat[sd_id];
		if (!test_bit(IS_SUBDEV_START, &sd->state)) {
			mswarn_hw("[%s] Not in START state 0x%x", instance, hw_ip,
					sd->name, sd->state);
			continue;
		}

		framemgr = GET_SUBDEV_FRAMEMGR(sd);

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_0, flags);
		frame = peek_frame(framemgr, FS_FREE);
		if (!frame) {
			msdbg_hw(2, "[%s] Not enough frame", instance, hw_ip,
					sd->name);
			frame_manager_print_queues(framemgr);
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);
			continue;
		}
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);

		frame->fcount = p_set->fcount;

		dma_out = cstat_hw_s_stat_cfg(sd_id, frame->dvaddr_buffer[0], p_set);
		if (!dma_out)
			continue;

		stat = &hw->stat[frame->index];

		/* Save the current stat info */
		switch (sd_id) {
		case IS_CSTAT_PRE_THUMB:
			memcpy(&stat->pre.dma, dma_out, sizeof(struct param_dma_output));
			stat->pre.kva = frame->kvaddr_buffer[0];
			stat->pre.bytes = dma_out->width * dma_out->height;
			break;
		case IS_CSTAT_AE_THUMB:
			memcpy(&stat->ae.dma, dma_out, sizeof(struct param_dma_output));
			stat->ae.kva = frame->kvaddr_buffer[0];
			stat->ae.bytes = dma_out->width * dma_out->height;
			break;
		case IS_CSTAT_AWB_THUMB:
			memcpy(&stat->awb.dma, dma_out, sizeof(struct param_dma_output));
			stat->awb.kva = frame->kvaddr_buffer[0];
			stat->awb.bytes = dma_out->width * dma_out->height;
			break;
		case IS_CSTAT_RGBY_HIST:
			memcpy(&stat->rgby.dma, dma_out, sizeof(struct param_dma_output));
			stat->rgby.kva = frame->kvaddr_buffer[0];
			stat->rgby.bytes = dma_out->width * dma_out->height;
			break;
		case IS_CSTAT_CDAF:
		default:
			continue;
		};

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_0, flags);
		trans_frame(framemgr, frame, FS_PROCESS);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_0, flags);
	}
}

static int is_hw_cstat_s_iq_regs(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct is_hw_cstat_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size, i;
	bool configured = false;

	iq_set = &hw->iq_set;
	base += CSTAT_COREX_SDW_OFFSET;

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;

		if (!test_bit(HW_CONFIG, &hw_ip->state) ||
				hw->cur_iq_set.size != regs_size ||
				memcmp(hw->cur_iq_set.regs, regs,
					sizeof(struct cr_set) * regs_size) != 0) {

			for (i = 0; i < regs_size; i++) {
				if ((regs[i].reg_addr == 0x4C4C) || /* byr_cgras_lut_access */
				    (regs[i].reg_addr == 0x4C50) || /* byr_cgras_lut_access_setb */
				    (regs[i].reg_addr == 0x588c) || /* rgb_lumaShading_lut_access_seta */
				    (regs[i].reg_addr == 0x5890) || /* rgb_lumaShading_lut_access_seta_setb */
				    (regs[i].reg_addr == 0x4C48) || /* byr_cgras_lut_start_add */
				    (regs[i].reg_addr == 0x5888))   /* rgb_lumaShading_lut_start_add */
					writel_relaxed(regs[i].reg_data,
						(base - CSTAT_COREX_SDW_OFFSET) + regs[i].reg_addr);
				else
					writel_relaxed(regs[i].reg_data, base + regs[i].reg_addr);
			}

			memcpy(hw->cur_iq_set.regs, regs,
					sizeof(struct cr_set) * regs_size);

			hw->cur_iq_set.size = regs_size;
		}

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
				instance, hw_ip,
				atomic_read(&hw_ip->fcount),
				iq_set->fcount, iq_set->state, hw->cur_iq_set.fcount);
		return -EINVAL;
	}

	return 0;
}

static void is_hw_cstat_s_block_reg(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	msdbg_hw(4, "%s\n", p_set->instance_id, hw_ip, __func__);

	cstat_hw_s_default_blk_cfg(hw_ip->regs[REG_SETA]);
}

static int is_hw_cstat_g_bitwidth(u32 bitwidth)
{
	switch (bitwidth) {
	case 10:
		return INPUT_10B;
	case 12:
		return INPUT_12B;
	case 14:
		return INPUT_14B;
	default:
		return -EINVAL;
	}
}

static int is_hw_cstat_s_size_regs(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_size_cfg *size_cfg;
	struct is_crop in_crop, cur_crop;
	u32 full_w, full_h;
	enum cstat_input_bitwidth bitwidth;
	u32 crop_id;
	bool crop_en;
	struct cstat_grid_cfg grid_cfg;
	struct is_cstat_config *cfg;
	u32 start_pos_x, start_pos_y;
	u32 binned_sensor_width, binned_sensor_height;
	u32 sensor_crop_x, sensor_crop_y;
	u32 prev_binning_x, prev_binning_y;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	size_cfg = &hw->size;
	cfg = &hw->config;

	if (hw->input == OTF) {
		in_crop.x = p_set->otf_input.bayer_crop_offset_x;
		in_crop.y = p_set->otf_input.bayer_crop_offset_y;
		in_crop.w = p_set->otf_input.bayer_crop_width;
		in_crop.h = p_set->otf_input.bayer_crop_height;
		full_w = p_set->otf_input.width;
		full_h = p_set->otf_input.height;
		bitwidth = is_hw_cstat_g_bitwidth(p_set->otf_input.bitwidth);
	} else {
		in_crop.x = p_set->dma_input.bayer_crop_offset_x;
		in_crop.y = p_set->dma_input.bayer_crop_offset_y;
		in_crop.w = p_set->dma_input.bayer_crop_width;
		in_crop.h = p_set->dma_input.bayer_crop_height;
		full_w = p_set->dma_input.width;
		full_h = p_set->dma_input.height;
		bitwidth = is_hw_cstat_g_bitwidth(p_set->dma_input.msb + 1);
	}

	if (bitwidth < 0) {
		mserr_hw("Invalid bitwidth %d", p_set->instance_id, hw_ip, bitwidth);
		return -EINVAL;
	}

	if (cstat_hw_s_format(base, full_w, full_h, bitwidth))
		return -EINVAL;

	/* CROP configuration before BNS */
	cur_crop.w = full_w;
	cur_crop.h = full_h;
	for (crop_id = 0; crop_id < CSTAT_CROP_BNS; crop_id++) {
		if (cstat_hw_is_bcrop(crop_id)) {
			size_cfg->bcrop.x = cur_crop.x = in_crop.x;
			size_cfg->bcrop.y = cur_crop.y = in_crop.y;
			size_cfg->bcrop.w = cur_crop.w = in_crop.w;
			size_cfg->bcrop.h = cur_crop.h = in_crop.h;
			crop_en = true;
		} else {
			cur_crop.x = 0;
			cur_crop.y = 0;
			crop_en = false;
		}

		cstat_hw_s_crop(base, crop_id, crop_en, &cur_crop, p_set, NULL);
	}

	/* GRID configuration for CGRAS */
	/* Center */
	if (cfg->sensor_center_x == 0 || cfg->sensor_center_y == 0) {
		cfg->sensor_center_x = (p_set->sensor_config.calibrated_width >> 1);
		cfg->sensor_center_y = (p_set->sensor_config.calibrated_height >> 1);
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)",
				p_set->instance_id, hw_ip, __func__,
				cfg->sensor_center_x, cfg->sensor_center_y);
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	grid_cfg.binning_x = (1024ULL * p_set->sensor_config.sensor_binning_ratio_x *
			p_set->sensor_config.bns_binning_ratio_x) / 1000 / 1000;
	grid_cfg.binning_y = (1024ULL * p_set->sensor_config.sensor_binning_ratio_y *
			p_set->sensor_config.bns_binning_ratio_y) / 1000 / 1000;
	if (grid_cfg.binning_x == 0 || grid_cfg.binning_y == 0) {
		grid_cfg.binning_x = 1024;
		grid_cfg.binning_y = 1024;
		msdbg_hw(2, "%s:[CGR] calculated binning(0,0). Fix to (%d,%d)",
				p_set->instance_id, hw_ip, __func__, 1024, 1024);
	}

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned_sensor_crop */
	if (p_set->sensor_config.freeform_sensor_crop_enable == 1) {
		sensor_crop_x = p_set->sensor_config.freeform_sensor_crop_offset_x;
		sensor_crop_y = p_set->sensor_config.freeform_sensor_crop_offset_y;
	} else {
		binned_sensor_width = p_set->sensor_config.calibrated_width * 1000 /
				p_set->sensor_config.sensor_binning_ratio_x;
		binned_sensor_height = p_set->sensor_config.calibrated_height * 1000 /
				p_set->sensor_config.sensor_binning_ratio_y;
		sensor_crop_x = ((binned_sensor_width - p_set->sensor_config.width) >> 1)
				& (~0x1);
		sensor_crop_y = ((binned_sensor_height - p_set->sensor_config.height) >> 1)
				& (~0x1);
	}

	start_pos_x = (sensor_crop_x * p_set->sensor_config.sensor_binning_ratio_x) / 1000;
	start_pos_y = (sensor_crop_y * p_set->sensor_config.sensor_binning_ratio_y) / 1000;

	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (cfg->sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (cfg->sensor_center_y - start_pos_y));

	cstat_hw_s_grid_cfg(base, CSTAT_GRID_CGRAS, &grid_cfg);

	msdbg_hw(2, "%s:[CGR] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			p_set->sensor_config.calibrated_width,
			p_set->sensor_config.calibrated_height,
			cfg->sensor_center_x,
			cfg->sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[CGR] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);

	/* BNS configuration */
	cstat_hw_s_bns_cfg(base, &cur_crop, size_cfg);

	/* Total_binning = sensor_binning * csis_bns_binning * cstat_bns_binning */
	switch (size_cfg->bns_r) {
	case CSTAT_BNS_X1P0:
		size_cfg->bns_binning = 1000;
		break;
	case CSTAT_BNS_X1P25:
		size_cfg->bns_binning = 1250;
		break;
	case CSTAT_BNS_X1P5:
		size_cfg->bns_binning = 1500;
		break;
	case CSTAT_BNS_X1P75:
		size_cfg->bns_binning = 1750;
		break;
	case CSTAT_BNS_X2P0:
		size_cfg->bns_binning = 2000;
		break;
	default:
		mserr_hw("unidentified bns_r(%d) use CSTAT_BNS_X2P0", p_set->instance_id, hw_ip);
		size_cfg->bns_binning = 2000;
		break;
	}

	/* CROP configuration after BNS */
	for (crop_id = CSTAT_CROP_BNS; crop_id < CSTAT_CROP_NUM; crop_id++) {
		cur_crop.x = 0;
		cur_crop.y = 0;
		crop_en = false;

		cstat_hw_s_crop(base, crop_id, crop_en, &cur_crop, p_set, size_cfg);
	}

	/* GRID configuration for Luma_shading */
	/* Center */
	if (cfg->sensor_center_x == 0 || cfg->sensor_center_y == 0) {
		cfg->sensor_center_x = (p_set->sensor_config.calibrated_width >> 1);
		cfg->sensor_center_y = (p_set->sensor_config.calibrated_height >> 1);
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)",
				p_set->instance_id, hw_ip, __func__,
				cfg->sensor_center_x, cfg->sensor_center_y);
	}

	prev_binning_x = (1024ULL * p_set->sensor_config.sensor_binning_ratio_x *
			p_set->sensor_config.bns_binning_ratio_x) / 1000 / 1000;
	grid_cfg.binning_x = (prev_binning_x * size_cfg->bns_binning) / 1000;
	prev_binning_y = (1024ULL * p_set->sensor_config.sensor_binning_ratio_y *
			p_set->sensor_config.bns_binning_ratio_y) / 1000 / 1000;
	grid_cfg.binning_y = (prev_binning_y * size_cfg->bns_binning) / 1000;
	if (grid_cfg.binning_x == 0 || grid_cfg.binning_y == 0) {
		grid_cfg.binning_x = 1024;
		grid_cfg.binning_y = 1024;
		msdbg_hw(2, "%s:[CGR] calculated binning(0,0). Fix to (%d,%d)",
				p_set->instance_id, hw_ip, __func__, 1024, 1024);
	}
	
	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned(CSTAT_bcrop) + (unbinned)sensor_crop */
	/* Not use crop_bns. if it is used, please, include it above formula */
	if (p_set->sensor_config.freeform_sensor_crop_enable == 1) {
		sensor_crop_x = p_set->sensor_config.freeform_sensor_crop_offset_x;
		sensor_crop_y = p_set->sensor_config.freeform_sensor_crop_offset_y;
	} else {
		binned_sensor_width = p_set->sensor_config.calibrated_width * 1000 /
				p_set->sensor_config.sensor_binning_ratio_x;
		binned_sensor_height = p_set->sensor_config.calibrated_height * 1000 /
				p_set->sensor_config.sensor_binning_ratio_y;
		sensor_crop_x = ((binned_sensor_width - p_set->sensor_config.width) >> 1)
				& (~0x1);
		sensor_crop_y = ((binned_sensor_height - p_set->sensor_config.height) >> 1)
				& (~0x1);
	}

	start_pos_x = ((size_cfg->bcrop.x * prev_binning_x) / 1024) +
			(sensor_crop_x * p_set->sensor_config.sensor_binning_ratio_x) / 1000;
	start_pos_y = ((size_cfg->bcrop.y * prev_binning_y) / 1024) +
			(sensor_crop_y * p_set->sensor_config.sensor_binning_ratio_y) / 1000;
	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (cfg->sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (cfg->sensor_center_y - start_pos_y));

	cstat_hw_s_grid_cfg(base, CSTAT_GRID_LSC, &grid_cfg);

	msdbg_hw(2, "%s:[LSC] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			p_set->sensor_config.calibrated_width,
			p_set->sensor_config.calibrated_height,
			cfg->sensor_center_x,
			cfg->sensor_center_y,
			sensor_crop_x, sensor_crop_y,
			start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[LSC] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
			p_set->instance_id, hw_ip, __func__,
			grid_cfg.binning_x,
			grid_cfg.binning_y,
			grid_cfg.step_x,
			grid_cfg.step_y,
			grid_cfg.crop_x,
			grid_cfg.crop_y,
			grid_cfg.crop_radial_x,
			grid_cfg.crop_radial_y);

	msdbg_hw(2, "%s:[CGR,LSC] bcrop: (%d,%d %dx%d), bns: (%dx%d, ratio:%d)\n",
			p_set->instance_id, hw_ip, __func__,
			size_cfg->bcrop.x, size_cfg->bcrop.y,
			size_cfg->bcrop.w, size_cfg->bcrop.h,
			cur_crop.w, cur_crop.h, size_cfg->bns_r);

	return 0;
}

static int is_hw_cstat_s_ds(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	struct is_hw_cstat *hw;
	void __iomem *base;
	struct cstat_size_cfg *size_cfg;
	u32 dma_id;
	int ret = 0;

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	base = hw_ip->regs[REG_SETA];
	size_cfg = &hw->size;

	for (dma_id = CSTAT_W_LMEDS0; dma_id < CSTAT_DMA_NUM; dma_id++)
		ret |= cstat_hw_s_ds_cfg(base, dma_id, size_cfg, p_set);

	FIMC_BUG(ret);

	return 0;
}

static int is_hw_cstat_s_dma(struct is_hw_ip *hw_ip, struct cstat_param_set *p_set)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	u32 dma_id;
	int disable;

	/* RDMA cfg */
	for (dma_id = CSTAT_R_BYR; dma_id < CSTAT_RDMA_NUM; dma_id++) {
		if (cstat_hw_s_rdma_cfg(&hw->dma[dma_id], p_set, (hw_ip->num_buffers & 0xffff)))
			return -EINVAL;
	}

	/* WDMA cfg */
	for (dma_id = CSTAT_W_RGBYHIST; dma_id < CSTAT_DMA_NUM; dma_id++) {
		disable = test_bit(dma_id, &dbg_info.dma_id);
		if (cstat_hw_s_wdma_cfg(hw_ip->regs[REG_SETA], &hw->dma[dma_id],
					p_set, (hw_ip->num_buffers & 0xffff), disable))
			return -EINVAL;
	}

	return 0;
}

static void is_hw_cstat_s_fro(struct is_hw_ip *hw_ip, u32 instance)
{
	return cstat_hw_s_fro(hw_ip->regs[REG_SETA], (hw_ip->num_buffers & 0xffff), 1);
}

static int is_hw_cstat_trigger(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_core *core = is_get_is_core();
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;
	void __iomem *base = hw_ip->regs[REG_SETA];
	struct param_sensor_config *sensor_cfg;
	struct cstat_time_cfg time_cfg;

	if (!core) {
		mserr_hw("core is NULL", instance, hw_ip);
		return -ENODEV;
	}

	switch (hw->input) {
	case OTF:
	case VOTF:
		if (!test_bit(HW_CONFIG, &hw_ip->state)) {
			is_hw_cstat_s_fro(hw_ip, instance);
			is_hw_cstat_s_global_enable(hw_ip, true, hw->hw_fro_en);

			if (test_bit(HW_SUSPEND, &hw_ip->state))
				cstat_hw_s_corex_enable(base, false);
		} else if (test_and_clear_bit(HW_SUSPEND, &hw_ip->state)) {
			cstat_hw_s_corex_enable(base, true);
		}

		/* Set COREX hw_trigger delay */
		sensor_cfg = &hw->param_set[instance].sensor_config;
		time_cfg.vvalid = sensor_cfg->vvalid_time;
		time_cfg.req_vvalid = sensor_cfg->req_vvalid_time;
		time_cfg.clk = is_dvfs_get_freq(IS_DVFS_CSIS) * MHZ;
		time_cfg.fro_en = hw->hw_fro_en;

		cstat_hw_s_corex_delay(base, &time_cfg);
		cstat_hw_corex_trigger(base);
		break;
	case DMA:
		return cstat_hw_s_one_shot_enable(base);
	default:
		mserr_hw("Not supported input %d", instance, hw_ip, hw->input);
		return -EINVAL;
	}

	return 0;
}

static int is_hw_cstat_shot(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_group *group;
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 b_idx, batch_num;
	bool do_blk_cfg = true, do_lib_shot;
	ulong debug_iq = (unsigned long) is_get_debug_param(IS_DEBUG_PARAM_IQ);

	msdbgs_hw(2, "[F%d][T%d]shot\n", instance, hw_ip, frame->fcount, frame->type);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	group = hw_ip->group[instance];
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];
	b_idx = frame->cur_buf_index;

	/* For the 1st shot on sub-stream, put it in suspend state */
	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state) &&
				!test_bit(HW_CONFIG, &hw_ip->state)) {
		set_bit(HW_SUSPEND, &hw_ip->state);
		do_lib_shot = false;
	} else {
		set_bit(hw_ip->id, &frame->core_flag);
		do_lib_shot = IS_ENABLED(CSTAT_DDK_LIB_CALL);
	}


	if (frame->type == SHOT_TYPE_INTERNAL)
		is_hw_cstat_internal_shot(hw_ip, frame, p_set);
	else
		is_hw_cstat_external_shot(hw_ip, frame, p_set);

	p_set->instance_id = instance;
	p_set->fcount = frame->fcount;

	/* multi-buffer */
	hw_ip->num_buffers = frame->num_buffers;
	hw->hw_fro_en = (hw_ip->num_buffers & 0xffff) > 1 ? true : false;
	batch_num = hw_ip->framemgr->batch_num;

	/* Update only for SW_FRO scenario */
	if (batch_num > 1 && hw_ip->num_buffers == 1) {
		hw_ip->num_buffers |= batch_num << SW_FRO_NUM_SHIFT;
		hw_ip->num_buffers |= b_idx << CURR_INDEX_SHIFT;
	}

	/* Pre-shot */
	if (do_lib_shot) {
		if (frame->shot) {
			if (is_lib_isp_set_ctrl(hw_ip, &hw->lib[instance], frame))
				mserr_hw("set_ctrl fail", instance, hw_ip);
		}

		if (is_lib_isp_sensor_update_control(&hw->lib[instance], instance,
					frame->fcount, frame->shot))
			mserr_hw("sensor_update_control fail",
					instance, hw_ip);
	}

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		ret = is_hw_cstat_init_config(hw_ip, instance, frame);
		if (ret) {
			mserr_hw("init_config fail", instance, hw_ip);
			return ret;
		}
	}

	cstat_hw_s_corex_type(hw_ip->regs[REG_SETA], COREX_IGNORE);

	ret = is_hw_cstat_s_size_regs(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	/* Shot */
	if (do_lib_shot) {
		ret = is_lib_isp_shot(hw_ip, &hw->lib[instance], p_set, frame->shot);
		if (ret)
			return ret;

		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			ret = is_hw_cstat_s_iq_regs(hw_ip, instance);
			_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret)
				mserr_hw("is_hw_cstat_s_iq_regs is fail", instance, hw_ip);
			else
				do_blk_cfg = false;
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}

		is_hw_cstat_s_stat_cfg(hw_ip, instance, p_set);
	}

	if (unlikely(do_blk_cfg))
		is_hw_cstat_s_block_reg(hw_ip, p_set);

	ret = is_hw_cstat_s_ds(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_ds fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = is_hw_cstat_s_dma(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_dma fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	ret = is_hw_cstat_trigger(hw_ip, instance);
	if (ret) {
		mserr_hw("trigger fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	set_bit(HW_CONFIG, &hw_ip->state);

	return 0;

shot_fail_recovery:
	if (do_lib_shot)
		is_lib_isp_reset_recovery(hw_ip, &hw->lib[instance], instance);

	return ret;
}

static int is_hw_cstat_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_group *group;
	struct is_hw_cstat *hw;
	struct cstat_param_set *p_set;
	u32 ctx_idx;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	group = hw_ip->group[instance];
	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	p_set = &hw->param_set[instance];

	/* For now, it only considers 2 contexts */
	if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
		ctx_idx = 1;
	else
		ctx_idx = 0;

	is_hw_cstat_update_param(hw_ip, &region->parameter, p_set, pmap, instance, ctx_idx);

	return 0;
}

static int is_hw_cstat_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame,
	ulong hw_map)
{
	int ret;
	struct is_hw_cstat *hw;
	u32 instance = atomic_read(&hw_ip->instance);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;


	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		hw = (struct is_hw_cstat *)hw_ip->priv_info;

		ret = is_lib_isp_get_meta(hw_ip, &hw->lib[instance], frame);
		if (ret) {
			mserr_hw("get_meta fail", instance, hw_ip);
			return ret;
		}
	}

	return 0;
}

static int is_hw_cstat_get_cap_meta(struct is_hw_ip *hw_ip, ulong hw_map,
	u32 instance, u32 fcount, u32 size, ulong addr)
{
	int ret = 0;
	struct is_hw_cstat *hw_cstat;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	FIMC_BUG(!hw_ip->priv_info);
	hw_cstat = (struct is_hw_cstat *)hw_ip->priv_info;
	if (unlikely(!hw_cstat)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		ret = is_lib_isp_get_cap_meta(hw_ip, &hw_cstat->lib[instance],
				instance, fcount, size, addr);
		if (ret)
			mserr_hw("get_cap_meta fail", instance, hw_ip);
	}

	return ret;
}

static int is_hw_cstat_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;

	if (done_type == IS_SHOT_TIMEOUT)
		is_hw_cstat_print_stall_status(hw_ip);

	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = is_hardware_frame_done(hw_ip, frame, -1,
				IS_HW_CORE_END, done_type, false);

	return ret;
}


static int is_hw_cstat_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0, flag;
	struct is_hw_ip_setfile *setfile;
	struct is_hw_cstat *hw;
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

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
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

static int is_hw_cstat_apply_setfile(struct is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret;
	struct is_hw_cstat *hw;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;
	u32 setfile_index;
	ulong cal_addr;

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

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
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

static int is_hw_cstat_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_cstat *hw;
	struct is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;
	u32 i;

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

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
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

static int is_hw_cstat_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_cstat *hw;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	is_hw_cstat_ctx_init(hw_ip);

	if (IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		hw = (struct is_hw_cstat *)hw_ip->priv_info;

		ret = is_lib_isp_reset_recovery(hw_ip, &hw->lib[instance], instance);
		if (ret) {
			mserr_hw("reset_recovery fail ret(%d)",
					instance, hw_ip, ret);
			return ret;
		}
	}

	return 0;
}

static int is_hw_cstat_sensor_start(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("NOT initialized!", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Skip conditions
	 * - Sensor mode change notifier is necessary only for OTF input scenario.
	 * - This notifier is for DDK.
	 */
	if (!test_bit(IS_GROUP_OTF_INPUT, &hw_ip->group[instance]->state) ||
			!IS_ENABLED(CSTAT_DDK_LIB_CALL))
		return 0;

	msinfo_hw("sensor_start: mode_change for sensor\n", instance, hw_ip);

	framemgr = hw_ip->framemgr;
	if (!framemgr) {
		mserr_hw("There is no framemgr", instance, hw_ip);
		return -EINVAL;
	}

	framemgr_e_barrier(framemgr, 0);
	frame = peek_frame(framemgr, FS_HW_CONFIGURE);
	framemgr_x_barrier(framemgr, 0);
	if (!frame) {
		mswarn_hw("sensor_start: No CONFIGURE frame (%d)", instance, hw_ip,
				framemgr->queued_count[FS_HW_CONFIGURE]);

		return 0;
	} else if (!frame->shot) {
		mswarn_hw("sensor_start:[F%d] No shot", instance, hw_ip,
				frame->fcount);

		return 0;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	if (is_lib_isp_sensor_info_mode_chg(&hw->lib[instance], instance, frame->shot)) {
		mserr_hw("[F%d]Failed to lib_isp_sensor_info_mode_chg", instance, hw_ip,
				frame->fcount);
		return -EINVAL;
	}

	return 0;
}

static int is_hw_cstat_change_chain(struct is_hw_ip *hw_ip, u32 instance,
	u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	struct is_hw_cstat *hw;
	u32 curr_id;
	u32 next_hw_id;
	int hw_slot;
	struct is_hw_ip *next_hw_ip;
	struct is_group *group;
	enum exynos_sensor_position sensor_position;

	curr_id = hw_ip->id - DEV_HW_3AA0;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip,
			curr_id, next_id);
		goto p_err;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("failed to get HW 3AA", instance, hw_ip);
		return -ENODEV;
	}

	/* Call DDK */
	ret = is_lib_isp_change_chain(hw_ip, &hw->lib[instance], instance, next_id);
	if (ret) {
		mserr_hw("is_lib_isp_change_chain", instance, hw_ip);
		return ret;
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
	ret = is_hw_cstat_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_cstat_disable is fail", instance, hw_ip);
		return -EINVAL;
	}

	/*
	 * Copy instance information.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be referred at close time.
	 */
	((struct is_hw_cstat *)(next_hw_ip->priv_info))->lib[instance]
		= hw->lib[instance];
	((struct is_hw_cstat *)(next_hw_ip->priv_info))->param_set[instance]
		= hw->param_set[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];

	sensor_position = hardware->sensor_position[instance];
	next_hw_ip->setfile[sensor_position] = hw_ip->setfile[sensor_position];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	/* This is for increasing run_rsccount */
	ret = is_hw_cstat_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_cstat_enable is fail", instance, next_hw_ip);
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
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip, atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip, atomic_read(&next_hw_ip->rsccount));
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
p_err:
	return ret;
}

static int is_hw_cstat_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct is_hw_cstat *hw;
	struct is_hw_cstat_iq *iq_set;
	ulong flag = 0;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;
	iq_set = &hw->iq_set;

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	msdbg_hw(2, "[F%d]Store IQ regs set: %p, size(%d)\n", instance, hw_ip,
			fcount, regs, regs_size);

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, (sizeof(struct cr_set) * regs_size));

	set_bit(CR_SET_CONFIG, &iq_set->state);

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return 0;
}

int is_hw_cstat_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	void __iomem *base;
	struct is_common_dma *dma;
	struct is_hw_cstat *hw_cstat = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		base = hw_ip->regs[REG_SETA];
		cstat_hw_dump(base);
		break;
	case IS_REG_DUMP_DMA:
		for (i = CSTAT_R_BYR; i < CSTAT_RDMA_NUM; i++) {
			hw_cstat = (struct is_hw_cstat *)hw_ip->priv_info;
			if (!hw_cstat)
				break;
			dma = &hw_cstat->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = CSTAT_W_RGBYHIST; i < CSTAT_DMA_NUM; i++) {
			hw_cstat = (struct is_hw_cstat *)hw_ip->priv_info;
			if (!hw_cstat)
				break;
			dma = &hw_cstat->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_cstat_set_config(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, void *conf)
{
	struct is_hw_cstat *hw;
	struct is_cstat_config *cstat_config = (struct is_cstat_config *)conf;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	msdbg_hw(2, "[F%d] set_config\n", instance, hw_ip, fcount);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (hw->config.sensor_center_x != cstat_config->sensor_center_x ||
	    hw->config.sensor_center_y != cstat_config->sensor_center_y)
		msinfo_hw("[F:%d] CSTAT cgras/luma_shading sensor_center(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->sensor_center_x,
				cstat_config->sensor_center_y);

	if (hw->config.lmeds_bypass != cstat_config->lmeds_bypass ||
	    hw->config.lmeds_w != cstat_config->lmeds_w ||
	    hw->config.lmeds_h != cstat_config->lmeds_h)
		msinfo_hw("[F:%d] CSTAT lmeds_bypass(%d), lmeds0(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->lmeds_bypass,
				cstat_config->lmeds_w, cstat_config->lmeds_h);

#if defined(ENABLE_LMEDS1)
	if (hw->config.lmeds1_w != cstat_config->lmeds1_w ||
	    hw->config.lmeds1_h != cstat_config->lmeds1_h)
		msinfo_hw("[F:%d] CSTAT lmeds1(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->lmeds1_w, cstat_config->lmeds1_h);
#endif

	if (hw->config.drcclct_bypass != cstat_config->drcclct_bypass ||
	    hw->config.drc_grid_w != cstat_config->drc_grid_w ||
	    hw->config.drc_grid_h != cstat_config->drc_grid_h)
		msinfo_hw("[F:%d] CSTAT drcclct_bypass(%d), drc_grid(%d x %d)\n",
				instance, hw_ip, fcount,
				cstat_config->drcclct_bypass,
				cstat_config->drc_grid_w, cstat_config->drc_grid_h);

	if (hw->config.vhist_bypass != cstat_config->vhist_bypass ||
	    hw->config.vhist_grid_num != cstat_config->vhist_grid_num)
		msinfo_hw("[F:%d] CSTAT vhist_bypass(%d), vhist_grid(%d)\n",
				instance, hw_ip, fcount,
				cstat_config->vhist_bypass,
				cstat_config->vhist_grid_num, cstat_config->vhist_grid_num);

	memcpy(&hw->config, conf, sizeof(hw->config));

	cstat_hw_s_dma_cfg(&hw->param_set[instance], &hw->config);

	return 0;
}

static int is_hw_cstat_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw = (struct is_hw_cstat *)hw_ip->priv_info;

	if (!hw->is_sfr_dumped) {
		cstat_hw_dump(hw_ip->regs[REG_SETA]);
		hw->is_sfr_dumped = true;
	}

	return is_lib_notify_timeout(&hw->lib[instance], instance);
}

static void is_hw_cstat_suspend(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_cstat *hw;

	if (!IS_ENABLED(CSTAT_DDK_LIB_CALL)) {
		return;
	} else if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		mswarn_hw("Not configured. Skip suspend.", instance, hw_ip);
		return;
	}

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	msinfo_hw("suspend\n", instance, hw_ip);

	/* DDK will wait the last FRAME_END event to do deferred stop sequence */
	is_lib_isp_stop(hw_ip, &hw->lib[instance], instance, 0);

	set_bit(HW_SUSPEND, &hw_ip->state);
}

const static struct is_hw_ip_ops is_hw_cstat_ops = {
	.open			= is_hw_cstat_open,
	.init			= is_hw_cstat_init,
	.deinit			= is_hw_cstat_deinit,
	.close			= is_hw_cstat_close,
	.enable			= is_hw_cstat_enable,
	.disable		= is_hw_cstat_disable,
	.shot			= is_hw_cstat_shot,
	.set_param		= is_hw_cstat_set_param,
	.get_meta		= is_hw_cstat_get_meta,
	.get_cap_meta		= is_hw_cstat_get_cap_meta,
	.frame_ndone		= is_hw_cstat_frame_ndone,
	.load_setfile		= is_hw_cstat_load_setfile,
	.apply_setfile		= is_hw_cstat_apply_setfile,
	.delete_setfile		= is_hw_cstat_delete_setfile,
	.restore		= is_hw_cstat_restore,
	.sensor_start		= is_hw_cstat_sensor_start,
	.change_chain		= is_hw_cstat_change_chain,
	.set_regs		= is_hw_cstat_set_regs,
	.dump_regs		= is_hw_cstat_dump_regs,
	.set_config		= is_hw_cstat_set_config,
	.notify_timeout		= is_hw_cstat_notify_timeout,
	.suspend		= is_hw_cstat_suspend,
};

int is_hw_cstat_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot = -1;

	/* initialize device hardware */
	hw_ip->ops = &is_hw_cstat_ops;

	hw_slot = is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_cstat_isr1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_cstat_isr2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	return 0;
}
