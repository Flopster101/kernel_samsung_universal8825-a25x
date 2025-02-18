/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MDNIE_H__
#define __MDNIE_H__

#include "panel.h"
#include "dpui.h"

typedef u8 mdnie_t;

#define MDNIE_DEV_NAME ("mdnie")
#define MAX_MDNIE_DEV_NAME_SIZE (32)
#define MDNIE_SYSFS_PREFIX      "/sdcard/mdnie/"

#define IS_MDNIE_ENABLED(_mdnie_)	(!!(_mdnie_)->props.enable)
#define IS_SCENARIO(idx)		(idx < SCENARIO_MAX && !(idx > VIDEO_NORMAL_MODE && idx < CAMERA_MODE))
#define IS_ACCESSIBILITY(idx)		(idx && idx < ACCESSIBILITY_MAX)
#define IS_HMD(idx)			(idx && idx < HMD_MDNIE_MAX)

#define SCENARIO_IS_VALID(idx)		(IS_SCENARIO(idx))
#define IS_NIGHT_MODE_ON(idx)		(idx == NIGHT_MODE_ON)
#define IS_COLOR_LENS_ON(idx)		(idx == COLOR_LENS_ON)
#define IS_HDR(idx)			(idx > HDR_OFF && idx < HDR_MAX)
#define IS_LIGHT_NOTIFICATION(idx)			(idx >= LIGHT_NOTIFICATION_ON && idx < LIGHT_NOTIFICATION_MAX)

#define COLOR_OFFSET_FUNC(x, y, num, den, con)  \
	    (((s64)y * 1024LL) + ((((s64)x * 1024LL) * ((s64)num)) / ((s64)den)) + ((s64)con * 1024LL))
#define RECOGNIZE_REGION(compV, compH)	\
		((((compV) > 0) ? (0 + (((compH) > 0) ? 0 : 1)) : (2 + (((compH) < 0) ? 0 : 1))))

#define IS_BYPASS_MODE(_mdnie_)	(((_mdnie_)->props.bypass >= BYPASS_ON) && ((_mdnie_)->props.bypass < BYPASS_MAX))
#define IS_ACCESSIBILITY_MODE(_mdnie_)	(((_mdnie_)->props.accessibility > ACCESSIBILITY_OFF) && ((_mdnie_)->props.accessibility < ACCESSIBILITY_MAX))
#define IS_LIGHT_NOTIFICATION_MODE(_mdnie_)			(((_mdnie_)->props.light_notification >= LIGHT_NOTIFICATION_ON) && ((_mdnie_)->props.light_notification < LIGHT_NOTIFICATION_MAX))
#define IS_HDR_MODE(_mdnie_)			(((_mdnie_)->props.hdr > HDR_OFF) && ((_mdnie_)->props.hdr < HDR_MAX))
#define IS_HMD_MODE(_mdnie_)			(((_mdnie_)->props.hmd > HMD_MDNIE_OFF) && ((_mdnie_)->props.hmd < HMD_MDNIE_MAX))
#define IS_NIGHT_MODE(_mdnie_)			(((_mdnie_)->props.night >= NIGHT_MODE_ON) && ((_mdnie_)->props.night < NIGHT_MODE_MAX))
#define IS_COLOR_LENS_MODE(_mdnie_)		(((_mdnie_)->props.color_lens >= COLOR_LENS_ON) && ((_mdnie_)->props.color_lens < COLOR_LENS_MAX))
#define IS_HBM_CE_MODE(_mdnie_)			(((_mdnie_)->props.extra_dim_level == 0) && ((_mdnie_)->props.hbm_ce_level > 0))
#define IS_SCENARIO_MODE(_mdnie_)		(((_mdnie_)->props.scenario >= UI_MODE && (_mdnie_)->props.scenario <= VIDEO_NORMAL_MODE) || \
		((_mdnie_)->props.scenario >= CAMERA_MODE && (_mdnie_)->props.scenario < SCENARIO_MAX))
