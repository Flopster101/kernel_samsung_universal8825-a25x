/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3_a25x_panel.h
 *
 * Header file for S6E3FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_A25X_PANEL_H__
#define __S6E3FC3_A25X_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "oled_function.h"
#include "s6e3fc3.h"
#include "s6e3fc3_a25x.h"
#include "s6e3fc3_dimming.h"
#ifdef CONFIG_USDM_MDNIE
#include "s6e3fc3_a25x_panel_mdnie.h"
#endif
#include "s6e3fc3_a25x_panel_dimming.h"
#ifdef CONFIG_USDM_PANEL_AOD_BL
#include "s6e3fc3_a25x_panel_aod_dimming.h"
#endif
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
#include "s6e3fc3_a25x_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e3fc3_a25x_resol.h"

#undef __pn_name__
#define __pn_name__	a25x

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

static u8 a25x_brt_table[S6E3FC3_TOTAL_STEP][2] = {
	{ 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x04 }, { 0x00, 0x05 }, { 0x00, 0x06 },
	{ 0x00, 0x08 }, { 0x00, 0x09 }, { 0x00, 0x0B }, { 0x00, 0x0D }, { 0x00, 0x0F },
	{ 0x00, 0x11 }, { 0x00, 0x13 }, { 0x00, 0x15 }, { 0x00, 0x17 }, { 0x00, 0x19 },
	{ 0x00, 0x1B }, { 0x00, 0x1E }, { 0x00, 0x20 }, { 0x00, 0x22 }, { 0x00, 0x25 },
	{ 0x00, 0x27 }, { 0x00, 0x2A }, { 0x00, 0x2C }, { 0x00, 0x2F }, { 0x00, 0x31 },
	{ 0x00, 0x34 }, { 0x00, 0x36 }, { 0x00, 0x39 }, { 0x00, 0x3C }, { 0x00, 0x3E },
	{ 0x00, 0x41 }, { 0x00, 0x44 }, { 0x00, 0x47 }, { 0x00, 0x4A }, { 0x00, 0x4C },
	{ 0x00, 0x4F }, { 0x00, 0x52 }, { 0x00, 0x55 }, { 0x00, 0x58 }, { 0x00, 0x5B },
	{ 0x00, 0x5E }, { 0x00, 0x61 }, { 0x00, 0x64 }, { 0x00, 0x67 }, { 0x00, 0x6A },
	{ 0x00, 0x6D }, { 0x00, 0x70 }, { 0x00, 0x74 }, { 0x00, 0x77 }, { 0x00, 0x7A },
	{ 0x00, 0x7D }, { 0x00, 0x80 }, { 0x00, 0x84 }, { 0x00, 0x87 }, { 0x00, 0x8A },
	{ 0x00, 0x8D }, { 0x00, 0x91 }, { 0x00, 0x94 }, { 0x00, 0x97 }, { 0x00, 0x9B },
	{ 0x00, 0x9E }, { 0x00, 0xA1 }, { 0x00, 0xA5 }, { 0x00, 0xA8 }, { 0x00, 0xAC },
	{ 0x00, 0xAF }, { 0x00, 0xB3 }, { 0x00, 0xB6 }, { 0x00, 0xBA }, { 0x00, 0xBD },
	{ 0x00, 0xC1 }, { 0x00, 0xC4 }, { 0x00, 0xC8 }, { 0x00, 0xCC }, { 0x00, 0xCF },
	{ 0x00, 0xD3 }, { 0x00, 0xD6 }, { 0x00, 0xDA }, { 0x00, 0xDE }, { 0x00, 0xE1 },
	{ 0x00, 0xE5 }, { 0x00, 0xE9 }, { 0x00, 0xEC }, { 0x00, 0xF0 }, { 0x00, 0xF4 },
	{ 0x00, 0xF8 }, { 0x00, 0xFB }, { 0x00, 0xFF }, { 0x01, 0x03 }, { 0x01, 0x07 },
	{ 0x01, 0x0B }, { 0x01, 0x0E }, { 0x01, 0x12 }, { 0x01, 0x16 }, { 0x01, 0x1A },
	{ 0x01, 0x1E }, { 0x01, 0x22 }, { 0x01, 0x26 }, { 0x01, 0x2A }, { 0x01, 0x2E },
	{ 0x01, 0x32 }, { 0x01, 0x36 }, { 0x01, 0x39 }, { 0x01, 0x3D }, { 0x01, 0x41 },
	{ 0x01, 0x45 }, { 0x01, 0x49 }, { 0x01, 0x4D }, { 0x01, 0x52 }, { 0x01, 0x56 },
	{ 0x01, 0x5A }, { 0x01, 0x5E }, { 0x01, 0x62 }, { 0x01, 0x66 }, { 0x01, 0x6A },
	{ 0x01, 0x6E }, { 0x01, 0x72 }, { 0x01, 0x76 }, { 0x01, 0x7B }, { 0x01, 0x7F },
	{ 0x01, 0x83 }, { 0x01, 0x87 }, { 0x01, 0x8B }, { 0x01, 0x90 }, { 0x01, 0x94 },
	{ 0x01, 0x98 }, { 0x01, 0x9C }, { 0x01, 0xA0 }, { 0x01, 0xA5 }, { 0x01, 0xA9 },
	{ 0x01, 0xAD }, { 0x01, 0xB1 }, { 0x01, 0xB6 }, { 0x01, 0xBA }, { 0x01, 0xBE },
	{ 0x01, 0xC2 }, { 0x01, 0xC7 }, { 0x01, 0xCB }, { 0x01, 0xCF }, { 0x01, 0xD4 },
	{ 0x01, 0xD8 }, { 0x01, 0xDC }, { 0x01, 0xE1 }, { 0x01, 0xE5 }, { 0x01, 0xE9 },
	{ 0x01, 0xEE }, { 0x01, 0xF2 }, { 0x01, 0xF6 }, { 0x01, 0xFB }, { 0x01, 0xFF },
	{ 0x02, 0x04 }, { 0x02, 0x08 }, { 0x02, 0x0D }, { 0x02, 0x11 }, { 0x02, 0x15 },
	{ 0x02, 0x1A }, { 0x02, 0x1E }, { 0x02, 0x23 }, { 0x02, 0x27 }, { 0x02, 0x2C },
	{ 0x02, 0x30 }, { 0x02, 0x35 }, { 0x02, 0x39 }, { 0x02, 0x3E }, { 0x02, 0x43 },
	{ 0x02, 0x47 }, { 0x02, 0x4C }, { 0x02, 0x50 }, { 0x02, 0x55 }, { 0x02, 0x59 },
	{ 0x02, 0x5E }, { 0x02, 0x63 }, { 0x02, 0x67 }, { 0x02, 0x6C }, { 0x02, 0x70 },
	{ 0x02, 0x75 }, { 0x02, 0x7A }, { 0x02, 0x7E }, { 0x02, 0x83 }, { 0x02, 0x88 },
	{ 0x02, 0x8C }, { 0x02, 0x91 }, { 0x02, 0x96 }, { 0x02, 0x9A }, { 0x02, 0x9F },
	{ 0x02, 0xA4 }, { 0x02, 0xA8 }, { 0x02, 0xAD }, { 0x02, 0xB2 }, { 0x02, 0xB7 },
	{ 0x02, 0xBB }, { 0x02, 0xC0 }, { 0x02, 0xC5 }, { 0x02, 0xCA }, { 0x02, 0xCE },
	{ 0x02, 0xD3 }, { 0x02, 0xD8 }, { 0x02, 0xDD }, { 0x02, 0xE2 }, { 0x02, 0xE6 },
	{ 0x02, 0xEB }, { 0x02, 0xF0 }, { 0x02, 0xF5 }, { 0x02, 0xFA }, { 0x02, 0xFF },
	{ 0x03, 0x04 }, { 0x03, 0x08 }, { 0x03, 0x0D }, { 0x03, 0x12 }, { 0x03, 0x17 },
	{ 0x03, 0x1C }, { 0x03, 0x21 }, { 0x03, 0x26 }, { 0x03, 0x2B }, { 0x03, 0x30 },
	{ 0x03, 0x34 }, { 0x03, 0x39 }, { 0x03, 0x3E }, { 0x03, 0x43 }, { 0x03, 0x48 },
	{ 0x03, 0x4D }, { 0x03, 0x52 }, { 0x03, 0x57 }, { 0x03, 0x5C }, { 0x03, 0x61 },
	{ 0x03, 0x66 }, { 0x03, 0x6B }, { 0x03, 0x70 }, { 0x03, 0x75 }, { 0x03, 0x7A },
	{ 0x03, 0x7F }, { 0x03, 0x84 }, { 0x03, 0x89 }, { 0x03, 0x8E }, { 0x03, 0x93 },
	{ 0x03, 0x98 }, { 0x03, 0x9E }, { 0x03, 0xA3 }, { 0x03, 0xA8 }, { 0x03, 0xAD },
	{ 0x03, 0xB2 }, { 0x03, 0xB7 }, { 0x03, 0xBC }, { 0x03, 0xC1 }, { 0x03, 0xC6 },
	{ 0x03, 0xCB }, { 0x03, 0xD1 }, { 0x03, 0xD6 }, { 0x03, 0xDB }, { 0x03, 0xE0 },
	{ 0x03, 0xE5 }, { 0x03, 0xEA }, { 0x03, 0xEF }, { 0x03, 0xF5 }, { 0x03, 0xFA },
	{ 0x03, 0xFF },
	{ 0x00, 0x02 }, { 0x00, 0x04 }, { 0x00, 0x06 }, { 0x00, 0x08 }, { 0x00, 0x09 },
	{ 0x00, 0x0B }, { 0x00, 0x0D }, { 0x00, 0x0F }, { 0x00, 0x11 }, { 0x00, 0x13 },
	{ 0x00, 0x15 }, { 0x00, 0x17 }, { 0x00, 0x19 }, { 0x00, 0x1B }, { 0x00, 0x1C },
	{ 0x00, 0x1E }, { 0x00, 0x20 }, { 0x00, 0x22 }, { 0x00, 0x24 }, { 0x00, 0x26 },
	{ 0x00, 0x28 }, { 0x00, 0x2A }, { 0x00, 0x2C }, { 0x00, 0x2E }, { 0x00, 0x2F },
	{ 0x00, 0x31 }, { 0x00, 0x33 }, { 0x00, 0x35 }, { 0x00, 0x37 }, { 0x00, 0x39 },
	{ 0x00, 0x3B }, { 0x00, 0x3D }, { 0x00, 0x3F }, { 0x00, 0x41 }, { 0x00, 0x42 },
	{ 0x00, 0x44 }, { 0x00, 0x46 }, { 0x00, 0x48 }, { 0x00, 0x4A }, { 0x00, 0x4C },
	{ 0x00, 0x4E }, { 0x00, 0x50 }, { 0x00, 0x52 }, { 0x00, 0x54 }, { 0x00, 0x55 },
	{ 0x00, 0x57 }, { 0x00, 0x59 }, { 0x00, 0x5B }, { 0x00, 0x5D }, { 0x00, 0x5F },
	{ 0x00, 0x61 }, { 0x00, 0x63 }, { 0x00, 0x65 }, { 0x00, 0x66 }, { 0x00, 0x68 },
	{ 0x00, 0x6A }, { 0x00, 0x6C }, { 0x00, 0x6E }, { 0x00, 0x70 }, { 0x00, 0x72 },
	{ 0x00, 0x74 }, { 0x00, 0x76 }, { 0x00, 0x78 }, { 0x00, 0x79 }, { 0x00, 0x7B },
	{ 0x00, 0x7D }, { 0x00, 0x7F }, { 0x00, 0x81 }, { 0x00, 0x83 }, { 0x00, 0x85 },
	{ 0x00, 0x87 }, { 0x00, 0x89 }, { 0x00, 0x8B }, { 0x00, 0x8C }, { 0x00, 0x8E },
	{ 0x00, 0x90 }, { 0x00, 0x92 }, { 0x00, 0x94 }, { 0x00, 0x96 }, { 0x00, 0x98 },
	{ 0x00, 0x9A }, { 0x00, 0x9C }, { 0x00, 0x9E }, { 0x00, 0x9F }, { 0x00, 0xA1 },
	{ 0x00, 0xA3 }, { 0x00, 0xA5 }, { 0x00, 0xA7 }, { 0x00, 0xA9 }, { 0x00, 0xAB },
	{ 0x00, 0xAD }, { 0x00, 0xAF }, { 0x00, 0xB1 }, { 0x00, 0xB2 }, { 0x00, 0xB4 },
	{ 0x00, 0xB6 }, { 0x00, 0xB8 }, { 0x00, 0xBA }, { 0x00, 0xBC }, { 0x00, 0xBE },
	{ 0x00, 0xC0 }, { 0x00, 0xC2 }, { 0x00, 0xC3 }, { 0x00, 0xC5 }, { 0x00, 0xC7 },
	{ 0x00, 0xC9 }, { 0x00, 0xCB }, { 0x00, 0xCD }, { 0x00, 0xCF }, { 0x00, 0xD1 },
	{ 0x00, 0xD3 }, { 0x00, 0xD5 }, { 0x00, 0xD6 }, { 0x00, 0xD8 }, { 0x00, 0xDA },
	{ 0x00, 0xDC }, { 0x00, 0xDE }, { 0x00, 0xE0 }, { 0x00, 0xE2 }, { 0x00, 0xE4 },
	{ 0x00, 0xE6 }, { 0x00, 0xE8 }, { 0x00, 0xE9 }, { 0x00, 0xEB }, { 0x00, 0xED },
	{ 0x00, 0xEF }, { 0x00, 0xF1 }, { 0x00, 0xF3 }, { 0x00, 0xF5 }, { 0x00, 0xF7 },
	{ 0x00, 0xF9 }, { 0x00, 0xFB }, { 0x00, 0xFC }, { 0x00, 0xFE }, { 0x01, 0x00 },
	{ 0x01, 0x02 }, { 0x01, 0x04 }, { 0x01, 0x06 }, { 0x01, 0x08 }, { 0x01, 0x0A },
	{ 0x01, 0x0C }, { 0x01, 0x0E }, { 0x01, 0x0F }, { 0x01, 0x11 }, { 0x01, 0x13 },
	{ 0x01, 0x15 }, { 0x01, 0x17 }, { 0x01, 0x19 }, { 0x01, 0x1B }, { 0x01, 0x1D },
	{ 0x01, 0x1F }, { 0x01, 0x21 }, { 0x01, 0x22 }, { 0x01, 0x24 }, { 0x01, 0x26 },
	{ 0x01, 0x28 }, { 0x01, 0x2A }, { 0x01, 0x2C }, { 0x01, 0x2E }, { 0x01, 0x30 },
	{ 0x01, 0x32 }, { 0x01, 0x33 }, { 0x01, 0x35 }, { 0x01, 0x37 }, { 0x01, 0x39 },
	{ 0x01, 0x3B }, { 0x01, 0x3D }, { 0x01, 0x3F }, { 0x01, 0x41 }, { 0x01, 0x43 },
	{ 0x01, 0x45 }, { 0x01, 0x46 }, { 0x01, 0x48 }, { 0x01, 0x4A }, { 0x01, 0x4C },
	{ 0x01, 0x4E }, { 0x01, 0x50 }, { 0x01, 0x52 }, { 0x01, 0x54 }, { 0x01, 0x56 },
	{ 0x01, 0x58 }, { 0x01, 0x59 }, { 0x01, 0x5B }, { 0x01, 0x5D }, { 0x01, 0x5F },
	{ 0x01, 0x61 }, { 0x01, 0x63 }, { 0x01, 0x65 }, { 0x01, 0x67 }, { 0x01, 0x69 },
	{ 0x01, 0x6B }, { 0x01, 0x6C }, { 0x01, 0x6E }, { 0x01, 0x70 }, { 0x01, 0x72 },
	{ 0x01, 0x74 }, { 0x01, 0x76 }, { 0x01, 0x78 }, { 0x01, 0x7A }, { 0x01, 0x7C },
	{ 0x01, 0x7E }, { 0x01, 0x7F }, { 0x01, 0x81 }, { 0x01, 0x83 }, { 0x01, 0x85 },
	{ 0x01, 0x87 }, { 0x01, 0x89 }, { 0x01, 0x8B }, { 0x01, 0x8D }, { 0x01, 0x8F },
	{ 0x01, 0x90 }, { 0x01, 0x92 }, { 0x01, 0x94 }, { 0x01, 0x96 }, { 0x01, 0x98 },
	{ 0x01, 0x9A }, { 0x01, 0x9C }, { 0x01, 0x9E }, { 0x01, 0xA0 }, { 0x01, 0xA2 },
	{ 0x01, 0xA3 }, { 0x01, 0xA5 }, { 0x01, 0xA7 }, { 0x01, 0xA9 }, { 0x01, 0xAB },
	{ 0x01, 0xAD }, { 0x01, 0xAF }, { 0x01, 0xB1 }, { 0x01, 0xB3 }, { 0x01, 0xB5 },
	{ 0x01, 0xB6 }, { 0x01, 0xB8 }, { 0x01, 0xBA }, { 0x01, 0xBC }, { 0x01, 0xBE },
	{ 0x01, 0xC0 }, { 0x01, 0xC2 }, { 0x01, 0xC4 }, { 0x01, 0xC6 }, { 0x01, 0xC8 },
	{ 0x01, 0xC9 }, { 0x01, 0xCB }, { 0x01, 0xCD }, { 0x01, 0xCF }, { 0x01, 0xD1 },
	{ 0x01, 0xD3 }, { 0x01, 0xD5 }, { 0x01, 0xD7 }, { 0x01, 0xD9 }, { 0x01, 0xDB },
	{ 0x01, 0xDC }, { 0x01, 0xDE }, { 0x01, 0xE0 }, { 0x01, 0xE2 }, { 0x01, 0xE4 },
};

