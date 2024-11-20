/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_I2C_H__
#define __PANEL_I2C_H__
#include <linux/i2c.h>
#include <linux/of_device.h>
#include "panel.h"


#define PANEL_I2C_MAX (4)


struct panel_i2c_dev {
	struct i2c_client *client;
	struct i2c_drv_ops *ops;
	struct mutex lock;

	int addr_len;
	int data_len;

	unsigned short reg;	/* slave addr*/

	int id;

	bool use;
};

struct i2c_drv_ops {
	int (*tx)(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);
	int (*rx)(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);
	int (*transfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
};


int panel_i2c_drv_probe(struct panel_device *panel);
#endif /* __PANEL_I2C_H__ */
