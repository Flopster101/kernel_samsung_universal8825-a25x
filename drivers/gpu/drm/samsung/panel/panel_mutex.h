/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_MUTEX_H__
#define __PANEL_MUTEX_H__

#include <linux/mutex.h>

#define PANEL_MUTEX_MAGIC 0x12345678
struct panel_mutex {
	void *ctx;
	unsigned int magic;
	struct mutex base;
};

void panel_mutex_init(void *ctx, struct panel_mutex *panel_lock);
void panel_mutex_lock(struct panel_mutex *panel_lock);
void panel_mutex_unlock(struct panel_mutex *panel_lock);

#endif /* __PANEL_MUTEX_H__ */
