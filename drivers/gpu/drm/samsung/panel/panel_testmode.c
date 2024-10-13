/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "panel.h"
#include "panel_drv.h"
#include "panel_bl.h"
#include "panel_debug.h"
#include "panel_testmode.h"

struct panel_drv_funcs testmode_dummy_funcs = {
	.register_cb = panel_testmode_dummy_int_void,
	.register_error_cb = panel_testmode_dummy_void,
	.get_panel_state = panel_testmode_dummy_void,
	.attach_adapter = panel_testmode_dummy_void,

	.probe = panel_testmode_dummy,
	.sleep_in = panel_testmode_dummy,
	.sleep_out = panel_testmode_dummy,
	.display_on = panel_testmode_dummy,
	.display_off = panel_testmode_dummy,
	.power_on = panel_testmode_dummy,
	.power_off = panel_testmode_dummy,

#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	.lock = panel_testmode_dummy,
	.unlock = panel_testmode_dummy,
#endif

	.debug_dump = panel_testmode_dummy,
#ifdef CONFIG_USDM_PANEL_LPM
	.doze = panel_testmode_dummy,
	.doze_suspend = panel_testmode_dummy,
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.set_display_mode = panel_testmode_dummy_void,
	.get_display_mode = panel_get_display_mode,
#endif
	.reset_lp11 = panel_testmode_dummy,

	.frame_done = panel_testmode_dummy_void,
	.vsync = panel_testmode_dummy_void,
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	.set_mask_layer = panel_testmode_dummy_void,
#endif
	.req_set_clock = panel_testmode_dummy_void,
	.get_ddi_props = panel_testmode_dummy_void,
	.get_rcd_info = panel_testmode_dummy_void,
};

bool panel_testmode_is_on(struct panel_device *panel)
{
	if (panel && panel->testmode.enabled)
		return true;
	return false;
}

void panel_testmode_on(struct panel_device *panel)
{
	if (panel->testmode.enabled) {
		panel_info("[TESTMODE] testmode is already ON.\n");
		return;
	}
	panel_info("[TESTMODE] testmode set to: ON\n");
	panel->testmode.enabled = PANEL_TESTMODE_ON;
	panel_dsi_set_bypass(panel, true);
	panel->testmode.funcs_original = panel->funcs;
	panel->funcs = &testmode_dummy_funcs;
}

void panel_testmode_off(struct panel_device *panel)
{
	if (!panel->testmode.enabled) {
		panel_info("[TESTMODE] testmode is already OFF.\n");
		return;
	}
	panel_info("[TESTMODE] testmode set to: OFF\n");
	panel->testmode.enabled = PANEL_TESTMODE_OFF;
	panel_dsi_set_bypass(panel, false);
	panel->funcs = panel->testmode.funcs_original;
}

void panel_testmode_display_on(struct panel_device *panel)
{
	call_panel_testmode_original_func(panel, display_on);
}

void panel_testmode_display_off(struct panel_device *panel)
{
	call_panel_testmode_original_func(panel, display_off);
}

void panel_testmode_sleep_out(struct panel_device *panel)
{
	call_panel_testmode_original_func(panel, sleep_out);
}

void panel_testmode_sleep_in(struct panel_device *panel)
{
	call_panel_testmode_original_func(panel, sleep_in);
}

void panel_testmode_doze(struct panel_device *panel)
{
	call_panel_testmode_original_func(panel, doze);
}

void panel_testmode_brightness(struct panel_device *panel, int arg)
{
	panel_update_subdev_brightness(panel, PANEL_BL_SUBDEV_TYPE_DISP, arg);
	panel_bl_set_brightness(&panel->panel_bl, panel->panel_bl.props.id, SEND_CMD);
}

int panel_testmode_dummy_func(struct panel_device *panel, int ret)
{
	panel_info("[TESTMODE] panel test mode is ON\n");
	return ret;
}

struct panel_testmode_set panel_testmode_sets[] = {
	{ .cmd = "DISPLAY_ON", .func = panel_testmode_display_on },
	{ .cmd = "DISPLAY_OFF", .func = panel_testmode_display_off },
	{ .cmd = "SLEEP_OUT", .func = panel_testmode_sleep_out },
	{ .cmd = "SLEEP_IN", .func = panel_testmode_sleep_in },
	{ .cmd = "DOZE", .func = panel_testmode_doze },
	{ .cmd = "DOZE_SUSPEND", .func = panel_testmode_doze },
	{ .cmd = "BRIGHTNESS", .func_arg = panel_testmode_brightness },
};

int panel_testmode_command(struct panel_device *panel, char *buf)
{
	int i, intarg, rc;
	char buffer[64] = { 0, };
	char *command, *pos;

	if (!panel || !buf)
		return -EINVAL;

	strncpy(buffer, buf, min(strlen(buf), ARRAY_SIZE(buffer) - 1));

	pos = buffer;
	command = strsep(&pos, ",");

	if (!command) {
		panel_err("[TESTMODE] command is null\n");
		return -EINVAL;
	}

	if (!strncmp(command, "ON", 2)) {
		panel_testmode_on(panel);
		return 0;
	}
	if (!strncmp(command, "OFF", 3)) {
		panel_testmode_off(panel);
		return 0;
	}
	if (!panel_testmode_is_on(panel))
		return -EBUSY;

	for (i = 0; i < ARRAY_SIZE(panel_testmode_sets); i++) {
		if (strlen(panel_testmode_sets[i].cmd) == strlen(command) &&
			!strncmp(panel_testmode_sets[i].cmd, command, strlen(panel_testmode_sets[i].cmd)))
			break;
	}

	if (i == ARRAY_SIZE(panel_testmode_sets)) {
		panel_err("[TESTMODE] unknown command: %s\n", command);
		return -EINVAL;
	}

	if (panel_testmode_sets[i].func_arg) {
		if (!pos) {
			panel_err("[TESTMODE] command %s: need 1 argument\n", command);
			return -EINVAL;
		}
		rc = kstrtoint(pos, 10, &intarg);
		if (rc < 0) {
			panel_err("[TESTMODE] command %s: need integer argument\n", command);
			return -EINVAL;
		}
		panel_testmode_sets[i].func_arg(panel, intarg);
		return 0;
	}

	if (panel_testmode_sets[i].func) {
		panel_testmode_sets[i].func(panel);
		return 0;
	}

	panel_err("[TESTMODE] command %s: invalid function table\n", command);
	return -EINVAL;
}
