/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * TUI file for Samsung EXYNOS DPU driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_drv.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_vblank.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_tui.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_dsim.h>

#include <cal_common/decon_cal.h>
#include <cal_common/dpp_cal.h>

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
struct stui_buf_info *(*tui_get_buf_info)(void);
void (*tui_free_video_space)(void);
#endif

/* only valid for LCD display crtc */
static struct drm_crtc *primary_crtc;

bool is_tui_trans(const struct drm_crtc_state *crtc_state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (crtc_state->crtc != primary_crtc)
		return false;

	return exynos_crtc_state->tui_changed;
}
EXPORT_SYMBOL(is_tui_trans);

int exynos_drm_atomic_check_tui(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_crtc_state = to_exynos_crtc_state(old_crtc_state);

		if (crtc != primary_crtc)
			continue;

		if (new_exynos_crtc_state->tui_status &&
				!new_exynos_crtc_state->tui_changed) {
			DRM_ERROR("tui enabled reject commit(%pK)\n", state);
			return -EPERM;
		}

		if (old_exynos_crtc_state->tui_changed &&
         !new_exynos_crtc_state->tui_changed &&
				(new_crtc_state->plane_mask == 0)) {
			DRM_ERROR("reject clear display commit(%pK)\n", state);
			return -EPERM;
		}
	}

	return 0;
}

/* See also: drm_atomic_helper_duplicate_state() */
static struct drm_atomic_state *
exynos_drm_atomic_duplicate_state(struct drm_crtc *crtc,
				  struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev;
	struct drm_atomic_state *state;
	struct drm_crtc_state *crtc_state;
	int err = 0;

	dev = crtc->dev;
	state = drm_atomic_state_alloc(dev);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->acquire_ctx = ctx;
	state->duplicated = true;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state)) {
		err = PTR_ERR(crtc_state);
		goto free;
	}

	err = drm_atomic_add_affected_planes(state, crtc);
	if (err < 0)
		goto free;

	err = drm_atomic_add_affected_connectors(state, crtc);
	if (err < 0)
		goto free;

	/* clear the acquire context so that it isn't accidentally reused */
	state->acquire_ctx = NULL;
free:
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
	}

	return state;
}

/**
 * exynos_atomic_enter_tui - save the current state and disable lcd display
 *
 * Disable display pipeline for lcd crtc, but skip control for the panel,
 * block power and te irq. The clock(disp, int) should be guaranteed for TUI.
 * Duplicate the current atomic state and this state should be restored in
 * exynos_atomic_exit_tui().
 * See also:
 * drm_atomic_helper_suspend()
 */
