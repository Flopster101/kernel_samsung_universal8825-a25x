// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_crtc.c
 *
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

#include <uapi/linux/sched/types.h>
#include <uapi/drm/drm.h>
#include <linux/circ_buf.h>
#include <linux/dma-fence.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_vblank.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_print.h>
#include <samsung_drm.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_drv.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_tui.h>
#include <exynos_drm_migov.h>
#include <exynos_drm_format.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_partial.h>

#include <dqe_cal.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_decon.h>
#include <mcd_drm_helper.h>
#endif

static void exynos_drm_crtc_atomic_enable(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_crtc_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	drm_crtc_vblank_on(crtc);

	if (exynos_crtc->ops->enable)
		exynos_crtc->ops->enable(exynos_crtc, old_crtc_state);
}

static void exynos_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_crtc_state *old_crtc_state)
{
	struct drm_atomic_state *old_state = old_crtc_state->state;
	const struct drm_plane_helper_funcs *plane_funcs;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	drm_atomic_crtc_state_for_each_plane(plane, old_crtc_state) {
		plane_funcs = plane->helper_private;
		old_plane_state = drm_atomic_get_old_plane_state(old_state, plane);
		if (old_plane_state && plane_funcs->atomic_disable)
			plane_funcs->atomic_disable(plane, old_plane_state);
	}

	if (exynos_crtc->ops->disable)
		exynos_crtc->ops->disable(exynos_crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}

	drm_crtc_vblank_off(crtc);
}

static void exynos_crtc_check_input_bpc(struct drm_crtc *crtc,
				struct drm_crtc_state *crtc_state)
{
	struct exynos_drm_crtc_state *new_exynos_state =
						to_exynos_crtc_state(crtc_state);
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct drm_plane *plane;
	const struct drm_plane_state *plane_state;
	uint32_t max_bpc = 8; /* initial bpc value */
	bool atc_enabled = false;

	if (new_exynos_state->wb_type == EXYNOS_WB_SWB) {
		new_exynos_state->in_bpc = max_bpc;
		return;
	}

	exynos_dqe_state(exynos_crtc->dqe, DQE_REG_ATC, &atc_enabled);
	if (atc_enabled)
		max_bpc = 10; /* extend to 10 bit for ATC dimming duration */

	drm_atomic_crtc_state_for_each_plane_state(plane, plane_state, crtc_state) {
		const struct drm_format_info *info;
		const struct dpu_fmt *fmt_info;

		info = plane_state->fb->format;
		fmt_info = dpu_find_fmt_info(info->format);
		if (fmt_info->bpc > max_bpc)
			max_bpc = fmt_info->bpc;

	}
	new_exynos_state->in_bpc = max_bpc;
}

static int exynos_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *crtc_state)
{
	int ret = 0;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	DRM_DEBUG("%s +\n", __func__);

	if (!crtc_state->enable)
		return ret;

	if (exynos_crtc->ops->atomic_check)
		ret = exynos_crtc->ops->atomic_check(exynos_crtc, crtc_state);

	exynos_crtc_check_input_bpc(crtc, crtc_state);

	DRM_DEBUG("%s -\n", __func__);

	return ret;
}

static void exynos_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	if (mcd_drm_check_commit_skip(exynos_crtc, __func__))
		return;
#endif
	if (exynos_crtc->ops->atomic_begin)
		exynos_crtc->ops->atomic_begin(exynos_crtc);
}

static void exynos_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_crtc_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct drm_crtc_state *new_crtc_state = crtc->state;
	struct exynos_drm_crtc_state *new_exynos_state, *old_exynos_state;
	struct exynos_partial *partial = exynos_crtc->partial;

	if (partial) {
		new_exynos_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_state = to_exynos_crtc_state(old_crtc_state);
		exynos_partial_update(partial, &old_exynos_state->partial_region,
				&new_exynos_state->partial_region);
	}

	if (exynos_crtc->ops->atomic_flush)
		exynos_crtc->ops->atomic_flush(exynos_crtc, old_crtc_state);
}

