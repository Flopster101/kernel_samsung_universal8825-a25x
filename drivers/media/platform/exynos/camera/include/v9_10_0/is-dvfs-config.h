/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo DVFS v2 functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DVFS_CONFIG_H
#define IS_DVFS_CONFIG_H

/* for backword compatibility with DVFS V1.0 */
#define IS_SN_MAX IS_DVFS_SN_MAX
#define IS_SN_END IS_DVFS_SN_END

/* Pablo DVFS SCENARIO enum */
enum IS_DVFS_SN {
	IS_DVFS_SN_DEFAULT,
	/* wide sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60,
	IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE,
	IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR,
	IS_DVFS_SN_REAR_SINGLE_WIDE_SSM,
	/* tele sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30,
	IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K_THERMAL,
	IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE,
	/* ultra wide sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30,
	IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM,
	/* macro sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30,
	/* ultra tele sensor scenarios */
	IS_DVFS_SN_REAR_SINGLE_ULTRATELE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_ULTRATELE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_ULTRATELE_VIDEO_FHD30,
	/* wide and tele dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30,
	/* wide and ultra wide dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30,
	/* wide and macro dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30,
	/* tele and ultra wide dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_VIDEO_FHD30,
	/* tele and ultra tele dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_VIDEO_FHD30,
	/* ultra wide and ultra tele dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_VIDEO_FHD30,
	/* wide and ultra tele dual sensor scenarios */
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_PHOTO,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_CAPTURE,
	IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_VIDEO_FHD30,
	/* front sensor scenarios */
	IS_DVFS_SN_FRONT_SINGLE_PHOTO,
	IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
	IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60,
	IS_DVFS_SN_FRONT_SINGLE_FASTAE,
	IS_DVFS_SN_FRONT_SINGLE_SECURE,
	IS_DVFS_SN_FRONT_SINGLE_VT,
	/* pip scenarios */
	IS_DVFS_SN_PIP_DUAL_PHOTO,
	IS_DVFS_SN_PIP_DUAL_CAPTURE,
	IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30,
	/* triple scenarios */
	IS_DVFS_SN_TRIPLE_PHOTO,
	IS_DVFS_SN_TRIPLE_VIDEO_FHD30,
	IS_DVFS_SN_TRIPLE_VIDEO_UHD30,
	IS_DVFS_SN_TRIPLE_VIDEO_FHD60,
	IS_DVFS_SN_TRIPLE_VIDEO_UHD60,
	IS_DVFS_SN_TRIPLE_CAPTURE,
	/* sensor only scenarios */
	IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE,
	IS_DVFS_SN_SENSOR_ONLY_REAR_DUAL,
	IS_DVFS_SN_SENSOR_ONLY_FRONT,
	IS_DVFS_SN_SENSOR_ONLY_FRONT_SECURE,
	IS_DVFS_SN_MAX,
	IS_DVFS_SN_END,
};

/* Tick count to get some time margin for DVFS scenario transition while streaming. */
#define KEEP_FRAME_TICK_DEFAULT (5)
#define IS_DVFS_CAPTURE_TICK (KEEP_FRAME_TICK_DEFAULT + 3)
#define IS_DVFS_DUAL_CAPTURE_TICK (2 * IS_DVFS_CAPTURE_TICK)

struct is_dvfs_dt_t {
	const char *parse_scenario_nm;	/* string for parsing from DTS */
	u32 scenario_id;	/* scenario_id */
	int keep_frame_tick;	/* keep qos lock during specific frames when dynamic scenario */
};

/* for DT parsing */
static struct is_dvfs_dt_t is_dvfs_dt_arr[IS_DVFS_SN_END] = {
	{
		.parse_scenario_nm	= "default_",
		.scenario_id		= IS_DVFS_SN_DEFAULT,
		.keep_frame_tick	= -1,
	},
	/* wide sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_wide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd120_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd240_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_fhd480_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_remosaic_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_remosaic_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_wide_fastae_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_videohdr_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_wide_ssm_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_WIDE_SSM,
		.keep_frame_tick	= -1,
	},
	/* tele sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_tele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k24_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_video_8k_thermal_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K_THERMAL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_remosaic_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_tele_remosaic_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	},
	/* ultra wide sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_ultrawide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_fhd480_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultrawide_ssm_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM,
		.keep_frame_tick	= -1,
	},
	/* macro sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_macro_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_macro_photo_full_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_macro_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_macro_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* ultra tele sensor scenarios */
	{
		.parse_scenario_nm	= "rear_single_ultratele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRATELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_single_ultratele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRATELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_single_ultratele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_SINGLE_ULTRATELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* wide and tele dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_tele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_tele_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	},
	/* wide and ultra wide dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultrawide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* wide and macro dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_macro_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_macro_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_macro_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* tele and ultra wide dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_tele_ultrawide_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_tele_ultrawide_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_tele_ultrawide_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* tele and ultra tele dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_tele_ultratele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_tele_ultratele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_tele_ultratele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* ultra wide and ultra tele dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_ultrawide_ultratele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_ultrawide_ultratele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_ultrawide_ultratele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* wide and ultra tele dual sensor scenarios */
	{
		.parse_scenario_nm	= "rear_dual_wide_ultratele_photo_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultratele_capture_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_DUAL_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "rear_dual_wide_ultratele_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* front sensor scenarios */
	{
		.parse_scenario_nm	= "front_single_photo_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_photo_full_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_capture_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_fhd120_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_fastae_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_FASTAE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_secure_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_SECURE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "front_single_vt_",
		.scenario_id		= IS_DVFS_SN_FRONT_SINGLE_VT,
		.keep_frame_tick	= -1,
	},
	/* pip scenarios */
	{
		.parse_scenario_nm	= "pip_dual_photo_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "pip_dual_capture_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	}, {
		.parse_scenario_nm	= "pip_dual_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	},
	/* triple scenarios */
	{
		.parse_scenario_nm	= "triple_photo_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_PHOTO,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_fhd30_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_FHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_uhd30_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_UHD30,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_fhd60_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_FHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_video_uhd60_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_VIDEO_UHD60,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "triple_capture_",
		.scenario_id		= IS_DVFS_SN_TRIPLE_CAPTURE,
		.keep_frame_tick	= IS_DVFS_CAPTURE_TICK,
	},
	/* snesor only scenarios */
	{
		.parse_scenario_nm	= "ext_rear_single_",
		.scenario_id		= IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_rear_dual_",
		.scenario_id		= IS_DVFS_SN_SENSOR_ONLY_REAR_DUAL,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_front_",
		.scenario_id		= IS_DVFS_SN_SENSOR_ONLY_FRONT,
		.keep_frame_tick	= -1,
	}, {
		.parse_scenario_nm	= "ext_front_secure_",
		.scenario_id		= IS_DVFS_SN_SENSOR_ONLY_FRONT_SECURE,
		.keep_frame_tick	= -1,
	},
	/* max scenario */
	{
		.parse_scenario_nm	= "max_",
		.scenario_id		= IS_DVFS_SN_MAX,
		.keep_frame_tick	= -1,
	},
};

#endif /* IS_DVFS_CONFIG_H */
