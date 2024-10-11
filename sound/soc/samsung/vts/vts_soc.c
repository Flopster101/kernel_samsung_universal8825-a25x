// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

void vts_cpu_power(struct device *dev, bool on)
{
	pr_info("%s(%d)\n", __func__, on);
}

int vts_cpu_enable(struct device *dev, bool enable)
{
	return 0;
}

void vts_reset_cpu(struct device *dev)
{
	pr_info("%s\n", __func__);
}

void vts_pad_retention(bool retention)
{
	pr_info("%s(%d)\n", __func__, retention);
}
EXPORT_SYMBOL(vts_pad_retention);

u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size)
{
	pr_info("%s\n", __func__);

	return 0;
}

int vts_acquire_sram(struct platform_device *pdev, int vts)
{
	pr_info("%s\n", __func__);

	return 0;
}
EXPORT_SYMBOL(vts_acquire_sram);

int vts_release_sram(struct platform_device *pdev, int vts)
{
	pr_info("%s\n", __func__);

	return 0;
}
EXPORT_SYMBOL(vts_release_sram);

int vts_clear_sram(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	return 0;
}
EXPORT_SYMBOL(vts_clear_sram);

int vts_soc_runtime_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
}

int vts_soc_runtime_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);
}

int vts_soc_cmpt_probe(struct device *dev)
{
	pr_info("%s\n", __func__);
}

int vts_soc_probe(struct device *dev)
{
	pr_info("%s\n", __func__);
}

void vts_soc_remove(struct device *dev)
{
	pr_info("%s\n", __func__);
}
