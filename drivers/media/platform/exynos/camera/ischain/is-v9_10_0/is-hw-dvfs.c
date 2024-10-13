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

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"

static const char *str_face[IS_DVFS_FACE_END + 1] = {
	"REAR",
	"FRONT",
	"PIP",
	"INVALID",
};

static const char *str_num[IS_DVFS_NUM_END + 1] = {
	"SINGLE",
	"DUAL",
	"TRIPLE",
	"INVALID",
};

static const char *str_sensor[IS_DVFS_SENSOR_END + 1] = {
	"WIDE",
	"WIDE_FASTAE",
	"WIDE_REMOSAIC",
	"WIDE_VIDEOHDR",
	"WIDE_SSM",
	"TELE",
	"TELE_REMOSAIC",
	"ULTRAWIDE",
	"ULTRAWIDE_SSM",
	"MACRO",
	"ULTRATELE"
	"WIDE_TELE",
	"WIDE_ULTRAWIDE",
	"WIDE_MACRO",
	"TELE_ULTRAWIDE",
	"TELE_ULTRATELE",
	"ULTRAWIDE_ULTRATELE",
	"WIDE_ULTRATELE",
	"FRONT",
	"FRONT_FASTAE",
	"FRONT_SECURE",
	"FRONT_VT",
	"PIP_COMMON",
	"TRIPLE_COMMON",
	"INVALID",
};

static const char *str_mode[IS_DVFS_MODE_END + 1] = {
	"PHOTO",
	"CAPTURE",
	"VIDEO",
	"SENSOR_ONLY",
	"INVALID",
};

static const char *str_resol[IS_DVFS_RESOL_END + 1] = {
	"FHD",
	"UHD",
	"8K",
	"FULL",
	"INVALID",
};

static const char *str_fps[IS_DVFS_FPS_END + 1] = {
	"24",
	"30",
	"60",
	"120",
	"240",
	"480",
	"INVALID",
};

#if defined(USE_OFFLINE_PROCESSING)
void is_hw_offline_save_restore_dvfs_param(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param, u32 path)
{
	if (path == DVFS_PARAM_RESTORE) {
		param->face = device->off_dvfs_param.face;
		param->num = device->off_dvfs_param.num;
		param->sensor = device->off_dvfs_param.sensor;
		param->resol = device->off_dvfs_param.resol;
		param->fps = device->off_dvfs_param.fps;
		param->mode = IS_DVFS_MODE_CAPTURE;
	} else {
		device->off_dvfs_param.face = param->face;
		device->off_dvfs_param.num = param->num;
		device->off_dvfs_param.sensor = param->sensor;
		device->off_dvfs_param.resol = param->resol;
		device->off_dvfs_param.fps = param->fps;
	}

	dbg_dvfs(1, "[DVFS] path(%d) device(%d, %d, %d, %d, %d) , param mode(%d) (%d, %d, %d, %d, %d)\n",
		device, path,
		device->off_dvfs_param.face, device->off_dvfs_param.num, device->off_dvfs_param.sensor,
		device->off_dvfs_param.resol, device->off_dvfs_param.fps,
		param->mode, param->face, param->num, param->sensor,
		param->resol, param->fps);
}
#endif

