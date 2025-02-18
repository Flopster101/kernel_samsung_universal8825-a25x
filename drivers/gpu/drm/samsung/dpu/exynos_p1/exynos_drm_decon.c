// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_decon.c
 *
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
 * Authors:
 *	Hyung-jun Kim <hyungjun07.kim@samsung.com>
 *	Seong-gyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <drm/drm_atomic.h>
#include <exynos_display_common.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_vblank.h>

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>
#include <linux/console.h>
#include <linux/iommu.h>
#include <linux/pinctrl/consumer.h>

#include <dpu_trace.h>
#include <video/videomode.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_drv.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_dp.h>
#include <exynos_drm_migov.h>
#include <exynos_drm_freq_hop.h>
#include <exynos_drm_recovery.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_tui.h>
#include "panel/panel-samsung-drv.h"

#include <soc/samsung/exynos-pd.h>

#include <decon_cal.h>
#include <regs-decon.h>
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_decon.h>
#include <mcd_drm_helper.h>
#endif

struct decon_device *decon_drvdata[MAX_DECON_CNT];

static int dpu_decon_log_level = 6;
module_param(dpu_decon_log_level, int, 0600);
MODULE_PARM_DESC(dpu_decon_log_level, "log level for dpu decon [default : 6]");

#define decon_info(decon, fmt, ...)	\
dpu_pr_info(drv_name((decon)), (decon)->id, dpu_decon_log_level, fmt, ##__VA_ARGS__)

#define decon_warn(decon, fmt, ...)	\
dpu_pr_warn(drv_name((decon)), (decon)->id, dpu_decon_log_level, fmt, ##__VA_ARGS__)

#define decon_err(decon, fmt, ...)	\
dpu_pr_err(drv_name((decon)), (decon)->id, dpu_decon_log_level, fmt, ##__VA_ARGS__)

#define decon_debug(decon, fmt, ...)	\
dpu_pr_debug(drv_name((decon)), (decon)->id, dpu_decon_log_level, fmt, ##__VA_ARGS__)

#define SHADOW_UPDATE_TIMEOUT_US	(300 * USEC_PER_MSEC) /* 300ms */

static const struct of_device_id decon_driver_dt_match[] = {
	{.compatible = "samsung,exynos-decon"},
	{},
};
MODULE_DEVICE_TABLE(of, decon_driver_dt_match);

void decon_dump(struct exynos_drm_crtc *exynos_crtc)
{
	int i;
	int acquired = console_trylock();
	struct decon_device *d;
	struct decon_device *decon = exynos_crtc->ctx;
	struct writeback_device *wb = decon_get_wb(decon);
	struct dsim_device *dsim = NULL;

	/* add connector dump */
	for (i = 0; i < REGS_DECON_ID_MAX; ++i) {
		d = get_decon_drvdata(i);
		if (!d)
			continue;

		if (d->state != DECON_STATE_ON) {
			decon_info(d, "DECON disabled(%d)\n", d->state);
			continue;
		}

		dsim = decon_get_dsim(d);
		if (!dsim)
			decon_info(d, "DECON(%d) has not DSIM\n", d->id);
		else
			if (dsim->id == 0)
				dsim_dump(dsim);

		__decon_dump(d->id, &d->regs, d->config.dsc.enabled);
		exynos_dqe_dump(d->crtc->dqe);
	}

	dpp_dump(decon->dpp, decon->dpp_cnt);

	if (wb)
		wb_dump(wb);

#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DP)
	if (decon_get_encoder(decon, DRM_MODE_ENCODER_TMDS))
		dp_debug_dump();
#endif

	if (acquired)
		console_unlock();
}

static bool __is_recovery_supported(struct decon_device *decon)
{
	enum recovery_state state = exynos_recovery_get_state(decon);

	return ((state == RECOVERY_NOT_SUPPORTED) ? false : true);
}

static bool __is_recovery_begin(struct decon_device *decon)
{
	bool begin = false;
	enum recovery_state state = exynos_recovery_get_state(decon);

	if ((state == RECOVERY_TRIGGER) || (state == RECOVERY_BEGIN))
		begin = true;

	return begin;
}

static bool __is_recovery_running(struct decon_device *decon)
{
	bool recovering = false;
	enum recovery_state state = exynos_recovery_get_state(decon);

	if ((__is_recovery_begin(decon)) || (state == RECOVERY_RESTORE))
		recovering = true;

	return recovering;
}

void decon_trigger_recovery(struct exynos_drm_crtc *exynos_crtc, bool force)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct exynos_recovery *recovery = &decon->recovery;

	exynos_recovery_set_state(decon, RECOVERY_TRIGGER);
	recovery->force = force;
	queue_work(system_highpri_wq, &recovery->work);
}

bool decon_read_recovering(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;

	return __is_recovery_running(decon);
}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
void decon_dump_event_log(struct exynos_drm_crtc *exynos_crtc)
{
	struct drm_printer p = drm_info_printer(exynos_crtc->dev);

	DPU_EVENT_SHOW(exynos_crtc, &p);
}
#endif

static inline u32 win_start_pos(int x, int y)
{
	return (WIN_STRPTR_Y_F(y) | WIN_STRPTR_X_F(x));
}

static inline u32 win_end_pos(int x2, int y2)
{
	return (WIN_ENDPTR_Y_F(y2 - 1) | WIN_ENDPTR_X_F(x2 - 1));
}

#define COLOR_MAP_BLACK			0x00000000
/*
 * This function can be used in cases where all windows are disabled
 * but need something to be rendered for display. This will make a black
 * frame via decon using a single window with color map enabled.
 */
static void decon_set_color_map(struct decon_device *decon, u32 win_id,
						u32 hactive, u32 vactive)
{
	u32 colormap = COLOR_MAP_BLACK;
	struct decon_window_regs win_info;

	decon_debug(decon, "%s + color(%#x)\n", __func__, colormap);

	memset(&win_info, 0, sizeof(struct decon_window_regs));
	win_info.start_pos = win_start_pos(0, 0);
	win_info.end_pos = win_end_pos(hactive, vactive);
	win_info.start_time = 0;
	win_info.colormap = colormap;
	win_info.blend = DECON_BLENDING_NONE;
	decon_reg_set_window_control(decon->id, win_id, &win_info, true);

	decon_debug(decon, "%s -\n", __func__);
}

static inline bool decon_is_te_enabled(const struct decon_device *decon)
{
	return (decon->config.mode.op_mode == DECON_COMMAND_MODE) &&
			(decon->config.mode.trig_mode == DECON_HW_TRIG);
}

static int decon_enable_vblank(struct exynos_drm_crtc *crtc)
{
	struct decon_device *decon = crtc->ctx;

	decon_debug(decon, "%s +\n", __func__);

	if (decon_is_te_enabled(decon))
		enable_irq(decon->irq_te);
	else /* use framestart interrupt to track vsyncs */
		enable_irq(decon->irq_fs);

	DPU_EVENT_LOG(DPU_EVT_VBLANK_ENABLE, crtc, NULL);

	decon_debug(decon, "%s -\n", __func__);
	return 0;
}

static void decon_disable_vblank(struct exynos_drm_crtc *crtc)
{
	struct decon_device *decon = crtc->ctx;

	decon_debug(decon, "%s +\n", __func__);

	if (decon_is_te_enabled(decon))
		disable_irq_nosync(decon->irq_te);
	else
		disable_irq_nosync(decon->irq_fs);

	DPU_EVENT_LOG(DPU_EVT_VBLANK_DISABLE, crtc, NULL);

	decon_debug(decon, "%s -\n", __func__);
}


static enum exynos_drm_writeback_type
decon_get_wb_type(struct drm_crtc_state *new_crtc_state)
{
	int i;
	struct drm_atomic_state *state = new_crtc_state->state;
	struct drm_connector_state *conn_state;
	struct drm_connector *conn;
	struct exynos_drm_writeback_state *wb_state;

	for_each_new_connector_in_state(state, conn, conn_state, i) {
		if (!(new_crtc_state->connector_mask &
					drm_connector_mask(conn)))
			continue;
		if (conn->connector_type == DRM_MODE_CONNECTOR_WRITEBACK) {
			wb_state = to_exynos_wb_state(conn_state);
			return wb_state->type;
		}
	}
	return EXYNOS_WB_NONE;
}

static void decon_mode_update_vscan_fps(struct decon_device *decon)
{
	u16 vscan_coef;
	const struct drm_connector *conn =
		crtc_get_conn(decon->crtc->base.state, DRM_MODE_CONNECTOR_DSI);
	const struct exynos_drm_connector_state *exynos_conn_state;

	if (!conn)
		return;

	exynos_conn_state = to_exynos_connector_state(conn->state);
	vscan_coef = exynos_conn_state->exynos_mode.vscan_coef;
	if (vscan_coef <= 1)
		return;

#if !IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	decon->bts.fps *= vscan_coef;
#endif
}

static void decon_mode_update_bts(struct decon_device *decon,
		const struct drm_display_mode *mode)
{
	struct videomode vm;
	const struct drm_crtc *crtc = &decon->crtc->base;
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc->state);
	int i;

	drm_display_mode_to_videomode(mode, &vm);

	decon->bts.vbp = vm.vback_porch;
	decon->bts.vfp = vm.vfront_porch;
	decon->bts.vsa = vm.vsync_len;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	decon->bts.fps = mcd_decon_get_bts_fps(mode);
#else
	decon->bts.fps = drm_mode_vrefresh(mode);
#endif
	decon_mode_update_vscan_fps(decon);
	decon_info(decon, "%s bts.fps: %d\n", __func__, decon->bts.fps);

	/* the bts.win_config should be intialized only at full modeset */
	if (!exynos_crtc_state->seamless_modeset) {
		for (i = 0; i < decon->win_cnt; i++) {
			decon->bts.win_config[i].state = DPU_WIN_STATE_DISABLED;
			decon->bts.win_config[i].dbg_dma_addr = 0;
		}
	}
}

static void decon_update_config(struct decon_config *config,
		const struct drm_display_mode *mode,
		const struct exynos_display_mode *exynos_mode)
{
	bool is_vid_mode;

	config->image_width = mode->hdisplay;
	config->image_height = mode->vdisplay;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	config->fps = drm_mode_vrefresh(mode) * max_t(typeof(mode->vscan), mode->vscan, 1);
#else
	config->fps = drm_mode_vrefresh(mode);
#endif

	if (!exynos_mode) {
		struct decon_device *decon =
			container_of(config, struct decon_device, config);

		decon_debug(decon, "%s: no private mode config\n", __func__);

		/* valid defaults (ex. for writeback) */
		config->dsc.enabled = false;
		config->out_bpc = 8;
		return;
	}

	config->dsc.enabled = exynos_mode->dsc.enabled;
	if (exynos_mode->dsc.enabled) {
		config->dsc.dsc_count = exynos_mode->dsc.dsc_count;
		config->dsc.slice_count = exynos_mode->dsc.slice_count;
		config->dsc.slice_height = exynos_mode->dsc.slice_height;
		config->dsc.slice_width = DIV_ROUND_UP(config->image_width,
				config->dsc.slice_count);
	}

	is_vid_mode = (exynos_mode->mode_flags & MIPI_DSI_MODE_VIDEO) != 0;
	config->mode.op_mode = is_vid_mode ? DECON_VIDEO_MODE : DECON_COMMAND_MODE;

	config->out_bpc = exynos_mode->bpc;
}

static int decon_check_seamless_modeset(struct exynos_drm_crtc *exynos_crtc,
		struct drm_crtc_state *crtc_state,
		const struct exynos_drm_connector_state *exynos_conn_state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (!exynos_conn_state->seamless_modeset)
		return 0;

	/*
	 * If it needs to check whether to change the display mode seamlessly
	 * for decon, please add to here.
	 */
	exynos_crtc_state->seamless_modeset =
		exynos_conn_state->seamless_modeset;
	crtc_state->mode_changed = false;

	return 0;
}

static int decon_check_modeset(struct exynos_drm_crtc *exynos_crtc,
		struct drm_crtc_state *crtc_state)
{
	struct drm_atomic_state *state = crtc_state->state;
	const struct decon_device *decon = exynos_crtc->ctx;
	const struct exynos_drm_connector_state *exynos_conn_state;
	int ret = 0;

	exynos_conn_state = crtc_get_exynos_connector_state(state, crtc_state);
	if (!exynos_conn_state)
		return 0;

	if (!(exynos_conn_state->exynos_mode.mode_flags & MIPI_DSI_MODE_VIDEO)) {
		if (!decon->irq_te || !decon->res.pinctrl) {
			decon_err(decon, "TE error: irq_te %d, te_pinctrl %p\n",
					decon->irq_te, decon->res.pinctrl);

			return -EINVAL;
		}
	}

	if (!crtc_state->active_changed && crtc_state->active &&
			!crtc_state->connectors_changed)
		ret = decon_check_seamless_modeset(exynos_crtc, crtc_state,
				exynos_conn_state);

	return ret;
}


static int decon_atomic_check(struct exynos_drm_crtc *exynos_crtc,
		struct drm_crtc_state *crtc_state)
{
	const struct decon_device *decon = exynos_crtc->ctx;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	int ret = 0;

	exynos_crtc_state->wb_type = decon_get_wb_type(crtc_state);

	crtc_state->no_vblank = false;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	if (mcd_drm_check_commit_skip(exynos_crtc, __func__))
		crtc_state->no_vblank = true;
#endif

	if (decon->config.out_type == DECON_OUT_WB) {
		crtc_state->no_vblank = true;
		ret = drm_atomic_add_affected_connectors(crtc_state->state,
				&exynos_crtc->base);
		if (ret)
			return ret;
	}

	if (crtc_state->mode_changed)
		ret = decon_check_modeset(exynos_crtc, crtc_state);

	exynos_dqe_prepare(exynos_crtc->dqe, crtc_state);

	return ret;
}

#define TIMEOUT_CNT (1)
static void decon_atomic_begin(struct exynos_drm_crtc *crtc)
{
	struct decon_device *decon = crtc->ctx;

	decon_debug(decon, "%s +\n", __func__);
	DPU_EVENT_LOG(DPU_EVT_ATOMIC_BEGIN, crtc, NULL);

	if (__is_recovery_supported(decon) && __is_recovery_begin(decon))
		decon_info(decon, "%s is skipped(recovery started)\n", __func__);
	else if (decon_reg_wait_update_done_and_mask(decon->id, &decon->config.mode,
				SHADOW_UPDATE_TIMEOUT_US)) {
		decon_info(decon, "%s : decon update timeout\n", __func__);
		dpu_dump_with_event(crtc);
	}
	decon_debug(decon, "%s -\n", __func__);
}

static int decon_get_win_id(const struct drm_crtc_state *crtc_state, int zpos)
{
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	const unsigned long win_mask = exynos_crtc_state->reserved_win_mask;
	int bit, i = 0;

	for_each_set_bit(bit, &win_mask, MAX_WIN_PER_DECON) {
		if (i == zpos)
			return bit;
		i++;
	}

	return -EINVAL;
}

static bool decon_is_win_used(const struct drm_crtc_state *crtc_state, int win_id)
{
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	const unsigned long win_mask = exynos_crtc_state->visible_win_mask;

	if (win_id > MAX_WIN_PER_DECON)
		return false;

	return (BIT(win_id) & win_mask) != 0;
}

static void decon_disable_win(struct decon_device *decon, int win_id)
{
	const struct drm_crtc *crtc = &decon->crtc->base;
	int win_cnt = get_plane_num(crtc->dev);

	decon_debug(decon, "disabling winid:%d\n", win_id);

	/*
	 * When disabling the plane, previously connected window (win_id) should be
	 * disabled, not the newly requested one. Only disable the old window if it
	 * was previously connected and it's not going to be used by any other plane.
	 */
	if (win_id < win_cnt && !decon_is_win_used(crtc->state, win_id))
		decon_reg_set_win_enable(decon->id, win_id, 0);
}

static void _dpp_disable(struct exynos_drm_plane *exynos_plane)
{
	if (exynos_plane->is_win_connected) {
		exynos_plane->ops->atomic_disable(exynos_plane);
		exynos_plane->is_win_connected = false;
	}
}

static void decon_update_plane(struct exynos_drm_crtc *exynos_crtc,
			       struct exynos_drm_plane *exynos_plane)
{
	const struct drm_plane_state *plane_state = exynos_plane->base.state;
	const struct exynos_drm_plane_state *exynos_plane_state =
			to_exynos_plane_state(plane_state);
	const struct drm_crtc_state *crtc_state = exynos_crtc->base.state;
	struct dpp_device *dpp = plane_to_dpp(exynos_plane);
	struct decon_device *decon = exynos_crtc->ctx;
	struct decon_window_regs win_info;
	unsigned int zpos;
	int win_id;
	bool is_colormap = false;
	u16 hw_alpha;
	unsigned int simplified_rot;

	decon_debug(decon, "%s +\n", __func__);

	zpos = plane_state->normalized_zpos;

	if (!exynos_plane->is_win_connected || crtc_state->zpos_changed) {
		win_id = decon_get_win_id(exynos_crtc->base.state, zpos);
		decon_debug(decon, "new win_id=%d zpos=%d mask=0x%x\n",
				win_id, zpos, crtc_state->plane_mask);
	} else {
		win_id = exynos_plane->win_id;
		decon_debug(decon, "reuse existing win_id=%d zpos=%d mask=0x%x\n",
				win_id, zpos, crtc_state->plane_mask);
	}

	if (WARN(win_id < 0 || win_id > MAX_WIN_PER_DECON,
		"couldn't find win id (%d) for zpos=%d plane_mask=0x%x\n",
		win_id, zpos, crtc_state->plane_mask))
		return;

	memset(&win_info, 0, sizeof(struct decon_window_regs));

	is_colormap = plane_state->fb && exynos_drm_fb_is_colormap(plane_state->fb);
	if (is_colormap)
		win_info.colormap = exynos_plane_state->colormap;

	win_info.start_pos = win_start_pos(plane_state->dst.x1,	plane_state->dst.y1);
	win_info.end_pos = win_end_pos(plane_state->dst.x2, plane_state->dst.y2);

	simplified_rot = drm_rotation_simplify(plane_state->rotation,
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	if ((plane_state->dst.y1 <= DECON_WIN_START_TIME) ||
		(simplified_rot & DRM_MODE_ROTATE_90))
		win_info.start_time = 0;
	else
		win_info.start_time = DECON_WIN_START_TIME;

	win_info.ch = dpp->id; /* DPP's id is DPP channel number */

	hw_alpha = DIV_ROUND_CLOSEST(plane_state->alpha * EXYNOS_PLANE_ALPHA_MAX,
			DRM_BLEND_ALPHA_OPAQUE);
	win_info.plane_alpha = hw_alpha;
	win_info.blend = plane_state->pixel_blend_mode;

	if (zpos == 0 && hw_alpha == EXYNOS_PLANE_ALPHA_MAX)
		win_info.blend = DRM_MODE_BLEND_PIXEL_NONE;

	/* disable previous window if zpos has changed */
	if (exynos_plane->win_id != win_id)
		decon_disable_win(decon, exynos_plane->win_id);

	decon_reg_set_window_control(decon->id, win_id, &win_info, is_colormap);

	dpp->decon_id = decon->id;
	if (!is_colormap) {
		struct exynos_drm_plane *exynos_plane = &dpp->plane;
		exynos_plane->ops->atomic_update(exynos_plane, exynos_plane_state);
		exynos_plane->is_win_connected = true;
	} else {
		_dpp_disable(exynos_plane);
	}

	exynos_plane->win_id = win_id;

	DPU_EVENT_LOG(DPU_EVT_PLANE_UPDATE, exynos_crtc, exynos_plane);
	decon_debug(decon, "plane idx[%d]: alpha(0x%x) hw alpha(0x%x)\n",
			drm_plane_index(&exynos_plane->base), plane_state->alpha,
			hw_alpha);
	decon_debug(decon, "blend_mode(%d) color(%s:0x%x)\n", win_info.blend,
			is_colormap ? "enable" : "disable", win_info.colormap);
	decon_debug(decon, "%s -\n", __func__);
}

static void decon_disable_plane(struct exynos_drm_crtc *exynos_crtc,
				struct exynos_drm_plane *exynos_plane)
{
	struct decon_device *decon = exynos_crtc->ctx;

	decon_debug(decon, "%s +\n", __func__);

	decon_disable_win(decon, exynos_plane->win_id);

	_dpp_disable(exynos_plane);

	DPU_EVENT_LOG(DPU_EVT_PLANE_DISABLE, exynos_crtc, exynos_plane);
	decon_debug(decon, "%s -\n", __func__);
}

static void
decon_seamless_set_mode(struct drm_crtc_state *crtc_state,
					struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc = crtc_state->crtc;
	struct decon_device *decon = to_exynos_crtc(crtc)->ctx;
	struct drm_connector *conn;
	struct drm_connector_state *new_conn_state;
	const struct exynos_drm_connector_state *exynos_conn_state;
	const struct drm_crtc_helper_funcs *funcs;
	int i;

	funcs = crtc->helper_private;

	if (crtc_state->enable && funcs->mode_set_nofb) {
		decon_info(decon, "seamless modeset[%s] on [CRTC:%d:%s]\n",
			crtc_state->mode.name, crtc->base.id, crtc->name);

		exynos_conn_state = crtc_get_exynos_connector_state(
						old_state, crtc_state);

		decon_update_config(&decon->config, &crtc_state->adjusted_mode,
			&exynos_conn_state->exynos_mode);

		funcs->mode_set_nofb(crtc);
	}

	for_each_new_connector_in_state(old_state, conn, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_display_mode *mode, *adjusted_mode;
		struct drm_bridge *bridge;

		if (!(crtc_state->connector_mask & drm_connector_mask(conn)))
			continue;

		if (!new_conn_state->best_encoder)
			continue;

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;
		mode = &crtc_state->mode;
		adjusted_mode = &crtc_state->adjusted_mode;

		decon_info(decon, "seamless modeset[%s] on [ENCODER:%d:%s]\n",
				mode->name, encoder->base.id, encoder->name);

		if (funcs && funcs->atomic_mode_set) {
			funcs->atomic_mode_set(encoder, crtc_state,
					       new_conn_state);
		} else if (funcs && funcs->mode_set) {
			funcs->mode_set(encoder, mode, adjusted_mode);
		}

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_bridge_chain_mode_set(bridge, mode, adjusted_mode);
	}
}

static void decon_atomic_flush(struct exynos_drm_crtc *exynos_crtc,
		struct drm_crtc_state *old_crtc_state)
{
	bool is_frameupdate = true;
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc_state *new_crtc_state = exynos_crtc->base.state;
	struct exynos_drm_crtc_state *new_exynos_crtc_state =
					to_exynos_crtc_state(new_crtc_state);
	struct exynos_drm_crtc_state *old_exynos_crtc_state =
					to_exynos_crtc_state(old_crtc_state);
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	bool color_map = true;

	new_exynos_crtc_state->skip_frame_update = false;
#endif
	decon_debug(decon, "%s +\n", __func__);

	if (new_exynos_crtc_state->wb_type == EXYNOS_WB_NONE &&
			decon->config.out_type == DECON_OUT_WB)
		is_frameupdate = false;

	if (new_exynos_crtc_state->wb_type == EXYNOS_WB_CWB)
		decon_reg_set_cwb_enable(decon->id, true);
	else if (old_exynos_crtc_state->wb_type == EXYNOS_WB_CWB)
		decon_reg_set_cwb_enable(decon->id, false);

	if (crtc_needs_colormap(new_crtc_state)) {
		const int win_id = decon_get_win_id(new_crtc_state, 0);

		if (win_id < 0) {
			decon_warn(decon, "unable to get free win_id=%d mask=0x%x\n",
				win_id, new_exynos_crtc_state->reserved_win_mask);
			return;
		}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		if ((decon->config.out_type & DECON_OUT_DSI) &&
			(old_crtc_state->active == 0) && (new_crtc_state->active == 1)) {

			decon_info(decon, "skip color map\n");
			new_exynos_crtc_state->skip_frame_update = true;
			color_map = false;
			is_frameupdate = false;

		}
		if (color_map) {
			decon_debug(decon, "no planes, enable color map win_id=%d\n", win_id);
			/*
			 * TODO: window id needs to be unique when using dual display,
			 * current hack is to use decon id, but it could conflict
			 * if planes are assigned to other display.
			 */
			decon_set_color_map(decon, win_id, decon->config.image_width,
					decon->config.image_height);
		}
#else
		decon_debug(decon, "no planes, enable color map win_id=%d\n", win_id);
		decon_set_color_map(decon, win_id, decon->config.image_width,
				decon->config.image_height);
#endif
	} else if (new_crtc_state->plane_mask == 0) {
		is_frameupdate = false;
	}

	decon->config.in_bpc = new_exynos_crtc_state->in_bpc;
	decon_reg_set_bpc_and_dither(decon->id, &decon->config);
	exynos_dqe_update(exynos_crtc->dqe, new_crtc_state);

	if (new_exynos_crtc_state->seamless_modeset) {
		decon_seamless_set_mode(new_crtc_state, old_crtc_state->state);

		if (new_exynos_crtc_state->modeset_only) {
			int win_id;
			const unsigned long win_mask =
				new_exynos_crtc_state->reserved_win_mask;

			for_each_set_bit(win_id, &win_mask, MAX_WIN_PER_DECON)
				decon_reg_set_win_enable(decon->id, win_id, 0);
			decon_info(decon, "modeset_only\n");
		}
	}

	decon_reg_all_win_shadow_update_req(decon->id);
	if (is_frameupdate)
		reinit_completion(&decon->framestart_done);

	if (exynos_crtc->ops->check_svsync_start)
		exynos_crtc->ops->check_svsync_start(exynos_crtc);

	decon_reg_start(decon->id, &decon->config);

	if (!new_crtc_state->no_vblank)
		exynos_crtc_handle_event(exynos_crtc);

	if (exynos_crtc->migov)
		exynos_migov_update_ems_frame_cnt(exynos_crtc->migov);

	DPU_EVENT_LOG(DPU_EVT_ATOMIC_FLUSH, exynos_crtc, NULL);

	decon_debug(decon, "%s -\n", __func__);
}

static void decon_atomic_print_state(struct drm_printer *p,
		const struct exynos_drm_crtc *exynos_crtc)
{
	const struct decon_device *decon = exynos_crtc->ctx;
	const struct decon_config *cfg = &decon->config;

	drm_printf(p, "\tDecon #%d (state:%d)\n", decon->id, decon->state);
	drm_printf(p, "\t\ttype=0x%x\n", cfg->out_type);
	drm_printf(p, "\t\tsize=%dx%d\n", cfg->image_width, cfg->image_height);
	drm_printf(p, "\t\tmode=%s (%d)\n",
			cfg->mode.op_mode == DECON_VIDEO_MODE ? "video" : "cmd",
			cfg->mode.dsi_mode);
	drm_printf(p, "\t\tin_bpc=%d out_bpc=%d\n", cfg->in_bpc,
			cfg->out_bpc);
	drm_printf(p, "\t\tbts_fps=%d\n", decon->bts.fps);
}

static int decon_late_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc *crtc = &exynos_crtc->base;
	struct dentry *urgent_dent;
	struct device_node *te_np;

	urgent_dent = debugfs_create_dir("urgent", crtc->debugfs_entry);
	if (!urgent_dent) {
		DRM_ERROR("failed to create debugfs urgent directory\n");
		goto err;
	}

	debugfs_create_u32("rd_en", 0664,
			urgent_dent, &decon->config.urgent.rd_en);

	debugfs_create_x32("rd_hi_thres", 0664,
			urgent_dent, &decon->config.urgent.rd_hi_thres);

	debugfs_create_x32("rd_lo_thres", 0664,
			urgent_dent, &decon->config.urgent.rd_lo_thres);

	debugfs_create_x32("rd_wait_cycle", 0664,
			urgent_dent, &decon->config.urgent.rd_wait_cycle);

	debugfs_create_u32("wr_en", 0664,
			urgent_dent, &decon->config.urgent.wr_en);

	debugfs_create_x32("wr_hi_thres", 0664,
			urgent_dent, &decon->config.urgent.wr_hi_thres);

	debugfs_create_x32("wr_lo_thres", 0664,
			urgent_dent, &decon->config.urgent.wr_lo_thres);

	if (!debugfs_create_bool("dta_en", 0664,
			urgent_dent, &decon->config.urgent.dta_en)) {
		DRM_ERROR("failed to create debugfs dta_en file\n");
		goto err_urgent;
	}

	debugfs_create_x32("dta_hi_thres", 0664,
			urgent_dent, &decon->config.urgent.dta_hi_thres);

	debugfs_create_x32("dta_lo_thres", 0664,
			urgent_dent, &decon->config.urgent.dta_lo_thres);

	dpu_freq_hop_debugfs(exynos_crtc);

	te_np = of_get_child_by_name(decon->dev->of_node, "te_eint");
	if (te_np) {
		exynos_crtc->d.eint_pend = of_iomap(te_np, 0);
		if (!exynos_crtc->d.eint_pend)
			decon_err(decon, "failed to remap te eint pend\n");
	}

	return 0;

err_urgent:
	debugfs_remove_recursive(urgent_dent);
err:
	return -ENOENT;
}

#define DEFAULT_TIMEOUT_FPS 60
static void decon_wait_framestart(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct drm_crtc_state *crtc_state = exynos_crtc->base.state;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);
	int fps;
	u32 framestart_timeout;

	/*
	 * In case of modeset_only, all windows are disabled.
	 * When all windows are disabled, frame start interrupt doesn't occur.
	 */
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	/* Code for bypass commit when panel was not connected */
	if (exynos_crtc->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
		if (crtc_state->no_vblank) {
			decon_debug(decon, "%s is skipped(no_vblank)\n", __func__);
			return;
		}
	}
#endif

	fps = decon ? decon->config.fps : DEFAULT_TIMEOUT_FPS;
	framestart_timeout = 5 * 1000000 / fps / 1000; /* 5 frame */

	if (__is_recovery_supported(decon) && __is_recovery_begin(decon))
		decon_info(decon, "%s is skipped(recovery started)\n", __func__);
	else if (exynos_crtc_state->seamless_modeset &&
		exynos_crtc_state->modeset_only)
		decon_debug(decon, "%s is skipped(modeset_only)\n", __func__);
	else if (!crtc_state->planes_changed && (crtc_state->plane_mask != 0))
		decon_debug(decon, "%s is skipped(no planes_changed)\n", __func__);
	else if (!wait_for_completion_timeout(&decon->framestart_done,
				msecs_to_jiffies(framestart_timeout))) {
		DPU_EVENT_LOG(DPU_EVT_FRAMESTART_TIMEOUT,
				exynos_crtc, NULL);
		decon_warn(decon, "framestart timeout\n");
	}
}

static void decon_set_trigger(struct exynos_drm_crtc *exynos_crtc,
			struct exynos_drm_crtc_state *exynos_crtc_state)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct decon_mode *mode;

	mode = &decon->config.mode;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	/* Code for bypass commit when panel was not connected */
	if (mcd_drm_check_commit_skip(exynos_crtc, __func__)) {
		if (mode->op_mode == DECON_COMMAND_MODE)
			decon_reg_set_trigger(decon->id, mode, DECON_TRIG_MASK);
		return;
	}
#endif

	if (__is_recovery_supported(decon) && __is_recovery_begin(decon)) {
		decon_info(decon, "%s is skipped(recovery started)\n", __func__);
		goto exit;
	}

	if (decon_reg_wait_update_done_timeout(decon->id,
		SHADOW_UPDATE_TIMEOUT_US)) {
		decon_info(decon, "%s : decon update timeout\n", __func__);
		if (atomic_inc_return(&exynos_crtc->d.shadow_timeout_cnt) >= TIMEOUT_CNT) {
			if (__is_recovery_supported(decon)) {
				if (!__is_recovery_running(decon))
					decon_trigger_recovery(exynos_crtc, false);
				else
					goto exit;
			} else {
				dpu_dump_with_event(exynos_crtc);
				dbg_snapshot_expire_watchdog();
				BUG();
			}
		}
	} else
		atomic_set(&exynos_crtc->d.shadow_timeout_cnt, 0);

exit:
	if (mode->op_mode == DECON_COMMAND_MODE &&
			exynos_crtc_state->dsr_status == false &&
			decon->dimming == false) {
		DPU_EVENT_LOG(DPU_EVT_DECON_TRIG_MASK,
				exynos_crtc, NULL);
		decon_reg_set_trigger(decon->id, mode,
				DECON_TRIG_MASK);
	}
}

