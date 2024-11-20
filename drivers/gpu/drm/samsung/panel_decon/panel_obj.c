// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>

#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"

struct panel_obj_property *panel_obj_find_property(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	name_len = strlen(name);

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
		return NULL;
	}

	list_for_each_entry(property, &properties->list, head) {
		if (strlen(property->name) != name_len)
			continue;

		if (!strncmp(property->name, name, name_len))
			return property;
	}

	return NULL;
}

int panel_obj_set_property_value(struct panel_obj_properties *properties, char *name, unsigned int value)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_VALUE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	property->value = value;

	return 0;
}

int panel_obj_get_property_value(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_VALUE) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	return property->value;
}

int panel_obj_set_property_str(struct panel_obj_properties *properties, char *name, char *str)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return -EINVAL;
	}

	property->str = str;

	return 0;
}

char *panel_obj_get_property_str(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return NULL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return NULL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return NULL;
	}

	if (property->type != PANEL_OBJ_PROP_TYPE_STR) {
		panel_err("property type is not match(%s).\n", name);
		return NULL;
	}

	return property->str;
}

int panel_obj_delete_property(struct panel_obj_properties *properties, char *name)
{
	struct panel_obj_property *property;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	property = panel_obj_find_property(properties, name);

	if (!property) {
		panel_err("property is not exist(%s).\n", name);
		return -EINVAL;
	}

	list_del(&property->head);
	kfree(property);

	return 0;
}

int panel_obj_add_property_value(struct panel_obj_properties *properties, char *name, unsigned int init_value)
{
	struct panel_obj_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_obj_find_property(properties, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	name_len = strlen(name);

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
		return -EINVAL;
	}

	property = kzalloc(sizeof(struct panel_obj_property), GFP_KERNEL);
	if (!property)
		return -ENOMEM;

	memcpy(property->name, name, name_len);
	property->value = init_value;
	property->type = PANEL_OBJ_PROP_TYPE_VALUE;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_value: %d.\n", property->name, property->value);

	return 0;
}

int panel_obj_add_property_str(struct panel_obj_properties *properties, char *name, char *str)
{
	struct panel_obj_property *property;
	int name_len;

	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	if (!name) {
		panel_err("name is null.\n");
		return -EINVAL;
	}

	if (panel_obj_find_property(properties, name)) {
		panel_err("same name property is exist.\n");
		return -EINVAL;
	}

	name_len = strlen(name);

	if (name_len > MAX_NAME_LEN) {
		panel_err("name is too long(len: %d max: %d)\n", name_len, MAX_NAME_LEN);
		return -EINVAL;
	}

	property = kzalloc(sizeof(struct panel_obj_property), GFP_KERNEL);
	if (!property)
		return -ENOMEM;

	memcpy(property->name, name, name_len);
	property->str = str;
	property->type = PANEL_OBJ_PROP_TYPE_STR;

	list_add(&property->head, &properties->list);

	panel_info("added: %s init_str: %s.\n", property->name, property->str);

	return 0;
}

int panel_obj_init(struct panel_obj_properties *properties)
{
	if (!properties) {
		panel_err("properties is null.\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&properties->list);

	/* add properties here */

	if (panel_obj_add_property_value(properties, PANEL_OBJ_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO) < 0) {
		panel_err("err. (%s)\n", PANEL_OBJ_PROPERTY_WAIT_TX_DONE);
		return -EINVAL;
	}

	return 0;
}

MODULE_DESCRIPTION("obj driver for panel");
MODULE_LICENSE("GPL");
