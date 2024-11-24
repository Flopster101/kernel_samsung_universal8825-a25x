/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3_a53x_panel.h
 *
 * Header file for S6E3FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_A53X_PANEL_H__
#define __S6E3FC3_A53X_PANEL_H__

#include "../decon_panel.h"
#include "../decon_panel_drv.h"
#include "s6e3fc3.h"
#include "s6e3fc3_a53x.h"
#include "s6e3fc3_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e3fc3_a53x_panel_mdnie.h"
#endif
#include "s6e3fc3_a53x_panel_dimming.h"
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3fc3_a53x_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3fc3_a53x_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e3fc3_a53x_resol.h"

#undef __pn_name__
#define __pn_name__	a53x

#undef __PN_NAME__
#define __PN_NAME__

/* ===================================================================================== */
/* ============================= [S6E3FC3 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FC3 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FC3 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a53x_brt_table[S6E3FC3_TOTAL_STEP][2] = {
	{ 0x00, 0x02 }, { 0x00, 0x03 }, { 0x00, 0x04 }, { 0x00, 0x05 }, { 0x00, 0x07 },
	{ 0x00, 0x08 }, { 0x00, 0x0A }, { 0x00, 0x0C }, { 0x00, 0x0E }, { 0x00, 0x10 },
	{ 0x00, 0x12 }, { 0x00, 0x14 }, { 0x00, 0x16 }, { 0x00, 0x18 }, { 0x00, 0x1B },
	{ 0x00, 0x1D }, { 0x00, 0x1F }, { 0x00, 0x22 }, { 0x00, 0x24 }, { 0x00, 0x27 },
	{ 0x00, 0x29 }, { 0x00, 0x2C }, { 0x00, 0x2E }, { 0x00, 0x31 }, { 0x00, 0x34 },
	{ 0x00, 0x36 }, { 0x00, 0x39 }, { 0x00, 0x3C }, { 0x00, 0x3F }, { 0x00, 0x41 },
	{ 0x00, 0x44 }, { 0x00, 0x47 }, { 0x00, 0x4A }, { 0x00, 0x4D }, { 0x00, 0x50 },
	{ 0x00, 0x53 }, { 0x00, 0x56 }, { 0x00, 0x59 }, { 0x00, 0x5C }, { 0x00, 0x5F },
	{ 0x00, 0x62 }, { 0x00, 0x66 }, { 0x00, 0x69 }, { 0x00, 0x6C }, { 0x00, 0x6F },
	{ 0x00, 0x72 }, { 0x00, 0x76 }, { 0x00, 0x79 }, { 0x00, 0x7C }, { 0x00, 0x80 },
	{ 0x00, 0x83 }, { 0x00, 0x86 }, { 0x00, 0x8A }, { 0x00, 0x8D }, { 0x00, 0x90 },
	{ 0x00, 0x94 }, { 0x00, 0x97 }, { 0x00, 0x9B }, { 0x00, 0x9E }, { 0x00, 0xA2 },
	{ 0x00, 0xA5 }, { 0x00, 0xA9 }, { 0x00, 0xAC }, { 0x00, 0xB0 }, { 0x00, 0xB4 },
	{ 0x00, 0xB7 }, { 0x00, 0xBB }, { 0x00, 0xBF }, { 0x00, 0xC2 }, { 0x00, 0xC6 },
	{ 0x00, 0xCA }, { 0x00, 0xCD }, { 0x00, 0xD1 }, { 0x00, 0xD5 }, { 0x00, 0xD9 },
	{ 0x00, 0xDC }, { 0x00, 0xE0 }, { 0x00, 0xE4 }, { 0x00, 0xE8 }, { 0x00, 0xEC },
	{ 0x00, 0xEF }, { 0x00, 0xF3 }, { 0x00, 0xF7 }, { 0x00, 0xFB }, { 0x00, 0xFF },
	{ 0x01, 0x03 }, { 0x01, 0x07 }, { 0x01, 0x0B }, { 0x01, 0x0F }, { 0x01, 0x13 },
	{ 0x01, 0x17 }, { 0x01, 0x1B }, { 0x01, 0x1F }, { 0x01, 0x23 }, { 0x01, 0x27 },
	{ 0x01, 0x2B }, { 0x01, 0x2F }, { 0x01, 0x33 }, { 0x01, 0x37 }, { 0x01, 0x3B },
	{ 0x01, 0x3F }, { 0x01, 0x43 }, { 0x01, 0x48 }, { 0x01, 0x4C }, { 0x01, 0x50 },
	{ 0x01, 0x54 }, { 0x01, 0x58 }, { 0x01, 0x5D }, { 0x01, 0x61 }, { 0x01, 0x65 },
	{ 0x01, 0x69 }, { 0x01, 0x6D }, { 0x01, 0x72 }, { 0x01, 0x76 }, { 0x01, 0x7A },
	{ 0x01, 0x7F }, { 0x01, 0x83 }, { 0x01, 0x87 }, { 0x01, 0x8C }, { 0x01, 0x90 },
	{ 0x01, 0x94 }, { 0x01, 0x99 }, { 0x01, 0x9D }, { 0x01, 0xA1 }, { 0x01, 0xA6 },
	{ 0x01, 0xAA }, { 0x01, 0xAF }, { 0x01, 0xB3 }, { 0x01, 0xB7 }, { 0x01, 0xBC },
	{ 0x01, 0xC0 }, { 0x01, 0xC5 }, { 0x01, 0xC9 }, { 0x01, 0xCE }, { 0x01, 0xD2 },
	{ 0x01, 0xD6 }, { 0x01, 0xDB }, { 0x01, 0xDF }, { 0x01, 0xE3 }, { 0x01, 0xE7 },
	{ 0x01, 0xEB }, { 0x01, 0xEF }, { 0x01, 0xF4 }, { 0x01, 0xF8 }, { 0x01, 0xFC },
	{ 0x02, 0x00 }, { 0x02, 0x04 }, { 0x02, 0x09 }, { 0x02, 0x0D }, { 0x02, 0x11 },
	{ 0x02, 0x15 }, { 0x02, 0x1A }, { 0x02, 0x1E }, { 0x02, 0x22 }, { 0x02, 0x27 },
	{ 0x02, 0x2B }, { 0x02, 0x2F }, { 0x02, 0x34 }, { 0x02, 0x38 }, { 0x02, 0x3C },
	{ 0x02, 0x41 }, { 0x02, 0x45 }, { 0x02, 0x49 }, { 0x02, 0x4E }, { 0x02, 0x52 },
	{ 0x02, 0x56 }, { 0x02, 0x5B }, { 0x02, 0x5F }, { 0x02, 0x64 }, { 0x02, 0x68 },
	{ 0x02, 0x6D }, { 0x02, 0x71 }, { 0x02, 0x75 }, { 0x02, 0x7A }, { 0x02, 0x7E },
	{ 0x02, 0x83 }, { 0x02, 0x87 }, { 0x02, 0x8C }, { 0x02, 0x90 }, { 0x02, 0x95 },
	{ 0x02, 0x99 }, { 0x02, 0x9E }, { 0x02, 0xA2 }, { 0x02, 0xA7 }, { 0x02, 0xAB },
	{ 0x02, 0xB0 }, { 0x02, 0xB4 }, { 0x02, 0xB9 }, { 0x02, 0xBE }, { 0x02, 0xC2 },
	{ 0x02, 0xC7 }, { 0x02, 0xCB }, { 0x02, 0xD0 }, { 0x02, 0xD5 }, { 0x02, 0xD9 },
	{ 0x02, 0xDE }, { 0x02, 0xE2 }, { 0x02, 0xE7 }, { 0x02, 0xEC }, { 0x02, 0xF0 },
	{ 0x02, 0xF5 }, { 0x02, 0xFA }, { 0x02, 0xFE }, { 0x03, 0x03 }, { 0x03, 0x08 },
	{ 0x03, 0x0C }, { 0x03, 0x11 }, { 0x03, 0x16 }, { 0x03, 0x1A }, { 0x03, 0x1F },
	{ 0x03, 0x24 }, { 0x03, 0x29 }, { 0x03, 0x2D }, { 0x03, 0x32 }, { 0x03, 0x37 },
	{ 0x03, 0x3C }, { 0x03, 0x40 }, { 0x03, 0x45 }, { 0x03, 0x4A }, { 0x03, 0x4F },
	{ 0x03, 0x53 }, { 0x03, 0x58 }, { 0x03, 0x5D }, { 0x03, 0x62 }, { 0x03, 0x67 },
	{ 0x03, 0x6B }, { 0x03, 0x70 }, { 0x03, 0x75 }, { 0x03, 0x7A }, { 0x03, 0x7F },
	{ 0x03, 0x84 }, { 0x03, 0x89 }, { 0x03, 0x8D }, { 0x03, 0x92 }, { 0x03, 0x97 },
	{ 0x03, 0x9C }, { 0x03, 0xA1 }, { 0x03, 0xA6 }, { 0x03, 0xAB }, { 0x03, 0xB0 },
	{ 0x03, 0xB5 }, { 0x03, 0xB9 }, { 0x03, 0xBE }, { 0x03, 0xC3 }, { 0x03, 0xC8 },
	{ 0x03, 0xCD }, { 0x03, 0xD2 }, { 0x03, 0xD7 }, { 0x03, 0xDC }, { 0x03, 0xE1 },
	{ 0x03, 0xE6 }, { 0x03, 0xEB }, { 0x03, 0xF0 }, { 0x03, 0xF5 }, { 0x03, 0xFA },
	{ 0x03, 0xFF },
	{ 0x00, 0x3A }, { 0x00, 0x3C }, { 0x00, 0x3E }, { 0x00, 0x40 }, { 0x00, 0x42 },
	{ 0x00, 0x44 }, { 0x00, 0x46 }, { 0x00, 0x48 }, { 0x00, 0x4A }, { 0x00, 0x4C },
	{ 0x00, 0x4E }, { 0x00, 0x50 }, { 0x00, 0x53 }, { 0x00, 0x55 }, { 0x00, 0x57 },
	{ 0x00, 0x59 }, { 0x00, 0x5B }, { 0x00, 0x5D }, { 0x00, 0x5F }, { 0x00, 0x61 },
	{ 0x00, 0x63 }, { 0x00, 0x65 }, { 0x00, 0x67 }, { 0x00, 0x69 }, { 0x00, 0x6B },
	{ 0x00, 0x6D }, { 0x00, 0x6F }, { 0x00, 0x71 }, { 0x00, 0x73 }, { 0x00, 0x75 },
	{ 0x00, 0x77 }, { 0x00, 0x79 }, { 0x00, 0x7B }, { 0x00, 0x7D }, { 0x00, 0x7F },
	{ 0x00, 0x82 }, { 0x00, 0x84 }, { 0x00, 0x86 }, { 0x00, 0x88 }, { 0x00, 0x8A },
	{ 0x00, 0x8C }, { 0x00, 0x8E }, { 0x00, 0x90 }, { 0x00, 0x92 }, { 0x00, 0x94 },
	{ 0x00, 0x96 }, { 0x00, 0x98 }, { 0x00, 0x9A }, { 0x00, 0x9C }, { 0x00, 0x9E },
	{ 0x00, 0xA0 }, { 0x00, 0xA2 }, { 0x00, 0xA4 }, { 0x00, 0xA6 }, { 0x00, 0xA8 },
	{ 0x00, 0xAA }, { 0x00, 0xAC }, { 0x00, 0xAE }, { 0x00, 0xB1 }, { 0x00, 0xB3 },
	{ 0x00, 0xB5 }, { 0x00, 0xB7 }, { 0x00, 0xB9 }, { 0x00, 0xBB }, { 0x00, 0xBD },
	{ 0x00, 0xBF }, { 0x00, 0xC1 }, { 0x00, 0xC3 }, { 0x00, 0xC5 }, { 0x00, 0xC7 },
	{ 0x00, 0xC9 }, { 0x00, 0xCB }, { 0x00, 0xCD }, { 0x00, 0xCF }, { 0x00, 0xD1 },
	{ 0x00, 0xD3 }, { 0x00, 0xD5 }, { 0x00, 0xD7 }, { 0x00, 0xD9 }, { 0x00, 0xDB },
	{ 0x00, 0xDD }, { 0x00, 0xE0 }, { 0x00, 0xE2 }, { 0x00, 0xE4 }, { 0x00, 0xE6 },
	{ 0x00, 0xE8 }, { 0x00, 0xEA }, { 0x00, 0xEC }, { 0x00, 0xEE }, { 0x00, 0xF0 },
	{ 0x00, 0xF2 }, { 0x00, 0xF4 }, { 0x00, 0xF6 }, { 0x00, 0xF8 }, { 0x00, 0xFA },
	{ 0x00, 0xFC }, { 0x00, 0xFE }, { 0x01, 0x00 }, { 0x01, 0x02 }, { 0x01, 0x04 },
	{ 0x01, 0x06 }, { 0x01, 0x08 }, { 0x01, 0x0A }, { 0x01, 0x0C }, { 0x01, 0x0F },
	{ 0x01, 0x11 }, { 0x01, 0x13 }, { 0x01, 0x15 }, { 0x01, 0x17 }, { 0x01, 0x19 },
	{ 0x01, 0x1B }, { 0x01, 0x1D }, { 0x01, 0x1F }, { 0x01, 0x21 }, { 0x01, 0x23 },
	{ 0x01, 0x25 }, { 0x01, 0x27 }, { 0x01, 0x29 }, { 0x01, 0x2B }, { 0x01, 0x2D },
	{ 0x01, 0x2F }, { 0x01, 0x31 }, { 0x01, 0x33 }, { 0x01, 0x35 }, { 0x01, 0x37 },
	{ 0x01, 0x39 }, { 0x01, 0x3B }, { 0x01, 0x3E }, { 0x01, 0x40 }, { 0x01, 0x42 },
	{ 0x01, 0x44 }, { 0x01, 0x46 }, { 0x01, 0x48 }, { 0x01, 0x4A }, { 0x01, 0x4C },
	{ 0x01, 0x4E }, { 0x01, 0x50 }, { 0x01, 0x52 }, { 0x01, 0x54 }, { 0x01, 0x56 },
	{ 0x01, 0x58 }, { 0x01, 0x5A }, { 0x01, 0x5C }, { 0x01, 0x5E }, { 0x01, 0x60 },
	{ 0x01, 0x62 }, { 0x01, 0x64 }, { 0x01, 0x66 }, { 0x01, 0x68 }, { 0x01, 0x6A },
	{ 0x01, 0x6D }, { 0x01, 0x6F }, { 0x01, 0x71 }, { 0x01, 0x73 }, { 0x01, 0x75 },
	{ 0x01, 0x77 }, { 0x01, 0x79 }, { 0x01, 0x7B }, { 0x01, 0x7D }, { 0x01, 0x7F },
	{ 0x01, 0x81 }, { 0x01, 0x83 }, { 0x01, 0x85 }, { 0x01, 0x87 }, { 0x01, 0x89 },
	{ 0x01, 0x8B }, { 0x01, 0x8D }, { 0x01, 0x8F }, { 0x01, 0x91 }, { 0x01, 0x93 },
	{ 0x01, 0x95 }, { 0x01, 0x97 }, { 0x01, 0x99 }, { 0x01, 0x9C }, { 0x01, 0x9E },
	{ 0x01, 0xA0 }, { 0x01, 0xA2 }, { 0x01, 0xA4 }, { 0x01, 0xA6 }, { 0x01, 0xA8 },
	{ 0x01, 0xAA }, { 0x01, 0xAC }, { 0x01, 0xAE }, { 0x01, 0xB0 }, { 0x01, 0xB2 },
	{ 0x01, 0xB4 }, { 0x01, 0xB6 }, { 0x01, 0xB8 }, { 0x01, 0xBA }, { 0x01, 0xBC },
	{ 0x01, 0xBE }, { 0x01, 0xC0 }, { 0x01, 0xC2 }, { 0x01, 0xC4 }, { 0x01, 0xC6 },
	{ 0x01, 0xC8 }, { 0x01, 0xCB }, { 0x01, 0xCD }, { 0x01, 0xCF }, { 0x01, 0xD1 },
	{ 0x01, 0xD3 }, { 0x01, 0xD5 }, { 0x01, 0xD7 }, { 0x01, 0xD9 }, { 0x01, 0xDB },
	{ 0x01, 0xDD }, { 0x01, 0xDF }, { 0x01, 0xE1 }, { 0x01, 0xE3 }, { 0x01, 0xE5 },
	{ 0x01, 0xE7 }, { 0x01, 0xE9 }, { 0x01, 0xEB }, { 0x01, 0xED }, { 0x01, 0xEF },
	{ 0x01, 0xF1 }, { 0x01, 0xF3 }, { 0x01, 0xF5 }, { 0x01, 0xF7 }, { 0x01, 0xFA },
	{ 0x01, 0xFC }, { 0x01, 0xFE }, { 0x02, 0x00 }, { 0x02, 0x02 }, { 0x02, 0x04 },
	{ 0x02, 0x06 }, { 0x02, 0x08 }, { 0x02, 0x0A }, { 0x02, 0x0C }, { 0x02, 0x0E },
	{ 0x02, 0x10 },
};

static u8 a53x_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
	/* HBM off */
	{
		/* Normal */
		{ 0x20 },
		/* Smooth */
		{ 0x28 },
	},
	/* HBM on */
	{
		/* Normal */
		{ 0xE0 },
		/* Smooth */
		{ 0xE0 }, /* No smooth dimming in HBM */
	}
};

