/*
 * include/linux/panel_modes.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2020 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DISPLAY_MODES_H__
#define __PANEL_DISPLAY_MODES_H__

#define PANEL_DISPLAY_MODE_NAME_LEN	(64)

/**
 * @REFRESH_MODE_NS: normal speed refresh mode
 * @REFRESH_MODE_HS: high speed refresh mode
 */
#define REFRESH_MODE_NS (0)
#define REFRESH_MODE_HS (1)
#define REFRESH_MODE_PASSIVE_HS (2)
#define MAX_REFRESH_MODE (REFRESH_MODE_PASSIVE_HS + 1)

#define REFRESH_MODE_STR(_refresh_mode_) \
	((_refresh_mode_ == (REFRESH_MODE_NS)) ? "NS" : "HS")

enum PANEL_PORCH_H {
	PANEL_PORCH_HBP,
	PANEL_PORCH_HFP,
	PANEL_PORCH_HSA,
	MAX_PANEL_H_PORCH,
};

enum PANEL_PORCH_V {
	PANEL_PORCH_VBP,
	PANEL_PORCH_VFP,
	PANEL_PORCH_VSA,
	MAX_PANEL_V_PORCH,
};

struct panel_display_mode {
	/**
	 * @head:
	 *
	 * struct list_head for mode lists.
	 */
	struct list_head head;

	/**
	 * @name:
	 *
	 * Human-readable name of the mode, filled out with panel_display_mode_set_name().
	 */
	char name[PANEL_DISPLAY_MODE_NAME_LEN];

	unsigned int width;
	unsigned int height;
	unsigned int refresh_rate;
	unsigned int refresh_mode;
	unsigned int panel_refresh_rate;
	unsigned int panel_refresh_mode;
	unsigned int panel_te_st;
	unsigned int panel_te_ed;
	unsigned int panel_te_sw_skip_count;
	unsigned int panel_te_hw_skip_count;
	/* dsc parameters */
	bool dsc_en;
	unsigned int dsc_cnt;
	unsigned int dsc_slice_num;
	unsigned int dsc_slice_w;
	unsigned int dsc_slice_h;
	/* video mode parameters */
	bool panel_video_mode;
	unsigned int panel_hporch[MAX_PANEL_H_PORCH];
	unsigned int panel_vporch[MAX_PANEL_V_PORCH];
	/*
	 * TODO: move to display controller's display mode structure.
	 *
	 * panel_display_mode contains common parameters
	 * between display controller and panel driver.
	 * @cmd_lp_ref is only necessary in display controller
	 * especially for dsim driver. so it need to move to
	 * display controller's display mode structure.
	 */
	/* dsi parameters */
	unsigned int cmd_lp_ref;

	/* qos parameters */
	unsigned int disp_qos_fps;

	void *pdata;
};

struct panel_display_modes {
	unsigned int num_modes;
	unsigned int native_mode;

	struct panel_display_mode **modes;
};

#define PANEL_MODE_FMT    "\"%s\" %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d"
#define PANEL_MODE_ARG(m) \
	(m)->name, (m)->width, (m)->height, \
	(m)->refresh_rate, (m)->refresh_mode, \
	(m)->panel_refresh_rate, (m)->panel_refresh_mode, \
	(m)->panel_te_st, (m)->panel_te_ed, \
	(m)->panel_te_sw_skip_count, (m)->panel_te_hw_skip_count, \
	(m)->dsc_en, (m)->dsc_cnt, (m)->dsc_slice_num, (m)->dsc_slice_w, (m)->dsc_slice_h, \
	(m)->panel_video_mode, \
	(m)->panel_hporch[PANEL_PORCH_HBP], (m)->panel_hporch[PANEL_PORCH_HFP], (m)->panel_hporch[PANEL_PORCH_HSA], \
	(m)->panel_vporch[PANEL_PORCH_VBP], (m)->panel_vporch[PANEL_PORCH_VFP], (m)->panel_vporch[PANEL_PORCH_VSA], \
	(m)->cmd_lp_ref

#define PANEL_MODE(nm, w, h, rr, rm, prr, prm, te_st, te_ed, te_ssc, te_hsc, dsce, dscc, dscsn, dscsh, video_mode, hbp, hfp, hsa, vbp, vfp, vsa, lp_ref) \
	.name = (nm), .width = (w), .height = (h), \
	.refresh_rate = (rr), .refresh_mode = (rm), \
	.panel_refresh_rate = (prr), .panel_refresh_mode = (prm), \
	.panel_te_st = (te_st), .panel_te_ed = (te_ed), \
	.panel_te_sw_skip_count = (te_ssc), .panel_te_hw_skip_count = (te_hsc), \
	.dsc_en = (dsce), .dsc_cnt = (dscc), .dsc_slice_num = (dscsn), \
	.dsc_slice_w = ((w) / (dscsn)), .dsc_slice_h = (dscsh), \
	.panel_video_mode = (video_mode), \
	.panel_hporch[PANEL_PORCH_HBP] = (hbp), .panel_hporch[PANEL_PORCH_HFP] = (hfp), .panel_hporch[PANEL_PORCH_HSA] = (hsa), \
	.panel_vporch[PANEL_PORCH_VBP] = (vbp), .panel_vporch[PANEL_PORCH_VFP] = (vfp), .panel_vporch[PANEL_PORCH_VSA] = (vsa), \
	.cmd_lp_ref = (lp_ref)

void panel_mode_set_name(struct panel_display_mode *mode);
struct panel_display_mode *panel_mode_create(void);
struct panel_display_modes *of_get_panel_display_modes(const struct device_node *np);
const char *refresh_mode_to_str(int refresh_mode);
int str_to_refresh_mode(const char *str);
int panel_mode_vscan(const struct panel_display_mode *mode);
#endif /* __PANEL_DISPLAY_MODES_H__ */
