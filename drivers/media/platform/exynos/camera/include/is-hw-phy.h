/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_PHY_H
#define IS_HW_PHY_H

enum pablo_phy_tune_mode {
	PABLO_PHY_TUNE_DISABLE = 0,
	PABLO_PHY_TUNE_CPHY,
	PABLO_PHY_TUNE_DPHY,
};

enum pablo_phy_info {
	PPI_VERSION,
	PPI_TYPE,
	PPI_LANES,
	PPI_SPEED,
	PPI_SETTLE,
	PPI_MAX,
};

enum pablo_phy_type {
	PPT_MIPI_DPHY,
	PPT_MIPI_CPHY,
};

enum pablo_phy_setfile {
	PPS_COMM,
	PPS_LANE,
	PPS_MAX,
};

enum pablo_phy_port {
	PPP_COMMON, /* can be used with all phy ch */
	PPP_CSIA,
	PPP_CSIB,
	PPP_CSIC,
	PPP_CSID,
	PPP_CSIE,
	PPP_CSIF,
	PPP_CSIG,
};

struct phy_setfile_header {
	u8 body_cnt;
	u8 body[0];
} __packed;

struct phy_setfile_body {
	u8 phy_type;
	u8 phy_port;
	u8 cnt[PPS_MAX];
	u8 data[0];
} __packed;

struct phy_setfile {
	u32 addr;
	u32 start;
	u32 width;
	u32 val;
	u32 index;
	u32 max_lane;
};

struct phy_setfile_table {
	struct phy_setfile	*sf_comm;
	size_t			sz_comm;
	struct phy_setfile	*sf_lane;
	size_t			sz_lane;
};

static const char * const phy_setfile_str[] = {"comm", "lane"};

#endif /* IS_HW_PHY_H */
