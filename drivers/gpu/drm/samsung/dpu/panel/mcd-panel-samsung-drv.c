// SPDX-License-Identifier: GPL-2.0-only
/*
 * MIPI-DSI based Samsung common panel driver.
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_detect.h>

#include <drm/drm_panel.h>
#include <drm/drm_encoder.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include <exynos_drm_decon.h>
#include <exynos_display_common.h>
#include "../exynos_drm_crtc.h"
#include <exynos_drm_connector.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_tui.h>
#include <mcd_drm_decon.h>
#include <mcd_drm_dsim.h>
#include "../exynos_drm_dqe.h"
#include "panel-samsung-drv.h"
#include "mcd-panel-samsung-helper.h"
#include "usdm_panel_drv.h"
#if IS_ENABLED(CONFIG_PANEL_FREQ_HOP) || IS_ENABLED(CONFIG_USDM_PANEL_FREQ_HOP)
#include "usdm_panel_freq_hop.h"
#endif

#define MCD_PANEL_PROBE_DELAY_MSEC (5000)

#define call_mcd_panel_func(p, func, args...) \
	(((p) && (p)->funcs && (p)->funcs->func) ? (p)->funcs->func(p, ##args) : -EINVAL)

static int panel_log_level = 6;
module_param(panel_log_level, int, 0644);
MODULE_PARM_DESC(panel_log_level, "log level for panel drv [default : 6]");

int usdm_get_panel_log_level(void)
{
	return panel_log_level;
}
EXPORT_SYMBOL(usdm_get_panel_log_level);

static int panel_cmd_log;
module_param(panel_cmd_log, int, 0644);
MODULE_PARM_DESC(panel_cmd_log, "log level for panel command [default : 0]");

static int exynos_panel_get_bts_fps(const struct exynos_panel *ctx,
				     const struct exynos_panel_mode *pmode);
static int mcd_drm_panel_set_display_mode(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode);

struct edid panel_edid;

static inline bool need_panel_recovery(struct exynos_panel *ctx)
{
	const struct drm_connector_state *conn_state =
		ctx->exynos_connector.base.state;
	struct drm_crtc *crtc = conn_state->crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct exynos_drm_connector *exynos_conn;
	struct exynos_drm_connector_state *exynos_conn_state;

	if (!crtc)
		return false;

	exynos_crtc = to_exynos_crtc(crtc);

	if (!exynos_crtc || !exynos_crtc->ops)
		return false;

	exynos_conn = &ctx->exynos_connector;
	if (!exynos_conn)
		return false;

	exynos_conn_state = to_exynos_connector_state(exynos_conn->base.state);
	if (!exynos_conn_state)
		return false;

	if (exynos_crtc->ops->is_recovering &&
		exynos_crtc->ops->is_recovering(exynos_crtc) &&
		!exynos_conn_state->requested_panel_recovery) {
		return true;
	}

	return false;
}

static inline bool is_panel_tui(struct exynos_panel *ctx)
{
	const struct drm_connector_state *conn_state =
		ctx->exynos_connector.base.state;

	if (conn_state && conn_state->crtc)
		return usdm_is_tui_trans(conn_state->crtc->state);

	return false;
}

int usdm_mcd_drm_panel_get_modes(struct drm_panel *panel, struct drm_connector *conn)
{
	struct exynos_panel *ctx =
		container_of(panel, struct exynos_panel, panel);
	struct drm_display_mode *preferred_mode = NULL;
	int i;

	panel_debug(ctx, "+\n");

	for (i = 0; i < (int)ctx->desc->num_modes; i++) {
		const struct exynos_panel_mode *pmode = &ctx->desc->modes[i];
		struct drm_display_mode *mode;

		mode = drm_mode_duplicate(conn->dev, &pmode->mode);
		if (!mode) {
			panel_err(ctx, "failed to add mode %s\n", pmode->mode.name);
			return -ENOMEM;
		}

		mode->type |= DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(conn, mode);

		panel_debug(ctx, "added display mode: %s\n", mode->name);

		if (!preferred_mode || (mode->type & DRM_MODE_TYPE_PREFERRED))
			preferred_mode = mode;
	}

	if (preferred_mode) {
		panel_debug(ctx, "preferred display mode: %s\n",
				preferred_mode->name);
		preferred_mode->type |= DRM_MODE_TYPE_PREFERRED;
		conn->display_info.width_mm = preferred_mode->width_mm;
		conn->display_info.height_mm = preferred_mode->height_mm;
	}

	panel_debug(ctx, "-\n");

	return i;
}
EXPORT_SYMBOL(usdm_mcd_drm_panel_get_modes);

void usdm_exynos_panel_set_lp_mode(struct exynos_panel *ctx, const struct exynos_panel_mode *pmode)
{
	int ret;

	if (!ctx->enabled) {
		panel_info(ctx, "exynos panel is disabed\n");
		return;
	}

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, doze);
	if (ret < 0)
		panel_err(ctx, "mcd_panel doze failed(ret:%d)\n", ret);
}
EXPORT_SYMBOL(usdm_exynos_panel_set_lp_mode);

static void exynos_panel_connector_print_state(struct drm_printer *p,
		const struct exynos_drm_connector_state *exynos_conn_state)
{
	const struct exynos_drm_connector *exynos_conn =
		to_exynos_connector(exynos_conn_state->base.connector);
	const struct exynos_display_mode *exynos_mode =
		&exynos_conn_state->exynos_mode;
	const struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_panel_desc *desc = ctx->desc;

	drm_printf(p, "\tenabled: %d\n", ctx->enabled);
	if (ctx->current_mode) {
		const struct drm_display_mode *m = &ctx->current_mode->mode;

		drm_printf(p, " \tcurrent mode: %s\n", m->name);
	}
	if (exynos_mode) {
		const struct exynos_display_dsc *dsc = &exynos_mode->dsc;

		drm_printf(p, " \tcurrent exynos_mode:\n");
		drm_printf(p, " \t\tdsc: en=%d dsc_cnt=%d slice_cnt=%d slice_h=%d\n",
				dsc->enabled, dsc->dsc_count, dsc->slice_count,
				dsc->slice_height);
		drm_printf(p, " \t\tpanel bpc: %d\n", exynos_mode->bpc);
		drm_printf(p, " \t\top_mode: %s\n", exynos_mode->mode_flags &
				MIPI_DSI_MODE_VIDEO ? "video" : "cmd");
		drm_printf(p, " \t\tlp_mode_state: %d\n", exynos_mode->is_lp_mode);
		drm_printf(p, " \t\tbts_fps: %d\n", exynos_mode->bts_fps);
	}
	drm_printf(p, "\tluminance: [%u, %u] avg: %u\n",
		   desc->min_luminance, desc->max_luminance,
		   desc->max_avg_luminance);
	drm_printf(p, "\thdr_formats: 0x%x\n", desc->hdr_formats);
	drm_printf(p, "\tadjusted_fps: %d\n", exynos_conn_state->adjusted_fps);
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	drm_printf(p, "\tfingerprint_mask_req: 0x%x\n", exynos_conn_state->fingerprint_mask);
#endif
}

int usdm_exynos_drm_cmdset_add(struct exynos_panel *ctx, u8 type, size_t size,
			  const u8 *data)
{
	u8 *buf;
	int index;

	if (data == NULL || size <= 0)
		return -EINVAL;

	if (ctx->cmdset_msg_total >= MAX_CMDSET_NUM ||
	    ctx->cmdset_payload_total + size >= MAX_CMDSET_PAYLOAD) {
		panel_err(ctx, "Command set buffer is full\n");
		return -EINVAL;
	}

	buf = kzalloc(sizeof(u8) * size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	memcpy(buf, data, size);
	index = ctx->cmdset_msg_total++;
	ctx->msg[index].type = type;
	ctx->msg[index].tx_buf = buf;
	ctx->msg[index].tx_len = size;
	ctx->cmdset_payload_total += size;

	panel_debug(ctx, "%d msgs, %d payload\n",
		 ctx->cmdset_msg_total, ctx->cmdset_payload_total);

	return 0;
}
EXPORT_SYMBOL(usdm_exynos_drm_cmdset_add);

int usdm_exynos_drm_cmdset_cleanup(struct exynos_panel *ctx)
{
	int i;
	int msg_total = ctx->cmdset_msg_total;

	panel_debug(ctx, "msg:%d\n", ctx->cmdset_msg_total);

	for (i = 0; i < msg_total; i++) {
		if (ctx->msg[i].tx_buf != NULL)
			kfree(ctx->msg[i].tx_buf);
		ctx->msg[i].tx_buf = NULL;
		ctx->msg[i].type = 0;
		ctx->msg[i].tx_len = 0;
		ctx->msg[i].flags = 0;
	}
	ctx->cmdset_msg_total = 0;
	ctx->cmdset_payload_total = 0;

	panel_debug(ctx, "-\n");

	return 0;
}
EXPORT_SYMBOL(usdm_exynos_drm_cmdset_cleanup);

int usdm_exynos_drm_cmdset_flush(struct exynos_panel *ctx, bool wait_vsync, bool wait_fifo)
{
	int ret;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_msg *msg_head;

	if (!ctx->cmdset_msg_total) {
		dev_err(ctx->dev, "there is no MIPI command to transfer\n");
		return -EINVAL;
	}

	msg_head = ctx->msg;
	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg_head->flags |= MIPI_DSI_MSG_USE_LPM;

	ret = usdm_dsim_host_cmdset_transfer(dsi->host, ctx->msg, ctx->cmdset_msg_total, wait_vsync, wait_fifo);
	if (ret < 0)
		panel_err(ctx, "failed to tx command set data\n");

	ret = usdm_exynos_drm_cmdset_cleanup(ctx);
	if (ret < 0)
		panel_err(ctx, "failed to cleanup command set data\n");

	return ret;
}
EXPORT_SYMBOL(usdm_exynos_drm_cmdset_flush);

static int
exynos_panel_connector_set_property(struct exynos_drm_connector *exynos_conn,
			struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t val)
{
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_drm_connector_properties *p =
		usdm_exynos_drm_connector_get_properties(&ctx->exynos_connector);

	if (property == p->fingerprint_mask)
		exynos_conn_state->fingerprint_mask = val;
#endif
	return 0;
}

static int
exynos_panel_connector_get_property(struct exynos_drm_connector *exynos_conn,
			const struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t *val)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_drm_connector_properties *p =
		usdm_exynos_drm_connector_get_properties(&ctx->exynos_connector);

	if (property == p->max_luminance)
		*val = ctx->desc->max_luminance;
	else if (property == p->max_avg_luminance)
		*val = ctx->desc->max_avg_luminance;
	else if (property == p->min_luminance)
		*val = ctx->desc->min_luminance;
	else if (property == p->hdr_formats)
		*val = ctx->desc->hdr_formats;
	else if (property == p->adjusted_fps)
		*val = exynos_conn_state->adjusted_fps;
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	else if (property == p->fingerprint_mask)
		*val = ctx->fingerprint_mask;
#endif
	else
		return -EINVAL;

	return 0;
}

static const struct exynos_drm_connector_funcs exynos_panel_connector_funcs = {
	.atomic_print_state = exynos_panel_connector_print_state,
	.atomic_set_property = exynos_panel_connector_set_property,
	.atomic_get_property = exynos_panel_connector_get_property,
};

static int exynos_drm_connector_modes(struct drm_connector *connector)
{
	struct exynos_panel *ctx = connector_to_exynos_panel(connector);
	int ret;

	ret = drm_panel_get_modes(&ctx->panel, connector);
	if (ret < 0) {
		panel_err(ctx, "failed to get panel display modes\n");
		return ret;
	}

	return ret;
}

static const struct exynos_panel_mode *
exynos_panel_get_mode(struct exynos_panel *ctx,
			const struct drm_display_mode *mode)
{
	int i;
	const struct exynos_panel_mode *pmode;

	for (i = 0; i < (int)ctx->desc->num_modes; i++) {
		pmode = &ctx->desc->modes[i];

		if (drm_mode_equal(&pmode->mode, mode))
			return pmode;
	}

	for (i = 0; i < (int)ctx->desc->num_lp_modes; i++) {
		pmode = &ctx->desc->lp_modes[i];

		if (pmode && drm_mode_equal(&pmode->mode, mode))
			return pmode;
	}

	panel_err(ctx, "fail to get panel mode matching w/ mode(%s)\n",
			mode->name);

	return NULL;
}

static void exynos_drm_connector_check_seamless_modeset(
			struct exynos_drm_connector_state *exynos_conn_state,
			const struct exynos_panel_mode *new_pmode,
			const struct exynos_panel_mode *old_pmode)
{
	const struct drm_display_mode *new_mode;
	const struct drm_display_mode *old_mode;

	if (!old_pmode)
		return;

	new_mode = &new_pmode->mode;
	old_mode = &old_pmode->mode;

	if (!drm_mode_match(new_mode, old_mode, DRM_MODE_MATCH_TIMINGS))
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_MRES;
	if (drm_mode_vrefresh(new_mode) != drm_mode_vrefresh(old_mode))
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_VREF;
	if (new_pmode->exynos_mode.is_lp_mode != old_pmode->exynos_mode.is_lp_mode)
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_LP;
}

static inline bool is_seamless_mode_change(struct drm_crtc_state *crtc_state,
					const struct exynos_panel_mode *pmode)
{
	return (crtc_state->active && !crtc_state->active_changed);
}

static int exynos_drm_connector_atomic_check(struct drm_connector *connector,
				      struct drm_atomic_state *state)
{
	struct drm_connector_state *connector_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(connector_state);
	struct drm_crtc_state *new_crtc_state, *old_crtc_state;
	struct exynos_panel *ctx = connector_to_exynos_panel(connector);
	const struct exynos_panel_mode *pmode;
	const struct exynos_panel_mode *old_pmode;

	if (!connector_state->best_encoder) {
		panel_err(ctx, "encoder is null\n");
		return 0;
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(state, connector_state->crtc);
	new_crtc_state = drm_atomic_get_new_crtc_state(state, connector_state->crtc);

	pmode = exynos_panel_get_mode(ctx, &new_crtc_state->mode);
	if (!pmode) {
		panel_err(ctx, "%s can't support none panel mode\n",
				new_crtc_state->mode.name);
		return -EINVAL;
	}

	old_pmode = exynos_panel_get_mode(ctx, &old_crtc_state->mode);
	if (old_pmode) {
		if ((old_pmode->exynos_mode.is_lp_mode && pmode->exynos_mode.is_lp_mode) &&
			((old_crtc_state->active == 1 && new_crtc_state->active == 0) ||	/* doze->doze_suspend */
			(old_crtc_state->active == 0 && new_crtc_state->active == 1)))		/* doze_suspend->doze */
			exynos_conn_state->is_lp_transition = 1;

		panel_debug(ctx, "old lp_mode(%d), old crtc active(%d)\n",
			old_pmode->exynos_mode.is_lp_mode, old_crtc_state->active);
		panel_debug(ctx, "new lp_mode(%d), new crtc active(%d)\n",
			pmode->exynos_mode.is_lp_mode, new_crtc_state->active);
		panel_debug(ctx, "is_lp_transition(%d)\n",
			exynos_conn_state->is_lp_transition);
	}

	/* display state : doze_suspend */
	if (pmode->exynos_mode.is_lp_mode && !new_crtc_state->active) {
		down_write(&ctx->panel_drm_state_lock);
		ctx->panel_drm_state = PANEL_DRM_STATE_LPM_DISABLED;
		up_write(&ctx->panel_drm_state_lock);
	}

	if (!new_crtc_state->mode_changed)
		return 0;

	exynos_conn_state->exynos_mode = pmode->exynos_mode;
	if (!exynos_conn_state->exynos_mode.bts_fps)
		exynos_conn_state->exynos_mode.bts_fps =
				exynos_panel_get_bts_fps(ctx, pmode);

	if (is_seamless_mode_change(new_crtc_state, pmode)) {
		exynos_drm_connector_check_seamless_modeset(
				exynos_conn_state, pmode, old_pmode);
	}

	panel_debug(ctx, "old mode_changed(%d), active_changed(%d)\n",
			old_crtc_state->mode_changed,
			old_crtc_state->active_changed);
	panel_debug(ctx, "new mode_changed(%d), active_changed(%d)\n",
			new_crtc_state->mode_changed,
			new_crtc_state->active_changed);

	return 0;
}

