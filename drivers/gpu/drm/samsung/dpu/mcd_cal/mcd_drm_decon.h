/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/mcd_cal/mcd_drm_dsim.h
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung MCD DECON Driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_DRM_DECON_H__
#define __MCD_DRM_DECON_H__

#include <drm/drm_mode.h>
#include <drm/drm_modes.h>

int mcd_decon_get_bts_fps(const struct drm_display_mode *mode);

#endif /* __MCD_DRM_DECON_H__ */
