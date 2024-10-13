/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Google LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 */

#ifndef __EXYNOS_DISPLAY_COMMON_H__
#define __EXYNOS_DISPLAY_COMMON_H__

/* Structures and definitions shared across exynos display pipeline. */

#ifndef KHZ
#define KHZ (1000)
#endif
#ifndef MHZ
#define MHZ (1000*1000)
#endif
#ifndef MSEC
#define MSEC (1000)
#endif

#define EXYNOS_DISPLAY_MODE_FLAG_TUI		(1 << 1)

#endif
