/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for S5E8825 devfreq
 */

#ifndef _DT_BINDINGS_EXYNOS_8825_DEVFREQ_H
#define _DT_BINDINGS_EXYNOS_8825_DEVFREQ_H

/* DEVFREQ TYPE LIST */
#define DEVFREQ_MIF		0
#define DEVFREQ_INT		1
#define DEVFREQ_NPU		2
#define DEVFREQ_DISP		3
#define DEVFREQ_AUD		4
#define DEVFREQ_CAM		5
#define DEVFREQ_ISP		6
#define DEVFREQ_MFC		7
#define DEVFREQ_TYPE_END	9

/* DEVFREQ GOV TYPE */
#define SIMPLE_INTERACTIVE	0

#define ESS_FLAG_CPU_CL0	0
#define ESS_FLAG_CPU_CL1	1
#define ESS_FLAG_CPU_CL2	2
#define ESS_FLAG_INT		3
#define ESS_FLAG_MIF		4
#define ESS_FLAG_CAM		5
#define ESS_FLAG_DISP		6
#define ESS_FLAG_INTCAM		7
#define ESS_FLAG_AUD		8
#define ESS_FLAG_MFC		9
#define ESS_FLAG_NPU		10
#define ESS_FLAG_DSU		11
#define ESS_FLAG_CSIS		13
#define ESS_FLAG_ISP		14

#endif
