// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_fb.c
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

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>

#include <dpu_trace.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_fbdev.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_format.h>
#include <exynos_drm_gem.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_modifier.h>
#include <exynos_drm_writeback.h>
#include <exynos_drm_freq_hop.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_helper.h>
#endif

static const struct drm_framebuffer_funcs exynos_drm_fb_funcs = {
	.destroy	= drm_gem_fb_destroy,
	.create_handle	= drm_gem_fb_create_handle,
};

struct drm_framebuffer *
exynos_drm_framebuffer_init(struct drm_device *dev,
			    const struct drm_mode_fb_cmd2 *mode_cmd,
			    struct drm_gem_object **obj,
			    int count)
{
	struct drm_framebuffer *fb;
	int i;
	int ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++)
		fb->obj[i] = obj[i];

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	ret = drm_framebuffer_init(dev, fb, &exynos_drm_fb_funcs);
	if (ret < 0) {
		DRM_ERROR("failed to initialize framebuffer\n");
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}

static size_t get_plane_size(const struct drm_mode_fb_cmd2 *mode_cmd, u32 idx,
		const struct drm_format_info *info)
{
	u32 height;
	size_t size = 0;
	bool is_10bpc;
	bool is_sbwc = mode_cmd->modifier[idx] & SBWC_IDENTIFIER;
	bool is_lossy = mode_cmd->modifier[idx] & SBWC_FORMAT_MOD_LOSSY;

	if (is_sbwc && is_lossy) {
		int mul;
		int blk_32x;

		is_10bpc = IS_10BPC(dpu_find_fmt_info(mode_cmd->pixel_format));
		mul = is_10bpc ? 20 : 25;
		blk_32x = SBWC_BLK_BYTE_GET(mode_cmd->modifier[idx]);

		/*
		 * mapping size[0] : luminance PL
		 * mapping size[1] : chrominance PL
		 */
		if (idx == 0)
			size = Y_SIZE_SBWCL(mode_cmd->width, mode_cmd->height,
					mul * blk_32x, is_10bpc);
		else if (idx == 1)
			size = UV_SIZE_SBWCL(mode_cmd->width, mode_cmd->height,
					mul * blk_32x, is_10bpc);
	} else if (is_sbwc) {
		is_10bpc = IS_10BPC(dpu_find_fmt_info(mode_cmd->pixel_format));

		/*
		 * mapping size[0] : luminance PL/HD
		 * mapping size[1] : chrominance PL/HD
		 */
		if (idx == 0)
			size = Y_SIZE_SBWC(mode_cmd->width, mode_cmd->height,
					is_10bpc);
		else if (idx == 1)
			size = UV_SIZE_SBWC(mode_cmd->width, mode_cmd->height,
					is_10bpc);
	} else if (mode_cmd->modifier[idx] & SAJC_IDENTIFIER) {
		if (idx == 0)
			size = mode_cmd->height * mode_cmd->pitches[idx];
		else if (idx == 1)
			/* TODO: use macro */
			size = mode_cmd->pitches[idx] * 2 ;
	} else {
		height = (idx == 0) ? mode_cmd->height :
			DIV_ROUND_UP(mode_cmd->height, info->vsub);
		size = height * mode_cmd->pitches[idx];
	}

	return size;
}

