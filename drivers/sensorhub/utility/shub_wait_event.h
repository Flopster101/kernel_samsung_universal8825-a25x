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

#ifndef _SHUB_WAIT_EVENT_H__
#define _SHUB_WAIT_EVENT_H__

#include <linux/wait.h>

struct shub_waitevent {
	wait_queue_head_t waitqueue;
	atomic_t state;
};

int shub_wait_event_timeout(struct shub_waitevent *lock, int timeout);
void shub_lock_wait_event(struct shub_waitevent *lock);
void shub_wake_up_wait_event(struct shub_waitevent *lock);

#endif