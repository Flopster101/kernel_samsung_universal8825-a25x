/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "oled_common_aod.h"

void oled_maptbl_copy_aod_digital_pos(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;
	if (props->digital.en == 0) {
		panel_info("digital clk was disabled\n");
		return;
	}

	dst[DIG_CLK_POS1_X1_REG] = (u8)(props->digital.pos1_x >> 8);
	dst[DIG_CLK_POS1_X2_REG] = (u8)(props->digital.pos1_x & 0xff);
	dst[DIG_CLK_POS1_Y1_REG] = (u8)(props->digital.pos1_y >> 8);
	dst[DIG_CLK_POS1_Y2_REG] = (u8)(props->digital.pos1_y & 0xff);

	dst[DIG_CLK_POS2_X1_REG] = (u8)(props->digital.pos2_x >> 8);
	dst[DIG_CLK_POS2_X2_REG] = (u8)(props->digital.pos2_x & 0xff);
	dst[DIG_CLK_POS2_Y1_REG] = (u8)(props->digital.pos2_y >> 8);
	dst[DIG_CLK_POS2_Y2_REG] = (u8)(props->digital.pos2_y & 0xff);

	dst[DIG_CLK_POS3_X1_REG] = (u8)(props->digital.pos3_x >> 8);
	dst[DIG_CLK_POS3_X2_REG] = (u8)(props->digital.pos3_x & 0xff);
	dst[DIG_CLK_POS3_Y1_REG] = (u8)(props->digital.pos3_y >> 8);
	dst[DIG_CLK_POS3_Y2_REG] = (u8)(props->digital.pos3_y & 0xff);

	dst[DIG_CLK_POS4_X1_REG] = (u8)(props->digital.pos4_x >> 8);
	dst[DIG_CLK_POS4_X2_REG] = (u8)(props->digital.pos4_x & 0xff);
	dst[DIG_CLK_POS4_Y1_REG] = (u8)(props->digital.pos4_y >> 8);
	dst[DIG_CLK_POS4_Y2_REG] = (u8)(props->digital.pos4_y & 0xff);

	dst[DIG_CLK_WIDTH1_REG] = (u8)(props->digital.img_width >> 8);
	dst[DIG_CLK_WIDTH2_REG] = (u8)(props->digital.img_width & 0xff);
	dst[DIG_CLK_HEIGHT1_REG] = (u8)(props->digital.img_height >> 8);
	dst[DIG_CLK_HEIGHT2_REG] = (u8)(props->digital.img_height & 0xff);
}

void oled_maptbl_copy_aod_time(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	dst[TIME_HH_REG] = props->cur_time.cur_h;
	dst[TIME_MM_REG] = props->cur_time.cur_m;
	dst[TIME_SS_REG] = props->cur_time.cur_s;
	dst[TIME_MSS_REG] = props->cur_time.cur_ms;

	panel_info("%x %x %x\n",
			dst[TIME_HH_REG], dst[TIME_MM_REG], dst[TIME_SS_REG]);
}

void oled_maptbl_copy_aod_timer_rate(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	/* in case of analog set comp_en */
	if ((props->analog.en) && (props->first_clk_update == 0))
		dst[2] |= 0x10;
	else
		dst[2] &= ~0x10;

	switch (props->cur_time.interval) {
	case ALG_INTERVAL_100m:
		dst[1] = 0x3;
		dst[2] = (dst[2] & ~0x03);
		break;
	case ALG_INTERVAL_200m:
		dst[1] = 0x06;
		dst[2] = (dst[2] & ~0x03) | 0x01;
		break;
	case ALG_INTERVAL_500m:
		dst[1] = 0x0f;
		dst[2] = (dst[2] & ~0x03) | 0x02;
		break;
	case ALG_INTERVAL_1000m:
		dst[1] = 0x1e;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	case INTERVAL_DEBUG:
		dst[1] = 0x01;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	default:
		panel_info("invalid interval:%d\n",
				props->cur_time.interval);
		break;
	}
	panel_info("dst[1]:%x, dst[2]:%x\n",
			dst[1], dst[2]);
}

