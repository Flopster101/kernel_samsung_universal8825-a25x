/* SPDX-License-Identifier: GPL-2.0 */
/*
 * s2mf301_log.h - Driver for the s2mf301
 *
 */

#ifndef __S2MF301_LOG_H__
#define __S2MF301_LOG_H__

#ifdef CONFIG_USB_USING_ADVANCED_USBLOG
#include <linux/usblog_proc_notify.h>

#define s2mf301_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#define s2mf301_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#else
#define s2mf301_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
	})
#define s2mf301_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
	})
#endif

#endif /* __S2MF301_LOG_H__ */
