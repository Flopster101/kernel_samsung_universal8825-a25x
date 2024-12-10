/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sec_detect.h>
#include "../usdm_panel_debug.h"
#include "../usdm_panel_function.h"
#include "tft_common.h"
#include "tft_function.h"

struct pnobj_func tft_function_table[MAX_TFT_FUNCTION] = {
	[TFT_MAPTBL_INIT_DEFAULT] = __PNOBJ_FUNC_INITIALIZER(TFT_MAPTBL_INIT_DEFAULT, tft_maptbl_init_default),
	[TFT_MAPTBL_INIT_BRT] = __PNOBJ_FUNC_INITIALIZER(TFT_MAPTBL_INIT_BRT, tft_maptbl_init_brt),
	[TFT_MAPTBL_GETIDX_BRT] = __PNOBJ_FUNC_INITIALIZER(TFT_MAPTBL_GETIDX_BRT, TFT_MAPTBL_GETIDX_BRT),
	[TFT_MAPTBL_COPY_DEFAULT] = __PNOBJ_FUNC_INITIALIZER(TFT_MAPTBL_COPY_DEFAULT, tft_maptbl_copy_default),
};
EXPORT_SYMBOL(tft_function_table);

static int __init tft_function_init(void)
{
	int ret;

	if (sec_current_device != SEC_GTA4XLS) {
		SEC_DETECT_LOG("Skipped usdm tft common panel driver\n");
		return 0;
	}
	ret = usdm_panel_function_insert_array(tft_function_table,
			ARRAY_SIZE(tft_function_table));
	if (ret < 0)
		panel_err("failed to insert tft_function_table\n");
		
	SEC_DETECT_LOG("Initialized usdm tft common panel driver\n");
	return 0;
}

static void __exit tft_function_exit(void)
{
}

module_init(tft_function_init)
module_exit(tft_function_exit)

MODULE_DESCRIPTION("tft_function driver");
MODULE_LICENSE("GPL");
