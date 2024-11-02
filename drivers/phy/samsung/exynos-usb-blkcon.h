/* SPDX-License-Identifier: GPL-2.0 */
/**
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *        http://www.samsung.com
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _USB_USBPHY_CAL_PHY_EXYNOS_USBCON_H_
#define _USB_USBPHY_CAL_PHY_EXYNOS_USBCON_H_

extern void exynos_usbcon_init_link(struct exynos_usbphy_info *cal_info);
extern void exynos_usbcon_detach_pipe3_phy(struct exynos_usbphy_info *cal_info);
extern void exynos_usbcon_disable_pipe3_phy(struct exynos_usbphy_info *cal_info);
extern void exynos_usbcon_ready_to_pipe3_phy(struct exynos_usbphy_info *cal_info);
extern u64 exynos_usbcon_get_logic_trace(struct exynos_usbphy_info *cal_info);
extern void exynos_usbcon_set_fsv_out_en(struct exynos_usbphy_info *cal_info, u32 en);
extern u8 exynos_usbcon_get_fs_vplus_vminus(struct exynos_usbphy_info *cal_info);

/* Remote Wake-up advisor APIs*/
extern int exynos_usbcon_enable_rewa(struct exynos_usbphy_info *cal_info);
extern int exynos_usbcon_rewa_req_sys_valid(struct exynos_usbphy_info *cal_info);
extern int exynos_usbcon_rewa_disable(struct exynos_usbphy_info *info);
extern int exynos_usbcon_rewa_cancel(struct exynos_usbphy_info *info);
extern void exynos_usbcon_u3_rewa_enable(struct exynos_usbphy_info *cal_info, int lfps_overlap_mode);
extern void exynos_usbcon_u3_rewa_disable(struct exynos_usbphy_info *cal_info);

#endif
