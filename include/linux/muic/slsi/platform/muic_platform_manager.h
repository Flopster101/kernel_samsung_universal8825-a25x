/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#ifndef __MUIC_PLATFORM_MANAGER_H__
#define __MUIC_PLATFORM_MANAGER_H__

#include <linux/muic/common/muic.h>
#include <linux/muic/slsi/platform/muic_platform_layer.h>

typedef enum {
	PDIC_RID_UNDEFINED = 0,
	PDIC_RID_000K,
	PDIC_RID_001K,
	PDIC_RID_255K,
	PDIC_RID_301K,
	PDIC_RID_523K,
	PDIC_RID_619K,
	PDIC_RID_OPEN,
} muic_pdic_rid_t;

struct pdic_rid_desc_t {
	char *name;
	int attached_dev;
};

struct pdic_desc_t {
	int pdic_evt_attached; /* 1: attached, -1: detached, 0: undefined */
	int pdic_evt_rid; /* the last rid */
	int pdic_evt_rprd; /*rprd */
	int pdic_evt_roleswap; /* check rprd role swap event */
	int pdic_evt_dcdcnt; /* count dcd timeout */
	int attached_dev; /* attached dev */
	struct pdic_rid_desc_t *rid_desc;
};

typedef enum {
	MUIC_NORMAL_OTG,
	MUIC_ABNORMAL_OTG,
} muic_usb_killer_t;

typedef enum {
	MUIC_PRSWAP_UNDIFINED,
	MUIC_PRSWAP_TO_SINK,
	MUIC_PRSWAP_TO_SRC,
} muic_prswap_t;

struct muic_interface_t {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;
	struct notifier_block nb;

	struct muic_share_data *sdata;
	struct pdic_desc_t *pdic;

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
	/* USB Notifier */
	struct notifier_block	usb_nb;
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct notifier_block	manager_nb;
#else
	struct notifier_block	pdic_nb;
#endif
	struct delayed_work	pdic_work;

	/* Operation Mode */
	enum muic_op_mode	opmode;
	bool is_water;
};

#define REQUEST_IRQ(_irq, _dev_id, _name, _func)			\
do {									\
	ret = request_threaded_irq(_irq, NULL, _func,			\
				0, _name, _dev_id);			\
	if (ret < 0) {							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s:%s IRQ(%d):%s free done\n", MUIC_DEV_NAME,	\
				__func__, _irq, _name);			\
	}								\
} while (0)

#if IS_ENABLED(CONFIG_MUIC_PLATFORM)
extern struct muic_interface_t *register_muic_platform_manager
	(struct muic_share_data *pdata);
extern void unregister_muic_platform_manager
		(struct muic_share_data *sdata);
#else
static inline struct muic_interface_t *register_muic_platform_manager
	(struct muic_share_data *pdata) {return NULL; }
static inline void unregister_muic_platform_manager
		(struct muic_share_data *sdata) {}
#endif

#endif /* __MUIC_PLATFORM_MANAGER_H__ */
