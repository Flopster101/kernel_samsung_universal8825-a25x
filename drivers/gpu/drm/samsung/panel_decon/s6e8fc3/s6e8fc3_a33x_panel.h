/*
 * linux/drivers/video/fbdev/exynos/panel/s6e8fc3/s6e8fc3_a33x_panel.h
 *
 * Header file for S6E8FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E8FC3_A33X_PANEL_H__
#define __S6E8FC3_A33X_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "s6e8fc3.h"
#include "s6e8fc3_a33x.h"
#include "s6e8fc3_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e8fc3_a33x_panel_mdnie.h"
#endif
#include "s6e8fc3_a33x_panel_dimming.h"

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e8fc3_a33x_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e8fc3_a33x_resol.h"

#undef __pn_name__
#define __pn_name__	a33x

#undef __PN_NAME__
#define __PN_NAME__

/* ===================================================================================== */
/* ============================= [S6E8FC3 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E8FC3 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E8FC3 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a33x_brt_table[S6E8FC3_TOTAL_STEP][2] = {
	{0x00, 0x02}, {0x00, 0x03}, {0x00, 0x04}, {0x00, 0x05}, {0x00, 0x07}, {0x00, 0x08}, {0x00, 0x0A}, {0x00, 0x0C}, {0x00, 0x0E}, {0x00, 0x0F},
	{0x00, 0x11}, {0x00, 0x13}, {0x00, 0x16}, {0x00, 0x18}, {0x00, 0x1A}, {0x00, 0x1C}, {0x00, 0x1E}, {0x00, 0x21}, {0x00, 0x23}, {0x00, 0x26},
	{0x00, 0x28}, {0x00, 0x2B}, {0x00, 0x2D}, {0x00, 0x30}, {0x00, 0x32}, {0x00, 0x35}, {0x00, 0x38}, {0x00, 0x3A}, {0x00, 0x3D}, {0x00, 0x40},
	{0x00, 0x42}, {0x00, 0x45}, {0x00, 0x48}, {0x00, 0x4B}, {0x00, 0x4E}, {0x00, 0x51}, {0x00, 0x54}, {0x00, 0x57}, {0x00, 0x5A}, {0x00, 0x5D},
	{0x00, 0x60}, {0x00, 0x63}, {0x00, 0x66}, {0x00, 0x69}, {0x00, 0x6C}, {0x00, 0x6F}, {0x00, 0x72}, {0x00, 0x76}, {0x00, 0x79}, {0x00, 0x7C},
	{0x00, 0x7F}, {0x00, 0x83}, {0x00, 0x86}, {0x00, 0x89}, {0x00, 0x8C}, {0x00, 0x90}, {0x00, 0x93}, {0x00, 0x97}, {0x00, 0x9A}, {0x00, 0x9D},
	{0x00, 0xA1}, {0x00, 0xA4}, {0x00, 0xA8}, {0x00, 0xAB}, {0x00, 0xAF}, {0x00, 0xB2}, {0x00, 0xB6}, {0x00, 0xB9}, {0x00, 0xBD}, {0x00, 0xC0},
	{0x00, 0xC4}, {0x00, 0xC8}, {0x00, 0xCB}, {0x00, 0xCF}, {0x00, 0xD3}, {0x00, 0xD6}, {0x00, 0xDA}, {0x00, 0xDE}, {0x00, 0xE1}, {0x00, 0xE5},
	{0x00, 0xE9}, {0x00, 0xED}, {0x00, 0xF0}, {0x00, 0xF4}, {0x00, 0xF8}, {0x00, 0xFC}, {0x01, 0x00}, {0x01, 0x03}, {0x01, 0x07}, {0x01, 0x0B},
	{0x01, 0x0F}, {0x01, 0x13}, {0x01, 0x17}, {0x01, 0x1B}, {0x01, 0x1F}, {0x01, 0x23}, {0x01, 0x26}, {0x01, 0x2A}, {0x01, 0x2E}, {0x01, 0x32},
	{0x01, 0x36}, {0x01, 0x3A}, {0x01, 0x3E}, {0x01, 0x43}, {0x01, 0x47}, {0x01, 0x4B}, {0x01, 0x4F}, {0x01, 0x53}, {0x01, 0x57}, {0x01, 0x5B},
	{0x01, 0x5F}, {0x01, 0x63}, {0x01, 0x67}, {0x01, 0x6C}, {0x01, 0x70}, {0x01, 0x74}, {0x01, 0x78}, {0x01, 0x7C}, {0x01, 0x80}, {0x01, 0x85},
	{0x01, 0x89}, {0x01, 0x8D}, {0x01, 0x91}, {0x01, 0x96}, {0x01, 0x9A}, {0x01, 0x9E}, {0x01, 0xA3}, {0x01, 0xA7}, {0x01, 0xAB}, {0x01, 0xAF},
	{0x01, 0xB4}, {0x01, 0xB8}, {0x01, 0xBD}, {0x01, 0xC1}, {0x01, 0xC5}, {0x01, 0xC9}, {0x01, 0xCE}, {0x01, 0xD2}, {0x01, 0xD6}, {0x01, 0xDA},
	{0x01, 0xDF}, {0x01, 0xE3}, {0x01, 0xE7}, {0x01, 0xEC}, {0x01, 0xF0}, {0x01, 0xF4}, {0x01, 0xF9}, {0x01, 0xFD}, {0x02, 0x01}, {0x02, 0x06},
	{0x02, 0x0A}, {0x02, 0x0E}, {0x02, 0x13}, {0x02, 0x17}, {0x02, 0x1C}, {0x02, 0x20}, {0x02, 0x24}, {0x02, 0x29}, {0x02, 0x2D}, {0x02, 0x32},
	{0x02, 0x36}, {0x02, 0x3B}, {0x02, 0x3F}, {0x02, 0x44}, {0x02, 0x48}, {0x02, 0x4D}, {0x02, 0x51}, {0x02, 0x56}, {0x02, 0x5A}, {0x02, 0x5F},
	{0x02, 0x63}, {0x02, 0x68}, {0x02, 0x6C}, {0x02, 0x71}, {0x02, 0x75}, {0x02, 0x7A}, {0x02, 0x7F}, {0x02, 0x83}, {0x02, 0x88}, {0x02, 0x8C},
	{0x02, 0x91}, {0x02, 0x96}, {0x02, 0x9A}, {0x02, 0x9F}, {0x02, 0xA3}, {0x02, 0xA8}, {0x02, 0xAD}, {0x02, 0xB1}, {0x02, 0xB6}, {0x02, 0xBB},
	{0x02, 0xBF}, {0x02, 0xC4}, {0x02, 0xC9}, {0x02, 0xCE}, {0x02, 0xD2}, {0x02, 0xD7}, {0x02, 0xDC}, {0x02, 0xE0}, {0x02, 0xE5}, {0x02, 0xEA},
	{0x02, 0xEF}, {0x02, 0xF4}, {0x02, 0xF8}, {0x02, 0xFD}, {0x03, 0x02}, {0x03, 0x07}, {0x03, 0x0B}, {0x03, 0x10}, {0x03, 0x15}, {0x03, 0x1A},
	{0x03, 0x1F}, {0x03, 0x24}, {0x03, 0x28}, {0x03, 0x2D}, {0x03, 0x32}, {0x03, 0x37}, {0x03, 0x3C}, {0x03, 0x41}, {0x03, 0x46}, {0x03, 0x4B},
	{0x03, 0x4F}, {0x03, 0x54}, {0x03, 0x59}, {0x03, 0x5E}, {0x03, 0x63}, {0x03, 0x68}, {0x03, 0x6D}, {0x03, 0x72}, {0x03, 0x77}, {0x03, 0x7C},
	{0x03, 0x81}, {0x03, 0x86}, {0x03, 0x8B}, {0x03, 0x90}, {0x03, 0x95}, {0x03, 0x9A}, {0x03, 0x9F}, {0x03, 0xA4}, {0x03, 0xA9}, {0x03, 0xAE},
	{0x03, 0xB3}, {0x03, 0xB8}, {0x03, 0xBD}, {0x03, 0xC2}, {0x03, 0xC7}, {0x03, 0xCC}, {0x03, 0xD1}, {0x03, 0xD6}, {0x03, 0xDB}, {0x03, 0xE0},
	{0x03, 0xE5}, {0x03, 0xEB}, {0x03, 0xF0}, {0x03, 0xF5}, {0x03, 0xFA}, {0x03, 0xFF}, {0x00, 0x0C}, {0x00, 0x10}, {0x00, 0x15}, {0x00, 0x19},
	{0x00, 0x1E}, {0x00, 0x22}, {0x00, 0x26}, {0x00, 0x2B}, {0x00, 0x2F}, {0x00, 0x34}, {0x00, 0x38}, {0x00, 0x3C}, {0x00, 0x41}, {0x00, 0x45},
	{0x00, 0x4A}, {0x00, 0x4E}, {0x00, 0x52}, {0x00, 0x57}, {0x00, 0x5B}, {0x00, 0x60}, {0x00, 0x64}, {0x00, 0x68}, {0x00, 0x6D}, {0x00, 0x71},
	{0x00, 0x75}, {0x00, 0x7A}, {0x00, 0x7E}, {0x00, 0x83}, {0x00, 0x87}, {0x00, 0x8B}, {0x00, 0x90}, {0x00, 0x94}, {0x00, 0x99}, {0x00, 0x9D},
	{0x00, 0xA1}, {0x00, 0xA6}, {0x00, 0xAA}, {0x00, 0xAF}, {0x00, 0xB3}, {0x00, 0xB7}, {0x00, 0xBC}, {0x00, 0xC0}, {0x00, 0xC5}, {0x00, 0xC9},
	{0x00, 0xCD}, {0x00, 0xD2}, {0x00, 0xD6}, {0x00, 0xDB}, {0x00, 0xDF}, {0x00, 0xE3}, {0x00, 0xE8}, {0x00, 0xEC}, {0x00, 0xF1}, {0x00, 0xF5},
	{0x00, 0xF9}, {0x00, 0xFE}, {0x01, 0x02}, {0x01, 0x07}, {0x01, 0x0B}, {0x01, 0x0F}, {0x01, 0x14}, {0x01, 0x18}, {0x01, 0x1D}, {0x01, 0x21},
	{0x01, 0x25}, {0x01, 0x2A}, {0x01, 0x2E}, {0x01, 0x33}, {0x01, 0x37}, {0x01, 0x3B}, {0x01, 0x40}, {0x01, 0x44}, {0x01, 0x48}, {0x01, 0x4D},
	{0x01, 0x51}, {0x01, 0x56}, {0x01, 0x5A}, {0x01, 0x5E}, {0x01, 0x63}, {0x01, 0x67}, {0x01, 0x6C}, {0x01, 0x70}, {0x01, 0x74}, {0x01, 0x79},
	{0x01, 0x7D}, {0x01, 0x82}, {0x01, 0x86}, {0x01, 0x8A}, {0x01, 0x8F}, {0x01, 0x93}, {0x01, 0x98}, {0x01, 0x9C}, {0x01, 0xA0}, {0x01, 0xA5},
	{0x01, 0xA9}, {0x01, 0xAE}, {0x01, 0xB2}, {0x01, 0xB6}, {0x01, 0xBB}, {0x01, 0xBF}, {0x01, 0xC4}, {0x01, 0xC8}, {0x01, 0xCC}, {0x01, 0xD1},
	{0x01, 0xD5}, {0x01, 0xDA}, {0x01, 0xDE}, {0x01, 0xE2}, {0x01, 0xE7}, {0x01, 0xEB}, {0x01, 0xF0}, {0x01, 0xF4}, {0x01, 0xF8}, {0x01, 0xFD},
	{0x02, 0x01}, {0x02, 0x05}, {0x02, 0x0A}, {0x02, 0x0E}, {0x02, 0x13}, {0x02, 0x17}, {0x02, 0x1B}, {0x02, 0x20}, {0x02, 0x24}, {0x02, 0x29},
	{0x02, 0x2D}, {0x02, 0x31}, {0x02, 0x36}, {0x02, 0x3A}, {0x02, 0x3F}, {0x02, 0x43}, {0x02, 0x47}, {0x02, 0x4C}, {0x02, 0x50}, {0x02, 0x55},
	{0x02, 0x59}, {0x02, 0x5D}, {0x02, 0x62}, {0x02, 0x66}, {0x02, 0x6B}, {0x02, 0x6F}, {0x02, 0x73}, {0x02, 0x78}, {0x02, 0x7C}, {0x02, 0x81},
	{0x02, 0x85}, {0x02, 0x89}, {0x02, 0x8E}, {0x02, 0x92}, {0x02, 0x97}, {0x02, 0x9B}, {0x02, 0x9F}, {0x02, 0xA4}, {0x02, 0xA8}, {0x02, 0xAD},
	{0x02, 0xB1}, {0x02, 0xB5}, {0x02, 0xBA}, {0x02, 0xBE}, {0x02, 0xC3}, {0x02, 0xC7}, {0x02, 0xCB}, {0x02, 0xD0}, {0x02, 0xD4}, {0x02, 0xD8},
	{0x02, 0xDD}, {0x02, 0xE1}, {0x02, 0xE6}, {0x02, 0xEA}, {0x02, 0xEE}, {0x02, 0xF3}, {0x02, 0xF7}, {0x02, 0xFC}, {0x03, 0x00}, {0x03, 0x04},
	{0x03, 0x09}, {0x03, 0x0D}, {0x03, 0x12}, {0x03, 0x16}, {0x03, 0x1A}, {0x03, 0x1F}, {0x03, 0x23}, {0x03, 0x28}, {0x03, 0x2C}, {0x03, 0x30},
	{0x03, 0x35}, {0x03, 0x39}, {0x03, 0x3E}, {0x03, 0x42}, {0x03, 0x46}, {0x03, 0x4B}, {0x03, 0x4F}, {0x03, 0x54}, {0x03, 0x58}, {0x03, 0x5C},
	{0x03, 0x61}, {0x03, 0x65}, {0x03, 0x6A}, {0x03, 0x6E}, {0x03, 0x72}, {0x03, 0x77}, {0x03, 0x7B}, {0x03, 0x80}, {0x03, 0x84}, {0x03, 0x88},
	{0x03, 0x8D}, {0x03, 0x91}, {0x03, 0x95}, {0x03, 0x9A}, {0x03, 0x9E}, {0x03, 0xA3}, {0x03, 0xA7}, {0x03, 0xAB}, {0x03, 0xB0}, {0x03, 0xB4},
	{0x03, 0xB9}, {0x03, 0xBD}, {0x03, 0xC1}, {0x03, 0xC6}, {0x03, 0xCA}, {0x03, 0xCF}, {0x03, 0xD3}, {0x03, 0xD7}, {0x03, 0xDC}, {0x03, 0xE0},
	{0x03, 0xE5}, {0x03, 0xE9}, {0x03, 0xED}, {0x03, 0xF2}, {0x03, 0xF6}, {0x03, 0xFB}, {0x03, 0xFF},
};

static u8 a33x_elvss_table[S6E8FC3_TOTAL_STEP][1] = {
	/* OVER_ZERO */
	[0 ... 255] = { 0x16 },
	/* HBM */
	[256 ... 270] = { 0x26 },
	[271 ... 283] = { 0x25 },
	[284 ... 297] = { 0x24 },
	[298 ... 310] = { 0x23 },
	[311 ... 324] = { 0x22 },
	[325 ... 337] = { 0x21 },
	[338 ... 351] = { 0x20 },
	[352 ... 364] = { 0x1F },
	[365 ... 378] = { 0x1E },
	[379 ... 391] = { 0x1D },
	[392 ... 405] = { 0x1C },
	[406 ... 418] = { 0x1B },
	[419 ... 432] = { 0x1A },
	[433 ... 445] = { 0x19 },
	[446 ... 459] = { 0x18 },
	[460 ... 472] = { 0x17 },
	[473 ... 486] = { 0x16 },
};

