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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include "amdgpu.h"
#include "amdgpu_cwsr.h"

static u32 amdgpu_sws_get_vmid_candidate(struct amdgpu_device *adev)
{
	int i;
	u32 idx, min_usage;
	struct amdgpu_sws *sws;

	sws = &adev->sws;
	idx = 0;
	min_usage = sws->vmid_usage[0];
	for (i = 0; i < sws->max_vmid_num; i++) {
		if (min_usage > sws->vmid_usage[i]) {
			min_usage = sws->vmid_usage[i];
			idx = i;
		}
	}

	return idx;
}

static int amdgpu_sws_get_vmid(struct amdgpu_ring *ring,
			       struct amdgpu_vm *vm)
{
	int r;
	u32 bit;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx;
	struct amdgpu_vmid_mgr *id_mgr;
	struct amdgpu_vmid *vmid;
	struct amdgpu_device *adev = ring->adev;

	sws = &adev->sws;
	sws_ctx = &ring->sws_ctx;
	id_mgr = &adev->vm_manager.id_mgr[AMDGPU_GFXHUB_0];

	if (vm->reserved_vmid[AMDGPU_GFXHUB_0]) {
		if (ring->cwsr_vmid !=
		    vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid) {
			ring->cwsr_vmid =
				vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid;
			bit = ring->cwsr_vmid - AMDGPU_SWS_VMID_BEGIN;
			ring->vm_inv_eng = sws->inv_eng[bit];
		} else {
			bit = ring->cwsr_vmid - AMDGPU_SWS_VMID_BEGIN;
		}

		DRM_DEBUG_DRIVER("%s reuse vmid:%u and invalidate eng:%u\n",
				 ring->name, ring->cwsr_vmid, ring->vm_inv_eng);

		if (test_bit(bit, sws->vmid_bitmap) &&
		    refcount_read(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0]) == 0)
			return AMDGPU_SWS_HW_RES_BUSY;

		set_bit(bit, sws->vmid_bitmap);
		if (refcount_read(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0]) == 0)
			refcount_set(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0],
				     1);
		else
			refcount_inc(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0]);

		amdgpu_gmc_cwsr_flush_gpu_tlb(ring->cwsr_vmid,
					      vm->root.base.bo, ring);
		return 0;
	}

	r = amdgpu_vmid_cwsr_grab(adev, &vmid);
	if (r) {
		DRM_WARN("failed to get vmid structure.\n");
		return -EINVAL;
	}

	bit = find_first_zero_bit(sws->vmid_bitmap, sws->max_vmid_num);
	if (bit == sws->max_vmid_num) {
		sws->vmid_res_state = AMDGPU_SWS_HW_RES_BUSY;
		list_del_init(&sws_ctx->list);
		list_add_tail(&sws_ctx->list, &sws->idle_list);
		bit = amdgpu_sws_get_vmid_candidate(adev);
		r = AMDGPU_SWS_HW_RES_BUSY;
	} else {
		set_bit(bit, sws->vmid_bitmap);
		refcount_set(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0], 1);
	}

	ring->vm_inv_eng = sws->inv_eng[bit];
	ring->cwsr_vmid = bit + AMDGPU_SWS_VMID_BEGIN;
	vmid->cwsr_vmid = ring->cwsr_vmid;
	vm->reserved_vmid[AMDGPU_GFXHUB_0] = vmid;

	//set and flush VM table
	if (!r)
		amdgpu_gmc_cwsr_flush_gpu_tlb(ring->cwsr_vmid,
					      vm->root.base.bo, ring);

	DRM_DEBUG_DRIVER("%s get vmid:%u and invalidate eng:%u\n",
			 ring->name, ring->cwsr_vmid, ring->vm_inv_eng);
	return r;
}

static void amdgpu_sws_put_vmid(struct amdgpu_ring *ring,
				struct amdgpu_vm *vm)
{
	struct amdgpu_device *adev;
	struct amdgpu_sws *sws;
	u32 bit;

	if (!ring->cwsr)
		return;

	if (!vm->reserved_vmid[AMDGPU_GFXHUB_0]) {
		DRM_WARN("NO reserved VMID!!\n");
		return;
	}

	if (refcount_read(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0]) == 0) {
		DRM_WARN("skip put vmid when ref is 0!!\n");
		return;
	}
	adev = ring->adev;
	sws = &adev->sws;

	bit = vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid -
		AMDGPU_SWS_VMID_BEGIN;

	if (refcount_dec_and_test(&vm->reserved_vmid_ref[AMDGPU_GFXHUB_0])) {
		clear_bit(bit, sws->vmid_bitmap);

		amdgpu_gmc_cwsr_flush_gpu_tlb(ring->cwsr_vmid,
					      vm->root.base.bo, ring);
		sws->vmid_res_state = AMDGPU_SWS_HW_RES_AVAILABLE;
		DRM_DEBUG_DRIVER("put vmid %u\n",
				 vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid);
	}
}

