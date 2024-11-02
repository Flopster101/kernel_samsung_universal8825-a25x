/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_CSTAT_H
#define IS_HW_API_CSTAT_H

#include "is-hw-api-common.h"
#include "is-interface-ddk.h"
#include "is-hw-common-dma.h"
#include "is-type.h"

#define CSTAT_COREX_SDW_BIT	15
#define CSTAT_COREX_SDW_OFFSET	(1 << CSTAT_COREX_SDW_BIT)
#define CSTAT_CH_MAX		4
#define CSAT_STAT_BUF_NUM	2

#define CSTAT_USE_COREX_SW_TRIGGER	0

enum cstat_corex_type {
	COREX_IGNORE = 0,
	COREX_COPY = 1,
	COREX_SWAP = 2,
};

enum cstat_corex_trigger {
	HW_TRIGGER = 0,
	SW_TRIGGER = 1,
};

enum cstat_input_path {
	OTF = 1,
	DMA = 2,
	VOTF = 3,
	CSTST_INPUT_PATH_NUM
};

enum cstat_fifo_end_type {
	LAST_DATA = 0,
	VVALID_FALL = 1,
};

enum cstat_int_on_col_row_src {
	CSTAT_CINFIFO = 0,
	CSTAT_BCROP0 = 1,
	CSTAT_BCROP1 = 2,
	CSTAT_BCROP2 = 3,
};

enum cstat_frame_start_type {
	VVALID_ASAP = 0,
	VVALID_RISE = 1,
};

enum cstat_input_bitwidth {
	INPUT_10B = 0,
	INPUT_12B = 1,
	INPUT_14B = 2,
	CSTAT_INPUT_BITWIDTH_NUM
};

enum cstat_grp_mask {
	CSTAT_GRP_1, /* COL_ROW_INT */
	CSTAT_GRP_2, /* RGBYHIST, THSTAT_PRE, THSTAT_AWB, THSTAT_AE, VHIST */
	CSTAT_GRP_3, /* CDAF */
	CSTAT_GRP_4, /* FDPIG_DS, CDS0 */
	CSTAT_GRP_5, /* LME_DS0, LME_DS1 */
	CSTAT_GRP_MASK_NUM
};

enum cstat_int_src {
	CSTAT_INT1,
	CSTAT_INT2,
	CSTAT_INT_NUM
};

enum cstat_int_type {
	CSTAT_INT_PULSE = 0,
	CSTAT_INT_LEVEL = 1,
};

enum cstat_event_type {
	CSTAT_INIT,
	/* INT1 */
	CSTAT_FS,
	CSTAT_LINE,
	CSTAT_FE,
	CSTAT_RGBY,
	CSTAT_COREX_END,
	CSTAT_ERR,
	/* INT2 */
	CSTAT_CDAF,
	CSTAT_THSTAT_PRE,
	CSTAT_THSTAT_AWB,
	CSTAT_THSTAT_AE,
};

enum cstat_dma_id {
	CSTAT_DMA_NONE,
	CSTAT_R_CL,
	CSTAT_R_BYR,
	CSTAT_RDMA_NUM,
	CSTAT_W_RGBYHIST = CSTAT_RDMA_NUM,
	CSTAT_W_VHIST,
	CSTAT_W_THSTATPRE,
	CSTAT_W_THSTATAWB,
	CSTAT_W_THSTATAE,
	CSTAT_W_DRCGRID,
	CSTAT_W_LMEDS0,
	CSTAT_W_LMEDS1,
	CSTAT_W_MEDS0 = CSTAT_W_LMEDS1,
	CSTAT_W_MEDS1,
	CSTAT_W_FDPIG,
	CSTAT_W_CDS0,
	CSTAT_DMA_NUM
};

enum cstat_lic_mode {
	CSTAT_LIC_DYNAMIC = 0,
	CSTAT_LIC_STATIC = 1,
	CSTAT_LIC_SINGLE = 2,
};

enum cstat_crop_id {
	CSTAT_CROP_IN,
	CSTAT_CROP_DZOOM,
	CSTAT_CROP_BNS,
	CSTAT_CROP_MENR,
	CSTAT_CROP_NUM,
};

enum cstat_grid_id {
	CSTAT_GRID_CGRAS,
	CSTAT_GRID_LSC,
};

enum cstat_bns_scale_ratio {
	CSTAT_BNS_X1P0,
	CSTAT_BNS_X1P25,
	CSTAT_BNS_X1P5,
	CSTAT_BNS_X1P75,
	CSTAT_BNS_X2P0,
	CSTAT_BNS_RATIO_NUM,
};

enum cstat_stat_id {
	CSTAT_STAT_CDAF,
	CSTAT_STAT_EDGE_SCORE,
	CSTAT_STAT_ID_NUM,
};

struct cstat_lic_lut {
	enum cstat_lic_mode mode;
	u32 param[CSTAT_CH_MAX]; /* per channel */
};

struct cstat_lic_cfg {
	enum cstat_lic_mode mode;
	enum cstat_input_path input;
	u32 half_ppc_en;
};

struct cstat_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

struct cstat_stat_buf_info {
	u32 w;
	u32 h;
	u32 bpp;
	u32 num;
};

struct cstat_size_cfg {
	struct is_crop bcrop;
	struct is_crop bns;
	enum cstat_bns_scale_ratio bns_r;
	u32 bns_binning;
};

