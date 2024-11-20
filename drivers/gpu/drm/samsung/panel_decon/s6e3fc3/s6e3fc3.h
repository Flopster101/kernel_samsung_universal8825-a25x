/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3.h
 *
 * Header file for S6E3FC3 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_H__
#define __S6E3FC3_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => S6E3FC3_XXX_OFS (0)
 * XXX 2nd param => S6E3FC3_XXX_OFS (1)
 * XXX 36th param => S6E3FC3_XXX_OFS (35)
 */

#define S6E3FC3_ADDR_OFS	(0)
#define S6E3FC3_ADDR_LEN	(1)
#define S6E3FC3_DATA_OFS	(S6E3FC3_ADDR_OFS + S6E3FC3_ADDR_LEN)

#define S6E3FC3_DATE_REG			0xA1
#define S6E3FC3_DATE_OFS			4
#define S6E3FC3_DATE_LEN			(PANEL_DATE_LEN)

#define S6E3FC3_COORDINATE_REG		0xA1
#define S6E3FC3_COORDINATE_OFS		0
#define S6E3FC3_COORDINATE_LEN		(PANEL_COORD_LEN)

#define S6E3FC3_ID_REG				0x04
#define S6E3FC3_ID_OFS				0
#define S6E3FC3_ID_LEN				(PANEL_ID_LEN)

#define S6E3FC3_CODE_REG			0xD6
#define S6E3FC3_CODE_OFS			0
#define S6E3FC3_CODE_LEN			5

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FC3_M34)
#define S6E3FC3_OCTA_ID_0_REG			0xA1
#define S6E3FC3_OCTA_ID_0_OFS			11
#define S6E3FC3_OCTA_ID_0_LEN			4

#define S6E3FC3_OCTA_ID_1_REG			0x92
#define S6E3FC3_OCTA_ID_1_OFS			2
#define S6E3FC3_OCTA_ID_1_LEN			10

#define S6E3FC3_OCTA_ID_2_REG			0x92
#define S6E3FC3_OCTA_ID_2_OFS			12
#define S6E3FC3_OCTA_ID_2_LEN			6

#define S6E3FC3_OCTA_ID_LEN			(S6E3FC3_OCTA_ID_0_LEN + S6E3FC3_OCTA_ID_1_LEN + S6E3FC3_OCTA_ID_2_LEN)
#else
#define S6E3FC3_OCTA_ID_0_REG			0xA1
#define S6E3FC3_OCTA_ID_0_OFS			11
#define S6E3FC3_OCTA_ID_0_LEN			4

#define S6E3FC3_OCTA_ID_1_REG			0x92
#define S6E3FC3_OCTA_ID_1_OFS			2
#define S6E3FC3_OCTA_ID_1_LEN			16

#define S6E3FC3_OCTA_ID_LEN			(S6E3FC3_OCTA_ID_0_LEN + S6E3FC3_OCTA_ID_1_LEN)
#endif

/* for panel dump */
#define S6E3FC3_RDDPM_REG			0x0A
#define S6E3FC3_RDDPM_OFS			0
#define S6E3FC3_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define S6E3FC3_RDDSM_REG			0x0E
#define S6E3FC3_RDDSM_OFS			0
#define S6E3FC3_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define S6E3FC3_ERR_REG				0xE9
#define S6E3FC3_ERR_OFS				0
#define S6E3FC3_ERR_LEN				5

#define S6E3FC3_ERR_FG_REG			0xEE
#define S6E3FC3_ERR_FG_OFS			0
#define S6E3FC3_ERR_FG_LEN			1

#define S6E3FC3_DSI_ERR_REG			0x05
#define S6E3FC3_DSI_ERR_OFS			0
#define S6E3FC3_DSI_ERR_LEN			1

#define S6E3FC3_SELF_DIAG_REG			0x0F
#define S6E3FC3_SELF_DIAG_OFS			0
#define S6E3FC3_SELF_DIAG_LEN			1

#define S6E3FC3_SELF_MASK_CHECKSUM_REG		0x14
#define S6E3FC3_SELF_MASK_CHECKSUM_OFS		0
#define S6E3FC3_SELF_MASK_CHECKSUM_LEN		2