static int amdgpu_sws_get_queue(struct amdgpu_ring *ring)
{
	int i;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx;
	struct amdgpu_device *adev = ring->adev;
	u32 irq_type;
	struct amdgpu_irq_src *irq_src;
	DECLARE_BITMAP(tmp_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);

	sws = &adev->sws;
	sws_ctx = &ring->sws_ctx;
	irq_src = &adev->gfx.eop_irq;

	bitmap_xor(tmp_bitmap, sws->avail_queue_bitmap,
		   sws->run_queue_bitmap,
		   AMDGPU_MAX_COMPUTE_QUEUES);

	bitmap_andnot(tmp_bitmap, tmp_bitmap,
		      sws->broken_queue_bitmap,
		      AMDGPU_MAX_COMPUTE_QUEUES);

	i = find_first_bit(tmp_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
	if (i == AMDGPU_MAX_COMPUTE_QUEUES) {
		sws->queue_res_state = AMDGPU_SWS_HW_RES_BUSY;
		list_del_init(&sws_ctx->list);
		list_add_tail(&sws_ctx->list, &sws->idle_list);
		return AMDGPU_SWS_HW_RES_BUSY;
	}

	set_bit(i, sws->run_queue_bitmap);

	ring->queue = i % adev->gfx.mec.num_queue_per_pipe;
	ring->pipe = (i / adev->gfx.mec.num_queue_per_pipe)
		     % adev->gfx.mec.num_pipe_per_mec;
	ring->me = (i / adev->gfx.mec.num_queue_per_pipe)
		   / adev->gfx.mec.num_pipe_per_mec;
	ring->me += 1;

	list_del_init(&sws_ctx->list);
	list_add_tail(&sws_ctx->list, &sws->run_list);

	irq_type = AMDGPU_CP_IRQ_COMPUTE_MEC1_PIPE0_EOP
		+ ((ring->me - 1) * adev->gfx.mec.num_pipe_per_mec)
		+ ring->pipe;

	ring->fence_drv.irq_type = irq_type;
	amdgpu_irq_get(adev, irq_src, irq_type);

	DRM_DEBUG_DRIVER("get queue %u.%u.%u and irq type(%d) for ring %s\n",
			 ring->me, ring->pipe, ring->queue,
			 irq_type, ring->name);
	return 0;
}

static void amdgpu_sws_put_queue(struct amdgpu_ring *ring)
{
	int bit;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx;
	struct amdgpu_device *adev = ring->adev;

	sws = &adev->sws;
	sws_ctx = &ring->sws_ctx;

	bit = amdgpu_gfx_mec_queue_to_bit(adev, ring->me - 1, ring->pipe,
					  ring->queue);

	if (!test_bit(bit, sws->run_queue_bitmap)) {
		DRM_WARN("Put an sws null queue!!\n");
		return;
	}

	clear_bit(bit, sws->run_queue_bitmap);
	sws->queue_res_state = AMDGPU_SWS_HW_RES_AVAILABLE;

	amdgpu_irq_put(adev, &adev->gfx.eop_irq,
		       ring->fence_drv.irq_type);
	list_del_init(&sws_ctx->list);
	list_add_tail(&sws_ctx->list, &sws->idle_list);
}

int amdgpu_sws_early_init_ctx(struct amdgpu_ctx *ctx)
{
	int r;
	unsigned long flags;
	struct amdgpu_sws *sws;
	struct amdgpu_device *adev;

	adev = ctx->adev;
	sws = &adev->sws;

	ctx->cwsr_ring = kzalloc(sizeof(struct amdgpu_ring), GFP_KERNEL);
	if (ctx->cwsr_ring == NULL)
		return -ENOMEM;

	ctx->cwsr_ring->funcs = adev->gfx.compute_ring[0].funcs;

	spin_lock_irqsave(&sws->lock, flags);
	r = ida_simple_get(&sws->doorbell_ida,
			   adev->doorbell_index.first_cwsr,
			   adev->doorbell_index.last_cwsr + 1,
			   GFP_KERNEL);
	if (r < 0)
		goto err1;

	ctx->cwsr_ring->doorbell_index = r << 1;

	r = ida_simple_get(&sws->ring_idx_ida,
			   adev->num_rings,
			   AMDGPU_MAX_RINGS,
			   GFP_KERNEL);
	if (r < 0)
		goto err2;

	ctx->cwsr_ring->idx = r;
	if (adev->rings[r] == NULL)
		adev->rings[r] = ctx->cwsr_ring;
	else
		goto err3;

	sprintf(ctx->cwsr_ring->name, "comp_cwsr_%d", r);

	spin_unlock_irqrestore(&sws->lock, flags);

	return 0;

err3:
	ida_simple_remove(&sws->ring_idx_ida,
			  ctx->cwsr_ring->idx);

err2:
	ida_simple_remove(&sws->doorbell_ida,
			  ctx->cwsr_ring->doorbell_index >> 1);
err1:
	spin_unlock_irqrestore(&sws->lock, flags);

	kfree(ctx->cwsr_ring);
	ctx->cwsr_ring = NULL;
	DRM_WARN("failed to do early init\n");

	return r;
}

void amdgpu_sws_late_deinit_ctx(struct amdgpu_ctx *ctx)
{
	unsigned long flags;
	struct amdgpu_sws *sws;
	struct amdgpu_device *adev;

	adev = ctx->adev;
	sws = &adev->sws;

	spin_lock_irqsave(&sws->lock, flags);
	ida_simple_remove(&sws->ring_idx_ida,
			  ctx->cwsr_ring->idx);
	ida_simple_remove(&sws->doorbell_ida,
			  ctx->cwsr_ring->doorbell_index >> 1);
	adev->rings[ctx->cwsr_ring->idx] = NULL;

	spin_unlock_irqrestore(&sws->lock, flags);

	kfree(ctx->cwsr_ring);
}

int amdgpu_sws_init_ctx(struct amdgpu_ctx *ctx, struct amdgpu_fpriv *fpriv)
{
	struct amdgpu_sws *sws;
	struct amdgpu_device *adev;
	struct amdgpu_vm *vm;
	struct amdgpu_ring *ring;
	struct amdgpu_sws_ctx *sws_ctx;
	int r, b;
	unsigned long flags;

	adev = ctx->adev;
	sws = &adev->sws;
	vm = &fpriv->vm;

	ring = ctx->cwsr_ring;
	sws_ctx = &ring->sws_ctx;

	spin_lock_irqsave(&sws->lock, flags);
	INIT_LIST_HEAD(&sws_ctx->list);

	sws_ctx->ring = ring;
	sws_ctx->ctx = ctx;

	switch (ctx->ctx_priority) {
	case AMDGPU_CTX_PRIORITY_HIGH:
		sws_ctx->priority = SWS_SCHED_PRIORITY_HIGH;
		break;

	case AMDGPU_CTX_PRIORITY_UNSET:
	case AMDGPU_CTX_PRIORITY_NORMAL:
		sws_ctx->priority = SWS_SCHED_PRIORITY_NORMAL;
		break;

	case AMDGPU_CTX_PRIORITY_LOW:
	case AMDGPU_CTX_PRIORITY_VERY_LOW:
		sws_ctx->priority = SWS_SCHED_PRIORITY_LOW;
		break;

	default:
		DRM_WARN("SWS does not handle tunnel priority ctx\n");
		spin_unlock_irqrestore(&sws->lock, flags);
		return -EINVAL;
	}

	ring->cwsr_vmid = AMDGPU_SWS_NO_VMID;
	r = amdgpu_sws_get_vmid(ring, vm);
	if (r < 0) {
		spin_unlock_irqrestore(&sws->lock, flags);
		return -EINVAL;
	} else if (r == AMDGPU_SWS_HW_RES_BUSY) {
		goto out;
	}

	r = amdgpu_sws_get_queue(ring);
	if (r)
		goto out;

	pm_runtime_mark_last_busy(adev->dev);
	r = amdgpu_cwsr_init_queue(ring);
	if (r) {
		b = amdgpu_gfx_mec_queue_to_bit(adev, ring->me - 1,
						ring->pipe,
						ring->queue);
		set_bit(b, sws->broken_queue_bitmap);
		list_del_init(&sws_ctx->list);
		list_add_tail(&sws_ctx->list, &sws->broken_list);

		//next commands will be handled by legacy queue
		ring->cwsr_queue_broken = true;

		DRM_WARN("failed to init cwsr queue\n");
		spin_unlock_irqrestore(&sws->lock, flags);
		return -EINVAL;
	}

	sws_ctx->sched_round_begin = sws->sched_round;
out:
	sws->vmid_usage[ring->cwsr_vmid - AMDGPU_SWS_VMID_BEGIN]++;
	sws->ctx_num++;
	spin_unlock_irqrestore(&sws->lock, flags);
	return r;
}

void amdgpu_sws_deinit_ctx(struct amdgpu_ctx *ctx,
			   struct amdgpu_fpriv *fpriv)
{
	struct amdgpu_sws *sws;
	struct amdgpu_ring *ring;
	struct amdgpu_device *adev;
	struct amdgpu_vm *vm;
	unsigned long flags;

	adev = ctx->adev;
	sws = &adev->sws;
	vm = &fpriv->vm;

	ring = ctx->cwsr_ring;
	spin_lock_irqsave(&sws->lock, flags);
	pm_runtime_mark_last_busy(adev->dev);
	if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_ENABLED) {
		amdgpu_cwsr_deinit_queue(ring);
		amdgpu_sws_put_queue(ring);
		amdgpu_sws_put_vmid(ring, vm);
	}

	list_del_init(&ring->sws_ctx.list);
	sws->vmid_usage[ring->cwsr_vmid - AMDGPU_SWS_VMID_BEGIN]--;
	memset(&ring->sws_ctx, 0, sizeof(struct amdgpu_sws_ctx));
	sws->ctx_num--;
	spin_unlock_irqrestore(&sws->lock, flags);
}