static u8 a53x_acl_frame_avg_table[][1] = {
	{ 0x44 }, /* 16 Frame Avg */
	{ 0x55 }, /* 32 Frame Avg */
};

static u8 a53x_acl_start_point_table[][2] = {
	{ 0x00, 0xB0 }, /* 50 Percent */
	{ 0x40, 0x28 }, /* 60 Percent */
};

static u8 a53x_acl_dim_speed_table[MAX_S6E3FC3_ACL_DIM][1] = {
	[S6E3FC3_ACL_DIM_OFF] = { 0x00 }, /* 0x00 : ACL Dimming Off */
	[S6E3FC3_ACL_DIM_ON] = { 0x20 }, /* 0x20 : ACL Dimming 32 Frames */
};

static u8 a53x_acl_opr_table[MAX_S6E3FC3_ACL_OPR][1] = {
	[S6E3FC3_ACL_OPR_0] = { 0x00 }, /* ACL OFF, OPR 0% */
	[S6E3FC3_ACL_OPR_1] = { 0x01 }, /* ACL ON, OPR 8% */
};

static u8 a53x_lpm_nit_table[4][1] = {
	/* LPM 2NIT: */
	{ 0x27 },
	/* LPM 10NIT */
	{ 0x26 },
	/* LPM 30NIT */
	{ 0x25  },
	/* LPM 60NIT */
	{ 0x24 },
};

