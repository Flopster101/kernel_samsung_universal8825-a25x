/*
 * Samsung Exynos SoC series NPU HW driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_HWDEVICE_H_
#define _NPU_HWDEVICE_H_

#include "npu-device.h"
#include "npu-scheduler.h"

#define NPU_HWDEVICE_MAX_DEVICES	10
enum npu_hw_device_id {	/* all devices id */
	NPU_HWDEV_ID_MISC = 0x0,
	NPU_HWDEV_ID_DNC = 0x1,
	NPU_HWDEV_ID_NPU = 0x2,
	NPU_HWDEV_ID_DSP = 0x4,
	NPU_HWDEV_ID_MIF = 0x8,
	NPU_HWDEV_ID_INT = 0x10,
	NPU_HWDEV_ID_CL0 = 0x20,
	NPU_HWDEV_ID_CL1 = 0x40,
};

enum npu_hw_device_status {	/* all status including error */
	NPU_HWDEV_STATUS_PWR_CLK_OFF = 0x0,
	NPU_HWDEV_STATUS_PWR_CLK_ON = 0x1,
	NPU_HWDEV_STATUS_ACTIVE = 0x2,
	NPU_HWDEV_STATUS_ERROR = 0x4,
};

enum npu_hw_device_type {
	NPU_HWDEV_TYPE_PWRCTRL = 0x1,
	NPU_HWDEV_TYPE_CLKCTRL = 0x2,
	NPU_HWDEV_TYPE_DVFS = 0x4,
	NPU_HWDEV_TYPE_BTS = 0x8,
};

struct npu_hw_device;
struct npu_hw_refcount {
	atomic_t	refcount;
	struct npu_hw_device	*hdev;
	int	(*first)(struct npu_device *device,
			struct npu_hw_device *hdev);
	int	(*final)(struct npu_device *device,
			struct npu_hw_device *hdev);
};

struct npu_hw_ops {
	int	(*boot)(struct npu_hw_device *hdev, bool on);
	int	(*init)(struct npu_hw_device *hdev, bool on);
};

struct npu_hw_device {
	struct device *dev;
	struct npu_device *device;
	struct npu_clocks clks;
	char* name;
	enum npu_hw_device_id id;
	char* parent;

	struct npu_hw_refcount boot_cnt;
	struct npu_hw_refcount init_cnt;

	struct npu_scheduler_dvfs_info *dvfs;
	unsigned int	fps_load;	/* 0.01 unit */
	s32 idle_load;

	u32 status;		// including error status
	u32 type;
	struct npu_hw_ops ops;
};

#if 0
static inline struct npu_hw_device *get_hdev(
				struct npu_device *device, char *name)
{
	int i;
	struct npu_hw_device *hdev = NULL;
	for (i = 0; i < NPU_HWDEVICE_MAX_DEVICES; i++) {
		if (!strcmp((*(device->hdevs) + i)->name, name)) {
			hdev = (*(device->hdevs) + i);
			break;
		}
	}
	return hdev;
}
#endif
struct npu_hw_device *npu_get_hdev(char *name);
struct npu_hw_device *npu_get_hdev_by_id(int id);

static inline int npu_hw_ref_get(
		struct npu_device *device, struct npu_hw_refcount *hw_ref)
{
	if (!hw_ref->first)
		return 0;

	return (atomic_inc_return(&hw_ref->refcount) == 1) ?
		hw_ref->first(device, hw_ref->hdev) : 0;
}

static inline int npu_hw_ref_put(
		struct npu_device *device, struct npu_hw_refcount *hw_ref)
{
	if (!hw_ref->final)
		return 0;

	return (atomic_dec_return(&hw_ref->refcount) == 0) ?
		hw_ref->final(device, hw_ref->hdev) : 0;
}

static inline int npu_hw_ref_open(
		struct npu_device *device, struct npu_hw_device *hdev)
{
	int ret = 0;
	struct npu_hw_device *phdev;

	if (hdev->parent) {
		phdev = npu_get_hdev(hdev->parent);
		if (!phdev) {
			npu_err("no hw-device as %s\n", hdev->parent);
			ret = -ENODEV;
			goto error_hwref;
		}
		npu_hw_ref_get(device, &phdev->boot_cnt);
	}

	if (hdev->ops.boot)
		ret = (hdev->ops.boot(hdev, true));
error_hwref:
	return ret;
}

static inline int npu_hw_ref_close(
		struct npu_device *device, struct npu_hw_device *hdev)
{
	int ret = 0;
	struct npu_hw_device *phdev;

	if (hdev->ops.boot)
		ret = (hdev->ops.boot)(hdev, false);

	if (ret)
		goto error_hwref;

	if (hdev->parent) {
		phdev = npu_get_hdev(hdev->parent);
		if (!phdev) {
			npu_err("no hw-device as %s\n", hdev->parent);
			ret = -ENODEV;
			goto error_hwref;
		}
		npu_hw_ref_put(device, &phdev->boot_cnt);
	}
error_hwref:
	return ret;
}

static inline int npu_hw_ref_init(
		struct npu_device *device, struct npu_hw_device *hdev)
{
	int ret = 0;
	struct npu_hw_device *phdev;

	if (hdev->parent) {
		phdev = npu_get_hdev(hdev->parent);
		if (!phdev) {
			npu_err("no hw-device as %s\n", hdev->parent);
			ret = -ENODEV;
			goto error_hwref;
		}
		npu_hw_ref_get(device, &phdev->init_cnt);
	}

	if (hdev->ops.init)
		ret = (hdev->ops.init(hdev, true));
error_hwref:
	return ret;
}

static inline int npu_hw_ref_deinit(
		struct npu_device *device, struct npu_hw_device *hdev)
{
	int ret = 0;
	struct npu_hw_device *phdev;

	if (hdev->ops.init)
		ret = (hdev->ops.init(hdev, false));

	if (ret)
		goto error_hwref;

	if (hdev->parent) {
		phdev = npu_get_hdev(hdev->parent);
		if (!phdev) {
			npu_err("no hw-device as %s\n", hdev->parent);
			ret = -ENODEV;
			goto error_hwref;
		}
		npu_hw_ref_put(device, &phdev->init_cnt);
	}
error_hwref:
	return ret;
}

static inline void npu_hw_ref_setup(
	struct npu_hw_refcount *hw_ref, struct npu_hw_device *hdev,
	int (*first)(struct npu_device *device, struct npu_hw_device *hdev),
	int (*final)(struct npu_device *device, struct npu_hw_device *hdev))
{
	if (!hdev->ops.boot || !hdev->ops.init) {
		atomic_set(&hw_ref->refcount, 0);
		return;
	}

	hw_ref->hdev = hdev;
	hw_ref->first = first;
	hw_ref->final = final;
	atomic_set(&hw_ref->refcount, 0);
}

int npu_hwdev_register_hwdevs(struct npu_device *device, struct npu_system *system);
int npu_hwdev_bootup(struct npu_device *device, __u32 hids);
int npu_hwdev_shutdown(struct npu_device *device, __u32 hids);
int npu_hwdev_recovery_shutdown(struct npu_device *device);
int npu_hwdev_hwacg(struct npu_system *system, u32 hids, bool on);

int __init npu_hwdev_register(void);
void __exit npu_hwdev_unregister(void);
#endif // _NPU_HWDEVICE_H_