#define IS_LDU_MODE(_mdnie_)			(((_mdnie_)->props.ldu > LDU_MODE_OFF) && ((_mdnie_)->props.ldu < MAX_LDU_MODE))

#define MAX_MDNIE_SCR_LEN	(24)
#define MAX_COLOR (3)

#define GET_MSB_8BIT(x)		((x >> 8) & (BIT(8) - 1))
#define GET_LSB_8BIT(x)		((x >> 0) & (BIT(8) - 1))

#define MIN_WRGB_OFS	(-250)
#define MAX_WRGB_OFS	(0)
#define IS_VALID_WRGB_OFS(_ofs_)	(((int)(_ofs_) <= MAX_WRGB_OFS) && ((int)(_ofs_) >= (MIN_WRGB_OFS)))

#define MIN_WCRD_X	(2930)
#define MAX_WCRD_X	(3060)
#define MIN_WCRD_Y	(3050)
#define MAX_WCRD_Y	(3260)

#ifdef CONFIG_USDM_MDNIE_AFC
#define MAX_AFC_ROI_LEN	(12)
#endif

enum MDNIE_MODE {
	MDNIE_OFF_MODE,
	MDNIE_BYPASS_MODE,
	MDNIE_ACCESSIBILITY_MODE,
	MDNIE_LIGHT_NOTIFICATION_MODE,
	MDNIE_COLOR_LENS_MODE,
	MDNIE_HDR_MODE,
	MDNIE_HMD_MODE,
	MDNIE_NIGHT_MODE,
	MDNIE_HBM_CE_MODE,
	MDNIE_SCENARIO_MODE,
	MAX_MDNIE_MODE,
};

#define MDNIE_MODE_PROPERTY ("mdnie_mode")

enum SCENARIO {
	UI_MODE,
	VIDEO_NORMAL_MODE,
	CAMERA_MODE = 4,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	BROWSER_MODE,
	EBOOK_MODE,
	EMAIL_MODE,
	GAME_LOW_MODE,
	GAME_MID_MODE,
	GAME_HIGH_MODE,
	VIDEO_ENHANCER,
	VIDEO_ENHANCER_THIRD,
	HMD_8_MODE,
	HMD_16_MODE,
	SCENARIO_MAX,
};

#define MDNIE_SCENARIO_PROPERTY ("mdnie_scenario")

enum SCENARIO_MODE {
	DYNAMIC,
	STANDARD,
	NATURAL,
	MOVIE,
	AUTO,
	MODE_MAX,
};

#define MDNIE_SCENARIO_MODE_PROPERTY ("mdnie_scenario_mode")

enum MDNIE_SCREEN_MODE {
	MDNIE_SCREEN_MODE_VIVID,
	MDNIE_SCREEN_MODE_NATURAL,
	MAX_MDNIE_SCREEN_MODE
};

#define MDNIE_SCREEN_MODE_PROPERTY ("mdnie_screen_mode")

enum BYPASS {
	BYPASS_OFF = 0,
	BYPASS_ON,
	BYPASS_MAX
};

#define MDNIE_BYPASS_PROPERTY ("mdnie_bypass")

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	GRAYSCALE,
	GRAYSCALE_NEGATIVE,
	COLOR_BLIND_HBM,
	ACCESSIBILITY_MAX
};

#define MDNIE_ACCESSIBILITY_PROPERTY ("mdnie_accessibility")

#define MDNIE_HBM_CE_PROPERTY ("mdnie_hbm_ce")

enum HBM_CE_MODE {
	HBM_CE_MODE_OFF,
	HBM_CE_MODE_ON,
	HBM_CE_MODE_MAX,
};

#define MDNIE_HBM_CE_LEVEL_PROPERTY ("mdnie_hbm_ce_level")

enum HMD_MODE {
	HMD_MDNIE_OFF,
	HMD_3000K,
	HMD_4000K,
	HMD_5000K,
	HMD_6500K,
	HMD_7500K,
	HMD_MDNIE_MAX
};

#define MDNIE_HMD_PROPERTY ("mdnie_hmd_mode")

