/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos Performance (XPerf)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_PERF_H
#define EXYNOS_PERF_H __FILE__

/* GMC */
enum gmc_type {
	GMC_TYPE0 = 0,
	GMC_TYPE1,
	NUM_OF_GMC_TYPE,
};

#if IS_ENABLED(CONFIG_EXYNOS_PERF)
extern void exynos_gmc_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time));
#else
static inline void exynos_gmc_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time)) { }
#endif

#endif /* EXYNOS_PERF_H */
