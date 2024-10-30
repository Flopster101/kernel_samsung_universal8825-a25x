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

#ifndef IS_GROUP_MGR_CONFIG_H
#define IS_GROUP_MGR_CONFIG_H

/* #define DEBUG_AA */
/* #define DEBUG_FLASH */

#define GROUP_STREAM_INVALID	0xFFFFFFFF

#define GROUP_ID_3AA0           0
#define GROUP_ID_3AA1           1
#define GROUP_ID_3AA2           2
#define GROUP_ID_3AA3           3
#define GROUP_ID_BYRP           4
#define GROUP_ID_RGBP           5
#define GROUP_ID_MCS0           6
#define GROUP_ID_MCS1           7
#define GROUP_ID_VRA0           8
#define GROUP_ID_PAF0           9
#define GROUP_ID_PAF1           10
#define GROUP_ID_PAF2           11
#define GROUP_ID_PAF3           12
#define GROUP_ID_MCFP           13
#define GROUP_ID_YUVP           14
#define GROUP_ID_LME0           15
#define GROUP_ID_LME1           16
#define GROUP_ID_SS0            17
#define GROUP_ID_SS1            18
#define GROUP_ID_SS2            19
#define GROUP_ID_SS3            20
#define GROUP_ID_SS4            21
#define GROUP_ID_SS5            22
#define GROUP_ID_SS6            23
#define GROUP_ID_SS7            24
#define GROUP_ID_SS8            25
/* not used, just for compatibility with previous versions */
#define GROUP_ID_ISP0           26
#define GROUP_ID_ISP1           27
#define GROUP_ID_CLH0           28
#define GROUP_ID_YPP            29
#define GROUP_ID_ORB0           30
#define GROUP_ID_ORB1           31
#define GROUP_ID_MAX            32
#define GROUP_ID_PARM_MASK      ((1UL << (GROUP_ID_SS0)) - 1)
#define GROUP_ID(id)            (1UL << (id))

#define GROUP_SLOT_SENSOR       0
#define GROUP_SLOT_PAF          1
#define GROUP_SLOT_3AA          2
#define GROUP_SLOT_LME          3
#define GROUP_SLOT_BYRP         4
#define GROUP_SLOT_RGBP         5
#define GROUP_SLOT_MCFP         6
#define GROUP_SLOT_YUVP         7
#define GROUP_SLOT_MCS          8
#define GROUP_SLOT_MAX          9
/* not used, just for compatibility with previous versions */
#define GROUP_SLOT_ISP          (GROUP_SLOT_MAX + 1)
#define GROUP_SLOT_YPP          (GROUP_SLOT_MAX + 2)
#define GROUP_SLOT_CLH          (GROUP_SLOT_MAX + 3)
#define GROUP_SLOT_VRA          (GROUP_SLOT_MAX + 4)
#define GROUP_SLOT_ORB          (GROUP_SLOT_MAX + 5)

static const char * const group_id_name[GROUP_ID_MAX + 1] = {
	[GROUP_ID_3AA0] = "G:CSTAT0",
	[GROUP_ID_3AA1] = "G:CSTAT1",
	[GROUP_ID_3AA2] = "G:CSTAT2",
	[GROUP_ID_3AA3] = "G:CSTAT3",
	[GROUP_ID_BYRP] = "G:BYRP",
	[GROUP_ID_RGBP] = "G:RGBP",
	[GROUP_ID_MCS0] = "G:MCS",
	[GROUP_ID_MCS1] = "G:ERR7",
	[GROUP_ID_VRA0] = "G:ERR8",
	[GROUP_ID_PAF0] = "G:PDP0",
	[GROUP_ID_PAF1] = "G:PDP1",
	[GROUP_ID_PAF2] = "G:PDP2",
	[GROUP_ID_PAF3] = "G:PDP3",
	[GROUP_ID_MCFP] = "G:MCFP",
	[GROUP_ID_YUVP] = "G:YUVP",
	[GROUP_ID_LME0] = "G:LME",
	[GROUP_ID_LME1] = "G:ERR16",
	[GROUP_ID_SS0 ] = "G:SS0",
	[GROUP_ID_SS1 ] = "G:SS1",
	[GROUP_ID_SS2 ] = "G:SS2",
	[GROUP_ID_SS3 ] = "G:SS3",
	[GROUP_ID_SS4 ] = "G:SS4",
	[GROUP_ID_SS5 ] = "G:SS5",
	[GROUP_ID_SS6 ] = "G:SS6",
	[GROUP_ID_SS6 ] = "G:SS7",
	[GROUP_ID_SS8 ] = "G:SS8",
	[GROUP_ID_MAX ] = "G:MAX"
};

/*
 * <LINE_FOR_SHOT_VALID_TIME>
 * If valid time is too short when image height is small, use this feature.
 * If height is smaller than this value, async_shot is increased.
 */
#define LINE_FOR_SHOT_VALID_TIME	500

#define IS_MAX_GFRAME	(VIDEO_MAX_FRAME) /* max shot buffer of F/W : 32 */
#define MIN_OF_ASYNC_SHOTS	1
#define MIN_OF_SYNC_SHOTS	2

#define MIN_OF_SHOT_RSC		(1)
#define MIN_OF_ASYNC_SHOTS_240FPS	(MIN_OF_ASYNC_SHOTS + 0)

#ifdef ENABLE_SYNC_REPROCESSING
#define REPROCESSING_TICK_COUNT	(7) /* about 200ms */
#endif
#endif

