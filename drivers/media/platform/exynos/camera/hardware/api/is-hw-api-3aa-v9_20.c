// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-common.h"
#include "is-hw-api-3aa.h"
#include "sfr/is-sfr-3aa-v9_20.h"
#include "csi/is-hw-csi-v5_4.h"

#define CSIS_3AA_DMA_IRQ_MASK	((0)\
				/* |(1 << CSIS_INT_DMA_LINE_END) */\
				/* |(1 << (CSIS_INT_DMA_LINE_END + 1)) */\
				/* |(1 << (CSIS_INT_DMA_LINE_END + 2)) */\
				/* |(1 << (CSIS_INT_DMA_LINE_END + 3)) */\
				|(1 << CSIS_INT_DMA_FRAME_START)\
				/* |(1 << (CSIS_INT_DMA_FRAME_START + 1)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_START + 2)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_START + 3)) */\
				|(1 << CSIS_INT_DMA_FRAME_END)\
				/* |(1 << (CSIS_INT_DMA_FRAME_END + 1)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_END + 2)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_END + 3)) */\
				|(1 << CSIS_INT_DMA_FIFO_FULL)\
				|(1 << CSIS_INT_DMA_ABORT_DONE)\
				|(1 << CSIS_INT_DMA_LASTDATA_ERROR)\
				|(1 << CSIS_INT_DMA_LASTADDR_ERROR)\
				|(1 << CSIS_INT_DMA_FSTART_IN_FLUSH)\
				/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 1)) */\
				/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 2)) */\
				/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 3)) */\
				|(1 << CSIS_INT_DMA_OVERLAP)\
				/* |(1 << (CSIS_INT_DMA_OVERLAP + 1)) */\
				/* |(1 << (CSIS_INT_DMA_OVERLAP + 2)) */\
				/* |(1 << (CSIS_INT_DMA_OVERLAP + 3)) */\
				|(1 << CSIS_INT_DMA_FRAME_DROP)\
				/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 1)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 2)) */\
				/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 3)) */\
				/* |(1 << CSIS_INT_DMA_C2COM_LOST_FLUSH) */\
				/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 1)) */\
				/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 2)) */\
				/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 3)) */\
				)

#define CSIS_3AA_DMA_CFG_ERR_IRQ_MASK	((0)\
					/* |(1 << CSIS_INT_DMA_LINE_END) */\
					/* |(1 << (CSIS_INT_DMA_LINE_END + 1)) */\
					/* |(1 << (CSIS_INT_DMA_LINE_END + 2)) */\
					/* |(1 << (CSIS_INT_DMA_LINE_END + 3)) */\
					/* |(1 << CSIS_INT_DMA_FRAME_START) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_START + 1)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_START + 2)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_START + 3)) */\
					/* |(1 << CSIS_INT_DMA_FRAME_END) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_END + 1)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_END + 2)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_END + 3)) */\
					/* |(1 << CSIS_INT_DMA_FIFO_FULL) */\
					/* |(1 << CSIS_INT_DMA_ABORT_DONE) */\
					|(1 << CSIS_INT_DMA_LASTDATA_ERROR)\
					|(1 << CSIS_INT_DMA_LASTADDR_ERROR)\
					/* |(1 << CSIS_INT_DMA_FSTART_IN_FLUSH) */\
					/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 1)) */\
					/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 2)) */\
					/* |(1 << (CSIS_INT_DMA_FSTART_IN_FLUSH + 3)) */\
					|(1 << CSIS_INT_DMA_OVERLAP)\
					/* |(1 << (CSIS_INT_DMA_OVERLAP + 1)) */\
					/* |(1 << (CSIS_INT_DMA_OVERLAP + 2)) */\
					/* |(1 << (CSIS_INT_DMA_OVERLAP + 3)) */\
					/* |(1 << CSIS_INT_DMA_FRAME_DROP) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 1)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 2)) */\
					/* |(1 << (CSIS_INT_DMA_FRAME_DROP + 3)) */\
					/* |(1 << CSIS_INT_DMA_C2COM_LOST_FLUSH) */\
					/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 1)) */\
					/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 2)) */\
					/* |(1 << (CSIS_INT_DMA_C2COM_LOST_FLUSH + 3)) */\
					)

/* IP register direct read/write */
#define TAA_COREX_SET	COREX_SET_A
#define CSIS_DMAX_OFFSET	0x400

#define TAA_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &taa_regs[R], val)
#define TAA_SET_R(base, R, val) \
	is_hw_set_reg(base + GET_COREX_OFFSET(TAA_COREX_SET), &taa_regs[R], val)
#define TAA_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base, &taa_regs[R])
#define TAA_GET_R(base, R) \
	is_hw_get_reg(base + GET_COREX_OFFSET(TAA_COREX_SET), &taa_regs[R])

#define TAA_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &taa_regs[R], &taa_fields[F], val)
#define TAA_SET_F(base, R, F, val) \
	is_hw_set_field(base + GET_COREX_OFFSET(TAA_COREX_SET), &taa_regs[R], &taa_fields[F], val)
#define TAA_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &taa_regs[R], &taa_fields[F])
#define TAA_GET_F(base, R, F) \
	is_hw_get_field(base, GET_COREX_OFFSET(TAA_COREX_SET), &taa_regs[R], &taa_fields[F])

#define TAA_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &taa_fields[F], val)
#define TAA_GET_V(val, F) \
	is_hw_get_field_value(val, &taa_fields[F])

#define CSIS_DMAX_CHX_SET_R(base, R, val) \
	is_hw_set_reg(base, &csi_dmax_chx_regs[R], val)
#define CSIS_DMAX_CHX_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &csi_dmax_chx_fields[F], val)
#define CSIS_DMAX_CHX_GET_R(base, R) \
	is_hw_get_reg(base, &csi_dmax_chx_regs[R])
#define CSIS_DMAX_SET_R(base, R, val) \
	is_hw_set_reg(base + CSIS_DMAX_OFFSET, &csi_dmax_regs[R], val)
#define CSIS_DMAX_GET_R(base, R) \
	is_hw_get_reg(base + CSIS_DMAX_OFFSET, &csi_dmax_regs[R])


#define TAA_TRY_COUNT			(10000)
#define TAA_LUT_REG_CNT			(825)	/* CGRAS */
#define TAA_LUT_NUM			(2)

/* Guided value */
#define TAA_MIN_HBLNK			(45)
#define TAA_DRAIN_TOT_LINE		(12)
#define TAA_CDAF_STAT_LENGTH		(181)

/* Constraints */
#define TAA_MAX_WIDTH			(2880)

u32 __taa_hw_occurred_int1(u32 status, enum taa_event_type type)
{
	u32 mask;

	switch (type) {
	case TAA_EVENT_FRAME_START:
		mask = 1 << INTR1_TAA_FRAME_START;
		break;
	case TAA_EVENT_FRAME_LINE:
		mask = 1 << INTR1_TAA_FRAME_INT_ON_ROW_COL_INFO;
		break;
	case TAA_EVENT_FRAME_END:
		mask = 1 << INTR1_TAA_FRAME_END_INTERRUPT;
		break;
	case TAA_EVENT_FRAME_COREX:
		mask = 1 << INTR1_TAA_COREX_END_INT_0;
		break;
	case TAA_EVENT_ERR:
		mask = INT1_ERR_MASK;
		break;
	default:
		return 0;
	}
	return status & mask;
}

u32 __taa_hw_occurred_int2(unsigned int status, enum taa_event_type type)
{
	u32 mask;

	switch (type) {
	case TAA_EVENT_FRAME_CDAF:
		mask = 1 << INTR2_TAA_CDAF_INT;
		break;
	case TAA_EVENT_ERR:
		mask = INT2_ERR_MASK;
		break;
	default:
		return 0;
	}
	return status & mask;
}

u32 __taa_hw_occurred_int3_4(unsigned int status, enum taa_event_type type)
{
	u32 mask;

	switch (type) {
	case TAA_EVENT_FRAME_START:
		mask = 1 << CSIS_INT_DMA_FRAME_START;
		break;
	case TAA_EVENT_FRAME_END:
		mask = 1 << CSIS_INT_DMA_FRAME_END;
		break;
	case TAA_EVENT_WDMA_ABORT_DONE:
		mask = 1 << CSIS_INT_DMA_ABORT_DONE;
		break;
	case TAA_EVENT_WDMA_FRAME_DROP:
		mask = 1 << CSIS_INT_DMA_FRAME_DROP;
		break;
	case TAA_EVENT_WDMA_CFG_ERR:
		mask = CSIS_3AA_DMA_CFG_ERR_IRQ_MASK;
		break;
	case TAA_EVENT_WDMA_FIFOFULL_ERR:
		mask = 1 << CSIS_INT_DMA_FIFO_FULL;
		break;
	case TAA_EVENT_WDMA_FSTART_IN_FLUSH_ERR:
		mask = 1 << CSIS_INT_DMA_FSTART_IN_FLUSH;
		break;
	default:
		return 0;
	}
	return status & mask;
}

