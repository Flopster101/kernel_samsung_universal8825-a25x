// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_writeback.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/dma-buf.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>

#include <drm/drm_atomic.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_fourcc.h>

#include <exynos_drm_fb.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_format.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_crtc.h>

static int dpu_writeback_log_level = 6;
module_param(dpu_writeback_log_level, int, 0600);
MODULE_PARM_DESC(dpu_writeback_log_level, "log level for writeback [default : 6]");

#define wb_info(wb, fmt, ...)	\
dpu_pr_info(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_warn(wb, fmt, ...)	\
dpu_pr_warn(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_err(wb, fmt, ...)	\
dpu_pr_err(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

#define wb_debug(wb, fmt, ...)	\
dpu_pr_debug(drv_name((wb)), (wb)->id, dpu_writeback_log_level, fmt, ##__VA_ARGS__)

static inline enum exynos_drm_writeback_type
get_wb_type(const struct drm_connector_state *state)
{
	struct drm_crtc *crtc = state->crtc;
	const struct decon_device *decon;

	if (!crtc) {
		pr_err("[%s] no attach wb to crtc\n", __func__);
		return EXYNOS_WB_NONE;
	}
	decon = to_exynos_crtc(crtc)->ctx;

	return decon->config.out_type == DECON_OUT_WB ?
				EXYNOS_WB_SWB : EXYNOS_WB_CWB;
}

void wb_dump(struct writeback_device *wb)
{
	if (wb->state != WB_STATE_ON) {
		wb_info(wb, "writeback state is off\n");
		return;
	}

	__dpp_dump(wb->id, &wb->regs);
}

static const uint32_t writeback_formats[] = {
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_NV12,
};

static const struct of_device_id wb_of_match[] = {
	{
		.compatible = "samsung,exynos-writeback",
		/* TODO : check odma, wbmux, decon_win restriction */
		.data = &dpp_drv_data,
	},
};

static int writeback_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	const static struct drm_display_mode dft_mode = {
		DRM_MODE("4096x4096", DRM_MODE_TYPE_DRIVER, 556188, 4096,
				4097, 4098, 4099, 0, 4096, 4097, 4098, 4099, 0,
				DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC) };
	int num_modes = 1;

	mode = drm_mode_duplicate(connector->dev, &dft_mode);
	drm_mode_probed_add(connector, mode);

	mode->type |= DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	return num_modes;
}

static void repeater_get_base_addr(struct dpp_params_info *config,
		const struct exynos_drm_writeback_state *state)
{
	int i;
	struct drm_framebuffer *fb = state->base.writeback_job->fb;
	struct writeback_device *wb = conn_to_wb_dev(state->base.connector);

	/* 1. init */
	for (i = 0; i < MAX_PLANE_ADDR_CNT; i++)
		config->addr[i] = wb->rdb.dma_addr[state->repeater_valid_buf_idx];
	config->votf_o_idx = state->repeater_valid_buf_idx;
	config->votf_o_en = true;

	/* 2. adjustment */
	switch (fb->format->format) {
	case DRM_FORMAT_NV12:
		config->addr[1] = NV12N_CBCR_BASE(config->addr[0], fb->width, fb->height);
		break;
	default:
		wb_debug(wb, "format(%d) doesn't require BASE ADDR Calc.\n",
				fb->format->format);
		break;
	}
}

static void wb_convert_connector_state_to_config(struct dpp_params_info *config,
				const struct exynos_drm_writeback_state *state)
{
	struct drm_framebuffer *fb = state->base.writeback_job->fb;
	const struct writeback_device *wb = conn_to_wb_dev(state->base.connector);
	const struct drm_crtc_state *crtc_state = state->base.crtc->state;

	wb_debug(wb, "%s +\n", __func__);

	config->src.x = 0;
	config->src.y = 0;
	config->src.f_w = fb->width;
	config->src.f_h = fb->height;
	switch (state->type) {
	case EXYNOS_WB_CWB:
		config->src.w = crtc_state->mode.hdisplay;
		config->src.h = crtc_state->mode.vdisplay;
		break;
	case EXYNOS_WB_SWB:
		config->src.w = fb->width;
		config->src.h = fb->height;
		break;
	default:
		config->src.w = fb->width;
		config->src.h = fb->height;
		wb_err(wb, "%s unsupported wb_type(%d)\n", __func__, state->type);
		break;
	}

	config->comp_type = COMP_TYPE_NONE;

	config->format = fb->format->format;
	config->standard = state->standard;
	config->range = state->range;
	config->y_hd_y2_stride = 0;
	config->y_pl_c2_stride = 0;
	config->c_hd_stride = 0;
	config->c_pl_stride = 0;

	config->votf_o_en = false;
	config->addr[0] = exynos_drm_fb_dma_addr(fb, 0);
	config->addr[1] = exynos_drm_fb_dma_addr(fb, 1);
	config->addr[2] = exynos_drm_fb_dma_addr(fb, 2);
	config->addr[3] = exynos_drm_fb_dma_addr(fb, 3);
	if (wb->rdb.repeater_buf_active == true
			&& state->use_repeater_buffer == true)
		repeater_get_base_addr(config, state);

	/* TODO: blocking mode will be implemented later */
	config->is_block = false;
	/* TODO: very big count.. recovery will be not working... */
	config->rcv_num = 0x7FFFFFFF;

	wb_debug(wb, "%s -\n", __func__);
}

static int writeback_check_cwb(struct drm_crtc_state *crtc_state,
					const struct drm_framebuffer *fb)
{
	/* the fb smaller than lcd resol are not supported */
	if (fb->width < crtc_state->mode.hdisplay ||
			fb->height < crtc_state->mode.vdisplay)
		return -EINVAL;

	return 0;
}

static int writeback_check_swb(struct drm_crtc_state *crtc_state,
					const struct drm_framebuffer *fb)
{
	/* the fb bigger than 4K x 4K are not supported */
	if (fb->width > crtc_state->mode.hdisplay ||
			fb->height > crtc_state->mode.vdisplay)
		return -EINVAL;

	return 0;
}

static int writeback_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	const struct writeback_device *wb = enc_to_wb_dev(encoder);
	const struct drm_framebuffer *fb;
	struct exynos_drm_writeback_state *wb_state = to_exynos_wb_state(conn_state);
	int i, ret = 0;

	if (!wb_check_job(conn_state))
		return ret;

	wb_state->type = get_wb_type(conn_state);
	conn_state->writeback_job->priv = conn_state;
	fb = conn_state->writeback_job->fb;

	if (wb_state->type == EXYNOS_WB_CWB)
		ret = writeback_check_cwb(crtc_state, fb);
	else
		ret = writeback_check_swb(crtc_state, fb);
	if (ret) {
		wb_err(wb, "Invalid fb size %ux%u for %s mode size %ux%u\n",
				fb->width, fb->height,
				wb_state->type == EXYNOS_WB_CWB ? "CWB" : "SWB",
				crtc_state->mode.hdisplay,
				crtc_state->mode.vdisplay);
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(writeback_formats); i++)
		if (fb->format->format == writeback_formats[i])
			break;

	if (i == ARRAY_SIZE(writeback_formats)) {
		struct drm_format_name_buf format_name;

		drm_get_format_name(fb->format->format, &format_name);
		wb_err(wb, "Invalid fb format(%s)\n", format_name);
		ret = -EINVAL;
		goto err;
	}

err:
	return ret;
}

static void get_repeater_buffer(struct writeback_device *wb,
		struct exynos_drm_writeback_state *wb_state)
{
	struct repeater_dma_buf *rdb = &wb->rdb;
	int i;
	const struct decon_device *decon = wb_get_decon(wb);
	int ret = 0;
	if (wb->rdb.votf_o_feature_enabled == false ||
			wb_state->type == EXYNOS_WB_CWB)
		return;

	if (wb->rdb.repeater_buf_active == false) {
		ret = repeater_request_buffer(&rdb->repeater_shared_buf, 0);
		if (ret || (rdb->repeater_shared_buf.buffer_count > MAX_SHARED_BUF_NUM)) {
			wb_debug(wb, "repeater buffer request failed\n");
			goto err_request_buf;
		}
		if (IS_ERR_OR_NULL(decon->dev)) {
			wb_err(wb, "decon device connected to wb invalid\n");
			goto err_request_buf;
		}
		for (i = 0; i < MAX_SHARED_BUF_NUM; i++) {
			rdb->attachment[i] = dma_buf_attach(rdb->repeater_shared_buf.bufs[i],
					decon->dev);
			rdb->sg_table[i] = dma_buf_map_attachment(rdb->attachment[i],
					DMA_TO_DEVICE);
			rdb->dma_addr[i] = sg_dma_address(rdb->sg_table[i]->sgl);
			if (IS_ERR_OR_NULL(rdb->attachment[i]) ||
				IS_ERR_OR_NULL(rdb->sg_table[i]) ||
				!!!rdb->dma_addr[i]) {
				wb_err(wb, "repeater buffer map failed\n");
				goto err_map_buf;
			}
		}
		wb->rdb.repeater_buf_active = true;
	}
	return;
err_map_buf:
	for (i = 0; i < MAX_SHARED_BUF_NUM; i++) {
		if (!IS_ERR_OR_NULL(rdb->attachment[i]) && !IS_ERR_OR_NULL(rdb->sg_table[i]))
			dma_buf_unmap_attachment(rdb->attachment[i],
					rdb->sg_table[i], DMA_TO_DEVICE);
		if (rdb->repeater_shared_buf.bufs[i] && !IS_ERR_OR_NULL(rdb->attachment[i]))
			dma_buf_detach(rdb->repeater_shared_buf.bufs[i],
					rdb->attachment[i]);
		if (rdb->repeater_shared_buf.bufs[i])
			dma_buf_put(rdb->repeater_shared_buf.bufs[i]);
	}
err_request_buf:
	wb->rdb.repeater_buf_active = false;
	return;
}

static int acquire_repeater_buffer(struct writeback_device *wb,
	struct exynos_drm_writeback_state *wb_state)
{
	int ret = 0;

	get_repeater_buffer(wb, wb_state);
	if (wb->rdb.repeater_buf_active &&
		wb_state->use_repeater_buffer == true) {
		if (repeater_get_valid_buffer(&wb->rdb.repeater_valid_buf_idx) < 0) {
			wb_err(wb, "failed to get valid repeater buffer idx(%d)\n",
					wb->rdb.repeater_valid_buf_idx);
			repeater_set_valid_buffer(wb->rdb.repeater_valid_buf_idx, -1);
			ret = -EINVAL;
			goto err;
		} else {
			wb_state->repeater_valid_buf_idx = wb->rdb.repeater_valid_buf_idx;
			wb->rdb.repeater_valid_buf_idx++;
			if (wb->rdb.repeater_valid_buf_idx >= MAX_SHARED_BUF_NUM)
				wb->rdb.repeater_valid_buf_idx = 0;
		}
	}
err:
	return ret;
}

static int writeback_prepare_job(struct drm_writeback_connector *connector,
				     struct drm_writeback_job *job)
{
	struct writeback_device *wb = conn_to_wb_dev(&connector->base);
	struct drm_connector_state *state = job->priv;
	struct exynos_drm_writeback_state *wb_state = to_exynos_wb_state(state);

	return acquire_repeater_buffer(wb, wb_state);
}

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
static void set_protection(struct writeback_device *wb, uint64_t modifier)
{
	bool protection;
	u32 protection_id;
	int ret = 0;

	protection = (modifier & DRM_FORMAT_MOD_PROTECTION) != 0;
	if (wb->protection == protection)
		return;

	protection_id = PROT_WB0;

	ret = exynos_smc(SMC_PROTECTION_SET, 0, protection_id,
			(protection ? SMC_PROTECTION_ENABLE :
			SMC_PROTECTION_DISABLE));
	if (ret) {
		wb_err(wb, "failed to %s protection(ret:%d)\n",
				protection ? "enable" : "disable", ret);
		return;
	}
	wb->protection = protection;

	wb_debug(wb, "%s: en:%d\n", __func__, protection);

	return;
}
#else
static inline void
set_protection(struct writeback_device *wb, uint64_t modifier) { }
#endif

static void writeback_atomic_commit(struct drm_connector *connector,
		struct drm_connector_state *state)
{
	struct drm_writeback_connector *wb_conn = conn_to_wb_conn(connector);
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(state->crtc);
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct exynos_drm_writeback_state *wb_state = to_exynos_wb_state(state);
	struct dpp_params_info *config = &wb->win_config;

	if (wb->state == WB_STATE_OFF) {
		wb_info(wb, "disabled(%d), can't support commit\n", wb->state);
		return;
	}

	wb_debug(wb, "%s +\n", __func__);

	wb_convert_connector_state_to_config(config, to_exynos_wb_state(state));
	set_protection(wb, state->writeback_job->fb->modifier);
	dpp_reg_configure_params(wb->id, config, wb->attr);
	if (wb_state->type == EXYNOS_WB_SWB) {
		const struct drm_framebuffer *fb;

		fb = state->writeback_job->fb;
		decon_reg_config_wb_size(wb->decon_id, fb->height, fb->width);
	}
	drm_writeback_queue_job(wb_conn, state);

	DPU_EVENT_LOG(DPU_EVT_WB_ATOMIC_COMMIT, exynos_crtc,
			&exynos_crtc->bts->wb_config);

	wb_debug(wb, "%s -\n", __func__);
}

static const struct drm_connector_helper_funcs wb_connector_helper_funcs = {
	.get_modes = writeback_get_modes,
	.atomic_commit = writeback_atomic_commit,
	.prepare_writeback_job = writeback_prepare_job,
};

struct drm_connector_state *
exynos_drm_writeback_duplicate_state(struct drm_connector *connector)
{
	struct exynos_drm_writeback_state *exynos_state;
	struct exynos_drm_writeback_state *copy;

	exynos_state = to_exynos_wb_state(connector->state);
	copy = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_state, sizeof(*exynos_state));
	copy->type = EXYNOS_WB_NONE;
	__drm_atomic_helper_connector_duplicate_state(connector, &copy->base);

	return &copy->base;
}

static void exynos_drm_writeback_destroy_state(struct drm_connector *connector,
		struct drm_connector_state *old_state)
{
	struct exynos_drm_writeback_state *old_exynos_state =
						to_exynos_wb_state(old_state);

	__drm_atomic_helper_connector_destroy_state(old_state);
	kfree(old_exynos_state);
}

static void exynos_drm_writeback_reset(struct drm_connector *connector)
{
	struct exynos_drm_writeback_state *exynos_state;

	if (connector->state) {
		exynos_drm_writeback_destroy_state(connector, connector->state);
		connector->state = NULL;
	}

	exynos_state = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (exynos_state) {
		connector->state = &exynos_state->base;
		connector->state->connector = connector;
		/*
		 * If it needs to initialize the specific property value,
		 * please add to here.
		 */
	}
}

static void exynos_drm_writeback_destroy(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);

	drm_connector_cleanup(connector);
	kfree(wb);
}

