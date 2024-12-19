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

#include <drm/drm_panel.h>
#include <drm/drm_encoder.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>

#include <exynos_drm_decon.h>
#include <exynos_drm_dsim.h>
#include <exynos_display_common.h>
#include "../exynos_drm_crtc.h"
#include "../exynos_drm_connector.h"
#include "../exynos_drm_tui.h"
#include "../exynos_drm_dqe.h"
#include "panel-samsung-drv.h"

static int panel_log_level = 6;
module_param(panel_log_level, int, 0644);
MODULE_PARM_DESC(panel_log_level, "log level for panel drv [default : 6]");

int decon_get_panel_log_level(void)
{
	return panel_log_level;
}
EXPORT_SYMBOL(decon_get_panel_log_level);
static int exynos_panel_get_bts_fps(const struct exynos_panel *ctx,
				     const struct exynos_panel_mode *pmode);

struct edid panel_edid;
static struct exynos_panel *base_ctx;

static int exynos_panel_get_bts_fps(const struct exynos_panel *ctx,
				     const struct exynos_panel_mode *pmode);

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
		return decon_is_tui_trans(conn_state->crtc->state);

	return false;
}

static int exynos_panel_parse_gpios(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;

	panel_debug(ctx, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR)) {
		panel_info(ctx, "no reset/enable pins on emulator\n");
		return 0;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(ctx->reset_gpio)) {
		panel_err(ctx, "failed to get reset-gpios %ld",
				PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->enabled = gpiod_get_raw_value(ctx->reset_gpio) > 0;
	if (!ctx->enabled)
		gpiod_direction_output(ctx->reset_gpio, 0);

	ctx->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio))
		ctx->enable_gpio = NULL;

	panel_debug(ctx, "-\n");
	return 0;
}

static int exynos_panel_parse_regulators(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;

	ctx->vddi = devm_regulator_get(dev, "vddi");
	if (IS_ERR(ctx->vddi)) {
		panel_warn(ctx, "failed to get panel vddi.\n");
		ctx->vddi = NULL;
	}

	ctx->vci = devm_regulator_get(dev, "vci");
	if (IS_ERR(ctx->vci)) {
		panel_warn(ctx, "failed to get panel vci.\n");
		ctx->vci = NULL;
	}

	if (ctx->enabled) {
		if (ctx->vddi != NULL && regulator_enable(ctx->vddi))
			panel_warn(ctx, "vddi enable failed\n");

		if (ctx->vci != NULL && regulator_enable(ctx->vci))
			panel_warn(ctx, "vci enable failed\n");
	}

	return 0;
}

#define PANEL_ID_READ_SIZE	3
static int exynos_panel_read_id(struct exynos_panel *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	char buf[4] = {0, };
	int ret;

	ret = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_DISPLAY_ID, buf,
			PANEL_ID_READ_SIZE);
	if (ret != PANEL_ID_READ_SIZE) {
		panel_warn(ctx, "Unable to read panel id (%d)\n", ret);
		return ret;
	}
	panel_info(ctx, "read panel buf(0x%x%x%x%x)\n", buf[3], buf[2], buf[1], buf[0]);

	return 0;
}

void decon_exynos_panel_reset(struct exynos_panel *ctx)
{
	panel_debug(ctx, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return;

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);

	exynos_panel_read_id(ctx);

	panel_debug(ctx, "-\n");
}
EXPORT_SYMBOL(decon_exynos_panel_reset);

int decon_exynos_panel_set_power(struct exynos_panel *ctx, bool on)
{
	int ret;

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR))
		return 0;

#if 0
	if (!ctx->vddi) {
		panel_debug(ctx, "trying to get vddi regulator\n");
		ctx->vddi = devm_regulator_get(ctx->dev, "vddi");
		if (IS_ERR(ctx->vddi)) {
			panel_warn(ctx, "failed to get vddi regulator\n");
			ctx->vddi = NULL;
		}
	}

	if (!ctx->vci) {
		panel_debug(ctx, "trying to get vci regulator\n");
		ctx->vci = devm_regulator_get(ctx->dev, "vci");
		if (IS_ERR(ctx->vci)) {
			panel_warn(ctx, "failed to get vci regulator\n");
			ctx->vci = NULL;
		}
	}
