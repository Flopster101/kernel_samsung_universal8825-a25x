/*
 * Copyright 2020 Advanced Micro Devices, Inc.
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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pci.h>
#include "amdgpu.h"
#include "amdgpu_cwsr.h"
#include "amdgpu_vm.h"
#include "amdgpu_gmc.h"
#include "amdgpu_ring.h"
#include "cwsr_trap_handler.h"

static int amdgpu_cwsr_static_map(struct amdgpu_device *adev,
				  struct amdgpu_vm *vm,
				  struct amdgpu_bo *bo,
				  struct amdgpu_bo_va **bo_va,
				  u64 addr,
				  u32 size)
{
	struct ww_acquire_ctx ticket;
	struct list_head list;
	struct amdgpu_bo_list_entry pd;
	struct ttm_validate_buffer tv;
	u64 pte_flag = 0;
	int r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&tv.head);
	tv.bo = &bo->tbo;
	tv.num_shared = 1;

	list_add(&tv.head, &list);
	amdgpu_vm_get_pd_bo(vm, &list, &pd);

	DRM_DEBUG_DRIVER("map addr 0x%llx, size 0x%x\n", addr, size);

	r = ttm_eu_reserve_buffers(&ticket, &list, true, NULL);
	if (r) {
		DRM_ERROR("failed to reserve cwsr BOs: err=%d\n", r);
		return r;
	}

	*bo_va = amdgpu_vm_bo_add(adev, vm, bo);
	if (!*bo_va) {
		r = -ENOMEM;
		DRM_ERROR("failed to create va for static cwsr map\n");
		goto err1;
	}

	r = amdgpu_vm_clear_freed(adev, vm, NULL);
	if (r) {
		DRM_ERROR("failed to clear bo table, err=%d\n", r);
		goto err1;
	}

	//make sure size is PAGE_SIZE aligned
	size = round_up(size, 1 << 12);
	pte_flag = AMDGPU_PTE_READABLE | AMDGPU_PTE_WRITEABLE |
		AMDGPU_PTE_EXECUTABLE |
		amdgpu_gmc_map_mtype(adev, AMDGPU_VM_MTYPE_UC);
	r = amdgpu_vm_bo_map(adev, *bo_va, addr, 0, size, pte_flag);
	if (r) {
		DRM_ERROR("failed to do cwsr bo map, err=%d\n", r);
		goto err2;
	}

	r = amdgpu_vm_bo_update(adev, *bo_va, false);
	if (r) {
		DRM_ERROR("failed to update cwsr bo table, err=%d\n", r);
		goto err3;
	}

	if ((*bo_va)->last_pt_update) {
		r = dma_fence_wait((*bo_va)->last_pt_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			goto err3;
		}
	}

	r = amdgpu_vm_update_pdes(adev, vm, false);
	if (r) {
		DRM_ERROR("failed to update pde, err=%d\n", r);
		goto err3;
	}

	if (vm->last_update) {
		r = dma_fence_wait(vm->last_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			goto err3;
		}
	}

	ttm_eu_backoff_reservation(&ticket, &list);

	return 0;

err3:
	amdgpu_vm_bo_unmap(adev, *bo_va, addr);
err2:
	amdgpu_vm_bo_rmv(adev, *bo_va);
err1:
	ttm_eu_backoff_reservation(&ticket, &list);
	return r;
}

static int amdgpu_cwsr_static_unmap(struct amdgpu_device *adev,
				    struct amdgpu_vm *vm,
				    struct amdgpu_bo *bo,
				    struct amdgpu_bo_va *bo_va,
				    uint64_t addr)
{
	struct list_head list;
	struct amdgpu_bo_list_entry pd;
	struct ttm_validate_buffer tv;
	struct ww_acquire_ctx ticket;
	int r;

	INIT_LIST_HEAD(&list);
	INIT_LIST_HEAD(&tv.head);
	tv.bo = &bo->tbo;
	tv.num_shared = 1;

	list_add(&tv.head, &list);
	amdgpu_vm_get_pd_bo(vm, &list, &pd);

	DRM_DEBUG_DRIVER("unmap addr 0x%llx\n", addr);
	r = ttm_eu_reserve_buffers(&ticket, &list, true, NULL);
	if (r) {
		DRM_ERROR("failed to reserve cwsr BOs: err=%d\n", r);
		return r;
	}

	r = amdgpu_vm_bo_unmap(adev, bo_va, addr);
	if (r) {
		DRM_ERROR("failed to unmap cwsr BOs: err=%d\n", r);
		return r;
	}

	amdgpu_vm_clear_freed(adev, vm, &bo_va->last_pt_update);
	if (bo_va->last_pt_update) {
		r = dma_fence_wait(bo_va->last_pt_update, true);
		if (r) {
			DRM_ERROR("failed to get pde clear fence, err=%d\n",
				  r);
		}
	}

	r = amdgpu_vm_update_pdes(adev, vm, false);
	if (r) {
		DRM_ERROR("failed to update pde, err=%d\n", r);
		return r;
	}

	if (vm->last_update) {
		r = dma_fence_wait(vm->last_update, true);
		if (r) {
			DRM_ERROR("failed to get pde update fence, err=%d\n",
				  r);
			return r;
		}
	}

	amdgpu_vm_bo_rmv(adev, bo_va);

	ttm_eu_backoff_reservation(&ticket, &list);

	return r;
}

void amdgpu_cwsr_wb_free(struct amdgpu_fpriv *fpriv, u32 wb)
{
	wb >>= 3;
	if (wb < fpriv->cwsr_wb.num_wb)
		__clear_bit(wb, fpriv->cwsr_wb.used);
}

int amdgpu_cwsr_wb_get(struct amdgpu_fpriv *fpriv, u32 *wb)
{
	unsigned long offset;

	offset = find_first_zero_bit(fpriv->cwsr_wb.used,
				     fpriv->cwsr_wb.num_wb);

	if (offset < fpriv->cwsr_wb.num_wb) {
		__set_bit(offset, fpriv->cwsr_wb.used);
		*wb = offset << 3; /* convert to dw offset */

		return 0;
	}

	return -EINVAL;
}

