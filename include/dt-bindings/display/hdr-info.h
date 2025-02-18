/*
 * Copyright c 2020 Samsung Electronics Co., Ltd.
 *
 * Author: Minwoo kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for panel display.
 */

#ifndef _DT_BINDINGS_HDR_INFO_H_
#define _DT_BINDINGS_HDR_INFO_H_

/*
 * Refer to exynos_drm_connector_create_hdr_formats_property()
   @exynos_drm_connector.c
 */

#define HDR_DOLBY_VISION	1
#define HDR_HDR10		2
#define HDR_HLG			3
#define HDR_HDR10_PLUS		4

#endif //_DT_BINDINGS_HDR_INFO_H_
