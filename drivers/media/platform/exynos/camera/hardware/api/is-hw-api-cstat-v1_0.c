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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <dt-bindings/camera/exynos_is_dt.h>

#include "is-hw-api-common.h"
#include "is-hw-api-cstat.h"
#include "sfr/is-sfr-cstat-v1_0.h"
#include "is-debug.h"

/* COREX SRAM(shadowed) register read/write */
#define CSTAT_SET_R(base, R, val) \
	is_hw_set_reg((base + CSTAT_COREX_SDW_OFFSET), &cstat_regs[R], val)
#define CSTAT_GET_R_COREX(base, R) \
	is_hw_get_reg((base + CSTAT_COREX_SDW_OFFSET), &cstat_regs[R])
#define CSTAT_SET_F(base, R, F, val) \
	is_hw_set_field((base + CSTAT_COREX_SDW_OFFSET), &cstat_regs[R],\
			&cstat_fields[F], val)

/* IP register direct read/write */
#define CSTAT_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &cstat_regs[R], val)
#define CSTAT_GET_R(base, R) \
	is_hw_get_reg(base, &cstat_regs[R])
#define CSTAT_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &cstat_regs[R], &cstat_fields[F], val)
#define CSTAT_GET_F(base, R, F) \
	is_hw_get_field(base, &cstat_regs[R], &cstat_fields[F])
#define CSTAT_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &cstat_fields[F], val)
#define CSTAT_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &cstat_fields[F])

#define CSTAT_TRY_COUNT			10000
#define CSTAT_CH_CNT			4
#define CSTAT_LUT_REG_CNT		1650
#define CSTAT_LUT_NUM			2	/* CGRAS, LSC */
#define CSTAT_MIN_BNS_RATIO		CSTAT_BNS_X2P0

/* Guided value */
#define LIC_WEIGHT_MAX			7
#define LIC_VL_IMAGE			2
#define LIC_VL_PREEMPTION_THRES_DYNAMIC	1024
#define LIC_VL_PREEMPTION_THRES_STATIC	512
#define VBLANK_CYCLE			45
#define HBLANK_CYCLE			10
#define CDAF_STAT_LENGTH		211
#define BNS_LINEGAP			35

/* Constraints */
#define CSTAT_LMEDS_OUT_MAX_W		2016
#define CSTAT_LMEDS_OUT_MAX_H		1512
#define CSTAT_CDS_OUT_MAX_W		1920
#define CSTAT_CDS_OUT_MAX_H		1080
#define CSTAT_FDPIG_OUT_MAX_W		640
#define CSTAT_FDPIG_OUT_MAX_H		480
#define CSTAT_SRAM_MAX_SIZE		16384

struct cstat_hw_dma {
	enum cstat_dma_id dma_id;
	enum is_hw_cstat_reg_name reg_id;
	char *name;
};

const static struct cstat_hw_dma cstat_dmas[CSTAT_DMA_NUM] = {
	{CSTAT_DMA_NONE,	CSTAT_REG_CNT,			"DMA_NONE"},
	{CSTAT_R_CL,		CSTAT_R_STAT_RDMACL_EN,		"CSTAT_R_CL"},
	{CSTAT_R_BYR,		CSTAT_R_BYR_RDMABYRIN_EN,	"CSTAT_R_BYR"},
	{CSTAT_W_RGBYHIST,	CSTAT_R_STAT_WDMARGBYHIST_EN,	"CSTAT_W_RGBYHIST"},
	{CSTAT_W_VHIST,		CSTAT_R_STAT_WDMAVHIST_EN,	"CSTAT_W_VHIST"},
	{CSTAT_W_THSTATPRE,	CSTAT_R_STAT_WDMATHSTATPRE_EN,	"CSTAT_W_THSTATPRE"},
	{CSTAT_W_THSTATAWB,	CSTAT_R_STAT_WDMATHSTATAWB_EN,	"CSTAT_W_THSTATAWB"},
	{CSTAT_W_THSTATAE,	CSTAT_R_STAT_WDMATHSTATAE_EN,	"CSTAT_W_THSTATAE"},
	{CSTAT_W_DRCGRID,	CSTAT_R_STAT_WDMADRCGRID_EN,	"CSTAT_W_DRCGRID"},
	{CSTAT_W_LMEDS0,	CSTAT_R_Y_WDMALMEDS_EN,		"CSTAT_W_LMEDS"},
	{CSTAT_W_MEDS0,		CSTAT_R_Y_WDMAMEDS0_EN,		"CSTAT_W_MEDS0"},
	{CSTAT_W_MEDS1,		CSTAT_R_Y_WDMAMEDS1_EN,		"CSTAT_W_MEDS1"},
	{CSTAT_W_FDPIG,		CSTAT_R_RGB_WDMAFDPIG_EN,	"CSTAT_W_FDPIG"},
	{CSTAT_W_CDS0,		CSTAT_R_YUV_WDMACDS0_EN,	"CSTAT_W_CDS0"},
};

struct cstat_dma_cfg {
	u32 *addrs;
	u32 num_planes;
	u32 num_buffers;
	u32 buf_idx;
	u32 p_size[3]; /* Payload size of each plane */
	u32 sbwc_en;
};

struct cstat_sram_offset {
	u32 pre[CSTAT_CH_CNT];
	u32 post[CSTAT_CH_CNT];
};

static struct cstat_sram_offset sram_offset = {
	.pre = {0, 5440, 10880, 16320},	/* offset < 16384 & 32px aligned (8ppc) */
	.post = {0, 2720, 5440, 8160},	/* offset <  8192 & 16px aligned (4ppc) */
};

struct cstat_hw_bns_factor {
	u32 fac;
	u32 div;
	u32 mul;
};

const static struct cstat_hw_bns_factor bns_factors[CSTAT_BNS_RATIO_NUM] = {
	/* factor, divider, multiplier */
	{2,	1,	1}, /* CSTAT_BNS_X1P0 */
	{0,	10,	8}, /* CSTAT_BNS_X1P25 */
	{3,	6,	4}, /* CSTAT_BNS_X1P5 */
	{1,	14,	8}, /* CSTAT_BNS_X1P75 */
	{4,	8,	4}, /* CSTAT_BNS_X2P0 */
};

#define CSTAT_BNS_FACTOR_NUM	5
#define CSTAT_BNS_WEIGHT_NUM	11
const static u32 bns_weights[CSTAT_BNS_FACTOR_NUM][CSTAT_BNS_WEIGHT_NUM] = {
	{ 13, 228,  15, 208,  48, 112, 144,  80, 176,   0,   0},
	{ 29, 184,  43,  15, 228,  13,  69, 151,  36, 144, 112},
	{  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
	{ 23, 204,  29,  96, 160,   0,   0,   0,   0,   0,   0},
	{ 12, 183,  61,   0,   0,   0,   0,   0,   0,   0,   0},
};

#define CSTAT_GET_BNS_OUT_SIZE(in, ratio) \
	(((in) / bns_factors[ratio].div) * bns_factors[ratio].mul)

enum cstat_ds_id {
	CSTAT_DS_FDPIG,
	CSTAT_DS_CDS0,
};

struct cstat_ds_cfg {
	u32 scale_x; /* @5.12 */
	u32 scale_y; /* @5.12 */
	u32 inv_scale_x; /* @0.14 */
	u32 inv_scale_y; /* @0.14 */
	u32 inv_shift_x; /* 26-31 */
	u32 inv_shift_y; /* 26-31 */
	struct is_crop in_crop; /* Crop */
	struct is_crop ot_crop; /* Scale down */
};

struct cstat_blk_reg_cfg {
	u32 reg_id;
	int val;
};

struct cstat_blk_reg_cfg cstat_bitmask[] = {
	{CSTAT_R_BYR_BITMASK_BYPASS, 0},
	{CSTAT_R_BYR_BITMASK_BITTAGEIN, 10},
	{CSTAT_R_BYR_BITMASK_BITTAGEOUT, 14},
};

struct cstat_blk_reg_cfg cstat_make_rgb[] = {
	{CSTAT_R_BYR_MAKERGB_BYPASS, 0},
	{CSTAT_R_BYR_MAKERGB_PIXEL_ORDER, 0},
	{CSTAT_R_BYR_MAKERGB_RGB_LOWER_LIMIT, -4095},
	{CSTAT_R_BYR_MAKERGB_RGB_HIGHER_LIMIT, 4095},
	{CSTAT_R_BYR_MAKERGB_DMSC_POST_HPF_TH_HIGH, 2560},
	{CSTAT_R_BYR_MAKERGB_DMSC_POST_HPF_TH_LOW, -2560},
	{CSTAT_R_BYR_MAKERGB_DMSC_POST_HPF_GAIN, 128},
	{CSTAT_R_BYR_MAKERGB_PEDESTAL, 0},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_0, 0},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_1, -3},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_2, 119},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_3, -222},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_4, 393},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_0_5, 456},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_0, 0},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_1, 256},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_2, -12},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_3, -275},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_4, -12},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_1_5, 598},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_0, 0},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_1, -220},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_2, 512},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_3, 103},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_4, 3},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_2_5, 668},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_0, 0},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_1, -60},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_2, 7},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_3, -175},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_4, 512},
	{CSTAT_R_BYR_MAKERGB_DMSC_COEFF_3_5, 576},
	{CSTAT_R_BYR_MAKERGB_LINEGAP, 15},
};

struct cstat_blk_reg_cfg cstat_rgb_to_yuv_cds0[] = {
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_R1, 306},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_R2, -172},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_R3, 512},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_G1, 601},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_G2, -338},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_G3, -428},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_B1, 117},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_B2, 512},
	{CSTAT_R_RGB_RGBTOYUVCDS0_COEFF_B3, -82},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_YMIN, 0},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_YMAX, 1023},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_UMIN, 0},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_UMAX, 1023},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_VMIN, 0},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LIMITS_VMAX, 1023},
	{CSTAT_R_RGB_RGBTOYUVCDS0_LSHIFT, 0},
	{CSTAT_R_RGB_RGBTOYUVCDS0_OFFSET_Y, 0},
	{CSTAT_R_RGB_RGBTOYUVCDS0_OFFSET_U, 512},
	{CSTAT_R_RGB_RGBTOYUVCDS0_OFFSET_V, 512}
};

struct cstat_blk_reg_cfg cstat_444_to_422_cds0[] = {
	{CSTAT_R_YUV_YUV444TO422CDS0_MCOEFFS_0, 0x20402000},
	{CSTAT_R_YUV_YUV444TO422CDS0_MCOEFFS_1, 0},
};

static __always_inline void cstat_hw_wait_corex_idle(void __iomem *base)
{
	u32 try_cnt = 0;

	while (CSTAT_GET_F(base, CSTAT_R_COREX_STATUS_0, CSTAT_F_COREX_BUSY_0)) {
		udelay(1);

		if (++try_cnt >= CSTAT_TRY_COUNT) {
			err_hw("[CSTAT] Failed to wait COREX idle");
			break;
		}

		dbg_hw(2, "[CSTAT]%s: try_cnt %d\n", __func__, try_cnt);
	}
}

