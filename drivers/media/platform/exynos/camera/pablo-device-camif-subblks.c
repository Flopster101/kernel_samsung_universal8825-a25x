// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>

#include "is-hw.h"
#include "is-debug.h"
#include "pablo-device-camif-subblks.h"

/* BNS */
static struct pablo_camif_bns *pablo_bns;

#if IS_ENABLED(CONFIG_PABLO_CAMIF_BNS)
struct pablo_camif_bns *pablo_camif_bns_get(void)
{
	return pablo_bns;
}

void pablo_camif_bns_cfg(struct pablo_camif_bns *bns,
		struct is_sensor_cfg *sensor_cfg,
		u32 ch)
{
	bns->en = csi_hw_s_bns_cfg(bns->regs, sensor_cfg,
			&bns->width, &bns->height);
	if (!bns->en)
		return;

	csi_hw_s_bns_ch(bns->mux_regs, ch);

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI)))
		csi_hw_bns_dump(bns->regs);
}

void pablo_camif_bns_reset(struct pablo_camif_bns *bns)
{
	if (!bns->en)
		return;

	csi_hw_reset_bns(bns->regs);
	bns->en = false;

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_CSI)))
		csi_hw_bns_dump(bns->regs);
}
#endif

static int pablo_camif_bns_probe(struct platform_device *pdev,
		struct pablo_camif_subblks_data *data)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *dnode = dev->of_node;
	struct resource *res;
	u32 elems;

	pablo_bns = devm_kzalloc(dev, sizeof(*pablo_bns), GFP_KERNEL);
	if (!pablo_bns) {
		dev_err(dev, "failed to allocate drv_data\n");
		return -ENOMEM;
	}

	pablo_bns->dev = &pdev->dev;
	pablo_bns->data = data;
	pablo_bns->en = false;

	/* Get BNS base address */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctl");
	if (!res) {
		dev_err(dev, "failed to get ctl IORESOURCE_MEM");
		ret = -ENOMEM;
		goto ioremap_ctl_err;
	}

	pablo_bns->regs = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(pablo_bns->regs)) {
		dev_err(dev, "failed to ioremap for ctl\n");
		ret = -ENOMEM;
		goto ioremap_ctl_err;
	}

	/* Get BNS In/Out mux address */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mux");
	if (!res) {
		dev_err(dev, "failed to get mux IORESOURCE_MEM");
		ret = -ENOMEM;
		goto ioremap_mux_err;
	}

	pablo_bns->mux_regs = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(pablo_bns->regs)) {
		dev_err(dev, "failed to ioremap for ctl\n");
		ret = -ENOMEM;
		goto ioremap_mux_err;
	}

	/* Get BNS In/Out mux values */
	ret = of_property_count_u32_elems(dnode, "mux");
	if (ret < 0) {
		dev_err(dev, "failed to get mux_val\n");
		goto mux_val_err;
	}

	elems = ret;
	pablo_bns->mux_val = devm_kcalloc(dev, elems, sizeof(*pablo_bns->mux_val), GFP_KERNEL);
	if (!pablo_bns->mux_val) {
		dev_err(dev, "failed to allocate mux_val memory\n");
		ret = -ENOMEM;
		goto mux_val_err;
	}

	ret = of_property_read_u32_array(dnode, "mux", pablo_bns->mux_val, elems);
	if (ret) {
		dev_err(dev, "failed to get mux_val resource\n");
		goto mux_val_read_err;
	}

	ret = of_property_read_u32(dnode, "dma_mux", &pablo_bns->dma_mux_val);
	if (ret) {
		dev_err(dev, "failed to get dma_mux_val resource\n");
		goto mux_val_read_err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mux");

	platform_set_drvdata(pdev, pablo_bns);

	dev_info(dev, "%s done\n", __func__);

	return 0;

mux_val_read_err:
	devm_kfree(dev, pablo_bns->mux_val);
mux_val_err:
	devm_iounmap(dev, pablo_bns->mux_regs);
ioremap_mux_err:
	devm_iounmap(dev, pablo_bns->regs);
ioremap_ctl_err:
	devm_kfree(dev, pablo_bns);

	return ret;
}

static struct pablo_camif_subblks_data bns_data = {
	.slock = __SPIN_LOCK_UNLOCKED(bns_data.slock),
	.probe = pablo_camif_bns_probe,
};

/* MCB */
static struct pablo_camif_mcb *camif_mcb;

struct pablo_camif_mcb *pablo_camif_mcb_get(void)
{
	return camif_mcb;
}

static int pablo_camif_mcb_probe(struct platform_device *pdev,
		struct pablo_camif_subblks_data *data)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	camif_mcb = devm_kzalloc(dev, sizeof(*camif_mcb), GFP_KERNEL);
	if (!camif_mcb) {
		dev_err(dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		probe_err("failed to get memory resource for MCB");
		ret = -ENODEV;
		goto err_get_res;
	}

	camif_mcb->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(camif_mcb->regs)) {
		dev_err(dev, "failed to get & ioremap for MCB control\n");
		ret = PTR_ERR(camif_mcb->regs);
		goto err_get_ioremap_ctl;
	}

	camif_mcb->active_ch = 0;
	mutex_init(&camif_mcb->lock);

	dev_info(dev, "%s done\n", __func__);

	return 0;