int taa_hw_s_c0_reset(void __iomem *base)
{
	u32 try_cnt = 0;

	info_hw("[3AA] reset\n");

	/* 2: Core + Configuration register reset for FRO controller */
	TAA_SET_R_DIRECT(base, TAA_R_FRO_SW_RESET, 2);

	TAA_SET_R_DIRECT(base, TAA_R_SW_RESET, 0x1);

	while (TAA_GET_R_DIRECT(base, TAA_R_SW_RESET)) {
		udelay(3); /* 3us * 10000 = 30ms */

		if (++try_cnt >= TAA_TRY_COUNT) {
			err_hw("[3AA] Failed to sw reset\n");
			return -EBUSY;
		}
	}

	return 0;
}

int taa_hw_s_c0_control_init(void __iomem *base)
{
	TAA_SET_R_DIRECT(base, TAA_R_IP_PROCESSING, 1);
	/*
	 * dmaclients_error_irq,                               //  9
	 * frame_start_before_frame_end_corrupted, //  8
	 * cinfifo_stream_overflow_sig,                     //  7
	 * cinfifo_columns_error_int,                        //  6
	 * cinfifo_lines_error_int,                             //  5
	 * cinfifo_total_size_error_int,                      //  4
	 * coutfifo_overflow_error,                            //  3
	 * coutfifo_col_error,                                   //  2
	 * coutfifo_line_error,                                  //  1
	 * coutfifo_size_error                                  //  0
	 */
	TAA_SET_R_DIRECT(base, TAA_R_IP_ROL_SELECT, 0x3FF);
	TAA_SET_R(base, TAA_R_IP_ROL_SELECT, 0x3FF);
	/*
	 * 0-  none- no ROL operation.
	 * 1-  single mode- when one of the selected interrupts occurs for the first time,
	 *     the state will be captured.
	 * 2-  continuous mode- every time one of the selected interrupts occurs,
	 *     the state will be re-captured.
	 */
	TAA_SET_R_DIRECT(base, TAA_R_IP_ROL_MODE, 1);
	TAA_SET_R(base, TAA_R_IP_ROL_MODE, 1);

	return 0;
}

int taa_hw_s_c0_lic(void __iomem *base, u32 num_set, u32 *offset_seta, u32 *offset_setb)
{
	u32 val;

	if (num_set != 2) {
		err_hw("invalid num_set (%d)\n", num_set);
		return -EINVAL;
	}

	val = 0;
	val = TAA_SET_V(val, TAA_F_LINE_BUFFER_OFFSET1_SETA, offset_seta[0]);
	val = TAA_SET_V(val, TAA_F_LINE_BUFFER_OFFSET2_SETA, offset_seta[1]);
	TAA_SET_R_DIRECT(base, TAA_R_LIC_3CH_BUFFER_CONFIG, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_LINE_BUFFER_OFFSET1_SETB, offset_setb[0]);
	val = TAA_SET_V(val, TAA_F_LINE_BUFFER_OFFSET2_SETB, offset_setb[1]);
	TAA_SET_R_DIRECT(base, TAA_R_LIC_3CH_BUFFER_CONFIG2, val);

	return 0;
}

int taa_hw_s_control_init(void __iomem *base)
{
	TAA_SET_R_DIRECT(base, TAA_R_IP_POST_FRAME_GAP, 0);

	/*
	 * 0-Start frame ASAP
	 * 1-Start frame upon cinfifo detection (VVALID rise)
	 */
	TAA_SET_R(base, TAA_R_IP_USE_CINFIFO_FRAME_START_IN, 1);

	/*
	 * 0- CINFIFO end interrupt will be triggered by last data
	 *  1- CINFIFO end interrupt will be triggered by VVALID fall
	 */
	TAA_SET_R(base, TAA_R_IP_CINFIFO_END_ON_VSYNC_FALL, 0);
	TAA_SET_R(base, TAA_R_IP_COUTFIFO_END_ON_VSYNC_FALL, 0);

	/*
	 * To make corrupt interrupt. It is masked following interrupt.
	 * dmaclients_error_irq,                               //  9
	 * frame_start_before_frame_end_corrupted, //  8
	 * cinfifo_stream_overflow_sig,                     //  7
	 * cinfifo_columns_error_int,                        //  6
	 * cinfifo_lines_error_int,                             //  5
	 * cinfifo_total_size_error_int,                      //  4
	 * coutfifo_overflow_error,                            //  3
	 * coutfifo_col_error,                                   //  2
	 * coutfifo_line_error,                                  //  1
	 * coutfifo_size_error                                  //  0
	 */
	TAA_SET_R(base, TAA_R_IP_CORRUPTED_INTERRUPT_ENABLE, 0xFFFF);

	/* Automatically mask global_enable if corrupted interrupt occurred. */
	TAA_SET_R_DIRECT(base, TAA_R_GLOBAL_ENABLE_STOP_CRPT, 1);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_DELAY_HW_TRIGGER, 0);

	return 0;
}

int taa_hw_s_control_config(void __iomem *base, struct taa_control_config *config)
{
	u32 val;

	val = 0;
	if (config->int_row_cord) {
		val = TAA_SET_V(val, TAA_F_IP_INT_SRC_SEL, 0);
		val = TAA_SET_V(val, TAA_F_IP_INT_COL_EN, 0);
		val = TAA_SET_V(val, TAA_F_IP_INT_ROW_EN, 1);
	}
	TAA_SET_R(base, TAA_R_IP_INT_ON_COL_ROW, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_IP_INT_COL_CORD, 0);
	val = TAA_SET_V(val, TAA_F_IP_INT_ROW_CORD, config->int_row_cord);
	TAA_SET_R(base, TAA_R_IP_INT_ON_COL_ROW_CORD, val);


	TAA_SET_R(base, TAA_R_PATH_SEL_BLC_ZSL_INPUT, config->zsl_input_type);

	val = 0;
	val = TAA_SET_V(val, TAA_F_PATH_OTF_OUT_ZSL_OUT_EN, config->zsl_en);
	val = TAA_SET_V(val, TAA_F_PATH_OTF_OUT_ZSL_OUT_DISABLE_POST_STALL, !config->zsl_en);
	TAA_SET_R(base, TAA_R_PATH_OTF_OUT_ZSL_OUT_CONFIG, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_PATH_OTF_OUT_STRP_OUT_EN, config->strp_en);
	val = TAA_SET_V(val, TAA_F_PATH_OTF_OUT_STRP_OUT_DISABLE_POST_STALL, !config->strp_en);
	TAA_SET_R(base, TAA_R_PATH_OTF_OUT_STRP_OUT_CONFIG, val);

	return 0;
}

int taa_hw_s_global_enable(void __iomem *base, bool enable, bool fro_en)
{
	u32 val;

	dbg_hw(2, "%s: enable: %d, for_en: %d\n", __func__, enable, fro_en);

	if (enable) {
		if (!fro_en)
			TAA_SET_R_DIRECT(base, TAA_R_GLOBAL_ENABLE, 1);
		else
			TAA_SET_R_DIRECT(base, TAA_R_FRO_GLOBAL_ENABLE, 1);
	} else {
		TAA_SET_R_DIRECT(base, TAA_R_FRO_GLOBAL_ENABLE, 0);

		val = 0;
		val = TAA_SET_V(val, TAA_F_GLOBAL_ENABLE_CLEAR, 1);
		TAA_SET_R_DIRECT(base, TAA_R_GLOBAL_ENABLE, val);

		TAA_SET_R_DIRECT(base, TAA_R_GLOBAL_ENABLE, 0);
	}

	return 0;
}