static void _cstat_hw_s_corex_enable(void __iomem *base, bool enable)
{
	u32 i;

	if (!enable) {
		/* Disable COREX HW trigger */
		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_MODE_0,
				CSTAT_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_MODE_1,
				CSTAT_F_COREX_UPDATE_MODE_1, SW_TRIGGER);

		cstat_hw_wait_corex_idle(base);

		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_ENABLE, CSTAT_F_COREX_ENABLE, 0);

		if (!in_irq())
			info_hw("[CSTAT] Disable COREX done\n");

		return;
	}

	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_ENABLE, CSTAT_F_COREX_ENABLE, 1);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_RESET, CSTAT_F_COREX_RESET, 1);

	/* Reset the address of register type table to write */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_TYPE_WRITE_TRIGGER,
			CSTAT_F_COREX_TYPE_WRITE_TRIGGER, 1);

	/* Loop per 32 regs: Set type0 for every registers */
	for (i = 0; i < DIV_ROUND_UP(cstat_regs[CSTAT_REG_CNT - 1].sfr_offset, (4 * 32)); i++)
		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_TYPE_WRITE, CSTAT_F_COREX_TYPE_WRITE, 0);

	/* Config type0 registers: Copy from SRAM & SW trigger */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_TYPE_0,
			CSTAT_F_COREX_UPDATE_TYPE_0, COREX_COPY);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_TYPE_1,
			CSTAT_F_COREX_UPDATE_TYPE_1, COREX_IGNORE);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_MODE_0,
			CSTAT_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_MODE_1,
			CSTAT_F_COREX_UPDATE_MODE_1, SW_TRIGGER);

	/* Initialize type0 SRAM registers from IP's */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_COPY_FROM_IP_0, CSTAT_F_COREX_COPY_FROM_IP_0, 1);

	/* Finishing COREX configuration */
	cstat_hw_wait_corex_idle(base);

	/* SW trigger to copy from SRAM */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_START_0, CSTAT_F_COREX_START_0, 1);

	/* Finishing copy from SRAM */
	cstat_hw_wait_corex_idle(base);

	/* Change into HW trigger mode */
	if (!IS_ENABLED(CSTAT_USE_COREX_SW_TRIGGER))
		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_MODE_0,
				CSTAT_F_COREX_UPDATE_MODE_0, HW_TRIGGER);

	info_hw("[CSTAT] Enable COREX done\n");
}

void cstat_hw_s_corex_type(void __iomem *base, u32 type)
{
	if (IS_ENABLED(CSTAT_USE_COREX_SW_TRIGGER))
		return;

	CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_UPDATE_TYPE_0, CSTAT_F_COREX_UPDATE_TYPE_0, type);

	if (type == COREX_IGNORE)
		cstat_hw_wait_corex_idle(base);
}

void cstat_hw_s_corex_delay(void __iomem *base, struct cstat_time_cfg *time_cfg)
{
	int delay;
	u32 cur_delay;
	ulong clk = 267000000; /* 267Mhz */

	dbg_hw(3, "[CSTAT]%s: vvalid %d req_vvalid %d clk %ld\n", __func__,
			time_cfg->vvalid,
			time_cfg->req_vvalid,
			time_cfg->clk);


	clk = (time_cfg->clk > clk) ? time_cfg->clk : clk;
	delay = (int)(time_cfg->req_vvalid - time_cfg->vvalid); /* usec */

	if (delay <= 0)
		delay = 0x10;	/* reset value */
	else
		delay *= (u32)(clk / 1000000); /* cycle */

	cur_delay = CSTAT_GET_R(base, CSTAT_R_COREX_DELAY_HW_TRIGGER);
	if (delay != cur_delay) {
		info_hw("[CSTAT] Change corex_delay %d -> %d vvalid %d(%d) clk %lu\n",
				cur_delay, delay,
				time_cfg->vvalid, time_cfg->req_vvalid,
				clk);

		CSTAT_SET_R_DIRECT(base, CSTAT_R_COREX_DELAY_HW_TRIGGER, delay);
	}
}

static __always_inline int cstat_hw_wait_idle(void __iomem *base)
{
	u32 try_cnt = 0;

	while (!CSTAT_GET_F(base, CSTAT_R_IDLENESS_STATUS, CSTAT_F_IDLENESS_STATUS)) {
		udelay(3); /* 3us * 10000 = 30ms */

		if (++try_cnt >= CSTAT_TRY_COUNT) {
			err_hw("[CSTAT] Failed to wait idle");

			return -EBUSY;
		}

		dbg_hw(2, "[CSTAT]%s: try_cnt %d\n", __func__, try_cnt);
	}

	return 0;
}

static void cstat_hw_s_cinfifo(void __iomem *base, bool enable)
{
	if (!enable) {
		CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_ENABLE, CSTAT_F_BYR_CINFIFO_ENABLE, 0);

		return;
	}

	/*
	 * Enable Input STALL signal for below status
	 *  1) Until frame_start
	 *  2) IP busy (frame overlap)
	 *  3) Vblank
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);

	/*
	 * Auto recovery for below status
	 *  1) Protocal violation or pixel missing (frame missing condition)
	 *  2) Stall rising on stall is NOT allowed (overflow)
	 *  3) Frame overlap (When Input STALL is being disabled)
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_AUTO_RECOVERY_EN, 1);

	/*
	 * Enable Register On Latch
	 *  Reset
	 *   0: Reset by ROL_RESET trigger
	 *   1: Reset by frame_start
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_ROL_EN, 1);
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_ROL_RESET_ON_FRAME_START, 1);

	/*
	 * Enable debugging feature
	 *  - Monitor stall count
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_DEBUG_EN, 1);

	/*
	 * Enable STRGEN
	 *  data_type
	 *   0: fixed data (configured by mode_data)
	 *   1: incremental data (0~255), MSB aligned
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_STRGEN_MODE_DATA_TYPE, 0);
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_STRGEN_MODE_DATA, 127); /* u8-bit: 0 ~ 255 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_CONFIG,
			CSTAT_F_BYR_CINFIFO_STRGEN_MODE_EN, 0);

	/*
	 * Interval configuration
	 */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_INTERVAL_VBLANK,
			CSTAT_F_BYR_CINFIFO_INTERVAL_VBLANK, VBLANK_CYCLE); /* Cycle: 32-bit: 2 ~ */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_INTERVALS,
			CSTAT_F_BYR_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE); /* Cycle: 16-bit: 1 ~ */
	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_INTERVALS,
			CSTAT_F_BYR_CINFIFO_INTERVAL_PIXEL, 0x0); /* Cycle: 16-bit: 0 ~ */

	CSTAT_SET_F(base, CSTAT_R_BYR_CINFIFO_ENABLE, CSTAT_F_BYR_CINFIFO_ENABLE, 1);
}

static void cstat_hw_g_fmt_map(enum cstat_dma_id dma_id,
		ulong *byr_fmt_map,
		ulong *yuv_fmt_map,
		ulong *rgb_fmt_map)
{
	switch (dma_id) {
	case CSTAT_R_CL:
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U10BIT_PACK)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID10)
				| BIT_MASK(DMA_FMT_U12BIT_PACK)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID12)
				| BIT_MASK(DMA_FMT_U14BIT_PACK)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
			       ) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case CSTAT_R_BYR:
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_PACK)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID10)
				| BIT_MASK(DMA_FMT_U12BIT_PACK)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
				| BIT_MASK(DMA_FMT_ANDROID12)
				| BIT_MASK(DMA_FMT_U14BIT_PACK)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
			       ) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case CSTAT_W_RGBYHIST:
	case CSTAT_W_VHIST:
	case CSTAT_W_THSTATPRE:
	case CSTAT_W_THSTATAWB:
	case CSTAT_W_THSTATAE:
	case CSTAT_W_DRCGRID:
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
			       ) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case CSTAT_W_LMEDS0:
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
			       ) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case CSTAT_W_MEDS0:
	case CSTAT_W_MEDS1:
		*byr_fmt_map = (0
				| BIT_MASK(DMA_FMT_U8BIT_PACK)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO)
				| BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)
			       ) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case CSTAT_W_FDPIG:
		*byr_fmt_map = 0;
		*yuv_fmt_map = (0
				| BIT_MASK(DMA_FMT_YUV444_3P)
			       ) & IS_YUV_FORMAT_MASK;
		*rgb_fmt_map = (0
				| BIT_MASK(DMA_FMT_RGB_RGBA8888)
				| BIT_MASK(DMA_FMT_RGB_ARGB8888)
				| BIT_MASK(DMA_FMT_RGB_ABGR8888)
				| BIT_MASK(DMA_FMT_RGB_BGRA8888)
			       ) & IS_RGB_FORMAT_MASK;
		break;
	case CSTAT_W_CDS0:
		*byr_fmt_map = 0;
		*yuv_fmt_map = (0
				| BIT_MASK(DMA_FMT_YUV420_2P_UFIRST)
				| BIT_MASK(DMA_FMT_YUV420_2P_VFIRST)
				| BIT_MASK(DMA_FMT_YUV422_2P_UFIRST)
				| BIT_MASK(DMA_FMT_YUV444_1P)
			       ) & IS_YUV_FORMAT_MASK;
		*rgb_fmt_map = (0
				| BIT_MASK(DMA_FMT_RGB_RGBA8888)
				| BIT_MASK(DMA_FMT_RGB_ABGR8888)
				| BIT_MASK(DMA_FMT_RGB_ARGB8888)
				| BIT_MASK(DMA_FMT_RGB_BGRA8888)
			       ) & IS_RGB_FORMAT_MASK;
		break;
	default:
		err_hw("[CSTAT] Invalid DMA id %d", dma_id);
		break;
	};
}

static void cstat_hw_s_dma_addrs(struct is_common_dma *dma, struct cstat_dma_cfg *cfg)
{
	u32 b, p, i;
	dma_addr_t addr[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	for (p = 0; p < cfg->num_planes; p++) {
		for (b = 0; b < cfg->num_buffers; b++) {
			i = (b * cfg->num_planes) + p;
			addr[b] = (dma_addr_t) cfg->addrs[i];
		}

		CALL_DMA_OPS(dma, dma_set_img_addr, addr, p,
				cfg->buf_idx, cfg->num_buffers);
	}


	if (cfg->sbwc_en == 1) {
		for (p = 0; p < cfg->num_planes; p++) {
			for (b = 0; b < cfg->num_buffers; b++)
				hdr_addr[b] = addr[b] + cfg->p_size[p];

			CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, p,
					cfg->buf_idx, cfg->num_buffers);
		}
	}
}

static void cstat_hw_s_crop_in(void __iomem *base, bool en, struct is_crop *crop)
{
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPIN_START_X,
			CSTAT_F_BYR_CROPIN_START_X, crop->x);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPIN_START_Y,
			CSTAT_F_BYR_CROPIN_START_Y, crop->y);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPIN_SIZE_X,
			CSTAT_F_BYR_CROPIN_SIZE_X, crop->w);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPIN_SIZE_Y,
			CSTAT_F_BYR_CROPIN_SIZE_Y, crop->h);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPIN_BYPASS,
			CSTAT_F_BYR_CROPIN_BYPASS, !en);
}

static void cstat_hw_s_crop_dzoom(void __iomem *base, bool en, struct is_crop *crop)
{
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPDZOOM_START_X,
			CSTAT_F_BYR_CROPDZOOM_START_X, crop->x);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPDZOOM_START_Y,
			CSTAT_F_BYR_CROPDZOOM_START_Y, crop->y);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPDZOOM_SIZE_X,
			CSTAT_F_BYR_CROPDZOOM_SIZE_X, crop->w);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPDZOOM_SIZE_Y,
			CSTAT_F_BYR_CROPDZOOM_SIZE_Y, crop->h);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPDZOOM_BYPASS,
			CSTAT_F_BYR_CROPDZOOM_BYPASS, !en);
}

