// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include "panel_kunit.h"
#include "panel_drv.h"
#include "mdnie.h"
#include "panel_debug.h"
#include "panel_property.h"
#ifdef CONFIG_USDM_PANEL_COPR
#include "copr.h"
#endif
#ifdef CONFIG_USDM_PANEL_DPUI
#include "dpui.h"
#endif
#ifdef CONFIG_USDM_PANEL_TESTMODE
#include "panel_testmode.h"
#endif

#ifdef MDNIE_SELF_TEST
int g_coord_x = MIN_WCRD_X;
int g_coord_y = MIN_WCRD_Y;
#endif

static const char * const scr_white_mode_name[] = {
	[SCR_WHITE_MODE_NONE] = "none",
	[SCR_WHITE_MODE_COLOR_COORDINATE] = "coordinate",
	[SCR_WHITE_MODE_ADJUST_LDU] = "adjust-ldu",
	[SCR_WHITE_MODE_SENSOR_RGB] = "sensor-rgb",
};

static const char * const mdnie_mode_name[] = {
	[MDNIE_OFF_MODE] = "off",
	[MDNIE_BYPASS_MODE] = "bypass",
	[MDNIE_LIGHT_NOTIFICATION_MODE] = "light_notification",
	[MDNIE_ACCESSIBILITY_MODE] = "accessibility",
	[MDNIE_COLOR_LENS_MODE] = "color_lens",
	[MDNIE_HDR_MODE] = "hdr",
	[MDNIE_HMD_MODE] = "hmd",
	[MDNIE_NIGHT_MODE] = "night",
	[MDNIE_HBM_CE_MODE] = "hbm_ce",
	[MDNIE_SCENARIO_MODE] = "scenario",
};

static const char * const scenario_mode_name[] = {
	[DYNAMIC] = "dynamic",
	[STANDARD] = "standard",
	[NATURAL] = "natural",
	[MOVIE] = "movie",
	[AUTO] = "auto",
};

static const char * const scenario_name[] = {
	[UI_MODE] = "ui",
	[VIDEO_NORMAL_MODE] = "video_normal",
	[CAMERA_MODE] = "camera",
	[NAVI_MODE] = "navi",
	[GALLERY_MODE] = "gallery",
	[VT_MODE] = "vt",
	[BROWSER_MODE] = "browser",
	[EBOOK_MODE] = "ebook",
	[EMAIL_MODE] = "email",
	[GAME_LOW_MODE] = "game_low",
	[GAME_MID_MODE] = "game_mid",
	[GAME_HIGH_MODE] = "game_high",
	[VIDEO_ENHANCER] = "video_enhancer",
	[VIDEO_ENHANCER_THIRD] = "video_enhancer_third",
	[HMD_8_MODE] = "hmd_8",
	[HMD_16_MODE] = "hmd_16",
};

static const char * const accessibility_name[] = {
	[ACCESSIBILITY_OFF] = "off",
	[NEGATIVE] = "negative",
	[COLOR_BLIND] = "color_blind",
	[SCREEN_CURTAIN] = "screen_curtain",
	[GRAYSCALE] = "grayscale",
	[GRAYSCALE_NEGATIVE] = "grayscale_negative",
	[COLOR_BLIND_HBM] = "color_blind_hbm",
};

static struct panel_prop_enum_item mdnie_mode_enum_items[MAX_MDNIE_MODE] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_OFF_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_BYPASS_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_ACCESSIBILITY_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_LIGHT_NOTIFICATION_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_COLOR_LENS_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_HDR_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_HMD_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_NIGHT_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_HBM_CE_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_SCENARIO_MODE),
};

static struct panel_prop_enum_item mdnie_scenario_enum_items[SCENARIO_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(UI_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VIDEO_NORMAL_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CAMERA_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NAVI_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GALLERY_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VT_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(BROWSER_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(EBOOK_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(EMAIL_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GAME_LOW_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GAME_MID_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GAME_HIGH_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VIDEO_ENHANCER),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VIDEO_ENHANCER_THIRD),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_8_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_16_MODE),
};

static struct panel_prop_enum_item mdnie_scenario_mode_enum_items[MODE_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(DYNAMIC),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(STANDARD),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NATURAL),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MOVIE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(AUTO),
};

static struct panel_prop_enum_item mdnie_screen_mode_enum_items[MAX_MDNIE_SCREEN_MODE] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_SCREEN_MODE_VIVID),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(MDNIE_SCREEN_MODE_NATURAL),
};

static struct panel_prop_enum_item mdnie_bypass_enum_items[BYPASS_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(BYPASS_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(BYPASS_ON),
};

static struct panel_prop_enum_item mdnie_accessibility_enum_items[ACCESSIBILITY_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ACCESSIBILITY_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NEGATIVE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_BLIND),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SCREEN_CURTAIN),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GRAYSCALE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GRAYSCALE_NEGATIVE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_BLIND_HBM),
};

static struct panel_prop_enum_item mdnie_hmd_enum_items[HMD_MDNIE_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_MDNIE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_3000K),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_4000K),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_5000K),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_6500K),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HMD_7500K),
};

static struct panel_prop_enum_item mdnie_hbm_ce_enum_items[HBM_CE_MODE_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HBM_CE_MODE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HBM_CE_MODE_ON),
};

static struct panel_prop_enum_item mdnie_night_mode_enum_items[NIGHT_MODE_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NIGHT_MODE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NIGHT_MODE_ON),
};

static struct panel_prop_enum_item mdnie_anti_glare_enum_items[ANTI_GLARE_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANTI_GLARE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANTI_GLARE_ON),
};

static struct panel_prop_enum_item mdnie_color_lens_enum_items[COLOR_LENS_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_ON),
};

static struct panel_prop_enum_item mdnie_color_lens_color_enum_items[COLOR_LENS_COLOR_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_BLUE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_AZURE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_CYAN),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_SPRING_GREEN),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_GREEN),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_CHARTREUSE_GREEN),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_YELLOW),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_ORANGE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_RED),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_ROSE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_MAGENTA),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_COLOR_VIOLET),
};

static struct panel_prop_enum_item mdnie_color_lens_level_enum_items[COLOR_LENS_LEVEL_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_20P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_25P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_30P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_35P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_40P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_45P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_50P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_55P),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(COLOR_LENS_LEVEL_60P),
};

static struct panel_prop_enum_item mdnie_hdr_enum_items[HDR_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HDR_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HDR_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HDR_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(HDR_3),
};

static struct panel_prop_enum_item mdnie_light_notification_enum_items[LIGHT_NOTIFICATION_MAX] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LIGHT_NOTIFICATION_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LIGHT_NOTIFICATION_ON),
};

static struct panel_prop_enum_item mdnie_ccrd_pt_enum_items[MAX_CCRD_PT] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_NONE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_3),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_4),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_5),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_6),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_7),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_8),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(CCRD_PT_9),
};

static struct panel_prop_enum_item mdnie_ldu_mode_enum_items[MAX_LDU_MODE] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_1),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_2),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_3),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_4),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(LDU_MODE_5),
};

static struct panel_prop_enum_item mdnie_scr_white_mode_enum_items[MAX_SCR_WHITE_MODE] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SCR_WHITE_MODE_NONE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SCR_WHITE_MODE_COLOR_COORDINATE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SCR_WHITE_MODE_ADJUST_LDU),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SCR_WHITE_MODE_SENSOR_RGB),
};

static struct panel_prop_enum_item mdnie_trans_mode_enum_items[MAX_TRANS_MODE] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(TRANS_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(TRANS_ON),
};

static struct panel_prop_list mdnie_property_array[] = {
	/* enum property */
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_MODE_PROPERTY,
			MDNIE_OFF_MODE, mdnie_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_SCENARIO_PROPERTY,
			UI_MODE, mdnie_scenario_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_SCENARIO_MODE_PROPERTY,
			AUTO, mdnie_scenario_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_SCREEN_MODE_PROPERTY,
			MDNIE_SCREEN_MODE_VIVID, mdnie_screen_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_BYPASS_PROPERTY,
			BYPASS_OFF, mdnie_bypass_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_ACCESSIBILITY_PROPERTY,
			ACCESSIBILITY_OFF, mdnie_accessibility_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_HMD_PROPERTY,
			HMD_MDNIE_OFF, mdnie_hmd_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_HBM_CE_PROPERTY,
			HBM_CE_MODE_OFF, mdnie_hbm_ce_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_NIGHT_MODE_PROPERTY,
			NIGHT_MODE_OFF, mdnie_night_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_ANTI_GLARE_PROPERTY,
			ANTI_GLARE_OFF, mdnie_anti_glare_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_COLOR_LENS_PROPERTY,
			COLOR_LENS_OFF, mdnie_color_lens_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_COLOR_LENS_COLOR_PROPERTY,
			COLOR_LENS_COLOR_BLUE, mdnie_color_lens_color_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_COLOR_LENS_LEVEL_PROPERTY,
			COLOR_LENS_LEVEL_20P, mdnie_color_lens_level_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_HDR_PROPERTY,
			HDR_OFF, mdnie_hdr_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_LIGHT_NOTIFICATION_PROPERTY,
			LIGHT_NOTIFICATION_OFF, mdnie_light_notification_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_CCRD_PT_PROPERTY,
			CCRD_PT_NONE, mdnie_ccrd_pt_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_LDU_MODE_PROPERTY,
			LDU_MODE_OFF, mdnie_ldu_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_SCR_WHITE_MODE_PROPERTY,
			SCR_WHITE_MODE_NONE, mdnie_scr_white_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(MDNIE_TRANS_MODE_PROPERTY,
			TRANS_ON, mdnie_trans_mode_enum_items),
	/* range property */
	__PANEL_PROPERTY_RANGE_INITIALIZER(MDNIE_ENABLE_PROPERTY,
			0, 0, 1),
	__PANEL_PROPERTY_RANGE_INITIALIZER(MDNIE_NIGHT_LEVEL_PROPERTY,
			NIGHT_LEVEL_6500K, 0, 305),
	__PANEL_PROPERTY_RANGE_INITIALIZER(MDNIE_HBM_CE_LEVEL_PROPERTY,
			0, 0, MAX_HBM_CE_LEVEL),
	__PANEL_PROPERTY_RANGE_INITIALIZER(MDNIE_EXTRA_DIM_LEVEL_PROPERTY,
			0, 0, MAX_EXTRA_DIM_LEVEL),
	__PANEL_PROPERTY_RANGE_INITIALIZER(MDNIE_VIVIDNESS_LEVEL_PROPERTY,
			0, 0, MAX_VIVIDNESS_LEVEL),
};

