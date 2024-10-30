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

#include <linux/module.h>
#include <linux/delay.h>
#include <videodev2_exynos_camera.h>

#include "is-core.h"
#include "is-framemgr.h"
#include "is-resourcemgr.h"
#include "is-device-sensor.h"
#include "is-video.h"
#include "is-hw.h"

/*
 * @sensor self test
 */
static int pablo_test_set_sensor_run(const char *val, const struct kernel_param *kp);
static int pablo_test_get_sensor_run(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_sensor_run = {
	.set = pablo_test_set_sensor_run,
	.get = pablo_test_get_sensor_run,
};

static int pablo_test_set_phy_tune(const char *val, const struct kernel_param *kp);
static int pablo_test_get_phy_tune(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_phy_tune = {
	.set = pablo_test_set_phy_tune,
	.get = pablo_test_get_phy_tune,
};

static int pablo_test_set_rt_phy_tune(const char *val, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_rt_phy_tune = {
	.set = pablo_test_set_rt_phy_tune,
};

module_param_cb(test_sensor_run, &pablo_param_ops_sensor_run, NULL, 0644);
module_param_cb(test_phy_tune, &pablo_param_ops_phy_tune, NULL, 0644);
module_param_cb(test_rt_phy_tune, &pablo_param_ops_rt_phy_tune, NULL, 0644);

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
	u32 ex_mode;
};

static struct pablo_info_sensor_run sr_info = {
	.act = PABLO_SENSOR_RUN_STOP,
	.position = 0,
	.width = 0,
	.height = 0,
	.fps = 0,
	.ex_mode = 0,
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
static int sensor_self_stop(void)
{
	int ret;
	struct is_device_csi *csi;

	if (!sensor) {
		err("sensor is NULL");
		return -ENODEV;
	}

	ret = is_sensor_close(sensor);
	if (ret) {
		err("fail is_sensor_close(%d)", ret);
		return ret;
	}

	/* HACK */
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	ret = phy_power_off(csi->phy);
	if (ret)
		err("failed to csi%d power off", csi->ch);

	sensor = NULL;

	return 0;
}

static int sensor_self_start(void)
{
	int ret;
	u32 position = sr_info.position + SP_REAR;
	u32 i, device_id, rsc_type, video_id;
	u32 scenario = SENSOR_SCENARIO_VISION;
	u32 stream_ldr = 1;
	static struct is_video_ctx vctx;
	struct v4l2_streamparm param;
	struct is_core *core;
	struct is_queue *queue = &vctx.queue;
	struct is_video *video;
	struct is_module_enum *module;
	struct is_device_csi *csi;

	core = is_get_is_core();

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		ret = is_search_sensor_module_with_position(&core->sensor[i],
							position, &module);
		if (!ret) {
			device_id = i;
			info("%s: pos(%d) dev(%d)", __func__, position, device_id);
			break;
		}
	}

	if (ret) {
		err("position(%d) is not supported.", position);
		return -ENODEV;
	}

	rsc_type = device_id + RESOURCE_TYPE_SENSOR0;
	video_id = device_id + IS_VIDEO_SS0_NUM;

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
	sensor->ex_mode = sr_info.ex_mode;

	ret = CALL_QOPS(queue, s_fmt, sensor, queue);
	if (ret) {
		err("fail sensor s_fmt(%d)", ret);
		return ret;
	}

	ret = CALL_QOPS(queue, start_streaming, sensor, queue);
	if (ret) {
		err("fail sensor start_streaming(%d)", ret);
		return ret;
	}

	ret = is_sensor_front_start(sensor, 0, 0);
	if (ret) {
		err("fail is_sensor_front_start(%d)", ret);
		return ret;
	}

	/* HACK */
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	ret = phy_power_on(csi->phy);
	if (ret) {
		err("failed to csi%d power on", csi->ch);
		return ret;
	}

	return 0;
}

static int pablo_test_set_sensor_run(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &sr_info.act);
	if (ret) {
		err("Invalid act %d ret %d", sr_info.act, ret);
		goto func_exit;
	}

	switch (sr_info.act) {
	case PABLO_SENSOR_RUN_STOP:
		if (argc < 1) {
			err("Not enough parameters. %d < 1", argc);
			goto func_exit;
		}

		sensor_self_stop();

		info("stop sensor\n");
		break;
	case PABLO_SENSOR_RUN_START:
		if (argc < 5) {
			err("Not enough parameters. %d < 5", argc);
			err("ex) echo 1 0 4032 3024 30");
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &sr_info.position);
		ret |= kstrtouint(argv[2], 0, &sr_info.width);
		ret |= kstrtouint(argv[3], 0, &sr_info.height);
		ret |= kstrtouint(argv[4], 0, &sr_info.fps);
		if (argc > 5)
			ret |= kstrtouint(argv[5], 0, &sr_info.ex_mode);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		info("start sensor(position %d: %d x %d @ %d fps, ex(%d))\n",
			sr_info.position, sr_info.width, sr_info.height,
			sr_info.fps, sr_info.ex_mode);

		sensor_self_start();
		break;
	default:
		err("NOT supported act %u", sr_info.act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_test_get_sensor_run(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "> Set\n");
	ret += sprintf(buffer + ret, "# echo <act> <position> <width> <height> <fps> > sensor_run\n");
	ret += sprintf(buffer + ret, " - act: 0=PABLO_SENSOR_RUN_STOP, 1=PABLO_SENSOR_RUN_START\n");
	ret += sprintf(buffer + ret, " - position: 0=REAR 1=FRONT 2=REAR2 3=FRONT2 ...\n");
	ret += sprintf(buffer + ret, "act %d position %d width %d height %d fps %d ex %d\n",
			sr_info.act,
			sr_info.position,
			sr_info.width,
			sr_info.height,
			sr_info.fps,
			sr_info.ex_mode);

	return ret;
}

/*
 * @phy tune
 */
static int sensor_stream_on(struct v4l2_subdev *sd_csi, struct v4l2_subdev *sd_mod)
{
	int ret;

	/* link */
	ret = v4l2_subdev_call(sd_csi, video, s_stream, IS_ENABLE_LINK_ONLY);
	if (ret) {
		err("fail CSIS stream on(%d)", ret);
		return ret;
	}

	/* sensor */
	ret = v4l2_subdev_call(sd_mod, video, s_stream, true);
	if (ret) {
		err("fail sensor stream on(%d)", ret);
		return ret;
	}

	return 0;
}

static int sensor_stream_off(struct v4l2_subdev *sd_csi, struct v4l2_subdev *sd_mod)
{
	int ret;

	/* sensor */
	ret = v4l2_subdev_call(sd_mod, video, s_stream, false);
	if (ret) {
		err("fail sensor stream off(%d)", ret);
		return ret;
	}

	/* link */
	ret = v4l2_subdev_call(sd_csi, video, s_stream, IS_DISABLE_STREAM);
	if (ret) {
		err("fail CSIS stream off(%d)", ret);
		return ret;
	}

	return 0;
}

#define PHY_LANE_OFFSET		0x100
#define PHY_ADDR_SD_DESKEW_CON0	0x40
#define PHY_ADDR_SD_DESKEW_CON2	0x48
#define PHY_ADDR_SD_CRC_CON0	0x60

#define SKEW_CAL_CLK_START_POS	0
#define SKEW_CAL_DATA_START_POS	8

static u32 open_window;
static u32 total_err;
static u32 first_open;

static void __skew_code_sweep(u32 num_skew_code, u32 skew_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs,
			u32 start)
{
	struct phy_setfile sf_info;
	u32 i, lane;
	u32 tmp_open = 0, tmp_open_window = 0;
	u32 cont_err = 0, cont_end = 11; /* check continuous error */

	sf_info.addr = PHY_ADDR_SD_DESKEW_CON2;
	sf_info.start = start;
	sf_info.width = 5;

	for (i = 0; i < num_skew_code; i++) {
		sf_info.val = i;

		for (lane = 0; lane < max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		sensor_stream_on(sd_csi, sd_mod);
		fsleep(skew_test_time * 1000);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s[SKEW_CAL_%s_CODE: %d] s:%d, e:%d, err:%d\n", __func__,
			start ? "DATA" : "CLK", i, csi->state_cnt.str,
			csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			total_err += csi->state_cnt.err;
			cont_err++;
			tmp_open = 0;
		} else {
			cont_end = 2;
			if (csi->state_cnt.str && csi->state_cnt.end) {
				if (++tmp_open > tmp_open_window)
					tmp_open_window = tmp_open;

				cont_err = 0;
			} else {
				err("invalid state");
			}
		}

		if (cont_err >= cont_end)
			break;
	}

	if (start)
		first_open = tmp_open_window;

	open_window += tmp_open_window;

	sf_info.val = 0;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

static void skew_code_sweep(u32 num_skew_code, u32 skew_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs)
{
	struct phy_setfile sf_info;
	u32 lane;

	/* Enable SKEW_CAL */
	sf_info.addr = PHY_ADDR_SD_DESKEW_CON0;
	sf_info.start = 0;
	sf_info.width = 1;
	sf_info.val = 0;

	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);

	/* Sweep SKEW_CAL_DATA_CODE */
	__skew_code_sweep(num_skew_code, skew_test_time, max_lane, csi, sd_csi,
			sd_mod, regs, SKEW_CAL_DATA_START_POS);

	/* Sweep SKEW_CAL_CLK_CODE */
	__skew_code_sweep(num_skew_code, skew_test_time, max_lane, csi, sd_csi,
			sd_mod, regs, SKEW_CAL_CLK_START_POS);

	sf_info.val = 1;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

static void crc_code_sweep(u32 num_crc_code, u32 crc_test_time, u32 max_lane,
			struct is_device_csi *csi, struct v4l2_subdev *sd_csi,
			struct v4l2_subdev *sd_mod, void __iomem *regs)
{
	struct phy_setfile sf_info;
	u32 i, lane;
	u32 tmp_open = 0, tmp_first;
	u32 cont_err = 0, cont_end = 21; /* check continuous error */

	/* Sweep CRC_FORCE_CODE */
	sf_info.addr = PHY_ADDR_SD_CRC_CON0;
	sf_info.start = 0;
	sf_info.width = 10;
	for (i = 0; i < num_crc_code; i++) {
		sf_info.val = i << 4 | 0x1;

		for (lane = 0; lane < max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		sensor_stream_on(sd_csi, sd_mod);
		msleep(crc_test_time);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s[CRC_FORCE_CODE: %d] s:%d, e:%d, err:%d\n", __func__,
			i, csi->state_cnt.str, csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			total_err += csi->state_cnt.err;
			cont_err++;
			tmp_open = 0;
		} else {
			cont_end = 5;
			if (csi->state_cnt.str && csi->state_cnt.end) {
				if (!tmp_open)
					tmp_first = i;

				if (++tmp_open > open_window) {
					open_window = tmp_open;
					first_open = tmp_first;
				}

				cont_err = 0;
			} else {
				err("invalid state\n");
			}
		}

		if (cont_err >= cont_end)
			break;
	}

	sf_info.val = 0;
	for (lane = 0; lane < max_lane; lane++)
		update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
				sf_info.start, sf_info.width, sf_info.val);
}

static int pablo_test_set_phy_tune(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	struct phy_setfile sf_info;
	struct mipi_phy_desc *phy_desc;
	void __iomem *regs;
	struct is_device_csi *csi;
	struct v4l2_subdev *sd_csi, *sd_mod;
	u32 debug_phy_tune;
	u32 size, pre_arg_num;
	u32 num_test_code, test_time;
	u32 arg_idx = 0, tune_idx, i, lane, data_lane;
	u32 prev, next;
	const u32 elem_sz = sizeof(struct phy_setfile) / sizeof(u32);
	void (*run_code_sweep)(u32, u32, u32, struct is_device_csi*, struct v4l2_subdev*,
			struct v4l2_subdev*, void __iomem*);

	debug_phy_tune = is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE);
	if (!debug_phy_tune) {
		err("Turn on debug_phy_tune! # echo 1 > debug_phy_tune");
		return -EINVAL;
	}

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	pre_arg_num = argc % elem_sz;

	/* default value */
	if (is_get_debug_param(IS_DEBUG_PARAM_PHY_TUNE) == PABLO_PHY_TUNE_DPHY) {
		run_code_sweep = skew_code_sweep;
		num_test_code = 26;
	} else {
		run_code_sweep = crc_code_sweep;
		num_test_code = 48;
	}

	test_time = 100; /* msecs */

	if (pre_arg_num == 3) {
		ret = kstrtouint(argv[arg_idx++], 0, &num_test_code);
		if (ret) {
			err("Invalid num_test_code %d ret %d", num_test_code, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[arg_idx++], 0, &test_time);
		if (ret) {
			err("Invalid test_time %d ret %d", test_time, ret);
			goto func_exit;
		}
	} else if (pre_arg_num != 1) {
		err("invalid num of parameters. %d", argc);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &size);
	if (ret) {
		err("Invalid size %d ret %d", size, ret);
		goto func_exit;
	}

	if (argc < (size * elem_sz + arg_idx)) {
		err("Not enough parameters. %d < %d", argc, size * elem_sz + 1);
		goto func_exit;
	}

	if (!sensor) {
		err("sensor is NULL");
		goto func_exit;
	}

	sd_csi = sensor->subdev_csi;
	sd_mod = sensor->subdev_module;

	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	phy_desc = phy_get_drvdata(csi->phy);

	sensor_stream_off(sd_csi, sd_mod);

	data_lane = csi->sensor_cfg->lanes + 1;

	/* Set phy tune value */
	for (i = 0; i < size; i++) {
		tune_idx = i * elem_sz + arg_idx;
		ret = kstrtouint(argv[tune_idx++], 0, &sf_info.addr);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.start);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.width);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.val);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.index);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.max_lane);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto invalid_param;
		}

		if (sf_info.index == IDX_CLK_VAL) {
			regs = phy_desc->regs;
		} else {
			regs = phy_desc->regs_lane;
			data_lane = sf_info.max_lane;
		}

		/* check parameter validation */
		if ((sf_info.addr >= PHY_LANE_OFFSET * 2)
			|| (sf_info.start + sf_info.width >= BITS_PER_TYPE(u32))
			|| (sf_info.max_lane > CSI_DATA_LANES_MAX)) {
			err("%s: invalid param(addr(0x%X),start(%d),width(%d),max_lane(%d)",
				__func__, sf_info.addr, sf_info.start,
				sf_info.width, sf_info.max_lane);
			goto invalid_param;
		}

		prev = readl(regs + sf_info.addr);

		for (lane = 0; lane < sf_info.max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		next = readl(regs + sf_info.addr);

		info("%s (0x%04X: 0x%08X -> 0x%08X)\n", __func__,
			sf_info.addr, prev, next);
	}

	open_window = 0;
	total_err = 0;
	first_open = 0;

	if (!num_test_code) {
		sensor_stream_on(sd_csi, sd_mod);
		msleep(test_time);
		sensor_stream_off(sd_csi, sd_mod);

		info("%s s:%d, e:%d, err:%d\n", __func__,
			csi->state_cnt.str, csi->state_cnt.end, csi->state_cnt.err);

		if (csi->state_cnt.err) {
			open_window = 0;
		} else {
			if (csi->state_cnt.str && csi->state_cnt.end)
				open_window = 1;
			else
				err("invalid state\n");
		}
	} else {
		run_code_sweep(num_test_code, test_time, data_lane, csi, sd_csi, sd_mod, phy_desc->regs_lane);
	}