static int amdgpu_sws_map(struct amdgpu_sws_ctx *sws_ctx)
{
	int r, b;
	struct amdgpu_ring *ring;
	struct amdgpu_ctx *ctx;
	struct amdgpu_device *adev;
	struct amdgpu_sws *sws;
	struct amdgpu_vm *vm;

	ring = sws_ctx->ring;
	ctx = sws_ctx->ctx;
	adev = ring->adev;
	sws = &adev->sws;
	vm = &ctx->fpriv->vm;

	if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_DEQUEUED) {
		r = amdgpu_sws_get_vmid(ring, vm);
		if (r) {
			DRM_DEBUG_DRIVER("failed to get vmid for ring:%s\n",
					 ring->name);
			return r;
		}

		r = amdgpu_sws_get_queue(ring);
		if (r) {
			amdgpu_sws_put_vmid(ring, vm);
			DRM_ERROR("failed to get queue for ring:%s\n",
				  ring->name);
			return r;
		}
		DRM_DEBUG_DRIVER("map ring(%s) on queue %u.%u.%u\n",
				 ring->name, ring->me, ring->pipe, ring->queue);

		r = amdgpu_cwsr_relaunch(ring);
		if (r) {
			b = amdgpu_gfx_mec_queue_to_bit(adev, ring->me - 1,
							ring->pipe,
							ring->queue);
			set_bit(b, sws->broken_queue_bitmap);

			list_del_init(&sws_ctx->list);
			list_add_tail(&sws_ctx->list, &sws->broken_list);

			//next commands will be handled by legacy queue
			ring->cwsr_queue_broken = true;
			DRM_ERROR("failed to relaunch for ring:%s\n",
				  ring->name);
			return r;
		}

		sws_ctx->sched_round_begin = sws->sched_round;
	} else if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_DISABLED) {
		if (ring->cwsr_vmid == AMDGPU_SWS_NO_VMID) {
			r = amdgpu_sws_get_vmid(ring, vm);
			if (r) {
				DRM_WARN("failed to get vmid for ring:%s\n",
					 ring->name);
				return r;
			}
		}

		r = amdgpu_sws_get_queue(ring);
		if (r) {
			DRM_WARN("failed to get queue for ring:%s\n",
				 ring->name);
			return r;
		}
		DRM_DEBUG_DRIVER("init ring(%s) on queue %u.%u.%u\n",
				 ring->name, ring->me, ring->pipe, ring->queue);

		r = amdgpu_cwsr_init_queue(ring);
		if (r) {
			b = amdgpu_gfx_mec_queue_to_bit(adev, ring->me - 1,
							ring->pipe,
							ring->queue);
			set_bit(b, sws->broken_queue_bitmap);
			list_del_init(&sws_ctx->list);
			list_add_tail(&sws_ctx->list, &sws->broken_list);

			//next commands will be handled by legacy queue
			ring->cwsr_queue_broken = true;
			DRM_ERROR("failed to init queue for ring:%s\n",
				  ring->name);
			return r;
		}

		sws_ctx->sched_round_begin = sws->sched_round;
		ring->sws_ctx.queue_state = AMDGPU_SWS_QUEUE_ENABLED;
	} else {
		DRM_WARN("Skip to map queue because of wrong state:%u\n",
			 ring->sws_ctx.queue_state);
	}

	return 0;
}