static u8 a33x_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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
		{ 0xE0 }, /* R8 no smooth in HBM */
	}
};

static u8 a33x_acl_frame_avg_table[][1] = {
	{ 0x44 }, /* 16 Frame Avg */
	{ 0x55 }, /* 32 Frame Avg */
};

static u8 a33x_acl_start_point_table[][2] = {
	{ 0x00, 0xB0 }, /* 50 Percent */
	{ 0x40, 0x28 }, /* 60 Percent */
};

static u8 a33x_acl_dim_speed_table[][1] = {
	{ 0x00 }, /* 0x00 : ACL Dimming Off */
	{ 0x20 }, /* 0x20 : ACL Dimming 32 Frames */
};

static u8 a33x_acl_opr_table[MAX_S6E8FC3_ACL_OPR][1] = {
	[S6E8FC3_ACL_OPR_0] = { 0x00 }, /* ACL OFF OPR */
	[S6E8FC3_ACL_OPR_1] = { 0x01 }, /* ACL ON OPR_8 */
	[S6E8FC3_ACL_OPR_2] = { 0x03 }, /* ACL ON OPR_15 */
};

#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 a33x_fast_discharge_table[][1] = {
	{ 0x80 },	//fd off
	{ 0x40 },	//fd on
};
#endif

static u8 a33x_fps_table_1[][1] = {
	[S6E8FC3_VRR_FPS_90] = { 0x01 },
	[S6E8FC3_VRR_FPS_60] = { 0x21 },
};

