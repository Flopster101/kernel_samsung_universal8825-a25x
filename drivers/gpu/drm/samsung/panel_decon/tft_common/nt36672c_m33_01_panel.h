/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c/nt36672c_m33x_01_panel.h
 *
 * Header file for NT36672C Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36672C_M33X_01_PANEL_H__
#define __NT36672C_M33X_01_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "nt36672c_m33_01_resol.h"

#undef __pn_name__
#define __pn_name__	m33x

#undef __PN_NAME__
#define __PN_NAME__

#define NT36672C_NR_STEP (256)
#define NT36672C_HBM_STEP (51)
#define NT36672C_TOTAL_STEP (NT36672C_NR_STEP + NT36672C_HBM_STEP) /* 0 ~ 306 */

static unsigned int nt36672c_m33x_01_brt_tbl[NT36672C_TOTAL_STEP] = {
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
	BRT(256), BRT(257), BRT(258), BRT(259), BRT(260),
	BRT(261), BRT(262), BRT(263), BRT(264), BRT(265), BRT(266), BRT(267), BRT(268), BRT(269), BRT(270),
	BRT(271), BRT(272), BRT(273), BRT(274), BRT(275), BRT(276), BRT(277), BRT(278), BRT(279), BRT(280),
	BRT(281), BRT(282), BRT(283), BRT(284), BRT(285), BRT(286), BRT(287), BRT(288), BRT(289), BRT(290),
	BRT(291), BRT(292), BRT(293), BRT(294), BRT(295), BRT(296), BRT(297), BRT(298), BRT(299), BRT(300),
	BRT(301), BRT(302), BRT(303), BRT(304), BRT(305), BRT(306),
};

static unsigned int nt36672c_m33x_01_step_cnt_tbl[NT36672C_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 306] = 1,
};