int exynos_atomic_enter_tui(void)
{
	struct drm_device *dev;
	struct drm_atomic_state *old_state, *new_state;
	struct drm_mode_config *mode_config;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_plane_state *new_plane_state;
	struct drm_plane *plane;
	struct exynos_drm_plane_state *new_exynos_plane_state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_connector *connector;
	struct drm_connector_state *new_connector_state;
	struct exynos_drm_connector_state *new_exynos_connector_state;
	struct resolution_info res_info;
	int i, ret = 0;
	unsigned long op_mode = 0;
	u32 id;

	crtc = primary_crtc;
	if (!crtc)
		return -ENODEV;

	dev = crtc->dev;
	DRM_DEV_INFO(dev->dev, "%s\n", __func__);
	pr_info("%s +\n", __func__);

	old_exynos_crtc_state = to_exynos_crtc_state(crtc->state);
	if (old_exynos_crtc_state->tui_status) {
		pr_info("%s: already in tui state\n", __func__);
		return -EBUSY;
	}

	if (!crtc->state->active || !crtc->state->enable)
		return -EINVAL;

	mode_config = &dev->mode_config;

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, ret);

	exynos_crtc = to_exynos_crtc(crtc);
	hibernation_block_exit(exynos_crtc->hibernation);

	old_state = exynos_drm_atomic_duplicate_state(&exynos_crtc->base, &ctx);
	if (IS_ERR(old_state))
		goto err_dup;

	mode_config->suspend_state = old_state;

	new_state = exynos_drm_atomic_duplicate_state(&exynos_crtc->base, &ctx);
	if (IS_ERR(new_state)) {
		ret = PTR_ERR(new_state);
		mode_config->suspend_state = NULL;
		drm_atomic_state_put(old_state);
		goto err_dup;
	}
	new_state->acquire_ctx = &ctx;

	for_each_new_connector_in_state(new_state, connector, new_connector_state, i) {
		if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
			new_exynos_connector_state = to_exynos_connector_state(new_connector_state);
			op_mode = new_exynos_connector_state->exynos_mode.mode_flags;
			pr_info("%s: Read operation mode(%lu)\n", __func__, op_mode);
			break;
		}
	}

	drm_wait_one_vblank(dev, crtc->index);

	crtc_state = drm_atomic_get_crtc_state(new_state, crtc);
	if (!(op_mode & MIPI_DSI_MODE_VIDEO)) { /* command mode */
		crtc_state->active = false;
		pr_info("%s: Set crtc_state->active to false\n", __func__);
	}
	new_exynos_crtc_state = to_exynos_crtc_state(crtc_state);
	new_exynos_crtc_state->tui_status = true;
	new_exynos_crtc_state->tui_changed = true;
	new_exynos_crtc_state->dqe_fd = -1;

	/* full frame update */
	if (exynos_crtc->partial) {
		exynos_partial_set_full(&new_exynos_crtc_state->base.mode,
				&new_exynos_crtc_state->partial_region);
		pr_info("%s: x1(%d) y1(%d) x2(%d) y2(%d)\n", __func__,
				new_exynos_crtc_state->partial_region.x1,
				new_exynos_crtc_state->partial_region.y1,
				new_exynos_crtc_state->partial_region.x2,
				new_exynos_crtc_state->partial_region.y2);
		exynos_partial_update(exynos_crtc->partial, &old_exynos_crtc_state->partial_region,
				&new_exynos_crtc_state->partial_region);
	}

	/* if need, set the default display mode for TUI */
	if (!(op_mode & MIPI_DSI_MODE_VIDEO)) { /* command mode */
		for_each_new_plane_in_state(new_state, plane, new_plane_state, i) {
			new_exynos_plane_state = to_exynos_plane_state(new_plane_state);
			new_exynos_plane_state->hdr_fd = -1;
			ret = drm_atomic_set_crtc_for_plane(new_plane_state, NULL);
			if (ret < 0)
				goto err_new_state;

			drm_atomic_set_fb_for_plane(new_plane_state, NULL);
		}
	} else if (op_mode & MIPI_DSI_MODE_VIDEO) {
		for_each_new_plane_in_state(new_state, plane, new_plane_state, i) {
			new_exynos_plane_state = to_exynos_plane_state(new_plane_state);
			new_exynos_plane_state->hdr_fd = -1;
		}

		decon_reg_set_interrupts(0, false);
		for (id = 0; id <= REGS_DPP7_ID; id++) {
			idma_reg_set_irq_disable(id);
			dpp_reg_set_irq_disable(id);
		}
	}

	res_info.xres = crtc_state->adjusted_mode.hdisplay;
	res_info.yres = crtc_state->adjusted_mode.vdisplay;
	if (!(op_mode & MIPI_DSI_MODE_VIDEO))
		res_info.mode = 0; /* command mode */
	else
		res_info.mode = 1; /* video mode */

	ret = drm_atomic_commit(new_state);
	if (ret < 0)
		goto err_new_state;

	/* shoule set the default disp clock for command mode tui */
	if (IS_ENABLED(CONFIG_EXYNOS_BTS) && (!(op_mode & MIPI_DSI_MODE_VIDEO))) {
		if (exynos_pm_qos_request_active(&exynos_crtc->bts->int_qos))
			exynos_pm_qos_update_request(&exynos_crtc->bts->int_qos, 133 * 1000);
		else
			DRM_DEV_ERROR(dev->dev, "int qos setting error\n");

		if (exynos_pm_qos_request_active(&exynos_crtc->bts->disp_qos))
			exynos_pm_qos_update_request(&exynos_crtc->bts->disp_qos, 200 * 1000);
		else
			DRM_DEV_ERROR(dev->dev, "disp qos setting error\n");
	}

	DPU_EVENT_LOG("TUI_ENTER", exynos_crtc, 0, "resolution[%ux%u] mode[%s]",
			res_info.xres, res_info.yres, res_info.mode ?
			"Video" : "Command");

err_new_state:
	drm_atomic_state_put(new_state);
