/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OLED_COMMON_COND_H__
#define __OLED_COMMON_COND_H__

#include <linux/types.h>
#include <linux/kernel.h>

bool oled_cond_is_panel_display_mode_changed(struct panel_device *panel);
bool oled_cond_is_panel_refresh_rate_changed(struct panel_device *panel);
bool oled_cond_is_panel_refresh_mode_changed(struct panel_device *panel);
bool oled_cond_is_panel_refresh_mode_ns(struct panel_device *panel);
bool oled_cond_is_panel_refresh_mode_hs(struct panel_device *panel);
bool oled_cond_is_panel_state_lpm(struct panel_device *panel);
bool oled_cond_is_panel_state_not_lpm(struct panel_device *panel);
bool oled_cond_is_first_set_bl(struct panel_device *panel);
bool oled_cond_is_panel_mres_updated(struct panel_device *panel);
bool oled_cond_is_panel_mres_updated_bigger(struct panel_device *panel);
bool oled_cond_is_acl_pwrsave_on(struct panel_device *panel);

#endif /* __OLED_COMMON_COND_H__ */