static struct drm_framebuffer *
exynos_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info = drm_get_format_info(dev, mode_cmd);
	struct drm_gem_object *obj[MAX_FB_BUFFER] = { 0 };
	struct drm_framebuffer *fb = ERR_PTR(-ENOMEM);
	struct drm_format_name_buf format_name;
	size_t size;
	int i;
	int ret = 0;

	DRM_DEBUG("%s +\n", __func__);

	DPU_ATRACE_BEGIN("exynos_fb_create");
	if (unlikely(info->num_planes > MAX_FB_BUFFER)) {
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < info->num_planes; i++) {
		if (mode_cmd->modifier[i] ==
				DRM_FORMAT_MOD_SAMSUNG_COLORMAP) {
			struct exynos_drm_gem *exynos_gem;

			exynos_gem = exynos_drm_gem_alloc(dev, 0,
						EXYNOS_DRM_GEM_FLAG_COLORMAP);
			if (IS_ERR(exynos_gem)) {
				DRM_ERROR("failed to create colormap gem\n");
				ret = PTR_ERR(exynos_gem);
				goto err_gem;
			}

			obj[i] = &exynos_gem->base;
			continue;
		}

		obj[i] = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj[i]) {
			DRM_ERROR("failed to lookup gem object\n");
			ret = -ENOENT;
			goto err_gem;
		}

		size = get_plane_size(mode_cmd, i, info);
		if (!size || mode_cmd->offsets[i] + size > obj[i]->size) {
			DRM_ERROR("offsets[%d](%d) + size(%zu) > obj_size[%d](%zu)\n",
				i, mode_cmd->offsets[i], size, i, obj[i]->size);
			DRM_ERROR("mod(%llx)\n", mode_cmd->modifier[i]);
			i++;
			ret = -EINVAL;
			goto err_gem;
		}
		DRM_DEBUG("[plane:%d] handle(0x%x), offset(%d) pitches(%d),\
				size(%lu)\n", i, mode_cmd->handles[i],
				mode_cmd->offsets[i], mode_cmd->pitches[i], size);
	}
	DRM_DEBUG("width(%d), height(%d), mod(0x%llx) format(%s) \n",
			mode_cmd->width, mode_cmd->height, mode_cmd->modifier[0],
			drm_get_format_name(mode_cmd->pixel_format, &format_name));

	fb = exynos_drm_framebuffer_init(dev, mode_cmd, obj, i);
	if (IS_ERR(fb)) {
		ret = PTR_ERR(fb);
		goto err_gem;
	}

err_gem:
	if (ret) {
		while (i--)
			drm_gem_object_put(obj[i]);
	}
err:
	DPU_ATRACE_END("exynos_fb_create");

	return ret ? ERR_PTR(ret) : fb;
}