__visible_for_testing int mdnie_set_property_value(struct mdnie_info *mdnie,
		char *propname, unsigned int value)
{
	return panel_set_property_value(to_panel_device(mdnie),
				propname, value);
}

__visible_for_testing int mdnie_set_property(struct mdnie_info *mdnie,
		u32 *property, unsigned int value)
{
	char *propname = NULL;

	if (!mdnie) {
		panel_err("mdnie is null\n");
		return -EINVAL;
	}

	if (!property) {
		panel_err("property is null\n");
		return -EINVAL;
	}
	if (property == &mdnie->props.enable)
		propname = MDNIE_ENABLE_PROPERTY;
	else if (property == &mdnie->props.mdnie_mode)
		propname = MDNIE_MODE_PROPERTY;
	else if (property == &mdnie->props.scenario)
		propname = MDNIE_SCENARIO_PROPERTY;
	else if (property == &mdnie->props.scenario_mode)
		propname = MDNIE_SCENARIO_MODE_PROPERTY;
	else if (property == &mdnie->props.screen_mode)
		propname = MDNIE_SCREEN_MODE_PROPERTY;
	else if (property == &mdnie->props.bypass)
		propname = MDNIE_BYPASS_PROPERTY;
	else if (property == &mdnie->props.accessibility)
		propname = MDNIE_ACCESSIBILITY_PROPERTY;
	else if (property == &mdnie->props.hbm_ce_level)
		propname = MDNIE_HBM_CE_LEVEL_PROPERTY;
	else if (property == &mdnie->props.hmd)
		propname = MDNIE_HMD_PROPERTY;
	else if (property == &mdnie->props.night)
		propname = MDNIE_NIGHT_MODE_PROPERTY;
	else if (property == &mdnie->props.night_level)
		propname = MDNIE_NIGHT_LEVEL_PROPERTY;
	else if (property == &mdnie->props.anti_glare)
		propname = MDNIE_ANTI_GLARE_PROPERTY;
	else if (property == &mdnie->props.color_lens)
		propname = MDNIE_COLOR_LENS_PROPERTY;
	else if (property == &mdnie->props.color_lens_color)
		propname = MDNIE_COLOR_LENS_COLOR_PROPERTY;
	else if (property == &mdnie->props.color_lens_level)
		propname = MDNIE_COLOR_LENS_LEVEL_PROPERTY;
	else if (property == &mdnie->props.hdr)
		propname = MDNIE_HDR_PROPERTY;
	else if (property == &mdnie->props.light_notification)
		propname = MDNIE_LIGHT_NOTIFICATION_PROPERTY;
	else if (property == &mdnie->props.ldu)
		propname = MDNIE_LDU_MODE_PROPERTY;
	else if (property == &mdnie->props.scr_white_mode)
		propname = MDNIE_SCR_WHITE_MODE_PROPERTY;
	else if (property == &mdnie->props.trans_mode)
		propname = MDNIE_TRANS_MODE_PROPERTY;
	else if (property == &mdnie->props.extra_dim_level)
		propname = MDNIE_EXTRA_DIM_LEVEL_PROPERTY;
	else if (property == &mdnie->props.vividness_level)
		propname = MDNIE_VIVIDNESS_LEVEL_PROPERTY;

	if (!propname) {
		panel_err("unknown property\n");
		return -EINVAL;
	}

	if (mdnie_set_property_value(mdnie,
				propname, value) < 0) {
		panel_warn("failed to set property(%s) %d\n",
				propname, value);
		return -EINVAL;
	}
	*property = value;

	return 0;
}

int mdnie_current_state(struct mdnie_info *mdnie)
{
	struct panel_device *panel =
		container_of(mdnie, struct panel_device, mdnie);
	int mdnie_mode;

	if (IS_BYPASS_MODE(mdnie))
		mdnie_mode = MDNIE_BYPASS_MODE;
	else if (IS_LIGHT_NOTIFICATION_MODE(mdnie))
		mdnie_mode = MDNIE_LIGHT_NOTIFICATION_MODE;
	else if (IS_ACCESSIBILITY_MODE(mdnie))
		mdnie_mode = MDNIE_ACCESSIBILITY_MODE;
	else if (IS_COLOR_LENS_MODE(mdnie))
		mdnie_mode = MDNIE_COLOR_LENS_MODE;
	else if (IS_HMD_MODE(mdnie))
		mdnie_mode = MDNIE_HMD_MODE;
	else if (IS_NIGHT_MODE(mdnie))
		mdnie_mode = MDNIE_NIGHT_MODE;
	else if (IS_HBM_CE_MODE(mdnie))
		mdnie_mode = MDNIE_HBM_CE_MODE;
	else if (IS_HDR_MODE(mdnie))
		mdnie_mode = MDNIE_HDR_MODE;
	else if (IS_SCENARIO_MODE(mdnie))
		mdnie_mode = MDNIE_SCENARIO_MODE;
	else
		mdnie_mode = MDNIE_OFF_MODE;

	if (panel_get_cur_state(panel) == PANEL_STATE_ALPM &&
	((mdnie_mode == MDNIE_ACCESSIBILITY_MODE &&
	(mdnie->props.accessibility == NEGATIVE ||
	mdnie->props.accessibility == GRAYSCALE_NEGATIVE)) ||
	(mdnie_mode == MDNIE_SCENARIO_MODE && !IS_LDU_MODE(mdnie)) ||
	mdnie_mode == MDNIE_COLOR_LENS_MODE ||
	mdnie_mode == MDNIE_HDR_MODE ||
	mdnie_mode == MDNIE_LIGHT_NOTIFICATION_MODE ||
	mdnie_mode == MDNIE_HMD_MODE)) {
		panel_dbg("block mdnie (%s->%s) in doze mode\n",
				mdnie_mode_name[mdnie_mode],
				mdnie_mode_name[MDNIE_BYPASS_MODE]);
		mdnie_mode = MDNIE_BYPASS_MODE;
	}

	return mdnie_mode;
}

char *mdnie_get_accessibility_sequence_name(struct mdnie_info *mdnie)
{
	static char *accessibility_seqname_array[] = {
		[ACCESSIBILITY_OFF] = NULL,
		[NEGATIVE] = MDNIE_NEGATIVE_SEQ,
		[COLOR_BLIND] = MDNIE_COLOR_BLIND_SEQ,
		[SCREEN_CURTAIN] = MDNIE_SCREEN_CURTAIN_SEQ,
		[GRAYSCALE] = MDNIE_GRAYSCALE_SEQ,
		[GRAYSCALE_NEGATIVE] = MDNIE_GRAYSCALE_NEGATIVE_SEQ,
		[COLOR_BLIND_HBM] = MDNIE_COLOR_BLIND_HBM_SEQ,
	};

	if (mdnie->props.accessibility == 0 ||
			(mdnie->props.accessibility >=
			ARRAY_SIZE(accessibility_seqname_array)))
		return NULL;

	return accessibility_seqname_array[mdnie->props.accessibility];
}

char *mdnie_get_sequence_name(struct mdnie_info *mdnie)
{
	char *seqname;
	int mdnie_mode = mdnie_current_state(mdnie);

	switch (mdnie_mode) {
	case MDNIE_BYPASS_MODE:
		seqname = MDNIE_BYPASS_SEQ;
		break;
	case MDNIE_LIGHT_NOTIFICATION_MODE:
		seqname = MDNIE_LIGHT_NOTIFICATION_SEQ;
		break;
	case MDNIE_ACCESSIBILITY_MODE:
		seqname = mdnie_get_accessibility_sequence_name(mdnie);
		break;
	case MDNIE_COLOR_LENS_MODE:
		seqname = MDNIE_COLOR_LENS_SEQ;
		break;
	case MDNIE_HDR_MODE:
		seqname = MDNIE_HDR_SEQ;
		break;
	case MDNIE_HMD_MODE:
		seqname = MDNIE_HMD_SEQ;
		break;
	case MDNIE_NIGHT_MODE:
		seqname = MDNIE_NIGHT_SEQ;
		break;
	case MDNIE_HBM_CE_MODE:
		seqname = MDNIE_HBM_CE_SEQ;
		break;
	case MDNIE_SCENARIO_MODE:
		seqname = MDNIE_SCENARIO_SEQ;
		break;
	default:
		seqname = NULL;
		panel_err("unknown mdnie\n");
		break;
	}

	if (seqname) {
		panel_dbg("mdnie mode:%s(%d), seq:%s found\n",
				mdnie_mode_name[mdnie_mode], mdnie_mode, seqname);
	} else {
		panel_err("mdnie mode:%s(%d), seq not found\n",
				mdnie_mode_name[mdnie_mode], mdnie_mode);
	}

	return seqname;
}
EXPORT_SYMBOL(mdnie_get_sequence_name);

