/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_FUNCTION_H__
#define __PANEL_FUNCTION_H__

#include "panel_obj.h"

#define PNOBJ_FUNC_NAME_LEN (PNOBJ_NAME_LEN)

#define PNOBJ_FUNC(_name) PN_CONCAT(pnobj_func, _name)

#define __PNOBJ_FUNC_INITIALIZER(_name, _func) \
	{ .base = __PNOBJ_INITIALIZER(_name, CMD_TYPE_FUNC) \
	, .symaddr = (unsigned long)_func }

#define DEFINE_PNOBJ_FUNC(_name, _func) \
	struct pnobj_func PNOBJ_FUNC(_name) = __PNOBJ_FUNC_INITIALIZER(_name, _func)

struct pnobj_func {
	struct pnobj base;
	unsigned long symaddr;
};

struct pnobj_func *create_pnobj_function(char *name, void *addr);
void destroy_pnobj_function(struct pnobj_func *pnobj_func);
struct pnobj_func *deepcopy_pnobj_function(struct pnobj_func *dst,
		struct pnobj_func *src);
int pnobj_function_list_add(struct pnobj_func *f, struct list_head *list);
char *get_pnobj_function_name(struct pnobj_func *pnobj_func);
unsigned long get_pnobj_function_addr(struct pnobj_func *pnobj_func);

int panel_function_insert(struct pnobj_func *f);
struct pnobj_func *panel_function_lookup(char *name);
int panel_function_insert_array(struct pnobj_func *array, size_t size);
void panel_function_init(void);
void panel_function_deinit(void);

#endif /* __PANEL_FUNCTION_H__ */