static enum drm_mode_status exynos_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->mode_valid)
		return exynos_crtc->ops->mode_valid(exynos_crtc, mode);

	return MODE_OK;
}

static bool exynos_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->mode_fixup)
		return exynos_crtc->ops->mode_fixup(exynos_crtc, mode,
						    adjusted_mode);

	return true;
}

uint32_t crtc_get_bts_fps(const struct drm_crtc *crtc)
{
	const struct drm_connector *conn =
		crtc_get_conn(crtc->state, DRM_MODE_CONNECTOR_DSI);
	const struct exynos_drm_connector_state *exynos_conn_state;

	if (!conn)
		return drm_mode_vrefresh(&crtc->state->adjusted_mode);

	exynos_conn_state = to_exynos_connector_state(conn->state);

	return exynos_conn_state->exynos_mode.bts_fps;
}

static void exynos_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	const struct drm_crtc_state *crtc_state = crtc->state;
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (exynos_crtc->ops->mode_set)
		exynos_crtc->ops->mode_set(exynos_crtc, &crtc_state->mode,
				&crtc_state->adjusted_mode);

	if (exynos_crtc_state->bts_fps_ptr) {
		u32 bts_fps = crtc_get_bts_fps(crtc);

		if (put_user(bts_fps, exynos_crtc_state->bts_fps_ptr))
			pr_err("%s: failed to put bts_fps(%d) to user_ptr(%p)\n",
				__func__, bts_fps, exynos_crtc_state->bts_fps_ptr);
	}
}

static const struct drm_crtc_helper_funcs exynos_crtc_helper_funcs = {
	.mode_valid	= exynos_crtc_mode_valid,
	.mode_fixup	= exynos_crtc_mode_fixup,
	.mode_set_nofb	= exynos_crtc_mode_set_nofb,
	.atomic_check	= exynos_crtc_atomic_check,
	.atomic_begin	= exynos_crtc_atomic_begin,
	.atomic_flush	= exynos_crtc_atomic_flush,
	.atomic_enable	= exynos_drm_crtc_atomic_enable,
	.atomic_disable	= exynos_drm_crtc_atomic_disable,
};

void exynos_crtc_handle_event(struct exynos_drm_crtc *exynos_crtc)
{
	struct dma_fence *fence;
	struct drm_crtc *crtc = &exynos_crtc->base;
	struct drm_pending_vblank_event *event = crtc->state->event;
	unsigned long flags;

	if (!event)
		return;
	crtc->state->event = NULL;

	WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	fence = event->base.fence;
	if (fence)
		DPU_EVENT_LOG("ARM_CRTC_OUT_FENCE", exynos_crtc, EVENT_FLAG_FENCE,
				FENCE_FMT, FENCE_ARG(fence));
	drm_crtc_arm_vblank_event(crtc, event);
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

static void exynos_drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->migov)
		kfree(exynos_crtc->migov);

	exynos_hibernation_destroy(exynos_crtc->hibernation);

	if (exynos_crtc->thread)
		kthread_stop(exynos_crtc->thread);

	drm_crtc_cleanup(crtc);
	kfree(exynos_crtc);
}

static int exynos_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->enable_vblank)
		return exynos_crtc->ops->enable_vblank(exynos_crtc);

	return 0;
}

static void exynos_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->disable_vblank)
		exynos_crtc->ops->disable_vblank(exynos_crtc);
}

static u32 exynos_drm_crtc_get_vblank_counter(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->get_vblank_counter)
		return exynos_crtc->ops->get_vblank_counter(exynos_crtc);

	return 0;
}

static void exynos_drm_crtc_destroy_state(struct drm_crtc *crtc,
					struct drm_crtc_state *state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;

	exynos_crtc_state = to_exynos_crtc_state(state);
	drm_property_blob_put(exynos_crtc_state->partial);
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(exynos_crtc_state);
}

static void exynos_drm_crtc_reset(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;

	if (crtc->state) {
		exynos_drm_crtc_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
	}

	exynos_crtc_state = kzalloc(sizeof(*exynos_crtc_state), GFP_KERNEL);
	if (exynos_crtc_state) {
		crtc->state = &exynos_crtc_state->base;
		crtc->state->crtc = crtc;
	} else {
		pr_err("failed to allocate exynos crtc state\n");
	}
}