void oled_maptbl_copy_aod_timer_rate_hop(struct maptbl *tbl, u8 *dst)
{
	u8 value = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	dst[2] &= ~0x10;

	if (props->analog.en) {
		value = SELF_IP_HOP_SS_EN | SELF_IP_HOP_MSS_EN;

		if (props->first_clk_update == 0)
			dst[2] |= 0x10;
	}

	switch (props->cur_time.interval) {
	case ALG_INTERVAL_100m:
		value |= 0x03;
		dst[2] = (dst[2] & ~0x03);
		break;
	case ALG_INTERVAL_200m:
		value |= 0x06;
		dst[2] = (dst[2] & ~0x03) | 0x01;
		break;
	case ALG_INTERVAL_500m:
		value |= 0x0f;
		dst[2] = (dst[2] & ~0x03) | 0x02;
		break;
	case ALG_INTERVAL_1000m:
		value |= 0x1e;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	case INTERVAL_DEBUG:
		value |= 0x01;
		dst[2] = (dst[2] & ~0x03) | 0x03;
		break;
	default:
		panel_info("invalid interval:%d\n",
				props->cur_time.interval);
		break;
	}

	dst[1] = value;
	panel_info("dst[1]:%x, dst[2]:%x\n", dst[1], dst[2]);
}

void oled_maptbl_copy_aod_self_move_on_ctrl(struct maptbl *tbl, u8 *dst)
{
	u8 enable = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->self_move_en)
		enable |= FB_SELF_MOVE_EN;

	dst[SM_ENABLE_REG] = enable;

	panel_info("%x\n", dst[SM_ENABLE_REG]);
}

void oled_maptbl_copy_aod_analog_pos(struct maptbl *tbl, u8 *dst)
{
	int pos_x, pos_y;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	switch (props->analog.rotate) {
	case ALG_ROTATE_0:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_0;
		break;
	case ALG_ROTATE_90:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_90;
		break;
	case ALG_ROTATE_180:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_180;
		break;
	case ALG_ROTATE_270:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_270;
		break;
	default:
		dst[ANALOG_ROT_REG] = ALG_ROTATE_0;
		panel_err("undefined rotation mode : %d\n",
				props->analog.rotate);
		break;
	}

	pos_x = props->analog.pos_x;
	pos_y = props->analog.pos_y;

	if (pos_x < 0) {
		panel_err("invalid pos_x %d\n", pos_x);
		pos_x = 0;
	}

	if (pos_y < 0) {
		panel_err("invalid pos_y %d \n", pos_y);
		pos_y = 0;
	}

	dst[ANALOG_POS_X1_REG] = (u8)(pos_x >> 8);
	dst[ANALOG_POS_X2_REG] = (u8)(pos_x & 0xff);
	dst[ANALOG_POS_Y1_REG] = (u8)(pos_y >> 8);
	dst[ANALOG_POS_Y2_REG] = (u8)(pos_y & 0xff);

	panel_dbg("%x %x %x %x\n",
		dst[ANALOG_POS_X1_REG], dst[ANALOG_POS_X2_REG],
		dst[ANALOG_POS_Y1_REG], dst[ANALOG_POS_Y2_REG]);
}

void oled_maptbl_copy_aod_analog_en(struct maptbl *tbl, u8 *dst)
{
	u8 en_reg = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->analog.en)
		en_reg = SC_A_CLK_EN | SC_DISP_ON;

	dst[ANALOG_EN_REG] = en_reg;

	panel_dbg("%x %x %x\n", dst[0], dst[1], dst[2]);
}

