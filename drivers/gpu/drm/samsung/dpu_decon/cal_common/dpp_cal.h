/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DPP CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DPP_CAL_H__
#define __SAMSUNG_DPP_CAL_H__

#include <exynos_drm_format.h>
#include <exynos_drm_plane.h>
#include <regs-dpp.h>

#define DPP_CSC_IDX_BT601_625			0
#define DPP_CSC_IDX_BT601_625_UNADJUSTED	2
#define DPP_CSC_IDX_BT601_525			4
#define DPP_CSC_IDX_BT601_525_UNADJUSTED	6
#define DPP_CSC_IDX_BT2020_CONSTANT_LUMINANCE	8
#define DPP_CSC_IDX_BT470M			10
#define DPP_CSC_IDX_FILM			12
#define DPP_CSC_IDX_ADOBE_RGB			14
/* for R-to-Y conversion */
#define DPP_CSC_IDX_BT709			16
#define DPP_CSC_IDX_BT2020			18
#define DPP_CSC_IDX_DCI_P3			20

enum dpp_attr {
	DPP_ATTR_AFBC		= 0,
	DPP_ATTR_SAJC		= 0,
	DPP_ATTR_BLOCK		= 1,
	DPP_ATTR_FLIP		= 2,
	DPP_ATTR_ROT		= 3,
	DPP_ATTR_CSC		= 4,
	DPP_ATTR_SCALE		= 5,
	DPP_ATTR_HDR		= 6,
	DPP_ATTR_C_HDR		= 7,
	DPP_ATTR_C_HDR10_PLUS	= 8,
	DPP_ATTR_WCG            = 9,
	DPP_ATTR_SBWC           = 10,
	DPP_ATTR_HDR10_PLUS	= 11,

	DPP_ATTR_IDMA		= 16,
	DPP_ATTR_ODMA		= 17,
	DPP_ATTR_DPP		= 18,
	DPP_ATTR_SRAMC		= 19,
	DPP_ATTR_HDR_COMM	= 20,
};

enum dpp_csc_defs {
	/* csc_type */
	DPP_CSC_BT_601 = 0,
	DPP_CSC_BT_709 = 1,
	/* csc_range */
	DPP_CSC_NARROW = 0,
	DPP_CSC_WIDE = 1,
	/* csc_mode */
	CSC_COEF_HARDWIRED = 0,
	CSC_COEF_CUSTOMIZED = 1,
	/* csc_id used in csc_3x3_t[] : increase by even value */
	DPP_CSC_ID_BT_2020 = 0,
	DPP_CSC_ID_DCI_P3 = 2,
	CSC_CUSTOMIZED_START = 4,
};

enum dpp_comp_type {
	COMP_TYPE_NONE = 0,
	COMP_TYPE_AFBC,
	COMP_TYPE_SAJC,
	COMP_TYPE_SBWC,		/* lossless */
	COMP_TYPE_SBWCL,	/* lossy */
};

enum dpp_comp_blk_size {
	/* SAJC indepedent block size */
	SAJC_BLK_64B  = 0,
	SAJC_BLK_128B = 1,
	SAJC_BLK_256B = 2,
	/* SBWC blk byte num */
	SBWC_BLK_32x2 = 2,
	SBWC_BLK_32x3 = 3,
	SBWC_BLK_32x4 = 4,
	SBWC_BLK_32x5 = 5,
	SBWC_BLK_32x6 = 6,
};

struct dpp_regs {
	void __iomem *dpp_base_regs;
	void __iomem *dma_base_regs;
	void __iomem *sramc_base_regs;
	void __iomem *votf_base_regs;
	void __iomem *scl_coef_base_regs;
	void __iomem *hdr_comm_base_regs;
	void __iomem *dpp_debug_base_regs;
};

struct decon_frame {
	int x;
	int y;
	u32 w;
	u32 h;
	u32 f_w;
	u32 f_h;
};

struct decon_win_rect {
	int x;
	int y;
	u32 w;
	u32 h;
};

enum dpp_hdr_standard {
	DPP_HDR_OFF = 0,
	DPP_HDR_ST2084,
	DPP_HDR_HLG,
};

