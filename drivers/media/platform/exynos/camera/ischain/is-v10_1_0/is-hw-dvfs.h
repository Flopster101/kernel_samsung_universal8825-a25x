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
#define IS_DVFS_OTF_PPC		8
#define IS_DVFS_POTF_BIT	128

/* for porting DVFS V2.0 */
/* sensor position */
#define IS_DVFS_SENSOR_POSITION_WIDE 0
#define IS_DVFS_SENSOR_POSITION_FRONT 1
#define IS_DVFS_SENSOR_POSITION_TELE 2
#define IS_DVFS_SENSOR_POSITION_ULTRAWIDE 4
#define IS_DVFS_SENSOR_POSITION_TELE2 6
#define IS_DVFS_SENSOR_POSITION_MACRO -1 /* 6 */
#define IS_DVFS_SENSOR_POSITION_TOF -1 /* 8 */
/* specitial feature */
#define IS_DVFS_FEATURE_WIDE_DUALFPS 0
#define IS_DVFS_FEATURE_WIDE_VIDEOHDR 0

/* for resolution calculation */
#define IS_DVFS_RESOL_FHD_TH (3*1920*1080)
#define IS_DVFS_RESOL_UHD_TH (3*3840*2160)
#define IS_DVFS_RESOL_8K_TH (3*7680*4320)

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
	IS_DVFS_SENSOR_WIDE,
	IS_DVFS_SENSOR_WIDE_FASTAE,
	IS_DVFS_SENSOR_WIDE_REMOSAIC,
	IS_DVFS_SENSOR_WIDE_VIDEOHDR,
	IS_DVFS_SENSOR_WIDE_SSM,
	IS_DVFS_SENSOR_WIDE_VT,
	IS_DVFS_SENSOR_TELE,
	IS_DVFS_SENSOR_TELE_REMOSAIC,
	IS_DVFS_SENSOR_ULTRAWIDE,
	IS_DVFS_SENSOR_ULTRAWIDE_SSM,
	IS_DVFS_SENSOR_MACRO,
	IS_DVFS_SENSOR_WIDE_TELE,
	IS_DVFS_SENSOR_WIDE_ULTRAWIDE,
	IS_DVFS_SENSOR_WIDE_MACRO,
	IS_DVFS_SENSOR_FRONT,
	IS_DVFS_SENSOR_FRONT_FASTAE,
	IS_DVFS_SENSOR_FRONT_SECURE,
	IS_DVFS_SENSOR_FRONT_VT,
	IS_DVFS_SENSOR_PIP,
	IS_DVFS_SENSOR_TRIPLE,
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
	int tele2_mask;
	int ultrawide_mask;
	int macro_mask;
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
	int hf; /* HF IP on/off */
	bool sensor_only;
};

/* for assign staic / dynamic scenario check logic data */
int is_hw_dvfs_get_face(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_num(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_sensor(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_mode(struct is_device_ischain *device, int flag_capture, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_resol(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_fps(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_scenario(struct is_device_ischain *device, int flag_capture);
int is_hw_dvfs_get_rear_single_wide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_wide_remosaic_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_tele_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_tele_remosaic_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_ultrawide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_single_macro_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_tele_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_rear_dual_wide_macro_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_front_single_front_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_pip_dual_wide_front_scenario(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
unsigned int is_hw_dvfs_get_bit_count(unsigned long bits);
void is_hw_dvfs_init_face_mask(struct is_device_ischain *device, struct is_dvfs_scenario_param *param);
int is_hw_dvfs_get_scenario_param(struct is_device_ischain *device, int flag_capture, struct is_dvfs_scenario_param *param);
bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *bundle_num,
		u32 *bundle_domain, u32 *bundle_scn, bool *operating);
u32 is_hw_dvfs_get_lv(struct is_device_ischain *device, u32 type);


#endif /* IS_HW_DVFS_H */