static int amdgpu_cwsr_init_wb(struct amdgpu_device *adev,
			       struct amdgpu_fpriv *fpriv)
{
	int r;
	u32 wb_size;

	if (fpriv->cwsr_wb.wb_obj)
		return 0;

	/* AMDGPU_MAX_WB * sizeof(uint32_t) * 8
	 * = AMDGPU_MAX_WB 256bit slots
	 */
	wb_size = AMDGPU_MAX_WB * sizeof(uint32_t) * 8;
	r = amdgpu_bo_create_kernel(adev, wb_size,
				    PAGE_SIZE, AMDGPU_GEM_DOMAIN_GTT,
				    &fpriv->cwsr_wb.wb_obj,
				    NULL,
				    (void **)&fpriv->cwsr_wb.wb);
	if (r) {
		DRM_ERROR("create WB bo failed(%d)\n", r);
		return r;
	}

	fpriv->cwsr_wb.num_wb = AMDGPU_MAX_WB;
	memset(&fpriv->cwsr_wb.used, 0, sizeof(fpriv->cwsr_wb.used));

	/* clear wb memory */
	memset((char *)fpriv->cwsr_wb.wb, 0, wb_size);

	r = amdgpu_cwsr_static_map(adev, &fpriv->vm,
				   fpriv->cwsr_wb.wb_obj,
				   &fpriv->cwsr_wb_va,
				   AMDGPU_CWSR_WB_OFFSET,
				   wb_size);

	if (r) {
		DRM_ERROR("map cwsr wb failed(%d)\n", r);
		amdgpu_bo_free_kernel(&fpriv->cwsr_wb.wb_obj,
				      &fpriv->cwsr_wb.gpu_addr,
				      (void **)&fpriv->cwsr_wb.wb);
		return r;
	}

	fpriv->cwsr_wb.gpu_addr = AMDGPU_CWSR_WB_OFFSET;

	return 0;
}

static void amdgpu_cwsr_deinit_wb(struct amdgpu_device *adev,
				  struct amdgpu_fpriv *fpriv)
{
	if (fpriv->cwsr_wb.wb_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 &fpriv->vm,
					 fpriv->cwsr_wb.wb_obj,
					 fpriv->cwsr_wb_va,
					 fpriv->cwsr_wb.gpu_addr);

		fpriv->cwsr_wb_va = NULL;

		amdgpu_bo_free_kernel(&fpriv->cwsr_wb.wb_obj,
				      &fpriv->cwsr_wb.gpu_addr,
				      (void **)&fpriv->cwsr_wb.wb);
	}
}

static int amdgpu_cwsr_init_hqd_eop(struct amdgpu_device *adev,
				    struct amdgpu_fpriv *fpriv)
{
	int r;
	size_t mec_hqd_size;

	if (fpriv->cwsr_hqd_eop_obj)
		return 0;

	mec_hqd_size = AMDGPU_CWSR_MEC_HQD_EOP_SIZE * 8;

	r = amdgpu_bo_create_kernel(adev, mec_hqd_size, PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &fpriv->cwsr_hqd_eop_obj,
				    NULL, &fpriv->cwsr_hqd_cpu_addr);
	if (r)
		return r;

	memset(fpriv->cwsr_hqd_cpu_addr, 0, mec_hqd_size);

	r = amdgpu_cwsr_static_map(adev, &fpriv->vm,
				   fpriv->cwsr_hqd_eop_obj,
				   &fpriv->cwsr_hqd_eop_va,
				   AMDGPU_CWSR_HQD_EOP_OFFSET,
				   mec_hqd_size);
	if (r) {
		DRM_ERROR("map cwsr hqd failed(%d)\n", r);
		amdgpu_bo_free_kernel(&fpriv->cwsr_hqd_eop_obj, NULL, NULL);
	}