int is_hw_dvfs_get_scenario_param(struct is_device_ischain *device,
	int flag_capture, struct is_dvfs_scenario_param *param)
{
	struct is_core *core;
	struct is_device_sensor *sensor_device;
	struct is_resourcemgr *resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl;

	core = (struct is_core *)device->interface->core;

	sensor_device = device->sensor;
	resourcemgr = sensor_device->resourcemgr;
	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	param->sensor_active_map = 0;
	for (device_id = 0; device_id < IS_SENSOR_COUNT; device_id++) {
		ids = is_get_sensor_device(device_id);
		if (!ids || !test_bit(IS_SENSOR_START, &ids->state))
			continue;

		if (ids->position > SENSOR_POSITION_MAX) {
			minfo("[DVFS]Invalid sensor position(%d)\n", ids, ids->position);
			continue;
		}

		param->sensor_active_map |= BIT(ids->position);
		param->sensor_only = test_bit(IS_SENSOR_ONLY, &ids->state);
	}

	/* All sensor stop at close sqeunce */
	if (!param->sensor_active_map)
		return -EINVAL;

	param->rear_face = param->sensor_active_map & param->rear_mask;
	param->front_face = param->sensor_active_map & param->front_mask;
	dbg_dvfs(1, "[DVFS] %s() sensor_active_map: 0x%x, rear: 0x%x, front: 0x%x\n",
		device, __func__, param->sensor_active_map,
		param->rear_face, param->front_face);

	if (!test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state)) {
		sensor_device = device->sensor;
		sensor_device->cfg = is_sensor_g_mode(sensor_device);
		if (!sensor_device->cfg) {
			merr("sensor cfg is invalid", sensor_device);
			return -EINVAL;
		}
		param->sensor_mode = sensor_device->cfg->mode;
		param->sensor_fps = is_sensor_g_framerate(sensor_device);
	}
	param->setfile = (device->setfile & IS_SETFILE_MASK);
	param->scen = (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
	param->dvfs_scenario = dvfs_ctrl->dvfs_scenario;
	param->secure = core->scenario;
	dbg_dvfs(1, "[DVFS] %s() sensor_mode: %d, fps: %d, setfile: %d, scen: %d, dvfs_scenario: %x, secure: %d\n",
		device, __func__, param->sensor_mode, param->sensor_fps,
		param->setfile, param->scen, param->dvfs_scenario, param->secure);

	param->face = is_hw_dvfs_get_face(device, param);
	param->num = is_hw_dvfs_get_num(device, param);
	param->sensor = is_hw_dvfs_get_sensor(device, param);

	dbg_dvfs(1, "[DVFS] %s() param_face(%d), num(%d), sensor(%d)\n",
		device, __func__, param->face, param->num, param->sensor);

	if (param->face > IS_DVFS_FACE_END || param->face < 0)
		param->face = IS_DVFS_FACE_END;
	if (param->num > IS_DVFS_NUM_END || param->num < 0)
		param->num = IS_DVFS_NUM_END;
	if (param->sensor > IS_DVFS_SENSOR_END || param->sensor < 0)
		param->sensor = IS_DVFS_SENSOR_END;

	param->mode = is_hw_dvfs_get_mode(device, flag_capture, param);
	param->resol = is_hw_dvfs_get_resol(device, param);
	param->fps = is_hw_dvfs_get_fps(device, param);

	dbg_dvfs(1, "[DVFS] %s() param_mode(%d), resol(%d), fps(%d)\n",
		device, __func__, param->mode, param->resol, param->fps);

	if (param->mode > IS_DVFS_MODE_END || param->mode < 0)
		param->mode = IS_DVFS_MODE_END;
	if (param->resol > IS_DVFS_RESOL_END || param->resol < 0)
		param->resol = IS_DVFS_RESOL_END;
	if (param->fps > IS_DVFS_FPS_END || param->fps < 0)
		param->fps = IS_DVFS_FPS_END;

	return 0;
}

