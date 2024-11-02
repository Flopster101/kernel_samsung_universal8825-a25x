// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <asm-generic/errno-base.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include "kernel/irq/internals.h"

#include "panel_gpio.h"
#include "panel_debug.h"

static bool panel_gpio_is_valid(struct panel_gpio *gpio)
{
	if (!gpio)
		return false;

	if (!gpio_is_valid(gpio->num)) {
		panel_err("invalid gpio number(%d)\n", gpio->num);
		return false;
	}

	if (!gpio->name) {
		panel_err("invalid gpio-%d name is null\n", gpio->num);
		return false;
	}

	return true;
}

static int panel_gpio_get_num(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	return gpio->num;
}

static const char *panel_gpio_get_name(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return NULL;

	return gpio->name;
}

static int panel_gpio_set_value(struct panel_gpio *gpio, int value)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	gpio_set_value(gpio->num, value);

	return 0;
}

static int panel_gpio_get_value(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	return gpio_get_value(gpio->num);
}

static int panel_gpio_get_state(struct panel_gpio *gpio)
{
	int value;

	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	value = panel_gpio_helper_get_value(gpio);
	if (value < 0)
		return value;

	/*
	 * ACTIVE_LOW(NORMAL_HIGH)
	 *   gpio:low - ABNORMAL
	 *   gpio:high - NORMAL
	 * ACTIVE_HIGH(NORMAL_LOW)
	 *   gpio:low - NORMAL
	 *   gpio:high - ABNORMAL
	 */
	if (gpio->active_low)
		return value ? PANEL_GPIO_NORMAL_STATE : PANEL_GPIO_ABNORMAL_STATE;
	else
		return value ? PANEL_GPIO_ABNORMAL_STATE : PANEL_GPIO_NORMAL_STATE;
}

static int panel_gpio_get_irq_num(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	return gpio->irq;
}

static int panel_gpio_get_irq_type(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_valid(gpio))
		return -EINVAL;

	return gpio->irq_type;
}

static bool panel_gpio_is_irq_valid(struct panel_gpio *gpio)
{
	return !(panel_gpio_get_irq_type(gpio) <= 0);
}

static int panel_gpio_enable_irq(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_irq_valid(gpio))
		return -EINVAL;

	if (gpio->irq_enable == true) {
		panel_err("unbalanced enable for irq(%s:%d)\n",
				panel_gpio_helper_get_name(gpio),
				panel_gpio_helper_get_irq_num(gpio));
		return 0;
	}

	enable_irq(gpio->irq);
	gpio->irq_enable = true;

	return 0;
}

static int panel_gpio_disable_irq(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_irq_valid(gpio))
		return -EINVAL;

	if (gpio->irq_enable == false) {
		panel_err("unbalanced disable for irq(%s:%d)\n",
				panel_gpio_helper_get_name(gpio),
				panel_gpio_helper_get_irq_num(gpio));
		return 0;
	}

	disable_irq(gpio->irq);
	gpio->irq_enable = false;

	return 0;
}

static int panel_gpio_clear_irq_pending_bit(struct panel_gpio *gpio)
{
	struct irq_desc *desc;
	int irq;

	if (!panel_gpio_helper_is_irq_valid(gpio))
		return -EINVAL;

	irq = panel_gpio_helper_get_irq_num(gpio);
	if (irq < 0)
		return -EINVAL;

	desc = irq_to_desc(irq);
	if (!desc) {
		panel_err("irq(%s:%d) not found\n",
				panel_gpio_helper_get_name(gpio), irq);
		return -EINVAL;
	}

	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}

	return 0;
};

static bool panel_gpio_is_irq_enabled(struct panel_gpio *gpio)
{
	if (!panel_gpio_helper_is_irq_valid(gpio))
		return false;

	return gpio->irq_enable;
}

static int panel_gpio_devm_request_irq(struct panel_gpio *gpio,
		struct device *dev, irq_handler_t handler, const char *devname, void *dev_id)
{
	int ret;

	if (!panel_gpio_helper_is_irq_valid(gpio))
		return -EINVAL;

	/* W/A: clear pending irq before request_irq */
	irq_set_irq_type(gpio->irq, gpio->irq_type);

	ret = devm_request_irq(dev, gpio->irq, handler,
			gpio->irq_type, devname, dev_id);
	if (ret < 0)
		return ret;

	gpio->irq_registered = true;
	gpio->irq_enable = true;

	return ret;
}

struct panel_gpio_funcs panel_gpio_funcs = {
	.is_valid = panel_gpio_is_valid,
	.get_num = panel_gpio_get_num,
	.get_name = panel_gpio_get_name,
	.set_value = panel_gpio_set_value,
	.get_value = panel_gpio_get_value,
	.get_state = panel_gpio_get_state,

	/* panel gpio irq */
	.is_irq_valid = panel_gpio_is_irq_valid,
	.get_irq_num = panel_gpio_get_irq_num,
	.get_irq_type = panel_gpio_get_irq_type,
	.enable_irq = panel_gpio_enable_irq,
	.disable_irq = panel_gpio_disable_irq,
	.clear_irq_pending_bit = panel_gpio_clear_irq_pending_bit,
	.is_irq_enabled = panel_gpio_is_irq_enabled,
	.devm_request_irq = panel_gpio_devm_request_irq,
};

