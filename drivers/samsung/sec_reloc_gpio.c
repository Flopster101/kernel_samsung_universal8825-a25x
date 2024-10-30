// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <linux/sec_class.h>
#include <linux/samsung/builder_pattern.h>

struct reloc_gpio_chip {
	const char *label;
	size_t label_len;
	int base;
};

struct reloc_gpio_drvdata {
	struct builder bd;
	struct device *reloc_gpio_dev;
	struct reloc_gpio_chip *chip;
	size_t nr_chip;
	/* relocated gpio number which is request from sysfs interface */
	int gpio_num;
	/* found index after calling gpiochip_find */
	int chip_idx_found;
};

static int __reloc_gpio_parse_dt_reloc_base(struct builder *bd,
		struct device_node *np)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *dev = bd->dev;
	int nr_chip;
	struct reloc_gpio_chip *chip;
	u32 base;
	int i;

	nr_chip = of_property_count_elems_of_size(np, "sec,reloc-base",
			sizeof(u32));
	if (nr_chip < 0) {
		/* assume '0' if -EINVAL or -ENODATA is returned */
		drvdata->nr_chip = 0;
		return 0;
	}

	chip = devm_kmalloc_array(dev, nr_chip, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	for (i = 0; i < nr_chip; i++) {
		int err = of_property_read_u32_index(np, "sec,reloc-base",
				i, &base);
		if (err) {
			dev_err(dev, "can't read sec,reloc-base [%d]\n", i);
			return err;
		}

		chip[i].base = (int)base;
	}

	drvdata->chip = chip;
	drvdata->nr_chip = nr_chip;

	return 0;
}

static int __reloc_gpio_parse_dt_gpio_label(struct builder *bd,
		struct device_node *np)
{
	struct reloc_gpio_drvdata *drvdata =
		container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *dev = bd->dev;
	const char *label;
	struct reloc_gpio_chip *chip;
	int nr_chip;
	int i;

	chip = drvdata->chip;
	nr_chip = drvdata->nr_chip;
	for (i = 0; i < nr_chip; i++) {
		int err = of_property_read_string_helper(np,
				"sec,gpio-label", &label, 1, i);
		if (err < 0) {
			dev_err(dev, "can't read sec,gpio-label [%d]\n", i);
			return err;
		}

		chip[i].label = label;
		chip[i].label_len = strlen(label);
	}

	return 0;
}

static struct dt_builder __reloc_gpio_dt_builder[] = {
	DT_BUILDER(__reloc_gpio_parse_dt_reloc_base),
	DT_BUILDER(__reloc_gpio_parse_dt_gpio_label),
};

static int __reloc_gpio_probe_prolog(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);

	drvdata->gpio_num = -EINVAL;

	return 0;
}

static int __reloc_gpio_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __reloc_gpio_dt_builder,
			ARRAY_SIZE(__reloc_gpio_dt_builder));
}

static int __reloc_gpio_sec_class_create(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *reloc_gpio_dev;

	reloc_gpio_dev = sec_device_create(NULL, "gpio");
	if (IS_ERR(reloc_gpio_dev))
		return PTR_ERR(reloc_gpio_dev);

	dev_set_drvdata(reloc_gpio_dev, drvdata);

	drvdata->reloc_gpio_dev = reloc_gpio_dev;

	return 0;
}

static void __reloc_gpio_sec_class_remove(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *reloc_gpio_dev = drvdata->reloc_gpio_dev;

	if (!reloc_gpio_dev)
		return;

	sec_device_destroy(reloc_gpio_dev->devt);
}
#if 0 // keep the qc code
static bool __reloc_gpio_is_valid_gpio_num(struct reloc_gpio_chip *chip,
		int nr_gpio, int gpio_num)
{
	int min = chip->base;
	int max = chip->base + nr_gpio - 1;

	if ((gpio_num >= min) && (gpio_num <= max))
		return true;

	return false;
}

static bool __reloc_gpio_is_matched(struct gpio_chip *gc,
		struct reloc_gpio_chip *chip, int gpio_num)
{
	/* memcmp is preferred rather than strncmp to include a NULL
	 * termination.
	 */
	if (memcmp(gc->label, chip->label, chip->label_len + 1))
		return false;

	return __reloc_gpio_is_valid_gpio_num(chip, gc->ngpio, gpio_num);
}

static int sec_reloc_gpio_is_matched_gpio_chip(struct gpio_chip *gc,
		void *__drvdata)
{
	struct reloc_gpio_drvdata *drvdata = __drvdata;
	struct reloc_gpio_chip *chip = drvdata->chip;
	size_t nr_chip = drvdata->nr_chip;
	int gpio_num = drvdata->gpio_num;
	size_t i;

	for (i = 0; i < nr_chip; i++) {
		struct reloc_gpio_chip *this_chip = &chip[i];

		if (__reloc_gpio_is_matched(gc, this_chip, gpio_num))
			return 1;
	}

	return 0;
}

