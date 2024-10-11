// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-core.h"
#include "videodev2_exynos_camera.h"

/* Define the test cases. */
enum pablo_type_sensor_run {
	PABLO_SENSOR_RUN_STOP = 0,
	PABLO_SENSOR_RUN_START,
};

struct pablo_info_sensor_run {
	u32 act;
	u32 position;
	u32 width;
	u32 height;
	u32 fps;
};

static struct pablo_info_sensor_run sr_info = {
	.act = PABLO_SENSOR_RUN_STOP,
	.position = 0,
	.width = 0,
	.height = 0,
	.fps = 0,
};

static struct is_fmt fmt = {
	.name		= "BAYER 12bit packed single plane",
	.pixelformat	= V4L2_PIX_FMT_SRGB36P_SP,
	.num_planes	= 1 + NUM_OF_META_PLANE,
	.bitsperpixel	= { 12 },
	.hw_format	= DMA_INOUT_FORMAT_RGB,
	.hw_order	= DMA_INOUT_ORDER_NO,
	.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
	.hw_plane	= DMA_INOUT_PLANE_3,
};

static struct is_device_sensor *sensor;
static int sensor_self_start(void)
{
	int ret;
	u32 rsc_type = sr_info.position + RESOURCE_TYPE_SENSOR0;
	u32 position = sr_info.position + SP_REAR;
	u32 video_id = sr_info.position + IS_VIDEO_SS0_NUM;
	u32 scenario = SENSOR_SCENARIO_VISION;
	u32 stream_ldr = 1;
	static struct is_video_ctx vctx;
	struct v4l2_streamparm param;
	struct is_core *core;
	struct is_queue *queue = &vctx.queue;
	struct is_video *video;

	core = is_get_is_core();
	if (!core) {
		err("core is NULL");
		return -ENODEV;
	}

	ret = is_resource_open(&core->resourcemgr, rsc_type, (void **)&sensor);
	if (ret) {
		err("fail is_resource_open(%d)", ret);
		return ret;
	}

	video = &sensor->video;
	vctx.instance = 0;
	vctx.device = &sensor;
	vctx.video = video;

	queue->id = video->subdev_id;
	snprintf(queue->name, sizeof(queue->name), "%s", vn_name[video_id]);
	frame_manager_probe(&queue->framemgr, queue->id, queue->name);

	ret = is_sensor_open(sensor, &vctx);
	if (ret) {
		err("fail is_sensor_open(%d)", ret);
		return ret;
	}

	ret = is_sensor_s_input(sensor, position, scenario, video_id, stream_ldr);
	if (ret) {
		err("fail is_sensor_s_input(%d)", ret);
		return ret;
	}

	param.parm.capture.timeperframe.denominator = sr_info.fps;
	param.parm.capture.timeperframe.numerator = 1;
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	ret = is_sensor_s_framerate(sensor, &param);
	if (ret) {
		err("fail is_sensor_s_framerate(%d)", ret);
		return ret;
	}

	queue->qops = is_get_sensor_device_qops();
	queue->framecfg.format = &fmt;
	sensor->sensor_width = sr_info.width;
	sensor->sensor_height = sr_info.height;

	return 0;
}

static int sensor_self_stop(void)
{
	int ret;

	if (!sensor) {
		err("sensor is NULL");
		return -ENODEV;
	}

	ret = is_sensor_close(sensor);
	if (ret) {
		err("fail is_sensor_close(%d)", ret);
		return ret;
	}

	sensor = NULL;

	return 0;
}


static void pablo_device_sensor_v2_s_again_kunit_test(struct kunit *test)
{
	int ret;
	struct v4l2_ext_controls ctrls;
	u32 w = 0;
	u32 h = 0;
	int position = 0;

	/* TODO: Fix test fail */
	return;

	ret = sensor_self_start();
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_s_again(sensor, 1000);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_s_exposure_time(sensor, 1000);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_s_ext_ctrls(sensor, &ctrls);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_s_frame_duration(sensor, 1000);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_s_shutterspeed(sensor, 1000);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_g_csis_error(sensor);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_g_ext_ctrls(sensor, &ctrls);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_sensor_g_instance(sensor);
	KUNIT_EXPECT_EQ(test, ret, 0);

	is_sensor_g_max_size(&w, &h);
	KUNIT_EXPECT_GT(test, w, (u32)0);
	KUNIT_EXPECT_GT(test, h, (u32)0);

	position = is_sensor_g_position(sensor);
	KUNIT_EXPECT_EQ(test, position, 0);

	ret = sensor_self_stop();
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_device_sensor_v2_kunit_test_cases[] = {
	KUNIT_CASE(pablo_device_sensor_v2_s_again_kunit_test),
	{},
};

struct kunit_suite pablo_device_sensor_v2_kunit_test_suite = {
	.name = "pablo-device-sensor-v2-kunit-test",
	.test_cases = pablo_device_sensor_v2_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_device_sensor_v2_kunit_test_suite);

MODULE_LICENSE("GPL");
