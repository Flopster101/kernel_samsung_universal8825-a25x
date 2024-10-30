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
#include <sound/vts.h>

#include <soc/samsung/exynos-pmu-if.h>

#include "vts_soc.h"
#include "vts_soc_v1.h"
#include "vts_dbg.h"

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))

#ifdef CONFIG_SOC_EXYNOS9830_EVT0
static struct reserved_mem *vts_rmem;

static void *vts_rmem_vmap(struct reserved_mem *rmem)
{
	phys_addr_t phys = rmem->base;
	size_t size = rmem->size;
	unsigned int num_pages = (unsigned int)DIV_ROUND_UP(size, PAGE_SIZE);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages, **page;
	void *vaddr = NULL;

	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages)
		goto out;

	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);
out:
	return vaddr;
}

static int __init vts_rmem_setup(struct reserved_mem *rmem)
{
	vts_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	vts_rmem = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(vts_rmem, "exynos,vts_rmem", vts_rmem_setup);
#endif

void vts_cpu_power(struct device *dev, bool on)
{
	vts_dev_info(dev, "%s(%d)\n", __func__, on);

#ifndef EMULATOR
	exynos_pmu_update(VTS_CPU_CONFIGURATION, VTS_CPU_LOCAL_PWR_CFG,
		on ? VTS_CPU_LOCAL_PWR_CFG : 0);
#else
	update_mask_value(pmu_alive + VTS_CPU_CONFIGURATION,
		VTS_CPU_LOCAL_PWR_CFG, on ? VTS_CPU_LOCAL_PWR_CFG : 0);
#endif

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
	if (!on) {
#ifndef EMULATOR
		exynos_pmu_update(VTS_CPU_OPTION,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK);
#else
		update_mask_value(pmu_alive + VTS_CPU_OPTION,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK,
				VTS_CPU_OPTION_USE_STANDBYWFI_MASK);
#endif
	}
#endif
}

#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
int vts_cpu_enable(struct device *dev, bool enable)
{
	unsigned int mask = VTS_CPU_RESET_OPTION_ENABLE_CPU_MASK;
	unsigned int val = (enable ? mask : 0);
	unsigned int status = 0;
	unsigned long after;

	vts_dev_info(dev, "%s(%d)\n", __func__, enable);

#ifndef EMULATOR
	exynos_pmu_update(VTS_CPU_RESET_OPTION, mask, val);
#else
	update_mask_value(pmu_alive + VTS_CPU_RESET_OPTION, mask, val);
#endif
	if (enable) {
		after = jiffies + LIMIT_IN_JIFFIES;
		do {
			schedule();
#ifndef EMULATOR
			exynos_pmu_read(VTS_CPU_STATUS, &status);
#else
			status = readl(pmu_alive + VTS_CPU_STATUS);
#endif
		} while (((status & VTS_CPU_STATUS_STATUS_MASK)
				!= VTS_CPU_STATUS_STATUS_MASK)
				&& time_is_after_eq_jiffies(after));
		if (time_is_before_jiffies(after)) {
			vts_err("vts cpu enable timeout\n");
			return -ETIME;
		}
	}

	return 0;
}
#else
int vts_cpu_enable(bool enable)
{
	unsigned int status = 0;
	unsigned long after;

	after = jiffies + LIMIT_IN_JIFFIES;
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

	return 0;
}
#endif

void vts_reset_cpu(struct device *dev)
{
#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
#ifndef EMULATOR
	vts_cpu_enable(false);
	vts_cpu_power(false);
	vts_cpu_power(true);
	vts_cpu_enable(true);
#endif
#else
	vts_cpu_enable(dev, false);
	vts_cpu_power(dev, false);
	vts_cpu_enable(dev, true);
	vts_cpu_power(dev, true);
#endif
}

void vts_pad_retention(bool retention)
{
	if (!retention) {
#if (defined(CONFIG_SOC_EXYNOS9810) || defined(CONFIG_SOC_EXYNOS8895))
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x10000000, 0x10000000);
#else
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x1 << 11, 0x1 << 11);
#endif
	}
}
EXPORT_SYMBOL(vts_pad_retention);

#if !(defined(CONFIG_SOC_EXYNOS8895))
u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size)
{
	u32 aligned_size = round_up(size, SZ_4M);
	u64 aligned_base = round_down(base, aligned_size);

	/* set VTS BAAW config */
	writel(0x40100, sfr_base);
	writel(0x40200, sfr_base + 0x4);
	writel(0x15900, sfr_base + 0x8);
	writel(0x80000003, sfr_base + 0xC);

	writel(VTS_BAAW_BASE / SZ_4K, sfr_base + VTS_BAAW_SRC_START_ADDRESS);
	vts_err("%s 0x%x\n", __func__, VTS_BAAW_BASE / SZ_4K);
	writel((VTS_BAAW_BASE + aligned_size) / SZ_4K,
		sfr_base + VTS_BAAW_SRC_END_ADDRESS);
	vts_err("%s 0x%x\n", __func__, (VTS_BAAW_BASE + aligned_size) / SZ_4K);
	writel(aligned_base / SZ_4K, sfr_base + VTS_BAAW_REMAPPED_ADDRESS);
	vts_err("%s 0x%x\n", __func__, aligned_base / SZ_4K);
	writel(0x80000003, sfr_base + VTS_BAAW_INIT_DONE);

	return base - aligned_base + VTS_BAAW_BASE;
}
#endif