static const struct drm_format_info *
exynos_get_format_info(const struct drm_mode_fb_cmd2 *cmd)
{
	struct drm_format_name_buf n;
	const char *format_name;
	const struct drm_format_info *info = NULL;

	if (cmd->modifier[0] == DRM_FORMAT_MOD_SAMSUNG_COLORMAP) {
		info = drm_format_info(cmd->pixel_format);
		if (info->format == DRM_FORMAT_BGRA8888)
			return info;

		format_name = drm_get_format_name(cmd->pixel_format, &n);
		DRM_WARN("%s is not proper format for colormap\n", format_name);
	} else if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0),	cmd->modifier[0])) {
		int i;
		/* SAJC RGB format need 2-planes for header, payload */
		static const struct drm_format_info exynos_infos[] = {
			{ .format = DRM_FORMAT_ARGB8888,	.depth = 32, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR8888,	.depth = 32, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_XRGB8888,	.depth = 24, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_XBGR8888,	.depth = 24, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_RGB565,		.depth = 16, .num_planes = 2, .cpp = { 2, 0, 0 }, .hsub = 1, .vsub = 1 },
			{ .format = DRM_FORMAT_ARGB2101010,	.depth = 30, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR2101010,	.depth = 30, .num_planes = 2, .cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
			{ .format = DRM_FORMAT_ABGR16161616F,	.depth = 0,  .num_planes = 2, .cpp = { 8, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
		};
		for (i = 0; i < ARRAY_SIZE(exynos_infos); ++i) {
			if (exynos_infos[i].format == cmd->pixel_format)
				return &exynos_infos[i];
		}

		format_name = drm_get_format_name(cmd->pixel_format, &n);
		DRM_WARN("%s is not proper format for SAJC\n", format_name);
	}

	return NULL;
}

dma_addr_t exynos_drm_fb_dma_addr(const struct drm_framebuffer *fb, int index)
{
	const struct exynos_drm_gem *exynos_gem;

	if (WARN_ON_ONCE(index >= MAX_FB_BUFFER) || !fb->obj[index])
		return 0;

	exynos_gem = to_exynos_gem(fb->obj[index]);

	DRM_DEBUG("%s: fb(%d) dma_addr[%d] = %#llx (+%#x)\n", __func__,
		fb->base.id, index, exynos_gem->dma_addr, fb->offsets[index]);

	return exynos_gem->dma_addr + fb->offsets[index];
}

static void plane_state_to_win_config(struct dpu_bts_win_config *win_config,
				      const struct drm_plane_state *plane_state)
{
	const struct drm_framebuffer *fb = plane_state->fb;
	unsigned int simplified_rot;
	struct exynos_drm_plane_state *exynos_state =
				to_exynos_plane_state(plane_state);


	win_config->src_x = plane_state->src.x1 >> 16;
	win_config->src_y = plane_state->src.y1 >> 16;
	win_config->src_w = drm_rect_width(&plane_state->src) >> 16;
	win_config->src_h = drm_rect_height(&plane_state->src) >> 16;
	win_config->src_f_w = fb->width;
	win_config->src_f_h = fb->height;

	win_config->dst_x = plane_state->dst.x1;
	win_config->dst_y = plane_state->dst.y1;
	win_config->dst_w = drm_rect_width(&plane_state->dst);
	win_config->dst_h = drm_rect_height(&plane_state->dst);

	win_config->dbg_dma_addr = exynos_drm_fb_dma_addr(plane_state->fb, 0);

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0), fb->modifier) ||
		has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), fb->modifier) ||
			has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(0, 0),
				fb->modifier))
		win_config->is_comp = true;
	else
		win_config->is_comp = false;

	if (exynos_drm_fb_is_colormap(fb))
		win_config->state = DPU_WIN_STATE_COLOR;
	else
		win_config->state = DPU_WIN_STATE_BUFFER;

	win_config->format = fb->format->format;
	win_config->dpp_ch = plane_state->plane->index;

	win_config->comp_src = 0;
	if (has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), fb->modifier))
		win_config->comp_src =
			(fb->modifier & AFBC_FORMAT_MOD_SOURCE_MASK);

	simplified_rot = drm_rotation_simplify(plane_state->rotation,
			DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
			DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	win_config->is_rot = false;
	win_config->is_xflip = false;
	if (simplified_rot & DRM_MODE_ROTATE_90)
		win_config->is_rot = true;

	if (simplified_rot & DRM_MODE_REFLECT_X)
		win_config->is_xflip = true;

	win_config->is_hdr = exynos_state->hdr_en;

	DRM_DEBUG("src[%d %d %d %d], dst[%d %d %d %d]\n",
			win_config->src_x, win_config->src_y,
			win_config->src_w, win_config->src_h,
			win_config->dst_x, win_config->dst_y,
			win_config->dst_w, win_config->dst_h);
	DRM_DEBUG("rot[%d] afbc[%d] format[%d] ch[%d] zpos[%d] comp_src[%#llx] hdr[%d]\n",
			win_config->is_rot, win_config->is_comp,
			win_config->format, win_config->dpp_ch,
			plane_state->normalized_zpos,
			win_config->comp_src, win_config->is_hdr);
	DRM_DEBUG("alpha[%d] blend mode[%d]\n",
			plane_state->alpha, plane_state->pixel_blend_mode);
	DRM_DEBUG("simplified rot[0x%x]\n", simplified_rot);
}

static void conn_state_to_win_config(struct dpu_bts_win_config *win_config,
				const struct drm_connector_state *conn_state)
{
	const struct writeback_device *wb = conn_to_wb_dev(conn_state->connector);
	const struct drm_framebuffer *fb = conn_state->writeback_job->fb;

	win_config->src_x = 0;
	win_config->src_y = 0;
	win_config->src_w = fb->width;
	win_config->src_h = fb->height;
	win_config->dst_x = 0;
	win_config->dst_y = 0;
	win_config->dst_w = fb->width;
	win_config->dst_h = fb->height;

	win_config->dbg_dma_addr = exynos_drm_fb_dma_addr(fb, 0);

	win_config->is_comp = false;
	win_config->state = DPU_WIN_STATE_BUFFER;
	win_config->format = fb->format->format;
	win_config->dpp_ch = wb->id;
	win_config->comp_src = 0;
	win_config->is_rot = false;

	DRM_DEBUG("src[%d %d %d %d], dst[%d %d %d %d]\n",
			win_config->src_x, win_config->src_y,
			win_config->src_w, win_config->src_h,
			win_config->dst_x, win_config->dst_y,
			win_config->dst_w, win_config->dst_h);
	DRM_DEBUG("rot[%d] afbc[%d] format[%d] ch[%d] comp_src[%#llx]\n",
			win_config->is_rot, win_config->is_comp,
			win_config->format, win_config->dpp_ch,
			win_config->comp_src);
}