static void cstat_hw_s_crop_bns(void __iomem *base, bool en, struct is_crop *crop)
{
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPBNS_START_X,
			CSTAT_F_BYR_CROPBNS_START_X, crop->x);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPBNS_START_Y,
			CSTAT_F_BYR_CROPBNS_START_Y, crop->y);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPBNS_SIZE_X,
			CSTAT_F_BYR_CROPBNS_SIZE_X, crop->w);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPBNS_SIZE_Y,
			CSTAT_F_BYR_CROPBNS_SIZE_Y, crop->h);
	CSTAT_SET_F(base, CSTAT_R_BYR_CROPBNS_BYPASS,
			CSTAT_F_BYR_CROPBNS_BYPASS, !en);
}

static void cstat_hw_s_crop_menr(void __iomem *base, struct is_crop *crop)
{
	u32 val;
	u32 b_ratio = 0x400; /* 1.0(10 fractional bits) */

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_CROPX, crop->x);
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_CROPY, crop->y);
	CSTAT_SET_R(base, CSTAT_R_Y_MENR_CROP, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_X_IMAGE_SIZE, crop->w);
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_XBIN, b_ratio);
	CSTAT_SET_R(base, CSTAT_R_Y_MENR_X_IMAGE_SIZE, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_Y_IMAGE_SIZE, crop->h);
	val = CSTAT_SET_V(val, CSTAT_F_Y_MENR_YBIN, b_ratio);
	CSTAT_SET_R(base, CSTAT_R_Y_MENR_Y_IMAGE_SIZE, val);
}

static void cstat_hw_s_grid_cgras(void __iomem *base, struct cstat_grid_cfg *cfg)
{
	u32 val;

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_CGRAS_BINNING_X, cfg->binning_x);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_CGRAS_BINNING_Y, cfg->binning_y);
	CSTAT_SET_R(base, CSTAT_R_BYR_CGRAS_BINNING_X, val);

	CSTAT_SET_F(base, CSTAT_R_BYR_CGRAS_CROP_START_X,
			CSTAT_F_BYR_CGRAS_CROP_START_X, cfg->crop_x);
	CSTAT_SET_F(base, CSTAT_R_BYR_CGRAS_CROP_START_Y,
			CSTAT_F_BYR_CGRAS_CROP_START_Y, cfg->crop_y);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_CGRAS_CROP_RADIAL_X, cfg->crop_radial_x);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_CGRAS_CROP_RADIAL_Y, cfg->crop_radial_y);
	CSTAT_SET_R(base, CSTAT_R_BYR_CGRAS_CROP_START_REAL, val);
}

static void cstat_hw_s_grid_lsc(void __iomem *base, struct cstat_grid_cfg *cfg)
{
	u32 val;

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_LUMASHADING_BINNING_X, cfg->binning_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_LUMASHADING_BINNING_Y, cfg->binning_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_LUMASHADING_BINNING, val);

	CSTAT_SET_F(base, CSTAT_R_RGB_LUMASHADING_CROP_START_X,
			CSTAT_F_RGB_LUMASHADING_CROP_START_X, cfg->crop_x);
	CSTAT_SET_F(base, CSTAT_R_RGB_LUMASHADING_CROP_START_Y,
			CSTAT_F_RGB_LUMASHADING_CROP_START_Y, cfg->crop_y);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_LUMASHADING_CROP_RADIAL_X, cfg->crop_radial_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_LUMASHADING_CROP_RADIAL_Y, cfg->crop_radial_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_LUMASHADING_CROP_START_REAL, val);
}

static void cstat_hw_s_ds_lme(void __iomem *base, bool en, struct cstat_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		val = 0;
		val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_BYPASS, 1);
		val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_OUTPUT_EN, 0);
		CSTAT_SET_R(base, CSTAT_R_Y_LMEDS_BYPASS, val);

		return;
	}

	/* Scale configuration */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_OUT_W, cfg->ot_crop.w);
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_OUT_H, cfg->ot_crop.h);
	CSTAT_SET_R(base, CSTAT_R_Y_LMEDS_OUT, val);

	/* Scale factor configuration */
	CSTAT_SET_F(base, CSTAT_R_Y_LMEDS_X_SCALE,
			CSTAT_F_Y_LMEDS_SCALE_FACTOR_X, cfg->scale_x);
	CSTAT_SET_F(base, CSTAT_R_Y_LMEDS_Y_SCALE,
			CSTAT_F_Y_LMEDS_SCALE_FACTOR_Y, cfg->scale_y);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_INV_SCALE_X, cfg->inv_scale_x);
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_INV_SCALE_Y, cfg->inv_scale_y);
	CSTAT_SET_R(base, CSTAT_R_Y_LMEDS_INV_SCALE, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_INV_SHIFT_X, cfg->inv_shift_x);
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_INV_SHIFT_Y, cfg->inv_shift_y);
	CSTAT_SET_R(base, CSTAT_R_Y_LMEDS_INV_SHIFT, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_BYPASS, 0);
	val = CSTAT_SET_V(val, CSTAT_F_Y_LMEDS_OUTPUT_EN, 1);
	CSTAT_SET_R(base, CSTAT_R_Y_LMEDS_BYPASS, val);
}

static void cstat_hw_s_ds_fdpig(void __iomem *base, bool en, struct cstat_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		val = 0;
		val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_BYPASS_FDS, 1);
		val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_ENABLE_FDS, 0);
		CSTAT_SET_R(base, CSTAT_R_RGB_DS_BYPASS_FDS, val);

		return;
	}

	/* Crop configuration */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_CROP_START_X, cfg->in_crop.x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_CROP_START_Y, cfg->in_crop.y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_FDPIG_CROP_START, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_CROP_SIZE_X, cfg->in_crop.w);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_CROP_SIZE_Y, cfg->in_crop.h);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_FDPIG_CROP_SIZE, val);

	CSTAT_SET_F(base, CSTAT_R_RGB_DS_FDPIG_CROP_EN,	CSTAT_F_RGB_DS_FDPIG_CROP_EN, 1);

	/* Scale configuration */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_OUTPUT_W, cfg->ot_crop.w);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_OUTPUT_H, cfg->ot_crop.h);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_FDPIG_OUTPUT_SIZE, val);

	/* Scale factor configuration */
	CSTAT_SET_F(base, CSTAT_R_RGB_DS_FDPIG_SCALE_X,
			CSTAT_F_RGB_DS_FDPIG_SCALE_X, cfg->scale_x);
	CSTAT_SET_F(base, CSTAT_R_RGB_DS_FDPIG_SCALE_Y,
			CSTAT_F_RGB_DS_FDPIG_SCALE_Y, cfg->scale_y);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_INV_SCALE_X, cfg->inv_scale_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_INV_SCALE_Y, cfg->inv_scale_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_FDPIG_INV_SCALE, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_INV_SHIFT_X, cfg->inv_shift_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_FDPIG_INV_SHIFT_Y, cfg->inv_shift_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_FDPIG_INV_SHIFT, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_BYPASS_FDS, 0);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_ENABLE_FDS, 1);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_BYPASS_FDS, val);
}

static void cstat_hw_s_ds_cds(void __iomem *base, bool en, struct cstat_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		val = 0;
		val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_BYPASS_CDS0, 1);
		val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_ENABLE_CDS0, 0);
		CSTAT_SET_R(base, CSTAT_R_RGB_DS_BYPASS_CDS0, val);

		return;
	}

	/* Crop configuration */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_START_X, cfg->in_crop.x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_START_Y, cfg->in_crop.y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_CONTENTS_DS0_CROP_START, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_SIZE_X, cfg->in_crop.w);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_SIZE_Y, cfg->in_crop.h);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_CONTENTS_DS0_CROP_SIZE, val);

	if (cfg->in_crop.x || cfg->in_crop.y) {
		CSTAT_SET_F(base, CSTAT_R_RGB_DS_CONTENTS_DS0_CROP_EN,
				CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_EN, 1);
	} else {
		CSTAT_SET_F(base, CSTAT_R_RGB_DS_CONTENTS_DS0_CROP_EN,
				CSTAT_F_RGB_DS_CONTENTS_DS0_CROP_EN, 0);
	}

	/* Scale configuration */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_OUTPUT_W, cfg->ot_crop.w);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_OUTPUT_H, cfg->ot_crop.h);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_CONTENTS_DS0_OUTPUT_SIZE, val);

	/* Scale factor configuration */
	CSTAT_SET_F(base, CSTAT_R_RGB_DS_CONTENTS_DS0_SCALE_X,
			CSTAT_F_RGB_DS_CONTENTS_DS0_SCALE_X, cfg->scale_x);
	CSTAT_SET_F(base, CSTAT_R_RGB_DS_CONTENTS_DS0_SCALE_Y,
			CSTAT_F_RGB_DS_CONTENTS_DS0_SCALE_Y, cfg->scale_y);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_INV_SCALE_X, cfg->inv_scale_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_INV_SCALE_Y, cfg->inv_scale_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_CONTENTS_DS0_INV_SCALE, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_INV_SHIFT_X, cfg->inv_shift_x);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_CONTENTS_DS0_INV_SHIFT_Y, cfg->inv_shift_y);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_CONTENTS_DS0_INV_SHIFT, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_BYPASS_CDS0, 0);
	val = CSTAT_SET_V(val, CSTAT_F_RGB_DS_ENABLE_CDS0, 1);
	CSTAT_SET_R(base, CSTAT_R_RGB_DS_BYPASS_CDS0, val);
}

static void cstat_hw_s_conv_rgb_2_yuv(void __iomem *base, bool en)
{
	CSTAT_SET_F(base, CSTAT_R_RGB_RGBTOYUVCDS0_BYPASS,
			CSTAT_F_RGB_RGBTOYUVCDS0_BYPASS, !en);
}

static void cstat_hw_s_conv_444_2_422(void __iomem *base, bool en)
{
	CSTAT_SET_F(base, CSTAT_R_YUV_YUV444TO422CDS0_BYPASS,
			CSTAT_F_YUV_YUV444TO422CDS0_BYPASS, !en);
}

static void cstat_hw_s_conv_422_2_420(void __iomem *base, bool en)
{
	CSTAT_SET_F(base, CSTAT_R_YUV_YUV422TO420CDS0_BYPASS,
			CSTAT_F_YUV_YUV422TO420CDS0_BYPASS, !en);
}

static void _cstat_hw_s_global_enable(void __iomem *base, bool enable)
{
	CSTAT_SET_F_DIRECT(base, CSTAT_R_FRO_GLOBAL_ENABLE, CSTAT_F_FRO_GLOBAL_ENABLE, enable);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_GLOBAL_ENABLE, CSTAT_F_GLOBAL_ENABLE, enable);
}

void cstat_hw_s_global_enable(void __iomem *base, bool enable, bool fro_en)
{
	/* COREX must be enabled before global enable */
	_cstat_hw_s_corex_enable(base, enable);

	CSTAT_SET_F_DIRECT(base, CSTAT_R_GLOBAL_ENABLE_STOP_CRPT,
			CSTAT_F_GLOBAL_ENABLE_STOP_CRPT, enable);

	_cstat_hw_s_global_enable(base, enable);

	if (!enable)
		CSTAT_SET_F(base, CSTAT_R_GLOBAL_ENABLE,
				CSTAT_F_GLOBAL_ENABLE_CLEAR, 1);
}