int is_hw_dvfs_get_face(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	if (param->rear_face && param->front_face)
		return IS_DVFS_FACE_PIP;
	else if (param->rear_face)
		return IS_DVFS_FACE_REAR;
	else if (param->front_face)
		return IS_DVFS_FACE_FRONT;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_num(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	switch (param->face) {
	case IS_DVFS_FACE_REAR:
		if (is_get_bit_count(param->rear_face) == 1)
			return IS_DVFS_NUM_SINGLE;
		else if (is_get_bit_count(param->rear_face) == 2)
				return IS_DVFS_NUM_DUAL;
		else if (is_get_bit_count(param->rear_face) == 3)
				return IS_DVFS_NUM_TRIPLE;
		else
			return -EINVAL;
	case IS_DVFS_FACE_FRONT:
		if (is_get_bit_count(param->front_face) == 1)
			return IS_DVFS_NUM_SINGLE;
		else
			return -EINVAL;
	case IS_DVFS_FACE_PIP:
		if (is_get_bit_count(param->rear_face) +
			is_get_bit_count(param->front_face) == 2)
			return IS_DVFS_NUM_DUAL;
		else if (is_get_bit_count(param->rear_face) +
			is_get_bit_count(param->front_face) == 3)
			return IS_DVFS_NUM_TRIPLE;
		else
			return -EINVAL;
	default:
		break;
	}
	return -EINVAL;
}

int is_hw_dvfs_get_sensor(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int vendor;
	vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT)
			& IS_DVFS_SCENARIO_VENDOR_MASK;
	switch (param->face) {
	case IS_DVFS_FACE_REAR:
		switch (param->num) {
		case IS_DVFS_NUM_SINGLE:
			if (param->sensor_active_map & param->wide_mask) {
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_WIDE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_WIDE_REMOSAIC)
					return IS_DVFS_SENSOR_WIDE_REMOSAIC;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_WIDE_SSM;
				return IS_DVFS_SENSOR_WIDE;
			} else if (param->sensor_active_map & param->tele_mask) {
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_TELE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_TELE_REMOSAIC)
					return IS_DVFS_SENSOR_TELE_REMOSAIC;
				return IS_DVFS_SENSOR_TELE;
			} else if (param->sensor_active_map & param->ultrawide_mask) {
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_ULTRAWIDE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_ULTRAWIDE_SSM;
				return IS_DVFS_SENSOR_ULTRAWIDE;
			} else if (param->sensor_active_map & param->macro_mask) {
				if (param->sensor_mode == IS_DVFS_SENSOR_MODE_MACRO_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				return IS_DVFS_SENSOR_MACRO;
			} else if (param->sensor_active_map & param->ultratele_mask) {
				return IS_DVFS_SENSOR_ULTRATELE;
			} else {
				return -EINVAL;
			}
		case IS_DVFS_NUM_DUAL:
			if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->tele_mask)) {
				return IS_DVFS_SENSOR_WIDE_TELE;
			} else if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->ultrawide_mask)) {
				return IS_DVFS_SENSOR_WIDE_ULTRAWIDE;
			} else if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->macro_mask)) {
				return IS_DVFS_SENSOR_WIDE_MACRO;
			} else if ((param->sensor_active_map & param->ultrawide_mask) &&
				(param->sensor_active_map & param->tele_mask)) {
				return IS_DVFS_SENSOR_TELE_ULTRAWIDE;
			} else if ((param->sensor_active_map & param->tele_mask) &&
				(param->sensor_active_map & param->ultratele_mask)) {
				return IS_DVFS_SENSOR_TELE_ULTRATELE;
			} else if ((param->sensor_active_map & param->ultrawide_mask) &&
				(param->sensor_active_map & param->ultratele_mask)) {
				return IS_DVFS_SENSOR_ULTRAWIDE_ULTRATELE;
			} else if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->ultratele_mask)) {
				return IS_DVFS_SENSOR_WIDE_ULTRATELE;
			} else {
				return -EINVAL;
			}
		case IS_DVFS_NUM_TRIPLE:
			return IS_DVFS_SENSOR_TRIPLE;
		default:
			break;
		}
	case IS_DVFS_FACE_FRONT:
		if (param->sensor_active_map & param->front_sensor_mask) {
			if (param->sensor_mode == IS_DVFS_SENSOR_MODE_FRONT_FASTAE)
				return IS_DVFS_SENSOR_FRONT_FASTAE;
			else if (param->secure)
				return IS_DVFS_SENSOR_FRONT_SECURE;
			else if (vendor == IS_DVFS_SCENARIO_VENDOR_VT)
				return IS_DVFS_SENSOR_FRONT_VT;
			return IS_DVFS_SENSOR_FRONT;
		} else {
			return -EINVAL;
		}
	case IS_DVFS_FACE_PIP:
		switch (param->num) {
		case IS_DVFS_NUM_DUAL:
			return IS_DVFS_SENSOR_PIP;
		case IS_DVFS_NUM_TRIPLE:
			return IS_DVFS_SENSOR_TRIPLE;
		}
		break;
	default:
		break;
	}
	return -EINVAL;
}

int is_hw_dvfs_get_mode(struct is_device_ischain *device, int flag_capture,
	struct is_dvfs_scenario_param *param)
{
	int common = param->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK;

	if (param->sensor_only)
		return IS_DVFS_MODE_SENSOR_ONLY;

	if (flag_capture)
		return IS_DVFS_MODE_CAPTURE;

