/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.c
 *
 * TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <linux/sec_detect.h>
#include <video/mipi_display.h>
#include "s6e8fc3_a33x.h"
#include "s6e8fc3_a33x_panel.h"

__visible_for_testing int __init s6e8fc3_a33x_panel_init(void)
{
	if (sec_current_device == SEC_A33) {
		printk(KERN_INFO "Initialized s6e8fc3_a33x_panel\n");
		register_common_panel(&s6e8fc3_a33x_panel_info);
	} else {
		printk(KERN_INFO "Skipped a33x panel\n");
	}
	return 0;
}

__visible_for_testing void __exit s6e8fc3_a33x_panel_exit(void)
{
	deregister_common_panel(&s6e8fc3_a33x_panel_info);
}

module_init(s6e8fc3_a33x_panel_init)
module_exit(s6e8fc3_a33x_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
