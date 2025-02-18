// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fourcc.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_format.h>
#include <exynos_drm_modifier.h>
#include <exynos_drm_partial.h>

static struct drm_plane_state *
exynos_drm_plane_duplicate_state(struct drm_plane *plane)
{
	struct exynos_drm_plane_state *exynos_state;
	struct exynos_drm_plane_state *copy;

	exynos_state = to_exynos_plane_state(plane->state);
	copy = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_state, sizeof(*exynos_state));
	copy->hdr_en = false;

	__drm_atomic_helper_plane_duplicate_state(plane, &copy->base);
	return &copy->base;
}

static void exynos_drm_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *old_state)
{
	struct exynos_drm_plane_state *old_exynos_state =
					to_exynos_plane_state(old_state);

	__drm_atomic_helper_plane_destroy_state(old_state);
	kfree(old_exynos_state);
}

static void exynos_drm_plane_reset(struct drm_plane *plane)
{
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	struct exynos_drm_plane_state *exynos_state;

	if (plane->state) {
		exynos_drm_plane_destroy_state(plane, plane->state);
		plane->state = NULL;
	}

	exynos_state = kzalloc(sizeof(*exynos_state), GFP_KERNEL);
	if (exynos_state) {
		plane->state = &exynos_state->base;
		plane->state->plane = plane;
		plane->state->zpos = exynos_plane->index;
		plane->state->normalized_zpos = exynos_plane->index;
		plane->state->alpha = DRM_BLEND_ALPHA_OPAQUE;
		plane->state->pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;
	}
}

static int exynos_drm_plane_set_property(struct drm_plane *plane,
				   struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t val)
{
	const struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(plane->dev, DRM_MODE_OBJECT_PLANE);
	struct exynos_drm_plane_state *exynos_state =
						to_exynos_plane_state(state);

	if (property == p->standard) {
		exynos_state->standard = val;
	} else if (property == p->transfer) {
		exynos_state->transfer = val;
	} else if (property == p->range) {
		exynos_state->range = val;
	} else if (property == p->colormap) {
		exynos_state->colormap = val;
	} else if (property == p->split) {
		exynos_state->split = val;
	} else if (property == p->hdr_fd) {
		exynos_state->hdr_fd = U642I64(val);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int exynos_drm_plane_get_property(struct drm_plane *plane,
				   const struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t *val)
{
	const struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(plane->dev, DRM_MODE_OBJECT_PLANE);
	struct exynos_drm_plane_state *exynos_state =
			to_exynos_plane_state((struct drm_plane_state *)state);

	if (property == p->restriction)
		*val = exynos_state->blob_id_restriction;
	else if (property == p->standard)
		*val = exynos_state->standard;
	else if (property == p->transfer)
		*val = exynos_state->transfer;
	else if (property == p->range)
		*val = exynos_state->range;
	else if (property == p->colormap)
		*val = exynos_state->colormap;
	else if (property == p->split)
		*val = exynos_state->split;
	else if (property == p->hdr_fd)
		*val = I642U64(exynos_state->hdr_fd);
	else
		return -EINVAL;

	return 0;
}

static void exynos_drm_plane_print_state(struct drm_printer *p,
					 const struct drm_plane_state *state)
{
	const struct exynos_drm_plane_state *exynos_state =
		to_exynos_plane_state(state);
	const struct exynos_drm_plane *exynos_plane =
		to_exynos_plane(state->plane);

	drm_printf(p, "\talpha: 0x%x\n", state->alpha);
	drm_printf(p, "\tblend_mode: 0x%x\n", state->pixel_blend_mode);
	drm_printf(p, "\tsplit: 0x%x\n", exynos_state->split);
	drm_printf(p, "\tstandard: %d\n", exynos_state->standard);
	drm_printf(p, "\ttransfer: %d\n", exynos_state->transfer);
	drm_printf(p, "\trange: %d\n", exynos_state->range);
	drm_printf(p, "\thdr_fd: %lld\n", exynos_state->hdr_fd);
	if (exynos_plane->is_win_connected)
		drm_printf(p, "\t\twin_id=%d\n", exynos_plane->win_id);

	if (exynos_plane->ops->atomic_print_state)
		exynos_plane->ops->atomic_print_state(p, exynos_plane);
}

static bool
exynos_drm_plane_format_mode_supported(struct drm_plane *plane, uint32_t format,
				     uint64_t modifier)
{
	int i;
	const struct dpu_fmt *fmt = dpu_find_fmt_info(format);

	if (!fmt)
		return false;

	if (!plane->modifier_count || !modifier)
		return true;

	for (i = 0; i < plane->modifier_count; i++)
		if (has_all_bits(plane->modifiers[i], modifier)) break;

	if (i == plane->modifier_count) {
		DRM_ERROR("no modifiers supported(0x%llx)\n", modifier);
		return false;
	}

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_COLORMAP, modifier) &&
			format == DRM_FORMAT_BGRA8888)
		return true;

	/* allow rest of the modifiers to support content protection */
	modifier &= ~DRM_FORMAT_MOD_PROTECTION;
	if (!modifier)
		return true;

	if (has_all_bits(DRM_FORMAT_MOD_ARM_AFBC(0), modifier))
		if (BIT(FMT_COMP_AFBC) & fmt->comp_mask)
				return true;

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SAJC(0), modifier))
		if (BIT(FMT_COMP_SAJC) & fmt->comp_mask)
				return true;

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_SBWC(0, 0), modifier))
		if (BIT(FMT_COMP_SBWC) & fmt->comp_mask)
				return true;

	if (has_all_bits(DRM_FORMAT_MOD_SAMSUNG_VOTF(0), modifier))
		return true;

	/* If need, check whether to valid the combination of modifier & format */

	return false;
}