	if (common == IS_DVFS_SCENARIO_COMMON_MODE_PHOTO)
		return IS_DVFS_MODE_PHOTO;
	else if (common == IS_DVFS_SCENARIO_COMMON_MODE_VIDEO)
		return IS_DVFS_MODE_VIDEO;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_resol(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	struct is_resourcemgr *resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl;
	int width, height, resol = 0;
	u32 sensor_size = 0;

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	width = dvfs_ctrl->dvfs_rec_size & 0xffff;
	height = dvfs_ctrl->dvfs_rec_size >> IS_DVFS_SCENARIO_HEIGHT_SHIFT;
	dbg_dvfs(1, "[DVFS] width: %d, height: %d\n",
		device,	width, height);
	resol = width * height;

	sensor_size = is_sensor_g_width(device->sensor) * is_sensor_g_height(device->sensor);
	if (param->mode == IS_DVFS_MODE_PHOTO && resol == sensor_size)
		return IS_DVFS_RESOL_FULL;
	else if (resol < IS_DVFS_RESOL_FHD_TH)
		return IS_DVFS_RESOL_FHD;
	else if (resol < IS_DVFS_RESOL_UHD_TH)
		return IS_DVFS_RESOL_UHD;
	else if (resol < IS_DVFS_RESOL_8K_TH)
		return IS_DVFS_RESOL_8K;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_fps(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	if (param->sensor_fps <= 24)
		return IS_DVFS_FPS_24;
	else if (param->sensor_fps <= 30)
		return IS_DVFS_FPS_30;
	else if (param->sensor_fps <= 60)
		return IS_DVFS_FPS_60;
	else if (param->sensor_fps <= 120)
		return IS_DVFS_FPS_120;
	else if (param->sensor_fps <= 240)
		return IS_DVFS_FPS_240;
	else if (param->sensor_fps <= 480)
		return IS_DVFS_FPS_480;
	else
		return -EINVAL;
}

int is_hw_dvfs_get_rear_single_wide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD120;
			case IS_DVFS_FPS_240:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD240;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_wide_remosaic_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_WIDE_REMOSAIC_CAPTURE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_tele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_8K:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24;
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_tele_remosaic_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_TELE_REMOSAIC_CAPTURE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_ultrawide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30;
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_macro_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_MACRO_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_MACRO_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_SINGLE_MACRO_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_single_ultratele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_ULTRATELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_ULTRATELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_SINGLE_ULTRATELE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_tele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30;
		case IS_DVFS_RESOL_UHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_macro_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_tele_ultrawide_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_TELE_ULTRAWIDE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_tele_ultratele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_TELE_ULTRATELE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_ultrawide_ultratele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_ULTRAWIDE_ULTRATELE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_rear_dual_wide_ultratele_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRATELE_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_front_single_front_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL;
		else
			return IS_DVFS_SN_FRONT_SINGLE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_FRONT_SINGLE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_MODE_SENSOR_ONLY:
		return IS_DVFS_SN_SENSOR_ONLY_FRONT;
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_pip_dual_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_PIP_DUAL_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_PIP_DUAL_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			return IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30;
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_triple_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_TRIPLE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_TRIPLE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_TRIPLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_TRIPLE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("%s fails. (mode: %s, resol: %s, fps: %s) not supported.", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);
	return -EINVAL;
}

int is_hw_dvfs_get_scenario(struct is_device_ischain *device, int flag_capture)
{
	struct is_dvfs_scenario_param param;

	if (!IS_ENABLED(ENABLE_DVFS))
		return IS_DVFS_SN_DEFAULT;

	is_hw_dvfs_init_face_mask(device, &param);
	if (is_hw_dvfs_get_scenario_param(device, flag_capture, &param))
		return IS_DVFS_SN_DEFAULT;

	dbg_dvfs(1, "[DVFS] %s() face: %s, num: %s, sensor: %s\n", device, __func__,
		str_face[param.face], str_num[param.num], str_sensor[param.sensor]);

#if defined(USE_OFFLINE_PROCESSING)
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state))
			is_hw_offline_save_restore_dvfs_param(device, &param, DVFS_PARAM_RESTORE);
	} else {
		is_hw_offline_save_restore_dvfs_param(device, &param, DVFS_PARAM_SAVE);
	}