static u8 a53x_lpm_on_table[2][1] = {
	[ALPM_MODE] = { 0x23 },
	[HLPM_MODE] = { 0x23 },
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 a53x_vgh_table[][1] = {
	{ 0xC0 },	/* off 7.0 V */
	{ 0x60 },	/* on 6.2 V */
};
#endif

static u8 a53x_ffc_table[MAX_S6E3FC3_HS_CLK][2] = {
	[S6E3FC3_HS_CLK_1108] = {0x53, 0xC7}, // FFC for HS: 1108
	[S6E3FC3_HS_CLK_1124] = {0x52, 0x96}, // FFC for HS: 1124
	[S6E3FC3_HS_CLK_1125] = {0x52, 0x83}, // FFC for HS: 1125
};

static u8 a53x_fps_table_1[][1] = {
	[S6E3FC3_VRR_FPS_120] = { 0x08 },
	[S6E3FC3_VRR_FPS_60] = { 0x00 },
};

static u8 a53x_fps_table_2[][1] = {
	[S6E3FC3_VRR_FPS_120] = { 0x02 },
	[S6E3FC3_VRR_FPS_60] = { 0x22 },
};

static u8 a53x_dimming_speep_table_1[][1] = {
	[S6E3FC3_SMOOTH_DIMMING_OFF] = { 0x20 },
	[S6E3FC3_SMOOTH_DIMMING_ON] = { 0x60 },
};

static struct maptbl a53x_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(a53x_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(a53x_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),

	[ACL_FRAME_AVG_MAPTBL] = DEFINE_2D_MAPTBL(a53x_acl_frame_avg_table, init_common_table, getidx_acl_onoff_table, copy_common_maptbl),
	[ACL_START_POINT_MAPTBL] = DEFINE_2D_MAPTBL(a53x_acl_start_point_table, init_common_table, getidx_hbm_onoff_table, copy_common_maptbl),
	[ACL_DIM_SPEED_MAPTBL] = DEFINE_2D_MAPTBL(a53x_acl_dim_speed_table, init_common_table, getidx_acl_dim_onoff_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a53x_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),

	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a53x_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(a53x_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(a53x_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
	[FPS_MAPTBL_1] = DEFINE_2D_MAPTBL(a53x_fps_table_1, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[FPS_MAPTBL_2] = DEFINE_2D_MAPTBL(a53x_fps_table_2, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[DIMMING_SPEED] = DEFINE_2D_MAPTBL(a53x_dimming_speep_table_1, init_common_table, getidx_smooth_transition_table, copy_common_maptbl),
	[SET_FFC_MAPTBL] = DEFINE_2D_MAPTBL(a53x_ffc_table, init_common_table, s6e3fc3_getidx_ffc_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E3FC3 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A53X_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A53X_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A53X_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 A53X_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A53X_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A53X_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A53X_SLEEP_OUT[] = { 0x11 };
static u8 A53X_SLEEP_IN[] = { 0x10 };
static u8 A53X_DISPLAY_OFF[] = { 0x28 };
static u8 A53X_DISPLAY_ON[] = { 0x29 };

static u8 A53X_MULTI_CMD_ENABLE[] = { 0x72, 0x2C, 0x21 };
static u8 A53X_MULTI_CMD_DISABLE[] = { 0x72, 0x2C, 0x01 };
static u8 A53X_MULTI_CMD_DUMMY[] = { 0x0A, 0x00 };

static u8 A53X_TE_ON[] = { 0x35, 0x00, 0x00 };
static u8 A53X_TE_OFFSET[] = { 0xB9, 0x01, 0x09, 0x58, 0x00, 0x0B};

static DEFINE_STATIC_PACKET(a53x_level1_key_enable, DSI_PKT_TYPE_WR, A53X_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a53x_level2_key_enable, DSI_PKT_TYPE_WR, A53X_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a53x_level3_key_enable, DSI_PKT_TYPE_WR, A53X_KEY3_ENABLE, 0);

static DEFINE_STATIC_PACKET(a53x_level1_key_disable, DSI_PKT_TYPE_WR, A53X_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a53x_level2_key_disable, DSI_PKT_TYPE_WR, A53X_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a53x_level3_key_disable, DSI_PKT_TYPE_WR, A53X_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a53x_multi_cmd_enable, DSI_PKT_TYPE_WR, A53X_MULTI_CMD_ENABLE, 0);
static DEFINE_STATIC_PACKET(a53x_multi_cmd_disable, DSI_PKT_TYPE_WR, A53X_MULTI_CMD_DISABLE, 0);
static DEFINE_STATIC_PACKET(a53x_multi_cmd_dummy, DSI_PKT_TYPE_WR, A53X_MULTI_CMD_DUMMY, 0);

static DEFINE_STATIC_PACKET(a53x_sleep_out, DSI_PKT_TYPE_WR, A53X_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a53x_sleep_in, DSI_PKT_TYPE_WR, A53X_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a53x_display_on, DSI_PKT_TYPE_WR, A53X_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a53x_display_off, DSI_PKT_TYPE_WR, A53X_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(a53x_te_offset, DSI_PKT_TYPE_WR, A53X_TE_OFFSET, 0);
static DEFINE_STATIC_PACKET(a53x_te_on, DSI_PKT_TYPE_WR, A53X_TE_ON, 0);

static u8 A53X_TSET_SET[] = {
	0xB5,
	0x00,
};
static DEFINE_PKTUI(a53x_tset_set, &a53x_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_tset_set, DSI_PKT_TYPE_WR, A53X_TSET_SET, 0x00);

static u8 A53X_SEED_CRC_ON[] = { 0x6A, 0x01 };
static DEFINE_STATIC_PACKET(a53x_seed_crc_on, DSI_PKT_TYPE_WR, A53X_SEED_CRC_ON, 0x06);

static u8 A53X_AOD_VAINT[] = { 0xF4, 0x28 };
static DEFINE_STATIC_PACKET(a53x_aod_vaint, DSI_PKT_TYPE_WR, A53X_AOD_VAINT, 0x4C);

static u8 A53X_AOD_SETTING[] = { 0x91, 0x01, 0x01 };
static DEFINE_STATIC_PACKET(a53x_aod_setting, DSI_PKT_TYPE_WR, A53X_AOD_SETTING, 0x0);

static u8 A53X_LPM_PORCH_ON[] = { 0xF2, 0x24, 0xA4 };
static DEFINE_STATIC_PACKET(a53x_lpm_porch_on, DSI_PKT_TYPE_WR, A53X_LPM_PORCH_ON, 0x10);

static u8 A53X_SWIRE_NO_PULSE[] = { 0xB5, 0x00 };
static DEFINE_STATIC_PACKET(a53x_swire_no_pulse, DSI_PKT_TYPE_WR, A53X_SWIRE_NO_PULSE, 0x0A);

static u8 A53X_SWIRE_NO_PULSE_LPM_OFF[] = { 0xB5, 0x00, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a53x_swire_no_pulse_lpm_off, DSI_PKT_TYPE_WR, A53X_SWIRE_NO_PULSE_LPM_OFF, 0x07);

static u8 A52_LPM_ON_HFP_1[] = {
	0xCB,
	0x40, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x40
};
static DEFINE_STATIC_PACKET(a52_lpm_on_hfp_1, DSI_PKT_TYPE_WR, A52_LPM_ON_HFP_1, 0x188);

static u8 A52_LPM_ON_HFP_2[] = {
	0xCB,
	0x38, 0x00, 0x00, 0x00, 0x18, 0x03, 0x38
};
static DEFINE_STATIC_PACKET(a52_lpm_on_hfp_2, DSI_PKT_TYPE_WR, A52_LPM_ON_HFP_2, 0x1C0);

static u8 A52_LPM_OFF_HFP_1[] = {
	0xCB,
	0x45, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x45
};
static DEFINE_STATIC_PACKET(a52_lpm_off_hfp_1, DSI_PKT_TYPE_WR, A52_LPM_OFF_HFP_1, 0x188);

static u8 A52_LPM_OFF_HFP_2[] = {
	0xCB,
	0x3D, 0x00, 0x00, 0x00, 0x18, 0x03, 0x3D
};
static DEFINE_STATIC_PACKET(a52_lpm_off_hfp_2, DSI_PKT_TYPE_WR, A52_LPM_OFF_HFP_2, 0x1C0);

static u8 A53X_LPM_OFF_SYNC_CTRL[] = {
	0x63, 0x20
};
static DEFINE_STATIC_PACKET(a53x_lpm_off_sync_ctrl, DSI_PKT_TYPE_WR, A53X_LPM_OFF_SYNC_CTRL, 0x91);

static u8 A53X_LPM_PORCH_OFF[] = { 0xF2, 0x26, 0xE4 };
static DEFINE_STATIC_PACKET(a53x_lpm_porch_off, DSI_PKT_TYPE_WR, A53X_LPM_PORCH_OFF, 0x10);

static u8 A53X_HW_CODE[] = { 0xF2, 0x26, 0xF0 };
static DEFINE_STATIC_PACKET(a53x_hw_code, DSI_PKT_TYPE_WR, A53X_HW_CODE, 0x10);

static u8 A53X_NORMAL_SETTING[] = { 0x91, 0x02, 0x01 };
static DEFINE_STATIC_PACKET(a53x_normal_setting, DSI_PKT_TYPE_WR, A53X_NORMAL_SETTING, 0x00);

static u8 A53X_LPM_NIT[] = { 0x53, 0x27 };
static DEFINE_PKTUI(a53x_lpm_nit, &a53x_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_lpm_nit, DSI_PKT_TYPE_WR, A53X_LPM_NIT, 0x00);

#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
static u8 A53X_DECODER_TEST_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static DEFINE_STATIC_PACKET(a53x_decoder_test_caset, DSI_PKT_TYPE_WR, A53X_DECODER_TEST_CASET, 0x00);

static u8 A53X_DECODER_TEST_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };
static DEFINE_STATIC_PACKET(a53x_decoder_test_paset, DSI_PKT_TYPE_WR, A53X_DECODER_TEST_PASET, 0x00);

static u8 A53X_DECODER_TEST_2C[] = { 0x2C, 0x00 };
static DEFINE_STATIC_PACKET(a53x_decoder_test_2c, DSI_PKT_TYPE_WR, A53X_DECODER_TEST_2C, 0x00);

static u8 A53X_DECODER_CRC_PATTERN_ENABLE[] = { 0xBE, 0x07 };
static DEFINE_STATIC_PACKET(a53x_decoder_crc_pattern_enable, DSI_PKT_TYPE_WR, A53X_DECODER_CRC_PATTERN_ENABLE, 0x00);

static u8 A53X_DECODER_READ_SET_1[] = { 0xD8, 0x11 };
static DEFINE_STATIC_PACKET(a53x_decoder_read_set_1, DSI_PKT_TYPE_WR, A53X_DECODER_READ_SET_1, 0x27);

static u8 A53X_DECODER_READ_SET_2[] = { 0xD8, 0x20 };
static DEFINE_STATIC_PACKET(a53x_decoder_read_set_2, DSI_PKT_TYPE_WR, A53X_DECODER_READ_SET_2, 0x27);

static u8 A53X_DECODER_VDDM_LOW_SET_1[] = { 0xD7, 0x04 };
static DEFINE_STATIC_PACKET(a53x_decoder_vddm_low_set_1, DSI_PKT_TYPE_WR, A53X_DECODER_VDDM_LOW_SET_1, 0x02);

static u8 A53X_DECODER_VDDM_LOW_SET_2[] = { 0xF4, 0x07 };
static DEFINE_STATIC_PACKET(a53x_decoder_vddm_low_set_2, DSI_PKT_TYPE_WR, A53X_DECODER_VDDM_LOW_SET_2, 0x0F);

static u8 A53X_DECODER_FUSING_UPDATE_1[] = { 0xFE, 0xB0 };
static DEFINE_STATIC_PACKET(a53x_decoder_fusing_update_1, DSI_PKT_TYPE_WR, A53X_DECODER_FUSING_UPDATE_1, 0x00);

static u8 A53X_DECODER_FUSING_UPDATE_2[] = { 0xFE, 0x30 };
static DEFINE_STATIC_PACKET(a53x_decoder_fusing_update_2, DSI_PKT_TYPE_WR, A53X_DECODER_FUSING_UPDATE_2, 0x0F);

static u8 A53X_DECODER_CRC_PATERN_DISABLE[] = { 0xBE, 0x00 };
static DEFINE_STATIC_PACKET(a53x_decoder_crc_pattern_disable, DSI_PKT_TYPE_WR, A53X_DECODER_CRC_PATERN_DISABLE, 0x00);

static u8 A53X_DECODER_VDDM_RETURN_SET_1[] = { 0xD7, 0x00 };
static DEFINE_STATIC_PACKET(a53x_decoder_vddm_return_set_1, DSI_PKT_TYPE_WR, A53X_DECODER_VDDM_RETURN_SET_1, 0x02);

static u8 A53X_DECODER_VDDM_RETURN_SET_2[] = { 0xF4, 0x00 };
static DEFINE_STATIC_PACKET(a53x_decoder_vddm_return_set_2, DSI_PKT_TYPE_WR, A53X_DECODER_VDDM_RETURN_SET_2, 0x0F);
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_MDELAY(a53x_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a53x_wait_9msec, 9);
static DEFINE_PANEL_MDELAY(a53x_wait_7msec, 7);

#endif
static DEFINE_PANEL_MDELAY(a53x_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a53x_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(a53x_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(a53x_wait_dsc_test, 20);

static DEFINE_PANEL_MDELAY(a53x_wait_34msec, 34);

static DEFINE_PANEL_MDELAY(a53x_wait_sleep_out_90msec, 90);
static DEFINE_PANEL_MDELAY(a53x_wait_display_off, 20);

static DEFINE_PANEL_MDELAY(a53x_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a53x_wait_1usec, 1);

static DEFINE_PANEL_KEY(a53x_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a53x_level1_key_enable));
static DEFINE_PANEL_KEY(a53x_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a53x_level2_key_enable));
static DEFINE_PANEL_KEY(a53x_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a53x_level3_key_enable));

static DEFINE_PANEL_KEY(a53x_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a53x_level1_key_disable));
static DEFINE_PANEL_KEY(a53x_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a53x_level2_key_disable));
static DEFINE_PANEL_KEY(a53x_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a53x_level3_key_disable));


#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_VSYNC_DELAY(a53x_wait_1_vsync, 1);
static DEFINE_PANEL_FRAME_DELAY(a53x_wait_2_frame, 2);
static DEFINE_COND(a53x_cond_is_60hz, s6e3fc3_is_60hz);
static DEFINE_COND(a53x_cond_is_120hz, s6e3fc3_is_120hz);
#endif

static u8 A53X_HBM_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(a53x_hbm_transition, &a53x_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_hbm_transition, DSI_PKT_TYPE_WR, A53X_HBM_TRANSITION, 0);

static u8 A53X_ACL_SET[] = {
	0x65,
	0x55, 0x00, 0xB0, 0x51, 0x66, 0x98, 0x15, 0x55,
	0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48, 0x40, 0x00,
	0x20, 0x10, 0x09
};
static DECLARE_PKTUI(a53x_acl_set) = {
	{ .offset = 1, .maptbl = &a53x_maptbl[ACL_FRAME_AVG_MAPTBL] },
	{ .offset = 2, .maptbl = &a53x_maptbl[ACL_START_POINT_MAPTBL] },
	{ .offset = 17, .maptbl = &a53x_maptbl[ACL_DIM_SPEED_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a53x_acl_set, DSI_PKT_TYPE_WR, A53X_ACL_SET, 0x3B3);

static u8 A53X_ACL[] = {
	0x55, 0x01
};
static DEFINE_PKTUI(a53x_acl_control, &a53x_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_acl_control, DSI_PKT_TYPE_WR, A53X_ACL, 0);

static u8 A53X_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(a53x_wrdisbv, &a53x_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_wrdisbv, DSI_PKT_TYPE_WR, A53X_WRDISBV, 0);

static u8 A53X_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 A53X_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };
static DEFINE_STATIC_PACKET(a53x_caset, DSI_PKT_TYPE_WR, A53X_CASET, 0);
static DEFINE_STATIC_PACKET(a53x_paset, DSI_PKT_TYPE_WR, A53X_PASET, 0);

static u8 A53X_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C, 0x51	/* 1st 0x5C: default high, 2nd 0x51 : Enable SW RESET */
};
static DEFINE_STATIC_PACKET(a53x_pcd_det_set, DSI_PKT_TYPE_WR, A53X_PCD_SET_DET_LOW, 0);

static u8 A53X_ERR_FG_ON[] = {
	0xE5, 0x15
};
static DEFINE_STATIC_PACKET(a53x_err_fg_on, DSI_PKT_TYPE_WR, A53X_ERR_FG_ON, 0);

static u8 A53X_ERR_FG_OFF[] = {
	0xE5, 0x05
};
static DEFINE_STATIC_PACKET(a53x_err_fg_off, DSI_PKT_TYPE_WR, A53X_ERR_FG_OFF, 0);

static u8 A53X_ERR_FG_SETTING[] = {
	0xED,
	0x04, 0x4C, 0x20
	/* Vlin1, ELVDD, Vlin3 Monitor On */
	/* Defalut LOW */
};
static DEFINE_STATIC_PACKET(a53x_err_fg_setting, DSI_PKT_TYPE_WR, A53X_ERR_FG_SETTING, 0);

static u8 A53X_VSYNC_SET[] = {
	0xF2,
	0x54
};
static DEFINE_STATIC_PACKET(a53x_vsync_set, DSI_PKT_TYPE_WR, A53X_VSYNC_SET, 0x04);

static u8 A53X_FREQ_SET[] = {
	0xF2,
	0x00
};
static DEFINE_STATIC_PACKET(a53x_freq_set, DSI_PKT_TYPE_WR, A53X_FREQ_SET, 0x27);

static u8 A53X_PORCH_SET[] = {
	0xF2,
	0x55
};
static DEFINE_STATIC_PACKET(a53x_porch_set, DSI_PKT_TYPE_WR, A53X_PORCH_SET, 0x2E);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 A53X_XTALK_MODE[] = { 0xD9, 0x60 };
static DEFINE_PKTUI(a53x_xtalk_mode, &a53x_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a53x_xtalk_mode, DSI_PKT_TYPE_WR, A53X_XTALK_MODE, 0x1C);
#endif

static u8 A53X_FPS_1[] = { 0x60, 0x08, 0x00 };
static DEFINE_PKTUI(a53x_fps_1, &a53x_maptbl[FPS_MAPTBL_1], 1);
static DEFINE_VARIABLE_PACKET(a53x_fps_1, DSI_PKT_TYPE_WR, A53X_FPS_1, 0);


static u8 A53X_FPS_2[] = { 0x68, 0x00, };
static DEFINE_PKTUI(a53x_fps_2, &a53x_maptbl[FPS_MAPTBL_2], 1);
static DEFINE_VARIABLE_PACKET(a53x_fps_2, DSI_PKT_TYPE_WR, A53X_FPS_2, 0x28);

static u8 A53X_DIMMING_SPEED[] = { 0x63, 0x60 };
static DEFINE_PKTUI(a53x_dimming_speed, &a53x_maptbl[DIMMING_SPEED], 1);
static DEFINE_VARIABLE_PACKET(a53x_dimming_speed, DSI_PKT_TYPE_WR, A53X_DIMMING_SPEED, 0x91);

static u8 A53X_PANEL_UPDATE[] = {
	0xF7,
	0x0F
};
static DEFINE_STATIC_PACKET(a53x_panel_update, DSI_PKT_TYPE_WR, A53X_PANEL_UPDATE, 0);

static u8 A53X_GLOBAL_PARAM_SETTING[] = {
	0xF2,
	0x00, 0x05, 0x0E, 0x58, 0x54, 0x01, 0x0C, 0x00,
	0x04, 0x27, 0x16, 0x27, 0x16, 0x0C, 0x09, 0x74,
	0x27, 0x16, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};
static DEFINE_STATIC_PACKET(a53x_global_param_setting, DSI_PKT_TYPE_WR, A53X_GLOBAL_PARAM_SETTING, 0);

static u8 A52_GLOBAL_PARAM_SETTING[] = {
	0xF2,
	0x00, 0x05, 0x0E, 0x58, 0x54, 0x01, 0x0C, 0x00,
	0x04, 0x26, 0xE4, 0x2F, 0xB0, 0x0C, 0x09, 0x74,
	0x26, 0xE4, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};
static DEFINE_STATIC_PACKET(a52_global_param_setting, DSI_PKT_TYPE_WR, A52_GLOBAL_PARAM_SETTING, 0);

static u8 A53X_MIN_ROI_SETTING[] = {
	0xC2,
	0x1B, 0x41, 0xB0, 0x0E, 0x00, 0x3C, 0x5A, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(a53x_min_roi_setting, DSI_PKT_TYPE_WR, A53X_MIN_ROI_SETTING, 0);

static u8 A53X_FOD_ENTER[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a53x_fod_enter, DSI_PKT_TYPE_WR, A53X_FOD_ENTER, 0);

static u8 A53X_FOD_EXIT[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a53x_fod_exit, DSI_PKT_TYPE_WR, A53X_FOD_EXIT, 0);

static u8 A53X_TIG_ENABLE[] = {
	0xBF, 0x01, 0x00
};
static DEFINE_STATIC_PACKET(a53x_tig_enable, DSI_PKT_TYPE_WR, A53X_TIG_ENABLE, 0);

static u8 A53X_TIG_DISABLE[] = {
	0xBF, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(a53x_tig_disable, DSI_PKT_TYPE_WR, A53X_TIG_DISABLE, 0);

static u8 A53X_DSC[] = { 0x01 };
static DEFINE_STATIC_PACKET(a53x_dsc, DSI_PKT_TYPE_COMP, A53X_DSC, 0);

static u8 A53X_PPS[] = {
	//1080x2400 Slice Info : 540x40
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
	0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(a53x_pps, DSI_PKT_TYPE_PPS, A53X_PPS, 0);

static u8 A53X_NORMAL_MODE[] = {
	0x53, 0x20

};
static DEFINE_STATIC_PACKET(a53x_normal_mode, DSI_PKT_TYPE_WR, A53X_NORMAL_MODE, 0);

static u8 A53X_FFC_SETTING[] = {
	0xC5,
	0x0D, 0x10, 0x80, 0x45, 0x53, 0xC7
};
static DEFINE_PKTUI(a53x_ffc_setting, &a53x_maptbl[SET_FFC_MAPTBL], 5);
static DEFINE_VARIABLE_PACKET(a53x_ffc_setting, DSI_PKT_TYPE_WR, A53X_FFC_SETTING, 0x2A);
static DEFINE_COND(a53x_cond_is_a52_panel, s6e3fc3_is_a52_panel);

static DEFINE_PANEL_TIMER_MDELAY(a53x_init_complete_delay, 30);
static DEFINE_PANEL_TIMER_BEGIN(a53x_init_complete_delay,
		TIMER_DLYINFO(&a53x_init_complete_delay));

static struct seqinfo SEQINFO(a53x_set_bl_param_seq);
static struct seqinfo SEQINFO(a53x_set_fps_param_seq);
#if defined(CONFIG_MCD_PANEL_FACTORY)
static struct seqinfo SEQINFO(a53x_res_init_seq);
#endif

static void *a53x_init_cmdtbl[] = {
	&PKTINFO(a53x_sleep_out),
	&DLYINFO(a53x_wait_sleep_out_90msec),

	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),

	/* common setting */
	&CONDINFO_IF(a53x_cond_is_a52_panel),
		&PKTINFO(a52_global_param_setting),
	&CONDINFO_EL(a53x_cond_is_a52_panel),
		&PKTINFO(a53x_global_param_setting),
	&CONDINFO_FI(a53x_cond_is_a52_panel),

	&PKTINFO(a53x_panel_update),

	&PKTINFO(a53x_dsc),
	&PKTINFO(a53x_pps),

	&PKTINFO(a53x_te_offset),
	&PKTINFO(a53x_te_on),

	&PKTINFO(a53x_caset),
	&PKTINFO(a53x_paset),

	&PKTINFO(a53x_min_roi_setting),

	&PKTINFO(a53x_ffc_setting),
	&PKTINFO(a53x_panel_update),
	&PKTINFO(a53x_err_fg_on),
	&PKTINFO(a53x_err_fg_setting),
	&PKTINFO(a53x_vsync_set),
	&PKTINFO(a53x_pcd_det_set),
	&PKTINFO(a53x_freq_set),
	&PKTINFO(a53x_swire_no_pulse),
	&PKTINFO(a53x_seed_crc_on),
	&PKTINFO(a53x_panel_update),

	&PKTINFO(a53x_porch_set),
	&PKTINFO(a53x_panel_update),

	&PKTINFO(a53x_hbm_transition), /* 53h should be not included in bl_seq */
	&SEQINFO(a53x_set_bl_param_seq), /* includes FPS setting also */

	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),

	&TIMER_DLYINFO_BEGIN(a53x_init_complete_delay),
#if defined(CONFIG_MCD_PANEL_FACTORY)
	&SEQINFO(a53x_res_init_seq),
#endif
	&TIMER_DLYINFO(a53x_init_complete_delay),
};

static void *a53x_res_init_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),

	&s6e3fc3_restbl[RES_ID],
	&s6e3fc3_restbl[RES_COORDINATE],
	&s6e3fc3_restbl[RES_CODE],
	&s6e3fc3_restbl[RES_DATE],
	&s6e3fc3_restbl[RES_OCTA_ID],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fc3_restbl[RES_SELF_DIAG],
	&s6e3fc3_restbl[RES_ERR_FG],
	&s6e3fc3_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};
#if defined(CONFIG_MCD_PANEL_FACTORY)
static DEFINE_SEQINFO(a53x_res_init_seq, a53x_res_init_cmdtbl);
#endif

static void *a53x_set_bl_param_cmdtbl[] = {
	&PKTINFO(a53x_fps_1),
	&CONDINFO_IF(a53x_cond_is_a52_panel),
		&PKTINFO(a53x_fps_2),
	&CONDINFO_FI(a53x_cond_is_a52_panel),
	&PKTINFO(a53x_dimming_speed),
	&PKTINFO(a53x_tset_set),
	&PKTINFO(a53x_acl_set),
	&PKTINFO(a53x_panel_update),
	&PKTINFO(a53x_acl_control),
	&PKTINFO(a53x_wrdisbv),
	&PKTINFO(a53x_panel_update),
};

static DEFINE_SEQINFO(a53x_set_bl_param_seq, a53x_set_bl_param_cmdtbl);

static void *a53x_set_bl_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&PKTINFO(a53x_hbm_transition),
	&SEQINFO(a53x_set_bl_param_seq),
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static DEFINE_COND(a53x_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);
static DEFINE_COND(a53x_cond_is_panel_state_lpm, is_panel_state_lpm);

static void *a53x_set_fps_cmdtbl[] = {
	&CONDINFO_IF(a53x_cond_is_panel_state_not_lpm),
		&KEYINFO(a53x_level1_key_enable),
		&KEYINFO(a53x_level2_key_enable),
		&KEYINFO(a53x_level3_key_enable),
		&PKTINFO(a53x_hbm_transition),
		&SEQINFO(a53x_set_bl_param_seq),
		&KEYINFO(a53x_level3_key_disable),
		&KEYINFO(a53x_level2_key_disable),
		&KEYINFO(a53x_level1_key_disable),
	&CONDINFO_FI(a53x_cond_is_panel_state_not_lpm),
};

static void *a53x_display_on_cmdtbl[] = {
	&CONDINFO_IF(a53x_cond_is_panel_state_lpm),
		&KEYINFO(a53x_level2_key_enable),
		&PKTINFO(a53x_tig_enable),
		&KEYINFO(a53x_level2_key_disable),
		&DLYINFO(a53x_wait_17msec),
	&CONDINFO_FI(a53x_cond_is_panel_state_lpm),
	&KEYINFO(a53x_level1_key_enable),
	&PKTINFO(a53x_display_on),
	&KEYINFO(a53x_level1_key_disable),
	&CONDINFO_IF(a53x_cond_is_panel_state_lpm),
		&DLYINFO(a53x_wait_17msec),
		&KEYINFO(a53x_level2_key_enable),
		&PKTINFO(a53x_tig_disable),
		&KEYINFO(a53x_level2_key_disable),
	&CONDINFO_FI(a53x_cond_is_panel_state_lpm),
};

static void *a53x_display_off_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&PKTINFO(a53x_display_off),
	&PKTINFO(a53x_err_fg_off),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
	&DLYINFO(a53x_wait_display_off),
};

static void *a53x_exit_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fc3_dmptbl[DUMP_RDDPM_SLEEP_IN],
	&s6e3fc3_dmptbl[DUMP_RDDSM],
	&s6e3fc3_dmptbl[DUMP_ERR],
	&s6e3fc3_dmptbl[DUMP_DSI_ERR],
	&s6e3fc3_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(a53x_sleep_in),
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
	&DLYINFO(a53x_wait_sleep_in),
};