#define S6E3FC3_SELF_MASK_CRC_REG		0x7F
#define S6E3FC3_SELF_MASK_CRC_OFS	6
#define S6E3FC3_SELF_MASK_CRC_LEN		4

#define S6E3FC3_DECODER_TEST1_REG			0x14
#define S6E3FC3_DECODER_TEST1_OFS			0
#define S6E3FC3_DECODER_TEST1_LEN			2

#define S6E3FC3_DECODER_TEST2_REG			0x15
#define S6E3FC3_DECODER_TEST2_OFS			0
#define S6E3FC3_DECODER_TEST2_LEN			2

#define S6E3FC3_DECODER_TEST3_REG			0x14
#define S6E3FC3_DECODER_TEST3_OFS			0
#define S6E3FC3_DECODER_TEST3_LEN			2

#define S6E3FC3_DECODER_TEST4_REG			0x15
#define S6E3FC3_DECODER_TEST4_OFS			0
#define S6E3FC3_DECODER_TEST4_LEN			2

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#define NR_S6E3FC3_MDNIE_REG	(4)

#define S6E3FC3_MDNIE_0_REG		(0x5D)
#define S6E3FC3_MDNIE_0_OFS		(0)
#define S6E3FC3_MDNIE_0_LEN		(7)

#define S6E3FC3_MDNIE_1_REG		(0x62)
#define S6E3FC3_MDNIE_1_OFS		(S6E3FC3_MDNIE_0_OFS + S6E3FC3_MDNIE_0_LEN)
#define S6E3FC3_MDNIE_1_LEN		(25)

#define S6E3FC3_MDNIE_2_REG		(0xB0)
#define S6E3FC3_MDNIE_2_OFS		(S6E3FC3_MDNIE_1_OFS + S6E3FC3_MDNIE_1_LEN)
#define S6E3FC3_MDNIE_2_LEN		(3)

#define S6E3FC3_MDNIE_3_REG		(0x62)
#define S6E3FC3_MDNIE_3_OFS		(S6E3FC3_MDNIE_2_OFS + S6E3FC3_MDNIE_2_LEN)
#define S6E3FC3_MDNIE_3_LEN		(218)
#define S6E3FC3_MDNIE_LEN		(S6E3FC3_MDNIE_0_LEN + S6E3FC3_MDNIE_1_LEN + S6E3FC3_MDNIE_2_LEN + S6E3FC3_MDNIE_3_LEN )

#ifdef CONFIG_SUPPORT_AFC
#define S6E3FC3_AFC_REG			(0xE2)
#define S6E3FC3_AFC_OFS			(0)
#define S6E3FC3_AFC_LEN			(70)
#define S6E3FC3_AFC_ROI_OFS		(55)
#define S6E3FC3_AFC_ROI_LEN		(12)
#endif

#define S6E3FC3_SCR_CR_OFS	(1)
#define S6E3FC3_SCR_WR_OFS	(19)
#define S6E3FC3_SCR_WG_OFS	(20)
#define S6E3FC3_SCR_WB_OFS	(21)
#define S6E3FC3_NIGHT_MODE_OFS	(S6E3FC3_SCR_CR_OFS)
#define S6E3FC3_NIGHT_MODE_LEN	(21)
#define S6E3FC3_COLOR_LENS_OFS	(S6E3FC3_SCR_CR_OFS)
#define S6E3FC3_COLOR_LENS_LEN	(21)
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
#define S6E3FC3_CMDLOG_REG			0x9C
#define S6E3FC3_CMDLOG_OFS			0
#define S6E3FC3_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_PANEL_FREQ_HOP
#define S6E3FC3_MAX_MIPI_FREQ			3
#define S6E3FC3_DEFAULT_MIPI_FREQ		0
enum {
	S6E3FC3_OSC_DEFAULT,
	MAX_S6E3FC3_OSC,
};
#endif


enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	AOR_MAPTBL,
	TSET_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_DIM_SPEED_MAPTBL,
	ACL_OPR_MAPTBL,
	FPS_MAPTBL_1,
	FPS_MAPTBL_2,
	DIMMING_SPEED,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	HBM_ONOFF_MAPTBL,
#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	SET_FFC_MAPTBL,
	SET_FFC_MAPTBL_2,
	IRC_MODE_MAPTBL,
	MAX_MAPTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	READ_COPR_SPI,
	READ_COPR_DSI,
