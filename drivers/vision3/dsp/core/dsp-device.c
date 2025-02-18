// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#if defined(CONFIG_EXYNOS_IOVMM)
#include <linux/exynos_iovmm.h>
#endif
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hardware.h"
#include "dsp-device.h"

static struct device *device_dev;

int dsp_device_npu_start(bool boot, dma_addr_t fw_iova)
{
	int ret;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(device_dev);
	mutex_lock(&dspdev->lock);
	ret = dspdev->system.ops->npu_start(&dspdev->system, boot, fw_iova);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return ret;
}

#if defined(CONFIG_EXYNOS_IOVMM)
static int __attribute__((unused)) dsp_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flag, void *token)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	if (dsp_device_power_active(dspdev)) {
		dsp_err("Invalid access to device virtual(0x%lX)\n",
				fault_addr);
		dspdev->system.ops->iovmm_fault_dump(&dspdev->system);
	} else {
		dsp_info("dsp is not running\n");
	}

	dsp_leave();
	return 0;
}
#else
static int __attribute__((unused)) dsp_fault_handler(struct iommu_fault *fault,
		void *data)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = data;

	if (dsp_device_power_active(dspdev)) {
		if (fault->type == IOMMU_FAULT_DMA_UNRECOV) {
			dsp_err("UNRECOV Fault(%llX)\n", fault->event.addr);
			dsp_err("(%u / %u / %u / %u / %llX)\n",
					fault->event.reason, fault->event.flags,
					fault->event.pasid, fault->event.perm,
					fault->event.fetch_addr);
		} else if (fault->type == IOMMU_FAULT_PAGE_REQ) {
			dsp_err("PAGE_REQ Fault(%llX)\n", fault->prm.addr);
			dsp_err("(%u / %u / %u / %u)\n",
					fault->prm.flags, fault->prm.pasid,
					fault->prm.grpid, fault->prm.perm);
		} else {
			dsp_err("Unknown Fault : %u\n", fault->type);
		}
		dspdev->system.ops->iovmm_fault_dump(&dspdev->system);
	} else {
		dsp_info("dsp is not running\n");
	}

	dsp_leave();
	return 0;
}
#endif

int dsp_device_resume(struct dsp_device *dspdev)
{
	int ret = 0;

	dsp_enter();
	if (dsp_device_power_active(dspdev))
		ret = dspdev->system.ops->resume(&dspdev->system);

	dsp_leave();
	return ret;
}

int dsp_device_suspend(struct dsp_device *dspdev)
{
	int ret = 0;

	dsp_enter();
	if (dsp_device_power_active(dspdev))
		ret = dspdev->system.ops->suspend(&dspdev->system);

	dsp_leave();
	return ret;
}

int dsp_device_runtime_resume(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	ret = dspdev->system.ops->runtime_resume(&dspdev->system);
	if (ret)
		goto p_err_system;

	dsp_leave();
	return 0;
p_err_system:
	return ret;
}

int dsp_device_runtime_suspend(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	ret = dspdev->system.ops->runtime_suspend(&dspdev->system);
	if (ret)
		goto p_err_system;

	dsp_leave();
	return 0;
p_err_system:
	return ret;
}

int dsp_device_power_active(struct dsp_device *dspdev)
{
	dsp_check();
	return dspdev->system.ops->power_active(&dspdev->system);
}

int dsp_device_power_on(struct dsp_device *dspdev, unsigned int pm_level)
{
	int ret;

	dsp_enter();
	if (dsp_device_power_active(dspdev)) {
		if (dspdev->power_count + 1 < dspdev->power_count) {
			ret = -EINVAL;
			dsp_err("power count is overflowed\n");
			goto p_err;
		}

		dspdev->power_count++;
		dsp_warn("Duplicated request to enable power(%u)",
				dspdev->power_count);
		return 0;
	}

	ret = dspdev->system.ops->set_boot_qos(&dspdev->system, pm_level);
	if (ret)
		goto p_err_fail;

#if defined(CONFIG_PM)
	ret = pm_runtime_get_sync(dspdev->dev);
	if (ret) {
		dsp_err("Failed to get runtime_pm(%d)\n", ret);
		goto p_err_fail;
	}
#else
	ret = dsp_device_runtime_resume(dspdev->dev);
	if (ret)
		goto p_err_fail;
#endif

	dspdev->power_count = 1;
	dsp_leave();
	return 0;
p_err_fail:
	dspdev->power_count = 0;
p_err:
	return ret;
}