#if defined(CONFIG_EXYNOS_CMD_SVSYNC)
static bool dpu_svsync_log;
module_param_named(dpu_svsync_print, dpu_svsync_log, bool, 0600);
MODULE_PARM_DESC(dpu_svsync_print,
		"enable print when sv-sync mode works [default : false]");

static bool decon_check_svsync_start(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct decon_config *config;
	bool svsync = false;
	static u32 frame_cnt_a;
	static bool checked;

	config = &decon->config;
	if ((config->out_type & DECON_OUT_DSI) &&
			(config->mode.op_mode == DECON_COMMAND_MODE) &&
			(config->svsync_time)) {
		if (decon_reg_get_trigger_mask(decon->id)) {
			checked = true;
			frame_cnt_a = decon_reg_get_frame_cnt(decon->id);
		} else if (checked) {
			checked = false;
			if (frame_cnt_a < decon_reg_get_frame_cnt(decon->id))
				svsync = true;
		}
	}

	if (dpu_svsync_log) {
		if (svsync)
			decon_info(decon, "frame started due to svsync\n");
		decon_info(decon, "%s -\n", __func__);
	}

	return svsync;
}
#endif


#if defined(CONFIG_EXYNOS_PLL_SLEEP)
/*
 * 1. need to mask PLL_SLEEP to avoid DPU stuck by cmd_lock
 *   add delay as much as the PLL lock time required for PLL_SLEEP exit.
 * 2. PLL_SLEEP_MASK_OUTIF is non-shadow
 *   set as soon as the value(1) is written. (mask)
 *   this prevents PLL_SLEEP at blank time.
 *
 * 3. need to unmask PLL_SLEEP when there is no pending command.
 * 4. PLL_SLEEP_MASK_OUTIF is non-shadow
 *    clear as soon as the value(0) is written. (unmask)
 */
