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
#include <linux/sec_detect.h>
#include "nt36672c_m33_00_panel.h"

static int __init nt36672c_m33_00_panel_init(void)
{
	if (sec_current_device != SEC_M33) {
		SEC_DETECT_LOG("Skipped nt36672c_m33_00 panel driver\n");
		return 0;
	}
	decon_register_common_panel(&nt36672c_m33x_00_panel_info);

	SEC_DETECT_LOG("Initialized nt36672c_m33_00 panel driver\n");
	return 0;
}

static void __exit nt36672c_m33_00_panel_exit(void)
{
	decon_deregister_common_panel(&nt36672c_m33x_00_panel_info);
}

module_init(nt36672c_m33_00_panel_init)
module_exit(nt36672c_m33_00_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