	return r;
}

static void amdgpu_cwsr_deinit_hqd_eop(struct amdgpu_device *adev,
				       struct amdgpu_fpriv *fpriv)
{
	if (fpriv->cwsr_hqd_eop_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 &fpriv->vm,
					 fpriv->cwsr_hqd_eop_obj,
					 fpriv->cwsr_hqd_eop_va,
					 AMDGPU_CWSR_HQD_EOP_OFFSET);

		fpriv->cwsr_hqd_eop_va = NULL;

		amdgpu_bo_free_kernel(&fpriv->cwsr_hqd_eop_obj, NULL,
				      &fpriv->cwsr_hqd_cpu_addr);
	}
}

static int amdgpu_cwsr_init_mqd(struct amdgpu_device *adev,
				struct amdgpu_fpriv *fpriv,
				struct amdgpu_ring *ring)
{
	int r;

	if (ring->mqd_obj)
		return 0;

	r = amdgpu_bo_create_kernel(adev,
				    AMDGPU_CWSR_MQD_SIZE,
				    PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &ring->mqd_obj,
				    NULL, &ring->mqd_ptr);
	if (r) {
		DRM_ERROR("failed to create ring mqd bo (%d)", r);
		return r;
	}

	memset(ring->mqd_ptr, 0, AMDGPU_CWSR_MQD_SIZE);

	r = amdgpu_cwsr_static_map(adev, &fpriv->vm,
				   ring->mqd_obj,
				   &ring->cwsr_mqd_va,
				   AMDGPU_CWSR_MQD_OFFSET +
				   ring->cwsr_slot_idx *
				   AMDGPU_CWSR_MQD_SIZE,
				   AMDGPU_CWSR_MQD_SIZE);
	if (r) {
		DRM_ERROR("failed to map cwsr ring mqd bo (%d)", r);
		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				      &ring->mqd_ptr);
	}

	return r;
}

static void amdgpu_cwsr_deinit_mqd(struct amdgpu_device *adev,
				   struct amdgpu_fpriv *fpriv,
				   struct amdgpu_ring *ring)
{
	if (ring->mqd_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 &fpriv->vm,
					 ring->mqd_obj,
					 ring->cwsr_mqd_va,
					 AMDGPU_CWSR_MQD_OFFSET +
					 ring->cwsr_slot_idx *
					 AMDGPU_CWSR_MQD_SIZE);

		ring->cwsr_mqd_va = NULL;

		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				      &ring->mqd_ptr);
	}
}

static int amdgpu_cwsr_init_ring_bo(struct amdgpu_device *adev,
				    struct amdgpu_fpriv *fpriv,
				    struct amdgpu_ring *ring)
{
	int r;

	if (ring->ring_obj)
		return 0;

	r = amdgpu_bo_create_kernel(adev,
				    AMDGPU_CWSR_RING_BUF_SIZE,
				    PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_GTT,
				    &ring->ring_obj,
				    NULL,
				    (void **)&ring->ring);
	if (r) {
		DRM_ERROR("ring create failed(%d)\n", r);
		return r;
	}

	r = amdgpu_cwsr_static_map(adev, &fpriv->vm,
				   ring->ring_obj,
				   &ring->cwsr_ring_va,
				   AMDGPU_CWSR_RING_BUF_OFFSET +
				   ring->cwsr_slot_idx *
				   AMDGPU_CWSR_RING_BUF_SIZE,
				   AMDGPU_CWSR_RING_BUF_SIZE);
	if (r) {
		dev_err(adev->dev, "map cwsr ring bo failed(%d)\n", r);
		amdgpu_bo_free_kernel(&ring->mqd_obj,
				      NULL,
				     (void **)&ring->ring);
	}

	return r;
}

static void amdgpu_cwsr_deinit_ring_bo(struct amdgpu_device *adev,
				       struct amdgpu_fpriv *fpriv,
				       struct amdgpu_ring *ring)
{
	if (ring->ring_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 &fpriv->vm,
					 ring->ring_obj,
					 ring->cwsr_ring_va,
					 AMDGPU_CWSR_RING_BUF_OFFSET +
					 ring->cwsr_slot_idx *
					 AMDGPU_CWSR_RING_BUF_SIZE);

		ring->cwsr_ring_va = NULL;

		amdgpu_bo_free_kernel(&ring->ring_obj,
				      NULL,
				      (void **)&ring->ring);
	}
}

static int amdgpu_cwsr_init_ring_wb(struct amdgpu_device *adev,
				    struct amdgpu_ring *ring,
				    struct amdgpu_fpriv *fpriv)
{
	int r;

