/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include "is-actuator-dw9780.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"
#include "is-helper-i2c.h"

#include "interface/is-interface-library.h"

#define ACTUATOR_NAME		"DW9780"

#define DW9780_FIRST_POSITION		100
#define DW9780_SECOND_POSITION		170
#define DW9780_DELAY			(2*1000) /* us */

int sensor_dw9780_actuator_mode_select(struct i2c_client *client, u32 mode)
{
	int ret = 0;
	u16 addr, val, mode_sel;

	switch (mode) {
	case 0: /* Calibration mode */
		mode_sel = 0x4000;
		break;
	case 1: /* OIS mode */
		mode_sel = 0x8000;
		break;
	default:
		mode_sel = 0x0000;
		break;
	}

	/* 1) Set mode */
	addr = DW9780_REG_OP_MODE;
	val = mode_sel;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	/* 2) Set OLAF mode */
	addr = DW9780_REG_OLAF_ACT;
	val = 0x001B; /* OLAF enable | SAC 3.5 */
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	/* 3) Operation start enable */
	addr = DW9780_REG_CMD_UPD;
	val = 0x1000;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}
p_err:

	return ret;
}

int sensor_dw9780_actuator_mode_start(struct i2c_client *client, u32 mode)
{
	int ret = 0;
	u16 addr, val;

	sensor_dw9780_actuator_mode_select(client, 2); /* select OIS mode */

	switch (mode) {
	case 0: /* OIS on, Servo On */
		val = 0x0000;
		break;
	case 1: /* OIS off, Servo On */
		val = 0x0001;
		break;
	case 2: /* OIS off, Servo Off */
		val = 0x0002;
		break;
	default:
		goto p_err;
		break;
	}

	addr = DW9780_REG_OIS_CON;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

p_err:

	return ret;
}

int sensor_dw9780_init(struct i2c_client *client, struct is_caldata_list_dw9780 *cal_data)
{
	int ret = 0;
	u16 addr, val;

	/* 1) Chip disable & DSP disable */
	addr = DW9780_REG_CHIP_ACT;
	val = 0x0001;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	/* 2) Chip enable & DSP disable */
	addr = DW9780_REG_CHIP_ACT;
	val = 0x0000;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	/* 3) Wait (2ms) */
	usleep_range(DW9780_DELAY, DW9780_DELAY);

	/* 4) Chip enable & DSP enable */
	addr = DW9780_REG_CHIP_ACT;
	val = 0x0010;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	/* 5) User protection release */
	addr = DW9780_REG_USER_CONTROL_PROTECTION;
	val = 0x56FA;
	ret = is_sensor_write16(client, addr, val);
	if (ret < 0) {
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", addr, val, ret);
		goto p_err;
	}

	ret = sensor_dw9780_actuator_mode_start(client, 2);
p_err:

	return ret;
}

static int sensor_dw9780_write_position(struct i2c_client *client, u32 val)
{
	int ret = 0;

	BUG_ON(!client);

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	if (val > DW9780_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					val, DW9780_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_write16(client, DW9780_REG_OLAF_TARGET, val);

p_err:
	return ret;
}

int sensor_dw9780_actuator_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct is_caldata_list_dw9780 *cal_data = NULL;
	struct i2c_client *client = NULL;
	struct is_lib_support *lib = is_get_lib_support();
	long cal_addr;
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

	/* EEPROM AF calData address */
	cal_addr = lib->minfo->kvaddr_cal[SENSOR_POSITION_REAR] + EEPROM_OEM_BASE;
	cal_data = (struct is_caldata_list_dw9780 *)(cal_addr);

	/* Read into EEPROM data or default setting */
	ret = sensor_dw9780_init(client, cal_data);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif

p_err:
	return ret;
}

int sensor_dw9780_actuator_get_status(struct v4l2_subdev *subdev, u32 *info)
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

	/* If data is 1 of 0x1 and 0x2 bit, will have to actuator not move */
	*info = ((val & 0x3) == 0) ? ACTUATOR_STATUS_NO_BUSY : ACTUATOR_STATUS_BUSY;

#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif

p_err:
	return ret;
}

int sensor_dw9780_actuator_set_position(struct v4l2_subdev *subdev, u32 *info)
{
	int ret = 0;
	struct is_actuator *actuator;
	struct i2c_client *client;
	u32 position = 0;
	struct is_sysfs_actuator *sysfs_actuator = is_get_sysfs_actuator();
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

	position = *info;
	if (position > DW9780_POS_MAX_SIZE) {
		err("Invalid af position(position : %d, Max : %d).\n",
					position, DW9780_POS_MAX_SIZE);
		ret = -EINVAL;
		goto p_err;
	}

	/* debug option : fixed position testing */
	if (sysfs_actuator.enable_fixed)
		position = sysfs_actuator.fixed_position;

	/* position Set */
	ret = sensor_dw9780_write_position(client, position);
	if (ret <0)
		goto p_err;
	actuator->position = position;

	dbg_actuator("%s: position(%d)\n", __func__, position);

#ifdef DEBUG_ACTUATOR_TIME
	pr_info("[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
#endif
p_err:
	return ret;
}

static int sensor_dw9780_actuator_g_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	u32 val = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_GET_STATUS:
		ret = sensor_dw9780_actuator_get_status(subdev, &val);
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

static int sensor_dw9780_actuator_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;

	switch(ctrl->id) {
	case V4L2_CID_ACTUATOR_SET_POSITION:
		ret = sensor_dw9780_actuator_set_position(subdev, &ctrl->value);
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

long sensor_dw9780_actuator_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct v4l2_control *ctrl;

	ctrl = (struct v4l2_control *)arg;
	switch (cmd) {
	case SENSOR_IOCTL_ACT_S_CTRL:
		ret = sensor_dw9780_actuator_s_ctrl(subdev, ctrl);
		if (ret) {
			err("err!!! actuator_s_ctrl failed(%d)", ret);
			goto p_err;
		}
		break;
	case SENSOR_IOCTL_ACT_G_CTRL:
		ret = sensor_dw9780_actuator_g_ctrl(subdev, ctrl);
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
	.init = sensor_dw9780_actuator_init,
	.ioctl = sensor_dw9780_actuator_ioctl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int sensor_dw9780_actuator_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_core *core= NULL;
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
	actuator->id = ACTUATOR_NAME_DW9780;
	actuator->subdev = subdev_actuator;
	actuator->device = sensor_id;
	actuator->client = client;
	actuator->position = 0;
	actuator->max_position = DW9780_POS_MAX_SIZE;
	actuator->pos_size_bit = DW9780_POS_SIZE_BIT;
	actuator->pos_direction = DW9780_POS_DIRECTION;
	actuator->i2c_lock = NULL;
	actuator->need_softlanding = 0;
	actuator->actuator_ops = NULL;

	device->subdev_actuator[place] = subdev_actuator;
	device->actuator[place] = actuator;

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

static const struct of_device_id sensor_actuator_dw9780_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-actuator-dw9780",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_actuator_dw9780_match);

static const struct i2c_device_id sensor_actuator_dw9780_idt[] = {
	{ ACTUATOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_actuator_dw9780_driver = {
	.probe  = sensor_dw9780_actuator_probe,
	.driver = {
		.name	= ACTUATOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_actuator_dw9780_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_actuator_dw9780_idt,
};

static int __init sensor_actuator_dw9780_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_actuator_dw9780_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_actuator_dw9780_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_actuator_dw9780_init);

MODULE_LICENSE("GPL");
