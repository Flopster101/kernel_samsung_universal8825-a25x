/*
 * Copyright 2015, 2019-2021 Advanced Micro Devices, Inc.
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
 *
 */
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_bpmd.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#include "exynos_gpu_interface.h"
#endif /* CONFIG_DRM_SGPU_EXYNOS */

static void amdgpu_job_timedout(struct drm_sched_job *s_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_job->sched);
	struct amdgpu_job *job = to_amdgpu_job(s_job);
	struct amdgpu_task_info ti;
	struct amdgpu_device *adev = ring->adev;
	int r = 0;

	if (adev->runpm) {
		r = pm_runtime_get_sync(adev->ddev.dev);
		if (r < 0)
			goto pm_put;
		vangogh_lite_ifpo_power_on(adev);
	}

	if (amdgpu_fault_detect) {
		if (test_bit(FAULT_DETECT_RUNNING,
				&adev->fault_detect_flags)) {
			set_bit(FAULT_DETECT_JOB_TIMEOUT,
					&adev->fault_detect_flags);
			set_bit(FAULT_DETECT_WAKEUP,
					&adev->fault_detect_flags);
			wake_up(&adev->fault_detect_wake_up);
		}
	}

	if (ring->funcs->check_ring_done && s_job->s_fence->parent) {
		struct dma_fence *fence = s_job->s_fence->parent;

		job = to_amdgpu_job(s_job);
		DRM_INFO("%s: vmid %u job_id %lld FENCE drm %lld/%lld/%lld sgpu %lld/%lld\n",
			 ring->name, job->vmid, s_job->id, job->num_ibs,
			 s_job->s_fence->scheduled.context,
			 s_job->s_fence->finished.context,
			 s_job->s_fence->finished.seqno,
			 fence->context, fence->seqno);

		SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
				"%s: vmid %u job_id %lld FENCE drm %lld/%lld/%lld sgpu %lld/%lld\n",
				ring->name, job->vmid, s_job->id, job->num_ibs,
				s_job->s_fence->scheduled.context,
				s_job->s_fence->finished.context,
				s_job->s_fence->finished.seqno,
				fence->context, fence->seqno);

		ring->funcs->check_ring_done(ring);
	}

	if (ring->funcs->get_rreg(ring)	== ring->funcs->get_rptr(ring))
		goto out;

	if (sgpu_jobtimeout_to_panic) {
#ifdef CONFIG_DRM_SGPU_BPMD
		if (ring->adev->bpmd.funcs != NULL)
			sgpu_bpmd_dump(ring->adev);
#endif  /* CONFIG_DRM_SGPU_BPMD */
		list_add(&s_job->node, &s_job->sched->ring_mirror_list);
		panic("%s panic\n", __func__);
	}

	memset(&ti, 0, sizeof(struct amdgpu_task_info));

	if (amdgpu_gpu_recovery &&
	    amdgpu_ring_soft_recovery(ring, job->vmid, s_job->s_fence->parent)) {
		DRM_ERROR("ring %s timeout, but soft recovered\n",
			  s_job->sched->name);

		SGPU_LOG(adev, DMSG_INFO, DMSG_ETC, "ring %s timeout, but soft recovered\n",
				s_job->sched->name);

		goto out;
	}

	amdgpu_vm_get_task_info(ring->adev, job->pasid, &ti);
	DRM_ERROR("ring %s timeout, signaled seq=%u, emitted seq=%u\n",
		  job->base.sched->name,
		  atomic_read(&ring->fence_drv.last_seq),
		  ring->fence_drv.sync_seq);
	DRM_ERROR("Process information: process %s pid %d thread %s pid %d\n",
		  ti.process_name, ti.tgid, ti.task_name, ti.pid);

	SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
			"Process information: process %s pid %d thread %s pid %d\n",
			ti.process_name, ti.tgid, ti.task_name, ti.pid);

	if (amdgpu_device_should_recover_gpu(ring->adev)) {
		amdgpu_device_gpu_recover(ring->adev, job);
	} else {
		drm_sched_suspend_timeout(&ring->sched);
		if (amdgpu_sriov_vf(adev))
			adev->virt.tdr_debug = true;
	}

out:
	if (adev->runpm) {
		atomic_dec(&adev->in_ifpo);
		pm_runtime_mark_last_busy(adev->ddev.dev);
	}
pm_put:
	if (adev->runpm)
		pm_runtime_put_autosuspend(adev->ddev.dev);
}

