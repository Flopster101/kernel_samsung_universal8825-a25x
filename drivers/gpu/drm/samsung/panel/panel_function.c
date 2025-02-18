// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kallsyms.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"
#include "panel_function.h"

LIST_HEAD(panel_function_list);

char *get_pnobj_function_name(struct pnobj_func *pnobj_func)
{
	return get_pnobj_name(&pnobj_func->base);
}

unsigned long get_pnobj_function_addr(struct pnobj_func *pnobj_func)
{
	return pnobj_func->symaddr;
}

struct pnobj_func *create_pnobj_function(char *name, void *addr)
{
	struct pnobj_func *pnobj_func;

	if (!name || !addr)
		return NULL;

	pnobj_func = kzalloc(sizeof(struct pnobj_func), GFP_KERNEL);
	if (!pnobj_func)
		return NULL;

	pnobj_init(&pnobj_func->base, CMD_TYPE_FUNC, name);
	pnobj_func->symaddr = (unsigned long)addr;

	return pnobj_func;
}

void destroy_pnobj_function(struct pnobj_func *pnobj_func)
{
	if (!pnobj_func)
		return;

	pnobj_deinit(&pnobj_func->base);
	kfree(pnobj_func);
}

struct pnobj_func *deepcopy_pnobj_function(struct pnobj_func *dst,
		struct pnobj_func *src)
{
	if (!dst || !src)
		return NULL;

	if (dst == src)
		return dst;

	pnobj_init(&dst->base, CMD_TYPE_FUNC,
			get_pnobj_function_name(src));
	dst->symaddr = src->symaddr;

	return dst;
}

int pnobj_function_list_add(struct pnobj_func *f, struct list_head *list)
{
	struct pnobj_func *pnobj_func;
	char *name;

	if (!f)
		return -EINVAL;

	if (!list)
		return -EINVAL;

	name = get_pnobj_function_name(f);
	if (!name)
		return -EINVAL;

	if (pnobj_find_by_name(list, name))
		return 0;

	pnobj_func = create_pnobj_function(name,
			(void *)get_pnobj_function_addr(f));
	if (!pnobj_func) {
		panel_err("failed to create pnobj function(%s)\n", name);
		return -EINVAL;
	}

	list_add_tail(get_pnobj_list(&pnobj_func->base), list);

	return 0;
}

int panel_function_insert(struct pnobj_func *f)
{
	int ret;

	if (!f)
		return -EINVAL;

	ret = pnobj_function_list_add(f, &panel_function_list);
	if (ret < 0)
		panel_err("failed to add (%s) on function table\n",
				get_pnobj_function_name(f));

	return ret;
}
EXPORT_SYMBOL(panel_function_insert);

struct pnobj_func *panel_function_lookup(char *name)
{
	struct pnobj *pnobj;

	if (!name)
		return NULL;

	pnobj = pnobj_find_by_name(&panel_function_list, name);
	if (!pnobj)
		return NULL;

	return pnobj_container_of(pnobj, struct pnobj_func);
}
EXPORT_SYMBOL(panel_function_lookup);

int panel_function_insert_array(struct pnobj_func *array, size_t size)
{
	int i, ret;

	for (i = 0; i < size; i++) {
		if (!get_pnobj_function_addr(&array[i])) {
			panel_warn("array[%d] is empty\n", i);
			continue;
		}

		ret = panel_function_insert(&array[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(panel_function_insert_array);

void panel_function_init(void)
{
	INIT_LIST_HEAD(&panel_function_list);
}

void panel_function_deinit(void)
{
	struct pnobj *pos, *next;

	list_for_each_entry_safe(pos, next, &panel_function_list, list)
		destroy_pnobj_function(pnobj_container_of(pos, struct pnobj_func));
}

MODULE_DESCRIPTION("panel function driver");
MODULE_LICENSE("GPL v2");
