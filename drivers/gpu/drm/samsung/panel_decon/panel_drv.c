// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/ctype.h>
#include <video/mipi_display.h>
#include "panel_kunit.h"
#include "kernel/irq/internals.h"
#include "panel_modes.h"
#include "panel.h"
#include "panel_bl.h"
#include "panel_vrr.h"
#include "panel_drv.h"
#include "panel_debug.h"
#include "panel_drv_ioctl.h"
#include "panel_gpio.h"
#include "panel_regulator.h"
#include "panel_obj.h"

#include "dpui.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "mdnie.h"
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
#include "spi.h"
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "panel_poc.h"
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "./aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "./mafpc/mafpc_drv.h"
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
#include "panel_spi.h"
#endif

#if defined(CONFIG_TDMB_NOTIFIER)
#include <linux/tdmb_notifier.h>
#endif

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
#include <drivers/input/sec_input/sec_input.h>
#endif

#if defined(CONFIG_PANEL_FREQ_HOP)
#include "panel_freq_hop.h"
#endif

__visible_for_testing struct class *lcd_class;

static char *panel_state_names[] = {
	"OFF",		/* POWER OFF */
	"ON",		/* POWER ON */
	"NORMAL",	/* SLEEP OUT */
	"LPM",		/* LPM */
};

/* panel workqueue */
static char *panel_work_names[] = {
	[PANEL_WORK_DISP_DET] = "disp-det",
	[PANEL_WORK_PCD] = "pcd",
	[PANEL_WORK_ERR_FG] = "err-fg",
	[PANEL_WORK_CONN_DET] = "conn-det",
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[PANEL_WORK_DIM_FLASH] = "dim-flash",
#endif
	[PANEL_WORK_CHECK_CONDITION] = "panel-condition-check",
	[PANEL_WORK_UPDATE] = "panel-update",
#ifdef CONFIG_EVASION_DISP_DET
	[PANEL_WORK_EVASION_DISP_DET] = "evasion-disp-det",
#endif
};

static void disp_det_handler(struct work_struct *data);
static void conn_det_handler(struct work_struct *data);
static void err_fg_handler(struct work_struct *data);
static void panel_condition_handler(struct work_struct *work);
#ifdef CONFIG_SUPPORT_DIM_FLASH
static void dim_flash_handler(struct work_struct *work);
#endif
static void panel_update_handler(struct work_struct *work);
#ifdef CONFIG_EVASION_DISP_DET
static void evasion_disp_det_handler(struct work_struct *work);
#endif
static void pcd_handler(struct work_struct *data);

int panel_disp_det_state(struct panel_device *panel);
int panel_conn_det_state(struct panel_device *panel);
int panel_pcd_state(struct panel_device *panel);
int panel_drv_set_regulators(struct panel_device *panel);

static panel_wq_handler panel_wq_handlers[] = {
	[PANEL_WORK_DISP_DET] = disp_det_handler,
	[PANEL_WORK_PCD] = pcd_handler,
	[PANEL_WORK_ERR_FG] = err_fg_handler,
	[PANEL_WORK_CONN_DET] = conn_det_handler,
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[PANEL_WORK_DIM_FLASH] = dim_flash_handler,
#endif
	[PANEL_WORK_CHECK_CONDITION] = panel_condition_handler,
	[PANEL_WORK_UPDATE] = panel_update_handler,
#ifdef CONFIG_EVASION_DISP_DET
	[PANEL_WORK_EVASION_DISP_DET] = evasion_disp_det_handler,
#endif
};

static char *panel_thread_names[PANEL_THREAD_MAX] = {
#ifdef CONFIG_PANEL_VRR_BRIDGE
	[PANEL_THREAD_VRR_BRIDGE] = "panel-vrr-bridge",
#endif
};

static panel_thread_fn panel_thread_fns[PANEL_THREAD_MAX] = {
#ifdef CONFIG_PANEL_VRR_BRIDGE
	[PANEL_THREAD_VRR_BRIDGE] = panel_vrr_bridge_thread,
#endif
};

/* panel gpio */
static char *panel_gpio_names[PANEL_GPIO_MAX] = {
	[PANEL_GPIO_RESET] = PANEL_GPIO_NAME_RESET,
	[PANEL_GPIO_DISP_DET] = PANEL_GPIO_NAME_DISP_DET,
	[PANEL_GPIO_PCD] = PANEL_GPIO_NAME_PCD,
	[PANEL_GPIO_ERR_FG] = PANEL_GPIO_NAME_ERR_FG,
	[PANEL_GPIO_CONN_DET] = PANEL_GPIO_NAME_CONN_DET,
	[PANEL_GPIO_DISP_TE] = PANEL_GPIO_NAME_DISP_TE,
};

static int boot_panel_id;
#if defined(CONFIG_UML)
/* suppress normal log because of exception with too much log in UML */
int panel_log_level = 3;
#else
int panel_log_level = 6;
#endif
EXPORT_SYMBOL(panel_log_level);
module_param(panel_log_level, int, 0600);
int panel_cmd_log;
EXPORT_SYMBOL(panel_cmd_log);
#ifdef CONFIG_SUPPORT_PANEL_SWAP
int panel_reprobe(struct panel_device *panel);
#endif
inline char *get_panel_state_names(enum panel_active_state idx)
{
	if (idx < MAX_PANEL_STATE)
		return panel_state_names[idx];
	return NULL;
}

static struct panel_device *panel_get_panel_device(void)
{
	struct panel_device *panel;
	struct platform_device *pdev;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "samsung,panel-drv");
	if (!np) {
		panel_err("compatible(\"samsung,panel-drv\") node not found\n");
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		panel_err("mcd-panel device not found\n");
		return NULL;
	}

	panel = (struct panel_device *)platform_get_drvdata(pdev);
	if (!panel) {
		panel_err("failed to get panel_device\n");
		return NULL;
	}

	return panel;
}

int panel_find_max_brightness_from_cpi(struct common_panel_info *info)
{
	struct panel_dt_lut *lut_info = NULL;
	int max_brightness = 0;
	struct panel_device *panel;

	panel = panel_get_panel_device();

	if (!panel)
		return -EINVAL;

	if (!info)
		return -EINVAL;

	lut_info = find_panel_lut(panel, boot_panel_id);

	if (IS_ERR_OR_NULL(lut_info)) {
		panel_err("failed to find panel lookup table\n");
		return -EINVAL;
	}

	if (strncmp(lut_info->name, info->name, 128))
		return 0;

	if (!info->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP])
		return 0;

	max_brightness = max_brt_tbl(info->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP]->brt_tbl);
	if (max_brightness < 0) {
		panel_err("failed to get max brightness\n");
		return 0;
	}

	if (!panel->panel_bl.bd) {
		panel_err("backlight device is null\n");
		return 0;
	}

	panel->panel_bl.bd->props.max_brightness = max_brightness;

	panel_info("%s: max brightness=%d\n", lut_info->name,
			panel->panel_bl.bd->props.max_brightness);

	return 0;
}

__visible_for_testing int panel_snprintf_bypass(struct panel_device *panel, char *buf, size_t size)
{
	if (!panel || !buf || !size)
		return 0;

	return snprintf(buf, size, "bypass:%s",
			panel_bypass_is_on(panel) ? "on" : "off");
}

__visible_for_testing int panel_get_bypass_reason(struct panel_device *panel)
{
	if (!panel)
		return -EINVAL;

	/* bypass:off state */
	if (!panel_bypass_is_on(panel))
		return BYPASS_REASON_NONE;

	if (panel_conn_det_state(panel) == PANEL_STATE_NOK)
		return BYPASS_REASON_DISPLAY_CONNECTOR_IS_DISCONNECTED;

	if (panel_disp_det_state(panel) == PANEL_STATE_NOK)
		return BYPASS_REASON_AVDD_SWIRE_IS_LOW_AFTER_SLPOUT;

	if (panel_pcd_state(panel) == PANEL_STATE_NOK)
		return BYPASS_REASON_PCD_DETECTED;

	if (get_panel_id(panel) == 0)
		return BYPASS_REASON_PANEL_ID_READ_FAILURE;

	return BYPASS_REASON_NONE;
}

__visible_for_testing const char * const panel_get_bypass_reason_str(struct panel_device *panel)
{
	static const char * const str_bypass_reason[MAX_BYPASS_REASON] = {
		[BYPASS_REASON_NONE] = "none",
		/* the connection status is checked by 'con-det' pin */
		[BYPASS_REASON_DISPLAY_CONNECTOR_IS_DISCONNECTED] = "display connector is disconnected",
		/* AVDD_SWIRE(or EL_ON1) status is checked by 'disp-det' pin */
		[BYPASS_REASON_AVDD_SWIRE_IS_LOW_AFTER_SLPOUT] = "AVDD_SWIRE is off after SLPOUT",
		/* panel id read failure by control interface(MIPI, SPI, ...) */
		[BYPASS_REASON_PANEL_ID_READ_FAILURE] = "panel id read failure",
		[BYPASS_REASON_PCD_DETECTED] = "panel crack detected",
	};
	int reason;

	reason = panel_get_bypass_reason(panel);
	if (reason < 0)
		return NULL;

	return str_bypass_reason[reason];
}

__visible_for_testing int panel_snprintf_bypass_reason(struct panel_device *panel, char *buf, size_t size)
{
	struct panel_state *state;
	int len = 0;

	if (!panel || !buf || !size)
		return 0;

	state = &panel->state;

	len = panel_snprintf_bypass(panel, buf, size);
	len += snprintf(buf + len, size - len, "(reason:%s)", panel_get_bypass_reason_str(panel));

	return len;
}

__visible_for_testing void panel_print_bypass_reason(struct panel_device *panel)
{
	char buf[256] = { 0, };

	if (!panel)
		return;

	panel_snprintf_bypass_reason(panel, buf, sizeof(buf));

	panel_info("%s\n", buf);
}

#if 0
static int __init get_boot_panel_id(char *arg)
{
	get_option(&arg, &boot_panel_id);
	panel_info("boot_panel_id : 0x%x\n", boot_panel_id);

	return 0;
}
early_param("lcdtype", get_boot_panel_id);
#else
module_param(boot_panel_id, int, S_IRUGO);
#endif

/**
 * get_lcd info - get lcd global information.
 * @arg: key string of lcd information
 *
 * if get lcd info successfully, return 0 or positive value.
 * if not, return -EINVAL.
 */
int get_lcd_info(char *arg)
{
	if (!arg) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	if (!strncmp(arg, "connected", 9))
		return (boot_panel_id < 0) ? 0 : 1;
	else if (!strncmp(arg, "id", 2))
		return (boot_panel_id < 0) ? 0 : boot_panel_id;
	else if (!strncmp(arg, "window_color", 12))
		return (boot_panel_id < 0) ? 0 : ((boot_panel_id & 0x0F0000) >> 16);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(get_lcd_info);

int panel_initialize_regulator(struct panel_device *panel)
{
	struct panel_regulator *regulator;
	int ret, result = 0;

	if (!panel)
		return -EINVAL;

	list_for_each_entry(regulator, &panel->regulator_list, head) {
		ret = panel_regulator_helper_init(regulator);
		if (ret < 0) {
			panel_err("%s(%s) init failed\n", regulator->node_name, regulator->name);
			result = ret;
		}
	}
	return result;
}

struct panel_gpio *panel_gpio_list_find_by_name(struct panel_device *panel, const char *name)
{
	struct panel_gpio *gpio;

	if (!panel || !name)
		return NULL;

	list_for_each_entry(gpio, &panel->gpio_list, head) {
		if (!strcmp(gpio->name, name))
			return gpio;
	}

	return NULL;
}

__visible_for_testing struct panel_gpio *panel_get_gpio(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id)
{
	struct panel_gpio *gp;

	if (!panel)
		return NULL;

	if (panel_gpio_id >= PANEL_GPIO_MAX)
		return NULL;

	gp = panel_gpio_list_find_by_name(panel, panel_gpio_names[panel_gpio_id]);
	if (!gp)
		return NULL;

	if (!panel_gpio_helper_is_valid(gp))
		return NULL;

	return gp;
}

__visible_for_testing int panel_get_gpio_value(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	return panel_gpio_helper_get_value(panel_get_gpio(panel, panel_gpio_id));
}

__visible_for_testing int panel_set_gpio_value(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id, int value)
{
	return panel_gpio_helper_set_value(panel_get_gpio(panel, panel_gpio_id), value);
}

__visible_for_testing const char *panel_get_gpio_name(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	return panel_gpio_helper_get_name(panel_get_gpio(panel, panel_gpio_id));
}

__visible_for_testing bool panel_is_gpio_irq_valid(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id)
{
	return panel_gpio_helper_is_irq_valid(panel_get_gpio(panel, panel_gpio_id));
}

inline bool panel_is_gpio_valid(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id)
{
	return panel_get_gpio(panel, panel_gpio_id) ? true : false;
}

__visible_for_testing int panel_get_gpio_state(struct panel_device *panel, enum panel_gpio_lists panel_gpio_id)
{
	int state;

	state = panel_gpio_helper_get_state(panel_get_gpio(panel, panel_gpio_id));
	if (state < 0)
		return state;

	return (state == PANEL_GPIO_NORMAL_STATE) ? PANEL_STATE_OK : PANEL_STATE_NOK;
}

bool panel_is_gpio_irq_enabled(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	if (!panel_is_gpio_valid(panel, panel_gpio_id))
		return false;

	return panel_gpio_helper_is_irq_enabled(panel_get_gpio(panel, panel_gpio_id));
}

int panel_enable_gpio_irq(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	struct panel_gpio *gpio;
	int ret;

	if (!panel_is_gpio_valid(panel, panel_gpio_id))
		return -EINVAL;

	gpio = panel_get_gpio(panel, panel_gpio_id);
	if (!gpio)
		return -EINVAL;

	/* clear pending bit */
	ret = panel_gpio_helper_clear_irq_pending_bit(gpio);
	if (ret < 0)
		return -EINVAL;

	return panel_gpio_helper_enable_irq(gpio);
}

int panel_disable_gpio_irq(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	if (!panel_is_gpio_valid(panel, panel_gpio_id))
		return -EINVAL;

	return panel_gpio_helper_disable_irq(panel_get_gpio(panel, panel_gpio_id));
}

int panel_poll_gpio(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id, bool expect_level,
		unsigned long sleep_us, unsigned long timeout_us)
{
	ktime_t timeout = ktime_add_us(ktime_get(), timeout_us);
	int level;

	if (!panel_is_gpio_valid(panel, panel_gpio_id)) {
		panel_err("invalid gpio(%s)\n", panel_gpio_names[panel_gpio_id]);
		return -EINVAL;
	}

	for (;;) {
		level = panel_get_gpio_value(panel, panel_gpio_id);
		if (level == expect_level)
			break;
		if (ktime_after(ktime_get(), timeout))
			break;
		if (sleep_us)
			usleep_range((sleep_us >> 2) + 1, sleep_us);
	}
	return (level == expect_level) ? 0 : -ETIMEDOUT;
}

int panel_enable_disp_det_irq(struct panel_device *panel)
{
#ifndef CONFIG_EVASION_DISP_DET
	int ret;
#endif

	if (!panel)
		return -EINVAL;

#ifdef CONFIG_EVASION_DISP_DET
	cancel_delayed_work_sync(&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork);
	queue_delayed_work(panel->work[PANEL_WORK_EVASION_DISP_DET].wq,
		&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork, msecs_to_jiffies(EVASION_DISP_DET_DELAY_MSEC));
	panel_info("disp_det_irq queued\n");
#else
	ret = panel_enable_gpio_irq(panel, PANEL_GPIO_DISP_DET);
	if (ret < 0)
		panel_warn("do not support irq\n");
#endif

	return 0;
}

int panel_disable_disp_det_irq(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

#ifdef CONFIG_EVASION_DISP_DET
	cancel_delayed_work_sync(&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork);
	mutex_lock(&panel->work[PANEL_WORK_EVASION_DISP_DET].lock);
#endif

	ret = panel_disable_gpio_irq(panel, PANEL_GPIO_DISP_DET);
	if (ret < 0)
		panel_warn("do not support irq\n");

#ifdef CONFIG_EVASION_DISP_DET
	mutex_unlock(&panel->work[PANEL_WORK_EVASION_DISP_DET].lock);
#endif

	panel_info("disp_det_irq cleared\n");

	return 0;
}

int panel_enable_pcd_irq(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	ret = panel_enable_gpio_irq(panel, PANEL_GPIO_PCD);
	if (ret < 0)
		panel_warn("do not support irq\n");

	return 0;
}

int panel_disable_pcd_irq(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	ret = panel_disable_gpio_irq(panel, PANEL_GPIO_PCD);
	if (ret < 0)
		panel_warn("do not support irq\n");

	panel_info("pcd_irq cleared\n");

	return 0;
}

struct panel_power_ctrl *panel_power_control_find_sequence(struct panel_device *panel,
	const char *dev_name, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !dev_name || !name)
		return ERR_PTR(-EINVAL);

	panel_dbg("find: %s, %s\n", dev_name, name);
	list_for_each_entry(pctrl, &panel->power_ctrl_list, head) {
		if (!pctrl->dev_name) {
			panel_err("invalid pctrl->dev_name, skip to check\n");
			continue;
		}
		if (!pctrl->name) {
			panel_err("invalid pctrl->name, skip to check\n");
			continue;
		}
		if (!strcmp(pctrl->dev_name, dev_name) && !strcmp(pctrl->name, name))
			return pctrl;
	}
	return ERR_PTR(-ENODATA);
}

bool panel_power_control_exists(struct panel_device *panel, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !name) {
		panel_err("invalid arg");
		return false;
	}

	pctrl = panel_power_control_find_sequence(panel, panel->of_node_name, name);
	if (IS_ERR_OR_NULL(pctrl)) {
		if (PTR_ERR(pctrl) == -ENODEV)
			panel_dbg("not found %s\n", name);
		else
			panel_err("error occurred when find %s, %ld\n", name, PTR_ERR(pctrl));
		return false;
	}
	return true;
}


int panel_power_control_execute(struct panel_device *panel, const char *name)
{
	struct panel_power_ctrl *pctrl;

	if (!panel || !name) {
		panel_err("invalid arg");
		return -EINVAL;
	}

	pctrl = panel_power_control_find_sequence(panel, panel->of_node_name, name);
	if (IS_ERR_OR_NULL(pctrl)) {
		if (PTR_ERR(pctrl) == -ENODATA) {
			panel_dbg("%s not found\n", name);
			return -ENODATA;
		}
		panel_err("error occurred when find %s, %ld\n", name, PTR_ERR(pctrl));
		return PTR_ERR(pctrl);
	}
	return panel_power_ctrl_helper_execute(pctrl);
}