__visible_for_testing int mdnie_get_coordinate(struct mdnie_info *mdnie, int *x, int *y)
{
	struct panel_device *panel =
		container_of(mdnie, struct panel_device, mdnie);
	u8 coordinate[PANEL_COORD_LEN] = { 0, };
	int ret;

	if (!mdnie || !x || !y) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	ret = panel_resource_copy(panel, coordinate, "coordinate");
	if (ret < 0) {
		panel_err("failed to copy 'coordinate' resource\n");
		return -EINVAL;
	}

	*x = (coordinate[0] << 8) | coordinate[1];
	*y = (coordinate[2] << 8) | coordinate[3];
#ifdef MDNIE_SELF_TEST
	*x = g_coord_x;
	*y = g_coord_y;
#endif

	if (*x < MIN_WCRD_X || *x > MAX_WCRD_X ||
			*y < MIN_WCRD_Y || *y > MAX_WCRD_Y)
		panel_warn("need to check coord_x:%d coord_y:%d\n", *x, *y);

	return 0;
}

static bool mdnie_coordinate_changed(struct mdnie_info *mdnie)
{
	int x, y;

	mdnie_get_coordinate(mdnie, &x, &y);

	return (mdnie->props.wcrd_x != x || mdnie->props.wcrd_y != y);
}

static int mdnie_coordinate_area(struct mdnie_info *mdnie, int x, int y)
{
	s64 result[MAX_CAL_LINE];
	int i, area;

	for (i = 0; i < MAX_CAL_LINE; i++)
		result[i] = COLOR_OFFSET_FUNC(x, y,
				mdnie->props.line[i].num,
				mdnie->props.line[i].den,
				mdnie->props.line[i].con);

	area = RECOGNIZE_REGION(result[H_LINE], result[V_LINE]);
	panel_dbg("coord %d %d area Q%d, result %lld %lld\n",
			x, y, area + 1, result[H_LINE], result[V_LINE]);

	return area;
}

static int mdnie_coordinate_tune_x(struct mdnie_info *mdnie, int x, int y)
{
	int res, area;

	area = mdnie_coordinate_area(mdnie, x, y);
	res = ((mdnie->props.coef[area].a * x) + (mdnie->props.coef[area].b * y) +
		(((mdnie->props.coef[area].c * x + (1L << 9)) >> 10) * y) +
		(mdnie->props.coef[area].d * 10000) + (1L << 9)) >> 10;

	return max(min(res, 1024), 0);
}

static int mdnie_coordinate_tune_y(struct mdnie_info *mdnie, int x, int y)
{
	int res, area;

	area = mdnie_coordinate_area(mdnie, x, y);
	res = ((mdnie->props.coef[area].e * x) + (mdnie->props.coef[area].f * y) +
		(((mdnie->props.coef[area].g * x + (1L << 9)) >> 10) * y) +
		(mdnie->props.coef[area].h * 10000) + (1L << 9)) >> 10;

	return max(min(res, 1024), 0);
}

static void mdnie_coordinate_tune_rgb(struct mdnie_info *mdnie, int x, int y, u8 (*tune_rgb)[MAX_COLOR])
{
	static int pt[MAX_QUAD][MAX_RGB_PT] = {
		{ CCRD_PT_5, CCRD_PT_2, CCRD_PT_6, CCRD_PT_3 },	/* QUAD_1 */
		{ CCRD_PT_5, CCRD_PT_2, CCRD_PT_4, CCRD_PT_1 },	/* QUAD_2 */
		{ CCRD_PT_5, CCRD_PT_8, CCRD_PT_4, CCRD_PT_7 },	/* QUAD_3 */
		{ CCRD_PT_5, CCRD_PT_8, CCRD_PT_6, CCRD_PT_9 },	/* QUAD_4 */
	};
	int i, c, type, area, tune_x, tune_y, res[MAX_COLOR][MAX_RGB_PT];
	s64 result[MAX_CAL_LINE];

	area = mdnie_coordinate_area(mdnie, x, y);

	if (((x - mdnie->props.cal_x_center) * (x - mdnie->props.cal_x_center) + (y - mdnie->props.cal_y_center) * (y - mdnie->props.cal_y_center)) <= mdnie->props.cal_boundary_center) {
		tune_x = 0;
		tune_y = 0;
	} else {
		tune_x = mdnie_coordinate_tune_x(mdnie, x, y);
		tune_y = mdnie_coordinate_tune_y(mdnie, x, y);
	}

	for (i = 0; i < MAX_CAL_LINE; i++)
		result[i] = COLOR_OFFSET_FUNC(x, y,
				mdnie->props.line[i].num,
				mdnie->props.line[i].den,
				mdnie->props.line[i].con);

	for (type = 0; type < MAX_WCRD_TYPE; type++) {
		for (c = 0; c < MAX_COLOR; c++) {
			for (i = 0; i < MAX_RGB_PT; i++)
				res[c][i] = mdnie->props.vtx[type][pt[area][i]][c];
			tune_rgb[type][c] =
				((((res[c][RGB_00] * (1024 - tune_x) + (res[c][RGB_10] * tune_x)) * (1024 - tune_y)) +
				  ((res[c][RGB_01] * (1024 - tune_x) + (res[c][RGB_11] * tune_x)) * tune_y)) + (1L << 19)) >> 20;
		}
	}
	panel_info("coord (x:%4d y:%4d Q%d compV:%8lld compH:%8lld)\t"
			"tune_coord (%4d %4d) tune_rgb[ADT] (%3d %3d %3d) tune_rgb[D65] (%3d %3d %3d)\n",
			x, y, area + 1, result[H_LINE], result[V_LINE], tune_x, tune_y,
			tune_rgb[WCRD_TYPE_ADAPTIVE][0], tune_rgb[WCRD_TYPE_ADAPTIVE][1],
			tune_rgb[WCRD_TYPE_ADAPTIVE][2], tune_rgb[WCRD_TYPE_D65][0],
			tune_rgb[WCRD_TYPE_D65][1], tune_rgb[WCRD_TYPE_D65][2]);

}

__visible_for_testing int mdnie_init_coordinate_tune(struct mdnie_info *mdnie)
{
	int x = 0, y = 0;

	if (!mdnie)
		return -EINVAL;

	mdnie_get_coordinate(mdnie, &x, &y);
	mdnie->props.wcrd_x = x;
	mdnie->props.wcrd_y = y;
	mdnie_coordinate_tune_rgb(mdnie, x, y, mdnie->props.coord_wrgb);

	return 0;
}

struct seqinfo *find_mdnie_sequence(struct mdnie_info *mdnie, char *seqname)
{
	if (!mdnie) {
		panel_err("mdnie is null\n");
		return NULL;
	}

	return find_panel_seq_by_name(to_panel_device(mdnie), seqname);
}

bool is_mdnie_sequence_exist(struct mdnie_info *mdnie, char *seqname)
{
	struct seqinfo *seq;

	seq = find_mdnie_sequence(mdnie, seqname);
	if (!seq)
		return false;

	if (!is_valid_sequence(seq))
		return false;

	return true;
}

int mdnie_do_sequence_nolock(struct mdnie_info *mdnie, char *seqname)
{
	struct seqinfo *seq;

	if (!mdnie) {
		panel_err("mdnie is null\n");
		return -EINVAL;
	}

	seq = find_mdnie_sequence(mdnie, seqname);
	if (!seq)
		return -EINVAL;

	return execute_sequence_nolock(to_panel_device(mdnie), seq);
}

int mdnie_do_sequence(struct mdnie_info *mdnie, char *seqname)
{
	struct panel_device *panel =
		to_panel_device(mdnie);
	int ret;

	panel_mutex_lock(&panel->op_lock);
	ret = mdnie_do_sequence_nolock(mdnie, seqname);
	panel_mutex_unlock(&panel->op_lock);

	return ret;
}

