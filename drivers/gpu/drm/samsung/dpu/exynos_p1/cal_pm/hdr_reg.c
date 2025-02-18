// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <cal_config.h>
#include <hdr_cal.h>
//#include <regs-hdr.h>
#include <dpp_cal.h>
#include <regs-dpp.h>

#define REGS_HDR_ID_MAX REGS_DPP_ID_MAX

static struct cal_regs_desc regs_hdr[REGS_HDR_TYPE_MAX][REGS_HDR_ID_MAX];

#define hdr_regs_desc(id)			(&regs_hdr[REGS_HDR_LSI][id])
#define hdr_read(id, offset)	cal_read(hdr_regs_desc(id), offset)
#define hdr_write(id, offset, val)	cal_write(hdr_regs_desc(id), offset, val)
#define hdr_read_mask(id, offset, mask)		\
		cal_read_mask(hdr_regs_desc(id), offset, mask)
#define hdr_write_mask(id, offset, val, mask)	\
		cal_write_mask(hdr_regs_desc(id), offset, val, mask)
#define hdr_write_relaxed(id, offset, val)		\
		cal_write_relaxed(hdr_regs_desc(id), offset, val)

/* HDR DUMP */
void __hdr_dump(u32 id, struct hdr_regs	*regs)
{
	void __iomem *hdr_base_regs = regs->hdr_base_regs;

	cal_log_info(id, "=== HDR_LSI SFR DUMP ===\n");
	dpu_print_hex_dump(hdr_base_regs, hdr_base_regs + 0x0000, 0x470);
}

/* HDR VERIFY */
bool hdr_reg_verify(u32 id, u32 hw_type, u32 start_offset, u32 length)
{
	if (hw_type == HDR_HW_TYPE_0)	// none
		goto error;

	return true;
error:
	cal_log_err(id, "invalid params: type %d off 0x%x len %d\n", hw_type, start_offset, length);
	return false;
}

/* HDR LUT */
void hdr_reg_set_lut(u32 id, u32 offset, u32 length, const u32 *lut)
{
	int i;

	if (!lut)
		return;
	for (i = 0; i < length; i++)
		hdr_write_relaxed(id, offset+(i*4), lut[i]);
}

void hdr_regs_desc_init(u32 id, void __iomem *regs, const char *name,
		enum hdr_regs_type type)
{
	regs_hdr[type][id].regs = regs;
	regs_hdr[type][id].name = name;
}

#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
void hdr_write_reg(u32 id, u32 offset, u32 value)
{
	hdr_write_relaxed(id, offset, value);
}

uint32_t hdr_read_reg(u32 id, u32 offset)
{
	return hdr_read(id, offset);
}
#endif
