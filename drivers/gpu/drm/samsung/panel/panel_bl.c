// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/sec_panel_notifier_v2.h>
#include "panel_kunit.h"
#include "panel.h"
#include "panel_bl.h"
#ifdef CONFIG_USDM_PANEL_TESTMODE
#include "panel_testmode.h"
#endif
#include "panel_property.h"
#ifdef CONFIG_USDM_PANEL_COPR
#include "copr.h"
#endif

#include "timenval.h"
#include "panel_debug.h"

#ifdef CONFIG_USDM_PANEL_DIMMING
#include "dimming.h"
#include "panel_dimming.h"
#endif

#include "panel_drv.h"
#include "panel_irc.h"

static struct panel_prop_enum_item smooth_transition_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SMOOTH_TRANS_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SMOOTH_TRANS_ON),
};

static struct panel_prop_enum_item acl_pwrsave_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ACL_PWRSAVE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ACL_PWRSAVE_ON),
};

static struct panel_prop_enum_item night_dim_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NIGHT_DIM_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(NIGHT_DIM_ON),
};

static struct panel_prop_list panel_bl_property_array[] = {
	/* enum property */
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_BL_PROPERTY_SMOOTH_TRANSITION,
			SMOOTH_TRANS_ON, smooth_transition_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_BL_PROPERTY_ACL_PWRSAVE,
			ACL_PWRSAVE_OFF, acl_pwrsave_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_BL_PROPERTY_NIGHT_DIM,
			NIGHT_DIM_OFF, night_dim_enum_items),
	/* range property */
	__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_BRIGHTNESS,
			UI_DEF_BRIGHTNESS, 0, 1000000000),
	__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_PREV_BRIGHTNESS,
			UI_DEF_BRIGHTNESS, 0, 1000000000),
};

const char *dim_type_to_str(unsigned int dim_type)
{
	static char *dim_type_str[MAX_DIM_TYPE_STR] = {
		[DIM_TYPE_STR_TABLE] = "table",
		[DIM_TYPE_STR_FLASH] = "flash",
		[DIM_TYPE_STR_GM2] = "gm2",
	};

	if (dim_type >= MAX_DIM_TYPE_STR)
		return NULL;

	return dim_type_str[dim_type];
}

int panel_bl_set_property(struct panel_bl_device *panel_bl,
		int *property, unsigned int value)
{
	struct panel_device *panel;
	char *propname = NULL;

	if (!panel_bl) {
		panel_err("panel_bl is null\n");
		return -EINVAL;
	}
	panel = to_panel_device(panel_bl);

	if (!property) {
		panel_err("property is null\n");
		return -EINVAL;
	}

	if (property == &panel_bl->props.brightness)
		propname = PANEL_BL_PROPERTY_BRIGHTNESS;
	if (property == &panel_bl->props.prev_brightness)
		propname = PANEL_BL_PROPERTY_PREV_BRIGHTNESS;
	else if (property == &panel_bl->props.smooth_transition)
		propname = PANEL_BL_PROPERTY_SMOOTH_TRANSITION;
	else if (property == &panel_bl->props.acl_opr)
		propname = PANEL_BL_PROPERTY_ACL_OPR;
	else if (property == &panel_bl->props.acl_pwrsave)
		propname = PANEL_BL_PROPERTY_ACL_PWRSAVE;
	else if (property == &panel_bl->props.night_dim)
		propname = PANEL_BL_PROPERTY_NIGHT_DIM;

	if (!propname) {
		panel_err("unknown property\n");
		return 0;
	}

	if (panel_set_property_value(panel,
				propname, value) < 0) {
		panel_warn("failed to set property(%s) %d\n",
				propname, value);
		return -EINVAL;
	}
	*property = value;

	return 0;
}

#ifdef DEBUG_PAC
static void print_tbl(int *tbl, int sz)
{
	int i;

	for (i = 0; i < sz; i++) {
		panel_info("%d", tbl[i]);
		if (!((i + 1) % 10))
			panel_info("\n");
	}
}
#else
static void print_tbl(int *tbl, int sz) {}
#endif

int max_brt_tbl(struct brightness_table *brt_tbl)
{
	if (unlikely(!brt_tbl || !brt_tbl->brt || !brt_tbl->sz_brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	return brt_tbl->brt[brt_tbl->sz_brt - 1];
}

static int get_subdev_max_brightness(struct panel_bl_device *panel_bl, int id)
{
	struct panel_bl_sub_dev *subdev;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (id < 0 || id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("bl-%d exceeded max subdev\n", id);
		return -EINVAL;
	}
	subdev = &panel_bl->subdev[id];
	return max_brt_tbl(&subdev->brt_tbl);
}

int get_max_brightness(struct panel_bl_device *panel_bl)
{
	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return false;
	}

	return get_subdev_max_brightness(panel_bl, panel_bl->props.id);
}

static bool is_valid_brightness(struct panel_bl_device *panel_bl, int brightness)
{
	int max_brightness;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return false;
	}

	max_brightness = get_max_brightness(panel_bl);
	if (brightness < 0 || brightness > max_brightness) {
		panel_err("out of range %d, (min:0, max:%d)\n",
				brightness, max_brightness);
		return false;
	}

	return true;
}

bool __mockable is_hbm_brightness(struct panel_bl_device *panel_bl, int brightness)
{
	struct panel_bl_sub_dev *subdev;
	int luminance;
	int sz_ui_lum;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return false;
	}

	subdev = &panel_bl->subdev[panel_bl->props.id];

	if (subdev->brt_tbl.control_type == BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2)
		return (brightness > subdev->brt_tbl.brt[subdev->brt_tbl.sz_ui_brt - 1]);

	luminance = get_actual_brightness(panel_bl, brightness);

	sz_ui_lum = subdev->brt_tbl.sz_ui_lum;
	if (sz_ui_lum <= 0 || sz_ui_lum > subdev->brt_tbl.sz_lum) {
		panel_err("bl-%d out of range (sz_ui_lum %d)\n",
				panel_bl->props.id, sz_ui_lum);
		return false;
	}

	return (luminance > subdev->brt_tbl.lum[sz_ui_lum - 1]);
}
EXPORT_SYMBOL(is_hbm_brightness);

