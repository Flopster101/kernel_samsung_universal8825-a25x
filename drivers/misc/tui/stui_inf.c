/* tui/stui_inf.c
 *
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include "stui_inf.h"

#define TUI_REE_EXTERNAL_EVENT	42
#define SESSION_CANCEL_DELAY	150
#define MAX_WAIT_CNT		10

#define TUIHW_LOG_TAG "tuill_hw"

static enum tui_version tui_version = TUI_NOPE;

static int tui_mode = STUI_MODE_OFF;
static int stui_touch_type;
static DEFINE_SPINLOCK(tui_lock);

#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
static DEFINE_SPINLOCK(iwdf_lock);

struct iwd_functions {
	int (*cancel_session)(void);
};

struct iwd_functions iwdf = {
	.cancel_session = NULL
};

void register_iwd_functions(struct iwd_functions *f)
{
	unsigned long fls;

	spin_lock_irqsave(&iwdf_lock, fls);
	iwdf.cancel_session = f->cancel_session;
	spin_unlock_irqrestore(&iwdf_lock, fls);
}
EXPORT_SYMBOL(register_iwd_functions);

void unregister_iwd_functions(void)
{
	unsigned long fls;

	spin_lock_irqsave(&iwdf_lock, fls);
	iwdf.cancel_session = NULL;
	spin_unlock_irqrestore(&iwdf_lock, fls);
}
EXPORT_SYMBOL(unregister_iwd_functions);
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */

int stui_get_mode(void)
{
	unsigned long fls;
	int ret_mode;

	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = tui_mode;
	spin_unlock_irqrestore(&tui_lock, fls);
	pr_debug(TUIHW_LOG_TAG " %s << ret_mode=%#X\n", __func__, ret_mode);
	return ret_mode;
}
EXPORT_SYMBOL(stui_get_mode);

void stui_set_mode(int mode)
{
	unsigned long fls;

	pr_debug(TUIHW_LOG_TAG " %s >> mode=%#X\n", __func__, mode);
	spin_lock_irqsave(&tui_lock, fls);
	tui_mode = mode;
	spin_unlock_irqrestore(&tui_lock, fls);
}
EXPORT_SYMBOL(stui_set_mode);

int stui_set_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	pr_debug(TUIHW_LOG_TAG " %s >> mask=%#X\n", __func__, mask);
	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode |= mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_set_mask);

int stui_clear_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	pr_debug(TUIHW_LOG_TAG " %s >> mask=%#X\n", __func__, mask);
	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode &= ~mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_clear_mask);

void stui_set_touch_type(uint32_t type)
{
	stui_touch_type = type;
}
EXPORT_SYMBOL(stui_set_touch_type);

int stui_get_touch_type(void)
{
	return stui_touch_type;
}
EXPORT_SYMBOL(stui_get_touch_type);

void stui_set_tui_version(enum tui_version version)
{
	tui_version = version;
}
EXPORT_SYMBOL(stui_set_tui_version);

enum tui_version stui_get_tui_version(void)
{
	return tui_version;
}
EXPORT_SYMBOL(stui_get_tui_version);

int stui_cancel_session(void)
{
#ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
	int ret = -1;
	if (tui_version == TUI_LL) {
		unsigned long fls;

		if (!(STUI_MODE_ALL & stui_get_mode())) {
			pr_debug(TUIHW_LOG_TAG " session cancel is not needed\n");
			return 0;
		}

		spin_lock_irqsave(&iwdf_lock, fls);
		if (iwdf.cancel_session != NULL)
			ret = iwdf.cancel_session();

		spin_unlock_irqrestore(&iwdf_lock, fls);
	} else {
		pr_debug(TUIHW_LOG_TAG " old tui session\n");
	}
	return ret;
#else /* CONFIG_SAMSUNG_TUI_LOWLEVEL */
	pr_err(TUIHW_LOG_TAG " %s not supported\n", __func__);
	return -1;
#endif /* CONFIG_SAMSUNG_TUI_LOWLEVEL */
}
EXPORT_SYMBOL(stui_cancel_session);

static int __init teegris_tui_inf_init(void)
{
	pr_info(TUIHW_LOG_TAG "=============== Running TEEgris TUI Inf ===============");
	return 0;
}

static void __exit teegris_tui_inf_exit(void)
{
	pr_info(TUIHW_LOG_TAG "Unloading teegris tui inf module.");
}

module_init(teegris_tui_inf_init);
module_exit(teegris_tui_inf_exit);

MODULE_AUTHOR("TUI Teegris");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TEEGRIS TUI");
