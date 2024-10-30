// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_connector.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_property.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_connector.h>

#define HDR_DOLBY_VISION	BIT(1)
#define HDR_HDR10		BIT(2)
#define HDR_HLG			BIT(3)
#define HDR_HDR10_PLUS		BIT(4)

void exynos_drm_boost_bts_fps(struct exynos_drm_connector *exynos_connector,
		u32 fps, ktime_t expire_time)
{
	exynos_connector->boost_bts_fps = fps;
	exynos_connector->boost_expire_time = expire_time;
}
EXPORT_SYMBOL(exynos_drm_boost_bts_fps);

struct exynos_drm_connector_properties *
exynos_drm_connector_get_properties(struct exynos_drm_connector *exynos_connector)
{
	return dev_get_exynos_properties(exynos_connector->base.dev,
			DRM_MODE_OBJECT_CONNECTOR);
}
EXPORT_SYMBOL(exynos_drm_connector_get_properties);

static struct drm_connector_state *
exynos_drm_connector_duplicate_state(struct drm_connector *connector)
{
	struct exynos_drm_connector_state *exynos_connector_state, *copy;

	if (WARN_ON(!connector->state))
		return NULL;

	exynos_connector_state = to_exynos_connector_state(connector->state);
	copy = kzalloc(sizeof(*copy), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_connector_state, sizeof(*copy));
	copy->seamless_modeset = 0;
	copy->is_lp_transition = 0;

	__drm_atomic_helper_connector_duplicate_state(connector, &copy->base);

	return &copy->base;
}

static void exynos_drm_connector_destroy_state(struct drm_connector *connector,
		struct drm_connector_state *connector_state)
{
	struct exynos_drm_connector_state *exynos_connector_state;

	exynos_connector_state = to_exynos_connector_state(connector_state);
	/* if need, put ref of blob property */
	__drm_atomic_helper_connector_destroy_state(connector_state);
	kfree(exynos_connector_state);
}

static void exynos_drm_connector_reset(struct drm_connector *connector)
{
	struct exynos_drm_connector_state *exynos_connector_state;

	if (connector->state) {
		exynos_drm_connector_destroy_state(connector, connector->state);
		connector->state = NULL;
	}

	exynos_connector_state =
			kzalloc(sizeof(*exynos_connector_state), GFP_KERNEL);
	if (exynos_connector_state) {
		connector->state = &exynos_connector_state->base;
		connector->state->connector = connector;
	} else {
		pr_err("failed to allocate exynos connector state\n");
	}
}

static int exynos_drm_connector_get_property(struct drm_connector *connector,
		const struct drm_connector_state *conn_state,
		struct drm_property *property, uint64_t *val)
{
	struct exynos_drm_connector *exynos_conn =
		to_exynos_connector(conn_state->connector);
	const struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(conn_state);
	const struct exynos_drm_connector_funcs *funcs = exynos_conn->funcs;

	if (funcs && funcs->atomic_get_property)
		return funcs->atomic_get_property(exynos_conn, exynos_conn_state,
				property, val);

	return -EINVAL;
}

static int exynos_drm_connector_set_property(struct drm_connector *connector,
		struct drm_connector_state *conn_state,
		struct drm_property *property, uint64_t val)
{
	struct exynos_drm_connector *exynos_conn =
		to_exynos_connector(conn_state->connector);
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(conn_state);
	const struct exynos_drm_connector_funcs *funcs = exynos_conn->funcs;

	if (funcs && funcs->atomic_set_property)
		return funcs->atomic_set_property(exynos_conn, exynos_conn_state,
				property, val);

	return -EINVAL;
}

static void exynos_drm_connector_print_state(struct drm_printer *p,
		const struct drm_connector_state *state)
{
	struct exynos_drm_connector *exynos_conn =
		to_exynos_connector(state->connector);
	const struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(state);
	const struct exynos_drm_connector_funcs *funcs = exynos_conn->funcs;

	if (funcs && funcs->atomic_print_state)
		funcs->atomic_print_state(p, exynos_conn_state);
}

static const struct drm_connector_funcs exynos_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.reset = exynos_drm_connector_reset,
	.atomic_duplicate_state = exynos_drm_connector_duplicate_state,
	.atomic_destroy_state = exynos_drm_connector_destroy_state,
	.atomic_get_property = exynos_drm_connector_get_property,
	.atomic_set_property = exynos_drm_connector_set_property,
	.atomic_print_state = exynos_drm_connector_print_state,
};