void taa_hw_enable_int(void __iomem *base)
{
	/*
	 * Output format selector. 1=level, 0=pulse.
	 */
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_LEVEL_PULSE_N_SEL, 3);

	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT1_CLEAR, (u32)((1ULL << INTR1_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_FRO_INT0_CLEAR, (u32)((1ULL << INTR1_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT1_ENABLE, INT1_EN_MASK);
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT2_CLEAR, (u32)((1ULL << INTR2_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_FRO_INT1_CLEAR, (u32)((1ULL << INTR2_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT2_ENABLE, INT2_EN_MASK);
}

void taa_hw_disable_int(void __iomem *base)
{
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT1_CLEAR, (u32)((1ULL << INTR1_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_FRO_INT0_CLEAR, (u32)((1ULL << INTR1_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT1_ENABLE, 0);
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT2_CLEAR, (u32)((1ULL << INTR2_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_FRO_INT1_CLEAR, (u32)((1ULL << INTR2_TAA_MAX) - 1));
	TAA_SET_R_DIRECT(base, TAA_R_CONTINT_INT2_ENABLE, 0);
}

void taa_hw_enable_csis_int(void __iomem *base)
{
	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_INT_SRC, (u32)((1ULL << CSIS_INT_DMA_MAX) - 1));
	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_INT_ENABLE, CSIS_3AA_DMA_IRQ_MASK);
}

void taa_hw_disable_csis_int(void __iomem *base)
{
	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_INT_SRC, (u32)((1ULL << CSIS_INT_DMA_MAX) - 1));
	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_INT_ENABLE, 0);
}

u32 taa_hw_g_int_status(void __iomem *base, u32 irq_id, bool clear, bool fro_en)
{
	enum is_taa_reg_name int_req, int_req_clear;
	enum is_taa_reg_name fro_int_req, fro_int_req_clear;
	u32 int_status, fro_int_status;

	switch (irq_id) {
	case 0:
		int_req = TAA_R_CONTINT_INT1_STATUS;
		int_req_clear = TAA_R_CONTINT_INT1_CLEAR;
		fro_int_req = TAA_R_FRO_INT0;
		fro_int_req_clear = TAA_R_FRO_INT0_CLEAR;
		break;
	case 1:
		int_req = TAA_R_CONTINT_INT2_STATUS;
		int_req_clear = TAA_R_CONTINT_INT2_CLEAR;
		fro_int_req = TAA_R_FRO_INT1;
		fro_int_req_clear = TAA_R_FRO_INT1_CLEAR;
		break;
	default:
		err_hw("[3AA] invalid irq_id (%d)", irq_id);
		return 0;
	}

	int_status = TAA_GET_R_DIRECT(base, int_req);
	fro_int_status = TAA_GET_R_DIRECT(base, fro_int_req);

	if (clear) {
		TAA_SET_R_DIRECT(base, int_req_clear, int_status);
		TAA_SET_R_DIRECT(base, fro_int_req_clear, fro_int_status);
	}

	return fro_en ? fro_int_status : int_status;
}

u32 taa_hw_g_csis_int_status(void __iomem *base, u32 irq_id, bool clear)
{
	enum is_hw_csi_dmax_reg_name int_req, int_req_clear;
	u32 int_status;

	switch (irq_id) {
	case 2:
		int_req = CSIS_DMAX_R_INT_SRC;
		int_req_clear = CSIS_DMAX_R_INT_SRC;
		break;
	case 3:
		int_req = CSIS_DMAX_R_INT_SRC;
		int_req_clear = CSIS_DMAX_R_INT_SRC;
		break;
	default:
		err_hw("[3AA] invalid irq_id (%d)", irq_id);
		return 0;
	}

	int_status = CSIS_DMAX_GET_R(base, int_req);

	if (clear)
		CSIS_DMAX_SET_R(base, int_req_clear, int_status);

	return int_status;
}

void taa_hw_wait_isr_clear(void __iomem *base, void __iomem *zsl_base,
		void __iomem *strp_base, bool fro_en)
{
	u32 status;
	u32 try_cnt;

	try_cnt = 0;
	while (INT1_EN_MASK & (status = taa_hw_g_int_status(base, 0, true, fro_en))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[3AA] Failed to wait int1 clear. 0x%x", status);
			break;
		}

		dbg_hw(2, "[3AA]%s: int1 try_cnt %d\n", __func__, try_cnt);
	}

	try_cnt = 0;
	while (INT2_EN_MASK & (status = taa_hw_g_int_status(base, 1, true, fro_en))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[3AA] Failed to wait int2 clear. 0x%x", status);
			break;
		}

		dbg_hw(2, "[3AA]%s: int2 try_cnt %d\n", __func__, try_cnt);
	}

	try_cnt = 0;
	while (CSIS_3AA_DMA_IRQ_MASK & (status = taa_hw_g_csis_int_status(zsl_base, 2, true))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[3AA] Failed to wait zsl clear. 0x%x", status);
			break;
		}

		dbg_hw(2, "[3AA]%s: zsl try_cnt %d\n", __func__, try_cnt);
	}

	try_cnt = 0;
	while (CSIS_3AA_DMA_IRQ_MASK & (status = taa_hw_g_csis_int_status(strp_base, 3, true))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[3AA] Failed to wait strp clear. 0x%x", status);
			break;
		}

		dbg_hw(2, "[3AA]%s: strp try_cnt %d\n", __func__, try_cnt);
	}
}

u32 taa_hw_occurred(u32 status, u32 irq_id, enum taa_event_type type)
{
	switch (irq_id) {
	case 0:
		return __taa_hw_occurred_int1(status, type);
	case 1:
		return __taa_hw_occurred_int2(status, type);
	case 2:
	case 3:
		return __taa_hw_occurred_int3_4(status, type);
	default:
		err_hw("[3AA] invalid irq_id (%d)", irq_id);
		return 0;
	}
}

void taa_hw_print_error(u32 irq_id, ulong err_state)
{
	ulong err_bit;

	switch (irq_id) {
	case 0:
		while (err_state) {
			err_bit = find_first_bit(&err_state, INTR1_TAA_MAX);
			if (err_bit < INTR1_TAA_MAX) {
				err_hw("[3AA] ERR INT1(%d):%s", err_bit, taa_int1_str[err_bit]);
				err_state &= ~(0x1 << err_bit);
			}
		}
		break;
	case 1:
		while (err_state) {
			err_bit = find_first_bit(&err_state, INTR2_TAA_MAX);
			if (err_bit < INTR2_TAA_MAX) {
				err_hw("[3AA] ERR INT2(%d):%s", err_bit, taa_int2_str[err_bit]);
				err_state &= ~(0x1 << err_bit);
			}
		}
		break;
	default:
		break;
	}
}

static __always_inline void __taa_hw_wait_corex_idle(void __iomem *base)
{
	u32 try_cnt = 0;

	while (TAA_GET_F_DIRECT(base, TAA_R_COREX_STATUS_0, TAA_F_COREX_BUSY_0)) {
		udelay(1);

		if (++try_cnt >= TAA_TRY_COUNT) {
			err_hw("[3AA] Failed to wait COREX idle");
			break;
		}

		dbg_hw(3, "[3AA]%s: try_cnt %d\n", __func__, try_cnt);
	}
}


int taa_hw_s_corex_init(void __iomem *base)
{
	u32 i;

	TAA_SET_R_DIRECT(base, TAA_R_COREX_ENABLE, 1);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_0, COREX_IGNORE);
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_MODE_0, HW_TRIGGER);
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_1, COREX_IGNORE);
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_MODE_1, HW_TRIGGER);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_TYPE_WRITE_TRIGGER, 1);

	/* Loop per 32 regs: Set type0 for every registers */
	for (i = 0; i < DIV_ROUND_UP(taa_regs[TAA_REG_CNT - 1].sfr_offset, (4 * 32)); i++)
		TAA_SET_R_DIRECT(base, TAA_R_COREX_TYPE_WRITE, 0);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_COPY_FROM_IP_0, 1);
	__taa_hw_wait_corex_idle(base);

	return 0;
}

int taa_hw_enable_corex_trigger(void __iomem *base)
{
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_0, COREX_COPY);

	return 0;
}

int taa_hw_disable_corex_trigger(void __iomem *base)
{
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_0, COREX_IGNORE);

	return 0;
}

int taa_hw_enable_corex(void __iomem *base)
{
	dbg_hw(2, "%s is called\n", __func__);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_0, COREX_COPY);
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_MODE_0, SW_TRIGGER);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_START_0, 1);
	__taa_hw_wait_corex_idle(base);

	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_TYPE_0, COREX_IGNORE);
	TAA_SET_R_DIRECT(base, TAA_R_COREX_UPDATE_MODE_0, HW_TRIGGER);

	return 0;
}