static u8 a33x_dimming_speep_table_1[][1] = {
	[S6E8FC3_SMOOTH_DIMMING_OFF] = { 0x20 },
	[S6E8FC3_SMOOTH_DIMMING_ON] = { 0x60 },
};

static struct maptbl a33x_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(a33x_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(a33x_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),

	[ACL_FRAME_AVG_MAPTBL] = DEFINE_2D_MAPTBL(a33x_acl_frame_avg_table, init_common_table, getidx_acl_onoff_table, copy_common_maptbl),
	[ACL_START_POINT_MAPTBL] = DEFINE_2D_MAPTBL(a33x_acl_start_point_table, init_common_table, getidx_hbm_onoff_table, copy_common_maptbl),
	[ACL_DIM_SPEED_MAPTBL] = DEFINE_2D_MAPTBL(a33x_acl_dim_speed_table, init_common_table, getidx_acl_dim_onoff_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a33x_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),

	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a33x_tset_table, init_common_table, NULL, copy_tset_maptbl),
#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[FAST_DISCHARGE_MAPTBL] = DEFINE_2D_MAPTBL(a33x_fast_discharge_table,
			init_common_table, getidx_fast_discharge_table, copy_common_maptbl),
#endif
	[FPS_MAPTBL_1] = DEFINE_2D_MAPTBL(a33x_fps_table_1, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[DIMMING_SPEED] = DEFINE_2D_MAPTBL(a33x_dimming_speep_table_1, init_common_table, getidx_smooth_transition_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E8FC3 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A33X_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A33X_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A33X_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 A33X_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A33X_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A33X_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A33X_SLEEP_OUT[] = { 0x11 };
static u8 A33X_SLEEP_IN[] = { 0x10 };
static u8 A33X_DISPLAY_OFF[] = { 0x28 };
static u8 A33X_DISPLAY_ON[] = { 0x29 };

static DEFINE_STATIC_PACKET(a33x_level1_key_enable, DSI_PKT_TYPE_WR, A33X_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a33x_level2_key_enable, DSI_PKT_TYPE_WR, A33X_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a33x_level3_key_enable, DSI_PKT_TYPE_WR, A33X_KEY3_ENABLE, 0);

static DEFINE_STATIC_PACKET(a33x_level1_key_disable, DSI_PKT_TYPE_WR, A33X_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a33x_level2_key_disable, DSI_PKT_TYPE_WR, A33X_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a33x_level3_key_disable, DSI_PKT_TYPE_WR, A33X_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a33x_sleep_out, DSI_PKT_TYPE_WR, A33X_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a33x_sleep_in, DSI_PKT_TYPE_WR, A33X_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a33x_display_on, DSI_PKT_TYPE_WR, A33X_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a33x_display_off, DSI_PKT_TYPE_WR, A33X_DISPLAY_OFF, 0);

static u8 A33X_TSET_SET[] = {
	0xCD,
	0x00,
};
static DEFINE_PKTUI(a33x_tset_set, &a33x_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_tset_set, DSI_PKT_TYPE_WR, A33X_TSET_SET, 0x0F);

#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_MDELAY(a33x_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a33x_wait_6msec, 6);
#endif
static DEFINE_PANEL_MDELAY(a33x_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a33x_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a33x_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(a33x_wait_17msec, 17);

static DEFINE_PANEL_MDELAY(a33x_wait_34msec, 34);

static DEFINE_PANEL_MDELAY(a33x_wait_sleep_out_20msec, 20);
static DEFINE_PANEL_MDELAY(a33x_wait_display_off, 20);

static DEFINE_PANEL_MDELAY(a33x_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a33x_wait_1usec, 1);

static DEFINE_PANEL_KEY(a33x_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a33x_level1_key_enable));
static DEFINE_PANEL_KEY(a33x_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a33x_level2_key_enable));
static DEFINE_PANEL_KEY(a33x_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a33x_level3_key_enable));

