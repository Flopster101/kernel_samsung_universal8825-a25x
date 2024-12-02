// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/dma-fence.h>
#include <linux/sec_detect.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_vblank.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_fbdev.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_writeback.h>
#include <exynos_display_common.h>
#include <exynos_drm_migov.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_tui.h>
#include <exynos_drm_recovery.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_helper.h>
#include <linux/delay.h>
#endif


#define CREATE_TRACE_POINTS
#include <dpu_trace.h>

#define DRIVER_NAME	"exynos-drmdpu"
#define DRIVER_DESC	"Samsung SoC DRM"
#define DRIVER_DATE	"20110530"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

#if IS_ENABLED(CONFIG_DRM_PANEL_MCD_COMMON)
int usdm_no_display;
module_param(usdm_no_display, int, S_IRUGO);
EXPORT_SYMBOL(usdm_no_display);
int usdm_bypass_display;
EXPORT_SYMBOL(usdm_bypass_display);
int usdm_commit_retry;
EXPORT_SYMBOL(usdm_commit_retry);
#endif

#define for_each_crtc_in_state(__state, crtc, __i)			\
	for ((__i) = 0;							\
	     (__i) < (__state)->dev->mode_config.num_crtc;		\
	     (__i)++)							\
		for_each_if ((__state)->crtcs[__i].ptr &&		\
			     ((crtc) = (__state)->crtcs[__i].ptr,	\
		/* Only to avoid unused-but-set-variable warning */	\
			     (void)(crtc), 1))

inline void *
dev_get_exynos_properties(struct drm_device *drm_dev, int obj_type)
{
	struct exynos_drm_private *priv = drm_dev->dev_private;

	if (!priv) {
		pr_err("%s: invalid param, priv is null\n", __func__);
		return NULL;
	}

	/* TODO: change if-condition to array-index */
	if (obj_type == DRM_MODE_OBJECT_CRTC)
		return &priv->crtc_props;
	else if (obj_type == DRM_MODE_OBJECT_PLANE)
		return &priv->plane_props;
	else if (obj_type == DRM_MODE_OBJECT_CONNECTOR)
		return &priv->connector_props;

	DRM_DEV_ERROR(drm_dev->dev, "no properties for object_type(0x%x)\n", obj_type);
	return NULL;
}

struct exynos_drm_priv_state *
exynos_drm_get_priv_state(struct drm_atomic_state *state)
{
	struct exynos_drm_private *priv = state->dev->dev_private;
	struct drm_private_state *priv_state;

	priv_state = drm_atomic_get_private_obj_state(state, &priv->obj);
	if (IS_ERR(priv_state))
		return ERR_CAST(priv_state);

	return to_exynos_priv_state(priv_state);
}

static unsigned int find_set_bits_mask(unsigned int mask, size_t count)
{
	unsigned int out = 0;
	int i;

	for (i = count; i > 0; i--) {
		int bit = ffs(mask) - 1;

		if (bit < 0)
			return 0;

		mask &= ~BIT(bit);
		out |= BIT(bit);
	}

	return out;
}

static unsigned int exynos_drm_crtc_get_win_cnt(struct drm_crtc_state *crtc_state)
{
	unsigned int num_planes;

	if (!crtc_state->active)
		return 0;

	num_planes = hweight32(crtc_state->plane_mask);

	/* at least one window is required for color map, if active
	   and there are no planes on DSI path*/
	if (crtc_needs_colormap(crtc_state))
		num_planes = 1;

	return num_planes;
}

static void
exynos_drm_crtc_alloc_windows(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct exynos_drm_private *priv;
	unsigned int win_mask;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state;

		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);

		if (!new_exynos_crtc_state->win_inserted)
			continue;

		/* allocate windows after swap state phase */
		win_mask = new_exynos_crtc_state->reserved_win_mask;
		priv = dev->dev_private;
		mutex_lock(&priv->lock);
		priv->available_win_mask &= ~win_mask;
		mutex_unlock(&priv->lock);
	}
}