bool taa_hw_is_corex(u32 cr_offset)
{
	switch (cr_offset) {
	case 0x0C5C: /* TAA_R_CGRAS_LUT_START_ADD_0*/
	case 0x0C60: /* TAA_R_CGRAS_LUT_START_ADD_1 */
	case 0x0C64: /* TAA_R_CGRAS_LUT_ACCESS_0 */
	case 0x0C68: /* TAA_R_CGRAS_LUT_ACCESS_0_SETB */
	case 0x0C6C: /* TAA_R_CGRAS_LUT_ACCESS_1 */
	case 0x0C70: /* TAA_R_CGRAS_LUT_ACCESS_1_SETB */
	case 0x3E88: /* TAA_R_RGBYHIST_GRID_TBL_START_ADD */
	case 0x3E8C: /* TAA_R_RGBYHIST_GRID_TBL_ACCESS */
	case 0x3E90: /* TAA_R_RGBYHIST_GRID_TBL_ACCESS_SET_B */
		return false;
	default:
		return true;
	}
}

int taa_hw_s_cinfifo_init(void __iomem *base)
{
	u32 val;

	val = 0;
	/*
	 * 0x0 - t2 interval will last at least until frame_start signal will arrive
	 * 0x1 - t2 interval wait for frame_start is disabled
	 */
	val = TAA_SET_V(val, TAA_F_CINFIFO_INPUT_T2_DISABLE_WAIT_FOR_FS, 0);
	/*
	 * 0x0 - t2 counter will not reset at frame_start
	 * 0x1 - if  cinfifo_t2_disable_wait_for_fs=0 t2 counter resets at frame_start
	 */
	val = TAA_SET_V(val, TAA_F_CINFIFO_INPUT_T2_RESET_COUNTER_AT_FS, 0);
	TAA_SET_R_DIRECT(base, TAA_R_CINFIFO_INPUT_T2_WAIT_FOR_FS, val);

	TAA_SET_R(base, TAA_R_CINFIFO_INPUT_T3_INTERVAL, TAA_MIN_HBLNK);

	return 0;
}

int taa_hw_s_cinfifo_config(void __iomem *base, struct taa_cinfifo_config *config)
{
	u32 val;

	if (config->enable) {
		TAA_SET_R(base, TAA_R_CINFIFO_INPUT_CINFIFO_ENABLE, 1);

		val = 0;
		val = TAA_SET_V(val, TAA_F_CINFIFO_INPUT_IMAGE_WIDTH, config->width);
		val = TAA_SET_V(val, TAA_F_CINFIFO_INPUT_IMAGE_HEIGHT,
				config->height + TAA_DRAIN_TOT_LINE);
		TAA_SET_R(base, TAA_R_CINFIFO_INPUT_IMAGE_DIMENSIONS, val);

		TAA_SET_R(base, TAA_R_CINFIFO_INPUT_BIT14_MODE_EN, config->bit14_mode);
	} else
		TAA_SET_R(base, TAA_R_CINFIFO_INPUT_CINFIFO_ENABLE, 0);

	return 0;
}

int taa_hw_g_buf_info(enum taa_internal_buf_id buf_id, struct taa_buf_info *info)
{
	u32 width, height, bpp, num;

	switch (buf_id) {
	case TAA_INTERNAL_BUF_CDAF:
		/* (181 stat) * (4 bytes) */
		width = TAA_CDAF_STAT_LENGTH;
		height = 1;
		bpp = 32;
		num = 1;
		break;
	case TAA_INTERNAL_BUF_THUMB_PRE:
	case TAA_INTERNAL_BUF_THUMB:
		/* (64 x 64) * (10 stat) * (2 bytes) */
		width = 81920;
		height = 1;
		bpp = 8;
		num = 2;
		break;
	case TAA_INTERNAL_BUF_RGBYHIST:
		/* (9 channel) * (256 bins) (4 bytes) */
		width = 9216;
		height = 1;
		bpp = 8;
		num = 2;
		break;
	default:
		return -EINVAL;
	}

	info->w = width;
	info->h = height;
	info->bpp = bpp;
	info->num = num;

	return 0;
}

int taa_hw_g_stat_param(enum taa_dma_id dma_id, u32 w, u32 h,
	struct param_dma_output *dma_output)
{
	if (!w || !h) {
		err_hw("[3AA][DI%d]%s: invalid size (%dx%d)\n", dma_id, __func__, w, h);
		return -EINVAL;
	}

	switch (dma_id) {
	case TAA_WDMA_THSTAT_PRE:
	case TAA_WDMA_THSTAT:
		/* grid_w * 10 stats * 2bytes */
		dma_output->width = w * 10 * 2;
		dma_output->height = h;
		break;
	case TAA_WDMA_RGBYHIST:
		/* w channels * h bins * 4 bytes */
		dma_output->width = w * h * 4;
		dma_output->height = 1;
		break;
	case TAA_WDMA_DRC:
		/* w grids * 4 bytes */
		dma_output->width = w * 4;
		dma_output->height = h;
		break;
	case TAA_WDMA_ORBDS0:
	case TAA_WDMA_ORBDS1:
		dma_output->width = w;
		dma_output->height = h;
		break;
	default:
		break;
	}

	dma_output->format = DMA_INOUT_FORMAT_Y;
	dma_output->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	dma_output->msb = 7;
	dma_output->plane = DMA_INOUT_PLANE_1;

	return 0;
}

int taa_hw_create_dma(void __iomem *base, enum taa_dma_id dma_id, struct is_common_dma *dma)
{
	int ret;
	void __iomem *base_reg;
	ulong available_bayer_format_map;
	ulong available_yuv_format_map;
	char name[64];

	switch (dma_id) {
	case TAA_WDMA_THSTAT_PRE:
		base_reg = base + taa_regs[TAA_R_WDMA_THSTATPRE_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_THSTAT_PRE", sizeof(name) - 1);
		break;
	case TAA_WDMA_RGBYHIST:
		base_reg = base + taa_regs[TAA_R_WDMA_RGBHIST_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_RGBYHIST", sizeof(name) - 1);
		break;
	case TAA_WDMA_THSTAT:
		base_reg = base + taa_regs[TAA_R_WDMA_THSTAT_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_THSTAT", sizeof(name) - 1);
		break;
	case TAA_WDMA_DRC:
		base_reg = base + taa_regs[TAA_R_WDMA_DRC_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_DRC", sizeof(name) - 1);
		break;
	case TAA_WDMA_FDPIG:
		base_reg = base + taa_regs[TAA_R_WDMA_FDPIG_EN].sfr_offset;
		available_bayer_format_map = 0;
		available_yuv_format_map =	(0
						| BIT_MASK(DMA_FMT_YUV422_2P_UFIRST)
						| BIT_MASK(DMA_FMT_YUV444_3P)
						) & IS_YUV_FORMAT_MASK;
		strncpy(name, "TAA_WDMA_FDPIG", sizeof(name) - 1);
		break;
	case TAA_WDMA_ORBDS0:
		base_reg = base + taa_regs[TAA_R_WDMA_DS0_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_ORBDS0", sizeof(name) - 1);
		break;
	case TAA_WDMA_ORBDS1:
		base_reg = base + taa_regs[TAA_R_WDMA_DS1_EN].sfr_offset;
		available_bayer_format_map = BIT_MASK(DMA_FMT_U8BIT_PACK) & IS_BAYER_FORMAT_MASK;
		available_yuv_format_map = 0;
		strncpy(name, "TAA_WDMA_ORBDS1", sizeof(name) - 1);
		break;
	case TAA_WDMA_STRP:
		/* for only name */
		base_reg = NULL;
		available_bayer_format_map = 0;
		available_yuv_format_map = 0;
		strncpy(name, "CSIS_WDMA_STRP", sizeof(name) - 1);
		break;
	case TAA_WDMA_ZSL:
		/* for only name */
		base_reg = NULL;
		available_bayer_format_map = 0;
		available_yuv_format_map = 0;
		strncpy(name, "CSIS_WDMA_ZSL", sizeof(name) - 1);
		break;
	default:
		err_hw("[3AA] invalid dma_id (%d)", dma_id);
		return -EINVAL;
	}

	ret = is_hw_dma_set_ops(dma);
	ret |= is_hw_dma_create(dma, base_reg, dma_id, name,
		available_bayer_format_map, available_yuv_format_map, 0, 0);

	return ret;
}

int __taa_hw_s_stat_wdma(struct is_common_dma *dma, struct param_dma_output *dma_output,
	u32 *addr, u32 num_buffers, u32 en_fro)
{
	int ret;
	u32 stride_1p;
	u32 format, hwformat, memory_bitwidth, pixelsize;
	u32 width, height;
	u32 i;
	dma_addr_t address[IS_MAX_FRO];

	width = dma_output->width;
	height = dma_output->height;
	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	ret = CALL_DMA_OPS(dma, dma_set_corex_id, TAA_COREX_SET);

	ret |= CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);
	if (dma_output->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return 0;

	format = is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, 0, true);
	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);

	/* orbmch can't support stride */
	if ((dma->id == TAA_WDMA_ORBDS0) || (dma->id == TAA_WDMA_ORBDS1))
		stride_1p = width;
	else
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
						     width, 16, true);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);

	if (dma->id == TAA_WDMA_RGBYHIST)
		ret |= CALL_DMA_OPS(dma, dma_set_auto_flush_en, false);
	else
		ret |= CALL_DMA_OPS(dma, dma_set_auto_flush_en, true);

	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + (i * en_fro));
	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, (dma_addr_t *)address, 0, 0, num_buffers);

	return 0;
}

