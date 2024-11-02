/*
 *  Copyright (C) 2023, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SHUB_DROP_CLASSIFIER_H__
#define __SHUB_DROP_CLASSIFIER_H__

#define MAX_MATRIX_LEN 54

#include <linux/device.h>

struct drop_classifier_event {
	u8 drop_result;
	u32 height;
	s32 acc_x;
	s32 acc_y;
	s32 acc_z;
	u32 sns_ts_diff_ms;
	u32 rt_ts_diff_ms;
	u64 timestamp;
} __attribute__((__packed__));

#endif /* __SHUB_DROP_CLASSIFIER_H__ */