	r = amdgpu_cwsr_wb_get(fpriv, &ring->rptr_offs);
	if (r) {
		DRM_ERROR("(%d) failed to get rptr_offs\n", r);
		return r;
	}

	ring->cwsr_rptr_gpu_addr =
		fpriv->cwsr_wb.gpu_addr + (ring->rptr_offs * 4);
	ring->cwsr_rptr_cpu_addr = &fpriv->cwsr_wb.wb[ring->rptr_offs];
	*ring->cwsr_rptr_cpu_addr = 0;

	r = amdgpu_cwsr_wb_get(fpriv, &ring->wptr_offs);
	if (r) {
		DRM_ERROR("(%d) ring wptr_offs wb alloc failed\n", r);
		goto err1;
	}

	ring->cwsr_wptr_gpu_addr =
		fpriv->cwsr_wb.gpu_addr + (ring->wptr_offs * 4);
	ring->cwsr_wptr_cpu_addr = &fpriv->cwsr_wb.wb[ring->wptr_offs];
	*ring->cwsr_wptr_cpu_addr = 0;

	r = amdgpu_cwsr_wb_get(fpriv, &ring->fence_offs);
	if (r) {
		dev_err(adev->dev, "(%d) ring fence_offs wb alloc failed\n", r);
		goto err2;
	}
	ring->cwsr_fence_gpu_addr =
		fpriv->cwsr_wb.gpu_addr + (ring->fence_offs * 4);
	ring->cwsr_fence_cpu_addr = &fpriv->cwsr_wb.wb[ring->fence_offs];
	*ring->cwsr_fence_cpu_addr = 0;

	r = amdgpu_cwsr_wb_get(fpriv, &ring->trail_fence_offs);
	if (r) {
		dev_err(adev->dev,
			"(%d) ring trail_fence_offs wb alloc failed\n", r);
		goto err3;
	}
	ring->trail_fence_gpu_addr =
		fpriv->cwsr_wb.gpu_addr + (ring->trail_fence_offs * 4);
	ring->trail_fence_cpu_addr = &fpriv->cwsr_wb.wb[ring->trail_fence_offs];
	*ring->trail_fence_cpu_addr = 0;

	r = amdgpu_cwsr_wb_get(fpriv, &ring->cond_exe_offs);
	if (r) {
		dev_err(adev->dev, "(%d) ring cond_exec_offs wb alloc failed\n",
			r);
		goto err4;
	}
	ring->cond_exe_gpu_addr = fpriv->cwsr_wb.gpu_addr
				  + (ring->cond_exe_offs * 4);
	ring->cond_exe_cpu_addr = &fpriv->cwsr_wb.wb[ring->cond_exe_offs];
	*ring->cond_exe_cpu_addr = 1;

	return 0;

err4:
	amdgpu_cwsr_wb_free(fpriv, ring->trail_fence_offs);
err3:
	amdgpu_cwsr_wb_free(fpriv, ring->fence_offs);
err2:
	amdgpu_cwsr_wb_free(fpriv, ring->wptr_offs);
err1:
	amdgpu_cwsr_wb_free(fpriv, ring->rptr_offs);

	return r;
}

static void amdgpu_cwsr_deinit_ring_wb(struct amdgpu_ring *ring,
				       struct amdgpu_fpriv *fpriv)
{
	amdgpu_cwsr_wb_free(fpriv, ring->cond_exe_offs);
	amdgpu_cwsr_wb_free(fpriv, ring->trail_fence_offs);
	amdgpu_cwsr_wb_free(fpriv, ring->fence_offs);
	amdgpu_cwsr_wb_free(fpriv, ring->wptr_offs);
	amdgpu_cwsr_wb_free(fpriv, ring->rptr_offs);
}

static int amdgpu_cwsr_init_ring(struct amdgpu_device *adev,
				 struct amdgpu_ring *ring,
				 struct amdgpu_ctx *ctx,
				 struct amdgpu_fpriv *fpriv)
{
	int r;

	if (ring->cwsr)
		return 0;

	ring->cwsr = true;

	ring->ring_obj = NULL;
	ring->use_doorbell = true;
	ring->use_pollfence = amdgpu_poll_eop;

	ring->eop_gpu_addr = AMDGPU_CWSR_HQD_EOP_OFFSET +
		ctx->cwsr_slot_idx * AMDGPU_CWSR_MEC_HQD_EOP_SIZE;
	memset(fpriv->cwsr_hqd_cpu_addr +
	       ctx->cwsr_slot_idx * AMDGPU_CWSR_MEC_HQD_EOP_SIZE,
	       0, AMDGPU_CWSR_MEC_HQD_EOP_SIZE);

	ring->adev = adev;

	ring->cwsr_slot_idx = ctx->cwsr_slot_idx;

	r = amdgpu_cwsr_init_ring_wb(adev, ring, fpriv);
	if (r) {
		DRM_ERROR("(%d) failed to init cwsr wb\n", r);
		goto err1;
	}

