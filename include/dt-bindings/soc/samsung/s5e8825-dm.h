/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for S5E9925
 */

#ifndef _DT_BINDINGS_S5E8825_DM_H
#define _DT_BINDINGS_S5E8825_DM_H

/* NUMBER FOR DVFS MANAGER */
#define DM_MIF		0
#define DM_INT		1
#define DM_CPUCL0	2
#define DM_CPUCL1	3
#define DM_DSU		4
#define DM_NPU		5
#define DM_DISP		6
#define DM_AUD		7
#define DM_G3D		8
#define DM_CP_CPU	9
#define DM_CP		10
#define DM_CAM		11
#define DM_ISP		12
#define DM_MFC		13
#define DM_INTSCI	14
#define DM_INTG3D	15
#define DM_GNSS		16
#define DM_WLBT		17
#define DM_ALIVE	18
#define DM_CHUB		19

/* CONSTRAINT TYPE */
#define CONSTRAINT_MIN	0
#define CONSTRAINT_MAX	1

#endif	/* _DT_BINDINGS_S5E8825_DM_H */
