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
	"NORMAL",
	"FASTAE",
	"REMOSAIC",
	"VIDEOHDR",
	"SSM",
	"VT",
	"SECURE",
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
	"HD",
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
	unsigned int num = is_get_bit_count(param->sensor_active_map);

	if (num == 1)
		return IS_DVFS_NUM_SINGLE;
	else if (num == 2)
		return IS_DVFS_NUM_DUAL;
	else if (num == 3)
		return IS_DVFS_NUM_TRIPLE;
	else
		return -EINVAL;
}

static u32 _check_remosaic_size(struct is_device_ischain *device)
{
	struct is_group *child;
	struct is_subdev *ldr;
	u32 w = 0, h = 0;

	child = &device->group_paf;
	if (child) {
		ldr = &child->leader;
		w = MAX(w, ldr->input.width);
		h = MAX(h, ldr->input.height);

		child = child->child;
	}

	dbg_dvfs(1, "[DVFS] check remosaic size (%d * %d vs %d)\n", device, w, h, MIN_REMOSAIC_SIZE);

	if (w * h >= MIN_REMOSAIC_SIZE)
		return true;

	return false;
}

int is_hw_dvfs_get_sensor(struct is_device_ischain *device, int flag_capture,
	struct is_dvfs_scenario_param *param)
{
	int vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT)
			& IS_DVFS_SCENARIO_VENDOR_MASK;

	if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
		return IS_DVFS_SENSOR_FASTAE;

	if (param->sensor_mode == IS_SPECIAL_MODE_REMOSAIC)
		return IS_DVFS_SENSOR_REMOSAIC;

	if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
		return IS_DVFS_SENSOR_SSM;

	if (vendor == IS_DVFS_SCENARIO_VENDOR_LOW_POWER ||
	    vendor == IS_DVFS_SCENARIO_VENDOR_VT)
		return IS_DVFS_SENSOR_VT;

	if (param->secure)
		return IS_DVFS_SENSOR_SECURE;

	if (flag_capture && _check_remosaic_size(device))
		return IS_DVFS_SENSOR_REMOSAIC;

	return IS_DVFS_SENSOR_NORMAL;
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
	dbg_dvfs(1, "[DVFS] %s() width: %d, height: %d\n",
		device,	__func__, width, height);
	resol = width * height;

	sensor_size = is_sensor_g_width(device->sensor) * is_sensor_g_height(device->sensor);
	if (param->mode == IS_DVFS_MODE_PHOTO &&
		(resol == sensor_size || param->scen == IS_SCENARIO_FULL_SIZE))
		return IS_DVFS_RESOL_FULL;
	else if (resol < IS_DVFS_RESOL_HD_TH)
		return IS_DVFS_RESOL_HD;
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
	struct is_device_sensor *ids;
	u32 device_id;

	core = (struct is_core *)device->interface->core;

	sensor_device = device->sensor;
	resourcemgr = sensor_device->resourcemgr;
	dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	is_hw_dvfs_init_face_mask(device, param);

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

	param->sensor_mode = sensor_device->cfg->special_mode;
	param->sensor_fps = is_sensor_g_framerate(sensor_device);
	param->setfile = (device->setfile & IS_SETFILE_MASK);
	param->scen = (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
	param->dvfs_scenario = dvfs_ctrl->dvfs_scenario;
	param->secure = core->scenario;
	dbg_dvfs(1, "[DVFS] %s() sensor_mode: %d, fps: %d, setfile: %d, scen: %d, dvfs_scenario: %x, secure: %d\n",
		device, __func__, param->sensor_mode, param->sensor_fps,
		param->setfile, param->scen, param->dvfs_scenario, param->secure);

	param->face = is_hw_dvfs_get_face(device, param);
	param->num = is_hw_dvfs_get_num(device, param);
	param->sensor = is_hw_dvfs_get_sensor(device, flag_capture, param);

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

int is_hw_dvfs_get_rear_single_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		if (param->resol == IS_DVFS_RESOL_FULL)
			return IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL;
		else
			return IS_DVFS_SN_REAR_SINGLE_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_CAPTURE;
	case IS_DVFS_MODE_VIDEO:
		switch (param->resol) {
		case IS_DVFS_RESOL_HD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_HD30;
			case IS_DVFS_FPS_240:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_HD240;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_FHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD120;
			case IS_DVFS_FPS_240:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD240;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_8K:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24;
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30;
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

int is_hw_dvfs_get_rear_single_remosaic_scenario(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	dbg_dvfs(1, "[DVFS] %s() mode: %s, resol: %s, fps: %s\n", device, __func__,
		str_mode[param->mode], str_resol[param->resol], str_fps[param->fps]);

	switch (param->mode) {
	case IS_DVFS_MODE_PHOTO:
		return IS_DVFS_SN_REAR_SINGLE_REMOSAIC_PHOTO;
	case IS_DVFS_MODE_CAPTURE:
		return IS_DVFS_SN_REAR_SINGLE_REMOSAIC_CAPTURE;
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
		case IS_DVFS_RESOL_HD:
			switch (param->fps) {
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_HD30;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_HD120;
			default:
				goto dvfs_err;
			}
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

	if (is_hw_dvfs_get_scenario_param(device, flag_capture, &param))
		return IS_DVFS_SN_DEFAULT;

	minfo("[DVFS] scenario - [%s_%s_%s_%s_%s%s]\n", device,
		str_face[param.face], str_num[param.num],
		str_sensor[param.sensor], str_mode[param.mode],
		str_resol[param.resol], str_fps[param.fps]);

#if defined(USE_OFFLINE_PROCESSING)
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state)) {
		if (test_bit(IS_ISCHAIN_OFFLINE_REPROCESSING, &device->state))
			is_hw_offline_save_restore_dvfs_param(device, &param, DVFS_PARAM_RESTORE);
	} else {
		is_hw_offline_save_restore_dvfs_param(device, &param, DVFS_PARAM_SAVE);
	}
#endif

	switch (param.num) {
	case IS_DVFS_NUM_SINGLE:
		switch (param.face) {
		case IS_DVFS_FACE_REAR:
			switch (param.sensor) {
			case IS_DVFS_SENSOR_NORMAL:
				return is_hw_dvfs_get_rear_single_scenario(device, &param);
			case IS_DVFS_SENSOR_FASTAE:
				return IS_DVFS_SN_REAR_SINGLE_FASTAE;
			case IS_DVFS_SENSOR_REMOSAIC:
				return is_hw_dvfs_get_rear_single_remosaic_scenario(device, &param);
			case IS_DVFS_SENSOR_SSM:
				return IS_DVFS_SN_REAR_SINGLE_SSM;
			case IS_DVFS_SENSOR_VT:
				return IS_DVFS_SN_REAR_SINGLE_VT_PHOTO;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_FACE_FRONT:
			switch (param.sensor) {
			case IS_DVFS_SENSOR_NORMAL:
				return is_hw_dvfs_get_front_single_front_scenario(device, &param);
			case IS_DVFS_SENSOR_FASTAE:
				return IS_DVFS_SN_FRONT_SINGLE_FASTAE;
			case IS_DVFS_SENSOR_SECURE:
				return IS_DVFS_SN_FRONT_SINGLE_SECURE;
			case IS_DVFS_SENSOR_VT:
				return IS_DVFS_SN_FRONT_SINGLE_VT;
			default:
				goto dvfs_err;
			}
		default:
			goto dvfs_err;
		}
	case IS_DVFS_NUM_DUAL:
		switch (param.face) {
		case IS_DVFS_FACE_REAR:
			/* wide and tele dual sensor scenarios */
			if ((param.sensor_active_map & param.wide_mask) &&
			    (param.sensor_active_map & param.tele_mask)) {
				return is_hw_dvfs_get_rear_dual_wide_tele_scenario(device, &param);
			/* wide and ultra wide dual sensor scenarios */
			} else if ((param.sensor_active_map & param.wide_mask) &&
			    (param.sensor_active_map & param.ultrawide_mask)) {
				return is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(device, &param);
			/* wide and macro dual sensor scenarios */
			} else if ((param.sensor_active_map & param.wide_mask) &&
			    (param.sensor_active_map & param.macro_mask)) {
				return is_hw_dvfs_get_rear_dual_wide_macro_scenario(device, &param);
			/* tele and ultra wide dual sensor scenarios */
			} else if ((param.sensor_active_map & param.ultrawide_mask) &&
			    (param.sensor_active_map & param.tele_mask)) {
				return is_hw_dvfs_get_rear_dual_tele_ultrawide_scenario(device, &param);
			/* tele and ultra tele dual sensor scenarios */
			} else if ((param.sensor_active_map & param.tele_mask) &&
			    (param.sensor_active_map & param.ultratele_mask)) {
				return is_hw_dvfs_get_rear_dual_tele_ultratele_scenario(device, &param);
			/* tele and ultra wide dual sensor scenarios */
			} else if ((param.sensor_active_map & param.ultrawide_mask) &&
			    (param.sensor_active_map & param.ultratele_mask)) {
				return is_hw_dvfs_get_rear_dual_ultrawide_ultratele_scenario(device, &param);
			/* wide and ultra tele dual sensor scenarios */
			} else if ((param.sensor_active_map & param.wide_mask) &&
			    (param.sensor_active_map & param.ultratele_mask)) {
				return is_hw_dvfs_get_rear_dual_wide_ultratele_scenario(device, &param);
			} else {
				goto dvfs_err;
			}
		case IS_DVFS_FACE_PIP:
			return is_hw_dvfs_get_pip_dual_scenario(device, &param);
		default:
			goto dvfs_err;
		}
	case IS_DVFS_NUM_TRIPLE:
		return is_hw_dvfs_get_triple_scenario(device, &param);
	default:
		goto dvfs_err;
	}

dvfs_err:
	merr("[DVFS] %s fails.", device, __func__);
	return -EINVAL;
}

bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *nums,
		u32 *types, u32 *scns, bool *operating)
{
	/* if CSIS_PDP use votf, it need to setting. */
	return false;
}

static struct v4l2_fract sw_margin = {
	.numerator = 100,
	.denominator = 80,
};

/* CSIS, PDP, 3AA */
static u32 _get_cam_lv(struct is_device_ischain *device, struct is_core *core,
			struct exynos_platform_is *pdata)
{
	struct is_device_sensor *ids;
	struct is_sensor_cfg *cfg;
	u32 device_id;
	u32 cphy, lanes, bit;
	u32 t_clk_csis, t_clk_3aa;
	u32 clk_csis = 0, lv_csis, clk, lv;

	for (device_id = 0; device_id < IS_SENSOR_COUNT; device_id++) {
		ids = is_get_sensor_device(device_id);
		if (!test_bit(IS_SENSOR_START, &ids->state))
			continue;

		cfg = ids->cfg;
		if (!cfg) {
			mwarn("[DVFS] sensor_cfg is NULL.", ids);
			continue;
		}

		if (cfg->dvfs_lv_csis < IS_DVFS_LV_END)
			return cfg->dvfs_lv_csis;

		cphy = ids->pdata->use_cphy;
		lanes = cfg->lanes + 1;

		if (cfg->input[CSI_VIRTUAL_CH_0].hwformat == HW_FORMAT_RAW12)
			bit = 12;
		else
			bit = 10;

		if (cfg->votf) {
			if (cphy)
				t_clk_csis = cfg->mipi_speed * 16 / 7 * lanes / IS_DVFS_POTF_BIT;
			else
				t_clk_csis = cfg->mipi_speed * lanes / IS_DVFS_POTF_BIT;

			t_clk_3aa = cfg->width * cfg->height * cfg->max_fps / IS_DVFS_OTF_PPC / MHZ;

			clk_csis += MAX(t_clk_csis, t_clk_3aa);
		} else {
			if (cphy)
				clk_csis += cfg->mipi_speed * 16 / 7 * lanes / bit / IS_DVFS_OTF_PPC;
			else
				clk_csis += cfg->mipi_speed * lanes / bit / IS_DVFS_OTF_PPC;
		}
		dbg_dvfs(2, "[DVFS] mipi_speed(%d) lanes(%d)\n", device, cfg->mipi_speed, lanes);
	}

	lv_csis = 0;
	if (clk_csis) {
		for (lv = 0; lv < IS_DVFS_LV_END; lv++) {
			clk = pdata->qos_tb[IS_DVFS_CAM][lv][IS_DVFS_VAL_FRQ];

			if (clk >= clk_csis)
				lv_csis = lv;
			else
				break;
		}
	} else {
		lv_csis = 0;
		mwarn("[DVFS] There is no active stream.", device);
	}

	dbg_dvfs(1, "[DVFS] CSIS clk(%d) lv(%d)\n", device, clk_csis, lv_csis);

	if (lv_csis > IS_DVFS_CAM_MIN_LV) {
		lv_csis = IS_DVFS_CAM_MIN_LV;
		dbg_dvfs(1, "[DVFS] change CSIS clk(%d) lv(%d)\n", device, clk_csis, lv_csis);
	}

	return lv_csis;
}

/* TNR, ISP, MCSC, GDC */
static u32 _get_isp_lv(struct is_device_ischain *device, struct exynos_platform_is *pdata)
{
	struct is_device_sensor *ids;
	struct is_device_ischain *idi;
	struct is_sensor_cfg *cfg;
	struct is_group *child;
	struct is_subdev *ldr;
	u32 clk_isp = 0, lv_isp = 0, clk, lv;
	u32 w = 0, h = 0;
	u32 max_fps, device_id;
	u32 ppc = IS_DVFS_M2M_PPC;
	u32 stripe_num;

	for (device_id = 0; device_id < IS_SENSOR_COUNT; device_id++) {
		ids = is_get_sensor_device(device_id);
		if (!test_bit(IS_SENSOR_START, &ids->state))
			continue;

		cfg = ids->cfg;
		max_fps = cfg->max_fps;
		idi = ids->ischain;

		if (!idi)
			continue;

		child = &idi->group_isp;
		while (child) {
			ldr = &child->leader;
			w = MAX(w, ldr->input.width);
			h = MAX(h, ldr->input.height);

			child = child->child;
		}

		stripe_num = DIV_ROUND_UP(w - STRIPE_MARGIN_WIDTH * 2,
				ALIGN_DOWN(GROUP_ISP_MAX_WIDTH - STRIPE_MARGIN_WIDTH * 2,
				STRIPE_WIDTH_ALIGN));
		w += STRIPE_MARGIN_WIDTH * 2 * (stripe_num - 1);

		clk_isp += w * h * max_fps / ppc / MHZ
			* sw_margin.numerator / sw_margin.denominator;
		dbg_dvfs(2, "[DVFS] w(%d) h(%d) max_fps(%d) stripe_num(%d)\n", device, w, h, max_fps, stripe_num);
	}

	if (clk_isp) {
		for (lv = 0; lv < IS_DVFS_LV_END; lv++) {
			clk = pdata->qos_tb[IS_DVFS_ISP][lv][IS_DVFS_VAL_FRQ];

			if (clk >= clk_isp)
				lv_isp = lv;
			else
				break;
		}
	} else {
		lv_isp = 0;
		mwarn("[DVFS] There is no active stream.", device);
	}

	dbg_dvfs(1, "[DVFS] clk(isp:%d MHz) lv(%d)\n", device, clk_isp, lv_isp);

	if (lv_isp > IS_DVFS_ISP_MIN_LV) {
		lv_isp = IS_DVFS_ISP_MIN_LV;
		dbg_dvfs(1, "[DVFS] change clk(isp:%d MHz) lv(%d)\n", device, clk_isp, lv_isp);
	}

	return lv_isp;
}

u32 is_hw_dvfs_get_lv(struct is_device_ischain *device, u32 type)
{
	struct is_core *core;
	struct exynos_platform_is *pdata;
	u32 lv;

	if (!device) {
		warn("[DVFS] device is null");
		return 0;
	}

	core = is_get_is_core();
	if (!core) {
		merr("[DVFS] core is null.", device);
		return 0;
	}

	pdata = core->pdata;
	if (!pdata) {
		merr("[DVFS] pdata is null.", device);
		return 0;
	}

	switch (type) {
	case IS_DVFS_CAM:
		lv = _get_cam_lv(device, core, pdata);
		break;
	case IS_DVFS_ISP:
		lv = _get_isp_lv(device, pdata);
		break;
	default:
		return 0;
	}

	return lv;
}
