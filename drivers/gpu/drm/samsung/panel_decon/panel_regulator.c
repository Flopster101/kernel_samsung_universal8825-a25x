// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <asm-generic/errno-base.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#include "panel_kunit.h"

#include "panel_regulator.h"
#include "panel_debug.h"

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_enable_wrapper, RETURNS(int), PARAMS(struct regulator *));
__visible_for_testing int REAL_ID(regulator_enable_wrapper)(struct regulator *regulator)
{
	return regulator_enable(regulator);
}

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_disable_wrapper, RETURNS(int), PARAMS(struct regulator *));
__visible_for_testing int REAL_ID(regulator_disable_wrapper)(struct regulator *regulator)
{
	return regulator_disable(regulator);
}

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_get_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *));
__visible_for_testing int REAL_ID(regulator_get_voltage_wrapper)(struct regulator *regulator)
{
	return regulator_get_voltage(regulator);
}

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_set_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));
__visible_for_testing int REAL_ID(regulator_set_voltage_wrapper)(struct regulator *regulator, int min_uV, int max_uV)
{
	return regulator_set_voltage(regulator, min_uV, max_uV);
}

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_get_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *));
__visible_for_testing int REAL_ID(regulator_get_current_limit_wrapper)(struct regulator *regulator)
{
	return regulator_get_current_limit(regulator);
}

__visible_for_testing DEFINE_REDIRECT_MOCKABLE(regulator_set_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));
__visible_for_testing int REAL_ID(regulator_set_current_limit_wrapper)(struct regulator *regulator, int min_uA, int max_uA)
{
	return regulator_set_current_limit(regulator, min_uA, max_uA);
}

static int panel_regulator_init(struct panel_regulator *regulator)
{
	if (!regulator)
		return -EINVAL;

	if (!regulator->name)
		return -EINVAL;

	if (!IS_ERR_OR_NULL(regulator->reg)) {
		panel_warn("%s already initialized\n", regulator->name);
		return 0;
	}

	regulator->reg = regulator_get(NULL, regulator->name);
	if (IS_ERR(regulator->reg)) {
		panel_err("failed to get regulator %s\n", regulator->name);
		return -ENODEV;
	}
	panel_info("initialize regulator(%s)\n", regulator->name);

	return 0;
}

static int panel_regulator_enable(struct panel_regulator *regulator)
{
	int ret;

	if (!regulator)
		return -EINVAL;

	if (IS_ERR_OR_NULL(regulator->reg))
		return -EINVAL;

	if (regulator->type != PANEL_REGULATOR_TYPE_PWR)
		return 0;

	ret = regulator_enable_wrapper(regulator->reg);
	if (ret) {
		panel_err("failed to enable regulator(%s), ret:%d\n",
				regulator->name, ret);
		return ret;
	}

	panel_info("enable regulator(%s)\n", regulator->name);

	return 0;
}

static int panel_regulator_disable(struct panel_regulator *regulator)
{
	int ret;

	if (!regulator)
		return -EINVAL;

	if (IS_ERR_OR_NULL(regulator->reg))
		return -EINVAL;

	if (regulator->type != PANEL_REGULATOR_TYPE_PWR)
		return 0;

	ret = regulator_disable_wrapper(regulator->reg);
	if (ret) {
		panel_err("failed to disable regulator(%s), ret:%d\n",
				regulator->name, ret);
		return ret;
	}

	panel_info("disable regulator(%s)\n", regulator->name);

	return 0;
}

static int panel_regulator_set_voltage(struct panel_regulator *regulator, int uV)
{
	int ret, cur_uV;

	if (!regulator)
		return -EINVAL;

	if (IS_ERR_OR_NULL(regulator->reg))
		return -EINVAL;

	if (regulator->type != PANEL_REGULATOR_TYPE_PWR)
		return 0;

	if (!uV)
		return 0;

	cur_uV = regulator_get_voltage_wrapper(regulator->reg);
	if (cur_uV < 0) {
		panel_err("failed to get regulator(%s) voltage, ret:%d\n",
				regulator->name, cur_uV);
		return cur_uV;
	}

	if (uV == cur_uV)
		return 0;

	ret = regulator_set_voltage_wrapper(regulator->reg, uV, uV);
	if (ret < 0) {
		panel_err("failed to set regulator(%s) voltage(%d), ret:%d\n",
				regulator->name, uV, ret);
		return ret;
	}

	panel_info("voltage: %duV->%duV\n", cur_uV, uV);

	return ret;
}