int panel_disp_det_state(struct panel_device *panel)
{
	int state;

	state = panel_get_gpio_state(panel, PANEL_GPIO_DISP_DET);
	if (state >= 0)
		panel_dbg("disp_det:%s\n", state ? "EL-OFF" : "EL-ON");

	return state;
}

#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
int panel_err_fg_state(struct panel_device *panel)
{
	int state;

	state = panel_get_gpio_state(panel, PANEL_GPIO_ERR_FG);
	if (state >= 0)
		panel_info("err_fg:%s\n", state ? "ERR_FG OK" : "ERR_FG NOK");

	return state;
}
#endif

int panel_pcd_state(struct panel_device *panel)
{
	int state;

	state = panel_get_gpio_state(panel, PANEL_GPIO_PCD);
	if (state >= 0)
		panel_dbg("pcd:%s\n", state ? "OK" : "NOK");

	return state;
}

int panel_conn_det_state(struct panel_device *panel)
{
	int state;

	state = panel_get_gpio_state(panel, PANEL_GPIO_CONN_DET);
	if (state >= 0)
		panel_dbg("conn_det:%s\n", state ? "connected" : "disconnected");

	return state;
}

bool panel_disconnected(struct panel_device *panel)
{
	int state;

	state = panel_conn_det_state(panel);
	if (state < 0)
		return false;

	return !state;
}

static void panel_set_cur_state(struct panel_device *panel, enum panel_active_state state)
{
	panel->state.cur_state = state;
}

#ifdef CONFIG_PANEL_NOTIFY
static inline void panel_send_ubconn_notify(u32 state)
{
	struct panel_ub_con_event_data data;

	data.state = state;
	panel_notifier_call_chain(PANEL_EVENT_UB_CON_CHANGED, &data);
	panel_info("call EVENT_UB_CON notifier %d\n", data.state);
}

void panel_send_screen_mode_notify(int display_idx, u32 mode)
{
	struct panel_screen_mode_data data;

	data.display_idx = display_idx;
	data.mode = mode;
	panel_notifier_call_chain(PANEL_EVENT_SCREEN_MODE_CHANGED, &data);
	panel_info("call EVENT_SCREEN_MODE_CHANGED notifier %d %d\n", data.display_idx, data.mode);
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
static int cmd_v4l2_mafpc_dev(struct panel_device *panel, int cmd, void *param)
{
	int ret = 0;

	if (panel->mafpc_sd) {
		ret = v4l2_subdev_call(panel->mafpc_sd, core, ioctl, cmd, param);
		if (ret)
			panel_err("failed to v4l2 subdev call\n");
	}

	return ret;
}

int panel_get_v4l2_abc_dev(struct panel_device *panel, struct mafpc_info *info)
{
	int ret;
	struct platform_device *pdev;
	struct device_node *np;
	struct mafpc_device *mafpc;

	if ((!panel) || (!info)) {
		panel_err("MCD:ABC: null\n");
		return -EINVAL;
	}

	np = of_find_compatible_node(NULL, NULL, "samsung,panel-mafpc");
	if (!np) {
		panel_err("compatible(\"samsung,panel-mafpc\") node not found\n");
		return -ENOENT;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		panel_err("mafpc device not found\n");
		return -ENODEV;
	}

	mafpc = (struct mafpc_device *)platform_get_drvdata(pdev);
	if (!mafpc) {
		panel_err("failed to get mafpc device\n");
		return -ENODEV;
	}
	panel->mafpc_sd = &mafpc->sd;
	mafpc->panel = panel;

	ret = cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_PROBE_ABC, (void *)info);
	if (unlikely(ret < 0)) {
		panel_err("failed to v4l2 mafpc probe\n");
		return -ENODEV;
	}

	return 0;
}
#endif

static void panel_init_clock_info(struct panel_device *panel)
{
	struct panel_properties *props;
	struct ddi_properties *ddi_props;

	if (!panel)
		return;

	props = &panel->panel_data.props;
	ddi_props = &panel->panel_data.ddi_props;

	props->dsi_freq = ddi_props->dft_dsi_freq;
	props->osc_freq = ddi_props->dft_osc_freq;
}

int __set_panel_power(struct panel_device *panel, int power)
{
	int ret = 0;

	if (panel->state.power == power) {
		panel_warn("same status.. skip..\n");
		return 0;
	}

	if (power == PANEL_POWER_ON) {
		ret = panel_power_control_execute(panel, "panel_power_on");
		if (ret < 0)
			panel_warn("failed to execute panel_power_on, ret:%d\n", ret);
	} else {
		ret = panel_power_control_execute(panel, "panel_power_off");
		if (ret < 0)
			panel_warn("failed to execute panel_power_off, ret:%d\n", ret);
	}
	panel_info("power(%s)\n", power == PANEL_POWER_ON ? "on" : "off");

	panel->state.power = power;

	return 0;
}

static int __panel_seq_display_on(struct panel_device *panel)
{
	int ret;

	panel_info("PANEL_DISPLAY_ON_SEQ\n");

	ret = panel_do_seqtbl_by_index(panel, PANEL_DISPLAY_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DISPLAY_ON_SEQ)\n");
		return ret;
	}

	return 0;
}

static int __panel_seq_display_off(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DISPLAY_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DISPLAY_OFF_SEQ)\n");
		return ret;
	}

	return 0;
}

static int __panel_seq_res_init(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_RES_INIT_SEQ)\n");
		return ret;
	}
#ifdef CONFIG_SUPPORT_GM2_FLASH
	ret = panel_do_seqtbl_by_index(panel, PANEL_GM2_FLASH_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_GM2_FLASH_RES_INIT_SEQ)\n");
		return ret;
	}
#endif

	return 0;
}

static int __panel_seq_boot(struct panel_device *panel)
{
	int ret;

	if (!check_seqtbl_exist(&panel->panel_data, PANEL_BOOT_SEQ))
		return 0;

	ret = panel_do_seqtbl_by_index(panel, PANEL_BOOT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_BOOT_SEQ)\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int __panel_seq_dim_flash_res_init(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DIM_FLASH_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DIM_FLASH_RES_INIT_SEQ)\n");
		return ret;
	}

	return 0;
}
#endif

static int __panel_seq_init(struct panel_device *panel)
{
	int ret = 0;
	int retry = 20;
	s64 time_diff;
	ktime_t timestamp = ktime_get();
	struct panel_bl_device *panel_bl = &panel->panel_bl;

#if !defined(CONFIG_UML)
	if (panel_disp_det_state(panel) == PANEL_STATE_OK) {
		panel_warn("panel already initialized\n");
		return 0;
	}
#endif

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	ret = panel_power_control_execute(panel, "panel_power_init");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_power_init\n");

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

#ifdef CONFIG_SUPPORT_PANEL_SWAP
	if (panel->panel_data.ddi_props.init_seq_by_lpdt)
		panel_dsi_set_lpdt(panel, true);

	ret = panel_reprobe(panel);
	if (ret < 0) {
		panel_err("failed to panel_reprobe\n");
		return ret;
	}

	if (panel->panel_data.ddi_props.init_seq_by_lpdt)
		panel_dsi_set_lpdt(panel, false);
#endif

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	if (panel->panel_data.ddi_props.init_seq_by_lpdt)
		panel_dsi_set_lpdt(panel, true);

	ret = panel_ddi_init(panel);
	if (ret < 0 && ret != -ENOENT) {
		panel_err("failed to panel_ddi_init\n");
	}

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_INIT_SEQ);

	if (panel->panel_data.ddi_props.init_seq_by_lpdt)
		panel_dsi_set_lpdt(panel, false);

	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
		goto err_init_seq;
	}

#ifdef CONFIG_SUPPORT_MAFPC
	cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_MAFPC_PANEL_INIT, NULL);
#endif

	time_diff = ktime_to_us(ktime_sub(ktime_get(), timestamp));
	panel_info("Time for Panel Init : %llu\n", time_diff);

	mutex_unlock(&panel->op_lock);

check_disp_det:
	if (panel_disp_det_state(panel) == PANEL_STATE_NOK) {
		usleep_range(1000, 1000 + 10);
		if (--retry >= 0)
			goto check_disp_det;
		panel_err("check disp det .. not ok\n");
		mutex_unlock(&panel_bl->lock);
		return -EAGAIN;
	}

	retry = 20;
check_pcd:
	if (panel_pcd_state(panel) == PANEL_STATE_NOK) {
		usleep_range(1000, 1000 + 10);
		if (--retry >= 0)
			goto check_pcd;

		panel_dsi_set_bypass(panel, true); /* ignore TE checking & frame update request*/
		panel_err("check pcd .. not ok\n");
		mutex_unlock(&panel_bl->lock);
		return -EAGAIN;
	}

	time_diff = ktime_to_us(ktime_sub(ktime_get(), timestamp));
	panel_info("check disp det .. success %llu\n", time_diff);

	ret = panel_power_control_execute(panel, "panel_fd_enable");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_fd_enable\n");

	panel_enable_disp_det_irq(panel);
	panel_enable_pcd_irq(panel);

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_init_panel(panel, INIT_WITH_LOCK);
	if (ret)
		panel_err("failed to aod init_panel\n");
#endif
	panel_bl_set_saved_flag(panel_bl, false);
	mutex_unlock(&panel_bl->lock);

	return 0;

err_init_seq:
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);
	return -EAGAIN;
}

static int __panel_seq_exit(struct panel_device *panel)
{
	int ret;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	ret = panel_disable_disp_det_irq(panel);
	if (ret < 0)
		panel_err("failed to disable disp_det irq\n");

	ret = panel_disable_pcd_irq(panel);
	if (ret < 0)
		panel_err("failed to disable pcd irq\n");

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_EXIT_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write exit seqtbl\n");

#ifdef CONFIG_SUPPORT_MAFPC
	cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_MAFPC_PANEL_EXIT, NULL);
#endif

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

	return ret;
}

#ifdef CONFIG_SUPPORT_HMD
static int __panel_seq_hmd_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;

	if (!panel) {
		panel_err("pane is null\n");
		return 0;
	}
	state = &panel->state;

	panel_info("hmd was on, setting hmd on seq\n");
	ret = panel_do_seqtbl_by_index(panel, PANEL_HMD_ON_SEQ);
	if (ret)
		panel_err("failed to set hmd on seq\n");

	return ret;
}

static int panel_set_hmd_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;

	if (!panel) {
		panel_err("pane is null\n");
		return 0;
	}
	state = &panel->state;

	if (state->hmd_on != PANEL_HMD_ON)
		return ret;

	panel_info("hmd was on, setting hmd on seq\n");
	ret = __panel_seq_hmd_on(panel);
	if (ret)
		panel_err("failed to set hmd on seq\n");

	ret = panel_bl_set_brightness(&panel->panel_bl,
			PANEL_BL_SUBDEV_TYPE_HMD, SEND_CMD);
	if (ret)
		panel_err("fail to set hmd brightness\n");

	return ret;
}


#endif

#ifdef CONFIG_MCD_PANEL_LPM
static int __panel_seq_exit_alpm(struct panel_device *panel)
{
	int ret = 0;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel_info("was called\n");

	ret = panel_power_control_execute(panel, "panel_power_exit_alpm_pre");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_power_exit_alpm_pre\n");

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_exit_from_lpm(panel);
	if (ret)
		panel_err("failed to exit_lpm ops\n");
#endif

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	ret = panel_disable_disp_det_irq(panel);
	if (ret < 0)
		panel_err("failed to disable disp_det irq\n");

	ret = panel_power_control_execute(panel, "panel_power_exit_alpm");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_power_exit_alpm\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_EXIT_SEQ);
	if (ret)
		panel_err("failed to alpm-exit\n");

	panel->panel_data.props.lpm_brightness = -1;
	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	if (panel->panel_data.props.panel_partial_disp != -1)
		panel->panel_data.props.panel_partial_disp = 0;

	/* PANEL_ALPM_EXIT_AFTER_SEQ temporary added */
	if (check_seqtbl_exist(&panel->panel_data, PANEL_ALPM_EXIT_AFTER_SEQ)) {
		panel_bl_set_brightness(&panel->panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, SKIP_CMD);
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_EXIT_AFTER_SEQ);
		if (ret)
			panel_err("failed to alpm-exit-after seq\n");
	}
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

	/* if PANEL_ALPM_EXIT_AFTER_SEQ is not exists, update brightness by panel_drv. */
	if (!check_seqtbl_exist(&panel->panel_data, PANEL_ALPM_EXIT_AFTER_SEQ))
		panel_update_brightness(panel);

	panel_enable_disp_det_irq(panel);

	return ret;
}
#ifdef CONFIG_MCD_PANEL_FACTORY
inline int panel_seq_exit_alpm(struct panel_device *panel)
{
	return __panel_seq_exit_alpm(panel);
}
#endif
/* delay to prevent current leackage when alpm */
/* according to ha6 opmanual, the dealy value is 126msec */
static void __delay_normal_alpm(struct panel_device *panel)
{
	u32 gap;
	u32 delay = 0;
	struct seqinfo *seqtbl;
	struct delayinfo *delaycmd;

	if (!check_seqtbl_exist(&panel->panel_data, PANEL_ALPM_DELAY_SEQ))
		goto exit_delay;

	seqtbl = find_index_seqtbl(&panel->panel_data, PANEL_ALPM_DELAY_SEQ);
	if (unlikely(!seqtbl))
		goto exit_delay;

	delaycmd = (struct delayinfo *)seqtbl->cmdtbl[0];
	if (delaycmd->type != CMD_TYPE_DELAY) {
		panel_err("can't find value\n");
		goto exit_delay;
	}

	if (ktime_after(ktime_get(), panel->ktime_panel_on)) {
		gap = ktime_to_us(ktime_sub(ktime_get(), panel->ktime_panel_on));
		if (gap > delaycmd->usec)
			goto exit_delay;

		delay = delaycmd->usec - gap;
		usleep_range(delay, delay + 10);
	}
	panel_info("total elapsed time : %d\n",
		(int)ktime_to_us(ktime_sub(ktime_get(), panel->ktime_panel_on)));
exit_delay:
	return;
}

static int __panel_seq_set_alpm(struct panel_device *panel)
{
	int ret;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel_info("%s was called\n", __func__);
	__delay_normal_alpm(panel);

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	ret = panel_disable_disp_det_irq(panel);
	if (ret < 0)
		panel_err("failed to disable disp_det irq\n");

	if (check_seqtbl_exist(&panel->panel_data, PANEL_ALPM_INIT_SEQ)) {
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_INIT_SEQ);
		if (ret)
			panel_err("failed to alpm-init\n");
	}

	ret = panel_power_control_execute(panel, "panel_power_enter_alpm");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_power_enter_alpm\n");

#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD);
#endif
	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_SET_BL_SEQ);
	if (ret)
		panel_err("failed to alpm-set-bl\n");

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_enter_to_lpm(panel);
	if (ret) {
		panel_err("failed to enter to lpm\n");
		return ret;
	}
#endif

	panel_enable_disp_det_irq(panel);

	return 0;
}
#ifdef CONFIG_MCD_PANEL_FACTORY
inline int panel_seq_set_alpm(struct panel_device *panel)
{
	return __panel_seq_set_alpm(panel);
}
#endif
#endif

static int __panel_seq_dump(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DUMP_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write dump seqtbl\n");

	return ret;
}

static int panel_debug_dump(struct panel_device *panel)
{
	int ret;

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		goto dump_exit;
	}

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_info("Current state:%d\n", panel->state.cur_state);
		goto dump_exit;
	}

	ret = __panel_seq_dump(panel);
	if (ret) {
		panel_err("failed to dump\n");
		return ret;
	}

	panel_info("disp_det_state:%s\n",
			panel_disp_det_state(panel) == PANEL_STATE_OK ? "OK" : "NOK");
dump_exit:
	return 0;
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
int panel_seq_cmdlog_dump(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_CMDLOG_DUMP_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write cmdlog dump seqtbl\n");

	return ret;
}
#endif

int panel_display_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	struct panel_bl_device *panel_bl;

	if (!panel)
		return -EINVAL;

	state = &panel->state;
	panel_bl = &panel->panel_bl;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		goto do_exit;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON) {
		panel_warn("invalid state\n");
		goto do_exit;
	}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	mdnie_enable(&panel->mdnie);
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	// Transmit Black Frame
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		ret = panel_aod_black_grid_on(panel);
		if (ret)
			panel_info("PANEL_ERR:failed to black grid on\n");
	}
#endif

	ret = __panel_seq_display_on(panel);
	if (ret) {
		panel_err("failed to display on\n");
		return ret;
	}
	state->disp_on = PANEL_DISPLAY_ON;

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		usleep_range(33400, 33500);
		ret = panel_aod_black_grid_off(panel);
		if (ret)
			panel_info("PANEL_ERR:failed to black grid on\n");
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	copr_enable(&panel->copr);
#endif

	/*
	 * update brightness if saved brightness exists.
	 * W/A for 'set_brightness' is called when init-seq is running but
	 * panel state is 'PANEL_STATE_ON'.
	 */
	if (panel_bl_get_saved_flag(panel_bl))
		panel_update_brightness(panel);

	return 0;

do_exit:
	return ret;
}

__visible_for_testing int panel_display_off(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;

	if (!panel)
		return -EINVAL;

	state = &panel->state;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		goto do_exit;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON) {
		panel_warn("invalid state\n");
		goto do_exit;
	}

	ret = __panel_seq_display_off(panel);
	if (unlikely(ret < 0))
		panel_err("failed to write init seqtbl\n");
	state->disp_on = PANEL_DISPLAY_OFF;

	return 0;
do_exit:
	return ret;
}

static struct common_panel_info *panel_detect(struct panel_device *panel)
{
	u8 id[3];
	u32 panel_id;
	int ret = 0;
	struct common_panel_info *info;
	struct panel_info *panel_data;
	bool detect = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return NULL;
	}
	panel_data = &panel->panel_data;

	memset(id, 0, sizeof(id));
#if !defined(CONFIG_SUPPORT_PANEL_SWAP) || IS_ENABLED(CONFIG_UNSUPPORT_INIT_READ)
	panel_info("use BL panel id : 0x%06x\n", boot_panel_id);
	id[0] = (boot_panel_id >> 16) & 0xFF;
	id[1] = (boot_panel_id >> 8) & 0xFF;
	id[2] = boot_panel_id & 0xFF;
	ret = 0;
	detect = true;
#else
#if IS_ENABLED(CONFIG_PANEL_ID_READ_BY_LPDT)
	panel_dsi_set_lpdt(panel, true);
#endif
	ret = read_panel_id(panel, id);
	if (unlikely(ret < 0)) {
		panel_err("failed to read id(ret %d)\n", ret);
		detect = false;
	}
#if IS_ENABLED(CONFIG_PANEL_ID_READ_BY_LPDT)
	panel_dsi_set_lpdt(panel, false);
#endif
#endif
	panel_id = (id[0] << 16) | (id[1] << 8) | id[2];
	memcpy(panel_data->id, id, sizeof(id));
	panel_info("panel id : 0x%06x\n", panel_id);