static int exynos_drm_writeback_set_property(struct drm_connector *connector,
				struct drm_connector_state *state,
				struct drm_property *property, uint64_t val)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct exynos_drm_writeback_state *exynos_state =
						to_exynos_wb_state(state);

	if (property == wb->props.standard)
		exynos_state->standard = val;
	else if (property == wb->props.range)
		exynos_state->range = val;
	else if (property == wb->props.use_repeater_buffer)
		exynos_state->use_repeater_buffer = val;
	else
		return -EINVAL;

	return 0;
}

static int exynos_drm_writeback_get_property(struct drm_connector *connector,
				const struct drm_connector_state *state,
				struct drm_property *property, uint64_t *val)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct exynos_drm_writeback_state *exynos_state =
						to_exynos_wb_state(state);

	if (property == wb->props.restriction)
		*val = exynos_state->blob_id_restriction;
	else if (property == wb->props.standard)
		*val = exynos_state->standard;
	else if (property == wb->props.range)
		*val = exynos_state->range;
	else if (property == wb->props.use_repeater_buffer)
		*val = exynos_state->use_repeater_buffer;
	else
		return -EINVAL;

	return 0;
}

const char *
writeback_get_type_name(enum exynos_drm_writeback_type type)
{
	static const char * const writeback_type_name[] = {
		[EXYNOS_WB_NONE] = "None",
		[EXYNOS_WB_CWB] = "Concurrent",
		[EXYNOS_WB_SWB] = "Stand_alone",
	};

	if (WARN_ON(type >= ARRAY_SIZE(writeback_type_name)))
		return "Unknown";

	return writeback_type_name[type];
}

