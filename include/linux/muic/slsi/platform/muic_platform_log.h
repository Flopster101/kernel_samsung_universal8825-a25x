/* SPDX-License-Identifier: GPL-2.0 */
/*
 * muic_platform_log.h - Driver for the muic_platform_layer
 *
 */

#ifndef __MUIC_PLATFORM_LAYER_LOG_H__
#define __MUIC_PLATFORM_LAYER_LOG_H__

#ifdef CONFIG_USB_USING_ADVANCED_USBLOG
#include <linux/usblog_proc_notify.h>

#define mpl_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#define mpl_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#else
#define mpl_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
	})
#define mpl_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
	})
#endif

#endif /* __MUIC_PLATFORM_LAYER_LOG_H__ */
