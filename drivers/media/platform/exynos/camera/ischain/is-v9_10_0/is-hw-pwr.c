// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "is-core.h"
#include "is-config.h"
#include "is-type.h"
#include "is-device-sensor.h"
#include "is-device-ischain.h"

/* sensor */
int is_sensor_runtime_resume_pre(struct device *dev)
{
#ifndef CONFIG_PM

#endif
	return 0;
}

int is_sensor_runtime_suspend_pre(struct device *dev)
{
	return 0;
}

/* ischain */
int is_ischain_runtime_resume_pre(struct device *dev)
{
	return 0;
}

int is_ischain_runtime_resume_post(struct device *dev)
{
	struct is_device_ischain *device;

	device = (struct is_device_ischain *)dev_get_drvdata(dev);

	info("%s\n", __func__);

	is_hw_ischain_enable(device);

	return 0;
}

int is_ischain_runtime_suspend_post(struct device *dev)
{
	struct is_device_ischain *device;

	device = (struct is_device_ischain *)dev_get_drvdata(dev);

	info("%s\n", __func__);

	is_hw_ischain_disable(device);

	return 0;
}

#if defined(USE_OFFLINE_PROCESSING)
int is_runtime_suspend_pre(struct device *dev)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)dev_get_drvdata(is_dev);
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	FIMC_BUG(!core);

	device = &core->sensor[0];

	subdev_csi = device->subdev_csi;
	if (!subdev_csi) {
		err("subdev_csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, core, s_power, 0);
	if (ret) {
		err("v4l2_csi_call(s_power) is fail(%d)", ret);
		goto p_err;
	}

p_err:
	info("%s(): %d\n", __func__, ret);
	return ret;
}

int is_runtime_resume_post(struct device *dev)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)dev_get_drvdata(is_dev);
	struct is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	FIMC_BUG(!core);

	device = &core->sensor[0];

	subdev_csi = device->subdev_csi;
	if (!subdev_csi) {
		err("subdev_csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, core, s_power, 1);
	if (ret) {
		err("v4l2_csi_call(s_power) is fail(%d)", ret);
		goto p_err;
	}

p_err:
	info("%s(): %d\n", __func__, ret);
	return ret;
}
#endif

int is_runtime_suspend_post(struct device *dev)
{
	return 0;
}