enum NIGHT_MODE {
	NIGHT_MODE_OFF,
	NIGHT_MODE_ON,
	NIGHT_MODE_MAX
};

#define MDNIE_NIGHT_MODE_PROPERTY ("mdnie_night_mode")

enum NIGHT_LEVEL {
	NIGHT_LEVEL_6500K,
	NIGHT_LEVEL_6100K,
	NIGHT_LEVEL_5700K,
	NIGHT_LEVEL_5300K,
	NIGHT_LEVEL_4900K,
	NIGHT_LEVEL_4500K,
	NIGHT_LEVEL_4100K,
	NIGHT_LEVEL_3700K,
	NIGHT_LEVEL_3300K,
	NIGHT_LEVEL_2900K,
	NIGHT_LEVEL_2500K,
	NIGHT_LEVEL_7000K,
	MAX_NIGHT_LEVEL,
};

#define MDNIE_NIGHT_LEVEL_PROPERTY ("mdnie_night_level")

enum COLOR_LENS {
	COLOR_LENS_OFF,
	COLOR_LENS_ON,
	COLOR_LENS_MAX
};

#define MDNIE_ANTI_GLARE_PROPERTY ("mdnie_anti_glare")

enum ANTI_GLARE {
	ANTI_GLARE_OFF,
	ANTI_GLARE_ON,
	ANTI_GLARE_MAX
};


#define MDNIE_COLOR_LENS_PROPERTY ("mdnie_color_lens")

enum COLOR_LENS_COLOR {
	COLOR_LENS_COLOR_BLUE,
	COLOR_LENS_COLOR_AZURE,
	COLOR_LENS_COLOR_CYAN,
	COLOR_LENS_COLOR_SPRING_GREEN,
	COLOR_LENS_COLOR_GREEN,
	COLOR_LENS_COLOR_CHARTREUSE_GREEN,
	COLOR_LENS_COLOR_YELLOW,
	COLOR_LENS_COLOR_ORANGE,
	COLOR_LENS_COLOR_RED,
	COLOR_LENS_COLOR_ROSE,
	COLOR_LENS_COLOR_MAGENTA,
	COLOR_LENS_COLOR_VIOLET,
	COLOR_LENS_COLOR_MAX
};

#define MDNIE_COLOR_LENS_COLOR_PROPERTY ("mdnie_color_lens_color")

enum COLOR_LENS_LEVEL {
	COLOR_LENS_LEVEL_20P,
	COLOR_LENS_LEVEL_25P,
	COLOR_LENS_LEVEL_30P,
	COLOR_LENS_LEVEL_35P,
	COLOR_LENS_LEVEL_40P,
	COLOR_LENS_LEVEL_45P,
	COLOR_LENS_LEVEL_50P,
	COLOR_LENS_LEVEL_55P,
	COLOR_LENS_LEVEL_60P,
	COLOR_LENS_LEVEL_MAX,
};

#define MDNIE_COLOR_LENS_LEVEL_PROPERTY ("mdnie_color_lens_level")

enum HDR {
	HDR_OFF,
	HDR_1,
	HDR_2,
	HDR_3,
	HDR_MAX
};

#define MDNIE_HDR_PROPERTY ("mdnie_hdr")

enum LIGHT_NOTIFICATION {
	LIGHT_NOTIFICATION_OFF = 0,
	LIGHT_NOTIFICATION_ON,
	LIGHT_NOTIFICATION_MAX
};

#define MDNIE_LIGHT_NOTIFICATION_PROPERTY ("mdnie_light_notification")

enum {
	CCRD_PT_NONE,
	CCRD_PT_1,
	CCRD_PT_2,
	CCRD_PT_3,
	CCRD_PT_4,
	CCRD_PT_5,
	CCRD_PT_6,
	CCRD_PT_7,
	CCRD_PT_8,
	CCRD_PT_9,
	MAX_CCRD_PT,
};

#define MDNIE_CCRD_PT_PROPERTY ("mdnie_ccrd_pt")

