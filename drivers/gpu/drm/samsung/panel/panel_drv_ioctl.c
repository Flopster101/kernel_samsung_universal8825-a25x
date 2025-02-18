// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/module.h>
#include "panel_drv_ioctl.h"
#include "panel_drv.h"
#include "panel_debug.h"

size_t panel_drv_ioctl_scnprintf_cmd(char *buf, size_t size, unsigned int cmd)
{
	return scnprintf(buf, size, "cmd=0x%x (%c nr=%d len=%d dir=%d)",
			cmd, !(_IOC_TYPE(cmd)) ? ' ' : _IOC_TYPE(cmd),
			_IOC_NR(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
}

size_t panel_drv_ioctl_scnprintf_cmd_name(char *buf, size_t size, unsigned int cmd)
{
    static const char *ioctl_cmd_names[] = {
		[_IOC_NR(PANEL_IOC_ATTACH_ADAPTER)] = "ATTACH_ADAPTER",
		[_IOC_NR(PANEL_IOC_GET_PANEL_STATE)] = "GET_PANEL_STATE",
		[_IOC_NR(PANEL_IOC_PANEL_PROBE)] = "PANEL_PROBE",
		[_IOC_NR(PANEL_IOC_SET_POWER)] = "SET_POWER",
		[_IOC_NR(PANEL_IOC_SLEEP_IN)] = "SLEEP_IN",
		[_IOC_NR(PANEL_IOC_SLEEP_OUT)] = "SLEEP_OUT",
		[_IOC_NR(PANEL_IOC_DISP_ON)] = "DISP_ON",
		[_IOC_NR(PANEL_IOC_PANEL_DUMP)] = "PANEL_DUMP",
		[_IOC_NR(PANEL_IOC_EVT_FRAME_DONE)] = "EVT_FRAME_DONE",
		[_IOC_NR(PANEL_IOC_EVT_VSYNC)] = "EVT_VSYNC",
#ifdef CONFIG_USDM_PANEL_LPM
		[_IOC_NR(PANEL_IOC_DOZE)] = "DOZE",
		[_IOC_NR(PANEL_IOC_DOZE_SUSPEND)] = "DOZE_SUSPEND",
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
		[_IOC_NR(PANEL_IOC_GET_DISPLAY_MODE)] = "GET_DISPLAY_MODE",
		[_IOC_NR(PANEL_IOC_SET_DISPLAY_MODE)] = "SET_DISPLAY_MODE",
		[_IOC_NR(PANEL_IOC_REG_DISPLAY_MODE_CB)] = "REG_DISPLAY_MODE_CB",
#endif
		[_IOC_NR(PANEL_IOC_REG_RESET_CB)] = "REG_RESET_CB",
		[_IOC_NR(PANEL_IOC_REG_VRR_CB)] = "REG_VRR_CB",
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
		[_IOC_NR(PANEL_IOC_SET_MASK_LAYER)] = "SET_MASK_LAYER",
#endif
	};
	int nr = _IOC_NR(cmd);

	if (_IOC_TYPE(cmd) != PANEL_IOC_BASE ||
			nr >= ARRAY_SIZE(ioctl_cmd_names) || !ioctl_cmd_names[nr])
		return panel_drv_ioctl_scnprintf_cmd(buf, size, cmd);

	return scnprintf(buf, size, "%s", ioctl_cmd_names[nr]);
}

/* panel driver ioctl table */
static const struct panel_drv_ioctl_desc panel_drv_ioctls[] = {
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_ATTACH_ADAPTER, panel_drv_attach_adapter_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_GET_PANEL_STATE, panel_drv_get_panel_state_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_PANEL_PROBE, panel_drv_panel_probe_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_SET_POWER, panel_drv_set_power_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_SLEEP_IN, panel_drv_sleep_in_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_SLEEP_OUT, panel_drv_sleep_out_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_DISP_ON, panel_drv_disp_on_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_PANEL_DUMP, panel_drv_panel_dump_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_EVT_FRAME_DONE, panel_drv_evt_frame_done_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_EVT_VSYNC, panel_drv_evt_vsync_ioctl),
#ifdef CONFIG_USDM_PANEL_LPM
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_DOZE, panel_drv_doze_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_DOZE_SUSPEND, panel_drv_doze_suspend_ioctl),
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_GET_DISPLAY_MODE, panel_drv_get_display_mode_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_SET_DISPLAY_MODE, panel_drv_set_display_mode_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_REG_DISPLAY_MODE_CB, panel_drv_reg_display_mode_cb_ioctl),
#endif
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_REG_RESET_CB, panel_drv_reg_reset_cb_ioctl),
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_REG_VRR_CB, panel_drv_reg_vrr_cb_ioctl),
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	PANEL_DRV_IOCTL_DEF(PANEL_IOC_SET_MASK_LAYER, panel_drv_set_mask_layer_ioctl),
#endif
};

long panel_ioctl(struct panel_device *panel, unsigned int cmd, void *arg)
{
	char buf[128];
	int nr = _IOC_NR(cmd);
	int ret;

	if (!panel)
		return -EINVAL;

	if (nr >= ARRAY_SIZE(panel_drv_ioctls) ||
			cmd != panel_drv_ioctls[nr].cmd ||
			panel_drv_ioctls[nr].func == NULL) {
		panel_drv_ioctl_scnprintf_cmd(buf, ARRAY_SIZE(buf), cmd);
		panel_warn("unknown ioctl: %s\n", buf);
		return -EINVAL;
	}

	panel_mutex_lock(&panel->io_lock);
	ret = panel_drv_ioctls[nr].func(panel, arg);
	if (ret < 0) {
		panel_drv_ioctl_scnprintf_cmd(buf, ARRAY_SIZE(buf), cmd);
		panel_err("ioctl(%s) failed. ret:%d\n", buf, ret);
	}
	panel_mutex_unlock(&panel->io_lock);

	return (long)ret;
}

MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_AUTHOR("<gwanghui.lee@samsung.com>");
MODULE_AUTHOR("<kernel.lee@samsung.com>");
MODULE_LICENSE("GPL");
