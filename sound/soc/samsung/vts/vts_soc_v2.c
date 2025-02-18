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

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <sound/samsung/vts.h>
#include <linux/firmware.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/delay.h>

#include "vts_soc.h"
#include "vts_soc_v2.h"
#include "vts_dbg.h"

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))

void vts_cpu_power(struct device *dev, bool on)
{
	vts_dev_info(dev, "%s(%d)\n", __func__, on);

	exynos_pmu_update(VTS_CPU_CONFIGURATION, VTS_CPU_LOCAL_PWR_CFG,
		on ? VTS_CPU_LOCAL_PWR_CFG : 0);
}

#define VTS_WFI_VAL (0x19A006)
#define VTS_WFI_MASK (0xFFFFFF)
#define VTS_WFI_CNT (5)
int vts_cpu_enable(struct device *dev, bool enable)
{
/* WFI status register is removed located in VTS_CPU_IN */
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int status = 0;
	unsigned int status_prev = 0;
	unsigned int hit_rate = 0;
	unsigned long after;

	after = jiffies + LIMIT_IN_JIFFIES;
#if 0
	do {
		schedule();
		exynos_pmu_read(VTS_CPU_IN, &status);
	} while (((status & VTS_CPU_IN_SLEEPING_MASK)
		!= VTS_CPU_IN_SLEEPING_MASK)
		&& time_is_after_eq_jiffies(after));
	if (time_is_before_jiffies(after)) {
		vts_err("vts cpu enable timeout\n");
		return -ETIME;
	}
#else
	do {
		msleep(1);
		status_prev = status;
		status = readl(data->gpr_base + 0x0);
		if (((status & VTS_WFI_MASK) == VTS_WFI_VAL))
		/* if (((status & VTS_WFI_MASK) == status_prev)) */
			hit_rate = VTS_WFI_CNT + 2;
		else
			hit_rate ++;

		/* if (hit_rate >= VTS_WFI_CNT) */
			vts_dev_info(dev, "YAMIN PC : 0x%x 0x%x\n",
				status,
				status_prev);
	} while (time_is_after_eq_jiffies(after) &&
			hit_rate < VTS_WFI_CNT);

	if (time_is_before_jiffies(after)) {
		vts_err("[VTS] cpu disable timeout\n");
		return -ETIME;
	}
#endif

	return 0;
}

void vts_reset_cpu(struct device *dev)
{
	vts_cpu_enable(dev, false);
	vts_cpu_power(dev, false);
	vts_cpu_enable(dev, true);
	vts_cpu_power(dev, true);
}

void vts_pad_retention(bool retention)
{
	if (!retention) {
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x1 << 11, 0x1 << 11);
	}
}
EXPORT_SYMBOL(vts_pad_retention);

u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size)
{
	u32 aligned_size = round_up(size, SZ_4M);
	u64 aligned_base = round_down(base, aligned_size);

	if (IS_ENABLED(CONFIG_SOC_S5E9925)) {
		if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
			/* EVT0 */
			/* set VTS BAAW config */
			writel(0x40100, sfr_base + 0x0);
			writel(0x40200, sfr_base + 0x4);
			writel(0x15900, sfr_base + 0x8);
			writel(0x80000003, sfr_base + 0xC);

			writel(0x40200, sfr_base + 0x10);
			writel(0x40300, sfr_base + 0x14);
			writel(0x14F00, sfr_base + 0x18);
			writel(0x80000003, sfr_base + 0x1C);

			pr_info("[vts] %s %d 0x%x\n", __func__, __LINE__,
				aligned_base / SZ_4K);
			/* GUIDE: writel(0x060000, sfr_base + 0x20); */
			writel(VTS_BAAW_BASE / SZ_4K, sfr_base + 0x20);
			/* GUIDE: writel(0x100000, sfr_base + 0x24); */
			writel((VTS_BAAW_BASE + aligned_size) / SZ_4K,
					sfr_base + 0x24);
			/* GUIDE: writel(0x060000, sfr_base + 0x28); */
			writel(aligned_base / SZ_4K, sfr_base + 0x28);
			writel(0x80000003, sfr_base + 0x2C);
		} else {
			/* EVT1 */
			/* set VTS BAAW config */
			/* VTS SRAM */
			writel(0x000000, sfr_base + 0x0);
			writel(0x000204, sfr_base + 0x4);
			writel(0x002500, sfr_base + 0x8);
			writel(0x80000003, sfr_base + 0xC);

			/* CHUB SRAM */
			writel(0x000300, sfr_base + 0x10);
			writel(0x000480, sfr_base + 0x14);
			writel(0x002800, sfr_base + 0x18);
			writel(0x80000003, sfr_base + 0x1C);

			/* ALIVE MAILBOX, CHUB_RTC */
			writel(0x040000, sfr_base + 0x20);
			writel(0x040100, sfr_base + 0x24);
			writel(0x014C00, sfr_base + 0x28);
			writel(0x80000003, sfr_base + 0x2C);

			/* CMGP SFR */
			writel(0x040100, sfr_base + 0x30);
			writel(0x040300, sfr_base + 0x34);
			writel(0x014E00, sfr_base + 0x38);
			writel(0x80000003, sfr_base + 0x3C);

			/* VTS SFR */
			writel(0x041000, sfr_base + 0x40);
			writel(0x0411F0, sfr_base + 0x44);
			writel(0x015300, sfr_base + 0x48);
			writel(0x80000003, sfr_base + 0x4C);

			/* CHUBVTS SFR */
			writel(0x042000, sfr_base + 0x50);
			writel(0x042030, sfr_base + 0x54);
			writel(0x0155D0, sfr_base + 0x58);
			writel(0x80000003, sfr_base + 0x5C);

			/* DRAM */
			pr_info("[vts] %s %d 0x%x\n", __func__, __LINE__,
				aligned_base / SZ_4K);
			/* GUIDE: writel(0x060000, sfr_base + 0x60); */
			writel(VTS_BAAW_BASE / SZ_4K, sfr_base + 0x60);
			/* GUIDE: writel(0x100000, sfr_base + 0x64); */
			writel((VTS_BAAW_BASE + aligned_size) / SZ_4K,
					sfr_base + 0x64);
			/* GUIDE: writel(0x060000, sfr_base + 0x68); */
			writel(aligned_base / SZ_4K, sfr_base + 0x68);
			writel(0x80000003, sfr_base + 0x6C);
		}
	}

	return base - aligned_base + VTS_BAAW_BASE;
}

