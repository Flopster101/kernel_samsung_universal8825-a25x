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

#ifndef _PHY_EXYNOS_SNPS_USBDP_H_
#define _PHY_EXYNOS_SNPS_USBDP_H_

extern void phy_exynos_snps_usbdp_phy_initiate(struct exynos_usbphy_info *info);
extern int phy_exynos_snps_usbdp_phy_enable(struct exynos_usbphy_info *info);
extern void phy_exynos_snps_usbdp_phy_disable(struct exynos_usbphy_info *info);
extern void phy_exynos_snps_usbdp_tune_each(struct exynos_usbphy_info *info, char *name, int val);
extern void phy_exynos_snps_usbdp_tune_each_cr(struct exynos_usbphy_info *info, char *name, int val);
extern u16 phy_exynos_snps_usbdp_cr_read(struct exynos_usbphy_info *info, u16 addr);
extern void phy_exynos_snps_usbdp_cr_write(struct exynos_usbphy_info *info, u16 addr, u16 data);
extern void phy_exynos_snps_usbdp_tune(struct exynos_usbphy_info *info);

#endif