static int panel_set_mdnie(struct panel_device *panel)
{
	int ret;
	struct mdnie_info *mdnie = &panel->mdnie;
	int mdnie_mode = mdnie_current_state(mdnie);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return -EAGAIN;

#ifdef CONFIG_USDM_MDNIE_AFC
	panel_info("do mdnie-seq (mode:%s, afc:%s)\n",
			mdnie_mode_name[mdnie_mode],
			!mdnie->props.afc_on ? "off" : "on");
#else
	panel_info("do mdnie-seq (mode:%s)\n",
			mdnie_mode_name[mdnie_mode]);
#endif

	panel_mutex_lock(&panel->op_lock);
	mdnie_set_property(mdnie,
			&mdnie->props.mdnie_mode, mdnie_mode);

	if (is_mdnie_sequence_exist(mdnie, MDNIE_PRE_SEQ)) {
		ret = mdnie_do_sequence_nolock(mdnie, MDNIE_PRE_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to run sequence(%s)\n",
					MDNIE_PRE_SEQ);
			goto err;
		}
	}

	ret = mdnie_do_sequence_nolock(mdnie,
			mdnie_get_sequence_name(mdnie));
	if (unlikely(ret < 0)) {
		panel_err("failed to run sequence(%s)\n",
				mdnie_get_sequence_name(mdnie));
		goto err;
	}

	if (is_mdnie_sequence_exist(mdnie, MDNIE_POST_SEQ)) {
		ret = mdnie_do_sequence_nolock(mdnie, MDNIE_POST_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to run sequence(%s)\n",
					MDNIE_POST_SEQ);
			goto err;
		}
	}

#ifdef CONFIG_USDM_MDNIE_AFC
	ret = mdnie_do_sequence_nolock(mdnie,
			!mdnie->props.afc_on ?
			MDNIE_AFC_OFF_SEQ : MDNIE_AFC_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to run sequence(%s)\n",
				!mdnie->props.afc_on ?
				MDNIE_AFC_OFF_SEQ : MDNIE_AFC_ON_SEQ);
		goto err;
	}
#endif

err:
	panel_mutex_unlock(&panel->op_lock);

	return ret;
}

static void mdnie_update_scr_white_mode(struct mdnie_info *mdnie)
{
	int mdnie_mode = mdnie_current_state(mdnie);

	if (!mdnie->props.wcrd_x || !mdnie->props.wcrd_y) {
		mdnie_set_property(mdnie, &mdnie->props.scr_white_mode, SCR_WHITE_MODE_NONE);
		panel_warn("need to check coord_x:%d coord_y:%d, scr_white_mode %s\n",
				mdnie->props.wcrd_x, mdnie->props.wcrd_y,
				scr_white_mode_name[mdnie->props.scr_white_mode]);
		return;
	}

	if (mdnie_mode == MDNIE_SCENARIO_MODE) {
		if ((IS_LDU_MODE(mdnie)) && (mdnie->props.scenario != EBOOK_MODE)) {
			mdnie_set_property(mdnie,
					&mdnie->props.scr_white_mode,
					SCR_WHITE_MODE_ADJUST_LDU);
		} else if (mdnie->props.update_sensorRGB &&
				mdnie->props.scenario_mode == AUTO &&
				(mdnie->props.scenario == BROWSER_MODE ||
				 mdnie->props.scenario == EBOOK_MODE)) {
			mdnie_set_property(mdnie,
					&mdnie->props.scr_white_mode,
					SCR_WHITE_MODE_SENSOR_RGB);
			mdnie->props.update_sensorRGB = false;
		} else if (mdnie->props.scenario <= SCENARIO_MAX &&
				mdnie->props.scenario != EBOOK_MODE) {
			mdnie_set_property(mdnie,
					&mdnie->props.scr_white_mode,
					SCR_WHITE_MODE_COLOR_COORDINATE);
		} else {
			mdnie_set_property(mdnie,
					&mdnie->props.scr_white_mode,
					SCR_WHITE_MODE_NONE);
		}
	} else if (mdnie_mode == MDNIE_HBM_CE_MODE &&
			!mdnie->props.force_scr_white_mode_none_on_hbm) {
		mdnie_set_property(mdnie,
				&mdnie->props.scr_white_mode,
				SCR_WHITE_MODE_COLOR_COORDINATE);
	} else {
		mdnie_set_property(mdnie,
				&mdnie->props.scr_white_mode,
				SCR_WHITE_MODE_NONE);
	}

	panel_dbg("scr_white_mode %s\n",
			scr_white_mode_name[mdnie->props.scr_white_mode]);
}

int mdnie_set_def_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b)
{
	if (!mdnie)
		return -EINVAL;

	mdnie->props.def_wrgb[RED] = r;
	mdnie->props.def_wrgb[GREEN] = g;
	mdnie->props.def_wrgb[BLUE] = b;

	panel_dbg("def_wrgb: %d(%02X) %d(%02X) %d(%02X)\n",
			mdnie->props.def_wrgb[RED], mdnie->props.def_wrgb[RED],
			mdnie->props.def_wrgb[GREEN], mdnie->props.def_wrgb[GREEN],
			mdnie->props.def_wrgb[BLUE], mdnie->props.def_wrgb[BLUE]);

	return 0;
}
EXPORT_SYMBOL(mdnie_set_def_wrgb);

int mdnie_set_cur_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b)
{
	if (!mdnie)
		return -EINVAL;

	mdnie->props.cur_wrgb[RED] = r;
	mdnie->props.cur_wrgb[GREEN] = g;
	mdnie->props.cur_wrgb[BLUE] = b;

	panel_dbg("cur_wrgb: %d(%02X) %d(%02X) %d(%02X)\n",
			mdnie->props.cur_wrgb[RED], mdnie->props.cur_wrgb[RED],
			mdnie->props.cur_wrgb[GREEN], mdnie->props.cur_wrgb[GREEN],
			mdnie->props.cur_wrgb[BLUE], mdnie->props.cur_wrgb[BLUE]);

	return 0;
}
EXPORT_SYMBOL(mdnie_set_cur_wrgb);

int mdnie_cur_wrgb_to_byte_array(struct mdnie_info *mdnie,
		unsigned char *dst, unsigned int stride)
{
	if (!mdnie || !dst || !stride)
		return -EINVAL;

	copy_to_sliced_byte_array(dst,
			mdnie->props.cur_wrgb, 0, MAX_COLOR * stride, stride);

	return 0;
}
EXPORT_SYMBOL(mdnie_cur_wrgb_to_byte_array);

#define MDNIE_DEFAULT_ANTI_GLARE_RATIO (100)

int mdnie_get_anti_glare_ratio(struct mdnie_info *mdnie)
{
	if (!mdnie->props.anti_glare)
		return MDNIE_DEFAULT_ANTI_GLARE_RATIO;

	if (mdnie->props.anti_glare_level >=
			ARRAY_SIZE(mdnie->props.anti_glare_ratio))
		return MDNIE_DEFAULT_ANTI_GLARE_RATIO;

	return mdnie->props.anti_glare_ratio[mdnie->props.anti_glare_level];
}
EXPORT_SYMBOL(mdnie_get_anti_glare_ratio);

int mdnie_update_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char src[MAX_COLOR] = { r, g, b };
	unsigned char dst[MAX_COLOR] = { 0, };
	int i, value;

	if (!mdnie)
		return -EINVAL;

	if (mdnie->props.scr_white_mode < 0 ||
			mdnie->props.scr_white_mode >= MAX_SCR_WHITE_MODE) {
		panel_warn("out of range %d\n",
				mdnie->props.scr_white_mode);
		return -EINVAL;
	}

	if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_COLOR_COORDINATE) {
		mdnie_set_def_wrgb(mdnie, r, g, b);
		for_each_color(i) {
			value = (int)mdnie->props.def_wrgb[i] +
				(int)(((mdnie->props.scenario_mode == AUTO) || (mdnie->props.scenario_mode == DYNAMIC)) ?
						mdnie->props.def_wrgb_ofs[i] : 0);
			dst[i] = min(max(value, 0), 255);
		}
		mdnie_set_cur_wrgb(mdnie, dst[RED], dst[GREEN], dst[BLUE]);
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_ADJUST_LDU) {
		for_each_color(i) {
			value = (int)src[i] + (int)(((mdnie->props.scenario_mode == AUTO) &&
							(mdnie->props.scenario != EBOOK_MODE)) ?
						mdnie->props.def_wrgb_ofs[i] : 0);
			dst[i] = min(max(value, 0), 255);
		}
		mdnie_set_cur_wrgb(mdnie, dst[RED], dst[GREEN], dst[BLUE]);
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_SENSOR_RGB) {
		mdnie_set_cur_wrgb(mdnie, r, g, b);
	} else {
		panel_warn("wrgb is not updated in scr_white_mode(%d)\n",
				mdnie->props.scr_white_mode);
	}

	return 0;
}
EXPORT_SYMBOL(mdnie_update_wrgb);

int panel_mdnie_update(struct panel_device *panel)
{
	int ret;
	struct mdnie_info *mdnie = &panel->mdnie;

	panel_mutex_lock(&mdnie->lock);
	if (!IS_MDNIE_ENABLED(mdnie)) {
		panel_info("mdnie is off state\n");
		panel_mutex_unlock(&mdnie->lock);
		return -EINVAL;
	}

	if (mdnie_coordinate_changed(mdnie)) {
		mdnie_init_coordinate_tune(mdnie);
	}
	mdnie_update_scr_white_mode(mdnie);

	ret = panel_set_mdnie(panel);
	if (ret < 0 && ret != -EAGAIN) {
		panel_err("failed to set mdnie %d\n", ret);
		panel_mutex_unlock(&mdnie->lock);
		return ret;
	}
	panel_mutex_unlock(&mdnie->lock);

#ifdef CONFIG_USDM_PANEL_COPR
	copr_update_start(&panel->copr, 3);
#endif

	return 0;
}