int cstat_hw_s_one_shot_enable(void __iomem *base)
{
	int ret;

	_cstat_hw_s_global_enable(base, false);

	ret = cstat_hw_wait_idle(base);
	if (ret) {
		err_hw("[CSTAT] Failed to wait idle. ret %d", ret);

		return ret;
	}

	CSTAT_SET_F(base, CSTAT_R_FRO_ONE_SHOT_ENABLE, CSTAT_F_FRO_ONE_SHOT_ENABLE, 0);
	CSTAT_SET_F(base, CSTAT_R_FRO_ONE_SHOT_ENABLE, CSTAT_F_FRO_ONE_SHOT_ENABLE, 1);

	CSTAT_SET_F(base, CSTAT_R_ONE_SHOT_ENABLE, CSTAT_F_ONE_SHOT_ENABLE, 0);
	CSTAT_SET_F(base, CSTAT_R_ONE_SHOT_ENABLE, CSTAT_F_ONE_SHOT_ENABLE, 1);

	return 0;
}
KUNIT_EXPORT_SYMBOL(cstat_hw_s_one_shot_enable);

void cstat_hw_corex_trigger(void __iomem *base)
{
	/* SW trigger to copy from SRAM */
	if (IS_ENABLED(CSTAT_USE_COREX_SW_TRIGGER))
		CSTAT_SET_F_DIRECT(base, CSTAT_R_COREX_START_0,
				CSTAT_F_COREX_START_0, 1);
	else
		cstat_hw_s_corex_type(base, COREX_COPY);
}

int cstat_hw_s_reset(void __iomem *base)
{
	CSTAT_SET_R_DIRECT(base, CSTAT_R_SW_RESET, 0x1);

	info_hw("[CSTAT] SW reset\n");

	return cstat_hw_wait_idle(base);
}

void cstat_hw_s_post_frame_gap(void __iomem *base, u32 gap)
{
	/* Increate V-blank interval */
	CSTAT_SET_R_DIRECT(base, CSTAT_R_IP_POST_FRAME_GAP, gap);
}

void cstat_hw_s_int_on_col_row(void __iomem *base, bool enable,
		enum cstat_int_on_col_row_src src, u32 col, u32 row)
{
	u32 val;

	if (!enable) {
		CSTAT_SET_F(base, CSTAT_R_IP_INT_ON_COL_ROW, CSTAT_F_IP_INT_COL_EN, 0);
		CSTAT_SET_F(base, CSTAT_R_IP_INT_ON_COL_ROW, CSTAT_F_IP_INT_ROW_EN, 0);

		return;
	}

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_IP_INT_COL_CORD, col);
	val = CSTAT_SET_V(val, CSTAT_F_IP_INT_ROW_CORD, row);
	CSTAT_SET_R(base, CSTAT_R_IP_INT_ON_COL_ROW_CORD, val);

	info_hw("[CSTAT]Set LINE_INT %dx%d. input %d\n",
			col, row, src);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_IP_INT_SRC_SEL, (u32)src);
	val = CSTAT_SET_V(val, CSTAT_F_IP_INT_COL_EN, 1);
	val = CSTAT_SET_V(val, CSTAT_F_IP_INT_ROW_EN, 1);
	CSTAT_SET_R(base, CSTAT_R_IP_INT_ON_COL_ROW, val);
}

void cstat_hw_s_freeze_on_col_row(void __iomem *base,
		enum cstat_int_on_col_row_src src, u32 col, u32 row)
{
	u32 val;
	u32 cur_col, cur_row;

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_IP_DBG_CORE_FREEZE_TARGET_COL, col);
	val = CSTAT_SET_V(val, CSTAT_F_IP_DBG_CORE_FREEZE_TARGET_ROW, row);
	CSTAT_SET_R(base, CSTAT_R_IP_DBG_CORE_FREEZE_ON_COL_ROW_TARGET, val);

	CSTAT_SET_F(base, CSTAT_R_IP_DBG_CORE_FREEZE_ON_COL_ROW,
			CSTAT_F_IP_DBG_CORE_FREEZE_SRC_SEL, (u32)src);

	cur_col = CSTAT_GET_F(base, CSTAT_R_IP_DBG_CORE_FREEZE_ON_COL_ROW_POS,
			CSTAT_F_IP_DBG_CORE_FREEZE_POS_COL);
	cur_row = CSTAT_GET_F(base, CSTAT_R_IP_DBG_CORE_FREEZE_ON_COL_ROW_POS,
			CSTAT_F_IP_DBG_CORE_FREEZE_POS_ROW);

	info_hw("[CSTAT]Freeze on %dx%d. cur_pos %dx%d input %d\n",
			col, row, cur_col, cur_row, src);

	CSTAT_SET_F(base, CSTAT_R_IP_DBG_CORE_FREEZE_ON_COL_ROW,
			CSTAT_F_IP_DBG_CORE_FREEZE_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_s_freeze_on_col_row);

/* It is valid only for CTX0. */
void cstat_hw_core_init(void __iomem *base)
{
	CSTAT_SET_F_DIRECT(base, CSTAT_R_AUTO_MASK_PREADY,
			CSTAT_F_AUTO_MASK_PREADY, 1);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_IP_PROCESSING,
			CSTAT_F_IP_PROCESSING, 1);
}

void cstat_hw_ctx_init(void __iomem *base)
{
	CSTAT_SET_F_DIRECT(base, CSTAT_R_INTERRUPT_AUTO_MASK,
			CSTAT_F_INTERRUPT_AUTO_MASK, 1);
}

u32 cstat_hw_g_version(void __iomem *base)
{
	return CSTAT_GET_R(base, CSTAT_R_IP_ISP_VERSION);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_g_version);

int cstat_hw_s_input(void __iomem *base, enum cstat_input_path input)
{
	u32 otf_fs_t, dma_fs_t, fe_t, cinfifo_en;

	switch (input) {
	case OTF:
		otf_fs_t = VVALID_RISE;
		cinfifo_en = 1;
		break;
	case DMA:
		otf_fs_t = VVALID_ASAP;
		cinfifo_en = 0;
		break;
	default:
		err_hw("[CSTAT] Invalid s_input %d", input);
		return -ERANGE;
	}

	dma_fs_t = VVALID_ASAP;
	fe_t = VVALID_FALL;

	CSTAT_SET_F(base, CSTAT_R_CHAIN_INPUT_0_SELECT,
			CSTAT_F_CHAIN_INPUT_0_SELECT, input);

	/* FS timing */
	CSTAT_SET_F(base, CSTAT_R_IP_USE_CINFIFO_FRAME_START_IN,
			CSTAT_F_IP_USE_CINFIFO_FRAME_START_IN, otf_fs_t);
	CSTAT_SET_F(base, CSTAT_R_IP_RDMA_VVALID_START_ENABLE,
			CSTAT_F_IP_RDMA_VVALID_START_ENABLE, dma_fs_t);

	/* FE timing */
	CSTAT_SET_F(base, CSTAT_R_IP_CINFIFO_END_ON_VSYNC_FALL,
			CSTAT_F_IP_CINFIFO_END_ON_VSYNC_FALL, fe_t);
	CSTAT_SET_F(base, CSTAT_R_IP_COUTFIFO_END_ON_VSYNC_FALL,
			CSTAT_F_IP_COUTFIFO_END_ON_VSYNC_FALL, fe_t);

	cstat_hw_s_cinfifo(base, cinfifo_en);

	return 0;
}

int cstat_hw_s_format(void __iomem *base, u32 width, u32 height,
		enum cstat_input_bitwidth bitwidth)
{
	u32 val;

	if (bitwidth >= CSTAT_INPUT_BITWIDTH_NUM) {
		err_hw("[CSTAT] Invalid s_format %dx%d bitwidth %d",
				width, height, bitwidth);
		return -EINVAL;
	}

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_CHAIN_IMG_WIDTH, width);
	val = CSTAT_SET_V(val, CSTAT_F_CHAIN_IMG_HEIGHT, height);
	CSTAT_SET_R(base, CSTAT_R_CHAIN_IMG_SIZE, val);

	CSTAT_SET_F(base, CSTAT_R_CHAIN_INPUT_BITWIDTH,
			CSTAT_F_CHAIN_INPUT_BITWIDTH, bitwidth);

	/* LIC context configuration */
	CSTAT_SET_F(base, CSTAT_R_LIC_INPUT_CONFIG1,
			CSTAT_F_LIC_INPUT_BIT_MODE, bitwidth);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_LIC_INPUT_IMAGE_WIDTH, width);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_INPUT_IMAGE_HEIGHT, height);
	CSTAT_SET_R(base, CSTAT_R_LIC_INPUT_CONFIG2, val);

	return 0;
}

void cstat_hw_s_sdc_enable(void __iomem *base, bool enable)
{
	info_hw("[CSTAT] SDC %s\n",
			enable ? "ON" : "OFF");

	CSTAT_SET_F(base, CSTAT_R_ENABLE_SDC,
			CSTAT_F_ENABLE_SDC, enable);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_s_sdc_enable);

void cstat_hw_s_fro(void __iomem *base, u32 num_buffer, unsigned long grp_mask)
{
	u32 center_frame_num, grp_id;

	if (num_buffer <= 1) {
		CSTAT_SET_F_DIRECT(base, CSTAT_R_FRO_MODE_ENABLE, CSTAT_F_FRO_MODE_ENABLE, 0);

		return;
	}

	CSTAT_SET_F_DIRECT(base, CSTAT_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
		CSTAT_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW, num_buffer - 1);

	center_frame_num = num_buffer / 2 - 1;
	for (grp_id = CSTAT_GRP_1; grp_id < CSTAT_GRP_MASK_NUM; grp_id++) {
		if (test_bit(grp_id, &grp_mask))
			CSTAT_SET_F_DIRECT(base, (CSTAT_R_FRO_RUN_FRAME_NUMBER_FOR_GROUP1 + grp_id),
					CSTAT_F_FRO_RUN_FRAME_NUMBER_FOR_GROUPX, center_frame_num);
	}

	CSTAT_SET_F_DIRECT(base, CSTAT_R_FRO_MODE_ENABLE, CSTAT_F_FRO_MODE_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_s_fro);

void cstat_hw_s_corex_enable(void __iomem *base, bool enable)
{
	_cstat_hw_s_corex_enable(base, enable);
}

void cstat_hw_s_int_enable(void __iomem *base)
{
	CSTAT_SET_F_DIRECT(base,
			CSTAT_R_CONTINT_SIPUIPP1P0P0_LEVEL_PULSE_N_SEL,
			CSTAT_F_CONTINT_SIPUIPP1P0P0_LEVEL_PULSE_N_SEL,
			CSTAT_INT_LEVEL);

	/*
	 * The IP_END_INTERRUPT_ENABLE makes and condition.
	 * If some bits are set,
	 * frame end interrupt is generated when all designated blocks must be finished.
	 * So, unused block must be not set.
	 * Usually all bit doesn't have to set becase we don't care about each block end.
	 * We are only interested in core end interrupt.
	 */
	CSTAT_SET_F(base, CSTAT_R_IP_USE_END_INTERRUPT_ENABLE,
			CSTAT_F_IP_USE_END_INTERRUPT_ENABLE, 0);
	CSTAT_SET_F(base, CSTAT_R_IP_END_INTERRUPT_ENABLE,
			CSTAT_F_IP_END_INTERRUPT_ENABLE, 0);

	CSTAT_SET_R(base, CSTAT_R_IP_CORRUPTED_INTERRUPT_ENABLE, INT_CRPT_EN_MASK);
	CSTAT_SET_R_DIRECT(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT1_ENABLE, INT1_EN_MASK);
	CSTAT_SET_R_DIRECT(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT2_ENABLE, INT2_EN_MASK);
}

u32 cstat_hw_g_int1_status(void __iomem *base, bool clear, bool fro_en)
{
	u32 src, fro_src;

	src = CSTAT_GET_R(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT1);
	fro_src = CSTAT_GET_R(base, CSTAT_R_FRO_INT0);

	if (clear) {
		CSTAT_SET_R_DIRECT(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT1_CLEAR, src);
		CSTAT_SET_R_DIRECT(base, CSTAT_R_FRO_INT0_CLEAR, fro_src);
	}

	return (src | fro_src);
}

u32 cstat_hw_g_int2_status(void __iomem *base, bool clear, bool fro_en)
{
	u32 src, fro_src;

	src = CSTAT_GET_R(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT2);
	fro_src = CSTAT_GET_R(base, CSTAT_R_FRO_INT1);

	if (clear) {
		CSTAT_SET_R_DIRECT(base, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT2_CLEAR, src);
		CSTAT_SET_R_DIRECT(base, CSTAT_R_FRO_INT1_CLEAR, fro_src);
	}

	return (src | fro_src);
}

void cstat_hw_wait_isr_clear(void __iomem *base, bool fro_en)
{
	u32 try_cnt;
	u32 int_status;

	try_cnt = 0;
	while (INT1_EN_MASK & (int_status = cstat_hw_g_int1_status(base, false, fro_en))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[CSTAT] Failed to wait int1 clear. %x", int_status);
			break;
		}

		dbg_hw(2, "[CSTAT]%s: try_cnt %d\n", __func__, try_cnt);
	}

	try_cnt = 0;
	while (INT2_EN_MASK & (int_status = cstat_hw_g_int2_status(base, false, fro_en))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[CSTAT] Failed to wait int2 clear. %x", int_status);
			break;
		}

		dbg_hw(2, "[CSTAT]%s: try_cnt %d\n", __func__, try_cnt);
	}
}

