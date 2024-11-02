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

#include "is-core.h"
#include "is-hw.h"
#include "pablo-device-iommu-group.h"

#define IOMMU_GROUP		0x01
#define IOMMU_GROUP_MODULE	0x02

static struct pablo_device_iommu_group_module *iommu_group_module;

struct pablo_device_iommu_group_module *pablo_iommu_group_module_get(void)
{
	return iommu_group_module;
}

struct pablo_device_iommu_group *pablo_iommu_group_get(unsigned int idx)
{
	if (iommu_group_module) {
		if (idx < iommu_group_module->num_of_groups)
			return iommu_group_module->iommu_group[idx];
		else
			return NULL;
	} else {
		return NULL;
	}
}

static int pablo_iommu_group_probe(struct platform_device *pdev,
				struct pablo_iommu_group_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct pablo_device_iommu_group *iommu_group;
	int ret;

	if (data->type != IOMMU_GROUP) {
		dev_err(dev, "invalid probe function\n");
		return -EINVAL;
	}

	iommu_group = devm_kzalloc(dev, sizeof(*iommu_group), GFP_KERNEL);
	if (!iommu_group)
		return -ENOMEM;

	iommu_group->dev = &pdev->dev;
	iommu_group->data = data;
	iommu_group->index = of_alias_get_id(node, data->alias_stem);
	if ((iommu_group->index < 0) ||
	    (iommu_group->index >= (iommu_group_module->num_of_groups))) {
		dev_err(dev, "invalid global index for iommu_group: %d\n", iommu_group->index);
		ret = iommu_group->index;
		goto err_get_iommu_group_index;
	}

	iommu_group_module->iommu_group[iommu_group->index] = iommu_group;

	ret = is_mem_init(&iommu_group->mem, pdev);
	if (ret) {
		probe_err("is_mem_probe is fail(%d)", ret);
		goto err_mem_init;
	}

#if defined(CONFIG_PM)
	pm_runtime_enable(iommu_group->dev);
#endif

	is_register_iommu_fault_handler(iommu_group->dev);

#ifdef USE_CAMERA_IOVM_BEST_FIT
	iommu_dma_enable_best_fit_algo(iommu_group->dev);
#endif
	dev_info(dev, "IOMMU-GROUP%d was added successfully\n", iommu_group->index);

	platform_set_drvdata(pdev, iommu_group);

	return 0;

err_mem_init:
err_get_iommu_group_index:
	devm_kfree(dev, iommu_group);

	return ret;
}

static struct pablo_iommu_group_data pablo_iommu_group = {
	.type = IOMMU_GROUP,
	.alias_stem = "iommu-group",

	.probe = pablo_iommu_group_probe,
};

static int pablo_iommu_group_module_probe(struct platform_device *pdev,
					struct pablo_iommu_group_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int ret;
	int num_of_groups;
	struct pablo_device_iommu_group **iommu_group;

	if (data->type != IOMMU_GROUP_MODULE) {
		dev_err(dev, "invalid probe function\n");
		return -EINVAL;
	}

	iommu_group_module = devm_kzalloc(dev, sizeof(*iommu_group_module), GFP_KERNEL);
	if (!iommu_group_module)
		return -ENOMEM;

	iommu_group_module->dev = &pdev->dev;
	iommu_group_module->data = data;
	iommu_group_module->index = of_alias_get_id(node, data->alias_stem);
	if (iommu_group_module->index < 0) {
		dev_err(dev, "invalid index for iommu-group-module\n");
		ret = iommu_group_module->index;
		goto err_get_iommu_group_module_index;
	}

	if (!of_find_property(node, "groups", NULL)) {
		dev_err(dev, "iommu_group_module has no groups\n");
		ret = -EINVAL;
		goto err_no_groups;
	}

	num_of_groups = of_count_phandle_with_args(node, "groups", NULL);
	iommu_group_module->num_of_groups = num_of_groups;

	iommu_group = devm_kzalloc(dev, sizeof(*iommu_group) * num_of_groups, GFP_KERNEL);
	iommu_group_module->iommu_group = iommu_group;
	if (!iommu_group_module->iommu_group) {
		dev_err(dev, "failed to allocate iommu_group_module->iommu_group\n");
		ret = -ENOMEM;
		goto err_alloc_iommu_group;
	}

	dev_info(dev, "IOMMU-GROUP-MODULE%d was added successfully with\n",
			iommu_group_module->index);

	platform_set_drvdata(pdev, iommu_group_module);

	return 0;

err_alloc_iommu_group:
err_no_groups:
err_get_iommu_group_module_index:
	devm_kfree(dev, iommu_group_module);

	return ret;
}

static struct pablo_iommu_group_data pablo_iommu_group_module = {
	.type = IOMMU_GROUP_MODULE,
	.alias_stem = "iommu-group-module",

	.probe = pablo_iommu_group_module_probe,
};

static const struct of_device_id pable_iommu_group_of_table[] = {
	{
		.compatible = "samsung,exynos-is-iommu-group",
		.data = &pablo_iommu_group,
	},
	{
		.compatible = "samsung,exynos-is-iommu-group-module",
		.data = &pablo_iommu_group_module,
	},
	{},
};
MODULE_DEVICE_TABLE(of, pable_iommu_group_of_table);

static int pablo_device_iommu_group_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_iommu_group_data *data;

	of_id = of_match_device(of_match_ptr(pable_iommu_group_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_iommu_group_data *)of_id->data;

	return data->probe(pdev, data);
}

static int pablo_device_iommu_group_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_iommu_group_data *data;

	of_id = of_match_device(of_match_ptr(pable_iommu_group_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_iommu_group_data *)of_id->data;

	return data->remove(pdev, data);
}

struct platform_driver pablo_iommu_group_driver = {
	.probe = pablo_device_iommu_group_probe,
	.remove = pablo_device_iommu_group_remove,
	.driver = {
		.name	= "pablo-iommu-group",
		.owner	= THIS_MODULE,
		.of_match_table = pable_iommu_group_of_table,
	}
};

#ifndef MODULE
static int __init pablo_iommu_group_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_iommu_group_driver,
		pablo_device_iommu_group_probe);
	if (ret)
		pr_err("failed to probe %s driver: %d\n",
			"pablo-iommu-group", ret);

	return ret;
}

device_initcall_sync(pablo_iommu_group_init);
#endif

MODULE_AUTHOR("Sooyoung Choi");
MODULE_DESCRIPTION("Exynos Pablo IOMMU group driver");
MODULE_LICENSE("GPL v2");
