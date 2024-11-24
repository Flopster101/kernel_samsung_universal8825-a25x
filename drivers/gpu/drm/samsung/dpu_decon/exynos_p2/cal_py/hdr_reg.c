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
#include <regs-hdr.h>
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

	if (hw_type == HDR_HW_TYPE_1) {	// S.LSI
		switch (start_offset) {
		case HDR_LSI_L_COM_CTRL:
			if (length != 1 && length != 2)
				goto error;
			break;
		case HDR_LSI_L_MOD_CTRL:
			if (length != 1)
				goto error;
			break;
		case HDR_LSI_L_OETF_POSX(0):
			if (length != HDR_OETF_L_POSX_LUT_REG_CNT &&
				length != (HDR_OETF_L_POSX_LUT_REG_CNT+HDR_OETF_L_POSY_LUT_REG_CNT))
				goto error;
			break;
		case HDR_LSI_L_OETF_POSY(0):
			if (length != HDR_OETF_L_POSY_LUT_REG_CNT)
				goto error;
			break;
		case HDR_LSI_L_EOTF_POSX(0):
			if (length != HDR_EOTF_POSX_LUT_REG_CNT &&
				length != (HDR_EOTF_POSX_LUT_REG_CNT+HDR_EOTF_POSY_LUT_REG_CNT))
				goto error;
			break;
		case HDR_LSI_L_EOTF_POSY(0):
			if (length != HDR_EOTF_POSY_LUT_REG_CNT)
				goto error;
			break;
		case HDR_LSI_L_GM_COEF(0):
			if (length != HDR_GM_COEF_REG_CNT &&
				length != (HDR_GM_COEF_REG_CNT+HDR_GM_OFFS_REG_CNT))
				goto error;
			break;
		case HDR_LSI_L_GM_OFFS(0):
			if (length != HDR_GM_OFFS_REG_CNT)
				goto error;
			break;
		default:
			goto error;
		}
	}

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

