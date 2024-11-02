/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS2100 - NPU
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DT_BINDINGS_EXYNOS2100_NPU_H__
#define __DT_BINDINGS_EXYNOS2100_NPU_H__

#define	NPU_HWDEV_ID_MISC	0x0
#define	NPU_HWDEV_ID_DNC	0x1
#define	NPU_HWDEV_ID_NPU	0x2
#define	NPU_HWDEV_ID_DSP	0x4
#define	NPU_HWDEV_ID_MIF	0x8
#define	NPU_HWDEV_ID_INT	0x10
#define	NPU_HWDEV_ID_CL0	0x20
#define	NPU_HWDEV_ID_CL1	0x40

#define NPU_HWDEV_TYPE_PWRCTRL	0x1
#define NPU_HWDEV_TYPE_CLKCTRL	0x2
#define NPU_HWDEV_TYPE_DVFS	0x4
#define NPU_HWDEV_TYPE_BTS	0x8

#endif /* _DT_BINDINGS_EXYNOS2100_NPU_H */
