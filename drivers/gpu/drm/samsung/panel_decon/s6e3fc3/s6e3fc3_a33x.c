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
#include <video/mipi_display.h>
#include "s6e3fc3_a33x.h"
#include "s6e3fc3_a33x_panel.h"

static int __init s6e3fc3_a33x_panel_init(void)
{
	register_common_panel(&s6e3fc3_a33x_panel_info);

	return 0;
}

static void __exit s6e3fc3_a33x_panel_exit(void)
{
	deregister_common_panel(&s6e3fc3_a33x_panel_info);
}

module_init(s6e3fc3_a33x_panel_init)
module_exit(s6e3fc3_a33x_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