static int mdnie_update(struct mdnie_info *mdnie)
{
	int ret;
	struct panel_device *panel =
		container_of(mdnie, struct panel_device, mdnie);

	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	ret = panel_mdnie_update(panel);
	panel_wake_unlock(panel);

	return ret;
}

#ifdef MDNIE_SELF_TEST
static void mdnie_coordinate_tune_test(struct mdnie_info *mdnie)
{
	int i, x, y;
	u8 tune_rgb[MAX_WCRD_TYPE][MAX_COLOR];
	int input[27][2] = {
		{ 2936, 3144 }, { 2987, 3203 }, { 3032, 3265 }, { 2954, 3108 }, { 2991, 3158 }, { 3041, 3210 }, { 2967, 3058 }, { 3004, 3091 },
		{ 3063, 3156 }, { 2985, 3206 }, { 3032, 3238 }, { 2955, 3094 }, { 2997, 3151 }, { 3045, 3191 }, { 2971, 3047 }, { 3020, 3100 },
		{ 3066, 3144 }, { 2983, 3131 }, { 2987, 3170 }, { 2975, 3159 }, { 3021, 3112 }, { 2982, 3122 }, { 2987, 3130 }, { 2930, 3260 },
		{ 2930, 3050 }, { 3060, 3050 }, { 3060, 3260 },
	};
	u8 output[27][MAX_COLOR] = {
		{ 0xFF, 0xFA, 0xF9 }, { 0xFE, 0xFA, 0xFE }, { 0xF8, 0xF6, 0xFF }, { 0xFF, 0xFD, 0xFB },
		{ 0xFF, 0xFE, 0xFF }, { 0xF9, 0xFA, 0xFF }, { 0xFC, 0xFF, 0xF9 }, { 0xFB, 0xFF, 0xFB },
		{ 0xF9, 0xFF, 0xFF }, { 0xFE, 0xFA, 0xFE }, { 0xF9, 0xF8, 0xFF }, { 0xFE, 0xFE, 0xFA },
		{ 0xFE, 0xFF, 0xFF }, { 0xF8, 0xFC, 0xFF }, { 0xFA, 0xFF, 0xF8 }, { 0xF9, 0xFF, 0xFC },
		{ 0xF8, 0xFF, 0xFF }, { 0xFE, 0xFF, 0xFD }, { 0xFF, 0xFD, 0xFF }, { 0xFF, 0xFD, 0xFD },
		{ 0xF9, 0xFF, 0xFC }, { 0xFE, 0xFF, 0xFD }, { 0xFE, 0xFF, 0xFD }, { 0xFF, 0xFF, 0xFF },
		{ 0xFF, 0xFF, 0xFF }, { 0xFF, 0xFF, 0xFF }, { 0xFF, 0xFF, 0xFF },
	};

	for (x = MIN_WCRD_X; x <= MAX_WCRD_X; x += 10)
		for (y = MIN_WCRD_Y; y <= MAX_WCRD_Y; y += 10)
			mdnie_coordinate_tune_rgb(mdnie, x, y, tune_rgb);

	for (i = 0; i < 27; i++) {
		g_coord_x = input[i][0];
		g_coord_y = input[i][1];
		mdnie_update(mdnie);
		panel_info("compare %02X %02X %02X : %02X %02X %02X (%s)\n",
				output[i][0], output[i][1], output[i][2],
				mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][0],
				mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][1],
				mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][2],
				output[i][0] == mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][0] &&
				output[i][1] == mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][1] &&
				output[i][2] == mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][2] ?  "SUCCESS" : "FAILED");
	}
}
#endif

static ssize_t mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.scenario_mode);
}

static ssize_t mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value >= MODE_MAX) {
		panel_err("invalid value=%d\n", value);
		return -EINVAL;
	}

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.scenario_mode, value);
	mdnie_set_property(mdnie, &mdnie->props.screen_mode,
			(value == AUTO) ? MDNIE_SCREEN_MODE_VIVID :
			MDNIE_SCREEN_MODE_NATURAL);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t scenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.scenario);
}

static ssize_t scenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (!SCENARIO_IS_VALID(value)) {
		panel_err("invalid scenario %d\n", value);
		return -EINVAL;
	}

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.scenario, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t accessibility_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.accessibility);
}

static ssize_t accessibility_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int s[12] = {0, };
	int i, value = 0, ret;

	ret = sscanf(buf, "%d %x %x %x %x %x %x %x %x %x %x %x %x",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8], &s[9], &s[10], &s[11]);

	if (ret <= 0 || ret > ARRAY_SIZE(s) + 1 ||
			((ret - 1) > (MAX_MDNIE_SCR_LEN / 2))) {
		panel_err("invalid size %d\n", ret);
		return -EINVAL;
	}

	if (value < 0 || value >= ACCESSIBILITY_MAX) {
		panel_err("unknown accessibility %d\n", value);
		return -EINVAL;
	}

	panel_info("value: %d, cnt: %d\n", value, ret);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.accessibility, value);
	if (ret > 1 && (value == COLOR_BLIND || value == COLOR_BLIND_HBM)) {
		for (i = 0; i < ret - 1; i++) {
			mdnie->props.scr[i * 2 + 0] = GET_LSB_8BIT(s[i]);
			mdnie->props.scr[i * 2 + 1] = GET_MSB_8BIT(s[i]);
		}
		mdnie->props.sz_scr = (ret - 1) * 2;
	}
	panel_mutex_unlock(&mdnie->lock);

	panel_info("%s\n", buf);
	mdnie_update(mdnie);

	return count;
}

static ssize_t bypass_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.bypass);
}

static ssize_t bypass_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value >= BYPASS_MAX) {
		panel_err("invalid value=%d\n", value);
		return -EINVAL;
	}

	panel_info("value=%d\n", value);

	value = (value) ? BYPASS_ON : BYPASS_OFF;

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.bypass, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t lux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.hbm_ce_level);
}

static ssize_t lux_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int i, ret, value;
	unsigned int hbm_ce_level;
	unsigned int anti_glare_level;
	bool update = false;

	ret = kstrtoint(buf, 0, &value);
	if (ret < 0)
		return ret;

	panel_mutex_lock(&mdnie->lock);
	for (i = 0; i < MAX_HBM_CE_LEVEL; i++) {
		if (!mdnie->props.hbm_ce_lux[i])
			break;

		if (value < mdnie->props.hbm_ce_lux[i])
			break;
	}
	hbm_ce_level = i;

	if (mdnie->props.hbm_ce_level != hbm_ce_level)
		update = true;

	mdnie_set_property_value(mdnie, MDNIE_HBM_CE_PROPERTY,
			(hbm_ce_level > 0) ? HBM_CE_MODE_ON : HBM_CE_MODE_OFF);
	mdnie_set_property(mdnie, &mdnie->props.hbm_ce_level, hbm_ce_level);

	if (value < 0) {
		anti_glare_level = 0;
	} else {
		for (i = 0; i < MAX_ANTI_GLARE_LEVEL; i++) {
			if (!mdnie->props.anti_glare_lux[i])
				break;

			if (value >= mdnie->props.anti_glare_lux[i])
				break;
		}
		anti_glare_level = i;
	}

	if (mdnie->props.anti_glare_level != anti_glare_level) {
		mdnie->props.anti_glare_level = anti_glare_level;
		update = true;
	}

	panel_mutex_unlock(&mdnie->lock);

	if (update) {
		panel_info("hbm_ce:%d anti_glare:%d (lux:%d)\n",
				mdnie->props.hbm_ce_level,
				mdnie->props.anti_glare_level,
				value);
		mdnie_update(mdnie);
	}

	return count;
}

