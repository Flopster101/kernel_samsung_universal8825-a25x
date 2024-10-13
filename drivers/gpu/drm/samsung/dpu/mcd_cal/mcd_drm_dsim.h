/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/mcd_cal/mcd_drm_dsim.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung MCD MIPI DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_DRM_DSI_H__
#define __MCD_DRM_DSI_H__

/* Add header */
struct dsim_device;

#define MAX_HS_CLOCK			25000000 /*2.5Ghz*/
#define MAX_HS_CLOCK_STR_LEN		9
#define PMSK_VALUE_COUNT		14
#define DT_NAME_HS_TIMING		"hs_pll_timing"
#define DT_NAME_PMSK	"pmsk"

int mcd_dsim_check_dsi_freq(struct dsim_device *dsim, unsigned int freq);
int mcd_dsim_update_dsi_freq(struct dsim_device *dsim, unsigned int freq);
#endif /* __MCD_DRM_DSI_H__ */