static DEFINE_PANEL_KEY(a33x_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a33x_level1_key_disable));
static DEFINE_PANEL_KEY(a33x_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a33x_level2_key_disable));
static DEFINE_PANEL_KEY(a33x_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a33x_level3_key_disable));


static DEFINE_PANEL_VSYNC_DELAY(a33x_wait_1_vsync, 1);
#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_VSYNC_DELAY(a33x_wait_2_vsync, 2);
static DEFINE_COND(a33x_cond_is_60hz, s6e8fc3_is_60hz);
static DEFINE_COND(a33x_cond_is_90hz, s6e8fc3_is_90hz);
#endif
static DEFINE_COND(a33x_cond_is_bringup_panel, s6e8fc3_a33_is_bringup_panel);
static DEFINE_COND(a33x_cond_is_real_panel, s6e8fc3_a33_is_real_panel);
static DEFINE_COND(a33x_cond_is_real_panel_rev04, s6e8fc3_a33_is_real_panel_rev04);

static u8 A33X_HBM_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(a33x_hbm_transition, &a33x_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_hbm_transition, DSI_PKT_TYPE_WR, A33X_HBM_TRANSITION, 0);

static u8 A33X_ACL_SET[] = {
	0xB4,
	0x44, 0x00, 0xB0, 0x57, 0x66, 0xA1, 0x15,
	0x55, 0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48,
	0x40, 0x00, 0x00
};