int vts_acquire_sram(struct platform_device *pdev, int vts)
{
#ifdef CONFIG_SOC_EXYNOS8895
	struct vts_data *data = platform_get_drvdata(pdev);
	int previous;
	unsigned long flag;

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895)) {
		vts_dev_info(&pdev->dev, "%s(%d)\n", __func__, vts);

		if (!vts) {
			while (pm_runtime_active(&pdev->dev)) {
				vts_dev_warn(&pdev->dev,
					"%s Clear existing active states\n",
					__func__);
				pm_runtime_put_sync(&pdev->dev);
			}
		}
		previous = test_and_set_bit(0, &data->sram_acquired);
		if (previous) {
			vts_dev_err(&pdev->dev, "sram acquisition failed\n");
			return -EBUSY;
		}

		if (!vts) {
			pm_runtime_get_sync(&pdev->dev);
			data->voicecall_enabled = true;
			spin_lock_irqsave(&data->state_spinlock, flag);
			data->vts_state = VTS_STATE_VOICECALL;
			spin_unlock_irqrestore(&data->state_spinlock, flag);
		}

		writel((vts ? 0 : 1) << VTS_MEM_SEL_OFFSET,
			data->sfr_base + VTS_SHARED_MEM_CTRL);
	}
#endif

	return 0;
}
EXPORT_SYMBOL(vts_acquire_sram);

int vts_release_sram(struct platform_device *pdev, int vts)
{
#ifdef CONFIG_SOC_EXYNOS8895
	struct vts_data *data = platform_get_drvdata(pdev);

	vts_dev_info(&pdev->dev, "%s(%d)\n", __func__, vts);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895)) {
		if (test_bit(0, &data->sram_acquired) &&
				(data->voicecall_enabled || vts)) {
			writel(0 << VTS_MEM_SEL_OFFSET,
					data->sfr_base + VTS_SHARED_MEM_CTRL);
			clear_bit(0, &data->sram_acquired);

			if (!vts) {
				pm_runtime_put_sync(&pdev->dev);
				data->voicecall_enabled = false;
			}
			vts_dev_info(&pdev->dev, "%s(%d) completed\n",
					__func__, vts);
		} else
			vts_dev_warn(&pdev->dev, "%s(%d) already released\n",
					__func__, vts);
	}

#endif
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

#ifdef CONFIG_SOC_EXYNOS8895
static int set_vtsexec_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct vts_data *data = p_vts_data;
	u32 values[3];
	int result = 0;
	int vtsexecution_mode;

	u32 keyword_type = 1;
	char env[100] = {0,};
	char *envp[2] = {env, NULL};
	struct device *dev = &data->pdev->dev;
	int loopcnt = 10;

	pm_runtime_barrier(component->dev);

	while (data->voicecall_enabled) {
		vts_dev_warn(component->dev,
			"%s voicecall (%d)\n", __func__,
			data->voicecall_enabled);

		if (loopcnt <= 0) {
			vts_dev_warn(component->dev, "%s VTS SRAM is Used for CP call\n",
				__func__);

			keyword_type = -EBUSY;
			snprintf(env, sizeof(env),
				 "VOICE_WAKEUP_WORD_ID=%x",
				 keyword_type);

			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
			return -EBUSY;
		}

		loopcnt--;
		usleep_range(9990, 10000);
	}

	vts_dev_warn(component->dev, "%s voicecall (%d) (End)\n",
		__func__, data->voicecall_enabled);

	vtsexecution_mode = ucontrol->value.integer.value[0];

	if (vtsexecution_mode >= VTS_MODE_COUNT) {
		vts_dev_err(component->dev,
		"Invalid voice control mode =%d", vtsexecution_mode);
		return 0;
	}

	vts_dev_info(component->dev, "%s Current: %d requested %s\n",
			 __func__, data->exec_mode,
			 vtsexec_mode_text[vtsexecution_mode]);
	if (data->exec_mode == VTS_OFF_MODE &&
		 vtsexecution_mode != VTS_OFF_MODE) {
		pm_runtime_get_sync(component->dev);
		vts_start_runtime_resume(component->dev);
		vts_clk_set_rate(component->dev, data->syssel_rate);
	}

	if (pm_runtime_active(component->dev)) {
		values[0] = vtsexecution_mode;
		values[1] = 0;
		values[2] = 0;
		result = vts_start_ipc_transaction(component->dev,
				 data, VTS_IRQ_AP_SET_MODE,
				 &values, 0, 1);
		if (result < 0) {
			vts_dev_err(component->dev,
				"%s SET_MODE IPC transaction Failed\n",
				vtsexec_mode_text[vtsexecution_mode]);
			if (data->exec_mode == VTS_OFF_MODE &&
				 vtsexecution_mode != VTS_OFF_MODE)
				pm_runtime_put_sync(component->dev);
			return result;
		}
	}
	if (vtsexecution_mode <= VTS_SENSORY_TRIGGER_MODE)
		data->exec_mode |= (0x1 << vtsexecution_mode);
	else
		data->exec_mode &= ~(0x1 << (vtsexecution_mode -
					VTS_SENSORY_TRIGGER_MODE));
	vts_dev_info(component->dev, "%s Configured: [%d] %s\n",
		 __func__, data->exec_mode,
		 vtsexec_mode_text[vtsexecution_mode]);

	if (data->exec_mode == VTS_OFF_MODE &&
		 pm_runtime_active(component->dev)) {
		pm_runtime_put_sync(component->dev);
	}
	return  0;
}
#endif

