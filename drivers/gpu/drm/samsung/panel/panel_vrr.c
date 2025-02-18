// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "panel_kunit.h"
#include "panel.h"
#include "panel_drv.h"
#include "panel_vrr.h"
#include "panel_debug.h"

const char *vrr_lfd_client_name[MAX_VRR_LFD_CLIENT] = {
	[VRR_LFD_CLIENT_FAC] = "fac",
	[VRR_LFD_CLIENT_DISP] = "disp",
	[VRR_LFD_CLIENT_INPUT] = "input",
	[VRR_LFD_CLIENT_AOD] = "aod",
	[VRR_LFD_CLIENT_VID] = "vid",
};

const char *vrr_lfd_scope_name[MAX_VRR_LFD_SCOPE] = {
	[VRR_LFD_SCOPE_NORMAL] = "normal",
	[VRR_LFD_SCOPE_HMD] = "hmd",
	[VRR_LFD_SCOPE_LPM] = "lpm",
};

const char *get_vrr_lfd_client_name(int index)
{
	if (index < 0 || index >= MAX_VRR_LFD_CLIENT)
		return NULL;

	return vrr_lfd_client_name[index];
}

int find_vrr_lfd_client_name(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VRR_LFD_CLIENT; i++)
		if (!strcmp(vrr_lfd_client_name[i], name))
			break;

	if (i == MAX_VRR_LFD_CLIENT)
		return -EINVAL;

	return i;
}

const char *get_vrr_lfd_scope_name(int index)
{
	if (index < 0 || index >= MAX_VRR_LFD_SCOPE)
		return NULL;

	return vrr_lfd_scope_name[index];
}

int find_vrr_lfd_scope_name(const char *name)
{
	int i;

	if (name == NULL)
		return -EINVAL;

	for (i = 0; i < MAX_VRR_LFD_SCOPE; i++)
		if (!strcmp(vrr_lfd_scope_name[i], name))
			break;

	if (i == MAX_VRR_LFD_SCOPE)
		return -EINVAL;

	return i;
}

