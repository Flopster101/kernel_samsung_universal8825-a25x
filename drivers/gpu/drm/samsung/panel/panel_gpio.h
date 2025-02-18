/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_GPIO_H__
#define __PANEL_GPIO_H__

#include <linux/interrupt.h>

#define call_panel_gpio_func(q, _f, args...)              \
    (((q) && (q)->funcs && (q)->funcs->_f) ? ((q)->funcs->_f(q, ##args)) : 0)

struct panel_gpio_funcs;

enum panel_gpio_state {
	PANEL_GPIO_ABNORMAL_STATE,
	PANEL_GPIO_NORMAL_STATE,
	MAX_PANEL_GPIO_STATE,
};

struct panel_gpio {
	const char *name;
	int num;
	bool active_low; /* 0:HIGH ACTIVE(NORMAL LOW), 1:LOW ACTIVE(NORMAL HIGH) */
	int dir; /* 0:OUT, 1:IN */
	int irq;
	unsigned int irq_type;
	void __iomem *irq_pend_reg;
	int irq_pend_bit;
	bool irq_registered;
	bool irq_enable;
	struct panel_gpio_funcs *funcs;
	struct list_head head;
};

struct panel_gpio_funcs {
	bool (*is_valid)(struct panel_gpio *);
	int (*get_num)(struct panel_gpio *);
	const char *(*get_name)(struct panel_gpio *);
	int (*set_value)(struct panel_gpio *, int);
	int (*get_value)(struct panel_gpio *);
	int (*get_state)(struct panel_gpio *);

	/* panel gpio irq */
	bool (*is_irq_valid)(struct panel_gpio *);
	int (*get_irq_num)(struct panel_gpio *);
	int (*get_irq_type)(struct panel_gpio *);
	int (*enable_irq)(struct panel_gpio *);
	int (*disable_irq)(struct panel_gpio *);
	int (*clear_irq_pending_bit)(struct panel_gpio *);
	bool (*is_irq_enabled)(struct panel_gpio *);
	int (*devm_request_irq)(struct panel_gpio *,
			struct device *, irq_handler_t, const char *, void *);
};

bool panel_gpio_helper_is_valid(struct panel_gpio *gpio);
int panel_gpio_helper_get_num(struct panel_gpio *gpio);
const char *panel_gpio_helper_get_name(struct panel_gpio *gpio);
int panel_gpio_helper_set_value(struct panel_gpio *gpio, int value);
int panel_gpio_helper_get_value(struct panel_gpio *gpio);
int panel_gpio_helper_get_state(struct panel_gpio *gpio);
bool panel_gpio_helper_is_irq_valid(struct panel_gpio *gpio);
int panel_gpio_helper_get_irq_num(struct panel_gpio *gpio);
int panel_gpio_helper_get_irq_type(struct panel_gpio *gpio);
int panel_gpio_helper_enable_irq(struct panel_gpio *gpio);
int panel_gpio_helper_disable_irq(struct panel_gpio *gpio);
int panel_gpio_helper_clear_irq_pending_bit(struct panel_gpio *gpio);
bool panel_gpio_helper_is_irq_enabled(struct panel_gpio *gpio);
int panel_gpio_helper_devm_request_irq(struct panel_gpio *gpio,
		struct device *dev, irq_handler_t handler, const char *devname, void *dev_id);
int of_get_panel_gpio(struct device_node *np, struct panel_gpio *gpio);
struct panel_gpio *panel_gpio_create(void);
void panel_gpio_destroy(struct panel_gpio *gpio);

#endif /* __PANEL_GPIO_H__ */
