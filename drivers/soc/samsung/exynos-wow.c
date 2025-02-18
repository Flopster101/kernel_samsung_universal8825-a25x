/*
 * exynos-wow.c - Exynos Workload Watcher Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <soc/samsung/exynos-wow.h>
#include <linux/of_address.h>

static struct exynos_wow_data *exynos_wow;
static char *exynos_wow_event_name[] = { "CCNT", "RW_ACITVE", "RW_DATA", "RW_REQUEST", "R_MO_COUNT" };

static int exynos_wow_set_polling(struct exynos_wow_data *data)
{
	schedule_delayed_work(&data->dwork, msecs_to_jiffies(data->polling_delay));

	return 0;
}

static int exynos_wow_start_polling(struct exynos_wow_data *data)
{
	schedule_delayed_work(&data->dwork, 0);

	return 0;
}

static int exynos_wow_stop_polling(struct exynos_wow_data *data)
{
	cancel_delayed_work(&data->dwork);

	return 0;
}

static int exynos_wow_set_counter(struct exynos_wow_data *data, int mode)
{
	int i, ret = 0;
	unsigned int value, offset;

	switch (mode) {
		case WOW_PPC_START:
			value = WOW_PPC_PMNC_GLB_CNT_EN | WOW_PPC_PMNC_Q_CH_MODE;
			offset = WOW_PPC_PMNC;
			break;
		case WOW_PPC_STOP:
			value = 0;
			offset = WOW_PPC_PMNC;
			break;
		case WOW_PPC_RESET:
			value = WOW_PPC_PMNC_RESET_CCNT_PMCNT;
			offset = WOW_PPC_PMNC;
			break;
		default:
			return -EINVAL;
	}

	for (i = 0; i < data->nr_ip; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		int j;

		for (j = 0; j < ip_data->nr_base; j++) {
			void __iomem *base = ip_data->wow_base[j];

			// start counter
			writel(value, base + offset);
		}
	}

	return ret;
}

static inline int exynos_wow_start_counter(struct exynos_wow_data *data)
{
	return exynos_wow_set_counter(data, WOW_PPC_START);
}

static inline int exynos_wow_stop_counter(struct exynos_wow_data *data)
{
	return exynos_wow_set_counter(data, WOW_PPC_STOP);
}

static inline int exynos_wow_reset_counter(struct exynos_wow_data *data)
{
	return exynos_wow_set_counter(data, WOW_PPC_RESET);
}

static int exynos_wow_accumulator(struct exynos_wow_data *data)
{
	int i;
	u64 total_ccnt = 0, total_active = 0;

	exynos_wow_stop_counter(data);

	for (i = 0; i < data->nr_ip; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		void __iomem *base = ip_data->wow_base[0];
		u64 rw_data, rw_request, active, ccnt, mo_count;

		// Get counter value
		ccnt = readl(base + WOW_PPC_CCNT);
		active = readl(base + WOW_PPC_PMCNT0);
		rw_data = readl(base + WOW_PPC_PMCNT1) * ip_data->nr_ppc;
		rw_request = readl(base + WOW_PPC_PMCNT2);
		mo_count = readl(base + WOW_PPC_PMCNT3_HIGH) & 0xff;
		mo_count = mo_count << 32 | readl(base + WOW_PPC_PMCNT3);

		// update per IP counter
		ip_data->wow_counter[CCNT] += ccnt; 
		ip_data->wow_counter[RW_ACTIVE] += active;
		ip_data->wow_counter[RW_DATA] += rw_data;
		ip_data->wow_counter[RW_REQUEST] += rw_request;
		ip_data->wow_counter[R_MO_COUNT] += mo_count;

		// update total counter
		if (!ip_data->monitor_only) {
			data->profile.transfer_data += (rw_data * ip_data->bus_width);

			if (total_active < active) {
				total_ccnt = ccnt;
				total_active = active;
			}

			if (ip_data->use_latency) {
				data->profile.nr_requests += rw_request;
				data->profile.mo_count += mo_count;
			}
		}
	}

	data->profile.ccnt += total_ccnt;
	data->profile.active += total_active;

	exynos_wow_reset_counter(data);
	exynos_wow_start_counter(data);

	return 0;
}

int exynos_wow_get_data(struct exynos_wow_profile *result)
{
	struct exynos_wow_data *data = exynos_wow;

	if (data == NULL)
		return -ENODEV;

	mutex_lock(&data->lock);

	exynos_wow_accumulator(data);

	memcpy(result, &data->profile, sizeof(struct exynos_wow_profile));

	mutex_unlock(&data->lock);

	return 0;
}
EXPORT_SYMBOL(exynos_wow_get_data);

static void exynos_wow_work(struct work_struct *work)
{
	struct exynos_wow_data *data =
		container_of(work, struct exynos_wow_data, dwork.work);

	mutex_lock(&data->lock);

	exynos_wow_accumulator(data);

	if (data->mode == WOW_ENABLED)
		exynos_wow_set_polling(data);

	mutex_unlock(&data->lock);
}

/* sysfs nodes for amb control */
#define sysfs_printf(...) count += snprintf(buf + count, PAGE_SIZE, __VA_ARGS__)