/* Temporary solution: Do not use this sysfs as official purpose */
static ssize_t mdnie_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	char *seqname = mdnie_get_sequence_name(mdnie);
	int mdnie_mode = mdnie_current_state(mdnie);
	unsigned int i, len = 0;

	if (!IS_MDNIE_ENABLED(mdnie)) {
		panel_err("mdnie state is off\n");
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
			"mdnie %s-mode, seq:%s\n",
			mdnie_mode_name[mdnie_mode], seqname);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"accessibility %s(%d), hdr %d, hmd %d, hbm_ce %d\n",
			accessibility_name[mdnie->props.accessibility],
			mdnie->props.accessibility, mdnie->props.hdr,
			mdnie->props.hmd, mdnie->props.hbm_ce_level);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"scenario %s(%d), mode %s(%d)\n",
			scenario_name[mdnie->props.scenario], mdnie->props.scenario,
			scenario_mode_name[mdnie->props.scenario_mode], mdnie->props.scenario_mode);
	len += snprintf(buf + len, PAGE_SIZE - len, "scr_white_mode %s\n",
			scr_white_mode_name[mdnie->props.scr_white_mode]);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"mdnie_ldu %d, coord x %d, y %d area Q%d\n",
			mdnie->props.ldu, mdnie->props.wcrd_x, mdnie->props.wcrd_y,
			mdnie_coordinate_area(mdnie, mdnie->props.wcrd_x, mdnie->props.wcrd_y) + 1);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"coord_wrgb[adpt] r:%d(%02X) g:%d(%02X) b:%d(%02X)\n",
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][0],
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][0],
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][1],
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][1],
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][2],
			mdnie->props.coord_wrgb[WCRD_TYPE_ADAPTIVE][2]);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"coord_wrgb[d65] r:%d(%02X) g:%d(%02X) b:%d(%02X)\n",
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][0],
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][0],
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][1],
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][1],
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][2],
			mdnie->props.coord_wrgb[WCRD_TYPE_D65][2]);
	len += snprintf(buf + len, PAGE_SIZE - len, "cur_wrgb r:%d(%02X) g:%d(%02X) b:%d(%02X)\n",
			mdnie->props.cur_wrgb[0], mdnie->props.cur_wrgb[0],
			mdnie->props.cur_wrgb[1], mdnie->props.cur_wrgb[1],
			mdnie->props.cur_wrgb[2], mdnie->props.cur_wrgb[2]);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"ssr_wrgb r:%d(%02X) g:%d(%02X) b:%d(%02X)\n",
			mdnie->props.ssr_wrgb[0], mdnie->props.ssr_wrgb[0],
			mdnie->props.ssr_wrgb[1], mdnie->props.ssr_wrgb[1],
			mdnie->props.ssr_wrgb[2], mdnie->props.ssr_wrgb[2]);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"def_wrgb r:%d(%02X) g:%d(%02X) b:%d(%02X) offset r:%d g:%d b:%d\n",
			mdnie->props.def_wrgb[0], mdnie->props.def_wrgb[0],
			mdnie->props.def_wrgb[1], mdnie->props.def_wrgb[1],
			mdnie->props.def_wrgb[2], mdnie->props.def_wrgb[2],
			mdnie->props.def_wrgb_ofs[0], mdnie->props.def_wrgb_ofs[1],
			mdnie->props.def_wrgb_ofs[2]);

	len += snprintf(buf + len, PAGE_SIZE - len, "scr : ");
	if (mdnie->props.sz_scr) {
		for (i = 0; i < mdnie->props.sz_scr; i++)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%02x ", mdnie->props.scr[i]);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "none");
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");


	len += snprintf(buf + len, PAGE_SIZE - len,
			"night_mode %s, level %d\n",
			mdnie->props.night ? "on" : "off",
			mdnie->props.night_level);

#ifdef MDNIE_SELF_TEST
	mdnie_coordinate_tune_test(mdnie);
#endif

	return len;
}

static ssize_t sensorRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", mdnie->props.cur_wrgb[0],
			mdnie->props.cur_wrgb[1], mdnie->props.cur_wrgb[2]);
}

static ssize_t sensorRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int white_red, white_green, white_blue;
	int mdnie_mode = mdnie_current_state(mdnie), ret;

	ret = sscanf(buf, "%u %u %u",
		&white_red, &white_green, &white_blue);
	if (ret != 3)
		return -EINVAL;

	 panel_info("white_r %u, white_g %u, white_b %u\n",
			 white_red, white_green, white_blue);

	if (mdnie_mode == MDNIE_SCENARIO_MODE &&
			mdnie->props.scenario_mode == AUTO &&
		(mdnie->props.scenario == BROWSER_MODE ||
		 mdnie->props.scenario == EBOOK_MODE)) {
		panel_mutex_lock(&mdnie->lock);
		mdnie->props.ssr_wrgb[0] = white_red;
		mdnie->props.ssr_wrgb[1] = white_green;
		mdnie->props.ssr_wrgb[2] = white_blue;
		mdnie->props.update_sensorRGB = true;
		panel_mutex_unlock(&mdnie->lock);
		mdnie_update(mdnie);
	}

	return count;
}

static ssize_t whiteRGB_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", mdnie->props.def_wrgb_ofs[0],
			mdnie->props.def_wrgb_ofs[1], mdnie->props.def_wrgb_ofs[2]);
}

static ssize_t whiteRGB_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int wr_offset, wg_offset, wb_offset;
	int ret;

	ret = sscanf(buf, "%d %d %d",
		&wr_offset, &wg_offset, &wb_offset);
	if (ret != 3)
		return -EINVAL;

	if (!IS_VALID_WRGB_OFS(wr_offset) ||
			!IS_VALID_WRGB_OFS(wg_offset) ||
			!IS_VALID_WRGB_OFS(wb_offset)) {
		panel_err("invalid offset %d %d %d\n",
				wr_offset, wg_offset, wb_offset);
		return -EINVAL;
	}

	panel_info("wr_offset %d, wg_offset %d, wb_offset %d\n",
		 wr_offset, wg_offset, wb_offset);

	panel_mutex_lock(&mdnie->lock);
	mdnie->props.def_wrgb_ofs[0] = wr_offset;
	mdnie->props.def_wrgb_ofs[1] = wg_offset;
	mdnie->props.def_wrgb_ofs[2] = wb_offset;
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t night_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d %d\n",
			mdnie->props.night, mdnie->props.night_level);
}

static ssize_t night_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int enable, level, ret;

	ret = sscanf(buf, "%d %d", &enable, &level);
	if (ret != 2)
		return -EINVAL;

	if (level < 0 || level >= mdnie->props.num_night_level)
		return -EINVAL;

	panel_info("night_mode %s level %d\n",
			enable ? "on" : "off", level);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.night, !!enable);
	mdnie_set_property(mdnie, &mdnie->props.night_level, level);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t vividness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.vividness_level);
}

static ssize_t vividness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int level, ret;

	ret = sscanf(buf, "%d", &level);
	if (ret != 1)
		return -EINVAL;

	if (level < 0 || level >= MAX_VIVIDNESS_LEVEL)
		return -EINVAL;

	panel_info("vividness_level %d\n", level);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.vividness_level, level);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t anti_glare_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.anti_glare);
}

static ssize_t anti_glare_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int enable, ret;

	ret = sscanf(buf, "%d", &enable);
	if (ret != 1)
		return -EINVAL;

	if (enable < ANTI_GLARE_OFF || enable >= ANTI_GLARE_MAX)
		return -EINVAL;

	panel_info("anti_glare %s\n", enable ? "on" : "off");

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.anti_glare, enable);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t extra_dim_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.extra_dim_level);
}

static ssize_t extra_dim_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int level, ret;

	ret = sscanf(buf, "%d", &level);
	if (ret != 1)
		return -EINVAL;

	if (level < 0 || level > MAX_EXTRA_DIM_LEVEL)
		return -EINVAL;

	panel_info("extra_dim level %d\n", level);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.extra_dim_level, level);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t color_lens_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n",
			mdnie->props.color_lens,
			mdnie->props.color_lens_color,
			mdnie->props.color_lens_level);
}

static ssize_t color_lens_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int enable, level, color, ret;

	ret = sscanf(buf, "%d %d %d", &enable, &color, &level);
	if (ret != 3)
		return -EINVAL;

	if (color < 0 || color >= COLOR_LENS_COLOR_MAX)
		return -EINVAL;

	if (level < 0 || level >= COLOR_LENS_LEVEL_MAX)
		return -EINVAL;

	panel_info("color_lens_mode %s color %d level %d\n",
			enable ? "on" : "off", color, level);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.color_lens, !!enable);
	mdnie_set_property(mdnie, &mdnie->props.color_lens_color, color);
	mdnie_set_property(mdnie, &mdnie->props.color_lens_level, level);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t hdr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.hdr);
}

static ssize_t hdr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value >= HDR_MAX) {
		panel_err("invalid value=%d\n", value);
		return -EINVAL;
	}

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.hdr, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t light_notification_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", mdnie->props.light_notification);
}

static ssize_t light_notification_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value >= LIGHT_NOTIFICATION_MAX)
		return -EINVAL;

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.light_notification, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

static ssize_t mdnie_ldu_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", mdnie->props.cur_wrgb[0],
			mdnie->props.cur_wrgb[1], mdnie->props.cur_wrgb[2]);
}

static ssize_t mdnie_ldu_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int value, ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	if (value < 0 || value >= MAX_LDU_MODE) {
		panel_err("out of range %d\n",
				value);
		return -EINVAL;
	}

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.ldu, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}

#ifdef CONFIG_USDM_PANEL_HMD
static ssize_t hmt_color_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "hmd_mode: %d\n", mdnie->props.hmd);
}

static ssize_t hmt_color_temperature_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	if (value >= HMD_MDNIE_MAX)
		return -EINVAL;

	if (value == mdnie->props.hmd)
		return count;

	panel_info("value=%d\n", value);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.hmd, value);
	panel_mutex_unlock(&mdnie->lock);
	mdnie_update(mdnie);

	return count;
}
#endif

#ifdef CONFIG_USDM_MDNIE_AFC
static ssize_t afc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int len = 0;
	size_t i;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d", mdnie->props.afc_on);
	for (i = 0; i < ARRAY_SIZE(mdnie->props.afc_roi); i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
				" %d", mdnie->props.afc_roi[i]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t afc_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	int s[12] = {0, };
	int value = 0, ret;
	size_t i;

	ret = sscanf(buf, "%i %i %i %i %i %i %i %i %i %i %i %i %i",
		&value, &s[0], &s[1], &s[2], &s[3],
		&s[4], &s[5], &s[6], &s[7], &s[8], &s[9], &s[10], &s[11]);

	if ((ret != 1 && ret != ARRAY_SIZE(s) + 1) ||
			ARRAY_SIZE(s) != MAX_AFC_ROI_LEN) {
		panel_err("invalid size %d\n", ret);
		return -EINVAL;
	}

	panel_info("value=%d, cnt=%d\n", value, ret);

	panel_mutex_lock(&mdnie->lock);
	mdnie->props.afc_on = !!value;
	for (i = 0; i < ARRAY_SIZE(mdnie->props.afc_roi); i++)
		mdnie->props.afc_roi[i] = s[i] & 0xFF;
	panel_mutex_unlock(&mdnie->lock);

	panel_info("%s\n", buf);
	mdnie_update(mdnie);

	return count;
}
#endif

