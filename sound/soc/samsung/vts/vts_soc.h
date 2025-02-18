/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_soc_v1.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_DEP_SOC_H
#define __SND_SOC_VTS_DEP_SOC_H

#include "vts.h"

#define VTS_SOC_VERSION(m, n, r) (((m) << 16) | ((n) << 8) | (r))

#if (VTS_SOC_VERSION(3, 0, 0) <= CONFIG_SND_SOC_SAMSUNG_VTS_VERSION)
#include "vts_soc_v3.h"
#elif (VTS_SOC_VERSION(2, 1, 0) <= CONFIG_SND_SOC_SAMSUNG_VTS_VERSION)
#include "vts_soc_v2_1.h"
#elif (VTS_SOC_VERSION(2, 0, 0) <= CONFIG_SND_SOC_SAMSUNG_VTS_VERSION)
#include "vts_soc_v2.h"
#elif (VTS_SOC_VERSION(1, 0, 0) <= CONFIG_SND_SOC_SAMSUNG_VTS_VERSION)
#include "vts_soc_v1.h"
#else
#include "vts_soc_v1.h"
#endif

/* interface functions */
void vts_cpu_power(struct device *dev, bool on);
int vts_cpu_enable(struct device *dev, bool enable);
void vts_reset_cpu(struct device *dev);

void vts_pad_retention(bool retention);
u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size);

int vts_acquire_sram(struct platform_device *pdev, int vts);
int vts_release_sram(struct platform_device *pdev, int vts);
int vts_clear_sram(struct platform_device *pdev);

int vts_soc_runtime_resume(struct device *dev);
int vts_soc_runtime_suspend(struct device *dev);

int vts_soc_cmpnt_probe(struct device *dev);
int vts_soc_probe(struct device *dev);
void vts_soc_remove(struct device *dev);
#endif /* __SND_SOC_VTS_DEP_SOC_H */