static int
exynos_atomic_check_windows(struct drm_device *dev, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	unsigned int win_mask;
	bool need_wait = false;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state;
		struct exynos_drm_crtc_state *old_exynos_crtc_state;
		unsigned int old_win_cnt, new_win_cnt;

		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_crtc_state = to_exynos_crtc_state(old_crtc_state);

		old_win_cnt = old_crtc_state->active ? hweight32(old_exynos_crtc_state->reserved_win_mask) : 0;
		new_win_cnt = exynos_drm_crtc_get_win_cnt(new_crtc_state);

		if (old_win_cnt == new_win_cnt)
			continue;

		pr_debug("%s: win cnt changed: %d -> %d\n", crtc->name, old_win_cnt, new_win_cnt);

		if (old_win_cnt > new_win_cnt) {
			const unsigned int curr_win_mask = new_exynos_crtc_state->reserved_win_mask;

			if (new_win_cnt)
				win_mask = find_set_bits_mask(curr_win_mask,
						old_win_cnt - new_win_cnt);
			else /* freeing all windows */
				win_mask = curr_win_mask;

			WARN(!win_mask, "%s: invalid reserved mask (0x%x) for win_cnt (%d->%d)",
					crtc->name, curr_win_mask, old_win_cnt, new_win_cnt);

			new_exynos_crtc_state->freed_win_mask |= win_mask;
			new_exynos_crtc_state->reserved_win_mask &= ~win_mask;
		} else {
			struct exynos_drm_private *priv = dev->dev_private;

			struct drm_crtc_commit *old_commit = old_crtc_state->commit;

retry:
			mutex_lock(&priv->lock);
			win_mask = find_set_bits_mask(priv->available_win_mask, new_win_cnt - old_win_cnt);
			if (!win_mask) {
				DRM_WARN("%s: No windows available for req win cnt=%d->%d (0x%x)\n",
						crtc->name, old_win_cnt, new_win_cnt,
						priv->available_win_mask);
				mutex_unlock(&priv->lock);
				if (old_exynos_crtc_state->freed_win_mask && !need_wait) {
					wait_for_completion_timeout(&old_commit->hw_done, msecs_to_jiffies(20));
					need_wait = true;
					pr_info("%s, re-check the window resource\n", __func__);
					goto retry;
				}
				return -ENOENT;
			}
			pr_debug("%s: current win_mask=0x%x new=0x%x avail=0x%x\n", crtc->name,
					new_exynos_crtc_state->reserved_win_mask, win_mask,
					priv->available_win_mask);

			mutex_unlock(&priv->lock);
			new_exynos_crtc_state->reserved_win_mask |= win_mask;
			new_exynos_crtc_state->win_inserted = true;
		}
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);
		const unsigned long reserved_win_mask =
			new_exynos_crtc_state->reserved_win_mask;
		unsigned long visible_zpos = 0;
		struct drm_plane *plane;
		const struct drm_plane_state *plane_state;
		unsigned int pmask, bit, win_cnt;

		new_exynos_crtc_state->visible_win_mask = 0;

		if (!new_crtc_state->plane_mask)
			continue;

		drm_atomic_crtc_state_for_each_plane_state(plane, plane_state,
				new_crtc_state)
			if (plane_state->visible)
				visible_zpos |= BIT(plane_state->normalized_zpos);

		pmask = 1;
		win_cnt = get_plane_num(dev);
		for_each_set_bit(bit, &reserved_win_mask, win_cnt) {
			if (pmask & visible_zpos)
				new_exynos_crtc_state->visible_win_mask |= BIT(bit);
			pmask <<= 1;
		}
		pr_debug("%s: visible_win_mask(0x%x)\n", __func__,
				new_exynos_crtc_state->visible_win_mask);
	}

	return 0;
}

static void exynos_atomic_prepare_partial_update(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	struct exynos_partial *partial;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		partial = exynos_crtc->partial;

		if (!new_crtc_state->active || !partial)
			continue;

		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_crtc_state = to_exynos_crtc_state(old_crtc_state);

		if (drm_atomic_crtc_needs_modeset(new_crtc_state) ||
				(new_exynos_crtc_state->seamless_modeset
				& SEAMLESS_MODESET_MRES)) {
			const struct exynos_drm_connector_state *exynos_conn_state =
				crtc_get_exynos_connector_state(state, new_crtc_state);

			if (exynos_conn_state) {
				const struct exynos_display_mode *exynos_mode =
					&exynos_conn_state->exynos_mode;

				exynos_partial_initialize(partial,
						&new_crtc_state->mode, exynos_mode);
			}
		}

		if (!partial->enabled)
			continue;

		exynos_partial_prepare(partial, old_exynos_crtc_state,
				new_exynos_crtc_state);
	}
}

