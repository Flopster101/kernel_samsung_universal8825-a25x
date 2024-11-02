/*
 * linux/drivers/video/fbdev/exynos/panel/aod_drv.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AOD_DRV_H__
#define __AOD_DRV_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>

#include "aod_drv_ioctl.h"

#define AOD_DEV_NAME "self_display"

/* TODO: configs should be moved to Kconfig */
#define SUPPORT_NORMAL_SELF_MOVE
#undef SUPPORT_SELF_GRID

#define IMG_HD_SZ (2)

#define HEADER_SELF_ICON ("IC")
#define HEADER_ANALOG_CLK ("AC")
#define HEADER_DIGITAL_CLK ("DC")

#define AOD_DRV_SELF_MOVE_ON (1)
#define AOD_DRV_SELF_MOVE_OFF (0)

#if defined(SUPPORT_NORMAL_SELF_MOVE)
enum {
	NORMAL_SELF_MOVE_PATTERN_OFF,
	NORMAL_SELF_MOVE_PATTERN_1,
	NORMAL_SELF_MOVE_PATTERN_2,
	NORMAL_SELF_MOVE_PATTERN_3,
	NORMAL_SELF_MOVE_PATTERN_4,
	MAX_NORMAL_SELF_MOVE_PATTERN,
};
#endif

#define SELF_MASK_IMG_SEQ ("self_mask_img_seq")
#define SELF_MASK_ENA_SEQ ("self_mask_ena_seq")
#define SELF_MASK_DIS_SEQ ("self_mask_dis_seq")
#define ANALOG_IMG_SEQ ("analog_img_seq")
#define ANALOG_CTRL_SEQ ("analog_ctrl_seq")
#define ANALOG_DISABLE_SEQ ("analog_disable_seq")
#define DIGITAL_IMG_SEQ ("digital_img_seq")
#define DIGITAL_CTRL_SEQ ("digital_ctrl_seq")
#define SELF_MOVE_ON_SEQ ("self_move_on_seq")
#define SELF_MOVE_OFF_SEQ ("self_move_off_seq")
#define SELF_MOVE_RESET_SEQ ("self_move_reset_seq")
#if defined(SUPPORT_SELF_ICON)
#define SELF_ICON_IMG_SEQ ("self_icon_img_seq")
#define CTRL_ICON_SEQ ("ctrl_icon_seq")
#define DISABLE_ICON_SEQ ("disable_icon_seq")
#endif
#define ICON_GRID_ON_SEQ ("icon_grid_on_seq")
#define ICON_GRID_OFF_SEQ ("icon_grid_off_seq")
#define ENTER_AOD_SEQ ("enter_aod_seq")
#define EXIT_AOD_SEQ ("exit_aod_seq")
#define SET_TIME_SEQ ("set_time_seq")
#define ENABLE_PARTIAL_SCAN_SEQ ("enable_partial_scan_seq")
#define DISABLE_PARTIAL_SCAN_SEQ ("disable_partial_scan_seq")
#if defined(SUPPORT_NORMAL_SELF_MOVE)
#define ENABLE_SELF_MOVE_SEQ ("enable_self_move_seq")
#define DISABLE_SELF_MOVE_SEQ ("disable_self_move_seq")
#endif
#define SELF_MASK_CRC_SEQ ("self_mask_crc_seq")
#define SELF_MASK_DATA_CHECK_SEQ ("self_mask_data_check_seq")