static void exynos_drm_writeback_print_state(struct drm_printer *p,
		const struct drm_connector_state *state)
{
	const struct exynos_drm_writeback_state *wb_state =
		to_exynos_wb_state(state);
	const struct writeback_device *wb = conn_to_wb_dev(state->connector);

	drm_printf(p, "\trepeater_buffer: en=%d idx=%d\n",
		wb_state->use_repeater_buffer, wb_state->repeater_valid_buf_idx);
	drm_printf(p, "\ttype: %s\n", writeback_get_type_name(wb_state->type));

	drm_printf(p, "\tDPP #%d", wb->id);
	if (wb->state == WB_STATE_OFF) {
		drm_printf(p, " (off)\n");
		return;
	}

	drm_printf(p, "\n\t\tport=%d\n", wb->port);
	drm_printf(p, "\t\tdecon_id=%d\n", wb->decon_id);
	drm_printf(p, "\t\tprotection=%d\n", wb->protection);
}

static const struct drm_connector_funcs wb_connector_funcs = {
	.reset = exynos_drm_writeback_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = exynos_drm_writeback_destroy,
	.atomic_duplicate_state = exynos_drm_writeback_duplicate_state,
	.atomic_destroy_state = exynos_drm_writeback_destroy_state,
	.atomic_set_property = exynos_drm_writeback_set_property,
	.atomic_get_property = exynos_drm_writeback_get_property,
	.atomic_print_state = exynos_drm_writeback_print_state,
};