int amdgpu_job_alloc(struct amdgpu_device *adev, unsigned num_ibs,
		     struct amdgpu_job **job, struct amdgpu_vm *vm)
{
	size_t size = sizeof(struct amdgpu_job);

	if (num_ibs == 0)
		return -EINVAL;

	size += sizeof(struct amdgpu_ib) * num_ibs;

	*job = kzalloc(size, GFP_KERNEL);
	if (!*job)
		return -ENOMEM;

	/*
	 * Initialize the scheduler to at least some ring so that we always
	 * have a pointer to adev.
	 */
	(*job)->base.sched = &adev->rings[0]->sched;
	(*job)->vm = vm;
	(*job)->ibs = (void *)&(*job)[1];
	(*job)->num_ibs = num_ibs;

	amdgpu_sync_create(&(*job)->sync);
	amdgpu_sync_create(&(*job)->sched_sync);
	(*job)->vram_lost_counter = atomic_read(&adev->vram_lost_counter);
	(*job)->vm_pd_addr = AMDGPU_BO_INVALID_OFFSET;

	(*job)->end_of_frame = false;

	return 0;
}

int amdgpu_job_alloc_with_ib(struct amdgpu_device *adev, unsigned size,
		enum amdgpu_ib_pool_type pool_type,
		struct amdgpu_job **job)
{
	int r;

	r = amdgpu_job_alloc(adev, 1, job, NULL);
	if (r)
		return r;

	r = amdgpu_ib_get(adev, NULL, size, pool_type, &(*job)->ibs[0]);
	if (r)
		kfree(*job);

	return r;
}