static int panel_regulator_set_current_limit(struct panel_regulator *regulator, int uA)
{
	int ret, cur_uA;

	if (!regulator)
		return -EINVAL;

	if (IS_ERR_OR_NULL(regulator->reg))
		return -EINVAL;

	if (regulator->type != PANEL_REGULATOR_TYPE_SSD)
		return 0;

	cur_uA = regulator_get_current_limit_wrapper(regulator->reg);
	if (cur_uA < 0) {
		panel_err("failed to get regulator(%s) current limitation, ret:%d\n",
				regulator->name, cur_uA);
		return cur_uA;
	}

	if (uA == cur_uA)
		return 0;

	ret = regulator_set_current_limit_wrapper(regulator->reg, uA, uA);
	if (ret < 0) {
		panel_err("failed to set regulator(%s) current limitation, ret:%d\n",
				regulator->name, uA);
		return ret;
	}

	panel_info("set regulator(%s) SSD:%duA->%duA\n",
			regulator->name, cur_uA, uA);

	return 0;
}

struct panel_regulator_funcs panel_regulator_funcs = {
	.init = panel_regulator_init,
	.enable = panel_regulator_enable,
	.disable = panel_regulator_disable,
	.set_voltage = panel_regulator_set_voltage,
	.set_current_limit = panel_regulator_set_current_limit,
};

int panel_regulator_helper_init(struct panel_regulator *regulator)
{
	if (!regulator)
		return -EINVAL;

	return call_panel_regulator_func(regulator, init);
}

int panel_regulator_helper_enable(struct panel_regulator *regulator)
{
	if (!regulator)
		return -EINVAL;

	return call_panel_regulator_func(regulator, enable);
}

int panel_regulator_helper_disable(struct panel_regulator *regulator)
{
	if (!regulator)
		return -EINVAL;

	return call_panel_regulator_func(regulator, disable);
}

int panel_regulator_helper_set_voltage(struct panel_regulator *regulator, int uV)
{
	if (!regulator)
		return -EINVAL;

	return call_panel_regulator_func(regulator, set_voltage, uV);
}

int panel_regulator_helper_set_current_limit(struct panel_regulator *regulator, int uA)
{
	if (!regulator)
		return -EINVAL;

	return call_panel_regulator_func(regulator, set_current_limit, uA);
}

int of_get_panel_regulator(struct device_node *np, struct panel_regulator *regulator)
{
	struct device_node *reg_np;

	reg_np = of_parse_phandle(np, "regulator", 0);
	if (!reg_np) {
		panel_err("%s:'regulator' node not found\n", np->name);
		return -EINVAL;
	}

	if (of_property_read_string(reg_np, "regulator-name",
				(const char **)&regulator->name)) {
		panel_err("%s:%s:property('regulator-name') not found\n",
				np->name, reg_np->name);
		of_node_put(reg_np);
		return -EINVAL;
	}
	of_node_put(reg_np);
	of_property_read_u32(np, "type", &regulator->type);
	if (regulator->type >= PANEL_REGULATOR_TYPE_MAX) {
		panel_err("%s invalid type %d\n", np->name, regulator->type);
		return -EINVAL;
	}
	regulator->node_name = np->name;
	regulator->funcs = &panel_regulator_funcs;

	return 0;
}
EXPORT_SYMBOL(of_get_panel_regulator);

struct panel_regulator *panel_regulator_create(void)
{
	struct panel_regulator *regulator;

	regulator = kzalloc(sizeof(struct panel_regulator), GFP_KERNEL);
	if (!regulator)
		return NULL;

	regulator->funcs = &panel_regulator_funcs;

	return regulator;
}
EXPORT_SYMBOL(panel_regulator_create);

void panel_regulator_destroy(struct panel_regulator *regulator)
{
	if (!regulator)
		return;

	regulator_put(regulator->reg);
	kfree(regulator);
}
EXPORT_SYMBOL(panel_regulator_destroy);
