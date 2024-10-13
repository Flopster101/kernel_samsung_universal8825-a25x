/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_TESTMODE_H__
#define __PANEL_TESTMODE_H__

#include "panel_drv.h"

enum {
	PANEL_TESTMODE_OFF = 0,
	PANEL_TESTMODE_ON = 1,
};

struct panel_testmode_set {
	char cmd[16];
	void (*func)(struct panel_device *panel);
	void (*func_arg)(struct panel_device *panel, int arg);
};

#define call_panel_testmode_original_func(q, _f, args...)              \
	(((q) && (q)->testmode.funcs_original && (q)->testmode.funcs_original->_f) ? \
	((q)->testmode.funcs_original->_f(q, ##args)) : 0)

int panel_testmode_command(struct panel_device *panel, char *command);
void panel_testmode_on(struct panel_device *panel);
void panel_testmode_off(struct panel_device *panel);
int panel_testmode_dummy_func(struct panel_device *panel, int ret);

static inline int panel_testmode_dummy(struct panel_device *panel)
{
	return panel_testmode_dummy_func(panel, 0);
}

static inline int panel_testmode_dummy_void(struct panel_device *panel, void *arg1)
{
	return panel_testmode_dummy_func(panel, 0);
}

static inline int panel_testmode_dummy_int_void(struct panel_device *panel, int arg1, void *arg2)
{
	return panel_testmode_dummy_func(panel, 0);
}

bool panel_testmode_is_on(struct panel_device *panel);

#endif