/*
 * ACL SETTING
 * 1st Para.
 * 0x44 : 16 Frame Average
 * 0x55 : 32 Frame Average
 * 2nd 3rd Para
 * 0x00, 0xB0 : ACL Start Step 50%
 * 0x40, 0x28 : ACL Start Step 60%
 * 17th Para.
 * 0x00 : ACL Dimming Off
 * 0x20 : ACL 32 Frame Dimming
 */

static DECLARE_PKTUI(a33x_acl_set) = {
	{ .offset = 1, .maptbl = &a33x_maptbl[ACL_FRAME_AVG_MAPTBL] },
	{ .offset = 2, .maptbl = &a33x_maptbl[ACL_START_POINT_MAPTBL] },
	{ .offset = 17, .maptbl = &a33x_maptbl[ACL_DIM_SPEED_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a33x_acl_set, DSI_PKT_TYPE_WR, A33X_ACL_SET, 0x0D);

static u8 A33X_ACL[] = {
	0x55, 0x03
};
static DEFINE_PKTUI(a33x_acl_control, &a33x_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_acl_control, DSI_PKT_TYPE_WR, A33X_ACL, 0);

static u8 A33X_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(a33x_wrdisbv, &a33x_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_wrdisbv, DSI_PKT_TYPE_WR, A33X_WRDISBV, 0);

static u8 A33X_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 A33X_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };
static DEFINE_STATIC_PACKET(a33x_caset, DSI_PKT_TYPE_WR, A33X_CASET, 0);
static DEFINE_STATIC_PACKET(a33x_paset, DSI_PKT_TYPE_WR, A33X_PASET, 0);

#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 A33X_FAST_DISCHARGE[] = {
	0xB5,
	0x40, 0x40	/* FD enable */
};
static DEFINE_PKTUI(a33x_fast_discharge, &a33x_maptbl[FAST_DISCHARGE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_fast_discharge, DSI_PKT_TYPE_WR, A33X_FAST_DISCHARGE, 0x0A);
#endif

static u8 A33X_PCD_SET_DET_LOW[] = {
	0xEA,
	0x5C, 0x51	/* 1st 0x5C: default HIGH, 2nd 0x51 : Enable SW RESET */
};
static DEFINE_STATIC_PACKET(a33x_pcd_det_set, DSI_PKT_TYPE_WR, A33X_PCD_SET_DET_LOW, 0);

static u8 A33X_ERR_FG_ON[] = {
	0xE1, 0x83
};
static DEFINE_STATIC_PACKET(a33x_err_fg_on, DSI_PKT_TYPE_WR, A33X_ERR_FG_ON, 0);

static u8 A33X_ERR_FG_OFF[] = {
	0xE1, 0x03
};
static DEFINE_STATIC_PACKET(a33x_err_fg_off, DSI_PKT_TYPE_WR, A33X_ERR_FG_OFF, 0);

static u8 A33X_ERR_FG_SETTING_1[] = {
	0xED,
	0x01, 0x26,
	/* Vlin1, ELVDD, Vlin3 Monitor On */
};
static DEFINE_STATIC_PACKET(a33x_err_fg_setting_1, DSI_PKT_TYPE_WR, A33X_ERR_FG_SETTING_1, 0);

static u8 A33X_ERR_FG_SETTING_2[] = {
	0xF4, 0x1C
};
static DEFINE_STATIC_PACKET(a33x_err_fg_setting_2, DSI_PKT_TYPE_WR, A33X_ERR_FG_SETTING_2, 0x06);

static u8 A33X_VSYNC_SET[] = {
	0xF2,
	0x54
};
static DEFINE_STATIC_PACKET(a33x_vsync_set, DSI_PKT_TYPE_WR, A33X_VSYNC_SET, 0x04);

static u8 A33X_FREQ_SET[] = {
	0xF2,
	0x00
};
static DEFINE_STATIC_PACKET(a33x_freq_set, DSI_PKT_TYPE_WR, A33X_FREQ_SET, 0x27);

static u8 A33X_PORCH_SET[] = {
	0xF2,
	0x55
};
static DEFINE_STATIC_PACKET(a33x_porch_set, DSI_PKT_TYPE_WR, A33X_PORCH_SET, 0x2E);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 A33X_XTALK_MODE[] = { 0xD9, 0x60 };
static DEFINE_PKTUI(a33x_xtalk_mode, &a33x_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a33x_xtalk_mode, DSI_PKT_TYPE_WR, A33X_XTALK_MODE, 0x1C);
#endif

static u8 A33X_FPS_1[] = { 0x60, 0x00, 0x00};
static DEFINE_PKTUI(a33x_fps_1, &a33x_maptbl[FPS_MAPTBL_1], 1);
static DEFINE_VARIABLE_PACKET(a33x_fps_1, DSI_PKT_TYPE_WR, A33X_FPS_1, 0);

static u8 A33X_PANEL_UPDATE[] = {
	0xF7,
	0x0B
};
static DEFINE_STATIC_PACKET(a33x_panel_update, DSI_PKT_TYPE_WR, A33X_PANEL_UPDATE, 0);

static u8 A33X_GLOBAL_PARAM_SETTING[] = {
	0xF2,
	0x00, 0x05, 0x0E, 0x58, 0x50, 0x00, 0x0C, 0x00,
	0x04, 0x30, 0xB1, 0x30, 0xB1, 0x0C, 0x04, 0xBC,
	0x26, 0xE9, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
	0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x34, 0x10,
	0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
	0xCE
};
static DEFINE_STATIC_PACKET(a33x_global_param_setting, DSI_PKT_TYPE_WR, A33X_GLOBAL_PARAM_SETTING, 0);

static u8 A33X_MIN_ROI_SETTING[] = {
	0xC2,
	0x1B, 0x41, 0xB0, 0x0E, 0x00, 0x3C, 0x5A, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(a33x_min_roi_setting, DSI_PKT_TYPE_WR, A33X_MIN_ROI_SETTING, 0);


/* WARNING - S6E8FC3 using normal data type for DSC setting */
static u8 A33X_DSC[] = { 0x9D, 0x01 };
static DEFINE_STATIC_PACKET(a33x_dsc, DSI_PKT_TYPE_WR, A33X_DSC, 0);

static u8 A33X_PPS[] = {
	//1080x2400 Slice Info : 540x40 / 2 slice mode
	0x9E,
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
	0x00
};
static DEFINE_STATIC_PACKET(a33x_pps, DSI_PKT_TYPE_WR, A33X_PPS, 0);

static u8 A33X_NORMAL_MODE[] = {
	0x53, 0x20

};
static DEFINE_STATIC_PACKET(a33x_normal_mode, DSI_PKT_TYPE_WR, A33X_NORMAL_MODE, 0);

static u8 A33X_BLACK_INSERT_OFF[] = { 0xF2, 0x0C };
static DEFINE_STATIC_PACKET(a33x_black_insert_off, DSI_PKT_TYPE_WR, A33X_BLACK_INSERT_OFF, 0);

static u8 A33X_TSP_VSYNC_ON[] = {
	0xE8,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01
};
static DEFINE_STATIC_PACKET(a33x_tsp_vsync_on, DSI_PKT_TYPE_WR, A33X_TSP_VSYNC_ON, 0);

static u8 A33X_OSC_SETTING_1[] = {
	0xD7,
	0x90
};
static DEFINE_STATIC_PACKET(a33x_osc_setting_1, DSI_PKT_TYPE_WR, A33X_OSC_SETTING_1, 0x1B);

static u8 A33X_OSC_SETTING_2[] = {
	0xFE,
	0xB0
};
static DEFINE_STATIC_PACKET(a33x_osc_setting_2, DSI_PKT_TYPE_WR, A33X_OSC_SETTING_2, 0);

static u8 A33X_OSC_SETTING_3[] = {
	0xFE,
	0x30
};
static DEFINE_STATIC_PACKET(a33x_osc_setting_3, DSI_PKT_TYPE_WR, A33X_OSC_SETTING_3, 0);

/* 1 ~ 3 has same value */
static u8 A33X_ETC_SETTING_1[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a33x_etc_setting_1, DSI_PKT_TYPE_WR, A33X_ETC_SETTING_1, 0x15);

static u8 A33X_ETC_SETTING_2[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a33x_etc_setting_2, DSI_PKT_TYPE_WR, A33X_ETC_SETTING_2, 0x28);

static u8 A33X_ETC_SETTING_3[] = {
	0xF6,
	0xF0
};
static DEFINE_STATIC_PACKET(a33x_etc_setting_3, DSI_PKT_TYPE_WR, A33X_ETC_SETTING_3, 0x3B);

static u8 A33X_SEED_SETTING[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x5F, 0x8F, 0x00, 0x5F, 0x8F
};
static DEFINE_STATIC_PACKET(a33x_seed_setting, DSI_PKT_TYPE_WR, A33X_SEED_SETTING, 0);

static u8 A33X_FFC_DEFAULT_REV04[] = {
	0xDF,
	0x09, 0x30, 0x95, 0x2F, 0xD6, 0x4B, 0x51
};
static DEFINE_STATIC_PACKET(a33x_ffc_default_rev04, DSI_PKT_TYPE_WR, A33X_FFC_DEFAULT_REV04, 0);

static u8 A33X_FFC_DEFAULT[] = {
	0xDF,
	0x09, 0x30, 0x95, 0x56, 0xDD, 0x56, 0xDD
};
static DEFINE_STATIC_PACKET(a33x_ffc_default, DSI_PKT_TYPE_WR, A33X_FFC_DEFAULT, 0);

#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 A33X_CCD_ENABLE[] = { 0xEA, 0x5C, 0x51, 0x01, 0x00 };
static u8 A33X_CCD_DISABLE[] = { 0xEA, 0x5C, 0x51, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a33x_ccd_test_enable, DSI_PKT_TYPE_WR, A33X_CCD_ENABLE, 0x00);
static DEFINE_STATIC_PACKET(a33x_ccd_test_disable, DSI_PKT_TYPE_WR, A33X_CCD_DISABLE, 0x00);

static u8 A33X_CCD_F1_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 A33X_CCD_F1_DISABLE[] = { 0xF1, 0xA5, 0xA5 };
static DEFINE_STATIC_PACKET(a33x_ccd_f1_enable, DSI_PKT_TYPE_WR, A33X_CCD_F1_ENABLE, 0x00);
static DEFINE_STATIC_PACKET(a33x_ccd_f1_disable, DSI_PKT_TYPE_WR, A33X_CCD_F1_DISABLE, 0x00);
#endif

static DEFINE_SETPROP_VALUE(a33x_set_wait_tx_done_property_off, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_SETPROP_VALUE(a33x_set_wait_tx_done_property_auto, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static struct seqinfo SEQINFO(a33x_set_bl_param_seq);
static struct seqinfo SEQINFO(a33x_set_fps_param_seq);
#if defined(CONFIG_MCD_PANEL_FACTORY)
static struct seqinfo SEQINFO(a33x_res_init_seq);
#endif

static void *a33x_init_cmdtbl[] = {
	&DLYINFO(a33x_wait_10msec),
	&PKTINFO(a33x_sleep_out),
	&DLYINFO(a33x_wait_100msec),

	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),

	&s6e8fc3_restbl[RES_ID],

	&PKTINFO(a33x_dsc),
	&PKTINFO(a33x_pps),

	&PKTINFO(a33x_tsp_vsync_on),

	&CONDINFO_IF(a33x_cond_is_real_panel_rev04),
		&PKTINFO(a33x_ffc_default_rev04),
	&CONDINFO_FI(a33x_cond_is_real_panel_rev04),

	&CONDINFO_IF(a33x_cond_is_real_panel),
		&PKTINFO(a33x_ffc_default),
	&CONDINFO_FI(a33x_cond_is_real_panel),

	&PKTINFO(a33x_black_insert_off),
	&PKTINFO(a33x_panel_update),

	&PKTINFO(a33x_etc_setting_1),
	&PKTINFO(a33x_etc_setting_2),
	&PKTINFO(a33x_etc_setting_3),
	&PKTINFO(a33x_panel_update),

	&PKTINFO(a33x_err_fg_setting_1),
	&PKTINFO(a33x_err_fg_setting_2),
	&PKTINFO(a33x_err_fg_on),
	&PKTINFO(a33x_pcd_det_set),

	&PKTINFO(a33x_seed_setting),
	&PKTINFO(a33x_panel_update),

	&SEQINFO(a33x_set_bl_param_seq), /* includes FPS setting also */

	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),

#if defined(CONFIG_MCD_PANEL_FACTORY)
	&SEQINFO(a33x_res_init_seq),
#endif
};

static void *a33x_res_init_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),

	&s6e8fc3_restbl[RES_ID],
	&s6e8fc3_restbl[RES_COORDINATE],
	&s6e8fc3_restbl[RES_CODE],
	&s6e8fc3_restbl[RES_DATE],
	&s6e8fc3_restbl[RES_ELVSS],
	&s6e8fc3_restbl[RES_OCTA_ID],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e8fc3_restbl[RES_SELF_DIAG],
	&s6e8fc3_restbl[RES_ERR_FG],
	&s6e8fc3_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};