#ifdef CONFIG_SUPPORT_PANEL_SWAP
	if ((boot_panel_id >= 0) && (detect == true)) {
		boot_panel_id = (id[0] << 16) | (id[1] << 8) | id[2];
		panel_info("boot_panel_id : 0x%06x\n", boot_panel_id);
	}
#endif

	info = find_panel(panel, panel_id);
	if (unlikely(!info)) {
		panel_err("panel not found (id 0x%06X)\n", panel_id);
		return NULL;
	}

	return info;
}

static int panel_alloc_command_buffer(struct panel_device *panel)
{
	int fifo_size;

	fifo_size = get_panel_adapter_fifo_size(panel);
	if (fifo_size <= 0) {
		panel_err("invalid fifo_size(%d)\n", fifo_size);
		return -EINVAL;
	}

	if (fifo_size > SZ_8K) {
		panel_warn("fifo_size(%d) is too big\n", fifo_size);
		fifo_size = SZ_8K;
	}
	panel_info("fifo_size %d\n", fifo_size);

	panel->cmdbuf = kmalloc(fifo_size, GFP_KERNEL);
	if (!panel->cmdbuf)
		return -ENOMEM;

	return 0;
}

static int panel_prepare(struct panel_device *panel, struct common_panel_info *info)
{
	int i;
	struct panel_info *panel_data;

	if (!panel || !info)
		return -EINVAL;

	panel_data = &panel->panel_data;
	mutex_lock(&panel->op_lock);

	if (!panel->cmdbuf) {
		int ret;

		ret = panel_alloc_command_buffer(panel);
		if (ret < 0) {
			panel_err("failed to alloc command buffer\n");
			mutex_unlock(&panel->op_lock);
			return ret;
		}
	}

	panel_data->props.panel_partial_disp =
		(info->ddi_props.support_partial_disp) ? 0 : -1;

	/* maptbl */
	panel_data->maptbl = info->maptbl;
	panel_data->nr_maptbl = info->nr_maptbl;
	for (i = 0; i < panel_data->nr_maptbl; i++)
		panel_data->maptbl[i].pdata = panel;

	/* sequence table */
	panel_data->seqtbl = info->seqtbl;
	panel_data->nr_seqtbl = info->nr_seqtbl;

	/* read information table */
	panel_data->rditbl = info->rditbl;
	panel_data->nr_rditbl = info->nr_rditbl;

	/* resource table */
	panel_data->restbl = info->restbl;
	panel_data->nr_restbl = info->nr_restbl;
	for (i = 0; i < panel_data->nr_restbl; i++)
		panel_data->restbl[i].state = RES_UNINITIALIZED;

	/* dump information */
	panel_data->dumpinfo = info->dumpinfo;
	panel_data->nr_dumpinfo = info->nr_dumpinfo;

	/* dimming information */
	for (i = 0; i < MAX_PANEL_BL_SUBDEV; i++)
		panel_data->panel_dim_info[i] = info->panel_dim_info[i];

	/* ddi properties */
	memcpy(&panel_data->ddi_props, &info->ddi_props, sizeof(panel_data->ddi_props));

	/* ddi operations */
	memcpy(&panel_data->ddi_ops, &info->ddi_ops, sizeof(panel_data->ddi_ops));

	/* multi-resolution */
	memcpy(&panel_data->mres, &info->mres, sizeof(panel_data->mres));

	/* variable-refresh-rate */
	panel_data->vrrtbl = info->vrrtbl;
	panel_data->nr_vrrtbl = info->nr_vrrtbl;

	/* display-mode */
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	panel_data->common_panel_modes = info->common_panel_modes;
#endif
	/* backlight IC table */
#ifdef CONFIG_MCD_PANEL_BLIC
	panel_data->blic_data_tbl = info->blic_data_tbl;
	panel_data->nr_blic_data_tbl = info->nr_blic_data_tbl;
#endif
#ifdef CONFIG_MCD_PANEL_RCD
	panel_data->rcd_data = info->rcd_data;
#endif
	/* panel vendor name */
	memcpy(panel_data->vendor, info->vendor,
			sizeof(panel_data->vendor));

	mutex_unlock(&panel->op_lock);

	return 0;
}

static int panel_resource_init(struct panel_device *panel)
{
	if (!panel)
		return -EINVAL;

	__panel_seq_res_init(panel);

	return 0;
}

static int panel_boot_on(struct panel_device *panel)
{
	if (!panel)
		return -EINVAL;

	__panel_seq_boot(panel);

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int panel_dim_flash_resource_init(struct panel_device *panel)
{
	return __panel_seq_dim_flash_res_init(panel);
}
#endif

static int panel_maptbl_init(struct panel_device *panel)
{
	int i, ret;
	struct panel_info *panel_data = &panel->panel_data;

	mutex_lock(&panel->op_lock);
	for (i = 0; i < panel_data->nr_maptbl; i++) {
		ret = maptbl_init(&panel_data->maptbl[i]);
		if (ret == -ENODATA)
			panel_dbg("empty maptbl(idx:%d)\n", i);
		else if (ret < 0)
			panel_err("maptbl[%d] init failed\n", i);
	}
	mutex_unlock(&panel->op_lock);

	return 0;
}

/*
 * panel_mtp_gamma_check - call gamma_check function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_mtp_gamma_check(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->mtp_gamma_check) {
		panel_warn("not supported");
		return -1;
	}
	return ops->mtp_gamma_check(panel, NULL, 0);
}

/*
 * panel_flash_checksum_calc - call gamma_flash_checksum function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_flash_checksum_calc(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->gamma_flash_checksum) {
		panel_warn("not supported");
		return 0;
	}
	return ops->gamma_flash_checksum(panel, &panel->flash_checksum_result, sizeof(panel->flash_checksum_result));
}

#ifdef CONFIG_SUPPORT_SSR_TEST
/*
 * panel_ssr_test - call ssr test function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_ssr_test(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->ssr_test) {
		panel_warn("not supported");
		return -ENOENT;
	}
	return ops->ssr_test(panel, NULL, 0);
}
#endif

#ifdef CONFIG_SUPPORT_ECC_TEST
/*
 * panel_ecc_test - call ssr test function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_ecc_test(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->ecc_test) {
		panel_warn("not supported");
		return -ENOENT;
	}
	return ops->ecc_test(panel, NULL, 0);
}
#endif

/*
 * panel_decoder_test - call decoder test function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_decoder_test(struct panel_device *panel, u8 *buf, int len)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->decoder_test) {
		panel_warn("not supported");
		return -ENOENT;
	}
	return ops->decoder_test(panel, buf, len);
}

bool check_panel_decoder_test_exists(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->decoder_test) {
		panel_warn("not supported");
		return false;
	}
	return true;
}

#ifdef CONFIG_SUPPORT_PANEL_VCOM_TRIM_TEST
/*
 * panel_vcom_trim_test - call vcom_trim test function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_vcom_trim_test(struct panel_device *panel, u8 *buf, int len)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->vcom_trim_test)
		return -ENOENT;

	return ops->vcom_trim_test(panel, buf, len);
}
#endif

int panel_ddi_init(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->ddi_init) {
		panel_dbg("not supported");
		return -ENOENT;
	}
	return ops->ddi_init(panel, NULL, 0);
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
int panel_update_dim_type(struct panel_device *panel, u32 dim_type)
{
	struct dim_flash_result *result;
	u8 mtp_reg[64];
	int sz_mtp_reg = 0;
	int state, state_all = 0;
	int index, result_idx = 0;
	int ret;

	if (dim_type == DIM_TYPE_DIM_FLASH) {
		if (!panel->dim_flash_result) {
			panel_err("dim buffer not found\n");
			return -ENOMEM;
		}

		memset(panel->dim_flash_result, 0,
			sizeof(struct dim_flash_result) * panel->max_nr_dim_flash_result);

		for (index = 0; index < panel->max_nr_dim_flash_result; index++) {
			if (get_poc_partition_size(&panel->poc_dev,
						POC_DIM_PARTITION + index) == 0) {
				continue;
			}
			result = &panel->dim_flash_result[result_idx++];
			state = 0;
			do {
				/* DIM */
				ret = set_panel_poc(&panel->poc_dev, POC_OP_DIM_READ, &index);
				if (unlikely(ret < 0)) {
					panel_err("failed to read gamma flash(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				ret = check_poc_partition_exists(&panel->poc_dev,
						POC_DIM_PARTITION + index);
				if (unlikely(ret < 0)) {
					panel_err("failed to check dim_flash exist\n");
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				if (ret == PARTITION_WRITE_CHECK_NOK) {
					panel_err("dim partition not exist(%d)\n", ret);
					state = GAMMA_FLASH_ERROR_NOT_EXIST;
					break;
				}
#endif
				ret = get_poc_partition_chksum(&panel->poc_dev,
						POC_DIM_PARTITION + index,
						&result->dim_chksum_ok,
						&result->dim_chksum_by_calc,
						&result->dim_chksum_by_read);
#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (unlikely(ret < 0)) {
					panel_err("failed to get chksum(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}
				if (result->dim_chksum_by_calc !=
					result->dim_chksum_by_read) {
					panel_err("dim flash checksum(%04X,%04X) mismatch\n",
							result->dim_chksum_by_calc,
							result->dim_chksum_by_read);
					state = GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;
					break;
				}
#endif
				/* MTP */
				ret = set_panel_poc(&panel->poc_dev, POC_OP_MTP_READ, &index);
				if (unlikely(ret)) {
					panel_err("failed to read mtp flash(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				ret = get_poc_partition_chksum(&panel->poc_dev,
						POC_MTP_PARTITION + index,
						&result->mtp_chksum_ok,
						&result->mtp_chksum_by_calc,
						&result->mtp_chksum_by_read);
#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (unlikely(ret < 0)) {
					panel_err("failed to get chksum(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				if (result->mtp_chksum_by_calc != result->mtp_chksum_by_read) {
					panel_err("mtp flash checksum(%04X,%04X) mismatch\n",
							result->mtp_chksum_by_calc, result->mtp_chksum_by_read);
					state = GAMMA_FLASH_ERROR_MTP_OFFSET;
					break;
				}
#endif

				ret = get_resource_size_by_name(&panel->panel_data, "mtp");
				if (unlikely(ret < 0)) {
					panel_err("failed to get resource mtp size (ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}
				sz_mtp_reg = ret;

				ret = resource_copy_by_name(&panel->panel_data, mtp_reg, "mtp");
				if (unlikely(ret < 0)) {
					panel_err("failed to copy resource mtp (ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (cmp_poc_partition_data(&panel->poc_dev,
					POC_MTP_PARTITION + index, 0, mtp_reg, sz_mtp_reg)) {
					panel_err("mismatch mtp(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_MTP_OFFSET;
					break;
				}
#endif
				result->mtp_chksum_by_reg = calc_checksum_16bit(mtp_reg, sz_mtp_reg);
			} while (0);

			if (state_all == 0)
				state_all = state;

			if (state == 0)
				result->state = GAMMA_FLASH_SUCCESS;
			else
				result->state = state;

		}
		panel->nr_dim_flash_result = result_idx;

		if (state_all != 0)
			return state_all;
		/* update dimming flash, mtp, hbm_gamma resources */
		ret = panel_dim_flash_resource_init(panel);
		if (unlikely(ret)) {
			panel_err("failed to resource init\n");
			state_all = GAMMA_FLASH_ERROR_READ_FAIL;
		}
	}

	mutex_lock(&panel->op_lock);
	panel->panel_data.props.cur_dim_type = dim_type;
	mutex_unlock(&panel->op_lock);

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		state_all = -ENODEV;
	}

	return state_all;
}
#endif

int panel_reprobe(struct panel_device *panel)
{
	struct common_panel_info *info;
	int ret;

	info = panel_detect(panel);
	if (unlikely(!info)) {
		panel_err("panel detection failed\n");
		return -ENODEV;
	}

	ret = panel_prepare(panel, info);
	if (unlikely(ret)) {
		panel_err("failed to panel_prepare\n");
		return ret;
	}

	ret = panel_resource_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return ret;
	}

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_probe(panel, info->poc_data);
	if (unlikely(ret)) {
		panel_err("failed to probe poc driver\n");
		return -ENODEV;
	}
#endif /* CONFIG_SUPPORT_DDI_FLASH */

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to maptbl init\n");
		return -ENODEV;
	}

	ret = panel_bl_probe(&panel->panel_bl);
	if (unlikely(ret)) {
		panel_err("failed to probe backlight driver\n");
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static void dim_flash_handler(struct work_struct *work)
{
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_DIM_FLASH]);
	int ret;

	mutex_lock(&panel->panel_bl.lock);
	if (atomic_read(&w->running) >= 2) {
		panel_info("already running\n");
		mutex_unlock(&panel->panel_bl.lock);
		return;
	}
	atomic_set(&w->running, 2);
	panel_info("+\n");
	ret = panel_update_dim_type(panel, DIM_TYPE_DIM_FLASH);
	if (ret < 0) {
		panel_err("failed to update dim_flash %d\n", ret);
		w->ret = ret;
	} else {
		panel_info("update dim_flash done %d\n", ret);
		w->ret = ret;
	}
	panel_info("-\n");
	atomic_set(&w->running, 0);
	mutex_unlock(&panel->panel_bl.lock);
	panel_update_brightness(panel);
}
#endif

void clear_check_wq_var(struct panel_condition_check *pcc)
{
	pcc->check_state = NO_CHECK_STATE;
	pcc->is_panel_check = false;
	pcc->frame_cnt = 0;
}

bool show_copr_value(struct panel_device *panel, int frame_cnt)
{
	bool retVal = false;
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	int ret = 0;
	struct copr_info *copr = &panel->copr;
	char write_buf[200] = {0, };
	int c = 0, i = 0, len = 0;

	if (copr_is_enabled(copr)) {
		ret = copr_get_value(copr);
		if (ret < 0) {
			panel_err("failed to get copr\n");
			return retVal;
		}
		len += snprintf(write_buf + len, sizeof(write_buf) - len, "cur:%d avg:%d ",
			copr->props.cur_copr, copr->props.avg_copr);
		if (copr->props.nr_roi > 0) {
			len += snprintf(write_buf + len, sizeof(write_buf) - len, "roi:");
			for (i = 0; i < copr->props.nr_roi; i++) {
				for (c = 0; c < 3; c++) {
					if (sizeof(write_buf) - len > 0) {
						len += snprintf(write_buf + len, sizeof(write_buf) - len, "%d%s",
							copr->props.copr_roi_r[i][c],
							((i == copr->props.nr_roi - 1) && c == 2) ? "\n" : " ");
					}
				}
			}
		} else {
			len += snprintf(write_buf + len, sizeof(write_buf) - len, "%s", "\n");
		}
		panel_info("copr(frame_cnt=%d) -> %s", frame_cnt, write_buf);
		if (copr->props.cur_copr > 0) /* not black */
			retVal = true;
	} else {
		panel_info("copr do not support\n");
	}
#else
	panel_info("copr feature is disabled\n");
#endif
	return retVal;
}

static void panel_condition_handler(struct work_struct *work)
{
	int ret = 0;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_CHECK_CONDITION]);
	struct panel_condition_check *condition = &panel->condition_check;

	if (atomic_read(&w->running)) {
		panel_info("already running\n");
		return;
	}
	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	atomic_set(&w->running, 1);
	mutex_lock(&w->lock);
	panel_info("%s\n", condition->str_state[condition->check_state]);

	switch (condition->check_state) {
	case PRINT_NORMAL_PANEL_INFO:
		// print rddpm
		ret = panel_do_seqtbl_by_index(panel, PANEL_CHECK_CONDITION_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write panel check\n");
		if (show_copr_value(panel, condition->frame_cnt))
			clear_check_wq_var(condition);
		else
			condition->check_state = CHECK_NORMAL_PANEL_INFO;
		break;
	case PRINT_DOZE_PANEL_INFO:
		ret = panel_do_seqtbl_by_index(panel, PANEL_CHECK_CONDITION_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write panel check\n");
		clear_check_wq_var(condition);
		break;
	case CHECK_NORMAL_PANEL_INFO:
		if (show_copr_value(panel, condition->frame_cnt))
			clear_check_wq_var(condition);
		break;
	default:
		panel_info("state %d\n", condition->check_state);
		clear_check_wq_var(condition);
		break;
	}
	mutex_unlock(&w->lock);
	atomic_set(&w->running, 0);
	panel_wake_unlock(panel);
}

int init_check_wq(struct panel_condition_check *condition)
{
	clear_check_wq_var(condition);
	strcpy(condition->str_state[NO_CHECK_STATE], STR_NO_CHECK);
	strcpy(condition->str_state[PRINT_NORMAL_PANEL_INFO], STR_NOMARL_ON);
	strcpy(condition->str_state[CHECK_NORMAL_PANEL_INFO], STR_NOMARL_100FRAME);
	strcpy(condition->str_state[PRINT_DOZE_PANEL_INFO], STR_AOD_ON);

	return 0;
}

void panel_check_ready(struct panel_device *panel)
{
	struct panel_condition_check *pcc = &panel->condition_check;
	struct panel_work *pw = &panel->work[PANEL_WORK_CHECK_CONDITION];

	mutex_lock(&pw->lock);
	pcc->is_panel_check = true;
	if (panel->state.cur_state == PANEL_STATE_NORMAL)
		pcc->check_state = PRINT_NORMAL_PANEL_INFO;
	if (panel->state.cur_state == PANEL_STATE_ALPM)
		pcc->check_state = PRINT_DOZE_PANEL_INFO;
	mutex_unlock(&pw->lock);
}

static void panel_check_start(struct panel_device *panel)
{
	struct panel_condition_check *pcc = &panel->condition_check;
	struct panel_work *pw = &panel->work[PANEL_WORK_CHECK_CONDITION];

	mutex_lock(&pw->lock);
	if (pcc->frame_cnt < 100) {
		pcc->frame_cnt++;
		switch (pcc->check_state) {
		case PRINT_NORMAL_PANEL_INFO:
		case PRINT_DOZE_PANEL_INFO:
			if (pcc->frame_cnt == 1)
				queue_delayed_work(pw->wq, &pw->dwork, msecs_to_jiffies(0));
			break;
		case CHECK_NORMAL_PANEL_INFO:
			if (pcc->frame_cnt % 10 == 0)
				queue_delayed_work(pw->wq, &pw->dwork, msecs_to_jiffies(0));
			break;
		case NO_CHECK_STATE:
			// do nothing
			break;
		default:
			panel_warn("state is invalid %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
			clear_check_wq_var(pcc);
			break;
		}
	} else {
		if (pcc->check_state == CHECK_NORMAL_PANEL_INFO)
			panel_warn("screen is black in first 100 frame %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
		else
			panel_warn("state is invalid %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
		clear_check_wq_var(pcc);
	}
	mutex_unlock(&pw->lock);
}

static void panel_update_handler(struct work_struct *work)
{
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_UPDATE]);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct panel_properties *props = &panel->panel_data.props;
	bool vrr_updated = false;
	int ret = 0;

	mutex_lock(&w->lock);
	mutex_lock(&panel_bl->lock);
	ret = update_vrr_lfd(&props->vrr_lfd_info);
	if (panel_bl->bd && ret == VRR_LFD_UPDATED) {
		props->vrr_updated = true;
		vrr_updated = true;
	}
	mutex_unlock(&panel_bl->lock);

	if (vrr_updated) {
		ret = panel_update_brightness(panel);
		if (ret < 0)
			panel_err("failed to update vrr & brightness\n");
	}
	mutex_unlock(&w->lock);
}

#ifdef CONFIG_EVASION_DISP_DET
static void evasion_disp_det_handler(struct work_struct *work)
{
	int ret;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_EVASION_DISP_DET]);

	if (panel->state.cur_state == PANEL_STATE_OFF) {
		panel_warn("panel is off state\n");
		return;
	}

	mutex_lock(&w->lock);
	ret = panel_enable_gpio_irq(panel, PANEL_GPIO_DISP_DET);
	if (ret < 0)
		panel_warn("do not support irq\n");
	mutex_unlock(&w->lock);
	panel_info("disp_det_irq enabled\n");

}
#endif

