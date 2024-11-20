/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_OBJ_H__
#define __PANEL_OBJ_H__

#include "panel.h"

#define MAX_NAME_LEN (32)

/* wait_tx_done */
#define PANEL_OBJ_PROPERTY_WAIT_TX_DONE ("wait_tx_done")
enum {
	WAIT_TX_DONE_AUTO,  /* Wait TX DONE every end of cmdq set */
	WAIT_TX_DONE_MANUAL_OFF, /* DO NOT wait for TX DONE */
	WAIT_TX_DONE_MANUAL_ON, /* Wait TX DONE every each cmd */
};

enum PANEL_OBJ_PROP_TYPE {
	PANEL_OBJ_PROP_TYPE_VALUE,
	PANEL_OBJ_PROP_TYPE_STR,
	MAX_PANEL_OBJ_PROP_TYPE
};

struct panel_obj_property {
	char name[MAX_NAME_LEN + 1];
	enum PANEL_OBJ_PROP_TYPE type;
	union {
		unsigned int value;
		char *str;
	};
	struct list_head head;
};

struct panel_obj_properties {
	struct list_head list;
};

struct panel_obj_property *panel_obj_find_property(struct panel_obj_properties *properties, char *name);

int panel_obj_set_property_value(struct panel_obj_properties *properties, char *name, unsigned int value);
int panel_obj_get_property_value(struct panel_obj_properties *properties, char *name);
int panel_obj_add_property_value(struct panel_obj_properties *properties, char *name, unsigned int init_value);

int panel_obj_set_property_str(struct panel_obj_properties *properties, char *name, char *str);
char *panel_obj_get_property_str(struct panel_obj_properties *properties, char *name);
int panel_obj_add_property_str(struct panel_obj_properties *properties, char *name, char *str);

int panel_obj_delete_property(struct panel_obj_properties *properties, char *name);
int panel_obj_init(struct panel_obj_properties *properties);
#endif /* __PANEL_OBJ_H__ */
