// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Gwanghui Lee <Gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include "panel_kunit.h"
#include "panel_modes.h"

static const char *refresh_mode_names[MAX_REFRESH_MODE] = {
	[REFRESH_MODE_NS] = "ns",
	[REFRESH_MODE_HS] = "hs",
	[REFRESH_MODE_PASSIVE_HS] = "phs"
};

__visible_for_testing int panel_mode_snprintf(const struct panel_display_mode *mode, char *buf, size_t size)
{
	if (!mode || !buf || !size)
		return 0;

	return snprintf(buf, size, "Panel Modeline " PANEL_MODE_FMT "\n", PANEL_MODE_ARG(mode));
}

void panel_mode_debug_printmodeline(const struct panel_display_mode *mode)
{
	char buf[128];

	panel_mode_snprintf(mode, buf, sizeof(buf));
	pr_debug("%s", buf);
}
EXPORT_SYMBOL(panel_mode_debug_printmodeline);

void panel_mode_info_printmodeline(const struct panel_display_mode *mode)
{
	char buf[128];

	panel_mode_snprintf(mode, buf, sizeof(buf));
	pr_info("%s", buf);
}
EXPORT_SYMBOL(panel_mode_info_printmodeline);

const char *refresh_mode_to_str(int refresh_mode)
{
	if (refresh_mode >= MAX_REFRESH_MODE)
		return NULL;

	return refresh_mode_names[refresh_mode];
}
EXPORT_SYMBOL(refresh_mode_to_str);

int str_to_refresh_mode(const char *str)
{
	int i;

	if (!str)
		return -EINVAL;

	for (i = 0; i < (int)ARRAY_SIZE(refresh_mode_names); i++)
		if (!strncmp(str, refresh_mode_names[i],
					strlen(refresh_mode_names[i]) + 1))
			return i;

	return -1;
}
EXPORT_SYMBOL(str_to_refresh_mode);

int panel_mode_vscan(const struct panel_display_mode *mode)
{
	if (!mode)
		return -EINVAL;

	return max_t(typeof(mode->panel_te_sw_skip_count), mode->panel_te_sw_skip_count + 1, 1) *
		max_t(typeof(mode->panel_te_hw_skip_count), mode->panel_te_hw_skip_count + 1, 1);
}
EXPORT_SYMBOL(panel_mode_vscan);

/**
 * of_parse_panel_display_mode - parse panel_display_mode entry from device_node
 * @np: device_node with the properties
 **/
static int of_parse_panel_display_mode(const struct device_node *np,
		struct panel_display_mode *pdm)
{
	int ret = 0;
	unsigned int val = 0;
	const char *name;

	memset(pdm, 0, sizeof(*pdm));

	ret |= of_property_read_string(np, "id", &name);
	strncpy(pdm->name, name, PANEL_DISPLAY_MODE_NAME_LEN - 1);
	pdm->name[PANEL_DISPLAY_MODE_NAME_LEN - 1] = '\0';
	ret |= of_property_read_u32(np, "width", &pdm->width);
	ret |= of_property_read_u32(np, "height", &pdm->height);
	ret |= of_property_read_u32(np, "refresh_rate", &pdm->refresh_rate);
	ret |= of_property_read_u32(np, "refresh_mode", &pdm->refresh_mode);
	ret |= of_property_read_u32(np, "panel_refresh_rate", &pdm->panel_refresh_rate);
	ret |= of_property_read_u32(np, "panel_refresh_mode", &pdm->panel_refresh_mode);
	ret |= of_property_read_u32(np, "panel_te_st", &pdm->panel_te_st);
	ret |= of_property_read_u32(np, "panel_te_ed", &pdm->panel_te_ed);
	ret |= of_property_read_u32(np, "panel_te_sw_skip_count", &pdm->panel_te_sw_skip_count);
	ret |= of_property_read_u32(np, "panel_te_hw_skip_count", &pdm->panel_te_hw_skip_count);
	ret |= of_property_read_u32(np, "dsc_en", &val);
	pdm->dsc_en = !!val;
	ret |= of_property_read_u32(np, "dsc_cnt", &pdm->dsc_cnt);
	ret |= of_property_read_u32(np, "dsc_slice_num", &pdm->dsc_slice_num);
	ret |= of_property_read_u32(np, "dsc_slice_w", &pdm->dsc_slice_w);
	ret |= of_property_read_u32(np, "dsc_slice_h", &pdm->dsc_slice_h);
	ret |= of_property_read_u32(np, "cmd_lp_ref", &pdm->cmd_lp_ref);
	pdm->panel_video_mode = of_property_read_bool(np, "panel_video_mode");
	ret |= of_property_read_u32_array(np, "panel_h_porch", pdm->panel_hporch, MAX_PANEL_H_PORCH);
	ret |= of_property_read_u32_array(np, "panel_v_porch", pdm->panel_vporch, MAX_PANEL_V_PORCH);
	of_property_read_u32(np, "disp_qos_fps", &pdm->disp_qos_fps);	/* optional */

