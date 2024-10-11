// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/list.h>
#include <linux/firmware.h>
#include <linux/time64.h>
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_obj.h"
#include "panel_function.h"
#if defined(CONFIG_USDM_PANEL_JSON)
#include "ezop/panel_json.h"
#endif

void panel_firmware_set_name(struct panel_device *panel, char *name)
{
	if (!panel)
		return;

	kfree(panel->fw.name);
	panel->fw.name = kstrndup(name, PNOBJ_NAME_LEN-1, GFP_KERNEL);
}

char *panel_firmware_get_name(struct panel_device *panel)
{
	if (!panel)
		return NULL;

	return panel->fw.name;
}

void panel_firmware_inc_load_count(struct panel_device *panel)
{
	if (!panel)
		return;

	panel->fw.load_count++;
}

void panel_firmware_clr_load_count(struct panel_device *panel)
{
	if (!panel)
		return;

	panel->fw.load_count = 0;
}

int panel_firmware_get_load_count(struct panel_device *panel)
{
	if (!panel)
		return -EINVAL;

	return panel->fw.load_count;
}

void panel_firmware_set_load_status(struct panel_device *panel, unsigned int status)
{
	if (!panel)
		return;

	panel->fw.load_status = status;
}

u32 panel_firmware_get_load_status(struct panel_device *panel)
{
	if (!panel)
		return false;

	return panel->fw.load_status;
}

bool is_panel_firmwarel_load_success(struct panel_device *panel)
{
	return panel_firmware_get_load_status(panel) ==
		PANEL_FIRMWARE_LOAD_STATUS_SUCCESS;
}

static u32 calc_csum(const void *p, size_t len)
{
	u32 csum = 0;
	const unsigned char *_p = p;
	int i;
	
	for (i = 0; i < len; i++)
		csum = csum + (*_p++);

	return csum;
}

static void panel_firmware_set_csum(struct panel_device *panel, u32 csum)
{
	if (!panel)
		return;

	panel->fw.csum = csum;
}

u64 panel_firmware_get_csum(struct panel_device *panel)
{
	return panel->fw.csum;
}

static void panel_firmware_set_load_time(struct panel_device *panel,
		struct timespec64 time)
{
	panel->fw.time = time;
}

struct timespec64 panel_firmware_get_load_time(struct panel_device *panel)
{
	return panel->fw.time;
}

int panel_firmware_load(struct panel_device *panel,
		char *firmware_name, struct list_head *pnobj_list)
{
	const struct firmware *fw_entry = NULL;
	struct pnobj *pos, *next;
	char *json = NULL;
	int ret;
	u32 csum;
	struct timespec64 time;

	ret = request_firmware_direct(&fw_entry, firmware_name, panel->dev);
	if (ret < 0) {
		panel_err("failed to request firmware(%s)\n", firmware_name);
		return ret;
	}

	panel_info("firmware:%s size:%ld\n", firmware_name, fw_entry->size);

	json = kvmalloc(fw_entry->size, GFP_KERNEL);
	if (!json) {
		panel_err("failed to alloc json buffer\n");
		ret = -ENOMEM;
		goto err;
	}
	memcpy(json, fw_entry->data, fw_entry->size);
	csum = calc_csum(json, fw_entry->size);

#if defined(CONFIG_USDM_PANEL_JSON)
	ret = panel_json_parse(json, fw_entry->size, pnobj_list);
	if (ret < 0)
		goto err;
#endif

	kvfree(json);
	release_firmware(fw_entry);

	panel_firmware_set_name(panel, firmware_name);
	panel_firmware_set_load_status(panel, PANEL_FIRMWARE_LOAD_STATUS_SUCCESS);
	ktime_get_ts64(&time);
	panel_firmware_set_load_time(panel, time);
	panel_firmware_inc_load_count(panel);
	panel_firmware_set_csum(panel, csum);

	panel_info("firmware(%s) has been loaded successfully\n", firmware_name);

	return 0;

err:
	list_for_each_entry_safe(pos, next, pnobj_list, list)
		destroy_panel_object(pos);
	kvfree(json);
	release_firmware(fw_entry);
	panel_firmware_set_load_status(panel, PANEL_FIRMWARE_LOAD_STATUS_FAILURE);

	panel_info("failed to load firmware(%s) (ret:%d)\n", firmware_name, ret);

	return ret;
}

MODULE_DESCRIPTION("firmware driver for panel");
MODULE_LICENSE("GPL");