bool panel_gpio_helper_is_valid(struct panel_gpio *gpio)
{
	if (!gpio)
		return false;

	return call_panel_gpio_func(gpio, is_valid);
}

int panel_gpio_helper_get_num(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, get_num);
}

const char *panel_gpio_helper_get_name(struct panel_gpio *gpio)
{
	if (!gpio)
		return NULL;

	return call_panel_gpio_func(gpio, get_name);
}

int panel_gpio_helper_set_value(struct panel_gpio *gpio, int value)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, set_value, value);
}

int panel_gpio_helper_get_value(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, get_value);
}

int panel_gpio_helper_get_state(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, get_state);
}

bool panel_gpio_helper_is_irq_valid(struct panel_gpio *gpio)
{
	if (!gpio)
		return false;

	return call_panel_gpio_func(gpio, is_irq_valid);
}

int panel_gpio_helper_get_irq_num(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, get_irq_num);
}

int panel_gpio_helper_get_irq_type(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, get_irq_type);
}

int panel_gpio_helper_enable_irq(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, enable_irq);
}

int panel_gpio_helper_disable_irq(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, disable_irq);
}

int panel_gpio_helper_clear_irq_pending_bit(struct panel_gpio *gpio)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, clear_irq_pending_bit);
}

bool panel_gpio_helper_is_irq_enabled(struct panel_gpio *gpio)
{
	if (!gpio)
		return false;

	return call_panel_gpio_func(gpio, is_irq_enabled);
}

int panel_gpio_helper_devm_request_irq(struct panel_gpio *gpio,
		struct device *dev, irq_handler_t handler, const char *devname, void *dev_id)
{
	if (!gpio)
		return -EINVAL;

	return call_panel_gpio_func(gpio, devm_request_irq, dev, handler, devname, dev_id);
}

int of_get_panel_gpio(struct device_node *np, struct panel_gpio *gpio)
{
	struct device_node *pend_np;
	enum of_gpio_flags flags;
	int ret;

	if (of_gpio_count(np) < 1)
		return -ENODEV;

	if (!gpio)
		return -EINVAL;

	gpio->name = np->name;
	gpio->num = of_get_gpio_flags(np, 0, &flags);
	if (!gpio_is_valid(gpio->num)) {
		panel_err("%s:invalid gpio %s:%d\n", np->name, gpio->name, gpio->num);
		return -ENODEV;
	}
	gpio->active_low = flags & OF_GPIO_ACTIVE_LOW;

	if (of_property_read_u32(np, "dir", &gpio->dir))
		panel_warn("%s:property('dir') not found\n", np->name);

	if ((gpio->dir & GPIOF_DIR_IN) == GPIOF_DIR_OUT) {
		ret = gpio_request(gpio->num, gpio->name);
		if (ret < 0) {
			panel_err("failed to request gpio(%s:%d)\n", gpio->name, gpio->num);
			return ret;
		}
	} else {
		ret = gpio_request_one(gpio->num, GPIOF_IN, gpio->name);
		if (ret < 0) {
			panel_err("failed to request gpio(%s:%d)\n", gpio->name, gpio->num);
			return ret;
		}
	}

	if (of_property_read_u32(np, "irq-type", &gpio->irq_type))
		panel_warn("%s:property('irq-type') not found\n", np->name);

	if (gpio->irq_type > 0) {
		gpio->irq = gpio_to_irq(gpio->num);

		pend_np = of_get_child_by_name(np, "irq-pend");
		if (pend_np) {
			gpio->irq_pend_reg = of_iomap(pend_np, 0);
			if (gpio->irq_pend_reg == NULL)
				panel_err("%s:%s:property('reg') not found\n", np->name, pend_np->name);

			if (of_property_read_u32(pend_np, "bit",
						&gpio->irq_pend_bit)) {
				panel_err("%s:%s:property('bit') not found\n", np->name, pend_np->name);
				gpio->irq_pend_bit = -EINVAL;
			}
			of_node_put(pend_np);
		}
	}
	gpio->irq_enable = false;
	gpio->funcs = &panel_gpio_funcs;

	panel_info("gpio(%s:%d) active:%s dir:%d irq_type:%04x\n",
			gpio->name, gpio->num, gpio->active_low ? "low" : "high",
			gpio->dir, gpio->irq_type);

	return 0;
}
EXPORT_SYMBOL(of_get_panel_gpio);

struct panel_gpio *panel_gpio_create(void)
{
	struct panel_gpio *gpio;

	gpio = kzalloc(sizeof(struct panel_gpio), GFP_KERNEL);
	if (!gpio)
		return NULL;

	gpio->funcs = &panel_gpio_funcs;

	return gpio;
}
EXPORT_SYMBOL(panel_gpio_create);

void panel_gpio_destroy(struct panel_gpio *gpio)
{
	kfree(gpio);
}
EXPORT_SYMBOL(panel_gpio_destroy);