static int exynos_atomic_add_affected_objects(struct drm_device *dev,
					       struct drm_atomic_state *state)
{
	int i;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		conn = crtc_get_conn(new_crtc_state, DRM_MODE_CONNECTOR_WRITEBACK);
		if (conn) {
			new_conn_state = drm_atomic_get_connector_state(state, conn);
			if (IS_ERR(new_conn_state))
				return PTR_ERR(new_conn_state);
		}
	}

	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
int exynos_drm_check_frame_skip(struct drm_atomic_state *state)
{
	int i;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		if (mcd_drm_check_commit_retry(exynos_crtc, __func__)) {
			usleep_range(16600, 16700);
			return -EAGAIN;
		}
	}

	return 0;
}

#endif


int exynos_atomic_check(struct drm_device *dev, struct drm_atomic_state *state)
{
	int ret;

	DPU_ATRACE_BEGIN("exynos_atomic_check");

	ret = exynos_atomic_add_affected_objects(dev, state);
	if (ret)
		goto out;

	ret = exynos_drm_atomic_check_tui(state);
	if (ret)
		goto out;

	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		goto out;

	exynos_atomic_prepare_partial_update(state);

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	ret = exynos_drm_check_frame_skip(state);
	if (ret)
		goto out;
#endif
	ret = drm_atomic_normalize_zpos(dev, state);
	if (ret)
		goto out;

	ret = drm_atomic_helper_check_planes(dev, state);
	if (ret)
		goto out;

	exynos_drm_init_resource_cnt();
	ret = exynos_atomic_check_windows(dev, state);
	if (ret)
		goto out;

out:
	DPU_ATRACE_END("exynos_atomic_check");
	return ret;
}

static struct
drm_private_state *exynos_atomic_duplicate_priv_state(struct drm_private_obj *obj)
{
	const struct exynos_drm_priv_state *old_state =
					to_exynos_priv_state(obj->state);
	struct exynos_drm_priv_state *new_state;

	new_state = kmemdup(old_state, sizeof(*old_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &new_state->base);
	return &new_state->base;
}

static void exynos_atomic_destroy_priv_state(struct drm_private_obj *obj,
		struct drm_private_state *state)
{
	struct exynos_drm_priv_state *priv_state = to_exynos_priv_state(state);

	kfree(priv_state);
}

static const struct drm_private_state_funcs exynos_priv_state_funcs = {
	.atomic_duplicate_state = exynos_atomic_duplicate_priv_state,
	.atomic_destroy_state = exynos_atomic_destroy_priv_state,
};

/* See also: drm_atomic_helper_wait_for_fences() */
#define FENCE_TIMEOUT_MSEC	(3200)			/* 3200msec */
#define NUM_VBLANK		(5)			/* vblank wait count */
#define DFT_FPS			(60)			/* default fps */
static int exynos_drm_atomic_helper_wait_for_fences(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool pre_swap)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct dma_fence *fence;
	const struct dma_fence_ops *ops;
	ktime_t fence_wait_start;
	s64 fence_wait_ms, debug_timeout_ms;
	int i, ret, fps;

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		if (!new_plane_state->fence)
			continue;

		WARN_ON(!new_plane_state->fb);

		/*
		 * If waiting for fences pre-swap (ie: nonblock), userspace can
		 * still interrupt the operation. Instead of blocking until the
		 * timer expires, make the wait interruptible.
		 */

		fence = new_plane_state->fence;
		exynos_crtc = to_exynos_crtc(new_plane_state->crtc);
		DPU_EVENT_LOG("WAIT_PLANE_IN_FENCE", exynos_crtc, EVENT_FLAG_FENCE,
				FENCE_FMT, FENCE_ARG(fence));

		new_crtc_state = new_plane_state->crtc->state;
		fps = drm_mode_vrefresh(&new_crtc_state->adjusted_mode) ? : DFT_FPS;
		debug_timeout_ms = NUM_VBLANK * MSEC_PER_SEC / fps;

		fence_wait_start = ktime_get();
		ret = dma_fence_wait_timeout(fence, pre_swap,
				msecs_to_jiffies(debug_timeout_ms));
		if (ret <= 0) {
			DPU_EVENT_LOG("TIMEOUT_PLANE_IN_FENCE", exynos_crtc,
					EVENT_FLAG_FENCE, "in_fence_wait: %lldmsec",
					debug_timeout_ms);
			ops = fence->ops;
			pr_info("[%s] plane-%lu fence waiting timeout(%lldmsec)\n",
					__func__, plane->index,	debug_timeout_ms);
			pr_info("[%s] driver=%s, timeline =%s\n", __func__,
					ops->get_driver_name(fence),
					ops->get_timeline_name(fence));
		}

		ret = dma_fence_wait(fence, pre_swap);
		fence_wait_ms = ktime_ms_delta(ktime_get(), fence_wait_start);
		if (ret)
			return ret;

		if (fence_wait_ms > FENCE_TIMEOUT_MSEC) {
			DPU_EVENT_LOG("ELAPSED_PLANE_IN_FENCE", exynos_crtc,
					EVENT_FLAG_FENCE, "in_fence_wait: %lldmsec",
					fence_wait_ms);
		}

		dma_fence_put(new_plane_state->fence);
		new_plane_state->fence = NULL;
	}

	return 0;
}