#endif
	READ_ID,
	READ_COORDINATE,
	READ_CODE,
	READ_ELVSS,
	READ_ELVSS_TEMP_0,
	READ_ELVSS_TEMP_1,
	READ_MTP,
	READ_DATE,
	READ_OCTA_ID_0,
	READ_OCTA_ID_1,
	READ_OCTA_ID_2,
	/* READ_CHIP_ID, */
	/* for brightness debugging */
	READ_AOR,
	READ_VINT,
	READ_ELVSS_T,
	READ_IRC,

	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CHECKSUM,
	READ_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_POC_SPI
	READ_POC_SPI_READ,
	READ_POC_SPI_STATUS1,
	READ_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	READ_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT_CAL,
#endif
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
	READ_DECODER_TEST1,
	READ_DECODER_TEST2,
	READ_DECODER_TEST3,
	READ_DECODER_TEST4,
#endif
	MAX_READTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	RES_COPR_SPI,
	RES_COPR_DSI,
#endif
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_ELVSS,
	RES_ELVSS_TEMP_0,
	RES_ELVSS_TEMP_1,
	RES_MTP,
	RES_DATE,
	RES_OCTA_ID,
	/* RES_CHIP_ID, */
	/* for brightness debugging */
	RES_AOR,
	RES_VINT,
	RES_ELVSS_T,
	RES_IRC,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	RES_POC_SPI_READ,
	RES_POC_SPI_STATUS1,
	RES_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	RES_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_FAIL,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CHECKSUM,
	RES_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
	RES_DECODER_TEST1,
	RES_DECODER_TEST2,
	RES_DECODER_TEST3,
	RES_DECODER_TEST4,
#endif
	MAX_RESTBL
};

enum {
	HLPM_ON_LOW,
	HLPM_ON,
	MAX_HLPM_ON
};


enum {
	S6E3FC3_VRR_FPS_120,
	S6E3FC3_VRR_FPS_90,
	S6E3FC3_VRR_FPS_60,
	MAX_S6E3FC3_VRR_FPS,
};


enum {
	S6E3FC3_SMOOTH_DIMMING_OFF,
	S6E3FC3_SMOOTH_DIMMING_ON,
	MAX_S6E3FC3_SMOOTH_DIMMING,
};

static u8 S6E3FC3_ID[S6E3FC3_ID_LEN];
static u8 S6E3FC3_COORDINATE[S6E3FC3_COORDINATE_LEN];
static u8 S6E3FC3_CODE[S6E3FC3_CODE_LEN];
static u8 S6E3FC3_DATE[S6E3FC3_DATE_LEN];
static u8 S6E3FC3_OCTA_ID[S6E3FC3_OCTA_ID_LEN];
/* for brightness debugging */
static u8 S6E3FC3_RDDPM[S6E3FC3_RDDPM_LEN];
static u8 S6E3FC3_RDDSM[S6E3FC3_RDDSM_LEN];
static u8 S6E3FC3_ERR[S6E3FC3_ERR_LEN];
static u8 S6E3FC3_ERR_FG[S6E3FC3_ERR_FG_LEN];
static u8 S6E3FC3_DSI_ERR[S6E3FC3_DSI_ERR_LEN];
static u8 S6E3FC3_SELF_DIAG[S6E3FC3_SELF_DIAG_LEN];
static u8 S6E3FC3_SELF_MASK_CHECKSUM[S6E3FC3_SELF_MASK_CHECKSUM_LEN];
static u8 S6E3FC3_SELF_MASK_CRC[S6E3FC3_SELF_MASK_CRC_LEN];
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
static u8 S6E3FC3_DECODER_TEST1[S6E3FC3_DECODER_TEST1_LEN];
static u8 S6E3FC3_DECODER_TEST2[S6E3FC3_DECODER_TEST2_LEN];
static u8 S6E3FC3_DECODER_TEST3[S6E3FC3_DECODER_TEST3_LEN];
static u8 S6E3FC3_DECODER_TEST4[S6E3FC3_DECODER_TEST4_LEN];
#endif