static void amdgpu_job_wa_pc_rings(struct amdgpu_ctx *ctx,
				   struct amdgpu_ib *ib)
{
	if (ib->flags & AMDGPU_IB_FLAG_PERF_COUNTER) {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->pc_gfx_rings |= (1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->pc_compute_rings |= (1 << ib->ring);
	} else {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->pc_gfx_rings &= ~(1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->pc_compute_rings &= ~(1 << ib->ring);
	}
}

static void amdgpu_job_wa_sqtt_rings(struct amdgpu_ctx *ctx,
				     struct amdgpu_ib *ib)
{
	if (ib->flags & AMDGPU_IB_FLAG_SQ_THREAD_TRACE) {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->sqtt_gfx_rings |= (1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->sqtt_compute_rings |= (1 << ib->ring);
	} else {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->sqtt_gfx_rings &= ~(1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->sqtt_compute_rings &= ~(1 << ib->ring);
	}
}

static void amdgpu_job_track_pc_sqtt(struct amdgpu_device *adev,
				     struct amdgpu_job *job)
{
	struct amdgpu_ctx *ctx = job->ctx;
	struct amdgpu_ib *ib;
	bool old_rings, new_rings;
	int i;

	job->pc_wa_enable = job->pc_wa_disable = false;
	job->sqtt_wa_enable = job->sqtt_wa_disable = false;
	for (i = 0; i < job->num_ibs; i++) {
		ib = &job->ibs[i];

		/* Are there any rings that have pc active */
		old_rings = (ctx->pc_gfx_rings || ctx->pc_compute_rings);
		amdgpu_job_wa_pc_rings(ctx, ib);
		new_rings = (ctx->pc_gfx_rings || ctx->pc_compute_rings);
		/* If old and new is not equal, it means there is a change
		 * in Perfcount active/inactive. */
		if (old_rings != new_rings) {
			/* If new_rings is true, enable workaround for this job.
			 * If new_rings is false, disable workaround after this job.*/
			if (new_rings) {
				job->pc_wa_enable = true;
				job->pc_wa_disable = false;
			} else
				job->pc_wa_disable = true;
		}

		old_rings = (ctx->sqtt_gfx_rings || ctx->sqtt_compute_rings);
		amdgpu_job_wa_sqtt_rings(ctx, ib);
		new_rings = (ctx->sqtt_gfx_rings || ctx->sqtt_compute_rings);
		if (old_rings != new_rings) {
			if (new_rings) {
				job->sqtt_wa_enable = true;
				job->sqtt_wa_disable = false;
			} else
				job->sqtt_wa_disable = true;
		}
	}
}

static void amdgpu_job_pc_workaround_enable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	/* if adev->pc_count is 0, workaround is disabled.
	 * Enable the workaround. */
	if (atomic_read(&adev->pc_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_PERFCOUNTER, 1);
	atomic_inc(&adev->pc_count);
}

static void amdgpu_job_pc_workaround_disable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (atomic_read(&adev->pc_count) == 0) {
		DRM_ERROR("Tracking Perfcounter active/inactive out of bound\n");
		return;
	}

	atomic_dec(&adev->pc_count);
	/* if adev->pc_count become 0, workaround is enabled.
	 * Disable the workaround. */
	if (atomic_read(&adev->pc_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_PERFCOUNTER, 0);
}

static void amdgpu_job_sqtt_workaround_enable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	/* if adev->sqtt_count is 0, workaround is disabled.
	 * Enable the workaround. */
	if (atomic_read(&adev->sqtt_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_SQ_THREAD_TRACE, 1);
	atomic_inc(&adev->sqtt_count);
}

static void amdgpu_job_sqtt_workaround_disable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (atomic_read(&adev->sqtt_count) == 0) {
		DRM_ERROR("Tracking SQTT active/inactive out of bound\n");
		return;
	}

	atomic_dec(&adev->sqtt_count);
	/* if adev->sqtt_count become 0, workaround is enabled.
	 * Disable the workaround. */
	if (atomic_read(&adev->sqtt_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_SQ_THREAD_TRACE, 0);
}

void amdgpu_job_free_resources(struct amdgpu_job *job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(job->base.sched);
	struct dma_fence *f;
	unsigned i;

	/* use sched fence if available */
	f = job->base.s_fence ? &job->base.s_fence->finished : job->fence;

	for (i = 0; i < job->num_ibs; ++i)
		amdgpu_ib_free(ring->adev, &job->ibs[i], f);
}

static void amdgpu_job_free_cb(struct drm_sched_job *s_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_job->sched);
	struct amdgpu_device *adev = ring->adev;
	struct amdgpu_job *job = to_amdgpu_job(s_job);
	bool fault_detect_notify = false;
	ktime_t scheduled, finished;

	if (sgpu_unscheduled_job_debug)
		cancel_work_sync(&job->wait_on_scheduled_work);

	if (amdgpu_fault_detect) {
		if (ring->funcs->type == AMDGPU_RING_TYPE_GFX &&
				(atomic_dec_return(&adev->gfx_job_cnt) == 0)) {
			clear_bit(FAULT_DETECT_GFX_ACTIVE,
					&adev->fault_detect_flags);
			fault_detect_notify = true;
		} else if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE &&
				(atomic_dec_return(&adev->compute_job_cnt)
						== 0)) {
			clear_bit(FAULT_DETECT_COMPUTE_ACTIVE,
					&adev->fault_detect_flags);
			fault_detect_notify = true;
		}

		/* If both GFX and Compute are idle inform to fault detect */
		if (fault_detect_notify
			&& (!test_bit(FAULT_DETECT_GFX_ACTIVE,
				&adev->fault_detect_flags) &&
					!test_bit(FAULT_DETECT_COMPUTE_ACTIVE,
						&adev->fault_detect_flags))) {

			set_bit(FAULT_DETECT_WAKEUP,
					&adev->fault_detect_flags);
			wake_up(&adev->fault_detect_wake_up);
		}
	}

	if (sgpu_amigo_user_time &&
	    ring->funcs->type == AMDGPU_RING_TYPE_GFX) {
		scheduled = s_job->s_fence->scheduled.timestamp;
		finished = s_job->s_fence->finished.timestamp;

#ifdef CONFIG_DRM_SGPU_EXYNOS
		exynos_amigo_interframe_hw_update(scheduled, finished,
						  job->end_of_frame);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
	}


	drm_sched_job_cleanup(s_job);
	atomic_dec(&ring->num_jobs);

	dma_fence_put(job->fence);
	amdgpu_sync_free(&job->sync);
	amdgpu_sync_free(&job->sched_sync);

	/* Is workaround not needed after this job */
	if (adev->asic_type == CHIP_VANGOGH_LITE) {
		mutex_lock(&adev->pc_sqtt_mutex);
		if (job->pc_wa_disable)
			amdgpu_job_pc_workaround_disable(ring);
		if (job->sqtt_wa_disable)
			amdgpu_job_sqtt_workaround_disable(ring);
		mutex_unlock(&adev->pc_sqtt_mutex);
	}

	kfree(job);

	SGPU_LOG(adev, DMSG_INFO, DMSG_ETC, "amdgpu_job_free_cb, active_jobs :%d",
									atomic_read(&adev->ifpo_active_jobs));
	if (adev->runpm) {
		mutex_lock(&adev->ifpo_mutex);
		if (adev->ifpo_runtime_control &&
				atomic_read(&adev->in_ifpo) == 0 &&
				atomic_read(&adev->dev->power.usage_count) == 0 &&
				atomic_read(&adev->ifpo_active_jobs) == 0)
			vangogh_lite_ifpo_power_off(adev);
		mutex_unlock(&adev->ifpo_mutex);
	}
}

void amdgpu_job_free(struct amdgpu_job *job)
{
	amdgpu_job_free_resources(job);

	dma_fence_put(job->fence);
	amdgpu_sync_free(&job->sync);
	amdgpu_sync_free(&job->sched_sync);

	kfree(job);
}

int amdgpu_job_submit(struct amdgpu_job *job, struct drm_sched_entity *entity,
		      void *owner, struct dma_fence **f)
{
	enum drm_sched_priority priority;
	struct amdgpu_ring *ring;
	int r;

	if (!f)
		return -EINVAL;

	r = drm_sched_job_init(&job->base, entity, owner);
	if (r)
		return r;

	*f = dma_fence_get(&job->base.s_fence->finished);
	amdgpu_job_free_resources(job);
	drm_sched_entity_push_job(&job->base, entity);

	ring = to_amdgpu_ring(entity->rq->sched);
	priority = job->base.s_priority;
	atomic_inc(&ring->num_jobs);

	return 0;
}

int amdgpu_job_submit_direct(struct amdgpu_job *job, struct amdgpu_ring *ring,
			     struct dma_fence **fence)
{
	int r;

	job->base.sched = &ring->sched;
	r = amdgpu_ib_schedule(ring, job->num_ibs, job->ibs, NULL, fence);
	job->fence = dma_fence_get(*fence);
	if (r)
		return r;

	/* update ring fence seq by SW */
	if (job->ifh_mode &&
	    (ring->funcs->type == AMDGPU_RING_TYPE_GFX ||
	     ring->funcs->type == AMDGPU_RING_TYPE_SDMA ||
	     ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE))
		amdgpu_fence_driver_force_completion(ring);

	amdgpu_job_free(job);
	return 0;
}

static struct dma_fence *amdgpu_job_dependency(struct drm_sched_job *sched_job,
					       struct drm_sched_entity *s_entity)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_entity->rq->sched);
	struct amdgpu_job *job = to_amdgpu_job(sched_job);
	struct amdgpu_vm *vm = job->vm;
	struct dma_fence *fence;
	int r;