static struct drm_crtc_state *
exynos_drm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;
	struct exynos_drm_crtc_state *copy;

	exynos_crtc_state = to_exynos_crtc_state(crtc->state);
	copy = kzalloc(sizeof(*copy), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_crtc_state, sizeof(*copy));
	copy->wb_type = EXYNOS_WB_NONE;
	copy->seamless_modeset = 0;
	copy->freed_win_mask = 0;
	copy->win_inserted = false;
	copy->modeset_only = false;
	copy->skip_frameupdate = false;
	copy->tui_changed = false;
	copy->bts_fps_ptr = NULL;
	copy->boost_bts_fps = 0;

	if (copy->partial)
		drm_property_blob_get(copy->partial);

	__drm_atomic_helper_crtc_duplicate_state(crtc, &copy->base);

	return &copy->base;
}

static int
exynos_drm_replace_property_blob_from_id(struct drm_device *dev,
					 struct drm_property_blob **blob,
					 uint64_t blob_id,
					 ssize_t expected_size)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id != 0) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (!new_blob)
			return -EINVAL;

		if (new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}

	drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);

	return 0;
}

static int exynos_drm_crtc_set_property(struct drm_crtc *crtc,
					struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t val)
{
	const struct exynos_drm_crtc_properties *p;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	int ret = 0;

	if (!crtc) {
		pr_err("%s: invalid param, crtc is null\n", __func__);
		return -EINVAL;
	}

	p = dev_get_exynos_properties(crtc->dev, DRM_MODE_OBJECT_CRTC);
	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	exynos_crtc_state = to_exynos_crtc_state(state);

	if ((p->color_mode) && (property == p->color_mode)) {
		exynos_crtc_state->color_mode = val;
		state->color_mgmt_changed = true;
	} else if ((p->render_intent) && (property == p->render_intent)) {
		exynos_crtc_state->render_intent = val;
		state->color_mgmt_changed = true;
	} else if ((p->adjusted_vblank) && (property == p->adjusted_vblank)) {
		ret = 0; /* adj_vblank setting is not supported  */
	} else if ((p->dsr_status) && (property == p->dsr_status)) {
		exynos_crtc_state->dsr_status = val;
	} else if ((p->modeset_only) && (property == p->modeset_only)) {
		exynos_crtc_state->modeset_only = val;
	} else if ((p->partial) && (property == p->partial)) {
		ret = exynos_drm_replace_property_blob_from_id(crtc->dev,
				&exynos_crtc_state->partial,
				val, sizeof(struct drm_clip_rect));
	} else if ((p->dqe_fd) && (property == p->dqe_fd)) {
		exynos_crtc_state->dqe_fd = U642I64(val);
		state->color_mgmt_changed = true;
	} else if ((p->bts_fps) && (property == p->bts_fps)) {
		exynos_crtc_state->bts_fps_ptr = u64_to_user_ptr(val);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

#define NEXT_ADJ_VBLANK_OFFSET	2
static int get_next_adjusted_vblank_timestamp(struct drm_crtc *crtc, uint64_t *val)
{
	struct drm_vblank_crtc *vblank;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	int count = 0;
	ktime_t timestamp, cur_time, diff;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	s64 elapsed_time;
	ktime_t s_time = ktime_get();
#endif

	if (drm_crtc_index(crtc) >= crtc->dev->num_crtcs) {
		pr_err("%s: invalid param, crtc index is out of range\n", __func__);
		return -EINVAL;
	}

	spin_lock(&crtc->commit_lock);
	if (!list_empty(&crtc->commit_list))
		count++;
	spin_unlock(&crtc->commit_lock);

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	elapsed_time = ktime_to_us(ktime_sub(ktime_get(), s_time));
	if ((count >= 10) || (elapsed_time > 50000))
		pr_info("%s: elapsed time: %lld, count: %d\n", __func__, elapsed_time, count);
#endif

	vblank = &crtc->dev->vblank[drm_crtc_index(crtc)];
	timestamp = vblank->time + (count + NEXT_ADJ_VBLANK_OFFSET) *
		vblank->framedur_ns;
	cur_time = ktime_get();
	if (cur_time > timestamp) {
		diff = cur_time - timestamp;
		timestamp += (DIV_ROUND_DOWN_ULL(diff, vblank->framedur_ns) +
				NEXT_ADJ_VBLANK_OFFSET) * vblank->framedur_ns;
	}
	*val = timestamp;

	DPU_EVENT_LOG("NEXT_ADJ_VBLANK", exynos_crtc, 0, "timestamp(%lld)", timestamp);

	return 0;
}

static int exynos_drm_crtc_get_property(struct drm_crtc *crtc,
					const struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	const struct exynos_drm_crtc_properties *p;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;

	if (!crtc) {
		pr_err("%s: invalid param, crtc is null\n", __func__);
		return -EINVAL;
	}

	p = dev_get_exynos_properties(crtc->dev, DRM_MODE_OBJECT_CRTC);
	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	exynos_crtc_state = to_exynos_crtc_state(state);
	exynos_crtc = to_exynos_crtc(crtc);

	if ((p->color_mode) && (property == p->color_mode))
		*val = exynos_crtc_state->color_mode;
	else if ((p->render_intent) && (property == p->render_intent))
		*val = exynos_crtc_state->render_intent;
	else if ((p->adjusted_vblank) && (property == p->adjusted_vblank))
		return get_next_adjusted_vblank_timestamp(crtc, val);
	else if ((p->dsr_status) && (property == p->dsr_status))
		*val = exynos_crtc_state->dsr_status;
	else if ((p->modeset_only) && (property == p->modeset_only))
		*val = exynos_crtc_state->modeset_only;
	else if ((p->partial) && (property == p->partial))
		*val = exynos_crtc_state->partial ?
			exynos_crtc_state->partial->base.id : 0;
	else if ((p->dqe_fd) && (property == p->dqe_fd))
		*val = I642U64(exynos_crtc_state->dqe_fd);
	else if ((p->bts_fps) && (property == p->bts_fps))
		*val = exynos_crtc->bts->fps;
	else
		return -EINVAL;

	return 0;
}

static void exynos_drm_crtc_print_state(struct drm_printer *p,
					const struct drm_crtc_state *state)
{
	const struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(state->crtc);
	const struct exynos_drm_crtc_state *exynos_crtc_state =
						to_exynos_crtc_state(state);

	drm_printf(p, "\treserved_win_mask=0x%x\n",
			exynos_crtc_state->reserved_win_mask);
	drm_printf(p, "\tdsr_status=%d\n", exynos_crtc_state->dsr_status);
	drm_printf(p, "\tcolor_mode=%d\n", exynos_crtc_state->color_mode);
	drm_printf(p, "\trender_intent=%d\n", exynos_crtc_state->render_intent);
	drm_printf(p, "\tmodeset_only=%d\n", exynos_crtc_state->modeset_only);
	drm_printf(p, "\tdqe_fd=%lld\n", exynos_crtc_state->dqe_fd);
	if (exynos_crtc_state->partial) {
		struct drm_clip_rect *partial_region =
			(struct drm_clip_rect *)exynos_crtc_state->partial->data;

		drm_printf(p, "\tblob(%d) partial region[%d %d %d %d]\n",
				exynos_crtc_state->partial->base.id,
				partial_region->x1, partial_region->y1,
				partial_region->x2 - partial_region->x1,
				partial_region->y2 - partial_region->y1);
	} else {
		drm_printf(p, "\tno partial region request\n");
	}

	if (exynos_crtc->ops->atomic_print_state)
		exynos_crtc->ops->atomic_print_state(p, exynos_crtc);
}

static int exynos_drm_crtc_late_register(struct drm_crtc *crtc)
{
	int ret = 0;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	ret = dpu_init_debug(exynos_crtc);
	if (ret)
		return ret;

	if (exynos_crtc->ops->late_register)
		ret = exynos_crtc->ops->late_register(exynos_crtc);

	return ret;
}

static void exysno_drm_crtc_early_unregister(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	dpu_deinit_debug(exynos_crtc);
}

/**
 * exynos_drm_crtc_add_crc_entry - Add entry with CRC information for a frame
 * @crtc: CRTC to which the frame belongs
 * @has_frame: whether this entry has a frame number to go with
 * @frame: number of the frame these CRCs are about
 * @crcs: array of CRC values, with length matching #drm_crtc_crc.values_cnt
 *
 * NOTE : this funcation is for temporary using
 * and need to replace to dem_crtc_add_crc_entry.
 */
int exynos_drm_crtc_add_crc_entry(struct drm_crtc *crtc, bool has_frame,
			   uint32_t frame, uint32_t *crcs)
{
	struct drm_crtc_crc *crc = &crtc->crc;
	struct drm_crtc_crc_entry *entry;
	int head, tail;
	unsigned long flags;

	spin_lock_irqsave(&crc->lock, flags);

	/* Caller may not have noticed yet that userspace has stopped reading */
	if (!crc->entries) {
		spin_unlock_irqrestore(&crc->lock, flags);
		return -EINVAL;
	}

	head = crc->head;
	tail = crc->tail;

	if (CIRC_SPACE(head, tail, DRM_CRC_ENTRIES_NR) < 1) {
		bool was_overflow = crc->overflow;

		crc->overflow = true;
		spin_unlock_irqrestore(&crc->lock, flags);

		if (!was_overflow)
			DRM_ERROR("Overflow of CRC buffer, userspace reads too slow.\n");

		return -ENOBUFS;
	}

	entry = &crc->entries[head];
	entry->frame = frame;
	entry->has_frame_counter = has_frame;
	memcpy(&entry->crcs, crcs, sizeof(*crcs) * crc->values_cnt);

	head = (head + 1) & (DRM_CRC_ENTRIES_NR - 1);
	crc->head = head;

	spin_unlock_irqrestore(&crc->lock, flags);

	wake_up_interruptible(&crc->wq);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_drm_crtc_add_crc_entry);

static const char * const exynos_crc_source[] = {"auto"};
static int exynos_crtc_parse_crc_source(const char *source)
{
	if (!source)
		return 0;

	if (strcmp(source, "auto") == 0)
		return 1;

	return -EINVAL;
}

int exynos_drm_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (!source) {
		DRM_ERROR("CRC source for crtc(%d) was not set\n", crtc->index);
		exynos_crtc->crc_state = EXYNOS_DRM_CRC_STOP;
		return 0;
	}

	if (exynos_crtc->crc_state == EXYNOS_DRM_CRC_STOP)
		exynos_crtc->crc_state = EXYNOS_DRM_CRC_REQ;

	return 0;
}

int exynos_drm_crtc_verify_crc_source(struct drm_crtc *crtc, const char *source,
				      size_t *values_cnt)
{
	int idx = exynos_crtc_parse_crc_source(source);

	if (idx < 0) {
		DRM_INFO("Unknown or invalid CRC source for CRTC%d\n", crtc->index);
		return -EINVAL;
	}

	*values_cnt = 3;
	return 0;
}

const char *const *exynos_drm_crtc_get_crc_sources(struct drm_crtc *crtc,
						   size_t *count)
{
	*count = ARRAY_SIZE(exynos_crc_source);
	return exynos_crc_source;
}

static const struct drm_crtc_funcs exynos_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.destroy		= exynos_drm_crtc_destroy,
	.reset			= exynos_drm_crtc_reset,
	.atomic_duplicate_state	= exynos_drm_crtc_duplicate_state,
	.atomic_destroy_state	= exynos_drm_crtc_destroy_state,
	.atomic_set_property	= exynos_drm_crtc_set_property,
	.atomic_get_property	= exynos_drm_crtc_get_property,
	.atomic_print_state     = exynos_drm_crtc_print_state,
	.enable_vblank		= exynos_drm_crtc_enable_vblank,
	.disable_vblank		= exynos_drm_crtc_disable_vblank,
	.get_vblank_counter	= exynos_drm_crtc_get_vblank_counter,
	.late_register		= exynos_drm_crtc_late_register,
	.early_unregister	= exysno_drm_crtc_early_unregister,
	.set_crc_source		= exynos_drm_crtc_set_crc_source,
	.verify_crc_source		= exynos_drm_crtc_verify_crc_source,
	.get_crc_sources		= exynos_drm_crtc_get_crc_sources,
};

