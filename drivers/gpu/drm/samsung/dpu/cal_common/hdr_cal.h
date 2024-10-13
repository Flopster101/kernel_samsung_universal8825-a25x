/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG DQE CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_HDR_CAL_H__
#define __SAMSUNG_HDR_CAL_H__

#include <linux/types.h>

struct hdr_regs {
	void __iomem *hdr_base_regs;
};

enum hdr_regs_type {
	REGS_HDR_LSI = 0,
	REGS_HDR_TYPE_MAX
};

enum hdr_hw_type {
	HDR_HW_TYPE_0 = 0,	// none
	HDR_HW_TYPE_1,		// S.LSI
	HDR_HW_TYPE_2,		// custom
	HDR_HW_TYPE_MAX,
};

void __hdr_dump(u32 id, struct hdr_regs	*regs);
bool hdr_reg_verify(u32 id, u32 hw_type, u32 start_offset, u32 length);
void hdr_reg_set_lut(u32 id, u32 offset, u32 length, const u32 *lut);
void hdr_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		enum hdr_regs_type type);

#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
void hdr_write_reg(u32 id, u32 offset, u32 value);
uint32_t hdr_read_reg(u32 id, u32 offset);
#endif

#endif /* __SAMSUNG_HDR_CAL_H__ */
