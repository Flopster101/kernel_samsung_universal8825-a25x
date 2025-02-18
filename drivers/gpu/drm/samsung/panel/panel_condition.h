/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_CONDITION_H__
#define __PANEL_CONDITION_H__
#include "panel_obj.h"
#include "panel_expression.h"

struct panel_device;

struct cond_rule {
	struct panel_expr_data *item;
	unsigned int num_item;
	struct panel_expr_node *root;
};

struct condinfo {
	struct pnobj base;
	struct cond_rule rule;
};

#define MAX_COND_RULE_ITEM (8)

struct condinfo_container {
	struct panel_expr_data item[MAX_COND_RULE_ITEM];
	unsigned int num_item;
	struct condinfo cond;
};

extern struct condinfo panel_cond_else;
extern struct condinfo panel_cond_endif;

#define CONDINFO_CONTAINER(_condname) PN_CONCAT(cond_info, _condname)
#define CONDINFO_IF(_condname) (CONDINFO_CONTAINER(_condname).cond)
#define CONDINFO_EL(_condname) (panel_cond_else)
#define CONDINFO_FI(_condname) (panel_cond_endif)

#define COND_RULE_MASK_ALL (0xFFFFFFFF)

#define __COND_RULE_ITEM_RULE_INITIALIZER(_propname, _operator, _value, _mask) \
	PANEL_EXPR_OPERATOR(FROM), \
	PANEL_EXPR_OPERAND_PROP(_propname), \
	PANEL_EXPR_OPERATOR(BIT_AND), \
	PANEL_EXPR_OPERAND_U32(_mask), \
	PANEL_EXPR_OPERATOR(TO), \
	PANEL_EXPR_OPERATOR(_operator), \
	PANEL_EXPR_OPERAND_U32(_value)

#define __COND_RULE_ITEM_FUNC_INITIALIZER(_func) \
	PANEL_EXPR_OPERAND_FUNC(_func) \

#define DEFINE_CONDITION(_condname, _item) \
struct condinfo_container PN_CONCAT(cond_info, _condname) = { \
	.cond = { \
		.base = __PNOBJ_INITIALIZER(_condname, CMD_TYPE_COND_IF), \
		.rule = { \
			.item = (_item), .num_item = ARRAY_SIZE(_item), \
		} \
	} \
}

#define DEFINE_FUNC_BASED_COND(_condname, _func) \
struct condinfo_container PN_CONCAT(cond_info, _condname) = { \
	.item = { __COND_RULE_ITEM_FUNC_INITIALIZER(_func) }, \
	.num_item = 1, \
	.cond = { \
		.base = __PNOBJ_INITIALIZER(_condname, CMD_TYPE_COND_IF), \
		.rule = { \
			.item = CONDINFO_CONTAINER(_condname).item, \
			.num_item = 1, \
		} \
	} \
}

#define DEFINE_RULE_BASED_COND(_condname, _propname, _comparator, _value) \
struct condinfo_container PN_CONCAT(cond_info, _condname) = { \
	.item = { __COND_RULE_ITEM_RULE_INITIALIZER(_propname, _comparator, _value, COND_RULE_MASK_ALL) }, \
	.num_item = 7, \
	.cond = { \
		.base = __PNOBJ_INITIALIZER(_condname, CMD_TYPE_COND_IF), \
		.rule = { \
			.item = CONDINFO_CONTAINER(_condname).item, \
			.num_item = 7, \
		} \
	} \
}

#define DEFINE_RULE_BASED_COND_WITH_MASK(_condname, _propname, _comparator, _value, _mask) \
struct condinfo_container PN_CONCAT(cond_info, _condname) = { \
	.item = { __COND_RULE_ITEM_RULE_INITIALIZER(_propname, _comparator, _value, _mask) }, \
	.num_item = 7, \
	.cond = { \
		.base = __PNOBJ_INITIALIZER(_condname, CMD_TYPE_COND_IF), \
		.rule = { \
			.item = CONDINFO_CONTAINER(_condname).item, \
			.num_item = 7, \
		} \
	} \
}

static inline unsigned int get_condition_type(struct condinfo *cond)
{
	return get_pnobj_cmd_type(&cond->base);
}

static inline char *get_condition_name(struct condinfo *cond)
{
	return get_pnobj_name(&cond->base);
}

struct condinfo *create_condition(char *name,
		unsigned int type, struct cond_rule *rule);
struct condinfo *duplicate_condition(struct condinfo *src);
void destroy_condition(struct condinfo *cond);
bool panel_do_condition(struct panel_device *panel, struct condinfo *info);

#endif /* __PANEL_CONDITION_H__ */
