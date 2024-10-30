/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2014-2019, Samsung Electronics.
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

#ifndef __GNSS_MBOX_H__
#define __GNSS_MBOX_H__

#include "include/gnss.h"

/* Registers */
#define EXYNOS_MBOX_MCUCTLR	0x0
#define EXYNOS_MBOX_INTGR0	0x8
#define EXYNOS_MBOX_INTCR0	0xc
#define EXYNOS_MBOX_INTMR0	0x10
#define EXYNOS_MBOX_INTSR0	0x14
#define EXYNOS_MBOX_INTMSR0	0x18
#define EXYNOS_MBOX_INTGR1	0x1c
#define EXYNOS_MBOX_INTCR1	0x20
#define EXYNOS_MBOX_INTMR1	0x24
#define EXYNOS_MBOX_INTSR1	0x28
#define EXYNOS_MBOX_INTMSR1	0x2c

/* Bit definition */
#define MBOX_MCUCTLR_MSWRST	(0)		/* MCUCTRL S/W Reset */

/* Shared register with 64 * 32 words */
#define MAX_MBOX_NUM	64

enum gnss_mbox_region {
	MBOX_REGION_GNSS,
	MAX_MBOX_REGION,
};

struct gnss_mbox_ipc_handler {
	void *data;
	irq_handler_t handler;
};

struct gnss_mbox_drv_data {
	char *name;
	u32 id;

	void __iomem *base;

	u32 num_shared_reg;
	u32 shared_reg_offset;
	bool use_sw_reset_reg;

	u32 registered_irq;
	unsigned long unmasked_irq;

	struct gnss_mbox_ipc_handler hd[16];
	spinlock_t lock;

	struct gnss_irq irq_gnss_mbox;
};

/* Functions */
extern int gnss_mbox_register_irq(enum gnss_mbox_region id, u32 int_num, irq_handler_t handler, void *data);
extern int gnss_mbox_unregister_irq(enum gnss_mbox_region id, u32 int_num, irq_handler_t handler);
extern int gnss_mbox_enable_irq(enum gnss_mbox_region id, u32 int_num);
extern int gnss_mbox_disable_irq(enum gnss_mbox_region id, u32 int_num);

extern void gnss_mbox_set_interrupt(enum gnss_mbox_region id, u32 int_num);

extern u32 gnss_mbox_get_sr(enum gnss_mbox_region id, u32 sr_num);
extern u32 gnss_mbox_extract_sr(enum gnss_mbox_region id, u32 sr_num, u32 mask, u32 pos);
extern void gnss_mbox_set_sr(enum gnss_mbox_region id, u32 sr_num, u32 msg);
extern void gnss_mbox_update_sr(enum gnss_mbox_region id, u32 sr_num, u32 msg, u32 mask, u32 pos);
extern void gnss_mbox_dump_sr(enum gnss_mbox_region id);

extern void gnss_mbox_sw_reset(enum gnss_mbox_region id);
extern void gnss_mbox_clear_all_interrupt(enum gnss_mbox_region id);

#endif /* __GNSS_MBOX_H__ */
