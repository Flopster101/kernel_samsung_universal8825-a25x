/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_FIRMWARE_H__
#define __PANEL_FIRMWARE_H__

enum {
	PANEL_FIRMWARE_LOAD_STATUS_FAILURE,
	PANEL_FIRMWARE_LOAD_STATUS_SUCCESS,
	MAX_PANEL_FIRMWARE_LOAD_STATUS
};

#define PANEL_BUILT_IN_FW_NAME ("built-in")

struct panel_firmware {
	char *name;
	u32 load_count;
	u32 load_status;
	struct timespec64 time;
	u32 csum;
};

void panel_firmware_set_name(struct panel_device *panel, char *name);
char *panel_firmware_get_name(struct panel_device *panel);
u32 panel_firmware_get_load_status(struct panel_device *panel);
bool is_panel_firmwarel_load_success(struct panel_device *panel);
int panel_firmware_get_load_count(struct panel_device *panel);
struct timespec64 panel_firmware_get_load_time(struct panel_device *panel);
u64 panel_firmware_get_csum(struct panel_device *panel);
#if defined(CONFIG_USDM_PANEL_JSON)
int panel_firmware_load(struct panel_device *panel,
		char *firmware_name, const char *ezop_json, struct list_head *pnobj_list);
#endif

#endif /* __PANEL_FIRMWARE_H__ */
