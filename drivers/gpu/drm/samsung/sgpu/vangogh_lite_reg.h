/*
***************************************************************************
*
*  Copyright (C) 2020 Advanced Micro Devices, Inc.  All rights reserved.
*
***************************************************************************/

#include "amdgpu.h"

#ifndef __AMDGPU_VANGOGH_REG_H__
#define __AMDGPU_VANGOGH_REG_H__

#define BG3D_PWRCTL_CTL1_OFFSET 	0x0
#define BG3D_PWRCTL_CTL2_OFFSET 	0x4
#define BG3D_PWRCTL_STATUS_OFFSET 	0x8
#define BG3D_PWRCTL_LOCK_OFFSET		0xc

#define BG3D_PWRCTL_STATUS_GPU_READY_MASK 		0x80000000L
#define BG3D_PWRCTL_LOCK_GPU_RESET_MASK 		0x00000001L
#define BG3D_PWRCTL_LOCK_GPU_READY_MASK 		0x80000000L
#define BG3D_PWRCTL_LOCK_GPUPWRREQ_VALUE_MASK	0x40000000L

#define RSMU_HARD_RESETB_GC_OFFSET	0x40008
#define GL2ACEM_RST_OFFSET			0x3f100
#define SMC_SAFE_WAIT_US			1000
#define QUIESCE_TIMEOUT_WAIT_US		20000
#define ACEM_INSTANCES_COUNT		4

#endif