static bool check_recovery_state(const struct drm_crtc* crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->is_recovering &&
			exynos_crtc->ops->is_recovering(exynos_crtc))
		return true;

	return false;
}

static void commit_tail(struct drm_atomic_state *old_state)
{
	int i;
	struct drm_device *dev = old_state->dev;
	const struct drm_mode_config_helper_funcs *funcs;
	const struct drm_crtc *crtc;
	bool recovery = false;

	funcs = dev->mode_config.helper_private;

	DPU_ATRACE_BEGIN("wait_for_in_fences");
	exynos_drm_atomic_helper_wait_for_fences(dev, old_state, false);
	DPU_ATRACE_END("wait_for_in_fences");

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
	exynos_atomic_commit_prepare_buf_sanity(old_state);
#endif

	for_each_crtc_in_state(old_state, crtc, i) {
		const struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

		if (exynos_crtc->migov)
			exynos_migov_update_ems_fence_cnt(exynos_crtc->migov);

		recovery = check_recovery_state(crtc);
	}

	if (unlikely(recovery))
		pr_info("recovery, skip for all preceding commits\n");
	else
		drm_atomic_helper_wait_for_dependencies(old_state);

	if (funcs && funcs->atomic_commit_tail)
		funcs->atomic_commit_tail(old_state);
	else
		drm_atomic_helper_commit_tail(old_state);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void commit_kthread_work(struct kthread_work *work)
{
	struct exynos_drm_crtc_state *new_exynos_crtc_state =
		container_of(work, struct exynos_drm_crtc_state, commit_work);
	struct drm_atomic_state *old_state = new_exynos_crtc_state->old_state;

	commit_tail(old_state);
}

static void commit_work(struct work_struct *work)
{
	struct drm_atomic_state *old_state =
		container_of(work, struct drm_atomic_state, commit_work);

	commit_tail(old_state);
}

static void exynos_atomic_queue_work(struct drm_atomic_state *old_state, bool nonblock)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i;

	/*
	 * our driver don't support multiple display update with same commit
	 * but if need, consider to split work per display
	 */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);

		new_exynos_crtc_state->old_state = old_state;
		kthread_init_work(&new_exynos_crtc_state->commit_work,
				commit_kthread_work);
		kthread_queue_work(&exynos_crtc->worker,
				&new_exynos_crtc_state->commit_work);
		return;
	}

	/* fallback to regular commit work if we get here (no crtcs in commit state) */
	INIT_WORK(&old_state->commit_work, commit_work);
	queue_work(system_highpri_wq, &old_state->commit_work);
}