static int amdgpu_sws_unmap(struct amdgpu_sws_ctx *sws_ctx)
{
	struct amdgpu_ring *ring;
	struct amdgpu_ctx *ctx;
	struct amdgpu_device *adev;
	struct amdgpu_sws *sws;
	struct amdgpu_fpriv *fpriv;
	int r, b;

	ring = sws_ctx->ring;
	ctx = sws_ctx->ctx;
	adev = ring->adev;
	sws = &adev->sws;
	fpriv = ctx->fpriv;

	if (ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_ENABLED) {
		DRM_DEBUG_DRIVER("unmap ring(%s) on queue %u.%u.%u\n",
				 ring->name, ring->me, ring->pipe, ring->queue);

		r = amdgpu_cwsr_dequeue(ring);
		if (r) {
			b = amdgpu_gfx_mec_queue_to_bit(adev, ring->me - 1,
							ring->pipe,
							ring->queue);
			set_bit(b, sws->broken_queue_bitmap);

			list_del_init(&sws_ctx->list);
			list_add_tail(&sws_ctx->list, &sws->broken_list);

			//next commands will be handled by legacy queue
			ring->cwsr_queue_broken = true;
			return r;
		}

		amdgpu_sws_put_queue(ring);
		amdgpu_sws_put_vmid(ring, &fpriv->vm);
	} else {
		DRM_WARN("Skip unmapping queue because of wrong state%u\n",
			 ring->sws_ctx.queue_state);
	}

	return 0;
}

