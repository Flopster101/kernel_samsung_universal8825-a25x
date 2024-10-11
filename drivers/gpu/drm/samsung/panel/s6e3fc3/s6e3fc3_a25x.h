/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c_m33x_00/nt36672c_m33_00.h
 *
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_A25X_H__
#define __S6E3FC3_A25X_H__
#include "../panel.h"
#include "../panel_drv.h"

enum s6e3fc3_a25x_function {
	S6E3FC3_A25X_MAPTBL_GETIDX_FFC,
	MAX_S6E3FC3_A25X_FUNCTION,
};

extern struct pnobj_func s6e3fc3_a25x_function_table[MAX_S6E3FC3_A25X_FUNCTION];

#undef PANEL_FUNC
#define PANEL_FUNC(_index) (s6e3fc3_a25x_function_table[_index])

enum {
	S6E3FC3_A25X_HS_CLK_1108 = 0,
	S6E3FC3_A25X_HS_CLK_1124,
	S6E3FC3_A25X_HS_CLK_1125,
	MAX_S6E3FC3_A25X_HS_CLK
};

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int s6e3fc3_a25x_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
int s6e3fc3_a25x_getidx_ffc_table(struct maptbl *tbl);

#endif /* __S6E3FC3_A25X_H__ */
