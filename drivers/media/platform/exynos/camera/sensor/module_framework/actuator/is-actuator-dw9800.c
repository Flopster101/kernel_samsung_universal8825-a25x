/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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

#include "is-actuator-dw9800.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#include "is-helper-i2c.h"

#include "interface/is-interface-library.h"
#define ACTUATOR_NAME		"DW9800"

#define REG_TARGET_HIGH		0x03
#define REG_TARGET_LOW		0x04

static int sensor_dw9800_write_position(struct i2c_client *client, u32 val)
{
	int ret = 0;
	u8 val_high = 0, val_low = 0;

	BUG_ON(!client);

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (val > DW9800_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					val, DW9800_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * If when use uSlewRate in driver show the example code
	 * val_low = ((val & 0x0F) << 4) | uSlewRate
	 */
	val_high = (val & 0x03FF) >> 8;
	val_low = (val & 0xFF);

	ret = is_sensor_addr8_write8(client, REG_TARGET_HIGH, val_high);
	ret = is_sensor_addr8_write8(client, REG_TARGET_LOW, val_low);

p_err:
	return ret;
}

int sensor_dw9800_actuator_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	u8 product_id = 0;
	struct is_actuator *actuator;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	ktime_t st = ktime_get();
#endif

	BUG_ON(!subdev);

	dbg_actuator("%s\n", __func__);


	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	if (!actuator) {
		err("actuator is not detect!\n");
		goto p_err;
	}

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(actuator->i2c_lock);

	ret = is_sensor_addr8_write8(client, 0x02, 0x02);
	if (ret < 0)
		goto p_err_mutex;

	I2C_MUTEX_UNLOCK(actuator->i2c_lock);

	usleep_range(5000, 5000);

	I2C_MUTEX_LOCK(actuator->i2c_lock);

	ret = is_sensor_addr8_write8(client, 0x06, 0x40);
	if (ret < 0)
		goto p_err_mutex;

	ret = is_sensor_addr8_write8(client, 0x07, 0x7A);
	if (ret < 0)
		goto p_err_mutex;

	ret = is_sensor_addr8_read8(client, 0x00, &product_id);
	dbg_actuator("%s: product_id(%#x)\n", __func__, product_id);
	if (ret < 0)
		goto p_err_mutex;

#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif

p_err_mutex:
	I2C_MUTEX_UNLOCK(actuator->i2c_lock);

p_err:
	return ret;
}

