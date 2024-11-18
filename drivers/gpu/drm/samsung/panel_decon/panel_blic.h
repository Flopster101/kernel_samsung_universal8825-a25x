/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_BLIC_H__
#define __PANEL_BLIC_H__
#include <linux/i2c.h>
#include <linux/of_device.h>
#include "panel.h"

#define PANEL_BLIC_MAX (4)

enum panel_blic_gpio_list {
	PANEL_BLIC_GPIO_HWEN,
	PANEL_BLIC_GPIO_VSP,
	PANEL_BLIC_GPIO_VSN,
	MAX_PANEL_BLIC_GPIO,
};

enum panel_blic_seq {
	PANEL_BLIC_I2C_ON_SEQ,
	PANEL_BLIC_I2C_OFF_SEQ,
	MAX_PANEL_BLIC_SEQ,
};

struct blic_i2c_seq {
	u8 *cmd;
	int size;
};

struct blic_data {
	char *name;
	struct seqinfo *seqtbl;
};

struct panel_blic_gpio {
	int gpio_num;
	int delay_before;
	int delay_after;
};

struct panel_blic_dev;

struct panel_blic_ops {
	int (*do_seq)(struct panel_blic_dev *blic, enum panel_blic_seq index);
	int (*execute_power_ctrl)(struct panel_blic_dev *blic, const char *name);
};

struct panel_blic_dev {
	struct device *dev;
	const char *name;
	const char *regulator_name;
	const char *regulator_desc_name;
	struct device_node *np;
	struct device_node *regulator_node;
	bool regulator_boot_on;

	u32 i2c_match[3];
	u32 i2c_reg;
#ifdef CONFIG_MCD_PANEL_I2C
	struct panel_i2c_dev *i2c_dev;
#endif
	struct panel_blic_gpio gpios[MAX_PANEL_BLIC_GPIO];
	struct list_head gpio_list;

	struct panel_blic_ops *ops;

	int id;

	bool enable;
};

int panel_blic_probe(struct panel_device *panel);
struct panel_gpio *panel_blic_gpio_list_find_by_name(struct panel_blic_dev *blic_dev, const char *name);
bool panel_blic_power_control_exists(struct panel_blic_dev *blic, const char *name);
int panel_blic_power_control_execute(struct panel_blic_dev *blic, const char *name);

#endif /* __PANEL_BLIC_H__ */
