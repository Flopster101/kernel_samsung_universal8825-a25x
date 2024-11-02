/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_CONFIG_H__
#define __PANEL_CONFIG_H__

#include "panel_property.h"

struct panel_device;

struct pnobj_config {
	struct pnobj base;
	struct panel_property *prop;
	char prop_name[PANEL_PROP_NAME_LEN];
	unsigned int value;
};

#define __PNOBJ_CONFIG_INITIALIZER(_name, _prop_name, _value) \
	{ .base = __PNOBJ_INITIALIZER(_name, CMD_TYPE_CFG) \
	, .prop = (NULL) \
	, .prop_name = (_prop_name) \
	, .value = (_value) }

#define DEFINE_PNOBJ_CONFIG(_name, _prop_name, _value) \
struct pnobj_config PN_CONCAT(pnobj_config_, _name) = \
	__PNOBJ_CONFIG_INITIALIZER(_name, _prop_name, _value)

#define PNOBJ_CONFIG(_name_) PN_CONCAT(pnobj_config_, _name_)

int panel_apply_pnobj_config(struct panel_device *panel,
		struct pnobj_config *pnobj_config);

static inline char *get_pnobj_config_name(struct pnobj_config *prop)
{
	return get_pnobj_name(&prop->base);
}

struct pnobj_config *create_pnobj_config(char *name,
		char *prop_name, unsigned int value);
struct pnobj_config *duplicate_pnobj_config(struct pnobj_config *src);
void destroy_pnobj_config(struct pnobj_config *cfg);

#endif /* __PANEL_CONFIG_H__ */
