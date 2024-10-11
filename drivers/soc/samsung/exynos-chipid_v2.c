/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - CHIP ID support
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <soc/samsung/exynos-smc.h>
#include <linux/module.h>

#define ADDITIONAL_INFO1_OFFSET 0x900C
#define ADDITIONAL_INFO2_OFFSET 0xF07C

struct exynos_chipid_info exynos_soc_info;
EXPORT_SYMBOL(exynos_soc_info);

static const char * product_id_to_name(unsigned int product_id)
{
	const char *soc_name;
	unsigned int soc_id = product_id;

	switch (soc_id) {
	case EXYNOS3250_SOC_ID:
		soc_name = "EXYNOS3250";
		break;
	case EXYNOS4210_SOC_ID:
		soc_name = "EXYNOS4210";
		break;
	case EXYNOS4212_SOC_ID:
		soc_name = "EXYNOS4212";
		break;
	case EXYNOS4412_SOC_ID:
		soc_name = "EXYNOS4412";
		break;
	case EXYNOS4415_SOC_ID:
		soc_name = "EXYNOS4415";
		break;
	case EXYNOS5250_SOC_ID:
		soc_name = "EXYNOS5250";
		break;
	case EXYNOS5260_SOC_ID:
		soc_name = "EXYNOS5260";
		break;
	case EXYNOS5420_SOC_ID:
		soc_name = "EXYNOS5420";
		break;
	case EXYNOS5440_SOC_ID:
		soc_name = "EXYNOS5440";
		break;
	case EXYNOS5800_SOC_ID:
		soc_name = "EXYNOS5800";
		break;
	case EXYNOS7872_SOC_ID:
		soc_name = "EXYNOS7872";
		break;
	case EXYNOS8890_SOC_ID:
		soc_name = "EXYNOS8890";
		break;
	case EXYNOS8895_SOC_ID:
		soc_name = "EXYNOS8895";
		break;
	case EXYNOS9810_SOC_ID:
		soc_name = "EXYNOS9810";
		break;
	case EXYNOS9820_SOC_ID:
		soc_name = "EXYNOS9820";
		break;
	case EXYNOS9830_SOC_ID:
		soc_name = "EXYNOS9830";
		break;
	case EXYNOS2100_SOC_ID:
		soc_name = "EXYNOS2100";
		break;
	case S5E9925_SOC_ID:
		soc_name = "S5E9925";
		break;
	case S5E8825_SOC_ID:
		soc_name = "S5E8825";
		break;
	default:
		soc_name = "UNKNOWN";
	}
	return soc_name;
}
static const struct exynos_chipid_variant drv_data_exynos8890 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x14,
	.rev_reg	= 0x0,
	.main_rev_bit	= 0,
	.sub_rev_bit	= 4,
};

static const struct exynos_chipid_variant drv_data_exynos8895 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos7872 = {
	.product_ver	= 2,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos9810 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos9830 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos2100 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_s5e9925 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_s5e8825 = {
        .product_ver    = 1,
        .unique_id_reg  = 0x04,
        .rev_reg        = 0x10,
        .main_rev_bit   = 20,
        .sub_rev_bit    = 16,
};

static const struct of_device_id of_exynos_chipid_ids[] = {
	{
		.compatible	= "samsung,exynos8890-chipid",
		.data		= &drv_data_exynos8890,
	},
	{
		.compatible	= "samsung,exynos8895-chipid",
		.data		= &drv_data_exynos8895,
	},
	{
		.compatible	= "samsung,exynos7872-chipid",
		.data		= &drv_data_exynos8895,
	},
	{
		.compatible	= "samsung,exynos9810-chipid",
		.data		= &drv_data_exynos9810,
	},
	{
		.compatible	= "samsung,exynos9830-chipid",
		.data		= &drv_data_exynos9830,
	},
	{
		.compatible	= "samsung,exynos2100-chipid",
		.data		= &drv_data_exynos2100,
	},
	{
		.compatible	= "samsung,s5e9925-chipid",
		.data		= &drv_data_s5e9925,
	},
	{
		.compatible     = "samsung,s5e8825-chipid",
		.data           = &drv_data_s5e8825,
	},
	{},
};

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/chip-id
 *  path.
 */
static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t product_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%08X\n", exynos_soc_info.product_id);
}

static ssize_t unique_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%010LX\n", exynos_soc_info.unique_id);
}

static ssize_t lot_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%08X\n", exynos_soc_info.lot_id);
}

static ssize_t revision_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%08X\n", exynos_soc_info.revision);
}

static ssize_t evt_ver_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.revision == 0)
		return snprintf(buf, 14, "EVT0\n");
	else
		return snprintf(buf, 14, "EVT%1X.%1X\n",
				exynos_soc_info.main_rev,
				exynos_soc_info.sub_rev);
}

static ssize_t additional_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", exynos_soc_info.additional_info);

}

static ssize_t gub_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%c\n", exynos_soc_info.gub_info);
}

static ssize_t l_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.l_info)
		return snprintf(buf, 6, "2 3\n");
	else
		return 0;
}

