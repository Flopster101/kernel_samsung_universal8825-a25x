/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_TYPE_H
#define IS_HW_API_TYPE_H

struct is_hw_size_config {
	u32 sensor_calibrated_width;
	u32 sensor_calibrated_height;
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	u32 sensor_crop_x;
	u32 sensor_crop_y;
	u32 sensor_crop_width;
	u32 sensor_crop_height;
	u32 bcrop_x;
	u32 bcrop_y;
	u32 bcrop_width;
	u32 bcrop_height;
	u32 taasc_width;
	u32 dma_crop_x;
	u32 width;
	u32 height;
};

#endif /* IS_HW_API_TYPE_H */