int __taa_hw_s_fdpig_wdma(struct is_common_dma *dma, struct param_dma_output *dma_output,
	u32 *addr, u32 num_buffers)
{
	int ret;
	u32 stride, stride_1p, stride_2p, stride_3p;
	u32 format, hwformat, memory_bitwidth, pixelsize, plane, order;
	u32 width, height;
	u32 i;
	dma_addr_t address[IS_MAX_FRO];

	width = dma_output->width;
	height = dma_output->height;
	hwformat = dma_output->format;
	memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	pixelsize = 8;
	plane = dma_output->plane;
	order = dma_output->order;

	ret = CALL_DMA_OPS(dma, dma_set_corex_id, TAA_COREX_SET);

	ret |= CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);

	if (dma_output->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return 0;

	format = is_hw_dma_get_yuv_format(memory_bitwidth, hwformat, plane, order);
	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_YUV);

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);

	stride = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat, width, 16, true);
	if (dma_output->stride_plane0)
		stride_1p = dma_output->stride_plane0;
	else
		stride_1p = stride;
	if (dma_output->stride_plane1)
		stride_2p = dma_output->stride_plane1;
	else
		stride_2p = stride;
	if (dma_output->stride_plane2)
		stride_3p = dma_output->stride_plane2;
	else
		stride_3p = stride;
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, stride_2p, stride_3p);

	ret |= CALL_DMA_OPS(dma, dma_set_auto_flush_en, true);

	/* only BGR order */
	/* R buffer*/
	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + 2);
	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, address, 0, 0, num_buffers);
	/* G buffer */
	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + 1);
	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, address, 1, 0, num_buffers);
	/* B buffer */
	for (i = 0; i < num_buffers; i++)
		address[i] = (dma_addr_t)*(addr + 0);
	ret |= CALL_DMA_OPS(dma, dma_set_img_addr, address, 2, 0, num_buffers);

	if (ret)
		err_hw("%s fail (%d)", __func__, ret);

	return 0;
}

u32 __taa_hw_g_csis_dma_format(u32 bitwidth, u32 msb)
{
	switch (bitwidth) {
	case DMA_INOUT_BIT_WIDTH_16BIT:
		if (msb == 12)
			return CSIS_DMA_FMT_S12BIT_UNPACK_MSB_ZERO;
		else if (msb == 11)
			return CSIS_DMA_FMT_U12BIT_UNPACK_MSB_ZERO;
		else if (msb == 10)
			return CSIS_DMA_FMT_S10BIT_UNPACK_MSB_ZERO;
		else if (msb == 9)
			return CSIS_DMA_FMT_U10BIT_UNPACK_MSB_ZERO;
		else if (msb == 8)
			return CSIS_DMA_FMT_S8BIT_UNPACK_MSB_ZERO;
		else if (msb == 7)
			return CSIS_DMA_FMT_U8BIT_UNPACK_MSB_ZERO;
		err_hw("[3AA] invalid msb (%d)", msb);
		return CSIS_DMA_FMT_U8BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_15BIT:
		return CSIS_DMA_FMT_S14BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_14BIT:
		return CSIS_DMA_FMT_U14BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_13BIT:
		return CSIS_DMA_FMT_S12BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_12BIT:
		return CSIS_DMA_FMT_U12BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_11BIT:
		return CSIS_DMA_FMT_S10BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_10BIT:
		return CSIS_DMA_FMT_U10BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_9BIT:
		return CSIS_DMA_FMT_S8BIT_PACK;
	case DMA_INOUT_BIT_WIDTH_8BIT:
		return CSIS_DMA_FMT_U8BIT_PACK;
	default:
		err_hw("[dns] invalid bitwidth (%d)", bitwidth);
		return CSIS_DMA_FMT_U8BIT_PACK;
	}
}

int __taa_hw_s_csis_wdma(void __iomem *base, struct param_dma_output *dma_output,
	u32 *addr, u32 num_buffers)
{
	u32 val;
	u32 i;
	u32 stride_1p;
	u32 format, hwformat, memory_bitwidth, pixelsize;
	u32 width, height;
	u32 fro_en = (num_buffers > 1) ? 1 : 0;
	u32 addr_set = 0;
	u32 frameseq = 1;
	u32 act_framecnt_seq;

	if (dma_output->cmd == DMA_OUTPUT_COMMAND_DISABLE) {
		CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_CTRL, 0);
		return 0;
	}

	width = dma_output->width;
	height = dma_output->height;
	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;
	stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat, width, 16, true);
	format = __taa_hw_g_csis_dma_format(memory_bitwidth, dma_output->msb);

	val = 0;
	val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_DATAFORMAT, format);
	CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_FMT, val);

	val = 0;
	val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_HRESOL, width);
	val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_VRESOL, height);
	CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_RESOL, val);

	CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_STRIDE, stride_1p);

	val = 0;
	if (fro_en) {
		act_framecnt_seq = CSIS_DMAX_CHX_GET_R(base, CSIS_DMAX_CHX_R_ACT_FRAMECNT_SEQ);
		/* Change scenario normal mode into FRO mode */
		if (act_framecnt_seq == 1) {
			/* Initiaiize frame pointer at first frame */
			val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_UPDT_FRAMEPTR, 0);
			val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_UPDT_PTR_EN, 1);
		} else if (act_framecnt_seq == (1 << num_buffers) - 1) {
			/* Use toggling address set[1...num_buffers] and [17...17+num_buffers] */
			addr_set = 16;
		} else {
			addr_set = 0;
		}
	}

	frameseq = ((1 << (addr_set + num_buffers)) - 1) & ~((1 << (addr_set)) - 1);
	CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_FCNTSEQ, frameseq);
	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_FRO_INT_FRAME_NUM, (num_buffers - 1));

	for (i = 0; i < num_buffers; i++)
		CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_ADDR1 + addr_set + i, *(addr + i));

	val = CSIS_DMAX_CHX_SET_V(val, CSIS_DMAX_CHX_F_DMA_ENABLE, 1);
	CSIS_DMAX_CHX_SET_R(base, CSIS_DMAX_CHX_R_CTRL, val);

	return 0;
}

int taa_hw_s_wdma(struct is_common_dma *dma, void __iomem *base,
	struct param_dma_output *dma_output, u32 *addr, u32 num_buffers)
{
	int ret;

	switch (dma->id) {
	case TAA_WDMA_THSTAT_PRE:
	case TAA_WDMA_THSTAT:
	case TAA_WDMA_ORBDS0:
	case TAA_WDMA_ORBDS1:
	case TAA_WDMA_RGBYHIST:
		ret = __taa_hw_s_stat_wdma(dma, dma_output, addr, num_buffers, 0);
		break;
	case TAA_WDMA_DRC:
		ret = __taa_hw_s_stat_wdma(dma, dma_output, addr, num_buffers, 1);
		break;
	case TAA_WDMA_FDPIG:
		ret = __taa_hw_s_fdpig_wdma(dma, dma_output, addr, num_buffers);
		break;
	case TAA_WDMA_ZSL:
	case TAA_WDMA_STRP:
		ret = __taa_hw_s_csis_wdma(base, dma_output, addr, num_buffers);
		break;
	default:
		err_hw("[dns] invalid dma_id (%d)", dma->id);
		return -EINVAL;
	}

	return ret;
}

int __taa_hw_abort_csis_wdma(void __iomem *base)
{
	u32 try_cnt = 0;

	CSIS_DMAX_SET_R(base, CSIS_DMAX_R_CMN_CTRL, 1);

	while (CSIS_DMAX_GET_R(base, CSIS_DMAX_R_CMN_CTRL)) {
		if (++try_cnt >= TAA_TRY_COUNT) {
			err_hw("[3AA] Failed to dma abort\n");
			return -EBUSY;
		}
	}

	return 0;
}

