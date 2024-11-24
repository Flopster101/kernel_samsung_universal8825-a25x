// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_dsimfc.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	Wonyeong Choi <won0.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/dma-buf.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/clk.h>
#include <linux/console.h>

#include <exynos_drm_dsimfc.h>
#include <exynos_drm_debug.h>

#include <regs-dsimfc.h>

static int dsimfc_log_level = 6;
module_param_named(dpu_dsimfc_log_level, dsimfc_log_level, int, 0600);
MODULE_PARM_DESC(dpu_dsimfc_log_level, "log level for dpu bts [default : 6]");

#define dsimfc_err(dsimfc, fmt, ...)	\
dpu_pr_err(drv_name((dsimfc)), (dsimfc)->id, dsimfc_log_level, fmt, ##__VA_ARGS__)

#define dsimfc_warn(dsim_fc, fmt, ...)	\
dpu_pr_warn(drv_name((dsimfc)), (dsimfc)->id, dsimfc_log_level, fmt, ##__VA_ARGS__)

#define dsimfc_info(dsimfc, fmt, ...)	\
dpu_pr_info(drv_name((dsimfc)), (dsimfc)->id, dsimfc_log_level, fmt, ##__VA_ARGS__)

#define dsimfc_dbg(dsimfc, fmt, ...)	\
dpu_pr_debug(drv_name((dsimfc)), (dsimfc)->id, dsimfc_log_level, fmt, ##__VA_ARGS__)

struct dsimfc_device *dsimfc_drvdata[MAX_DSIMFC_CNT];

void dsimfc_dump(struct dsimfc_device *dsimfc)
{
	int acquired = console_trylock();

	__dsimfc_dump(dsimfc->id, dsimfc->res.regs);

	if (acquired)
		console_unlock();
}

static int dsimfc_set_config(struct dsimfc_device *dsimfc,
		struct dsimfc_config *config)
{
	int ret = 0;

	mutex_lock(&dsimfc->lock);

	dsimfc->config = config;
	dsimfc_reg_init(dsimfc->id);
	dsimfc_reg_set_config(dsimfc->id);

	dsimfc_dbg(dsimfc, "%s: configuration\n", __func__);

	mutex_unlock(&dsimfc->lock);
	return ret;
}

static int dsimfc_start(struct dsimfc_device *dsimfc)
{
	int ret = 0;

	mutex_lock(&dsimfc->lock);

	enable_irq(dsimfc->res.irq);
	dsimfc_reg_irq_enable(dsimfc->id);
	dsimfc_reg_set_start(dsimfc->id);

	dsimfc_dbg(dsimfc, "%s: is started\n", __func__);

	mutex_unlock(&dsimfc->lock);
	return ret;
}

static int dsimfc_stop(struct dsimfc_device *dsimfc)
{
	int ret = 0;

	mutex_lock(&dsimfc->lock);

	if (dsimfc_reg_deinit(dsimfc->id, 1)) {
		dsimfc_err(dsimfc, "%s: dsimfc%d stop error\n", __func__);
		ret = -EINVAL;
	}
	disable_irq(dsimfc->res.irq);

	dsimfc_dbg(dsimfc, "%s: is stopped\n", __func__);

	mutex_unlock(&dsimfc->lock);
	return ret;
}

static int dsimfc_parse_dt(struct dsimfc_device *dsimfc, struct device *dev)
{
	struct device_node *node = dev->of_node;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		dsimfc_warn(dsimfc, "no device tree information\n");
		return -EINVAL;
	}

	dsimfc->id = of_alias_get_id(dev->of_node, "dsimfc");
	dsimfc_info(dsimfc, "%s: probe start..\n", __func__);
	of_property_read_u32(node, "port", (u32 *)&dsimfc->port);
	dsimfc_info(dsimfc, "AXI port = %d\n", dsimfc->port);

	return 0;
}

static irqreturn_t dsimfc_irq_handler(int irq, void *priv)
{
	struct dsimfc_device *dsimfc = priv;
	u32 irqs;

	spin_lock(&dsimfc->slock);

	irqs = dsimfc_reg_get_irq_and_clear(dsimfc->id);

	if (irqs & DSIMFC_CONFIG_ERR_IRQ) {
		dsimfc_err(dsimfc, "config error irq occurs\n");
		goto irq_end;
	}
	if (irqs & DSIMFC_READ_SLAVE_ERR_IRQ) {
		dsimfc_err(dsimfc, "read slave error irq occurs\n");
		dsimfc_dump(dsimfc);
		goto irq_end;
	}
	if (irqs & DSIMFC_DEADLOCK_IRQ) {
		dsimfc_err(dsimfc, "deadlock irq occurs\n");
		dsimfc_dump(dsimfc);
		goto irq_end;
	}
	if (irqs & DSIMFC_FRAME_DONE_IRQ) {
		dsimfc_dbg(dsimfc, "frame done irq occurs\n");
		dsimfc->config->done = 1;
		wake_up_interruptible_all(&dsimfc->xferdone_wq);
		goto irq_end;
	}

irq_end:
	spin_unlock(&dsimfc->slock);
	return IRQ_HANDLED;
}

static int dsimfc_init_resources(struct dsimfc_device *dsimfc, struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dsimfc_err(dsimfc, "failed to get mem resource\n");
		return -ENOENT;
	}
	dsimfc_info(dsimfc, "res: start(0x%x), end(0x%x)\n",
			(u32)res->start, (u32)res->end);

	dsimfc->res.regs = devm_ioremap_resource(dsimfc->dev, res);
	if (IS_ERR_OR_NULL(dsimfc->res.regs)) {
		dsimfc_err(dsimfc, "failed to remap dsimfc sfr region\n");
		return -EINVAL;
	}
	dsimfc_regs_desc_init(dsimfc->res.regs, "dsimfc", dsimfc->id);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dsimfc_err(dsimfc, "failed to get dsimfc irq resource\n");
		return -ENOENT;
	}
	dsimfc_info(dsimfc, "dsimfc irq no = %lld\n", res->start);

	dsimfc->res.irq = res->start;
	ret = devm_request_irq(dsimfc->dev, res->start, dsimfc_irq_handler, 0,
			pdev->name, dsimfc);
	if (ret) {
		dsimfc_err(dsimfc, "failed to install dsimfc irq\n");
		return -EINVAL;
	}
	disable_irq(dsimfc->res.irq);

	return 0;
}

struct exynos_dsimfc_ops dsimfc_ops = {
	.atomic_config = dsimfc_set_config,
	.atomic_start = dsimfc_start,
	.atomic_stop = dsimfc_stop,
};

static int dsimfc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dsimfc_device *dsimfc;
	int ret = 0;

	dsimfc = devm_kzalloc(dev, sizeof(*dsimfc), GFP_KERNEL);
	if (!dsimfc)
		return -ENOMEM;

	dsimfc->dev = dev;

	dma_set_mask(dev, DMA_BIT_MASK(32));

	ret = dsimfc_parse_dt(dsimfc, dsimfc->dev);
	if (ret)
		goto err_dt;

	dsimfc_drvdata[dsimfc->id] = dsimfc;

	spin_lock_init(&dsimfc->slock);
	mutex_init(&dsimfc->lock);

	ret = dsimfc_init_resources(dsimfc, pdev);
	if (ret)
		goto err_dt;

	init_waitqueue_head(&dsimfc->xferdone_wq);

	dsimfc->ops = &dsimfc_ops;
	platform_set_drvdata(pdev, dsimfc);

	dsimfc_info(dsimfc, "is probed successfully\n");

	return 0;

err_dt:
	kfree(dsimfc);

	return ret;
}

static int dsimfc_remove(struct platform_device *pdev)
{
	struct dsimfc_device *dsimfc = platform_get_drvdata(pdev);

	dsimfc_info(dsimfc, "driver unloaded\n");
	return 0;
}

static const struct of_device_id dsimfc_of_match[] = {
	{ .compatible = "samsung,exynos-dsimfc" },
	{},
};
MODULE_DEVICE_TABLE(of, dsimfc_of_match);


struct platform_driver dsimfc_driver = {
	.probe		= dsimfc_probe,
	.remove		= dsimfc_remove,
	.driver = {
		   .name = "exynos-dsimfc",
		   .owner = THIS_MODULE,
		   .of_match_table = dsimfc_of_match,
	},

};

MODULE_AUTHOR("Haksoo Kim <herb@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DMA DSIM Fast Command driver");
MODULE_LICENSE("GPL");
