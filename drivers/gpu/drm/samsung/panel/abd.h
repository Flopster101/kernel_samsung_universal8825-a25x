/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ABD_H__
#define __ABD_H__

#include <asm/unaligned.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#define ABD_LOG_MAX	100
#define ABD_STR_TMP	200	/* stack */

struct fb_ops;

struct str_log {
	u64 stamp;
	u64 ktime;

	//unsigned int size;
	//unsigned int value;
	char *print[1];
};

struct abd_str {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct str_log log[ABD_LOG_MAX];
};

struct pin_log {
	u64 stamp;
	u64 ktime;

	/* pin */
	unsigned int level;
	unsigned int onoff;
};

struct abd_pin {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct pin_log log[ABD_LOG_MAX];
};

struct abd_pin_info {
	const char *name;
	unsigned int irq;
	struct irq_desc *desc;
	int gpio;
	int level;
	int active_level;
	unsigned int active_depth;
	unsigned int enable;
	unsigned int index;
	unsigned int bug_flag;

	struct abd_pin p_first;
	struct abd_pin p_lcdon;
	struct abd_pin p_event;

	struct list_head event_chain;
	struct list_head enter_chain;
	struct list_head leave_chain;
};

struct abd_sub_info {
	struct list_head node;
	void *chain_data;
	union {
		irq_handler_t handler;
		notifier_fn_t notifier;
		int (*listener)(int id, void *data);
		int (*show)(struct seq_file *m, void *v);
	};
};

struct fto_log {
	u64 stamp;
	u64 ktime;

	/* fence */
	unsigned int winid;
};

struct abd_fto {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct fto_log log[ABD_LOG_MAX];
};

struct udr_log {
	u64 stamp;
	u64 ktime;

	/* underrun */
	unsigned long mif;
	unsigned long iint;
	unsigned long disp;
	unsigned long aclk;
	void *bts;
};

struct abd_udr {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct udr_log log[ABD_LOG_MAX];
};

struct abd_bit_info {
	unsigned int reg;
	unsigned int len;
	char **print;
	u64 expect;
	unsigned int offset;
	u64 invert;
	u64 mask;
	union {
		u64 result;
		void *result_buf;
		u8 *u8_buf;
	};
	union {
		unsigned int reserved;
		unsigned int dpui_key;
		char *name;
	};
};

struct bit_log {
	u64 stamp;
	u64 ktime;

	/* bit error */
	unsigned int size;
	unsigned int value;
	char *print[BITS_PER_BYTE * sizeof(u32)];	/* max: 32 bit */
};

struct abd_bit {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct bit_log log[ABD_LOG_MAX];
};

enum {
	ABD_PIN_PCD,
	ABD_PIN_DET,
	ABD_PIN_ERR,
	ABD_PIN_CON,
	ABD_PIN_LOG,	/* reserved for just log */
	ABD_PIN_MAX
};

struct abd_protect {
	struct abd_pin_info pin[ABD_PIN_MAX];

	struct abd_bit b_first;
	struct abd_bit b_event;

	struct abd_str s_event;

	struct list_head printer_list;

	struct notifier_block pin_early_notifier;
	struct notifier_block pin_after_notifier;

	unsigned int enable;
	struct notifier_block reboot_notifier;
	spinlock_t slock;

	struct workqueue_struct *blank_workqueue;
	struct work_struct blank_work;
	unsigned int blank_flag;

	struct notifier_block con_fb_notifier;
	struct fb_ops fbops;
	void *lcd_driver;

	unsigned int islcmconnected;
	unsigned int init_done;
	unsigned int boot_done;

	struct miscdevice misc_entry;
	struct mutex misc_lock;

	struct dentry *debugfs_root;
};

struct abd_pending {
	struct list_head node;
	struct seq_operations printer;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
static inline u64 get_unaligned_le24(const u8 *p)
{
	return (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16;
}
#endif

static inline u64 get_unaligned_le40(const u8 *p)
{
	return (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16 | (u64)p[3] << 24 | (u64)p[4] << 32;
}

static inline u64 get_unaligned_le48(const u8 *p)
{
	return (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16 | (u64)p[3] << 24 | (u64)p[4] << 32 | (u64)p[5] << 40;
}

static inline u64 get_unaligned_le56(const u8 *p)
{
	return (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16 | (u64)p[3] << 24 | (u64)p[4] << 32 | (u64)p[5] << 40 | (u64)p[6] << 48;
}

static inline u64 get_merged_value(const void *p, u8 byte)
{
	u64 value = 0;

	switch (byte) {
	case 1: value = *((u8 *)p);
		break;
	case 2: value = get_unaligned_le16(p);
		break;
	case 3: value = get_unaligned_le24(p);
		break;
	case 4: value = get_unaligned_le32(p);
		break;
	case 5: value = get_unaligned_le40(p);
		break;
	case 6: value = get_unaligned_le48(p);
		break;
	case 7: value = get_unaligned_le56(p);
		break;
	case 8: value = get_unaligned_le64(p);
		break;
	default:
		break;
	}

	return value;
}

extern unsigned int lcdtype;
extern int get_lk_boot_panel_id(void);

#if defined(CONFIG_MEDIATEK_SOLUTION) || defined(CONFIG_ARCH_MEDIATEK)
extern int mtkfb_debug_show(struct seq_file *m, void *unused);
#endif

extern int usdm_abd_init(void);
extern void usdm_abd_save_str(struct abd_protect *abd, const char *print);
extern void usdm_abd_save_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print);
extern void usdm_abd_mask_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print, u32 invert);
extern void usdm_abd_enable(struct abd_protect *abd, unsigned int enable);
extern int usdm_abd_pin_register_handler(struct abd_protect *abd, int id, irq_handler_t func, void *dev_id);
extern int usdm_abd_pin_register_refresh_handler(struct abd_protect *abd, int id);
extern int usdm_abd_register_printer(struct abd_protect *abd, int (*show)(struct seq_file *, void *), void *data);
extern void usdm_abd_con_register(struct abd_protect *abd);
extern void usdm_abd_blank(struct abd_protect *abd);
void usdm_abd_simple_print(struct abd_protect *abd, struct seq_file *m, void *unused);
#endif	/* __ABD_H__ */

