/* SPDX-License-Identifier: GPL-2.0
 *
 * Samsung Exynos SoC series Pablo driver
 *
 * MCFP HW control APIs
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_ITP_H
#define IS_HW_API_ITP_H

#include "is-hw-common-dma.h"
#include "is-hw-api-type.h"

struct itp_control_config {
	bool dirty_bayer_sel_dns;
	bool ww_lc_en;
};

struct itp_cin_cout_config {
	bool enable;
	u32 width;
	u32 height;
};

/* CONTROL */
int itp_hw_s_control_init(void __iomem *base, u32 set_id);
int itp_hw_s_control_config(void __iomem *base, u32 set_id, struct itp_control_config *config);
void itp_hw_s_control_debug(void __iomem *base, u32 set_id);

/* CINFIFO */
int itp_hw_s_cinfifo_config(void __iomem *base, u32 set_id, struct itp_cin_cout_config *config);

/* COUTFIFO */
int itp_hw_s_coutfifo_init(void __iomem *base, u32 set_id);
int itp_hw_s_coutfifo_config(void __iomem *base, u32 set_id, struct itp_cin_cout_config *config);

/* MODULE */
int itp_hw_s_module_init(void __iomem *base, u32 set_id);
int itp_hw_s_module_bypass(void __iomem *base, u32 set_id);
int itp_hw_s_module_size(void __iomem *base, u32 set_id, struct is_hw_size_config *config);
int itp_hw_s_module_format(void __iomem *base, u32 set_id, u32 bayer_order);

/* TUNE */
void itp_hw_s_crc(void __iomem *base, u32 seed);

/* DUMP */
void itp_hw_dump(void __iomem *base);

/* INFO */
u32 itp_hw_g_reg_cnt(void);

#endif