void oled_maptbl_copy_aod_digital_en(struct maptbl *tbl, u8 *dst)
{
	u8 en_reg = 0;
	u8 disp_format = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->digital.en) {
		en_reg = SC_D_CLK_EN | SC_DISP_ON;
		if (props->digital.en_hh)
			disp_format |= (props->digital.en_hh & 0x03) << 2;

		if (props->digital.en_mm)
			disp_format |= (props->digital.en_mm & 0x03);

		if (props->cur_time.disp_24h)
			disp_format |= (props->cur_time.disp_24h & 0x03) << 4;

		dst[DIGITAL_UN_REG] = props->digital.unicode_attr;
		dst[DIGITAL_FMT_REG] = disp_format;
	}

	dst[DIGITAL_EN_REG] = en_reg;

	panel_info("%x %x %x %x\n", dst[0], dst[1], dst[2], dst[3]);
}

int oled_maptbl_getidx_aod_self_mode_pos(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	switch (props->cur_time.interval) {
	case ALG_INTERVAL_100m:
		panel_info("interval : 100msec\n");
		row = 0;
		break;
	case ALG_INTERVAL_200m:
		panel_info("interval : 200msec\n");
		row = 1;
		break;
	case ALG_INTERVAL_500m:
		panel_info("interval : 500msec\n");
		row = 2;
		break;
	case ALG_INTERVAL_1000m:
		panel_info("interval : 1sec\n");
		row = 3;
		break;
	case INTERVAL_DEBUG:
		panel_info("interval : debug\n");
		row = 4;
		break;
	default:
		panel_info("invalid interval:%d\n",
				props->cur_time.interval);
		row = 0;
		break;
	}
	return maptbl_index(tbl, 0, row, 0);
}

void oled_maptbl_copy_aod_self_move_reset(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	props->self_reset_cnt++;

	dst[REG_MOVE_DSP_X] = (u8)props->self_reset_cnt;

	panel_info("%x:%x:%x:%x:%x\n",
			dst[0], dst[1], dst[2], dst[3], dst[4]);
}

void oled_maptbl_copy_aod_icon_ctrl(struct maptbl *tbl, u8 *dst)
{
	u8 enable = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	panel_info("%d\n", props->icon.en);

	if (props->icon.en) {
		dst[ICON_REG_XPOS0] = (u8)(props->icon.pos_x >> 8);
		dst[ICON_REG_XPOS1] = (u8)(props->icon.pos_x & 0x00ff);
		dst[ICON_REG_YPOS0] = (u8)(props->icon.pos_y >> 8);
		dst[ICON_REG_YPOS1] = (u8)(props->icon.pos_y & 0x00ff);

		dst[ICON_REG_WIDTH0] = (u8)(props->icon.width >> 8);
		dst[ICON_REG_WIDTH1] = (u8)(props->icon.width & 0xff);
		dst[ICON_REG_HEIGHT0] = (u8)(props->icon.height >> 8);
		dst[ICON_REG_HEIGHT1] = (u8)(props->icon.height & 0x00ff);

		dst[ICON_REG_COLOR0] = (u8)(props->icon.color >> 24);
		dst[ICON_REG_COLOR1] = (u8)(props->icon.color >> 16);
		dst[ICON_REG_COLOR2] = (u8)(props->icon.color >> 8);
		dst[ICON_REG_COLOR3] = (u8)(props->icon.color & 0x00ff);

		enable = ICON_ENABLE;
	}

	dst[ICON_REG_EN] = enable;

	panel_info("%x:%x:%x:%x:%x\n",
			dst[0], dst[1], dst[2], dst[3], dst[4]);
}

void oled_maptbl_copy_aod_digital_color(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (!props->digital.en)
		return;

	dst[DIG_COLOR_ALPHA_REG] = (u8)((props->digital.color >> 24) & 0xff);
	dst[DIG_COLOR_RED_REG] = (u8)((props->digital.color >> 16) & 0xff);
	dst[DIG_COLOR_GREEN_REG] = (u8)((props->digital.color >> 8) & 0xff);
	dst[DIG_COLOR_BLUE_REG] = (u8)(props->digital.color & 0xff);
}

void oled_maptbl_copy_aod_digital_un_width(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (!props->digital.en)
		return;

	dst[DIG_UN_WIDTH0] = (u8)((props->digital.unicode_width >> 8) & 0xff);
	dst[DIG_UN_WIDTH1] = (u8)(props->digital.unicode_width & 0xff);
}

