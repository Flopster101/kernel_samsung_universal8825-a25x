/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_EXPRESSION_H__
#define __PANEL_EXPRESSION_H__
#include "panel_obj.h"

struct panel_device;

typedef bool (*expr_func_t)(struct panel_device *panel);

#define call_expr_func(_expr, _arg1) \
	(((_expr) && (_expr)->op.func && (_expr)->op.func->symaddr) ? \
	  ((expr_func_t)(_expr)->op.func->symaddr)(_arg1) : 0)

enum {
	/* operand */
	PANEL_EXPR_TYPE_OPERAND_U32,
	PANEL_EXPR_TYPE_OPERAND_S32,
	PANEL_EXPR_TYPE_OPERAND_STR,
	PANEL_EXPR_TYPE_OPERAND_PROP,
	PANEL_EXPR_TYPE_OPERAND_FUNC,

	/* operator */
	PANEL_EXPR_TYPE_OPERATOR_FROM,
	PANEL_EXPR_TYPE_OPERATOR_TO,
	/* logical not operator: ! */
	PANEL_EXPR_TYPE_OPERATOR_NOT,
	/* arithmetic operator: % */
	PANEL_EXPR_TYPE_OPERATOR_MOD,
	/* relational operator: <, <=, >, >= */
	PANEL_EXPR_TYPE_OPERATOR_LT,
	PANEL_EXPR_TYPE_OPERATOR_GT,
	PANEL_EXPR_TYPE_OPERATOR_LE,
	PANEL_EXPR_TYPE_OPERATOR_GE,
	/* relational operator: ==, != */
	PANEL_EXPR_TYPE_OPERATOR_EQ,
	PANEL_EXPR_TYPE_OPERATOR_NE,
	/* bitwise operator &, | */
	PANEL_EXPR_TYPE_OPERATOR_BIT_AND,
	PANEL_EXPR_TYPE_OPERATOR_BIT_OR,
	/* logical operator &&, || */
	PANEL_EXPR_TYPE_OPERATOR_AND,
	PANEL_EXPR_TYPE_OPERATOR_OR,
	MAX_PANEL_EXPR_TYPE,
};

const char *panel_expr_type_enum_to_string(unsigned int type);
int panel_expr_type_string_to_enum(char *str);

#define IS_PANEL_EXPR_OPERAND(_type) \
	(((_type) >= PANEL_EXPR_TYPE_OPERAND_U32) && \
	 (_type) <= PANEL_EXPR_TYPE_OPERAND_FUNC)

#define IS_PANEL_EXPR_OPERATOR_FROM(_type) ((_type) == PANEL_EXPR_TYPE_OPERATOR_FROM)
#define IS_PANEL_EXPR_OPERATOR_TO(_type) ((_type) == PANEL_EXPR_TYPE_OPERATOR_TO)

#define IS_PANEL_EXPR_GROUP_OPERATOR(_type) \
	((_type) == PANEL_EXPR_TYPE_OPERATOR_FROM || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_TO)

#define IS_PANEL_EXPR_ARITHMETIC_OPERATOR(_type) \
	((_type) == PANEL_EXPR_TYPE_OPERATOR_MOD)

#define IS_PANEL_EXPR_RELATION_OPERATOR(_type) \
	((_type) == PANEL_EXPR_TYPE_OPERATOR_LT || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_GT || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_LE || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_GE)

#define IS_PANEL_EXPR_BITWISE_OPERATOR(_type) \
	((_type) == PANEL_EXPR_TYPE_OPERATOR_BIT_AND || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_BIT_OR)

#define IS_PANEL_EXPR_LOGIC_OPERATOR(_type) \
	((_type) == PANEL_EXPR_TYPE_OPERATOR_AND || \
	 (_type) == PANEL_EXPR_TYPE_OPERATOR_OR)

#define IS_PANEL_EXPR_OPERATOR(_type) \
	(((_type) >= PANEL_EXPR_TYPE_OPERATOR_FROM) && \
	 (_type) <= PANEL_EXPR_TYPE_OPERATOR_OR)

#define IS_PANEL_EXPR_UNARY_OPERATOR(_type) \
	 ((_type) == PANEL_EXPR_TYPE_OPERATOR_NOT)

#define IS_PANEL_EXPR_BINARY_OPERATOR(_type) \
	(IS_PANEL_EXPR_OPERATOR(_type) && \
	 !IS_PANEL_EXPR_UNARY_OPERATOR(_type) && \
	 !IS_PANEL_EXPR_GROUP_OPERATOR(_type))

#define IS_PANEL_EXPR_TYPE(_type) \
	(((_type) >= 0) && (_type) <= MAX_PANEL_EXPR_TYPE)

struct panel_expr_data {
	unsigned int type;
	union {
		unsigned int u32;
		int s32;
		char *str;
		struct pnobj_func *func;
	} op;
};

struct panel_expr_node {
	struct panel_expr_data data;
	struct panel_expr_node *left;
	struct panel_expr_node *right;
	struct list_head list;
};

struct panel_expr {
	struct panel_expr_data *data;
	unsigned int num_data;
	struct panel_expr_node *root;
};

#define PANEL_EXPR_OPERATOR(_operator) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERATOR_ ## _operator), \
	.op.u32 = (0U), \
}

#define PANEL_EXPR_OPERAND_U32(_operand) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERAND_U32), \
	.op.u32 = (_operand), \
}

#define PANEL_EXPR_OPERAND_S32(_operand) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERAND_S32), \
	.op.s32 = (_operand), \
}

#define PANEL_EXPR_OPERAND_STR(_operand) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERAND_STR), \
	.op.str = (_operand), \
}

#define PANEL_EXPR_OPERAND_PROP(_operand) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERAND_PROP), \
	.op.str = (_operand), \
}

#define PANEL_EXPR_OPERAND_FUNC(_operand) \
{ \
	.type = (PANEL_EXPR_TYPE_OPERAND_FUNC), \
	.op.func = (_operand), \
}

void exprtree_delete(struct panel_expr_node *node);
struct panel_expr_node *panel_expr_from_infix(struct panel_expr_data *data, size_t num_data);
int panel_expr_eval(struct panel_device *panel, struct panel_expr_node *root);

#endif /* __PANEL_EXPRESSION_H__ */