static int panel_init_property(struct panel_device *panel)
{
	struct panel_info *panel_data;
	int i;

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;

	mutex_lock(&panel->op_lock);
	panel_data->props.temperature = NORMAL_TEMPERATURE;
	panel_data->props.alpm_mode = ALPM_OFF;
	panel_data->props.cur_alpm_mode = ALPM_OFF;
	panel_data->props.lpm_opr = 250;		/* default LPM OPR 2.5 */
	panel_data->props.cur_lpm_opr = 250;	/* default LPM OPR 2.5 */
	panel_data->props.panel_partial_disp = 0;
	panel_data->props.dia_mode = 1;
	panel_data->props.irc_mode = 0;

	memset(panel_data->props.mcd_rs_range, -1,
			sizeof(panel_data->props.mcd_rs_range));

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	panel_data->props.gct_on = GRAM_TEST_OFF;
	panel_data->props.gct_vddm = VDDM_ORIG;
	panel_data->props.gct_pattern = GCT_PATTERN_NONE;
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	panel_data->props.dynamic_hlpm = 0;
#endif
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	panel_data->props.tdmb_on = false;
	panel_data->props.cur_tdmb_on = false;
#endif
#ifdef CONFIG_SUPPORT_DIM_FLASH
	panel_data->props.cur_dim_type = DIM_TYPE_AID_DIMMING;
#endif
	for (i = 0; i < MAX_CMD_LEVEL; i++)
		panel_data->props.key[i] = 0;
	mutex_unlock(&panel->op_lock);

	mutex_lock(&panel->panel_bl.lock);
	panel_data->props.adaptive_control = 1;
#ifdef CONFIG_SUPPORT_XTALK_MODE
	panel_data->props.xtalk_mode = XTALK_OFF;
#endif
	panel_data->props.poc_onoff = POC_ONOFF_ON;
	mutex_unlock(&panel->panel_bl.lock);

	panel_data->props.mres_mode = 0;
	panel_data->props.old_mres_mode = 0;
	panel_data->props.mres_updated = false;
	panel_data->props.ub_con_cnt = 0;
	panel_data->props.conn_det_enable = 0;

	/* variable refresh rate */
	panel_data->props.vrr_fps = 60;
	panel_data->props.vrr_mode = VRR_NORMAL_MODE;
	panel_data->props.vrr_idx = 0;
	panel_data->props.vrr_origin_fps = 60;
	panel_data->props.vrr_origin_mode = VRR_NORMAL_MODE;
	panel_data->props.vrr_origin_idx = 0;
#ifdef CONFIG_PANEL_VRR_BRIDGE
	mutex_lock(&panel->io_lock);
	panel_data->props.vrr_bridge_enable = true;
	mutex_unlock(&panel->io_lock);
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	panel_data->props.enable_fd = 1;
#endif

#ifdef CONFIG_MCD_PANEL_FACTORY
	/*
	 * set vrr_lfd min/max high frequency to
	 * disable lfd in factory binary as default.
	 */
	for (i = 0; i < MAX_VRR_LFD_SCOPE; i++)
		panel_data->props.vrr_lfd_info.req[VRR_LFD_CLIENT_FAC][i].fix = VRR_LFD_FREQ_HIGH;
	queue_delayed_work(panel->work[PANEL_WORK_UPDATE].wq,
			&panel->work[PANEL_WORK_UPDATE].dwork, msecs_to_jiffies(0));
#endif
	return 0;
}

__visible_for_testing int panel_create_lcd_class(void)
{
	if (lcd_class) {
		panel_warn("lcd_class already exist\n");
		return 0;
	}

	lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR_OR_NULL(lcd_class)) {
		panel_err("failed to create lcd class\n");
		return -EINVAL;
	}

	return 0;
}

__visible_for_testing int panel_destroy_lcd_class(void)
{
	if (!lcd_class)
		return -EINVAL;

	class_destroy(lcd_class);
	lcd_class = NULL;

	return 0;
}

__visible_for_testing int panel_create_lcd_device(struct panel_device *panel, unsigned int id)
{
	char name[MAX_PANEL_DEV_NAME_SIZE];

	if (!lcd_class || !panel)
		return -EINVAL;

	if (id == 0)
		snprintf(name, MAX_PANEL_DEV_NAME_SIZE,
				"%s", PANEL_DEV_NAME);
	else
		snprintf(name, MAX_PANEL_DEV_NAME_SIZE,
				"%s-%d", PANEL_DEV_NAME, id);

	panel->lcd_dev = device_create(lcd_class,
			panel->dev, 0, panel, "%s", name);
	if (IS_ERR_OR_NULL(panel->lcd_dev)) {
		panel_err("failed to create lcd device\n");
		return PTR_ERR(panel->lcd_dev);
	}

	return 0;
}

__visible_for_testing int panel_destroy_lcd_device(struct panel_device *panel)
{
	if (!panel || !panel->lcd_dev)
		return -EINVAL;

	device_unregister(panel->lcd_dev);
	panel->lcd_dev = NULL;

	return 0;
}

int panel_probe(struct panel_device *panel)
{
	int ret = 0;
	struct panel_info *panel_data;
	struct common_panel_info *info;

	panel_info("+\n");

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	ret = panel_initialize_regulator(panel);
	if (ret < 0)
		panel_warn("error occurred during initialize regulator\n");
	panel_drv_set_regulators(panel);

	info = panel_detect(panel);
	if (unlikely(!info)) {
		panel_err("panel detection failed\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
	/*
	 * TODO : move to parse dt function
	 * 1. get panel_spi device node.
	 * 2. get spi_device of node
	 */
	panel->spi = of_find_panel_spi_by_node(NULL);
	if (!panel->spi)
		panel_warn("panel spi device unsupported\n");
#endif

	ret = panel_init_property(panel);
	if (ret < 0) {
		panel_err("failed to initialize property\n");
		return -EINVAL;
	}

	ret = panel_prepare(panel, info);
	if (unlikely(ret < 0)) {
		panel_err("failed to prepare common panel driver\n");
		return -ENODEV;
	}

#ifdef CONFIG_SUPPORT_POC_SPI
	ret = panel_spi_drv_probe(panel, info->spi_data_tbl, info->nr_spi_data_tbl);
	if (unlikely(ret))
		panel_err("failed to probe panel spi driver\n");

#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_probe(panel, info->poc_data);
	if (unlikely(ret))
		panel_err("failed to probe poc driver\n");

#endif /* CONFIG_SUPPORT_DDI_FLASH */

	ret = panel_resource_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return -ENODEV;
	}

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return -ENODEV;
	}

	ret = panel_bl_probe(&panel->panel_bl);
	if (unlikely(ret)) {
		panel_err("failed to probe backlight driver\n");
		return -ENODEV;
	}

	ret = panel_boot_on(panel);
	if (unlikely(ret)) {
		panel_err("failed to panel boot on seq\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_probe(&panel->mdnie, info->mdnie_tune);
	if (unlikely(ret)) {
		panel_err("failed to probe mdnie driver\n");
		return -ENODEV;
	}
#endif


#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	ret = copr_probe(panel, info->copr_data);
	if (unlikely(ret)) {
		panel_err("failed to probe copr driver\n");
		BUG();
		return -ENODEV;
	}
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = aod_drv_probe(panel, info->aod_tune);
	if (unlikely(ret)) {
		panel_err("failed to probe aod driver\n");
		BUG();
		return -ENODEV;
	}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
	ret = panel_get_v4l2_abc_dev(panel, info->mafpc_info);
	if (unlikely(ret < 0)) {
		panel_err("failed to probe mafpc driver\n");
		return -ENODEV;
	}
#endif

#if defined(CONFIG_PANEL_FREQ_HOP)
	ret = panel_freq_hop_probe(panel,
			info->freq_hop_elems, info->nr_freq_hop_elems);
	if (ret)
		panel_err("failed to register dynamic mipi module\n");
#endif
	panel_init_clock_info(panel);

#ifdef CONFIG_SUPPORT_DIM_FLASH
	mutex_lock(&panel->panel_bl.lock);
	mutex_lock(&panel->op_lock);
	for (i = 0; i < MAX_PANEL_BL_SUBDEV; i++) {
		if (panel_data->panel_dim_info[i] == NULL)
			continue;

		if (panel_data->panel_dim_info[i]->dim_flash_on) {
			panel_data->props.dim_flash_on = true;
			panel_info("dim_flash : on\n");
			break;
		}
	}
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->panel_bl.lock);

	if (panel_data->props.dim_flash_on)
		queue_delayed_work(panel->work[PANEL_WORK_DIM_FLASH].wq,
				&panel->work[PANEL_WORK_DIM_FLASH].dwork,
				msecs_to_jiffies(500));
#endif /* CONFIG_SUPPORT_DIM_FLASH */
	init_check_wq(&panel->condition_check);

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	mutex_lock(&panel->op_lock);
	ret = panel_display_mode_get_native_mode(panel);
	if (ret >= 0) {
		panel_data->props.panel_mode = ret;
#if defined(CONFIG_PANEL_VRR_BRIDGE)
		panel->panel_data.props.target_panel_mode = ret;
#endif
		panel_info("apply default panel_mode %d\n", ret);

	} else {
		panel_err("failed to update panel_mode\n");
	}
	mutex_unlock(&panel->op_lock);

	panel_update_brightness_cmd_skip(panel);

	ret = panel_update_display_mode(panel);
	if (ret < 0)
		panel_err("failed to panel_update_display_mode\n");

#endif

	panel_info("-\n");

	return 0;
}

int panel_remove(struct panel_device *panel)
{
	int ret;

#ifdef CONFIG_SUPPORT_DIM_FLASH
	/* TODO: dim flash need to flush before sleep in */
	if (panel->panel_data.props.dim_flash_on)
		flush_delayed_work(&panel->work[PANEL_WORK_DIM_FLASH].dwork);
#endif

#if defined(CONFIG_PANEL_FREQ_HOP)
	/* TODO: remove dynamic_freq */
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = aod_drv_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove aod driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	ret = copr_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove copr driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_remove(&panel->mdnie);
	if (ret < 0) {
		panel_err("failed to remove mdnie driver\n");
		return ret;
	}
#endif

	ret = panel_bl_remove(&panel->panel_bl);
	if (ret < 0) {
		panel_err("failed to remove panel-bl driver\n");
		return ret;
	}

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove poc driver\n");
		return ret;
	}
#endif /* CONFIG_SUPPORT_DDI_FLASH */

#ifdef CONFIG_SUPPORT_POC_SPI
	ret = panel_spi_drv_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel spi driver\n");
		return ret;
	}
#endif
	panel->funcs = NULL;

	return 0;
}

__visible_for_testing int panel_sleep_in(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	enum panel_active_state prev_state;
	ktime_t start;

	if (!panel)
		return -EINVAL;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		goto do_exit;
	}

	state = &panel->state;
	prev_state = state->cur_state;
	PRINT_PANEL_STATE_BEGIN(prev_state, PANEL_STATE_ON, start);

	switch (state->cur_state) {
	case PANEL_STATE_ON:
		panel_warn("panel already %s state\n", panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_NORMAL:
	case PANEL_STATE_ALPM:
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
		copr_disable(&panel->copr);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
		mdnie_disable(&panel->mdnie);
#endif
		ret = panel_display_off(panel);
		if (unlikely(ret < 0))
			panel_err("failed to write display_off seqtbl\n");
		ret = __panel_seq_exit(panel);
		if (unlikely(ret < 0))
			panel_err("failed to write exit seqtbl\n");
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}

	panel_set_cur_state(panel, PANEL_STATE_ON);
	PRINT_PANEL_STATE_END(prev_state, state->cur_state, start);

	return 0;

do_exit:
	return ret;
}

__visible_for_testing int panel_power_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	enum panel_active_state prev_state;
	ktime_t start;

	if (!panel)
		return -EINVAL;

	panel->state.connected = panel_conn_det_state(panel);
	if (panel->state.connected < 0) {
		panel_dbg("conn-det unsupported\n");
	} else if (panel->state.connected == PANEL_STATE_NOK) {
		panel_warn("panel disconnected\n");
		panel_dsi_set_bypass(panel, true);
		if (!panel_bypass_is_on(panel))
			panel_set_bypass(panel, PANEL_BYPASS_ON);
	} else {
		panel_dbg("panel connected\n");
		panel_dsi_set_bypass(panel, false);
		if (panel_bypass_is_on(panel))
			panel_set_bypass(panel, PANEL_BYPASS_OFF);
	}

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		ret = -ENODEV;
		goto do_exit;
	}

	state = &panel->state;
	prev_state = state->cur_state;
	PRINT_PANEL_STATE_BEGIN(prev_state, PANEL_STATE_ON, start);

	if (state->cur_state == PANEL_STATE_OFF) {
		ret = __set_panel_power(panel, PANEL_POWER_ON);
		if (ret) {
			panel_err("failed to panel power on\n");
			goto do_exit;
		}
		panel_set_cur_state(panel, PANEL_STATE_ON);
	}
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(panel->state.connected == PANEL_STATE_OK ?
		PANEL_EVENT_UB_CON_CONNECTED : PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
	PRINT_PANEL_STATE_END(prev_state, state->cur_state, start);

	return 0;

do_exit:
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
	return ret;
}

__visible_for_testing int panel_power_off(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	enum panel_active_state prev_state;
	ktime_t start;

	if (!panel)
		return -EINVAL;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		return -ENODEV;
	}

	state = &panel->state;
	prev_state = state->cur_state;
	PRINT_PANEL_STATE_BEGIN(prev_state, PANEL_STATE_OFF, start);

	switch (state->cur_state) {
	case PANEL_STATE_OFF:
		panel_info("panel already %s state\n", panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_ALPM:
	case PANEL_STATE_NORMAL:
		panel_sleep_in(panel);
	case PANEL_STATE_ON:
		ret = __set_panel_power(panel, PANEL_POWER_OFF);
		if (ret) {
			panel_err("failed to panel power off\n");
			goto do_exit;
		}
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}

	ret = panel_power_control_execute(panel, "panel_fd_disable");
	if (ret < 0 && ret != -ENODATA)
		panel_err("failed to panel_fd_disable\n");

	panel_set_cur_state(panel, PANEL_STATE_OFF);
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_power_off(panel);
	if (ret)
		panel_err("failed to aod power off\n");
#endif
	PRINT_PANEL_STATE_END(prev_state, state->cur_state, start);

	return 0;

do_exit:
	return ret;
}

__visible_for_testing int panel_reset_lp11(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	enum panel_active_state prev_state;
	struct panel_bl_device *panel_bl;

	if (!panel)
		return -EINVAL;

	panel_bl = &panel->panel_bl;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		ret = -ENODEV;
		goto do_exit;
	}

	state = &panel->state;
	prev_state = state->cur_state;

	if (state->cur_state == PANEL_POWER_ON) {
		mutex_lock(&panel_bl->lock);
		mutex_lock(&panel->op_lock);

		ret = panel_power_control_execute(panel, "panel_reset_lp11");
		if (ret < 0 && ret != -ENODATA)
			panel_err("failed to panel_reset_lp11\n");

		mutex_unlock(&panel->op_lock);
		mutex_unlock(&panel_bl->lock);
	}

	return 0;

do_exit:
	return ret;
}

