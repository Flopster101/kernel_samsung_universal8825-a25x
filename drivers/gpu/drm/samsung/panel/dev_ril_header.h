/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __DEV_RIL_HEADER_H__
#define __DEV_RIL_HEADER_H__
#if !defined(CONFIG_UML)
#include <linux/dev_ril_bridge.h>
#else
#if !defined(CONFIG_USDM_SDP_ADAPTIVE_MIPI)
#include "dev_ril_bridge.h"
#endif
#endif
#endif /* __DEV_RIL_HEADER_H__ */
