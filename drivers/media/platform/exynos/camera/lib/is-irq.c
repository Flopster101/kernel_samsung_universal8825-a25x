// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS IS (Imaging Subsystem) driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/irqchip/arm-gic-v3.h>

#include "is-irq.h"
#include "is-debug.h"
#include "is-common-config.h"
#include "is-core.h"

#if IS_ENABLED(GIC_VENDOR_HOOK)
#include <trace/hooks/gic_v3.h>
#else
#define register_trace_android_vh_gic_v3_set_affinity(f, d)	do { } while(0)
#endif

void is_set_affinity_irq(unsigned int irq, bool enable)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	struct exynos_platform_is *pdata;
	char buf[10];
	struct cpumask cpumask;

	pdata = dev_get_platdata(is_get_is_dev());
	snprintf(buf, sizeof(buf), "0-%d", (pdata->cpu_cluster[1] - 1));
#endif

	if (IS_ENABLED(IRQ_MULTI_TARGET_CL0)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
		cpulist_parse(buf, &cpumask);
		irq_set_affinity_hint(irq, enable ? &cpumask : NULL);
#else
		irq_force_affinity(irq, cpumask_of(0));
#endif
	}
}

int is_request_irq(unsigned int irq,
	irqreturn_t (*func)(int, void *),
	const char *name,
	unsigned int added_irq_flags,
	void *dev)
{
	int ret = 0;

	ret = request_irq(irq, func,
			IS_HW_IRQ_FLAG | added_irq_flags,
			name,
			dev);
	if (ret) {
		err("%s, request_irq fail", name);
		return ret;
	}

	is_set_affinity_irq(irq, true);

	return ret;
}

void is_free_irq(unsigned int irq, void *dev)
{
	is_set_affinity_irq(irq, false);
	free_irq(irq, dev);
}

#if IS_ENABLED(GIC_VENDOR_HOOK)
#define GICD_BASE			0x10200000
#define GICD_ICLAR			(GICD_BASE + 0xE000)
#define GICD_ICLAR_BIT(irqno)		(((irqno) & 0xf) * 2)
#define GICD_ICLAR_MASK(irqno)		(3U << GICD_ICLAR_BIT(irqno))
#define GICD_ICLAR_CLA1NOT(irqno)	(2U << GICD_ICLAR_BIT(irqno))
#define GICD_ICLAR_CLA0NOT(irqno)	(1U << GICD_ICLAR_BIT(irqno))
static void pablo_gic_v3_set_affinity(void *data, struct irq_data *irq,
					const struct cpumask *mask_val, u64 *affinity)
{
	cpumask_t curr_mask, routing_mask;
	void __iomem *reg;
	u32 val;
	struct exynos_platform_is *pdata;
	char buf[10];

	cpumask_and(&curr_mask, mask_val, cpu_online_mask);

	pdata = dev_get_platdata(is_get_is_dev());
	snprintf(buf, sizeof(buf), "0-%d", (pdata->cpu_cluster[1] - 1));
	cpulist_parse(buf, &routing_mask);

	if (cpumask_equal(&curr_mask, &routing_mask)) {
		*affinity = GICD_IROUTER_SPI_MODE_ANY;

		reg = ioremap(GICD_ICLAR, SZ_1K) + ((irq->hwirq / 16) * 4);
		val = readl_relaxed(reg) & ~GICD_ICLAR_MASK(irq->hwirq);
		val |= GICD_ICLAR_CLA1NOT(irq->hwirq);
		writel_relaxed(val, reg);
		iounmap(reg);
	}
}
#endif

void pablo_irq_probe(void)
{
	if (IS_ENABLED(GIC_VENDOR_HOOK))
		register_trace_android_vh_gic_v3_set_affinity(pablo_gic_v3_set_affinity, NULL);
}
