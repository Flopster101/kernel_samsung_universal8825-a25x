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
#ifdef CONFIG_USDM_PANEL_DDI_FLASH
#include "../panel_poc.h"
#endif
#include "../panel_drv.h"
#include "../panel.h"
#include "../maptbl.h"
#include "oled_function.h"

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

#define S6E3FC3_SELF_MASK_CRC_REG		0x14
#define S6E3FC3_SELF_MASK_CRC_OFS		0
#define S6E3FC3_SELF_MASK_CRC_LEN		2

#define S6E3FC3_SELF_MASK_CHECKSUM_REG		0x7F
#define S6E3FC3_SELF_MASK_CHECKSUM_OFS	1
#define S6E3FC3_SELF_MASK_CHECKSUM_LEN		4

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

#define S6E3FC3_PCD_REG				0xCD
#define S6E3FC3_PCD_OFS				0
#define S6E3FC3_PCD_LEN				1

#define S6E3FC3_PCD2_REG				0xCC
#define S6E3FC3_PCD2_OFS				0
#define S6E3FC3_PCD2_LEN				2

#ifdef CONFIG_USDM_DDI_CMDLOG
#define S6E3FC3_CMDLOG_REG			0x9C
#define S6E3FC3_CMDLOG_OFS			0
#define S6E3FC3_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_USDM_PANEL_FREQ_HOP
#define S6E3FC3_MAX_MIPI_FREQ			3
#define S6E3FC3_DEFAULT_MIPI_FREQ		0
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
#define S6E3FC3_CCD_STATE_REG				0xCD
#define S6E3FC3_CCD_STATE_OFS				0
#define S6E3FC3_CCD_STATE_LEN				1
#define S6E3FC3_CCD_STATE_PASS_LIST_LEN				2
#endif

enum s6e3fc3_function {
	S6E3FC3_MAPTBL_INIT_GAMMA_MODE2_BRT,
	S6E3FC3_MAPTBL_GETIDX_HBM_TRANSITION,
	S6E3FC3_MAPTBL_GETIDX_NORMAL_HBM_TRANSITION,
	S6E3FC3_MAPTBL_GETIDX_SMOOTH_TRANSITION,
	S6E3FC3_MAPTBL_GETIDX_ACL_OPR,
	S6E3FC3_MAPTBL_GETIDX_HBM_ONOFF,
	S6E3FC3_MAPTBL_GETIDX_ACL_ONOFF,
	S6E3FC3_MAPTBL_GETIDX_ACL_DIM_ONOFF,
	S6E3FC3_MAPTBL_INIT_LPM_BRT,
	S6E3FC3_MAPTBL_GETIDX_LPM_BRT,
	S6E3FC3_MAPTBL_GETIDX_IRC_MODE,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	S6E3FC3_MAPTBL_GETIDX_FAST_DISCHARGE,
#endif
	S6E3FC3_MAPTBL_GETIDX_VRR_FPS,
	S6E3FC3_MAPTBL_GETIDX_VRR,
	MAX_S6E3FC3_FUNCTION,
};

extern struct pnobj_func s6e3fc3_function_table[MAX_S6E3FC3_FUNCTION];

#undef DDI_FUNC
#define DDI_FUNC(_index) (s6e3fc3_function_table[_index])

#ifdef CONFIG_USDM_PANEL_FREQ_HOP
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
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_DIM_SPEED_MAPTBL,
	ACL_OPR_MAPTBL,
	FPS_MAPTBL_1,
	FPS_MAPTBL_2,
	FPS_MAPTBL_3,
	FPS_MAPTBL_4,
	DIMMING_SPEED,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	LPM_EXIT_NIT_MAPTBL,
	HBM_ONOFF_MAPTBL,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	SET_FFC_MAPTBL,
	SET_FFC_MAPTBL_2,
	IRC_MODE_MAPTBL,
	MAX_MAPTBL,
};

enum {
#ifdef CONFIG_USDM_PANEL_COPR
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
	READ_IRC,

	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CRC,
	READ_SELF_MASK_CHECKSUM,
#ifdef CONFIG_USDM_POC_SPI
	READ_POC_SPI_READ,
	READ_POC_SPI_STATUS1,
	READ_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_USDM_PANEL_POC_FLASH
	READ_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_USDM_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT_CAL,
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	READ_DECODER_TEST1,
	READ_DECODER_TEST2,
	READ_DECODER_TEST3,
	READ_DECODER_TEST4,
#endif
	READ_PCD,
	READ_PCD2,
	MAX_READTBL,
};