enum LDU_MODE {
	LDU_MODE_OFF,
	LDU_MODE_1,
	LDU_MODE_2,
	LDU_MODE_3,
	LDU_MODE_4,
	LDU_MODE_5,
	MAX_LDU_MODE,
};

#define MDNIE_LDU_MODE_PROPERTY ("mdnie_ldu_mode")

enum SCR_WHITE_MODE {
	SCR_WHITE_MODE_NONE,
	SCR_WHITE_MODE_COLOR_COORDINATE,
	SCR_WHITE_MODE_ADJUST_LDU,
	SCR_WHITE_MODE_SENSOR_RGB,
	MAX_SCR_WHITE_MODE,
};

#define MDNIE_SCR_WHITE_MODE_PROPERTY ("mdnie_scr_white")

enum TRANS_MODE {
	TRANS_OFF,
	TRANS_ON,
	MAX_TRANS_MODE,
};

#define MDNIE_EXTRA_DIM_LEVEL_PROPERTY ("mdnie_extra_dim_level")

#define MDNIE_TRANS_MODE_PROPERTY ("mdnie_trans_mode")

#define MDNIE_VIVIDNESS_LEVEL_PROPERTY ("mdnie_vividness_level")

#define MDNIE_ENABLE_PROPERTY ("mdnie_enable")

#define MDNIE_PRE_SEQ ("mdnie_pre_seq")
#define MDNIE_POST_SEQ ("mdnie_post_seq")

/* scenario */
#define MDNIE_SCENARIO_SEQ ("mdnie_scenario_seq")

/* accessibility */
#define MDNIE_NEGATIVE_SEQ ("mdnie_negative_seq")
#define MDNIE_COLOR_BLIND_SEQ ("mdnie_color_blind_seq")
#define MDNIE_SCREEN_CURTAIN_SEQ ("mdnie_screen_curtain_seq")
#define MDNIE_GRAYSCALE_SEQ ("mdnie_grayscale_seq")
#define MDNIE_GRAYSCALE_NEGATIVE_SEQ ("mdnie_grayscale_negative_seq")
#define MDNIE_COLOR_BLIND_HBM_SEQ ("mdnie_color_blind_hbm_seq")

/* mdnie sysfs related */
#define MDNIE_NIGHT_SEQ ("mdnie_night_seq")
#define MDNIE_BYPASS_SEQ ("mdnie_bypass_seq")
#define MDNIE_HBM_CE_SEQ ("mdnie_hbm_ce_seq")
#define MDNIE_HMD_SEQ ("mdnie_hmd_seq")
#define MDNIE_HDR_SEQ ("mdnie_hdr_seq")
#define MDNIE_NIGHT_SEQ ("mdnie_night_seq")
#define MDNIE_LIGHT_NOTIFICATION_SEQ ("mdnie_light_notification_seq")
#define MDNIE_COLOR_LENS_SEQ ("mdnie_color_lens_seq")

#ifdef CONFIG_USDM_MDNIE_AFC
#define MDNIE_AFC_OFF_SEQ ("mdnie_afc_off_seq")
#define MDNIE_AFC_ON_SEQ ("mdnie_afc_on_seq")
#endif

enum {
	WCRD_TYPE_ADAPTIVE,
	WCRD_TYPE_D65,
	MAX_WCRD_TYPE,
};

enum {
	H_LINE,
	V_LINE,
	MAX_CAL_LINE,
};

enum {
	RGB_00,
	RGB_01,
	RGB_10,
	RGB_11,
	MAX_RGB_PT,
};

enum {
	QUAD_1,
	QUAD_2,
	QUAD_3,
	QUAD_4,
	MAX_QUAD,
};

struct cal_center {
	int x;
	int y;
	int thres;
};

struct cal_line {
	int num;
	int den;
	int con;
};

struct cal_coef {
	int a, b, c, d;
	int e, f, g, h;
};