static bool amdgpu_sws_expired(u32 sched_round,
			       enum sws_sched_priority priority)
{
	if (sched_round >= AMDGPU_SWS_HIGH_ROUND)
		return true;

	if (priority == SWS_SCHED_PRIORITY_NORMAL &&
	    sched_round >= AMDGPU_SWS_NORMAL_ROUND)
		return true;

	if (priority == SWS_SCHED_PRIORITY_LOW &&
	    sched_round >= AMDGPU_SWS_LOW_ROUND)
		return true;

	return false;
}

static enum hrtimer_restart amdgpu_sws_sched_timer_fn(struct hrtimer *timer)
{
	struct amdgpu_sws *sws;
	struct amdgpu_device *adev;

	sws = container_of(timer, struct amdgpu_sws, timer);
	adev = container_of(sws, struct amdgpu_device, sws);

	if (amdgpu_in_reset(adev) || adev->in_suspend)
		goto out;

	if (!work_pending(&sws->sched_work))
		queue_work(sws->sched, &sws->sched_work);
	else
		printk_ratelimited("sws has pending work!\n");

out:
	hrtimer_start(&sws->timer, ns_to_ktime(sws->quantum),
		      HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

/* return true if running process is using same vmid as
 * candidate process.
 */
static bool amdgpu_sws_vmid_busy(struct amdgpu_sws *sws,
				 struct amdgpu_sws_ctx *sws_ctx)
{
	struct amdgpu_sws_ctx *sws_run_ctx;
	struct amdgpu_vm *run_vm;
	struct amdgpu_vm *vm;

	vm = &sws_ctx->ctx->fpriv->vm;
	list_for_each_entry(sws_run_ctx, &sws->run_list, list) {
		run_vm = &sws_run_ctx->ctx->fpriv->vm;
		if (vm == run_vm)
			return false;

		if (vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid ==
		    run_vm->reserved_vmid[AMDGPU_GFXHUB_0]->cwsr_vmid)
			return true;
	}

	return false;
}

static void amdgpu_sws_sched_fn(struct work_struct *work)
{
	int i;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx, *tmp;
	struct amdgpu_ring *ring;
	unsigned long flags;
	struct amdgpu_device *adev;
	u32 running_queue, running_vmid, pending_ctx;

	sws = container_of(work, struct amdgpu_sws, sched_work);
	adev = container_of(sws, struct amdgpu_device, sws);

	spin_lock_irqsave(&sws->lock, flags);
	sws->sched_round++;

	//go to unmap directly if any pending job on gfx ring
	for (i = 0; i < AMDGPU_MAX_GFX_RINGS; i++) {
		ring = &adev->gfx.gfx_ring[i];
		if (ring->ring_obj == NULL)
			continue;

		if (atomic_read(&ring->fence_drv.last_seq) !=
		    ring->fence_drv.sync_seq)
			goto unmap;
	}

	//skip scheduling
	if (list_empty(&sws->idle_list)) {
		list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list)
			sws_ctx->sched_num++;
		goto out;
	}

	pm_runtime_mark_last_busy(adev->dev);

	//skip unmap when there is enough res for the pending ctx
	running_queue = bitmap_weight(sws->run_queue_bitmap,
				      AMDGPU_MAX_COMPUTE_QUEUES);
	running_vmid = bitmap_weight(sws->vmid_bitmap,
				     sws->max_vmid_num);
	pending_ctx = 0;
	list_for_each_entry_safe(sws_ctx, tmp, &sws->idle_list, list) {
		ring = container_of(sws_ctx, struct amdgpu_ring, sws_ctx);
		if (atomic_read(&ring->fence_drv.last_seq) !=
		    ring->fence_drv.sync_seq)
			pending_ctx++;
	}

	if ((pending_ctx + running_queue) <= sws->queue_num &&
	    (pending_ctx + running_vmid) <= AMDGPU_SWS_MAX_VMID_NUM &&
	    sws->vmid_res_state == AMDGPU_SWS_HW_RES_AVAILABLE &&
	    sws->queue_res_state == AMDGPU_SWS_HW_RES_AVAILABLE) {
		list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list)
			sws_ctx->sched_num++;
		goto map;
	}

unmap:
	list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list) {
		sws_ctx->sched_num++;

		//unmap expired ctx
		if (amdgpu_sws_expired(sws->sched_round -
				       sws_ctx->sched_round_begin,
				       sws_ctx->priority)) {
			amdgpu_sws_unmap(sws_ctx);
		}
	}

	if (sws->queue_res_state || sws->vmid_res_state)
		goto out;

