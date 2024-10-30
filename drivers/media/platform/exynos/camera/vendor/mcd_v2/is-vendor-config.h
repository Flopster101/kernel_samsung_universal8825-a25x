/*
* Samsung Exynos5 SoC series IS driver
 *
 * exynos5 is vender functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_CONFIG_H
#define IS_VENDOR_CONFIG_H

#define USE_BINARY_PADDING_DATA_ADDED             /* Apply Sign DDK/RTA Binary */

#if defined(USE_BINARY_PADDING_DATA_ADDED) && (defined(CONFIG_USE_SIGNED_BINARY) || defined(CONFIG_SAMSUNG_PRODUCT_SHIP))
#define TZ_CONTROLLED_MEM_ATTRIBUTE 1
#else
#define TZ_CONTROLLED_MEM_ATTRIBUTE 0
#endif

#define USE_AF_SLEEP_MODE

#define IS_MAX_FW_BUFFER_SIZE (4100 * 1024)
#define IS_MAX_CAL_SIZE (64 * 1024)

#if defined(CONFIG_CAMERA_AAV_A53X)
#include "aav_v53x/is-vendor-config_aav_v53x.h"
#elif defined(CONFIG_CAMERA_AAW_A25X)
#include "aaw_v25x/is-vendor-config_aaw_v25x.h"
#elif defined(CONFIG_CAMERA_AAV_A33X)
#include "aav_v33x/is-vendor-config_aav_v33x.h"
#elif defined(CONFIG_CAMERA_MMV_M33X)
#include "mmv_v33x/is-vendor-config_mmv_v33x.h"
#elif defined(CONFIG_CAMERA_MMW_M34X)
#include "mmw_v34x/is-vendor-config_mmw_v34x.h"
#elif defined(CONFIG_CAMERA_STX_V04)
#include "stx_v04/is-vendor-config_stx_v04.h"
#else
#include "aav_v33x/is-vendor-config_aav_v33x.h"
#endif

#endif