static const struct drm_connector_helper_funcs exynos_connector_helper_funcs = {
	.atomic_check = exynos_drm_connector_atomic_check,
	.get_modes = exynos_drm_connector_modes,
};

static u8 exynos_drm_connector_edid_get_checksum(const u8 *raw_edid)
{
	int i;
	u8 csum = 0;

	for (i = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];

	return csum;
}

static void exynos_drm_connector_edid(struct drm_connector *connector)
{
	int ret;
	struct edid *edid = &panel_edid;
	const u8 edid_header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
	const u8 edid_display_name[] = {0x73, 0x61, 0x6d, 0x73, 0x75, 0x6e, 0x67, 0x20, 0x6c, 0x63, 0x64, 0x20, 0x20};

	memset(edid, 0, sizeof(struct edid));
	memcpy(edid, edid_header, sizeof(edid_header));

	/*
	 * If you want to manipulate EDID information, use member variables
	 * of edid structure in here.
	 *
	 * ex) edid.width_cm = xx; edid.height_cm = yy
	 */
	edid->mfg_id[0] = 0x4C;    // manufacturer ID for samsung
	edid->mfg_id[1] = 0x2D;
	edid->mfg_week = 0x10;	/* 16 week */
	edid->mfg_year = 0x1E;	/* 1990 + 30 = 2020 year */
	edid->detailed_timings[0].data.other_data.type = 0xfc;  // for display name
	memcpy(edid->detailed_timings[0].data.other_data.data.str.str, edid_display_name, 13);
	/* sum of all 128 bytes should equal 0 (mod 0x100) */
	edid->checksum = 0x100 - exynos_drm_connector_edid_get_checksum((const u8 *)edid);

	pr_info("%s: checksum(0x%x)\n", __func__,
			exynos_drm_connector_edid_get_checksum((const u8 *)edid));

	connector->override_edid = false;
	ret = drm_connector_update_edid_property(connector, edid);
}

static int exynos_panel_attach_lp_mode(struct exynos_drm_connector *exynos_conn,
				       const struct exynos_panel_desc *desc)
{
	struct exynos_drm_connector_properties *p =
		usdm_exynos_drm_connector_get_properties(exynos_conn);
	struct drm_mode_modeinfo *umodes;
	struct drm_property_blob *blob;
	const struct exynos_panel_mode *lp_modes = desc->lp_modes;
	const size_t num_lp_modes = desc->num_lp_modes;
	int ret = 0, i = 0;

	if (!lp_modes) {
		ret = -ENOENT;
		goto err;
	}

	umodes = kzalloc(num_lp_modes * sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!umodes) {
		ret = -ENOMEM;
		goto err;
	}
	for (i = 0; i < (int)num_lp_modes; i++)
		drm_mode_convert_to_umode(&umodes[i], &lp_modes[i].mode);

	blob = drm_property_create_blob(exynos_conn->base.dev,
			num_lp_modes * sizeof(struct drm_mode_modeinfo), umodes);
	if (IS_ERR(blob)) {
		ret = PTR_ERR(blob);
		goto err_blob;
	}

	drm_object_attach_property(&exynos_conn->base.base, p->lp_mode, blob->base.id);

err_blob:
	kfree(umodes);
err:
	return ret;
}

static int exynos_panel_attach_properties(struct exynos_panel *ctx)
{
	const struct exynos_drm_connector_properties *p =
		usdm_exynos_drm_connector_get_properties(&ctx->exynos_connector);
	struct drm_mode_object *obj = &ctx->exynos_connector.base.base;
	const struct exynos_panel_desc *desc = ctx->desc;
	int ret = 0;

	if (!p || !desc)
		return -ENOENT;

	drm_object_attach_property(obj, p->min_luminance, 2);
	drm_object_attach_property(obj, p->max_luminance, 500);
	drm_object_attach_property(obj, p->max_avg_luminance, 0);
	drm_object_attach_property(obj, p->hdr_formats, 0);
	drm_object_attach_property(obj, p->adjusted_fps, 0);
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	drm_object_attach_property(obj, p->fingerprint_mask, 0);
#endif

	if (IS_ENABLED(CONFIG_DRM_SAMSUNG_DOZE) && sec_doze) {
		ret = exynos_panel_attach_lp_mode(&ctx->exynos_connector, desc);
		if (ret)
			panel_err(ctx, "Failed to attach lp mode (%d)\n", ret);
	}

	return ret;
}

