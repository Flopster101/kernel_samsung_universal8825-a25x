/*
 * @file hdm.h
 * @brief Header file for HDM driver
 * Copyright (c) 2019, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __HDM_H__
#define __HDM_H__
#include <linux/types.h>


#ifndef __ASSEMBLY__

#define HDM_CMD_LEN ((size_t)8)

#define HDM_P_BITMASK       0xFFFF
#define HDM_C_BITMASK       0xF0000
#define HDM_FLAG_SET        0x10000
#define HDM_FLAG_UNSET      0x20000
#define HDM_HYP_CALL        0x40000
#define HDM_HYP_INIT        0x50000
#define HDM_HYP_CLEAR       0x60000
#define HDM_HYP_CALLP       0x80000
#define HDM_CMD_MAX         0xFFFFF

#define HDM_GET_SUPPORTED_SUBSYSTEM 6

#define HDM_WIFI_SUPPORT_BIT    0x08
#define HDM_CP_SUPPORT_BIT      0x100

extern int hdm_is_wlan_enabled(void);
extern int hdm_is_cp_enabled(void);

enum {
	HDM_ALLOW = 0,
	HDM_PROTECT,
};

extern const struct file_operations hdm_fops;

#endif //__ASSEMBLY__
#endif //__HDM_H__
