// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CONFIG_H
#define IS_CONFIG_H

#include <linux/version.h>
#include "is-common-config.h"
#include "is-vendor-config.h"

/*
 * =================================================================================================
 * CONFIG - GLOBAL OPTIONS
 * =================================================================================================
 */
#define IS_SENSOR_COUNT		6
#define IS_STREAM_COUNT		10

/*
 * =================================================================================================
 * CONFIG - Chain IP configurations
 * =================================================================================================
 */
/* Sensor VC */
#define SOC_SSVC0	1
#define SOC_SSVC1	1
#define SOC_SSVC2	1
#define SOC_SSVC3	1

/* CSTAT */
#define SOC_CSTAT0		1
#define SOC_CSTAT1		1
#define SOC_CSTAT2		1
#define SOC_CSTAT3		1
#define SOC_CSTAT_SVHIST	1

/* BYRP */
#define SOC_BYRP	1

/* RGBP */
#define SOC_RGBP	1

/* MCFP */
#define SOC_MCFP	1

/* YUVP */
#define SOC_YUVP	1

/* MCSC */
#define SOC_MCS		1

/* LME */
#define SOC_LME0	1

#define CSIS_COMP_BLOCK_WIDTH		256
#define CSIS_COMP_BLOCK_HEIGHT		1

#define CSTAT_COMP_BLOCK_WIDTH		256
#define CSTAT_COMP_BLOCK_HEIGHT		1

#define BYRP_COMP_BLOCK_WIDTH		256
#define BYRP_COMP_BLOCK_HEIGHT		1

#define RGBP_COMP_BLOCK_WIDTH		32
#define RGBP_COMP_BLOCK_HEIGHT		4

#define MCFP_COMP_BLOCK_WIDTH		32
#define MCFP_COMP_BLOCK_HEIGHT		4

#define YUVP_COMP_BLOCK_WIDTH		32
#define YUVP_COMP_BLOCK_HEIGHT		4

#define MCSC_COMP_BLOCK_WIDTH		32
#define MCSC_COMP_BLOCK_HEIGHT		4
#define MCSC_HF_COMP_BLOCK_WIDTH	256
#define MCSC_HF_COMP_BLOCK_HEIGHT	1

#define COMP_LOSSYBYTE32NUM_32X4_U8	2
#define COMP_LOSSYBYTE32NUM_32X4_U10	2
#define COMP_LOSSYBYTE32NUM_32X4_U12	4
#define COMP_LOSSYBYTE32NUM_256X1_U10	5
#define COMP_LOSSYBYTE32NUM_256X1_U12	9

#define DEV_HW_ISP0_ID		DEV_HW_BYRP

/*
 * =================================================================================================
 * CONFIG - SW configurations
 * =================================================================================================
 */
#define CHAIN_STRIPE_PROCESSING	1
#define MCFP_WIDTH_ALIGN		128
#define STRIPE_MARGIN_WIDTH		768
#define STRIPE_MCSC_MARGIN_WIDTH	100
#define STRIPE_WIDTH_ALIGN		512
#define STRIPE_RATIO_PRECISION		1000
#define CHECK_STRIPE_EACH_GROUP		1
#define ENABLE_LMEDS
#define ENABLE_LMEDS1
#define ENABLE_BYRP_HDR
#define ENABLE_10BIT_MCSC
#define ENABLE_DJAG_IN_MCSC
#define ENABLE_MCSC_CENTER_CROP
#define USE_MCSC_STRIP_OUT_CROP		/* use for MCSC stripe */
#define USE_DJAG_COEFF_CONFIG		0
#define USE_MCSC_H_V_COEFF		/* support different u,v coefficient */
#define CONFIG_LME_SAD			1
#define CONFIG_LME_MBMV_INTERNAL_BUFFER	1
#define CONFIG_LME_FRAME_END_EVENT_NOTIFICATION	1
#define DDK_INTERFACE_VER		0x1010

