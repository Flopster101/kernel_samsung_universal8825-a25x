/*
 * drivers/media/platform/exynos/mfc/mfc_sysevent.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)

#include <linux/platform_device.h>

#include "mfc_sysevent.h"

int mfc_sysevent_powerup(const struct sysevent_desc *desc)
{
	return 0;
}

int mfc_sysevent_shutdown(const struct sysevent_desc *desc, bool force_stop)
{
	return 0;
}

void mfc_sysevent_crash_shutdown(const struct sysevent_desc *desc)
{
	return;
}

static int mfc_core_sysevent_notifier_cb(struct notifier_block *this, unsigned long code,
								void *_cmd)
{
	struct notif_data *notifdata = NULL;
	notifdata = (struct notif_data *) _cmd;

	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
		break;
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
		break;
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
		break;
	case SYSTEM_EVENT_BEFORE_POWERUP:
		if (_cmd)
			notifdata = (struct notif_data *) _cmd;
		break;
	case SYSTEM_EVENT_AFTER_POWERUP:
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

struct notifier_block mfc_core_nb = {
	.notifier_call = mfc_core_sysevent_notifier_cb,
};

int mfc_core_sysevent_desc_init(struct platform_device *pdev, struct mfc_core *core)
{
	int ret = 0;

	core->sysevent_desc.name = core->name;
	core->sysevent_desc.owner = THIS_MODULE;
	core->sysevent_desc.shutdown = mfc_sysevent_shutdown;
	core->sysevent_desc.powerup = mfc_sysevent_powerup;
	core->sysevent_desc.crash_shutdown = mfc_sysevent_crash_shutdown;
	core->sysevent_desc.dev = &pdev->dev;

	core->sysevent_dev = sysevent_register(&core->sysevent_desc);
	if (IS_ERR(core->sysevent_dev)) {
		ret = PTR_ERR(core->sysevent_dev);
		core->sysevent_dev = NULL;
		mfc_core_err("%s: sysevent_register failed :%d\n", pdev->name, ret);
	} else {
		mfc_core_info("%s: sysevent_register success\n", pdev->name);
		sysevent_notif_register_notifier(core->sysevent_desc.name, &mfc_core_nb);
	}

	return ret;
}

void mfc_core_sysevent_desc_deinit(struct mfc_core *core)
{
	sysevent_unregister(core->sysevent_dev);
	core->sysevent_dev = NULL;
}
#endif
