/*
 * Copyright 2020-2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __AMDGPU_SWS_H__
#define __AMDGPU_SWS_H__

struct amdgpu_device;
struct amdgpu_ring;
struct amdgpu_ctx;

//100ms
#define AMDGPU_SWS_TIMER 100

//n * 100ms
#define AMDGPU_SWS_HIGH_ROUND 3
#define AMDGPU_SWS_NORMAL_ROUND 2
#define AMDGPU_SWS_LOW_ROUND 1

//max reserved VMIDs for SWS
#define AMDGPU_SWS_MAX_VMID_NUM 8
#define AMDGPU_SWS_VMID_BEGIN 8

#define AMDGPU_SWS_NO_INV_ENG 0xffff
#define AMDGPU_SWS_NO_VMID 0

#define AMDGPU_SWS_MAX_RUNNING_QUEUE (AMDGPU_SWS_MAX_VMID_NUM)

#define AMDGPU_SWS_HW_RES_AVAILABLE 0
#define AMDGPU_SWS_HW_RES_BUSY 1

#define AMDGPU_SWS_DEBUGFS_SIZE 4096

//queue state
#define AMDGPU_SWS_QUEUE_DISABLED 0
#define AMDGPU_SWS_QUEUE_ENABLED 1
#define AMDGPU_SWS_QUEUE_DEQUEUED 2

//APIs for CWSR
int amdgpu_sws_early_init_ctx(struct amdgpu_ctx *ctx);
void amdgpu_sws_late_deinit_ctx(struct amdgpu_ctx *ctx);

int amdgpu_sws_init_ctx(struct amdgpu_ctx *ctx, struct amdgpu_fpriv *fpriv);
void amdgpu_sws_deinit_ctx(struct amdgpu_ctx *ctx,
			   struct amdgpu_fpriv *fpriv);

//API for init and deinit SWS
int amdgpu_sws_init(struct amdgpu_device *adev);
void amdgpu_sws_deinit(struct amdgpu_device *adev);

int amdgpu_sws_init_debugfs(struct amdgpu_device *adev);
int amdgpu_sws_deinit_debugfs(struct amdgpu_device *adev);

//clear broken queue after gpu reset
void amdgpu_sws_clear_broken_queue(struct amdgpu_device *adev);

#endif