static void exynos_atomic_update_conn_bts(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_connector *conn;
	const struct drm_connector_state *old_conn_state, *new_conn_state;
	struct dpu_bts_win_config *win_config;
	int i;

	for_each_oldnew_connector_in_state(old_state, conn, old_conn_state,
					new_conn_state, i) {
		bool new_job;

		if (conn->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;

		new_job = wb_check_job(new_conn_state);

		if (new_job) {
			exynos_crtc = to_exynos_crtc(new_conn_state->crtc);
			win_config = &exynos_crtc->bts->wb_config;
			conn_state_to_win_config(win_config, new_conn_state);
		} else {
			if (!old_conn_state->crtc)
				continue;
			exynos_crtc = to_exynos_crtc(old_conn_state->crtc);
			win_config = &exynos_crtc->bts->wb_config;
			win_config->state = DPU_WIN_STATE_DISABLED;
		}
	}
}

static void exynos_atomic_bts_pre_update(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	const struct drm_crtc_state *new_crtc_state;
	struct drm_plane *plane;
	const struct drm_plane_state *old_plane_state, *new_plane_state;
	struct dpu_bts_win_config *win_config;
	int i;

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS))
		return;

	for_each_oldnew_plane_in_state(old_state, plane, old_plane_state,
				       new_plane_state, i) {
		const int zpos = new_plane_state->normalized_zpos;

		if (new_plane_state->crtc) {
			exynos_crtc = to_exynos_crtc(new_plane_state->crtc);
			win_config = &exynos_crtc->bts->win_config[zpos];
			plane_state_to_win_config(win_config, new_plane_state);
		} else if (old_plane_state->crtc) {
			exynos_crtc = to_exynos_crtc(old_plane_state->crtc);
			win_config = &exynos_crtc->bts->win_config[zpos];
			win_config->dbg_dma_addr = 0;
		}
	}

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc;
		struct exynos_drm_crtc_state *new_exynos_crtc_state =
					to_exynos_crtc_state(new_crtc_state);
		const size_t max_planes = get_plane_num(dev);
		const size_t num_planes = hweight32(new_crtc_state->plane_mask);
		int j;

		exynos_crtc = to_exynos_crtc(crtc);

		if (!new_crtc_state->active)
			continue;

		if (new_crtc_state->planes_changed) {
			for (j = num_planes; j < max_planes; j++) {
				win_config = &exynos_crtc->bts->win_config[j];
				win_config->state = DPU_WIN_STATE_DISABLED;
			}
		}

		if (exynos_crtc->ops->update_bts_fps)
			exynos_crtc->ops->update_bts_fps(exynos_crtc);

		for (j = 0; j < num_planes; j++)
			DPU_EVENT_LOG(j == 0 ? "ATOMIC_COMMIT" : "", exynos_crtc,
					EVENT_FLAG_LONG, dpu_config_to_string,
					&exynos_crtc->bts->win_config[j]);

		/*
		 * In the case of seamless modeset, update the bts
		 * in decon_seamless_set_mode.
		 */
		if (!new_exynos_crtc_state->seamless_modeset) {
			exynos_crtc->bts->ops->calc_bw(exynos_crtc);
			exynos_crtc->bts->ops->update_bw(exynos_crtc, false);
		}
	}
}

static void exynos_atomic_bts_post_update(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	const struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_plane *exynos_plane;
	struct drm_plane *plane;
	const struct dpu_bts_win_config *win_config;
	int i, win_id, win_cnt = get_plane_num(dev);

	if (!IS_ENABLED(CONFIG_EXYNOS_BTS))
		return;

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);

		if (new_crtc_state->planes_changed && new_crtc_state->active) {

			/*
			 * keeping a copy of comp src in dpp after it has been
			 * applied in hardware for debugging purposes.
			 * plane order == dpp channel order
			 */
			drm_for_each_plane(plane, dev) {
				exynos_plane = to_exynos_plane(plane);
				if (exynos_plane->win_id >= win_cnt)
					continue;

				win_id = exynos_plane->win_id;
				win_config =
					&exynos_crtc->bts->win_config[win_id];
				if (win_config->state == DPU_WIN_STATE_BUFFER &&
						win_config->is_comp)
					exynos_plane->comp_src = win_config->comp_src;
			}

			exynos_crtc->bts->ops->update_bw(exynos_crtc, true);
			DPU_EVENT_LOG("DECON_CH_OCCUPANCY", exynos_crtc, EVENT_FLAG_LONG,
					 dpu_rsc_ch_to_string, exynos_crtc);
			DPU_EVENT_LOG("DECON_WIN_OCCUPANCY", exynos_crtc, EVENT_FLAG_LONG,
					 dpu_rsc_win_to_string, exynos_crtc);
		}

		if (!new_crtc_state->active && new_crtc_state->active_changed)
			exynos_crtc->bts->ops->release_bw(exynos_crtc);
	}
}

