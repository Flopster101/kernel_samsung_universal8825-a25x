/* SPDX-License-Identifier: GPL-2.0 */
/*
 * <linux/usb/f_dm.h> -- USB F_DM extern function definitions.
 * Copyright (C) 2021 Samsung Electronics.
 *
 */
#ifndef __LINUX_USB_DM_H
#define __LINUX_USB_DM_H
extern int init_dm_direct_path(int req_num,
			void (*check_status)(void *addr, int length, void *context),
			void (*active_noti)(void *context),
			void (*disable_noti)(void *context),
			void *context);
extern int usb_dm_request(void *buf, unsigned int length);
#endif /* __LINUX_USB_DM_H */