static ssize_t
exynos_wow_mode_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	sysfs_printf("%s\n", data->mode ? "enabled" : "disabled");

	return count;
}

static ssize_t
exynos_wow_mode_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);

	data->mode = true;

	return 0;
}

static ssize_t
exynos_wow_raw_counter_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int i, j, count = 0;

	sysfs_printf("IP_NAME ");

	for (i = 0; i < NUM_WOW_EVENT; i++)
		sysfs_printf("%s ", exynos_wow_event_name[i]);

	sysfs_printf("\n");

	for (i = 0; i < data->nr_ip; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];

		sysfs_printf("%s ", ip_data->ppc_name);

		for (j = 0; j < NUM_WOW_EVENT; j++)
			sysfs_printf("%llu ", ip_data->wow_counter[j]);

		sysfs_printf("\n");
	}

	return count;
}

static ssize_t
exynos_wow_total_counter_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	unsigned int i, count = 0;
	struct exynos_wow_profile profile;

	exynos_wow_get_data(&profile);

	sysfs_printf("CCNT | ACTIVE | TRANSFER_DATA | MO_COUNT | NR_REQUEST\n");

	for (i = 0; i < (sizeof(struct exynos_wow_profile) / sizeof(u64)); i++)
		sysfs_printf("%llu ", ((u64*)&profile)[i]);
	sysfs_printf("\n");


	return count;
}

static ssize_t
exynos_wow_start_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	return 0;
}

static ssize_t
exynos_wow_start_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct exynos_wow_data *data = exynos_wow;

	exynos_wow_start_counter(data);
	return 0;
}

static ssize_t
exynos_wow_stop_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	return 0;
}

static ssize_t
exynos_wow_stop_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct exynos_wow_data *data = exynos_wow;

	exynos_wow_stop_counter(data);
	return 0;
}

static DEVICE_ATTR(wow_mode, S_IWUSR | S_IRUGO,
		exynos_wow_mode_show, exynos_wow_mode_store);

static DEVICE_ATTR(raw_counter, S_IRUGO,
		exynos_wow_raw_counter_show, NULL);

static DEVICE_ATTR(total_counter, S_IRUGO,
		exynos_wow_total_counter_show, NULL);

static DEVICE_ATTR(start, S_IWUSR | S_IRUGO,
		exynos_wow_start_show, exynos_wow_start_store);

static DEVICE_ATTR(stop, S_IWUSR | S_IRUGO,
		exynos_wow_stop_show, exynos_wow_stop_store);

static struct attribute *exynos_wow_attrs[] = {
	&dev_attr_wow_mode.attr,
	&dev_attr_raw_counter.attr,
	&dev_attr_total_counter.attr,
	&dev_attr_start.attr,
	&dev_attr_stop.attr,
	NULL,
};

static const struct attribute_group exynos_wow_attr_group = {
	.attrs = exynos_wow_attrs,
};

static int exynos_wow_work_init(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	/*
	if (!thermal_irq_wq) {
		attr.nice = 0;
		attr.no_numa = true;
		cpumask_copy(attr.cpumask, cpu_coregroup_mask(0));

		thermal_irq_wq = alloc_workqueue("%s", WQ_HIGHPRI | WQ_UNBOUND |\
				WQ_MEM_RECLAIM | WQ_FREEZABLE,
				0, "thermal_irq");
		apply_workqueue_attrs(thermal_irq_wq, &attr);
	}
	*/

	INIT_DELAYED_WORK(&data->dwork, exynos_wow_work);

	return ret;
}

static int exynos_wow_ip_init(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int i, ret = 0;
	unsigned int value;

	for (i = 0; i < data->nr_ip; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		int j;

		for (j = 0; j < ip_data->nr_base; j++) {
			void __iomem *base = ip_data->wow_base[j];
#ifdef USE_PPMU
			// Set PPMU event type
			writel(PPMU_PPC_EVENT_TYPE_R_ACTIVE, base + PPMU_PPC_EVENT_EV0_TYPE);
			writel(PPMU_PPC_EVENT_TYPE_RW_DATA, base + PPMU_PPC_EVENT_EV1_TYPE);
			writel(PPMU_PPC_EVENT_TYPE_RW_REQUEST, base + PPMU_PPC_EVENT_EV2_TYPE);
			writel(PPMU_PPC_EVENT_TYPE_R_MO_COUNT, base + PPMU_PPC_EVENT_EV3_TYPE);
#endif
			// Enable counters
			value = 1 << WOW_PPC_CNTENS_CCNT_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT0_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT1_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT2_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT3_OFFSET;
			writel(value, base + WOW_PPC_CNTENS);

			// Run counters
			writel(WOW_PPC_PMNC_GLB_CNT_EN | WOW_PPC_PMNC_Q_CH_MODE, base + WOW_PPC_PMNC);
		}
	}

	return ret;
}