#define DEFAULT_TIMEOUT_FPS 60
static void exynos_atomic_wait_for_vblanks(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i, ret;
	unsigned crtc_mask = 0;

	/*
	 * Legacy cursor ioctls are completely unsynced, and userspace
	 * relies on that (by doing tons of cursor updates).
	 */
	if (old_state->legacy_cursor_update)
		return;

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		struct exynos_drm_crtc *exynos_crtc;
		if (!new_crtc_state->active || new_crtc_state->no_vblank)
			continue;

		exynos_crtc = to_exynos_crtc(crtc);
		if (exynos_crtc->ops->check_svsync_start)
			if (exynos_crtc->ops->check_svsync_start(exynos_crtc))
				continue;

		ret = drm_crtc_vblank_get(crtc);
		if (ret != 0)
			continue;

		crtc_mask |= drm_crtc_mask(crtc);
		old_state->crtcs[i].last_vblank_count = drm_crtc_vblank_count(crtc);
	}

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		static ktime_t timestamp_s;
		s64 diff_msec;
		u64 fps;
		u32 default_vblank_timeout, max_vblank_timeout, interval;

		if (!(crtc_mask & drm_crtc_mask(crtc)))
			continue;

		fps = drm_mode_vrefresh(&new_crtc_state->mode) ?: DEFAULT_TIMEOUT_FPS;
		interval = MSEC_PER_SEC / fps;
		max_vblank_timeout = 12 * interval;
		default_vblank_timeout = 6 * interval;

		timestamp_s = ktime_get();
		ret = wait_event_timeout(dev->vblank[i].queue,
				old_state->crtcs[i].last_vblank_count !=
				drm_crtc_vblank_count(crtc),
				msecs_to_jiffies(max_vblank_timeout));
		diff_msec = ktime_to_ms(ktime_sub(ktime_get(), timestamp_s));

		if (diff_msec >= default_vblank_timeout)
			pr_warn("[CRTC:%d:%s] vblank wait timed out!(%lld ms/%lld ms)\n",
				crtc->base.id, crtc->name, diff_msec,
				default_vblank_timeout);

		if (!ret) {
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
			/* Code for bypass commit when panel was not connected */
			if (mcd_drm_check_commit_skip(NULL, __func__)) {
				drm_crtc_handle_vblank(crtc);
			} else {
				DPU_EVENT_LOG("VBLANK_TIMEOUT", to_exynos_crtc(crtc),
						EVENT_FLAG_ERROR, NULL);
				dpu_print_eint_state(crtc);
				dpu_check_panel_status(crtc);
			}
#else
			DPU_EVENT_LOG("VBLANK_TIMEOUT", to_exynos_crtc(crtc),
					EVENT_FLAG_ERROR, NULL);
			dpu_print_eint_state(crtc);
			dpu_check_panel_status(crtc);
#endif
		}
		drm_crtc_vblank_put(crtc);
	}
}

static void set_repeater_buffer(struct drm_atomic_state *old_state)
{
	struct writeback_device *wb;
	struct exynos_drm_writeback_state *wb_state;
	struct drm_connector *conn;
	const struct drm_connector_state *old_conn_state, *new_conn_state;
	int i;

	for_each_oldnew_connector_in_state(old_state, conn, old_conn_state,
			new_conn_state, i) {
		if (conn->connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
			continue;
		wb = conn_to_wb_dev(conn);
		wb_state = to_exynos_wb_state(new_conn_state);
		if (wb->rdb.repeater_buf_active == true
				&& wb_state->use_repeater_buffer == true)
			repeater_set_valid_buffer(wb_state->repeater_valid_buf_idx,
					wb_state->repeater_valid_buf_idx);
	}
}

static void
exynos_atomic_framestart_post_processing(struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc_state *new_exynos_crtc_state;
	struct exynos_drm_private *priv;
	int i;

	set_repeater_buffer(old_state);

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);

		if (!new_exynos_crtc_state->freed_win_mask)
			continue;

		priv = old_state->dev->dev_private;

		/* release windows after framestart */
		mutex_lock(&priv->lock);
		priv->available_win_mask |= new_exynos_crtc_state->freed_win_mask;
		mutex_unlock(&priv->lock);
	}
}

