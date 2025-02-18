// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_CHAIN_V9_10_H
#define IS_HW_CHAIN_V9_10_H

#include "is-hw-config.h"
#include "is-hw-api-common.h"
#include "is-groupmgr.h"
#include "is-config.h"

enum sysreg_csis_reg_name {
	SYSREG_R_CSIS_MEMCLK,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON0,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON1,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON3,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON4,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON5,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON6,
	SYSREG_R_CSIS_CSIS_PDP_VC_CON0,
	SYSREG_R_CSIS_CSIS_PDP_VC_CON1,
	SYSREG_R_CSIS_CSIS_PDP_VC_CON2,
	SYSREG_R_CSIS_CSIS_PDP_VC_CON3,
	SYSREG_R_CSIS_CSIS_FRAME_ID_EN,
	SYSREG_R_CSIS_CSIS_PDP_SC_PDP3_IN_EN,
	SYSREG_R_CSIS_CSIS_PDP_SC_PDP2_IN_EN,
	SYSREG_R_CSIS_CSIS_PDP_SC_PDP1_IN_EN,
	SYSREG_R_CSIS_CSIS_PDP_SC_PDP0_IN_EN,
	SYSREG_R_CSIS_MIPI_PHY_CON,
	SYSREG_R_CSIS_MIPI_PHY_SEL,
	SYSREG_R_CSIS_CSIS_PDP_SC_CON2,
	SYSREG_CSIS_REG_CNT
};

enum sysreg_csis_reg_field {
	SYSREG_F_CSIS_EN,
	SYSREG_F_CSIS_GLUEMUX_PDP0_VAL,
	SYSREG_F_CSIS_GLUEMUX_PDP1_VAL,
	SYSREG_F_CSIS_GLUEMUX_CSIS_DMA0_OTF_SEL,
	SYSREG_F_CSIS_GLUEMUX_CSIS_DMA1_OTF_SEL,
	SYSREG_F_CSIS_GLUEMUX_CSIS_DMA2_OTF_SEL,
	SYSREG_F_CSIS_GLUEMUX_CSIS_DMA3_OTF_SEL,
	SYSREG_F_CSIS_MUX_IMG_VC_PDP0,
	SYSREG_F_CSIS_MUX_AF_VC_PDP0,
	SYSREG_F_CSIS_MUX_IMG_VC_PDP1,
	SYSREG_F_CSIS_MUX_AF_VC_PDP1,
	SYSREG_F_CSIS_MUX_IMG_VC_PDP2,
	SYSREG_F_CSIS_MUX_AF_VC_PDP2,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS5,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS4,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS3,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS2,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS1,
	SYSREG_F_CSIS_FRAME_ID_EN_CSIS0,
	SYSREG_F_CSIS_PDP2_IN_CSIS5_EN,
	SYSREG_F_CSIS_PDP2_IN_CSIS4_EN,
	SYSREG_F_CSIS_PDP2_IN_CSIS3_EN,
	SYSREG_F_CSIS_PDP2_IN_CSIS2_EN,
	SYSREG_F_CSIS_PDP2_IN_CSIS1_EN,
	SYSREG_F_CSIS_PDP2_IN_CSIS0_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS5_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS4_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS3_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS2_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS1_EN,
	SYSREG_F_CSIS_PDP1_IN_CSIS0_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS5_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS4_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS3_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS2_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS1_EN,
	SYSREG_F_CSIS_PDP0_IN_CSIS0_EN,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S3,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S2,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S1,
	SYSREG_F_CSIS_MIPI_RESETN_DPHY_S,
	SYSREG_F_CSIS_MIPI_RESETN_DCPHY_S1,
	SYSREG_F_CSIS_MIPI_RESETN_DCPHY_S,
	SYSREG_F_CSIS_MIPI_SEPARATION_SEL,
	SYSREG_F_CSIS_GLUEMUX_PDP2_VAL,
	SYSREG_CSIS_REG_FIELD_CNT
};

enum sysreg_taa_reg_name {
	SYSREG_R_TAA_MEMCLK,
	SYSREG_R_TAA_TAA_USER_CON1,
	SYSREG_TAA_REG_CNT
};

