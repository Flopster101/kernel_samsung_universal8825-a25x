/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.h
 *
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TFT_COMMON_H__
#define __TFT_COMMON_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include "../maptbl.h"

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => TFT_COMMON_XXX_OFS (0)
 * XXX 2nd param => TFT_COMMON_XXX_OFS (1)
 * XXX 36th param => TFT_COMMON_XXX_OFS (35)
 */

#define TFT_COMMON_ADDR_OFS	(0)
#define TFT_COMMON_ADDR_LEN	(1)
#define TFT_COMMON_DATA_OFS	(TFT_COMMON_ADDR_OFS + TFT_COMMON_ADDR_LEN)

#define TFT_COMMON_ID_LEN				(PANEL_ID_LEN)

#define TFT_COMMON_ID_DA_REG				0xDA
#define TFT_COMMON_ID_DA_OFS				0
#define TFT_COMMON_ID_DA_LEN				1

#define TFT_COMMON_ID_DB_REG				0xDB
#define TFT_COMMON_ID_DB_OFS				0
#define TFT_COMMON_ID_DB_LEN				1

#define TFT_COMMON_ID_DC_REG				0xDC
#define TFT_COMMON_ID_DC_OFS				0
#define TFT_COMMON_ID_DC_LEN				1

enum {
	BRT_MAPTBL,
	BLIC_BRT_MAPTBL,
	MAX_MAPTBL,
};

enum {
	READ_ID,
	READ_ID_DA,
	READ_ID_DB,
	READ_ID_DC,
};

enum {
	RES_ID,
};

int tft_maptbl_init_default(struct maptbl *tbl);
int tft_maptbl_init_brt(struct maptbl *tbl);
int tft_maptbl_getidx_brt(struct maptbl *tbl);
void tft_maptbl_copy_default(struct maptbl *tbl, u8 *dst);

#endif /* __TFT_COMMON_H__ */
