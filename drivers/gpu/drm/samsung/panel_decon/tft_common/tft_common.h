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
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

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

#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
#define TFT_COMMON_VCOM_TRIM_REG			0x7F
#define TFT_COMMON_VCOM_TRIM_OFS			0
#define TFT_COMMON_VCOM_TRIM_LEN			1

#define TFT_COMMON_VCOM_MARK1_REG			0xAB
#define TFT_COMMON_VCOM_MARK1_OFS			0
#define TFT_COMMON_VCOM_MARK1_LEN			1

#define TFT_COMMON_VCOM_MARK2_REG			0xAC
#define TFT_COMMON_VCOM_MARK2_OFS			0
#define TFT_COMMON_VCOM_MARK2_LEN			1
#endif

enum {
	BRT_MAPTBL,
	MAX_MAPTBL,
};

enum {
	READ_ID,
	READ_ID_DA,
	READ_ID_DB,
	READ_ID_DC,
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	READ_VCOM_TRIM,
	READ_VCOM_MARK1,
	READ_VCOM_MARK2,
#endif
};

enum {
	RES_ID,
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	RES_VCOM_TRIM,
	RES_VCOM_MARK1,
	RES_VCOM_MARK2,
#endif
};

static u8 TFT_COMMON_ID[TFT_COMMON_ID_LEN];
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
static u8 TFT_COMMON_VCOM_TRIM[TFT_COMMON_VCOM_TRIM_LEN];
static u8 TFT_COMMON_VCOM_MARK1[TFT_COMMON_VCOM_MARK1_LEN];
static u8 TFT_COMMON_VCOM_MARK2[TFT_COMMON_VCOM_MARK2_LEN];
#endif

static struct rdinfo tft_common_rditbl[] = {
	[READ_ID_DA] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN),
	[READ_ID_DB] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DB_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN),
	[READ_ID_DC] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DC_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN),
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	[READ_VCOM_TRIM] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_VCOM_TRIM_REG, TFT_COMMON_VCOM_TRIM_OFS, TFT_COMMON_VCOM_TRIM_LEN),
	[READ_VCOM_MARK1] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_VCOM_MARK1_REG, TFT_COMMON_VCOM_MARK1_OFS, TFT_COMMON_VCOM_MARK1_LEN),
	[READ_VCOM_MARK2] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_VCOM_MARK2_REG, TFT_COMMON_VCOM_MARK2_OFS, TFT_COMMON_VCOM_MARK2_LEN),
#endif
};

static DECLARE_RESUI(id) = {
	{.rditbl = &tft_common_rditbl[READ_ID_DA], .offset = 0},
	{.rditbl = &tft_common_rditbl[READ_ID_DB], .offset = 1},
	{.rditbl = &tft_common_rditbl[READ_ID_DC], .offset = 2},
};
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
static DEFINE_RESUI(vcom_trim, &tft_common_rditbl[READ_VCOM_TRIM], 0);
static DEFINE_RESUI(vcom_mark1, &tft_common_rditbl[READ_VCOM_MARK1], 0);
static DEFINE_RESUI(vcom_mark2, &tft_common_rditbl[READ_VCOM_MARK2], 0);
#endif

static struct resinfo tft_common_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, TFT_COMMON_ID, RESUI(id)),
#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
	[RES_VCOM_TRIM] = RESINFO_INIT(vcom_trim, TFT_COMMON_VCOM_TRIM, RESUI(vcom_trim)),
	[RES_VCOM_MARK1] = RESINFO_INIT(vcom_mark1, TFT_COMMON_VCOM_MARK1, RESUI(vcom_mark1)),
	[RES_VCOM_MARK2] = RESINFO_INIT(vcom_mark2, TFT_COMMON_VCOM_MARK2, RESUI(vcom_mark2)),
#endif
};

int init_brt_table(struct maptbl *tbl);
int getidx_brt_table(struct maptbl *);
void copy_common_maptbl(struct maptbl *, u8 *);

#endif /* __TFT_COMMON_H__ */