	// get ring buffer object
	ring->ring_size = AMDGPU_CWSR_RING_BUF_SIZE;
	ring->buf_mask = (ring->ring_size / 4) - 1;
	ring->ptr_mask = ring->funcs->support_64bit_ptrs ?
			0xffffffffffffffff : ring->buf_mask;
	r = amdgpu_cwsr_init_ring_bo(adev, fpriv, ring);
	if (r) {
		DRM_ERROR("failed to get ring buffer object(%d).\n", r);
		goto err2;
	}
	amdgpu_ring_clear_ring(ring);

	ring->gpu_addr = AMDGPU_CWSR_RING_BUF_OFFSET +
			 ring->cwsr_slot_idx *
			 AMDGPU_CWSR_RING_BUF_SIZE;

	ring->max_dw = AMDGPU_CWSR_RING_MAX_DW;
	ring->priority = DRM_SCHED_PRIORITY_NORMAL;
	mutex_init(&ring->priority_mutex);

	ring->cwsr_tba_gpu_addr = AMDGPU_CWSR_TBA_OFFSET;
	ring->cwsr_tma_gpu_addr = AMDGPU_CWSR_TMA_OFFSET;

	return 0;

err2:
	amdgpu_cwsr_deinit_ring_wb(ring, fpriv);
err1:
	ring->cwsr = false;
	ring->adev = NULL;

	return r;
}

static void amdgpu_cwsr_deinit_ring(struct amdgpu_ring *ring,
				    struct amdgpu_fpriv *fpriv)
{
	struct amdgpu_device *adev;

	if (!ring->cwsr)
		return;

	adev = ring->adev;
	/* Don't deinit a ring which is not initialized */
	if (!adev)
		return;

	amdgpu_cwsr_deinit_ring_bo(adev, fpriv, ring);

	amdgpu_cwsr_deinit_ring_wb(ring, fpriv);

	ring->vmid_wait = NULL;
	ring->me = 0;
	ring->cwsr = false;
	ring->adev = NULL;
}

static int amdgpu_cwsr_init_trap(struct amdgpu_device *adev,
				 struct amdgpu_fpriv *fpriv)
{
	int r;

	if (fpriv->cwsr_trap_obj)
		return 0;

	r = amdgpu_bo_create_kernel(adev, AMDGPU_CWSR_TBA_TMA_SIZE,
				    PAGE_SIZE, AMDGPU_GEM_DOMAIN_GTT,
				    &fpriv->cwsr_trap_obj,
				    NULL,
				    &fpriv->cwsr_trap_cpu_addr);
	if (r) {
		DRM_ERROR("(%d) failed to create cwsr trap bo\n", r);
		return r;
	}

	/* clear memory */
	memset((char *)fpriv->cwsr_trap_cpu_addr, 0, AMDGPU_CWSR_TBA_TMA_SIZE);

	if (adev->asic_type == CHIP_VANGOGH_LITE)
		memcpy(fpriv->cwsr_trap_cpu_addr,
		       cwsr_trap_m0_hex, sizeof(cwsr_trap_m0_hex));
	else
		memcpy(fpriv->cwsr_trap_cpu_addr,
		       cwsr_trap_nv14_hex, sizeof(cwsr_trap_nv14_hex));

	r = amdgpu_cwsr_static_map(adev, &fpriv->vm,
				   fpriv->cwsr_trap_obj,
				   &fpriv->cwsr_trap_va,
				   AMDGPU_CWSR_TBA_OFFSET,
				   AMDGPU_CWSR_TBA_TMA_SIZE);
	if (r) {
		DRM_ERROR("map cwsr trap failed(%d)\n", r);
		amdgpu_bo_free_kernel(&fpriv->cwsr_trap_obj,
				      NULL,
				      (void **)&fpriv->cwsr_trap_cpu_addr);
		return r;
	}

	return 0;
}

static void amdgpu_cwsr_deinit_trap(struct amdgpu_device *adev,
				    struct amdgpu_fpriv *fpriv)
{
	if (fpriv->cwsr_trap_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 &fpriv->vm,
					 fpriv->cwsr_trap_obj,
					 fpriv->cwsr_trap_va,
					 AMDGPU_CWSR_TBA_OFFSET);

		fpriv->cwsr_trap_va = NULL;
		amdgpu_bo_free_kernel(&fpriv->cwsr_trap_obj,
				      NULL,
				      &fpriv->cwsr_trap_cpu_addr);
	}
}

static int amdgpu_cwsr_init_sr_res(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm,
				   struct amdgpu_ring *ring)
{
	int r;
	u32 ctl_stack_size, wg_data_size;
	u32 cu_num;

	if (ring->cwsr_sr_obj)
		return 0;

