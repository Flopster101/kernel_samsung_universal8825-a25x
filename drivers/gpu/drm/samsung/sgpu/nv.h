/*
 * Copyright 2019-2021 Advanced Micro Devices, Inc.
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

#ifndef __NV_H__
#define __NV_H__

#include "nbio_v2_3.h"

void nv_grbm_select(struct amdgpu_device *adev,
		    u32 me, u32 pipe, u32 queue, u32 vmid);
void nv_set_virt_ops(struct amdgpu_device *adev);
int nv_set_ip_blocks(struct amdgpu_device *adev);
int navi10_reg_base_init(struct amdgpu_device *adev);
int navi14_reg_base_init(struct amdgpu_device *adev);
int navi12_reg_base_init(struct amdgpu_device *adev);
int sienna_cichlid_reg_base_init(struct amdgpu_device *adev);
int vangogh_lite_reg_base_init(struct amdgpu_device *adev);
void vangogh_lite_gc_update_median_grain_clock_gating(struct amdgpu_device *adev,
						      bool enable);
void vangogh_lite_gpu_reset(struct amdgpu_device *adev);
void navi1x_soft_reset_SQG_workaround(struct amdgpu_device *adev);

void vangogh_lite_gc_clock_gating_workaround(struct amdgpu_device *adev);
void vangogh_lite_gc_perfcounter_cg_workaround(struct amdgpu_device *adev,
					        bool enable);
void vangogh_lite_gpu_hard_reset(struct amdgpu_device *adev);
void vangogh_lite_gpu_soft_reset(struct amdgpu_device *adev);
void vangogh_lite_gpu_quiesce(struct amdgpu_device *adev);
#endif