static struct drm_plane_funcs exynos_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset		= exynos_drm_plane_reset,
	.atomic_duplicate_state = exynos_drm_plane_duplicate_state,
	.atomic_destroy_state = exynos_drm_plane_destroy_state,
	.atomic_set_property = exynos_drm_plane_set_property,
	.atomic_get_property = exynos_drm_plane_get_property,
	.atomic_print_state = exynos_drm_plane_print_state,
	.format_mod_supported = exynos_drm_plane_format_mode_supported,
};

static int exynos_plane_atomic_check(struct drm_plane *plane,
				     struct drm_plane_state *state)
{
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	struct exynos_drm_plane_state *exynos_state =
						to_exynos_plane_state(state);
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc_state *new_exynos_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	int ret = 0;

	DRM_DEBUG("%s +\n", __func__);

	if (!state->crtc || !state->fb)
		return 0;

	new_crtc_state = drm_atomic_get_new_crtc_state(state->state,
							state->crtc);

	new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
	if (!new_crtc_state->planes_changed || !new_crtc_state->active ||
			new_exynos_crtc_state->modeset_only)
		return 0;

	ret = drm_atomic_helper_check_plane_state(state, new_crtc_state, 0,
			INT_MAX, true, false);
	if (ret)
		return ret;

	new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
	exynos_crtc = to_exynos_crtc(state->crtc);
	if (exynos_crtc->partial && new_exynos_crtc_state->needs_reconfigure)
		exynos_partial_reconfig_coords(exynos_crtc->partial, state,
				&new_exynos_crtc_state->partial_region);

	if (exynos_plane->ops->atomic_check && state->visible) {
		ret = exynos_plane->ops->atomic_check(exynos_plane, exynos_state);
		if (ret)
			return ret;
	}

	DRM_DEBUG("%s -\n", __func__);

	return ret;
}

static void exynos_plane_disable(struct drm_plane *plane, struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);

	if (exynos_crtc->ops->disable_plane)
		exynos_crtc->ops->disable_plane(exynos_crtc, exynos_plane);
}