u32 cstat_hw_is_occurred(ulong state, enum cstat_event_type type)
{
	u32 mask;

	switch (type) {
	case CSTAT_FS:
		mask = BIT_MASK(FRAME_START);
		break;
	case CSTAT_LINE:
		mask = BIT_MASK(FRAME_INT_ON_ROW_COL_INFO);
		break;
	case CSTAT_FE:
		mask = BIT_MASK(FRAME_END);
		break;
	case CSTAT_RGBY:
		mask = BIT_MASK(RGBYHIST_OUT);
		break;
	case CSTAT_COREX_END:
		mask = BIT_MASK(COREX_END_INT_0);
		break;
	case CSTAT_ERR:
		mask = (BIT_MASK(IRQ_CORRUPTED) |
			BIT_MASK(COREX_ERR) |
			BIT_MASK(SDC_ERR) |
			BIT_MASK(LIC_OVERFLOW) |
			BIT_MASK(LIC_ROW_ERR) |
			BIT_MASK(LIC_COL_ERR) |
			BIT_MASK(CINFIFO_PROT_ERR) |
			BIT_MASK(CINFIFO_PCNT_ERR) |
			BIT_MASK(CINFIFO_OVERFLOW) |
			BIT_MASK(CINFIFO_OVERLAP));
		break;
	case CSTAT_CDAF:
		mask = BIT_MASK(CDAF_DONE);
		break;
	case CSTAT_THSTAT_PRE:
		mask = BIT_MASK(THSTAT_PRE_DONE);
		break;
	case CSTAT_THSTAT_AWB:
		mask = BIT_MASK(THSTAT_AWB_DONE);
		break;
	case CSTAT_THSTAT_AE:
		mask = BIT_MASK(THSTAT_AE_DONE);
		break;
	default:
		mask = 0;
		break;
	};

	return state & mask;
}

void cstat_hw_print_err(char *name, u32 instance, u32 fcount, ulong src1, ulong src2)
{
	u32 err;

	err = find_first_bit(&src1, CSTAT_INT1_CNT);
	while (err < CSTAT_INT1_CNT) {
		switch (err) {
		case IRQ_CORRUPTED:
		case COREX_ERR:
		case SDC_ERR:
		case LIC_OVERFLOW:
		case LIC_ROW_ERR:
		case LIC_COL_ERR:
		case CINFIFO_PROT_ERR:
		case CINFIFO_PCNT_ERR:
		case CINFIFO_OVERFLOW:
		case CINFIFO_OVERLAP:
			err_hw("[%d][%s][F%d] %s 0x%lx 0x%lx", instance, name, fcount,
					cstat_int1_str[err], src1, src2);
			break;
		default:
			/* Do nothing */
			break;
		}

		err = find_next_bit(&src1, CSTAT_INT1_CNT, err + 1);
	}
}
KUNIT_EXPORT_SYMBOL(cstat_hw_print_err);

void cstat_hw_s_seqid(void __iomem *base, u32 seqid)
{
	/* TODO: get secure scenario */
	CSTAT_SET_F(base, CSTAT_R_SECU_CTRL_SEQID, CSTAT_F_SECU_CTRL_SEQID, seqid);
}

void cstat_hw_s_crc_seed(void __iomem *base, u32 seed)
{

}

int cstat_hw_create_dma(void __iomem *base, enum cstat_dma_id dma_id,
		struct is_common_dma *dma)
{
	int ret;
	void __iomem *dma_base;
	ulong byr_fmt_map, yuv_fmt_map, rgb_fmt_map;

	if (dma_id >= CSTAT_DMA_NUM) {
		err_hw("[CSTAT] Invalid dma_id %d", dma_id);
		return -EINVAL;
	}

	dma_base = base + cstat_regs[cstat_dmas[dma_id].reg_id].sfr_offset;
	cstat_hw_g_fmt_map(dma_id, &byr_fmt_map, &yuv_fmt_map, &rgb_fmt_map);

	ret = is_hw_dma_set_ops(dma);
	ret = is_hw_dma_create(dma, dma_base, dma_id, cstat_dmas[dma_id].name,
			byr_fmt_map, yuv_fmt_map, rgb_fmt_map,
			/* state */0);

	dbg_hw(2, "[CSTAT][%s] created. id %d BYR 0x%X YUV 0x%X RGB 0x%X\n",
			cstat_dmas[dma_id].name, dma_id,
			byr_fmt_map, yuv_fmt_map, rgb_fmt_map);

	return 0;
}

static inline bool cstat_hw_is_rgb(u32 hw_order)
{
	/* Common DMA RGB format always covers 4 color channels. */
	switch (hw_order) {
	case DMA_INOUT_ORDER_ARGB:
	case DMA_INOUT_ORDER_BGRA:
	case DMA_INOUT_ORDER_RGBA:
	case DMA_INOUT_ORDER_ABGR:
		return true;
	default:
		return false;
	}
}

static inline bool cstat_hw_is_yuv(u32 hw_format, u32 hw_order)
{
	switch (hw_format) {
	case DMA_INOUT_FORMAT_YUV444:
	case DMA_INOUT_FORMAT_YUV422:
	case DMA_INOUT_FORMAT_YUV420:
	case DMA_INOUT_FORMAT_YUV422_CHUNKER:
	case DMA_INOUT_FORMAT_YUV444_TRUNCATED:
	case DMA_INOUT_FORMAT_YUV422_TRUNCATED:
	case DMA_INOUT_FORMAT_YUV422_PACKED:
		return true;
	case DMA_INOUT_FORMAT_RGB:
		/* 8bit RGB planar format is handled as YUV444 3p format */
		if (hw_order == DMA_INOUT_ORDER_BGR)
			return true;
	default:
		return false;
	}
}

int cstat_hw_s_rdma_cfg(struct is_common_dma *dma,
		struct cstat_param_set *param, u32 num_buffers)
{
	int ret = 0;
	struct param_dma_input *dma_in;
	struct cstat_dma_cfg cfg;
	u32 sbwc_type, lossy_byte32num;
	u32 hw_format, bit_width, pixel_size, format;
	u32 width, height;
	u32 en_votf;
	u32 comp_64b_align, stride_1p, hdr_stride_1p = 0;
	enum format_type dma_type;

	switch (dma->id) {
	case CSTAT_R_BYR:
		dma_in = &param->dma_input;
		cfg.addrs = param->input_dva;
		dma_type = DMA_FMT_BAYER;
		break;
	case CSTAT_DMA_NONE:
		/* Not existing DMA */
		return 0;
	case CSTAT_R_CL:
	default:
		warn_hw("[CSTAT][%s] NOT supported DMA", dma->name);
		return 0;
	}

	if (dma_in->cmd == DMA_INPUT_COMMAND_DISABLE)
		goto skip_dma;

	sbwc_type = dma_in->sbwc_type;
	hw_format = dma_in->format;
	bit_width = dma_in->bitwidth;
	pixel_size = dma_in->msb + 1;
	lossy_byte32num = COMP_LOSSYBYTE32NUM_256X1_U12;
	width = dma_in->width;
	height = dma_in->height;
	en_votf = dma_in->v_otf_enable;

	cfg.sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	switch (cfg.sbwc_en) {
	case 0: /* SBWC_DISABLE */
		stride_1p = is_hw_dma_get_img_stride(bit_width, pixel_size, hw_format,
				width, 16, true);
		break;
	case 1: /* SBWC_LOSSYLESS_32B/64B */
		hdr_stride_1p = is_hw_dma_get_header_stride(width, CSTAT_COMP_BLOCK_WIDTH, 16);
	case 2: /* SBWC_LOSSY_32B/64B */
		stride_1p = is_hw_dma_get_payload_stride(cfg.sbwc_en, pixel_size, width,
				comp_64b_align, lossy_byte32num,
				CSTAT_COMP_BLOCK_WIDTH, CSTAT_COMP_BLOCK_HEIGHT);
		break;
	default:
		err_hw("[CSTAT][%s] Invalid SBWC mode. ret %d", dma->name, cfg.sbwc_en);
		goto skip_dma;
	}

	format = is_hw_dma_get_bayer_format(bit_width, pixel_size, hw_format,
			cfg.sbwc_en, /* is_msb */true);
	if (format < 0)
		goto skip_dma;

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, dma_type);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, cfg.sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	/* ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info); */

	if (ret)
		goto skip_dma;

	switch (cfg.sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, hdr_stride_1p, 0);
		cfg.p_size[0] = ALIGN(height, CSTAT_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	if (ret)
		goto skip_dma;

	cfg.num_planes = dma_in->plane;
	cfg.num_buffers = num_buffers;
	cfg.buf_idx = 0;

	cstat_hw_s_dma_addrs(dma, &cfg);

	dbg_hw(2, "[CSTAT][%s]dma_cfg: size %dx%d format %d-%d plane %d votf %d\n",
			dma->name,
			width, height, dma_type, format, cfg.num_planes, en_votf);
	dbg_hw(2, "[CSTAT][%s]stride_cfg: img %d hdr %d\n",
			dma->name,
			stride_1p, hdr_stride_1p);
	dbg_hw(2, "[CSTAT][%s]sbwc_cfg: en %d 64b_align %d byte32num %d\n",
			dma->name,
			cfg.sbwc_en, comp_64b_align, lossy_byte32num);
	dbg_hw(2, "[CSTAT][%s]dma_addr: img[0] 0x%lx\n",
			dma->name,
			cfg.addrs[0]);

	CALL_DMA_OPS(dma, dma_enable, 1);

	return 0;

skip_dma:
	dbg_hw(2, "[CSTAT][%s]dma_cfg: OFF\n", dma->name);

	CALL_DMA_OPS(dma, dma_enable, 0);

	return 0;
}

