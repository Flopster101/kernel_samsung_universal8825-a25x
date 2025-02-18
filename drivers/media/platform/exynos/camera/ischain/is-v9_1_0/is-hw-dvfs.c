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

#include <dt-bindings/camera/exynos_is_dt.h>

#include "is-core.h"
#include "is-dvfs.h"
#include "is-hw-dvfs.h"
#include "is-param.h"

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
	"WIDE_VT",
	"TELE",
	"TELE_REMOSAIC",
	"ULTRAWIDE",
	"ULTRAWIDE_SSM",
	"MACRO",
	"WIDE_TELE",
	"WIDE_ULTRAWIDE",
	"WIDE_MACRO",
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

static int is_hw_dvfs_get_hf(struct is_device_ischain *device,
	struct is_dvfs_scenario_param *param)
{
	struct param_mcs_output *mcs_output;

	/* only wide single */
	if (!(param->num == IS_DVFS_NUM_SINGLE))
		return 0;

	mcs_output = is_itf_g_param(device, NULL, PARAM_MCS_OUTPUT5);
	if (mcs_output->dma_cmd == DMA_OUTPUT_COMMAND_ENABLE)
		return 1;
	else
		return 0;
}

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
	param->sensor = is_hw_dvfs_get_sensor(device, param);
	param->hf = is_hw_dvfs_get_hf(device, param);

	if (param->face > IS_DVFS_FACE_END || param->face < 0)
		param->face = IS_DVFS_FACE_END;
	if (param->num > IS_DVFS_NUM_END || param->num < 0)
		param->num = IS_DVFS_NUM_END;
	if (param->sensor > IS_DVFS_SENSOR_END || param->sensor < 0)
		param->sensor = IS_DVFS_SENSOR_END;

	param->mode = is_hw_dvfs_get_mode(device, flag_capture, param);
	param->resol = is_hw_dvfs_get_resol(device, param);
	param->fps = is_hw_dvfs_get_fps(device, param);

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
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_SPECIAL_MODE_REMOSAIC)
					return IS_DVFS_SENSOR_WIDE_REMOSAIC;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_WIDE_SSM;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_LOW_POWER)
					return IS_DVFS_SENSOR_WIDE_VT;
				return IS_DVFS_SENSOR_WIDE;
			} else if ((param->sensor_active_map & param->tele_mask)
				|| (param->sensor_active_map & param->tele2_mask)) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (param->sensor_mode == IS_SPECIAL_MODE_REMOSAIC)
					return IS_DVFS_SENSOR_TELE_REMOSAIC;
				return IS_DVFS_SENSOR_TELE;
			} else if (param->sensor_active_map & param->ultrawide_mask) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				if (vendor == IS_DVFS_SCENARIO_VENDOR_SSM)
					return IS_DVFS_SENSOR_ULTRAWIDE_SSM;
				return IS_DVFS_SENSOR_ULTRAWIDE;
			} else if (param->sensor_active_map & param->macro_mask) {
				if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
					return IS_DVFS_SENSOR_WIDE_FASTAE;
				return IS_DVFS_SENSOR_MACRO;
			} else {
				return -EINVAL;
			}
		case IS_DVFS_NUM_DUAL:
			if (((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->tele_mask))
				|| (param->sensor_active_map & param->tele2_mask)) {
				return IS_DVFS_SENSOR_WIDE_TELE;
			} else if (((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->ultrawide_mask))
				|| (param->sensor_active_map & param->ultrawide_mask)) {
				return IS_DVFS_SENSOR_WIDE_ULTRAWIDE;
			} else if ((param->sensor_active_map & param->wide_mask) &&
				(param->sensor_active_map & param->macro_mask)) {
				return IS_DVFS_SENSOR_WIDE_MACRO;
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
			if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
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
		if (param->sensor_mode == IS_SPECIAL_MODE_FASTAE)
			return IS_DVFS_SENSOR_WIDE_FASTAE;

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
	int vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;

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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				if (vendor == IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR)
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEOHDR;
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60_HF;
				default:
					goto dvfs_err;
				}
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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD120;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_8K:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K24_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_8K30_HF;
				default:
					goto dvfs_err;
				}
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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
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
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24_HF;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30;
				case 1:
					return IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30_HF;
				default:
					goto dvfs_err;
				}
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
	int vendor = (param->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;

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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30_HF_SUPERSTEADY;
					else
						goto dvfs_err;
				default:
					goto dvfs_err;
				}
			case IS_DVFS_FPS_60:
				switch (param->hf) {
				case 0:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_SUPERSTEADY;
					return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60;
				case 1:
					if (vendor == IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY)
						return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD60_HF_SUPERSTEADY;
					else
						goto dvfs_err;
				default:
					goto dvfs_err;
				}
			/* use the same scenario as that of wide sensor */
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD120;
			case IS_DVFS_FPS_480:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD480;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD60;
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
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60;
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
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60;
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
			case IS_DVFS_FPS_24:
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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60;
			case IS_DVFS_FPS_120:
				return IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120;
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
			case IS_DVFS_FPS_24:
			case IS_DVFS_FPS_30:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD30;
			case IS_DVFS_FPS_60:
				return IS_DVFS_SN_TRIPLE_VIDEO_FHD60;
			default:
				goto dvfs_err;
			}
		case IS_DVFS_RESOL_UHD:
			switch (param->fps) {
			case IS_DVFS_FPS_24:
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
			case IS_DVFS_SENSOR_WIDE_VT:
				return IS_DVFS_SN_REAR_SINGLE_WIDE_VT;
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
		if (param.sensor == IS_DVFS_SENSOR_WIDE_FASTAE)
			return IS_DVFS_SN_REAR_SINGLE_WIDE_FASTAE;
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
	pos = IS_DVFS_SENSOR_POSITION_TELE2;
	param->tele2_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_ULTRAWIDE;
	param->ultrawide_mask = (pos < 0) ? 0 : (1 << pos);
	pos = IS_DVFS_SENSOR_POSITION_MACRO;
	param->macro_mask = (pos < 0) ? 0 : (1 << pos);
	param->rear_mask = param->wide_mask | param->tele_mask | param->tele2_mask |
				param->ultrawide_mask | param->macro_mask;

	pos = IS_DVFS_SENSOR_POSITION_FRONT;
	param->front_sensor_mask = (pos < 0) ? 0 : (1 << pos);
	param->front_mask = param->front_sensor_mask;

	dbg_dvfs(1, "[DVFS] %s() rear mask: 0x%x, front mask: 0x%x\n", device,
		__func__, param->rear_mask, param->front_mask);
}

