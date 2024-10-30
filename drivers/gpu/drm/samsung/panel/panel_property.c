// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"

const char *panel_prop_type_name[MAX_PANEL_PROP_TYPE] = {
	[PANEL_PROP_TYPE_RANGE] = "RANGE",
	[PANEL_PROP_TYPE_ENUM] = "ENUM",
};

const char *prop_type_to_string(u32 type)
{
	if (type >= MAX_PANEL_PROP_TYPE)
		return NULL;

	return panel_prop_type_name[type];
}

int string_to_prop_type(const char *str)
{
	unsigned int type;

	if (!str)
		return -EINVAL;

	for (type = 0; type < MAX_PANEL_PROP_TYPE; type++) {
		if (!panel_prop_type_name[type])
			continue;

		if (!strcmp(panel_prop_type_name[type], str))
			return type;
	}

	return -EINVAL;
}

int snprintf_property(char *buf, size_t size,
		struct panel_property *property)
{
	int len;

	if (!buf || !size || !property)
		return 0;

	len = snprintf(buf, size, "%s\n",
			get_panel_property_name(property));
	len += snprintf(buf + len, size - len, "type: %s\n",
			prop_type_to_string(property->type));
	if (property->type == PANEL_PROP_TYPE_RANGE) {
		len += snprintf(buf + len, size - len, "value: %d (min: %d, max: %d)",
				property->value, property->min, property->max);
	} else if (property->type == PANEL_PROP_TYPE_ENUM) {
		struct panel_property_enum *prop_enum;

		prop_enum = panel_property_find_enum_item_by_value(property, property->value);
		if (!prop_enum) {
			panel_err("property(%s) enum (%d) not found\n",
					get_panel_property_name(property), property->value);
			return 0;
		}

		len += snprintf(buf + len, size - len, "value: %d (%s)\n",
				property->value, prop_enum->name);
		len += snprintf(buf + len, size - len, "enum:\n");
		list_for_each_entry(prop_enum, &property->enum_list, head) {
			len += snprintf(buf + len, size - len, "%s[%d]: %s",
					(prop_enum->value == property->value) ? "*" : " ",
					prop_enum->value, prop_enum->name);
			if (prop_enum->value != property->max)
				len += snprintf(buf + len, size - len, "\n");
		}
	}

	return len;
}
EXPORT_SYMBOL(snprintf_property);

struct panel_property *panel_property_create(u32 type, const char *name)
{
	struct panel_property *property = NULL;

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	if (WARN_ON(strlen(name) >= PANEL_PROP_NAME_LEN))
		return NULL;

	property = kzalloc(sizeof(*property), GFP_KERNEL);
	if (!property)
		return NULL;

	pnobj_init(&property->base, CMD_TYPE_PROP, (char *)name);
	property->type = type;
	INIT_LIST_HEAD(&property->enum_list);

	return property;
}

void panel_property_enum_free(struct panel_property *property)
{
	struct panel_property_enum *prop_enum, *pt;

	list_for_each_entry_safe(prop_enum, pt, &property->enum_list, head) {
		list_del(&prop_enum->head);
		kfree(prop_enum);
	}
}

void panel_property_destroy(struct panel_property *property)
{
	if (!property)
		return;

	panel_dbg("%s\n", get_panel_property_name(property));
	panel_property_enum_free(property);
	pnobj_deinit(&property->base);
	kfree(property);
}

int panel_property_compare(struct panel_property *lprop, struct panel_property *rprop)
{
	int ret;

	if (lprop == rprop)
		return 0;

	ret = strncmp(get_panel_property_name(lprop),
			get_panel_property_name(rprop),
			PANEL_PROP_NAME_LEN);
	if (ret != 0)
		return ret;

	if (lprop->type != rprop->type)
		return lprop->type - rprop->type;

	if (lprop->min != rprop->min)
		return lprop->min - rprop->min;

	if (lprop->max != rprop->max)
		return lprop->max - rprop->max;

	return 0;
}