__visible_for_testing int panel_sleep_out(struct panel_device *panel)
{
	int ret = 0;
	static int retry = 3;
	struct panel_state *state;
	enum panel_active_state prev_state;
	ktime_t start;
	int pcd_state;

	if (!panel)
		return -EINVAL;

	panel->state.connected = panel_conn_det_state(panel);
	if (panel->state.connected < 0) {
		panel_dbg("conn-det unsupported\n");
	} else if (panel->state.connected == PANEL_STATE_NOK) {
		panel_warn("panel disconnected\n");
		panel_set_bypass(panel, PANEL_BYPASS_ON);
		panel_print_bypass_reason(panel);
#if defined(CONFIG_SUPPORT_PANEL_SWAP)
		return -ENODEV;
#endif
	} else {
		panel_info("panel connected\n");
		panel_dsi_set_bypass(panel, PANEL_BYPASS_OFF);
		panel_set_bypass(panel, PANEL_BYPASS_OFF);
	}

	state = &panel->state;
	prev_state = state->cur_state;
	PRINT_PANEL_STATE_BEGIN(prev_state, PANEL_STATE_NORMAL, start);

	switch (state->cur_state) {
	case PANEL_STATE_NORMAL:
		panel_warn("panel already %s state\n",
				panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_ALPM:
#ifdef CONFIG_MCD_PANEL_LPM
		ret = __panel_seq_exit_alpm(panel);
		if (ret) {
			panel_err("failed to panel exit alpm\n");
			goto do_exit;
		}
#endif
		break;
	case PANEL_STATE_OFF:
		ret = panel_power_on(panel);
		if (ret) {
			panel_err("failed to power on\n");
			goto do_exit;
		}
	case PANEL_STATE_ON:
		ret = __panel_seq_init(panel);
		if (ret) {
			if (--retry >= 0 && ret == -EAGAIN) {
				panel_dsi_set_bypass(panel, true);
				panel_power_off(panel);
				msleep(100);
				goto do_exit;
			}

			pcd_state = panel_pcd_state(panel);

			if (pcd_state == PANEL_STATE_NOK) {
				/*
				 * When panel sleep out failed 3 times continuously,
				 * If PCD is NOT OK, Phone Should run as well. (concept).
				 * Do not BUG(); Just leave bypass flag to keep going.
				 */
				panel_dsi_set_bypass(panel, true); /* ignore TE checking & frame update request*/
				panel_err("failed to panel init seq. but PCD is NOT OK. keep going.\n");

				/* Below irq is not enabled in "__panel_seq_init", so turn them on here*/
				panel_enable_disp_det_irq(panel);
				panel_enable_pcd_irq(panel);
			} else {
				panel_err("failed to panel init seq\n");
				BUG();
			}
		}
		retry = 3;
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}
	panel_set_cur_state(panel, PANEL_STATE_NORMAL);
	panel->ktime_panel_on = ktime_get();

	mutex_lock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	clear_check_wq_var(&panel->condition_check);
	mutex_unlock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
#ifdef CONFIG_PANEL_VRR_BRIDGE
	if (prev_state == PANEL_STATE_ALPM) {
		mutex_lock(&panel->op_lock);
		panel->panel_data.props.panel_mode =
			panel->panel_data.props.target_panel_mode;
		mutex_unlock(&panel->op_lock);
		ret = panel_update_display_mode(panel);
		if (ret < 0)
			panel_err("failed to panel_update_display_mode\n");
	}
#endif
#ifdef CONFIG_SUPPORT_HMD
	ret = panel_set_hmd_on(panel);
	if (ret)
		panel_err("failed to set hmd on seq\n");
#endif
	PRINT_PANEL_STATE_END(prev_state, state->cur_state, start);

	return 0;

do_exit:
	return ret;
}

#ifdef CONFIG_MCD_PANEL_LPM
__visible_for_testing int panel_doze(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;
	enum panel_active_state prev_state;
	ktime_t start;

	if (!panel)
		return -EINVAL;

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		goto do_exit;
	}

	state = &panel->state;
	prev_state = state->cur_state;
	PRINT_PANEL_STATE_BEGIN(prev_state, PANEL_STATE_ALPM, start);

	switch (state->cur_state) {
	case PANEL_STATE_ALPM:
		panel_warn("panel already %s state\n",
				panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_POWER_ON:
	case PANEL_POWER_OFF:
		ret = panel_sleep_out(panel);
		if (ret) {
			panel_err("failed to set normal\n");
			goto do_exit;
		}
	case PANEL_STATE_NORMAL:
		ret = __panel_seq_set_alpm(panel);
		if (ret)
			panel_err("failed to write alpm\n");
		panel_set_cur_state(panel, PANEL_STATE_ALPM);
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
		panel_mdnie_update(panel);
#endif
		break;
	default:
		break;
	}
	mutex_lock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	clear_check_wq_var(&panel->condition_check);
	mutex_unlock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	PRINT_PANEL_STATE_END(prev_state, state->cur_state, start);

do_exit:
	return ret;
}
#endif /* CONFIG_MCD_PANEL_LPM */

static int panel_register_cb(struct panel_device *panel, int cb_id, void *arg)
{
	struct disp_cb_info *cb_info = arg;

	if (!panel || !cb_info)
		return -EINVAL;

	if (cb_id >= MAX_PANEL_CB) {
		panel_err("out of range (cb_id:%d)\n", cb_id);
		return -EINVAL;
	}

	memcpy(&panel->cb_info[cb_id], cb_info, sizeof(*cb_info));

	return 0;
}

int panel_vrr_cb(struct panel_device *panel)
{
	struct disp_cb_info *vrr_cb_info = &panel->cb_info[PANEL_CB_VRR];
#ifdef CONFIG_PANEL_NOTIFY
	struct panel_dms_data dms_data;
	static struct panel_dms_data old_dms_data;
#endif
	struct panel_properties *props;
	struct panel_vrr *vrr;
	int ret = 0;

	props = &panel->panel_data.props;
	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	if (vrr_cb_info->cb) {
		ret = vrr_cb_info->cb((void *)vrr_cb_info->data, (void *)vrr);
		if (ret < 0)
			panel_err("failed to vrr callback\n");
	}

#ifdef CONFIG_PANEL_NOTIFY
	dms_data.fps = vrr->fps /
		max(TE_SKIP_TO_DIV(vrr->te_sw_skip_count,
					vrr->te_hw_skip_count), MIN_VRR_DIV_COUNT);
	dms_data.lfd_min_freq = props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL].lfd_min_freq;
	dms_data.lfd_max_freq = props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL].lfd_max_freq;

	/* notify clients that vrr has changed */
	if (old_dms_data.fps != dms_data.fps) {
		panel_notifier_call_chain(PANEL_EVENT_VRR_CHANGED, &dms_data);
		panel_info("PANEL_EVENT_VRR_CHANGED fps:%d lfd_freq:%d~%dHz\n",
				dms_data.fps, dms_data.lfd_min_freq, dms_data.lfd_max_freq);
	}

	/* notify clients that fps or lfd has changed */
	if (memcmp(&old_dms_data, &dms_data, sizeof(struct panel_dms_data))) {
		panel_notifier_call_chain(PANEL_EVENT_LFD_CHANGED, &dms_data);
		panel_info("PANEL_EVENT_LFD_CHANGED fps:%d lfd_freq:%d~%dHz\n",
				dms_data.fps, dms_data.lfd_min_freq, dms_data.lfd_max_freq);
	}

	memcpy(&old_dms_data, &dms_data, sizeof(struct panel_dms_data));
#endif

	return ret;
}

#ifdef CONFIG_MCD_PANEL_RCD
int panel_get_rcd_info(struct panel_device *panel, void *arg)
{
	struct panel_rcd_data **rcd_info = arg;

	if (!panel || !rcd_info) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	if (!panel->panel_data.rcd_data) {
		panel_info("rcd_data is empty\n");
		return -ENODATA;
	}

	*rcd_info = panel->panel_data.rcd_data;

	return 0;
}
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
int panel_get_display_mode(struct panel_device *panel, void *arg)
{
	struct panel_display_modes **panel_modes = arg;

	if (!panel || !panel_modes) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	if (!panel->panel_modes) {
		panel_err("panel_modes not prepared\n");
		return -EINVAL;
	}

	*panel_modes = panel->panel_modes;

	return 0;
}

int panel_display_mode_cb(struct panel_device *panel)
{
	struct disp_cb_info *cb_info = &panel->cb_info[PANEL_CB_DISPLAY_MODE];
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_properties *props = &panel->panel_data.props;
	int ret = 0, panel_mode = props->panel_mode;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	if (cb_info->cb) {
		ret = cb_info->cb(cb_info->data,
				common_panel_modes->modes[panel_mode]);
		if (ret)
			panel_err("failed to display_mode callback\n");
	}
	panel_vrr_cb(panel);

	return ret;
}

static bool check_display_mode_cond(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

#if defined(CONFIG_MCD_PANEL_FACTORY)
	if (props->alpm_mode != ALPM_OFF) {
		panel_warn("could not change display mode in lpm(%d) state\n",
			   props->alpm_mode);
		return false;
	}
#endif
	if (props->mcd_on == true) {
		panel_warn("could not change display mode in mcd(%d) state\n",
			   props->mcd_on);
		return false;
	}

	return true;
}

bool panel_display_mode_is_supported(struct panel_device *panel)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;

	if (!common_panel_modes)
		return false;

	if (common_panel_modes->num_modes == 0)
		return false;

	if (!check_seqtbl_exist(&panel->panel_data,
				PANEL_DISPLAY_MODE_SEQ))
		return false;

	return true;
}

static struct common_panel_display_mode *
panel_find_common_panel_display_mode(struct panel_device *panel,
		struct panel_display_mode *pdm)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	int i;

	if (!common_panel_modes) {
		panel_err("common_panel_modes not prepared\n");
		return NULL;
	}

	for (i = 0; i < common_panel_modes->num_modes; i++) {
		if (common_panel_modes->modes[i] == NULL)
			continue;

		if (!strncmp(common_panel_modes->modes[i]->name,
					pdm->name, sizeof(pdm->name))) {
			panel_dbg("pdm:%s cpdm:%d:%s\n",
					pdm->name, i, common_panel_modes->modes[i]->name);
			return common_panel_modes->modes[i];
		}
	}

	return NULL;
}

int find_panel_mode_by_common_panel_display_mode(struct panel_device *panel,
		struct common_panel_display_mode *cpdm)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	int i;

	for (i = 0; i < common_panel_modes->num_modes; i++)
		if (cpdm == common_panel_modes->modes[i])
			break;

	if (i == common_panel_modes->num_modes)
		return -EINVAL;

	return i;
}

int panel_display_mode_find_panel_mode(struct panel_device *panel,
		struct panel_display_mode *pdm)
{
	struct common_panel_display_mode *cpdm;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	cpdm = panel_find_common_panel_display_mode(panel, pdm);
	if (!cpdm) {
		panel_err("panel_display_mode(%s) not found\n", pdm->name);
		return -EINVAL;
	}
	panel_dbg("%s:%s\n", pdm->name, cpdm->name);

	return find_panel_mode_by_common_panel_display_mode(panel, cpdm);
}

int panel_display_mode_get_native_mode(struct panel_device *panel)
{
	struct panel_display_modes *panel_modes =
		panel->panel_modes;
	struct panel_display_mode *pdm;
	int panel_mode;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (!panel_modes) {
		panel_err("panel_modes is null\n");
		return -EINVAL;
	}

	if (panel_modes->num_modes <= 0) {
		panel_err("panel_modes->num_modes is 0\n");
		return -EINVAL;
	}

	if (panel_modes->native_mode >=
			panel_modes->num_modes) {
		panel_err("panel_modes->native_mode(%d) is out of range\n",
				panel_modes->native_mode);
		return -EINVAL;
	}

	pdm = panel_modes->modes[panel_modes->native_mode];
	panel_mode =
		panel_display_mode_find_panel_mode(panel, pdm);
	if (panel_mode < 0) {
		panel_err("could not find panel_display_mode(%s)\n", pdm->name);
		return -EINVAL;
	}

	return panel_mode;
}

int panel_display_mode_get_mres_mode(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_resol *resol;
	int i;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	resol = common_panel_modes->modes[panel_mode]->resol;
	for (i = 0; i < mres->nr_resol; i++)
		if (resol == &mres->resol[i])
			break;

	return i;
}

bool panel_display_mode_is_mres_mode_changed(struct panel_device *panel, int panel_mode)
{
	struct panel_properties *props =
		&panel->panel_data.props;
	int mres_mode;

	mres_mode = panel_display_mode_get_mres_mode(panel, panel_mode);
	if (mres_mode < 0)
		return mres_mode;

	return (props->mres_mode != mres_mode);
}

int panel_display_mode_get_vrr_idx(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_vrr *vrr, **vrrtbl = panel->panel_data.vrrtbl;
	int i, num_vrrs = panel->panel_data.nr_vrrtbl;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	vrr = common_panel_modes->modes[panel_mode]->vrr;
	if (vrr == NULL) {
		panel_err("vrr is null of panel_mode(%d)\n", panel_mode);
		return -EINVAL;
	}

	for (i = 0; i < num_vrrs; i++)
		if (vrr == vrrtbl[i])
			break;

	if (i == num_vrrs) {
		panel_warn("panel_mode(%d) vrr(%d%s) not found\n",
				panel_mode, vrr->fps, REFRESH_MODE_STR(vrr->mode));
		return -EINVAL;
	}

	return i;
}

static int panel_update_display_mode_props(struct panel_device *panel, int panel_mode)
{
	struct panel_properties *props = &panel->panel_data.props;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_vrr *vrr;
	struct panel_resol *resol;
	int mres_mode, vrr_idx;

	props->panel_mode = panel_mode;

	mres_mode = panel_display_mode_get_mres_mode(panel, panel_mode);
	if (mres_mode < 0) {
		panel_err("failed to get mres_mode from panel_mode(%d)\n", panel_mode);
		return -EINVAL;
	}

	if (mres_mode >= mres->nr_resol) {
		panel_err("out of range mres_mode(%d)\n", mres_mode);
		return -EINVAL;
	}

	resol = &mres->resol[mres_mode];
	if (props->mres_mode == mres_mode) {
		panel_dbg("same resolution(%d:%dx%d)\n",
				mres_mode, resol->w, resol->h);
		props->mres_updated = false;
	} else {
		props->old_mres_mode = props->mres_mode;
		props->mres_mode = mres_mode;
		props->mres_updated = true;
	}

	vrr_idx = panel_display_mode_get_vrr_idx(panel, panel_mode);
	if (vrr_idx < 0) {
		panel_err("failed to get vrr_idx from panel_mode(%d)\n",
				panel_mode);
		return -EINVAL;
	}

	if (vrr_idx >= panel->panel_data.nr_vrrtbl) {
		panel_err("out of range vrr_idx(%d)\n", vrr_idx);
		return -EINVAL;
	}

	vrr = panel->panel_data.vrrtbl[vrr_idx];

	/* keep origin data */
	props->vrr_origin_fps = props->vrr_fps;
	props->vrr_origin_mode = props->vrr_mode;
	props->vrr_origin_idx = props->vrr_idx;

	/* update vrr data */
	props->vrr_fps = vrr->fps;
	props->vrr_mode = vrr->mode;
	props->vrr_idx = vrr_idx;

	panel_info("updated mres(%d:%dx%d) vrr(%d:%d%s,te:%dHz)\n",
			props->mres_mode, resol->w, resol->h,
			props->vrr_idx, props->vrr_fps,
			REFRESH_MODE_STR(props->vrr_mode),
			props->vrr_fps / (vrr->te_hw_skip_count + 1));

	return 0;
}

static int panel_seq_display_mode(struct panel_device *panel)
{
	int ret;

	if (unlikely(!panel)) {
		panel_err("panel_device is null!!\n");
		return -EINVAL;
	}

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (!check_display_mode_cond(panel))
		return -EINVAL;

	ret = panel_do_seqtbl_by_index_nolock(panel,
			PANEL_DISPLAY_MODE_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to do display-mode-seq\n");
		return ret;
	}

	return 0;
}