map:
	list_for_each_entry_safe(sws_ctx, tmp, &sws->idle_list, list) {
		ring = container_of(sws_ctx, struct amdgpu_ring, sws_ctx);
		if (bitmap_weight(sws->run_queue_bitmap,
				  AMDGPU_MAX_COMPUTE_QUEUES) < sws->queue_num) {
			if (atomic_read(&ring->fence_drv.last_seq) ==
			    ring->fence_drv.sync_seq)
				continue;

			if (amdgpu_sws_vmid_busy(sws, sws_ctx))
				continue;

			if (amdgpu_sws_map(sws_ctx) < 0)
				goto out;
		} else {
			break;
		}
	}

out:
	spin_unlock_irqrestore(&sws->lock, flags);
}

static ssize_t amdgpu_sws_read_debugfs(struct file *f, char __user *ubuf,
				       size_t size, loff_t *pos)
{
	int r, i;
	struct amdgpu_sws *sws;
	struct amdgpu_ring *ring;
	struct amdgpu_sws_ctx *sws_ctx;
	char *buf = NULL;
	struct amdgpu_device *adev = file_inode(f)->i_private;
	unsigned long flags;
	int ret_ignore = 0;

	sws = &adev->sws;

	if (!size)
		return 0;

	buf = kmalloc(AMDGPU_SWS_DEBUGFS_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	if (*pos) {
		buf[0] = '\0';
		ret_ignore = copy_to_user(ubuf, buf, 1);
		kfree(buf);

		if ((AMDGPU_SWS_DEBUGFS_SIZE - *pos) >= size) {
			*pos += size;
		} else {
			*pos = AMDGPU_SWS_DEBUGFS_SIZE;
			size = AMDGPU_SWS_DEBUGFS_SIZE - *pos;
		}

		return size;
	}

	spin_lock_irqsave(&sws->lock, flags);
	r = sprintf(buf, "sws general info:\n");
	r += sprintf(&buf[r], "\tquantum granularity:%llu ms\n",
		     sws->quantum / NSEC_PER_MSEC);
	r += sprintf(&buf[r], "\tactive ctx number:%u\n", sws->ctx_num);
	r += sprintf(&buf[r], "\ttotal queue number:%u\n", sws->queue_num);
	r += sprintf(&buf[r], "\ttotal available vmid number:%u\n",
		     sws->max_vmid_num);
	r += sprintf(&buf[r], "\tHW res(vmid,inv eng) state:%s\n",
		     sws->vmid_res_state ? "busy" : "available");
	r += sprintf(&buf[r], "\tHW res(queue) state:%s\n",
		     sws->queue_res_state ? "busy" : "available");
	r += sprintf(&buf[r], "\tvmid usage\n");
	r += sprintf(&buf[r], "\tvmid:\t8\t9\t10\t11\t12\t13\t14\t15\n");
	r += sprintf(&buf[r], "\tusage:");
	for (i = 0; i < sws->max_vmid_num; i++)
		r += sprintf(&buf[r], "\t%u", sws->vmid_usage[i]);
	r += sprintf(&buf[r], "\n");
	r += sprintf(&buf[r], "\tsched round:%u\n", sws->sched_round);

	r += sprintf(&buf[r], "\n\nrun list:\n");
	list_for_each_entry(sws_ctx, &sws->run_list, list) {
		if (AMDGPU_SWS_DEBUGFS_SIZE - r < 200) {
			r = sprintf(&buf[r], "skip too much ring status\n");
			goto out;
		}

		ring = sws_ctx->ctx->cwsr_ring;
		r += sprintf(&buf[r],
			     "\tring name: %s; doorbel idx:%u; priority: %u;",
			     ring->name, ring->doorbell_index,
			     sws_ctx->priority);

		r += sprintf(&buf[r],
			     " vmid: %u; sched times:%u; queue:%u.%u.%u\n",
			     ring->cwsr_vmid, sws_ctx->sched_num, ring->me,
			     ring->pipe, ring->queue);
	}
	if (list_empty(&sws->run_list))
		r += sprintf(&buf[r], "\tN/A\n");

	r += sprintf(&buf[r], "\n\nidle list:\n");
	list_for_each_entry(sws_ctx, &sws->idle_list, list) {
		if (AMDGPU_SWS_DEBUGFS_SIZE - r < 200) {
			r = sprintf(&buf[r], "skip too much ring status\n");
			goto out;
		}

		ring = sws_ctx->ctx->cwsr_ring;
		r += sprintf(&buf[r],
			     "\tring name: %s; doorbell idx: %d; priority: %u;",
			     ring->name,
			     ring->doorbell_index,
			     sws_ctx->priority);

		r += sprintf(&buf[r],
			     " vmid: %u; sched times:%u last queue:%u.%u.%u\n",
			     ring->cwsr_vmid, sws_ctx->sched_num, ring->me,
			     ring->pipe, ring->queue);
	}

	if (list_empty(&sws->idle_list))
		r += sprintf(&buf[r], "\tN/A\n");

	r += sprintf(&buf[r], "\n\nbroken list:\n");
	list_for_each_entry(sws_ctx, &sws->broken_list, list) {
		if (AMDGPU_SWS_DEBUGFS_SIZE - r < 200) {
			r = sprintf(&buf[r], "skip too much ring status\n");
			goto out;
		}

		ring = sws_ctx->ctx->cwsr_ring;
		r += sprintf(&buf[r],
			     "\tring name: %s; doorbell idx: %d; priority: %u;",
			     ring->name,
			     ring->doorbell_index,
			     sws_ctx->priority);

		r += sprintf(&buf[r],
			     " vmid: %u; sched times:%u last queue:%u.%u.%u\n",
			     ring->cwsr_vmid, sws_ctx->sched_num, ring->me,
			     ring->pipe, ring->queue);
	}

	if (list_empty(&sws->broken_list))
		r += sprintf(&buf[r], "\tN/A\n");
out:
	spin_unlock_irqrestore(&sws->lock, flags);

	ret_ignore = copy_to_user(ubuf, buf, r);
	kfree(buf);
	*pos = r;

	return r;
}

static ssize_t amdgpu_sws_write_debugfs(struct file *file,
					const char __user *buf,
					size_t len, loff_t *ppos)
{
	int r;
	u32 val;
	size_t size;
	char local_buf[256];
	unsigned long flags;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx, *tmp;
	struct amdgpu_device *adev = file_inode(file)->i_private;

	sws = &adev->sws;
	size = min(sizeof(local_buf) - 1, len);
	if (copy_from_user(local_buf, buf, size)) {
		DRM_WARN("failed to copy buf from user space\n");
		return len;
	}

	local_buf[size] = '\0';
	r = kstrtou32(local_buf, 0, &val);
	if (r)
		return r;

	spin_lock_irqsave(&sws->lock, flags);
	switch (val) {
	/* pause scheduler */
	case 0:
		hrtimer_cancel(&sws->timer);
		break;

	/* continue scheudler */
	case 1:
		hrtimer_start(&sws->timer, ns_to_ktime(sws->quantum),
			      HRTIMER_MODE_REL);
		break;

	/* pause scheduler and force one round re-sched(FIFO) for all ctx */
	case 2:
		hrtimer_cancel(&sws->timer);
		list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list) {
			sws_ctx->sched_num++;
			amdgpu_sws_unmap(sws_ctx);
		}

		list_for_each_entry_safe(sws_ctx, tmp, &sws->idle_list, list) {
			if (bitmap_weight(sws->run_queue_bitmap,
					  AMDGPU_MAX_COMPUTE_QUEUES) <
			    sws->queue_num)
				amdgpu_sws_map(sws_ctx);
			else
				break;
		}

		break;

	/* pause scheduler and force one round re-sched(FILO) for all ctx */
	case 3:
		hrtimer_cancel(&sws->timer);
		list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list) {
			sws_ctx->sched_num++;
			amdgpu_sws_unmap(sws_ctx);
		}

		list_for_each_entry_safe_reverse(sws_ctx, tmp,
						 &sws->idle_list, list) {
			if (bitmap_weight(sws->run_queue_bitmap,
					  AMDGPU_MAX_COMPUTE_QUEUES) <
			    sws->queue_num)
				amdgpu_sws_map(sws_ctx);
			else
				break;
		}

		break;

	/* pause scheduler and unmap all running ctx */
	case 4:
		hrtimer_cancel(&sws->timer);
		list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list) {
			sws_ctx->sched_num++;
			amdgpu_sws_unmap(sws_ctx);
		}

		break;

	/* add first running ctx into broken_list for test */
	case 5:
		sws_ctx = list_first_entry(&sws->run_list, struct amdgpu_sws_ctx, list);
		amdgpu_sws_unmap(sws_ctx);

		list_del_init(&sws_ctx->list);
		list_add_tail(&sws_ctx->list, &sws->broken_list);
		break;

	default:
		DRM_WARN("command is not supported\n");
	}

	spin_unlock_irqrestore(&sws->lock, flags);
	return len;
}