#define MAX_HBM_CE_LEVEL (64)
#define MAX_ANTI_GLARE_LEVEL (6)
#define MAX_EXTRA_DIM_LEVEL (100)
#define MAX_VIVIDNESS_LEVEL (3)

struct mdnie_tune {
	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct cal_center center;
	struct cal_line line[MAX_CAL_LINE];
	struct cal_coef coef[MAX_QUAD];
	u32 cal_x_center;
	u32 cal_y_center;
	u32 cal_boundary_center;
	u8 vtx[MAX_WCRD_TYPE][MAX_CCRD_PT][MAX_COLOR];
	u8 adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR];
	u32 num_ldu_mode;
	u32 num_night_level;
	u32 num_color_lens_color;
	u32 num_color_lens_level;
	u32 hbm_ce_lux[MAX_HBM_CE_LEVEL];
	u32 anti_glare_lux[MAX_ANTI_GLARE_LEVEL];
	u32 anti_glare_ratio[MAX_ANTI_GLARE_LEVEL];
	u32 scr_white_len;
	u32 scr_cr_ofs;
	u32 night_mode_ofs;
	u32 color_lens_ofs;

	/* w/a flag */
	unsigned force_scr_white_mode_none_on_hbm:1; /* don't update scr_white values in HBM */
};

struct mdnie_properties {
	u32 enable;
	enum MDNIE_MODE mdnie_mode;
	enum SCENARIO scenario;
	enum SCENARIO_MODE scenario_mode;
	enum MDNIE_SCREEN_MODE screen_mode;
	enum BYPASS bypass;
	enum HMD_MODE hmd;
	enum NIGHT_MODE night;
	enum ANTI_GLARE anti_glare;
	enum COLOR_LENS color_lens;
	enum COLOR_LENS_COLOR color_lens_color;
	enum COLOR_LENS_LEVEL color_lens_level;
	enum HDR hdr;
	enum LIGHT_NOTIFICATION light_notification;
	enum ACCESSIBILITY accessibility;
	enum LDU_MODE ldu;
	enum SCR_WHITE_MODE scr_white_mode;
	enum TRANS_MODE trans_mode;
	u32 night_level;
	u32 hbm_ce_level;
	u32 anti_glare_level;
	u32 extra_dim_level;
	u32 vividness_level;

	/* for color adjustment */
	u8 scr[MAX_MDNIE_SCR_LEN];
	u32 sz_scr;

	int wcrd_x, wcrd_y;

	/* default whiteRGB : color coordinated wrgb */
	u8 coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR];
	/* sensor whiteRGB */
	u8 ssr_wrgb[MAX_COLOR];
	/* current whiteRGB */
	u8 cur_wrgb[MAX_COLOR];
	/* default whiteRGB : color coordinated wrgb */
	u8 def_wrgb[MAX_COLOR];
	s8 def_wrgb_ofs[MAX_COLOR];
#ifdef CONFIG_USDM_MDNIE_AFC
	u8 afc_roi[MAX_AFC_ROI_LEN];
	bool afc_on;
#endif

	u32 num_ldu_mode;
	u32 num_night_level;
	u32 num_color_lens_color;
	u32 num_color_lens_level;
	struct cal_line line[MAX_CAL_LINE];
	struct cal_coef coef[MAX_QUAD];
	u32 cal_x_center;
	u32 cal_y_center;
	u32 cal_boundary_center;
	u8 vtx[MAX_WCRD_TYPE][MAX_CCRD_PT][MAX_COLOR];
	u8 adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR];

	s32 *hbm_ce_lux;
	u32 num_hbm_ce_lux;
	s32 anti_glare_lux[MAX_ANTI_GLARE_LEVEL];
	s32 anti_glare_ratio[MAX_ANTI_GLARE_LEVEL];
	u32 scr_white_len;
	u32 scr_cr_ofs;
	u32 night_mode_ofs;
	u32 color_lens_ofs;

	/* support */
	unsigned support_ldu:1;
	unsigned support_trans:1;

	/* dirty flag */
	unsigned tuning:1;				/* tuning file loaded */
	unsigned update_color_adj:1;	/* color adjustement updated */
	unsigned update_scenario:1;		/* scenario updated */
	unsigned update_color_coord:1;	/* color coordinate updated */
	unsigned update_ldu:1;			/* adjust ldu updated */
	unsigned update_sensorRGB:1;	/* sensorRGB scr white updated */

	/* w/a flag */
	unsigned force_scr_white_mode_none_on_hbm:1; /* don't update scr_white values in HBM */

	/* mdnie tuning */
