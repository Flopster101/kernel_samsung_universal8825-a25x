// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_mutex.h"

void panel_mutex_init(void *ctx, struct panel_mutex *panel_lock)
{
	if (!ctx)
		return;

	panel_lock->ctx = ctx;
	panel_lock->magic = PANEL_MUTEX_MAGIC;
	mutex_init(&panel_lock->base);
}

void panel_mutex_lock(struct panel_mutex *panel_lock)
{
	static struct panel_device *panel;

	if (!panel)
		panel = panel_lock->ctx;

	if (!panel)
		BUG();

	if (panel_lock->magic != PANEL_MUTEX_MAGIC)
		BUG();

#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	if (current->pid != panel->commit_thread_pid) {
		mutex_lock(&panel->panel_mutex_op_lock);
		if (panel->panel_mutex_depth == 0)
			mutex_lock(&panel->panel_mutex_big_lock);
		panel->panel_mutex_depth++;
		mutex_unlock(&panel->panel_mutex_op_lock);
	}
#endif
	mutex_lock(&panel_lock->base);
}

void panel_mutex_unlock(struct panel_mutex *panel_lock)
{
	static struct panel_device *panel;

	if (!panel)
		panel = panel_lock->ctx;

	if (!panel)
		BUG();

	if (panel_lock->magic != PANEL_MUTEX_MAGIC)
		BUG();

	mutex_unlock(&panel_lock->base);

#if IS_ENABLED(CONFIG_USDM_PANEL_BIG_LOCK)
	if (current->pid != panel->commit_thread_pid) {
		mutex_lock(&panel->panel_mutex_op_lock);
		panel->panel_mutex_depth--;
		if (panel->panel_mutex_depth == 0)
			mutex_unlock(&panel->panel_mutex_big_lock);
		mutex_unlock(&panel->panel_mutex_op_lock);
	}
#endif
}

MODULE_DESCRIPTION("mutex driver for panel");
MODULE_LICENSE("GPL");
