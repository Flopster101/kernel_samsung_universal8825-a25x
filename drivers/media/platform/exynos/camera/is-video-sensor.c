/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "is-core.h"
#include "is-device-sensor.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-resourcemgr.h"
#include "is-vender.h"

#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
extern int s2mpb02_ir_led_current(uint32_t current_value);
extern int s2mpb02_ir_led_pulse_width(uint32_t width);
extern int s2mpb02_ir_led_pulse_delay(uint32_t delay);
extern int s2mpb02_ir_led_max_time(uint32_t max_time);
#endif

#define SYSREG_ISP_MIPIPHY_CON	0x144F1040
#define PMU_MIPI_PHY_M0S2_CONTROL	0x10480734

static int is_ssx_video_check_3aa_state(struct is_device_sensor *device,  u32 next_id)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct exynos_platform_is *pdata;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_group *group_3aa;

	core = is_get_is_core();
	if (!core) {
		merr("[SS%d] The core is NULL", device, device->instance);
		ret = -ENXIO;
		goto p_err;
	}

	pdata = core->pdata;
	if (!pdata) {
		merr("[SS%d] The pdata is NULL", device, device->instance);
		ret = -ENXIO;
		goto p_err;
	}

	if (next_id >= pdata->num_of_ip.taa) {
		merr("[SS%d] The next_id is too big.(next_id: %d)", device, device->instance, next_id);
		ret = -ENODEV;
		goto p_err;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("[SS%d] current sensor is not in stop state", device, device->instance);
		ret = -EBUSY;
		goto p_err;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		if (test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
			ischain = sensor->ischain;
			if (!ischain)
				continue;

			group_3aa = &ischain->group_3aa;
			if (group_3aa->id == GROUP_ID_3AA0 + next_id) {
				merr("[SS%d] 3AA%d is busy", device, device->instance, next_id);
				ret = -EBUSY;
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

static int is_ssx_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vender_ssx_video_s_ctrl(ctrl, device);
	if (ret) {
		merr("is_vender_ssx_video_s_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		{
			u32 sstream, instant, noblock, standby;

			sstream = (ctrl->value & SENSOR_SSTREAM_MASK) >> SENSOR_SSTREAM_SHIFT;
			standby = (ctrl->value & SENSOR_USE_STANDBY_MASK) >> SENSOR_USE_STANDBY_SHIFT;
			instant = (ctrl->value & SENSOR_INSTANT_MASK) >> SENSOR_INSTANT_SHIFT;
			noblock = (ctrl->value & SENSOR_NOBLOCK_MASK) >> SENSOR_NOBLOCK_SHIFT;
			/*
			 * nonblock(0) : blocking command
			 * nonblock(1) : non-blocking command
			 */

			minfo(" Sensor Stream %s : (cnt:%d, noblk:%d, standby:%d)\n", device,
				sstream ? "On" : "Off", instant, noblock, standby);

			device->use_standby = standby;
			device->sstream = sstream;

			if (sstream == IS_ENABLE_STREAM) {
				ret = is_sensor_front_start(device, instant, noblock);
				if (ret) {
					merr("is_sensor_front_start is fail(%d)", device, ret);
					goto p_err;
				}
			} else {
				ret = is_sensor_front_stop(device, false);
				if (ret) {
					merr("is_sensor_front_stop is fail(%d)", device, ret);
					goto p_err;
				}
			}
		}
		break;
	case V4L2_CID_IS_CHANGE_CHAIN_PATH:	/* Change OTF HW path */
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
				ctrl->value, ret);
			goto p_err;
		}

		ret = is_group_change_chain(device->groupmgr, &device->group_sensor, ctrl->value);
		if (ret)
			merr("is_group_change_chain is fail(%d)", device, ret);
		break;
	case V4L2_CID_IS_CHECK_CHAIN_STATE:
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
				ctrl->value, ret);
			goto p_err;
		}
		break;
	/*
	 * gain boost: min_target_fps,  max_target_fps, scene_mode
	 */
	case V4L2_CID_IS_MIN_TARGET_FPS:
		minfo("%s: set min_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value, device->state);

		device->min_target_fps = ctrl->value;
		break;
	case V4L2_CID_IS_MAX_TARGET_FPS:
		minfo("%s: set max_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value, device->state);

		device->max_target_fps = ctrl->value;
		break;
	case V4L2_CID_SCENEMODE:
		if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set scene_mode: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->scene_mode = ctrl->value;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		if (is_sensor_s_frame_duration(device, ctrl->value)) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		if (is_sensor_s_exposure_time(device, ctrl->value)) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IS_S_SENSOR_SIZE:
		device->sensor_width = (ctrl->value & SENSOR_SIZE_WIDTH_MASK) >> SENSOR_SIZE_WIDTH_SHIFT;
		device->sensor_height = (ctrl->value & SENSOR_SIZE_HEIGHT_MASK) >> SENSOR_SIZE_HEIGHT_SHIFT;
		minfo("sensor size : %d x %d (0x%08X)", device,
				device->sensor_width, device->sensor_height, ctrl->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
	case V4L2_CID_IS_END_OF_STREAM:
	case V4L2_CID_IS_CAMERA_TYPE:
	case V4L2_CID_IS_SET_SETFILE:
	case V4L2_CID_IS_HAL_VERSION:
	case V4L2_CID_IS_DEBUG_DUMP:
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER2:
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_IS_INTENT:
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
	case V4L2_CID_IS_TRANSIENT_ACTION:
	case V4L2_CID_IS_FORCE_FLASH_MODE:
	case V4L2_CID_IS_OPENING_HINT:
	case V4L2_CID_IS_CLOSING_HINT:
	case V4L2_CID_IS_S_LLC_CONFIG:
		ret = is_vidioc_s_ctrl(file, priv, ctrl);
		if (ret) {
			merr("is_vidioc_s_ctrl is fail(%d): %lx, %d", device, ret,
							ctrl->id, ctrl->id & 0xff);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SECURE_MODE:
	{
		u32 scenario;
		struct is_core *core;

		scenario = (ctrl->value & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
		core = is_get_is_core();
		if (core && scenario == IS_SCENARIO_SECURE) {
			mvinfo("[SCENE_MODE] SECURE scenario(%d) was detected\n",
				device, GET_VIDEO(vctx), scenario);
			device->ex_scenario = core->scenario = scenario;
		}
		break;
	}
	case V4L2_CID_SENSOR_SET_GAIN:
		if (is_sensor_s_again(device, ctrl->value)) {
			err("failed to set gain : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_SHUTTER:
		if (is_sensor_s_shutterspeed(device, ctrl->value)) {
			err("failed to set shutter speed : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
	case V4L2_CID_IRLED_SET_WIDTH:
		ret = s2mpb02_ir_led_pulse_width(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse width : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_DELAY:
		ret = s2mpb02_ir_led_pulse_delay(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse delay : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_CURRENT:
		ret = s2mpb02_ir_led_current(ctrl->value);
		if (ret < 0) {
			err("failed to set irled current : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_ONTIME:
		ret = s2mpb02_ir_led_max_time(ctrl->value);
		if (ret < 0) {
			err("failed to set irled max time : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#endif
	case V4L2_CID_IS_S_BNS:
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE:
		device->ex_mode = ctrl->value;
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE_EXTRA:
		device->ex_mode_extra = ctrl->value;
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE_FORMAT:
		device->ex_mode_format = ctrl->value;
		break;
	case V4L2_CID_IS_S_STANDBY_OFF:
		/*
		 * User must set before qbuf for next stream on in standby on state.
		 * If it is not cleared, all qbuf buffer is returned with unprocessed.
		 */
		clear_bit(IS_GROUP_STANDBY, &device->group_sensor.state);
		minfo("Clear STANDBY state", device);
		break;
	default:
		ret = is_sensor_s_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_s_ext_ctrls(struct file *file, void *priv,
				 struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_sensor_s_ext_ctrls(device, ctrls);
	if (ret) {
		merr("s_ext_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_ext_ctrls(struct file *file, void *priv,
				    struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	device = GET_DEVICE(vctx);
	FIMC_BUG(!device);

	ret = is_sensor_g_ext_ctrls(device, ctrls);
	if (ret) {
		merr("g_ext_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vender_ssx_video_g_ctrl(ctrl, device);
	if (ret) {
		merr("is_vender_ssx_video_g_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_G_STREAM:
		if (device->instant_ret)
			ctrl->value = device->instant_ret;
		else
			ctrl->value = (test_bit(IS_SENSOR_FRONT_START, &device->state) ?
				IS_ENABLE_STREAM : IS_DISABLE_STREAM);
		break;
	case V4L2_CID_IS_G_BNS_SIZE:
		{
			u32 width, height;

			width = is_sensor_g_bns_width(device);
			height = is_sensor_g_bns_height(device);

			ctrl->value = (width << 16) | height;
		}
		break;
	case V4L2_CID_IS_G_SNR_STATUS:
		if (test_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		/* additional set for ESD notification */
		if (test_bit(IS_SENSOR_ESD_RECOVERY, &device->state))
			ctrl->value += 2;
		if (test_bit(IS_SENSOR_ASSERT_CRASH, &device->state))
			ctrl->value += 4;
		break;
	case V4L2_CID_IS_G_MIPI_ERR:
		ctrl->value = is_sensor_g_csis_error(device);
		break;
	case V4L2_CID_IS_G_ACTIVE_CAMERA:
		if (test_bit(IS_SENSOR_S_INPUT, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		break;
	default:
		ret = is_sensor_g_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tfp;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	cp = &parm->parm.capture;
	tfp = &cp->timeperframe;
	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tfp->numerator = 1;
	tfp->denominator = device->image.framerate;

	return 0;
}

static int is_ssx_video_s_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_s_framerate(device, parm);
	if (ret) {
		merr("is_ssx_s_framerate is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_ssx_video_ioctl_ops = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_prepare_buf		= is_vidioc_prepare_buf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_s_ctrl			= is_ssx_video_s_ctrl,
	.vidioc_s_ext_ctrls		= is_ssx_video_s_ext_ctrls,
	.vidioc_g_ext_ctrls		= is_ssx_video_g_ext_ctrls,
	.vidioc_g_ctrl			= is_ssx_video_g_ctrl,
	.vidioc_g_parm			= is_ssx_video_g_parm,
	.vidioc_s_parm			= is_ssx_video_s_parm,
};

int is_ssx_video_probe(void *data)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_video *video;
	char name[30];
	u32 device_id;

	FIMC_BUG(!data);

	device = (struct is_device_sensor *)data;
	device_id = device->device_id;
	video = &device->video;
	video->resourcemgr = device->resourcemgr;
	snprintf(name, sizeof(name), "%s%d", IS_VIDEO_SSX_NAME, device_id);

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_SS0 + device_id;
	video->group_ofs = offsetof(struct is_device_sensor, group_sensor);
	video->subdev_id = ENTRY_SENSOR;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_SSX_READY_BUFFERS;

	ret = is_video_probe(video,
		name,
		IS_VIDEO_SS0_NUM + device_id,
#ifdef CONFIG_USE_SENSOR_GROUP
		VFL_DIR_M2M,
#else
		VFL_DIR_RX,
#endif
		&device->resourcemgr->mem,
		&device->v4l2_dev,
		NULL,
		&is_ssx_video_ioctl_ops,
		NULL);
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
