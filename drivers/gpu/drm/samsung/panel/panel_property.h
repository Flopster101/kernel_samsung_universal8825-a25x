/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_PROPERTY_H__
#define __PANEL_PROPERTY_H__

#include "panel_obj.h"

struct panel_device;
struct panel_property;

#define PANEL_PROP_NAME_LEN (32)
#define PROP_ENUM_NAME_LEN (64)

enum PANEL_PROP_TYPE {
	PANEL_PROP_TYPE_RANGE,
	PANEL_PROP_TYPE_ENUM,
	MAX_PANEL_PROP_TYPE
};

#define PANEL_PROPERTY_NUMBER_0 ("number_0")

#define __PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(_type) { \
	.type = (_type), \
	.name = (#_type), \
}

#define __PANEL_PROPERTY_INITIALIZER(_name, \
		_type, _init_value, _min, _max, _items, _num_items, _update_func) { \
	.name = (_name), \
	.type = (_type), \
	.init_value = (_init_value), \
	.min = (_min), \
	.max = (_max), \
	.items = (_items), \
	.num_items = (_num_items), \
	.update = (_update_func), \
}

#define __PANEL_PROPERTY_RANGE_INITIALIZER(_name, _init_value, _min, _max) \
	__PANEL_PROPERTY_INITIALIZER(_name, \
			PANEL_PROP_TYPE_RANGE, \
			_init_value, _min, _max, \
			NULL, 0, NULL)

#define __PANEL_PROPERTY_ENUM_INITIALIZER(_name, _init_value, _items) \
	__PANEL_PROPERTY_INITIALIZER(_name, \
			PANEL_PROP_TYPE_ENUM, \
			_init_value, UINT_MAX, 0, \
			_items, ARRAY_SIZE(_items), NULL)

#define __DIMEN_PROPERTY_RANGE_INITIALIZER(_name, _init_value, _min, _max, _update_func) \
	__PANEL_PROPERTY_INITIALIZER(_name, \
			PANEL_PROP_TYPE_RANGE, \
			_init_value, _min, _max, \
			NULL, 0, _update_func)

#define __DIMEN_PROPERTY_ENUM_INITIALIZER(_name, _init_value, _items, _update_func) \
	__PANEL_PROPERTY_INITIALIZER(_name, \
			PANEL_PROP_TYPE_ENUM, \
			_init_value, UINT_MAX, 0, \
			(_items), ARRAY_SIZE(_items), _update_func)

struct panel_prop_enum_item {
	int type;
	const char *name;
};

struct panel_prop_list {
	const char *name;
	unsigned int type;
	int init_value;
	int min;
	int max;
	struct panel_prop_enum_item *items;
	unsigned int num_items;
	int (*update)(struct panel_property *prop);
};

struct panel_property_enum {
	unsigned int value;
	struct list_head head;
	char name[PROP_ENUM_NAME_LEN];
};

struct panel_property {
	struct pnobj base;
	enum PANEL_PROP_TYPE type;
	unsigned int value;
	unsigned int min;
	unsigned int max;
	struct list_head enum_list;
	struct panel_device *panel;
	int (*update)(struct panel_property *prop);
};

const char *prop_type_to_string(u32 type);
int string_to_prop_type(const char *str);

static inline char *get_panel_property_name(struct panel_property *property)
{
	return get_pnobj_name(&property->base);
}

static inline bool panel_property_type_is(struct panel_property *property,
		u32 type)
{
	return property->type == type;
}

int snprintf_property(char *buf, size_t size,
		struct panel_property *property);

struct panel_property *panel_property_create(u32 type, const char *name);
void panel_property_enum_free(struct panel_property *property);
void panel_property_destroy(struct panel_property *property);
int panel_property_compare(struct panel_property *lprop, struct panel_property *rprop);
int panel_property_update(struct panel_device *panel, char *name);
int panel_property_set_range_value(struct panel_property *property, unsigned int value);
struct panel_property_enum *panel_property_find_enum_item_by_name(
		struct panel_property *property, char *name);
struct panel_property_enum *panel_property_find_enum_item_by_value(
		struct panel_property *property, int value);
int panel_property_get_enum_value(struct panel_property *property, char *name);
int panel_property_set_enum_value(struct panel_property *property, unsigned int value);
int panel_property_set_value(struct panel_property *property, unsigned int value);
int panel_property_get_value(struct panel_property *property);
int panel_property_add_enum_value(struct panel_property *property,
		unsigned int value, const char *name);
struct panel_property *panel_property_create_range(char *name,
		unsigned int init_value, unsigned int min, unsigned int max);
struct panel_property *panel_property_create_enum(char *name,
		unsigned int init_value, const struct panel_prop_enum_item *items,
		int num_items);
struct panel_property *panel_property_clone(struct panel_property *property);

struct panel_property *panel_find_property(struct panel_device *panel, char *name);
struct panel_property *panel_find_property_by_property(struct panel_device *panel,
		struct panel_property *rprop);
int panel_set_property_value(struct panel_device *panel, char *name, unsigned int value);
int panel_get_property_value(struct panel_device *panel, char *name);
int panel_add_range_property(struct panel_device *panel,
		char *name, unsigned int init_value, unsigned int min, unsigned int max,
		int (*update)(struct panel_property *));
int panel_add_enum_property(struct panel_device *panel,
		char *name, unsigned int init_value,
		const struct panel_prop_enum_item *props,
		int num_values, int (*update)(struct panel_property *));
struct panel_property *panel_clone_property(struct panel_device *panel, char *name);
int panel_add_property_from_array(struct panel_device *panel,
		struct panel_prop_list *prop_array, size_t num_prop_array);
int panel_delete_property(struct panel_device *panel, char *name);
int panel_delete_property_from_array(struct panel_device *panel,
		struct panel_prop_list *prop_array, size_t num_prop_array);
void panel_delete_property_all(struct panel_device *panel);

#endif /* __PANEL_PROPERTY_H__ */