static int exynos_panel_bridge_attach(struct drm_bridge *bridge,
					enum drm_bridge_attach_flags flags)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct drm_connector *connector = &ctx->exynos_connector.base;
	int ret;

	ret = usdm_exynos_drm_connector_init(bridge->dev, &ctx->exynos_connector,
				 &exynos_panel_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		panel_err(ctx, "failed to initialize connector with drm\n");
		return ret;
	}

	ret = exynos_panel_attach_properties(ctx);
	if (ret) {
		panel_err(ctx, "failed to attach connector properties\n");
		return ret;
	}

	drm_connector_helper_add(connector, &exynos_connector_helper_funcs);

	ret = drm_connector_register(connector);
	if (ret)
		goto err;

	drm_connector_attach_encoder(connector, bridge->encoder);
	connector->funcs->reset(connector);
	connector->status = connector_status_connected;

	drm_kms_helper_hotplug_event(connector->dev);

	exynos_drm_connector_edid(connector);

	return 0;

err:
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	return ret;
}

static void exynos_panel_bridge_detach(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);

	drm_connector_unregister(&ctx->exynos_connector.base);
	drm_connector_cleanup(&ctx->exynos_connector.base);
}

static void exynos_panel_enable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);
	const struct drm_display_mode *current_mode = &ctx->current_mode->mode;

	if (exynos_conn_state->is_lp_transition && ctx->enabled) {
		panel_info(ctx, "skip in lp mode transition\n");
		return;
	}

	if (is_panel_tui(ctx) || need_panel_recovery(ctx)) {
		panel_info(ctx, "tui transition : skip\n");
		return;
	}

	if (ctx->enabled) {
		panel_info(ctx, "panel is already initialized\n");
		return;
	}

	if (ctx->desc->lp11_reset) {
		panel_info(ctx, "%s lp11_reset:%d\n", __func__, ctx->desc->lp11_reset);

		/* 1. RST HIGH in LP11 */
		call_mcd_panel_func(ctx->mcd_panel_dev, reset_lp11);

		/* 2. LP11 -> HS CLK (dsim) */
		usdm_dsim_atomic_activate(bridge->encoder);
	}

	if (drm_panel_enable(&ctx->panel))
		return;

	exynos_conn_state->adjusted_fps = drm_mode_vrefresh(current_mode);
}

static void exynos_panel_pre_enable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);

	if (exynos_conn_state->is_lp_transition && ctx->enabled) {
		panel_info(ctx, "skip in lp mode transition\n");
		/* display state : doze */
		down_write(&ctx->panel_drm_state_lock);
		ctx->panel_drm_state = PANEL_DRM_STATE_LPM_ENABLED;
		up_write(&ctx->panel_drm_state_lock);
		return;
	}

	if (ctx->enabled) {
		panel_info(ctx, "panel is already initialized\n");
		return;
	}

	if (is_panel_tui(ctx) || need_panel_recovery(ctx)) {
		panel_info(ctx, "tui transition : skip\n");
		return;
	}

	drm_panel_prepare(&ctx->panel);

	down_write(&ctx->panel_drm_state_lock);
	ctx->panel_drm_state = PANEL_DRM_STATE_ENABLED;
	up_write(&ctx->panel_drm_state_lock);
}

static void exynos_panel_disable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);

	if (exynos_conn_state->is_lp_transition) {
		panel_info(ctx, "skip in lp mode transition\n");
		return;
	}

	if (is_panel_tui(ctx) || need_panel_recovery(ctx)) {
		panel_info(ctx, "tui transition : skip\n");
		return;
	}

	drm_panel_disable(&ctx->panel);
}

static void exynos_panel_post_disable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);

	if (exynos_conn_state->is_lp_transition) {
		panel_info(ctx, "skip in lp mode transition\n");
		return;
	}

	if (is_panel_tui(ctx) || need_panel_recovery(ctx)) {
		panel_info(ctx, "tui transition : skip\n");
		return;
	}

	down_write(&ctx->panel_drm_state_lock);
	ctx->panel_drm_state = PANEL_DRM_STATE_DISABLED;
	up_write(&ctx->panel_drm_state_lock);

	drm_panel_unprepare(&ctx->panel);
}

static void exynos_panel_bridge_mode_set(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  const struct drm_display_mode *adjusted_mode)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	const struct exynos_panel_funcs *funcs = ctx->desc->exynos_panel_func;
	const struct exynos_panel_mode *curr_pmode = ctx->current_mode;
	const struct exynos_panel_mode *pmode =
				exynos_panel_get_mode(ctx, adjusted_mode);

	panel_debug(ctx, "+\n");

	if (WARN_ON(!pmode))
		return;

	dsi->mode_flags = pmode->exynos_mode.mode_flags;

	if (funcs) {
		const bool is_lp_mode = pmode->exynos_mode.is_lp_mode;
		const struct drm_connector *conn = &ctx->exynos_connector.base;
		struct exynos_drm_connector_state *exynos_state =
			to_exynos_connector_state(conn->state);

		if (is_lp_mode && funcs->set_lp_mode)
			funcs->set_lp_mode(ctx, pmode);
		else if (funcs->mode_set) {

			if (!curr_pmode)
				exynos_state->seamless_modeset |= SEAMLESS_MODESET_VREF;

			funcs->mode_set(ctx, pmode, exynos_state->seamless_modeset);
		}

		if (SEAMLESS_MODESET_VREF & exynos_state->seamless_modeset &&
				ctx->enabled)
			exynos_state->adjusted_fps = drm_mode_vrefresh(&pmode->mode);
	}

	panel_info(ctx, "change the panel(%s) display mode (%s -> %s)\n",
		ctx->enabled ? "on" : "off", curr_pmode ?
		curr_pmode->mode.name : "none", pmode->mode.name);
	ctx->current_mode = pmode;

	panel_debug(ctx, "-\n");
}

static const struct drm_bridge_funcs exynos_panel_bridge_funcs = {
	.attach = exynos_panel_bridge_attach,
	.detach = exynos_panel_bridge_detach,
	.pre_enable = exynos_panel_pre_enable,
	.enable = exynos_panel_enable,
	.disable = exynos_panel_disable,
	.post_disable = exynos_panel_post_disable,
	.mode_set = exynos_panel_bridge_mode_set,
};

static void exynos_panel_parse_vendor_pps(struct exynos_panel *ctx)
{
		struct device_node *np;
		struct drm_device *drm_dev = NULL;
		struct drm_crtc *crtc;
		struct exynos_drm_crtc *exynos_crtc = NULL;
		struct decon_device *decon;

		if (!ctx->mcd_panel_dev) {
			panel_err(ctx, "mcd_panel_dev has null\n");
			return;
		}

		drm_dev = ctx->exynos_connector.base.dev;
		if (!drm_dev) {
			panel_info(ctx, "drm_dev has null\n");
			return;
		}

		drm_for_each_crtc(crtc, drm_dev)
			if (to_exynos_crtc(crtc)->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
				exynos_crtc = to_exynos_crtc(crtc);
				break;
			}

		if (!exynos_crtc) {
			panel_info(ctx, "exynos_crtc has null\n");
			return;
		}

		decon = exynos_crtc->ctx;
		if (!decon) {
			panel_info(ctx, "decon has null\n");
			return;
		}

		np = ctx->mcd_panel_dev->ap_vendor_setting_node;
		if (!np) {
			panel_err(ctx, "mcd_panel ddi-node is null\n");
			return;
		}

		/* get vendor pps parameter from panel dt node */
		of_property_read_u32(np, "initial_xmit_delay",
				&decon->config.vendor_pps.initial_xmit_delay);
		of_property_read_u32(np, "initial_dec_delay",
				&decon->config.vendor_pps.initial_dec_delay);
		of_property_read_u32(np, "scale_increment_interval",
				&decon->config.vendor_pps.scale_increment_interval);
		of_property_read_u32(np, "final_offset",
				&decon->config.vendor_pps.final_offset);
}

static void exynos_panel_parse_vfp_detail(struct exynos_panel *ctx)
{
	struct device_node *np;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	if (!ctx->mcd_panel_dev) {
		panel_err(ctx, "mcd_panel_dev has null\n");
		return;
	}

	np = ctx->mcd_panel_dev->ap_vendor_setting_node;
	if (!np) {
		panel_err(ctx, "mcd_panel ddi-node is null\n");
		return;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	if (dsim != NULL) {
		if (of_property_read_u32(np, "lines-cmd-allow",
					 &dsim->config.line_cmd_allow))
			dsim->config.line_cmd_allow = 4;

		if (of_property_read_u32(np, "lines-stable-vfp",
					 &dsim->config.line_stable_vfp))
			dsim->config.line_stable_vfp = 2;
	} else {
		panel_err(ctx, "DSIM is not found\n");
	}
}

static int exynos_panel_get_bts_fps(const struct exynos_panel *ctx,
				     const struct exynos_panel_mode *pmode)
{
	size_t num_modes = ctx->desc->num_modes;
	int i;
	int mode_fps, max_fps = 0;

	/* @command mode */
	if (!(pmode->exynos_mode.mode_flags & MIPI_DSI_MODE_VIDEO)) {
#if !IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		return drm_mode_vrefresh(&pmode->mode);
#else
		return usdm_mcd_decon_get_bts_fps(&pmode->mode);
#endif
	}

	/*
	 * In order to frame rate in video mode.
	 * VRR can be supported through changing VFP value.
	 * video processing time is always required as max fps.
	 * so bts.fps value have to be fixed in video mode operation.
	 */
	for (i = 0; i < num_modes; i++) {
		mode_fps = drm_mode_vrefresh(&ctx->desc->modes[i].mode);
		if (max_fps < mode_fps)
			max_fps = mode_fps;
	}
	panel_info(ctx, "[Video Mode] max_fps(%d)\n", max_fps);

	return max_fps;
}

static void exynos_panel_set_dqe_xml(struct device *dev, struct exynos_panel *ctx)
{
	struct drm_device *drm_dev = NULL;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc = NULL;
	struct exynos_dqe *dqe;

	drm_dev = ctx->exynos_connector.base.dev;
	if (!drm_dev) {
		panel_info(ctx, "drm_dev has null\n");
		return;
	}

	drm_for_each_crtc(crtc, drm_dev)
		if (to_exynos_crtc(crtc)->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
			exynos_crtc = to_exynos_crtc(crtc);
			break;
		}

	if (!exynos_crtc) {
		panel_info(ctx, "exynos_crtc has null\n");
		return;
	}

	dqe = exynos_crtc->dqe;
	if (!dqe) {
		panel_info(ctx, "dqe has null\n");
		return;
	}

	if (ctx->desc->xml_suffix == NULL)
		return;
	strncpy(dqe->xml_suffix, ctx->desc->xml_suffix, DQE_XML_SUFFIX_SIZE - 1);

	panel_info(ctx, "DQE XML Suffix (%s)\n", dqe->xml_suffix);
}

int mcd_drm_panel_check_probe(struct exynos_panel *ctx)
{
	int ret = 0;

	if (!ctx || !ctx->mcd_panel_dev)
		return -ENODEV;

	mutex_lock(&ctx->probe_lock);
	if (ctx->mcd_panel_probed)
		goto out;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, probe);
	if (ret < 0) {
		panel_err(ctx, "mcd_panel probe failed(ret:%d)\n", ret);
		goto out;
	}
	ctx->mcd_panel_probed = true;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_ddi_props, &ctx->ddi_props);
	if (ret < 0) {
		panel_err(ctx, "mcd_panel get ddi props failed(ret:%d)\n", ret);
		goto out;
	}

