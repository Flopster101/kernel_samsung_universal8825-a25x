/* SPDX-License-Identifier: GPL-2.0 */
/*
 * usbpd_log.h - Driver for the usbpd
 *
 */

#ifndef __USBPD_LOG_H__
#define __USBPD_LOG_H__

#ifdef CONFIG_USB_USING_ADVANCED_USBLOG
#include <linux/usblog_proc_notify.h>

#define usbpd_info(fmt, ...)				\
({										\
	pr_info(fmt, ##__VA_ARGS__);		\
	printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
})
#define usbpd_err(fmt, ...)				\
({										\
	pr_err(fmt, ##__VA_ARGS__);			\
	printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
})
#else
#define usbpd_info(fmt, ...)				\
({										\
	pr_info(fmt, ##__VA_ARGS__);		\
})
#define usbpd_err(fmt, ...)				\
({										\
	pr_err(fmt, ##__VA_ARGS__);			\
})
#endif

#endif /* __USBPD_LOG_H__ */