int sensor_dw9800_actuator_get_status(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	u8 val = 0;
	struct is_actuator *actuator = NULL;
	struct i2c_client *client = NULL;
#ifdef DEBUG_ACTUATOR_TIME
	ktime_t st = ktime_get();
#endif

	dbg_actuator("%s\n", __func__);

	BUG_ON(!subdev);
	BUG_ON(!info);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	BUG_ON(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * Byte1                     Byte2
	 * PD FL AG D9 D8 D7 D6 D5   D4 D3 D2 D1 D0 S3 S2 S1 S0
	 * PD : Power down mode. 1: Power down mode (active high), 0: Normal operation mode
	 * FL : Busy flag. FLAG must keep "L" at writing operation.
	 */
	//ret = is_sensor_addr8_read8(client, 0x05, &val);

	*info = (((val) & 0x01) == 0) ? ACTUATOR_STATUS_NO_BUSY : ACTUATOR_STATUS_BUSY;
#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif

p_err:
	return ret;
}

int sensor_dw9800_actuator_set_position(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct i2c_client *client;
	struct is_sysfs_actuator *sysfs_actuator;
	u32 position = 0;
#ifdef DEBUG_ACTUATOR_TIME
	ktime_t st = ktime_get();
#endif

	BUG_ON(!subdev);
	BUG_ON(!info);

	actuator = (struct is_actuator *)v4l2_get_subdevdata(subdev);
	BUG_ON(!actuator);

	client = actuator->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	I2C_MUTEX_LOCK(actuator->i2c_lock);
	position = *info;
	if (position > DW9800_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					position, DW9800_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err_mutex;
	}

	sysfs_actuator = is_get_sysfs_actuator();
	/* debug option : fixed position testing */
	if (sysfs_actuator->enable_fixed)
		position = sysfs_actuator->fixed_position;

	/* position Set */
	ret = sensor_dw9800_write_position(client, position);
	if (ret < 0)
		goto p_err_mutex;
	actuator->position = position;

	dbg_actuator("%s: position(%d)\n", __func__, position);

#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif
p_err_mutex:
  I2C_MUTEX_UNLOCK(actuator->i2c_lock);
p_err:
	return ret;
}

static int sensor_dw9800_actuator_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	u32 val = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = sensor_dw9800_actuator_get_status(subdev, &val);
		if (ret < 0) {
			err("err!!! ret(%d), actuator status(%d)", ret, val);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

	ctrl->value = val;

p_err:
	return ret;
}

static int sensor_dw9800_actuator_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = sensor_dw9800_actuator_set_position(subdev, &ctrl->value);
		if (ret) {
			err("failed to actuator set position: %d, (%d)\n", ctrl->value, ret);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

long sensor_dw9800_actuator_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_ACT_S_CTRL:
		ret = sensor_dw9800_actuator_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! actuator_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_ACT_G_CTRL:
		ret = sensor_dw9800_actuator_g_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! actuator_g_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown command(%#x)", cmd);
		ret = -EINVAL;
		goto p_err;
	}
p_err:
	return (long)ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_dw9800_actuator_init,
	.ioctl = sensor_dw9800_actuator_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int sensor_dw9800_actuator_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core = NULL;
	struct v4l2_subdev *subdev_actuator = NULL;
	struct is_actuator *actuator = NULL;
	struct is_device_sensor *device = NULL;
	u32 sensor_id = 0;
	u32 place = 0;
	struct device *dev;
	struct device_node *dnode;

	BUG_ON(!is_dev);
	BUG_ON(!client);

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		err("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	dev = &client->dev;
	dnode = dev->of_node;

	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "place", &place);
	if (ret) {
		pr_info("place read is fail(%d)", ret);
		place = 0;
	}
	probe_info("%s sensor_id(%d) actuator_place(%d)\n", __func__, sensor_id, place);

	device = &core->sensor[sensor_id];
	if (!test_bit(IS_SENSOR_PROBE, &device->state)) {
		err("sensor device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	actuator = kzalloc(sizeof(struct is_actuator), GFP_KERNEL);
	if (!actuator) {
		err("actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_actuator = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_actuator) {
		err("subdev_actuator is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* This name must is match to sensor_open_extended actuator name */
	actuator->id = ACTUATOR_NAME_DW9800;
	actuator->subdev = subdev_actuator;
	actuator->device = sensor_id;
	actuator->client = client;
	actuator->position = 0;
	actuator->max_position = DW9800_POS_MAX_SIZE;
	actuator->pos_size_bit = DW9800_POS_SIZE_BIT;
	actuator->pos_direction = DW9800_POS_DIRECTION;
	actuator->i2c_lock = NULL;
	actuator->need_softlanding = 0;
	actuator->actuator_ops = NULL;

	device->subdev_actuator[sensor_id] = subdev_actuator;
	device->actuator[sensor_id] = actuator;

	v4l2_i2c_subdev_init(subdev_actuator, client, &subdev_ops);
	v4l2_set_subdevdata(subdev_actuator, actuator);
	v4l2_set_subdev_hostdata(subdev_actuator, device);


	snprintf(subdev_actuator->name, V4L2_SUBDEV_NAME_SIZE, "actuator-subdev.%d", actuator->id);

	probe_info("%s done\n", __func__);
	return ret;

p_err:
	if (actuator)
		kzfree(actuator);

	if (subdev_actuator)
		kzfree(subdev_actuator);

	return ret;
}

static const struct of_device_id sensor_actuator_dw9800_match[] = {
	{
		.compatible = "samsung,exynos-is-actuator-dw9800",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_actuator_dw9800_match);

static const struct i2c_device_id sensor_actuator_dw9800_idt[] = {
	{ ACTUATOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_actuator_dw9800_driver = {
	.probe  = sensor_dw9800_actuator_probe,
	.driver = {
		.name	= ACTUATOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_actuator_dw9800_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_actuator_dw9800_idt,
};

#ifdef MODULE
builtin_i2c_driver(sensor_actuator_dw9800_driver);
#else
static int __init sensor_actuator_dw9800_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_actuator_dw9800_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_actuator_dw9800_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_actuator_dw9800_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