enum {
#ifdef CONFIG_USDM_PANEL_COPR
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
	RES_IRC,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_USDM_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_USDM_POC_SPI
	RES_POC_SPI_READ,
	RES_POC_SPI_STATUS1,
	RES_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_USDM_PANEL_POC_FLASH
	RES_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_PASS_LIST,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CRC,
	RES_SELF_MASK_CHECKSUM,
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	RES_DECODER_TEST1,
	RES_DECODER_TEST2,
	RES_DECODER_TEST3,
	RES_DECODER_TEST4,
#endif
	RES_PCD,
	RES_PCD2,
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

enum {
	S6E3FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_NORMAL,
	S6E3FC3_NORMAL_HBM_TRANSITION_NORMAL_TO_HBM,
	S6E3FC3_NORMAL_HBM_TRANSITION_HBM_TO_NORMAL,
	S6E3FC3_NORMAL_HBM_TRANSITION_HBM_TO_HBM,
	MAX_S6E3FC3_NORMAL_HBM_TRANSITION,
};

#define S6E3FC3_VRR_FPS_PROPERTY ("s6e3fc3_vrr_fps")
#define S6E3FC3_NORMAL_HBM_TRANSITION_PROPERTY ("s6e3fc3_normal_hbm_transition")

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
static u8 S6E3FC3_SELF_MASK_CRC[S6E3FC3_SELF_MASK_CRC_LEN];
static u8 S6E3FC3_SELF_MASK_CHECKSUM[S6E3FC3_SELF_MASK_CHECKSUM_LEN];
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 S6E3FC3_DECODER_TEST1[S6E3FC3_DECODER_TEST1_LEN];
static u8 S6E3FC3_DECODER_TEST2[S6E3FC3_DECODER_TEST2_LEN];
static u8 S6E3FC3_DECODER_TEST3[S6E3FC3_DECODER_TEST3_LEN];
static u8 S6E3FC3_DECODER_TEST4[S6E3FC3_DECODER_TEST4_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 S6E3FC3_CCD_STATE[S6E3FC3_CCD_STATE_LEN];
static u8 S6E3FC3_CCD_CHKSUM_PASS_LIST[S6E3FC3_CCD_STATE_PASS_LIST_LEN] = { 0x00, 0x08, };
#endif
static u8 S6E3FC3_PCD[S6E3FC3_PCD_LEN];
static u8 S6E3FC3_PCD2[S6E3FC3_PCD2_LEN];

static struct rdinfo s6e3fc3_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, S6E3FC3_ID_REG, S6E3FC3_ID_OFS, S6E3FC3_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, S6E3FC3_COORDINATE_REG, S6E3FC3_COORDINATE_OFS, S6E3FC3_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, S6E3FC3_CODE_REG, S6E3FC3_CODE_OFS, S6E3FC3_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, S6E3FC3_DATE_REG, S6E3FC3_DATE_OFS, S6E3FC3_DATE_LEN),
	[READ_OCTA_ID_0] = RDINFO_INIT(octa_id1, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_0_REG, S6E3FC3_OCTA_ID_0_OFS, S6E3FC3_OCTA_ID_0_LEN),
	[READ_OCTA_ID_1] = RDINFO_INIT(octa_id2, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_1_REG, S6E3FC3_OCTA_ID_1_OFS, S6E3FC3_OCTA_ID_1_LEN),
	[READ_OCTA_ID_2] = RDINFO_INIT(octa_id3, DSI_PKT_TYPE_RD, S6E3FC3_OCTA_ID_2_REG, S6E3FC3_OCTA_ID_2_OFS, S6E3FC3_OCTA_ID_2_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, S6E3FC3_RDDPM_REG, S6E3FC3_RDDPM_OFS, S6E3FC3_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, S6E3FC3_RDDSM_REG, S6E3FC3_RDDSM_OFS, S6E3FC3_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, S6E3FC3_ERR_REG, S6E3FC3_ERR_OFS, S6E3FC3_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, S6E3FC3_ERR_FG_REG, S6E3FC3_ERR_FG_OFS, S6E3FC3_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, S6E3FC3_DSI_ERR_REG, S6E3FC3_DSI_ERR_OFS, S6E3FC3_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, S6E3FC3_SELF_DIAG_REG, S6E3FC3_SELF_DIAG_OFS, S6E3FC3_SELF_DIAG_LEN),
	[READ_SELF_MASK_CRC] = RDINFO_INIT(self_mask_crc, DSI_PKT_TYPE_RD, S6E3FC3_SELF_MASK_CRC_REG, S6E3FC3_SELF_MASK_CRC_OFS, S6E3FC3_SELF_MASK_CRC_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, S6E3FC3_SELF_MASK_CHECKSUM_REG, S6E3FC3_SELF_MASK_CHECKSUM_OFS, S6E3FC3_SELF_MASK_CHECKSUM_LEN),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[READ_DECODER_TEST1] = RDINFO_INIT(decoder_test1, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST1_REG, S6E3FC3_DECODER_TEST1_OFS, S6E3FC3_DECODER_TEST1_LEN),
	[READ_DECODER_TEST2] = RDINFO_INIT(decoder_test2, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST2_REG, S6E3FC3_DECODER_TEST2_OFS, S6E3FC3_DECODER_TEST2_LEN),
	[READ_DECODER_TEST3] = RDINFO_INIT(decoder_test3, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST3_REG, S6E3FC3_DECODER_TEST1_OFS, S6E3FC3_DECODER_TEST3_LEN),
	[READ_DECODER_TEST4] = RDINFO_INIT(decoder_test4, DSI_PKT_TYPE_RD, S6E3FC3_DECODER_TEST4_REG, S6E3FC3_DECODER_TEST2_OFS, S6E3FC3_DECODER_TEST4_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, S6E3FC3_CCD_STATE_REG, S6E3FC3_CCD_STATE_OFS, S6E3FC3_CCD_STATE_LEN),
#endif
	[READ_PCD] = RDINFO_INIT(pcd, DSI_PKT_TYPE_RD, S6E3FC3_PCD_REG, S6E3FC3_PCD_OFS, S6E3FC3_PCD_LEN),
	[READ_PCD2] = RDINFO_INIT(pcd2, DSI_PKT_TYPE_RD, S6E3FC3_PCD2_REG, S6E3FC3_PCD2_OFS, S6E3FC3_PCD2_LEN),
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
	{ .rditbl = &s6e3fc3_rditbl[READ_OCTA_ID_2], .offset = 14 },
};