err_get_ioremap_ctl:
err_get_res:
	devm_kfree(dev, camif_mcb);

	return ret;
}

static struct pablo_camif_subblks_data mcb_data = {
	.probe = pablo_camif_mcb_probe,
};

/* EBUF */
static struct pablo_camif_ebuf *camif_ebuf;

struct pablo_camif_ebuf *pablo_camif_ebuf_get(void)
{
	return camif_ebuf;
}

static int pablo_camif_ebuf_probe(struct platform_device *pdev,
		struct pablo_camif_subblks_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *dnode = dev->of_node;
	struct resource *res;
	int ret;

	camif_ebuf = devm_kzalloc(dev, sizeof(*camif_ebuf), GFP_KERNEL);
	if (!camif_ebuf) {
		dev_err(dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		probe_err("failed to get memory resource for EUBF");
		ret = -ENODEV;
		goto err_get_res;
	}

	camif_ebuf->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(camif_ebuf->regs)) {
		dev_err(dev, "failed to get & ioremap for EBUF control\n");
		ret = PTR_ERR(camif_ebuf->regs);
		goto err_get_ioremap_ctl;
	}

	camif_ebuf->irq = platform_get_irq(pdev, 0);
	if (camif_ebuf->irq < 0) {
		dev_err(dev, "failed to get IRQ for EBUF: %d\n", camif_ebuf->irq);
		ret = camif_ebuf->irq;
		goto err_get_irq;
	}

	ret = of_property_read_u32(dnode, "num_of_ebuf", &camif_ebuf->num_of_ebuf);
	if (ret) {
		dev_err(dev, "failed to get num_of_ebuf property\n");
		goto err_get_num_of_ebuf;
	}

	mutex_init(&camif_ebuf->lock);

	dev_info(dev, "%s done\n", __func__);

	return 0;

err_get_num_of_ebuf:
err_get_irq:
	devm_iounmap(dev, camif_ebuf->regs);

err_get_ioremap_ctl:
err_get_res:
	devm_kfree(dev, camif_ebuf);

	return ret;
}

static struct pablo_camif_subblks_data ebuf_data = {
	.probe = pablo_camif_ebuf_probe,
};

/* COMMON */
static const struct of_device_id pablo_camif_subblks_of_table[] = {
	{
		.name = "camif-bns",
		.compatible = "samsung,camif-bns",
		.data = &bns_data,
	},
	{
		.name = "camif-mcb",
		.compatible = "samsung,camif-mcb",
		.data = &mcb_data,
	},
	{
		.name = "camif-ebuf",
		.compatible = "samsung,camif-ebuf",
		.data = &ebuf_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_camif_subblks_of_table);

static int pablo_camif_subblks_suspend(struct device *dev)
{
	return 0;
}

static int pablo_camif_subblks_resume(struct device *dev)
{
	return 0;
}

int pablo_camif_subblks_runtime_suspend(struct device *dev)
{
	return 0;
}

int pablo_camif_subblks_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pablo_camif_subblks_pm_ops = {
	.suspend		= pablo_camif_subblks_suspend,
	.resume			= pablo_camif_subblks_resume,
	.runtime_suspend	= pablo_camif_subblks_runtime_suspend,
	.runtime_resume		= pablo_camif_subblks_runtime_resume,
};

static int pablo_camif_subblks_pdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_camif_subblks_data *data;

	of_id = of_match_device(of_match_ptr(pablo_camif_subblks_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_camif_subblks_data *)of_id->data;

	return data->probe(pdev, data);
}

static int pablo_camif_subblks_pdev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_camif_subblks_data *data;

	of_id = of_match_device(of_match_ptr(pablo_camif_subblks_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_camif_subblks_data *)of_id->data;

	return data->remove(pdev, data);
}

struct platform_driver pablo_camif_subblks_driver = {
	.probe = pablo_camif_subblks_pdev_probe,
	.remove = pablo_camif_subblks_pdev_remove,
	.driver = {
		.name		= "pablo-camif-subblks",
		.owner		= THIS_MODULE,
		.of_match_table	= pablo_camif_subblks_of_table,
		.pm		= &pablo_camif_subblks_pm_ops,
	}
};

#ifndef MODULE
static int __init pablo_camif_subblks_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_camif_subblks_driver,
		pablo_camif_subblks_pdev_probe);
	if (ret)
		pr_err("failed to probe %s driver: %d\n",
			"pablo-camif-subblks", ret);

	return ret;
}

device_initcall_sync(pablo_camif_subblks_init);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo CAMIF Sub-blocks driver");
MODULE_ALIAS("platform:samsung-pablo-camif-subblks");
MODULE_LICENSE("GPL v2");
