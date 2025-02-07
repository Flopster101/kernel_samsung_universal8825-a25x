/*
 * sm5714_log.h - Driver for the sm5714
 *
 */

#ifndef __SM5714_LOG_H__
#define __SM5714_LOG_H__

#include <linux/usblog_proc_notify.h>

#ifdef CONFIG_USB_USING_ADVANCED_USBLOG
#define sm5714_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#define sm5714_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
		printk_usb(NOTIFY_PRINTK_USB_NORMAL, fmt, ##__VA_ARGS__);	\
	})
#else
#define sm5714_info(fmt, ...)				\
	({										\
		pr_info(fmt, ##__VA_ARGS__);		\
	})
#define sm5714_err(fmt, ...)				\
	({										\
		pr_err(fmt, ##__VA_ARGS__);			\
	})
#endif

#endif /* __SM5714_LOG_H__ */