static void _writeback_enable(struct writeback_device *wb)
{
	exynos_dpuf_set_ring_clk(wb->votf, true);
	dpp_reg_init(wb->id, wb->attr);
	enable_irq(wb->odma_irq);
}

static void writeback_atomic_enable(struct drm_encoder *encoder,
		struct drm_atomic_state *state)
{
	struct writeback_device *wb = enc_to_wb_dev(encoder);
	const struct decon_device *decon;

	if (wb->state == WB_STATE_ON) {
		wb_info(wb, "already enabled(%d)\n", wb->state);
		return;
	}

	wb_debug(wb, "%s +\n", __func__);

	_writeback_enable(wb);
	decon = wb_get_decon(wb);
	wb->decon_id = decon->id;
	wb->state = WB_STATE_ON;
	DPU_EVENT_LOG(DPU_EVT_WB_ENABLE, decon->crtc, wb);

	wb_debug(wb, "%s -\n", __func__);
}

void writeback_exit_hibernation(struct writeback_device *wb)
{
	struct exynos_drm_crtc *exynos_crtc;

	if (wb->state != WB_STATE_HIBERNATION)
		return;

	_writeback_enable(wb);
	wb->state = WB_STATE_ON;
	exynos_crtc = wb_get_exynos_crtc(wb);
	DPU_EVENT_LOG(DPU_EVT_WB_EXIT_HIBERNATION, exynos_crtc, wb);
}