/* for brightness debugging */
static DEFINE_RESUI(rddpm, &s6e3fc3_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &s6e3fc3_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &s6e3fc3_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &s6e3fc3_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &s6e3fc3_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &s6e3fc3_rditbl[READ_SELF_DIAG], 0);
static DEFINE_RESUI(self_mask_crc, &s6e3fc3_rditbl[READ_SELF_MASK_CRC], 0);
static DEFINE_RESUI(self_mask_checksum, &s6e3fc3_rditbl[READ_SELF_MASK_CHECKSUM], 0);
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static DEFINE_RESUI(decoder_test1, &s6e3fc3_rditbl[READ_DECODER_TEST1], 0);
static DEFINE_RESUI(decoder_test2, &s6e3fc3_rditbl[READ_DECODER_TEST2], 0);
static DEFINE_RESUI(decoder_test3, &s6e3fc3_rditbl[READ_DECODER_TEST3], 0);
static DEFINE_RESUI(decoder_test4, &s6e3fc3_rditbl[READ_DECODER_TEST4], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static DEFINE_RESUI(ccd_state, &s6e3fc3_rditbl[READ_CCD_STATE], 0);
#endif
static DEFINE_RESUI(pcd, &s6e3fc3_rditbl[READ_PCD], 0);
static DEFINE_RESUI(pcd2, &s6e3fc3_rditbl[READ_PCD2], 0);

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
	[RES_SELF_MASK_CRC] = RESINFO_INIT(self_mask_crc, S6E3FC3_SELF_MASK_CRC, RESUI(self_mask_crc)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, S6E3FC3_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[RES_DECODER_TEST1] = RESINFO_INIT(decoder_test1, S6E3FC3_DECODER_TEST1, RESUI(decoder_test1)),
	[RES_DECODER_TEST2] = RESINFO_INIT(decoder_test2, S6E3FC3_DECODER_TEST2, RESUI(decoder_test2)),
	[RES_DECODER_TEST3] = RESINFO_INIT(decoder_test3, S6E3FC3_DECODER_TEST3, RESUI(decoder_test3)),
	[RES_DECODER_TEST4] = RESINFO_INIT(decoder_test4, S6E3FC3_DECODER_TEST4, RESUI(decoder_test4)),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, S6E3FC3_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_PASS_LIST] = RESINFO_IMMUTABLE_INIT(ccd_chksum_pass_list, S6E3FC3_CCD_CHKSUM_PASS_LIST),
#endif
	[RES_PCD] = RESINFO_INIT(pcd, S6E3FC3_PCD, RESUI(pcd)),
	[RES_PCD2] = RESINFO_INIT(pcd2, S6E3FC3_PCD2, RESUI(pcd2)),
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
	DUMP_SELF_MASK_CHECKSUM,
#ifdef CONFIG_USDM_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
	DUMP_PCD,
	DUMP_PCD2,
};