int exynos_atomic_commit(struct drm_device *dev, struct drm_atomic_state *state, bool nonblock)
{
	const struct drm_crtc *crtc;
	int ret;
	int i;

	DPU_ATRACE_BEGIN("exynos_atomic_commit");
	ret = drm_atomic_helper_setup_commit(state, nonblock);
	if (ret)
		goto err;

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		goto err;

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret) {
		drm_atomic_helper_cleanup_planes(dev, state);
		goto err;
	}

	exynos_drm_crtc_alloc_windows(dev, state);

	for_each_crtc_in_state(state, crtc, i) {
		struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
		if (exynos_crtc->freq_hop)
			exynos_crtc->freq_hop->update_freq_hop(exynos_crtc);
	}

	/*
	 * Everything below can be run asynchronously without the need to grab
	 * any modeset locks at all under one condition: It must be guaranteed
	 * that the asynchronous work has either been cancelled (if the driver
	 * supports it, which at least requires that the framebuffers get
	 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
	 * before the new state gets committed on the software side with
	 * drm_atomic_helper_swap_state().
	 *
	 * This scheme allows new atomic state updates to be prepared and
	 * checked in parallel to the asynchronous completion of the previous
	 * update. Which is important since compositors need to figure out the
	 * composition of the next frame right after having submitted the
	 * current layout.
	 *
	 * NOTE: Commit work has multiple phases, first hardware commit, then
	 * cleanup. We want them to overlap, hence need system_unbound_wq to
	 * make sure work items don't artificially stall on each another.
	 */

	drm_atomic_state_get(state);
	if (!nonblock)
		commit_tail(state);
	else
		exynos_atomic_queue_work(state, nonblock);

err:
	DPU_ATRACE_END("exynos_atomic_commit");
	return ret;
}

static const struct vm_operations_struct exynos_drm_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static int exynos_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file->driver_priv = file_priv;

	return 0;
}

static void exynos_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	kfree(file->driver_priv);
	file->driver_priv = NULL;
}

static void exynos_drm_lastclose(struct drm_device *dev)
{
	exynos_drm_fbdev_restore_mode(dev);
}

static const struct file_operations exynos_drm_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= exynos_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.release	= drm_release,
};

static struct drm_driver exynos_drm_driver = {
	.driver_features	   = DRIVER_MODESET | DRIVER_ATOMIC |
				     DRIVER_RENDER | DRIVER_GEM,
	.open			   = exynos_drm_open,
	.lastclose		   = exynos_drm_lastclose,
	.postclose		   = exynos_drm_postclose,
	.gem_free_object_unlocked  = exynos_drm_gem_free_object,
	.gem_vm_ops		   = &exynos_drm_gem_vm_ops,
	.dumb_create		   = exynos_drm_gem_dumb_create,
	.dumb_map_offset	   = exynos_drm_gem_dumb_map_offset,
	.prime_handle_to_fd	   = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	   = exynos_drm_gem_prime_fd_to_handle,
	.gem_prime_import	   = exynos_drm_gem_prime_import,
	.gem_prime_import_sg_table = exynos_drm_gem_prime_import_sg_table,
	.fops			   = &exynos_drm_driver_fops,
	.name			   = DRIVER_NAME,
	.desc			   = DRIVER_DESC,
	.date			   = DRIVER_DATE,
	.major			   = DRIVER_MAJOR,
	.minor			   = DRIVER_MINOR,
};

/* forward declaration */
static struct platform_driver exynos_drm_platform_driver;

struct exynos_drm_driver_info {
	struct platform_driver *driver;
	unsigned int flags;
};

#define DRM_COMPONENT_DRIVER	BIT(0)	/* supports component framework */
#define DRM_VIRTUAL_DEVICE	BIT(1)	/* create virtual platform device */
#define DRM_DMA_DEVICE		BIT(2)	/* can be used for dma allocations */

#define DRV_PTR(drv, cond) (IS_ENABLED(cond) ? &drv : NULL)

/*
 * Connector drivers should not be placed before associated crtc drivers,
 * because connector requires pipe number of its crtc during initialization.
 */
static struct exynos_drm_driver_info exynos_drm_drivers[] = {
	{
		DRV_PTR(dpp_driver, CONFIG_DRM_SAMSUNG_DPP),
		DRM_COMPONENT_DRIVER
	}, {
		/* writeback connector should be bound before bts init. */
		DRV_PTR(writeback_driver, CONFIG_DRM_SAMSUNG_WB),
		DRM_COMPONENT_DRIVER
	}, {
		DRV_PTR(decon_driver, CONFIG_DRM_SAMSUNG_DECON),
		DRM_COMPONENT_DRIVER | DRM_DMA_DEVICE
	}, {
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	/*
	 * dsimfc_driver should be registered before dsim_driver,
	 *  because dsimfc drvdata need to be connected to dsim_driver.
	 */
		DRV_PTR(dsimfc_driver, CONFIG_EXYNOS_DMA_DSIMFC),
		0 /* non-DRM */
	}, {
#endif
		DRV_PTR(dsim_driver, CONFIG_DRM_SAMSUNG_DSI),
		DRM_COMPONENT_DRIVER
	}, {
#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DP)
		DRV_PTR(dp_driver, CONFIG_DRM_SAMSUNG_DP),
		DRM_COMPONENT_DRIVER
	}, {
#endif
		&exynos_drm_platform_driver,
		DRM_VIRTUAL_DEVICE
	}
};

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *exynos_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];
		struct device *p = NULL, *d;

		if (!info->driver || !(info->flags & DRM_COMPONENT_DRIVER))
			continue;

		while ((d = platform_find_device_by_driver(p,
						&info->driver->driver))) {
			put_device(p);
			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
}

