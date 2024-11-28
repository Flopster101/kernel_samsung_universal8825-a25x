/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83121 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_I2C)
#include "himax_platform.h"
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_HIMAX_SPI)
#include "himax_platform_SPI.h"
#endif

#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>

#define hx83121a_fw_addr_raw_out_sel			0x100072EC
//#define hx83121a_ic_adr_tcon_rst	0x80020004
#define hx83121a_data_df_rx		60
#define hx83121a_data_df_tx		40
//#define hx83121a_data_df_x_res		1600
//#define hx83121a_data_df_y_res		2560

#define HX83121A_FLASH_SIZE 261120

#ifdef HX_ESD_RECOVERY
	extern u8 HX_ESD_RESET_ACTIVATE;
#endif

