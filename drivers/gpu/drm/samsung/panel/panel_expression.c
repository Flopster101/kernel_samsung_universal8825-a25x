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
#include "panel_function.h"

static const char * const panel_expr_type_name[] = {
	/* operand */
	[PANEL_EXPR_TYPE_OPERAND_U32] = "U32",
	[PANEL_EXPR_TYPE_OPERAND_S32] = "S32",
	[PANEL_EXPR_TYPE_OPERAND_STR] = "STR",
	[PANEL_EXPR_TYPE_OPERAND_PROP] = "PROP",
	[PANEL_EXPR_TYPE_OPERAND_FUNC] = "FUNC",

	/* operator */
	[PANEL_EXPR_TYPE_OPERATOR_FROM] = "FROM",
	[PANEL_EXPR_TYPE_OPERATOR_TO] = "TO",
	/* logical not operator */
	[PANEL_EXPR_TYPE_OPERATOR_NOT] = "NOT",
	/* arithmetic operator: *, /, % */
	[PANEL_EXPR_TYPE_OPERATOR_MOD] = "MOD",
	/* relational operator: <, <=, >, >= */
	[PANEL_EXPR_TYPE_OPERATOR_LT] = "LT",
	[PANEL_EXPR_TYPE_OPERATOR_GT] = "GT",
	[PANEL_EXPR_TYPE_OPERATOR_LE] = "LE",
	[PANEL_EXPR_TYPE_OPERATOR_GE] = "GE",
	/* relational operator: ==, != */
	[PANEL_EXPR_TYPE_OPERATOR_EQ] = "EQ",
	[PANEL_EXPR_TYPE_OPERATOR_NE] = "NE",
	/* bitwise operator &, | */
	[PANEL_EXPR_TYPE_OPERATOR_BIT_AND] = "BIT_AND",
	[PANEL_EXPR_TYPE_OPERATOR_BIT_OR] = "BIT_OR",
	/* logical operator &&, || */
	[PANEL_EXPR_TYPE_OPERATOR_AND] = "AND",
	[PANEL_EXPR_TYPE_OPERATOR_OR] = "OR",
};

const char *panel_expr_type_enum_to_string(unsigned int type)
{
	if (type >= ARRAY_SIZE(panel_expr_type_name))
		return NULL;

	return panel_expr_type_name[type];
}

int panel_expr_type_string_to_enum(char *str)
{
	int i;

	if (!str)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(panel_expr_type_name); i++) {
		if (panel_expr_type_name[i] != NULL &&
				!strncmp(panel_expr_type_name[i], str, SZ_32))
			return i;
	}

	BUILD_BUG_ON_MSG(ARRAY_SIZE(panel_expr_type_name) != MAX_PANEL_EXPR_TYPE,
			"panel_expr_type enum and array mismatch!!");

	return -EINVAL;
}

__visible_for_testing int operator_precedence(struct panel_expr_data *data)
{
	int precedence;

	if (!data)
		return -EINVAL;

	switch (data->type) {
	case PANEL_EXPR_TYPE_OPERATOR_NOT:
		precedence = 8;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_MOD:
		precedence = 7;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_LT:
	case PANEL_EXPR_TYPE_OPERATOR_GT:
	case PANEL_EXPR_TYPE_OPERATOR_LE:
	case PANEL_EXPR_TYPE_OPERATOR_GE:
		precedence = 6;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_EQ:
	case PANEL_EXPR_TYPE_OPERATOR_NE:
		precedence = 5;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_BIT_AND:
		precedence = 4;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_BIT_OR:
		precedence = 3;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_AND:
		precedence = 2;
		break;
	case PANEL_EXPR_TYPE_OPERATOR_OR:
		precedence = 1;
		break;
	default:
		precedence = -1;
		break;
	}

	return precedence;
}