static int exynos_drm_bind(struct device *dev)
{
	struct exynos_drm_private *private;
	struct drm_encoder *encoder;
	struct drm_device *drm;
	struct exynos_drm_priv_state *priv_state;
	u32 wb_mask = 0;
	u32 encoder_mask = 0;
	int ret, win_cnt;

	drm = drm_dev_alloc(&exynos_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	private = kzalloc(sizeof(struct exynos_drm_private), GFP_KERNEL);
	if (!private) {
		ret = -ENOMEM;
		goto err_free_drm;
	}

	init_waitqueue_head(&private->wait);
	mutex_init(&private->lock);
	timer_setup(&private->debug_timer, exynos_drm_gem_timeout_detector, 0);

	dev_set_drvdata(dev, drm);
	drm->dev_private = (void *)private;

	drm_mode_config_init(drm);

	exynos_drm_mode_config_init(drm);

	priv_state = kzalloc(sizeof(*priv_state), GFP_KERNEL);
	if (!priv_state) {
		ret = -ENOMEM;
		goto err_mode_config_cleanup;
	}

	drm_atomic_private_obj_init(drm, &private->obj, &priv_state->base,
			&exynos_priv_state_funcs);

	/* Try to bind all sub drivers. */
	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_priv_state_cleanup;

	win_cnt = get_plane_num(drm);

	mutex_lock(&private->lock);
	private->available_win_mask = BIT(win_cnt) - 1;
	mutex_unlock(&private->lock);

	drm_for_each_encoder(encoder, drm) {
		if (encoder->encoder_type == DRM_MODE_ENCODER_VIRTUAL)
			wb_mask |= drm_encoder_mask(encoder);

		encoder_mask |= drm_encoder_mask(encoder);
	}

	drm_for_each_encoder(encoder, drm) {
		if (encoder->encoder_type == DRM_MODE_ENCODER_VIRTUAL) {
			struct writeback_device *wb = enc_to_wb_dev(encoder);

			encoder->possible_clones = encoder_mask;
			encoder->possible_crtcs =
				exynos_drm_get_possible_crtcs(encoder, wb->output_type);
		} else {
			encoder->possible_clones = drm_encoder_mask(encoder) |
				wb_mask;
		}

		pr_info("encoder[%d] type(0x%x) possible_clones(0x%x)\n",
				encoder->index, encoder->encoder_type,
				encoder->possible_clones);
	}

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret)
		goto err_unbind_all;

	drm_mode_config_reset(drm);

	/*
	 * enable drm irq mode.
	 * - with irq_enabled = true, we can use the vblank feature.
	 *
	 * P.S. note that we wouldn't use drm irq handler but
	 *	just specific driver own one instead because
	 *	drm framework supports only one irq handler.
	 */
	drm->irq_enabled = true;

	/*
	 * If true, vblank interrupt will be disabled immediately when the
	 * refcount drops to zero, as opposed to via the vblank disable
	 * timer.
	 */
	drm->vblank_disable_immediate = true;

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm);

	ret = exynos_drm_fbdev_init(drm);
	if (ret)
		goto err_cleanup_poll;

	/* register the DRM device */
	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_cleanup_fbdev;

	return 0;

err_cleanup_fbdev:
	exynos_drm_fbdev_fini(drm);
err_cleanup_poll:
	drm_kms_helper_poll_fini(drm);
err_unbind_all:
	component_unbind_all(drm->dev, drm);
err_priv_state_cleanup:
       drm_atomic_private_obj_fini(&private->obj);
err_mode_config_cleanup:
	drm_mode_config_cleanup(drm);
err_free_drm:
	drm_dev_put(drm);
	kfree(private);

	return ret;
}