int taa_hw_abort_wdma(struct is_common_dma *dma, void __iomem *base)
{
	int ret;

	switch (dma->id) {
	case TAA_WDMA_THSTAT_PRE:
	case TAA_WDMA_THSTAT:
	case TAA_WDMA_DRC:
	case TAA_WDMA_ORBDS0:
	case TAA_WDMA_ORBDS1:
	case TAA_WDMA_RGBYHIST:
	case TAA_WDMA_FDPIG:
		ret = 0;
		break;
	case TAA_WDMA_ZSL:
	case TAA_WDMA_STRP:
		ret = __taa_hw_abort_csis_wdma(base);
		break;
	default:
		err_hw("[3AA] invalid dma_id (%d)", dma->id);
		return -EINVAL;
	}

	return ret;
}

int __taa_hw_s_dtp_size(void __iomem *base, struct taa_size_config *config)
{
	/* pattern size register can control color bar size*/
	TAA_SET_R(base, TAA_R_WDRDTP_PATTERN_SIZE_X, config->sensor_crop.w / 4);
	TAA_SET_R(base, TAA_R_WDRDTP_PATTERN_SIZE_Y, config->sensor_crop.h / 4);

	return 0;
}

int taa_hw_enable_dtp(void __iomem *base, bool enable)
{
	TAA_SET_R(base, TAA_R_WDRDTP_BYPASS, !enable);

	if (!enable)
		return 0;

	/* color bar */
	TAA_SET_R(base, TAA_R_WDRDTP_MODE, 8);
	TAA_SET_R(base, TAA_R_WDRDTP_LAYER_WEIGHTS_1, 0x20);
	return 0;
}

int taa_hw_bypass_drc(void __iomem *base)
{
	u32 val, rta_val;

	val = 0;
	val = TAA_SET_V(val, TAA_F_DRCCLCT_BYPASS, 1);
	val = TAA_SET_V(val, TAA_F_DRCCLCT_GRIDLAST_NEW_ON, 0);

	rta_val = TAA_GET_R(base, TAA_R_DRCCLCT_BYPASS);
	TAA_SET_R(base, TAA_R_DRCCLCT_BYPASS, val);

	if (rta_val != val)
		return 1;
	else
		return 0;
}

int taa_hw_s_module_bypass(void __iomem *base)
{
	TAA_SET_R(base, TAA_R_AFIDENT_0_BYPASS, 1);

	TAA_SET_R(base, TAA_R_BITMASK_BYPASS, 1);
	TAA_SET_R(base, TAA_R_BITMASK_MASK, 0x3FF0);

	TAA_SET_R(base, TAA_R_BLC_BYPASS, 1);

	TAA_SET_R(base, TAA_R_CGRAS_BYPASS_REG, 1);

	TAA_SET_R(base, TAA_R_DSPCLTOP_BYPASS, 1);

	TAA_SET_R(base, TAA_R_RECONST_BYPASS, 1);

	TAA_SET_R(base, TAA_R_WBG_RWC_14BIT_BYPASS, 1);

	TAA_SET_R(base, TAA_R_GAMMA_15BIT_BAYER_3_RWC_BYPASS, 1);

	TAA_SET_R(base, TAA_R_THSTATPRE_BYPASS, 1);

	TAA_SET_R(base, TAA_R_THSTAT_BYPASS, 1);

	TAA_SET_R(base, TAA_R_RGBYHIST_BYPASS, 1);

	TAA_SET_R(base, TAA_R_CDAF_BYPASS, 1);

	TAA_SET_R(base, TAA_R_DRCCLCT_BYPASS, 1);

	TAA_SET_R(base, TAA_R_FDPIG_POST_GAMMA_BYPASS, 1);
	TAA_SET_R(base, TAA_R_FDPIG_CCM9_BYPASS, 1);
	TAA_SET_R(base, TAA_R_FDPIG_RGB_GAMMA_BYPASS, 1);

	TAA_SET_R(base, TAA_R_ORBDS_PRE_GAMMA_BYPASS, 1);
	TAA_SET_R(base, TAA_R_ORB_DS_BYPASS, 1);
	TAA_SET_R(base, TAA_R_ORB_DS_OUTPUT_EN, 0);
	TAA_SET_R(base, TAA_R_ME_NR0_BYPASS, 1);

	TAA_SET_R(base, TAA_R_BLC_ZSL_BYPASS, 1);
	TAA_SET_R(base, TAA_R_BLC_STRP_BYPASS, 1);
	TAA_SET_R(base, TAA_R_BLC_DNS_BYPASS, 1);

	return 0;
}

int taa_hw_g_cdaf_stat(void __iomem *base, u32 *buf, u32 *size)
{
	u32 start_add, count, i;

	count = 0;
	while (1) {
		TAA_SET_R_DIRECT(base, TAA_R_CDAF_STAT_START_ADD, 0);
		start_add = TAA_GET_R_DIRECT(base, TAA_R_CDAF_STAT_START_ADD);
		count++;

		if (!start_add)
			break;

		if (count > 10) {
			err_hw("%s: writing cdaf_start_add register failed\n", __func__);
			return -EIO;
		}
	}

	for (i = 0; i < TAA_CDAF_STAT_LENGTH; i++)
		buf[i] = TAA_GET_R_DIRECT(base, TAA_R_CDAF_STAT_ACCESS);
	*size = TAA_CDAF_STAT_LENGTH * sizeof(u32);

	return 0;
}

int taa_hw_s_module_init(void __iomem *base)
{
	TAA_SET_R(base, TAA_R_DSPCLTOP_LBCTRL_HBI, TAA_MIN_HBLNK);
	TAA_SET_R(base, TAA_R_RECONST_H_BLANK_CYCLE, TAA_MIN_HBLNK);
	return 0;
}

static u32 __taa_hw_g_inv_shift(u32 scale)
{
	u32 shift_num;

	/* 12 fractional bit calculation */
	if (scale == (1 << 12)) /* x1.0 */
		shift_num = 26;
	else if (scale <= (2 << 12)) /* x2.0 */
		shift_num = 27;
	else if (scale <= (4 << 12)) /* x4.0 */
		shift_num = 28;
	else if (scale <= (8 << 12)) /* x8.0 */
		shift_num = 29;
	else if (scale <= (16 << 12)) /* x16.0 */
		shift_num = 30;
	else
		shift_num = 31;

	return shift_num;
}

static void __taa_hw_s_bds_size(void __iomem *base, struct taa_size_config *config)
{
	u32 val;
	bool ds_en;
	u32 bypass, op_mode;
	u32 scale_y, inv_shift_y, int_scale_y;
	u32 ratio_h;
	const u32 scale_unity = 4096;
	const u64 ratio_unity = 1048576;

	ds_en = (config->bcrop1.w != config->bds.w) ||
		(config->bcrop1.h != config->bds.h);

	/* op_mode - 0 : BNM+BDS, 2 : BNM only */
	if (ds_en || config->fdpig_en)
		op_mode = 0;
	else
		op_mode = 2;

	bypass = 0;
	scale_y = config->bcrop1.h * scale_unity / config->bds.h;
	inv_shift_y = __taa_hw_g_inv_shift(scale_y);
	int_scale_y = (1 << inv_shift_y) / scale_y;

	TAA_SET_F(base, TAA_R_BDS_CONTROL, TAA_F_BDS_BYPASS, bypass);
	TAA_SET_F(base, TAA_R_BDS_CONTROL, TAA_F_BDS_OPERATIONAL_MODE, op_mode);
	TAA_SET_F(base, TAA_R_BDS_CONTROL, TAA_F_BDS_LINE_GAP, TAA_MIN_HBLNK);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_OUTPUT_W, config->bds.w);
	val = TAA_SET_V(val, TAA_F_BDS_OUTPUT_H, config->bds.h);
	TAA_SET_R(base, TAA_R_BDS_OUT_SIZE, val);

	TAA_SET_R(base, TAA_R_BDS_Y_SCALE, scale_y);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_INV_SCALE_Y, int_scale_y);
	val = TAA_SET_V(val, TAA_F_BDS_INV_SHIFT_Y, inv_shift_y);
	TAA_SET_R(base, TAA_R_BDS_INV_Y, val);

	TAA_SET_R(base, TAA_R_BDS_BPLH_ENABLE, !bypass);
	TAA_SET_R(base, TAA_R_BDS_BPLH_IN_SIZE_H, config->bcrop1.w);
	TAA_SET_R(base, TAA_R_BDS_BPLH_IN_SIZE_V, config->bcrop1.h);
	TAA_SET_R(base, TAA_R_BDS_BPLH_DST_SIZE_H, config->bds.w);

	ratio_h = (u32)(config->bcrop1.w * ratio_unity / config->bds.w);
	TAA_SET_R(base, TAA_R_BDS_BPLH_RATIO_H, ratio_h);
}

