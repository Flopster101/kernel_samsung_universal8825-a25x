// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/gpio.h>

#ifdef CONFIG_SENSOR_DRV
#include "main.h"
#endif
#include "comms.h"
#include "chub.h"
#include "ipc_chub.h"
#include "chub_dbg.h"
#include "chub_exynos.h"

#include <soc/samsung/cal-if.h>

int contexthub_blk_poweron(struct contexthub_ipc_info *chub)
{
	/* only for soc default power down */
	return cal_chub_on();
}

int contexthub_soc_poweron(struct contexthub_ipc_info *chub)
{
	int ret;

	/* pmu reset-release on CHUB */
	ret = cal_chub_reset_release();

	return ret;
}

int contexthub_cm55_reset(struct contexthub_ipc_info *chub)
{
#if defined(CONFIG_SOC_S5E5515)
	int ret;

	/* pmu reset-release on CHUB */
	ret = cal_chub_cm55_reset_release();

	return ret;
#else
	return 0;
#endif
}

int contexthub_core_reset(struct contexthub_ipc_info *chub)
{
	if (!IS_ERR_OR_NULL(chub->pmu_chub_cpu)) {
		__raw_writel(__raw_readl(chub->pmu_chub_cpu) & ~0x1,
			     chub->pmu_chub_cpu);
		msleep(20);
		__raw_writel(__raw_readl(chub->pmu_chub_cpu) | 0x1,
			     chub->pmu_chub_cpu);
		msleep(100);

		if (__raw_readl(chub->pmu_chub_cpu + 0x4) != 0x1) {
			nanohub_dev_err(chub->dev, "%s fail!\n", __func__);
			return -ETIMEDOUT;
		} else {
			return 0;
		}
	} else {
		nanohub_err("%s: core reset is not supported\n", __func__);
		return -EINVAL;
	}
}

void contexthub_disable_pin(struct contexthub_ipc_info *chub)
{
	int i;
	u32 irq;

	for (i = 0; i < chub->irq_pin_len; i++) {
		irq = gpio_to_irq(chub->irq_pins[i]);
		disable_irq_nosync(irq);
		nanohub_dev_info(chub->dev, "%s: %d irq (pin:%d) is for chub. disable it\n",
				 __func__, irq, chub->irq_pins[i]);
	}
}

int contexthub_get_qch_base(struct contexthub_ipc_info *chub)
{
	return 0;
}

static struct clk *contexthub_devm_clk_prepare(struct device *dev, const char *name)
{
	struct clk *clk = NULL;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		nanohub_dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		nanohub_dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

	ret = clk_enable(clk);
	if (ret < 0) {
		nanohub_dev_err(dev, "Failed to enable clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

int contexthub_set_clk(struct contexthub_ipc_info *chub)
{
	struct clk *clk;

	clk = contexthub_devm_clk_prepare(chub->dev, "chub_bus");
	if (IS_ERR_OR_NULL(clk))
		return -ENODEV;

	chub->clkrate = clk_get_rate(clk);
	if (!chub->clkrate) {
		dev_info(chub->dev, "clk not set, default %lu\n", chub->clkrate);
		chub->clkrate = 400000000;
	}

	return 0;
}

int contexthub_get_clock_names(struct contexthub_ipc_info *chub)
{
	return 0;
}

void contexthub_upmu_init(struct contexthub_ipc_info *chub)
{
#ifdef CONTEXTHUB_UPMU
	if (!IS_ERR_OR_NULL(chub->chub_out))
		__raw_writel(__raw_readl(chub->chub_out) | SEL_UPMU, chub->chub_out);
	if (!IS_ERR_OR_NULL(chub->upmu)) {
		__raw_writel(__raw_readl(chub->upmu + UPMU_INT_EN) | WAKEUP_REQ | PD_REQ,
			     chub->upmu + UPMU_INT_EN);
		__raw_writel(__raw_readl(chub->upmu + UPMU_INT_TYPE) | WAKEUP_REQ,
			     chub->upmu + UPMU_INT_TYPE);
		__raw_writel(__raw_readl(chub->upmu + UPMU_SYSTEM_CTRL) | RESETN_HCU,
			     chub->upmu + UPMU_SYSTEM_CTRL);
	}
	nanohub_info("%s done!\n", __func__);
#else
	nanohub_info("%s not supported!\n", __func__);
#endif
}

#if !IS_ENABLED(CONFIG_CHUB_PMUCAL)
int cal_chub_on(void) { return 0; }
int cal_chub_reset_assert(void) { return 0; }
int cal_chub_reset_release_config(void) { return 0; }
int cal_chub_reset_release(void) { return 0; }
#endif