#if defined(CONFIG_MCD_PANEL_FACTORY)
static DEFINE_SEQINFO(a33x_res_init_seq, a33x_res_init_cmdtbl);
#endif

static void *a33x_set_bl_param_cmdtbl[] = {
	&PKTINFO(a33x_fps_1),
	&PKTINFO(a33x_tset_set),
	&PKTINFO(a33x_acl_set),
	&PKTINFO(a33x_acl_control),
	&PKTINFO(a33x_hbm_transition),
	&PKTINFO(a33x_wrdisbv),
	&PKTINFO(a33x_panel_update),
};

static DEFINE_SEQINFO(a33x_set_bl_param_seq, a33x_set_bl_param_cmdtbl);

static void *a33x_set_bl_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	&SEQINFO(a33x_set_bl_param_seq),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

static DEFINE_COND(a33x_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);

static void *a33x_display_mode_cmdtbl[] = {
	&SETPROP(a33x_set_wait_tx_done_property_off),
		&DLYINFO(a33x_wait_1_vsync),
		/* VFP start */
		&DLYINFO(a33x_wait_1msec),
		/* VFP CMD TX ALLOW END */

		/* Let's stack cmd to sent on next VFP */
		&KEYINFO(a33x_level1_key_enable),
		&KEYINFO(a33x_level2_key_enable),
		&KEYINFO(a33x_level3_key_enable),
		&SEQINFO(a33x_set_bl_param_seq),
		&KEYINFO(a33x_level3_key_disable),
		&KEYINFO(a33x_level2_key_disable),
		&KEYINFO(a33x_level1_key_disable),
		/* Will flush on next VFP */
	&SETPROP(a33x_set_wait_tx_done_property_auto),
};