out:
	mutex_unlock(&ctx->probe_lock);
	return ret;
}

static int mcd_drm_panel_display_on(struct exynos_panel *ctx)
{
	if (!ctx)
		return -ENODEV;

	queue_delayed_work(ctx->wqs[MCD_DRM_DRV_WQ_DISPON].wq,
			&ctx->wqs[MCD_DRM_DRV_WQ_DISPON].dwork, msecs_to_jiffies(0));
	panel_info(ctx, "MCD_DRM_DRV_WQ_DISPON queued\n");

	return 0;
}

static int mcd_drm_panel_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct exynos_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	panel_info(ctx, "+\n");
	ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_in);
	if (ret < 0) {
		panel_err(ctx, "failed to sleep in(ret:%d)\n", ret);
		return ret;
	}
	ctx->enabled = false;
	panel_info(ctx, "-\n");

	return 0;
}

static int mcd_drm_panel_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx;
	const struct exynos_panel_mode *pmode;
	const struct exynos_panel_funcs *funcs;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct exynos_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	pmode = ctx->current_mode;
	if (!pmode) {
		dev_err(ctx->dev, "no current mode set\n");
		return -EINVAL;
	}

	ret = mcd_drm_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	if (!pmode->exynos_mode.is_lp_mode) {
		ret = mcd_drm_panel_set_display_mode(ctx, pmode);
		if (ret < 0)
			panel_err(ctx, "failed to set display mode(%s)\n",
					pmode->mode.name);
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_out);
	if (ret < 0) {
		panel_err(ctx, "failed to sleep out(ret:%d)\n", ret);
		return ret;
	}
	ctx->enabled = true;

	funcs = ctx->desc->exynos_panel_func;
	if (pmode->exynos_mode.is_lp_mode && funcs->set_lp_mode)
		funcs->set_lp_mode(ctx, pmode);

	ret = mcd_drm_panel_display_on(ctx);
	if (ret < 0)
		panel_err(ctx, "failed to display on(ret:%d)\n", ret);

	return 0;
}

static int mcd_drm_panel_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct exynos_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	panel_info(ctx, "+\n");
	ret = call_mcd_panel_func(ctx->mcd_panel_dev, power_off);
	if (ret < 0) {
		panel_err(ctx, "failed to power off(ret:%d)\n", ret);
		return ret;
	}
	panel_info(ctx, "-\n");

	return 0;
}

static int mcd_drm_panel_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx;
	int ret;

	if (!panel) {
		pr_err("%s: invalid drm_panel\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(panel, struct exynos_panel, panel);
	if (!ctx->mcd_panel_dev)
		return -ENODEV;

	ret = mcd_drm_panel_check_probe(ctx);
	if (ret < 0)
		return ret;

	panel_info(ctx, "+\n");
	ret = call_mcd_panel_func(ctx->mcd_panel_dev, power_on);
	if (ret < 0) {
		panel_err(ctx, "failed to power on(ret:%d)\n", ret);
		return ret;
	}
	panel_info(ctx, "-\n");

	return 0;
}

static int mcd_drm_panel_set_display_mode(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	const struct drm_display_mode *mode;
	struct panel_display_modes *pdms;
	struct panel_display_mode *pdm;
	int ret;

	if (!ctx || !pmode)
		return -EINVAL;

	mode = &pmode->mode;

	panel_debug(ctx, "+\n");
	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_display_mode, &pdms);
	if (ret < 0) {
		panel_err(ctx, "failed to get display mode(ret:%d)\n", ret);
		return ret;
	}

	pdm = usdm_exynos_panel_find_panel_mode(pdms, mode);
	if (pdm == NULL) {
		panel_err(ctx, "panel_mode(%dx%d@%d) is not found\n",
				mode->hdisplay, mode->vdisplay,
				drm_mode_vrefresh(mode));
		return -EINVAL;
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, set_display_mode, pdm);
	if (ret < 0) {
		panel_err(ctx, "failed to set display mode(ret:%d)\n", ret);
		return ret;
	}

	panel_info(ctx, "drm-mode:%s, panel-mode:%s\n",
			mode->name, pdm->name);

	panel_debug(ctx, "-\n");

	return 0;
}

static void mcd_drm_panel_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	int ret;

	panel_info(ctx, "+\n");

	if (!ctx->enabled)
		return;

	if (ctx->current_mode->exynos_mode.is_lp_mode) {
		panel_info(ctx, "exit lp mode\n");
		ret = call_mcd_panel_func(ctx->mcd_panel_dev, sleep_out);
		if (ret < 0)
			panel_err(ctx, "failed to sleep_out(ret:%d)\n", ret);
	}

	if (SEAMLESS_MODESET_MRES & flags || SEAMLESS_MODESET_VREF & flags) {
		ret = mcd_drm_panel_set_display_mode(ctx, pmode);
		if (ret < 0)
			panel_err(ctx, "failed to set vrefresh\n");
	}

	if (ctx->current_mode->exynos_mode.is_lp_mode) {
		ret = mcd_drm_panel_display_on(ctx);
		if (ret < 0)
			panel_err(ctx, "failed to display_on(ret:%d)\n", ret);
	}

	panel_info(ctx, "-\n");
}

static void mcd_drm_request_set_clock(struct exynos_panel *ctx, void *arg)
{
	int ret;

	if (!ctx || !arg) {
		pr_err("%s invalid param\n", __func__);
		return;
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, req_set_clock, arg);
	if (ret < 0)
		panel_err(ctx, "mcd_panel req_set_clock failed(ret:%d)\n", ret);
}

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
static int mcd_drm_panel_set_fingermask_layer(struct exynos_panel *ctx, u32 after)
{
	struct mask_layer_data data;
	const struct drm_connector *conn = &ctx->exynos_connector.base;
	struct exynos_drm_connector_state *exynos_state =
				to_exynos_connector_state(conn->state);

	int ret = 0;

	if (ctx->fingerprint_mask == exynos_state->fingerprint_mask)
		return 0;

	data.trigger_time = after;
	data.req_mask_layer = exynos_state->fingerprint_mask;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, set_mask_layer, &data);

	if (after)
		ctx->fingerprint_mask = exynos_state->fingerprint_mask;

	if (ret < 0) {
		panel_err(ctx, "mcd_panel set_mask_layer failed(ret:%d)\n", ret);
		return -EINVAL;
	}

	panel_info(ctx, "(%s)(%s)\n",
			data.trigger_time ? "after" : "before",
			data.req_mask_layer ? "enable" : "disable");

	return ret;
}
#endif

static int _mcd_drm_mipi_write_exec(struct mipi_dsi_device *dsi, const u8 *buf, int len)
{
	int ret;

	if (!dsi) {
		pr_err("%s: invalid dsi device\n", __func__);
		return -EINVAL;
	}

	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	ret = mipi_dsi_dcs_write_buffer(dsi, buf, len);
	if (ret < 0) {
		dev_err(&dsi->dev, "%s failed to write %d\n", __func__, ret);
		return ret;
	}

	/* w/a for mipi_write returned 0 when success, request fix to lsi */
	ret = len;

	if (ret != len)
		dev_warn(&dsi->dev, "%s req %d, written %d bytes\n", __func__, len, ret);

	return len;
}

static int _mcd_drm_mipi_read_exec(struct mipi_dsi_device *dsi, u8 cmd, u8 *buf, int len)
{
	int ret;

	if (!dsi) {
		pr_err("%s: invalid dsi device\n", __func__);
		return -EINVAL;
	}

	if (!dsi->host) {
		dev_err(&dsi->dev, "%s: invalid dsi host\n", __func__);
		return -EINVAL;
	}

	ret = mipi_dsi_dcs_read(dsi, cmd, buf, len);
	if (ret < 0) {
		dev_err(&dsi->dev, "%s failed to write %d\n", __func__, ret);
		return ret;
	}
	if (ret != len)
		dev_warn(&dsi->dev, "%s req %d, written %d bytes\n", __func__, len, ret);

	return ret;
}

static void print_tx(u8 cmd_id, const u8 *cmd, int size)
{
	char data[256];
	int i, len;
	bool newline = false;

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", cmd_id);
	for (i = 0; i < min_t(int, size, 256); i++) {
		if (newline)
			len += snprintf(data + len, ARRAY_SIZE(data) - len, "     ");
		newline = (!((i + 1) % 16) || (i + 1 == size)) ? true : false;
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X%s", cmd[i], newline ? "\n" : " ");
		if (newline) {
			pr_info("%s: %s", __func__, data);
			len = 0;
		}
	}
}

static void print_dsim_cmd(const struct cmd_set *cmd_set, int size)
{
	int i;

	for (i = 0; i < size; i++)
		print_tx(cmd_set[i].cmd_id,
				cmd_set[i].buf,
				cmd_set[i].size);
}