	fence = amdgpu_sync_get_fence(&job->sync);
	if (fence && drm_sched_dependency_optimized(fence, s_entity)) {
		r = amdgpu_sync_fence(&job->sched_sync, fence);
		if (r)
			DRM_ERROR("Error adding fence (%d)\n", r);
	}

	while (fence == NULL && vm && !job->vmid) {
		r = amdgpu_vmid_grab(vm, ring, &job->sync,
				     &job->base.s_fence->finished,
				     job);
		if (r)
			DRM_ERROR("Error getting VM ID (%d)\n", r);

		fence = amdgpu_sync_get_fence(&job->sync);
	}

	return fence;
}

static struct dma_fence *amdgpu_job_run(struct drm_sched_job *sched_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(sched_job->sched);
	struct amdgpu_device *adev = ring->adev;
	struct dma_fence *fence = NULL, *finished;
	struct amdgpu_job *job;
	int r = 0;

	if (adev->runpm) {
		r = pm_runtime_get_sync(adev->ddev.dev);
		if (r < 0)
			goto pm_put;
		r = 0;
		vangogh_lite_ifpo_power_on(adev);
	}

	atomic_dec(&adev->ifpo_active_jobs);
	job = to_amdgpu_job(sched_job);
	finished = &job->base.s_fence->finished;

	BUG_ON(amdgpu_sync_peek_fence(&job->sync, NULL));

	trace_amdgpu_sched_run_job(job);

	/* Is workaround needed for this job */
	if (adev->asic_type == CHIP_VANGOGH_LITE) {
		if (!amdgpu_in_reset(adev)) {
			mutex_lock(&adev->pc_sqtt_mutex);
			amdgpu_job_track_pc_sqtt(adev, job);
			if (job->pc_wa_enable)
				amdgpu_job_pc_workaround_enable(ring);
			if (job->sqtt_wa_enable)
				amdgpu_job_sqtt_workaround_enable(ring);
			mutex_unlock(&adev->pc_sqtt_mutex);
		}
	}

	if (job->vram_lost_counter != atomic_read(&ring->adev->vram_lost_counter))
		dma_fence_set_error(finished, -ECANCELED);/* skip IB as well if VRAM lost */

	if (finished->error < 0) {
		DRM_INFO("Skip scheduling IBs!\n");
		dma_fence_signal(finished);
	} else if (job->vm->process_flags == PF_EXITING) {
		DRM_INFO("Skip scheduling IBs PF_EXITING!\n");
		dma_fence_set_error(finished, -ENOEXEC);
		dma_fence_signal(finished);
	} else {
		r = amdgpu_ib_schedule(ring, job->num_ibs, job->ibs, job,
				       &fence);
		if (r) {
			DRM_ERROR("Error scheduling IBs (%d)\n", r);
		} else {
			if (amdgpu_fault_detect) {
				if (ring->funcs->type ==
						AMDGPU_RING_TYPE_GFX) {

					atomic_inc(&adev->gfx_job_cnt);

					set_bit(
					FAULT_DETECT_GFX_ACTIVE,
					&adev->fault_detect_flags);

					if (!test_bit(FAULT_DETECT_RUNNING,
						&adev->fault_detect_flags)) {

						set_bit(
						FAULT_DETECT_WAKEUP,
						&adev->fault_detect_flags
						);

						wake_up(
						&adev->fault_detect_wake_up);
					}
				} else if (ring->funcs->type
						== AMDGPU_RING_TYPE_COMPUTE) {

					atomic_inc(&adev->compute_job_cnt);

					set_bit(
					FAULT_DETECT_COMPUTE_ACTIVE,
					&adev->fault_detect_flags);

					if (!test_bit(FAULT_DETECT_RUNNING,
						&adev->fault_detect_flags)) {

						set_bit(
						FAULT_DETECT_WAKEUP,
						&adev->fault_detect_flags);

						wake_up(
						&adev->fault_detect_wake_up);
					}
				}
			}
		}
	}
	/* if gpu reset, hw fence will be replaced here */
	dma_fence_put(job->fence);
	job->fence = dma_fence_get(fence);

	amdgpu_job_free_resources(job);

	/* update ring fence seq by SW */
	if (job->ifh_mode &&
	    (ring->funcs->type == AMDGPU_RING_TYPE_GFX ||
	     ring->funcs->type == AMDGPU_RING_TYPE_SDMA ||
	     ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE))
		amdgpu_fence_driver_force_completion(ring);

	fence = r ? ERR_PTR(r) : fence;

pm_put:
	if (adev->runpm) {
		atomic_dec(&adev->in_ifpo);
		pm_runtime_put_autosuspend(adev->ddev.dev);
	}

	return fence;
}

