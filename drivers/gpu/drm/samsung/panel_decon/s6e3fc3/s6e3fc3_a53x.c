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
#include "s6e3fc3_a53x.h"
#include "s6e3fc3_a53x_panel.h"

int s6e3fc3_getidx_ffc_table(struct maptbl *tbl)
{
	int idx;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;

	switch (dsi_clk) {
	case 1108000:
		idx = S6E3FC3_HS_CLK_1108;
		break;
	case 1124000:
		idx = S6E3FC3_HS_CLK_1124;
		break;
	case 1125000:
		idx = S6E3FC3_HS_CLK_1125;
		break;
	default:
		pr_info("%s: invalid dsi clock: %d\n", __func__, dsi_clk);
		BUG();
	}
	return decon_maptbl_index(tbl, 0, idx, 0);
}

static int __init s6e3fc3_a53x_panel_init(void)
{
	if (sec_current_device != SEC_A53) {
		SEC_DETECT_LOG("Skipped s6e3fc3_a53x panel driver\n");
		return 0;
	}
	decon_register_common_panel(&s6e3fc3_a53x_panel_info);

	SEC_DETECT_LOG("Initialized s6e3fc3_a53x panel driver\n");
	return 0;
}

static void __exit s6e3fc3_a53x_panel_exit(void)
{
	decon_deregister_common_panel(&s6e3fc3_a53x_panel_info);
}

module_init(s6e3fc3_a53x_panel_init)
module_exit(s6e3fc3_a53x_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