int mcd_drm_mipi_write(void *_ctx, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	u8 gpara[4] = { 0xB0, 0x00, };
	int gpara_len = 1;
	int ret;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!cmd) {
		panel_err(ctx, "invalid cmd data\n");
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	if (offset) {
		if (option & DSIM_OPTION_2BYTE_GPARA)
			gpara[gpara_len++] = (offset >> 8) & 0xFF;

		gpara[gpara_len++] = offset & 0xFF;
		if (option & DSIM_OPTION_POINT_GPARA)
			gpara[gpara_len++] = cmd[0];
		if (panel_cmd_log)
			print_tx(MIPI_DSI_DCS_LONG_WRITE, gpara, gpara_len);

		ret = _mcd_drm_mipi_write_exec(dsi, gpara, gpara_len);
		if (ret < 0)
			return ret;
	}

	if (cmd_id == MIPI_DSI_WR_PPS_CMD) {
		ret = mipi_dsi_picture_parameter_set(dsi, (struct drm_dsc_picture_parameter_set *)cmd);

		if (panel_cmd_log)
			print_tx(MIPI_DSI_PICTURE_PARAMETER_SET, cmd, size);
		if (ret == 0)
			ret = size;
	} else if (cmd_id == MIPI_DSI_WR_DSC_CMD) {
		ret = mipi_dsi_compression_mode(dsi, (*cmd) ? true : false);
		if (panel_cmd_log)
			print_tx(MIPI_DSI_COMPRESSION_MODE, cmd, size);

		if (ret == 0)
			ret = size;
	} else {
		ret = _mcd_drm_mipi_write_exec(dsi, cmd, size);
		if (panel_cmd_log)
			print_tx(MIPI_DSI_DCS_LONG_WRITE, cmd, size);
	}

	return ret;
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define FCMD_DATA_MAX_SIZE	0x00100000
struct dsim_fcmd *create_dsim_fast_cmd(void *vaddr, const u8 *payload, u32 size, u32 align)
{
	struct dsim_fcmd *fcmd;
	struct mipi_dsi_msg *msg;
	u32 padd_u = 0, padd_c, padd_t;
	int xfer_cnt, xfer_sz, xfer_unit, xfer_unit_aligned;
	u32 payload_xfer_unit;
	u32 remaineder = size;
	u32 size_aligned;
	u8 *tx_buf = (u8 *)vaddr;
	int i;

	if (!vaddr || !payload)
		return ERR_PTR(-EINVAL);

	if (size == 0 || size > FCMD_DATA_MAX_SIZE)
		return ERR_PTR(-EINVAL);

	fcmd = kzalloc(sizeof(*fcmd), GFP_KERNEL);
	if (!fcmd)
		return ERR_PTR(-ENOMEM);

	/* minimum alignment size is 1 */
	align = max(1U, align);
	size_aligned = roundup(size, align);

	payload_xfer_unit = rounddown(min((u32)(DSIM_PL_FIFO_THRESHOLD - 1), size_aligned), align);
	xfer_cnt = roundup(size_aligned, payload_xfer_unit) / payload_xfer_unit;
	xfer_unit = payload_xfer_unit + 1;
	xfer_unit_aligned = roundup(xfer_unit, DSIM_FCMD_ALIGN_CONSTRAINT);

	/* add address(e.g. 4C ,5C..) payload count */
	xfer_sz = size_aligned + xfer_cnt;

	/* padding is necessary if next xfer payload exists. so add (xfer_cnt - 1) times */
	if (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		padd_u = DSIM_FCMD_ALIGN_CONSTRAINT - (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT);
		padd_c = xfer_cnt - 1;
		padd_t = padd_u * padd_c;
		xfer_sz += padd_t;
	}

	for (i = 0; i < xfer_cnt; i++) {
		int copy_size = min(remaineder, payload_xfer_unit);

		tx_buf[i * xfer_unit_aligned] = (i == 0) ? 0x4C : 0x5C;
		memcpy(&tx_buf[i * xfer_unit_aligned + 1],
				&payload[i * payload_xfer_unit], copy_size);
		remaineder -= copy_size;
	}
	fcmd->xfer_unit = xfer_unit;
	msg = &fcmd->msg;
	msg->type = MIPI_DSI_DCS_LONG_WRITE;
	msg->tx_buf = (const u8 *)tx_buf;
	msg->tx_len = xfer_sz;

	return fcmd;
}

void destroy_dsim_fast_cmd(struct dsim_fcmd *fcmd)
{
	kfree(fcmd);
}

int usdm_drm_mipi_fcmd_write(void *_ctx, const u8 *payload, int size, u32 align)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct dsim_fcmd *fcmd;
	int ret = 0;
	static DEFINE_MUTEX(lock);

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!payload) {
		panel_err(ctx, "invalid payload\n");
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	if (!dsim->fcmd_buf_allocated) {
		panel_err(ctx, "fcmd_buf not allocated\n");
		return -ENOMEM;
	}

	mutex_lock(&lock);
	fcmd = create_dsim_fast_cmd(dsim->fcmd_buf_vaddr, payload, size, align);
	if (IS_ERR(fcmd)) {
		panel_err(ctx, "failed to create fast-command\n");
		ret = PTR_ERR(fcmd);
		goto out;
	}

	ret = usdm_dsim_host_fcmd_transfer(dsi->host, &fcmd->msg);
	if (ret < 0)
		panel_err(ctx, "failed to transfer fast-command\n");

out:
	mutex_unlock(&lock);
	destroy_dsim_fast_cmd(fcmd);

	return ret;
}
EXPORT_SYMBOL(usdm_drm_mipi_fcmd_write);

static inline int mcd_drm_mipi_get_spsram_constrain_align_size(u32 option)
{
	return (option & PKT_OPTION_SR_ALIGN_12) ? 12 : 16;
}

int mcd_drm_mipi_sr_write(void *_ctx, u8 cmd_id, const u8 *payload, u32 offset, int size, u32 option)
{
	return usdm_drm_mipi_fcmd_write((struct exynos_panel *)_ctx, payload, size,
			mcd_drm_mipi_get_spsram_constrain_align_size(option));
}
#endif

int mcd_drm_mipi_read(void *_ctx, u8 addr, u32 offset, u8 *buf, int size, u32 option)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	u8 gpara[4] = { 0xB0, 0x00, };
	int gpara_len = 1;
	int ret;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!buf) {
		panel_err(ctx, "invalid read buffer\n");
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	if (offset) {
		if (option & DSIM_OPTION_2BYTE_GPARA)
			gpara[gpara_len++] = (offset >> 8) & 0xFF;

		gpara[gpara_len++] = offset & 0xFF;
		if (option & DSIM_OPTION_POINT_GPARA)
			gpara[gpara_len++] = addr;

		ret = _mcd_drm_mipi_write_exec(dsi, gpara, gpara_len);
		if (ret < 0)
			return ret;
	}

	ret = _mcd_drm_mipi_read_exec(dsi, addr, buf, size);
	return ret;
}

#define DSIM_TX_FLOW_CONTROL
#define MAX_DSIM_PL_SIZE (DSIM_PL_FIFO_THRESHOLD)
#define MAX_CMD_SET_SIZE (MAX_PANEL_CMD_QUEUE)

static int mipi_write_table(void *_ctx, const struct cmd_set *cmd, int size, u32 option)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	int ret, total_size = 0;
	int i, sz_pl = 0;
#if defined(DSIM_TX_FLOW_CONTROL)
	int from = 0;
#endif
	s64 elapsed_usec;
	struct timespec64 cur_ts, last_ts, delta_ts;

	/* Todo: get flag from panel-drv */
	bool wait_tx_done = (option & DSIM_OPTION_WAIT_TX_DONE);
//	bool wait_tx_done = false;
	bool wait_vsync = false;

	if (!cmd) {
		pr_err("%s: cmd is null\n", __func__);
		return -EINVAL;
	}

	if (size <= 0) {
		panel_err(ctx, "invalid cmd size %d\n", size);
		return -EINVAL;
	}

	if (size > MAX_CMD_SET_SIZE) {
		panel_err(ctx, "exceeded MAX_CMD_SET_SIZE(%d) (size:%d)\n",
				MAX_CMD_SET_SIZE, size);
		return -EINVAL;
	}

	/* 1. Cleanup Buffer */
	usdm_exynos_drm_cmdset_cleanup(ctx);

	ktime_get_ts64(&last_ts);

	/* 2. Queue command set Buffer */
	for (i = 0; i < size; i++) {
		if (cmd[i].buf == NULL) {
			panel_err(ctx, "cmd[%d].buf is null\n", i);
			continue;
		}

#if defined(DSIM_TX_FLOW_CONTROL)
		/* If FIFO has no space to stack cmd, then flush first */
		if ((i - from >= (MAX_CMDSET_NUM - 1)) ||
			(sz_pl + ALIGN(cmd[i].size, 4) >= MAX_CMDSET_PAYLOAD)) {

			/* 3. Flush Command Set Buffer */
			if (usdm_exynos_drm_cmdset_flush(ctx, wait_vsync, wait_tx_done)) {
				panel_err(ctx, "failed to usdm_exynos_drm_cmdset_flush\n");
				ret = -EIO;
				goto error;
			}

			if (panel_cmd_log)
				print_dsim_cmd(&cmd[from], i - from);

			panel_debug(ctx, "cmd_set:%d pl:%d\n", i - from, sz_pl);

			from = i;
			sz_pl = 0;
		}
#endif

		if (cmd[i].cmd_id == MIPI_DSI_WR_DSC_CMD) {
			ret = usdm_exynos_drm_cmdset_add(ctx, MIPI_DSI_COMPRESSION_MODE, cmd[i].size, cmd[i].buf);
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_PPS_CMD) {
			ret = usdm_exynos_drm_cmdset_add(ctx, MIPI_DSI_PICTURE_PARAMETER_SET, cmd[i].size, cmd[i].buf);
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_GEN_CMD) {
			if (cmd[i].size == 1)
				ret = usdm_exynos_drm_cmdset_add(ctx, MIPI_DSI_DCS_SHORT_WRITE, cmd[i].size, cmd[i].buf);
			else
				ret = usdm_exynos_drm_cmdset_add(ctx, MIPI_DSI_DCS_LONG_WRITE, cmd[i].size, cmd[i].buf);
		} else {
			panel_err(ctx, "invalid cmd_id %d\n", cmd[i].cmd_id);
			ret = -EINVAL;
			goto error;
		}

		if (ret) {
			panel_err(ctx, "failed to usdm_exynos_drm_cmdset_add\n");
			goto error;
		}

		sz_pl += ALIGN(cmd[i].size, 4);
		total_size += cmd[i].size;
	}

	/* 3. Flush Command Set Buffer */
	if (usdm_exynos_drm_cmdset_flush(ctx, wait_vsync, wait_tx_done)) {
		panel_err(ctx, "failed to usdm_exynos_drm_cmdset_flush\n");
		ret = -EIO;
		goto error;
	}

	if (panel_cmd_log)
		print_dsim_cmd(&cmd[from], i - from);

	ktime_get_ts64(&cur_ts);
	delta_ts = timespec64_sub(cur_ts, last_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	panel_debug(ctx, "done (cmd_set:%d size:%d elapsed %2lld.%03lld msec)\n",
			size, total_size,
			elapsed_usec / 1000, elapsed_usec % 1000);

	ret = total_size;

error:

	return ret;
}