ssize_t mdnie_store_check_test_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_device_attr *pattr = container_of(attr, struct panel_device_attr, dev_attr);
#ifdef CONFIG_USDM_PANEL_TESTMODE
	struct mdnie_info *mdnie = dev_get_drvdata(dev);
	struct panel_device *panel = to_panel_device(mdnie);
	ssize_t ret;

	if (panel_testmode_is_on(panel)) {
		if (buf[0] != '!') {
			panel_info("%s_store: testmode is running. ignore inputs.\n", attr->attr.name);
			return size;
		}
		ret = pattr->store(dev, attr, buf + 1, size - 1);
		if (ret >= 0)
			ret += 1;
		return ret;
	}
#endif
	return pattr->store(dev, attr, buf, size);
}

struct panel_device_attr mdnie_dev_attrs[] = {
	__MDNIE_ATTR_RW(mode, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(scenario, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(accessibility, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(bypass, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(lux, 0000, PA_DEFAULT),
	__MDNIE_ATTR_RO(mdnie, 0444, PA_DEFAULT),
	__MDNIE_ATTR_RW(sensorRGB, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(whiteRGB, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(night_mode, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(vividness, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(anti_glare, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(extra_dim, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(color_lens, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(hdr, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(light_notification, 0664, PA_DEFAULT),
	__MDNIE_ATTR_RW(mdnie_ldu, 0664, PA_DEFAULT),
#ifdef CONFIG_USDM_PANEL_HMD
	__MDNIE_ATTR_RW(hmt_color_temperature, 0664, PA_DEFAULT),
#endif
#ifdef CONFIG_USDM_MDNIE_AFC
	__MDNIE_ATTR_RW(afc, 0664, PA_DEFAULT),
#endif
};

int mdnie_enable(struct mdnie_info *mdnie)
{
	struct panel_device *panel =
		container_of(mdnie, struct panel_device, mdnie);
	int ret;

	if (IS_MDNIE_ENABLED(mdnie)) {
		panel_info("mdnie already enabled\n");
		panel_mdnie_update(panel);
		return 0;
	}

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.enable, 1);
	mdnie_set_property(mdnie, &mdnie->props.light_notification, LIGHT_NOTIFICATION_OFF);
	if (IS_HBM_CE_MODE(mdnie))
		mdnie_set_property(mdnie, &mdnie->props.trans_mode, TRANS_ON);
	panel_mutex_unlock(&mdnie->lock);
	ret = panel_mdnie_update(panel);
	if (ret < 0)
		mdnie_set_property(mdnie, &mdnie->props.enable, 0);

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.trans_mode, TRANS_ON);
	panel_mutex_unlock(&mdnie->lock);

	panel_info("done %u\n", mdnie->props.enable);

	return 0;
}

int mdnie_disable(struct mdnie_info *mdnie)
{
	if (!IS_MDNIE_ENABLED(mdnie)) {
		panel_info("mdnie already disabled\n");
		return 0;
	}

	panel_mutex_lock(&mdnie->lock);
	mdnie_set_property(mdnie, &mdnie->props.enable, 0);
	mdnie_set_property(mdnie, &mdnie->props.trans_mode, TRANS_OFF);
	mdnie->props.update_sensorRGB = false;
	panel_mutex_unlock(&mdnie->lock);

	panel_info("done\n");

	return 0;
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	mdnie = container_of(self, struct mdnie_info, fb_notif);

	fb_blank = *(int *)evdata->data;

	panel_dbg("%d\n", fb_blank);

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK) {
		panel_mutex_lock(&mdnie->lock);
		mdnie_set_property(mdnie, &mdnie->props.light_notification, LIGHT_NOTIFICATION_OFF);
		if (IS_HBM_CE_MODE(mdnie))
			mdnie_set_property(mdnie, &mdnie->props.trans_mode, TRANS_ON);
		panel_mutex_unlock(&mdnie->lock);
		mdnie_update(mdnie);
	}

	return 0;
}

static int mdnie_register_fb(struct mdnie_info *mdnie)
{
	memset(&mdnie->fb_notif, 0, sizeof(mdnie->fb_notif));
	mdnie->fb_notif.notifier_call = fb_notifier_callback;
	fb_register_client(&mdnie->fb_notif);

	return 0;
}

static int mdnie_unregister_fb(struct mdnie_info *mdnie)
{
	fb_unregister_client(&mdnie->fb_notif);
	mdnie->fb_notif.notifier_call = NULL;

	return 0;
}

#ifdef CONFIG_USDM_PANEL_DPUI
static int dpui_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct mdnie_info *mdnie;
	struct panel_device *panel;
	struct panel_info *panel_data;
	char tbuf[MAX_DPUI_VAL_LEN];
	u8 coordinate[PANEL_COORD_LEN] = { 0, };
	int size;

	mdnie = container_of(self, struct mdnie_info, dpui_notif);
	panel = container_of(mdnie, struct panel_device, mdnie);
	panel_data = &panel->panel_data;

	panel_mutex_lock(&mdnie->lock);

	panel_resource_copy(panel, coordinate, "coordinate");
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d",
			(coordinate[0] << 8) | coordinate[1]);
	set_dpui_field(DPUI_KEY_WCRD_X, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d",
			(coordinate[2] << 8) | coordinate[3]);
	set_dpui_field(DPUI_KEY_WCRD_Y, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d",
			mdnie->props.def_wrgb_ofs[0]);
	set_dpui_field(DPUI_KEY_WOFS_R, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d",
			mdnie->props.def_wrgb_ofs[1]);
	set_dpui_field(DPUI_KEY_WOFS_G, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d",
			mdnie->props.def_wrgb_ofs[2]);
	set_dpui_field(DPUI_KEY_WOFS_B, tbuf, size);

#if 0 /* disable for GKI build */
	mdnie_get_efs(MDNIE_WOFS_ORG_PATH, def_wrgb_ofs_org);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", def_wrgb_ofs_org[0]);
	set_dpui_field(DPUI_KEY_WOFS_R_ORG, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", def_wrgb_ofs_org[1]);
	set_dpui_field(DPUI_KEY_WOFS_G_ORG, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", def_wrgb_ofs_org[2]);
	set_dpui_field(DPUI_KEY_WOFS_B_ORG, tbuf, size);
#endif

	panel_mutex_unlock(&mdnie->lock);

	return 0;
}

static int mdnie_register_dpui(struct mdnie_info *mdnie)
{
	memset(&mdnie->dpui_notif, 0, sizeof(mdnie->dpui_notif));
	mdnie->dpui_notif.notifier_call = dpui_notifier_callback;
	return dpui_logging_register(&mdnie->dpui_notif, DPUI_TYPE_PANEL);
}

static int mdnie_unregister_dpui(struct mdnie_info *mdnie)
{
	dpui_logging_unregister(&mdnie->dpui_notif);
	mdnie->dpui_notif.notifier_call = NULL;

	return 0;
}
#endif /* CONFIG_USDM_PANEL_DPUI */

__visible_for_testing int mdnie_init_property(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune)
{
	int ret;

	if (!mdnie || !mdnie_tune)
		return -EINVAL;

	mdnie->props.tuning = 0;
	mdnie->props.update_sensorRGB = false;
	mdnie->props.sz_scr = 0;

	/* initialization by mdnie_tune */
	mdnie->props.num_ldu_mode = mdnie_tune->num_ldu_mode;
	mdnie->props.num_night_level = mdnie_tune->num_night_level;
	mdnie->props.num_color_lens_color = mdnie_tune->num_color_lens_color;
	mdnie->props.num_color_lens_level = mdnie_tune->num_color_lens_level;
	memcpy(mdnie->props.line, mdnie_tune->line, sizeof(mdnie->props.line));
	memcpy(mdnie->props.coef, mdnie_tune->coef, sizeof(mdnie->props.coef));
	memcpy(mdnie->props.vtx, mdnie_tune->vtx, sizeof(mdnie->props.vtx));
	memcpy(mdnie->props.adjust_ldu_wrgb, mdnie_tune->adjust_ldu_wrgb, sizeof(mdnie->props.adjust_ldu_wrgb));
	mdnie->props.cal_x_center = mdnie_tune->cal_x_center;
	mdnie->props.cal_y_center = mdnie_tune->cal_y_center;
	mdnie->props.cal_boundary_center = mdnie_tune->cal_boundary_center;
	mdnie->props.hbm_ce_lux = kmemdup(mdnie_tune->hbm_ce_lux,
			sizeof(mdnie_tune->hbm_ce_lux), GFP_KERNEL);
	memcpy(mdnie->props.anti_glare_lux, mdnie_tune->anti_glare_lux,
			sizeof(mdnie_tune->anti_glare_lux));
	memcpy(mdnie->props.anti_glare_ratio, mdnie_tune->anti_glare_ratio,
			sizeof(mdnie_tune->anti_glare_ratio));
	mdnie->props.scr_white_len = mdnie_tune->scr_white_len;
	mdnie->props.scr_cr_ofs = mdnie_tune->scr_cr_ofs;
	mdnie->props.night_mode_ofs = mdnie_tune->night_mode_ofs;
	mdnie->props.color_lens_ofs = mdnie_tune->color_lens_ofs;
	mdnie->props.force_scr_white_mode_none_on_hbm =
		mdnie_tune->force_scr_white_mode_none_on_hbm;

	ret = panel_add_property_from_array(to_panel_device(mdnie),
			mdnie_property_array,
			ARRAY_SIZE(mdnie_property_array));
	if (ret < 0) {
		panel_err("failed to add mdnie property array\n");
		return ret;
	}

	return 0;
}

__visible_for_testing int mdnie_deinit_property(struct mdnie_info *mdnie)
{
	int ret;

	if (!mdnie)
		return -EINVAL;

	ret = panel_delete_property_from_array(to_panel_device(mdnie),
			mdnie_property_array,
			ARRAY_SIZE(mdnie_property_array));
	if (ret < 0) {
		panel_err("failed to delete mdnie property array\n");
		return ret;
	}

	kfree(mdnie->props.hbm_ce_lux);
	mdnie->props.hbm_ce_lux = NULL;

	return 0;
}

__visible_for_testing int mdnie_set_name(struct mdnie_info *mdnie, unsigned int id)
{
	if (!mdnie)
		return -EINVAL;

	if (id == 0)
		snprintf(mdnie->name, MAX_MDNIE_DEV_NAME_SIZE,
				"%s", MDNIE_DEV_NAME);
	else
		snprintf(mdnie->name, MAX_MDNIE_DEV_NAME_SIZE,
				"%s-%d", MDNIE_DEV_NAME, id);

	return 0;
}

__visible_for_testing const char *mdnie_get_name(struct mdnie_info *mdnie)
{
	return mdnie ? mdnie->name : NULL;
}

__visible_for_testing int mdnie_create_class(struct mdnie_info *mdnie)
{
	if (!mdnie)
		return -EINVAL;

	mdnie->class = class_create(THIS_MODULE, mdnie_get_name(mdnie));
	if (IS_ERR_OR_NULL(mdnie->class)) {
		panel_err("failed to create mdnie class\n");
		return -EINVAL;
	}

	return 0;
}

__visible_for_testing int mdnie_destroy_class(struct mdnie_info *mdnie)
{
	if (!mdnie || !mdnie->class)
		return -EINVAL;

	class_destroy(mdnie->class);
	mdnie->class = NULL;

	return 0;
}

__visible_for_testing int mdnie_create_device(struct mdnie_info *mdnie)
{
	if (!mdnie)
		return -EINVAL;

	if (!to_panel_device(mdnie)->lcd_dev) {
		panel_err("lcd_device is null\n");
		return -EINVAL;
	}

	mdnie->dev = device_create(mdnie->class,
			to_panel_device(mdnie)->lcd_dev,
			0, &mdnie, "%s", mdnie_get_name(mdnie));
	if (IS_ERR_OR_NULL(mdnie->dev)) {
		panel_err("failed to create mdnie device\n");
		return -EINVAL;
	}

	dev_set_drvdata(mdnie->dev, mdnie);

	return 0;
}

__visible_for_testing int mdnie_destroy_device(struct mdnie_info *mdnie)
{
	if (!mdnie)
		return -EINVAL;

	if (!mdnie->dev) {
		panel_err("mdnie device is null\n");
		return -ENODEV;
	}

	device_unregister(mdnie->dev);
	mdnie->dev = NULL;

	return 0;
}

__visible_for_testing int mdnie_create_device_files(struct mdnie_info *mdnie)
{
	int i, ret;

	if (!mdnie || !mdnie->dev)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(mdnie_dev_attrs); i++) {
		ret = device_create_file(mdnie->dev, &mdnie_dev_attrs[i].dev_attr);
		if (ret < 0) {
			panel_err("failed to add %s sysfs entries, %d\n",
					mdnie_dev_attrs[i].dev_attr.attr.name, ret);
			return ret;
		}
	}

	return 0;
}

__visible_for_testing int mdnie_remove_device_files(struct mdnie_info *mdnie)
{
	int i;

	if (!mdnie || !mdnie->dev)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(mdnie_dev_attrs); i++)
		device_remove_file(mdnie->dev, &mdnie_dev_attrs[i].dev_attr);

	return 0;
}

__visible_for_testing int mdnie_create_class_and_device(struct mdnie_info *mdnie)
{
	int ret;

	if (!mdnie)
		return -EINVAL;

	ret = mdnie_create_class(mdnie);
	if (ret < 0)
		return ret;

	ret = mdnie_create_device(mdnie);
	if (ret < 0)
		goto error1;

	ret = mdnie_create_device_files(mdnie);
	if (ret < 0)
		goto error2;

	return 0;

error2:
	mdnie_destroy_device(mdnie);
error1:
	mdnie_destroy_class(mdnie);

	return ret;
}

__visible_for_testing int mdnie_remove_class_and_device(struct mdnie_info *mdnie)
{
	int ret;

	ret = mdnie_remove_device_files(mdnie);
	if (ret < 0)
		return -EINVAL;

	ret = mdnie_destroy_device(mdnie);
	if (ret < 0)
		return -EINVAL;

	ret = mdnie_destroy_class(mdnie);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

int mdnie_init(struct mdnie_info *mdnie)
{
	int ret;
	struct panel_device *panel;

	if (!mdnie)
		return -EINVAL;

	panel = to_panel_device(mdnie);
	panel_mutex_init(panel, &mdnie->lock);

	ret = mdnie_set_name(mdnie, panel->id);
	if (ret < 0)
		return -EINVAL;

	ret = mdnie_create_class_and_device(mdnie);
	if (ret < 0)
		return -EINVAL;

	panel_info("mdnie init success\n");

	return 0;
}

int mdnie_exit(struct mdnie_info *mdnie)
{
	if (!mdnie)
		return -EINVAL;

	panel_mutex_lock(&mdnie->lock);
	mdnie_remove_class_and_device(mdnie);
	memset(mdnie->name, 0, sizeof(mdnie->name));
	panel_mutex_unlock(&mdnie->lock);

	return 0;
}

int mdnie_prepare(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune)
{
	int ret = 0;
	struct panel_device *panel;

	if (!mdnie || !mdnie_tune)
		return -EINVAL;

	panel = to_panel_device(mdnie);

	panel_mutex_lock(&mdnie->lock);
	ret = mdnie_init_property(mdnie, mdnie_tune);
	if (ret < 0)
		goto err;

	ret = panel_add_command_from_initdata_seqtbl(mdnie_tune->seqtbl,
			mdnie_tune->nr_seqtbl, &panel->command_initdata_list);
	if (ret < 0) {
		panel_err("failed to panel_add_command_from_initdata_seqtbl\n");
		goto err;
	}

	panel_mutex_unlock(&mdnie->lock);

	return 0;

err:
	panel_mutex_unlock(&mdnie->lock);
	panel_err("failed to prepare mdnie\n");

	return ret;
}

int mdnie_unprepare(struct mdnie_info *mdnie)
{
	int ret;
	struct panel_device *panel;

	if (!mdnie)
		return -EINVAL;

	panel = to_panel_device(mdnie);
	panel_mutex_lock(&mdnie->lock);
	ret = mdnie_deinit_property(mdnie);
	if (ret < 0)
		goto err;
	panel_mutex_unlock(&mdnie->lock);

	return 0;

err:
	panel_mutex_unlock(&mdnie->lock);
	panel_err("failed to unprepare mdnie\n");

	return ret;
}

int mdnie_probe(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune)
{
	int ret;

	if (unlikely(!mdnie || !mdnie_tune)) {
		panel_err("invalid argument\n");
		return -ENODEV;
	}

	panel_mutex_lock(&mdnie->lock);

	ret = mdnie_register_fb(mdnie);
	if (ret < 0)
		goto err;

#ifdef CONFIG_USDM_PANEL_DPUI
	ret = mdnie_register_dpui(mdnie);
	if (ret < 0)
		goto err;
#endif
	panel_mutex_unlock(&mdnie->lock);

	ret = mdnie_enable(mdnie);
	if (ret < 0) {
		panel_err("failed to enable mdnie\n");
		return ret;
	}

	panel_info("mdnie probe success\n");

	return 0;

err:
	panel_mutex_unlock(&mdnie->lock);
	panel_err("failed to probe mdnie\n");

	return ret;
}

int mdnie_remove(struct mdnie_info *mdnie)
{
	if (!mdnie)
		return -EINVAL;

	mdnie_disable(mdnie);
	panel_mutex_lock(&mdnie->lock);
#ifdef CONFIG_USDM_PANEL_DPUI
	mdnie_unregister_dpui(mdnie);
#endif
	mdnie_unregister_fb(mdnie);

	panel_mutex_unlock(&mdnie->lock);

	return 0;
}