static ssize_t m_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	switch (exynos_soc_info.m_info){
	case 0:
		return 0;
	case 1:
		return snprintf(buf, 4, "5\n");
	case 2:
		return snprintf(buf, 4, "6\n");
	case 3:
		return snprintf(buf, 6, "5 6\n");
	default:
		return 0;
	}
}

static ssize_t b_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.b_info)
		return snprintf(buf, 4, "7\n");
	else
		return 0;
}

static ssize_t md_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.b_info)
		return snprintf(buf, 4, "1\n");
	else
		return 0;
}

static ssize_t i_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.b_info)
		return snprintf(buf, 4, "1\n");
	else
		return 0;
}

static ssize_t g_info_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	switch (exynos_soc_info.g_info){
	case 0:
		return 0;
	case 1:
		return snprintf(buf, 4, "0\n");
	case 2:
		return snprintf(buf, 4, "1\n");
	case 3:
		return snprintf(buf, 6, "0 1\n");
	case 4:
		return snprintf(buf, 4, "2\n");
	case 5:
		return snprintf(buf, 6, "0 2\n");
	case 6:
		return snprintf(buf, 6, "1 2\n");
	case 7:
		return snprintf(buf, 8, "0 1 2\n");
	default:
		return 0;
	}
}

static ssize_t mt_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	switch (exynos_soc_info.mt_info) {
	case 1:
		return snprintf(buf, 4, "CO\n");
	case 2:
		return snprintf(buf, 4, "EW\n");
	default:
		return 0;
	}
}

static DEVICE_ATTR_RO(product_id);
static DEVICE_ATTR_RO(unique_id);
static DEVICE_ATTR_RO(lot_id);
static DEVICE_ATTR_RO(revision);
static DEVICE_ATTR_RO(evt_ver);
static DEVICE_ATTR_RO(additional_info);
static DEVICE_ATTR_RO(gub_info);
static DEVICE_ATTR_RO(l_info);
static DEVICE_ATTR_RO(m_info);
static DEVICE_ATTR_RO(b_info);
static DEVICE_ATTR_RO(md_info);
static DEVICE_ATTR_RO(i_info);
static DEVICE_ATTR_RO(g_info);
static DEVICE_ATTR_RO(mt_info);

static struct attribute *chipid_sysfs_attrs[] = {
	&dev_attr_product_id.attr,
	&dev_attr_unique_id.attr,
	&dev_attr_lot_id.attr,
	&dev_attr_revision.attr,
	&dev_attr_evt_ver.attr,
	&dev_attr_additional_info.attr,
	&dev_attr_gub_info.attr,
	&dev_attr_l_info.attr,
	&dev_attr_m_info.attr,
	&dev_attr_b_info.attr,
	&dev_attr_md_info.attr,
	&dev_attr_i_info.attr,
	&dev_attr_g_info.attr,
	&dev_attr_mt_info.attr,
	NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static ssize_t SVC_AP_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%010llX\n",
			(exynos_soc_info.unique_id));
}

static DEVICE_ATTR_RO(SVC_AP);

static struct attribute *svc_ap_attrs[] = {
	&dev_attr_SVC_AP.attr,
	NULL,
};

static struct attribute_group svc_ap_group = {
	.attrs = svc_ap_attrs,
};

static const struct attribute_group *svc_ap_groups[] = {
	&svc_ap_group,
	NULL,
};

static void svc_ap_release(struct device *dev)
{
	kfree(dev);
}

static int sysfs_create_svc_ap(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *top_kobj = NULL;
	struct device *dev;
	int err;

	/* To find svc kobject */
	top_kobj = &exynos_soc_info.pdev->dev.kobj.kset->kobj;
	if (!top_kobj) {
		pr_err("No exynos_soc kobject\n");
		return -ENODEV;
	}
		
	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Existing path /sys/devices/svc : 0x%pK\n", data);
		else
			pr_info("Created /sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Found svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		pr_err("Error allocating svc_ap device\n");
		return -ENOMEM;
	}

	err = dev_set_name(dev, "AP");
	if (err < 0)
		goto err_name;

	dev->kobj.parent = data;
	dev->groups = svc_ap_groups;
	dev->release = svc_ap_release;

	err = device_register(dev);
	if (err < 0)
		goto err_dev_reg;

	exynos_soc_info.svc_dev = dev;
	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return err;
}

static int chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret) {
		if (exynos_soc_info.pdev) {
			dev_err(&exynos_soc_info.pdev->dev,
				"fail to register chip-id subsys\n");
		} else {
			pr_err("fail to register chip-id subsys\n");
		}
	}

	ret = sysfs_create_svc_ap();
	if (ret)
		pr_err("fail to create svc_ap sysfs\n");

	return ret;
}