int panel_set_display_mode_nolock(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes;
	struct panel_properties *props;
	int ret = 0;

	if (unlikely(!panel)) {
		panel_err("panel_device is null!!\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	common_panel_modes = panel->panel_data.common_panel_modes;
	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	/*
	 * apply panel device dependent display mode
	 */
	ret = panel_update_display_mode_props(panel, panel_mode);
	if (ret < 0) {
		panel_err("failed to update display mode properties\n");
		return ret;
	}

	ret = panel_seq_display_mode(panel);
	if (unlikely(ret < 0)) {
		panel_err("failed to panel_seq_display_mode\n");
		return ret;
	}

	props->vrr_origin_fps = props->vrr_fps;
	props->vrr_origin_mode = props->vrr_mode;
	props->vrr_origin_idx = props->vrr_idx;

	return ret;
}

#ifdef CONFIG_PANEL_VRR_BRIDGE
static int panel_run_vrr_bridge_thread(struct panel_device *panel)
{
	wake_up_interruptible_all(&panel->thread[PANEL_THREAD_VRR_BRIDGE].wait);

	return 0;
}
#endif

static int panel_set_display_mode(struct panel_device *panel, void *arg)
{
	int ret = 0, panel_mode;
	struct panel_display_mode *pdm = arg;
	struct panel_properties *props =
		&panel->panel_data.props;

	if (unlikely(!pdm)) {
		panel_err("panel_display_mode is null\n");
		return -EINVAL;
	}

	mutex_lock(&panel->op_lock);
	panel_mode =
		panel_display_mode_find_panel_mode(panel, pdm);
	if (panel_mode < 0) {
		panel_err("could not find panel_display_mode(%s)\n", pdm->name);
		ret = -EINVAL;
		goto out;
	}

	if (props->panel_mode == panel_mode) {
		panel_info("same panel_mode(%d)\n", panel_mode);
		goto out;
	}

	if (panel->state.cur_state != PANEL_STATE_NORMAL &&
			panel->state.cur_state != PANEL_STATE_ALPM) {
		ret = panel_update_display_mode_props(panel, panel_mode);
		if (ret < 0) {
			panel_err("failed to update display mode properties\n");
			goto out;
		}

		props->vrr_origin_fps = props->vrr_fps;
		props->vrr_origin_mode = props->vrr_mode;
		props->vrr_origin_idx = props->vrr_idx;

		goto out;
	}

#ifdef CONFIG_PANEL_VRR_BRIDGE
	props->target_panel_mode = panel_mode;
	if (panel_vrr_bridge_changeable(panel) &&
		!panel_display_mode_is_mres_mode_changed(panel, panel_mode)) {
		/* run vrr-bridge thread */
		ret = panel_run_vrr_bridge_thread(panel);
		if (ret < 0)
			panel_err("failed to run vrr-bridge thread\n");
		goto out;
	}
#endif

	ret = panel_set_display_mode_nolock(panel, panel_mode);
	if (ret < 0)
		panel_err("failed to panel_set_display_mode_nolock\n");

out:
	mutex_unlock(&panel->op_lock);

	/* callback to notify current display mode */
	if (!ret)
		panel_display_mode_cb(panel);

	return ret;
}

/**
 * panel_update_display_mode - update display mode
 * @panel: panel device
 *
 * execute DISPLAY_MODE seq with current display mode.
 */
int panel_update_display_mode(struct panel_device *panel)
{
	int ret;
	struct panel_properties *props =
		&panel->panel_data.props;

	mutex_lock(&panel->op_lock);
	ret = panel_set_display_mode_nolock(panel, props->panel_mode);
	if (ret < 0)
		panel_err("failed to panel_set_display_mode_nolock\n");
	mutex_unlock(&panel->op_lock);

	/* callback to notify current display mode */
	panel_display_mode_cb(panel);

	return ret;
}
#endif /* CONFIG_PANEL_DISPLAY_MODE */

#ifdef CONFIG_SUPPORT_DSU
static int panel_set_mres(struct panel_device *panel, void *arg)
{
	int ret = 0;
	int mres_idx;
	struct panel_properties *props;
	struct panel_mres *mres;

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	mres = &panel->panel_data.mres;
	mres_idx = *(int *)arg;

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		panel_err("multi-resolution unsupported!!\n");
		return -EINVAL;
	}

	if (mres_idx >= mres->nr_resol) {
		panel_err("invalid mres idx:%d, number:%d\n",
				mres_idx, mres->nr_resol);
		return -EINVAL;
	}

	props->old_mres_mode = props->mres_mode;
	props->mres_mode = mres_idx;
	props->mres_updated = true;
	ret = panel_do_seqtbl_by_index(panel, PANEL_DSU_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write dsu seqtbl\n");
		goto do_exit;
	}
	props->xres = mres->resol[mres_idx].w;
	props->yres = mres->resol[mres_idx].h;

	return 0;

do_exit:
	return ret;
}
#endif /* CONFIG_SUPPORT_DSU */

static inline int panel_set_ffc_seq(struct panel_device *panel, u32 dsi_freq)
{
	int ret;
	struct panel_properties *props = &panel->panel_data.props;
	u32 origin = props->dsi_freq;

	if ((props->dsi_freq == 0) ||
		(props->dsi_freq == dsi_freq))
		return 0;

	panel_info("panel update ffc frequency %d -> %dkhz\n", origin, dsi_freq);

	props->dsi_freq = dsi_freq;

	ret = panel_do_seqtbl_by_index(panel, PANEL_FFC_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write ffc seqtbl\n");
		props->dsi_freq = origin;
		return ret;
	}

	return 0;
}

static inline int panel_set_osc_seq(struct panel_device *panel, u32 osc_freq)
{
	struct panel_properties *props = &panel->panel_data.props;

	if ((props->osc_freq == 0) ||
		(props->osc_freq == osc_freq))
		return 0;

	panel_info("panel update osc frequency %dkhz -> %d\n", props->osc_freq, osc_freq);

	props->osc_freq = osc_freq;

	return 0;
}

static int panel_request_set_clock(struct panel_device *panel, void *arg)
{
	int ret = 0;
	struct panel_clock_info *info = arg;

	switch (info->clock_id) {
	case CLOCK_ID_DSI:
		ret = panel_set_ffc_seq(panel, info->clock_rate);
		break;
	case CLOCK_ID_OSC:
		ret = panel_set_osc_seq(panel, info->clock_rate);
		break;
	default:
		panel_err("Invalid clock id: %d\n", info->clock_id);
		ret = -EINVAL;
	}

	return ret;
}

static int panel_get_ddi_props(struct panel_device *panel, void *arg)
{
	if (!panel || !arg)
		return -EINVAL;

	memcpy(arg, &panel->panel_data.ddi_props, sizeof(struct ddi_properties));

	return 0;
}

int panel_ioctl_attach_adapter(struct panel_device *panel, void *arg)
{
	if (!panel || !arg) {
		panel_err("invalid argument\n");
		return -EINVAL;
	}

	memcpy(&panel->adapter, arg, sizeof(panel->adapter));

	return panel_parse_lcd_info(panel);
}

int panel_ioctl_get_panel_state(struct panel_device *panel, void *arg)
{
	struct panel_state **state = arg;

	if (!panel || !state)
		return -EINVAL;

	/* TODO: extract function to update connected state */
	panel->state.connected = panel_conn_det_state(panel);
	//Todo..
	panel_info("connected : %d\n", panel->state.connected);
	panel->state.connected = 1;

	*state = &panel->state;

	return 0;
}

int panel_register_error_cb(struct panel_device *panel, void *arg)
{
	struct disp_error_cb_info *error_cb_info = arg;

	if (!panel || !error_cb_info)
		return -EINVAL;

	panel->error_cb_info.error_cb = error_cb_info->error_cb;
	panel->error_cb_info.powerdown_cb = error_cb_info->powerdown_cb;
	panel->error_cb_info.data = error_cb_info->data;

	return 0;
}

#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
static int panel_check_cb(void *data)
{
	struct panel_device *panel = data;
	int status = DISP_CHECK_STATUS_OK;

	if (panel_conn_det_state(panel) == PANEL_STATE_NOK)
		status |= DISP_CHECK_STATUS_NODEV;
	if (panel_disp_det_state(panel) == PANEL_STATE_NOK)
		status |= DISP_CHECK_STATUS_ELOFF;
#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
	if (panel_err_fg_state(panel) == PANEL_STATE_NOK
		&& panel->panel_data.ddi_props.err_fg_powerdown)
		status |= DISP_CHECK_STATUS_NODEV;
#endif
	return status;
}


static int panel_error_cb(struct panel_device *panel)
{
	struct disp_error_cb_info *error_cb_info = &panel->error_cb_info;
	struct disp_check_cb_info panel_check_info = {
		.check_cb = (disp_check_cb *)panel_check_cb,
		.data = panel,
	};
	int ret = 0;

	if (error_cb_info->error_cb) {
		ret = error_cb_info->error_cb((void *)(error_cb_info->data),
				&panel_check_info);
		if (ret)
			panel_err("failed to recovery panel\n");
	}

	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
static int panel_powerdown_cb(struct panel_device *panel)
{
	struct disp_error_cb_info *error_cb_info = &panel->error_cb_info;
	struct disp_check_cb_info panel_check_info = {
		.check_cb = (disp_check_cb *)panel_check_cb,
		.data = panel,
	};
	int ret = 0;

	if (error_cb_info->powerdown_cb) {
		ret = error_cb_info->powerdown_cb((void *)(error_cb_info->data),
				&panel_check_info);
		if (ret)
			panel_err("failed to powerdown panel\n");
	}

	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
static int panel_set_mask_layer(struct panel_device *panel, void *arg)
{
	int ret = 0;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct mask_layer_data *req_data = (struct mask_layer_data *)arg;

	if (!panel->lcd_dev) {
		panel_err("lcd device not exist\n");
		return -EINVAL;
	}

	if (req_data->req_mask_layer == MASK_LAYER_ON) {
		if (req_data->trigger_time  == MASK_LAYER_TRIGGER_BEFORE) {
			/*
			 * W/A - During smooth dimming transition,
			 * Smooth dimming transition should stop here.
			 */

			/* 0. STOP SMOOTH DIMMING */
			mutex_lock(&panel_bl->lock);
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_STOP_DIMMING_SEQ)) {
				panel_bl->props.smooth_transition = SMOOTH_TRANS_OFF;
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_STOP_DIMMING_SEQ);
			}

			/* 1. REQ ON + FRAME START BEFORE */
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_BEFORE_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_BEFORE_SEQ);
			panel_bl->props.mask_layer_br_hook = MASK_LAYER_HOOK_ON;
			panel_bl->props.smooth_transition = SMOOTH_TRANS_OFF;
			panel_bl->props.acl_opr = ACL_OPR_OFF;
			panel_bl->props.acl_pwrsave = ACL_PWRSAVE_OFF;
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_ENTER_BR_SEQ)
				&&(panel->state.cur_state != PANEL_STATE_ALPM)) {
				panel_bl->props.brightness = panel_bl->props.mask_layer_br_target;
				panel_info("mask_layer_br enter (%d)->(%d)\n",
					panel_bl->bd->props.brightness, panel_bl->props.mask_layer_br_target);
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_ENTER_BR_SEQ);
				mutex_unlock(&panel_bl->lock);
			} else {
				mutex_unlock(&panel_bl->lock);
				panel_update_brightness(panel);
			}
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_AFTER_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_AFTER_SEQ);

		} else if  (req_data->trigger_time  == MASK_LAYER_TRIGGER_AFTER)  {
			/* 2. REQ ON + FRAME START AFTER */
			panel_bl->props.mask_layer_br_actual = panel_bl->props.mask_layer_br_target;
			sysfs_notify(&panel->lcd_dev->kobj, NULL, "actual_mask_brightness");
		}
	} else if (req_data->req_mask_layer == MASK_LAYER_OFF) {
		if (req_data->trigger_time  == MASK_LAYER_TRIGGER_BEFORE) {
			/* 3. REQ OFF + FRAME START BEFORE */
			mutex_lock(&panel_bl->lock);
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_BEFORE_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_BEFORE_SEQ);
			panel_bl->props.mask_layer_br_hook = MASK_LAYER_HOOK_OFF;

			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_EXIT_BR_SEQ)
				&& (panel->state.cur_state != PANEL_STATE_ALPM)) {
				panel_info("mask_layer_br exit (%d)->(%d)\n",
					panel_bl->props.mask_layer_br_target, panel_bl->bd->props.brightness);
				panel_bl->props.brightness = panel_bl->bd->props.brightness;
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_DISP].brightness = panel_bl->props.brightness;
#ifdef CONFIG_SUPPORT_AOD_BL
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness = panel_bl->props.brightness;
#endif
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_EXIT_BR_SEQ);
				mutex_unlock(&panel_bl->lock);
			} else {
				mutex_unlock(&panel_bl->lock);
				panel_update_brightness(panel);
			}
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_AFTER_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_AFTER_SEQ);
		} else if  (req_data->trigger_time  == MASK_LAYER_TRIGGER_AFTER)  {
			/* 4. REQ OFF + FRAME START AFTER */
			/* HOLD TE <-> DDI VSYNC Duration */
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_EXIT_AFTER_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_EXIT_AFTER_SEQ);

			panel_bl->props.smooth_transition = SMOOTH_TRANS_ON;
			panel_bl->props.mask_layer_br_actual = 0;
			sysfs_notify(&panel->lcd_dev->kobj, NULL, "actual_mask_brightness");
		}
	}
	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
static int panel_notify_frame_done_mafpc(struct panel_device *panel)
{
	int ret = 0;

	ret = cmd_v4l2_mafpc_dev(panel, V4L2_IOCL_MAFPC_FRAME_DONE, panel);

	return ret;
}
#endif

int panel_ioctl_event_frame_done(struct panel_device *panel, void *arg)
{
	int ret = 0;
#ifdef CONFIG_SUPPORT_DSU
	static int mres_updated_frame_cnt;
#endif

	if (panel->state.cur_state != PANEL_STATE_NORMAL &&
			panel->state.cur_state != PANEL_STATE_ALPM) {
		panel_warn("FRAME_DONE (panel_state:%s, disp_on:%s)\n",
				panel_state_names[panel->state.cur_state],
				panel->state.disp_on ? "on" : "off");
		return 0;
	}

	if (unlikely(panel->state.disp_on == PANEL_DISPLAY_OFF)) {
		panel_info("FRAME_DONE (panel_state:%s, display on)\n",
				panel_state_names[panel->state.cur_state]);
		ret = panel_display_on(panel);
		panel_check_ready(panel);
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	copr_update_start(&panel->copr, 3);
#endif

#ifdef CONFIG_SUPPORT_DSU
	if (panel->panel_data.props.mres_updated &&
			(++mres_updated_frame_cnt > 1)) {
		panel->panel_data.props.mres_updated = false;
		mres_updated_frame_cnt = 0;
	}
#endif
	if (panel->condition_check.is_panel_check)
		panel_check_start(panel);
#ifdef CONFIG_SUPPORT_MAFPC
	panel_notify_frame_done_mafpc(panel);
#endif

	return ret;
}

int panel_ioctl_event_vsync(struct panel_device *panel, void *arg)
{
	return 0;
}

int panel_drv_attach_adapter_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, attach_adapter, arg);
}

int panel_drv_get_panel_state_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, get_panel_state, arg);
}

int panel_drv_panel_probe_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, probe);
}

int panel_drv_set_power_ioctl(struct panel_device *panel, void *arg)
{
	if (!panel || !arg)
		return -EINVAL;

	return (*(int *)arg == 0) ?
		call_panel_drv_func(panel, power_off) :
		call_panel_drv_func(panel, power_on);
}

int panel_drv_sleep_in_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, sleep_in);
}

int panel_drv_sleep_out_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, sleep_out);
}

int panel_drv_disp_on_ioctl(struct panel_device *panel, void *arg)
{
	if (!panel || !arg)
		return -EINVAL;

	return (*(int *)arg == 0) ?
		call_panel_drv_func(panel, display_off) :
		call_panel_drv_func(panel, display_on);
}

int panel_drv_panel_dump_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, debug_dump);
}

int panel_drv_evt_frame_done_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, frame_done, arg);
}

int panel_drv_evt_vsync_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, vsync, arg);
}

#ifdef CONFIG_MCD_PANEL_LPM
int panel_drv_doze_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, doze);
}

int panel_drv_doze_suspend_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, doze);
}
#endif

int panel_drv_set_mres_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, set_mres, arg);
}

int panel_drv_get_mres_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, get_mres, arg);
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
int panel_drv_get_display_mode_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, get_display_mode, arg);
}

int panel_drv_set_display_mode_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, set_display_mode, arg);
}

int panel_drv_reg_display_mode_cb_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, register_cb, PANEL_CB_DISPLAY_MODE, arg);
}
#endif

int panel_drv_reg_reset_cb_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, register_error_cb, arg);
}

int panel_drv_reg_vrr_cb_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, register_cb, PANEL_CB_VRR, arg);
}

#ifdef CONFIG_SUPPORT_MASK_LAYER
int panel_drv_set_mask_layer_ioctl(struct panel_device *panel, void *arg)
{
	return call_panel_drv_func(panel, set_mask_layer, arg);
}
#endif