int mcd_drm_mipi_get_state(void *_ctx)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	return (dsim->state == DSIM_STATE_SUSPEND && !dsim->lp_mode_state) ?
		CTRL_INTERFACE_STATE_INACTIVE : CTRL_INTERFACE_STATE_ACTIVE;
}

static int parse_hdr_info(struct device_node *node, struct panel_hdr_info *hdr)
{
	int i;
	int ret;
	u32 hdr_num = 0;
	u32 hdr_type[MAX_HDR_TYPE] = {0, };
	u32 formats = 0;

	if (!node) {
		pr_err("%s node is null\n", __func__);
		return -EINVAL;
	}

	if (!hdr) {
		pr_err("%s hdr is null\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "hdr_num", &hdr_num);
	if ((ret) || (hdr_num == 0)) {
		pr_info("%s : Not support hdr foramt(ret:%d, hdr_num: %d)\n", __func__, ret, hdr_num);
		return 0;
	}
	if (hdr_num > MAX_HDR_TYPE) {
		pr_err("%s: exceed supporting hdr type: %d\n", __func__, hdr_num);
		hdr_num = MAX_HDR_TYPE;
	}

	ret = of_property_read_u32_array(node, "hdr_type", hdr_type, hdr_num);
	if (ret) {
		pr_err("%s: wrong hdr_type count(%d:%d)\n", __func__, hdr_num, ret);
		return -EINVAL;
	}

	for (i = 0; i < hdr_num; i++)
		formats |= (1 << hdr_type[i]);

	hdr->formats = formats;
	of_property_read_u32(node, "hdr_max_luma", &hdr->max_luma);
	of_property_read_u32(node, "hdr_max_avg_luma", &hdr->max_avg_luma);
	of_property_read_u32(node, "hdr_min_luma", &hdr->min_luma);

	pr_info("support hdr info: num: %d, formats: %x, luma max: %d, avg: %d, min:%d\n",
		hdr_num, formats, hdr->max_luma, hdr->max_avg_luma, hdr->min_luma);

	return 0;
}

static int parse_panel_info(struct exynos_panel *ctx, struct device_node *np)
{
	int ret;
	struct panel_device *panel;

	if (!ctx) {
		pr_err("%s ctx is null\n", __func__);
		return -EINVAL;
	}

	panel = ctx->mcd_panel_dev;
	if (!panel) {
		panel_err(ctx, "panel is null\n");
		return -EINVAL;
	}

	ret = parse_hdr_info(np, &panel->hdr);
	if (ret)
		panel_err(ctx, "failed to parse hdr info\n");

	return 0;
}

int mcd_drm_parse_dt(void *_ctx, struct device_node *np)
{
	int ret;
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!np) {
		panel_err(ctx, "no device tree\n");
		return -EINVAL;
	}

	panel_info(ctx, "+\n");
	dsi = to_mipi_dsi_device(ctx->dev);
	ret = parse_panel_info(ctx, np);
	if (ret)
		panel_err(ctx, "failed to parse panel info\n");
	panel_info(ctx, "-\n");

	return 0;
}

void wq_vblank_handler(struct work_struct *data)
{
	struct mcd_drm_drv_wq *w;
	struct exynos_panel *ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct drm_crtc *crtc;
	int ret = 0;

	if (!data) {
		pr_err("%s: invalid work_struct\n", __func__);
		return;
	}

	w = container_of(to_delayed_work(data), struct mcd_drm_drv_wq, dwork);
	ctx = wq_to_exynos_panel(w);

	panel_debug(ctx, "+\n");
	down_read(&ctx->panel_drm_state_lock);
	if (ctx->panel_drm_state == PANEL_DRM_STATE_DISABLED) {
		panel_err(ctx, "panel drm is disabled state\n");
		ret = -EINVAL;
		goto err;
	}

	if (ctx->panel_drm_state == PANEL_DRM_STATE_LPM_DISABLED) {
		/* TODO: get vsync period from drm_display_mode */
		usleep_range(33400, 33500);
		goto out;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		ret = -EINVAL;
		goto err;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	if (!crtc) {
		panel_err(ctx, "invalid crtc\n");
		ret = -EINVAL;
		goto err;
	}

	ret = drm_crtc_vblank_get(crtc);
	if (ret < 0) {
		panel_err(ctx, "failed to get vblank\n");
		ret = -EINVAL;
		goto err;
	}
	drm_crtc_wait_one_vblank(crtc);
	drm_crtc_vblank_put(crtc);

out:
	atomic_inc(&w->count);
	wake_up_interruptible_all(&w->wait);

err:
	up_read(&ctx->panel_drm_state_lock);
	w->ret = ret;
	panel_debug(ctx, "- (ret:%d)\n", w->ret);
}

void wq_framedone_handler(struct work_struct *data)
{
	struct mcd_drm_drv_wq *w;
	struct exynos_panel *ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *crtc_ops;
	struct drm_crtc_state *crtc_state;
	int ret = 0;

	if (!data) {
		pr_err("%s: invalid work_struct\n", __func__);
		return;
	}

	w = container_of(to_delayed_work(data), struct mcd_drm_drv_wq, dwork);
	ctx = wq_to_exynos_panel(w);

	panel_debug(ctx, "+\n");
	down_read(&ctx->panel_drm_state_lock);
	if (PANEL_DRM_STATE_IS_DISABLED(ctx->panel_drm_state)) {
		panel_err(ctx, "panel drm is disabled state\n");
		ret = -EINVAL;
		goto err;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		ret = -EINVAL;
		goto err;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	if (!crtc) {
		panel_err(ctx, "invalid crtc\n");
		ret = -EINVAL;
		goto err;
	}
	exynos_crtc = to_exynos_crtc(crtc);
	crtc_state = crtc->state;

	/* The following code prevents garbage screen in the first frame
	 * when it is turned on in the connected state
	 * after the panel is turned off without connection.
	 */
	if (crtc_state->no_vblank)
		usleep_range(100000, 100100);

	crtc_ops = exynos_crtc->ops;
	if (!crtc_ops || !crtc_ops->wait_framestart) {
		panel_err(ctx, "invalid crtc_ops\n");
		ret = -EINVAL;
		goto err;
	}

	crtc_ops->wait_framestart(exynos_crtc);

	atomic_inc(&w->count);

err:
	up_read(&ctx->panel_drm_state_lock);
	w->ret = ret;
	panel_debug(ctx, "- (ret:%d)\n", w->ret);
}

void wq_framedone_handler_fsync(struct work_struct *data)
{
	struct mcd_drm_drv_wq *w = container_of(to_delayed_work(data), struct mcd_drm_drv_wq, dwork);

	wq_framedone_handler(data);
	wake_up_interruptible_all(&w->wait);
}

void wq_framedone_handler_dispon(struct work_struct *data)
{
	struct mcd_drm_drv_wq *w = container_of(to_delayed_work(data), struct mcd_drm_drv_wq, dwork);
	struct exynos_panel *ctx = container_of(w, struct exynos_panel, wqs[MCD_DRM_DRV_WQ_DISPON]);
	int ret;

	panel_info(ctx, "+\n");
	if (!ctx->ddi_props.support_avoid_sandstorm)
		wq_framedone_handler(data);

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, display_on);
	if (ret < 0)
		panel_err(ctx, "failed to display_on(ret:%d)\n", ret);
	panel_info(ctx, "-\n");
}

void mcd_panel_probe_handler(struct work_struct *data)
{
	struct mcd_drm_drv_wq *w = container_of(to_delayed_work(data), struct mcd_drm_drv_wq, dwork);
	struct exynos_panel *ctx = container_of(w, struct exynos_panel, wqs[MCD_DRM_DRV_WQ_PANEL_PROBE]);
	int ret;

	panel_info(ctx, "+\n");
	ret = mcd_drm_panel_check_probe(ctx);
	if (ret < 0)
		panel_err(ctx, "mcd-panel not probed(ret:%d)\n", ret);
	panel_info(ctx, "-\n");
}

__visible_for_testing int mcd_drm_wait_work(void *_ctx, u32 work_idx, u32 timeout)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mcd_drm_drv_wq *w;
	unsigned int count;
	int ret;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (work_idx >= MAX_MCD_DRM_DRV_WQ) {
		panel_err(ctx, "invalid work index(%d)\n", work_idx);
		return -EINVAL;
	}

	if (!timeout) {
		panel_err(ctx, "timeout must be greater than 0\n");
		return -EINVAL;
	}

	w = &ctx->wqs[work_idx];
	if (!w->wq) {
		panel_err(ctx, "invalid workqueue\n");
		return -EFAULT;
	}

	count = atomic_read(&w->count);
	panel_debug(ctx, "+ count:%u\n", count);
	if (!queue_delayed_work(w->wq, &w->dwork, msecs_to_jiffies(0))) {
		panel_err(ctx, "failed to queueing work\n");
		return -EINVAL;
	}

	ret = wait_event_interruptible_timeout(w->wait,
		count < atomic_read(&w->count), msecs_to_jiffies(timeout));
	if (ret == 0) {
		panel_warn(ctx, "timeout(ret:%d, %dms)\n", ret, timeout);
		return -ETIMEDOUT;
	}
	panel_debug(ctx, "- count:%u\n", atomic_read(&w->count));

	return 0;
}

__visible_for_testing inline int mcd_drm_wait_for_vsync(void *ctx, u32 timeout)
{
	return mcd_drm_wait_work(ctx, MCD_DRM_DRV_WQ_VSYNC, timeout);
}

__visible_for_testing inline int mcd_drm_wait_for_fsync(void *ctx, u32 timeout)
{
	return mcd_drm_wait_work(ctx, MCD_DRM_DRV_WQ_FSYNC, timeout);
}

__visible_for_testing int mcd_drm_set_bypass(void *ctx, bool on)
{
	pr_info("%s %s\n", __func__, on ? "on" : "off");
	usdm_bypass_display = (on ? 1 : 0);
	return 0;
}

__visible_for_testing int mcd_drm_set_commit_retry(void *ctx, bool on)
{
	pr_info("%s %s\n", __func__, on ? "on" : "off");
	usdm_commit_retry = (on ? 1 : 0);
	return 0;
}

__visible_for_testing int mcd_drm_get_bypass(void *ctx)
{
	return (usdm_bypass_display > 0 ? 1 : 0);
}

__visible_for_testing int mcd_drm_set_lpdt(void *_ctx, bool on)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	panel_info(ctx, "%s\n", on ? "on" : "off");

	if (on)
		dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	else
		dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	return 0;
}