#define USE_ONE_BINARY
#define USE_RTA_BINARY
#ifdef CONFIG_CAMERA_VENDER_MCD
#define DISABLE_DDK_HEAP_FREE	1
#else
#define DISABLE_DDK_HEAP_FREE	1
#endif
#define USE_BINARY_PADDING_DATA_ADDED	/* for DDK signature */
#define USE_DDK_SHUT_DOWN_FUNC
#define ENABLE_IRQ_MULTI_TARGET

/* #define ENABLE_MODECHANGE_CAPTURE */

#define LOGICAL_VIDEO_NODE	1
/* #define ENABLE_LVN_DUMMYOUTPUT */
/* #define ENABLE_SKIP_PER_FRAME_MAP */
#define CSTAT_DDK_LIB_CALL	1
#define LME_DDK_LIB_CALL	1
#define BYRP_DDK_LIB_CALL	1
#define RGBP_DDK_LIB_CALL	1
#define MCFP_DDK_LIB_CALL	1
#define YPP_DDK_LIB_CALL	1

#ifndef CONFIG_CAMERA_VENDER_MCD
#define CAMERA_2LD_MIRROR_FLIP	1
#endif

/*
 * PDP0: RDMA
 * PDP1: RDMA
 * PDP2: RDMA
 * 3AA0: 3AA0, ZSL/STRIP DMA0, MEIP0
 * 3AA1: 3AA1, ZSL/STRIP DMA1, MEIP1
 * 3AA2: 3AA2, ZSL/STRIP DMA2, MEIP2
 * ITP0: TNR, ITP0, DNS0
 * MCSC0:
 *
 */
#define HW_SLOT_MAX            (16)
#define valid_hw_slot_id(slot_id) \
	(slot_id >= 0 && slot_id < HW_SLOT_MAX)
/* #define SKIP_SETFILE_LOAD */
/* #define SKIP_LIB_LOAD */

#define ENABLE_HMP_BOOST

#define DYNAMIC_HEAP_FOR_DDK_RTA	1

/* FIMC-IS task priority setting */
#define TASK_SENSOR_WORK_PRIO		(IS_MAX_PRIO - 48) /* 52 */
#define TASK_GRP_OTF_INPUT_PRIO		(IS_MAX_PRIO - 49) /* 51 */
#define TASK_GRP_DMA_INPUT_PRIO		(IS_MAX_PRIO - 50) /* 50 */
#define TASK_MSHOT_WORK_PRIO		(IS_MAX_PRIO - 43) /* 57 */
#define TASK_LIB_OTF_PRIO		(IS_MAX_PRIO - 44) /* 56 */
#define TASK_LIB_AF_PRIO		(IS_MAX_PRIO - 45) /* 55 */
#define TASK_LIB_ISP_DMA_PRIO		(IS_MAX_PRIO - 46) /* 54 */
#define TASK_LIB_3AA_DMA_PRIO		(IS_MAX_PRIO - 47) /* 53 */
#define TASK_LIB_AA_PRIO		(IS_MAX_PRIO - 48) /* 52 */
#define TASK_LIB_RTA_PRIO		(IS_MAX_PRIO - 49) /* 51 */
#define TASK_LIB_VRA_PRIO		(IS_MAX_PRIO - 45) /* 55 */

/* EXTRA chain for 3AA and ITP
 * Each IP has 4 slot
 * 3AA: 0~3	ITP: 0~3
 * MEIP: 4~7	MCFP0: 4~7
 * DMA: 8~11	DNS: 8~11
 * LME: 8~11	MCFP1: 8~11
 */
#define LIC_CHAIN_NUM		(2)
#define LIC_CHAIN_OFFSET_NUM	(8)

/* 0x0: 32B_ALIGN, 0x4: 64B_ALIGN */
#define SBWC_BASE_ALIGN_MASK_LLC_OFF	(0x0)
#define SBWC_BASE_ALIGN_MASK		(0x4)

#if IS_ENABLED(LOGICAL_VIDEO_NODE) || !IS_ENABLED(SOC_VRA)
#define PARAM_FD_OTF_INPUT	0
#endif

#if IS_ENABLED(CLOG_RESERVED_MEM)
#undef CLOG_RESERVED_MEM
#endif

