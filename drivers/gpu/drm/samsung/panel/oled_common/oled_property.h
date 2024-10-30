/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_PROPERTY_H__
#define __OLED_PROPERTY_H__

#include "../panel_property.h"

enum {
	OLED_BR_HBM_OFF,
	OLED_BR_HBM_ON,
	MAX_OLED_BR_HBM,
};

enum {
	OLED_PREV_BR_HBM_OFF,
	OLED_PREV_BR_HBM_ON,
	MAX_OLED_PREV_BR_HBM,
};

/* oled common property */
#define OLED_BR_HBM_PROPERTY ("oled_br_hbm")
#define OLED_PREV_BR_HBM_PROPERTY ("oled_prev_br_hbm")
#define OLED_TSET_PROPERTY ("oled_tset")

/* model property */
#define OLED_NRM_BR_INDEX_PROPERTY ("oled_nrm_br_index")
#define OLED_HMD_BR_INDEX_PROPERTY ("oled_hmd_br_index")
#define OLED_LPM_BR_INDEX_PROPERTY ("oled_lpm_br_index")
#define OLED_HS_CLK_PROPERTY ("oled_hs_clk")
#define OLED_MDNIE_NIGHT_LEVEL_PROPERTY ("oled_mdnie_night_level")
#define OLED_MDNIE_HBM_CE_LEVEL_PROPERTY ("oled_mdnie_hbm_ce_level")

extern struct panel_prop_list oled_property_array[];
extern unsigned int oled_property_array_size;

#endif /* __OLED_PROPERTY_H__ */
