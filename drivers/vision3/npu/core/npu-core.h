/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_CORE_H_
#define _NPU_CORE_H_

#include <linux/version.h>
#include <linux/device.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
#include <soc/samsung/sysevent.h>
#endif

#include "npu-clock.h"
#include "npu-system.h"

#define NPU_CORE_NAME	"npu-core"

struct npu_core {
	struct device		*dev;
	u32			id;
	struct npu_clocks	clks;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
	/* SYSTEM EVENT */
	struct sysevent_desc sysevent_desc;
	struct sysevent_device *sysevent_dev;
#endif
};

#define NPU_CORE_MAX_NUM	8

int npu_core_on(struct npu_system *system);
int npu_core_off(struct npu_system *system);
int npu_core_clock_on(struct npu_system *system);
int npu_core_clock_off(struct npu_system *system);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0)
int npu_core_probe(struct platform_device *pdev);
int npu_core_suspend(struct device *dev);
int npu_core_resume(struct device *dev);
int npu_core_runtime_suspend(struct device *dev);
int npu_core_runtime_resume(struct device *dev);
int npu_core_remove(struct platform_device *pdev);
#endif
#endif // _NPU_CORE_H_
