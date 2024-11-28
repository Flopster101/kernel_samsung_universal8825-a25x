/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TFT_FUNCTION_H__
#define __TFT_FUNCTION_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include "../panel_function.h"

enum tft_function {
	TFT_MAPTBL_INIT_DEFAULT,
	TFT_MAPTBL_INIT_BRT,
	TFT_MAPTBL_GETIDX_BRT,
	TFT_MAPTBL_COPY_DEFAULT,
	MAX_TFT_FUNCTION,
};

extern struct pnobj_func tft_function_table[MAX_TFT_FUNCTION];

#define TFT_FUNC(_index) (tft_function_table[_index])

#endif /* __TFT_FUNCTION_H__ */