static int
exynos_drm_crtc_create_color_mode_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);
	static const struct drm_prop_enum_list color_mode_list[] = {
		{ HAL_COLOR_MODE_NATIVE, "Native" },
		{ HAL_COLOR_MODE_DCI_P3, "DCI-P3" },
		{ HAL_COLOR_MODE_SRGB, "SRGB" },
                { HAL_COLOR_MODE_STANDARD_BT601_625, "BT601-625" },
                { HAL_COLOR_MODE_STANDARD_BT601_625_UNADJUSTED, "BT601-625-UNADJUSTED" },
                { HAL_COLOR_MODE_STANDARD_BT601_525, "BT601-525" },
                { HAL_COLOR_MODE_STANDARD_BT601_525_UNADJUSTED, "BT601-525-UNADJUSTED" },
                { HAL_COLOR_MODE_STANDARD_BT709, "BT709" },
                { HAL_COLOR_MODE_ADOBE_RGB, "ADOBE-RGB" },
                { HAL_COLOR_MODE_DISPLAY_P3, "DISPLAY-P3" },
                { HAL_COLOR_MODE_BT2020, "BT2020" },
                { HAL_COLOR_MODE_BT2100_PQ, "BT2100-PQ" },
                { HAL_COLOR_MODE_BT2100_HLG, "BT2100-HLG" },
                { HAL_COLOR_MODE_CUSTOM_0, "CUSTOM-0" },
                { HAL_COLOR_MODE_CUSTOM_1, "CUSTOM-1" },
                { HAL_COLOR_MODE_CUSTOM_2, "CUSTOM-2" },
                { HAL_COLOR_MODE_CUSTOM_3, "CUSTOM-3" },
                { HAL_COLOR_MODE_CUSTOM_4, "CUSTOM-4" },
                { HAL_COLOR_MODE_CUSTOM_5, "CUSTOM-5" },
                { HAL_COLOR_MODE_CUSTOM_6, "CUSTOM-6" },
                { HAL_COLOR_MODE_CUSTOM_7, "CUSTOM-7" },
                { HAL_COLOR_MODE_CUSTOM_8, "CUSTOM-8" },
                { HAL_COLOR_MODE_CUSTOM_9, "CUSTOM-9" },
                { HAL_COLOR_MODE_CUSTOM_10, "CUSTOM-10" },
                { HAL_COLOR_MODE_CUSTOM_11, "CUSTOM-11" },
                { HAL_COLOR_MODE_CUSTOM_12, "CUSTOM-12" },
                { HAL_COLOR_MODE_CUSTOM_13, "CUSTOM-13" },
                { HAL_COLOR_MODE_CUSTOM_14, "CUSTOM-14" },
                { HAL_COLOR_MODE_CUSTOM_15, "CUSTOM-15" },
                { HAL_COLOR_MODE_CUSTOM_16, "CUSTOM-16" },
                { HAL_COLOR_MODE_CUSTOM_17, "CUSTOM-17" },
                { HAL_COLOR_MODE_CUSTOM_18, "CUSTOM-18" },
                { HAL_COLOR_MODE_CUSTOM_19, "CUSTOM-19" },
                { HAL_COLOR_MODE_CUSTOM_20, "CUSTOM-20" },
                { HAL_COLOR_MODE_CUSTOM_21, "CUSTOM-21" },
                { HAL_COLOR_MODE_CUSTOM_22, "CUSTOM-22" },
                { HAL_COLOR_MODE_CUSTOM_23, "CUSTOM-23" },
                { HAL_COLOR_MODE_CUSTOM_24, "CUSTOM-24" },
                { HAL_COLOR_MODE_CUSTOM_25, "CUSTOM-25" },
                { HAL_COLOR_MODE_CUSTOM_26, "CUSTOM-26" },
                { HAL_COLOR_MODE_CUSTOM_27, "CUSTOM-27" },
                { HAL_COLOR_MODE_CUSTOM_28, "CUSTOM-28" },
                { HAL_COLOR_MODE_CUSTOM_29, "CUSTOM-29" },
	};

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->color_mode = drm_property_create_enum(drm_dev, 0, "color mode",
			color_mode_list, ARRAY_SIZE(color_mode_list));
	if (!p->color_mode)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_crtc_create_render_intent_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);
	static const struct drm_prop_enum_list render_intent_list[] = {
		{ HAL_RENDER_INTENT_COLORIMETRIC, "Colorimetric" },
		{ HAL_RENDER_INTENT_ENHANCE, "Enhance" },
		{ HAL_RENDER_INTENT_TONE_MAP_COLORIMETRIC, "Tone Map Colorimetric" },
		{ HAL_RENDER_INTENT_TONE_MAP_ENHANCE, "Tone Map Enhance" },
		{ HAL_RENDER_INTENT_CUSTOM_0, "CUSTOM-0" },
		{ HAL_RENDER_INTENT_CUSTOM_1, "CUSTOM-1" },
		{ HAL_RENDER_INTENT_CUSTOM_2, "CUSTOM-2" },
		{ HAL_RENDER_INTENT_CUSTOM_3, "CUSTOM-3" },
	};

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->render_intent = drm_property_create_enum(drm_dev, 0, "render intent",
			render_intent_list, ARRAY_SIZE(render_intent_list));
	if (!p->render_intent)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_crtc_create_vblank_timeline_property(
		struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->adjusted_vblank =
		drm_property_create_signed_range(drm_dev, 0, "adjusted_vblank",
			0, LONG_MAX);
	if (!p->adjusted_vblank)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_crtc_create_operation_mode_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);
	static const struct drm_prop_enum_list op_mode_list[] = {
		{ EXYNOS_VIDEO_MODE, "video_mode" },
		{ EXYNOS_COMMAND_MODE, "command_mode" },
	};

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->operation_mode = drm_property_create_enum(drm_dev, DRM_MODE_PROP_IMMUTABLE,
			"operation_mode", op_mode_list, ARRAY_SIZE(op_mode_list));
	if (!p->operation_mode)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_crtc_create_dsr_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->dsr_status = drm_property_create_bool(drm_dev, 0, "dsr_status");
	if (!p->dsr_status)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_crtc_create_modeset_only_property(
		struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->modeset_only = drm_property_create_bool(drm_dev, 0, "modeset_only");
	if (!p->modeset_only)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_crtc_create_partial_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->partial = drm_property_create(drm_dev, DRM_MODE_PROP_BLOB,
			"partial_region", 0);
	if (!p->partial)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_crtc_create_dqe_fd(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->dqe_fd = drm_property_create_signed_range(drm_dev, 0, "DQE_FD", -1,
			1023);
	if (!p->dqe_fd)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_crtc_create_bts_fps_property(struct drm_device *drm_dev)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);

	if (!p) {
		pr_err("%s: invalid param, properties is null\n", __func__);
		return -EINVAL;
	}

	p->bts_fps = drm_property_create_range(drm_dev, 0, "bts_fps",
			0, U64_MAX);
	if (!p->bts_fps)
		return -ENOMEM;

	return 0;
}

