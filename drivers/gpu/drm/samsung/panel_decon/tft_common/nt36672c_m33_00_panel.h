/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c/nt36672c_m33x_00_panel.h
 *
 * Header file for NT36672C Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36672C_M33X_00_PANEL_H__
#define __NT36672C_M33X_00_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "nt36672c_m33_00_resol.h"

#undef __pn_name__
#define __pn_name__	m33x

#undef __PN_NAME__
#define __PN_NAME__

#define NT36672C_NR_STEP (256)
#define NT36672C_HBM_STEP (51)
#define NT36672C_TOTAL_STEP (NT36672C_NR_STEP + NT36672C_HBM_STEP) /* 0 ~ 306 */

static unsigned int nt36672c_m33x_00_brt_tbl[NT36672C_TOTAL_STEP] = {
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

static unsigned int nt36672c_m33x_00_step_cnt_tbl[NT36672C_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 306] = 1,
};

struct brightness_table nt36672c_m33x_00_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = nt36672c_m33x_00_brt_tbl,
	.sz_brt = ARRAY_SIZE(nt36672c_m33x_00_brt_tbl),
	.sz_ui_brt = NT36672C_NR_STEP,
	.sz_hbm_brt = NT36672C_HBM_STEP,
	.lum = nt36672c_m33x_00_brt_tbl,
	.sz_lum = ARRAY_SIZE(nt36672c_m33x_00_brt_tbl),
	.sz_ui_lum = NT36672C_NR_STEP,
	.sz_hbm_lum = NT36672C_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = nt36672c_m33x_00_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(nt36672c_m33x_00_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info nt36672c_m33x_00_panel_dimming_info = {
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
	.brt_tbl = &nt36672c_m33x_00_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 nt36672c_m33x_00_brt_table[NT36672C_TOTAL_STEP][1] = {
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

static struct maptbl nt36672c_m33x_00_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(nt36672c_m33x_00_brt_table, init_brt_table, getidx_brt_table, copy_common_maptbl),
};

static u8 SEQ_NT36672C_M33X_00_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_NT36672C_M33X_00_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_NT36672C_M33X_00_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_NT36672C_M33X_00_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_NT36672C_M33X_00_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 SEQ_NT36672C_M33X_00_BRIGHTNESS_ON[] = {
	0x53,
	0x2C,
};

static u8 SEQ_NT36672C_M33X_00_001[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_002[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_003[] = {
	0xB0,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_004[] = {
	0xC1,
	0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B,
	0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7,
};

static u8 SEQ_NT36672C_M33X_00_005[] = {
	0xC2,
	0x1B, 0xA0,
};

static u8 SEQ_NT36672C_M33X_00_006[] = {
	0xFF,
	0x20,
};

static u8 SEQ_NT36672C_M33X_00_007[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_008[] = {
	0x01,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_009[] = {
	0x06,
	0x64,
};

static u8 SEQ_NT36672C_M33X_00_010[] = {
	0x07,
	0x28,
};

static u8 SEQ_NT36672C_M33X_00_011[] = {
	0x17,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_012[] = {
	0x1B,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_013[] = {
	0x1F,
	0x02,
};

static u8 SEQ_NT36672C_M33X_00_014[] = {
	0x20,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_015[] = {
	0x5C,
	0x90,
};

static u8 SEQ_NT36672C_M33X_00_016[] = {
	0x5E,
	0xB0,
};

static u8 SEQ_NT36672C_M33X_00_017[] = {
	0x69,
	0xD0,
};

static u8 SEQ_NT36672C_M33X_00_018[] = {
	0x95,
	0xD1,
};

static u8 SEQ_NT36672C_M33X_00_019[] = {
	0x96,
	0xD1,
};

static u8 SEQ_NT36672C_M33X_00_020[] = {
	0xF2,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_021[] = {
	0xF3,
	0x54,
};

static u8 SEQ_NT36672C_M33X_00_022[] = {
	0xF4,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_023[] = {
	0xF5,
	0x54,
};

static u8 SEQ_NT36672C_M33X_00_024[] = {
	0xF6,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_025[] = {
	0xF7,
	0x54,
};

static u8 SEQ_NT36672C_M33X_00_026[] = {
	0xF8,
	0x66,
};

static u8 SEQ_NT36672C_M33X_00_027[] = {
	0xF9,
	0x54,
};

static u8 SEQ_NT36672C_M33X_00_028[] = {
	0xFF,
	0x21,
};

static u8 SEQ_NT36672C_M33X_00_029[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_030[] = {
	0xFF,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_031[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_032[] = {
	0x00,
	0x26,
};

static u8 SEQ_NT36672C_M33X_00_033[] = {
	0x01,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_034[] = {
	0x02,
	0x27,
};

static u8 SEQ_NT36672C_M33X_00_035[] = {
	0x03,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_036[] = {
	0x04,
	0x28,
};

static u8 SEQ_NT36672C_M33X_00_037[] = {
	0x05,
	0x17,
};

static u8 SEQ_NT36672C_M33X_00_038[] = {
	0x07,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_039[] = {
	0x08,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_040[] = {
	0x0A,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_041[] = {
	0x0C,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_042[] = {
	0x0D,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_043[] = {
	0x0E,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_044[] = {
	0x10,
	0x2D,
};

static u8 SEQ_NT36672C_M33X_00_045[] = {
	0x11,
	0x2F,
};

static u8 SEQ_NT36672C_M33X_00_046[] = {
	0x12,
	0x31,
};

static u8 SEQ_NT36672C_M33X_00_047[] = {
	0x13,
	0x33,
};

static u8 SEQ_NT36672C_M33X_00_048[] = {
	0x15,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_00_049[] = {
	0x17,
	0x0C,
};

static u8 SEQ_NT36672C_M33X_00_050[] = {
	0x18,
	0x26,
};

static u8 SEQ_NT36672C_M33X_00_051[] = {
	0x19,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_052[] = {
	0x1A,
	0x27,
};

static u8 SEQ_NT36672C_M33X_00_053[] = {
	0x1B,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_054[] = {
	0x1C,
	0x28,
};

static u8 SEQ_NT36672C_M33X_00_055[] = {
	0x1D,
	0x17,
};

static u8 SEQ_NT36672C_M33X_00_056[] = {
	0x1F,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_057[] = {
	0x20,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_058[] = {
	0x22,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_059[] = {
	0x24,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_060[] = {
	0x25,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_061[] = {
	0x26,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_062[] = {
	0x28,
	0x2C,
};

static u8 SEQ_NT36672C_M33X_00_063[] = {
	0x29,
	0x2E,
};

static u8 SEQ_NT36672C_M33X_00_064[] = {
	0x2A,
	0x30,
};

static u8 SEQ_NT36672C_M33X_00_065[] = {
	0x2B,
	0x32,
};

static u8 SEQ_NT36672C_M33X_00_066[] = {
	0x2F,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_00_067[] = {
	0x31,
	0x0C,
};

static u8 SEQ_NT36672C_M33X_00_068[] = {
	0x32,
	0x09,
};

static u8 SEQ_NT36672C_M33X_00_069[] = {
	0x33,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_070[] = {
	0x34,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_071[] = {
	0x35,
	0x07,
};

static u8 SEQ_NT36672C_M33X_00_072[] = {
	0x36,
	0x3C,
};

static u8 SEQ_NT36672C_M33X_00_073[] = {
	0x4E,
	0x37,
};

static u8 SEQ_NT36672C_M33X_00_074[] = {
	0x4F,
	0x37,
};

static u8 SEQ_NT36672C_M33X_00_075[] = {
	0x53,
	0x37,
};

static u8 SEQ_NT36672C_M33X_00_076[] = {
	0x77,
	0x80,
};

static u8 SEQ_NT36672C_M33X_00_077[] = {
	0x79,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_078[] = {
	0x7A,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_079[] = {
	0x7B,
	0x8E,
};

static u8 SEQ_NT36672C_M33X_00_080[] = {
	0x7D,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_081[] = {
	0x80,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_082[] = {
	0x81,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_083[] = {
	0x82,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_084[] = {
	0x84,
	0x31,
};

static u8 SEQ_NT36672C_M33X_00_085[] = {
	0x85,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_086[] = {
	0x86,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_087[] = {
	0x87,
	0x31,
};

static u8 SEQ_NT36672C_M33X_00_088[] = {
	0x90,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_089[] = {
	0x92,
	0x31,
};

static u8 SEQ_NT36672C_M33X_00_090[] = {
	0x93,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_091[] = {
	0x94,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_092[] = {
	0x95,
	0x31,
};

static u8 SEQ_NT36672C_M33X_00_093[] = {
	0x9C,
	0xF4,
};

static u8 SEQ_NT36672C_M33X_00_094[] = {
	0x9D,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_095[] = {
	0xA0,
	0x0E,
};

static u8 SEQ_NT36672C_M33X_00_096[] = {
	0xA2,
	0x0E,
};

static u8 SEQ_NT36672C_M33X_00_097[] = {
	0xA3,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_098[] = {
	0xA4,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_099[] = {
	0xA5,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_100[] = {
	0xC4,
	0x80,
};

static u8 SEQ_NT36672C_M33X_00_101[] = {
	0xC6,
	0xC0,
};

static u8 SEQ_NT36672C_M33X_00_102[] = {
	0xC9,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_103[] = {
	0xD9,
	0x80,
};

static u8 SEQ_NT36672C_M33X_00_104[] = {
	0xE9,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_105[] = {
	0xFF,
	0x25,
};

static u8 SEQ_NT36672C_M33X_00_106[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_107[] = {
	0x0F,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_108[] = {
	0x19,
	0xE4,
};

static u8 SEQ_NT36672C_M33X_00_109[] = {
	0x21,
	0x40,
};

static u8 SEQ_NT36672C_M33X_00_110[] = {
	0x63,
	0x8F,
};

static u8 SEQ_NT36672C_M33X_00_111[] = {
	0x66,
	0x5D,
};

static u8 SEQ_NT36672C_M33X_00_112[] = {
	0x67,
	0x16,
};

static u8 SEQ_NT36672C_M33X_00_113[] = {
	0x68,
	0x58,
};

static u8 SEQ_NT36672C_M33X_00_114[] = {
	0x69,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_115[] = {
	0x6B,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_116[] = {
	0x70,
	0xE5,
};

static u8 SEQ_NT36672C_M33X_00_117[] = {
	0x71,
	0x6D,
};

static u8 SEQ_NT36672C_M33X_00_118[] = {
	0x77,
	0x62,
};

static u8 SEQ_NT36672C_M33X_00_119[] = {
	0x7E,
	0x2D,
};

static u8 SEQ_NT36672C_M33X_00_120[] = {
	0x84,
	0x78,
};

static u8 SEQ_NT36672C_M33X_00_121[] = {
	0x85,
	0x75,
};

static u8 SEQ_NT36672C_M33X_00_122[] = {
	0x8D,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_123[] = {
	0xC1,
	0xA9,
};

static u8 SEQ_NT36672C_M33X_00_124[] = {
	0xC2,
	0x5A,
};

static u8 SEQ_NT36672C_M33X_00_125[] = {
	0xC3,
	0x07,
};

static u8 SEQ_NT36672C_M33X_00_126[] = {
	0xC4,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_127[] = {
	0xC6,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_128[] = {
	0xF0,
	0x05,
};

static u8 SEQ_NT36672C_M33X_00_129[] = {
	0xEF,
	0x28,
};

static u8 SEQ_NT36672C_M33X_00_130[] = {
	0xF1,
	0x14,
};

static u8 SEQ_NT36672C_M33X_00_131[] = {
	0xFF,
	0x26,
};

static u8 SEQ_NT36672C_M33X_00_132[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_133[] = {
	0x00,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_134[] = {
	0x01,
	0xFB,
};

static u8 SEQ_NT36672C_M33X_00_135[] = {
	0x03,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_136[] = {
	0x04,
	0xFB,
};

static u8 SEQ_NT36672C_M33X_00_137[] = {
	0x05,
	0x08,
};

static u8 SEQ_NT36672C_M33X_00_138[] = {
	0x06,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_139[] = {
	0x08,
	0x13,
};

static u8 SEQ_NT36672C_M33X_00_140[] = {
	0x14,
	0x06,
};

static u8 SEQ_NT36672C_M33X_00_141[] = {
	0x15,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_142[] = {
	0x74,
	0xAF,
};

static u8 SEQ_NT36672C_M33X_00_143[] = {
	0x81,
	0x0E,
};

static u8 SEQ_NT36672C_M33X_00_144[] = {
	0x83,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_145[] = {
	0x84,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_146[] = {
	0x85,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_147[] = {
	0x86,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_148[] = {
	0x87,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_149[] = {
	0x88,
	0x07,
};

static u8 SEQ_NT36672C_M33X_00_150[] = {
	0x8A,
	0x1A,
};

static u8 SEQ_NT36672C_M33X_00_151[] = {
	0x8B,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_152[] = {
	0x8C,
	0x24,
};

static u8 SEQ_NT36672C_M33X_00_153[] = {
	0x8E,
	0x42,
};

static u8 SEQ_NT36672C_M33X_00_154[] = {
	0x8F,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_155[] = {
	0x90,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_156[] = {
	0x91,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_157[] = {
	0x9A,
	0x80,
};

static u8 SEQ_NT36672C_M33X_00_158[] = {
	0x9B,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_159[] = {
	0x9C,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_160[] = {
	0x9D,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_161[] = {
	0x9E,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_162[] = {
	0xFF,
	0x27,
};

static u8 SEQ_NT36672C_M33X_00_163[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_164[] = {
	0x01,
	0x68,
};

static u8 SEQ_NT36672C_M33X_00_165[] = {
	0x20,
	0x81,
};

static u8 SEQ_NT36672C_M33X_00_166[] = {
	0x21,
	0x6F,
};

static u8 SEQ_NT36672C_M33X_00_167[] = {
	0x25,
	0x81,
};

static u8 SEQ_NT36672C_M33X_00_168[] = {
	0x26,
	0x97,
};

static u8 SEQ_NT36672C_M33X_00_169[] = {
	0x6E,
	0x12,
};

static u8 SEQ_NT36672C_M33X_00_170[] = {
	0x6F,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_171[] = {
	0x70,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_172[] = {
	0x71,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_173[] = {
	0x72,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_174[] = {
	0x73,
	0x76,
};

static u8 SEQ_NT36672C_M33X_00_175[] = {
	0x74,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_176[] = {
	0x75,
	0x32,
};

static u8 SEQ_NT36672C_M33X_00_177[] = {
	0x76,
	0x54,
};

static u8 SEQ_NT36672C_M33X_00_178[] = {
	0x77,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_179[] = {
	0x7D,
	0x09,
};

static u8 SEQ_NT36672C_M33X_00_180[] = {
	0x7E,
	0x6B,
};

static u8 SEQ_NT36672C_M33X_00_181[] = {
	0x80,
	0x27,
};

static u8 SEQ_NT36672C_M33X_00_182[] = {
	0x82,
	0x09,
};

static u8 SEQ_NT36672C_M33X_00_183[] = {
	0x83,
	0x6B,
};

static u8 SEQ_NT36672C_M33X_00_184[] = {
	0x88,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_185[] = {
	0x89,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_186[] = {
	0xE3,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_187[] = {
	0xE4,
	0xE9,
};

static u8 SEQ_NT36672C_M33X_00_188[] = {
	0xE5,
	0x02,
};

static u8 SEQ_NT36672C_M33X_00_189[] = {
	0xE6,
	0xDE,
};

static u8 SEQ_NT36672C_M33X_00_190[] = {
	0xE9,
	0x02,
};

static u8 SEQ_NT36672C_M33X_00_191[] = {
	0xEA,
	0x1E,
};

static u8 SEQ_NT36672C_M33X_00_192[] = {
	0xEB,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_193[] = {
	0xEC,
	0x2D,
};

static u8 SEQ_NT36672C_M33X_00_194[] = {
	0xFF,
	0x2A,
};

static u8 SEQ_NT36672C_M33X_00_195[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_196[] = {
	0x00,
	0x91,
};

static u8 SEQ_NT36672C_M33X_00_197[] = {
	0x03,
	0x20,
};

static u8 SEQ_NT36672C_M33X_00_198[] = {
	0x06,
	0x06,
};

static u8 SEQ_NT36672C_M33X_00_199[] = {
	0x07,
	0x50,
};

static u8 SEQ_NT36672C_M33X_00_200[] = {
	0x0A,
	0x60,
};

static u8 SEQ_NT36672C_M33X_00_201[] = {
	0x0C,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_202[] = {
	0x0D,
	0x40,
};

static u8 SEQ_NT36672C_M33X_00_203[] = {
	0x0F,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_204[] = {
	0x11,
	0xE1,
};

static u8 SEQ_NT36672C_M33X_00_205[] = {
	0x15,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_206[] = {
	0x16,
	0xBE,
};

static u8 SEQ_NT36672C_M33X_00_207[] = {
	0x19,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_208[] = {
	0x1A,
	0x92,
};

static u8 SEQ_NT36672C_M33X_00_209[] = {
	0x1B,
	0x12,
};

static u8 SEQ_NT36672C_M33X_00_210[] = {
	0x1D,
	0x36,
};

static u8 SEQ_NT36672C_M33X_00_211[] = {
	0x1E,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_212[] = {
	0x1F,
	0x48,
};

static u8 SEQ_NT36672C_M33X_00_213[] = {
	0x20,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_214[] = {
	0x27,
	0x80,
};

static u8 SEQ_NT36672C_M33X_00_215[] = {
	0x28,
	0xFD,
};

static u8 SEQ_NT36672C_M33X_00_216[] = {
	0x29,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_00_217[] = {
	0x2A,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_218[] = {
	0x2D,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_00_219[] = {
	0x2F,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_220[] = {
	0x30,
	0x85,
};

static u8 SEQ_NT36672C_M33X_00_221[] = {
	0x31,
	0xB4,
};

static u8 SEQ_NT36672C_M33X_00_222[] = {
	0x33,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_223[] = {
	0x34,
	0xFF,
};

static u8 SEQ_NT36672C_M33X_00_224[] = {
	0x35,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_225[] = {
	0x36,
	0x05,
};

static u8 SEQ_NT36672C_M33X_00_226[] = {
	0x37,
	0xF9,
};

static u8 SEQ_NT36672C_M33X_00_227[] = {
	0x38,
	0x44,
};

static u8 SEQ_NT36672C_M33X_00_228[] = {
	0x39,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_229[] = {
	0x3A,
	0x85,
};

static u8 SEQ_NT36672C_M33X_00_230[] = {
	0x45,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_231[] = {
	0x46,
	0x40,
};

static u8 SEQ_NT36672C_M33X_00_232[] = {
	0x48,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_233[] = {
	0x4A,
	0xE1,
};

static u8 SEQ_NT36672C_M33X_00_234[] = {
	0x4E,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_235[] = {
	0x4F,
	0xBE,
};

static u8 SEQ_NT36672C_M33X_00_236[] = {
	0x52,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_237[] = {
	0x53,
	0x92,
};

static u8 SEQ_NT36672C_M33X_00_238[] = {
	0x54,
	0x12,
};

static u8 SEQ_NT36672C_M33X_00_239[] = {
	0x56,
	0x36,
};

static u8 SEQ_NT36672C_M33X_00_240[] = {
	0x57,
	0x57,
};

static u8 SEQ_NT36672C_M33X_00_241[] = {
	0x58,
	0x61,
};

static u8 SEQ_NT36672C_M33X_00_242[] = {
	0x59,
	0x57,
};

static u8 SEQ_NT36672C_M33X_00_243[] = {
	0x7A,
	0x09,
};

static u8 SEQ_NT36672C_M33X_00_244[] = {
	0x7B,
	0x40,
};

static u8 SEQ_NT36672C_M33X_00_245[] = {
	0x7F,
	0xF0,
};

static u8 SEQ_NT36672C_M33X_00_246[] = {
	0x83,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_247[] = {
	0x84,
	0xBE,
};

static u8 SEQ_NT36672C_M33X_00_248[] = {
	0x87,
	0x0F,
};

static u8 SEQ_NT36672C_M33X_00_249[] = {
	0x88,
	0x92,
};

static u8 SEQ_NT36672C_M33X_00_250[] = {
	0x89,
	0x12,
};

static u8 SEQ_NT36672C_M33X_00_251[] = {
	0x8B,
	0x36,
};

static u8 SEQ_NT36672C_M33X_00_252[] = {
	0x8C,
	0x7E,
};

static u8 SEQ_NT36672C_M33X_00_253[] = {
	0x8D,
	0x7E,
};

static u8 SEQ_NT36672C_M33X_00_254[] = {
	0x8E,
	0x7E,
};

static u8 SEQ_NT36672C_M33X_00_255[] = {
	0xFF,
	0x2C,
};

static u8 SEQ_NT36672C_M33X_00_256[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_257[] = {
	0x03,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_258[] = {
	0x04,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_259[] = {
	0x05,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_260[] = {
	0x0D,
	0x06,
};

static u8 SEQ_NT36672C_M33X_00_261[] = {
	0x0E,
	0x56,
};

static u8 SEQ_NT36672C_M33X_00_262[] = {
	0x17,
	0x4E,
};

static u8 SEQ_NT36672C_M33X_00_263[] = {
	0x18,
	0x4E,
};

static u8 SEQ_NT36672C_M33X_00_264[] = {
	0x19,
	0x4E,
};

static u8 SEQ_NT36672C_M33X_00_265[] = {
	0x2D,
	0xAF,
};

static u8 SEQ_NT36672C_M33X_00_266[] = {
	0x2F,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_267[] = {
	0x30,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_00_268[] = {
	0x32,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_269[] = {
	0x33,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_00_270[] = {
	0x35,
	0x19,
};

static u8 SEQ_NT36672C_M33X_00_271[] = {
	0x37,
	0x19,
};

static u8 SEQ_NT36672C_M33X_00_272[] = {
	0x4D,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_273[] = {
	0x4E,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_274[] = {
	0x4F,
	0x09,
};

static u8 SEQ_NT36672C_M33X_00_275[] = {
	0x56,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_276[] = {
	0x58,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_277[] = {
	0x59,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_278[] = {
	0x62,
	0x6D,
};

static u8 SEQ_NT36672C_M33X_00_279[] = {
	0x6B,
	0x6A,
};

static u8 SEQ_NT36672C_M33X_00_280[] = {
	0x6C,
	0x6A,
};

static u8 SEQ_NT36672C_M33X_00_281[] = {
	0x6D,
	0x6A,
};

static u8 SEQ_NT36672C_M33X_00_282[] = {
	0x80,
	0xAF,
};

static u8 SEQ_NT36672C_M33X_00_283[] = {
	0x81,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_284[] = {
	0x82,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_00_285[] = {
	0x84,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_286[] = {
	0x85,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_00_287[] = {
	0x87,
	0x20,
};

static u8 SEQ_NT36672C_M33X_00_288[] = {
	0x89,
	0x20,
};

static u8 SEQ_NT36672C_M33X_00_289[] = {
	0x9E,
	0x04,
};

static u8 SEQ_NT36672C_M33X_00_290[] = {
	0x9F,
	0x1E,
};

static u8 SEQ_NT36672C_M33X_00_291[] = {
	0xFF,
	0xE0,
};

static u8 SEQ_NT36672C_M33X_00_292[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_293[] = {
	0x35,
	0x82,
};

static u8 SEQ_NT36672C_M33X_00_294[] = {
	0xFF,
	0xF0,
};

static u8 SEQ_NT36672C_M33X_00_295[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_296[] = {
	0x1C,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_297[] = {
	0x33,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_298[] = {
	0x5A,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_299[] = {
	0xFF,
	0xD0,
};

static u8 SEQ_NT36672C_M33X_00_300[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_301[] = {
	0x53,
	0x22,
};

static u8 SEQ_NT36672C_M33X_00_302[] = {
	0x54,
	0x02,
};

static u8 SEQ_NT36672C_M33X_00_303[] = {
	0xFF,
	0xC0,
};

static u8 SEQ_NT36672C_M33X_00_304[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_305[] = {
	0x9C,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_306[] = {
	0x9D,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_307[] = {
	0xFF,
	0x2B,
};

static u8 SEQ_NT36672C_M33X_00_308[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_309[] = {
	0xB7,
	0x1A,
};

static u8 SEQ_NT36672C_M33X_00_310[] = {
	0xB8,
	0x15,
};

static u8 SEQ_NT36672C_M33X_00_311[] = {
	0xC0,
	0x03,
};

static u8 SEQ_NT36672C_M33X_00_312[] = {
	0xFF,
	0xF0,
};

static u8 SEQ_NT36672C_M33X_00_313[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_314[] = {
	0xD2,
	0x50,
};

static u8 SEQ_NT36672C_M33X_00_315[] = {
	0xFF,
	0x23,
};

static u8 SEQ_NT36672C_M33X_00_316[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_317[] = {
	0x00,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_318[] = {
	0x07,
	0x60,
};

static u8 SEQ_NT36672C_M33X_00_319[] = {
	0x08,
	0x06,
};

static u8 SEQ_NT36672C_M33X_00_320[] = {
	0x09,
	0x1C,
};

static u8 SEQ_NT36672C_M33X_00_321[] = {
	0x0A,
	0x3C,
};

static u8 SEQ_NT36672C_M33X_00_322[] = {
	0x0B,
	0x3C,
};

static u8 SEQ_NT36672C_M33X_00_323[] = {
	0x0C,
	0x3C,
};

static u8 SEQ_NT36672C_M33X_00_324[] = {
	0x0D,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_325[] = {
	0x10,
	0x50,
};

static u8 SEQ_NT36672C_M33X_00_326[] = {
	0x11,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_327[] = {
	0x12,
	0x95,
};

static u8 SEQ_NT36672C_M33X_00_328[] = {
	0x15,
	0x68,
};

static u8 SEQ_NT36672C_M33X_00_329[] = {
	0x16,
	0x0B,
};

static u8 SEQ_NT36672C_M33X_00_330[] = {
	0x19,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_331[] = {
	0x1A,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_332[] = {
	0x1B,
	0x1B,
};

static u8 SEQ_NT36672C_M33X_00_333[] = {
	0x1C,
	0x1C,
};

static u8 SEQ_NT36672C_M33X_00_334[] = {
	0x1D,
	0x1C,
};

static u8 SEQ_NT36672C_M33X_00_335[] = {
	0x1E,
	0x1C,
};

static u8 SEQ_NT36672C_M33X_00_336[] = {
	0x1F,
	0x1D,
};

static u8 SEQ_NT36672C_M33X_00_337[] = {
	0x20,
	0x28,
};

static u8 SEQ_NT36672C_M33X_00_338[] = {
	0x21,
	0x2A,
};

static u8 SEQ_NT36672C_M33X_00_339[] = {
	0x22,
	0x2B,
};

static u8 SEQ_NT36672C_M33X_00_340[] = {
	0x23,
	0x30,
};

static u8 SEQ_NT36672C_M33X_00_341[] = {
	0x24,
	0x33,
};

static u8 SEQ_NT36672C_M33X_00_342[] = {
	0x25,
	0x38,
};

static u8 SEQ_NT36672C_M33X_00_343[] = {
	0x26,
	0x3C,
};

static u8 SEQ_NT36672C_M33X_00_344[] = {
	0x27,
	0x3E,
};

static u8 SEQ_NT36672C_M33X_00_345[] = {
	0x28,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_346[] = {
	0x29,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_347[] = {
	0x2A,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_348[] = {
	0x2B,
	0x3F,
};

static u8 SEQ_NT36672C_M33X_00_349[] = {
	0x30,
	0xFF,
};

static u8 SEQ_NT36672C_M33X_00_350[] = {
	0x31,
	0xFE,
};

static u8 SEQ_NT36672C_M33X_00_351[] = {
	0x32,
	0xFD,
};

static u8 SEQ_NT36672C_M33X_00_352[] = {
	0x33,
	0xFC,
};

static u8 SEQ_NT36672C_M33X_00_353[] = {
	0x34,
	0xFB,
};

static u8 SEQ_NT36672C_M33X_00_354[] = {
	0x35,
	0xFA,
};

static u8 SEQ_NT36672C_M33X_00_355[] = {
	0x36,
	0xF9,
};

static u8 SEQ_NT36672C_M33X_00_356[] = {
	0x37,
	0xF7,
};

static u8 SEQ_NT36672C_M33X_00_357[] = {
	0x38,
	0xF5,
};

static u8 SEQ_NT36672C_M33X_00_358[] = {
	0x39,
	0xF3,
};

static u8 SEQ_NT36672C_M33X_00_359[] = {
	0x3A,
	0xF1,
};

static u8 SEQ_NT36672C_M33X_00_360[] = {
	0x3B,
	0xEE,
};

static u8 SEQ_NT36672C_M33X_00_361[] = {
	0x3D,
	0xEC,
};

static u8 SEQ_NT36672C_M33X_00_362[] = {
	0x3F,
	0xEA,
};

static u8 SEQ_NT36672C_M33X_00_363[] = {
	0x40,
	0xE8,
};

static u8 SEQ_NT36672C_M33X_00_364[] = {
	0x41,
	0xE6,
};

static u8 SEQ_NT36672C_M33X_00_365[] = {
	0x04,
	0x00,
};

static u8 SEQ_NT36672C_M33X_00_366[] = {
	0xA0,
	0x11,
};

static u8 SEQ_NT36672C_M33X_00_367[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36672C_M33X_00_368[] = {
	0xFB,
	0x01,
};

/*
static u8 SEQ_NT36672C_M33X_00_369[] = {
	0x53,
	0x2C,
};
*/

static u8 SEQ_NT36672C_M33X_00_370[] = {
	0x55,
	0x01,
};

static u8 SEQ_NT36672C_M33X_00_371[] = {
	0x68,
	0x00, 0x01,
};


static DEFINE_STATIC_PACKET(nt36672c_m33x_00_sleep_out, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_sleep_in, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_display_on, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_display_off, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_brightness_on, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(nt36672c_m33x_00_brightness, &nt36672c_m33x_00_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(nt36672c_m33x_00_brightness, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(nt36672c_m33x_00_001, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_001, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_002, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_002, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_003, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_003, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_004, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_004, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_005, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_005, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_006, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_006, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_007, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_007, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_008, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_008, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_009, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_009, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_010, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_010, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_011, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_011, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_012, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_012, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_013, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_013, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_014, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_014, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_015, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_015, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_016, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_016, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_017, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_017, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_018, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_018, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_019, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_019, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_020, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_020, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_021, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_021, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_022, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_022, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_023, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_023, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_024, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_024, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_025, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_025, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_026, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_026, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_027, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_027, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_028, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_028, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_029, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_029, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_030, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_030, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_031, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_031, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_032, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_032, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_033, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_033, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_034, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_034, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_035, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_035, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_036, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_036, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_037, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_037, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_038, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_038, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_039, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_039, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_040, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_040, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_041, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_041, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_042, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_042, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_043, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_043, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_044, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_044, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_045, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_045, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_046, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_046, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_047, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_047, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_048, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_048, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_049, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_049, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_050, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_050, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_051, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_051, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_052, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_052, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_053, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_053, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_054, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_054, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_055, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_055, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_056, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_056, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_057, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_057, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_058, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_058, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_059, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_059, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_060, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_060, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_061, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_061, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_062, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_062, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_063, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_063, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_064, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_064, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_065, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_065, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_066, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_066, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_067, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_067, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_068, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_068, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_069, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_069, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_070, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_070, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_071, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_071, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_072, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_072, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_073, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_073, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_074, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_074, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_075, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_075, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_076, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_076, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_077, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_077, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_078, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_078, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_079, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_079, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_080, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_080, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_081, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_081, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_082, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_082, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_083, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_083, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_084, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_084, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_085, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_085, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_086, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_086, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_087, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_087, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_088, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_088, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_089, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_089, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_090, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_090, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_091, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_091, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_092, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_092, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_093, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_093, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_094, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_094, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_095, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_095, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_096, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_096, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_097, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_097, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_098, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_098, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_099, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_099, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_100, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_100, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_101, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_101, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_102, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_102, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_103, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_103, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_104, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_104, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_105, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_105, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_106, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_106, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_107, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_107, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_108, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_108, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_109, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_109, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_110, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_110, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_111, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_111, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_112, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_112, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_113, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_113, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_114, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_114, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_115, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_115, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_116, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_116, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_117, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_117, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_118, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_118, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_119, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_119, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_120, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_120, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_121, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_121, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_122, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_122, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_123, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_123, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_124, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_124, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_125, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_125, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_126, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_126, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_127, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_127, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_128, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_128, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_129, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_129, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_130, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_130, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_131, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_131, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_132, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_132, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_133, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_133, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_134, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_134, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_135, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_135, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_136, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_136, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_137, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_137, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_138, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_138, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_139, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_139, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_140, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_140, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_141, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_141, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_142, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_142, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_143, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_143, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_144, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_144, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_145, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_145, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_146, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_146, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_147, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_147, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_148, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_148, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_149, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_149, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_150, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_150, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_151, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_151, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_152, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_152, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_153, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_153, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_154, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_154, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_155, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_155, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_156, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_156, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_157, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_157, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_158, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_158, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_159, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_159, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_160, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_160, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_161, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_161, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_162, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_162, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_163, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_163, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_164, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_164, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_165, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_165, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_166, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_166, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_167, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_167, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_168, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_168, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_169, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_169, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_170, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_170, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_171, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_171, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_172, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_172, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_173, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_173, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_174, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_174, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_175, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_175, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_176, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_176, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_177, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_177, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_178, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_178, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_179, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_179, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_180, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_180, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_181, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_181, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_182, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_182, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_183, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_183, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_184, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_184, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_185, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_185, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_186, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_186, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_187, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_187, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_188, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_188, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_189, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_189, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_190, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_190, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_191, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_191, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_192, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_192, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_193, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_193, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_194, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_194, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_195, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_195, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_196, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_196, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_197, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_197, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_198, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_198, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_199, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_199, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_200, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_200, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_201, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_201, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_202, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_202, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_203, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_203, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_204, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_204, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_205, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_205, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_206, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_206, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_207, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_207, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_208, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_208, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_209, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_209, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_210, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_210, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_211, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_211, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_212, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_212, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_213, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_213, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_214, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_214, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_215, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_215, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_216, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_216, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_217, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_217, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_218, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_218, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_219, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_219, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_220, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_220, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_221, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_221, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_222, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_222, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_223, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_223, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_224, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_224, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_225, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_225, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_226, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_226, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_227, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_227, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_228, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_228, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_229, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_229, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_230, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_230, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_231, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_231, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_232, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_232, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_233, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_233, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_234, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_234, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_235, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_235, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_236, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_236, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_237, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_237, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_238, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_238, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_239, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_239, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_240, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_240, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_241, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_241, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_242, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_242, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_243, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_243, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_244, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_244, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_245, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_245, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_246, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_246, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_247, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_247, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_248, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_248, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_249, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_249, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_250, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_250, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_251, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_251, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_252, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_252, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_253, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_253, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_254, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_254, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_255, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_255, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_256, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_256, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_257, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_257, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_258, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_258, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_259, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_259, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_260, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_260, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_261, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_261, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_262, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_262, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_263, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_263, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_264, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_264, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_265, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_265, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_266, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_266, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_267, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_267, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_268, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_268, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_269, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_269, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_270, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_270, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_271, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_271, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_272, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_272, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_273, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_273, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_274, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_274, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_275, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_275, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_276, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_276, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_277, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_277, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_278, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_278, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_279, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_279, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_280, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_280, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_281, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_281, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_282, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_282, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_283, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_283, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_284, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_284, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_285, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_285, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_286, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_286, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_287, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_287, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_288, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_288, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_289, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_289, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_290, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_290, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_291, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_291, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_292, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_292, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_293, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_293, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_294, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_294, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_295, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_295, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_296, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_296, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_297, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_297, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_298, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_298, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_299, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_299, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_300, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_300, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_301, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_301, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_302, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_302, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_303, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_303, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_304, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_304, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_305, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_305, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_306, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_306, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_307, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_307, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_308, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_308, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_309, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_309, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_310, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_310, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_311, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_311, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_312, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_312, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_313, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_313, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_314, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_314, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_315, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_315, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_316, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_316, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_317, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_317, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_318, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_318, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_319, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_319, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_320, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_320, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_321, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_321, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_322, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_322, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_323, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_323, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_324, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_324, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_325, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_325, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_326, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_326, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_327, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_327, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_328, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_328, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_329, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_329, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_330, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_330, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_331, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_331, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_332, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_332, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_333, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_333, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_334, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_334, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_335, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_335, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_336, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_336, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_337, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_337, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_338, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_338, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_339, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_339, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_340, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_340, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_341, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_341, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_342, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_342, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_343, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_343, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_344, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_344, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_345, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_345, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_346, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_346, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_347, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_347, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_348, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_348, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_349, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_349, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_350, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_350, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_351, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_351, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_352, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_352, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_353, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_353, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_354, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_354, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_355, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_355, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_356, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_356, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_357, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_357, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_358, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_358, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_359, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_359, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_360, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_360, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_361, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_361, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_362, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_362, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_363, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_363, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_364, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_364, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_365, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_365, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_366, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_366, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_367, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_367, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_368, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_368, 0);
//static DEFINE_STATIC_PACKET(nt36672c_m33x_00_369, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_369, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_370, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_370, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_371, DSI_PKT_TYPE_WR, SEQ_NT36672C_M33X_00_371, 0);


static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_60msec, 60); /* 4 frame */
static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_wait_2msec, 2);

static DEFINE_SETPROP_VALUE(m33x_00_set_wait_tx_done_property_off, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_SETPROP_VALUE(m33x_00_set_wait_tx_done_property_auto, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static void *nt36672c_m33x_00_init_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_00_001),
	&PKTINFO(nt36672c_m33x_00_002),
	&PKTINFO(nt36672c_m33x_00_003),
	&PKTINFO(nt36672c_m33x_00_004),
	&PKTINFO(nt36672c_m33x_00_005),
	&PKTINFO(nt36672c_m33x_00_006),
	&PKTINFO(nt36672c_m33x_00_007),
	&PKTINFO(nt36672c_m33x_00_008),
	&PKTINFO(nt36672c_m33x_00_009),
	&PKTINFO(nt36672c_m33x_00_010),
	&PKTINFO(nt36672c_m33x_00_011),
	&PKTINFO(nt36672c_m33x_00_012),
	&PKTINFO(nt36672c_m33x_00_013),
	&PKTINFO(nt36672c_m33x_00_014),
	&PKTINFO(nt36672c_m33x_00_015),
	&PKTINFO(nt36672c_m33x_00_016),
	&PKTINFO(nt36672c_m33x_00_017),
	&PKTINFO(nt36672c_m33x_00_018),
	&PKTINFO(nt36672c_m33x_00_019),
	&PKTINFO(nt36672c_m33x_00_020),
	&PKTINFO(nt36672c_m33x_00_021),
	&PKTINFO(nt36672c_m33x_00_022),
	&PKTINFO(nt36672c_m33x_00_023),
	&PKTINFO(nt36672c_m33x_00_024),
	&PKTINFO(nt36672c_m33x_00_025),
	&PKTINFO(nt36672c_m33x_00_026),
	&PKTINFO(nt36672c_m33x_00_027),
	&PKTINFO(nt36672c_m33x_00_028),
	&PKTINFO(nt36672c_m33x_00_029),
	&PKTINFO(nt36672c_m33x_00_030),
	&PKTINFO(nt36672c_m33x_00_031),
	&PKTINFO(nt36672c_m33x_00_032),
	&PKTINFO(nt36672c_m33x_00_033),
	&PKTINFO(nt36672c_m33x_00_034),
	&PKTINFO(nt36672c_m33x_00_035),
	&PKTINFO(nt36672c_m33x_00_036),
	&PKTINFO(nt36672c_m33x_00_037),
	&PKTINFO(nt36672c_m33x_00_038),
	&PKTINFO(nt36672c_m33x_00_039),
	&PKTINFO(nt36672c_m33x_00_040),
	&PKTINFO(nt36672c_m33x_00_041),
	&PKTINFO(nt36672c_m33x_00_042),
	&PKTINFO(nt36672c_m33x_00_043),
	&PKTINFO(nt36672c_m33x_00_044),
	&PKTINFO(nt36672c_m33x_00_045),
	&PKTINFO(nt36672c_m33x_00_046),
	&PKTINFO(nt36672c_m33x_00_047),
	&PKTINFO(nt36672c_m33x_00_048),
	&PKTINFO(nt36672c_m33x_00_049),
	&PKTINFO(nt36672c_m33x_00_050),
	&PKTINFO(nt36672c_m33x_00_051),
	&PKTINFO(nt36672c_m33x_00_052),
	&PKTINFO(nt36672c_m33x_00_053),
	&PKTINFO(nt36672c_m33x_00_054),
	&PKTINFO(nt36672c_m33x_00_055),
	&PKTINFO(nt36672c_m33x_00_056),
	&PKTINFO(nt36672c_m33x_00_057),
	&PKTINFO(nt36672c_m33x_00_058),
	&PKTINFO(nt36672c_m33x_00_059),
	&PKTINFO(nt36672c_m33x_00_060),
	&PKTINFO(nt36672c_m33x_00_061),
	&PKTINFO(nt36672c_m33x_00_062),
	&PKTINFO(nt36672c_m33x_00_063),
	&PKTINFO(nt36672c_m33x_00_064),
	&PKTINFO(nt36672c_m33x_00_065),
	&PKTINFO(nt36672c_m33x_00_066),
	&PKTINFO(nt36672c_m33x_00_067),
	&PKTINFO(nt36672c_m33x_00_068),
	&PKTINFO(nt36672c_m33x_00_069),
	&PKTINFO(nt36672c_m33x_00_070),
	&PKTINFO(nt36672c_m33x_00_071),
	&PKTINFO(nt36672c_m33x_00_072),
	&PKTINFO(nt36672c_m33x_00_073),
	&PKTINFO(nt36672c_m33x_00_074),
	&PKTINFO(nt36672c_m33x_00_075),
	&PKTINFO(nt36672c_m33x_00_076),
	&PKTINFO(nt36672c_m33x_00_077),
	&PKTINFO(nt36672c_m33x_00_078),
	&PKTINFO(nt36672c_m33x_00_079),
	&PKTINFO(nt36672c_m33x_00_080),
	&PKTINFO(nt36672c_m33x_00_081),
	&PKTINFO(nt36672c_m33x_00_082),
	&PKTINFO(nt36672c_m33x_00_083),
	&PKTINFO(nt36672c_m33x_00_084),
	&PKTINFO(nt36672c_m33x_00_085),
	&PKTINFO(nt36672c_m33x_00_086),
	&PKTINFO(nt36672c_m33x_00_087),
	&PKTINFO(nt36672c_m33x_00_088),
	&PKTINFO(nt36672c_m33x_00_089),
	&PKTINFO(nt36672c_m33x_00_090),
	&PKTINFO(nt36672c_m33x_00_091),
	&PKTINFO(nt36672c_m33x_00_092),
	&PKTINFO(nt36672c_m33x_00_093),
	&PKTINFO(nt36672c_m33x_00_094),
	&PKTINFO(nt36672c_m33x_00_095),
	&PKTINFO(nt36672c_m33x_00_096),
	&PKTINFO(nt36672c_m33x_00_097),
	&PKTINFO(nt36672c_m33x_00_098),
	&PKTINFO(nt36672c_m33x_00_099),
	&PKTINFO(nt36672c_m33x_00_100),
	&PKTINFO(nt36672c_m33x_00_101),
	&PKTINFO(nt36672c_m33x_00_102),
	&PKTINFO(nt36672c_m33x_00_103),
	&PKTINFO(nt36672c_m33x_00_104),
	&PKTINFO(nt36672c_m33x_00_105),
	&PKTINFO(nt36672c_m33x_00_106),
	&PKTINFO(nt36672c_m33x_00_107),
	&PKTINFO(nt36672c_m33x_00_108),
	&PKTINFO(nt36672c_m33x_00_109),
	&PKTINFO(nt36672c_m33x_00_110),
	&PKTINFO(nt36672c_m33x_00_111),
	&PKTINFO(nt36672c_m33x_00_112),
	&PKTINFO(nt36672c_m33x_00_113),
	&PKTINFO(nt36672c_m33x_00_114),
	&PKTINFO(nt36672c_m33x_00_115),
	&PKTINFO(nt36672c_m33x_00_116),
	&PKTINFO(nt36672c_m33x_00_117),
	&PKTINFO(nt36672c_m33x_00_118),
	&PKTINFO(nt36672c_m33x_00_119),
	&PKTINFO(nt36672c_m33x_00_120),
	&PKTINFO(nt36672c_m33x_00_121),
	&PKTINFO(nt36672c_m33x_00_122),
	&PKTINFO(nt36672c_m33x_00_123),
	&PKTINFO(nt36672c_m33x_00_124),
	&PKTINFO(nt36672c_m33x_00_125),
	&PKTINFO(nt36672c_m33x_00_126),
	&PKTINFO(nt36672c_m33x_00_127),
	&PKTINFO(nt36672c_m33x_00_128),
	&PKTINFO(nt36672c_m33x_00_129),
	&PKTINFO(nt36672c_m33x_00_130),
	&PKTINFO(nt36672c_m33x_00_131),
	&PKTINFO(nt36672c_m33x_00_132),
	&PKTINFO(nt36672c_m33x_00_133),
	&PKTINFO(nt36672c_m33x_00_134),
	&PKTINFO(nt36672c_m33x_00_135),
	&PKTINFO(nt36672c_m33x_00_136),
	&PKTINFO(nt36672c_m33x_00_137),
	&PKTINFO(nt36672c_m33x_00_138),
	&PKTINFO(nt36672c_m33x_00_139),
	&PKTINFO(nt36672c_m33x_00_140),
	&PKTINFO(nt36672c_m33x_00_141),
	&PKTINFO(nt36672c_m33x_00_142),
	&PKTINFO(nt36672c_m33x_00_143),
	&PKTINFO(nt36672c_m33x_00_144),
	&PKTINFO(nt36672c_m33x_00_145),
	&PKTINFO(nt36672c_m33x_00_146),
	&PKTINFO(nt36672c_m33x_00_147),
	&PKTINFO(nt36672c_m33x_00_148),
	&PKTINFO(nt36672c_m33x_00_149),
	&PKTINFO(nt36672c_m33x_00_150),
	&PKTINFO(nt36672c_m33x_00_151),
	&PKTINFO(nt36672c_m33x_00_152),
	&PKTINFO(nt36672c_m33x_00_153),
	&PKTINFO(nt36672c_m33x_00_154),
	&PKTINFO(nt36672c_m33x_00_155),
	&PKTINFO(nt36672c_m33x_00_156),
	&PKTINFO(nt36672c_m33x_00_157),
	&PKTINFO(nt36672c_m33x_00_158),
	&PKTINFO(nt36672c_m33x_00_159),
	&PKTINFO(nt36672c_m33x_00_160),
	&PKTINFO(nt36672c_m33x_00_161),
	&PKTINFO(nt36672c_m33x_00_162),
	&PKTINFO(nt36672c_m33x_00_163),
	&PKTINFO(nt36672c_m33x_00_164),
	&PKTINFO(nt36672c_m33x_00_165),
	&PKTINFO(nt36672c_m33x_00_166),
	&PKTINFO(nt36672c_m33x_00_167),
	&PKTINFO(nt36672c_m33x_00_168),
	&PKTINFO(nt36672c_m33x_00_169),
	&PKTINFO(nt36672c_m33x_00_170),
	&PKTINFO(nt36672c_m33x_00_171),
	&PKTINFO(nt36672c_m33x_00_172),
	&PKTINFO(nt36672c_m33x_00_173),
	&PKTINFO(nt36672c_m33x_00_174),
	&PKTINFO(nt36672c_m33x_00_175),
	&PKTINFO(nt36672c_m33x_00_176),
	&PKTINFO(nt36672c_m33x_00_177),
	&PKTINFO(nt36672c_m33x_00_178),
	&PKTINFO(nt36672c_m33x_00_179),
	&PKTINFO(nt36672c_m33x_00_180),
	&PKTINFO(nt36672c_m33x_00_181),
	&PKTINFO(nt36672c_m33x_00_182),
	&PKTINFO(nt36672c_m33x_00_183),
	&PKTINFO(nt36672c_m33x_00_184),
	&PKTINFO(nt36672c_m33x_00_185),
	&PKTINFO(nt36672c_m33x_00_186),
	&PKTINFO(nt36672c_m33x_00_187),
	&PKTINFO(nt36672c_m33x_00_188),
	&PKTINFO(nt36672c_m33x_00_189),
	&PKTINFO(nt36672c_m33x_00_190),
	&PKTINFO(nt36672c_m33x_00_191),
	&PKTINFO(nt36672c_m33x_00_192),
	&PKTINFO(nt36672c_m33x_00_193),
	&PKTINFO(nt36672c_m33x_00_194),
	&PKTINFO(nt36672c_m33x_00_195),
	&PKTINFO(nt36672c_m33x_00_196),
	&PKTINFO(nt36672c_m33x_00_197),
	&PKTINFO(nt36672c_m33x_00_198),
	&PKTINFO(nt36672c_m33x_00_199),
	&PKTINFO(nt36672c_m33x_00_200),
	&PKTINFO(nt36672c_m33x_00_201),
	&PKTINFO(nt36672c_m33x_00_202),
	&PKTINFO(nt36672c_m33x_00_203),
	&PKTINFO(nt36672c_m33x_00_204),
	&PKTINFO(nt36672c_m33x_00_205),
	&PKTINFO(nt36672c_m33x_00_206),
	&PKTINFO(nt36672c_m33x_00_207),
	&PKTINFO(nt36672c_m33x_00_208),
	&PKTINFO(nt36672c_m33x_00_209),
	&PKTINFO(nt36672c_m33x_00_210),
	&PKTINFO(nt36672c_m33x_00_211),
	&PKTINFO(nt36672c_m33x_00_212),
	&PKTINFO(nt36672c_m33x_00_213),
	&PKTINFO(nt36672c_m33x_00_214),
	&PKTINFO(nt36672c_m33x_00_215),
	&PKTINFO(nt36672c_m33x_00_216),
	&PKTINFO(nt36672c_m33x_00_217),
	&PKTINFO(nt36672c_m33x_00_218),
	&PKTINFO(nt36672c_m33x_00_219),
	&PKTINFO(nt36672c_m33x_00_220),
	&PKTINFO(nt36672c_m33x_00_221),
	&PKTINFO(nt36672c_m33x_00_222),
	&PKTINFO(nt36672c_m33x_00_223),
	&PKTINFO(nt36672c_m33x_00_224),
	&PKTINFO(nt36672c_m33x_00_225),
	&PKTINFO(nt36672c_m33x_00_226),
	&PKTINFO(nt36672c_m33x_00_227),
	&PKTINFO(nt36672c_m33x_00_228),
	&PKTINFO(nt36672c_m33x_00_229),
	&PKTINFO(nt36672c_m33x_00_230),
	&PKTINFO(nt36672c_m33x_00_231),
	&PKTINFO(nt36672c_m33x_00_232),
	&PKTINFO(nt36672c_m33x_00_233),
	&PKTINFO(nt36672c_m33x_00_234),
	&PKTINFO(nt36672c_m33x_00_235),
	&PKTINFO(nt36672c_m33x_00_236),
	&PKTINFO(nt36672c_m33x_00_237),
	&PKTINFO(nt36672c_m33x_00_238),
	&PKTINFO(nt36672c_m33x_00_239),
	&PKTINFO(nt36672c_m33x_00_240),
	&PKTINFO(nt36672c_m33x_00_241),
	&PKTINFO(nt36672c_m33x_00_242),
	&PKTINFO(nt36672c_m33x_00_243),
	&PKTINFO(nt36672c_m33x_00_244),
	&PKTINFO(nt36672c_m33x_00_245),
	&PKTINFO(nt36672c_m33x_00_246),
	&PKTINFO(nt36672c_m33x_00_247),
	&PKTINFO(nt36672c_m33x_00_248),
	&PKTINFO(nt36672c_m33x_00_249),
	&PKTINFO(nt36672c_m33x_00_250),
	&PKTINFO(nt36672c_m33x_00_251),
	&PKTINFO(nt36672c_m33x_00_252),
	&PKTINFO(nt36672c_m33x_00_253),
	&PKTINFO(nt36672c_m33x_00_254),
	&PKTINFO(nt36672c_m33x_00_255),
	&PKTINFO(nt36672c_m33x_00_256),
	&PKTINFO(nt36672c_m33x_00_257),
	&PKTINFO(nt36672c_m33x_00_258),
	&PKTINFO(nt36672c_m33x_00_259),
	&PKTINFO(nt36672c_m33x_00_260),
	&PKTINFO(nt36672c_m33x_00_261),
	&PKTINFO(nt36672c_m33x_00_262),
	&PKTINFO(nt36672c_m33x_00_263),
	&PKTINFO(nt36672c_m33x_00_264),
	&PKTINFO(nt36672c_m33x_00_265),
	&PKTINFO(nt36672c_m33x_00_266),
	&PKTINFO(nt36672c_m33x_00_267),
	&PKTINFO(nt36672c_m33x_00_268),
	&PKTINFO(nt36672c_m33x_00_269),
	&PKTINFO(nt36672c_m33x_00_270),
	&PKTINFO(nt36672c_m33x_00_271),
	&PKTINFO(nt36672c_m33x_00_272),
	&PKTINFO(nt36672c_m33x_00_273),
	&PKTINFO(nt36672c_m33x_00_274),
	&PKTINFO(nt36672c_m33x_00_275),
	&PKTINFO(nt36672c_m33x_00_276),
	&PKTINFO(nt36672c_m33x_00_277),
	&PKTINFO(nt36672c_m33x_00_278),
	&PKTINFO(nt36672c_m33x_00_279),
	&PKTINFO(nt36672c_m33x_00_280),
	&PKTINFO(nt36672c_m33x_00_281),
	&PKTINFO(nt36672c_m33x_00_282),
	&PKTINFO(nt36672c_m33x_00_283),
	&PKTINFO(nt36672c_m33x_00_284),
	&PKTINFO(nt36672c_m33x_00_285),
	&PKTINFO(nt36672c_m33x_00_286),
	&PKTINFO(nt36672c_m33x_00_287),
	&PKTINFO(nt36672c_m33x_00_288),
	&PKTINFO(nt36672c_m33x_00_289),
	&PKTINFO(nt36672c_m33x_00_290),
	&PKTINFO(nt36672c_m33x_00_291),
	&PKTINFO(nt36672c_m33x_00_292),
	&PKTINFO(nt36672c_m33x_00_293),
	&PKTINFO(nt36672c_m33x_00_294),
	&PKTINFO(nt36672c_m33x_00_295),
	&PKTINFO(nt36672c_m33x_00_296),
	&PKTINFO(nt36672c_m33x_00_297),
	&PKTINFO(nt36672c_m33x_00_298),
	&PKTINFO(nt36672c_m33x_00_299),
	&PKTINFO(nt36672c_m33x_00_300),
	&PKTINFO(nt36672c_m33x_00_301),
	&PKTINFO(nt36672c_m33x_00_302),
	&PKTINFO(nt36672c_m33x_00_303),
	&PKTINFO(nt36672c_m33x_00_304),
	&PKTINFO(nt36672c_m33x_00_305),
	&PKTINFO(nt36672c_m33x_00_306),
	&PKTINFO(nt36672c_m33x_00_307),
	&PKTINFO(nt36672c_m33x_00_308),
	&PKTINFO(nt36672c_m33x_00_309),
	&PKTINFO(nt36672c_m33x_00_310),
	&PKTINFO(nt36672c_m33x_00_311),
	&PKTINFO(nt36672c_m33x_00_312),
	&PKTINFO(nt36672c_m33x_00_313),
	&PKTINFO(nt36672c_m33x_00_314),
	&PKTINFO(nt36672c_m33x_00_315),
	&PKTINFO(nt36672c_m33x_00_316),
	&PKTINFO(nt36672c_m33x_00_317),
	&PKTINFO(nt36672c_m33x_00_318),
	&PKTINFO(nt36672c_m33x_00_319),
	&PKTINFO(nt36672c_m33x_00_320),
	&PKTINFO(nt36672c_m33x_00_321),
	&PKTINFO(nt36672c_m33x_00_322),
	&PKTINFO(nt36672c_m33x_00_323),
	&PKTINFO(nt36672c_m33x_00_324),
	&PKTINFO(nt36672c_m33x_00_325),
	&PKTINFO(nt36672c_m33x_00_326),
	&PKTINFO(nt36672c_m33x_00_327),
	&PKTINFO(nt36672c_m33x_00_328),
	&PKTINFO(nt36672c_m33x_00_329),
	&PKTINFO(nt36672c_m33x_00_330),
	&PKTINFO(nt36672c_m33x_00_331),
	&PKTINFO(nt36672c_m33x_00_332),
	&PKTINFO(nt36672c_m33x_00_333),
	&PKTINFO(nt36672c_m33x_00_334),
	&PKTINFO(nt36672c_m33x_00_335),
	&PKTINFO(nt36672c_m33x_00_336),
	&PKTINFO(nt36672c_m33x_00_337),
	&PKTINFO(nt36672c_m33x_00_338),
	&PKTINFO(nt36672c_m33x_00_339),
	&PKTINFO(nt36672c_m33x_00_340),
	&PKTINFO(nt36672c_m33x_00_341),
	&PKTINFO(nt36672c_m33x_00_342),
	&PKTINFO(nt36672c_m33x_00_343),
	&PKTINFO(nt36672c_m33x_00_344),
	&PKTINFO(nt36672c_m33x_00_345),
	&PKTINFO(nt36672c_m33x_00_346),
	&PKTINFO(nt36672c_m33x_00_347),
	&PKTINFO(nt36672c_m33x_00_348),
	&PKTINFO(nt36672c_m33x_00_349),
	&PKTINFO(nt36672c_m33x_00_350),
	&PKTINFO(nt36672c_m33x_00_351),
	&PKTINFO(nt36672c_m33x_00_352),
	&PKTINFO(nt36672c_m33x_00_353),
	&PKTINFO(nt36672c_m33x_00_354),
	&PKTINFO(nt36672c_m33x_00_355),
	&PKTINFO(nt36672c_m33x_00_356),
	&PKTINFO(nt36672c_m33x_00_357),
	&PKTINFO(nt36672c_m33x_00_358),
	&PKTINFO(nt36672c_m33x_00_359),
	&PKTINFO(nt36672c_m33x_00_360),
	&PKTINFO(nt36672c_m33x_00_361),
	&PKTINFO(nt36672c_m33x_00_362),
	&PKTINFO(nt36672c_m33x_00_363),
	&PKTINFO(nt36672c_m33x_00_364),
	&PKTINFO(nt36672c_m33x_00_365),
	&PKTINFO(nt36672c_m33x_00_366),
	&PKTINFO(nt36672c_m33x_00_367),
	&PKTINFO(nt36672c_m33x_00_368),
//	&PKTINFO(nt36672c_m33x_00_369),
	&PKTINFO(nt36672c_m33x_00_370),
	&PKTINFO(nt36672c_m33x_00_371),

	&PKTINFO(nt36672c_m33x_00_sleep_out),
	&DLYINFO(nt36672c_m33x_00_wait_100msec),
	&PKTINFO(nt36672c_m33x_00_display_on),
};

static void *nt36672c_m33x_00_res_init_cmdtbl[] = {
	&tft_common_restbl[RES_ID],
};

static void *nt36672c_m33x_00_set_bl_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_00_brightness),
};

static void *nt36672c_m33x_00_display_on_cmdtbl[] = {
	&DLYINFO(nt36672c_m33x_00_wait_40msec),
	&PKTINFO(nt36672c_m33x_00_brightness),
	&PKTINFO(nt36672c_m33x_00_brightness_on),
};

static void *nt36672c_m33x_00_display_off_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_00_display_off),
	&DLYINFO(nt36672c_m33x_00_wait_20msec),
};

static void *nt36672c_m33x_00_exit_cmdtbl[] = {
	&PKTINFO(nt36672c_m33x_00_sleep_in),
};

static void *nt36672c_m33x_00_display_mode_cmdtbl[] = {
	&SETPROP(m33x_00_set_wait_tx_done_property_off),
		&PKTINFO(nt36672c_m33x_00_brightness),
		/* Will flush on next VFP */
	&SETPROP(m33x_00_set_wait_tx_done_property_auto),
};

static void *nt36672c_m33x_00_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo nt36672c_m33x_00_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", nt36672c_m33x_00_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", nt36672c_m33x_00_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", nt36672c_m33x_00_set_bl_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("set-bl-seq", nt36672c_m33x_00_display_mode_cmdtbl), /* Dummy */
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", nt36672c_m33x_00_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", nt36672c_m33x_00_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", nt36672c_m33x_00_exit_cmdtbl),
};


/* BLIC SETTING START */
static u8 NT36672C_M33X_00_KTZ8864_I2C_INIT[] = {
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

static u8 NT36672C_M33X_00_KTZ8864_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 NT36672C_M33X_00_KTZ8864_I2C_DUMP[] = {
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

static DEFINE_STATIC_PACKET(nt36672c_m33x_00_ktz8864_i2c_init, I2C_PKT_TYPE_WR, NT36672C_M33X_00_KTZ8864_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_ktz8864_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36672C_M33X_00_KTZ8864_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36672c_m33x_00_ktz8864_i2c_dump, I2C_PKT_TYPE_RD, NT36672C_M33X_00_KTZ8864_I2C_DUMP, 0);

static DEFINE_PANEL_MDELAY(nt36672c_m33x_00_blic_wait_delay, 2);

static void *nt36672c_m33x_00_ktz8864_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_00_ktz8864_i2c_dump),
#endif
	&PKTINFO(nt36672c_m33x_00_ktz8864_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_00_ktz8864_i2c_dump),
#endif
};

static void *nt36672c_m33x_00_ktz8864_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36672c_m33x_00_ktz8864_i2c_dump),
#endif
	&PKTINFO(nt36672c_m33x_00_ktz8864_i2c_exit_blen),
};

static struct seqinfo nt36672c_m33x_00_ktz8864_seq_tbl[MAX_PANEL_BLIC_SEQ] = {
	[PANEL_BLIC_I2C_ON_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36672c_m33x_00_ktz8864_init_cmdtbl),
	[PANEL_BLIC_I2C_OFF_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36672c_m33x_00_ktz8864_exit_cmdtbl),
};

static struct blic_data nt36672c_m33x_00_ktz8864_blic_data = {
	.name = "ktz8864",
	.seqtbl = nt36672c_m33x_00_ktz8864_seq_tbl,
};

static struct blic_data *nt36672c_m33x_00_blic_tbl[] = {
	&nt36672c_m33x_00_ktz8864_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info nt36672c_m33x_00_panel_info = {
	.ldi_name = "nt36672c",
	.name = "nt36672c_m33x_00",
	.model = "CSOT_6_58_inch",
	.vendor = "CSO",
	.id = 0x4BF240,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &nt36672c_m33x_00_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36672c_m33x_00_default_resol),
		.resol = nt36672c_m33x_00_default_resol,
	},
	.vrrtbl = nt36672c_m33x_00_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(nt36672c_m33x_00_default_vrrtbl),
	.maptbl = nt36672c_m33x_00_maptbl,
	.nr_maptbl = ARRAY_SIZE(nt36672c_m33x_00_maptbl),
	.seqtbl = nt36672c_m33x_00_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(nt36672c_m33x_00_seqtbl),
	.rditbl = tft_common_rditbl,
	.nr_rditbl = ARRAY_SIZE(tft_common_rditbl),
	.restbl = tft_common_restbl,
	.nr_restbl = ARRAY_SIZE(tft_common_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &nt36672c_m33x_00_panel_dimming_info,
	},
	.blic_data_tbl = nt36672c_m33x_00_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(nt36672c_m33x_00_blic_tbl),
};

#endif /* __NT36672C_M33X_00_PANEL_H__ */