#endif

	if (on) {
		if (ctx->enable_gpio) {
			gpiod_set_value(ctx->enable_gpio, 1);
			usleep_range(10000, 11000);
		}

		if (ctx->vddi) {
			ret = regulator_enable(ctx->vddi);
			if (ret) {
				panel_err(ctx, "vddi enable failed\n");
				return ret;
			}
			usleep_range(5000, 6000);
		}

		if (ctx->vci) {
			ret = regulator_enable(ctx->vci);
			if (ret) {
				panel_err(ctx, "vci enable failed\n");
				return ret;
			}
		}
	} else {
		gpiod_set_value(ctx->reset_gpio, 0);
		if (ctx->enable_gpio)
			gpiod_set_value(ctx->enable_gpio, 0);

		if (ctx->vddi) {
			ret = regulator_disable(ctx->vddi);
			if (ret) {
				panel_err(ctx, "vddi disable failed\n");
				return ret;
			}
		}

		if (ctx->vci > 0) {
			ret = regulator_disable(ctx->vci);
			if (ret) {
				panel_err(ctx, "vci disable failed\n");
				return ret;
			}
		}
	}

	ctx->bl->props.power = on ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN;

	return 0;
}
EXPORT_SYMBOL(decon_exynos_panel_set_power);

static int exynos_panel_parse_dt(struct exynos_panel *ctx)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx->dev->of_node)) {
		panel_err(ctx, "no device tree information of exynos panel\n");
		return -EINVAL;
	}

	ret = exynos_panel_parse_gpios(ctx);
	if (ret)
		goto err;

	ret = exynos_panel_parse_regulators(ctx);
	if (ret)
		goto err;
err:
	return ret;
}

int decon_exynos_panel_get_modes(struct drm_panel *panel, struct drm_connector *conn)
{
	struct exynos_panel *ctx =
		container_of(panel, struct exynos_panel, panel);
	struct drm_display_mode *preferred_mode = NULL;
	int i;

	panel_debug(ctx, "+\n");

	for (i = 0; i < ctx->desc->num_modes; i++) {
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
EXPORT_SYMBOL(decon_exynos_panel_get_modes);

void decon_exynos_panel_set_lp_mode(struct exynos_panel *ctx, const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}
EXPORT_SYMBOL(decon_exynos_panel_set_lp_mode);

static int exynos_get_brightness(struct backlight_device *bl)
{
	int ret;
	u16 brightness;
	struct exynos_panel *ctx = bl_get_data(bl);

	if (!ctx->enabled) {
		panel_err(ctx, "panel is not enabled\n");
		return -EPERM;
	}

	ret = exynos_dcs_get_brightness(ctx, &brightness);
	if (ret < 0)
		return ret;

	return brightness;
}

static int exynos_update_status(struct backlight_device *bl)
{
	int ret;
	struct exynos_panel *ctx = bl_get_data(bl);
	const struct exynos_panel_funcs *exynos_panel_func;
	int brightness = bl->props.brightness;

	panel_debug(ctx, "br: %d, max br: %d\n", brightness,
		bl->props.max_brightness);

	if (!ctx->enabled) {
		panel_err(ctx, "panel is not enabled\n");
		return -EPERM;
	}

	/* check if backlight is forced off */
	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	exynos_panel_func = ctx->desc->exynos_panel_func;
	if (exynos_panel_func && exynos_panel_func->set_brightness)
		ret = exynos_panel_func->set_brightness(ctx, brightness);
	else
		ret = exynos_dcs_set_brightness(ctx, brightness);

	return ret;
}

static const struct backlight_ops exynos_backlight_ops = {
	.get_brightness = exynos_get_brightness,
	.update_status = exynos_update_status,
};

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
}

int decon_exynos_drm_cmdset_add(struct exynos_panel *ctx, u8 type, size_t size,
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
EXPORT_SYMBOL(decon_exynos_drm_cmdset_add);

int decon_exynos_drm_cmdset_cleanup(struct exynos_panel *ctx)
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
	}
	ctx->cmdset_msg_total = 0;
	ctx->cmdset_payload_total = 0;

	panel_debug(ctx, "-\n");

	return 0;
}
EXPORT_SYMBOL(decon_exynos_drm_cmdset_cleanup);

int decon_exynos_drm_cmdset_flush(struct exynos_panel *ctx, bool wait_vsync,
							bool wait_fifo)
{
	int ret;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_msg *msg_head;

	if (!ctx->cmdset_msg_total) {
		panel_err(ctx, "there is no MIPI command to transfer\n");
		return -EINVAL;
	}

	msg_head = ctx->msg;
	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg_head->flags |= MIPI_DSI_MSG_USE_LPM;

	ret = decon_dsim_host_cmdset_transfer(dsi->host, ctx->msg, ctx->cmdset_msg_total, wait_vsync, wait_fifo);
	if (ret < 0)
		panel_err(ctx, "failed to tx command set data\n");

	ret = decon_exynos_drm_cmdset_cleanup(ctx);
	if (ret < 0)
		panel_err(ctx, "failed to cleanup command set data\n");

	return ret;
}
EXPORT_SYMBOL(decon_exynos_drm_cmdset_flush);