/*
 * =================================================================================================
 * CONFIG - FEATURE ENABLE
 * =================================================================================================
 */
#define DISABLE_CHECK_PERFRAME_FMT_SIZE
#define IS_MAX_TASK		(40)

#define HW_TIMEOUT_PANIC_ENABLE
#ifdef HW_TIMEOUT_PANIC_ENABLE
#define CHECK_TIMEOUT_PANIC_GROUP(gid)	((gid == GROUP_ID_BYRP) || \
				(gid == GROUP_ID_RGBP) || \
				(gid == GROUP_ID_MCFP) || \
				(gid == GROUP_ID_YUVP) || \
				(gid == GROUP_ID_LME0))
#endif
#if defined(CONFIG_ARM_EXYNOS_DEVFREQ)
#define CONFIG_IS_BUS_DEVFREQ
#endif

#if defined(OVERFLOW_PANIC_ENABLE_ISCHAIN)
#define DDK_OVERFLOW_RECOVERY		(0)	/* 0: do not execute recovery, 1: execute recovery */
#else
#define DDK_OVERFLOW_RECOVERY		(1)	/* 0: do not execute recovery, 1: execute recovery */
#endif

#define CAPTURE_NODE_MAX		12
#define OTF_YUV_FORMAT			(OTF_INPUT_FORMAT_YUV422)
#define MSB_OF_3AA_DMA_OUT		(11)
#define MSB_OF_DNG_DMA_OUT		(9)
/* #define USE_YUV_RANGE_BY_ISP */

/* #define ENABLE_ULTRA_FAST_SHOT */
#define ENABLE_HWFC
#define USE_I2C_LOCK
#define SENSOR_REQUEST_DELAY		2

#ifdef ENABLE_IRQ_MULTI_TARGET
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
#define IS_HW_IRQ_FLAG     0
#else
#define IS_HW_IRQ_FLAG     IRQF_GIC_MULTI_TARGET
#endif
#define IRQ_MULTI_TARGET_CL0 1
#else
#define IS_HW_IRQ_FLAG     0
#endif

/* #define MULTI_SHOT_KTHREAD */
#define MULTI_SHOT_TASKLET
/* #define ENABLE_EARLY_SHOT */
#define SUPPORT_HW_FRO(gid)	((gid >= GROUP_ID_PAF0) && \
				(gid <= GROUP_ID_PAF3))
#define FULL_OTF_TAIL_GROUP_ID		GROUP_ID_MCS0

#define SKIP_ISP_SHOT_FOR_MULTI_SHOT	1

#if defined(USE_I2C_LOCK)
#define I2C_MUTEX_LOCK(lock)	\
	({if (!in_atomic())	\
		mutex_lock(lock);})
#define I2C_MUTEX_UNLOCK(lock)	\
	({if (!in_atomic())	\
		mutex_unlock(lock);})
#else
#define I2C_MUTEX_LOCK(lock)
#define I2C_MUTEX_UNLOCK(lock)
#endif

#define ENABLE_DBG_EVENT_PRINT
/* #define ENABLE_DBG_CLK_PRINT */

#ifdef SECURE_CAMERA_IRIS
#undef SECURE_CAMERA_IRIS
#endif

#define SECURE_CAMERA_FACE		1 /* For face Detection and face authentication */
#define SECURE_CAMERA_CH		((1 << CSI_ID_C) | (1 << CSI_ID_E))
#define SECURE_CAMERA_HEAP_ID		(11)
#define SECURE_CAMERA_MEM_SHARE		1
#define SECURE_CAMERA_MEM_ADDR		(0x96000000)	/* secure_camera_heap */
#define SECURE_CAMERA_MEM_SIZE		(0x01EE0000)
#define NON_SECURE_CAMERA_MEM_ADDR	(0x0)	/* camera_heap */
#define NON_SECURE_CAMERA_MEM_SIZE	(0x0)
#define ION_EXYNOS_FLAG_PROTECTED	BIT(16)
#define ION_EXYNOS_FLAG_IOVA_EXTENSION	BIT(20)