static void exynos_atomic_bridge_chain_disable(struct drm_bridge *bridge,
				     struct drm_atomic_state *old_state)
{
	struct drm_encoder *encoder;
	struct drm_bridge *iter;

	if (!bridge)
		return;

	encoder = bridge->encoder;
	list_for_each_entry_reverse(iter, &encoder->bridge_chain, chain_node) {
		if (iter->funcs->disable)
			iter->funcs->disable(iter);

		if (iter == bridge)
			break;
	}
}

void exynos_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	int i;
	struct drm_device *dev = old_state->dev;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	unsigned int hibernation_crtc_mask = 0;
	unsigned int disabling_crtc_mask = 0;

	DPU_ATRACE_BEGIN("exynos_atomic_commit_tail");

	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* to find bridge & exynos_mode */
		struct drm_bridge *bridge;
		struct drm_connector_state *old_conn_state;
		struct drm_encoder *encoder;
		struct drm_connector *connector =
			crtc_get_conn(old_crtc_state, DRM_MODE_CONNECTOR_DSI);
		struct exynos_drm_connector_state *exynos_conn_state;

		exynos_crtc = to_exynos_crtc(crtc);

		DRM_DEBUG("[CRTC-%d] old en:%d active:%d change[p:%d m:%d a:%d c:%d]\n",
				drm_crtc_index(crtc), STATE_ARG(old_crtc_state));

		DRM_DEBUG("[CRTC-%d] new en:%d active:%d change[p:%d m:%d a:%d c:%d]\n",
				drm_crtc_index(crtc), STATE_ARG(new_crtc_state));

		DPU_EVENT_LOG("REQ_CRTC_INFO_OLD", exynos_crtc, 0, STATE_FMT,
				STATE_ARG(old_crtc_state));
		DPU_EVENT_LOG("REQ_CRTC_INFO_NEW", exynos_crtc,	0, STATE_FMT,
				STATE_ARG(new_crtc_state));

		if (new_crtc_state->active || new_crtc_state->active_changed) {
			hibernation_block_exit(exynos_crtc->hibernation);
			hibernation_crtc_mask |= drm_crtc_mask(crtc);
		}

		if (old_crtc_state->active && drm_atomic_crtc_needs_modeset(new_crtc_state)) {
			/* keep runtime vote while disabling is taking place */
			pm_runtime_get_sync(exynos_crtc->dev);
			disabling_crtc_mask |= drm_crtc_mask(crtc);

			DPU_ATRACE_BEGIN("crtc_disable");

			/*
			 * To prevent screen noise when turnning decon off for video mode,
			 * should be disabled panel at first.
			 */
			if (connector != NULL) {
				/* In case DP or WB, connector can be NULL value */
				old_conn_state = drm_atomic_get_old_connector_state(old_state, connector);
				encoder = old_conn_state->best_encoder;
				bridge = drm_bridge_chain_get_first_bridge(encoder);
				exynos_conn_state = to_exynos_connector_state(old_conn_state);
				if (exynos_conn_state->exynos_mode.mode_flags & MIPI_DSI_MODE_VIDEO)
					exynos_atomic_bridge_chain_disable(bridge, old_state);
			}

			funcs = crtc->helper_private;
			if (funcs->atomic_disable)
				funcs->atomic_disable(crtc, old_crtc_state);

			DPU_ATRACE_END("crtc_disable");
		}
	}

	DPU_ATRACE_BEGIN("modeset");
	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	exynos_atomic_update_conn_bts(dev, old_state);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	/* request to change DPHY PLL frequency */
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {

		if (!new_crtc_state->active)
			continue;

		exynos_crtc = to_exynos_crtc(crtc);
		if (exynos_crtc->freq_hop)
			exynos_crtc->freq_hop->set_freq_hop(exynos_crtc, true);
	}

	exynos_atomic_bts_pre_update(dev, old_state);
	DPU_ATRACE_END("modeset");

	DPU_ATRACE_BEGIN("commit_planes");
	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);
	DPU_ATRACE_END("commit_planes");

	/*
	 * hw is flushed at this point, signal flip done for fake commit to
	 * unblock nonblocking atomic commits once vblank occurs
	 */
	if (old_state->fake_commit)
		complete_all(&old_state->fake_commit->flip_done);

	drm_atomic_helper_fake_vblank(old_state);

	DPU_ATRACE_BEGIN("wait_for_vblanks");
	exynos_atomic_wait_for_vblanks(dev, old_state);
	DPU_ATRACE_END("wait_for_vblanks");

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {
#else
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
#endif
		const struct exynos_drm_crtc_ops *ops;
		struct exynos_drm_crtc_state *exynos_crtc_state =
			to_exynos_crtc_state(new_crtc_state);

		exynos_crtc = to_exynos_crtc(crtc);

		if (!new_crtc_state->active)
			continue;

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
		if (exynos_crtc_state->skip_frameupdate &&
				crtc_get_encoder(new_crtc_state, DRM_MODE_ENCODER_DSI)) {
			pr_info("skip_frameupdate\n");
			continue;
		}
#endif
		ops = exynos_crtc->ops;
		DPU_ATRACE_BEGIN("wait_for_frame_start");
		if (ops->wait_framestart)
			ops->wait_framestart(exynos_crtc);
		DPU_ATRACE_END("wait_for_frame_start");

		if (ops->set_trigger)
			ops->set_trigger(exynos_crtc, exynos_crtc_state);

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
		if (ops->set_fingerprint_mask)
			ops->set_fingerprint_mask(exynos_crtc, old_crtc_state, 1);
#endif
	}
	exynos_atomic_framestart_post_processing(old_state);

	exynos_atomic_bts_post_update(dev, old_state);

	/*
	 * After shadow update, changed PLL is applied and
	 * target M value is stored
	 */
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
			new_crtc_state, i) {
		const struct exynos_drm_crtc_ops *ops;

		if (!new_crtc_state->active)
			continue;

		exynos_crtc = to_exynos_crtc(crtc);
		if (exynos_crtc->freq_hop)
			exynos_crtc->freq_hop->set_freq_hop(exynos_crtc, false);
		ops = exynos_crtc->ops;
		if (ops->pll_sleep_mask)
			ops->pll_sleep_mask(exynos_crtc, false);
	}

	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		exynos_crtc = to_exynos_crtc(crtc);
		if (hibernation_crtc_mask & drm_crtc_mask(crtc))
			hibernation_unblock(exynos_crtc->hibernation);
		if (disabling_crtc_mask & drm_crtc_mask(crtc))
			pm_runtime_put_sync(exynos_crtc->dev);
	}

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);