static u8 a25x_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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

static u8 a25x_acl_frame_avg_table[][1] = {
	{ 0x44 }, /* 16 Frame Avg */
	{ 0x55 }, /* 32 Frame Avg */
};

static u8 a25x_acl_start_point_table[][2] = {
	{ 0x00, 0xB0 }, /* 50 Percent */
	{ 0x40, 0x28 }, /* 60 Percent */
};

static u8 a25x_acl_dim_speed_table[MAX_S6E3FC3_ACL_DIM][1] = {
	[S6E3FC3_ACL_DIM_OFF] = { 0x00 }, /* 0x00 : ACL Dimming Off */
	[S6E3FC3_ACL_DIM_ON] = { 0x20 }, /* 0x20 : ACL Dimming 32 Frames */
};

static u8 a25x_acl_opr_table[MAX_S6E3FC3_ACL_OPR][1] = {
	[S6E3FC3_ACL_OPR_0] = { 0x00 }, /* ACL OFF, OPR 0% */
	[S6E3FC3_ACL_OPR_1] = { 0x01 }, /* ACL ON, OPR 8% */
	[S6E3FC3_ACL_OPR_2] = { 0x03 }, /* ACL ON, OPR 15% */
	[S6E3FC3_ACL_OPR_3] = { 0x03 }, /* ACL ON, OPR 15% */
};

