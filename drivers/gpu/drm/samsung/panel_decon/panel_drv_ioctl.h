/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DRV_IOCTL_H__
#define __PANEL_DRV_IOCTL_H__
#include "panel_drv.h"

typedef int panel_drv_ioctl_t(struct panel_device *panel, void *arg);

struct panel_drv_ioctl_desc {
    unsigned int cmd;
    panel_drv_ioctl_t *func;
    const char *name;
};

#define PANEL_DRV_IOCTL_DEF(ioctl, _func) \
	[_IOC_NR(ioctl)] = {       \
		.cmd = ioctl,           \
		.func = _func,          \
		.name = #ioctl          \
	}

size_t panel_drv_ioctl_scnprintf_cmd(char *buf, size_t size, unsigned int cmd);
size_t panel_drv_ioctl_scnprintf_cmd_name(char *buf, size_t size, unsigned int cmd);
long panel_ioctl(struct panel_device *panel, unsigned int cmd, void *arg);
#endif /* __PANEL_DRV_IOCTL_H__ */