const char *get_panel_expr_operator_symbol(unsigned int type)
{
	static const char * const panel_expr_operator_symbol[] = {
		[PANEL_EXPR_TYPE_OPERATOR_FROM] = "(",
		[PANEL_EXPR_TYPE_OPERATOR_TO] = ")",
		[PANEL_EXPR_TYPE_OPERATOR_NOT] = "!",
		[PANEL_EXPR_TYPE_OPERATOR_MOD] = "%",
		[PANEL_EXPR_TYPE_OPERATOR_LT] = "<",
		[PANEL_EXPR_TYPE_OPERATOR_GT] = ">",
		[PANEL_EXPR_TYPE_OPERATOR_LE] = "<=",
		[PANEL_EXPR_TYPE_OPERATOR_GE] = ">=",
		[PANEL_EXPR_TYPE_OPERATOR_EQ] = "==",
		[PANEL_EXPR_TYPE_OPERATOR_NE] = "!=",
		[PANEL_EXPR_TYPE_OPERATOR_BIT_AND] = "&",
		[PANEL_EXPR_TYPE_OPERATOR_BIT_OR] = "|",
		[PANEL_EXPR_TYPE_OPERATOR_AND] = "&&",
		[PANEL_EXPR_TYPE_OPERATOR_OR] = "||",
	};

	if (!IS_PANEL_EXPR_OPERATOR(type))
		return NULL;

	return panel_expr_operator_symbol[type];
}

__visible_for_testing int snprintf_panel_expr_data(char *buf, size_t size, struct panel_expr_data *data)
{
	int len = 0;

	if (IS_PANEL_EXPR_OPERATOR(data->type)) {
		len = snprintf(buf, size, "%s", get_panel_expr_operator_symbol(data->type));
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_U32) {
		len = snprintf(buf, size, "%u", data->op.u32);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_S32) {
		len = snprintf(buf, size, "%d", data->op.s32);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_STR) {
		len = snprintf(buf, size, "%s", data->op.str);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_PROP) {
		len = snprintf(buf, size, "[%s]", data->op.str);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_FUNC) {
		len = snprintf(buf, size, "[%s()]",
				get_pnobj_function_name(data->op.func));
	} else {
		len = 0;
	}

	return len;
}

__visible_for_testing int snprintf_panel_expr_data_array(char *buf, size_t size,
		struct panel_expr_data *data, unsigned int num_data)
{
	unsigned int i;
	int len = 0;

	if (!data)
		return 0;

	for (i = 0; i < num_data; i++)
		len += snprintf_panel_expr_data(buf + len, size - len, &data[i]);

	return len;
}