/* TODO: check dump log */
static struct dump_expect rddpm_after_display_on_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
	{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
	{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
	{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
};

static struct dump_expect rddpm_before_sleep_in_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
	{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
	{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Display Mode : ON(NG)" },
};

static struct dump_expect rddsm_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x80, .msg = "TE Mode : OFF(NG)" },
};

static struct dump_expect dsi_err_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x00, .msg = "DSI Protocol Violation" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Data P Lane Contention Detetion" },
	{ .offset = 0, .mask = 0x20, .value = 0x00, .msg = "Invalid Transmission Length" },
	{ .offset = 0, .mask = 0x10, .value = 0x00, .msg = "DSI VC ID Invalid" },
	{ .offset = 0, .mask = 0x08, .value = 0x00, .msg = "DSI Data Type Not Recognized" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Checksum Error" },
	{ .offset = 0, .mask = 0x02, .value = 0x00, .msg = "ECC Error, multi-bit (detected, not corrected)" },
	{ .offset = 0, .mask = 0x01, .value = 0x00, .msg = "ECC Error, single-bit (detected and corrected)" },
	{ .offset = 1, .mask = 0x80, .value = 0x00, .msg = "Data Lane Contention Detection" },
	{ .offset = 1, .mask = 0x40, .value = 0x00, .msg = "False Control Error" },
	{ .offset = 1, .mask = 0x20, .value = 0x00, .msg = "HS RX Timeout" },
	{ .offset = 1, .mask = 0x10, .value = 0x00, .msg = "Low-Power Transmit Sync Error" },
	{ .offset = 1, .mask = 0x08, .value = 0x00, .msg = "Escape Mode Entry Command Error" },
	{ .offset = 1, .mask = 0x04, .value = 0x00, .msg = "EoT Sync Error" },
	{ .offset = 1, .mask = 0x02, .value = 0x00, .msg = "SoT Sync Error" },
	{ .offset = 1, .mask = 0x01, .value = 0x00, .msg = "SoT Error" },
	{ .offset = 2, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
	{ .offset = 3, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
	{ .offset = 4, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
};

static struct dump_expect err_fg_expects[] = {
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "VLIN1 Error" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "VLOUT3 Error" },
	{ .offset = 0, .mask = 0x08, .value = 0x00, .msg = "ELVDD Error" },
};

static struct dump_expect dsie_cnt_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "DSI Error Count" },
};

static struct dump_expect self_diag_expects[] = {
	{ .offset = 0, .mask = 0x40, .value = 0x40, .msg = "Panel Boosting Error" },
};

#ifdef CONFIG_USDM_DDI_CMDLOG
static struct dump_expect cmdlog_expects[] = {
};
#endif

static struct dump_expect self_mask_crc_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x7C, .msg = "Self Mask CRC[0] Error(NG)" },
	{ .offset = 1, .mask = 0xFF, .value = 0x38, .msg = "Self Mask CRC[1] Error(NG)" },
};

static struct dump_expect self_mask_checksum_expects[] = {
};

static struct dump_expect pcd_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "PCD Occur" },
};

static struct dump_expect pcd2_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x5C, .msg = "PCD Setting error" },
	{ .offset = 1, .mask = 0xFF, .value = 0x51, .msg = "PCD Setting error" },
};

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int s6e3fc3_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif

static struct dumpinfo s6e3fc3_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT_V2(rddpm, &s6e3fc3_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM), rddpm_after_display_on_expects),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT_V2(rddpm_sleep_in, &s6e3fc3_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM_BEFORE_SLEEP_IN), rddpm_before_sleep_in_expects),
	[DUMP_RDDSM] = DUMPINFO_INIT_V2(rddsm, &s6e3fc3_restbl[RES_RDDSM], &OLED_FUNC(OLED_DUMP_SHOW_RDDSM), rddsm_expects),
	[DUMP_ERR] = DUMPINFO_INIT_V2(err, &s6e3fc3_restbl[RES_ERR], &OLED_FUNC(OLED_DUMP_SHOW_ERR), dsi_err_expects),
	[DUMP_ERR_FG] = DUMPINFO_INIT_V2(err_fg, &s6e3fc3_restbl[RES_ERR_FG], &OLED_FUNC(OLED_DUMP_SHOW_ERR_FG), err_fg_expects),
	[DUMP_DSI_ERR] = DUMPINFO_INIT_V2(dsi_err, &s6e3fc3_restbl[RES_DSI_ERR], &OLED_FUNC(OLED_DUMP_SHOW_DSI_ERR), dsie_cnt_expects),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT_V2(self_diag, &s6e3fc3_restbl[RES_SELF_DIAG], &OLED_FUNC(OLED_DUMP_SHOW_SELF_DIAG), self_diag_expects),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT_V2(cmdlog, &s6e3fc3_restbl[RES_CMDLOG], &OLED_FUNC(OLED_DUMP_SHOW_CMDLOG), cmdlog_expects),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT_V2(self_mask_checksum, &s6e3fc3_restbl[RES_SELF_MASK_CRC], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), self_mask_crc_expects),
	//[DUMP_SELF_MASK_CHECKSUM] = DUMPINFO_INIT_V2(self_mask_checksum, &s6e3fc3_restbl[RES_SELF_MASK_CHECKSUM], &OLED_FUNC(OLED_DUMP_SHOW_SELF_MASK_CHECKSUM), self_mask_checksum_expects),
	[DUMP_PCD] = DUMPINFO_INIT_V2(pcd, &s6e3fc3_restbl[RES_PCD], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), pcd_expects),
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	[DUMP_PCD2] = DUMPINFO_INIT_V2(pcd2, &s6e3fc3_restbl[RES_PCD2], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), pcd2_expects),
#else
	[DUMP_PCD2] = DUMPINFO_INIT(pcd2, &s6e3fc3_restbl[RES_PCD2], &OLED_FUNC(OLED_DUMP_SHOW_RESOURCE_AND_PANIC)),
#endif
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
	S6E3FC3_VRR_60HS_120HS_TE_HW_SKIP_1,
	S6E3FC3_VRR_60HS,
	MAX_S6E3FC3_VRR,
};

enum {
	S6E3FC3_RESOL_1080x2340
};

enum {
	S6E3FC3_RESOL_1080x2400
};

enum {
	S6E3FC3_VRR_KEY_REFRESH_RATE,
	S6E3FC3_VRR_KEY_REFRESH_MODE,
	S6E3FC3_VRR_KEY_TE_SW_SKIP_COUNT,
	S6E3FC3_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_S6E3FC3_VRR_KEY,
};

static u32 S6E3FC3_VRR_FPS[MAX_S6E3FC3_VRR][MAX_S6E3FC3_VRR_KEY] = {
	[S6E3FC3_VRR_120HS] = { 120, VRR_HS_MODE, 0, 0 },
	[S6E3FC3_VRR_90HS] = { 90, VRR_HS_MODE, 0, 0 },
	[S6E3FC3_VRR_60HS_120HS_TE_HW_SKIP_1] = { 120, VRR_HS_MODE, 0, 1 },
	[S6E3FC3_VRR_60HS] = { 60, VRR_HS_MODE, 0, 0 },
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

//#define S6E3FC3_TSET_PROPERTY ("s6e3fc3_tset")
#define S6E3FC3_ACL_DIM_PROPERTY ("s6e3fc3_acl_dim")
#define S6E3FC3_ACL_OPR_PROPERTY ("s6e3fc3_acl_opr")
#define S6E3FC3_VRR_PROPERTY ("s6e3fc3_vrr")
#define S6E3FC3_VRR_MODE_PROPERTY ("s6e3fc3_vrr_mode")

int s6e3fc3_get_octa_id(struct panel_device *panel, void *buf);
int s6e3fc3_get_cell_id(struct panel_device *panel, void *buf);
int s6e3fc3_get_manufacture_code(struct panel_device *panel, void *buf);
int s6e3fc3_get_manufacture_date(struct panel_device *panel, void *buf);
int s6e3fc3_init(struct common_panel_info *cpi);

#endif /* __S6E3FC3_H__ */