static void exynos_plane_atomic_update(struct drm_plane *plane,
				       struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct exynos_drm_crtc *exynos_crtc;
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);

	if (!state->crtc)
		return;

	exynos_crtc = to_exynos_crtc(state->crtc);

	if (!state->visible)
		exynos_plane_disable(plane, state->crtc);
	else if (exynos_crtc->ops->update_plane)
		exynos_crtc->ops->update_plane(exynos_crtc, exynos_plane);
}

static void exynos_plane_atomic_disable(struct drm_plane *plane,
					struct drm_plane_state *old_state)
{
	struct drm_crtc *crtc;

	crtc = old_state ? old_state->crtc : plane->state->crtc;
	if (!crtc)
		return;

	exynos_plane_disable(plane, crtc);
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.atomic_check = exynos_plane_atomic_check,
	.atomic_update = exynos_plane_atomic_update,
	.atomic_disable = exynos_plane_atomic_disable,
};

static int
exynos_drm_plane_create_restriction_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);

	p->restriction = drm_property_create(drm_dev,
				   DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
				   "hw restrictions", 0);
	if (!p->restriction)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_attach_restriction_property(
				struct drm_device *drm_dev,
				struct drm_mode_object *obj,
				const struct dpp_ch_restriction *res)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);
	struct drm_property_blob *blob;

	blob = drm_property_create_blob(drm_dev, sizeof(*res), res);
	if (IS_ERR(blob))
		return PTR_ERR(blob);

	drm_object_attach_property(obj, p->restriction, blob->base.id);

	return 0;
}

static int exynos_drm_plane_create_standard_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);
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

	p->standard = drm_property_create_enum(drm_dev, 0, "standard",
				standard_list, ARRAY_SIZE(standard_list));
	if (!p->standard)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_create_transfer_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);
	static const struct drm_prop_enum_list transfer_list[] = {
		{ EXYNOS_TRANSFER_UNSPECIFIED, "Unspecified" },
		{ EXYNOS_TRANSFER_LINEAR, "Linear" },
		{ EXYNOS_TRANSFER_SRGB, "sRGB" },
		{ EXYNOS_TRANSFER_SMPTE_170M, "SMPTE 170M" },
		{ EXYNOS_TRANSFER_GAMMA2_2, "Gamma 2.2" },
		{ EXYNOS_TRANSFER_GAMMA2_6, "Gamma 2.6" },
		{ EXYNOS_TRANSFER_GAMMA2_8, "Gamma 2.8" },
		{ EXYNOS_TRANSFER_ST2084, "ST2084" },
		{ EXYNOS_TRANSFER_HLG, "HLG" },
	};

	p->transfer = drm_property_create_enum(drm_dev, 0, "transfer",
				transfer_list, ARRAY_SIZE(transfer_list));
	if (!p->transfer)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_create_range_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);
	static const struct drm_prop_enum_list range_list[] = {
		{ EXYNOS_RANGE_UNSPECIFIED, "Unspecified" },
		{ EXYNOS_RANGE_FULL, "Full" },
		{ EXYNOS_RANGE_LIMITED, "Limited" },
		{ EXYNOS_RANGE_EXTENDED, "Extended" },
	};

	p->range = drm_property_create_enum(drm_dev, 0, "range", range_list,
						ARRAY_SIZE(range_list));
	if (!p->range)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_create_colormap_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);

	p->colormap = drm_property_create_range(drm_dev, 0, "colormap", 0,
			UINT_MAX);
	if (!p->colormap)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_create_split_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);
	static const struct drm_prop_enum_list split_list[] = {
		{ EXYNOS_SPLIT_NONE, "No Split" },
		{ EXYNOS_SPLIT_LEFT, "Split Left" },
		{ EXYNOS_SPLIT_RIGHT, "Split Right" },
		{ EXYNOS_SPLIT_TOP, "Split Top" },
		{ EXYNOS_SPLIT_BOTTOM, "Split Bottom" },
	};

	p->split = drm_property_create_enum(drm_dev, 0, "split", split_list,
						ARRAY_SIZE(split_list));
	if (!p->split)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_plane_create_hdr_fd_property(struct drm_device *drm_dev)
{
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(drm_dev, DRM_MODE_OBJECT_PLANE);

	p->hdr_fd = drm_property_create_signed_range(drm_dev, 0, "HDR_FD", -1,
			1023);
	if (!p->hdr_fd)
		return -ENOMEM;

	return 0;
}