static struct rdinfo s6e3fc3_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, S6E3FC3_ID_REG, S6E3FC3_ID_OFS, S6E3FC3_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, S6E3FC3_COORDINATE_REG, S6E3FC3_COORDINATE_OFS, S6E3FC3_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, S6E3FC3_CODE_REG, S6E3FC3_CODE_OFS, S6E3FC3_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, S6E3FC3_DATE_REG, S6E3FC3_DATE_OFS, S6E3FC3_DATE_LEN),
	[READ_OCTA_ID_0] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_0_REG, S6E3FC3_OCTA_ID_0_OFS, S6E3FC3_OCTA_ID_0_LEN),
	[READ_OCTA_ID_1] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_1_REG, S6E3FC3_OCTA_ID_1_OFS, S6E3FC3_OCTA_ID_1_LEN),
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FC3_M34)
	[READ_OCTA_ID_2] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_2_REG, S6E3FC3_OCTA_ID_2_OFS, S6E3FC3_OCTA_ID_2_LEN),
#endif
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, S6E3FC3_RDDPM_REG, S6E3FC3_RDDPM_OFS, S6E3FC3_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, S6E3FC3_RDDSM_REG, S6E3FC3_RDDSM_OFS, S6E3FC3_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, S6E3FC3_ERR_REG, S6E3FC3_ERR_OFS, S6E3FC3_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, S6E3FC3_ERR_FG_REG, S6E3FC3_ERR_FG_OFS, S6E3FC3_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, S6E3FC3_DSI_ERR_REG, S6E3FC3_DSI_ERR_OFS, S6E3FC3_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, S6E3FC3_SELF_DIAG_REG, S6E3FC3_SELF_DIAG_OFS, S6E3FC3_SELF_DIAG_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, S6E3FC3_SELF_MASK_CHECKSUM_REG, S6E3FC3_SELF_MASK_CHECKSUM_OFS, S6E3FC3_SELF_MASK_CHECKSUM_LEN),
	[READ_SELF_MASK_CRC] = RDINFO_INIT(self_mask_crc, DSI_PKT_TYPE_RD, S6E3FC3_SELF_MASK_CRC_REG, S6E3FC3_SELF_MASK_CRC_OFS, S6E3FC3_SELF_MASK_CRC_LEN),
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
	[READ_DECODER_TEST1] = RDINFO_INIT(decoder_test1, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST1_REG, S6E3FC3_DECODER_TEST1_OFS, S6E3FC3_DECODER_TEST1_LEN),
	[READ_DECODER_TEST2] = RDINFO_INIT(decoder_test2, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST2_REG, S6E3FC3_DECODER_TEST2_OFS, S6E3FC3_DECODER_TEST2_LEN),
	[READ_DECODER_TEST3] = RDINFO_INIT(decoder_test3, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST3_REG, S6E3FC3_DECODER_TEST1_OFS, S6E3FC3_DECODER_TEST3_LEN),
	[READ_DECODER_TEST4] = RDINFO_INIT(decoder_test4, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST4_REG, S6E3FC3_DECODER_TEST2_OFS, S6E3FC3_DECODER_TEST4_LEN),
#endif
};

static DEFINE_RESUI(id, &s6e3fc3_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &s6e3fc3_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &s6e3fc3_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &s6e3fc3_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &s6e3fc3_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &s6e3fc3_rditbl[READ_DATE], 0);
static DECLARE_RESUI(octa_id) = {
	{ .rditbl = &s6e3fc3_rditbl[READ_OCTA_ID_0], .offset = 0 },
	{ .rditbl = &s6e3fc3_rditbl[READ_OCTA_ID_1], .offset = 4 },
#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FC3_M34)
	{ .rditbl = &s6e3fc3_rditbl[READ_OCTA_ID_2], .offset = 14 },
#endif
};