static void *a53x_alpm_set_bl_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),

	&PKTINFO(a53x_aod_vaint),
	&PKTINFO(a53x_aod_setting),
	&PKTINFO(a53x_lpm_nit),
	&CONDINFO_IF(a53x_cond_is_a52_panel),
		&PKTINFO(a52_lpm_on_hfp_1),
		&PKTINFO(a52_lpm_on_hfp_2),
	&CONDINFO_FI(a53x_cond_is_a52_panel),
	&PKTINFO(a53x_lpm_porch_on),
	&PKTINFO(a53x_panel_update),

	&DLYINFO(a53x_wait_1usec),
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
	&DLYINFO(a53x_wait_17msec),
};

static void *a53x_alpm_exit_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&PKTINFO(a53x_wrdisbv),
	&PKTINFO(a53x_swire_no_pulse_lpm_off),
	&CONDINFO_IF(a53x_cond_is_a52_panel),
		&PKTINFO(a52_lpm_off_hfp_1),
		&PKTINFO(a52_lpm_off_hfp_2),
	&CONDINFO_FI(a53x_cond_is_a52_panel),
	&PKTINFO(a53x_lpm_porch_off),
	&PKTINFO(a53x_normal_setting),
	&PKTINFO(a53x_lpm_off_sync_ctrl),
	&PKTINFO(a53x_normal_mode),
	&PKTINFO(a53x_panel_update),
	&DLYINFO(a53x_wait_34msec),

	&SEQINFO(a53x_set_bl_param_seq),
	&DLYINFO(a53x_wait_1usec),

	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static void *a53x_dump_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&s6e3fc3_dmptbl[DUMP_RDDPM],
	&s6e3fc3_dmptbl[DUMP_RDDSM],
	&s6e3fc3_dmptbl[DUMP_DSI_ERR],
	&s6e3fc3_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static void *a53x_ffc_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&PKTINFO(a53x_ffc_setting),
	&PKTINFO(a53x_panel_update),
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static void *a53x_check_condition_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&s6e3fc3_dmptbl[DUMP_RDDPM],
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_MASK_LAYER
static void *a53x_mask_layer_workaround_cmdtbl[] = {
	&PKTINFO(a53x_wrdisbv),
	&PKTINFO(a53x_hbm_transition),
	&DLYINFO(a53x_wait_2_frame),
};