static void *a33x_display_on_cmdtbl[] = {
	&DLYINFO(a33x_wait_sleep_out_20msec),
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),

	&PKTINFO(a33x_display_on),

	&CONDINFO_IF(a33x_cond_is_bringup_panel),
		/* OSC setting */
		&DLYINFO(a33x_wait_30msec),
		&PKTINFO(a33x_osc_setting_1),
		&PKTINFO(a33x_osc_setting_2),
		&PKTINFO(a33x_osc_setting_3),
		&PKTINFO(a33x_panel_update),
		/* FFC setting */
		&DLYINFO(a33x_wait_30msec),
		&PKTINFO(a33x_ffc_default_rev04),
	&CONDINFO_FI(a33x_cond_is_bringup_panel),

	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

static void *a33x_display_off_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	&PKTINFO(a33x_err_fg_off),
	&PKTINFO(a33x_display_off),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
	&DLYINFO(a33x_wait_display_off),
};

static void *a33x_exit_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e8fc3_dmptbl[DUMP_DSI_ERR],
	&s6e8fc3_dmptbl[DUMP_SELF_DIAG],
#endif
	&s6e8fc3_dmptbl[DUMP_SELF_MASK_CRC],
	&PKTINFO(a33x_sleep_in),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
	&DLYINFO(a33x_wait_sleep_in),
};