int update_vrr_lfd(struct panel_device *panel)
{
	int i, scope;
	u32 lfd_fix, lfd_min, lfd_max, lfd_scalability;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_info *prev_vrr_lfd_info = &props->prev_vrr_lfd_info;
	struct vrr_lfd_info *vrr_lfd_info = &props->vrr_lfd_info;
	int updated = VRR_LFD_NOT_UPDATED;

	/* fix */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_fix = VRR_LFD_FREQ_NONE;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].fix != VRR_LFD_FREQ_NONE) {
				lfd_fix = vrr_lfd_info->req[i][scope].fix;
				panel_info("client:%s scope:%s fix:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].fix);
				break;
			}
		}

		panel_set_property(panel, &vrr_lfd_info->cur[scope].fix, lfd_fix);
		if (prev_vrr_lfd_info->cur[scope].fix != vrr_lfd_info->cur[scope].fix) {
			panel_info("scope:%s fix:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					prev_vrr_lfd_info->cur[scope].fix,
					vrr_lfd_info->cur[scope].fix);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* scalability */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_scalability = VRR_LFD_SCALABILITY_MAX;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			/* scalability changed */
			if (vrr_lfd_info->req[i][scope].scalability != VRR_LFD_SCALABILITY_NONE) {
				lfd_scalability = min(lfd_scalability,
						vrr_lfd_info->req[i][scope].scalability);
				panel_info("client:%s scope:%s scalability:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].scalability);
			}
		}

		vrr_lfd_info->cur[scope].scalability =
			(lfd_scalability == VRR_LFD_SCALABILITY_MAX) ?
			VRR_LFD_SCALABILITY_NONE : lfd_scalability;
		if (prev_vrr_lfd_info->cur[scope].scalability !=
				vrr_lfd_info->cur[scope].scalability) {
			panel_info("scope:%s scalability:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					prev_vrr_lfd_info->cur[scope].scalability,
					vrr_lfd_info->cur[scope].scalability);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* min */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_min = 0;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].min != 0)
				panel_info("client:%s scope:%s min:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].min);
			lfd_min = max(lfd_min, vrr_lfd_info->req[i][scope].min);
		}

		vrr_lfd_info->cur[scope].min = lfd_min;
		if (prev_vrr_lfd_info->cur[scope].min !=
				vrr_lfd_info->cur[scope].min) {
			panel_info("scope:%s min:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					prev_vrr_lfd_info->cur[scope].min,
					vrr_lfd_info->cur[scope].min);
			updated = VRR_LFD_UPDATED;
		}
	}

	/* max */
	for (scope = 0; scope < MAX_VRR_LFD_SCOPE; scope++) {
		lfd_max = 0;
		for (i = 0; i < MAX_VRR_LFD_CLIENT; i++) {
			if (vrr_lfd_info->req[i][scope].max != 0)
				panel_info("client:%s scope:%s max:%d\n",
						get_vrr_lfd_client_name(i),
						get_vrr_lfd_scope_name(scope),
						vrr_lfd_info->req[i][scope].max);
			lfd_max = max(lfd_max, vrr_lfd_info->req[i][scope].max);
		}

		vrr_lfd_info->cur[scope].max = lfd_max;
		if (prev_vrr_lfd_info->cur[scope].max !=
				vrr_lfd_info->cur[scope].max) {
			panel_info("scope:%s max:%d->%d\n",
					get_vrr_lfd_scope_name(scope),
					prev_vrr_lfd_info->cur[scope].max,
					vrr_lfd_info->cur[scope].max);
			updated = VRR_LFD_UPDATED;
		}
	}

	memcpy(prev_vrr_lfd_info,
			vrr_lfd_info, sizeof(*prev_vrr_lfd_info));

	return updated;
}

bool panel_vrr_is_supported(struct panel_device *panel)
{
	struct panel_info *data;

	if (!panel)
		return false;

	data = &panel->panel_data;

	panel_dbg("support_vrr %s vrrtbl %s nr_vrrtbl %d\n",
			data->ddi_props.support_vrr ? "true" : "false",
			data->vrrtbl == NULL ? "is null" : "is not null",
			data->nr_vrrtbl);

	if (!data->ddi_props.support_vrr)
		return false;

	if (data->vrrtbl == NULL || data->nr_vrrtbl < 1)
		return false;

	return true;
}

bool panel_vrr_lfd_is_supported(struct panel_device *panel)
{
	struct panel_info *data;

	if (!panel)
		return false;

	data = &panel->panel_data;

	return (panel_vrr_is_supported(panel) && data->ddi_props.support_vrr_lfd);
}

bool panel_vrr_is_valid(struct panel_device *panel)
{
	struct panel_info *data = &panel->panel_data;
	struct panel_properties *props = &data->props;

	if (panel_vrr_is_supported(panel) == false)
		return false;

	if (data->nr_vrrtbl <= props->vrr_idx) {
		panel_warn("vrr_idx(%d) exceed number of vrrtbl(%d)\n",
				props->vrr_idx, data->nr_vrrtbl);
		return false;
	}

	return true;
}

__mockable struct panel_vrr *get_panel_vrr(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

	if (panel_vrr_is_valid(panel) == false)
		return NULL;

	return panel->panel_data.vrrtbl[props->vrr_idx];
}
EXPORT_SYMBOL(get_panel_vrr);

__mockable int get_panel_refresh_rate(struct panel_device *panel)
{
	struct panel_vrr *vrr;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	return vrr->fps;
}
EXPORT_SYMBOL(get_panel_refresh_rate);

__mockable int get_panel_refresh_mode(struct panel_device *panel)
{
	struct panel_vrr *vrr;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	return vrr->mode;
}
EXPORT_SYMBOL(get_panel_refresh_mode);