	if (ret) {
		pr_err("%pOF: error reading panel_display_mode mode properties\n", np);
		return -EINVAL;
	}

	return 0;
}

void panel_display_modes_release(struct panel_display_modes *disp)
{
	if (disp->modes) {
		unsigned int i;

		for (i = 0; i < disp->num_modes; i++)
			kfree(disp->modes[i]);
		kfree(disp->modes);
	}
	kfree(disp);
}
EXPORT_SYMBOL_GPL(panel_display_modes_release);

/**
 * of_get_panel_display_modes - parse all panel_display_mode entries from a device_node
 * @np: device_node with the subnodes
 **/
struct panel_display_modes *of_get_panel_display_modes(const struct device_node *np)
{
	struct device_node *entry;
	struct device_node *native_mode;
	struct panel_display_modes *disp;
	struct of_phandle_iterator iter;
	int err;

	if (!np)
		return NULL;

	disp = kzalloc(sizeof(*disp), GFP_KERNEL);
	if (!disp) {
		pr_err("%pOF: could not allocate struct panel_display_modes'\n", np);
		return NULL;
	}

	entry = of_parse_phandle(np, "native-mode", 0);
	/* assume first child as native mode if none provided */
	if (!entry)
		entry = of_get_next_child(np, NULL);
	/* if there is no child, it is useless to go on */
	if (!entry) {
		pr_err("%pOF: no panel mode specifications given\n", np);
		goto entryfail;
	}

	pr_debug("%pOF: using %s as default panel mode\n", np, entry->name);

	native_mode = entry;

	disp->num_modes = of_get_child_count(np);
	if (disp->num_modes == 0) {
		/* should never happen, as entry was already found above */
		pr_err("%pOF: no modes specified\n", np);
		goto entryfail;
	}

	disp->modes = kcalloc(disp->num_modes,
				sizeof(struct panel_display_mode *),
				GFP_KERNEL);
	if (!disp->modes) {
		pr_err("%pOF: could not allocate modes array\n", np);
		goto entryfail;
	}

	disp->num_modes = 0;
	disp->native_mode = 0;

	of_for_each_phandle(&iter, err, np, "exposed-mode", NULL, 0) {
		struct panel_display_mode *pdm;
		int r;

		pdm = kzalloc(sizeof(*pdm), GFP_KERNEL);
		if (!pdm) {
			pr_err("%pOF: could not allocate panel_display_mode struct\n",
				np);
			goto modefail;
		}

		r = of_parse_panel_display_mode(iter.node, pdm);
		if (r) {
			/*
			 * to not encourage wrong devicetrees, fail in case of
			 * an error
			 */
			pr_err("%pOF: error in mode %d\n",
				np, disp->num_modes + 1);
			kfree(pdm);
			goto modefail;
		}

		if (native_mode == iter.node)
			disp->native_mode = disp->num_modes;

		disp->modes[disp->num_modes] = pdm;
		disp->num_modes++;
		panel_mode_info_printmodeline(pdm);
	}
	/*
	 * native_mode points to the device_node returned by of_parse_phandle
	 * therefore call of_node_put on it
	 */
	of_node_put(native_mode);

	pr_debug("%pOF: got %d modes. Using mode #%d as default\n",
		np, disp->num_modes,
		disp->native_mode + 1);

	return disp;

modefail:
	of_node_put(native_mode);
	panel_display_modes_release(disp);
	disp = NULL;
entryfail:
	kfree(disp);
	return NULL;
}
EXPORT_SYMBOL_GPL(of_get_panel_display_modes);

void panel_mode_set_name(struct panel_display_mode *mode)
{
	snprintf(mode->name, PANEL_DISPLAY_MODE_NAME_LEN, "%dx%d@%d%s",
			mode->width, mode->height, mode->refresh_rate,
			(mode->refresh_mode == REFRESH_MODE_NS) ? "NS" : "HS");
}
EXPORT_SYMBOL(panel_mode_set_name);

struct panel_display_mode *panel_mode_create(void)
{
	struct panel_display_mode *nmode;

	nmode = kzalloc(sizeof(struct panel_display_mode), GFP_KERNEL);
	if (!nmode)
		return NULL;

	return nmode;
}
EXPORT_SYMBOL(panel_mode_create);
