/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.c
 *
 * TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include <linux/sec_detect.h>
#include "hx83102e_gta4xls_00_panel.h"
#include "../usdm_panel_debug.h"

static int gta4xls_first_br_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	int index;

	if (atomic_read(&panel_bl->props.brightness_non_zero_set_count) == 1)
		index = GTA4XLS_FIRST_BR_ON;
	else
		index = GTA4XLS_FIRST_BR_OFF;

	if (index == GTA4XLS_FIRST_BR_ON)
		panel_info("first non-zero br!\n");

	return usdm_panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item gta4xls_first_br_enum_items[MAX_GTA4XLS_FIRST_BR] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GTA4XLS_FIRST_BR_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GTA4XLS_FIRST_BR_ON),
};


static struct panel_prop_list hx83102e_gta4xls_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(GTA4XLS_FIRST_BR_PROPERTY,
			GTA4XLS_FIRST_BR_OFF, gta4xls_first_br_enum_items,
			gta4xls_first_br_property_update),
};

static int __init hx83102e_gta4xls_00_panel_init(void)
{
	struct common_panel_info *cpi = &hx83102e_gta4xls_00_panel_info;

	if (sec_current_device != SEC_GTA4XLS) {
		SEC_DETECT_LOG("Skipped hx83102e_gta4xls_00 panel driver\n");
		return 0;
	}
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = hx83102e_gta4xls_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(hx83102e_gta4xls_property_array);

	usdm_register_common_panel(&hx83102e_gta4xls_00_panel_info);

	SEC_DETECT_LOG("Initialized hx83102e_gta4xls_00 panel driver\n");
	return 0;
}

static void __exit hx83102e_gta4xls_00_panel_exit(void)
{
	usdm_deregister_common_panel(&hx83102e_gta4xls_00_panel_info);
}

module_init(hx83102e_gta4xls_00_panel_init)
module_exit(hx83102e_gta4xls_00_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
