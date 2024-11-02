/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>

#include <exynos-is-sensor.h>
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#include "interface/is-interface-library.h"

#include <linux/laser_tof8801.h>

#define LASER_AF_NAME		"LASER-AF-TOF8801"

int sensor_tof8801_laser_af_resume(struct v4l2_subdev *subdev)
{
	int ret;

	ret = tof8801_resume();
	if (ret)
		err("failed to resume tof8801 laser");

	return ret;
}

int sensor_tof8801_laser_af_suspend(struct v4l2_subdev *subdev)
{
	int ret;

	ret = tof8801_suspend();
	if (ret)
		err("failed to suspend tof8801 laser");

	return ret;
}

int sensor_tof8801_laser_af_get_distance_info(struct v4l2_subdev *subdev, u32 *distance_mm,
				u32 *confidence)
{
	int ret = 0;
	ret = tof8801_get_distance(distance_mm, confidence);
	*confidence *= 16; /* x16 for RTA */
	dbg_sensor(1, "%s, [distance: 0x%04x][confidence: 0x%02x]", __func__, *distance_mm, *confidence);

	return ret;

}

static const struct v4l2_subdev_ops subdev_ops;

static struct is_laser_af_ops laser_af_ops = {
	.resume = sensor_tof8801_laser_af_resume,
	.suspend = sensor_tof8801_laser_af_suspend,
	.get_distance_info = sensor_tof8801_laser_af_get_distance_info,
};

static int __init laser_af_tof8801_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_laser_af;
	struct is_device_sensor *device;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_laser_af *laser_af;
	u32 sensor_id = 0;
	struct device_node *dnode;

	FIMC_BUG(!is_dev);
	FIMC_BUG(!dev);

	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	device = &core->sensor[sensor_id];
	if (!test_bit(IS_SENSOR_PROBE, &device->state)) {
		err("sensor device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_peri = find_peri_by_laser_af_id(device, LASER_AF_NAME_TOF8801);
	if (!sensor_peri) {
		probe_info("sensor peri is net yet probed");
		return -EPROBE_DEFER;
	}

	laser_af = kzalloc(sizeof(struct is_laser_af), GFP_KERNEL);
	if (!laser_af) {
		err("laser is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_laser_af = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_laser_af) {
		err("subdev_laser is NULL");
		ret = -ENOMEM;
		kfree(laser_af);
		goto p_err;
	}

	sensor_peri->subdev_laser_af = subdev_laser_af;

	laser_af->id = LASER_AF_NAME_TOF8801;
	laser_af->subdev = subdev_laser_af;
	laser_af->client = client;

	laser_af->laser_af_data.distance = 0;
	laser_af->laser_af_data.confidence = 0;

	laser_af->laser_af_ops = &laser_af_ops;

	device->subdev_laser_af = subdev_laser_af;
	device->laser_af = laser_af;

	v4l2_subdev_init(subdev_laser_af, &subdev_ops);

	v4l2_set_subdevdata(subdev_laser_af, laser_af);
	v4l2_set_subdev_hostdata(subdev_laser_af, device);
	snprintf(subdev_laser_af->name, V4L2_SUBDEV_NAME_SIZE, "laser-af-subdev.%d", laser_af->id);

p_err:
	return ret;
}

static int __init laser_af_tof8801_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	FIMC_BUG(!pdev);

	dev = &pdev->dev;

	ret = laser_af_tof8801_probe(dev, NULL);
	if (ret < 0) {
		probe_err("laser tof8801 probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id sensor_laser_af_tof8801_match[] = {
	{
		.compatible = "samsung,sensor-laser-af-tof8801",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_laser_af_tof8801_match);

/* register platform driver */
static struct platform_driver sensor_laser_af_tof8801_platform_driver = {
	.driver = {
		.name   = LASER_AF_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = sensor_laser_af_tof8801_match,
	}
};

static int __init sensor_laser_af_tof8801_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_laser_af_tof8801_platform_driver,
				laser_af_tof8801_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_laser_af_tof8801_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_laser_af_tof8801_init);
