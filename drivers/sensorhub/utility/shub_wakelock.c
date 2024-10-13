/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "shub_wakelock.h"

static struct wakeup_source *ws;

void shub_wake_lock_register(void)
{
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE) || defined(CONFIG_SHUB_PM_WAKEUP_LINUX_VER_5_4) || defined(CONFIG_SSP_PM_WAKEUP_LINUX_VER_5_4)
	ws = wakeup_source_register(get_shub_device(), "shub_wake_lock");
#else
	ws = wakeup_source_register("shub_wake_lock");
#endif
}

void shub_wake_lock_unregister(void)
{
	wakeup_source_unregister(ws);
}

void shub_wake_lock(void)
{
	__pm_stay_awake(ws);
}

void shub_wake_lock_timeout(unsigned int msec)
{
	__pm_wakeup_event(ws, msec);
}

void shub_wake_unlock(void)
{
	__pm_relax(ws);
}