static inline int cstat_hw_g_inv_shift(u32 scale)
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

int cstat_hw_s_ds_cfg(void __iomem *base, enum cstat_dma_id dma_id,
		struct cstat_size_cfg *size_cfg,
		struct cstat_param_set *p_set)
{
	struct param_dma_output *dma_out;
	struct cstat_ds_cfg cfg;
	void (*func_ds_cfg)(void __iomem *base, bool en, struct cstat_ds_cfg *cfg);
	u32 max_w, max_h;

	switch (dma_id) {
	case CSTAT_W_LMEDS0:
		dma_out = &p_set->dma_output_lme_ds0;
		func_ds_cfg = cstat_hw_s_ds_lme;
		max_w = CSTAT_LMEDS_OUT_MAX_W;
		max_h = CSTAT_LMEDS_OUT_MAX_H;
		dma_out->dma_crop_offset_x = 0;
		dma_out->dma_crop_offset_y = 0;
		dma_out->dma_crop_width = size_cfg->bcrop.w;
		dma_out->dma_crop_height = size_cfg->bcrop.h;
		break;
	case CSTAT_W_FDPIG:
		dma_out = &p_set->dma_output_fdpig;
		func_ds_cfg = cstat_hw_s_ds_fdpig;
		max_w = CSTAT_FDPIG_OUT_MAX_W;
		max_h = CSTAT_FDPIG_OUT_MAX_H;
		break;
	case CSTAT_W_CDS0:
		dma_out = &p_set->dma_output_cds;
		func_ds_cfg = cstat_hw_s_ds_cds;
		max_w = CSTAT_CDS_OUT_MAX_W;
		max_h = CSTAT_CDS_OUT_MAX_H;
		break;
	default:
		/* Other DMA doesn't have DS or doesn't be controlled by driver. */
		return 0;
	}

	cfg.in_crop.x = CSTAT_GET_BNS_OUT_SIZE(dma_out->dma_crop_offset_x, size_cfg->bns_r);
	cfg.in_crop.y = CSTAT_GET_BNS_OUT_SIZE(dma_out->dma_crop_offset_y, size_cfg->bns_r);
	cfg.in_crop.w = CSTAT_GET_BNS_OUT_SIZE(dma_out->dma_crop_width, size_cfg->bns_r);
	cfg.in_crop.h = CSTAT_GET_BNS_OUT_SIZE(dma_out->dma_crop_height, size_cfg->bns_r);
	cfg.ot_crop.x = 0;
	cfg.ot_crop.y = 0;
	cfg.ot_crop.w = min3(max_w, cfg.in_crop.w, dma_out->width);
	cfg.ot_crop.h = min3(max_h, cfg.in_crop.h, dma_out->height);

	/* Apply the modified ds out size to DMA */
	dma_out->width = cfg.ot_crop.w;
	dma_out->height = cfg.ot_crop.h;

	cfg.scale_x = (cfg.in_crop.w << 12) / cfg.ot_crop.w;
	cfg.scale_y = (cfg.in_crop.h << 12) / cfg.ot_crop.h;
	cfg.inv_shift_x = cstat_hw_g_inv_shift(cfg.scale_x);
	cfg.inv_shift_y = cstat_hw_g_inv_shift(cfg.scale_y);
	cfg.inv_scale_x = (1 << cfg.inv_shift_x) / cfg.scale_x;
	cfg.inv_scale_y = (1 << cfg.inv_shift_y) / cfg.scale_y;

	func_ds_cfg(base, dma_out->cmd, &cfg);

	dbg_hw(2, "[CSTAT][%s]DS: %d,%d %dx%d -> %dx%d\n",
			cstat_dmas[dma_id].name,
			cfg.in_crop.x, cfg.in_crop.y,
			cfg.in_crop.w, cfg.in_crop.h,
			cfg.ot_crop.w, cfg.ot_crop.h);

	return 0;
}

static void cstat_hw_s_conv_cfg(void __iomem *base, u32 hw_format)
{
	bool en_yuv = false, en_422 = false, en_420 = false;

	switch (hw_format) {
	case DMA_INOUT_FORMAT_YUV420:
		en_420 = true;
	case DMA_INOUT_FORMAT_YUV422:
		en_422 = true;
	case DMA_INOUT_FORMAT_YUV444:
		en_yuv = true;
	default:
		break;
	}

	cstat_hw_s_conv_rgb_2_yuv(base, en_yuv);
	cstat_hw_s_conv_444_2_422(base, en_422);
	cstat_hw_s_conv_422_2_420(base, en_420);
}

int cstat_hw_s_wdma_cfg(void __iomem *base, struct is_common_dma *dma,
		struct cstat_param_set *param, u32 num_buffers, int disable)
{
	int ret = 0;
	struct param_dma_output *dma_out;
	struct cstat_dma_cfg cfg;
	u32 hw_format, hw_order, bit_width, pixel_size, format;
	u32 width, height;
	u32 stride_1p, stride_2p = 0, stride_3p = 0;
	enum format_type dma_type;

	switch (dma->id) {
	case CSTAT_W_RGBYHIST:
		dma_out = &param->rgby;
		cfg.addrs = &param->rbgy_dva;
		break;
	case CSTAT_W_VHIST:
		dma_out = &param->dma_output_svhist;
		cfg.addrs = param->output_dva_svhist;
		break;
	case CSTAT_W_THSTATPRE:
		dma_out = &param->pre;
		cfg.addrs = &param->pre_dva;
		break;
	case CSTAT_W_THSTATAWB:
		dma_out = &param->awb;
		cfg.addrs = &param->awb_dva;
		break;
	case CSTAT_W_THSTATAE:
		dma_out = &param->ae;
		cfg.addrs = &param->ae_dva;
		break;
	case CSTAT_W_DRCGRID:
		dma_out = &param->dma_output_drc;
		cfg.addrs = param->output_dva_drc;
		break;
	case CSTAT_W_LMEDS0:
		dma_out = &param->dma_output_lme_ds0;
		cfg.addrs = param->output_dva_lem_ds0;
		break;
	case CSTAT_W_CDS0:
		dma_out = &param->dma_output_cds;
		cfg.addrs = param->output_dva_cds;
		cstat_hw_s_conv_cfg(base, dma_out->format);
		break;
	case CSTAT_W_FDPIG:
		dma_out = &param->dma_output_fdpig;
		cfg.addrs = param->output_dva_fdpig;
		break;
	case CSTAT_DMA_NONE:
		/* Not existing DMA */
	case CSTAT_W_MEDS0:
	case CSTAT_W_MEDS1:
		/* These will be deprecated. */
		return 0;
	default:
		warn_hw("[CSTAT][%s] NOT supported DMA", dma->name);
		return 0;
	}


	if (disable || dma_out->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		goto skip_dma;

	hw_format = dma_out->format;
	hw_order = dma_out->order;
	bit_width = dma_out->bitwidth;
	pixel_size = dma_out->msb + 1;
	width = dma_out->width;
	height = dma_out->height;
	stride_1p = dma_out->stride_plane0 ? dma_out->stride_plane0 : width;
	stride_2p = dma_out->stride_plane1 ? dma_out->stride_plane1 : width;
	stride_3p = dma_out->stride_plane2 ? dma_out->stride_plane2 : width;

	if (cstat_hw_is_yuv(hw_format, hw_order)) {
		dma_type = DMA_FMT_YUV;
		format = is_hw_dma_get_yuv_format(bit_width, hw_format,
				dma_out->plane, dma_out->order);
	} else if (cstat_hw_is_rgb(hw_order)) {
		dma_type = DMA_FMT_RGB;
		format = is_hw_dma_get_rgb_format(bit_width, dma_out->plane, dma_out->order);
		stride_1p = is_hw_dma_get_img_stride(bit_width, pixel_size, hw_format,
				stride_1p, 16, true);
	} else {
		dma_type = DMA_FMT_BAYER;
		format = is_hw_dma_get_bayer_format(bit_width, pixel_size, hw_format,
				/* sbwc_en */0, /* is_msb */true);
		stride_1p = is_hw_dma_get_img_stride(bit_width, pixel_size, hw_format,
				stride_1p, 16, true);
	}

	if (format < 0)
		goto skip_dma;

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, dma_type);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, stride_2p, stride_3p);
	/* ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info); */
	if (ret)
		goto skip_dma;

	cfg.num_planes = dma_out->plane;
	cfg.num_buffers = num_buffers;
	cfg.buf_idx = 0;

	cstat_hw_s_dma_addrs(dma, &cfg);

	dbg_hw(2, "[CSTAT][%s]dma_cfg: size %dx%d format %d-%d plane %d\n",
			dma->name,
			width, height, dma_type, format, cfg.num_planes);
	dbg_hw(2, "[CSTAT][%s]stride_cfg: img %d\n",
			dma->name,
			stride_1p);
	dbg_hw(2, "[CSTAT][%s]dma_addr: img[0] 0x%lx\n",
			dma->name,
			cfg.addrs[0]);

	CALL_DMA_OPS(dma, dma_enable, 1);

	return 0;

skip_dma:
	dbg_hw(2, "[CSTAT][%s]dma_cfg: OFF\n", dma->name);

	CALL_DMA_OPS(dma, dma_enable, 0);

	return 0;
}

void cstat_hw_s_lic_cmn_cfg(void __iomem *base, struct cstat_lic_lut *lic_lut, u32 ch)
{
	u32 op_mode_val = 0;

	switch (lic_lut->mode) {
	case CSTAT_LIC_STATIC:
		CSTAT_SET_F(base, CSTAT_R_LIC_STATIC_ALLOC_CONFIG2,
				CSTAT_F_LIC_BUFFERSIZE_CONTEXT_0, lic_lut->param[0]);
		CSTAT_SET_F(base, CSTAT_R_LIC_STATIC_ALLOC_CONFIG2,
				CSTAT_F_LIC_BUFFERSIZE_CONTEXT_1, lic_lut->param[1]);
		CSTAT_SET_F(base, CSTAT_R_LIC_STATIC_ALLOC_CONFIG3,
				CSTAT_F_LIC_BUFFERSIZE_CONTEXT_2, lic_lut->param[2]);
		CSTAT_SET_F(base, CSTAT_R_LIC_STATIC_ALLOC_CONFIG3,
				CSTAT_F_LIC_BUFFERSIZE_CONTEXT_3, lic_lut->param[3]);
		break;
	case CSTAT_LIC_SINGLE:
		op_mode_val = CSTAT_SET_V(op_mode_val, CSTAT_F_LIC_SEL_SINGLE_INPUT, ch);
		break;
	case CSTAT_LIC_DYNAMIC:
	default:
		/* Nothing to do */
		break;
	}

	op_mode_val = CSTAT_SET_V(op_mode_val, CSTAT_F_LIC_OPERATION_MODE, lic_lut->mode);
	CSTAT_SET_R(base, CSTAT_R_LIC_OPERATION_MODE, op_mode_val);
}