static int
exynos_panel_connector_set_property(struct exynos_drm_connector *exynos_conn,
			struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t val)
{
	/* if necessary, fill it up */
	return 0;
}

static int
exynos_panel_connector_get_property(struct exynos_drm_connector *exynos_conn,
			const struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t *val)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_drm_connector_properties *p =
		decon_exynos_drm_connector_get_properties(&ctx->exynos_connector);

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
	else
		return -EINVAL;

	return 0;
}

#define PANEL_QUERY_CMD_COUNT	3
/*
 * - MIPI_DCS_GET_ERROR_COUNT_ON_DSI
 * : The display module returns the number of corrupted packets previously
 *   received on the DSI link.
 *
 * - MIPI_DCS_GET_POWER_MODE
 * : The display module returns the current power mode.
 *   D2 : display off/on
 *   D3 : normal mode off/on
 *   D4 : sleep mode on/off ('0' = sleep mode on, '1' = sleep mode off)
 *   D5 : partial mode off/on
 *   D6 : idle mode off/on
 *
 * - MIPI_DCS_GET_SIGNAL_MODE
 * : The display module returns the Display Signal Mode.
 *   D0 : error on dsi
 *   D6 : TE output mode
 *   D7 : TE off/on
 */
static void
exynos_panel_connector_query_status(struct exynos_drm_connector *exynos_conn)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u32 i;
	u8 dcs_cmd[PANEL_QUERY_CMD_COUNT] = {MIPI_DCS_GET_ERROR_COUNT_ON_DSI,
			MIPI_DCS_GET_POWER_MODE, MIPI_DCS_GET_SIGNAL_MODE};
	char status[PANEL_QUERY_CMD_COUNT];
	int ret[PANEL_QUERY_CMD_COUNT];

	if (!ctx->enabled) {
		panel_info(ctx, "panel is not enabled\n");
		return;
	}

	for (i = 0; i < PANEL_QUERY_CMD_COUNT; i++) {
		ret[i] = mipi_dsi_dcs_read(dsi, dcs_cmd[i], &status[i], 1);
		if (ret[i] <= 0)
			goto query_end;
	}

query_end:
	for (i = 0; i < PANEL_QUERY_CMD_COUNT; i++) {
		if (ret[i] == 1)
			panel_info(ctx, "0x%02X = 0x%02x\n", dcs_cmd[i], status[i]);
		else {
			if (ret[i] == 0)
				ret[i] = -ENODATA;

			panel_err(ctx, "Unable to read 0x%02X(ret = %d)\n",
					dcs_cmd[i], ret[i]);
			break;
		}
	}
}

static const struct exynos_drm_connector_funcs exynos_panel_connector_funcs = {
	.atomic_print_state = exynos_panel_connector_print_state,
	.atomic_set_property = exynos_panel_connector_set_property,
	.atomic_get_property = exynos_panel_connector_get_property,
	.query_status = exynos_panel_connector_query_status,
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

	for (i = 0; i < ctx->desc->num_modes; i++) {
		pmode = &ctx->desc->modes[i];

		if (drm_mode_equal(&pmode->mode, mode))
			return pmode;
	}

	for (i = 0; i < ctx->desc->num_lp_modes; i++) {
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
		decon_exynos_drm_connector_get_properties(exynos_conn);
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
	for (i = 0; i < num_lp_modes; i++)
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
		decon_exynos_drm_connector_get_properties(&ctx->exynos_connector);
	struct drm_mode_object *obj = &ctx->exynos_connector.base.base;
	const struct exynos_panel_desc *desc = ctx->desc;
	int ret = 0;

	if (!p || !desc)
		return -ENOENT;

	drm_object_attach_property(obj, p->min_luminance, desc->min_luminance);
	drm_object_attach_property(obj, p->max_luminance, desc->max_luminance);
	drm_object_attach_property(obj, p->max_avg_luminance,
			desc->max_avg_luminance);
	drm_object_attach_property(obj, p->hdr_formats, desc->hdr_formats);
	drm_object_attach_property(obj, p->adjusted_fps, 0);

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

	ret = decon_exynos_drm_connector_init(bridge->dev, &ctx->exynos_connector,
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
		decon_exynos_panel_reset(ctx);
		decon_dsim_atomic_activate(bridge->encoder);
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

void exynos_panel_active_off(struct exynos_panel *panel)
{
	struct drm_crtc *crtc = panel->bridge.encoder->crtc;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);;

	if (exynos_crtc->ops->emergency_off)
		exynos_crtc->ops->emergency_off(exynos_crtc);
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

static void exynos_panel_parse_vendor_pps(struct device *dev, struct exynos_panel *ctx)
{
		struct device_node *np = dev->of_node;
		struct drm_device *drm_dev = NULL;
		struct drm_crtc *crtc;
		struct exynos_drm_crtc *exynos_crtc = NULL;
		struct decon_device *decon;

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
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);
	struct dsim_device *dsim =
		container_of(dsi->host, struct dsim_device, dsi_host);

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
	if (!(pmode->exynos_mode.mode_flags & MIPI_DSI_MODE_VIDEO))
		return drm_mode_vrefresh(&pmode->mode);

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
	struct device_node *np = dev->of_node;
	struct drm_device *drm_dev = NULL;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc = NULL;
	struct exynos_dqe *dqe;
	const char *xml_suffix = NULL;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);
	struct dsim_device *dsim =
		container_of(dsi->host, struct dsim_device, dsi_host);

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

	if (of_property_read_string(np, "dqe-suffix", &xml_suffix) != 0) {
		if (dsim && dsim->dsi_device) {
			if (strlen(dsim->dsi_device->name) > 0)
				xml_suffix = dsim->dsi_device->name;
			else if (dsim->dsi_device->dev.driver)
				xml_suffix = dsim->dsi_device->dev.driver->name;
		}
	}

	if (xml_suffix != NULL)
		strlcpy(dqe->xml_suffix, xml_suffix, DQE_XML_SUFFIX_SIZE);

	panel_info(ctx, "DQE XML Suffix (%s)\n", dqe->xml_suffix);
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

int decon_drm_mipi_fcmd_write(void *_ctx, const u8 *payload, int size, u32 align)
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
		pr_err("%s: invalid payloda\n", __func__);
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

	ret = decon_dsim_host_fcmd_transfer(dsi->host, &fcmd->msg);
	if (ret < 0)
		panel_err(ctx, "failed to transfer fast-command\n");

out:
	mutex_unlock(&lock);
	destroy_dsim_fast_cmd(fcmd);

	return ret;
}
EXPORT_SYMBOL(decon_drm_mipi_fcmd_write);
#endif

static ssize_t active_off_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t active_off_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	exynos_panel_active_off(base_ctx);
	return len;
}
static DEVICE_ATTR_RW(active_off);

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