#ifdef CONFIG_USDM_MDNIE_TUNING
	char tfilepath[128];			/* tuning file path */
#endif
};

struct mdnie_info {
	struct device *dev;
	struct class *class;
	char name[MAX_MDNIE_DEV_NAME_SIZE];
	struct panel_mutex lock;
	struct mdnie_properties props;
	struct notifier_block fb_notif;
#ifdef CONFIG_USDM_PANEL_DPUI
	struct notifier_block dpui_notif;
#endif
};

#define __MDNIE_ATTR_RO(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 PN_CONCAT(_name, show), NULL),		\
				.flags = _flags,				\
			}

#define __MDNIE_ATTR_WO(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 NULL, mdnie_store_check_test_mode),		\
				.flags = _flags,				\
				.store = PN_CONCAT(_name, store) \
			}

#define __MDNIE_ATTR_RW(_name, _mode, _flags) {	\
				.dev_attr = __ATTR(_name, _mode,		\
					 PN_CONCAT(_name, show), mdnie_store_check_test_mode),		\
				.flags = _flags,				\
				.store = PN_CONCAT(_name, store) \
			}


#ifdef CONFIG_USDM_MDNIE
extern int mdnie_init(struct mdnie_info *mdnie);
extern int mdnie_exit(struct mdnie_info *mdnie);
extern int mdnie_prepare(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune);
extern int mdnie_unprepare(struct mdnie_info *mdnie);
extern int mdnie_probe(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune);
extern int mdnie_remove(struct mdnie_info *mdnie);
extern int mdnie_enable(struct mdnie_info *mdnie);
extern int mdnie_disable(struct mdnie_info *mdnie);
extern int mdnie_set_def_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b);
extern int mdnie_set_cur_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b);
extern int mdnie_cur_wrgb_to_byte_array(struct mdnie_info *mdnie,
		unsigned char *dst, unsigned int stride);
extern int mdnie_get_anti_glare_ratio(struct mdnie_info *mdnie);
extern int panel_mdnie_update(struct panel_device *panel);
extern int mdnie_update_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b);
#else
static inline int mdnie_init(struct mdnie_info *mdnie) { return 0; }
static inline int mdnie_exit(struct mdnie_info *mdnie) { return 0; }
static inline int mdnie_prepare(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune) { return 0; }
static inline int mdnie_unprepare(struct mdnie_info *mdnie) { return 0; }
static inline int mdnie_probe(struct mdnie_info *mdnie, struct mdnie_tune *mdnie_tune) { return 0; }
static inline int mdnie_remove(struct mdnie_info *mdnie) { return 0; }
static inline int mdnie_enable(struct mdnie_info *mdnie) { return 0; }
static inline int mdnie_disable(struct mdnie_info *mdnie) { return 0; }
static inline int panel_mdnie_update(struct panel_device *panel) { return 0; }
static inline int mdnie_set_def_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b) { return 0; }
static inline int mdnie_set_cur_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b) { return 0; }
static inline int mdnie_cur_wrgb_to_byte_array(struct mdnie_info *mdnie,
		unsigned char *dst, unsigned int stride) { return 0; }
static inline int mdnie_update_wrgb(struct mdnie_info *mdnie,
		unsigned char r, unsigned char g, unsigned char b) { return 0; }
#endif
extern int mdnie_do_sequence_nolock(struct mdnie_info *mdnie, char *seqname);
extern int mdnie_do_sequence(struct mdnie_info *mdnie, char *seqname);
#endif /* __MDNIE_H__ */
