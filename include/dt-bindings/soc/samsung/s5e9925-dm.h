/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for S5E9925
 */

#ifndef _DT_BINDINGS_S5E9925_DM_H
#define _DT_BINDINGS_S5E9925_DM_H

/* NUMBER FOR DVFS MANAGER */
#define DM_CPU_CL0	0
#define DM_CPU_CL1	1
#define DM_CPU_CL2	2
#define DM_MIF		3
#define DM_INT		4
#define DM_NPU		5
#define DM_DSU		6
#define DM_DISP		7
#define DM_AUD		8
#define DM_GPU		9
#define DM_INTCAM	10
#define DM_CAM		11
#define DM_CSIS		12
#define DM_ISP		13
#define DM_MFC0		14
#define DM_MFC1		15
#define DM_DSP		16
#define DM_DNC		17
#define DM_GNSS		18
#define DM_ALIVE	19
#define DM_CHUB		20
#define DM_VTS		21
#define DM_HSI0		22
#define DM_UFD		23

/* CONSTRAINT TYPE */
#define CONSTRAINT_MIN	0
#define CONSTRAINT_MAX	1

#endif	/* _DT_BINDINGS_S5E9925_DM_H */