invalid_param:
func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_test_get_phy_tune(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "%d %d %d\n", open_window, total_err, first_open);

	return ret;
}

static int pablo_test_set_rt_phy_tune(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	struct phy_setfile sf_info;
	struct mipi_phy_desc *phy_desc;
	void __iomem *regs;
	struct is_device_csi *csi;
	struct v4l2_subdev *sd_csi, *sd_mod;
	u32 sensor_id, size;
	u32 arg_idx = 0, tune_idx, i, lane;
	u32 prev, next;
	const u32 elem_sz = sizeof(struct phy_setfile) / sizeof(u32);
	struct is_device_sensor *ids;
	struct is_core *core;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &sensor_id);
	if (ret) {
		err("Invalid sensor_id %d ret %d", sensor_id, ret);
		goto func_exit;
	}

	if (sensor_id >= IS_SENSOR_COUNT) {
		err("Invalid sensor_id %d", sensor_id);
		goto func_exit;
	}

	core = is_get_is_core();

	ids = &core->sensor[sensor_id];
	if (!test_bit(IS_SENSOR_FRONT_START, &ids->state)) {
		err("sensor is not in stream on state");
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_idx++], 0, &size);
	if (ret) {
		err("Invalid size %d ret %d", size, ret);
		goto func_exit;
	}

	if (argc < (size * elem_sz + arg_idx)) {
		err("Not enough parameters. %d < %d", argc, size * elem_sz + 1);
		goto func_exit;
	}

	sd_csi = ids->subdev_csi;
	sd_mod = ids->subdev_module;

	csi = v4l2_get_subdevdata(ids->subdev_csi);
	phy_desc = phy_get_drvdata(csi->phy);

	/* Set phy tune value */
	for (i = 0; i < size; i++) {
		tune_idx = i * elem_sz + arg_idx;
		ret = kstrtouint(argv[tune_idx++], 0, &sf_info.addr);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.start);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.width);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.val);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.index);
		ret |= kstrtouint(argv[tune_idx++], 0, &sf_info.max_lane);
		if (ret) {
			err("Invalid parameters(ret %d)", ret);
			goto func_exit;
		}

		if (sf_info.index == IDX_CLK_VAL)
			regs = phy_desc->regs;
		else
			regs = phy_desc->regs_lane;

		/* check parameter validation */
		if ((sf_info.addr >= PHY_LANE_OFFSET * 2)
		    || (sf_info.start + sf_info.width >= BITS_PER_TYPE(u32))
		    || (sf_info.max_lane > CSI_DATA_LANES_MAX)) {
			err("%s: invalid param(addr(0x%X),start(%d),width(%d),max_lane(%d)",
				__func__, sf_info.addr, sf_info.start,
				sf_info.width, sf_info.max_lane);
			goto func_exit;
		}

		prev = readl(regs + sf_info.addr);

		for (lane = 0; lane < sf_info.max_lane; lane++)
			update_bits(regs + (lane * PHY_LANE_OFFSET) + sf_info.addr,
					sf_info.start, sf_info.width, sf_info.val);

		next = readl(regs + sf_info.addr);

		info("%s (0x%04X: 0x%08X -> 0x%08X)\n", __func__,
			sf_info.addr, prev, next);
	}