void cstat_hw_s_lic_cfg(void __iomem *base, struct cstat_lic_cfg *cfg)
{
	u32 limit_input_line_en, max_input_line;
	u32 weight, vl_thres, val;

	/* LIC context configuration */
	if (cfg->mode == CSTAT_LIC_DYNAMIC)
		vl_thres = LIC_VL_PREEMPTION_THRES_DYNAMIC;
	else if (cfg->mode == CSTAT_LIC_STATIC && cfg->input == OTF)
		vl_thres = LIC_VL_PREEMPTION_THRES_STATIC / 3;
	else
		vl_thres = LIC_VL_PREEMPTION_THRES_STATIC;

	if (cfg->input == DMA) {
		limit_input_line_en = 1;
		max_input_line = 1;
		weight = 0;
	} else {
		limit_input_line_en = 0;
		max_input_line = 0;
		weight = LIC_WEIGHT_MAX;
	}

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_LIC_LIMIT_INPUT_LINE_EN_CONTEXT, limit_input_line_en);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_MAX_INPUT_LINE_CONTEXT, max_input_line);
	CSTAT_SET_R(base, CSTAT_R_LIC_DYNAMIC_ALLOC_CONFIG, val);

	CSTAT_SET_F(base, CSTAT_R_LIC_STATIC_ALLOC_CONFIG4,
			CSTAT_F_LIC_WEIGHT_CONTEXT, weight);

	CSTAT_SET_F(base, CSTAT_R_LIC_INPUT_CONFIG1,
			CSTAT_F_LIC_INPUT_HALF_PPC_EN, cfg->half_ppc_en);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_LIC_OUTPUT_HBLANK_CYCLE, HBLANK_CYCLE);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_OUTPUT_DISABLE_MASK, 0);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_OUTPUT_HOLD_AT_COREX_BUSY, 1);
	CSTAT_SET_R(base, CSTAT_R_LIC_OUTPUT_CONFIG, val);

	CSTAT_SET_F(base, CSTAT_R_LIC_VIRTUAL_LINE_CONFIG_0,
			CSTAT_F_LIC_VL_NUM_LINES_IMAGE, LIC_VL_IMAGE);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_LIC_VL_PREEMPTION_EN, 1);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_VL_PREEMPTION_THRESHOLD, vl_thres);
	CSTAT_SET_R(base, CSTAT_R_LIC_VIRTUAL_LINE_CONFIG_1, val);

	/*
	 * LIC debug feature configuration
	 * - ROL condition: Latch timing
	 *   [0]: overflow
	 *   [1]: input column error
	 *   [2]: input row error
	 */
	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_LIC_ROL_ENABLE, 1);
	val = CSTAT_SET_V(val, CSTAT_F_LIC_ROL_CONDITION, 0x7);
	CSTAT_SET_R(base, CSTAT_R_LIC_DEBUG_CONFIG, val);
}

/* It is only valid for context0 */
int cstat_hw_s_sram_offset(void __iomem *base, u32 ch, u32 width)
{
	/* TODO: Support dynamic configuration for each channel. */

	/* Pre BNS - SET A */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A0_PRE_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET0_SETA_PRE_BNS, sram_offset.pre[0]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A0_PRE_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET1_SETA_PRE_BNS, sram_offset.pre[1]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A1_PRE_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET2_SETA_PRE_BNS, sram_offset.pre[2]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A1_PRE_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET3_SETA_PRE_BNS, sram_offset.pre[3]);

	/* Post BNS - SET A */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A0_POST_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET0_SETA_POST_BNS, sram_offset.post[0]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A0_POST_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET1_SETA_POST_BNS, sram_offset.post[1]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A1_POST_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET2_SETA_POST_BNS, sram_offset.post[2]);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_LIC_4CH_BUFFER_CONFIG_A1_POST_BNS,
			CSTAT_F_LINE_BUFFER_OFFSET3_SETA_POST_BNS, sram_offset.post[3]);

	return 0;
}

static void cstat_hw_update_param_post_crop_bns(struct is_crop *crop,
		struct cstat_param_set *p_set)
{
	/* LMEDS0 */
	p_set->dma_output_lme_ds0.dma_crop_width = crop->w;
	p_set->dma_output_lme_ds0.dma_crop_height = crop->h;

	/* DRC_CLCT */
	p_set->dma_output_drc.dma_crop_width = crop->w;
	p_set->dma_output_drc.dma_crop_height = crop->h;

	/* VHIST */
	p_set->dma_output_svhist.dma_crop_width = crop->w;
	p_set->dma_output_svhist.dma_crop_height = crop->h;
}

void cstat_hw_s_crop(void __iomem *base, enum cstat_crop_id id,
		bool en, struct is_crop *crop, struct cstat_param_set *p_set,
		struct cstat_size_cfg *size_cfg)
{
	switch (id) {
	case CSTAT_CROP_IN:
		cstat_hw_s_crop_in(base, en, crop);
		break;
	case CSTAT_CROP_DZOOM:
		cstat_hw_s_crop_dzoom(base, en, crop);
		break;
	case CSTAT_CROP_BNS:
		cstat_hw_s_crop_bns(base, en, crop);
		cstat_hw_update_param_post_crop_bns(crop, p_set);
		break;
	case CSTAT_CROP_MENR:
		cstat_hw_s_crop_menr(base, crop);
		break;
	default:
		break;
	};
}

bool cstat_hw_is_bcrop(enum cstat_crop_id id)
{
	return (id == CSTAT_CROP_IN);
}

void cstat_hw_s_grid_cfg(void __iomem *base, enum cstat_grid_id id,
		struct cstat_grid_cfg *cfg)
{
	switch (id) {
	case CSTAT_GRID_CGRAS:
		cstat_hw_s_grid_cgras(base, cfg);
		break;
	case CSTAT_GRID_LSC:
		cstat_hw_s_grid_lsc(base, cfg);
		break;
	};
}

static void cstat_hw_s_bns_weight(void __iomem *base, u32 fac)
{
	u32 val;

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_0, bns_weights[fac][0]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_1, bns_weights[fac][1]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_2, bns_weights[fac][2]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_3, bns_weights[fac][3]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_X_0, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_4, bns_weights[fac][4]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_5, bns_weights[fac][5]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_6, bns_weights[fac][6]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_7, bns_weights[fac][7]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_X_4, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_8, bns_weights[fac][8]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_9, bns_weights[fac][9]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_X_0_10, bns_weights[fac][10]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_X_8, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_0, bns_weights[fac][0]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_1, bns_weights[fac][1]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_2, bns_weights[fac][2]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_3, bns_weights[fac][3]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_Y_0, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_4, bns_weights[fac][4]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_5, bns_weights[fac][5]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_6, bns_weights[fac][6]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_7, bns_weights[fac][7]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_Y_4, val);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_8, bns_weights[fac][8]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_9, bns_weights[fac][9]);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_WEIGHT_Y_0_10, bns_weights[fac][10]);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_WEIGHT_Y_8, val);
}

void cstat_hw_s_bns_cfg(void __iomem *base,
		struct is_crop *crop,
		struct cstat_size_cfg *size_cfg)
{
	u32 out_w, out_h, bypass, val;
	int i;
	enum cstat_bns_scale_ratio ratio;

	for (i = CSTAT_BNS_X2P0; i >= CSTAT_BNS_X1P0; i--) {
		ratio = i;
		out_w = CSTAT_GET_BNS_OUT_SIZE(crop->w, ratio);
		out_h = CSTAT_GET_BNS_OUT_SIZE(crop->h, ratio);

		if ((out_w >= CSTAT_LMEDS_OUT_MAX_W && out_h >= CSTAT_LMEDS_OUT_MAX_H) ||
				ratio == CSTAT_MIN_BNS_RATIO)
			break;
	}

	dbg_hw(3, "[CSTAT]bns_cfg: %dx%d -> %dx%d (%d)\n",
			crop->w, crop->h,
			out_w, out_h,
			ratio);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_FACTORX, bns_factors[ratio].fac);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_FACTORY, bns_factors[ratio].fac);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_CONFIG, val);

	cstat_hw_s_bns_weight(base, bns_factors[ratio].fac);

	val = 0;
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_NOUTPUTTOTALWIDTH, out_w);
	val = CSTAT_SET_V(val, CSTAT_F_BYR_BNS_NOUTPUTTOTALHEIGHT, out_h);
	CSTAT_SET_R(base, CSTAT_R_BYR_BNS_OUTPUTSIZE, val);

	CSTAT_SET_F(base, CSTAT_R_BYR_BNS_LINEGAP,
			CSTAT_F_BYR_BNS_LINEGAP, BNS_LINEGAP);

	bypass = (ratio == CSTAT_BNS_X1P0) ? 1 : 0;
	CSTAT_SET_F(base, CSTAT_R_BYR_BNS_BYPASS,
			CSTAT_F_BYR_BNS_BYPASS, bypass);

	size_cfg->bns.x = 0;
	size_cfg->bns.y = 0;
	size_cfg->bns.w = crop->w = out_w;
	size_cfg->bns.h = crop->h = out_h;
	size_cfg->bns_r = ratio;
}