/* for brightness debugging */
static DEFINE_RESUI(aor, &s6e3fc3_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &s6e3fc3_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &s6e3fc3_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &s6e3fc3_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &s6e3fc3_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &s6e3fc3_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &s6e3fc3_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &s6e3fc3_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &s6e3fc3_rditbl[READ_SELF_DIAG], 0);
static DEFINE_RESUI(self_mask_checksum, &s6e3fc3_rditbl[READ_SELF_MASK_CHECKSUM], 0);
static DEFINE_RESUI(self_mask_crc, &s6e3fc3_rditbl[READ_SELF_MASK_CRC], 0);
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
static DEFINE_RESUI(decoder_test1, &s6e3fc3_rditbl[READ_DECODER_TEST1], 0);
static DEFINE_RESUI(decoder_test2, &s6e3fc3_rditbl[READ_DECODER_TEST2], 0);
static DEFINE_RESUI(decoder_test3, &s6e3fc3_rditbl[READ_DECODER_TEST3], 0);
static DEFINE_RESUI(decoder_test4, &s6e3fc3_rditbl[READ_DECODER_TEST4], 0);
#endif
static struct resinfo s6e3fc3_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, S6E3FC3_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, S6E3FC3_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, S6E3FC3_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, S6E3FC3_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, S6E3FC3_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, S6E3FC3_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, S6E3FC3_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, S6E3FC3_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, S6E3FC3_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, S6E3FC3_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, S6E3FC3_SELF_DIAG, RESUI(self_diag)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, S6E3FC3_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
	[RES_SELF_MASK_CRC] = RESINFO_INIT(self_mask_crc, S6E3FC3_SELF_MASK_CRC, RESUI(self_mask_crc)),
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
	[RES_DECODER_TEST1] = RESINFO_INIT(decoder_test1, S6E3FC3_DECODER_TEST1, RESUI(decoder_test1)),
	[RES_DECODER_TEST2] = RESINFO_INIT(decoder_test2, S6E3FC3_DECODER_TEST2, RESUI(decoder_test2)),
	[RES_DECODER_TEST3] = RESINFO_INIT(decoder_test3, S6E3FC3_DECODER_TEST3, RESUI(decoder_test3)),
	[RES_DECODER_TEST4] = RESINFO_INIT(decoder_test4, S6E3FC3_DECODER_TEST4, RESUI(decoder_test4)),
#endif
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDPM_SLEEP_IN,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
	DUMP_DSI_ERR,
	DUMP_SELF_DIAG,
	DUMP_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
};

int show_rddpm(struct dumpinfo *info);
int show_rddpm_before_sleep_in(struct dumpinfo *info);
int show_rddsm(struct dumpinfo *info);
int show_err(struct dumpinfo *info);
int show_err_fg(struct dumpinfo *info);
int show_dsi_err(struct dumpinfo *info);
int show_self_diag(struct dumpinfo *info);
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
int show_cmdlog(struct dumpinfo *info);
#endif
int show_self_mask_crc(struct dumpinfo *info);
#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
int s6e3fc3_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif

static struct dumpinfo s6e3fc3_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &s6e3fc3_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT(rddpm_sleep_in, &s6e3fc3_restbl[RES_RDDPM], show_rddpm_before_sleep_in),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &s6e3fc3_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &s6e3fc3_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &s6e3fc3_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &s6e3fc3_restbl[RES_DSI_ERR], show_dsi_err),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT(self_diag, &s6e3fc3_restbl[RES_SELF_DIAG], show_self_diag),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT(cmdlog, &s6e3fc3_restbl[RES_CMDLOG], show_cmdlog),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT(self_mask_crc, &s6e3fc3_restbl[RES_SELF_MASK_CRC], show_self_mask_crc),
};

/* Variable Refresh Rate */
enum {
	S6E3FC3_VRR_MODE_NS,
	S6E3FC3_VRR_MODE_HS,
	MAX_S6E3FC3_VRR_MODE,
};

enum {
	S6E3FC3_VRR_120HS,
	S6E3FC3_VRR_90HS,
	S6E3FC3_VRR_60HS,
	MAX_S6E3FC3_VRR,
};

enum {
	S6E3FC3_RESOL_1080x2340,
};

enum {
	S6E3FC3_RESOL_1080x2400,
};

enum {
	S6E3FC3_DISPLAY_MODE_1080x2400_120HS,
	S6E3FC3_DISPLAY_MODE_1080x2400_90HS,
	S6E3FC3_DISPLAY_MODE_1080x2400_60HS,
	MAX_S6E3FC3_DISPLAY_MODE,
};

enum {
	S6E3FC3_A53X_DISPLAY_MODE_1080x2400_120HS,
	S6E3FC3_A53X_DISPLAY_MODE_1080x2400_60HS,
	MAX_S6E3FC3_A53X_DISPLAY_MODE,
};