bool is_hw_dvfs_get_bundle_update_seq(u32 scenario_id, u32 *nums,
		u32 *types, u32 *scns, bool *operating)
{
	int i = 0;
	bool need_update = false;

	/* By adopting CSIS_PDP_VOTF_GLOBAL_WA, it doesn't need this code anymore */
	return false;

	FIMC_BUG(!types);
	FIMC_BUG(!scns);

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_WIDE_CAPTURE:
	case IS_DVFS_SN_REAR_SINGLE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE:
	case IS_DVFS_SN_FRONT_SINGLE_CAPTURE:
		need_update = true;
		*operating = true;
		break;
	case IS_DVFS_SN_REAR_SINGLE_WIDE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_WIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_PHOTO:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD60:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60:
		need_update = true;
		break;
	default:
		if (*operating) {
			need_update = true;
			*operating = false;
		}
		break;
	}

	/*
	 * If CSIS - PDP votf used as global mode,
	 * It should be ensured that
	 * frequency of CSIS should always be greater than or equal to PDP.
	 * As shown in the sequence below,
	 * it should be increased to Max level and
	 * level must be changed according to the sequence.
	 * It is the same when restoring after change.
	 */
	if (need_update) {
		types[i] = IS_DVFS_CSIS;
		scns[i] = IS_DVFS_SN_MAX;
		i++;
		types[i] = IS_DVFS_CAM;
		scns[i] = IS_DVFS_SN_MAX;
		i++;
		types[i] = IS_DVFS_CAM;
		scns[i] = scenario_id;
		i++;
		types[i] = IS_DVFS_CSIS;
		scns[i] = scenario_id;
		i++;

		*nums = i;
	}

	return need_update;
}

u32 is_hw_dvfs_get_lv(struct is_device_ischain *device, u32 type)
{
	return 0;
}