int decon_exynos_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct exynos_panel *ctx;
	int ret = 0;
	char name[32];

	ctx = devm_kzalloc(dev, sizeof(struct exynos_panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	pr_info("%s: %s+\n", dev->driver->name, __func__);

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	ctx->desc = of_device_get_match_data(dev);

	dsi->lanes = ctx->desc->data_lane_cnt;
	dsi->format = MIPI_DSI_FMT_RGB888;

	exynos_panel_parse_dt(ctx);

	notify_lp11_reset(ctx, ctx->desc->lp11_reset);

	snprintf(name, sizeof(name), "panel%d-backlight", dsi->channel);
	ctx->bl = devm_backlight_device_register(ctx->dev, name, NULL,
			ctx, &exynos_backlight_ops, NULL);
	if (IS_ERR(ctx->bl)) {
		panel_err(ctx, "failed to register backlight device\n");
		return PTR_ERR(ctx->bl);
	}
	ctx->bl->props.max_brightness = ctx->desc->max_brightness;
	ctx->bl->props.brightness = ctx->desc->dft_brightness;

	drm_panel_init(&ctx->panel, dev, ctx->desc->panel_func, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ctx->bridge.funcs = &exynos_panel_bridge_funcs;
#ifdef CONFIG_OF
	ctx->bridge.of_node = ctx->dev->of_node;
#endif
	drm_bridge_add(&ctx->bridge);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		goto err_panel;

	exynos_panel_set_dqe_xml(dev, ctx);

	exynos_panel_parse_vendor_pps(dev, ctx);

	exynos_panel_parse_vfp_detail(ctx);

	exynos_panel_get_max_fps(dev, ctx);

	ret = device_create_file(dev, &dev_attr_active_off);
	if (ret < 0)
		pr_err("failed to add sysfs entries\n");

	base_ctx = ctx;

	panel_info(ctx, "samsung common panel driver has been probed\n");

	return 0;

err_panel:
	drm_panel_remove(&ctx->panel);
	panel_err(ctx, "failed to probe samsung panel driver(%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL(decon_exynos_panel_probe);

int decon_exynos_panel_remove(struct mipi_dsi_device *dsi)
{
	struct exynos_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	drm_bridge_remove(&ctx->bridge);
	devm_backlight_device_unregister(ctx->dev, ctx->bl);
	device_remove_file(&dsi->dev, &dev_attr_active_off);

	return 0;
}
EXPORT_SYMBOL(decon_exynos_panel_remove);

MODULE_AUTHOR("Jiun Yu <jiun.yu@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung common panel driver");
MODULE_LICENSE("GPL");