int vts_acquire_sram(struct platform_device *pdev, int vts)
{
	return 0;
}
EXPORT_SYMBOL(vts_acquire_sram);

int vts_release_sram(struct platform_device *pdev, int vts)
{
	return 0;
}
EXPORT_SYMBOL(vts_release_sram);

int vts_clear_sram(struct platform_device *pdev)
{
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_info("%s\n", __func__);

	memset(data->sram_base, 0, data->sram_size);

	return 0;
}
EXPORT_SYMBOL(vts_clear_sram);

static void vts_soc_complete_fw_request(
	const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int *pversion = NULL;

	if (!fw) {
		vts_dev_err(dev, "Failed to request firmware\n");
		return;
	}

	data->firmware = fw;
	pversion = (unsigned int *) (fw->data + VTSFW_VERSION_OFFSET);
	/* data->vtsfw_version = *pversion; */
	pversion = (unsigned int *) (fw->data + DETLIB_VERSION_OFFSET);
	/* data->vtsdetectlib_version = *pversion; */

	vts_dev_info(dev, "firmware loaded at %p (%zu)\n", fw->data, fw->size);
}

int vts_soc_runtime_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	int clk_val = 49152000;
	const char* fw_name;
	int ret = 0;

	vts_dev_info(dev, "%s\n", __func__);

#if IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)
	clk_val = 73728000;