static void _writeback_disable(struct writeback_device *wb)
{
	disable_irq(wb->odma_irq);
	exynos_dpuf_set_ring_clk(wb->votf, false);
	dpp_reg_deinit(wb->id, false, wb->attr);
}

static void put_repeater_buffer(struct writeback_device *wb)
{
	struct repeater_dma_buf *rdb = &wb->rdb;
	int i;
	if (wb->rdb.votf_o_feature_enabled == false)
		return;
	if (rdb->repeater_buf_active == true) {
		for (i = 0; i < MAX_SHARED_BUF_NUM; i++) {
			if (!IS_ERR_OR_NULL(rdb->attachment[i]) && !IS_ERR_OR_NULL(rdb->sg_table[i]))
				dma_buf_unmap_attachment(rdb->attachment[i],
						rdb->sg_table[i], DMA_TO_DEVICE);
			if (rdb->repeater_shared_buf.bufs[i] && !IS_ERR_OR_NULL(rdb->attachment[i]))
				dma_buf_detach(rdb->repeater_shared_buf.bufs[i],
						rdb->attachment[i]);
			if (rdb->repeater_shared_buf.bufs[i])
				dma_buf_put(rdb->repeater_shared_buf.bufs[i]);
		}
		wb->rdb.repeater_buf_active = false;
	}
}

static void release_repeater_buffer(struct writeback_device *wb)
{
	put_repeater_buffer(wb);
}

static void writeback_atomic_disable(struct drm_encoder *encoder,
			       struct drm_atomic_state *state)
{
	struct writeback_device *wb = enc_to_wb_dev(encoder);
	struct drm_connector_state *old_conn_state;
	struct exynos_drm_crtc *exynos_crtc;

	old_conn_state = drm_atomic_get_old_connector_state(state,
			&wb->writeback.base);

	wb_debug(wb, "%s +\n", __func__);

	if (wb->state == WB_STATE_OFF) {
		wb_info(wb, "already disabled(%d)\n", wb->state);
		return;
	}

	_writeback_disable(wb);
	set_protection(wb, 0);
	release_repeater_buffer(wb);
	wb->state = WB_STATE_OFF;
	exynos_crtc = to_exynos_crtc(old_conn_state->crtc);
	DPU_EVENT_LOG(DPU_EVT_WB_DISABLE, exynos_crtc, wb);