__visible_for_testing bool valid_panel_expr_infix(struct panel_expr_data *data, size_t num_data)
{
	int i, prev_type, next_type;
	int brace_count = 0;
	char expr[SZ_128];

	if (!data || num_data == 0)
		return false;

	/* starts with binary operator */
	if (IS_PANEL_EXPR_BINARY_OPERATOR(data[0].type)) {
		panel_err("invalid expression: starts with binary operator(%s)\n",
				get_panel_expr_operator_symbol(data[0].type));
		goto err;
	}

	/* ends with binary operator */
	if (IS_PANEL_EXPR_BINARY_OPERATOR(data[num_data - 1].type)) {
		panel_err("invalid expression: ends with binary operator(%s)\n",
				get_panel_expr_operator_symbol(data[num_data - 1].type));
		goto err;
	}

	if (IS_PANEL_EXPR_UNARY_OPERATOR(data[num_data - 1].type)) {
		panel_err("invalid expression: ends with unary operator(%s)\n",
				get_panel_expr_operator_symbol(data[num_data - 1].type));
		goto err;
	}

	for (i = 0; i < num_data; i++) {
		prev_type = (i == 0) ? -1 : data[i - 1].type;
		next_type = (i == num_data - 1) ? -1 : data[i + 1].type;

		if (IS_PANEL_EXPR_OPERAND(data[i].type)) {
			if (IS_PANEL_EXPR_OPERAND(prev_type)) {
				panel_err("invalid expression:%d: continuous operand\n", i);
				goto err;
			}
		} else if (IS_PANEL_EXPR_OPERATOR_FROM(data[i].type)) {
			if (prev_type != -1 &&
				!IS_PANEL_EXPR_OPERATOR_FROM(prev_type) &&
				!IS_PANEL_EXPR_UNARY_OPERATOR(prev_type) &&
				!IS_PANEL_EXPR_BINARY_OPERATOR(prev_type)) {
				panel_err("invalid expression:%d: ['(' or unary, binary operator] are permitted on the left\n", i);
				goto err;
			}
		} else if (IS_PANEL_EXPR_OPERATOR_TO(data[i].type)) {
			if (prev_type != -1 &&
				!IS_PANEL_EXPR_OPERAND(prev_type) &&
				!IS_PANEL_EXPR_OPERATOR_TO(prev_type)) {
				panel_err("invalid expression:%d: [operand, ')'] are permitted on the left\n", i);
				goto err;
			}
		} else if (IS_PANEL_EXPR_UNARY_OPERATOR(data[i].type)) {
			if (next_type != -1 &&
				!IS_PANEL_EXPR_OPERAND(next_type) &&
				!IS_PANEL_EXPR_UNARY_OPERATOR(next_type) &&
				!IS_PANEL_EXPR_OPERATOR_FROM(next_type)) {
				panel_err("invalid expression:%d: [operand, '!', '('] are permitted on the right\n", i);
				goto err;
			}
		} else if (IS_PANEL_EXPR_BINARY_OPERATOR(data[i].type)) {
			if (prev_type != -1 &&
				!IS_PANEL_EXPR_OPERAND(prev_type) &&
				!IS_PANEL_EXPR_OPERATOR_TO(prev_type)) {
				panel_err("invalid expression:%d: [operand or ')'] are permitted on the left\n", i);
				goto err;
			}

			if (next_type != -1 &&
				!IS_PANEL_EXPR_OPERAND(next_type) &&
				!IS_PANEL_EXPR_UNARY_OPERATOR(next_type) &&
				!IS_PANEL_EXPR_OPERATOR_FROM(next_type)) {
				panel_err("invalid expression:%d: [operand, '!', '('] are permitted on the right\n", i);
				goto err;
			}
		}

		if (IS_PANEL_EXPR_GROUP_OPERATOR(data[i].type)) {
			if (IS_PANEL_EXPR_OPERATOR_FROM(data[i].type))
				brace_count++;
			else if (IS_PANEL_EXPR_OPERATOR_TO(data[i].type))
				brace_count--;

			if (brace_count < 0) {
				panel_err("invalid expression:%d: parenthesis not opened\n", i);
				goto err;
			}
		}
	}

	if (brace_count != 0) {
		panel_err("invalid expression: parenthesis not %s\n",
				brace_count < 0 ? "opened" : "closed");
		goto err;
	}

	return true;

err:
	snprintf_panel_expr_data_array(expr, SZ_128, data, num_data);
	panel_err("expr: %s\n", expr);

	return false;
}

struct panel_expr_data *infix_to_postfix(struct panel_expr_data *infix, size_t num_infix)
{
	int i, j, top = -1;
	struct panel_expr_data *postfix =
		kzalloc(sizeof(*infix) * (num_infix + 2), GFP_KERNEL);
	struct panel_expr_data *ops =
		kzalloc(sizeof(*infix) * (num_infix + 2), GFP_KERNEL);

	if (!valid_panel_expr_infix(infix, num_infix)) {
		panel_err("invalid infix expression\n");
		goto err;
	}

	for (i = 0, j = 0; i < num_infix; i++) {
		if (!IS_PANEL_EXPR_TYPE(infix[i].type))
			continue;
		if (IS_PANEL_EXPR_OPERAND(infix[i].type)) {
			memcpy(&postfix[j++], &infix[i], sizeof(*infix));
		} else if (IS_PANEL_EXPR_OPERATOR_FROM(infix[i].type)) {
			memcpy(&ops[++top], &infix[i], sizeof(*infix));
		} else if (IS_PANEL_EXPR_OPERATOR_TO(infix[i].type)) {
			while (top > -1 && !IS_PANEL_EXPR_OPERATOR_FROM(ops[top].type))
				memcpy(&postfix[j++], &ops[top--], sizeof(*infix));
			if (top > -1 && !IS_PANEL_EXPR_OPERATOR_FROM(ops[top].type)) {
				panel_err("Invalid Expression");
				goto err;
			}
			top--;
		} else if (IS_PANEL_EXPR_UNARY_OPERATOR(infix[i].type)) {
			memcpy(&ops[++top], &infix[i], sizeof(*infix));
		} else if (IS_PANEL_EXPR_OPERATOR(infix[i].type)) {
			while (top > -1 && operator_precedence(&ops[top]) >=
					operator_precedence(&infix[i])) {
				memcpy(&postfix[j++], &ops[top--], sizeof(*infix));
			}
			memcpy(&ops[++top], &infix[i], sizeof(*infix));
		}
	}