static void
decon_pll_sleep_mask(struct exynos_drm_crtc *exynos_crtc, bool mask)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct decon_config *config;
	bool decon_state_ok = false;

	config = &decon->config;

	if ((config->out_type & DECON_OUT_DSI) &&
		(config->mode.op_mode == DECON_COMMAND_MODE) &&
		(config->mode.dsi_mode != DSI_MODE_DUAL_DSI) &&
		!__is_recovery_running(decon)) {
		decon_debug(decon, "%s +\n", __func__);
		if (decon->state == DECON_STATE_INIT ||
			decon->state == DECON_STATE_ON)
			decon_state_ok = true;
		if (decon_state_ok) {
			decon_debug(decon, "decon_state(%d) mask(%d)\n",
					decon->state, mask);
			if (mask) {
				decon_set_pll_sleep_masked(decon, true);
				decon_reg_set_pll_wakeup(decon->id, 1);
			} else {
				if (decon_get_pll_sleep_masked(decon))
					decon_set_pll_sleep_masked(decon, false);
				else
					decon_reg_set_pll_wakeup(decon->id, 0);
			}
			decon_debug(decon, "decon_state(%d) mask(%d)\n",
						decon->state, mask);
		} else
			decon_info(decon, "decon_state(%d) is not on\n",
						decon->state);
		decon_debug(decon, "%s -\n", __func__);
	}
}
#endif