bool is_ext_hbm_brightness(struct panel_bl_device *panel_bl, int brightness)
{
	struct panel_bl_sub_dev *subdev;
	int luminance;
	int sz_ui_lum;
	int sz_hbm_lum;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return false;
	}

	subdev = &panel_bl->subdev[panel_bl->props.id];
	if (subdev->brt_tbl.control_type == BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2)
		return false;

	luminance = get_actual_brightness(panel_bl, brightness);

	sz_ui_lum = subdev->brt_tbl.sz_ui_lum;
	sz_hbm_lum = subdev->brt_tbl.sz_hbm_lum;
	if (sz_hbm_lum <= 0 || sz_ui_lum + sz_hbm_lum > subdev->brt_tbl.sz_lum) {
		panel_err("bl-%d out of range (sz_hbm_lum %d)\n",
				panel_bl->props.id, sz_hbm_lum);
		return false;
	}

	return (luminance > subdev->brt_tbl.lum[sz_ui_lum + sz_hbm_lum - 1]);
}

/*
 * search_tbl - binary search an array of integer elements
 * @tbl : pointer to first element to search
 * @sz : number of elements
 * @type : type of searching type (i.e LOWER, UPPER, EXACT value)
 * @value : a value being searched for
 *
 * This function does a binary search on the given array.
 * The contents of the array should already be in ascending sorted order
 *
 * Note that the value need to be inside of the range of array ['start', 'end']
 * if the value is out of array range, return -1.
 * if not, this function returns array index.
 */
int search_tbl(int *tbl, int sz, enum SEARCH_TYPE type, int value)
{
	int l = 0, m, r = sz - 1;

	if (unlikely(!tbl || sz == 0)) {
		panel_err("invalid parameter(tbl %p, sz %d)\n", tbl, sz);
		return -EINVAL;
	}

	if (value <= tbl[l])
		return l;

	if (value >= tbl[r])
		return r;

	while (l <= r) {
		m = l + (r - l) / 2;
		if (tbl[m] == value)
			return m;
		if (tbl[m] < value)
			l = m + 1;
		else
			r = m - 1;
	}

	if (type == SEARCH_TYPE_LOWER)
		return ((r < 0) ? -1 : r);
	else if (type == SEARCH_TYPE_UPPER)
		return ((l > sz - 1) ? -1 : l);
	else if (type == SEARCH_TYPE_EXACT)
		return -1;

	return -1;
}

#ifdef CONFIG_USDM_PANEL_DIMMING
static int search_brt_tbl(struct brightness_table *brt_tbl, int brightness)
{
	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	return search_tbl(brt_tbl->brt, brt_tbl->sz_brt,
			SEARCH_TYPE_UPPER, brightness);
}