static int exynos_wow_parse_dt(struct platform_device *pdev)
{
	struct device_node *np, *wow_ip, *child;
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	struct resource res;
	int i = 0;

	np = pdev->dev.of_node;
	if (of_property_read_u32(np, "polling_delay", &data->polling_delay))
		data->polling_delay = 60000;
	dev_info(&pdev->dev, "polling_delay (%u)\n", data->polling_delay);

	/* Get wow-ip info */
	wow_ip = of_find_node_by_name(np, "wow-ip");
	data->nr_ip = of_get_child_count(wow_ip);
	data->ip_data = kzalloc(sizeof(struct exynos_wow_ip_data) *
			data->nr_ip, GFP_KERNEL);

	for_each_available_child_of_node(wow_ip, child) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i++];
		int index = 0, reg_index[10];

		if (of_property_read_u32(child, "bus_width", &ip_data->bus_width))
			ip_data->bus_width = 32;
		dev_info(&pdev->dev, "bus_width (%u)\n", ip_data->bus_width);

		if (of_property_read_u32(child, "nr_ppc", &ip_data->nr_ppc))
			ip_data->nr_ppc = 1;
		dev_info(&pdev->dev, "nr_ppc (%u)\n", ip_data->nr_ppc);

		if (of_property_read_u32(child, "nr_base", &ip_data->nr_base))
			ip_data->nr_base = 1;
		dev_info(&pdev->dev, "nr_base (%u)\n", ip_data->nr_base);

		if (of_property_read_bool(child, "monitor_only"))
			ip_data->monitor_only = true;
		dev_info(&pdev->dev, "monitor_only (%d)\n", ip_data->monitor_only);

		if (of_property_read_bool(child, "use_latency"))
			ip_data->use_latency = true;
		dev_info(&pdev->dev, "use_latency (%d)\n", ip_data->use_latency);

		strncpy(ip_data->ppc_name, child->name, PPC_NAME_LENGTH);

		of_property_read_u32_array(child, "reg_index", reg_index, ip_data->nr_base);

		ip_data->wow_base = kzalloc(sizeof(void __iomem *) * ip_data->nr_base, GFP_KERNEL);

		for (index = 0; index < ip_data->nr_base; index++) {
			of_address_to_resource(np, reg_index[index], &res);

			ip_data->wow_base[index] = devm_ioremap(&pdev->dev, res.start, resource_size(&res));
			dev_info(&pdev->dev, "paddr (0x%llx) vaddr (0x%llx)\n", res.start, ip_data->wow_base[index]);
		}
	}

	return 0;
}

static int exynos_wow_probe(struct platform_device *pdev)
{
	int ret = 0;

	exynos_wow = kzalloc(sizeof(struct exynos_wow_data), GFP_KERNEL);
	platform_set_drvdata(pdev, exynos_wow);

	ret = exynos_wow_parse_dt(pdev);

	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt (%ld)\n", ret);
		kfree(exynos_wow);
		exynos_wow = NULL;
		return ret;
	}

	mutex_init(&exynos_wow->lock);

	exynos_wow_ip_init(pdev);

	exynos_wow_work_init(pdev);

//	wow->pm_notify.notifier_call = exynos_wow_pm_notify;
//	register_pm_notifier(&wow->pm_notify);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_wow_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos wow attr group");

	exynos_wow->mode = WOW_ENABLED;

	exynos_wow_start_polling(exynos_wow);

	dev_info(&pdev->dev, "Probe exynos wow successfully\n");

	return 0;
}

static int exynos_wow_remove(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);

	kfree(data);

	return 0;
}

static int exynos_wow_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	// lock
	mutex_lock(&data->lock);

	// disable wow
	data->mode = WOW_DISABLED;

	// unlock
	mutex_unlock(&data->lock);

	exynos_wow_stop_polling(data);
	exynos_wow_stop_counter(data);

	return ret;
}


static int exynos_wow_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	// lock
	mutex_lock(&data->lock);

	// enable wow
	exynos_wow_ip_init(pdev);
	data->mode = WOW_ENABLED;

	// unlock
	mutex_unlock(&data->lock);

	exynos_wow_start_polling(data);

	return ret;
}

static const struct of_device_id exynos_wow_match[] = {
	{ .compatible = "samsung,exynos-wow", },
	{ /* sentinel */ },
};

static const struct dev_pm_ops exynos_wow_pm_ops = {
	.suspend_late = exynos_wow_suspend,
	.resume_early = exynos_wow_resume,
};

static struct platform_driver exynos_wow_driver = {
	.driver = {
		.name   = "exynos-wow",
		.of_match_table = exynos_wow_match,
		.suppress_bind_attrs = true,
		.pm = &exynos_wow_pm_ops,
	},
	.probe = exynos_wow_probe,
	.remove	= exynos_wow_remove,
};
module_platform_driver(exynos_wow_driver);

MODULE_DEVICE_TABLE(of, exynos_wow_match);

MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("EXYNOS Workload Watcher Driver");
MODULE_LICENSE("GPL");