static void decon_print_config_info(struct decon_device *decon)
{
	char *str_output = NULL;
	char *str_trigger = NULL;

	if (decon->config.mode.trig_mode == DECON_HW_TRIG)
		str_trigger = "hw trigger.";
	else if (decon->config.mode.trig_mode == DECON_SW_TRIG)
		str_trigger = "sw trigger.";
	if (decon->config.mode.op_mode == DECON_VIDEO_MODE)
		str_trigger = "";

	if (decon->config.out_type == DECON_OUT_DSI)
		str_output = "Dual DSI";
	else if (decon->config.out_type & DECON_OUT_DSI0)
		str_output = "DSI0";
	else if  (decon->config.out_type & DECON_OUT_DSI1)
		str_output = "DSI1";
	else if  (decon->config.out_type & DECON_OUT_DP0)
		str_output = "DP0";
	else if  (decon->config.out_type & DECON_OUT_DP1)
		str_output = "DP1";
	else if  (decon->config.out_type & DECON_OUT_WB)
		str_output = "WB";

	decon_info(decon, "%s mode. %s %s output.(%dx%d@%dhz)\n",
			decon->config.mode.op_mode ? "command" : "video",
			str_trigger, str_output,
			decon->config.image_width, decon->config.image_height,
			decon->bts.fps);
}

static void decon_set_te_pinctrl(struct decon_device *decon, bool en)
{
	int ret;

	if ((decon->config.mode.op_mode != DECON_COMMAND_MODE) ||
			(decon->config.mode.trig_mode != DECON_HW_TRIG))
		return;

	if (!decon->res.pinctrl || !decon->res.te_on)
		return;

	ret = pinctrl_select_state(decon->res.pinctrl,
			en ? decon->res.te_on : decon->res.te_off);
	if (ret)
		decon_err(decon, "failed to control decon TE(%d)\n", en);
}

static void decon_enable_irqs(struct decon_device *decon)
{
	decon_reg_set_interrupts(decon->id, 1);

	enable_irq(decon->irq_fd);
	enable_irq(decon->irq_ext);
	if ((decon->irq_sramc_d) >= 0)
		enable_irq(decon->irq_sramc_d);
	if ((decon->irq_sramc1_d) >= 0)
		enable_irq(decon->irq_sramc1_d);
	if (decon_is_te_enabled(decon))
		enable_irq(decon->irq_fs);
}

static void _decon_enable(struct decon_device *decon)
{
	struct drm_crtc *crtc = &decon->crtc->base;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	decon_reg_init(decon->id, &decon->config);
	exynos_dqe_enable(decon->crtc->dqe);
	decon_enable_irqs(decon);

	WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	atomic_set(&exynos_crtc->d.shadow_timeout_cnt, 0);
}