	while (top > -1)
		memcpy(&postfix[j++], &ops[top--], sizeof(*infix));

	kfree(ops);

	return postfix;

err:
	kfree(postfix);
	kfree(ops);

	return NULL;
}

static void panel_expr_node_push(struct panel_expr_node *node,
		struct list_head *head)
{
	list_add(&node->list, head);
}

static struct panel_expr_node *panel_expr_node_pop(struct list_head *head)
{
	struct panel_expr_node *node;

	if (list_empty(head))
		return NULL;

	node = list_first_entry(head, struct panel_expr_node, list);
	list_del(&node->list);

	return node;
}

static struct panel_expr_node *panel_expr_node_create(struct panel_expr_data *data)
{
	struct panel_expr_node *node;

	if (!data)
		return NULL;

	node = kzalloc(sizeof(struct panel_expr_node), GFP_KERNEL);
	if (!node)
		return NULL;

	memcpy(&node->data, data, sizeof(*data));

	return node;
}

static void panel_expr_node_combine(struct panel_expr_node *node, struct list_head *head)
{
	if (!node || !head)
		return;

	if (IS_PANEL_EXPR_BINARY_OPERATOR(node->data.type))
		node->right = panel_expr_node_pop(head);
	else
		node->right = NULL;
	node->left = panel_expr_node_pop(head);
	panel_expr_node_push(node, head);
}

void exprtree_inorder(struct panel_expr_node *node, struct list_head *inorder_head)
{
	if (!node)
		return;

	exprtree_inorder(node->left, inorder_head);
	list_add_tail(&node->list, inorder_head);
	exprtree_inorder(node->right, inorder_head);
}

void exprtree_postorder(struct panel_expr_node *node, struct list_head *postorder_head)
{
	if (!node)
		return;

	exprtree_postorder(node->left, postorder_head);
	exprtree_postorder(node->right, postorder_head);
	list_add_tail(&node->list, postorder_head);
}

void exprtree_delete(struct panel_expr_node *node)
{
	if (!node)
		return;

	exprtree_delete(node->left);
	exprtree_delete(node->right);
	kfree(node);
}

struct panel_expr_node *panel_expr_from_infix(struct panel_expr_data *data, size_t num_data)
{
	int i, top = -1;
	struct list_head head;
	struct panel_expr_data *ops =
		kzalloc(sizeof(*data) * (num_data + 2), GFP_KERNEL);

	INIT_LIST_HEAD(&head);

	if (!valid_panel_expr_infix(data, num_data)) {
		panel_err("invalid infix expression\n");
		goto err;
	}

	for (i = 0; i < num_data; i++) {
		if (!IS_PANEL_EXPR_TYPE(data[i].type)) {
			panel_err("Invalid operator i(%d) type(%s)\n",
					i, panel_expr_type_enum_to_string(data[i].type));
			goto err;
		}
		if (IS_PANEL_EXPR_OPERAND(data[i].type)) {
			panel_expr_node_push(panel_expr_node_create(&data[i]), &head);
		} else if (IS_PANEL_EXPR_OPERATOR_FROM(data[i].type)) {
			memcpy(&ops[++top], &data[i], sizeof(*data));
		} else if (IS_PANEL_EXPR_OPERATOR_TO(data[i].type)) {
			while (top > -1 && !IS_PANEL_EXPR_OPERATOR_FROM(ops[top].type))
				panel_expr_node_combine(panel_expr_node_create(&ops[top--]), &head);
			if (top > -1 && !IS_PANEL_EXPR_OPERATOR_FROM(ops[top].type)) {
				panel_err("Invalid Expression");
				goto err;
			}
			top--;
		} else if (IS_PANEL_EXPR_UNARY_OPERATOR(data[i].type)) {
			memcpy(&ops[++top], &data[i], sizeof(*data));
		} else if (IS_PANEL_EXPR_OPERATOR(data[i].type)) {
			while (top > -1 && operator_precedence(&ops[top]) >=
					operator_precedence(&data[i])) {
				panel_expr_node_combine(panel_expr_node_create(&ops[top--]), &head);
			}
			memcpy(&ops[++top], &data[i], sizeof(*data));
		}
	}