	cu_num = adev->gfx.config.max_cu_per_sh *
		 adev->gfx.config.max_sh_per_se *
		 adev->gfx.config.max_shader_engines;

	//8 + 16 bytes are for header of ctl stack
	ctl_stack_size = cu_num * AMDGPU_CWSR_WAVES_PER_CU *
			 AMDGPU_CWSR_CNTL_STACK_BYTES_PER_WAVE + 8 + 16;
	ctl_stack_size = round_up(ctl_stack_size, 1 << 12);
	ring->cwsr_sr_ctl_size = ctl_stack_size;

	wg_data_size = cu_num *
		AMDGPU_CWSR_WG_CONTEXT_DATA_SIZE_PER_CU(adev->asic_type);
	wg_data_size = round_up(wg_data_size, 1 << 12);
	ring->cwsr_sr_size = wg_data_size + ctl_stack_size;

	r = amdgpu_bo_create_kernel(adev, ring->cwsr_sr_size,
				    PAGE_SIZE, AMDGPU_GEM_DOMAIN_GTT,
				    &ring->cwsr_sr_obj,
				    NULL,
				    (void **)&ring->cwsr_sr_cpu_addr);
	if (r) {
		DRM_ERROR("(%d) failed to create cwsr sr bo\n", r);
		return r;
	}
	/* clear memory */
	memset((char *)ring->cwsr_sr_cpu_addr, 0, ring->cwsr_sr_size);

	//ctl Stack is also named as Relaunch Stack
	r = amdgpu_cwsr_static_map(adev, vm,
				   ring->cwsr_sr_obj,
				   &ring->cwsr_sr_va,
				   AMDGPU_CWSR_SR_OFFSET +
				   ring->cwsr_slot_idx *
				   ring->cwsr_sr_size,
				   ring->cwsr_sr_size);
	if (r) {
		DRM_ERROR("map cwsr sr failed(%d)\n", r);
		amdgpu_bo_free_kernel(&ring->cwsr_sr_obj,
				      NULL,
				      (void **)&ring->cwsr_sr_cpu_addr);
		return r;
	}

	ring->cwsr_sr_gpu_addr = AMDGPU_CWSR_SR_OFFSET +
				ring->cwsr_slot_idx * ring->cwsr_sr_size;

	return 0;
}

static void amdgpu_cwsr_deinit_sr_res(struct amdgpu_device *adev,
				      struct amdgpu_vm *vm,
				      struct amdgpu_ring *ring)
{
	if (ring->cwsr_sr_obj) {
		amdgpu_cwsr_static_unmap(adev,
					 vm,
					 ring->cwsr_sr_obj,
					 ring->cwsr_sr_va,
					 AMDGPU_CWSR_SR_OFFSET +
					 ring->cwsr_slot_idx *
					 ring->cwsr_sr_size);

		ring->cwsr_sr_va = NULL;
		amdgpu_bo_free_kernel(&ring->cwsr_sr_obj,
				      NULL,
				      (void **)&ring->cwsr_sr_cpu_addr);
	}
}

static int amdgpu_cwsr_init_res(struct amdgpu_device *adev,
				struct amdgpu_fpriv *fpriv)
{
	int r;

	if (fpriv->cwsr_ready) {
		atomic_inc(&fpriv->cwsr_ctx_ref);
		return 0;
	}

	//init wb bo
	r = amdgpu_cwsr_init_wb(adev, fpriv);
	if (r)
		return r;

	//allocate eop buffer
	r = amdgpu_cwsr_init_hqd_eop(adev, fpriv);
	if (r)
		goto err1;

	r = amdgpu_cwsr_init_trap(adev, fpriv);
	if (r)
		goto err2;

	atomic_set(&fpriv->cwsr_ctx_ref, 1);

	fpriv->cwsr_ready = true;
	return 0;

err2:
	amdgpu_cwsr_deinit_hqd_eop(adev, fpriv);
err1:
	amdgpu_cwsr_deinit_wb(adev, fpriv);

	return r;
}

static void amdgpu_cwsr_deinit_res(struct amdgpu_device *adev,
				   struct amdgpu_fpriv *fpriv)
{
	if (!fpriv->cwsr_ready)
		return;


	if (atomic_dec_return(&fpriv->cwsr_ctx_ref) > 0)
		return;

	fpriv->cwsr_ready = false;
	amdgpu_cwsr_deinit_trap(adev, fpriv);
	amdgpu_cwsr_deinit_hqd_eop(adev, fpriv);
	amdgpu_cwsr_deinit_wb(adev, fpriv);
	DRM_DEBUG_DRIVER("deinit cwsr per VM global resource\n");
}

static void amdgpu_cwsr_fence_driver_start_ring(struct amdgpu_ring *ring,
						struct amdgpu_fpriv *fpriv)
{
	u32 seq;
	u32 irq_type;
	struct amdgpu_device *adev;
	struct amdgpu_irq_src *irq_src;