static void decon_mode_set(struct exynos_drm_crtc *crtc,
			   const struct drm_display_mode *mode,
			   const struct drm_display_mode *adjusted_mode)
{
	struct decon_device *decon = crtc->ctx;
	const struct drm_crtc_state *crtc_state = crtc->base.state;
	struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	decon_mode_update_bts(decon, adjusted_mode);

	if (!exynos_crtc_state->seamless_modeset)
		return;

	if (IS_ENABLED(CONFIG_EXYNOS_BTS)) {
		decon->bts.ops->calc_bw(crtc);
		decon->bts.ops->update_bw(crtc, false);
	}

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	DPU_EVENT_LOG_MODE_SET(crtc,
		exynos_crtc_state->seamless_modeset & (SEAMLESS_MODESET_MRES | SEAMLESS_MODESET_VREF),
		(char *)adjusted_mode->name);
#endif
	if (exynos_crtc_state->seamless_modeset & SEAMLESS_MODESET_MRES)
		decon_reg_set_mres(decon->id, &decon->config);
#if defined(CONFIG_DRM_SAMSUNG_EWR)
	if (exynos_crtc_state->seamless_modeset & SEAMLESS_MODESET_VREF)
		decon_reg_update_ewr_control(decon->id, decon->config.fps);
#endif
}

#if IS_ENABLED(CONFIG_EXYNOS_PD)
extern int exynos_pd_booton_rel(const char *pd_name);
#else
static int exynos_pd_booton_rel(const char *pd_name) { return 0; }
#endif

static void
decon_enable(struct exynos_drm_crtc *crtc, struct drm_crtc_state *old_crtc_state)
{
	const struct drm_crtc_state *crtc_state = crtc->base.state;
	struct decon_device *decon = crtc->ctx;
	int i;
#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DP)
	struct dp_device *dp = get_dp_drvdata();

	if ((decon->config.out_type & DECON_OUT_DP)
			&& (dp->hpd_current_state == EXYNOS_HPD_UNPLUG)) {
		decon_info(decon, "%s() hpd is low\n", __func__);
		return;
	}
#endif

	if (drm_atomic_crtc_needs_modeset(crtc_state)) {
		const struct drm_atomic_state *state = old_crtc_state->state;
		const struct exynos_drm_connector_state *exynos_conn_state =
			crtc_get_exynos_connector_state(state, crtc_state);
		const struct exynos_display_mode *exynos_mode = NULL;

		if (exynos_conn_state)
			exynos_mode = &exynos_conn_state->exynos_mode;

		decon_update_config(&decon->config, &crtc_state->adjusted_mode,
				exynos_mode);
	}

	if (decon->state == DECON_STATE_ON) {
		decon_info(decon, "already enabled(%d)\n", decon->state);
		return;
	}

	decon_info(decon, "%s +\n", __func__);

	if (is_tui_trans(old_crtc_state)) {
		decon_info(decon, "tui transition : skip power enable\n");
	} else {
#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
		exynos_pd_booton_rel("pd_dpuf0");
		exynos_pd_booton_rel("pd_dpuf1");
#else
		exynos_pd_booton_rel("pd_dpuf");
#endif

		pm_runtime_get_sync(decon->dev);
		decon_set_te_pinctrl(decon, true);
		if (!__is_recovery_running(decon))
			pm_stay_awake(decon->dev);
	}

	if (decon->config.out_type != DECON_OUT_WB) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		dev_warn(decon->dev, "pm_stay_awake(active : %lu, relax : %lu)\n",
				decon->dev->power.wakeup->active_count,
				decon->dev->power.wakeup->relax_count);
#else
		dev_warn(decon->dev, "pm_stay_awake\n");
#endif
	}

	_decon_enable(decon);

       /*
        * Make sure all window connections are disabled when getting enabled,
	* in case there are any stale mappings.
	* New mappings will happen later before atomic flush.
	* This needs to be modified because it can disable the windows
	* that other decons are using.
        */
       for (i = 0; i < MAX_WIN_PER_DECON; ++i)
               decon_reg_set_win_enable(decon->id, i, 0);

	decon_print_config_info(decon);

	decon->state = DECON_STATE_ON;

	if (IS_ENABLED(CONFIG_EXYNOS_BTS) && decon->bts.ops->set_bus_qos)
		decon->bts.ops->set_bus_qos(decon->crtc);

	DPU_EVENT_LOG(DPU_EVT_DECON_ENABLED, crtc, decon);

	decon_info(decon, "%s -\n", __func__);
}

void decon_exit_hibernation(struct decon_device *decon)
{
	if (decon->state != DECON_STATE_HIBERNATION)
		return;

	decon_debug(decon, "%s +\n", __func__);

	_decon_enable(decon);

	decon->state = DECON_STATE_ON;

	decon_debug(decon, "%s -\n", __func__);
}

static void decon_disable_irqs(struct decon_device *decon)
{
	disable_irq(decon->irq_fd);
	disable_irq(decon->irq_ext);
	if ((decon->irq_sramc_d) >= 0)
		disable_irq(decon->irq_sramc_d);
	if ((decon->irq_sramc1_d) >= 0)
		disable_irq(decon->irq_sramc1_d);
	decon_reg_set_interrupts(decon->id, 0);
	if (decon_is_te_enabled(decon))
		disable_irq(decon->irq_fs);
	if(decon->dimming) {
		decon_debug(decon, "dqe dimming clear\n");
		DPU_EVENT_LOG(DPU_EVT_DQE_DIMEND, decon->crtc, NULL);
		decon->dimming = false;
	}
}

static void _decon_disable(struct decon_device *decon)
{
	struct drm_crtc *crtc = &decon->crtc->base;

	decon_disable_irqs(decon);
	exynos_dqe_disable(decon->crtc->dqe);
	decon_reg_stop(decon->id, &decon->config, true, decon->bts.fps);
	drm_crtc_vblank_put(crtc);
}

void decon_enter_hibernation(struct decon_device *decon)
{
	int i;
	decon_debug(decon, "%s +\n", __func__);

	if (decon->state != DECON_STATE_ON)
		return;

	_decon_disable(decon);

	for (i = 0; i < decon->dpp_cnt; ++i) {
		struct dpp_device *dpp = decon->dpp[i];

		if (dpp->decon_id == decon->id) {
			struct exynos_drm_plane *exynos_plane = &dpp->plane;
			exynos_plane->ops->atomic_disable(exynos_plane);
		}
	}

	decon->state = DECON_STATE_HIBERNATION;
	decon_debug(decon, "%s -\n", __func__);
}


static void decon_disable(struct exynos_drm_crtc *crtc)
{
	struct decon_device *decon = crtc->ctx;

	if (decon->state == DECON_STATE_OFF) {
		decon_info(decon, "already disabled(%d)\n", decon->state);
		return;
	}

	decon_info(decon, "%s +\n", __func__);

	_decon_disable(decon);

	if (is_tui_trans(crtc->base.state)) {
		decon_info(decon, "tui transition : skip power disable\n");
	} else {
		decon_set_te_pinctrl(decon, false);
		pm_runtime_put_sync(decon->dev);
		if (!__is_recovery_running(decon))
			pm_relax(decon->dev);
	}

	if (decon->config.out_type != DECON_OUT_WB) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		dev_warn(decon->dev, "pm_relax(active : %lu, relax : %lu)\n",
				decon->dev->power.wakeup->active_count,
				decon->dev->power.wakeup->relax_count);
#else
		dev_warn(decon->dev, "pm_relax\n");
#endif
	}

	decon->state = DECON_STATE_OFF;

	DPU_EVENT_LOG(DPU_EVT_DECON_DISABLED, crtc, decon);

	decon_info(decon, "%s -\n", __func__);
}

static const struct exynos_drm_crtc_ops decon_crtc_ops = {
	.enable = decon_enable,
	.disable = decon_disable,
	.enable_vblank = decon_enable_vblank,
	.disable_vblank = decon_disable_vblank,
	.mode_set = decon_mode_set,
	.atomic_check = decon_atomic_check,
	.atomic_begin = decon_atomic_begin,
	.update_plane = decon_update_plane,
	.disable_plane = decon_disable_plane,
	.atomic_flush = decon_atomic_flush,
	.atomic_print_state = decon_atomic_print_state,
	.late_register = decon_late_register,
	.wait_framestart = decon_wait_framestart,
	.set_trigger = decon_set_trigger,
#if defined(CONFIG_EXYNOS_CMD_SVSYNC)
	.check_svsync_start = decon_check_svsync_start,
#endif
	.dump_register = decon_dump,
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	.dump_event_log = decon_dump_event_log,
#endif
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	.pll_sleep_mask = decon_pll_sleep_mask,
#endif
	.recovery = decon_trigger_recovery,
	.is_recovering = decon_read_recovering,
};

#if IS_ENABLED(CONFIG_DRM_DPU_EVENT_LOG_FOR_USER)
static int dpu_event_log_set(const char *val, const struct kernel_param *kp)
{
	unsigned int *pvalue = kp->arg;
	unsigned int res = param_set_uint(val, kp);

	if (*pvalue >= 0 && *pvalue < 3) {
		pr_info("Print decon(%d) event log\n", *pvalue);
		dpu_event(decon_drvdata[*pvalue]->crtc);
	} else
		pr_warn("Wrong value of set param(%d)\n", *pvalue);

	return res;
}

static const struct kernel_param_ops dpu_event_log_ops = {
	.set = dpu_event_log_set,
	.get = param_get_uint,
};

unsigned int param = 0xff;
module_param_cb(dpu_event_log, &dpu_event_log_ops, &param, 0644);
__MODULE_PARM_TYPE(dpu_event_log, "uint");
MODULE_PARM_DESC(dpu_event_log,
		 "FOR DPU_DEBUG. Use event log to get trace of DPU D/D operation.");
#endif


