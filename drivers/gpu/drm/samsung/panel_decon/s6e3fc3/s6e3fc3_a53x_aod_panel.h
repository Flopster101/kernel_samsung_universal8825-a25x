/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc3/s6e3fc3_a53x_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC3_A53X_AOD_PANEL_H__
#define __S6E3FC3_A53X_AOD_PANEL_H__

#include "s6e3fc3_aod.h"
#include "s6e3fc3_a53x_self_mask_img.h"
#include "s6e3fc3_a53x_self_mask_img_factory.h"

__visible_for_testing bool s6e3fc3_is_a52_panel(struct panel_device *panel)
{
	struct panel_info *panel_data;

	if (!panel)
		return false;

	panel_data = &panel->panel_data;

	if (panel_data->id[0] == 0x90 && panel_data->id[1] == 0x40 && panel_data->id[2] & 0x02)
		return true;

	return false;
}

static u8 S6E3FC3_AOD_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 S6E3FC3_AOD_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };

static u8 S6E3FC3_AOD_KEY2_ENABLE[] = { 0xF1, 0x5A, 0x5A };
static u8 S6E3FC3_AOD_KEY2_DISABLE[] = { 0xF1, 0xA5, 0xA5 };

static u8 S6E3FC3_AOD_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 S6E3FC3_AOD_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };

static DEFINE_STATIC_PACKET(s6e3fc3_aod_l1_key_enable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3fc3_aod_l1_key_disable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY1_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3fc3_aod_l2_key_enable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3fc3_aod_l2_key_disable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(s6e3fc3_aod_l3_key_enable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(s6e3fc3_aod_l3_key_disable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_KEY3_DISABLE, 0);

static DEFINE_PANEL_MDELAY(s6e3fc3_aod_self_spsram_write_delay, 1);
static DEFINE_PANEL_UDELAY(s6e3fc3_aod_self_spsram_sel_delay, 1);

static DEFINE_PANEL_UDELAY(s6e3fc3_aod_self_mask_checksum_1frame_delay, 16700);
static DEFINE_PANEL_UDELAY(s6e3fc3_aod_self_mask_checksum_2frame_delay, 33400);
static DEFINE_PANEL_UDELAY(s6e3fc3_aod_time_update_delay, 33400);

static DEFINE_COND(a53x_aod_cond_is_a52_panel, s6e3fc3_is_a52_panel);

static DEFINE_PANEL_KEY(s6e3fc3_aod_l1_key_enable, CMD_LEVEL_1,
	KEY_ENABLE, &PKTINFO(s6e3fc3_aod_l1_key_enable));
static DEFINE_PANEL_KEY(s6e3fc3_aod_l1_key_disable, CMD_LEVEL_1,
	KEY_DISABLE, &PKTINFO(s6e3fc3_aod_l1_key_disable));

static DEFINE_PANEL_KEY(s6e3fc3_aod_l2_key_enable, CMD_LEVEL_2,
	KEY_ENABLE, &PKTINFO(s6e3fc3_aod_l2_key_enable));
static DEFINE_PANEL_KEY(s6e3fc3_aod_l2_key_disable, CMD_LEVEL_2,
	KEY_DISABLE, &PKTINFO(s6e3fc3_aod_l2_key_disable));

static DEFINE_PANEL_KEY(s6e3fc3_aod_l3_key_enable, CMD_LEVEL_3,
	KEY_ENABLE, &PKTINFO(s6e3fc3_aod_l3_key_enable));
static DEFINE_PANEL_KEY(s6e3fc3_aod_l3_key_disable, CMD_LEVEL_3,
	KEY_DISABLE, &PKTINFO(s6e3fc3_aod_l3_key_disable));

static struct keyinfo KEYINFO(s6e3fc3_aod_l1_key_enable);
static struct keyinfo KEYINFO(s6e3fc3_aod_l1_key_disable);
static struct keyinfo KEYINFO(s6e3fc3_aod_l2_key_enable);
static struct keyinfo KEYINFO(s6e3fc3_aod_l2_key_disable);
static struct keyinfo KEYINFO(s6e3fc3_aod_l3_key_enable);
static struct keyinfo KEYINFO(s6e3fc3_aod_l3_key_disable);

static struct maptbl s6e3fc3_aod_maptbl[] = {
	[SELF_MASK_CTRL_MAPTBL] = DEFINE_0D_MAPTBL(s6e3fc3_aod_self_mask, s6e3fc3_init_self_mask_ctrl, NULL, s6e3fc3_copy_self_mask_ctrl),
};

// --------------------- Image for self mask image ---------------------

static char S6E3FC3_AOD_RESET_SD_PATH[] = {
	0x75,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_reset_sd_path, DSI_PKT_TYPE_WR, S6E3FC3_AOD_RESET_SD_PATH, 0);


static char S6E3FC3_AOD_SELF_MASK_SD_PATH[] = {
	0x75,
	0x10,
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_sd_path, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_SD_PATH, 0);

// --------------------- End of self mask image ---------------------


// --------------------- Image for self mask control ---------------------
#ifdef CONFIG_SELFMASK_FACTORY
static char S6E3FC3_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x60, 0x09, 0x61,
	0x09, 0x62, 0x09, 0x63
};
#else
static char S6E3FC3_AOD_SELF_MASK_ENA[] = {
	0x7A,
	0x01, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2B,
	0x08, 0x34, 0x09, 0x5F
};
#endif

static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_ctrl_ena, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_ENA, 0);