#endif

	if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
		vts_dev_info(dev, "%s: EVT0 \n", __func__);
		fw_name = "vts_evt0.bin";
	} else {
		fw_name = "vts.bin";
	}

	if (data->clk_slif_src) {
		vts_dev_info(dev, "clk_slif_src bef:%d :%d\n",
			clk_val, clk_get_rate(data->clk_slif_src));

		ret = clk_set_rate(data->clk_slif_src, 76800000);
		if (ret < 0)
			vts_dev_err(dev, "clk_slif_src: %d\n", ret);

		vts_dev_info(dev, "clk_slif_src aft:%d\n",
				clk_get_rate(data->clk_slif_src));

		ret = clk_enable(data->clk_slif_src);
		if (ret < 0) {
			vts_dev_err(dev, "Failed to en slif_src:%d\n", ret);
			return ret;
		}
	}

	if (data->clk_slif_src1) {
		vts_dev_info(dev, "clk_slif_src1 bef:%d :%d\n",
			clk_val, clk_get_rate(data->clk_slif_src1));

		vts_dev_info(dev, "clk_slif_src1 aft:%d\n",
				clk_get_rate(data->clk_slif_src1));

		ret = clk_enable(data->clk_slif_src1);
		if (ret < 0) {
			vts_dev_err(dev, "Failed to en slif_src1:%d\n", ret);
			return ret;
		}
	}

	if (data->clk_slif_src2) {
		vts_dev_info(dev, "clk_slif_src2 bef:%d :%d\n",
			clk_val, clk_get_rate(data->clk_slif_src2));

		vts_dev_info(dev, "clk_slif_src2 aft:%d\n",
				clk_get_rate(data->clk_slif_src2));

		ret = clk_enable(data->clk_slif_src2);
		if (ret < 0) {
			vts_dev_err(dev, "Failed to en slif_src2:%d\n", ret);
			return ret;
		}
	} else {
		vts_dev_warn(dev, "clk_slif_src2 is null\n",
			clk_get_rate(data->clk_slif_src2));
	}

	if (data->clk_dmic_sync) {
		ret = clk_enable(data->clk_dmic_sync);

		if (ret < 0) {
			vts_dev_err(dev, "Failed to enable the clock\n");
			return ret;
		}
	} else {
		vts_dev_info(dev, "%s clk_dmic_sync is NULL\n", __func__);
	}

	vts_dev_info(dev, "dmic clock rate:%lu\n",
			clk_get_rate(data->clk_dmic_sync));

	if (data->clk_sys_mux) {
		ret = clk_set_rate(data->clk_sys_mux, VTS_SYS_CLOCK_MAX);
		if (ret < 0) {
			dev_err(dev, "clk_sys_mux: %d\n", ret);
			return ret;
		}
	} else {
		vts_dev_info(dev, "%s clk_sys_mux is NULL\n", __func__);
	}

	if (data->clk_sys) {
		if (data->target_sysclk == 0)
			ret = clk_set_rate(data->clk_sys, VTS_SYS_CLOCK);
		else
			ret = clk_set_rate(data->clk_sys, data->target_sysclk);

		if (ret < 0) {
			dev_err(dev, "clk_sys: %d\n", ret);
			return ret;
		}
	} else {
		vts_dev_info(dev, "%s clk_sys is NULL\n", __func__);
	}

	data->sysclk_rate = clk_get_rate(data->clk_sys);
	vts_dev_info(dev, "System Clock : %ld\n", data->sysclk_rate);

	/* SRAM intmem */
	if (data->intmem_code)
		writel(0x03FF0000, data->intmem_code + 0x4);
	if (data->intmem_data)
		writel(0x03FF0000, data->intmem_data + 0x4);
	if (data->intmem_pcm)
		writel(0x03FF0000, data->intmem_pcm + 0x4);
	if (data->intmem_data1)
		writel(0x03FF0000, data->intmem_data1 + 0x4);

	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct: %s\n",
			fw_name,
			__func__);
		ret = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			fw_name, dev);

		if (ret < 0) {
			vts_dev_err(dev, "request_firmware_direct failed\n");
			return ret;
		}
		vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
		vts_soc_complete_fw_request(data->firmware, dev);
	}

	return ret;
}

#define YAMIN_MCU_VTS_QCH_CLKIN (0x70e8)
int vts_soc_runtime_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
#if 0
	volatile unsigned long yamin_mcu_vts_qch_clkin;
	unsigned int status = 0;

	yamin_mcu_vts_qch_clkin =
		(volatile unsigned long)ioremap_wt(0x15507000, 0x100);

	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n",
			readl((volatile void *)(yamin_mcu_vts_qch_clkin
					+ 0xe8)));
	iounmap((volatile void __iomem *)yamin_mcu_vts_qch_clkin);

	exynos_pmu_read(YAMIN_MCU_VTS_QCH_CLKIN, &status);
	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n", status);
#endif

	vts_dev_info(dev, "%s\n", __func__);

	if (data->clk_dmic_sync)
		clk_disable(data->clk_dmic_sync);

	if (data->clk_slif_src2)
		clk_disable(data->clk_slif_src2);

	if (data->clk_slif_src1)
		clk_disable(data->clk_slif_src1);

	if (data->clk_slif_src)
		clk_disable(data->clk_slif_src);

	return 0;
}

int vts_soc_cmpnt_probe(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	const char* fw_name;
	int ret;

	vts_dev_info(dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
		vts_dev_info(dev, "%s: EVT0 \n", __func__);
		fw_name = "vts_evt0.bin";
	} else {
		fw_name = "vts.bin";
	}

	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct: %s\n",
				fw_name,
				__func__);
		ret = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			fw_name, dev);

		if (ret < 0) {
			vts_dev_err(dev, "Failed to request_firmware_nowait\n");
		} else {
			vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
			vts_soc_complete_fw_request(data->firmware, dev);
		}
	}

	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return 0;
}

int vts_soc_probe(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}

void vts_soc_remove(struct device *dev)
{
	pr_info("%s\n", __func__);
}
