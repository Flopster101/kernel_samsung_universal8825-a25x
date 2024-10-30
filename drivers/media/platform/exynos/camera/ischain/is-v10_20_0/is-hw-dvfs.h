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

#ifndef IS_HW_DVFS_H
#define IS_HW_DVFS_H

#include "is-device-ischain.h"
#include "is-dvfs-config.h"

/* for clock calculation */
#define IS_DVFS_OTF_PPC		4
#define IS_DVFS_M2M_PPC		2
#define IS_DVFS_POTF_BIT	64
#define IS_DVFS_CAM_MIN_LV	5
#define IS_DVFS_ISP_MIN_LV	6

#define MIN_REMOSAIC_SIZE	(8000 * 6000)

/* for porting DVFS V2.0 */
/* sensor position */
#define IS_DVFS_SENSOR_POSITION_WIDE		SP_REAR
#define IS_DVFS_SENSOR_POSITION_FRONT		SP_FRONT
#define IS_DVFS_SENSOR_POSITION_TELE		SP_REAR2
#define IS_DVFS_SENSOR_POSITION_ULTRAWIDE	SP_REAR3
#define IS_DVFS_SENSOR_POSITION_ULTRATELE	SP_REAR4
#define IS_DVFS_SENSOR_POSITION_MACRO -1 /* 6 */
#define IS_DVFS_SENSOR_POSITION_TOF -1 /* 8 */
/* specitial feature */
#define IS_DVFS_FEATURE_WIDE_DUALFPS 0
#define IS_DVFS_FEATURE_WIDE_VIDEOHDR 0

/* for resolution calculation */
#define IS_DVFS_RESOL_HD_TH (2*1280*720)
#define IS_DVFS_RESOL_FHD_TH (3*1920*1080)
#define IS_DVFS_RESOL_UHD_TH (3*3840*2160)
#define IS_DVFS_RESOL_8K_TH (3*7680*4320)

#define DVFS_PARAM_SAVE	0
#define DVFS_PARAM_RESTORE	1

/* enum type for each hierarchical level of DVFS scenario */
enum IS_DVFS_FACE {
	IS_DVFS_FACE_REAR,
	IS_DVFS_FACE_FRONT,
	IS_DVFS_FACE_PIP,
	IS_DVFS_FACE_END,
};

enum IS_DVFS_NUM {
	IS_DVFS_NUM_SINGLE,
	IS_DVFS_NUM_DUAL,
	IS_DVFS_NUM_TRIPLE,
	IS_DVFS_NUM_END,
};

enum IS_DVFS_SENSOR {
	IS_DVFS_SENSOR_NORMAL,
	IS_DVFS_SENSOR_FASTAE,
	IS_DVFS_SENSOR_REMOSAIC,
	IS_DVFS_SENSOR_VIDEOHDR,
	IS_DVFS_SENSOR_SSM,
	IS_DVFS_SENSOR_VT,
	IS_DVFS_SENSOR_SECURE,
	IS_DVFS_SENSOR_END,
};

enum IS_DVFS_MODE {
	IS_DVFS_MODE_PHOTO,
	IS_DVFS_MODE_CAPTURE,
	IS_DVFS_MODE_VIDEO,
	IS_DVFS_MODE_SENSOR_ONLY,
	IS_DVFS_MODE_END,
};

enum IS_DVFS_RESOL {
	IS_DVFS_RESOL_HD,
	IS_DVFS_RESOL_FHD,
	IS_DVFS_RESOL_UHD,
	IS_DVFS_RESOL_8K,
	IS_DVFS_RESOL_FULL,
	IS_DVFS_RESOL_END,
};

enum IS_DVFS_FPS {
	IS_DVFS_FPS_24,
	IS_DVFS_FPS_30,
	IS_DVFS_FPS_60,
	IS_DVFS_FPS_120,
	IS_DVFS_FPS_240,
	IS_DVFS_FPS_480,
	IS_DVFS_FPS_END,
};

struct is_dvfs_scenario_param {
	int rear_mask;
	int front_mask;
	int rear_face;
	int front_face;
	int wide_mask;
	int tele_mask;
	int ultrawide_mask;
	int macro_mask;
	int ultratele_mask;
	int front_sensor_mask;
	unsigned long sensor_active_map;
	u32 sensor_mode;
	int sensor_fps;
	int setfile;
	int scen;
	int dvfs_scenario;
	int secure;
	int face;
	int num;
	int sensor;
	int mode;
	int resol;
	int fps;
	bool sensor_only;
};

/* for assign staic / dynamic scenario check logic data */
int is_hw_dvfs_get_scenario(struct is_device_ischain *device, int flag_capture);
int is_hw_dvfs_get_scenario_param(struct is_device_ischain *device, int flag_capture, struct is_dvfs_scenario_param *param);
bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *bundle_num,
		u32 *bundle_domain, u32 *bundle_scn, bool *operating);
u32 is_hw_dvfs_get_lv(struct is_device_ischain *device, u32 type);


#endif /* IS_HW_DVFS_H */