enum sysreg_taa_reg_field {
	SYSREG_F_TAA_EN, /* 0x108 */
	SYSREG_F_TAA_GLUEMUX_OTFOUT_SEL,  /* 0x404 */
	SYSREG_TAA_REG_FIELD_CNT
};

enum sysreg_tnr_reg_name {
	SYSREG_R_TNR_MEMCLK,
	SYSREG_R_TNR_TNR_USER_CON0,
	SYSREG_TNR_REG_CNT
};

enum sysreg_tnr_reg_field {
	SYSREG_F_TNR_EN, /* 0x108 */
	SYSREG_F_TNR_SW_RESETN_LHS_AST_GLUE_OTF1_TNRISP,
	SYSREG_F_TNR_TYPE_LHS_AST_GLUE_OTF1_TNRISP,
	SYSREG_F_TNR_EN_OTF_IN_LH_AST_SI_OTF1_TNRISP,
	SYSREG_F_TNR_SW_RESETN_LHS_AST_GLUE_OTF0_TNRISP,
	SYSREG_F_TNR_TYPE_LHS_AST_GLUE_OTF0_TNRISP,
	SYSREG_F_TNR_EN_OTF_IN_LH_AST_SI_OTF0_TNRISP,
	SYSREG_TNR_REG_FIELD_CNT
};

enum sysreg_isp_reg_name {
	SYSREG_R_ISP_MEMCLK,
	SYSREG_R_ISP_ISP_USER_CON3,
	SYSREG_ISP_REG_CNT
};

enum sysreg_isp_reg_field {
	SYSREG_F_ISP_EN, /* 0x108 */
	SYSREG_F_ISP_OTF_SEL, /* 0x400 */
	SYSREG_ISP_REG_FIELD_CNT
};

enum sysreg_mcsc_reg_name {
	SYSREG_R_MCSC_MEMCLK,
	SYSREG_R_MCSC_MCSC_USER_CON2,
	SYSREG_MCSC_REG_CNT
};

enum sysreg_mcsc_reg_field {
	SYSREG_F_MCSC_EN, /* 0x108 */
	SYSREG_F_MCSC_EN_OTF_IN_LH_AST_MI_OTF_ISPMCSC, /* 0x408 */
	SYSREG_MCSC_REG_FIELD_CNT
};

#define GROUP_HW_MAX (GROUP_SLOT_MAX)

#define IORESOURCE_CSIS_DMA	0
#define IORESOURCE_PDP_CORE0	1
#define IORESOURCE_PDP_CORE1	2
#define IORESOURCE_PDP_CORE2	3
#define IORESOURCE_3AA0		4
#define IORESOURCE_3AA1		5
#define IORESOURCE_3AA2		6
#define IORESOURCE_ZSL0_DMA	7
#define IORESOURCE_ZSL1_DMA	8
#define IORESOURCE_ZSL2_DMA	9
#define IORESOURCE_STRP0_DMA	10
#define IORESOURCE_STRP1_DMA	11
#define IORESOURCE_STRP2_DMA	12
#define IORESOURCE_ORBMCH0	13
#define IORESOURCE_ITP		14
#define IORESOURCE_MCFP0	15
#define IORESOURCE_MCFP1	16
#define IORESOURCE_DNS		17
#define IORESOURCE_MCSC		18

#define GROUP_SENSOR_MAX_WIDTH	16376
#define GROUP_SENSOR_MAX_HEIGHT	12248
#define GROUP_PDP_MAX_WIDTH	16376
#define GROUP_PDP_MAX_HEIGHT	12248
#define GROUP_3AA_MAX_WIDTH	16376
#define GROUP_3AA_MAX_HEIGHT	12248
#define GROUP_ISP_MAX_WIDTH	2880
#define GROUP_ISP_MAX_HEIGHT	8192
#define GROUP_ORBMCH_MAX_WIDTH	2016
#define GROUP_ORBMCH_MAX_HEIGHT	1512

