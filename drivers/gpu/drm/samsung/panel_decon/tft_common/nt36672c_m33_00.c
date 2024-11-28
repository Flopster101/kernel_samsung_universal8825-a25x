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
#include "nt36672c_m33_00_panel.h"

static int __init nt36672c_m33_00_panel_init(void)
{
	register_common_panel(&nt36672c_m33x_00_panel_info);
	return 0;
}

static void __exit nt36672c_m33_00_panel_exit(void)
{
	deregister_common_panel(&nt36672c_m33x_00_panel_info);
}

module_init(nt36672c_m33_00_panel_init)
module_exit(nt36672c_m33_00_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