static void exynos_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct exynos_drm_private *private = drm->dev_private;

	drm_dev_unregister(drm);

	drm_atomic_private_obj_fini(&private->obj);
	exynos_drm_fbdev_fini(drm);
	drm_kms_helper_poll_fini(drm);

	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);

	kfree(drm->dev_private);
	drm->dev_private = NULL;
	dev_set_drvdata(dev, NULL);

	drm_dev_put(drm);
}

static const struct component_master_ops exynos_drm_ops = {
	.bind		= exynos_drm_bind,
	.unbind		= exynos_drm_unbind,
};

static int exynos_drm_platform_probe(struct platform_device *pdev)
{
	struct component_match *match;

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	match = exynos_drm_match_add(&pdev->dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(&pdev->dev, &exynos_drm_ops,
					       match);
}

static int exynos_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &exynos_drm_ops);
	return 0;
}

static void exynos_drm_platform_shutdown(struct platform_device *pdev)
{
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_device *dev;
	struct exynos_drm_crtc_state *exynos_crtc_state;

	pr_info("%s\n", __func__);

	dev = platform_get_drvdata(pdev);
	if (!dev) {
		pr_err("%s: failed to get platform drive data\n", __func__);
		return;
	}

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		pr_err("%s: failed to alloc shutdown state\n", __func__);
		return;
	}

	state->acquire_ctx = &ctx;

	drm_for_each_crtc(crtc, dev) {
		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			pr_err("%s: failed to get crtc state\n", __func__);
			goto free;
		}

		crtc_state->active = false;

		exynos_crtc_state = to_exynos_crtc_state(crtc_state);
		exynos_crtc_state->dqe_fd = -1;
	}

	drm_atomic_commit(state);

free:
	drm_atomic_state_put(state);
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);
}

static struct platform_driver exynos_drm_platform_driver = {
	.probe	= exynos_drm_platform_probe,
	.remove	= exynos_drm_platform_remove,
	.shutdown = exynos_drm_platform_shutdown,
	.driver	= {
		.name	= "exynos-drm",
	},
};

static void exynos_drm_unregister_devices(void)
{
	int i;

	for (i = ARRAY_SIZE(exynos_drm_drivers) - 1; i >= 0; --i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];
		struct device *dev;

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		while ((dev = platform_find_device_by_driver(NULL,
						&info->driver->driver))) {
			put_device(dev);
			platform_device_unregister(to_platform_device(dev));
		}
	}
}

static int exynos_drm_register_devices(void)
{
	struct platform_device *pdev;
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver || !(info->flags & DRM_VIRTUAL_DEVICE))
			continue;

		pdev = platform_device_register_simple(
					info->driver->driver.name, -1, NULL, 0);
		if (IS_ERR(pdev))
			goto fail;
	}

	return 0;
fail:
	exynos_drm_unregister_devices();
	return PTR_ERR(pdev);
}

static void exynos_drm_unregister_drivers(void)
{
	int i;

	for (i = ARRAY_SIZE(exynos_drm_drivers) - 1; i >= 0; --i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver)
			continue;

		platform_driver_unregister(info->driver);
	}
}

static int exynos_drm_register_drivers(void)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(exynos_drm_drivers); ++i) {
		struct exynos_drm_driver_info *info = &exynos_drm_drivers[i];

		if (!info->driver)
			continue;

		ret = platform_driver_register(info->driver);
		if (ret)
			goto fail;
	}
	return 0;
fail:
	exynos_drm_unregister_drivers();
	return ret;
}

static int __init exynos_drm_init(void)
{
	int ret;

	if (sec_needs_decon)
		return 0;

	SEC_DETECT_LOG("Initialized Exynos DPU driver for usdm\n");
	ret = exynos_drm_register_devices();
	if (ret)
		return ret;

	ret = exynos_drm_register_drivers();
	if (ret)
		goto err_unregister_pdevs;

	return 0;

err_unregister_pdevs:
	exynos_drm_unregister_devices();

	return ret;
}

static void __exit exynos_drm_exit(void)
{
	exynos_drm_unregister_drivers();
	exynos_drm_unregister_devices();
}

module_init(exynos_drm_init);
module_exit(exynos_drm_exit);

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_AUTHOR("Seung-Woo Kim <sw0312.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC DRM Driver");
MODULE_LICENSE("GPL");
