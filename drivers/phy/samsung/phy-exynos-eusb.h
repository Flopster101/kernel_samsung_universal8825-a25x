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

#ifndef _PHY_EXYNOS_EUSB_H_
#define _PHY_EXYNOS_EUSB_H_

extern void phy_exynos_eusb_reset(struct exynos_usbphy_info *info);
extern void phy_exynos_eusb_initiate(struct exynos_usbphy_info *info);
extern u8 phy_exynos_eusb_get_eusb_state(struct exynos_usbphy_info *info);
extern void phy_exynos_eusb_terminate(struct exynos_usbphy_info *info);

#endif