static void __taa_hw_s_fdpig_size(void __iomem *base, struct taa_size_config *config)
{
	u32 val;
	u32 scale_x, scale_y;
	u32 inv_shift_x, inv_shift_y;
	u32 inv_scale_x, inv_scale_y;
	const u32 scale_unity = 4096;

	TAA_SET_R(base, TAA_R_BDS_RGB_OUTPUT_EN, config->fdpig_en);
	TAA_SET_R(base, TAA_R_BDS_RGB_CROP_EN, config->fdpig_crop_en);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_RGB_OUTPUT_W, config->fdpig.w);
	val = TAA_SET_V(val, TAA_F_BDS_RGB_OUTPUT_H, config->fdpig.h);
	TAA_SET_R(base, TAA_R_BDS_RGB_OUTPUT, val);

	scale_x = config->fdpig_crop.w * scale_unity / config->fdpig.w;
	inv_shift_x = __taa_hw_g_inv_shift(scale_x);
	inv_scale_x = (1 << inv_shift_x) / scale_x;

	scale_y = config->fdpig_crop.h * scale_unity / config->fdpig.h;
	inv_shift_y = __taa_hw_g_inv_shift(scale_y);
	inv_scale_y = (1 << inv_shift_y) / scale_y;

	TAA_SET_R(base, TAA_R_BDS_RGB_SCALE_X, scale_x);
	TAA_SET_R(base, TAA_R_BDS_RGB_SCALE_Y, scale_y);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_RGB_INV_SCALE_X, inv_scale_x);
	val = TAA_SET_V(val, TAA_F_BDS_RGB_INV_SHIFT_X, inv_shift_x);
	TAA_SET_R(base, TAA_R_BDS_RGB_INV_X, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_RGB_INV_SCALE_Y, inv_scale_y);
	val = TAA_SET_V(val, TAA_F_BDS_RGB_INV_SHIFT_Y, inv_shift_y);
	TAA_SET_R(base, TAA_R_BDS_RGB_INV_Y, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_RGB_CROP_START_X, config->fdpig_crop.x);
	val = TAA_SET_V(val, TAA_F_BDS_RGB_CROP_START_Y, config->fdpig_crop.y);
	TAA_SET_R(base, TAA_R_BDS_RGB_CROP_START, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_BDS_RGB_CROP_SIZE_X, config->fdpig_crop.w);
	val = TAA_SET_V(val, TAA_F_BDS_RGB_CROP_SIZE_Y, config->fdpig_crop.h);
	TAA_SET_R(base, TAA_R_BDS_RGB_CROP_SIZE, val);

}

static void __taa_hw_s_cgras_size(void __iomem *base, struct taa_size_config *config)
{
	u32 val;
	u32 scaling_x, scaling_y;
	u32 step_x, step_y;
	u32 crop_x, crop_y, logic_crop_x, logic_crop_y;
	int center_x, center_y;
	const u32 binning_factor_unity = 1024;
	const u32 step_frac_bits = 10;
	const u32 logic_width = 32 << step_frac_bits;
	const u32 logic_height = 24 << step_frac_bits;

	scaling_x = config->sensor_binning_x * binning_factor_unity / 1000;
	scaling_y = config->sensor_binning_y * binning_factor_unity / 1000;
	step_x = config->sensor_calibrated.w;
	step_x = ((logic_width << step_frac_bits) + (step_x / 2)) /
		step_x;
	step_y = config->sensor_calibrated.h;
	step_y = ((logic_height << step_frac_bits) + (step_y / 2)) /
		step_y;
	crop_x = config->sensor_crop.x * config->sensor_binning_x / 1000;
	logic_crop_x = crop_x * step_x;
	center_x = (-1) * (config->sensor_center_x - crop_x);
	crop_y = config->sensor_crop.y * config->sensor_binning_y / 1000;
	logic_crop_y = crop_y * step_y;
	center_y = (-1) * (config->sensor_center_y - crop_y);

	val = 0;
	val = TAA_SET_V(val, TAA_F_CGRAS_BINNING_X, scaling_x);
	val = TAA_SET_V(val, TAA_F_CGRAS_BINNING_Y, scaling_y);
	TAA_SET_R(base, TAA_R_CGRAS_BINNING_X, val);

	TAA_SET_R(base, TAA_R_CGRAS_STEP_X, step_x);
	TAA_SET_R(base, TAA_R_CGRAS_STEP_Y, step_y);

	TAA_SET_R(base, TAA_R_CGRAS_CROP_START_X, logic_crop_x);
	TAA_SET_R(base, TAA_R_CGRAS_CROP_START_Y, logic_crop_y);

	val = 0;
	val = TAA_SET_V(val, TAA_F_CGRAS_CROP_RADIAL_X, (u32)center_x);
	val = TAA_SET_V(val, TAA_F_CGRAS_CROP_RADIAL_Y, (u32)center_y);
	TAA_SET_R(base, TAA_R_CGRAS_CROP_START_REAL, val);
}

static void __taa_hw_s_dspcl_size(void __iomem *base, struct taa_size_config *config)
{
	u32 val;

	TAA_SET_F(base, TAA_R_DSPCLTOP_RADIAL_COMPENSATION, TAA_F_DSPCLTOP_SENSOR_WIDTH,
		config->sensor_calibrated.w);

	TAA_SET_F(base, TAA_R_DSPCLTOP_RADIAL_COMPENSATION_1, TAA_F_DSPCLTOP_SENSOR_HEIGHT,
		config->sensor_calibrated.h);

	TAA_SET_F(base, TAA_R_DSPCLTOP_MEMORY_SPECKLES_CORR, TAA_F_DSPCLTOP_START_X, 0);

	val = 0;
	val = TAA_SET_V(val, TAA_F_DSPCLTOP_START_Y, 0);
	val = TAA_SET_V(val, TAA_F_DSPCLTOP_CROP_WIDTH, config->sensor_crop.w);
	TAA_SET_R(base, TAA_R_DSPCLTOP_MEMORY_SPECKLES_CORR_1, val);

	val = 0;
	val = TAA_SET_V(val, TAA_F_DSPCLTOP_CROP_HEIGHT, config->sensor_crop.h);
	val = TAA_SET_V(val, TAA_F_DSPCLTOP_BPR_MIRROR_X_EN, 0);
	val = TAA_SET_V(val, TAA_F_DSPCLTOP_BPR_MIRROR_Y_EN, 0);
	TAA_SET_R(base, TAA_R_DSPCLTOP_MEMORY_SPECKLES_CORR_2, val);
}

static void __taa_hw_s_recon_size(void __iomem *base, struct taa_size_config *config)
{
	u32 val;
	u32 binning_x, binning_y;

	TAA_SET_R(base, TAA_R_RECONST_SENSOR_WIDTH, config->sensor_calibrated.w);
	TAA_SET_R(base, TAA_R_RECONST_SENSOR_HEIGHT, config->sensor_calibrated.h);

	TAA_SET_R(base, TAA_R_RECONST_CROP_X, config->sensor_crop.x);
	TAA_SET_R(base, TAA_R_RECONST_CROP_Y, config->sensor_crop.y);

	if (config->sensor_binning_x < 2000)
		binning_x = 0;
	else if (config->sensor_binning_x < 4000)
		binning_x = 1;
	else if (config->sensor_binning_x < 8000)
		binning_x = 2;
	else
		binning_x = 3;

	if (config->sensor_binning_y < 2000)
		binning_y = 0;
	else if (config->sensor_binning_y < 4000)
		binning_y = 1;
	else if (config->sensor_binning_y < 8000)
		binning_y = 2;
	else
		binning_y = 3;

	val = 0;
	val = TAA_SET_V(val, TAA_F_RECONST_BINNING_X, binning_x);
	val = TAA_SET_V(val, TAA_F_RECONST_BINNING_Y, binning_y);
	TAA_SET_R(base, TAA_R_RECONST_BINNING, val);
}