#endif

	switch (param.face) {
	case IS_DVFS_FACE_REAR:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			switch (param.sensor) {
			/* wide sensor scenarios */
			case IS_DVFS_SENSOR_WIDE:
				return is_hw_dvfs_get_rear_single_wide_scenario(device, &param);
			case IS_DVFS_SENSOR_WIDE_FASTAE:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE;
			case IS_DVFS_SENSOR_WIDE_REMOSAIC:
				return is_hw_dvfs_get_rear_single_wide_remosaic_scenario(device, &param);
			case IS_DVFS_SENSOR_WIDE_VIDEOHDR:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR;
			case IS_DVFS_SENSOR_WIDE_SSM:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_SSM;
			/* tele sensor scenarios */
			case IS_DVFS_SENSOR_TELE:
				return is_hw_dvfs_get_rear_single_tele_scenario(device, &param);
			case IS_DVFS_SENSOR_TELE_REMOSAIC:
				return is_hw_dvfs_get_rear_single_tele_remosaic_scenario(device, &param);
			/* ultra wide sensor scenarios */
			case IS_DVFS_SENSOR_ULTRAWIDE:
				return is_hw_dvfs_get_rear_single_ultrawide_scenario(device, &param);
			case IS_DVFS_SENSOR_ULTRAWIDE_SSM:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_SSM;
			/* macro sensor scenarios */
			case IS_DVFS_SENSOR_MACRO:
				return is_hw_dvfs_get_rear_single_macro_scenario(device, &param);
			/* ultra tele sensor scenarios */
			case IS_DVFS_SENSOR_ULTRATELE:
				return is_hw_dvfs_get_rear_single_ultratele_scenario(device, &param);
			default:
				goto dvfs_err;
			}
		case IS_DVFS_NUM_DUAL:
			switch (param.sensor) {
			/* wide and tele dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_TELE:
				return is_hw_dvfs_get_rear_dual_wide_tele_scenario(device, &param);
			/* wide and ultra wide dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_ULTRAWIDE:
				return is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(device, &param);
			/* wide and macro dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_MACRO:
				return is_hw_dvfs_get_rear_dual_wide_macro_scenario(device, &param);
			/* tele and ultra wide dual sensor scenarios */
			case IS_DVFS_SENSOR_TELE_ULTRAWIDE:
				return is_hw_dvfs_get_rear_dual_tele_ultrawide_scenario(device, &param);
			/* tele and ultra tele dual sensor scenarios */
			case IS_DVFS_SENSOR_TELE_ULTRATELE:
				return is_hw_dvfs_get_rear_dual_tele_ultratele_scenario(device, &param);
			/* tele and ultra wide dual sensor scenarios */
			case IS_DVFS_SENSOR_ULTRAWIDE_ULTRATELE:
				return is_hw_dvfs_get_rear_dual_ultrawide_ultratele_scenario(device, &param);
			/* wide and ultra tele dual sensor scenarios */
			case IS_DVFS_SENSOR_WIDE_ULTRATELE:
				return is_hw_dvfs_get_rear_dual_wide_ultratele_scenario(device, &param);
			default:
				goto dvfs_err;
			}
		case IS_DVFS_NUM_TRIPLE:
			return is_hw_dvfs_get_triple_scenario(device, &param);
		default:
			goto dvfs_err;
		}
	case IS_DVFS_FACE_FRONT:
		switch (param.num) {
		case IS_DVFS_NUM_SINGLE:
			switch (param.sensor) {
			case IS_DVFS_SENSOR_FRONT:
				return is_hw_dvfs_get_front_single_front_scenario(device, &param);
			case IS_DVFS_SENSOR_FRONT_FASTAE:
				return IS_DVFS_SN_FRONT_SINGLE_FASTAE;
			case IS_DVFS_SENSOR_FRONT_SECURE:
				return IS_DVFS_SN_FRONT_SINGLE_SECURE;
			case IS_DVFS_SENSOR_FRONT_VT:
				return IS_DVFS_SN_FRONT_SINGLE_VT;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_FACE_PIP:
		switch (param.num) {
		case IS_DVFS_NUM_DUAL:
			return is_hw_dvfs_get_pip_dual_scenario(device, &param);
		case IS_DVFS_NUM_TRIPLE:
			return is_hw_dvfs_get_triple_scenario(device, &param);
		default:
			goto dvfs_err;
		}
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("[DVFS] %s fails. (face: %s, num: %s ,sensor: %s) not supported.\n", device,
		__func__, str_face[param.face], str_num[param.num], str_sensor[param.sensor]);
	return -EINVAL;
}

void is_hw_dvfs_init_face_mask(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	int pos;

	pos = IS_DVFS_SENSOR_POSITION_WIDE;
	param->wide_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_TELE;
	param->tele_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_ULTRAWIDE;
	param->ultrawide_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_MACRO;
	param->macro_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_ULTRATELE;
	param->ultratele_mask = (pos < 0) ? 0 : (1 << pos);
	param->rear_mask = param->wide_mask | param->tele_mask |
				param->ultrawide_mask | param->macro_mask | param->ultratele_mask;

	pos = IS_DVFS_SENSOR_POSITION_FRONT;
	param->front_sensor_mask = (pos < 0) ? 0 : (1 << pos);
	param->front_mask = param->front_sensor_mask;

	dbg_dvfs(1, "[DVFS] %s() rear mask: 0x%x, front mask: 0x%x\n", device,
		__func__, param->rear_mask, param->front_mask);
}

bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *nums,
		u32 *types, u32 *scns, bool *operating)
{
	/* if CSIS_PDP use votf, it need to setting. */
	return false;
}
