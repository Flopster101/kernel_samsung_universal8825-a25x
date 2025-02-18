/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DEVICE_IOMMU_GROUP_H
#define PABLO_DEVICE_IOMMU_GROUP_H

#include "pablo-mem.h"

struct pablo_device_iommu_group {
	struct device *dev;
	struct pablo_iommu_group_data *data;
	int index;
	struct is_mem mem;
};

/* camera IOMMU group module */
struct pablo_device_iommu_group_module {
	struct device *dev;
	struct pablo_iommu_group_data *data;
	int index;
	int num_of_groups;
	struct pablo_device_iommu_group **iommu_group;
};

struct pablo_iommu_group_data {
	unsigned int type;
	char *alias_stem;

	int (*probe)(struct platform_device *pdev,
			struct pablo_iommu_group_data *data);
	int (*remove)(struct platform_device *pdev,
			struct pablo_iommu_group_data *data);
};

struct pablo_device_iommu_group *pablo_iommu_group_get(unsigned int idx);
struct pablo_device_iommu_group_module *pablo_iommu_group_module_get(void);
#endif