/**
 * struct cstat_time_cfg - CSTAT IP processing time configuration
 * @vvalid:	vvalid time in usec.
 * @req_vvalid:	required vvalid time for CSTAT in usec.
 * @clk:	current IP clock rate in Hz
 */
struct cstat_time_cfg {
	u32	vvalid;
	u32	req_vvalid;
	ulong	clk;
	bool	fro_en;
};

/*
 * CTRL_CONFIG_0/1
 */
void cstat_hw_s_global_enable(void __iomem *base, bool enable, bool fro_en);
int cstat_hw_s_one_shot_enable(void __iomem *base);
void cstat_hw_corex_trigger(void __iomem *base);
int cstat_hw_s_reset(void __iomem *base);
void cstat_hw_s_post_frame_gap(void __iomem *base, u32 gap);
void cstat_hw_s_int_on_col_row(void __iomem *base, bool enable,
		enum cstat_int_on_col_row_src src, u32 col, u32 row);
void cstat_hw_s_freeze_on_col_row(void __iomem *base,
		enum cstat_int_on_col_row_src src, u32 col, u32 row);
void cstat_hw_core_init(void __iomem *base);
void cstat_hw_ctx_init(void __iomem *base);
u32 cstat_hw_g_version(void __iomem *base);

/*
 * CHAIN_CONFIG
 */
int cstat_hw_s_input(void __iomem *base, enum cstat_input_path input);
int cstat_hw_s_format(void __iomem *base, u32 width, u32 height,
		enum cstat_input_bitwidth bitwidth);
void cstat_hw_s_sdc_enable(void __iomem *base, bool enable);

/*
 * FRO
 */
void cstat_hw_s_fro(void __iomem *base, u32 num_buffer, unsigned long grp_mask);

/*
 * COREX
 */
void cstat_hw_s_corex_enable(void __iomem *base, bool enable);
void cstat_hw_s_corex_type(void __iomem *base, u32 type);
void cstat_hw_s_corex_delay(void __iomem *base, struct cstat_time_cfg *time_cfg);

/*
 * CRC
 */
void cstat_hw_s_crc_seed(void __iomem *base, u32 seed);

/*
 * CONTINT
 */
void cstat_hw_s_int_enable(void __iomem *base);
u32 cstat_hw_g_int1_status(void __iomem *base, bool clear, bool fro_en);
u32 cstat_hw_g_int2_status(void __iomem *base, bool clear, bool fro_en);
void cstat_hw_wait_isr_clear(void __iomem *base, bool fro_en);
u32 cstat_hw_is_occurred(ulong state, enum cstat_event_type type);
void cstat_hw_print_err(char *name, u32 instance, u32 fcount, ulong src1, ulong src2);

/*
 * SECU_CTRL
 */
void cstat_hw_s_seqid(void __iomem *base, u32 seqid);

/*
 * DMA
 */
int cstat_hw_create_dma(void __iomem *base, enum cstat_dma_id dma_id,
		struct is_common_dma *dma);
int cstat_hw_s_rdma_cfg(struct is_common_dma *dma,
		struct cstat_param_set *param, u32 num_buffers);
int cstat_hw_s_wdma_cfg(void __iomem *base, struct is_common_dma *dma,
		struct cstat_param_set *param, u32 num_buffers, int disable);

/*
 * LIC
 */
void cstat_hw_s_lic_cmn_cfg(void __iomem *base, struct cstat_lic_lut *lic_lut, u32 ch);
void cstat_hw_s_lic_cfg(void __iomem *base, struct cstat_lic_cfg *cfg);
int cstat_hw_s_sram_offset(void __iomem *base, u32 ch, u32 width);

/*
 * PIXEL_ORDER
 */
void cstat_hw_s_pixel_order(void __iomem *base, u32 pixel_order);

/*
 * CROP_XXX
 */
void cstat_hw_s_crop(void __iomem *base, enum cstat_crop_id id,
		bool en, struct is_crop *crop, struct cstat_param_set *p_set,
		struct cstat_size_cfg *size_cfg);
bool cstat_hw_is_bcrop(enum cstat_crop_id id);

/*
 * CGRAS & LUMA_SHADING
 */
void cstat_hw_s_grid_cfg(void __iomem *base, enum cstat_grid_id id,
		struct cstat_grid_cfg *cfg);

/*
 * BNS
 */
void cstat_hw_s_bns_cfg(void __iomem *base,
		struct is_crop *crop,
		struct cstat_size_cfg *size_cfg);

/*
 * DS
 */
int cstat_hw_s_ds_cfg(void __iomem *base, enum cstat_dma_id dma_id,
		struct cstat_size_cfg *size_cfg,
		struct cstat_param_set *p_set);

/*
 * HW Information APIs
 */
int cstat_hw_g_stat_size(u32 sd_id, struct cstat_stat_buf_info *info);
int cstat_hw_g_stat_data(void __iomem *base, u32 stat_id, void *data);
struct param_dma_output *cstat_hw_s_stat_cfg(u32 sd_id, dma_addr_t addr, struct cstat_param_set *p_set);
void cstat_hw_s_dma_cfg(struct cstat_param_set *p_set, struct is_cstat_config *conf);

/*
 * DEBUG
 */
void cstat_hw_s_default_blk_cfg(void __iomem *base);
void cstat_hw_dump(void __iomem *base);
void cstat_hw_print_stall_status(void __iomem *base, int ch);

u32 cstat_hw_g_reg_cnt(void);

#endif
