/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __LINUX_HOST_NOTIFY_CALSS_H__
#define __LINUX_HOST_NOTIFY_CALSS_H__

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
extern int notify_class_init(void);
extern void notify_class_exit(void);
#else
static inline int notify_class_init(void) {return 0;}
static inline void notify_class_exit(void) {}
#endif
#endif

