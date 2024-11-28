/*
 * linux/drivers/video/fbdev/exynos/panel/hx83102e/hx83102e_gta4xls_00_panel.h
 *
 * Header file for HX83102E Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102E_GTA4XLS_00_PANEL_H__
#define __HX83102E_GTA4XLS_00_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "tft_function.h"
#include "hx83102e_gta4xls_00_resol.h"

#undef __pn_name__
#define __pn_name__	gta4xls

#undef __PN_NAME__
#define __PN_NAME__

#define GTA4XLS_FIRST_BR_PROPERTY ("gta4xls_first_br_property")

enum {
	GTA4XLS_FIRST_BR_OFF = 0,
	GTA4XLS_FIRST_BR_ON,
	MAX_GTA4XLS_FIRST_BR
};

#define HX83102E_NR_STEP (256)
#define HX83102E_HBM_STEP (1)
#define HX83102E_TOTAL_STEP (HX83102E_NR_STEP + HX83102E_HBM_STEP) /* 0 ~ 256 */

static unsigned int hx83102e_gta4xls_00_brt_tbl[HX83102E_TOTAL_STEP] = {
	BRT(0),
	BRT(1), BRT(2), BRT(3), BRT(4), BRT(5), BRT(6), BRT(7), BRT(8), BRT(9), BRT(10),
	BRT(11), BRT(12), BRT(13), BRT(14), BRT(15), BRT(16), BRT(17), BRT(18), BRT(19), BRT(20),
	BRT(21), BRT(22), BRT(23), BRT(24), BRT(25), BRT(26), BRT(27), BRT(28), BRT(29), BRT(30),
	BRT(31), BRT(32), BRT(33), BRT(34), BRT(35), BRT(36), BRT(37), BRT(38), BRT(39), BRT(40),
	BRT(41), BRT(42), BRT(43), BRT(44), BRT(45), BRT(46), BRT(47), BRT(48), BRT(49), BRT(50),
	BRT(51), BRT(52), BRT(53), BRT(54), BRT(55), BRT(56), BRT(57), BRT(58), BRT(59), BRT(60),
	BRT(61), BRT(62), BRT(63), BRT(64), BRT(65), BRT(66), BRT(67), BRT(68), BRT(69), BRT(70),
	BRT(71), BRT(72), BRT(73), BRT(74), BRT(75), BRT(76), BRT(77), BRT(78), BRT(79), BRT(80),
	BRT(81), BRT(82), BRT(83), BRT(84), BRT(85), BRT(86), BRT(87), BRT(88), BRT(89), BRT(90),
	BRT(91), BRT(92), BRT(93), BRT(94), BRT(95), BRT(96), BRT(97), BRT(98), BRT(99), BRT(100),
	BRT(101), BRT(102), BRT(103), BRT(104), BRT(105), BRT(106), BRT(107), BRT(108), BRT(109), BRT(110),
	BRT(111), BRT(112), BRT(113), BRT(114), BRT(115), BRT(116), BRT(117), BRT(118), BRT(119), BRT(120),
	BRT(121), BRT(122), BRT(123), BRT(124), BRT(125), BRT(126), BRT(127), BRT(128), BRT(129), BRT(130),
	BRT(131), BRT(132), BRT(133), BRT(134), BRT(135), BRT(136), BRT(137), BRT(138), BRT(139), BRT(140),
	BRT(141), BRT(142), BRT(143), BRT(144), BRT(145), BRT(146), BRT(147), BRT(148), BRT(149), BRT(150),
	BRT(151), BRT(152), BRT(153), BRT(154), BRT(155), BRT(156), BRT(157), BRT(158), BRT(159), BRT(160),
	BRT(161), BRT(162), BRT(163), BRT(164), BRT(165), BRT(166), BRT(167), BRT(168), BRT(169), BRT(170),
	BRT(171), BRT(172), BRT(173), BRT(174), BRT(175), BRT(176), BRT(177), BRT(178), BRT(179), BRT(180),
	BRT(181), BRT(182), BRT(183), BRT(184), BRT(185), BRT(186), BRT(187), BRT(188), BRT(189), BRT(190),
	BRT(191), BRT(192), BRT(193), BRT(194), BRT(195), BRT(196), BRT(197), BRT(198), BRT(199), BRT(200),
	BRT(201), BRT(202), BRT(203), BRT(204), BRT(205), BRT(206), BRT(207), BRT(208), BRT(209), BRT(210),
	BRT(211), BRT(212), BRT(213), BRT(214), BRT(215), BRT(216), BRT(217), BRT(218), BRT(219), BRT(220),
	BRT(221), BRT(222), BRT(223), BRT(224), BRT(225), BRT(226), BRT(227), BRT(228), BRT(229), BRT(230),
	BRT(231), BRT(232), BRT(233), BRT(234), BRT(235), BRT(236), BRT(237), BRT(238), BRT(239), BRT(240),
	BRT(241), BRT(242), BRT(243), BRT(244), BRT(245), BRT(246), BRT(247), BRT(248), BRT(249), BRT(250),
	BRT(251), BRT(252), BRT(253), BRT(254), BRT(255),

	/* HBM */
	BRT(256)
};