static int amdgpu_sws_local_release(struct inode *inode, struct file *file)
{
	i_size_write(inode, AMDGPU_SWS_DEBUGFS_SIZE);
	return 0;
}

static const struct file_operations amdgpu_sws_debugfs_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_sws_read_debugfs,
	.write   = amdgpu_sws_write_debugfs,
	.llseek = default_llseek,
	.release = amdgpu_sws_local_release,
};

int amdgpu_sws_init_debugfs(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	struct drm_minor *minor = adev_to_drm(adev)->primary;
	struct dentry *root = minor->debugfs_root;

	struct amdgpu_sws *sws;

	sws = &adev->sws;
	sws->ent = debugfs_create_file("amdgpu_sws", 0444,
				       root, adev,
				       &amdgpu_sws_debugfs_fops);

	i_size_write(sws->ent->d_inode, AMDGPU_SWS_DEBUGFS_SIZE);
#endif
	return 0;
}

int amdgpu_sws_deinit_debugfs(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	struct amdgpu_sws *sws;

	sws = &adev->sws;

	debugfs_remove(sws->ent);
#endif
	return 0;
}

int amdgpu_sws_init(struct amdgpu_device *adev)
{
	int i;
	u32 queue, pipe, mec;
	struct amdgpu_sws *sws;

	if (!cwsr_enable)
		return 0;

	sws = &adev->sws;
	INIT_LIST_HEAD(&sws->run_list);
	INIT_LIST_HEAD(&sws->idle_list);
	INIT_LIST_HEAD(&sws->broken_list);
	spin_lock_init(&sws->lock);

	for (i = 0; i < AMDGPU_MAX_COMPUTE_QUEUES; ++i) {
		queue = i % adev->gfx.mec.num_queue_per_pipe;
		pipe = (i / adev->gfx.mec.num_queue_per_pipe)
			% adev->gfx.mec.num_pipe_per_mec;
		mec = (i / adev->gfx.mec.num_queue_per_pipe)
			/ adev->gfx.mec.num_pipe_per_mec;

		if (mec  >= 1)
			break;

		if (test_bit(i, adev->gfx.mec.queue_bitmap))
			continue;

		set_bit(i, sws->avail_queue_bitmap);
	}

	sws->queue_num = bitmap_weight(sws->avail_queue_bitmap,
				       AMDGPU_MAX_COMPUTE_QUEUES);
	bitmap_zero(sws->run_queue_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
	bitmap_zero(sws->broken_queue_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
	sws->vmid_res_state = AMDGPU_SWS_HW_RES_AVAILABLE;
	sws->queue_res_state = AMDGPU_SWS_HW_RES_AVAILABLE;

	//reserve VMID for cwsr
	bitmap_zero(sws->vmid_bitmap, AMDGPU_SWS_MAX_VMID_NUM);
	for (i = 0; i < AMDGPU_SWS_MAX_VMID_NUM; ++i)
		sws->inv_eng[i] = AMDGPU_SWS_NO_INV_ENG;

	sws->sched = alloc_workqueue("sws_sched", WQ_UNBOUND,
				     WQ_UNBOUND_MAX_ACTIVE);
	if (IS_ERR(sws->sched)) {
		DRM_ERROR("failed to create work queue\n");
		return PTR_ERR(sws->sched);
	}

	INIT_WORK(&sws->sched_work, amdgpu_sws_sched_fn);

	if (amdgpu_sws_quantum == -1) {
		sws->quantum = AMDGPU_SWS_TIMER * NSEC_PER_MSEC;
		if (amdgpu_emu_mode == 1)
			sws->quantum *= 10;
	} else {
		sws->quantum = amdgpu_sws_quantum * NSEC_PER_MSEC;
	}

	hrtimer_init(&sws->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sws->timer.function = amdgpu_sws_sched_timer_fn;
	hrtimer_start(&sws->timer, ns_to_ktime(sws->quantum),
		      HRTIMER_MODE_REL);

	ida_init(&sws->doorbell_ida);
	ida_init(&sws->ring_idx_ida);

	DRM_INFO("Software scheudler is ready\n");
	return 0;
}

void amdgpu_sws_deinit(struct amdgpu_device *adev)
{
	unsigned long flags;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx, *tmp;

	if (!cwsr_enable)
		return;

	sws = &adev->sws;

	if (!cwsr_enable)
		return;

	amdgpu_sws_deinit_debugfs(adev);
	ida_destroy(&sws->doorbell_ida);
	hrtimer_cancel(&sws->timer);
	destroy_workqueue(sws->sched);
	spin_lock_irqsave(&sws->lock, flags);
	list_for_each_entry_safe(sws_ctx, tmp, &sws->run_list, list)
		amdgpu_sws_unmap(sws_ctx);
	spin_unlock_irqrestore(&sws->lock, flags);
}

void amdgpu_sws_clear_broken_queue(struct amdgpu_device *adev)
{
	unsigned long flags;
	struct amdgpu_sws *sws;
	struct amdgpu_sws_ctx *sws_ctx, *tmp;

	if (!cwsr_enable)
		return;

	sws = &adev->sws;

	spin_lock_irqsave(&sws->lock, flags);
	list_for_each_entry_safe(sws_ctx, tmp, &sws->broken_list, list) {
		list_del_init(&sws_ctx->list);
		list_add_tail(&sws_ctx->list, &sws->idle_list);
	}

	bitmap_zero(sws->broken_queue_bitmap, AMDGPU_MAX_COMPUTE_QUEUES);
	spin_unlock_irqrestore(&sws->lock, flags);
}