void oled_maptbl_copy_aod_partial_mode(struct maptbl *tbl, u8 *dst)
{
	u8 enable = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->partial.hlpm_scan_en) {
		enable = dst[SCAN_ENABLE_REG] | ENABLE_PARTIAL_HLPM_VAL;
		dst[SCAN_MODE_REG] = props->partial.hlpm_mode_sel;
	}

	if (props->partial.scan_en)
		enable |= dst[SCAN_ENABLE_REG] | ENABLE_PARTIAL_SCAN_VAL;

	dst[SCAN_ENABLE_REG] = enable;

	panel_info("enable : 0x%x\n", enable);
	/* todo: remove temporary code for always on partial hlpm below*/
	dst[SCAN_ENABLE_REG] = 0x81;
	panel_info("force enable partial hlpm to 0x81\n");
}

void oled_maptbl_copy_aod_partial_area(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->partial.scan_en) {
		dst[PARTIAL_AREA_SL0_REG] = (u8)(props->partial.scan_sl >> 8);
		dst[PARTIAL_AREA_SL1_REG] = (u8)(props->partial.scan_sl & 0xff);
		dst[PARTIAL_AREA_EL0_REG] = (u8)(props->partial.scan_el >> 8);
		dst[PARTIAL_AREA_EL1_REG] = (u8)(props->partial.scan_el & 0xff);
	}
}

void oled_maptbl_copy_aod_partial_hlpm(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	if (props->partial.hlpm_scan_en) {
		dst[PARTIAL_HLPM1_L0_REG] = (u8)(props->partial.hlpm_area_1 >> 8);
		dst[PARTIAL_HLPM1_L1_REG] = (u8)(props->partial.hlpm_area_1 & 0xff);

		dst[PARTIAL_HLPM2_L0_REG] = (u8)(props->partial.hlpm_area_2 >> 8);
		dst[PARTIAL_HLPM2_L1_REG] = (u8)(props->partial.hlpm_area_2 & 0xff);

		dst[PARTIAL_HLPM3_L0_REG] = (u8)(props->partial.hlpm_area_3 >> 8);
		dst[PARTIAL_HLPM3_L1_REG] = (u8)(props->partial.hlpm_area_3 & 0xff);

		dst[PARTIAL_HLPM4_L0_REG] = (u8)(props->partial.hlpm_area_4 >> 8);
		dst[PARTIAL_HLPM4_L1_REG] = (u8)(props->partial.hlpm_area_4 & 0xff);
	}
}

#ifdef SUPPORT_NORMAL_SELF_MOVE
int oled_maptbl_getidx_aod_self_pattern(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct aod_dev_info *aod;
	struct aod_ioctl_props *props;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	aod = &panel->aod;
	props = &aod->props;

	switch (props->self_move_pattern) {
	case 1:
	case 3:
		panel_info("pattern : %d\n",
				props->self_move_pattern);
		row = 0;
		break;
	case 2:
	case 4:
		panel_info("pattern : %d\n",
				props->self_move_pattern);
		row = 1;
		break;
	default:
		panel_info("invalid pattern:%d\n",
				props->self_move_pattern);
		row = 0;
		break;
	}

	return maptbl_index(tbl, 0, row, 0);
}

void oled_maptbl_copy_aod_self_move_pattern(struct maptbl *tbl, u8 *dst)
{
	int idx;
	struct panel_device *panel;
	struct aod_dev_info *aod;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;
	aod = &panel->aod;
	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * maptbl_get_sizeof_copy(tbl));

#ifdef FAST_TIMER
	dst[7] = 0x22;
#endif
	panel_info("%x:%x:%x:%x:%x:%x:%x:(%x):%x\n",
		dst[0], dst[1], dst[2], dst[3],
		dst[4], dst[5], dst[6], dst[7], dst[8]);
}
#endif

MODULE_DESCRIPTION("oled_common_aod driver");
MODULE_LICENSE("GPL");