static void exynos_chipid_get_chipid_info(void)
{
	const struct exynos_chipid_variant *data = exynos_soc_info.drv_data;
	u64 val;

	val = __raw_readl(exynos_soc_info.reg);

	switch (data->product_ver) {
	case 2:
		exynos_soc_info.product_id = val & EXYNOS_SOC_MASK_V2;
		break;
	case 1:
	default:
		exynos_soc_info.product_id = val & EXYNOS_SOC_MASK;
		break;
	}

	val = __raw_readl(exynos_soc_info.reg + data->rev_reg);
	exynos_soc_info.main_rev = (val >> data->main_rev_bit) & EXYNOS_REV_MASK;
	exynos_soc_info.sub_rev = (val >> data->sub_rev_bit) & EXYNOS_REV_MASK;
	exynos_soc_info.revision = (exynos_soc_info.main_rev << 4) | exynos_soc_info.sub_rev;

	val = __raw_readl(exynos_soc_info.reg + data->unique_id_reg);
	val |= (u64)__raw_readl(exynos_soc_info.reg + data->unique_id_reg + 4) << 32UL;
	exynos_soc_info.unique_id  = val;
	exynos_soc_info.lot_id = val & EXYNOS_LOTID_MASK;
}

void get_additional_info(unsigned long addr) {
	unsigned long additional_info1_val;
	u32 additional_info2_val;

	exynos_smc_readsfr(addr + ADDITIONAL_INFO1_OFFSET, &additional_info1_val);
	additional_info2_val = __raw_readl(exynos_soc_info.reg + ADDITIONAL_INFO2_OFFSET);

	exynos_soc_info.additional_info = additional_info1_val & 0x7f;
	if (exynos_soc_info.additional_info == 0x3) {
		if (((additional_info1_val >> 15) & 0x1ff) == 0xA)
			exynos_soc_info.gub_info = 'A';
		else if (((additional_info1_val >> 15) & 0x1ff) == 0xB)
			exynos_soc_info.gub_info = 'B';
		else
			exynos_soc_info.gub_info = 0;
	} else if (exynos_soc_info.additional_info == 0x4) {
		if (additional_info2_val & BIT(20))
			exynos_soc_info.gub_info = 'A';
		else if (additional_info2_val & BIT(21))
			exynos_soc_info.gub_info = 'B';
		else if (additional_info2_val & BIT(22))
			exynos_soc_info.gub_info = 'C';
		else
			exynos_soc_info.gub_info = 0;
		exynos_soc_info.l_info = (additional_info2_val >> 23) & 0x1;
		exynos_soc_info.m_info = (additional_info2_val >> 24) & 0x3;
		exynos_soc_info.b_info = (additional_info2_val >> 26) & 0x1;
		exynos_soc_info.md_info = (additional_info2_val >> 27) & 0x1;
		exynos_soc_info.i_info = (additional_info2_val >> 28) & 0x1;
		exynos_soc_info.g_info = (additional_info2_val >> 29) & 0x7;
	};

	exynos_soc_info.mt_info = (additional_info2_val >> 2) & 0x3;
};

/**
 *  exynos_chipid_early_init: Early chipid initialization
 *  @dev: pointer to chipid device
 */
void exynos_chipid_early_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;

	if (exynos_soc_info.reg)
		return;

	np = of_find_matching_node_and_match(NULL, of_exynos_chipid_ids, &match);
	if (!np || !match)
		panic("%s, failed to find chipid node or match\n", __func__);

	exynos_soc_info.drv_data = (struct exynos_chipid_variant *)match->data;
	exynos_soc_info.reg = of_iomap(np, 0);
	if (!exynos_soc_info.reg)
		panic("%s: failed to map registers\n", __func__);

	exynos_chipid_get_chipid_info();
}

static int exynos_chipid_probe(struct platform_device *pdev)
{
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device_node *root;
	int ret;
	u32 has_additional_info;
	struct resource res;

	exynos_chipid_early_init();
	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENODEV;

	soc_dev_attr->family = "Samsung Exynos";

	root = of_find_node_by_path("/");
	ret = of_property_read_string(root, "model", &soc_dev_attr->machine);
	of_node_put(root);
	if (ret)
		goto free_soc;

	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%d",
			exynos_soc_info.revision);
	if (!soc_dev_attr->revision)
		goto free_soc;

	soc_dev_attr->soc_id = product_id_to_name(exynos_soc_info.product_id);
	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev))
		goto free_rev;

//	dev_set_socdata(&pdev->dev, "Exynos", "ChipID");
	dev_info(&pdev->dev, "CPU[%s] CPU_REV[0x%x] Detected\n",
			product_id_to_name(exynos_soc_info.product_id),
			exynos_soc_info.revision);
	exynos_soc_info.pdev = pdev;

	chipid_sysfs_init();

	ret = of_property_read_u32(pdev->dev.of_node, "has_additional_info", &has_additional_info);
	if (!ret && !!has_additional_info) {
		ret = of_address_to_resource(pdev->dev.of_node, 0, &res);
		if (!ret) {
			get_additional_info((unsigned long)res.start);
		}
	}

	return 0;
free_rev:
	kfree(soc_dev_attr->revision);
free_soc:
	kfree(soc_dev_attr);
	return -EINVAL;
}

static struct platform_driver exynos_chipid_driver = {
	.driver = {
		.name = "exynos-chipid",
		.of_match_table = of_exynos_chipid_ids,
	},
	.probe = exynos_chipid_probe,
};

module_platform_driver(exynos_chipid_driver);

MODULE_DESCRIPTION("Exynos ChipID driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-chipid");