#define DPP_X_FLIP		(1 << 0)
#define DPP_Y_FLIP		(1 << 1)
#define DPP_ROT			(1 << 2)
#define DPP_ROT_FLIP_MASK	(DPP_X_FLIP | DPP_Y_FLIP | DPP_ROT)
enum dpp_rotate {
	DPP_ROT_NORMAL	 = 0x0,
	DPP_ROT_XFLIP	 = DPP_X_FLIP,
	DPP_ROT_YFLIP	 = DPP_Y_FLIP,
	DPP_ROT_180	 = DPP_X_FLIP | DPP_Y_FLIP,
	DPP_ROT_90	 = DPP_ROT,
	DPP_ROT_90_XFLIP = DPP_ROT | DPP_X_FLIP,
	DPP_ROT_90_YFLIP = DPP_ROT | DPP_Y_FLIP,
	DPP_ROT_270	 = DPP_ROT | DPP_X_FLIP | DPP_Y_FLIP,
};

enum dpp_reg_area {
	REG_AREA_DPP = 0,
	REG_AREA_DMA,
	REG_AREA_DMA_COM,
};

enum dpp_bpc {
	DPP_BPC_8 = 0,
	DPP_BPC_10,
};

struct dpp_dpuf_resource {
	u8 check;
	u32 sajc;
	u32 sbwc;
	u32 rot;
	u32 scl;
	u32 itp_csc;
	u32 sramc; /* sranm controller count */
	u32 sram_w; /* sram unit (x80bit)  */
	u32 sram; /* total sram count */
};

struct dpp_dpuf_req_rsc {
	u32 sajc;
	u32 sbwc;
	u32 rot;
	u32 scl;
	u32 itp_csc;
	u32 sram;
};

#define MAX_PLANE_ADDR_CNT	4

struct dpp_params_info {
	struct decon_frame src;
	struct decon_frame dst;
	struct decon_win_rect block;
	u32 rot;

	enum dpp_hdr_standard hdr;
	u32 min_luminance;  /* TODO: remove, if can */
	u32 max_luminance;  /* TODO: remove, if can */
	u32 y_hd_y2_stride; /* Luminance header (or Y-2B) stride */
	u32 y_pl_c2_stride; /* Luminance payload (or C-2B) stride */
	u32 c_hd_stride;    /* Chrominance header stride */
	u32 c_pl_stride;    /* Chrominance payload stride */

	bool is_scale;
	bool is_block;
	u32 format;
	dma_addr_t addr[MAX_PLANE_ADDR_CNT];
	u32 dataspace;
	int h_ratio;
	int v_ratio;
	u32 standard;
	u32 transfer;
	u32 range;
	u32 split;
	enum dpp_bpc in_bpc;

	unsigned long rcv_num;
	enum dpp_comp_type comp_type;
	enum dpp_comp_blk_size blk_size;

	/* votf for output */
	bool votf_o_en;
	u32 votf_o_idx;
	u32 votf_o_base_addr;
	u32 votf_o_mfc_addr;

	bool votf_en;
	u32 votf_buf_idx;
	u32 votf_base_addr;
};

void dpp_regs_desc_init(void __iomem *regs, const char *name,
		enum dpp_regs_type type, unsigned int id);

/* DPP CAL APIs exposed to DPP driver */
void dpp_reg_init(u32 id, const unsigned long attr);
int dpp_reg_deinit(u32 id, bool reset, const unsigned long attr);
void dpp_reg_configure_params(u32 id, struct dpp_params_info *p,
		const unsigned long attr);
void dpp_reg_set_hdr_mul_con(u32 id, u32 en);
void idma_reg_set_votf_disable(u32 id);

/* DPU_DMA, DPP DEBUG */
void __dpp_dump(u32 id, struct dpp_regs *regs);
void __dpp_common_dump(u32 fid, struct dpp_regs *regs);

/* DPUF hw resource count init */
void __dpp_init_dpuf_resource_cnt(void);

/* DPP hw limitation check */
int __dpp_check(u32 id, const struct dpp_params_info *p, unsigned long attr);

/* DPU_DMA and DPP interrupt handler */
u32 dpp_reg_get_irq_and_clear(u32 id);
u32 idma_reg_get_irq_and_clear(u32 id);
u32 odma_reg_get_irq_and_clear(u32 id);
void idma_reg_set_irq_enable(u32 id);
void dpp_reg_set_irq_enable(u32 id);
void idma_reg_set_irq_disable(u32 id);
void dpp_reg_set_irq_disable(u32 id);

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr);

void votf_reg_init_trs(u32 fid);
void votf_reg_set_global_init(u32 fid, u32 votf_base_addr, bool en);

#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
void dpp_reg_set_hdrc_dither(u32 id, bool mcd_hdr, enum dpp_bpc bpc);
#endif
#endif /* __SAMSUNG_DPP_CAL_H__ */