int exynos_drm_plane_create_properties(struct drm_device *drm_dev)
{
	int ret = 0;

	ret = exynos_drm_plane_create_restriction_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_standard_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_transfer_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_range_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_colormap_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_split_property(drm_dev);
	if (ret)
		return ret;

	ret = exynos_drm_plane_create_hdr_fd_property(drm_dev);
	if (ret)
		return ret;

	return ret;
}

int exynos_plane_init(struct drm_device *dev,
		      struct exynos_drm_plane *exynos_plane, unsigned int index,
		      const struct exynos_drm_plane_config *config,
		      const struct exynos_drm_plane_ops *ops)
{
	int ret;
	unsigned int supported_modes = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				       BIT(DRM_MODE_BLEND_PREMULTI) |
				       BIT(DRM_MODE_BLEND_COVERAGE);
	struct exynos_drm_plane_properties *p =
		dev_get_exynos_properties(dev, DRM_MODE_OBJECT_PLANE);
	struct drm_plane *plane = &exynos_plane->base;

	ret = drm_universal_plane_init(dev, plane, 0, &exynos_plane_funcs,
				       config->pixel_formats,
				       config->num_pixel_formats,
				       config->modifiers, config->type, NULL);
	if (ret) {
		DRM_ERROR("failed to initialize plane\n");
		return ret;
	}

	drm_plane_helper_add(plane, &plane_helper_funcs);

	exynos_plane->index = index;
	exynos_plane->ops = ops;

	ret = drm_plane_create_alpha_property(plane);
	if (ret)
		goto err_prop;

	ret = drm_plane_create_blend_mode_property(plane, supported_modes);
	if (ret)
		goto err_prop;

	ret = drm_plane_create_zpos_property(plane, config->zpos, 0, config->max_zpos);
	if (ret)
		goto err_prop;

	if (EXYNOS_DRM_PLANE_CAP_ROT & config->capabilities)
		ret = drm_plane_create_rotation_property(plane, DRM_MODE_ROTATE_0,
				DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
				DRM_MODE_ROTATE_180 | DRM_MODE_ROTATE_270 |
				DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	else if (EXYNOS_DRM_PLANE_CAP_FLIP & config->capabilities)
		ret = drm_plane_create_rotation_property(plane, DRM_MODE_ROTATE_0,
				DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_180 |
				DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y);
	if (ret)
		goto err_prop;

	if (EXYNOS_DRM_PLANE_CAP_CSC & config->capabilities)
		drm_object_attach_property(&plane->base, p->split, EXYNOS_SPLIT_NONE);

	drm_object_attach_property(&plane->base, p->hdr_fd, -1);

	ret = exynos_drm_plane_attach_restriction_property(dev, &plane->base,
			&config->res);
	if (ret)
		goto err_prop;

	drm_object_attach_property(&plane->base, p->standard,
				EXYNOS_STANDARD_UNSPECIFIED);

	drm_object_attach_property(&plane->base, p->transfer,
				EXYNOS_TRANSFER_UNSPECIFIED);

	drm_object_attach_property(&plane->base, p->range,
				EXYNOS_RANGE_UNSPECIFIED);

	drm_object_attach_property(&plane->base, p->colormap, 0);

	return 0;

err_prop:
	plane->funcs->destroy(plane);
	return ret;
}
