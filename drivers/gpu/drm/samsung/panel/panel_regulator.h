/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_REGULATOR_H__
#define __PANEL_REGULATOR_H__

#include <linux/regulator/consumer.h>
#include "panel_kunit.h"
#include <dt-bindings/display/panel-display.h>

#define call_panel_regulator_func(q, _f, args...) \
    (((q) && (q)->funcs && (q)->funcs->_f) ? ((q)->funcs->_f(q, ##args)) : 0)

struct panel_regulator_funcs;

enum {
	PANEL_REGULATOR_TYPE_NONE = REGULATOR_TYPE_NONE,
	PANEL_REGULATOR_TYPE_PWR = REGULATOR_TYPE_PWR,
	PANEL_REGULATOR_TYPE_SSD = REGULATOR_TYPE_SSD,
	PANEL_REGULATOR_TYPE_MAX,
};

struct panel_regulator {
	const char *name;
	const char *node_name;
	struct regulator *reg;
	u32 type;
	struct panel_regulator_funcs *funcs;
	struct list_head head;
	struct mutex lock;
	bool enabled;
};

struct panel_regulator_funcs {
	int (*init)(struct panel_regulator *regulator);
	int (*enable)(struct panel_regulator *regulator);
	int (*disable)(struct panel_regulator *regulator);
	int (*set_voltage)(struct panel_regulator *regulator, int uV);
	int (*set_current_limit)(struct panel_regulator *regulator, int uA);
	int (*force_disable)(struct panel_regulator *regulator);
};

int panel_regulator_helper_init(struct panel_regulator *regulator);
int panel_regulator_helper_enable(struct panel_regulator *regulator);
int panel_regulator_helper_disable(struct panel_regulator *regulator);
int panel_regulator_helper_set_voltage(struct panel_regulator *regulator, int uV);
int panel_regulator_helper_set_current_limit(struct panel_regulator *regulator, int uA);
int of_get_panel_regulator(struct device_node *np, struct panel_regulator *regulator);
int panel_regulator_helper_force_disable(struct panel_regulator *regulator);
struct panel_regulator *panel_regulator_create(void);
void panel_regulator_destroy(struct panel_regulator *regulator);

#endif /* __PANEL_REGULATOR_H__ */