static void *s6e3fc3_a53x_aod_self_mask_ena_cmdtbl[] = {
	&CONDINFO_IF(a53x_aod_cond_is_a52_panel),
	&CONDINFO_EL(a53x_aod_cond_is_a52_panel),
		&KEYINFO(s6e3fc3_aod_l1_key_enable),
		&PKTINFO(s6e3fc3_aod_self_mask_ctrl_ena),
		&KEYINFO(s6e3fc3_aod_l1_key_disable),
	&CONDINFO_FI(a53x_aod_cond_is_a52_panel),
};

static char S6E3FC3_AOD_SELF_MASK_DISABLE[] = {
	0x7A,
	0x00,
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_disable, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_DISABLE, 0);

static void *s6e3fc3_a53x_aod_self_mask_dis_cmdtbl[] = {
	&KEYINFO(s6e3fc3_aod_l1_key_enable),
	&PKTINFO(s6e3fc3_aod_self_mask_disable),
	&KEYINFO(s6e3fc3_aod_l1_key_disable),
};
// --------------------- End of self mask control ---------------------

// --------------------- check sum control ----------------------------
static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3fc3_a53x_aod_self_mask_img_pkt,
		DSI_PKT_TYPE_WR_SR, S6E3FC3_A53X_SELF_MASK_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static DEFINE_STATIC_PACKET_WITH_OPTION(s6e3fc3_a53x_aod_self_mask_crc_img_pkt,
		DSI_PKT_TYPE_WR_SR, S6E3FC3_A53X_SELF_MASK_CRC_IMG, 0, PKT_OPTION_SR_ALIGN_16);

static char S6E3FC3_AOD_SELF_MASK_CRC_ON1[] = {
	0xD8,
	0x16,
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_crc_on1, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_CRC_ON1, 0x27);

static char S6E3FC3_AOD_SELF_MASK_DBIST_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_dbist_on, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_DBIST_ON, 0);

static char S6E3FC3_AOD_SELF_MASK_DBIST_OFF[] = {
	0xBF, 0x00
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_dbist_off, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_DBIST_OFF, 0);

static char S6E3FC3_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM[] = {
	0x7A,
	0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0xF4, 0x02, 0x33,
	0x09, 0x60, 0x09, 0x61, 0x00, 0x00, 0xFF, 0xFF,
	0xFF
};

static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_for_checksum, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_ENABLE_FOR_CHECKSUM, 0);