int panel_drv_set_gpios(struct panel_device *panel)
{
	int rst_val = -1, det_val = -1;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	/* FIX ME : check if we can delete the rst pin condition */

	if (panel_is_gpio_valid(panel, PANEL_GPIO_RESET)) {
		rst_val = panel_get_gpio_value(panel, PANEL_GPIO_RESET);
	} else if (find_panel_regulator_by_node_name(panel, PANEL_REGULATOR_NAME_RESET)) {
		rst_val = (boot_panel_id >= 0) ? 1 : 0;
		panel_info("Fixed reg(%s) is exist.\n", PANEL_REGULATOR_NAME_RESET);
	} else {
		panel_err("gpio(%s) not exist\n",
				panel_get_gpio_name(panel, PANEL_GPIO_RESET));
		return -EINVAL;
	}

	if (panel_is_gpio_valid(panel, PANEL_GPIO_DISP_DET))
		det_val = panel_get_gpio_value(panel, PANEL_GPIO_DISP_DET);

	else
		panel_err("gpio(%s) not exist\n",
				panel_get_gpio_name(panel, PANEL_GPIO_DISP_DET));

	/*
	 * panel state is decided by rst, conn_det and disp_det pin
	 *
	 * @rst_val
	 *  0 : need to init panel in kernel
	 *  1 : already initialized in bootloader
	 *
	 * @conn_det
	 *  < 0 : unsupported
	 *  = 0 : panel connector is disconnected
	 *  = 1 : panel connector is connected
	 *
	 * @det_val
	 *  < 0 : unsupported
	 *  0 : panel is "sleep in" state
	 *  1 : panel is "sleep out" state
	 */

	panel->state.init_at = (rst_val == 1) ?
		PANEL_INIT_BOOT : PANEL_INIT_KERNEL;

	panel->state.connected = panel_conn_det_state(panel);

	/* bypass : decide to use or ignore panel */
	if ((panel->state.init_at == PANEL_INIT_BOOT) &&
		(panel->state.connected != 0) && (det_val != 0)) {
		/*
		 * connect panel condition
		 * conn_det is normal(not zero)
		 * disp_det is nomal(1) or unsupported(< 0)
		 * init panel in bootloader(rst == 1)
		 */
		panel_set_bypass(panel, PANEL_BYPASS_OFF);
		panel_set_cur_state(panel, PANEL_STATE_NORMAL);
		panel->state.power = PANEL_POWER_ON;
		panel->state.disp_on = PANEL_DISPLAY_ON;
		panel_set_gpio_value(panel, PANEL_GPIO_RESET, 1);
	} else {
		panel_set_bypass(panel, PANEL_BYPASS_ON);
		panel_set_cur_state(panel, PANEL_STATE_OFF);
		panel->state.power = PANEL_POWER_OFF;
		panel->state.disp_on = PANEL_DISPLAY_OFF;
		panel_set_gpio_value(panel, PANEL_GPIO_RESET, 0);
	}
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(panel->state.connected == PANEL_STATE_OK ?
		PANEL_EVENT_UB_CON_CONNECTED : PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
	panel_info("rst:%d, disp_det:%d (init_at:%s, ub_con:%d(%s) panel:(%s))\n",
			rst_val, det_val, (panel->state.init_at ? "BL" : "KERNEL"),
			panel->state.connected,
			(panel->state.connected < 0 ? "UNSUPPORTED" :
			 (panel->state.connected == true ? "CONNECTED" : "DISCONNECTED")),
			(!panel_bypass_is_on(panel) ? "USE" : "NO USE"));

	return 0;
}

int panel_drv_set_regulators(struct panel_device *panel)
{
	int ret = 0;

	if (panel->state.init_at == PANEL_INIT_BOOT) {
		ret = panel_power_control_execute(panel, "panel_boot_on");
		if (ret < 0 && ret != -ENODATA)
			panel_err("failed to execute panel_boot_on\n");

		if (panel_bypass_is_on(panel)) {
			ret = panel_power_control_execute(panel, "panel_power_off");
			if (ret < 0 && ret != -ENODATA)
				panel_err("failed to execute panel_power_off\n");
		}
	}
	return ret;
}

#if !defined(CONFIG_UML)
static int panel_parse_pinctrl(struct panel_device *panel)
{
	int ret = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel->pinctrl = devm_pinctrl_get(panel->dev);
	if (IS_ERR(panel->pinctrl)) {
		panel_err("failed to get device's pinctrl\n");
		goto exit_parse_pinctrl;
	}

	panel->default_gpio_pinctrl = pinctrl_lookup_state(panel->pinctrl, "default");
	if (IS_ERR(panel->default_gpio_pinctrl)) {
		panel_err("can't get default pinctrl setting\n");
		goto exit_parse_pinctrl;
	}

	if (pinctrl_select_state(panel->pinctrl, panel->default_gpio_pinctrl)) {
		panel_err("%s failed to set default pinctrl\n", __func__);
		goto exit_parse_pinctrl;
	}

	panel_info("pinctrl setting done\n");

exit_parse_pinctrl:
	return ret;
}
#endif

struct panel_gpio *panel_gpio_list_add(struct panel_device *panel, struct panel_gpio *_gpio)
{
	struct panel_gpio *gpio;

	if (!panel || !_gpio) {
		panel_err("invalid args\n");
		return NULL;
	}

	gpio = kzalloc(sizeof(struct panel_gpio), GFP_KERNEL);
	if (!gpio)
		return NULL;

	memcpy(gpio, _gpio, sizeof(struct panel_gpio));
	list_add(&gpio->head, &panel->gpio_list);
	return gpio;
}

static int panel_parse_gpio(struct panel_device *panel)
{
	struct device *dev = panel->dev;
	struct device_node *gpios_np, *np;
	struct panel_gpio gpio;
	struct panel_gpio *p_gpio;

	gpios_np = of_get_child_by_name(dev->of_node, "gpios");
	if (!gpios_np) {
		panel_err("'gpios' node not found\n");
		return -EINVAL;
	}

	for_each_child_of_node(gpios_np, np) {
		memset(&gpio, 0, sizeof(gpio));
		if (of_get_panel_gpio(np, &gpio)) {
			panel_err("failed to get gpio %s\n", np->name);
			break;
		}

		p_gpio = panel_gpio_list_add(panel, &gpio);
		if (!p_gpio) {
			panel_err("failed to add gpio list %s\n", gpio.name);
			break;
		}
	}
	of_node_put(gpios_np);

	return 0;
}

__visible_for_testing struct panel_regulator *panel_regulator_list_add(struct panel_device *panel,
	struct panel_regulator *_reg)
{
	struct panel_regulator *reg;

	if (!panel || !_reg) {
		panel_err("invalid args\n");
		return ERR_PTR(-EINVAL);
	}

	reg = kzalloc(sizeof(struct panel_regulator), GFP_KERNEL);
	if (!reg)
		return ERR_PTR(-ENOMEM);

	memcpy(reg, _reg, sizeof(struct panel_regulator));
	list_add(&reg->head, &panel->regulator_list);
	return reg;
}

static int panel_parse_regulator(struct panel_device *panel)
{
	struct device *dev = panel->dev;
	struct panel_regulator regulator;
	struct device_node *regulators_np, *np;

	regulators_np = of_get_child_by_name(dev->of_node, "regulators");
	if (!regulators_np) {
		panel_err("'regulators' node not found\n");
		return -EINVAL;
	}

	for_each_child_of_node(regulators_np, np) {
		memset(&regulator, 0, sizeof(regulator));
		if (of_get_panel_regulator(np, &regulator) < 0) {
			panel_err("failed to get regulator %s\n", np->name);
			of_node_put(regulators_np);
			return -EINVAL;
		}

		if (IS_ERR_OR_NULL(panel_regulator_list_add(panel, &regulator))) {
			panel_err("failed to add regulator list %s\n", regulator.name);
			break;
		}

		panel_info("found regulator name:%s type:%d\n", regulator.name, regulator.type);
	}
	of_node_put(regulators_np);

	panel_dbg("done\n");

	return 0;
}

struct panel_regulator *find_panel_regulator_by_node_name(struct panel_device *panel, const char *node_name)
{
	struct panel_regulator *regulator;

	if (!panel || !node_name)
		return NULL;

	list_for_each_entry(regulator, &panel->regulator_list, head) {
		if (!strcmp(regulator->node_name, node_name))
			return regulator;
	}

	return NULL;
}

static int panel_parse_power_ctrl(struct panel_device *panel)
{
	struct device_node *power_np, *seq_np;
	struct property *pp;
	struct panel_power_ctrl *p_seq;
	int ret = 0;

	panel_info("++\n");
	power_np = panel->power_ctrl_node;
	if (!power_np) {
		panel_err("'power_ctrl' node not found\n");
		ret = -EINVAL;
		goto exit;
	}

	seq_np = of_get_child_by_name(power_np, "sequences");
	if (!seq_np) {
		panel_err("'sequences' node not found\n");
		ret = -EINVAL;
		goto exit_power;
	}

	for_each_property_of_node(seq_np, pp) {
		if (!strcmp(pp->name, "name") || !strcmp(pp->name, "phandle"))
			continue;
		p_seq = kzalloc(sizeof(struct panel_power_ctrl), GFP_KERNEL);
		if (!p_seq) {
			ret = -ENOMEM;
			goto exit_power;
		}
		p_seq->dev_name = panel->of_node_name;
		p_seq->name = pp->name;
		ret = of_get_panel_power_ctrl(panel, seq_np, pp->name, p_seq);
		if (ret < 0) {
			panel_err("failed to get power_ctrl %s\n", pp->name);
			break;
		}
		list_add_tail(&p_seq->head, &panel->power_ctrl_list);
		panel_info("power_ctrl '%s' initialized\n", p_seq->name);
	}
	of_node_put(seq_np);
exit_power:
	of_node_put(power_np);
exit:
	panel_info("--\n");
	return ret;
}


static irqreturn_t panel_work_isr(int irq, void *dev_id)
{
	struct panel_work *w = (struct panel_work *)dev_id;

	queue_delayed_work(w->wq, &w->dwork, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static struct panel_work *panel_find_work(struct panel_device *panel, const char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < PANEL_WORK_MAX; i++) {
		if (!strncmp(panel_work_names[i],
					name, MAX_PANEL_WORK_NAME_SIZE))
			break;
	}

	if (i == PANEL_WORK_MAX)
		return NULL;

	return &panel->work[i];
}

static int panel_devm_request_irq(struct panel_device *panel,
		enum panel_gpio_lists panel_gpio_id)
{
	struct panel_gpio *gpio;
	struct panel_work *work;

	if (!panel_is_gpio_irq_valid(panel, panel_gpio_id))
		return -EINVAL;

	gpio = panel_get_gpio(panel, panel_gpio_id);
	if (!gpio)
		return -EINVAL;

	work = panel_find_work(panel, gpio->name);
	if (!work) {
		panel_err("failed to find work(%s)\n", gpio->name);
		return -EINVAL;
	}

	return panel_gpio_helper_devm_request_irq(gpio,
			panel->dev, panel_work_isr, work->name, work);
}

int panel_register_isr(struct panel_device *panel)
{
	int i = 0, ret = 0;

	if (panel == NULL) {
		panel_err("panel is NULL\n");
		return -EINVAL;
	}

	if (panel->dev == NULL) {
		panel_err("panel->dev is NULL\n");
		return -ENODEV;
	}

	if (panel_bypass_is_on(panel)) {
		panel_print_bypass_reason(panel);
		return 0;
	}

	for (i = 0; i < PANEL_GPIO_MAX; i++) {
		if (!panel_is_gpio_irq_valid(panel, i))
			continue;

		if (!panel_find_work(panel, panel_gpio_names[i]))
			continue;

		ret = panel_devm_request_irq(panel, i);
		if (ret < 0) {
			panel_err("failed to register irq(%s)\n",
					panel_gpio_names[i]);
			break;
		}
	}

	if (ret < 0)
		return ret;

	return 0;
}

int panel_parse_lcd_info(struct panel_device *panel)
{
	struct device_node *node;
	struct device *dev;
	int ret = 0;

	if (!panel || !(panel->dev))
		return -EINVAL;

	dev = panel->dev;

	panel_info("PANEL_INFO:panel id : %x\n", boot_panel_id);

	node = find_panel_power_ctrl_node(panel, boot_panel_id);
	if (!node) {
		panel_err("panel not found (boot_panel_id 0x%08X)\n", boot_panel_id);
		node = of_parse_phandle(dev->of_node, "panel-power-ctrl", 0);
		if (!node) {
			panel_err("failed to get phandle of panel-power-ctrl\n");
			return -EINVAL;
		}
	}

	panel->power_ctrl_node = node;

	ret = panel_parse_power_ctrl(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse power_ctrl\n", panel->id);
		return ret;
	}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	node = find_panel_modes_node(panel, boot_panel_id);
	if (node != NULL) {
		panel->panel_modes =
			of_get_panel_display_modes(node);
		if (panel->panel_modes == NULL)
			panel_warn("failed to get panel_modes\n");
		else
			panel_info("get panel display modes(%d)\n",
					panel->panel_modes->num_modes);
	} else {
		panel_warn("panel modes not found (boot_panel_id 0x%08X)\n", boot_panel_id);
	}
#endif

	node = find_panel_ddi_node(panel, boot_panel_id);
	if (!node) {
		panel_err("panel not found (boot_panel_id 0x%08X)\n", boot_panel_id);
		node = of_parse_phandle(dev->of_node, "ddi-info", 0);
		if (!node) {
			panel_err("failed to get phandle of ddi-info\n");
			return -EINVAL;
		}
	}
	panel->ap_vendor_setting_node = node;

	panel->panel_data.dqe_suffix =
		get_panel_lut_dqe_suffix(panel, boot_panel_id);

#if defined(CONFIG_PANEL_FREQ_HOP)
	panel->freq_hop_node =
		get_panel_lut_freq_hop_node(panel, boot_panel_id);
#endif

	panel_parse_ap_vendor_node(panel, node);

	return 0;
}


static int panel_parse_panel_lookup(struct panel_device *panel)
{
	struct device *dev = panel->dev;
	struct panel_dt_lut *lut;
	struct panel_id_mask *id_mask;
	struct device_node *lookup_np, *panel_np, *node;
	struct property *pp;
	int ret, i, sz;
	int tmparr[32];

	lookup_np = of_get_child_by_name(dev->of_node, "panel-lut");
	if (unlikely(!lookup_np)) {
		panel_warn("No DT node for panel-lut\n");
		return -EINVAL;
	}

	for_each_property_of_node(lookup_np, pp) {
		if (!strcmp(pp->name, "name") || !strcmp(pp->name, "phandle"))
			continue;

		lut = kzalloc(sizeof(struct panel_dt_lut), GFP_KERNEL);
		if (!lut)
			return -ENOMEM;

		INIT_LIST_HEAD(&lut->id_mask_list);

		panel_np = of_parse_phandle(lookup_np, pp->name, 0);
		if (!panel_np) {
			panel_err("failed to get phandle of panel %s\n", pp->name);
			return -EINVAL;
		}
		lut->name = panel_np->name;
		node = of_parse_phandle(panel_np, "ddi", 0);
		if (!node) {
			panel_err("failed to get phandle of ddi\n");
			return -EINVAL;
		}
		lut->ap_vendor_setting_node = node;
		of_node_put(node);

		node = of_parse_phandle(panel_np, "display-mode", 0);
		if (!node) {
			panel_err("failed to get phandle of display-mode\n");
			return -EINVAL;
		}
		lut->panel_modes_node = node;
		of_node_put(node);

		node = of_parse_phandle(panel_np, "power-ctrl", 0);
		if (!node) {
			panel_err("failed to get phandle of power-ctrl\n");
			return -EINVAL;
		}
		lut->power_ctrl_node= node;
		of_node_put(node);

		ret = of_property_read_string(panel_np, "dqe-suffix", &lut->dqe_suffix);
		if (ret != 0 || lut->dqe_suffix == NULL) {
			panel_warn("dqe-suffix is empty\n");
		} else {
			panel_info("found dqe-suffix:%s\n", lut->dqe_suffix);
		}

#if defined(CONFIG_PANEL_FREQ_HOP)
		node = of_parse_phandle(panel_np, DT_NAME_FREQ_TABLE, 0);
		if (!node) {
			panel_err("failed to get phandle of %s\n", DT_NAME_FREQ_TABLE);
			return -EINVAL;
		}
		lut->freq_hop_node = node;
		of_node_put(node);
#endif

		sz = of_property_count_u32_elems(panel_np, "id-mask");
		if (sz <= 0) {
			panel_err("failed to get count of id-mask property\n");
			return -EINVAL;
		}

		if (sz % 2 > 0) {
			panel_err("id-mask value must be pair (sz:%d)\n", sz);
			return -EINVAL;
		}

		if (sz > ARRAY_SIZE(tmparr)) {
			panel_warn("id mask size exceeded (sz:%d arr-size:%ld)\n", sz, ARRAY_SIZE(tmparr));
			sz = ARRAY_SIZE(tmparr);
		}

		memset(tmparr, 0, ARRAY_SIZE(tmparr) * sizeof(int));
		ret = of_property_read_u32_array(panel_np, "id-mask", tmparr, sz);
		if (ret < 0) {
			panel_err("failed to get id-mask values\n");
			return -EINVAL;
		}

		for (i = 0; i < sz / 2; i++) {
			id_mask = kzalloc(sizeof(struct panel_id_mask), GFP_KERNEL);
			if (!id_mask)
				return -ENOMEM;
			id_mask->id = tmparr[i * 2];
			id_mask->mask = tmparr[i * 2 + 1];
			list_add_tail(&id_mask->head, &lut->id_mask_list);
		}
		list_add_tail(&lut->head, &panel->panel_lut_list);
		panel_info("%s name:\"%s\"\n", pp->name, lut->name);
		print_panel_lut(lut);
		of_node_put(panel_np);
	}
	of_node_put(lookup_np);
	return 0;
}

int panel_parse_dt(struct panel_device *panel)
{
	struct device *dev;
	int ret = 0;

	if (panel == NULL)
		return -EINVAL;

	dev = panel->dev;
	if (dev == NULL)
		return -ENODEV;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		panel_err("failed to get dt info\n");
		return -EINVAL;
	}

	panel->of_node_name = dev->of_node->name;

	if (of_property_read_u32(dev->of_node, "panel,id", &panel->id))
		panel_err("Invalid panel's id : %d\n", panel->id);
	panel_dbg("panel-id: %d\n", panel->id);

#if !defined(CONFIG_UML)
	ret = panel_parse_pinctrl(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse pinctrl\n", panel->id);
		return ret;
	}
#endif

	ret = panel_parse_gpio(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse gpio\n", panel->id);
		return ret;
	}

	ret = panel_parse_regulator(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse regulator\n", panel->id);
		return ret;
	}

	ret = panel_parse_panel_lookup(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse panel lookup\n", panel->id);
		return ret;
	}

	return ret;
}

static void disp_det_handler(struct work_struct *work)
{
	int con_det_state;
	int ret, disp_det_state;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_DISP_DET]);
	struct panel_state *state = &panel->state;

	ret = panel_disable_disp_det_irq(panel);
	if (ret < 0)
		panel_err("failed to disable disp_det irq\n");

	panel_err("disp_det is abnormal state\n");

	con_det_state = panel_conn_det_state(panel);
	disp_det_state = panel_disp_det_state(panel);
	panel_info("1'st disp_det_state: %s, con_det_state: %s, reset: %d\n",
			disp_det_state == PANEL_STATE_OK ? "OK" : "NOK",
			con_det_state == PANEL_STATE_OK ? "OK" : "NOK",
			panel_get_gpio_value(panel, PANEL_GPIO_RESET));

	/* delay for disp_det deboundce */
	usleep_range(10000, 11000);

	con_det_state = panel_conn_det_state(panel);
	disp_det_state = panel_disp_det_state(panel);
	panel_info("2'nd disp_det_state: %s, con_det_state: %s, reset: %d\n",
			disp_det_state == PANEL_STATE_OK ? "OK" : "NOK",
			con_det_state == PANEL_STATE_OK ? "OK" : "NOK",
			panel_get_gpio_value(panel, PANEL_GPIO_RESET));

	if (disp_det_state != PANEL_STATE_NOK)
		goto exit;

	if (con_det_state == PANEL_STATE_NOK) {
		panel_dsi_set_bypass(panel, true);
		panel->state.connected = PANEL_STATE_NOK;

		__set_panel_power(panel, PANEL_POWER_OFF);
		panel_set_bypass(panel, PANEL_BYPASS_ON);
		panel_set_cur_state(panel, PANEL_STATE_OFF);
		state->disp_on = PANEL_DISPLAY_OFF;
		usleep_range(300000, 301000);
		return;
	}

	if (state->cur_state == PANEL_STATE_OFF) {
		panel_err("panel is off state\n");
		return;
	}

	return;

exit:

	panel_enable_disp_det_irq(panel);
	panel_enable_pcd_irq(panel);

}

void panel_send_ubconn_uevent(struct panel_device *panel)
{
	char *uevent_conn_str[3] = {
		"CONNECTOR_NAME=UB_CONNECT",
		"CONNECTOR_TYPE=HIGH_LEVEL",
		NULL,
	};

	if (!panel->lcd_dev)
		return;

	kobject_uevent_env(&panel->lcd_dev->kobj, KOBJ_CHANGE, uevent_conn_str);
	panel_info("%s, %s\n", uevent_conn_str[0], uevent_conn_str[1]);
}

#if IS_ENABLED(CONFIG_SEC_ABC)
void panel_send_ubconn_to_abc(struct panel_device *panel)
{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	sec_abc_send_event("MODULE=ub_main@INFO=ub_disconnected");
#else
	sec_abc_send_event("MODULE=ub_main@WARN=ub_disconnected");
#endif
	panel_info("done\n");
}
#endif

void conn_det_handler(struct work_struct *data)
{
	struct panel_work *w = container_of(to_delayed_work(data),
		struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_CONN_DET]);
	bool is_disconnected;

	is_disconnected = panel_disconnected(panel);
	panel_info("%s state:%d cnt:%d\n", __func__,
		is_disconnected, panel->panel_data.props.ub_con_cnt);

#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify((is_disconnected ?
		PANEL_EVENT_UB_CON_DISCONNECTED : PANEL_EVENT_UB_CON_CONNECTED));
#endif

	if (!is_disconnected)
		return;

	if (panel->panel_data.props.conn_det_enable) {
		panel_send_ubconn_uevent(panel);
#if IS_ENABLED(CONFIG_SEC_ABC)
		panel_send_ubconn_to_abc(panel);
#endif
	}
	panel->panel_data.props.ub_con_cnt++;

	/* OCTA: power off is handled in disp_det handler */
	/* TFT(if doesn't have disp_det): power off here  */
	if (!panel_is_gpio_valid(panel, PANEL_GPIO_DISP_DET)) {
		panel_dsi_set_bypass(panel, true);
		panel->state.connected = PANEL_STATE_NOK;

		/* if panel_bypass is set, then power off will be returned */
		/* turn off panel power here first */
		__set_panel_power(panel, PANEL_POWER_OFF);
		panel_set_bypass(panel, PANEL_BYPASS_ON);
		panel_set_cur_state(panel, PANEL_STATE_OFF);
#ifdef CONFIG_MCD_PANEL_FACTORY
		panel_emergency_off(panel);
#endif
		usleep_range(300000, 301000);
	}
}

void pcd_handler(struct work_struct *data)
{
	struct panel_work *w = container_of(to_delayed_work(data),
		struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_PCD]);
	int pcd_state;

	pcd_state = panel_pcd_state(panel);

	if (pcd_state == PANEL_STATE_NOK)
		panel_dsi_set_bypass(panel, true); /* ignore TE checking & frame update request*/

	panel_info("state:%d\n", pcd_state);
}

void err_fg_handler(struct work_struct *data)
{
#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
	int ret, err_fg_state;
	bool err_fg_recovery = false, err_fg_powerdown = false;
	struct panel_work *w = container_of(to_delayed_work(data),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_ERR_FG]);
	struct panel_state *state = &panel->state;

	err_fg_recovery = panel->panel_data.ddi_props.err_fg_recovery;
	err_fg_powerdown = panel->panel_data.ddi_props.err_fg_powerdown;

	err_fg_state = panel_err_fg_state(panel);
	panel_info("err_fg_state:%s recover:%s powerdown: %s\n",
			err_fg_state == PANEL_STATE_OK ? "OK" : "NOK",
			err_fg_recovery ? "true" : "false",
			err_fg_powerdown ? "true" : "false");

	if (!(err_fg_recovery || err_fg_powerdown))
		return;

	switch (state->cur_state) {
	case PANEL_STATE_ALPM:
	case PANEL_STATE_NORMAL:
		if (err_fg_state == PANEL_STATE_NOK) {
			ret = panel_disable_gpio_irq(panel, PANEL_GPIO_ERR_FG);
			if (ret < 0)
				panel_warn("do not support irq\n");

			/* delay for disp_det deboundce */
			usleep_range(10000, 11000);
			if (err_fg_powerdown) {
				panel_err("powerdown: err_fg is abnormal state\n");
#ifdef CONFIG_PANEL_NOTIFY
				panel_send_ubconn_notify(PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
				ret = panel_powerdown_cb(panel);
				if (ret)
					panel_err("failed to powerdown_cb\n");
#ifdef CONFIG_PANEL_NOTIFY
				panel_send_ubconn_notify(PANEL_EVENT_UB_CON_CONNECTED);
#endif
			} else if (err_fg_recovery) {
				panel_err("recovery: err_fg is abnormal state\n");
				ret = panel_error_cb(panel);
				if (ret)
					panel_err("failed to recover_cb\n");
			}
			ret = panel_enable_gpio_irq(panel, PANEL_GPIO_ERR_FG);
			if (ret < 0)
				panel_warn("do not support irq\n");
		}
		break;
	default:
		break;
	}
#endif
	panel_info("done\n");
}