static int decon_bind(struct device *dev, struct device *master, void *data)
{
	struct decon_device *decon = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct exynos_drm_private *priv = drm_dev->dev_private;
	struct drm_plane *default_plane;
	enum exynos_drm_op_mode op_mode;
	int i;

	decon->drm_dev = drm_dev;

	default_plane = &decon->dpp[decon->id]->plane.base;

	op_mode = decon->config.mode.op_mode ?
		EXYNOS_COMMAND_MODE : EXYNOS_VIDEO_MODE;
	decon->crtc = exynos_drm_crtc_create(drm_dev, default_plane,
			decon->con_type, op_mode, &decon_crtc_ops, decon);
	if (IS_ERR(decon->crtc))
		return PTR_ERR(decon->crtc);

	decon->crtc->bts = &decon->bts;
	decon->crtc->dev = dev;
	decon->crtc->partial = exynos_partial_register(decon->crtc);

	for (i = 0; i < decon->dpp_cnt; ++i) {
		struct dpp_device *dpp = decon->dpp[i];
		struct drm_plane *plane = &dpp->plane.base;

		plane->possible_crtcs |=
			drm_crtc_mask(&decon->crtc->base);
		decon_debug(decon, "plane possible_crtcs = 0x%x\n",
				plane->possible_crtcs);
	}

	priv->iommu_client = dev;

	iommu_register_device_fault_handler(dev, dpu_sysmmu_fault_handler,
			decon->crtc);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON)
	decon->crtc->d.itmon_nb.notifier_call = dpu_itmon_notifier;
	itmon_notifier_chain_register(&decon->crtc->d.itmon_nb);
#endif

	if (IS_ENABLED(CONFIG_EXYNOS_BTS)) {
		decon->bts.ops = &dpu_bts_control;
		decon->bts.ops->init(decon->crtc);
	}

	decon_debug(decon, "%s -\n", __func__);
	return 0;
}

static void decon_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	decon_debug(decon, "%s +\n", __func__);
	if (IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->deinit(decon->crtc);

	decon_disable(decon->crtc);
	decon_debug(decon, "%s -\n", __func__);
}

static const struct component_ops decon_component_ops = {
	.bind	= decon_bind,
	.unbind = decon_unbind,
};

static bool dpu_frame_time_check = false;
module_param(dpu_frame_time_check, bool, 0600);
MODULE_PARM_DESC(dpu_frame_time_check, "dpu frame time check for dpu bts [default : false]");

static bool dpu_sfr_dump = false;
module_param(dpu_sfr_dump, bool, 0600);
MODULE_PARM_DESC(dpu_sfr_dump, "dpu sfr dump [default : false, reset after sfr dumping]");
static irqreturn_t decon_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;
	u32 irq_sts_reg;
	u32 ext_irq = 0;
	struct timespec64 tv, tv_d;
	static ktime_t timestamp_s;
	ktime_t timestamp_d;
	long elapsed_t;

	spin_lock(&decon->slock);
	if (decon->state != DECON_STATE_ON)
		goto irq_end;

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id, &ext_irq);
	decon_debug(decon, "%s: irq_sts_reg = %x, ext_irq = %x\n",
			__func__, irq_sts_reg, ext_irq);

	if (irq_sts_reg & DPU_FRAME_START_INT_PEND) {
		decon->busy = true;
		complete(&decon->framestart_done);
		DPU_EVENT_LOG(DPU_EVT_DECON_FRAMESTART, exynos_crtc, exynos_crtc);
		if ((decon->config.out_type & DECON_OUT_DSI) &&
			(decon->config.mode.op_mode == DECON_COMMAND_MODE)) {
			timestamp_s = ktime_get();
			tv = ktime_to_timespec64(timestamp_s);
			decon_debug(decon, "[%6ld.%06ld] frame start\n",
					tv.tv_sec, (tv.tv_nsec / 1000));
		} else
			decon_debug(decon, "%s: frame start\n", __func__);
		if (dpu_sfr_dump) {
			decon_dump(exynos_crtc);
			dpu_sfr_dump = false;
		}
	}

	if (irq_sts_reg & DPU_FRAME_DONE_INT_PEND) {
		DPU_EVENT_LOG(DPU_EVT_DECON_FRAMEDONE, exynos_crtc, exynos_crtc);
		decon->busy = false;
		wake_up_interruptible_all(&decon->framedone_wait);
		if (timestamp_s &&
			(decon->config.out_type & DECON_OUT_DSI) &&
			(decon->config.mode.op_mode == DECON_COMMAND_MODE)) {
			timestamp_d = ktime_get();
			tv_d = ktime_to_timespec64(timestamp_d - timestamp_s);
			tv = ktime_to_timespec64(timestamp_d);
			decon_debug(decon, "[%6ld.%06ld] frame done\n",
					tv.tv_sec, (tv.tv_nsec / 1000));
			/*
			 * Elapsed time is wanted under
			 * the calculated value based on 1.01 x fps.
			 * In addition, idle time should also be considered.
			 */
			elapsed_t = 1000000000L / decon->bts.fps * 100 / 101;
			elapsed_t = elapsed_t - 500000L;
			if (tv_d.tv_nsec > elapsed_t)
				decon_warn(decon, "elapsed(%3ld.%03ldmsec)\n",
						(tv_d.tv_nsec / 1000000U),
						(tv_d.tv_nsec % 1000000U));
			else if (dpu_frame_time_check)
				decon_info(decon, "elapsed(%3ld.%03ldmsec)\n",
						(tv_d.tv_nsec / 1000000U),
						(tv_d.tv_nsec % 1000000U));
			else
				decon_debug(decon, "elapsed(%3ld.%03ldmsec)\n",
						(tv_d.tv_nsec / 1000000U),
						(tv_d.tv_nsec % 1000000U));
		} else
			decon_debug(decon, "%s: frame done\n", __func__);
	}

	if (irq_sts_reg & DPU_DQE_DIMMING_START_INT_PEND) {
		decon_debug(decon, "dqe dimming start\n");
		DPU_EVENT_LOG(DPU_EVT_DQE_DIMSTART, exynos_crtc, NULL);
		decon->dimming = true;
	}

	if (irq_sts_reg & DPU_DQE_DIMMING_END_INT_PEND) {
		decon_debug(decon, "dqe dimming end\n");
		DPU_EVENT_LOG(DPU_EVT_DQE_DIMEND, exynos_crtc, NULL);
		decon->dimming = false;
	}

	if (ext_irq & DPU_RESOURCE_CONFLICT_INT_PEND)
		decon_debug(decon, "%s: resource conflict\n", __func__);

	if (ext_irq & DPU_TIME_OUT_INT_PEND) {
		decon_err(decon, "%s: timeout irq occurs\n", __func__);
		decon_dump(exynos_crtc);
		WARN_ON(1);
	}

irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