__visible_for_testing int mcd_drm_dpu_register_dump(void *_ctx)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	const struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *crtc_ops;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	if (!crtc) {
		panel_err(ctx, "invalid crtc\n");
		return -EINVAL;
	}

	exynos_crtc = to_exynos_crtc(crtc);
	crtc_ops = exynos_crtc->ops;
	if (!crtc_ops) {
		panel_err(ctx, "invalid crtc_ops\n");
		return -EINVAL;
	}

	if (!crtc_ops->dump_register) {
		panel_err(ctx, "no dump_register handler\n");
		return -EINVAL;
	}

	crtc_ops->dump_register(exynos_crtc);

	return 0;
}

__visible_for_testing int mcd_drm_dpu_event_log_print(void *_ctx)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	const struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *crtc_ops;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	if (!crtc) {
		panel_err(ctx, "invalid crtc\n");
		return -EINVAL;
	}

	exynos_crtc = to_exynos_crtc(crtc);
	crtc_ops = exynos_crtc->ops;
	if (!crtc_ops) {
		panel_err(ctx, "invalid crtc_ops\n");
		return -EINVAL;
	}

	if (!crtc_ops->dump_event_log) {
		panel_err(ctx, "no dump_event_log handler\n");
		return -EINVAL;
	}

	crtc_ops->dump_event_log(exynos_crtc);

	return 0;
}

__visible_for_testing int mcd_drm_emergency_off(void *_ctx)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	const struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc;
	const struct exynos_drm_crtc_ops *crtc_ops;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	crtc = dsim->encoder.crtc;
	if (!crtc) {
		panel_err(ctx, "invalid crtc\n");
		return -EINVAL;
	}

	exynos_crtc = to_exynos_crtc(crtc);
	crtc_ops = exynos_crtc->ops;
	if (!crtc_ops) {
		panel_err(ctx, "invalid crtc_ops\n");
		return -EINVAL;
	}

	if (!crtc_ops->emergency_off) {
		panel_err(ctx, "no emergency_off handler\n");
		return -EINVAL;
	}

	if (exynos_crtc->ops->emergency_off)
		exynos_crtc->ops->emergency_off(exynos_crtc);

	return 0;
}

#if IS_ENABLED(CONFIG_PANEL_FREQ_HOP) || IS_ENABLED(CONFIG_USDM_PANEL_FREQ_HOP)
static int mcd_drm_panel_set_osc(struct exynos_panel *ctx, u32 frequency)
{
	struct panel_clock_info info;
	int ret;

	if (!ctx) {
		pr_err("FREQ_HOP: ERR:%s: invalid param\n", __func__);
		return -EINVAL;
	}

	info.clock_id = CLOCK_ID_OSC;
	info.clock_rate = frequency;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev,
			req_set_clock, &info);
	if (ret < 0) {
		pr_err("%s: failed to set ffc\n", __func__);
		return ret;
	}

	return 0;
}

static int mcd_drm_panel_set_ffc(struct exynos_panel *ctx, u32 frequency)
{
	struct panel_clock_info info;
	int ret;

	if (!ctx) {
		pr_err("FREQ_HOP: ERR:%s: invalid param\n", __func__);
		return -EINVAL;
	}

	info.clock_id = CLOCK_ID_DSI;
	info.clock_rate = frequency;

	ret = call_mcd_panel_func(ctx->mcd_panel_dev,
			req_set_clock, &info);
	if (ret < 0) {
		pr_err("%s: failed to set ffc\n", __func__);
		return ret;
	}

	return 0;
}

__visible_for_testing int mcd_drm_set_freq_hop(void *_ctx,
		struct freq_hop_param *param)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct exynos_panel *ctx = _ctx;
	int ret;

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi) {
		pr_err("%s: invalid dsi\n", __func__);
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	ret = usdm_mcd_dsim_update_dsi_freq(dsim, param->dsi_freq);
	if (ret < 0) {
		pr_err("%s: failed to update dsi_freq(%d)\n",
				__func__, param->dsi_freq);
		return ret;
	}

	ret = mcd_drm_panel_set_ffc(ctx, param->dsi_freq);
	if (ret < 0) {
		pr_err("%s: failed to set ffc(%d)\n",
				__func__, param->dsi_freq);
		return ret;
	}

	ret = mcd_drm_panel_set_osc(ctx, param->osc_freq);
	if (ret < 0) {
		pr_err("%s: failed to set osc(%d)\n",
				__func__, param->osc_freq);
		return ret;
	}

	return 0;
}
#endif

struct panel_adapter_funcs mcd_panel_adapter_funcs = {
	.read = mcd_drm_mipi_read,
	.write = mcd_drm_mipi_write,
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	.sr_write = mcd_drm_mipi_sr_write,
#else
	.sr_write = NULL,
#endif
	.write_table = mipi_write_table,
	.get_state = mcd_drm_mipi_get_state,
	.parse_dt = mcd_drm_parse_dt,
	.wait_for_vsync = mcd_drm_wait_for_vsync,
	.wait_for_fsync = mcd_drm_wait_for_fsync,
	.set_bypass = mcd_drm_set_bypass,
	.get_bypass = mcd_drm_get_bypass,
	.set_lpdt = mcd_drm_set_lpdt,
	.dpu_register_dump = mcd_drm_dpu_register_dump,
	.dpu_event_log_print = mcd_drm_dpu_event_log_print,
	.set_commit_retry = mcd_drm_set_commit_retry,
	.emergency_off = mcd_drm_emergency_off,
#if IS_ENABLED(CONFIG_PANEL_FREQ_HOP) || IS_ENABLED(CONFIG_USDM_PANEL_FREQ_HOP)
	.set_freq_hop = mcd_drm_set_freq_hop,
#endif
};

static const struct drm_panel_funcs mcd_drm_panel_funcs = {
	.disable = mcd_drm_panel_disable,
	.unprepare = mcd_drm_panel_unprepare,
	.prepare = mcd_drm_panel_prepare,
	.enable = mcd_drm_panel_enable,
	.get_modes = usdm_mcd_drm_panel_get_modes,
};

static const struct exynos_panel_funcs mcd_exynos_panel_funcs = {
	.set_lp_mode = usdm_exynos_panel_set_lp_mode,
	.mode_set = mcd_drm_panel_mode_set,
	.req_set_clock = mcd_drm_request_set_clock,
#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	.set_fingermask_layer = mcd_drm_panel_set_fingermask_layer,
#endif
};
static const struct exynos_panel_mode exynos_panel_modes[] = {
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	{
		.mode = { DRM_MODE("1080x2316@120", DRM_MODE_TYPE_DRIVER, 303196,
			1080, 1081, 1082, 1083, 0, 2316, 2331, 2332, 2333, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
			8, false, true, 2, 2, 193) },
	},
	{
		.mode = { DRM_MODE("1080x2340@120", DRM_MODE_TYPE_DRIVER, 306315,
			1080, 1081, 1082, 1083, 0, 2340, 2355, 2356, 2357, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
			8, false, true, 2, 2, 117) },
	},
	{
		.mode = { DRM_MODE("1080x2400@120", DRM_MODE_TYPE_DRIVER, 314113,
			1080, 1081, 1082, 1083, 0, 2400, 2415, 2416, 2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
			8, false, true, 2, 2, 120) },
	},
#else
	{
		.mode = { DRM_MODE("1080x2400@120", DRM_MODE_TYPE_DRIVER, 314113,
			1080, 1081, 1082, 1083, 0, 2400, 2415, 2416, 2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
			8, false, true, 1, 2, 40) },
	},
#endif
};
static const struct exynos_panel_mode exynos_panel_lp_mode[] = {
#if IS_ENABLED(CONFIG_SOC_S5E9925)
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78528,
				1080, 1081, 1082, 1083, 0, 2400, 2415, 2416, 2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 2, 2, 120) },
	},
#else
	{
		.mode = { DRM_MODE("1080x2400@30", DRM_MODE_TYPE_DRIVER, 78528,
				1080, 1081, 1082, 1083, 0, 2400, 2415, 2416, 2417, 0, 0) },
		.exynos_mode = { EXYNOS_MODE(MIPI_DSI_CLOCK_NON_CONTINUOUS,
				8, true, true, 1, 2, 40) },
	},
#endif
};

const struct exynos_panel_desc default_mcd_samsung_panel_desc = {
	.data_lane_cnt = 4,
	.max_brightness = 1023, /* TODO check */
	.dft_brightness = 511, /* TODO check */
	.modes = exynos_panel_modes,
	.num_modes = ARRAY_SIZE(exynos_panel_modes),
	.lp_modes = exynos_panel_lp_mode,
	.num_lp_modes = ARRAY_SIZE(exynos_panel_lp_mode),
	.panel_func = &mcd_drm_panel_funcs,
	.exynos_panel_func = &mcd_exynos_panel_funcs,
};

work_func_t mcd_drm_drv_wq_fns[MAX_MCD_DRM_DRV_WQ] = {
	[MCD_DRM_DRV_WQ_VSYNC] = wq_vblank_handler,
	[MCD_DRM_DRV_WQ_FSYNC] = wq_framedone_handler_fsync,
	[MCD_DRM_DRV_WQ_DISPON] = wq_framedone_handler_dispon,
	[MCD_DRM_DRV_WQ_PANEL_PROBE] = mcd_panel_probe_handler,
};