#define to_drm_sched_job(sched_job)		\
		container_of((sched_job), struct drm_sched_job, queue_node)

void amdgpu_job_stop_all_jobs_on_sched(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *s_job;
	struct drm_sched_entity *s_entity = NULL;
	int i;

	/* Signal all jobs not yet scheduled */
	for (i = DRM_SCHED_PRIORITY_COUNT - 1; i >= DRM_SCHED_PRIORITY_MIN; i--) {
		struct drm_sched_rq *rq = &sched->sched_rq[i];

		if (!rq)
			continue;

		spin_lock(&rq->lock);
		list_for_each_entry(s_entity, &rq->entities, list) {
			while ((s_job = to_drm_sched_job(spsc_queue_pop(&s_entity->job_queue)))) {
				struct drm_sched_fence *s_fence = s_job->s_fence;

				dma_fence_signal(&s_fence->scheduled);
				dma_fence_set_error(&s_fence->finished, -EHWPOISON);
				dma_fence_signal(&s_fence->finished);
			}
		}
		spin_unlock(&rq->lock);
	}

	/* Signal all jobs already scheduled to HW */
	list_for_each_entry(s_job, &sched->ring_mirror_list, node) {
		struct drm_sched_fence *s_fence = s_job->s_fence;

		dma_fence_set_error(&s_fence->finished, -EHWPOISON);
		dma_fence_signal(&s_fence->finished);
	}
}

const struct drm_sched_backend_ops amdgpu_sched_ops = {
	.dependency = amdgpu_job_dependency,
	.run_job = amdgpu_job_run,
	.timedout_job = amdgpu_job_timedout,
	.free_job = amdgpu_job_free_cb
};