static int decon_parse_dt(struct decon_device *decon, struct device_node *np)
{
	struct device_node *dpp_np = NULL;
	struct property *prop;
	const __be32 *cur;
	u32 val;
	int ret = 0, i;
	u32 dfs_lv_cnt, dfs_lv[BTS_DFS_MAX] = {400000, 0, };
	bool err_flag = false;
	int dpp_cnt, dpp_id;

	of_property_read_u32(np, "decon,id", &decon->id);

	ret = of_property_read_u32(np, "max_win", &decon->win_cnt);
	if (ret) {
		decon_err(decon, "failed to parse max windows count\n");
		return ret;
	}

	ret = of_property_read_u32(np, "op_mode", &decon->config.mode.op_mode);
	if (ret) {
		decon_err(decon, "failed to parse operation mode(%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "trig_mode",
			&decon->config.mode.trig_mode);
	if (ret) {
		decon_err(decon, "failed to parse trigger mode(%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "rd_en", &decon->config.urgent.rd_en);
	if (ret)
		decon_warn(decon, "failed to parse urgent rd_en(%d)\n", ret);

	ret = of_property_read_u32(np, "rd_hi_thres",
			&decon->config.urgent.rd_hi_thres);
	if (ret) {
		decon_warn(decon, "failed to parse urgent rd_hi_thres(%d)\n",
				ret);
	}

	ret = of_property_read_u32(np, "rd_lo_thres",
			&decon->config.urgent.rd_lo_thres);
	if (ret) {
		decon_warn(decon, "failed to parse urgent rd_lo_thres(%d)\n",
				ret);
	}

	ret = of_property_read_u32(np, "rd_wait_cycle",
			&decon->config.urgent.rd_wait_cycle);
	if (ret) {
		decon_warn(decon, "failed to parse urgent rd_wait_cycle(%d)\n",
				ret);
	}

	ret = of_property_read_u32(np, "wr_en", &decon->config.urgent.wr_en);
	if (ret)
		decon_warn(decon, "failed to parse urgent wr_en(%d)\n", ret);

	ret = of_property_read_u32(np, "wr_hi_thres",
			&decon->config.urgent.wr_hi_thres);
	if (ret) {
		decon_warn(decon, "failed to parse urgent wr_hi_thres(%d)\n",
				ret);
	}

	ret = of_property_read_u32(np, "wr_lo_thres",
			&decon->config.urgent.wr_lo_thres);
	if (ret) {
		decon_warn(decon, "failed to parse urgent wr_lo_thres(%d)\n",
				ret);
	}

	decon->config.urgent.dta_en = of_property_read_bool(np, "dta_en");
	if (decon->config.urgent.dta_en) {
		ret = of_property_read_u32(np, "dta_hi_thres",
				&decon->config.urgent.dta_hi_thres);
		if (ret) {
			decon_err(decon, "failed to parse dta_hi_thres(%d)\n",
					ret);
		}
		ret = of_property_read_u32(np, "dta_lo_thres",
				&decon->config.urgent.dta_lo_thres);
		if (ret) {
			decon_err(decon, "failed to parse dta_lo_thres(%d)\n",
					ret);
		}
	}

	ret = of_property_read_u32(np, "out_type", &decon->config.out_type);
	if (ret) {
		decon_err(decon, "failed to parse output type(%d)\n", ret);
		return ret;
	}

	if (decon->config.mode.trig_mode == DECON_HW_TRIG) {
		ret = of_property_read_u32(np, "te_from",
				&decon->config.te_from);
		if (ret) {
			decon_err(decon, "failed to get TE from DDI\n");
			return ret;
		}
		if (decon->config.te_from >= MAX_DECON_TE_FROM_DDI) {
			decon_err(decon, "TE from DDI is wrong(%d)\n",
					decon->config.te_from);
			return ret;
		}
		decon_info(decon, "TE from DDI%d\n", decon->config.te_from);
	} else {
		decon->config.te_from = MAX_DECON_TE_FROM_DDI;
		decon_info(decon, "TE from NONE\n");
	}

	if (of_property_read_u32(np, "svsync_time_us",
					&decon->config.svsync_time))
		decon->config.svsync_time = 0;
	else
		decon_info(decon, "svsync_time is defined as %dusec in DT.\n",
					decon->config.svsync_time);

	if (of_property_read_u32(np, "ppc", (u32 *)&decon->bts.ppc))
		decon->bts.ppc = 2UL;
	decon_info(decon, "PPC(%llu)\n", decon->bts.ppc);

	if (of_property_read_u32(np, "ppc_rotator",
					(u32 *)&decon->bts.ppc_rotator)) {
		decon->bts.ppc_rotator = 8U;
		decon_warn(decon, "WARN: rotator ppc is not defined in DT.\n");
	}
	decon_info(decon, "rotator ppc(%d)\n", decon->bts.ppc_rotator);

	if (of_property_read_u32(np, "ppc_scaler",
					(u32 *)&decon->bts.ppc_scaler)) {
		decon->bts.ppc_scaler = 4U;
		decon_warn(decon, "WARN: scaler ppc is not defined in DT.\n");
	}
	decon_info(decon, "scaler ppc(%d)\n", decon->bts.ppc_scaler);

	if (of_property_read_u32(np, "delay_comp",
				(u32 *)&decon->bts.delay_comp)) {
		decon->bts.delay_comp = 4UL;
		decon_warn(decon, "WARN: comp line delay is not defined in DT.\n");
	}
	decon_info(decon, "line delay comp(%d)\n", decon->bts.delay_comp);

	if (of_property_read_u32(np, "delay_scaler",
				(u32 *)&decon->bts.delay_scaler)) {
		decon->bts.delay_scaler = 2UL;
		decon_warn(decon, "WARN: scaler line delay is not defined in DT.\n");
	}
	decon_info(decon, "line delay scaler(%d)\n", decon->bts.delay_scaler);

	if (of_property_read_u32(np, "inner_width",
				(u32 *)&decon->bts.inner_width)) {
		decon->bts.inner_width = 16UL;
		decon_warn(decon, "WARN: internal process width is not defined in DT.\n");
	}
	decon_info(decon, "internal process width(%d)\n", decon->bts.inner_width);

	if (of_property_read_u32(np, "bus_width",
				&decon->bts.bus_width)) {
		decon->bts.bus_width = 32UL;
		decon_warn(decon, "WARN: bus width is not defined in DT.\n");
	}
	if (of_property_read_u32(np, "rot_util",
				&decon->bts.rot_util)) {
		decon->bts.rot_util = 60UL;
		decon_warn(decon, "WARN: rot util is not defined in DT.\n");
	}
	decon_info(decon, "bus_width(%d) rot_util(%d)\n",
			decon->bts.bus_width, decon->bts.rot_util);

	if (of_property_read_u32(np, "dfs_lv_cnt", &dfs_lv_cnt)) {
		err_flag = true;
		dfs_lv_cnt = 1;
		decon->bts.dfs_lv[0] = 400000U; /* 400Mhz */
		decon_warn(decon, "WARN: DPU DFS Info is not defined in DT.\n");
	}
	decon->bts.dfs_lv_cnt = dfs_lv_cnt;

	if (dfs_lv_cnt > BTS_DFS_MAX) {
		decon_warn(decon, "dfs_lv_cnt(%d) > BTS_DFS_MAX(%d)"
				"increase BTS_DFS_MAX" ,dfs_lv_cnt ,BTS_DFS_MAX);
			return -EINVAL;
	}

	if (!err_flag) {
		of_property_read_u32_array(np, "dfs_lv",
					dfs_lv, dfs_lv_cnt);

		decon_info(decon, KERN_ALERT "DPU DFS Level : ");
		for (i = 0; i < dfs_lv_cnt; i++) {
			decon->bts.dfs_lv[i] = dfs_lv[i];
			decon_info(decon, KERN_CONT "%6d ", dfs_lv[i]);
		}
	}

	if (decon->config.out_type == DECON_OUT_DSI)
		decon->config.mode.dsi_mode = DSI_MODE_DUAL_DSI;
	else if (decon->config.out_type & (DECON_OUT_DSI0 | DECON_OUT_DSI1))
		decon->config.mode.dsi_mode = DSI_MODE_SINGLE;
	else
		decon->config.mode.dsi_mode = DSI_MODE_NONE;

	dpp_cnt = of_count_phandle_with_args(np, "dpps", NULL);
	if (dpp_cnt <= 0) {
		decon_err(decon, "failed to get dpp_cnt\n");
		return -ENODEV;
	}
	decon->dpp_cnt = dpp_cnt;

	for (i = 0; i < decon->dpp_cnt; ++i) {
		dpp_np = of_parse_phandle(np, "dpps", i);
		if (!dpp_np) {
			decon_err(decon, "can't find dpp%d node\n", i);
			return -EINVAL;
		}

		decon->dpp[i] = of_find_dpp_by_node(dpp_np);
		if (!decon->dpp[i]) {
			decon_err(decon, "can't find dpp%d structure\n", i);
			return -EINVAL;
		}

		dpp_id = decon->dpp[i]->id;
		decon_info(decon, "found dpp%d\n", dpp_id);

		if (dpp_np)
			of_node_put(dpp_np);
	}

	of_property_for_each_u32(np, "connector", prop, cur, val)
		decon->con_type |= val;

	return 0;
}

static int decon_remap_by_name(struct decon_device *decon, struct device_node *np,
		void __iomem **base, const char *reg_name, enum decon_regs_type type)
{
	int i;

	i = of_property_match_string(np, "reg-names", reg_name);
	*base = of_iomap(np, i);
	if (!(*base)) {
		decon_err(decon, "failed to remap %s SFR region\n", reg_name);
		return -EINVAL;
	}
	decon_regs_desc_init(*base, reg_name, type, decon->id);

	return 0;

}

static int decon_remap_regs(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct device_node *np = dev->of_node;

	if (decon_remap_by_name(decon, np, &decon->regs.regs, "main", REGS_DECON))
		goto err;

	if (decon_remap_by_name(decon, np, &decon->regs.win_regs,"win",
				REGS_DECON_WIN))
		goto err_main;

	if (decon_remap_by_name(decon, np, &decon->regs.sub_regs, "sub",
				REGS_DECON_SUB))
		goto err_win;

	if (decon_remap_by_name(decon, np, &decon->regs.wincon_regs, "wincon",
				REGS_DECON_WINCON))
		goto err_sub;

	if (decon_remap_by_name(decon, np, &decon->regs.sramc_d_regs[0],
				"sramc_d", REGS_DECON_SRAMC))
		goto err_wincon;

	if (decon_remap_by_name(decon, np, &decon->regs.sramc_d_regs[1],
				"sramc1_d", REGS_DECON_SRAMC1))
		goto err_sramc_d;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos9-disp_ss");
	if (IS_ERR_OR_NULL(np)) {
		decon_err(decon, "failed to find disp_ss node");
		goto err_sramc1_d;
	}

	if (decon_remap_by_name(decon, np, &decon->regs.ss_regs, "sys",
				REGS_DECON_SYS))
		goto err_sramc1_d;

	return 0;

err_sramc1_d:
	iounmap(decon->regs.sramc_d_regs[1]);
err_sramc_d:
	iounmap(decon->regs.sramc_d_regs[0]);
err_wincon:
	iounmap(decon->regs.wincon_regs);
err_sub:
	iounmap(decon->regs.sub_regs);
err_win:
	iounmap(decon->regs.win_regs);
err_main:
	iounmap(decon->regs.regs);
err:
	return -EINVAL;
}

static bool dpu_te_duration_check;
module_param(dpu_te_duration_check, bool, 0600);
MODULE_PARM_DESC(dpu_te_duration_check, "dpu te duration check [default : false]");

static irqreturn_t decon_te_irq_handler(int irq, void *dev_id)
{
	struct decon_device *decon = dev_id;
	struct exynos_hibernation *hibernation;
	struct exynos_drm_crtc *exynos_crtc;

	DPU_ATRACE_BEGIN("te_signal");
	if (!decon)
		goto end;

	exynos_crtc = decon->crtc;

	decon_debug(decon, "%s: state(%d)\n", __func__, decon->state);
	if (dpu_te_duration_check) {
		static ktime_t timestamp_s;
		ktime_t timestamp_d = ktime_get();
		s64 diff_usec = ktime_to_us(ktime_sub(timestamp_d, timestamp_s));

		if (timestamp_s)
			decon_info(decon, "vsync elapsed(%3ld.%03ldmsec)\n",
					(diff_usec / USEC_PER_MSEC),
					(diff_usec % USEC_PER_MSEC));
		timestamp_s = timestamp_d;
	}

	if (decon->state != DECON_STATE_ON &&
			decon->state != DECON_STATE_HIBERNATION)
		goto end;

	DPU_EVENT_LOG(DPU_EVT_TE_INTERRUPT, exynos_crtc, NULL);

	if (exynos_crtc->migov)
		exynos_migov_update_vsync_cnt(exynos_crtc->migov);

	hibernation = exynos_crtc->hibernation;

	if (hibernation && !is_hibernaton_blocked(hibernation))
		kthread_queue_work(&exynos_crtc->worker, &hibernation->work);

	if (decon->config.mode.op_mode == DECON_COMMAND_MODE)
		drm_crtc_handle_vblank(&exynos_crtc->base);

end:
	DPU_ATRACE_END("te_signal");
	return IRQ_HANDLED;
}

static irqreturn_t sramc_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;

	spin_lock(&decon->slock);
	if (decon->state != DECON_STATE_ON)
		goto irq_end;

	sramc_d_reg_get_irq_and_clear(decon->id);

	decon_err(decon, "%s: sramc error irq occurs\n", __func__);
	if (IS_ENABLED(CONFIG_EXYNOS_BTS))
		decon->bts.ops->print_info(decon->crtc);

	dbg_snapshot_expire_watchdog();
	decon_dump(decon->crtc);
	WARN_ON(1);

irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}


static int decon_register_irqs(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	int ret = 0;
	int gpio;

	pdev = container_of(dev, struct platform_device, dev);

	/* 1: FRAME START */
	decon->irq_fs = of_irq_get_byname(np, "frame_start");
	ret = devm_request_irq(dev, decon->irq_fs, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err(decon, "failed to install FRAME START irq\n");
		return ret;
	}
	disable_irq(decon->irq_fs);

	/* 2: FRAME DONE */
	decon->irq_fd = of_irq_get_byname(np, "frame_done");
	ret = devm_request_irq(dev, decon->irq_fd, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err(decon, "failed to install FRAME DONE irq\n");
		return ret;
	}
	disable_irq(decon->irq_fd);

	/* 3: EXTRA: resource conflict, timeout and error irq */
	decon->irq_ext = of_irq_get_byname(np, "extra");
	ret = devm_request_irq(dev, decon->irq_ext, decon_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err(decon, "failed to install EXTRA irq\n");
		return ret;
	}
	disable_irq(decon->irq_ext);

	/* 4: SRAMC_D: sram controller error irq */
	decon->irq_sramc_d = of_irq_get_byname(np, "sramc_d");
	if ((decon->irq_sramc_d) >= 0) {
		ret = devm_request_irq(dev, decon->irq_sramc_d, sramc_irq_handler,
				0, pdev->name, decon);
		if (ret) {
			decon_err(decon, "failed to install SRAMC_D irq\n");
			return ret;
		}
		disable_irq(decon->irq_sramc_d);
	}

	decon->irq_sramc1_d = of_irq_get_byname(np, "sramc1_d");
	if ((decon->irq_sramc1_d) >= 0) {
		ret = devm_request_irq(dev, decon->irq_sramc1_d, sramc_irq_handler,
				0, pdev->name, decon);
		if (ret) {
			decon_err(decon, "failed to install SRAMC1_D irq\n");
			return ret;
		}
		disable_irq(decon->irq_sramc1_d);
	}

	/*
	 * Get IRQ resource and register IRQ handler. Only enabled in command
	 * mode.
	 */
	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		gpio = of_get_gpio(dev->of_node, 0);
		if (gpio < 0) {
			decon_err(decon, "failed to get TE gpio\n");
			return -ENODEV;
		}
	} else {
		decon_debug(decon, "failed to find TE gpio node\n");
		return 0;
	}

	decon->irq_te = gpio_to_irq(gpio);

	decon_info(decon, "TE irq number(%d)\n", decon->irq_te);
	irq_set_status_flags(decon->irq_te, IRQ_DISABLE_UNLAZY);
	ret = devm_request_irq(dev, decon->irq_te, decon_te_irq_handler,
			IRQF_TRIGGER_RISING, pdev->name, decon);
	disable_irq(decon->irq_te);

	return ret;
}

static int decon_get_pinctrl(struct decon_device *decon)
{
	int ret = 0;

	decon->res.pinctrl = devm_pinctrl_get(decon->dev);
	if (IS_ERR(decon->res.pinctrl)) {
		decon_debug(decon, "failed to get pinctrl\n");
		ret = PTR_ERR(decon->res.pinctrl);
		decon->res.pinctrl = NULL;
		/* optional in video mode */
		return 0;
	}

	decon->res.te_on = pinctrl_lookup_state(decon->res.pinctrl, "hw_te_on");
	if (IS_ERR(decon->res.te_on)) {
		decon_err(decon, "failed to get hw_te_on pin state\n");
		ret = PTR_ERR(decon->res.te_on);
		decon->res.te_on = NULL;
		goto err;
	}
	decon->res.te_off = pinctrl_lookup_state(decon->res.pinctrl,
			"hw_te_off");
	if (IS_ERR(decon->res.te_off)) {
		decon_err(decon, "failed to get hw_te_off pin state\n");
		ret = PTR_ERR(decon->res.te_off);
		decon->res.te_off = NULL;
		goto err;
	}

err:
	return ret;
}

#ifndef CONFIG_BOARD_EMULATOR
static int decon_get_clock(struct decon_device *decon)
{
	decon->res.aclk = devm_clk_get(decon->dev, "aclk");
	if (IS_ERR_OR_NULL(decon->res.aclk)) {
		decon_info(decon, "failed to get aclk(optional)\n");
		decon->res.aclk = NULL;
	}

	decon->res.aclk_disp = devm_clk_get(decon->dev, "aclk-disp");
	if (IS_ERR_OR_NULL(decon->res.aclk_disp)) {
		decon_info(decon, "failed to get aclk_disp(optional)\n");
		decon->res.aclk_disp = NULL;
	}

	return 0;
}
#else
static inline int decon_get_clock(struct decon_device *decon) { return 0; }
#endif

static int decon_init_resources(struct decon_device *decon)
{
	int ret = 0;

	ret = decon_remap_regs(decon);
	if (ret)
		goto err;

	ret = decon_register_irqs(decon);
	if (ret)
		goto err;

	ret = decon_get_pinctrl(decon);
	if (ret)
		goto err;

	ret = decon_get_clock(decon);
	if (ret)
		goto err;
err:
	return ret;
}

#define EXYNOS_DRM_DECON_WA_FIRST_ENABLE
static int decon_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct decon_device *decon;
	struct device *dev = &pdev->dev;

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon)
		return -ENOMEM;

	decon->dev = dev;

	ret = decon_parse_dt(decon, dev->of_node);
	if (ret)
		goto err;

	decon_drvdata[decon->id] = decon;

	spin_lock_init(&decon->slock);
	init_completion(&decon->framestart_done);
	init_waitqueue_head(&decon->framedone_wait);

	decon->state = DECON_STATE_OFF;
	pm_runtime_enable(decon->dev);

	/* prevent sleep enter during display(LCD, DP) on */
	ret = device_init_wakeup(decon->dev, true);
	if (ret) {
		dev_err(decon->dev, "failed to init wakeup device\n");
		goto err;
	}

	ret = decon_init_resources(decon);
	if (ret)
		goto err;

	/* set drvdata */
	platform_set_drvdata(pdev, decon);

	exynos_recovery_register(decon);

	ret = component_add(dev, &decon_component_ops);
	if (ret)
		goto err;

