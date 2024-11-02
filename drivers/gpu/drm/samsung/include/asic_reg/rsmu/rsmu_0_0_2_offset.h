/*
 * Copyright (C) 2019  Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _rsmu_0_0_2_OFFSET_HEADER
#define _rsmu_0_0_2_OFFSET_HEADER

#define	mmRSMU_UMC_INDEX_REGISTER_NBIF_VG20_GPU								0x0d91
#define	mmRSMU_UMC_INDEX_REGISTER_NBIF_VG20_GPU_BASE_IDX						0

/* RSMU registers required for the boot sequence. This is not the complete regsiter spec */
#define RSMU_AEB_LOCK_0_GC                              0x40800
#define RSMU_AEB_LOCK_1_GC                              0x40804
#define RSMU_COLD_RESETB_GC                             0x40004
#define RSMU_HARD_RESETB_GC                             0x40008
#define RSMU_CUSTOM_HARD_RESETB_GC	                0x402e0
#define RSMU_SEC_MASTER_TRUST_LEVEL_6_GC	        0x40848
#define RSMU_SEC_MISC_MASK_SET0_GROUP_DEFAULT_GC	0x40920
#define RSMU_SEC_MISC_MASK_SET1_GROUP_DEFAULT_GC	0x4092c
#define RSMU_SEC_ACCESS_CONTROL_GROUP_DEFAULT_GC	0x40930
#define RSMU_SEC_START_ADDR_GROUP_0_GC	                0x40934
#define RSMU_SEC_END_ADDR_GROUP_0_GC	                0x40938
#define RSMU_SEC_MISC_MASK_SET0_GROUP_0_GC	        0x40944
#define RSMU_SEC_MISC_MASK_SET1_GROUP_0_GC	        0x40950
#define RSMU_SEC_ACCESS_CONTROL_GROUP_0_GC      	0x40954
#define RSMU_SEC_START_ADDR_GROUP_1_GC          	0x40958
#define RSMU_SEC_END_ADDR_GROUP_1_GC	                0x4095c
#define RSMU_SEC_MISC_MASK_SET0_GROUP_1_GC	        0x40968
#define RSMU_SEC_MISC_MASK_SET1_GROUP_1_GC	        0x40974
#define RSMU_SEC_ACCESS_CONTROL_GROUP_1_GC	        0x40978
#define RSMU_SEC_START_ADDR_GROUP_2_GC	                0x4097c
#define RSMU_SEC_END_ADDR_GROUP_2_GC	                0x40980
#define RSMU_SEC_MISC_MASK_SET0_GROUP_2_GC              0x4098c
#define RSMU_SEC_MISC_MASK_SET1_GROUP_2_GC              0x40998
#define RSMU_SEC_ACCESS_CONTROL_GROUP_2_GC              0x4099c
#define RSMU_SEC_START_ADDR_GROUP_3_GC	                0x409a0
#define RSMU_SEC_END_ADDR_GROUP_3_GC	                0x409a4
#define RSMU_SEC_MISC_MASK_SET0_GROUP_3_GC	        0x409b0
#define RSMU_SEC_MISC_MASK_SET1_GROUP_3_GC	        0x409bc
#define RSMU_SEC_ACCESS_CONTROL_GROUP_3_GC       	0x409c0
#define RSMU_SEC_START_ADDR_GROUP_4_GC	                0x409c4
#define RSMU_SEC_END_ADDR_GROUP_4_GC            	0x409c8
#define RSMU_SEC_MISC_MASK_SET0_GROUP_4_GC              0x409d4
#define RSMU_SEC_MISC_MASK_SET1_GROUP_4_GC              0x409e0
#define RSMU_SEC_ACCESS_CONTROL_GROUP_4_GC              0x409e4
#define RSMU_SEC_MASTER_TRUST_LEVEL_RSMU_GC	        0x40910
#define RSMU_SEC_ACCESS_CONTROL_RSMU_GC	                0x40914
#define RSMU_SEC_SLAVE_RANGE_ENABLE_GC	                0x40db8

#endif
