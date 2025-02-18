// SPDX-License-Identifier: GPL-2.0-only
/**
 * irq-gic-v3-vh.c - Vendor hook driver for GIC V3
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
*/


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <trace/hooks/gic_v3.h>
#include <linux/cpumask.h>
#include <linux/of_address.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irq.h>


#include <trace/hooks/gic.h>
#include "irq-gic-common.h"

#define GICD_ICLAR			0xE000

#define GICD_ICLAR_BIT(irqno)		(((irqno) & 0xf) * 2)
#define GICD_ICLAR_MASK(irqno)		(3U << GICD_ICLAR_BIT(irqno))
#define GICD_ICLAR_CLA1NOT(irqno)	(2U << GICD_ICLAR_BIT(irqno))
#define GICD_ICLAR_CLA0NOT(irqno)	(1U << GICD_ICLAR_BIT(irqno))


static struct cpumask affinity_class0, affinity_class1;

/*
 * from irq-gic-v3.c
 */

enum gic_intid_range {
	SGI_RANGE,
	PPI_RANGE,
	SPI_RANGE,
	EPPI_RANGE,
	ESPI_RANGE,
	LPI_RANGE,
	__INVALID_RANGE__
};

static enum gic_intid_range __get_intid_range(irq_hw_number_t hwirq)
{
	switch (hwirq) {
	case 0 ... 15:
		return SGI_RANGE;
	case 16 ... 31:
		return PPI_RANGE;
	case 32 ... 1019:
		return SPI_RANGE;
	case EPPI_BASE_INTID ... (EPPI_BASE_INTID + 63):
		return EPPI_RANGE;
	case ESPI_BASE_INTID ... (ESPI_BASE_INTID + 1023):
		return ESPI_RANGE;
	case 8192 ... GENMASK(23, 0):
		return LPI_RANGE;
	default:
		return __INVALID_RANGE__;
	}
}

static enum gic_intid_range get_intid_range(struct irq_data *d)
{
	return __get_intid_range(d->hwirq);
}

static inline bool gic_irq_in_rdist(struct irq_data *d)
{
	switch (get_intid_range(d)) {
	case SGI_RANGE:
	case PPI_RANGE:
	case EPPI_RANGE:
		return true;
	default:
		return false;
	}
}

static u32 convert_offset_index(struct irq_data *d, u32 offset, u32 *index)
{
	switch (get_intid_range(d)) {
	case SGI_RANGE:
	case PPI_RANGE:
	case SPI_RANGE:
		*index = d->hwirq;
		return offset;
	case EPPI_RANGE:
		/*
		 * Contrary to the ESPI range, the EPPI range is contiguous
		 * to the PPI range in the registers, so let's adjust the
		 * displacement accordingly. Consistency is overrated.
		 */
		*index = d->hwirq - EPPI_BASE_INTID + 32;
		return offset;
	case ESPI_RANGE:
		*index = d->hwirq - ESPI_BASE_INTID;
		switch (offset) {
		case GICD_ISENABLER:
			return GICD_ISENABLERnE;
		case GICD_ICENABLER:
			return GICD_ICENABLERnE;
		case GICD_ISPENDR:
			return GICD_ISPENDRnE;
		case GICD_ICPENDR:
			return GICD_ICPENDRnE;
		case GICD_ISACTIVER:
			return GICD_ISACTIVERnE;
		case GICD_ICACTIVER:
			return GICD_ICACTIVERnE;
		case GICD_IPRIORITYR:
			return GICD_IPRIORITYRnE;
		case GICD_ICFGR:
			return GICD_ICFGRnE;
		case GICD_IROUTER:
			return GICD_IROUTERnE;
		default:
			break;
		}
		break;
	default:
		break;
	}

	WARN_ON(1);
	*index = d->hwirq;
	return offset;
}

/*
 * vendor hook functions
 */
static void set_affinity_vh(void *data , struct irq_data *d,
		const struct cpumask *mask_val, u64 *affinity, bool force, void __iomem* base)
{
	cpumask_t curr_mask;
	void __iomem *iclar_reg;
	u32 iclar_val;
	u32 offset, index;
	void __iomem *irout_reg;


	/* It is managed only in the SPI/ESPI range. */
	if (gic_irq_in_rdist(d))
		return;

	cpumask_and(&curr_mask, mask_val, cpu_online_mask);

	offset = convert_offset_index(d, GICD_IROUTER, &index);
	irout_reg = base + offset + (index * 8);

	if (!(cpumask_equal(&CPU_MASK_NONE, &affinity_class0)) &&
			(cpumask_equal(&curr_mask, &affinity_class0))) {
		*affinity = GICD_IROUTER_SPI_MODE_ANY;
		gic_write_irouter(GICD_IROUTER_SPI_MODE_ANY, irout_reg);
		iclar_reg = base +
			GICD_ICLAR + ((d->hwirq / 16) * 4);
		iclar_val = readl_relaxed(iclar_reg) & ~GICD_ICLAR_MASK(d->hwirq);
		iclar_val |= GICD_ICLAR_CLA1NOT(d->hwirq);
		writel_relaxed(iclar_val, iclar_reg);
	} else if (!(cpumask_equal(&CPU_MASK_NONE, &affinity_class1)) &&
			(cpumask_equal(&curr_mask, &affinity_class1))) {
		*affinity = GICD_IROUTER_SPI_MODE_ANY;
		gic_write_irouter(GICD_IROUTER_SPI_MODE_ANY, irout_reg);
		iclar_reg = base +
			GICD_ICLAR + ((d->hwirq / 16) * 4);
		iclar_val = readl_relaxed(iclar_reg) & ~GICD_ICLAR_MASK(d->hwirq);
		iclar_val |= GICD_ICLAR_CLA0NOT(d->hwirq);
		writel_relaxed(iclar_val, iclar_reg);
	}
}

static int gic_v3_vh_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	const char *name;
	int ret;

	dev_info(dev, "gic-v3-vh probe\n");

	if (of_property_read_string(node, "class0-cpus", &name)) {
		affinity_class0 = CPU_MASK_NONE;
	} else {
		cpulist_parse(name, &affinity_class0);
	}

	if (of_property_read_string(node, "class1-cpus", &name)) {
		affinity_class1 = CPU_MASK_NONE;
	} else {
		cpulist_parse(name, &affinity_class1);
	}

	ret = register_trace_android_rvh_gic_v3_set_affinity(set_affinity_vh, NULL);

	if (ret) {
		pr_err("Failed to register trace_android_vh_gic_v3_set_affinity\n");
		return ret;
	}

	return 0;

}

static const struct of_device_id gic_v3_vh_match_table[] = {
	{ .compatible = "arm,gic-v3-vh" },
	{ },
};
MODULE_DEVICE_TABLE(of, gic_v3_vh_match_table);

static struct platform_driver gic_v3_vh_driver = {
	.probe		= gic_v3_vh_probe,
	.driver		= {
		.name	= "gic-v3-vh",
		.of_match_table = gic_v3_vh_match_table,
	},
};

static int __init gic_v3_vh_modinit(void)
{
	return platform_driver_register(&gic_v3_vh_driver);
}


module_init(gic_v3_vh_modinit);

MODULE_DESCRIPTION("Vendor hook driver for GIC v3");
MODULE_AUTHOR("Hyunki Koo <hyunki00.koo@samsung.com>");
MODULE_AUTHOR("Donghoon Yu <hoony.yu@samsung.com>");
MODULE_LICENSE("GPL v2");