int panel_property_update(struct panel_device *panel, char *name)
{
	struct panel_property *property;

	property = panel_find_property(panel, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (!property->update)
		return 0;

	return property->update(property);
}

int panel_property_set_range_value(struct panel_property *property, unsigned int value)
{
	if (!property) {
		panel_err("property is null.\n");
		return -EINVAL;
	}

	if (!panel_property_type_is(property, PANEL_PROP_TYPE_RANGE)) {
		panel_err("property type is not match(%s).\n",
				get_panel_property_name(property));
		return -EINVAL;
	}

	if ((value < property->min) || (value > property->max)) {
		panel_err("property(%s) value(%u) out of range(%u ~ %u)\n",
				get_panel_property_name(property),
				value, property->min, property->max);
		return -EINVAL;
	}

	property->value = value;

	return 0;
}

struct panel_property_enum *panel_property_find_enum_item_by_name(
		struct panel_property *property, char *name)
{
	struct panel_property_enum *prop_enum;

	if (!property)
		return NULL;

	list_for_each_entry(prop_enum, &property->enum_list, head) {
		if (!strncmp(name, prop_enum->name, PROP_ENUM_NAME_LEN))
			return prop_enum;
	}

	return NULL;
}

struct panel_property_enum *panel_property_find_enum_item_by_value(
		struct panel_property *property, int value)
{
	struct panel_property_enum *prop_enum;

	if (!property)
		return NULL;

	list_for_each_entry(prop_enum, &property->enum_list, head) {
		if (prop_enum->value == value)
			return prop_enum;
	}

	return NULL;
}

int panel_property_get_enum_value(struct panel_property *property, char *name)
{
	struct panel_property_enum *prop_enum;

	if (!property)
		return -EINVAL;

	prop_enum = panel_property_find_enum_item_by_name(property, name);
	if (!prop_enum) {
		panel_err("property(%s) enum (%s) not found\n",
				get_panel_property_name(property), name);
		return -EINVAL;
	}

	return prop_enum->value;
}

int panel_property_set_enum_value(struct panel_property *property, unsigned int value)
{
	struct panel_property_enum *prop_enum;

	if (!property)
		return -EINVAL;

	if (!panel_property_type_is(property, PANEL_PROP_TYPE_ENUM)) {
		panel_err("property type is not match(%s).\n",
				get_panel_property_name(property));
		return -EINVAL;
	}

	prop_enum = panel_property_find_enum_item_by_value(property, value);
	if (!prop_enum) {
		panel_err("property(%s) enum (%d) not found\n",
				get_panel_property_name(property), value);
		return -EINVAL;
	}

	property->value = value;

	return 0;
}

int panel_property_set_value(struct panel_property *property, unsigned int value)
{
	int ret;

	if (!property)
		return -EINVAL;

	if (panel_property_type_is(property, PANEL_PROP_TYPE_RANGE)) {
		ret = panel_property_set_range_value(property, value);
		if (ret < 0)
			return ret;
	} else if (panel_property_type_is(property, PANEL_PROP_TYPE_ENUM)) {
		ret = panel_property_set_enum_value(property, value);
		if (ret < 0)
			return ret;
	} else {
		panel_err("unknown property type (%d:%s).\n",
				property->type, prop_type_to_string(property->type));
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(panel_property_set_value);

int panel_property_get_value(struct panel_property *property)
{
	if (!property)
		return -EINVAL;

	return property->value;
}
EXPORT_SYMBOL(panel_property_get_value);

int panel_property_add_enum_value(struct panel_property *property,
		unsigned int value, const char *name)
{
	struct panel_property_enum *prop_enum;
	int index = 0;

	if (!property)
		return -EINVAL;

	list_for_each_entry(prop_enum, &property->enum_list, head) {
		if (WARN_ON(prop_enum->value == value)) {
			panel_err("%s:%s:%d already exists\n",
					get_panel_property_name(property),
					name, value);
			return -EINVAL;
		}
		index++;
	}

	prop_enum = kzalloc(sizeof(*prop_enum), GFP_KERNEL);
	if (!prop_enum)
		return -ENOMEM;

	strncpy(prop_enum->name, name, PROP_ENUM_NAME_LEN);
	prop_enum->name[PROP_ENUM_NAME_LEN-1] = '\0';
	prop_enum->value = value;
	property->min = min_t(u32, property->min, (u32)value);
	property->max = max_t(u32, property->max, (u32)value);

	list_add_tail(&prop_enum->head, &property->enum_list);

	return 0;
}

struct panel_property *panel_property_create_range(char *name,
		unsigned int init_value, unsigned int min, unsigned int max)
{
	struct panel_property *property;

	property = panel_property_create(PANEL_PROP_TYPE_RANGE, name);
	if (!property)
		return NULL;

	property->value = init_value;
	property->min = min;
	property->max = max;

	return property;
}

struct panel_property *panel_property_create_enum(char *name,
		unsigned int init_value, const struct panel_prop_enum_item *items,
		int num_items)
{
	struct panel_property *property;
	int i, ret;

	property = panel_property_create(PANEL_PROP_TYPE_ENUM, name);
	if (!property)
		return NULL;

	property->value = init_value;
	for (i = 0; i < num_items; i++) {
		if (!items[i].name)
			continue;

		ret = panel_property_add_enum_value(property,
				items[i].type, items[i].name);
		if (ret) {
			panel_property_destroy(property);
			return NULL;
		}
	}

	return property;
}

struct panel_property *panel_property_clone(struct panel_property *property)
{
	struct panel_property *new_prop;
	struct panel_property_enum *prop_enum;
	int ret;

	new_prop = panel_property_create(property->type,
			get_panel_property_name(property));
	if (!new_prop)
		return NULL;

	new_prop->value = property->value;
	new_prop->min = property->min;
	new_prop->max = property->max;

	list_for_each_entry(prop_enum, &property->enum_list, head) {
		ret = panel_property_add_enum_value(new_prop,
				prop_enum->value,
				prop_enum->name);
		if (ret < 0) {
			panel_property_destroy(new_prop);
			return NULL;
		}
	}
	new_prop->panel = property->panel;
	new_prop->update = property->update;

	return new_prop;
}

struct panel_property *panel_find_property(struct panel_device *panel, char *name)
{
	struct pnobj *pos;

	if (!panel) {
		panel_err("panel is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	if (WARN_ON(strlen(name) >= PANEL_PROP_NAME_LEN))
		return NULL;

	list_for_each_entry(pos, &panel->prop_list, list) {
		struct panel_property *property =
			pnobj_container_of(pos, struct panel_property);

		if (!strncmp(get_panel_property_name(property),
					name, PANEL_PROP_NAME_LEN))
			return property;
	}

	return NULL;
}

struct panel_property *panel_find_property_by_property(struct panel_device *panel,
		struct panel_property *rprop)
{
	struct pnobj *pos;

	if (!panel) {
		panel_err("panel is null.\n");
		return NULL;
	}

	if (!rprop)
		return NULL;

	list_for_each_entry(pos, &panel->prop_list, list) {
		struct panel_property *lprop =
			pnobj_container_of(pos, struct panel_property);

		if (!panel_property_compare(lprop, rprop))
			return lprop;
	}

	return NULL;
}

int panel_set_property_value(struct panel_device *panel, char *name, unsigned int value)
{
	struct panel_property *property;

	if (!panel) {
		panel_err("panel is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_find_property(panel, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	return panel_property_set_value(property, value);
}
EXPORT_SYMBOL(panel_set_property_value);

int panel_get_property_value(struct panel_device *panel, char *name)
{
	struct panel_property *property;
	int ret;

	property = panel_find_property(panel, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->update) {
		ret = property->update(property);
		if (ret < 0) {
			panel_err("failed to update property(%s)\n", name);
			return ret;
		}
	}

	return panel_property_get_value(property);
}
EXPORT_SYMBOL(panel_get_property_value);

int panel_add_range_property(struct panel_device *panel,
		char *name, unsigned int init_value, unsigned int min, unsigned int max,
		int (*update)(struct panel_property *))
{
	struct panel_property *property;

	if (!panel) {
		panel_err("panel is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_find_property(panel, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	property = panel_property_create_range(name, init_value, min, max);
	if (!property)
		return -EINVAL;

	property->panel = panel;
	property->update = update;

	list_add_tail(get_pnobj_list(&property->base), &panel->prop_list);

	panel_dbg("added: %s init_value: %d.\n",
			get_panel_property_name(property), property->value);

	return 0;
}

int panel_add_enum_property(struct panel_device *panel,
		char *name, unsigned int init_value,
		const struct panel_prop_enum_item *props,
		int num_values, int (*update)(struct panel_property *))
{
	struct panel_property *property;

	if (panel_find_property(panel, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	property = panel_property_create_enum(name, init_value, props, num_values);
	if (!property)
		return -EINVAL;

	property->panel = panel;
	property->update = update;

	list_add_tail(get_pnobj_list(&property->base), &panel->prop_list);

	panel_dbg("added: %s init_enum: %d.\n",
			get_panel_property_name(property), property->value);

	return 0;
}

struct panel_property *panel_clone_property(struct panel_device *panel, char *name)
{
	struct panel_property *property;

	property = panel_find_property(panel, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return NULL;
	}

	return panel_property_clone(property);
}

int panel_add_property_from_array(struct panel_device *panel,
		struct panel_prop_list *prop_array, size_t num_prop_array)
{
	int i;

	for (i = 0; i < num_prop_array; i++) {
		int ret = 0;

		if (prop_array[i].type == PANEL_PROP_TYPE_RANGE) {
			ret = panel_add_range_property(panel,
					(char *)prop_array[i].name,
					prop_array[i].init_value,
					prop_array[i].min, prop_array[i].max,
					prop_array[i].update);
		} else if (prop_array[i].type == PANEL_PROP_TYPE_ENUM) {
			ret = panel_add_enum_property(panel,
					(char *)prop_array[i].name,
					prop_array[i].init_value,
					prop_array[i].items, prop_array[i].num_items,
					prop_array[i].update);
		}

		if (ret < 0) {
			panel_err("failed to add property(%s:%s)\n",
					prop_type_to_string(prop_array[i].type),
					prop_array[i].name);
			return ret;
		}
	}

	return 0;
}

int panel_delete_property(struct panel_device *panel, char *name)
{
	struct panel_property *property;

	if (!panel) {
		panel_err("panel is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_find_property(panel, name);
	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	panel_property_destroy(property);

	return 0;
}

int panel_delete_property_from_array(struct panel_device *panel,
		struct panel_prop_list *prop_array, size_t num_prop_array)
{
	int i;

	for (i = 0; i < num_prop_array; i++) {
		int ret = 0;

		ret = panel_delete_property(panel, (char *)prop_array[i].name);
		if (ret < 0) {
			panel_err("failed to delete property(%s:%s)\n",
					prop_type_to_string(prop_array[i].type),
					prop_array[i].name);
			return ret;
		}
	}

	return 0;
}

void panel_delete_property_all(struct panel_device *panel)
{
	struct pnobj *pos, *next;

	if (!panel) {
		panel_err("panel is null.\n");
		return;
	}

	list_for_each_entry_safe(pos, next, &panel->prop_list, list)
		panel_property_destroy(pnobj_container_of(pos, struct panel_property));
}

MODULE_DESCRIPTION("property driver for panel");
MODULE_LICENSE("GPL v2");