int cstat_hw_g_stat_size(u32 sd_id, struct cstat_stat_buf_info *info)
{
	u32 width, height, bpp, num;

	switch (sd_id) {
	case IS_CSTAT_CDAF:
		/* u32 x 211 */
		width = CDAF_STAT_LENGTH;
		height = 1;
		bpp = 32;
		num = 1;
		break;
	case IS_CSTAT_PRE_THUMB:
		/* 98,304 byte */
		width = 98304;
		height = 1;
		bpp = 8;
		num = CSAT_STAT_BUF_NUM;
		break;
	case IS_CSTAT_AE_THUMB:
	case IS_CSTAT_AWB_THUMB:
		/* 65,536 byte */
		width = 65536;
		height = 1;
		bpp = 8;
		num = CSAT_STAT_BUF_NUM;
		break;
	case IS_CSTAT_RGBY_HIST:
		/* 5,120 byte */
		width = 5120;
		height = 1;
		bpp = 8;
		num = CSAT_STAT_BUF_NUM;
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

static void cstat_hw_g_cdaf_stat(void __iomem *base, void *data)
{
	u32 i;
	struct cstat_cdaf *cdaf = (struct cstat_cdaf *)data;
	u32 *dst = (u32 *)cdaf->kva;

	/* Initialize the read access pointer */
	CSTAT_SET_F_DIRECT(base, CSTAT_R_BYR_CDAF_STAT_START_ADD,
			CSTAT_F_BYR_CDAF_STAT_START_ADD, 0);

	for (i = 0; i < CDAF_STAT_LENGTH; i++)
		dst[i] = CSTAT_GET_R(base, CSTAT_R_BYR_CDAF_STAT_ACCESS);

	CSTAT_SET_F_DIRECT(base, CSTAT_R_BYR_CDAF_STAT_ACCESS_END,
			CSTAT_F_BYR_CDAF_STAT_ACCESS_END, 1);
	CSTAT_SET_F_DIRECT(base, CSTAT_R_BYR_CDAF_STAT_ACCESS_END,
			CSTAT_F_BYR_CDAF_STAT_ACCESS_END, 0);

	cdaf->bytes = (CDAF_STAT_LENGTH * 4);
}


int cstat_hw_g_stat_data(void __iomem *base, u32 stat_id, void *data)
{
	void (*func_g_stat)(void __iomem *base, void *stat);

	switch (stat_id) {
	case CSTAT_STAT_CDAF:
		func_g_stat = cstat_hw_g_cdaf_stat;
		break;
	case CSTAT_STAT_EDGE_SCORE:
		/* Not supported */
		return -EINVAL;
	default:
		warn_hw("[CSTAT] Not supported stat_id %d",  stat_id);
		return -EINVAL;
	}

	func_g_stat(base, data);

	return 0;
}

struct param_dma_output *cstat_hw_s_stat_cfg(u32 sd_id, dma_addr_t addr,
		struct cstat_param_set *p_set)
{
	struct param_dma_output *dma_out;

	switch (sd_id) {
	case IS_CSTAT_PRE_THUMB:
		dma_out = &p_set->pre;
		p_set->pre_dva = addr;
		break;
	case IS_CSTAT_AE_THUMB:
		dma_out = &p_set->ae;
		p_set->ae_dva = addr;
		break;
	case IS_CSTAT_AWB_THUMB:
		dma_out = &p_set->awb;
		p_set->awb_dva = addr;
		break;
	case IS_CSTAT_RGBY_HIST:
		dma_out = &p_set->rgby;
		p_set->rbgy_dva = addr;
		break;
	case IS_CSTAT_CDAF:
	default:
		return NULL;
	}

	if (dma_out->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return NULL;

	dma_out->format = DMA_INOUT_FORMAT_BAYER;
	dma_out->order = DMA_INOUT_ORDER_NO;
	dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	dma_out->msb = dma_out->bitwidth - 1;
	dma_out->plane = DMA_INOUT_PLANE_1;

	return dma_out;
}

void cstat_hw_s_dma_cfg(struct cstat_param_set *p_set, struct is_cstat_config *conf)
{
	/* Each formula follows the guide from IQ */
	p_set->pre.cmd = !conf->thstatpre_bypass;
	if (p_set->pre.cmd) {
		p_set->pre.width = conf->pre_grid_w * 2 * 12;
		p_set->pre.height = conf->pre_grid_h;

		if (!p_set->pre.width || !p_set->pre.height) {
			p_set->pre.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid PRE size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->pre.width, p_set->pre.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] pre %dx%d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				conf->pre_grid_w,
				conf->pre_grid_h);
	}

	p_set->awb.cmd = !conf->thstatawb_bypass;
	if (p_set->awb.cmd) {
		p_set->awb.width = conf->awb_grid_w * 2 * 8;
		p_set->awb.height = conf->awb_grid_h;

		if (!p_set->awb.width || !p_set->awb.height) {
			p_set->awb.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid AWB size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->awb.width, p_set->awb.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] awb %dx%d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				conf->awb_grid_w,
				conf->awb_grid_h);
	}

	p_set->ae.cmd = !conf->thstatae_bypass;
	if (p_set->ae.cmd) {
		p_set->ae.width = conf->ae_grid_w * 2 * 8;
		p_set->ae.height = conf->ae_grid_h;

		if (!p_set->ae.width || !p_set->ae.height) {
			p_set->ae.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid AE size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->ae.width, p_set->ae.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] ae %dx%d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				conf->ae_grid_w,
				conf->ae_grid_h);
	}

	p_set->rgby.cmd = !conf->rgbyhist_bypass;
	if (p_set->rgby.cmd) {
		p_set->rgby.width = conf->rgby_bin_num * 4 * conf->rgby_hist_num;
		p_set->rgby.height = 1;

		if (!p_set->rgby.width || !p_set->rgby.height) {
			p_set->rgby.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid RGBY size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->rgby.width, p_set->rgby.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] rgby %d, %d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				conf->rgby_bin_num,
				conf->rgby_hist_num);
	}

	if (p_set->dma_output_lme_ds0.cmd && conf->lmeds_bypass) {
		p_set->dma_output_lme_ds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		warn_hw("[%d][CSTAT][F%d] bypass LMEDS", p_set->instance_id, p_set->fcount);
	} else if (p_set->dma_output_lme_ds0.cmd) {
		p_set->dma_output_lme_ds0.width = conf->lmeds_w;
		p_set->dma_output_lme_ds0.height = conf->lmeds_h;

		if (!p_set->dma_output_lme_ds0.width || !p_set->dma_output_lme_ds0.height) {
			p_set->dma_output_lme_ds0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid LMEDS0 size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->dma_output_lme_ds0.width,
					p_set->dma_output_lme_ds0.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] lmeds %dx%d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				conf->lmeds_w,
				conf->lmeds_h);
	}

	if (p_set->dma_output_drc.cmd && conf->drcclct_bypass) {
		p_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		warn_hw("[%d][CSTAT][F%d] bypass DRCCLCT", p_set->instance_id, p_set->fcount);
	} else if (p_set->dma_output_drc.cmd) {
		p_set->dma_output_drc.width = conf->drc_grid_w * 4;
		p_set->dma_output_drc.height = conf->drc_grid_h;

		if (!p_set->dma_output_drc.width || !p_set->dma_output_drc.height) {
			p_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid DRCCLT size %dx%d", p_set->instance_id, p_set->fcount,
					p_set->dma_output_drc.width,
					p_set->dma_output_drc.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] drc %dx%d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				p_set->dma_output_drc.width,
				p_set->dma_output_drc.height);
	}

	if (p_set->dma_output_svhist.cmd && conf->vhist_bypass) {
		warn_hw("[%d][CSTAT][F%d] bypass VHIST", p_set->instance_id, p_set->fcount);
		p_set->dma_output_svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	} else if (p_set->dma_output_svhist.cmd) {
		p_set->dma_output_svhist.width = 1024;
		p_set->dma_output_svhist.height = conf->vhist_grid_num;

		if (!p_set->dma_output_svhist.height) {
			warn_hw("[%d][CSTAT][F%d] Invalid SVHIST size %d", p_set->instance_id, p_set->fcount,
					p_set->dma_output_svhist.height);
			p_set->dma_output_svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] svhist %d\n",
				p_set->instance_id,
				__func__,
				p_set->fcount,
				p_set->dma_output_svhist.height);
	}
}

void cstat_hw_s_default_blk_cfg(void __iomem *base)
{
	u32 i;

	/* BITMASK */
	for (i = 0; i < ARRAY_SIZE(cstat_bitmask); i++)
		CSTAT_SET_R(base, cstat_bitmask[i].reg_id, (u32)cstat_bitmask[i].val);

	/* MAKE_RGB */
	for (i = 0; i < ARRAY_SIZE(cstat_make_rgb); i++)
		CSTAT_SET_R(base, cstat_make_rgb[i].reg_id, (u32)cstat_make_rgb[i].val);

	/* RGB_TO_YUV_CDS0 */
	for (i = 0; i < ARRAY_SIZE(cstat_rgb_to_yuv_cds0); i++)
		CSTAT_SET_R(base, cstat_rgb_to_yuv_cds0[i].reg_id,
				(u32)cstat_rgb_to_yuv_cds0[i].val);

	/* YUV444_TO_YUV422_CDS0 */
	for (i = 0; i < ARRAY_SIZE(cstat_444_to_422_cds0); i++)
		CSTAT_SET_R(base, cstat_444_to_422_cds0[i].reg_id,
				(u32)cstat_444_to_422_cds0[i].val);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_s_default_blk_cfg);

void cstat_hw_s_pixel_order(void __iomem *base, u32 pixel_order)
{
	CSTAT_SET_R(base, CSTAT_R_BYR_DTP_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_THSTATPRE_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_CGRAS_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_THSTATAWB_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_THSTATAE_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_RGB_RGBYHIST_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_WBG_PIXEL_ORDER, pixel_order);
	CSTAT_SET_R(base, CSTAT_R_BYR_MAKERGB_PIXEL_ORDER, pixel_order);
}

void cstat_hw_dump(void __iomem *base)
{
	info_hw("[CSTAT] REG DUMP (v1.0)\n");

	is_hw_dump_regs(base, cstat_regs, CSTAT_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_dump);

void cstat_hw_print_stall_status(void __iomem *base, int ch)
{
	u32 val;
	u32 cin_op_stat, cin_err_stat;
	u32 cin_col, cin_row, cin_stall, cin_full;
	u32 lic_in_col, lic_in_row;
	u32 lic_ot_col, lic_ot_row;
	u32 lic_full, stall_in, stall_ot;
	u32 lic_stall_cur, lic_stall_prev;

	/* stall out status */
	val = CSTAT_GET_R(base, CSTAT_R_IP_STALL_OUT_STAT);

	info_hw("[CSTAT%d] STALL_OUT_STAT 0x%08x\n", ch, val);

	/* CINFIFO status */
	val = CSTAT_GET_R(base, CSTAT_R_BYR_CINFIFO_STATUS);
	cin_op_stat = CSTAT_GET_V(val, CSTAT_F_BYR_CINFIFO_OP_STATE_MONITOR);
	cin_err_stat = CSTAT_GET_V(val, CSTAT_F_BYR_CINFIFO_ERROR_STATE_MONITOR);

	val = CSTAT_GET_R(base, CSTAT_R_BYR_CINFIFO_INPUT_CNT);
	cin_col = CSTAT_GET_V(val, CSTAT_F_BYR_CINFIFO_COL_CNT);
	cin_row = CSTAT_GET_V(val, CSTAT_F_BYR_CINFIFO_ROW_CNT);

	cin_stall = CSTAT_GET_R(base, CSTAT_R_BYR_CINFIFO_STALL_CNT);
	cin_full = CSTAT_GET_R(base, CSTAT_R_BYR_CINFIFO_FIFO_FULLNESS);

	info_hw("[CSTAT%d] DBG_CIN in %dx%d\n", ch, (cin_col * 8), cin_row);
	info_hw("[CSTAT%d] DBG_CIN op 0x%02x err 0x%02x stall %d full 0x%02x\n", ch,
			cin_op_stat, cin_err_stat, cin_stall, cin_full);

	/* LIC status */
	val = CSTAT_GET_R(base, CSTAT_R_LIC_DEBUG_INPUT);
	lic_in_col = CSTAT_GET_V(val, CSTAT_F_LIC_INPUT_COLUMN_COUNTER);
	lic_in_row = CSTAT_GET_V(val, CSTAT_F_LIC_INPUT_ROW_COUNTER);

	val = CSTAT_GET_R(base, CSTAT_R_LIC_DEBUG_OUTPUT);
	lic_ot_col = CSTAT_GET_V(val, CSTAT_F_LIC_OUTPUT_COLUMN_COUNTER);
	lic_ot_row = CSTAT_GET_V(val, CSTAT_F_LIC_OUTPUT_ROW_COUNTER);

	val = CSTAT_GET_R(base, CSTAT_R_LIC_DEBUG_BUFFER);
	lic_full = CSTAT_GET_V(val, CSTAT_F_LIC_BUFFER_FULLNESS);
	stall_in = CSTAT_GET_V(val, CSTAT_F_LIC_STALL_IN);
	stall_ot = CSTAT_GET_V(val, CSTAT_F_LIC_STALL_OUT);

	lic_stall_cur = CSTAT_GET_R(base, CSTAT_R_LIC_STALL_COUNTER_IMAGE_CURRENT);
	lic_stall_prev = CSTAT_GET_R(base, CSTAT_R_LIC_STALL_COUNTER_IMAGE_PREVIOUS);

	/* col unit: 8 pixels */
	info_hw("[CSTAT%d] DBG_LIC in %dx%d(%d) out %dx%d(%d)\n", ch,
			(lic_in_col * 8), lic_in_row, stall_in,
			(lic_ot_col * 8), lic_ot_row, stall_ot);
	info_hw("[CSTAT%d] DBG_LIC full 0x%03x stall_cnt cur %d prev %d\n", ch,
			lic_full, lic_stall_cur, lic_stall_prev);
}
KUNIT_EXPORT_SYMBOL(cstat_hw_print_stall_status);

u32 cstat_hw_g_reg_cnt(void)
{
	return CSTAT_REG_CNT + (CSTAT_LUT_REG_CNT * CSTAT_LUT_NUM);
}