	adev = ring->adev;
	irq_src = &adev->gfx.eop_irq;
	irq_type = AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE0_EOP
		   + ((ring->me - 1) * adev->gfx.mec.num_pipe_per_mec)
		   + ring->pipe;

	ring->fence_drv.cpu_addr = &fpriv->cwsr_wb.wb[ring->fence_offs];
	ring->fence_drv.gpu_addr = fpriv->cwsr_wb.gpu_addr
				   + (ring->fence_offs * 4);
	seq = atomic_read(&ring->fence_drv.last_seq);
	*ring->fence_drv.cpu_addr = cpu_to_le32(seq);

	ring->fence_drv.irq_src = irq_src;
	ring->fence_drv.irq_type = irq_type;
	ring->fence_drv.initialized = true;
}

static int amdgpu_cwsr_init_ctx(struct amdgpu_device *adev,
				struct amdgpu_fpriv *fpriv,
				struct amdgpu_ctx *ctx)
{
	int r;
	int sched_hw_submission = amdgpu_sched_hw_submission;
	struct drm_gpu_scheduler *sched = NULL;
	struct drm_sched_entity *entity;
	struct amdgpu_ring *ring;

	if (!fpriv->cwsr_ready)
		return 0;

	r = ida_simple_get(&fpriv->cwsr_res_slots, 0,
			   AMDGPU_CWSR_MAX_RING, GFP_KERNEL);
	if (r < 0) {
		DRM_DEBUG_DRIVER("no valid solt for CWSR\n");
		return -EINVAL;
	}
	ctx->cwsr_slot_idx = r;
	DRM_DEBUG_DRIVER("get cwsr slot idx:%u\n", r);

	r = amdgpu_sws_early_init_ctx(ctx);
	if (r) {
		ida_simple_remove(&fpriv->cwsr_res_slots, ctx->cwsr_slot_idx);
		DRM_WARN("failed to do early ring init\n");
		return r;
	}

	ring = ctx->cwsr_ring;
	r = amdgpu_cwsr_init_ring(adev, ring, ctx, fpriv);
	if (r) {
		DRM_ERROR("failed to init cwsr ring\n");
		goto err1;
	}

	r = amdgpu_cwsr_init_mqd(adev, fpriv, ring);
	if (r) {
		DRM_ERROR("failed to get mqd for cwsr\n");
		goto err2;
	}

	r = amdgpu_cwsr_init_sr_res(adev, &fpriv->vm, ring);
	if (r) {
		DRM_ERROR("failed to get sr for cwsr\n");
		goto err3;
	}

	r = amdgpu_fence_driver_init_ring(ring, sched_hw_submission);
	if (r) {
		DRM_ERROR("(%d) failed to init cwsr fence drv\n", r);
		goto err4;
	}

	entity = &ctx->cwsr_entities->entity;
	sched = &ring->sched;
	r = drm_sched_entity_init(entity,
				  ctx->init_priority,
				  &sched,
				  1, &ctx->guilty);
	if (r) {
		DRM_ERROR("(%d) failed to init entity\n", r);
		goto err5;
	}

	amdgpu_cwsr_fence_driver_start_ring(ring, fpriv);

	r = amdgpu_sws_init_ctx(ctx, fpriv);
	if (r == AMDGPU_SWS_HW_RES_BUSY) {
		if (amdgpu_debugfs_ring_init(adev, ring))
			DRM_WARN("failed to init debugfs for ring:%s!\n",
				 ring->name);
		ctx->cwsr = true;
		return r;
	} else if (r < 0) {
		DRM_DEBUG_DRIVER("(%d) failed to init queue\n", r);
		goto err6;
	}

	if (amdgpu_debugfs_ring_init(adev, ring))
		DRM_WARN("failed to init debugfs for ring:%s!\n",
			 ring->name);
	ctx->cwsr = true;

	return 0;

err6:
	drm_sched_entity_destroy(entity);
err5:
	amdgpu_fence_driver_deinit_ring(ring);
err4:
	amdgpu_cwsr_deinit_sr_res(adev, &fpriv->vm, ring);
err3:
	amdgpu_cwsr_deinit_mqd(adev, fpriv, ring);
err2:
	amdgpu_cwsr_deinit_ring(ring, fpriv);
err1:
	amdgpu_sws_late_deinit_ctx(ctx);
	ida_simple_remove(&fpriv->cwsr_res_slots, ctx->cwsr_slot_idx);
	return r;
}

static void amdgpu_cwsr_deinit_ctx(struct amdgpu_device *adev,
				   struct amdgpu_ctx *ctx)
{
	struct amdgpu_ring *ring;
	struct amdgpu_fpriv *fpriv;

	if (!ctx->cwsr)
		return;

	fpriv = ctx->fpriv;
	ctx->cwsr = false;
	ring = ctx->cwsr_ring;

	amdgpu_debugfs_ring_fini(ring);