enum {
	S6E3FC3_A33X_DISPLAY_MODE_1080x2400_90HS,
	S6E3FC3_A33X_DISPLAY_MODE_1080x2400_60HS,
	MAX_S6E3FC3_A33X_DISPLAY_MODE,
};

enum {
	S6E3FC3_M34X_DISPLAY_MODE_1080x2340_120HS,
	S6E3FC3_M34X_DISPLAY_MODE_1080x2340_60HS,
	MAX_S6E3FC3_M34X_DISPLAY_MODE,
};

enum {
	S6E3FC3_VRR_KEY_REFRESH_RATE,
	S6E3FC3_VRR_KEY_REFRESH_MODE,
	S6E3FC3_VRR_KEY_TE_SW_SKIP_COUNT,
	S6E3FC3_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_S6E3FC3_VRR_KEY,
};

enum {
	S6E3FC3_ACL_DIM_OFF,
	S6E3FC3_ACL_DIM_ON,
	MAX_S6E3FC3_ACL_DIM,
};

/* use according to adaptive_control sysfs */
enum {
	S6E3FC3_ACL_OPR_0,
	S6E3FC3_ACL_OPR_1,
	S6E3FC3_ACL_OPR_2,
	S6E3FC3_ACL_OPR_3,
	MAX_S6E3FC3_ACL_OPR
};


enum {
	S6E3FC3_HS_CLK_1108 = 0,
	S6E3FC3_HS_CLK_1124,
	S6E3FC3_HS_CLK_1125,
	MAX_S6E3FC3_HS_CLK
};

int init_common_table(struct maptbl *tbl);
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
int getidx_common_maptbl(struct maptbl *tbl);
#endif
int init_gamma_mode2_brt_table(struct maptbl *tbl);
int getidx_gamma_mode2_brt_table(struct maptbl *tbl);
int getidx_hbm_transition_table(struct maptbl *tbl);
int getidx_smooth_transition_table(struct maptbl *tbl);

int getidx_acl_opr_table(struct maptbl *tbl);
int getidx_hbm_onoff_table(struct maptbl *);
int getidx_acl_onoff_table(struct maptbl *);
int getidx_acl_dim_onoff_table(struct maptbl *tbl);

int init_lpm_brt_table(struct maptbl *tbl);
int getidx_lpm_brt_table(struct maptbl *tbl);

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
#endif
void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
void copy_tset_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_SUPPORT_XTALK_MODE
int getidx_vgh_table(struct maptbl *tbl);
#endif
#if defined(CONFIG_MCD_PANEL_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
int getidx_fast_discharge_table(struct maptbl *tbl);
#endif
int getidx_vrr_fps_table(struct maptbl *);
#if defined(__PANEL_NOT_USED_VARIABLE__)
int getidx_vrr_gamma_table(struct maptbl *);
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
bool s6e3fc3_is_120hz(struct panel_device *panel);
bool s6e3fc3_is_60hz(struct panel_device *panel);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
int init_color_blind_table(struct maptbl *tbl);
int getidx_mdnie_scenario_maptbl(struct maptbl *tbl);
int getidx_mdnie_hdr_maptbl(struct maptbl *tbl);
int init_mdnie_night_mode_table(struct maptbl *tbl);
int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl);
int init_mdnie_color_lens_table(struct maptbl *tbl);
int getidx_color_lens_maptbl(struct maptbl *tbl);
int init_color_coordinate_table(struct maptbl *tbl);
int init_sensor_rgb_table(struct maptbl *tbl);
int getidx_adjust_ldu_maptbl(struct maptbl *tbl);
int getidx_color_coordinate_maptbl(struct maptbl *tbl);
void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst);
int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui);
int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui);
int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui);
int getidx_mdnie_3_maptbl(struct pkt_update_info *pktui);

#if defined(__PANEL_NOT_USED_VARIABLE__)
int init_gamma_mtp_all_table(struct maptbl *tbl);
#endif

int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */
bool is_panel_state_not_lpm(struct panel_device *panel);
static inline bool is_panel_state_lpm(struct panel_device *panel) {
	return !is_panel_state_not_lpm(panel);
};

#endif /* __S6E3FC3_H__ */
