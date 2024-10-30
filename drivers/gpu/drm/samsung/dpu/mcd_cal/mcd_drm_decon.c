// SPDX-License-Identifier: GPL-2.0-only
/* mcd_drm_decon.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <drm/drm_mode.h>
#include <drm/drm_modes.h>

int mcd_decon_get_bts_fps(const struct drm_display_mode *mode)
{
	unsigned int num, den, vfp;

	if (mode->htotal == 0 || mode->vtotal == 0)
		return 0;

	vfp = mode->vsync_start - mode->vdisplay;
	if (mode->vtotal <= vfp)
		return 0;

	num = mode->clock;
	den = mode->htotal * (mode->vtotal - vfp);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		num *= 2;
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		den *= 2;

	return DIV_ROUND_CLOSEST_ULL(mul_u32_u32(num, 1000), den);
}
EXPORT_SYMBOL(mcd_decon_get_bts_fps);
