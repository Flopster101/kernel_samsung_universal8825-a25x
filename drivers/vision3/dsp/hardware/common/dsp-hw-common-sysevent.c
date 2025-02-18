// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/platform_device.h>

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-sysevent.h"

int dsp_hw_common_sysevent_get(struct dsp_sysevent *sysevent)
{
	int ret;
	void *retval;

	dsp_enter();
	if (sysevent->dev) {
		retval = sysevent_get(sysevent->desc.name);
		if (IS_ERR(retval)) {
			ret = PTR_ERR(retval);
			dsp_err("Failed to get sysevent(%d)\n", ret);
			sysevent->enabled = false;
			goto p_err;

		}

		sysevent->enabled = true;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_hw_common_sysevent_put(struct dsp_sysevent *sysevent)
{
	dsp_enter();
	if (sysevent->dev && sysevent->enabled) {
		sysevent_put(sysevent->dev);
		sysevent->enabled = false;
	}
	dsp_leave();
	return 0;
}

static int dsp_hw_common_sysevent_shutdown(const struct sysevent_desc *desc,
		bool force_stop)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_sysevent_powerup(const struct sysevent_desc *desc)
{
	dsp_check();
	return 0;
}

static void dsp_hw_common_sysevent_crash_shutdown(
		const struct sysevent_desc *desc)
{
	dsp_check();
}

static int dsp_hw_common_sysevent_ramdump(int enable,
		const struct sysevent_desc *desc)
{
	dsp_check();
	return 0;
}

static int dsp_hw_common_sysevent_notifier_cb(struct notifier_block *this,
		unsigned long code, void *cmd)
{
	struct notif_data *notifdata;

	notifdata = (struct notif_data *)cmd;
	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
	case SYSTEM_EVENT_BEFORE_POWERUP:
	case SYSTEM_EVENT_AFTER_POWERUP:
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
	case SYSTEM_EVENT_POWERUP_FAILURE:
	case SYSTEM_EVENT_SOC_RESET:
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block sysevent_nb = {
	.notifier_call = dsp_hw_common_sysevent_notifier_cb,
};

int dsp_hw_common_sysevent_probe(struct dsp_sysevent *sysevent, void *sys)
{
	int ret;
	struct sysevent_desc *desc;
	struct platform_device *pdev;

	dsp_enter();
	sysevent->sys = sys;
	desc = &sysevent->desc;
	pdev = to_platform_device(sysevent->sys->dev);

	desc->name = pdev->name;
	desc->dev = sysevent->sys->dev;
	desc->owner = THIS_MODULE;
	desc->shutdown = dsp_hw_common_sysevent_shutdown;
	desc->powerup = dsp_hw_common_sysevent_powerup;
	desc->crash_shutdown = dsp_hw_common_sysevent_crash_shutdown;
	desc->ramdump = dsp_hw_common_sysevent_ramdump;

	sysevent->dev = sysevent_register(desc);
	if (IS_ERR(sysevent->dev)) {
		ret = PTR_ERR(sysevent->dev);
		dsp_err("Failed to register sysevent(%d)\n", ret);
		goto p_err;
	}

	sysevent_notif_register_notifier(desc->name, &sysevent_nb);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_sysevent_remove(struct dsp_sysevent *sysevent)
{
	dsp_enter();
	sysevent_unregister(sysevent->dev);
	dsp_leave();
}

int dsp_hw_common_sysevent_set_ops(struct dsp_sysevent *sysevent,
		const void *ops)
{
	dsp_enter();
	sysevent->ops = ops;
	dsp_leave();
	return 0;
}