#ifdef EXYNOS_DRM_DECON_WA_FIRST_ENABLE
	if (decon->id == 0)
		decon_reg_stop(decon->id, &decon->config, true, decon->bts.fps);
#endif
	decon_info(decon, "successfully probed");

err:
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);

	component_del(&pdev->dev, &decon_component_ops);

	iounmap(decon->regs.ss_regs);
	iounmap(decon->regs.sramc_d_regs[1]);
	iounmap(decon->regs.sramc_d_regs[0]);
	iounmap(decon->regs.wincon_regs);
	iounmap(decon->regs.sub_regs);
	iounmap(decon->regs.win_regs);
	iounmap(decon->regs.regs);

	return 0;
}

#ifdef CONFIG_PM
static int decon_runtime_suspend(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	if (decon->res.aclk)
		clk_disable_unprepare(decon->res.aclk);

	if (decon->res.aclk_disp)
		clk_disable_unprepare(decon->res.aclk_disp);

	decon_debug(decon, "runtime suspended\n");

	return 0;
}

static int decon_runtime_resume(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	if (decon->res.aclk)
		clk_prepare_enable(decon->res.aclk);

	if (decon->res.aclk_disp)
		clk_prepare_enable(decon->res.aclk_disp);

	decon_debug(decon, "runtime resumed\n");

	return 0;
}

static int decon_suspend(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	exynos_dqe_suspend(decon->crtc->dqe);

	decon_debug(decon, "suspended\n");

	return 0;
}

static int decon_resume(struct device *dev)
{
	struct decon_device *decon = dev_get_drvdata(dev);

	exynos_dqe_resume(decon->crtc->dqe);

	decon_debug(decon, "resumed\n");

	return 0;
}

static const struct dev_pm_ops decon_pm_ops = {
	SET_RUNTIME_PM_OPS(decon_runtime_suspend, decon_runtime_resume, NULL)
	.suspend	= decon_suspend,
	.resume		= decon_resume,
};
#endif

struct platform_driver decon_driver = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.driver		= {
		.name	= "exynos-drmdecon",
#ifdef CONFIG_PM
		.pm	= &decon_pm_ops,
#endif
		.of_match_table = decon_driver_dt_match,
	},
};

MODULE_AUTHOR("Hyung-jun Kim <hyungjun07.kim@samsung.com>");
MODULE_AUTHOR("Seong-gyu Park <seongyu.park@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC Display and Enhancement Controller");
MODULE_LICENSE("GPL v2");