func_exit:
	argv_free(argv);
	return ret;
}

/*
 * @for AT-command
 */
int test_phy_margin_check(char *buffer, int cam_index)
{
	int ret;
	char const *phy_tune_default_mode;
	char sensor_run_args[20] = {0, };
	char phy_tune_args[10] = {0, };
	char tmpChar[5] = {0, };
	u32 position = cam_index + SP_REAR;
	u32 i, device_id, rsc_type;
	struct is_core *core;
	struct is_module_enum *module;
	struct device_node *dnode;
	struct device *dev;
	struct is_device_sensor device;

	core = is_get_is_core();
	if (!core) {
		err("core is NULL");
		return -ENODEV;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		ret = is_search_sensor_module_with_position(&core->sensor[i],
							position, &module);
		if (!ret) {
			device_id = i;
			info("%s: pos(%d) dev(%d)", __func__, position, device_id);
			break;
		}
	}

	if (ret) {
		err("position(%d) is not supported.", position);
		return -ENODEV;
	}

	device = core->sensor[device_id];
	rsc_type = device_id + RESOURCE_TYPE_SENSOR0;

	ret = is_resource_open(&core->resourcemgr, rsc_type, (void **)&sensor);
	if (ret) {
		err("fail is_resource_open(%d)", ret);
		return ret;
	}

	dev = &sensor->pdev->dev;
	dnode = dev->of_node;

	ret = of_property_read_string(dnode, "phy_tune_default_mode", &phy_tune_default_mode);
	if (ret) {
		err("phy_tune_default_mode read fail!!");
		return ret;
	}

	if (device.pdata->use_cphy)
		is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 1);
	else
		is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 2);

	strcpy(sensor_run_args, "1"); // action : start=1, stop=0
	sprintf(tmpChar, " %d ", position);
	strncat(sensor_run_args, tmpChar, strlen(tmpChar)); // camera position
	strncat(sensor_run_args, phy_tune_default_mode, strlen(phy_tune_default_mode)); // camera default mode
	pablo_test_set_sensor_run(sensor_run_args, NULL);

	if (device.pdata->use_cphy)
		strcpy(phy_tune_args, "32 100 0"); // default phy tune command
	else
		strcpy(phy_tune_args, "12 100 0"); // dphy tune command

	info("%s: sensor_run_args : %s, phy_tune_args : %s", __func__, sensor_run_args, phy_tune_args);
	pablo_test_set_phy_tune(phy_tune_args, NULL);

	sprintf(buffer, "%d,%d,%d", open_window, total_err, first_open);

	sensor_self_stop();

	is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 0);

	return 0;
}