static void *a33x_dump_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	&s6e8fc3_dmptbl[DUMP_RDDPM],
	&s6e8fc3_dmptbl[DUMP_RDDSM],
	&s6e8fc3_dmptbl[DUMP_DSI_ERR],
	&s6e8fc3_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static void *a33x_fast_discharge_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&PKTINFO(a33x_fast_discharge),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
	&DLYINFO(a33x_wait_34msec),
};
#endif

static void *a33x_check_condition_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	&s6e8fc3_dmptbl[DUMP_RDDPM],
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_MASK_LAYER
static void *a33x_mask_layer_workaround_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	&PKTINFO(a33x_wrdisbv),
	&PKTINFO(a33x_hbm_transition),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
	&DLYINFO(a33x_wait_2_vsync),
};

static void *a33x_mask_layer_before_cmdtbl[] = {
	&DLYINFO(a33x_wait_1_vsync),
	/* VFP start */
	&DLYINFO(a33x_wait_1msec),
	/* VFP CMD TX ALLOW END */
};

static void *a33x_mask_layer_enter_br_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	/* BL CMD */
	&SEQINFO(a33x_set_bl_param_seq),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

static void *a33x_mask_layer_exit_br_cmdtbl[] = {
	&KEYINFO(a33x_level1_key_enable),
	&KEYINFO(a33x_level2_key_enable),
	&KEYINFO(a33x_level3_key_enable),
	/* BL CMD */
	&SEQINFO(a33x_set_bl_param_seq),
	&KEYINFO(a33x_level3_key_disable),
	&KEYINFO(a33x_level2_key_disable),
	&KEYINFO(a33x_level1_key_disable),
};

static void *a33x_mask_layer_after_cmdtbl[] = {
	/* VFP start */
	/* CMD will be flushed here */
	&CONDINFO_IF(a33x_cond_is_90hz),
		&DLYINFO(a33x_wait_1msec),
	&CONDINFO_FI(a33x_cond_is_90hz),

	&CONDINFO_IF(a33x_cond_is_60hz),
		&DLYINFO(a33x_wait_6msec),
	&CONDINFO_FI(a33x_cond_is_60hz),
	/* VSYNC start */
	/* DECON update for next frame */
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *a33x_ccd_test_cmdtbl[] = {
	&PKTINFO(a33x_ccd_f1_enable),
	&KEYINFO(a33x_level3_key_enable),
	&PKTINFO(a33x_ccd_test_enable),
	&DLYINFO(a33x_wait_1_vsync),
	&s6e8fc3_restbl[RES_CCD_STATE],
	&PKTINFO(a33x_ccd_test_disable),
	&PKTINFO(a33x_ccd_f1_disable),
	&KEYINFO(a33x_level3_key_disable),
};
#endif

static void *a33x_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo a33x_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a33x_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a33x_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a33x_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a33x_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a33x_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a33x_exit_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("fps-seq", a33x_display_mode_cmdtbl),
#ifdef CONFIG_SUPPORT_MASK_LAYER
	[PANEL_MASK_LAYER_STOP_DIMMING_SEQ] = SEQINFO_INIT("mask-layer-workaround-seq", a33x_mask_layer_workaround_cmdtbl),
	[PANEL_MASK_LAYER_BEFORE_SEQ] = SEQINFO_INIT("mask-layer-before-seq", a33x_mask_layer_before_cmdtbl),
	[PANEL_MASK_LAYER_ENTER_BR_SEQ] = SEQINFO_INIT("mask-layer-enter-br-seq", a33x_mask_layer_enter_br_cmdtbl),
	[PANEL_MASK_LAYER_EXIT_BR_SEQ] = SEQINFO_INIT("mask-layer-exit-br-seq", a33x_mask_layer_exit_br_cmdtbl),
	[PANEL_MASK_LAYER_AFTER_SEQ] = SEQINFO_INIT("mask-layer-after-seq", a33x_mask_layer_after_cmdtbl),
#endif
#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[PANEL_FD_SEQ] = SEQINFO_INIT("fast-discharge-seq", a33x_fast_discharge_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", a33x_ccd_test_cmdtbl),
#endif
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a33x_dump_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", a33x_check_condition_cmdtbl),
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", a33x_dummy_cmdtbl),
};

struct common_panel_info s6e8fc3_a33x_panel_info = {
	.ldi_name = "s6e8fc3",
	.name = "s6e8fc3_a33x",
	.model = "AMS638YQ01",
	.vendor = "SDC",
	.id = 0x800004,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_2BYTE_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e8fc3_a33x_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e8fc3_a33x_default_resol),
		.resol = s6e8fc3_a33x_default_resol,
	},
	.vrrtbl = s6e8fc3_a33x_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e8fc3_a33x_default_vrrtbl),
	.maptbl = a33x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a33x_maptbl),
	.seqtbl = a33x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a33x_seqtbl),
	.rditbl = s6e8fc3_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e8fc3_rditbl),
	.restbl = s6e8fc3_restbl,
	.nr_restbl = ARRAY_SIZE(s6e8fc3_restbl),
	.dumpinfo = s6e8fc3_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e8fc3_dmptbl),

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e8fc3_a33x_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e8fc3_a33x_panel_dimming_info,
	},
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e8fc3_a33x_aod,
#endif
};
#endif /* __S6E8FC3_A33X_PANEL_H__ */
