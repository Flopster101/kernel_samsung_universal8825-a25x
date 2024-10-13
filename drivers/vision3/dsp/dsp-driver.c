// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include "dsp-log.h"
#include "dsp-binary.h"
#include "dsp-device.h"
#include "dsp-task.h"
#include "dsp-debug-core.h"

struct dsp_driver {
	struct dsp_device		dspdev;
	struct dsp_task_manager		task_manager;
	struct dsp_debug		debug;
};

static int __dsp_driver_get_drvdata(struct device *dev, void **dspdev)
{
	void *data;

	dsp_enter();
	data = dev_get_drvdata(dev);
	if (!data) {
		dsp_err("drvdata is NULL\n");
		return -EFAULT;
	}

	if (!dspdev) {
		dsp_err("target memory for dspdev must not be NULL\n");
		return -EINVAL;
	}

	*dspdev = data;
	dsp_leave();
	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int dsp_driver_resume(struct device *dev)
{
	int ret;
	void *dspdev;

	dsp_enter();
	ret = __dsp_driver_get_drvdata(dev, &dspdev);
	if (ret)
		goto p_err;

	ret = dsp_device_resume(dspdev);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_driver_suspend(struct device *dev)
{
	int ret;
	void *dspdev;

	dsp_enter();
	ret = __dsp_driver_get_drvdata(dev, &dspdev);
	if (ret)
		goto p_err;

	ret = dsp_device_suspend(dspdev);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
#endif

static int dsp_driver_runtime_resume(struct device *dev)
{
	int ret;
	void *dspdev;

	dsp_enter();
	ret = __dsp_driver_get_drvdata(dev, &dspdev);
	if (ret)
		goto p_err;

	ret = dsp_device_runtime_resume(dspdev);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_driver_runtime_suspend(struct device *dev)
{
	int ret;
	void *dspdev;

	dsp_enter();
	ret = __dsp_driver_get_drvdata(dev, &dspdev);
	if (ret)
		goto p_err;

	ret = dsp_device_runtime_suspend(dspdev);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static const struct dev_pm_ops dsp_driver_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			dsp_driver_suspend,
			dsp_driver_resume)
	SET_RUNTIME_PM_OPS(
			dsp_driver_runtime_suspend,
			dsp_driver_runtime_resume,
			NULL)
};

static int dsp_driver_probe(struct platform_device *pdev)
{
	int ret;
	struct dsp_driver *drv;

	dsp_enter();
	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc driver\n");
		goto p_err;
	}

	dsp_binary_init(&pdev->dev);

	ret = dsp_device_probe(&drv->dspdev, drv, &pdev->dev);
	if (ret)
		goto p_err_device;

	ret = dsp_task_manager_probe(&drv->task_manager);
	if (ret)
		goto p_err_tmgr;

	ret = dsp_debug_probe(&drv->debug, &drv->dspdev);
	if (ret)
		goto p_err_debug;

	dsp_leave();
	dsp_info("dsp initialization completed\n");
	return 0;
p_err_debug:
	dsp_task_manager_remove(&drv->task_manager);
p_err_tmgr:
	dsp_device_remove(&drv->dspdev);
p_err_device:
	devm_kfree(&pdev->dev, drv);
p_err:
	return ret;
}

static int dsp_driver_remove(struct platform_device *pdev)
{
	struct dsp_device *dspdev;
	struct dsp_driver *drv;

	dsp_enter();
	dspdev = dev_get_drvdata(&pdev->dev);

	drv = dsp_device_get_drvdata(dspdev);
	if (!drv) {
		dsp_err("Failed to get drvdata\n");
		return -EFAULT;
	}

	dsp_debug_remove(&drv->debug);
	dsp_task_manager_remove(&drv->task_manager);
	dsp_device_remove(dspdev);
	devm_kfree(&pdev->dev, drv);
	dsp_leave();
	return 0;
}

static void dsp_driver_shutdown(struct platform_device *pdev)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(&pdev->dev);
	dsp_device_shutdown(dspdev);
	dsp_leave();
}

#if defined(CONFIG_OF)
static const struct of_device_id exynos_dsp_match[] = {
	{
		.compatible = "samsung,exynos-dsp",
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_dsp_match);
#endif

static struct platform_driver dsp_pdev = {
	.probe			= dsp_driver_probe,
	.remove			= dsp_driver_remove,
	.shutdown		= dsp_driver_shutdown,
	.driver = {
		.name		= "exynos-dsp",
		.owner		= THIS_MODULE,
		.pm		= &dsp_driver_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table	= of_match_ptr(exynos_dsp_match)
#endif
	}
};

static int __init dsp_driver_init(void)
{
	int ret;

	dsp_enter();
	ret = platform_driver_register(&dsp_pdev);
	if (ret)
		pr_err("[exynos-dsp] Failed to register driver(%d)\n", ret);

	dsp_leave();
	return ret;
}

static void __exit dsp_driver_exit(void)
{
	dsp_enter();
	platform_driver_unregister(&dsp_pdev);
	dsp_leave();
}

#if defined(MODULE)
module_init(dsp_driver_init);
#else
late_initcall(dsp_driver_init);
#endif
module_exit(dsp_driver_exit);

MODULE_AUTHOR("@samsung.com>");
MODULE_DESCRIPTION("Exynos dsp driver");
MODULE_LICENSE("GPL");