char *mcd_drm_drv_wq_names[MAX_MCD_DRM_DRV_WQ] = {
	[MCD_DRM_DRV_WQ_VSYNC] = "wq_vsync",
	[MCD_DRM_DRV_WQ_FSYNC] = "wq_fsync",
	[MCD_DRM_DRV_WQ_DISPON] = "wq_dispon",
	[MCD_DRM_DRV_WQ_PANEL_PROBE] = "wq_panel_probe",
};

int mcd_drm_drv_wq_exit(struct exynos_panel *ctx)
{
	struct mcd_drm_drv_wq *w;
	int i;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_MCD_DRM_DRV_WQ; i++) {
		if (!mcd_drm_drv_wq_fns[i])
			continue;

		w = &ctx->wqs[i];
		if (w->wq) {
			destroy_workqueue(w->wq);
			w->wq = NULL;
		}
	}
	return 0;
}

int mcd_drm_drv_wq_init(struct exynos_panel *ctx)
{
	struct mcd_drm_drv_wq *w;
	int ret, i;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < MAX_MCD_DRM_DRV_WQ; i++) {
		if (!mcd_drm_drv_wq_fns[i])
			continue;

		w = &ctx->wqs[i];
		w->index = i;
		INIT_DELAYED_WORK(&w->dwork, mcd_drm_drv_wq_fns[i]);
		w->wq = create_singlethread_workqueue(mcd_drm_drv_wq_names[i]);
		if (!w->wq) {
			panel_err(ctx, "failed to workqueue(%s) initialize\n",
					mcd_drm_drv_wq_names[i]);
			ret = mcd_drm_drv_wq_exit(ctx);
			if (ret < 0)
				panel_err(ctx, "failed to workqueue(%s) exit(ret:%d)\n",
						mcd_drm_drv_wq_names[i], ret);
			return -EINVAL;
		}
		w->name = mcd_drm_drv_wq_names[i];
		init_waitqueue_head(&w->wait);
		atomic_set(&w->count, 0);
	}

	return 0;
}

int mcd_drm_panel_get_size_mm(struct exynos_panel *ctx,
		unsigned int *width_mm, unsigned int *height_mm)
{
	struct device_node *np;
	unsigned int size_mm[2];

	if (!ctx || !ctx->dev || !ctx->mcd_panel_dev)
		return -ENODEV;

	/* TODO: get width_mm, height_mm from mcd-panel */

	/* temporary get width_mm, height_mm directly */
	np = ctx->mcd_panel_dev->ap_vendor_setting_node;
	if (!np) {
		panel_err(ctx, "mcd_panel ddi-node is null\n");
		return -EINVAL;
	}

	of_property_read_u32_array(np, "size", size_mm, 2);

	*width_mm = size_mm[0];
	*height_mm = size_mm[1];

	return 0;
}

bool mcd_drm_panel_get_lp11_reset(struct exynos_panel *ctx)
{
	struct device_node *np;
	bool ret = false;
	u32 reg = 0;

	if (!ctx || !ctx->dev || !ctx->mcd_panel_dev)
		return -ENODEV;

	np = ctx->mcd_panel_dev->ap_vendor_setting_node;
	if (!np) {
		panel_err(ctx, "mcd_panel ddi-node is null\n");
		return -EINVAL;
	}

	of_property_read_u32(np, "lp11_reset", &reg);

	ret = reg ? true : false;

	panel_info(ctx, "lp11_reset:(%d)\n", reg);

	return ret;
}

struct exynos_panel_desc *
mcd_drm_panel_get_exynos_panel_desc(struct exynos_panel *ctx)
{
	struct panel_display_modes *pdms;
	struct exynos_panel_desc *desc;
	unsigned int width_mm = 0, height_mm = 0;
	struct exynos_panel_mode *epms;
	int i, ret;

	if (!ctx)
		return ERR_PTR(-EINVAL);

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, get_display_mode, &pdms);
	if (ret < 0) {
		panel_err(ctx, "failed to get_display_mode(ret:%d)\n", ret);
		return ERR_PTR(ret);
	}

	/* create exynos_panel_desc from mcd-panel driver */
	desc = usdm_exynos_panel_desc_create_from_panel_display_modes(ctx, pdms);
	desc->panel_func = &mcd_drm_panel_funcs;
	desc->exynos_panel_func = &mcd_exynos_panel_funcs;
	desc->lp11_reset = mcd_drm_panel_get_lp11_reset(ctx);

	/* set width_mm, height_mm on exynos_panel_mode */
	ret = mcd_drm_panel_get_size_mm(ctx, &width_mm, &height_mm);
	if (ret < 0) {
		panel_err(ctx, "failed to get width_mm, height_mm(ret:%d)\n", ret);
		kfree(desc);
		return ERR_PTR(ret);
	}

	epms = (struct exynos_panel_mode *)desc->modes;
	for (i = 0; i < (int)desc->num_modes; i++) {
		epms[i].mode.width_mm = width_mm;
		epms[i].mode.height_mm = height_mm;
	}

	epms = (struct exynos_panel_mode *)desc->lp_modes;
	for (i = 0; i < (int)desc->num_lp_modes; i++) {
		epms[i].mode.width_mm = width_mm;
		epms[i].mode.height_mm = height_mm;
	}

	return desc;
}

int mcd_drm_panel_probe(struct exynos_panel *ctx)
{
	struct platform_device *pdev;
	struct device_node *np;
	struct panel_adapter adapter = {
		.ctx = ctx,
		.fifo_size = DSIM_PL_FIFO_THRESHOLD,
		.funcs = &mcd_panel_adapter_funcs,
	};
	int ret;

	if (!ctx)
		return -EINVAL;

	init_rwsem(&ctx->panel_drm_state_lock);
	mutex_init(&ctx->probe_lock);
	ctx->panel_drm_state = PANEL_DRM_STATE_DISABLED;

	np = of_find_compatible_node(NULL, NULL, "samsung,panel-drv");
	if (!np) {
		panel_err(ctx, "compatible(\"samsung,panel-drv\") node not found\n");
		return -ENOENT;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		panel_err(ctx, "mcd-panel device not found\n");
		return -ENODEV;
	}

	ctx->mcd_panel_dev = (struct panel_device *)platform_get_drvdata(pdev);
	if (!ctx->mcd_panel_dev) {
		panel_err(ctx, "failed to get panel_device\n");
		return -ENODEV;
	}

	ret = call_mcd_panel_func(ctx->mcd_panel_dev, attach_adapter, &adapter);
	if (ret < 0) {
		panel_err(ctx, "mcd_panel call attach_adapter failed %d", ret);
		return ret;
	}

	ret = mcd_drm_drv_wq_init(ctx);
	if (ret < 0) {
		panel_err(ctx, "mcd_panel initialize workqueue failed(ret:%d)\n", ret);
		return ret;
	}

	queue_delayed_work(ctx->wqs[MCD_DRM_DRV_WQ_PANEL_PROBE].wq,
			&ctx->wqs[MCD_DRM_DRV_WQ_PANEL_PROBE].dwork,
			msecs_to_jiffies(MCD_PANEL_PROBE_DELAY_MSEC));

	return 0;
}

static void notify_lp11_reset(struct exynos_panel *ctx, bool en)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi || !dsi->host) {
		panel_err(ctx, "dsi not attached yet\n");
		return;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	dsim->lp11_reset = en;
}

int usdm_exynos_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct exynos_panel *ctx;
	int ret = 0;

	if (!sec_needs_decon) {
		SEC_DETECT_LOG("Initialized mcd common panel driver for usdm\n");
		ctx = devm_kzalloc(dev, sizeof(struct exynos_panel), GFP_KERNEL);
		if (!ctx)
			return -ENOMEM;

		pr_info("%s: %s+\n", dev->driver->name, __func__);

		mipi_dsi_set_drvdata(dsi, ctx);
		ctx->dev = dev;

		ret = mcd_drm_panel_probe(ctx);
		if (ret < 0) {
			panel_err(ctx, "failed to probe mcd_drm_panel(ret:%d)\n", ret);
			return ret;
		}

		ctx->desc = mcd_drm_panel_get_exynos_panel_desc(ctx);
		if (IS_ERR_OR_NULL(ctx->desc)) {
			panel_err(ctx, "failed to get exynos_panel_desc (ret:%ld)\n",
					PTR_ERR(ctx->desc));
			return PTR_ERR(ctx->desc);
		}

		dsi->lanes = ctx->desc->data_lane_cnt;
		dsi->format = MIPI_DSI_FMT_RGB888;

		notify_lp11_reset(ctx, ctx->desc->lp11_reset);

		drm_panel_init(&ctx->panel, dev,
				&mcd_drm_panel_funcs, DRM_MODE_CONNECTOR_DSI);
		drm_panel_add(&ctx->panel);

		ctx->bridge.funcs = &exynos_panel_bridge_funcs;
#ifdef CONFIG_OF
		ctx->bridge.of_node = ctx->dev->of_node;
#endif
		drm_bridge_add(&ctx->bridge);

		ret = mipi_dsi_attach(dsi);
		if (ret < 0) {
			panel_err(ctx, "failed to attach mipi dsi(ret:%d)\n", ret);
			return ret;
		}

		exynos_panel_set_dqe_xml(dev, ctx);

		exynos_panel_parse_vendor_pps(ctx);

		exynos_panel_parse_vfp_detail(ctx);

		panel_info(ctx, "mcd common panel driver has been probed\n");
	} else {
		return -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(usdm_exynos_panel_probe);

int usdm_exynos_panel_remove(struct mipi_dsi_device *dsi)
{
	struct exynos_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	drm_bridge_remove(&ctx->bridge);
	devm_backlight_device_unregister(ctx->dev, ctx->bl);
	usdm_exynos_panel_desc_destroy(ctx, (struct exynos_panel_desc *)ctx->desc);

	return 0;
}
EXPORT_SYMBOL(usdm_exynos_panel_remove);

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,mcd-panel-samsung-drv" },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = usdm_exynos_panel_probe,
	.remove = usdm_exynos_panel_remove,
	.driver = {
		.name = "mcd-panel-samsung-drv-usdm",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_SOFTDEP("pre: mcd-panel");

MODULE_AUTHOR("Jiun Yu <jiun.yu@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung common panel driver");
MODULE_LICENSE("GPL");