static void *a53x_mask_layer_before_cmdtbl[] = {
	&DLYINFO(a53x_wait_1_vsync),

	&CONDINFO_IF(a53x_cond_is_120hz),
		&DLYINFO(a53x_wait_1msec),
	&CONDINFO_FI(a53x_cond_is_120hz),

	&CONDINFO_IF(a53x_cond_is_60hz),
		&DLYINFO(a53x_wait_9msec),
	&CONDINFO_FI(a53x_cond_is_60hz),
};

static void *a53x_mask_layer_enter_br_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),

	&PKTINFO(a53x_acl_control),
	&PKTINFO(a53x_dimming_speed),
	&PKTINFO(a53x_hbm_transition),
	&PKTINFO(a53x_wrdisbv),
	&PKTINFO(a53x_fod_enter),

	&CONDINFO_IF(a53x_cond_is_120hz),
		&DLYINFO(a53x_wait_9msec),
	&CONDINFO_FI(a53x_cond_is_120hz),

	&CONDINFO_IF(a53x_cond_is_60hz),
		&DLYINFO(a53x_wait_17msec),
	&CONDINFO_FI(a53x_cond_is_60hz),

	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static void *a53x_mask_layer_exit_br_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),

	&PKTINFO(a53x_acl_control),

	&PKTINFO(a53x_dimming_speed),
	&PKTINFO(a53x_hbm_transition),
	&PKTINFO(a53x_wrdisbv),
	&PKTINFO(a53x_fod_exit),

	&CONDINFO_IF(a53x_cond_is_120hz),
		&DLYINFO(a53x_wait_9msec),
	&CONDINFO_FI(a53x_cond_is_120hz),

	&CONDINFO_IF(a53x_cond_is_60hz),
		&DLYINFO(a53x_wait_17msec),
	&CONDINFO_FI(a53x_cond_is_60hz),

	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};