struct brightness_table nt36672c_m33x_01_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = nt36672c_m33x_01_brt_tbl,
	.sz_brt = ARRAY_SIZE(nt36672c_m33x_01_brt_tbl),
	.sz_ui_brt = NT36672C_NR_STEP,
	.sz_hbm_brt = NT36672C_HBM_STEP,
	.lum = nt36672c_m33x_01_brt_tbl,
	.sz_lum = ARRAY_SIZE(nt36672c_m33x_01_brt_tbl),
	.sz_ui_lum = NT36672C_NR_STEP,
	.sz_hbm_lum = NT36672C_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = nt36672c_m33x_01_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(nt36672c_m33x_01_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info nt36672c_m33x_01_panel_dimming_info = {
	.name = "nt36672c_m33x",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &nt36672c_m33x_01_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 nt36672c_m33x_01_brt_table[NT36672C_TOTAL_STEP][1] = {
	{0},
	{1}, {1}, {2}, {2}, {3}, {3}, {4}, {4}, {5}, {5},
	{6}, {6}, {7}, {7}, {8}, {8}, {9}, {9}, {10}, {10},
	{11}, {11}, {12}, {12}, {13}, {13}, {14}, {14}, {15}, {15},
	{16}, {16}, {17}, {17}, {18}, {19}, {19}, {20}, {20}, {21},
	{22}, {22}, {23}, {23}, {24}, {25}, {25}, {26}, {26}, {27},
	{28}, {28}, {29}, {29}, {30}, {31}, {31}, {32}, {32}, {33},
	{34}, {34}, {35}, {35}, {36}, {37}, {37}, {38}, {38}, {39},
	{40}, {40}, {41}, {41}, {42}, {43}, {43}, {44}, {44}, {45},
	{46}, {47}, {47}, {48}, {49}, {50}, {51}, {51}, {52}, {53},
	{54}, {55}, {55}, {56}, {57}, {58}, {58}, {59}, {60}, {61},
	{62}, {62}, {63}, {64}, {65}, {66}, {66}, {67}, {68}, {69},
	{70}, {70}, {71}, {72}, {73}, {74}, {74}, {75}, {76}, {77},
	{77}, {78}, {79}, {80}, {81}, {81}, {82}, {83}, {84}, {85},
	{86}, {87}, {88}, {89}, {90}, {91}, {92}, {93}, {94}, {95},
	{96}, {97}, {98}, {99}, {100}, {101}, {102}, {103}, {104}, {105},
	{106}, {107}, {108}, {109}, {110}, {111}, {112}, {113}, {114}, {116},
	{117}, {118}, {119}, {120}, {121}, {122}, {123}, {124}, {125}, {126},
	{127}, {128}, {129}, {130}, {131}, {132}, {133}, {134}, {135}, {136},
	{137}, {138}, {139}, {140}, {141}, {142}, {143}, {144}, {145}, {146},
	{147}, {148}, {149}, {150}, {151}, {152}, {153}, {154}, {155}, {156},
	{157}, {158}, {159}, {160}, {161}, {162}, {163}, {164}, {165}, {166},
	{167}, {168}, {169}, {170}, {171}, {172}, {173}, {174}, {175}, {176},
	{177}, {178}, {179}, {181}, {182}, {183}, {184}, {185}, {186}, {187},
	{188}, {189}, {190}, {191}, {192}, {193}, {194}, {195}, {196}, {197},
	{198}, {199}, {200}, {201}, {202}, {203}, {204}, {205}, {206}, {207},
	{208}, {209}, {210}, {211}, {212}, {213}, {214}, {215}, {215}, {216},
	{217}, {218}, {219}, {220}, {220}, {221}, {222}, {223}, {224}, {225},
	{225}, {226}, {227}, {228}, {229}, {230}, {231}, {231}, {232}, {233},
	{234}, {235}, {236}, {236}, {237}, {238}, {239}, {240}, {241}, {242},
	{242}, {243}, {244}, {245}, {246}, {247}, {247}, {248}, {249}, {250},
	{251}, {252}, {252}, {253}, {254}, {255},
};

static struct maptbl nt36672c_m33x_01_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(nt36672c_m33x_01_brt_table, init_brt_table, getidx_brt_table, copy_common_maptbl),
};

static u8 SEQ_NT36672C_M33X_01_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_NT36672C_M33X_01_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_NT36672C_M33X_01_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_NT36672C_M33X_01_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_NT36672C_M33X_01_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

/* < CABC Mode control Function > */

static u8 SEQ_NT36672C_M33X_01_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

/* Display config (1) */
static u8 SEQ_NT36672C_M33X_01_001[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36672C_M33X_01_002[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_003[] = {
	0x36,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_004[] = {
	0x3B,
	0x03, 0x12, 0x36, 0x04, 0x04,
};

static u8 SEQ_NT36672C_M33X_01_005[] = {
	0xB0,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_006[] = {
	0xC0,
	0x03,
};

static u8 SEQ_NT36672C_M33X_01_007[] = {
	0xC1,
	0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B,
	0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7,
};

static u8 SEQ_NT36672C_M33X_01_008[] = {
	0xC2,
	0x1B, 0xA0,
};

static u8 SEQ_NT36672C_M33X_01_009[] = {
	0xFF,
	0x2A,
};

static u8 SEQ_NT36672C_M33X_01_010[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_011[] = {
	0x1B,
	0x12,
};

static u8 SEQ_NT36672C_M33X_01_012[] = {
	0x1D,
	0x36,
};

static u8 SEQ_NT36672C_M33X_01_013[] = {
	0xFF,
	0xE0,
};

static u8 SEQ_NT36672C_M33X_01_014[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_015[] = {
	0x35,
	0x82,
};

static u8 SEQ_NT36672C_M33X_01_016[] = {
	0x85,
	0x30,
};

static u8 SEQ_NT36672C_M33X_01_017[] = {
	0xFF,
	0xF0,
};

static u8 SEQ_NT36672C_M33X_01_018[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_019[] = {
	0x5A,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_020[] = {
	0xD2,
	0x50,
};

static u8 SEQ_NT36672C_M33X_01_021[] = {
	0xFF,
	0xC0,
};

static u8 SEQ_NT36672C_M33X_01_022[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_023[] = {
	0x9C,
	0x11,
};

static u8 SEQ_NT36672C_M33X_01_024[] = {
	0x9D,
	0x11,
};

static u8 SEQ_NT36672C_M33X_01_025[] = {
	0xFF,
	0xD0,
};

static u8 SEQ_NT36672C_M33X_01_026[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_027[] = {
	0x53,
	0x22,
};

static u8 SEQ_NT36672C_M33X_01_028[] = {
	0x54,
	0x02,
};

static u8 SEQ_NT36672C_M33X_01_029[] = {
	0xFF,
	0x23,
};

static u8 SEQ_NT36672C_M33X_01_030[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_031[] = {
	0x00,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_032[] = {
	0x07,
	0x60,
};

static u8 SEQ_NT36672C_M33X_01_033[] = {
	0x08,
	0x06,
};

static u8 SEQ_NT36672C_M33X_01_034[] = {
	0x09,
	0x1C,
};

static u8 SEQ_NT36672C_M33X_01_035[] = {
	0x0A,
	0x2B,
};

static u8 SEQ_NT36672C_M33X_01_036[] = {
	0x0B,
	0x2B,
};

static u8 SEQ_NT36672C_M33X_01_037[] = {
	0x0C,
	0x2B,
};

static u8 SEQ_NT36672C_M33X_01_038[] = {
	0x0D,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_039[] = {
	0x10,
	0x50,
};

static u8 SEQ_NT36672C_M33X_01_040[] = {
	0x11,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_041[] = {
	0x12,
	0x95,
};

static u8 SEQ_NT36672C_M33X_01_042[] = {
	0x15,
	0x68,
};

static u8 SEQ_NT36672C_M33X_01_043[] = {
	0x16,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_01_044[] = {
	0x19,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_045[] = {
	0x1A,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_046[] = {
	0x1B,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_047[] = {
	0x1C,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_048[] = {
	0x1D,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_049[] = {
	0x1E,
	0x03,
};

static u8 SEQ_NT36672C_M33X_01_050[] = {
	0x1F,
	0x05,
};

static u8 SEQ_NT36672C_M33X_01_051[] = {
	0x20,
	0x0C,
};

static u8 SEQ_NT36672C_M33X_01_052[] = {
	0x21,
	0x13,
};

static u8 SEQ_NT36672C_M33X_01_053[] = {
	0x22,
	0x17,
};

static u8 SEQ_NT36672C_M33X_01_054[] = {
	0x23,
	0x1D,
};

static u8 SEQ_NT36672C_M33X_01_055[] = {
	0x24,
	0x23,
};

static u8 SEQ_NT36672C_M33X_01_056[] = {
	0x25,
	0x2C,
};

static u8 SEQ_NT36672C_M33X_01_057[] = {
	0x26,
	0x33,
};

static u8 SEQ_NT36672C_M33X_01_058[] = {
	0x27,
	0x39,
};

static u8 SEQ_NT36672C_M33X_01_059[] = {
	0x28,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_01_060[] = {
	0x29,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_01_061[] = {
	0x2A,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_01_062[] = {
	0x2B,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_01_063[] = {
	0x30,
	0xFF,
};

static u8 SEQ_NT36672C_M33X_01_064[] = {
	0x31,
	0xFE,
};

static u8 SEQ_NT36672C_M33X_01_065[] = {
	0x32,
	0xFD,
};

static u8 SEQ_NT36672C_M33X_01_066[] = {
	0x33,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_01_067[] = {
	0x34,
	0xFB,
};

static u8 SEQ_NT36672C_M33X_01_068[] = {
	0x35,
	0xFA,
};

static u8 SEQ_NT36672C_M33X_01_069[] = {
	0x36,
	0xF9,
};

static u8 SEQ_NT36672C_M33X_01_070[] = {
	0x37,
	0xF7,
};

static u8 SEQ_NT36672C_M33X_01_071[] = {
	0x38,
	0xF5,
};

static u8 SEQ_NT36672C_M33X_01_072[] = {
	0x39,
	0xF3,
};

static u8 SEQ_NT36672C_M33X_01_073[] = {
	0x3A,
	0xF1,
};

static u8 SEQ_NT36672C_M33X_01_074[] = {
	0x3B,
	0xEE,
};

static u8 SEQ_NT36672C_M33X_01_075[] = {
	0x3D,
	0xEC,
};

static u8 SEQ_NT36672C_M33X_01_076[] = {
	0x3F,
	0xEA,
};

static u8 SEQ_NT36672C_M33X_01_077[] = {
	0x40,
	0xE8,
};

static u8 SEQ_NT36672C_M33X_01_078[] = {
	0x41,
	0xE6,
};

static u8 SEQ_NT36672C_M33X_01_079[] = {
	0x04,
	0x00,
};

static u8 SEQ_NT36672C_M33X_01_080[] = {
	0xA0,
	0x11,
};

static u8 SEQ_NT36672C_M33X_01_081[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36672C_M33X_01_082[] = {
	0xFB,
	0x01,
};

/*
 * static u8 SEQ_NT36672C_M33X_01_083[] = {
 *	0x53,
 *	0x2C,
 * };
 */

static u8 SEQ_NT36672C_M33X_01_084[] = {
	0x55,
	0x01,
};

static u8 SEQ_NT36672C_M33X_01_085[] = {
	0x68,
	0x00, 0x01,
};


static DEFINE_STATIC_PACKET(nt36672c_m33x_01_sleep_out, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_sleep_in, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_display_on, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_display_off, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_brightness_on, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(nt36672c_m33x_01_brightness, &nt36672c_m33x_01_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(nt36672c_m33x_01_brightness, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(nt36672c_m33x_01_001, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_001, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_002, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_002, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_003, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_003, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_004, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_004, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_005, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_005, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_006, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_006, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_007, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_007, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_008, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_008, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_009, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_009, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_010, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_010, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_011, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_011, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_012, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_012, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_013, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_013, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_014, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_014, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_015, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_015, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_016, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_016, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_017, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_017, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_018, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_018, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_019, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_019, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_020, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_020, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_021, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_021, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_022, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_022, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_023, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_023, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_024, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_024, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_025, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_025, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_026, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_026, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_027, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_027, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_028, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_028, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_029, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_029, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_030, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_030, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_031, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_031, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_032, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_032, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_033, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_033, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_034, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_034, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_035, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_035, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_036, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_036, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_037, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_037, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_038, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_038, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_039, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_039, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_040, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_040, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_041, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_041, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_042, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_042, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_043, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_043, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_044, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_044, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_045, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_045, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_046, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_046, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_047, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_047, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_048, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_048, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_049, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_049, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_050, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_050, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_051, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_051, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_052, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_052, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_053, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_053, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_054, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_054, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_055, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_055, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_056, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_056, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_057, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_057, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_058, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_058, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_059, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_059, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_060, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_060, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_061, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_061, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_062, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_062, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_063, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_063, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_064, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_064, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_065, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_065, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_066, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_066, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_067, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_067, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_068, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_068, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_069, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_069, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_070, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_070, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_071, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_071, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_072, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_072, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_073, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_073, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_074, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_074, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_075, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_075, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_076, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_076, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_077, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_077, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_078, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_078, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_079, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_079, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_080, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_080, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_081, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_081, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_082, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_082, 0);
/*
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_083, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_083, 0);
*/
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_084, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_084, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_085, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_01_085, 0);

static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_60msec, 60); /* 4 frame */
static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_wait_2msec, 2);

static DEFINE_SETPROP_VALUE(m33x_01_set_wait_tx_done_property_off, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_SETPROP_VALUE(m33x_01_set_wait_tx_done_property_auto, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static void *nt36672c_m33x_01_init_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_01_001),
	&PKTINFO(nt36672c_m33x_01_002),
	&PKTINFO(nt36672c_m33x_01_003),
	&PKTINFO(nt36672c_m33x_01_004),
	&PKTINFO(nt36672c_m33x_01_005),
	&PKTINFO(nt36672c_m33x_01_006),
	&PKTINFO(nt36672c_m33x_01_007),
	&PKTINFO(nt36672c_m33x_01_008),
	&PKTINFO(nt36672c_m33x_01_009),
	&PKTINFO(nt36672c_m33x_01_010),
	&PKTINFO(nt36672c_m33x_01_011),
	&PKTINFO(nt36672c_m33x_01_012),
	&PKTINFO(nt36672c_m33x_01_013),
	&PKTINFO(nt36672c_m33x_01_014),
	&PKTINFO(nt36672c_m33x_01_015),
	&PKTINFO(nt36672c_m33x_01_016),
	&PKTINFO(nt36672c_m33x_01_017),
	&PKTINFO(nt36672c_m33x_01_018),
	&PKTINFO(nt36672c_m33x_01_019),
	&PKTINFO(nt36672c_m33x_01_020),
	&PKTINFO(nt36672c_m33x_01_021),
	&PKTINFO(nt36672c_m33x_01_022),
	&PKTINFO(nt36672c_m33x_01_023),
	&PKTINFO(nt36672c_m33x_01_024),
	&PKTINFO(nt36672c_m33x_01_025),
	&PKTINFO(nt36672c_m33x_01_026),
	&PKTINFO(nt36672c_m33x_01_027),
	&PKTINFO(nt36672c_m33x_01_028),
	&PKTINFO(nt36672c_m33x_01_029),
	&PKTINFO(nt36672c_m33x_01_030),
	&PKTINFO(nt36672c_m33x_01_031),
	&PKTINFO(nt36672c_m33x_01_032),
	&PKTINFO(nt36672c_m33x_01_033),
	&PKTINFO(nt36672c_m33x_01_034),
	&PKTINFO(nt36672c_m33x_01_035),
	&PKTINFO(nt36672c_m33x_01_036),
	&PKTINFO(nt36672c_m33x_01_037),
	&PKTINFO(nt36672c_m33x_01_038),
	&PKTINFO(nt36672c_m33x_01_039),
	&PKTINFO(nt36672c_m33x_01_040),
	&PKTINFO(nt36672c_m33x_01_041),
	&PKTINFO(nt36672c_m33x_01_042),
	&PKTINFO(nt36672c_m33x_01_043),
	&PKTINFO(nt36672c_m33x_01_044),
	&PKTINFO(nt36672c_m33x_01_045),
	&PKTINFO(nt36672c_m33x_01_046),
	&PKTINFO(nt36672c_m33x_01_047),
	&PKTINFO(nt36672c_m33x_01_048),
	&PKTINFO(nt36672c_m33x_01_049),
	&PKTINFO(nt36672c_m33x_01_050),
	&PKTINFO(nt36672c_m33x_01_051),
	&PKTINFO(nt36672c_m33x_01_052),
	&PKTINFO(nt36672c_m33x_01_053),
	&PKTINFO(nt36672c_m33x_01_054),
	&PKTINFO(nt36672c_m33x_01_055),
	&PKTINFO(nt36672c_m33x_01_056),
	&PKTINFO(nt36672c_m33x_01_057),
	&PKTINFO(nt36672c_m33x_01_058),
	&PKTINFO(nt36672c_m33x_01_059),
	&PKTINFO(nt36672c_m33x_01_060),
	&PKTINFO(nt36672c_m33x_01_061),
	&PKTINFO(nt36672c_m33x_01_062),
	&PKTINFO(nt36672c_m33x_01_063),
	&PKTINFO(nt36672c_m33x_01_064),
	&PKTINFO(nt36672c_m33x_01_065),
	&PKTINFO(nt36672c_m33x_01_066),
	&PKTINFO(nt36672c_m33x_01_067),
	&PKTINFO(nt36672c_m33x_01_068),
	&PKTINFO(nt36672c_m33x_01_069),
	&PKTINFO(nt36672c_m33x_01_070),
	&PKTINFO(nt36672c_m33x_01_071),
	&PKTINFO(nt36672c_m33x_01_072),
	&PKTINFO(nt36672c_m33x_01_073),
	&PKTINFO(nt36672c_m33x_01_074),
	&PKTINFO(nt36672c_m33x_01_075),
	&PKTINFO(nt36672c_m33x_01_076),
	&PKTINFO(nt36672c_m33x_01_077),
	&PKTINFO(nt36672c_m33x_01_078),
	&PKTINFO(nt36672c_m33x_01_079),
	&PKTINFO(nt36672c_m33x_01_080),
	&PKTINFO(nt36672c_m33x_01_081),
	&PKTINFO(nt36672c_m33x_01_082),
/*
	&PKTINFO(nt36672c_m33x_01_083),
*/
	&PKTINFO(nt36672c_m33x_01_084),
	&PKTINFO(nt36672c_m33x_01_085),
	&PKTINFO(nt36672c_m33x_01_sleep_out),
	&DLYINFO(nt36672c_m33x_01_wait_100msec),
	&PKTINFO(nt36672c_m33x_01_display_on),
};

static void *nt36672c_m33x_01_res_init_cmdtbl[] = {
	&tft_common_restbl[RES_ID],
};

static void *nt36672c_m33x_01_set_bl_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_01_brightness),
};

static void *nt36672c_m33x_01_display_on_cmdtbl[] = {
	&DLYINFO(nt36672c_m33x_01_wait_40msec),
	&PKTINFO(nt36672c_m33x_01_brightness),
	&PKTINFO(nt36672c_m33x_01_brightness_on),
};

static void *nt36672c_m33x_01_display_off_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_01_display_off),
	&DLYINFO(nt36672c_m33x_01_wait_20msec),
};

static void *nt36672c_m33x_01_exit_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_01_sleep_in),
};

static void *nt36672c_m33x_01_display_mode_cmdtbl[] = {
	&SETPROP(m33x_01_set_wait_tx_done_property_off),
		&PKTINFO(nt36672c_m33x_01_brightness),
		/* Will flush on next VFP */
	&SETPROP(m33x_01_set_wait_tx_done_property_auto),
};

static void *nt36672c_m33x_01_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo nt36672c_m33x_01_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", nt36672c_m33x_01_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", nt36672c_m33x_01_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", nt36672c_m33x_01_set_bl_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("set-bl-seq", nt36672c_m33x_01_display_mode_cmdtbl), /* Dummy */
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", nt36672c_m33x_01_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", nt36672c_m33x_01_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", nt36672c_m33x_01_exit_cmdtbl),
};


/* BLIC SETTING START */
static u8 NT36672C_M33X_01_KTZ8864_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x03,
	0x05, 0xC2,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 NT36672C_M33X_01_KTZ8864_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 NT36672C_M33X_01_KTZ8864_I2C_DUMP[] = {
	0x0C, 0x00,
	0x0D, 0x00,
	0x0E, 0x00,
	0x09, 0x00,
	0x09, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x11, 0x00,
	0x04, 0x00,
	0x05, 0x00,
	0x10, 0x00,
	0x08, 0x00,
};

static DEFINE_STATIC_PACKET(nt36672c_m33x_01_ktz8864_i2c_init, I2C_PKT_TYPE_WR, NT36672C_M33X_01_KTZ8864_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_ktz8864_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36672C_M33X_01_KTZ8864_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_01_ktz8864_i2c_dump, I2C_PKT_TYPE_RD, NT36672C_M33X_01_KTZ8864_I2C_DUMP, 0);

static DEFINE_PANEL_MDELAY(nt36672c_m33x_01_blic_wait_delay, 2);

static void *nt36672c_m33x_01_ktz8864_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_01_ktz8864_i2c_dump),
#endif
	&PKTINFO(nt36672c_m33x_01_ktz8864_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_01_ktz8864_i2c_dump),
#endif
};

static void *nt36672c_m33x_01_ktz8864_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_01_ktz8864_i2c_dump),
#endif
	&PKTINFO(nt36672c_m33x_01_ktz8864_i2c_exit_blen),
};

static struct seqinfo nt36672c_m33x_01_ktz8864_seq_tbl[MAX_PANEL_BLIC_SEQ] = {
	[PANEL_BLIC_I2C_ON_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36672c_m33x_01_ktz8864_init_cmdtbl),
	[PANEL_BLIC_I2C_OFF_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36672c_m33x_01_ktz8864_exit_cmdtbl),
};

static struct blic_data nt36672c_m33x_01_ktz8864_blic_data = {
	.name = "ktz8864",
	.seqtbl = nt36672c_m33x_01_ktz8864_seq_tbl,
};

static struct blic_data *nt36672c_m33x_01_blic_tbl[] = {
	&nt36672c_m33x_01_ktz8864_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info nt36672c_m33x_01_panel_info = {
	.ldi_name = "nt36672c",
	.name = "nt36672c_m33x_01",
	.model = "tianma_6_58_inch",
	.vendor = "TMC",
	.id = 0x4BF240,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &nt36672c_m33x_01_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36672c_m33x_01_default_resol),
		.resol = nt36672c_m33x_01_default_resol,
	},
	.vrrtbl = nt36672c_m33x_01_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(nt36672c_m33x_01_default_vrrtbl),
	.maptbl = nt36672c_m33x_01_maptbl,
	.nr_maptbl = ARRAY_SIZE(nt36672c_m33x_01_maptbl),
	.seqtbl = nt36672c_m33x_01_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(nt36672c_m33x_01_seqtbl),
	.rditbl = tft_common_rditbl,
	.nr_rditbl = ARRAY_SIZE(tft_common_rditbl),
	.restbl = tft_common_restbl,
	.nr_restbl = ARRAY_SIZE(tft_common_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &nt36672c_m33x_01_panel_dimming_info,
	},
	.blic_data_tbl = nt36672c_m33x_01_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(nt36672c_m33x_01_blic_tbl),
};

#endif /* __NT36672C_M33X_01_PANEL_H__ */