	wb->decon_id = -1;

	wb_debug(wb, "%s -\n", __func__);
}

void writeback_enter_hibernation(struct writeback_device *wb)
{
	struct exynos_drm_crtc *exynos_crtc;

	if (wb->state != WB_STATE_ON)
		return;

	_writeback_disable(wb);
	wb->state = WB_STATE_HIBERNATION;
	exynos_crtc = wb_get_exynos_crtc(wb);
	DPU_EVENT_LOG(DPU_EVT_WB_ENTER_HIBERNATION, exynos_crtc, wb);
}

static const struct drm_encoder_helper_funcs wb_encoder_helper_funcs = {
	.atomic_enable = writeback_atomic_enable,
	.atomic_disable = writeback_atomic_disable,
	.atomic_check = writeback_atomic_check,
};

static int
exynos_drm_wb_conn_create_standard_property(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct drm_property *prop;

	static const struct drm_prop_enum_list standard_list[] = {
		{ EXYNOS_STANDARD_UNSPECIFIED, "Unspecified" },
		{ EXYNOS_STANDARD_BT709, "BT709" },
		{ EXYNOS_STANDARD_BT601_625, "BT601_625" },
		{ EXYNOS_STANDARD_BT601_625_UNADJUSTED, "BT601_625_UNADJUSTED"},
		{ EXYNOS_STANDARD_BT601_525, "BT601_525" },
		{ EXYNOS_STANDARD_BT601_525_UNADJUSTED, "BT601_525_UNADJUSTED"},
		{ EXYNOS_STANDARD_BT2020, "BT2020" },
		{ EXYNOS_STANDARD_BT2020_CONSTANT_LUMINANCE,
						"BT2020_CONSTANT_LUMINANCE"},
		{ EXYNOS_STANDARD_BT470M, "BT470M" },
		{ EXYNOS_STANDARD_FILM, "FILM" },
		{ EXYNOS_STANDARD_DCI_P3, "DCI-P3" },
		{ EXYNOS_STANDARD_ADOBE_RGB, "Adobe RGB" },
	};

	prop = drm_property_create_enum(connector->dev, 0, "standard",
				standard_list, ARRAY_SIZE(standard_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop,
				EXYNOS_STANDARD_UNSPECIFIED);
	wb->props.standard = prop;

	return 0;
}

static int
exynos_drm_wb_conn_create_range_property(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct drm_property *prop;
	static const struct drm_prop_enum_list range_list[] = {
		{ EXYNOS_RANGE_UNSPECIFIED, "Unspecified" },
		{ EXYNOS_RANGE_FULL, "Full" },
		{ EXYNOS_RANGE_LIMITED, "Limited" },
		{ EXYNOS_RANGE_EXTENDED, "Extended" },
	};

	prop = drm_property_create_enum(connector->dev, 0, "range", range_list,
						ARRAY_SIZE(range_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop,
				EXYNOS_RANGE_UNSPECIFIED);
	wb->props.range = prop;

	return 0;
}

static int
exynos_drm_wb_conn_create_restriction_property(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct drm_property *prop;
	struct drm_property_blob *blob;
	struct dpp_ch_restriction res;

	memcpy(&res.restriction, &wb->restriction,
				sizeof(struct dpp_restriction));
	res.id = wb->id;
	res.attr = wb->attr;

	blob = drm_property_create_blob(connector->dev, sizeof(res), &res);
	if (IS_ERR(blob))
		return PTR_ERR(blob);

	prop = drm_property_create(connector->dev,
				   DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
				   "hw restrictions", 0);
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop, blob->base.id);
	wb->props.restriction = prop;

	return 0;
}

static int
exynos_drm_wb_conn_create_use_repeater_buffer_property(struct drm_connector *connector)
{
	struct writeback_device *wb = conn_to_wb_dev(connector);
	struct drm_property *prop;

	prop = drm_property_create_bool(connector->dev, 0, "WRITEBACK_USE_REPEATER_BUFFER");
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&connector->base, prop, false);
	wb->props.use_repeater_buffer = prop;

	return 0;
}

