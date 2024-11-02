/*
 * Exynos FMP driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * Authors: Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <crypto/fmp.h>

#include "fmp_fips_main.h"
#include "fmp_test.h"
#include "fmp_fips_info.h"

#define FMP_SIM_VERSION "1.0.0"

static void *exynos_fmp_init(struct platform_device *pdev)
{
	struct exynos_fmp *fmp;
	int ret = 0;

	if (!pdev) {
		pr_err("%s: Invalid platform_device.\n", __func__);
		goto err;
	}

	fmp = get_fmp();

	ret = exynos_fmp_fips_register(fmp);
	if (ret) {
		dev_err(&pdev->dev, "%s: Fail to exynos_fmp_fips_register. ret(0x%x)",
				__func__, ret);
		goto err;
	}

	dev_info(&pdev->dev, "Exynos FMP Simulator driver is initialized\n");
	dev_info(&pdev->dev, "Exynos FMP Simulator driver Version: %s\n", FMP_SIM_VERSION);
	return fmp;

err:
	return NULL;
}

void exynos_fmp_exit(struct platform_device *pdev)
{
	struct exynos_fmp *fmp = dev_get_drvdata(&pdev->dev);

	exynos_fmp_fips_deregister(fmp);
	devm_kfree(&pdev->dev, fmp);
}

static int exynos_fmp_probe(struct platform_device *pdev)
{
	struct exynos_fmp *fmp_ctx = exynos_fmp_init(pdev);
	int ret;

	if (!fmp_ctx) {
		dev_err(&pdev->dev,
			"%s: Fail to get fmp_ctx\n", __func__);
		return -EINVAL;
	}

	dev_set_drvdata(&pdev->dev, fmp_ctx);

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	if (ret) {
		dev_warn(&pdev->dev,
			"%s: No suitable DMA available (%d)\n", __func__, ret);
	}

	return 0;
}

static int exynos_fmp_remove(struct platform_device *pdev)
{
	exynos_fmp_exit(pdev);
	return 0;
}

static const struct of_device_id exynos_fmp_match[] = {
	{ .compatible = "samsung,exynos-fmp-simulator" },
	{},
};

static struct platform_driver exynos_fmp_driver = {
	.driver = {
		   .name = "exynos-fmp-simulator",
			.owner = THIS_MODULE,
			.pm = NULL,
		   .of_match_table = exynos_fmp_match,
		   },
	.probe = exynos_fmp_probe,
	.remove = exynos_fmp_remove,
};
module_platform_driver(exynos_fmp_driver);

MODULE_DESCRIPTION("FMP simulator driver");
MODULE_AUTHOR("Jonghwi Rha <jonghwi.rha@samsung.com>");
MODULE_LICENSE("GPL");