	while (top > -1)
		panel_expr_node_combine(panel_expr_node_create(&ops[top--]), &head);

	kfree(ops);

	if (list_empty(&head))
		return NULL;

	return list_first_entry(&head, struct panel_expr_node, list);

err:
	kfree(ops);

	if (list_empty(&head))
		return NULL;

	exprtree_delete(list_first_entry(&head, struct panel_expr_node, list));

	return NULL;
}

struct panel_expr_node *panel_expr_from_postfix(struct panel_expr_data *data, size_t num_data)
{
	int i;
	struct list_head head;
	struct panel_expr_node *node;

	INIT_LIST_HEAD(&head);

	for (i = 0; i < num_data; i++) {
		if (!IS_PANEL_EXPR_TYPE(data[i].type)) {
			panel_err("Invalid operator i(%d) type(%s)\n",
					i, panel_expr_type_enum_to_string(data[i].type));
			goto err;
		}
		if (IS_PANEL_EXPR_OPERATOR_FROM(data[i].type) ||
			IS_PANEL_EXPR_OPERATOR_TO(data[i].type)) {
			panel_err("Invalid operator i(%d) type(%s)\n",
					i, panel_expr_type_enum_to_string(data[i].type));
			goto err;
		}
		if (IS_PANEL_EXPR_OPERAND(data[i].type)) {
			node = panel_expr_node_create(&data[i]);
			panel_expr_node_push(node, &head);
		} else if (IS_PANEL_EXPR_OPERATOR(data[i].type)) {
			node = panel_expr_node_create(&data[i]);
			node->right = panel_expr_node_pop(&head);
			node->left = panel_expr_node_pop(&head);
			panel_expr_node_push(node, &head);
		}
	}

	if (list_empty(&head))
		return NULL;

	return list_first_entry(&head, struct panel_expr_node, list);

err:
	if (list_empty(&head))
		return NULL;

	exprtree_delete(list_first_entry(&head, struct panel_expr_node, list));
	return NULL;
}

static int panel_expr_data_get_value(struct panel_device *panel, struct panel_expr_data *data)
{
	int value;

	if (!data)
		return 0;

	if (data->type == PANEL_EXPR_TYPE_OPERAND_U32) {
		value = data->op.u32;
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_S32) {
		value = data->op.s32;
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_STR) {
		/* not implemented */
		value = 0;
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_PROP) {
		value = panel_get_property_value(panel, data->op.str);
		if (value < 0) {
			panel_err("failed to get property(%s) value\n", data->op.str);
			return 0;
		}
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_FUNC) {
		value = call_expr_func(data, panel);
	} else {
		panel_err("invalid operand type(%d)\n", data->type);
		return 0;
	}

	return value;
}

int panel_expr_eval(struct panel_device *panel, struct panel_expr_node *root)
{
	int lvalue, rvalue, ret = 0;
	unsigned int type;

	if (!root)
		return 0;

	if (!root->left && !root->right)
		return panel_expr_data_get_value(panel, &root->data);

	type = root->data.type;
	lvalue = panel_expr_eval(panel, root->left);
	rvalue = panel_expr_eval(panel, root->right);

	if (type == PANEL_EXPR_TYPE_OPERATOR_NOT) {
		ret = !lvalue;
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_MOD) {
		if (!rvalue) {
			panel_err("rvalue of MOD operator shouldn't be zero\n");
			return 0;
		}
		ret = (lvalue % rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_LT) {
		ret = (lvalue < rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_GT) {
		ret = (lvalue > rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_LE) {
		ret = (lvalue <= rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_GE) {
		ret = (lvalue >= rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_EQ) {
		ret = (lvalue == rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_NE) {
		ret = (lvalue != rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_BIT_AND) {
		ret = (lvalue & rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_BIT_OR) {
		ret = (lvalue | rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_AND) {
		ret = (lvalue && rvalue);
	} else if (type == PANEL_EXPR_TYPE_OPERATOR_OR) {
		ret = (lvalue || rvalue);
	}

	return ret;
}

MODULE_DESCRIPTION("expression driver for panel");
MODULE_LICENSE("GPL v2");