static void *a53x_mask_layer_exit_after_cmdtbl[] = {
	&CONDINFO_IF(a53x_cond_is_60hz),
		&DLYINFO(a53x_wait_9msec),
	&CONDINFO_FI(a53x_cond_is_60hz),
};
#endif

#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
static void *a53x_decoder_test_cmdtbl[] = {
	&KEYINFO(a53x_level1_key_enable),
	&KEYINFO(a53x_level2_key_enable),
	&KEYINFO(a53x_level3_key_enable),
	&PKTINFO(a53x_decoder_test_caset),
	&PKTINFO(a53x_decoder_test_paset),
	&PKTINFO(a53x_decoder_test_2c),
	&PKTINFO(a53x_decoder_crc_pattern_enable),
	&PKTINFO(a53x_decoder_read_set_1),
	&DLYINFO(a53x_wait_dsc_test),
	&s6e3fc3_restbl[RES_DECODER_TEST1],
	&PKTINFO(a53x_decoder_read_set_2),
	&DLYINFO(a53x_wait_dsc_test),
	&s6e3fc3_restbl[RES_DECODER_TEST2],
	&PKTINFO(a53x_decoder_vddm_low_set_1),
	&PKTINFO(a53x_decoder_vddm_low_set_2),
	&PKTINFO(a53x_decoder_fusing_update_1),
	&PKTINFO(a53x_decoder_fusing_update_2),
	&PKTINFO(a53x_decoder_crc_pattern_enable),
	&PKTINFO(a53x_decoder_read_set_1),
	&DLYINFO(a53x_wait_dsc_test),
	&s6e3fc3_restbl[RES_DECODER_TEST3],
	&PKTINFO(a53x_decoder_read_set_2),
	&DLYINFO(a53x_wait_dsc_test),
	&s6e3fc3_restbl[RES_DECODER_TEST4],
	&PKTINFO(a53x_decoder_crc_pattern_disable),
	&PKTINFO(a53x_decoder_vddm_return_set_1),
	&PKTINFO(a53x_decoder_vddm_return_set_2),
	&PKTINFO(a53x_decoder_fusing_update_1),
	&PKTINFO(a53x_decoder_fusing_update_2),
	&KEYINFO(a53x_level3_key_disable),
	&KEYINFO(a53x_level2_key_disable),
	&KEYINFO(a53x_level1_key_disable),
};
#endif

