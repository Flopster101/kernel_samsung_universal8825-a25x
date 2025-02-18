/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef IS_STRIPE_H
#define IS_STRIPE_H

#include "is-subdev-ctrl.h"

#define dbg_stripe(level, fmt, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_STREAM)) >= (level), \
		"[stripe]", fmt, ##args)

int is_calc_region_num(int input_width, struct is_subdev *subdev);

int is_ischain_g_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		const struct is_crop *in,
		const struct is_crop *incrop,
		const struct is_crop *otcrop);

#endif /* IS_STRIPE_H */