#undef SECURE_CAMERA_FACE_SEQ_CHK	/* To check sequence before applying secure protection */

#define PM_QOS_TNR_THROUGHPUT	0

#define ENABLE_HWACG_CONTROL

#define USE_PDP_LINE_INTR_FOR_PDAF	0
#define USE_DEBUG_PD_DUMP	0
#define DEBUG_PD_DUMP_CH	0  /* set a bit corresponding to each channel */

#define FAST_FDAE

#define NFD_OBJECT_DETECT	1

/* init AWB */
#ifdef CONFIG_CAMERA_VENDER_MCD
#define INIT_AWB		0
#else
#define INIT_AWB		1
#endif
#define WB_GAIN_COUNT		(4)
#define INIT_AWB_COUNT_REAR	(3)
#define INIT_AWB_COUNT_FRONT	(8)

/* use CIS global work for enhance launching time */
#define USE_CIS_GLOBAL_WORK	1

#define USE_CAMIF_FIX_UP	0
#define CHAIN_TAG_SENSOR_IN_SOFTIRQ_CONTEXT	0
#define CHAIN_TAG_VC0_DMA_IN_HARDIRQ_CONTEXT	1

#define USE_SKIP_DUMP_LIC_OVERFLOW	1

#define CONFIG_VOTF_ONESHOT	0	/* oneshot mode is used when using VOTF in PDP input.  */
#define VOTF_BACK_FIRST_OFF	1
#define USE_VOTF_AXI_APB
#define USE_YPP_VOTF
#define USE_YPP_SHARED_INTERNAL_BUFFER		1

/* #define SDC_HEADER_GEN */

/* BTS */
/* #define DISABLE_BTS_CALC	*/ /* This is only for v8.1. next AP don't have to use this */

#define THROTTLING_MIF_ENABLE   1
#define THROTTLING_INT_ENABLE 0
#define THROTTLING_INTCAM_ENABLE 1

#define AP_PHY_LDO_ALL_SAME  	1

#define SYNC_SHOT_ALWAYS	/* This is a feature for reducing late shot. */

#define CHECK_TAD_ID(vid)	((vid == IS_VIDEO_30D_NUM) || \
				(vid == IS_VIDEO_31D_NUM) || \
				(vid == IS_VIDEO_32D_NUM) || \
				(vid == IS_VIDEO_33D_NUM))
#define CHECK_TAH_ID(vid)	((vid == IS_VIDEO_30H_NUM) || \
				(vid == IS_VIDEO_31H_NUM) || \
				(vid == IS_VIDEO_32H_NUM) || \
				(vid == IS_VIDEO_33H_NUM))

#define CHECK_NEED_KVADDR_ID(vid)	((0)					\
				|| ((vid) == IS_VIDEO_LME0C_NUM)		\
				|| ((vid) == IS_VIDEO_LME0M_NUM)		\
				|| ((vid) == IS_LVN_MCFP_MV)		\
				|| (is_get_debug_param(IS_DEBUG_PARAM_LVN) 	\
				&&(vid) == IS_VIDEO_IMM_NUM)			\
				)


#define MCSC_HF_ID	(IS_VIDEO_M5P_NUM)

#define CHECK_YPP_NOISE_RDMA_DISALBE(exynos_soc_info)	\
		(exynos_soc_info.main_rev >= 1 && exynos_soc_info.sub_rev >= 1)

#define HW_RUNNING_FPS		1

#define CLOGGING		1
/*
 * ======================================================
 * CONFIG - Interface version configs
 * ======================================================
 */

#define SETFILE_VERSION_INFO_HEADER1	(0xF85A20B4)
#define SETFILE_VERSION_INFO_HEADER2	(0xCA539ADF)

#define META_ITF_VER_20192003

#define VH_FPSIMD_API

#define USE_DDK_INTF_LIC_OFFSET
#define USE_DDK_HWIP_INTERFACE		1

#define IMAGE_RTA      1
#define USE_DDK_INTF_CAP_META		1
#endif /* #ifndef IS_CONFIG_H */