static int __reloc_gpio_relocated_to_actual(struct reloc_gpio_drvdata *drvdata)
{
	struct gpio_chip *gc;
	struct reloc_gpio_chip *chip;
	int gpio_num = drvdata->gpio_num;

	if (!drvdata->nr_chip)
		return gpio_num;

	gc = gpiochip_find(drvdata, sec_reloc_gpio_is_matched_gpio_chip);
	if (IS_ERR_OR_NULL(gc))
		return gpio_num;

	/* drvdata->chip_idx_found is determined when 'gpiochip_find' is run. */
	chip = &drvdata->chip[drvdata->chip_idx_found];
	if (gpio_num < chip->base)
		return -ERANGE;

	return (gpio_num - chip->base) + gc->base;
}
#endif
static ssize_t check_requested_gpio_show(struct device *sec_class_dev,
		struct device_attribute *attr, char *buf)
{
	struct reloc_gpio_drvdata *drvdata = dev_get_drvdata(sec_class_dev);
	int gpio_actual = -EINVAL;
	int val;

	if (drvdata->gpio_num < 0) {
		val = -ENODEV;
		goto __finally;
	}
#if 0 // keep the qc code
	gpio_actual = __reloc_gpio_relocated_to_actual(drvdata);

	if (gpio_actual < 0) {
		val = -ENOENT;
		goto __finally;
	}
#else
	gpio_actual = drvdata->gpio_num;

	if (!gpio_is_valid(gpio_actual)) {
		val = -ENOENT;
		goto __finally;
	}
#endif

	val = gpio_get_value(gpio_actual);

__finally:
	drvdata->gpio_num = -EINVAL;
	return scnprintf(buf, PAGE_SIZE, "GPIO[%d] : [%d]", gpio_actual, val);
}

static ssize_t check_requested_gpio_store(struct device *sec_class_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct reloc_gpio_drvdata *drvdata = dev_get_drvdata(sec_class_dev);
	struct device *dev = drvdata->bd.dev;
	int gpio_num;
	int err;

	err = kstrtoint(buf, 10, &gpio_num);
	if (err < 0) {
		dev_warn(dev, "requested gpio number is malformed or wrong\n");
		print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 16, 1,
				buf, count, 1);
		return err;
	}

	drvdata->gpio_num = gpio_num;

	return count;
}

static DEVICE_ATTR(check_requested_gpio, 0664,
		check_requested_gpio_show, check_requested_gpio_store);

static struct attribute *sec_reloc_gpio_attrs[] = {
	&dev_attr_check_requested_gpio.attr,
	NULL,
};

static const struct attribute_group sec_reloc_gpio_attr_group = {
	.attrs = sec_reloc_gpio_attrs,
};

static int __reloc_gpio_sysfs_create(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *dev = drvdata->reloc_gpio_dev;
	int err;

	err = sysfs_create_group(&dev->kobj, &sec_reloc_gpio_attr_group);
	if (err)
		return err;

	return 0;
}

static void __reloc_gpio_sysfs_remove(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
			container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *dev = drvdata->reloc_gpio_dev;

	sysfs_remove_group(&dev->kobj, &sec_reloc_gpio_attr_group);
}

static int __reloc_gpio_epilog(struct builder *bd)
{
	struct reloc_gpio_drvdata *drvdata =
		container_of(bd, struct reloc_gpio_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);

	return 0;
}

static int __reloc_gpio_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct reloc_gpio_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __reloc_gpio_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct reloc_gpio_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static struct dev_builder __reloc_gpio_dev_builder[] = {
	DEVICE_BUILDER(__reloc_gpio_parse_dt, NULL),
	DEVICE_BUILDER(__reloc_gpio_probe_prolog, NULL),
	DEVICE_BUILDER(__reloc_gpio_sec_class_create,
		       __reloc_gpio_sec_class_remove),
	DEVICE_BUILDER(__reloc_gpio_sysfs_create,
		       __reloc_gpio_sysfs_remove),
	DEVICE_BUILDER(__reloc_gpio_epilog, NULL),
};

static int sec_reloc_gpio_probe(struct platform_device *pdev)
{
	return __reloc_gpio_probe(pdev, __reloc_gpio_dev_builder,
			ARRAY_SIZE(__reloc_gpio_dev_builder));
}

static int sec_reloc_gpio_remove(struct platform_device *pdev)
{
	return __reloc_gpio_remove(pdev, __reloc_gpio_dev_builder,
			ARRAY_SIZE(__reloc_gpio_dev_builder));
}

static const struct of_device_id sec_reloc_gpio_match_table[] = {
	{ .compatible = "samsung,reloc_gpio" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_reloc_gpio_match_table);

static struct platform_driver sec_reloc_gpio_driver = {
	.driver = {
		.name = "samsung,reloc_gpio",
		.of_match_table = of_match_ptr(sec_reloc_gpio_match_table),
	},
	.probe = sec_reloc_gpio_probe,
	.remove = sec_reloc_gpio_remove,
};

static int __init sec_reloc_gpio_init(void)
{
	return platform_driver_register(&sec_reloc_gpio_driver);
}
module_init(sec_reloc_gpio_init);

static void __exit sec_reloc_gpio_exit(void)
{
	platform_driver_unregister(&sec_reloc_gpio_driver);
}
module_exit(sec_reloc_gpio_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Legacy-Style Relocated GPIO Interface for Factory Mode");
MODULE_LICENSE("GPL v2");