static void *a53x_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo a53x_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a53x_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a53x_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a53x_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a53x_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a53x_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a53x_exit_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("fps-seq", a53x_set_fps_cmdtbl),
	[PANEL_ALPM_SET_BL_SEQ] = SEQINFO_INIT("alpm-set-bl-seq", a53x_alpm_set_bl_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", a53x_alpm_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_MASK_LAYER
	[PANEL_MASK_LAYER_STOP_DIMMING_SEQ] = SEQINFO_INIT("mask-layer-workaround-seq", a53x_mask_layer_workaround_cmdtbl),
	[PANEL_MASK_LAYER_BEFORE_SEQ] = SEQINFO_INIT("mask-layer-before-seq", a53x_mask_layer_before_cmdtbl),
	[PANEL_MASK_LAYER_ENTER_BR_SEQ] = SEQINFO_INIT("mask-layer-enter-br-seq", a53x_mask_layer_enter_br_cmdtbl), //temp br
	[PANEL_MASK_LAYER_EXIT_BR_SEQ] = SEQINFO_INIT("mask-layer-exit-br-seq", a53x_mask_layer_exit_br_cmdtbl),
	[PANEL_MASK_LAYER_EXIT_AFTER_SEQ] = SEQINFO_INIT("mask-layer-exit-after-seq", a53x_mask_layer_exit_after_cmdtbl),
#endif
	[PANEL_FFC_SEQ] = SEQINFO_INIT("set-ffc-seq", a53x_ffc_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a53x_dump_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", a53x_check_condition_cmdtbl),
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
	[PANEL_DECODER_TEST_SEQ] = SEQINFO_INIT("decoder-test-seq", a53x_decoder_test_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", a53x_dummy_cmdtbl),
};

struct common_panel_info s6e3fc3_a53x_panel_info = {
	.ldi_name = "s6e3fc3",
	.name = "s6e3fc3_a53x",
	.model = "AMS646YB01",
	.vendor = "SDC",
	.id = 0x800042,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA | DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_2BYTE_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
		.dft_osc_freq = 181300,
		/* Todo the hs_clk must be synchronized with the value of LK,
		 * It must be changed to be read and set in the LK.
		 */
		.dft_dsi_freq = 1108000,
	},
	.ddi_ops = {
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
		.decoder_test = s6e3fc3_decoder_test,
#endif
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3fc3_a53x_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fc3_a53x_default_resol),
		.resol = s6e3fc3_a53x_default_resol,
	},
	.vrrtbl = s6e3fc3_a53x_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3fc3_a53x_default_vrrtbl),
	.maptbl = a53x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a53x_maptbl),
	.seqtbl = a53x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a53x_seqtbl),
	.rditbl = s6e3fc3_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fc3_rditbl),
	.restbl = s6e3fc3_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fc3_restbl),
	.dumpinfo = s6e3fc3_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fc3_dmptbl),

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3fc3_a53x_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fc3_a53x_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fc3_a53x_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3fc3_a53x_aod,
#endif
};

#endif /* __S6E3FC3_A53X_PANEL_H__ */