static char S6E3FC3_AOD_SELF_MASK_RESTORE[] = {
	0x7A,
	0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x09, 0x60, 0x09, 0x61,
	0x09, 0x62, 0x09, 0x63, 0x00, 0x00, 0x00, 0x00,
	0x00
};
static DEFINE_STATIC_PACKET(s6e3fc3_aod_self_mask_restore, DSI_PKT_TYPE_WR, S6E3FC3_AOD_SELF_MASK_RESTORE, 0);

static void *s6e3fc3_a53x_aod_self_mask_checksum_cmdtbl[] = {
	&KEYINFO(s6e3fc3_aod_l1_key_enable),
	&KEYINFO(s6e3fc3_aod_l2_key_enable),
	&KEYINFO(s6e3fc3_aod_l3_key_enable),
	&PKTINFO(s6e3fc3_aod_self_mask_crc_on1),
	&PKTINFO(s6e3fc3_aod_self_mask_dbist_on),
	&PKTINFO(s6e3fc3_aod_self_mask_disable),
	&DLYINFO(s6e3fc3_aod_self_mask_checksum_1frame_delay),
	&PKTINFO(s6e3fc3_aod_self_mask_sd_path),
	&DLYINFO(s6e3fc3_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3fc3_a53x_aod_self_mask_crc_img_pkt),
	&DLYINFO(s6e3fc3_aod_self_spsram_write_delay),
	&PKTINFO(s6e3fc3_aod_reset_sd_path),
	&DLYINFO(s6e3fc3_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3fc3_aod_self_mask_for_checksum),
	&DLYINFO(s6e3fc3_aod_self_mask_checksum_2frame_delay),
	&s6e3fc3_restbl[RES_SELF_MASK_CHECKSUM],
	&PKTINFO(s6e3fc3_aod_self_mask_restore),
	&PKTINFO(s6e3fc3_aod_self_mask_dbist_off),
	&KEYINFO(s6e3fc3_aod_l3_key_disable),
	&KEYINFO(s6e3fc3_aod_l2_key_disable),
	&KEYINFO(s6e3fc3_aod_l1_key_disable),
};

// --------------------- end of check sum control ----------------------------

static void *s6e3fc3_a53x_aod_self_mask_img_cmdtbl[] = {
	&KEYINFO(s6e3fc3_aod_l1_key_enable),
	&PKTINFO(s6e3fc3_aod_self_mask_sd_path),
	&DLYINFO(s6e3fc3_aod_self_spsram_sel_delay),
	&PKTINFO(s6e3fc3_a53x_aod_self_mask_img_pkt),
	&DLYINFO(s6e3fc3_aod_self_spsram_write_delay),
	&PKTINFO(s6e3fc3_aod_reset_sd_path),
	&KEYINFO(s6e3fc3_aod_l1_key_disable),
};

static struct seqinfo s6e3fc3_a53x_aod_seqtbl[MAX_AOD_SEQ] = {
	[SELF_MASK_IMG_SEQ] = SEQINFO_INIT("self_mask_img", s6e3fc3_a53x_aod_self_mask_img_cmdtbl),
	[SELF_MASK_ENA_SEQ] = SEQINFO_INIT("self_mask_ena", s6e3fc3_a53x_aod_self_mask_ena_cmdtbl),
	[SELF_MASK_DIS_SEQ] = SEQINFO_INIT("self_mask_dis", s6e3fc3_a53x_aod_self_mask_dis_cmdtbl),
	[SELF_MASK_CHECKSUM_SEQ] = SEQINFO_INIT("self_mask_checksum", s6e3fc3_a53x_aod_self_mask_checksum_cmdtbl),
};

static struct aod_tune s6e3fc3_a53x_aod = {
	.name = "s6e3fc3_a53x_aod",
	.nr_seqtbl = ARRAY_SIZE(s6e3fc3_a53x_aod_seqtbl),
	.seqtbl = s6e3fc3_a53x_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(s6e3fc3_aod_maptbl),
	.maptbl = s6e3fc3_aod_maptbl,
	.self_mask_en = true,
};
#endif