static void __taa_hw_s_bcrop_size(void __iomem *base, u32 base_offset,
	struct is_crop *in, struct is_crop *out)
{
	if ((in->w != out->w) || (in->h != out->h))
		TAA_SET_R(base + base_offset, TAA_R_BCROP_0_BYPASS, 0);
	else
		TAA_SET_R(base + base_offset, TAA_R_BCROP_0_BYPASS, 1);
	TAA_SET_R(base + base_offset, TAA_R_BCROP_0_START_X, out->x);
	TAA_SET_R(base + base_offset, TAA_R_BCROP_0_START_Y, out->y);
	TAA_SET_R(base + base_offset, TAA_R_BCROP_0_SIZE_X, out->w);
	TAA_SET_R(base + base_offset, TAA_R_BCROP_0_SIZE_Y, out->h);

}

int taa_hw_s_module_size(void __iomem *base, struct taa_size_config *config)
{
	struct is_crop in_size;
	struct is_crop out_size;
	u32 base_offset;

	__taa_hw_s_dtp_size(base, config);

	in_size = config->sensor_crop;
	out_size = config->bcrop0;
	__taa_hw_s_bcrop_size(base, 0, &in_size, &out_size);

	__taa_hw_s_cgras_size(base, config);
	__taa_hw_s_dspcl_size(base, config);
	__taa_hw_s_recon_size(base, config);

	in_size = config->bcrop0;
	out_size = config->bcrop1;
	base_offset = taa_regs[TAA_R_BCROP_1_BYPASS].sfr_offset -
		taa_regs[TAA_R_BCROP_0_BYPASS].sfr_offset;
	__taa_hw_s_bcrop_size(base, base_offset, &in_size, &out_size);

	__taa_hw_s_bds_size(base, config);

	in_size.w = config->bds.w;
	in_size.h = config->bds.h;
	out_size = config->bcrop2;
	base_offset = taa_regs[TAA_R_BCROP_2_BYPASS].sfr_offset -
		taa_regs[TAA_R_BCROP_0_BYPASS].sfr_offset;
	__taa_hw_s_bcrop_size(base, base_offset, &in_size, &out_size);

	__taa_hw_s_fdpig_size(base, config);

	return 0;
}

int taa_hw_s_module_format(void __iomem *base, struct taa_format_config *config)
{
	u32 val;

	TAA_SET_R(base, TAA_R_WDRDTP_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_CGRAS_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_DSPCLTOP_EXPOSURE_ORDER, config->order);
	TAA_SET_R(base, TAA_R_RECONST_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_THSTAT_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_WBG_RWC_14BIT_PIXEL_ORDER, config->order);
	TAA_SET_F(base, TAA_R_BDS_CONTROL, TAA_F_BDS_PIXEL_ORDER, config->order);
	TAA_SET_F(base, TAA_R_DRCCLCT_V_MODE_CFG0, TAA_F_DRCCLCT_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_THSTATPRE_PIXEL_ORDER, config->order);

	/* FDPIG output : BGR */
	TAA_SET_R(base, TAA_R_FDPIG_RGB2YUV_BYPASS, 1);
	TAA_SET_R(base, TAA_R_FDPIG_YUV444TO422_BYPASS, 1);

	/* ORBDS output : Y */
	val = 0;
	val = TAA_SET_V(val, TAA_F_B2Y_BYPASS, 0);
	val = TAA_SET_V(val, TAA_F_B2Y_PIXEL_ORDER, config->order);
	TAA_SET_R(base, TAA_R_B2Y_PIXEL_ORDER, val);

	return 0;
}

int taa_hw_s_fro(void __iomem *base, u32 num_buffers)
{
	u32 prev_fro_en = TAA_GET_R_DIRECT(base, TAA_R_FRO_MODE_ENABLE);
	u32 fro_en = (num_buffers > 1) ? 1 : 0;
	u32 col_row_frame = num_buffers / 2;

	if (prev_fro_en != fro_en) {
		info_hw("[3AA] FRO: %d -> %d\n", prev_fro_en, fro_en);
		/* fro core reset */
		TAA_SET_R_DIRECT(base, TAA_R_FRO_SW_RESET, 1);
	}

	TAA_SET_R_DIRECT(base, TAA_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW, (num_buffers - 1));
	TAA_SET_R_DIRECT(base, TAA_R_FRO_RUN_FRAME_NUMBER_FOR_GROUP1, col_row_frame);
	TAA_SET_R_DIRECT(base, TAA_R_FRO_MODE_ENABLE, fro_en);

	return 0;
}

void taa_hw_s_fro_reset(void __iomem *base, void __iomem *zsl_base,
		void __iomem *strp_base)
{
	CSIS_DMAX_CHX_SET_R(zsl_base, CSIS_DMAX_CHX_R_FRO_FRM, 0);
	CSIS_DMAX_CHX_SET_R(strp_base, CSIS_DMAX_CHX_R_FRO_FRM, 0);
	TAA_SET_R_DIRECT(base, TAA_R_FRO_SW_RESET, 1);
}

void taa_hw_s_crc(void __iomem *base, u32 seed)
{
	TAA_SET_F_DIRECT(base, TAA_R_BITMASK_STREAM_CRC, TAA_F_BITMASK_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_AFIDENT_0_STREAM_CRC, TAA_F_AFIDENT_0_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_AFIDENT_1_STREAM_CRC, TAA_F_AFIDENT_1_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_ME_NR0_CRC, TAA_F_ME_NR0_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_WDRDTP_STREAM_CRC, TAA_F_WDRDTP_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BCROP_0_STREAM_CRC, TAA_F_BCROP_0_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BLC_STREAM_CRC, TAA_F_BLC_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_CGRAS_STREAM_CRC, TAA_F_CGRAS_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_DSPCLTOP_STREAM_CRC, TAA_F_DSPCLTOP_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_RECONST_STREAM_CRC, TAA_F_RECONST_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_THSTAT_CRC, TAA_F_THSTAT_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_WBG_RWC_14BIT_STREAM_CRC, TAA_F_WBG_RWC_14BIT_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_GAMMA_15BIT_BAYER_3_RWC_STREAM_CRC,
			 TAA_F_GAMMA_15BIT_BAYER_3_RWC_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BCROP_1_STREAM_CRC, TAA_F_BCROP_1_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BDS_STREAM_CRC, TAA_F_BDS_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_DRCCLCT_STREAM_CRC, TAA_F_DRCCLCT_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_ORBDS_PRE_GAMMA_STREAM_CRC,
			 TAA_F_ORBDS_PRE_GAMMA_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_ORB_DS_STREAM_CRC, TAA_F_ORB_DS_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_FDPIG_POST_GAMMA_STREAM_CRC,
			 TAA_F_FDPIG_POST_GAMMA_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_FDPIG_CCM9_STREAM_CRC, TAA_F_FDPIG_CCM9_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_FDPIG_RGB_GAMMA_STREAM_CRC,
			 TAA_F_FDPIG_RGB_GAMMA_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_FDPIG_RGB2YUV_STREAM_CRC, TAA_F_FDPIG_RGB2YUV_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_FDPIG_YUV444TO422_STREAM_CRC,
			 TAA_F_FDPIG_YUV444TO422_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BCROP_2_STREAM_CRC, TAA_F_BCROP_2_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BLC_ZSL_STREAM_CRC, TAA_F_BLC_ZSL_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BLC_STRP_STREAM_CRC, TAA_F_BLC_STRP_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_BLC_DNS_STREAM_CRC, TAA_F_BLC_DNS_CRC_SEED, seed);
	TAA_SET_F_DIRECT(base, TAA_R_THSTATPRE_CRC, TAA_F_THSTATPRE_CRC_SEED, seed);
}

void taa_hw_dump(void __iomem *base)
{
	info_hw("[3AA] SFR DUMP (v9.20)\n");

	is_hw_dump_regs(base, taa_regs, TAA_REG_CNT);
}

void taa_hw_dump_strp(void __iomem *base)
{
	info_hw("[STRP] SFR DUMP\n");
	is_hw_dump_regs(base, csi_dmax_chx_regs, CSIS_DMAX_CHX_REG_CNT);
	is_hw_dump_regs(base + CSIS_DMAX_OFFSET, csi_dmax_regs, CSIS_DMAX_REG_CNT);
}

void taa_hw_dump_zsl(void __iomem *base)
{
	info_hw("[ZSL] SFR DUMP\n");
	is_hw_dump_regs(base, csi_dmax_chx_regs, CSIS_DMAX_CHX_REG_CNT);
	is_hw_dump_regs(base + CSIS_DMAX_OFFSET, csi_dmax_regs, CSIS_DMAX_REG_CNT);
}

u32 taa_hw_g_reg_cnt(void)
{
	return TAA_REG_CNT + TAA_LUT_REG_CNT * TAA_LUT_NUM;
}