static unsigned int hx83102e_gta4xls_00_step_cnt_tbl[HX83102E_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256] = 1,
};

struct brightness_table hx83102e_gta4xls_00_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = hx83102e_gta4xls_00_brt_tbl,
	.sz_brt = ARRAY_SIZE(hx83102e_gta4xls_00_brt_tbl),
	.sz_ui_brt = HX83102E_NR_STEP,
	.sz_hbm_brt = HX83102E_HBM_STEP,
	.lum = hx83102e_gta4xls_00_brt_tbl,
	.sz_lum = ARRAY_SIZE(hx83102e_gta4xls_00_brt_tbl),
	.sz_ui_lum = HX83102E_NR_STEP,
	.sz_hbm_lum = HX83102E_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = hx83102e_gta4xls_00_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(hx83102e_gta4xls_00_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info hx83102e_gta4xls_00_panel_dimming_info = {
	.name = "hx83102e_gta4xls",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &hx83102e_gta4xls_00_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 hx83102e_gta4xls_00_brt_table[HX83102E_TOTAL_STEP][2] = {
	{0x00, 0x00},
	{0x00, 0x00}, {0x00, 0x20}, {0x00, 0x1F}, {0x00, 0x2A}, {0x00, 0x35},
	{0x00, 0x3F}, {0x00, 0x4A}, {0x00, 0x55}, {0x00, 0x5F}, {0x00, 0x6A},
	{0x00, 0x75}, {0x00, 0x7F}, {0x00, 0x8A}, {0x00, 0x95}, {0x00, 0x9F},
	{0x00, 0xAA}, {0x00, 0xB5}, {0x00, 0xBF}, {0x00, 0xCA}, {0x00, 0xD5},
	{0x00, 0xDF}, {0x00, 0xEA}, {0x00, 0xF5}, {0x00, 0xFF}, {0x01, 0x0A},
	{0x01, 0x15}, {0x01, 0x1F}, {0x01, 0x2A}, {0x01, 0x35}, {0x01, 0x3F},
	{0x01, 0x4A}, {0x01, 0x55}, {0x01, 0x5F}, {0x01, 0x6A}, {0x01, 0x75},
	{0x01, 0x7F}, {0x01, 0x8A}, {0x01, 0x95}, {0x01, 0x9F}, {0x01, 0xAA},
	{0x01, 0xB5}, {0x01, 0xBF}, {0x01, 0xCA}, {0x01, 0xD5}, {0x01, 0xDF},
	{0x01, 0xEA}, {0x01, 0xF5}, {0x01, 0xFF}, {0x02, 0x0A}, {0x02, 0x15},
	{0x02, 0x1F}, {0x02, 0x2A}, {0x02, 0x35}, {0x02, 0x3F}, {0x02, 0x4A},
	{0x02, 0x55}, {0x02, 0x5F}, {0x02, 0x6A}, {0x02, 0x75}, {0x02, 0x7F},
	{0x02, 0x8A}, {0x02, 0x95}, {0x02, 0x9F}, {0x02, 0xAA}, {0x02, 0xB5},
	{0x02, 0xBF}, {0x02, 0xCA}, {0x02, 0xD5}, {0x02, 0xDF}, {0x02, 0xEA},
	{0x02, 0xF5}, {0x02, 0xFF}, {0x03, 0x0A}, {0x03, 0x15}, {0x03, 0x1F},
	{0x03, 0x2A}, {0x03, 0x35}, {0x03, 0x3F}, {0x03, 0x4A}, {0x03, 0x55},
	{0x03, 0x5F}, {0x03, 0x6A}, {0x03, 0x75}, {0x03, 0x7F}, {0x03, 0x8A},
	{0x03, 0x95}, {0x03, 0x9F}, {0x03, 0xAA}, {0x03, 0xB5}, {0x03, 0xBF},
	{0x03, 0xCA}, {0x03, 0xD5}, {0x03, 0xDF}, {0x03, 0xEA}, {0x03, 0xF5},
	{0x03, 0xFF}, {0x04, 0x0A}, {0x04, 0x15}, {0x04, 0x1F}, {0x04, 0x2A},
	{0x04, 0x35}, {0x04, 0x3F}, {0x04, 0x4A}, {0x04, 0x55}, {0x04, 0x5F},
	{0x04, 0x6A}, {0x04, 0x75}, {0x04, 0x7F}, {0x04, 0x8A}, {0x04, 0x95},
	{0x04, 0x9F}, {0x04, 0xAA}, {0x04, 0xB5}, {0x04, 0xBF}, {0x04, 0xCA},
	{0x04, 0xD5}, {0x04, 0xDF}, {0x04, 0xEA}, {0x04, 0xF5}, {0x04, 0xFF},
	{0x05, 0x0A}, {0x05, 0x15}, {0x05, 0x1F}, {0x05, 0x2A}, {0x05, 0x35},
	{0x05, 0x3F}, {0x05, 0x4A}, {0x05, 0x55}, {0x05, 0x61}, {0x05, 0x6E},
	{0x05, 0x7B}, {0x05, 0x87}, {0x05, 0x94}, {0x05, 0xA1}, {0x05, 0xAE},
	{0x05, 0xBA}, {0x05, 0xC7}, {0x05, 0xD4}, {0x05, 0xDF}, {0x05, 0xED},
	{0x05, 0xFA}, {0x06, 0x07}, {0x06, 0x14}, {0x06, 0x20}, {0x06, 0x2D},
	{0x06, 0x3A}, {0x06, 0x47}, {0x06, 0x53}, {0x06, 0x60}, {0x06, 0x6D},
	{0x06, 0x7A}, {0x06, 0x86}, {0x06, 0x93}, {0x06, 0xA0}, {0x06, 0xAC},
	{0x06, 0xB9}, {0x06, 0xC6}, {0x06, 0xD3}, {0x06, 0xDF}, {0x06, 0xEC},
	{0x06, 0xF9}, {0x07, 0x06}, {0x07, 0x12}, {0x07, 0x1F}, {0x07, 0x2C},
	{0x07, 0x39}, {0x07, 0x45}, {0x07, 0x52}, {0x07, 0x5F}, {0x07, 0x6C},
	{0x07, 0x78}, {0x07, 0x85}, {0x07, 0x92}, {0x07, 0x9F}, {0x07, 0xAB},
	{0x07, 0xB8}, {0x07, 0xC5}, {0x07, 0xD1}, {0x07, 0xDE}, {0x07, 0xEB},
	{0x07, 0xF8}, {0x08, 0x04}, {0x08, 0x11}, {0x08, 0x1E}, {0x08, 0x2B},
	{0x08, 0x37}, {0x08, 0x44}, {0x08, 0x51}, {0x08, 0x5E}, {0x08, 0x6A},
	{0x08, 0x77}, {0x08, 0x84}, {0x08, 0x91}, {0x08, 0x9D}, {0x08, 0xAA},
	{0x08, 0xB7}, {0x08, 0xC4}, {0x08, 0xD0}, {0x08, 0xDD}, {0x08, 0xEA},
	{0x08, 0xF6}, {0x09, 0x03}, {0x09, 0x10}, {0x09, 0x1D}, {0x09, 0x29},
	{0x09, 0x36}, {0x09, 0x43}, {0x09, 0x50}, {0x09, 0x5C}, {0x09, 0x69},
	{0x09, 0x76}, {0x09, 0x83}, {0x09, 0x8F}, {0x09, 0x9C}, {0x09, 0xA9},
	{0x09, 0xB6}, {0x09, 0xC2}, {0x09, 0xCF}, {0x09, 0xDC}, {0x09, 0x2A},
	{0x09, 0xF5}, {0x0A, 0x02}, {0x0A, 0x0F}, {0x0A, 0x1B}, {0x0A, 0x28},
	{0x0A, 0x35}, {0x0A, 0x42}, {0x0A, 0x4E}, {0x0A, 0x5B}, {0x0A, 0x68},
	{0x0A, 0x75}, {0x0A, 0x81}, {0x0A, 0x8E}, {0x0A, 0x9B}, {0x0A, 0xA8},
	{0x0A, 0xB4}, {0x0A, 0xC1}, {0x0A, 0xCE}, {0x0A, 0xDB}, {0x0A, 0xE7},
	{0x0A, 0xF4}, {0x0B, 0x01}, {0x0B, 0x0E}, {0x0B, 0x1A}, {0x0B, 0x27},
	{0x0B, 0x34}, {0x0B, 0x40}, {0x0B, 0x4D}, {0x0B, 0x5A}, {0x0B, 0x67},
	{0x0B, 0x73}, {0x0B, 0x80}, {0x0B, 0x8D}, {0x0B, 0x9A}, {0x0B, 0xA6},
	{0x0D, 0xE1},
};

static struct maptbl hx83102e_gta4xls_00_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(hx83102e_gta4xls_00_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

/* < CABC Mode control Function > */

/* Display config (1) */
static u8 SEQ_HX83102E_GTA4XLS_00_001[] = {
	0xE9,
	0xCD,
};

static u8 SEQ_HX83102E_GTA4XLS_00_002[] = {
	0xBB,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_003[] = {
	0xE9,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_004[] = {
	0xBA,
	0x70, 0x03, 0xA8, 0x83, 0xF2, 0x80, 0x00, 0x0D,
};

static u8 SEQ_HX83102E_GTA4XLS_00_005[] = {
	0x51,
	0x00, 0x00
};

static u8 SEQ_HX83102E_GTA4XLS_00_006[] = {
	0x53,
	0x24,
};

static u8 SEQ_HX83102E_GTA4XLS_00_007[] = {
	0xC9,
	0x04, 0x0B, 0x5D,
};

static u8 SEQ_HX83102E_GTA4XLS_00_008[] = {
	0xCE,
	0x00, 0x50, 0xF0,
};

static u8 SEQ_HX83102E_GTA4XLS_00_009[] = {
	0xD2,
	0xE8, 0xE8,
};

static u8 SEQ_HX83102E_GTA4XLS_00_010[] = {
	0xD9,
	0x04, 0x01, 0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_011[] = {
	0xD5,
	0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x18, 0x18,
	0x24, 0x24, 0x19, 0x19, 0x18, 0x18, 0x19, 0x19, 0x21, 0x20,
	0x23, 0x22, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18,
};

static u8 SEQ_HX83102E_GTA4XLS_00_012[] = {
	0xD6,
	0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x18, 0x18,
	0x24, 0x24, 0x19, 0x19, 0x19, 0x19, 0x18, 0x18, 0x22, 0x23,
	0x20, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18,
};

static u8 SEQ_HX83102E_GTA4XLS_00_013[] = {
	0xB4,
	0x80, 0x60, 0x01, 0x01, 0x80, 0x60, 0x68, 0x50, 0x01, 0x8E,
	0x01, 0x58, 0x00, 0xFF, 0x00, 0xFF,
};

static u8 SEQ_HX83102E_GTA4XLS_00_014[] = {
	0xD3,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x08, 0x08, 0x27,
	0x27, 0x22, 0x2F, 0x23, 0x23, 0x04, 0x04, 0x32, 0x10, 0x21,
	0x00, 0x21, 0x32, 0x10, 0x1F, 0x00, 0x02, 0x32, 0x17, 0xFD,
	0x00, 0x10, 0x00, 0x00, 0x20, 0x30, 0x01, 0x55, 0x21, 0x38,
	0x01, 0x55,
};

static u8 SEQ_HX83102E_GTA4XLS_00_015[] = {
	0xD1,
	0x67, 0x0C, 0xFF, 0x05,
};

static u8 SEQ_HX83102E_GTA4XLS_00_016[] = {
	0xB1,
	0x10, 0xFA, 0xAF, 0xAF, 0x29, 0x2D, 0xB2, 0x57, 0x4D, 0x36,
	0x36, 0x36, 0x36, 0x22, 0x21, 0x15, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_017[] = {
	0xB2,
	0x00, 0xB0, 0x47, 0xD0, 0x00, 0x14, 0x3C, 0x2E,
};

static u8 SEQ_HX83102E_GTA4XLS_00_018[] = {
	0xE0,
	0x00, 0x01, 0x0A, 0x11, 0x19, 0x2B, 0x44, 0x4C, 0x55, 0x53,
	0x71, 0x76, 0x7D, 0x8C, 0x89, 0x91, 0x9A, 0xAD, 0xAC, 0x56,
	0x5E, 0x68, 0x73, 0x00, 0x09, 0x12, 0x19, 0x1F, 0x37, 0x4E,
	0x54, 0x5B, 0x57, 0x6F, 0x74, 0x79, 0x86, 0x83, 0x89, 0x90,
	0xA3, 0xA2, 0x50, 0x58, 0x64, 0x73,
};

static u8 SEQ_HX83102E_GTA4XLS_00_019[] = {
	0xC0,
	0x23, 0x23, 0x22, 0x11, 0xA2, 0x13, 0x00, 0x80, 0x00, 0x00,
	0x08, 0x00, 0x63, 0x63,
};

static u8 SEQ_HX83102E_GTA4XLS_00_020[] = {
	0xCC,
	0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_021[] = {
	0xC8,
	0x00, 0x04, 0x04, 0x00, 0x00, 0x82,
};

static u8 SEQ_HX83102E_GTA4XLS_00_022[] = {
	0xBF,
	0xFC, 0x85, 0x80,
};

static u8 SEQ_HX83102E_GTA4XLS_00_023[] = {
	0xD0,
	0x07, 0x04, 0x05,
};

static u8 SEQ_HX83102E_GTA4XLS_00_024[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_025[] = {
	0xD3,
	0x01, 0x00, 0xFC, 0x00, 0x00, 0x11, 0x10, 0x00, 0x0E,
};

static u8 SEQ_HX83102E_GTA4XLS_00_026[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_027[] = {
	0xB4,
	0x4E,
};

static u8 SEQ_HX83102E_GTA4XLS_00_028[] = {
	0xBF,
	0xF2,
};

static u8 SEQ_HX83102E_GTA4XLS_00_029[] = {
	0xD8,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xF0,
};

static u8 SEQ_HX83102E_GTA4XLS_00_030[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_031[] = {
	0xE7,
	0x12, 0x13, 0x02, 0x02, 0x2B, 0x00, 0x0E, 0x0E, 0x00, 0x02,
	0x27, 0x76, 0x1E, 0x76, 0x01, 0x27, 0x00, 0x00, 0x00, 0x00,
	0x17, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_032[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_033[] = {
	0xE7,
	0x02, 0x30, 0x01, 0x94, 0x0D, 0xB8, 0x0E,
};

static u8 SEQ_HX83102E_GTA4XLS_00_034[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_035[] = {
	0xE7,
	0xFF, 0x01, 0xFD, 0x01, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x02, 0x40,
};

static u8 SEQ_HX83102E_GTA4XLS_00_036[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_037[] = {
	0x55,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_038[] = {
	0xE4,
	0x25, 0x41, 0x2C, 0x33, 0x4C, 0x66, 0x81, 0x99, 0xB2, 0xCC,
	0xE5, 0xFF, 0xFF, 0xFF, 0x03, 0x1E, 0x1E, 0x1E, 0x1E, 0x00,
	0x00, 0x05, 0x01, 0x14,
};

static u8 SEQ_HX83102E_GTA4XLS_00_039[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_040[] = {
	0xE4,
	0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xC7, 0xB2,
	0xA0, 0x90, 0x81, 0x75, 0x69, 0x5F, 0x55, 0x4C, 0x44, 0x3D,
	0x36, 0x2F, 0x2A, 0x24, 0x1E, 0x19, 0x14, 0x10, 0x0D, 0x0B,
	0x09, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
};

static u8 SEQ_HX83102E_GTA4XLS_00_041[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_042[] = {
	0xC1,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_043[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83102E_GTA4XLS_00_044[] = {
	0xC1,
	0x01, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x27, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C,
	0x54, 0x5C, 0x63, 0x6B, 0x73, 0x7B, 0x83, 0x8A, 0x92, 0x9A,
	0xA2, 0xAA, 0xB3, 0xBA, 0xC1, 0xCA, 0xD1, 0xD9, 0xE1, 0xE9,
	0xF1, 0xF5, 0xF7, 0xF8, 0xFA, 0xFC, 0x00, 0x40, 0x4C, 0x0B,
	0xE4, 0x5F, 0xF7, 0xF5, 0x08, 0x95, 0x03, 0xC0,
};

static u8 SEQ_HX83102E_GTA4XLS_00_045[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_046[] = {
	0xC1,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x28, 0x2C, 0x30, 0x34, 0x39, 0x3D, 0x41, 0x45, 0x48, 0x4C,
	0x55, 0x5C, 0x64, 0x6C, 0x74, 0x7C, 0x84, 0x8B, 0x93, 0x9B,
	0xA3, 0xAB, 0xB4, 0xBB, 0xC3, 0xCB, 0xD3, 0xDB, 0xE3, 0xEB,
	0xF3, 0xF7, 0xF9, 0xFB, 0xFE, 0xFF, 0x01, 0x45, 0x92, 0xA1,
	0x5F, 0x3B, 0xF7, 0xFB, 0x13, 0x9B, 0xFF, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_047[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83102E_GTA4XLS_00_048[] = {
	0xC1,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x28, 0x2D, 0x31, 0x35, 0x3A, 0x3E, 0x42, 0x46, 0x49, 0x4E,
	0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E, 0x85, 0x8D, 0x95, 0x9D,
	0xA4, 0xAC, 0xB5, 0xBC, 0xC4, 0xCC, 0xD4, 0xDB, 0xE4, 0xEC,
	0xF3, 0xF7, 0xF9, 0xFC, 0xFE, 0xFF, 0x15, 0x9A, 0xFC, 0x21,
	0x5C, 0x40, 0x18, 0x0B, 0x01, 0x30, 0xFC, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_049[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_050[] = {
	0xE1,
	0x07, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_HX83102E_GTA4XLS_00_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_HX83102E_GTA4XLS_00_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_HX83102E_GTA4XLS_00_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_HX83102E_GTA4XLS_00_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

static u8 SEQ_HX83102E_GTA4XLS_00_BRIGHTNESS[] = {
	0x51,
	0x05, 0x78,	/* From 9610 orig project */
};

static u8 SEQ_HX83102E_GTA4XLS_00_WORKAROUND_01[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_WORKAROUND_02[] = {
	0xE4,
	0x2D,
};

static u8 SEQ_HX83102E_GTA4XLS_00_LOCK_OPEN[] = {
	0xB9,
	0x83, 0x10, 0x2E,
};

static u8 SEQ_HX83102E_GTA4XLS_00_LOCK_CLOSE[] = {
	0xB9,
	0x00, 0x00, 0x00,
};

static u8 SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_01[] = {
	0xE9,
	0xFF,
};

static u8 SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_02[] = {
	0xBF,
	0x02,
};

static u8 SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_03[] = {
	0xE9,
	0x00,
};


static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_sleep_out, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_sleep_in, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_display_on, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_display_off, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_brightness_on, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(hx83102e_gta4xls_00_brightness, &hx83102e_gta4xls_00_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(hx83102e_gta4xls_00_brightness, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_001, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_001, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_002, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_002, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_003, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_003, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_004, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_004, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_005, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_005, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_006, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_006, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_007, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_007, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_008, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_008, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_009, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_009, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_010, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_010, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_011, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_011, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_012, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_012, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_013, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_013, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_014, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_014, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_015, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_015, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_016, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_016, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_017, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_017, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_018, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_018, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_019, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_019, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_020, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_020, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_021, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_021, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_022, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_022, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_023, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_023, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_024, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_024, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_025, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_025, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_026, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_026, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_027, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_027, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_028, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_028, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_029, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_029, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_030, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_030, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_031, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_031, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_032, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_032, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_033, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_033, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_034, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_034, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_035, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_035, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_036, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_036, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_037, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_037, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_038, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_038, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_039, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_039, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_040, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_040, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_041, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_041, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_042, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_042, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_043, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_043, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_044, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_044, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_045, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_045, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_046, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_046, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_047, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_047, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_048, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_048, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_049, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_049, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_050, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_050, 0);


static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_workaround_01, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_WORKAROUND_01, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_workaround_02, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_WORKAROUND_02, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_lock_open, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_LOCK_OPEN, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_lock_close, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_LOCK_CLOSE, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_vcom_soft_01, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_01, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_vcom_soft_02, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_02, 0);
static DEFINE_STATIC_PACKET(hx83102e_gta4xls_00_vcom_soft_03, DSI_PKT_TYPE_WR, SEQ_HX83102E_GTA4XLS_00_VCOM_SOFT_03, 0);


static DEFINE_PANEL_MDELAY(hx83102e_gta4xls_00_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(hx83102e_gta4xls_00_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(hx83102e_gta4xls_00_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(hx83102e_gta4xls_00_wait_120msec, 120);

static DEFINE_PNOBJ_CONFIG(hx83102e_gta4xls_00_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(hx83102e_gta4xls_00_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static DEFINE_RULE_BASED_COND(gta4xls_00_cond_is_first_br,
		GTA4XLS_FIRST_BR_PROPERTY, EQ, GTA4XLS_FIRST_BR_ON);

static u8 HX83102E_GTA4XLS_00_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(hx83102e_gta4xls_00_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(hx83102e_gta4xls_00_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DB_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(hx83102e_gta4xls_00_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DC_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(hx83102e_gta4xls_00_id) = {
	{ .rditbl = &RDINFO(hx83102e_gta4xls_00_id1), .offset = 0 },
	{ .rditbl = &RDINFO(hx83102e_gta4xls_00_id2), .offset = 1 },
	{ .rditbl = &RDINFO(hx83102e_gta4xls_00_id3), .offset = 2 },
};
static DEFINE_RESOURCE(hx83102e_gta4xls_00_id, HX83102E_GTA4XLS_00_ID, RESUI(hx83102e_gta4xls_00_id));

static DEFINE_PNOBJ_CONFIG(gta4xls_set_separate_tx_off, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_OFF);
static DEFINE_PNOBJ_CONFIG(gta4xls_set_separate_tx_on, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_ON);

static void *hx83102e_gta4xls_00_init_cmdtbl[] = {
	&PNOBJ_CONFIG(gta4xls_set_separate_tx_on),
	&RESINFO(hx83102e_gta4xls_00_id),

	&PKTINFO(hx83102e_gta4xls_00_lock_open),

	&PKTINFO(hx83102e_gta4xls_00_001),
	&PKTINFO(hx83102e_gta4xls_00_002),
	&PKTINFO(hx83102e_gta4xls_00_003),
	&PKTINFO(hx83102e_gta4xls_00_004),
	&PKTINFO(hx83102e_gta4xls_00_005),
	&PKTINFO(hx83102e_gta4xls_00_006),
	&PKTINFO(hx83102e_gta4xls_00_007),
	&PKTINFO(hx83102e_gta4xls_00_008),
	&PKTINFO(hx83102e_gta4xls_00_009),
	&PKTINFO(hx83102e_gta4xls_00_010),
	&PKTINFO(hx83102e_gta4xls_00_011),
	&PKTINFO(hx83102e_gta4xls_00_012),
	&PKTINFO(hx83102e_gta4xls_00_013),
	&PKTINFO(hx83102e_gta4xls_00_014),
	&PKTINFO(hx83102e_gta4xls_00_015),
	&PKTINFO(hx83102e_gta4xls_00_016),
	&PKTINFO(hx83102e_gta4xls_00_017),
	&PKTINFO(hx83102e_gta4xls_00_018),
	&PKTINFO(hx83102e_gta4xls_00_019),
	&PKTINFO(hx83102e_gta4xls_00_020),
	&PKTINFO(hx83102e_gta4xls_00_021),
	&PKTINFO(hx83102e_gta4xls_00_022),
	&PKTINFO(hx83102e_gta4xls_00_023),
	&PKTINFO(hx83102e_gta4xls_00_024),
	&PKTINFO(hx83102e_gta4xls_00_025),
	&PKTINFO(hx83102e_gta4xls_00_026),
	&PKTINFO(hx83102e_gta4xls_00_027),
	&PKTINFO(hx83102e_gta4xls_00_028),
	&PKTINFO(hx83102e_gta4xls_00_029),
	&PKTINFO(hx83102e_gta4xls_00_030),
	&PKTINFO(hx83102e_gta4xls_00_031),
	&PKTINFO(hx83102e_gta4xls_00_032),
	&PKTINFO(hx83102e_gta4xls_00_033),
	&PKTINFO(hx83102e_gta4xls_00_034),
	&PKTINFO(hx83102e_gta4xls_00_035),
	&PKTINFO(hx83102e_gta4xls_00_036),
	&PKTINFO(hx83102e_gta4xls_00_037),
	&PKTINFO(hx83102e_gta4xls_00_038),
	&PKTINFO(hx83102e_gta4xls_00_039),
	&PKTINFO(hx83102e_gta4xls_00_040),
	&PKTINFO(hx83102e_gta4xls_00_041),
	&PKTINFO(hx83102e_gta4xls_00_042),
	&PKTINFO(hx83102e_gta4xls_00_043),
	&PKTINFO(hx83102e_gta4xls_00_044),
	&PKTINFO(hx83102e_gta4xls_00_045),
	&PKTINFO(hx83102e_gta4xls_00_046),
	&PKTINFO(hx83102e_gta4xls_00_047),
	&PKTINFO(hx83102e_gta4xls_00_048),
	&PKTINFO(hx83102e_gta4xls_00_049),
	&PKTINFO(hx83102e_gta4xls_00_050),

	&PKTINFO(hx83102e_gta4xls_00_sleep_out),
	&DLYINFO(hx83102e_gta4xls_00_wait_120msec),
	&PKTINFO(hx83102e_gta4xls_00_display_on),

	&PKTINFO(hx83102e_gta4xls_00_lock_close),

	&PNOBJ_CONFIG(gta4xls_set_separate_tx_off),
	&DLYINFO(hx83102e_gta4xls_00_wait_20msec),
};

static void *hx83102e_gta4xls_00_res_init_cmdtbl[] = {
	&RESINFO(hx83102e_gta4xls_00_id),
};

static void *hx83102e_gta4xls_00_set_bl_cmdtbl[] = {
	&PKTINFO(hx83102e_gta4xls_00_brightness),
	/* If first non zero br After BL ON */
	&CONDINFO_IF(gta4xls_00_cond_is_first_br),
		&DLYINFO(hx83102e_gta4xls_00_wait_1msec),
		&PKTINFO(hx83102e_gta4xls_00_lock_open),
		&PKTINFO(hx83102e_gta4xls_00_workaround_01),
		&PKTINFO(hx83102e_gta4xls_00_workaround_02),
		&PKTINFO(hx83102e_gta4xls_00_lock_close),
	&CONDINFO_FI(gta4xls_00_cond_is_first_br),
};

static DEFINE_SEQINFO(hx83102e_gta4xls_00_set_bl_seq, hx83102e_gta4xls_00_set_bl_cmdtbl);

static void *hx83102e_gta4xls_00_display_on_cmdtbl[] = {
	&DLYINFO(hx83102e_gta4xls_00_wait_20msec),
	&SEQINFO(hx83102e_gta4xls_00_set_bl_seq),
};

static void *hx83102e_gta4xls_00_display_off_cmdtbl[] = {
	&PKTINFO(hx83102e_gta4xls_00_display_off),
};

static void *hx83102e_gta4xls_00_exit_cmdtbl[] = {
	&PKTINFO(hx83102e_gta4xls_00_lock_open),
	&PKTINFO(hx83102e_gta4xls_00_vcom_soft_01),
	&PKTINFO(hx83102e_gta4xls_00_vcom_soft_02),
	&PKTINFO(hx83102e_gta4xls_00_vcom_soft_03),
	&PKTINFO(hx83102e_gta4xls_00_lock_close),
	&PKTINFO(hx83102e_gta4xls_00_sleep_in),
	&DLYINFO(hx83102e_gta4xls_00_wait_40msec),
};

static void *hx83102e_gta4xls_00_display_mode_cmdtbl[] = {
	&PNOBJ_CONFIG(hx83102e_gta4xls_00_set_wait_tx_done_property_off),
		&PKTINFO(hx83102e_gta4xls_00_brightness),
		/* Will flush on next VFP */
	&PNOBJ_CONFIG(hx83102e_gta4xls_00_set_wait_tx_done_property_auto),
};

static struct seqinfo hx83102e_gta4xls_00_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, hx83102e_gta4xls_00_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, hx83102e_gta4xls_00_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, hx83102e_gta4xls_00_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, hx83102e_gta4xls_00_display_mode_cmdtbl), /* Dummy */
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, hx83102e_gta4xls_00_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, hx83102e_gta4xls_00_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, hx83102e_gta4xls_00_exit_cmdtbl),
};

/* BLIC SETTING START */
static struct blic_data hx83102e_gta4xls_00_isl98608_blic_data = {
	.name = "isl98608",
	.seqtbl = NULL,
	.nr_seqtbl = 0,
};

static struct blic_data *hx83102e_gta4xls_00_blic_tbl[] = {
	&hx83102e_gta4xls_00_isl98608_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info hx83102e_gta4xls_00_panel_info = {
	.ldi_name = "hx83102e",
	.name = "hx83102e_gta4xls_00",
	.model = "BOE_GTA4XLS",
	.vendor = "BOE",
	.id = 0x13D230,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &hx83102e_gta4xls_00_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(hx83102e_gta4xls_00_default_resol),
		.resol = hx83102e_gta4xls_00_default_resol,
	},
	.vrrtbl = hx83102e_gta4xls_00_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(hx83102e_gta4xls_00_default_vrrtbl),
	.maptbl = hx83102e_gta4xls_00_maptbl,
	.nr_maptbl = ARRAY_SIZE(hx83102e_gta4xls_00_maptbl),
	.seqtbl = hx83102e_gta4xls_00_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(hx83102e_gta4xls_00_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &hx83102e_gta4xls_00_panel_dimming_info,
	},
	.blic_data_tbl = hx83102e_gta4xls_00_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(hx83102e_gta4xls_00_blic_tbl),
};
#endif /* __HX83102E_GTA4XLS_00_PANEL_H__ */