/* RTA HEAP: 6MB */
#define IS_RESERVE_LIB_SIZE	(0x00600000)

/* ORBMCH DMA: Moved to user space */
#define TAAISP_ORBMCH_SIZE	(0)

/* DDK DMA: Moved into driver */
#define IS_TAAISP_SIZE		(0)

/* TNR DMA: Moved into driver*/
#define TAAISP_TNR_SIZE		(0)

/* Secure TNR DMA: Moved into driver */
#define TAAISP_TNR_S_SIZE	(0)

/* DDK HEAP: 90MB */
#define IS_HEAP_SIZE		(0x05A00000)

/* Rule checker size for DDK */
#define IS_RCHECKER_SIZE_RO	(SZ_4M + SZ_1M)
#define IS_RCHECKER_SIZE_RW	(SZ_256K)

#define SYSREG_CSIS_BASE_ADDR	(0x15020000)
#define SYSREG_TAA_BASE_ADDR	(0x15520000)
#define SYSREG_TNR_BASE_ADDR	(0x15320000)
#define SYSREG_ISP_BASE_ADDR	(0x15420000)
#define SYSREG_MCSC_BASE_ADDR	(0x15620000)

#define HWFC_INDEX_RESET_ADDR	(0x15641050)

enum ext_chain_id {
	ID_ORBMCH_0 = 0,
	ID_ORBMCH_1 = 1,
};

#define INTR_ID_BASE_OFFSET	(INTR_HWIP_MAX)
#define GET_IRQ_ID(y, x)	(x - (INTR_ID_BASE_OFFSET * y))
#define valid_3aaisp_intr_index(intr_index) \
	(intr_index >= 0 && intr_index < INTR_HWIP_MAX)

/* Specific interrupt map belonged to each IP */
/* ORBMCH */
#define ORB_DESC_BUF_NUM		(2)		/* Previous, Current descriptor */
#define ORB_REGION_NUM			(9)		/* L1 Region (0...7) + L2 Region */
#define ORB_DB_MAX_SIZE		(300)
#define ORB_KEY_DATA_BIT		(128)
#define ORB_DESC_DATA_BIT		(256)
#define ORB_KEY_STRIDE			((ORB_DB_MAX_SIZE * ORB_KEY_DATA_BIT) / BITS_PER_BYTE)
#define ORB_DESC_STRIDE		((ORB_DB_MAX_SIZE * ORB_DESC_DATA_BIT) / BITS_PER_BYTE)
#define ORB_KEY_BUF_SIZE		(ORB_KEY_STRIDE * ORB_REGION_NUM)
#define ORB_DESC_BUF_SIZE		(ORB_DESC_STRIDE * ORB_REGION_NUM)
#define MCH_MAX_MATCH_NUM		(3)
#define MCH_MAX_OUTPUT_SIZE		(4)
#define MCH_OUT_BUF_SIZE		(ORB_DB_MAX_SIZE * MCH_MAX_MATCH_NUM * MCH_MAX_OUTPUT_SIZE * ORB_REGION_NUM)
#define ORBMCH_DVFS_DOMAIN		(IS_DVFS_ISP)
#define ORBMCH_PROC_SW_MARGIN		(10)	/* orbmch processing time sw margin */

/* MC-Scaler */
#define USE_DMA_BUFFER_INDEX		(0) /* 0 ~ 7 */
#define MCSC_OFFSET_ALIGN		(2)
#define MCSC_WIDTH_ALIGN		(2)
#define MCSC_HEIGHT_ALIGN		(2)
#define MCSC_PRECISION			(20)
#define MCSC_POLY_RATIO_UP		(25)
#define MCSC_POLY_QUALITY_RATIO_DOWN	(4)
#define MCSC_POLY_RATIO_DOWN		(16)
#define MCSC_POLY_MAX_RATIO_DOWN	(256)
#define MCSC_POST_RATIO_DOWN		(16)
#define MCSC_POST_MAX_WIDTH		(720)

