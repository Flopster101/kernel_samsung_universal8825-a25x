// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"
#include "panel_expression.h"
#include "panel_condition.h"

struct condinfo panel_cond_else = {
	.base = __PNOBJ_INITIALIZER(panel_cond_else, CMD_TYPE_COND_EL),
};
EXPORT_SYMBOL(panel_cond_else);

struct condinfo panel_cond_endif = {
	.base = __PNOBJ_INITIALIZER(panel_cond_endif, CMD_TYPE_COND_FI),
};
EXPORT_SYMBOL(panel_cond_endif);

struct condinfo *create_condition(char *name,
		unsigned int type, struct cond_rule *rule)
{
	struct condinfo *cond;
	struct panel_expr_data *item = NULL;
	unsigned int i;

	if (!name) {
		panel_err("name is null\n");
		return NULL;
	}

	if (!IS_CMD_TYPE_COND(type)) {
		panel_err("invalid condition type(%d)\n", type);
		return NULL;
	}

	cond = kzalloc(sizeof(*cond), GFP_KERNEL);
	if (!cond)
		return NULL;

	pnobj_init(&cond->base, type, name);
	if (rule->item) {
		item = kzalloc(sizeof(*item) * rule->num_item, GFP_KERNEL);
		if (!item)
			goto err;

		for (i = 0; i < rule->num_item; i++) {
			memcpy(&item[i], &rule->item[i], sizeof(*item));
			if (rule->item[i].type == PANEL_EXPR_TYPE_OPERAND_PROP)
				item[i].op.str = kstrndup(rule->item[i].op.str,
						PANEL_PROP_NAME_LEN-1, GFP_KERNEL);
		}
	}

	cond->rule.item = item;
	cond->rule.num_item = rule->num_item;

	return cond;

err:
	kfree(cond);
	kfree(item);

	return NULL;
}

struct condinfo *duplicate_condition(struct condinfo *src)
{
	return create_condition(get_pnobj_name(&src->base),
			get_pnobj_cmd_type(&src->base),
			&src->rule);
}

void destroy_condition(struct condinfo *cond)
{
	unsigned int i;

	if (!cond)
		return;

	pnobj_deinit(&cond->base);
	for (i = 0; i < cond->rule.num_item; i++)
		if (cond->rule.item[i].type == PANEL_EXPR_TYPE_OPERAND_PROP)
			kfree(cond->rule.item[i].op.str);
	kfree(cond->rule.item);
	kfree(cond);
}

bool panel_do_condition(struct panel_device *panel, struct condinfo *cond)
{
	if (!panel) {
		panel_err("panel is null\n");
		return false;
	}

	if (!cond) {
		panel_err("condinfo is null\n");
		return false;
	}

	if (!cond->rule.root) {
		if (!cond->rule.item) {
			panel_err("panel expression is empty\n");
			return false;
		}

		cond->rule.root =
			panel_expr_from_infix(cond->rule.item, cond->rule.num_item);
		if (!cond->rule.root) {
			panel_err("failed to construct expreesion tree\n");
			return false;
		}
	}

	return panel_expr_eval(panel, cond->rule.root);
}

MODULE_DESCRIPTION("condition driver for panel");
MODULE_LICENSE("GPL v2");