static int panel_fb_notifier(struct notifier_block *self, unsigned long event, void *data)
{
#if 0
	int *blank = NULL;
	struct panel_device *panel;

	struct fb_event *fb_event = data;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
	case FB_EVENT_BLANK:
		break;
	case FB_EVENT_FB_REGISTERED:
		panel_dbg("FB Registeted\n");
		return 0;
	default:
		return 0;
	}

	panel = container_of(self, struct panel_device, fb_notif);
	blank = fb_event->data;
	if (!blank || !panel) {
		panel_err("blank is null\n");
		return 0;
	}

	switch (*blank) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		if (event == FB_EARLY_EVENT_BLANK)
			panel_dbg("EARLY BLANK POWER DOWN\n");
		else
			panel_dbg("BLANK POWER DOWN\n");
		break;
	case FB_BLANK_UNBLANK:
		if (event == FB_EARLY_EVENT_BLANK)
			panel_dbg("EARLY UNBLANK\n");
		else
			panel_dbg("UNBLANK\n");
		break;
	}
#endif
	return 0;
}

#ifdef CONFIG_DISPLAY_USE_INFO
unsigned int g_rddpm = 0xFF;
unsigned int g_rddsm = 0xFF;

unsigned int get_panel_bigdata(void)
{
	unsigned int val = 0;

	val = (g_rddsm << 8) | g_rddpm;

	return val;
}

static int panel_dpui_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct common_panel_info *info;
	int panel_id;
	struct dpui_info *dpui = data;
	char tbuf[MAX_DPUI_VAL_LEN];
	u8 panel_datetime[7] = { 0, };
	u8 panel_coord[4] = { 0, };
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	bool cell_id_exist = true;
	int size;

	if (dpui == NULL) {
		panel_err("dpui is null\n");
		return 0;
	}

	panel = container_of(self, struct panel_device, panel_dpui_notif);
	panel_data = &panel->panel_data;
	panel_id = panel_data->id[0] << 16 | panel_data->id[1] << 8 | panel_data->id[2];

	info = find_panel(panel, panel_id);
	if (unlikely(!info)) {
		panel_err("panel not found\n");
		return -ENODEV;
	}

	resource_copy_by_name(panel_data, panel_datetime, "date");
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%04d%02d%02d %02d%02d%02d",
			((panel_datetime[0] & 0xF0) >> 4) + 2011, panel_datetime[0] & 0xF, panel_datetime[1] & 0x1F,
			panel_datetime[2] & 0x1F, panel_datetime[3] & 0x3F, panel_datetime[4] & 0x3F);
	set_dpui_field(DPUI_KEY_MAID_DATE, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[0]);
	set_dpui_field(DPUI_KEY_LCDID1, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[1]);
	set_dpui_field(DPUI_KEY_LCDID2, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[2]);
	set_dpui_field(DPUI_KEY_LCDID3, tbuf, size);

	resource_copy_by_name(panel_data, panel_coord, "coordinate");
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		panel_datetime[0], panel_datetime[1], panel_datetime[2], panel_datetime[3],
		panel_datetime[4], panel_datetime[5], panel_datetime[6],
		panel_coord[0], panel_coord[1], panel_coord[2], panel_coord[3]);
	set_dpui_field(DPUI_KEY_CELLID, tbuf, size);

	/* OCTAID */
	resource_copy_by_name(panel_data, octa_id, "octa_id");
	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			size += snprintf(tbuf + size, MAX_DPUI_VAL_LEN - size, "%c", cell_id[i]);
	}
	set_dpui_field(DPUI_KEY_OCTAID, tbuf, size);

#ifdef CONFIG_SUPPORT_DIM_FLASH
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN,
			"%d", panel->work[PANEL_WORK_DIM_FLASH].ret);
	set_dpui_field(DPUI_KEY_PNGFLS, tbuf, size);
#endif
	inc_dpui_u32_field(DPUI_KEY_UB_CON, panel->panel_data.props.ub_con_cnt);
	panel->panel_data.props.ub_con_cnt = 0;

	return 0;
}
#endif /* CONFIG_DISPLAY_USE_INFO */

#ifdef CONFIG_SUPPORT_TDMB_TUNE
static int panel_tdmb_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct tdmb_notifier_struct *value = data;
	int ret;

	panel = container_of(nb, struct panel_device, tdmb_notif);
	panel_data = &panel->panel_data;

	mutex_lock(&panel->io_lock);
	mutex_lock(&panel->op_lock);
	switch (value->event) {
	case TDMB_NOTIFY_EVENT_TUNNER:
		panel_data->props.tdmb_on = value->tdmb_status.pwr;
		if (!IS_PANEL_ACTIVE(panel)) {
			panel_info("keep tdmb state (%s) and affect later\n",
					panel_data->props.tdmb_on ? "on" : "off");
			break;
		}
		panel_info("tdmb state (%s)\n",
				panel_data->props.tdmb_on ? "on" : "off");
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_TDMB_TUNE_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write tdmb-tune seqtbl\n");
		panel_data->props.cur_tdmb_on = panel_data->props.tdmb_on;
		break;
	default:
		break;
	}
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->io_lock);
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static int panel_input_notifier_callback(struct notifier_block *nb,
		unsigned long data, void *v)
{
	struct panel_device *panel =
		container_of(nb, struct panel_device, input_notif);
	struct panel_info *panel_data = &panel->panel_data;

	switch (data) {
	case NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST:	/* set LFD min lock */
	case NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE: /* unset LFD min lock */
		/* TODO : input scalability need to be passed by dtsi or panel config */
		if (panel_vrr_lfd_is_supported(panel)) {
			panel_data->props.vrr_lfd_info.req[VRR_LFD_CLIENT_INPUT][VRR_LFD_SCOPE_NORMAL].scalability =
				(data == NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST) ?
				VRR_LFD_SCALABILITY_2 : VRR_LFD_SCALABILITY_NONE;
			queue_delayed_work(panel->work[PANEL_WORK_UPDATE].wq,
					&panel->work[PANEL_WORK_UPDATE].dwork, msecs_to_jiffies(0));
		}
		break;
	}
	return 0;
}
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
int panel_fast_discharge_set(struct panel_device *panel)
{
	int ret = 0;

	mutex_lock(&panel->io_lock);
	mutex_lock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_FD_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write fast discharge seqtbl\n");

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->io_lock);
	return ret;
}
#endif

static int panel_init_work(struct panel_work *w,
		char *name, panel_wq_handler handler)
{
	if (w == NULL || name == NULL || handler == NULL) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	mutex_init(&w->lock);
	strncpy(w->name, name, MAX_PANEL_WORK_NAME_SIZE - 1);
	INIT_DELAYED_WORK(&w->dwork, handler);
	w->wq = create_singlethread_workqueue(name);
	if (w->wq == NULL) {
		panel_err("failed to create %s workqueue\n", name);
		return -ENOMEM;
	}
	atomic_set(&w->running, 0);
	panel_info("%s:done\n", name);

	return 0;
}

static int panel_drv_init_work(struct panel_device *panel)
{
	int i, ret;
	char name[MAX_PANEL_WORK_NAME_SIZE];

	for (i = 0; i < PANEL_WORK_MAX; i++) {
		if (!panel_wq_handlers[i])
			continue;

		snprintf(name, MAX_PANEL_WORK_NAME_SIZE, "panel%d:%s",
				panel->id, panel_work_names[i]);

		ret = panel_init_work(&panel->work[i],
				name, panel_wq_handlers[i]);
		if (ret < 0) {
			panel_err("failed to initialize panel_work(%s)\n",
					panel_work_names[i]);
			return ret;
		}
	}

	return 0;
}

static int panel_create_thread(struct panel_device *panel)
{
	size_t i;

	if (unlikely(!panel)) {
		panel_warn("panel is null\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(panel->thread); i++) {
		if (panel_thread_fns[i] == NULL)
			continue;

		panel_info("%s-%d:init\n",
				panel_thread_names[i], panel->id);
		panel->thread[i].should_stop = false;
		init_waitqueue_head(&panel->thread[i].wait);
		panel->thread[i].thread =
			kthread_run(panel_thread_fns[i], panel,
					"%s-%d", panel_thread_names[i], panel->id);
		if (IS_ERR_OR_NULL(panel->thread[i].thread)) {
			panel_err("failed to run %s-%d thread\n",
					panel_thread_names[i], panel->id);
			panel->thread[i].thread = NULL;
			return PTR_ERR(panel->thread[i].thread);
		}
		panel_info("%s-%d:done\n",
				panel_thread_names[i], panel->id);
	}

	return 0;
}

static int panel_destroy_thread(struct panel_device *panel)
{
	size_t i;

	if (unlikely(!panel)) {
		panel_warn("panel is null\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(panel->thread); i++) {
		if (panel_thread_fns[i] == NULL)
			continue;

		if (IS_ERR_OR_NULL(panel->thread[i].thread))
			continue;

		panel->thread[i].should_stop = true;
		/* wake up waitqueue to stop */
		wake_up_interruptible_all(&panel->thread[i].wait);
		/* kthread_should_stop() == true */
		kthread_stop(panel->thread[i].thread);
	}

	return 0;
}

struct panel_device *panel_device_create(void)
{
	struct panel_device *panel;

	panel = kzalloc(sizeof(struct panel_device), GFP_KERNEL);
	if (!panel)
		return NULL;

	return panel;
}
EXPORT_SYMBOL(panel_device_create);

void panel_device_destroy(struct panel_device *panel)
{
	kfree(panel);
}
EXPORT_SYMBOL(panel_device_destroy);

int panel_device_register_notifiers(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	panel->fb_notif.notifier_call = panel_fb_notifier;
	ret = fb_register_client(&panel->fb_notif);
	if (ret < 0) {
		panel_err("failed to register fb notifier callback\n");
		return ret;
	}

#ifdef CONFIG_DISPLAY_USE_INFO
	panel->panel_dpui_notif.notifier_call = panel_dpui_notifier_callback;
	ret = dpui_logging_register(&panel->panel_dpui_notif, DPUI_TYPE_PANEL);
	if (ret < 0) {
		panel_err("failed to register dpui notifier callback\n");
		return ret;
	}
#endif

#ifdef CONFIG_SUPPORT_TDMB_TUNE
	ret = tdmb_notifier_register(&panel->tdmb_notif,
			panel_tdmb_notifier_callback, TDMB_NOTIFY_DEV_LCD);
	if (ret < 0) {
		panel_err("failed to register tdmb notifier callback\n");
		return ret;
	}
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&panel->input_notif,
			panel_input_notifier_callback, 3);
#endif

	return 0;
}

struct panel_drv_funcs panel_drv_funcs = {
	.register_error_cb = panel_register_error_cb,
	.register_cb = panel_register_cb,
	.get_panel_state = panel_ioctl_get_panel_state,
	.attach_adapter = panel_ioctl_attach_adapter,

	.probe = panel_probe,
	.sleep_in = panel_sleep_in,
	.sleep_out = panel_sleep_out,
	.display_on = panel_display_on,
	.display_off = panel_display_off,
	.power_on = panel_power_on,
	.power_off = panel_power_off,

	.debug_dump = panel_debug_dump,
#ifdef CONFIG_MCD_PANEL_LPM
	.doze = panel_doze,
	.doze_suspend = panel_doze,
#endif
#ifdef CONFIG_SUPPORT_DSU
	.set_mres = panel_set_mres,
#endif
	.get_mres = NULL,
	.set_display_mode = panel_set_display_mode,
	.get_display_mode = panel_get_display_mode,

	.reset_lp11 = panel_reset_lp11,

	.frame_done = panel_ioctl_event_frame_done,
	.vsync = panel_ioctl_event_vsync,
#ifdef CONFIG_SUPPORT_MASK_LAYER
	.set_mask_layer = panel_set_mask_layer,
#endif
	.req_set_clock = panel_request_set_clock,
	.get_ddi_props = panel_get_ddi_props,
	.get_rcd_info = panel_get_rcd_info,
};

int panel_device_init(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	mutex_init(&panel->cmdq.lock);

	panel->cmdq.top = -1;
	panel->state.init_at = PANEL_INIT_BOOT;
	panel_set_bypass(panel, PANEL_BYPASS_OFF);
	panel->state.connected = true;
	panel_set_cur_state(panel, PANEL_STATE_OFF);
	panel->state.power = PANEL_POWER_OFF;
	panel->state.disp_on = PANEL_DISPLAY_OFF;
	panel->ktime_panel_on = ktime_get();
#ifdef CONFIG_SUPPORT_HMD
	panel->state.hmd_on = PANEL_HMD_OFF;
#endif

	mutex_init(&panel->op_lock);
	mutex_init(&panel->data_lock);
	mutex_init(&panel->io_lock);

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	mutex_init(&panel->copr.lock);
#endif
	panel->funcs = &panel_drv_funcs;

	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);

#ifdef CONFIG_MCD_PANEL_I2C
	ret = panel_i2c_drv_probe(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse i2c\n", panel->id);
		return ret;
	}
#endif

#ifdef CONFIG_MCD_PANEL_BLIC
	ret = panel_blic_probe(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse blic\n", panel->id);
		return ret;
	}
#endif

	ret = panel_obj_init(&panel->properties);
	if (ret < 0) {
		panel_err("panel-%d:failed to init panel properties\n", panel->id);
		return ret;
	}

	panel_parse_dt(panel);

	ret = panel_create_lcd_device(panel, panel->id);
	if (ret < 0) {
		panel_err("failed to create lcd device\n");
		return -EINVAL;
	}

	panel_drv_set_gpios(panel);
	panel_drv_init_work(panel);
	panel_create_thread(panel);

//remove debugfs
#if 1
#ifdef CONFIG_PANEL_DEBUG
	panel_create_debugfs(panel);
#endif
#endif

	/* sub-modules init */
	ret = panel_bl_init(&panel->panel_bl);
	if (ret < 0) {
		panel_err("failed to init panel_bl\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_init(&panel->mdnie);
	if (ret < 0) {
		panel_err("failed to init mdnie\n");
		return ret;
	}
#endif

	ret = panel_device_register_notifiers(panel);
	if (ret < 0) {
		panel_err("failed to register notifiers\n");
		return ret;
	}

	ret = panel_register_isr(panel);
	if (ret < 0) {
		panel_err("failed to register isr\n");
		return ret;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_SYSFS
	ret = panel_sysfs_probe(panel);
	if (ret < 0) {
		panel_err("failed to init sysfs\n");
		return -ENODEV;
	}
#endif

	panel_info("done\n");

	return 0;
}
EXPORT_SYMBOL(panel_device_init);

int panel_device_exit(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

#ifdef CONFIG_EXYNOS_DECON_LCD_SYSFS
	ret = panel_sysfs_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel-sysfs driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_exit(&panel->mdnie);
	if (ret < 0) {
		panel_err("failed to exit mdnie driver\n");
		return ret;
	}
#endif

	ret = panel_destroy_lcd_device(panel);
	if (ret < 0) {
		panel_err("failed to destroy lcd device\n");
		return ret;
	}

	ret = panel_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel\n");
		return ret;
	}

	/* TODO: unregister isr */
//	if (panel->max_nr_dim_flash_result > 0)
//		kfree(panel->dim_flash_result);

	panel_destroy_thread(panel);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&panel->input_notif);
#endif

#ifdef CONFIG_DISPLAY_USE_INFO
	dpui_logging_unregister(&panel->panel_dpui_notif);
#endif
	fb_unregister_client(&panel->fb_notif);
#if 0
#ifdef CONFIG_PANEL_DEBUG
	panel_destroy_debugfs(panel);
#endif
#endif
	kfree(panel->cmdbuf);

	return 0;
}
EXPORT_SYMBOL(panel_device_exit);

static int panel_drv_probe(struct platform_device *pdev)
{
	struct panel_device *panel;
	int ret;

	panel_info("...");
	if (!pdev)
		return -EINVAL;

	panel = panel_device_create();
	if (!panel)
		return -ENOMEM;

	panel->id = -1;
	panel->dev = &pdev->dev;
	platform_set_drvdata(pdev, panel);
	ret = panel_device_init(panel);
	if (ret < 0) {
		panel_err("failed to initialize panel device\n");
		goto err;
	}

	panel_info("done");

	return 0;

err:
	panel_device_destroy(panel);
	return ret;
}

static int panel_drv_remove(struct platform_device *pdev)
{
	struct panel_device *panel;

	panel = platform_get_drvdata(pdev);

	panel_device_destroy(panel);

	return 0;
}

static const struct of_device_id panel_drv_of_match_table[] = {
	{ .compatible = "samsung,panel-drv", },
	{ },
};
MODULE_DEVICE_TABLE(of, panel_drv_of_match_table);

struct platform_driver panel_driver = {
	.probe = panel_drv_probe,
	.remove = panel_drv_remove,
	.driver = {
		.name = PANEL_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(panel_drv_of_match_table),
	}
};

#ifdef CONFIG_SUPPORT_MAFPC
extern struct platform_driver mafpc_driver;
#endif
static int __init panel_drv_init(void)
{
	int ret;

	panel_info("++\n");
	ret = panel_create_lcd_class();
	if (ret < 0) {
		panel_err("panel_create_lcd_class returned %d\n", ret);
		return -EINVAL;
	}

#ifdef CONFIG_SUPPORT_MAFPC
	ret = platform_driver_register(&mafpc_driver);
	if (ret) {
		panel_err("failed to register mafpc driver\n");
		return ret;
	}
#endif
	ret = platform_driver_register(&panel_driver);
	if (ret) {
		panel_err("failed to register panel driver\n");
		return ret;
	}
	panel_info("--\n");

	return ret;
}

static void __exit panel_drv_exit(void)
{
	platform_driver_unregister(&panel_driver);
#ifdef CONFIG_SUPPORT_MAFPC
	platform_driver_unregister(&mafpc_driver);
#endif
	panel_destroy_lcd_class();
}

#ifdef CONFIG_EXYNOS_DPU30_DUAL
device_initcall_sync(panel_drv_init);
#else
module_init(panel_drv_init);
#endif
module_exit(panel_drv_exit);

MODULE_SOFTDEP("pre: s2dos05-regulator i2c-exynos5");
MODULE_DESCRIPTION("Samsung's Panel Driver");
MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");