/* #define MCSC_POST_WA */
/* #define MCSC_POST_WA_SHIFT	(8)*/	/* 256 = 2^8 */
#define MCSC_USE_DEJAG_TUNING_PARAM	(true)
#define MCSC_SETFILE_VERSION		(0x14027435)
#define MCSC_DJAG_ENABLE_SENSOR_BRATIO	(2000)
#define MCSC_LINE_BUF_SIZE		(2880)
#define HWFC_DMA_ID_OFFSET		(8)
#define ENTRY_HF			ENTRY_M5P	/* Subdev ID of MCSC port for High Frequency */
#define CAC_G2_VERSION		1

/* TODO: remove this, compile check only */
/* VRA */
#define VRA_CH1_INTR_CNT_PER_FRAME	(6)

#define CSIS0_QCH_EN_ADDR		(0x15030004)
#define CSIS1_QCH_EN_ADDR		(0x15040004)
#define CSIS2_QCH_EN_ADDR		(0x15050004)
#define CSIS3_QCH_EN_ADDR		(0x15060004)
#define CSIS4_QCH_EN_ADDR		(0x15070004)
#define CSIS5_QCH_EN_ADDR		(0x15080004)

#define ALIGN_UPDOWN_STRIPE_WIDTH(w, align) \
	(w & (align) >> 1 ? ALIGN(w, (align)) : ALIGN_DOWN(w, (align)))

int exynos8825_is_dump_clk(struct device *dev);

#define IS_LLC_CACHE_HINT_SHIFT 4

enum is_llc_cache_hint {
	IS_LLC_CACHE_HINT_INVALID = 0,
	IS_LLC_CACHE_HINT_BYPASS_TYPE,
	IS_LLC_CACHE_HINT_CACHE_ALLOC_TYPE,
	IS_LLC_CACHE_HINT_CACHE_NOALLOC_TYPE,
	IS_LLC_CACHE_HINT_VOTF_TYPE,
	IS_LLC_CACHE_HINT_LAST_BUT_SHARED,
	IS_LLC_CACHE_HINT_NOT_USED_FAR,
	IS_LLC_CACHE_HINT_LAST_ACCESS,
	IS_LLC_CACHE_HINT_MAX
};

enum is_llc_sn {
	IS_LLC_SN_DEFAULT = 0,
	IS_LLC_SN_FHD,
	IS_LLC_SN_UHD,
	IS_LLC_SN_8K,
	IS_LLC_SN_PREVIEW,
	IS_LLC_SN_END,
};

struct is_llc_way_num {
	int votf;
	int mcfp;
};

static ip_video_probe_t is_video_probe_fns[] = {
	/* 3AA */
	is_30s_video_probe,
	is_30c_video_probe,
	is_30p_video_probe,
	is_30f_video_probe,
	is_30g_video_probe,
	is_30o_video_probe,
	is_30l_video_probe,

	is_31s_video_probe,
	is_31c_video_probe,
	is_31p_video_probe,
	is_31f_video_probe,
	is_31g_video_probe,
	is_31o_video_probe,
	is_31l_video_probe,

	is_32s_video_probe,
	is_32c_video_probe,
	is_32p_video_probe,
	is_32f_video_probe,
	is_32g_video_probe,
	is_32o_video_probe,
	is_32l_video_probe,

	is_33s_video_probe,
	is_33c_video_probe,
	is_33p_video_probe,
	is_33f_video_probe,
	is_33g_video_probe,
	is_33o_video_probe,
	is_33l_video_probe,

	/* ISP */
	is_i0s_video_probe,
	is_i0c_video_probe,

	/* TNR */
	is_i0t_video_probe,
	is_i0g_video_probe,
	is_i0v_video_probe,
	is_i0w_video_probe,

	/* ORB */
	is_orb0_video_probe,
	is_orb0c_video_probe,
	is_orb1c_video_probe,

	/* MCSC */
	is_m0s_video_probe,
	is_m1s_video_probe,

	is_m0p_video_probe,
	is_m1p_video_probe,
	is_m2p_video_probe,
	is_m3p_video_probe,
	is_m4p_video_probe,
	is_m5p_video_probe,

	/* YUVPP */
	is_ypp_video_probe,
};

#endif