err_dup:
	hibernation_unblock(exynos_crtc->hibernation);
	DRM_MODESET_LOCK_ALL_END(dev, ctx, ret);
	pr_info("%s ret(%d) -\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(exynos_atomic_enter_tui);

int exynos_atomic_exit_tui(void)
{
	struct drm_device *dev;
	struct drm_atomic_state *suspend_state;
	struct drm_mode_config *mode_config;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_plane_state *new_plane_state;
	struct drm_plane *plane;
	struct exynos_drm_plane_state *new_exynos_plane_state;
	struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc = primary_crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_connector_state;
	struct exynos_drm_connector_state *new_exynos_connector_state;
	int ret, i;
	unsigned long op_mode = 0;
	u32 id;

	pr_info("%s +\n", __func__);

	if (!crtc)
		return -ENODEV;

	old_exynos_crtc_state = to_exynos_crtc_state(crtc->state);
	if (!old_exynos_crtc_state->tui_status) {
		pr_info("%s: already out tui state\n", __func__);
		return -EBUSY;
	}

	dev = crtc->dev;
	mode_config = &dev->mode_config;

	/* if necessary, fix up suspend atomic suspend_state. */
	suspend_state = mode_config->suspend_state;
	if (!suspend_state) {
		pr_err("there is not suspend_state\n");
		return -EINVAL;
	}

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, ret);

	suspend_state->acquire_ctx = &ctx;

	for_each_new_connector_in_state(suspend_state, connector, new_connector_state, i) {
		if (connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
			new_exynos_connector_state = to_exynos_connector_state(new_connector_state);
			op_mode = new_exynos_connector_state->exynos_mode.mode_flags;
			pr_info("%s: Read operation mode(%lu)\n", __func__, op_mode);
			break;
		}
	}

	new_crtc_state = drm_atomic_get_crtc_state(suspend_state, crtc);
	if (IS_ERR(new_crtc_state)) {
		ret = PTR_ERR(new_crtc_state);
		goto err;
	}
	new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
	new_exynos_crtc_state->tui_status = false;
	new_exynos_crtc_state->tui_changed = true;
	new_exynos_crtc_state->dqe_fd = -1;
	if (!(op_mode & MIPI_DSI_MODE_VIDEO)) {
		new_exynos_crtc_state->reserved_win_mask = 0;
		new_exynos_crtc_state->freed_win_mask = 0;
		new_exynos_crtc_state->win_inserted = false;
		new_exynos_crtc_state->visible_win_mask = 0;
	}

	for_each_new_plane_in_state(suspend_state, plane, new_plane_state, i) {
		new_exynos_plane_state = to_exynos_plane_state(new_plane_state);
		new_exynos_plane_state->hdr_fd = -1;
	}

	exynos_crtc = to_exynos_crtc(crtc);
	if (exynos_crtc->partial) {
		exynos_partial_set_full(&new_exynos_crtc_state->base.mode,
				&new_exynos_crtc_state->partial_region);
	}

    if (op_mode & MIPI_DSI_MODE_VIDEO) {
           decon_reg_set_interrupts(0, true);
           for (id = 0; id <= REGS_DPP7_ID; id++) {
                   idma_reg_set_irq_enable(id);
                   dpp_reg_set_irq_enable(id);
           }
	}

	exynos_crtc = to_exynos_crtc(crtc);
	if (exynos_crtc->partial) {
		exynos_partial_set_full(&new_exynos_crtc_state->base.mode,
			&new_exynos_crtc_state->partial_region);
	}

	ret = drm_atomic_helper_commit_duplicated_state(suspend_state, &ctx);
	if (ret < 0) {
		pr_err("failed to atomic commit suspend_state(0x%x)\n", ret);
		goto err;
	}

	mode_config->suspend_state = NULL;

	DPU_EVENT_LOG("TUI_EXIT", exynos_crtc, 0, NULL);
err:
	drm_atomic_state_put(suspend_state);
	DRM_MODESET_LOCK_ALL_END(dev, ctx, ret);

	pr_info("%s ret(%d) -\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(exynos_atomic_exit_tui);

void exynos_tui_register(struct drm_crtc *crtc)
{
	if (crtc->index == 0)
		primary_crtc = crtc;

	return;
}

void exynos_tui_get_resolution(struct resolution_info *res_info)
{
	struct drm_device *dev;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_connector *conn;
	struct exynos_drm_connector_state *exynos_conn_state;
	unsigned long op_mode = 0;
	int ret = 0;

	dev = primary_crtc->dev;

	DRM_MODESET_LOCK_ALL_BEGIN(dev, ctx, 0, ret);

	conn = crtc_get_conn(primary_crtc->state, DRM_MODE_CONNECTOR_DSI);
	exynos_conn_state = to_exynos_connector_state(conn->state);
	op_mode = exynos_conn_state->exynos_mode.mode_flags;

	if (!(op_mode & MIPI_DSI_MODE_VIDEO))
		res_info->mode = 0; /* command mode */
	else
		res_info->mode = 1; /* video mode */
	pr_info("%s: res_info->mode(%d)\n", __func__, res_info->mode);

	res_info->xres = primary_crtc->state->adjusted_mode.hdisplay;
	res_info->yres = primary_crtc->state->adjusted_mode.vdisplay;

	DRM_MODESET_LOCK_ALL_END(dev, ctx, ret);
}
EXPORT_SYMBOL(exynos_tui_get_resolution);

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
void exynos_tui_set_stui_funcs(struct stui_buf_info *(*func1)(void), void (*func2)(void))
{
	tui_get_buf_info = func1;
	tui_free_video_space = func2;
}
EXPORT_SYMBOL(exynos_tui_set_stui_funcs);
#endif

int exynos_tui_get_panel_info(u64 *buf, int size)
{
	struct dsim_device *dsim_dev = get_dsim_drvdata(0);
	struct dsim_reg_config *dsim_config = &dsim_dev->config;
	struct dpu_panel_timing *timing = &dsim_config->p_timing;
	struct exynos_dsc *dsc = &dsim_config->dsc;
	struct dsim_clks *clks = &dsim_dev->clk_param;
	struct stdphy_pms *pms = &dsim_config->dphy_pms;
	int idx = 0;
	u64 data, max;

	pr_info("%s +\n", __func__);
	if (size < idx + 1)
		return -EINVAL;

	data = ((u64)timing->vactive * 100000000) +
		((u64)timing->hactive * 10000) + ((u64)timing->vrefresh);
	buf[idx++] = data;

	max = (timing->vfp > timing->vsa) ? timing->vfp : timing->vsa;
	max = (timing->vbp > max) ? timing->vbp : max;
	max = (timing->hfp > max) ? timing->hfp : max;
	max = (timing->hsa > max) ? timing->hsa : max;
	max = (timing->hbp > max) ? timing->hbp : max;
	if (max >= 100) {
		data = ((u64)timing->vfp * 10000000000)
			+ ((u64)timing->vsa * 100000000)
			+ ((u64)timing->vbp * 1000000)
			+ ((u64)timing->hfp * 10000)
			+ ((u64)timing->hsa * 100)
			+ ((u64)timing->hbp);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
	} else {
		data = ((u64)timing->vfp * 10000000000)
			+ ((u64)timing->vsa * 100000)
			+ ((u64)timing->vbp);
		data |= (1ULL << 63);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;

		data = ((u64)timing->hfp * 10000000000)
			+ ((u64)timing->hsa * 100000)
			+ ((u64)timing->hbp);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
	}

	data = ((u64)dsc->slice_height * 1000) + ((u64)dsc->slice_count * 100)
		+ ((u64)dsc->dsc_count * 10) + ((u64)dsim_config->data_lane_cnt);
	data |= ((dsc->enabled) ? (1ULL << 63) : 0);
	data |= ((dsim_config->mode == DSIM_COMMAND_MODE) ? (1ULL << 62) : 0);
	data |= ((pms->dither_en) ? (1ULL << 61) : 0);

	if (size < idx + 1)
		return -EINVAL;
	buf[idx++] = data;

	data = ((u64)clks->hs_clk * 10000000000)
		+ ((u64)clks->esc_clk * 100000)
		+ ((u64)dsim_config->cmd_underrun_cnt);

	if (size < idx + 1)
		return -EINVAL;
	buf[idx++] = data;

	max = (pms->mfr) | (pms->mrr) | (pms->sel_pf) | (pms->icp)
		| (pms->afc_enb) | (pms->extafc) | (pms->feed_en)
		| (pms->fsel) | (pms->fout_mask) | (pms->rsel);

	if (max == 0) {
		data = ((u64)pms->p * 10000000000) + ((u64)pms->m * 100000) + ((u64)pms->s);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
		data = ((u64)pms->k * 10000000000);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
	} else {
		data = ((u64)pms->p * 10000000000) + ((u64)pms->m * 100000) + ((u64)pms->s);
		data |= (1ULL << 63);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
		data = ((u64)pms->k * 10000000000) + ((u64)pms->mfr * 100000) + ((u64)pms->mrr);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
		data = ((u64)pms->sel_pf * 10000000000) + ((u64)pms->icp * 100000) + ((u64)pms->afc_enb);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
		data = ((u64)pms->extafc * 10000000000) + ((u64)pms->feed_en * 100000) + ((u64)pms->fsel);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
		data = ((u64)pms->fout_mask * 100000) + ((u64)pms->rsel);

		if (size < idx + 1)
			return -EINVAL;
		buf[idx++] = data;
	}

	pr_info("%s -\n", __func__);
	return 0;
}
EXPORT_SYMBOL(exynos_tui_get_panel_info);