int exynos_drm_crtc_create_properties(struct drm_device *drm_dev)
{
	int ret = 0;

	ret = exynos_drm_crtc_create_color_mode_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_render_intent_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_vblank_timeline_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_operation_mode_property(drm_dev);
	if (ret)
		return ret;

	/* decon self refresh for VR */
	ret = exynos_drm_crtc_create_dsr_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_modeset_only_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_dqe_fd(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_partial_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_crtc_create_bts_fps_property(drm_dev);
	if (ret)
		return ret;

	return ret;
}

struct exynos_drm_crtc *exynos_drm_crtc_create(struct drm_device *drm_dev,
					struct drm_plane *plane,
					enum exynos_drm_output_type type,
					enum exynos_drm_op_mode op_mode,
					const struct exynos_drm_crtc_ops *ops,
					void *ctx)
{
	struct exynos_drm_crtc_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_CRTC);
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	struct sched_param param = {
		.sched_priority = 20
	};
	int ret;

	exynos_crtc = kzalloc(sizeof(*exynos_crtc), GFP_KERNEL);
	if (!exynos_crtc)
		return ERR_PTR(-ENOMEM);

	exynos_crtc->possible_type = type;
	exynos_crtc->ops = ops;
	exynos_crtc->ctx = ctx;

	exynos_crtc->dqe = exynos_dqe_register(ctx);

	crtc = &exynos_crtc->base;

	ret = drm_crtc_init_with_planes(drm_dev, crtc, plane, NULL,
					&exynos_crtc_funcs, "exynos-crtc");
	if (ret < 0)
		goto err_crtc;

	drm_crtc_helper_add(crtc, &exynos_crtc_helper_funcs);

	/* valid only for LCD display */
	if (!crtc->index) {
		drm_object_attach_property(&crtc->base, p->color_mode, HAL_COLOR_MODE_NATIVE);
		drm_object_attach_property(&crtc->base, p->render_intent,
				HAL_RENDER_INTENT_COLORIMETRIC);
		drm_object_attach_property(&crtc->base, p->dqe_fd, I642U64(-1));
		drm_object_attach_property(&crtc->base, p->adjusted_vblank, 0);
		drm_object_attach_property(&crtc->base, p->dsr_status, 0);
		drm_object_attach_property(&crtc->base, p->operation_mode, op_mode);
		drm_object_attach_property(&crtc->base, p->modeset_only, 0);
		drm_object_attach_property(&crtc->base, p->partial, 0);
		drm_object_attach_property(&crtc->base, p->bts_fps, 0);
	}

	drm_crtc_enable_color_mgmt(crtc, 0, false, 0);

	kthread_init_worker(&exynos_crtc->worker);
	exynos_crtc->thread = kthread_run(kthread_worker_fn, &exynos_crtc->worker,
			"crtc%d_kthread", crtc->index);
	if (IS_ERR(exynos_crtc->thread)) {
		DRM_ERROR("failed to run display thread of crtc(%d)\n", crtc->index);
		ret = PTR_ERR(exynos_crtc->thread);
		goto err_crtc;
	}
	sched_setscheduler_nocheck(exynos_crtc->thread, SCHED_FIFO, &param);

	exynos_crtc->hibernation = exynos_hibernation_register(exynos_crtc);
	exynos_crtc->migov = exynos_migov_register(exynos_crtc);
	exynos_tui_register(&exynos_crtc->base);

	return exynos_crtc;

err_crtc:
	plane->funcs->destroy(plane);
	kfree(exynos_crtc);
	return ERR_PTR(ret);
}

struct exynos_drm_crtc *exynos_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum exynos_drm_output_type out_type)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev)
		if (to_exynos_crtc(crtc)->possible_type == out_type)
			return to_exynos_crtc(crtc);

	return ERR_PTR(-EPERM);
}

uint32_t exynos_drm_get_possible_crtcs(struct drm_encoder *encoder,
		enum exynos_drm_output_type out_type)
{
	struct exynos_drm_crtc *find_crtc;
	struct drm_crtc *crtc;
	uint32_t possible_crtcs = 0;

	drm_for_each_crtc(crtc, encoder->dev) {
		if (to_exynos_crtc(crtc)->possible_type & out_type) {
			find_crtc = to_exynos_crtc(crtc);
			possible_crtcs |= drm_crtc_mask(&find_crtc->base);
		}
	}

	return possible_crtcs;
}

void exynos_drm_crtc_te_handler(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->te_handler)
		exynos_crtc->ops->te_handler(exynos_crtc);
}
