/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_drv.h"
#include "../panel_bl.h"
#include "../panel_debug.h"
#include "../panel_property.h"
#include "oled_property.h"

static struct panel_prop_enum_item oled_br_hbm_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(OLED_BR_HBM_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(OLED_BR_HBM_ON),
};

static struct panel_prop_enum_item oled_prev_br_hbm_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(OLED_PREV_BR_HBM_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(OLED_PREV_BR_HBM_ON),
};

static int oled_br_hbm_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			is_hbm_brightness(panel_bl, panel_bl->props.brightness));
}

static int oled_prev_br_hbm_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			is_hbm_brightness(panel_bl, panel_bl->props.prev_brightness));
}

static int oled_tset_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_info *panel_data = &panel->panel_data;

	return panel_property_set_value(prop,
			(panel_data->props.temperature < 0) ?
			BIT(7) | abs(panel_data->props.temperature) :
			panel_data->props.temperature);
}

struct panel_prop_list oled_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(OLED_BR_HBM_PROPERTY,
			OLED_BR_HBM_OFF, oled_br_hbm_enum_items,
			oled_br_hbm_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(OLED_PREV_BR_HBM_PROPERTY,
			OLED_BR_HBM_OFF, oled_prev_br_hbm_enum_items,
			oled_prev_br_hbm_property_update),
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_TSET_PROPERTY,
			0, 0, 256, oled_tset_property_update),
};
EXPORT_SYMBOL(oled_property_array);

unsigned int oled_property_array_size = ARRAY_SIZE(oled_property_array);
EXPORT_SYMBOL(oled_property_array_size);

MODULE_DESCRIPTION("oled_property driver");
MODULE_LICENSE("GPL");