	amdgpu_sws_deinit_ctx(ctx, fpriv);

	amdgpu_fence_driver_deinit_ring(ring);

	amdgpu_cwsr_deinit_sr_res(adev, &fpriv->vm, ring);
	amdgpu_cwsr_deinit_mqd(adev, fpriv, ring);
	amdgpu_cwsr_deinit_ring(ring, fpriv);
	amdgpu_sws_late_deinit_ctx(ctx);
	ida_simple_remove(&fpriv->cwsr_res_slots, ctx->cwsr_slot_idx);
}

int amdgpu_cwsr_init_queue(struct amdgpu_ring *ring)
{
	int r;
	struct amdgpu_device *adev;

	if (!ring->cwsr ||
	    ring->sws_ctx.queue_state != AMDGPU_SWS_QUEUE_DISABLED)
		return 0;

	adev = ring->adev;

	ring->mqd_gpu_addr = AMDGPU_CWSR_MQD_OFFSET +
			 ring->cwsr_slot_idx *
			 AMDGPU_CWSR_MQD_SIZE;

	//init mqd
	r = amdgpu_ring_compute_mqd_init(ring);
	if (r) {
		DRM_ERROR("failed to init mqd for cwsr\n");
		goto err1;
	}

	//map queue
	r = adev->gfx.mec.map_cwsr_queue(ring);
	if (r) {
		DRM_ERROR("failed to map queue for cwsr\n");
		goto err1;
	}

	ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_ENABLED;

	return 0;
err1:
	ring->cwsr_queue_broken = true;

	return r;
}

void amdgpu_cwsr_deinit_queue(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (!ring->cwsr ||
	    ring->sws_ctx.queue_state != AMDGPU_SWS_QUEUE_ENABLED)
		return;

	//unmap queue
	adev->gfx.mec.unmap_cwsr_queue(ring, AMDGPU_CP_HQD_DEQUEUE_MODE_STD);

	ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_DISABLED;
}

int amdgpu_cwsr_init(struct amdgpu_ctx *ctx)
{
	struct amdgpu_fpriv *fpriv;
	struct amdgpu_device *adev;
	int r = 0;

	adev = ctx->adev;
	fpriv = ctx->fpriv;

	if (cwsr_enable == 0)
		return 0;

	mutex_lock(&fpriv->cwsr_lock);
	ctx->cwsr_init = true;

	r = amdgpu_cwsr_init_res(adev, fpriv);
	if (r) {
		DRM_WARN("failed to init cwsr res\n");
		mutex_unlock(&fpriv->cwsr_lock);
		return r;
	}

	r = amdgpu_cwsr_init_ctx(adev, fpriv, ctx);
	if (r == AMDGPU_SWS_HW_RES_BUSY) {
		mutex_unlock(&fpriv->cwsr_lock);
		return AMDGPU_SWS_HW_RES_BUSY;
	} else if (r < 0) {
		DRM_DEBUG_DRIVER("failed to init cwsr ctx\n");
		goto err1;
	}

	mutex_unlock(&fpriv->cwsr_lock);
	return r;

err1:
	amdgpu_cwsr_deinit_res(adev, fpriv);
	mutex_unlock(&fpriv->cwsr_lock);
	return r;
}

void amdgpu_cwsr_deinit(struct amdgpu_ctx *ctx)
{
	struct amdgpu_fpriv *fpriv;
	struct amdgpu_device *adev;

	adev = ctx->adev;
	fpriv = ctx->fpriv;

	if (cwsr_enable == 0)
		return;

	mutex_lock(&fpriv->cwsr_lock);
	if (ctx->cwsr) {
		amdgpu_cwsr_deinit_ctx(adev, ctx);
		amdgpu_cwsr_deinit_res(adev, fpriv);
	}
	mutex_unlock(&fpriv->cwsr_lock);
}

int amdgpu_cwsr_dequeue(struct amdgpu_ring *ring)
{
	int r;

	struct amdgpu_device *adev = ring->adev;

	if (ring->sws_ctx.queue_state != AMDGPU_SWS_QUEUE_ENABLED)
		return 0;

	r = adev->gfx.mec.unmap_cwsr_queue(ring,
					   AMDGPU_CP_HQD_DEQUEUE_MODE_SSSD);
	if (r)
		ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_DISABLED;
	else
		ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_DEQUEUED;

	return r;
}

int amdgpu_cwsr_relaunch(struct amdgpu_ring *ring)
{
	int r;
	struct amdgpu_device *adev = ring->adev;

	if (ring->sws_ctx.queue_state != AMDGPU_SWS_QUEUE_DEQUEUED)
		return 0;

	//relaunch queue
	r = adev->gfx.mec.map_cwsr_queue(ring);
	if (r) {
		DRM_ERROR("failed to map queue for cwsr\n");

		return r;
	}

	ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_ENABLED;

	return r;
}