static int search_brt_to_step_tbl(struct brightness_table *brt_tbl, int brightness)
{
	if (unlikely(!brt_tbl || !brt_tbl->brt_to_step)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	return search_tbl(brt_tbl->brt_to_step, brt_tbl->sz_brt_to_step,
			SEARCH_TYPE_UPPER, brightness);
}

__mockable int get_subdev_actual_brightness_index(struct panel_bl_device *panel_bl,
		int id, int brightness)
{
	int index;
	struct panel_bl_sub_dev *subdev;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (id < 0 || id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("bl-%d exceeded max subdev\n", id);
		return -EINVAL;
	}

	if (!is_valid_brightness(panel_bl, brightness)) {
		panel_err("bl-%d invalid brightness\n", id);
		return -EINVAL;
	}

	subdev = &panel_bl->subdev[id];

#if defined(CONFIG_PANEL_BL_USE_BRT_CACHE)
	if (brightness >= subdev->sz_brt_cache_tbl) {
		panel_err("bl-%d exceeded brt_cache_tbl size %d\n", id, brightness);
		return -EINVAL;
	}

	if (subdev->brt_cache_tbl[brightness] == -1) {
		index = search_brt_tbl(&subdev->brt_tbl, brightness);
		if (index < 0) {
			panel_err("bl-%d failed to search %d, ret %d\n",
					id, brightness, index);
			return index;
		}
		subdev->brt_cache_tbl[brightness] = index;
#ifdef DEBUG_PAC
		panel_info("bl-%d brightness %d, brt_cache_tbl[%d] = %d\n",
				id, brightness, brightness,
				subdev->brt_cache_tbl[brightness]);
#endif
	} else {
		index = subdev->brt_cache_tbl[brightness];
#ifdef DEBUG_PAC
		panel_info("bl-%d brightness %d, brt_cache_tbl[%d] = %d\n",
				id, brightness, brightness,
				subdev->brt_cache_tbl[brightness]);
#endif
	}
#else
	index = search_brt_tbl(&subdev->brt_tbl, brightness);
	if (index < 0) {
		panel_err("bl-%d failed to search %d, ret %d\n",
				id, brightness, index);
		return index;
	}
#endif

	if (index > subdev->brt_tbl.sz_brt - 1)
		index = subdev->brt_tbl.sz_brt - 1;

	return index;
}
EXPORT_SYMBOL(get_subdev_actual_brightness_index);

__mockable int get_actual_brightness_index(struct panel_bl_device *panel_bl, int brightness)
{
	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (!is_valid_brightness(panel_bl, brightness)) {
		panel_err("bl-%d invalid brightness\n", panel_bl->props.id);
		return -EINVAL;
	}

	return get_subdev_actual_brightness_index(panel_bl,
			panel_bl->props.id, brightness);
}
EXPORT_SYMBOL(get_actual_brightness_index);

int get_brightness_pac_step_by_subdev_id(struct panel_bl_device *panel_bl, int id, int brightness)
{
	int index;
	struct panel_bl_sub_dev *subdev;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (unlikely(id >= MAX_PANEL_BL_SUBDEV)) {
		panel_err("invalid id %d\n", id);
		return -EINVAL;
	}

	subdev = &panel_bl->subdev[id];

	if (!is_valid_brightness(panel_bl, brightness)) {
		panel_err("bl-%d invalid brightness\n", id);
		return -EINVAL;
	}

	index = search_brt_to_step_tbl(&subdev->brt_tbl, brightness);
	if (index < 0) {
		panel_err("failed to search %d, ret %d\n", brightness, index);
		print_tbl(subdev->brt_tbl.brt_to_step,
				subdev->brt_tbl.sz_brt_to_step);
		return index;
	}

#ifdef DEBUG_PAC
	panel_info("bl-%d brightness %d, pac step %d, brt %d\n",
			id, brightness, index,
			subdev->brt_tbl.brt_to_step[index]);
#endif

	if (index > subdev->brt_tbl.sz_brt_to_step - 1)
		index = subdev->brt_tbl.sz_brt_to_step - 1;

	return index;
}
EXPORT_SYMBOL(get_brightness_pac_step_by_subdev_id);

int get_brightness_pac_step(struct panel_bl_device *panel_bl, int brightness)
{
	int id;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	id = panel_bl->props.id;

	return get_brightness_pac_step_by_subdev_id(panel_bl, id, brightness);
}
EXPORT_SYMBOL(get_brightness_pac_step);

int get_brightness_of_brt_to_step(struct panel_bl_device *panel_bl, int id, int brightness)
{
	struct panel_bl_sub_dev *subdev;
	struct brightness_table *brt_tbl;
	int step;

	subdev = &panel_bl->subdev[id];
	brt_tbl = &subdev->brt_tbl;

	step = get_brightness_pac_step(panel_bl, brightness);
	if (step < 0) {
		panel_err("bl-%d invalid pac stap %d\n", id, step);
		return -EINVAL;
	}
	return brt_tbl->brt_to_step[step];
}

int get_subdev_actual_brightness(struct panel_bl_device *panel_bl, int id, int brightness)
{
	int index;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (id < 0 || id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("bl-%d exceeded max subdev\n", id);
		return -EINVAL;
	}

	/* return 0 if luminance table is invalid  */
	if (panel_bl->subdev[id].brt_tbl.lum == NULL || panel_bl->subdev[id].brt_tbl.sz_lum == 0)
		return 0;

	index = get_subdev_actual_brightness_index(panel_bl, id, brightness);
	if (index < 0) {
		panel_err("bl-%d failed to get actual_brightness_index %d\n", id, index);
		return index;
	}
	return panel_bl->subdev[id].brt_tbl.lum[index];
}

int get_subdev_actual_brightness_interpolation(struct panel_bl_device *panel_bl, int id, int brightness)
{
	int upper_idx, lower_idx;
	int upper_lum, lower_lum;
	int upper_brt, lower_brt;
	int step, upper_step, lower_step;

	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	if (id < 0 || id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("bl-%d exceeded max subdev\n", id);
		return -EINVAL;
	}

	upper_idx = get_subdev_actual_brightness_index(panel_bl, id, brightness);
	if (upper_idx < 0) {
		panel_err("bl-%d failed to get actual_brightness_index %d\n",
				id, upper_idx);
		return upper_idx;
	}
	lower_idx = max(0, (upper_idx - 1));

	lower_lum = panel_bl->subdev[id].brt_tbl.lum[lower_idx];
	upper_lum = panel_bl->subdev[id].brt_tbl.lum[upper_idx];

	lower_brt = panel_bl->subdev[id].brt_tbl.brt[lower_idx];
	upper_brt = panel_bl->subdev[id].brt_tbl.brt[upper_idx];

	lower_step = get_brightness_pac_step(panel_bl, lower_brt);
	step = get_brightness_pac_step(panel_bl, brightness);
	upper_step = get_brightness_pac_step(panel_bl, upper_brt);

	return (lower_lum * CALC_SCALE) + (s32)((upper_step == lower_step) ? 0 :
			((s64)(upper_lum - lower_lum) * (step - lower_step) * CALC_SCALE) /
			(s64)(upper_step - lower_step));
}

int get_actual_brightness(struct panel_bl_device *panel_bl, int brightness)
{
	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	return get_subdev_actual_brightness(panel_bl,
			panel_bl->props.id, brightness);
}
EXPORT_SYMBOL(get_actual_brightness);

int get_actual_brightness_interpolation(struct panel_bl_device *panel_bl, int brightness)
{
	if (unlikely(!panel_bl)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	return get_subdev_actual_brightness_interpolation(panel_bl,
			panel_bl->props.id, brightness);
}
#endif /* CONFIG_USDM_PANEL_DIMMING */

void panel_bl_update_acl_state(struct panel_bl_device *panel_bl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	panel = to_panel_device(panel_bl);
	panel_data = &panel->panel_data;

#ifdef CONFIG_USDM_PANEL_HMD
	if (panel_bl->props.id == PANEL_BL_SUBDEV_TYPE_HMD) {
		panel_bl->props.acl_opr = 0;
		panel_bl_set_property(panel_bl, &panel_bl->props.acl_pwrsave, ACL_PWRSAVE_OFF);
		return;
	}
#endif
#ifdef CONFIG_USDM_PANEL_AOD_BL
	if (panel_bl->props.id == PANEL_BL_SUBDEV_TYPE_AOD) {
		panel_bl->props.acl_opr = 0;
		panel_bl_set_property(panel_bl, &panel_bl->props.acl_pwrsave, ACL_PWRSAVE_OFF);
		return;
	}
#endif
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	if (panel_bl->props.mask_layer_br_hook == MASK_LAYER_HOOK_ON) {
		panel_bl->props.acl_opr = 0;
		panel_bl_set_property(panel_bl, &panel_bl->props.acl_pwrsave, ACL_PWRSAVE_OFF);
		return;
	}
#endif
	if (panel_data->props.adaptive_control > 100) {
		panel_warn("invalid range %d\n", panel_data->props.adaptive_control);
		return;
	}
	panel_bl->props.acl_opr = panel_data->props.adaptive_control;
	panel_bl_set_property(panel_bl, &panel_bl->props.acl_pwrsave,
		(panel_data->props.adaptive_control == 0) ?
		ACL_PWRSAVE_OFF : ACL_PWRSAVE_ON);
}

int panel_bl_get_acl_pwrsave(struct panel_bl_device *panel_bl)
{
	return panel_bl->props.acl_pwrsave;
}
EXPORT_SYMBOL(panel_bl_get_acl_pwrsave);

int panel_bl_get_acl_opr(struct panel_bl_device *panel_bl)
{
	return panel_bl->props.acl_opr;
}
EXPORT_SYMBOL(panel_bl_get_acl_opr);

int panel_bl_get_smooth_transition(struct panel_bl_device *panel_bl)
{
	return panel_bl->props.smooth_transition;
}
EXPORT_SYMBOL(panel_bl_get_smooth_transition);

void panel_bl_clear_brightness_set_count(struct panel_bl_device *panel_bl)
{
	atomic_set(&panel_bl->props.brightness_set_count, 0);
}

int panel_bl_get_brightness_set_count(struct panel_bl_device *panel_bl)
{
	return atomic_read(&panel_bl->props.brightness_set_count);
}
EXPORT_SYMBOL(panel_bl_get_brightness_set_count);

inline void panel_bl_inc_brightness_set_count(struct panel_bl_device *panel_bl)
{
	atomic_inc(&panel_bl->props.brightness_set_count);
}

void panel_bl_clear_brightness_non_zero_set_count(struct panel_bl_device *panel_bl)
{
	atomic_set(&panel_bl->props.brightness_non_zero_set_count, 0);
}

int panel_bl_get_brightness_non_zero_set_count(struct panel_bl_device *panel_bl)
{
	return atomic_read(&panel_bl->props.brightness_non_zero_set_count);
}

inline void panel_bl_inc_brightness_non_zero_set_count(struct panel_bl_device *panel_bl)
{
	atomic_inc(&panel_bl->props.brightness_non_zero_set_count);
}

int panel_bl_set_subdev(struct panel_bl_device *panel_bl, int id)
{
	panel_bl->props.id = id;
	panel_bl_clear_brightness_set_count(panel_bl);
	return 0;
}

int panel_bl_update_average(struct panel_bl_device *panel_bl, size_t index)
{
	struct timespec64 cur_ts;
	int cur_brt;

	if (index >= ARRAY_SIZE(panel_bl->tnv))
		return -EINVAL;

	ktime_get_ts64(&cur_ts);
	cur_brt = panel_bl->props.actual_brightness_intrp / CALC_SCALE;
	timenval_update_snapshot(&panel_bl->tnv[index], cur_brt, cur_ts);

	return 0;
}

int panel_bl_clear_average(struct panel_bl_device *panel_bl, size_t index)
{
	if (index >= ARRAY_SIZE(panel_bl->tnv))
		return -EINVAL;

	timenval_clear_average(&panel_bl->tnv[index]);

	return 0;
}

int panel_bl_get_average_and_clear(struct panel_bl_device *panel_bl, size_t index)
{
	int avg;

	if (index >= ARRAY_SIZE(panel_bl->tnv))
		return -EINVAL;

	panel_mutex_lock(&panel_bl->lock);
	panel_bl_update_average(panel_bl, index);
	avg = panel_bl->tnv[index].avg;
	panel_bl_clear_average(panel_bl, index);
	panel_mutex_unlock(&panel_bl->lock);

	return avg;
}

/*
 * aor interpolation function type 1
 * A-Dimming : calculate interpolation aor.
 * S-Dimming : calculate interpolation aor using vbase luminance.
 */
int aor_interpolation(unsigned int *brt_tbl, unsigned int *lum_tbl,
		u8(*aor_tbl)[2], int size, int size_ui_lum, u32 vtotal, int brightness)
{
	int upper_idx, lower_idx;
	u64 upper_lum, lower_lum;
	u64 upper_brt, lower_brt;
	u64 upper_aor, lower_aor, aor = 0;
	u64 upper_aor_ratio, lower_aor_ratio, aor_ratio = 0;
	u64 intrp_brt = 0, vbase_lum = 0;
	enum DIMTYPE dimtype;

	upper_idx = search_tbl(brt_tbl, size, SEARCH_TYPE_UPPER, brightness);
	lower_idx = max(0, (upper_idx - 1));
	upper_lum = lum_tbl[upper_idx] * CALC_SCALE;
	lower_lum = lum_tbl[lower_idx] * CALC_SCALE;
	upper_brt = brt_tbl[upper_idx];
	lower_brt = brt_tbl[lower_idx];
	upper_aor = aor_tbl[upper_idx][0] << 8 | aor_tbl[upper_idx][1];
	lower_aor = aor_tbl[lower_idx][0] << 8 | aor_tbl[lower_idx][1];
	upper_aor_ratio = AOR_TO_RATIO(upper_aor, vtotal);
	lower_aor_ratio = AOR_TO_RATIO(lower_aor, vtotal);

	if (upper_brt == brightness)
		return (int)upper_aor;

	dimtype = ((upper_aor == lower_aor) ||
			((upper_idx < size_ui_lum - 1) &&
			 (aor_tbl[upper_idx + 1][0] << 8 |
			  aor_tbl[upper_idx + 1][1]) == upper_aor) ||
			((lower_idx > 0) &&
			 (aor_tbl[lower_idx - 1][0] << 8 |
			  aor_tbl[lower_idx - 1][1]) == lower_aor)) ?
		S_DIMMING : A_DIMMING;

	if (dimtype == A_DIMMING) {
		aor_ratio = (disp_interpolation64(lower_aor_ratio * disp_pow(10, 3), upper_aor_ratio * disp_pow(10, 3),
					(s32)((u64)brightness - lower_brt) * disp_pow(10, 2),
					(s32)(upper_brt - lower_brt) * disp_pow(10, 2)) + 5 * disp_pow(10, 2)) / disp_pow(10, 3);
		aor = disp_div64(vtotal * aor_ratio + 5 * disp_pow(10, 3), disp_pow(10, 4));
	} else if (dimtype == S_DIMMING) {
		vbase_lum = VIRTUAL_BASE_LUMINANCE(upper_lum, upper_aor_ratio);
		vbase_lum = disp_pow_round(vbase_lum, 2);
		intrp_brt = disp_interpolation64(lower_lum * disp_pow(10, 4), upper_lum * disp_pow(10, 4),
				(s32)((u64)brightness - lower_brt), (s32)(upper_brt - lower_brt));
		intrp_brt = disp_pow_round(intrp_brt, 4);
		aor_ratio = disp_pow(10, 8) - disp_div64(intrp_brt * disp_pow(10, 6), vbase_lum);
		aor_ratio = disp_pow_round(aor_ratio, 4) / disp_pow(10, 4);
		aor = disp_pow_round(vtotal * aor_ratio, 4) / disp_pow(10, 4);
	}

	panel_dbg("aor: brightness %3d.%02d lum %3lld aor %2lld.%02lld, vbase_lum %3lld.%04lld, intrp_brt %3lld.%03lld, aor(%2lld.%02lld %3lld %04X)\n",
			brightness / CALC_SCALE, brightness % CALC_SCALE, upper_lum / CALC_SCALE, upper_aor_ratio / CALC_SCALE, upper_aor_ratio % CALC_SCALE,
			vbase_lum / disp_pow(10, 4), vbase_lum % disp_pow(10, 4),
			intrp_brt / disp_pow(10, 6), intrp_brt % disp_pow(10, 6) / disp_pow(10, 3),
			aor_ratio / disp_pow(10, 2), aor_ratio % disp_pow(10, 2), aor, (int)aor);

	return (int)aor;
}

/*
 * aor interpolation function type 2
 * calculate interpolation aor without
 * distinction of A/S dimming type.
 */
int aor_interpolation_2(unsigned int *brt_tbl,
		u8(*aor_tbl)[2], int size, int size_ui_lum, u32 vtotal, int brightness)
{
	int upper_idx, lower_idx;
	u64 upper_brt, lower_brt;
	u64 upper_aor, lower_aor, aor;
	u64 upper_aor_ratio, lower_aor_ratio, aor_ratio = 0;

	upper_idx = search_tbl(brt_tbl, size, SEARCH_TYPE_UPPER, brightness);
	lower_idx = max(0, (upper_idx - 1));
	upper_brt = brt_tbl[upper_idx];
	lower_brt = brt_tbl[lower_idx];
	upper_aor = aor_tbl[upper_idx][0] << 8 | aor_tbl[upper_idx][1];
	lower_aor = aor_tbl[lower_idx][0] << 8 | aor_tbl[lower_idx][1];
	upper_aor_ratio = AOR_TO_RATIO(upper_aor, vtotal);
	lower_aor_ratio = AOR_TO_RATIO(lower_aor, vtotal);

	if (upper_brt == brightness) {
		aor = upper_aor;
	} else {
		aor_ratio = (disp_interpolation64(lower_aor_ratio * disp_pow(10, 3), upper_aor_ratio * disp_pow(10, 3),
					(s32)((u64)brightness - lower_brt) * disp_pow(10, 2),
					(s32)(upper_brt - lower_brt) * disp_pow(10, 2)) + 5 * disp_pow(10, 2)) / disp_pow(10, 3);
		aor = disp_div64(vtotal * aor_ratio + 5 * disp_pow(10, 3), disp_pow(10, 4));
	}

	panel_dbg("aor: brightness %3d.%02d aor([%d]%2lld.%02lld(0x%04X) [%d]%2lld.%02lld(0x%04X)) aor(%2lld.%02lld %3lld %04X) vtotal %d\n",
			brightness / CALC_SCALE, brightness % CALC_SCALE, upper_idx, upper_aor_ratio / CALC_SCALE, upper_aor_ratio % CALC_SCALE, (unsigned int)upper_aor,
			lower_idx, lower_aor_ratio / CALC_SCALE, lower_aor_ratio % CALC_SCALE, (unsigned int)lower_aor,
			aor_ratio / disp_pow(10, 2), aor_ratio % disp_pow(10, 2), aor, (unsigned int)aor, vtotal);

	return (int)aor;
}

int panel_bl_aor_interpolation(struct panel_bl_device *panel_bl,
		int id, u8(*aor_tbl)[2])
{
	struct panel_bl_sub_dev *subdev;
	struct brightness_table *brt_tbl;
	int brightness;

	subdev = &panel_bl->subdev[id];
	brt_tbl = &subdev->brt_tbl;
	brightness = subdev->brightness;
	brightness = get_brightness_of_brt_to_step(panel_bl, id, brightness);

	return aor_interpolation(brt_tbl->brt, brt_tbl->lum,
			aor_tbl, brt_tbl->sz_lum,
			brt_tbl->sz_ui_lum, brt_tbl->vtotal, brightness);
}

int panel_bl_aor_interpolation_2(struct panel_bl_device *panel_bl,
		int id, u8(*aor_tbl)[2])
{
	struct panel_bl_sub_dev *subdev;
	struct brightness_table *brt_tbl;
	int brightness;

	subdev = &panel_bl->subdev[id];
	brt_tbl = &subdev->brt_tbl;
	brightness = subdev->brightness;
	brightness = get_brightness_of_brt_to_step(panel_bl, id, brightness);

	return aor_interpolation_2(brt_tbl->brt, aor_tbl, brt_tbl->sz_lum,
			brt_tbl->sz_ui_lum, brt_tbl->vtotal, brightness);
}
EXPORT_SYMBOL(panel_bl_aor_interpolation_2);

int panel_bl_irc_interpolation(struct panel_bl_device *panel_bl, int id, struct panel_irc_info *irc_info)
{
	struct panel_bl_sub_dev *subdev;
	struct brightness_table *brt_tbl;
	int brightness;

	subdev = &panel_bl->subdev[id];
	brt_tbl = &subdev->brt_tbl;
	brightness = subdev->brightness;
	brightness = get_brightness_of_brt_to_step(panel_bl, id, brightness);

	if (is_ext_hbm_brightness(panel_bl, brightness))
		brightness = brt_tbl->brt[brt_tbl->sz_ui_lum + brt_tbl->sz_hbm_lum - 1];

	return generate_irc(brt_tbl, irc_info, brightness);
}
EXPORT_SYMBOL(panel_bl_irc_interpolation);

#ifdef CONFIG_USDM_BLIC_I2C
/*
 * If blic cmd should be sent when brightness change,
 * Use this func and add [PANEL_BLIC_I2C_BRIGHTNESS_SEQ] on blic seqtbl.
 */
int panel_bl_set_brightness_blic(struct panel_bl_device *panel_bl)
{
	struct panel_device *panel;
	int ret;

	panel = to_panel_device(panel_bl);

	ret = panel_blic_brightness(panel, false);

	return ret;
}
#endif

//void g_tracing_mark_write(char id, char *str1, int value);
int panel_bl_set_brightness(struct panel_bl_device *panel_bl, int id, u32 send_cmd)
{
	int ret = 0, ilum = 0, luminance = 0, brightness, step;
	char *seqname = PANEL_SET_BL_SEQ;
	struct panel_bl_sub_dev *subdev;
	struct panel_device *panel;
	bool need_update_display_mode = false;

	if (panel_bl == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (id < 0 || id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("bl-%d exceeded max subdev\n", id);
		return -EINVAL;
	}

	panel = to_panel_device(panel_bl);
	panel_bl->props.id = id;
	subdev = &panel_bl->subdev[id];
	brightness = subdev->brightness;

	if (!subdev->brt_tbl.brt || subdev->brt_tbl.sz_brt == 0) {
		panel_err("bl-%d brightness table not exist\n", id);
		return -EINVAL;
	}

	if (!is_valid_brightness(panel_bl, brightness)) {
		panel_err("bl-%d invalid brightness\n", id);
		return -EINVAL;
	}

	luminance = get_actual_brightness(panel_bl, brightness);
	ilum = get_actual_brightness_index(panel_bl, brightness);
	step = get_brightness_pac_step(panel_bl, brightness);
	if (step < 0) {
		panel_err("bl-%d invalid pac stap %d\n", id, step);
		return -EINVAL;
	}

	panel_bl_set_property(panel_bl, &panel_bl->props.prev_brightness,
			panel_bl->props.brightness);
	panel_bl_set_property(panel_bl, &panel_bl->props.brightness, brightness);
	panel_bl->props.actual_brightness = luminance;
	panel_bl->props.actual_brightness_index = ilum;
	panel_bl->props.actual_brightness_intrp =
		get_actual_brightness_interpolation(panel_bl, brightness);
	panel_bl->props.step = step;
	panel_bl->props.brightness_of_step = subdev->brt_tbl.brt_to_step[step];
	panel_bl_update_acl_state(panel_bl);

	panel_info("bl-%d br:%d nit:%d acl:%d cnt:%d%s\n",
			id, brightness, luminance,
			panel_bl_get_acl_opr(panel_bl),
			panel_bl_get_brightness_set_count(panel_bl),
			(send_cmd == SKIP_CMD ? " (saved)" : ""));

	if (unlikely(send_cmd == SKIP_CMD)) {
		panel_bl_set_saved_flag(panel_bl, true);
		goto set_br_exit;
	}

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	if (panel_vrr_is_supported(panel) && panel->panel_data.props.vrr_updated == true) {
		panel->panel_data.props.vrr_updated = false;
		need_update_display_mode = true;
	}
#endif

	//g_tracing_mark_write('C', "lcd_br", luminance);
#ifdef CONFIG_USDM_PANEL_HMD
	if (id == PANEL_BL_SUBDEV_TYPE_HMD)
		seqname = PANEL_HMD_BL_SEQ;
#endif
#ifdef CONFIG_USDM_PANEL_AOD_BL
	if (id == PANEL_BL_SUBDEV_TYPE_AOD)
		seqname = PANEL_ALPM_SET_BL_SEQ;
#endif

	if (panel->panel_data.ddi_props.evasion_disp_det) {
		if (panel_get_cur_state(panel) == PANEL_STATE_NORMAL)
			panel_disable_irq(panel, PANEL_IRQ_DISP_DET);
	}

	if (brightness > 0)
		panel_bl_inc_brightness_non_zero_set_count(panel_bl);

	if (!strcmp(seqname, PANEL_SET_BL_SEQ) && need_update_display_mode) {
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
		ret = panel_set_display_mode_nolock(panel, panel->panel_data.props.panel_mode);
		if (unlikely(ret < 0)) {
			panel_err("failed to panel_set_display_mode\n");
			goto set_br_exit;
		}
		panel_display_mode_cb(panel);
#endif
	} else {
		ret = panel_do_seqtbl_by_name_nolock(panel, seqname);
		if (unlikely(ret < 0)) {
			panel_err("failed to write set_bl seqtbl %s\n", seqname);
			goto set_br_exit;
		}
#ifdef CONFIG_USDM_BLIC_I2C
		ret = panel_bl_set_brightness_blic(panel_bl);
		if (unlikely(ret < 0)) {
			panel_err("failed to write panel_bl_set_brightness_blic\n");
			goto set_br_exit;
		}
#endif
	}

	panel_bl_update_average(panel_bl, 0);
	panel_bl_update_average(panel_bl, 1);

	wake_up_interruptible_all(&panel_bl->wq.wait);
	panel_bl_inc_brightness_set_count(panel_bl);

	if (panel->panel_data.ddi_props.evasion_disp_det) {
		if (panel_get_cur_state(panel) == PANEL_STATE_NORMAL)
			panel_enable_irq(panel, PANEL_IRQ_DISP_DET);
	}
	panel_bl_set_saved_flag(panel_bl, false);

set_br_exit:
	return ret;
}

static int panel_bl_subdev_get_valid_brightness(struct panel_bl_device *panel_bl,
	const int subdev_id, const int brt)
{
	int max;

	max = get_subdev_max_brightness(panel_bl, subdev_id);
	if (max < 0) {
		panel_err("failed to update subdev[%d] brightness\n", subdev_id);
		return 0;
	} else if (max < brt) {
		panel_dbg("subdev[%d] brightness set to %d -> %d\n", subdev_id, brt, max);
		return max;
	}
	return brt;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct panel_bl_device *panel_bl = bl_get_data(bd);

	return get_actual_brightness(panel_bl, bd->props.brightness);
}

int panel_update_subdev_brightness(struct panel_device *panel, u32 subdev_id, u32 brightness)
{
	struct panel_bl_device *panel_bl;

	if (!panel) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	if (subdev_id >= MAX_PANEL_BL_SUBDEV) {
		panel_err("invalid subdev id %u\n", subdev_id);
		return -EINVAL;
	}
	panel_bl->subdev[subdev_id].brightness =
		panel_bl_subdev_get_valid_brightness(panel_bl, subdev_id, brightness);

	return 0;
}

int _panel_update_brightness_nolock(struct panel_device *panel, u32 send_cmd)
{
	int ret = 0;
	int id, brightness;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct backlight_device *bd = panel_bl->bd;

	if (bd == NULL) {
		panel_dbg("panel_bl not prepared\n");
		return -ENODEV;
	}

#ifdef CONFIG_USDM_PANEL_TESTMODE
	if (panel_testmode_is_on(panel)) {
		panel_info("testmode is running. ignore brightness updates.");
		return 0;
	}
#endif

	if (panel_get_cur_state(panel) == PANEL_STATE_OFF ||
		panel_get_cur_state(panel) == PANEL_STATE_ON)
		send_cmd = SKIP_CMD;

	brightness = bd->props.brightness;
	if (brightness < 0) {
		panel_err("brightness %d is out of range\n", brightness);
		brightness = 0;
	}

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	if (panel_bl->props.mask_layer_br_hook == MASK_LAYER_HOOK_ON) {
		brightness = panel_bl->props.mask_layer_br_target;
		panel_info("mask_layer_br_hook (%d)->(%d), skip brighntess\n",
			bd->props.brightness, panel_bl->props.mask_layer_br_target);
		send_cmd = SKIP_CMD;
	}
#endif

	panel_update_subdev_brightness(panel, PANEL_BL_SUBDEV_TYPE_DISP,
		panel_bl_subdev_get_valid_brightness(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, brightness));
#ifdef CONFIG_USDM_PANEL_AOD_BL
	panel_update_subdev_brightness(panel, PANEL_BL_SUBDEV_TYPE_AOD,
		panel_bl_subdev_get_valid_brightness(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD, brightness));
#endif

	id = panel_bl->props.id;
#ifdef CONFIG_USDM_PANEL_HMD
	if (id == PANEL_BL_SUBDEV_TYPE_HMD) {
		panel_info("keep plat_br:%d\n", brightness);
		ret = -EINVAL;
		goto exit_set;
	}
#endif
	ret = panel_bl_set_brightness(panel_bl, id, send_cmd);
	if (ret) {
		panel_err("failed to set_brightness (ret %d)\n", ret);
		goto exit_set;
	}

exit_set:
	return ret;
}

int _panel_update_brightness(struct panel_device *panel, u32 send_cmd)
{
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	int ret;

	panel_mutex_lock(&panel_bl->lock);
	panel_mutex_lock(&panel->op_lock);

	ret = _panel_update_brightness_nolock(panel, send_cmd);

	panel_mutex_unlock(&panel->op_lock);
	panel_mutex_unlock(&panel_bl->lock);

	return ret;
}

inline int panel_update_brightness(struct panel_device *panel)
{
	return _panel_update_brightness(panel, SEND_CMD);
}

inline int panel_update_brightness_cmd_skip(struct panel_device *panel)
{
	return _panel_update_brightness(panel, SKIP_CMD);
}

inline int panel_update_brightness_cmd_skip_nolock(struct panel_device *panel)
{
	return _panel_update_brightness_nolock(panel, SKIP_CMD);
}

static int panel_bl_update_status(struct backlight_device *bd)
{
	int ret;
	struct panel_bl_device *panel_bl = bl_get_data(bd);
	struct panel_device *panel = to_panel_device(panel_bl);

	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	ret = panel_update_brightness(panel);
	panel_wake_unlock(panel);

	return ret;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_bl_update_status,
};

static int panel_bl_thread(void *data)
{
	struct panel_bl_device *panel_bl = data;
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	struct panel_notifier_event_data evt_data;
#endif
	int ret, brightness, acl_state;
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	int mask_layer_br_hook;
#endif
	bool should_stop = false;

	while (!kthread_should_stop()) {
		brightness = panel_bl->props.brightness;
		acl_state = panel_bl->props.acl_pwrsave;
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
		mask_layer_br_hook = panel_bl->props.mask_layer_br_hook;
#endif
		ret = wait_event_interruptible(panel_bl->wq.wait,
				(should_stop = panel_bl->wq.should_stop || kthread_should_stop()) ||
				(brightness != panel_bl->props.brightness) ||
				(acl_state != panel_bl->props.acl_pwrsave)
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
				|| (mask_layer_br_hook != panel_bl->props.mask_layer_br_hook)
#endif
				);
		if (should_stop)
			break;

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
		evt_data.display_index = to_panel_device(panel_bl)->id;
		evt_data.d.bl.level = panel_bl->props.brightness;
		evt_data.d.bl.aor =
			(panel_bl->props.id == PANEL_BL_SUBDEV_TYPE_DISP) ?
			panel_bl->props.aor_ratio : 0;
		evt_data.d.bl.acl_status =
			(panel_bl->props.acl_pwrsave == ACL_PWRSAVE_OFF) ? 0 : 1;
		evt_data.d.bl.gradual_acl_val = panel_bl->props.acl_opr;
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
		evt_data.d.bl.finger_mask_hbm_on =
			(panel_bl->props.mask_layer_br_hook == MASK_LAYER_HOOK_ON);
#endif
		panel_notifier_call_chain(PANEL_EVENT_BL_STATE_CHANGED, &evt_data);
#endif
	}

	return 0;
}

static int panel_bl_create_thread(struct panel_bl_device *panel_bl)
{
	if (unlikely(!panel_bl)) {
		panel_warn("panel_bl unsupported\n");
		return 0;
	}

	init_waitqueue_head(&panel_bl->wq.wait);

	panel_bl->wq.should_stop = false;
	panel_bl->wq.thread = kthread_run(panel_bl_thread,
			panel_bl, "panel-bl-thread");
	if (IS_ERR_OR_NULL(panel_bl->wq.thread)) {
		panel_err("failed to run panel_bl thread\n");
		panel_bl->wq.thread = NULL;
		return PTR_ERR(panel_bl->wq.thread);
	}

	return 0;
}

static int panel_bl_destroy_thread(struct panel_bl_device *panel_bl)
{
	if (IS_ERR_OR_NULL(panel_bl->wq.thread))
		return 0;

	panel_bl->wq.should_stop = true;
	/* wake up waitqueue to stop */
	wake_up_interruptible_all(&panel_bl->wq.wait);
	/* kthread_should_stop() == true */
	kthread_stop(panel_bl->wq.thread);

	return 0;
}

__visible_for_testing int panel_bl_set_name(struct panel_bl_device *panel_bl, unsigned int id)
{
	if (!panel_bl)
		return -EINVAL;

	if (id == 0)
		snprintf(panel_bl->name, MAX_PANEL_BL_NAME_SIZE,
				"%s", PANEL_DEV_NAME);
	else
		snprintf(panel_bl->name, MAX_PANEL_BL_NAME_SIZE,
				"%s-%d", PANEL_DEV_NAME, id);

	return 0;
}

__visible_for_testing const char *panel_bl_get_name(struct panel_bl_device *panel_bl)
{
	return panel_bl ? panel_bl->name : NULL;
}

__visible_for_testing int panel_bl_init_property(struct panel_bl_device *panel_bl)
{
	struct panel_device *panel;
	int ret;

	if (!panel_bl)
		return -EINVAL;

	panel = to_panel_device(panel_bl);

	panel_bl->props.id = PANEL_BL_SUBDEV_TYPE_DISP;
	panel_bl->props.brightness = UI_DEF_BRIGHTNESS;
	panel_bl->props.prev_brightness = UI_DEF_BRIGHTNESS;
	panel_bl->props.acl_pwrsave = ACL_PWRSAVE_OFF;
	panel_bl->props.acl_opr = 1;
	panel_bl->props.smooth_transition = SMOOTH_TRANS_ON;

	ret = panel_add_property_from_array(panel,
			panel_bl_property_array,
			ARRAY_SIZE(panel_bl_property_array));
	if (ret < 0) {
		panel_err("failed to add prop array\n");
		return ret;
	}

	return 0;
}

int panel_bl_init(struct panel_bl_device *panel_bl)
{
	struct backlight_device *bd;
	struct panel_device *panel;
	int ret;

	if (!panel_bl)
		return -EINVAL;

	panel = to_panel_device(panel_bl);
	panel_mutex_init(panel, &panel_bl->lock);

	ret = panel_bl_set_name(panel_bl, panel->id);
	if (ret < 0)
		return -EINVAL;

	ret = panel_bl_init_property(panel_bl);
	if (ret < 0) {
		panel_err("failed to init property\n");
		return ret;
	}

	bd = backlight_device_register(panel_bl_get_name(panel_bl),
			panel->dev, panel_bl, &panel_backlight_ops, NULL);
	if (IS_ERR(bd)) {
		panel_err("failed to register backlight\n");
		return PTR_ERR(panel_bl->bd);
	}
	panel_bl->bd = bd;
	panel_bl->bd->props.brightness = UI_DEF_BRIGHTNESS;
	panel_bl->bd->props.max_brightness = UI_MAX_BRIGHTNESS;

	ret = panel_bl_create_thread(panel_bl);
	if (ret < 0) {
		panel_err("failed to create panel_bl thread\n");
		return ret;
	}

	panel_info("panel_bl init success\n");

	return 0;
}

int panel_bl_exit(struct panel_bl_device *panel_bl)
{
	if (!panel_bl)
		return -EINVAL;

	if (IS_ERR_OR_NULL(panel_bl->bd)) {
		panel_err("backlight device is null\n");
		return -EINVAL;
	}

	panel_mutex_lock(&panel_bl->lock);
	panel_bl_destroy_thread(panel_bl);
	backlight_device_unregister(panel_bl->bd);
	panel_bl->bd = NULL;
	memset(panel_bl->name, 0, sizeof(panel_bl->name));
	panel_mutex_unlock(&panel_bl->lock);

	return 0;
}

__visible_for_testing int panel_bl_init_backlight_device_property(struct panel_bl_device *panel_bl)
{
	if (!panel_bl)
		return -EINVAL;

	if (!panel_bl->bd) {
		panel_err("backlight_device is null\n");
		return -EINVAL;
	}

	panel_bl->bd->props.max_brightness =
		get_subdev_max_brightness(panel_bl, panel_bl->props.id);

	return 0;
}

#if defined(CONFIG_PANEL_BL_USE_BRT_CACHE)
__visible_for_testing int panel_bl_alloc_brt_cache(struct panel_bl_device *panel_bl)
{
	int id, size;

	if (!panel_bl)
		return -EINVAL;

	for (id = 0; id < MAX_PANEL_BL_SUBDEV; id++) {
		size = get_subdev_max_brightness(panel_bl, id) + 1;
		if (size <= 0) {
			panel_warn("invalid brightness table (size %d)\n", size);
			panel_bl->subdev[id].sz_brt_cache_tbl = -1;
			continue;
		}

		if (panel_bl->subdev[id].brt_cache_tbl) {
			panel_info("bl-%d free brt_cache\n", id);
			kfree(panel_bl->subdev[id].brt_cache_tbl);
			panel_bl->subdev[id].sz_brt_cache_tbl = 0;
		}

		panel_bl->subdev[id].brt_cache_tbl =
			kcalloc(size, sizeof(int), GFP_KERNEL);
		memset(panel_bl->subdev[id].brt_cache_tbl, 0xFF, size * sizeof(int));
		panel_bl->subdev[id].sz_brt_cache_tbl = size;
	}

	return 0;
}

__visible_for_testing int panel_bl_free_brt_cache(struct panel_bl_device *panel_bl)
{
	int id;

	if (!panel_bl)
		return -EINVAL;

	for (id = 0; id < MAX_PANEL_BL_SUBDEV; id++) {
		if (panel_bl->subdev[id].sz_brt_cache_tbl <= 0)
			continue;
		kfree(panel_bl->subdev[id].brt_cache_tbl);
	}

	return 0;
}
#else
static inline int panel_bl_alloc_brt_cache(struct panel_bl_device *panel_bl) { return 0; }
static inline int panel_bl_free_brt_cache(struct panel_bl_device *panel_bl) { return 0; }
#endif

int panel_bl_probe(struct panel_bl_device *panel_bl)
{
	struct panel_device *panel;
	int ret;

	if (!panel_bl)
		return -EINVAL;

	panel = to_panel_device(panel_bl);
	panel_mutex_lock(&panel_bl->lock);
	panel_mutex_lock(&panel->op_lock);
	ret = panel_bl_init_backlight_device_property(panel_bl);
	if (ret < 0) {
		panel_err("failed to init backlight device property\n");
		goto err;
	}

	ret = panel_bl_alloc_brt_cache(panel_bl);
	if (ret < 0) {
		panel_err("failed to alloc brt cache\n");
		goto err;
	}

	panel_info("panel_bl probe success\n");

err:
	panel_mutex_unlock(&panel->op_lock);
	panel_mutex_unlock(&panel_bl->lock);

	return ret;
}

int panel_bl_remove(struct panel_bl_device *panel_bl)
{
	struct panel_device *panel;

	if (!panel_bl)
		return -EINVAL;

	panel = to_panel_device(panel_bl);
	panel_mutex_lock(&panel_bl->lock);
	panel_bl_free_brt_cache(panel_bl);
	panel_mutex_unlock(&panel_bl->lock);

	panel_info("done\n");

	return 0;
}