#if IS_ENABLED(CONFIG_EXYNOS_DRM_BUFFER_SANITY_CHECK)
	exynos_atomic_commit_check_buf_sanity(old_state);
#endif

	DPU_ATRACE_END("exynos_atomic_commit_tail");
}

static struct drm_mode_config_helper_funcs exynos_drm_mode_config_helpers = {
	.atomic_commit_tail = exynos_atomic_commit_tail,
};

static const struct drm_mode_config_funcs exynos_drm_mode_config_funcs = {
	.fb_create = exynos_user_fb_create,
	.get_format_info = exynos_get_format_info,
	.output_poll_changed = exynos_drm_output_poll_changed,
	.atomic_check = exynos_atomic_check,
	.atomic_commit = exynos_atomic_commit,
};

void exynos_drm_mode_config_init(struct drm_device *drm_dev)
{
	drm_dev->mode_config.min_width = 0;
	drm_dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(8192x8192).
	 * this value would be used to check the limitation of framebuffer size
	 * and display mode resolution.
	 */
	drm_dev->mode_config.max_width = 8192;
	drm_dev->mode_config.max_height = 8192;
	drm_dev->mode_config.funcs = &exynos_drm_mode_config_funcs;
	drm_dev->mode_config.helper_private = &exynos_drm_mode_config_helpers;

	/* create properties ahead of binding to make them available to all drivers */
	exynos_drm_crtc_create_properties(drm_dev);
	exynos_drm_plane_create_properties(drm_dev);
	exynos_drm_connector_create_properties(drm_dev);
}