enum AOD_MAPTBL {
	SELF_MASK_CTRL_MAPTBL = 0,
	ANALOG_POS_MAPTBL,
	ANALOG_CLK_CTRL_MAPTBL,
	DIGITAL_CLK_CTRL_MAPTBL,
	DIGITAL_POS_MAPTBL,
	DIGITAL_BLK_MAPTBL,
	SET_TIME_MAPTBL,
#if defined(SUPPORT_SELF_ICON)
	CTRL_ICON_MAPTBL,
#endif
	SELF_MOVE_MAPTBL,
	SELF_MOVE_POS_MAPTBL,
	SELF_MOVE_RESET_MAPTBL,
	//todo ha9 need additional config for ha9
	SET_TIME_RATE_MAPTBL,
	SET_DIGITAL_COLOR_MAPTBL,
	SET_DIGITAL_UN_WIDTH_MAPTBL,
	SET_PARTIAL_MODE_MAPTBL,
	SET_PARTIAL_AREA_MAPTBL,
	SET_PARTIAL_HLPM_MAPTBL,
#if defined(SUPPORT_NORMAL_SELF_MOVE)
	SELF_MOVE_PATTERN_MAPTBL,
#endif
	MAX_AOD_MAPTBL,
};
struct aod_dev_info;

struct aod_custom_ops {
	int (*self_mask_data_check)(struct aod_dev_info *aod);
};

struct aod_tune {
	char *name;
	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;
	int self_mask_en;
	u8 *self_mask_crc;
	u32 self_mask_crc_len;
	struct aod_custom_ops custom_ops;
};

struct clk_pos {
	int pos_x;
	int pos_y;
};

struct img_info {
	u8 *buf;
	u32 size;
	unsigned int up_flag;
};

struct aod_ioctl_props {
	int self_mask_en;
	int self_move_en;
	int self_reset_cnt;
	/* Reg: 0x77, Offset : 0x03 */
	struct aod_cur_time cur_time;
	/* Reg: 0x77, Offset: 0x09 */
	struct clk_pos pos;
	/* Reg: 0x77, Offset: 0x07 */

	struct self_icon_info icon;
	struct self_grid_info self_grid;
	struct analog_clk_info analog;
	struct digital_clk_info digital;
	struct partial_scan_info partial;

	int clk_rate;
	int debug_interval;
	int debug_rotate;
	int debug_force_update;

	int first_clk_update;
#if defined(SUPPORT_NORMAL_SELF_MOVE)
	int self_move_pattern;
#endif
	int prev_rotate;
	u8 *self_mask_crc;
	/*
		because len is different in each ddi
		0 : unsupport
	*/
	int self_mask_crc_len;
};

struct aod_dev_info {
	int reset_flag;

	struct panel_mutex lock;
	struct aod_ioctl_props props;
	struct notifier_block fb_notif;
	char *temp;

	struct aod_ops *ops;
	struct aod_custom_ops *custom_ops;
	struct miscdevice dev;

	struct img_info icon_img;
	struct img_info dc_img;
	struct img_info ac_img;
};

struct aod_ops {
	int (*init_panel)(struct aod_dev_info *aod_dev, int lock);
	int (*enter_to_lpm)(struct aod_dev_info *aod_dev);
	int (*exit_from_lpm)(struct aod_dev_info *aod_dev);
	int (*doze_suspend)(struct aod_dev_info *aod_dev);
	int (*power_off)(struct aod_dev_info *aod_dev);
#if defined(SUPPORT_NORMAL_SELF_MOVE)
	int (*self_move_pattern_update)(struct aod_dev_info *aod_dev);
#endif
	int (*black_grid_on)(struct aod_dev_info *aod_dev);
	int (*black_grid_off)(struct aod_dev_info *aod_dev);
};

int aod_drv_prepare(struct panel_device *panel,
		struct aod_tune *aod_tune);
int aod_drv_unprepare(struct panel_device *panel);
int aod_drv_probe(struct panel_device *panel, struct aod_tune *aod_tune);
int aod_drv_remove(struct panel_device *panel);
int panel_do_aod_seqtbl_by_name(struct aod_dev_info *aod, char *seqname);
bool check_aod_seqtbl_exist(struct aod_dev_info *aod, char *seqname);
int panel_do_aod_seqtbl_by_name_nolock(struct aod_dev_info *aod, char *seqname);
#endif //__AOD_DRV_H__
