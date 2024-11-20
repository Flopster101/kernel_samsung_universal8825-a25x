/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_HDR_INFO_H__
#define __PANEL_HDR_INFO_H__

#define MAX_HDR_TYPE 5

struct panel_hdr_info {
	unsigned int formats;
	unsigned int max_luma;
	unsigned int max_avg_luma;
	unsigned int min_luma;
};

#endif /* __PANEL_HDR_INFO_H__ */