static u8 a25x_lpm_nit_table[4][1] = {
	/* LPM 2NIT: */
	{ 0x27 },
	/* LPM 10NIT */
	{ 0x26 },
	/* LPM 30NIT */
	{ 0x25  },
	/* LPM 60NIT */
	{ 0x24 },
};

static u8 a25x_exit_lpm_nit_table[4][1] = {
	/* LPM 2NIT: */
	{ 0x23 },
	/* LPM 10NIT */
	{ 0x22 },
	/* LPM 30NIT */
	{ 0x21  },
	/* LPM 60NIT */
	{ 0x20 },
};

static u8 a25x_lpm_on_table[2][1] = {
	[ALPM_MODE] = { 0x23 },
	[HLPM_MODE] = { 0x23 },
};

static u8 a25x_ffc_table[MAX_S6E3FC3_A25X_HS_CLK][2] = {
	[S6E3FC3_A25X_HS_CLK_1108] = {0x53, 0xC7}, // FFC for HS: 1108
	[S6E3FC3_A25X_HS_CLK_1124] = {0x52, 0x96}, // FFC for HS: 1124
	[S6E3FC3_A25X_HS_CLK_1125] = {0x52, 0x83}, // FFC for HS: 1125
};

static u8 a25x_fps_table_1[][2] = {
	[S6E3FC3_VRR_FPS_120] = { 0x08 , 0x00 },
	[S6E3FC3_VRR_FPS_60] = { 0x00, 0x00 },
};

static u8 a25x_fps_table_2[][1] = {
	[S6E3FC3_VRR_FPS_120] = { 0x02 },
	[S6E3FC3_VRR_FPS_60] = { 0x22 },
};

static u8 a25x_dimming_speep_table_1[][1] = {
	[S6E3FC3_SMOOTH_DIMMING_OFF] = { 0x20 },
	[S6E3FC3_SMOOTH_DIMMING_ON] = { 0x60 },
};

static u8 a25x_irc_mode_table[][7] = {
	[0 ... S6E3FC3_TOTAL_STEP - 2] = { 0x27, 0x25, 0xFC, 0x47, 0xD0, 0x80, 0x01 },		/* flat gamma */
	[S6E3FC3_TOTAL_STEP - 1] = { 0x27, 0x65, 0xFC, 0x47, 0x00, 0x80, 0x01 },		/* moderato */
};