static int writeback_bind(struct device *dev, struct device *master, void *data)
{
	struct writeback_device *wb = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct drm_connector *connector = &wb->writeback.base;
	int ret;

	wb_info(wb, "%s +\n", __func__);

	drm_connector_helper_add(connector, &wb_connector_helper_funcs);
	ret = drm_writeback_connector_init(drm_dev, &wb->writeback,
			&wb_connector_funcs, &wb_encoder_helper_funcs,
			wb->pixel_formats, wb->num_pixel_formats);
	if (ret) {
		wb_err(wb, "failed to init writeback connector\n");
		return ret;
	}

	exynos_drm_wb_conn_create_standard_property(connector);
	exynos_drm_wb_conn_create_range_property(connector);
	exynos_drm_wb_conn_create_restriction_property(connector);
	if (wb->rdb.votf_o_feature_enabled)
		exynos_drm_wb_conn_create_use_repeater_buffer_property(connector);

	wb_info(wb, "%s -\n", __func__);

	return 0;
}

static void
writeback_unbind(struct device *dev, struct device *master, void *data)
{
	const struct writeback_device *wb = dev_get_drvdata(dev);

	wb_debug(wb, "%s +\n", __func__);
	wb_debug(wb, "%s -\n", __func__);
}

static const struct component_ops exynos_wb_component_ops = {
	.bind	= writeback_bind,
	.unbind	= writeback_unbind,
};

static int exynos_wb_parse_dt(struct writeback_device *wb,
				struct device_node *np)
{
	int dpuf_id, ret;
	struct dpp_restriction *res = &wb->restriction;

	wb_debug(wb, "%s +\n", __func__);

	ret = of_property_read_u32(np, "dpp,id", &wb->id);
	if (ret)
		return ret;

	of_property_read_u32(np, "attr", (u32 *)&wb->attr);
	of_property_read_u32(np, "port", &wb->port);

	wb->pixel_formats = writeback_formats;
	wb->num_pixel_formats = ARRAY_SIZE(writeback_formats);

	of_property_read_u32(np, "scale_down", (u32 *)&res->scale_down);
	of_property_read_u32(np, "scale_up", (u32 *)&res->scale_up);

	wb_info(wb, "attr(0x%lx), port(%d)\n", wb->attr, wb->port);

	wb->rdb.votf_o_feature_enabled = of_property_read_bool(np, "votf_o,enabled");
	wb->rdb.repeater_buf_active = false;

	if (!of_property_read_u32(np, "dpuf,id", &dpuf_id))
		wb->votf = dpuf_blkdata[dpuf_id];

	dpp_print_restriction(wb->id, &wb->restriction);

	wb_debug(wb, "%s -\n", __func__);

	return ret;
}

static irqreturn_t odma_irq_handler(int irq, void *priv)
{
	struct writeback_device *wb = priv;
	const struct drm_connector_state *conn_state = wb->writeback.base.state;
	const struct exynos_drm_writeback_state *wb_state =
				to_exynos_wb_state(conn_state);
	u32 irqs;

	spin_lock(&wb->odma_slock);
	if (wb->state == WB_STATE_OFF)
		goto irq_end;

	irqs = odma_reg_get_irq_and_clear(wb->id);

	if (irqs & ODMA_STATUS_FRAMEDONE_IRQ || irqs & ODMA_INST_OFF_DONE_IRQ) {
		struct exynos_drm_crtc *exynos_crtc =
			to_exynos_crtc(conn_state->crtc);

		if (irqs & ODMA_STATUS_FRAMEDONE_IRQ)
			wb_debug(wb, "framedone irq occurs\n");
		else
			wb_warn(wb, "instant off irq occurs\n");

		if (wb_state->type == EXYNOS_WB_CWB)
			decon_reg_set_cwb_enable(wb->decon_id, false);

		drm_writeback_signal_completion(&wb->writeback, 0);
		DPU_EVENT_LOG(DPU_EVT_WB_FRAMEDONE, exynos_crtc, wb);
	}

	if ((irqs & ODMA_WRITE_SLAVE_ERROR) || (irqs & ODMA_STATUS_DEADLOCK_IRQ)) {
		wb_err(wb, "error irq occur(0x%x)\n", irqs);
		wb_dump(wb);
		goto irq_end;
	}

irq_end:
	spin_unlock(&wb->odma_slock);
	return IRQ_HANDLED;
}