void dsp_device_power_off(struct dsp_device *dspdev)
{
	dsp_enter();
	if (!dspdev->power_count) {
		dsp_warn("power is already disabled\n");
	} else if (dspdev->power_count == 1) {
#if defined(CONFIG_PM)
		pm_runtime_put_sync(dspdev->dev);
#else
		dsp_device_runtime_suspend(dspdev->dev);
#endif
		dspdev->power_count = 0;
	} else {
		dspdev->power_count--;
		dsp_warn("The power reference count is cleared(%u)\n",
				dspdev->power_count);
	}
	dsp_leave();
}

int dsp_device_start(struct dsp_device *dspdev, unsigned int pm_level)
{
	int ret;

	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->open_count) {
		mutex_unlock(&dspdev->lock);
		ret = -ENOSTR;
		dsp_err("device is not opened\n");
		goto p_err_open;
	}

	if (dspdev->sec_start_count) {
		int retry = 1000;
		bool done = false;

		dsp_warn("secure operation(%u) is running\n",
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		while (retry) {
			mutex_lock(&dspdev->lock);
			if (!dspdev->sec_start_count) {
				done = true;
				break;
			}

			mutex_unlock(&dspdev->lock);
			retry--;
			mdelay(5);
		}

		if (!done) {
			ret = -ETIMEDOUT;
			dsp_err("secure operation(%u) was not finished\n",
					dspdev->sec_start_count);
			goto p_err_check;
		}
	}

	if (dspdev->start_count) {
		if (dspdev->start_count + 1 < dspdev->start_count) {
			ret = -EINVAL;
			dsp_err("start count is overflowed\n");
			goto p_err_count;
		}

		dspdev->start_count++;
		dsp_info("start count is incresed(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	ret = dsp_device_power_on(dspdev, pm_level);
	if (ret)
		goto p_err_power;

	ret = dspdev->system.ops->start(&dspdev->system);
	if (ret)
		goto p_err_system;

	ret = dspdev->system.ops->boot(&dspdev->system);
	if (ret)
		goto p_err_boot;

	dspdev->start_count = 1;
	dsp_info("device is started(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
p_err_boot:
	dspdev->system.ops->stop(&dspdev->system);
p_err_system:
	dsp_device_power_off(dspdev);
p_err_power:
p_err_count:
	mutex_unlock(&dspdev->lock);
p_err_check:
p_err_open:
	return ret;
}

int dsp_device_start_secure(struct dsp_device *dspdev, unsigned int pm_level,
		unsigned int mem_size)
{
	int ret;
	struct dsp_system *sys;
	struct dsp_memory *mem;

	dsp_enter();
	sys = &dspdev->system;
	mem = &sys->memory;

	mutex_lock(&dspdev->lock);
	if (!dspdev->open_count) {
		mutex_unlock(&dspdev->lock);
		ret = -ENOSTR;
		dsp_err("device is not opened\n");
		goto p_err_open;
	}

	if (dspdev->start_count) {
		int retry = 1000;
		bool done = false;

		dsp_warn("normal operation(%u) is running\n",
				dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		while (retry) {
			mutex_lock(&dspdev->lock);
			if (!dspdev->start_count) {
				done = true;
				break;
			}

			mutex_unlock(&dspdev->lock);
			retry--;
			mdelay(5);
		}

		if (!done) {
			ret = -ETIMEDOUT;
			dsp_err("normal operation(%u) was not finished\n",
					dspdev->start_count);
			goto p_err_check;
		}
	}

	if (dspdev->sec_start_count) {
		if (dspdev->sec_start_count + 1 < dspdev->sec_start_count) {
			ret = -EINVAL;
			dsp_err("secure start count is overflowed\n");
			goto p_err_count;
		}

		dspdev->sec_start_count++;
		dsp_info("secure start count is incresed(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	ret = dsp_device_power_on(dspdev, pm_level);
	if (ret)
		goto p_err_power;

	if (mem_size) {
		snprintf(mem->sec_mem.name, DSP_MEM_NAME_LEN, "secure");
		mem->sec_mem.size = mem_size;
		mem->sec_mem.flags = 0;
		mem->sec_mem.dir = DMA_BIDIRECTIONAL;

		ret = mem->ops->ion_alloc_secure(mem, &mem->sec_mem);
		if (ret) {
			mem->sec_mem.size = 0;
			goto p_err_alloc;
		}
	}

	ret = sys->pm.ops->dvfs_disable(&sys->pm, pm_level);
	if (ret)
		goto p_err_dvfs;

	dspdev->sec_pm_level = pm_level;
	dspdev->sec_start_count = 1;
	dsp_info("device is started(secure)(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
p_err_dvfs:
	if (mem_size) {
		mem->ops->ion_free_secure(mem, &mem->sec_mem);
		mem->sec_mem.size = 0;
	}
p_err_alloc:
	dsp_device_power_off(dspdev);
p_err_power:
p_err_count:
	mutex_unlock(&dspdev->lock);
p_err_check:
p_err_open:
	return ret;
}

static int __dsp_device_stop(struct dsp_device *dspdev)
{
	dsp_enter();
	dspdev->system.ops->reset(&dspdev->system);
	dspdev->system.ops->stop(&dspdev->system);
	dsp_device_power_off(dspdev);
	dspdev->start_count = 0;
	dsp_leave();
	return 0;
}

int dsp_device_stop(struct dsp_device *dspdev, unsigned int count)
{
	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->start_count) {
		dsp_warn("device already stopped(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->start_count > count) {
		dspdev->start_count -= count;
		dsp_info("start count is decreased(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->start_count < count)
		dsp_warn("start count is unstable(%u/%u)",
				dspdev->start_count, count);

	__dsp_device_stop(dspdev);

	dsp_info("device stopped(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
}

static int __dsp_device_stop_secure(struct dsp_device *dspdev)
{
	struct dsp_system *sys;

	dsp_enter();
	sys = &dspdev->system;

	sys->pm.ops->dvfs_enable(&sys->pm, dspdev->sec_pm_level);
	sys->memory.ops->ion_free_secure(&sys->memory, &sys->memory.sec_mem);
	sys->memory.sec_mem.size = 0;
	dspdev->sec_pm_level = -1;
	dsp_device_power_off(dspdev);
	dspdev->sec_start_count = 0;
	dsp_leave();
	return 0;
}

int dsp_device_stop_secure(struct dsp_device *dspdev, unsigned int count)
{
	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->sec_start_count) {
		dsp_warn("device already stopped(secure)(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->sec_start_count > count) {
		dspdev->sec_start_count -= count;
		dsp_info("secure start count is decreased(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->sec_start_count < count)
		dsp_warn("secure start count is unstable(%u/%u)",
				dspdev->sec_start_count, count);

	__dsp_device_stop_secure(dspdev);

	dsp_info("device stopped(secure)(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
}

int dsp_device_open(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		if (dspdev->open_count + 1 < dspdev->open_count) {
			ret = -EINVAL;
			dsp_err("open count is overflowed\n");
			goto p_err_count;
		}

		dspdev->open_count++;
		dsp_info("open count is incresed(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	ret = dspdev->system.ops->open(&dspdev->system);
	if (ret)
		goto p_err_system;

	dspdev->open_count = 1;
	dspdev->start_count = 0;
	dspdev->sec_start_count = 0;
	dsp_info("device TAG : OFI_SDK_%d_%d_%d\n",
			GET_DRIVER_MAJOR_VERSION(dspdev->version),
			GET_DRIVER_MINOR_VERSION(dspdev->version),
			GET_DRIVER_BUILD_VERSION(dspdev->version));
	dsp_info("device is opened(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
p_err_system:
p_err_count:
	mutex_unlock(&dspdev->lock);
	return ret;
}

int dsp_device_close(struct dsp_device *dspdev)
{
	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->open_count) {
		dsp_warn("device is already closed(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->open_count != 1) {
		dspdev->open_count--;
		dsp_info("open count is decreased(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->start_count) {
		dsp_warn("device will forcibly stop(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		dspdev->start_count = 1;
		__dsp_device_stop(dspdev);
	} else if (dspdev->sec_start_count) {
		dsp_warn("device will forcibly stop(secure)(%u/%u/%u)\n",
				dspdev->open_count, dspdev->start_count,
				dspdev->sec_start_count);
		dspdev->sec_start_count = 1;
		__dsp_device_stop_secure(dspdev);
	}

	dspdev->system.ops->close(&dspdev->system);

	dspdev->open_count = 0;
	dsp_info("device is closed(%u/%u/%u)\n",
			dspdev->open_count, dspdev->start_count,
			dspdev->sec_start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
}

static int __dsp_device_parse_dt(struct dsp_device *dspdev,
		struct device_node *np)
{
	int ret;
	unsigned int dev_id;

	dsp_enter();
	ret = of_property_read_u32(np, "samsung,dsp_dev_id", &dev_id);
	if (ret) {
		dsp_err("Failed to parse dsp_dev_id property(%d)\n", ret);
		goto p_err;
	}

	if (dev_id >= DSP_DEVICE_ID_END) {
		ret = -EINVAL;
		dsp_err("dev_id(%u) is not supported(%u ~ %u)\n",
				dev_id, DSP_DEVICE_ID_START,
				DSP_DEVICE_ID_END - 1);
		goto p_err;
	}

	dsp_info("dev_id : %u\n", dev_id);
	dspdev->dev_id = dev_id;

	ret = dsp_hardware_set_ops(dev_id, &dspdev->system);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_device_probe(struct dsp_device *dspdev, void *drv, void *dev)
{
	int ret;

	dsp_enter();
	device_dev = dev;
#if defined(CONFIG_EXYNOS_DSP_HW_N1) || defined(CONFIG_EXYNOS_DSP_HW_N3)
	dev_set_socdata(dev, "Exynos", "dsp");
#endif

	get_device(dev);
	dev_set_drvdata(dev, dspdev);
	dspdev->dev = dev;
	dspdev->drv = drv;

	ret = dsp_hardware_register_ops();
	if (ret)
		goto p_err_init;

	ret = __dsp_device_parse_dt(dspdev, dspdev->dev->of_node);
	if (ret)
		goto p_err_parse;

	ret = dspdev->system.ops->probe(&dspdev->system, dspdev);
	if (ret)
		goto p_err_system;

	ret = dsp_core_probe(&dspdev->core, dspdev);
	if (ret)
		goto p_err_core;

#if defined(CONFIG_EXYNOS_IOVMM)
	iovmm_set_fault_handler(dspdev->dev, dsp_fault_handler, dspdev);
	ret = iovmm_activate(dspdev->dev);
	if (ret) {
		dsp_err("Failed to activate iovmm(%d)\n", ret);
		goto p_err_iovmm;
	}
#else
	iommu_register_device_fault_handler(dspdev->dev, dsp_fault_handler,
			dspdev);
#endif
	mutex_init(&dspdev->lock);
	dspdev->sec_pm_level = -1;

	dspdev->version = DSP_DRIVER_VERSION(1, 5, 0);
	dsp_leave();
	return 0;
#if defined(CONFIG_EXYNOS_IOVMM)
p_err_iovmm:
	dsp_core_remove(&dspdev->core);
#endif
p_err_core:
	dspdev->system.ops->remove(&dspdev->system);
p_err_system:
p_err_parse:
p_err_init:
	put_device(dev);
	return ret;
}

void dsp_device_remove(struct dsp_device *dspdev)
{
	dsp_enter();
#if defined(CONFIG_EXYNOS_IOVMM)
	iovmm_deactivate(dspdev->dev);
#endif
	dsp_core_remove(&dspdev->core);
	dspdev->system.ops->remove(&dspdev->system);
	put_device(dspdev->dev);
	dsp_leave();
}

void dsp_device_shutdown(struct dsp_device *dspdev)
{
	dsp_check();
}