static struct maptbl a25x_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(a25x_brt_table, &DDI_FUNC(S6E3FC3_MAPTBL_INIT_GAMMA_MODE2_BRT), &OLED_FUNC(OLED_MAPTBL_GETIDX_GM2_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(a25x_hbm_transition_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_HBM_TRANSITION), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),

	[ACL_FRAME_AVG_MAPTBL] = DEFINE_2D_MAPTBL(a25x_acl_frame_avg_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_ACL_ONOFF), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[ACL_START_POINT_MAPTBL] = DEFINE_2D_MAPTBL(a25x_acl_start_point_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_HBM_ONOFF), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[ACL_DIM_SPEED_MAPTBL] = DEFINE_2D_MAPTBL(a25x_acl_dim_speed_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_ACL_DIM_ONOFF), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a25x_acl_opr_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_ACL_OPR), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),

	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a25x_tset_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), NULL, &OLED_FUNC(OLED_MAPTBL_COPY_TSET)),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(a25x_lpm_nit_table, &DDI_FUNC(S6E3FC3_MAPTBL_INIT_LPM_BRT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_LPM_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[LPM_EXIT_NIT_MAPTBL] = DEFINE_2D_MAPTBL(a25x_exit_lpm_nit_table, &DDI_FUNC(S6E3FC3_MAPTBL_INIT_LPM_BRT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_LPM_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[FPS_MAPTBL_1] = DEFINE_2D_MAPTBL(a25x_fps_table_1, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_VRR_FPS), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[FPS_MAPTBL_2] = DEFINE_2D_MAPTBL(a25x_fps_table_2, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_VRR_FPS), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[DIMMING_SPEED] = DEFINE_2D_MAPTBL(a25x_dimming_speep_table_1, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC3_MAPTBL_GETIDX_SMOOTH_TRANSITION), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[SET_FFC_MAPTBL] = DEFINE_2D_MAPTBL(a25x_ffc_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &PANEL_FUNC(S6E3FC3_A25X_MAPTBL_GETIDX_FFC), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[IRC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(a25x_irc_mode_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &OLED_FUNC(OLED_MAPTBL_GETIDX_GM2_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
};

/* ===================================================================================== */
/* ============================== [S6E3FC3 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A25X_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A25X_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A25X_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 A25X_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A25X_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A25X_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A25X_SLEEP_OUT[] = { 0x11 };
static u8 A25X_SLEEP_IN[] = { 0x10 };
static u8 A25X_DISPLAY_OFF[] = { 0x28 };
static u8 A25X_DISPLAY_ON[] = { 0x29 };

static u8 A25X_MULTI_CMD_ENABLE[] = { 0x72, 0x2C, 0x21 };
static u8 A25X_MULTI_CMD_DISABLE[] = { 0x72, 0x2C, 0x01 };
static u8 A25X_MULTI_CMD_DUMMY[] = { 0x0A, 0x00 };

static u8 A25X_TE_ON[] = { 0x35, 0x00, 0x00 };
static u8 A25X_TE_SETTING[] = { 0xB9, 0x01, 0x09, 0x0F, 0x00, 0x0F};
static u8 A25X_TE_AOD_SETTING[] = { 0xB9, 0x01, 0x09, 0x36, 0x00, 0x0F};

static DEFINE_STATIC_PACKET(a25x_level1_key_enable, DSI_PKT_TYPE_WR, A25X_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a25x_level2_key_enable, DSI_PKT_TYPE_WR, A25X_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a25x_level3_key_enable, DSI_PKT_TYPE_WR, A25X_KEY3_ENABLE, 0);

static DEFINE_STATIC_PACKET(a25x_level1_key_disable, DSI_PKT_TYPE_WR, A25X_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a25x_level2_key_disable, DSI_PKT_TYPE_WR, A25X_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a25x_level3_key_disable, DSI_PKT_TYPE_WR, A25X_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a25x_multi_cmd_enable, DSI_PKT_TYPE_WR, A25X_MULTI_CMD_ENABLE, 0);
static DEFINE_STATIC_PACKET(a25x_multi_cmd_disable, DSI_PKT_TYPE_WR, A25X_MULTI_CMD_DISABLE, 0);
static DEFINE_STATIC_PACKET(a25x_multi_cmd_dummy, DSI_PKT_TYPE_WR, A25X_MULTI_CMD_DUMMY, 0);

static DEFINE_STATIC_PACKET(a25x_sleep_out, DSI_PKT_TYPE_WR, A25X_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a25x_sleep_in, DSI_PKT_TYPE_WR, A25X_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a25x_display_on, DSI_PKT_TYPE_WR, A25X_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a25x_display_off, DSI_PKT_TYPE_WR, A25X_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(a25x_te_on, DSI_PKT_TYPE_WR, A25X_TE_ON, 0);
static DEFINE_STATIC_PACKET(a25x_te_setting, DSI_PKT_TYPE_WR, A25X_TE_SETTING, 0);
static DEFINE_STATIC_PACKET(a25x_te_aod_setting, DSI_PKT_TYPE_WR, A25X_TE_AOD_SETTING, 0);

static u8 A25X_TSET_SET[] = {
	0xB5,
	0x00,
};
static DEFINE_PKTUI(a25x_tset_set, &a25x_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_tset_set, DSI_PKT_TYPE_WR, A25X_TSET_SET, 0x00);

static u8 A25X_AOD_VAINT[] = { 0xF4, 0x28 };
static DEFINE_STATIC_PACKET(a25x_aod_vaint, DSI_PKT_TYPE_WR, A25X_AOD_VAINT, 0x4C);

static u8 A25X_AOD_SETTING[] = { 0x91, 0x01, 0x01 };
static DEFINE_STATIC_PACKET(a25x_aod_setting, DSI_PKT_TYPE_WR, A25X_AOD_SETTING, 0x0);

static u8 A25X_LPM_PORCH_ON[] = { 0xF2, 0x25, 0x60 };
static DEFINE_STATIC_PACKET(a25x_lpm_porch_on, DSI_PKT_TYPE_WR, A25X_LPM_PORCH_ON, 0x10);

static u8 A25X_SWIRE_NO_PULSE[] = { 0xB5, 0x00 };
static DEFINE_STATIC_PACKET(a25x_swire_no_pulse, DSI_PKT_TYPE_WR, A25X_SWIRE_NO_PULSE, 0x0A);

static u8 A25X_SWIRE_NO_PULSE_LPM_OFF[] = { 0xB5, 0x00, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a25x_swire_no_pulse_lpm_off, DSI_PKT_TYPE_WR, A25X_SWIRE_NO_PULSE_LPM_OFF, 0x07);

static u8 A25X_LPM_OFF_SYNC_CTRL[] = {
	0x63, 0x20
};
static DEFINE_STATIC_PACKET(a25x_lpm_off_sync_ctrl, DSI_PKT_TYPE_WR, A25X_LPM_OFF_SYNC_CTRL, 0x91);

static u8 A25X_LPM_PORCH_OFF[] = { 0xF2, 0x27, 0xE0 };
static DEFINE_STATIC_PACKET(a25x_lpm_porch_off, DSI_PKT_TYPE_WR, A25X_LPM_PORCH_OFF, 0x10);

static u8 A25X_HW_CODE[] = { 0xF2, 0x26, 0xF0 };
static DEFINE_STATIC_PACKET(a25x_hw_code, DSI_PKT_TYPE_WR, A25X_HW_CODE, 0x10);

static u8 A25X_NORMAL_SETTING[] = { 0x91, 0x02, 0x01 };
static DEFINE_STATIC_PACKET(a25x_normal_setting, DSI_PKT_TYPE_WR, A25X_NORMAL_SETTING, 0x00);

static u8 A25X_LPM_CTRL[] = { 0x63, 0x01, 0x09, 0x20, 0x02 };
static DEFINE_STATIC_PACKET(a25x_lpm_ctrl, DSI_PKT_TYPE_WR, A25X_LPM_CTRL, 0x76);

static u8 A25X_LPM_NIT[] = { 0x53, 0x27 };
static DEFINE_PKTUI(a25x_lpm_nit, &a25x_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_lpm_nit, DSI_PKT_TYPE_WR, A25X_LPM_NIT, 0x00);

static u8 A25X_LPM_AOR[] = { 0x63, 0x00, 0x00, 0x18, 0x02 };
static DEFINE_STATIC_PACKET(a25x_lpm_aor, DSI_PKT_TYPE_WR, A25X_LPM_AOR, 0x76);

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 A25X_DECODER_TEST_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static DEFINE_STATIC_PACKET(a25x_decoder_test_caset, DSI_PKT_TYPE_WR, A25X_DECODER_TEST_CASET, 0x00);

static u8 A25X_DECODER_TEST_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };
static DEFINE_STATIC_PACKET(a25x_decoder_test_paset, DSI_PKT_TYPE_WR, A25X_DECODER_TEST_PASET, 0x00);

static u8 A25X_DECODER_TEST_2C[] = { 0x2C, 0x00 };
static DEFINE_STATIC_PACKET(a25x_decoder_test_2c, DSI_PKT_TYPE_WR, A25X_DECODER_TEST_2C, 0x00);

static u8 A25X_DECODER_CRC_PATTERN_ENABLE[] = { 0xBE, 0x07 };
static DEFINE_STATIC_PACKET(a25x_decoder_crc_pattern_enable, DSI_PKT_TYPE_WR, A25X_DECODER_CRC_PATTERN_ENABLE, 0x00);

static u8 A25X_DECODER_READ_SET_1[] = { 0xD8, 0x11 };
static DEFINE_STATIC_PACKET(a25x_decoder_read_set_1, DSI_PKT_TYPE_WR, A25X_DECODER_READ_SET_1, 0x27);

static u8 A25X_DECODER_READ_SET_2[] = { 0xD8, 0x20 };
static DEFINE_STATIC_PACKET(a25x_decoder_read_set_2, DSI_PKT_TYPE_WR, A25X_DECODER_READ_SET_2, 0x27);

static u8 A25X_DECODER_VDDM_LOW_SET_1[] = { 0xD7, 0x04 };
static DEFINE_STATIC_PACKET(a25x_decoder_vddm_low_set_1, DSI_PKT_TYPE_WR, A25X_DECODER_VDDM_LOW_SET_1, 0x02);

static u8 A25X_DECODER_VDDM_LOW_SET_2[] = { 0xF4, 0x07 };
static DEFINE_STATIC_PACKET(a25x_decoder_vddm_low_set_2, DSI_PKT_TYPE_WR, A25X_DECODER_VDDM_LOW_SET_2, 0x0F);

static u8 A25X_DECODER_FUSING_UPDATE_1[] = { 0xFE, 0xB0 };
static DEFINE_STATIC_PACKET(a25x_decoder_fusing_update_1, DSI_PKT_TYPE_WR, A25X_DECODER_FUSING_UPDATE_1, 0x00);

static u8 A25X_DECODER_FUSING_UPDATE_2[] = { 0xFE, 0x30 };
static DEFINE_STATIC_PACKET(a25x_decoder_fusing_update_2, DSI_PKT_TYPE_WR, A25X_DECODER_FUSING_UPDATE_2, 0x0F);

static u8 A25X_DECODER_CRC_PATERN_DISABLE[] = { 0xBE, 0x00 };
static DEFINE_STATIC_PACKET(a25x_decoder_crc_pattern_disable, DSI_PKT_TYPE_WR, A25X_DECODER_CRC_PATERN_DISABLE, 0x00);

static u8 A25X_DECODER_VDDM_RETURN_SET_1[] = { 0xD7, 0x00 };
static DEFINE_STATIC_PACKET(a25x_decoder_vddm_return_set_1, DSI_PKT_TYPE_WR, A25X_DECODER_VDDM_RETURN_SET_1, 0x02);

static u8 A25X_DECODER_VDDM_RETURN_SET_2[] = { 0xF4, 0x00 };
static DEFINE_STATIC_PACKET(a25x_decoder_vddm_return_set_2, DSI_PKT_TYPE_WR, A25X_DECODER_VDDM_RETURN_SET_2, 0x0F);
#endif

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static DEFINE_PANEL_MDELAY(a25x_wait_9msec, 9);
static DEFINE_PANEL_MDELAY(a25x_wait_7msec, 7);
#endif

static DEFINE_PANEL_MDELAY(a25x_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a25x_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a25x_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(a25x_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(a25x_wait_20msec, 20);

static DEFINE_PANEL_MDELAY(a25x_wait_dsc_test_100msec, 100);
static DEFINE_PANEL_MDELAY(a25x_wait_dsc_test_20msec, 20);

static DEFINE_PANEL_MDELAY(a25x_wait_34msec, 34);

static DEFINE_PANEL_MDELAY(a25x_wait_sleep_out_30msec, 30);
static DEFINE_PANEL_MDELAY(a25x_wait_display_off, 20);

static DEFINE_PANEL_MDELAY(a25x_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a25x_wait_1usec, 1);

static DEFINE_PANEL_FRAME_DELAY(a25x_wait_1_frame, 1);

static DEFINE_PANEL_KEY(a25x_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a25x_level1_key_enable));
static DEFINE_PANEL_KEY(a25x_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a25x_level2_key_enable));
static DEFINE_PANEL_KEY(a25x_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a25x_level3_key_enable));

static DEFINE_PANEL_KEY(a25x_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a25x_level1_key_disable));
static DEFINE_PANEL_KEY(a25x_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a25x_level2_key_disable));
static DEFINE_PANEL_KEY(a25x_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a25x_level3_key_disable));

static u8 A25X_HBM_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(a25x_hbm_transition, &a25x_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_hbm_transition, DSI_PKT_TYPE_WR, A25X_HBM_TRANSITION, 0);

static u8 A25X_ACL_SET[] = {
	0x65,
	0x55, 0x00, 0xB0, 0x51, 0x66, 0x98, 0x15, 0x55,
	0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48, 0x40, 0x00,
	0x20, 0x10, 0x09
};
static DECLARE_PKTUI(a25x_acl_set) = {
	{ .offset = 1, .maptbl = &a25x_maptbl[ACL_FRAME_AVG_MAPTBL] },
	{ .offset = 2, .maptbl = &a25x_maptbl[ACL_START_POINT_MAPTBL] },
	{ .offset = 17, .maptbl = &a25x_maptbl[ACL_DIM_SPEED_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a25x_acl_set, DSI_PKT_TYPE_WR, A25X_ACL_SET, 0x3B3);

static u8 A25X_ACL[] = {
	0x55, 0x01
};
static DEFINE_PKTUI(a25x_acl_control, &a25x_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_acl_control, DSI_PKT_TYPE_WR, A25X_ACL, 0);

static u8 A25X_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(a25x_wrdisbv, &a25x_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_wrdisbv, DSI_PKT_TYPE_WR, A25X_WRDISBV, 0);

static u8 A25X_IRC_MDOE[] = {
	0x8F,
	0x27, 0x65, 0xFC, 0x47, 0x00, 0x80, 0x01
};
static DEFINE_PKTUI(a25x_irc_mode, &a25x_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_irc_mode, DSI_PKT_TYPE_WR, A25X_IRC_MDOE, 0x02);

static u8 A25X_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 A25X_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };
static DEFINE_STATIC_PACKET(a25x_caset, DSI_PKT_TYPE_WR, A25X_CASET, 0);
static DEFINE_STATIC_PACKET(a25x_paset, DSI_PKT_TYPE_WR, A25X_PASET, 0);

static u8 A25X_PCD_SET_DET_LOW[] = {
	0xCC,
	0x5C, 0x51	/* 1st 0x5C: default high, 2nd 0x51 : Enable SW RESET */
};
static DEFINE_STATIC_PACKET(a25x_pcd_setting, DSI_PKT_TYPE_WR, A25X_PCD_SET_DET_LOW, 0);

static u8 A25X_ERR_FG_ON[] = {
	0xE5, 0x15
};
static DEFINE_STATIC_PACKET(a25x_err_fg_on, DSI_PKT_TYPE_WR, A25X_ERR_FG_ON, 0);

static u8 A25X_ERR_FG_OFF[] = {
	0xE5, 0x05
};
static DEFINE_STATIC_PACKET(a25x_err_fg_off, DSI_PKT_TYPE_WR, A25X_ERR_FG_OFF, 0);

static u8 A25X_ERR_FG_SETTING[] = {
	0xED,
	0x04, 0x4C, 0x20
	/* Vlin1, ELVDD, Vlin3 Monitor On */
	/* Defalut LOW */
};
static DEFINE_STATIC_PACKET(a25x_err_fg_setting, DSI_PKT_TYPE_WR, A25X_ERR_FG_SETTING, 0);

static u8 A25X_VSYNC_SET[] = {
	0xF2,
	0x54
};
static DEFINE_STATIC_PACKET(a25x_vsync_set, DSI_PKT_TYPE_WR, A25X_VSYNC_SET, 0x04);

static u8 A25X_FREQ_SET[] = {
	0xF2,
	0x00
};
static DEFINE_STATIC_PACKET(a25x_freq_set, DSI_PKT_TYPE_WR, A25X_FREQ_SET, 0x27);

static u8 A25X_PORCH_SET[] = {
	0xF2,
	0x55
};
static DEFINE_STATIC_PACKET(a25x_porch_set, DSI_PKT_TYPE_WR, A25X_PORCH_SET, 0x2E);

static u8 A25X_FPS_1[] = { 0x60, 0x08, 0x00 };
static DEFINE_PKTUI(a25x_fps_1, &a25x_maptbl[FPS_MAPTBL_1], 1);
static DEFINE_VARIABLE_PACKET(a25x_fps_1, DSI_PKT_TYPE_WR, A25X_FPS_1, 0);

static u8 A25X_FPS_2[] = { 0x68, 0x02, };
static DEFINE_PKTUI(a25x_fps_2, &a25x_maptbl[FPS_MAPTBL_2], 1);
static DEFINE_VARIABLE_PACKET(a25x_fps_2, DSI_PKT_TYPE_WR, A25X_FPS_2, 0x28);

static u8 A25X_DIMMING_SPEED[] = { 0x63, 0x60 };
static DEFINE_PKTUI(a25x_dimming_speed, &a25x_maptbl[DIMMING_SPEED], 1);
static DEFINE_VARIABLE_PACKET(a25x_dimming_speed, DSI_PKT_TYPE_WR, A25X_DIMMING_SPEED, 0x91);

static u8 A25X_PANEL_UPDATE[] = {
	0xF7,
	0x0F
};
static DEFINE_STATIC_PACKET(a25x_panel_update, DSI_PKT_TYPE_WR, A25X_PANEL_UPDATE, 0);

static u8 A25X_GLOBAL_PARAM_SETTING[] = {
	0xF2,
	0x60, 0x05, 0x0E, 0x49, 0x54, 0x01, 0x10, 0x00,
	0x0C, 0x27, 0xE0, 0x27, 0xE0, 0x10, 0x09, 0x4C,
	0x27, 0xE0, 0x10, 0x00, 0x0C, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};
static DEFINE_STATIC_PACKET(a25x_global_param_setting, DSI_PKT_TYPE_WR, A25X_GLOBAL_PARAM_SETTING, 0);

static u8 A25X_CRC_ENABLE[] = { 0x6A, 0x01 };
static DEFINE_STATIC_PACKET(a25x_crc_enable, DSI_PKT_TYPE_WR, A25X_CRC_ENABLE, 0x06);

static u8 A25X_FOD_ENTER[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a25x_fod_enter, DSI_PKT_TYPE_WR, A25X_FOD_ENTER, 0);

static u8 A25X_FOD_EXIT[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a25x_fod_exit, DSI_PKT_TYPE_WR, A25X_FOD_EXIT, 0);

static u8 A25X_TIG_ENABLE[] = {
	0xBF, 0x01, 0x00
};
static DEFINE_STATIC_PACKET(a25x_tig_enable, DSI_PKT_TYPE_WR, A25X_TIG_ENABLE, 0);

static u8 A25X_TIG_DISABLE[] = {
	0xBF, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(a25x_tig_disable, DSI_PKT_TYPE_WR, A25X_TIG_DISABLE, 0);

static u8 A25X_DSC[] = { 0x01 };
static DEFINE_STATIC_PACKET(a25x_dsc, DSI_PKT_TYPE_WR_COMP, A25X_DSC, 0);

static u8 A25X_PPS[] = {
	//1080x2340 Slice Info : 540x30
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
	0x04, 0x38, 0x00, 0x1E, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x02, 0xE3,
	0x00, 0x07, 0x00, 0x0C, 0x03, 0x50, 0x03, 0x64,
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
static DEFINE_STATIC_PACKET(a25x_pps, DSI_PKT_TYPE_WR_PPS, A25X_PPS, 0);

static u8 A25X_NORMAL_MODE[] = { 0x53, 0x20 };
static DEFINE_PKTUI(a25x_normal_mode, &a25x_maptbl[LPM_EXIT_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_normal_mode, DSI_PKT_TYPE_WR, A25X_NORMAL_MODE, 0x00);

static u8 A25X_FFC_SETTING_1[] = {
	0xC5,
	0x0D, 0x10, 0x80, 0x05
};
static DEFINE_PKTUI(a25x_ffc_setting_1, &a25x_maptbl[SET_FFC_MAPTBL], 1);
static DEFINE_STATIC_PACKET(a25x_ffc_setting_1, DSI_PKT_TYPE_WR, A25X_FFC_SETTING_1, 0x2A);

static u8 A25X_FFC_SETTING_2[] = {
	0xC5,
	0x73, 0x2B
};
static DEFINE_PKTUI(a25x_ffc_setting_2, &a25x_maptbl[SET_FFC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a25x_ffc_setting_2, DSI_PKT_TYPE_WR, A25X_FFC_SETTING_2, 0x2E);


static u8 SEQ_S6E3FC3_A25X_120HZ_SET_1[] = {
	0x65,
	0x09, 0x40, 0x08, 0xCA, 0x08, 0xCA, 0x08,
	0xCA, 0x08, 0x6C, 0x08, 0x0E, 0x07, 0x54,
	0x06, 0x98, 0x05, 0xDC, 0x05, 0x22, 0x04,
	0x66, 0x03, 0xAC, 0x02, 0x9A, 0x01, 0x7A,
	0x01, 0x7A, 0x00, 0x18
};
static DEFINE_STATIC_PACKET(a25x_120hz_set_1, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_A25X_120HZ_SET_1, 0x1B4);

static u8 SEQ_S6E3FC3_A25X_120HZ_FREQ_1[] = {
	0x60,
	0x08, 0x00
};
static DEFINE_STATIC_PACKET(a25x_120hz_freq_1, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_A25X_120HZ_FREQ_1, 0);

static u8 SEQ_S6E3FC3_A25X_120HZ_FREQ_2[] = {
	0xF2, 0x0C
};
static DEFINE_STATIC_PACKET(a25x_120hz_freq_2, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_A25X_120HZ_FREQ_2, 0x8);

static u8 SEQ_S6E3FC3_A25X_120HZ_FREQ_3[] = {
	0x6A,
	0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(a25x_120hz_freq_3, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_A25X_120HZ_FREQ_3, 0x2A);

static u8 SEQ_S6E3FC3_A25X_120HZ_FREQ_4[] = {
	0x68, 0x02
};
static DEFINE_STATIC_PACKET(a25x_120hz_freq_4, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_A25X_120HZ_FREQ_4, 0x28);

static u8 SEQ_S6E3FC3_FD_ENABLE[] = {
	0xB5, 0x40, 0x60
};
static DEFINE_STATIC_PACKET(a25x_fd_enable, DSI_PKT_TYPE_WR, SEQ_S6E3FC3_FD_ENABLE, 0x0A);

static u8 A25X_BLACK_FRAME_OFF[] = { 0xBB, 0x31 };
static DEFINE_STATIC_PACKET(a25x_black_frame_off, DSI_PKT_TYPE_WR, A25X_BLACK_FRAME_OFF, 0x00);

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 A25X_CCD_ENABLE[] = {
	0xCC,
	0x5C, 0x51, 0x01
};

static u8 A25X_CCD_DISABLE[] = {
	0xCC,
	0x5C, 0x51, 0x02
};
static DEFINE_STATIC_PACKET(a25x_ccd_test_enable, DSI_PKT_TYPE_WR, A25X_CCD_ENABLE, 0x00);
static DEFINE_STATIC_PACKET(a25x_ccd_test_disable, DSI_PKT_TYPE_WR, A25X_CCD_DISABLE, 0x00);
#endif

static DEFINE_PANEL_TIMER_MDELAY(a25x_init_complete_delay, 90);
static DEFINE_PANEL_TIMER_BEGIN(a25x_init_complete_delay,
		TIMER_DLYINFO(&a25x_init_complete_delay));

static struct seqinfo SEQINFO(a25x_set_bl_param_seq);
static struct seqinfo SEQINFO(a25x_set_fps_param_seq);
#if defined(CONFIG_USDM_FACTORY)
static struct seqinfo SEQINFO(a25x_res_init_seq);
#endif

static DEFINE_PNOBJ_CONFIG(a25x_set_separate_tx_off, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_OFF);
static DEFINE_PNOBJ_CONFIG(a25x_set_separate_tx_on, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_ON);

static void *a25x_common_setting_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),

	&PKTINFO(a25x_global_param_setting),
	&PKTINFO(a25x_panel_update),

	&DLYINFO(a25x_wait_1msec),

	&PKTINFO(a25x_crc_enable),

	&PKTINFO(a25x_te_on),
	&PKTINFO(a25x_te_setting),

	&PKTINFO(a25x_caset),
	&PKTINFO(a25x_paset),

	&PKTINFO(a25x_ffc_setting_1),
	&PKTINFO(a25x_ffc_setting_2),

	&PKTINFO(a25x_err_fg_on),
	&PKTINFO(a25x_err_fg_setting),

	&PKTINFO(a25x_vsync_set),
	&PKTINFO(a25x_pcd_setting),

	&PKTINFO(a25x_acl_set),
	&PKTINFO(a25x_panel_update),

	&PKTINFO(a25x_dsc),
	&PKTINFO(a25x_pps),

	&PKTINFO(a25x_freq_set),
	&PKTINFO(a25x_panel_update),

	&PKTINFO(a25x_porch_set),
	&PKTINFO(a25x_panel_update),

	/* 120Hz setting */
	&DLYINFO(a25x_wait_20msec),
	&PKTINFO(a25x_120hz_set_1),
	&PKTINFO(a25x_120hz_freq_1),
	&PKTINFO(a25x_120hz_freq_2),
	&DLYINFO(a25x_wait_20msec),
	&PKTINFO(a25x_120hz_freq_3),
	&PKTINFO(a25x_panel_update),
	&DLYINFO(a25x_wait_20msec),
	&PKTINFO(a25x_120hz_freq_4),
	&PKTINFO(a25x_panel_update),

#if defined(CONFIG_USDM_FACTORY)
	&PKTINFO(a25x_fd_enable),
#else
	&PKTINFO(a25x_swire_no_pulse),
#endif
	&PKTINFO(a25x_panel_update),

	&PKTINFO(a25x_hbm_transition), /* 53h should be not included in bl_seq */
	&SEQINFO(a25x_set_bl_param_seq), /* includes FPS setting also */

	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static DEFINE_SEQINFO(a25x_common_setting_seq,
		a25x_common_setting_cmdtbl);

static void *a25x_init_cmdtbl[] = {
	&DLYINFO(a25x_wait_1msec),

	&PKTINFO(a25x_sleep_out),
	&DLYINFO(a25x_wait_sleep_out_30msec),

	&SEQINFO(a25x_common_setting_seq),

	&TIMER_DLYINFO_BEGIN(a25x_init_complete_delay),
#if defined(CONFIG_USDM_FACTORY)
	&SEQINFO(a25x_res_init_seq),
#endif
	&TIMER_DLYINFO(a25x_init_complete_delay),
};

static void *a25x_res_init_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&s6e3fc3_restbl[RES_ID],
	&s6e3fc3_restbl[RES_COORDINATE],
	&s6e3fc3_restbl[RES_CODE],
	&s6e3fc3_restbl[RES_DATE],
	&s6e3fc3_restbl[RES_OCTA_ID],
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fc3_restbl[RES_SELF_DIAG],
	&s6e3fc3_restbl[RES_ERR_FG],
	&s6e3fc3_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static void *a25x_pcd_dump_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&s6e3fc3_dmptbl[DUMP_PCD],
	&s6e3fc3_dmptbl[DUMP_RDDPM],
	&s6e3fc3_dmptbl[DUMP_RDDSM],
	&s6e3fc3_dmptbl[DUMP_ERR],
	&s6e3fc3_dmptbl[DUMP_ERR_FG],
	&s6e3fc3_dmptbl[DUMP_DSI_ERR],
	&s6e3fc3_dmptbl[DUMP_SELF_DIAG],
	&s6e3fc3_dmptbl[DUMP_PCD2],
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

#if defined(CONFIG_USDM_FACTORY)
static DEFINE_SEQINFO(a25x_res_init_seq, a25x_res_init_cmdtbl);
#endif

/* bl for normal */
static void *a25x_set_bl_param_cmdtbl[] = {
	&PKTINFO(a25x_fps_1),
	&PKTINFO(a25x_fps_2),
	&PKTINFO(a25x_dimming_speed),
	&PKTINFO(a25x_tset_set),
	&PKTINFO(a25x_panel_update),
	&PKTINFO(a25x_acl_control),
	&PKTINFO(a25x_wrdisbv),
	&PKTINFO(a25x_irc_mode),
	&PKTINFO(a25x_panel_update),
};

static DEFINE_SEQINFO(a25x_set_bl_param_seq, a25x_set_bl_param_cmdtbl);

static void *a25x_set_bl_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&PKTINFO(a25x_hbm_transition),
	&SEQINFO(a25x_set_bl_param_seq),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static DEFINE_RULE_BASED_COND(a25x_cond_is_panel_state_not_lpm,
		PANEL_PROPERTY_PANEL_STATE, NE, PANEL_STATE_ALPM);
static DEFINE_RULE_BASED_COND(a25x_cond_is_panel_state_lpm,
		PANEL_PROPERTY_PANEL_STATE, EQ, PANEL_STATE_ALPM);

static void *a25x_set_fps_cmdtbl[] = {
	&CONDINFO_IF(a25x_cond_is_panel_state_not_lpm),
		&KEYINFO(a25x_level1_key_enable),
		&KEYINFO(a25x_level2_key_enable),
		&KEYINFO(a25x_level3_key_enable),
		&PKTINFO(a25x_hbm_transition),
		&SEQINFO(a25x_set_bl_param_seq),
		&KEYINFO(a25x_level3_key_disable),
		&KEYINFO(a25x_level2_key_disable),
		&KEYINFO(a25x_level1_key_disable),
	&CONDINFO_FI(a25x_cond_is_panel_state_not_lpm),
};

static void *a25x_display_on_cmdtbl[] = {
	&CONDINFO_IF(a25x_cond_is_panel_state_lpm),
		&KEYINFO(a25x_level2_key_enable),
		&PKTINFO(a25x_tig_enable),
		&KEYINFO(a25x_level2_key_disable),
		&DLYINFO(a25x_wait_17msec),
	&CONDINFO_FI(a25x_cond_is_panel_state_lpm),
	&KEYINFO(a25x_level1_key_enable),
	&PKTINFO(a25x_display_on),
	&KEYINFO(a25x_level1_key_disable),
	&CONDINFO_IF(a25x_cond_is_panel_state_lpm),
		&DLYINFO(a25x_wait_17msec),
		&KEYINFO(a25x_level2_key_enable),
		&PKTINFO(a25x_tig_disable),
		&KEYINFO(a25x_level2_key_disable),
	&CONDINFO_FI(a25x_cond_is_panel_state_lpm),
};

static void *a25x_display_off_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&PKTINFO(a25x_display_off),
	&PKTINFO(a25x_err_fg_off),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
	&DLYINFO(a25x_wait_display_off),
};

static void *a25x_exit_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fc3_dmptbl[DUMP_RDDPM_SLEEP_IN],
	&s6e3fc3_dmptbl[DUMP_RDDSM],
	&s6e3fc3_dmptbl[DUMP_ERR],
	&s6e3fc3_dmptbl[DUMP_DSI_ERR],
	&s6e3fc3_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(a25x_sleep_in),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
	&DLYINFO(a25x_wait_sleep_in),
};

static void *a25x_alpm_set_bl_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),

	&PKTINFO(a25x_te_aod_setting),
	&PKTINFO(a25x_black_frame_off),

	&DLYINFO(a25x_wait_20msec),

	&PKTINFO(a25x_aod_setting),
	&PKTINFO(a25x_lpm_porch_on),
	&PKTINFO(a25x_lpm_nit),

	&PKTINFO(a25x_panel_update),

	&DLYINFO(a25x_wait_1usec),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static void *a25x_alpm_exit_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),

	&PKTINFO(a25x_wrdisbv),
	&PKTINFO(a25x_te_setting),
	&PKTINFO(a25x_black_frame_off),

	&DLYINFO(a25x_wait_34msec),

	&PKTINFO(a25x_swire_no_pulse_lpm_off),
	&PKTINFO(a25x_lpm_porch_off),
	&PKTINFO(a25x_normal_mode),
	&PKTINFO(a25x_wrdisbv),
	&PKTINFO(a25x_panel_update),

	&DLYINFO(a25x_wait_34msec),

	&PKTINFO(a25x_normal_setting),
	&PKTINFO(a25x_120hz_freq_1),
	&PKTINFO(a25x_120hz_freq_4),
	&PKTINFO(a25x_panel_update),

	&DLYINFO(a25x_wait_1usec),

	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};


static void *a25x_alpm_exit_after_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&PKTINFO(a25x_hbm_transition),
	&SEQINFO(a25x_set_bl_param_seq),
	&DLYINFO(a25x_wait_1usec),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static void *a25x_dump_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&s6e3fc3_dmptbl[DUMP_RDDPM],
	&s6e3fc3_dmptbl[DUMP_RDDSM],
	&s6e3fc3_dmptbl[DUMP_DSI_ERR],
	&s6e3fc3_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static void *a25x_ffc_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&PKTINFO(a25x_ffc_setting_1),
	&PKTINFO(a25x_ffc_setting_2),
	&PKTINFO(a25x_panel_update),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};

static void *a25x_check_condition_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&s6e3fc3_dmptbl[DUMP_RDDPM],
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};


#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static void *a25x_decoder_test_cmdtbl[] = {
	&KEYINFO(a25x_level1_key_enable),
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&PKTINFO(a25x_decoder_test_caset),
	&PKTINFO(a25x_decoder_test_paset),
	&PKTINFO(a25x_decoder_test_2c),
	&PKTINFO(a25x_decoder_crc_pattern_enable),
	&PKTINFO(a25x_decoder_read_set_1),
	&DLYINFO(a25x_wait_dsc_test_100msec),
	&s6e3fc3_restbl[RES_DECODER_TEST1],
	&PKTINFO(a25x_decoder_read_set_2),
	&DLYINFO(a25x_wait_dsc_test_100msec),
	&s6e3fc3_restbl[RES_DECODER_TEST2],
	&PKTINFO(a25x_decoder_vddm_low_set_1),
	&PKTINFO(a25x_decoder_vddm_low_set_2),
	&PKTINFO(a25x_decoder_fusing_update_1),
	&PKTINFO(a25x_decoder_fusing_update_2),
	&PKTINFO(a25x_decoder_crc_pattern_enable),
	&PKTINFO(a25x_decoder_read_set_1),
	&DLYINFO(a25x_wait_dsc_test_20msec),
	&s6e3fc3_restbl[RES_DECODER_TEST3],
	&PKTINFO(a25x_decoder_read_set_2),
	&DLYINFO(a25x_wait_dsc_test_20msec),
	&s6e3fc3_restbl[RES_DECODER_TEST4],
	&PKTINFO(a25x_decoder_crc_pattern_disable),
	&PKTINFO(a25x_decoder_vddm_return_set_1),
	&PKTINFO(a25x_decoder_vddm_return_set_2),
	&PKTINFO(a25x_decoder_fusing_update_1),
	&PKTINFO(a25x_decoder_fusing_update_2),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
	&KEYINFO(a25x_level1_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static void *a25x_ccd_test_cmdtbl[] = {
	&KEYINFO(a25x_level2_key_enable),
	&KEYINFO(a25x_level3_key_enable),
	&PKTINFO(a25x_ccd_test_enable),
	&DLYINFO(a25x_wait_20msec),
	&s6e3fc3_restbl[RES_CCD_STATE],
	&PKTINFO(a25x_ccd_test_disable),
	&KEYINFO(a25x_level3_key_disable),
	&KEYINFO(a25x_level2_key_disable),
};
#endif

static void *a25x_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo a25x_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, a25x_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, a25x_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, a25x_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, a25x_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, a25x_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, a25x_exit_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, a25x_set_fps_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_SET_BL_SEQ, a25x_alpm_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_SEQ, a25x_alpm_exit_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_AFTER_SEQ, a25x_alpm_exit_after_cmdtbl),
	SEQINFO_INIT(PANEL_FFC_SEQ, a25x_ffc_cmdtbl),
	SEQINFO_INIT(PANEL_DUMP_SEQ, a25x_dump_cmdtbl),
	SEQINFO_INIT(PANEL_CHECK_CONDITION_SEQ, a25x_check_condition_cmdtbl),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	SEQINFO_INIT(PANEL_DECODER_TEST_SEQ, a25x_decoder_test_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	SEQINFO_INIT(PANEL_CCD_TEST_SEQ, a25x_ccd_test_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_PCD_DUMP_SEQ, a25x_pcd_dump_cmdtbl),
	SEQINFO_INIT(PANEL_DUMMY_SEQ, a25x_dummy_cmdtbl),
};

struct common_panel_info s6e3fc3_a25x_panel_info = {
	.ldi_name = "s6e3fc3",
	.name = "s6e3fc3_a25x",
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
		.get_cell_id = s6e3fc3_get_cell_id,
		.get_octa_id = s6e3fc3_get_octa_id,
		.get_manufacture_code = s6e3fc3_get_manufacture_code,
		.get_manufacture_date = s6e3fc3_get_manufacture_date,
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
		.decoder_test = s6e3fc3_a25x_decoder_test,
#endif
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3fc3_a25x_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fc3_a25x_default_resol),
		.resol = s6e3fc3_a25x_default_resol,
	},
	.vrrtbl = s6e3fc3_a25x_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3fc3_a25x_default_vrrtbl),
	.maptbl = a25x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a25x_maptbl),
	.seqtbl = a25x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a25x_seqtbl),
	.rditbl = s6e3fc3_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fc3_rditbl),
	.restbl = s6e3fc3_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fc3_restbl),
	.dumpinfo = s6e3fc3_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fc3_dmptbl),

#ifdef CONFIG_USDM_MDNIE
	.mdnie_tune = &s6e3fc3_a25x_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fc3_a25x_panel_dimming_info,
#ifdef CONFIG_USDM_PANEL_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fc3_a25x_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	.aod_tune = &s6e3fc3_a25x_aod,
#endif
};

#endif /* __S6E3FC3_A25X_PANEL_H__ */