bool is_exynos_drm_connector(const struct drm_connector *connector)
{
	return connector->funcs == &exynos_connector_funcs;
}

int exynos_drm_connector_init(struct drm_device *dev,
		struct exynos_drm_connector *exynos_connector,
		const struct exynos_drm_connector_funcs *funcs,
		int connector_type)
{
	exynos_connector->funcs = funcs;

	return drm_connector_init(dev, &exynos_connector->base,
			&exynos_connector_funcs,
			DRM_MODE_CONNECTOR_DSI);
}
EXPORT_SYMBOL(exynos_drm_connector_init);

static int
exynos_drm_connector_create_luminance_properties(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);

	p->max_luminance = drm_property_create_range(dev, 0, "max_luminance",
			0, UINT_MAX);
	if (!p->max_luminance)
		return -ENOMEM;

	p->max_avg_luminance = drm_property_create_range(dev, 0,
			"max_avg_luminance", 0, UINT_MAX);
	if (!p->max_avg_luminance)
		return -ENOMEM;

	p->min_luminance = drm_property_create_range(dev, 0, "min_luminance",
			0, UINT_MAX);
	if (!p->min_luminance)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_connector_create_hdr_formats_property(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);
	static const struct drm_prop_enum_list props[] = {
		{ __builtin_ffs(HDR_DOLBY_VISION) - 1,	"Dolby Vision"	},
		{ __builtin_ffs(HDR_HDR10) - 1,		"HDR10"		},
		{ __builtin_ffs(HDR_HLG) - 1,		"HLG"		},
#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
		{ __builtin_ffs(HDR_HDR10_PLUS) - 1,	"HDR10_PLUS"	},
#endif
	};

	p->hdr_formats = drm_property_create_bitmask(dev, 0,
			"hdr_formats", props, ARRAY_SIZE(props),
			HDR_DOLBY_VISION | HDR_HDR10 | HDR_HLG
#if IS_ENABLED(CONFIG_DRM_MCD_HDR)
			| HDR_HDR10_PLUS
#endif
			);
	if (!p->hdr_formats)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_connector_create_adjusted_fps_property(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);

	p->adjusted_fps =
		drm_property_create_range(dev, 0, "adjusted_fps", 0, UINT_MAX);
	if (!p->adjusted_fps)
		return -ENOMEM;

	return 0;
}

static int
exynos_drm_connector_create_lp_mode_property(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);

	p->lp_mode = drm_property_create(dev, DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
					 "lp_mode", 0);
	if (IS_ERR(p->lp_mode))
		return PTR_ERR(p->lp_mode);

	return 0;
}

static int
exynos_drm_connector_create_hdr_sink_connected_property(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);

	p->hdr_sink_connected =
			drm_property_create_bool(dev, 0, "hdr_sink_connected");
	if (!p->hdr_sink_connected)
		return -ENOMEM;

	return 0;
}

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
static int exynos_drm_plane_create_fingerprint_mask_property(struct drm_device *dev)
{
	struct exynos_drm_connector_properties *p = dev_get_exynos_properties(dev,
			DRM_MODE_OBJECT_CONNECTOR);

	p->fingerprint_mask = drm_property_create_range(dev, 0, "fingerprint_mask", 0, 1);

	if (!p->fingerprint_mask)
		return -ENOMEM;

	return 0;
}
#endif

int exynos_drm_connector_create_properties(struct drm_device *dev)
{
	int ret;

#if IS_ENABLED(CONFIG_SUPPORT_MASK_LAYER) || IS_ENABLED(CONFIG_USDM_PANEL_MASK_LAYER)
	ret = exynos_drm_plane_create_fingerprint_mask_property(dev);
	if (ret)
		return ret;
#endif

	ret = exynos_drm_connector_create_luminance_properties(dev);
	if (ret)
		return ret;

	ret = exynos_drm_connector_create_hdr_formats_property(dev);
	if (ret)
		return ret;

	ret = exynos_drm_connector_create_adjusted_fps_property(dev);
	if (ret)
		return ret;

	ret = exynos_drm_connector_create_lp_mode_property(dev);
	if (ret)
		return ret;

	ret = exynos_drm_connector_create_hdr_sink_connected_property(dev);

	return ret;
}
