/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GNSS_UTILS_H__
#define __GNSS_UTILS_H__

#include "gnss_prj.h"

/* #define DEBUG_GNSS_IPC_PKT	1 */

struct __packed gnss_log {
	u8 fmt_msg;
	u8 boot_msg;
	u8 dump_msg;
	u8 rfs_msg;
	u8 log_msg;
	u8 ps_msg;
	u8 router_msg;
	u8 debug_log;
};

extern struct gnss_log log_info;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static const char * const direction_string[] = {
	[TX] = "TX",
	[RX] = "RX"
};
#else
static const char * const direction_string[] = {
	[TX] = "TX",
	[RX] = "RX"
};
#endif

static const inline char *dir_str(enum direction dir)
{
	if (unlikely(dir >= MAX_DIR))
		return "INVALID";
	else
		return direction_string[dir];
}

/* gnss wake lock */
static inline struct wakeup_source *gnssif_wake_lock_register(struct device *dev, const char *name)
{
	struct wakeup_source *ws = NULL;

	ws = wakeup_source_register(dev, name);
	if (ws == NULL) {
		gif_err("%s: wakelock register fail\n", name);
		return NULL;
	}

	return ws;
}

static inline void gnssif_wake_lock_unregister(struct wakeup_source *ws)
{
	if (ws == NULL) {
		gif_err("wakelock unregister fail\n");
		return;
	}

	wakeup_source_unregister(ws);
}

static inline void gnssif_wake_lock(struct wakeup_source *ws)
{
	if (ws == NULL) {
		gif_err("wakelock fail\n");
		return;
	}

	__pm_stay_awake(ws);
}

static inline void gnssif_wake_lock_timeout(struct wakeup_source *ws, long timeout)
{
	if (ws == NULL) {
		gif_err("wakelock timeout fail\n");
		return;
	}

	__pm_wakeup_event(ws, jiffies_to_msecs(timeout));
}

static inline void gnssif_wake_unlock(struct wakeup_source *ws)
{
	if (ws == NULL) {
		gif_err("wake unlock fail\n");
		return;
	}

	__pm_relax(ws);
}

static inline int gnssif_wake_lock_active(struct wakeup_source *ws)
{
	if (ws == NULL) {
		gif_err("wakelock active fail\n");
		return 0;
	}

	return ws->active;
}

#if defined(DEBUG_GNSS_IPC_PKT)
/* print IPC message packet */
void gnss_log_ipc_pkt(struct sk_buff *skb, enum direction dir);
#endif

/* get gnssif version */
const char *get_gnssif_driver_version(void);

/* gnss irq */
void gif_init_irq(struct gnss_irq *irq, unsigned int num, const char *name,
			unsigned long flags);
int gif_request_irq(struct gnss_irq *irq, irq_handler_t isr, void *data);
void gif_enable_irq(struct gnss_irq *irq);
void gif_disable_irq_nosync(struct gnss_irq *irq);
void gif_disable_irq_sync(struct gnss_irq *irq);
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
int gif_gpio_get_value(unsigned int gpio, bool log_print);
#endif

#endif/*__GNSS_UTILS_H__*/