static int wb_init_resources(struct writeback_device *wb)
{
	int ret = 0;
	struct device *dev = wb->dev;
	struct device_node *np = dev->of_node;
	struct platform_device *pdev;
	struct resource *res;
	int i;

	wb_debug(wb, "%s +\n", __func__);

	pdev = container_of(dev, struct platform_device, dev);

	i = of_property_match_string(np, "reg-names", "dma");
	wb->regs.dma_base_regs = of_iomap(np, i);
	if (!wb->regs.dma_base_regs) {
		wb_err(wb, "failed to remap DPU_DMA SFR region\n");
		return -EINVAL;
	}
	dpp_regs_desc_init(wb->regs.dma_base_regs, "dma", REGS_DMA, wb->id);

	wb->odma_irq = of_irq_get_byname(np, "dma");
	wb_info(wb, "dma irq no = %lld\n", wb->odma_irq);
	ret = devm_request_irq(dev, wb->odma_irq, odma_irq_handler, 0,
			pdev->name, wb);
	if (ret) {
		wb_err(wb, "failed to install DPU DMA irq\n");
		return -EINVAL;
	}
	disable_irq(wb->odma_irq);

	if (test_bit(DPP_ATTR_DPP, &wb->attr)) {
		i = of_property_match_string(np, "reg-names", "dpp");
		wb->regs.dpp_base_regs = of_iomap(np, i);
		if (!wb->regs.dpp_base_regs) {
			wb_err(wb, "failed to remap DPP SFR region\n");
			return -EINVAL;
		}
		dpp_regs_desc_init(wb->regs.dpp_base_regs, "dpp", REGS_DPP,
				wb->id);
	}

	i = of_property_match_string(np, "reg-names", "votf");
	if (i >= 0) {
		wb->regs.votf_base_regs = of_iomap(np, i);
		if (!wb->regs.votf_base_regs) {
			wb_err(wb, "failed to remap VOTF SFR region\n");
			return -EINVAL;
		}
		dpp_regs_desc_init(wb->regs.votf_base_regs, "votf", REGS_VOTF, wb->id);
	}

	/* vOTF_MFC SFR 1:1 mapping */
	i = of_property_match_string(np, "reg-names", "votf_mfc");
	if (i >= 0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		wb->win_config.votf_o_mfc_addr = res->start;
		ret = iommu_map(iommu_get_domain_for_dev(wb->dev),
				res->start, res->start,
				res->end - res->start + 1, 0);
		if (ret) {
			wb_err(wb, "failed to iommu_map MFC votf SFR region\n");
			return -EINVAL;
		}

		ret = iommu_dma_reserve_iova(wb->dev, res->start,
				res->end - res->start + 1);
		if (ret) {
			wb_err(wb, "failed to dma_reserve MFC votf SFR region\n");
			return -EINVAL;
		}
	}

	wb_debug(wb, "%s -\n", __func__);

	return ret;
}

static int writeback_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct writeback_device *writeback;
	const struct dpp_restriction *restriction;

	writeback = devm_kzalloc(dev, sizeof(struct writeback_device),
			GFP_KERNEL);
	if (!writeback)
		return -ENOMEM;

	restriction = of_device_get_match_data(dev);
	memcpy(&writeback->restriction, restriction, sizeof(*restriction));

	writeback->dev = dev;
	ret = exynos_wb_parse_dt(writeback, dev->of_node);
	if (ret)
		goto fail;

	writeback->output_type = EXYNOS_DISPLAY_TYPE_VIDI;

	spin_lock_init(&writeback->odma_slock);

	writeback->state = WB_STATE_OFF;

	ret = wb_init_resources(writeback);
	if (ret)
		goto fail;

	/* dpp is not connected decon now */
	writeback->decon_id = -1;

	platform_set_drvdata(pdev, writeback);

	wb_info(writeback, "successfully probed\n");

	ret = component_add(dev, &exynos_wb_component_ops);
	if (ret)
		goto fail;

	return ret;

fail:
	wb_err(writeback, "probe failed\n");
	return ret;
}

static int writeback_remove(struct platform_device *pdev)
{
	struct writeback_device *wb = platform_get_drvdata(pdev);

	component_del(&pdev->dev, &exynos_wb_component_ops);

	if (test_bit(DPP_ATTR_DPP, &wb->attr))
		iounmap(wb->regs.dpp_base_regs);
	iounmap(wb->regs.dma_base_regs);

	return 0;
}

struct platform_driver writeback_driver = {
	.probe = writeback_probe,
	.remove = writeback_remove,
	.driver = {
		   .name = "exynos-writeback",
		   .owner = THIS_MODULE,
		   .of_match_table = wb_of_match,
	},
};

MODULE_AUTHOR("Wonyeong Choi <won0.choi@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC Display WriteBack");
MODULE_LICENSE("GPL v2");
